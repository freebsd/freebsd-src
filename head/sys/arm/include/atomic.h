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

#ifndef _KERNEL
#include <machine/sysarch.h>
#else
#include <machine/cpuconf.h>
#endif

#if defined (__ARM_ARCH_7__) || defined (__ARM_ARCH_7A__)
#define isb()  __asm __volatile("isb" : : : "memory")
#define dsb()  __asm __volatile("dsb" : : : "memory")
#define dmb()  __asm __volatile("dmb" : : : "memory")
#elif defined (__ARM_ARCH_6__) || defined (__ARM_ARCH_6J__) || \
  defined (__ARM_ARCH_6K__) || defined (__ARM_ARCH_6T2__) || \
  defined (__ARM_ARCH_6Z__) || defined (__ARM_ARCH_6ZK__)
#define isb()  __asm __volatile("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory")
#define dsb()  __asm __volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
#define dmb()  __asm __volatile("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")
#else
#define isb()  __asm __volatile("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory")
#define dsb()  __asm __volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
#define dmb()  dsb()
#endif

#define mb()   dmb()
#define wmb()  dmb()
#define rmb()  dmb()

#ifndef I32_bit
#define I32_bit (1 << 7)        /* IRQ disable */
#endif
#ifndef F32_bit
#define F32_bit (1 << 6)        /* FIQ disable */
#endif

/*
 * It would be nice to use _HAVE_ARMv6_INSTRUCTIONS from machine/asm.h
 * here, but that header can't be included here because this is C
 * code.  I would like to move the _HAVE_ARMv6_INSTRUCTIONS definition
 * out of asm.h so it can be used in both asm and C code. - kientzle@
 */
#if defined (__ARM_ARCH_7__) || \
	defined (__ARM_ARCH_7A__)  || \
	defined (__ARM_ARCH_6__)   || \
	defined (__ARM_ARCH_6J__)  || \
	defined (__ARM_ARCH_6K__)  || \
	defined (__ARM_ARCH_6T2__) || \
	defined (__ARM_ARCH_6Z__)  || \
	defined (__ARM_ARCH_6ZK__)
#define	ARM_HAVE_ATOMIC64

static __inline void
__do_dmb(void)
{

#if defined (__ARM_ARCH_7__) || defined (__ARM_ARCH_7A__)
	__asm __volatile("dmb" : : : "memory");
#else
	__asm __volatile("mcr p15, 0, r0, c7, c10, 5" : : : "memory");
#endif
}

#define ATOMIC_ACQ_REL_LONG(NAME)					\
static __inline void							\
atomic_##NAME##_acq_long(__volatile u_long *p, u_long v)		\
{									\
	atomic_##NAME##_long(p, v);					\
	__do_dmb();							\
}									\
									\
static __inline  void							\
atomic_##NAME##_rel_long(__volatile u_long *p, u_long v)		\
{									\
	__do_dmb();							\
	atomic_##NAME##_long(p, v);					\
}

#define	ATOMIC_ACQ_REL(NAME, WIDTH)					\
static __inline  void							\
atomic_##NAME##_acq_##WIDTH(__volatile uint##WIDTH##_t *p, uint##WIDTH##_t v)\
{									\
	atomic_##NAME##_##WIDTH(p, v);					\
	__do_dmb();							\
}									\
									\
static __inline  void							\
atomic_##NAME##_rel_##WIDTH(__volatile uint##WIDTH##_t *p, uint##WIDTH##_t v)\
{									\
	__do_dmb();							\
	atomic_##NAME##_##WIDTH(p, v);					\
}

static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	uint32_t tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "orr %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "=&r" (tmp), "+r" (tmp2)
			   , "+r" (address), "+r" (setmask) : : "cc", "memory");
			     
}

static __inline void
atomic_set_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   orr      %Q[tmp], %Q[val]\n"
		"   orr      %R[tmp], %R[val]\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [exf]    "=&r"  (exflag), 
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [val]    "r"    (val)
		:   "cc", "memory");
}

static __inline void
atomic_set_long(volatile u_long *address, u_long setmask)
{
	u_long tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "orr %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "=&r" (tmp), "+r" (tmp2)
			   , "+r" (address), "+r" (setmask) : : "cc", "memory");
			     
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t setmask)
{
	uint32_t tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "bic %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "=&r" (tmp), "+r" (tmp2)
			   ,"+r" (address), "+r" (setmask) : : "cc", "memory");
}

static __inline void
atomic_clear_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   bic      %Q[tmp], %Q[val]\n"
		"   bic      %R[tmp], %R[val]\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [exf]    "=&r"  (exflag), 
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [val]    "r"    (val)
		:   "cc", "memory");
}

static __inline void
atomic_clear_long(volatile u_long *address, u_long setmask)
{
	u_long tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "bic %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "=&r" (tmp), "+r" (tmp2)
			   ,"+r" (address), "+r" (setmask) : : "cc", "memory");
}

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	uint32_t ret;
	
	__asm __volatile("1: ldrex %0, [%1]\n"
	                 "cmp %0, %2\n"
	                 "itt ne\n"
			 "movne %0, #0\n"
			 "bne 2f\n"
			 "strex %0, %3, [%1]\n"
			 "cmp %0, #0\n"
	                 "ite eq\n"
			 "moveq %0, #1\n"
			 "bne	1b\n"
			 "2:"
			 : "=&r" (ret)
			 ,"+r" (p), "+r" (cmpval), "+r" (newval) : : "cc",
			 "memory");
	return (ret);
}

static __inline int
atomic_cmpset_64(volatile uint64_t *p, uint64_t cmpval, uint64_t newval)
{
	uint64_t tmp;
	uint32_t ret;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   teq      %Q[tmp], %Q[cmpval]\n"
		"   itee eq  \n"
		"   teqeq    %R[tmp], %R[cmpval]\n"
		"   movne    %[ret], #0\n"
		"   bne      2f\n"
		"   strexd   %[ret], %[newval], [%[ptr]]\n"
		"   teq      %[ret], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		"   mov      %[ret], #1\n"
		"2:          \n"
		:   [ret]    "=&r"  (ret), 
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [cmpval] "r"    (cmpval), 
		    [newval] "r"    (newval)
		:   "cc", "memory");
	return (ret);
}

static __inline u_long
atomic_cmpset_long(volatile u_long *p, volatile u_long cmpval, volatile u_long newval)
{
	u_long ret;
	
	__asm __volatile("1: ldrex %0, [%1]\n"
	                 "cmp %0, %2\n"
	                 "itt ne\n"
			 "movne %0, #0\n"
			 "bne 2f\n"
			 "strex %0, %3, [%1]\n"
			 "cmp %0, #0\n"
	                 "ite eq\n"
			 "moveq %0, #1\n"
			 "bne	1b\n"
			 "2:"
			 : "=&r" (ret)
			 ,"+r" (p), "+r" (cmpval), "+r" (newval) : : "cc",
			 "memory");
	return (ret);
}

static __inline u_int32_t
atomic_cmpset_acq_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	u_int32_t ret = atomic_cmpset_32(p, cmpval, newval);

	__do_dmb();
	return (ret);
}

static __inline uint64_t
atomic_cmpset_acq_64(volatile uint64_t *p, volatile uint64_t cmpval, volatile uint64_t newval)
{
	uint64_t ret = atomic_cmpset_64(p, cmpval, newval);

	__do_dmb();
	return (ret);
}

static __inline u_long
atomic_cmpset_acq_long(volatile u_long *p, volatile u_long cmpval, volatile u_long newval)
{
	u_long ret = atomic_cmpset_long(p, cmpval, newval);

	__do_dmb();
	return (ret);
}

static __inline u_int32_t
atomic_cmpset_rel_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	
	__do_dmb();
	return (atomic_cmpset_32(p, cmpval, newval));
}

static __inline uint64_t
atomic_cmpset_rel_64(volatile uint64_t *p, volatile uint64_t cmpval, volatile uint64_t newval)
{
	
	__do_dmb();
	return (atomic_cmpset_64(p, cmpval, newval));
}

static __inline u_long
atomic_cmpset_rel_long(volatile u_long *p, volatile u_long cmpval, volatile u_long newval)
{
	
	__do_dmb();
	return (atomic_cmpset_long(p, cmpval, newval));
}


static __inline void
atomic_add_32(volatile u_int32_t *p, u_int32_t val)
{
	uint32_t tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "add %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			    : "=&r" (tmp), "+r" (tmp2)
			    ,"+r" (p), "+r" (val) : : "cc", "memory");
}

static __inline void
atomic_add_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   adds     %Q[tmp], %Q[val]\n"
		"   adc      %R[tmp], %R[val]\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [exf]    "=&r"  (exflag), 
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [val]    "r"    (val)
		:   "cc", "memory");
}

static __inline void
atomic_add_long(volatile u_long *p, u_long val)
{
	u_long tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "add %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			    : "=&r" (tmp), "+r" (tmp2)
			    ,"+r" (p), "+r" (val) : : "cc", "memory");
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	uint32_t tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "sub %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			    : "=&r" (tmp), "+r" (tmp2)
			    ,"+r" (p), "+r" (val) : : "cc", "memory");
}

static __inline void
atomic_subtract_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   subs     %Q[tmp], %Q[val]\n"
		"   sbc      %R[tmp], %R[val]\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [exf]    "=&r"  (exflag), 
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [val]    "r"    (val)
		:   "cc", "memory");
}

static __inline void
atomic_subtract_long(volatile u_long *p, u_long val)
{
	u_long tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%2]\n"
	    		    "sub %0, %0, %3\n"
			    "strex %1, %0, [%2]\n"
			    "cmp %1, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			    : "=&r" (tmp), "+r" (tmp2)
			    ,"+r" (p), "+r" (val) : : "cc", "memory");
}

ATOMIC_ACQ_REL(clear, 32)
ATOMIC_ACQ_REL(add, 32)
ATOMIC_ACQ_REL(subtract, 32)
ATOMIC_ACQ_REL(set, 32)
ATOMIC_ACQ_REL(clear, 64)
ATOMIC_ACQ_REL(add, 64)
ATOMIC_ACQ_REL(subtract, 64)
ATOMIC_ACQ_REL(set, 64)
ATOMIC_ACQ_REL_LONG(clear)
ATOMIC_ACQ_REL_LONG(add)
ATOMIC_ACQ_REL_LONG(subtract)
ATOMIC_ACQ_REL_LONG(set)

#undef ATOMIC_ACQ_REL
#undef ATOMIC_ACQ_REL_LONG

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp = 0, tmp2 = 0, ret = 0;

	__asm __volatile("1: ldrex %0, [%3]\n"
	    		    "add %1, %0, %4\n"
			    "strex %2, %1, [%3]\n"
			    "cmp %2, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "+r" (ret), "=&r" (tmp), "+r" (tmp2)
			   ,"+r" (p), "+r" (val) : : "cc", "memory");
	return (ret);
}

static __inline uint32_t
atomic_readandclear_32(volatile u_int32_t *p)
{
	uint32_t ret, tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%3]\n"
	    		 "mov %1, #0\n"
			 "strex %2, %1, [%3]\n"
			 "cmp %2, #0\n"
	                 "it ne\n"
			 "bne 1b\n"
			 : "=r" (ret), "=&r" (tmp), "+r" (tmp2)
			 ,"+r" (p) : : "cc", "memory");
	return (ret);
}

static __inline uint32_t
atomic_load_acq_32(volatile uint32_t *p)
{
	uint32_t v;

	v = *p;
	__do_dmb();
	return (v);
}

static __inline void
atomic_store_rel_32(volatile uint32_t *p, uint32_t v)
{
	
	__do_dmb();
	*p = v;
}

static __inline uint64_t
atomic_fetchadd_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t ret, tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[ret], [%[ptr]]\n"
		"   adds     %Q[tmp], %Q[ret], %Q[val]\n"
		"   adc      %R[tmp], %R[ret], %R[val]\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [ret]    "=&r"  (ret),
		    [exf]    "=&r"  (exflag),
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p), 
		    [val]    "r"    (val)
		:   "cc", "memory");
	return (ret);
}

static __inline uint64_t
atomic_readandclear_64(volatile uint64_t *p)
{
	uint64_t ret, tmp;
	uint32_t exflag;

	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[ret], [%[ptr]]\n"
		"   mov      %Q[tmp], #0\n"
		"   mov      %R[tmp], #0\n"
		"   strexd   %[exf], %[tmp], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [ret]    "=&r"  (ret),
		    [exf]    "=&r"  (exflag),
		    [tmp]    "=&r"  (tmp)
		:   [ptr]    "r"    (p)
		:   "cc", "memory");
	return (ret);
}

static __inline uint64_t
atomic_load_64(volatile uint64_t *p)
{
	uint64_t ret;

	/*
	 * The only way to atomically load 64 bits is with LDREXD which puts the
	 * exclusive monitor into the open state, so reset it with CLREX because
	 * we don't actually need to store anything.
	 */
	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[ret], [%[ptr]]\n"
		"   clrex    \n"
		:   [ret]    "=&r"  (ret)
		:   [ptr]    "r"    (p)
		:   "cc", "memory");
	return (ret);
}

static __inline uint64_t
atomic_load_acq_64(volatile uint64_t *p)
{
	uint64_t ret;

	ret = atomic_load_64(p);
	__do_dmb();
	return (ret);
}

static __inline void
atomic_store_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	uint32_t exflag;

	/*
	 * The only way to atomically store 64 bits is with STREXD, which will
	 * succeed only if paired up with a preceeding LDREXD using the same
	 * address, so we read and discard the existing value before storing.
	 */
	__asm __volatile(
		"1:          \n"
		"   ldrexd   %[tmp], [%[ptr]]\n"
		"   strexd   %[exf], %[val], [%[ptr]]\n"
		"   teq      %[exf], #0\n"
		"   it ne    \n"
		"   bne      1b\n"
		:   [tmp]    "=&r"  (tmp),
		    [exf]    "=&r"  (exflag)
		:   [ptr]    "r"    (p),
		    [val]    "r"    (val)
		:   "cc", "memory");
}

static __inline void
atomic_store_rel_64(volatile uint64_t *p, uint64_t val)
{

	__do_dmb();
	atomic_store_64(p, val);
}

static __inline u_long
atomic_fetchadd_long(volatile u_long *p, u_long val)
{
	u_long tmp = 0, tmp2 = 0, ret = 0;

	__asm __volatile("1: ldrex %0, [%3]\n"
	    		    "add %1, %0, %4\n"
			    "strex %2, %1, [%3]\n"
			    "cmp %2, #0\n"
	                    "it ne\n"
			    "bne	1b\n"
			   : "+r" (ret), "=&r" (tmp), "+r" (tmp2)
			   ,"+r" (p), "+r" (val) : : "cc", "memory");
	return (ret);
}

static __inline u_long
atomic_readandclear_long(volatile u_long *p)
{
	u_long ret, tmp = 0, tmp2 = 0;

	__asm __volatile("1: ldrex %0, [%3]\n"
	    		 "mov %1, #0\n"
			 "strex %2, %1, [%3]\n"
			 "cmp %2, #0\n"
	                 "it ne\n"
			 "bne 1b\n"
			 : "=r" (ret), "=&r" (tmp), "+r" (tmp2)
			 ,"+r" (p) : : "cc", "memory");
	return (ret);
}

static __inline u_long
atomic_load_acq_long(volatile u_long *p)
{
	u_long v;

	v = *p;
	__do_dmb();
	return (v);
}

static __inline void
atomic_store_rel_long(volatile u_long *p, u_long v)
{
	
	__do_dmb();
	*p = v;
}
#else /* < armv6 */

#define __with_interrupts_disabled(expr) \
	do {						\
		u_int cpsr_save, tmp;			\
							\
		__asm __volatile(			\
			"mrs  %0, cpsr;"		\
			"orr  %1, %0, %2;"		\
			"msr  cpsr_fsxc, %1;"		\
			: "=r" (cpsr_save), "=r" (tmp)	\
			: "I" (I32_bit | F32_bit)		\
		        : "cc" );		\
		(expr);				\
		 __asm __volatile(		\
			"msr  cpsr_fsxc, %0"	\
			: /* no output */	\
			: "r" (cpsr_save)	\
			: "cc" );		\
	} while(0)

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
#define	ARM_HAVE_ATOMIC64

static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	__with_interrupts_disabled(*address |= setmask);
}

static __inline void
atomic_set_64(volatile uint64_t *address, uint64_t setmask)
{
	__with_interrupts_disabled(*address |= setmask);
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	__with_interrupts_disabled(*address &= ~clearmask);
}

static __inline void
atomic_clear_64(volatile uint64_t *address, uint64_t clearmask)
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

static __inline u_int64_t
atomic_cmpset_64(volatile u_int64_t *p, volatile u_int64_t cmpval, volatile u_int64_t newval)
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
atomic_add_64(volatile u_int64_t *p, u_int64_t val)
{
	__with_interrupts_disabled(*p += val);
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	__with_interrupts_disabled(*p -= val);
}

static __inline void
atomic_subtract_64(volatile u_int64_t *p, u_int64_t val)
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

static __inline uint64_t
atomic_fetchadd_64(volatile uint64_t *p, uint64_t v)
{
	uint64_t value;

	__with_interrupts_disabled(
	{
	    	value = *p;
		*p += v;
	});
	return (value);
}

static __inline uint64_t
atomic_load_64(volatile uint64_t *p)
{
	uint64_t value;

	__with_interrupts_disabled(value = *p);
	return (value);
}

static __inline void
atomic_store_64(volatile uint64_t *p, uint64_t value)
{
	__with_interrupts_disabled(*p = value);
}

#else /* !_KERNEL */

static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t *p, volatile u_int32_t cmpval, volatile u_int32_t newval)
{
	register int done, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%2]\n"
	    "cmp	%1, %3\n"
	    "streq	%4, [%2]\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "mov	%1, #0xffffffff\n"
	    "str	%1, [%0, #4]\n"
	    "moveq	%1, #1\n"
	    "movne	%1, #0\n"
	    : "+r" (ras_start), "=r" (done)
	    ,"+r" (p), "+r" (cmpval), "+r" (newval) : : "cc", "memory");
	return (done);
}

static __inline void
atomic_add_32(volatile u_int32_t *p, u_int32_t val)
{
	int start, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%2]\n"
	    "add	%1, %1, %3\n"
	    "str	%1, [%2]\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "mov	%1, #0xffffffff\n"
	    "str	%1, [%0, #4]\n"
	    : "+r" (ras_start), "=r" (start), "+r" (p), "+r" (val)
	    : : "memory");
}

static __inline void
atomic_subtract_32(volatile u_int32_t *p, u_int32_t val)
{
	int start, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%2]\n"
	    "sub	%1, %1, %3\n"
	    "str	%1, [%2]\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "mov	%1, #0xffffffff\n"
	    "str	%1, [%0, #4]\n"

	    : "+r" (ras_start), "=r" (start), "+r" (p), "+r" (val)
	    : : "memory");
}

static __inline void
atomic_set_32(volatile uint32_t *address, uint32_t setmask)
{
	int start, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%2]\n"
	    "orr	%1, %1, %3\n"
	    "str	%1, [%2]\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "mov	%1, #0xffffffff\n"
	    "str	%1, [%0, #4]\n"

	    : "+r" (ras_start), "=r" (start), "+r" (address), "+r" (setmask)
	    : : "memory");
}

static __inline void
atomic_clear_32(volatile uint32_t *address, uint32_t clearmask)
{
	int start, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%2]\n"
	    "bic	%1, %1, %3\n"
	    "str	%1, [%2]\n"
	    "2:\n"
	    "mov	%1, #0\n"
	    "str	%1, [%0]\n"
	    "mov	%1, #0xffffffff\n"
	    "str	%1, [%0, #4]\n"
	    : "+r" (ras_start), "=r" (start), "+r" (address), "+r" (clearmask)
	    : : "memory");

}

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t start, tmp, ras_start = ARM_RAS_START;

	__asm __volatile("1:\n"
	    "adr	%1, 1b\n"
	    "str	%1, [%0]\n"
	    "adr	%1, 2f\n"
	    "str	%1, [%0, #4]\n"
	    "ldr	%1, [%3]\n"
	    "mov	%2, %1\n"
	    "add	%2, %2, %4\n"
	    "str	%2, [%3]\n"
	    "2:\n"
	    "mov	%2, #0\n"
	    "str	%2, [%0]\n"
	    "mov	%2, #0xffffffff\n"
	    "str	%2, [%0, #4]\n"
	    : "+r" (ras_start), "=r" (start), "=r" (tmp), "+r" (p), "+r" (v)
	    : : "memory");
	return (start);
}

#endif /* _KERNEL */


static __inline uint32_t
atomic_readandclear_32(volatile u_int32_t *p)
{

	return (__swp(0, p));
}

#define atomic_cmpset_rel_32	atomic_cmpset_32
#define atomic_cmpset_acq_32	atomic_cmpset_32
#define atomic_set_rel_32	atomic_set_32
#define atomic_set_acq_32	atomic_set_32
#define atomic_clear_rel_32	atomic_clear_32
#define atomic_clear_acq_32	atomic_clear_32
#define atomic_add_rel_32	atomic_add_32
#define atomic_add_acq_32	atomic_add_32
#define atomic_subtract_rel_32	atomic_subtract_32
#define atomic_subtract_acq_32	atomic_subtract_32
#define atomic_store_rel_32	atomic_store_32
#define atomic_store_rel_long	atomic_store_long
#define atomic_load_acq_32	atomic_load_32
#define atomic_load_acq_long	atomic_load_long
#define atomic_add_acq_long		atomic_add_long
#define atomic_add_rel_long		atomic_add_long
#define atomic_subtract_acq_long	atomic_subtract_long
#define atomic_subtract_rel_long	atomic_subtract_long
#define atomic_clear_acq_long		atomic_clear_long
#define atomic_clear_rel_long		atomic_clear_long
#define atomic_set_acq_long		atomic_set_long
#define atomic_set_rel_long		atomic_set_long
#define atomic_cmpset_acq_long		atomic_cmpset_long
#define atomic_cmpset_rel_long		atomic_cmpset_long
#define atomic_load_acq_long		atomic_load_long
#undef __with_interrupts_disabled

static __inline void
atomic_add_long(volatile u_long *p, u_long v)
{

	atomic_add_32((volatile uint32_t *)p, v);
}

static __inline void
atomic_clear_long(volatile u_long *p, u_long v)
{

	atomic_clear_32((volatile uint32_t *)p, v);
}

static __inline int
atomic_cmpset_long(volatile u_long *dst, u_long old, u_long newe)
{

	return (atomic_cmpset_32((volatile uint32_t *)dst, old, newe));
}

static __inline u_long
atomic_fetchadd_long(volatile u_long *p, u_long v)
{

	return (atomic_fetchadd_32((volatile uint32_t *)p, v));
}

static __inline void
atomic_readandclear_long(volatile u_long *p)
{

	atomic_readandclear_32((volatile uint32_t *)p);
}

static __inline void
atomic_set_long(volatile u_long *p, u_long v)
{

	atomic_set_32((volatile uint32_t *)p, v);
}

static __inline void
atomic_subtract_long(volatile u_long *p, u_long v)
{

	atomic_subtract_32((volatile uint32_t *)p, v);
}



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
