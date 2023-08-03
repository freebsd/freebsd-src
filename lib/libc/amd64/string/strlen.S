/*-
 * Written by Mateusz Guzik <mjg@freebsd.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "amd64_archlevel.h"

/*
 * Note: this routine was written with kernel use in mind (read: no simd),
 * it is only present in userspace as a temporary measure until something
 * better gets imported.
 */

#define ALIGN_TEXT      .p2align 4,0x90 /* 16-byte alignment, nop filled */

ARCHFUNCS(strlen)
	ARCHFUNC(strlen, scalar)
	ARCHFUNC(strlen, baseline)
ENDARCHFUNCS(strlen)

/*
 * strlen(string)
 *	  %rdi
 *
 * Uses the ((x - 0x01....01) & ~x & 0x80....80) trick.
 *
 * 0x01....01 is replaced with 0x0 - 0x01....01 so that it can be added
 * with leaq.
 *
 * For a description see either:
 * - "Hacker's Delight" by Henry S. Warren, Jr.
 * - "Optimizing subroutines in assembly language: An optimization guide for x86 platforms"
 *   by Agner Fog
 *
 * The latter contains a 32-bit variant of the same algorithm coded in assembly for i386.
 */
ARCHENTRY(strlen, scalar)
	movabsq	$0xfefefefefefefeff,%r8
	movabsq	$0x8080808080808080,%r9

	movq	%rdi,%r10
	movq	%rdi,%rcx
	testb	$7,%dil
	jz	2f

	/*
	 * Handle misaligned reads: align to 8 and fill
	 * the spurious bytes.
	 */
	andq	$~7,%rdi
	movq	(%rdi),%r11
	shlq	$3,%rcx
	movq	$-1,%rdx
	shlq	%cl,%rdx
	notq	%rdx
	orq	%rdx,%r11

	leaq	(%r11,%r8),%rcx
	notq	%r11
	andq	%r11,%rcx
	andq	%r9,%rcx
	jnz	3f

	/*
	 * Main loop.
	 */
	ALIGN_TEXT
1:
	leaq	8(%rdi),%rdi
2:
	movq	(%rdi),%r11
	leaq	(%r11,%r8),%rcx
	notq	%r11
	andq	%r11,%rcx
	andq	%r9,%rcx
	jz	1b
3:
	bsfq	%rcx,%rcx
	shrq	$3,%rcx
	leaq	(%rcx,%rdi),%rax
	subq	%r10,%rax
	ret
ARCHEND(strlen, scalar)

ARCHENTRY(strlen, baseline)
	mov	%rdi, %rcx
	pxor	%xmm1, %xmm1
	and	$~0xf, %rdi			# align string
	pcmpeqb	(%rdi), %xmm1			# compare head (with junk before string)
	mov	%rcx, %rsi			# string pointer copy for later
	and	$0xf, %ecx			# amount of bytes rdi is past 16 byte alignment
	pmovmskb %xmm1, %eax
	add	$32, %rdi			# advance to next iteration
	shr	%cl, %eax			# clear out matches in junk bytes
	test	%eax, %eax			# any match? (can't use ZF from SHR as CL=0 is possible)
	jnz	2f

	ALIGN_TEXT
1:	pxor	%xmm1, %xmm1
	pcmpeqb	-16(%rdi), %xmm1		# find NUL bytes
	pmovmskb %xmm1, %eax
	test	%eax, %eax			# were any NUL bytes present?
	jnz	3f

	/* the same unrolled once more */
	pxor	%xmm1, %xmm1
	pcmpeqb	(%rdi), %xmm1
	pmovmskb %xmm1, %eax
	add	$32, %rdi			# advance to next iteration
	test	%eax, %eax
	jz	1b

	/* match found in loop body */
	sub	$16, %rdi			# undo half the advancement
3:	tzcnt	%eax, %eax			# find the first NUL byte
	sub	%rsi, %rdi			# string length until beginning of (%rdi)
	lea	-16(%rdi, %rax, 1), %rax	# that plus loc. of NUL byte: full string length
	ret

	/* match found in head */
2:	tzcnt	%eax, %eax			# compute string length
	ret
ARCHEND(strlen, baseline)

	.section .note.GNU-stack,"",%progbits
