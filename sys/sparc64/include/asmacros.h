/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD: src/sys/sparc64/include/asmacros.h,v 1.19.26.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_ASMACROS_H_
#define	_MACHINE_ASMACROS_H_

#ifdef _KERNEL

/*
 * Normal and alternate %g6 point to the pcb of the current process.  Normal,
 & alternate and interrupt %g7 point to per-cpu data.
 */
#define	PCB_REG		%g6
#define	PCPU_REG	%g7

/*
 * Alternate %g5 points to a per-cpu panic stack, which is used as a last
 * resort, and for temporarily saving alternate globals.
 */
#define	ASP_REG		%g5

#ifdef LOCORE

/*
 * Atomically decrement an integer in memory.
 */
#define	ATOMIC_DEC_INT(r1, r2, r3) \
	lduw	[r1], r2 ; \
9:	sub	r2, 1, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically increment an integer in memory.
 */
#define	ATOMIC_INC_INT(r1, r2, r3) \
	lduw	[r1], r2 ; \
9:	add	r2, 1, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically increment an u_long in memory.
 */
#define	ATOMIC_INC_ULONG(r1, r2, r3) \
	ldx	[r1], r2 ; \
9:	add	r2, 1, r3 ; \
	casxa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

/*
 * Atomically clear a number of bits of an integer in memory.
 */
#define	ATOMIC_CLEAR_INT(r1, r2, r3, bits) \
	lduw	[r1], r2 ; \
9:	andn	r2, bits, r3 ; \
	casa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%icc, 9b ; \
	 mov	r3, r2

#define	PCPU(member)	PCPU_REG + PC_ ## member
#define	PCPU_ADDR(member, reg) \
	add	PCPU_REG, PC_ ## member, reg

#define	DEBUGGER() \
	ta	%xcc, 1

#define	PANIC(msg, r1) \
	.sect	.rodata ; \
9:	.asciz	msg ; \
	.previous ; \
	SET(9b, r1, %o0) ; \
	call	panic ; \
	 nop

#ifdef INVARIANTS
#define	KASSERT(r1, msg) \
	brnz	r1, 8f ; \
	 nop ; \
	PANIC(msg, r1) ; \
8:
#else
#define	KASSERT(r1, msg)
#endif

#define	PUTS(msg, r1) \
	.sect	.rodata ; \
9:	.asciz	msg ; \
	.previous ; \
	SET(9b, r1, %o0) ; \
	call	printf ; \
	 nop

#define	_ALIGN_DATA	.align 8

#define	DATA(name) \
	.data ; \
	_ALIGN_DATA ; \
	.globl	name ; \
	.type	name, @object ; \
name:

#define	EMPTY

#endif /* LOCORE */

#endif /* _KERNEL */

#endif /* !_MACHINE_ASMACROS_H_ */
