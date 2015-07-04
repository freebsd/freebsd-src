/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

ENTRY(.rtld_start)
	mov	x19, x0		/* Put ps_strings in a callee-saved register */
	mov	x20, sp		/* And the stack pointer */

	/* Handle the old style stack */
	/* TODO: Remove this when the kernel correctly aligns the stack */
	cbnz	x0, 1f
	mov	x0, sp		/* sp points to the args */
	and	sp, x0, #~0xf	/* Align the stack as needed */

1:	sub	sp, sp, #16	/* Make room for obj_main & exit proc */

	mov	x1, sp		/* exit_proc */
	add	x2, x1, #8	/* obj_main */
	bl	_rtld		/* Call the loader */
	mov	x8, x0		/* Backup the entry point */

	ldr	x2, [sp]	/* Load cleanup */
	ldr	x1, [sp, #8]	/* Load obj_main */
	mov	x0, x19		/* Restore ps_strings */
	mov	sp, x20		/* Restore the stack pointer */
	br	x8		/* Jump to the entry point */
END(.rtld_start)

/*
 * sp + 0 = &GOT[x + 3]
 * sp + 8 = RA
 * x16 = &GOT[2]
 * x17 = &_rtld_bind_start
 */
ENTRY(_rtld_bind_start)
	mov	x17, sp

	/* Save the arguments */
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, xzr, [sp, #-16]!

	/* Save any floating-point arguments */
	stp	q0, q1, [sp, #-32]!
	stp	q2, q3, [sp, #-32]!
	stp	q4, q5, [sp, #-32]!
	stp	q6, q7, [sp, #-32]!

	/* Calculate reloff */
	ldr	x2, [x17, #0]	/* Get the address of the entry */
	sub	x1, x2, x16	/* Find its offset */
	sub	x1, x1, #8	/* Adjust for x16 not being at offset 0 */
	/* Each rela item has 3 entriesso we need reloff = 3 * index */
	lsl	x3, x1, #1	/* x3 = 2 * offset */
	add	x1, x1, x3	/* x1 = x3 + offset = 3 * offset */

	/* Load obj */
	ldr	x0, [x16, #-8]

	/* Call into rtld */
	bl	_rtld_bind

	/* Restore the registers saved by the plt code */
	ldp	xzr, x30, [sp, #(5 * 16 + 4 * 32)]

	/* Backup the address to branch to */
	mov	x16, x0

	/* restore the arguments */
	ldp	q6, q7, [sp], #32
	ldp	q4, q5, [sp], #32
	ldp	q2, q3, [sp], #32
	ldp	q0, q1, [sp], #32
	ldp	x8, xzr, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x0, x1, [sp], #16
	/* And the part of the stack the plt entry handled */
	add	sp, sp, #16

	/* Call into the correct function */
	br	x16
END(_rtld_bind_start)

/*
 * uint64_t _rtld_tlsdesc(struct tlsdesc *);
 *
 * struct tlsdesc {
 *  uint64_t ptr;
 *  uint64_t data;
 * };
 *
 * Returns the data.
 */
ENTRY(_rtld_tlsdesc)
	ldr	x0, [x0, #8]
	ret
END(_rtld_tlsdesc)

/*
 * uint64_t _rtld_tlsdesc_dynamic(struct tlsdesc *);
 *
 * TODO: We could lookup the saved index here to skip saving the entire stack.
 */
ENTRY(_rtld_tlsdesc_dynamic)
	/* Store any registers we may use in rtld_tlsdesc_handle */
	stp	x29, x30, [sp, #-(10 * 16)]!
	mov	x29, sp
	stp	x1, x2,   [sp, #(1 * 16)]
	stp	x3, x4,   [sp, #(2 * 16)]
	stp	x5, x6,   [sp, #(3 * 16)]
	stp	x7, x8,   [sp, #(4 * 16)]
	stp	x9, x10,  [sp, #(5 * 16)]
	stp	x11, x12, [sp, #(6 * 16)]
	stp	x13, x14, [sp, #(7 * 16)]
	stp	x15, x16, [sp, #(8 * 16)]
	stp	x17, x18, [sp, #(9 * 16)]

	/* Find the tls offset */
	ldr	x0, [x0, #8]
	mov	x1, #1
	bl	rtld_tlsdesc_handle

	/* Restore the registers */
	ldp	x17, x18, [sp, #(9 * 16)]
	ldp	x15, x16, [sp, #(8 * 16)]
	ldp	x13, x14, [sp, #(7 * 16)]
	ldp	x11, x12, [sp, #(6 * 16)]
	ldp	x9, x10,  [sp, #(5 * 16)]
	ldp	x7, x8,   [sp, #(4 * 16)]
	ldp	x5, x6,   [sp, #(3 * 16)]
	ldp	x3, x4,   [sp, #(2 * 16)]
	ldp	x1, x2,   [sp, #(1 * 16)]
	ldp	x29, x30, [sp], #(10 * 16)

	ret
END(_rtld_tlsdesc_dynamic)
