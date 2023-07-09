/*
 * memcpy - copy memory area
 *
 * Copyright (c) 2012-2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

/* Assumptions:
 *
 * ARMv8-a, AArch64, unaligned accesses.
 *
 */

#include <machine/asm.h>

#define L(l) .L ## l

#define dstin	x0
#define src	x1
#define count	x2
#define dst	x3
#define srcend	x4
#define dstend	x5
#define A_l	x6
#define A_lw	w6
#define A_h	x7
#define B_l	x8
#define B_lw	w8
#define B_h	x9
#define C_l	x10
#define C_lw	w10
#define C_h	x11
#define D_l	x12
#define D_h	x13
#define E_l	x14
#define E_h	x15
#define F_l	x16
#define F_h	x17
#define G_l	count
#define G_h	dst
#define H_l	src
#define H_h	srcend
#define tmp1	x14

/* This implementation handles overlaps and supports both memcpy and memmove
   from a single entry point.  It uses unaligned accesses and branchless
   sequences to keep the code small, simple and improve performance.

   Copies are split into 3 main cases: small copies of up to 32 bytes, medium
   copies of up to 128 bytes, and large copies.  The overhead of the overlap
   check is negligible since it is only required for large copies.

   Large copies use a software pipelined loop processing 64 bytes per iteration.
   The destination pointer is 16-byte aligned to minimize unaligned accesses.
   The loop tail is handled by always copying 64 bytes from the end.
*/

EENTRY(memmove)
ENTRY(memcpy)
	add	srcend, src, count
	add	dstend, dstin, count
	cmp	count, 128
	b.hi	L(copy_long)
	cmp	count, 32
	b.hi	L(copy32_128)

	/* Small copies: 0..32 bytes.  */
	cmp	count, 16
	b.lo	L(copy16)
	ldp	A_l, A_h, [src]
	ldp	D_l, D_h, [srcend, -16]
	stp	A_l, A_h, [dstin]
	stp	D_l, D_h, [dstend, -16]
	ret

	/* Copy 8-15 bytes.  */
L(copy16):
	tbz	count, 3, L(copy8)
	ldr	A_l, [src]
	ldr	A_h, [srcend, -8]
	str	A_l, [dstin]
	str	A_h, [dstend, -8]
	ret

	.p2align 3
	/* Copy 4-7 bytes.  */
L(copy8):
	tbz	count, 2, L(copy4)
	ldr	A_lw, [src]
	ldr	B_lw, [srcend, -4]
	str	A_lw, [dstin]
	str	B_lw, [dstend, -4]
	ret

	/* Copy 0..3 bytes using a branchless sequence.  */
L(copy4):
	cbz	count, L(copy0)
	lsr	tmp1, count, 1
	ldrb	A_lw, [src]
	ldrb	C_lw, [srcend, -1]
	ldrb	B_lw, [src, tmp1]
	strb	A_lw, [dstin]
	strb	B_lw, [dstin, tmp1]
	strb	C_lw, [dstend, -1]
L(copy0):
	ret

	.p2align 4
	/* Medium copies: 33..128 bytes.  */
L(copy32_128):
	ldp	A_l, A_h, [src]
	ldp	B_l, B_h, [src, 16]
	ldp	C_l, C_h, [srcend, -32]
	ldp	D_l, D_h, [srcend, -16]
	cmp	count, 64
	b.hi	L(copy128)
	stp	A_l, A_h, [dstin]
	stp	B_l, B_h, [dstin, 16]
	stp	C_l, C_h, [dstend, -32]
	stp	D_l, D_h, [dstend, -16]
	ret

	.p2align 4
	/* Copy 65..128 bytes.  */
L(copy128):
	ldp	E_l, E_h, [src, 32]
	ldp	F_l, F_h, [src, 48]
	cmp	count, 96
	b.ls	L(copy96)
	ldp	G_l, G_h, [srcend, -64]
	ldp	H_l, H_h, [srcend, -48]
	stp	G_l, G_h, [dstend, -64]
	stp	H_l, H_h, [dstend, -48]
L(copy96):
	stp	A_l, A_h, [dstin]
	stp	B_l, B_h, [dstin, 16]
	stp	E_l, E_h, [dstin, 32]
	stp	F_l, F_h, [dstin, 48]
	stp	C_l, C_h, [dstend, -32]
	stp	D_l, D_h, [dstend, -16]
	ret

	.p2align 4
	/* Copy more than 128 bytes.  */
L(copy_long):
	/* Use backwards copy if there is an overlap.  */
	sub	tmp1, dstin, src
	cbz	tmp1, L(copy0)
	cmp	tmp1, count
	b.lo	L(copy_long_backwards)

	/* Copy 16 bytes and then align dst to 16-byte alignment.  */

	ldp	D_l, D_h, [src]
	and	tmp1, dstin, 15
	bic	dst, dstin, 15
	sub	src, src, tmp1
	add	count, count, tmp1	/* Count is now 16 too large.  */
	ldp	A_l, A_h, [src, 16]
	stp	D_l, D_h, [dstin]
	ldp	B_l, B_h, [src, 32]
	ldp	C_l, C_h, [src, 48]
	ldp	D_l, D_h, [src, 64]!
	subs	count, count, 128 + 16	/* Test and readjust count.  */
	b.ls	L(copy64_from_end)

L(loop64):
	stp	A_l, A_h, [dst, 16]
	ldp	A_l, A_h, [src, 16]
	stp	B_l, B_h, [dst, 32]
	ldp	B_l, B_h, [src, 32]
	stp	C_l, C_h, [dst, 48]
	ldp	C_l, C_h, [src, 48]
	stp	D_l, D_h, [dst, 64]!
	ldp	D_l, D_h, [src, 64]!
	subs	count, count, 64
	b.hi	L(loop64)

	/* Write the last iteration and copy 64 bytes from the end.  */
L(copy64_from_end):
	ldp	E_l, E_h, [srcend, -64]
	stp	A_l, A_h, [dst, 16]
	ldp	A_l, A_h, [srcend, -48]
	stp	B_l, B_h, [dst, 32]
	ldp	B_l, B_h, [srcend, -32]
	stp	C_l, C_h, [dst, 48]
	ldp	C_l, C_h, [srcend, -16]
	stp	D_l, D_h, [dst, 64]
	stp	E_l, E_h, [dstend, -64]
	stp	A_l, A_h, [dstend, -48]
	stp	B_l, B_h, [dstend, -32]
	stp	C_l, C_h, [dstend, -16]
	ret

	.p2align 4

	/* Large backwards copy for overlapping copies.
	   Copy 16 bytes and then align dst to 16-byte alignment.  */
L(copy_long_backwards):
	ldp	D_l, D_h, [srcend, -16]
	and	tmp1, dstend, 15
	sub	srcend, srcend, tmp1
	sub	count, count, tmp1
	ldp	A_l, A_h, [srcend, -16]
	stp	D_l, D_h, [dstend, -16]
	ldp	B_l, B_h, [srcend, -32]
	ldp	C_l, C_h, [srcend, -48]
	ldp	D_l, D_h, [srcend, -64]!
	sub	dstend, dstend, tmp1
	subs	count, count, 128
	b.ls	L(copy64_from_start)

L(loop64_backwards):
	stp	A_l, A_h, [dstend, -16]
	ldp	A_l, A_h, [srcend, -16]
	stp	B_l, B_h, [dstend, -32]
	ldp	B_l, B_h, [srcend, -32]
	stp	C_l, C_h, [dstend, -48]
	ldp	C_l, C_h, [srcend, -48]
	stp	D_l, D_h, [dstend, -64]!
	ldp	D_l, D_h, [srcend, -64]!
	subs	count, count, 64
	b.hi	L(loop64_backwards)

	/* Write the last iteration and copy 64 bytes from the start.  */
L(copy64_from_start):
	ldp	G_l, G_h, [src, 48]
	stp	A_l, A_h, [dstend, -16]
	ldp	A_l, A_h, [src, 32]
	stp	B_l, B_h, [dstend, -32]
	ldp	B_l, B_h, [src, 16]
	stp	C_l, C_h, [dstend, -48]
	ldp	C_l, C_h, [src]
	stp	D_l, D_h, [dstend, -64]
	stp	G_l, G_h, [dstin, 48]
	stp	A_l, A_h, [dstin, 32]
	stp	B_l, B_h, [dstin, 16]
	stp	C_l, C_h, [dstin]
	ret
END(memcpy)
EEND(memmove)

ENTRY(disasm_str_test)
	/* str immediate post-index */
	str	w0, [x3], #-256
	str	w1, [sp], #255

	str	x2, [x4], #1
	str	x0, [sp], #-1

	/* str immedate pre-index */
	str	w0, [x3, #-256]!
	str	w1, [sp, #255]!

	str	x2, [x4, #1]
	str	x0, [sp, #-1]

	/* str immediate unsigned offset */
	str	w0, [x3]
	str	w0, [x3, #16380]
	str	w1, [sp, #16380]

	str	x0, [x3]
	str	x2, [x3, #32760]
	str	x3, [sp, #32760]

	/* str register */
	str	w0, [x1, x2, lsl #2]
	str	w0, [x1, w2, uxtw #2]
	str	w0, [sp, w2, sxtw]
	str	w0, [sp, x2, sxtx #0]

	str	x0, [x1, x2, lsl #3]
	str	x0, [x1, w2, uxtw #3]
	str	x0, [sp, w2, sxtw]
	str	x0, [sp, x2, sxtx #0]

	/* strb immediate post-index */
	strb	w0, [x3], #-256
	strb	w1, [sp], #255

	/* strb immediate pre-index */
	strb	w0, [x3, #-256]!
	strb	w1, [sp, #255]!

	/* strb immediate unsigned offset */
	strb	w0, [x3]
	strb	w0, [x3, #4095]
	strb	w1, [sp, #4095]

	/* strb register */
	strb	w0, [x1, x2, sxtx #0]
	strb	w0, [x1, x2, lsl #0]
	strb	w0, [x1, w2, uxtw]
	strb	w0, [sp, w2, sxtw]
	strb	w0, [sp, x2, sxtx]

	/* strh immediate post-index */
	strh	w0, [x3], #-256
	strh	w1, [sp], #255

	/* strh immediate pre-index */
	strh	w0, [x3, #-256]!
	strh	w1, [sp, #255]!

	/* strh immediate unsigned offset */
	strh	w0, [x3]
	strh	w0, [x3, #8190]
	strh	w1, [sp, #8190]
	strh	wzr, [sp, #8190]
	
	/* strh register */
	strh	w0, [x1, w2, uxtw #1]
	strh	w0, [x1, x2, lsl #1]
	strh	w0, [sp, w2, sxtw]
	strh	w0, [sp, x2, sxtx #0]
	strh	wzr, [sp, x2, sxtx #0]
END(disasm_str_test)

ENTRY(disasm_shifted_reg_with_rsv)
	/* adds shifted regiser */
	adds	w0, w1, w3, asr #1
	adds	x0, x1, x2, lsl #2

	/* cmn shifted register, alias of adds */
	cmn	w0, w4, lsr #31
	cmn	x1, x10, asr #63

	/* subs shifted register */
	subs	w0, w11, w12, lsr #0
	subs	x1, x12, x11, lsl #63

	/* cmp shifted register, alias of subs */
	cmp	w0, w1, asr #31
	cmp	x0, x2, lsl #63

	/* negs shifted register, alias of subs */
	negs	w0, w1, lsr #31
	negs	x0, x20, asr #0

	/* sub shifted register */
	sub	w0, w1, w2, lsl #1
	sub	x0, x2, x1, lsl #5

	/* neg shifted register */
	neg	w0, w1, lsl #2
	neg	x2, x3, asr #8
END(disasm_shifted_reg_with_rsv)

ENTRY(disasm_shifted_reg_with_ror)
	/* mvn shifted register */
	mvn	w0, w1
	mvn	x0, x1

	mvn	w0, w1, ror #3
	mvn	x0, x1, ror #2

	mvn	w0, wzr, asr #2
	mvn	x0, xzr, lsl #1

	/* orn shifted register */
	orn	w0, wzr, w1
	orn	x0, xzr, x1
	
	orn	w0, w1, w2, ror #3
	orn	x0, x1, x2, ror #2

	orn	w0, w2, wzr, asr #2
	orn	x0, x2, xzr, lsr #2

	/* orr shifted register */
	orr	w0, wzr, w1
	orr	x0, xzr, x1
	
	orr	w0, w1, w2, ror #3
	orr	x0, x1, x2, ror #2

	orr	w0, w2, wzr, asr #2
	orr	x0, x2, xzr, lsr #2

	/* and shifted register */
	and	w0, wzr, w1
	and	x0, xzr, x1
	
	and	w0, w1, w2, ror #3
	and	x0, x1, x2, ror #2

	and	w0, w2, wzr, asr #2
	and	x0, x2, xzr, lsr #2

	/* ands shifted register */
	ands	w0, wzr, w1
	ands	x0, xzr, x1
	
	ands	w0, w1, w2, ror #3
	ands	x0, x1, x2, ror #2

	ands	w0, w2, wzr, asr #2
	ands	x0, x2, xzr, lsr #2

	/* bic shifted register */
	bic	w0, wzr, w1
	bic	x0, xzr, x1
	
	bic	w0, w1, w2, ror #3
	bic	x0, x1, x2, ror #2

	bic	w0, w2, wzr, asr #2
	bic	x0, x2, xzr, lsr #2

	/* bics shifted register */
	bics	w0, wzr, w1
	bics	x0, xzr, x1
	
	bics	w0, w1, w2, ror #3
	bics	x0, x1, x2, ror #2

	bics	w0, w2, wzr, asr #2
	bics	x0, x2, xzr, lsr #2

	/* eon shifted register */
	eon	w0, wzr, w1
	eon	x0, xzr, x1
	
	eon	w0, w1, w2, ror #3
	eon	x0, x1, x2, ror #2

	eon	w0, w2, wzr, asr #2
	eon	x0, x2, xzr, lsr #2

	/* eor shifted register */
	eor	w0, wzr, w1
	eor	x0, xzr, x1
	
	eor	w0, w1, w2, ror #3
	eor	x0, x1, x2, ror #2

	eor	w0, w2, wzr, asr #2
	eor	x0, x2, xzr, lsr #2

	/* tst shifted register */
	tst	w0, w1
	tst	x0, x1

	tst	w0, w1, ror #3
	tst	x0, x1, ror #2

	tst	w0, wzr, asr #2
	tst	x0, xzr, lsl #1
END(disasm_shifter_reg_with_ror)

ENTRY(disasm_ext_reg)
	/* add extended register */
	add	w1,  w2,  w3, uxtb #1
	add	wsp, w2,  w3, uxth #2
	add	wsp, wsp, w3, lsl  #3
	add	w1,  wsp, w3, lsl  #1
	add	wsp, w2,  w3, lsl  #4
	add	w1,  w2,  w3, uxtw
	add	w1,  wsp, w3, uxtx
	add	wsp, w2,  w3, sxtb #1
	add	wsp, wsp, w3, sxth #2
	add	w1,  wsp, w3, sxtw #3
	add	w1,  wsp, w3, sxtx #4

	add	x1, x2, w3, uxtb #1
	add	sp, x2, w3, uxth #2
	add	sp, sp, x3, lsl  #3
	add	x1, sp, x3, lsl  #1
	add	sp, x2, x3, lsl  #4
	add	x1, x2, w3, uxtw
	add	x1, sp, x3, uxtx
	add	sp, x2, w3, sxtb #1
	add	sp, sp, w3, sxth #2
	add	x1, sp, w3, sxtw #3
	add	x1, sp, x3, sxtx #4

	/* adds extended register */
	adds	w1,  w2,  w3, uxtb #1
	adds	w1,  w2,  w3, uxth #2
	adds	w1,  wsp, w3, lsl  #3
	adds	w1,  wsp, w3, lsl  #1
	adds	w1,  w2,  w3, lsl  #4
	adds	w1,  w2,  w3, uxtw
	adds	w1,  wsp, w3, uxtx
	adds	w1,  w2,  w3, sxtb #1
	adds	w1,  wsp, w3, sxth #2
	adds	w1,  wsp, w3, sxtw #3
	adds	w1,  wsp, w3, sxtx #4

	adds	x1, x2, w3, uxtb #1
	adds	x1, x2, w3, uxth #2
	adds	x1, sp, x3, lsl  #3
	adds	x1, sp, x3, lsl  #1
	adds	x1, x2, x3, lsl  #4
	adds	x1, x2, w3, uxtw
	adds	x1, sp, x3, uxtx
	adds	x1, x2, w3, sxtb #1
	adds	x1, sp, w3, sxth #2
	adds	x1, sp, w3, sxtw #3
	adds	x1, sp, x3, sxtx #4

	/* cmn extended register */
	cmn	w2,  w3, uxtb #1
	cmn	w2,  w3, uxth #2
	cmn	wsp, w3, lsl  #3
	cmn	wsp, w3, lsl  #1
	cmn	w2,  w3, lsl  #4
	cmn	w2,  w3, uxtw
	cmn	wsp, w3, uxtx
	cmn	w2,  w3, sxtb #1
	cmn	wsp, w3, sxth #2
	cmn	wsp, w3, sxtw #3
	cmn	wsp, w3, sxtx #4

	cmn	x2, w3, uxtb #1
	cmn	x2, w3, uxth #2
	cmn	sp, x3, lsl  #3
	cmn	sp, x3, lsl  #1
	cmn	x2, x3, lsl  #4
	cmn	x2, w3, uxtw
	cmn	sp, x3, uxtx
	cmn	x2, w3, sxtb #1
	cmn	sp, w3, sxth #2
	cmn	sp, w3, sxtw #3
	cmn	sp, x3, sxtx #4

	/* cmp extended register */
	cmp	w2,  w3, uxtb #1
	cmp	w2,  w3, uxth #2
	cmp	wsp, w3, lsl  #3
	cmp	wsp, w3, lsl  #1
	cmp	w2,  w3, lsl  #4
	cmp	w2,  w3, uxtw
	cmp	wsp, w3, uxtx
	cmp	w2,  w3, sxtb #1
	cmp	wsp, w3, sxth #2
	cmp	wsp, w3, sxtw #3
	cmp	wsp, w3, sxtx #4

	cmp	x2, w3, uxtb #1
	cmp	x2, w3, uxth #2
	cmp	sp, x3, lsl  #3
	cmp	sp, x3, lsl  #1
	cmp	x2, x3, lsl  #4
	cmp	x2, w3, uxtw
	cmp	sp, x3, uxtx
	cmp	x2, w3, sxtb #1
	cmp	sp, w3, sxth #2
	cmp	sp, w3, sxtw #3
	cmp	sp, x3, sxtx #4

	/* sub extended register */
	sub	w1,  w2,  w3, uxtb #1
	sub	wsp, w2,  w3, uxth #2
	sub	wsp, wsp, w3, lsl  #3
	sub	w1,  wsp, w3, lsl  #1
	sub	wsp, w2,  w3, lsl  #4
	sub	w1,  w2,  w3, uxtw
	sub	w1,  wsp, w3, uxtx
	sub	wsp, w2,  w3, sxtb #1
	sub	wsp, wsp, w3, sxth #2
	sub	w1,  wsp, w3, sxtw #3
	sub	w1,  wsp, w3, sxtx #4

	sub	x1, x2, w3, uxtb #1
	sub	sp, x2, w3, uxth #2
	sub	sp, sp, x3, lsl  #3
	sub	x1, sp, x3, lsl  #1
	sub	sp, x2, x3, lsl  #4
	sub	x1, x2, w3, uxtw
	sub	x1, sp, x3, uxtx
	sub	sp, x2, w3, sxtb #1
	sub	sp, sp, w3, sxth #2
	sub	x1, sp, w3, sxtw #3
	sub	x1, sp, x3, sxtx #4
	
	/* subs extended register */
	subs	w1,  w2,  w3, uxtb #1
	subs	w1,  w2,  w3, uxth #2
	subs	w1,  wsp, w3, lsl  #3
	subs	w1,  wsp, w3, lsl  #1
	subs	w1,  w2,  w3, lsl  #4
	subs	w1,  w2,  w3, uxtw
	subs	w1,  wsp, w3, uxtx
	subs	w1,  w2,  w3, sxtb #1
	subs	w1,  wsp, w3, sxth #2
	subs	w1,  wsp, w3, sxtw #3
	subs	w1,  wsp, w3, sxtx #4

	subs	x1, x2, w3, uxtb #1
	subs	x1, x2, w3, uxth #2
	subs	x1, sp, x3, lsl  #3
	subs	x1, sp, x3, lsl  #1
	subs	x1, x2, x3, lsl  #4
	subs	x1, x2, w3, uxtw
	subs	x1, sp, x3, uxtx
	subs	x1, x2, w3, sxtb #1
	subs	x1, sp, w3, sxth #2
	subs	x1, sp, w3, sxtw #3
	subs	x1, sp, x3, sxtx #4
END(disasm_ext_reg)

