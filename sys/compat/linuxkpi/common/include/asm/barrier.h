/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#ifndef _LINUXKPI_ASM_BARRIER_H_
#define	_LINUXKPI_ASM_BARRIER_H_

#include <sys/types.h>
#include <machine/atomic.h>

#include <asm/atomic.h>
#include <linux/compiler.h>

/* TODO: Check other archs for atomic_thread_fence_* useability */
#if defined(__amd64__) || defined(__i386__)
#define	smp_mb()	atomic_thread_fence_seq_cst()
#define	smp_wmb()	atomic_thread_fence_rel()
#define	smp_rmb()	atomic_thread_fence_acq()
#define	smp_store_mb(x, v)	do { (void)xchg(&(x), v); } while (0)
#endif

#ifndef	smp_mb
#define	smp_mb()	mb()
#endif
#ifndef	smp_wmb
#define	smp_wmb()	wmb()
#endif
#ifndef	smp_rmb
#define	smp_rmb()	rmb()
#endif
#ifndef	smp_store_mb
#define	smp_store_mb(x, v)	do { WRITE_ONCE(x, v); smp_mb(); } while (0)
#endif

#define	smp_mb__before_atomic()	barrier()
#define	smp_mb__after_atomic()	barrier()

#endif	/* _LINUXKPI_ASM_BARRIER_H_ */
