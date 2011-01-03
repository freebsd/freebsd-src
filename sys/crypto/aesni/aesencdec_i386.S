/*-
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asmacros.h>

ENTRY(aesni_enc)
	.cfi_startproc
	pushl	%ebp
	.cfi_adjust_cfa_offset 4
	movl	%esp,%ebp
	movl	8(%ebp),%ecx	/* rounds */
	movl	16(%ebp),%edx
	movdqu	(%edx),%xmm0	/* from */
	movl	24(%ebp),%eax	/* iv */
	cmpl	$0,%eax
	je	1f
	movdqu	(%eax),%xmm1
	pxor	%xmm1,%xmm0
1:
	movl	12(%ebp),%eax	/* key */
	pxor	(%eax),%xmm0
2:
	addl	$0x10,%eax
//	aesenc	(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdc,0x00
	loopne	2b
	addl	$0x10,%eax
//	aesenclast (%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdd,0x00
	movl	20(%ebp),%eax
	movdqu	%xmm0,(%eax)	/* to */
	leave
	.cfi_adjust_cfa_offset -4
	retl
	.cfi_endproc
END(aesni_enc)

ENTRY(aesni_dec)
	.cfi_startproc
	pushl	%ebp
	.cfi_adjust_cfa_offset 4
	movl	%esp,%ebp
	movl	8(%ebp),%ecx	/* rounds */
	movl	16(%ebp),%edx
	movdqu	(%edx),%xmm0	/* from */
	movl	12(%ebp),%eax	/* key */
	pxor	(%eax),%xmm0
1:
	addl	$0x10,%eax
//	aesdec	(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x00
	loopne	1b
	addl	$0x10,%eax
//	aesdeclast (%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x00
	movl	24(%ebp),%eax
	cmpl	$0,%eax		/* iv */
	je	2f
	movdqu	(%eax),%xmm1
	pxor	%xmm1,%xmm0
2:
	movl	20(%ebp),%eax
	movdqu	%xmm0,(%eax)	/* to */
	leave
	.cfi_adjust_cfa_offset -4
	retl
	.cfi_endproc
END(aesni_dec)

ENTRY(aesni_decrypt_cbc)
	.cfi_startproc
	pushl	%ebp
	.cfi_adjust_cfa_offset 4
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	movl	12(%ebp),%eax	/* key */
	movl	16(%ebp),%ecx	/* length */
	shrl	$4,%ecx
	movl	20(%ebp),%ebx	/* buf */
	movl	24(%ebp),%esi
	movdqu	(%esi),%xmm1	/* iv */
	movl	8(%ebp),%esi	/* rounds */
1:
	movdqu	(%ebx),%xmm0
	movdqa	%xmm0,%xmm2
	pxor	(%eax),%xmm0
	cmpl	$12,%esi
//	aesdec	0x10(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x10
//	aesdec	0x20(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x20
//	aesdec	0x30(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x30
//	aesdec	0x40(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x40
//	aesdec	0x50(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x50
//	aesdec	0x60(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x60
//	aesdec	0x70(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x40,0x70
//	aesdec	0x80(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0x80,0x00,0x00,0x00
//	aesdec	0x90(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0x90,0x00,0x00,0x00
	jge	2f
//	aesdeclast 0xa0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x80,0xa0,0x00,0x00,0x00
	jmp	4f
2:
//	aesdec	0xa0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0xa0,0x00,0x00,0x00
//	aesdec	0xb0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0xb0,0x00,0x00,0x00
	jg	3f
//	aesdeclast 0xc0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x80,0xc0,0x00,0x00,0x00
	jmp	4f
3:
//	aesdec	0xc0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0xc0,0x00,0x00,0x00
//	aesdec	0xd0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x80,0xd0,0x00,0x00,0x00
//	aesdeclast 0xe0(%eax),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x80,0xe0,0x00,0x00,0x00
4:
	pxor	%xmm1,%xmm0
	movdqu	%xmm0,(%ebx)
	movdqa	%xmm2,%xmm1
	addl	$0x10,%ebx
	decl	%ecx
	jne	1b

	popl	%esi
	popl	%ebx
	leave
	.cfi_adjust_cfa_offset -4
	retl
	.cfi_endproc
END(aesni_decrypt_cbc)

	.ident	"$FreeBSD$"
