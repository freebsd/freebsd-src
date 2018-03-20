/*-
 * Copyright (c) 2003 Peter Wemm <peter@FreeBSD.org>
 * Copyright (c) 2011-2012 Spectra Logic Corporation
 * Copyright (c) 2013 Roger Pau Monne <royger@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by Cherry G. Mathew <cherry@zyx.in>
 * under sponsorship from Spectra Logic Corporation.
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

#include <xen/xen-os.h>
#define __ASSEMBLY__
#include <xen/interface/elfnote.h>

#include "assym.inc"

.section __xen_guest
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_OS,       .asciz, "FreeBSD")
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_VERSION,  .asciz, __XSTRING(__FreeBSD_version))
	ELFNOTE(Xen, XEN_ELFNOTE_XEN_VERSION,    .asciz, "xen-3.0")
	ELFNOTE(Xen, XEN_ELFNOTE_VIRT_BASE,      .quad,  KERNBASE)
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,   .quad,  0)
	ELFNOTE(Xen, XEN_ELFNOTE_ENTRY,          .quad,  xen_start)
	ELFNOTE(Xen, XEN_ELFNOTE_HYPERCALL_PAGE, .quad,	 hypercall_page)
	ELFNOTE(Xen, XEN_ELFNOTE_HV_START_LOW,   .quad,  HYPERVISOR_VIRT_START)
	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,       .asciz, "writable_descriptor_tables|auto_translated_physmap|supervisor_mode_kernel|hvm_callback_vector")
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,       .asciz, "yes")
	ELFNOTE(Xen, XEN_ELFNOTE_L1_MFN_VALID,   .long,  PG_V, PG_V)
	ELFNOTE(Xen, XEN_ELFNOTE_LOADER,         .asciz, "generic")
	ELFNOTE(Xen, XEN_ELFNOTE_SUSPEND_CANCEL, .long,  0)
	ELFNOTE(Xen, XEN_ELFNOTE_BSD_SYMTAB,	 .asciz, "yes")

	.text
.p2align PAGE_SHIFT, 0x90	/* Hypercall_page needs to be PAGE aligned */

NON_GPROF_ENTRY(hypercall_page)
	.skip	0x1000, 0x90	/* Fill with "nop"s */

NON_GPROF_ENTRY(xen_start)
	/* Don't trust what the loader gives for rflags. */
	pushq	$PSL_KERNEL
	popfq

	/* Parameters for the xen init function */
	movq	%rsi, %rdi		/* shared_info (arg 1) */
	movq	%rsp, %rsi		/* xenstack    (arg 2) */

	/* Use our own stack */
	movq	$bootstack,%rsp
	xorl	%ebp, %ebp

	/* u_int64_t hammer_time_xen(start_info_t *si, u_int64_t xenstack); */
	call	hammer_time_xen
	movq	%rax, %rsp		/* set up kstack for mi_startup() */
	call	mi_startup		/* autoconfiguration, mountroot etc */

	/* NOTREACHED */
0:	hlt
	jmp 	0b
