/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)trap.c	8.5 (Berkeley) 6/5/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "shell.h"
#include "main.h"
#include "nodes.h"	/* for other headers */
#include "eval.h"
#include "jobs.h"
#include "show.h"
#include "options.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "trap.h"
#include "mystring.h"


/*
 * Sigmode records the current value of the signal handlers for the various
 * modes.  A value of zero means that the current handler is not known.
 * S_HARD_IGN indicates that the signal was ignored on entry to the shell,
 */

#define S_DFL 1			/* default signal handling (SIG_DFL) */
#define S_CATCH 2		/* signal is caught */
#define S_IGN 3			/* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4		/* signal is ignored permanently */
#define S_RESET 5		/* temporary - to reset a hard ignored sig */


MKINIT char sigmode[NSIG];	/* current value of signal */
int pendingsigs;		/* indicates some signal received */
int in_dotrap;			/* do we execute in a trap handler? */
static char *volatile trap[NSIG];	/* trap handler commands */
static volatile sig_atomic_t gotsig[NSIG]; 
				/* indicates specified signal received */
static int ignore_sigchld;	/* Used while handling SIGCHLD traps. */

static int getsigaction(int, sig_t *);


/*
 * Map a string to a signal number.
 */
static int
sigstring_to_signum(char *sig)
{

	if (is_number(sig)) {
		int signo;

		signo = atoi(sig);
		return ((signo >= 0 && signo < NSIG) ? signo : (-1));
	} else if (strcasecmp(sig, "exit") == 0) {
		return (0);
	} else {
		int n;

		if (strncasecmp(sig, "sig", 3) == 0)
			sig += 3;
		for (n = 1; n < NSIG; n++)
			if (strcasecmp(sys_signame[n], sig) == 0)
				return (n);
	}
	return (-1);
}


/*
 * Print a list of valid signal names.
 */
static void
printsignals(void)
{
	int n;

	for (n = 1; n < NSIG; n++) {
		out1fmt("%s", sys_signame[n]);
		if (n == (NSIG / 2) || n == (NSIG - 1))
			out1str("\n");
		else
			out1c(' ');
	}
}


/*
 * The trap builtin.
 */
int
trapcmd(int argc, char **argv)
{
	char *action;
	int signo;

	if (argc <= 1) {
		for (signo = 0 ; signo < NSIG ; signo++) {
			if (trap[signo] != NULL)
				out1fmt("trap -- '%s' %s\n", trap[signo],
					(signo) ? sys_signame[signo] : "exit");
		}
		return 0;
	}
	action = NULL;
	if (*++argv && strcmp(*argv, "--") == 0)
		argv++;
	if (*argv && sigstring_to_signum(*argv) == -1) {
		if ((*argv)[0] != '-') {
			action = *argv;
			argv++;
		} else if ((*argv)[1] == '\0') {
			argv++;
		} else if ((*argv)[1] == 'l' && (*argv)[2] == '\0') {
			printsignals();
			return 0;
		} else {
			error("bad option %s", *argv);
		}
	}
	while (*argv) {
		if ((signo = sigstring_to_signum(*argv)) == -1)
			error("bad signal %s", *argv);
		INTOFF;
		if (action)
			action = savestr(action);
		if (trap[signo])
			ckfree(trap[signo]);
		trap[signo] = action;
		if (signo != 0)
			setsignal(signo);
		INTON;
		argv++;
	}
	return 0;
}


/*
 * Clear traps on a fork.
 */
void
clear_traps(void)
{
	char *volatile *tp;

	for (tp = trap ; tp <= &trap[NSIG - 1] ; tp++) {
		if (*tp && **tp) {	/* trap not NULL or SIG_IGN */
			INTOFF;
			ckfree(*tp);
			*tp = NULL;
			if (tp != &trap[0])
				setsignal(tp - trap);
			INTON;
		}
	}
}


/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */
void
setsignal(int signo)
{
	int action;
	sig_t sig, sigact = SIG_DFL;
	char *t;

	if ((t = trap[signo]) == NULL)
		action = S_DFL;
	else if (*t != '\0')
		action = S_CATCH;
	else
		action = S_IGN;
	if (action == S_DFL) {
		switch (signo) {
		case SIGINT:
			action = S_CATCH;
			break;
		case SIGQUIT:
#ifdef DEBUG
			{
			extern int debug;

			if (debug)
				break;
			}
#endif
			action = S_CATCH;
			break;
		case SIGTERM:
			if (rootshell && iflag)
				action = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (rootshell && mflag)
				action = S_IGN;
			break;
#endif
		}
	}

	t = &sigmode[signo];
	if (*t == 0) {
		/*
		 * current setting unknown
		 */
		if (!getsigaction(signo, &sigact)) {
			/*
			 * Pretend it worked; maybe we should give a warning
			 * here, but other shells don't. We don't alter
			 * sigmode, so that we retry every time.
			 */
			return;
		}
		if (sigact == SIG_IGN) {
			if (mflag && (signo == SIGTSTP ||
			     signo == SIGTTIN || signo == SIGTTOU)) {
				*t = S_IGN;	/* don't hard ignore these */
			} else
				*t = S_HARD_IGN;
		} else {
			*t = S_RESET;	/* force to be set */
		}
	}
	if (*t == S_HARD_IGN || *t == action)
		return;
	switch (action) {
		case S_DFL:	sigact = SIG_DFL;	break;
		case S_CATCH:  	sigact = onsig;		break;
		case S_IGN:	sigact = SIG_IGN;	break;
	}
	*t = action;
	sig = signal(signo, sigact);
#ifdef BSD
	if (sig != SIG_ERR && action == S_CATCH)
		siginterrupt(signo, 1);
#endif
}


/*
 * Return the current setting for sig w/o changing it.
 */
static int
getsigaction(int signo, sig_t *sigact)
{
	struct sigaction sa;

	if (sigaction(signo, (struct sigaction *)0, &sa) == -1)
		return 0;
	*sigact = (sig_t) sa.sa_handler;
	return 1;
}


/*
 * Ignore a signal.
 */
void
ignoresig(int signo)
{

	if (sigmode[signo] != S_IGN && sigmode[signo] != S_HARD_IGN) {
		signal(signo, SIG_IGN);
	}
	sigmode[signo] = S_HARD_IGN;
}


#ifdef mkinit
INCLUDE <signal.h>
INCLUDE "trap.h"

SHELLPROC {
	char *sm;

	clear_traps();
	for (sm = sigmode ; sm < sigmode + NSIG ; sm++) {
		if (*sm == S_IGN)
			*sm = S_HARD_IGN;
	}
}
#endif


/*
 * Signal handler.
 */
void
onsig(int signo)
{

#ifndef BSD
	signal(signo, onsig);
#endif
	if (signo == SIGINT && trap[SIGINT] == NULL) {
		onint();
		return;
	}

	if (signo != SIGCHLD || !ignore_sigchld)
		gotsig[signo] = 1;
	pendingsigs++;

	/* If we are currently in a wait builtin, prepare to break it */
	if ((signo == SIGINT || signo == SIGQUIT) && in_waitcmd != 0)
		breakwaitcmd = 1;
	/* 
	 * If a trap is set, not ignored and not the null command, we need 
	 * to make sure traps are executed even when a child blocks signals.
	 */
	if (Tflag &&
	    trap[signo] != NULL && 
	    ! trap[signo][0] == '\0' &&
	    ! (trap[signo][0] == ':' && trap[signo][1] == '\0'))
		breakwaitcmd = 1;
}


/*
 * Called to execute a trap.  Perhaps we should avoid entering new trap
 * handlers while we are executing a trap handler.
 */
void
dotrap(void)
{
	int i;
	int savestatus;

	in_dotrap++;
	for (;;) {
		for (i = 1; i < NSIG; i++) {
			if (gotsig[i]) {
				gotsig[i] = 0;
				if (trap[i]) {
					/*
					 * Ignore SIGCHLD to avoid infinite recursion
					 * if the trap action does a fork.
					 */
					if (i == SIGCHLD)
						ignore_sigchld++;
					savestatus = exitstatus;
					evalstring(trap[i]);
					exitstatus = savestatus;
					if (i == SIGCHLD)
						ignore_sigchld--;
				}
				break;
			}
		}
		if (i >= NSIG)
			break;
	}
	in_dotrap--;
	pendingsigs = 0;
}


/*
 * Controls whether the shell is interactive or not.
 */
void
setinteractive(int on)
{
	static int is_interactive = -1;

	if (on == is_interactive)
		return;
	setsignal(SIGINT);
	setsignal(SIGQUIT);
	setsignal(SIGTERM);
	is_interactive = on;
}


/*
 * Called to exit the shell.
 */
void
exitshell(int status)
{
	struct jmploc loc1, loc2;
	char *p;

	TRACE(("exitshell(%d) pid=%d\n", status, getpid()));
	if (setjmp(loc1.loc)) {
		goto l1;
	}
	if (setjmp(loc2.loc)) {
		goto l2;
	}
	handler = &loc1;
	if ((p = trap[0]) != NULL && *p != '\0') {
		trap[0] = NULL;
		evalstring(p);
	}
l1:   handler = &loc2;			/* probably unnecessary */
	flushall();
#if JOBS
	setjobctl(0);
#endif
l2:   _exit(status);
}
