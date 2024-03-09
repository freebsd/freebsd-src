/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
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
#include "opt_ktrace.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/blockcount.h>
#include <sys/condvar.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#endif
#ifdef EPOCH_TRACE
#include <sys/epoch.h>
#endif

#include <machine/cpu.h>

static void synch_setup(void *dummy);
SYSINIT(synch_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, synch_setup,
    NULL);

int	hogticks;
static const char pause_wchan[MAXCPU];

static struct callout loadav_callout;

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */
/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static uint64_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, SYSCTL_NULL_INT_PTR, FSCALE,
    "Fixed-point scale factor used for calculating load average values");

static void	loadav(void *arg);

SDT_PROVIDER_DECLARE(sched);
SDT_PROBE_DEFINE(sched, , , preempt);

static void
sleepinit(void *unused)
{

	hogticks = (hz / 10) * 2;	/* Default only. */
	init_sleepqueues();
}

/*
 * vmem tries to lock the sleepq mutexes when free'ing kva, so make sure
 * it is available.
 */
SYSINIT(sleepinit, SI_SUB_KMEM, SI_ORDER_ANY, sleepinit, NULL);

/*
 * General sleep call.  Suspends the current thread until a wakeup is
 * performed on the specified identifier.  The thread will then be made
 * runnable with the specified priority.  Sleeps at most sbt units of time
 * (0 means no timeout).  If pri includes the PCATCH flag, let signals
 * interrupt the sleep, otherwise ignore them while sleeping.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal becomes pending, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The lock argument is unlocked before the caller is suspended, and
 * re-locked before _sleep() returns.  If priority includes the PDROP
 * flag the lock is not re-locked before returning.
 */
int
_sleep(const void *ident, struct lock_object *lock, int priority,
    const char *wmesg, sbintime_t sbt, sbintime_t pr, int flags)
{
	struct thread *td __ktrace_used;
	struct lock_class *class;
	uintptr_t lock_state;
	int catch, pri, rval, sleepq_flags;
	WITNESS_SAVE_DECL(lock_witness);

	TSENTER();
	td = curthread;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0, wmesg);
#endif
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, lock,
	    "Sleeping on \"%s\"", wmesg);
	KASSERT(sbt != 0 || mtx_owned(&Giant) || lock != NULL ||
	    (priority & PNOLOCK) != 0,
	    ("sleeping without a lock"));
	KASSERT(ident != NULL, ("_sleep: NULL ident"));
	KASSERT(TD_IS_RUNNING(td), ("_sleep: curthread not running"));
	if (priority & PDROP)
		KASSERT(lock != NULL && lock != &Giant.lock_object,
		    ("PDROP requires a non-Giant lock"));
	if (lock != NULL)
		class = LOCK_CLASS(lock);
	else
		class = NULL;

	if (SCHEDULER_STOPPED()) {
		if (lock != NULL && priority & PDROP)
			class->lc_unlock(lock);
		return (0);
	}
	catch = priority & PCATCH;
	pri = priority & PRIMASK;

	KASSERT(!TD_ON_SLEEPQ(td), ("recursive sleep"));

	if ((uintptr_t)ident >= (uintptr_t)&pause_wchan[0] &&
	    (uintptr_t)ident <= (uintptr_t)&pause_wchan[MAXCPU - 1])
		sleepq_flags = SLEEPQ_PAUSE;
	else
		sleepq_flags = SLEEPQ_SLEEP;
	if (catch)
		sleepq_flags |= SLEEPQ_INTERRUPTIBLE;

	sleepq_lock(ident);
	CTR5(KTR_PROC, "sleep: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, td->td_proc->p_pid, td->td_name, wmesg, ident);

	if (lock == &Giant.lock_object)
		mtx_assert(&Giant, MA_OWNED);
	DROP_GIANT();
	if (lock != NULL && lock != &Giant.lock_object &&
	    !(class->lc_flags & LC_SLEEPABLE)) {
		KASSERT(!(class->lc_flags & LC_SPINLOCK),
		    ("spin locks can only use msleep_spin"));
		WITNESS_SAVE(lock, lock_witness);
		lock_state = class->lc_unlock(lock);
	} else
		/* GCC needs to follow the Yellow Brick Road */
		lock_state = -1;

	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling thread_suspend_check, as we could stop there,
	 * and a wakeup or a SIGCONT (or both) could occur while we were
	 * stopped without resuming us.  Thus, we must be ready for sleep
	 * when cursig() is called.  If the wakeup happens while we're
	 * stopped, then td will no longer be on a sleep queue upon
	 * return from cursig().
	 */
	sleepq_add(ident, lock, wmesg, sleepq_flags, 0);
	if (sbt != 0)
		sleepq_set_timeout_sbt(ident, sbt, pr, flags);
	if (lock != NULL && class->lc_flags & LC_SLEEPABLE) {
		sleepq_release(ident);
		WITNESS_SAVE(lock, lock_witness);
		lock_state = class->lc_unlock(lock);
		sleepq_lock(ident);
	}
	if (sbt != 0 && catch)
		rval = sleepq_timedwait_sig(ident, pri);
	else if (sbt != 0)
		rval = sleepq_timedwait(ident, pri);
	else if (catch)
		rval = sleepq_wait_sig(ident, pri);
	else {
		sleepq_wait(ident, pri);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0, wmesg);
#endif
	PICKUP_GIANT();
	if (lock != NULL && lock != &Giant.lock_object && !(priority & PDROP)) {
		class->lc_lock(lock, lock_state);
		WITNESS_RESTORE(lock, lock_witness);
	}
	TSEXIT();
	return (rval);
}

int
msleep_spin_sbt(const void *ident, struct mtx *mtx, const char *wmesg,
    sbintime_t sbt, sbintime_t pr, int flags)
{
	struct thread *td __ktrace_used;
	int rval;
	WITNESS_SAVE_DECL(mtx);

	td = curthread;
	KASSERT(mtx != NULL, ("sleeping without a mutex"));
	KASSERT(ident != NULL, ("msleep_spin_sbt: NULL ident"));
	KASSERT(TD_IS_RUNNING(td), ("msleep_spin_sbt: curthread not running"));

	if (SCHEDULER_STOPPED())
		return (0);

	sleepq_lock(ident);
	CTR5(KTR_PROC, "msleep_spin: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, td->td_proc->p_pid, td->td_name, wmesg, ident);

	DROP_GIANT();
	mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
	WITNESS_SAVE(&mtx->lock_object, mtx);
	mtx_unlock_spin(mtx);

	/*
	 * We put ourselves on the sleep queue and start our timeout.
	 */
	sleepq_add(ident, &mtx->lock_object, wmesg, SLEEPQ_SLEEP, 0);
	if (sbt != 0)
		sleepq_set_timeout_sbt(ident, sbt, pr, flags);

	/*
	 * Can't call ktrace with any spin locks held so it can lock the
	 * ktrace_mtx lock, and WITNESS_WARN considers it an error to hold
	 * any spin lock.  Thus, we have to drop the sleepq spin lock while
	 * we handle those requests.  This is safe since we have placed our
	 * thread on the sleep queue already.
	 */
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW)) {
		sleepq_release(ident);
		ktrcsw(1, 0, wmesg);
		sleepq_lock(ident);
	}
#endif
#ifdef WITNESS
	sleepq_release(ident);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "Sleeping on \"%s\"",
	    wmesg);
	sleepq_lock(ident);
#endif
	if (sbt != 0)
		rval = sleepq_timedwait(ident, 0);
	else {
		sleepq_wait(ident, 0);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0, wmesg);
#endif
	PICKUP_GIANT();
	mtx_lock_spin(mtx);
	WITNESS_RESTORE(&mtx->lock_object, mtx);
	return (rval);
}

/*
 * pause_sbt() delays the calling thread by the given signed binary
 * time. During cold bootup, pause_sbt() uses the DELAY() function
 * instead of the _sleep() function to do the waiting. The "sbt"
 * argument must be greater than or equal to zero. A "sbt" value of
 * zero is equivalent to a "sbt" value of one tick.
 */
int
pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t pr, int flags)
{
	KASSERT(sbt >= 0, ("pause_sbt: timeout must be >= 0"));

	/* silently convert invalid timeouts */
	if (sbt == 0)
		sbt = tick_sbt;

	if ((cold && curthread == &thread0) || kdb_active ||
	    SCHEDULER_STOPPED()) {
		/*
		 * We delay one second at a time to avoid overflowing the
		 * system specific DELAY() function(s):
		 */
		while (sbt >= SBT_1S) {
			DELAY(1000000);
			sbt -= SBT_1S;
		}
		/* Do the delay remainder, if any */
		sbt = howmany(sbt, SBT_1US);
		if (sbt > 0)
			DELAY(sbt);
		return (EWOULDBLOCK);
	}
	return (_sleep(&pause_wchan[curcpu], NULL,
	    (flags & C_CATCH) ? PCATCH : 0, wmesg, sbt, pr, flags));
}

/*
 * Make all threads sleeping on the specified identifier runnable.
 */
void
wakeup(const void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_broadcast(ident, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(ident);
	if (wakeup_swapper) {
		KASSERT(ident != &proc0,
		    ("wakeup and wakeup_swapper and proc0"));
		kick_proc0();
	}
}

/*
 * Make a thread sleeping on the specified identifier runnable.
 * May wake more than one thread if a target thread is currently
 * swapped out.
 */
void
wakeup_one(const void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_signal(ident, SLEEPQ_SLEEP | SLEEPQ_DROP, 0, 0);
	if (wakeup_swapper)
		kick_proc0();
}

void
wakeup_any(const void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_signal(ident, SLEEPQ_SLEEP | SLEEPQ_UNFAIR |
	    SLEEPQ_DROP, 0, 0);
	if (wakeup_swapper)
		kick_proc0();
}

/*
 * Signal sleeping waiters after the counter has reached zero.
 */
void
_blockcount_wakeup(blockcount_t *bc, u_int old)
{

	KASSERT(_BLOCKCOUNT_WAITERS(old),
	    ("%s: no waiters on %p", __func__, bc));

	if (atomic_cmpset_int(&bc->__count, _BLOCKCOUNT_WAITERS_FLAG, 0))
		wakeup(bc);
}

/*
 * Wait for a wakeup or a signal.  This does not guarantee that the count is
 * still zero on return.  Callers wanting a precise answer should use
 * blockcount_wait() with an interlock.
 *
 * If there is no work to wait for, return 0.  If the sleep was interrupted by a
 * signal, return EINTR or ERESTART, and return EAGAIN otherwise.
 */
int
_blockcount_sleep(blockcount_t *bc, struct lock_object *lock, const char *wmesg,
    int prio)
{
	void *wchan;
	uintptr_t lock_state;
	u_int old;
	int ret;
	bool catch, drop;

	KASSERT(lock != &Giant.lock_object,
	    ("%s: cannot use Giant as the interlock", __func__));

	catch = (prio & PCATCH) != 0;
	drop = (prio & PDROP) != 0;
	prio &= PRIMASK;

	/*
	 * Synchronize with the fence in blockcount_release().  If we end up
	 * waiting, the sleepqueue lock acquisition will provide the required
	 * side effects.
	 *
	 * If there is no work to wait for, but waiters are present, try to put
	 * ourselves to sleep to avoid jumping ahead.
	 */
	if (atomic_load_acq_int(&bc->__count) == 0) {
		if (lock != NULL && drop)
			LOCK_CLASS(lock)->lc_unlock(lock);
		return (0);
	}
	lock_state = 0;
	wchan = bc;
	sleepq_lock(wchan);
	DROP_GIANT();
	if (lock != NULL)
		lock_state = LOCK_CLASS(lock)->lc_unlock(lock);
	old = blockcount_read(bc);
	ret = 0;
	do {
		if (_BLOCKCOUNT_COUNT(old) == 0) {
			sleepq_release(wchan);
			goto out;
		}
		if (_BLOCKCOUNT_WAITERS(old))
			break;
	} while (!atomic_fcmpset_int(&bc->__count, &old,
	    old | _BLOCKCOUNT_WAITERS_FLAG));
	sleepq_add(wchan, NULL, wmesg, catch ? SLEEPQ_INTERRUPTIBLE : 0, 0);
	if (catch)
		ret = sleepq_wait_sig(wchan, prio);
	else
		sleepq_wait(wchan, prio);
	if (ret == 0)
		ret = EAGAIN;

out:
	PICKUP_GIANT();
	if (lock != NULL && !drop)
		LOCK_CLASS(lock)->lc_lock(lock, lock_state);

	return (ret);
}

static void
kdb_switch(void)
{
	thread_unlock(curthread);
	kdb_backtrace();
	kdb_reenter();
	panic("%s: did not reenter debugger", __func__);
}

/*
 * mi_switch(9): The machine-independent parts of context switching.
 *
 * The thread lock is required on entry and is no longer held on return.
 */
void
mi_switch(int flags)
{
	uint64_t runtime, new_switchtime;
	struct thread *td;

	td = curthread;			/* XXX */
	THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
	KASSERT(!TD_ON_RUNQ(td), ("mi_switch: called by old code"));
#ifdef INVARIANTS
	if (!TD_ON_LOCK(td) && !TD_IS_RUNNING(td))
		mtx_assert(&Giant, MA_NOTOWNED);
#endif
	/* thread_lock() performs spinlock_enter(). */
	KASSERT(td->td_critnest == 1 || KERNEL_PANICKED(),
	    ("mi_switch: switch in a critical section"));
	KASSERT((flags & (SW_INVOL | SW_VOL)) != 0,
	    ("mi_switch: switch must be voluntary or involuntary"));
	KASSERT((flags & SW_TYPE_MASK) != 0,
	    ("mi_switch: a switch reason (type) must be specified"));
	KASSERT((flags & SW_TYPE_MASK) < SWT_COUNT,
	    ("mi_switch: invalid switch reason %d", (flags & SW_TYPE_MASK)));

	/*
	 * Don't perform context switches from the debugger.
	 */
	if (kdb_active)
		kdb_switch();
	if (SCHEDULER_STOPPED())
		return;
	if (flags & SW_VOL) {
		td->td_ru.ru_nvcsw++;
		td->td_swvoltick = ticks;
	} else {
		td->td_ru.ru_nivcsw++;
		td->td_swinvoltick = ticks;
	}
#ifdef SCHED_STATS
	SCHED_STAT_INC(sched_switch_stats[flags & SW_TYPE_MASK]);
#endif
	/*
	 * Compute the amount of time during which the current
	 * thread was running, and add that to its total so far.
	 */
	new_switchtime = cpu_ticks();
	runtime = new_switchtime - PCPU_GET(switchtime);
	td->td_runtime += runtime;
	td->td_incruntime += runtime;
	PCPU_SET(switchtime, new_switchtime);
	td->td_generation++;	/* bump preempt-detect counter */
	VM_CNT_INC(v_swtch);
	PCPU_SET(switchticks, ticks);
	CTR4(KTR_PROC, "mi_switch: old thread %ld (td_sched %p, pid %ld, %s)",
	    td->td_tid, td_get_sched(td), td->td_proc->p_pid, td->td_name);
#ifdef KDTRACE_HOOKS
	if (SDT_PROBES_ENABLED() &&
	    ((flags & SW_PREEMPT) != 0 || ((flags & SW_INVOL) != 0 &&
	    (flags & SW_TYPE_MASK) == SWT_NEEDRESCHED)))
		SDT_PROBE0(sched, , , preempt);
#endif
	sched_switch(td, flags);
	CTR4(KTR_PROC, "mi_switch: new thread %ld (td_sched %p, pid %ld, %s)",
	    td->td_tid, td_get_sched(td), td->td_proc->p_pid, td->td_name);

	/* 
	 * If the last thread was exiting, finish cleaning it up.
	 */
	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
	spinlock_exit();
}

/*
 * Change thread state to be runnable, placing it on the run queue if
 * it is in memory.  If it is swapped out, return true so our caller
 * will know to awaken the swapper.
 *
 * Requires the thread lock on entry, drops on exit.
 */
int
setrunnable(struct thread *td, int srqflags)
{
	int swapin;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(td->td_proc->p_state != PRS_ZOMBIE,
	    ("setrunnable: pid %d is a zombie", td->td_proc->p_pid));

	swapin = 0;
	switch (TD_GET_STATE(td)) {
	case TDS_RUNNING:
	case TDS_RUNQ:
		break;
	case TDS_CAN_RUN:
		KASSERT((td->td_flags & TDF_INMEM) != 0,
		    ("setrunnable: td %p not in mem, flags 0x%X inhibit 0x%X",
		    td, td->td_flags, td->td_inhibitors));
		/* unlocks thread lock according to flags */
		sched_wakeup(td, srqflags);
		return (0);
	case TDS_INHIBITED:
		/*
		 * If we are only inhibited because we are swapped out
		 * arrange to swap in this process.
		 */
		if (td->td_inhibitors == TDI_SWAPPED &&
		    (td->td_flags & TDF_SWAPINREQ) == 0) {
			td->td_flags |= TDF_SWAPINREQ;
			swapin = 1;
		}
		break;
	default:
		panic("setrunnable: state 0x%x", TD_GET_STATE(td));
	}
	if ((srqflags & (SRQ_HOLD | SRQ_HOLDTD)) == 0)
		thread_unlock(td);

	return (swapin);
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(void *arg)
{
	int i;
	uint64_t nrun;
	struct loadavg *avg;

	nrun = (uint64_t)sched_load();
	avg = &averunnable;

	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * (uint64_t)avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;

	/*
	 * Schedule the next update to occur after 5 seconds, but add a
	 * random variation to avoid synchronisation with processes that
	 * run at regular intervals.
	 */
	callout_reset_sbt(&loadav_callout,
	    SBT_1US * (4000000 + (int)(random() % 2000001)), SBT_1US,
	    loadav, NULL, C_DIRECT_EXEC | C_PREL(32));
}

static void
ast_scheduler(struct thread *td, int tda __unused)
{
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 1, __func__);
#endif
	thread_lock(td);
	sched_prio(td, td->td_user_pri);
	mi_switch(SW_INVOL | SWT_NEEDRESCHED);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 1, __func__);
#endif
}

static void
synch_setup(void *dummy __unused)
{
	callout_init(&loadav_callout, 1);
	ast_register(TDA_SCHED, ASTR_ASTF_REQUIRED, 0, ast_scheduler);

	/* Kick off timeout driven events by calling first time. */
	loadav(NULL);
}

bool
should_yield(void)
{

	return ((u_int)ticks - (u_int)curthread->td_swvoltick >= hogticks);
}

void
maybe_yield(void)
{

	if (should_yield())
		kern_yield(PRI_USER);
}

void
kern_yield(int prio)
{
	struct thread *td;

	td = curthread;
	DROP_GIANT();
	thread_lock(td);
	if (prio == PRI_USER)
		prio = td->td_user_pri;
	if (prio >= 0)
		sched_prio(td, prio);
	mi_switch(SW_VOL | SWT_RELINQUISH);
	PICKUP_GIANT();
}

/*
 * General purpose yield system call.
 */
int
sys_yield(struct thread *td, struct yield_args *uap)
{

	thread_lock(td);
	if (PRI_BASE(td->td_pri_class) == PRI_TIMESHARE)
		sched_prio(td, PRI_MAX_TIMESHARE);
	mi_switch(SW_VOL | SWT_RELINQUISH);
	td->td_retval[0] = 0;
	return (0);
}

int
sys_sched_getcpu(struct thread *td, struct sched_getcpu_args *uap)
{
	td->td_retval[0] = td->td_oncpu;
	return (0);
}
