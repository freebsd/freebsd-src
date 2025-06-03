/*-
 * Copyright (c) 2013 The Go Authors. All rights reserved.
 * Copyright (c) 2024 Robert Clausecker <fuz@freebsd.org>
 *
 * Adapted from Go's crypto/sha1/sha1block_amd64.s.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *   * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

/*
 * SHA-1 block routine. See sha1c.c for C equivalent.
 *
 * There are 80 rounds of 4 types:
 *   - rounds 0-15 are type 1 and load data (round1 macro).
 *   - rounds 16-19 are type 1 and do not load data (round1x macro).
 *   - rounds 20-39 are type 2 and do not load data (round2 macro).
 *   - rounds 40-59 are type 3 and do not load data (round3 macro).
 *   - rounds 60-79 are type 4 and do not load data (round4 macro).
 *
 * Each round loads or shuffles the data, then computes a per-round
 * function of b, c, d, and then mixes the result into and rotates the
 * five registers a, b, c, d, e holding the intermediate results.
 *
 * The register rotation is implemented by rotating the arguments to
 * the round macros instead of by explicit move instructions.
 */
.macro	load		index
	mov		(\index)*4(%rsi), %r10d
	bswap		%r10d
	mov		%r10d, (\index)*4(%rsp)
.endm

.macro	shuffle		index
	mov		((\index   )&0xf)*4(%rsp), %r10d
	xor		((\index- 3)&0xf)*4(%rsp), %r10d
	xor		((\index- 8)&0xf)*4(%rsp), %r10d
	xor		((\index-14)&0xf)*4(%rsp), %r10d
	rol		$1, %r10d
	mov		%r10d, ((\index)&0xf)*4(%rsp)
.endm

.macro	func1		a, b, c, d, e
	mov		\d, %r9d
	xor		\c, %r9d
	and		\b, %r9d
	xor		\d, %r9d
.endm

.macro	func2		a, b, c, d, e
	mov		\b, %r9d
	xor		\c, %r9d
	xor		\d, %r9d
.endm

.macro	func3		a, b, c, d, e
	mov		\b, %r8d
	or		\c, %r8d
	and		\d, %r8d
	mov		\b, %r9d
	and		\c, %r9d
	or		%r8d, %r9d
.endm

.macro	func4		a, b, c, d, e
	func2		\a, \b, \c, \d, \e
.endm

.macro	mix		a, b, c, d, e, const
	rol		$30, \b
	add		%r9d, \e
	mov		\a, %r8d
	rol		$5, %r8d
	lea		\const(\e, %r10d, 1), \e
	add		%r8d, \e
.endm

.macro	round1		a, b, c, d, e, index
	load		\index
	func1		\a, \b, \c, \d, \e
	mix		\a, \b, \c, \d, \e, 0x5a827999
.endm

.macro	round1x		a, b, c, d, e, index
	shuffle		\index
	func1		\a, \b, \c, \d, \e
	mix		\a, \b, \c, \d, \e, 0x5a827999
.endm

.macro	round2		a, b, c, d, e, index
	shuffle		\index
	func2		\a, \b, \c, \d, \e
	mix		\a, \b, \c, \d, \e, 0x6ed9eba1
.endm

.macro	round3		a, b, c, d, e, index
	shuffle		\index
	func3		\a, \b, \c, \d, \e
	mix		\a, \b, \c, \d, \e, 0x8f1bbcdc
.endm

.macro	round4		a, b, c, d, e, index
	shuffle		\index
	func4		\a, \b, \c, \d, \e
	mix		\a, \b, \c, \d, \e, 0xca62c1d6
.endm

	// sha1block(SHA1_CTX, buf, len)
ENTRY(_libmd_sha1block_scalar)
	push		%rbp
	push		%rbx
	push		%r12
	push		%r13
	push		%r14
	push		%r15
	push		%rdi			// rdi: SHA1_CTX
	sub		$64+8, %rsp		// 64 bytes for round keys
						// plus alignment

	mov		%rdi, %rbp
						// rsi: buf
	and		$~63, %rdx		// rdx: length in blocks
	lea		(%rsi, %rdx, 1), %rdi	// rdi: end pointer
	mov		(%rbp),	%eax		// c->h0
	mov		4(%rbp), %ebx		// c->h1
	mov		8(%rbp), %ecx		// c->h2
	mov		12(%rbp), %edx		// c->h3
	mov		16(%rbp), %ebp		// c->h4

	cmp		%rsi, %rdi		// any data to process?
	je		.Lend

.Lloop:	mov		%eax, %r11d
	mov		%ebx, %r12d
	mov		%ecx, %r13d
	mov		%edx, %r14d
	mov		%ebp, %r15d

	round1		%eax, %ebx, %ecx, %edx, %ebp,  0
	round1		%ebp, %eax, %ebx, %ecx, %edx,  1
	round1		%edx, %ebp, %eax, %ebx, %ecx,  2
	round1		%ecx, %edx, %ebp, %eax, %ebx,  3
	round1		%ebx, %ecx, %edx, %ebp, %eax,  4

	round1		%eax, %ebx, %ecx, %edx, %ebp,  5
	round1		%ebp, %eax, %ebx, %ecx, %edx,  6
	round1		%edx, %ebp, %eax, %ebx, %ecx,  7
	round1		%ecx, %edx, %ebp, %eax, %ebx,  8
	round1		%ebx, %ecx, %edx, %ebp, %eax,  9

	round1		%eax, %ebx, %ecx, %edx, %ebp, 10
	round1		%ebp, %eax, %ebx, %ecx, %edx, 11
	round1		%edx, %ebp, %eax, %ebx, %ecx, 12
	round1		%ecx, %edx, %ebp, %eax, %ebx, 13
	round1		%ebx, %ecx, %edx, %ebp, %eax, 14

	round1		%eax, %ebx, %ecx, %edx, %ebp, 15
	round1x		%ebp, %eax, %ebx, %ecx, %edx, 16
	round1x		%edx, %ebp, %eax, %ebx, %ecx, 17
	round1x		%ecx, %edx, %ebp, %eax, %ebx, 18
	round1x		%ebx, %ecx, %edx, %ebp, %eax, 19

	round2		%eax, %ebx, %ecx, %edx, %ebp, 20
	round2		%ebp, %eax, %ebx, %ecx, %edx, 21
	round2		%edx, %ebp, %eax, %ebx, %ecx, 22
	round2		%ecx, %edx, %ebp, %eax, %ebx, 23
	round2		%ebx, %ecx, %edx, %ebp, %eax, 24

	round2		%eax, %ebx, %ecx, %edx, %ebp, 25
	round2		%ebp, %eax, %ebx, %ecx, %edx, 26
	round2		%edx, %ebp, %eax, %ebx, %ecx, 27
	round2		%ecx, %edx, %ebp, %eax, %ebx, 28
	round2		%ebx, %ecx, %edx, %ebp, %eax, 29

	round2		%eax, %ebx, %ecx, %edx, %ebp, 30
	round2		%ebp, %eax, %ebx, %ecx, %edx, 31
	round2		%edx, %ebp, %eax, %ebx, %ecx, 32
	round2		%ecx, %edx, %ebp, %eax, %ebx, 33
	round2		%ebx, %ecx, %edx, %ebp, %eax, 34

	round2		%eax, %ebx, %ecx, %edx, %ebp, 35
	round2		%ebp, %eax, %ebx, %ecx, %edx, 36
	round2		%edx, %ebp, %eax, %ebx, %ecx, 37
	round2		%ecx, %edx, %ebp, %eax, %ebx, 38
	round2		%ebx, %ecx, %edx, %ebp, %eax, 39

	round3		%eax, %ebx, %ecx, %edx, %ebp, 40
	round3		%ebp, %eax, %ebx, %ecx, %edx, 41
	round3		%edx, %ebp, %eax, %ebx, %ecx, 42
	round3		%ecx, %edx, %ebp, %eax, %ebx, 43
	round3		%ebx, %ecx, %edx, %ebp, %eax, 44

	round3		%eax, %ebx, %ecx, %edx, %ebp, 45
	round3		%ebp, %eax, %ebx, %ecx, %edx, 46
	round3		%edx, %ebp, %eax, %ebx, %ecx, 47
	round3		%ecx, %edx, %ebp, %eax, %ebx, 48
	round3		%ebx, %ecx, %edx, %ebp, %eax, 49

	round3		%eax, %ebx, %ecx, %edx, %ebp, 50
	round3		%ebp, %eax, %ebx, %ecx, %edx, 51
	round3		%edx, %ebp, %eax, %ebx, %ecx, 52
	round3		%ecx, %edx, %ebp, %eax, %ebx, 53
	round3		%ebx, %ecx, %edx, %ebp, %eax, 54

	round3		%eax, %ebx, %ecx, %edx, %ebp, 55
	round3		%ebp, %eax, %ebx, %ecx, %edx, 56
	round3		%edx, %ebp, %eax, %ebx, %ecx, 57
	round3		%ecx, %edx, %ebp, %eax, %ebx, 58
	round3		%ebx, %ecx, %edx, %ebp, %eax, 59

	round4		%eax, %ebx, %ecx, %edx, %ebp, 60
	round4		%ebp, %eax, %ebx, %ecx, %edx, 61
	round4		%edx, %ebp, %eax, %ebx, %ecx, 62
	round4		%ecx, %edx, %ebp, %eax, %ebx, 63
	round4		%ebx, %ecx, %edx, %ebp, %eax, 64

	round4		%eax, %ebx, %ecx, %edx, %ebp, 65
	round4		%ebp, %eax, %ebx, %ecx, %edx, 66
	round4		%edx, %ebp, %eax, %ebx, %ecx, 67
	round4		%ecx, %edx, %ebp, %eax, %ebx, 68
	round4		%ebx, %ecx, %edx, %ebp, %eax, 69

	round4		%eax, %ebx, %ecx, %edx, %ebp, 70
	round4		%ebp, %eax, %ebx, %ecx, %edx, 71
	round4		%edx, %ebp, %eax, %ebx, %ecx, 72
	round4		%ecx, %edx, %ebp, %eax, %ebx, 73
	round4		%ebx, %ecx, %edx, %ebp, %eax, 74

	round4		%eax, %ebx, %ecx, %edx, %ebp, 75
	round4		%ebp, %eax, %ebx, %ecx, %edx, 76
	round4		%edx, %ebp, %eax, %ebx, %ecx, 77
	round4		%ecx, %edx, %ebp, %eax, %ebx, 78
	round4		%ebx, %ecx, %edx, %ebp, %eax, 79

	add		%r11d, %eax
	add		%r12d, %ebx
	add		%r13d, %ecx
	add		%r14d, %edx
	add		%r15d, %ebp

	add		$64, %rsi
	cmp		%rdi, %rsi
	jb		.Lloop

.Lend:	add		$64+8, %rsp
	pop		%rdi			// SHA1_CTX
	mov		%eax, (%rdi)
	mov		%ebx, 4(%rdi)
	mov		%ecx, 8(%rdi)
	mov		%edx, 12(%rdi)
	mov		%ebp, 16(%rdi)

	pop		%r15
	pop		%r14
	pop		%r13
	pop		%r12
	pop		%rbx
	pop		%rbp
	ret
END(_libmd_sha1block_scalar)

/*
 * This is the implementation using AVX2, BMI1 and BMI2. It is based on:
 * "SHA-1 implementation with Intel(R) AVX2 instruction set extensions"
 * From http://software.intel.com/en-us/articles
 * (look for improving-the-performance-of-the-secure-hash-algorithm-1)
 * This implementation is 2x unrolled, and interleaves vector instructions,
 * used to precompute W, with scalar computation of current round
 * for optimal scheduling.
 */

	/* trivial helper macros */
.macro	update_hash	a, tb, c, d, e
	add		(%r9), \a
	mov		\a, (%r9)
	add		4(%r9), \tb
	mov		\tb, 4(%r9)
	add		8(%r9), \c
	mov		\c, 8(%r9)
	add		12(%r9), \d
	mov		\d, 12(%r9)
	add		16(%r9), \e
	mov		\e, 16(%r9)
.endm

	/* help macros for recalc, which does precomputations */
.macro	precalc0	offset
	vmovdqu		\offset(%r10), %xmm0
.endm

.macro	precalc1	offset
	vinserti128	$1, \offset(%r13), %ymm0, %ymm0
.endm

.macro	precalc2	yreg
	vpshufb		%ymm10, %ymm0, \yreg
.endm

.macro	precalc4	yreg, k_offset
	vpaddd		\k_offset(%r8), \yreg, %ymm0
.endm

.macro	precalc7	offset
	vmovdqu		%ymm0, (\offset)*2(%r14)
.endm

/*
 * Message scheduling pre-compute for rounds 0-15
 * r13      is a pointer to the even 64-byte block
 * r10      is a pointer to the odd 64-byte block
 * r14      is a pointer to the temp buffer
 * xmm0     is used as a temp register
 * yreg     is clobbered as part of the computation
 * offset   chooses a 16 byte chunk within a block
 * r8       is a pointer to the constants block
 * k_offset chooses K constants relevant to this round
 * xmm10    holds the swap mask
 */
.macro	precalc00_15	offset, yreg
	precalc0	\offset
	precalc1	\offset
	precalc2	\yreg
	precalc4	\yreg, 0
	precalc7	\offset
.endm

	/* helper macros for precalc16_31 */
.macro	precalc16	reg_sub16, reg_sub12, reg_sub4, reg
	vpalignr	$8, \reg_sub16, \reg_sub12, \reg	// w[i - 14]
	vpsrldq		$4, \reg_sub4, %ymm0			// w[i -  3]
.endm

.macro	precalc17	reg_sub16, reg_sub8, reg
	vpxor		\reg_sub8, \reg, \reg
	vpxor		\reg_sub16, %ymm0, %ymm0
.endm

.macro	precalc18	reg
	vpxor		%ymm0, \reg, \reg
	vpslldq		$12, \reg, %ymm9
.endm

.macro	precalc19	reg
	vpslld		$1, \reg, %ymm0
	vpsrld		$31, \reg, \reg
	.endm

.macro	precalc20	reg
	vpor		\reg, %ymm0, %ymm0
	vpslld		$2, %ymm9, \reg
.endm

.macro	precalc21	reg
	vpsrld		$30, %ymm9, %ymm9
	vpxor		\reg, %ymm0, %ymm0
.endm

.macro	precalc23	reg, k_offset, offset
	vpxor		%ymm9, %ymm0, \reg
	vpaddd		\k_offset(%r8), \reg, %ymm0
	vmovdqu		%ymm0, (\offset)(%r14)
.endm

/*
 * Message scheduling pre-compute for rounds 16-31
 * calculating last 32 w[i] values in 8 XMM registers
 * pre-calculate K+w[i] values and store to mem
 * for later load by ALU add instruction.
 * "brute force" vectorization for rounds 16-31 only
 * due to w[i]->w[i-3] dependency.
 + clobbers 5 input ymm registers REG_SUB*
 * uses xmm0 and xmm9 as temp registers
 * As always, r8 is a pointer to constants block
 * and r14 is a pointer to temp buffer
 */
.macro	precalc16_31	reg, reg_sub4, reg_sub8, reg_sub12, reg_sub16, k_offset, offset
	precalc16	\reg_sub16, \reg_sub12, \reg_sub4, \reg
	precalc17	\reg_sub16, \reg_sub8, \reg
	precalc18	\reg
	precalc19	\reg
	precalc20	\reg
	precalc21	\reg
	precalc23	\reg, \k_offset, \offset
.endm

	/* helper macros for precalc_32_79 */
.macro	precalc32	reg_sub8, reg_sub4
	vpalignr	$8, \reg_sub8, \reg_sub4, %ymm0
.endm

.macro	precalc33	reg_sub28, reg
	vpxor		\reg_sub28, \reg, \reg
.endm

.macro	precalc34	reg_sub16
	vpxor		\reg_sub16, %ymm0, %ymm0
.endm

.macro	precalc35	reg
	vpxor		%ymm0, \reg, \reg
.endm

.macro	precalc36	reg
	vpslld		$2, \reg, %ymm0
.endm

.macro	precalc37	reg
	vpsrld		$30, \reg, \reg
	vpor		\reg, %ymm0, \reg
.endm

.macro	precalc39	reg, k_offset, offset
	vpaddd		\k_offset(%r8), \reg, %ymm0
	vmovdqu		%ymm0, \offset(%r14)
.endm

.macro	precalc32_79	reg, reg_sub4, reg_sub8, reg_sub16, reg_sub28, k_offset, offset
	precalc32	\reg_sub8, \reg_sub4
	precalc33	\reg_sub28, \reg
	precalc34	\reg_sub16
	precalc35	\reg
	precalc36	\reg
	precalc37	\reg
	precalc39	\reg, \k_offset, \offset
.endm

.macro	precalc
	precalc00_15	0x00, %ymm15
	precalc00_15	0x10, %ymm14
	precalc00_15	0x20, %ymm13
	precalc00_15	0x30, %ymm12
	precalc16_31	%ymm8,  %ymm12, %ymm13, %ymm14, %ymm15, 0x00, 0x080
	precalc16_31	%ymm7,  %ymm8,  %ymm12, %ymm13, %ymm14, 0x20, 0x0a0
	precalc16_31	%ymm5,  %ymm7,  %ymm8,  %ymm12, %ymm13, 0x20, 0x0c0
	precalc16_31	%ymm3,  %ymm5,  %ymm7,  %ymm8,  %ymm12, 0x20, 0x0e0
	precalc32_79	%ymm15, %ymm3,  %ymm5,  %ymm8,  %ymm14, 0x20, 0x100
	precalc32_79	%ymm14, %ymm15, %ymm3,  %ymm7,  %ymm13, 0x20, 0x120
	precalc32_79	%ymm13, %ymm14, %ymm15, %ymm5,  %ymm12, 0x40, 0x140
	precalc32_79	%ymm12, %ymm13, %ymm14, %ymm3,  %ymm8,  0x40, 0x160
	precalc32_79	%ymm8,  %ymm12, %ymm13, %ymm15, %ymm7,  0x40, 0x180
	precalc32_79	%ymm7,  %ymm8,  %ymm12, %ymm14, %ymm5,  0x40, 0x1a0
	precalc32_79	%ymm5,  %ymm7,  %ymm8,  %ymm13, %ymm3,  0x40, 0x1c0
	precalc32_79	%ymm3,  %ymm5,  %ymm7,  %ymm12, %ymm15, 0x60, 0x1e0
	precalc32_79	%ymm15, %ymm3,  %ymm5,  %ymm8,  %ymm14, 0x60, 0x200
	precalc32_79	%ymm14, %ymm15, %ymm3,  %ymm7,  %ymm13, 0x60, 0x220
	precalc32_79	%ymm13, %ymm14, %ymm15, %ymm5,  %ymm12, 0x60, 0x240
	precalc32_79	%ymm12, %ymm13, %ymm14, %ymm3,  %ymm8,  0x60, 0x260
.endm

/*
 * Macros calculating individual rounds have general form
 * calc_round_pre + precalc_round + calc_round_post
 * calc_round_{pre,post} macros follow
 */
.macro	calc_f1_pre	offset, reg_a, reg_b, reg_c, reg_e
	add		\offset(%r15), \reg_e
	andn		\reg_c, \reg_a, %ebp
	add		\reg_b, \reg_e			// add F from the previous round
	rorx		$0x1b, \reg_a, %r12d
	rorx		$2, \reg_a, \reg_b		// for the next round
.endm

/*
 * Calculate F for the next round
 */
.macro	calc_f1_post	reg_a, reg_b, reg_e
	and		\reg_b, \reg_a			// b & c
	xor		%ebp, \reg_a			// F1 = (b&c) ^ (~b&d)
	add		%r12d, \reg_e
.endm

/*
 * Registers are cyclically rotated:
 * edx -> eax -> edi -> esi -> ebx -> ecx
 */
.macro	calc0
	mov		%esi, %ebx			// precalculate first round
	rorx		$2, %esi, %esi
	andn		%eax, %ebx, %ebp
	and		%edi, %ebx
	xor		%ebp, %ebx
	calc_f1_pre	0x0, %ecx, %ebx, %edi, %edx
	precalc0	0x80
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc1
	calc_f1_pre	0x4, %edx, %ecx, %esi, %eax
	precalc1	0x80
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc2
	calc_f1_pre	0x8, %eax, %edx, %ebx, %edi
	precalc2	%ymm15
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc3
	calc_f1_pre	0xc, %edi, %eax, %ecx, %esi
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc4
	calc_f1_pre	0x20, %esi, %edi, %edx, %ebx
	precalc4	%ymm15, 0x0
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc5
	calc_f1_pre	0x24, %ebx, %esi, %eax, %ecx
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc6
	calc_f1_pre	0x28, %ecx, %ebx, %edi, %edx
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc7
	calc_f1_pre	0x2c, %edx, %ecx, %esi, %eax
	precalc7	0x0
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc8
	calc_f1_pre	0x40, %eax, %edx, %ebx, %edi
	precalc0	0x90
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc9
	calc_f1_pre	0x44, %edi, %eax, %ecx, %esi
	precalc1	0x90
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc10
	calc_f1_pre	0x48, %esi, %edi, %edx, %ebx
	precalc2	%ymm14
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc11
	calc_f1_pre	0x4c, %ebx, %esi, %eax, %ecx
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc12
	calc_f1_pre	0x60, %ecx, %ebx, %edi, %edx
	precalc4	%ymm14, 0
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc13
	calc_f1_pre	0x64, %edx, %ecx, %esi, %eax
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc14
	calc_f1_pre	0x68, %eax, %edx, %ebx, %edi
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc15
	calc_f1_pre	0x6c, %edi, %eax, %ecx, %esi
	precalc7	0x10
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc16
	calc_f1_pre	0x80, %esi, %edi, %edx, %ebx
	precalc0	0xa0
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc17
	calc_f1_pre	0x84, %ebx, %esi, %eax, %ecx
	precalc1	0xa0
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc18
	calc_f1_pre	0x88, %ecx, %ebx, %edi, %edx
	precalc2	%ymm13
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc_f2_pre	offset, reg_a, reg_b, reg_e
	add		\offset(%r15), \reg_e
	add		\reg_b, \reg_e			// add F from the previous round
	rorx		$0x1b, \reg_a, %r12d
	rorx		$2, \reg_a, \reg_b		// for next round
.endm

.macro	calc_f2_post	reg_a, reg_b, reg_c, reg_e
	xor		\reg_b, \reg_a
	add		%r12d, \reg_e
	xor		\reg_c, \reg_a
.endm

.macro	calc19
	calc_f2_pre	0x8c, %edx, %ecx, %eax
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc20
	calc_f2_pre	0xa0, %eax, %edx, %edi
	precalc4	%ymm13, 0x0
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc21
	calc_f2_pre	0xa4, %edi, %eax, %esi
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc22
	calc_f2_pre	0xa8, %esi, %edi, %ebx
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc23
	calc_f2_pre	0xac, %ebx, %esi, %ecx
	precalc7	0x20
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc24
	calc_f2_pre	0xc0, %ecx, %ebx, %edx
	precalc0	0xb0
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc25
	calc_f2_pre	0xc4, %edx, %ecx, %eax
	precalc1	0xb0
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc26
	calc_f2_pre	0xc8, %eax, %edx, %edi
	precalc2	%ymm12
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc27
	calc_f2_pre	0xcc, %edi, %eax, %esi
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc28
	calc_f2_pre	0xe0, %esi, %edi, %ebx
	precalc4	%ymm12, 0x0
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc29
	calc_f2_pre	0xe4, %ebx, %esi, %ecx
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc30
	calc_f2_pre	0xe8, %ecx, %ebx, %edx
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc31
	calc_f2_pre	0xec, %edx, %ecx, %eax
	precalc7	0x30
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc32
	calc_f2_pre	0x100, %eax, %edx, %edi
	precalc16	%ymm15, %ymm14, %ymm12, %ymm8
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc33
	calc_f2_pre	0x104, %edi, %eax, %esi
	precalc17	%ymm15, %ymm13, %ymm8
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc34
	calc_f2_pre	0x108, %esi, %edi, %ebx
	precalc18	%ymm8
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc35
	calc_f2_pre	0x10c, %ebx, %esi, %ecx
	precalc19	%ymm8
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc36
	calc_f2_pre	0x120, %ecx, %ebx, %edx
	precalc20	%ymm8
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc37
	calc_f2_pre	0x124, %edx, %ecx, %eax
	precalc21	%ymm8
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc38
	calc_f2_pre	0x128, %eax, %edx, %edi
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc_f3_pre	offset, reg_e
	add		\offset(%r15), \reg_e
.endm

.macro	calc_f3_post	reg_a, reg_b, reg_c, reg_e, reg_tb
	add		\reg_tb, \reg_e		// add F from the previous round
	mov		\reg_b, %ebp
	or		\reg_a, %ebp
	rorx		$0x1b, \reg_a, %r12d
	rorx		$2, \reg_a, \reg_tb
	and		\reg_c, %ebp		// calculate F for the next round
	and		\reg_b, \reg_a
	or		%ebp, \reg_a
	add		%r12d, \reg_e
.endm

.macro	calc39
	calc_f3_pre	0x12c, %esi
	precalc23	%ymm8, 0x0, 0x80
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc40
	calc_f3_pre	0x140, %ebx
	precalc16	%ymm14, %ymm13, %ymm8, %ymm7
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc41
	calc_f3_pre	0x144, %ecx
	precalc17	%ymm14, %ymm12, %ymm7
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc42
	calc_f3_pre	0x148, %edx
	precalc18	%ymm7
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc43
	calc_f3_pre	0x14c, %eax
	precalc19	%ymm7
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc44
	calc_f3_pre	0x160, %edi
	precalc20	%ymm7
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc45
	calc_f3_pre	0x164, %esi
	precalc21	%ymm7
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc46
	calc_f3_pre	0x168, %ebx
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc47
	calc_f3_pre	0x16c, %ecx
	vpxor		%ymm9, %ymm0, %ymm7
	vpaddd		0x20(%r8), %ymm7, %ymm0
	vmovdqu		%ymm0, 0xa0(%r14)
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc48
	calc_f3_pre	0x180, %edx
	precalc16	%ymm13, %ymm12, %ymm7, %ymm5
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc49
	calc_f3_pre	0x184, %eax
	precalc17	%ymm13, %ymm8, %ymm5
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc50
	calc_f3_pre	0x188, %edi
	precalc18	%ymm5
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc51
	calc_f3_pre	0x18c, %esi
	precalc19	%ymm5
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc52
	calc_f3_pre	0x1a0, %ebx
	precalc20	%ymm5
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc53
	calc_f3_pre	0x1a4, %ecx
	precalc21	%ymm5
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc54
	calc_f3_pre	0x1a8, %edx
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc55
	calc_f3_pre	0x1ac, %eax
	precalc23	%ymm5, 0x20, 0xc0
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc56
	calc_f3_pre	0x1c0, %edi
	precalc16	%ymm12, %ymm8, %ymm5, %ymm3
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc57
	calc_f3_pre	0x1c4, %esi
	precalc17	%ymm12, %ymm7, %ymm3
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc58
	calc_f3_pre	0x1c8, %ebx
	precalc18	%ymm3
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc59
	calc_f2_pre	0x1cc, %ebx, %esi, %ecx
	precalc19	%ymm3
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc60
	calc_f2_pre	0x1e0, %ecx, %ebx, %edx
	precalc20	%ymm3
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc61
	calc_f2_pre	0x1e4, %edx, %ecx, %eax
	precalc21	%ymm3
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc62
	calc_f2_pre	0x1e8, %eax, %edx, %edi
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc63
	calc_f2_pre	0x1ec, %edi, %eax, %esi
	precalc23	%ymm3, 0x20, 0xe0
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc64
	calc_f2_pre	0x200, %esi, %edi, %ebx
	precalc32	%ymm5, %ymm3
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc65
	calc_f2_pre	0x204, %ebx, %esi, %ecx
	precalc33	%ymm14, %ymm15
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc66
	calc_f2_pre	0x208, %ecx, %ebx, %edx
	precalc34	%ymm8
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc67
	calc_f2_pre	0x20c, %edx, %ecx, %eax
	precalc35	%ymm15
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc68
	calc_f2_pre	0x220, %eax, %edx, %edi
	precalc36	%ymm15
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc69
	calc_f2_pre	0x224, %edi, %eax, %esi
	precalc37	%ymm15
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc70
	calc_f2_pre	0x228, %esi, %edi, %ebx
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc71
	calc_f2_pre	0x22c, %ebx, %esi, %ecx
	precalc39	%ymm15, 0x20, 0x100
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc72
	calc_f2_pre	0x240, %ecx, %ebx, %edx
	precalc32	%ymm3, %ymm15
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc73
	calc_f2_pre	0x244, %edx, %ecx, %eax
	precalc33	%ymm13, %ymm14
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc74
	calc_f2_pre	0x248, %eax, %edx, %edi
	precalc34	%ymm7
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc75
	calc_f2_pre	0x24c, %edi, %eax, %esi
	precalc35	%ymm14
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc76
	calc_f2_pre	0x260, %esi, %edi, %ebx
	precalc36	%ymm14
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc77
	calc_f2_pre	0x264, %ebx, %esi, %ecx
	precalc37	%ymm14
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc78
	calc_f2_pre	0x268, %ecx, %ebx, %edx
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc79
	add		0x26c(%r15), %eax
	add		%ecx, %eax
	rorx		$0x1b, %edx, %r12d
	precalc39	%ymm14, 0x20, 0x120
	add		%r12d, %eax
.endm

/*
 * Similar to calc0
 */
.macro	calc80
	mov		%ecx, %edx			// precalculate first round
	rorx		$2, %ecx, %ecx
	andn		%esi, %edx, %ebp
	and		%ebx, %edx
	xor		%ebp, %edx
	calc_f1_pre	0x10, %eax, %edx, %ebx, %edi
	precalc32	%ymm15, %ymm14
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc81
	calc_f1_pre	0x14, %edi, %eax, %ecx, %esi
	precalc33	%ymm12, %ymm13
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc82
	calc_f1_pre	0x18, %esi, %edi, %edx, %ebx
	precalc34	%ymm5
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc83
	calc_f1_pre	0x1c, %ebx, %esi, %eax, %ecx
	precalc35	%ymm13
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc84
	calc_f1_pre	0x30, %ecx, %ebx, %edi, %edx
	precalc36	%ymm13
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc85
	calc_f1_pre	0x34, %edx, %ecx, %esi, %eax
	precalc37	%ymm13
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc86
	calc_f1_pre	0x38, %eax, %edx, %ebx, %edi
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc87
	calc_f1_pre	0x3c, %edi, %eax, %ecx, %esi
	precalc39	%ymm13, 0x40, 0x140
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc88
	calc_f1_pre	0x50, %esi, %edi, %edx, %ebx
	precalc32	%ymm14, %ymm13
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc89
	calc_f1_pre	0x54, %ebx, %esi, %eax, %ecx
	precalc33	%ymm8, %ymm12
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc90
	calc_f1_pre	0x58, %ecx, %ebx, %edi, %edx
	precalc34	%ymm3
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc91
	calc_f1_pre	0x5c, %edx, %ecx, %esi, %eax
	precalc35	%ymm12
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc92
	calc_f1_pre	0x70, %eax, %edx, %ebx, %edi
	precalc36	%ymm12
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc93
	calc_f1_pre	0x74, %edi, %eax, %ecx, %esi
	precalc37	%ymm12
	calc_f1_post	%edi, %edx, %esi
.endm

.macro	calc94
	calc_f1_pre	0x78, %esi, %edi, %edx, %ebx
	calc_f1_post	%esi, %eax, %ebx
.endm

.macro	calc95
	calc_f1_pre	0x7c, %ebx, %esi, %eax, %ecx
	precalc39	%ymm12, 0x40, 0x160
	calc_f1_post	%ebx, %edi, %ecx
.endm

.macro	calc96
	calc_f1_pre	0x90, %ecx, %ebx, %edi, %edx
	precalc32	%ymm13, %ymm12
	calc_f1_post	%ecx, %esi, %edx
.endm

.macro	calc97
	calc_f1_pre	0x94, %edx, %ecx, %esi, %eax
	precalc33	%ymm7, %ymm8
	calc_f1_post	%edx, %ebx, %eax
.endm

.macro	calc98
	calc_f1_pre	0x98, %eax, %edx, %ebx, %edi
	precalc34	%ymm15
	calc_f1_post	%eax, %ecx, %edi
.endm

.macro	calc99
	calc_f2_pre	0x9c, %edi, %eax, %esi
	precalc35	%ymm8
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc100
	calc_f2_pre	0xb0, %esi, %edi, %ebx
	precalc36	%ymm8
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc101
	calc_f2_pre	0xb4, %ebx, %esi, %ecx
	precalc37	%ymm8
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc102
	calc_f2_pre	0xb8, %ecx, %ebx, %edx
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc103
	calc_f2_pre	0xbc, %edx, %ecx, %eax
	precalc39	%ymm8, 0x40, 0x180
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc104
	calc_f2_pre	0xd0, %eax, %edx, %edi
	precalc32	%ymm12, %ymm8
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc105
	calc_f2_pre	0xd4, %edi, %eax, %esi
	precalc33	%ymm5, %ymm7
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc106
	calc_f2_pre	0xd8, %esi, %edi, %ebx
	precalc34	%ymm14
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc107
	calc_f2_pre	0xdc, %ebx, %esi, %ecx
	precalc35	%ymm7
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc108
	calc_f2_pre	0xf0, %ecx, %ebx, %edx
	precalc36	%ymm7
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc109
	calc_f2_pre	0xf4, %edx, %ecx, %eax
	precalc37	%ymm7
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc110
	calc_f2_pre	0xf8, %eax, %edx, %edi
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc111
	calc_f2_pre	0xfc, %edi, %eax, %esi
	precalc39	%ymm7, 0x40, 0x1a0
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc112
	calc_f2_pre	0x110, %esi, %edi, %ebx
	precalc32	%ymm8, %ymm7
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc113
	calc_f2_pre	0x114, %ebx, %esi, %ecx
	precalc33	%ymm3, %ymm5
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc114
	calc_f2_pre	0x118, %ecx, %ebx, %edx
	precalc34	%ymm13
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc115
	calc_f2_pre	0x11c, %edx, %ecx, %eax
	precalc35	%ymm5
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc116
	calc_f2_pre	0x130, %eax, %edx, %edi
	precalc36	%ymm5
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc117
	calc_f2_pre	0x134, %edi, %eax, %esi
	precalc37	%ymm5
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc118
	calc_f2_pre	0x138, %esi, %edi, %ebx
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc119
	calc_f3_pre	0x13c, %ecx
	precalc39	%ymm5, 0x40, 0x1c0
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc120
	calc_f3_pre	0x150, %edx
	precalc32	%ymm7, %ymm5
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc121
	calc_f3_pre	0x154, %eax
	precalc33	%ymm15, %ymm3
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc122
	calc_f3_pre	0x158, %edi
	precalc34	%ymm12
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc123
	calc_f3_pre	0x15c, %esi
	precalc35	%ymm3
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc124
	calc_f3_pre	0x170, %ebx
	precalc36	%ymm3
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc125
	calc_f3_pre	0x174, %ecx
	precalc37	%ymm3
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc126
	calc_f3_pre	0x178, %edx
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc127
	calc_f3_pre	0x17c, %eax
	precalc39	%ymm3, 0x60, 0x1e0
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc128
	calc_f3_pre	0x190, %edi
	precalc32	%ymm5, %ymm3
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc129
	calc_f3_pre	0x194, %esi
	precalc33	%ymm14, %ymm15
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc130
	calc_f3_pre	0x198, %ebx
	precalc34	%ymm8
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc131
	calc_f3_pre	0x19c, %ecx
	precalc35	%ymm15
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc132
	calc_f3_pre	0x1b0, %edx
	precalc36	%ymm15
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc133
	calc_f3_pre	0x1b4, %eax
	precalc37	%ymm15
	calc_f3_post	%edx, %ebx, %esi, %eax, %ecx
.endm

.macro	calc134
	calc_f3_pre	0x1b8, %edi
	calc_f3_post	%eax, %ecx, %ebx, %edi, %edx
.endm

.macro	calc135
	calc_f3_pre	0x1bc, %esi
	precalc39	%ymm15, 0x60, 0x200
	calc_f3_post	%edi, %edx, %ecx, %esi, %eax
.endm

.macro	calc136
	calc_f3_pre	0x1d0, %ebx
	precalc32	%ymm3, %ymm15
	calc_f3_post	%esi, %eax, %edx, %ebx, %edi
.endm

.macro	calc137
	calc_f3_pre	0x1d4, %ecx
	precalc33	%ymm13, %ymm14
	calc_f3_post	%ebx, %edi, %eax, %ecx, %esi
.endm

.macro	calc138
	calc_f3_pre	0x1d8, %edx
	precalc34	%ymm7
	calc_f3_post	%ecx, %esi, %edi, %edx, %ebx
.endm

.macro	calc139
	calc_f2_pre	0x1dc, %edx, %ecx, %eax
	precalc35	%ymm14
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc140
	calc_f2_pre	0x1f0, %eax, %edx, %edi
	precalc36	%ymm14
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc141
	calc_f2_pre	0x1f4, %edi, %eax, %esi
	precalc37	%ymm14
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc142
	calc_f2_pre	0x1f8, %esi, %edi, %ebx
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc143
	calc_f2_pre	0x1fc, %ebx, %esi, %ecx
	precalc39	%ymm14, 0x60, 0x220
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc144
	calc_f2_pre	0x210, %ecx, %ebx, %edx
	precalc32	%ymm15, %ymm14
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc145
	calc_f2_pre	0x214, %edx, %ecx, %eax
	precalc33	%ymm12, %ymm13
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc146
	calc_f2_pre	0x218, %eax, %edx, %edi
	precalc34	%ymm5
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc147
	calc_f2_pre	0x21c, %edi, %eax, %esi
	precalc35	%ymm13
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc148
	calc_f2_pre	0x230, %esi, %edi, %ebx
	precalc36	%ymm13
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc149
	calc_f2_pre	0x234, %ebx, %esi, %ecx
	precalc37	%ymm13
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc150
	calc_f2_pre	0x238, %ecx, %ebx, %edx
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc151
	calc_f2_pre	0x23c, %edx, %ecx, %eax
	precalc39	%ymm13, 0x60, 0x240
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc152
	calc_f2_pre	0x250, %eax, %edx, %edi
	precalc32	%ymm14, %ymm13
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc153
	calc_f2_pre	0x254, %edi, %eax, %esi
	precalc33	%ymm8, %ymm12
	calc_f2_post	%edi, %edx, %ecx, %esi
.endm

.macro	calc154
	calc_f2_pre	0x258, %esi, %edi, %ebx
	precalc34	%ymm3
	calc_f2_post	%esi, %eax, %edx, %ebx
.endm

.macro	calc155
	calc_f2_pre	0x25c, %ebx, %esi, %ecx
	precalc35	%ymm12
	calc_f2_post	%ebx, %edi, %eax, %ecx
.endm

.macro	calc156
	calc_f2_pre	0x270, %ecx, %ebx, %edx
	precalc36	%ymm12
	calc_f2_post	%ecx, %esi, %edi, %edx
.endm

.macro	calc157
	calc_f2_pre	0x274, %edx, %ecx, %eax
	precalc37	%ymm12
	calc_f2_post	%edx, %ebx, %esi, %eax
.endm

.macro	calc158
	calc_f2_pre	0x278, %eax, %edx, %edi
	calc_f2_post	%eax, %ecx, %ebx, %edi
.endm

.macro	calc159
	add		0x27c(%r15), %esi
	add		%eax, %esi
	rorx		$0x1b, %edi, %r12d
	precalc39	%ymm12, 0x60, 0x260
	add		%r12d, %esi
.endm

	// sha1block(SHA1_CTX, buf, len)
ENTRY(_libmd_sha1block_avx2)
	push		%rbx
	push		%rbp
	push		%r12
	push		%r13
	push		%r14
	push		%r15
	sub		$1408+8, %rsp

	and		$~63, %rdx
	lea		k_xmm_ar(%rip), %r8
	mov		%rdi, %r9
	mov		%rsi, %r10
	lea		64(%rsi), %r13
	lea		64(%rsi, %rdx), %r11
	cmp		%r11, %r13
	cmovae		%r8, %r13
	vmovdqu		bswap_shufb_ctl(%rip), %ymm10

	mov		(%r9), %ecx
	mov		4(%r9), %esi
	mov		8(%r9), %edi
	mov		12(%r9), %eax
	mov		16(%r9), %edx
	mov		%rsp, %r14
	lea		2*4*80+32(%rsp), %r15
	precalc						// precalc WK for first 2 blocks
	xchg		%r14, %r15

	// this is unrolled
.Loop:	cmp		%r8, %r10			// we use the value of R8 (set below)
							// as a signal of the last block
	jne		.Lbegin
	add		$1408+8, %rsp
	pop		%r15
	pop		%r14
	pop		%r13
	pop		%r12
	pop		%rbp
	pop		%rbx
	vzeroupper
	ret

.Lbegin:
	calc0
	calc1
	calc2
	calc3
	calc4
	calc5
	calc6
	calc7
	calc8
	calc9
	calc10
	calc11
	calc12
	calc13
	calc14
	calc15
	calc16
	calc17
	calc18
	calc19
	calc20
	calc21
	calc22
	calc23
	calc24
	calc25
	calc26
	calc27
	calc28
	calc29
	calc30
	calc31
	calc32
	calc33
	calc34
	calc35
	calc36
	calc37
	calc38
	calc39
	calc40
	calc41
	calc42
	calc43
	calc44
	calc45
	calc46
	calc47
	calc48
	calc49
	calc50
	calc51
	calc52
	calc53
	calc54
	calc55
	calc56
	calc57
	calc58
	calc59

	add		$128, %r10		// move to the next even-64-byte block
	cmp		%r11, %r10		// is the current block the last one?
	cmovae		%r8, %r10		// signal the last iteration smartly

	calc60
	calc61
	calc62
	calc63
	calc64
	calc65
	calc66
	calc67
	calc68
	calc69
	calc70
	calc71
	calc72
	calc73
	calc74
	calc75
	calc76
	calc77
	calc78
	calc79

	update_hash	%eax, %edx, %ebx, %esi, %edi
	cmp		%r8, %r10		// is the current block the last one?
	je		.Loop
	mov		%edx, %ecx

	calc80
	calc81
	calc82
	calc83
	calc84
	calc85
	calc86
	calc87
	calc88
	calc89
	calc90
	calc91
	calc92
	calc93
	calc94
	calc95
	calc96
	calc97
	calc98
	calc99
	calc100
	calc101
	calc102
	calc103
	calc104
	calc105
	calc106
	calc107
	calc108
	calc109
	calc110
	calc111
	calc112
	calc113
	calc114
	calc115
	calc116
	calc117
	calc118
	calc119
	calc120
	calc121
	calc122
	calc123
	calc124
	calc125
	calc126
	calc127
	calc128
	calc129
	calc130
	calc131
	calc132
	calc133
	calc134
	calc135
	calc136
	calc137
	calc138
	calc139

	add		$128, %r13		// move to the next even-64-byte block
	cmp		%r11, %r13		// is the current block the last one?
	cmovae		%r8, %r10

	calc140
	calc141
	calc142
	calc143
	calc144
	calc145
	calc146
	calc147
	calc148
	calc149
	calc150
	calc151
	calc152
	calc153
	calc154
	calc155
	calc156
	calc157
	calc158
	calc159

	update_hash	%esi, %edi, %edx, %ecx, %ebx
	mov		%esi, %r12d		// reset state for AVX2 reg permutation
	mov		%edi, %esi
	mov		%edx, %edi
	mov		%ebx, %edx
	mov		%ecx, %eax
	mov		%r12d, %ecx
	xchg		%r14, %r15
	jmp		.Loop
END(_libmd_sha1block_avx2)

	.section	.rodata
	.balign		32
k_xmm_ar:
	.fill		8, 4, 0x5a827999
	.fill		8, 4, 0x6ed9eba1
	.fill		8, 4, 0x8f1bbcdc
	.fill		8, 4, 0xca62c1d6
	.size		k_xmm_ar, .-k_xmm_ar

bswap_shufb_ctl:
	.4byte		0x00010203
	.4byte		0x04050607
	.4byte		0x08090a0b
	.4byte		0x0c0d0e0f
	.4byte		0x00010203
	.4byte		0x04050607
	.4byte		0x08090a0b
	.4byte		0x0c0d0e0f
	.size		bswap_shufb_ctl, .-bswap_shufb_ctl

	/*
	 * SHA1 implementation using the Intel SHA extensions (SHANI).
	 *
	 * Imlemented according to the Intel white paper
	 *
	 * S. Gulley, V. Gopal, K. Yap, W. Feghali, J. Guilford,
	 * G. Wolrich: "Intel SHA Extensions: new instruction supporting
	 * the Secure Hash Algorithm on Intel® architecture processors",
	 * July 2013.
	 */
	// sha1block(SHA1_CTX, buf, len)
ENTRY(_libmd_sha1block_shani)
	and		$~63, %rdx		// round length to block-size multiple
	lea		(%rsi, %rdx, 1), %rcx	// end pointer
	test		%rdx, %rdx		// nothing to do?
	je		1f			// if so, terminate immediately

	movdqu		(%rdi), %xmm6		// h0, h1, h2, h3
	pxor		%xmm7, %xmm7
	pshufd		$0x1b, %xmm6, %xmm6	// h3, h2, h1, h0
	pinsrd		$3, 16(%rdi), %xmm7	// h4 in the highest word of xmm7
	movdqu		shuf_mask(%rip), %xmm4

	// main loop
0:	movdqa		%xmm6, %xmm8		// stash ABCD
	movdqa		%xmm7, %xmm9		// stash E

	// rounds 0--3
	movdqu		0*16(%rsi), %xmm0	// load first message block
	pshufb		%xmm4, %xmm0		// and byte-swap
	paddd		%xmm0, %xmm7		// E += w[0]
	movdqa		%xmm6, %xmm5		// E' = A
	sha1rnds4	$0, %xmm7, %xmm6	// perform rounds 0--3

	// rounds 4--7
	movdqu		1*16(%rsi), %xmm1
	pshufb		%xmm4, %xmm1
	sha1nexte	%xmm1, %xmm5
	movdqa		%xmm6, %xmm7
	sha1rnds4	$0, %xmm5, %xmm6
	sha1msg1	%xmm1, %xmm0

	// rounds 8--11
	movdqu		2*16(%rsi), %xmm2
	pshufb		%xmm4, %xmm2
	sha1nexte	%xmm2, %xmm7
	movdqa		%xmm6, %xmm5
	sha1rnds4	$0, %xmm7, %xmm6
	sha1msg1	%xmm2, %xmm1
	pxor		%xmm2, %xmm0

.macro	midround	msg3, msg0, msg1, msg2, e1, e0, k
	sha1nexte	\msg3, \e1
	movdqa		%xmm6, \e0
	sha1msg2	\msg3, \msg0
	sha1rnds4	$\k, \e1, %xmm6
	sha1msg1	\msg3, \msg2
	pxor		\msg3, \msg1
.endm

	movdqu		3*16(%rsi), %xmm3	// load third message block
	pshufb		%xmm4, %xmm3

	add		$4*16, %rsi

	midround	%xmm3, %xmm0, %xmm1, %xmm2, %xmm5, %xmm7, 0	// 12--15
	midround	%xmm0, %xmm1, %xmm2, %xmm3, %xmm7, %xmm5, 0	// 16--19
	midround	%xmm1, %xmm2, %xmm3, %xmm0, %xmm5, %xmm7, 1	// 20--23
	midround	%xmm2, %xmm3, %xmm0, %xmm1, %xmm7, %xmm5, 1	// 24--27
	midround	%xmm3, %xmm0, %xmm1, %xmm2, %xmm5, %xmm7, 1	// 28--31
	midround	%xmm0, %xmm1, %xmm2, %xmm3, %xmm7, %xmm5, 1	// 32--35
	midround	%xmm1, %xmm2, %xmm3, %xmm0, %xmm5, %xmm7, 1	// 36--39
	midround	%xmm2, %xmm3, %xmm0, %xmm1, %xmm7, %xmm5, 2	// 40--43
	midround	%xmm3, %xmm0, %xmm1, %xmm2, %xmm5, %xmm7, 2	// 44--47
	midround	%xmm0, %xmm1, %xmm2, %xmm3, %xmm7, %xmm5, 2	// 48--51
	midround	%xmm1, %xmm2, %xmm3, %xmm0, %xmm5, %xmm7, 2	// 52--55
	midround	%xmm2, %xmm3, %xmm0, %xmm1, %xmm7, %xmm5, 2	// 56--59
	midround	%xmm3, %xmm0, %xmm1, %xmm2, %xmm5, %xmm7, 3	// 60--63
	midround	%xmm0, %xmm1, %xmm2, %xmm3, %xmm7, %xmm5, 3	// 64--67

	// rounds 68--71
	sha1nexte	%xmm1, %xmm5
	movdqa		%xmm6, %xmm7
	sha1msg2	%xmm1, %xmm2
	sha1rnds4	$3, %xmm5, %xmm6
	pxor		%xmm1, %xmm3

	// rounds 72--75
	sha1nexte	%xmm2, %xmm7
	movdqa		%xmm6, %xmm5
	sha1msg2	%xmm2, %xmm3
	sha1rnds4	$3, %xmm7, %xmm6

	// rounds 76--79
	sha1nexte	%xmm3, %xmm5
	movdqa		%xmm6, %xmm7
	sha1rnds4	$3, %xmm5, %xmm6

	sha1nexte	%xmm9, %xmm7		// add saved E
	paddd		%xmm8, %xmm6		// add saved ABCD

	cmp		%rsi, %rcx		// end reached?
	jne		0b

	pshufd		$0x1b, %xmm6, %xmm6	// restore order of h0--h3
	movdqu		%xmm6, (%rdi)		// write h0--h3
	pextrd		$3, %xmm7, 16(%rdi)	// write h4
1:	ret
END(_libmd_sha1block_shani)

	.section	.rodata
	.balign		16
shuf_mask:
	.8byte		0x08090a0b0c0d0e0f
	.8byte		0x0001020304050607
	.size		shuf_mask, .-shuf_mask

	.section .note.GNU-stack,"",%progbits
