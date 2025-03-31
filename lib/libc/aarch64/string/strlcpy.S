/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>

	.weak strlcpy
	.set strlcpy, __strlcpy
	.text

ENTRY(__strlcpy)
	subs	x2, x2, #1
	b.lo	.L0

	mov	x9, x0			// stash copy of dst pointer
	bic	x10, x1, #0xf		// src aligned
	and	x11, x1, #0xf		// src offset

	ldr	q1, [x10]
	cmeq	v1.16b, v1.16b, #0	// NUL found in head?

	mov	x8, #-1			// fill register with 0xfff..fff
	lsl	x12, x11, #2
	lsl	x8, x8, x12		// mask of bytes in the string

	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1

	ands	x5, x5, x8
	b.ne	.Lhead_nul

	ldr	q3, [x10, #16]		// load second string chunk
	ldr	q2, [x1]		// load true head
	mov	x8, #32
	sub	x8, x8, x11

	cmeq	v1.16b, v3.16b, #0	// NUL found in second chunk?

	subs	x2, x2, x8
	b.ls	.Lhead_buf_end

	/* process second chunk */
	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1
	cbnz	x5, .Lsecond_nul

	/* string didn't end in second chunk and neither did buffer */
	ldr	q1,	[x10, #32]	// load next string chunk
	str	q2,	[x0]		// deposit head into buffer
	sub	x0, x0, x11		// adjust x0
	str	q3,	[x0, #16]	// deposit second chunk
	add	x10, x10, #32		// advance src
	add	x0, x0, #32		// advance dst
	subs	x2, x2, #16		// enough left for another round?
	b.ls	1f

	/* main loop unrolled twice */
	.p2align 4
0:
	cmeq	v2.16b, v1.16b, #0	// NUL found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2

	cbnz	x5, 3f

	str	q1, [x0]
	ldr	q1, [x10, #16]		// load next chunk

	cmp	x2, #16			// more than a full chunk left?
	b.ls	2f

	add	x10, x10, #32		// advance pointers
	add	x0, x0, #32

	cmeq	v2.16b, v1.16b, #0	// NUL found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2
	cbnz	x5, 4f			// process chunk if match

	str	q1, [x0, #-16]
	ldr	q1, [x10]		// load next chunk

	subs	x2, x2, #32
	b.hi	0b

1:
	sub	x10, x10, #16		// undo second advancement
	add	x2, x2, #16
	sub	x0, x0, #16

	/* 1--16 bytes left in the buffer but string has not ended yet */
2:
	cmeq	v2.16b, v1.16b, #0	// NUL found in second chunk?
	shrn	v2.8b, v2.8h, #4
	fmov	x4, d2

	mov	x6, #0xf
	mov	x7, x4

	lsl	x5, x2, #2		// shift 0xf to the limits position
	lsl	x5, x6, x5
	cmp	x2, #16			// dont induce match if limit >=16
	csel	x5, x5, xzr, lo
	orr	x8, x4, x5		// treat limit as if terminator present

	rbit	x8, x8			// simulate x86 tzcnt
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	add	x0, x0, x8

	ldr	q1, [x10, x8]		// load tail
	str	q1, [x0]		// store tail
	strb	wzr, [x0, #16]

	/* continue to find the end of the string */
	cbnz	x7, 1f

	/* we opt for a simpler strlen than the one in libc as the
	 * cmeq, shrn approach is faster for shorter strings.
	 */
	.p2align 4
0:
	ldr	q1, [x10, #32]
	cmeq	v1.16b, v1.16b, #0	// bytewise compare against NUL
	shrn	v1.8b, v1.8h, #4
	fmov	x7, d1
	cbnz	x7, 2f

	ldr	q1, [x10, #48]
	cmeq	v1.16b, v1.16b, #0	// bytewise compare against NUL
	shrn	v1.8b, v1.8h, #4
	fmov	x7, d1
	add	x10, x10, #32
	cbz	x7, 0b

1:	sub	x10, x10, #16
2:	rbit	x8, x7
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	sub	x10, x10, x1
	add	x0, x10, #32
	add	x0, x0, x8

	ret

4:
	sub	x10, x10, #16		// undo second advancement
	sub	x0, x0, #16		// undo second advancement

	/* string has ended but buffer has not */
3:
	rbit	x8, x5
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	add	x0, x0, x8		// restore dst pointer
	add	x10, x10, x8

	ldr	q1, [x10, #-15]
	str	q1, [x0, #-15]
	add	x0, x0, #1
	sub	x0, x10, x1

	ret

.Lhead_buf_end:
	shrn	v1.8b, v1.8h, #4
	fmov	x8, d1

	add	x2, x2, #32		// restore limit

	mov	x7, x8
	mov	x6, #0xf

	cmp	x2, #16			// should we induce a match or not
	b.lo	0f

	rbit	x8, x8
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2
	add	x8, x8, #16

	cmp	x8, x2
	csel	x8, x8, x2, lo		// copy min(buflen, srclen) bytes
	b	1f
0:

	rbit	x8, x8
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	mov	x8, x2
1:

	sub	x8, x8, x11
	strb	wzr, [x9, x8]

	/* continue to find the end of the string */
	cbnz	x7, 1f

	/* we opt for a simpler strlen than the one in libc as the
	 * cmeq, shrn approach is faster for shorter strings.
	 */
	.p2align 4
0:
	ldr	q1, [x10, #32]
	cmeq	v1.16b, v1.16b, #0	// bytewise compare against NUL
	shrn	v1.8b, v1.8h, #4
	fmov	x7, d1
	cbnz	x7, 2f

	ldr	q1, [x10, #48]
	cmeq	v1.16b, v1.16b, #0	// bytewise compare against NUL
	shrn	v1.8b, v1.8h, #4
	fmov	x7, d1
	add	x10, x10, #32
	cbz	x7, 0b

1:	sub	x10, x10, #16
2:	rbit	x6, x7
	clz	x6, x6			// index of mismatch
	lsr	x6, x6, #2

	sub	x10, x10, x1
	add	x0, x10, #32
	add	x0, x0, x6

	add	x4, x9, x8		// dst + cnt
	add	x5, x1, x8		// src + cnt

	b	.L1732

.Lsecond_nul:
	add	x2, x2, x8

	rbit	x8, x5
	clz	x8, x8			// index of mismatch
	lsr	x5, x8, #2

	sub	x8, x11, #16
	sub	x0, x5, x8		// string length

	cmp	x0, x2			// did we match or hit limit first?
	csel	x8, x2, x0, hi

	add	x4, x9, x8		// dst + cnt
	add	x5, x1, x8		// src + cnt

	strb	wzr, [x4]

	/* copy 17-32 bytes */
.L1732:
	cmp	x8, #16
	b.lo	.L0816
	ldp	x16, x17, [x1]
	ldp	x12, x1, [x5, #-16]
	stp	x16, x17, [x9]
	stp	x12, x1, [x4, #-16]
	ret

.Lhead_nul:
	rbit	x8, x5
	clz	x8, x8			// index of mismatch
	lsr	x8, x8, #2

	sub	x0, x8, x11
	cmp	x0, x2
	csel	x8, x2, x0, hi

	add	x4, x9, x8		// dst + cnt
	add	x5, x1, x8		// src + cnt
	strb	wzr, [x4]

	/* Copy 8-16 bytes */
.L0816:
	tbz	x8, #3, .L0407
	ldr	x16, [x1]
	ldr	x17, [x5, #-8]
	str	x16, [x9]
	str	x17, [x4, #-8]
	ret

	/* Copy 4-7 bytes */
	.p2align 4
.L0407:
	cmp	x8, #3
	b.ls	.L0203
	ldr	w16, [x1]
	ldr	w18, [x5, #-4]
	str	w16, [x9]
	str	w18, [x4, #-4]
	ret

.L0203:
	tbz	x8, 1, .L0001
	ldrh	w16, [x1]
	ldrh	w17, [x5, #-2]
	strh	w16, [x9]
	strh	w17, [x4, #-2]
	ret

.L0001:
	ldrb	w16, [x1]
	strb	w16, [x9]
	strb	wzr, [x4]
	ret

.L0:
	mov	x0, x1
	b	strlen
	ret
END(__strlcpy)
