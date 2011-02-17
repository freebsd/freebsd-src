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

	.text

ENTRY(aesni_enc)
	.cfi_startproc
	movdqu	(%rdx),%xmm0
	cmpq	$0,%r8
	je	1f
	movdqu	(%r8),%xmm1	/* unaligned load into reg */
	pxor	%xmm1,%xmm0	/* pxor otherwise can fault on iv */
1:
	pxor	(%rsi),%xmm0
2:
	addq	$0x10,%rsi
//	aesenc	(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdc,0x06
	decl	%edi
	jne	2b
	addq	$0x10,%rsi
//	aesenclast (%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdd,0x06
	movdqu	%xmm0,(%rcx)
	retq
	.cfi_endproc
END(aesni_enc)

ENTRY(aesni_dec)
	.cfi_startproc
	movdqu	(%rdx),%xmm0
	pxor	(%rsi),%xmm0
1:
	addq	$0x10,%rsi
//	aesdec	(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x06
	decl	%edi
	jne	1b
	addq	$0x10,%rsi
//	aesdeclast (%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x06
	cmpq	$0,%r8
	je	2f
	movdqu	(%r8),%xmm1
	pxor	%xmm1,%xmm0
2:
	movdqu	%xmm0,(%rcx)
	retq
	.cfi_endproc
END(aesni_dec)

ENTRY(aesni_decrypt_cbc)
	.cfi_startproc
	shrq	$4,%rdx
	movdqu	(%r8),%xmm1
1:
	movdqu	(%rcx),%xmm0
	movdqa	%xmm0,%xmm2
	pxor	(%rsi),%xmm0
	cmpl	$12,%edi
//	aesdec	0x10(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x10
//	aesdec	0x20(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x20
//	aesdec	0x30(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x30
//	aesdec	0x40(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x40
//	aesdec	0x50(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x50
//	aesdec	0x60(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x60
//	aesdec	0x70(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x46,0x70
//	aesdec	0x80(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0x80,0x00,0x00,0x00
//	aesdec	0x90(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0x90,0x00,0x00,0x00
	jge	2f
//	aesdeclast 0xa0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x86,0xa0,0x00,0x00,0x00
	jmp	4f
2:
//	aesdec	0xa0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0xa0,0x00,0x00,0x00
//	aesdec	0xb0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0xb0,0x00,0x00,0x00
	jg	3f
//	aesdeclast 0xc0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x86,0xc0,0x00,0x00,0x00
	jmp	4f
3:
//	aesdec	0xc0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0xc0,0x00,0x00,0x00
//	aesdec	0xd0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xde,0x86,0xd0,0x00,0x00,0x00
//	aesdeclast 0xe0(%rsi),%xmm0
	.byte	0x66,0x0f,0x38,0xdf,0x86,0xe0,0x00,0x00,0x00
4:
	pxor	%xmm1,%xmm0
	movdqu	%xmm0,(%rcx)
	movdqa	%xmm2,%xmm1	// iv
	addq	$0x10,%rcx
	decq	%rdx
	jne	1b
	retq
	.cfi_endproc
END(aesni_decrypt_cbc)

	.ident	"$FreeBSD$"
