/*
 * This file is in public domain.
 * Written by Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * $FreeBSD$
 */

	.global cpu_to_simd
	.type cpu_to_simd, @function
cpu_to_simd:
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

	.size cpu_to_simd, . - cpu_to_simd


	.global simd_to_cpu
	.type simd_to_cpu, @function
simd_to_cpu:
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

	.size simd_to_cpu, . - simd_to_cpu

	.section        .note.GNU-stack,"",@progbits
