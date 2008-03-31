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

#define RWLOCK_WRITE_OWNER	0x80000000U
#define RWLOCK_WRITE_WAITERS	0x40000000U
#define RWLOCK_READ_WAITERS	0x20000000U
#define RWLOCK_MAX_READERS	0x1fffffffU
#define RWLOCK_READER_COUNT(c)	((c) & RWLOCK_MAX_READERS)

__weak_reference(_pthread_rwlock_destroy, pthread_rwlock_destroy);
__weak_reference(_pthread_rwlock_init, pthread_rwlock_init);
__weak_reference(_pthread_rwlock_rdlock, pthread_rwlock_rdlock);
__weak_reference(_pthread_rwlock_timedrdlock, pthread_rwlock_timedrdlock);
__weak_reference(_pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
__weak_reference(_pthread_rwlock_trywrlock, pthread_rwlock_trywrlock);
__weak_reference(_pthread_rwlock_unlock, pthread_rwlock_unlock);
__weak_reference(_pthread_rwlock_wrlock, pthread_rwlock_wrlock);
__weak_reference(_pthread_rwlock_timedwrlock, pthread_rwlock_timedwrlock);

/*
 * Prototypes
 */

static int
rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr __unused)
{
	pthread_rwlock_t prwlock;
	int ret;

	/* allocate rwlock object */
	prwlock = (pthread_rwlock_t)malloc(sizeof(struct pthread_rwlock));

	if (prwlock == NULL)
		return (ENOMEM);

	/* initialize the lock */
	if ((ret = _pthread_mutex_init(&prwlock->lock, NULL)) != 0)
		free(prwlock);
	else {
		/* initialize the read condition signal */
		ret = _pthread_cond_init(&prwlock->read_signal, NULL);

		if (ret != 0) {
			_pthread_mutex_destroy(&prwlock->lock);
			free(prwlock);
		} else {
			/* initialize the write condition signal */
			ret = _pthread_cond_init(&prwlock->write_signal, NULL);

			if (ret != 0) {
				_pthread_cond_destroy(&prwlock->read_signal);
				_pthread_mutex_destroy(&prwlock->lock);
				free(prwlock);
			} else {
				/* success */
				prwlock->state = 0;
				prwlock->blocked_readers = 0;
				prwlock->blocked_writers = 0;
				prwlock->owner = NULL;
				*rwlock = prwlock;
			}
		}
	}

	return (ret);
}

int
_pthread_rwlock_destroy (pthread_rwlock_t *rwlock)
{
	int ret;

	if (rwlock == NULL)
		ret = EINVAL;
	else {
		pthread_rwlock_t prwlock;

		prwlock = *rwlock;
		*rwlock = NULL;

		_pthread_mutex_destroy(&prwlock->lock);
		_pthread_cond_destroy(&prwlock->read_signal);
		_pthread_cond_destroy(&prwlock->write_signal);
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

	if (*rwlock == NULL)
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

static inline int
rwlock_tryrdlock(struct pthread_rwlock *prwlock, int prefer_reader)
{
	int32_t state;
	int32_t wrflags;
 
	if (prefer_reader)
		wrflags = RWLOCK_WRITE_OWNER;
	else
		wrflags = RWLOCK_WRITE_OWNER | RWLOCK_WRITE_WAITERS;
	state = prwlock->state;
        while (!(state & wrflags)) {
		if (RWLOCK_READER_COUNT(state) == RWLOCK_MAX_READERS)
			return (EAGAIN);
		if (atomic_cmpset_acq_32(&prwlock->state, state, state + 1))
			return (0);
		CPU_SPINWAIT;
		state = prwlock->state;
	}

	return (EBUSY);
}

static int
rwlock_rdlock_common(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	const int prefer_read = curthread->rdlock_count > 0;
	pthread_rwlock_t prwlock;
	int ret, wrflags, old;
	int32_t state;

	if (__predict_false(rwlock == NULL))
		return (EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (__predict_false(prwlock == NULL)) {
		if ((ret = init_static(curthread, rwlock)) != 0)
			return (ret);

		prwlock = *rwlock;
	}

	/*
	 * POSIX said the validity of the abstimeout parameter need
	 * not be checked if the lock can be immediately acquired.
	 */
	ret = rwlock_tryrdlock(prwlock, prefer_read);
	if (ret == 0) {
		curthread->rdlock_count++;
		return (ret);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	if (prefer_read) {
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

		wrflags = RWLOCK_WRITE_OWNER;
	} else
		wrflags = RWLOCK_WRITE_OWNER | RWLOCK_WRITE_WAITERS;

	/* reset to zero */
	ret = 0;
	for (;;) {
		_pthread_mutex_lock(&prwlock->lock);
		state = prwlock->state;
		/* set read contention bit */
		while ((state & wrflags) && !(state & RWLOCK_READ_WAITERS)) {
			if (atomic_cmpset_acq_32(&prwlock->state, state, state | RWLOCK_READ_WAITERS))
				break;
			CPU_SPINWAIT;
			state = prwlock->state;
		}
 
		atomic_add_32(&prwlock->blocked_readers, 1);
		if (state & wrflags) {
			ret = _pthread_cond_wait_unlocked(&prwlock->read_signal, &prwlock->lock, abstime);
			old = atomic_fetchadd_32(&prwlock->blocked_readers, -1);
			if (old == 1)
				_pthread_mutex_lock(&prwlock->lock);
			else
				goto try_it;
		} else {
			atomic_subtract_32(&prwlock->blocked_readers, 1);
		}

		if (prwlock->blocked_readers == 0)
			atomic_clear_32(&prwlock->state, RWLOCK_READ_WAITERS);
		_pthread_mutex_unlock(&prwlock->lock);

try_it:
		/* try to lock it again. */
		if (rwlock_tryrdlock(prwlock, prefer_read) == 0) {
			curthread->rdlock_count++;
			ret = 0;
			break;
		}

		if (ret)
			break;
	}
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
	int ret;

	if (__predict_false(rwlock == NULL))
		return (EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (__predict_false(prwlock == NULL)) {
		if ((ret = init_static(curthread, rwlock)) != 0)
			return (ret);

		prwlock = *rwlock;
	}

	ret = rwlock_tryrdlock(prwlock, curthread->rdlock_count > 0);
	if (ret == 0)
		curthread->rdlock_count++;
	return (ret);
}

static inline int
rwlock_trywrlock(struct pthread_rwlock *prwlock)
{
	int32_t state;

	state = prwlock->state;
	while (!(state & RWLOCK_WRITE_OWNER) && RWLOCK_READER_COUNT(state) == 0) {
		if (atomic_cmpset_acq_32(&prwlock->state, state, state | RWLOCK_WRITE_OWNER))
			return (0);
		CPU_SPINWAIT;
		state = prwlock->state;
	}
	return (EBUSY);
}

int
_pthread_rwlock_trywrlock (pthread_rwlock_t *rwlock)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	int ret;

	if (__predict_false(rwlock == NULL))
		return (EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (__predict_false(prwlock == NULL)) {
		if ((ret = init_static(curthread, rwlock)) != 0)
			return (ret);

		prwlock = *rwlock;
	}

	ret = rwlock_trywrlock(prwlock);
	if (ret == 0)
		prwlock->owner = curthread;
	return (ret);
}

static int
rwlock_wrlock_common (pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	pthread_rwlock_t prwlock;
	int ret;
	int32_t state;

	if (__predict_false(rwlock == NULL))
		return (EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (__predict_false(prwlock == NULL)) {
		if ((ret = init_static(curthread, rwlock)) != 0)
			return (ret);

		prwlock = *rwlock;
	}

	/*
	 * POSIX said the validity of the abstimeout parameter need
	 * not be checked if the lock can be immediately acquired.
	 */

	/* try to lock it in userland */
	ret = rwlock_trywrlock(prwlock);
	if (ret == 0) {
		prwlock->owner = curthread;
		return (ret);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	/* reset to zero */
	ret = 0;

	for (;;) {
		_pthread_mutex_lock(&prwlock->lock);
		state = prwlock->state;
		while (((state & RWLOCK_WRITE_OWNER) || RWLOCK_READER_COUNT(state) != 0) &&
			(state & RWLOCK_WRITE_WAITERS) == 0) {
			if (atomic_cmpset_acq_32(&prwlock->state, state, state | RWLOCK_WRITE_WAITERS))
				break;
			CPU_SPINWAIT;
			state = prwlock->state;
		}

		prwlock->blocked_writers++;

		while ((state & RWLOCK_WRITE_OWNER) || RWLOCK_READER_COUNT(state) != 0) {
			if (abstime == NULL)
				ret = _pthread_cond_wait(&prwlock->write_signal, &prwlock->lock);
			else
				ret = _pthread_cond_timedwait(&prwlock->write_signal, &prwlock->lock, abstime);

			if (ret)
				break;
			state = prwlock->state;
		}

		prwlock->blocked_writers--;
		if (prwlock->blocked_writers == 0)
			atomic_clear_32(&prwlock->state, RWLOCK_WRITE_WAITERS);
		_pthread_mutex_unlock(&prwlock->lock);

		if (rwlock_trywrlock(prwlock) == 0) {
			prwlock->owner = curthread;
			ret = 0;
			break;
		}

		if (ret)
			break;
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
	int32_t state;

	if (__predict_false(rwlock == NULL))
		return (EINVAL);

	prwlock = *rwlock;

	if (__predict_false(prwlock == NULL))
		return (EINVAL);

	state = prwlock->state;

	if (state & RWLOCK_WRITE_OWNER) {
		if (__predict_false(prwlock->owner != curthread))
			return (EPERM);
		prwlock->owner = NULL;
		while (!atomic_cmpset_rel_32(&prwlock->state, state, state & ~RWLOCK_WRITE_OWNER)) {
			CPU_SPINWAIT;
			state = prwlock->state;
		}
	} else if (RWLOCK_READER_COUNT(state) != 0) {
		while (!atomic_cmpset_rel_32(&prwlock->state, state, state - 1)) {
			CPU_SPINWAIT;
			state = prwlock->state;
			if (RWLOCK_READER_COUNT(state) == 0)
				return (EPERM);
		}
		curthread->rdlock_count--;
        } else {
		return (EPERM);
	}

#if 0
	if (state & RWLOCK_WRITE_WAITERS) {
		_pthread_mutex_lock(&prwlock->lock);
		_pthread_cond_signal(&prwlock->write_signal);
		_pthread_mutex_unlock(&prwlock->lock);
	} else if (state & RWLOCK_READ_WAITERS) {
		_pthread_mutex_lock(&prwlock->lock);
		_pthread_cond_broadcast(&prwlock->read_signal);
		_pthread_mutex_unlock(&prwlock->lock);
	}
#endif

	if (state & RWLOCK_WRITE_WAITERS) {
		_pthread_mutex_lock(&prwlock->lock);
		_pthread_cond_broadcast_unlock(&prwlock->write_signal, &prwlock->lock, 0);
	} else if (state & RWLOCK_READ_WAITERS) {
		_pthread_mutex_lock(&prwlock->lock);
		_pthread_cond_broadcast_unlock(&prwlock->write_signal, &prwlock->lock, 1);
	}
	return (0);
}
