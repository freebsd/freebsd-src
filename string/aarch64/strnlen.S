/*
 * strnlen - calculate the length of a string with limit.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

/* Assumptions:
 *
 * ARMv8-a, AArch64, Advanced SIMD.
 * MTE compatible.
 */

#include "../asmdefs.h"

#define srcin		x0
#define cntin		x1
#define result		x0

#define src		x2
#define synd		x3
#define	shift		x4
#define wtmp		w4
#define tmp		x4
#define cntrem		x5

#define qdata		q0
#define vdata		v0
#define vhas_chr	v1
#define vrepmask	v2
#define vend		v3
#define dend		d3

/*
   Core algorithm:

   For each 16-byte chunk we calculate a 64-bit syndrome value with four bits
   per byte. For even bytes, bits 0-3 are set if the relevant byte matched the
   requested character or the byte is NUL. Bits 4-7 must be zero. Bits 4-7 are
   set likewise for odd bytes so that adjacent bytes can be merged. Since the
   bits in the syndrome reflect the order in which things occur in the original
   string, counting trailing zeros identifies exactly which byte matched.  */

ENTRY (__strnlen_aarch64)
	PTR_ARG (0)
	SIZE_ARG (1)
	bic	src, srcin, 15
	mov	wtmp, 0xf00f
	cbz	cntin, L(nomatch)
	ld1	{vdata.16b}, [src], 16
	dup	vrepmask.8h, wtmp
	cmeq	vhas_chr.16b, vdata.16b, 0
	lsl	shift, srcin, 2
	and	vhas_chr.16b, vhas_chr.16b, vrepmask.16b
	addp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
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

L(start_loop):
	sub	tmp, src, srcin
	subs	cntrem, cntin, tmp
	b.ls	L(nomatch)

	/* Make sure that it won't overread by a 16-byte chunk */
	add	tmp, cntrem, 15
	tbnz	tmp, 4, L(loop32_2)

	.p2align 5
L(loop32):
	ldr	qdata, [src], 16
	cmeq	vhas_chr.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbnz	synd, L(end)
L(loop32_2):
	ldr	qdata, [src], 16
	subs	cntrem, cntrem, 32
	cmeq	vhas_chr.16b, vdata.16b, 0
	b.ls	L(end)
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbz	synd, L(loop32)

L(end):
	and	vhas_chr.16b, vhas_chr.16b, vrepmask.16b
	addp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	sub	src, src, 16
	mov	synd, vend.d[0]
	sub	result, src, srcin
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	clz	synd, synd
	add	result, result, synd, lsr 2
	cmp	cntin, result
	csel	result, cntin, result, ls
	ret

L(nomatch):
	mov	result, cntin
	ret

END (__strnlen_aarch64)

