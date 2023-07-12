/*-
 * Copyright (c) 2018, 2023 The FreeBSD Foundation
 *
 * This software was developed by Mateusz Guzik <mjg@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <machine/param.h>

#include "amd64_archlevel.h"

/*
 * Note: this routine was written with kernel use in mind (read: no simd),
 * it is only present in userspace as a temporary measure until something
 * better gets imported.
 */

#define ALIGN_TEXT      .p2align 4,0x90 /* 16-byte alignment, nop filled */

#ifdef BCMP
#define memcmp bcmp
#endif

ARCHFUNCS(memcmp)
	ARCHFUNC(memcmp, scalar)
	ARCHFUNC(memcmp, baseline)
ENDARCHFUNCS(memcmp)

ARCHENTRY(memcmp, scalar)
	xorl	%eax,%eax
10:
	cmpq	$16,%rdx
	ja	101632f

	cmpb	$8,%dl
	jg	100816f

	cmpb	$4,%dl
	jg	100408f

	cmpb	$2,%dl
	jge	100204f

	cmpb	$1,%dl
	jl	100000f
	movzbl	(%rdi),%eax
	movzbl	(%rsi),%r8d
	subl	%r8d,%eax
100000:
	ret

	ALIGN_TEXT
100816:
	movq	(%rdi),%r8
	movq	(%rsi),%r9
	cmpq	%r8,%r9
	jne	80f
	movq	-8(%rdi,%rdx),%r8
	movq	-8(%rsi,%rdx),%r9
	cmpq	%r8,%r9
	jne	10081608f
	ret
	ALIGN_TEXT
100408:
	movl	(%rdi),%r8d
	movl	(%rsi),%r9d
	cmpl	%r8d,%r9d
	jne	80f
	movl	-4(%rdi,%rdx),%r8d
	movl	-4(%rsi,%rdx),%r9d
	cmpl	%r8d,%r9d
	jne	10040804f
	ret
	ALIGN_TEXT
100204:
	movzwl	(%rdi),%r8d
	movzwl	(%rsi),%r9d
	cmpl	%r8d,%r9d
	jne	1f
	movzwl	-2(%rdi,%rdx),%r8d
	movzwl	-2(%rsi,%rdx),%r9d
	cmpl	%r8d,%r9d
	jne	1f
	ret
	ALIGN_TEXT
101632:
	cmpq	$32,%rdx
	ja	103200f
	movq	(%rdi),%r8
	movq	(%rsi),%r9
	cmpq	%r8,%r9
	jne	80f
	movq	8(%rdi),%r8
	movq	8(%rsi),%r9
	cmpq	%r8,%r9
	jne	10163208f
	movq	-16(%rdi,%rdx),%r8
	movq	-16(%rsi,%rdx),%r9
	cmpq	%r8,%r9
	jne	10163216f
	movq	-8(%rdi,%rdx),%r8
	movq	-8(%rsi,%rdx),%r9
	cmpq	%r8,%r9
	jne	10163224f
	ret
	ALIGN_TEXT
103200:
	movq	(%rdi),%r8
	movq	8(%rdi),%r9
	subq	(%rsi),%r8
	subq	8(%rsi),%r9
	orq	%r8,%r9
	jnz	10320000f

	movq    16(%rdi),%r8
	movq    24(%rdi),%r9
	subq    16(%rsi),%r8
	subq    24(%rsi),%r9
	orq	%r8,%r9
	jnz     10320016f

	leaq	32(%rdi),%rdi
	leaq	32(%rsi),%rsi
	subq	$32,%rdx
	cmpq	$32,%rdx
	jae	103200b
	cmpb	$0,%dl
	jne	10b
	ret

/*
 * Mismatch was found.
 */
#ifdef BCMP
	ALIGN_TEXT
10320016:
10320000:
10081608:
10163224:
10163216:
10163208:
10040804:
80:
1:
	leal	1(%eax),%eax
	ret
#else
/*
 * We need to compute the difference between strings.
 * Start with narrowing the range down (16 -> 8 -> 4 bytes).
 */
	ALIGN_TEXT
10320016:
	leaq	16(%rdi),%rdi
	leaq	16(%rsi),%rsi
10320000:
	movq	(%rdi),%r8
	movq	(%rsi),%r9
	cmpq	%r8,%r9
	jne	80f
	leaq	8(%rdi),%rdi
	leaq	8(%rsi),%rsi
	jmp	80f
	ALIGN_TEXT
10081608:
10163224:
	leaq	-8(%rdi,%rdx),%rdi
	leaq	-8(%rsi,%rdx),%rsi
	jmp	80f
	ALIGN_TEXT
10163216:
	leaq	-16(%rdi,%rdx),%rdi
	leaq	-16(%rsi,%rdx),%rsi
	jmp	80f
	ALIGN_TEXT
10163208:
	leaq	8(%rdi),%rdi
	leaq	8(%rsi),%rsi
	jmp	80f
	ALIGN_TEXT
10040804:
	leaq	-4(%rdi,%rdx),%rdi
	leaq	-4(%rsi,%rdx),%rsi
	jmp	1f

	ALIGN_TEXT
80:
	movl	(%rdi),%r8d
	movl	(%rsi),%r9d
	cmpl	%r8d,%r9d
	jne	1f
	leaq	4(%rdi),%rdi
	leaq	4(%rsi),%rsi

/*
 * We have up to 4 bytes to inspect.
 */
1:
	movzbl	(%rdi),%eax
	movzbl	(%rsi),%r8d
	cmpb	%r8b,%al
	jne	2f

	movzbl	1(%rdi),%eax
	movzbl	1(%rsi),%r8d
	cmpb	%r8b,%al
	jne	2f

	movzbl	2(%rdi),%eax
	movzbl	2(%rsi),%r8d
	cmpb	%r8b,%al
	jne	2f

	movzbl	3(%rdi),%eax
	movzbl	3(%rsi),%r8d
2:
	subl	%r8d,%eax
	ret
#endif
ARCHEND(memcmp, scalar)

ARCHENTRY(memcmp, baseline)
	cmp		$32, %rdx		# enough to permit use of the long kernel?
	ja		.Llong

	test		%rdx, %rdx		# zero bytes buffer?
	je		.L0

	/*
	 * Compare strings of 1--32 bytes.  We want to do this by
	 * loading into two xmm registers and then comparing.  To avoid
	 * crossing into unmapped pages, we either load 32 bytes from
	 * the start of the buffer or 32 bytes before its end, depending
	 * on whether there is a page boundary between the overread area
	 * or not.
	 */

	/* check for page boundaries overreads */
	lea		31(%rdi), %eax		# end of overread
	lea		31(%rsi), %r8d
	lea		-1(%rdi, %rdx, 1), %ecx	# last character in buffer
	lea		-1(%rsi, %rdx, 1), %r9d
	xor		%ecx, %eax
	xor		%r9d, %r8d
	test		$PAGE_SIZE, %eax	# are they on different pages?
	jz		0f

	/* fix up rdi */
	movdqu		-32(%rdi, %rdx, 1), %xmm0
	movdqu		-16(%rdi, %rdx, 1), %xmm1
	lea		-8(%rsp), %rdi		# end of replacement buffer
	sub		%rdx, %rdi		# start of replacement buffer
	movdqa		%xmm0, -40(%rsp)	# copy to replacement buffer
	movdqa		%xmm1, -24(%rsp)

0:	test		$PAGE_SIZE, %r8d
	jz		0f

	/* fix up rsi */
	movdqu		-32(%rsi, %rdx, 1), %xmm0
	movdqu		-16(%rsi, %rdx, 1), %xmm1
	lea		-40(%rsp), %rsi		# end of replacement buffer
	sub		%rdx, %rsi		# start of replacement buffer
	movdqa		%xmm0, -72(%rsp)	# copy to replacement buffer
	movdqa		%xmm1, -56(%rsp)

	/* load data and compare properly */
0:	movdqu		16(%rdi), %xmm1
	movdqu		16(%rsi), %xmm3
	movdqu		(%rdi), %xmm0
	movdqu		(%rsi), %xmm2
	mov		%edx, %ecx
	mov		$-1, %edx
	shl		%cl, %rdx		# ones where the buffer is not
	pcmpeqb		%xmm3, %xmm1
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm1, %ecx
	pmovmskb	%xmm0, %eax
	shl		$16, %ecx
	or		%ecx, %eax		# ones where the buffers match
	or		%edx, %eax		# including where the buffer is not
	not		%eax			# ones where there is a mismatch
#ifndef BCMP
	bsf		%eax, %edx		# location of the first mismatch
	cmovz		%eax, %edx		# including if there is no mismatch
	movzbl		(%rdi, %rdx, 1), %eax	# mismatching bytes
	movzbl		(%rsi, %rdx, 1), %edx
	sub		%edx, %eax
#endif
	ret

	/* empty input */
.L0:	xor		%eax, %eax
	ret

	/* compare 33+ bytes */
	ALIGN_TEXT
.Llong:	movdqu		(%rdi), %xmm0		# load head
	movdqu		(%rsi), %xmm2
	mov		%rdi, %rcx
	sub		%rdi, %rsi		# express rsi as distance from rdi
	and		$~0xf, %rdi		# align rdi to 16 bytes
	movdqu		16(%rsi, %rdi, 1), %xmm1
	pcmpeqb		16(%rdi), %xmm1		# compare second half of this iteration
	add		%rcx, %rdx		# pointer to last byte in buffer
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm0, %eax
	xor		$0xffff, %eax		# any mismatch?
	jne		.Lmismatch_head
	add		$64, %rdi		# advance to next iteration
	jmp		1f			# and get going with the loop

	/* process buffer 32 bytes at a time */
	ALIGN_TEXT
0:	movdqu		-32(%rsi, %rdi, 1), %xmm0
	movdqu		-16(%rsi, %rdi, 1), %xmm1
	pcmpeqb		-32(%rdi), %xmm0
	pcmpeqb		-16(%rdi), %xmm1
	add		$32, %rdi		# advance to next iteration
1:	pand		%xmm0, %xmm1		# 0xff where both halves matched
	pmovmskb	%xmm1, %eax
	cmp		$0xffff, %eax		# all bytes matched?
	jne		.Lmismatch
	cmp		%rdx, %rdi		# end of buffer reached?
	jb		0b

	/* less than 32 bytes left to compare */
	movdqu		-16(%rdx), %xmm1	# load 32 byte tail through end pointer
	movdqu		-16(%rdx, %rsi, 1), %xmm3
	movdqu		-32(%rdx), %xmm0
	movdqu		-32(%rdx, %rsi, 1), %xmm2
	pcmpeqb		%xmm3, %xmm1
	pcmpeqb		%xmm2, %xmm0
	pmovmskb	%xmm1, %ecx
	pmovmskb	%xmm0, %eax
	shl		$16, %ecx
	or		%ecx, %eax		# ones where the buffers match
	not		%eax			# ones where there is a mismatch
#ifndef BCMP
	bsf		%eax, %ecx		# location of the first mismatch
	cmovz		%eax, %ecx		# including if there is no mismatch
	add		%rcx, %rdx		# pointer to potential mismatch
	movzbl		-32(%rdx), %eax		# mismatching bytes
	movzbl		-32(%rdx, %rsi, 1), %edx
	sub		%edx, %eax
#endif
	ret

#ifdef BCMP
.Lmismatch:
	mov		$1, %eax
.Lmismatch_head:
	ret
#else /* memcmp */
.Lmismatch_head:
	tzcnt		%eax, %eax		# location of mismatch
	add		%rax, %rcx		# pointer to mismatch
	movzbl		(%rcx), %eax		# mismatching bytes
	movzbl		(%rcx, %rsi, 1), %ecx
	sub		%ecx, %eax
	ret

.Lmismatch:
	movdqu		-48(%rsi, %rdi, 1), %xmm1
	pcmpeqb		-48(%rdi), %xmm1	# reconstruct xmm1 before PAND
	pmovmskb	%xmm0, %eax		# mismatches in first 16 bytes
	pmovmskb	%xmm1, %edx		# mismatches in second 16 bytes
	shl		$16, %edx
	or		%edx, %eax		# mismatches in both
	not		%eax			# matches in both
	tzcnt		%eax, %eax		# location of mismatch
	add		%rax, %rdi		# pointer to mismatch
	movzbl		-64(%rdi), %eax		# mismatching bytes
	movzbl		-64(%rdi, %rsi, 1), %ecx
	sub		%ecx, %eax
	ret
#endif
ARCHEND(memcmp, baseline)

	.section .note.GNU-stack,"",%progbits
