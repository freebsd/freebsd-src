/* $NetBSD: atomic.h,v 1.1 2002/10/19 12:22:34 bsh Exp $ */

/*-
 * Copyright (C) 2003-2004 Olivier Houchard
 * Copyright (C) 1994-1997 Mark Brinicombe
 * Copyright (C) 1994 Brini
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of Brini may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#include <sys/types.h>
#include <machine/armreg.h>

#include <machine/acle-compat.h>

#ifndef _KERNEL
#include <machine/sysarch.h>
#else
#include <machine/cpuconf.h>
#endif

#if __ARM_ARCH >= 6
#include <machine/atomic-v6.h>
#else /* < armv6 */
#include <machine/atomic-v4.h>
#endif /* Arch >= v6 */

static __inline int
atomic_load_32(volatile uint32_t *v)
{

	return (*v);
}

static __inline void
atomic_store_32(volatile uint32_t *dst, uint32_t src)
{
	*dst = src;
}

static __inline int
atomic_load_long(volatile u_long *v)
{

	return (*v);
}

static __inline void
atomic_store_long(volatile u_long *dst, u_long src)
{
	*dst = src;
}

static __inline void
atomic_thread_fence_acq(void)
{

	dmb();
}

static __inline void
atomic_thread_fence_rel(void)
{

	dmb();
}

static __inline void
atomic_thread_fence_acq_rel(void)
{

	dmb();
}

static __inline void
atomic_thread_fence_seq_cst(void)
{

	dmb();
}

#define atomic_clear_ptr		atomic_clear_32
#define atomic_set_ptr			atomic_set_32
#define atomic_cmpset_ptr		atomic_cmpset_32
#define atomic_cmpset_rel_ptr		atomic_cmpset_rel_32
#define atomic_cmpset_acq_ptr		atomic_cmpset_acq_32
#define atomic_store_ptr		atomic_store_32
#define atomic_store_rel_ptr		atomic_store_rel_32

#define atomic_add_int			atomic_add_32
#define atomic_add_acq_int		atomic_add_acq_32
#define atomic_add_rel_int		atomic_add_rel_32
#define atomic_subtract_int		atomic_subtract_32
#define atomic_subtract_acq_int		atomic_subtract_acq_32
#define atomic_subtract_rel_int		atomic_subtract_rel_32
#define atomic_clear_int		atomic_clear_32
#define atomic_clear_acq_int		atomic_clear_acq_32
#define atomic_clear_rel_int		atomic_clear_rel_32
#define atomic_set_int			atomic_set_32
#define atomic_set_acq_int		atomic_set_acq_32
#define atomic_set_rel_int		atomic_set_rel_32
#define atomic_cmpset_int		atomic_cmpset_32
#define atomic_cmpset_acq_int		atomic_cmpset_acq_32
#define atomic_cmpset_rel_int		atomic_cmpset_rel_32
#define atomic_fetchadd_int		atomic_fetchadd_32
#define atomic_readandclear_int		atomic_readandclear_32
#define atomic_load_acq_int		atomic_load_acq_32
#define atomic_store_rel_int		atomic_store_rel_32

#endif /* _MACHINE_ATOMIC_H_ */
