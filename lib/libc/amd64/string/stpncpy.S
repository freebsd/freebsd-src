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

	.weak stpncpy
	.set stpncpy, __stpncpy
ARCHFUNCS(__stpncpy)
	ARCHFUNC(__stpncpy, scalar)
	ARCHFUNC(__stpncpy, baseline)
ENDARCHFUNCS(__stpncpy)

ARCHENTRY(__stpncpy, scalar)
	push	%rbp		# establish stack frame
	mov	%rsp, %rbp

	push	%rdx
	push	%rdi
	push	%rsi
	push	%rax		# dummy push for alignment

	mov	%rsi, %rdi
	xor	%esi, %esi
	call	CNAME(__memchr)	# memchr(src, '\0', len)
	pop	%rcx		# dummy pop
	pop	%rsi
	mov	-16(%rbp), %rdi

	test	%rax, %rax	# NUL found?
	jz	.Lfullcopy

	mov	%rax, %rdx
	sub	%rsi, %rdx	# copy until the NUL byte
	add	%rdx, -16(%rbp)	# advance destination by string length
	sub	%rdx, -8(%rbp)	# and shorten buffer size by string length
	call	CNAME(memcpy)

	pop	%rdi
	pop	%rdx
	xor	%esi, %esi
	pop	%rbp
	jmp	CNAME(memset)	# clear remaining buffer

.Lfullcopy:
	mov	-8(%rbp), %rdx
	call	CNAME(memcpy)	# copy whole string
	add	-8(%rbp), %rax	# point to dest[n]
	leave
	ret
ARCHEND(__stpncpy, scalar)

	/*
	 * this mask allows us to generate masks of 16-n 0xff bytes
	 * followed by n 0x00 bytes by loading from .Lmask+n.
	 */
	.section	.rodata
.Lmask:	.quad		0xffffffffffffffff
	.quad		0xffffffffffffffff
	.quad		0x0000000000000000
	.quad		0x0000000000000000

/* stpncpy(char *restrict rdi, const char *rsi, size_t rdx) */
ARCHENTRY(__stpncpy, baseline)
#define bounce		(-3*16-8)		/* location of on-stack bounce buffer */

	test		%rdx, %rdx		# no bytes to copy?
	jz		.L0

	mov		%esi, %ecx
	and		$~0xf, %rsi		# align source to 16 bytes
	movdqa		(%rsi), %xmm0		# load head
	and		$0xf, %ecx		# offset from alignment
	mov		$-1, %r9d
	lea		-32(%rcx), %rax		# set up overflow-proof comparison rdx+rcx<=32
	shl		%cl, %r9d		# mask of bytes belonging to the string
	sub		%rcx, %rdi		# adjust RDI to correspond to RSI
	pxor		%xmm1, %xmm1
	movdqa		%xmm0, bounce(%rsp)	# stash copy of head on the stack
	pcmpeqb		%xmm1, %xmm0
	pmovmskb	%xmm0, %r8d

	lea		(%rdx, %rcx, 1), %r10	# buffer length from alignment boundary
	add		%rdx, %rax		# less than 2 chunks (32 bytes) to play with?
	jnc		.Lrunt			# if yes, use special runt processing

	movdqu		%xmm1, -16(%rdi, %r10, 1) # clear final bytes of destination
	and		%r9d, %r8d		# end of string within head?
	jnz		.Lheadnul

	movdqu		(%rsi, %rcx, 1), %xmm2	# load head from source buffer
	movdqu		%xmm2, (%rdi, %rcx, 1)	# an deposit

	add		$16, %rsi
	add		$16, %rdi
	sub		$32, %r10

	/* main loop unrolled twice */
	ALIGN_TEXT
0:	movdqa		(%rsi), %xmm0
	pxor		%xmm1, %xmm1
	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
	pmovmskb	%xmm1, %r8d
	test		%r8d, %r8d
	jnz		3f

	movdqu		%xmm0, (%rdi)
	cmp		$16, %r10		# more than a full chunk left?
	jbe		1f

	movdqa		16(%rsi), %xmm0
	add		$32, %rdi		# advance pointers to next chunk
	add		$32, %rsi
	pxor		%xmm1, %xmm1
	pcmpeqb		%xmm0, %xmm1		# NUL byte encountered?
	pmovmskb	%xmm1, %r8d
	test		%r8d, %r8d
	jnz		2f

	movdqu		%xmm0, -16(%rdi)
	sub		$32, %r10		# more than another full chunk left?
	ja		0b

	sub		$16, %rdi		# undo second advancement
	sub		$16, %rsi
	add		$16, %r10d		# restore number of remaining bytes

	/* 1--16 bytes left but string has not ended yet */
1:	pxor		%xmm1, %xmm1
	pcmpeqb		16(%rsi), %xmm1		# NUL byte in source tail?
	pmovmskb	%xmm1, %r8d
	bts		%r10d, %r8d		# treat end of buffer as NUL
	tzcnt		%r8d, %r8d		# where is the NUL byte?
	movdqu		(%rsi, %r8, 1), %xmm0	# load source tail before NUL
	lea		16(%rdi, %r8, 1), %rax	# point return value to NUL byte
						# or end of buffer
	movdqu		%xmm0, (%rdi, %r8, 1)	# store tail into the buffer
	ret

2:	sub		$16, %rdi		# undo second advancement
	sub		$16, %rsi
	sub		$16, %r10

	/* string has ended and buffer has not */
3:	tzcnt		%r8d, %r8d		# where did the string end?
	lea		.Lmask+16(%rip), %rcx
	lea		(%rdi, %r8, 1), %rax 	# where the NUL byte will be
	neg		%r8
	movdqu		(%rcx, %r8, 1), %xmm1	# mask with FF where the string is,
						# 00 where it is not
	pand		%xmm1, %xmm0		# mask out bytes after the string
	movdqu		%xmm0, (%rdi)	 	# store masked current chunk
	pxor		%xmm1, %xmm1
	sub		$16, %r10		# another full chunk left?
	jbe		1f

	/* clear remaining destination buffer (tail has been cleared earlier) */
	ALIGN_TEXT
0:	movdqu		%xmm1, 16(%rdi)
	cmp		$16, %r10
	jbe		1f

	movdqu		%xmm1, 32(%rdi)
	add		$32, %rdi
	sub		$32, %r10
	ja		0b

1:	ret

	/* at least two chunks to play with and NUL while processing head */
.Lheadnul:
	movdqu		bounce(%rsp, %rcx, 1), %xmm0 # load start of source from stack
	tzcnt		%r8d, %r8d		# find location of NUL byte
	movdqu		%xmm0, (%rdi, %rcx, 1)	# deposit head in the destination
	movdqu		%xmm1, (%rdi, %r8, 1)	# clear out following bytes
	movdqu		%xmm1, 16(%rdi)		# clear out second chunk
	lea		(%rdi, %r8, 1), %rax	# make RAX point to the NUL byte

	add		$32, %rdi		# advance past first two chunks
	sub		$32+16, %r10		# advance past first three chunks
	jbe		1f			# did we pass the end of the buffer?

	/* clear remaining destination buffer (tail has been cleared earlier) */
	ALIGN_TEXT
0:	movdqu		%xmm1, (%rdi)		# clear out buffer chunk
	cmp		$16, %r10
	jbe		1f

	movdqu		%xmm1, 16(%rdi)
	add		$32, %rdi
	sub		$32, %r10
	ja		0b

1:	ret

	/* 1--32 bytes to copy, bounce through the stack */
.Lrunt:	movdqa		%xmm1, bounce+16(%rsp)	# clear out rest of on-stack copy
	bts		%r10d, %r8d		# treat end of buffer as end of string
	and		%r9w, %r8w		# end of string within first buffer?
	jnz		0f			# if yes, do not inspect second buffer

	movdqa		16(%rsi), %xmm0		# load second chunk of input
	movdqa		%xmm0, bounce+16(%rsp)	# stash copy on stack
	pcmpeqb		%xmm1, %xmm0		# NUL in second chunk?
	pmovmskb	%xmm0, %r9d
	shl		$16, %r9d
	or		%r9d, %r8d		# merge found NUL bytes into NUL mask

	/* end of string after one buffer */
0:	tzcnt		%r8d, %r8d		# location of last char in string
	movdqu		%xmm1, bounce(%rsp, %r8, 1) # clear bytes behind string
	lea		bounce(%rsp, %rcx, 1), %rsi # start of string copy on stack
	lea		(%rdi, %r8, 1), %rax	# return pointer to NUL byte

	cmp		$16, %edx		# at least 16 bytes to transfer?
	jae		.L1631

	mov		(%rsi), %r8		# load string head
	cmp		$8, %edx		# at least 8 bytes to transfer?
	jae		.L0815

	cmp		$4, %edx		# at least 4 bytes to transfer?
	jae		.L0407

	movzwl		-2(%rsi, %rdx, 1), %esi	# load last two bytes of string
	mov		%r8b, (%rdi, %rcx, 1)	# store first byte

	cmp		$2, %edx		# at least 2 bytes to transfer?
	jb		.L1

	mov		%si, -2(%rdi, %r10, 1)	# store last two bytes of string
.L1:	ret

.L1631:	movdqu		(%rsi), %xmm0		# load first 16 bytes of string
	movdqu		-16(%rsi, %rdx, 1), %xmm1 # load last 16 bytes of string
	movdqu		%xmm0, (%rdi, %rcx, 1)
	movdqu		%xmm1, -16(%rdi, %r10, 1)
	ret

.L0815:	mov		-8(%rsi, %rdx, 1), %rdx	# load last 8 bytes of string
	mov		%r8, (%rdi, %rcx, 1)
	mov		%rdx, -8(%rdi, %r10, 1)
	ret

.L0407:	mov		-4(%rsi, %rdx, 1), %edx	# load last four bytes of string
	mov		%r8d, (%rdi, %rcx, 1)
	mov		%edx, -4(%rdi, %r10, 1)
	ret

	/* length 0 buffer: just return dest */
.L0:	mov		%rdi, %rax
	ret
ARCHEND(__stpncpy, baseline)

	.section .note.GNU-stack,"",%progbits
