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
#ifndef	_LINUX_MUTEX_H_
#define	_LINUX_MUTEX_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <linux/spinlock.h>

typedef struct mutex {
	struct sx sx;
} mutex_t;

#define	mutex_lock(_m)			sx_xlock(&(_m)->sx)
#define	mutex_lock_nested(_m, _s)	mutex_lock(_m)
#define	mutex_lock_interruptible(_m)	({ mutex_lock((_m)); 0; })
#define	mutex_unlock(_m)		sx_xunlock(&(_m)->sx)
#define	mutex_trylock(_m)		!!sx_try_xlock(&(_m)->sx)

#define DEFINE_MUTEX(lock)						\
	mutex_t lock;							\
	SX_SYSINIT_FLAGS(lock, &(lock).sx, "lnxmtx", SX_NOWITNESS)

static inline void
linux_mutex_init(mutex_t *m)
{

	memset(&m->sx, 0, sizeof(m->sx));
	sx_init_flags(&m->sx, "lnxmtx",  SX_NOWITNESS);
}

#define	mutex_init	linux_mutex_init

#endif	/* _LINUX_MUTEX_H_ */
