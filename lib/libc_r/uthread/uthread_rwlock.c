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

#ifdef _THREAD_SAFE
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <pthread.h>
#include "pthread_private.h"

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1)

static int init_static (pthread_rwlock_t *rwlock);

static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

static int
init_static (pthread_rwlock_t *rwlock)
{
	int ret;

	_SPINLOCK(&static_init_lock);

	if (*rwlock == NULL)
		ret = pthread_rwlock_init(rwlock, NULL);
	else
		ret = 0;

	_SPINUNLOCK(&static_init_lock);

	return(ret);
}

int
pthread_rwlock_destroy (pthread_rwlock_t *rwlock)
{
	int ret;

	if (rwlock == NULL)
		ret = EINVAL;
	else {
		pthread_rwlock_t prwlock;

		prwlock = *rwlock;

		pthread_mutex_destroy(&prwlock->lock);
		pthread_cond_destroy(&prwlock->read_signal);
		pthread_cond_destroy(&prwlock->write_signal);
		free(prwlock);

		*rwlock = NULL;

		ret = 0;
	}

	return(ret);
}

int
pthread_rwlock_init (pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	pthread_rwlock_t	prwlock;
	int			ret;

	/* allocate rwlock object */
	prwlock = (pthread_rwlock_t)malloc(sizeof(struct pthread_rwlock));

	if (prwlock == NULL)
		return(ENOMEM);

	/* initialize the lock */
	if ((ret = pthread_mutex_init(&prwlock->lock, NULL)) != 0)
		free(prwlock);
	else {
		/* initialize the read condition signal */
		ret = pthread_cond_init(&prwlock->read_signal, NULL);

		if (ret != 0) {
			pthread_mutex_destroy(&prwlock->lock);
			free(prwlock);
		} else {
			/* initialize the write condition signal */
			ret = pthread_cond_init(&prwlock->write_signal, NULL);

			if (ret != 0) {
				pthread_cond_destroy(&prwlock->read_signal);
				pthread_mutex_destroy(&prwlock->lock);
				free(prwlock);
			} else {
				/* success */
				prwlock->state		 = 0;
				prwlock->blocked_writers = 0;

				*rwlock = prwlock;
			}
		}
	}

	return(ret);
}

int
pthread_rwlock_rdlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t 	prwlock;
	int			ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	/* give writers priority over readers */
	while (prwlock->blocked_writers || prwlock->state < 0) {
		ret = pthread_cond_wait(&prwlock->read_signal, &prwlock->lock);

		if (ret != 0) {
			/* can't do a whole lot if this fails */
			pthread_mutex_unlock(&prwlock->lock);
			return(ret);
		}
	}

	/* check lock count */
	if (prwlock->state == MAX_READ_LOCKS)
		ret = EAGAIN;
	else
		++prwlock->state; /* indicate we are locked for reading */

	/*
	 * Something is really wrong if this call fails.  Returning
	 * error won't do because we've already obtained the read
	 * lock.  Decrementing 'state' is no good because we probably
	 * don't have the monitor lock.
	 */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

int
pthread_rwlock_tryrdlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t 	prwlock;
	int			ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	/* give writers priority over readers */
	if (prwlock->blocked_writers || prwlock->state < 0)
		ret = EBUSY;
	else if (prwlock->state == MAX_READ_LOCKS)
		ret = EAGAIN; /* too many read locks acquired */
	else
		++prwlock->state; /* indicate we are locked for reading */

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

int
pthread_rwlock_trywrlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t 	prwlock;
	int			ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	if (prwlock->state != 0)
		ret = EBUSY;
	else
		/* indicate we are locked for writing */
		prwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

int
pthread_rwlock_unlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t 	prwlock;
	int			ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	if (prwlock == NULL)
		return(EINVAL);

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	if (prwlock->state > 0) {
		if (--prwlock->state == 0 && prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
	} else if (prwlock->state < 0) {
		prwlock->state = 0;

		if (prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
		else
			ret = pthread_cond_broadcast(&prwlock->read_signal);
	} else
		ret = EINVAL;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

int
pthread_rwlock_wrlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t 	prwlock;
	int			ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	while (prwlock->state != 0) {
		++prwlock->blocked_writers;

		ret = pthread_cond_wait(&prwlock->write_signal, &prwlock->lock);

		if (ret != 0) {
			--prwlock->blocked_writers;
			pthread_mutex_unlock(&prwlock->lock);
			return(ret);
		}

		--prwlock->blocked_writers;
	}

	/* indicate we are locked for writing */
	prwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return(ret);
}

#endif /* _THREAD_SAFE */
