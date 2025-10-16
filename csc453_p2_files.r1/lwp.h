#ifndef LWPH
#define LWPH
#include <sys/types.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifdef __APPLE__
#endif


#if defined(__x86_64)
#include "fp.h"
typedef struct __attribute__ ((aligned(16))) __attribute__ ((packed))
registers {
  uintptr_t rax;            /* the sixteen architecturally-visible regs. */
  uintptr_t rbx;
  uintptr_t rcx;
  uintptr_t rdx;
  uintptr_t rsi;
  uintptr_t rdi;
  uintptr_t rbp;
  uintptr_t rsp;
  uintptr_t r8;
  uintptr_t r9;
  uintptr_t r10;
  uintptr_t r11;
  uintptr_t r12;
  uintptr_t r13;
  uintptr_t r14;
  uintptr_t r15;
  struct fxsave fxsave;   /* space to save floating point state */
} rfile;
#elif defined(__i386)
typedef struct registers {
  uintptr_t eax;            /* the eight architecturally-visible regs. */
  uintptr_t ebx;
  uintptr_t ecx;
  uintptr_t edx;
  uintptr_t esi;
  uintptr_t edi;
  uintptr_t ebp;
  uintptr_t esp;
} rfile;
#elif defined(__aarch64__)
typedef struct registers {
  // 64-bit General Purpose Registers (x16, x17, and x19 - x31)
  uintptr_t x0;
  uintptr_t x1;
  uintptr_t x2;
  uintptr_t x3;
  uintptr_t x4;
  uintptr_t x5;
  uintptr_t x6;
  uintptr_t x7;
  uintptr_t x8;
  uintptr_t x9;
  uintptr_t x10;
  uintptr_t x11;
  uintptr_t x12;
  uintptr_t x13;
  uintptr_t x14;
  uintptr_t x15;
  uintptr_t x16;
  uintptr_t x17;
  uintptr_t x18;
  uintptr_t x19;
  uintptr_t x20;
  uintptr_t x21;
  uintptr_t x22;
  uintptr_t x23;
  uintptr_t x24;
  uintptr_t x25;
  uintptr_t x26;
  uintptr_t x27;
  uintptr_t x28;
  uintptr_t fp;   // Frame Pointer
  uintptr_t lr;   // Link Register
  uintptr_t sp;   // Stack Pointer
  // 64-bit Floating Point Registers (d8 - d15)
  uintptr_t d8;
  uintptr_t d9;
  uintptr_t d10;
  uintptr_t d11;
  uintptr_t d12;
  uintptr_t d13;
  uintptr_t d14;
  uintptr_t d15;
} rfile;
#else
  #error "Architecture not supported"
#endif

typedef unsigned long tid_t;
#define NO_THREAD 0             /* an always invalid thread id */

typedef struct threadinfo_st *thread;
typedef struct threadinfo_st {
  tid_t         tid;            /* lightweight process id  */
  unsigned long *stack;         /* Base of allocated stack */
  size_t        stacksize;      /* Size of allocated stack */
  rfile         state;          /* saved registers         */
  unsigned int  status;         /* exited? exit status?    */
  thread        lib_one;        /* Two pointers reserved   */
  thread        lib_two;        /* for use by the library  */
  thread        sched_one;      /* Two more for            */
  thread        sched_two;      /* schedulers to use       */
} context;

typedef int (*lwpfun)(void *);  /* type for lwp function */

/* Tuple that describes a scheduler */
typedef struct scheduler {
  void   (*init)(void);            /* initialize any structures     */
  void   (*shutdown)(void);        /* tear down any structures      */
  void   (*admit)(thread new);     /* add a thread to the pool      */
  void   (*remove)(thread victim); /* remove a thread from the pool */
  thread (*next)(void);            /* select a thread to schedule   */
} *scheduler;

/* lwp functions */
extern tid_t lwp_create(lwpfun,void *,size_t);
extern void  lwp_exit(int status);
extern tid_t lwp_gettid(void);
extern void  lwp_yield(void);
extern void  lwp_start(void);
extern void  lwp_stop(void);
extern tid_t lwp_wait(int *);
extern void  lwp_set_scheduler(scheduler fun);
extern scheduler lwp_get_scheduler(void);
extern thread tid2thread(tid_t tid);

/* for lwp_wait */
#define TERMOFFSET        8
#define MKTERMSTAT(a,b)   ( (a)<<TERMOFFSET | ((b) & ((1<<TERMOFFSET)-1)) )
#define LWP_TERM          1
#define LWP_LIVE          0
#define LWPTERMINATED(s)  ( ((s>>TERMOFFSET)&LWP_TERM) == LWP_TERM )
#define LWPTERMSTAT(s)    ( (s) & ((1<<TERMOFFSET)-1) )

/* prototypes for asm functions */
void save_ctx(rfile *old);
void load_ctx(rfile *new);
void swap_ctx(rfile *old, rfile *new);

#endif
