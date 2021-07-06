/*
 * strnlen - calculate the length of a string with limit.
 *
 * Copyright (c) 2019-2021, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#include "../asmdefs.h"

#if __ARM_FEATURE_SVE
/* Assumptions:
 *
 * ARMv8-a, AArch64
 * SVE Available.
 */

ENTRY (__strnlen_aarch64_sve)
	PTR_ARG (0)
	SIZE_ARG (1)
	setffr				/* initialize FFR */
	mov	x2, 0			/* initialize len */
	b	1f

	.p2align 4
	/* We have off + vl <= max, and so may read the whole vector.  */
0:	ldff1b	z0.b, p0/z, [x0, x2]
	rdffrs	p1.b, p0/z
	b.nlast	2f

	/* First fault did not fail: the whole vector is valid.
	   Avoid depending on the contents of FFR beyond the branch.  */
	cmpeq	p2.b, p0/z, z0.b, 0
	b.any	8f
	incb	x2

1:	whilelo	p0.b, x2, x1
	b.last	0b

	/* We have off + vl < max.  Test for off == max before proceeding.  */
	b.none	9f

	ldff1b	z0.b, p0/z, [x0, x2]
	rdffrs	p1.b, p0/z
	b.nlast	2f

	/* First fault did not fail: the vector up to max is valid.
	   Avoid depending on the contents of FFR beyond the branch.
	   Compare for end-of-string, but there are no more bytes.  */
	cmpeq	p2.b, p0/z, z0.b, 0

	/* Found end-of-string or zero.  */
8:	brkb	p2.b, p0/z, p2.b
	mov	x0, x2
	incp	x0, p2.b
	ret

	/* First fault failed: only some of the vector is valid.
	   Perform the comparison only on the valid bytes.  */
2:	cmpeq	p2.b, p1/z, z0.b, 0
	b.any	8b

	/* No inequality or zero found.  Re-init FFR, incr and loop.  */
	setffr
	incp	x2, p1.b
	b	1b

	/* End of count.  Return max.  */
9:	mov	x0, x1
	ret

END (__strnlen_aarch64_sve)

#endif

