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
#include <machine/sigframe.h>

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
 */
#define	MAX_CACHED_THREADS	100
/*
 * Define high water marks for the maximum number of KSEs and KSE groups
 * that will be cached. Because we support 1:1 threading, there could have
 * same number of KSEs and KSE groups as threads. Once these levels are
 * reached, any extra KSE and KSE groups will be free()'d.
 */
#ifdef SYSTEM_SCOPE_ONLY
#define	MAX_CACHED_KSES		100
#define	MAX_CACHED_KSEGS	100
#else
#define	MAX_CACHED_KSES		50
#define	MAX_CACHED_KSEGS	50
#endif

#define	KSE_SET_MBOX(kse, thrd) \
	(kse)->k_kcb->kcb_kmbx.km_curthread = &(thrd)->tcb->tcb_tmbx

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

#define THR_NEED_CANCEL(thrd)						\
	 (((thrd)->cancelflags & THR_CANCELLING) != 0 &&		\
	  ((thrd)->cancelflags & PTHREAD_CANCEL_DISABLE) == 0 &&	\
	  (((thrd)->cancelflags & THR_AT_CANCEL_POINT) != 0 ||		\
	   ((thrd)->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))

#define THR_NEED_ASYNC_CANCEL(thrd)					\
	 (((thrd)->cancelflags & THR_CANCELLING) != 0 &&		\
	  ((thrd)->cancelflags & PTHREAD_CANCEL_DISABLE) == 0 &&	\
	  (((thrd)->cancelflags & THR_AT_CANCEL_POINT) == 0 &&		\
	   ((thrd)->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))

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
static u_int64_t		next_uniqueid = 1;

LIST_HEAD(thread_hash_head, pthread);
#define THREAD_HASH_QUEUES	127
static struct thread_hash_head	thr_hashtable[THREAD_HASH_QUEUES];
#define	THREAD_HASH(thrd)	((unsigned long)thrd % THREAD_HASH_QUEUES)

#ifdef DEBUG_THREAD_KERN
static void	dump_queues(struct kse *curkse);
#endif
static void	kse_check_completed(struct kse *kse);
static void	kse_check_waitq(struct kse *kse);
static void	kse_fini(struct kse *curkse);
static void	kse_reinit(struct kse *kse, int sys_scope);
static void	kse_sched_multi(struct kse_mailbox *kmbx);
static void	kse_sched_single(struct kse_mailbox *kmbx);
static void	kse_switchout_thread(struct kse *kse, struct pthread *thread);
static void	kse_wait(struct kse *kse, struct pthread *td_wait, int sigseq);
static void	kse_free_unlocked(struct kse *kse);
static void	kse_destroy(struct kse *kse);
static void	kseg_free_unlocked(struct kse_group *kseg);
static void	kseg_init(struct kse_group *kseg);
static void	kseg_reinit(struct kse_group *kseg);
static void	kseg_destroy(struct kse_group *kseg);
static void	kse_waitq_insert(struct pthread *thread);
static void	kse_wakeup_multi(struct kse *curkse);
static struct kse_mailbox *kse_wakeup_one(struct pthread *thread);
static void	thr_cleanup(struct kse *kse, struct pthread *curthread);
static void	thr_link(struct pthread *thread);
static void	thr_resume_wrapper(int sig, siginfo_t *, ucontext_t *);
static void	thr_resume_check(struct pthread *curthread, ucontext_t *ucp,
		    struct pthread_sigframe *psf);
static int	thr_timedout(struct pthread *thread, struct timespec *curtime);
static void	thr_unlink(struct pthread *thread);
static void	thr_destroy(struct pthread *thread);
static void	thread_gc(struct pthread *thread);
static void	kse_gc(struct pthread *thread);
static void	kseg_gc(struct pthread *thread);

static void __inline
thr_accounting(struct pthread *thread)
{
	if ((thread->slice_usec != -1) &&
	    (thread->slice_usec <= TIMESLICE_USEC) &&
	    (thread->attr.sched_policy != SCHED_FIFO)) {
		thread->slice_usec += (thread->tcb->tcb_tmbx.tm_uticks
		    + thread->tcb->tcb_tmbx.tm_sticks) * _clock_res_usec;
		/* Check for time quantum exceeded: */
		if (thread->slice_usec > TIMESLICE_USEC)
			thread->slice_usec = -1;
	}
	thread->tcb->tcb_tmbx.tm_uticks = 0;
	thread->tcb->tcb_tmbx.tm_sticks = 0;
}

/*
 * This is called after a fork().
 * No locks need to be taken here since we are guaranteed to be
 * single threaded.
 * 
 * XXX
 * POSIX says for threaded process, fork() function is used
 * only to run new programs, and the effects of calling functions
 * that require certain resources between the call to fork() and
 * the call to an exec function are undefined.
 *
 * Here it is not safe to reinitialize the library after fork().
 * Because memory management may be corrupted, further calling
 * malloc()/free() may cause undefined behavior.
 */
void
_kse_single_thread(struct pthread *curthread)
{
#ifdef NOTYET
	struct kse *kse;
	struct kse_group *kseg;
	struct pthread *thread;
	kse_critical_t crit;
	int i;

	if (__isthreaded) {
		_thr_rtld_fini();
		_thr_signal_deinit();
	}
	__isthreaded = 0;
	/*
	 * Restore signal mask early, so any memory problems could
	 * dump core.
	 */ 
	sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
	_thr_active_threads = 1;

	/*
	 * Enter a loop to remove and free all threads other than
	 * the running thread from the active thread list:
	 */
	while ((thread = TAILQ_FIRST(&_thread_list)) != NULL) {
		THR_GCLIST_REMOVE(thread);
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
			thr_destroy(thread);
		}
	}

	TAILQ_INIT(&curthread->mutexq);		/* initialize mutex queue */
	curthread->joiner = NULL;		/* no joining threads yet */
	curthread->refcount = 0;
	SIGEMPTYSET(curthread->sigpend);	/* clear pending signals */
	if (curthread->specific != NULL) {
		free(curthread->specific);
		curthread->specific = NULL;
		curthread->specific_data_count = 0;
	}

	/* Free the free KSEs: */
	while ((kse = TAILQ_FIRST(&free_kseq)) != NULL) {
		TAILQ_REMOVE(&free_kseq, kse, k_qe);
		kse_destroy(kse);
	}
	free_kse_count = 0;

	/* Free the active KSEs: */
	while ((kse = TAILQ_FIRST(&active_kseq)) != NULL) {
		TAILQ_REMOVE(&active_kseq, kse, k_qe);
		kse_destroy(kse);
	}
	active_kse_count = 0;

	/* Free the free KSEGs: */
	while ((kseg = TAILQ_FIRST(&free_kse_groupq)) != NULL) {
		TAILQ_REMOVE(&free_kse_groupq, kseg, kg_qe);
		kseg_destroy(kseg);
	}
	free_kseg_count = 0;

	/* Free the active KSEGs: */
	while ((kseg = TAILQ_FIRST(&active_kse_groupq)) != NULL) {
		TAILQ_REMOVE(&active_kse_groupq, kseg, kg_qe);
		kseg_destroy(kseg);
	}
	active_kseg_count = 0;

	/* Free the free threads. */
	while ((thread = TAILQ_FIRST(&free_threadq)) != NULL) {
		TAILQ_REMOVE(&free_threadq, thread, tle);
		thr_destroy(thread);
	}
	free_thread_count = 0;

	/* Free the to-be-gc'd threads. */
	while ((thread = TAILQ_FIRST(&_thread_gc_list)) != NULL) {
		TAILQ_REMOVE(&_thread_gc_list, thread, gcle);
		thr_destroy(thread);
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
#else
	int i;

	/* Reset the current thread and KSE lock data. */
	for (i = 0; i < curthread->locklevel; i++) {
		_lockuser_reinit(&curthread->lockusers[i], (void *)curthread);
	}
	curthread->locklevel = 0;
	for (i = 0; i < curthread->kse->k_locklevel; i++) {
		_lockuser_reinit(&curthread->kse->k_lockusers[i],
		    (void *)curthread->kse);
		_LCK_SET_PRIVATE2(&curthread->kse->k_lockusers[i], NULL);
	}
	curthread->kse->k_locklevel = 0;
	_thr_spinlock_init();
	if (__isthreaded) {
		_thr_rtld_fini();
		_thr_signal_deinit();
	}
	__isthreaded = 0;
	/*
	 * Restore signal mask early, so any memory problems could
	 * dump core.
	 */ 
	sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
	curthread->kse->k_kcb->kcb_kmbx.km_curthread = NULL;
	curthread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
	_thr_active_threads = 1;
#endif
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
	sigset_t sigset;

	if ((threaded != 0) && (__isthreaded == 0)) {
		SIGFILLSET(sigset);
		__sys_sigprocmask(SIG_SETMASK, &sigset, &_thr_initial->sigmask);

		/*
		 * Tell the kernel to create a KSE for the initial thread
		 * and enable upcalls in it.
		 */
		_kse_initial->k_flags |= KF_STARTED;

#ifdef SYSTEM_SCOPE_ONLY
		/*
		 * For bound thread, kernel reads mailbox pointer once,
		 * we'd set it here before calling kse_create
		 */
		_tcb_set(_kse_initial->k_kcb, _thr_initial->tcb);
		KSE_SET_MBOX(_kse_initial, _thr_initial);
		_kse_initial->k_kcb->kcb_kmbx.km_flags |= KMF_BOUND;
#else
		_thr_initial->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;
		_kse_initial->k_kseg->kg_flags &= ~KGF_SINGLE_THREAD;
		_kse_initial->k_kcb->kcb_kmbx.km_curthread = NULL;
#endif

		/*
		 * Locking functions in libc are required when there are
		 * threads other than the initial thread.
		 */
		_thr_rtld_init();

		__isthreaded = 1;
		if (kse_create(&_kse_initial->k_kcb->kcb_kmbx, 0) != 0) {
			_kse_initial->k_flags &= ~KF_STARTED;
			__isthreaded = 0;
			PANIC("kse_create() failed\n");
			return (-1);
		}

#ifndef SYSTEM_SCOPE_ONLY
		/* Set current thread to initial thread */
		_tcb_set(_kse_initial->k_kcb, _thr_initial->tcb);
		KSE_SET_MBOX(_kse_initial, _thr_initial);
		_thr_start_sig_daemon();
		_thr_setmaxconcurrency();
#else
		__sys_sigprocmask(SIG_SETMASK, &_thr_initial->sigmask, NULL);
#endif
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

	if (curkse->k_kcb->kcb_kmbx.km_curthread != NULL)
		PANIC("kse_lock_wait does not disable upcall.\n");
	/*
	 * Enter a loop to wait until we get the lock.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;  /* 1 sec */
	while (!_LCK_GRANTED(lu)) {
		/*
		 * Yield the kse and wait to be notified when the lock
		 * is granted.
		 */
		saved_flags = curkse->k_kcb->kcb_kmbx.km_flags;
		curkse->k_kcb->kcb_kmbx.km_flags |= KMF_NOUPCALL |
		    KMF_NOCOMPLETED;
		kse_release(&ts);
		curkse->k_kcb->kcb_kmbx.km_flags = saved_flags;
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
		mbx = &kse->k_kcb->kcb_kmbx;
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
		THR_LOCK_SWITCH(curthread);
		THR_SET_STATE(curthread, PS_LOCKWAIT);
		_thr_sched_switch_unlocked(curthread);
	} while (!_LCK_GRANTED(lu));
}

void
_thr_lock_wakeup(struct lock *lock, struct lockuser *lu)
{
	struct pthread *thread;
	struct pthread *curthread;
	struct kse_mailbox *kmbx;

	curthread = _get_curthread();
	thread = (struct pthread *)_LCK_GET_PRIVATE(lu);

	THR_SCHED_LOCK(curthread, thread);
	_lock_grant(lock, lu);
	kmbx = _thr_setrunnable_unlocked(thread);
	THR_SCHED_UNLOCK(curthread, thread);
	if (kmbx != NULL)
		kse_wakeup(kmbx);
}

kse_critical_t
_kse_critical_enter(void)
{
	kse_critical_t crit;

	crit = (kse_critical_t)_kcb_critical_enter();
	return (crit);
}

void
_kse_critical_leave(kse_critical_t crit)
{
	struct pthread *curthread;

	_kcb_critical_leave((struct kse_thr_mailbox *)crit);
	if ((crit != NULL) && ((curthread = _get_curthread()) != NULL))
		THR_YIELD_CHECK(curthread);
}

int
_kse_in_critical(void)
{
	return (_kcb_in_critical());
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
	struct pthread_sigframe psf;
	struct kse *curkse;
	volatile int resume_once = 0;
	ucontext_t *uc;

	/* We're in the scheduler, 5 by 5: */
	curkse = _get_curkse();

	curthread->need_switchout = 1;	/* The thread yielded on its own. */
	curthread->critical_yield = 0;	/* No need to yield anymore. */

	/* Thread can unlock the scheduler lock. */
	curthread->lock_switch = 1;

	/*
	 * The signal frame is allocated off the stack because
	 * a thread can be interrupted by other signals while
	 * it is running down pending signals.
	 */
	psf.psf_valid = 0;
	curthread->curframe = &psf;

	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		kse_sched_single(&curkse->k_kcb->kcb_kmbx);
	else {
		curkse->k_switch = 1;
		_thread_enter_uts(curthread->tcb, curkse->k_kcb);
	}
	
	/*
	 * It is ugly we must increase critical count, because we
	 * have a frame saved, we must backout state in psf
	 * before we can process signals.
 	 */
	curthread->critical_count += psf.psf_valid;

	/*
	 * Unlock the scheduling queue and leave the
	 * critical region.
	 */
	/* Don't trust this after a switch! */
	curkse = _get_curkse();

	curthread->lock_switch = 0;
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	_kse_critical_leave(&curthread->tcb->tcb_tmbx);

	/*
	 * This thread is being resumed; check for cancellations.
	 */
	if ((psf.psf_valid ||
	    ((curthread->check_pending || THR_NEED_ASYNC_CANCEL(curthread))
	    && !THR_IN_CRITICAL(curthread)))) {
		uc = alloca(sizeof(ucontext_t));
		resume_once = 0;
		THR_GETCONTEXT(uc);
		if (resume_once == 0) {
			resume_once = 1;
			curthread->check_pending = 0;
			thr_resume_check(curthread, uc, &psf);
		}
	}
	THR_ACTIVATE_LAST_LOCK(curthread);
}

/*
 * This is the scheduler for a KSE which runs a scope system thread.
 * The multi-thread KSE scheduler should also work for a single threaded
 * KSE, but we use a separate scheduler so that it can be fine-tuned
 * to be more efficient (and perhaps not need a separate stack for
 * the KSE, allowing it to use the thread's stack).
 */

static void
kse_sched_single(struct kse_mailbox *kmbx)
{
	struct kse *curkse;
	struct pthread *curthread;
	struct timespec ts;
	sigset_t sigmask;
	int i, sigseqno, level, first = 0;

	curkse = (struct kse *)kmbx->km_udata;
	curthread = curkse->k_curthread;

	if ((curkse->k_flags & KF_INITIALIZED) == 0) {
		/* Setup this KSEs specific data. */
		_kcb_set(curkse->k_kcb);
		_tcb_set(curkse->k_kcb, curthread->tcb);
		curkse->k_flags |= KF_INITIALIZED;
		first = 1;
		curthread->active = 1;
		
		/* Setup kernel signal masks for new thread. */
		__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
		/*
		 * Enter critical region, this is meanless for bound thread,
		 * It is used to let other code work, those code want mailbox
		 * to be cleared.
		 */
		(void)_kse_critical_enter();
 	} else {
		/*
		 * Bound thread always has tcb set, this prevent some
		 * code from blindly setting bound thread tcb to NULL,
		 * buggy code ?
		 */
		_tcb_set(curkse->k_kcb, curthread->tcb);
	}

	curthread->critical_yield = 0;
	curthread->need_switchout = 0;

	/*
	 * Lock the scheduling queue.
	 *
	 * There is no scheduling queue for single threaded KSEs,
	 * but we need a lock for protection regardless.
	 */
	if (curthread->lock_switch == 0)
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);

	/*
	 * This has to do the job of kse_switchout_thread(), only
	 * for a single threaded KSE/KSEG.
	 */

	switch (curthread->state) {
	case PS_MUTEX_WAIT:
	case PS_COND_WAIT:
		if (THR_NEED_CANCEL(curthread)) {
			curthread->interrupted = 1;
			curthread->continuation = _thr_finish_cancellation;
			THR_SET_STATE(curthread, PS_RUNNING);
		}
		break;

	case PS_LOCKWAIT:
		/*
		 * This state doesn't timeout.
		 */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;
		level = curthread->locklevel - 1;
		if (_LCK_GRANTED(&curthread->lockusers[level]))
			THR_SET_STATE(curthread, PS_RUNNING);
		break;

	case PS_DEAD:
		curthread->check_pending = 0;
		/* Unlock the scheduling queue and exit the KSE and thread. */
		thr_cleanup(curkse, curthread);
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		PANIC("bound thread shouldn't get here\n");
		break;

	case PS_JOIN:
		if (THR_NEED_CANCEL(curthread)) {
			curthread->join_status.thread = NULL;
			THR_SET_STATE(curthread, PS_RUNNING);
		} else {
			/*
			 * This state doesn't timeout.
			 */
			curthread->wakeup_time.tv_sec = -1;
			curthread->wakeup_time.tv_nsec = -1;
		}
		break;

	case PS_SUSPENDED:
		if (THR_NEED_CANCEL(curthread)) {
			curthread->interrupted = 1;
			THR_SET_STATE(curthread, PS_RUNNING);
		} else {
			/*
			 * These states don't timeout.
			 */
			curthread->wakeup_time.tv_sec = -1;
			curthread->wakeup_time.tv_nsec = -1;
		}
		break;

	case PS_RUNNING:
		if ((curthread->flags & THR_FLAGS_SUSPENDED) != 0 &&
		    !THR_NEED_CANCEL(curthread)) {
			THR_SET_STATE(curthread, PS_SUSPENDED);
			/*
			 * These states don't timeout.
			 */
			curthread->wakeup_time.tv_sec = -1;
			curthread->wakeup_time.tv_nsec = -1;
		}
		break;

	case PS_SIGWAIT:
		PANIC("bound thread does not have SIGWAIT state\n");

	case PS_SLEEP_WAIT:
		PANIC("bound thread does not have SLEEP_WAIT state\n");

	case PS_SIGSUSPEND:
		PANIC("bound thread does not have SIGSUSPEND state\n");
	
	case PS_DEADLOCK:
		/*
		 * These states don't timeout and don't need
		 * to be in the waiting queue.
		 */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;
		break;

	default:
		PANIC("Unknown state\n");
		break;
	}

	while (curthread->state != PS_RUNNING) {
		sigseqno = curkse->k_sigseqno;
		if (curthread->check_pending != 0) {
			/*
			 * Install pending signals into the frame, possible
			 * cause mutex or condvar backout.
			 */
			curthread->check_pending = 0;
			SIGFILLSET(sigmask);

			/*
			 * Lock out kernel signal code when we are processing
			 * signals, and get a fresh copy of signal mask.
			 */
			__sys_sigprocmask(SIG_SETMASK, &sigmask,
					  &curthread->sigmask);
			for (i = 1; i <= _SIG_MAXSIG; i++) {
				if (SIGISMEMBER(curthread->sigmask, i))
					continue;
				if (SIGISMEMBER(curthread->sigpend, i))
					(void)_thr_sig_add(curthread, i, 
					    &curthread->siginfo[i-1]);
			}
			__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask,
				NULL);
			/* The above code might make thread runnable */
			if (curthread->state == PS_RUNNING)
				break;
		}
		THR_DEACTIVATE_LAST_LOCK(curthread);
		kse_wait(curkse, curthread, sigseqno);
		THR_ACTIVATE_LAST_LOCK(curthread);
		KSE_GET_TOD(curkse, &ts);
		if (thr_timedout(curthread, &ts)) {
			/* Indicate the thread timedout: */
			curthread->timeout = 1;
			/* Make the thread runnable. */
			THR_SET_STATE(curthread, PS_RUNNING);
		}
	}

	/* Remove the frame reference. */
	curthread->curframe = NULL;

	if (curthread->lock_switch == 0) {
		/* Unlock the scheduling queue. */
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	}

	DBG_MSG("Continuing bound thread %p\n", curthread);
	if (first) {
		_kse_critical_leave(&curthread->tcb->tcb_tmbx);
		pthread_exit(curthread->start_routine(curthread->arg));
	}
}

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
kse_sched_multi(struct kse_mailbox *kmbx)
{
	struct kse *curkse;
	struct pthread *curthread, *td_wait;
	struct pthread_sigframe *curframe;
	int ret;

	curkse = (struct kse *)kmbx->km_udata;
	THR_ASSERT(curkse->k_kcb->kcb_kmbx.km_curthread == NULL,
	    "Mailbox not null in kse_sched_multi");

	/* Check for first time initialization: */
	if ((curkse->k_flags & KF_INITIALIZED) == 0) {
		/* Setup this KSEs specific data. */
		_kcb_set(curkse->k_kcb);

		/* Set this before grabbing the context. */
		curkse->k_flags |= KF_INITIALIZED;
	}

	/*
	 * No current thread anymore, calling _get_curthread in UTS
	 * should dump core
	 */
	_tcb_set(curkse->k_kcb, NULL);

	/* If this is an upcall; take the scheduler lock. */
	if (curkse->k_switch == 0)
		KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	curkse->k_switch = 0;

	/*
	 * Now that the scheduler lock is held, get the current
	 * thread.  The KSE's current thread cannot be safely
	 * examined without the lock because it could have returned
	 * as completed on another KSE.  See kse_check_completed().
	 */
	curthread = curkse->k_curthread;

	if (KSE_IS_IDLE(curkse)) {
		KSE_CLEAR_IDLE(curkse);
		curkse->k_kseg->kg_idle_kses--;
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
		ret = _thread_switch(curkse->k_kcb, curthread->tcb, 1);
		if (ret != 0)
			PANIC("Can't resume thread in critical region\n");
	}
	else if ((curthread->flags & THR_FLAGS_IN_RUNQ) == 0)
		kse_switchout_thread(curkse, curthread);
	curkse->k_curthread = NULL;

#ifdef DEBUG_THREAD_KERN
	dump_queues(curkse);
#endif

	/* Check if there are no threads ready to run: */
	while (((curthread = KSE_RUNQ_FIRST(curkse)) == NULL) &&
	    (curkse->k_kseg->kg_threadcount != 0) &&
	    ((curkse->k_flags & KF_TERMINATED) == 0)) {
		/*
		 * Wait for a thread to become active or until there are
		 * no more threads.
		 */
		td_wait = KSE_WAITQ_FIRST(curkse);
		kse_wait(curkse, td_wait, 0);
		kse_check_completed(curkse);
		kse_check_waitq(curkse);
	}

	/* Check for no more threads: */
	if ((curkse->k_kseg->kg_threadcount == 0) ||
	    ((curkse->k_flags & KF_TERMINATED) != 0)) {
		/*
		 * Normally this shouldn't return, but it will if there
		 * are other KSEs running that create new threads that
		 * are assigned to this KSE[G].  For instance, if a scope
		 * system thread were to create a scope process thread
		 * and this kse[g] is the initial kse[g], then that newly
		 * created thread would be assigned to us (the initial
		 * kse[g]).
		 */
		kse_wakeup_multi(curkse);
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		kse_fini(curkse);
		/* never returns */
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

	/*
	 * The thread's current signal frame will only be NULL if it
	 * is being resumed after being blocked in the kernel.  In
	 * this case, and if the thread needs to run down pending
	 * signals or needs a cancellation check, we need to add a
	 * signal frame to the thread's context.
	 */
	if ((curframe == NULL) && (curthread->state == PS_RUNNING) &&
	    (curthread->check_pending != 0 ||
	     THR_NEED_ASYNC_CANCEL(curthread)) &&
	    !THR_IN_CRITICAL(curthread)) {
		curthread->check_pending = 0;
		signalcontext(&curthread->tcb->tcb_tmbx.tm_context, 0,
		    (__sighandler_t *)thr_resume_wrapper);
	}
	kse_wakeup_multi(curkse);
	/*
	 * Continue the thread at its current frame:
	 */
	if (curthread->lock_switch != 0) {
		/*
		 * This thread came from a scheduler switch; it will
		 * unlock the scheduler lock and set the mailbox.
		 */
		ret = _thread_switch(curkse->k_kcb, curthread->tcb, 0);
	} else {
		/* This thread won't unlock the scheduler lock. */
		KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
		ret = _thread_switch(curkse->k_kcb, curthread->tcb, 1);
	}
	if (ret != 0)
		PANIC("Thread has returned from _thread_switch");

	/* This point should not be reached. */
	PANIC("Thread has returned from _thread_switch");
}

static void
thr_resume_wrapper(int sig, siginfo_t *siginfo, ucontext_t *ucp)
{
	struct pthread *curthread = _get_curthread();
	struct kse *curkse;
	int ret, err_save = errno;

	DBG_MSG(">>> sig wrapper\n");
	if (curthread->lock_switch)
		PANIC("thr_resume_wrapper, lock_switch != 0\n");
	thr_resume_check(curthread, ucp, NULL);
	errno = err_save;
	_kse_critical_enter();
	curkse = _get_curkse();
	curthread->tcb->tcb_tmbx.tm_context = *ucp;
	ret = _thread_switch(curkse->k_kcb, curthread->tcb, 1);
	if (ret != 0)
		PANIC("thr_resume_wrapper: thread has returned "
		      "from _thread_switch");
	/* THR_SETCONTEXT(ucp); */ /* not work, why ? */
}

static void
thr_resume_check(struct pthread *curthread, ucontext_t *ucp,
    struct pthread_sigframe *psf)
{
	_thr_sig_rundown(curthread, ucp, psf);

	if (THR_NEED_ASYNC_CANCEL(curthread))
		pthread_testcancel();
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
	struct kse_mailbox *kmbx = NULL;
	int sys_scope;

	if ((joiner = thread->joiner) != NULL) {
		/* Joinee scheduler lock held; joiner won't leave. */
		if (joiner->kseg == curkse->k_kseg) {
			if (joiner->join_status.thread == thread) {
				joiner->join_status.thread = NULL;
				joiner->join_status.ret = thread->ret;
				(void)_thr_setrunnable_unlocked(joiner);
			}
		} else {
			KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
			/* The joiner may have removed itself and exited. */
			if (_thr_ref_add(thread, joiner, 0) == 0) {
				KSE_SCHED_LOCK(curkse, joiner->kseg);
				if (joiner->join_status.thread == thread) {
					joiner->join_status.thread = NULL;
					joiner->join_status.ret = thread->ret;
					kmbx = _thr_setrunnable_unlocked(joiner);
				}
				KSE_SCHED_UNLOCK(curkse, joiner->kseg);
				_thr_ref_delete(thread, joiner);
				if (kmbx != NULL)
					kse_wakeup(kmbx);
			}
			KSE_SCHED_LOCK(curkse, curkse->k_kseg);
		}
		thread->attr.flags |= PTHREAD_DETACHED;
	}

	if (!(sys_scope = (thread->attr.flags & PTHREAD_SCOPE_SYSTEM))) {
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
	if (sys_scope) {
		/*
		 * System scope thread is single thread group, 
		 * when thread is exited, its kse and ksegrp should
		 * be recycled as well.
		 * kse upcall stack belongs to thread, clear it here.
		 */
		curkse->k_stack.ss_sp = 0;
		curkse->k_stack.ss_size = 0;
		kse_exit();
		PANIC("kse_exit() failed for system scope thread");
	}
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
}

void
_thr_gc(struct pthread *curthread)
{
	thread_gc(curthread);
	kse_gc(curthread);
	kseg_gc(curthread);
}

static void
thread_gc(struct pthread *curthread)
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
		else if (((td->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) &&
		    ((td->kse->k_kcb->kcb_kmbx.km_flags & KMF_DONE) == 0)) {
			/*
			 * The thread and KSE are operating on the same
			 * stack.  Wait for the KSE to exit before freeing
			 * the thread's stack as well as everything else.
			 */
			continue;
		}
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
		/*
		 * XXX we don't free initial thread and its kse
		 * (if thread is a bound thread), because there might
		 * have some code referencing initial thread and kse.
		 */
		if (td == _thr_initial) {
			DBG_MSG("Initial thread won't be freed\n");
			continue;
		}

		if ((td->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) {
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

static void
kse_gc(struct pthread *curthread)
{
	kse_critical_t crit;
	TAILQ_HEAD(, kse) worklist;
	struct kse *kse;

	if (free_kse_count <= MAX_CACHED_KSES)
		return;
	TAILQ_INIT(&worklist);
	crit = _kse_critical_enter();
	KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
	while (free_kse_count > MAX_CACHED_KSES) {
		kse = TAILQ_FIRST(&free_kseq);
		TAILQ_REMOVE(&free_kseq, kse, k_qe);
		TAILQ_INSERT_HEAD(&worklist, kse, k_qe);
		free_kse_count--;
	}
	KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
	_kse_critical_leave(crit);

	while ((kse = TAILQ_FIRST(&worklist))) {
		TAILQ_REMOVE(&worklist, kse, k_qe);
		kse_destroy(kse);
	}
}

static void
kseg_gc(struct pthread *curthread)
{
	kse_critical_t crit;
	TAILQ_HEAD(, kse_group) worklist;
	struct kse_group *kseg;

	if (free_kseg_count <= MAX_CACHED_KSEGS)
		return; 
	crit = _kse_critical_enter();
	KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
	while (free_kseg_count > MAX_CACHED_KSEGS) {
		kseg = TAILQ_FIRST(&free_kse_groupq);
		TAILQ_REMOVE(&free_kse_groupq, kseg, kg_qe);
		free_kseg_count--;
		TAILQ_INSERT_HEAD(&worklist, kseg, kg_qe);
	}
	KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
	_kse_critical_leave(crit);

	while ((kseg = TAILQ_FIRST(&worklist))) {
		TAILQ_REMOVE(&worklist, kseg, kg_qe);
		kseg_destroy(kseg);
	}
}

/*
 * Only new threads that are running or suspended may be scheduled.
 */
int
_thr_schedule_add(struct pthread *curthread, struct pthread *newthread)
{
	kse_critical_t crit;
	int ret;

	/* Add the new thread. */
	thr_link(newthread);

	/*
	 * If this is the first time creating a thread, make sure
	 * the mailbox is set for the current thread.
	 */
	if ((newthread->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) {
		/* We use the thread's stack as the KSE's stack. */
		newthread->kse->k_kcb->kcb_kmbx.km_stack.ss_sp =
		    newthread->attr.stackaddr_attr;
		newthread->kse->k_kcb->kcb_kmbx.km_stack.ss_size =
		    newthread->attr.stacksize_attr;

		/*
		 * No need to lock the scheduling queue since the
		 * KSE/KSEG pair have not yet been started.
		 */
		KSEG_THRQ_ADD(newthread->kseg, newthread);
		/* this thread never gives up kse */
		newthread->active = 1;
		newthread->kse->k_curthread = newthread;
		newthread->kse->k_kcb->kcb_kmbx.km_flags = KMF_BOUND;
		newthread->kse->k_kcb->kcb_kmbx.km_func =
		    (kse_func_t *)kse_sched_single;
		newthread->kse->k_kcb->kcb_kmbx.km_quantum = 0;
		KSE_SET_MBOX(newthread->kse, newthread);
		/*
		 * This thread needs a new KSE and KSEG.
		 */
		newthread->kse->k_flags &= ~KF_INITIALIZED;
		newthread->kse->k_flags |= KF_STARTED;
		/* Fire up! */
		ret = kse_create(&newthread->kse->k_kcb->kcb_kmbx, 1);
		if (ret != 0)
			ret = errno;
	}
	else {
		/*
		 * Lock the KSE and add the new thread to its list of
		 * assigned threads.  If the new thread is runnable, also
		 * add it to the KSE's run queue.
		 */
		crit = _kse_critical_enter();
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
			newthread->kse->k_kcb->kcb_kmbx.km_func =
			    (kse_func_t *)kse_sched_multi;
			newthread->kse->k_kcb->kcb_kmbx.km_flags = 0;
			kse_create(&newthread->kse->k_kcb->kcb_kmbx, 0);
		 } else if ((newthread->state == PS_RUNNING) &&
		     KSE_IS_IDLE(newthread->kse)) {
			/*
			 * The thread is being scheduled on another KSEG.
			 */
			kse_wakeup_one(newthread);
		}
		KSE_SCHED_UNLOCK(curthread->kse, newthread->kseg);
		_kse_critical_leave(crit);
		ret = 0;
	}
	if (ret != 0)
		thr_unlink(newthread);

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
	int sig;

	if ((completed = kse->k_kcb->kcb_kmbx.km_completed) != NULL) {
		kse->k_kcb->kcb_kmbx.km_completed = NULL;
		while (completed != NULL) {
			thread = completed->tm_udata;
			DBG_MSG("Found completed thread %p, name %s\n",
			    thread,
			    (thread->name == NULL) ? "none" : thread->name);
			thread->blocked = 0;
			if (thread != kse->k_curthread) {
				thr_accounting(thread);
				if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
					THR_SET_STATE(thread, PS_SUSPENDED);
				else
					KSE_RUNQ_INSERT_TAIL(kse, thread);
				if ((thread->kse != kse) &&
				    (thread->kse->k_curthread == thread)) {
					/*
					 * Remove this thread from its
					 * previous KSE so that it (the KSE)
					 * doesn't think it is still active.
					 */
					thread->kse->k_curthread = NULL;
					thread->active = 0;
				}
			}
			if ((sig = thread->tcb->tcb_tmbx.tm_syncsig.si_signo)
			    != 0) {
				if (SIGISMEMBER(thread->sigmask, sig))
					SIGADDSET(thread->sigpend, sig);
				else if (THR_IN_CRITICAL(thread))
					kse_thr_interrupt(NULL, KSE_INTR_SIGEXIT, sig);
				else
					(void)_thr_sig_add(thread, sig,
					    &thread->tcb->tcb_tmbx.tm_syncsig);
				thread->tcb->tcb_tmbx.tm_syncsig.si_signo = 0;
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
	int restart;
	siginfo_t siginfo;

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
		/*
		 * Check for pending signals and cancellation for
		 * this thread to see if we need to interrupt it
		 * in the kernel.
		 */
		if (THR_NEED_CANCEL(thread)) {
			kse_thr_interrupt(&thread->tcb->tcb_tmbx,
					  KSE_INTR_INTERRUPT, 0);
		} else if (thread->check_pending != 0) {
			for (i = 1; i <= _SIG_MAXSIG; ++i) {
				if (SIGISMEMBER(thread->sigpend, i) &&
				    !SIGISMEMBER(thread->sigmask, i)) {
					restart = _thread_sigact[i - 1].sa_flags & SA_RESTART;
					kse_thr_interrupt(&thread->tcb->tcb_tmbx,
					    restart ? KSE_INTR_RESTART : KSE_INTR_INTERRUPT, 0);
					break;
				}
			}
		}
	}
	else {
		switch (thread->state) {
		case PS_MUTEX_WAIT:
		case PS_COND_WAIT:
			if (THR_NEED_CANCEL(thread)) {
				thread->interrupted = 1;
				thread->continuation = _thr_finish_cancellation;
				THR_SET_STATE(thread, PS_RUNNING);
			} else {
				/* Insert into the waiting queue: */
				KSE_WAITQ_INSERT(kse, thread);
			}
			break;

		case PS_LOCKWAIT:
			/*
			 * This state doesn't timeout.
			 */
			thread->wakeup_time.tv_sec = -1;
			thread->wakeup_time.tv_nsec = -1;
			level = thread->locklevel - 1;
			if (!_LCK_GRANTED(&thread->lockusers[level]))
				KSE_WAITQ_INSERT(kse, thread);
			else
				THR_SET_STATE(thread, PS_RUNNING);
			break;

		case PS_SLEEP_WAIT:
		case PS_SIGWAIT:
			if (THR_NEED_CANCEL(thread)) {
				thread->interrupted = 1;
				THR_SET_STATE(thread, PS_RUNNING);
			} else {
				KSE_WAITQ_INSERT(kse, thread);
			}
			break;

		case PS_JOIN:
			if (THR_NEED_CANCEL(thread)) {
				thread->join_status.thread = NULL;
				THR_SET_STATE(thread, PS_RUNNING);
			} else {
				/*
				 * This state doesn't timeout.
				 */
				thread->wakeup_time.tv_sec = -1;
				thread->wakeup_time.tv_nsec = -1;

				/* Insert into the waiting queue: */
				KSE_WAITQ_INSERT(kse, thread);
			}
			break;

		case PS_SIGSUSPEND:
		case PS_SUSPENDED:
			if (THR_NEED_CANCEL(thread)) {
				thread->interrupted = 1;
				THR_SET_STATE(thread, PS_RUNNING);
			} else {
				/*
				 * These states don't timeout.
				 */
				thread->wakeup_time.tv_sec = -1;
				thread->wakeup_time.tv_nsec = -1;

				/* Insert into the waiting queue: */
				KSE_WAITQ_INSERT(kse, thread);
			}
			break;

		case PS_DEAD:
			/*
			 * The scheduler is operating on a different
			 * stack.  It is safe to do garbage collecting
			 * here.
			 */
			thread->active = 0;
			thread->need_switchout = 0;
			thread->lock_switch = 0;
			thr_cleanup(kse, thread);
			return;
			break;

		case PS_RUNNING:
			if ((thread->flags & THR_FLAGS_SUSPENDED) != 0 &&
			    !THR_NEED_CANCEL(thread))
				THR_SET_STATE(thread, PS_SUSPENDED);
			break;

		case PS_DEADLOCK:
			/*
			 * These states don't timeout.
			 */
			thread->wakeup_time.tv_sec = -1;
			thread->wakeup_time.tv_nsec = -1;

			/* Insert into the waiting queue: */
			KSE_WAITQ_INSERT(kse, thread);
			break;

		default:
			PANIC("Unknown state\n");
			break;
		}

		thr_accounting(thread);
		if (thread->state == PS_RUNNING) {
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
		KSE_LOCK_ACQUIRE(kse, &_thread_signal_lock);
		for (i = 1; i <= _SIG_MAXSIG; i++) {
			if (SIGISMEMBER(thread->sigmask, i))
				continue;
			if (SIGISMEMBER(thread->sigpend, i))
				(void)_thr_sig_add(thread, i,
				    &thread->siginfo[i-1]);
			else if (SIGISMEMBER(_thr_proc_sigpending, i) &&
				_thr_getprocsig_unlocked(i, &siginfo)) {
				(void)_thr_sig_add(thread, i, &siginfo);
			}
		}
		KSE_LOCK_RELEASE(kse, &_thread_signal_lock);
	}
}

/*
 * This function waits for the smallest timeout value of any waiting
 * thread, or until it receives a message from another KSE.
 *
 * This must be called with the scheduling lock held.
 */
static void
kse_wait(struct kse *kse, struct pthread *td_wait, int sigseqno)
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
		if ((kse->k_kseg->kg_flags & KGF_SINGLE_THREAD) &&
		    (kse->k_sigseqno != sigseqno))
			; /* don't sleep */
		else {
			saved_flags = kse->k_kcb->kcb_kmbx.km_flags;
			kse->k_kcb->kcb_kmbx.km_flags |= KMF_NOUPCALL;
			kse_release(&ts_sleep);
			kse->k_kcb->kcb_kmbx.km_flags = saved_flags;
		}
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
	/* struct kse_group *free_kseg = NULL; */
	struct timespec ts;
	struct pthread *td;

	/*
	 * Check to see if this is one of the main kses.
	 */
	if (kse->k_kseg != _kse_initial->k_kseg) {
		PANIC("shouldn't get here");
		/* This is for supporting thread groups. */
#ifdef NOT_YET
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
		KSE_LOCK_ACQUIRE(kse, &kse_lock);
		if (free_kseg != NULL)
			kseg_free_unlocked(free_kseg);
		kse_free_unlocked(kse);
		KSE_LOCK_RELEASE(kse, &kse_lock);
		kse_exit();
		/* Never returns. */
		PANIC("kse_exit()");
#endif
	} else {
		/*
		 * We allow program to kill kse in initial group (by
		 * lowering the concurrency).
		 */
		if ((kse != _kse_initial) &&
		    ((kse->k_flags & KF_TERMINATED) != 0)) {
			KSE_SCHED_LOCK(kse, kse->k_kseg);
			TAILQ_REMOVE(&kse->k_kseg->kg_kseq, kse, k_kgqe);
			kse->k_kseg->kg_ksecount--;
			/*
			 * Migrate thread to  _kse_initial if its lastest
			 * kse it ran on is the kse.
			 */
			td = TAILQ_FIRST(&kse->k_kseg->kg_threadq);
			while (td != NULL) {
				if (td->kse == kse)
					td->kse = _kse_initial;
				td = TAILQ_NEXT(td, kle);
			}
			KSE_SCHED_UNLOCK(kse, kse->k_kseg);
			KSE_LOCK_ACQUIRE(kse, &kse_lock);
			kse_free_unlocked(kse);
			KSE_LOCK_RELEASE(kse, &kse_lock);
			/* Make sure there is always at least one is awake */
			KSE_WAKEUP(_kse_initial);
			kse_exit();
                        /* Never returns. */
                        PANIC("kse_exit() failed for initial kseg");
                }
		KSE_SCHED_LOCK(kse, kse->k_kseg);
		KSE_SET_IDLE(kse);
		kse->k_kseg->kg_idle_kses++;
		KSE_SCHED_UNLOCK(kse, kse->k_kseg);
		ts.tv_sec = 120;
		ts.tv_nsec = 0;
		kse->k_kcb->kcb_kmbx.km_flags = 0;
		kse_release(&ts);
		/* Never reach */
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
	struct kse_mailbox *kmbx;

	crit = _kse_critical_enter();
	KSE_SCHED_LOCK(curthread->kse, thread->kseg);
	kmbx = _thr_setrunnable_unlocked(thread);
	KSE_SCHED_UNLOCK(curthread->kse, thread->kseg);
	_kse_critical_leave(crit);
	if ((kmbx != NULL) && (__isthreaded != 0))
		kse_wakeup(kmbx);
}

struct kse_mailbox *
_thr_setrunnable_unlocked(struct pthread *thread)
{
	struct kse_mailbox *kmbx = NULL;

	if ((thread->kseg->kg_flags & KGF_SINGLE_THREAD) != 0) {
		/* No silly queues for these threads. */
		if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
			THR_SET_STATE(thread, PS_SUSPENDED);
		else {
			THR_SET_STATE(thread, PS_RUNNING);
			kmbx = kse_wakeup_one(thread);
		}

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
			/*
			 * XXX - Threads are not yet assigned to specific
			 *       KSEs; they are assigned to the KSEG.  So
			 *       the fact that a thread's KSE is waiting
			 *       doesn't necessarily mean that it will be
			 *       the KSE that runs the thread after the
			 *       lock is granted.  But we don't know if the
			 *       other KSEs within the same KSEG are also
			 *       in a waiting state or not so we err on the
			 *       side of caution and wakeup the thread's
			 *       last known KSE.  We ensure that the
			 *       threads KSE doesn't change while it's
			 *       scheduling lock is held so it is safe to
			 *       reference it (the KSE).  If the KSE wakes
			 *       up and doesn't find any more work it will
			 *       again go back to waiting so no harm is
			 *       done.
			 */
			kmbx = kse_wakeup_one(thread);
		}
	}
	return (kmbx);
}

static struct kse_mailbox *
kse_wakeup_one(struct pthread *thread)
{
	struct kse *ke;

	if (KSE_IS_IDLE(thread->kse)) {
		KSE_CLEAR_IDLE(thread->kse);
		thread->kseg->kg_idle_kses--;
		return (&thread->kse->k_kcb->kcb_kmbx);
	} else {
		TAILQ_FOREACH(ke, &thread->kseg->kg_kseq, k_kgqe) {
			if (KSE_IS_IDLE(ke)) {
				KSE_CLEAR_IDLE(ke);
				ke->k_kseg->kg_idle_kses--;
				return (&ke->k_kcb->kcb_kmbx);
			}
		}
	}
	return (NULL);
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

static void
kseg_destroy(struct kse_group *kseg)
{
	_lock_destroy(&kseg->kg_lock);
	_pq_free(&kseg->kg_schedq.sq_runq);
	free(kseg);
}

/*
 * Allocate a new KSE.
 *
 * We allow the current thread to be NULL in the case that this
 * is the first time a KSE is being created (library initialization).
 * In this case, we don't need to (and can't) take any locks.
 */
struct kse *
_kse_alloc(struct pthread *curthread, int sys_scope)
{
	struct kse *kse = NULL;
	char *stack;
	kse_critical_t crit;
	int i;

	if ((curthread != NULL) && (free_kse_count > 0)) {
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		/* Search for a finished KSE. */
		kse = TAILQ_FIRST(&free_kseq);
		while ((kse != NULL) &&
		    ((kse->k_kcb->kcb_kmbx.km_flags & KMF_DONE) == 0)) {
			kse = TAILQ_NEXT(kse, k_qe);
		}
		if (kse != NULL) {
			DBG_MSG("found an unused kse.\n");
			TAILQ_REMOVE(&free_kseq, kse, k_qe);
			free_kse_count--;
			TAILQ_INSERT_TAIL(&active_kseq, kse, k_qe);
			active_kse_count++;
		}
		KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
		_kse_critical_leave(crit);
		if (kse != NULL)
			kse_reinit(kse, sys_scope);
	}
	if ((kse == NULL) &&
	    ((kse = (struct kse *)malloc(sizeof(*kse))) != NULL)) {
		if (sys_scope != 0)
			stack = NULL;
		else if ((stack = malloc(KSE_STACKSIZE)) == NULL) {
			free(kse);
			return (NULL);
		}
		bzero(kse, sizeof(*kse));

		/* Initialize KCB without the lock. */
		if ((kse->k_kcb = _kcb_ctor(kse)) == NULL) {
			if (stack != NULL)
				free(stack);
			free(kse);
			return (NULL);
		}

		/* Initialize the lockusers. */
		for (i = 0; i < MAX_KSE_LOCKLEVEL; i++) {
			_lockuser_init(&kse->k_lockusers[i], (void *)kse);
			_LCK_SET_PRIVATE2(&kse->k_lockusers[i], NULL);
		}
		/* _lock_init(kse->k_lock, ...) */

		if (curthread != NULL) {
			crit = _kse_critical_enter();
			KSE_LOCK_ACQUIRE(curthread->kse, &kse_lock);
		}
		kse->k_flags = 0;
		TAILQ_INSERT_TAIL(&active_kseq, kse, k_qe);
		active_kse_count++;
		if (curthread != NULL) {
			KSE_LOCK_RELEASE(curthread->kse, &kse_lock);
			_kse_critical_leave(crit);
		}
		/*
		 * Create the KSE context.
		 * Scope system threads (one thread per KSE) are not required
		 * to have a stack for an unneeded kse upcall.
		 */
		if (!sys_scope) {
			kse->k_kcb->kcb_kmbx.km_func = (kse_func_t *)kse_sched_multi;
			kse->k_stack.ss_sp = stack;
			kse->k_stack.ss_size = KSE_STACKSIZE;
		} else {
			kse->k_kcb->kcb_kmbx.km_func = (kse_func_t *)kse_sched_single;
			kse->k_stack.ss_sp = NULL;
			kse->k_stack.ss_size = 0;
		}
		kse->k_kcb->kcb_kmbx.km_udata = (void *)kse;
		kse->k_kcb->kcb_kmbx.km_quantum = 20000;
		/*
		 * We need to keep a copy of the stack in case it
		 * doesn't get used; a KSE running a scope system
		 * thread will use that thread's stack.
		 */
		kse->k_kcb->kcb_kmbx.km_stack = kse->k_stack;
	}
	return (kse);
}

static void
kse_reinit(struct kse *kse, int sys_scope)
{
	if (!sys_scope) {
		kse->k_kcb->kcb_kmbx.km_func = (kse_func_t *)kse_sched_multi;
		if (kse->k_stack.ss_sp == NULL) {
			/* XXX check allocation failure */
			kse->k_stack.ss_sp = (char *) malloc(KSE_STACKSIZE);
			kse->k_stack.ss_size = KSE_STACKSIZE;
		}
		kse->k_kcb->kcb_kmbx.km_quantum = 20000;
	} else {
		kse->k_kcb->kcb_kmbx.km_func = (kse_func_t *)kse_sched_single;
		if (kse->k_stack.ss_sp)
			free(kse->k_stack.ss_sp);
		kse->k_stack.ss_sp = NULL;
		kse->k_stack.ss_size = 0;
		kse->k_kcb->kcb_kmbx.km_quantum = 0;
	}
	kse->k_kcb->kcb_kmbx.km_stack = kse->k_stack;
	kse->k_kcb->kcb_kmbx.km_udata = (void *)kse;
	kse->k_kcb->kcb_kmbx.km_curthread = NULL;
	kse->k_kcb->kcb_kmbx.km_flags = 0;
	kse->k_curthread = NULL;
	kse->k_kseg = 0;
	kse->k_schedq = 0;
	kse->k_locklevel = 0;
	kse->k_flags = 0;
	kse->k_idle = 0;
	kse->k_error = 0;
	kse->k_cpu = 0;
	kse->k_done = 0;
	kse->k_switch = 0;
	kse->k_sigseqno = 0;
}

void
kse_free_unlocked(struct kse *kse)
{
	TAILQ_REMOVE(&active_kseq, kse, k_qe);
	active_kse_count--;
	kse->k_kseg = NULL;
	kse->k_kcb->kcb_kmbx.km_quantum = 20000;
	kse->k_flags = 0;
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
kse_destroy(struct kse *kse)
{
	int i;

	if (kse->k_stack.ss_sp != NULL)
		free(kse->k_stack.ss_sp);
	_kcb_dtor(kse->k_kcb);
	for (i = 0; i < MAX_KSE_LOCKLEVEL; ++i)
		_lockuser_destroy(&kse->k_lockusers[i]);
	_lock_destroy(&kse->k_lock);
	free(kse);
}

struct pthread *
_thr_alloc(struct pthread *curthread)
{
	kse_critical_t	crit;
	struct pthread	*thread = NULL;
	int i;

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
	if ((thread == NULL) &&
	    ((thread = malloc(sizeof(struct pthread))) != NULL)) {
		bzero(thread, sizeof(struct pthread));
		if ((thread->tcb = _tcb_ctor(thread)) == NULL) {
			free(thread);
			thread = NULL;
		} else {
			thread->siginfo = calloc(_SIG_MAXSIG,
				sizeof(siginfo_t));
			/*
			 * Initialize thread locking.
			 * Lock initializing needs malloc, so don't
			 * enter critical region before doing this!
			 */
			if (_lock_init(&thread->lock, LCK_ADAPTIVE,
			    _thr_lock_wait, _thr_lock_wakeup) != 0)
				PANIC("Cannot initialize thread lock");
			for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
				_lockuser_init(&thread->lockusers[i],
				    (void *)thread);
				_LCK_SET_PRIVATE2(&thread->lockusers[i],
				    (void *)thread);
			}
		}
	}
	return (thread);
}

void
_thr_free(struct pthread *curthread, struct pthread *thread)
{
	kse_critical_t crit;

	DBG_MSG("Freeing thread %p\n", thread);
	if (thread->name) {
		free(thread->name);
		thread->name = NULL;
	}
	if ((curthread == NULL) || (free_thread_count >= MAX_CACHED_THREADS)) {
		thr_destroy(thread);
	} else {
		/* Add the thread to the free thread list. */
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &thread_lock);
		TAILQ_INSERT_TAIL(&free_threadq, thread, tle);
		free_thread_count++;
		KSE_LOCK_RELEASE(curthread->kse, &thread_lock);
		_kse_critical_leave(crit);
	}
}

static void
thr_destroy(struct pthread *thread)
{
	int i;

	for (i = 0; i < MAX_THR_LOCKLEVEL; i++)
		_lockuser_destroy(&thread->lockusers[i]);
	_lock_destroy(&thread->lock);
	_tcb_dtor(thread->tcb);
	free(thread->siginfo);
	free(thread);
}

/*
 * Add an active thread:
 *
 *   o Assign the thread a unique id (which GDB uses to track
 *     threads.
 *   o Add the thread to the list of all threads and increment
 *     number of active threads.
 */
static void
thr_link(struct pthread *thread)
{
	kse_critical_t crit;
	struct kse *curkse;

	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
	/*
	 * Initialize the unique id (which GDB uses to track
	 * threads), add the thread to the list of all threads,
	 * and
	 */
	thread->uniqueid = next_uniqueid++;
	THR_LIST_ADD(thread);
	_thr_active_threads++;
	KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
	_kse_critical_leave(crit);
}

/*
 * Remove an active thread.
 */
static void
thr_unlink(struct pthread *thread)
{
	kse_critical_t crit;
	struct kse *curkse;

	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
	THR_LIST_REMOVE(thread);
	_thr_active_threads--;
	KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
	_kse_critical_leave(crit);
}

void
_thr_hash_add(struct pthread *thread)
{
	struct thread_hash_head *head;

	head = &thr_hashtable[THREAD_HASH(thread)];
	LIST_INSERT_HEAD(head, thread, hle);
}

void
_thr_hash_remove(struct pthread *thread)
{
	LIST_REMOVE(thread, hle);
}

struct pthread *
_thr_hash_find(struct pthread *thread)
{
	struct pthread *td;
	struct thread_hash_head *head;

	head = &thr_hashtable[THREAD_HASH(thread)];
	LIST_FOREACH(td, head, hle) {
		if (td == thread)
			return (thread);
	}
	return (NULL);
}

