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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>

#include <machine/cpu.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.  The main clock, running hz times per second, is used to keep
 * track of real time.  The second timer handles kernel and user profiling,
 * and does resource use estimation.  If the second timer is programmable,
 * it is randomized to avoid aliasing between the two clocks.  For example,
 * the randomization prevents an adversary from always giving up the cpu
 * just before its quantum expires.  Otherwise, it would never accumulate
 * cpu ticks.  The mean frequency of the second timer is stathz.
 *
 * If no second timer exists, stathz will be zero; in this case we drive
 * profiling and statistics off the main clock.  This WILL NOT be accurate;
 * do not do it unless absolutely necessary.
 *
 * The statistics clock may (or may not) be run at a higher rate while
 * profiling.  This profile clock runs at profhz.  We require that profhz
 * be an integral multiple of stathz.
 *
 * If the statistics clock is running fast, it must be divided by the ratio
 * profhz/stathz for statistics.  (For profiling, every tick counts.)
 */

/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */

/*
 * Bump a timeval by a small number of usec's.
 */
#define BUMPTIME(t, usec) { \
	register volatile struct timeval *tp = (t); \
	register long us; \
 \
	tp->tv_usec = us = tp->tv_usec + (usec); \
	if (us >= 1000000) { \
		tp->tv_usec = us - 1000000; \
		tp->tv_sec++; \
	} \
}

int	stathz;
int	profhz;
int	profprocs;
int	ticks;
static int psdiv, pscnt;	/* prof => stat divider */
int	psratio;		/* ratio: prof / stat */

volatile struct	timeval time;
volatile struct	timeval mono_time;

/*
 * Initialize clock frequencies and start both clocks running.
 */
void
initclocks()
{
	register int i;

	/*
	 * Set divisors to 1 (normal case) and let the machine-specific
	 * code do its bit.
	 */
	psdiv = pscnt = 1;
	cpu_initclocks();

	/*
	 * Compute profhz/stathz, and fix profhz if needed.
	 */
	i = stathz ? stathz : hz;
	if (profhz == 0)
		profhz = i;
	psratio = profhz / i;
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(frame)
	register struct clockframe *frame;
{
	register struct callout *p1;
	register struct proc *p;
	register int delta, needsoft;
	extern int tickdelta;
	extern long timedelta;

	/*
	 * Update real-time timeout queue.
	 * At front of queue are some number of events which are ``due''.
	 * The time to these is <= 0 and if negative represents the
	 * number of ticks which have passed since it was supposed to happen.
	 * The rest of the q elements (times > 0) are events yet to happen,
	 * where the time for each is given as a delta from the previous.
	 * Decrementing just the first of these serves to decrement the time
	 * to all events.
	 */
	needsoft = 0;
	for (p1 = calltodo.c_next; p1 != NULL; p1 = p1->c_next) {
		if (--p1->c_time > 0)
			break;
		needsoft = 1;
		if (p1->c_time == 0)
			break;
	}

	p = curproc;
	if (p) {
		register struct pstats *pstats;

		/*
		 * Run current process's virtual and profile time, as needed.
		 */
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timerisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(p, SIGVTALRM);
		if (timerisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			psignal(p, SIGPROF);
	}

	/*
	 * If no separate statistics clock is available, run it from here.
	 */
	if (stathz == 0)
		statclock(frame);

	/*
	 * Increment the time-of-day.  The increment is just ``tick'' unless
	 * we are still adjusting the clock; see adjtime().
	 */
	ticks++;
	if (timedelta == 0)
		delta = tick;
	else {
		delta = tick + tickdelta;
		timedelta -= tickdelta;
	}
	BUMPTIME(&time, delta);
	BUMPTIME(&mono_time, delta);

	/*
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	if (needsoft) {
		if (CLKF_BASEPRI(frame)) {
			/*
			 * Save the overhead of a software interrupt;
			 * it will happen as soon as we return, so do it now.
			 */
			(void)splsoftclock();
			softclock();
		} else
			setsoftclock();
	}
}

/*
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 */
/*ARGSUSED*/
void
softclock()
{
	register struct callout *c;
	register void *arg;
	register void (*func) __P((void *));
	register int s;

	s = splhigh();
	while ((c = calltodo.c_next) != NULL && c->c_time <= 0) {
		func = c->c_func;
		arg = c->c_arg;
		calltodo.c_next = c->c_next;
		c->c_next = callfree;
		callfree = c;
		splx(s);
		(*func)(arg);
		(void) splhigh();
	}
	splx(s);
}

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that no identification
 *	value is returned from timeout, rather, the original arguments
 *	to timeout are used to identify entries for untimeout.
 */
void
timeout(ftn, arg, ticks)
	void (*ftn) __P((void *));
	void *arg;
	register int ticks;
{
	register struct callout *new, *p, *t;
	register int s;

	if (ticks <= 0)
		ticks = 1;

	/* Lock out the clock. */
	s = splhigh();

	/* Fill in the next free callout structure. */
	if (callfree == NULL)
		panic("timeout table full");
	new = callfree;
	callfree = new->c_next;
	new->c_arg = arg;
	new->c_func = ftn;

	/*
	 * The time for each event is stored as a difference from the time
	 * of the previous event on the queue.  Walk the queue, correcting
	 * the ticks argument for queue entries passed.  Correct the ticks
	 * value for the queue entry immediately after the insertion point
	 * as well.  Watch out for negative c_time values; these represent
	 * overdue events.
	 */
	for (p = &calltodo;
	    (t = p->c_next) != NULL && ticks > t->c_time; p = t)
		if (t->c_time > 0)
			ticks -= t->c_time;
	new->c_time = ticks;
	if (t != NULL)
		t->c_time -= ticks;

	/* Insert the new entry into the queue. */
	p->c_next = new;
	new->c_next = t;
	splx(s);
}

void
untimeout(ftn, arg)
	void (*ftn) __P((void *));
	void *arg;
{
	register struct callout *p, *t;
	register int s;

	s = splhigh();
	for (p = &calltodo; (t = p->c_next) != NULL; p = t)
		if (t->c_func == ftn && t->c_arg == arg) {
			/* Increment next entry's tick count. */
			if (t->c_next && t->c_time > 0)
				t->c_next->c_time += t->c_time;

			/* Move entry from callout queue to callfree queue. */
			p->c_next = t->c_next;
			t->c_next = callfree;
			callfree = t;
			break;
		}
	splx(s);
}

/*
 * Compute number of hz until specified time.  Used to
 * compute third argument to timeout() from an absolute time.
 */
int
hzto(tv)
	struct timeval *tv;
{
	register long ticks, sec;
	int s;

	/*
	 * If number of milliseconds will fit in 32 bit arithmetic,
	 * then compute number of milliseconds to time and scale to
	 * ticks.  Otherwise just compute number of hz in time, rounding
	 * times greater than representible to maximum value.
	 *
	 * Delta times less than 25 days can be computed ``exactly''.
	 * Maximum value for any timeout in 10ms ticks is 250 days.
	 */
	s = splhigh();
	sec = tv->tv_sec - time.tv_sec;
	if (sec <= 0x7fffffff / 1000 - 1000)
		ticks = ((tv->tv_sec - time.tv_sec) * 1000 +
			(tv->tv_usec - time.tv_usec) / 1000) / (tick / 1000);
	else if (sec <= 0x7fffffff / hz)
		ticks = sec * hz;
	else
		ticks = 0x7fffffff;
	splx(s);
	return (ticks);
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(p)
	register struct proc *p;
{
	int s;

	if ((p->p_flag & P_PROFIL) == 0) {
		p->p_flag |= P_PROFIL;
		if (++profprocs == 1 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = psratio;
			setstatclockrate(profhz);
			splx(s);
		}
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(p)
	register struct proc *p;
{
	int s;

	if (p->p_flag & P_PROFIL) {
		p->p_flag &= ~P_PROFIL;
		if (--profprocs == 0 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = 1;
			setstatclockrate(stathz);
			splx(s);
		}
	}
}

int	dk_ndrive = DK_NDRIVE;

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.
 */
void
statclock(frame)
	register struct clockframe *frame;
{
#ifdef GPROF
	register struct gmonparam *g;
#endif
	register struct proc *p;
	register int i;

	if (CLKF_USERMODE(frame)) {
		p = curproc;
		if (p->p_flag & P_PROFIL)
			addupc_intr(p, CLKF_PC(frame), 1);
		if (--pscnt > 0)
			return;
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled record the tick.
		 */
		p->p_uticks++;
		if (p->p_nice > NZERO)
			cp_time[CP_NICE]++;
		else
			cp_time[CP_USER]++;
	} else {
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = CLKF_PC(frame) - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (--pscnt > 0)
			return;
		/*
		 * Came from kernel mode, so we were:
		 * - handling an interrupt,
		 * - doing syscall or trap work on behalf of the current
		 *   user process, or
		 * - spinning in the idle loop.
		 * Whichever it is, charge the time as appropriate.
		 * Note that we charge interrupts to the current process,
		 * regardless of whether they are ``for'' that process,
		 * so that we know how much of its real time was spent
		 * in ``non-process'' (i.e., interrupt) work.
		 */
		p = curproc;
		if (CLKF_INTR(frame)) {
			if (p != NULL)
				p->p_iticks++;
			cp_time[CP_INTR]++;
		} else if (p != NULL) {
			p->p_sticks++;
			cp_time[CP_SYS]++;
		} else
			cp_time[CP_IDLE]++;
	}
	pscnt = psdiv;

	/*
	 * We maintain statistics shown by user-level statistics
	 * programs:  the amount of time in each cpu state, and
	 * the amount of time each of DK_NDRIVE ``drives'' is busy.
	 *
	 * XXX	should either run linked list of drives, or (better)
	 *	grab timestamps in the start & done code.
	 */
	for (i = 0; i < DK_NDRIVE; i++)
		if (dk_busy & (1 << i))
			dk_time[i]++;

	/*
	 * We adjust the priority of the current process.  The priority of
	 * a process gets worse as it accumulates CPU time.  The cpu usage
	 * estimator (p_estcpu) is increased here.  The formula for computing
	 * priorities (in kern_synch.c) will compute a different value each
	 * time p_estcpu increases by 4.  The cpu usage estimator ramps up
	 * quite quickly when the process is running (linearly), and decays
	 * away exponentially, at a rate which is proportionally slower when
	 * the system is busy.  The basic principal is that the system will
	 * 90% forget that the process used a lot of CPU time in 5 * loadav
	 * seconds.  This causes the system to favor processes which haven't
	 * run much recently, and to round-robin among other processes.
	 */
	if (p != NULL) {
		p->p_cpticks++;
		if (++p->p_estcpu == 0)
			p->p_estcpu--;
		if ((p->p_estcpu & 3) == 0) {
			resetpriority(p);
			if (p->p_priority >= PUSER)
				p->p_priority = p->p_usrpri;
		}
	}
}

/*
 * Return information about system clocks.
 */
sysctl_clockrate(where, sizep)
	register char *where;
	size_t *sizep;
{
	struct clockinfo clkinfo;

	/*
	 * Construct clockinfo structure.
	 */
	clkinfo.hz = hz;
	clkinfo.tick = tick;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;
	return (sysctl_rdstruct(where, sizep, NULL, &clkinfo, sizeof(clkinfo)));
}
