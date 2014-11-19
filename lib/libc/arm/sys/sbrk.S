/*	$NetBSD: sbrk.S,v 1.7 2003/08/07 16:42:05 agc Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sbrk.s	5.1 (Berkeley) 4/23/90
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
#include "SYS.h"

	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
	.align	0
	.globl	CURBRK
	.type	CURBRK,#object
CURBRK:
	.word	_C_LABEL(_end)

/*
 * Change the data segment size
 */
ENTRY(_sbrk)
#ifdef PIC
	/* Setup the GOT */
	ldr	r3, .Lgot
	add	r3, pc, r3
.L1:
	ldr	r2, .Lcurbrk
	ldr	r2, [r3, r2]
#else
	ldr	r2, .Lcurbrk
#endif
	/* Get the current brk address */
	ldr	r1, [r2]

	/* Calculate new value */
	mov	r3, r0
	add	r0, r0, r1
	SYSTRAP(break)
	bcs	PIC_SYM(CERROR, PLT)

	/* Store new curbrk value */
	ldr	r0, [r2]
	add	r1, r0, r3
	str	r1, [r2]

	/* Return old curbrk value */
	RET

	.align	0
#ifdef PIC
.Lgot:
	.word	_GLOBAL_OFFSET_TABLE_ - (.L1+4)
#endif
.Lcurbrk:
	.word	PIC_SYM(CURBRK, GOT)
END(_sbrk)
