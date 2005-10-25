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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_device_polling.h"
#include "opt_hwpmc_hooks.h"
#include "opt_ntp.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
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
#include <sys/limits.h>
#include <sys/timetc.h>

#include <machine/cpu.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#ifdef DEVICE_POLLING
extern void hardclock_device_poll(void);
#endif /* DEVICE_POLLING */

static void initclocks(void *dummy);
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL)

/* Some of these don't belong here, but it's easiest to concentrate them. */
long cp_time[CPUSTATES];

static int
sysctl_kern_cp_time(SYSCTL_HANDLER_ARGS)
{
	int error;
#ifdef SCTL_MASK32
	int i;
	unsigned int cp_time32[CPUSTATES];
	
	if (req->flags & SCTL_MASK32) {
		if (!req->oldptr)
			return SYSCTL_OUT(req, 0, sizeof(cp_time32));
		for (i = 0; i < CPUSTATES; i++)
			cp_time32[i] = (unsigned int)cp_time[i];
		error = SYSCTL_OUT(req, cp_time32, sizeof(cp_time32));
	} else
#endif
	{
		if (!req->oldptr)
			return SYSCTL_OUT(req, 0, sizeof(cp_time));
		error = SYSCTL_OUT(req, cp_time, sizeof(cp_time));
	}
	return error;
}

SYSCTL_PROC(_kern, OID_AUTO, cp_time, CTLTYPE_LONG|CTLFLAG_RD, 
    0,0, sysctl_kern_cp_time, "LU", "CPU time statistics");

#ifdef SW_WATCHDOG
#include <sys/watchdog.h>

static int watchdog_ticks;
static int watchdog_enabled;
static void watchdog_fire(void);
static void watchdog_config(void *, u_int, int *);
#endif /* SW_WATCHDOG */

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

	/*
	 * Compute profhz/stathz, and fix profhz if needed.
	 */
	i = stathz ? stathz : hz;
	if (profhz == 0)
		profhz = i;
	psratio = profhz / i;
#ifdef SW_WATCHDOG
	EVENTHANDLER_REGISTER(watchdog_list, watchdog_config, NULL, 0);
#endif
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
	if (p->p_flag & P_SA) {
		/* XXXKSE What to do? */
	} else {
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0) {
			p->p_sflag |= PS_ALRMPEND;
			td->td_flags |= TDF_ASTPENDING;
		}
		if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0) {
			p->p_sflag |= PS_PROFPEND;
			td->td_flags |= TDF_ASTPENDING;
		}
	}
	mtx_unlock_spin_flags(&sched_lock, MTX_QUIET);

#ifdef	HWPMC_HOOKS
	if (PMC_CPU_HAS_SAMPLES(PCPU_GET(cpuid)))
		PMC_CALL_HOOK_UNLOCKED(curthread, PMC_FN_DO_SAMPLES, NULL);
#endif
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

#ifdef SW_WATCHDOG
	if (watchdog_enabled > 0 && --watchdog_ticks <= 0)
		watchdog_fire();
#endif /* SW_WATCHDOG */
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
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (p->p_flag & P_STOPPROF)
		return;
	if ((p->p_flag & P_PROFIL) == 0) {
		mtx_lock_spin(&sched_lock);
		p->p_flag |= P_PROFIL;
		if (++profprocs == 1)
			cpu_startprofclock();
		mtx_unlock_spin(&sched_lock);
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(p)
	register struct proc *p;
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (p->p_flag & P_PROFIL) {
		if (p->p_profthreads != 0) {
			p->p_flag |= P_STOPPROF;
			while (p->p_profthreads != 0)
				msleep(&p->p_profthreads, &p->p_mtx, PPAUSE,
				    "stopprof", 0);
			p->p_flag &= ~P_STOPPROF;
		}
		if ((p->p_flag & P_PROFIL) == 0)
			return;
		mtx_lock_spin(&sched_lock);
		p->p_flag &= ~P_PROFIL;
		if (--profprocs == 0)
			cpu_stopprofclock();
		mtx_unlock_spin(&sched_lock);
	}
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.  Most of the statistics are only
 * used by user-level statistics programs.  The main exceptions are
 * ke->ke_uticks, p->p_rux.rux_sticks, p->p_rux.rux_iticks, and p->p_estcpu.
 * This should be called by all active processors.
 */
void
statclock(frame)
	register struct clockframe *frame;
{
	struct rusage *ru;
	struct vmspace *vm;
	struct thread *td;
	struct proc *p;
	long rss;

	td = curthread;
	p = td->td_proc;

	mtx_lock_spin_flags(&sched_lock, MTX_QUIET);
	if (CLKF_USERMODE(frame)) {
		/*
		 * Charge the time as appropriate.
		 */
		if (p->p_flag & P_SA)
			thread_statclock(1);
		p->p_rux.rux_uticks++;
		if (p->p_nice > NZERO)
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
		if ((td->td_pflags & TDP_ITHREAD) ||
		    td->td_intr_nesting_level >= 2) {
			p->p_rux.rux_iticks++;
			cp_time[CP_INTR]++;
		} else {
			if (p->p_flag & P_SA)
				thread_statclock(0);
			td->td_sticks++;
			p->p_rux.rux_sticks++;
			if (td != PCPU_GET(idlethread))
				cp_time[CP_SYS]++;
			else
				cp_time[CP_IDLE]++;
		}
	}
	CTR4(KTR_SCHED, "statclock: %p(%s) prio %d stathz %d",
	    td, td->td_proc->p_comm, td->td_priority, (stathz)?stathz:hz);

	sched_clock(td);

	/* Update resource usage integrals and maximums. */
	MPASS(p->p_stats != NULL);
	MPASS(p->p_vmspace != NULL);
	vm = p->p_vmspace;
	ru = &p->p_stats->p_ru;
	ru->ru_ixrss += pgtok(vm->vm_tsize);
	ru->ru_idrss += pgtok(vm->vm_dsize);
	ru->ru_isrss += pgtok(vm->vm_ssize);
	rss = pgtok(vmspace_resident_count(vm));
	if (ru->ru_maxrss < rss)
		ru->ru_maxrss = rss;
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

	td = curthread;
	if (CLKF_USERMODE(frame)) {
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled, record the tick.
		 * if there is no related user location yet, don't
		 * bother trying to count it.
		 */
		if (td->td_proc->p_flag & P_PROFIL)
			addupc_intr(td, CLKF_PC(frame), 1);
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

#ifdef SW_WATCHDOG

static void
watchdog_config(void *unused __unused, u_int cmd, int *err)
{
	u_int u;

	u = cmd & WD_INTERVAL;
	if (cmd && u >= WD_TO_1SEC) {
		u = cmd & WD_INTERVAL;
		watchdog_ticks = (1 << (u - WD_TO_1SEC)) * hz;
		watchdog_enabled = 1;
		*err = 0;
	} else {
		watchdog_enabled = 0;
	}
}

/*
 * Handle a watchdog timeout by dumping interrupt information and
 * then either dropping to DDB or panicing.
 */
static void
watchdog_fire(void)
{
	int nintr;
	u_int64_t inttotal;
	u_long *curintr;
	char *curname;

	curintr = intrcnt;
	curname = intrnames;
	inttotal = 0;
	nintr = eintrcnt - intrcnt;
	
	printf("interrupt                   total\n");
	while (--nintr >= 0) {
		if (*curintr)
			printf("%-12s %20lu\n", curname, *curintr);
		curname += strlen(curname) + 1;
		inttotal += *curintr++;
	}
	printf("Total        %20ju\n", (uintmax_t)inttotal);

#ifdef KDB
	kdb_backtrace();
	kdb_enter("watchdog timeout");
#else
	panic("watchdog timeout");
#endif /* KDB */
}

#endif /* SW_WATCHDOG */
