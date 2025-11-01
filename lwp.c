#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include "lwp.h"
#include "fp.h"
#include "smartalloc.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/resource.h>

static thread curr_td = NULL;
static thread wait_head = NULL;
static thread zomb_head = NULL;
static void lwp_wrap(lwpfun, void *);
void swap_rfiles(rfile *old, rfile *new);

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
    if (!sched_head) return NULL;
    else if (sched_head->sched_one == sched_head) return NULL;

    
    return sched_head->sched_one;;
}

struct scheduler rr_publish = {rr_init, NULL, rr_admit, rr_remove, rr_next};
scheduler RoundRobin = &rr_publish;

static scheduler sched = NULL;

void add_queue(thread *list_head, thread new_td) {
    if (*list_head) {
        new_td->lib_one = *list_head;
        new_td->lib_two = (*list_head)->lib_two;
        (*list_head)->lib_two->lib_one = new_td;
        (*list_head)->lib_two = new_td;
    }
    else {
        *list_head = new_td;
        (*list_head)->lib_one = *list_head;
        (*list_head)->lib_two = *list_head;
    }
}

void rm_queue(thread *list_head, thread victim_td) {
    if ((*list_head)->lib_one == *list_head && (*list_head)->tid == victim_td->tid) {
        *list_head = NULL;
        return;
    }
    thread iter = *list_head;
    while (iter->lib_one != *list_head) {
        if (iter->tid == victim_td->tid) {
            (*list_head)->lib_two->lib_one = (*list_head)->lib_one;
            (*list_head)->lib_one->lib_two = (*list_head)->lib_two;
            *list_head = (*list_head)->lib_two;
            return;
        }
        iter = iter->lib_one;
    }
}


#define DFLT_STACK 8*1024*1024

static tid_t tid_cntr = NO_THREAD;
tid_t lwp_create(lwpfun fun, void *param, size_t size) {
    if (!sched) sched = RoundRobin;

    size_t stack_size;
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) stack_size = DFLT_STACK;
    struct rlimit r1;
    if (getrlimit(RLIMIT_STACK, &r1) == -1) stack_size = DFLT_STACK;
    else if (r1.rlim_cur == RLIM_INFINITY || r1.rlim_cur == 0) stack_size = DFLT_STACK;
    else stack_size = r1.rlim_cur;

    stack_size = ((stack_size + page_size - 1) / page_size) * page_size; //round up to page size

    unsigned long *s = (unsigned long *)mmap(NULL,stack_size,
        PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,-1,0);

    if(s == MAP_FAILED) {
        perror("lwp_create: stack creation failed\n");
        return NO_THREAD;
    }

    thread td = (thread) malloc(sizeof(context));
    tid_cntr++;
    td->tid = tid_cntr;
    td->stack = s;
    td->stacksize = stack_size;
    td->status = MKTERMSTAT(LWP_LIVE,0);
    td->state.fxsave = FPU_INIT;
    
    td->state.rdi = (unsigned long) fun;
    td->state.rsi = (unsigned long) param;
    
    unsigned long *s_top = (unsigned long *)((char *) td->stack + td->stacksize);
    s_top = (unsigned long *) ((unsigned long)s_top & ~(16)); //alignment
    *(--s_top) = 0;
    *(--s_top) = (unsigned long) lwp_wrap; //this is not working <<<
    *(--s_top) = 0;
    
    td->state.rbp = (unsigned long) s_top;
    td->state.rsp = (unsigned long) s_top;

    sched->admit(td);
    return td->tid;

    /*
    thread newThread = calloc(1, sizeof(context));
    if(!newThread) {
        perror("lwp_create: calloc for a thread context failed");
        return NO_THREAD;
    }

    
    size_t stackSize;

    long pageSize = sysconf(_SC_PAGE_SIZE);
    if(pageSize == -1) {
        perror("stack_size: page size error, using default size\n");
        return (size_t)DFLT_STACK;
    }

    struct rlimit rlim;
    if(getrlimit(RLIMIT_STACK, &rlim) == -1) {
        perror("stack_size: stack limit error, using default size\n");
        return (size_t)DFLT_STACK;
    }

    
    if(rlim.rlim_cur == RLIM_INFINITY || rlim.rlim_cur == 0) {
        stackSize = (size_t)DFLT_STACK;
    }

    else {
        stackSize = (size_t)rlim.rlim_cur;
    }

    
    stackSize = (stackSize - 1) / (size_t)pageSize + 1;
    stackSize *= (size_t)pageSize;
    void *stack = mmap(NULL, stackSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if(stack == MAP_FAILED) {
        perror("lwp_create: stack creation failed\n");
        free(newThread);
        return NO_THREAD;
    }

    tid_cntr++;

    
    newThread->tid          = tid_cntr;
    newThread->stack        = (unsigned long *)stack;
    newThread->stacksize    = stackSize;
    newThread->status       = MKTERMSTAT(LWP_LIVE, 0);
    newThread->state.fxsave = FPU_INIT;
    newThread->state.rdi    = (unsigned long)fun;
    newThread->state.rsi    = (unsigned long)param;

    
    unsigned long *stackTop = (unsigned long *)((char *)stack + stackSize);
    stackTop = (unsigned long*)((unsigned long)stackTop & ~(16));

    *--stackTop = 0;                            
    *--stackTop = (unsigned long)lwp_wrap;      
    *--stackTop = 0;                            

    newThread->state.rbp = (unsigned long)stackTop;
    newThread->state.rsp = (unsigned long)stackTop;
    
    
    sched->admit(newThread);

    return newThread->tid;*/
}

void  lwp_start(void) {
    if (!sched) sched = RoundRobin;
    
    thread td = (thread) malloc(sizeof(context));
    memset(td, 0, sizeof(context));
    tid_cntr++;
    td->tid = tid_cntr;
    td->status = MKTERMSTAT(LWP_LIVE,0);
    td->stack = NULL;
    td->state.fxsave = FPU_INIT;
    curr_td = td;

    sched->admit(td);
    lwp_yield();
}

void  lwp_exit(int status) {
    fprintf(stderr, "exiting thread %d\n", (int)curr_td->tid);
    curr_td->status = MKTERMSTAT(LWP_TERM, status);
    add_queue(&zomb_head, curr_td);
    sched->remove(curr_td);
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
    if (!next_td) exit(curr_td->status); //no more runnable threads
    
    curr_td = next_td;
    swap_rfiles(&(curr_td->state), &(next_td->state));
}

tid_t lwp_wait(int *status) {
    int stat;
    int runnableCond = 0;
    thread iter;

    iter = curr_td;
    if (iter) {
        do {
            stat = iter->status;
            if (LWPTERMSTAT(stat) == LWP_LIVE) runnableCond = 1;
            iter = iter->sched_one;
        } while (iter != curr_td);
    }

    if (!zomb_head) {
        if (!runnableCond) return NO_THREAD;

        fprintf(stderr, "blocking\n");
        //block until one terminates
        add_queue(&wait_head, curr_td);
        sched->remove(curr_td);
        lwp_yield();
    }

    else {
        iter = zomb_head;
        stat = iter->status;
        rm_queue(&zomb_head, iter);
    }
    
    *status = stat;
    
    tid_t term_tid = iter->tid;
    if (iter->stack) {
        if (munmap(iter->stack, iter->stacksize) == -1) {
            perror("lwp_wait: munmap failed");
            exit(1);
        }
    }
    free(iter);

    //pop blocking thread off wait queue and re-admit to scheduler
    if (wait_head) {
        sched->admit(wait_head);
        rm_queue(&wait_head, wait_head);
    }

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
    //search schedule
    thread iter = curr_td;
    while (iter->tid != tid) {
        if (iter->sched_one == curr_td) break;
        iter = iter->sched_one;
    }
    if (iter->tid == tid) return iter;
    
    //search wait queue
    iter = wait_head;
    while(iter->tid != tid) {
        if (iter->lib_one == wait_head) break;
        iter = iter->lib_one;
    }
    if (iter->tid == tid) return iter;

    return NO_THREAD;
}

static void lwp_wrap(lwpfun fun, void *arg) {
    int rval = fun(arg);
    lwp_exit(rval);
}
