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
 * $FreeBSD$
 */

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/timetc.h>

#include <machine/cpu.h>
#include <machine/limits.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

#ifdef DEVICE_POLLING
extern void init_device_poll(void);
extern void hardclock_device_poll(void);
#endif /* DEVICE_POLLING */

static void initclocks(void *dummy);
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL)

/* Some of these don't belong here, but it's easiest to concentrate them. */
long cp_time[CPUSTATES];

SYSCTL_OPAQUE(_kern, OID_AUTO, cp_time, CTLFLAG_RD, &cp_time, sizeof(cp_time),
    "LU", "CPU time statistics");

long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

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
int	profprocs;
int	ticks;
int	psratio;

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
 * Each time the real-time timer fires, this function is called on all CPUs.
 * Note that hardclock() calls hardclock_process() for the boot CPU, so only
 * the other CPUs in the system need to call this function.
 */
void
hardclock_process(frame)
	register struct clockframe *frame;
{
	struct pstats *pstats;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;

	/*
	 * Run current process's virtual and profile time, as needed.
	 */
	mtx_lock_spin_flags(&sched_lock, MTX_QUIET);
	if (p->p_flag & P_KSES) {
		/* XXXKSE What to do? */
	} else {
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0) {
			p->p_sflag |= PS_ALRMPEND;
			td->td_kse->ke_flags |= KEF_ASTPENDING;
		}
		if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0) {
			p->p_sflag |= PS_PROFPEND;
			td->td_kse->ke_flags |= KEF_ASTPENDING;
		}
	}
	mtx_unlock_spin_flags(&sched_lock, MTX_QUIET);
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(frame)
	register struct clockframe *frame;
{
	int need_softclock = 0;

	CTR0(KTR_CLK, "hardclock fired");
	hardclock_process(frame);

	tc_ticktock();
	/*
	 * If no separate statistics clock is available, run it from here.
	 *
	 * XXX: this only works for UP
	 */
	if (stathz == 0) {
		profclock(frame);
		statclock(frame);
	}

#ifdef DEVICE_POLLING
	hardclock_device_poll();	/* this is very short and quick */
#endif /* DEVICE_POLLING */

	/*
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	mtx_lock_spin_flags(&callout_lock, MTX_QUIET);
	ticks++;
	if (TAILQ_FIRST(&callwheel[ticks & callwheelmask]) != NULL) {
		need_softclock = 1;
	} else if (softticks + 1 == ticks)
		++softticks;
	mtx_unlock_spin_flags(&callout_lock, MTX_QUIET);

	/*
	 * swi_sched acquires sched_lock, so we don't want to call it with
	 * callout_lock held; incorrect locking order.
	 */
	if (need_softclock)
		swi_sched(softclock_ih, 0);
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

	/*
	 * XXX; Right now sched_lock protects statclock(), but perhaps
	 * it should be protected later on by a time_lock, which would
	 * cover psdiv, etc. as well.
	 */
	mtx_lock_spin(&sched_lock);
	if ((p->p_sflag & PS_PROFIL) == 0) {
		p->p_sflag |= PS_PROFIL;
		if (++profprocs == 1)
			cpu_startprofclock();
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(p)
	register struct proc *p;
{

	mtx_lock_spin(&sched_lock);
	if (p->p_sflag & PS_PROFIL) {
		p->p_sflag &= ~PS_PROFIL;
		if (--profprocs == 0)
			cpu_stopprofclock();
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.  Most of the statistics are only
 * used by user-level statistics programs.  The main exceptions are
 * ke->ke_uticks, p->p_sticks, p->p_iticks, and p->p_estcpu.
 * This should be called by all active processors.
 */
void
statclock(frame)
	register struct clockframe *frame;
{
	struct pstats *pstats;
	struct rusage *ru;
	struct vmspace *vm;
	struct thread *td;
	struct kse *ke;
	struct proc *p;
	long rss;

	td = curthread;
	p = td->td_proc;

	mtx_lock_spin_flags(&sched_lock, MTX_QUIET);
	ke = td->td_kse;
	if (CLKF_USERMODE(frame)) {
		/*
		 * Charge the time as appropriate.
		 */
		if (p->p_flag & P_KSES)
			thread_add_ticks_intr(1, 1);
		ke->ke_uticks++;
		if (ke->ke_ksegrp->kg_nice > NZERO)
			cp_time[CP_NICE]++;
		else
			cp_time[CP_USER]++;
	} else {
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
		if ((td->td_ithd != NULL) || td->td_intr_nesting_level >= 2) {
			ke->ke_iticks++;
			cp_time[CP_INTR]++;
		} else {
			if (p->p_flag & P_KSES)
				thread_add_ticks_intr(0, 1);
			ke->ke_sticks++;
			if (p != PCPU_GET(idlethread)->td_proc)
				cp_time[CP_SYS]++;
			else
				cp_time[CP_IDLE]++;
		}
	}

	sched_clock(ke->ke_thread);

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
	mtx_unlock_spin_flags(&sched_lock, MTX_QUIET);
}

void
profclock(frame)
	register struct clockframe *frame;
{
	struct thread *td;
#ifdef GPROF
	struct gmonparam *g;
	int i;
#endif

	if (CLKF_USERMODE(frame)) {
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled, record the tick.
		 */
		td = curthread;
		if (td->td_proc->p_sflag & PS_PROFIL)
			addupc_intr(td->td_kse, CLKF_PC(frame), 1);
	}
#ifdef GPROF
	else {
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
	}
#endif
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
	bzero(&clkinfo, sizeof(clkinfo));
	clkinfo.hz = hz;
	clkinfo.tick = tick;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;
	return (sysctl_handle_opaque(oidp, &clkinfo, sizeof clkinfo, req));
}

SYSCTL_PROC(_kern, KERN_CLOCKRATE, clockrate, CTLTYPE_STRUCT|CTLFLAG_RD,
	0, 0, sysctl_kern_clockrate, "S,clockinfo",
	"Rate and period of various kernel clocks");
