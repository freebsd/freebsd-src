/*-
 * Copyright (c) 2023, The FreeBSD Foundation
 *
 * SPDX-License-Expression: BSD-2-Clause
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * Adapted from NetBSD's common/lib/libc/arch/x86_64/string/strcmp.S
 * written by J.T. Conklin <jtc@acorntoolworks.com> that was originally
 * dedicated to the public domain.
 */

#include <machine/asm.h>
#include <machine/param.h>

#if 0
	RCSID("$NetBSD: strcmp.S,v 1.3 2004/07/19 20:04:41 drochner Exp $")
#endif

#include "amd64_archlevel.h"

#define ALIGN_TEXT	.p2align 4, 0x90

ARCHFUNCS(strcmp)
	ARCHFUNC(strcmp, scalar)
	ARCHFUNC(strcmp, baseline)
ENDARCHFUNCS(strcmp)

ARCHENTRY(strcmp, scalar)
	/*
	 * Align s1 to word boundary.
	 * Consider unrolling loop?
	 */
.Ls1align:
	testb	$7,%dil
	je	.Ls1aligned
	movb	(%rdi),%al
	incq	%rdi
	movb	(%rsi),%dl
	incq	%rsi
	testb	%al,%al
	je	.Ldone
	cmpb	%al,%dl
	je	.Ls1align
	jmp	.Ldone

	/*
	 * Check whether s2 is aligned to a word boundary.  If it is, we
	 * can compare by words.  Otherwise we have to compare by bytes.
	 */
.Ls1aligned:
	testb	$7,%sil
	jne	.Lbyte_loop

	movabsq	$0x0101010101010101,%r8
	subq	$8,%rdi
	movabsq	$0x8080808080808080,%r9
	subq	$8,%rsi

	ALIGN_TEXT
.Lword_loop:
	movq	8(%rdi),%rax
	addq	$8,%rdi
	movq	8(%rsi),%rdx
	addq	$8,%rsi
	cmpq	%rax,%rdx
	jne	.Lbyte_loop
	subq	%r8,%rdx
	notq	%rax
	andq	%rax,%rdx
	testq	%r9,%rdx
	je	.Lword_loop

	ALIGN_TEXT
.Lbyte_loop:
	movb	(%rdi),%al
	incq	%rdi
	movb	(%rsi),%dl
	incq	%rsi
	testb	%al,%al
	je	.Ldone
	cmpb	%al,%dl
	je	.Lbyte_loop

.Ldone:
	movzbq	%al,%rax
	movzbq	%dl,%rdx
	subq	%rdx,%rax
	ret
ARCHEND(strcmp, scalar)

ARCHENTRY(strcmp, baseline)
	/* check if either string crosses a page in the head */
	lea		15(%rdi), %r8d	# end of head
	lea		15(%rsi), %r9d
	mov		%edi, %eax
	mov		%esi, %edx
	xor		%edi, %r8d	# bits that changed between first and last byte
	xor		%esi, %r9d
	and		$~0xf, %rdi	# align heads to 16 bytes
	and		$~0xf, %rsi
	or		%r8d, %r9d	# in either RSI or RDI
	and		$0xf, %eax	# offset from alignment
	and		$0xf, %edx
	pxor		%xmm1, %xmm1
	test		$PAGE_SIZE, %r9d # did the page change?
	jz		0f		# if not, take fast path

	/* heads may cross page boundary, avoid unmapped loads */
	movdqa		(%rdi), %xmm0	# load aligned heads
	movdqa		(%rsi), %xmm2
	mov		$-1, %r8d
	mov		$-1, %r9d
	mov		%eax, %ecx
	shl		%cl, %r8d	# string head in XMM0
	mov		%edx, %ecx
	shl		%cl, %r9d	# string head in XMM2
	movdqa		%xmm0, -40(%rsp) # stash copies of the heads on the stack
	movdqa		%xmm2, -24(%rsp)
	pcmpeqb		%xmm1, %xmm0
	pcmpeqb		%xmm1, %xmm2
	pmovmskb	%xmm0, %r10d
	pmovmskb	%xmm2, %r11d
	test		%r8d, %r10d	# NUL byte present in first string?
	lea		-40(%rsp), %r8
	cmovz		%rdi, %r8
	test		%r9d, %r11d	# NUL byte present in second string?
	lea		-24(%rsp), %r9
	cmovz		%rsi, %r9
	movdqu		(%r8, %rax, 1), %xmm0 # load true (or fake) heads
	movdqu		(%r9, %rdx, 1), %xmm4
	jmp		1f

0:	movdqu		(%rdi, %rax, 1), %xmm0 # load true heads
	movdqu		(%rsi, %rdx, 1), %xmm4
1:	pxor		%xmm2, %xmm2
	pcmpeqb		%xmm0, %xmm2	# NUL byte present?
	pcmpeqb		%xmm0, %xmm4	# which bytes match?
	pandn		%xmm4, %xmm2	# match and not NUL byte?
	pmovmskb	%xmm2, %r9d
	xor		$0xffff, %r9d	# mismatch or NUL byte?
	jnz		.Lhead_mismatch

	/* load head and second chunk */
	movdqa		16(%rdi), %xmm2	# load second chunks
	movdqa		16(%rsi), %xmm3
	sub		%rdx, %rax	# is a&0xf >= b&0xf?
	jb		.Lswapped	# if not, proceed with swapped operands

	neg		%rax
	movdqu		16(%rsi, %rax, 1), %xmm0
	sub		%rdi, %rsi	# express RSI as distance from RDI
	lea		(%rsi, %rax, 1), %rdx # point RDX to offset in second string
	neg		%rax
	pcmpeqb		%xmm3, %xmm1	# ... corresponding to RDI
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	add		$16, %rdi
	test		%r8d, %r8d
	jnz		.Lnul_found
	xor		$0xffff, %r9d
	jnz		.Lmismatch
	add		$16, %rdi	# advance aligned pointers

	/*
	 * During the main loop, the layout of the two strings is something like:
	 *
	 *          v ------1------ v ------2------ v
	 *     RDI:    AAAAAAAAAAAAABBBBBBBBBBBBBBBB...
	 *     RSI: AAAAAAAAAAAAABBBBBBBBBBBBBBBBCCC...
	 *
	 * where v indicates the alignment boundaries and corresponding chunks
	 * of the strings have the same letters.  Chunk A has been checked in
	 * the previous iteration.  This iteration, we first check that string
	 * RSI doesn't end within region 2, then we compare chunk B between the
	 * two strings.  As RSI is known not to hold a NUL byte in regsions 1
	 * and 2 at this point, this also ensures that RDI has not ended yet.
	 */
	ALIGN_TEXT
0:	movdqu		(%rdi, %rdx, 1), %xmm0 # chunk of 2nd string corresponding to RDI?
	pxor		%xmm1, %xmm1
	pcmpeqb		(%rdi, %rsi, 1), %xmm1 # end of string in RSI?
	pcmpeqb		(%rdi), %xmm0	# where do the chunks match?
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	test		%r8d, %r8d
	jnz		.Lnul_found
	xor		$0xffff, %r9d	# any mismatches?
	jnz		.Lmismatch

	/* main loop unrolled twice */
	movdqu		16(%rdi, %rdx, 1), %xmm0 # chunk of 2nd string corresponding to RDI?
	pxor		%xmm1, %xmm1
	pcmpeqb		16(%rdi, %rsi, 1), %xmm1 # end of string in RSI?
	pcmpeqb		16(%rdi), %xmm0	# where do the chunks match?
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	add		$32, %rdi
	test		%r8d, %r8d
	jnz		.Lnul_found2
	xor		$0xffff, %r9d	# any mismatches?
	jz		0b

	sub		$16, %rdi	# roll back second increment

	/* a mismatch has been found between RDX and RSI */
.Lmismatch:
	tzcnt		%r9d, %r9d	# where is the mismatch?
	add		%rdi, %rdx	# turn RDX from offset to pointer
	movzbl		(%rdx, %r9, 1), %ecx
	movzbl		(%rdi, %r9, 1), %eax
	sub		%ecx, %eax	# difference of the mismatching chars
	ret

	/* mismatch in true heads */
.Lhead_mismatch:
	tzcnt		%r9d, %r9d	# where is the mismatch?
	add		%rax, %rdi	# return to true heads
	add		%rdx, %rsi
	movzbl		(%rdi, %r9, 1), %eax # mismatching characters
	movzbl		(%rsi, %r9, 1), %ecx
	sub		%ecx, %eax
	ret

.Lnul_found2:
	sub		$16, %rdi	# roll back second increment

	/* a NUL has been found in RSI */
.Lnul_found:
	mov		%eax, %ecx
	mov		%r8d, %r10d
	shl		%cl, %r8w	# adjust NUL mask to positions in RDI/RDX
	xor		$0xffff, %r9d	# mask of mismatches
	or		%r8d, %r9d	# NUL bytes also count as mismatches
	jnz		.Lmismatch

	/*
	 * (RDI) == (RSI) and NUL is past the string.
	 * Compare (RSI) with the corresponding part
	 * of the other string until the NUL byte.
	 */
	movdqu		(%rdi, %rax, 1), %xmm0
	pcmpeqb		(%rdi, %rsi, 1), %xmm0
	add		%rdi, %rsi	# restore RSI pointer
	add		%rax, %rdi	# point RDI to chunk corresponding to (RSI)
	pmovmskb	%xmm0, %ecx	# mask of matches
	not		%ecx		# mask of mismatches
	or		%r10d, %ecx	# mask of mismatches or NUL bytes
	tzcnt		%ecx, %ecx	# location of first mismatch
	movzbl		(%rdi, %rcx, 1), %eax
	movzbl		(%rsi, %rcx, 1), %ecx
	sub		%ecx, %eax
	ret

	/*
	 * If (a&0xf) < (b&0xf), we do the same thing but with swapped
	 * operands.  I found that this performs slightly better than
	 * using conditional moves to do the swap branchless.
	 */
.Lswapped:
	movdqu		16(%rdi, %rax, 1), %xmm0
	sub		%rsi, %rdi	# express RDI as distance from RSI
	lea		(%rdi, %rax, 1), %rdx # point RDX to offset in RDI corresponding to RSI
	neg		%rax		# make difference positive
	pcmpeqb		%xmm2, %xmm1
	pcmpeqb		%xmm3, %xmm0
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	add		$16, %rsi	# advance aligned pointers
	test		%r8d, %r8d
	jnz		.Lnul_founds
	xor		$0xffff, %r9d
	jnz		.Lmismatchs
	add		$16, %rsi

	/*
	 * During the main loop, the layout of the two strings is something like:
	 *
	 *          v ------1------ v ------2------ v
	 *     RDI:    AAAAAAAAAAAAABBBBBBBBBBBBBBBB...
	 *     RSI: AAAAAAAAAAAAABBBBBBBBBBBBBBBBCCC...
	 *
	 * where v indicates the alignment boundaries and corresponding chunks
	 * of the strings have the same letters.  Chunk A has been checked in
	 * the previous iteration.  This iteration, we first check that string
	 * RSI doesn't end within region 2, then we compare chunk B between the
	 * two strings.  As RSI is known not to hold a NUL byte in regsions 1
	 * and 2 at this point, this also ensures that RDI has not ended yet.
	 */
	ALIGN_TEXT
0:	movdqu		(%rsi, %rdx, 1), %xmm0 # chunk of 2nd string corresponding to RDI?
	pxor		%xmm1, %xmm1
	pcmpeqb		(%rsi, %rdi, 1), %xmm1 # end of string in RSI?
	pcmpeqb		(%rsi), %xmm0	# where do the chunks match?
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	test		%r8d, %r8d
	jnz		.Lnul_founds
	xor		$0xffff, %r9d	# any mismatches?
	jnz		.Lmismatchs

	/* main loop unrolled twice */
	movdqu		16(%rsi, %rdx, 1), %xmm0 # chunk of 2nd string corresponding to RDI?
	pxor		%xmm1, %xmm1
	pcmpeqb		16(%rsi, %rdi, 1), %xmm1 # end of string in RSI?
	pcmpeqb		16(%rsi), %xmm0	# where do the chunks match?
	pmovmskb	%xmm1, %r8d
	pmovmskb	%xmm0, %r9d
	add		$32, %rsi
	test		%r8d, %r8d
	jnz		.Lnul_found2s
	xor		$0xffff, %r9d	# any mismatches?
	jz		0b

	sub		$16, %rsi	# roll back second increment

	/* a mismatch has been found between RDX and RDI */
.Lmismatchs:
	tzcnt		%r9d, %r9d	# where is the mismatch?
	add		%rsi, %rdx	# turn RDX from offset to pointer
	movzbl		(%rdx, %r9, 1), %eax
	movzbl		(%rsi, %r9, 1), %ecx
	sub		%ecx, %eax	# difference of the mismatching chars
	ret

.Lnul_found2s:
	sub		$16, %rsi	# roll back second increment

	/* a NUL has been found in RSI */
.Lnul_founds:
	mov		%eax, %ecx
	mov		%r8d, %r10d
	shl		%cl, %r8w	# adjust NUL mask to positions in RDI/RDX
	xor		$0xffff, %r9d	# mask of mismatches
	or		%r8d, %r9d	# NUL bytes also count as mismatches
	jnz		.Lmismatchs

	/*
	 * (RDI) == (RSI) and NUL is past the string.
	 * Compare (RSI) with the corresponding part
	 * of the other string until the NUL byte.
	 */
	movdqu		(%rsi, %rax, 1), %xmm0
	pcmpeqb		(%rsi, %rdi, 1), %xmm0
	add		%rsi, %rdi	# restore RDI pointer
	add		%rax, %rsi	# point RSI to chunk corresponding to (RDI)
	pmovmskb	%xmm0, %ecx	# mask of matches
	not		%ecx		# mask of mismatches
	or		%r10d, %ecx	# mask of mismatches or NUL bytes
	tzcnt		%ecx, %ecx	# location of first mismatch
	movzbl		(%rdi, %rcx, 1), %eax
	movzbl		(%rsi, %rcx, 1), %ecx
	sub		%ecx, %eax
	ret
ARCHEND(strcmp, baseline)

	.section .note.GNU-stack,"",%progbits
