/*	$NetBSD: ixp425_a4x_io.S,v 1.2 2005/12/11 12:16:51 christos Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*	
 * There are simple bus space functions for IO registers mapped at
 * 32-bit aligned positions.  offset is multiplied by 4.  
 *
 * Based loosely on pxa2x0_a2x_io.S
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * bus_space I/O functions with offset*4
 */

/*
 * Read single
 */
ENTRY(a4x_bs_r_1)
	ldr	r0, [r1, r2, LSL #2]
	and	r0, r0, #0xff
	mov	pc, lr

ENTRY(a4x_bs_r_2)
	ldr	r0, [r1, r2, LSL #2]
	mov	r1, #0xff
	orr	r1, r1, r1, lsl #8
	and	r0, r0, r1
	mov	pc, lr

ENTRY(a4x_bs_r_4)
	ldr	r0, [r1, r2, LSL #2]
	mov	pc, lr

/*
 * Write single
 */
ENTRY(a4x_bs_w_1)
	and	r3, r3, #0xff
	str	r3, [r1, r2, LSL #2]
	mov	pc, lr

ENTRY(a4x_bs_w_2)
	mov	r0, #0xff
	orr	r0, r0, r0, lsl #8
	and	r3, r3, r0
	str	r3, [r1, r2, LSL #2]
	mov	pc, lr

ENTRY(a4x_bs_w_4)
	str	r3, [r1, r2, LSL #2]
	mov	pc, lr

/*
 * Read multiple
 */
ENTRY(a4x_bs_rm_1)
	add	r0, r1, r2, lsl #2
	ldr	r2, [sp, #0]
	mov	r1, r3
	teq	r2, #0
	moveq	pc, lr
1:	ldr	r3, [r0]
	subs	r2, r2, #1
	strb	r3, [r1], #1
	bne	1b
	mov	pc, lr

ENTRY(a4x_bs_rm_2)
	add	r0, r1, r2, lsl #2
	ldr	r2, [sp, #0]
	mov	r1, r3
	teq	r2, #0
	moveq	pc, lr
1:	ldr	r3, [r0]
	subs	r2, r2, #1
	strh	r3, [r1], #2
	bne	1b
	mov	pc, lr

/*
 * Write multiple
 */
ENTRY(a4x_bs_wm_1)
	add	r0, r1, r2, lsl #2
	ldr	r2, [sp, #0]
	mov	r1, r3
	teq	r2, #0
	moveq	pc, lr
1:	ldrb	r3, [r1], #1
	subs	r2, r2, #1
	str	r3, [r0]
	bne	1b
	mov	pc, lr

ENTRY(a4x_bs_wm_2)
	add	r0, r1, r2, lsl #2
	ldr	r2, [sp, #0]
	mov	r1, r3
	teq	r2, #0
	moveq	pc, lr
1:	ldrh	r3, [r1], #2
	subs	r2, r2, #1
	str	r3, [r0]
	bne	1b
	mov	pc, lr
