/*
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
	pcmpeqb		(%rsi), %xmm1		# NUL found in head?
	mov		$-1, %r8d
	and		$0xf, %ecx
	shl		%cl, %r8d		# mask of bytes in the string
	pmovmskb	%xmm1, %eax
	and		%r8d, %eax
	jnz		.Lhead_nul

	movdqa		16(%rsi), %xmm3		# load second string chunk
	movdqu		(%r9), %xmm2		# load unaligned string head
	mov		$32, %r8d
	sub		%ecx, %r8d		# head length + length of second chunk
	movdqa		%xmm4, %xmm1
	pcmpeqb		%xmm3, %xmm1		# NUL found in second chunk?

	sub		%r8, %rdx		# enough space left for the second chunk?
	jb		.Lhead_buf_end

	/* process second chunk */
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		.Lsecond_nul

	/* string didn't end in second chunk and neither did buffer -- not a runt! */
	movdqa		32(%rsi), %xmm0		# load next string chunk
	movdqa		%xmm4, %xmm1
	movdqu		%xmm2, (%rdi)		# deposit head into buffer
	sub		%rcx, %rdi		# adjust RDI to correspond to RSI
	movdqu		%xmm3, 16(%rdi)		# deposit second chunk
	sub		%rsi, %rdi		# express RDI as distance from RSI
	add		$32, %rsi		# advance RSI past first two chunks
	sub		$16, %rdx		# enough left for another round?
	jb		1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		3f

	movdqu		%xmm0, (%rsi, %rdi)
	movdqa		16(%rsi), %xmm0		# load next string chunk
	movdqa		%xmm4, %xmm1
	cmp		$16, %rdx		# more than a full chunk left?
	jb		2f

	add		$32, %rsi		# advance pointers to next chunk
	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
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
2:	pcmpeqb		%xmm1, %xmm0		# NUL byte encountered?
	pmovmskb	%xmm0, %r8d
	mov		%r8d, %ecx
	bts		%edx, %r8d		# treat end of buffer as end of string
	or		$0x10000, %eax		# ensure TZCNT finds a set bit
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
	add		$16, %rdx		# restore number of remaining bytes

	/* string has ended but buffer has not */
3:	tzcnt		%eax, %eax		# find length of string tail
	movdqu		-15(%rsi, %rax, 1), %xmm0 # load string tail (incl. NUL)
	add		%rsi, %rdi		# restore destination pointer
	movdqu		%xmm0, -15(%rdi, %rax, 1) # store string tail (incl. NUL)
	lea		1(%rdi, %rax, 1), %rax	# compute return value
	ret

.Lhead_buf_end:
	pmovmskb	%xmm1, %r8d
	add		$32, %edx		# restore edx to (len-1) + ecx
	shl		$16, %r8d		# place 2nd chunk NUL mask into bits 16--31
	mov		%r8d, %r10d
	bts		%rdx, %r8		# treat end of buffer as if terminator present
	xor		%eax, %eax		# return value if terminator not found
	tzcnt		%r8, %rdx		# find string/buffer len from alignment boundary
	lea		1(%rdi, %rdx, 1), %r8	# return value if terminator found + rcx
	sub		%rcx, %r8		# subtract rcx
	bt		%rdx, %r10		# was the terminator present?
	cmovc		%r8, %rax		# if yes, return pointer, else NULL
	sub		%ecx, %edx		# find actual string/buffer len
	jmp		.L0132

.Lsecond_nul:
	add		%r8, %rdx		# restore buffer length
	tzcnt		%eax, %r8d		# where is the NUL byte?
	lea		-16(%rcx), %eax
	sub		%eax, %r8d		# string length
	lea		1(%rdi, %r8, 1), %rax	# return value if NUL before end of buffer
	xor		%ecx, %ecx		# return value if not
	cmp		%r8, %rdx		# is the string shorter than the buffer?
	cmova		%r8, %rdx		# copy only min(buflen, srclen) bytes
	cmovb		%rcx, %rax		# return NUL if buffer ended before string
.L0132:	cmp		$16, %rdx		# at least 17 bytes to copy (not incl NUL)?
	jb		.L0116

	/* copy 17--32 bytes */
	movdqu		(%r9), %xmm0		# load first 16 bytes
	movdqu		-15(%r9, %rdx, 1), %xmm1 # load last 16 bytes
	movdqu		%xmm0, (%rdi)
	movdqu		%xmm1, -15(%rdi, %rdx, 1)
	ret

.Lhead_nul:
	tzcnt		%eax, %r8d		# where is the NUL byte?
	sub		%ecx, %r8d		# ... from the beginning of the string?
	lea		1(%rdi, %r8, 1), %rax	# return value if NUL before end of buffer
	xor		%ecx, %ecx		# return value if not
	cmp		%r8, %rdx		# is the string shorter than the buffer?
	cmova		%r8, %rdx		# copy only min(buflen, srclen) bytes
	cmovb		%rcx, %rax		# return NUL if buffer ended before string

	/* process strings of 1--16 bytes (rdx: min(buflen, srclen), rax: srclen) */
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
