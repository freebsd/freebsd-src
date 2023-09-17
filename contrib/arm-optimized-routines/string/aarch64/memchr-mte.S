/*
 * memchr - find a character in a memory zone
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
#define chrin		w1
#define cntin		x2
#define result		x0

#define src		x3
#define cntrem		x4
#define synd		x5
#define shift		x6
#define	tmp		x7

#define vrepchr		v0
#define qdata		q1
#define vdata		v1
#define vhas_chr	v2
#define vend		v3
#define dend		d3

/*
   Core algorithm:
   For each 16-byte chunk we calculate a 64-bit nibble mask value with four bits
   per byte. We take 4 bits of every comparison byte with shift right and narrow
   by 4 instruction. Since the bits in the nibble mask reflect the order in
   which things occur in the original string, counting leading zeros identifies
   exactly which byte matched.  */

ENTRY (__memchr_aarch64_mte)
	PTR_ARG (0)
	SIZE_ARG (2)
	bic	src, srcin, 15
	cbz	cntin, L(nomatch)
	ld1	{vdata.16b}, [src]
	dup	vrepchr.16b, chrin
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	lsl	shift, srcin, 2
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsr	synd, synd, shift
	cbz	synd, L(start_loop)

	rbit	synd, synd
	clz	synd, synd
	cmp	cntin, synd, lsr 2
	add	result, srcin, synd, lsr 2
	csel	result, result, xzr, hi
	ret

	.p2align 3
L(start_loop):
	sub	tmp, src, srcin
	add	tmp, tmp, 17
	subs	cntrem, cntin, tmp
	b.lo	L(nomatch)

	/* Make sure that it won't overread by a 16-byte chunk */
	tbz	cntrem, 4, L(loop32_2)
	sub	src, src, 16
	.p2align 4
L(loop32):
	ldr	qdata, [src, 32]!
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbnz	synd, L(end)

L(loop32_2):
	ldr	qdata, [src, 16]
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	subs	cntrem, cntrem, 32
	b.lo	L(end_2)
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbz	synd, L(loop32)
L(end_2):
	add	src, src, 16
L(end):
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	sub	cntrem, src, srcin
	fmov	synd, dend
	sub	cntrem, cntin, cntrem
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	clz	synd, synd
	cmp	cntrem, synd, lsr 2
	add	result, src, synd, lsr 2
	csel	result, result, xzr, hi
	ret

L(nomatch):
	mov	result, 0
	ret

END (__memchr_aarch64_mte)

