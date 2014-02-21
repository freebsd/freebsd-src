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
/*      $NetBSD: brk.S,v 1.9 2000/06/26 06:25:43 kleink Exp $   */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "SYS.h"

        .globl  HIDENAME(curbrk)
        .globl  HIDENAME(minbrk)
        .globl  CNAME(_end)

        .data
HIDENAME(minbrk):
        .long   CNAME(_end)

        .text

ENTRY(brk)
#ifdef PIC
	mflr    %r10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r9
	mtlr	%r10
	lwz	%r5,HIDENAME(minbrk)@got(%r9)
	lwz	%r6,0(%r5)
#else
	lis	%r5,HIDENAME(minbrk)@ha
	lwz	%r6,HIDENAME(minbrk)@l(%r5)
#endif
	cmplw   %r6,%r3			/* if (minbrk <= r3) */
	bgt     0f
	mr	%r6,%r3			/* r6 = r3 */
0:
	mr      %r3,%r6			/* new break value */
	li      %r0,SYS_break
	sc				/* assume, that r5 is kept */
	bso	1f
#ifdef PIC
	lwz	%r7,HIDENAME(curbrk)@got(%r9)
	stw	%r6,0(%r7)
#else
	lis	%r7,HIDENAME(curbrk)@ha /* record new break */
	stw	%r6,HIDENAME(curbrk)@l(%r7)
#endif
	blr				/* return 0 */

1:
	b	PIC_PLT(HIDENAME(cerror))

	.section .note.GNU-stack,"",%progbits
