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
 *
 * Warning: Use this version with gcc 2.7.2.x only.  Newer compilers
 * require different constraints in the asm statement.
 */
#if defined(KLD_MODULE)
#define ATOMIC_ASM(NAME, TYPE, OP, CONS, V)			\
	extern void atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v);

#else /* !KLD_MODULE */
#if defined(SMP)
#define MPLOCKED	"lock ; "
#else
#define MPLOCKED
#endif

/*
 * The assembly is volatilized to demark potential before-and-after side
 * effects if an interrupt or SMP collision were to occur.
 */
#define ATOMIC_ASM(NAME, TYPE, OP, CONS, V)		\
static __inline void					\
atomic_##NAME##_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	__asm __volatile(MPLOCKED OP			\
			 : "=m" (*p)			\
			 : CONS (V));			\
}
#endif /* KLD_MODULE */

ATOMIC_ASM(set,	     char,  "orb %b1,%0",  "iq",  v)
ATOMIC_ASM(clear,    char,  "andb %b1,%0", "iq", ~v)
ATOMIC_ASM(add,	     char,  "addb %b1,%0", "iq",  v)
ATOMIC_ASM(subtract, char,  "subb %b1,%0", "iq",  v)

ATOMIC_ASM(set,	     short, "orw %w1,%0",  "ir",  v)
ATOMIC_ASM(clear,    short, "andw %w1,%0", "ir", ~v)
ATOMIC_ASM(add,	     short, "addw %w1,%0", "ir",  v)
ATOMIC_ASM(subtract, short, "subw %w1,%0", "ir",  v)

ATOMIC_ASM(set,	     int,   "orl %1,%0",   "ir",  v)
ATOMIC_ASM(clear,    int,   "andl %1,%0",  "ir", ~v)
ATOMIC_ASM(add,	     int,   "addl %1,%0",  "ir",  v)
ATOMIC_ASM(subtract, int,   "subl %1,%0",  "ir",  v)

ATOMIC_ASM(set,	     long,  "orl %1,%0",   "ir",  v)
ATOMIC_ASM(clear,    long,  "andl %1,%0",  "ir", ~v)
ATOMIC_ASM(add,	     long,  "addl %1,%0",  "ir",  v)
ATOMIC_ASM(subtract, long,  "subl %1,%0",  "ir",  v)

#endif /* ! _MACHINE_ATOMIC_H_ */
