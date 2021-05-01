/*	$NetBSD: bcopy_page.S,v 1.7 2003/10/13 21:03:13 scw Exp $	*/

/*-
 * Copyright (c) 1995 Scott Stevens
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Scott Stevens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * bcopy_page.S
 *
 * page optimised bcopy and bzero routines
 *
 * Created      : 08/04/95
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");

#include "assym.inc"


/*
 * armv5e version of bcopy_page
 */
ENTRY(bcopy_page)
	pld	[r0]
	stmfd	sp!, {r4, r5}
	_SAVE({r4, r5})
	mov	ip, #32
	ldr	r2, [r0], #0x04		/* 0x00 */
	ldr	r3, [r0], #0x04		/* 0x04 */
1:	pld	[r0, #0x18]		/* Prefetch 0x20 */
	ldr	r4, [r0], #0x04		/* 0x08 */
	ldr	r5, [r0], #0x04		/* 0x0c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x10 */
	ldr	r3, [r0], #0x04		/* 0x14 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x18 */
	ldr	r5, [r0], #0x04		/* 0x1c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x20 */
	ldr	r3, [r0], #0x04		/* 0x24 */
	pld	[r0, #0x18]		/* Prefetch 0x40 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x28 */
	ldr	r5, [r0], #0x04		/* 0x2c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x30 */
	ldr	r3, [r0], #0x04		/* 0x34 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x38 */
	ldr	r5, [r0], #0x04		/* 0x3c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x40 */
	ldr	r3, [r0], #0x04		/* 0x44 */
	pld	[r0, #0x18]		/* Prefetch 0x60 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x48 */
	ldr	r5, [r0], #0x04		/* 0x4c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x50 */
	ldr	r3, [r0], #0x04		/* 0x54 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x58 */
	ldr	r5, [r0], #0x04		/* 0x5c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x60 */
	ldr	r3, [r0], #0x04		/* 0x64 */
	pld	[r0, #0x18]		/* Prefetch 0x80 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x68 */
	ldr	r5, [r0], #0x04		/* 0x6c */
	strd	r2, [r1], #0x08
	ldr	r2, [r0], #0x04		/* 0x70 */
	ldr	r3, [r0], #0x04		/* 0x74 */
	strd	r4, [r1], #0x08
	ldr	r4, [r0], #0x04		/* 0x78 */
	ldr	r5, [r0], #0x04		/* 0x7c */
	strd	r2, [r1], #0x08
	subs	ip, ip, #0x01
	ldrgt	r2, [r0], #0x04		/* 0x80 */
	ldrgt	r3, [r0], #0x04		/* 0x84 */
	strd	r4, [r1], #0x08
	bgt	1b
	ldmfd	sp!, {r4, r5}
	RET
END(bcopy_page)

/*
 * armv5e version of bzero_page
 */
ENTRY(bzero_page)
	mov	r1, #PAGE_SIZE
	mov	r2, #0
	mov	r3, #0
1:	strd	r2, [r0], #8		/* 32 */
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8		/* 64 */
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8		/* 96 */
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8		/* 128 */
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	strd	r2, [r0], #8
	subs	r1, r1, #128
	bne	1b
	RET
END(bzero_page)
