/*-
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

#define ALIGN_TEXT      .p2align 4,0x90 /* 16-byte alignment, nop filled */

ARCHFUNCS(timingsafe_bcmp)
	ARCHFUNC(timingsafe_bcmp, scalar)
	ARCHFUNC(timingsafe_bcmp, baseline)
ENDARCHFUNCS(timingsafe_bcmp)

ARCHENTRY(timingsafe_bcmp, scalar)
	cmp	$16, %rdx		# at least 17 bytes to process?
	ja	.Lgt16

	cmp	$8, %edx		# at least 9 bytes to process?
	ja	.L0916

	cmp	$4, %edx		# at least 5 bytes to process?
	ja	.L0508

	cmp	$2, %edx		# at least 3 bytes to process?
	ja	.L0304

	test	%edx, %edx		# buffer empty?
	jnz	.L0102

	xor	%eax, %eax		# empty buffer always matches
	ret

.L0102:	movzbl	(%rdi), %eax		# load 1--2 bytes from first buffer
	movzbl	-1(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %al		# xor in second buffer
	xor	-1(%rsi, %rdx, 1), %cl
	or	%ecx, %eax		# mismatch in any of the two?
	ret

.L0304:	movzwl	(%rdi), %eax
	movzwl	-2(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %ax
	xor	-2(%rsi, %rdx, 1), %cx
	or	%ecx, %eax
	ret

.L0508:	mov	(%rdi), %eax
	mov	-4(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %eax
	xor	-4(%rsi, %rdx, 1), %ecx
	or	%ecx, %eax
	ret

.L0916:	mov	(%rdi), %rax
	mov	-8(%rdi, %rdx, 1), %rcx
	xor	(%rsi), %rax
	xor	-8(%rsi, %rdx, 1), %rcx
	or	%rcx, %rax
	setnz	%al			# ensure EAX nonzero even if only
	ret				# high bits of RAX were set

	/* more than 16 bytes: process buffer in a loop */
.Lgt16:	mov	(%rdi), %rax		# process first 16 bytes
	mov	8(%rdi), %r9
	mov	$32, %ecx
	xor	(%rsi), %rax
	xor	8(%rsi), %r9
	or	%r9, %rax

	cmp	%rdx, %rcx		# enough left for a full iteration?
	jae	.Ltail

	/* main loop processing 16 bytes per iteration */
	ALIGN_TEXT
0:	mov	-16(%rdi, %rcx, 1), %r8
	mov	-8(%rdi, %rcx, 1), %r9
	xor	-16(%rsi, %rcx, 1), %r8
	xor	-8(%rsi, %rcx, 1), %r9
	add	$16, %rcx
	or	%r9, %r8
	or	%r8, %rax

	cmp	%rdx, %rcx
	jb	0b

	/* process last 16 bytes */
.Ltail:	mov	-16(%rdi, %rdx, 1), %r8
	mov	-8(%rdi, %rdx, 1), %r9
	xor	-16(%rsi, %rdx, 1), %r8
	xor	-8(%rsi, %rdx, 1), %r9
	or	%r9, %r8
	or	%r8, %rax
	setnz	%al
	ret
ARCHEND(timingsafe_bcmp, scalar)

ARCHENTRY(timingsafe_bcmp, baseline)
	cmp	$32, %rdx		# at least 33 bytes to process?
	ja	.Lgt32b

	cmp	$16, %edx		# at least 17 bytes to process?
	ja	.L1732b

	cmp	$8, %edx		# at least 9 bytes to process?
	ja	.L0916b

	cmp	$4, %edx		# at least 5 bytes to process?
	ja	.L0508b

	cmp	$2, %edx		# at least 3 bytes to process?
	ja	.L0304b

	test	%edx, %edx		# buffer empty?
	jnz	.L0102b

	xor	%eax, %eax		# empty buffer always matches
	ret

.L0102b:
	movzbl	(%rdi), %eax		# load 1--2 bytes from first buffer
	movzbl	-1(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %al		# xor in second buffer
	xor	-1(%rsi, %rdx, 1), %cl
	or	%ecx, %eax		# mismatch in any of the two?
	ret

.L0304b:
	movzwl	(%rdi), %eax
	movzwl	-2(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %ax
	xor	-2(%rsi, %rdx, 1), %cx
	or	%ecx, %eax
	ret

.L0508b:
	mov	(%rdi), %eax
	mov	-4(%rdi, %rdx, 1), %ecx
	xor	(%rsi), %eax
	xor	-4(%rsi, %rdx, 1), %ecx
	or	%ecx, %eax
	ret

.L0916b:
	mov	(%rdi), %rax
	mov	-8(%rdi, %rdx, 1), %rcx
	xor	(%rsi), %rax
	xor	-8(%rsi, %rdx, 1), %rcx
	or	%rcx, %rax
	setnz	%al			# ensure EAX nonzero even if only
	ret				# high bits of RAX were set

.L1732b:
	movdqu		(%rdi), %xmm0
	movdqu		(%rsi), %xmm2
	movdqu		-16(%rdi, %rdx, 1), %xmm1
	movdqu		-16(%rsi, %rdx, 1), %xmm3
	pcmpeqb		%xmm2, %xmm0
	pcmpeqb		%xmm3, %xmm1
	pand		%xmm1, %xmm0
	pmovmskb	%xmm0, %eax	# 1 where equal
	xor		$0xffff, %eax	# 1 where not equal
	ret

	/* more than 32 bytes: process buffer in a loop */
.Lgt32b:
	movdqu		(%rdi), %xmm4
	movdqu		(%rsi), %xmm2
	movdqu		16(%rdi), %xmm1
	movdqu		16(%rsi), %xmm3
	mov		$64, %ecx
	pcmpeqb		%xmm2, %xmm4
	pcmpeqb		%xmm3, %xmm1
	pand		%xmm1, %xmm4
	cmp		%rdx, %rcx	# enough left for a full iteration?
	jae		.Ltailb

	/* main loop processing 32 bytes per iteration */
	ALIGN_TEXT
0:	movdqu		-32(%rdi, %rcx, 1), %xmm0
	movdqu		-32(%rsi, %rcx, 1), %xmm2
	movdqu		-16(%rdi, %rcx, 1), %xmm1
	movdqu		-16(%rsi, %rcx, 1), %xmm3
	add		$32, %rcx
	pcmpeqb		%xmm2, %xmm0
	pcmpeqb		%xmm3, %xmm1
	pand		%xmm1, %xmm0
	pand		%xmm0, %xmm4
	cmp		%rdx, %rcx
	jb		0b

	/* process last 32 bytes */
.Ltailb:
	movdqu		-32(%rdi, %rdx, 1), %xmm0
	movdqu		-32(%rsi, %rdx, 1), %xmm2
	movdqu		-16(%rdi, %rdx, 1), %xmm1
	movdqu		-16(%rsi, %rdx, 1), %xmm3
	pcmpeqb		%xmm2, %xmm0
	pcmpeqb		%xmm3, %xmm1
	pand		%xmm1, %xmm0
	pand		%xmm4, %xmm0
	pmovmskb	%xmm0, %eax
	xor		$0xffff, %eax
	ret
ARCHEND(timingsafe_bcmp, baseline)

	.section .note.GNU-stack,"",%progbits
