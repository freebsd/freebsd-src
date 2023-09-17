/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Robert Clausecker <fuz@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
 */

#include <machine/asm.h>

#include "amd64_archlevel.h"

#define ALIGN_TEXT	.p2align 4,0x90	# 16-byte alignment, nop-filled

	.weak	strchrnul
	.set	strchrnul, __strchrnul

ARCHFUNCS(__strchrnul)
	ARCHFUNC(__strchrnul, scalar)
	ARCHFUNC(__strchrnul, baseline)
ENDARCHFUNCS(__strchrnul)

/*
 * strchrnul(str, c)
 * This is implemented like strlen(str), but we check for the
 * presence of both NUL and c in each iteration.
 */
ARCHENTRY(__strchrnul, scalar)
	mov	%edi, %ecx
	and	$~7, %rdi		# align to 8 byte
	movzbl	%sil, %esi		# clear stray high bits
	movabs	$0x0101010101010101, %r8
	mov	(%rdi), %rax		# load first word
	imul	%r8, %rsi		# replicate char 8 times

	/*
	 * Unaligned input: align to 8 bytes.  Then proceed the same
	 * way as with aligned input, but prevent matches before the
	 * beginning of the string.  This is achieved by oring 0x01
	 * into each byte of the buffer before the string
	 */
	shl	$3, %ecx
	mov	%r8, %r10
	add	$8, %rdi
	shl	%cl, %r10		# 0x01 where the string is
	xor	%r8, %r10		# 0x01 where it is not
	neg	%r8			# negate 01..01 so we can use lea
	movabs	$0x8080808080808080, %r9

	mov	%rsi, %rcx
	xor	%rax, %rcx		# str ^ c
	or	%r10, %rax		# str without NUL bytes before it
	or	%r10, %rcx		# (str ^ c) without matches before it
	lea	(%rax, %r8, 1), %rdx	# str - 0x01..01
	lea	(%rcx, %r8, 1), %r11	# (str ^ c) - 0x01..01
	not	%rax			# ~str
	not	%rcx			# ~(str ^ c)
	and	%rdx, %rax		# (str - 0x01..01) & ~str
	and	%r11, %rcx		# ((str ^ c - 0x01..01) & ~(str ^ c)
	or	%rcx, %rax		# matches for both
	and	%r9, %rax		# not including junk bytes
	jnz	1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	mov	(%rdi), %rax		# str
	mov	%rsi, %rcx
	xor	%rax, %rcx		# str ^ c
	lea	(%rax, %r8, 1), %rdx	# str - 0x01..01
	lea	(%rcx, %r8, 1), %r11	# (str ^ c) - 0x01..01
	not	%rax			# ~str
	not	%rcx			# ~(str ^ c)
	and	%rdx, %rax		# (str - 0x01..01) & ~str
	and	%r11, %rcx		# ((str ^ c - 0x01..01) & ~(str ^ c)
	or	%rcx, %rax		# matches for both
	and	%r9, %rax		# not including junk bits
	jnz	2f

	mov	8(%rdi), %rax		# str
	add	$16, %rdi
	mov	%rsi, %rcx
	xor	%rax, %rcx		# str ^ c
	lea	(%rax, %r8, 1), %rdx	# str - 0x01..01
	lea	(%rcx, %r8, 1), %r11	# (str ^ c) - 0x01..01
	not	%rax			# ~str
	not	%rcx			# ~(str ^ c)
	and	%rdx, %rax		# (str - 0x01..01) & ~str
	and	%r11, %rcx		# ((str ^ c - 0x01..01) & ~(str ^ c)
	or	%rcx, %rax		# matches for both
	and	%r9, %rax		# not including junk bits
	jz	0b

	/* NUL or c found */
1:	sub	$8, %rdi		# undo advance past buffer
2:	tzcnt	%rax, %rax		# first NUL or c byte match
	shr	$3, %eax		# scale from bit to byte index
	add	%rdi, %rax		# pointer to found c or NUL
	ret
ARCHEND(__strchrnul, scalar)

ARCHENTRY(__strchrnul, baseline)
	mov		%edi, %ecx
	and		$~0xf, %rdi		# align to 16 byte
	movdqa		(%rdi), %xmm1
	movd		%esi, %xmm0
	and		$0xf, %ecx		# distance from (%rdi) to start of string
	pxor		%xmm2, %xmm2
	mov		$-1, %edx
	punpcklbw	%xmm0, %xmm0		# c -> cc
	shl		%cl, %edx		# bits corresponding to bytes in the string
	punpcklwd	%xmm0, %xmm0		# cc -> cccc
	add		$16, %rdi

	/* check for match in head */
	pcmpeqb		%xmm1, %xmm2		# NUL bytes present?
	pshufd		$0, %xmm0, %xmm0	# cccc -> cccccccccccccccc
	pcmpeqb		%xmm0, %xmm1		# c present?
	por		%xmm2, %xmm1		# either present?
	pmovmskb	%xmm1, %eax
	and		%edx, %eax		# match in the string?
	jnz		1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	movdqa		(%rdi), %xmm1
	pxor		%xmm2, %xmm2
	pcmpeqb		%xmm1, %xmm2		# NUL bytes present?
	pcmpeqb		%xmm0, %xmm1		# c present?
	por		%xmm2, %xmm1		# either present?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax		# match in the string?
	jnz		2f

	movdqa		16(%rdi), %xmm1
	add		$32, %rdi
	pxor		%xmm2, %xmm2
	pcmpeqb		%xmm1, %xmm2		# NUL bytes present?
	pcmpeqb		%xmm0, %xmm1		# c present?
	por		%xmm2, %xmm1		# either present?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax		# match in the string?
	jz		0b

1:	sub		$16, %rdi		# undo advance past buffer
2:	tzcnt		%eax, %eax		# where is the match?
	add		%rdi, %rax		# pointer to found c or NUL
	ret
ARCHEND(__strchrnul, baseline)

	.section .note.GNU-stack,"",%progbits
