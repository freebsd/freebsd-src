/*-
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
 *	@(#)kern_synch.c	8.9 (Berkeley) 5/19/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_synch.c,v 1.302.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

static void synch_setup(void *dummy);
SYSINIT(synch_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, synch_setup,
    NULL);

int	hogticks;
int	lbolt;
static int pause_wchan;

static struct callout loadav_callout;
static struct callout lbolt_callout;

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */
/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
static int      fscale __unused = FSCALE;
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, 0, FSCALE, "");

static void	loadav(void *arg);
static void	lboltcb(void *arg);

void
sleepinit(void)
{

	hogticks = (hz / 10) * 2;	/* Default only. */
	init_sleepqueues();
}

/*
 * General sleep call.  Suspends the current thread until a wakeup is
 * performed on the specified identifier.  The thread will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The lock argument is unlocked before the caller is suspended, and
 * re-locked before _sleep() returns.  If priority includes the PDROP
 * flag the lock is not re-locked before returning.
 */
int
_sleep(void *ident, struct lock_object *lock, int priority,
    const char *wmesg, int timo)
{
	struct thread *td;
	struct proc *p;
	struct lock_class *class;
	int catch, flags, lock_state, pri, rval;
	WITNESS_SAVE_DECL(lock_witness);

	td = curthread;
	p = td->td_proc;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, lock,
	    "Sleeping on \"%s\"", wmesg);
	KASSERT(timo != 0 || mtx_owned(&Giant) || lock != NULL ||
	    ident == &lbolt, ("sleeping without a lock"));
	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));
	if (lock != NULL)
		class = LOCK_CLASS(lock);
	else
		class = NULL;

	if (cold) {
		/*
		 * During autoconfiguration, just return;
		 * don't run any other threads or panic below,
		 * in case this is the idle thread and already asleep.
		 * XXX: this used to do "s = splhigh(); splx(safepri);
		 * splx(s);" to give interrupts a chance, but there is
		 * no way to give interrupts a chance now.
		 */
		if (lock != NULL && priority & PDROP)
			class->lc_unlock(lock);
		return (0);
	}
	catch = priority & PCATCH;
	rval = 0;

	/*
	 * If we are already on a sleep queue, then remove us from that
	 * sleep queue first.  We have to do this to handle recursive
	 * sleeps.
	 */
	if (TD_ON_SLEEPQ(td))
		sleepq_remove(td, td->td_wchan);

	if (ident == &pause_wchan)
		flags = SLEEPQ_PAUSE;
	else
		flags = SLEEPQ_SLEEP;
	if (catch)
		flags |= SLEEPQ_INTERRUPTIBLE;

	sleepq_lock(ident);
	CTR5(KTR_PROC, "sleep: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, p->p_pid, p->p_comm, wmesg, ident);

	DROP_GIANT();
	if (lock != NULL && !(class->lc_flags & LC_SLEEPABLE)) {
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
	sleepq_add(ident, ident == &lbolt ? NULL : lock, wmesg, flags, 0);
	if (timo)
		sleepq_set_timeout(ident, timo);
	if (lock != NULL && class->lc_flags & LC_SLEEPABLE) {
		sleepq_release(ident);
		WITNESS_SAVE(lock, lock_witness);
		lock_state = class->lc_unlock(lock);
		sleepq_lock(ident);
	}

	/*
	 * Adjust this thread's priority, if necessary.
	 */
	pri = priority & PRIMASK;
	if (pri != 0 && pri != td->td_priority) {
		thread_lock(td);
		sched_prio(td, pri);
		thread_unlock(td);
	}

	if (timo && catch)
		rval = sleepq_timedwait_sig(ident);
	else if (timo)
		rval = sleepq_timedwait(ident);
	else if (catch)
		rval = sleepq_wait_sig(ident);
	else {
		sleepq_wait(ident);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	if (lock != NULL && !(priority & PDROP)) {
		class->lc_lock(lock, lock_state);
		WITNESS_RESTORE(lock, lock_witness);
	}
	return (rval);
}

int
msleep_spin(void *ident, struct mtx *mtx, const char *wmesg, int timo)
{
	struct thread *td;
	struct proc *p;
	int rval;
	WITNESS_SAVE_DECL(mtx);

	td = curthread;
	p = td->td_proc;
	KASSERT(mtx != NULL, ("sleeping without a mutex"));
	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));

	if (cold) {
		/*
		 * During autoconfiguration, just return;
		 * don't run any other threads or panic below,
		 * in case this is the idle thread and already asleep.
		 * XXX: this used to do "s = splhigh(); splx(safepri);
		 * splx(s);" to give interrupts a chance, but there is
		 * no way to give interrupts a chance now.
		 */
		return (0);
	}

	sleepq_lock(ident);
	CTR5(KTR_PROC, "msleep_spin: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, p->p_pid, p->p_comm, wmesg, ident);

	DROP_GIANT();
	mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
	WITNESS_SAVE(&mtx->lock_object, mtx);
	mtx_unlock_spin(mtx);

	/*
	 * We put ourselves on the sleep queue and start our timeout.
	 */
	sleepq_add(ident, &mtx->lock_object, wmesg, SLEEPQ_SLEEP, 0);
	if (timo)
		sleepq_set_timeout(ident, timo);

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
		ktrcsw(1, 0);
		sleepq_lock(ident);
	}
#endif
#ifdef WITNESS
	sleepq_release(ident);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "Sleeping on \"%s\"",
	    wmesg);
	sleepq_lock(ident);
#endif
	if (timo)
		rval = sleepq_timedwait(ident);
	else {
		sleepq_wait(ident);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock_spin(mtx);
	WITNESS_RESTORE(&mtx->lock_object, mtx);
	return (rval);
}

/*
 * pause() is like tsleep() except that the intention is to not be
 * explicitly woken up by another thread.  Instead, the current thread
 * simply wishes to sleep until the timeout expires.  It is
 * implemented using a dummy wait channel.
 */
int
pause(const char *wmesg, int timo)
{

	KASSERT(timo != 0, ("pause: timeout required"));
	return (tsleep(&pause_wchan, 0, wmesg, timo));
}

/*
 * Make all threads sleeping on the specified identifier runnable.
 */
void
wakeup(void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_broadcast(ident, SLEEPQ_SLEEP, -1, 0);
	if (wakeup_swapper)
		kick_proc0();
}

/*
 * Make a thread sleeping on the specified identifier runnable.
 * May wake more than one thread if a target thread is currently
 * swapped out.
 */
void
wakeup_one(void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_signal(ident, SLEEPQ_SLEEP, -1, 0);
	sleepq_release(ident);
	if (wakeup_swapper)
		kick_proc0();
}

/*
 * The machine independent parts of context switching.
 */
void
mi_switch(int flags, struct thread *newtd)
{
	uint64_t runtime, new_switchtime;
	struct thread *td;
	struct proc *p;

	td = curthread;			/* XXX */
	THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
	p = td->td_proc;		/* XXX */
	KASSERT(!TD_ON_RUNQ(td), ("mi_switch: called by old code"));
#ifdef INVARIANTS
	if (!TD_ON_LOCK(td) && !TD_IS_RUNNING(td))
		mtx_assert(&Giant, MA_NOTOWNED);
#endif
	KASSERT(td->td_critnest == 1 || (td->td_critnest == 2 &&
	    (td->td_owepreempt) && (flags & SW_INVOL) != 0 &&
	    newtd == NULL) || panicstr,
	    ("mi_switch: switch in a critical section"));
	KASSERT((flags & (SW_INVOL | SW_VOL)) != 0,
	    ("mi_switch: switch must be voluntary or involuntary"));
	KASSERT(newtd != curthread, ("mi_switch: preempting back to ourself"));

	/*
	 * Don't perform context switches from the debugger.
	 */
	if (kdb_active) {
		thread_unlock(td);
		kdb_backtrace();
		kdb_reenter();
		panic("%s: did not reenter debugger", __func__);
	}
	if (flags & SW_VOL)
		td->td_ru.ru_nvcsw++;
	else
		td->td_ru.ru_nivcsw++;
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
	PCPU_INC(cnt.v_swtch);
	PCPU_SET(switchticks, ticks);
	CTR4(KTR_PROC, "mi_switch: old thread %ld (kse %p, pid %ld, %s)",
	    td->td_tid, td->td_sched, p->p_pid, p->p_comm);
#if (KTR_COMPILE & KTR_SCHED) != 0
	if (TD_IS_IDLETHREAD(td))
		CTR3(KTR_SCHED, "mi_switch: %p(%s) prio %d idle",
		    td, td->td_proc->p_comm, td->td_priority);
	else if (newtd != NULL)
		CTR5(KTR_SCHED,
		    "mi_switch: %p(%s) prio %d preempted by %p(%s)",
		    td, td->td_proc->p_comm, td->td_priority, newtd,
		    newtd->td_proc->p_comm);
	else
		CTR6(KTR_SCHED,
		    "mi_switch: %p(%s) prio %d inhibit %d wmesg %s lock %s",
		    td, td->td_proc->p_comm, td->td_priority,
		    td->td_inhibitors, td->td_wmesg, td->td_lockname);
#endif
	/*
	 * We call thread_switchout after the KTR_SCHED prints above so kse
	 * selecting a new thread to run does not show up as a preemption.
	 */
#ifdef KSE
	if ((flags & SW_VOL) && (td->td_proc->p_flag & P_SA))
		newtd = thread_switchout(td, flags, newtd);
#endif
	sched_switch(td, newtd, flags);
	CTR3(KTR_SCHED, "mi_switch: running %p(%s) prio %d",
	    td, td->td_proc->p_comm, td->td_priority);

	CTR4(KTR_PROC, "mi_switch: new thread %ld (kse %p, pid %ld, %s)",
	    td->td_tid, td->td_sched, p->p_pid, p->p_comm);

	/* 
	 * If the last thread was exiting, finish cleaning it up.
	 */
	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
}

/*
 * Change thread state to be runnable, placing it on the run queue if
 * it is in memory.  If it is swapped out, return true so our caller
 * will know to awaken the swapper.
 */
int
setrunnable(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(td->td_proc->p_state != PRS_ZOMBIE,
	    ("setrunnable: pid %d is a zombie", td->td_proc->p_pid));
	switch (td->td_state) {
	case TDS_RUNNING:
	case TDS_RUNQ:
		return (0);
	case TDS_INHIBITED:
		/*
		 * If we are only inhibited because we are swapped out
		 * then arange to swap in this process. Otherwise just return.
		 */
		if (td->td_inhibitors != TDI_SWAPPED)
			return (0);
		/* FALLTHROUGH */
	case TDS_CAN_RUN:
		break;
	default:
		printf("state is 0x%x", td->td_state);
		panic("setrunnable(2)");
	}
	if ((td->td_flags & TDF_INMEM) == 0) {
		if ((td->td_flags & TDF_SWAPINREQ) == 0) {
			td->td_flags |= TDF_SWAPINREQ;
			return (1);
		}
	} else
		sched_wakeup(td);
	return (0);
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 * XXXKSE   Needs complete rewrite when correct info is available.
 * Completely Bogus.. only works with 1:1 (but compiles ok now :-)
 */
static void
loadav(void *arg)
{
	int i, nrun;
	struct loadavg *avg;

	nrun = sched_load();
	avg = &averunnable;

	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;

	/*
	 * Schedule the next update to occur after 5 seconds, but add a
	 * random variation to avoid synchronisation with processes that
	 * run at regular intervals.
	 */
	callout_reset(&loadav_callout, hz * 4 + (int)(random() % (hz * 2 + 1)),
	    loadav, NULL);
}

static void
lboltcb(void *arg)
{
	wakeup(&lbolt);
	callout_reset(&lbolt_callout, hz, lboltcb, NULL);
}

/* ARGSUSED */
static void
synch_setup(void *dummy)
{
	callout_init(&loadav_callout, CALLOUT_MPSAFE);
	callout_init(&lbolt_callout, CALLOUT_MPSAFE);

	/* Kick off timeout driven events by calling first time. */
	loadav(NULL);
	lboltcb(NULL);
}

/*
 * General purpose yield system call.
 */
int
yield(struct thread *td, struct yield_args *uap)
{

	thread_lock(td);
	sched_prio(td, PRI_MAX_TIMESHARE);
	mi_switch(SW_VOL, NULL);
	thread_unlock(td);
	td->td_retval[0] = 0;
	return (0);
}
