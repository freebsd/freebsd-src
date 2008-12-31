/*	$NetBSD: mainbus_io_asm.S,v 1.1 2001/02/24 19:38:02 reinoud Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/nexus_io_asm.S,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * bus_space I/O functions for nexus
 */


/*
 * read single
 */

ENTRY(nexus_bs_r_1)
	ldrb	r0, [r1, r2, lsl #2]
	RET

ENTRY(nexus_bs_r_2)
	ldr	r0, [r1, r2, lsl #2]
	bic	r0, r0, #0xff000000
	bic	r0, r0, #0x00ff0000
	RET

ENTRY(nexus_bs_r_4)
	ldr	r0, [r1, r2, lsl #2]
	RET

/*
 * write single
 */

ENTRY(nexus_bs_w_1)
	strb	r3, [r1, r2, lsl #2]
	RET

ENTRY(nexus_bs_w_2)
	mov	r3, r3, lsl #16
	orr	r3, r3, r3, lsr #16
	str	r3, [r1, r2, lsl #2]
	RET

ENTRY(nexus_bs_w_4)
	str	r3, [r1, r2, lsl #2]
	RET

/*
 * read multiple
 */

ENTRY(nexus_bs_rm_2)
	add	r0, r1, r2, lsl #2
	mov	r1, r3
	ldr	r2, [sp, #0]
	b	_C_LABEL(insw16)

/*
 * write multiple
 */

ENTRY(nexus_bs_wm_1)
	add	r0, r1, r2, lsl #2
	ldr	r2, [sp, #0]

	/* Make sure that we have a positive length */
	cmp	r2, #0x00000000
	movle	pc, lr

nexus_wm_1_loop:
	ldrb	r1, [r3], #0x0001
	str	r1, [r0]
	subs	r2, r2, #0x00000001
	bgt	nexus_wm_1_loop

	RET

ENTRY(nexus_bs_wm_2)
	add	r0, r1, r2, lsl #2
	mov	r1, r3
	ldr	r2, [sp, #0]
	b	_C_LABEL(outsw16)
