/* $NetBSD: atomic.h,v 1.1 2002/10/19 12:22:34 bsh Exp $ */

/*
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



#ifndef _LOCORE

#include <sys/types.h>

#ifndef I32_bit
#define I32_bit (1 << 7)        /* IRQ disable */
#endif
#ifndef F32_bit
#define F32_bit (1 << 6)        /* FIQ disable */
#endif

#define __with_interrupts_disabled(expr) \
	do {						\
		u_int cpsr_save, tmp;			\
							\
		__asm __volatile(			\
			"mrs  %0, cpsr;"		\
			"orr  %1, %0, %2;"		\
			"msr  cpsr_all, %1;"		\
			: "=r" (cpsr_save), "=r" (tmp)	\
			: "I" (I32_bit)		\
		        : "cc" );		\
		(expr);				\
		 __asm __volatile(		\
			"msr  cpsr_all, %0"	\
			: /* no output */	\
			: "r" (cpsr_save)	\
			: "cc" );		\
	} while(0)

static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	__with_interrupts_disabled( *address |= setmask);
}

static __inline void
atomic_set_ptr(volatile void *ptr, uint32_t src)
{
	atomic_set_32((volatile uint32_t *)ptr, (uint32_t)src);
}

#define atomic_set_rel_int atomic_set_32
#define atomic_set_int atomic_set_32
#define atomic_readandclear_int atomic_readandclear_32
static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	__with_interrupts_disabled( *address &= ~clearmask);
}

static __inline void
atomic_clear_ptr(volatile void *ptr, uint32_t src)
{
	atomic_clear_32((volatile uint32_t *)ptr, (uint32_t)src);
}

static __inline int
atomic_load_acq_int(volatile uint32_t *v)
{
	int bla;

	__with_interrupts_disabled(bla = *v);
	return (bla);
}

#define atomic_clear_int atomic_clear_32
static __inline void
atomic_store_32(volatile uint32_t *dst, uint32_t src)
{
	__with_interrupts_disabled(*dst = src);
}

static __inline void
atomic_store_ptr(volatile void *dst, void *src)
{
	atomic_store_32((volatile uint32_t *)dst, (uint32_t) src);
}

#define atomic_store_rel_ptr atomic_store_ptr
#define atomic_store_rel_int atomic_store_32

static __inline uint32_t
atomic_readandclear_32(volatile u_int32_t *p)
{
	uint32_t ret;

	__with_interrupts_disabled((ret = *p) != 0 ? *p = 0 : 0);
	return (ret);
}

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
{
	int done = 0;
	__with_interrupts_disabled(*p = (*p == cmpval ? newval + done++ : *p));
	return (done);
}

static __inline void
atomic_add_32(volatile u_int32_t *p, u_int32_t val)
{
	__with_interrupts_disabled(*p += val);
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	__with_interrupts_disabled(*p -= val);
}

#define atomic_subtract_int atomic_subtract_32
#define atomic_subtract_rel_int atomic_subtract_32
#define atomic_subtract_acq_int atomic_subtract_32
#define atomic_add_int atomic_add_32
#define atomic_add_rel_int atomic_add_32
#define atomic_add_acq_int atomic_add_32
#define atomic_cmpset_int atomic_cmpset_32
#define atomic_cmpset_rel_int atomic_cmpset_32
#define atomic_cmpset_acq_int atomic_cmpset_32

static __inline u_int32_t
atomic_cmpset_ptr(volatile void *dst, void *exp, void *src)
{
	return (atomic_cmpset_32((volatile u_int32_t *)dst, (u_int32_t)exp,
                (u_int32_t)src));
}

static __inline u_int32_t
atomic_cmpset_rel_32(volatile u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
{
	return (atomic_cmpset_32(p, cmpval, newval));
}

static __inline u_int32_t
atomic_cmpset_rel_ptr(volatile void *dst, void *exp, void *src)
{
	return (atomic_cmpset_32((volatile u_int32_t *)dst, 
	    (u_int32_t)exp, (u_int32_t)src));
}

#define atomic_cmpset_acq_ptr atomic_cmpset_ptr

#if !defined(ATOMIC_SET_BIT_NOINLINE)

#define atomic_set_bit(a,m)   atomic_set_32(a,m)
#define atomic_clear_bit(a,m) atomic_clear_32(a,m)

#endif

#undef __with_interrupts_disabled

#endif /* _LOCORE */
#endif /* _MACHINE_ATOMIC_H_ */
