/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
/*static char sccsid[] = "from: @(#)trap.c	5.2 (Berkeley) 4/12/91";*/
static char rcsid[] = "trap.c,v 1.5 1993/08/06 21:50:18 mycroft Exp";
#endif /* not lint */

#include "shell.h"
#include "main.h"
#include "nodes.h"	/* for other headers */
#include "eval.h"
#include "jobs.h"
#include "options.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "trap.h"
#include "mystring.h"
#include <signal.h>


/*
 * Sigmode records the current value of the signal handlers for the various
 * modes.  A value of zero means that the current handler is not known.
 * S_HARD_IGN indicates that the signal was ignored on entry to the shell,
 */

#define S_DFL 1			/* default signal handling (SIG_DFL) */
#define S_CATCH 2		/* signal is caught */
#define S_IGN 3			/* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4		/* signal is ignored permenantly */


extern char nullstr[1];		/* null string */

char *trap[NSIG];		/* trap handler commands */
MKINIT char sigmode[NSIG];	/* current value of signal */
char gotsig[NSIG];		/* indicates specified signal received */
int pendingsigs;		/* indicates some signal received */

/*
 * The trap builtin.
 */

trapcmd(argc, argv)  char **argv; {
	char *action;
	char **ap;
	int signo;

	if (argc <= 1) {
		for (signo = 0 ; signo < NSIG ; signo++) {
			if (trap[signo] != NULL)
				out1fmt("%d: %s\n", signo, trap[signo]);
		}
		return 0;
	}
	ap = argv + 1;
	if (is_number(*ap))
		action = NULL;
	else
		action = *ap++;
	while (*ap) {
		if ((signo = number(*ap)) < 0 || signo >= NSIG)
			error("%s: bad trap", *ap);
		INTOFF;
		if (action)
			action = savestr(action);
		if (trap[signo])
			ckfree(trap[signo]);
		trap[signo] = action;
		if (signo != 0)
			setsignal(signo);
		INTON;
		ap++;
	}
	return 0;
}



/*
 * Clear traps on a fork.
 */

void
clear_traps() {
	char **tp;

	for (tp = trap ; tp < &trap[NSIG] ; tp++) {
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

int
setsignal(signo) {
	int action;
	sig_t sigact;
	char *t;
	extern void onsig();

	if ((t = trap[signo]) == NULL)
		action = S_DFL;
	else if (*t != '\0')
		action = S_CATCH;
	else
		action = S_IGN;
	if (rootshell && action == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag)
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
			/* FALLTHROUGH */
		case SIGTERM:
			if (iflag)
				action = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (jflag)
				action = S_IGN;
			break;
#endif
		}
	}
	t = &sigmode[signo];
	if (*t == 0) {	/* current setting unknown */
		/*
		 * There is a race condition here if action is not S_IGN.
		 * A signal can be ignored that shouldn't be.
		 */
		if ((int)(sigact = signal(signo, SIG_IGN)) == -1)
			error("Signal system call failed");
		if (sigact == SIG_IGN) {
			*t = S_HARD_IGN;
		} else {
			*t = S_IGN;
		}
	}
	if (*t == S_HARD_IGN || *t == action)
		return 0;
	switch (action) {
		case S_DFL:	sigact = SIG_DFL;	break;
		case S_CATCH:  	sigact = onsig;		break;
		case S_IGN:	sigact = SIG_IGN;	break;
	}
	*t = action;
	return (int)signal(signo, sigact);
}


/*
 * Ignore a signal.
 */

void
ignoresig(signo) {
	if (sigmode[signo] != S_IGN && sigmode[signo] != S_HARD_IGN) {
		signal(signo, SIG_IGN);
	}
	sigmode[signo] = S_HARD_IGN;
}


#ifdef mkinit
INCLUDE <sys/signal.h>
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
onsig(signo) {
	signal(signo, onsig);
	if (signo == SIGINT && trap[SIGINT] == NULL) {
		onint();
		return;
	}
	gotsig[signo] = 1;
	pendingsigs++;
}



/*
 * Called to execute a trap.  Perhaps we should avoid entering new trap
 * handlers while we are executing a trap handler.
 */

void
dotrap() {
	int i;
	int savestatus;

	for (;;) {
		for (i = 1 ; ; i++) {
			if (i >= NSIG)
				goto done;
			if (gotsig[i])
				break;
		}
		gotsig[i] = 0;
		savestatus=exitstatus;
		evalstring(trap[i]);
		exitstatus=savestatus;
	}
done:
	pendingsigs = 0;
}



/*
 * Controls whether the shell is interactive or not.
 */

int is_interactive;

void
setinteractive(on) {
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
exitshell(status) {
	struct jmploc loc1, loc2;
	char *p;

	TRACE(("exitshell(%d) pid=%d\n", status, getpid()));
	if (setjmp(loc1.loc))  goto l1;
	if (setjmp(loc2.loc))  goto l2;
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
