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

	.weak strlcpy
	.set strlcpy, __strlcpy
ARCHFUNCS(__strlcpy)
	ARCHFUNC(__strlcpy, scalar)
	ARCHFUNC(__strlcpy, baseline)
ENDARCHFUNCS(__strlcpy)

ARCHENTRY(__strlcpy, scalar)
	push	%rbp		# establish stack frame
	mov	%rsp, %rbp
	push	%rsi
	push	%rbx
	push	%rdi
	push	%rdx
	mov	%rsi, %rdi
	call	CNAME(strlen)	# strlen(src)
	pop	%rdx
	pop	%rdi
	mov	-8(%rbp), %rsi
	mov	%rax, %rbx	# remember string length for return value
	sub	$1, %rdx	# do not copy into the final byte of the buffer
	jc	0f		# skip copying altogether if buffer was empty
	cmp	%rax, %rdx	# is the buffer longer than the input?
	cmova	%rax, %rdx	# if yes, only copy the part that fits
	movb	$0, (%rdi, %rdx, 1) # NUL-terminate output buffer
	call	CNAME(memcpy)	# copy string to output
0:	mov	%rbx, %rax	# restore return value
	pop	%rbx
	leave
	ret
ARCHEND(__strlcpy, scalar)

ARCHENTRY(__strlcpy, baseline)
	sub		$1, %rdx		# do not count NUL byte in buffer length
	jb		.L0			# go to special code path if len was 0

	mov		%esi, %ecx
	pxor		%xmm1, %xmm1
	mov		%rsi, %r9		# stash a copy of the source pointer for later
	and		$~0xf, %rsi
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
	pxor		%xmm1, %xmm1
	pcmpeqb		%xmm3, %xmm1		# NUL found in second chunk?

	sub		%r8, %rdx		# enough space left for the second chunk?
	jbe		.Lhead_buf_end

	/* process second chunk */
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		.Lsecond_nul

	/* string didn't end in second chunk and neither did buffer -- not a runt! */
	movdqa		32(%rsi), %xmm0		# load next string chunk
	pxor		%xmm1, %xmm1
	movdqu		%xmm2, (%rdi)		# deposit head into buffer
	sub		%rcx, %rdi		# adjust RDI to correspond to RSI
	movdqu		%xmm3, 16(%rdi)		# deposit second chunk
	sub		%rsi, %rdi		# express RDI as distance from RSI
	add		$32, %rsi		# advance RSI past first two chunks
	sub		$16, %rdx		# enough left for another round?
	jbe		1f

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		3f

	movdqu		%xmm0, (%rsi, %rdi)
	movdqa		16(%rsi), %xmm0		# load next string chunk
	pxor		%xmm1, %xmm1
	cmp		$16, %rdx		# more than a full chunk left?
	jbe		2f

	add		$32, %rsi		# advance pointers to next chunk
	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
	pmovmskb	%xmm1, %eax
	test		%eax, %eax
	jnz		4f

	movdqu		%xmm0, -16(%rsi, %rdi)
	movdqa		(%rsi), %xmm0		# load next string chunk
	pxor		%xmm1, %xmm1
	sub		$32, %rdx
	ja		0b

1:	sub		$16, %rsi		# undo second advancement
	add		$16, %edx

	/* 1--16 bytes left in the buffer but string has not ended yet */
2:	pcmpeqb		%xmm1, %xmm0		# NUL byte encountered?
	pmovmskb	%xmm0, %r8d
	mov		%r8d, %eax
	bts		%edx, %r8d		# treat end of buffer as end of string
	tzcnt		%r8d, %r8d		# find tail length
	add		%rsi, %rdi		# restore RDI
	movdqu		(%rsi, %r8, 1), %xmm0	# load string tail
	movdqu		%xmm0, (%rdi, %r8, 1)	# store string tail
	movb		$0, 16(%rdi, %r8, 1)	# NUL terminate

	/* continue to find the end of the string */
	test		%eax, %eax		# end of string already reached?
	jnz		1f

	ALIGN_TEXT
0:	pcmpeqb		32(%rsi), %xmm1
	pmovmskb	%xmm1, %eax
	pxor		%xmm1, %xmm1
	test		%eax, %eax
	jnz		2f

	pcmpeqb		48(%rsi), %xmm1
	pmovmskb	%xmm1, %eax
	add		$32, %rsi
	pxor		%xmm1, %xmm1
	test		%eax, %eax
	jz		0b

1:	sub		$16, %rsi		# undo second advancement
2:	tzcnt		%eax, %eax		# where is the NUL byte?
	sub		%r9, %rsi
	lea		32(%rsi, %rax, 1), %rax	# return string length
	ret

4:	sub		$16, %rsi		# undo second advancement
	add		$16, %rdx		# restore number of remaining bytes

	/* string has ended but buffer has not */
3:	tzcnt		%eax, %eax		# find length of string tail
	movdqu		-15(%rsi, %rax, 1), %xmm0 # load string tail (incl. NUL)
	add		%rsi, %rdi		# restore destination pointer
	movdqu		%xmm0, -15(%rdi, %rax, 1) # store string tail (incl. NUL)
	sub		%r9, %rsi		# string length to current chunk
	add		%rsi, %rax		# plus length of current chunk
	ret

.Lhead_buf_end:
	pmovmskb	%xmm1, %r8d
	add		$32, %edx		# restore edx to (len-1) + ecx
	mov		%r8d, %eax
	shl		$16, %r8d		# place 2nd chunk NUL mask into bits 16--31
	bts		%rdx, %r8		# treat end of buffer as end of string
	tzcnt		%r8, %rdx		# find string/bufer len from alignment boundary
	sub		%ecx, %edx		# find actual string/buffer len
	movb		$0, (%rdi, %rdx, 1)	# write NUL terminator

	/* continue to find the end of the string */
	test		%eax, %eax		# end of string already reached?
	jnz		1f

	ALIGN_TEXT
0:	pcmpeqb		32(%rsi), %xmm1
	pmovmskb	%xmm1, %eax
	pxor		%xmm1, %xmm1
	test		%eax, %eax
	jnz		2f

	pcmpeqb		48(%rsi), %xmm1
	pmovmskb	%xmm1, %eax
	add		$32, %rsi
	pxor		%xmm1, %xmm1
	test		%eax, %eax
	jz		0b

1:	sub		$16, %rsi
2:	tzcnt		%eax, %eax
	sub		%r9, %rsi
	lea		32(%rsi, %rax, 1), %rax	# return string length
	jmp		.L0031

.Lsecond_nul:
	add		%r8, %rdx		# restore buffer length
	tzcnt		%eax, %eax		# where is the NUL byte?
	lea		-16(%rcx), %r8d
	sub		%r8d, %eax		# string length
	cmp		%rax, %rdx		# is the string shorter than the buffer?
	cmova		%rax, %rdx		# copy only min(buflen, srclen) bytes
	movb		$0, (%rdi, %rdx, 1)	# write NUL terminator
.L0031:	cmp		$16, %rdx		# at least 16 bytes to copy (not incl NUL)?
	jb		.L0015

	/* copy 16--31 bytes */
	movdqu		(%r9), %xmm0		# load first 16 bytes
	movdqu		-16(%r9, %rdx, 1), %xmm1 # load last 16 bytes
	movdqu		%xmm0, (%rdi)
	movdqu		%xmm1, -16(%rdi, %rdx, 1)
	ret

.Lhead_nul:
	tzcnt		%eax, %eax		# where is the NUL byte?
	sub		%ecx, %eax		# ... from the beginning of the string?
	cmp		%rax, %rdx		# is the string shorter than the buffer?
	cmova		%rax, %rdx		# copy only min(buflen, srclen) bytes
	movb		$0, (%rdi, %rdx, 1)	# write NUL terminator

	/* process strings of 0--15 bytes (rdx: min(buflen, srclen), rax: srclen) */
.L0015:	cmp		$8, %rdx		# at least 8 bytes to copy?
	jae		.L0815

	cmp		$4, %rdx		# at least 4 bytes to copy?
	jae		.L0407

	cmp		$2, %rdx		# at least 2 bytes to copy?
	jae		.L0203

	movzbl		(%r9), %ecx		# load first byte from src
	mov		%cl, (%rdi)		# deposit into destination
	movb		$0, (%rdi, %rdx, 1)	# add NUL terminator (again)
	ret

.L0203:	movzwl		(%r9), %ecx
	movzwl		-2(%r9, %rdx, 1), %esi
	mov		%cx, (%rdi)
	mov		%si, -2(%rdi, %rdx, 1)
	ret

.L0407:	mov		(%r9), %ecx
	mov		-4(%r9, %rdx, 1), %esi
	mov		%ecx, (%rdi)
	mov		%esi, -4(%rdi, %rdx, 1)
	ret

.L0815:	mov		(%r9), %rcx
	mov		-8(%r9, %rdx, 1), %rsi
	mov		%rcx, (%rdi)
	mov		%rsi, -8(%rdi, %rdx, 1)
	ret

	/* length zero destination: just return the string length */
.L0:	mov		%rsi, %rdi
	jmp		CNAME(strlen)
ARCHEND(__strlcpy, baseline)

	.section .note.GNU-stack,"",%progbits
