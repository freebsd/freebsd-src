/*
 * __mtag_tag_region - tag memory
 *
 * Copyright (c) 2021-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Assumptions:
 *
 * ARMv8-a, AArch64, MTE, LP64 ABI.
 *
 * Interface contract:
 * Address is 16 byte aligned and size is multiple of 16.
 * Returns the passed pointer.
 * The memory region may remain untagged if tagging is not enabled.
 */

#include "asmdefs.h"

#if __ARM_FEATURE_MEMORY_TAGGING

#define dstin	x0
#define count	x1
#define dst	x2
#define dstend	x3
#define tmp	x4
#define zva_val	x4

ENTRY (__mtag_tag_region)
	add	dstend, dstin, count

	cmp	count, 96
	b.hi	L(set_long)

	tbnz	count, 6, L(set96)

	/* Set 0, 16, 32, or 48 bytes.  */
	lsr	tmp, count, 5
	add	tmp, dstin, tmp, lsl 4
	cbz     count, L(end)
	stg	dstin, [dstin]
	stg	dstin, [tmp]
	stg	dstin, [dstend, -16]
L(end):
	ret

	.p2align 4
	/* Set 64..96 bytes.  Write 64 bytes from the start and
	   32 bytes from the end.  */
L(set96):
	st2g	dstin, [dstin]
	st2g	dstin, [dstin, 32]
	st2g	dstin, [dstend, -32]
	ret

	.p2align 4
	/* Size is > 96 bytes.  */
L(set_long):
	cmp	count, 160
	b.lo	L(no_zva)

#ifndef SKIP_ZVA_CHECK
	mrs	zva_val, dczid_el0
	and	zva_val, zva_val, 31
	cmp	zva_val, 4		/* ZVA size is 64 bytes.  */
	b.ne	L(no_zva)
#endif
	st2g	dstin, [dstin]
	st2g	dstin, [dstin, 32]
	bic	dst, dstin, 63
	sub	count, dstend, dst	/* Count is now 64 too large.  */
	sub	count, count, 128	/* Adjust count and bias for loop.  */

	.p2align 4
L(zva_loop):
	add	dst, dst, 64
	dc	gva, dst
	subs	count, count, 64
	b.hi	L(zva_loop)
	st2g	dstin, [dstend, -64]
	st2g	dstin, [dstend, -32]
	ret

L(no_zva):
	sub	dst, dstin, 32		/* Dst is biased by -32.  */
	sub	count, count, 64	/* Adjust count for loop.  */
L(no_zva_loop):
	st2g	dstin, [dst, 32]
	st2g	dstin, [dst, 64]!
	subs	count, count, 64
	b.hi	L(no_zva_loop)
	st2g	dstin, [dstend, -64]
	st2g	dstin, [dstend, -32]
	ret

END (__mtag_tag_region)
#endif
