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

/*
 * Some notes about this implementation.
 *
 * This is mostly a simple implementation of POSIX semaphores that
 * does not need threading.  Any semaphore created is a kernel-based
 * semaphore regardless of the pshared attribute.  This is necessary
 * because libc's stub for pthread_cond_wait() doesn't really wait,
 * and it is not worth the effort impose this behavior on libc.
 *
 * All functions here are designed to be thread-safe so that a
 * threads library need not provide wrappers except to make
 * sem_wait() and sem_timedwait() cancellation points or to
 * provide a faster userland implementation for non-pshared
 * semaphores.
 *
 * Also, this implementation of semaphores cannot really support
 * real pshared semaphores.  The sem_t is an allocated object
 * and can't be seen by other processes when placed in shared
 * memory.  It should work across forks as long as the semaphore
 * is created before any forks.
 *
 * The function sem_init() should be overridden by a threads
 * library if it wants to provide a different userland version
 * of semaphores.  The functions sem_wait() and sem_timedwait()
 * need to be wrapped to provide cancellation points.  The function
 * sem_post() may need to be wrapped to be signal-safe.
 */
#include "namespace.h"
#include <sys/types.h>
#include <sys/queue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <_semaphore.h>
#include "un-namespace.h"
#include "libc_private.h"

static sem_t sem_alloc(unsigned int value, semid_t semid, int system_sem);
static void  sem_free(sem_t sem);

static LIST_HEAD(, sem) named_sems = LIST_HEAD_INITIALIZER(&named_sems);
static pthread_mutex_t named_sems_mtx = PTHREAD_MUTEX_INITIALIZER;

__weak_reference(__sem_init, sem_init);
__weak_reference(__sem_destroy, sem_destroy);
__weak_reference(__sem_open, sem_open);
__weak_reference(__sem_close, sem_close);
__weak_reference(__sem_unlink, sem_unlink);
__weak_reference(__sem_wait, sem_wait);
__weak_reference(__sem_trywait, sem_trywait);
__weak_reference(__sem_timedwait, sem_timedwait);
__weak_reference(__sem_post, sem_post);
__weak_reference(__sem_getvalue, sem_getvalue);


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
sem_free(sem_t sem)
{

	_pthread_mutex_destroy(&sem->lock);
	_pthread_cond_destroy(&sem->gtzero);
	sem->magic = 0;
	free(sem);
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

	sem->count = (u_int32_t)value;
	sem->nwaiters = 0;
	sem->magic = SEM_MAGIC;
	sem->semid = semid;
	sem->syssem = system_sem;
	sem->lock = PTHREAD_MUTEX_INITIALIZER;
	sem->gtzero = PTHREAD_COND_INITIALIZER;
	return (sem);
}

int
__sem_init(sem_t *sem, int pshared, unsigned int value)
{
	semid_t semid;

	/*
	 * We always have to create the kernel semaphore if the
	 * threads library isn't present since libc's version of
	 * pthread_cond_wait() is just a stub that doesn't really
	 * wait.
	 */
	if (ksem_init(&semid, value) != 0)
		return (-1);

	(*sem) = sem_alloc(value, semid, 1);
	if ((*sem) == NULL) {
		ksem_destroy(semid);
		return (-1);
	}
	return (0);
}

int
__sem_destroy(sem_t *sem)
{
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	_pthread_mutex_lock(&(*sem)->lock);
	/*
	 * If this is a system semaphore let the kernel track it otherwise
	 * make sure there are no waiters.
	 */
	if ((*sem)->syssem != 0)
		retval = ksem_destroy((*sem)->semid);
	else if ((*sem)->nwaiters > 0) {
		errno = EBUSY;
		retval = -1;
	}
	else {
		retval = 0;
		(*sem)->magic = 0;
	}
	_pthread_mutex_unlock(&(*sem)->lock);

	if (retval == 0) {
		_pthread_mutex_destroy(&(*sem)->lock);
		_pthread_cond_destroy(&(*sem)->gtzero);
		sem_free(*sem);
	}
	return (retval);
}

sem_t *
__sem_open(const char *name, int oflag, ...)
{
	sem_t *sem;
	sem_t s;
	semid_t semid;
	mode_t mode;
	unsigned int value;

	mode = 0;
	value = 0;

	if ((oflag & O_CREAT) != 0) {
		va_list ap;

		va_start(ap, oflag);
		mode = va_arg(ap, int);
		value = va_arg(ap, unsigned int);
		va_end(ap);
	}
	/*
	 * we can be lazy and let the kernel handle the "oflag",
	 * we'll just merge duplicate IDs into our list.
	 */
	if (ksem_open(&semid, name, oflag, mode, value) == -1)
		return (SEM_FAILED);
	/*
	 * search for a duplicate ID, we must return the same sem_t *
	 * if we locate one.
	 */
	_pthread_mutex_lock(&named_sems_mtx);
	LIST_FOREACH(s, &named_sems, entry) {
		if (s->semid == semid) {
			sem = s->backpointer;
			_pthread_mutex_unlock(&named_sems_mtx);
			return (sem);
		}
	}
	sem = (sem_t *)malloc(sizeof(*sem));
	if (sem == NULL)
		goto err;
	*sem = sem_alloc(value, semid, 1);
	if ((*sem) == NULL)
		goto err;
	LIST_INSERT_HEAD(&named_sems, *sem, entry);
	(*sem)->backpointer = sem;
	_pthread_mutex_unlock(&named_sems_mtx);
	return (sem);
err:
	_pthread_mutex_unlock(&named_sems_mtx);
	ksem_close(semid);
	if (sem != NULL) {
		if (*sem != NULL)
			sem_free(*sem);
		else
			errno = ENOSPC;
		free(sem);
	} else {
		errno = ENOSPC;
	}
	return (SEM_FAILED);
}

int
__sem_close(sem_t *sem)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem == 0) {
		errno = EINVAL;
		return (-1);
	}

	_pthread_mutex_lock(&named_sems_mtx);
	if (ksem_close((*sem)->semid) != 0) {
		_pthread_mutex_unlock(&named_sems_mtx);
		return (-1);
	}
	LIST_REMOVE((*sem), entry);
	_pthread_mutex_unlock(&named_sems_mtx);
	sem_free(*sem);
	*sem = NULL;
	free(sem);
	return (0);
}

int
__sem_unlink(const char *name)
{

	return (ksem_unlink(name));
}

int
__sem_wait(sem_t *sem)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	return (ksem_wait((*sem)->semid));
}

int
__sem_trywait(sem_t *sem)
{
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
 		retval = ksem_trywait((*sem)->semid);
	else {
		_pthread_mutex_lock(&(*sem)->lock);
		if ((*sem)->count > 0) {
			(*sem)->count--;
			retval = 0;
		} else {
			errno = EAGAIN;
			retval = -1;
		}
		_pthread_mutex_unlock(&(*sem)->lock);
	}
	return (retval);
}

int
__sem_timedwait(sem_t * __restrict sem,
    const struct timespec * __restrict abs_timeout)
{
	if (sem_check_validity(sem) != 0)
		return (-1);

	return (ksem_timedwait((*sem)->semid, abs_timeout));
}

int
__sem_post(sem_t *sem)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	return (ksem_post((*sem)->semid));
}

int
__sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{
	int retval;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if ((*sem)->syssem != 0)
		retval = ksem_getvalue((*sem)->semid, sval);
	else {
		_pthread_mutex_lock(&(*sem)->lock);
		*sval = (int)(*sem)->count;
		_pthread_mutex_unlock(&(*sem)->lock);

		retval = 0;
	}
	return (retval);
}
