#include <lwp.h>
#include <stdlib.h>
#include <stdio.h>
#include "schedulers.h"

static thread qhead=NULL;
#define tnext sched_one
#define tprev sched_two

static void s_admit(thread new) {

  /* add to queue */
  if ( qhead ) {
    new->tnext = qhead;
    new->tprev = qhead->tprev;
    new->tprev->tnext = new;
    qhead->tprev = new;
  } else {
    qhead = new;
    qhead->tnext = new;
    qhead->tprev = new;
  }
}

static void s_remove(thread victim) {
  if ( victim->tnext && victim->tprev) {

    /* cut out of queue */
    victim->tprev->tnext = victim->tnext;
    victim->tnext->tprev = victim->tprev;
    /* what if it were qhead? */
    if ( victim == qhead ) {
      if ( victim->tnext != victim)
        qhead = victim->tnext;
      else
        qhead = NULL;
    }
    victim->tnext = NULL;
    victim->tprev = NULL;
  }
}

static thread s_next() {
  thread res;

  if ( qhead )
    res = qhead;
  else
    res = NO_THREAD;

  return res;
}

static struct scheduler publish = {NULL,NULL,s_admit,s_remove,s_next};
scheduler AlwaysZero=&publish;

/*********************************************************/
__attribute__ ((unused))
void az_dp() {
  thread l;
  if ( !qhead )
    fprintf(stderr,"  AZ qhead is NULL\n");
  else {
    fprintf(stderr,"  AZ queue:\n");
    l = qhead;
    do {
      fprintf(stderr,"  (tid=%lu)\n", l->tid);
      l=l->tnext;
    } while ( l != qhead ) ;
    fprintf(stderr,"\n");
  }
}

