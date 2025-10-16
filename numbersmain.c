/*
 * snake:  This is a demonstration program to investigate the viability
 *         of a curses-based assignment.
 *
 * Author: Dr. Phillip Nico
 *         Department of Computer Science
 *         California Polytechnic State University
 *         One Grand Avenue.
 *         San Luis Obispo, CA  93407  USA
 *
 * Email:  pnico@csc.calpoly.edu
 *
 * Revision History:
 *         $Log: numbersmain.c,v $
 *         Revision 1.3  2013-04-07 12:13:43-07  pnico
 *         Changed new_lwp() to lwp_create()
 *
 *         Revision 1.2  2013-04-02 17:04:17-07  pnico
 *         forgot to include the header
 *
 *         Revision 1.1  2013-04-02 16:39:24-07  pnico
 *         Initial revision
 *
 *         Revision 1.2  2004-04-13 12:31:50-07  pnico
 *         checkpointing with listener
 *
 *         Revision 1.1  2004-04-13 09:53:55-07  pnico
 *         Initial revision
 *
 *         Revision 1.1  2004-04-13 09:52:46-07  pnico
 *         Initial revision
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lwp.h"
#include "schedulers.h"

#define MAXSNAKES  100
#define INITIALSTACK 2048

static void indentnum(void *num);

int main(int argc, char *argv[]){
  long i;

  /* demonstrate schedulers */
  for (i=1;i<argc;i++) {                /* check options */
    if ( !strcmp(argv[i],"-z") ){ /* -z = schedule element 0 */
      lwp_set_scheduler(AlwaysZero);
    } else {
      fprintf(stderr,"%s: unknown option\n",argv[i]);
      exit(-1);
    }
  }

  printf("Launching LWPS\n");

  /* spawn a number of individual LWPs */
  for(i=1;i<=5;i++) {
    lwp_create((lwpfun)indentnum,(void*)i,INITIALSTACK);
  }

  lwp_start();                     /* returns when the last lwp exits */

  /* spawn a number of individual LWPs */
  for(i=1;i<=5;i++) {
    int status,num;
    tid_t t;
    t = lwp_wait(&status);
    num = LWPTERMSTAT(status);
    printf("Thread %ld exited with status %d\n",t,num);
  }

  printf("Back from LWPS.\n");
  lwp_exit(0);
  return 0;
}

static void indentnum(void *num) {
  /* print the number num num times, indented by 5*num spaces
   * Not terribly interesting, but it is instructive.
   */
  long i;
  int howfar;

  howfar=(long)num;              /* interpret num as an integer */
  for(i=0;i<howfar;i++){
    printf("%*d\n",howfar*5,howfar);
    lwp_yield();                /* let another have a turn */
  }
  lwp_exit(i);                  /* bail when done.  This should
                                 * be unnecessary if the stack has
                                 * been properly prepared
                                 */
}

