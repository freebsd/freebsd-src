/*-
 * Copyright (c) 2018 The FreeBSD Foundation
 *
 * This software was developed by Mateusz Guzik <mjg@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Note: this routine was written with kernel use in mind (read: no simd),
 * it is only present in userspace as a temporary measure until something
 * better gets imported.
 */

#define ALIGN_TEXT      .p2align 4,0x90 /* 16-byte alignment, nop filled */

#ifdef BCMP
ENTRY(bcmp)
#else
ENTRY(memcmp)
#endif
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
END(bcmp)
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
END(memcmp)
#endif

	.section .note.GNU-stack,"",%progbits
