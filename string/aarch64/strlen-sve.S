/*
 * __strlen_aarch64_sve - compute the length of a string
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

ENTRY (__strlen_aarch64_sve)
	PTR_ARG (0)
	setffr			/* initialize FFR */
	ptrue	p2.b		/* all ones; loop invariant */
	mov	x1, 0		/* initialize length */

	/* Read a vector's worth of bytes, stopping on first fault.  */
	.p2align 4
0:	ldff1b	z0.b, p2/z, [x0, x1]
	rdffrs	p0.b, p2/z
	b.nlast	2f

	/* First fault did not fail: the whole vector is valid.
	   Avoid depending on the contents of FFR beyond the branch.  */
	incb	x1, all			/* speculate increment */
	cmpeq	p1.b, p2/z, z0.b, 0	/* loop if no zeros */
	b.none	0b
	decb	x1, all			/* undo speculate */

	/* Zero found.  Select the bytes before the first and count them.  */
1:	brkb	p0.b, p2/z, p1.b
	incp	x1, p0.b
	mov	x0, x1
	ret

	/* First fault failed: only some of the vector is valid.
	   Perform the comparison only on the valid bytes.  */
2:	cmpeq	p1.b, p0/z, z0.b, 0
	b.any	1b

	/* No zero found.  Re-init FFR, increment, and loop.  */
	setffr
	incp	x1, p0.b
	b	0b

END (__strlen_aarch64_sve)

#endif

