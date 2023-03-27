/*
 * strlen - calculate the length of a string.
 *
 * Copyright (c) 2020-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Assumptions:
 *
 * ARMv8-a, AArch64, Advanced SIMD.
 * MTE compatible.
 */

#include "asmdefs.h"

#define srcin		x0
#define result		x0

#define src		x1
#define	synd		x2
#define tmp		x3
#define shift		x4

#define data		q0
#define vdata		v0
#define vhas_nul	v1
#define vend		v2
#define dend		d2

/* Core algorithm:
   Process the string in 16-byte aligned chunks. Compute a 64-bit mask with
   four bits per byte using the shrn instruction. A count trailing zeros then
   identifies the first zero byte.  */

ENTRY (__strlen_aarch64_mte)
	PTR_ARG (0)
	bic	src, srcin, 15
	ld1	{vdata.16b}, [src]
	cmeq	vhas_nul.16b, vdata.16b, 0
	lsl	shift, srcin, 2
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsr	synd, synd, shift
	cbz	synd, L(loop)

	rbit	synd, synd
	clz	result, synd
	lsr	result, result, 2
	ret

	.p2align 5
L(loop):
	ldr	data, [src, 16]
	cmeq	vhas_nul.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbnz	synd, L(loop_end)
	ldr	data, [src, 32]!
	cmeq	vhas_nul.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbz	synd, L(loop)
	sub	src, src, 16
L(loop_end):
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	sub	result, src, srcin
	fmov	synd, dend
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	add	result, result, 16
	clz	tmp, synd
	add	result, result, tmp, lsr 2
	ret

END (__strlen_aarch64_mte)

