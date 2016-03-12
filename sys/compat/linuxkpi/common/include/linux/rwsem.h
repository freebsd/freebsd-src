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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_RWSEM_H_
#define	_LINUX_RWSEM_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

struct rw_semaphore {
	struct sx sx;
};

#define	down_write(_rw)			sx_xlock(&(_rw)->sx)
#define	up_write(_rw)			sx_xunlock(&(_rw)->sx)
#define	down_read(_rw)			sx_slock(&(_rw)->sx)
#define	up_read(_rw)			sx_sunlock(&(_rw)->sx)
#define	down_read_trylock(_rw)		!!sx_try_slock(&(_rw)->sx)
#define	down_write_trylock(_rw)		!!sx_try_xlock(&(_rw)->sx)
#define	downgrade_write(_rw)		sx_downgrade(&(_rw)->sx)
#define	down_read_nested(_rw, _sc)	down_read(_rw)

static inline void
init_rwsem(struct rw_semaphore *rw)
{

	memset(&rw->sx, 0, sizeof(rw->sx));
	sx_init_flags(&rw->sx, "lnxrwsem", SX_NOWITNESS);
}

#endif	/* _LINUX_RWSEM_H_ */
