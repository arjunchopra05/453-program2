#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include "lwp.h"
#include "smartalloc.h"

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

tid_t lwp_create(lwpfun,void *,size_t);
void  lwp_exit(int status);
tid_t lwp_gettid(void);
void  lwp_yield(void);
void  lwp_start(void);
void  lwp_stop(void);
tid_t lwp_wait(int *);
void  lwp_set_scheduler(scheduler fun);
scheduler lwp_get_scheduler(void);
thread tid2thread(tid_t tid);

static void lwp_wrap(lwpfun fun, void *arg) {
    int val = fun(arg);
    lwp_exit(val);
}

void save_ctx(rfile *old);
void load_ctx(rfile *new);
void swap_ctx(rfile *old, rfile *new);