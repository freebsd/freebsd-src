/*
 * strchr - find a character in a string
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
#define result		x0

#define src		x2
#define tmp1		x1
#define tmp2		x3

#define vrepchr		v0
#define vdata		v1
#define qdata		q1
#define vhas_nul	v2
#define vhas_chr	v3
#define vrepmask	v4
#define vend		v5
#define dend		d5

/* Core algorithm.

   For each 16-byte chunk we calculate a 64-bit syndrome value with four bits
   per byte. Bits 0-1 are set if the relevant byte matched the requested
   character, bits 2-3 are set if the byte is NUL or matched. Count trailing
   zeroes gives the position of the matching byte if it is a multiple of 4.
   If it is not a multiple of 4, there was no match.  */

ENTRY (__strchr_aarch64_mte)
	PTR_ARG (0)
	bic	src, srcin, 15
	dup	vrepchr.16b, chrin
	ld1	{vdata.16b}, [src]
	movi	vrepmask.16b, 0x33
	cmeq	vhas_nul.16b, vdata.16b, 0
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	bit	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	lsl	tmp2, srcin, 2
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	tmp1, dend
	lsr	tmp1, tmp1, tmp2
	cbz	tmp1, L(loop)

	rbit	tmp1, tmp1
	clz	tmp1, tmp1
	/* Tmp1 is an even multiple of 2 if the target character was
	   found first. Otherwise we've found the end of string.  */
	tst	tmp1, 2
	add	result, srcin, tmp1, lsr 2
	csel	result, result, xzr, eq
	ret

	.p2align 4
L(loop):
	ldr	qdata, [src, 16]
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	cmhs	vhas_nul.16b, vhas_chr.16b, vdata.16b
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	tmp1, dend
	cbnz	tmp1, L(end)
	ldr	qdata, [src, 32]!
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	cmhs	vhas_nul.16b, vhas_chr.16b, vdata.16b
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	tmp1, dend
	cbz	tmp1, L(loop)
	sub	src, src, 16
L(end):

#ifdef __AARCH64EB__
	bif	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	tmp1, dend
#else
	bit	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	tmp1, dend
	rbit	tmp1, tmp1
#endif
	add	src, src, 16
	clz	tmp1, tmp1
	/* Tmp1 is a multiple of 4 if the target character was found.  */
	tst	tmp1, 2
	add	result, src, tmp1, lsr 2
	csel	result, result, xzr, eq
	ret

END (__strchr_aarch64_mte)

