/*-
 * Copyright (c) 1997, 1998 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * $FreeBSD$
 */

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/timex.h>
#include <sys/timepps.h>
#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/limits.h>
#include <machine/smp.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

#ifdef DEVICE_POLLING
extern void init_device_poll(void);
extern void hardclock_device_poll(void);
#endif /* DEVICE_POLLING */

/*
 * Number of timecounters used to implement stable storage
 */
#ifndef NTIMECOUNTER
#define NTIMECOUNTER	5
#endif

static MALLOC_DEFINE(M_TIMECOUNTER, "timecounter", 
	"Timecounter stable storage");

static void initclocks __P((void *dummy));
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL)

static void tco_forward __P((int force));
static void tco_setscales __P((struct timecounter *tc));
static __inline unsigned tco_delta __P((struct timecounter *tc));

/* Some of these don't belong here, but it's easiest to concentrate them. */
long cp_time[CPUSTATES];

SYSCTL_OPAQUE(_kern, OID_AUTO, cp_time, CTLFLAG_RD, &cp_time, sizeof(cp_time),
    "LU", "CPU time statistics");

long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

time_t time_second;

struct	timeval boottime;
SYSCTL_STRUCT(_kern, KERN_BOOTTIME, boottime, CTLFLAG_RD,
    &boottime, timeval, "System boottime");

/*
 * Which update policy to use.
 *   0 - every tick, bad hardware may fail with "calcru negative..."
 *   1 - more resistent to the above hardware, but less efficient.
 */
static int tco_method;

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * timeservices.
 */

static unsigned 
dummy_get_timecount(struct timecounter *tc)
{
	static unsigned now;
	return (++now);
}

static struct timecounter dummy_timecounter = {
	dummy_get_timecount,
	0,
	~0u,
	1000000,
	"dummy"
};

struct timecounter *timecounter = &dummy_timecounter;

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.
 *
 * The main timer, running hz times per second, is used to trigger interval
 * timers, timeouts and rescheduling as needed.
 *
 * The second timer handles kernel and user profiling,
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
 *
 * Time-of-day is maintained using a "timecounter", which may or may
 * not be related to the hardware generating the above mentioned
 * interrupts.
 */

int	stathz;
int	profhz;
static int profprocs;
int	ticks;
static int psdiv, pscnt;		/* prof => stat divider */
int	psratio;			/* ratio: prof / stat */

/*
 * Initialize clock frequencies and start both clocks running.
 */
/* ARGSUSED*/
static void
initclocks(dummy)
	void *dummy;
{
	register int i;

	/*
	 * Set divisors to 1 (normal case) and let the machine-specific
	 * code do its bit.
	 */
	psdiv = pscnt = 1;
	cpu_initclocks();

#ifdef DEVICE_POLLING
	init_device_poll();
#endif

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
	register struct proc *p;

	p = curproc;
	if (p) {
		register struct pstats *pstats;

		/*
		 * Run current process's virtual and profile time, as needed.
		 */
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(p, SIGVTALRM);
		if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			psignal(p, SIGPROF);
	}

#if defined(SMP) && defined(BETTER_CLOCK)
	forward_hardclock(pscnt);
#endif

	/*
	 * If no separate statistics clock is available, run it from here.
	 */
	if (stathz == 0)
		statclock(frame);

	tco_forward(0);
	ticks++;

#ifdef DEVICE_POLLING
	hardclock_device_poll();	/* this is very short and quick */
#endif /* DEVICE_POLLING */

	/*
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	if (TAILQ_FIRST(&callwheel[ticks & callwheelmask]) != NULL) {
		if (CLKF_BASEPRI(frame)) {
			/*
			 * Save the overhead of a software interrupt;
			 * it will happen as soon as we return, so do it now.
			 */
			(void)splsoftclock();
			softclock();
		} else
			setsoftclock();
	} else if (softticks + 1 == ticks)
		++softticks;
}

/*
 * Compute number of ticks in the specified amount of time.
 */
int
tvtohz(tv)
	struct timeval *tv;
{
	register unsigned long ticks;
	register long sec, usec;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * difference fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time difference fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time difference down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	sec = tv->tv_sec;
	usec = tv->tv_usec;
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}
	if (sec < 0) {
#ifdef DIAGNOSTIC
		if (usec > 0) {
			sec++;
			usec -= 1000000;
		}
		printf("tvotohz: negative time difference %ld sec %ld usec\n",
		       sec, usec);
#endif
		ticks = 1;
	} else if (sec <= LONG_MAX / 1000000)
		ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
			/ tick + 1;
	else if (sec <= LONG_MAX / hz)
		ticks = sec * hz
			+ ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		ticks = LONG_MAX;
	if (ticks > INT_MAX)
		ticks = INT_MAX;
	return ((int)ticks);
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

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.  Most of the statistics are only
 * used by user-level statistics programs.  The main exceptions are
 * p->p_uticks, p->p_sticks, p->p_iticks, and p->p_estcpu.
 */
void
statclock(frame)
	register struct clockframe *frame;
{
#ifdef GPROF
	register struct gmonparam *g;
	int i;
#endif
	register struct proc *p;
	struct pstats *pstats;
	long rss;
	struct rusage *ru;
	struct vmspace *vm;

	if (curproc != NULL && CLKF_USERMODE(frame)) {
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled, record the tick.
		 */
		p = curproc;
		if (p->p_flag & P_PROFIL)
			addupc_intr(p, CLKF_PC(frame), 1);
#if defined(SMP) && defined(BETTER_CLOCK)
		if (stathz != 0)
			forward_statclock(pscnt);
#endif
		if (--pscnt > 0)
			return;
		/*
		 * Charge the time as appropriate.
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
#if defined(SMP) && defined(BETTER_CLOCK)
		if (stathz != 0)
			forward_statclock(pscnt);
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

	if (p != NULL) {
		schedclock(p);

		/* Update resource usage integrals and maximums. */
		if ((pstats = p->p_stats) != NULL &&
		    (ru = &pstats->p_ru) != NULL &&
		    (vm = p->p_vmspace) != NULL) {
			ru->ru_ixrss += pgtok(vm->vm_tsize);
			ru->ru_idrss += pgtok(vm->vm_dsize);
			ru->ru_isrss += pgtok(vm->vm_ssize);
			rss = pgtok(vmspace_resident_count(vm));
			if (ru->ru_maxrss < rss)
				ru->ru_maxrss = rss;
		}
	}
}

/*
 * Return information about system clocks.
 */
static int
sysctl_kern_clockrate(SYSCTL_HANDLER_ARGS)
{
	struct clockinfo clkinfo;
	/*
	 * Construct clockinfo structure.
	 */
	clkinfo.hz = hz;
	clkinfo.tick = tick;
	clkinfo.tickadj = tickadj;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;
	return (sysctl_handle_opaque(oidp, &clkinfo, sizeof clkinfo, req));
}

SYSCTL_PROC(_kern, KERN_CLOCKRATE, clockrate, CTLTYPE_STRUCT|CTLFLAG_RD,
	0, 0, sysctl_kern_clockrate, "S,clockinfo","");

static __inline unsigned
tco_delta(struct timecounter *tc)
{

	return ((tc->tc_get_timecount(tc) - tc->tc_offset_count) & 
	    tc->tc_counter_mask);
}

/*
 * We have eight functions for looking at the clock, four for
 * microseconds and four for nanoseconds.  For each there is fast
 * but less precise version "get{nano|micro}[up]time" which will
 * return a time which is up to 1/HZ previous to the call, whereas
 * the raw version "{nano|micro}[up]time" will return a timestamp
 * which is as precise as possible.  The "up" variants return the
 * time relative to system boot, these are well suited for time
 * interval measurements.
 */

void
getmicrotime(struct timeval *tvp)
{
	struct timecounter *tc;

	if (!tco_method) {
		tc = timecounter;
		*tvp = tc->tc_microtime;
	} else {
		microtime(tvp);
	}
}

void
getnanotime(struct timespec *tsp)
{
	struct timecounter *tc;

	if (!tco_method) {
		tc = timecounter;
		*tsp = tc->tc_nanotime;
	} else {
		nanotime(tsp);
	}
}

void
microtime(struct timeval *tv)
{
	struct timecounter *tc;

	tc = timecounter;
	tv->tv_sec = tc->tc_offset_sec;
	tv->tv_usec = tc->tc_offset_micro;
	tv->tv_usec += ((u_int64_t)tco_delta(tc) * tc->tc_scale_micro) >> 32;
	tv->tv_usec += boottime.tv_usec;
	tv->tv_sec += boottime.tv_sec;
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void
nanotime(struct timespec *ts)
{
	unsigned count;
	u_int64_t delta;
	struct timecounter *tc;

	tc = timecounter;
	ts->tv_sec = tc->tc_offset_sec;
	count = tco_delta(tc);
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)count * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)count * tc->tc_scale_nano_i);
	delta += boottime.tv_usec * 1000;
	ts->tv_sec += boottime.tv_sec;
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts->tv_sec++;
	}
	ts->tv_nsec = delta;
}

void
getmicrouptime(struct timeval *tvp)
{
	struct timecounter *tc;

	if (!tco_method) {
		tc = timecounter;
		tvp->tv_sec = tc->tc_offset_sec;
		tvp->tv_usec = tc->tc_offset_micro;
	} else {
		microuptime(tvp);
	}
}

void
getnanouptime(struct timespec *tsp)
{
	struct timecounter *tc;

	if (!tco_method) {
		tc = timecounter;
		tsp->tv_sec = tc->tc_offset_sec;
		tsp->tv_nsec = tc->tc_offset_nano >> 32;
	} else {
		nanouptime(tsp);
	}
}

void
microuptime(struct timeval *tv)
{
	struct timecounter *tc;

	tc = timecounter;
	tv->tv_sec = tc->tc_offset_sec;
	tv->tv_usec = tc->tc_offset_micro;
	tv->tv_usec += ((u_int64_t)tco_delta(tc) * tc->tc_scale_micro) >> 32;
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void
nanouptime(struct timespec *ts)
{
	unsigned count;
	u_int64_t delta;
	struct timecounter *tc;

	tc = timecounter;
	ts->tv_sec = tc->tc_offset_sec;
	count = tco_delta(tc);
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)count * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)count * tc->tc_scale_nano_i);
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts->tv_sec++;
	}
	ts->tv_nsec = delta;
}

static void
tco_setscales(struct timecounter *tc)
{
	u_int64_t scale;

	scale = 1000000000LL << 32;
	scale += tc->tc_adjustment;
	scale /= tc->tc_tweak->tc_frequency;
	tc->tc_scale_micro = scale / 1000;
	tc->tc_scale_nano_f = scale & 0xffffffff;
	tc->tc_scale_nano_i = scale >> 32;
}

void
update_timecounter(struct timecounter *tc)
{
	tco_setscales(tc);
}

void
init_timecounter(struct timecounter *tc)
{
	struct timespec ts1;
	struct timecounter *t1, *t2, *t3;
	unsigned u;
	int i;

	u = tc->tc_frequency / tc->tc_counter_mask;
	if (u > hz) {
		printf("Timecounter \"%s\" frequency %lu Hz"
		       " -- Insufficient hz, needs at least %u\n",
		       tc->tc_name, (u_long) tc->tc_frequency, u);
		return;
	}

	tc->tc_adjustment = 0;
	tc->tc_tweak = tc;
	tco_setscales(tc);
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	if (timecounter == &dummy_timecounter)
		tc->tc_avail = tc;
	else {
		tc->tc_avail = timecounter->tc_tweak->tc_avail;
		timecounter->tc_tweak->tc_avail = tc;
	}
	MALLOC(t1, struct timecounter *, sizeof *t1, M_TIMECOUNTER, M_WAITOK);
	tc->tc_other = t1;
	*t1 = *tc;
	t2 = t1;
	for (i = 1; i < NTIMECOUNTER; i++) {
		MALLOC(t3, struct timecounter *, sizeof *t3,
		    M_TIMECOUNTER, M_WAITOK);
		*t3 = *tc;
		t3->tc_other = t2;
		t2 = t3;
	}
	t1->tc_other = t3;
	tc = t1;

	printf("Timecounter \"%s\"  frequency %lu Hz\n", 
	    tc->tc_name, (u_long)tc->tc_frequency);

	/* XXX: For now always start using the counter. */
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	nanouptime(&ts1);
	tc->tc_offset_nano = (u_int64_t)ts1.tv_nsec << 32;
	tc->tc_offset_micro = ts1.tv_nsec / 1000;
	tc->tc_offset_sec = ts1.tv_sec;
	timecounter = tc;
}

void
set_timecounter(struct timespec *ts)
{
	struct timespec ts2;

	nanouptime(&ts2);
	boottime.tv_sec = ts->tv_sec - ts2.tv_sec;
	boottime.tv_usec = (ts->tv_nsec - ts2.tv_nsec) / 1000;
	if (boottime.tv_usec < 0) {
		boottime.tv_usec += 1000000;
		boottime.tv_sec--;
	}
	/* fiddle all the little crinkly bits around the fiords... */
	tco_forward(1);
}

static void
switch_timecounter(struct timecounter *newtc)
{
	int s;
	struct timecounter *tc;
	struct timespec ts;

	s = splclock();
	tc = timecounter;
	if (newtc->tc_tweak == tc->tc_tweak) {
		splx(s);
		return;
	}
	newtc = newtc->tc_tweak->tc_other;
	nanouptime(&ts);
	newtc->tc_offset_sec = ts.tv_sec;
	newtc->tc_offset_nano = (u_int64_t)ts.tv_nsec << 32;
	newtc->tc_offset_micro = ts.tv_nsec / 1000;
	newtc->tc_offset_count = newtc->tc_get_timecount(newtc);
	tco_setscales(newtc);
	timecounter = newtc;
	splx(s);
}

static struct timecounter *
sync_other_counter(void)
{
	struct timecounter *tc, *tcn, *tco;
	unsigned delta;

	tco = timecounter;
	tc = tco->tc_other;
	tcn = tc->tc_other;
	*tc = *tco;
	tc->tc_other = tcn;
	delta = tco_delta(tc);
	tc->tc_offset_count += delta;
	tc->tc_offset_count &= tc->tc_counter_mask;
	tc->tc_offset_nano += (u_int64_t)delta * tc->tc_scale_nano_f;
	tc->tc_offset_nano += (u_int64_t)delta * tc->tc_scale_nano_i << 32;
	return (tc);
}

static void
tco_forward(int force)
{
	struct timecounter *tc, *tco;
	struct timeval tvt;

	tco = timecounter;
	tc = sync_other_counter();
	/*
	 * We may be inducing a tiny error here, the tc_poll_pps() may
	 * process a latched count which happens after the tco_delta()
	 * in sync_other_counter(), which would extend the previous
	 * counters parameters into the domain of this new one.
	 * Since the timewindow is very small for this, the error is
	 * going to be only a few weenieseconds (as Dave Mills would
	 * say), so lets just not talk more about it, OK ?
	 */
	if (tco->tc_poll_pps) 
		tco->tc_poll_pps(tco);
	if (timedelta != 0) {
		tvt = boottime;
		tvt.tv_usec += tickdelta;
		if (tvt.tv_usec >= 1000000) {
			tvt.tv_sec++;
			tvt.tv_usec -= 1000000;
		} else if (tvt.tv_usec < 0) {
			tvt.tv_sec--;
			tvt.tv_usec += 1000000;
		}
		boottime = tvt;
		timedelta -= tickdelta;
	}

	while (tc->tc_offset_nano >= 1000000000ULL << 32) {
		tc->tc_offset_nano -= 1000000000ULL << 32;
		tc->tc_offset_sec++;
		ntp_update_second(tc);	/* XXX only needed if xntpd runs */
		tco_setscales(tc);
		force++;
	}

	if (tco_method && !force)
		return;

	tc->tc_offset_micro = (tc->tc_offset_nano / 1000) >> 32;

	/* Figure out the wall-clock time */
	tc->tc_nanotime.tv_sec = tc->tc_offset_sec + boottime.tv_sec;
	tc->tc_nanotime.tv_nsec = 
	    (tc->tc_offset_nano >> 32) + boottime.tv_usec * 1000;
	tc->tc_microtime.tv_usec = tc->tc_offset_micro + boottime.tv_usec;
	while (tc->tc_nanotime.tv_nsec >= 1000000000) {
		tc->tc_nanotime.tv_nsec -= 1000000000;
		tc->tc_microtime.tv_usec -= 1000000;
		tc->tc_nanotime.tv_sec++;
	}
	time_second = tc->tc_microtime.tv_sec = tc->tc_nanotime.tv_sec;

	timecounter = tc;
}

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW, 0, "");

SYSCTL_INT(_kern_timecounter, OID_AUTO, method, CTLFLAG_RW, &tco_method, 0,
    "This variable determines the method used for updating timecounters. "
    "If the default algorithm (0) fails with \"calcru negative...\" messages "
    "try the alternate algorithm (1) which handles bad hardware better."

);

static int
sysctl_kern_timecounter_hardware(SYSCTL_HANDLER_ARGS)
{
	char newname[32];
	struct timecounter *newtc, *tc;
	int error;

	tc = timecounter->tc_tweak;
	strncpy(newname, tc->tc_name, sizeof(newname));
	error = sysctl_handle_string(oidp, &newname[0], sizeof(newname), req);
	if (error == 0 && req->newptr != NULL &&
	    strcmp(newname, tc->tc_name) != 0) {
		for (newtc = tc->tc_avail; newtc != tc;
		    newtc = newtc->tc_avail) {
			if (strcmp(newname, newtc->tc_name) == 0) {
				/* Warm up new timecounter. */
				(void)newtc->tc_get_timecount(newtc);

				switch_timecounter(newtc);
				return (0);
			}
		}
		return (EINVAL);
	}
	return (error);
}

SYSCTL_PROC(_kern_timecounter, OID_AUTO, hardware, CTLTYPE_STRING | CTLFLAG_RW,
    0, 0, sysctl_kern_timecounter_hardware, "A", "");


int
pps_ioctl(u_long cmd, caddr_t data, struct pps_state *pps)
{
	pps_params_t *app;
	struct pps_fetch_args *fapi;
#ifdef PPS_SYNC
	struct pps_kcbind_args *kapi;
#endif

	switch (cmd) {
	case PPS_IOC_CREATE:
		return (0);
	case PPS_IOC_DESTROY:
		return (0);
	case PPS_IOC_SETPARAMS:
		app = (pps_params_t *)data;
		if (app->mode & ~pps->ppscap)
			return (EINVAL);
		pps->ppsparam = *app;         
		return (0);
	case PPS_IOC_GETPARAMS:
		app = (pps_params_t *)data;
		*app = pps->ppsparam;
		app->api_version = PPS_API_VERS_1;
		return (0);
	case PPS_IOC_GETCAP:
		*(int*)data = pps->ppscap;
		return (0);
	case PPS_IOC_FETCH:
		fapi = (struct pps_fetch_args *)data;
		if (fapi->tsformat && fapi->tsformat != PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (fapi->timeout.tv_sec || fapi->timeout.tv_nsec)
			return (EOPNOTSUPP);
		pps->ppsinfo.current_mode = pps->ppsparam.mode;         
		fapi->pps_info_buf = pps->ppsinfo;
		return (0);
	case PPS_IOC_KCBIND:
#ifdef PPS_SYNC
		kapi = (struct pps_kcbind_args *)data;
		/* XXX Only root should be able to do this */
		if (kapi->tsformat && kapi->tsformat != PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (kapi->kernel_consumer != PPS_KC_HARDPPS)
			return (EINVAL);
		if (kapi->edge & ~pps->ppscap)
			return (EINVAL);
		pps->kcmode = kapi->edge;
		return (0);
#else
		return (EOPNOTSUPP);
#endif
	default:
		return (ENOTTY);
	}
}

void
pps_init(struct pps_state *pps)
{
	pps->ppscap |= PPS_TSFMT_TSPEC;
	if (pps->ppscap & PPS_CAPTUREASSERT)
		pps->ppscap |= PPS_OFFSETASSERT;
	if (pps->ppscap & PPS_CAPTURECLEAR)
		pps->ppscap |= PPS_OFFSETCLEAR;
}

void
pps_event(struct pps_state *pps, struct timecounter *tc, unsigned count, int event)
{
	struct timespec ts, *tsp, *osp;
	u_int64_t delta;
	unsigned tcount, *pcount;
	int foff, fhard;
	pps_seq_t	*pseq;

	/* Things would be easier with arrays... */
	if (event == PPS_CAPTUREASSERT) {
		tsp = &pps->ppsinfo.assert_timestamp;
		osp = &pps->ppsparam.assert_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETASSERT;
		fhard = pps->kcmode & PPS_CAPTUREASSERT;
		pcount = &pps->ppscount[0];
		pseq = &pps->ppsinfo.assert_sequence;
	} else {
		tsp = &pps->ppsinfo.clear_timestamp;
		osp = &pps->ppsparam.clear_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETCLEAR;
		fhard = pps->kcmode & PPS_CAPTURECLEAR;
		pcount = &pps->ppscount[1];
		pseq = &pps->ppsinfo.clear_sequence;
	}

	/* The timecounter changed: bail */
	if (!pps->ppstc || 
	    pps->ppstc->tc_name != tc->tc_name || 
	    tc->tc_name != timecounter->tc_name) {
		pps->ppstc = tc;
		*pcount = count;
		return;
	}

	/* Nothing really happened */
	if (*pcount == count)
		return;

	*pcount = count;

	/* Convert the count to timespec */
	ts.tv_sec = tc->tc_offset_sec;
	tcount = count - tc->tc_offset_count;
	tcount &= tc->tc_counter_mask;
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)tcount * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)tcount * tc->tc_scale_nano_i);
	delta += boottime.tv_usec * 1000;
	ts.tv_sec += boottime.tv_sec;
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts.tv_sec++;
	}
	ts.tv_nsec = delta;

	(*pseq)++;
	*tsp = ts;

	if (foff) {
		timespecadd(tsp, osp);
		if (tsp->tv_nsec < 0) {
			tsp->tv_nsec += 1000000000;
			tsp->tv_sec -= 1;
		}
	}
#ifdef PPS_SYNC
	if (fhard) {
		/* magic, at its best... */
		tcount = count - pps->ppscount[2];
		pps->ppscount[2] = count;
		tcount &= tc->tc_counter_mask;
		delta = ((u_int64_t)tcount * tc->tc_tweak->tc_scale_nano_f);
		delta >>= 32;
		delta += ((u_int64_t)tcount * tc->tc_tweak->tc_scale_nano_i);
		hardpps(tsp, delta);
	}
#endif
}
