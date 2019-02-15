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

.macro MEMSET bzero erms
.if \bzero == 1
	movq	%rsi,%rcx
	movq	%rsi,%rdx
	xorl	%eax,%eax
.else
	movq	%rdi,%r9
	movq	%rdx,%rcx
	movzbq	%sil,%r8
	movabs	$0x0101010101010101,%rax
	imulq	%r8,%rax
.endif

	cmpq	$32,%rcx
	jb	1016f

	cmpq	$256,%rcx
	ja	1256f

1032:
	movq	%rax,(%rdi)
	movq	%rax,8(%rdi)
	movq	%rax,16(%rdi)
	movq	%rax,24(%rdi)
	leaq	32(%rdi),%rdi
	subq	$32,%rcx
	cmpq	$32,%rcx
	jae	1032b
	cmpb	$0,%cl
	je	1000f
1016:
	cmpb	$16,%cl
	jl	1008f
	movq	%rax,(%rdi)
	movq	%rax,8(%rdi)
	subb	$16,%cl
	jz	1000f
	leaq	16(%rdi),%rdi
1008:
	cmpb	$8,%cl
	jl	1004f
	movq	%rax,(%rdi)
	subb	$8,%cl
	jz	1000f
	leaq	8(%rdi),%rdi
1004:
	cmpb	$4,%cl
	jl	1002f
	movl	%eax,(%rdi)
	subb	$4,%cl
	jz	1000f
	leaq	4(%rdi),%rdi
1002:
	cmpb	$2,%cl
	jl	1001f
	movw	%ax,(%rdi)
	subb	$2,%cl
	jz	1000f
	leaq	2(%rdi),%rdi
1001:
	cmpb	$1,%cl
	jl	1000f
	movb	%al,(%rdi)
1000:
.if \bzero == 0
	movq	%r9,%rax
.endif
	ret

1256:
.if \erms == 1
	rep
	stosb
.else
	shrq	$3,%rcx
	rep
	stosq
	movq	%rdx,%rcx
	andb	$7,%cl
	jne	1004b
.endif
.if \bzero == 0
	movq	%r9,%rax
.endif
	ret
.endm

#ifndef BZERO
ENTRY(memset)
	MEMSET bzero=0 erms=0
END(memset)
#else
ENTRY(bzero)
	MEMSET bzero=1 erms=0
END(bzero)
#endif

	.section .note.GNU-stack,"",%progbits
