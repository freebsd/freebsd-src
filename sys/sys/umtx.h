/*-
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
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

#ifndef _SYS_UMTX_H_
#define	_SYS_UMTX_H_

#include <sys/limits.h>

/* 
 * See pthread_*
 */

#define	UMTX_UNOWNED	0x0
#define	UMTX_CONTESTED	LONG_MIN

struct umtx {
	void	*u_owner;	/* Owner of the mutex. */
};

/* op code for _umtx_op */
#define UMTX_OP_LOCK		0
#define UMTX_OP_UNLOCK		1
#define UMTX_OP_WAIT		2
#define UMTX_OP_WAKE		3

#ifndef _KERNEL

/*
 * System calls for acquiring and releasing contested mutexes.
 */
/* deprecated becaues it can only use thread id */
int _umtx_lock(struct umtx *mtx);
/* deprecated becaues it can only use thread id */
int _umtx_unlock(struct umtx *mtx);
int _umtx_op(struct umtx *umtx, int op, long id, void *uaddr, void *uaddr2);

/*
 * Standard api.  Try uncontested acquire/release and asks the
 * kernel to resolve failures.
 */
static __inline void
umtx_init(struct umtx *umtx)
{
	umtx->u_owner = UMTX_UNOWNED;
}

static __inline long
umtx_owner(struct umtx *umtx)
{
	return ((long)umtx->u_owner & ~LONG_MIN);
}

static __inline int
umtx_lock(struct umtx *umtx, long id)
{
	if (atomic_cmpset_acq_ptr(&umtx->u_owner, (void *)UMTX_UNOWNED,
	    (void *)id) == 0)
		if (_umtx_lock(umtx) == -1)
			return (errno);
	return (0);
}

static __inline int
umtx_trylock(struct umtx *umtx, long id)
{
	if (atomic_cmpset_acq_ptr(&umtx->u_owner, (void *)UMTX_UNOWNED,
	    (void *)id) == 0)
		return (EBUSY);
	return (0);
}

static __inline int
umtx_timedlock(struct umtx *umtx, long id, const struct timespec *timeout)
{
	if (atomic_cmpset_acq_ptr(&umtx->u_owner, (void *)UMTX_UNOWNED,
	    (void *)id) == 0)
		if (_umtx_op(umtx, UMTX_OP_LOCK, id, 0, (void *)timeout) == -1)
			return (errno);
	return (0);
}

static __inline int
umtx_unlock(struct umtx *umtx, long id)
{
	if (atomic_cmpset_rel_ptr(&umtx->u_owner, (void *)id,
	    (void *)UMTX_UNOWNED) == 0)
		if (_umtx_unlock(umtx) == -1)
			return (errno);
	return (0);
}

static __inline int
umtx_wait(struct umtx *umtx, long id, const struct timespec *timeout)
{
	if (_umtx_op(umtx, UMTX_OP_WAIT, id, 0, (void *)timeout) == -1)
		return (errno);
	return (0);
}

/* Wake threads waiting on a user address. */
static __inline int
umtx_wake(struct umtx *umtx, int nr_wakeup)
{
	if (_umtx_op(umtx, UMTX_OP_WAKE, nr_wakeup, 0, 0) == -1)
		return (errno);
	return (0);
}

#endif /* !_KERNEL */
#endif /* !_SYS_UMTX_H_ */
