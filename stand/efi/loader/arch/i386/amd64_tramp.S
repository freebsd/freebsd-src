/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * Copyright (c) 2023 Ahmad Khalifa <ahmadkhalifa570@gmail.com>
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 */

#include <machine/asmacros.h>

	.text
	.globl	amd64_tramp

/*
 * void amd64_tramp(uint32_t stack, void *copy_finish, uint32_t kernend,
 * 		    uint32_t modulep, uint32_t pagetable, uint32_t gdtr, uint64_t entry)
 */
amd64_tramp:
	cli	/* Make sure we don't get interrupted. */

	calll *8(%esp)	/* Call copy_finish so we're all ready to go. */

	movl %cr0, %eax	/* Paging may be enabled, disable it. */
	andl $0x7FFFFFFF, %eax
	movl %eax, %cr0

	movl %cr4, %eax	/* PAE may be disabled, enable it. */
	orl $0x20, %eax
	movl %eax, %cr4

	movl 20(%esp), %eax	/* Swap page tables. */
	movl %eax, %cr3

	movl $0xC0000080, %ecx	/* Enable long mode. */
	rdmsr
	orl $0x100, %eax
	wrmsr

	movl 12(%esp), %edi	/* Stash the kernel and GDT values for later. */
	movl 16(%esp), %esi
	movl 24(%esp), %ebx
	movl 28(%esp), %edx
	movl 32(%esp), %ebp

	movl 4(%esp), %esp	/* Switch to our temporary stack. */

	movl %cr0, %eax	/* Enable paging and enter compatibility mode. */
	orl $0x80000000, %eax
	movl %eax, %cr0

	lgdtl (%ebx)	/* Load GDT. */

	pushl %edi	/* Push kernend. */
	pushl %esi	/* Push modulep. */
	pushl $0x0
	pushl %ebp	/* Push 64-bit entry address. */
	pushl %edx

	calll 0f	/* Find the address of ".longmode". */
0:	popl %eax
	addl $(.longmode-0b), %eax

	pushl $0x8	/* Push CS. */
	pushl %eax	/* Push the address. */
	lretl	/* "Return" to 64-bit code. */

	.code64

.longmode:
	retq	/* "Return" to kernel entry. */

	.code32

	ALIGN_TEXT
amd64_tramp_end:

	.data
	.globl	amd64_tramp_size
amd64_tramp_size:
	.long	amd64_tramp_end-amd64_tramp
