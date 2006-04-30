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

int
__thr_umtx_lock(volatile umtx_t *mtx, long id)
{
	while (_umtx_op((struct umtx *)mtx, UMTX_OP_LOCK, id, 0, 0))
		;
	return (0);
}

int
__thr_umtx_timedlock(volatile umtx_t *mtx, long id,
	const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	if (_umtx_op((struct umtx *)mtx, UMTX_OP_LOCK, id, 0,
		(void *)timeout) == 0)
		return (0);
	return (errno);
}

int
__thr_umtx_unlock(volatile umtx_t *mtx, long id)
{
	if (_umtx_op((struct umtx *)mtx, UMTX_OP_UNLOCK, id, 0, 0) == 0)
		return (0);
	return (errno);
}

int
_thr_umtx_wait(volatile umtx_t *mtx, long id, const struct timespec *timeout)
{
	if (timeout && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 &&
		timeout->tv_nsec <= 0)))
		return (ETIMEDOUT);
	if (_umtx_op((struct umtx *)mtx, UMTX_OP_WAIT, id, 0,
		(void*) timeout) == 0)
		return (0);
	return (errno);
}

int
_thr_umtx_wake(volatile umtx_t *mtx, int nr_wakeup)
{
	if (_umtx_op((struct umtx *)mtx, UMTX_OP_WAKE, nr_wakeup, 0, 0) == 0)
		return (0);
	return (errno);
}
