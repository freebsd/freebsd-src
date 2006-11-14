/*-
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
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

#ifndef _THR_FBSD_UMTX_H_
#define _THR_FBSD_UMTX_H_

#include <sys/umtx.h>

typedef long umtx_t;

int __thr_umtx_lock(volatile umtx_t *mtx, long id) __hidden;
int __thr_umtx_timedlock(volatile umtx_t *mtx, long id,
	const struct timespec *timeout) __hidden;
int __thr_umtx_unlock(volatile umtx_t *mtx, long id) __hidden;

static inline void
_thr_umtx_init(volatile umtx_t *mtx)
{
    *mtx = 0;
}

static inline int
_thr_umtx_trylock(volatile umtx_t *mtx, long id)
{
    if (atomic_cmpset_acq_ptr((volatile uintptr_t *)mtx,
	(uintptr_t)UMTX_UNOWNED, (uintptr_t)id))
	return (0);
    return (EBUSY);
}

static inline int
_thr_umtx_lock(volatile umtx_t *mtx, long id)
{
    if (atomic_cmpset_acq_ptr((volatile uintptr_t *)mtx,
	(uintptr_t)UMTX_UNOWNED, (uintptr_t)id))
	return (0);
    return (__thr_umtx_lock(mtx, id));
}

static inline int
_thr_umtx_timedlock(volatile umtx_t *mtx, long id,
	const struct timespec *timeout)
{
    if (atomic_cmpset_acq_ptr((volatile uintptr_t *)mtx,
	(uintptr_t)UMTX_UNOWNED, (uintptr_t)id))
	return (0);
    return (__thr_umtx_timedlock(mtx, id, timeout));
}

static inline int
_thr_umtx_unlock(volatile umtx_t *mtx, long id)
{
    if (atomic_cmpset_rel_ptr((volatile uintptr_t *)mtx,
	(uintptr_t)id, (uintptr_t)UMTX_UNOWNED))
	return (0);
    return __thr_umtx_unlock(mtx, id);
}

int _thr_umtx_wait(volatile umtx_t *mtx, umtx_t exp,
	const struct timespec *timeout) __hidden;
int _thr_umtx_wake(volatile umtx_t *mtx, int count) __hidden;
#endif
