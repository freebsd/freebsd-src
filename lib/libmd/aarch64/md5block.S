/*-
 * Copyright (c) 2024 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/elf_common.h>
#include <machine/asm.h>

# optimal instruction sequence for k = \key + \m
.macro	addkm	key, m
.if 0x100000000 - \key > 0x00ffffff
	movz	k, #\key & 0xffff
	movk	k, #\key >> 16, lsl #16
	add	k, k, \m
.elseif 0x100000000 - \key > 0x0000ffff
	sub	k, \m, #(0x100000000 - \key) & 0xfff000
	sub	k, k, #(0x100000000 - \key) & 0xfff
.else
	movz	k, #0x100000000 - \key
	sub	k, \m, k
.endif
.endm

.macro	round	a, b, c, d, f, key, m, s
	\f	f, \b, \c, \d
	addkm	\key, \m		// k[i] + m[g]
	add	\a, \a, k		// k[i] + m[g] + a
	add	\a, \a, f		// k[i] + m[g] + a + f
	ror	\a, \a, #32-\s
	add	\a, \a, \b
.endm

	/* f = b ? c : d */
.macro	f0	f, b, c, d
	eor	\f, \c, \d
	and	\f, \f, \b
	eor	\f, \f, \d
.endm

	/*
	 * special cased round 1 function
	 * f1 = d ? b : c = (d & b) + (~d & c)
	 */
.macro	round1	a, b, c, d, key, m, s
	bic	tmp, \c, \d		// ~d & c
	addkm	\key, \m		// k[i] + m[g]
	add	\a, \a, k		// k[i] + m[g] + a
	and	f, \b, \d		// d & b
	add	\a, \a, tmp		// k[i] + m[g] + a + (~d & c)
	add	\a, \a, f		// k[i] + m[g] + a + (~d & c) + (d & b)
	ror	\a, \a, #32-\s
	add	\a, \a, \b
.endm

	/* f = b ^ c ^ d */
.macro	f2	f, b, c, d
	eor	\f, \c, \d
	eor	\f, \f, \b
.endm

	/* f = c ^ (b | ~d) */
.macro	f3	f, b, c, d
	orn	\f, \b, \d
	eor	\f, \f, \c
.endm

	/* do 4 rounds */
.macro	rounds	f, m0, m1, m2, m3, s0, s1, s2, s3, k0, k1, k2, k3
	round	a, b, c, d, \f, \k0, \m0, \s0
	round	d, a, b, c, \f, \k1, \m1, \s1
	round	c, d, a, b, \f, \k2, \m2, \s2
	round	b, c, d, a, \f, \k3, \m3, \s3
.endm

	/* do 4 rounds with f0, f1, f2, f3 */
.macro	rounds0	m0, m1, m2, m3, k0, k1, k2, k3
	rounds	f0, \m0, \m1, \m2, \m3, 7, 12, 17, 22, \k0, \k1, \k2, \k3
.endm

.macro	rounds1	m0, m1, m2, m3, k0, k1, k2, k3
	round1	a, b, c, d, \k0, \m0,  5
	round1	d, a, b, c, \k1, \m1,  9
	round1	c, d, a, b, \k2, \m2, 14
	round1	b, c, d, a, \k3, \m3, 20
.endm

.macro	rounds2	m0, m1, m2, m3, k0, k1, k2, k3
	rounds	f2, \m0, \m1, \m2, \m3, 4, 11, 16, 23, \k0, \k1, \k2, \k3
.endm

.macro	rounds3	m0, m1, m2, m3, k0, k1, k2, k3
	rounds	f3, \m0, \m1, \m2, \m3, 6, 10, 15, 21, \k0, \k1, \k2, \k3
.endm

	/* md5block(MD5_CTX, buf, len) */
ENTRY(_libmd_md5block)
ctx	.req	x0
buf	.req	x1
len	.req	x2
end	.req	x2			// aliases len
a	.req	w3
b	.req	w4
c	.req	w5
d	.req	w6
f	.req	w7
tmp	.req	w8
k	.req	w9
m0	.req	w10
m1	.req	w11
m2	.req	w12
m3	.req	w13
m4	.req	w14
m5	.req	w15
m6	.req	w16
m7	.req	w17
					// x18 is the platform register
m8	.req	w19
m9	.req	w20
m10	.req	w21
m11	.req	w22
m12	.req	w23
m13	.req	w24
m14	.req	w25
m15	.req	w26

a_	.req	m0
b_	.req	m7
c_	.req	m14
d_	.req	m5

	stp	x19, x20, [sp, #-0x40]!
	stp	x21, x22, [sp, #0x10]
	stp	x23, x24, [sp, #0x20]
	stp	x25, x26, [sp, #0x30]

	bics	len, len, #63		// length in blocks
	add	end, buf, len		// end pointer

	beq	.Lend			// was len == 0 after BICS?

	ldp	a, b, [ctx, #0]
	ldp	c, d, [ctx, #8]

	/* first eight rounds interleaved with data loads */
.Lloop:	ldp	m0, m1, [buf, #0]
	round	a, b, c, d, f0, 0xd76aa478, m0,  7
	ldp	m2, m3, [buf, #8]
	round	d, a, b, c, f0, 0xe8c7b756, m1, 12
	ldp	m4, m5, [buf, #16]
	round	c, d, a, b, f0, 0x242070db, m2, 17
	ldp	m6, m7, [buf, #24]
	round	b, c, d, a, f0, 0xc1bdceee, m3, 22

	ldp	m8, m9, [buf, #32]
	round	a, b, c, d, f0, 0xf57c0faf, m4,  7
	ldp	m10, m11, [buf, #40]
	round	d, a, b, c, f0, 0x4787c62a, m5, 12
	ldp	m12, m13, [buf, #48]
	round	c, d, a, b, f0, 0xa8304613, m6, 17
	ldp	m14, m15, [buf, #56]
	round	b, c, d, a, f0, 0xfd469501, m7, 22

	/* remaining rounds use the roundsX macros */
	rounds0	 m8,  m9, m10, m11, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be
	rounds0	m12, m13, m14, m15, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821

	rounds1	 m1,  m6, m11,  m0, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa
	rounds1	 m5, m10, m15,  m4, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8
	rounds1	 m9, m14,  m3,  m8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed
	rounds1	m13,  m2,  m7, m12, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a

	rounds2	 m5,  m8, m11, m14, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c
	rounds2	 m1,  m4,  m7, m10, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70
	rounds2	m13,  m0,  m3,  m6, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05
	rounds2	 m9, m12, m15,  m2, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665

	rounds3	 m0,  m7, m14,  m5, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039
	rounds3	m12,  m3, m10,  m1, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1
	rounds3	 m8, m15,  m6, m13, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1
	rounds3	 m4, m11,  m2,  m9, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391

	ldp	a_, b_, [ctx, #0]
	ldp	c_, d_, [ctx, #8]
	add	a, a, a_
	add	b, b, b_
	add	c, c, c_
	add	d, d, d_
	stp	a, b, [ctx, #0]
	stp	c, d, [ctx, #8]

	add	buf, buf, #64
	cmp	buf, end
	bne	.Lloop

.Lend:	ldp	x25, x26, [sp, #0x30]
	ldp	x23, x24, [sp, #0x20]
	ldp	x21, x22, [sp, #0x10]
	ldp	x19, x20, [sp], #0x40

	ret
END(_libmd_md5block)

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)

	.section .note.GNU-stack,"",%progbits
