/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Robert Clausecker
 */

#include <machine/asm.h>

ENTRY(timingsafe_bcmp)
	cmp	x2, #32			// at least 33 bytes to process?
	bhi	.Lgt32

	cmp	x2, #16			// at least 17 bytes to process?
	bhi	.L1732

	cmp	x2, #8			// at least 9 bytes to process?
	bhi	.L0916

	cmp	x2, #4			// at least 5 bytes to process?
	bhi	.L0508

	cmp	x2, #2			// at least 3 bytes to process?
	bhi	.L0304

	cbnz	x2, .L0102		// buffer empty?

	mov	w0, #0			// empty buffer always matches
	ret

.L0102:	ldrb	w3, [x0]		// load first bytes
	ldrb	w4, [x1]
	sub	x2, x2, #1
	ldrb	w5, [x0, x2]		// load last bytes
	ldrb	w6, [x1, x2]
	eor	w3, w3, w4
	eor	w5, w5, w6
	orr	w0, w3, w5
	ret

.L0304:	ldrh	w3, [x0]		// load first halfwords
	ldrh	w4, [x1]
	sub	x2, x2, #2
	ldrh	w5, [x0, x2]		// load last halfwords
	ldrh	w6, [x1, x2]
	eor	w3, w3, w4
	eor	w5, w5, w6
	orr	w0, w3, w5
	ret

.L0508:	ldr	w3, [x0]		// load first words
	ldr	w4, [x1]
	sub	x2, x2, #4
	ldr	w5, [x0, x2]		// load last words
	ldr	w6, [x1, x2]
	eor	w3, w3, w4
	eor	w5, w5, w6
	orr	w0, w3, w5
	ret

.L0916:	ldr	x3, [x0]
	ldr	x4, [x1]
	sub	x2, x2, #8
	ldr	x5, [x0, x2]
	ldr	x6, [x1, x2]
	eor	x3, x3, x4
	eor	x5, x5, x6
	orr	x0, x3, x5
	orr	x0, x0, x0, lsr #32	// ensure low 32 bits are nonzero iff mismatch
	ret

.L1732:	ldr	q0, [x0]
	ldr	q1, [x1]
	sub	x2, x2, #16
	ldr	q2, [x0, x2]
	ldr	q3, [x1, x2]
	eor	v0.16b, v0.16b, v1.16b
	eor	v2.16b, v2.16b, v3.16b
	orr	v0.16b, v0.16b, v2.16b
	umaxv	s0, v0.4s		// get a nonzero word if any
	mov	w0, v0.s[0]
	ret

	/* more than 32 bytes: process buffer in a loop */
.Lgt32:	ldp	q0, q1, [x0], #32
	ldp	q2, q3, [x1], #32
	eor	v0.16b, v0.16b, v2.16b
	eor	v1.16b, v1.16b, v3.16b
	orr	v4.16b, v0.16b, v1.16b
	subs	x2, x2, #64		// enough left for another iteration?
	bls	.Ltail

0:	ldp	q0, q1, [x0], #32
	ldp	q2, q3, [x1], #32
	eor	v0.16b, v0.16b, v2.16b
	eor	v1.16b, v1.16b, v3.16b
	orr	v0.16b, v0.16b, v1.16b
	orr	v4.16b, v4.16b, v0.16b
	subs	x2, x2, #32
	bhi	0b

	/* process last 32 bytes */
.Ltail:	add	x0, x0, x2		// point to the last 32 bytes in the buffer
	add	x1, x1, x2
	ldp	q0, q1, [x0]
	ldp	q2, q3, [x1]
	eor	v0.16b, v0.16b, v2.16b
	eor	v1.16b, v1.16b, v3.16b
	orr	v0.16b, v0.16b, v1.16b
	orr	v4.16b, v4.16b, v0.16b
	umaxv	s0, v4.4s		// get a nonzero word if any
	mov	w0, v0.s[0]
	ret
END(timingsafe_bcmp)
