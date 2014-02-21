/*-
 * Copyright (c) 2002 Peter Grehan.
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
 */
/*      $NetBSD: sbrk.S,v 1.8 2000/06/26 06:25:44 kleink Exp $  */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "SYS.h"

	.globl	HIDENAME(curbrk)
	.globl	CNAME(_end)

	.data
HIDENAME(curbrk):
	.long	CNAME(_end)

	.text
ENTRY(sbrk)

#ifdef PIC
	mflr	%r10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r5
	mtlr	%r10
	lwz	%r5,HIDENAME(curbrk)@got(%r5)
	lwz	%r6,0(%r5)
#else
	lis	%r5,HIDENAME(curbrk)@ha
	lwz	%r6,HIDENAME(curbrk)@l(%r5)	/* r6 = old break */
#endif
	cmpwi	%r3,0				/* sbrk(0) - return curbrk */
	beq	1f
	add	%r3,%r3,%r6
	mr	%r7,%r3				/* r7 = new break */
	li	%r0,SYS_break
	sc					/* break(new_break) */
	bso	2f
#ifdef PIC
	stw     %r7,0(%r5)
#else
	stw     %r7,HIDENAME(curbrk)@l(%r5)	/* record new break */
#endif
1:
	mr      %r3,%r6				/* set return value */
	blr
2:
	b	PIC_PLT(HIDENAME(cerror))

	.section .note.GNU-stack,"",%progbits
