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

#define ALIGN_TEXT	.p2align 4,0x90 /* 16-byte alignment, nop filled */

/* int timingsafe_memcmp(const void *rdi, const void *rsi, size_t rdx) */
ENTRY(timingsafe_memcmp)
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

.L0102:	movzbl	-1(%rdi, %rdx, 1), %eax	# load 1--2 bytes from first buffer
	movzbl	-1(%rsi, %rdx, 1), %ecx
	mov	(%rdi), %ah		# in big endian
	mov	(%rsi), %ch
	sub	%ecx, %eax
	ret

.L0304:	movzwl	-2(%rdi, %rdx, 1), %ecx
	movzwl	-2(%rsi, %rdx, 1), %edx
	movzwl	(%rdi), %eax
	movzwl	(%rsi), %esi
	bswap	%ecx			# convert to big endian
	bswap	%edx			# dito for edx, (e)ax, and (e)si
	rol	$8, %ax			# ROLW is used here so the upper two
	rol	$8, %si			# bytes stay clear, allowing us to
	sub	%edx, %ecx		# save a SBB compared to .L0508
	sbb	%esi, %eax
	or	%eax, %ecx		# nonzero if not equal
	setnz	%al
	ret

.L0508:	mov	-4(%rdi, %rdx, 1), %ecx
	mov	-4(%rsi, %rdx, 1), %edx
	mov	(%rdi), %edi
	mov	(%rsi), %esi
	bswap	%ecx			# compare in big endian
	bswap	%edx
	bswap	%edi
	bswap	%esi
	sub	%edx, %ecx
	sbb	%esi, %edi
	sbb	%eax, %eax		# -1 if less, 0 if greater or equal
	or	%edi, %ecx		# nonzero if not equal
	setnz	%al			# negative if <, 0 if =, 1 if >
	ret

.L0916:	mov	-8(%rdi, %rdx, 1), %rcx
	mov	-8(%rsi, %rdx, 1), %rdx
	mov	(%rdi), %rdi
	mov	(%rsi), %rsi
	bswap	%rcx			# compare in big endian
	bswap	%rdx
	bswap	%rdi
	bswap	%rsi
	sub	%rdx, %rcx
	sbb	%rsi, %rdi
	sbb	%eax, %eax		# -1 if less, 0 if greater or equal
	or	%rdi, %rcx		# nonzero if not equal
	setnz	%al			# negative if <, 0 if =, 1 if >
	ret

	/* compare 17+ bytes */
.Lgt16:	mov	(%rdi), %r8		# process first 16 bytes
	mov	(%rsi), %r9
	mov	$32, %ecx
	cmp	%r8, %r9		# mismatch in head?
	cmove	8(%rdi), %r8		# if not, try second pair
	cmove	8(%rsi), %r9
	cmp	%rdx, %rcx
	jae	.Ltail

	/* main loop processing 16 bytes per iteration */
	ALIGN_TEXT
0:	mov	-16(%rdi, %rcx, 1), %r10
	mov	-16(%rsi, %rcx, 1), %r11
	cmp	%r10, %r11		# mismatch in first pair?
	cmove	-8(%rdi, %rcx, 1), %r10	# if not, try second pair
	cmove	-8(%rsi, %rcx, 1), %r11
	cmp	%r8, %r9		# was there a mismatch previously?
	cmove	%r10, %r8		# apply new pair if there was not
	cmove	%r11, %r9
	add	$16, %rcx
	cmp	%rdx, %rcx
	jb	0b

.Ltail:	mov	-8(%rdi, %rdx, 1), %r10
	mov	-8(%rsi, %rdx, 1), %r11
	cmp	%r8, %r9
	cmove	-16(%rdi, %rdx, 1), %r8
	cmove	-16(%rsi, %rdx, 1), %r9
	bswap	%r10			# compare in big endian
	bswap	%r11
	bswap	%r8
	bswap	%r9
	sub	%r11, %r10
	sbb	%r9, %r8
	sbb	%eax, %eax		# -1 if less, 0 if greater or equal
	or	%r10, %r8		# nonzero if not equal
	setnz	%al			# negative if <, 0 if =, 1 if >
	ret
END(timingsafe_memcmp)

	.section .note.GNU-stack,"",%progbits
