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
        .align 3
HIDENAME(minbrk):
        .llong   CNAME(_end)

        .text

ENTRY(brk)
	addis	%r6,%r2,HIDENAME(minbrk)@toc@ha
	ld	%r6,HIDENAME(minbrk)@toc@l(%r6)
	cmpld   %r6,%r3			/* if (minbrk <= r3) */
	bgt     0f
	mr	%r6,%r3			/* r6 = r3 */
0:
	mr      %r3,%r6			/* new break value */
	li      %r0,SYS_break
	sc				/* assume, that r5 is kept */
	bso	1f

	/* record new break */
	addis	%r7,%r2,HIDENAME(curbrk)@toc@ha
	std	%r6,HIDENAME(curbrk)@toc@l(%r7)

	blr				/* return 0 */

1:
	mflr	%r0
	std	%r0,16(%r1)
	stdu	%r1,-48(%r1)
	bl	PIC_PLT(HIDENAME(cerror))
	nop
	ld	%r1,0(%r1)
	ld	%r0,16(%r1)
	mtlr	%r0
	blr
