/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Robert Clausecker
 */

#include <machine/asm.h>

#include "amd64_archlevel.h"

#define	ALIGN_TEXT	.p2align 4, 0x90

ARCHFUNCS(memrchr)
	ARCHFUNC(memrchr, scalar)
	ARCHFUNC(memrchr, baseline)
ENDARCHFUNCS(memrchr)

ARCHENTRY(memrchr, scalar)
	lea		-1(%rdi, %rdx, 1), %rax	# point to last char in buffer
	sub		$4, %rdx		# 4 bytes left to process?
	jb		.Ltail

	ALIGN_TEXT
0:	cmp		%sil, (%rax)		# match at last entry?
	je		1f

	cmp		%sil, -1(%rax)		# match at second to last entry?
	je		2f

	cmp		%sil, -2(%rax)		# match at third to last entry?
	je		3f

	cmp		%sil, -3(%rax)		# match at fourth to last entry?
	je		4f

	sub		$4, %rax
	sub		$4, %rdx
	jae		0b

.Ltail:	cmp		$-3, %edx		# at least one character left to process?
	jb		.Lnotfound

	cmp		%sil, (%rax)
	je		1f

	cmp		$-2, %edx		# at least two characters left to process?
	jb		.Lnotfound

	cmp		%sil, -1(%rax)
	je		2f

	cmp		$-1, %edx		# at least three characters left to process?
	jb		.Lnotfound

	cmp		%sil, -2(%rax)
	je		3f

.Lnotfound:
	xor		%eax, %eax
	ret

	/* match found -- adjust rax to point to matching byte */
4:	dec		%rax
3:	dec		%rax
2:	dec		%rax
1:	ret
ARCHEND(memrchr, scalar)

ARCHENTRY(memrchr, baseline)
	movd		%esi, %xmm4
	test		%rdx, %rdx		# empty buffer?
	jz		.L0			# if yes, return immediately

	punpcklbw	%xmm4, %xmm4		# c -> cc
	mov		%edi, %ecx
	punpcklwd	%xmm4, %xmm4		# cc -> cccc
	and		$~0xf, %rdi		# align source pointer
	pshufd		$0, %xmm4, %xmm4	# cccc -> cccccccccccccccc
	and		$0xf, %ecx
	movdqa		%xmm4, %xmm0
	mov		$-1, %r8d
	pcmpeqb		(%rdi), %xmm0		# compare aligned head
	shl		%cl, %r8d		# mask of bytes in the head of the buffer
	pmovmskb	%xmm0, %eax

	sub		$16, %rcx
	and		%r8d, %eax		# match mask
	add		%rcx, %rdx		# advance past head
	cmc
	jbe		.Lrunt			# did the string end in the buffer?

	mov		%rdi, %rsi		# pointer to matching chunk
	add		$16, %rdi
	sub		$16, %rdx		# enough left for another round?
	jbe		1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	movdqa		%xmm4, %xmm0
	pcmpeqb		(%rdi), %xmm0
	pmovmskb	%xmm0, %r8d

	cmp		$16, %rdx		# enough left for second chunk?
	jbe		2f

	movdqa		%xmm4, %xmm0
	pcmpeqb		16(%rdi), %xmm0
	pmovmskb	%xmm0, %ecx

	lea		16(%rdi), %r9
	test		%ecx, %ecx		# match found in second chunk?
	cmovz		%r8d, %ecx		# if not, use match data from first chunk
	cmovz		%rdi, %r9

	test		%ecx, %ecx		# any match found?
	cmovnz		%ecx, %eax		# if yes, overwrite previously found match
	cmovnz		%r9, %rsi

	add		$32, %rdi		# advance to next iteration
	sub		$32, %rdx		# advance to next chunks
	ja		0b

	/* process remaining 1--16 bytes */
1:	pcmpeqb		(%rdi), %xmm4
	mov		$0xffff, %r8d
	xor		%ecx, %ecx
	sub		%edx, %ecx		# number of bytes to be masked out
	pmovmskb	%xmm4, %r9d
	shr		%cl, %r8d		# mask of bytes to be kept in the buffer
	and		%r9d, %r8d
	cmovnz		%r8d, %eax
	cmovnz		%rdi, %rsi
	bsr		%eax, %eax
	lea		(%rsi, %rax, 1), %rsi	# pointer to match (or junk)
	cmovnz		%rsi, %rax		# if any match was found, return it
	ret

	/* end of chunk reached within first half iteration */
2:	test		%r8d, %r8d		# match in previous chunk?
	cmovnz		%r8d, %eax		# if yes, overwrite previous chunks
	cmovnz		%rdi, %rsi
	add		$16, %rdi		# point to tail
	sub		$16, %edx
	jmp		1b			# handle tail the same otherwise

	/* runt: string ends within head, edx has negated amount of invalid head bytes */
.Lrunt:	mov		$0xffff, %r8d
	xor		%ecx, %ecx
	sub		%edx, %ecx
	shr		%cl, %r8d
	and		%r8d, %eax
	bsr		%eax, %eax
	lea		(%rdi, %rax, 1), %rdi
	cmovnz		%rdi, %rax
	ret

	/* empty buffer: return a null pointer */
.L0:	xor		%eax, %eax
	ret
ARCHEND(memrchr, baseline)

	.section	.note.GNU-stack, "", %progbits
