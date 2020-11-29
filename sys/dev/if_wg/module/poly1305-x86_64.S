// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
//
// Copyright (C) 2017-2018 Samuel Neves <sneves@dei.uc.pt>. All Rights Reserved.
// Copyright (C) 2017-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
// Copyright (C) 2006-2017 CRYPTOGAMS by <appro@openssl.org>. All Rights Reserved.
//
// This code is taken from the OpenSSL project but the author, Andy Polyakov,
// has relicensed it under the licenses specified in the SPDX header above.
// The original headers, including the original license headers, are
// included below for completeness.
//
// ====================================================================
// Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
// project. The module is, however, dual licensed under OpenSSL and
// CRYPTOGAMS licenses depending on where you obtain it. For further
// details see http://www.openssl.org/~appro/cryptogams/.
// ====================================================================
//
// This module implements Poly1305 hash for x86_64.
//
// March 2015
//
// Initial release.
//
// December 2016
//
// Add AVX512F+VL+BW code path.
//
// November 2017
//
// Convert AVX512F+VL+BW code path to pure AVX512F, so that it can be
// executed even on Knights Landing. Trigger for modification was
// observation that AVX512 code paths can negatively affect overall
// Skylake-X system performance. Since we are likely to suppress
// AVX512F capability flag [at least on Skylake-X], conversion serves
// as kind of "investment protection". Note that next *lake processor,
// Cannolake, has AVX512IFMA code path to execute...
//
// Numbers are cycles per processed byte with poly1305_blocks alone,
// measured with rdtsc at fixed clock frequency.
//
//		IALU/gcc-4.8(*)	AVX(**)		AVX2	AVX-512
// P4		4.46/+120%	-
// Core 2	2.41/+90%	-
// Westmere	1.88/+120%	-
// Sandy Bridge	1.39/+140%	1.10
// Haswell	1.14/+175%	1.11		0.65
// Skylake[-X]	1.13/+120%	0.96		0.51	[0.35]
// Silvermont	2.83/+95%	-
// Knights L	3.60/?		1.65		1.10	0.41(***)
// Goldmont	1.70/+180%	-
// VIA Nano	1.82/+150%	-
// Sledgehammer	1.38/+160%	-
// Bulldozer	2.30/+130%	0.97
// Ryzen		1.15/+200%	1.08		1.18
//
// (*)	improvement coefficients relative to clang are more modest and
//	are ~50% on most processors, in both cases we are comparing to
//	__int128 code;
// (**)	SSE2 implementation was attempted, but among non-AVX processors
//	it was faster than integer-only code only on older Intel P4 and
//	Core processors, 50-30%, less newer processor is, but slower on
//	contemporary ones, for example almost 2x slower on Atom, and as
//	former are naturally disappearing, SSE2 is deemed unnecessary;
// (***)	strangely enough performance seems to vary from core to core,
//	listed result is best case;

// #include <linux/linkage.h>
.section .rodata
.align	64
.Lconst:
.Lmask24:
.long	0x0ffffff,0,0x0ffffff,0,0x0ffffff,0,0x0ffffff,0
.L129:
.long	16777216,0,16777216,0,16777216,0,16777216,0
.Lmask26:
.long	0x3ffffff,0,0x3ffffff,0,0x3ffffff,0,0x3ffffff,0
.Lpermd_avx2:
.long	2,2,2,3,2,0,2,1
.Lpermd_avx512:
.long	0,0,0,1, 0,2,0,3, 0,4,0,5, 0,6,0,7

.L2_44_inp_permd:
.long	0,1,1,2,2,3,7,7
.L2_44_inp_shift:
.quad	0,12,24,64
.L2_44_mask:
.quad	0xfffffffffff,0xfffffffffff,0x3ffffffffff,0xffffffffffffffff
.L2_44_shift_rgt:
.quad	44,44,42,64
.L2_44_shift_lft:
.quad	8,8,10,64

.align	64
.Lx_mask44:
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
.Lx_mask42:
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
.text
.align 32
SYM_FUNC_START(poly1305_init_x86_64)
.Lpoly1305_init_x86_64:
	xor	%rax,%rax
	mov	%rax,0(%rdi)		# initialize hash value
	mov	%rax,8(%rdi)
	mov	%rax,16(%rdi)

	cmp	$0,%rsi
	je	.Lno_key
	mov	$0x0ffffffc0fffffff,%rax
	mov	$0x0ffffffc0ffffffc,%rcx
	and	0(%rsi),%rax
	and	8(%rsi),%rcx
	mov	%rax,24(%rdi)
	mov	%rcx,32(%rdi)
	mov	$1,%eax
.Lno_key:
	ret
SYM_FUNC_END(poly1305_init_x86_64)
.align 32
SYM_FUNC_START(poly1305_blocks_x86_64)
.Lpoly1305_blocks_x86_64:
.Lblocks:
	shr	$4,%rdx
	jz	.Lno_data		# too short

	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rdi
.Lblocks_body:

	mov	%rdx,%r15		# reassign %rdx

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	mov	0(%rdi),%r14		# load hash value
	mov	8(%rdi),%rbx
	mov	16(%rdi),%r10

	mov	%r13,%r12
	shr	$2,%r13
	mov	%r12,%rax
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)
	jmp	.Loop

.align	32
.Loop:
	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	mulq	%r14			# h0*r1
	mov	%rax,%r9
	 mov	%r11,%rax
	mov	%rdx,%rdi

	mulq	%r14			# h0*r0
	mov	%rax,%r14		# future %r14
	 mov	%r11,%rax
	mov	%rdx,%r8

	mulq	%rbx			# h1*r0
	add	%rax,%r9
	 mov	%r13,%rax
	adc	%rdx,%rdi

	mulq	%rbx			# h1*s1
	 mov	%r10,%rbx			# borrow %rbx
	add	%rax,%r14
	adc	%rdx,%r8

	imulq	%r13,%rbx			# h2*s1
	add	%rbx,%r9
	 mov	%r8,%rbx
	adc	$0,%rdi

	imulq	%r11,%r10			# h2*r0
	add	%r9,%rbx
	mov	$-4,%rax		# mask value
	adc	%r10,%rdi

	and	%rdi,%rax		# last reduction step
	mov	%rdi,%r10
	shr	$2,%rdi
	and	$3,%r10
	add	%rdi,%rax
	add	%rax,%r14
	adc	$0,%rbx
	adc	$0,%r10
	mov	%r12,%rax
	dec	%r15			# len-=16
	jnz	.Loop

	mov	0(%rsp),%rdi

	mov	%r14,0(%rdi)		# store hash value
	mov	%rbx,8(%rdi)
	mov	%r10,16(%rdi)

	mov	8(%rsp),%r15
	mov	16(%rsp),%r14
	mov	24(%rsp),%r13
	mov	32(%rsp),%r12
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rsp
.Lno_data:
.Lblocks_epilogue:
	ret
SYM_FUNC_END(poly1305_blocks_x86_64)
.align 32
SYM_FUNC_START(poly1305_emit_x86_64)
.Lpoly1305_emit_x86_64:
.Lemit:
	mov	0(%rdi),%r8	# load hash value
	mov	8(%rdi),%r9
	mov	16(%rdi),%r10

	mov	%r8,%rax
	add	$5,%r8		# compare to modulus
	mov	%r9,%rcx
	adc	$0,%r9
	adc	$0,%r10
	shr	$2,%r10	# did 130-bit value overflow?
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx

	add	0(%rdx),%rax	# accumulate nonce
	adc	8(%rdx),%rcx
	mov	%rax,0(%rsi)	# write result
	mov	%rcx,8(%rsi)

	ret
SYM_FUNC_END(poly1305_emit_x86_64)
#ifdef CONFIG_AS_AVX
.type	__poly1305_block,@function
.align	32
__poly1305_block:
	push %rdi
	mulq	%r14			# h0*r1
	mov	%rax,%r9
	 mov	%r11,%rax
	mov	%rdx,%rdi

	mulq	%r14			# h0*r0
	mov	%rax,%r14		# future %r14
	 mov	%r11,%rax
	mov	%rdx,%r8

	mulq	%rbx			# h1*r0
	add	%rax,%r9
	 mov	%r13,%rax
	adc	%rdx,%rdi

	mulq	%rbx			# h1*s1
	 mov	%r10,%rbx			# borrow %rbx
	add	%rax,%r14
	adc	%rdx,%r8

	imulq	%r13,%rbx			# h2*s1
	add	%rbx,%r9
	 mov	%r8,%rbx
	adc	$0,%rdi

	imulq	%r11,%r10			# h2*r0
	add	%r9,%rbx
	mov	$-4,%rax		# mask value
	adc	%r10,%rdi

	and	%rdi,%rax		# last reduction step
	mov	%rdi,%r10
	shr	$2,%rdi
	and	$3,%r10
	add	%rdi,%rax
	add	%rax,%r14
	adc	$0,%rbx
	adc	$0,%r10
	pop %rdi
	ret
.size	__poly1305_block,.-__poly1305_block

.type	__poly1305_init_avx,@function
.align	32
__poly1305_init_avx:
	push %rbp
	mov %rsp,%rbp
	mov	%r11,%r14
	mov	%r12,%rbx
	xor	%r10,%r10

	lea	48+64(%rdi),%rdi	# size optimization

	mov	%r12,%rax
	call	__poly1305_block	# r^2

	mov	$0x3ffffff,%eax	# save interleaved r^2 and r base 2^26
	mov	$0x3ffffff,%edx
	mov	%r14,%r8
	and	%r14d,%eax
	mov	%r11,%r9
	and	%r11d,%edx
	mov	%eax,-64(%rdi)
	shr	$26,%r8
	mov	%edx,-60(%rdi)
	shr	$26,%r9

	mov	$0x3ffffff,%eax
	mov	$0x3ffffff,%edx
	and	%r8d,%eax
	and	%r9d,%edx
	mov	%eax,-48(%rdi)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,-44(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,-32(%rdi)
	shr	$26,%r8
	mov	%edx,-28(%rdi)
	shr	$26,%r9

	mov	%rbx,%rax
	mov	%r12,%rdx
	shl	$12,%rax
	shl	$12,%rdx
	or	%r8,%rax
	or	%r9,%rdx
	and	$0x3ffffff,%eax
	and	$0x3ffffff,%edx
	mov	%eax,-16(%rdi)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,-12(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,0(%rdi)
	mov	%rbx,%r8
	mov	%edx,4(%rdi)
	mov	%r12,%r9

	mov	$0x3ffffff,%eax
	mov	$0x3ffffff,%edx
	shr	$14,%r8
	shr	$14,%r9
	and	%r8d,%eax
	and	%r9d,%edx
	mov	%eax,16(%rdi)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,20(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,32(%rdi)
	shr	$26,%r8
	mov	%edx,36(%rdi)
	shr	$26,%r9

	mov	%r10,%rax
	shl	$24,%rax
	or	%rax,%r8
	mov	%r8d,48(%rdi)
	lea	(%r8,%r8,4),%r8		# *5
	mov	%r9d,52(%rdi)
	lea	(%r9,%r9,4),%r9		# *5
	mov	%r8d,64(%rdi)
	mov	%r9d,68(%rdi)

	mov	%r12,%rax
	call	__poly1305_block	# r^3

	mov	$0x3ffffff,%eax	# save r^3 base 2^26
	mov	%r14,%r8
	and	%r14d,%eax
	shr	$26,%r8
	mov	%eax,-52(%rdi)

	mov	$0x3ffffff,%edx
	and	%r8d,%edx
	mov	%edx,-36(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	$26,%r8
	mov	%edx,-20(%rdi)

	mov	%rbx,%rax
	shl	$12,%rax
	or	%r8,%rax
	and	$0x3ffffff,%eax
	mov	%eax,-4(%rdi)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%rbx,%r8
	mov	%eax,12(%rdi)

	mov	$0x3ffffff,%edx
	shr	$14,%r8
	and	%r8d,%edx
	mov	%edx,28(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	$26,%r8
	mov	%edx,44(%rdi)

	mov	%r10,%rax
	shl	$24,%rax
	or	%rax,%r8
	mov	%r8d,60(%rdi)
	lea	(%r8,%r8,4),%r8		# *5
	mov	%r8d,76(%rdi)

	mov	%r12,%rax
	call	__poly1305_block	# r^4

	mov	$0x3ffffff,%eax	# save r^4 base 2^26
	mov	%r14,%r8
	and	%r14d,%eax
	shr	$26,%r8
	mov	%eax,-56(%rdi)

	mov	$0x3ffffff,%edx
	and	%r8d,%edx
	mov	%edx,-40(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	$26,%r8
	mov	%edx,-24(%rdi)

	mov	%rbx,%rax
	shl	$12,%rax
	or	%r8,%rax
	and	$0x3ffffff,%eax
	mov	%eax,-8(%rdi)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%rbx,%r8
	mov	%eax,8(%rdi)

	mov	$0x3ffffff,%edx
	shr	$14,%r8
	and	%r8d,%edx
	mov	%edx,24(%rdi)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	$26,%r8
	mov	%edx,40(%rdi)

	mov	%r10,%rax
	shl	$24,%rax
	or	%rax,%r8
	mov	%r8d,56(%rdi)
	lea	(%r8,%r8,4),%r8		# *5
	mov	%r8d,72(%rdi)

	lea	-48-64(%rdi),%rdi	# size [de-]optimization
	pop %rbp
	ret
.size	__poly1305_init_avx,.-__poly1305_init_avx
.align 32
SYM_FUNC_START(poly1305_blocks_avx)
.Lpoly1305_blocks_avx:
	mov	20(%rdi),%r8d		# is_base2_26
	cmp	$128,%rdx
	jae	.Lblocks_avx
	test	%r8d,%r8d
	jz	.Lblocks

.Lblocks_avx:
	and	$-16,%rdx
	jz	.Lno_data_avx

	vzeroupper

	test	%r8d,%r8d
	jz	.Lbase2_64_avx

	test	$31,%rdx
	jz	.Leven_avx

	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_avx_body:

	mov	%rdx,%r15		# reassign %rdx

	mov	0(%rdi),%r8		# load hash value
	mov	8(%rdi),%r9
	mov	16(%rdi),%r10d

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	################################# base 2^26 -> base 2^64
	mov	%r8d,%r14d
	and	$-2147483648,%r8
	mov	%r9,%r12			# borrow %r12
	mov	%r9d,%ebx
	and	$-2147483648,%r9

	shr	$6,%r8
	shl	$52,%r12
	add	%r8,%r14
	shr	$12,%rbx
	shr	$18,%r9
	add	%r12,%r14
	adc	%r9,%rbx

	mov	%r10,%r8
	shl	$40,%r8
	shr	$24,%r10
	add	%r8,%rbx
	adc	$0,%r10			# can be partially reduced...

	mov	$-4,%r9		# ... so reduce
	mov	%r10,%r8
	and	%r10,%r9
	shr	$2,%r8
	and	$3,%r10
	add	%r9,%r8			# =*5
	add	%r8,%r14
	adc	$0,%rbx
	adc	$0,%r10

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10

	call	__poly1305_block

	test	%rcx,%rcx		# if %rcx is zero,
	jz	.Lstore_base2_64_avx	# store hash in base 2^64 format

	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r11
	mov	%rbx,%r12
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r11
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r11,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r12
	and	$0x3ffffff,%rbx		# h[3]
	or	%r12,%r10			# h[4]

	sub	$16,%r15
	jz	.Lstore_base2_26_avx

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	jmp	.Lproceed_avx

.align	32
.Lstore_base2_64_avx:
	mov	%r14,0(%rdi)
	mov	%rbx,8(%rdi)
	mov	%r10,16(%rdi)		# note that is_base2_26 is zeroed
	jmp	.Ldone_avx

.align	16
.Lstore_base2_26_avx:
	mov	%eax,0(%rdi)		# store hash value base 2^26
	mov	%edx,4(%rdi)
	mov	%r14d,8(%rdi)
	mov	%ebx,12(%rdi)
	mov	%r10d,16(%rdi)
.align	16
.Ldone_avx:
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lno_data_avx:
.Lblocks_avx_epilogue:
	ret

.align	32
.Lbase2_64_avx:
	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lbase2_64_avx_body:

	mov	%rdx,%r15		# reassign %rdx

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	mov	0(%rdi),%r14		# load hash value
	mov	8(%rdi),%rbx
	mov	16(%rdi),%r10d

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

	test	$31,%rdx
	jz	.Linit_avx

	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	sub	$16,%r15

	call	__poly1305_block

.Linit_avx:
	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r8
	mov	%rbx,%r9
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r8
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r8,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r9
	and	$0x3ffffff,%rbx		# h[3]
	or	%r9,%r10			# h[4]

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	movl	$1,20(%rdi)		# set is_base2_26

	call	__poly1305_init_avx

.Lproceed_avx:
	mov	%r15,%rdx
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lbase2_64_avx_epilogue:
	jmp	.Ldo_avx

.align	32
.Leven_avx:
	vmovd		4*0(%rdi),%xmm0		# load hash value
	vmovd		4*1(%rdi),%xmm1
	vmovd		4*2(%rdi),%xmm2
	vmovd		4*3(%rdi),%xmm3
	vmovd		4*4(%rdi),%xmm4

.Ldo_avx:
	lea		8(%rsp),%r10
	and		$-32,%rsp
	sub		$-8,%rsp
	lea		-0x58(%rsp),%r11
	sub		$0x178,%rsp
	
	sub		$64,%rdx
	lea		-32(%rsi),%rax
	cmovc		%rax,%rsi

	vmovdqu		48(%rdi),%xmm14	# preload r0^2
	lea		112(%rdi),%rdi	# size optimization
	lea		.Lconst(%rip),%rcx

	################################################################
	# load input
	vmovdqu		16*2(%rsi),%xmm5
	vmovdqu		16*3(%rsi),%xmm6
	vmovdqa		64(%rcx),%xmm15		# .Lmask26

	vpsrldq		$6,%xmm5,%xmm7		# splat input
	vpsrldq		$6,%xmm6,%xmm8
	vpunpckhqdq	%xmm6,%xmm5,%xmm9		# 4
	vpunpcklqdq	%xmm6,%xmm5,%xmm5		# 0:1
	vpunpcklqdq	%xmm8,%xmm7,%xmm8		# 2:3

	vpsrlq		$40,%xmm9,%xmm9		# 4
	vpsrlq		$26,%xmm5,%xmm6
	vpand		%xmm15,%xmm5,%xmm5		# 0
	vpsrlq		$4,%xmm8,%xmm7
	vpand		%xmm15,%xmm6,%xmm6		# 1
	vpsrlq		$30,%xmm8,%xmm8
	vpand		%xmm15,%xmm7,%xmm7		# 2
	vpand		%xmm15,%xmm8,%xmm8		# 3
	vpor		32(%rcx),%xmm9,%xmm9	# padbit, yes, always

	jbe		.Lskip_loop_avx

	# expand and copy pre-calculated table to stack
	vmovdqu		-48(%rdi),%xmm11
	vmovdqu		-32(%rdi),%xmm12
	vpshufd		$0xEE,%xmm14,%xmm13		# 34xx -> 3434
	vpshufd		$0x44,%xmm14,%xmm10		# xx12 -> 1212
	vmovdqa		%xmm13,-0x90(%r11)
	vmovdqa		%xmm10,0x00(%rsp)
	vpshufd		$0xEE,%xmm11,%xmm14
	vmovdqu		-16(%rdi),%xmm10
	vpshufd		$0x44,%xmm11,%xmm11
	vmovdqa		%xmm14,-0x80(%r11)
	vmovdqa		%xmm11,0x10(%rsp)
	vpshufd		$0xEE,%xmm12,%xmm13
	vmovdqu		0(%rdi),%xmm11
	vpshufd		$0x44,%xmm12,%xmm12
	vmovdqa		%xmm13,-0x70(%r11)
	vmovdqa		%xmm12,0x20(%rsp)
	vpshufd		$0xEE,%xmm10,%xmm14
	vmovdqu		16(%rdi),%xmm12
	vpshufd		$0x44,%xmm10,%xmm10
	vmovdqa		%xmm14,-0x60(%r11)
	vmovdqa		%xmm10,0x30(%rsp)
	vpshufd		$0xEE,%xmm11,%xmm13
	vmovdqu		32(%rdi),%xmm10
	vpshufd		$0x44,%xmm11,%xmm11
	vmovdqa		%xmm13,-0x50(%r11)
	vmovdqa		%xmm11,0x40(%rsp)
	vpshufd		$0xEE,%xmm12,%xmm14
	vmovdqu		48(%rdi),%xmm11
	vpshufd		$0x44,%xmm12,%xmm12
	vmovdqa		%xmm14,-0x40(%r11)
	vmovdqa		%xmm12,0x50(%rsp)
	vpshufd		$0xEE,%xmm10,%xmm13
	vmovdqu		64(%rdi),%xmm12
	vpshufd		$0x44,%xmm10,%xmm10
	vmovdqa		%xmm13,-0x30(%r11)
	vmovdqa		%xmm10,0x60(%rsp)
	vpshufd		$0xEE,%xmm11,%xmm14
	vpshufd		$0x44,%xmm11,%xmm11
	vmovdqa		%xmm14,-0x20(%r11)
	vmovdqa		%xmm11,0x70(%rsp)
	vpshufd		$0xEE,%xmm12,%xmm13
	 vmovdqa	0x00(%rsp),%xmm14		# preload r0^2
	vpshufd		$0x44,%xmm12,%xmm12
	vmovdqa		%xmm13,-0x10(%r11)
	vmovdqa		%xmm12,0x80(%rsp)

	jmp		.Loop_avx

.align	32
.Loop_avx:
	################################################################
	# ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2
	# ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^3+inp[7]*r
	#   ___________________/
	# ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2+inp[8])*r^2
	# ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^4+inp[7]*r^2+inp[9])*r
	#   ___________________/ ____________________/
	#
	# Note that we start with inp[2:3]*r^2. This is because it
	# doesn't depend on reduction in previous iteration.
	################################################################
	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# though note that  and  are "reversed" in this section,
	# and %xmm14 is preloaded with r0^2...

	vpmuludq	%xmm5,%xmm14,%xmm10		# d0 = h0*r0
	vpmuludq	%xmm6,%xmm14,%xmm11		# d1 = h1*r0
	  vmovdqa	%xmm2,0x20(%r11)				# offload hash
	vpmuludq	%xmm7,%xmm14,%xmm12		# d3 = h2*r0
	 vmovdqa	0x10(%rsp),%xmm2		# r1^2
	vpmuludq	%xmm8,%xmm14,%xmm13		# d3 = h3*r0
	vpmuludq	%xmm9,%xmm14,%xmm14		# d4 = h4*r0

	  vmovdqa	%xmm0,0x00(%r11)				#
	vpmuludq	0x20(%rsp),%xmm9,%xmm0	# h4*s1
	  vmovdqa	%xmm1,0x10(%r11)				#
	vpmuludq	%xmm8,%xmm2,%xmm1		# h3*r1
	vpaddq		%xmm0,%xmm10,%xmm10		# d0 += h4*s1
	vpaddq		%xmm1,%xmm14,%xmm14		# d4 += h3*r1
	  vmovdqa	%xmm3,0x30(%r11)				#
	vpmuludq	%xmm7,%xmm2,%xmm0		# h2*r1
	vpmuludq	%xmm6,%xmm2,%xmm1		# h1*r1
	vpaddq		%xmm0,%xmm13,%xmm13		# d3 += h2*r1
	 vmovdqa	0x30(%rsp),%xmm3		# r2^2
	vpaddq		%xmm1,%xmm12,%xmm12		# d2 += h1*r1
	  vmovdqa	%xmm4,0x40(%r11)				#
	vpmuludq	%xmm5,%xmm2,%xmm2		# h0*r1
	 vpmuludq	%xmm7,%xmm3,%xmm0		# h2*r2
	vpaddq		%xmm2,%xmm11,%xmm11		# d1 += h0*r1

	 vmovdqa	0x40(%rsp),%xmm4		# s2^2
	vpaddq		%xmm0,%xmm14,%xmm14		# d4 += h2*r2
	vpmuludq	%xmm6,%xmm3,%xmm1		# h1*r2
	vpmuludq	%xmm5,%xmm3,%xmm3		# h0*r2
	vpaddq		%xmm1,%xmm13,%xmm13		# d3 += h1*r2
	 vmovdqa	0x50(%rsp),%xmm2		# r3^2
	vpaddq		%xmm3,%xmm12,%xmm12		# d2 += h0*r2
	vpmuludq	%xmm9,%xmm4,%xmm0		# h4*s2
	vpmuludq	%xmm8,%xmm4,%xmm4		# h3*s2
	vpaddq		%xmm0,%xmm11,%xmm11		# d1 += h4*s2
	 vmovdqa	0x60(%rsp),%xmm3		# s3^2
	vpaddq		%xmm4,%xmm10,%xmm10		# d0 += h3*s2

	 vmovdqa	0x80(%rsp),%xmm4		# s4^2
	vpmuludq	%xmm6,%xmm2,%xmm1		# h1*r3
	vpmuludq	%xmm5,%xmm2,%xmm2		# h0*r3
	vpaddq		%xmm1,%xmm14,%xmm14		# d4 += h1*r3
	vpaddq		%xmm2,%xmm13,%xmm13		# d3 += h0*r3
	vpmuludq	%xmm9,%xmm3,%xmm0		# h4*s3
	vpmuludq	%xmm8,%xmm3,%xmm1		# h3*s3
	vpaddq		%xmm0,%xmm12,%xmm12		# d2 += h4*s3
	 vmovdqu	16*0(%rsi),%xmm0				# load input
	vpaddq		%xmm1,%xmm11,%xmm11		# d1 += h3*s3
	vpmuludq	%xmm7,%xmm3,%xmm3		# h2*s3
	 vpmuludq	%xmm7,%xmm4,%xmm7		# h2*s4
	vpaddq		%xmm3,%xmm10,%xmm10		# d0 += h2*s3

	 vmovdqu	16*1(%rsi),%xmm1				#
	vpaddq		%xmm7,%xmm11,%xmm11		# d1 += h2*s4
	vpmuludq	%xmm8,%xmm4,%xmm8		# h3*s4
	vpmuludq	%xmm9,%xmm4,%xmm9		# h4*s4
	 vpsrldq	$6,%xmm0,%xmm2				# splat input
	vpaddq		%xmm8,%xmm12,%xmm12		# d2 += h3*s4
	vpaddq		%xmm9,%xmm13,%xmm13		# d3 += h4*s4
	 vpsrldq	$6,%xmm1,%xmm3				#
	vpmuludq	0x70(%rsp),%xmm5,%xmm9	# h0*r4
	vpmuludq	%xmm6,%xmm4,%xmm5		# h1*s4
	 vpunpckhqdq	%xmm1,%xmm0,%xmm4		# 4
	vpaddq		%xmm9,%xmm14,%xmm14		# d4 += h0*r4
	 vmovdqa	-0x90(%r11),%xmm9		# r0^4
	vpaddq		%xmm5,%xmm10,%xmm10		# d0 += h1*s4

	vpunpcklqdq	%xmm1,%xmm0,%xmm0		# 0:1
	vpunpcklqdq	%xmm3,%xmm2,%xmm3		# 2:3

	#vpsrlq		$40,%xmm4,%xmm4		# 4
	vpsrldq		$5,%xmm4,%xmm4	# 4
	vpsrlq		$26,%xmm0,%xmm1
	vpand		%xmm15,%xmm0,%xmm0		# 0
	vpsrlq		$4,%xmm3,%xmm2
	vpand		%xmm15,%xmm1,%xmm1		# 1
	vpand		0(%rcx),%xmm4,%xmm4		# .Lmask24
	vpsrlq		$30,%xmm3,%xmm3
	vpand		%xmm15,%xmm2,%xmm2		# 2
	vpand		%xmm15,%xmm3,%xmm3		# 3
	vpor		32(%rcx),%xmm4,%xmm4	# padbit, yes, always

	vpaddq		0x00(%r11),%xmm0,%xmm0	# add hash value
	vpaddq		0x10(%r11),%xmm1,%xmm1
	vpaddq		0x20(%r11),%xmm2,%xmm2
	vpaddq		0x30(%r11),%xmm3,%xmm3
	vpaddq		0x40(%r11),%xmm4,%xmm4

	lea		16*2(%rsi),%rax
	lea		16*4(%rsi),%rsi
	sub		$64,%rdx
	cmovc		%rax,%rsi

	################################################################
	# Now we accumulate (inp[0:1]+hash)*r^4
	################################################################
	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4

	vpmuludq	%xmm0,%xmm9,%xmm5		# h0*r0
	vpmuludq	%xmm1,%xmm9,%xmm6		# h1*r0
	vpaddq		%xmm5,%xmm10,%xmm10
	vpaddq		%xmm6,%xmm11,%xmm11
	 vmovdqa	-0x80(%r11),%xmm7		# r1^4
	vpmuludq	%xmm2,%xmm9,%xmm5		# h2*r0
	vpmuludq	%xmm3,%xmm9,%xmm6		# h3*r0
	vpaddq		%xmm5,%xmm12,%xmm12
	vpaddq		%xmm6,%xmm13,%xmm13
	vpmuludq	%xmm4,%xmm9,%xmm9		# h4*r0
	 vpmuludq	-0x70(%r11),%xmm4,%xmm5	# h4*s1
	vpaddq		%xmm9,%xmm14,%xmm14

	vpaddq		%xmm5,%xmm10,%xmm10		# d0 += h4*s1
	vpmuludq	%xmm2,%xmm7,%xmm6		# h2*r1
	vpmuludq	%xmm3,%xmm7,%xmm5		# h3*r1
	vpaddq		%xmm6,%xmm13,%xmm13		# d3 += h2*r1
	 vmovdqa	-0x60(%r11),%xmm8		# r2^4
	vpaddq		%xmm5,%xmm14,%xmm14		# d4 += h3*r1
	vpmuludq	%xmm1,%xmm7,%xmm6		# h1*r1
	vpmuludq	%xmm0,%xmm7,%xmm7		# h0*r1
	vpaddq		%xmm6,%xmm12,%xmm12		# d2 += h1*r1
	vpaddq		%xmm7,%xmm11,%xmm11		# d1 += h0*r1

	 vmovdqa	-0x50(%r11),%xmm9		# s2^4
	vpmuludq	%xmm2,%xmm8,%xmm5		# h2*r2
	vpmuludq	%xmm1,%xmm8,%xmm6		# h1*r2
	vpaddq		%xmm5,%xmm14,%xmm14		# d4 += h2*r2
	vpaddq		%xmm6,%xmm13,%xmm13		# d3 += h1*r2
	 vmovdqa	-0x40(%r11),%xmm7		# r3^4
	vpmuludq	%xmm0,%xmm8,%xmm8		# h0*r2
	vpmuludq	%xmm4,%xmm9,%xmm5		# h4*s2
	vpaddq		%xmm8,%xmm12,%xmm12		# d2 += h0*r2
	vpaddq		%xmm5,%xmm11,%xmm11		# d1 += h4*s2
	 vmovdqa	-0x30(%r11),%xmm8		# s3^4
	vpmuludq	%xmm3,%xmm9,%xmm9		# h3*s2
	 vpmuludq	%xmm1,%xmm7,%xmm6		# h1*r3
	vpaddq		%xmm9,%xmm10,%xmm10		# d0 += h3*s2

	 vmovdqa	-0x10(%r11),%xmm9		# s4^4
	vpaddq		%xmm6,%xmm14,%xmm14		# d4 += h1*r3
	vpmuludq	%xmm0,%xmm7,%xmm7		# h0*r3
	vpmuludq	%xmm4,%xmm8,%xmm5		# h4*s3
	vpaddq		%xmm7,%xmm13,%xmm13		# d3 += h0*r3
	vpaddq		%xmm5,%xmm12,%xmm12		# d2 += h4*s3
	 vmovdqu	16*2(%rsi),%xmm5				# load input
	vpmuludq	%xmm3,%xmm8,%xmm7		# h3*s3
	vpmuludq	%xmm2,%xmm8,%xmm8		# h2*s3
	vpaddq		%xmm7,%xmm11,%xmm11		# d1 += h3*s3
	 vmovdqu	16*3(%rsi),%xmm6				#
	vpaddq		%xmm8,%xmm10,%xmm10		# d0 += h2*s3

	vpmuludq	%xmm2,%xmm9,%xmm2		# h2*s4
	vpmuludq	%xmm3,%xmm9,%xmm3		# h3*s4
	 vpsrldq	$6,%xmm5,%xmm7				# splat input
	vpaddq		%xmm2,%xmm11,%xmm11		# d1 += h2*s4
	vpmuludq	%xmm4,%xmm9,%xmm4		# h4*s4
	 vpsrldq	$6,%xmm6,%xmm8				#
	vpaddq		%xmm3,%xmm12,%xmm2		# h2 = d2 + h3*s4
	vpaddq		%xmm4,%xmm13,%xmm3		# h3 = d3 + h4*s4
	vpmuludq	-0x20(%r11),%xmm0,%xmm4	# h0*r4
	vpmuludq	%xmm1,%xmm9,%xmm0
	 vpunpckhqdq	%xmm6,%xmm5,%xmm9		# 4
	vpaddq		%xmm4,%xmm14,%xmm4		# h4 = d4 + h0*r4
	vpaddq		%xmm0,%xmm10,%xmm0		# h0 = d0 + h1*s4

	vpunpcklqdq	%xmm6,%xmm5,%xmm5		# 0:1
	vpunpcklqdq	%xmm8,%xmm7,%xmm8		# 2:3

	#vpsrlq		$40,%xmm9,%xmm9		# 4
	vpsrldq		$5,%xmm9,%xmm9	# 4
	vpsrlq		$26,%xmm5,%xmm6
	 vmovdqa	0x00(%rsp),%xmm14		# preload r0^2
	vpand		%xmm15,%xmm5,%xmm5		# 0
	vpsrlq		$4,%xmm8,%xmm7
	vpand		%xmm15,%xmm6,%xmm6		# 1
	vpand		0(%rcx),%xmm9,%xmm9		# .Lmask24
	vpsrlq		$30,%xmm8,%xmm8
	vpand		%xmm15,%xmm7,%xmm7		# 2
	vpand		%xmm15,%xmm8,%xmm8		# 3
	vpor		32(%rcx),%xmm9,%xmm9	# padbit, yes, always

	################################################################
	# lazy reduction as discussed in "NEON crypto" by D.J. Bernstein
	# and P. Schwabe

	vpsrlq		$26,%xmm3,%xmm13
	vpand		%xmm15,%xmm3,%xmm3
	vpaddq		%xmm13,%xmm4,%xmm4		# h3 -> h4

	vpsrlq		$26,%xmm0,%xmm10
	vpand		%xmm15,%xmm0,%xmm0
	vpaddq		%xmm10,%xmm11,%xmm1		# h0 -> h1

	vpsrlq		$26,%xmm4,%xmm10
	vpand		%xmm15,%xmm4,%xmm4

	vpsrlq		$26,%xmm1,%xmm11
	vpand		%xmm15,%xmm1,%xmm1
	vpaddq		%xmm11,%xmm2,%xmm2		# h1 -> h2

	vpaddq		%xmm10,%xmm0,%xmm0
	vpsllq		$2,%xmm10,%xmm10
	vpaddq		%xmm10,%xmm0,%xmm0		# h4 -> h0

	vpsrlq		$26,%xmm2,%xmm12
	vpand		%xmm15,%xmm2,%xmm2
	vpaddq		%xmm12,%xmm3,%xmm3		# h2 -> h3

	vpsrlq		$26,%xmm0,%xmm10
	vpand		%xmm15,%xmm0,%xmm0
	vpaddq		%xmm10,%xmm1,%xmm1		# h0 -> h1

	vpsrlq		$26,%xmm3,%xmm13
	vpand		%xmm15,%xmm3,%xmm3
	vpaddq		%xmm13,%xmm4,%xmm4		# h3 -> h4

	ja		.Loop_avx

.Lskip_loop_avx:
	################################################################
	# multiply (inp[0:1]+hash) or inp[2:3] by r^2:r^1

	vpshufd		$0x10,%xmm14,%xmm14		# r0^n, xx12 -> x1x2
	add		$32,%rdx
	jnz		.Long_tail_avx

	vpaddq		%xmm2,%xmm7,%xmm7
	vpaddq		%xmm0,%xmm5,%xmm5
	vpaddq		%xmm1,%xmm6,%xmm6
	vpaddq		%xmm3,%xmm8,%xmm8
	vpaddq		%xmm4,%xmm9,%xmm9

.Long_tail_avx:
	vmovdqa		%xmm2,0x20(%r11)
	vmovdqa		%xmm0,0x00(%r11)
	vmovdqa		%xmm1,0x10(%r11)
	vmovdqa		%xmm3,0x30(%r11)
	vmovdqa		%xmm4,0x40(%r11)

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4

	vpmuludq	%xmm7,%xmm14,%xmm12		# d2 = h2*r0
	vpmuludq	%xmm5,%xmm14,%xmm10		# d0 = h0*r0
	 vpshufd	$0x10,-48(%rdi),%xmm2		# r1^n
	vpmuludq	%xmm6,%xmm14,%xmm11		# d1 = h1*r0
	vpmuludq	%xmm8,%xmm14,%xmm13		# d3 = h3*r0
	vpmuludq	%xmm9,%xmm14,%xmm14		# d4 = h4*r0

	vpmuludq	%xmm8,%xmm2,%xmm0		# h3*r1
	vpaddq		%xmm0,%xmm14,%xmm14		# d4 += h3*r1
	 vpshufd	$0x10,-32(%rdi),%xmm3		# s1^n
	vpmuludq	%xmm7,%xmm2,%xmm1		# h2*r1
	vpaddq		%xmm1,%xmm13,%xmm13		# d3 += h2*r1
	 vpshufd	$0x10,-16(%rdi),%xmm4		# r2^n
	vpmuludq	%xmm6,%xmm2,%xmm0		# h1*r1
	vpaddq		%xmm0,%xmm12,%xmm12		# d2 += h1*r1
	vpmuludq	%xmm5,%xmm2,%xmm2		# h0*r1
	vpaddq		%xmm2,%xmm11,%xmm11		# d1 += h0*r1
	vpmuludq	%xmm9,%xmm3,%xmm3		# h4*s1
	vpaddq		%xmm3,%xmm10,%xmm10		# d0 += h4*s1

	 vpshufd	$0x10,0(%rdi),%xmm2		# s2^n
	vpmuludq	%xmm7,%xmm4,%xmm1		# h2*r2
	vpaddq		%xmm1,%xmm14,%xmm14		# d4 += h2*r2
	vpmuludq	%xmm6,%xmm4,%xmm0		# h1*r2
	vpaddq		%xmm0,%xmm13,%xmm13		# d3 += h1*r2
	 vpshufd	$0x10,16(%rdi),%xmm3		# r3^n
	vpmuludq	%xmm5,%xmm4,%xmm4		# h0*r2
	vpaddq		%xmm4,%xmm12,%xmm12		# d2 += h0*r2
	vpmuludq	%xmm9,%xmm2,%xmm1		# h4*s2
	vpaddq		%xmm1,%xmm11,%xmm11		# d1 += h4*s2
	 vpshufd	$0x10,32(%rdi),%xmm4		# s3^n
	vpmuludq	%xmm8,%xmm2,%xmm2		# h3*s2
	vpaddq		%xmm2,%xmm10,%xmm10		# d0 += h3*s2

	vpmuludq	%xmm6,%xmm3,%xmm0		# h1*r3
	vpaddq		%xmm0,%xmm14,%xmm14		# d4 += h1*r3
	vpmuludq	%xmm5,%xmm3,%xmm3		# h0*r3
	vpaddq		%xmm3,%xmm13,%xmm13		# d3 += h0*r3
	 vpshufd	$0x10,48(%rdi),%xmm2		# r4^n
	vpmuludq	%xmm9,%xmm4,%xmm1		# h4*s3
	vpaddq		%xmm1,%xmm12,%xmm12		# d2 += h4*s3
	 vpshufd	$0x10,64(%rdi),%xmm3		# s4^n
	vpmuludq	%xmm8,%xmm4,%xmm0		# h3*s3
	vpaddq		%xmm0,%xmm11,%xmm11		# d1 += h3*s3
	vpmuludq	%xmm7,%xmm4,%xmm4		# h2*s3
	vpaddq		%xmm4,%xmm10,%xmm10		# d0 += h2*s3

	vpmuludq	%xmm5,%xmm2,%xmm2		# h0*r4
	vpaddq		%xmm2,%xmm14,%xmm14		# h4 = d4 + h0*r4
	vpmuludq	%xmm9,%xmm3,%xmm1		# h4*s4
	vpaddq		%xmm1,%xmm13,%xmm13		# h3 = d3 + h4*s4
	vpmuludq	%xmm8,%xmm3,%xmm0		# h3*s4
	vpaddq		%xmm0,%xmm12,%xmm12		# h2 = d2 + h3*s4
	vpmuludq	%xmm7,%xmm3,%xmm1		# h2*s4
	vpaddq		%xmm1,%xmm11,%xmm11		# h1 = d1 + h2*s4
	vpmuludq	%xmm6,%xmm3,%xmm3		# h1*s4
	vpaddq		%xmm3,%xmm10,%xmm10		# h0 = d0 + h1*s4

	jz		.Lshort_tail_avx

	vmovdqu		16*0(%rsi),%xmm0		# load input
	vmovdqu		16*1(%rsi),%xmm1

	vpsrldq		$6,%xmm0,%xmm2		# splat input
	vpsrldq		$6,%xmm1,%xmm3
	vpunpckhqdq	%xmm1,%xmm0,%xmm4		# 4
	vpunpcklqdq	%xmm1,%xmm0,%xmm0		# 0:1
	vpunpcklqdq	%xmm3,%xmm2,%xmm3		# 2:3

	vpsrlq		$40,%xmm4,%xmm4		# 4
	vpsrlq		$26,%xmm0,%xmm1
	vpand		%xmm15,%xmm0,%xmm0		# 0
	vpsrlq		$4,%xmm3,%xmm2
	vpand		%xmm15,%xmm1,%xmm1		# 1
	vpsrlq		$30,%xmm3,%xmm3
	vpand		%xmm15,%xmm2,%xmm2		# 2
	vpand		%xmm15,%xmm3,%xmm3		# 3
	vpor		32(%rcx),%xmm4,%xmm4	# padbit, yes, always

	vpshufd		$0x32,-64(%rdi),%xmm9	# r0^n, 34xx -> x3x4
	vpaddq		0x00(%r11),%xmm0,%xmm0
	vpaddq		0x10(%r11),%xmm1,%xmm1
	vpaddq		0x20(%r11),%xmm2,%xmm2
	vpaddq		0x30(%r11),%xmm3,%xmm3
	vpaddq		0x40(%r11),%xmm4,%xmm4

	################################################################
	# multiply (inp[0:1]+hash) by r^4:r^3 and accumulate

	vpmuludq	%xmm0,%xmm9,%xmm5		# h0*r0
	vpaddq		%xmm5,%xmm10,%xmm10		# d0 += h0*r0
	vpmuludq	%xmm1,%xmm9,%xmm6		# h1*r0
	vpaddq		%xmm6,%xmm11,%xmm11		# d1 += h1*r0
	vpmuludq	%xmm2,%xmm9,%xmm5		# h2*r0
	vpaddq		%xmm5,%xmm12,%xmm12		# d2 += h2*r0
	 vpshufd	$0x32,-48(%rdi),%xmm7		# r1^n
	vpmuludq	%xmm3,%xmm9,%xmm6		# h3*r0
	vpaddq		%xmm6,%xmm13,%xmm13		# d3 += h3*r0
	vpmuludq	%xmm4,%xmm9,%xmm9		# h4*r0
	vpaddq		%xmm9,%xmm14,%xmm14		# d4 += h4*r0

	vpmuludq	%xmm3,%xmm7,%xmm5		# h3*r1
	vpaddq		%xmm5,%xmm14,%xmm14		# d4 += h3*r1
	 vpshufd	$0x32,-32(%rdi),%xmm8		# s1
	vpmuludq	%xmm2,%xmm7,%xmm6		# h2*r1
	vpaddq		%xmm6,%xmm13,%xmm13		# d3 += h2*r1
	 vpshufd	$0x32,-16(%rdi),%xmm9		# r2
	vpmuludq	%xmm1,%xmm7,%xmm5		# h1*r1
	vpaddq		%xmm5,%xmm12,%xmm12		# d2 += h1*r1
	vpmuludq	%xmm0,%xmm7,%xmm7		# h0*r1
	vpaddq		%xmm7,%xmm11,%xmm11		# d1 += h0*r1
	vpmuludq	%xmm4,%xmm8,%xmm8		# h4*s1
	vpaddq		%xmm8,%xmm10,%xmm10		# d0 += h4*s1

	 vpshufd	$0x32,0(%rdi),%xmm7		# s2
	vpmuludq	%xmm2,%xmm9,%xmm6		# h2*r2
	vpaddq		%xmm6,%xmm14,%xmm14		# d4 += h2*r2
	vpmuludq	%xmm1,%xmm9,%xmm5		# h1*r2
	vpaddq		%xmm5,%xmm13,%xmm13		# d3 += h1*r2
	 vpshufd	$0x32,16(%rdi),%xmm8		# r3
	vpmuludq	%xmm0,%xmm9,%xmm9		# h0*r2
	vpaddq		%xmm9,%xmm12,%xmm12		# d2 += h0*r2
	vpmuludq	%xmm4,%xmm7,%xmm6		# h4*s2
	vpaddq		%xmm6,%xmm11,%xmm11		# d1 += h4*s2
	 vpshufd	$0x32,32(%rdi),%xmm9		# s3
	vpmuludq	%xmm3,%xmm7,%xmm7		# h3*s2
	vpaddq		%xmm7,%xmm10,%xmm10		# d0 += h3*s2

	vpmuludq	%xmm1,%xmm8,%xmm5		# h1*r3
	vpaddq		%xmm5,%xmm14,%xmm14		# d4 += h1*r3
	vpmuludq	%xmm0,%xmm8,%xmm8		# h0*r3
	vpaddq		%xmm8,%xmm13,%xmm13		# d3 += h0*r3
	 vpshufd	$0x32,48(%rdi),%xmm7		# r4
	vpmuludq	%xmm4,%xmm9,%xmm6		# h4*s3
	vpaddq		%xmm6,%xmm12,%xmm12		# d2 += h4*s3
	 vpshufd	$0x32,64(%rdi),%xmm8		# s4
	vpmuludq	%xmm3,%xmm9,%xmm5		# h3*s3
	vpaddq		%xmm5,%xmm11,%xmm11		# d1 += h3*s3
	vpmuludq	%xmm2,%xmm9,%xmm9		# h2*s3
	vpaddq		%xmm9,%xmm10,%xmm10		# d0 += h2*s3

	vpmuludq	%xmm0,%xmm7,%xmm7		# h0*r4
	vpaddq		%xmm7,%xmm14,%xmm14		# d4 += h0*r4
	vpmuludq	%xmm4,%xmm8,%xmm6		# h4*s4
	vpaddq		%xmm6,%xmm13,%xmm13		# d3 += h4*s4
	vpmuludq	%xmm3,%xmm8,%xmm5		# h3*s4
	vpaddq		%xmm5,%xmm12,%xmm12		# d2 += h3*s4
	vpmuludq	%xmm2,%xmm8,%xmm6		# h2*s4
	vpaddq		%xmm6,%xmm11,%xmm11		# d1 += h2*s4
	vpmuludq	%xmm1,%xmm8,%xmm8		# h1*s4
	vpaddq		%xmm8,%xmm10,%xmm10		# d0 += h1*s4

.Lshort_tail_avx:
	################################################################
	# horizontal addition

	vpsrldq		$8,%xmm14,%xmm9
	vpsrldq		$8,%xmm13,%xmm8
	vpsrldq		$8,%xmm11,%xmm6
	vpsrldq		$8,%xmm10,%xmm5
	vpsrldq		$8,%xmm12,%xmm7
	vpaddq		%xmm8,%xmm13,%xmm13
	vpaddq		%xmm9,%xmm14,%xmm14
	vpaddq		%xmm5,%xmm10,%xmm10
	vpaddq		%xmm6,%xmm11,%xmm11
	vpaddq		%xmm7,%xmm12,%xmm12

	################################################################
	# lazy reduction

	vpsrlq		$26,%xmm13,%xmm3
	vpand		%xmm15,%xmm13,%xmm13
	vpaddq		%xmm3,%xmm14,%xmm14		# h3 -> h4

	vpsrlq		$26,%xmm10,%xmm0
	vpand		%xmm15,%xmm10,%xmm10
	vpaddq		%xmm0,%xmm11,%xmm11		# h0 -> h1

	vpsrlq		$26,%xmm14,%xmm4
	vpand		%xmm15,%xmm14,%xmm14

	vpsrlq		$26,%xmm11,%xmm1
	vpand		%xmm15,%xmm11,%xmm11
	vpaddq		%xmm1,%xmm12,%xmm12		# h1 -> h2

	vpaddq		%xmm4,%xmm10,%xmm10
	vpsllq		$2,%xmm4,%xmm4
	vpaddq		%xmm4,%xmm10,%xmm10		# h4 -> h0

	vpsrlq		$26,%xmm12,%xmm2
	vpand		%xmm15,%xmm12,%xmm12
	vpaddq		%xmm2,%xmm13,%xmm13		# h2 -> h3

	vpsrlq		$26,%xmm10,%xmm0
	vpand		%xmm15,%xmm10,%xmm10
	vpaddq		%xmm0,%xmm11,%xmm11		# h0 -> h1

	vpsrlq		$26,%xmm13,%xmm3
	vpand		%xmm15,%xmm13,%xmm13
	vpaddq		%xmm3,%xmm14,%xmm14		# h3 -> h4

	vmovd		%xmm10,-112(%rdi)	# save partially reduced
	vmovd		%xmm11,-108(%rdi)
	vmovd		%xmm12,-104(%rdi)
	vmovd		%xmm13,-100(%rdi)
	vmovd		%xmm14,-96(%rdi)
	lea		-8(%r10),%rsp
	vzeroupper
	ret
SYM_FUNC_END(poly1305_blocks_avx)
.align 32
SYM_FUNC_START(poly1305_emit_avx)
.Lpoly1305_emit_avx:
	cmpl	$0,20(%rdi)	# is_base2_26?
	je	.Lemit

	mov	0(%rdi),%eax	# load hash value base 2^26
	mov	4(%rdi),%ecx
	mov	8(%rdi),%r8d
	mov	12(%rdi),%r11d
	mov	16(%rdi),%r10d

	shl	$26,%rcx	# base 2^26 -> base 2^64
	mov	%r8,%r9
	shl	$52,%r8
	add	%rcx,%rax
	shr	$12,%r9
	add	%rax,%r8	# h0
	adc	$0,%r9

	shl	$14,%r11
	mov	%r10,%rax
	shr	$24,%r10
	add	%r11,%r9
	shl	$40,%rax
	add	%rax,%r9	# h1
	adc	$0,%r10	# h2

	mov	%r10,%rax	# could be partially reduced, so reduce
	mov	%r10,%rcx
	and	$3,%r10
	shr	$2,%rax
	and	$-4,%rcx
	add	%rcx,%rax
	add	%rax,%r8
	adc	$0,%r9
	adc	$0,%r10

	mov	%r8,%rax
	add	$5,%r8		# compare to modulus
	mov	%r9,%rcx
	adc	$0,%r9
	adc	$0,%r10
	shr	$2,%r10	# did 130-bit value overflow?
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx

	add	0(%rdx),%rax	# accumulate nonce
	adc	8(%rdx),%rcx
	mov	%rax,0(%rsi)	# write result
	mov	%rcx,8(%rsi)

	ret
SYM_FUNC_END(poly1305_emit_avx)
#endif
#ifdef CONFIG_AS_AVX2
.align 32
SYM_FUNC_START(poly1305_blocks_avx2)
.Lpoly1305_blocks_avx2:
	mov	20(%rdi),%r8d		# is_base2_26
	cmp	$128,%rdx
	jae	.Lblocks_avx2
	test	%r8d,%r8d
	jz	.Lblocks

.Lblocks_avx2:
	and	$-16,%rdx
	jz	.Lno_data_avx2

	vzeroupper

	test	%r8d,%r8d
	jz	.Lbase2_64_avx2

	test	$63,%rdx
	jz	.Leven_avx2

	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_avx2_body:

	mov	%rdx,%r15		# reassign %rdx

	mov	0(%rdi),%r8		# load hash value
	mov	8(%rdi),%r9
	mov	16(%rdi),%r10d

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	################################# base 2^26 -> base 2^64
	mov	%r8d,%r14d
	and	$-2147483648,%r8
	mov	%r9,%r12			# borrow %r12
	mov	%r9d,%ebx
	and	$-2147483648,%r9

	shr	$6,%r8
	shl	$52,%r12
	add	%r8,%r14
	shr	$12,%rbx
	shr	$18,%r9
	add	%r12,%r14
	adc	%r9,%rbx

	mov	%r10,%r8
	shl	$40,%r8
	shr	$24,%r10
	add	%r8,%rbx
	adc	$0,%r10			# can be partially reduced...

	mov	$-4,%r9		# ... so reduce
	mov	%r10,%r8
	and	%r10,%r9
	shr	$2,%r8
	and	$3,%r10
	add	%r9,%r8			# =*5
	add	%r8,%r14
	adc	$0,%rbx
	adc	$0,%r10

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

.Lbase2_26_pre_avx2:
	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	sub	$16,%r15

	call	__poly1305_block
	mov	%r12,%rax

	test	$63,%r15
	jnz	.Lbase2_26_pre_avx2

	test	%rcx,%rcx		# if %rcx is zero,
	jz	.Lstore_base2_64_avx2	# store hash in base 2^64 format

	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r11
	mov	%rbx,%r12
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r11
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r11,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r12
	and	$0x3ffffff,%rbx		# h[3]
	or	%r12,%r10			# h[4]

	test	%r15,%r15
	jz	.Lstore_base2_26_avx2

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	jmp	.Lproceed_avx2

.align	32
.Lstore_base2_64_avx2:
	mov	%r14,0(%rdi)
	mov	%rbx,8(%rdi)
	mov	%r10,16(%rdi)		# note that is_base2_26 is zeroed
	jmp	.Ldone_avx2

.align	16
.Lstore_base2_26_avx2:
	mov	%eax,0(%rdi)		# store hash value base 2^26
	mov	%edx,4(%rdi)
	mov	%r14d,8(%rdi)
	mov	%ebx,12(%rdi)
	mov	%r10d,16(%rdi)
.align	16
.Ldone_avx2:
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lno_data_avx2:
.Lblocks_avx2_epilogue:
	ret

.align	32
.Lbase2_64_avx2:
	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lbase2_64_avx2_body:

	mov	%rdx,%r15		# reassign %rdx

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	mov	0(%rdi),%r14		# load hash value
	mov	8(%rdi),%rbx
	mov	16(%rdi),%r10d

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

	test	$63,%rdx
	jz	.Linit_avx2

.Lbase2_64_pre_avx2:
	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	sub	$16,%r15

	call	__poly1305_block
	mov	%r12,%rax

	test	$63,%r15
	jnz	.Lbase2_64_pre_avx2

.Linit_avx2:
	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r8
	mov	%rbx,%r9
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r8
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r8,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r9
	and	$0x3ffffff,%rbx		# h[3]
	or	%r9,%r10			# h[4]

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	movl	$1,20(%rdi)		# set is_base2_26

	call	__poly1305_init_avx

.Lproceed_avx2:
	mov	%r15,%rdx			# restore %rdx
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lbase2_64_avx2_epilogue:
	jmp	.Ldo_avx2

.align	32
.Leven_avx2:
	vmovd		4*0(%rdi),%xmm0	# load hash value base 2^26
	vmovd		4*1(%rdi),%xmm1
	vmovd		4*2(%rdi),%xmm2
	vmovd		4*3(%rdi),%xmm3
	vmovd		4*4(%rdi),%xmm4

.Ldo_avx2:
	lea		8(%rsp),%r10
	sub		$0x128,%rsp
	lea		.Lconst(%rip),%rcx
	lea		48+64(%rdi),%rdi	# size optimization
	vmovdqa		96(%rcx),%ymm7		# .Lpermd_avx2

	# expand and copy pre-calculated table to stack
	vmovdqu		-64(%rdi),%xmm9
	and		$-512,%rsp
	vmovdqu		-48(%rdi),%xmm10
	vmovdqu		-32(%rdi),%xmm6
	vmovdqu		-16(%rdi),%xmm11
	vmovdqu		0(%rdi),%xmm12
	vmovdqu		16(%rdi),%xmm13
	lea		0x90(%rsp),%rax		# size optimization
	vmovdqu		32(%rdi),%xmm14
	vpermd		%ymm9,%ymm7,%ymm9		# 00003412 -> 14243444
	vmovdqu		48(%rdi),%xmm15
	vpermd		%ymm10,%ymm7,%ymm10
	vmovdqu		64(%rdi),%xmm5
	vpermd		%ymm6,%ymm7,%ymm6
	vmovdqa		%ymm9,0x00(%rsp)
	vpermd		%ymm11,%ymm7,%ymm11
	vmovdqa		%ymm10,0x20-0x90(%rax)
	vpermd		%ymm12,%ymm7,%ymm12
	vmovdqa		%ymm6,0x40-0x90(%rax)
	vpermd		%ymm13,%ymm7,%ymm13
	vmovdqa		%ymm11,0x60-0x90(%rax)
	vpermd		%ymm14,%ymm7,%ymm14
	vmovdqa		%ymm12,0x80-0x90(%rax)
	vpermd		%ymm15,%ymm7,%ymm15
	vmovdqa		%ymm13,0xa0-0x90(%rax)
	vpermd		%ymm5,%ymm7,%ymm5
	vmovdqa		%ymm14,0xc0-0x90(%rax)
	vmovdqa		%ymm15,0xe0-0x90(%rax)
	vmovdqa		%ymm5,0x100-0x90(%rax)
	vmovdqa		64(%rcx),%ymm5		# .Lmask26

	################################################################
	# load input
	vmovdqu		16*0(%rsi),%xmm7
	vmovdqu		16*1(%rsi),%xmm8
	vinserti128	$1,16*2(%rsi),%ymm7,%ymm7
	vinserti128	$1,16*3(%rsi),%ymm8,%ymm8
	lea		16*4(%rsi),%rsi

	vpsrldq		$6,%ymm7,%ymm9		# splat input
	vpsrldq		$6,%ymm8,%ymm10
	vpunpckhqdq	%ymm8,%ymm7,%ymm6		# 4
	vpunpcklqdq	%ymm10,%ymm9,%ymm9		# 2:3
	vpunpcklqdq	%ymm8,%ymm7,%ymm7		# 0:1

	vpsrlq		$30,%ymm9,%ymm10
	vpsrlq		$4,%ymm9,%ymm9
	vpsrlq		$26,%ymm7,%ymm8
	vpsrlq		$40,%ymm6,%ymm6		# 4
	vpand		%ymm5,%ymm9,%ymm9		# 2
	vpand		%ymm5,%ymm7,%ymm7		# 0
	vpand		%ymm5,%ymm8,%ymm8		# 1
	vpand		%ymm5,%ymm10,%ymm10		# 3
	vpor		32(%rcx),%ymm6,%ymm6	# padbit, yes, always

	vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	sub		$64,%rdx
	jz		.Ltail_avx2
	jmp		.Loop_avx2

.align	32
.Loop_avx2:
	################################################################
	# ((inp[0]*r^4+inp[4])*r^4+inp[ 8])*r^4
	# ((inp[1]*r^4+inp[5])*r^4+inp[ 9])*r^3
	# ((inp[2]*r^4+inp[6])*r^4+inp[10])*r^2
	# ((inp[3]*r^4+inp[7])*r^4+inp[11])*r^1
	#   ________/__________/
	################################################################
	#vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	vpaddq		%ymm0,%ymm7,%ymm0
	vmovdqa		0(%rsp),%ymm7	# r0^4
	vpaddq		%ymm1,%ymm8,%ymm1
	vmovdqa		32(%rsp),%ymm8	# r1^4
	vpaddq		%ymm3,%ymm10,%ymm3
	vmovdqa		96(%rsp),%ymm9	# r2^4
	vpaddq		%ymm4,%ymm6,%ymm4
	vmovdqa		48(%rax),%ymm10	# s3^4
	vmovdqa		112(%rax),%ymm5	# s4^4

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# however, as h2 is "chronologically" first one available pull
	# corresponding operations up, so it's
	#
	# d4 = h2*r2   + h4*r0 + h3*r1             + h1*r3   + h0*r4
	# d3 = h2*r1   + h3*r0           + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0           + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h2*5*r4 + h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3
	# d0 = h2*5*r3 + h0*r0 + h4*5*r1 + h3*5*r2           + h1*5*r4

	vpmuludq	%ymm2,%ymm7,%ymm13		# d2 = h2*r0
	vpmuludq	%ymm2,%ymm8,%ymm14		# d3 = h2*r1
	vpmuludq	%ymm2,%ymm9,%ymm15		# d4 = h2*r2
	vpmuludq	%ymm2,%ymm10,%ymm11		# d0 = h2*s3
	vpmuludq	%ymm2,%ymm5,%ymm12		# d1 = h2*s4

	vpmuludq	%ymm0,%ymm8,%ymm6		# h0*r1
	vpmuludq	%ymm1,%ymm8,%ymm2		# h1*r1, borrow %ymm2 as temp
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h0*r1
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h1*r1
	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*r1
	vpmuludq	64(%rsp),%ymm4,%ymm2	# h4*s1
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h3*r1
	vpaddq		%ymm2,%ymm11,%ymm11		# d0 += h4*s1
	 vmovdqa	-16(%rax),%ymm8	# s2

	vpmuludq	%ymm0,%ymm7,%ymm6		# h0*r0
	vpmuludq	%ymm1,%ymm7,%ymm2		# h1*r0
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h0*r0
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h1*r0
	vpmuludq	%ymm3,%ymm7,%ymm6		# h3*r0
	vpmuludq	%ymm4,%ymm7,%ymm2		# h4*r0
	 vmovdqu	16*0(%rsi),%xmm7	# load input
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h3*r0
	vpaddq		%ymm2,%ymm15,%ymm15		# d4 += h4*r0
	 vinserti128	$1,16*2(%rsi),%ymm7,%ymm7

	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*s2
	vpmuludq	%ymm4,%ymm8,%ymm2		# h4*s2
	 vmovdqu	16*1(%rsi),%xmm8
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h3*s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h4*s2
	 vmovdqa	16(%rax),%ymm2	# r3
	vpmuludq	%ymm1,%ymm9,%ymm6		# h1*r2
	vpmuludq	%ymm0,%ymm9,%ymm9		# h0*r2
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h1*r2
	vpaddq		%ymm9,%ymm13,%ymm13		# d2 += h0*r2
	 vinserti128	$1,16*3(%rsi),%ymm8,%ymm8
	 lea		16*4(%rsi),%rsi

	vpmuludq	%ymm1,%ymm2,%ymm6		# h1*r3
	vpmuludq	%ymm0,%ymm2,%ymm2		# h0*r3
	 vpsrldq	$6,%ymm7,%ymm9		# splat input
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h1*r3
	vpaddq		%ymm2,%ymm14,%ymm14		# d3 += h0*r3
	vpmuludq	%ymm3,%ymm10,%ymm6		# h3*s3
	vpmuludq	%ymm4,%ymm10,%ymm2		# h4*s3
	 vpsrldq	$6,%ymm8,%ymm10
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h3*s3
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h4*s3
	 vpunpckhqdq	%ymm8,%ymm7,%ymm6		# 4

	vpmuludq	%ymm3,%ymm5,%ymm3		# h3*s4
	vpmuludq	%ymm4,%ymm5,%ymm4		# h4*s4
	 vpunpcklqdq	%ymm8,%ymm7,%ymm7		# 0:1
	vpaddq		%ymm3,%ymm13,%ymm2		# h2 = d2 + h3*r4
	vpaddq		%ymm4,%ymm14,%ymm3		# h3 = d3 + h4*r4
	 vpunpcklqdq	%ymm10,%ymm9,%ymm10		# 2:3
	vpmuludq	80(%rax),%ymm0,%ymm4	# h0*r4
	vpmuludq	%ymm1,%ymm5,%ymm0		# h1*s4
	vmovdqa		64(%rcx),%ymm5		# .Lmask26
	vpaddq		%ymm4,%ymm15,%ymm4		# h4 = d4 + h0*r4
	vpaddq		%ymm0,%ymm11,%ymm0		# h0 = d0 + h1*s4

	################################################################
	# lazy reduction (interleaved with tail of input splat)

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm12,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm4,%ymm15
	vpand		%ymm5,%ymm4,%ymm4

	 vpsrlq		$4,%ymm10,%ymm9

	vpsrlq		$26,%ymm1,%ymm12
	vpand		%ymm5,%ymm1,%ymm1
	vpaddq		%ymm12,%ymm2,%ymm2		# h1 -> h2

	vpaddq		%ymm15,%ymm0,%ymm0
	vpsllq		$2,%ymm15,%ymm15
	vpaddq		%ymm15,%ymm0,%ymm0		# h4 -> h0

	 vpand		%ymm5,%ymm9,%ymm9		# 2
	 vpsrlq		$26,%ymm7,%ymm8

	vpsrlq		$26,%ymm2,%ymm13
	vpand		%ymm5,%ymm2,%ymm2
	vpaddq		%ymm13,%ymm3,%ymm3		# h2 -> h3

	 vpaddq		%ymm9,%ymm2,%ymm2		# modulo-scheduled
	 vpsrlq		$30,%ymm10,%ymm10

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	 vpsrlq		$40,%ymm6,%ymm6		# 4

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	 vpand		%ymm5,%ymm7,%ymm7		# 0
	 vpand		%ymm5,%ymm8,%ymm8		# 1
	 vpand		%ymm5,%ymm10,%ymm10		# 3
	 vpor		32(%rcx),%ymm6,%ymm6	# padbit, yes, always

	sub		$64,%rdx
	jnz		.Loop_avx2

	.byte		0x66,0x90
.Ltail_avx2:
	################################################################
	# while above multiplications were by r^4 in all lanes, in last
	# iteration we multiply least significant lane by r^4 and most
	# significant one by r, so copy of above except that references
	# to the precomputed table are displaced by 4...

	#vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	vpaddq		%ymm0,%ymm7,%ymm0
	vmovdqu		4(%rsp),%ymm7	# r0^4
	vpaddq		%ymm1,%ymm8,%ymm1
	vmovdqu		36(%rsp),%ymm8	# r1^4
	vpaddq		%ymm3,%ymm10,%ymm3
	vmovdqu		100(%rsp),%ymm9	# r2^4
	vpaddq		%ymm4,%ymm6,%ymm4
	vmovdqu		52(%rax),%ymm10	# s3^4
	vmovdqu		116(%rax),%ymm5	# s4^4

	vpmuludq	%ymm2,%ymm7,%ymm13		# d2 = h2*r0
	vpmuludq	%ymm2,%ymm8,%ymm14		# d3 = h2*r1
	vpmuludq	%ymm2,%ymm9,%ymm15		# d4 = h2*r2
	vpmuludq	%ymm2,%ymm10,%ymm11		# d0 = h2*s3
	vpmuludq	%ymm2,%ymm5,%ymm12		# d1 = h2*s4

	vpmuludq	%ymm0,%ymm8,%ymm6		# h0*r1
	vpmuludq	%ymm1,%ymm8,%ymm2		# h1*r1
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h0*r1
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h1*r1
	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*r1
	vpmuludq	68(%rsp),%ymm4,%ymm2	# h4*s1
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h3*r1
	vpaddq		%ymm2,%ymm11,%ymm11		# d0 += h4*s1

	vpmuludq	%ymm0,%ymm7,%ymm6		# h0*r0
	vpmuludq	%ymm1,%ymm7,%ymm2		# h1*r0
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h0*r0
	 vmovdqu	-12(%rax),%ymm8	# s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h1*r0
	vpmuludq	%ymm3,%ymm7,%ymm6		# h3*r0
	vpmuludq	%ymm4,%ymm7,%ymm2		# h4*r0
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h3*r0
	vpaddq		%ymm2,%ymm15,%ymm15		# d4 += h4*r0

	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*s2
	vpmuludq	%ymm4,%ymm8,%ymm2		# h4*s2
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h3*s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h4*s2
	 vmovdqu	20(%rax),%ymm2	# r3
	vpmuludq	%ymm1,%ymm9,%ymm6		# h1*r2
	vpmuludq	%ymm0,%ymm9,%ymm9		# h0*r2
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h1*r2
	vpaddq		%ymm9,%ymm13,%ymm13		# d2 += h0*r2

	vpmuludq	%ymm1,%ymm2,%ymm6		# h1*r3
	vpmuludq	%ymm0,%ymm2,%ymm2		# h0*r3
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h1*r3
	vpaddq		%ymm2,%ymm14,%ymm14		# d3 += h0*r3
	vpmuludq	%ymm3,%ymm10,%ymm6		# h3*s3
	vpmuludq	%ymm4,%ymm10,%ymm2		# h4*s3
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h3*s3
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h4*s3

	vpmuludq	%ymm3,%ymm5,%ymm3		# h3*s4
	vpmuludq	%ymm4,%ymm5,%ymm4		# h4*s4
	vpaddq		%ymm3,%ymm13,%ymm2		# h2 = d2 + h3*r4
	vpaddq		%ymm4,%ymm14,%ymm3		# h3 = d3 + h4*r4
	vpmuludq	84(%rax),%ymm0,%ymm4		# h0*r4
	vpmuludq	%ymm1,%ymm5,%ymm0		# h1*s4
	vmovdqa		64(%rcx),%ymm5		# .Lmask26
	vpaddq		%ymm4,%ymm15,%ymm4		# h4 = d4 + h0*r4
	vpaddq		%ymm0,%ymm11,%ymm0		# h0 = d0 + h1*s4

	################################################################
	# horizontal addition

	vpsrldq		$8,%ymm12,%ymm8
	vpsrldq		$8,%ymm2,%ymm9
	vpsrldq		$8,%ymm3,%ymm10
	vpsrldq		$8,%ymm4,%ymm6
	vpsrldq		$8,%ymm0,%ymm7
	vpaddq		%ymm8,%ymm12,%ymm12
	vpaddq		%ymm9,%ymm2,%ymm2
	vpaddq		%ymm10,%ymm3,%ymm3
	vpaddq		%ymm6,%ymm4,%ymm4
	vpaddq		%ymm7,%ymm0,%ymm0

	vpermq		$0x2,%ymm3,%ymm10
	vpermq		$0x2,%ymm4,%ymm6
	vpermq		$0x2,%ymm0,%ymm7
	vpermq		$0x2,%ymm12,%ymm8
	vpermq		$0x2,%ymm2,%ymm9
	vpaddq		%ymm10,%ymm3,%ymm3
	vpaddq		%ymm6,%ymm4,%ymm4
	vpaddq		%ymm7,%ymm0,%ymm0
	vpaddq		%ymm8,%ymm12,%ymm12
	vpaddq		%ymm9,%ymm2,%ymm2

	################################################################
	# lazy reduction

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm12,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm4,%ymm15
	vpand		%ymm5,%ymm4,%ymm4

	vpsrlq		$26,%ymm1,%ymm12
	vpand		%ymm5,%ymm1,%ymm1
	vpaddq		%ymm12,%ymm2,%ymm2		# h1 -> h2

	vpaddq		%ymm15,%ymm0,%ymm0
	vpsllq		$2,%ymm15,%ymm15
	vpaddq		%ymm15,%ymm0,%ymm0		# h4 -> h0

	vpsrlq		$26,%ymm2,%ymm13
	vpand		%ymm5,%ymm2,%ymm2
	vpaddq		%ymm13,%ymm3,%ymm3		# h2 -> h3

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vmovd		%xmm0,-112(%rdi)# save partially reduced
	vmovd		%xmm1,-108(%rdi)
	vmovd		%xmm2,-104(%rdi)
	vmovd		%xmm3,-100(%rdi)
	vmovd		%xmm4,-96(%rdi)
	lea		-8(%r10),%rsp
	vzeroupper
	ret
SYM_FUNC_END(poly1305_blocks_avx2)
#endif
#ifdef CONFIG_AS_AVX512
.align 32
SYM_FUNC_START(poly1305_blocks_avx512)
.Lpoly1305_blocks_avx512:
	mov	20(%rdi),%r8d		# is_base2_26
	cmp	$128,%rdx
	jae	.Lblocks_avx2_avx512
	test	%r8d,%r8d
	jz	.Lblocks

.Lblocks_avx2_avx512:
	and	$-16,%rdx
	jz	.Lno_data_avx2_avx512

	vzeroupper

	test	%r8d,%r8d
	jz	.Lbase2_64_avx2_avx512

	test	$63,%rdx
	jz	.Leven_avx2_avx512

	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_avx2_body_avx512:

	mov	%rdx,%r15		# reassign %rdx

	mov	0(%rdi),%r8		# load hash value
	mov	8(%rdi),%r9
	mov	16(%rdi),%r10d

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	################################# base 2^26 -> base 2^64
	mov	%r8d,%r14d
	and	$-2147483648,%r8
	mov	%r9,%r12			# borrow %r12
	mov	%r9d,%ebx
	and	$-2147483648,%r9

	shr	$6,%r8
	shl	$52,%r12
	add	%r8,%r14
	shr	$12,%rbx
	shr	$18,%r9
	add	%r12,%r14
	adc	%r9,%rbx

	mov	%r10,%r8
	shl	$40,%r8
	shr	$24,%r10
	add	%r8,%rbx
	adc	$0,%r10			# can be partially reduced...

	mov	$-4,%r9		# ... so reduce
	mov	%r10,%r8
	and	%r10,%r9
	shr	$2,%r8
	and	$3,%r10
	add	%r9,%r8			# =*5
	add	%r8,%r14
	adc	$0,%rbx
	adc	$0,%r10

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

.Lbase2_26_pre_avx2_avx512:
	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	sub	$16,%r15

	call	__poly1305_block
	mov	%r12,%rax

	test	$63,%r15
	jnz	.Lbase2_26_pre_avx2_avx512

	test	%rcx,%rcx		# if %rcx is zero,
	jz	.Lstore_base2_64_avx2_avx512	# store hash in base 2^64 format

	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r11
	mov	%rbx,%r12
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r11
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r11,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r12
	and	$0x3ffffff,%rbx		# h[3]
	or	%r12,%r10			# h[4]

	test	%r15,%r15
	jz	.Lstore_base2_26_avx2_avx512

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	jmp	.Lproceed_avx2_avx512

.align	32
.Lstore_base2_64_avx2_avx512:
	mov	%r14,0(%rdi)
	mov	%rbx,8(%rdi)
	mov	%r10,16(%rdi)		# note that is_base2_26 is zeroed
	jmp	.Ldone_avx2_avx512

.align	16
.Lstore_base2_26_avx2_avx512:
	mov	%eax,0(%rdi)		# store hash value base 2^26
	mov	%edx,4(%rdi)
	mov	%r14d,8(%rdi)
	mov	%ebx,12(%rdi)
	mov	%r10d,16(%rdi)
.align	16
.Ldone_avx2_avx512:
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lno_data_avx2_avx512:
.Lblocks_avx2_epilogue_avx512:
	ret

.align	32
.Lbase2_64_avx2_avx512:
	push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lbase2_64_avx2_body_avx512:

	mov	%rdx,%r15		# reassign %rdx

	mov	24(%rdi),%r11		# load r
	mov	32(%rdi),%r13

	mov	0(%rdi),%r14		# load hash value
	mov	8(%rdi),%rbx
	mov	16(%rdi),%r10d

	mov	%r13,%r12
	mov	%r13,%rax
	shr	$2,%r13
	add	%r12,%r13			# s1 = r1 + (r1 >> 2)

	test	$63,%rdx
	jz	.Linit_avx2_avx512

.Lbase2_64_pre_avx2_avx512:
	add	0(%rsi),%r14		# accumulate input
	adc	8(%rsi),%rbx
	lea	16(%rsi),%rsi
	adc	%rcx,%r10
	sub	$16,%r15

	call	__poly1305_block
	mov	%r12,%rax

	test	$63,%r15
	jnz	.Lbase2_64_pre_avx2_avx512

.Linit_avx2_avx512:
	################################# base 2^64 -> base 2^26
	mov	%r14,%rax
	mov	%r14,%rdx
	shr	$52,%r14
	mov	%rbx,%r8
	mov	%rbx,%r9
	shr	$26,%rdx
	and	$0x3ffffff,%rax	# h[0]
	shl	$12,%r8
	and	$0x3ffffff,%rdx	# h[1]
	shr	$14,%rbx
	or	%r8,%r14
	shl	$24,%r10
	and	$0x3ffffff,%r14		# h[2]
	shr	$40,%r9
	and	$0x3ffffff,%rbx		# h[3]
	or	%r9,%r10			# h[4]

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%r10d,%xmm4
	movl	$1,20(%rdi)		# set is_base2_26

	call	__poly1305_init_avx

.Lproceed_avx2_avx512:
	mov	%r15,%rdx			# restore %rdx
	pop 		%r15
	pop 		%r14
	pop 		%r13
	pop 		%r12
	pop 		%rbx
	pop 		%rbp
.Lbase2_64_avx2_epilogue_avx512:
	jmp	.Ldo_avx2_avx512

.align	32
.Leven_avx2_avx512:
	vmovd		4*0(%rdi),%xmm0	# load hash value base 2^26
	vmovd		4*1(%rdi),%xmm1
	vmovd		4*2(%rdi),%xmm2
	vmovd		4*3(%rdi),%xmm3
	vmovd		4*4(%rdi),%xmm4

.Ldo_avx2_avx512:
	cmp		$512,%rdx
	jae		.Lblocks_avx512
	lea		8(%rsp),%r10
	sub		$0x128,%rsp
	lea		.Lconst(%rip),%rcx
	lea		48+64(%rdi),%rdi	# size optimization
	vmovdqa		96(%rcx),%ymm7		# .Lpermd_avx2

	# expand and copy pre-calculated table to stack
	vmovdqu		-64(%rdi),%xmm9
	and		$-512,%rsp
	vmovdqu		-48(%rdi),%xmm10
	vmovdqu		-32(%rdi),%xmm6
	vmovdqu		-16(%rdi),%xmm11
	vmovdqu		0(%rdi),%xmm12
	vmovdqu		16(%rdi),%xmm13
	lea		0x90(%rsp),%rax		# size optimization
	vmovdqu		32(%rdi),%xmm14
	vpermd		%ymm9,%ymm7,%ymm9		# 00003412 -> 14243444
	vmovdqu		48(%rdi),%xmm15
	vpermd		%ymm10,%ymm7,%ymm10
	vmovdqu		64(%rdi),%xmm5
	vpermd		%ymm6,%ymm7,%ymm6
	vmovdqa		%ymm9,0x00(%rsp)
	vpermd		%ymm11,%ymm7,%ymm11
	vmovdqa		%ymm10,0x20-0x90(%rax)
	vpermd		%ymm12,%ymm7,%ymm12
	vmovdqa		%ymm6,0x40-0x90(%rax)
	vpermd		%ymm13,%ymm7,%ymm13
	vmovdqa		%ymm11,0x60-0x90(%rax)
	vpermd		%ymm14,%ymm7,%ymm14
	vmovdqa		%ymm12,0x80-0x90(%rax)
	vpermd		%ymm15,%ymm7,%ymm15
	vmovdqa		%ymm13,0xa0-0x90(%rax)
	vpermd		%ymm5,%ymm7,%ymm5
	vmovdqa		%ymm14,0xc0-0x90(%rax)
	vmovdqa		%ymm15,0xe0-0x90(%rax)
	vmovdqa		%ymm5,0x100-0x90(%rax)
	vmovdqa		64(%rcx),%ymm5		# .Lmask26

	################################################################
	# load input
	vmovdqu		16*0(%rsi),%xmm7
	vmovdqu		16*1(%rsi),%xmm8
	vinserti128	$1,16*2(%rsi),%ymm7,%ymm7
	vinserti128	$1,16*3(%rsi),%ymm8,%ymm8
	lea		16*4(%rsi),%rsi

	vpsrldq		$6,%ymm7,%ymm9		# splat input
	vpsrldq		$6,%ymm8,%ymm10
	vpunpckhqdq	%ymm8,%ymm7,%ymm6		# 4
	vpunpcklqdq	%ymm10,%ymm9,%ymm9		# 2:3
	vpunpcklqdq	%ymm8,%ymm7,%ymm7		# 0:1

	vpsrlq		$30,%ymm9,%ymm10
	vpsrlq		$4,%ymm9,%ymm9
	vpsrlq		$26,%ymm7,%ymm8
	vpsrlq		$40,%ymm6,%ymm6		# 4
	vpand		%ymm5,%ymm9,%ymm9		# 2
	vpand		%ymm5,%ymm7,%ymm7		# 0
	vpand		%ymm5,%ymm8,%ymm8		# 1
	vpand		%ymm5,%ymm10,%ymm10		# 3
	vpor		32(%rcx),%ymm6,%ymm6	# padbit, yes, always

	vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	sub		$64,%rdx
	jz		.Ltail_avx2_avx512
	jmp		.Loop_avx2_avx512

.align	32
.Loop_avx2_avx512:
	################################################################
	# ((inp[0]*r^4+inp[4])*r^4+inp[ 8])*r^4
	# ((inp[1]*r^4+inp[5])*r^4+inp[ 9])*r^3
	# ((inp[2]*r^4+inp[6])*r^4+inp[10])*r^2
	# ((inp[3]*r^4+inp[7])*r^4+inp[11])*r^1
	#   ________/__________/
	################################################################
	#vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	vpaddq		%ymm0,%ymm7,%ymm0
	vmovdqa		0(%rsp),%ymm7	# r0^4
	vpaddq		%ymm1,%ymm8,%ymm1
	vmovdqa		32(%rsp),%ymm8	# r1^4
	vpaddq		%ymm3,%ymm10,%ymm3
	vmovdqa		96(%rsp),%ymm9	# r2^4
	vpaddq		%ymm4,%ymm6,%ymm4
	vmovdqa		48(%rax),%ymm10	# s3^4
	vmovdqa		112(%rax),%ymm5	# s4^4

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# however, as h2 is "chronologically" first one available pull
	# corresponding operations up, so it's
	#
	# d4 = h2*r2   + h4*r0 + h3*r1             + h1*r3   + h0*r4
	# d3 = h2*r1   + h3*r0           + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0           + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h2*5*r4 + h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3
	# d0 = h2*5*r3 + h0*r0 + h4*5*r1 + h3*5*r2           + h1*5*r4

	vpmuludq	%ymm2,%ymm7,%ymm13		# d2 = h2*r0
	vpmuludq	%ymm2,%ymm8,%ymm14		# d3 = h2*r1
	vpmuludq	%ymm2,%ymm9,%ymm15		# d4 = h2*r2
	vpmuludq	%ymm2,%ymm10,%ymm11		# d0 = h2*s3
	vpmuludq	%ymm2,%ymm5,%ymm12		# d1 = h2*s4

	vpmuludq	%ymm0,%ymm8,%ymm6		# h0*r1
	vpmuludq	%ymm1,%ymm8,%ymm2		# h1*r1, borrow %ymm2 as temp
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h0*r1
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h1*r1
	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*r1
	vpmuludq	64(%rsp),%ymm4,%ymm2	# h4*s1
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h3*r1
	vpaddq		%ymm2,%ymm11,%ymm11		# d0 += h4*s1
	 vmovdqa	-16(%rax),%ymm8	# s2

	vpmuludq	%ymm0,%ymm7,%ymm6		# h0*r0
	vpmuludq	%ymm1,%ymm7,%ymm2		# h1*r0
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h0*r0
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h1*r0
	vpmuludq	%ymm3,%ymm7,%ymm6		# h3*r0
	vpmuludq	%ymm4,%ymm7,%ymm2		# h4*r0
	 vmovdqu	16*0(%rsi),%xmm7	# load input
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h3*r0
	vpaddq		%ymm2,%ymm15,%ymm15		# d4 += h4*r0
	 vinserti128	$1,16*2(%rsi),%ymm7,%ymm7

	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*s2
	vpmuludq	%ymm4,%ymm8,%ymm2		# h4*s2
	 vmovdqu	16*1(%rsi),%xmm8
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h3*s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h4*s2
	 vmovdqa	16(%rax),%ymm2	# r3
	vpmuludq	%ymm1,%ymm9,%ymm6		# h1*r2
	vpmuludq	%ymm0,%ymm9,%ymm9		# h0*r2
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h1*r2
	vpaddq		%ymm9,%ymm13,%ymm13		# d2 += h0*r2
	 vinserti128	$1,16*3(%rsi),%ymm8,%ymm8
	 lea		16*4(%rsi),%rsi

	vpmuludq	%ymm1,%ymm2,%ymm6		# h1*r3
	vpmuludq	%ymm0,%ymm2,%ymm2		# h0*r3
	 vpsrldq	$6,%ymm7,%ymm9		# splat input
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h1*r3
	vpaddq		%ymm2,%ymm14,%ymm14		# d3 += h0*r3
	vpmuludq	%ymm3,%ymm10,%ymm6		# h3*s3
	vpmuludq	%ymm4,%ymm10,%ymm2		# h4*s3
	 vpsrldq	$6,%ymm8,%ymm10
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h3*s3
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h4*s3
	 vpunpckhqdq	%ymm8,%ymm7,%ymm6		# 4

	vpmuludq	%ymm3,%ymm5,%ymm3		# h3*s4
	vpmuludq	%ymm4,%ymm5,%ymm4		# h4*s4
	 vpunpcklqdq	%ymm8,%ymm7,%ymm7		# 0:1
	vpaddq		%ymm3,%ymm13,%ymm2		# h2 = d2 + h3*r4
	vpaddq		%ymm4,%ymm14,%ymm3		# h3 = d3 + h4*r4
	 vpunpcklqdq	%ymm10,%ymm9,%ymm10		# 2:3
	vpmuludq	80(%rax),%ymm0,%ymm4	# h0*r4
	vpmuludq	%ymm1,%ymm5,%ymm0		# h1*s4
	vmovdqa		64(%rcx),%ymm5		# .Lmask26
	vpaddq		%ymm4,%ymm15,%ymm4		# h4 = d4 + h0*r4
	vpaddq		%ymm0,%ymm11,%ymm0		# h0 = d0 + h1*s4

	################################################################
	# lazy reduction (interleaved with tail of input splat)

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm12,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm4,%ymm15
	vpand		%ymm5,%ymm4,%ymm4

	 vpsrlq		$4,%ymm10,%ymm9

	vpsrlq		$26,%ymm1,%ymm12
	vpand		%ymm5,%ymm1,%ymm1
	vpaddq		%ymm12,%ymm2,%ymm2		# h1 -> h2

	vpaddq		%ymm15,%ymm0,%ymm0
	vpsllq		$2,%ymm15,%ymm15
	vpaddq		%ymm15,%ymm0,%ymm0		# h4 -> h0

	 vpand		%ymm5,%ymm9,%ymm9		# 2
	 vpsrlq		$26,%ymm7,%ymm8

	vpsrlq		$26,%ymm2,%ymm13
	vpand		%ymm5,%ymm2,%ymm2
	vpaddq		%ymm13,%ymm3,%ymm3		# h2 -> h3

	 vpaddq		%ymm9,%ymm2,%ymm2		# modulo-scheduled
	 vpsrlq		$30,%ymm10,%ymm10

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	 vpsrlq		$40,%ymm6,%ymm6		# 4

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	 vpand		%ymm5,%ymm7,%ymm7		# 0
	 vpand		%ymm5,%ymm8,%ymm8		# 1
	 vpand		%ymm5,%ymm10,%ymm10		# 3
	 vpor		32(%rcx),%ymm6,%ymm6	# padbit, yes, always

	sub		$64,%rdx
	jnz		.Loop_avx2_avx512

	.byte		0x66,0x90
.Ltail_avx2_avx512:
	################################################################
	# while above multiplications were by r^4 in all lanes, in last
	# iteration we multiply least significant lane by r^4 and most
	# significant one by r, so copy of above except that references
	# to the precomputed table are displaced by 4...

	#vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input
	vpaddq		%ymm0,%ymm7,%ymm0
	vmovdqu		4(%rsp),%ymm7	# r0^4
	vpaddq		%ymm1,%ymm8,%ymm1
	vmovdqu		36(%rsp),%ymm8	# r1^4
	vpaddq		%ymm3,%ymm10,%ymm3
	vmovdqu		100(%rsp),%ymm9	# r2^4
	vpaddq		%ymm4,%ymm6,%ymm4
	vmovdqu		52(%rax),%ymm10	# s3^4
	vmovdqu		116(%rax),%ymm5	# s4^4

	vpmuludq	%ymm2,%ymm7,%ymm13		# d2 = h2*r0
	vpmuludq	%ymm2,%ymm8,%ymm14		# d3 = h2*r1
	vpmuludq	%ymm2,%ymm9,%ymm15		# d4 = h2*r2
	vpmuludq	%ymm2,%ymm10,%ymm11		# d0 = h2*s3
	vpmuludq	%ymm2,%ymm5,%ymm12		# d1 = h2*s4

	vpmuludq	%ymm0,%ymm8,%ymm6		# h0*r1
	vpmuludq	%ymm1,%ymm8,%ymm2		# h1*r1
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h0*r1
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h1*r1
	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*r1
	vpmuludq	68(%rsp),%ymm4,%ymm2	# h4*s1
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h3*r1
	vpaddq		%ymm2,%ymm11,%ymm11		# d0 += h4*s1

	vpmuludq	%ymm0,%ymm7,%ymm6		# h0*r0
	vpmuludq	%ymm1,%ymm7,%ymm2		# h1*r0
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h0*r0
	 vmovdqu	-12(%rax),%ymm8	# s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h1*r0
	vpmuludq	%ymm3,%ymm7,%ymm6		# h3*r0
	vpmuludq	%ymm4,%ymm7,%ymm2		# h4*r0
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h3*r0
	vpaddq		%ymm2,%ymm15,%ymm15		# d4 += h4*r0

	vpmuludq	%ymm3,%ymm8,%ymm6		# h3*s2
	vpmuludq	%ymm4,%ymm8,%ymm2		# h4*s2
	vpaddq		%ymm6,%ymm11,%ymm11		# d0 += h3*s2
	vpaddq		%ymm2,%ymm12,%ymm12		# d1 += h4*s2
	 vmovdqu	20(%rax),%ymm2	# r3
	vpmuludq	%ymm1,%ymm9,%ymm6		# h1*r2
	vpmuludq	%ymm0,%ymm9,%ymm9		# h0*r2
	vpaddq		%ymm6,%ymm14,%ymm14		# d3 += h1*r2
	vpaddq		%ymm9,%ymm13,%ymm13		# d2 += h0*r2

	vpmuludq	%ymm1,%ymm2,%ymm6		# h1*r3
	vpmuludq	%ymm0,%ymm2,%ymm2		# h0*r3
	vpaddq		%ymm6,%ymm15,%ymm15		# d4 += h1*r3
	vpaddq		%ymm2,%ymm14,%ymm14		# d3 += h0*r3
	vpmuludq	%ymm3,%ymm10,%ymm6		# h3*s3
	vpmuludq	%ymm4,%ymm10,%ymm2		# h4*s3
	vpaddq		%ymm6,%ymm12,%ymm12		# d1 += h3*s3
	vpaddq		%ymm2,%ymm13,%ymm13		# d2 += h4*s3

	vpmuludq	%ymm3,%ymm5,%ymm3		# h3*s4
	vpmuludq	%ymm4,%ymm5,%ymm4		# h4*s4
	vpaddq		%ymm3,%ymm13,%ymm2		# h2 = d2 + h3*r4
	vpaddq		%ymm4,%ymm14,%ymm3		# h3 = d3 + h4*r4
	vpmuludq	84(%rax),%ymm0,%ymm4		# h0*r4
	vpmuludq	%ymm1,%ymm5,%ymm0		# h1*s4
	vmovdqa		64(%rcx),%ymm5		# .Lmask26
	vpaddq		%ymm4,%ymm15,%ymm4		# h4 = d4 + h0*r4
	vpaddq		%ymm0,%ymm11,%ymm0		# h0 = d0 + h1*s4

	################################################################
	# horizontal addition

	vpsrldq		$8,%ymm12,%ymm8
	vpsrldq		$8,%ymm2,%ymm9
	vpsrldq		$8,%ymm3,%ymm10
	vpsrldq		$8,%ymm4,%ymm6
	vpsrldq		$8,%ymm0,%ymm7
	vpaddq		%ymm8,%ymm12,%ymm12
	vpaddq		%ymm9,%ymm2,%ymm2
	vpaddq		%ymm10,%ymm3,%ymm3
	vpaddq		%ymm6,%ymm4,%ymm4
	vpaddq		%ymm7,%ymm0,%ymm0

	vpermq		$0x2,%ymm3,%ymm10
	vpermq		$0x2,%ymm4,%ymm6
	vpermq		$0x2,%ymm0,%ymm7
	vpermq		$0x2,%ymm12,%ymm8
	vpermq		$0x2,%ymm2,%ymm9
	vpaddq		%ymm10,%ymm3,%ymm3
	vpaddq		%ymm6,%ymm4,%ymm4
	vpaddq		%ymm7,%ymm0,%ymm0
	vpaddq		%ymm8,%ymm12,%ymm12
	vpaddq		%ymm9,%ymm2,%ymm2

	################################################################
	# lazy reduction

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm12,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm4,%ymm15
	vpand		%ymm5,%ymm4,%ymm4

	vpsrlq		$26,%ymm1,%ymm12
	vpand		%ymm5,%ymm1,%ymm1
	vpaddq		%ymm12,%ymm2,%ymm2		# h1 -> h2

	vpaddq		%ymm15,%ymm0,%ymm0
	vpsllq		$2,%ymm15,%ymm15
	vpaddq		%ymm15,%ymm0,%ymm0		# h4 -> h0

	vpsrlq		$26,%ymm2,%ymm13
	vpand		%ymm5,%ymm2,%ymm2
	vpaddq		%ymm13,%ymm3,%ymm3		# h2 -> h3

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vmovd		%xmm0,-112(%rdi)# save partially reduced
	vmovd		%xmm1,-108(%rdi)
	vmovd		%xmm2,-104(%rdi)
	vmovd		%xmm3,-100(%rdi)
	vmovd		%xmm4,-96(%rdi)
	lea		-8(%r10),%rsp
	vzeroupper
	ret
.Lblocks_avx512:
	mov		$15,%eax
	kmovw		%eax,%k2
	lea		8(%rsp),%r10
	sub		$0x128,%rsp
	lea		.Lconst(%rip),%rcx
	lea		48+64(%rdi),%rdi	# size optimization
	vmovdqa		96(%rcx),%ymm9		# .Lpermd_avx2

	# expand pre-calculated table
	vmovdqu		-64(%rdi),%xmm11	# will become expanded %zmm16
	and		$-512,%rsp
	vmovdqu		-48(%rdi),%xmm12	# will become ... %zmm17
	mov		$0x20,%rax
	vmovdqu		-32(%rdi),%xmm7	# ... %zmm21
	vmovdqu		-16(%rdi),%xmm13	# ... %zmm18
	vmovdqu		0(%rdi),%xmm8	# ... %zmm22
	vmovdqu		16(%rdi),%xmm14	# ... %zmm19
	vmovdqu		32(%rdi),%xmm10	# ... %zmm23
	vmovdqu		48(%rdi),%xmm15	# ... %zmm20
	vmovdqu		64(%rdi),%xmm6	# ... %zmm24
	vpermd		%zmm11,%zmm9,%zmm16		# 00003412 -> 14243444
	vpbroadcastq	64(%rcx),%zmm5		# .Lmask26
	vpermd		%zmm12,%zmm9,%zmm17
	vpermd		%zmm7,%zmm9,%zmm21
	vpermd		%zmm13,%zmm9,%zmm18
	vmovdqa64	%zmm16,0x00(%rsp){%k2}	# save in case %rdx%128 != 0
	 vpsrlq		$32,%zmm16,%zmm7		# 14243444 -> 01020304
	vpermd		%zmm8,%zmm9,%zmm22
	vmovdqu64	%zmm17,0x00(%rsp,%rax){%k2}
	 vpsrlq		$32,%zmm17,%zmm8
	vpermd		%zmm14,%zmm9,%zmm19
	vmovdqa64	%zmm21,0x40(%rsp){%k2}
	vpermd		%zmm10,%zmm9,%zmm23
	vpermd		%zmm15,%zmm9,%zmm20
	vmovdqu64	%zmm18,0x40(%rsp,%rax){%k2}
	vpermd		%zmm6,%zmm9,%zmm24
	vmovdqa64	%zmm22,0x80(%rsp){%k2}
	vmovdqu64	%zmm19,0x80(%rsp,%rax){%k2}
	vmovdqa64	%zmm23,0xc0(%rsp){%k2}
	vmovdqu64	%zmm20,0xc0(%rsp,%rax){%k2}
	vmovdqa64	%zmm24,0x100(%rsp){%k2}

	################################################################
	# calculate 5th through 8th powers of the key
	#
	# d0 = r0'*r0 + r1'*5*r4 + r2'*5*r3 + r3'*5*r2 + r4'*5*r1
	# d1 = r0'*r1 + r1'*r0   + r2'*5*r4 + r3'*5*r3 + r4'*5*r2
	# d2 = r0'*r2 + r1'*r1   + r2'*r0   + r3'*5*r4 + r4'*5*r3
	# d3 = r0'*r3 + r1'*r2   + r2'*r1   + r3'*r0   + r4'*5*r4
	# d4 = r0'*r4 + r1'*r3   + r2'*r2   + r3'*r1   + r4'*r0

	vpmuludq	%zmm7,%zmm16,%zmm11		# d0 = r0'*r0
	vpmuludq	%zmm7,%zmm17,%zmm12		# d1 = r0'*r1
	vpmuludq	%zmm7,%zmm18,%zmm13		# d2 = r0'*r2
	vpmuludq	%zmm7,%zmm19,%zmm14		# d3 = r0'*r3
	vpmuludq	%zmm7,%zmm20,%zmm15		# d4 = r0'*r4
	 vpsrlq		$32,%zmm18,%zmm9

	vpmuludq	%zmm8,%zmm24,%zmm25
	vpmuludq	%zmm8,%zmm16,%zmm26
	vpmuludq	%zmm8,%zmm17,%zmm27
	vpmuludq	%zmm8,%zmm18,%zmm28
	vpmuludq	%zmm8,%zmm19,%zmm29
	 vpsrlq		$32,%zmm19,%zmm10
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += r1'*5*r4
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += r1'*r0
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += r1'*r1
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += r1'*r2
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += r1'*r3

	vpmuludq	%zmm9,%zmm23,%zmm25
	vpmuludq	%zmm9,%zmm24,%zmm26
	vpmuludq	%zmm9,%zmm17,%zmm28
	vpmuludq	%zmm9,%zmm18,%zmm29
	vpmuludq	%zmm9,%zmm16,%zmm27
	 vpsrlq		$32,%zmm20,%zmm6
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += r2'*5*r3
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += r2'*5*r4
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += r2'*r1
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += r2'*r2
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += r2'*r0

	vpmuludq	%zmm10,%zmm22,%zmm25
	vpmuludq	%zmm10,%zmm16,%zmm28
	vpmuludq	%zmm10,%zmm17,%zmm29
	vpmuludq	%zmm10,%zmm23,%zmm26
	vpmuludq	%zmm10,%zmm24,%zmm27
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += r3'*5*r2
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += r3'*r0
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += r3'*r1
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += r3'*5*r3
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += r3'*5*r4

	vpmuludq	%zmm6,%zmm24,%zmm28
	vpmuludq	%zmm6,%zmm16,%zmm29
	vpmuludq	%zmm6,%zmm21,%zmm25
	vpmuludq	%zmm6,%zmm22,%zmm26
	vpmuludq	%zmm6,%zmm23,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += r2'*5*r4
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += r2'*r0
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += r2'*5*r1
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += r2'*5*r2
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += r2'*5*r3

	################################################################
	# load input
	vmovdqu64	16*0(%rsi),%zmm10
	vmovdqu64	16*4(%rsi),%zmm6
	lea		16*8(%rsi),%rsi

	################################################################
	# lazy reduction

	vpsrlq		$26,%zmm14,%zmm28
	vpandq		%zmm5,%zmm14,%zmm14
	vpaddq		%zmm28,%zmm15,%zmm15		# d3 -> d4

	vpsrlq		$26,%zmm11,%zmm25
	vpandq		%zmm5,%zmm11,%zmm11
	vpaddq		%zmm25,%zmm12,%zmm12		# d0 -> d1

	vpsrlq		$26,%zmm15,%zmm29
	vpandq		%zmm5,%zmm15,%zmm15

	vpsrlq		$26,%zmm12,%zmm26
	vpandq		%zmm5,%zmm12,%zmm12
	vpaddq		%zmm26,%zmm13,%zmm13		# d1 -> d2

	vpaddq		%zmm29,%zmm11,%zmm11
	vpsllq		$2,%zmm29,%zmm29
	vpaddq		%zmm29,%zmm11,%zmm11		# d4 -> d0

	vpsrlq		$26,%zmm13,%zmm27
	vpandq		%zmm5,%zmm13,%zmm13
	vpaddq		%zmm27,%zmm14,%zmm14		# d2 -> d3

	vpsrlq		$26,%zmm11,%zmm25
	vpandq		%zmm5,%zmm11,%zmm11
	vpaddq		%zmm25,%zmm12,%zmm12		# d0 -> d1

	vpsrlq		$26,%zmm14,%zmm28
	vpandq		%zmm5,%zmm14,%zmm14
	vpaddq		%zmm28,%zmm15,%zmm15		# d3 -> d4

	################################################################
	# at this point we have 14243444 in %zmm16-%zmm24 and 05060708 in
	# %zmm11-%zmm15, ...

	vpunpcklqdq	%zmm6,%zmm10,%zmm7	# transpose input
	vpunpckhqdq	%zmm6,%zmm10,%zmm6

	# ... since input 64-bit lanes are ordered as 73625140, we could
	# "vperm" it to 76543210 (here and in each loop iteration), *or*
	# we could just flow along, hence the goal for %zmm16-%zmm24 is
	# 1858286838784888 ...

	vmovdqa32	128(%rcx),%zmm25		# .Lpermd_avx512:
	mov		$0x7777,%eax
	kmovw		%eax,%k1

	vpermd		%zmm16,%zmm25,%zmm16		# 14243444 -> 1---2---3---4---
	vpermd		%zmm17,%zmm25,%zmm17
	vpermd		%zmm18,%zmm25,%zmm18
	vpermd		%zmm19,%zmm25,%zmm19
	vpermd		%zmm20,%zmm25,%zmm20

	vpermd		%zmm11,%zmm25,%zmm16{%k1}	# 05060708 -> 1858286838784888
	vpermd		%zmm12,%zmm25,%zmm17{%k1}
	vpermd		%zmm13,%zmm25,%zmm18{%k1}
	vpermd		%zmm14,%zmm25,%zmm19{%k1}
	vpermd		%zmm15,%zmm25,%zmm20{%k1}

	vpslld		$2,%zmm17,%zmm21		# *5
	vpslld		$2,%zmm18,%zmm22
	vpslld		$2,%zmm19,%zmm23
	vpslld		$2,%zmm20,%zmm24
	vpaddd		%zmm17,%zmm21,%zmm21
	vpaddd		%zmm18,%zmm22,%zmm22
	vpaddd		%zmm19,%zmm23,%zmm23
	vpaddd		%zmm20,%zmm24,%zmm24

	vpbroadcastq	32(%rcx),%zmm30	# .L129

	vpsrlq		$52,%zmm7,%zmm9		# splat input
	vpsllq		$12,%zmm6,%zmm10
	vporq		%zmm10,%zmm9,%zmm9
	vpsrlq		$26,%zmm7,%zmm8
	vpsrlq		$14,%zmm6,%zmm10
	vpsrlq		$40,%zmm6,%zmm6		# 4
	vpandq		%zmm5,%zmm9,%zmm9		# 2
	vpandq		%zmm5,%zmm7,%zmm7		# 0
	#vpandq		%zmm5,%zmm8,%zmm8		# 1
	#vpandq		%zmm5,%zmm10,%zmm10		# 3
	#vporq		%zmm30,%zmm6,%zmm6		# padbit, yes, always

	vpaddq		%zmm2,%zmm9,%zmm2		# accumulate input
	sub		$192,%rdx
	jbe		.Ltail_avx512
	jmp		.Loop_avx512

.align	32
.Loop_avx512:
	################################################################
	# ((inp[0]*r^8+inp[ 8])*r^8+inp[16])*r^8
	# ((inp[1]*r^8+inp[ 9])*r^8+inp[17])*r^7
	# ((inp[2]*r^8+inp[10])*r^8+inp[18])*r^6
	# ((inp[3]*r^8+inp[11])*r^8+inp[19])*r^5
	# ((inp[4]*r^8+inp[12])*r^8+inp[20])*r^4
	# ((inp[5]*r^8+inp[13])*r^8+inp[21])*r^3
	# ((inp[6]*r^8+inp[14])*r^8+inp[22])*r^2
	# ((inp[7]*r^8+inp[15])*r^8+inp[23])*r^1
	#   ________/___________/
	################################################################
	#vpaddq		%zmm2,%zmm9,%zmm2		# accumulate input

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# however, as h2 is "chronologically" first one available pull
	# corresponding operations up, so it's
	#
	# d3 = h2*r1   + h0*r3 + h1*r2   + h3*r0 + h4*5*r4
	# d4 = h2*r2   + h0*r4 + h1*r3   + h3*r1 + h4*r0
	# d0 = h2*5*r3 + h0*r0 + h1*5*r4         + h3*5*r2 + h4*5*r1
	# d1 = h2*5*r4 + h0*r1           + h1*r0 + h3*5*r3 + h4*5*r2
	# d2 = h2*r0           + h0*r2   + h1*r1 + h3*5*r4 + h4*5*r3

	vpmuludq	%zmm2,%zmm17,%zmm14		# d3 = h2*r1
	 vpaddq		%zmm0,%zmm7,%zmm0
	vpmuludq	%zmm2,%zmm18,%zmm15		# d4 = h2*r2
	 vpandq		%zmm5,%zmm8,%zmm8		# 1
	vpmuludq	%zmm2,%zmm23,%zmm11		# d0 = h2*s3
	 vpandq		%zmm5,%zmm10,%zmm10		# 3
	vpmuludq	%zmm2,%zmm24,%zmm12		# d1 = h2*s4
	 vporq		%zmm30,%zmm6,%zmm6		# padbit, yes, always
	vpmuludq	%zmm2,%zmm16,%zmm13		# d2 = h2*r0
	 vpaddq		%zmm1,%zmm8,%zmm1		# accumulate input
	 vpaddq		%zmm3,%zmm10,%zmm3
	 vpaddq		%zmm4,%zmm6,%zmm4

	  vmovdqu64	16*0(%rsi),%zmm10		# load input
	  vmovdqu64	16*4(%rsi),%zmm6
	  lea		16*8(%rsi),%rsi
	vpmuludq	%zmm0,%zmm19,%zmm28
	vpmuludq	%zmm0,%zmm20,%zmm29
	vpmuludq	%zmm0,%zmm16,%zmm25
	vpmuludq	%zmm0,%zmm17,%zmm26
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h0*r3
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h0*r4
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h0*r0
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h0*r1

	vpmuludq	%zmm1,%zmm18,%zmm28
	vpmuludq	%zmm1,%zmm19,%zmm29
	vpmuludq	%zmm1,%zmm24,%zmm25
	vpmuludq	%zmm0,%zmm18,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h1*r2
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h1*r3
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h1*s4
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h0*r2

	  vpunpcklqdq	%zmm6,%zmm10,%zmm7		# transpose input
	  vpunpckhqdq	%zmm6,%zmm10,%zmm6

	vpmuludq	%zmm3,%zmm16,%zmm28
	vpmuludq	%zmm3,%zmm17,%zmm29
	vpmuludq	%zmm1,%zmm16,%zmm26
	vpmuludq	%zmm1,%zmm17,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h3*r0
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h3*r1
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h1*r0
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h1*r1

	vpmuludq	%zmm4,%zmm24,%zmm28
	vpmuludq	%zmm4,%zmm16,%zmm29
	vpmuludq	%zmm3,%zmm22,%zmm25
	vpmuludq	%zmm3,%zmm23,%zmm26
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h4*s4
	vpmuludq	%zmm3,%zmm24,%zmm27
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h4*r0
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h3*s2
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h3*s3
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h3*s4

	vpmuludq	%zmm4,%zmm21,%zmm25
	vpmuludq	%zmm4,%zmm22,%zmm26
	vpmuludq	%zmm4,%zmm23,%zmm27
	vpaddq		%zmm25,%zmm11,%zmm0		# h0 = d0 + h4*s1
	vpaddq		%zmm26,%zmm12,%zmm1		# h1 = d2 + h4*s2
	vpaddq		%zmm27,%zmm13,%zmm2		# h2 = d3 + h4*s3

	################################################################
	# lazy reduction (interleaved with input splat)

	 vpsrlq		$52,%zmm7,%zmm9		# splat input
	 vpsllq		$12,%zmm6,%zmm10

	vpsrlq		$26,%zmm14,%zmm3
	vpandq		%zmm5,%zmm14,%zmm14
	vpaddq		%zmm3,%zmm15,%zmm4		# h3 -> h4

	 vporq		%zmm10,%zmm9,%zmm9

	vpsrlq		$26,%zmm0,%zmm11
	vpandq		%zmm5,%zmm0,%zmm0
	vpaddq		%zmm11,%zmm1,%zmm1		# h0 -> h1

	 vpandq		%zmm5,%zmm9,%zmm9		# 2

	vpsrlq		$26,%zmm4,%zmm15
	vpandq		%zmm5,%zmm4,%zmm4

	vpsrlq		$26,%zmm1,%zmm12
	vpandq		%zmm5,%zmm1,%zmm1
	vpaddq		%zmm12,%zmm2,%zmm2		# h1 -> h2

	vpaddq		%zmm15,%zmm0,%zmm0
	vpsllq		$2,%zmm15,%zmm15
	vpaddq		%zmm15,%zmm0,%zmm0		# h4 -> h0

	 vpaddq		%zmm9,%zmm2,%zmm2		# modulo-scheduled
	 vpsrlq		$26,%zmm7,%zmm8

	vpsrlq		$26,%zmm2,%zmm13
	vpandq		%zmm5,%zmm2,%zmm2
	vpaddq		%zmm13,%zmm14,%zmm3		# h2 -> h3

	 vpsrlq		$14,%zmm6,%zmm10

	vpsrlq		$26,%zmm0,%zmm11
	vpandq		%zmm5,%zmm0,%zmm0
	vpaddq		%zmm11,%zmm1,%zmm1		# h0 -> h1

	 vpsrlq		$40,%zmm6,%zmm6		# 4

	vpsrlq		$26,%zmm3,%zmm14
	vpandq		%zmm5,%zmm3,%zmm3
	vpaddq		%zmm14,%zmm4,%zmm4		# h3 -> h4

	 vpandq		%zmm5,%zmm7,%zmm7		# 0
	 #vpandq	%zmm5,%zmm8,%zmm8		# 1
	 #vpandq	%zmm5,%zmm10,%zmm10		# 3
	 #vporq		%zmm30,%zmm6,%zmm6		# padbit, yes, always

	sub		$128,%rdx
	ja		.Loop_avx512

.Ltail_avx512:
	################################################################
	# while above multiplications were by r^8 in all lanes, in last
	# iteration we multiply least significant lane by r^8 and most
	# significant one by r, that's why table gets shifted...

	vpsrlq		$32,%zmm16,%zmm16		# 0105020603070408
	vpsrlq		$32,%zmm17,%zmm17
	vpsrlq		$32,%zmm18,%zmm18
	vpsrlq		$32,%zmm23,%zmm23
	vpsrlq		$32,%zmm24,%zmm24
	vpsrlq		$32,%zmm19,%zmm19
	vpsrlq		$32,%zmm20,%zmm20
	vpsrlq		$32,%zmm21,%zmm21
	vpsrlq		$32,%zmm22,%zmm22

	################################################################
	# load either next or last 64 byte of input
	lea		(%rsi,%rdx),%rsi

	#vpaddq		%zmm2,%zmm9,%zmm2		# accumulate input
	vpaddq		%zmm0,%zmm7,%zmm0

	vpmuludq	%zmm2,%zmm17,%zmm14		# d3 = h2*r1
	vpmuludq	%zmm2,%zmm18,%zmm15		# d4 = h2*r2
	vpmuludq	%zmm2,%zmm23,%zmm11		# d0 = h2*s3
	 vpandq		%zmm5,%zmm8,%zmm8		# 1
	vpmuludq	%zmm2,%zmm24,%zmm12		# d1 = h2*s4
	 vpandq		%zmm5,%zmm10,%zmm10		# 3
	vpmuludq	%zmm2,%zmm16,%zmm13		# d2 = h2*r0
	 vporq		%zmm30,%zmm6,%zmm6		# padbit, yes, always
	 vpaddq		%zmm1,%zmm8,%zmm1		# accumulate input
	 vpaddq		%zmm3,%zmm10,%zmm3
	 vpaddq		%zmm4,%zmm6,%zmm4

	  vmovdqu	16*0(%rsi),%xmm7
	vpmuludq	%zmm0,%zmm19,%zmm28
	vpmuludq	%zmm0,%zmm20,%zmm29
	vpmuludq	%zmm0,%zmm16,%zmm25
	vpmuludq	%zmm0,%zmm17,%zmm26
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h0*r3
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h0*r4
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h0*r0
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h0*r1

	  vmovdqu	16*1(%rsi),%xmm8
	vpmuludq	%zmm1,%zmm18,%zmm28
	vpmuludq	%zmm1,%zmm19,%zmm29
	vpmuludq	%zmm1,%zmm24,%zmm25
	vpmuludq	%zmm0,%zmm18,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h1*r2
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h1*r3
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h1*s4
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h0*r2

	  vinserti128	$1,16*2(%rsi),%ymm7,%ymm7
	vpmuludq	%zmm3,%zmm16,%zmm28
	vpmuludq	%zmm3,%zmm17,%zmm29
	vpmuludq	%zmm1,%zmm16,%zmm26
	vpmuludq	%zmm1,%zmm17,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm14		# d3 += h3*r0
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h3*r1
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h1*r0
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h1*r1

	  vinserti128	$1,16*3(%rsi),%ymm8,%ymm8
	vpmuludq	%zmm4,%zmm24,%zmm28
	vpmuludq	%zmm4,%zmm16,%zmm29
	vpmuludq	%zmm3,%zmm22,%zmm25
	vpmuludq	%zmm3,%zmm23,%zmm26
	vpmuludq	%zmm3,%zmm24,%zmm27
	vpaddq		%zmm28,%zmm14,%zmm3		# h3 = d3 + h4*s4
	vpaddq		%zmm29,%zmm15,%zmm15		# d4 += h4*r0
	vpaddq		%zmm25,%zmm11,%zmm11		# d0 += h3*s2
	vpaddq		%zmm26,%zmm12,%zmm12		# d1 += h3*s3
	vpaddq		%zmm27,%zmm13,%zmm13		# d2 += h3*s4

	vpmuludq	%zmm4,%zmm21,%zmm25
	vpmuludq	%zmm4,%zmm22,%zmm26
	vpmuludq	%zmm4,%zmm23,%zmm27
	vpaddq		%zmm25,%zmm11,%zmm0		# h0 = d0 + h4*s1
	vpaddq		%zmm26,%zmm12,%zmm1		# h1 = d2 + h4*s2
	vpaddq		%zmm27,%zmm13,%zmm2		# h2 = d3 + h4*s3

	################################################################
	# horizontal addition

	mov		$1,%eax
	vpermq		$0xb1,%zmm3,%zmm14
	vpermq		$0xb1,%zmm15,%zmm4
	vpermq		$0xb1,%zmm0,%zmm11
	vpermq		$0xb1,%zmm1,%zmm12
	vpermq		$0xb1,%zmm2,%zmm13
	vpaddq		%zmm14,%zmm3,%zmm3
	vpaddq		%zmm15,%zmm4,%zmm4
	vpaddq		%zmm11,%zmm0,%zmm0
	vpaddq		%zmm12,%zmm1,%zmm1
	vpaddq		%zmm13,%zmm2,%zmm2

	kmovw		%eax,%k3
	vpermq		$0x2,%zmm3,%zmm14
	vpermq		$0x2,%zmm4,%zmm15
	vpermq		$0x2,%zmm0,%zmm11
	vpermq		$0x2,%zmm1,%zmm12
	vpermq		$0x2,%zmm2,%zmm13
	vpaddq		%zmm14,%zmm3,%zmm3
	vpaddq		%zmm15,%zmm4,%zmm4
	vpaddq		%zmm11,%zmm0,%zmm0
	vpaddq		%zmm12,%zmm1,%zmm1
	vpaddq		%zmm13,%zmm2,%zmm2

	vextracti64x4	$0x1,%zmm3,%ymm14
	vextracti64x4	$0x1,%zmm4,%ymm15
	vextracti64x4	$0x1,%zmm0,%ymm11
	vextracti64x4	$0x1,%zmm1,%ymm12
	vextracti64x4	$0x1,%zmm2,%ymm13
	vpaddq		%zmm14,%zmm3,%zmm3{%k3}{z}	# keep single qword in case
	vpaddq		%zmm15,%zmm4,%zmm4{%k3}{z}	# it's passed to .Ltail_avx2
	vpaddq		%zmm11,%zmm0,%zmm0{%k3}{z}
	vpaddq		%zmm12,%zmm1,%zmm1{%k3}{z}
	vpaddq		%zmm13,%zmm2,%zmm2{%k3}{z}
	################################################################
	# lazy reduction (interleaved with input splat)

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	 vpsrldq	$6,%ymm7,%ymm9		# splat input
	 vpsrldq	$6,%ymm8,%ymm10
	 vpunpckhqdq	%ymm8,%ymm7,%ymm6		# 4
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	 vpunpcklqdq	%ymm10,%ymm9,%ymm9		# 2:3
	 vpunpcklqdq	%ymm8,%ymm7,%ymm7		# 0:1
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm4,%ymm15
	vpand		%ymm5,%ymm4,%ymm4

	vpsrlq		$26,%ymm1,%ymm12
	vpand		%ymm5,%ymm1,%ymm1
	 vpsrlq		$30,%ymm9,%ymm10
	 vpsrlq		$4,%ymm9,%ymm9
	vpaddq		%ymm12,%ymm2,%ymm2		# h1 -> h2

	vpaddq		%ymm15,%ymm0,%ymm0
	vpsllq		$2,%ymm15,%ymm15
	 vpsrlq		$26,%ymm7,%ymm8
	 vpsrlq		$40,%ymm6,%ymm6		# 4
	vpaddq		%ymm15,%ymm0,%ymm0		# h4 -> h0

	vpsrlq		$26,%ymm2,%ymm13
	vpand		%ymm5,%ymm2,%ymm2
	 vpand		%ymm5,%ymm9,%ymm9		# 2
	 vpand		%ymm5,%ymm7,%ymm7		# 0
	vpaddq		%ymm13,%ymm3,%ymm3		# h2 -> h3

	vpsrlq		$26,%ymm0,%ymm11
	vpand		%ymm5,%ymm0,%ymm0
	 vpaddq		%ymm2,%ymm9,%ymm2		# accumulate input for .Ltail_avx2
	 vpand		%ymm5,%ymm8,%ymm8		# 1
	vpaddq		%ymm11,%ymm1,%ymm1		# h0 -> h1

	vpsrlq		$26,%ymm3,%ymm14
	vpand		%ymm5,%ymm3,%ymm3
	 vpand		%ymm5,%ymm10,%ymm10		# 3
	 vpor		32(%rcx),%ymm6,%ymm6	# padbit, yes, always
	vpaddq		%ymm14,%ymm4,%ymm4		# h3 -> h4

	lea		0x90(%rsp),%rax		# size optimization for .Ltail_avx2
	add		$64,%rdx
	jnz		.Ltail_avx2_avx512

	vpsubq		%ymm9,%ymm2,%ymm2		# undo input accumulation
	vmovd		%xmm0,-112(%rdi)# save partially reduced
	vmovd		%xmm1,-108(%rdi)
	vmovd		%xmm2,-104(%rdi)
	vmovd		%xmm3,-100(%rdi)
	vmovd		%xmm4,-96(%rdi)
	vzeroall
	lea		-8(%r10),%rsp
	ret
SYM_FUNC_END(poly1305_blocks_avx512)
#endif
