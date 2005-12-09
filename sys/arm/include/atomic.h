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

#define ARM_RAS_START	0xe0000004
#define ARM_RAS_END	0xe0000008

static __inline uint32_t
__swp(uint32_t val, volatile uint32_t *ptr)
{
	__asm __volatile("swp	%0, %2, [%3]"
	    : "=&r" (val), "=m" (*ptr)
	    : "r" (val), "r" (ptr), "m" (*ptr)
	    : "memory");
	return (val);
}


#ifdef _KERNEL
static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	__with_interrupts_disabled(*address |= setmask);
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	__with_interrupts_disabled(*address &= ~clearmask);
}

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	int ret;
	
	__with_interrupts_disabled(
	 {
	    	if (*p == cmpval) {
			*p = newval;
			ret = 1;
		} else {
			ret = 0;
		}
	});
	return (ret);
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

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t value;

	__with_interrupts_disabled(
	{
	    	value = *p;
		*p += v;
	});
	return (value);
}

#else /* !_KERNEL */

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	register int done, ras_start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "cmp	%1, %3\n"
	    "streq	%4, %2\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "moveq	%1, #1\n"
	    "movne	%1, #0\n"
	    : "=r" (ras_start), "=r" (done)
	    ,"=m" (*p), "+r" (cmpval), "+r" (newval)
	    : "m" (*p));
	return (done);
}

static __inline void
atomic_add_32(volatile u_int32_t *p, u_int32_t val)
{
	int ras_start, start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "add	%1, %1, %3\n"
	    "str	%1, %2\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    : "=r" (ras_start), "=r" (start), "=m" (*p), "+r" (val)
	    : "m" (*p));
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	int ras_start, start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "sub	%1, %1, %3\n"
	    "str	%1, %2\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"

	    : "=r" (ras_start), "=r" (start), "=m" (*p), "+r" (val)
	    : "m" (*p));
}

static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	int ras_start, start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "orr	%1, %1, %3\n"
	    "str	%1, %2\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"

	    : "=r" (ras_start), "=r" (start), "=m" (*address), "+r" (setmask)
	    : "m" (*address));
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	int ras_start, start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "bic	%1, %1, %3\n"
	    "str	%1, %2\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    : "=r" (ras_start), "=r" (start), "=m" (*address), "+r" (clearmask)
	    : "m" (*address));

}

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t ras_start, start;

	__asm __volatile("1:\n"
	    "mov	%0, #0xe0000008\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 1b\n"
	    "mov	%0, #0xe0000004\n"
	    "str	%1, [%0]\n"
	    "ldr	%1, %2\n"
	    "add	%3, %1, %3\n"
	    "str	%3, %2\n"
	    "2:\n"
	    "mov	%3, #0\n"
	    "str	%3, [%0]\n"
	    : "=r" (ras_start), "=r" (start), "=m" (*p), "+r" (v)
	    : "m" (*p));
	return (start);
}

	    
#endif /* _KERNEL */

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

static __inline uint32_t
atomic_readandclear_32(volatile u_int32_t *p)
{

	return (__swp(0, p));
}

#undef __with_interrupts_disabled

#endif /* _LOCORE */


static __inline int
atomic_cmpset_long(volatile u_long *dst, u_long exp, u_long src)
{
	return (atomic_cmpset_32((volatile u_int *)dst, (u_int)exp, 
	    (u_int)src));
}

#define atomic_set_rel_int		atomic_set_32
#define atomic_set_acq_long		atomic_set_32
#define atomic_set_int			atomic_set_32
#define atomic_readandclear_int		atomic_readandclear_32
#define atomic_clear_int		atomic_clear_32
#define atomic_clear_acq_long		atomic_clear_32
#define atomic_subtract_int		atomic_subtract_32
#define atomic_subtract_rel_int		atomic_subtract_32
#define atomic_subtract_acq_int		atomic_subtract_32
#define atomic_add_int			atomic_add_32
#define atomic_add_acq_long		atomic_add_32
#define atomic_add_rel_int		atomic_add_32
#define atomic_add_acq_int		atomic_add_32
#define atomic_cmpset_int		atomic_cmpset_32
#define atomic_cmpset_rel_int		atomic_cmpset_32
#define atomic_cmpset_rel_ptr		atomic_cmpset_ptr
#define atomic_cmpset_acq_int		atomic_cmpset_32
#define atomic_cmpset_acq_ptr		atomic_cmpset_ptr
#define atomic_cmpset_acq_long		atomic_cmpset_long
#define atomic_store_rel_ptr		atomic_store_ptr
#define atomic_store_rel_int		atomic_store_32
#define atomic_cmpset_rel_32		atomic_cmpset_32
#define atomic_cmpset_rel_ptr		atomic_cmpset_ptr
#define atomic_load_acq_int		atomic_load_32
#define	atomic_clear_ptr		atomic_clear_32
#define	atomic_store_ptr		atomic_store_32
#define	atomic_cmpset_ptr		atomic_cmpset_32
#define	atomic_set_ptr			atomic_set_32
#define	atomic_fetchadd_int		atomic_fetchadd_32

#endif /* _MACHINE_ATOMIC_H_ */
