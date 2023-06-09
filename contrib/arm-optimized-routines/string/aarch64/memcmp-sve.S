/*
 * memcmp - compare memory
 *
 * Copyright (c) 2018-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "asmdefs.h"

#if __ARM_FEATURE_SVE
/* Assumptions:
 *
 * ARMv8-a, AArch64
 * SVE Available.
 */

ENTRY (__memcmp_aarch64_sve)
	PTR_ARG (0)
	PTR_ARG (1)
	SIZE_ARG (2)
	mov	x3, 0			/* initialize off */

0:	whilelo	p0.b, x3, x2		/* while off < max */
	b.none	9f

	ld1b	z0.b, p0/z, [x0, x3]	/* read vectors bounded by max.  */
	ld1b	z1.b, p0/z, [x1, x3]

	/* Increment for a whole vector, even if we've only read a partial.
	   This is significantly cheaper than INCP, and since OFF is not
	   used after the loop it is ok to increment OFF past MAX.  */
	incb	x3

	cmpne	p1.b, p0/z, z0.b, z1.b	/* while no inequalities */
	b.none	0b

	/* Found inequality.  */
1:	brkb	p1.b, p0/z, p1.b	/* find first such */
	lasta	w0, p1, z0.b		/* extract each byte */
	lasta	w1, p1, z1.b
	sub	x0, x0, x1		/* return comparison */
	ret

	/* Found end-of-count.  */
9:	mov	x0, 0			/* return equality */
	ret

END (__memcmp_aarch64_sve)

#endif

