/*
 * Missing stuff from OS's
 */

#ifndef lint
#include <sys/cdefs.h>
__RCSID("$FreeBSD$");
#endif

#include <stdio.h>
#include <errno.h>
#include "make.h"

#if !__STDC__
# ifndef const
#  define const
# endif
#endif

#ifdef sun
extern int errno, sys_nerr;
extern char *sys_errlist[];

char *
strerror(e)
    int e;
{
    static char buf[100];
    if (e < 0 || e >= sys_nerr) {
	sprintf(buf, "Unknown error %d", e);
	return buf;
    }
    else
	return sys_errlist[e];
}
#endif

#ifdef ultrix
#include <string.h>

/* strdup
 *
 * Make a duplicate of a string.
 * For systems which lack this function.
 */
char *
strdup(str)
    const char *str;
{
    size_t len;

    if (str == NULL)
	return NULL;
    len = strlen(str) + 1;
    if ((p = malloc(len)) == NULL)
	return NULL;

    return memcpy(p, str, len);
}

#endif

#if defined(sun) || defined(__hpux) || defined(__sgi)

int
setenv(name, value, dum)
    const char *name;
    const char *value;
    int dum;
{
    register char *p;
    int len = strlen(name) + strlen(value) + 2; /* = \0 */
    char *ptr = (char*) malloc(len);

    (void) dum;

    if (ptr == NULL)
	return -1;

    p = ptr;

    while (*name)
	*p++ = *name++;

    *p++ = '=';

    while (*value)
	*p++ = *value++;

    *p = '\0';

    len = putenv(ptr);
/*    free(ptr); */
    return len;
}
#endif

#ifdef __hpux
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


int
killpg(pid, sig)
    int pid, sig;
{
    return kill(-pid, sig);
}

void
srandom(seed)
    long seed;
{
    srand48(seed);
}

long
random()
{
    return lrand48();
}

/* turn into bsd signals */
void (*
signal(s, a)) ()
    int     s;
    void (*a)();
{
    struct sigvec osv, sv;

    (void) sigvector(s, (struct sigvec *) 0, &osv);
    sv = osv;
    sv.sv_handler = a;
#ifdef SV_BSDSIG
    sv.sv_flags = SV_BSDSIG;
#endif

    if (sigvector(s, &sv, (struct sigvec *) 0) == -1)
        return (BADSIG);
    return (osv.sv_handler);
}

#if !defined(BSD) && !defined(d_fileno)
# define d_fileno d_ino
#endif

#ifndef DEV_DEV_COMPARE
# define DEV_DEV_COMPARE(a, b) ((a) == (b))
#endif
#define ISDOT(c) ((c)[0] == '.' && (((c)[1] == '\0') || ((c)[1] == '/')))
#define ISDOTDOT(c) ((c)[0] == '.' && ISDOT(&((c)[1])))


/* strrcpy():
 *	Like strcpy, going backwards and returning the new pointer
 */
static char *
strrcpy(ptr, str)
    register char *ptr, *str;
{
    register int len = strlen(str);

    while (len)
	*--ptr = str[--len];

    return (ptr);
} /* end strrcpy */


char   *
getwd(pathname)
    char   *pathname;
{
    DIR    *dp;
    struct dirent *d;

    struct stat st_root, st_cur, st_next, st_dotdot;
    char    pathbuf[MAXPATHLEN], nextpathbuf[MAXPATHLEN * 2];
    char   *pathptr, *nextpathptr, *cur_name_add;

    /* find the inode of root */
    if (stat("/", &st_root) == -1) {
	(void) sprintf(pathname,
			"getwd: Cannot stat \"/\" (%s)", strerror(errno));
	return (NULL);
    }
    pathbuf[MAXPATHLEN - 1] = '\0';
    pathptr = &pathbuf[MAXPATHLEN - 1];
    nextpathbuf[MAXPATHLEN - 1] = '\0';
    cur_name_add = nextpathptr = &nextpathbuf[MAXPATHLEN - 1];

    /* find the inode of the current directory */
    if (lstat(".", &st_cur) == -1) {
	(void) sprintf(pathname,
			"getwd: Cannot stat \".\" (%s)", strerror(errno));
	return (NULL);
    }
    nextpathptr = strrcpy(nextpathptr, "../");

    /* Descend to root */
    for (;;) {

	/* look if we found root yet */
	if (st_cur.st_ino == st_root.st_ino &&
	    DEV_DEV_COMPARE(st_cur.st_dev, st_root.st_dev)) {
	    (void) strcpy(pathname, *pathptr != '/' ? "/" : pathptr);
	    return (pathname);
	}

	/* open the parent directory */
	if (stat(nextpathptr, &st_dotdot) == -1) {
	    snprintf(pathname, sizeof(pathname),
			    "getwd: Cannot stat directory \"%s\" (%s)",
			    nextpathptr, strerror(errno));
	    return (NULL);
	}
	if ((dp = opendir(nextpathptr)) == NULL) {
	     snprintf(pathname, sizeof(pathname),
			    "getwd: Cannot open directory \"%s\" (%s)",
			    nextpathptr, strerror(errno));
	    return (NULL);
	}

	/* look in the parent for the entry with the same inode */
	if (DEV_DEV_COMPARE(st_dotdot.st_dev, st_cur.st_dev)) {
	    /* Parent has same device. No need to stat every member */
	    for (d = readdir(dp); d != NULL; d = readdir(dp))
		if (d->d_fileno == st_cur.st_ino)
		    break;
	}
	else {
	    /*
	     * Parent has a different device. This is a mount point so we
	     * need to stat every member
	     */
	    for (d = readdir(dp); d != NULL; d = readdir(dp)) {
		if (ISDOT(d->d_name) || ISDOTDOT(d->d_name))
		    continue;
		(void) strcpy(cur_name_add, d->d_name);
		if (lstat(nextpathptr, &st_next) == -1) {
		    snprintf(pathname, sizeof(pathname), "getwd: Cannot stat \"%s\" (%s)",
				    d->d_name, strerror(errno));
		    (void) closedir(dp);
		    return (NULL);
		}
		/* check if we found it yet */
		if (st_next.st_ino == st_cur.st_ino &&
		    DEV_DEV_COMPARE(st_next.st_dev, st_cur.st_dev))
		    break;
	    }
	}
	if (d == NULL) {
	    (void) sprintf(pathname, "getwd: Cannot find \".\" in \"..\"");
	    (void) closedir(dp);
	    return (NULL);
	}
	st_cur = st_dotdot;
	pathptr = strrcpy(pathptr, d->d_name);
	pathptr = strrcpy(pathptr, "/");
	nextpathptr = strrcpy(nextpathptr, "../");
	(void) closedir(dp);
	*cur_name_add = '\0';
    }
} /* end getwd */


char    *sys_siglist[] = {
        "Signal 0",
        "Hangup",                       /* SIGHUP    */
        "Interrupt",                    /* SIGINT    */
        "Quit",                         /* SIGQUIT   */
        "Illegal instruction",          /* SIGILL    */
        "Trace/BPT trap",               /* SIGTRAP   */
        "IOT trap",                     /* SIGIOT    */
        "EMT trap",                     /* SIGEMT    */
        "Floating point exception",     /* SIGFPE    */
        "Killed",                       /* SIGKILL   */
        "Bus error",                    /* SIGBUS    */
        "Segmentation fault",           /* SIGSEGV   */
        "Bad system call",              /* SIGSYS    */
        "Broken pipe",                  /* SIGPIPE   */
        "Alarm clock",                  /* SIGALRM   */
        "Terminated",                   /* SIGTERM   */
        "User defined signal 1",        /* SIGUSR1   */
        "User defined signal 2",        /* SIGUSR2   */
        "Child exited",                 /* SIGCLD    */
        "Power-fail restart",           /* SIGPWR    */
        "Virtual timer expired",        /* SIGVTALRM */
        "Profiling timer expired",      /* SIGPROF   */
        "I/O possible",                 /* SIGIO     */
        "Window size changes",          /* SIGWINDOW */
        "Stopped (signal)",             /* SIGSTOP   */
        "Stopped",                      /* SIGTSTP   */
        "Continued",                    /* SIGCONT   */
        "Stopped (tty input)",          /* SIGTTIN   */
        "Stopped (tty output)",         /* SIGTTOU   */
        "Urgent I/O condition",         /* SIGURG    */
        "Remote lock lost (NFS)",       /* SIGLOST   */
        "Signal 31",                    /* reserved  */
        "DIL signal"                    /* SIGDIL    */
};

int
utimes(file, tvp)
    char *file;
    struct timeval tvp[2];
{
    struct utimbuf t;

    t.actime  = tvp[0].tv_sec;
    t.modtime = tvp[1].tv_sec;
    return(utime(file, &t));
}


#endif /* __hpux */

#if defined(sun) && defined(__svr4__)
#include <signal.h>

/* turn into bsd signals */
void (*
signal(s, a)) ()
    int     s;
    void (*a)();
{
    struct sigaction sa, osa;

    sa.sa_handler = a;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(s, &sa, &osa) == -1)
	return SIG_ERR;
    else
	return osa.sa_handler;
}

#endif
