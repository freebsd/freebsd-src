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

#ifndef HAS__UMTX_OP_ERR
int _umtx_op_err(void *obj, int op, u_long val, void *uaddr, void *uaddr2)
{
	if (_umtx_op(obj, op, val, uaddr, uaddr2) == -1)
		return (errno);
	return (0);
}
#endif

void
_thr_umutex_init(struct umutex *mtx)
{
	static struct umutex default_mtx = DEFAULT_UMUTEX;

	*mtx = default_mtx;
}

int
__thr_umutex_lock(struct umutex *mtx)
{
	return _umtx_op(mtx, UMTX_OP_MUTEX_LOCK, 0, 0, 0);
}

int
__thr_umutex_timedlock(struct umutex *mtx,
	const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);

	return _umtx_op_err(mtx, UMTX_OP_MUTEX_LOCK, 0, 0,
		__DECONST(void *, timeout));
}

int
__thr_umutex_unlock(struct umutex *mtx)
{
	return _umtx_op_err(mtx, UMTX_OP_MUTEX_UNLOCK, 0, 0, 0);
}

int
__thr_umutex_trylock(struct umutex *mtx)
{
	return _umtx_op_err(mtx, UMTX_OP_MUTEX_TRYLOCK, 0, 0, 0);
}

int
__thr_umutex_set_ceiling(struct umutex *mtx, uint32_t ceiling,
	uint32_t *oldceiling)
{
	return _umtx_op_err(mtx, UMTX_OP_SET_CEILING, ceiling, oldceiling, 0);
}

int
_thr_umtx_wait(volatile long *mtx, long id, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	return _umtx_op_err(__DEVOLATILE(void *, mtx), UMTX_OP_WAIT, id, 0,
		__DECONST(void*, timeout));
}

int
_thr_umtx_wait_uint(volatile u_int *mtx, u_int id, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	return _umtx_op_err(__DEVOLATILE(void *, mtx), UMTX_OP_WAIT_UINT, id, 0,
		__DECONST(void*, timeout));
}

int
_thr_umtx_wake(volatile long *mtx, int nr_wakeup)
{
	return _umtx_op_err(__DEVOLATILE(void *, mtx), UMTX_OP_WAKE,
		nr_wakeup, 0, 0);
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
	return _umtx_op_err(cv, UMTX_OP_CV_WAIT,
		     check_unparking ? UMTX_CHECK_UNPARKING : 0, 
		     m, __DECONST(void*, timeout));
}
 
int
_thr_ucond_signal(struct ucond *cv)
{
	if (!cv->c_has_waiters)
		return (0);
	return _umtx_op_err(cv, UMTX_OP_CV_SIGNAL, 0, NULL, NULL);
}

int
_thr_ucond_broadcast(struct ucond *cv)
{
	if (!cv->c_has_waiters)
		return (0);
	return _umtx_op_err(cv, UMTX_OP_CV_BROADCAST, 0, NULL, NULL);
}

int
__thr_rwlock_rdlock(struct urwlock *rwlock, int flags, struct timespec *tsp)
{
	return _umtx_op_err(rwlock, UMTX_OP_RW_RDLOCK, flags, NULL, tsp);
}

int
__thr_rwlock_wrlock(struct urwlock *rwlock, struct timespec *tsp)
{
	return _umtx_op_err(rwlock, UMTX_OP_RW_WRLOCK, 0, NULL, tsp);
}

int
__thr_rwlock_unlock(struct urwlock *rwlock)
{
	return _umtx_op_err(rwlock, UMTX_OP_RW_UNLOCK, 0, NULL, NULL);
}
