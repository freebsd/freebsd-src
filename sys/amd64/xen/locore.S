/* $FreeBSD$ */
/*-
 * Copyright (c) 2011-2012 Spectra Logic Corporation
 * All rights reserved.
 *
 * This software was developed by Cherry G. Mathew <cherry@zyx.in>
 * under sponsorship from Spectra Logic Corporation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/pmap.h>
	
#define __ASSEMBLY__
#include <xen/interface/elfnote.h>

#include "assym.s"

#ifndef __XSTRING
#error XXX: TODO: need a portable way to "stringify" constants
#endif

.section __xen_guest
	.ascii "LOADER=generic,GUEST_OS=freebsd,GUEST_VER=9.0,XEN_VER=xen-3.0,BSD_SYMTAB,VIRT_BASE="
	.ascii __XSTRING(KERNBASE)
	.byte 0

	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_OS,       .asciz, "FreeBSD")
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_VERSION,  .asciz, "HEAD")
	ELFNOTE(Xen, XEN_ELFNOTE_XEN_VERSION,    .asciz, "xen-3.0")
	ELFNOTE(Xen, XEN_ELFNOTE_VIRT_BASE,      .quad,  KERNBASE)
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,   .quad,  KERNBASE) /* Xen honours elf->p_paddr; compensate for this */
	ELFNOTE(Xen, XEN_ELFNOTE_ENTRY,          .quad,  btext)
	ELFNOTE(Xen, XEN_ELFNOTE_HYPERCALL_PAGE, .quad,	 hypercall_page)
	ELFNOTE(Xen, XEN_ELFNOTE_HV_START_LOW,   .quad,  HYPERVISOR_VIRT_START)
 	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,       .asciz, "writable_page_tables|writable_descriptor_tables")
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,       .asciz, "no")
	ELFNOTE(Xen, XEN_ELFNOTE_L1_MFN_VALID,   .long,  PG_V, PG_V)
	ELFNOTE(Xen, XEN_ELFNOTE_LOADER,         .asciz, "generic")
	ELFNOTE(Xen, XEN_ELFNOTE_SUSPEND_CANCEL, .long,  0)
	ELFNOTE(Xen, XEN_ELFNOTE_BSD_SYMTAB,	 .asciz, "yes")
	/* XXX: TODO: dump-core notes */

/* For conf/ldscript.amd64 */

	.globl	kernbase
	.set	kernbase, KERNBASE

	.data
xenstack:
	.quad	0x00
	.globl	xenstack

	.text
.p2align PAGE_SHIFT, 0x90	/* Hypercall_page needs to be PAGE aligned */

NON_GPROF_ENTRY(hypercall_page)
	.skip	0x1000, 0x90	/* Fill with "nop"s */
	
/*
 *	XXX: put everything below this at the end, and arrange to blow
 *	it all away when it's safe.
 */
NON_GPROF_ENTRY(btext)
	/* For known state */
	pushq	$PSL_KERNEL
	popfq

	/* Use our own stack */
	/* XXX: %ss */
	movq	%rsp, xenstack	/* save a reference to the one xen gives us */
	movq	$bootstack, %rsp
	movq	$bootstack, %rbp

	movq	%rsi, %rdi
	call	initxen		/* vm_paddr_t init_xen(struct start_info *); */

	movq	%rax, %rsp
	call	mi_startup
	
	/* NOTREACHED */
0:	int	$3
	hlt
	jmp 	0b
	
	.bss	
	/* Stack bottom*/
	.p2align PAGE_SHIFT, 0x0	/* Align stack on page boundary */
	.space	PAGE_SIZE	/* Give it a page */
bootstack:			/* Stack top */
