/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <machine/asm.h>
#include <machine/param.h>

	.weak	strncmp
	.set	strncmp, __strncmp
	.text

ENTRY(__strncmp)

	bic	x8, x0, #0xf			// x0 aligned to the boundary
	and	x9, x0, #0xf			// x9 is the offset
	bic	x10, x1, #0xf			// x1 aligned to the boundary
	and	x11, x1, #0xf			// x11 is the offset

	subs	x2, x2, #1
	b.lo	.Lempty

	mov	x13, #-1			// save constants for later
	mov	x16, #0xf

	/*
	 * Check if either string is located at end of page to avoid crossing
	 * into unmapped page. If so, we load 16 bytes from the nearest
	 * alignment boundary and shift based on the offset.
	 */

	add	x3, x0, #16			// end of head
	add	x4, x1, #16
	eor	x3, x3, x0
	eor	x4, x4, x1			// bits that changed
	orr	x3, x3, x4			// in either str1 or str2
	cmp	x2,#16
	b.lo	.Llt16
	tbz	w3, #PAGE_SHIFT, .Lbegin

	ldr	q0, [x8]			// load aligned head
	ldr	q1, [x10]

	lsl	x14, x9, #2
	lsl	x15, x11, #2
	lsl	x3, x13, x14			// string head
	lsl	x4, x13, x15

	cmeq	v5.16b, v0.16b, #0
	cmeq	v6.16b, v1.16b, #0

	shrn	v5.8b, v5.8h, #4
	shrn	v6.8b, v6.8h, #4
	fmov	x5, d5
	fmov	x6, d6

	adrp	x14, shift_data
	add	x14, x14, :lo12:shift_data

	/* heads may cross page boundary, avoid unmapped loads */
	tst	x5, x3
	b.eq	0f

	ldr	q4, [x14, x9]			// load permutation table
	tbl	v0.16b, {v0.16b}, v4.16b

	b	1f
	.p2align 4
0:
	ldr	q0, [x0]			// load true head
1:
	tst	x6, x4
	b.eq	0f

	ldr	q4, [x14, x11]
	tbl	v4.16b, {v1.16b}, v4.16b

	b 1f

	.p2align 4
.Lbegin:
	ldr	q0, [x0]			// load true heads
0:
	ldr	q4, [x1]
1:
	cmeq	v2.16b, v0.16b, #0		// NUL byte present?
	cmeq	v4.16b, v0.16b, v4.16b		// which bytes match?

	orn	v2.16b, v2.16b, v4.16b		// mismatch or NUL byte?

	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2

	cbnz	x5, .Lhead_mismatch
	/* load head and second chunk */
	ldr	q2, [x8, #16]			// load second chunk
	ldr	q3, [x10, #16]

	add	x2, x2, x11
	sub	x2, x2, #16

	subs	x9, x9, x11			// is a&0xf >= b&0xf
	b.lo	.Lswapped			// if not swap operands
	b	.Lnormal

	.p2align 4
.Llt16:
	/*
	 * Check if either string is located at end of page to avoid crossing
	 * into unmapped page. If so, we load 16 bytes from the nearest
	 * alignment boundary and shift based on the offset.
	 */
	tbz	w3, #PAGE_SHIFT, 2f

	ldr	q0, [x8]			// load aligned head
	ldr	q1, [x10]

	lsl	x14, x9, #2
	lsl	x15, x11, #2
	lsl	x3, x13, x14			// string head
	lsl	x4, x13, x15

	/* Introduce a null byte match if the limit is within the aligned chunk */
	add	x14, x2, x9
	add	x15, x2, x11
	lsl	x14, x14, #2
	lsl	x15, x15, #2
	lsl	x14, x16, x14
	lsl	x15, x16, x15

	cmeq	v5.16b, v0.16b, #0
	cmeq	v6.16b, v1.16b, #0

	shrn	v5.8b, v5.8h, #4
	shrn	v6.8b, v6.8h, #4
	fmov	x5, d5
	fmov	x6, d6

	orr	x5, x5, x14			// insert match at limit
	orr	x6, x6, x15

	adrp	x14, shift_data
	add	x14, x14, :lo12:shift_data

	/* heads may cross page boundary, avoid unmapped loads */
	tst	x5, x3
	b.eq	0f

	ldr	q4, [x14, x9]			// load permutation table
	tbl	v0.16b, {v0.16b}, v4.16b

	b	1f
	.p2align 4
0:
	ldr	q0, [x0]			// load true head
1:
	tst	x6, x4
	b.eq	0f

	ldr	q4, [x14, x11]
	tbl	v4.16b, {v1.16b}, v4.16b

	b 1f

	.p2align 4
2:
	ldr	q0, [x0]			// load true heads
0:
	ldr	q4, [x1]
1:

	cmeq	v2.16b, v0.16b, #0		// NUL byte present?
	cmeq	v4.16b, v0.16b, v4.16b		// which bytes match?

	bic	v2.16b, v4.16b, v2.16b		// match and not NUL byte

	shrn	v2.8b, v2.8h, #4
	fmov	x5, d2
	lsl	x4, x2, #2
	lsl	x4, x13, x4
	orn	x5, x4, x5			// mismatch or NUL byte?

.Lhead_mismatch:
	rbit	x3, x5
	clz	x3, x3				// index of mismatch
	lsr	x3, x3, #2
	ldrb	w4, [x0, x3]
	ldrb	w5, [x1, x3]
	sub	w0, w4, w5
	ret

	.p2align 4
.Lnormal:
	sub	x12, x10, x9
	ldr	q0, [x12, #16]!
	sub	x10, x10, x8
	sub	x11, x10, x9

	cmeq	v1.16b, v3.16b, #0		// NUL present?
	cmeq	v0.16b, v0.16b, v2.16b		// Mismatch between chunks?
	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	add	x8, x8, #32			// advance to next iteration

	lsl	x4, x2, #2
	lsl	x4, x13, x4
	orr	x3, x6, x4			// introduce a null byte match
	cmp	x2, #16				// does the buffer end within x2
	csel	x6, x3, x6, lo
	cbnz	x6, .Lnulfound2			// NUL or end of buffer found?
	mvn	x5, x5
	cbnz	x5, .Lmismatch2
	sub	x2, x2, #16
	cmp	x2, #32				// end of buffer?
	b.lo	.Ltail
	/*
	 * During the main loop, the layout of the two strings is something like:
	 *
	 *          v ------1------ v ------2------ v
	 *      X0:    AAAAAAAAAAAAABBBBBBBBBBBBBBBB...
	 *      X1: AAAAAAAAAAAAABBBBBBBBBBBBBBBBCCC...
	 *
	 * where v indicates the alignment boundaries and corresponding chunks
	 * of the strings have the same letters.  Chunk A has been checked in
	 * the previous iteration.  This iteration, we first check that string
	 * X1 doesn't end within region 2, then we compare chunk B between the
	 * two strings.  As X1 is known not to hold a NUL byte in regions 1
	 * and 2 at this point, this also ensures that x0 has not ended yet.
	 */
	.p2align 4
0:
	ldr	q0, [x8, x11]
	ldr	q1, [x8, x10]
	ldr	q2, [x8]

	cmeq	v1.16b, v1.16b, #0		// end of string?
	cmeq	v0.16b, v0.16b, v2.16b		// do the chunks match?

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0
	cbnz	x6, .Lnulfound
	mvn	x5, x5				// any mismatches?
	cbnz	x5, .Lmismatch

	add	x8, x8, #16

	/* main loop unrolled twice */
	ldr	q0, [x8, x11]
	ldr	q1, [x8, x10]
	ldr	q2, [x8]

	add	x8, x8, #16
	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0
	cbnz	x6, .Lnulfound2
	mvn	x5, x5
	cbnz	x5, .Lmismatch2
	sub	x2, x2, #32
	cmp	x2, #32				// end of buffer?
	b.hs	0b				// if yes, process tail

	/* end of buffer will occur in next 32 bytes */
.Ltail:
	ldr	q0, [x8, x11]
	ldr	q1, [x8, x10]
	ldr	q2, [x8]

	cmeq	v1.16b, v1.16b, #0		// end of string?
	cmeq	v0.16b, v0.16b, v2.16b		// do the chunks match?

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	/*
	 * If x2 <= 16 then we introduce a NUL byte in the
	 * result from CMEQ to avoid comparing further!
	 */

	lsl	x4, x2, #2
	lsl	x4, x13, x4
	orr	x3, x6, x4			// introduce a null byte match
	cmp	x2, #16				// does the buffer end within x2
	csel	x6, x3, x6, lo

	cbnz	x6, .Lnulfound			// NUL or end of string found
	mvn	x5, x5
	cbnz	x5, .Lmismatch

	add	x8, x8, #16

	/* main loop unrolled twice */
	ldr	q0, [x8, x11]
	ldr	q1, [x8, x10]
	ldr	q2, [x8]

	add	x8, x8, #16
	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	ubfiz	x4, x2, #2, #4	// (x2 - 16) << 2
	lsl	x4, x13, x4			// take first half into account
	orr	x6, x6, x4			// introduce a null byte match

.Lnulfound2:
	sub	x8, x8, #16

.Lnulfound:
	mov	x4, x6

	ubfiz	x7, x9, #2, #4
	lsl	x6, x6, x7			// adjust NUL mask to indices

	orn	x5, x6, x5
	cbnz	x5, .Lmismatch

	/*
	 * (x0) == (x1) and NUL is past the string.
	 * Compare (x1) with the corresponding part
	 * of the other string until the NUL byte.
	 */
	ldr	q0, [x8, x9]
	ldr	q1, [x8, x10]

	cmeq	v1.16b, v0.16b, v1.16b
	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1

	orn	x5, x4, x5

	rbit	x3, x5
	clz	x3, x3
	lsr	x5, x3, #2

	add	x10, x10, x8			// restore x10 pointer
	add	x8, x8, x9			// point to corresponding chunk

	ldrb	w4, [x8, x5]
	ldrb	w5, [x10, x5]
	sub	w0, w4, w5
	ret

	.p2align 4
.Lmismatch2:
	sub	x8, x8, #16			// roll back second increment
.Lmismatch:
	rbit	x3, x5
	clz	x3, x3				// index of mismatch
	lsr	x3, x3, #2
	add	x11, x8, x11

	ldrb	w4, [x8, x3]
	ldrb	w5, [x11, x3]
	sub	w0, w4, w5			// byte difference
	ret

	/*
	 * If (a&0xf) < (b&0xf), we do the same thing but with swapped
	 * operands.  I found that this performs slightly better than
	 * using conditional moves to do the swap branchless.
	 */
	.p2align 4
.Lswapped:
	add	x12, x8, x9
	ldr	q0, [x12, #16]!
	sub	x8, x8, x10
	add	x11, x8, x9
	add	x2,x2,x9
	neg	x9, x9

	cmeq	v1.16b, v2.16b, #0
	cmeq	v0.16b, v0.16b, v3.16b
	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	add	x10, x10, #32

	lsl	x4, x2, #2
	lsl	x4, x13, x4
	orr	x3,x6,x4			// introduce a null byte match
	cmp	x2,#16
	csel	x6, x3, x6, lo
	cbnz	x6, .Lnulfound2s
	mvn	x5, x5
	cbnz	x5, .Lmismatch2s

	sub	x2, x2, #16
	cmp	x2, #32
	b.lo	.Ltails

	/*
	 * During the main loop, the layout of the two strings is something like:
	 *
	 *          v ------1------ v ------2------ v
	 *      X1:    AAAAAAAAAAAAABBBBBBBBBBBBBBBB...
	 *      X0: AAAAAAAAAAAAABBBBBBBBBBBBBBBBCCC...
	 *
	 * where v indicates the alignment boundaries and corresponding chunks
	 * of the strings have the same letters.  Chunk A has been checked in
	 * the previous iteration.  This iteration, we first check that string
	 * X0 doesn't end within region 2, then we compare chunk B between the
	 * two strings.  As X0 is known not to hold a NUL byte in regions 1
	 * and 2 at this point, this also ensures that X1 has not ended yet.
	 */
	.p2align 4
0:
	ldr	q0, [x10, x11]
	ldr	q1, [x10, x8]
	ldr	q2, [x10]

	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0
	cbnz	x6, .Lnulfounds
	mvn	x5, x5
	cbnz	x5, .Lmismatchs

	add	x10, x10, #16

	/* main loop unrolled twice */
	ldr	q0, [x10, x11]
	ldr	q1, [x10, x8]
	ldr	q2, [x10]

	add	x10, x10, #16
	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0
	cbnz	x6, .Lnulfound2s
	mvn	x5, x5
	cbnz	x5, .Lmismatch2s
	sub	x2, x2, #32
	cmp	x2, #32
	b.hs	0b

.Ltails:
	ldr	q0, [x10, x11]
	ldr	q1, [x10, x8]
	ldr	q2, [x10]

	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	/*
	 * If x2 <= 16 then we introduce a NUL byte in the
	 * result from CMEQ to avoid comparing further!
	 */

	lsl	x4, x2, #2
	lsl	x4, x13, x4
	orr	x3, x6, x4			// introduce a null byte match
	cmp	x2, #16
	csel	x6, x3, x6, lo

	cbnz	x6, .Lnulfounds
	mvn	x5, x5
	cbnz	x5, .Lmismatchs

	add	x10, x10, #16

	ldr	q0, [x10, x11]
	ldr	q1, [x10, x8]
	ldr	q2, [x10]

	add	x10, x10, #16
	cmeq	v1.16b, v1.16b, #0
	cmeq	v0.16b, v0.16b, v2.16b

	shrn	v1.8b, v1.8h, #4
	shrn	v0.8b, v0.8h, #4
	fmov	x6, d1
	fmov	x5, d0

	ubfiz	x4, x2, #2, #4
	lsl	x4, x13, x4
	orr	x6, x6, x4			// introduce a null byte match

.Lnulfound2s:
	sub	x10, x10, #16
.Lnulfounds:
	mov	x4, x6

	ubfiz	x7, x9, #2, #4
	lsl	x6, x6, x7

	orn	x5, x6, x5

	cbnz	x5, .Lmismatchs

	ldr	q0, [x10, x9]
	ldr	q1, [x10, x8]

	cmeq	v1.16b, v0.16b, v1.16b
	shrn	v1.8b, v1.8h, #4
	fmov	x5, d1

	orn	x5, x4, x5

	rbit	x3, x5
	clz	x3, x3
	lsr	x5, x3, #2

	add	x11, x10, x8
	add	x10, x10, x9

	ldrb	w4, [x10, x5]
	ldrb	w5, [x11, x5]
	sub	w0, w5, w4
	ret

	.p2align 4
.Lmismatch2s:
	sub	x10, x10, #16
.Lmismatchs:
	rbit	x3, x5
	clz	x3, x3
	lsr	x3, x3, #2
	add	x11, x10, x11

	ldrb	w4, [x10, x3]
	ldrb	w5, [x11, x3]
	sub	w0, w5, w4
	ret

	.p2align 4
.Lempty:
	eor	x0, x0, x0
	ret

END(__strncmp)

	.section .rodata
	.p2align 4
shift_data:
	.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	.fill 16, 1, -1
	.size shift_data, .-shift_data
