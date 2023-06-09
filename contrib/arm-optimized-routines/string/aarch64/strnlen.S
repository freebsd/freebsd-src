/*
 * strnlen - calculate the length of a string with limit.
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
#define cntin		x1
#define result		x0

#define src		x2
#define synd		x3
#define	shift		x4
#define tmp		x4
#define cntrem		x5

#define qdata		q0
#define vdata		v0
#define vhas_chr	v1
#define vend		v2
#define dend		d2

/*
   Core algorithm:
   Process the string in 16-byte aligned chunks. Compute a 64-bit mask with
   four bits per byte using the shrn instruction. A count trailing zeros then
   identifies the first zero byte.  */

ENTRY (__strnlen_aarch64)
	PTR_ARG (0)
	SIZE_ARG (1)
	bic	src, srcin, 15
	cbz	cntin, L(nomatch)
	ld1	{vdata.16b}, [src]
	cmeq	vhas_chr.16b, vdata.16b, 0
	lsl	shift, srcin, 2
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsr	synd, synd, shift
	cbz	synd, L(start_loop)
L(finish):
	rbit	synd, synd
	clz	synd, synd
	lsr	result, synd, 2
	cmp	cntin, result
	csel	result, cntin, result, ls
	ret

L(nomatch):
	mov	result, cntin
	ret

L(start_loop):
	sub	tmp, src, srcin
	add	tmp, tmp, 17
	subs	cntrem, cntin, tmp
	b.lo	L(nomatch)

	/* Make sure that it won't overread by a 16-byte chunk */
	tbz	cntrem, 4, L(loop32_2)
	sub	src, src, 16
	.p2align 5
L(loop32):
	ldr	qdata, [src, 32]!
	cmeq	vhas_chr.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbnz	synd, L(end)
L(loop32_2):
	ldr	qdata, [src, 16]
	subs	cntrem, cntrem, 32
	cmeq	vhas_chr.16b, vdata.16b, 0
	b.lo	L(end_2)
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbz	synd, L(loop32)
L(end_2):
	add	src, src, 16
L(end):
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	sub	result, src, srcin
	fmov	synd, dend
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	clz	synd, synd
	add	result, result, synd, lsr 2
	cmp	cntin, result
	csel	result, cntin, result, ls
	ret

END (__strnlen_aarch64)

