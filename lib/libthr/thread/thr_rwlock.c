/*-
 * Copyright (c) 1998 Alex Nash
 * Copyright (c) 2004 Michael Telahun Makonnen
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

#include <pthread.h>
#include "thr_private.h"

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1)

/*
 * For distinguishing operations on read and write locks.
 */
enum rwlock_type {RWT_READ, RWT_WRITE};

/* Support for staticaly initialized mutexes. */
static struct umtx init_lock = UMTX_INITIALIZER;

__weak_reference(_pthread_rwlock_destroy, pthread_rwlock_destroy);
__weak_reference(_pthread_rwlock_init, pthread_rwlock_init);
__weak_reference(_pthread_rwlock_rdlock, pthread_rwlock_rdlock);
__weak_reference(_pthread_rwlock_timedrdlock, pthread_rwlock_timedrdlock);
__weak_reference(_pthread_rwlock_timedwrlock, pthread_rwlock_timedwrlock);
__weak_reference(_pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
__weak_reference(_pthread_rwlock_trywrlock, pthread_rwlock_trywrlock);
__weak_reference(_pthread_rwlock_unlock, pthread_rwlock_unlock);
__weak_reference(_pthread_rwlock_wrlock, pthread_rwlock_wrlock);

static int	insert_rwlock(struct pthread_rwlock *, enum rwlock_type);
static int	rwlock_init_static(struct pthread_rwlock **rwlock);
static int	rwlock_rdlock_common(pthread_rwlock_t *, int,
		    const struct timespec *);
static int	rwlock_wrlock_common(pthread_rwlock_t *, int,
		    const struct timespec *);

int
_pthread_rwlock_destroy (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t prwlock;

	if (rwlock == NULL || *rwlock == NULL)
		return (EINVAL);

	prwlock = *rwlock;

	if (prwlock->state != 0)
		return (EBUSY);

	pthread_mutex_destroy(&prwlock->lock);
	pthread_cond_destroy(&prwlock->read_signal);
	pthread_cond_destroy(&prwlock->write_signal);
	free(prwlock);

	*rwlock = NULL;

	return (0);
}

int
_pthread_rwlock_init (pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	pthread_rwlock_t	prwlock;
	int			ret;

	/* allocate rwlock object */
	prwlock = (pthread_rwlock_t)malloc(sizeof(struct pthread_rwlock));

	if (prwlock == NULL) {
		ret = ENOMEM;
		goto out;
	}

	/* initialize the lock */
	if ((ret = pthread_mutex_init(&prwlock->lock, NULL)) != 0)
		goto out;

	/* initialize the read condition signal */
	if ((ret = pthread_cond_init(&prwlock->read_signal, NULL)) != 0)
		goto out_readcond;

	/* initialize the write condition signal */
	if ((ret = pthread_cond_init(&prwlock->write_signal, NULL)) != 0)
		goto out_writecond;

	/* success */
	prwlock->state		 = 0;
	prwlock->blocked_writers = 0;

	*rwlock = prwlock;
	return (0);

out_writecond:
	pthread_cond_destroy(&prwlock->read_signal);
out_readcond:
	pthread_mutex_destroy(&prwlock->lock);
out:
	if (prwlock != NULL)
		free(prwlock);
	return(ret);
}

/*
 * If nonblocking is 0 this function will wait on the lock. If
 * it is greater than 0 it will return immediately with EBUSY.
 */
static int
rwlock_rdlock_common(pthread_rwlock_t *rwlock, int nonblocking,
    const struct timespec *timeout)
{
	struct rwlock_held	*rh;
	pthread_rwlock_t 	prwlock;
	int			ret;

	rh = NULL;
	if (rwlock == NULL)
		return(EINVAL);

	/*
	 * Check for validity of the timeout parameter.
	 */
	if (timeout != NULL &&
	    (timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000))
		return (EINVAL);

	if ((ret = rwlock_init_static(rwlock)) !=0 )
		return (ret);
	prwlock = *rwlock;

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	/* check lock count */
	if (prwlock->state == MAX_READ_LOCKS) {
		pthread_mutex_unlock(&prwlock->lock);
		return (EAGAIN);
	}

	/* give writers priority over readers */
	while (prwlock->blocked_writers || prwlock->state < 0) {
		if (nonblocking) {
			pthread_mutex_unlock(&prwlock->lock);
			return (EBUSY);
		}

		/*
		 * If this lock is already held for writing we have
		 * a deadlock situation.
		 */
		if (curthread->rwlockList != NULL && prwlock->state < 0) {
			LIST_FOREACH(rh, curthread->rwlockList, rh_link) {
				if (rh->rh_rwlock == prwlock &&
				    rh->rh_wrcount > 0) {
					pthread_mutex_unlock(&prwlock->lock);
					return (EDEADLK);
				}
			}
		}
		if (timeout == NULL)
			ret = pthread_cond_wait(&prwlock->read_signal,
			    &prwlock->lock);
		else
			ret = pthread_cond_timedwait(&prwlock->read_signal,
			    &prwlock->lock, timeout);

		if (ret != 0 && ret != EINTR) {
			/* can't do a whole lot if this fails */
			pthread_mutex_unlock(&prwlock->lock);
			return(ret);
		}
	}

	++prwlock->state; /* indicate we are locked for reading */
	ret = insert_rwlock(prwlock, RWT_READ);
	if (ret != 0) {
		pthread_mutex_unlock(&prwlock->lock);
		return (ret);
	}

	/*
	 * Something is really wrong if this call fails.  Returning
	 * error won't do because we've already obtained the read
	 * lock.  Decrementing 'state' is no good because we probably
	 * don't have the monitor lock.
	 */
	pthread_mutex_unlock(&prwlock->lock);

	return(0);
}

int
_pthread_rwlock_rdlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_rdlock_common(rwlock, 0, NULL));
}

int
_pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock,
    const struct timespec *timeout)
{
	return (rwlock_rdlock_common(rwlock, 0, timeout));
}

int
_pthread_rwlock_tryrdlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_rdlock_common(rwlock, 1, NULL));
}

int
_pthread_rwlock_unlock (pthread_rwlock_t *rwlock)
{
	struct rwlock_held	*rh;
	pthread_rwlock_t 	prwlock;
	int			ret;

	rh = NULL;
	if (rwlock == NULL || *rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	if (curthread->rwlockList != NULL) {
		LIST_FOREACH(rh, curthread->rwlockList, rh_link) {
			if (rh->rh_rwlock == prwlock)
				break;
		}
	}
	if (rh == NULL) {
		ret = EPERM;
		goto out;
	}
	if (prwlock->state > 0) {
		PTHREAD_ASSERT(rh->rh_wrcount == 0,
		    "write count on a readlock should be zero!");
		rh->rh_rdcount--;
		if (--prwlock->state == 0 && prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
	} else if (prwlock->state < 0) {
		PTHREAD_ASSERT(rh->rh_rdcount == 0,
		    "read count on a writelock should be zero!");
		rh->rh_wrcount--;
		prwlock->state = 0;
		if (prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
		else
			ret = pthread_cond_broadcast(&prwlock->read_signal);
	} else {
		/*
		 * No thread holds this lock. We should never get here.
		 */
		PTHREAD_ASSERT(0, "state=0 on read-write lock held by thread");
		ret = EPERM;
		goto out;
	}
	if (rh->rh_wrcount == 0 && rh->rh_rdcount == 0) {
		LIST_REMOVE(rh, rh_link);
		free(rh);
	}

out:
	/* see the comment on this in rwlock_rdlock_common */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

int
_pthread_rwlock_wrlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_wrlock_common(rwlock, 0, NULL));
}

int
_pthread_rwlock_timedwrlock (pthread_rwlock_t *rwlock,
    const struct timespec *timeout)
{
	return (rwlock_wrlock_common(rwlock, 0, timeout));
}

int
_pthread_rwlock_trywrlock (pthread_rwlock_t *rwlock)
{
	return (rwlock_wrlock_common(rwlock, 1, NULL));
}

/*
 * If nonblocking is 0 this function will wait on the lock. If
 * it is greater than 0 it will return immediately with EBUSY.
 */
static int
rwlock_wrlock_common(pthread_rwlock_t *rwlock, int nonblocking,
    const struct timespec *timeout)
{
	struct rwlock_held	*rh;
	pthread_rwlock_t 	prwlock;
	int			ret;

	rh = NULL;
	if (rwlock == NULL)
		return(EINVAL);

	/*
	 * Check the timeout value for validity.
	 */
	if (timeout != NULL &&
	    (timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000))
		return (EINVAL);

	if ((ret = rwlock_init_static(rwlock)) !=0 )
		return (ret);
	prwlock = *rwlock;

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	while (prwlock->state != 0) {
		if (nonblocking) {
			pthread_mutex_unlock(&prwlock->lock);
			return (EBUSY);
		}

		/*
		 * If this thread already holds the lock for reading
		 * or writing we have a deadlock situation.
		 */
		if (curthread->rwlockList != NULL) {
			LIST_FOREACH(rh, curthread->rwlockList, rh_link) {
				if (rh->rh_rwlock == prwlock) {
					PTHREAD_ASSERT((rh->rh_rdcount > 0 ||
					    rh->rh_wrcount > 0),
					    "Invalid 0 R/RW count!");
					pthread_mutex_unlock(&prwlock->lock);
					return (EDEADLK);
					break;
				}
			}
		}

		++prwlock->blocked_writers;

		if (timeout == NULL)
			ret = pthread_cond_wait(&prwlock->write_signal,
			    &prwlock->lock);
		else
			ret = pthread_cond_timedwait(&prwlock->write_signal,
			    &prwlock->lock, timeout);

		if (ret != 0 && ret != EINTR) {
			--prwlock->blocked_writers;
			pthread_mutex_unlock(&prwlock->lock);
			return(ret);
		}

		--prwlock->blocked_writers;
	}

	/* indicate we are locked for writing */
	prwlock->state = -1;
	ret = insert_rwlock(prwlock, RWT_WRITE);
	if (ret != 0) {
		pthread_mutex_unlock(&prwlock->lock);
		return (ret);
	}

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return(0);
}

static int
insert_rwlock(struct pthread_rwlock *prwlock, enum rwlock_type rwt)
{
	struct rwlock_held *rh;

	/*
	 * Initialize the rwlock list in the thread. Although this function
	 * may be called for many read-write locks, the initialization
	 * of the the head happens only once during the lifetime of
	 * the thread.
	 */
	if (curthread->rwlockList == NULL) {
		curthread->rwlockList =
		    (struct rwlock_listhead *)malloc(sizeof(struct rwlock_listhead));
		if (curthread->rwlockList == NULL) {
			return (ENOMEM);
		}
		LIST_INIT(curthread->rwlockList);
	}

	LIST_FOREACH(rh, curthread->rwlockList, rh_link) {
		if (rh->rh_rwlock == prwlock) {
			if (rwt == RWT_READ)
				rh->rh_rdcount++;
			else if (rwt == RWT_WRITE)
				rh->rh_wrcount++;
			return (0);
		}
	}

	/*
	 * This is the first time we're holding this lock,
	 * create a new entry.
	 */
	rh = (struct rwlock_held *)malloc(sizeof(struct rwlock_held));
	if (rh == NULL)
		return (ENOMEM);
	rh->rh_rwlock = prwlock;
	rh->rh_rdcount = 0;
	rh->rh_wrcount = 0;
	if (rwt == RWT_READ)
		rh->rh_rdcount = 1;
	else if (rwt == RWT_WRITE)
		rh->rh_wrcount = 1;
	LIST_INSERT_HEAD(curthread->rwlockList, rh, rh_link);
	return (0);
}

/*
 * There are consumers of rwlocks, inluding our own libc, that depend on
 * a PTHREAD_RWLOCK_INITIALIZER to do for rwlocks what
 * a similarly named symbol does for statically initialized mutexes.
 * This symbol was dropped in The Open Group Base Specifications Issue 6
 * and does not exist in IEEE Std 1003.1, 2003, but it should still be
 * supported for backwards compatibility.
 */
static int
rwlock_init_static(struct pthread_rwlock **rwlock)
{
	int error;

	error = 0;
	UMTX_LOCK(&init_lock);
	if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
		error = _pthread_rwlock_init(rwlock, NULL);
	UMTX_UNLOCK(&init_lock);
	return (error);
}
