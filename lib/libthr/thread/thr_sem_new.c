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
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_sem_init, sem_init);
__weak_reference(_sem_destroy, sem_destroy);
__weak_reference(_sem_getvalue, sem_getvalue);
__weak_reference(_sem_trywait, sem_trywait);
__weak_reference(_sem_wait, sem_wait);
__weak_reference(_sem_timedwait, sem_timedwait);
__weak_reference(_sem_post, sem_post);

extern int _libc_sem_init(sem_t *sem, int pshared, unsigned int value);
extern int _libc_sem_destroy(sem_t *sem);
extern int _libc_sem_getvalue(sem_t * __restrict sem, int * __restrict sval);
extern int _libc_sem_trywait(sem_t *sem);
extern int _libc_sem_wait(sem_t *sem);
extern int _libc_sem_timedwait(sem_t * __restrict sem,
    const struct timespec * __restrict abstime);
extern int _libc_sem_post(sem_t *sem);

int
_sem_init(sem_t *sem, int pshared, unsigned int value)
{
	return _libc_sem_init(sem, pshared, value);
}

int
_sem_destroy(sem_t *sem)
{
	return _libc_sem_destroy(sem);
}

int
_sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{
	return _libc_sem_getvalue(sem, sval);
}

int
_sem_trywait(sem_t *sem)
{
	return _libc_sem_trywait(sem);
}

int
_sem_wait(sem_t *sem)
{
	return _libc_sem_wait(sem);
}

int
_sem_timedwait(sem_t * __restrict sem,
    const struct timespec * __restrict abstime)
{
	return _libc_sem_timedwait(sem, abstime);
}

int
_sem_post(sem_t *sem)
{
	return _libc_sem_post(sem);
}
