/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 * $Id: kern_timeout.c,v 1.50 1998/01/10 14:55:14 phk Exp $
 */

/* Portions of this software are covered by the following: */
/******************************************************************************
 *                                                                            *
 * Copyright (c) David L. Mills 1993, 1994                                    *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appears in all copies and that both the    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name University of Delaware not be used in     *
 * advertising or publicity pertaining to distribution of the software        *
 * without specific, written prior permission.  The University of Delaware    *
 * makes no representations about the suitability this software for any       *
 * purpose.  It is provided "as is" without express or implied warranty.      *
 *                                                                            *
 *****************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/timex.h>
#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#define CLOCK_HAIR		/* XXX */
#include <machine/clock.h>
#include <machine/limits.h>

/* Exported to machdep.c. */
struct callout *callout;
struct callout_list callfree;
int callwheelsize, callwheelbits, callwheelmask;
struct callout_tailq *callwheel;

int softticks;			/* Like ticks, but for softclock(). */
static struct callout *nextsoftcheck;	/* Next callout to be checked. */

/*
 * The callout mechanism is based on the work of Adam M. Costello and 
 * George Varghese, published in a technical report entitled "Redesigning
 * the BSD Callout and Timer Facilities" and modified slightly for inclusion
 * in FreeBSD by Justin T. Gibbs.  The original work on the data structures
 * used in this implementation was published by G.Varghese and A. Lauck in
 * the paper "Hashed and Hierarchical Timing Wheels: Data Structures for
 * the Efficient Implementation of a Timer Facility" in the Proceedings of
 * the 11th ACM Annual Symposium on Operating Systems Principles,
 * Austin, Texas Nov 1987.
 */

/*
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 */

#ifndef MAX_SOFTCLOCK_STEPS
#define MAX_SOFTCLOCK_STEPS 100 /* Maximum allowed value of steps. */
#endif /* MAX_SOFTCLOCK_STEPS */

/*ARGSUSED*/
void
softclock()
{
	register struct callout *c;
	register struct callout_tailq *bucket;
	register int s;
	register int curticks;
	register int steps;	/*
				 * Number of steps taken since
				 * we last allowed interrupts.
				 */


	(void)splsoftclock();

	steps = 0;
	s = splhigh();
	while (softticks != ticks) {
		softticks++;
		/*
		 * softticks may be modified by hard clock, so cache
		 * it while we work on a given bucket.
		 */
		curticks = softticks;
		bucket = &callwheel[curticks & callwheelmask];
		c = TAILQ_FIRST(bucket);
		while (c) {
			if (c->c_time != curticks) {
				c = TAILQ_NEXT(c, c_links.tqe);
				++steps;
				if (steps >= MAX_SOFTCLOCK_STEPS) {
					nextsoftcheck = c;
					/* Give interrupts a chance. */
					splx(s);
					s = splhigh();
					c = nextsoftcheck;
					steps = 0;
				}
			} else {
				void (*c_func)(void *);
				void *c_arg;

				nextsoftcheck = TAILQ_NEXT(c, c_links.tqe);
				TAILQ_REMOVE(bucket, c, c_links.tqe);
				c_func = c->c_func;
				c_arg = c->c_arg;
				c->c_func = NULL;
				SLIST_INSERT_HEAD(&callfree, c, c_links.sle);
				splx(s);
				c_func(c_arg);
				s = splhigh();
				steps = 0;
				c = nextsoftcheck;
			}
		}
	}
	nextsoftcheck = NULL;
	splx(s);
}

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 * callout_handle_init --
 *	Initialize a handle so that using it with untimeout is benign.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that although an 
 *	identification value is returned from timeout, the original
 *	arguments to timeout as well as the identifier are used to
 *	identify entries for untimeout.
 */
struct callout_handle
timeout(ftn, arg, to_ticks)
	timeout_t ftn;
	void *arg;
	register int to_ticks;
{
	int s;
	struct callout *new;
	struct callout_handle handle;

	if (to_ticks <= 0)
		to_ticks = 1;

	/* Lock out the clock. */
	s = splhigh();

	/* Fill in the next free callout structure. */
	new = SLIST_FIRST(&callfree);
	if (new == NULL)
		/* XXX Attempt to malloc first */
		panic("timeout table full");

	SLIST_REMOVE_HEAD(&callfree, c_links.sle);
	new->c_arg = arg;
	new->c_func = ftn;
	new->c_time = ticks + to_ticks;
	TAILQ_INSERT_TAIL(&callwheel[new->c_time & callwheelmask],
			  new, c_links.tqe);

	splx(s);
	handle.callout = new;
	return (handle);
}

void
untimeout(ftn, arg, handle)
	timeout_t ftn;
	void *arg;
	struct callout_handle handle;
{
	register int s;

	/*
	 * Check for a handle that was initialized
	 * by callout_handle_init, but never used
	 * for a real timeout.
	 */
	if (handle.callout == NULL)
		return;

	s = splhigh();
	if ((handle.callout->c_func == ftn)
	 && (handle.callout->c_arg == arg)) {
		if (nextsoftcheck == handle.callout) {
			nextsoftcheck = TAILQ_NEXT(handle.callout, c_links.tqe);
		}
		TAILQ_REMOVE(&callwheel[handle.callout->c_time & callwheelmask],
			     handle.callout, c_links.tqe);
		handle.callout->c_func = NULL;
		SLIST_INSERT_HEAD(&callfree, handle.callout, c_links.sle);
	}
	splx(s);
}

void
callout_handle_init(struct callout_handle *handle)
{
	handle->callout = NULL;
}

#ifdef APM_FIXUP_CALLTODO
/* 
 * Adjust the kernel calltodo timeout list.  This routine is used after 
 * an APM resume to recalculate the calltodo timer list values with the 
 * number of hz's we have been sleeping.  The next hardclock() will detect 
 * that there are fired timers and run softclock() to execute them.
 *
 * Please note, I have not done an exhaustive analysis of what code this
 * might break.  I am motivated to have my select()'s and alarm()'s that
 * have expired during suspend firing upon resume so that the applications
 * which set the timer can do the maintanence the timer was for as close
 * as possible to the originally intended time.  Testing this code for a 
 * week showed that resuming from a suspend resulted in 22 to 25 timers 
 * firing, which seemed independant on whether the suspend was 2 hours or
 * 2 days.  Your milage may vary.   - Ken Key <key@cs.utk.edu>
 */
void
adjust_timeout_calltodo(time_change)
    struct timeval *time_change;
{
	register struct callout *p;
	unsigned long delta_ticks;
	int s;

	/* 
	 * How many ticks were we asleep?
	 * (stolen from hzto()).
	 */

	/* Don't do anything */
	if (time_change->tv_sec < 0)
		return;
	else if (time_change->tv_sec <= LONG_MAX / 1000000)
		delta_ticks = (time_change->tv_sec * 1000000 +
			       time_change->tv_usec + (tick - 1)) / tick + 1;
	else if (time_change->tv_sec <= LONG_MAX / hz)
		delta_ticks = time_change->tv_sec * hz +
			      (time_change->tv_usec + (tick - 1)) / tick + 1;
	else
		delta_ticks = LONG_MAX;

	if (delta_ticks > INT_MAX)
		delta_ticks = INT_MAX;

	/* 
	 * Now rip through the timer calltodo list looking for timers
	 * to expire.
	 */

	/* don't collide with softclock() */
	s = splhigh(); 
	for (p = calltodo.c_next; p != NULL; p = p->c_next) {
		p->c_time -= delta_ticks;

		/* Break if the timer had more time on it than delta_ticks */
		if (p->c_time > 0)
			break;

		/* take back the ticks the timer didn't use (p->c_time <= 0) */
		delta_ticks = -p->c_time;
	}
	splx(s);

	return;
}
#endif /* APM_FIXUP_CALLTODO */
