/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)intr.c	8.1 (Berkeley) 3/23/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"

/* 
 * There's a count of how deep the interrupt level in the SCR structure
 * has gone.  Each routine that wants to be interrupted increments this
 * level, and decrements it when it tears the interrupts down.  There are
 * two simplifying assumptions:
 *
 *	1: All interruptible areas want the same handler (we don't have
 *	   to save/restore the old handler).
 *	2: This is the only way to turn on interrupts (we don't have to
 *	   worry about them already being turned on if the interrupt
 *	   level is 0).
 *
 * We do it this way because interrupts have to be very fast -- if the
 * O_REMAPMAX option is turned off, we are setting interrupts per key
 * stroke.
 *
 * If an interrupt arrives, the S_INTERRUPTED bit is set in any SCR that
 * has the S_INTERRUPTIBLE bit set.  In the future this may be a problem.
 * The user should be able to move to another screen and keep typing while
 * another screen runs.  Currently, if the user does this and the user has
 * more than one interruptible thing running, there will be no way to know
 * which one to stop.
 */

static void intr_def __P((int));

/*
 * intr_init --
 *	Set up a interrupts.
 */
int
intr_init(sp)
	SCR *sp;
{
	struct sigaction act;
	struct termios nterm;

	/* You can never interrupt sessions not using tty's. */
	if (!F_ISSET(sp->gp, G_STDIN_TTY))
		return (1);

	/* If interrupts already set up, just increase the level. */
	if (sp->intr_level++)
		return (0);

	/* Turn interrupts on in this screen. */
	F_SET(sp, S_INTERRUPTIBLE);

	/* Install a handler. */
	act.sa_handler = intr_def;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGINT, &act, &sp->intr_act)) {
		msgq(sp, M_SYSERR, "sigaction");
		goto err1;
	}

	/*
	 * Turn on interrupts.  ISIG turns on VINTR, VQUIT and VSUSP.  We
	 * want VINTR to interrupt, so we install a handler.  VQUIT is
	 * ignored by main() because nvi never wants to catch it.  A handler
	 * for VSUSP should have been installed by the screen code.
	 */
	if (tcgetattr(STDIN_FILENO, &sp->intr_term)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err2;
	}
	nterm = sp->intr_term;
	nterm.c_lflag |= ISIG;
	if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &nterm)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		/*
		 * If an error occurs, back out the changes and run
		 * without interrupts.
		 */
err2:		(void)sigaction(SIGINT, &sp->intr_act, NULL);
err1:		sp->intr_level = 0;
		F_CLR(sp, S_INTERRUPTIBLE);
		return (1);
	}
	return (0);
}

/*
 * intr_end --
 *	Tear down interrupts.
 */
void
intr_end(sp)
	SCR *sp;
{
	/* If not the bottom level of interrupts, just return. */
	if (--sp->intr_level)
		return;

	/* Turn off interrupts. */
	if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &sp->intr_term))
		msgq(sp, M_SYSERR, "tcsetattr");

	/* Reset the signal state. */
	if (sigaction(SIGINT, &sp->intr_act, NULL))
		msgq(sp, M_SYSERR, "sigaction");

	/* Clear interrupt bits in this screen. */
	F_CLR(sp, S_INTERRUPTED | S_INTERRUPTIBLE);
}

/*
 * intr_def --
 *	Default interrupt handler.
 */
static void
intr_def(signo)
	int signo;
{
	SCR *sp;

	for (sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, S_INTERRUPTIBLE))
			F_SET(sp, S_INTERRUPTED);
}
