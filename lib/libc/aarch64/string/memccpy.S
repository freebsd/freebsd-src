/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>

	.weak	memccpy
	.set	memccpy, __memccpy
	.text

ENTRY(__memccpy)
	subs	x3, x3, #1
	b.lo	.L0

	dup	v0.16b,	w2

	mov	x9, x0			// stash copy of src pointer
	bic	x10, x1, #0xf		// src aligned
	and	x11, x1, #0xf		// src offset

	ldr	q1, [x10]
	cmeq	v1.16b, v1.16b, v0.16b	// bytewise compare against src char

	mov	x8, #-1			// prepare a 0xfff..fff register
	mov	x6, #0xf

	lsl	x12, x11, #2
	lsl	x8, x8, x12		// mask of bytes in the string

	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1

	sub	x12, x11, #32
	adds	x12, x12, x3		// distance from alignment boundary - 32
	b.cc	.Lrunt			// branch if buffer length is 32 or less

	ands	x8, x8, x5
	b.eq	0f

	/* match in first chunk */
	rbit	x8, x8
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	sub	x8, x8, x11		// ... from beginning of the string

	add	x0, x0, x8
	add	x4, x9, x8		// dst + cnt
	add	x5, x1, x8		// src + cnt
	add	x0, x0, #1

	b	.L0816

0:
	ldr	q3,	[x10, #16]	// load second string chunk
	ldr	q2,	[x1]		// load true head
	cmeq	v1.16b, v3.16b, v0.16b	// char found in second chunk?

	/* process second chunk */
	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1

	cbz	x5, 0f

	/* match in second chunk */
	rbit	x8, x5
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	sub	x11, x11, #16
	sub	x8, x8, x11		// adjust for alignment offset
	add	x0, x0, x8		// return value
	add	x0, x0, #1

	add	x4, x9, x8
	add	x5, x1, x8
	b	.L1732

0:
	/* string didn't end in second chunk and neither did buffer */
	ldr	q1,	[x10, #32]	// load next string chunk
	str	q2,	[x0]		// deposit head into buffer
	sub	x0, x0, x11		// adjust x0
	mov	x3, x12
	str	q3,	[x0, #16]	// deposit second chunk

	add	x10, x10, #32		// advance src
	add	x0, x0, #32		// advance dst
	subs	x3, x3, #16		// enough left for another round?
	b.lo	1f

	/* main loop unrolled twice */
	.p2align 4
0:
	cmeq	v2.16b, v1.16b, v0.16b	// char found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2

	cbnz	x5, 3f

	str	q1, [x0]
	ldr	q1, [x10, #16]		// load next chunk

	cmp	x3, #16			// more than a full chunk left?
	b.lo	2f

	add	x10, x10, #32		// advance pointers
	add	x0, x0, #32

	cmeq	v2.16b, v1.16b, v0.16b	// char found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2
	cbnz	x5, 4f			// process chunk if match

	str	q1, [x0, #-16]
	ldr	q1, [x10]		// load next chunk

	subs	x3, x3, #32
	b.hs	0b

1:
	sub	x10, x10, #16		// undo second advancement
	add	x3, x3, #16
	sub	x0, x0, #16

	/* 1--16 bytes left in the buffer but string has not ended yet */
2:
	cmeq	v2.16b, v1.16b, v0.16b	// char found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x4, d2

	lsl	x5, x3, #2		// shift 0xf to the limits position
	lsl	x5, x6, x5
	orr	x8, x4, x5		// insert match in mask at limit

	rbit	x8, x8			// simulate x86 tzcnt
	clz	x7, x8			// index of mismatch
	lsr	x8, x7, #2

	lsl	x5, x6, x7		// simulate x86 bt with shifted 0xf

	add	x8, x8, #1
	add	x0, x0, x8

	ldr	q1, [x10, x8]		// load tail
	str	q1, [x0]		// store tail

	add	x0, x0, #16

	tst	x4, x5			// terminator encountered inside buffer?
	csel	x0, x0, xzr, ne		// if yes, return pointer, else NUL
	ret

4:
	sub	x10, x10, #16		// undo second advancement
	sub	x0, x0, #16		// undo second advancement

3:
	rbit	x8, x5
	clz	x8, x8			// index of mismatch
	lsr	x3, x8, #2

	add	x0, x0, x3		// restore dst pointer
	add	x10, x10, x3
	ldr	q1, [x10, #-15]
	str	q1, [x0, #-15]
	add	x0, x0, #1
	ret

.Lrunt:
	add	x13, x11, x3

	mov	x7, x5			// keep a copy of original match mask

	lsl	x4, x12, #2		// shift 0xf to the limits position
	lsl	x4, x6, x4

	cmp	x13, #16		// dont induce match if limit >=16
	csel	x4, x4, xzr, lo
	orr	x5, x5, x4		// insert match in mask at limit

	ands	x8, x8, x5		// if match always fall through
	b.ne	0f

	ldr	q4,	[x10, #16]	// load second string chunk
	cmeq	v1.16b, v4.16b, v0.16b	// char found in second chunk?

	/* process second chunk */
	shrn	v1.8b, v1.8h, #4
	fmov	x8, d1
	mov	x7, x8

	lsl	x4, x12, #2
	lsl	x4, x6, x4
	orr	x8, x8, x4		// induce match in upper bytes of mask

	rbit	x8, x8
	clz	x4, x8			// index of mismatch
	lsr	x8, x4, #2
	add	x8, x8, #16		// no match in first chunk
	b	1f

0:
	rbit	x8, x8
	clz	x4, x8			// index of mismatch
	lsr	x8, x4, #2
1:
	add	x0, x0, x8		// return value if terminator not found
	sub	x0, x0, x11
	add	x0, x0, #1

	/* check if we encountered a match or the limit first */
	lsl	x5, x6, x4
	ands	x7, x7, x5		// was the terminator present?
	csel	x0, xzr, x0, eq		// return value based on what we matched

	sub	x8, x8, x11
	add	x4, x9, x8		// dst + cnt
	add	x5, x1, x8		// src + cnt

	/* copy 17-32 bytes */
.L1732:
	cmp	x8, #16
	b.lo	.L0816
	add	x5, x5, #1		// ldp offsets are powers of 2
	add	x4, x4, #1
	ldp	x16, x17, [x1]
	ldp	x12, x13, [x5, #-16]
	stp	x16, x17, [x9]
	stp	x12, x13, [x4, #-16]
	ret

	/* Copy 8-16 bytes */
.L0816:
	tbz	x8, #3, .L0407
	ldr	x16, [x1]
	ldr	x17, [x5, #-7]
	str	x16, [x9]
	str	x17, [x4, #-7]
	ret

	/* Copy 4-7 bytes */
	.p2align 4
.L0407:
	cmp	x8, #3
	b.lo	.L0103
	ldr	w16, [x1]
	ldr	w18, [x5, #-3]
	str	w16, [x9]
	str	w18, [x4, #-3]
	ret

	/* Copy 1-3 bytes */
	.p2align 4
.L0103:
	lsr	x14, x8, #1
	ldrb	w16, [x1]
	ldrb	w15, [x5]
	ldrb	w18, [x1, x14]
	strb	w16, [x9]
	strb	w18, [x9, x14]
	strb	w15, [x4]
	ret

.L0:
	eor	x0, x0, x0
	ret

END(__memccpy)
