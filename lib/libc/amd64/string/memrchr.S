/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023, 2025 Robert Clausecker <fuz@FreeBSD.org>
 */

#include <machine/asm.h>

#include "amd64_archlevel.h"

#define	ALIGN_TEXT	.p2align 4, 0x90

ARCHFUNCS(memrchr)
	ARCHFUNC(memrchr, scalar)
	ARCHFUNC(memrchr, baseline)
ENDARCHFUNCS(memrchr)

ARCHENTRY(memrchr, scalar)
	xor	%eax, %eax		# prospective return value
	sub	$4, %rdx		# 4 bytes left to process?
	jb	1f

	ALIGN_TEXT
0:	xor	%r8, %r8
	lea	2(%rdi), %r10
	cmp	%sil, 2(%rdi)
	cmovne	%r8, %r10		# point to null if no match

	cmp	%sil, (%rdi)
	cmove	%rdi, %r8		# point to first char if match

	lea	1(%rdi), %r9
	cmp	%sil, 1(%rdi)
	cmovne	%r8, %r9		# point to first result if no match in second

	lea	3(%rdi), %r11
	cmp	%sil, 3(%rdi)
	cmovne	%r10, %r11

	test	%r11, %r11
	cmovz	%r9, %r11		# take first pair match if none in second

	test	%r11, %r11
	cmovnz	%r11, %rax		# take match in current set if any

	add	$4, %rdi
	sub	$4, %rdx
	jae	0b

1:	cmp	$-3, %edx		# a least one character left to process?
	jb	2f

	cmp	%sil, (%rdi)
	cmove	%rdi, %rax

	lea	1(%rdi), %rcx
	cmp	$-2, %edx		# at least two characters left to process?
	jb	2f

	cmp	%sil, 1(%rdi)
	cmove	%rcx, %rax

	lea	2(%rdi), %rcx
	cmp	$-1, %edx		# at least three character left to process?
	jb	2f

	cmp	%sil, 2(%rdi)
	cmove	%rcx, %rax

2:	ret
ARCHEND(memrchr, scalar)

ARCHENTRY(memrchr, baseline)
	test		%rdx, %rdx		# empty input?
	je		.Lnomatchb


	lea		(%rdi, %rdx, 1), %ecx	# pointer to end of buffer
	lea		-1(%rdi, %rdx, 1), %rdx	# pointer to last char in buffer
	movd		%esi, %xmm2
	and		$~0x1f, %rdx		# pointer to final 32 buffer bytes
	movdqa		(%rdx), %xmm0		# load last 32 bytes
	movdqa		16(%rdx), %xmm1

	punpcklbw	%xmm2, %xmm2		# c -> cc

	mov		$-1, %r8d
	neg		%ecx
	mov		%r8d, %r9d
	shr		%cl, %r8d		# mask with zeroes after the string

	punpcklwd	%xmm2, %xmm2		# cc -> cccc

	mov		%edi, %ecx
	mov		%r9d, %eax
	shl		%cl, %r9d		# mask with zeroes before the string

	pshufd		$0, %xmm2, %xmm2	# cccc -> cccccccccccccccc

	cmp		%rdx, %rdi		# tail is beginning of buffer?
	cmovae		%r9d, %eax		# if yes, do combined head/tail processing
	and		%r8d, %eax		# mak of bytes in tail part of string

	/* process tail */
	pcmpeqb		%xmm2, %xmm1
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm1, %esi
	pmovmskb	%xmm0, %ecx
	shl		$16, %esi
	or		%esi, %ecx		# locations of matches
	and		%ecx, %eax		# any match inside buffer?
	jnz		.Lprecisematchb

	cmp		%rdx, %rdi		# did the buffer begin here?
	jae		.Lnomatchb		# if yes, we are done

	/* main loop */
	ALIGN_TEXT
0:	movdqa		-32(%rdx), %xmm0	# load previous string chunk
	movdqa		-16(%rdx), %xmm1
	sub		$32, %rdx		# beginning of string reached?
	cmp		%rdx, %rdi
	jae		.Ltailb

	pcmpeqb		%xmm2, %xmm0
	pcmpeqb		%xmm2, %xmm1
	por		%xmm1, %xmm0		# match in either half?
	pmovmskb	%xmm0, %eax
	test		%eax, %eax
	jz		0b

.Lmatchb:
	pcmpeqb		(%rdx), %xmm2		# redo comparison of first 16 bytes
	pmovmskb	%xmm1, %ecx
	pmovmskb	%xmm2, %eax
	shl		$16, %ecx
	or		%ecx, %eax		# location of matches

.Lprecisematchb:
	bsr		%eax, %eax		# find location of match
	add		%rdx, %rax		# point to matching byte
	ret

.Ltailb:
	pcmpeqb		%xmm2, %xmm1
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm1, %ecx
	pmovmskb	%xmm0, %eax
	shl		$16, %ecx
	or		%ecx, %eax		# location of matches
	and		%r9d, %eax		# mask out matches before buffer
	bsr		%eax, %edi		# location of match
	lea		(%rdx, %rdi, 1), %rdx	# pointer to match (if any)
	cmovnz		%rdx, %rax		# point to match if present,
	ret					# else null pointer

.Lnomatchb:
	xor		%eax, %eax		# return null pointer
	ret
ARCHEND(memrchr, baseline)

	.section	.note.GNU-stack, "", %progbits
