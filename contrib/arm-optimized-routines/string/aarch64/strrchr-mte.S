/*
 * strrchr - find last position of a character in a string.
 *
 * Copyright (c) 2020-2023, Arm Limited.
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
#define tmp		x3
#define synd		x3
#define shift		x4
#define src_match	x4
#define nul_match	x5
#define chr_match	x6

#define vrepchr		v0
#define vdata		v1
#define vhas_nul	v2
#define vhas_chr	v3
#define vrepmask	v4
#define vend		v5
#define dend		d5

/* Core algorithm.

   For each 16-byte chunk we calculate a 64-bit syndrome value, with
   four bits per byte (LSB is always in bits 0 and 1, for both big
   and little-endian systems).  For each tuple, bits 0-1 are set if
   the relevant byte matched the requested character; bits 2-3 are set
   if the relevant byte matched the NUL end of string.  */

ENTRY (__strrchr_aarch64_mte)
	PTR_ARG (0)
	bic	src, srcin, 15
	dup	vrepchr.16b, chrin
	movi	vrepmask.16b, 0x33
	ld1	{vdata.16b}, [src]
	cmeq	vhas_nul.16b, vdata.16b, 0
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	bit	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	shrn	vend.8b, vhas_nul.8h, 4
	lsl	shift, srcin, 2
	fmov	synd, dend
	lsr	synd, synd, shift
	lsl	synd, synd, shift
	ands	nul_match, synd, 0xcccccccccccccccc
	bne	L(tail)
	cbnz	synd, L(loop2_start)

	.p2align 4
L(loop1):
	ldr	q1, [src, 16]
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	cmhs	vhas_nul.16b, vhas_chr.16b, vdata.16b
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbnz	synd, L(loop1_end)
	ldr	q1, [src, 32]!
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	cmhs	vhas_nul.16b, vhas_chr.16b, vdata.16b
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbz	synd, L(loop1)
	sub	src, src, 16
L(loop1_end):
	add	src, src, 16
	cmeq	vhas_nul.16b, vdata.16b, 0
#ifdef __AARCH64EB__
	bif	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	shrn	vend.8b, vhas_nul.8h, 4
	fmov	synd, dend
	rbit	synd, synd
#else
	bit	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	shrn	vend.8b, vhas_nul.8h, 4
	fmov	synd, dend
#endif
	ands	nul_match, synd, 0xcccccccccccccccc
	beq	L(loop2_start)
L(tail):
	sub	nul_match, nul_match, 1
	and	chr_match, synd, 0x3333333333333333
	ands	chr_match, chr_match, nul_match
	add	result, src, 15
	clz	tmp, chr_match
	sub	result, result, tmp, lsr 2
	csel	result, result, xzr, ne
	ret

	.p2align 4
	nop
	nop
L(loop2_start):
	add	src, src, 16
	bic	vrepmask.8h, 0xf0

L(loop2):
	cmp	synd, 0
	csel	src_match, src, src_match, ne
	csel	chr_match, synd, chr_match, ne
	ld1	{vdata.16b}, [src], 16
	cmeq	vhas_nul.16b, vdata.16b, 0
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	bit	vhas_nul.16b, vhas_chr.16b, vrepmask.16b
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	tst	synd, 0xcccccccccccccccc
	beq	L(loop2)

	bic	vhas_nul.8h, 0x0f, lsl 8
	addp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	and	nul_match, synd, 0xcccccccccccccccc
	sub	nul_match, nul_match, 1
	and	tmp, synd, 0x3333333333333333
	ands	tmp, tmp, nul_match
	csel	chr_match, tmp, chr_match, ne
	csel	src_match, src, src_match, ne
	sub	src_match, src_match, 1
	clz	tmp, chr_match
	sub	result, src_match, tmp, lsr 2
	ret

END (__strrchr_aarch64_mte)

