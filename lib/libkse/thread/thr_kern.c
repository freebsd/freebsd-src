/*
 * Copyright (C) 2003 Daniel M. Eischen <deischen@freebsd.org>
 * Copyright (C) 2002 Jonathon Mini <mini@freebsd.org>
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/kse.h>
#include <sys/signalvar.h>
#include <sys/queue.h>
#include <machine/atomic.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "atomic_ops.h"
#include "thr_private.h"
#include "pthread_md.h"
#include "libc_private.h"

/*#define DEBUG_THREAD_KERN */
#ifdef DEBUG_THREAD_KERN
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/*
 * Define a high water mark for the maximum number of threads that
 * will be cached.  Once this level is reached, any extra threads
 * will be free()'d.
 *
 * XXX - It doesn't make sense to worry about the maximum number of
 *       KSEs that we can cache because the system will limit us to
 *       something *much* less than the maximum number of threads
 *       that we can have.  Disregarding KSEs in their own group,
 *       the maximum number of KSEs is the number of processors in
 *       the system.
 */
#define	MAX_CACHED_THREADS	100
#define	KSE_STACKSIZE		16384

#define	KSE_SET_MBOX(kse, thrd) \
	(kse)->k_mbx.km_curthread = &(thrd)->tmbx

#define	KSE_SET_EXITED(kse)	(kse)->k_flags |= KF_EXITED

/*
 * Macros for manipulating the run queues.  The priority queue
 * routines use the thread's pqe link and also handle the setting
 * and clearing of the thread's THR_FLAGS_IN_RUNQ flag.
 */
#define	KSE_RUNQ_INSERT_HEAD(kse, thrd)			\
	_pq_insert_head(&(kse)->k_schedq->sq_runq, thrd)
#define	KSE_RUNQ_INSERT_TAIL(kse, thrd)			\
	_pq_insert_tail(&(kse)->k_schedq->sq_runq, thrd)
#define	KSE_RUNQ_REMOVE(kse, thrd)			\
	_pq_remove(&(kse)->k_schedq->sq_runq, thrd)
#define	KSE_RUNQ_FIRST(kse)	_pq_first(&(kse)->k_schedq->sq_runq)

#define KSE_RUNQ_THREADS(kse)	((kse)->k_schedq->sq_runq.pq_threads)

/*
 * We've got to keep track of everything that is allocated, not only
 * to have a speedy free list, but also so they can be deallocated
 * after a fork().
 */
static TAILQ_HEAD(, kse)	active_kseq;
static TAILQ_HEAD(, kse)	free_kseq;
static TAILQ_HEAD(, kse_group)	free_kse_groupq;
static TAILQ_HEAD(, kse_group)	active_kse_groupq;
static TAILQ_HEAD(, kse_group)	gc_ksegq;
static struct lock		kse_lock;	/* also used for kseg queue */
static int			free_kse_count = 0;
static int			free_kseg_count = 0;
static TAILQ_HEAD(, pthread)	free_threadq;
static struct lock		thread_lock;
static int			free_thread_count = 0;
static int			inited = 0;
static int			active_kse_count = 0;
static int			active_kseg_count = 0;

#ifdef DEBUG_THREAD_KERN
static void	dump_queues(struct kse *curkse);
#endif
static void	kse_check_completed(struct kse *kse);
static void	kse_check_waitq(struct kse *kse);
static void	kse_check_signals(struct kse *kse);
static void	kse_fini(struct kse *curkse);
static void	kse_reinit(struct kse *kse);
static void	kse_sched_multi(struct kse *curkse);
#ifdef NOT_YET
static void	kse_sched_single(struct kse *curkse);
#endif
static void	kse_switchout_thread(struct kse *kse, struct pthread *thread);
static void	kse_wait(struct kse *kse, struct pthread *td_wait);
static void	kse_free_unlocked(struct kse *kse);
static void	kseg_free_unlocked(struct kse_group *kseg);
static void	kseg_init(struct kse_group *kseg);
static void	kseg_reinit(struct kse_group *kseg);
static void	kse_waitq_insert(struct pthread *thread);
static void	kse_wakeup_multi(struct kse *curkse);
static void	kse_wakeup_one(struct pthread *thread);
static void	thr_cleanup(struct kse *kse, struct pthread *curthread);
static void	thr_resume_wrapper(int unused_1, siginfo_t *unused_2,
		    ucontext_t *ucp);
static void	thr_resume_check(struct pthread *curthread, ucontext_t *ucp,
		    struct pthread_sigframe *psf);
static int	thr_timedout(struct pthread *thread, struct timespec *curtime);

/*
 * This is called after a fork().
 * No locks need to be taken here since we are guaranteed to be
 * single threaded.
 */
void
_kse_single_thread(struct pthread *curthread)
{
	struct kse *kse, *kse_next;
	struct kse_group *kseg, *kseg_next;
	struct pthread *thread, *thread_next;
	kse_critical_t crit;
	int i;

	/*
	 * Disable upcalls and clear the threaded flag.
	 * XXX - I don't think we need to disable upcalls after a fork().
	 *       but it doesn't hurt.
	 */
	crit = _kse_critical_enter();
	__isthreaded = 0;

	/*
	 * Enter a loop to remove and free all threads other than
	 * the running thread from the active thread list:
	 */
	for (thread = TAILQ_FIRST(&_thread_list); thread != NULL;
	    thread = thread_next) {
		/*
		 * Advance to the next thread before the destroying
		 * the current thread.
		*/
		thread_next = TAILQ_NEXT(thread, tle);

		/*
		 * Remove this thread from the list (the current
		 * thread will be removed but re-added by libpthread
		 * initialization.
		 */
		TAILQ_REMOVE(&_thread_list, thread, tle);
		/* Make sure this isn't the running thread: */
		if (thread != curthread) {
			_thr_stack_free(&thread->attr);
			if (thread->specific != NULL)
				free(thread->specific);
			for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
				_lockuser_destroy(&thread->lockusers[i]);
			}
			_lock_destroy(&thread->lock);
			free(thread);
		}
	}

	TAILQ_INIT(&curthread->mutexq);		/* initialize mutex queue */
	curthread->joiner = NULL;		/* no joining threads yet */
	sigemptyset(&curthread->sigpend);	/* clear pending signals */
	if (curthread->specific != NULL) {
		free(curthread->specific);
		curthread->specific = NULL;
		curthread->specific_data_count = 0;
	}

	/* Free the free KSEs: */
	while ((kse = TAILQ_FIRST(&free_kseq)) != NULL) {
		TAILQ_REMOVE(&free_kseq, kse, k_qe);
		_ksd_destroy(&kse->k_ksd);
		if (kse->k_stack.ss_sp != NULL)
			free(kse->k_stack.ss_sp);
		free(kse);
	}
	free_kse_count = 0;

	/* Free the active KSEs: */
	for (kse = TAILQ_FIRST(&active_kseq); kse != NULL; kse = kse_next) {
		kse_next = TAILQ_NEXT(kse, k_qe);
		TAILQ_REMOVE(&active_kseq, kse, k_qe);
		for (i = 0; i < MAX_KSE_LOCKLEVEL; i++) {
			_lockuser_destroy(&kse->k_lockusers[i]);
		}
		if (kse->k_stack.ss_sp != NULL)
			free(kse->k_stack.ss_sp);
		_lock_destroy(&kse->k_lock);
		free(kse);
	}
	active_kse_count = 0;

	/* Free the free KSEGs: */
	while ((kseg = TAILQ_FIRST(&free_kse_groupq)) != NULL) {
		TAILQ_REMOVE(&free_kse_groupq, kseg, kg_qe);
		_lock_destroy(&kseg->kg_lock);
		_pq_free(&kseg->kg_schedq.sq_runq);
		free(kseg);
	}
	free_kseg_count = 0;

	/* Free the active KSEGs: */
	for (kseg = TAILQ_FIRST(&active_kse_groupq);
	    kseg != NULL; kseg = kseg_next) {
		kseg_next = TAILQ_NEXT(kseg, kg_qe);
		TAILQ_REMOVE(&active_kse_groupq, kseg, kg_qe);
		_lock_destroy(&kseg->kg_lock);
		_pq_free(&kseg->kg_schedq.sq_runq);
		free(kseg);
	}
	active_kseg_count = 0;

	/* Free the free threads. */
	while ((thread = TAILQ_FIRST(&free_threadq)) != NULL) {
		TAILQ_REMOVE(&free_threadq, thread, tle);
		if (thread->specific != NULL)
			free(thread->specific);
		for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
			_lockuser_destroy(&thread->lockusers[i]);
		}
		_lock_destroy(&thread->lock);
		free(thread);
	}
	free_thread_count = 0;

	/* Free the to-be-gc'd threads. */
	while ((thread = TAILQ_FIRST(&_thread_gc_list)) != NULL) {
		TAILQ_REMOVE(&_thread_gc_list, thread, gcle);
		for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
			_lockuser_destroy(&thread->lockusers[i]);
		}
		_lock_destroy(&thread->lock);
		free(thread);
	}
	TAILQ_INIT(&gc_ksegq);
	_gc_count = 0;

	if (inited != 0) {
		/*
		 * Destroy these locks; they'll be recreated to assure they
		 * are in the unlocked state.
		 */
		_lock_destroy(&kse_lock);
		_lock_destroy(&thread_lock);
		_lock_destroy(&_thread_list_lock);
		inited = 0;
	}

	/*
	 * After a fork(), the leftover thread goes back to being
	 * scope process.
	 */
	curthread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;
	curthread->attr.flags |= PTHREAD_SCOPE_PROCESS;

	/*
	 * After a fork, we are still operating on the thread's original
	 * stack.  Don't clear the THR_FLAGS_USER from the thread's
	 * attribute flags.
	 */

	/* Initialize the threads library. */
	curthread->kse = NULL;
	curthread->kseg = NULL;
	_kse_initial = NULL;
	_libpthread_init(curthread);
}

/*
 * This is used to initialize housekeeping and to initialize the
 * KSD for the KSE.
 */
void
_kse_init(void)
{
	if (inited == 0) {
		TAILQ_INIT(&active_kseq);
		TAILQ_INIT(&active_kse_groupq);
		TAILQ_INIT(&free_kseq);
		TAILQ_INIT(&free_kse_groupq);
		TAILQ_INIT(&free_threadq);
		TAILQ_INIT(&gc_ksegq);
		if (_lock_init(&kse_lock, LCK_ADAPTIVE,
		    _kse_lock_wait, _kse_lock_wakeup) != 0)
			PANIC("Unable to initialize free KSE queue lock");
		if (_lock_init(&thread_lock, LCK_ADAPTIVE,
		    _kse_lock_wait, _kse_lock_wakeup) != 0)
			PANIC("Unable to initialize free thread queue lock");
		if (_lock_init(&_thread_list_lock, LCK_ADAPTIVE,
		    _kse_lock_wait, _kse_lock_wakeup) != 0)
			PANIC("Unable to initialize thread list lock");
		active_kse_count = 0;
		active_kseg_count = 0;
		_gc_count = 0;
		inited = 1;
	}
}

int
_kse_isthreaded(void)
{
	return (__isthreaded != 0);
}

/*
 * This is called when the first thread (other than the initial
 * thread) is created.
 */
int
_kse_setthreaded(int threaded)
{
	if ((threaded != 0) && (__isthreaded == 0)) {
		/*
		 * Locking functions in libc are required when there are
		 * threads other than the initial thread.
		 */
		__isthreaded = 1;

		/*
		 * Tell the kernel to create a KSE for the initial thread
		 * and enable upcalls in it.
		 */
		_kse_initial->k_flags |= KF_STARTED;
		if (kse_create(&_kse_initial->k_mbx, 0) != 0) {
			_kse_initial->k_flags &= ~KF_STARTED;
			__isthreaded = 0;
			/* may abort() */
			DBG_MSG("kse_create failed\n");
			return (-1);
		}
		KSE_SET_MBOX(_kse_initial, _thr_initial);
		_thr_setmaxconcurrency();
	}
	return (0);
}

/*
 * Lock wait and wakeup handlers for KSE locks.  These are only used by
 * KSEs, and should never be used by threads.  KSE locks include the
 * KSE group lock (used for locking the scheduling queue) and the
 * kse_lock defined above.
 *
 * When a KSE lock attempt blocks, the entire KSE blocks allowing another
 * KSE to run.  For the most part, it doesn't make much sense to try and
 * schedule another thread because you need to lock the scheduling queue
 * in order to do that.  And since the KSE lock is used to lock the scheduling
 * queue, you would just end up blocking again.
 */
void
_kse_lock_wait(struct lock *lock, struct lockuser *lu)
{
	struct kse *curkse = (struct kse *)_LCK_GET_PRIVATE(lu);
	struct timespec ts;
	int saved_flags;

	if (curkse->k_mbx.km_curthread != NULL)
		PANIC("kse_lock_wait does not disable upcall.\n");
	/*
	 * Enter a loop to wait until we get the lock.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;  /* 1 sec */
	while (_LCK_BUSY(lu)) {
		/*
		 * Yield the kse and wait to be notified when the lock
		 * is granted.
		 */
		saved_flags = curkse->k_mbx.km_flags;
		curkse->k_mbx.km_flags |= KMF_NOUPCALL | KMF_NOCOMPLETED;
		kse_release(&ts);
		curkse->k_mbx.km_flags = saved_flags;
	}
}

void
_kse_lock_wakeup(struct lock *lock, struct lockuser *lu)
{
	struct kse *curkse;
	struct kse *kse;
	struct kse_mailbox *mbx;

	curkse = _get_curkse();
	kse = (struct kse *)_LCK_GET_PRIVATE(lu);

	if (kse == curkse)
		PANIC("KSE trying to wake itself up in lock");
	else {
		mbx = &kse->k_mbx;
		_lock_grant(lock, lu);
		/*
		 * Notify the owning kse that it has the lock.
		 * It is safe to pass invalid address to kse_wakeup
		 * even if the mailbox is not in kernel at all,
		 * and waking up a wrong kse is also harmless.
		 */
		kse_wakeup(mbx);
	}
}

/*
 * Thread wait and wakeup handlers for thread locks.  These are only used
 * by threads, never by KSEs.  Thread locks include the per-thread lock
 * (defined in its structure), and condition variable and mutex locks.
 */
void
_thr_lock_wait(struct lock *lock, struct lockuser *lu)
{
	struct pthread *curthread = (struct pthread *)lu->lu_private;

	do {
		THR_SCHED_LOCK(curthread, curthread);
		THR_SET_STATE(curthread, PS_LOCKWAIT);
		THR_SCHED_UNLOCK(curthread, curthread);
		_thr_sched_switch(curthread);
	} while _LCK_BUSY(lu);
}

void
_thr_lock_wakeup(struct lock *lock, struct lockuser *lu)
{
	struct pthread *thread;
	struct pthread *curthread;

	curthread = _get_curthread();
	thread = (struct pthread *)_LCK_GET_PRIVATE(lu);

	THR_SCHED_LOCK(curthread, thread);
	_lock_grant(lock, lu);
	_thr_setrunnable_unlocked(thread);
	THR_SCHED_UNLOCK(curthread, thread);
}

kse_critical_t
_kse_critical_enter(void)
{
	kse_critical_t crit;

	crit = _ksd_readandclear_tmbx;
	return (crit);
}

void
_kse_critical_leave(kse_critical_t crit)
{
	struct pthread *curthread;

	_ksd_set_tmbx(crit);
	if ((crit != NULL) && ((curthread = _get_curthread()) != NULL))
		THR_YIELD_CHECK(curthread);
}

int
_kse_in_critical(void)
{
	return (_ksd_get_tmbx() == NULL);
}

void
_thr_critical_enter(struct pthread *thread)
{
	thread->critical_count++;
}

void
_thr_critical_leave(struct pthread *thread)
{
	thread->critical_count--;
	THR_YIELD_CHECK(thread);
}

void
_thr_sched_switch(struct pthread *curthread)
{
	struct kse *curkse;

	(void)_kse_critical_enter();
	curkse = _get_curkse();
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	_thr_sched_switch_unlocked(curthread);
}

/*
 * XXX - We may need to take the scheduling lock before calling
 *       this, or perhaps take the lock within here before
 *       doing anything else.
 */
void
_thr_sched_switch_unlocked(struct pthread *curthread)
{
	struct pthread *td;
	struct pthread_sigframe psf;
	struct kse *curkse;
	int ret;
	volatile int uts_once;
	volatile int resume_once = 0;

	/* We're in the scheduler, 5 by 5: */
	curkse = _get_curkse();

	curthread->need_switchout = 1;	/* The thread yielded on its own. */
	curthread->critical_yield = 0;	/* No need to yield anymore. */
	curthread->slice_usec = -1;	/* Restart the time slice. */

	/* Thread can unlock the scheduler lock. */
	curthread->lock_switch = 1;

	/*
	 * The signal frame is allocated off the stack because
	 * a thread can be interrupted by other signals while
	 * it is running down pending signals.
	 */
	sigemptyset(&psf.psf_sigset);
	curthread->curframe = &psf;

	/*
	 * Enter the scheduler if any one of the following is true:
	 *
	 *   o The current thread is dead; it's stack needs to be
	 *     cleaned up and it can't be done while operating on
	 *     it.
	 *   o There are no runnable threads.
	 *   o The next thread to run won't unlock the scheduler
	 *     lock.  A side note: the current thread may be run
	 *     instead of the next thread in the run queue, but
	 *     we don't bother checking for that.
	 */
	if ((curthread->state == PS_DEAD) ||
	    (((td = KSE_RUNQ_FIRST(curkse)) == NULL) &&
	    (curthread->state != PS_RUNNING)) ||
	    ((td != NULL) && (td->lock_switch == 0)))
		_thread_enter_uts(&curthread->tmbx, &curkse->k_mbx);
	else {
		uts_once = 0;
		THR_GETCONTEXT(&curthread->tmbx.tm_context);
		if (uts_once == 0) {
			uts_once = 1;

			/* Switchout the current thread. */
			kse_switchout_thread(curkse, curthread);

		 	/* Choose another thread to run. */
			td = KSE_RUNQ_FIRST(curkse);
			KSE_RUNQ_REMOVE(curkse, td);
			curkse->k_curthread = td;

			/*
			 * Make sure the current thread's kse points to
			 * this kse.
			 */
			td->kse = curkse;

			/*
			 * Reset accounting.
			 */
			td->tmbx.tm_uticks = 0;
			td->tmbx.tm_sticks = 0;

			/*
			 * Reset the time slice if this thread is running
			 * for the first time or running again after using
			 * its full time slice allocation.
			 */
			if (td->slice_usec == -1)
				td->slice_usec = 0;

			/* Mark the thread active. */
			td->active = 1;

			/* Remove the frame reference. */
			td->curframe = NULL;

			/*
			 * Continue the thread at its current frame:
			 */
			ret = _thread_switch(&td->tmbx, NULL);
			/* This point should not be reached. */
			if (ret != 0)
				PANIC("Bad return from _thread_switch");
			PANIC("Thread has returned from _thread_switch");
		}
	}

	if (curthread->lock_switch != 0) {
		/*
		 * Unlock the scheduling queue and leave the
		 * critical region.
		 */
		/* Don't trust this after a switch! */
		curkse = _get_curkse();

		curthread->lock_switch = 0;
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		_kse_critical_leave(&curthread->tmbx);
	}
	/*
	 * This thread is being resumed; check for cancellations.
	 */
	if ((resume_once == 0) && (!THR_IN_CRITICAL(curthread))) {
		resume_once = 1;
		thr_resume_check(curthread, &curthread->tmbx.tm_context, &psf);
	}

	THR_ACTIVATE_LAST_LOCK(curthread);
}

/*
 * This is the scheduler for a KSE which runs a scope system thread.
 * The multi-thread KSE scheduler should also work for a single threaded
 * KSE, but we use a separate scheduler so that it can be fine-tuned
 * to be more efficient (and perhaps not need a separate stack for
 * the KSE, allowing it to use the thread's stack).
 *
 * XXX - This probably needs some work.
 */
#ifdef NOT_YET
static void
kse_sched_single(struct kse *curkse)
{
	struct pthread *curthread = curkse->k_curthread;
	struct pthread *td_wait;
	struct timespec ts;
	int level;

	if (curthread->active == 0) {
		if (curthread->state != PS_RUNNING) {
			/* Check to see if the thread has timed out. */
			KSE_GET_TOD(curkse, &ts);
			if (thr_timedout(curthread, &ts) != 0) {
				curthread->timeout = 1;
				curthread->state = PS_RUNNING;
			}
		}
	}

	/* This thread no longer needs to yield the CPU: */
	curthread->critical_yield = 0;
	curthread->need_switchout = 0;

	/*
	 * Lock the scheduling queue.
	 *
	 * There is no scheduling queue for single threaded KSEs,
	 * but we need a lock for protection regardless.
	 */
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);

	/*
	 * This has to do the job of kse_switchout_thread(), only
	 * for a single threaded KSE/KSEG.
	 */

	switch (curthread->state) {
	case PS_DEAD:
		/* Unlock the scheduling queue and exit the KSE. */
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		kse_fini(curkse);	/* does not return */
		break;

	case PS_COND_WAIT:
	case PS_SLEEP_WAIT:
		/* Only insert threads that can timeout: */
		if (curthread->wakeup_time.tv_sec != -1) {
			/* Insert into the waiting queue: */
			KSE_WAITQ_INSERT(curkse, curthread);
		}
		break;

	case PS_LOCKWAIT:
		level = curthread->locklevel - 1;
		if (_LCK_BUSY(&curthread->lockusers[level]))
			KSE_WAITQ_INSERT(curkse, curthread);
		else
			THR_SET_STATE(curthread, PS_RUNNING);
		break;

	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_RUNNING:
	case PS_SIGSUSPEND:
	case PS_SIGWAIT:
	case PS_SUSPENDED:
	case PS_DEADLOCK:
	default:
		/*
		 * These states don't timeout and don't need
		 * to be in the waiting queue.
		 */
		break;
	}
	while (curthread->state != PS_RUNNING) {
		curthread->active = 0;
		td_wait = KSE_WAITQ_FIRST(curkse);

		kse_wait(curkse, td_wait);

	    	if (td_wait != NULL) {
			KSE_GET_TOD(curkse, &ts);
			if (thr_timedout(curthread, &ts)) {
				/* Indicate the thread timedout: */
				td_wait->timeout = 1;

				/* Make the thread runnable. */
				THR_SET_STATE(td_wait, PS_RUNNING);
				KSE_WAITQ_REMOVE(curkse, td_wait);
			}
		}
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		kse_check_signals(curkse);
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	}

	/* Remove the frame reference. */
	curthread->curframe = NULL;

	/* Unlock the scheduling queue. */
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);

	/*
	 * Continue the thread at its current frame:
	 */
	DBG_MSG("Continuing bound thread %p\n", curthread);
	_thread_switch(&curthread->tmbx, &curkse->k_mbx.km_curthread);
	PANIC("Thread has returned from _thread_switch");
}
#endif

#ifdef DEBUG_THREAD_KERN
static void
dump_queues(struct kse *curkse)
{
	struct pthread *thread;

	DBG_MSG("Threads in waiting queue:\n");
	TAILQ_FOREACH(thread, &curkse->k_kseg->kg_schedq.sq_waitq, pqe) {
		DBG_MSG("  thread %p, state %d, blocked %d\n",
		    thread, thread->state, thread->blocked);
	}
}
#endif

/*
 * This is the scheduler for a KSE which runs multiple threads.
 */
static void
kse_sched_multi(struct kse *curkse)
{
	struct pthread *curthread, *td_wait;
	struct pthread_sigframe *curframe;
	int ret;

	THR_ASSERT(curkse->k_mbx.km_curthread == NULL,
	    "Mailbox not null in kse_sched_multi");

	/* Check for first time initialization: */
	if ((curkse->k_flags & KF_INITIALIZED) == 0) {
		/* Setup this KSEs specific data. */
		_ksd_setprivate(&curkse->k_ksd);
		_set_curkse(curkse);

		/* Set this before grabbing the context. */
		curkse->k_flags |= KF_INITIALIZED;
	}

	/* This may have returned from a kse_release(). */
	if (KSE_WAITING(curkse)) {
		DBG_MSG("Entered upcall when KSE is waiting.");
		KSE_CLEAR_WAIT(curkse);
	}

	/* Lock the scheduling lock. */
	curthread = curkse->k_curthread;
	if ((curthread == NULL) || (curthread->need_switchout == 0)) {
		/* This is an upcall; take the scheduler lock. */
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	}

	/*
	 * If the current thread was completed in another KSE, then
	 * it will be in the run queue.  Don't mark it as being blocked.
	 */
	if ((curthread != NULL) &&
	    ((curthread->flags & THR_FLAGS_IN_RUNQ) == 0) &&
	    (curthread->need_switchout == 0)) {
		/*
		 * Assume the current thread is blocked; when the
		 * completed threads are checked and if the current
		 * thread is among the completed, the blocked flag
		 * will be cleared.
		 */
		curthread->blocked = 1;
	}

	/* Check for any unblocked threads in the kernel. */
	kse_check_completed(curkse);

	/*
	 * Check for threads that have timed-out.
	 */
	kse_check_waitq(curkse);

	/*
	 * Switchout the current thread, if necessary, as the last step
	 * so that it is inserted into the run queue (if it's runnable)
	 * _after_ any other threads that were added to it above.
	 */
	if (curthread == NULL)
		;  /* Nothing to do here. */
	else if ((curthread->need_switchout == 0) &&
	    (curthread->blocked == 0) && (THR_IN_CRITICAL(curthread))) {
		/*
		 * Resume the thread and tell it to yield when
		 * it leaves the critical region.
		 */
		curthread->critical_yield = 1;
		curthread->active = 1;
		if ((curthread->flags & THR_FLAGS_IN_RUNQ) != 0)
			KSE_RUNQ_REMOVE(curkse, curthread);
		curkse->k_curthread = curthread;
		curthread->kse = curkse;
		DBG_MSG("Continuing thread %p in critical region\n",
		    curthread);
		kse_wakeup_multi(curkse);
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		ret = _thread_switch(&curthread->tmbx,
		    &curkse->k_mbx.km_curthread);
		if (ret != 0)
			PANIC("Can't resume thread in critical region\n");
	}
	else if ((curthread->flags & THR_FLAGS_IN_RUNQ) == 0)
		kse_switchout_thread(curkse, curthread);
	curkse->k_curthread = NULL;

	kse_wakeup_multi(curkse);

	/* This has to be done without the scheduling lock held. */
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	kse_check_signals(curkse);
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);

#ifdef DEBUG_THREAD_KERN
	dump_queues(curkse);
#endif

	/* Check if there are no threads ready to run: */
	while (((curthread = KSE_RUNQ_FIRST(curkse)) == NULL) &&
	    (curkse->k_kseg->kg_threadcount != 0)) {
		/*
		 * Wait for a thread to become active or until there are
		 * no more threads.
		 */
		td_wait = KSE_WAITQ_FIRST(curkse);
		kse_wait(curkse, td_wait);
		kse_check_completed(curkse);
		kse_check_waitq(curkse);
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		kse_check_signals(curkse);
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	}

	/* Check for no more threads: */
	if (curkse->k_kseg->kg_threadcount == 0) {
		/*
		 * Normally this shouldn't return, but it will if there
		 * are other KSEs running that create new threads that
		 * are assigned to this KSE[G].  For instance, if a scope
		 * system thread were to create a scope process thread
		 * and this kse[g] is the initial kse[g], then that newly
		 * created thread would be assigned to us (the initial
		 * kse[g]).
		 */
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		kse_fini(curkse);
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);
		curthread = KSE_RUNQ_FIRST(curkse);
	}

	THR_ASSERT(curthread != NULL,
	    "Return from kse_wait/fini without thread.");
	THR_ASSERT(curthread->state != PS_DEAD,
	    "Trying to resume dead thread!");
	KSE_RUNQ_REMOVE(curkse, curthread);

	/*
	 * Make the selected thread the current thread.
	 */
	curkse->k_curthread = curthread;

	/*
	 * Make sure the current thread's kse points to this kse.
	 */
	curthread->kse = curkse;

	/*
	 * Reset accounting.
	 */
	curthread->tmbx.tm_uticks = 0;
	curthread->tmbx.tm_sticks = 0;

	/*
	 * Reset the time slice if this thread is running for the first
	 * time or running again after using its full time slice allocation.
	 */
	if (curthread->slice_usec == -1)
		curthread->slice_usec = 0;

	/* Mark the thread active. */
	curthread->active = 1;

	/* Remove the frame reference. */
	curframe = curthread->curframe;
	curthread->curframe = NULL;

	kse_wakeup_multi(curkse);

	/*
	 * The thread's current signal frame will only be NULL if it
	 * is being resumed after being blocked in the kernel.  In
	 * this case, and if the thread needs to run down pending
	 * signals or needs a cancellation check, we need to add a
	 * signal frame to the thread's context.
	 */
#ifdef NOT_YET
	if ((curframe == NULL) && ((curthread->have_signals != 0) ||
	    (((curthread->cancelflags & THR_AT_CANCEL_POINT) == 0) &&
	    ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))))
		signalcontext(&curthread->tmbx.tm_context, 0,
		    (__sighandler_t *)thr_resume_wrapper);
#else
	if ((curframe == NULL) && (curthread->have_signals != 0))
		signalcontext(&curthread->tmbx.tm_context, 0,
		    (__sighandler_t *)thr_resume_wrapper);
#endif
	/*
	 * Continue the thread at its current frame:
	 */
	if (curthread->lock_switch != 0) {
		/*
		 * This thread came from a scheduler switch; it will
		 * unlock the scheduler lock and set the mailbox.
		 */
		ret = _thread_switch(&curthread->tmbx, NULL);
	} else {
		/* This thread won't unlock the scheduler lock. */
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		ret = _thread_switch(&curthread->tmbx,
		    &curkse->k_mbx.km_curthread);
	}
	if (ret != 0)
		PANIC("Thread has returned from _thread_switch");

	/* This point should not be reached. */
	PANIC("Thread has returned from _thread_switch");
}

static void
kse_check_signals(struct kse *curkse)
{
	sigset_t sigset;
	int i;

	/* Deliver posted signals. */
	for (i = 0; i < _SIG_WORDS; i++) {
		atomic_swap_int(&curkse->k_mbx.km_sigscaught.__bits[i],
		    0, &sigset.__bits[i]);
	}
	if (SIGNOTEMPTY(sigset)) {
		/*
		 * Dispatch each signal.
		 *
		 * XXX - There is no siginfo for any of these.
		 *       I think there should be, especially for
		 *       signals from other processes (si_pid, si_uid).
		 */
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sigset, i) != 0) {
				DBG_MSG("Dispatching signal %d\n", i);
				_thr_sig_dispatch(curkse, i,
				    NULL /* no siginfo */);
			}
		}
		sigemptyset(&sigset);
		__sys_sigprocmask(SIG_SETMASK, &sigset, NULL);
	}
}

static void
thr_resume_wrapper(int unused_1, siginfo_t *unused_2, ucontext_t *ucp)
{
	struct pthread *curthread = _get_curthread();

	thr_resume_check(curthread, ucp, NULL);
}

static void
thr_resume_check(struct pthread *curthread, ucontext_t *ucp,
    struct pthread_sigframe *psf)
{
	/* Check signals before cancellations. */
	while (curthread->have_signals != 0) {
		/* Clear the pending flag. */
		curthread->have_signals = 0;

		/*
		 * It's perfectly valid, though not portable, for
		 * signal handlers to munge their interrupted context
		 * and expect to return to it.  Ensure we use the
		 * correct context when running down signals.
		 */
		_thr_sig_rundown(curthread, ucp, psf);
	}

#ifdef NOT_YET
	if (((curthread->cancelflags & THR_AT_CANCEL_POINT) == 0) &&
	    ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))
		pthread_testcancel();
#endif
}

/*
 * Clean up a thread.  This must be called with the thread's KSE
 * scheduling lock held.  The thread must be a thread from the
 * KSE's group.
 */
static void
thr_cleanup(struct kse *curkse, struct pthread *thread)
{
	struct pthread *joiner;

	if ((joiner = thread->joiner) != NULL) {
		thread->joiner = NULL;
		if ((joiner->state == PS_JOIN) &&
		    (joiner->join_status.thread == thread)) {
			joiner->join_status.thread = NULL;

			/* Set the return status for the joining thread: */
			joiner->join_status.ret = thread->ret;

			/* Make the thread runnable. */
			if (joiner->kseg == curkse->k_kseg)
				_thr_setrunnable_unlocked(joiner);
			else {
				KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
				KSE_SCHED_LOCK(curkse, joiner->kseg);
				_thr_setrunnable_unlocked(joiner);
				KSE_SCHED_UNLOCK(curkse, joiner->kseg);
				KSE_SCHED_LOCK(curkse, curkse->k_kseg);
			}
		}
		thread->attr.flags |= PTHREAD_DETACHED;
	}

	if ((thread->attr.flags & PTHREAD_SCOPE_PROCESS) == 0) {
		/*
		 * Remove the thread from the KSEG's list of threads.
	 	 */
		KSEG_THRQ_REMOVE(thread->kseg, thread);
		/*
		 * Migrate the thread to the main KSE so that this
		 * KSE and KSEG can be cleaned when their last thread
		 * exits.
		 */
		thread->kseg = _kse_initial->k_kseg;
		thread->kse = _kse_initial;
	}
	thread->flags |= THR_FLAGS_GC_SAFE;

	/*
	 * We can't hold the thread list lock while holding the
	 * scheduler lock.
	 */
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	DBG_MSG("Adding thread %p to GC list\n", thread);
	KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
	THR_GCLIST_ADD(thread);
	KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
}

void
_thr_gc(struct pthread *curthread)
{
	struct pthread *td, *td_next;
	kse_critical_t crit;
	TAILQ_HEAD(, pthread) worklist;

	TAILQ_INIT(&worklist);
	crit = _kse_critical_enter();
	KSE_LOCK_ACQUIRE(curthread->kse, &_thread_list_lock);

	/* Check the threads waiting for GC. */
	for (td = TAILQ_FIRST(&_thread_gc_list); td != NULL; td = td_next) {
		td_next = TAILQ_NEXT(td, gcle);
		if ((td->flags & THR_FLAGS_GC_SAFE) == 0)
			continue;
#ifdef NOT_YET
		else if (((td->attr.flags & PTHREAD_SCOPE_PROCESS) != 0) &&
		    (td->kse->k_mbx.km_flags == 0)) {
			/*
			 * The thread and KSE are operating on the same
			 * stack.  Wait for the KSE to exit before freeing
			 * the thread's stack as well as everything else.
			 */
			continue;
		}
#endif
		/*
		 * Remove the thread from the GC list.  If the thread
		 * isn't yet detached, it will get added back to the
		 * GC list at a later time.
		 */
		THR_GCLIST_REMOVE(td);
		DBG_MSG("Freeing thread %p stack\n", td);
		/*
		 * We can free the thread stack since it's no longer
		 * in use.
		 */
		_thr_stack_free(&td->attr);
		if (((td->attr.flags & PTHREAD_DETACHED) != 0) &&
		    (td->refcount == 0)) {
			/*
			 * The thread has detached and is no longer
			 * referenced.  It is safe to remove all
			 * remnants of the thread.
			 */
			THR_LIST_REMOVE(td);
			TAILQ_INSERT_HEAD(&worklist, td, gcle);
		}
	}
	KSE_LOCK_RELEASE(curthread->kse, &_thread_list_lock);
	_kse_critical_leave(crit);

	while ((td = TAILQ_FIRST(&worklist)) != NULL) {
		TAILQ_REMOVE(&worklist, td, gcle);

		if ((td->attr.flags & PTHREAD_SCOPE_PROCESS) != 0) {
			crit = _kse_critical_enter();
			KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
			kse_free_unlocked(td->kse);
			kseg_free_unlocked(td->kseg);
			KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
			_kse_critical_leave(crit);
		}
		DBG_MSG("Freeing thread %p\n", td);
		_thr_free(curthread, td);
	}
}


/*
 * Only new threads that are running or suspended may be scheduled.
 */
int
_thr_schedule_add(struct pthread *curthread, struct pthread *newthread)
{
	struct kse *curkse;
	kse_critical_t crit;
	int need_start;
	int ret;

	/*
	 * If this is the first time creating a thread, make sure
	 * the mailbox is set for the current thread.
	 */
	if ((newthread->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) {
#ifdef NOT_YET
		/* We use the thread's stack as the KSE's stack. */
		new_thread->kse->k_mbx.km_stack.ss_sp =
		    new_thread->attr.stackaddr_attr;
		new_thread->kse->k_mbx.km_stack.ss_size =
		    new_thread->attr.stacksize_attr;
#endif
		/*
		 * No need to lock the scheduling queue since the
		 * KSE/KSEG pair have not yet been started.
		 */
		KSEG_THRQ_ADD(newthread->kseg, newthread);
		TAILQ_INSERT_TAIL(&newthread->kseg->kg_kseq, newthread->kse,
		    k_kgqe);
		newthread->kseg->kg_ksecount = 1;
		if (newthread->state == PS_RUNNING)
			THR_RUNQ_INSERT_TAIL(newthread);
		newthread->kse->k_curthread = NULL;
		newthread->kse->k_mbx.km_flags = 0;
		newthread->kse->k_mbx.km_func = (kse_func_t *)kse_sched_multi;
		newthread->kse->k_mbx.km_quantum = 0;

		/*
		 * This thread needs a new KSE and KSEG.
		 */
		crit = _kse_critical_enter();
		curkse = _get_curkse();
		_ksd_setprivate(&newthread->kse->k_ksd);
		newthread->kse->k_flags |= KF_INITIALIZED;
		ret = kse_create(&newthread->kse->k_mbx, 1);
		if (ret != 0)
			ret = errno;
		_ksd_setprivate(&curkse->k_ksd);
		_kse_critical_leave(crit);
	}
	else {
		/*
		 * Lock the KSE and add the new thread to its list of
		 * assigned threads.  If the new thread is runnable, also
		 * add it to the KSE's run queue.
		 */
		need_start = 0;
		KSE_SCHED_LOCK(curthread->kse, newthread->kseg);
		KSEG_THRQ_ADD(newthread->kseg, newthread);
		if (newthread->state == PS_RUNNING)
			THR_RUNQ_INSERT_TAIL(newthread);
		if ((newthread->kse->k_flags & KF_STARTED) == 0) {
			/*
			 * This KSE hasn't been started yet.  Start it
			 * outside of holding the lock.
			 */
			newthread->kse->k_flags |= KF_STARTED;
			newthread->kse->k_mbx.km_func =
			    (kse_func_t *)kse_sched_multi;
			newthread->kse->k_mbx.km_flags = 0;
			need_start = 1;
		}
		KSE_SCHED_UNLOCK(curthread->kse, newthread->kseg);

	  	if (need_start != 0)
			kse_create(&newthread->kse->k_mbx, 0);
		else if ((newthread->state == PS_RUNNING) &&
		    KSE_IS_IDLE(newthread->kse)) {
			/*
			 * The thread is being scheduled on another KSEG.
			 */
			kse_wakeup_one(newthread);
		}
		ret = 0;
	}
	return (ret);
}

void
kse_waitq_insert(struct pthread *thread)
{
	struct pthread *td;

	if (thread->wakeup_time.tv_sec == -1)
		TAILQ_INSERT_TAIL(&thread->kse->k_schedq->sq_waitq, thread,
		    pqe);
	else {
		td = TAILQ_FIRST(&thread->kse->k_schedq->sq_waitq);
		while ((td != NULL) && (td->wakeup_time.tv_sec != -1) &&
		    ((td->wakeup_time.tv_sec < thread->wakeup_time.tv_sec) ||
		    ((td->wakeup_time.tv_sec == thread->wakeup_time.tv_sec) &&
		    (td->wakeup_time.tv_nsec <= thread->wakeup_time.tv_nsec))))
			td = TAILQ_NEXT(td, pqe);
		if (td == NULL)
			TAILQ_INSERT_TAIL(&thread->kse->k_schedq->sq_waitq,
			    thread, pqe);
		else
			TAILQ_INSERT_BEFORE(td, thread, pqe);
	}
	thread->flags |= THR_FLAGS_IN_WAITQ;
}

/*
 * This must be called with the scheduling lock held.
 */
static void
kse_check_completed(struct kse *kse)
{
	struct pthread *thread;
	struct kse_thr_mailbox *completed;

	if ((completed = kse->k_mbx.km_completed) != NULL) {
		kse->k_mbx.km_completed = NULL;
		while (completed != NULL) {
			thread = completed->tm_udata;
			DBG_MSG("Found completed thread %p, name %s\n",
			    thread,
			    (thread->name == NULL) ? "none" : thread->name);
			thread->blocked = 0;
			if (thread != kse->k_curthread) {
				if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
					THR_SET_STATE(thread, PS_SUSPENDED);
				else
					KSE_RUNQ_INSERT_TAIL(kse, thread);
				if ((thread->kse != kse) &&
				    (thread->kse->k_curthread == thread)) {
					thread->kse->k_curthread = NULL;
					thread->active = 0;
				}
			}
			completed = completed->tm_next;
		}
	}
}

/*
 * This must be called with the scheduling lock held.
 */
static void
kse_check_waitq(struct kse *kse)
{
	struct pthread	*pthread;
	struct timespec ts;

	KSE_GET_TOD(kse, &ts);

	/*
	 * Wake up threads that have timedout.  This has to be
	 * done before adding the current thread to the run queue
	 * so that a CPU intensive thread doesn't get preference
	 * over waiting threads.
	 */
	while (((pthread = KSE_WAITQ_FIRST(kse)) != NULL) &&
	    thr_timedout(pthread, &ts)) {
		/* Remove the thread from the wait queue: */
		KSE_WAITQ_REMOVE(kse, pthread);
		DBG_MSG("Found timedout thread %p in waitq\n", pthread);

		/* Indicate the thread timedout: */
		pthread->timeout = 1;

		/* Add the thread to the priority queue: */
		if ((pthread->flags & THR_FLAGS_SUSPENDED) != 0)
			THR_SET_STATE(pthread, PS_SUSPENDED);
		else {
			THR_SET_STATE(pthread, PS_RUNNING);
			KSE_RUNQ_INSERT_TAIL(kse, pthread);
		}
	}
}

static int
thr_timedout(struct pthread *thread, struct timespec *curtime)
{
	if (thread->wakeup_time.tv_sec < 0)
		return (0);
	else if (thread->wakeup_time.tv_sec > curtime->tv_sec)
		return (0);
	else if ((thread->wakeup_time.tv_sec == curtime->tv_sec) &&
	    (thread->wakeup_time.tv_nsec > curtime->tv_nsec))
		return (0);
	else
		return (1);
}

/*
 * This must be called with the scheduling lock held.
 *
 * Each thread has a time slice, a wakeup time (used when it wants
 * to wait for a specified amount of time), a run state, and an
 * active flag.
 *
 * When a thread gets run by the scheduler, the active flag is
 * set to non-zero (1).  When a thread performs an explicit yield
 * or schedules a state change, it enters the scheduler and the
 * active flag is cleared.  When the active flag is still seen
 * set in the scheduler, that means that the thread is blocked in
 * the kernel (because it is cleared before entering the scheduler
 * in all other instances).
 *
 * The wakeup time is only set for those states that can timeout.
 * It is set to (-1, -1) for all other instances.
 *
 * The thread's run state, aside from being useful when debugging,
 * is used to place the thread in an appropriate queue.  There
 * are 2 basic queues:
 *
 *   o run queue - queue ordered by priority for all threads
 *                 that are runnable
 *   o waiting queue - queue sorted by wakeup time for all threads
 *                     that are not otherwise runnable (not blocked
 *                     in kernel, not waiting for locks)
 *
 * The thread's time slice is used for round-robin scheduling
 * (the default scheduling policy).  While a SCHED_RR thread
 * is runnable it's time slice accumulates.  When it reaches
 * the time slice interval, it gets reset and added to the end
 * of the queue of threads at its priority.  When a thread no
 * longer becomes runnable (blocks in kernel, waits, etc), its
 * time slice is reset.
 *
 * The job of kse_switchout_thread() is to handle all of the above.
 */
static void
kse_switchout_thread(struct kse *kse, struct pthread *thread)
{
	int level;
	int i;

	/*
	 * Place the currently running thread into the
	 * appropriate queue(s).
	 */
	DBG_MSG("Switching out thread %p, state %d\n", thread, thread->state);

	THR_DEACTIVATE_LAST_LOCK(thread);
	if (thread->blocked != 0) {
		thread->active = 0;
		thread->need_switchout = 0;
		/* This thread must have blocked in the kernel. */
		/* thread->slice_usec = -1;*/	/* restart timeslice */
		/*
		 * XXX - Check for pending signals for this thread to
		 *       see if we need to interrupt it in the kernel.
		 */
		/* if (thread->check_pending != 0) */
		if ((thread->slice_usec != -1) &&
		    (thread->attr.sched_policy != SCHED_FIFO))
			thread->slice_usec += (thread->tmbx.tm_uticks
			    + thread->tmbx.tm_sticks) * _clock_res_usec;
	}
	else {
		switch (thread->state) {
		case PS_DEAD:
			/*
			 * The scheduler is operating on a different
			 * stack.  It is safe to do garbage collecting
			 * here.
			 */
			thread->active = 0;
			thread->need_switchout = 0;
			thr_cleanup(kse, thread);
			return;
			break;

		case PS_RUNNING:
			if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
				THR_SET_STATE(thread, PS_SUSPENDED);
			break;

		case PS_COND_WAIT:
		case PS_SLEEP_WAIT:
			/* Insert into the waiting queue: */
			KSE_WAITQ_INSERT(kse, thread);
			break;

		case PS_LOCKWAIT:
			/*
			 * This state doesn't timeout.
			 */
			thread->wakeup_time.tv_sec = -1;
			thread->wakeup_time.tv_nsec = -1;
			level = thread->locklevel - 1;
			if (_LCK_BUSY(&thread->lockusers[level]))
				KSE_WAITQ_INSERT(kse, thread);
			else
				THR_SET_STATE(thread, PS_RUNNING);
			break;

		case PS_JOIN:
		case PS_MUTEX_WAIT:
		case PS_SIGSUSPEND:
		case PS_SIGWAIT:
		case PS_SUSPENDED:
		case PS_DEADLOCK:
		default:
			/*
			 * These states don't timeout.
			 */
			thread->wakeup_time.tv_sec = -1;
			thread->wakeup_time.tv_nsec = -1;

			/* Insert into the waiting queue: */
			KSE_WAITQ_INSERT(kse, thread);
			break;
		}
		if (thread->state != PS_RUNNING) {
			/* Restart the time slice: */
			thread->slice_usec = -1;
		} else {
			if (thread->need_switchout != 0)
				/*
				 * The thread yielded on its own;
				 * restart the timeslice.
				 */
				thread->slice_usec = -1;
			else if ((thread->slice_usec != -1) &&
	   		    (thread->attr.sched_policy != SCHED_FIFO)) {
				thread->slice_usec += (thread->tmbx.tm_uticks
				    + thread->tmbx.tm_sticks) * _clock_res_usec;
				/* Check for time quantum exceeded: */
				if (thread->slice_usec > TIMESLICE_USEC)
					thread->slice_usec = -1;
			}
			if (thread->slice_usec == -1) {
				/*
				 * The thread exceeded its time quantum or
				 * it yielded the CPU; place it at the tail
				 * of the queue for its priority.
				 */
				KSE_RUNQ_INSERT_TAIL(kse, thread);
			} else {
				/*
				 * The thread hasn't exceeded its interval
				 * Place it at the head of the queue for its
				 * priority.
				 */
				KSE_RUNQ_INSERT_HEAD(kse, thread);
			}
		}
	}
	thread->active = 0;
	thread->need_switchout = 0;
	if (thread->check_pending != 0) {
		/* Install pending signals into the frame. */
		thread->check_pending = 0;
		for (i = 0; i < _SIG_MAXSIG; i++) {
			if (sigismember(&thread->sigpend, i) &&
			    !sigismember(&thread->tmbx.tm_context.uc_sigmask, i))
				_thr_sig_add(thread, i, &thread->siginfo[i]);
		}
	}
}

/*
 * This function waits for the smallest timeout value of any waiting
 * thread, or until it receives a message from another KSE.
 *
 * This must be called with the scheduling lock held.
 */
static void
kse_wait(struct kse *kse, struct pthread *td_wait)
{
	struct timespec ts, ts_sleep;
	int saved_flags;

	KSE_GET_TOD(kse, &ts);

	if ((td_wait == NULL) || (td_wait->wakeup_time.tv_sec < 0)) {
		/* Limit sleep to no more than 1 minute. */
		ts_sleep.tv_sec = 60;
		ts_sleep.tv_nsec = 0;
	} else {
		TIMESPEC_SUB(&ts_sleep, &td_wait->wakeup_time, &ts);
		if (ts_sleep.tv_sec > 60) {
			ts_sleep.tv_sec = 60;
			ts_sleep.tv_nsec = 0;
		}
	}
	/* Don't sleep for negative times. */
	if ((ts_sleep.tv_sec >= 0) && (ts_sleep.tv_nsec >= 0)) {
		KSE_SET_IDLE(kse);
		kse->k_kseg->kg_idle_kses++;
		KSE_SCHED_UNLOCK(kse, kse->k_kseg);
		saved_flags = kse->k_mbx.km_flags;
		kse->k_mbx.km_flags |= KMF_NOUPCALL;
		kse_release(&ts_sleep);
		kse->k_mbx.km_flags = saved_flags;
		KSE_SCHED_LOCK(kse, kse->k_kseg);
		if (KSE_IS_IDLE(kse)) {
			KSE_CLEAR_IDLE(kse);
			kse->k_kseg->kg_idle_kses--;
		}
	}
}

/*
 * Avoid calling this kse_exit() so as not to confuse it with the
 * system call of the same name.
 */
static void
kse_fini(struct kse *kse)
{
	struct timespec ts;
	struct kse_group *free_kseg = NULL;

	if ((kse->k_kseg->kg_flags & KGF_SINGLE_THREAD) != 0)
		kse_exit();
	/*
	 * Check to see if this is one of the main kses.
	 */
	else if (kse->k_kseg != _kse_initial->k_kseg) {
		/* Remove this KSE from the KSEG's list of KSEs. */
		KSE_SCHED_LOCK(kse, kse->k_kseg);
		TAILQ_REMOVE(&kse->k_kseg->kg_kseq, kse, k_kgqe);
		kse->k_kseg->kg_ksecount--;
		if (TAILQ_EMPTY(&kse->k_kseg->kg_kseq))
			free_kseg = kse->k_kseg;
		KSE_SCHED_UNLOCK(kse, kse->k_kseg);

		/*
		 * Add this KSE to the list of free KSEs along with
		 * the KSEG if is now orphaned.
		 */
#ifdef NOT_YET
		KSE_LOCK_ACQUIRE(kse, &kse_lock);
		if (free_kseg != NULL)
			kseg_free_unlocked(free_kseg);
		kse_free_unlocked(kse);
		KSE_LOCK_RELEASE(kse, &kse_lock);
#endif
		kse_exit();
		/* Never returns. */
	} else {
		/*
		 * Wait for the last KSE/thread to exit, or for more
		 * threads to be created (it is possible for additional
		 * scope process threads to be created after the main
		 * thread exits).
		 */
		ts.tv_sec = 120;
		ts.tv_nsec = 0;
		KSE_SET_WAIT(kse);
		KSE_SCHED_LOCK(kse, kse->k_kseg);
		if ((active_kse_count > 1) &&
		    (kse->k_kseg->kg_threadcount == 0)) {
			KSE_SCHED_UNLOCK(kse, kse->k_kseg);
			kse_release(&ts);
			/* The above never returns. */
		}
		else
			KSE_SCHED_UNLOCK(kse, kse->k_kseg);

		/* There are no more threads; exit this process: */
		if (kse->k_kseg->kg_threadcount == 0) {
			/* kse_exit(); */
			__isthreaded = 0;
			exit(0);
		}
	}
}

void
_thr_set_timeout(const struct timespec *timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts;

	/* Reset the timeout flag for the running thread: */
	curthread->timeout = 0;

	/* Check if the thread is to wait forever: */
	if (timeout == NULL) {
		/*
		 * Set the wakeup time to something that can be recognised as
		 * different to an actual time of day:
		 */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;
	}
	/* Check if no waiting is required: */
	else if ((timeout->tv_sec == 0) && (timeout->tv_nsec == 0)) {
		/* Set the wake up time to 'immediately': */
		curthread->wakeup_time.tv_sec = 0;
		curthread->wakeup_time.tv_nsec = 0;
	} else {
		/* Calculate the time for the current thread to wakeup: */
		KSE_GET_TOD(curthread->kse, &ts);
		TIMESPEC_ADD(&curthread->wakeup_time, &ts, timeout);
	}
}

void
_thr_panic_exit(char *file, int line, char *msg)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "(%s:%d) %s\n", file, line, msg);
	__sys_write(2, buf, strlen(buf));
	abort();
}

void
_thr_setrunnable(struct pthread *curthread, struct pthread *thread)
{
	kse_critical_t crit;

	crit = _kse_critical_enter();
	KSE_SCHED_LOCK(curthread->kse, thread->kseg);
	_thr_setrunnable_unlocked(thread);
	KSE_SCHED_UNLOCK(curthread->kse, thread->kseg);
	_kse_critical_leave(crit);
}

void
_thr_setrunnable_unlocked(struct pthread *thread)
{
	if ((thread->kseg->kg_flags & KGF_SINGLE_THREAD) != 0) {
		/* No silly queues for these threads. */
		if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
			THR_SET_STATE(thread, PS_SUSPENDED);
		else
			THR_SET_STATE(thread, PS_RUNNING);
	} else if (thread->state != PS_RUNNING) {
		if ((thread->flags & THR_FLAGS_IN_WAITQ) != 0)
			KSE_WAITQ_REMOVE(thread->kse, thread);
		if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
			THR_SET_STATE(thread, PS_SUSPENDED);
		else {
			THR_SET_STATE(thread, PS_RUNNING);
			if ((thread->blocked == 0) && (thread->active == 0) &&
			    (thread->flags & THR_FLAGS_IN_RUNQ) == 0)
				THR_RUNQ_INSERT_TAIL(thread);
		}
	}
        /*
         * XXX - Threads are not yet assigned to specific KSEs; they are
         *       assigned to the KSEG.  So the fact that a thread's KSE is
         *       waiting doesn't necessarily mean that it will be the KSE
         *       that runs the thread after the lock is granted.  But we
         *       don't know if the other KSEs within the same KSEG are
         *       also in a waiting state or not so we err on the side of
         *       caution and wakeup the thread's last known KSE.  We
         *       ensure that the threads KSE doesn't change while it's
         *       scheduling lock is held so it is safe to reference it
         *       (the KSE).  If the KSE wakes up and doesn't find any more
         *       work it will again go back to waiting so no harm is done.
         */
	kse_wakeup_one(thread);
}

static void
kse_wakeup_one(struct pthread *thread)
{
	struct kse *ke;

	if (KSE_IS_IDLE(thread->kse)) {
		KSE_CLEAR_IDLE(thread->kse);
		thread->kseg->kg_idle_kses--;
		KSE_WAKEUP(thread->kse);
	} else {
		TAILQ_FOREACH(ke, &thread->kseg->kg_kseq, k_kgqe) {
			if (KSE_IS_IDLE(ke)) {
				KSE_CLEAR_IDLE(ke);
				ke->k_kseg->kg_idle_kses--;
				KSE_WAKEUP(ke);
				return;
			}
		}
	}
}

static void
kse_wakeup_multi(struct kse *curkse)
{
	struct kse *ke;
	int tmp;

	if ((tmp = KSE_RUNQ_THREADS(curkse)) && curkse->k_kseg->kg_idle_kses) {
		TAILQ_FOREACH(ke, &curkse->k_kseg->kg_kseq, k_kgqe) {
			if (KSE_IS_IDLE(ke)) {
				KSE_CLEAR_IDLE(ke);
				ke->k_kseg->kg_idle_kses--;
				KSE_WAKEUP(ke);
				if (--tmp == 0)
					break;
			}
		}
	}
}

struct pthread *
_get_curthread(void)
{
	return (_ksd_curthread);
}

/* This assumes the caller has disabled upcalls. */
struct kse *
_get_curkse(void)
{
	return (_ksd_curkse);
}

void
_set_curkse(struct kse *kse)
{
	_ksd_setprivate(&kse->k_ksd);
}

/*
 * Allocate a new KSEG.
 *
 * We allow the current thread to be NULL in the case that this
 * is the first time a KSEG is being created (library initialization).
 * In this case, we don't need to (and can't) take any locks.
 */
struct kse_group *
_kseg_alloc(struct pthread *curthread)
{
	struct kse_group *kseg = NULL;
	kse_critical_t crit;

	if ((curthread != NULL) && (free_kseg_count > 0)) {
		/* Use the kse lock for the kseg queue. */
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		if ((kseg = TAILQ_FIRST(&free_kse_groupq)) != NULL) {
			TAILQ_REMOVE(&free_kse_groupq, kseg, kg_qe);
			free_kseg_count--;
			active_kseg_count++;
			TAILQ_INSERT_TAIL(&active_kse_groupq, kseg, kg_qe);
		}
		KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
		_kse_critical_leave(crit);
		if (kseg)
			kseg_reinit(kseg);
	}

	/*
	 * If requested, attempt to allocate a new KSE group only if the
	 * KSE allocation was successful and a KSE group wasn't found in
	 * the free list.
	 */
	if ((kseg == NULL) &&
	    ((kseg = (struct kse_group *)malloc(sizeof(*kseg))) != NULL)) {
		if (_pq_alloc(&kseg->kg_schedq.sq_runq,
		    THR_MIN_PRIORITY, THR_LAST_PRIORITY) != 0) {
			free(kseg);
			kseg = NULL;
		} else {
			kseg_init(kseg);
			/* Add the KSEG to the list of active KSEGs. */
			if (curthread != NULL) {
				crit = _kse_critical_enter();
				KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
				active_kseg_count++;
				TAILQ_INSERT_TAIL(&active_kse_groupq,
				    kseg, kg_qe);
				KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
				_kse_critical_leave(crit);
			} else {
				active_kseg_count++;
				TAILQ_INSERT_TAIL(&active_kse_groupq,
				    kseg, kg_qe);
			}
		}
	}
	return (kseg);
}

/*
 * This must be called with the kse lock held and when there are
 * no more threads that reference it.
 */
static void
kseg_free_unlocked(struct kse_group *kseg)
{
	TAILQ_REMOVE(&active_kse_groupq, kseg, kg_qe);
	TAILQ_INSERT_HEAD(&free_kse_groupq, kseg, kg_qe);
	free_kseg_count++;
	active_kseg_count--;
}

void
_kseg_free(struct kse_group *kseg)
{
	struct kse *curkse;
	kse_critical_t crit;

	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_LOCK_ACQUIRE(curkse, &kse_lock);
	kseg_free_unlocked(kseg);
	KSE_LOCK_RELEASE(curkse, &kse_lock);
	_kse_critical_leave(crit);
}

/*
 * Allocate a new KSE.
 *
 * We allow the current thread to be NULL in the case that this
 * is the first time a KSE is being created (library initialization).
 * In this case, we don't need to (and can't) take any locks.
 */
struct kse *
_kse_alloc(struct pthread *curthread)
{
	struct kse *kse = NULL;
	kse_critical_t crit;
	int need_ksd = 0;
	int i;

	if ((curthread != NULL) && (free_kse_count > 0)) {
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		/* Search for a finished KSE. */
		kse = TAILQ_FIRST(&free_kseq);
#ifdef NOT_YET
#define KEMBX_DONE	0x04
		while ((kse != NULL) &&
		    ((kse->k_mbx.km_flags & KEMBX_DONE) == 0)) {
			kse = TAILQ_NEXT(kse, k_qe);
		}
#undef KEMBX_DONE
#endif
		if (kse != NULL) {
			TAILQ_REMOVE(&free_kseq, kse, k_qe);
			free_kse_count--;
			TAILQ_INSERT_TAIL(&active_kseq, kse, k_qe);
			active_kse_count++;
		}
		KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
		_kse_critical_leave(crit);
		if (kse != NULL)
			kse_reinit(kse);
	}
	if ((kse == NULL) &&
	    ((kse = (struct kse *)malloc(sizeof(*kse))) != NULL)) {
		bzero(kse, sizeof(*kse));

		/* Initialize the lockusers. */
		for (i = 0; i < MAX_KSE_LOCKLEVEL; i++) {
			_lockuser_init(&kse->k_lockusers[i], (void *)kse);
			_LCK_SET_PRIVATE2(&kse->k_lockusers[i], NULL);
		}
		/* _lock_init(kse->k_lock, ...) */

		/* We had to malloc a kse; mark it as needing a new ID.*/
		need_ksd = 1;

		/*
		 * Create the KSE context.
		 *
		 * XXX - For now this is done here in the allocation.
		 *       In the future, we may want to have it done
		 *       outside the allocation so that scope system
		 *       threads (one thread per KSE) are not required
		 *       to have a stack for an unneeded kse upcall.
		 */
		kse->k_mbx.km_func = (kse_func_t *)kse_sched_multi;
		kse->k_mbx.km_stack.ss_sp = (char *)malloc(KSE_STACKSIZE);
		kse->k_mbx.km_stack.ss_size = KSE_STACKSIZE;
		kse->k_mbx.km_udata = (void *)kse;
		kse->k_mbx.km_quantum = 20000;
		/*
		 * We need to keep a copy of the stack in case it
		 * doesn't get used; a KSE running a scope system
		 * thread will use that thread's stack.
		 */
		kse->k_stack.ss_sp = kse->k_mbx.km_stack.ss_sp;
		kse->k_stack.ss_size = kse->k_mbx.km_stack.ss_size;
		if (kse->k_mbx.km_stack.ss_sp == NULL) {
			for (i = 0; i < MAX_KSE_LOCKLEVEL; i++) {
				_lockuser_destroy(&kse->k_lockusers[i]);
			}
			/* _lock_destroy(&kse->k_lock); */
			free(kse);
			kse = NULL;
		}
	}
	if ((kse != NULL) && (need_ksd != 0)) {
		/* This KSE needs initialization. */
		if (curthread != NULL) {
			crit = _kse_critical_enter();
			KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		}
		/* Initialize KSD inside of the lock. */
		if (_ksd_create(&kse->k_ksd, (void *)kse, sizeof(*kse)) != 0) {
			if (curthread != NULL) {
				KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
				_kse_critical_leave(crit);
			}
			free(kse->k_mbx.km_stack.ss_sp);
			for (i = 0; i < MAX_KSE_LOCKLEVEL; i++) {
				_lockuser_destroy(&kse->k_lockusers[i]);
			}
			free(kse);
			return (NULL);
		}
		kse->k_flags = 0;
		TAILQ_INSERT_TAIL(&active_kseq, kse, k_qe);
		active_kse_count++;
		if (curthread != NULL) {
			KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
			_kse_critical_leave(crit);
		}
	}
	return (kse);
}

static void
kse_reinit(struct kse *kse)
{
	bzero(&kse->k_mbx, sizeof(struct kse_mailbox));
	kse->k_curthread = 0;
	kse->k_kseg = 0;
	kse->k_schedq = 0;
	kse->k_locklevel = 0;
	sigemptyset(&kse->k_sigmask);
	bzero(&kse->k_sigq, sizeof(kse->k_sigq));
	kse->k_check_sigq = 0;
	kse->k_flags = 0;
	kse->k_waiting = 0;
	kse->k_error = 0;
	kse->k_cpu = 0;
	kse->k_done = 0;
}

void
kse_free_unlocked(struct kse *kse)
{
	TAILQ_REMOVE(&active_kseq, kse, k_qe);
	active_kse_count--;
	kse->k_kseg = NULL;
	kse->k_mbx.km_quantum = 20000;
	kse->k_flags &= ~KF_INITIALIZED;
	TAILQ_INSERT_HEAD(&free_kseq, kse, k_qe);
	free_kse_count++;
}

void
_kse_free(struct pthread *curthread, struct kse *kse)
{
	kse_critical_t crit;

	if (curthread == NULL)
		kse_free_unlocked(kse);
	else {
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		kse_free_unlocked(kse);
		KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
		_kse_critical_leave(crit);
	}
}

static void
kseg_init(struct kse_group *kseg)
{
	kseg_reinit(kseg);
	_lock_init(&kseg->kg_lock, LCK_ADAPTIVE, _kse_lock_wait,
	    _kse_lock_wakeup);
}

static void
kseg_reinit(struct kse_group *kseg)
{
	TAILQ_INIT(&kseg->kg_kseq);
	TAILQ_INIT(&kseg->kg_threadq);
	TAILQ_INIT(&kseg->kg_schedq.sq_waitq);
	kseg->kg_threadcount = 0;
	kseg->kg_ksecount = 0;
	kseg->kg_idle_kses = 0;
	kseg->kg_flags = 0;
}

struct pthread *
_thr_alloc(struct pthread *curthread)
{
	kse_critical_t crit;
	void *p;
	struct pthread *thread = NULL;

	if (curthread != NULL) {
		if (GC_NEEDED())
			_thr_gc(curthread);
		if (free_thread_count > 0) {
			crit = _kse_critical_enter();
			KSE_LOCK_ACQUIRE(curthread->kse, &thread_lock);
			if ((thread = TAILQ_FIRST(&free_threadq)) != NULL) {
				TAILQ_REMOVE(&free_threadq, thread, tle);
				free_thread_count--;
			}
			KSE_LOCK_RELEASE(curthread->kse, &thread_lock);
			_kse_critical_leave(crit);
		}
	}
	if (thread == NULL) {
		p = malloc(sizeof(struct pthread) + THR_ALIGNBYTES);
		if (p != NULL) {
			thread = (struct pthread *)THR_ALIGN(p);
			thread->alloc_addr = p;
		}
	}
	return (thread);
}

void
_thr_free(struct pthread *curthread, struct pthread *thread)
{
	kse_critical_t crit;
	int i;

	DBG_MSG("Freeing thread %p\n", thread);
	if ((curthread == NULL) || (free_thread_count >= MAX_CACHED_THREADS)) {
		for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
			_lockuser_destroy(&thread->lockusers[i]);
		}
		_lock_destroy(&thread->lock);
		free(thread->alloc_addr);
	}
	else {
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &thread_lock);
		TAILQ_INSERT_HEAD(&free_threadq, thread, tle);
		free_thread_count++;
		KSE_LOCK_RELEASE(curthread->kse, &thread_lock);
		_kse_critical_leave(crit);
	}
}
