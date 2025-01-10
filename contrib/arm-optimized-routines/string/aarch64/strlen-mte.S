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
	bic	src, srcin, 15
	ld1	{vdata.16b}, [src]
	cmeq	vhas_nul.16b, vdata.16b, 0
	lsl	shift, srcin, 2
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsr	synd, synd, shift
	cbz	synd, L(next16)

	rbit	synd, synd
	clz	result, synd
	lsr	result, result, 2
	ret

L(next16):
	ldr	data, [src, 16]
	cmeq	vhas_nul.16b, vdata.16b, 0
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	synd, dend
	cbz	synd, L(loop)
	add	src, src, 16
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	sub	result, src, srcin
	clz	tmp, synd
	add	result, result, tmp, lsr 2
	ret

	.p2align 5
L(loop):
	ldr	data, [src, 32]!
	cmeq	vhas_nul.16b, vdata.16b, 0
	addhn	vend.8b, vhas_nul.8h, vhas_nul.8h
	fmov	synd, dend
	cbnz	synd, L(loop_end)
	ldr	data, [src, 16]
	cmeq	vhas_nul.16b, vdata.16b, 0
	addhn	vend.8b, vhas_nul.8h, vhas_nul.8h
	fmov	synd, dend
	cbz	synd, L(loop)
	add	src, src, 16
L(loop_end):
	sub	result, shift, src, lsl 2	/* (srcin - src) << 2.  */
#ifndef __AARCH64EB__
	rbit	synd, synd
	sub	result, result, 3
#endif
	clz	tmp, synd
	sub	result, tmp, result
	lsr	result, result, 2
	ret

END (__strlen_aarch64_mte)
