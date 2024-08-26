/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>

	.weak	strspn
	.set	strspn, __strspn
	.text

ENTRY(__strspn)

	/* check for special cases */
	ldrb	w4, [x1]		// first character in set
	cbz	w4, .Lzero		// empty set always returns 0

	mov	x15, #1			// preload register with 1 for stores

	// set is only one character
	ldrb	w5, [x1, #1]		// second character in the set
	cbz	w5, .Lsingle

	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, #256		// allocate 256 bytes on the stack

	/* no special case matches -- prepare lookup table */
	mov	w3, #28
0:	add	x9, sp, x3, lsl #3
	stp	xzr, xzr, [x9]
	stp	xzr, xzr, [x9, #16]
	subs	w3, w3, #4
	b.cs	0b

	strb	w15, [sp, x4]		// register first character in set
	add	x1, x1, #2

	/* process remaining chars in set */
	.p2align 4


0:	ldrb	w4, [x1]		// next char in set
	strb	w15, [sp, x5]		// register previous char
	cbz	w4, 1f			// NUL encountered?

	ldrb	w5, [x1, #1]
	add	x1, x1, #2
	strb	w15, [sp, x4]
	cbnz	w5, 0b

1:	mov	x5, x0			// stash a copy of src

	/* find mismatch */
	.p2align 4
0:	ldrb	w8, [x0]
	ldrb	w9, [sp, x8]
	cbz	w9, 2f

	ldrb	w8, [x0, #1]
	ldrb	w9, [sp, x8]
	cbz	w9, 3f

	ldrb	w8, [x0, #2]
	ldrb	w9, [sp, x8]
	cbz	w9, 4f

	ldrb	w8, [x0, #3]
	add	x0, x0, #4
	ldrb	w9, [sp, x8]
	cbnz	w9, 0b

	sub	x0, x0, #3
4:	sub	x5, x5, #1
3:	add	x0, x0, #1
2:	sub	x0, x0, x5
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret

.Lzero:
	mov	x0, #0
	ret

.Lsingle:
	ldrb	w8, [x0, x5]
	cmp	w4, w8
	b.ne	1f

	add	x5, x5, #1
	ldrb	w8, [x0, x5]
	cmp	w4, w8
	b.ne	1f

	add	x5, x5, #1
	ldrb	w8, [x0, x5]
	cmp	w4, w8
	b.ne	1f

	add	x5, x5, #1
	ldrb	w8, [x0, x5]
	add	x5, x5, #1
	cmp	w4, w8
	b.eq	.Lsingle

	sub	x5, x5, #1
1:	mov	x0, x5
	ret

END(__strspn)
