/*-
 * Copyright (c) 1993
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
static char sccsid[] = "@(#)timer.c	8.7 (Berkeley) 12/2/93";
#endif /* not lint */

#include <sys/time.h>

#include "vi.h"

static void busy_handler __P((int));

/*
 * XXX
 * There are two uses of timers in nvi.  The first is to push the recovery
 * information out to disk at periodic intervals.  The second is to display
 * a "busy" message if an operation takes too long.  Rather than solve this
 * in a general fashion, we depend on the fact that only a single screen in
 * a window is active at a time, and that there are only two parts of the
 * systems that use timers.
 *
 * It would be nice to reimplement this with multiple timers, a la POSIX
 * 1003.1, but not many systems offer them yet.
 */

/*
 * busy_on --
 *	Display a message if too much time passes.
 */
void
busy_on(sp, seconds, msg)
	SCR *sp;
	int seconds;
	char const *msg;
{
	struct itimerval value;
	struct sigaction act;

	/* No busy messages in batch mode. */
	if (F_ISSET(sp, S_EXSILENT))
		return;

	/* Turn off the current timer, saving its current value. */
	value.it_interval.tv_sec = value.it_value.tv_sec = 0;
	value.it_interval.tv_usec = value.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, &sp->time_value))
		return;

	/*
	 * Decrement the original timer by the number of seconds
	 * we're going to wait.
	 */
	if (sp->time_value.it_value.tv_sec > seconds)
		sp->time_value.it_value.tv_sec -= seconds;
	else
		sp->time_value.it_value.tv_sec = 1;

	/* Reset the handler, saving its current value. */
	act.sa_handler = busy_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGALRM, &act, &sp->time_handler);

	/* Reset the timer. */
	value.it_value.tv_sec = seconds;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = value.it_value.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &value, NULL);

	sp->time_msg = msg;
	F_SET(sp, S_TIMER_SET);
}

/*
 * busy_off --
 *	Reset the timer handlers.
 */
void
busy_off(sp)
	SCR *sp;
{
	struct itimerval ovalue, value;

	/* No busy messages in batch mode. */
	if (F_ISSET(sp, S_EXSILENT))
		return;

	/* If the timer flag isn't set, it must have fired. */
	if (!F_ISSET(sp, S_TIMER_SET))
		return;

	/* Ignore it if first on one of following system calls. */
	F_CLR(sp, S_TIMER_SET);

	/* Turn off the current timer. */
	value.it_interval.tv_sec = value.it_value.tv_sec = 0;
	value.it_interval.tv_usec = value.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, &ovalue))
		return;

	/* If the timer wasn't running, we're done. */
	if (sp->time_handler.sa_handler == SIG_DFL)
		return;

	/*
	 * Increment the old timer by the number of seconds
	 * remaining in the new one.
	 */
	sp->time_value.it_value.tv_sec += ovalue.it_value.tv_sec;

	/* Reset the handler to the original handler. */
	(void)sigaction(SIGALRM, &sp->time_handler, NULL);

	/* Reset the timer. */
	(void)setitimer(ITIMER_REAL, &sp->time_value, NULL);
}

/*
 * busy_handler --
 *	Display a message when the timer goes off, and restore the
 *	timer to its original values.
 */
static void
busy_handler(signo)
	int signo;
{
	SCR *sp;

	for (sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, S_TIMER_SET)) {
			sp->s_busy(sp, sp->time_msg);
			busy_off(sp);
		}
}
