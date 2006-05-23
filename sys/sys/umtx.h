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
 * $FreeBSD: src/sys/sys/umtx.h,v 1.4.2.1 2005/01/31 23:26:57 imp Exp $
 *
 */

#ifndef _SYS_UMTX_H_
#define	_SYS_UMTX_H_

/* 
 * See pthread_*
 */

#define	UMTX_UNOWNED	0x0

struct umtx {
	void	*u_owner;	/* Owner of the mutex. */
};

#ifndef _KERNEL

/*
 * System calls for acquiring and releasing contested mutexes.
 */
int _umtx_lock(struct umtx *mtx);
int _umtx_unlock(struct umtx *mtx);

/*
 * Standard api.  Try uncontested acquire/release and asks the
 * kernel to resolve failures.
 */
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
umtx_unlock(struct umtx *umtx, long id)
{
	if (atomic_cmpset_rel_ptr(&umtx->u_owner, (void *)id,
	    (void *)UMTX_UNOWNED) == 0)
		if (_umtx_unlock(umtx) == -1)
			return (errno);
	return (0);
}
#endif /* !_KERNEL */

#endif /* !_SYS_UMTX_H_ */
