/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * Copyright 2021 David Sebek <dasebek@gmail.com>
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <machine/asmacros.h>

	.text
	.globl	amd64_tramp_inline

/*
 * void amd64_tramp_inline(uint64_t stack %rdi, uint64_t kernend %rsi,
 *    uint64_t modulep %rdx, uint64_t pagetable %rcx, uint64_t entry %r8,
 *    uint64_t copy_dst %r9, uint64_t copy_src 8(%rsp),
 *    uint64_t copy_src_end 16(%rsp))
 */
amd64_tramp_inline:
	cli			/* Make sure we don't get interrupted. */

	/*
	 * Copy the kernel from the staging area to the expected location
	 * in memory. The following code is equivalent to the efi_copy_finish
	 * function that amd64_tramp used to call. Inlining this code avoids
	 * a scenario when the system froze because efi_copy_finish
	 * overwrote its own code that just happened to be located somewhere
	 * in the destination range.
	 *
	 * while (copy_src < copy_src_end) *copy_dst++ = *copy_src++;
	 */
	movq	8(%rsp), %rax	/* rax = copy_src */
	movq	16(%rsp), %r10	/* r10 = copy_src_end */
	cmpq	%r10, %rax
	jnb	copy_done
	subq	%rax, %r9	/* r9 = copy_dst - copy_src */
loop:
	movq	(%rax), %r11
	movq	%r11, (%rax,%r9)
	addq	$8, %rax
	cmpq	%rax, %r10
	ja	loop
copy_done:

	movq	%rdi,%rsp	/* Switch to our temporary stack. */

	pushq	%rsi		/* Push kernend. */
	salq	$32,%rdx	/* Shift modulep and push it. */
	pushq	%rdx
	pushq	%r8		/* Push the entry address. */
	movq	%rcx,%cr3	/* Switch page tables. */
	ret			/* "Return" to kernel entry. */

	ALIGN_TEXT
amd64_tramp_inline_end:

	.data
	.globl	amd64_tramp_inline_size
amd64_tramp_inline_size:
	.long	amd64_tramp_inline_end-amd64_tramp_inline
