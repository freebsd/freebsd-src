/*
 * Copyright (C) 2010 David Xu <davidxu@freebsd.org>.
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <machine/atomic.h>
#include <sys/umtx.h>
#include <limits.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include "un-namespace.h"

__weak_reference(_libc_sem_close, sem_close);
__weak_reference(_libc_sem_close, _sem_close);
__weak_reference(_libc_sem_destroy, sem_destroy);
__weak_reference(_libc_sem_destroy, _sem_destroy);
__weak_reference(_libc_sem_getvalue, sem_getvalue);
__weak_reference(_libc_sem_getvalue, _sem_getvalue);
__weak_reference(_libc_sem_init, sem_init);
__weak_reference(_libc_sem_init, _sem_init);
__weak_reference(_libc_sem_open, sem_open);
__weak_reference(_libc_sem_open, _sem_open);
__weak_reference(_libc_sem_post, sem_post);
__weak_reference(_libc_sem_post, _sem_post);
__weak_reference(_libc_sem_timedwait, sem_timedwait);
__weak_reference(_libc_sem_timedwait, _sem_timedwait);
__weak_reference(_libc_sem_trywait, sem_trywait);
__weak_reference(_libc_sem_trywait, _sem_trywait);
__weak_reference(_libc_sem_unlink, sem_unlink);
__weak_reference(_libc_sem_unlink, _sem_unlink);
__weak_reference(_libc_sem_wait, sem_wait);
__weak_reference(_libc_sem_wait, _sem_wait);

#define SEM_PREFIX	"/tmp/SEMD"
#define SEM_MAGIC	((u_int32_t)0x73656d31)

struct sem_nameinfo {
	int open_count;
	char *name;
	sem_t *sem;
	LIST_ENTRY(sem_nameinfo) next;
};

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t sem_llock;
static LIST_HEAD(,sem_nameinfo) sem_list = LIST_HEAD_INITIALIZER(sem_list);

static void
sem_prefork()
{
	
	_pthread_mutex_lock(&sem_llock);
}

static void
sem_postfork()
{
	_pthread_mutex_unlock(&sem_llock);
}

static void
sem_child_postfork()
{
	_pthread_mutex_unlock(&sem_llock);
}

static void
sem_module_init(void)
{
	pthread_mutexattr_t ma;

	_pthread_mutexattr_init(&ma);
	_pthread_mutexattr_settype(&ma,  PTHREAD_MUTEX_RECURSIVE);
	_pthread_mutex_init(&sem_llock, &ma);
	_pthread_mutexattr_destroy(&ma);
	_pthread_atfork(sem_prefork, sem_postfork, sem_child_postfork);
}

static inline int
sem_check_validity(sem_t *sem)
{

	if (sem->_magic == SEM_MAGIC)
		return (0);
	else {
		errno = EINVAL;
		return (-1);
	}
}

int
_libc_sem_init(sem_t *sem, int pshared, unsigned int value)
{

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return (-1);
	}
 
	bzero(sem, sizeof(sem_t));
	sem->_magic = SEM_MAGIC;
	sem->_kern._count = (u_int32_t)value;
	sem->_kern._has_waiters = 0;
	sem->_kern._flags = pshared ? USYNC_PROCESS_SHARED : 0;
	return (0);
}

sem_t *
_libc_sem_open(const char *name, int flags, ...)
{
	char path[PATH_MAX];

	struct stat sb;
	va_list ap;
	struct sem_nameinfo *ni = NULL;
	sem_t *sem = NULL;
	int fd = -1, mode, len;

	if (name[0] != '/') {
		errno = EINVAL;
		return (NULL);
	}
	name++;

	if (flags & ~(O_CREAT|O_EXCL)) {
		errno = EINVAL;
		return (NULL);
	}

	_pthread_once(&once, sem_module_init);

	_pthread_mutex_lock(&sem_llock);
	LIST_FOREACH(ni, &sem_list, next) {
		if (strcmp(name, ni->name) == 0) {
			ni->open_count++;
			sem = ni->sem;
			_pthread_mutex_unlock(&sem_llock);
			return (sem);
		}
	}

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}

	len = sizeof(*ni) + strlen(name) + 1;
	ni = (struct sem_nameinfo *)malloc(len);
	if (ni == NULL) {
		errno = ENOSPC;
		goto error;
	}

	ni->name = (char *)(ni+1);
	strcpy(ni->name, name);

	strcpy(path, SEM_PREFIX);
	if (strlcat(path, name, sizeof(path)) >= sizeof(path)) {
		errno = ENAMETOOLONG;
		goto error;
	}

	fd = _open(path, flags|O_RDWR, mode);
	if (fd == -1)
		goto error;
	if (flock(fd, LOCK_EX) == -1)
		goto error;
	if (_fstat(fd, &sb)) {
		flock(fd, LOCK_UN);
		goto error;
	}
	if (sb.st_size < sizeof(sem_t)) {
		sem_t tmp;

		tmp._magic = SEM_MAGIC;
		tmp._kern._has_waiters = 0;
		tmp._kern._count = 0;
		tmp._kern._flags = USYNC_PROCESS_SHARED | SEM_NAMED;
		if (_write(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
			flock(fd, LOCK_UN);
			goto error;
		}
	}
	flock(fd, LOCK_UN);
	sem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE,
		MAP_SHARED|MAP_NOSYNC, fd, 0);
	if (sem == MAP_FAILED) {
		sem = NULL;
		if (errno == ENOMEM)
			errno = ENOSPC;
		goto error;
	}
	if (sem->_magic != SEM_MAGIC) {
		errno = EINVAL;
		goto error;
	}
	ni->open_count = 1;
	ni->sem = sem;
	LIST_INSERT_HEAD(&sem_list, ni, next);
	_pthread_mutex_unlock(&sem_llock);
	_close(fd);
	return (sem);

error:
	_pthread_mutex_unlock(&sem_llock);
	if (fd != -1)
		_close(fd);
	if (sem != NULL)
		munmap(sem, sizeof(sem_t));
	free(ni);
	return (SEM_FAILED);
}

int
_libc_sem_close(sem_t *sem)
{
	struct sem_nameinfo *ni;

	if (sem_check_validity(sem) != 0)
		return (-1);

	if (!(sem->_kern._flags & SEM_NAMED)) {
		errno = EINVAL;
		return (-1);
	}

	_pthread_mutex_lock(&sem_llock);
	LIST_FOREACH(ni, &sem_list, next) {
		if (sem == ni->sem) {
			if (--ni->open_count > 0) {
				_pthread_mutex_unlock(&sem_llock);
				return (0);
			}
			else
				break;
		}
	}

	if (ni) {
		LIST_REMOVE(ni, next);
		_pthread_mutex_unlock(&sem_llock);
		munmap(sem, sizeof(*sem));
		free(ni);
		return (0);
	}
	_pthread_mutex_unlock(&sem_llock);
	return (-1);
}

int
_libc_sem_unlink(const char *name)
{
	char path[PATH_MAX];

	if (name[0] != '/') {
		errno = ENOENT;
		return -1;
	}
	name++;

	strcpy(path, SEM_PREFIX);
	if (strlcat(path, name, sizeof(path)) >= sizeof(path)) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return unlink(path);
}

int
_libc_sem_destroy(sem_t *sem)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	if (sem->_kern._flags & SEM_NAMED) {
		errno = EINVAL;
		return (-1);
	}
	sem->_magic = 0;
	return (0);
}

int
_libc_sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	*sval = (int)sem->_kern._count;
	return (0);
}

static __inline int
usem_wake(struct _usem *sem)
{
	if (!sem->_has_waiters)
		return (0);
	return _umtx_op(sem, UMTX_OP_SEM_WAKE, 0, NULL, NULL);
}

static __inline int
usem_wait(struct _usem *sem, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
	    timeout->tv_nsec <= 0))) {
		errno = ETIMEDOUT;
		return (-1);
	}
	return _umtx_op(sem, UMTX_OP_SEM_WAIT, 0, NULL,
			__DECONST(void*, timeout));
}

int
_libc_sem_trywait(sem_t *sem)
{
	int val;

	if (sem_check_validity(sem) != 0)
		return (-1);

	while ((val = sem->_kern._count) > 0) {
		if (atomic_cmpset_acq_int(&sem->_kern._count, val, val - 1))
			return (0);
	}
	errno = EAGAIN;
	return (-1);
}

static void
sem_cancel_handler(void *arg)
{
	sem_t *sem = arg;

	if (sem->_kern._has_waiters && sem->_kern._count)
		usem_wake(&sem->_kern);
}

#define TIMESPEC_SUB(dst, src, val)                             \
        do {                                                    \
                (dst)->tv_sec = (src)->tv_sec - (val)->tv_sec;  \
                (dst)->tv_nsec = (src)->tv_nsec - (val)->tv_nsec; \
                if ((dst)->tv_nsec < 0) {                       \
                        (dst)->tv_sec--;                        \
                        (dst)->tv_nsec += 1000000000;           \
                }                                               \
        } while (0)


static __inline int
enable_async_cancel(void)
{
	int old;

	_pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
	return (old);
}

static __inline void
restore_async_cancel(int val)
{
	_pthread_setcanceltype(val, NULL);
}

int
_libc_sem_timedwait(sem_t * __restrict sem,
	const struct timespec * __restrict abstime)
{
	struct timespec ts, ts2;
	int val, retval, saved_cancel;

	if (sem_check_validity(sem) != 0)
		return (-1);

	retval = 0;
	for (;;) {
		while ((val = sem->_kern._count) > 0) {
			if (atomic_cmpset_acq_int(&sem->_kern._count, val, val - 1))
				return (0);
		}

		if (retval)
			break;

		/*
		 * The timeout argument is only supposed to
		 * be checked if the thread would have blocked.
		 */
		if (abstime != NULL) {
			if (abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0) {
				errno = EINVAL;
				return (-1);
			}
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
		}
		pthread_cleanup_push(sem_cancel_handler, sem);
		saved_cancel = enable_async_cancel();
		retval = usem_wait(&sem->_kern, abstime ? &ts2 : NULL);
		restore_async_cancel(saved_cancel);
		pthread_cleanup_pop(0);
	}
	return (retval);
}

int
_libc_sem_wait(sem_t *sem)
{
	return _libc_sem_timedwait(sem, NULL);
}

/*
 * POSIX:
 * The sem_post() interface is reentrant with respect to signals and may be
 * invoked from a signal-catching function. 
 * The implementation does not use lock, so it should be safe.
 */
int
_libc_sem_post(sem_t *sem)
{

	if (sem_check_validity(sem) != 0)
		return (-1);

	atomic_add_rel_int(&sem->_kern._count, 1);
	return usem_wake(&sem->_kern);
}
