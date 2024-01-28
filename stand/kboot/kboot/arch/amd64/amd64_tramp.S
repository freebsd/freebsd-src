/*-
 * Copyright (c) 2022 Netflix, Inc
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
 */

/*
 * This is the trampoline that starts the FreeBSD kernel. Since the Linux kernel
 * calls this routine with no args, and has a different environment than the
 * boot loader provides and that the kernel expects, this code is responsible
 * for setting all that up and calling the normal kernel entry point. It's
 * analogous to the "purgatory" code in the linux kernel. Details about these
 * operations are contained in comments below. On amd64, the kernel starts all
 * the APs so we don't have to worry about them here.
 */

/*
 * Keep in sync with elf64_freebsd.c. Kexec starts tramp w/o any parameters, so
 * store them here. This is constructed to be a useful stack:
 *
 * struct trampoline_data {
 *	uint64_t	pt4;			// Page table address to pop
 *	uint64_t	entry;			// return address to jump to kernel
 *	uint32_t	fill1;			// 0
 *	uint32_t	modulep;		// 4 module metadata
 *	uint32_t	kernend;		// 8 kernel end
 *	uint32_t	fill2;			// 12
 * };
 *
 * loader.kboot will construct a stack that btext expects, which is arguments on
 * the stack, not in registers, and these args are 32-bit not 64
 *
 * Processor is already in long mode when we're called, paging is enabled and
 * boot loader loads things such that:
 * - kernel mapped at KERNBASE, aligned to 2MB, below 4GB, contiguous memory
 * - there is a 2M hole at KERNBASE (KERNSTART = KERNBASE + 2M)
 * - kernel is mapped with 2M superpages
 * - The kernel, modules and metadata is in first 4GB which is unity mapped
 * - There's additional memory after loader provided data for early allocations
 *
 * Unlike EFI, we don't support copying the staging area. We tell Linux to land
 * the kernel in its final location with the needed alignment, etc. We copy the
 * trampoline code to 1MB offset above KERNBASE since that memory is otherwise
 * free and safely above the lower 1MB swamp we inherited from IBM PC, though
 * this code makes no assumptions about where that might.
 *
 * Thus, the trampoline just needs to set %rsp to that stack pop the %cr3 value,
 * set it and then retq to jump to the kernel with its stack args filled in.
 * Since the handoff to this code used to be from 32-bit code, it uses the i386
 * calling conventions which put the arguments on the stack. The kernel's btext
 * routine expects this setup.
 */
	.text
	.globl	tramp
tramp:
	cli				/* Make sure we don't get interrupted. */
	leaq	tramp_pt4(%rip), %rsp	/* Setup our pre-filled-in stack */
	popq	%rax			/* Pop off the PT4 ptr for %cr3 */
	movq	%rax, %cr3		/* set the page table */
	retq				/* Return addr and args already on stack */
/*
 * The following is the stack for the above code. The stack will increase in
 * address as things are popped off of it, so we start with the stack pointing
 * to tramp_pt4.
 */
	.p2align	3		/* Stack has to be 8 byte aligned */
trampoline_data:
tramp_pt4:	.quad	0		/* New %cr3 value */
tramp_entry:	.quad	0		/* Entry to kernel (btext) */
	/* %rsp points here on entry to amd64 kernel's btext */
		.long	0		/* 0 filler, ignored (current loaders set to 0) */
tramp_modulep:	.long	0		/* 4 moudlep */
tramp_kernend:	.long	0		/* 8 kernend */
		.long	0		/* 12 alignment filler (also 0) */
tramp_end:

	.data
	.type   tramp_size,@object
	.globl	tramp_size
tramp_size:
	.long	tramp_end-tramp
	.size	tramp_size, 4

	.type   tramp_data_offset,@object
	.globl	tramp_data_offset
tramp_data_offset:
	.long	trampoline_data-tramp
	.size	tramp_data_offset, 4
