#include <signal.h>
#include "header.h"			/* "Larn is copyrighted 1986 by Noah Morgan.\n" */
#define BIT(a) (1<<((a)-1))
extern char savefilename[],wizard,predostuff,nosignal;
static s2choose()	/* text to be displayed if ^C during intro screen */
	{
	cursor(1,24); lprcat("Press "); setbold(); lprcat("return"); resetbold();
	lprcat(" to continue: ");   lflush(); 
	}

static void
cntlc()	/* what to do for a ^C */
	{
	if (nosignal) return;	/* don't do anything if inhibited */
	signal(SIGQUIT,SIG_IGN);	signal(SIGINT,SIG_IGN);
	quit(); if (predostuff==1) s2choose(); else showplayer();
	lflush();
	signal(SIGQUIT,cntlc);	signal(SIGINT,cntlc);
	}

/*
 *	subroutine to save the game if a hangup signal
 */
static void
sgam()
	{
	savegame(savefilename);  wizard=1;  died(-257); /* hangup signal */
	}

#ifdef SIGTSTP
static void
tstop() /* control Y	*/
	{
	if (nosignal)   return;  /* nothing if inhibited */
	lcreat((char*)0);  clearvt100();	lflush();	  signal(SIGTSTP,SIG_DFL);
#ifdef SIGVTALRM
	/* looks like BSD4.2 or higher - must clr mask for signal to take effect*/
	sigsetmask(sigblock(0)& ~BIT(SIGTSTP));
#endif
	kill(getpid(),SIGTSTP);

	setupvt100();  signal(SIGTSTP,tstop);
	if (predostuff==1) s2choose(); else drawscreen();
	showplayer();	lflush();
	}
#endif SIGTSTP

/*
 *	subroutine to issue the needed signal traps  called from main()
 */
static void sigpanic();
static void sigill()	{ sigpanic(SIGILL); }
static void sigtrap()	{ sigpanic(SIGTRAP); }
static void sigiot()	{ sigpanic(SIGIOT); }
static void sigemt()	{ sigpanic(SIGEMT); }
static void sigfpe()	{ sigpanic(SIGFPE); }
static void sigbus()	{ sigpanic(SIGBUS); }
static void sigsegv()	{ sigpanic(SIGSEGV); }
static void sigsys()	{ sigpanic(SIGSYS); }
static void sigpipe()	{ sigpanic(SIGPIPE); }
static void sigterm()	{ sigpanic(SIGTERM); }
sigsetup()
	{
	signal(SIGQUIT, cntlc); 		signal(SIGINT,  cntlc); 
	signal(SIGKILL, SIG_IGN);		signal(SIGHUP,  sgam);
	signal(SIGILL,  sigill);		signal(SIGTRAP, sigtrap);
	signal(SIGIOT,  sigiot);		signal(SIGEMT,  sigemt);
	signal(SIGFPE,  sigfpe);		signal(SIGBUS,  sigbus);
	signal(SIGSEGV, sigsegv);		signal(SIGSYS,  sigsys);
	signal(SIGPIPE, sigpipe);		signal(SIGTERM, sigterm);
#ifdef SIGTSTP
	signal(SIGTSTP,tstop);		signal(SIGSTOP,tstop);
#endif SIGTSTP
	}

#ifdef BSD	/* for BSD UNIX? */

static char *signame[NSIG] = { "",
"SIGHUP",  /*	1	 hangup */
"SIGINT",  /*	2	 interrupt */
"SIGQUIT", /*	3	 quit */
"SIGILL",  /*	4	 illegal instruction (not reset when caught) */
"SIGTRAP", /*	5	 trace trap (not reset when caught) */
"SIGIOT",  /*	6	 IOT instruction */
"SIGEMT",  /*	7	 EMT instruction */
"SIGFPE",  /*	8	 floating point exception */
"SIGKILL", /*	9	 kill (cannot be caught or ignored) */
"SIGBUS",  /*	10	 bus error */
"SIGSEGV", /*	11	 segmentation violation */
"SIGSYS",  /*	12	 bad argument to system call */
"SIGPIPE", /*	13	 write on a pipe with no one to read it */
"SIGALRM", /*	14	 alarm clock */
"SIGTERM", /*	15	 software termination signal from kill */
"SIGURG",  /*	16	 urgent condition on IO channel */
"SIGSTOP", /*	17	 sendable stop signal not from tty */
"SIGTSTP", /*	18	 stop signal from tty */
"SIGCONT", /*	19	 continue a stopped process */
"SIGCHLD", /*	20	 to parent on child stop or exit */
"SIGTTIN", /*	21	 to readers pgrp upon background tty read */
"SIGTTOU", /*	22	 like TTIN for output if (tp->t_local&LTOSTOP) */
"SIGIO",   /*	23	 input/output possible signal */
"SIGXCPU", /*	24	 exceeded CPU time limit */
"SIGXFSZ", /*	25	 exceeded file size limit */
"SIGVTALRM",/*  26	 virtual time alarm */
"SIGPROF", /*	27	 profiling time alarm */
"","","","" };

#else BSD	/* for system V? */

static char *signame[NSIG] = { "",
"SIGHUP",  /*	1	 hangup */
"SIGINT",  /*	2	 interrupt */
"SIGQUIT", /*	3	 quit */
"SIGILL",  /*	4	 illegal instruction (not reset when caught) */
"SIGTRAP", /*	5	 trace trap (not reset when caught) */
"SIGIOT",  /*	6	 IOT instruction */
"SIGEMT",  /*	7	 EMT instruction */
"SIGFPE",  /*	8	 floating point exception */
"SIGKILL", /*	9	 kill (cannot be caught or ignored) */
"SIGBUS",  /*	10	 bus error */
"SIGSEGV", /*	11	 segmentation violation */
"SIGSYS",  /*	12	 bad argument to system call */
"SIGPIPE", /*	13	 write on a pipe with no one to read it */
"SIGALRM", /*	14	 alarm clock */
"SIGTERM", /*	15	 software termination signal from kill */
"SIGUSR1",  /*	16	 user defines signal 1 */
"SIGUSR2", /*	17	 user defines signal 2 */
"SIGCLD",  /*	18	 child death */
"SIGPWR",  /*	19	 power fail */
"","","","","","","","","","","","" };

#endif BSD

/*
 *	routine to process a fatal error signal
 */
static void
sigpanic(sig)
	int sig;
	{
	char buf[128];
	signal(sig,SIG_DFL);
	sprintf(buf,"\nLarn - Panic! Signal %d received [%s]",sig,signame[sig]);
	write(2,buf,strlen(buf));  sleep(2);
	sncbr();
	savegame(savefilename); 
	kill(getpid(),sig); /* this will terminate us */
	}
