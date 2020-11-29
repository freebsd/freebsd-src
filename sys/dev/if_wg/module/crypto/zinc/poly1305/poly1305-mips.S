/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2016-2018 René van Dorst <opensource@vdorst.com> All Rights Reserved.
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MSB 0
#define LSB 3
#else
#define MSB 3
#define LSB 0
#endif

#define POLY1305_BLOCK_SIZE 16
.text
#define H0 $t0
#define H1 $t1
#define H2 $t2
#define H3 $t3
#define H4 $t4

#define R0 $t5
#define R1 $t6
#define R2 $t7
#define R3 $t8

#define O0 $s0
#define O1 $s4
#define O2 $v1
#define O3 $t9
#define O4 $s5

#define S1 $s1
#define S2 $s2
#define S3 $s3

#define SC $at
#define CA $v0

/* Input arguments */
#define poly	$a0
#define src	$a1
#define srclen	$a2
#define hibit	$a3

/* Location in the opaque buffer
 * R[0..3], CA, H[0..4]
 */
#define PTR_POLY1305_R(n) ( 0 + (n*4)) ## ($a0)
#define PTR_POLY1305_CA   (16        ) ## ($a0)
#define PTR_POLY1305_H(n) (20 + (n*4)) ## ($a0)

#define POLY1305_BLOCK_SIZE 16
#define POLY1305_STACK_SIZE 32

.set	noat
.align	4
.globl	poly1305_blocks_mips
.ent	poly1305_blocks_mips
poly1305_blocks_mips:
	.frame	$sp, POLY1305_STACK_SIZE, $ra
	/* srclen &= 0xFFFFFFF0 */
	ins	srclen, $zero, 0, 4

	addiu	$sp, -(POLY1305_STACK_SIZE)

	/* check srclen >= 16 bytes */
	beqz	srclen, .Lpoly1305_blocks_mips_end

	/* Calculate last round based on src address pointer.
	 * last round src ptr (srclen) = src + (srclen & 0xFFFFFFF0)
	 */
	addu	srclen, src

	lw	R0, PTR_POLY1305_R(0)
	lw	R1, PTR_POLY1305_R(1)
	lw	R2, PTR_POLY1305_R(2)
	lw	R3, PTR_POLY1305_R(3)

	/* store the used save registers. */
	sw	$s0, 0($sp)
	sw	$s1, 4($sp)
	sw	$s2, 8($sp)
	sw	$s3, 12($sp)
	sw	$s4, 16($sp)
	sw	$s5, 20($sp)

	/* load Hx and Carry */
	lw	CA, PTR_POLY1305_CA
	lw	H0, PTR_POLY1305_H(0)
	lw	H1, PTR_POLY1305_H(1)
	lw	H2, PTR_POLY1305_H(2)
	lw	H3, PTR_POLY1305_H(3)
	lw	H4, PTR_POLY1305_H(4)

	/* Sx = Rx + (Rx >> 2) */
	srl	S1, R1, 2
	srl	S2, R2, 2
	srl	S3, R3, 2
	addu	S1, R1
	addu	S2, R2
	addu	S3, R3

	addiu	SC, $zero, 1

.Lpoly1305_loop:
	lwl	O0, 0+MSB(src)
	lwl	O1, 4+MSB(src)
	lwl	O2, 8+MSB(src)
	lwl	O3,12+MSB(src)
	lwr	O0, 0+LSB(src)
	lwr	O1, 4+LSB(src)
	lwr	O2, 8+LSB(src)
	lwr	O3,12+LSB(src)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	wsbh	O0
	wsbh	O1
	wsbh	O2
	wsbh	O3
	rotr	O0, 16
	rotr	O1, 16
	rotr	O2, 16
	rotr	O3, 16
#endif

	/* h0 = (u32)(d0 = (u64)h0 + inp[0] + c 'Carry_previous cycle'); */
	addu	H0, CA
	sltu	CA, H0, CA
	addu	O0, H0
	sltu	H0, O0, H0
	addu	CA, H0

	/* h1 = (u32)(d1 = (u64)h1 + (d0 >> 32) + inp[4]); */
	addu	H1, CA
	sltu	CA, H1, CA
	addu	O1, H1
	sltu	H1, O1, H1
	addu	CA, H1

	/* h2 = (u32)(d2 = (u64)h2 + (d1 >> 32) + inp[8]); */
	addu	H2, CA
	sltu	CA, H2, CA
	addu	O2, H2
	sltu	H2, O2, H2
	addu	CA, H2

	/* h3 = (u32)(d3 = (u64)h3 + (d2 >> 32) + inp[12]); */
	addu	H3, CA
	sltu	CA, H3, CA
	addu	O3, H3
	sltu	H3, O3, H3
	addu	CA, H3

	/* h4 += (u32)(d3 >> 32) + padbit; */
	addu	H4, hibit
	addu	O4, H4, CA

	/* D0 */
	multu	O0, R0
	maddu	O1, S3
	maddu	O2, S2
	maddu	O3, S1
	mfhi	CA
	mflo	H0

	/* D1 */
	multu	O0, R1
	maddu	O1, R0
	maddu	O2, S3
	maddu	O3, S2
	maddu	O4, S1
	maddu	CA, SC
	mfhi	CA
	mflo	H1

	/* D2 */
	multu	O0, R2
	maddu	O1, R1
	maddu	O2, R0
	maddu	O3, S3
	maddu	O4, S2
	maddu	CA, SC
	mfhi	CA
	mflo	H2

	/* D4 */
	mul	H4, O4, R0

	/* D3 */
	multu	O0, R3
	maddu	O1, R2
	maddu	O2, R1
	maddu	O3, R0
	maddu	O4, S3
	maddu	CA, SC
	mfhi	CA
	mflo	H3

	addiu	src, POLY1305_BLOCK_SIZE

	/* h4 += (u32)(d3 >> 32); */
	addu	O4, H4, CA
	/* h4 &= 3 */
	andi	H4, O4, 3
	/* c = (h4 >> 2) + (h4 & ~3U); */
	srl	CA, O4, 2
	ins	O4, $zero, 0, 2

	addu	CA, O4

	/* able to do a 16 byte block. */
	bne	src, srclen, .Lpoly1305_loop

	/* restore the used save registers. */
	lw	$s0, 0($sp)
	lw	$s1, 4($sp)
	lw	$s2, 8($sp)
	lw	$s3, 12($sp)
	lw	$s4, 16($sp)
	lw	$s5, 20($sp)

	/* store Hx and Carry */
	sw	CA, PTR_POLY1305_CA
	sw	H0, PTR_POLY1305_H(0)
	sw	H1, PTR_POLY1305_H(1)
	sw	H2, PTR_POLY1305_H(2)
	sw	H3, PTR_POLY1305_H(3)
	sw	H4, PTR_POLY1305_H(4)

.Lpoly1305_blocks_mips_end:
	addiu	$sp, POLY1305_STACK_SIZE

	/* Jump Back */
	jr	$ra
.end poly1305_blocks_mips
.set at

/* Input arguments CTX=$a0, MAC=$a1, NONCE=$a2 */
#define MAC	$a1
#define NONCE	$a2

#define G0	$t5
#define G1	$t6
#define G2	$t7
#define G3	$t8
#define G4	$t9

.set	noat
.align	4
.globl	poly1305_emit_mips
.ent	poly1305_emit_mips
poly1305_emit_mips:
	/* load Hx and Carry */
	lw	CA, PTR_POLY1305_CA
	lw	H0, PTR_POLY1305_H(0)
	lw	H1, PTR_POLY1305_H(1)
	lw	H2, PTR_POLY1305_H(2)
	lw	H3, PTR_POLY1305_H(3)
	lw	H4, PTR_POLY1305_H(4)

	/* Add left over carry */
	addu	H0, CA
	sltu	CA, H0, CA
	addu	H1, CA
	sltu	CA, H1, CA
	addu	H2, CA
	sltu	CA, H2, CA
	addu	H3, CA
	sltu	CA, H3, CA
	addu	H4, CA

	/* compare to modulus by computing h + -p */
	addiu	G0, H0, 5
	sltu	CA, G0, H0
	addu	G1, H1, CA
	sltu	CA, G1, H1
	addu	G2, H2, CA
	sltu	CA, G2, H2
	addu	G3, H3, CA
	sltu	CA, G3, H3
	addu	G4, H4, CA

	srl	SC, G4, 2

	/* if there was carry into 131st bit, h3:h0 = g3:g0 */
	movn	H0, G0, SC
	movn	H1, G1, SC
	movn	H2, G2, SC
	movn	H3, G3, SC

	lwl	G0, 0+MSB(NONCE)
	lwl	G1, 4+MSB(NONCE)
	lwl	G2, 8+MSB(NONCE)
	lwl	G3,12+MSB(NONCE)
	lwr	G0, 0+LSB(NONCE)
	lwr	G1, 4+LSB(NONCE)
	lwr	G2, 8+LSB(NONCE)
	lwr	G3,12+LSB(NONCE)

	/* mac = (h + nonce) % (2^128) */
	addu	H0, G0
	sltu	CA, H0, G0

	/* H1 */
	addu	H1, CA
	sltu	CA, H1, CA
	addu	H1, G1
	sltu	G1, H1, G1
	addu	CA, G1

	/* H2 */
	addu	H2, CA
	sltu	CA, H2, CA
	addu	H2, G2
	sltu	G2, H2, G2
	addu	CA, G2

	/* H3 */
	addu	H3, CA
	addu	H3, G3

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	wsbh	H0
	wsbh	H1
	wsbh	H2
	wsbh	H3
	rotr	H0, 16
	rotr	H1, 16
	rotr	H2, 16
	rotr	H3, 16
#endif

	/* store MAC */
	swl	H0, 0+MSB(MAC)
	swl	H1, 4+MSB(MAC)
	swl	H2, 8+MSB(MAC)
	swl	H3,12+MSB(MAC)
	swr	H0, 0+LSB(MAC)
	swr	H1, 4+LSB(MAC)
	swr	H2, 8+LSB(MAC)
	swr	H3,12+LSB(MAC)

	jr	$ra
.end poly1305_emit_mips

#define PR0 $t0
#define PR1 $t1
#define PR2 $t2
#define PR3 $t3
#define PT0 $t4

/* Input arguments CTX=$a0, KEY=$a1 */

.align	4
.globl	poly1305_init_mips
.ent	poly1305_init_mips
poly1305_init_mips:
	lwl	PR0, 0+MSB($a1)
	lwl	PR1, 4+MSB($a1)
	lwl	PR2, 8+MSB($a1)
	lwl	PR3,12+MSB($a1)
	lwr	PR0, 0+LSB($a1)
	lwr	PR1, 4+LSB($a1)
	lwr	PR2, 8+LSB($a1)
	lwr	PR3,12+LSB($a1)

	/* store Hx and Carry */
	sw	$zero, PTR_POLY1305_CA
	sw	$zero, PTR_POLY1305_H(0)
	sw	$zero, PTR_POLY1305_H(1)
	sw	$zero, PTR_POLY1305_H(2)
	sw	$zero, PTR_POLY1305_H(3)
	sw	$zero, PTR_POLY1305_H(4)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	wsbh	PR0
	wsbh	PR1
	wsbh	PR2
	wsbh	PR3
	rotr	PR0, 16
	rotr	PR1, 16
	rotr	PR2, 16
	rotr	PR3, 16
#endif

	lui	PT0, 0x0FFF
	ori	PT0, 0xFFFC

	/* AND 0x0fffffff; */
	ext	PR0, PR0, 0, (32-4)

	/* AND 0x0ffffffc; */
	and	PR1, PT0
	and	PR2, PT0
	and	PR3, PT0

	/* store Rx */
	sw	PR0, PTR_POLY1305_R(0)
	sw	PR1, PTR_POLY1305_R(1)
	sw	PR2, PTR_POLY1305_R(2)
	sw	PR3, PTR_POLY1305_R(3)

	/* Jump Back  */
	jr	$ra
.end poly1305_init_mips
