#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include "lwp.h"
#include "smartalloc.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/resource.h>

/*
QUESTIONS:
- [MAYBE RESOLVED] what is the right way to "block" in lwp_wait?
- [MAYBE RESOLVED] how can I set the sched_head to whatever head the extern scheduler functions use?
*/
static thread curr_td = NULL;
static void lwp_wrap(lwpfun, void *);

static thread sched_head = NULL;
void rr_init() {
    curr_td = sched_head;
}

void rr_admit(thread new) {
    if (!sched_head) {
        sched_head = new;
        sched_head->sched_one = sched_head;
        sched_head->sched_two = sched_head;
    }
    else {
        new->sched_one = sched_head;
        new->sched_two = sched_head->sched_two;
        sched_head->sched_two->sched_one = new;
        sched_head->sched_two = new;
    }
}

void rr_remove(thread victim) {
    if (victim->tid == sched_head->tid) {
        sched_head = sched_head->sched_one;
    }
    victim->sched_two->sched_one = victim->sched_one;
    victim->sched_one->sched_two = victim->sched_two;
}

thread rr_next(void) {
    if (sched_head->sched_one == sched_head) return NULL;

    thread next = sched_head->sched_one;
    while (LWPTERMINATED(next->status)) next = next->sched_one;
    
    return next;
}

struct scheduler rr_publish = {rr_init, NULL, rr_admit, rr_remove, rr_next};
//scheduler RoundRobin = &rr_publish;
#define RoundRobin &rr_publish

static scheduler sched = RoundRobin;

/*
- each thread has its own "stack" (stack frame), though they are all just sequentially stored in the real stack
- stack frames must all be the same size exactly always
- when running a thread, change the stack pointer to point to the top of that thread's stack
- (lwpfun?) function call adds a stack frame to the stack
- stack frame will end with a return address for whatever [function] called
- have one function with no local variables and two parameters to make sure stacks are always perfectly aligned, 
    used only for the context swap
--> not necessary to worry about that when you're not doing the context swap
--> this is my own implementation for a context swap function
*/

//static thread wait_queue_head = NULL;

#define DFLT_STACK 8*1024*1024

static tid_t tid_cntr = NO_THREAD;
tid_t lwp_create(lwpfun fun,void *param, size_t size) {
    size_t stack_size;
    long page_size = sysconf(_SC_PAGE_SIZE);
    struct rlimit r1;
    if (getrlimit(RLIMIT_STACK, &r1)) stack_size = DFLT_STACK;
    else if (r1.rlim_cur == RLIM_INFINITY) stack_size = DFLT_STACK;
    else stack_size = r1.rlim_cur;

    stack_size = ((stack_size + page_size - 1) / page_size) * page_size; //round up to page size

    unsigned long *s = (unsigned long *)mmap(NULL,stack_size,
        PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,-1,0);

    if(s == MAP_FAILED) {
        perror("lwp_create: stack creation failed\n");
        return NO_THREAD;
    }

    thread td = (thread) malloc(sizeof(context));
    td->tid = ++tid_cntr;
    td->stack = s;
    td->stacksize = stack_size;
    td->status = MKTERMSTAT(LWP_LIVE,0);
    
    unsigned long *s_top = (unsigned long *)((char *) s + stack_size); //need to apply alignment?
    unsigned long *end_bytes = (unsigned long *)((char *)s_top - 2);
    if (((uintptr_t)end_bytes & 0xF) != 0){
        // we have to adjust the address of the stack pointer's initial address to be 16-byte aligned
        size_t byteAlign = (16 - ((uintptr_t)end_bytes & 0xF)) / sizeof(unsigned long);
        end_bytes = (unsigned long *)((char *)end_bytes - byteAlign);
    }
    end_bytes[0] = 0;
    end_bytes[1] = (unsigned long) lwp_wrap;

    memset(&(td->state), 0, sizeof(td->state));
    td->state.rbp = (uintptr_t) s_top; //end_bytes?
    td->state.rsp = (uintptr_t) s_top; //end_bytes?
    td->state.rdi = (uintptr_t) fun;
    td->state.rsi = (uintptr_t) param;
    td->state.fxsave = FPU_INIT;

    sched->admit(td);
    return td->tid;
}

void  lwp_start(void) {
    thread td = (thread) malloc(sizeof(context));
    td->tid = ++tid_cntr;
    td->status = MKTERMSTAT(LWP_LIVE,0);
    td->stack = NULL;
    td->stacksize = 0;
    td->state.fxsave = FPU_INIT;
    curr_td = td;

    sched->admit(td);
    lwp_yield();
}

void  lwp_exit(int status) {
    unsigned long retval = curr_td->state.rdi;
    curr_td->status = MKTERMSTAT(retval, status);
    //do I remove from scheduler here or in lwp_wait
    lwp_yield();
}

tid_t lwp_gettid(void) {
    if (LWPTERMSTAT(curr_td->status) == LWP_LIVE) {
        return curr_td->tid;
    }
    else {
        return (tid_t) NO_THREAD;
    }
}

void  lwp_yield(void) {
    thread next_td = sched->next(); //terminated ones must be skipped even with other schedulers
    if (!next_td) exit(LWPTERMSTAT(curr_td->status));
    swap_ctx(&(curr_td->state), &(next_td->state));
    curr_td = next_td;
}

tid_t lwp_wait(int *status) {
    //thread head = (sched->next())->sched_two;
    thread iter = curr_td;
    int stat = iter->status;

    int block_cond = 0;
    while (!LWPTERMINATED(stat)) {
        if (LWPTERMSTAT(iter->status) == LWP_LIVE) block_cond = 1;
        iter = iter->sched_one;
        stat = iter->status;
        if (iter == curr_td) {
            if (block_cond) {
                /*//block until one terminates
                if (wait_queue_head) {
                    curr_td->lib_one = wait_queue_head;
                    curr_td->lib_two = wait_queue_head->lib_two;
                    wait_queue_head->lib_two->lib_one = curr_td;
                    wait_queue_head->lib_two = curr_td;
                }
                else {
                    wait_queue_head = curr_td;
                    wait_queue_head->lib_one = wait_queue_head;
                    wait_queue_head->lib_two = wait_queue_head;
                }
                sched->remove(curr_td);*/
                lwp_yield();
            }
            else {
                //additional case: if there are no runnable threads that can terminate, return NO_THREAD
                return (tid_t) NO_THREAD; 
            }
        }
    }
    if (stat) *status = stat;
    
    tid_t term_tid = iter->tid;
    munmap(iter->stack, iter->stacksize);
    free(iter);
    sched->remove(iter);

    /*//pop blocking thread off wait queue and re-admit to scheduler
    if (wait_queue_head) {
        sched->admit(wait_queue_head);
        if (wait_queue_head->lib_one == wait_queue_head) {
            wait_queue_head = NULL;
        }
        else {
            wait_queue_head->lib_two->lib_one = wait_queue_head->lib_one;
            wait_queue_head->lib_one->lib_two = wait_queue_head->lib_two;
            wait_queue_head = wait_queue_head->lib_two;
        }
    }*/

    return term_tid;
}

void  lwp_set_scheduler(scheduler new_sched) {
    if (!new_sched) new_sched = RoundRobin;
    if (new_sched->init) new_sched->init();
    if (new_sched == sched) return;

    //transfer all threads
    thread next = sched->next();
    while (next) {
        sched->remove(next);
        new_sched->admit(next);
        next = sched->next();
    }

    if (sched->shutdown) sched->shutdown();
    sched = new_sched;
}
scheduler lwp_get_scheduler(void) {
    return sched;
}

thread tid2thread(tid_t tid) {
    thread iter = curr_td;
    while (iter->tid != tid) {
        if (iter->sched_one == curr_td) return NULL;
        iter = iter->sched_one;
    }
    return iter;
}

static void lwp_wrap(lwpfun fun, void *arg) {
    int rval;
    rval = fun(arg);
    lwp_exit(rval);
}
