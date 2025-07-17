/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#include "opt_kdb.h"
#include "opt_device_polling.h"
#include "opt_hwpmc_hooks.h"
#include "opt_ntp.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/epoch.h>
#include <sys/eventhandler.h>
#include <sys/gtaskqueue.h>
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

/* Spin-lock protecting profiling statistics. */
static struct mtx time_lock;

SDT_PROVIDER_DECLARE(sched);
SDT_PROBE_DEFINE2(sched, , , tick, "struct thread *", "struct proc *");

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

SYSCTL_PROC(_kern, OID_AUTO, cp_time, CTLTYPE_LONG|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0,0, sysctl_kern_cp_time, "LU", "CPU time statistics");

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

SYSCTL_PROC(_kern, OID_AUTO, cp_times, CTLTYPE_LONG|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0,0, sysctl_kern_cp_times, "LU", "per-CPU time statistics");

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
deadlres_td_on_lock(struct proc *p, struct thread *td, int blkticks)
{
	int tticks;

	sx_assert(&allproc_lock, SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	/*
	 * The thread should be blocked on a turnstile, simply check
	 * if the turnstile channel is in good state.
	 */
	MPASS(td->td_blocked != NULL);

	tticks = ticks - td->td_blktick;
	if (tticks > blkticks)
		/*
		 * Accordingly with provided thresholds, this thread is stuck
		 * for too long on a turnstile.
		 */
		panic("%s: possible deadlock detected for %p (%s), "
		    "blocked for %d ticks\n", __func__,
		    td, sched_tdname(td), tticks);
}

static void
deadlres_td_sleep_q(struct proc *p, struct thread *td, int slpticks)
{
	const void *wchan;
	int i, slptype, tticks;

	sx_assert(&allproc_lock, SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	/*
	 * Check if the thread is sleeping on a lock, otherwise skip the check.
	 * Drop the thread lock in order to avoid a LOR with the sleepqueue
	 * spinlock.
	 */
	wchan = td->td_wchan;
	tticks = ticks - td->td_slptick;
	slptype = sleepq_type(wchan);
	if ((slptype == SLEEPQ_SX || slptype == SLEEPQ_LK) &&
	    tticks > slpticks) {
		/*
		 * Accordingly with provided thresholds, this thread is stuck
		 * for too long on a sleepqueue.
		 * However, being on a sleepqueue, we might still check for the
		 * blessed list.
		 */
		for (i = 0; blessed[i] != NULL; i++)
			if (!strcmp(blessed[i], td->td_wmesg))
				return;

		panic("%s: possible deadlock detected for %p (%s), "
		    "blocked for %d ticks\n", __func__,
		    td, sched_tdname(td), tticks);
	}
}

static void
deadlkres(void)
{
	struct proc *p;
	struct thread *td;
	int blkticks, slpticks, tryl;

	tryl = 0;
	for (;;) {
		blkticks = blktime_threshold * hz;
		slpticks = slptime_threshold * hz;

		/*
		 * Avoid to sleep on the sx_lock in order to avoid a
		 * possible priority inversion problem leading to
		 * starvation.
		 * If the lock can't be held after 100 tries, panic.
		 */
		if (!sx_try_slock(&allproc_lock)) {
			if (tryl > 100)
				panic("%s: possible deadlock detected "
				    "on allproc_lock\n", __func__);
			tryl++;
			pause("allproc", sleepfreq * hz);
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
				if (TD_ON_LOCK(td))
					deadlres_td_on_lock(p, td,
					    blkticks);
				else if (TD_IS_SLEEPING(td))
					deadlres_td_sleep_q(p, td,
					    slpticks);
				thread_unlock(td);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);

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

static SYSCTL_NODE(_debug, OID_AUTO, deadlkres, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Deadlock resolver");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, slptime_threshold, CTLFLAG_RWTUN,
    &slptime_threshold, 0,
    "Number of seconds within is valid to sleep on a sleepqueue");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, blktime_threshold, CTLFLAG_RWTUN,
    &blktime_threshold, 0,
    "Number of seconds within is valid to block on a turnstile");
SYSCTL_INT(_debug_deadlkres, OID_AUTO, sleepfreq, CTLFLAG_RWTUN, &sleepfreq, 0,
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

#include <sys/watchdog.h>

static long watchdog_ticks;
static int watchdog_enabled;
static void watchdog_fire(void);
static void watchdog_config(void *, u_int, int *);

static void
watchdog_attach(void)
{
	EVENTHANDLER_REGISTER(watchdog_list, watchdog_config, NULL, 0);
}

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
int	psratio;

DPCPU_DEFINE_STATIC(long, pcputicks);	/* Per-CPU version of ticks. */
#ifdef DEVICE_POLLING
static int devpoll_run = 0;
#endif

static void
ast_oweupc(struct thread *td, int tda __unused)
{
	if ((td->td_proc->p_flag & P_PROFIL) == 0)
		return;
	addupc_task(td, td->td_profil_addr, td->td_profil_ticks);
	td->td_profil_ticks = 0;
	td->td_pflags &= ~TDP_OWEUPC;
}

static void
ast_alrm(struct thread *td, int tda __unused)
{
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK(p);
	kern_psignal(p, SIGVTALRM);
	PROC_UNLOCK(p);
}

static void
ast_prof(struct thread *td, int tda __unused)
{
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK(p);
	kern_psignal(p, SIGPROF);
	PROC_UNLOCK(p);
}

/*
 * Initialize clock frequencies and start both clocks running.
 */
static void
initclocks(void *dummy __unused)
{
	int i;

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

	ast_register(TDA_OWEUPC, ASTR_ASTF_REQUIRED, 0, ast_oweupc);
	ast_register(TDA_ALRM, ASTR_ASTF_REQUIRED, 0, ast_alrm);
	ast_register(TDA_PROF, ASTR_ASTF_REQUIRED, 0, ast_prof);

#ifdef SW_WATCHDOG
	/* Enable hardclock watchdog now, even if a hardware watchdog exists. */
	watchdog_attach();
#else
	/* Volunteer to run a software watchdog. */
	if (wdog_software_attach == NULL)
		wdog_software_attach = watchdog_attach;
#endif
}
SYSINIT(clocks, SI_SUB_CLOCKS, SI_ORDER_FIRST, initclocks, NULL);

static __noinline void
hardclock_itimer(struct thread *td, struct pstats *pstats, int cnt, int usermode)
{
	struct proc *p;
	int ast;

	ast = 0;
	p = td->td_proc;
	if (usermode &&
	    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value)) {
		PROC_ITIMLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL],
		    tick * cnt) == 0)
			ast |= TDAI(TDA_ALRM);
		PROC_ITIMUNLOCK(p);
	}
	if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value)) {
		PROC_ITIMLOCK(p);
		if (itimerdecr(&pstats->p_timer[ITIMER_PROF],
		    tick * cnt) == 0)
			ast |= TDAI(TDA_PROF);
		PROC_ITIMUNLOCK(p);
	}
	if (ast != 0)
		ast_sched_mask(td, ast);
}

void
hardclock(int cnt, int usermode)
{
	struct pstats *pstats;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	long global, newticks, *t;

	/*
	 * Update per-CPU and possibly global ticks values.
	 */
	t = DPCPU_PTR(pcputicks);
	*t += cnt;
	global = atomic_load_long(&ticksl);
	do {
		newticks = *t - global;
		if (newticks <= 0) {
			if (newticks < -1)
				*t = global - 1;
			newticks = 0;
			break;
		}
	} while (!atomic_fcmpset_long(&ticksl, &global, *t));

	/*
	 * Run current process's virtual and profile time, as needed.
	 */
	pstats = p->p_stats;
	if (__predict_false(
	    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) ||
	    timevalisset(&pstats->p_timer[ITIMER_PROF].it_value)))
		hardclock_itimer(td, pstats, cnt, usermode);

#ifdef	HWPMC_HOOKS
	if (PMC_CPU_HAS_SAMPLES(PCPU_GET(cpuid)))
		PMC_CALL_HOOK_UNLOCKED(curthread, PMC_FN_DO_SAMPLES, NULL);
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, hard, td->td_intr_frame);
#endif
	/* We are in charge to handle this tick duty. */
	if (newticks > 0) {
		tc_ticktock(newticks);
#ifdef DEVICE_POLLING
		/* Dangerous and no need to call these things concurrently. */
		if (atomic_cmpset_acq_int(&devpoll_run, 0, 1)) {
			/* This is very short and quick. */
			hardclock_device_poll();
			atomic_store_rel_int(&devpoll_run, 0);
		}
#endif /* DEVICE_POLLING */
		if (watchdog_enabled > 0) {
			long left;

			left = atomic_fetchadd_long(&watchdog_ticks, -newticks);
			if (left > 0 && left <= newticks)
				watchdog_fire();
		}
		intr_event_handle(clk_intr_event, NULL);
	}
	if (curcpu == CPU_FIRST())
		cpu_tick_calibration();
	if (__predict_false(DPCPU_GET(epoch_cb_count)))
		GROUPTASK_ENQUEUE(DPCPU_PTR(epoch_cb_task));
}

void
hardclock_sync(int cpu)
{
	long *t;

	KASSERT(!CPU_ABSENT(cpu), ("Absent CPU %d", cpu));

	t = DPCPU_ID_PTR(cpu, pcputicks);
	*t = ticksl;
}

/*
 * Regular integer scaling formula without losing precision:
 */
#define	TIME_INT_SCALE(value, mul, div) \
	(((value) / (div)) * (mul) + (((value) % (div)) * (mul)) / (div))

/*
 * Macro for converting seconds and microseconds into actual ticks,
 * based on the given hz value:
 */
#define	TIME_TO_TICKS(sec, usec, hz) \
	((sec) * (hz) + TIME_INT_SCALE(usec, hz, 1 << 6) / (1000000 >> 6))

#define	TIME_ASSERT_VALID_HZ(hz)	\
	_Static_assert(TIME_TO_TICKS(INT_MAX / (hz) - 1, 999999, hz) >= 0 && \
		       TIME_TO_TICKS(INT_MAX / (hz) - 1, 999999, hz) < INT_MAX,	\
		       "tvtohz() can overflow the regular integer type")

/*
 * Compile time assert the maximum and minimum values to fit into a
 * regular integer when computing TIME_TO_TICKS():
 */
TIME_ASSERT_VALID_HZ(HZ_MAXIMUM);
TIME_ASSERT_VALID_HZ(HZ_MINIMUM);

/*
 * The formula is mostly linear, but test some more common values just
 * in case:
 */
TIME_ASSERT_VALID_HZ(1024);
TIME_ASSERT_VALID_HZ(1000);
TIME_ASSERT_VALID_HZ(128);
TIME_ASSERT_VALID_HZ(100);

/*
 * Compute number of ticks representing the specified amount of time.
 * If the specified time is negative, a value of 1 is returned. This
 * function returns a value from 1 up to and including INT_MAX.
 */
int
tvtohz(struct timeval *tv)
{
	int retval;

	/*
	 * The values passed here may come from user-space and these
	 * checks ensure "tv_usec" is within its allowed range:
	 */

	/* check for tv_usec underflow */
	if (__predict_false(tv->tv_usec < 0)) {
		tv->tv_sec += tv->tv_usec / 1000000;
		tv->tv_usec = tv->tv_usec % 1000000;
		/* convert tv_usec to a positive value */
		if (__predict_true(tv->tv_usec < 0)) {
			tv->tv_usec += 1000000;
			tv->tv_sec -= 1;
		}
	/* check for tv_usec overflow */
	} else if (__predict_false(tv->tv_usec >= 1000000)) {
		tv->tv_sec += tv->tv_usec / 1000000;
		tv->tv_usec = tv->tv_usec % 1000000;
	}

	/* check for tv_sec underflow */
	if (__predict_false(tv->tv_sec < 0))
		return (1);
	/* check for tv_sec overflow (including room for the tv_usec part) */
	else if (__predict_false(tv->tv_sec >= tick_seconds_max))
		return (INT_MAX);

	/* cast to "int" to avoid platform differences */
	retval = TIME_TO_TICKS((int)tv->tv_sec, (int)tv->tv_usec, hz);

	/* add one additional tick */
	return (retval + 1);
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(struct proc *p)
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
stopprofclock(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (p->p_flag & P_PROFIL) {
		if (p->p_profthreads != 0) {
			while (p->p_profthreads != 0) {
				p->p_flag |= P_STOPPROF;
				msleep(&p->p_profthreads, &p->p_mtx, PPAUSE,
				    "stopprof", 0);
			}
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
statclock(int cnt, int usermode)
{
	struct rusage *ru;
	struct vmspace *vm;
	struct thread *td;
	struct proc *p;
	long rss;
	long *cp_time;
	uint64_t runtime, new_switchtime;

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

	/*
	 * Compute the amount of time during which the current
	 * thread was running, and add that to its total so far.
	 */
	new_switchtime = cpu_ticks();
	runtime = new_switchtime - PCPU_GET(switchtime);
	td->td_runtime += runtime;
	td->td_incruntime += runtime;
	PCPU_SET(switchtime, new_switchtime);

	sched_clock(td, cnt);
	thread_unlock(td);
#ifdef HWPMC_HOOKS
	if (td->td_intr_frame != NULL)
		PMC_SOFT_CALL_TF( , , clock, stat, td->td_intr_frame);
#endif
}

void
profclock(int cnt, int usermode, uintfptr_t pc)
{
	struct thread *td;

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
 * Handle a watchdog timeout by dropping to DDB or panicking.
 */
static void
watchdog_fire(void)
{

#if defined(KDB) && !defined(KDB_UNATTENDED)
	kdb_backtrace();
	kdb_enter(KDB_WHY_WATCHDOG, "watchdog timeout");
#else
	panic("watchdog timeout");
#endif
}
