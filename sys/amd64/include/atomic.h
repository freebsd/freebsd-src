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
 *
 * atomic_set_long(P, V)	(*(u_long*)(P) |= (V))
 * atomic_clear_long(P, V)	(*(u_long*)(P) &= ~(V))
 * atomic_add_long(P, V)	(*(u_long*)(P) += (V))
 * atomic_subtract_long(P, V)	(*(u_long*)(P) -= (V))
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
	extern void atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v);

extern int atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src);

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

#else
/* gcc <= 2.8 version */
#define ATOMIC_ASM(NAME, TYPE, OP, V)			\
static __inline void					\
atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	__asm __volatile(MPLOCKED OP			\
			 : "=m" (*p)			\
			 : "ir" (V));		 	\
}
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

#ifndef WANT_FUNCTIONS
static __inline int
atomic_cmpset_ptr(volatile void *dst, void *exp, void *src)
{

	return (
	    atomic_cmpset_int((volatile u_int *)dst, (u_int)exp, (u_int)src));
}
#endif

#endif /* ! _MACHINE_ATOMIC_H_ */
