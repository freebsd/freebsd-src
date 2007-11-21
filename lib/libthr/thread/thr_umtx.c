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
 *
 */

#include "thr_private.h"
#include "thr_umtx.h"

void
_thr_umutex_init(struct umutex *mtx)
{
	static struct umutex default_mtx = DEFAULT_UMUTEX;

	*mtx = default_mtx;
}

int
__thr_umutex_lock(struct umutex *mtx)
{
	if (_umtx_op(mtx, UMTX_OP_MUTEX_LOCK, 0, 0, 0) != -1)
		return 0;
	return (errno);
}

int
__thr_umutex_timedlock(struct umutex *mtx,
	const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	if (_umtx_op(mtx, UMTX_OP_MUTEX_LOCK, 0, 0,
		__DECONST(void *, timeout)) != -1)
		return (0);
	return (errno);
}

int
__thr_umutex_unlock(struct umutex *mtx)
{
	if (_umtx_op(mtx, UMTX_OP_MUTEX_UNLOCK, 0, 0, 0) != -1)
		return (0);
	return (errno);
}

int
__thr_umutex_trylock(struct umutex *mtx)
{
	if (_umtx_op(mtx, UMTX_OP_MUTEX_TRYLOCK, 0, 0, 0) != -1)
		return (0);
	return (errno);
}

int
__thr_umutex_set_ceiling(struct umutex *mtx, uint32_t ceiling,
	uint32_t *oldceiling)
{
	if (_umtx_op(mtx, UMTX_OP_SET_CEILING, ceiling, oldceiling, 0) != -1)
		return (0);
	return (errno);
}

int
_thr_umtx_wait(volatile long *mtx, long id, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	if (_umtx_op(__DEVOLATILE(void *, mtx), UMTX_OP_WAIT, id, 0,
		__DECONST(void*, timeout)) != -1)
		return (0);
	return (errno);
}

int
_thr_umtx_wait_uint(volatile u_int *mtx, u_int id, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	if (_umtx_op(__DEVOLATILE(void *, mtx), UMTX_OP_WAIT_UINT, id, 0,
		__DECONST(void*, timeout)) != -1)
		return (0);
	return (errno);
}

int
_thr_umtx_wake(volatile void *mtx, int nr_wakeup)
{
	if (_umtx_op(__DEVOLATILE(void *, mtx), UMTX_OP_WAKE,
		nr_wakeup, 0, 0) != -1)
		return (0);
	return (errno);
}

void
_thr_ucond_init(struct ucond *cv)
{
	bzero(cv, sizeof(struct ucond));
}

int
_thr_ucond_wait(struct ucond *cv, struct umutex *m,
	const struct timespec *timeout, int check_unparking)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
	    timeout->tv_nsec <= 0))) {
		__thr_umutex_unlock(m);
                return (ETIMEDOUT);
	}
	if (_umtx_op(cv, UMTX_OP_CV_WAIT,
		     check_unparking ? UMTX_CHECK_UNPARKING : 0, 
		     m, __DECONST(void*, timeout)) != -1) {
		return (0);
	}
	return (errno);
}
 
int
_thr_ucond_signal(struct ucond *cv)
{
	if (!cv->c_has_waiters)
		return (0);
	if (_umtx_op(cv, UMTX_OP_CV_SIGNAL, 0, NULL, NULL) != -1)
		return (0);
	return (errno);
}

int
_thr_ucond_broadcast(struct ucond *cv)
{
	if (!cv->c_has_waiters)
		return (0);
	if (_umtx_op(cv, UMTX_OP_CV_BROADCAST, 0, NULL, NULL) != -1)
		return (0);
	return (errno);
}
