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
 *
 * $FreeBSD$
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/condvar.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

/*
 * Common sanity checks for cv_wait* functions.
 */
#define	CV_ASSERT(cvp, mp, p) do {					\
	KASSERT((p) != NULL, ("%s: curproc NULL", __FUNCTION__));	\
	KASSERT((p)->p_stat == SRUN, ("%s: not SRUN", __FUNCTION__));	\
	KASSERT((cvp) != NULL, ("%s: cvp NULL", __FUNCTION__));		\
	KASSERT((mp) != NULL, ("%s: mp NULL", __FUNCTION__));		\
	mtx_assert((mp), MA_OWNED | MA_NOTRECURSED);			\
} while (0)

#ifdef CV_DEBUG
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
		    ("%s: Multiple mutexes", __FUNCTION__));		\
	}								\
} while (0)
#define	CV_SIGNAL_VALIDATE(cvp) do {					\
	if (!TAILQ_EMPTY(&(cvp)->cv_waitq)) {				\
		KASSERT(mtx_owned((cvp)->cv_mtx),			\
		    ("%s: Mutex not owned", __FUNCTION__));		\
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

	KASSERT(cv_waitq_empty(cvp), ("%s: cv_waitq non-empty", __FUNCTION__));
}

/*
 * Common code for cv_wait* functions.  All require sched_lock.
 */

/*
 * Switch context.
 */
static __inline void
cv_switch(struct proc *p)
{

	p->p_stat = SSLEEP;
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	CTR3(KTR_PROC, "cv_switch: resume proc %p (pid %d, %s)", p, p->p_pid,
	    p->p_comm);
}

/*
 * Switch context, catching signals.
 */
static __inline int
cv_switch_catch(struct proc *p)
{
	int sig;

	/*
	 * We put ourselves on the sleep queue and start our timeout before
	 * calling CURSIG, as we could stop there, and a wakeup or a SIGCONT (or
	 * both) could occur while we were stopped.  A SIGCONT would cause us to
	 * be marked as SSLEEP without resuming us, thus we must be ready for
	 * sleep when CURSIG is called.  If the wakeup happens while we're
	 * stopped, p->p_wchan will be 0 upon return from CURSIG.
	 */
	p->p_sflag |= PS_SINTR;
	mtx_unlock_spin(&sched_lock);
	PROC_LOCK(p);
	sig = CURSIG(p);
	mtx_lock_spin(&sched_lock);
	PROC_UNLOCK_NOSWITCH(p);
	if (sig != 0) {
		if (p->p_wchan != NULL)
			cv_waitq_remove(p);
		p->p_stat = SRUN;
	} else if (p->p_wchan != NULL) {
		cv_switch(p);
	}
	p->p_sflag &= ~PS_SINTR;

	return sig;
}

/*
 * Add a process to the wait queue of a condition variable.
 */
static __inline void
cv_waitq_add(struct cv *cvp, struct proc *p)
{

	/*
	 * Process may be sitting on a slpque if asleep() was called, remove it
	 * before re-adding.
	 */
	if (p->p_wchan != NULL)
		unsleep(p);

	p->p_sflag |= PS_CVWAITQ;
	p->p_wchan = cvp;
	p->p_wmesg = cvp->cv_description;
	p->p_slptime = 0;
	p->p_pri.pri_native = p->p_pri.pri_level;
	CTR3(KTR_PROC, "cv_waitq_add: proc %p (pid %d, %s)", p, p->p_pid,
	    p->p_comm);
	TAILQ_INSERT_TAIL(&cvp->cv_waitq, p, p_slpq);
}

/*
 * Wait on a condition variable.  The current process is placed on the condition
 * variable's wait queue and suspended.  A cv_signal or cv_broadcast on the same
 * condition variable will resume the process.  The mutex is released before
 * sleeping and will be held on return.  It is recommended that the mutex be
 * held when cv_signal or cv_broadcast are called.
 */
void
cv_wait(struct cv *cvp, struct mtx *mp)
{
	struct proc *p;
	WITNESS_SAVE_DECL(mp);

	p = CURPROC;
#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	CV_ASSERT(cvp, mp, p);
	WITNESS_SLEEP(0, &mp->mtx_object);
	WITNESS_SAVE(&mp->mtx_object, mp);

	mtx_lock_spin(&sched_lock);
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * procs or panic below, in case this is the idle process and
		 * already asleep.
		 */
		mtx_unlock_spin(&sched_lock);
		return;
	}
	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT_NOSWITCH();
	mtx_unlock_flags(mp, MTX_NOSWITCH);

	cv_waitq_add(cvp, p);
	cv_switch(p);

	mtx_unlock_spin(&sched_lock);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);
}

/*
 * Wait on a condition variable, allowing interruption by signals.  Return 0 if
 * the process was resumed with cv_signal or cv_broadcast, EINTR or ERESTART if
 * a signal was caught.  If ERESTART is returned the system call should be
 * restarted if possible.
 */
int
cv_wait_sig(struct cv *cvp, struct mtx *mp)
{
	struct proc *p;
	int rval;
	int sig;
	WITNESS_SAVE_DECL(mp);

	p = CURPROC;
	rval = 0;
#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	CV_ASSERT(cvp, mp, p);
	WITNESS_SLEEP(0, &mp->mtx_object);
	WITNESS_SAVE(&mp->mtx_object, mp);

	mtx_lock_spin(&sched_lock);
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * procs or panic below, in case this is the idle process and
		 * already asleep.
		 */
		mtx_unlock_spin(&sched_lock);
		return 0;
	}
	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT_NOSWITCH();
	mtx_unlock_flags(mp, MTX_NOSWITCH);

	cv_waitq_add(cvp, p);
	sig = cv_switch_catch(p);

	mtx_unlock_spin(&sched_lock);
	PICKUP_GIANT();

	PROC_LOCK(p);
	if (sig == 0)
		sig = CURSIG(p);
	if (sig != 0) {
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			rval = EINTR;
		else
			rval = ERESTART;
	}
	PROC_UNLOCK(p);

#ifdef KTRACE
	mtx_lock(&Giant);
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
	mtx_unlock(&Giant);
#endif
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
	struct proc *p;
	int rval;
	WITNESS_SAVE_DECL(mp);

	p = CURPROC;
	rval = 0;
#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	CV_ASSERT(cvp, mp, p);
	WITNESS_SLEEP(0, &mp->mtx_object);
	WITNESS_SAVE(&mp->mtx_object, mp);

	mtx_lock_spin(&sched_lock);
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * procs or panic below, in case this is the idle process and
		 * already asleep.
		 */
		mtx_unlock_spin(&sched_lock);
		return 0;
	}
	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT_NOSWITCH();
	mtx_unlock_flags(mp, MTX_NOSWITCH);

	cv_waitq_add(cvp, p);
	callout_reset(&p->p_slpcallout, timo, cv_timedwait_end, p);
	cv_switch(p);

	if (p->p_sflag & PS_TIMEOUT) {
		p->p_sflag &= ~PS_TIMEOUT;
		rval = EWOULDBLOCK;
	} else
		callout_stop(&p->p_slpcallout);

	mtx_unlock_spin(&sched_lock);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
#endif
	PICKUP_GIANT();
	mtx_lock(mp);
	WITNESS_RESTORE(&mp->mtx_object, mp);

	return (rval);
}

/*
 * Wait on a condition variable for at most timo/hz seconds, allowing
 * interruption by signals.  Returns 0 if the process was resumed by cv_signal
 * or cv_broadcast, EWOULDBLOCK if the timeout expires, and EINTR or ERESTART if
 * a signal was caught.
 */
int
cv_timedwait_sig(struct cv *cvp, struct mtx *mp, int timo)
{
	struct proc *p;
	int rval;
	int sig;
	WITNESS_SAVE_DECL(mp);

	p = CURPROC;
	rval = 0;
#ifdef KTRACE
	if (p && KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 1, 0);
#endif
	CV_ASSERT(cvp, mp, p);
	WITNESS_SLEEP(0, &mp->mtx_object);
	WITNESS_SAVE(&mp->mtx_object, mp);

	mtx_lock_spin(&sched_lock);
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration, just give
		 * interrupts a chance, then just return; don't run any other
		 * procs or panic below, in case this is the idle process and
		 * already asleep.
		 */
		mtx_unlock_spin(&sched_lock);
		return 0;
	}
	CV_WAIT_VALIDATE(cvp, mp);

	DROP_GIANT_NOSWITCH();
	mtx_unlock_flags(mp, MTX_NOSWITCH);

	cv_waitq_add(cvp, p);
	callout_reset(&p->p_slpcallout, timo, cv_timedwait_end, p);
	sig = cv_switch_catch(p);

	if (p->p_sflag & PS_TIMEOUT) {
		p->p_sflag &= ~PS_TIMEOUT;
		rval = EWOULDBLOCK;
	} else
		callout_stop(&p->p_slpcallout);

	mtx_unlock_spin(&sched_lock);
	PICKUP_GIANT();

	PROC_LOCK(p);
	if (sig == 0)
		sig = CURSIG(p);
	if (sig != 0) {
		if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
			rval = EINTR;
		else
			rval = ERESTART;
	}
	PROC_UNLOCK(p);

#ifdef KTRACE
	mtx_lock(&Giant);
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p->p_tracep, 0, 0);
	mtx_unlock(&Giant);
#endif
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
	struct proc *p;

	mtx_assert(&sched_lock, MA_OWNED);
	p = TAILQ_FIRST(&cvp->cv_waitq);
	KASSERT(p->p_wchan == cvp, ("%s: bogus wchan", __FUNCTION__));
	KASSERT(p->p_sflag & PS_CVWAITQ, ("%s: not on waitq", __FUNCTION__));
	TAILQ_REMOVE(&cvp->cv_waitq, p, p_slpq);
	p->p_sflag &= ~PS_CVWAITQ;
	p->p_wchan = 0;
	if (p->p_stat == SSLEEP) {
		/* OPTIMIZED EXPANSION OF setrunnable(p); */
		CTR3(KTR_PROC, "cv_signal: proc %p (pid %d, %s)",
		    p, p->p_pid, p->p_comm);
		if (p->p_slptime > 1)
			updatepri(p);
		p->p_slptime = 0;
		p->p_stat = SRUN;
		if (p->p_sflag & PS_INMEM) {
			setrunqueue(p);
			maybe_resched(p);
		} else {
			p->p_sflag |= PS_SWAPINREQ;
			wakeup(&proc0);
		}
		/* END INLINE EXPANSION */
	}
}

/*
 * Signal a condition variable, wakes up one waiting process.  Will also wakeup
 * the swapper if the process is not in memory, so that it can bring the
 * sleeping process in.  Note that this may also result in additional processes
 * being made runnable.  Should be called with the same mutex as was passed to
 * cv_wait held.
 */
void
cv_signal(struct cv *cvp)
{

	KASSERT(cvp != NULL, ("%s: cvp NULL", __FUNCTION__));
	mtx_lock_spin(&sched_lock);
	if (!TAILQ_EMPTY(&cvp->cv_waitq)) {
		CV_SIGNAL_VALIDATE(cvp);
		cv_wakeup(cvp);
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Broadcast a signal to a condition variable.  Wakes up all waiting processes.
 * Should be called with the same mutex as was passed to cv_wait held.
 */
void
cv_broadcast(struct cv *cvp)
{

	KASSERT(cvp != NULL, ("%s: cvp NULL", __FUNCTION__));
	mtx_lock_spin(&sched_lock);
	CV_SIGNAL_VALIDATE(cvp);
	while (!TAILQ_EMPTY(&cvp->cv_waitq))
		cv_wakeup(cvp);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Remove a process from the wait queue of its condition variable.  This may be
 * called externally.
 */
void
cv_waitq_remove(struct proc *p)
{
	struct cv *cvp;

	mtx_lock_spin(&sched_lock);
	if ((cvp = p->p_wchan) != NULL && p->p_sflag & PS_CVWAITQ) {
		TAILQ_REMOVE(&cvp->cv_waitq, p, p_slpq);
		p->p_sflag &= ~PS_CVWAITQ;
		p->p_wchan = NULL;
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Timeout function for cv_timedwait.  Put the process on the runqueue and set
 * its timeout flag.
 */
static void
cv_timedwait_end(void *arg)
{
	struct proc *p;

	p = arg;
	CTR3(KTR_PROC, "cv_timedwait_end: proc %p (pid %d, %s)", p, p->p_pid,
	    p->p_comm);
	mtx_lock_spin(&sched_lock);
	if (p->p_wchan != NULL) {
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			cv_waitq_remove(p);
		p->p_sflag |= PS_TIMEOUT;
	}
	mtx_unlock_spin(&sched_lock);
}
