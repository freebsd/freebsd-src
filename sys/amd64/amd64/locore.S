/*-
 * Copyright (c) 2003 Peter Wemm <peter@FreeBSD.org>
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

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.inc"

/*
 * Compiled KERNBASE location
 */
	.globl	kernbase, kernload, loc_PTmap, loc_PDmap, loc_PDPmap, dmapbase, dmapend
	.set	kernbase,KERNBASE
	.set    kernload,KERNLOAD
	.set	dmapbase,DMAP_MIN_ADDRESS
	.set	dmapend,DMAP_MAX_ADDRESS

	.text
/**********************************************************************
 *
 * This is where the loader trampoline start us, set the ball rolling...
 *
 * We are called with the stack looking like this:
 * 0(%rsp) = 32 bit return address (cannot be used)
 * 4(%rsp) = 32 bit modulep
 * 8(%rsp) = 32 bit kernend
 *
 * We are already in long mode, on a 64 bit %cs and running at KERNBASE.
 */
NON_GPROF_ENTRY(btext)

	/* Tell the bios to warmboot next time */
	movw	$0x1234,0x472

	/* Don't trust what the loader gives for rflags. */
	pushq	$PSL_KERNEL
	popfq

	/* Find the metadata pointers before we lose them */
	movq	%rsp, %rbp
	movl	4(%rbp),%edi		/* modulep (arg 1) */
	movl	8(%rbp),%esi		/* kernend (arg 2) */

	/* Get onto a stack that we can trust - there is no going back now. */
	movq	$bootstack,%rsp
	xorl	%ebp, %ebp

	call	hammer_time		/* set up cpu for unix operation */
	movq	%rax,%rsp		/* set up kstack for mi_startup() */
	call	mi_startup		/* autoconfiguration, mountroot etc */
0:	hlt
	jmp	0b

/* la57_trampoline(%rdi pml5) */
NON_GPROF_ENTRY(la57_trampoline)
	movq	%rsp,%r11
	movq	%rbx,%r10
	leaq	la57_trampoline_end(%rip),%rsp

	movq	%cr0,%rdx
	lgdtq	la57_trampoline_gdt_desc(%rip)

	pushq	$(2<<3)
	leaq	l1(%rip),%rax
	leaq	l2(%rip),%rbx

	pushq	%rax
	lretq
	.code32

l1:	movl	$(3<<3),%eax
	movl	%eax,%ss

	movl	%edx,%eax
	andl	$~CR0_PG,%eax
	movl	%eax,%cr0

	movl	%cr4,%eax
	orl	$CR4_LA57,%eax
	movl	%eax,%cr4

	movl	%edi,%cr3
	movl	%edx,%cr0

	pushl	$(1<<3)
	pushl	%ebx
	lretl
	.code64

l2:	movq	%r11,%rsp
	movq	%r10,%rbx
	retq
	.p2align 4,0
NON_GPROF_ENTRY(la57_trampoline_gdt_desc)
	.word	la57_trampoline_end - la57_trampoline_gdt
	.long	0		/* filled by pmap_bootstrap_la57 */
	.p2align 4,0
NON_GPROF_ENTRY(la57_trampoline_gdt)
	.long	0x00000000	/* null desc */
	.long	0x00000000
	.long	0x00000000	/* 64bit code */
	.long	0x00209800
	.long	0x0000ffff	/* 32bit code */
	.long	0x00cf9b00
	.long	0x0000ffff	/* universal data */
	.long	0x00cf9300
	.dcb.l	16,0
NON_GPROF_ENTRY(la57_trampoline_end)

	.bss
	ALIGN_DATA			/* just to be sure */
	.globl	bootstack
	.space	0x1000			/* space for bootstack - temporary stack */
bootstack:
