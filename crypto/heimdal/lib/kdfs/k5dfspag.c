/* 
 * lib/krb5/os/k5dfspag.c
 *
 * New Kerberos module to issue the DFS PAG syscalls. 
 * It also contains the routine to fork and exec the
 * k5dcecon routine to do most of the work. 
 * 
 * This file is designed to be as independent of DCE 
 * and DFS as possible. The only dependencies are on 
 * the syscall numbers.  If DFS not running or not installed,
 * the sig handlers will catch and the signal and 
 * will  continue. 
 *
 * krb5_dfs_newpag and krb5_dfs_getpag should not be real
 * Kerberos routines, since they should be setpag and getpag
 * in the DCE library, but without the DCE baggage. 
 * Thus they don't have context, and don't return a krb5 error. 
 *
 * 
 * 
 * krb5_dfs_pag()
 */

#include <krb5.h>

#ifdef DCE

#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/param.h>

/* Only run this DFS PAG code on systems with POSIX
 * All that we are interested in dor:, AIX 4.x,
 * Solaris 2.5.x, HPUX 10.x  Even SunOS 4.1.4, AIX 3.2.5
 * and SGI 5.3 are OK.  This simplifies
 * the build/configure which I don't want to change now.
 * All of them also have waitpid as well. 
 */

#define POSIX_SETJMP
#define POSIX_SIGNALS
#define HAVE_WAITPID

#include <signal.h>
#include <setjmp.h>
#ifndef POSIX_SETJMP
#undef sigjmp_buf
#undef sigsetjmp
#undef siglongjmp
#define sigjmp_buf  jmp_buf
#define sigsetjmp(j,s)  setjmp(j)
#define siglongjmp  longjmp
#endif

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

#define krb5_sigtype void
#define WAIT_USES_INT
typedef krb5_sigtype sigtype;


/* 
 * Need some syscall numbers based on different systems. 
 * These are based on: 
 * HPUX 10.10 /opt/dce/include/dcedfs/syscall.h 
 * Solaris 2.5 /opt/dcelocal/share/include/dcedfs/syscall.h
 * AIX 4.2  - needs some funny games with load and kafs_syscall
 * to get the kernel extentions. There should be a better way! 
 * 
 * DEE 5/27/97
 *
 */


#define AFSCALL_SETPAG 2
#define AFSCALL_GETPAG 11

#if defined(sun)
#define AFS_SYSCALL  72

#elif defined(hpux)
/* assume HPUX 10 +  or is it 50 */
#define AFS_SYSCALL 326

#elif defined(_AIX)
#ifndef DPAGAIX
#define DPAGAIX LIBEXECDIR ## "/dpagaix"
#endif
int *load();
static int (*dpagaix)(int, int, int, int, int, int) = 0;

#elif defined(sgi) || defined(_sgi)
#define AFS_SYSCALL      206+1000

#else
#define AFS_SYSCALL (Unknown_DFS_AFS_SYSCALL)
#endif


#ifdef  WAIT_USES_INT
                int wait_status;
#else   /* WAIT_USES_INT */
                union wait wait_status;
#endif  /* WAIT_USES_INT */

#ifndef K5DCECON
#define K5DCECON LIBEXECDIR ## "/k5dcecon"
#endif

/* 
 * mysig()
 *
 * signal handler if DFS not running
 *
 */

static sigjmp_buf setpag_buf;

static sigtype mysig()
{
  siglongjmp(setpag_buf, 1);
}

/*
 * krb5_dfs_pag_syscall()
 *
 * wrapper for the syscall with signal handlers
 *
 */

static int  krb5_dfs_pag_syscall(opt1,opt2) 
  int opt1;
  int opt2;
{
  handler sa1, osa1;
  handler sa2, osa2;
  int pag = -2;

  handler_init (sa1, mysig);
  handler_init (sa2, mysig);
  handler_swap (SIGSYS, sa1, osa1);
  handler_swap (SIGSEGV, sa2, osa2);
  
  if (sigsetjmp(setpag_buf, 1) == 0) {

#if defined(_AIX)
    if (!dpagaix)
      dpagaix = load(DPAGAIX, 0, 0);
    if (dpagaix) 
      pag = (*dpagaix)(opt1, opt2, 0, 0, 0, 0);
#else
    pag = syscall(AFS_SYSCALL, opt1, opt2, 0, 0, 0, 0); 
#endif

    handler_set (SIGSYS, osa1);
    handler_set (SIGSEGV, osa2);
    return(pag);
  }

  /* syscall failed! return 0 */
  handler_set (SIGSYS, osa1);
  handler_set (SIGSEGV, osa2);
  return(-2);
}

/*
 * krb5_dfs_newpag()
 *
 * issue a DCE/DFS setpag system call to set the newpag
 * for this process. This takes advantage of a currently
 * undocumented feature of the Transarc port of DFS. 
 * Even in DCE 1.2.2 for which the source is available,
 * (but no vendors have released), this feature is not
 * there, but it should be, or could be added. 
 * If new_pag is zero, then the syscall will get a new pag
 * and return its value. 
 */ 

int krb5_dfs_newpag(new_pag) 
  int new_pag;
{
  return(krb5_dfs_pag_syscall(AFSCALL_SETPAG, new_pag));
}

/* 
 * krb5_dfs_getpag()
 *
 * get the current PAG. Used mostly as a test. 
 */

int krb5_dfs_getpag()
{
  return(krb5_dfs_pag_syscall(AFSCALL_GETPAG, 0));
}

/*
 * krb5_dfs_pag()
 *
 * Given a principal and local username,
 * fork and exec the k5dcecon module to create 
 * refresh or join a new DCE/DFS
 * Process Authentication Group (PAG)
 * 
 * This routine should be called after krb5_kuserok has 
 * determined that this combination of local user and 
 * principal are acceptable for the local host. 
 * 
 * It should also be called after a forwarded ticket has 
 * been received, and the KRB5CCNAME environment variable
 * has been set to point at it. k5dcecon will convert this
 * to a new DCE context and a new pag and replace KRB5CCNAME
 * in the environment. 
 *
 * If there is no forwarded ticket, k5dcecon will attempt
 * to join an existing PAG for the same principal and local
 * user. 
 *
 * And it should be called before access to the home directory
 * as this may be in DFS, not accessable by root, and require
 * the PAG to have been setup. 
 * 
 * The krb5_afs_pag can be called after this routine to 
 * use the the cache obtained by k5dcecon to get an AFS token. 
 * DEE - 7/97
 */ 
 
int krb5_dfs_pag(context, flag, principal, luser)
	krb5_context context;
    int flag; /* 1 if a forwarded TGT is to be used */
	krb5_principal principal;
	const char *luser;

{
  
  struct stat stx;
  int fd[2];
  int i,j;
  int pid;
  int new_pag;
  int pag;
  char newccname[MAXPATHLEN] = ""; 
  char *princ;
  int err; 
  struct sigaction newsig, oldsig;

#ifdef  WAIT_USES_INT
  int wait_status;
#else   /* WAIT_USES_INT */
  union wait wait_status;
#endif  /* WAIT_USES_INT */

  if (krb5_unparse_name(context, principal, &princ))
   return(0);

   /* test if DFS is running or installed */
   if (krb5_dfs_getpag() == -2)
     return(0); /* DFS not running, dont try */
  
  if (pipe(fd) == -1) 
     return(0);

  /* Make sure that telnetd.c's SIGCHLD action don't happen right now... */
  memset((char *)&newsig, 0, sizeof(newsig));
  newsig.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &newsig, &oldsig);

  pid = fork();
  if (pid <0) 
   return(0);

  if (pid == 0) {  /* child process */

    close(1);       /* close stdout */
    dup(fd[1]);     /* point stdout at pipe here */
    close(fd[0]);   /* don't use end of pipe here */
    close(fd[1]);   /* pipe now as stdout */

    execl(K5DCECON, "k5dcecon",
         (flag) ? "-f" : "-s" ,
		 "-l", luser,
		 "-p", princ, (char *)0);

    exit(127);      /* incase execl fails */
  } 

  /* parent, wait for child to finish */

  close(fd[1]);  /* dont need this end of pipe */

/* #if defined(sgi) || defined(_sgi) */
  /* wait_status.w_status = 0; */
  /* waitpid((pid_t) pid, &wait_status.w_status, 0); */
/* #else */


  wait_status = 0;
#ifdef  HAVE_WAITPID
  err = waitpid((pid_t) pid, &wait_status, 0);
#else   /* HAVE_WAITPID */
  err = wait4(pid, &wait_status, 0, (struct rusage *) NULL);
#endif  /* HAVE_WAITPID */
/* #endif */

  sigaction(SIGCHLD, &oldsig, 0);
  if (WIFEXITED(wait_status)){
    if (WEXITSTATUS(wait_status) == 0) {
      i = 1;
      j = 0;
      while (i != 0) {
        i = read(fd[0], &newccname[j], sizeof(newccname)-1-j);
        if ( i > 0)
          j += i;
        if (j >=  sizeof(newccname)-1)
          i = 0;
      }
      close(fd[0]);
      if (j > 0) {
        newccname[j] = '\0'; 
        esetenv("KRB5CCNAME",newccname,1);
        sscanf(&newccname[j-8],"%8x",&new_pag);
        if (new_pag && strncmp("FILE:/opt/dcelocal/var/security/creds/dcecred_", newccname, 46) == 0) {
          if((pag = krb5_dfs_newpag(new_pag)) != -2) {
            return(pag);
          }
        }
      }
    }
  }
  return(0); /* something not right */
}

#else /* DCE */

/* 
 * krb5_dfs_pag - dummy version for the lib for systems 
 * which don't have DFS, or the needed setpag kernel code.  
 */

krb5_boolean
krb5_dfs_pag(context, principal, luser)
	krb5_context context;
	krb5_principal principal;
	const char *luser;
{
	return(0);
}

#endif /* DCE */
