/* Test the k5dcepag routine by setting a pag, and 
 * and execing a shell under this pag. 
 * 
 * This allows you to join a PAG which was created  
 * earlier by some other means. 
 * for example k5dcecon
 * 
 * Must be run as root for testing only. 
 *
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>

#define POSIX_SETJMP
#define POSIX_SIGNALS

#ifdef POSIX_SIGNALS
typedef struct sigaction handler;
#define handler_init(H,F)       (sigemptyset(&(H).sa_mask), \
                     (H).sa_flags=0, \
                     (H).sa_handler=(F))
#define handler_swap(S,NEW,OLD)     sigaction(S, &NEW, &OLD)
#define handler_set(S,OLD)      sigaction(S, &OLD, NULL)
#else
typedef sigtype (*handler)();
#define handler_init(H,F)       ((H) = (F))
#define handler_swap(S,NEW,OLD)     ((OLD) = signal ((S), (NEW)))

#define handler_set(S,OLD)      (signal ((S), (OLD)))
#endif

typedef void sigtype;

/*
 * We could include the dcedfs/syscall.h which should have these
 * numbers, but it has extra baggage. So for
 * simplicity sake now, we define these here.
 */


#define AFSCALL_SETPAG 2
#define AFSCALL_GETPAG 11

#if defined(sun)
#define AFS_SYSCALL  72

#elif defined(hpux)
/* assume HPUX 10 +  or is it 50 */
#define AFS_SYSCALL 326

#elif defined(_AIX)
#define DPAGAIX "dpagaix"
/* #define DPAGAIX "/krb5/sbin/dpagaix" */

#elif defined(sgi) || defined(_sgi)
#define AFS_SYSCALL  206+1000

#else
#define AFS_SYSCALL (Unknown_DFS_AFS_SYSCALL)
#endif

static sigjmp_buf setpag_buf;

static sigtype mysig()
{
  siglongjmp(setpag_buf, 1);
}


int  krb5_dfs_newpag(new_pag)
  int new_pag;
{
  handler sa1, osa1;
  handler sa2, osa2;
  int pag = -1;

  handler_init (sa1, mysig);
  handler_init (sa2, mysig);
  handler_swap (SIGSYS, sa1, osa1);
  handler_swap (SIGSEGV, sa2, osa2);
 
  if (sigsetjmp(setpag_buf, 1) == 0) {
#if defined(_AIX)
    int (*dpagaix)(int, int, int, int, int, int);

    if (dpagaix = load(DPAGAIX, 0, 0)) 
      pag = (*dpagaix)(AFSCALL_SETPAG, new_pag, 0, 0, 0, 0);
#else
    pag = syscall(AFS_SYSCALL,AFSCALL_SETPAG, new_pag, 0, 0, 0, 0);
#endif
    handler_set (SIGSYS, osa1);
    handler_set (SIGSEGV, osa2);
    return(pag);
  }

  fprintf(stderr,"Setpag failed with a system error\n");
  /* syscall failed! return 0 */
  handler_set (SIGSYS, osa1);
  handler_set (SIGSEGV, osa2);
  return(-1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
  extern int optind;
  extern char *optarg;
  int rv;
  int rc;
  unsigned int pag;
  unsigned int newpag = 0;
  char ccname[256];
  int nflag = 0;
  
  while((rv = getopt(argc,argv,"n:")) != -1) {
    switch(rv) {
     case 'n':
       nflag++;
       sscanf(optarg,"%8x",&newpag);
       break;
     default:
       printf("Usage: k5dcepagt -n pag \n");
       exit(1);
    }
  }

  if (nflag) {
    fprintf (stderr,"calling k5dcepag newpag=%8.8x\n",newpag);
    pag = krb5_dfs_newpag(newpag);

    fprintf (stderr,"PAG returned = %8.8x\n",pag);
    if ((pag != 0) && (pag != -1)) {
      sprintf (ccname,
        "FILE:/opt/dcelocal/var/security/creds/dcecred_%8.8x", 
        pag);
      esetenv("KRB5CCNAME",ccname,1);
      execl("/bin/csh","csh",0);
    }
    else {
      fprintf(stderr," Not a good pag value\n");
    }
  }
}
