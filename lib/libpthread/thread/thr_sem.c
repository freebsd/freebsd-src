/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <sys/queue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <_semaphore.h>
#include "un-namespace.h"
#include "libc_private.h"
#include "thr_private.h"


extern int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
extern int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
		struct timespec *);

__weak_reference(_sem_init, sem_init);
__weak_reference(_sem_wait, sem_wait);
__weak_reference(_sem_timedwait, sem_timedwait);
__weak_reference(_sem_post, sem_post);


static inline int
sem_check_validity(sem_t *sem)
{

	if ((sem != NULL) && ((*sem)->magic == SEM_MAGIC))
		return (0);
	else {
		errno = EINVAL;
		return (-1);
	}
}

static void
decrease_nwaiters(void *arg)
{
	sem_t *sem = (sem_t *)arg;

	(*sem)->nwaiters--;
	/*
	 * this function is called from cancellation point,
	 * the mutex should already be hold.
	 */
	_pthread_mutex_unlock(&(*sem)->lock);
}

static sem_t
sem_alloc(unsigned int value, semid_t semid, int system_sem)
{
	sem_t sem;

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return (NULL);
	}

	sem = (sem_t)malloc(sizeof(struct sem));
	if (sem == NULL) {
		errno = ENOSPC;
		return (NULL);
	}

	/*
	 * Initialize the semaphore.
	 */
	if (_pthread_mutex_init(&sem->lock, NULL) != 0) {
		free(sem);
		errno = ENOSPC;
		return (NULL);
	}

	if (_pthread_cond_init(&sem->gtzero, NULL) != 0) {
		_pthread_mutex_destroy(&sem->lock);
		free(sem);
		errno = ENOSPC;
		return (NULL);
	}

	sem->count = (u_int32_t)value;
	sem->nwaiters = 0;
	sem->magic = SEM_MAGIC;
	sem->semid = semid;
	sem->syssem = system_sem;
	return (sem);
}

int
_sem_init(sem_t *sem, int pshared, unsigned int value)
{
	semid_t semid;

	semid = (semid_t)SEM_USER;
	if ((pshared != 0) && (ksem_init(&semid, value) != 0))
		return (-1);

	(*sem) = sem_alloc(value, semid, pshared);
	if ((*sem) == NULL) {
		if (pshared != 0)
			ksem_destroy(semid);
		return (-1);
	}
	return (0);
}

int
_sem_wait(sem_t *sem)
{
	struct pthread *curthread;
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	curthread = _get_curthread();
	if ((*sem)->syssem != 0) {
		_thr_cancel_enter(curthread);
		retval = ksem_wait((*sem)->semid);
		_thr_cancel_leave(curthread, retval != 0);
	}
	else {
		pthread_testcancel();
		_pthread_mutex_lock(&(*sem)->lock);

		while ((*sem)->count <= 0) {
			(*sem)->nwaiters++;
			THR_CLEANUP_PUSH(curthread, decrease_nwaiters, sem);
			pthread_cond_wait(&(*sem)->gtzero, &(*sem)->lock);
			THR_CLEANUP_POP(curthread, 0);
			(*sem)->nwaiters--;
		}
		(*sem)->count--;

		_pthread_mutex_unlock(&(*sem)->lock);

		retval = 0;
	}
	return (retval);
}

int
_sem_timedwait(sem_t * __restrict sem,
    struct timespec * __restrict abs_timeout)
{
	struct pthread *curthread;
	int retval;
	int timeout_invalid;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0) {
		curthread = _get_curthread();
		_thr_cancel_enter(curthread);
		retval = ksem_timedwait((*sem)->semid, abs_timeout);
		_thr_cancel_leave(curthread, retval != 0);
	}
	else {
		/*
		 * The timeout argument is only supposed to
		 * be checked if the thread would have blocked.
		 * This is checked outside of the lock so a
		 * segfault on an invalid address doesn't end
		 * up leaving the mutex locked.
		 */
		pthread_testcancel();
		timeout_invalid = (abs_timeout->tv_nsec >= 1000000000) ||
		    (abs_timeout->tv_nsec < 0);
		_pthread_mutex_lock(&(*sem)->lock);

		if ((*sem)->count <= 0) {
			if (timeout_invalid) {
				_pthread_mutex_unlock(&(*sem)->lock);
				errno = EINVAL;
				return (-1);
			}
			(*sem)->nwaiters++;
			pthread_cleanup_push(decrease_nwaiters, sem);
			pthread_cond_timedwait(&(*sem)->gtzero,
			    &(*sem)->lock, abs_timeout);
			pthread_cleanup_pop(0);
			(*sem)->nwaiters--;
		}
		if ((*sem)->count == 0) {
			errno = ETIMEDOUT;
			retval = -1;
		}
		else {
			(*sem)->count--;
			retval = 0;
		}	

		_pthread_mutex_unlock(&(*sem)->lock);
	}

	return (retval);
}

int
_sem_post(sem_t *sem)
{
	struct pthread *curthread;
	int retval;
	
	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
		retval = ksem_post((*sem)->semid);
	else {
		/*
		 * sem_post() is required to be safe to call from within
		 * signal handlers.  Thus, we must enter a critical region.
		 */
		curthread = _get_curthread();
		_thr_critical_enter(curthread);
		_pthread_mutex_lock(&(*sem)->lock);

		(*sem)->count++;
		if ((*sem)->nwaiters > 0)
			_pthread_cond_signal(&(*sem)->gtzero);

		_pthread_mutex_unlock(&(*sem)->lock);
		_thr_critical_leave(curthread);
		retval = 0;
	}

	return (retval);
}
