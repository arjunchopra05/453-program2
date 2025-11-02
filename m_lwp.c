#include "lwp.h"
#include <sys/mman.h>
#include "smartalloc.h"
#include "fp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <errno.h>

// NOTES:
// - example mmap call: s = mmap(NULL, howbig, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
// - use sysconf() to look up the size of a memory page _SC_PAGE_SIZE
// - to get size of a resource limit, use getrlimit() (reports both hard and soft limits, use soft limit)
// - limit for stack size is RLIMIT_STACK, if it DNE or is RLIM_INFINITY, use 8MB
// - round up the limit ot the nearest multiple of the page size
// - use munmap() when done with a mapping
// - when initializing a thread's reg file, initialize the fxsave reg to FPU_INIT: newthread->state.fxsave = FPU_INIT
// - a SEGV or segmentation fault may mean: a stack overflow, a stack corruption, an attempt to access a stack frame that is not properly aligned, all the other usual cases
// - If sched->next() returns NULL, lwp_yield() will exit

// IMPORTANT: 
// - all stacks frames must be 16-byte aligned; the address of the bottom (lowest in memory) element of the argument area needs to be evenly divisible by 16;
//   (also, the saved base pointer's address must be evenly divisble by 16)
// - stack is a array of unsigned longs, so we can index it accordingly
// - use asm function "swap_rfiles" to perform an atomic context switch

// Makefile must build liblwp.a when invoked either with or without the target

// global thread ID counter
static tid_t tidCnt = 2;

static struct threadinfo_st mainSysThread;

// global list of all threads (could split into 2: "ready" and "not ready")
static thread threadList = NULL; // lib_one used as next pointer for this list

// zombie thread list
static thread zombieList = NULL; // lib_two used as next pointer for this list

// current scheduler (default: round robin)
static scheduler currSched = NULL; // sched_one used as the next pointer for the scheduler buffer

// current running thread
static thread currThread = NULL;

static int callableThreads = 0;

// function header for lwpfunction wrapper
static void lwp_wrap(lwpfun fun, void *arg);

// Assembly register file switch function
void swap_rfiles(rfile *old, rfile *new);

/* ROUND ROBIN SCHEDULER: */
// Will implement it as a singly linked list for now
static thread rrStart = NULL;
static thread rrEnd = NULL;

void rrInit(void){
    rrStart = NULL;
    rrEnd = NULL;
}

void rrShutdown(void){
    rrStart = NULL;
    rrEnd = NULL;
}

void rrAdmit(thread new){
    new->sched_one = NULL; // use "sched_one" as our next pointer
    if (rrStart == NULL){
        rrStart = new;
        rrEnd = new;
    }
    else{
        // move the tail
        rrEnd->sched_one = new;
        rrEnd = new;
    }
    callableThreads++;
}

// remove the passed context from the scheduler's scheduling pool (do not have to worry about exiting a thread here)
void rrRemove(thread victim){
    if (rrStart == NULL){
        return;
    }

    thread rrCurr = rrStart;
    thread rrPrev = NULL;

    while (rrCurr != NULL){
        if (rrCurr == victim){
            if (rrCurr == rrStart){
                rrStart = rrStart->sched_one;
                if (rrCurr == rrEnd){
                    rrEnd = NULL;
                }
            }
            else{
                rrPrev->sched_one = rrCurr->sched_one;
                if (rrEnd == victim){
                    rrEnd = rrPrev;
                }
            }
            victim->sched_one = NULL;
            callableThreads--;
            return;
        }
        rrPrev = rrCurr;
        rrCurr = rrCurr->sched_one;
    }
}

thread rrNext(void){
    if (rrStart == NULL){
        return NULL;
    }

    thread nextThread = rrStart;
    rrStart = rrStart->sched_one;

    if (rrStart == NULL) {
        rrEnd = NULL;
        nextThread->sched_one = NULL;
        return nextThread;
    }

    nextThread->sched_one = NULL;
    rrEnd->sched_one = nextThread;
    rrEnd = nextThread;
    return nextThread;
}

// RR instantiation
static struct scheduler rrPublish = {rrInit, rrShutdown, rrAdmit, rrRemove, rrNext};
static scheduler roundRobin = &rrPublish;

static unsigned long *stackAllocation(size_t *externSize){
    long pageSize = sysconf(_SC_PAGE_SIZE);
    struct rlimit rLimit; 
    size_t size;

    if (getrlimit(RLIMIT_STACK, &rLimit) == 0 && rLimit.rlim_cur != RLIM_INFINITY){
        size = rLimit.rlim_cur;
    }
    else{
        size = 8 * 1024 * 1024; // Set the limit to 8 MB if it is "infinity"
    }

    // Round up resource limit to closest pageSize using ceiling function
    size = ((size + pageSize - 1) / pageSize) * pageSize;

    void *stack = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
    if (stack == MAP_FAILED){
        return NULL;
    }
    else{
        // Cast the stack pointer to an unsigned long pointer for proper return value and easy stack traversal later on
        stack = (unsigned long *)stack;
    }

    if (externSize != NULL){
        *externSize = size;
    }
    return stack; // IMPORTANT: Don't forget to call munmap() later
}

static int runnableCheck(void){
    thread t = threadList;
    while (t != NULL){
        if (t->status == LWP_LIVE && t->tid != 1){ // CHECK
            return 1;
        }
        t = t->lib_one;
    }
    return 0;
}

/* LWP FUNCTIONS: */

// create a new LWP
tid_t lwp_create(lwpfun function, void *argument, size_t stackSize){
    // Step 1: Create new thread
    thread newThread = malloc(sizeof(struct threadinfo_st));
    if (newThread == NULL){
        return NO_THREAD;
    }

    newThread->stack = stackAllocation(&newThread->stacksize); // CHECK
    if (newThread->stack == NULL){
        free(newThread);
        return NO_THREAD;
    }
    newThread->tid = tidCnt++;
    newThread->state.fxsave = FPU_INIT;
    newThread->status = LWP_LIVE;
    newThread->lib_one = NULL;
    newThread->lib_two = NULL;
    newThread->sched_one = NULL;
    newThread->sched_two = NULL;


    // Step 2: Add thread to global list and Admit thread to the current scheduler
    newThread->lib_one = threadList; // LIFO implementation (because order does not matter for global list of threads)
    threadList = newThread;

    if (currSched == NULL){
        currSched = roundRobin;
        if (currSched->init != NULL){
            currSched->init();
        }
    }

    // Step 3: initialize the stack
    unsigned long *stackPointer = (unsigned long *)((char *)newThread->stack + newThread->stacksize);
    unsigned long *sp = stackPointer - 2; // saved and return 2 reg

    sp[0] = 0;
    sp[1] = (unsigned long)lwp_wrap;

    uintptr_t rbpAddr = (uintptr_t)&sp[0];
    if ((rbpAddr & 0xF) != 0){
        // we have to adjust the address of the stack pointer's initial address to be 16-byte aligned
        size_t byteAlign = (16 - (rbpAddr & 0xF)) / sizeof(unsigned long);
        sp -= byteAlign;
        sp[0] = 0;
        sp[1] = (unsigned long)lwp_wrap;
    }

    memset(&newThread->state, 0, sizeof(newThread->state));
    newThread->state.rbp = (unsigned long) &sp[0]; // Or should it be "stackPointer" here
    newThread->state.rsp = (unsigned long) &sp[0];
    newThread->state.rsi = (unsigned long)argument; // lwp_wrap uses these
    newThread->state.rdi = (unsigned long)function;

    newThread->state.fxsave = FPU_INIT; // Doing this again just in case memset messes with it

    if (currSched->admit != NULL){
        currSched->admit(newThread); // Use this to admit the newly created thread to the scheduler
    }

    // Step 4: Return the tid or NO_THREAD
    return newThread->tid;
}

// terminate the calling LWP
void  lwp_exit(int status){
    //unsigned long exitVal = currThread->state.rdi;
    currThread->status = MKTERMSTAT(LWP_TERM, (status & 0xFF)); // CHECK: this status input may be wrong
    if (currSched != NULL && currSched->remove != NULL){
        currSched->remove(currThread);
    }

    // Put the terminated thread in the zombie list so we can deallocate its resources in lwp_wait
    currThread->lib_two = zombieList;
    zombieList = currThread;

    lwp_yield();
}

// return thread ID of the calling LWP
tid_t lwp_gettid(void){
    if (currThread == NULL){
        return NO_THREAD;
    }
    else{
        return currThread->tid;
    }
}

// yield the CPU to another LWP
void  lwp_yield(void){
    // if (currSched == NULL){
    //     currSched = roundRobin;
    // }

    if (currSched == NULL || currSched->next == NULL){
        currSched = roundRobin;
    }

    thread nextThread = currSched->next();

    if (nextThread == NULL){
        int exStat = 0;
        if (currThread != NULL){
            exStat = LWPTERMSTAT(currThread->status);
        }
        exit(exStat);
    }

    // Get ready for a context swap in the registers
    thread prevThread = currThread;
    currThread = nextThread;
    swap_rfiles(&(prevThread->state), &(currThread->state));
}

// start the LWP system
void  lwp_start(void){
    // Check if there is an active scheduler, if not use round robin
    if (currSched == NULL){
        currSched = roundRobin;
        if (currSched->init != NULL){
            currSched->init(); 
        }
    }

    // allocate space in mem for the system thread struct
    thread originThread = &mainSysThread;

    originThread->tid = 1; // System thread gets number 0
    originThread->stack = NULL; // Uses the system's stack
    originThread->stacksize = 0;
    memset(&originThread->state, 0, sizeof(originThread->state));
    originThread->state.fxsave = FPU_INIT;
    originThread->status = LWP_LIVE;
    originThread->lib_one = NULL;
    originThread->lib_two = NULL;
    originThread->sched_one = NULL;
    originThread->sched_two = NULL;
    
    // Set the current thread and start of the thread list to be the system thread
    threadList = originThread;
    currThread = originThread;
    
    if (currSched->admit != NULL){
        currSched->admit(originThread);
    }

    lwp_yield(); // Switch to the next thread;
}

// IGNORE
// extern void  lwp_stop(void){
// }

// wait for a thread to terminate
tid_t lwp_wait(int *status){
    // block here if there are threads waiting to be terminated
    while (zombieList == NULL){
        if ((currSched == NULL || currSched->next == NULL) && (runnableCheck() == 0)){
            return NO_THREAD;
        }

        lwp_yield();
    }

    thread tThread = zombieList;

    zombieList = tThread->lib_two;

    if (status != NULL){
        *status = tThread->status; // CHECK if this is correct, could is be just tThread->status?
    }
    
    // Place the tid in a variable so it doesn't get erased during deallocation
    tid_t tidHolder = tThread->tid;
    if (tThread->stack != NULL){
        munmap(tThread->stack, tThread->stacksize);
    }
    free(tThread);
    return tidHolder;
}

// install a new scheduling function
void  lwp_set_scheduler(scheduler fun){
    // tear down structs of the current scheduler
    if (currSched != NULL && (currSched->shutdown) != NULL){
        currSched->shutdown();
    }

    // change the current schedule to the new one (or round robin if null)
    if (fun == NULL){
        currSched = roundRobin;
    }
    else{
        currSched = fun;
    }

    // initialize new scheduler
    if (currSched->init != NULL){
        currSched->init();
    }

    thread t = threadList;
    while (t != NULL){
        if (currSched->admit != NULL){
            currSched->admit(t);
        }
        t = t->lib_one;
    }
}

// find out what the current scheduler is
scheduler lwp_get_scheduler(void){
    return currSched;
}

// map a thread ID to a context
thread tid2thread(tid_t tid){
    thread t = threadList;
    while (t != NULL){
        if (t->tid == tid){
            return t;
        }
        t = t->lib_one;
    }
    return NULL;
}

// call the given lwpfunction with the given argument; calls lwp_exit() with its return value
static void lwp_wrap(lwpfun fun, void *arg){
    int rval;
    rval = fun(arg);
    lwp_exit(rval); // CHECK: Should it be rval & 0xFF here?
}