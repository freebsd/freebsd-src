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
 * $FreeBSD$
 */

#ifndef	_MACHINE_ASMACROS_H_
#define	_MACHINE_ASMACROS_H_

#ifdef _KERNEL

	.register %g2,#ignore
	.register %g3,#ignore
	.register %g6,#ignore
	.register %g7,#ignore

#define	PCPU(member)	%g7 + GD_ ## member
#define	PCPU_ADDR(member, reg) add %g7, GD_ ## member, reg

#define	DEBUGGER()	ta %xcc, 1

#define	PANIC(msg, reg) \
	.sect	.rodata ; \
9:	.asciz	msg ; \
	.previous ; \
	setx	9b, reg, %o0 ; \
	call	panic ; \
	 nop

#endif

#define	DATA(name) \
	.data ; \
	.globl	name ; \
	.type	name, @object ; \
name ## :

#define	EMPTY

#define	ENTRY(name) \
	.text ; \
	.align	16 ; \
	.globl	name ; \
	.type	name, @function ; \
name ## :

#define	END(name) \
	.size	name, . - name

/*
 * If the kernel can be located above 4G, setx needs to be used to load
 * symbol values, otherwise set is sufficient.
 */
#ifdef HIGH_KERNEL
#define	SET(sym, tmp, dst) \
	setx sym, tmp, dst
#else
#define	SET(sym, tmp, dst) \
	set sym, dst
#endif

#endif /* !_MACHINE_ASMACROS_H_ */
