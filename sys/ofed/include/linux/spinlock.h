/* $FreeBSD$ */
/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
 */
#ifndef	_LINUX_SPINLOCK_H_
#define	_LINUX_SPINLOCK_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/rwlock.h>

typedef struct {
	struct mtx m;
} spinlock_t;

#define	spin_lock(_l)		mtx_lock(&(_l)->m)
#define	spin_unlock(_l)		mtx_unlock(&(_l)->m)
#define	spin_trylock(_l)	mtx_trylock(&(_l)->m)
#define	spin_lock_nested(_l, _n) mtx_lock_flags(&(_l)->m, MTX_DUPOK)
#define	spin_lock_irq(lock)	spin_lock(lock)
#define	spin_unlock_irq(lock)	spin_unlock(lock)
#define	spin_lock_irqsave(lock, flags)   				\
    do {(flags) = 0; spin_lock(lock); } while (0)
#define	spin_unlock_irqrestore(lock, flags)				\
    do { spin_unlock(lock); } while (0)

static inline void
spin_lock_init(spinlock_t *lock)
{

	memset(&lock->m, 0, sizeof(lock->m));
	mtx_init(&lock->m, "lnxspin", NULL, MTX_DEF | MTX_NOWITNESS);
}

#define	DEFINE_SPINLOCK(lock)						\
	spinlock_t lock;						\
	MTX_SYSINIT(lock, &(lock).m, "lnxspin", MTX_DEF)

#endif	/* _LINUX_SPINLOCK_H_ */
