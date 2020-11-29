// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
//
// Copyright (C) 2017-2019 Samuel Neves <sneves@dei.uc.pt>. All Rights Reserved.
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
// November 2014
//
// ChaCha20 for x86_64.
//
// December 2016
//
// Add AVX512F code path.
//
// December 2017
//
// Add AVX512VL code path.
//
// Performance in cycles per byte out of large buffer.
//
//		IALU/gcc 4.8(i)	1x/2xSSSE3(ii)	4xSSSE3	    NxAVX(v)
//
// P4		9.48/+99%	-		-
// Core2		7.83/+55%	7.90/5.76	4.35
// Westmere	7.19/+50%	5.60/4.50	3.00
// Sandy Bridge	8.31/+42%	5.45/4.00	2.72
// Ivy Bridge	6.71/+46%	5.40/?		2.41
// Haswell	5.92/+43%	5.20/3.45	2.42        1.23
// Skylake[-X]	5.87/+39%	4.70/3.22	2.31        1.19[0.80(vi)]
// Silvermont	12.0/+33%	7.75/6.90	7.03(iii)
// Knights L	11.7/-		?		9.60(iii)   0.80
// Goldmont	10.6/+17%	5.10/3.52	3.28
// Sledgehammer	7.28/+52%	-		-
// Bulldozer	9.66/+28%	9.85/5.35(iv)	3.06(iv)
// Ryzen		5.96/+50%	5.19/3.00	2.40        2.09
// VIA Nano	10.5/+46%	6.72/6.88	6.05
//
// (i)	compared to older gcc 3.x one can observe >2x improvement on
//	most platforms;
// (ii)	2xSSSE3 is code path optimized specifically for 128 bytes used
//	by chacha20_poly1305_tls_cipher, results are EVP-free;
// (iii)	this is not optimal result for Atom because of MSROM
//	limitations, SSE2 can do better, but gain is considered too
//	low to justify the [maintenance] effort;
// (iv)	Bulldozer actually executes 4xXOP code path that delivers 2.20
//	and 4.85 for 128-byte inputs;
// (v)	8xAVX2, 8xAVX512VL or 16xAVX512F, whichever best applicable;
// (vi)	even though Skylake-X can execute AVX512F code and deliver 0.57
//	cpb in single thread, the corresponding capability is suppressed;

//#include <linux/linkage.h>
.section .rodata.cst16.Lzero, "aM", @progbits, 16
.align 16
.Lzero:
.long 0,0,0,0
.section .rodata.cst16.Lone, "aM", @progbits, 16
.align 16
.Lone:
.long 1,0,0,0
.section .rodata.cst16.Linc, "aM", @progbits, 16
.align 16
.Linc:
.long 0,1,2,3
.section .rodata.cst16.Lfour, "aM", @progbits, 16
.align 16
.Lfour:
.long 4,4,4,4
.section .rodata.cst32.Lincy, "aM", @progbits, 32
.align 32
.Lincy:
.long 0,2,4,6,1,3,5,7
.section .rodata.cst32.Leight, "aM", @progbits, 32
.align 32
.Leight:
.long 8,8,8,8,8,8,8,8
.section .rodata.cst16.Lrot16, "aM", @progbits, 16
.align 16
.Lrot16:
.byte 0x2,0x3,0x0,0x1, 0x6,0x7,0x4,0x5, 0xa,0xb,0x8,0x9, 0xe,0xf,0xc,0xd
.section .rodata.cst16.Lrot24, "aM", @progbits, 16
.align 16
.Lrot24:
.byte 0x3,0x0,0x1,0x2, 0x7,0x4,0x5,0x6, 0xb,0x8,0x9,0xa, 0xf,0xc,0xd,0xe
.section .rodata.cst32.Ltwoy, "aM", @progbits, 32
.align 32
.Ltwoy:
.long 2,0,0,0, 2,0,0,0
.section .rodata.cst64.Lzeroz, "aM", @progbits, 64
.align 64
.Lzeroz:
.long 0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0
.section .rodata.cst64.Lfourz, "aM", @progbits, 64
.align 64
.Lfourz:
.long 4,0,0,0, 4,0,0,0, 4,0,0,0, 4,0,0,0
.section .rodata.cst64.Lincz, "aM", @progbits, 64
.align 64
.Lincz:
.long 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
.section .rodata.cst64.Lsixteen, "aM", @progbits, 64
.align 64
.Lsixteen:
.long 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16
.section .rodata.cst16.Lsigma, "aM", @progbits, 16
.align 16
.Lsigma:
.ascii "expand 32-byte k"
.text
#ifdef CONFIG_AS_SSSE3
.align 32
SYM_FUNC_START(hchacha20_ssse3)
.Lhchacha20_ssse3:
	movdqa	.Lsigma(%rip),%xmm0
	movdqu	(%rdx),%xmm1
	movdqu	16(%rdx),%xmm2
	movdqu	(%rsi),%xmm3
	# This code is only used when targeting kernel.
	# If targeting win64, xmm{6,7} preserving needs to be added.
	movdqa	.Lrot16(%rip),%xmm6
	movdqa	.Lrot24(%rip),%xmm7
	mov	$10,%r8		# reuse %r8
	jmp	1f
.align	32
1:
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm6,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm7,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$147,%xmm0,%xmm0
	pshufd	$78,%xmm3,%xmm3
	pshufd	$57,%xmm2,%xmm2
	nop	
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm6,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm7,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$57,%xmm0,%xmm0
	pshufd	$78,%xmm3,%xmm3
	pshufd	$147,%xmm2,%xmm2
	dec	%r8
	jnz	1b
	movdqu %xmm0, (%rdi)
	movdqu %xmm3, 16(%rdi)
	ret
SYM_FUNC_END(hchacha20_ssse3)
.align 32
SYM_FUNC_START(chacha20_ssse3)
.Lchacha20_ssse3:
	lea	8(%rsp),%r10		# frame pointer
	cmp	$128,%rdx		# we might throw away some data,
	je	.Lchacha20_128
	ja	.Lchacha20_4x		# but overall it won't be slower

.Ldo_ssse3_after_all:
	sub	$64+8,%rsp
	and $-16,%rsp
	movdqa	.Lsigma(%rip),%xmm0
	movdqu	(%rcx),%xmm1
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	.Lrot16(%rip),%xmm6
	movdqa	.Lrot24(%rip),%xmm7

	movdqa	%xmm0,0x00(%rsp)
	movdqa	%xmm1,0x10(%rsp)
	movdqa	%xmm2,0x20(%rsp)
	movdqa	%xmm3,0x30(%rsp)
	mov	$10,%r8		# reuse %r8
	jmp	.Loop_ssse3

.align	32
.Loop_outer_ssse3:
	movdqa	.Lone(%rip),%xmm3
	movdqa	0x00(%rsp),%xmm0
	movdqa	0x10(%rsp),%xmm1
	movdqa	0x20(%rsp),%xmm2
	paddd	0x30(%rsp),%xmm3
	mov	$10,%r8
	movdqa	%xmm3,0x30(%rsp)
	jmp	.Loop_ssse3

.align	32
.Loop_ssse3:
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm6,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm7,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$147,%xmm0,%xmm0
	pshufd	$78,%xmm3,%xmm3
	pshufd	$57,%xmm2,%xmm2
	nop	
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm6,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
	pshufb	%xmm7,%xmm3
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$57,%xmm0,%xmm0
	pshufd	$78,%xmm3,%xmm3
	pshufd	$147,%xmm2,%xmm2
	dec	%r8
	jnz	.Loop_ssse3
	paddd	0x00(%rsp),%xmm0
	paddd	0x10(%rsp),%xmm1
	paddd	0x20(%rsp),%xmm2
	paddd	0x30(%rsp),%xmm3

	cmp	$64,%rdx
	jb	.Ltail_ssse3

	movdqu	0x00(%rsi),%xmm4
	movdqu	0x10(%rsi),%xmm5
	pxor	%xmm4,%xmm0			# xor with input
	movdqu	0x20(%rsi),%xmm4
	pxor	%xmm5,%xmm1
	movdqu	0x30(%rsi),%xmm5
	lea	0x40(%rsi),%rsi		# inp+=64
	pxor	%xmm4,%xmm2
	pxor	%xmm5,%xmm3

	movdqu	%xmm0,0x00(%rdi)		# write output
	movdqu	%xmm1,0x10(%rdi)
	movdqu	%xmm2,0x20(%rdi)
	movdqu	%xmm3,0x30(%rdi)
	lea	0x40(%rdi),%rdi		# out+=64

	sub	$64,%rdx
	jnz	.Loop_outer_ssse3

	jmp	.Ldone_ssse3

.align	16
.Ltail_ssse3:
	movdqa	%xmm0,0x00(%rsp)
	movdqa	%xmm1,0x10(%rsp)
	movdqa	%xmm2,0x20(%rsp)
	movdqa	%xmm3,0x30(%rsp)
	xor	%r8,%r8

.Loop_tail_ssse3:
	movzb	(%rsi,%r8),%eax
	movzb	(%rsp,%r8),%ecx
	lea	1(%r8),%r8
	xor	%ecx,%eax
	mov	%al,-1(%rdi,%r8)
	dec	%rdx
	jnz	.Loop_tail_ssse3

.Ldone_ssse3:
	lea	-8(%r10),%rsp
.Lssse3_epilogue:
	ret
SYM_FUNC_END(chacha20_ssse3)
.type	chacha20_128,@function
.align	32
chacha20_128:
.Lchacha20_128:
	lea	8(%rsp),%r10		# frame pointer
	sub	$64+8,%rsp
	and $-16,%rsp
	movdqa	.Lsigma(%rip),%xmm8
	movdqu	(%rcx),%xmm9
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	.Lone(%rip),%xmm1
	movdqa	.Lrot16(%rip),%xmm6
	movdqa	.Lrot24(%rip),%xmm7

	movdqa	%xmm8,%xmm10
	movdqa	%xmm8,0x00(%rsp)
	movdqa	%xmm9,%xmm11
	movdqa	%xmm9,0x10(%rsp)
	movdqa	%xmm2,%xmm0
	movdqa	%xmm2,0x20(%rsp)
	paddd	%xmm3,%xmm1
	movdqa	%xmm3,0x30(%rsp)
	mov	$10,%r8		# reuse %r8
	jmp	.Loop_128

.align	32
.Loop_128:
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
	pshufb	%xmm6,%xmm3
	pshufb	%xmm6,%xmm1
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
	pshufb	%xmm7,%xmm3
	pshufb	%xmm7,%xmm1
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$147,%xmm8,%xmm8
	pshufd	$78,%xmm3,%xmm3
	pshufd	$57,%xmm2,%xmm2
	pshufd	$147,%xmm10,%xmm10
	pshufd	$78,%xmm1,%xmm1
	pshufd	$57,%xmm0,%xmm0
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
	pshufb	%xmm6,%xmm3
	pshufb	%xmm6,%xmm1
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
	pshufb	%xmm7,%xmm3
	pshufb	%xmm7,%xmm1
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$57,%xmm8,%xmm8
	pshufd	$78,%xmm3,%xmm3
	pshufd	$147,%xmm2,%xmm2
	pshufd	$57,%xmm10,%xmm10
	pshufd	$78,%xmm1,%xmm1
	pshufd	$147,%xmm0,%xmm0
	dec	%r8
	jnz	.Loop_128
	paddd	0x00(%rsp),%xmm8
	paddd	0x10(%rsp),%xmm9
	paddd	0x20(%rsp),%xmm2
	paddd	0x30(%rsp),%xmm3
	paddd	.Lone(%rip),%xmm1
	paddd	0x00(%rsp),%xmm10
	paddd	0x10(%rsp),%xmm11
	paddd	0x20(%rsp),%xmm0
	paddd	0x30(%rsp),%xmm1

	movdqu	0x00(%rsi),%xmm4
	movdqu	0x10(%rsi),%xmm5
	pxor	%xmm4,%xmm8			# xor with input
	movdqu	0x20(%rsi),%xmm4
	pxor	%xmm5,%xmm9
	movdqu	0x30(%rsi),%xmm5
	pxor	%xmm4,%xmm2
	movdqu	0x40(%rsi),%xmm4
	pxor	%xmm5,%xmm3
	movdqu	0x50(%rsi),%xmm5
	pxor	%xmm4,%xmm10
	movdqu	0x60(%rsi),%xmm4
	pxor	%xmm5,%xmm11
	movdqu	0x70(%rsi),%xmm5
	pxor	%xmm4,%xmm0
	pxor	%xmm5,%xmm1

	movdqu	%xmm8,0x00(%rdi)		# write output
	movdqu	%xmm9,0x10(%rdi)
	movdqu	%xmm2,0x20(%rdi)
	movdqu	%xmm3,0x30(%rdi)
	movdqu	%xmm10,0x40(%rdi)
	movdqu	%xmm11,0x50(%rdi)
	movdqu	%xmm0,0x60(%rdi)
	movdqu	%xmm1,0x70(%rdi)
	lea	-8(%r10),%rsp
.L128_epilogue:
	ret
.size	chacha20_128,.-chacha20_128
.type	chacha20_4x,@function
.align	32
chacha20_4x:
.Lchacha20_4x:
	lea		8(%rsp),%r10		# frame pointer
	cmp		$192,%rdx
	ja		.Lproceed4x
.Lproceed4x:
	sub		$0x140+8,%rsp
	and		$-16,%rsp
	movdqa		.Lsigma(%rip),%xmm11	# key[0]
	movdqu		(%rcx),%xmm15		# key[1]
	movdqu		16(%rcx),%xmm7		# key[2]
	movdqu		(%r8),%xmm3		# key[3]
	lea		0x100(%rsp),%rcx	# size optimization
	lea		.Lrot16(%rip),%r9
	lea		.Lrot24(%rip),%r11

	pshufd		$0x00,%xmm11,%xmm8	# smash key by lanes...
	pshufd		$0x55,%xmm11,%xmm9
	movdqa		%xmm8,0x40(%rsp)		# ... and offload
	pshufd		$0xaa,%xmm11,%xmm10
	movdqa		%xmm9,0x50(%rsp)
	pshufd		$0xff,%xmm11,%xmm11
	movdqa		%xmm10,0x60(%rsp)
	movdqa		%xmm11,0x70(%rsp)

	pshufd		$0x00,%xmm15,%xmm12
	pshufd		$0x55,%xmm15,%xmm13
	movdqa		%xmm12,0x80-0x100(%rcx)
	pshufd		$0xaa,%xmm15,%xmm14
	movdqa		%xmm13,0x90-0x100(%rcx)
	pshufd		$0xff,%xmm15,%xmm15
	movdqa		%xmm14,0xa0-0x100(%rcx)
	movdqa		%xmm15,0xb0-0x100(%rcx)

	pshufd		$0x00,%xmm7,%xmm4	# ""
	pshufd		$0x55,%xmm7,%xmm5	# ""
	movdqa		%xmm4,0xc0-0x100(%rcx)
	pshufd		$0xaa,%xmm7,%xmm6	# ""
	movdqa		%xmm5,0xd0-0x100(%rcx)
	pshufd		$0xff,%xmm7,%xmm7	# ""
	movdqa		%xmm6,0xe0-0x100(%rcx)
	movdqa		%xmm7,0xf0-0x100(%rcx)

	pshufd		$0x00,%xmm3,%xmm0
	pshufd		$0x55,%xmm3,%xmm1
	paddd		.Linc(%rip),%xmm0	# don't save counters yet
	pshufd		$0xaa,%xmm3,%xmm2
	movdqa		%xmm1,0x110-0x100(%rcx)
	pshufd		$0xff,%xmm3,%xmm3
	movdqa		%xmm2,0x120-0x100(%rcx)
	movdqa		%xmm3,0x130-0x100(%rcx)

	jmp		.Loop_enter4x

.align	32
.Loop_outer4x:
	movdqa		0x40(%rsp),%xmm8		# re-load smashed key
	movdqa		0x50(%rsp),%xmm9
	movdqa		0x60(%rsp),%xmm10
	movdqa		0x70(%rsp),%xmm11
	movdqa		0x80-0x100(%rcx),%xmm12
	movdqa		0x90-0x100(%rcx),%xmm13
	movdqa		0xa0-0x100(%rcx),%xmm14
	movdqa		0xb0-0x100(%rcx),%xmm15
	movdqa		0xc0-0x100(%rcx),%xmm4	# ""
	movdqa		0xd0-0x100(%rcx),%xmm5	# ""
	movdqa		0xe0-0x100(%rcx),%xmm6	# ""
	movdqa		0xf0-0x100(%rcx),%xmm7	# ""
	movdqa		0x100-0x100(%rcx),%xmm0
	movdqa		0x110-0x100(%rcx),%xmm1
	movdqa		0x120-0x100(%rcx),%xmm2
	movdqa		0x130-0x100(%rcx),%xmm3
	paddd		.Lfour(%rip),%xmm0	# next SIMD counters

.Loop_enter4x:
	movdqa		%xmm6,0x20(%rsp)		# SIMD equivalent of "%nox"
	movdqa		%xmm7,0x30(%rsp)		# SIMD equivalent of "%nox"
	movdqa		(%r9),%xmm7		# .Lrot16(%rip)
	mov		$10,%eax
	movdqa		%xmm0,0x100-0x100(%rcx)	# save SIMD counters
	jmp		.Loop4x

.align	32
.Loop4x:
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
	pshufb	%xmm7,%xmm0
	pshufb	%xmm7,%xmm1
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm6
	pslld	$12,%xmm12
	psrld	$20,%xmm6
	movdqa	%xmm13,%xmm7
	pslld	$12,%xmm13
	por	%xmm6,%xmm12
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm13
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
	pshufb	%xmm6,%xmm0
	pshufb	%xmm6,%xmm1
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm7
	pslld	$7,%xmm12
	psrld	$25,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$7,%xmm13
	por	%xmm7,%xmm12
	psrld	$25,%xmm6
	movdqa	(%r9),%xmm7
	por	%xmm6,%xmm13
	movdqa	%xmm4,0(%rsp)
	movdqa	%xmm5,16(%rsp)
	movdqa	32(%rsp),%xmm4
	movdqa	48(%rsp),%xmm5
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
	pshufb	%xmm7,%xmm2
	pshufb	%xmm7,%xmm3
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm6
	pslld	$12,%xmm14
	psrld	$20,%xmm6
	movdqa	%xmm15,%xmm7
	pslld	$12,%xmm15
	por	%xmm6,%xmm14
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm15
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
	pshufb	%xmm6,%xmm2
	pshufb	%xmm6,%xmm3
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm7
	pslld	$7,%xmm14
	psrld	$25,%xmm7
	movdqa	%xmm15,%xmm6
	pslld	$7,%xmm15
	por	%xmm7,%xmm14
	psrld	$25,%xmm6
	movdqa	(%r9),%xmm7
	por	%xmm6,%xmm15
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
	pshufb	%xmm7,%xmm3
	pshufb	%xmm7,%xmm0
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm6
	pslld	$12,%xmm13
	psrld	$20,%xmm6
	movdqa	%xmm14,%xmm7
	pslld	$12,%xmm14
	por	%xmm6,%xmm13
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm14
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
	pshufb	%xmm6,%xmm3
	pshufb	%xmm6,%xmm0
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm7
	pslld	$7,%xmm13
	psrld	$25,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$7,%xmm14
	por	%xmm7,%xmm13
	psrld	$25,%xmm6
	movdqa	(%r9),%xmm7
	por	%xmm6,%xmm14
	movdqa	%xmm4,32(%rsp)
	movdqa	%xmm5,48(%rsp)
	movdqa	0(%rsp),%xmm4
	movdqa	16(%rsp),%xmm5
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
	pshufb	%xmm7,%xmm1
	pshufb	%xmm7,%xmm2
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm6
	pslld	$12,%xmm15
	psrld	$20,%xmm6
	movdqa	%xmm12,%xmm7
	pslld	$12,%xmm12
	por	%xmm6,%xmm15
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm12
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
	pshufb	%xmm6,%xmm1
	pshufb	%xmm6,%xmm2
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm7
	pslld	$7,%xmm15
	psrld	$25,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$7,%xmm12
	por	%xmm7,%xmm15
	psrld	$25,%xmm6
	movdqa	(%r9),%xmm7
	por	%xmm6,%xmm12
	dec		%eax
	jnz		.Loop4x

	paddd		0x40(%rsp),%xmm8		# accumulate key material
	paddd		0x50(%rsp),%xmm9
	paddd		0x60(%rsp),%xmm10
	paddd		0x70(%rsp),%xmm11

	movdqa		%xmm8,%xmm6		# "de-interlace" data
	punpckldq	%xmm9,%xmm8
	movdqa		%xmm10,%xmm7
	punpckldq	%xmm11,%xmm10
	punpckhdq	%xmm9,%xmm6
	punpckhdq	%xmm11,%xmm7
	movdqa		%xmm8,%xmm9
	punpcklqdq	%xmm10,%xmm8		# "a0"
	movdqa		%xmm6,%xmm11
	punpcklqdq	%xmm7,%xmm6		# "a2"
	punpckhqdq	%xmm10,%xmm9		# "a1"
	punpckhqdq	%xmm7,%xmm11		# "a3"
	paddd		0x80-0x100(%rcx),%xmm12
	paddd		0x90-0x100(%rcx),%xmm13
	paddd		0xa0-0x100(%rcx),%xmm14
	paddd		0xb0-0x100(%rcx),%xmm15

	movdqa		%xmm8,0x00(%rsp)		# offload 
	movdqa		%xmm9,0x10(%rsp)
	movdqa		0x20(%rsp),%xmm8		# "xc2"
	movdqa		0x30(%rsp),%xmm9		# "xc3"

	movdqa		%xmm12,%xmm10
	punpckldq	%xmm13,%xmm12
	movdqa		%xmm14,%xmm7
	punpckldq	%xmm15,%xmm14
	punpckhdq	%xmm13,%xmm10
	punpckhdq	%xmm15,%xmm7
	movdqa		%xmm12,%xmm13
	punpcklqdq	%xmm14,%xmm12		# "b0"
	movdqa		%xmm10,%xmm15
	punpcklqdq	%xmm7,%xmm10		# "b2"
	punpckhqdq	%xmm14,%xmm13		# "b1"
	punpckhqdq	%xmm7,%xmm15		# "b3"
	paddd		0xc0-0x100(%rcx),%xmm4
	paddd		0xd0-0x100(%rcx),%xmm5
	paddd		0xe0-0x100(%rcx),%xmm8
	paddd		0xf0-0x100(%rcx),%xmm9

	movdqa		%xmm6,0x20(%rsp)		# keep offloading 
	movdqa		%xmm11,0x30(%rsp)

	movdqa		%xmm4,%xmm14
	punpckldq	%xmm5,%xmm4
	movdqa		%xmm8,%xmm7
	punpckldq	%xmm9,%xmm8
	punpckhdq	%xmm5,%xmm14
	punpckhdq	%xmm9,%xmm7
	movdqa		%xmm4,%xmm5
	punpcklqdq	%xmm8,%xmm4		# "c0"
	movdqa		%xmm14,%xmm9
	punpcklqdq	%xmm7,%xmm14		# "c2"
	punpckhqdq	%xmm8,%xmm5		# "c1"
	punpckhqdq	%xmm7,%xmm9		# "c3"
	paddd		0x100-0x100(%rcx),%xmm0
	paddd		0x110-0x100(%rcx),%xmm1
	paddd		0x120-0x100(%rcx),%xmm2
	paddd		0x130-0x100(%rcx),%xmm3

	movdqa		%xmm0,%xmm8
	punpckldq	%xmm1,%xmm0
	movdqa		%xmm2,%xmm7
	punpckldq	%xmm3,%xmm2
	punpckhdq	%xmm1,%xmm8
	punpckhdq	%xmm3,%xmm7
	movdqa		%xmm0,%xmm1
	punpcklqdq	%xmm2,%xmm0		# "d0"
	movdqa		%xmm8,%xmm3
	punpcklqdq	%xmm7,%xmm8		# "d2"
	punpckhqdq	%xmm2,%xmm1		# "d1"
	punpckhqdq	%xmm7,%xmm3		# "d3"
	cmp		$64*4,%rdx
	jb		.Ltail4x

	movdqu		0x00(%rsi),%xmm6		# xor with input
	movdqu		0x10(%rsi),%xmm11
	movdqu		0x20(%rsi),%xmm2
	movdqu		0x30(%rsi),%xmm7
	pxor		0x00(%rsp),%xmm6		#  is offloaded, remember?
	pxor		%xmm12,%xmm11
	pxor		%xmm4,%xmm2
	pxor		%xmm0,%xmm7

	 movdqu		%xmm6,0x00(%rdi)
	movdqu		0x40(%rsi),%xmm6
	 movdqu		%xmm11,0x10(%rdi)
	movdqu		0x50(%rsi),%xmm11
	 movdqu		%xmm2,0x20(%rdi)
	movdqu		0x60(%rsi),%xmm2
	 movdqu		%xmm7,0x30(%rdi)
	movdqu		0x70(%rsi),%xmm7
	lea		0x80(%rsi),%rsi		# size optimization
	pxor		0x10(%rsp),%xmm6
	pxor		%xmm13,%xmm11
	pxor		%xmm5,%xmm2
	pxor		%xmm1,%xmm7

	 movdqu		%xmm6,0x40(%rdi)
	movdqu		0x00(%rsi),%xmm6
	 movdqu		%xmm11,0x50(%rdi)
	movdqu		0x10(%rsi),%xmm11
	 movdqu		%xmm2,0x60(%rdi)
	movdqu		0x20(%rsi),%xmm2
	 movdqu		%xmm7,0x70(%rdi)
	 lea		0x80(%rdi),%rdi		# size optimization
	movdqu		0x30(%rsi),%xmm7
	pxor		0x20(%rsp),%xmm6
	pxor		%xmm10,%xmm11
	pxor		%xmm14,%xmm2
	pxor		%xmm8,%xmm7

	 movdqu		%xmm6,0x00(%rdi)
	movdqu		0x40(%rsi),%xmm6
	 movdqu		%xmm11,0x10(%rdi)
	movdqu		0x50(%rsi),%xmm11
	 movdqu		%xmm2,0x20(%rdi)
	movdqu		0x60(%rsi),%xmm2
	 movdqu		%xmm7,0x30(%rdi)
	movdqu		0x70(%rsi),%xmm7
	lea		0x80(%rsi),%rsi		# inp+=64*4
	pxor		0x30(%rsp),%xmm6
	pxor		%xmm15,%xmm11
	pxor		%xmm9,%xmm2
	pxor		%xmm3,%xmm7
	movdqu		%xmm6,0x40(%rdi)
	movdqu		%xmm11,0x50(%rdi)
	movdqu		%xmm2,0x60(%rdi)
	movdqu		%xmm7,0x70(%rdi)
	lea		0x80(%rdi),%rdi		# out+=64*4

	sub		$64*4,%rdx
	jnz		.Loop_outer4x

	jmp		.Ldone4x

.Ltail4x:
	cmp		$192,%rdx
	jae		.L192_or_more4x
	cmp		$128,%rdx
	jae		.L128_or_more4x
	cmp		$64,%rdx
	jae		.L64_or_more4x

	#movdqa		0x00(%rsp),%xmm6		#  is offloaded, remember?
	xor		%r9,%r9
	#movdqa		%xmm6,0x00(%rsp)
	movdqa		%xmm12,0x10(%rsp)
	movdqa		%xmm4,0x20(%rsp)
	movdqa		%xmm0,0x30(%rsp)
	jmp		.Loop_tail4x

.align	32
.L64_or_more4x:
	movdqu		0x00(%rsi),%xmm6		# xor with input
	movdqu		0x10(%rsi),%xmm11
	movdqu		0x20(%rsi),%xmm2
	movdqu		0x30(%rsi),%xmm7
	pxor		0x00(%rsp),%xmm6		#  is offloaded, remember?
	pxor		%xmm12,%xmm11
	pxor		%xmm4,%xmm2
	pxor		%xmm0,%xmm7
	movdqu		%xmm6,0x00(%rdi)
	movdqu		%xmm11,0x10(%rdi)
	movdqu		%xmm2,0x20(%rdi)
	movdqu		%xmm7,0x30(%rdi)
	je		.Ldone4x

	movdqa		0x10(%rsp),%xmm6		#  is offloaded, remember?
	lea		0x40(%rsi),%rsi		# inp+=64*1
	xor		%r9,%r9
	movdqa		%xmm6,0x00(%rsp)
	movdqa		%xmm13,0x10(%rsp)
	lea		0x40(%rdi),%rdi		# out+=64*1
	movdqa		%xmm5,0x20(%rsp)
	sub		$64,%rdx		# len-=64*1
	movdqa		%xmm1,0x30(%rsp)
	jmp		.Loop_tail4x

.align	32
.L128_or_more4x:
	movdqu		0x00(%rsi),%xmm6		# xor with input
	movdqu		0x10(%rsi),%xmm11
	movdqu		0x20(%rsi),%xmm2
	movdqu		0x30(%rsi),%xmm7
	pxor		0x00(%rsp),%xmm6		#  is offloaded, remember?
	pxor		%xmm12,%xmm11
	pxor		%xmm4,%xmm2
	pxor		%xmm0,%xmm7

	 movdqu		%xmm6,0x00(%rdi)
	movdqu		0x40(%rsi),%xmm6
	 movdqu		%xmm11,0x10(%rdi)
	movdqu		0x50(%rsi),%xmm11
	 movdqu		%xmm2,0x20(%rdi)
	movdqu		0x60(%rsi),%xmm2
	 movdqu		%xmm7,0x30(%rdi)
	movdqu		0x70(%rsi),%xmm7
	pxor		0x10(%rsp),%xmm6
	pxor		%xmm13,%xmm11
	pxor		%xmm5,%xmm2
	pxor		%xmm1,%xmm7
	movdqu		%xmm6,0x40(%rdi)
	movdqu		%xmm11,0x50(%rdi)
	movdqu		%xmm2,0x60(%rdi)
	movdqu		%xmm7,0x70(%rdi)
	je		.Ldone4x

	movdqa		0x20(%rsp),%xmm6		#  is offloaded, remember?
	lea		0x80(%rsi),%rsi		# inp+=64*2
	xor		%r9,%r9
	movdqa		%xmm6,0x00(%rsp)
	movdqa		%xmm10,0x10(%rsp)
	lea		0x80(%rdi),%rdi		# out+=64*2
	movdqa		%xmm14,0x20(%rsp)
	sub		$128,%rdx		# len-=64*2
	movdqa		%xmm8,0x30(%rsp)
	jmp		.Loop_tail4x

.align	32
.L192_or_more4x:
	movdqu		0x00(%rsi),%xmm6		# xor with input
	movdqu		0x10(%rsi),%xmm11
	movdqu		0x20(%rsi),%xmm2
	movdqu		0x30(%rsi),%xmm7
	pxor		0x00(%rsp),%xmm6		#  is offloaded, remember?
	pxor		%xmm12,%xmm11
	pxor		%xmm4,%xmm2
	pxor		%xmm0,%xmm7

	 movdqu		%xmm6,0x00(%rdi)
	movdqu		0x40(%rsi),%xmm6
	 movdqu		%xmm11,0x10(%rdi)
	movdqu		0x50(%rsi),%xmm11
	 movdqu		%xmm2,0x20(%rdi)
	movdqu		0x60(%rsi),%xmm2
	 movdqu		%xmm7,0x30(%rdi)
	movdqu		0x70(%rsi),%xmm7
	lea		0x80(%rsi),%rsi		# size optimization
	pxor		0x10(%rsp),%xmm6
	pxor		%xmm13,%xmm11
	pxor		%xmm5,%xmm2
	pxor		%xmm1,%xmm7

	 movdqu		%xmm6,0x40(%rdi)
	movdqu		0x00(%rsi),%xmm6
	 movdqu		%xmm11,0x50(%rdi)
	movdqu		0x10(%rsi),%xmm11
	 movdqu		%xmm2,0x60(%rdi)
	movdqu		0x20(%rsi),%xmm2
	 movdqu		%xmm7,0x70(%rdi)
	 lea		0x80(%rdi),%rdi		# size optimization
	movdqu		0x30(%rsi),%xmm7
	pxor		0x20(%rsp),%xmm6
	pxor		%xmm10,%xmm11
	pxor		%xmm14,%xmm2
	pxor		%xmm8,%xmm7
	movdqu		%xmm6,0x00(%rdi)
	movdqu		%xmm11,0x10(%rdi)
	movdqu		%xmm2,0x20(%rdi)
	movdqu		%xmm7,0x30(%rdi)
	je		.Ldone4x

	movdqa		0x30(%rsp),%xmm6		#  is offloaded, remember?
	lea		0x40(%rsi),%rsi		# inp+=64*3
	xor		%r9,%r9
	movdqa		%xmm6,0x00(%rsp)
	movdqa		%xmm15,0x10(%rsp)
	lea		0x40(%rdi),%rdi		# out+=64*3
	movdqa		%xmm9,0x20(%rsp)
	sub		$192,%rdx		# len-=64*3
	movdqa		%xmm3,0x30(%rsp)

.Loop_tail4x:
	movzb		(%rsi,%r9),%eax
	movzb		(%rsp,%r9),%ecx
	lea		1(%r9),%r9
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r9)
	dec		%rdx
	jnz		.Loop_tail4x

.Ldone4x:
	lea		-8(%r10),%rsp
.L4x_epilogue:
	ret
.size	chacha20_4x,.-chacha20_4x
#endif
#ifdef CONFIG_AS_AVX2
.align 32
SYM_FUNC_START(chacha20_avx2)
.Lchacha20_avx2:
.Lchacha20_8x:
	lea		8(%rsp),%r10		# frame register
	sub		$0x280+8,%rsp
	and		$-32,%rsp
	vzeroupper

	################ stack layout
	# +0x00		SIMD equivalent of %r12d
	# ...
	# +0x80		constant copy of key[0-2] smashed by lanes
	# ...
	# +0x200	SIMD counters (with nonce smashed by lanes)
	# ...
	# +0x280

	vbroadcasti128	.Lsigma(%rip),%ymm11	# key[0]
	vbroadcasti128	(%rcx),%ymm3		# key[1]
	vbroadcasti128	16(%rcx),%ymm15		# key[2]
	vbroadcasti128	(%r8),%ymm7		# key[3]
	lea		0x100(%rsp),%rcx	# size optimization
	lea		0x200(%rsp),%rax	# size optimization
	lea		.Lrot16(%rip),%r9
	lea		.Lrot24(%rip),%r11

	vpshufd		$0x00,%ymm11,%ymm8	# smash key by lanes...
	vpshufd		$0x55,%ymm11,%ymm9
	vmovdqa		%ymm8,0x80-0x100(%rcx)	# ... and offload
	vpshufd		$0xaa,%ymm11,%ymm10
	vmovdqa		%ymm9,0xa0-0x100(%rcx)
	vpshufd		$0xff,%ymm11,%ymm11
	vmovdqa		%ymm10,0xc0-0x100(%rcx)
	vmovdqa		%ymm11,0xe0-0x100(%rcx)

	vpshufd		$0x00,%ymm3,%ymm0
	vpshufd		$0x55,%ymm3,%ymm1
	vmovdqa		%ymm0,0x100-0x100(%rcx)
	vpshufd		$0xaa,%ymm3,%ymm2
	vmovdqa		%ymm1,0x120-0x100(%rcx)
	vpshufd		$0xff,%ymm3,%ymm3
	vmovdqa		%ymm2,0x140-0x100(%rcx)
	vmovdqa		%ymm3,0x160-0x100(%rcx)

	vpshufd		$0x00,%ymm15,%ymm12	# "xc0"
	vpshufd		$0x55,%ymm15,%ymm13	# "xc1"
	vmovdqa		%ymm12,0x180-0x200(%rax)
	vpshufd		$0xaa,%ymm15,%ymm14	# "xc2"
	vmovdqa		%ymm13,0x1a0-0x200(%rax)
	vpshufd		$0xff,%ymm15,%ymm15	# "xc3"
	vmovdqa		%ymm14,0x1c0-0x200(%rax)
	vmovdqa		%ymm15,0x1e0-0x200(%rax)

	vpshufd		$0x00,%ymm7,%ymm4
	vpshufd		$0x55,%ymm7,%ymm5
	vpaddd		.Lincy(%rip),%ymm4,%ymm4	# don't save counters yet
	vpshufd		$0xaa,%ymm7,%ymm6
	vmovdqa		%ymm5,0x220-0x200(%rax)
	vpshufd		$0xff,%ymm7,%ymm7
	vmovdqa		%ymm6,0x240-0x200(%rax)
	vmovdqa		%ymm7,0x260-0x200(%rax)

	jmp		.Loop_enter8x

.align	32
.Loop_outer8x:
	vmovdqa		0x80-0x100(%rcx),%ymm8	# re-load smashed key
	vmovdqa		0xa0-0x100(%rcx),%ymm9
	vmovdqa		0xc0-0x100(%rcx),%ymm10
	vmovdqa		0xe0-0x100(%rcx),%ymm11
	vmovdqa		0x100-0x100(%rcx),%ymm0
	vmovdqa		0x120-0x100(%rcx),%ymm1
	vmovdqa		0x140-0x100(%rcx),%ymm2
	vmovdqa		0x160-0x100(%rcx),%ymm3
	vmovdqa		0x180-0x200(%rax),%ymm12	# "xc0"
	vmovdqa		0x1a0-0x200(%rax),%ymm13	# "xc1"
	vmovdqa		0x1c0-0x200(%rax),%ymm14	# "xc2"
	vmovdqa		0x1e0-0x200(%rax),%ymm15	# "xc3"
	vmovdqa		0x200-0x200(%rax),%ymm4
	vmovdqa		0x220-0x200(%rax),%ymm5
	vmovdqa		0x240-0x200(%rax),%ymm6
	vmovdqa		0x260-0x200(%rax),%ymm7
	vpaddd		.Leight(%rip),%ymm4,%ymm4	# next SIMD counters

.Loop_enter8x:
	vmovdqa		%ymm14,0x40(%rsp)		# SIMD equivalent of "%nox"
	vmovdqa		%ymm15,0x60(%rsp)		# SIMD equivalent of "%nox"
	vbroadcasti128	(%r9),%ymm15
	vmovdqa		%ymm4,0x200-0x200(%rax)	# save SIMD counters
	mov		$10,%eax
	jmp		.Loop8x

.align	32
.Loop8x:
	vpaddd	%ymm0,%ymm8,%ymm8
	vpxor	%ymm4,%ymm8,%ymm4
	vpshufb	%ymm15,%ymm4,%ymm4
	vpaddd	%ymm1,%ymm9,%ymm9
	vpxor	%ymm5,%ymm9,%ymm5
	vpshufb	%ymm15,%ymm5,%ymm5
	vpaddd	%ymm4,%ymm12,%ymm12
	vpxor	%ymm0,%ymm12,%ymm0
	vpslld	$12,%ymm0,%ymm14
	vpsrld	$20,%ymm0,%ymm0
	vpor	%ymm0,%ymm14,%ymm0
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm5,%ymm13,%ymm13
	vpxor	%ymm1,%ymm13,%ymm1
	vpslld	$12,%ymm1,%ymm15
	vpsrld	$20,%ymm1,%ymm1
	vpor	%ymm1,%ymm15,%ymm1
	vpaddd	%ymm0,%ymm8,%ymm8
	vpxor	%ymm4,%ymm8,%ymm4
	vpshufb	%ymm14,%ymm4,%ymm4
	vpaddd	%ymm1,%ymm9,%ymm9
	vpxor	%ymm5,%ymm9,%ymm5
	vpshufb	%ymm14,%ymm5,%ymm5
	vpaddd	%ymm4,%ymm12,%ymm12
	vpxor	%ymm0,%ymm12,%ymm0
	vpslld	$7,%ymm0,%ymm15
	vpsrld	$25,%ymm0,%ymm0
	vpor	%ymm0,%ymm15,%ymm0
	vbroadcasti128	(%r9),%ymm15
	vpaddd	%ymm5,%ymm13,%ymm13
	vpxor	%ymm1,%ymm13,%ymm1
	vpslld	$7,%ymm1,%ymm14
	vpsrld	$25,%ymm1,%ymm1
	vpor	%ymm1,%ymm14,%ymm1
	vmovdqa	%ymm12,0(%rsp)
	vmovdqa	%ymm13,32(%rsp)
	vmovdqa	64(%rsp),%ymm12
	vmovdqa	96(%rsp),%ymm13
	vpaddd	%ymm2,%ymm10,%ymm10
	vpxor	%ymm6,%ymm10,%ymm6
	vpshufb	%ymm15,%ymm6,%ymm6
	vpaddd	%ymm3,%ymm11,%ymm11
	vpxor	%ymm7,%ymm11,%ymm7
	vpshufb	%ymm15,%ymm7,%ymm7
	vpaddd	%ymm6,%ymm12,%ymm12
	vpxor	%ymm2,%ymm12,%ymm2
	vpslld	$12,%ymm2,%ymm14
	vpsrld	$20,%ymm2,%ymm2
	vpor	%ymm2,%ymm14,%ymm2
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm7,%ymm13,%ymm13
	vpxor	%ymm3,%ymm13,%ymm3
	vpslld	$12,%ymm3,%ymm15
	vpsrld	$20,%ymm3,%ymm3
	vpor	%ymm3,%ymm15,%ymm3
	vpaddd	%ymm2,%ymm10,%ymm10
	vpxor	%ymm6,%ymm10,%ymm6
	vpshufb	%ymm14,%ymm6,%ymm6
	vpaddd	%ymm3,%ymm11,%ymm11
	vpxor	%ymm7,%ymm11,%ymm7
	vpshufb	%ymm14,%ymm7,%ymm7
	vpaddd	%ymm6,%ymm12,%ymm12
	vpxor	%ymm2,%ymm12,%ymm2
	vpslld	$7,%ymm2,%ymm15
	vpsrld	$25,%ymm2,%ymm2
	vpor	%ymm2,%ymm15,%ymm2
	vbroadcasti128	(%r9),%ymm15
	vpaddd	%ymm7,%ymm13,%ymm13
	vpxor	%ymm3,%ymm13,%ymm3
	vpslld	$7,%ymm3,%ymm14
	vpsrld	$25,%ymm3,%ymm3
	vpor	%ymm3,%ymm14,%ymm3
	vpaddd	%ymm1,%ymm8,%ymm8
	vpxor	%ymm7,%ymm8,%ymm7
	vpshufb	%ymm15,%ymm7,%ymm7
	vpaddd	%ymm2,%ymm9,%ymm9
	vpxor	%ymm4,%ymm9,%ymm4
	vpshufb	%ymm15,%ymm4,%ymm4
	vpaddd	%ymm7,%ymm12,%ymm12
	vpxor	%ymm1,%ymm12,%ymm1
	vpslld	$12,%ymm1,%ymm14
	vpsrld	$20,%ymm1,%ymm1
	vpor	%ymm1,%ymm14,%ymm1
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm4,%ymm13,%ymm13
	vpxor	%ymm2,%ymm13,%ymm2
	vpslld	$12,%ymm2,%ymm15
	vpsrld	$20,%ymm2,%ymm2
	vpor	%ymm2,%ymm15,%ymm2
	vpaddd	%ymm1,%ymm8,%ymm8
	vpxor	%ymm7,%ymm8,%ymm7
	vpshufb	%ymm14,%ymm7,%ymm7
	vpaddd	%ymm2,%ymm9,%ymm9
	vpxor	%ymm4,%ymm9,%ymm4
	vpshufb	%ymm14,%ymm4,%ymm4
	vpaddd	%ymm7,%ymm12,%ymm12
	vpxor	%ymm1,%ymm12,%ymm1
	vpslld	$7,%ymm1,%ymm15
	vpsrld	$25,%ymm1,%ymm1
	vpor	%ymm1,%ymm15,%ymm1
	vbroadcasti128	(%r9),%ymm15
	vpaddd	%ymm4,%ymm13,%ymm13
	vpxor	%ymm2,%ymm13,%ymm2
	vpslld	$7,%ymm2,%ymm14
	vpsrld	$25,%ymm2,%ymm2
	vpor	%ymm2,%ymm14,%ymm2
	vmovdqa	%ymm12,64(%rsp)
	vmovdqa	%ymm13,96(%rsp)
	vmovdqa	0(%rsp),%ymm12
	vmovdqa	32(%rsp),%ymm13
	vpaddd	%ymm3,%ymm10,%ymm10
	vpxor	%ymm5,%ymm10,%ymm5
	vpshufb	%ymm15,%ymm5,%ymm5
	vpaddd	%ymm0,%ymm11,%ymm11
	vpxor	%ymm6,%ymm11,%ymm6
	vpshufb	%ymm15,%ymm6,%ymm6
	vpaddd	%ymm5,%ymm12,%ymm12
	vpxor	%ymm3,%ymm12,%ymm3
	vpslld	$12,%ymm3,%ymm14
	vpsrld	$20,%ymm3,%ymm3
	vpor	%ymm3,%ymm14,%ymm3
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm6,%ymm13,%ymm13
	vpxor	%ymm0,%ymm13,%ymm0
	vpslld	$12,%ymm0,%ymm15
	vpsrld	$20,%ymm0,%ymm0
	vpor	%ymm0,%ymm15,%ymm0
	vpaddd	%ymm3,%ymm10,%ymm10
	vpxor	%ymm5,%ymm10,%ymm5
	vpshufb	%ymm14,%ymm5,%ymm5
	vpaddd	%ymm0,%ymm11,%ymm11
	vpxor	%ymm6,%ymm11,%ymm6
	vpshufb	%ymm14,%ymm6,%ymm6
	vpaddd	%ymm5,%ymm12,%ymm12
	vpxor	%ymm3,%ymm12,%ymm3
	vpslld	$7,%ymm3,%ymm15
	vpsrld	$25,%ymm3,%ymm3
	vpor	%ymm3,%ymm15,%ymm3
	vbroadcasti128	(%r9),%ymm15
	vpaddd	%ymm6,%ymm13,%ymm13
	vpxor	%ymm0,%ymm13,%ymm0
	vpslld	$7,%ymm0,%ymm14
	vpsrld	$25,%ymm0,%ymm0
	vpor	%ymm0,%ymm14,%ymm0
	dec		%eax
	jnz		.Loop8x

	lea		0x200(%rsp),%rax	# size optimization
	vpaddd		0x80-0x100(%rcx),%ymm8,%ymm8	# accumulate key
	vpaddd		0xa0-0x100(%rcx),%ymm9,%ymm9
	vpaddd		0xc0-0x100(%rcx),%ymm10,%ymm10
	vpaddd		0xe0-0x100(%rcx),%ymm11,%ymm11

	vpunpckldq	%ymm9,%ymm8,%ymm14		# "de-interlace" data
	vpunpckldq	%ymm11,%ymm10,%ymm15
	vpunpckhdq	%ymm9,%ymm8,%ymm8
	vpunpckhdq	%ymm11,%ymm10,%ymm10
	vpunpcklqdq	%ymm15,%ymm14,%ymm9		# "a0"
	vpunpckhqdq	%ymm15,%ymm14,%ymm14		# "a1"
	vpunpcklqdq	%ymm10,%ymm8,%ymm11		# "a2"
	vpunpckhqdq	%ymm10,%ymm8,%ymm8		# "a3"
	vpaddd		0x100-0x100(%rcx),%ymm0,%ymm0
	vpaddd		0x120-0x100(%rcx),%ymm1,%ymm1
	vpaddd		0x140-0x100(%rcx),%ymm2,%ymm2
	vpaddd		0x160-0x100(%rcx),%ymm3,%ymm3

	vpunpckldq	%ymm1,%ymm0,%ymm10
	vpunpckldq	%ymm3,%ymm2,%ymm15
	vpunpckhdq	%ymm1,%ymm0,%ymm0
	vpunpckhdq	%ymm3,%ymm2,%ymm2
	vpunpcklqdq	%ymm15,%ymm10,%ymm1		# "b0"
	vpunpckhqdq	%ymm15,%ymm10,%ymm10		# "b1"
	vpunpcklqdq	%ymm2,%ymm0,%ymm3		# "b2"
	vpunpckhqdq	%ymm2,%ymm0,%ymm0		# "b3"
	vperm2i128	$0x20,%ymm1,%ymm9,%ymm15	# "de-interlace" further
	vperm2i128	$0x31,%ymm1,%ymm9,%ymm1
	vperm2i128	$0x20,%ymm10,%ymm14,%ymm9
	vperm2i128	$0x31,%ymm10,%ymm14,%ymm10
	vperm2i128	$0x20,%ymm3,%ymm11,%ymm14
	vperm2i128	$0x31,%ymm3,%ymm11,%ymm3
	vperm2i128	$0x20,%ymm0,%ymm8,%ymm11
	vperm2i128	$0x31,%ymm0,%ymm8,%ymm0
	vmovdqa		%ymm15,0x00(%rsp)		# offload 
	vmovdqa		%ymm9,0x20(%rsp)
	vmovdqa		0x40(%rsp),%ymm15		# %ymm15
	vmovdqa		0x60(%rsp),%ymm9		# %ymm9

	vpaddd		0x180-0x200(%rax),%ymm12,%ymm12
	vpaddd		0x1a0-0x200(%rax),%ymm13,%ymm13
	vpaddd		0x1c0-0x200(%rax),%ymm15,%ymm15
	vpaddd		0x1e0-0x200(%rax),%ymm9,%ymm9

	vpunpckldq	%ymm13,%ymm12,%ymm2
	vpunpckldq	%ymm9,%ymm15,%ymm8
	vpunpckhdq	%ymm13,%ymm12,%ymm12
	vpunpckhdq	%ymm9,%ymm15,%ymm15
	vpunpcklqdq	%ymm8,%ymm2,%ymm13		# "c0"
	vpunpckhqdq	%ymm8,%ymm2,%ymm2		# "c1"
	vpunpcklqdq	%ymm15,%ymm12,%ymm9		# "c2"
	vpunpckhqdq	%ymm15,%ymm12,%ymm12		# "c3"
	vpaddd		0x200-0x200(%rax),%ymm4,%ymm4
	vpaddd		0x220-0x200(%rax),%ymm5,%ymm5
	vpaddd		0x240-0x200(%rax),%ymm6,%ymm6
	vpaddd		0x260-0x200(%rax),%ymm7,%ymm7

	vpunpckldq	%ymm5,%ymm4,%ymm15
	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm4
	vpunpckhdq	%ymm7,%ymm6,%ymm6
	vpunpcklqdq	%ymm8,%ymm15,%ymm5		# "d0"
	vpunpckhqdq	%ymm8,%ymm15,%ymm15		# "d1"
	vpunpcklqdq	%ymm6,%ymm4,%ymm7		# "d2"
	vpunpckhqdq	%ymm6,%ymm4,%ymm4		# "d3"
	vperm2i128	$0x20,%ymm5,%ymm13,%ymm8	# "de-interlace" further
	vperm2i128	$0x31,%ymm5,%ymm13,%ymm5
	vperm2i128	$0x20,%ymm15,%ymm2,%ymm13
	vperm2i128	$0x31,%ymm15,%ymm2,%ymm15
	vperm2i128	$0x20,%ymm7,%ymm9,%ymm2
	vperm2i128	$0x31,%ymm7,%ymm9,%ymm7
	vperm2i128	$0x20,%ymm4,%ymm12,%ymm9
	vperm2i128	$0x31,%ymm4,%ymm12,%ymm4
	vmovdqa		0x00(%rsp),%ymm6		#  was offloaded, remember?
	vmovdqa		0x20(%rsp),%ymm12

	cmp		$64*8,%rdx
	jb		.Ltail8x

	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	lea		0x80(%rsi),%rsi		# size optimization
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	lea		0x80(%rdi),%rdi		# size optimization

	vpxor		0x00(%rsi),%ymm12,%ymm12
	vpxor		0x20(%rsi),%ymm13,%ymm13
	vpxor		0x40(%rsi),%ymm10,%ymm10
	vpxor		0x60(%rsi),%ymm15,%ymm15
	lea		0x80(%rsi),%rsi		# size optimization
	vmovdqu		%ymm12,0x00(%rdi)
	vmovdqu		%ymm13,0x20(%rdi)
	vmovdqu		%ymm10,0x40(%rdi)
	vmovdqu		%ymm15,0x60(%rdi)
	lea		0x80(%rdi),%rdi		# size optimization

	vpxor		0x00(%rsi),%ymm14,%ymm14
	vpxor		0x20(%rsi),%ymm2,%ymm2
	vpxor		0x40(%rsi),%ymm3,%ymm3
	vpxor		0x60(%rsi),%ymm7,%ymm7
	lea		0x80(%rsi),%rsi		# size optimization
	vmovdqu		%ymm14,0x00(%rdi)
	vmovdqu		%ymm2,0x20(%rdi)
	vmovdqu		%ymm3,0x40(%rdi)
	vmovdqu		%ymm7,0x60(%rdi)
	lea		0x80(%rdi),%rdi		# size optimization

	vpxor		0x00(%rsi),%ymm11,%ymm11
	vpxor		0x20(%rsi),%ymm9,%ymm9
	vpxor		0x40(%rsi),%ymm0,%ymm0
	vpxor		0x60(%rsi),%ymm4,%ymm4
	lea		0x80(%rsi),%rsi		# size optimization
	vmovdqu		%ymm11,0x00(%rdi)
	vmovdqu		%ymm9,0x20(%rdi)
	vmovdqu		%ymm0,0x40(%rdi)
	vmovdqu		%ymm4,0x60(%rdi)
	lea		0x80(%rdi),%rdi		# size optimization

	sub		$64*8,%rdx
	jnz		.Loop_outer8x

	jmp		.Ldone8x

.Ltail8x:
	cmp		$448,%rdx
	jae		.L448_or_more8x
	cmp		$384,%rdx
	jae		.L384_or_more8x
	cmp		$320,%rdx
	jae		.L320_or_more8x
	cmp		$256,%rdx
	jae		.L256_or_more8x
	cmp		$192,%rdx
	jae		.L192_or_more8x
	cmp		$128,%rdx
	jae		.L128_or_more8x
	cmp		$64,%rdx
	jae		.L64_or_more8x

	xor		%r9,%r9
	vmovdqa		%ymm6,0x00(%rsp)
	vmovdqa		%ymm8,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L64_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	je		.Ldone8x

	lea		0x40(%rsi),%rsi		# inp+=64*1
	xor		%r9,%r9
	vmovdqa		%ymm1,0x00(%rsp)
	lea		0x40(%rdi),%rdi		# out+=64*1
	sub		$64,%rdx		# len-=64*1
	vmovdqa		%ymm5,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L128_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	je		.Ldone8x

	lea		0x80(%rsi),%rsi		# inp+=64*2
	xor		%r9,%r9
	vmovdqa		%ymm12,0x00(%rsp)
	lea		0x80(%rdi),%rdi		# out+=64*2
	sub		$128,%rdx		# len-=64*2
	vmovdqa		%ymm13,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L192_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vpxor		0x80(%rsi),%ymm12,%ymm12
	vpxor		0xa0(%rsi),%ymm13,%ymm13
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	vmovdqu		%ymm12,0x80(%rdi)
	vmovdqu		%ymm13,0xa0(%rdi)
	je		.Ldone8x

	lea		0xc0(%rsi),%rsi		# inp+=64*3
	xor		%r9,%r9
	vmovdqa		%ymm10,0x00(%rsp)
	lea		0xc0(%rdi),%rdi		# out+=64*3
	sub		$192,%rdx		# len-=64*3
	vmovdqa		%ymm15,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L256_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vpxor		0x80(%rsi),%ymm12,%ymm12
	vpxor		0xa0(%rsi),%ymm13,%ymm13
	vpxor		0xc0(%rsi),%ymm10,%ymm10
	vpxor		0xe0(%rsi),%ymm15,%ymm15
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	vmovdqu		%ymm12,0x80(%rdi)
	vmovdqu		%ymm13,0xa0(%rdi)
	vmovdqu		%ymm10,0xc0(%rdi)
	vmovdqu		%ymm15,0xe0(%rdi)
	je		.Ldone8x

	lea		0x100(%rsi),%rsi	# inp+=64*4
	xor		%r9,%r9
	vmovdqa		%ymm14,0x00(%rsp)
	lea		0x100(%rdi),%rdi	# out+=64*4
	sub		$256,%rdx		# len-=64*4
	vmovdqa		%ymm2,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L320_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vpxor		0x80(%rsi),%ymm12,%ymm12
	vpxor		0xa0(%rsi),%ymm13,%ymm13
	vpxor		0xc0(%rsi),%ymm10,%ymm10
	vpxor		0xe0(%rsi),%ymm15,%ymm15
	vpxor		0x100(%rsi),%ymm14,%ymm14
	vpxor		0x120(%rsi),%ymm2,%ymm2
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	vmovdqu		%ymm12,0x80(%rdi)
	vmovdqu		%ymm13,0xa0(%rdi)
	vmovdqu		%ymm10,0xc0(%rdi)
	vmovdqu		%ymm15,0xe0(%rdi)
	vmovdqu		%ymm14,0x100(%rdi)
	vmovdqu		%ymm2,0x120(%rdi)
	je		.Ldone8x

	lea		0x140(%rsi),%rsi	# inp+=64*5
	xor		%r9,%r9
	vmovdqa		%ymm3,0x00(%rsp)
	lea		0x140(%rdi),%rdi	# out+=64*5
	sub		$320,%rdx		# len-=64*5
	vmovdqa		%ymm7,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L384_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vpxor		0x80(%rsi),%ymm12,%ymm12
	vpxor		0xa0(%rsi),%ymm13,%ymm13
	vpxor		0xc0(%rsi),%ymm10,%ymm10
	vpxor		0xe0(%rsi),%ymm15,%ymm15
	vpxor		0x100(%rsi),%ymm14,%ymm14
	vpxor		0x120(%rsi),%ymm2,%ymm2
	vpxor		0x140(%rsi),%ymm3,%ymm3
	vpxor		0x160(%rsi),%ymm7,%ymm7
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	vmovdqu		%ymm12,0x80(%rdi)
	vmovdqu		%ymm13,0xa0(%rdi)
	vmovdqu		%ymm10,0xc0(%rdi)
	vmovdqu		%ymm15,0xe0(%rdi)
	vmovdqu		%ymm14,0x100(%rdi)
	vmovdqu		%ymm2,0x120(%rdi)
	vmovdqu		%ymm3,0x140(%rdi)
	vmovdqu		%ymm7,0x160(%rdi)
	je		.Ldone8x

	lea		0x180(%rsi),%rsi	# inp+=64*6
	xor		%r9,%r9
	vmovdqa		%ymm11,0x00(%rsp)
	lea		0x180(%rdi),%rdi	# out+=64*6
	sub		$384,%rdx		# len-=64*6
	vmovdqa		%ymm9,0x20(%rsp)
	jmp		.Loop_tail8x

.align	32
.L448_or_more8x:
	vpxor		0x00(%rsi),%ymm6,%ymm6	# xor with input
	vpxor		0x20(%rsi),%ymm8,%ymm8
	vpxor		0x40(%rsi),%ymm1,%ymm1
	vpxor		0x60(%rsi),%ymm5,%ymm5
	vpxor		0x80(%rsi),%ymm12,%ymm12
	vpxor		0xa0(%rsi),%ymm13,%ymm13
	vpxor		0xc0(%rsi),%ymm10,%ymm10
	vpxor		0xe0(%rsi),%ymm15,%ymm15
	vpxor		0x100(%rsi),%ymm14,%ymm14
	vpxor		0x120(%rsi),%ymm2,%ymm2
	vpxor		0x140(%rsi),%ymm3,%ymm3
	vpxor		0x160(%rsi),%ymm7,%ymm7
	vpxor		0x180(%rsi),%ymm11,%ymm11
	vpxor		0x1a0(%rsi),%ymm9,%ymm9
	vmovdqu		%ymm6,0x00(%rdi)
	vmovdqu		%ymm8,0x20(%rdi)
	vmovdqu		%ymm1,0x40(%rdi)
	vmovdqu		%ymm5,0x60(%rdi)
	vmovdqu		%ymm12,0x80(%rdi)
	vmovdqu		%ymm13,0xa0(%rdi)
	vmovdqu		%ymm10,0xc0(%rdi)
	vmovdqu		%ymm15,0xe0(%rdi)
	vmovdqu		%ymm14,0x100(%rdi)
	vmovdqu		%ymm2,0x120(%rdi)
	vmovdqu		%ymm3,0x140(%rdi)
	vmovdqu		%ymm7,0x160(%rdi)
	vmovdqu		%ymm11,0x180(%rdi)
	vmovdqu		%ymm9,0x1a0(%rdi)
	je		.Ldone8x

	lea		0x1c0(%rsi),%rsi	# inp+=64*7
	xor		%r9,%r9
	vmovdqa		%ymm0,0x00(%rsp)
	lea		0x1c0(%rdi),%rdi	# out+=64*7
	sub		$448,%rdx		# len-=64*7
	vmovdqa		%ymm4,0x20(%rsp)

.Loop_tail8x:
	movzb		(%rsi,%r9),%eax
	movzb		(%rsp,%r9),%ecx
	lea		1(%r9),%r9
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r9)
	dec		%rdx
	jnz		.Loop_tail8x

.Ldone8x:
	vzeroall
	lea		-8(%r10),%rsp
.L8x_epilogue:
	ret
SYM_FUNC_END(chacha20_avx2)
#endif
#ifdef CONFIG_AS_AVX512
.align 32
SYM_FUNC_START(chacha20_avx512)
.Lchacha20_avx512:
	lea	8(%rsp),%r10		# frame pointer
	cmp	$512,%rdx
	ja	.Lchacha20_16x

	sub	$64+8,%rsp
	and $-64,%rsp
	vbroadcasti32x4	.Lsigma(%rip),%zmm0
	vbroadcasti32x4	(%rcx),%zmm1
	vbroadcasti32x4	16(%rcx),%zmm2
	vbroadcasti32x4	(%r8),%zmm3

	vmovdqa32	%zmm0,%zmm16
	vmovdqa32	%zmm1,%zmm17
	vmovdqa32	%zmm2,%zmm18
	vpaddd		.Lzeroz(%rip),%zmm3,%zmm3
	vmovdqa32	.Lfourz(%rip),%zmm20
	mov		$10,%r8	# reuse %r8
	vmovdqa32	%zmm3,%zmm19
	jmp		.Loop_avx512

.align	16
.Loop_outer_avx512:
	vmovdqa32	%zmm16,%zmm0
	vmovdqa32	%zmm17,%zmm1
	vmovdqa32	%zmm18,%zmm2
	vpaddd		%zmm20,%zmm19,%zmm3
	mov		$10,%r8
	vmovdqa32	%zmm3,%zmm19
	jmp		.Loop_avx512

.align	32
.Loop_avx512:
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$16,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$12,%zmm1,%zmm1
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$8,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$7,%zmm1,%zmm1
	vpshufd	$78,%zmm2,%zmm2
	vpshufd	$57,%zmm1,%zmm1
	vpshufd	$147,%zmm3,%zmm3
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$16,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$12,%zmm1,%zmm1
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$8,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$7,%zmm1,%zmm1
	vpshufd	$78,%zmm2,%zmm2
	vpshufd	$147,%zmm1,%zmm1
	vpshufd	$57,%zmm3,%zmm3
	dec	%r8
	jnz	.Loop_avx512
	vpaddd		%zmm16,%zmm0,%zmm0
	vpaddd		%zmm17,%zmm1,%zmm1
	vpaddd		%zmm18,%zmm2,%zmm2
	vpaddd		%zmm19,%zmm3,%zmm3

	sub		$64,%rdx
	jb		.Ltail64_avx512

	vpxor		0x00(%rsi),%xmm0,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm1,%xmm5
	vpxor		0x20(%rsi),%xmm2,%xmm6
	vpxor		0x30(%rsi),%xmm3,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	jz		.Ldone_avx512

	vextracti32x4	$1,%zmm0,%xmm4
	vextracti32x4	$1,%zmm1,%xmm5
	vextracti32x4	$1,%zmm2,%xmm6
	vextracti32x4	$1,%zmm3,%xmm7

	sub		$64,%rdx
	jb		.Ltail_avx512

	vpxor		0x00(%rsi),%xmm4,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm5,%xmm5
	vpxor		0x20(%rsi),%xmm6,%xmm6
	vpxor		0x30(%rsi),%xmm7,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	jz		.Ldone_avx512

	vextracti32x4	$2,%zmm0,%xmm4
	vextracti32x4	$2,%zmm1,%xmm5
	vextracti32x4	$2,%zmm2,%xmm6
	vextracti32x4	$2,%zmm3,%xmm7

	sub		$64,%rdx
	jb		.Ltail_avx512

	vpxor		0x00(%rsi),%xmm4,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm5,%xmm5
	vpxor		0x20(%rsi),%xmm6,%xmm6
	vpxor		0x30(%rsi),%xmm7,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	jz		.Ldone_avx512

	vextracti32x4	$3,%zmm0,%xmm4
	vextracti32x4	$3,%zmm1,%xmm5
	vextracti32x4	$3,%zmm2,%xmm6
	vextracti32x4	$3,%zmm3,%xmm7

	sub		$64,%rdx
	jb		.Ltail_avx512

	vpxor		0x00(%rsi),%xmm4,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm5,%xmm5
	vpxor		0x20(%rsi),%xmm6,%xmm6
	vpxor		0x30(%rsi),%xmm7,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	jnz		.Loop_outer_avx512

	jmp		.Ldone_avx512

.align	16
.Ltail64_avx512:
	vmovdqa		%xmm0,0x00(%rsp)
	vmovdqa		%xmm1,0x10(%rsp)
	vmovdqa		%xmm2,0x20(%rsp)
	vmovdqa		%xmm3,0x30(%rsp)
	add		$64,%rdx
	jmp		.Loop_tail_avx512

.align	16
.Ltail_avx512:
	vmovdqa		%xmm4,0x00(%rsp)
	vmovdqa		%xmm5,0x10(%rsp)
	vmovdqa		%xmm6,0x20(%rsp)
	vmovdqa		%xmm7,0x30(%rsp)
	add		$64,%rdx

.Loop_tail_avx512:
	movzb		(%rsi,%r8),%eax
	movzb		(%rsp,%r8),%ecx
	lea		1(%r8),%r8
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r8)
	dec		%rdx
	jnz		.Loop_tail_avx512

	vmovdqu32	%zmm16,0x00(%rsp)

.Ldone_avx512:
	vzeroall
	lea	-8(%r10),%rsp
.Lavx512_epilogue:
	ret
SYM_FUNC_END(chacha20_avx512)
.align 32
SYM_FUNC_START(chacha20_avx512vl)
.Lchacha20_avx512vl:
	lea	8(%rsp),%r10		# frame pointer
	cmp	$128,%rdx
	ja	.Lchacha20_8xvl

	sub	$64+8,%rsp
	and $-32,%rsp
	vbroadcasti128	.Lsigma(%rip),%ymm0
	vbroadcasti128	(%rcx),%ymm1
	vbroadcasti128	16(%rcx),%ymm2
	vbroadcasti128	(%r8),%ymm3

	vmovdqa32	%ymm0,%ymm16
	vmovdqa32	%ymm1,%ymm17
	vmovdqa32	%ymm2,%ymm18
	vpaddd		.Lzeroz(%rip),%ymm3,%ymm3
	vmovdqa32	.Ltwoy(%rip),%ymm20
	mov		$10,%r8	# reuse %r8
	vmovdqa32	%ymm3,%ymm19
	jmp		.Loop_avx512vl

.align	16
.Loop_outer_avx512vl:
	vmovdqa32	%ymm18,%ymm2
	vpaddd		%ymm20,%ymm19,%ymm3
	mov		$10,%r8
	vmovdqa32	%ymm3,%ymm19
	jmp		.Loop_avx512vl

.align	32
.Loop_avx512vl:
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$16,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$12,%ymm1,%ymm1
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$8,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$7,%ymm1,%ymm1
	vpshufd	$78,%ymm2,%ymm2
	vpshufd	$57,%ymm1,%ymm1
	vpshufd	$147,%ymm3,%ymm3
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$16,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$12,%ymm1,%ymm1
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$8,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$7,%ymm1,%ymm1
	vpshufd	$78,%ymm2,%ymm2
	vpshufd	$147,%ymm1,%ymm1
	vpshufd	$57,%ymm3,%ymm3
	dec	%r8
	jnz	.Loop_avx512vl
	vpaddd		%ymm16,%ymm0,%ymm0
	vpaddd		%ymm17,%ymm1,%ymm1
	vpaddd		%ymm18,%ymm2,%ymm2
	vpaddd		%ymm19,%ymm3,%ymm3

	sub		$64,%rdx
	jb		.Ltail64_avx512vl

	vpxor		0x00(%rsi),%xmm0,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm1,%xmm5
	vpxor		0x20(%rsi),%xmm2,%xmm6
	vpxor		0x30(%rsi),%xmm3,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	jz		.Ldone_avx512vl

	vextracti128	$1,%ymm0,%xmm4
	vextracti128	$1,%ymm1,%xmm5
	vextracti128	$1,%ymm2,%xmm6
	vextracti128	$1,%ymm3,%xmm7

	sub		$64,%rdx
	jb		.Ltail_avx512vl

	vpxor		0x00(%rsi),%xmm4,%xmm4	# xor with input
	vpxor		0x10(%rsi),%xmm5,%xmm5
	vpxor		0x20(%rsi),%xmm6,%xmm6
	vpxor		0x30(%rsi),%xmm7,%xmm7
	lea		0x40(%rsi),%rsi		# inp+=64

	vmovdqu		%xmm4,0x00(%rdi)		# write output
	vmovdqu		%xmm5,0x10(%rdi)
	vmovdqu		%xmm6,0x20(%rdi)
	vmovdqu		%xmm7,0x30(%rdi)
	lea		0x40(%rdi),%rdi		# out+=64

	vmovdqa32	%ymm16,%ymm0
	vmovdqa32	%ymm17,%ymm1
	jnz		.Loop_outer_avx512vl

	jmp		.Ldone_avx512vl

.align	16
.Ltail64_avx512vl:
	vmovdqa		%xmm0,0x00(%rsp)
	vmovdqa		%xmm1,0x10(%rsp)
	vmovdqa		%xmm2,0x20(%rsp)
	vmovdqa		%xmm3,0x30(%rsp)
	add		$64,%rdx
	jmp		.Loop_tail_avx512vl

.align	16
.Ltail_avx512vl:
	vmovdqa		%xmm4,0x00(%rsp)
	vmovdqa		%xmm5,0x10(%rsp)
	vmovdqa		%xmm6,0x20(%rsp)
	vmovdqa		%xmm7,0x30(%rsp)
	add		$64,%rdx

.Loop_tail_avx512vl:
	movzb		(%rsi,%r8),%eax
	movzb		(%rsp,%r8),%ecx
	lea		1(%r8),%r8
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r8)
	dec		%rdx
	jnz		.Loop_tail_avx512vl

	vmovdqu32	%ymm16,0x00(%rsp)
	vmovdqu32	%ymm16,0x20(%rsp)

.Ldone_avx512vl:
	vzeroall
	lea	-8(%r10),%rsp
.Lavx512vl_epilogue:
	ret
SYM_FUNC_END(chacha20_avx512vl)
.type	chacha20_16x,@function
.align	32
chacha20_16x:
.Lchacha20_16x:
	lea		8(%rsp),%r10		# frame register
	sub		$64+8,%rsp
	and		$-64,%rsp
	vzeroupper

	lea		.Lsigma(%rip),%r9
	vbroadcasti32x4	(%r9),%zmm3		# key[0]
	vbroadcasti32x4	(%rcx),%zmm7		# key[1]
	vbroadcasti32x4	16(%rcx),%zmm11		# key[2]
	vbroadcasti32x4	(%r8),%zmm15		# key[3]

	vpshufd		$0x00,%zmm3,%zmm0	# smash key by lanes...
	vpshufd		$0x55,%zmm3,%zmm1
	vpshufd		$0xaa,%zmm3,%zmm2
	vpshufd		$0xff,%zmm3,%zmm3
	vmovdqa64	%zmm0,%zmm16
	vmovdqa64	%zmm1,%zmm17
	vmovdqa64	%zmm2,%zmm18
	vmovdqa64	%zmm3,%zmm19

	vpshufd		$0x00,%zmm7,%zmm4
	vpshufd		$0x55,%zmm7,%zmm5
	vpshufd		$0xaa,%zmm7,%zmm6
	vpshufd		$0xff,%zmm7,%zmm7
	vmovdqa64	%zmm4,%zmm20
	vmovdqa64	%zmm5,%zmm21
	vmovdqa64	%zmm6,%zmm22
	vmovdqa64	%zmm7,%zmm23

	vpshufd		$0x00,%zmm11,%zmm8
	vpshufd		$0x55,%zmm11,%zmm9
	vpshufd		$0xaa,%zmm11,%zmm10
	vpshufd		$0xff,%zmm11,%zmm11
	vmovdqa64	%zmm8,%zmm24
	vmovdqa64	%zmm9,%zmm25
	vmovdqa64	%zmm10,%zmm26
	vmovdqa64	%zmm11,%zmm27

	vpshufd		$0x00,%zmm15,%zmm12
	vpshufd		$0x55,%zmm15,%zmm13
	vpshufd		$0xaa,%zmm15,%zmm14
	vpshufd		$0xff,%zmm15,%zmm15
	vpaddd		.Lincz(%rip),%zmm12,%zmm12	# don't save counters yet
	vmovdqa64	%zmm12,%zmm28
	vmovdqa64	%zmm13,%zmm29
	vmovdqa64	%zmm14,%zmm30
	vmovdqa64	%zmm15,%zmm31

	mov		$10,%eax
	jmp		.Loop16x

.align	32
.Loop_outer16x:
	vpbroadcastd	0(%r9),%zmm0		# reload key
	vpbroadcastd	4(%r9),%zmm1
	vpbroadcastd	8(%r9),%zmm2
	vpbroadcastd	12(%r9),%zmm3
	vpaddd		.Lsixteen(%rip),%zmm28,%zmm28	# next SIMD counters
	vmovdqa64	%zmm20,%zmm4
	vmovdqa64	%zmm21,%zmm5
	vmovdqa64	%zmm22,%zmm6
	vmovdqa64	%zmm23,%zmm7
	vmovdqa64	%zmm24,%zmm8
	vmovdqa64	%zmm25,%zmm9
	vmovdqa64	%zmm26,%zmm10
	vmovdqa64	%zmm27,%zmm11
	vmovdqa64	%zmm28,%zmm12
	vmovdqa64	%zmm29,%zmm13
	vmovdqa64	%zmm30,%zmm14
	vmovdqa64	%zmm31,%zmm15

	vmovdqa64	%zmm0,%zmm16
	vmovdqa64	%zmm1,%zmm17
	vmovdqa64	%zmm2,%zmm18
	vmovdqa64	%zmm3,%zmm19

	mov		$10,%eax
	jmp		.Loop16x

.align	32
.Loop16x:
	vpaddd	%zmm4,%zmm0,%zmm0
	vpaddd	%zmm5,%zmm1,%zmm1
	vpaddd	%zmm6,%zmm2,%zmm2
	vpaddd	%zmm7,%zmm3,%zmm3
	vpxord	%zmm0,%zmm12,%zmm12
	vpxord	%zmm1,%zmm13,%zmm13
	vpxord	%zmm2,%zmm14,%zmm14
	vpxord	%zmm3,%zmm15,%zmm15
	vprold	$16,%zmm12,%zmm12
	vprold	$16,%zmm13,%zmm13
	vprold	$16,%zmm14,%zmm14
	vprold	$16,%zmm15,%zmm15
	vpaddd	%zmm12,%zmm8,%zmm8
	vpaddd	%zmm13,%zmm9,%zmm9
	vpaddd	%zmm14,%zmm10,%zmm10
	vpaddd	%zmm15,%zmm11,%zmm11
	vpxord	%zmm8,%zmm4,%zmm4
	vpxord	%zmm9,%zmm5,%zmm5
	vpxord	%zmm10,%zmm6,%zmm6
	vpxord	%zmm11,%zmm7,%zmm7
	vprold	$12,%zmm4,%zmm4
	vprold	$12,%zmm5,%zmm5
	vprold	$12,%zmm6,%zmm6
	vprold	$12,%zmm7,%zmm7
	vpaddd	%zmm4,%zmm0,%zmm0
	vpaddd	%zmm5,%zmm1,%zmm1
	vpaddd	%zmm6,%zmm2,%zmm2
	vpaddd	%zmm7,%zmm3,%zmm3
	vpxord	%zmm0,%zmm12,%zmm12
	vpxord	%zmm1,%zmm13,%zmm13
	vpxord	%zmm2,%zmm14,%zmm14
	vpxord	%zmm3,%zmm15,%zmm15
	vprold	$8,%zmm12,%zmm12
	vprold	$8,%zmm13,%zmm13
	vprold	$8,%zmm14,%zmm14
	vprold	$8,%zmm15,%zmm15
	vpaddd	%zmm12,%zmm8,%zmm8
	vpaddd	%zmm13,%zmm9,%zmm9
	vpaddd	%zmm14,%zmm10,%zmm10
	vpaddd	%zmm15,%zmm11,%zmm11
	vpxord	%zmm8,%zmm4,%zmm4
	vpxord	%zmm9,%zmm5,%zmm5
	vpxord	%zmm10,%zmm6,%zmm6
	vpxord	%zmm11,%zmm7,%zmm7
	vprold	$7,%zmm4,%zmm4
	vprold	$7,%zmm5,%zmm5
	vprold	$7,%zmm6,%zmm6
	vprold	$7,%zmm7,%zmm7
	vpaddd	%zmm5,%zmm0,%zmm0
	vpaddd	%zmm6,%zmm1,%zmm1
	vpaddd	%zmm7,%zmm2,%zmm2
	vpaddd	%zmm4,%zmm3,%zmm3
	vpxord	%zmm0,%zmm15,%zmm15
	vpxord	%zmm1,%zmm12,%zmm12
	vpxord	%zmm2,%zmm13,%zmm13
	vpxord	%zmm3,%zmm14,%zmm14
	vprold	$16,%zmm15,%zmm15
	vprold	$16,%zmm12,%zmm12
	vprold	$16,%zmm13,%zmm13
	vprold	$16,%zmm14,%zmm14
	vpaddd	%zmm15,%zmm10,%zmm10
	vpaddd	%zmm12,%zmm11,%zmm11
	vpaddd	%zmm13,%zmm8,%zmm8
	vpaddd	%zmm14,%zmm9,%zmm9
	vpxord	%zmm10,%zmm5,%zmm5
	vpxord	%zmm11,%zmm6,%zmm6
	vpxord	%zmm8,%zmm7,%zmm7
	vpxord	%zmm9,%zmm4,%zmm4
	vprold	$12,%zmm5,%zmm5
	vprold	$12,%zmm6,%zmm6
	vprold	$12,%zmm7,%zmm7
	vprold	$12,%zmm4,%zmm4
	vpaddd	%zmm5,%zmm0,%zmm0
	vpaddd	%zmm6,%zmm1,%zmm1
	vpaddd	%zmm7,%zmm2,%zmm2
	vpaddd	%zmm4,%zmm3,%zmm3
	vpxord	%zmm0,%zmm15,%zmm15
	vpxord	%zmm1,%zmm12,%zmm12
	vpxord	%zmm2,%zmm13,%zmm13
	vpxord	%zmm3,%zmm14,%zmm14
	vprold	$8,%zmm15,%zmm15
	vprold	$8,%zmm12,%zmm12
	vprold	$8,%zmm13,%zmm13
	vprold	$8,%zmm14,%zmm14
	vpaddd	%zmm15,%zmm10,%zmm10
	vpaddd	%zmm12,%zmm11,%zmm11
	vpaddd	%zmm13,%zmm8,%zmm8
	vpaddd	%zmm14,%zmm9,%zmm9
	vpxord	%zmm10,%zmm5,%zmm5
	vpxord	%zmm11,%zmm6,%zmm6
	vpxord	%zmm8,%zmm7,%zmm7
	vpxord	%zmm9,%zmm4,%zmm4
	vprold	$7,%zmm5,%zmm5
	vprold	$7,%zmm6,%zmm6
	vprold	$7,%zmm7,%zmm7
	vprold	$7,%zmm4,%zmm4
	dec		%eax
	jnz		.Loop16x

	vpaddd		%zmm16,%zmm0,%zmm0	# accumulate key
	vpaddd		%zmm17,%zmm1,%zmm1
	vpaddd		%zmm18,%zmm2,%zmm2
	vpaddd		%zmm19,%zmm3,%zmm3

	vpunpckldq	%zmm1,%zmm0,%zmm18		# "de-interlace" data
	vpunpckldq	%zmm3,%zmm2,%zmm19
	vpunpckhdq	%zmm1,%zmm0,%zmm0
	vpunpckhdq	%zmm3,%zmm2,%zmm2
	vpunpcklqdq	%zmm19,%zmm18,%zmm1		# "a0"
	vpunpckhqdq	%zmm19,%zmm18,%zmm18		# "a1"
	vpunpcklqdq	%zmm2,%zmm0,%zmm3		# "a2"
	vpunpckhqdq	%zmm2,%zmm0,%zmm0		# "a3"
	vpaddd		%zmm20,%zmm4,%zmm4
	vpaddd		%zmm21,%zmm5,%zmm5
	vpaddd		%zmm22,%zmm6,%zmm6
	vpaddd		%zmm23,%zmm7,%zmm7

	vpunpckldq	%zmm5,%zmm4,%zmm2
	vpunpckldq	%zmm7,%zmm6,%zmm19
	vpunpckhdq	%zmm5,%zmm4,%zmm4
	vpunpckhdq	%zmm7,%zmm6,%zmm6
	vpunpcklqdq	%zmm19,%zmm2,%zmm5		# "b0"
	vpunpckhqdq	%zmm19,%zmm2,%zmm2		# "b1"
	vpunpcklqdq	%zmm6,%zmm4,%zmm7		# "b2"
	vpunpckhqdq	%zmm6,%zmm4,%zmm4		# "b3"
	vshufi32x4	$0x44,%zmm5,%zmm1,%zmm19	# "de-interlace" further
	vshufi32x4	$0xee,%zmm5,%zmm1,%zmm5
	vshufi32x4	$0x44,%zmm2,%zmm18,%zmm1
	vshufi32x4	$0xee,%zmm2,%zmm18,%zmm2
	vshufi32x4	$0x44,%zmm7,%zmm3,%zmm18
	vshufi32x4	$0xee,%zmm7,%zmm3,%zmm7
	vshufi32x4	$0x44,%zmm4,%zmm0,%zmm3
	vshufi32x4	$0xee,%zmm4,%zmm0,%zmm4
	vpaddd		%zmm24,%zmm8,%zmm8
	vpaddd		%zmm25,%zmm9,%zmm9
	vpaddd		%zmm26,%zmm10,%zmm10
	vpaddd		%zmm27,%zmm11,%zmm11

	vpunpckldq	%zmm9,%zmm8,%zmm6
	vpunpckldq	%zmm11,%zmm10,%zmm0
	vpunpckhdq	%zmm9,%zmm8,%zmm8
	vpunpckhdq	%zmm11,%zmm10,%zmm10
	vpunpcklqdq	%zmm0,%zmm6,%zmm9		# "c0"
	vpunpckhqdq	%zmm0,%zmm6,%zmm6		# "c1"
	vpunpcklqdq	%zmm10,%zmm8,%zmm11		# "c2"
	vpunpckhqdq	%zmm10,%zmm8,%zmm8		# "c3"
	vpaddd		%zmm28,%zmm12,%zmm12
	vpaddd		%zmm29,%zmm13,%zmm13
	vpaddd		%zmm30,%zmm14,%zmm14
	vpaddd		%zmm31,%zmm15,%zmm15

	vpunpckldq	%zmm13,%zmm12,%zmm10
	vpunpckldq	%zmm15,%zmm14,%zmm0
	vpunpckhdq	%zmm13,%zmm12,%zmm12
	vpunpckhdq	%zmm15,%zmm14,%zmm14
	vpunpcklqdq	%zmm0,%zmm10,%zmm13		# "d0"
	vpunpckhqdq	%zmm0,%zmm10,%zmm10		# "d1"
	vpunpcklqdq	%zmm14,%zmm12,%zmm15		# "d2"
	vpunpckhqdq	%zmm14,%zmm12,%zmm12		# "d3"
	vshufi32x4	$0x44,%zmm13,%zmm9,%zmm0	# "de-interlace" further
	vshufi32x4	$0xee,%zmm13,%zmm9,%zmm13
	vshufi32x4	$0x44,%zmm10,%zmm6,%zmm9
	vshufi32x4	$0xee,%zmm10,%zmm6,%zmm10
	vshufi32x4	$0x44,%zmm15,%zmm11,%zmm6
	vshufi32x4	$0xee,%zmm15,%zmm11,%zmm15
	vshufi32x4	$0x44,%zmm12,%zmm8,%zmm11
	vshufi32x4	$0xee,%zmm12,%zmm8,%zmm12
	vshufi32x4	$0x88,%zmm0,%zmm19,%zmm16	# "de-interlace" further
	vshufi32x4	$0xdd,%zmm0,%zmm19,%zmm19
	 vshufi32x4	$0x88,%zmm13,%zmm5,%zmm0
	 vshufi32x4	$0xdd,%zmm13,%zmm5,%zmm13
	vshufi32x4	$0x88,%zmm9,%zmm1,%zmm17
	vshufi32x4	$0xdd,%zmm9,%zmm1,%zmm1
	 vshufi32x4	$0x88,%zmm10,%zmm2,%zmm9
	 vshufi32x4	$0xdd,%zmm10,%zmm2,%zmm10
	vshufi32x4	$0x88,%zmm6,%zmm18,%zmm14
	vshufi32x4	$0xdd,%zmm6,%zmm18,%zmm18
	 vshufi32x4	$0x88,%zmm15,%zmm7,%zmm6
	 vshufi32x4	$0xdd,%zmm15,%zmm7,%zmm15
	vshufi32x4	$0x88,%zmm11,%zmm3,%zmm8
	vshufi32x4	$0xdd,%zmm11,%zmm3,%zmm3
	 vshufi32x4	$0x88,%zmm12,%zmm4,%zmm11
	 vshufi32x4	$0xdd,%zmm12,%zmm4,%zmm12
	cmp		$64*16,%rdx
	jb		.Ltail16x

	vpxord		0x00(%rsi),%zmm16,%zmm16	# xor with input
	vpxord		0x40(%rsi),%zmm17,%zmm17
	vpxord		0x80(%rsi),%zmm14,%zmm14
	vpxord		0xc0(%rsi),%zmm8,%zmm8
	vmovdqu32	%zmm16,0x00(%rdi)
	vmovdqu32	%zmm17,0x40(%rdi)
	vmovdqu32	%zmm14,0x80(%rdi)
	vmovdqu32	%zmm8,0xc0(%rdi)

	vpxord		0x100(%rsi),%zmm19,%zmm19
	vpxord		0x140(%rsi),%zmm1,%zmm1
	vpxord		0x180(%rsi),%zmm18,%zmm18
	vpxord		0x1c0(%rsi),%zmm3,%zmm3
	vmovdqu32	%zmm19,0x100(%rdi)
	vmovdqu32	%zmm1,0x140(%rdi)
	vmovdqu32	%zmm18,0x180(%rdi)
	vmovdqu32	%zmm3,0x1c0(%rdi)

	vpxord		0x200(%rsi),%zmm0,%zmm0
	vpxord		0x240(%rsi),%zmm9,%zmm9
	vpxord		0x280(%rsi),%zmm6,%zmm6
	vpxord		0x2c0(%rsi),%zmm11,%zmm11
	vmovdqu32	%zmm0,0x200(%rdi)
	vmovdqu32	%zmm9,0x240(%rdi)
	vmovdqu32	%zmm6,0x280(%rdi)
	vmovdqu32	%zmm11,0x2c0(%rdi)

	vpxord		0x300(%rsi),%zmm13,%zmm13
	vpxord		0x340(%rsi),%zmm10,%zmm10
	vpxord		0x380(%rsi),%zmm15,%zmm15
	vpxord		0x3c0(%rsi),%zmm12,%zmm12
	lea		0x400(%rsi),%rsi
	vmovdqu32	%zmm13,0x300(%rdi)
	vmovdqu32	%zmm10,0x340(%rdi)
	vmovdqu32	%zmm15,0x380(%rdi)
	vmovdqu32	%zmm12,0x3c0(%rdi)
	lea		0x400(%rdi),%rdi

	sub		$64*16,%rdx
	jnz		.Loop_outer16x

	jmp		.Ldone16x

.align	32
.Ltail16x:
	xor		%r9,%r9
	sub		%rsi,%rdi
	cmp		$64*1,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm16,%zmm16	# xor with input
	vmovdqu32	%zmm16,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm17,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*2,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm17,%zmm17
	vmovdqu32	%zmm17,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm14,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*3,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm14,%zmm14
	vmovdqu32	%zmm14,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm8,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*4,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm8,%zmm8
	vmovdqu32	%zmm8,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm19,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*5,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm19,%zmm19
	vmovdqu32	%zmm19,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm1,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*6,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm1,%zmm1
	vmovdqu32	%zmm1,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm18,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*7,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm18,%zmm18
	vmovdqu32	%zmm18,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm3,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*8,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm3,%zmm3
	vmovdqu32	%zmm3,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm0,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*9,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm0,%zmm0
	vmovdqu32	%zmm0,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm9,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*10,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm9,%zmm9
	vmovdqu32	%zmm9,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm6,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*11,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm6,%zmm6
	vmovdqu32	%zmm6,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm11,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*12,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm11,%zmm11
	vmovdqu32	%zmm11,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm13,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*13,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm13,%zmm13
	vmovdqu32	%zmm13,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm10,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*14,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm10,%zmm10
	vmovdqu32	%zmm10,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm15,%zmm16
	lea		64(%rsi),%rsi

	cmp		$64*15,%rdx
	jb		.Less_than_64_16x
	vpxord		(%rsi),%zmm15,%zmm15
	vmovdqu32	%zmm15,(%rdi,%rsi)
	je		.Ldone16x
	vmovdqa32	%zmm12,%zmm16
	lea		64(%rsi),%rsi

.Less_than_64_16x:
	vmovdqa32	%zmm16,0x00(%rsp)
	lea		(%rdi,%rsi),%rdi
	and		$63,%rdx

.Loop_tail16x:
	movzb		(%rsi,%r9),%eax
	movzb		(%rsp,%r9),%ecx
	lea		1(%r9),%r9
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r9)
	dec		%rdx
	jnz		.Loop_tail16x

	vpxord		%zmm16,%zmm16,%zmm16
	vmovdqa32	%zmm16,0(%rsp)

.Ldone16x:
	vzeroall
	lea		-8(%r10),%rsp
.L16x_epilogue:
	ret
.size	chacha20_16x,.-chacha20_16x
.type	chacha20_8xvl,@function
.align	32
chacha20_8xvl:
.Lchacha20_8xvl:
	lea		8(%rsp),%r10		# frame register
	sub		$64+8,%rsp
	and		$-64,%rsp
	vzeroupper

	lea		.Lsigma(%rip),%r9
	vbroadcasti128	(%r9),%ymm3		# key[0]
	vbroadcasti128	(%rcx),%ymm7		# key[1]
	vbroadcasti128	16(%rcx),%ymm11		# key[2]
	vbroadcasti128	(%r8),%ymm15		# key[3]

	vpshufd		$0x00,%ymm3,%ymm0	# smash key by lanes...
	vpshufd		$0x55,%ymm3,%ymm1
	vpshufd		$0xaa,%ymm3,%ymm2
	vpshufd		$0xff,%ymm3,%ymm3
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm1,%ymm17
	vmovdqa64	%ymm2,%ymm18
	vmovdqa64	%ymm3,%ymm19

	vpshufd		$0x00,%ymm7,%ymm4
	vpshufd		$0x55,%ymm7,%ymm5
	vpshufd		$0xaa,%ymm7,%ymm6
	vpshufd		$0xff,%ymm7,%ymm7
	vmovdqa64	%ymm4,%ymm20
	vmovdqa64	%ymm5,%ymm21
	vmovdqa64	%ymm6,%ymm22
	vmovdqa64	%ymm7,%ymm23

	vpshufd		$0x00,%ymm11,%ymm8
	vpshufd		$0x55,%ymm11,%ymm9
	vpshufd		$0xaa,%ymm11,%ymm10
	vpshufd		$0xff,%ymm11,%ymm11
	vmovdqa64	%ymm8,%ymm24
	vmovdqa64	%ymm9,%ymm25
	vmovdqa64	%ymm10,%ymm26
	vmovdqa64	%ymm11,%ymm27

	vpshufd		$0x00,%ymm15,%ymm12
	vpshufd		$0x55,%ymm15,%ymm13
	vpshufd		$0xaa,%ymm15,%ymm14
	vpshufd		$0xff,%ymm15,%ymm15
	vpaddd		.Lincy(%rip),%ymm12,%ymm12	# don't save counters yet
	vmovdqa64	%ymm12,%ymm28
	vmovdqa64	%ymm13,%ymm29
	vmovdqa64	%ymm14,%ymm30
	vmovdqa64	%ymm15,%ymm31

	mov		$10,%eax
	jmp		.Loop8xvl

.align	32
.Loop_outer8xvl:
	#vpbroadcastd	0(%r9),%ymm0		# reload key
	#vpbroadcastd	4(%r9),%ymm1
	vpbroadcastd	8(%r9),%ymm2
	vpbroadcastd	12(%r9),%ymm3
	vpaddd		.Leight(%rip),%ymm28,%ymm28	# next SIMD counters
	vmovdqa64	%ymm20,%ymm4
	vmovdqa64	%ymm21,%ymm5
	vmovdqa64	%ymm22,%ymm6
	vmovdqa64	%ymm23,%ymm7
	vmovdqa64	%ymm24,%ymm8
	vmovdqa64	%ymm25,%ymm9
	vmovdqa64	%ymm26,%ymm10
	vmovdqa64	%ymm27,%ymm11
	vmovdqa64	%ymm28,%ymm12
	vmovdqa64	%ymm29,%ymm13
	vmovdqa64	%ymm30,%ymm14
	vmovdqa64	%ymm31,%ymm15

	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm1,%ymm17
	vmovdqa64	%ymm2,%ymm18
	vmovdqa64	%ymm3,%ymm19

	mov		$10,%eax
	jmp		.Loop8xvl

.align	32
.Loop8xvl:
	vpaddd	%ymm4,%ymm0,%ymm0
	vpaddd	%ymm5,%ymm1,%ymm1
	vpaddd	%ymm6,%ymm2,%ymm2
	vpaddd	%ymm7,%ymm3,%ymm3
	vpxor	%ymm0,%ymm12,%ymm12
	vpxor	%ymm1,%ymm13,%ymm13
	vpxor	%ymm2,%ymm14,%ymm14
	vpxor	%ymm3,%ymm15,%ymm15
	vprold	$16,%ymm12,%ymm12
	vprold	$16,%ymm13,%ymm13
	vprold	$16,%ymm14,%ymm14
	vprold	$16,%ymm15,%ymm15
	vpaddd	%ymm12,%ymm8,%ymm8
	vpaddd	%ymm13,%ymm9,%ymm9
	vpaddd	%ymm14,%ymm10,%ymm10
	vpaddd	%ymm15,%ymm11,%ymm11
	vpxor	%ymm8,%ymm4,%ymm4
	vpxor	%ymm9,%ymm5,%ymm5
	vpxor	%ymm10,%ymm6,%ymm6
	vpxor	%ymm11,%ymm7,%ymm7
	vprold	$12,%ymm4,%ymm4
	vprold	$12,%ymm5,%ymm5
	vprold	$12,%ymm6,%ymm6
	vprold	$12,%ymm7,%ymm7
	vpaddd	%ymm4,%ymm0,%ymm0
	vpaddd	%ymm5,%ymm1,%ymm1
	vpaddd	%ymm6,%ymm2,%ymm2
	vpaddd	%ymm7,%ymm3,%ymm3
	vpxor	%ymm0,%ymm12,%ymm12
	vpxor	%ymm1,%ymm13,%ymm13
	vpxor	%ymm2,%ymm14,%ymm14
	vpxor	%ymm3,%ymm15,%ymm15
	vprold	$8,%ymm12,%ymm12
	vprold	$8,%ymm13,%ymm13
	vprold	$8,%ymm14,%ymm14
	vprold	$8,%ymm15,%ymm15
	vpaddd	%ymm12,%ymm8,%ymm8
	vpaddd	%ymm13,%ymm9,%ymm9
	vpaddd	%ymm14,%ymm10,%ymm10
	vpaddd	%ymm15,%ymm11,%ymm11
	vpxor	%ymm8,%ymm4,%ymm4
	vpxor	%ymm9,%ymm5,%ymm5
	vpxor	%ymm10,%ymm6,%ymm6
	vpxor	%ymm11,%ymm7,%ymm7
	vprold	$7,%ymm4,%ymm4
	vprold	$7,%ymm5,%ymm5
	vprold	$7,%ymm6,%ymm6
	vprold	$7,%ymm7,%ymm7
	vpaddd	%ymm5,%ymm0,%ymm0
	vpaddd	%ymm6,%ymm1,%ymm1
	vpaddd	%ymm7,%ymm2,%ymm2
	vpaddd	%ymm4,%ymm3,%ymm3
	vpxor	%ymm0,%ymm15,%ymm15
	vpxor	%ymm1,%ymm12,%ymm12
	vpxor	%ymm2,%ymm13,%ymm13
	vpxor	%ymm3,%ymm14,%ymm14
	vprold	$16,%ymm15,%ymm15
	vprold	$16,%ymm12,%ymm12
	vprold	$16,%ymm13,%ymm13
	vprold	$16,%ymm14,%ymm14
	vpaddd	%ymm15,%ymm10,%ymm10
	vpaddd	%ymm12,%ymm11,%ymm11
	vpaddd	%ymm13,%ymm8,%ymm8
	vpaddd	%ymm14,%ymm9,%ymm9
	vpxor	%ymm10,%ymm5,%ymm5
	vpxor	%ymm11,%ymm6,%ymm6
	vpxor	%ymm8,%ymm7,%ymm7
	vpxor	%ymm9,%ymm4,%ymm4
	vprold	$12,%ymm5,%ymm5
	vprold	$12,%ymm6,%ymm6
	vprold	$12,%ymm7,%ymm7
	vprold	$12,%ymm4,%ymm4
	vpaddd	%ymm5,%ymm0,%ymm0
	vpaddd	%ymm6,%ymm1,%ymm1
	vpaddd	%ymm7,%ymm2,%ymm2
	vpaddd	%ymm4,%ymm3,%ymm3
	vpxor	%ymm0,%ymm15,%ymm15
	vpxor	%ymm1,%ymm12,%ymm12
	vpxor	%ymm2,%ymm13,%ymm13
	vpxor	%ymm3,%ymm14,%ymm14
	vprold	$8,%ymm15,%ymm15
	vprold	$8,%ymm12,%ymm12
	vprold	$8,%ymm13,%ymm13
	vprold	$8,%ymm14,%ymm14
	vpaddd	%ymm15,%ymm10,%ymm10
	vpaddd	%ymm12,%ymm11,%ymm11
	vpaddd	%ymm13,%ymm8,%ymm8
	vpaddd	%ymm14,%ymm9,%ymm9
	vpxor	%ymm10,%ymm5,%ymm5
	vpxor	%ymm11,%ymm6,%ymm6
	vpxor	%ymm8,%ymm7,%ymm7
	vpxor	%ymm9,%ymm4,%ymm4
	vprold	$7,%ymm5,%ymm5
	vprold	$7,%ymm6,%ymm6
	vprold	$7,%ymm7,%ymm7
	vprold	$7,%ymm4,%ymm4
	dec		%eax
	jnz		.Loop8xvl

	vpaddd		%ymm16,%ymm0,%ymm0	# accumulate key
	vpaddd		%ymm17,%ymm1,%ymm1
	vpaddd		%ymm18,%ymm2,%ymm2
	vpaddd		%ymm19,%ymm3,%ymm3

	vpunpckldq	%ymm1,%ymm0,%ymm18		# "de-interlace" data
	vpunpckldq	%ymm3,%ymm2,%ymm19
	vpunpckhdq	%ymm1,%ymm0,%ymm0
	vpunpckhdq	%ymm3,%ymm2,%ymm2
	vpunpcklqdq	%ymm19,%ymm18,%ymm1		# "a0"
	vpunpckhqdq	%ymm19,%ymm18,%ymm18		# "a1"
	vpunpcklqdq	%ymm2,%ymm0,%ymm3		# "a2"
	vpunpckhqdq	%ymm2,%ymm0,%ymm0		# "a3"
	vpaddd		%ymm20,%ymm4,%ymm4
	vpaddd		%ymm21,%ymm5,%ymm5
	vpaddd		%ymm22,%ymm6,%ymm6
	vpaddd		%ymm23,%ymm7,%ymm7

	vpunpckldq	%ymm5,%ymm4,%ymm2
	vpunpckldq	%ymm7,%ymm6,%ymm19
	vpunpckhdq	%ymm5,%ymm4,%ymm4
	vpunpckhdq	%ymm7,%ymm6,%ymm6
	vpunpcklqdq	%ymm19,%ymm2,%ymm5		# "b0"
	vpunpckhqdq	%ymm19,%ymm2,%ymm2		# "b1"
	vpunpcklqdq	%ymm6,%ymm4,%ymm7		# "b2"
	vpunpckhqdq	%ymm6,%ymm4,%ymm4		# "b3"
	vshufi32x4	$0,%ymm5,%ymm1,%ymm19	# "de-interlace" further
	vshufi32x4	$3,%ymm5,%ymm1,%ymm5
	vshufi32x4	$0,%ymm2,%ymm18,%ymm1
	vshufi32x4	$3,%ymm2,%ymm18,%ymm2
	vshufi32x4	$0,%ymm7,%ymm3,%ymm18
	vshufi32x4	$3,%ymm7,%ymm3,%ymm7
	vshufi32x4	$0,%ymm4,%ymm0,%ymm3
	vshufi32x4	$3,%ymm4,%ymm0,%ymm4
	vpaddd		%ymm24,%ymm8,%ymm8
	vpaddd		%ymm25,%ymm9,%ymm9
	vpaddd		%ymm26,%ymm10,%ymm10
	vpaddd		%ymm27,%ymm11,%ymm11

	vpunpckldq	%ymm9,%ymm8,%ymm6
	vpunpckldq	%ymm11,%ymm10,%ymm0
	vpunpckhdq	%ymm9,%ymm8,%ymm8
	vpunpckhdq	%ymm11,%ymm10,%ymm10
	vpunpcklqdq	%ymm0,%ymm6,%ymm9		# "c0"
	vpunpckhqdq	%ymm0,%ymm6,%ymm6		# "c1"
	vpunpcklqdq	%ymm10,%ymm8,%ymm11		# "c2"
	vpunpckhqdq	%ymm10,%ymm8,%ymm8		# "c3"
	vpaddd		%ymm28,%ymm12,%ymm12
	vpaddd		%ymm29,%ymm13,%ymm13
	vpaddd		%ymm30,%ymm14,%ymm14
	vpaddd		%ymm31,%ymm15,%ymm15

	vpunpckldq	%ymm13,%ymm12,%ymm10
	vpunpckldq	%ymm15,%ymm14,%ymm0
	vpunpckhdq	%ymm13,%ymm12,%ymm12
	vpunpckhdq	%ymm15,%ymm14,%ymm14
	vpunpcklqdq	%ymm0,%ymm10,%ymm13		# "d0"
	vpunpckhqdq	%ymm0,%ymm10,%ymm10		# "d1"
	vpunpcklqdq	%ymm14,%ymm12,%ymm15		# "d2"
	vpunpckhqdq	%ymm14,%ymm12,%ymm12		# "d3"
	vperm2i128	$0x20,%ymm13,%ymm9,%ymm0	# "de-interlace" further
	vperm2i128	$0x31,%ymm13,%ymm9,%ymm13
	vperm2i128	$0x20,%ymm10,%ymm6,%ymm9
	vperm2i128	$0x31,%ymm10,%ymm6,%ymm10
	vperm2i128	$0x20,%ymm15,%ymm11,%ymm6
	vperm2i128	$0x31,%ymm15,%ymm11,%ymm15
	vperm2i128	$0x20,%ymm12,%ymm8,%ymm11
	vperm2i128	$0x31,%ymm12,%ymm8,%ymm12
	cmp		$64*8,%rdx
	jb		.Ltail8xvl

	mov		$0x80,%eax		# size optimization
	vpxord		0x00(%rsi),%ymm19,%ymm19	# xor with input
	vpxor		0x20(%rsi),%ymm0,%ymm0
	vpxor		0x40(%rsi),%ymm5,%ymm5
	vpxor		0x60(%rsi),%ymm13,%ymm13
	lea		(%rsi,%rax),%rsi	# size optimization
	vmovdqu32	%ymm19,0x00(%rdi)
	vmovdqu		%ymm0,0x20(%rdi)
	vmovdqu		%ymm5,0x40(%rdi)
	vmovdqu		%ymm13,0x60(%rdi)
	lea		(%rdi,%rax),%rdi	# size optimization

	vpxor		0x00(%rsi),%ymm1,%ymm1
	vpxor		0x20(%rsi),%ymm9,%ymm9
	vpxor		0x40(%rsi),%ymm2,%ymm2
	vpxor		0x60(%rsi),%ymm10,%ymm10
	lea		(%rsi,%rax),%rsi	# size optimization
	vmovdqu		%ymm1,0x00(%rdi)
	vmovdqu		%ymm9,0x20(%rdi)
	vmovdqu		%ymm2,0x40(%rdi)
	vmovdqu		%ymm10,0x60(%rdi)
	lea		(%rdi,%rax),%rdi	# size optimization

	vpxord		0x00(%rsi),%ymm18,%ymm18
	vpxor		0x20(%rsi),%ymm6,%ymm6
	vpxor		0x40(%rsi),%ymm7,%ymm7
	vpxor		0x60(%rsi),%ymm15,%ymm15
	lea		(%rsi,%rax),%rsi	# size optimization
	vmovdqu32	%ymm18,0x00(%rdi)
	vmovdqu		%ymm6,0x20(%rdi)
	vmovdqu		%ymm7,0x40(%rdi)
	vmovdqu		%ymm15,0x60(%rdi)
	lea		(%rdi,%rax),%rdi	# size optimization

	vpxor		0x00(%rsi),%ymm3,%ymm3
	vpxor		0x20(%rsi),%ymm11,%ymm11
	vpxor		0x40(%rsi),%ymm4,%ymm4
	vpxor		0x60(%rsi),%ymm12,%ymm12
	lea		(%rsi,%rax),%rsi	# size optimization
	vmovdqu		%ymm3,0x00(%rdi)
	vmovdqu		%ymm11,0x20(%rdi)
	vmovdqu		%ymm4,0x40(%rdi)
	vmovdqu		%ymm12,0x60(%rdi)
	lea		(%rdi,%rax),%rdi	# size optimization

	vpbroadcastd	0(%r9),%ymm0		# reload key
	vpbroadcastd	4(%r9),%ymm1

	sub		$64*8,%rdx
	jnz		.Loop_outer8xvl

	jmp		.Ldone8xvl

.align	32
.Ltail8xvl:
	vmovdqa64	%ymm19,%ymm8		# size optimization
	xor		%r9,%r9
	sub		%rsi,%rdi
	cmp		$64*1,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm8,%ymm8	# xor with input
	vpxor		0x20(%rsi),%ymm0,%ymm0
	vmovdqu		%ymm8,0x00(%rdi,%rsi)
	vmovdqu		%ymm0,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm5,%ymm8
	vmovdqa		%ymm13,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*2,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm5,%ymm5
	vpxor		0x20(%rsi),%ymm13,%ymm13
	vmovdqu		%ymm5,0x00(%rdi,%rsi)
	vmovdqu		%ymm13,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm1,%ymm8
	vmovdqa		%ymm9,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*3,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm1,%ymm1
	vpxor		0x20(%rsi),%ymm9,%ymm9
	vmovdqu		%ymm1,0x00(%rdi,%rsi)
	vmovdqu		%ymm9,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm2,%ymm8
	vmovdqa		%ymm10,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*4,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm2,%ymm2
	vpxor		0x20(%rsi),%ymm10,%ymm10
	vmovdqu		%ymm2,0x00(%rdi,%rsi)
	vmovdqu		%ymm10,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa32	%ymm18,%ymm8
	vmovdqa		%ymm6,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*5,%rdx
	jb		.Less_than_64_8xvl
	vpxord		0x00(%rsi),%ymm18,%ymm18
	vpxor		0x20(%rsi),%ymm6,%ymm6
	vmovdqu32	%ymm18,0x00(%rdi,%rsi)
	vmovdqu		%ymm6,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm7,%ymm8
	vmovdqa		%ymm15,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*6,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm7,%ymm7
	vpxor		0x20(%rsi),%ymm15,%ymm15
	vmovdqu		%ymm7,0x00(%rdi,%rsi)
	vmovdqu		%ymm15,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm3,%ymm8
	vmovdqa		%ymm11,%ymm0
	lea		64(%rsi),%rsi

	cmp		$64*7,%rdx
	jb		.Less_than_64_8xvl
	vpxor		0x00(%rsi),%ymm3,%ymm3
	vpxor		0x20(%rsi),%ymm11,%ymm11
	vmovdqu		%ymm3,0x00(%rdi,%rsi)
	vmovdqu		%ymm11,0x20(%rdi,%rsi)
	je		.Ldone8xvl
	vmovdqa		%ymm4,%ymm8
	vmovdqa		%ymm12,%ymm0
	lea		64(%rsi),%rsi

.Less_than_64_8xvl:
	vmovdqa		%ymm8,0x00(%rsp)
	vmovdqa		%ymm0,0x20(%rsp)
	lea		(%rdi,%rsi),%rdi
	and		$63,%rdx

.Loop_tail8xvl:
	movzb		(%rsi,%r9),%eax
	movzb		(%rsp,%r9),%ecx
	lea		1(%r9),%r9
	xor		%ecx,%eax
	mov		%al,-1(%rdi,%r9)
	dec		%rdx
	jnz		.Loop_tail8xvl

	vpxor		%ymm8,%ymm8,%ymm8
	vmovdqa		%ymm8,0x00(%rsp)
	vmovdqa		%ymm8,0x20(%rsp)

.Ldone8xvl:
	vzeroall
	lea		-8(%r10),%rsp
.L8xvl_epilogue:
	ret
.size	chacha20_8xvl,.-chacha20_8xvl
#endif
