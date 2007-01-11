/*	$NetBSD: sa11x0_io_asm.S,v 1.1 2001/07/08 23:37:53 rjs Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
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
__FBSDID("$FreeBSD: src/sys/arm/sa11x0/sa11x0_io_asm.S,v 1.2 2005/01/05 21:58:48 imp Exp $");

/*
 * bus_space I/O functions for sa11x0
 */

/*
 * read single
 */

ENTRY(sa11x0_bs_r_1)
	ldrb	r0, [r1, r2]
	mov	pc, lr

ENTRY(sa11x0_bs_r_2)
	ldrh	r0, [r1, r2]
	mov	pc, lr

ENTRY(sa11x0_bs_r_4)
	ldr	r0, [r1, r2]
	mov	pc, lr

/*
 * write single
 */

ENTRY(sa11x0_bs_w_1)
	strb	r3, [r1, r2]
	mov	pc, lr

ENTRY(sa11x0_bs_w_2)
	strh	r3, [r1, r2]
	mov	pc, lr

ENTRY(sa11x0_bs_w_4)
	str	r3, [r1, r2]
	mov	pc, lr


/*
 * read multiple
 */

ENTRY(sa11x0_bs_rm_1)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp     r2, #0x00000000
	movle   pc, lr

sa11x0_bs_rm_1_loop:
	ldrb	r1, [r0]
	subs	r2, r2, #0x00000001
	strb	r1, [r3], #0x0001
	bgt	sa11x0_bs_rm_1_loop

	mov	pc, lr

ENTRY(sa11x0_bs_rm_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

	tst	r2, #0x00000007
	tsteq	r3, #0x00000003
	beq	sa11x0_bs_rm_2_fast

sa11x0_bs_rm_2_loop:
	ldrh	r1, [r0]
	subs	r2, r2, #0x00000001
	strh	r1, [r3], #0x0002
	bgt	sa11x0_bs_rm_2_loop

	mov	pc, lr

sa11x0_bs_rm_2_fast:
	stmfd	sp!, {r4, r5, lr}

sa11x0_bs_rm_2_fastloop:
	ldrh	r1, [r0]
	ldrh	lr, [r0]
	orr	r1, r1, lr, lsl #16

	ldrh	r4, [r0]
	ldrh	lr, [r0]
	orr	r4, r4, lr, lsl #16

	ldrh	r5, [r0]
	ldrh	lr, [r0]
	orr	r5, r5, lr, lsl #16

	ldrh	ip, [r0]
	ldrh	lr, [r0]
	orr	ip, ip, lr, lsl #16

	stmia	r3!, {r1, r4, r5, ip}
	subs	r2, r2, #8
	bgt	sa11x0_bs_rm_2_fastloop

	ldmfd	sp!, {r4, r5, pc}


ENTRY(sa11x0_bs_rm_4)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_bs_rm_4_loop:
	ldr	r1, [r0]
	subs	r2, r2, #0x00000001
	str	r1, [r3], #0x0004
	bgt	sa11x0_bs_rm_4_loop

	mov	pc, lr

/*
 * write multiple
 */

ENTRY(sa11x0_bs_wm_1)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_wm_1_loop:
	ldrb	r1, [r3], #0x0001
	subs	r2, r2, #0x00000001
	strb	r1, [r0]
	bgt	sa11x0_wm_1_loop

	mov	pc, lr

ENTRY(sa11x0_bs_wm_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_bs_wm_2_loop:
	ldrh	r1, [r3], #0x0002
	subs	r2, r2, #0x00000001
	strh	r1, [r0]
	bgt	sa11x0_bs_wm_2_loop

	mov	pc, lr

ENTRY(sa11x0_bs_wm_4)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_bs_wm_4_loop:
	ldr	r1, [r3], #0x0004
	subs	r2, r2, #0x00000001
	str	r1, [r0]
	bgt	sa11x0_bs_wm_4_loop

	mov	pc, lr

/*
 * read region
 */

ENTRY(sa11x0_bs_rr_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_bs_rr_2_loop:
	ldrh	r1, [r0], #0x0002  
	strh	r1, [r3], #0x0002
        subs    r2, r2, #0x00000001
	bgt	sa11x0_bs_rr_2_loop

        mov     pc, lr

/*
 * write region
 */

ENTRY(sa11x0_bs_wr_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000  
	movle	pc, lr

sa11x0_bs_wr_2_loop:
	ldrh	r1, [r3], #0x0002
	strh	r1, [r0], #0x0002
	subs	r2, r2, #0x00000001
	bgt	sa11x0_bs_wr_2_loop

	mov	pc, lr

/*
 * set regiuon
 */

ENTRY(sa11x0_bs_sr_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	cmp	r2, #0x00000000
	movle	pc, lr

sa11x0_bs_sr_2_loop:
	strh	r3, [r0], #0x0002
	subs	r2, r2, #0x00000001
	bgt	sa11x0_bs_sr_2_loop

	mov	pc, lr

/*
 * copy region
 */

ENTRY(sa11x0_bs_c_2)
	add	r0, r1, r2
	ldr	r2, [sp, #0]
	add	r1, r2, r3
	ldr	r2, [sp, #4]
	cmp	r2, #0x00000000
	movle	pc, lr

	cmp	r0, r1
	blt	sa11x0_bs_c_2_backwards

sa11x0_bs_cf_2_loop:
	ldrh	r3, [r0], #0x0002
	strh	r3, [r1], #0x0002
	subs	r2, r2, #0x00000001
	bgt	sa11x0_bs_cf_2_loop

	mov	pc, lr

sa11x0_bs_c_2_backwards:
	add	r0, r0, r2, lsl #1
	add	r1, r1, r2, lsl #1
	sub	r0, r0, #2
	sub	r1, r1, #2

sa11x0_bs_cb_2_loop:
	ldrh	r3, [r0], #-2
	strh	r3, [r1], #-2
	subs	r2, r2, #1
	bne	sa11x0_bs_cb_2_loop

	mov     pc, lr

/* end of sa11x0_io_asm.S */
