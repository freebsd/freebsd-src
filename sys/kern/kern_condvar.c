/*-
 * Copyright (c) 2000 Jake Burkholder <jake@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/condvar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

/*
 * Common sanity checks for cv_wait* functions.
 */
#define	CV_ASSERT(cvp, mp, td) do {					\
	KASSERT((td) != NULL, ("%s: curthread NULL", __func__));	\
	KASSERT(TD_IS_RUNNING(td), ("%s: not TDS_RUNNING", __func__));	\
	KASSERT((cvp) != NULL, ("%s: cvp NULL", __func__));		\
	KASSERT((mp) != NULL, ("%s: mp NULL", __func__));		\
	mtx_assert((mp), MA_OWNED | MA_NOTRECURSED);			\
} while (0)

#ifdef INVARIANTS
#define	CV_WAIT_VALIDATE(cvp, mp) do {					\
	if (TAILQ_EMPTY(&(cvp)->cv_waitq)) {				\
		/* Only waiter. */					\
		(cvp)->cv_mtx = (mp);					\
	} else {							\
		/*							\
		 * Other waiter; assert that we're using the		\
		 * same mutex.						\
		 */							\
		KASSERT((cvp)->cv_mtx == (mp),				\
		    ("%s: Multiple mutexes", __func__));		\
	}								\
} while (0)

#define	CV_SIGNAL_VALIDATE(cvp) do {					\
	if (!TAILQ_EMPTY(&(cvp)->cv_waitq)) {				\
		KASSERT(mtx_owned((cvp)->cv_mtx),			\
		    ("%s: Mutex not owned", __func__));			\
	}								\
} while (0)

#else
#define	CV_WAIT_VALIDATE(cvp, mp)
#define	CV_SIGNAL_VALIDATE(cvp)
#endif

static void cv_timedwait_end(void *arg);

/*
 * Initialize a condition variable.  Must be called before use.
 */
void
cv_init(struct cv *cvp, const char *desc)
{

	TAILQ_INIT(&cvp->cv_waitq);
	cvp->cv_mtx = NULL;
	cvp->cv_description = desc;
}

/*
 * Destroy a condition variable.  The condition variable must be re-initialized
 * in order to be re-used.
 */
void
cv_destroy(struct cv *cvp)
{

	KASSERT(cv_waitq_empty(cvp), ("%s: cv_waitq non-empty", __func__));
}

/*
 * Common code for cv_wait* functions.  All require sched_lock.
 */

/*
 * Switch context.
 */
static __inline void
cv_switch(struct thread *td)
{
	TD_SET_SLEEPING(td);
	td->td_proc->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	CTR3(KTR_PROC, "cv_switch: resume thread %p (pid %d, %s)", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
}

/*
 * Switch context, catching signals.
 */
static __inline int
cv_switch_catch(struct thread *td)
{
	struct proc *p;
	int sig;

	/*
	 * We put ourselves on the sleep queue and start our timeout before
	 * calling cursig, as we could stop there, and a wakeup or a SIGCONT (or
	 * both) could occur while we were stopped.  A SIGCONT would cause us to
	 * be marked as TDS_SLP without resuming us, thus we must be ready for
	 * sleep when cursig is called.  If the wakeup happens while we're
	 * stopped, td->td_wchan will be 0 upon return from cursig,
	 * and TD_ON_SLEEPQ() will return false.
	 */
	td->td_flags |= TDF_SINTR;
	mtx_unlock_spin(&sched_lock);
	p = td->td_proc;
	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	sig = cursig(td);
	mtx_unlock(&p->p_sigacts->ps_mtx);
	if (thread_suspend_check(1))
		sig = SIGSTOP;
	mtx_lock_spin(&sched_lock);
	PROC_UNLOCK(p);
	if (sig != 0) {
		if (TD_ON_SLEEPQ(td))
			cv_waitq_remove(td);
		TD_SET_RUNNING(td);
	} else if (TD_ON_SLEEPQ(td)) {
		cv_switch(td);
	}
	td->td_flags &= ~TDF_SINTR;

	return sig;
}

/*
 * Add a thread to the wait queue of a condition variable.
 */
static __inline void
cv_waitq_add(struct cv *cvp, struct thread *td)
{

	td->td_flags |= TDF_CVWAITQ;
	TD_SET_ON_SLEEPQ(td);
	td->td_wchan = cvp;
	td->td_wmesg = cvp->cv_description;
	CTR3(KTR_PROC, "cv_waitq_add: thread %p (pid %d, %s)", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
	TAILQ_INSERT_TAIL(&cvp->cv_waitq, td, td_slpq);
	sched_sleep(td, td->td_priority);
}

/*
 * Wait on a condition variable.  The current thread is placed on the condition
 * variable's wait queue and suspended.  A cv_signal or cv_broadcast on the same
 * condition variable will resume the thread.  The mutex is released before
 * sleeping and will be held on return.  It is recommended that the mutex be
 * held when cv_signal or cv_broadcast are called.
 */
void
cv_wait(struct cv *cvp, struct mtx *mp)
{
	struct thread *td;
	WITNESS_SAVE_DECL(mp);

	td = curthread;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	CV_ASSERT(cvp, mp, td);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, &mp->mtx_object,
	    "Waiting on \"%s\"", cvp->cv_description);
	WITNESS_SAVE(&mp->mtx_object, mp);

	if (cold ) {
		/*
		 * During autoconfiguration, just give interrupts
		 * a chance, then just return.  Don't run any other
		 * thread or panic below, in case this is the idle
		 * process and already asleep.
		 */
		return;
	}

	mtx_lock_spin(&sched_lock);

	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT();
	mtx_unlock(mp);

	cv_waitq_add(cvp, td);
	cv_switch(td);

	mtx_unlock_spin(&sched_lock);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);
}

/*
 * Wait on a condition variable, allowing interruption by signals.  Return 0 if
 * the thread was resumed with cv_signal or cv_broadcast, EINTR or ERESTART if
 * a signal was caught.  If ERESTART is returned the system call should be
 * restarted if possible.
 */
int
cv_wait_sig(struct cv *cvp, struct mtx *mp)
{
	struct thread *td;
	struct proc *p;
	int rval;
	int sig;
	WITNESS_SAVE_DECL(mp);

	td = curthread;
	p = td->td_proc;
	rval = 0;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	CV_ASSERT(cvp, mp, td);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, &mp->mtx_object,
	    "Waiting on \"%s\"", cvp->cv_description);
	WITNESS_SAVE(&mp->mtx_object, mp);

	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * procs or panic below, in case this is the idle process and
		 * already asleep.
		 */
		return 0;
	}

	mtx_lock_spin(&sched_lock);

	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT();
	mtx_unlock(mp);

	cv_waitq_add(cvp, td);
	sig = cv_switch_catch(td);

	mtx_unlock_spin(&sched_lock);

	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	if (sig == 0)
		sig = cursig(td);	/* XXXKSE */
	if (sig != 0) {
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			rval = EINTR;
		else
			rval = ERESTART;
	}
	mtx_unlock(&p->p_sigacts->ps_mtx);
	if (p->p_flag & P_WEXIT)
		rval = EINTR;
	PROC_UNLOCK(p);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);

	return (rval);
}

/*
 * Wait on a condition variable for at most timo/hz seconds.  Returns 0 if the
 * process was resumed by cv_signal or cv_broadcast, EWOULDBLOCK if the timeout
 * expires.
 */
int
cv_timedwait(struct cv *cvp, struct mtx *mp, int timo)
{
	struct thread *td;
	int rval;
	WITNESS_SAVE_DECL(mp);

	td = curthread;
	rval = 0;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	CV_ASSERT(cvp, mp, td);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, &mp->mtx_object,
	    "Waiting on \"%s\"", cvp->cv_description);
	WITNESS_SAVE(&mp->mtx_object, mp);

	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * thread or panic below, in case this is the idle process and
		 * already asleep.
		 */
		return 0;
	}

	mtx_lock_spin(&sched_lock);

	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT();
	mtx_unlock(mp);

	cv_waitq_add(cvp, td);
	callout_reset(&td->td_slpcallout, timo, cv_timedwait_end, td);
	cv_switch(td);

	if (td->td_flags & TDF_TIMEOUT) {
		td->td_flags &= ~TDF_TIMEOUT;
		rval = EWOULDBLOCK;
	} else if (td->td_flags & TDF_TIMOFAIL)
		td->td_flags &= ~TDF_TIMOFAIL;
	else if (callout_stop(&td->td_slpcallout) == 0) {
		/*
		 * Work around race with cv_timedwait_end similar to that
		 * between msleep and endtsleep.
		 * Go back to sleep.
		 */
		TD_SET_SLEEPING(td);
		td->td_proc->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		td->td_flags &= ~TDF_TIMOFAIL;
	}

	mtx_unlock_spin(&sched_lock);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);

	return (rval);
}

/*
 * Wait on a condition variable for at most timo/hz seconds, allowing
 * interruption by signals.  Returns 0 if the thread was resumed by cv_signal
 * or cv_broadcast, EWOULDBLOCK if the timeout expires, and EINTR or ERESTART if
 * a signal was caught.
 */
int
cv_timedwait_sig(struct cv *cvp, struct mtx *mp, int timo)
{
	struct thread *td;
	struct proc *p;
	int rval;
	int sig;
	WITNESS_SAVE_DECL(mp);

	td = curthread;
	p = td->td_proc;
	rval = 0;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0);
#endif
	CV_ASSERT(cvp, mp, td);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, &mp->mtx_object,
	    "Waiting on \"%s\"", cvp->cv_description);
	WITNESS_SAVE(&mp->mtx_object, mp);

	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * thread or panic below, in case this is the idle process and
		 * already asleep.
		 */
		return 0;
	}

	mtx_lock_spin(&sched_lock);

	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT();
	mtx_unlock(mp);

	cv_waitq_add(cvp, td);
	callout_reset(&td->td_slpcallout, timo, cv_timedwait_end, td);
	sig = cv_switch_catch(td);

	if (td->td_flags & TDF_TIMEOUT) {
		td->td_flags &= ~TDF_TIMEOUT;
		rval = EWOULDBLOCK;
	} else if (td->td_flags & TDF_TIMOFAIL)
		td->td_flags &= ~TDF_TIMOFAIL;
	else if (callout_stop(&td->td_slpcallout) == 0) {
		/*
		 * Work around race with cv_timedwait_end similar to that
		 * between msleep and endtsleep.
		 * Go back to sleep.
		 */
		TD_SET_SLEEPING(td);
		td->td_proc->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		td->td_flags &= ~TDF_TIMOFAIL;
	}
	mtx_unlock_spin(&sched_lock);

	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	if (sig == 0)
		sig = cursig(td);
	if (sig != 0) {
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			rval = EINTR;
		else
			rval = ERESTART;
	}
	mtx_unlock(&p->p_sigacts->ps_mtx);
	if (p->p_flag & P_WEXIT)
		rval = EINTR;
	PROC_UNLOCK(p);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);

	return (rval);
}

/*
 * Common code for signal and broadcast.  Assumes waitq is not empty.  Must be
 * called with sched_lock held.
 */
static __inline void
cv_wakeup(struct cv *cvp)
{
	struct thread *td;

	mtx_assert(&sched_lock, MA_OWNED);
	td = TAILQ_FIRST(&cvp->cv_waitq);
	KASSERT(td->td_wchan == cvp, ("%s: bogus wchan", __func__));
	KASSERT(td->td_flags & TDF_CVWAITQ, ("%s: not on waitq", __func__));
	cv_waitq_remove(td);
	TD_CLR_SLEEPING(td);
	setrunnable(td);
}

/*
 * Signal a condition variable, wakes up one waiting thread.  Will also wakeup
 * the swapper if the process is not in memory, so that it can bring the
 * sleeping process in.  Note that this may also result in additional threads
 * being made runnable.  Should be called with the same mutex as was passed to
 * cv_wait held.
 */
void
cv_signal(struct cv *cvp)
{

	KASSERT(cvp != NULL, ("%s: cvp NULL", __func__));
	mtx_lock_spin(&sched_lock);
	if (!TAILQ_EMPTY(&cvp->cv_waitq)) {
		CV_SIGNAL_VALIDATE(cvp);
		cv_wakeup(cvp);
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Broadcast a signal to a condition variable.  Wakes up all waiting threads.
 * Should be called with the same mutex as was passed to cv_wait held.
 */
void
cv_broadcast(struct cv *cvp)
{

	KASSERT(cvp != NULL, ("%s: cvp NULL", __func__));
	mtx_lock_spin(&sched_lock);
	CV_SIGNAL_VALIDATE(cvp);
	while (!TAILQ_EMPTY(&cvp->cv_waitq))
		cv_wakeup(cvp);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Remove a thread from the wait queue of its condition variable.  This may be
 * called externally.
 */
void
cv_waitq_remove(struct thread *td)
{
	struct cv *cvp;

	mtx_assert(&sched_lock, MA_OWNED);
	if ((cvp = td->td_wchan) != NULL && td->td_flags & TDF_CVWAITQ) {
		TAILQ_REMOVE(&cvp->cv_waitq, td, td_slpq);
		td->td_flags &= ~TDF_CVWAITQ;
		td->td_wmesg = NULL;
		TD_CLR_ON_SLEEPQ(td);
	}
}

/*
 * Timeout function for cv_timedwait.  Put the thread on the runqueue and set
 * its timeout flag.
 */
static void
cv_timedwait_end(void *arg)
{
	struct thread *td;

	td = arg;
	CTR3(KTR_PROC, "cv_timedwait_end: thread %p (pid %d, %s)",
	    td, td->td_proc->p_pid, td->td_proc->p_comm);
	mtx_lock_spin(&sched_lock);
	if (TD_ON_SLEEPQ(td)) {
		cv_waitq_remove(td);
		td->td_flags |= TDF_TIMEOUT;
	} else {
		td->td_flags |= TDF_TIMOFAIL;
	}
	TD_CLR_SLEEPING(td);
	setrunnable(td);
	mtx_unlock_spin(&sched_lock);
}

/*
 * For now only abort interruptable waits.
 * The others will have to either complete on their own or have a timeout.
 */
void
cv_abort(struct thread *td)
{

	CTR3(KTR_PROC, "cv_abort: thread %p (pid %d, %s)", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
	mtx_lock_spin(&sched_lock);
	if ((td->td_flags & (TDF_SINTR|TDF_TIMEOUT)) == TDF_SINTR) {
		if (TD_ON_SLEEPQ(td)) {
			cv_waitq_remove(td);
		}
		TD_CLR_SLEEPING(td);
		setrunnable(td);
	}
	mtx_unlock_spin(&sched_lock);
}

