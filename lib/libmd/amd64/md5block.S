/*-
 * Copyright (c) 2024, 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <machine/asm.h>

/* apply the round keys to the four round functions */
.macro	allrounds	rfn0, rfn1, rfn2, rfn3
	\rfn0	 0, 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee
	\rfn0	 4, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501
	\rfn0	 8, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be
	\rfn0	12, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821

	\rfn1	16, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa
	\rfn1	20, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8
	\rfn1	24, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed
	\rfn1	28, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a

	\rfn2	32, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c
	\rfn2	36, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70
	\rfn2	40, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05
	\rfn2	44, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665

	\rfn3	48, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039
	\rfn3	52, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1
	\rfn3	56, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1
	\rfn3	60, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
.endm

	// md5block(MD5_CTX, buf, len)
ENTRY(_libmd_md5block_baseline)
.macro	round	a, b, c, d, f, k, m, s
	\f	%ebp, \b, \c, \d
	add	$\k, \a			// a + k[i]
	add	((\m)%16*4)(%rsi), \a	// a + k[i] + m[g]
	add	%ebp, \a		// a + k[i] + m[g] + f
	rol	$\s, \a
	add	\b, \a
.endm

	// f = b ? c : d
.macro	f0	f, b, c, d
	mov	\c, \f
	xor	\d, \f
	and	\b, \f
	xor	\d, \f
.endm

	// f = d ? b : c
.macro	f1	f, b, c, d
	mov	\c, \f
	xor	\b, \f
	and	\d, \f
	xor	\c, \f
.endm

	// f = b ^ c ^ d
.macro	f2	f, b, c, d
	mov	\c, \f
	xor	\d, \f
	xor	\b, \f
.endm

	// f = c ^ (b | ~d)
.macro	f3	f, b, c, d
	mov	$-1, \f
	xor	\d, \f
	or	\b, \f
	xor	\c, \f
.endm

	// do 4 rounds
.macro	rounds	f, p, q, s0, s1, s2, s3, k0, k1, k2, k3
	round	%eax, %ebx, %ecx, %edx, \f, \k0, \p*0+\q, \s0
	round	%edx, %eax, %ebx, %ecx, \f, \k1, \p*1+\q, \s1
	round	%ecx, %edx, %eax, %ebx, \f, \k2, \p*2+\q, \s2
	round	%ebx, %ecx, %edx, %eax, \f, \k3, \p*3+\q, \s3
.endm

	// do 4 rounds with f0, f1, f2, f3
.macro	rounds0	i, k0, k1, k2, k3
	rounds	f0, 1, \i, 7, 12, 17, 22, \k0, \k1, \k2, \k3
.endm

.macro	rounds1	i, k0, k1, k2, k3
	rounds	f1, 5, 5*\i+1, 5, 9, 14, 20, \k0, \k1, \k2, \k3
.endm

.macro	rounds2	i, k0, k1, k2, k3
	rounds	f2, 3, 3*\i+5, 4, 11, 16, 23, \k0, \k1, \k2, \k3
.endm

.macro	rounds3	i, k0, k1, k2, k3
	rounds	f3, 7, 7*\i, 6, 10, 15, 21, \k0, \k1, \k2, \k3
.endm

	push	%rbx
	push	%rbp
	push	%r12

	and	$~63, %rdx		// length in blocks
	lea	(%rsi, %rdx, 1), %r12	// end pointer

	mov	(%rdi), %eax		// a
	mov	4(%rdi), %ebx		// b
	mov	8(%rdi), %ecx		// c
	mov	12(%rdi), %edx		// d

	cmp	%rsi, %r12		// any data to process?
	je	.Lend

	.balign	16
.Lloop:	mov	%eax, %r8d
	mov	%ebx, %r9d
	mov	%ecx, %r10d
	mov	%edx, %r11d

	allrounds	rounds0, rounds1, rounds2, rounds3

	add	%r8d, %eax
	add	%r9d, %ebx
	add	%r10d, %ecx
	add	%r11d, %edx

	add	$64, %rsi
	cmp	%rsi, %r12
	jne	.Lloop

	mov	%eax, (%rdi)
	mov	%ebx, 4(%rdi)
	mov	%ecx, 8(%rdi)
	mov	%edx, 12(%rdi)

.Lend:	pop	%r12
	pop	%rbp
	pop	%rbx
	ret
END(_libmd_md5block_baseline)

	/*
	 * An implementation leveraging the ANDN instruction
	 * from BMI1 to shorten some dependency chains.
	 */
ENTRY(_libmd_md5block_bmi1)
	// special-cased round 1
	// f1 = d ? b : c = (d & b) + (~d & c)
.macro	round1	a, b, c, d, k, m, s
	andn	\c, \d, %edi		// ~d & c
	add	$\k, \a			// a + k[i]
	mov	\d, %ebp
	add	((\m)%16*4)(%rsi), \a	// a + k[i] + m[g]
	and	\b, %ebp		// d & b
	add	%edi, \a		// a + k[i] + m[g] + (~d & c)
	add	%ebp, \a		// a + k[i] + m[g] + (~d & c) + (d & b)
	rol	$\s, \a
	add	\b, \a
.endm

	// special-cased round 3
	// f3 = c ^ (b | ~d) = ~(c ^ ~b & d) = -1 - (c ^ ~b & d)
.macro	round3	a, b, c, d, k, m, s
	andn	\d, \b, %ebp
	add	$\k - 1, \a		// a + k[i] - 1
	add	((\m)%16*4)(%rsi), \a	// a + k[i] + m[g]
	xor	\c, %ebp
	sub	%ebp, \a		// a + k[i] + m[g] + f
	rol	$\s, \a
	add	\b, \a
.endm

	.purgem	rounds1
.macro	rounds1	i, k0, k1, k2, k3
	round1	%eax, %ebx, %ecx, %edx, \k0, 5*\i+ 1,  5
	round1	%edx, %eax, %ebx, %ecx, \k1, 5*\i+ 6,  9
	round1	%ecx, %edx, %eax, %ebx, \k2, 5*\i+11, 14
	round1	%ebx, %ecx, %edx, %eax, \k3, 5*\i+16, 20
.endm

	.purgem	rounds3
.macro	rounds3	i, k0, k1, k2, k3
	round3	%eax, %ebx, %ecx, %edx, \k0, 7*\i+ 0,  6
	round3	%edx, %eax, %ebx, %ecx, \k1, 7*\i+ 7, 10
	round3	%ecx, %edx, %eax, %ebx, \k2, 7*\i+14, 15
	round3	%ebx, %ecx, %edx, %eax, \k3, 7*\i+21, 21
.endm

	push	%rbx
	push	%rbp
	push	%r12

	and	$~63, %rdx		// length in blocks
	lea	(%rsi, %rdx, 1), %r12	// end pointer

	mov	(%rdi), %eax		// a
	mov	4(%rdi), %ebx		// b
	mov	8(%rdi), %ecx		// c
	mov	12(%rdi), %edx		// d

	cmp	%rsi, %r12		// any data to process?
	je	0f

	push	%rdi

	.balign	16
1:	mov	%eax, %r8d
	mov	%ebx, %r9d
	mov	%ecx, %r10d
	mov	%edx, %r11d

	allrounds	rounds0, rounds1, rounds2, rounds3

	add	%r8d, %eax
	add	%r9d, %ebx
	add	%r10d, %ecx
	add	%r11d, %edx

	add	$64, %rsi
	cmp	%rsi, %r12
	jne	1b

	pop	%rdi
	mov	%eax, (%rdi)
	mov	%ebx, 4(%rdi)
	mov	%ecx, 8(%rdi)
	mov	%edx, 12(%rdi)

0:	pop	%r12
	pop	%rbp
	pop	%rbx
	ret
END(_libmd_md5block_bmi1)

#ifndef _KERNEL
	/*
	 * An implementation leveraging AVX-512 for its VPTERNLOGD
	 * instruction.  We're using only XMM registers here,
	 * avoiding costly thermal licensing.
	 */
ENTRY(_libmd_md5block_avx512)
.macro	vround		a, b, c, d, f, i, m, mi, s
	vmovdqa		\b, %xmm4
	vpternlogd	$\f, \d, \c, %xmm4
	vpaddd		4*(\i)(%rax){1to4}, \m, %xmm5 // m[g] + k[i]
.if	\mi != 0
	vpshufd		$0x55 * \mi, %xmm5, %xmm5	// broadcast to each dword
.endif
	vpaddd		%xmm5, \a, \a		// a + k[i] + m[g]
	vpaddd		%xmm4, \a, \a		// a + k[i] + m[g] + f
	vprold		$\s, \a, \a
	vpaddd		\b, \a, \a
.endm

.macro	vrounds		f, i, m0, i0, m1, i1, m2, i2, m3, i3, s0, s1, s2, s3
	vround		%xmm0, %xmm1, %xmm2, %xmm3, \f, \i+0, \m0, \i0, \s0
	vround		%xmm3, %xmm0, %xmm1, %xmm2, \f, \i+1, \m1, \i1, \s1
	vround		%xmm2, %xmm3, %xmm0, %xmm1, \f, \i+2, \m2, \i2, \s2
	vround		%xmm1, %xmm2, %xmm3, %xmm0, \f, \i+3, \m3, \i3, \s3
.endm

/*
 * d c b f0 f1 f2 f3
 * 0 0 0  0  0  0  1
 * 1 0 0  1  0  1  0
 * 0 1 0  0  1  1  0
 * 1 1 0  1  0  0  1
 * 0 0 1  0  0  1  1
 * 1 0 1  0  1  0  1
 * 0 1 1  1  1  0  0
 * 1 1 1  1  1  1  0
 */

.macro	vrounds0	i, m
	vrounds		0xca, \i, \m, 0, \m, 1, \m, 2, \m, 3, 7, 12, 17, 22
.endm

.macro	vrounds1	i, m0, i0, m1, i1, m2, i2, m3, i3
	vrounds		0xe4, \i, \m0, \i0, \m1, \i1, \m2, \i2, \m3, \i3, 5, 9, 14, 20
.endm

.macro	vrounds2	i, m0, i0, m1, i1, m2, i2, m3, i3
	vrounds		0x96, \i, \m0, \i0, \m1, \i1, \m2, \i2, \m3, \i3, 4, 11, 16, 23
.endm

.macro	vrounds3	i, m0, i0, m1, i1, m2, i2, m3, i3
	vrounds		0x39, \i, \m0, \i0, \m1, \i1, \m2, \i2, \m3, \i3, 6, 10, 15, 21
.endm

	and		$~63, %rdx		// length in blocks
	add		%rsi, %rdx		// end pointer

	vmovd		(%rdi), %xmm0		// a
	vmovd		4(%rdi), %xmm1		// b
	vmovd		8(%rdi), %xmm2		// c
	vmovd		12(%rdi), %xmm3		// d

	lea		keys(%rip), %rax

	cmp		%rsi, %rdx		// any data to process?
	je		0f

	.balign		16
1:	vmovdqu		0*4(%rsi), %xmm8	// message words
	vmovdqu		4*4(%rsi), %xmm9
	vmovdqu		8*4(%rsi), %xmm10
	vmovdqu		12*4(%rsi), %xmm11

	vmovdqa		%xmm0, %xmm12		// stash old state variables
	vmovdqa		%xmm1, %xmm13
	vmovdqa		%xmm2, %xmm14
	vmovdqa		%xmm3, %xmm15

	vrounds0	 0, %xmm8
	vrounds0	 4, %xmm9
	vrounds0	 8, %xmm10
	vrounds0	12, %xmm11

	vrounds1	16,  %xmm8, 1,  %xmm9, 2, %xmm10, 3,  %xmm8, 0
	vrounds1	20,  %xmm9, 1, %xmm10, 2, %xmm11, 3,  %xmm9, 0
	vrounds1	24, %xmm10, 1, %xmm11, 2,  %xmm8, 3, %xmm10, 0
	vrounds1	28, %xmm11, 1,  %xmm8, 2,  %xmm9, 3, %xmm11, 0

	vrounds2	32,  %xmm9, 1, %xmm10, 0, %xmm10, 3, %xmm11, 2
	vrounds2	36,  %xmm8, 1,  %xmm9, 0,  %xmm9, 3, %xmm10, 2
	vrounds2	40, %xmm11, 1,  %xmm8, 0,  %xmm8, 3,  %xmm9, 2
	vrounds2	44  %xmm10, 1, %xmm11, 0, %xmm11, 3,  %xmm8, 2

	vrounds3	48,  %xmm8, 0,  %xmm9, 3, %xmm11, 2,  %xmm9, 1
	vrounds3	52, %xmm11, 0,  %xmm8, 3, %xmm10, 2,  %xmm8, 1
	vrounds3	56, %xmm10, 0, %xmm11, 3,  %xmm9, 2, %xmm11, 1
	vrounds3	60,  %xmm9, 0, %xmm10, 3,  %xmm8, 2, %xmm10, 1

	vpaddd		%xmm12, %xmm0, %xmm0
	vpaddd		%xmm13, %xmm1, %xmm1
	vpaddd		%xmm14, %xmm2, %xmm2
	vpaddd		%xmm15, %xmm3, %xmm3

	add		$64, %rsi
	cmp		%rsi, %rdx
	jne		1b

	vmovd		%xmm0, (%rdi)
	vmovd		%xmm1, 4(%rdi)
	vmovd		%xmm2, 8(%rdi)
	vmovd		%xmm3, 12(%rdi)

0:	ret
END(_libmd_md5block_avx512)

	// round keys, for use in md5block_avx512
	.section	.rodata
	.balign		16

.macro	putkeys		i, a, b, c, d
	.4byte		\a, \b, \c, \d
.endm

keys:	allrounds	putkeys, putkeys, putkeys, putkeys
	.size		keys, .-keys
#endif /* !defined(_KERNEL) */

	.section .note.GNU-stack,"",%progbits
