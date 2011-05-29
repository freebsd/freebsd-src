/*-
 * Copyright (c) 1998 Alex Nash
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "namespace.h"
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

__weak_reference(_pthread_rwlock_destroy, pthread_rwlock_destroy);
__weak_reference(_pthread_rwlock_init, pthread_rwlock_init);
__weak_reference(_pthread_rwlock_rdlock, pthread_rwlock_rdlock);
__weak_reference(_pthread_rwlock_timedrdlock, pthread_rwlock_timedrdlock);
__weak_reference(_pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
__weak_reference(_pthread_rwlock_trywrlock, pthread_rwlock_trywrlock);
__weak_reference(_pthread_rwlock_unlock, pthread_rwlock_unlock);
__weak_reference(_pthread_rwlock_wrlock, pthread_rwlock_wrlock);
__weak_reference(_pthread_rwlock_timedwrlock, pthread_rwlock_timedwrlock);

#define CHECK_AND_INIT_RWLOCK							\
	if (__predict_false((prwlock = (*rwlock)) <= THR_RWLOCK_DESTROYED)) {	\
		if (prwlock == THR_RWLOCK_INITIALIZER) {			\
			int ret;						\
			ret = init_static(_get_curthread(), rwlock);		\
			if (ret)						\
				return (ret);					\
		} else if (prwlock == THR_RWLOCK_DESTROYED) {			\
			return (EINVAL);					\
		}								\
		prwlock = *rwlock;						\
	}

/*
 * Prototypes
 */

static int
rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr __unused)
{
	pthread_rwlock_t prwlock;

	prwlock = (pthread_rwlock_t)calloc(1, sizeof(struct pthread_rwlock));
	if (prwlock == NULL)
		return (ENOMEM);
	*rwlock = prwlock;
	return (0);
}

int
_pthread_rwlock_destroy (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t prwlock;
	int ret;

	prwlock = *rwlock;
	if (prwlock == THR_RWLOCK_INITIALIZER)
		ret = 0;
	else if (prwlock == THR_RWLOCK_DESTROYED)
		ret = EINVAL;
	else {
		*rwlock = THR_RWLOCK_DESTROYED;

		free(prwlock);
		ret = 0;
	}
	return (ret);
}

static int
init_static(struct pthread *thread, pthread_rwlock_t *rwlock)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_rwlock_static_lock);

	if (*rwlock == THR_RWLOCK_INITIALIZER)
		ret = rwlock_init(rwlock, NULL);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_rwlock_static_lock);

	return (ret);
}

int
_pthread_rwlock_init (pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	*rwlock = NULL;
	return (rwlock_init(rwlock, attr));
}

static int
rwlock_rdlock_common(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	struct timespec ts, ts2, *tsp;
	int flags;
	int ret;

	CHECK_AND_INIT_RWLOCK

	if (curthread->rdlock_count) {
		/*
		 * To avoid having to track all the rdlocks held by
		 * a thread or all of the threads that hold a rdlock,
		 * we keep a simple count of all the rdlocks held by
		 * a thread.  If a thread holds any rdlocks it is
		 * possible that it is attempting to take a recursive
		 * rdlock.  If there are blocked writers and precedence
		 * is given to them, then that would result in the thread
		 * deadlocking.  So allowing a thread to take the rdlock
		 * when it already has one or more rdlocks avoids the
		 * deadlock.  I hope the reader can follow that logic ;-)
		 */
		flags = URWLOCK_PREFER_READER;
	} else {
		flags = 0;
	}

	/*
	 * POSIX said the validity of the abstimeout parameter need
	 * not be checked if the lock can be immediately acquired.
	 */
	ret = _thr_rwlock_tryrdlock(&prwlock->lock, flags);
	if (ret == 0) {
		curthread->rdlock_count++;
		return (ret);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	for (;;) {
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			if (ts2.tv_sec < 0 || 
			    (ts2.tv_sec == 0 && ts2.tv_nsec <= 0))
				return (ETIMEDOUT);
			tsp = &ts2;
		} else
			tsp = NULL;

		/* goto kernel and lock it */
		ret = __thr_rwlock_rdlock(&prwlock->lock, flags, tsp);
		if (ret != EINTR)
			break;

		/* if interrupted, try to lock it in userland again. */
		if (_thr_rwlock_tryrdlock(&prwlock->lock, flags) == 0) {
			ret = 0;
			break;
		}
	}
	if (ret == 0)
		curthread->rdlock_count++;
	return (ret);
}

int
_pthread_rwlock_rdlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_rdlock_common(rwlock, NULL));
}

int
_pthread_rwlock_timedrdlock (pthread_rwlock_t *rwlock,
	 const struct timespec *abstime)
{
	return (rwlock_rdlock_common(rwlock, abstime));
}

int
_pthread_rwlock_tryrdlock (pthread_rwlock_t *rwlock)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	int flags;
	int ret;

	CHECK_AND_INIT_RWLOCK

	if (curthread->rdlock_count) {
		/*
		 * To avoid having to track all the rdlocks held by
		 * a thread or all of the threads that hold a rdlock,
		 * we keep a simple count of all the rdlocks held by
		 * a thread.  If a thread holds any rdlocks it is
		 * possible that it is attempting to take a recursive
		 * rdlock.  If there are blocked writers and precedence
		 * is given to them, then that would result in the thread
		 * deadlocking.  So allowing a thread to take the rdlock
		 * when it already has one or more rdlocks avoids the
		 * deadlock.  I hope the reader can follow that logic ;-)
		 */
		flags = URWLOCK_PREFER_READER;
	} else {
		flags = 0;
	}

	ret = _thr_rwlock_tryrdlock(&prwlock->lock, flags);
	if (ret == 0)
		curthread->rdlock_count++;
	return (ret);
}

int
_pthread_rwlock_trywrlock (pthread_rwlock_t *rwlock)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	int ret;

	CHECK_AND_INIT_RWLOCK

	ret = _thr_rwlock_trywrlock(&prwlock->lock);
	if (ret == 0)
		prwlock->owner = curthread;
	return (ret);
}

static int
rwlock_wrlock_common (pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	struct timespec ts, ts2, *tsp;
	int ret;

	CHECK_AND_INIT_RWLOCK

	/*
	 * POSIX said the validity of the abstimeout parameter need
	 * not be checked if the lock can be immediately acquired.
	 */
	ret = _thr_rwlock_trywrlock(&prwlock->lock);
	if (ret == 0) {
		prwlock->owner = curthread;
		return (ret);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	for (;;) {
		if (abstime != NULL) {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			if (ts2.tv_sec < 0 || 
			    (ts2.tv_sec == 0 && ts2.tv_nsec <= 0))
				return (ETIMEDOUT);
			tsp = &ts2;
		} else
			tsp = NULL;

		/* goto kernel and lock it */
		ret = __thr_rwlock_wrlock(&prwlock->lock, tsp);
		if (ret == 0) {
			prwlock->owner = curthread;
			break;
		}

		if (ret != EINTR)
			break;

		/* if interrupted, try to lock it in userland again. */
		if (_thr_rwlock_trywrlock(&prwlock->lock) == 0) {
			ret = 0;
			prwlock->owner = curthread;
			break;
		}
	}
	return (ret);
}

int
_pthread_rwlock_wrlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_wrlock_common (rwlock, NULL));
}

int
_pthread_rwlock_timedwrlock (pthread_rwlock_t *rwlock,
    const struct timespec *abstime)
{
	return (rwlock_wrlock_common (rwlock, abstime));
}

int
_pthread_rwlock_unlock (pthread_rwlock_t *rwlock)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	int ret;
	int32_t state;

	prwlock = *rwlock;

	if (__predict_false(prwlock <= THR_RWLOCK_DESTROYED))
		return (EINVAL);

	state = prwlock->lock.rw_state;
	if (state & URWLOCK_WRITE_OWNER) {
		if (__predict_false(prwlock->owner != curthread))
			return (EPERM);
		prwlock->owner = NULL;
	}

	ret = _thr_rwlock_unlock(&prwlock->lock);
	if (ret == 0 && (state & URWLOCK_WRITE_OWNER) == 0)
		curthread->rdlock_count--;

	return (ret);
}
