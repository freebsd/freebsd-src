/*-
 * Copyright (c) 2015 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_SRCU_H_
#define	_LINUX_SRCU_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

struct srcu_struct {
	struct sx sx;
};

static inline int
init_srcu_struct(struct srcu_struct *srcu)
{
	sx_init(&srcu->sx, "SleepableRCU");
	return (0);
}

static inline void
cleanup_srcu_struct(struct srcu_struct *srcu)
{
	sx_destroy(&srcu->sx);
}

static inline int
srcu_read_lock(struct srcu_struct *srcu)
{
	sx_slock(&srcu->sx);
	return (0);
}

static inline void
srcu_read_unlock(struct srcu_struct *srcu, int key)
{
	sx_sunlock(&srcu->sx);
}

static inline void
synchronize_srcu(struct srcu_struct *srcu)
{
	sx_xlock(&srcu->sx);
	sx_xunlock(&srcu->sx);
}

#endif					/* _LINUX_SRCU_H_ */
