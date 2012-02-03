/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "un-namespace.h"

#include "thr_private.h"

/*
 * Prototypes
 */
int	__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int	__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec * abstime);
static int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
static int cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
		    const struct timespec *abstime, int cancel);
static int cond_signal_common(pthread_cond_t *cond);
static int cond_broadcast_common(pthread_cond_t *cond);

/*
 * Double underscore versions are cancellation points.  Single underscore
 * versions are not and are provided for libc internal usage (which
 * shouldn't introduce cancellation points).
 */
__weak_reference(__pthread_cond_wait, pthread_cond_wait);
__weak_reference(__pthread_cond_timedwait, pthread_cond_timedwait);

__weak_reference(_pthread_cond_init, pthread_cond_init);
__weak_reference(_pthread_cond_destroy, pthread_cond_destroy);
__weak_reference(_pthread_cond_signal, pthread_cond_signal);
__weak_reference(_pthread_cond_broadcast, pthread_cond_broadcast);

#define CV_PSHARED(cvp)	(((cvp)->__flags & USYNC_PROCESS_SHARED) != 0)

static int
cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	struct pthread_cond	*cvp;
	int	error = 0;

	if ((cvp = (pthread_cond_t)
	    calloc(1, sizeof(struct pthread_cond))) == NULL) {
		error = ENOMEM;
	} else {
		/*
		 * Initialise the condition variable structure:
		 */
		if (cond_attr == NULL || *cond_attr == NULL) {
			cvp->__clock_id = CLOCK_REALTIME;
		} else {
			if ((*cond_attr)->c_pshared)
				cvp->__flags |= USYNC_PROCESS_SHARED;
			cvp->__clock_id = (*cond_attr)->c_clockid;
		}
		*cond = cvp;
	}
	return (error);
}

static int
init_static(struct pthread *thread, pthread_cond_t *cond)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_cond_static_lock);

	if (*cond == NULL)
		ret = cond_init(cond, NULL);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_cond_static_lock);

	return (ret);
}

#define CHECK_AND_INIT_COND							\
	if (__predict_false((cvp = (*cond)) <= THR_COND_DESTROYED)) {		\
		if (cvp == THR_COND_INITIALIZER) {				\
			int ret;						\
			ret = init_static(_get_curthread(), cond);		\
			if (ret)						\
				return (ret);					\
		} else if (cvp == THR_COND_DESTROYED) {				\
			return (EINVAL);					\
		}								\
		cvp = *cond;							\
	}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{

	*cond = NULL;
	return (cond_init(cond, cond_attr));
}

int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	struct pthread_cond	*cvp;
	int			error = 0;

	if ((cvp = *cond) == THR_COND_INITIALIZER)
		error = 0;
	else if (cvp == THR_COND_DESTROYED)
		error = EINVAL;
	else {
		cvp = *cond;
		*cond = THR_COND_DESTROYED;

		/*
		 * Free the memory allocated for the condition
		 * variable structure:
		 */
		free(cvp);
	}
	return (error);
}

/*
 * Cancellation behaivor:
 *   Thread may be canceled at start, if thread is canceled, it means it
 *   did not get a wakeup from pthread_cond_signal(), otherwise, it is
 *   not canceled.
 *   Thread cancellation never cause wakeup from pthread_cond_signal()
 *   to be lost.
 */
static int
cond_wait_kernel(struct pthread_cond *cvp, struct pthread_mutex *mp,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	int		recurse;
	int		error, error2 = 0;

	error = _mutex_cv_detach(mp, &recurse);
	if (error != 0)
		return (error);

	if (cancel) {
		_thr_cancel_enter2(curthread, 0);
		error = _thr_ucond_wait((struct ucond *)&cvp->__has_kern_waiters,
			(struct umutex *)&mp->m_lock, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
		_thr_cancel_leave(curthread, 0);
	} else {
		error = _thr_ucond_wait((struct ucond *)&cvp->__has_kern_waiters,
			(struct umutex *)&mp->m_lock, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
	}

	/*
	 * Note that PP mutex and ROBUST mutex may return
	 * interesting error codes.
	 */
	if (error == 0) {
		error2 = _mutex_cv_lock(mp, recurse);
	} else if (error == EINTR || error == ETIMEDOUT) {
		error2 = _mutex_cv_lock(mp, recurse);
		if (error2 == 0 && cancel)
			_thr_testcancel(curthread);
		if (error == EINTR)
			error = 0;
	} else {
		/* We know that it didn't unlock the mutex. */
		error2 = _mutex_cv_attach(mp, recurse);
		if (error2 == 0 && cancel)
			_thr_testcancel(curthread);
	}
	return (error2 != 0 ? error2 : error);
}

/*
 * Thread waits in userland queue whenever possible, when thread
 * is signaled or broadcasted, it is removed from the queue, and
 * is saved in curthread's defer_waiters[] buffer, but won't be
 * woken up until mutex is unlocked.
 */

static int
cond_wait_user(struct pthread_cond *cvp, struct pthread_mutex *mp,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct sleepqueue *sq;
	int	recurse;
	int	error;

	if (curthread->wchan != NULL)
		PANIC("thread was already on queue.");

	if (cancel)
		_thr_testcancel(curthread);

	_sleepq_lock(cvp);
	/*
	 * set __has_user_waiters before unlocking mutex, this allows
	 * us to check it without locking in pthread_cond_signal().
	 */
	cvp->__has_user_waiters = 1; 
	curthread->will_sleep = 1;
	(void)_mutex_cv_unlock(mp, &recurse);
	curthread->mutex_obj = mp;
	_sleepq_add(cvp, curthread);
	for(;;) {
		_thr_clear_wake(curthread);
		_sleepq_unlock(cvp);

		if (cancel) {
			_thr_cancel_enter2(curthread, 0);
			error = _thr_sleep(curthread, cvp->__clock_id, abstime);
			_thr_cancel_leave(curthread, 0);
		} else {
			error = _thr_sleep(curthread, cvp->__clock_id, abstime);
		}

		_sleepq_lock(cvp);
		if (curthread->wchan == NULL) {
			error = 0;
			break;
		} else if (cancel && SHOULD_CANCEL(curthread)) {
			sq = _sleepq_lookup(cvp);
			cvp->__has_user_waiters = 
				_sleepq_remove(sq, curthread);
			_sleepq_unlock(cvp);
			curthread->mutex_obj = NULL;
			_mutex_cv_lock(mp, recurse);
			if (!THR_IN_CRITICAL(curthread))
				_pthread_exit(PTHREAD_CANCELED);
			else /* this should not happen */
				return (0);
		} else if (error == ETIMEDOUT) {
			sq = _sleepq_lookup(cvp);
			cvp->__has_user_waiters =
				_sleepq_remove(sq, curthread);
			break;
		}
	}
	_sleepq_unlock(cvp);
	curthread->mutex_obj = NULL;
	_mutex_cv_lock(mp, recurse);
	return (error);
}

static int
cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread_cond *cvp;
	struct pthread_mutex *mp;
	int	error;

	CHECK_AND_INIT_COND

	mp = *mutex;

	if ((error = _mutex_owned(curthread, mp)) != 0)
		return (error);

	if (curthread->attr.sched_policy != SCHED_OTHER ||
	    (mp->m_lock.m_flags & (UMUTEX_PRIO_PROTECT|UMUTEX_PRIO_INHERIT|
		USYNC_PROCESS_SHARED)) != 0 ||
	    (cvp->__flags & USYNC_PROCESS_SHARED) != 0)
		return cond_wait_kernel(cvp, mp, abstime, cancel);
	else
		return cond_wait_user(cvp, mp, abstime, cancel);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 0));
}

int
__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 1));
}

int
_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec * abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 0));
}

int
__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 1));
}

static int
cond_signal_common(pthread_cond_t *cond)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread *td;
	struct pthread_cond *cvp;
	struct pthread_mutex *mp;
	struct sleepqueue *sq;
	int	*waddr;
	int	pshared;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	pshared = CV_PSHARED(cvp);

	_thr_ucond_signal((struct ucond *)&cvp->__has_kern_waiters);

	if (pshared || cvp->__has_user_waiters == 0)
		return (0);

	curthread = _get_curthread();
	waddr = NULL;
	_sleepq_lock(cvp);
	sq = _sleepq_lookup(cvp);
	if (sq == NULL) {
		_sleepq_unlock(cvp);
		return (0);
	}

	td = _sleepq_first(sq);
	mp = td->mutex_obj;
	cvp->__has_user_waiters = _sleepq_remove(sq, td);
	if (mp->m_owner == curthread) {
		if (curthread->nwaiter_defer >= MAX_DEFER_WAITERS) {
			_thr_wake_all(curthread->defer_waiters,
					curthread->nwaiter_defer);
			curthread->nwaiter_defer = 0;
		}
		curthread->defer_waiters[curthread->nwaiter_defer++] =
			&td->wake_addr->value;
		mp->m_flags |= PMUTEX_FLAG_DEFERED;
	} else {
		waddr = &td->wake_addr->value;
	}
	_sleepq_unlock(cvp);
	if (waddr != NULL)
		_thr_set_wake(waddr);
	return (0);
}

struct broadcast_arg {
	struct pthread *curthread;
	unsigned int *waddrs[MAX_DEFER_WAITERS];
	int count;
};

static void
drop_cb(struct pthread *td, void *arg)
{
	struct broadcast_arg *ba = arg;
	struct pthread_mutex *mp;
	struct pthread *curthread = ba->curthread;

	mp = td->mutex_obj;
	if (mp->m_owner == curthread) {
		if (curthread->nwaiter_defer >= MAX_DEFER_WAITERS) {
			_thr_wake_all(curthread->defer_waiters,
				curthread->nwaiter_defer);
			curthread->nwaiter_defer = 0;
		}
		curthread->defer_waiters[curthread->nwaiter_defer++] =
			&td->wake_addr->value;
		mp->m_flags |= PMUTEX_FLAG_DEFERED;
	} else {
		if (ba->count >= MAX_DEFER_WAITERS) {
			_thr_wake_all(ba->waddrs, ba->count);
			ba->count = 0;
		}
		ba->waddrs[ba->count++] = &td->wake_addr->value;
	}
}

static int
cond_broadcast_common(pthread_cond_t *cond)
{
	int    pshared;
	struct pthread_cond *cvp;
	struct sleepqueue *sq;
	struct broadcast_arg ba;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	pshared = CV_PSHARED(cvp);

	_thr_ucond_broadcast((struct ucond *)&cvp->__has_kern_waiters);

	if (pshared || cvp->__has_user_waiters == 0)
		return (0);

	ba.curthread = _get_curthread();
	ba.count = 0;
	
	_sleepq_lock(cvp);
	sq = _sleepq_lookup(cvp);
	if (sq == NULL) {
		_sleepq_unlock(cvp);
		return (0);
	}
	_sleepq_drop(sq, drop_cb, &ba);
	cvp->__has_user_waiters = 0;
	_sleepq_unlock(cvp);
	if (ba.count > 0)
		_thr_wake_all(ba.waddrs, ba.count);
	return (0);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{

	return (cond_signal_common(cond));
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{

	return (cond_broadcast_common(cond));
}
