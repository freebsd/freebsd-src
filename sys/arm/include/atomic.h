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

static __inline uint32_t
__swp(uint32_t val, volatile uint32_t *ptr)
{
	__asm __volatile("swp	%0, %1, [%2]"
	    : "=r" (val) : "r" (val) , "r" (ptr) : "memory");
	return (val);
}


#define atomic_op(v, op, p) ({			\
    uint32_t e, r, s;				\
    for (e = *(volatile uint32_t *)p;; e = r) {	\
    	s = e op v;				\
    	r = __swp(s, p);			\
    	if (r == e)				\
    		break;				\
    }						\
    e;						\
})
static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	atomic_op(setmask, |, address);
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	atomic_op(clearmask, &~, address);
}

static __inline int
atomic_load_32(volatile uint32_t *v)
{

	return (__swp(*v, v));
}

static __inline void
atomic_store_32(volatile uint32_t *dst, uint32_t src)
{
	__swp(src, dst);
}

static __inline uint32_t
atomic_readandclear_32(volatile u_int32_t *p)
{

	return (__swp(0, p));
}

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
{
	uint32_t r, e;

	for (e = *p;; e = r) {
		if (*p == cmpval) {
			r = __swp(newval, p);
			if (r == e)
				return (1);
		} else
			return (0);
	}
}

static __inline void
atomic_add_32(volatile u_int32_t *p, u_int32_t val)
{
	atomic_op(val, +, p);
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	atomic_op(val, -, p);
}

#undef __with_interrupts_disabled

#endif /* _LOCORE */


#define atomic_set_rel_int		atomic_set_32
#define atomic_set_int			atomic_set_32
#define atomic_readandclear_int		atomic_readandclear_32
#define atomic_clear_int		atomic_clear_32
#define atomic_subtract_int		atomic_subtract_32
#define atomic_subtract_rel_int		atomic_subtract_32
#define atomic_subtract_acq_int		atomic_subtract_32
#define atomic_add_int			atomic_add_32
#define atomic_add_rel_int		atomic_add_32
#define atomic_add_acq_int		atomic_add_32
#define atomic_cmpset_int		atomic_cmpset_32
#define atomic_cmpset_rel_int		atomic_cmpset_32
#define atomic_cmpset_rel_ptr		atomic_cmpset_ptr
#define atomic_cmpset_acq_int		atomic_cmpset_32
#define atomic_cmpset_acq_ptr		atomic_cmpset_ptr
#define atomic_store_rel_ptr		atomic_store_ptr
#define atomic_store_rel_int		atomic_store_32
#define atomic_cmpset_rel_32		atomic_cmpset_32
#define atomic_smpset_rel_ptr		atomic_cmpset_ptr
#define atomic_load_acq_int		atomic_load_32
#define atomic_clear_ptr(ptr, bit)	atomic_clear_32( \
    (volatile uint32_t *)ptr, (uint32_t)bit)
#define atomic_store_ptr(ptr, bit)	atomic_store_32( \
    (volatile uint32_t *)ptr, (uint32_t)bit)
#define atomic_cmpset_ptr(dst, exp, s)	atomic_cmpset_32( \
    (volatile uint32_t *)dst, (uint32_t)exp, (uint32_t)s)
#define atomic_set_ptr(ptr, src)	atomic_set_32( \
    (volatile uint32_t *)ptr,  (uint32_t)src)

#endif /* _MACHINE_ATOMIC_H_ */
