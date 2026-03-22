/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 * Copyright (c) 2026 Robert Clausecker <fuz@FreeBSD.org>
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

	.weak	rindex
	.set	rindex, strrchr

ARCHFUNCS(strrchr)
	ARCHFUNC(strrchr, scalar)
	ARCHFUNC(strrchr, baseline)
ENDARCHFUNCS(strrchr)

ARCHENTRY(strrchr, scalar)
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
	shl	%cl, %r10		# 0x01 where the string is
	xor	%r8, %r10		# 0x01 where it is not
	neg	%r8			# negate 01..01 so we can use lea
	movabs	$0x8080808080808080, %r9

	mov	%rsi, %rcx
	xor	%rax, %rcx		# str ^ c
	or	%r10, %rax		# ensure str != 0 before string
	or	%r10, %rcx		# ensure str^c != 0 before string
	xor	%r11, %r11		# vector of last match (0 -> no match)
	add	$8, %rdi		# advance to next iteration
	lea	(%rax, %r8, 1), %rdx	# str - 0x01..01
	not	%rax			# ~str
	and	%rdx, %rax		# (str - 0x01..01) & ~str
	and	%r9, %rax		# NUL bytes in str, not including junk bits
	jnz	2f			# end of string?

	/* main loop */
	ALIGN_TEXT
3:	mov	(%rdi), %rax		# str
	bswap	%rcx			# (str ^ c) in reverse order, to find last match
	lea	(%rcx, %r8, 1), %rdx	# (str ^ c) - 0x01..01
	not	%rcx			# ~(str ^ c)
	and	%rdx, %rcx		# ((str ^ c - 0x01..01) & ~(str ^ c)
	and	%r9, %rcx		# matches in str, not including junk bits
	cmovnz	%rdi, %r11		# if match found, update match vector
	cmovnz	%rcx, %r10		# ... and match pointer

	add	$8, %rdi		# advance to next iteration
	mov	%rsi, %rcx
	xor	%rax, %rcx		# str ^ c
	lea	(%rax, %r8, 1), %rdx	# str - 0x01..01
	not	%rax			# ~str
	and	%rdx, %rax		# (str - 0x01..01) & ~str
	and	%r9, %rax		# NUL bytes in str, not including junk bits
	jz	3b			# end of string?

	/* NUL found, check for match in tail */
2:	mov	%rax, %rdx
	neg	%rax
	xor	%rdx, %rax		# all bytes behind the NUL byte
	or	%rax, %rcx		# (str ^ c) without matches behind NUL byte
	bswap	%rcx			# (src ^ c) in reverse order, to find last match
	lea	(%rcx, %r8, 1), %rdx	# (str ^ c) - 0x01..01
	not	%rcx			# ~(str ^ c)
	and	%rdx, %rcx		# ((str ^ c - 0x01..01) & ~(str ^ c)
	and	%r9, %rcx		# matches in str, not including junk bits
	cmovnz	%rdi, %r11		# if match found, update match vector
	cmovnz	%rcx, %r10		# ... and match pointer
	tzcnt	%r11, %rcx		# location of last match
	lea	-1(%r10), %rax		# address of last character in vector
	shr	$3, %ecx		# as byte offset
	sub	%rcx, %rax		# subtract character offset
	test	%r11, %r11		# was there actually a match?
	cmovz	%r11, %rax		# if not, return null pointer
	ret
ARCHEND(strrchr, scalar)

ARCHENTRY(strrchr, baseline)
	mov		%edi, %ecx
	and		$~0xf, %rdi		# align to 16 bytes
	movdqa		(%rdi), %xmm1
	movd		%esi, %xmm0
	and		$0xf, %ecx		# offset from alignment
	pxor		%xmm2, %xmm2
	mov		$-1, %edx
	punpcklbw	%xmm0, %xmm0		# c -> cc
	shl		%cl, %edx		# bits corresponding to bytes in the string
	punpcklwd	%xmm0, %xmm0		# cc -> cccc
	xor		%r8, %r8		# address of latest match
	mov		$1, %esi		# bit mask of latest match
	mov		%rdi, %r9		# candidate location for next match
	add		$16, %rdi		# advance to next chunk

	/* check for match in head */
	pcmpeqb		%xmm1, %xmm2		# NUL byte present?
	pshufd		$0, %xmm0, %xmm0	# cccc -> cccccccccccccccc
	pcmpeqb		%xmm0, %xmm1		# c present?
	pmovmskb	%xmm2, %eax
	pmovmskb	%xmm1, %ecx
	and		%edx, %ecx		# c present in the string?
	and		%edx, %eax		# NUL present in the string?
	jnz		.Lend2

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	movdqa		(%rdi), %xmm1
	test		%ecx, %ecx		# was there a match in the last iter.?
	cmovnz		%r9, %r8		# remember match if any
	cmovnz		%ecx, %esi
	pxor		%xmm2, %xmm2
	pcmpeqb		%xmm1, %xmm2		# NUL byte present?
	pcmpeqb		%xmm0, %xmm1		# c present?
	pmovmskb	%xmm2, %eax
	pmovmskb	%xmm1, %ecx
	test		%eax, %eax		# end of string in first half?
	jnz		.Lend

	movdqa		16(%rdi), %xmm1
	test		%ecx, %ecx		# was there a match in the last iter.?
	cmovnz		%rdi, %r8		# remember match if any
	cmovnz		%ecx, %esi
	pxor		%xmm2, %xmm2
	pcmpeqb		%xmm1, %xmm2		# NUL byte present?
	pcmpeqb		%xmm0, %xmm1		# c present?
	pmovmskb	%xmm2, %eax
	pmovmskb	%xmm1, %ecx
	lea		16(%rdi), %r9
	add		$32, %rdi
	test		%eax, %eax		# end of string in second half?
	jz		0b

	ALIGN_TEXT
.Lend2:	sub		$16, %rdi
.Lend:	lea 		-1(%rax), %edx
	xor		%edx, %eax		# mask of bytes in the string
	and		%eax, %ecx		# c found in the tail?
	cmovnz		%rdi, %r8
	cmovnz		%ecx, %esi
	bsr		%esi, %esi		# last location of c in (R8)
	lea		(%r8, %rsi, 1), %rax	# pointer to match
	ret
ARCHEND(strrchr, baseline)
	.section .note.GNU-stack,"",%progbits
