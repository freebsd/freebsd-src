/*
 * Copyright (c) 1993 Christoph M. Robitschko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * init.c
 * Main program for init.
 * Also contains definitions for global variables etc.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <setjmp.h>
#include <syslog.h>
#include <ttyent.h>
#include <string.h>

#include "init.h"
#include "prototypes.h"
#include "libutil.h"


/* global variables, preset to their defaults */
int		timeout_m2s_TERM = INIT_M2S_TERMTO;
int		timeout_m2s_KILL = INIT_M2S_KILLTO;
int		retrytime = RETRYTIME;
int		startup_single = 0;
int		checkonly = 0;
int		force_single = -1;
#ifdef DEBUG
int		force_debug = -1;
int		debug = DEBUG_LEVEL;
#endif
#ifdef CONFIGURE
char		*config_file = INIT_CONFIG;
#endif

static ttytab_t	*ttytab = (ttytab_t *)0;
static callout_t *callout_tab = (callout_t *)0;
static callout_t *callout_free = (callout_t *)0;
static int	callout_nfree = 0;
sigset_t	block_set;
jmp_buf		boing_singleuser,
		boing_single2multi,
		boing_multiuser,
		boing_multi2single,
		boing_waitforboot,
		*boing_m2stimeout;

static enum { SINGLEUSER, MULTIUSER, SINGLE2MULTI, MULTI2SINGLE }
		State;

struct ttyent	RCent_auto =		{
					"console",
					"/bin/sh sh /etc/rc autoboot",
					"dumb",
					TTY_ON | TTY_SECURE,
					0,
					0
					};
struct ttyent	RCent_fast =		{
					"console",
					"/bin/sh sh /etc/rc",
					"dumb",
					TTY_ON | TTY_SECURE,
					0,
					0
					};
struct ttyent	Single_ent =		{
					"console",
					"/bin/sh -",
					"pc3",
					TTY_ON | TTY_SECURE,
					0,
					0
					};
static struct ttyent	*RCent =	&RCent_auto;
static struct ttyent	*Singlesh =	&Single_ent;




/**********************************************************
 *                           Main                         *
 **********************************************************/
void
main(argc, argv)
int		argc;
char		**argv;
{


	/* make it a session leader */
	(void) setsid();

	/* initialize syslog */
	openlog("init", LOG_CONS | LOG_PID, LOG_DAEMON);


	/* parse command line */
	while(argc > 1) {
		if(!strcmp(argv[1], "-s"))		/* Singleuser */
			force_single = startup_single = 1;

		else if(!strcmp(argv[1], "-f"))		/* Fastboot */
			RCent = &RCent_fast;

#ifdef DEBUG
		else if (!strcmp(argv[1], "-d"))	/* Debug level */
			if (argc > 2) {
				if ((force_debug = str2u(argv[2])) >= 0) {
					debug = force_debug;
					argc --; argv++;
				} else
					syslog(LOG_ERR, "option -d needs positive integer argument");
			} else
				syslog(LOG_ERR, "option -d needs an argument");
#endif
#ifdef CONFIGURE
		else if (!strcmp(argv[1], "-C"))	/* Configuration file */
			if (argc > 2) {
				config_file = argv[2];
				argc--; argv++;
			} else
				syslog(LOG_ERR, "option -C needs an argument");

		else if (!strcmp(argv[1], "-S"))	/* Syntaxcheck only */
			checkonly = 1;
#endif
		else if (!strcmp(argv[1], "-"))		/* ignore this */
			;
		else if (!strcmp(argv[1], "--"))	/* ... and this */
			;
		else
			syslog(LOG_ERR, "unknown option \"%s\"", argv[1]);
		argc--; argv++;
	}


#ifndef TESTRUN
	/* did some idiot try to run init ? */
	if((getpid() != 1) && !checkonly) {
		const char errmsg[] = "init: system daemon, not runnable by user\r\n";
		write(2, errmsg, strlen(errmsg));
		exit(0xff);
	}
#endif /* ! TESTRUN */


#ifdef CONFIGURE
	/* read the default configuration (limits etc) */
	getconf();
	/* read configuration file */
	configure(config_file);
	/* set global configuration parameters */
	checkconf();
	if (checkonly)
		exit(0);

	/* values configured by command-line arguments take precedence	*/
	/* over values in the config file				*/
#  ifdef DEBUG
	if (force_debug >= 0)
		debug = force_debug;
#  endif
	if (force_single >= 0)
		startup_single = force_single;
#endif

	/*
	 * initialize callout table
	 */
	allocate_callout();

	/*
	 * initialize the longjmp buffers;
	 * after a longjmp(), the appropriate function is called and
	 * does not return.
	 */
	if (setjmp(boing_singleuser))
		singleuser();
	if (setjmp(boing_single2multi))
		single2multi();
	if (setjmp(boing_multiuser))
		multiuser();
	if (setjmp(boing_multi2single))
		multi2single();
	if (setjmp(boing_waitforboot))
		waitforboot();

	/* install signal handlers for catched signals */
	signal(SIGTSTP,	sig_tstp);
	signal(SIGTERM,	sig_term);
	signal(SIGHUP,	sig_hup);
	signal(SIGALRM, sig_alrm);
#ifdef DEBUG
	signal(SIGUSR1,	sig_usr1);
	signal(SIGUSR2,	sig_usr2);
#endif
#ifdef CONFIGURE
	signal(SIGTTIN, sig_ttin);
#endif
#if defined (UNTRUSTED) && !defined (TESTRUN)
	signal(SIGINT,  sig_int);
#endif

	/* define Set of signals to be blocked for critical parts */
	(void) sigemptyset (&block_set);
	(void) sigaddset (&block_set, SIGTSTP);
	(void) sigaddset (&block_set, SIGTERM);
	(void) sigaddset (&block_set, SIGHUP);
	(void) sigaddset (&block_set, SIGUSR1);
	(void) sigaddset (&block_set, SIGUSR2);
	(void) sigaddset (&block_set, SIGALRM);

	/* Action ! */
	if (startup_single)
		longjmp(boing_singleuser, 1);
	else
		longjmp(boing_single2multi, 1);

	/* NOTREACHED */
}



/**********************************************************
 *                      Signal Handlers                   *
 **********************************************************/

/* TSTP -- wait for children, but don't spawn new ones */
void
sig_tstp(sig)
int		sig;
{
	Debug(3, "TSTP Signal received");
	longjmp(boing_waitforboot, 2);
}


/* TERM -- Go to singleuser mode */
void
sig_term(sig)
int		sig;
{
	Debug(3, "Terminate Signal received");
	longjmp(boing_multi2single, 2);
}


/* HUP -- Reread /etc/ttys file */
void
sig_hup(sig)
int		sig;
{
	Debug(3, "Hangup Signal received");
	if (State == MULTIUSER)
		longjmp(boing_multiuser, 2);
}


/* ALRM -- Timeout Signal */
void
sig_alrm(sig)
int		sig;
{
	Debug(3, "Alarm Signal received");
	if (callout_tab)
		do_callout();
}


#ifdef DEBUG
/* USR1 -- Increment debugging level */
void
sig_usr1(sig)
int		sig;
{
	debug++;
	if (debug == 1)
		Debug(0, "I will chat like a gossip");
	else
		Debug(0, "I will chat like %d gossips", debug);
}

/* USR2 -- switch off debugging */
void
sig_usr2(sig)
int		sig;
{
	Debug(0, "OK, I will shut up now.");
	debug = 0;
}
#endif

#if defined (UNTRUSTED) && !defined (TESTRUN)
/* INT -- execute original init (Signal can be generated from the kernel
	debugger with 'call pfind(1)' and then 'call psignal(XXXXX, 2)'
	where XXXXX is the return value of the pfind call).
	This isn't very pretty, but it saved me from booting from floppy
	disk many times.  */
void
sig_int(sig)
int		sig;
{
	Debug(0, "Interrupt signal received; trying to execute /sbin/init.ori");
	Debug(0, "(Are you not satisfied with me ?)");
	kill (-1, SIGKILL);
	execl("/sbin/init.ori", "init", "-s", 0);
	Debug(0, "Could not execute /sbin/init.ori (%m)");
	longjmp(boing_multi2single, 1);
}
#endif /* UNTRUSTED */


#ifdef CONFIGURE
/* TTIN -- reread configuration file; only valid when in singleuser mode */
void
sig_ttin(sig)
int		sig;
{
	if (State == SINGLEUSER) {
		blocksig();
		Debug(0, "TTIN signal received, re-reading configuration file");
		setconf();
		configure(config_file);
		checkconf();
		unblocksig();
	} else
		syslog(LOG_NOTICE, "TTIN signal received, but not in singleuser mode");
}
#endif



/**********************************************************
 *                      SingleUserMode                    *
 **********************************************************/

void
singleuser(void)
{
int			status;


	State = SINGLEUSER;
	clear_callout();
	Debug(1, "Entered State singleuser");

	if (ttytab) {
		syslog(LOG_ERR, "internal error: multiple users in singleusermode");
		longjmp(boing_multi2single, 1);
	}


	RCent = &RCent_fast;
	blocksig();
	ttytab = ent_to_tab(Singlesh, (ttytab_t *)0, ttytab, INIT_NODEV | INIT_OPEN | INIT_ARG0);
	unblocksig();
	if (do_getty(ttytab, 0) < 0) {
		syslog(LOG_EMERG, "Unable to start singleuser shell");
		sync(); sleep(1);
		_exit(1);	/* What else should we do about this ? */
	}

#ifndef TESTRUN
	while(wait(&status) != ttytab->pid);
#else
	scanf("%d\n", &status);				/* XXX */
#endif
	Debug(1, "Singleusershell exited with status %d", status);


	blocksig();
	ttytab = free_tty(ttytab, ttytab);
	unblocksig();

	longjmp(boing_single2multi, 1);
	/* NOTREACHED */
}



/**********************************************************
 *                      Single 2 Multi                    *
 **********************************************************/

void
single2multi(void)
{
int			status;


	State = SINGLE2MULTI;
	clear_callout();
	Debug(1, "Entered State single2multi");

	if (ttytab) {
		syslog(LOG_ERR, "internal error: users in single2multi");
		longjmp(boing_multi2single, 1);
	}


	blocksig();
	ttytab = ent_to_tab(RCent, (ttytab_t *)0, ttytab, INIT_NODEV | INIT_OPEN | INIT_ARG0);
	unblocksig();
	if (do_getty(ttytab, 0) < 0) {
		syslog(LOG_ERR, "Unable to execute /etc/rc");
		ttytab = free_tty(ttytab, ttytab);
		longjmp(boing_singleuser, 2);
	}

#ifndef TESTRUN
	while(wait(&status) != ttytab->pid);
#else
	scanf("%d\n", &status);
#endif
	Debug(1, "/etc/rc exited with status %d", status);


	blocksig();
	ttytab = free_tty(ttytab, ttytab);
	unblocksig();

	if (status)
		longjmp(boing_singleuser, 1);
	else {
		logwtmp("~", "reboot", "");
		longjmp(boing_multiuser, 1);
	}
	/* NOTREACHED */
}



/**********************************************************
 *                       WaitForBoot                      *
 **********************************************************/

void
waitforboot(void)
{
int			status;
pid_t			pid;
ttytab_t		*tt;



	/* Note that the State variable is not set here */
	clear_callout();
	Debug(1, "Entered State waitforboot");

	while (1) {
	    pid = wait(&status);
	    if (pid < 0)
		pause();
	    else {
		Debug(4, "Process %d exited with status %d", pid, status);
		for (tt=ttytab; tt; tt = tt->next)
		    if (tt->pid == pid) {
			blocksig();
			ttytab = free_tty(ttytab, tt);
			unblocksig();
			break;
		    }
	    }
	}
	/* NOTREACHED */
}



/**********************************************************
 *                        MultiUser                       *
 **********************************************************/

void
multiuser(void)
{
ttytab_t		*tt;
struct ttyent		*tent;
int			pid;
int			status;



	State = MULTIUSER;
	clear_callout();
	Debug(1, "Entered State multiuser");

	/* First, (re)build ttytab based on what is in /etc/ttys */
	blocksig();
	setttyent();

	for (tt = ttytab; tt; tt = tt->next)
		tt->intflags &= ~(INIT_SEEN | INIT_CHANGED | INIT_NEW);

	while ((tent = getttyent()))
	    if (tent->ty_status & TTY_ON) {
		for (tt = ttytab; tt; tt = tt->next)
		    if (!strcmp(tent->ty_name, tt->name))
		   	break;
		ttytab = ent_to_tab(tent, tt, ttytab, 0);
	    }

	unblocksig();

	/* Kill the processes whose entries are deleted or changed */
	/* Also start the getty process on the lines that were just added */
	for (tt = ttytab; tt; tt = tt->next)
	    if (!(tt->intflags & INIT_SEEN)) {
	    	Debug(5, "killing %s (PID %d): Not seen", tt->name, tt->pid);
	    	kill (tt->pid, SIGKILL);
	    	tt->intflags |= INIT_DONTSPAWN;
	    }
	    else if (tt->intflags & INIT_NEW)
		(void)do_getty(tt, 0);
	    else if (tt->intflags & INIT_CHANGED) {
	    	Debug(5, "killing %s (PID %d): Changed", tt->name, tt->pid);
	        kill (tt->pid, SIGKILL);
	    }
	    else if (tt->intflags & INIT_FAILSLEEP) {
	    	Debug(5, "continuing %s (PID %d)", tt->name, tt->pid);
#define UNSLEEP(a)
	    	UNSLEEP(tt->pid);
	    	tt->intflags &= ~INIT_FAILED | INIT_FAILSLEEP;
	    	do_getty(tt, 0);
	    }


	/* Now handle terminating children and respawn gettys for lines */
	while (1) {
	    pid = wait(&status);
	    if (pid < 0) {
	    	switch (errno) {
	    		case EINTR: break;
	    		case ECHILD:
	    			syslog(LOG_ERR, "wait() found no child processes -- going singleuser.");
	    			longjmp(boing_multi2single, 2);
	    		default:
	    			syslog(LOG_ERR, "wait() failed: %m");
	    			sleep(5);
	    	}
	    } else {
	    	Debug(4, "Process %d terminated with status %d", pid, status);
	    	for (tt = ttytab; tt; tt = tt->next)
	    	    if (pid == tt->pid)
	    	    	if (tt->intflags & INIT_DONTSPAWN) {
	    	    	    blocksig();
	    	    	    ttytab = free_tty(ttytab, tt);
	    	    	    unblocksig();
	    	    	}
	    	    	else
	    	    	    (void)do_getty(tt, 0);
	    }
	}
	/* NOTREACHED */
}



/**********************************************************
 *                       Multi2Single                     *
 **********************************************************/

void
multi2single(void)
{
static jmp_buf	boing_timeout;
pid_t		pid;
int		status;
volatile int	round;


	State = MULTI2SINGLE;
	clear_callout();
	Debug(1, "Entering State multi2single");

	/* forget about the gettys */
	blocksig();
	while (ttytab)
		ttytab = free_tty(ttytab, ttytab);
	unblocksig();

	/*
	 * round = 1: TERMinate children, then wait for them (default 10 seconds)
	 * round = 2: KILL children, then wait for them (default 30 seconds)
	 * round = 3: timeout expired; go to singleuser mode
	 */
	round = 0;
	setjmp(boing_timeout);
	boing_m2stimeout = &boing_timeout;
	round ++;
	if (round < 3) {
	    if (round == 1){
	        Debug(3, "TERMinating processes");
		kill (-1, SIGTERM);
		kill (-1, SIGCONT);
		callout (timeout_m2s_TERM, CO_MUL2SIN, (void *)0);
	    }
	    else {
		Debug(3, "KILLing processes");
		kill (-1, SIGKILL);
		kill (-1, SIGCONT);
		callout (timeout_m2s_KILL, CO_MUL2SIN, (void *)0);
	    }
	    while ((pid = wait(&status)) >= 0)
		Debug(4, "Process %d exited with status %d", pid, status);
	    Debug(2, "Wait returned error: %m");
	}
	else
	    syslog(LOG_NOTICE, "There are still some (hung) processes.");

	/* We don't need no steenkin timeout any more... */
	boing_m2stimeout = (jmp_buf *)0;

	/* Jump ! (Rein ins Vergnuegen) */
	longjmp(boing_singleuser, 2);
	/* NOTREACHED */
}



/**********************************************************
 *                         Callout                        *
 * Schedule a retry operation for a later time            *
 **********************************************************/
void
callout(when, type, arg)
unsigned int	when;
retr_t		type;
void		*arg;
{
callout_t	*ntp,
		*ctp,
		*octp;


	Debug(3, "Scheduling callout in %d seconds.", when);
	blocksig();

	/* find a free callout entry */
	if (callout_nfree <= CALLOUT_MINFREE)
		allocate_callout();
	ntp = callout_free;
	if (!ntp) {
		syslog(LOG_WARNING, "Callout table is full !");
		return;
	}
	callout_free = ntp->next;
	callout_nfree --;

	/* look at which point we put it in the callout list */
	when += (unsigned int)time(NULL);
	for (octp = NULL, ctp = callout_tab; ctp; octp = ctp, ctp = ctp->next)
		if (when < ctp->sleept)
			break;
		else
			when -= ctp->sleept;


	if (octp)
		octp->next = ntp;
	else
		callout_tab = ntp;
	ntp->next = ctp;
	ntp->sleept = when;
	if (ctp) ctp->sleept -= when;
	ntp->what = type;
	ntp->arg = arg;

	/* schedule alarm */
	when = callout_tab->sleept - (unsigned int)time(NULL);
	if (when <= 0) {
		Debug(4, "Next callout: NOW !");
		alarm(0);
		kill (getpid(), SIGALRM);
	} else
		Debug(4, "Next callout in %d seconds.", when);
		alarm(when);

	unblocksig();
}


/**********************************************************
 *                      Allocate_Callout                  *
 * allocate (further) elements to the callout table       *
 *********************************************************/
void
allocate_callout(void)
{
callout_t	 *ntp;
int		i;


	ntp = malloc(sizeof(callout_t) * CALLOUT_CHUNK);
	if (ntp) {
		for (i=1; i< CALLOUT_CHUNK; i++)
			ntp[i].next = &ntp[i-1];
		ntp->next = callout_free;
		callout_free = &ntp[i-1];
		callout_nfree += CALLOUT_CHUNK;
	}
}


/**********************************************************
 *                      Clear_Callout                     *
 * Removes all callout entries                            *
 *********************************************************/
void
clear_callout(void)
{
callout_t	*ctp,
		*nctp;


	if (callout_tab)
		Debug(4, "All callouts for today cancelled.");
	blocksig();
	alarm(0);
	for (ctp = callout_tab; ctp; ctp = nctp) {
		nctp = ctp->next;
		ctp->next = callout_free;
		callout_free = ctp;
		callout_nfree ++;
	}
	callout_tab = (callout_t *)0;
	unblocksig();
}



/**********************************************************
 *                         Do_Callout                     *
 * calls the callback routines when the time has expired  *
 *********************************************************/
void
do_callout(void)
{
callout_t	*ctp;
unsigned int	now;


	now = (unsigned int) time(NULL);
	for (ctp = callout_tab; ctp;) {
		if (ctp->sleept > now)
			break;
		ctp = ctp->next;
		if (ctp)
			ctp->sleept += callout_tab->sleept;
		callout_tab->next = callout_free;
		callout_free = callout_tab;
		callout_tab = ctp;
		callout_nfree ++;

		switch (callout_free->what) {
			case CO_ENT2TAB:
				Debug(3, "Callout -> Multiuser");
				longjmp(boing_multiuser, 2);
			case CO_FORK:
			case CO_GETTY:
				Debug(3, "Callout -> do_getty()");
				(void)do_getty((ttytab_t *)callout_free->arg, 0);
				break;
			case CO_MUL2SIN:
				Debug(3, "Callout -> M2S timeout");
				if (boing_m2stimeout)
					longjmp(*boing_m2stimeout, 2);
		}
	}

	/* schedule next alarm */
	if (callout_tab) {
		Debug(4, "Next callout in %d seconds.", callout_tab->sleept - now);
		alarm(callout_tab->sleept - now);
	}
}

		
/**********************************************************
 *                      SignalsForChile                   *
 * Set up signals for the child processes                 *
 *********************************************************/
void
signalsforchile(void)
{
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGHUP , SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	signal(SIGINT,  SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	unblocksig();
}
