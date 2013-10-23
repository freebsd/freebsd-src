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

#include "opt_kdb.h"
#include "opt_device_polling.h"
#include "opt_hwpmc_hooks.h"
#include "opt_kdtrace.h"
#include "opt_ntp.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/limits.h>
#include <sys/timetc.h>

#include <vps/vps.h>
#include <vps/vps2.h>
#define _VPS_USER_H__ONLY_FLAGS
#include <vps/vps_user.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
PMC_SOFT_DEFINE( , , clock, hard);
PMC_SOFT_DEFINE( , , clock, stat);
PMC_SOFT_DEFINE_EX( , , clock, prof, \
    cpu_startprofclock, cpu_stopprofclock);
#endif

#ifdef DEVICE_POLLING
extern void hardclock_device_poll(void);
#endif /* DEVICE_POLLING */

static void initclocks(void *dummy);
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL);

/* Spin-lock protecting profiling statistics. */
static struct mtx time_lock;

SDT_PROVIDER_DECLARE(sched);
SDT_PROBE_DEFINE2(sched, , , tick, tick, "struct thread *", "struct proc *");

static int
sysctl_kern_cp_time(SYSCTL_HANDLER_ARGS)
{
	int error;
	long cp_time[CPUSTATES];
#ifdef SCTL_MASK32
	int i;
	unsigned int cp_time32[CPUSTATES];
#endif

	read_cpu_time(cp_time);

#ifdef VPS
	/* XXX calc real per-vps times */
	if (req->td->td_vps != vps0)
		memset(&cp_time, 0, sizeof(cp_time));
#endif

#ifdef SCTL_MASK32
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

_SYSCTL_PROC(_kern, OID_AUTO, cp_time, CTLTYPE_LONG|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0,0, sysctl_kern_cp_time, "LU", "CPU time statistics", VPS_PUBLIC);

static long empty[CPUSTATES];

static int
sysctl_kern_cp_times(SYSCTL_HANDLER_ARGS)
{
	struct pcpu *pcpu;
	int error;
	int c;
	long *cp_time;
#ifdef SCTL_MASK32
	unsigned int cp_time32[CPUSTATES];
	int i;
#endif

	if (!req->oldptr) {
#ifdef SCTL_MASK32
		if (req->flags & SCTL_MASK32)
			return SYSCTL_OUT(req, 0, sizeof(cp_time32) * (mp_maxid + 1));
		else
#endif
			return SYSCTL_OUT(req, 0, sizeof(long) * CPUSTATES * (mp_maxid + 1));
	}
	for (error = 0, c = 0; error == 0 && c <= mp_maxid; c++) {
		if (!CPU_ABSENT(c)) {
			pcpu = pcpu_find(c);
#ifdef VPS
			if (req->td->td_vps != vps0)
				cp_time = empty;
			else
#endif
				cp_time = pcpu->pc_cp_time;
		} else {
			cp_time = empty;
		}
#ifdef SCTL_MASK32
		if (req->flags & SCTL_MASK32) {
			for (i = 0; i < CPUSTATES; i++)
				cp_time32[i] = (unsigned int)cp_time[i];
			error = SYSCTL_OUT(req, cp_time32, sizeof(cp_time32));
		} else
#endif
			error = SYSCTL_OUT(req, cp_time, sizeof(long) * CPUSTATES);
	}
	return error;
}

_SYSCTL_PROC(_kern, OID_AUTO, cp_times, CTLTYPE_LONG|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0,0, sysctl_kern_cp_times, "LU", "per-CPU time statistics", VPS_PUBLIC);

#ifdef DEADLKRES
static const char *blessed[] = {
	"getblk",
	"so_snd_sx",
	"so_rcv_sx",
	NULL
};
static int slptime_threshold = 1800;
static int blktime_threshold = 900;
static int sleepfreq = 3;

static void
deadlkres(void)
{
	struct proc *p;
	struct thread *td;
	void *wchan;
	int blkticks, i, slpticks, slptype, tryl, tticks;
#ifdef VPS
        struct vps *vps, *vps_tmp, *save_vps;
#endif

	tryl = 0;
	for (;;) {

#ifdef VPS
		save_vps = curthread->td_vps;
		sx_slock(&vps_all_lock);
		LIST_FOREACH_SAFE(vps, &vps_head, vps_all, vps_tmp) {
		if (vps->vps_status == VPS_ST_DEAD)
			continue;
		vps_ref(vps, (struct ucred *)&deadlkres);
		sx_sunlock(&vps_all_lock);
		curthread->td_vps = vps;
#endif /* VPS */

		blkticks = blktime_threshold * hz;
		slpticks = slptime_threshold * hz;

		/*
		 * Avoid to sleep on the sx_lock in order to avoid a possible
		 * priority inversion problem leading to starvation.
		 * If the lock can't be held after 100 tries, panic.
		 */
		if (!sx_try_slock(&V_allproc_lock)) {
			if (tryl > 100)
		panic("%s: possible deadlock detected on allproc_lock\n",
				    __func__);
			tryl++;
			pause("allproc", sleepfreq * hz);
#ifdef VPS
			vps_deref(vps, (struct ucred *)&deadlkres);
			sx_slock(&vps_all_lock);
#endif /* VPS */
			continue;
		}
		tryl = 0;
		FOREACH_PROC_IN_SYSTEM(p) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NEW) {
				PROC_UNLOCK(p);
				continue;
			}
			FOREACH_THREAD_IN_PROC(p, td) {

				thread_lock(td);
				if (TD_ON_LOCK(td)) {

					/*
					 * The thread should be blocked on a
					 * turnstile, simply check if the
					 * turnstile channel is in good state.
					 */
					MPASS(td->td_blocked != NULL);

					tticks = ticks - td->td_blktick;
					thread_unlock(td);
					if (tticks > blkticks) {

						/*
						 * Accordingly with provided
						 * thresholds, this thread is
						 * stuck for too long on a
						 * turnstile.
						 */
						PROC_UNLOCK(p);
						sx_sunlock(&V_allproc_lock);
	panic("%s: possible deadlock detected for %p, blocked for %d ticks\n",
						    __func__, td, tticks);
					}
				} else if (TD_IS_SLEEPING(td) &&
				    TD_ON_SLEEPQ(td)) {

					/*
					 * Check if the thread is sleeping on a
					 * lock, otherwise skip the check.
					 * Drop the thread lock in order to
					 * avoid a LOR with the sleepqueue
					 * spinlock.
					 */
					wchan = td->td_wchan;
					tticks = ticks - td->td_slptick;
					thread_unlock(td);
					slptype = sleepq_type(wchan);
					if ((slptype == SLEEPQ_SX ||
					    slptype == SLEEPQ_LK) &&
					    tticks > slpticks) {

						/*
						 * Accordingly with provided
						 * thresholds, this thread is
						 * stuck for too long on a
						 * sleepqueue.
						 * However, being on a
						 * sleepqueue, we might still
						 * check for the blessed
						 * list.
						 */
						tryl = 0;
						for (i = 0; blessed[i] != NULL;
						    i++) {
							if (!strcmp(blessed[i],
							    td->td_wmesg)) {
								tryl = 1;
								break;
							}
						}
						if (tryl != 0) {
							tryl = 0;
							continue;
						}
						PROC_UNLOCK(p);
						sx_sunlock(&V_allproc_lock);
	panic("%s: possible deadlock detected for %p, blocked for %d ticks\n",
						    __func__, td, tticks);
					}
				} else
					thread_unlock(td);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&V_allproc_lock);
#ifdef VPS
		vps_deref(vps, (struct ucred *)&deadlkres);
		sx_slock(&vps_all_lock);
		}
		sx_sunlock(&vps_all_lock);
		curthread->td_vps = save_vps;
#endif

		/* Sleep for sleepfreq seconds. */
		pause("-", sleepfreq * hz);
	}
}

static struct kthread_desc deadlkres_kd = {
	"deadlkres",
	deadlkres,
	(struct thread **)NULL
};

SYSINIT(deadlkres, SI_SUB_CLOCKS, SI_ORDER_ANY, kthread_start, &deadlkres_kd);

static SYSCTL_NODE(_debug, OID_AUTO, deadlkres, CTLFLAG_RW, 0,
    "Deadlock resolver");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, slptime_threshold, CTLFLAG_RW,
    &slptime_threshold, 0,
    "Number of seconds within is valid to sleep on a sleepqueue");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, blktime_threshold, CTLFLAG_RW,
    &blktime_threshold, 0,
    "Number of seconds within is valid to block on a turnstile");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, sleepfreq, CTLFLAG_RW, &sleepfreq, 0,
    "Number of seconds between any deadlock resolver thread run");
#endif	/* DEADLKRES */

void
read_cpu_time(long *cp_time)
{
	struct pcpu *pc;
	int i, j;

	/* Sum up global cp_time[]. */
	bzero(cp_time, sizeof(long) * CPUSTATES);
	CPU_FOREACH(i) {
		pc = pcpu_find(i);
		for (j = 0; j < CPUSTATES; j++)
			cp_time[j] += pc->pc_cp_time[j];
	}
}

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
volatile int	ticks;
int	psratio;

static DPCPU_DEFINE(int, pcputicks);	/* Per-CPU version of ticks. */
static int global_hardclock_run = 0;

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
	mtx_init(&time_lock, "time lock", NULL, MTX_DEF);
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
 * Note that hardclock() calls hardclock_cpu() for the boot CPU, so only
 * the other CPUs in the system need to call this function.
 */
void
hardclock_cpu(int usermode)
{
	struct pstats *pstats;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int flags;

	/*
	 * Run current process's virtual and profile time, as needed.
	 */
	pstats = p->p_stats;
	flags = 0;
	if (usermode &&
	    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value)) {
		PROC_SLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			flags |= TDF_ALRMPEND | TDF_ASTPENDING;
		PROC_SUNLOCK(p);
	}
	if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value)) {
		PROC_SLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			flags |= TDF_PROFPEND | TDF_ASTPENDING;
		PROC_SUNLOCK(p);
	}
	thread_lock(td);
	sched_tick(1);
	td->td_flags |= flags;
	thread_unlock(td);

#ifdef HWPMC_HOOKS
	if (PMC_CPU_HAS_SAMPLES(PCPU_GET(cpuid)))
		PMC_CALL_HOOK_UNLOCKED(curthread, PMC_FN_DO_SAMPLES, NULL);
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, hard, td->td_intr_frame);
#endif
	callout_process(sbinuptime());
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(int usermode, uintfptr_t pc)
{

	atomic_add_int(&ticks, 1);
	hardclock_cpu(usermode);
	tc_ticktock(1);
	cpu_tick_calibration();
	/*
	 * If no separate statistics clock is available, run it from here.
	 *
	 * XXX: this only works for UP
	 */
	if (stathz == 0) {
		profclock(usermode, pc);
		statclock(usermode);
	}
#ifdef DEVICE_POLLING
	hardclock_device_poll();	/* this is very short and quick */
#endif /* DEVICE_POLLING */
#ifdef SW_WATCHDOG
	if (watchdog_enabled > 0 && --watchdog_ticks <= 0)
		watchdog_fire();
#endif /* SW_WATCHDOG */
}

void
hardclock_cnt(int cnt, int usermode)
{
	struct pstats *pstats;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int *t = DPCPU_PTR(pcputicks);
	int flags, global, newticks;
#ifdef SW_WATCHDOG
	int i;
#endif /* SW_WATCHDOG */

	/*
	 * Update per-CPU and possibly global ticks values.
	 */
	*t += cnt;
	do {
		global = ticks;
		newticks = *t - global;
		if (newticks <= 0) {
			if (newticks < -1)
				*t = global - 1;
			newticks = 0;
			break;
		}
	} while (!atomic_cmpset_int(&ticks, global, *t));

	/*
	 * Run current process's virtual and profile time, as needed.
	 */
	pstats = p->p_stats;
	flags = 0;
	if (usermode &&
	    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value)) {
		PROC_SLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL],
		    tick * cnt) == 0)
			flags |= TDF_ALRMPEND | TDF_ASTPENDING;
		PROC_SUNLOCK(p);
	}
	if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value)) {
		PROC_SLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_PROF],
		    tick * cnt) == 0)
			flags |= TDF_PROFPEND | TDF_ASTPENDING;
		PROC_SUNLOCK(p);
	}
	thread_lock(td);
	sched_tick(cnt);
	td->td_flags |= flags;
	thread_unlock(td);

#ifdef	HWPMC_HOOKS
	if (PMC_CPU_HAS_SAMPLES(PCPU_GET(cpuid)))
		PMC_CALL_HOOK_UNLOCKED(curthread, PMC_FN_DO_SAMPLES, NULL);
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, hard, td->td_intr_frame);
#endif
	/* We are in charge to handle this tick duty. */
	if (newticks > 0) {
		/* Dangerous and no need to call these things concurrently. */
		if (atomic_cmpset_acq_int(&global_hardclock_run, 0, 1)) {
			tc_ticktock(newticks);
#ifdef DEVICE_POLLING
			/* This is very short and quick. */
			hardclock_device_poll();
#endif /* DEVICE_POLLING */
			atomic_store_rel_int(&global_hardclock_run, 0);
		}
#ifdef SW_WATCHDOG
		if (watchdog_enabled > 0) {
			i = atomic_fetchadd_int(&watchdog_ticks, -newticks);
			if (i > 0 && i <= newticks)
				watchdog_fire();
		}
#endif /* SW_WATCHDOG */
	}
	if (curcpu == CPU_FIRST())
		cpu_tick_calibration();
}

void
hardclock_sync(int cpu)
{
	int	*t = DPCPU_ID_PTR(cpu, pcputicks);

	*t = ticks;
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

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (p->p_flag & P_STOPPROF)
		return;
	if ((p->p_flag & P_PROFIL) == 0) {
		p->p_flag |= P_PROFIL;
		mtx_lock(&time_lock);
		if (++profprocs == 1)
			cpu_startprofclock();
		mtx_unlock(&time_lock);
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
		p->p_flag &= ~P_PROFIL;
		mtx_lock(&time_lock);
		if (--profprocs == 0)
			cpu_stopprofclock();
		mtx_unlock(&time_lock);
	}
}

/*
 * Statistics clock.  Updates rusage information and calls the scheduler
 * to adjust priorities of the active thread.
 *
 * This should be called by all active processors.
 */
void
statclock(int usermode)
{

	statclock_cnt(1, usermode);
}

void
statclock_cnt(int cnt, int usermode)
{
	struct rusage *ru;
	struct vmspace *vm;
	struct thread *td;
	struct proc *p;
	long rss;
	long *cp_time;

	td = curthread;
	p = td->td_proc;

	cp_time = (long *)PCPU_PTR(cp_time);
	if (usermode) {
		/*
		 * Charge the time as appropriate.
		 */
		td->td_uticks += cnt;
		if (p->p_nice > NZERO)
			cp_time[CP_NICE] += cnt;
		else
			cp_time[CP_USER] += cnt;
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
			td->td_iticks += cnt;
			cp_time[CP_INTR] += cnt;
		} else {
			td->td_pticks += cnt;
			td->td_sticks += cnt;
			if (!TD_IS_IDLETHREAD(td))
				cp_time[CP_SYS] += cnt;
			else
				cp_time[CP_IDLE] += cnt;
		}
	}

	/* Update resource usage integrals and maximums. */
	MPASS(p->p_vmspace != NULL);
	vm = p->p_vmspace;
	ru = &td->td_ru;
	ru->ru_ixrss += pgtok(vm->vm_tsize) * cnt;
	ru->ru_idrss += pgtok(vm->vm_dsize) * cnt;
	ru->ru_isrss += pgtok(vm->vm_ssize) * cnt;
	rss = pgtok(vmspace_resident_count(vm));
	if (ru->ru_maxrss < rss)
		ru->ru_maxrss = rss;
	KTR_POINT2(KTR_SCHED, "thread", sched_tdname(td), "statclock",
	    "prio:%d", td->td_priority, "stathz:%d", (stathz)?stathz:hz);
	SDT_PROBE2(sched, , , tick, td, td->td_proc);
	thread_lock_flags(td, MTX_QUIET);
	for ( ; cnt > 0; cnt--)
		sched_clock(td);
	thread_unlock(td);
#ifdef HWPMC_HOOKS
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, stat, td->td_intr_frame);
#endif
}

void
profclock(int usermode, uintfptr_t pc)
{

	profclock_cnt(1, usermode, pc);
}

void
profclock_cnt(int cnt, int usermode, uintfptr_t pc)
{
	struct thread *td;
#ifdef GPROF
	struct gmonparam *g;
	uintfptr_t i;
#endif

	td = curthread;
	if (usermode) {
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled, record the tick.
		 * if there is no related user location yet, don't
		 * bother trying to count it.
		 */
		if (td->td_proc->p_flag & P_PROFIL)
			addupc_intr(td, pc, cnt);
	}
#ifdef GPROF
	else {
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON && pc >= g->lowpc) {
			i = PC_TO_I(g, pc);
			if (i < g->textsize) {
				KCOUNT(g, i) += cnt;
			}
		}
	}
#endif
#ifdef HWPMC_HOOKS
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, prof, td->td_intr_frame);
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

SYSCTL_PROC(_kern, KERN_CLOCKRATE, clockrate,
	CTLTYPE_STRUCT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	0, 0, sysctl_kern_clockrate, "S,clockinfo",
	"Rate and period of various kernel clocks");

#ifdef SW_WATCHDOG

static void
watchdog_config(void *unused __unused, u_int cmd, int *error)
{
	u_int u;

	u = cmd & WD_INTERVAL;
	if (u >= WD_TO_1SEC) {
		watchdog_ticks = (1 << (u - WD_TO_1SEC)) * hz;
		watchdog_enabled = 1;
		*error = 0;
	} else {
		watchdog_enabled = 0;
	}
}

/*
 * Handle a watchdog timeout by dumping interrupt information and
 * then either dropping to DDB or panicking.
 */
static void
watchdog_fire(void)
{
	int nintr;
	uint64_t inttotal;
	u_long *curintr;
	char *curname;

	curintr = intrcnt;
	curname = intrnames;
	inttotal = 0;
	nintr = sintrcnt / sizeof(u_long);

	printf("interrupt                   total\n");
	while (--nintr >= 0) {
		if (*curintr)
			printf("%-12s %20lu\n", curname, *curintr);
		curname += strlen(curname) + 1;
		inttotal += *curintr++;
	}
	printf("Total        %20ju\n", (uintmax_t)inttotal);

#if defined(KDB) && !defined(KDB_UNATTENDED)
	kdb_backtrace();
	kdb_enter(KDB_WHY_WATCHDOG, "watchdog timeout");
#else
	panic("watchdog timeout");
#endif
}

#endif /* SW_WATCHDOG */
