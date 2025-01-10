/*
 * memset - fill memory with a constant byte
 *
 * Copyright (c) 2012-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Assumptions:
 *
 * ARMv8-a, AArch64, Advanced SIMD, unaligned accesses.
 *
 */

#include "asmdefs.h"

#define dstin	x0
#define val	x1
#define valw	w1
#define count	x2
#define dst	x3
#define dstend	x4
#define zva_val	x5
#define off	x3
#define dstend2	x5

ENTRY (__memset_aarch64)
	dup	v0.16B, valw
	cmp	count, 16
	b.lo	L(set_small)

	add	dstend, dstin, count
	cmp	count, 64
	b.hs	L(set_128)

	/* Set 16..63 bytes.  */
	mov	off, 16
	and	off, off, count, lsr 1
	sub	dstend2, dstend, off
	str	q0, [dstin]
	str	q0, [dstin, off]
	str	q0, [dstend2, -16]
	str	q0, [dstend, -16]
	ret

	.p2align 4
	/* Set 0..15 bytes.  */
L(set_small):
	add	dstend, dstin, count
	cmp	count, 4
	b.lo	2f
	lsr	off, count, 3
	sub	dstend2, dstend, off, lsl 2
	str	s0, [dstin]
	str	s0, [dstin, off, lsl 2]
	str	s0, [dstend2, -4]
	str	s0, [dstend, -4]
	ret

	/* Set 0..3 bytes.  */
2:	cbz	count, 3f
	lsr	off, count, 1
	strb	valw, [dstin]
	strb	valw, [dstin, off]
	strb	valw, [dstend, -1]
3:	ret

	.p2align 4
L(set_128):
	bic	dst, dstin, 15
	cmp	count, 128
	b.hi	L(set_long)
	stp	q0, q0, [dstin]
	stp	q0, q0, [dstin, 32]
	stp	q0, q0, [dstend, -64]
	stp	q0, q0, [dstend, -32]
	ret

	.p2align 4
L(set_long):
	str	q0, [dstin]
	str	q0, [dst, 16]
	tst	valw, 255
	b.ne	L(no_zva)
#ifndef SKIP_ZVA_CHECK
	mrs	zva_val, dczid_el0
	and	zva_val, zva_val, 31
	cmp	zva_val, 4		/* ZVA size is 64 bytes.  */
	b.ne	L(no_zva)
#endif
	stp	q0, q0, [dst, 32]
	bic	dst, dstin, 63
	sub	count, dstend, dst	/* Count is now 64 too large.  */
	sub	count, count, 64 + 64	/* Adjust count and bias for loop.  */

	/* Write last bytes before ZVA loop.  */
	stp	q0, q0, [dstend, -64]
	stp	q0, q0, [dstend, -32]

	.p2align 4
L(zva64_loop):
	add	dst, dst, 64
	dc	zva, dst
	subs	count, count, 64
	b.hi	L(zva64_loop)
	ret

	.p2align 3
L(no_zva):
	sub	count, dstend, dst	/* Count is 32 too large.  */
	sub	count, count, 64 + 32	/* Adjust count and bias for loop.  */
L(no_zva_loop):
	stp	q0, q0, [dst, 32]
	stp	q0, q0, [dst, 64]
	add	dst, dst, 64
	subs	count, count, 64
	b.hi	L(no_zva_loop)
	stp	q0, q0, [dstend, -64]
	stp	q0, q0, [dstend, -32]
	ret

END (__memset_aarch64)
