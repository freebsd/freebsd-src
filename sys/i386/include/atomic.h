/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and multiple processors.
 *
 * atomic_set_char(P, V)	(*(u_char*)(P) |= (V))
 * atomic_clear_char(P, V)	(*(u_char*)(P) &= ~(V))
 * atomic_add_char(P, V)	(*(u_char*)(P) += (V))
 * atomic_subtract_char(P, V)	(*(u_char*)(P) -= (V))
 *
 * atomic_set_short(P, V)	(*(u_short*)(P) |= (V))
 * atomic_clear_short(P, V)	(*(u_short*)(P) &= ~(V))
 * atomic_add_short(P, V)	(*(u_short*)(P) += (V))
 * atomic_subtract_short(P, V)	(*(u_short*)(P) -= (V))
 *
 * atomic_set_int(P, V)		(*(u_int*)(P) |= (V))
 * atomic_clear_int(P, V)	(*(u_int*)(P) &= ~(V))
 * atomic_add_int(P, V)		(*(u_int*)(P) += (V))
 * atomic_subtract_int(P, V)	(*(u_int*)(P) -= (V))
 * atomic_readandclear_int(P)	(return  *(u_int*)P; *(u_int*)P = 0;)
 *
 * atomic_set_long(P, V)	(*(u_long*)(P) |= (V))
 * atomic_clear_long(P, V)	(*(u_long*)(P) &= ~(V))
 * atomic_add_long(P, V)	(*(u_long*)(P) += (V))
 * atomic_subtract_long(P, V)	(*(u_long*)(P) -= (V))
 * atomic_readandclear_long(P)	(return  *(u_long*)P; *(u_long*)P = 0;)
 */

/*
 * The above functions are expanded inline in the statically-linked
 * kernel.  Lock prefixes are generated if an SMP kernel is being
 * built.
 *
 * Kernel modules call real functions which are built into the kernel.
 * This allows kernel modules to be portable between UP and SMP systems.
 */
#if defined(KLD_MODULE)
#define ATOMIC_ASM(NAME, TYPE, OP, V)			\
void atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v);

int atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src);

#else /* !KLD_MODULE */
#if defined(SMP)
#if defined(LOCORE)
#define	MPLOCKED	lock ;
#else
#define MPLOCKED	"lock ; "
#endif
#else
#define MPLOCKED
#endif

/*
 * The assembly is volatilized to demark potential before-and-after side
 * effects if an interrupt or SMP collision were to occur.
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 9)
/* egcs 1.1.2+ version */
#define ATOMIC_ASM(NAME, TYPE, OP, V)			\
static __inline void					\
atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	__asm __volatile(MPLOCKED OP			\
			 : "=m" (*p)			\
			 :  "0" (*p), "ir" (V)); 	\
}

/*
 * Atomic compare and set, used by the mutex functions
 *
 * if (*dst == exp) *dst = src (all 32 bit words)
 *
 * Returns 0 on failure, non-zero on success
 */

#if defined(I386_CPU)
static __inline int
atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src)
{
	int res = exp;

	__asm __volatile(
	"	pushfl ;		"
	"	cli ;			"
	"	cmpl	%1,%3 ;		"
	"	jne	1f ;		"
	"	movl	%2,%3 ;		"
	"1:				"
	"       sete	%%al;		"
	"	movzbl	%%al,%0 ;	"
	"	popfl ;			"
	"# atomic_cmpset_int"
	: "=a" (res)			/* 0 (result) */
	: "0" (exp),			/* 1 */
	  "r" (src),			/* 2 */
	  "m" (*(dst))			/* 3 */
	: "memory");

	return (res);
}
#else /* defined(I386_CPU) */
static __inline int
atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src)
{
	int res = exp;

	__asm __volatile (
	"	" MPLOCKED "		"
	"	cmpxchgl %2,%3 ;	"
	"       setz	%%al ;		"
	"	movzbl	%%al,%0 ;	"
	"1:				"
	"# atomic_cmpset_int"
	: "=a" (res)			/* 0 (result) */
	: "0" (exp),			/* 1 */
	  "r" (src),			/* 2 */
	  "m" (*(dst))			/* 3 */
	: "memory");				 

	return (res);
}
#endif /* defined(I386_CPU) */

#define	atomic_cmpset_long	atomic_cmpset_int
#define atomic_cmpset_acq_int	atomic_cmpset_int
#define atomic_cmpset_rel_int	atomic_cmpset_int
#define	atomic_cmpset_acq_long	atomic_cmpset_acq_int
#define	atomic_cmpset_rel_long	atomic_cmpset_rel_int
    
#else
/* gcc <= 2.8 version */
#define ATOMIC_ASM(NAME, TYPE, OP, V)			\
static __inline void					\
atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	__asm __volatile(MPLOCKED OP			\
			 : "=m" (*p)			\
			 : "ir" (V));		 	\
}							\
							\

#endif
#endif /* KLD_MODULE */

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 9)

/* egcs 1.1.2+ version */
ATOMIC_ASM(set,	     char,  "orb %b2,%0",   v)
ATOMIC_ASM(clear,    char,  "andb %b2,%0", ~v)
ATOMIC_ASM(add,	     char,  "addb %b2,%0",  v)
ATOMIC_ASM(subtract, char,  "subb %b2,%0",  v)

ATOMIC_ASM(set,	     short, "orw %w2,%0",   v)
ATOMIC_ASM(clear,    short, "andw %w2,%0", ~v)
ATOMIC_ASM(add,	     short, "addw %w2,%0",  v)
ATOMIC_ASM(subtract, short, "subw %w2,%0",  v)

ATOMIC_ASM(set,	     int,   "orl %2,%0",   v)
ATOMIC_ASM(clear,    int,   "andl %2,%0", ~v)
ATOMIC_ASM(add,	     int,   "addl %2,%0",  v)
ATOMIC_ASM(subtract, int,   "subl %2,%0",  v)

ATOMIC_ASM(set,	     long,  "orl %2,%0",   v)
ATOMIC_ASM(clear,    long,  "andl %2,%0", ~v)
ATOMIC_ASM(add,	     long,  "addl %2,%0",  v)
ATOMIC_ASM(subtract, long,  "subl %2,%0",  v)

#else

/* gcc <= 2.8 version */
ATOMIC_ASM(set,	     char,  "orb %1,%0",   v)
ATOMIC_ASM(clear,    char,  "andb %1,%0", ~v)
ATOMIC_ASM(add,	     char,  "addb %1,%0",  v)
ATOMIC_ASM(subtract, char,  "subb %1,%0",  v)

ATOMIC_ASM(set,	     short, "orw %1,%0",   v)
ATOMIC_ASM(clear,    short, "andw %1,%0", ~v)
ATOMIC_ASM(add,	     short, "addw %1,%0",  v)
ATOMIC_ASM(subtract, short, "subw %1,%0",  v)

ATOMIC_ASM(set,	     int,   "orl %1,%0",   v)
ATOMIC_ASM(clear,    int,   "andl %1,%0", ~v)
ATOMIC_ASM(add,	     int,   "addl %1,%0",  v)
ATOMIC_ASM(subtract, int,   "subl %1,%0",  v)

ATOMIC_ASM(set,	     long,  "orl %1,%0",   v)
ATOMIC_ASM(clear,    long,  "andl %1,%0", ~v)
ATOMIC_ASM(add,	     long,  "addl %1,%0",  v)
ATOMIC_ASM(subtract, long,  "subl %1,%0",  v)

#endif

#undef ATOMIC_ASM

#ifndef WANT_FUNCTIONS
#define ATOMIC_ACQ_REL(NAME, TYPE)			\
static __inline void					\
atomic_##NAME##_acq_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	__asm __volatile("lock; addl $0,0(%esp)" : : : "memory");\
	atomic_##NAME##_##TYPE(p, v);			\
}							\
							\
static __inline void					\
atomic_##NAME##_rel_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	atomic_##NAME##_##TYPE(p, v);			\
}

ATOMIC_ACQ_REL(set,		char)
ATOMIC_ACQ_REL(clear,		char)
ATOMIC_ACQ_REL(add,		char)
ATOMIC_ACQ_REL(subtract,	char)
ATOMIC_ACQ_REL(set,		short)
ATOMIC_ACQ_REL(clear,		short)
ATOMIC_ACQ_REL(add,		short)
ATOMIC_ACQ_REL(subtract,	short)
ATOMIC_ACQ_REL(set,		int)
ATOMIC_ACQ_REL(clear,		int)
ATOMIC_ACQ_REL(add,		int)
ATOMIC_ACQ_REL(subtract,	int)
ATOMIC_ACQ_REL(set,		long)
ATOMIC_ACQ_REL(clear,		long)
ATOMIC_ACQ_REL(add,		long)
ATOMIC_ACQ_REL(subtract,	long)

#undef ATOMIC_ACQ_REL

/*
 * We assume that a = b will do atomic loads and stores.
 */
#define ATOMIC_STORE_LOAD(TYPE)				\
static __inline u_##TYPE				\
atomic_load_acq_##TYPE(volatile u_##TYPE *p)		\
{							\
	__asm __volatile("lock; addl $0,0(%esp)" : : : "memory");\
	return (*p);					\
}							\
							\
static __inline void					\
atomic_store_rel_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	*p = v;						\
	__asm __volatile("" : : : "memory");		\
}

ATOMIC_STORE_LOAD(char)
ATOMIC_STORE_LOAD(short)
ATOMIC_STORE_LOAD(int)
ATOMIC_STORE_LOAD(long)

#undef ATOMIC_STORE_LOAD

static __inline int
atomic_cmpset_ptr(volatile void *dst, void *exp, void *src)
{

	return (
	    atomic_cmpset_int((volatile u_int *)dst, (u_int)exp, (u_int)src));
}

#define atomic_cmpset_acq_ptr	atomic_cmpset_ptr
#define atomic_cmpset_rel_ptr	atomic_cmpset_ptr

static __inline void *
atomic_load_acq_ptr(volatile void *p)
{
	return (void *)atomic_load_acq_int((volatile u_int *)p);
}

static __inline void
atomic_store_rel_ptr(volatile void *p, void *v)
{
	atomic_store_rel_int((volatile u_int *)p, (u_int)v);
}

#define ATOMIC_PTR(NAME)				\
static __inline void					\
atomic_##NAME##_ptr(volatile void *p, uintptr_t v)	\
{							\
	atomic_##NAME##_int((volatile u_int *)p, v);	\
}							\
							\
static __inline void					\
atomic_##NAME##_acq_ptr(volatile void *p, uintptr_t v)	\
{							\
	atomic_##NAME##_acq_int((volatile u_int *)p, v);\
}							\
							\
static __inline void					\
atomic_##NAME##_rel_ptr(volatile void *p, uintptr_t v)	\
{							\
	atomic_##NAME##_rel_int((volatile u_int *)p, v);\
}

ATOMIC_PTR(set)
ATOMIC_PTR(clear)
ATOMIC_PTR(add)
ATOMIC_PTR(subtract)

#undef ATOMIC_PTR

static __inline u_int
atomic_readandclear_int(volatile u_int *addr)
{
	u_int result;

	__asm __volatile (
	"	xorl	%0,%0 ;		"
	"	xchgl	%1,%0 ;		"
	"# atomic_readandclear_int"
	: "=&r" (result)		/* 0 (result) */
	: "m" (*addr));			/* 1 (addr) */

	return (result);
}

static __inline u_long
atomic_readandclear_long(volatile u_long *addr)
{
	u_long result;

	__asm __volatile (
	"	xorl	%0,%0 ;		"
	"	xchgl	%1,%0 ;		"
	"# atomic_readandclear_int"
	: "=&r" (result)		/* 0 (result) */
	: "m" (*addr));			/* 1 (addr) */

	return (result);
}
#endif

#define	atomic_set_8		atomic_set_char
#define	atomic_set_acq_8	atomic_set_acq_char
#define	atomic_set_rel_8	atomic_set_rel_char
#define	atomic_clear_8		atomic_clear_char
#define	atomic_clear_acq_8	atomic_clear_acq_char
#define	atomic_clear_rel_8	atomic_clear_rel_char
#define	atomic_add_8		atomic_add_char
#define	atomic_add_acq_8	atomic_add_acq_char
#define	atomic_add_rel_8	atomic_add_rel_char
#define	atomic_subtract_8	atomic_subtract_char
#define	atomic_subtract_acq_8	atomic_subtract_acq_char
#define	atomic_subtract_rel_8	atomic_subtract_rel_char
#define	atomic_load_acq_8	atomic_load_acq_char
#define	atomic_store_rel_8	atomic_store_rel_char

#define	atomic_set_16		atomic_set_short
#define	atomic_set_acq_16	atomic_set_acq_short
#define	atomic_set_rel_16	atomic_set_rel_short
#define	atomic_clear_16		atomic_clear_short
#define	atomic_clear_acq_16	atomic_clear_acq_short
#define	atomic_clear_rel_16	atomic_clear_rel_short
#define	atomic_add_16		atomic_add_short
#define	atomic_add_acq_16	atomic_add_acq_short
#define	atomic_add_rel_16	atomic_add_rel_short
#define	atomic_subtract_16	atomic_subtract_short
#define	atomic_subtract_acq_16	atomic_subtract_acq_short
#define	atomic_subtract_rel_16	atomic_subtract_rel_short
#define	atomic_load_acq_16	atomic_load_acq_short
#define	atomic_store_rel_16	atomic_store_rel_short

#define	atomic_set_32		atomic_set_int
#define	atomic_set_acq_32	atomic_set_acq_int
#define	atomic_set_rel_32	atomic_set_rel_int
#define	atomic_clear_32		atomic_clear_int
#define	atomic_clear_acq_32	atomic_clear_acq_int
#define	atomic_clear_rel_32	atomic_clear_rel_int
#define	atomic_add_32		atomic_add_int
#define	atomic_add_acq_32	atomic_add_acq_int
#define	atomic_add_rel_32	atomic_add_rel_int
#define	atomic_subtract_32	atomic_subtract_int
#define	atomic_subtract_acq_32	atomic_subtract_acq_int
#define	atomic_subtract_rel_32	atomic_subtract_rel_int
#define	atomic_load_acq_32	atomic_load_acq_int
#define	atomic_store_rel_32	atomic_store_rel_int
#define	atomic_cmpset_32	atomic_cmpset_int
#define	atomic_cmpset_acq_32	atomic_cmpset_acq_int
#define	atomic_cmpset_rel_32	atomic_cmpset_rel_int
#define	atomic_readandclear_32	atomic_readandclear_int

#endif /* ! _MACHINE_ATOMIC_H_ */
