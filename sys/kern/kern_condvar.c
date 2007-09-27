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
#include <sys/sleepqueue.h>
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

/*
 * Initialize a condition variable.  Must be called before use.
 */
void
cv_init(struct cv *cvp, const char *desc)
{

	cvp->cv_description = desc;
	cvp->cv_waiters = 0;
}

/*
 * Destroy a condition variable.  The condition variable must be re-initialized
 * in order to be re-used.
 */
void
cv_destroy(struct cv *cvp)
{
#ifdef INVARIANTS
	struct sleepqueue *sq;

	sleepq_lock(cvp);
	sq = sleepq_lookup(cvp);
	sleepq_release(cvp);
	KASSERT(sq == NULL, ("%s: associated sleep queue non-empty", __func__));
#endif
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

	if (cold || panicstr) {
		/*
		 * During autoconfiguration, just give interrupts
		 * a chance, then just return.  Don't run any other
		 * thread or panic below, in case this is the idle
		 * process and already asleep.
		 */
		return;
	}

	sleepq_lock(cvp);

	cvp->cv_waiters++;
	DROP_GIANT();
	mtx_unlock(mp);

	sleepq_add(cvp, mp, cvp->cv_description, SLEEPQ_CONDVAR, 0);
	sleepq_wait(cvp);

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
	WITNESS_SAVE_DECL(mp);

	td = curthread;
	p = td->td_proc;
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
		return (0);
	}

	sleepq_lock(cvp);

	cvp->cv_waiters++;
	DROP_GIANT();
	mtx_unlock(mp);

	sleepq_add(cvp, mp, cvp->cv_description, SLEEPQ_CONDVAR |
	    SLEEPQ_INTERRUPTIBLE, 0);
	rval = sleepq_wait_sig(cvp);

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

	sleepq_lock(cvp);

	cvp->cv_waiters++;
	DROP_GIANT();
	mtx_unlock(mp);

	sleepq_add(cvp, mp, cvp->cv_description, SLEEPQ_CONDVAR, 0);
	sleepq_set_timeout(cvp, timo);
	rval = sleepq_timedwait(cvp);

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

	sleepq_lock(cvp);

	cvp->cv_waiters++;
	DROP_GIANT();
	mtx_unlock(mp);

	sleepq_add(cvp, mp, cvp->cv_description, SLEEPQ_CONDVAR |
	    SLEEPQ_INTERRUPTIBLE, 0);
	sleepq_set_timeout(cvp, timo);
	rval = sleepq_timedwait_sig(cvp);

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
 * Signal a condition variable, wakes up one waiting thread.  Will also wakeup
 * the swapper if the process is not in memory, so that it can bring the
 * sleeping process in.  Note that this may also result in additional threads
 * being made runnable.  Should be called with the same mutex as was passed to
 * cv_wait held.
 */
void
cv_signal(struct cv *cvp)
{

	sleepq_lock(cvp);
	if (cvp->cv_waiters > 0) {
		cvp->cv_waiters--;
		sleepq_signal(cvp, SLEEPQ_CONDVAR, -1, 0);
	} else
		sleepq_release(cvp);
}

/*
 * Broadcast a signal to a condition variable.  Wakes up all waiting threads.
 * Should be called with the same mutex as was passed to cv_wait held.
 */
void
cv_broadcastpri(struct cv *cvp, int pri)
{

	sleepq_lock(cvp);
	if (cvp->cv_waiters > 0) {
		cvp->cv_waiters = 0;
		sleepq_broadcast(cvp, SLEEPQ_CONDVAR, pri, 0);
	} else
		sleepq_release(cvp);
}
