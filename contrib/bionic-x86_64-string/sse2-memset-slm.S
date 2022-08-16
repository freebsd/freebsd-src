/*
Copyright (c) 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    * this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    * this list of conditions and the following disclaimer in the documentation
    * and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its contributors
    * may be used to endorse or promote products derived from this software
    * without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <private/bionic_asm.h>

#include "cache.h"

#ifndef L
# define L(label)	.L##label
#endif

#ifndef ALIGN
# define ALIGN(n)	.p2align n
#endif


ENTRY(__memset_chk_generic)
  # %rdi = dst, %rsi = byte, %rdx = n, %rcx = dst_len
  cmp %rcx, %rdx
  ja __memset_chk_fail
  // Fall through to memset...
END(__memset_chk_generic)


	.section .text.sse2,"ax",@progbits
ENTRY(memset_generic)
	movq	%rdi, %rax
	and	$0xff, %rsi
	mov	$0x0101010101010101, %rcx
	imul	%rsi, %rcx
	cmpq	$16, %rdx
	jae	L(16bytesormore)
	testb	$8, %dl
	jnz	L(8_15bytes)
	testb	$4, %dl
	jnz	L(4_7bytes)
	testb	$2, %dl
	jnz	L(2_3bytes)
	testb	$1, %dl
	jz	L(return)
	movb	%cl, (%rdi)
L(return):
	ret

L(8_15bytes):
	movq	%rcx, (%rdi)
	movq	%rcx, -8(%rdi, %rdx)
	ret

L(4_7bytes):
	movl	%ecx, (%rdi)
	movl	%ecx, -4(%rdi, %rdx)
	ret

L(2_3bytes):
	movw	%cx, (%rdi)
	movw	%cx, -2(%rdi, %rdx)
	ret

	ALIGN (4)
L(16bytesormore):
	movd	%rcx, %xmm0
	pshufd	$0, %xmm0, %xmm0
	movdqu	%xmm0, (%rdi)
	movdqu	%xmm0, -16(%rdi, %rdx)
	cmpq	$32, %rdx
	jbe	L(32bytesless)
	movdqu	%xmm0, 16(%rdi)
	movdqu	%xmm0, -32(%rdi, %rdx)
	cmpq	$64, %rdx
	jbe	L(64bytesless)
	movdqu	%xmm0, 32(%rdi)
	movdqu	%xmm0, 48(%rdi)
	movdqu	%xmm0, -64(%rdi, %rdx)
	movdqu	%xmm0, -48(%rdi, %rdx)
	cmpq	$128, %rdx
	ja	L(128bytesmore)
L(32bytesless):
L(64bytesless):
	ret

	ALIGN (4)
L(128bytesmore):
	leaq	64(%rdi), %rcx
	andq	$-64, %rcx
	movq	%rdx, %r8
	addq	%rdi, %rdx
	andq	$-64, %rdx
	cmpq	%rcx, %rdx
	je	L(return)

#ifdef SHARED_CACHE_SIZE
	cmp	$SHARED_CACHE_SIZE, %r8
#else
	cmp	__x86_64_shared_cache_size(%rip), %r8
#endif
	ja	L(128bytesmore_nt)

	ALIGN (4)
L(128bytesmore_normal):
	movdqa	%xmm0, (%rcx)
	movaps	%xmm0, 0x10(%rcx)
	movaps	%xmm0, 0x20(%rcx)
	movaps	%xmm0, 0x30(%rcx)
	addq	$64, %rcx
	cmpq	%rcx, %rdx
	jne	L(128bytesmore_normal)
	ret

	ALIGN (4)
L(128bytesmore_nt):
	movntdq	%xmm0, (%rcx)
	movntdq	%xmm0, 0x10(%rcx)
	movntdq	%xmm0, 0x20(%rcx)
	movntdq	%xmm0, 0x30(%rcx)
	leaq	64(%rcx), %rcx
	cmpq	%rcx, %rdx
	jne	L(128bytesmore_nt)
	sfence
	ret

END(memset_generic)
