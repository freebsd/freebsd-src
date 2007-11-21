/*
 * Copyright (C) 2005 David Xu <davidxu@freebsd.org>.
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
#include <sys/types.h>
#include <sys/queue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <_semaphore.h>
#include "un-namespace.h"

#include "thr_private.h"


__weak_reference(_sem_init, sem_init);
__weak_reference(_sem_destroy, sem_destroy);
__weak_reference(_sem_getvalue, sem_getvalue);
__weak_reference(_sem_trywait, sem_trywait);
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
	bzero(sem, sizeof(*sem));
	/*
	 * Fortunatly count and nwaiters are adjacency, so we can
	 * use umtx_wait to wait on it, umtx_wait needs an address
	 * can be accessed as a long interger.
	 */
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
_sem_destroy(sem_t *sem)
{
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	/*
	 * If this is a system semaphore let the kernel track it otherwise
	 * make sure there are no waiters.
	 */
	if ((*sem)->syssem != 0)
		retval = ksem_destroy((*sem)->semid);
	else {
		retval = 0;
		(*sem)->magic = 0;
	}
	if (retval == 0)
		free(*sem);
	return (retval);
}

int
_sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
		retval = ksem_getvalue((*sem)->semid, sval);
        else {
		*sval = (int)(*sem)->count;
		retval = 0;
	}
	return (retval);
}

int
_sem_trywait(sem_t *sem)
{
	int val;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
 		return (ksem_trywait((*sem)->semid));

	while ((val = (*sem)->count) > 0) {
		if (atomic_cmpset_acq_int(&(*sem)->count, val, val - 1))
			return (0);
	}
	errno = EAGAIN;
	return (-1);
}

int
_sem_wait(sem_t *sem)
{
	struct pthread *curthread;
	int val, retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	curthread = _get_curthread();
	if ((*sem)->syssem != 0) {
		_thr_cancel_enter(curthread);
		retval = ksem_wait((*sem)->semid);
		_thr_cancel_leave(curthread);
		return (retval);
	}

	_pthread_testcancel();
	do {
		while ((val = (*sem)->count) > 0) {
			if (atomic_cmpset_acq_int(&(*sem)->count, val, val - 1))
				return (0);
		}
		_thr_cancel_enter(curthread);
		retval = _thr_umtx_wait_uint(&(*sem)->count, 0, NULL);
		_thr_cancel_leave(curthread);
	} while (retval == 0);
	errno = retval;
	return (-1);
}

int
_sem_timedwait(sem_t * __restrict sem,
    const struct timespec * __restrict abstime)
{
	struct timespec ts, ts2;
	struct pthread *curthread;
	int val, retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	curthread = _get_curthread();
	if ((*sem)->syssem != 0) {
		_thr_cancel_enter(curthread);
		retval = ksem_timedwait((*sem)->semid, abstime);
		_thr_cancel_leave(curthread);
		return (retval);
	}

	/*
	 * The timeout argument is only supposed to
	 * be checked if the thread would have blocked.
	 */
	_pthread_testcancel();
	do {
		while ((val = (*sem)->count) > 0) {
			if (atomic_cmpset_acq_int(&(*sem)->count, val, val - 1))
				return (0);
		}
		if (abstime == NULL) {
			errno = EINVAL;
			return (-1);
		}
		clock_gettime(CLOCK_REALTIME, &ts);
		TIMESPEC_SUB(&ts2, abstime, &ts);
		_thr_cancel_enter(curthread);
		retval = _thr_umtx_wait_uint(&(*sem)->count, 0, &ts2);
		_thr_cancel_leave(curthread);
	} while (retval == 0);
	errno = retval;
	return (-1);
}

int
_sem_post(sem_t *sem)
{
	int val, retval;
	
	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
		return (ksem_post((*sem)->semid));

	/*
	 * sem_post() is required to be safe to call from within
	 * signal handlers, these code should work as that.
	 */
	do {
		val = (*sem)->count;
	} while (!atomic_cmpset_acq_int(&(*sem)->count, val, val + 1));
	retval = _thr_umtx_wake(&(*sem)->count, val + 1);
	if (retval > 0)
		retval = 0;
	return (retval);
}
