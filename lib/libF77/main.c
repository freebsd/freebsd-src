/* STARTUP PROCEDURE FOR UNIX FORTRAN PROGRAMS */

#include "stdio.h"
#include "signal.h"

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif

#ifndef KR_headers
#include "stdlib.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#ifdef NO__STDC
#define ONEXIT onexit
extern void f_exit();
#else
#ifndef KR_headers
extern void f_exit(void);
#ifndef NO_ONEXIT
#define ONEXIT atexit
extern int atexit(void (*)(void));
#endif
#else
#ifndef NO_ONEXIT
#define ONEXIT onexit
extern void f_exit();
#endif
#endif
#endif

#ifdef KR_headers
extern void f_init(), sig_die();
extern int MAIN__();
#define Int /* int */
#else
extern void f_init(void), sig_die(char*, int);
extern int MAIN__(void);
#define Int int
#endif

static void sigfdie(Int n)
{
sig_die("Floating Exception", 1);
}


static void sigidie(Int n)
{
sig_die("IOT Trap", 1);
}

#ifdef SIGQUIT
static void sigqdie(Int n)
{
sig_die("Quit signal", 1);
}
#endif


static void sigindie(Int n)
{
sig_die("Interrupt", 0);
}

static void sigtdie(Int n)
{
sig_die("Killed", 0);
}

#ifdef SIGTRAP
static void sigtrdie(Int n)
{
sig_die("Trace trap", 1);
}
#endif


int xargc;
char **xargv;

#ifdef KR_headers
main(argc, argv) int argc; char **argv;
#else
main(int argc, char **argv)
#endif
{
xargc = argc;
xargv = argv;
signal(SIGFPE, sigfdie);	/* ignore underflow, enable overflow */
signal(SIGIOT, sigidie);
#ifdef SIGTRAP
signal(SIGTRAP, sigtrdie);
#endif
#ifdef SIGQUIT
if(signal(SIGQUIT,sigqdie) == SIG_IGN)
	signal(SIGQUIT, SIG_IGN);
#endif
if(signal(SIGINT, sigindie) == SIG_IGN)
	signal(SIGINT, SIG_IGN);
signal(SIGTERM,sigtdie);

#ifdef pdp11
	ldfps(01200); /* detect overflow as an exception */
#endif

f_init();
#ifndef NO_ONEXIT
ONEXIT(f_exit);
#endif
MAIN__();
#ifdef NO_ONEXIT
f_exit();
#endif
exit(0);	/* exit(0) rather than return(0) to bypass Cray bug */
return 0;	/* For compilers that complain of missing return values; */
		/* others will complain that this is unreachable code. */
}
#ifdef __cplusplus
	}
#endif
