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

	.weak	rindex
	.set	rindex, strrchr

ARCHFUNCS(strrchr)
	ARCHFUNC(strrchr, scalar)
	ARCHFUNC(strrchr, baseline)
ENDARCHFUNCS(strrchr)

ARCHENTRY(strrchr, scalar)
	mov		%edi, %ecx
	and		$~7, %rdi		# Align %rdi to 8-byte boundary
	movzbl		%sil, %esi		# Clear high bits of search char
	movabs		$0x0101010101010101, %r8 # Constant for SWAR arithmetic
	movabs		$0x8080808080808080, %r9 # Constant for MSB bitmask
	imul		%r8, %rsi		# Replicate search char into all 8 bytes

	/*
	 * UNALIGNED START HANDLING:
	 * We read the first 8-byte word (which may contain bytes before the string).
	 * We create a mask (%r10) where 0xFF indicates a "junk" byte before the start.
	 */
	and		$7, %ecx		# %ecx = alignment offset (0-7)
	mov		(%rdi), %rax		# Load the first aligned 8-byte word

	mov		$-1, %r11		# Start with all bits set (0xFF...FF)
	shl		$3, %ecx		# Convert byte offset to bit offset
	mov		%r11, %r10
	xor		%r11, %r11
	shl		%cl, %r10		# %r10 = 0xFF where the string IS
	not		%r10			# %r10 = 0xFF where the string IS NOT (junk)

	xor		%r12, %r12		# %r12 = Pointer to the block with the last match
	xor		%r13, %r13		# %r13 = Bitmask of the last match

	/*
	 * Initial word processing:
	 * We OR the junk bytes with 0x01 to prevent NUL detection,
	 * and we will mask the match results later.
	 */
	jmp		.Lprocess_word

	.p2align 4
.Lloop:
	add		$8, %rdi
	mov		(%rdi), %rax		# Load next 8-byte chunk
	xor		%r10, %r10		# No junk mask for subsequent words

.Lprocess_word:
	/* NUL Detection
	 * Detects 0x00 bytes: ((val - 0x01) & ~val & 0x80)
	 */
	mov		%rax, %rdx
	or		%r10, %rdx		# Mask junk bytes so they don't look like NUL
	mov		%rdx, %r11
	sub		%r8, %r11
	not		%rdx
	and		%r11, %rdx
	and		%r9, %rdx		# %rdx = mask where bytes are NUL (0x80)

	/* Character Match Detection
	 * Detects search char: XOR with replicated char, then find NULs in result
	 */
	mov		%rax, %rcx
	xor		%rsi, %rcx		# Matching bytes become 0x00
	not		%r10
	and		%r10, %rcx		# Mask junk bytes so they don't look like matches
	not		%r10			# Restore r10
	mov		%rcx, %r11
	sub		%r8, %r11
	not		%rcx
	and		%r11, %rcx
	and		%r9, %rcx		# %rcx = mask where bytes match 'c' (0x80)

	/*  Termination Check */
	test		%rdx, %rdx
	jnz		.Lfound_nul

	/* Save Match and Continue */
	test		%rcx, %rcx		# Did we find the character in this block?
	jz		.Lloop
	mov		%rdi, %r12		# Update last seen block pointer
	mov		%rcx, %r13		# Update last seen match mask
	jmp		.Lloop

.Lfound_nul:
	/*
	 * End of string reached.
	 * We must ignore any 'matches' found after the first NUL byte.
	 */
	mov		%rdx, %r11
	neg		%r11
	and		%rdx, %r11		# Isolate the lowest 0x80 bit (the first NUL)
	lea		(%r11, %r11), %rax
	sub		$1, %rax		# Create mask of 1s up to the first NUL

	and		%rax, %rcx		# Truncate match mask at the NUL position

	test		%rcx, %rcx		# Any valid matches in this final block?
	jz		.Lfinalize
	mov		%rdi, %r12
	mov		%rcx, %r13

.Lfinalize:
	test		%r12, %r12		# Did we ever find a match?
	jz		.Lnot_found

	/*
	 * Found the last match. Use BSR to find the highest bit in the mask.
	 * In little-endian, the highest bit in the register is the latest byte in memory.
	 */
	bsr		%r13, %rax		# Bit Scan Reverse
	shr		$3, %rax		# Convert bit index to byte index (0-7)
	add		%r12, %rax		# Pointer = block_start + index
	ret

.Lnot_found:
	xor		%rax, %rax		# Return NULL
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
