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
 *	$Id: atomic.h,v 1.1 1998/08/24 08:39:36 dfr Exp $
 */
#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts.  This code is now SMP safe as well.
 *
 * The assembly is volatilized to demark potential before-and-after side
 * effects if an interrupt or SMP collision were to occurs.
 */

#ifdef SMP
#define ATOMIC_ASM(NAME, TYPE, OP, V)	\
static __inline void			\
NAME(void *p, TYPE v)			\
{					\
	__asm __volatile("lock; "	\
			 OP : "=m" (*(TYPE *)p) : "ir" (V), "0" (*(TYPE *)p)); \
}
#else
#define ATOMIC_ASM(NAME, TYPE, OP, V)	\
static __inline void			\
NAME(void *p, TYPE v)			\
{					\
	__asm __volatile(OP : "=m" (*(TYPE *)p) : "ir" (V), "0" (*(TYPE *)p)); \
}
#endif

ATOMIC_ASM(atomic_set_char,	u_char, "orb %1,%0",   v)
ATOMIC_ASM(atomic_clear_char,	u_char, "andb %1,%0", ~v)
ATOMIC_ASM(atomic_add_char,	u_char, "addb %1,%0",  v)
ATOMIC_ASM(atomic_subtract_char,u_char, "subb %1,%0",  v)

ATOMIC_ASM(atomic_set_short,	u_short,"orw %1,%0",   v)
ATOMIC_ASM(atomic_clear_short,	u_short,"andw %1,%0", ~v)
ATOMIC_ASM(atomic_add_short,	u_short,"addw %1,%0",  v)
ATOMIC_ASM(atomic_subtract_short,u_short,"subw %1,%0", v)

ATOMIC_ASM(atomic_set_int,	u_int,	"orl %1,%0",   v)
ATOMIC_ASM(atomic_clear_int,	u_int,	"andl %1,%0", ~v)
ATOMIC_ASM(atomic_add_int,	u_int,	"addl %1,%0",  v)
ATOMIC_ASM(atomic_subtract_int,	u_int,	"subl %1,%0",  v)

ATOMIC_ASM(atomic_set_long,	u_long,	"orl %1,%0",   v)
ATOMIC_ASM(atomic_clear_long,	u_long,	"andl %1,%0", ~v)
ATOMIC_ASM(atomic_add_long,	u_long,	"addl %1,%0",  v)
ATOMIC_ASM(atomic_subtract_long,u_long,	"subl %1,%0",  v)

#undef ATOMIC_ASM

#endif /* ! _MACHINE_ATOMIC_H_ */
