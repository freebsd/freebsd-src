/*-
 * Copyright (c) 2003 Peter Wemm <peter@FreeBSD.org>
 * All rights reserved.
 *
 * Copyright (c) 2020, 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.inc"

/*
 * Compiled KERNBASE location
 */
	.globl	kernbase, kernload, dmapbase, dmapend
	.set	kernbase,KERNBASE
	.set    kernload,KERNLOAD
	.set	dmapbase,DMAP_MIN_ADDRESS
	.set	dmapend,DMAP_MAX_ADDRESS

#define	BOOTSTACK_SIZE	(PAGE_SIZE * KSTACK_PAGES)

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
ENTRY(btext)

	/* Don't trust what the loader gives for rflags. */
	pushq	$PSL_KERNEL
	popfq

	/* Get onto a stack that we can trust - there is no going back now. */
	movq	%rsp, %rbp
	movq	$bootstack,%rsp

#ifdef KASAN
	/* Bootstrap a shadow map for the boot stack. */
	movq	$bootstack, %rdi
	subq	$BOOTSTACK_SIZE, %rdi
	movq	$BOOTSTACK_SIZE, %rsi
	call	kasan_init_early
#endif

	/* Grab metadata pointers from the loader. */
	movl	4(%rbp),%edi		/* modulep (arg 1) */
	movl	8(%rbp),%esi		/* kernend (arg 2) */
	xorq	%rbp, %rbp

	call	hammer_time		/* set up cpu for unix operation */
	movq	%rax,%rsp		/* set up kstack for mi_startup() */
	call	mi_startup		/* autoconfiguration, mountroot etc */
0:	hlt
	jmp	0b

/*
 * void la57_trampoline(%rdi pml5)
 *
 * Entered in 4-level paging long mode on AP, hopefully returns alive in
 * 5-level paging mode. The parameter is a pointer to a 5-level page
 * table root. The passed 5-level page table, and the current 4-level page
 * table, both must map the trampoline code page 1:1 physical, below 4G.
 * The trampoline must be PIC because it is copied from kernel text into
 * this page.
 *
 * The current paging level cannot be changed while paging is enabled, and
 * paging cannot be disabled while in long mode.  As consequence, code
 * switches into the compat mode, then disables paging to descend into
 * protected mode.  There, the paging level bit CR4.LA57 can be changed,
 * and code directly jumps back into long mode.
 *
 * Falling into the protected mode requires single-purpose GDT entries,
 * which are provided by the private GDT.  It is the caller's responsibility
 * to
 * - restore the GDT and %gsbase after the call
 * - reset IDT back to long mode.
 */
ENTRY(la57_trampoline)
	movq	%rsp,lst(%rip)		/* save registers into memeory */
	movq	%rbx,lst+8(%rip)	/* upper halves are not saved .. */
	movq	%rbp,lst+0x10(%rip)	/* by 64->32->64 switch */
	movq	%cr4,%rax
	orq	$CR4_LA57,%rax		/* 5-lvl %cr4 */
	movq	%rax,lst+0x18(%rip)
	leaq	la57_trampoline_end(%rip),%rsp /* priv stack */

	movq	%cr0,%rbp
	lgdtq	la57_trampoline_gdt_desc(%rip)

	pushq	$(2<<3)
	leaq	l1(%rip),%rax
	leaq	l2(%rip),%rbx

	pushq	%rax
	lretq
	.code32

l1:	movl	$(3<<3),%eax
	movl	%eax,%ss		/* 32bit paged, priv gdt and stack */

	movl	%cr4,%eax
	andl	$~(CR4_PGE | CR4_PCIDE),%eax /* clear sensitive paging ctrls */
	movl	%eax,%cr4

	movl	%ebp,%eax
	andl	$~CR0_PG,%eax		/* protected mode */
	movl	%eax,%cr0

	movl	$MSR_EFER,%ecx		/* disable long mode bit */
	rdmsr				/* to safer tweaking LA57 */
	andl	$~EFER_LME,%eax
	wrmsr

	movl	%cr4,%eax		/* finally safe to switch bit */
	orl	$CR4_LA57,%eax
	movl	%eax,%cr4

	movl	%edi,%cr3		/* and load the 5-level pgtable root */

	rdmsr
	orl	$EFER_LME,%eax
	wrmsr				/* prepare for ... */

	movl	%ebp,%cr0		/* and jump back directly into long */
	jmp	1f			/* mode from protected by enabling pg */

1:	pushl	$(1<<3)			/* reload %cs */
	pushl	%ebx
	lretl
	.code64

l2:	movq	lst(%rip),%rsp		/* back on C stack */
	movq	lst+8(%rip),%rbx
	movq	lst+0x10(%rip),%rbp
	movq	lst+0x18(%rip),%rax
	movq	%rax,%cr4		/* re-enable paging controls */
	retq				/* back to C */
	.p2align 4,0
lst:	.quad	0,0,0,0
ENTRY(la57_trampoline_gdt_desc)
	.word	la57_trampoline_end - la57_trampoline_gdt
	.long	0, 0		/* filled by pmap_bootstrap_la57 */
	.p2align 4,0
ENTRY(la57_trampoline_gdt)
	.long	0x00000000	/* null desc */
	.long	0x00000000
	.long	0x00000000	/* 64bit code */
	.long	0x00209800
	.long	0x0000ffff	/* 32bit code */
	.long	0x00cf9b00
	.long	0x0000ffff	/* universal data */
	.long	0x00cf9300
	.dcb.l	16,0
ENTRY(la57_trampoline_end)

	.bss
	.p2align PAGE_SHIFT
	.globl	bootstack
	.space	BOOTSTACK_SIZE		/* space for bootstack - temporary stack */
bootstack:
