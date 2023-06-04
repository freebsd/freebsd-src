/*
 * This file is in public domain.
 * Written by Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * $FreeBSD$
 */

#if defined(__FreeBSD__)
#include <machine/specialreg.h>
#else
#define	CPUID2_OSXSAVE			0x08000000
#define	CPUID2_AVX			0x10000000
#define	XFEATURE_ENABLED_X87		0x00000001
#define	XFEATURE_ENABLED_SSE		0x00000002
#define	XFEATURE_ENABLED_AVX		0x00000004
#define	XFEATURE_AVX					\
    (XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE | XFEATURE_ENABLED_AVX)
#endif

	.text

	.globl xregs_banks_max
	.type xregs_banks_max, @function
xregs_banks_max:
	pushq	%rbx
	movl	$1, %eax
	cpuid
	andl	$(CPUID2_AVX|CPUID2_OSXSAVE), %ecx
	cmpl	$(CPUID2_AVX|CPUID2_OSXSAVE), %ecx
	jne	sse
	xorl	%ecx, %ecx
	xgetbv
	andl	$XFEATURE_AVX, %eax
	cmpl	$XFEATURE_AVX, %eax
	jne	sse
	movl	$1, %eax
	jmp	out
sse:
	xorl	%eax, %eax
out:
	popq	%rbx
	retq

	.size xregs_banks_max, . - xregs_banks_max


	.globl cpu_to_xmm
	.type cpu_to_xmm, @function
cpu_to_xmm:
	movdqu	%xmm0, (%rdi)
	movdqu	%xmm1, 1 * 16(%rdi)
	movdqu	%xmm2, 2 * 16(%rdi)
	movdqu	%xmm3, 3 * 16(%rdi)
	movdqu	%xmm4, 4 * 16(%rdi)
	movdqu	%xmm5, 5 * 16(%rdi)
	movdqu	%xmm6, 6 * 16(%rdi)
	movdqu	%xmm7, 7 * 16(%rdi)
	movdqu	%xmm8, 8 * 16(%rdi)
	movdqu	%xmm9, 9 * 16(%rdi)
	movdqu	%xmm10, 10 * 16(%rdi)
	movdqu	%xmm11, 11 * 16(%rdi)
	movdqu	%xmm12, 12 * 16(%rdi)
	movdqu	%xmm13, 13 * 16(%rdi)
	movdqu	%xmm14, 14 * 16(%rdi)
	movdqu	%xmm15, 15 * 16(%rdi)
	retq

	.size cpu_to_xmm, . - cpu_to_xmm


	.globl xmm_to_cpu
	.type xmm_to_cpu, @function
xmm_to_cpu:
	movdqu	(%rdi), %xmm0
	movdqu	1 * 16(%rdi), %xmm1
	movdqu	2 * 16(%rdi), %xmm2
	movdqu	3 * 16(%rdi), %xmm3
	movdqu	4 * 16(%rdi), %xmm4
	movdqu	5 * 16(%rdi), %xmm5
	movdqu	6 * 16(%rdi), %xmm6
	movdqu	7 * 16(%rdi), %xmm7
	movdqu	8 * 16(%rdi), %xmm8
	movdqu	9 * 16(%rdi), %xmm9
	movdqu	10 * 16(%rdi), %xmm10
	movdqu	11 * 16(%rdi), %xmm11
	movdqu	12 * 16(%rdi), %xmm12
	movdqu	13 * 16(%rdi), %xmm13
	movdqu	14 * 16(%rdi), %xmm14
	movdqu	15 * 16(%rdi), %xmm15
	retq

	.size xmm_to_cpu, . - xmm_to_cpu


	.globl cpu_to_avx
	.type cpu_to_avx, @function
cpu_to_avx:
	vmovdqu	%ymm0, (%rdi)
	vmovdqu	%ymm1, 1 * 32(%rdi)
	vmovdqu	%ymm2, 2 * 32(%rdi)
	vmovdqu	%ymm3, 3 * 32(%rdi)
	vmovdqu	%ymm4, 4 * 32(%rdi)
	vmovdqu	%ymm5, 5 * 32(%rdi)
	vmovdqu	%ymm6, 6 * 32(%rdi)
	vmovdqu	%ymm7, 7 * 32(%rdi)
	vmovdqu	%ymm8, 8 * 32(%rdi)
	vmovdqu	%ymm9, 9 * 32(%rdi)
	vmovdqu	%ymm10, 10 * 32(%rdi)
	vmovdqu	%ymm11, 11 * 32(%rdi)
	vmovdqu	%ymm12, 12 * 32(%rdi)
	vmovdqu	%ymm13, 13 * 32(%rdi)
	vmovdqu	%ymm14, 14 * 32(%rdi)
	vmovdqu	%ymm15, 15 * 32(%rdi)
	retq

	.size cpu_to_avx, . - cpu_to_avx


	.globl avx_to_cpu
	.type avx_to_cpu, @function
avx_to_cpu:
	vmovdqu	(%rdi), %ymm0
	vmovdqu	1 * 32(%rdi), %ymm1
	vmovdqu	2 * 32(%rdi), %ymm2
	vmovdqu	3 * 32(%rdi), %ymm3
	vmovdqu	4 * 32(%rdi), %ymm4
	vmovdqu	5 * 32(%rdi), %ymm5
	vmovdqu	6 * 32(%rdi), %ymm6
	vmovdqu	7 * 32(%rdi), %ymm7
	vmovdqu	8 * 32(%rdi), %ymm8
	vmovdqu	9 * 32(%rdi), %ymm9
	vmovdqu	10 * 32(%rdi), %ymm10
	vmovdqu	11 * 32(%rdi), %ymm11
	vmovdqu	12 * 32(%rdi), %ymm12
	vmovdqu	13 * 32(%rdi), %ymm13
	vmovdqu	14 * 32(%rdi), %ymm14
	vmovdqu	15 * 32(%rdi), %ymm15
	retq

	.size avx_to_cpu, . - avx_to_cpu

	.section        .note.GNU-stack,"",@progbits
