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
static char sccsid[] = "@(#)timer.c	8.13 (Berkeley) 3/23/94";
#endif /* not lint */

#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"

/*
 * There are two uses of the ITIMER_REAL timer (SIGALRM) in nvi.  The first
 * is to push the recovery information out to disk at periodic intervals.
 * The second is to display a "busy" message if an operation takes more time
 * that users are willing to wait before seeing something happen.  Each of
 * these uses has a wall clock timer structure in each SCR structure.  Since
 * the busy timer has a much faster timeout than the recovery timer, most of
 * the code ignores the recovery timer unless it's the only thing running.
 *
 * XXX
 * It would be nice to reimplement this with two timers, a la POSIX 1003.1,
 * but not many systems offer them yet.
 */

/* 
 * busy_on --
 *	Set a busy message timer.
 */
int
busy_on(sp, msg)
	SCR *sp;
	char const *msg;
{
	struct itimerval value;
	struct timeval tod;

	/*
	 * Give the oldest busy message precedence, since it's
	 * the longer running operation.
	 */
	if (sp->busy_msg != NULL)
		return (1);

	/* Get the current time of day, and create a target time. */
	if (gettimeofday(&tod, NULL))
		return (1);
#define	USER_PATIENCE_USECS	(8 * 100000L)
	sp->busy_tod.tv_sec = tod.tv_sec;
	sp->busy_tod.tv_usec = tod.tv_usec + USER_PATIENCE_USECS;

	/* We depend on this being an atomic instruction. */
	sp->busy_msg = msg;

	/*
	 * Busy messages turn around fast.  Reset the timer regardless
	 * of its current state.
	 */
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = USER_PATIENCE_USECS;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, NULL))
		msgq(sp, M_SYSERR, "timer: setitimer");
	return (0);
}

/*
 * busy_off --
 *	Turn off a busy message timer.
 */
void
busy_off(sp)
	SCR *sp;
{
	/* We depend on this being an atomic instruction. */
	sp->busy_msg = NULL;
}

/*
 * rcv_on --
 *	Turn on recovery timer.
 */
int
rcv_on(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct itimerval value;
	struct timeval tod;

	/* Get the current time of day. */
	if (gettimeofday(&tod, NULL))
		return (1);

	/* Create target time of day. */
	ep->rcv_tod.tv_sec = tod.tv_sec + RCV_PERIOD;
	ep->rcv_tod.tv_usec = 0;

	/*
	 * If there's a busy message happening, we're done, the
	 * interrupt handler will start our timer as necessary.
	 */
	if (sp->busy_msg != NULL)
		return (0);

	value.it_value.tv_sec = RCV_PERIOD;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, NULL)) {
		msgq(sp, M_SYSERR, "timer: setitimer");
		return (1);
	}
	return (0);
}

/*
 * h_alrm --
 *	Handle SIGALRM.
 */
void
h_alrm(signo)
	int signo;
{
	struct itimerval value;
	struct timeval ntod, tod;
	SCR *sp;
	EXF *ep;

	/* XXX: Get the current time of day; if this fails, we're dead. */
	if (gettimeofday(&tod, NULL))
		return;
	
	/*
	 * Fire any timers that are past due, or any that are due
	 * in a tenth of a second or less.
	 */
	for (ntod.tv_sec = 0, sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next) {

		/* Check the busy timer if the msg pointer is set. */
		if (sp->busy_msg == NULL)
			goto skip_busy;
		if (sp->busy_tod.tv_sec > tod.tv_sec ||
		    sp->busy_tod.tv_sec == tod.tv_sec &&
		    sp->busy_tod.tv_usec > tod.tv_usec &&
		    sp->busy_tod.tv_usec - tod.tv_usec > 100000L) {
			if (ntod.tv_sec == 0 ||
			    ntod.tv_sec > sp->busy_tod.tv_sec ||
			    ntod.tv_sec == sp->busy_tod.tv_sec &&
			    ntod.tv_usec > sp->busy_tod.tv_usec)
				ntod = sp->busy_tod;
		} else {
			(void)sp->s_busy(sp, sp->busy_msg);
			sp->busy_msg = NULL;
		}

		/*
		 * Check the recovery timer if there's an EXF structure
		 * and the recovery bit is set.
		 */
skip_busy:	if ((ep = sp->ep) == NULL || !F_ISSET(sp->ep, F_RCV_ON))
			continue;
		if (ep->rcv_tod.tv_sec > tod.tv_sec ||
		    ep->rcv_tod.tv_sec == tod.tv_sec &&
		    ep->rcv_tod.tv_usec > tod.tv_usec &&
		    ep->rcv_tod.tv_usec - tod.tv_usec > 100000L) {
			if (ntod.tv_sec == 0 ||
			    ntod.tv_sec > ep->rcv_tod.tv_sec ||
			    ntod.tv_sec == ep->rcv_tod.tv_sec &&
			    ntod.tv_usec > ep->rcv_tod.tv_usec)
				ntod = ep->rcv_tod;
		} else {
			F_SET(sp->gp, G_SIGALRM);
			ep->rcv_tod = tod;
			ep->rcv_tod.tv_sec += RCV_PERIOD;

			if (ntod.tv_sec == 0 ||
			    ntod.tv_sec > ep->rcv_tod.tv_sec ||
			    ntod.tv_sec == ep->rcv_tod.tv_sec &&
			    ntod.tv_usec > ep->rcv_tod.tv_usec)
				ntod = ep->rcv_tod;
		}
	}

	if (ntod.tv_sec == 0)
		return;

	/* XXX: Set the timer; if this fails, we're dead. */
	value.it_value.tv_sec = ntod.tv_sec - tod.tv_sec;
	value.it_value.tv_usec = ntod.tv_usec - tod.tv_usec;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &value, NULL);
}
