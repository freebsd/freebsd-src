/*
 * Copyright (c) 2023, 2024 The FreeBSD Foundation
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

#define ALIGN_TEXT	.p2align 4, 0x90

	.weak memccpy
	.set memccpy, __memccpy
ARCHFUNCS(__memccpy)
	ARCHFUNC(__memccpy, scalar)
	ARCHFUNC(__memccpy, baseline)
ENDARCHFUNCS(__memccpy)

ARCHENTRY(__memccpy, scalar)
	push	%rbp			# establish stack frame
	mov	%rsp, %rbp
	push	%rax			# dummy push for alignment
	push	%rbx
	push	%rdi
	push	%rsi

	mov	%rsi, %rdi
	mov	%edx, %esi
	mov	%rcx, %rdx
	mov	%rcx, %rbx
	call	CNAME(__memchr)		# ptr = memchr(src, c, len)

	pop	%rsi
	pop	%rdi
	lea	1(%rax), %rdx
	sub	%rsi, %rdx		# size = ptr - src + 1
	mov	%rbx, %rcx
	lea	(%rdi, %rdx, 1), %rbx	# res = dest + size
	test	%rax, %rax		# if (ptr == NULL)
	cmovz	%rcx, %rdx		# size = len
	cmovz	%rax, %rbx		# res = NULL
	call	CNAME(memcpy)

	mov	%rbx, %rax		# return (res)
	pop	%rbx
	leave
	ret
ARCHEND(__memccpy, scalar)

ARCHENTRY(__memccpy, baseline)
	sub		$1, %rcx		# RCX refers to last character in buffer
	jb		.L0			# go to special code path if len was 0

	movd		%edx, %xmm4
	mov		%rcx, %rdx
	punpcklbw	%xmm4, %xmm4		# c -> cc
	mov		%esi, %ecx
	punpcklwd	%xmm4, %xmm4		# cc -> cccc
	mov		%rsi, %r9		# stash a copy of the source pointer for later
	pshufd		$0, %xmm4, %xmm4	# cccc -> cccccccccccccccc
	and		$~0xf, %rsi
	movdqa		%xmm4, %xmm1
	pcmpeqb		(%rsi), %xmm1		# c found in head?
	and		$0xf, %ecx
	mov		$-1, %eax
	pmovmskb	%xmm1, %r8d
	lea		-32(%rcx), %r11
	shl		%cl, %eax		# mask of bytes in the string
	add		%rdx, %r11		# distance from alignment boundary - 32
	jnc		.Lrunt			# jump if buffer length is 32 or less

	and		%r8d, %eax
	jz		0f			# match (or induced match) found?

	/* match in first chunk */
	tzcnt		%eax, %edx		# where is c?
	sub		%ecx, %edx		# ... from the beginning of the string?
	lea		1(%rdi, %rdx, 1), %rax	# return value
	jmp		.L0116

0:	movdqa		16(%rsi), %xmm3		# load second string chunk
	movdqu		(%r9), %xmm2		# load unaligned string head
	movdqa		%xmm4, %xmm1
	pcmpeqb		%xmm3, %xmm1		# c found in second chunk?

	/* process second chunk */
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jz		0f

	/* match in second chunk */
	tzcnt		%eax, %edx		# where is c?
	sub		$16, %ecx
	sub		%ecx, %edx		# adjust for alignment offset
	lea		1(%rdi, %rdx, 1), %rax	# return value
	jmp		.L0132

	/* c not found in second chunk: prepare for main loop */
0:	movdqa		32(%rsi), %xmm0		# load next string chunk
	movdqa		%xmm4, %xmm1
	movdqu		%xmm2, (%rdi)		# deposit head into buffer
	sub		%rcx, %rdi		# adjust RDI to correspond to RSI
	mov		%r11, %rdx
	movdqu		%xmm3, 16(%rdi)		# deposit second chunk
	sub		%rsi, %rdi		# express RDI as distance from RSI
	add		$32, %rsi		# advance RSI past first two chunks
	sub		$16, %rdx		# enough left for another round?
	jb		1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	pcmpeqb		%xmm0, %xmm1		# c encountered?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		3f

	movdqu		%xmm0, (%rsi, %rdi)
	movdqa		16(%rsi), %xmm0		# load next string chunk
	movdqa		%xmm4, %xmm1
	cmp		$16, %rdx		# more than a full chunk left?
	jb		2f

	add		$32, %rsi		# advance pointers to next chunk
	pcmpeqb		%xmm0, %xmm1		# c encountered?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		4f

	movdqu		%xmm0, -16(%rsi, %rdi)
	movdqa		(%rsi), %xmm0		# load next string chunk
	movdqa		%xmm4, %xmm1
	sub		$32, %rdx
	jae		0b

1:	sub		$16, %rsi		# undo second advancement
	add		$16, %edx

	/* 1--16 bytes left in the buffer but string has not ended yet */
2:	pcmpeqb		%xmm1, %xmm0		# c encountered?
	pmovmskb	%xmm0, %r8d
	mov		%r8d, %ecx
	bts		%edx, %r8d		# treat end of buffer as end of string
	tzcnt		%r8d, %r8d		# find tail length
	add		%rsi, %rdi		# restore RDI
	movdqu		1(%rsi, %r8, 1), %xmm0	# load string tail
	movdqu		%xmm0, 1(%rdi, %r8, 1)	# store string tail
	lea		17(%rdi, %r8, 1), %rsi	# return value if terminator encountered
	xor		%eax, %eax		# return value if no terminator encountered
	bt		%r8d, %ecx		# terminator encountered inside buffer?
	cmovc		%rsi, %rax		# if yes, return pointer, else NULL
	ret

4:	sub		$16, %rsi		# undo second advancement

	/* terminator found and buffer has not ended yet */
3:	tzcnt		%eax, %eax		# find length of string tail
	movdqu		-15(%rsi, %rax, 1), %xmm0 # load string tail (incl. c)
	add		%rsi, %rdi		# restore destination pointer
	movdqu		%xmm0, -15(%rdi, %rax, 1) # store string tail (incl. c)
	lea		1(%rdi, %rax, 1), %rax	# compute return value
	ret

	/* buffer is 1--32 bytes in size */
	ALIGN_TEXT
.Lrunt:	add		$32, %r11d		# undo earlier decrement
	mov		%r8d, %r10d		# keep a copy of the original match mask
	bts		%r11d, %r8d		# induce match at buffer end
	and		%ax, %r8w		# is there a match in the first 16 bytes?
	jnz		0f			# if yes, skip looking at second chunk

	pcmpeqb		16(%rsi), %xmm4		# check for match in second chunk
	pmovmskb	%xmm4, %r8d
	shl		$16, %r8d		# place second chunk matches in bits 16--31
	mov		%r8d, %r10d		# keep a copy of the original match mask
	bts		%r11d, %r8d		# induce a match at buffer end

0:	xor		%eax, %eax		# return value if terminator not found
	tzcnt		%r8d, %edx		# find string/buffer length from alignment boundary
	lea		1(%rdi, %rdx, 1), %r8	# return value if terminator found + rcx
	sub		%rcx, %r8
	bt		%edx, %r10d		# was the terminator present?
	cmovc		%r8, %rax		# if yes, return pointer, else NULL
	sub		%ecx, %edx		# find actual string/buffer length

	ALIGN_TEXT
.L0132:	cmp		$16, %rdx		# at least 17 bytes to copy?
	jb		.L0116

	/* copy 17--32 bytes */
	movdqu		(%r9), %xmm0		# load first 16 bytes
	movdqu		-15(%r9, %rdx, 1), %xmm1 # load last 16 bytes
	movdqu		%xmm0, (%rdi)
	movdqu		%xmm1, -15(%rdi, %rdx, 1)
	ret

	/* process strings of 1--16 bytes (rdx: min(buflen, srclen), rax: srclen) */
	ALIGN_TEXT
.L0116:	cmp		$8, %rdx		# at least 9 bytes to copy?
	jae		.L0916

	cmp		$4, %rdx		# at least 5 bytes to copy?
	jae		.L0508

	cmp		$2, %rdx		# at least 3 bytes to copy?
	jae		.L0304

	/* copy one or two bytes */
	movzbl		(%r9), %ecx		# load first byte from src
	movzbl		(%r9, %rdx, 1), %esi	# load last byte from src
	mov		%cl, (%rdi)		# deposit into destination
	mov		%sil, (%rdi, %rdx, 1)
	ret

.L0304:	movzwl		(%r9), %ecx
	movzwl		-1(%r9, %rdx, 1), %esi
	mov		%cx, (%rdi)
	mov		%si, -1(%rdi, %rdx, 1)
	ret

.L0508:	mov		(%r9), %ecx
	mov		-3(%r9, %rdx, 1), %esi
	mov		%ecx, (%rdi)
	mov		%esi, -3(%rdi, %rdx, 1)
	ret

.L0916:	mov		(%r9), %rcx
	mov		-7(%r9, %rdx, 1), %rsi
	mov		%rcx, (%rdi)
	mov		%rsi, -7(%rdi, %rdx, 1)
	ret

	/* length zero destination: return null pointer */
.L0:	xor		%eax, %eax
	ret
ARCHEND(__memccpy, baseline)

	.section .note.GNU-stack,"",%progbits
