/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>

	.weak	strcspn
	.set	strcspn, __strcspn
	.text

ENTRY(__strcspn)
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x15, #1			// preload register with 1 for stores

	/* check for special cases */
	ldrb	w4, [x1]		// first character in the set
	cbz	w4, .Lstrlen

	movi	v0.16b, #0

	ldrb	w5, [x1, #1]		// second character in the set
	cbz	w5, .Lstrchr

	sub	sp, sp, #256		// allocate 256 bytes on the stack

	/* no special case matches -- prepare lookup table */
	mov	w3, #20
	.p2align 4
0:	add	x9, sp, x3, lsl #3
	stp	xzr, xzr, [x9]
	stp	xzr, xzr, [x9, #16]
	subs	w3, w3, #4
	b.cs	0b

	/* utilize SIMD stores to speed up zeroing the table */
	stp	q0, q0, [sp, #6*32]
	stp	q0, q0, [sp, #7*32]

	add	x1, x1, #2
	strb	w15, [sp, x4]		// register first chars in the set
	strb	w15, [sp, x5]

	mov	x4, x0			// stash a copy of src

	/* process remaining chars in set */
	.p2align 4
0:	ldrb	w5, [x1]
	strb	w15, [sp, x5]
	cbz	w5, 1f			// end of set?

	ldrb	w5, [x1, #1]
	strb	w15, [sp, x5]
	cbz	w5, 1f

	add	x1, x1, #2
	b	0b

	/* find match */
	.p2align 4
1:	ldrb	w8, [x0]
	ldrb	w9, [sp, x8]
	cbnz	w9, 2f

	ldrb	w8, [x0, #1]
	ldrb	w9, [sp, x8]
	cbnz	w9, 3f

	ldrb	w8, [x0, #2]
	ldrb	w9, [sp, x8]
	cbnz	w9, 4f

	ldrb	w8, [x0, #3]
	ldrb	w9, [sp, x8]
	add	x0, x0, #4
	cbz	w9, 1b

	sub	x0, x0, #3		// fix up return value
4:	sub	x4, x4, #1
3:	add	x0, x0, #1
2:	sub	x0, x0, x4
	mov	sp, x29
	ldp	x29, x30, [sp], #16	// restore sp and lr
	ret

	/* set is empty, degrades to strlen */
	.p2align 4
.Lstrlen:
	mov	sp, x29
	ldp	x29, x30, [sp], #16	// restore sp and lr
	b	strlen

	/* just one character in set, degrades to strchrnul */
	.p2align 4
.Lstrchr:
	stp	x0, x1, [sp, #-16]!
	mov	x1, x4

	bl	strchrnul

	ldp	x18, x17, [sp], #16	// restore stashed src
	sub	x0, x0, x18

	ldp	x29, x30, [sp], #16	// Restore sp and lr
	ret

END(__strcspn)
