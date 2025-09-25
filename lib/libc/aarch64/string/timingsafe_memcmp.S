/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Robert Clausecker
 */

#include <machine/asm.h>

ENTRY(timingsafe_memcmp)
	cmp	x2, #16			// at least 17 bytes to process?
	bhi	.Lgt16

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
	bfi	w5, w3, #8, #8		// join bytes in big endian
	bfi	w6, w4, #8, #8
	sub	w0, w5, w6
	ret


.L0304:	ldrh	w3, [x0]		// load first halfwords
	ldrh	w4, [x1]
	sub	x2, x2, #2
	ldrh	w5, [x0, x2]		// load last halfwords
	ldrh	w6, [x1, x2]
	bfi	w3, w5, #16, #16	// join halfwords in little endian
	bfi	w4, w6, #16, #16
	rev	w3, w3			// swap word order
	rev	w4, w4
	cmp	w3, w4
	csetm	w0, lo			// w0 = w3 >= w4 ? 0 : -1
	csinc	w0, w0, wzr, ls		// w0 = w3 <=> w4 ? 1 : 0 : -1
	ret

.L0508:	ldr	w3, [x0]		// load first words
	ldr	w4, [x1]
	sub	x2, x2, #4
	ldr	w5, [x0, x2]		// load last words
	ldr	w6, [x1, x2]
	bfi	x3, x5, #32, #32	// join words in little endian
	bfi	x4, x6, #32, #32
	rev	x3, x3			// swap word order
	rev	x4, x4
	cmp	x3, x4
	csetm	w0, lo			// x0 = x3 >= w4 ? 0 : -1
	csinc	w0, w0, wzr, ls		// x0 = x3 <=> w4 ? 1 : 0 : -1
	ret

.L0916:	ldr	x3, [x0]
	ldr	x4, [x1]
	sub	x2, x2, #8
	ldr	x5, [x0, x2]
	ldr	x6, [x1, x2]
	cmp	x3, x4			// mismatch in first pair?
	csel	x3, x3, x5, ne		// use second pair if first pair equal
	csel	x4, x4, x6, ne
	rev	x3, x3
	rev	x4, x4
	cmp	x3, x4
	csetm	w0, lo
	csinc	w0, w0, wzr, ls
	ret

	/* more than 16 bytes: process buffer in a loop */
.Lgt16:	ldp	x3, x4, [x0], #16
	ldp	x5, x6, [x1], #16
	cmp	x3, x5			// mismatch in first pair?
	csel	x3, x3, x4, ne		// use second pair if first pair equal
	csel	x5, x5, x6, ne
	subs	x2, x2, #32
	bls	.Ltail

0:	ldp	x4, x7, [x0], #16
	ldp	x6, x8, [x1], #16
	cmp	x4, x6			// mismatch in first pair?
	csel	x4, x4, x7, ne		// if not, try second pair
	csel	x6, x6, x8, ne
	cmp	x3, x5			// was there a mismatch previously?
	csel	x3, x3, x4, ne		// apply new pair if there was not
	csel	x5, x5, x6, ne
	subs	x2, x2, #16
	bhi	0b

.Ltail:	add	x0, x0, x2
	add	x1, x1, x2
	ldp	x4, x7, [x0]
	ldp	x6, x8, [x1]
	cmp	x4, x6			// mismatch in first pair?
	csel	x4, x4, x7, ne		// if not, try second pair
	csel	x6, x6, x8, ne
	cmp	x3, x5			// was there a mismatch previously?
	csel	x3, x3, x4, ne		// apply new pair if there was not
	csel	x5, x5, x6, ne
	rev	x3, x3
	rev	x5, x5
	cmp	x3, x5
	csetm	w0, lo
	csinc	w0, w0, wzr, ls
	ret
END(timingsafe_memcmp)
