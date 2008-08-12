/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)locore.s	7.3 (Berkeley) 5/13/91
 * $FreeBSD$
 *
 *		originally from: locore.s, by William F. Jolitz
 *
 *		Substantially rewritten by David Greenman, Rod Grimes,
 *			Bruce Evans, Wolfgang Solfrank, Poul-Henning Kamp
 *			and many others.
 */

#include "opt_bootp.h"
#include "opt_compat.h"
#include "opt_nfsroot.h"
#include "opt_global.h"
#include "opt_pmap.h"

#include <sys/syscall.h>
#include <sys/reboot.h>

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#define __ASSEMBLY__	
#include <xen/interface/elfnote.h>
		
/* The defines below have been lifted out of <machine/xen-public/arch-x86_32.h> */
#define FLAT_RING1_CS 0xe019    /* GDT index 259 */
#define FLAT_RING1_DS 0xe021    /* GDT index 260 */
#define KERNEL_CS FLAT_RING1_CS 
#define KERNEL_DS FLAT_RING1_DS

#include "assym.s"

.section __xen_guest
#if 0	
	.ascii "LOADER=generic,GUEST_OS=freebsd,GUEST_VER=7.0,XEN_VER=xen-3.0,BSD_SYMTAB,VIRT_BASE=0xc0000000"
	.byte 0
#endif
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_OS,       .asciz, "FreeBSD")	
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_VERSION,  .asciz, "HEAD")
	ELFNOTE(Xen, XEN_ELFNOTE_XEN_VERSION,    .asciz, "xen-3.0")
	ELFNOTE(Xen, XEN_ELFNOTE_VIRT_BASE,      .long,  KERNBASE)
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,   .long,  KERNBASE)
	ELFNOTE(Xen, XEN_ELFNOTE_ENTRY,          .long,  btext)
	ELFNOTE(Xen, XEN_ELFNOTE_HYPERCALL_PAGE, .long,  hypercall_page)
	ELFNOTE(Xen, XEN_ELFNOTE_HV_START_LOW,   .long,  HYPERVISOR_VIRT_START)
#if 0
	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,       .asciz, "writable_page_tables|writable_descriptor_tables|auto_translated_physmap|pae_pgdir_above_4gb|supervisor_mode_kernel")
#endif
	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,       .asciz, "writable_page_tables|supervisor_mode_kernel|writable_descriptor_tables")
		
#ifdef PAE
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,       .asciz, "yes")
	ELFNOTE(Xen, XEN_ELFNOTE_L1_MFN_VALID,   .long,  PG_V, PG_V)
#else
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,       .asciz, "no")
	ELFNOTE(Xen, XEN_ELFNOTE_L1_MFN_VALID,   .long,  PG_V, PG_V)
#endif
	ELFNOTE(Xen, XEN_ELFNOTE_LOADER,         .asciz, "generic")
	ELFNOTE(Xen, XEN_ELFNOTE_SUSPEND_CANCEL, .long,  1)		

	
	
/*
 *	XXX
 *
 * Note: This version greatly munged to avoid various assembler errors
 * that may be fixed in newer versions of gas. Perhaps newer versions
 * will have more pleasant appearance.
 */

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.globl	PTmap,PTD,PTDpde
	.set	PTmap,(PTDPTDI << PDRSHIFT)
	.set	PTD,PTmap + (PTDPTDI * PAGE_SIZE)
	.set	PTDpde,PTD + (PTDPTDI * PDESIZE)

/*
 * Compiled KERNBASE location and the kernel load address
 */
	.globl	kernbase
	.set	kernbase,KERNBASE
	.globl	kernload
	.set	kernload,KERNLOAD

/*
 * Globals
 */
	.data
	ALIGN_DATA			/* just to be sure */

	.space	0x2000			/* space for tmpstk - temporary stack */
tmpstk:

		.globl	bootinfo
bootinfo:	.space	BOOTINFO_SIZE	/* bootinfo that we can handle */

		.globl KERNend
KERNend:	.long	0		/* phys addr end of kernel (just after bss) */
		.globl physfree
physfree:	.long	0		/* phys addr of next free page */

#ifdef SMP
		.globl	cpu0prvpage
cpu0pp:		.long	0		/* phys addr cpu0 private pg */
cpu0prvpage:	.long	0		/* relocated version */

		.globl	SMPpt
SMPptpa:	.long	0		/* phys addr SMP page table */
SMPpt:		.long	0		/* relocated version */
#endif /* SMP */

	.globl	IdlePTD
IdlePTD:	.long	0		/* phys addr of kernel PTD */

#ifdef PAE
	.globl	IdlePDPT
IdlePDPT:	.long	0		/* phys addr of kernel PDPT */
#endif

#ifdef SMP
	.globl	KPTphys
#endif
KPTphys:	.long	0		/* phys addr of kernel page tables */

	.globl	proc0kstack
proc0uarea:	.long	0		/* address of proc 0 uarea (unused)*/
proc0kstack:	.long	0		/* address of proc 0 kstack space */
p0upa:		.long	0		/* phys addr of proc0 UAREA (unused) */
p0kpa:		.long	0		/* phys addr of proc0's STACK */

vm86phystk:	.long	0		/* PA of vm86/bios stack */

	.globl	vm86paddr, vm86pa
vm86paddr:	.long	0		/* address of vm86 region */
vm86pa:		.long	0		/* phys addr of vm86 region */

#ifdef PC98
	.globl	pc98_system_parameter
pc98_system_parameter:
	.space	0x240
#endif

	.globl	avail_space
avail_space:	.long 0

/**********************************************************************
 *
 * Some handy macros
 *
 */

/*
 * We're already in protected mode, so no remapping is needed.
 */	
#define R(foo) (foo)
	
#define ALLOCPAGES(foo) \
	movl	R(physfree), %esi ; \
	movl	$((foo)*PAGE_SIZE), %eax ; \
	addl	%esi, %eax ; \
	movl	%eax, R(physfree) ; \
	movl	%esi, %edi ; \
	movl	$((foo)*PAGE_SIZE),%ecx ; \
	xorl	%eax,%eax ; \
	cld ; \
	rep ; \
	stosb

/*
 * fillkpt
 *	eax = page frame address
 *	ebx = index into page table
 *	ecx = how many pages to map
 * 	base = base address of page dir/table
 *	prot = protection bits
 */
#define	fillkpt(base, prot)		  \
	shll	$PTESHIFT,%ebx		; \
	addl	base,%ebx		; \
	orl	$PG_V,%eax		; \
	orl	prot,%eax		; \
1:	movl	%eax,(%ebx)		; \
	addl	$PAGE_SIZE,%eax		; /* increment physical address */ \
	addl	$PTESIZE,%ebx		; /* next pte */ \
	loop	1b

/*
 * fillkptphys(prot)
 *	eax = physical address
 *	ecx = how many pages to map
 *	prot = protection bits
 */
#define	fillkptphys(prot)		  \
	movl	%eax, %ebx		; \
	shrl	$PAGE_SHIFT, %ebx	; \
	fillkpt(R(KPTphys), prot)

/* Temporary stack */
.space 	8192
tmpstack:
	.long	tmpstack, KERNEL_DS

	.text

.p2align 12,	0x90	
		
#define HYPERCALL_PAGE_OFFSET 0x1000
.org HYPERCALL_PAGE_OFFSET
ENTRY(hypercall_page)
	.cfi_startproc
	.skip	0x1000
	.cfi_endproc

/**********************************************************************
 *
 * This is where the bootblocks start us, set the ball rolling...
 *
 */
NON_GPROF_ENTRY(btext)
	/* At the end of our stack, we shall have free space - so store it */
	movl	%esp,%ebx
	movl	%ebx,R(avail_space)

	lss	tmpstack,%esp

	pushl   %esi
	call	initvalues	
	popl	%esi

	/* Store the CPUID information */
	xorl	%eax,%eax
	cpuid					# cpuid 0
	movl	%eax,R(cpu_high)		# highest capability
	movl	%ebx,R(cpu_vendor)		# store vendor string
	movl	%edx,R(cpu_vendor+4)
	movl	%ecx,R(cpu_vendor+8)
	movb	$0,R(cpu_vendor+12)

	movl	$1,%eax
	cpuid					# cpuid 1
	movl	%eax,R(cpu_id)			# store cpu_id
	movl	%ebx,R(cpu_procinfo)		# store cpu_procinfo
	movl	%edx,R(cpu_feature)		# store cpu_feature
	movl	%ecx,R(cpu_feature2)		# store cpu_feature2
	rorl	$8,%eax				# extract family type
	andl	$15,%eax
	cmpl	$5,%eax
	movl	$CPU_686,R(cpu)

	movl	proc0kstack,%eax
	leal	(KSTACK_PAGES*PAGE_SIZE-PCB_SIZE)(%eax),%esp
	xorl    %ebp,%ebp               /* mark end of frames */
#ifdef PAE
	movl    IdlePDPT,%esi
#else	
	movl    IdlePTD,%esi
#endif	
	movl    %esi,(KSTACK_PAGES*PAGE_SIZE-PCB_SIZE+PCB_CR3)(%eax)
	pushl	physfree
	call	init386
	addl	$4, %esp
	call	mi_startup
	/* NOTREACHED */
	int	$3

/*
 * Signal trampoline, copied to top of user stack
 */
NON_GPROF_ENTRY(sigcode)
	calll	*SIGF_HANDLER(%esp)
	leal	SIGF_UC(%esp),%eax	/* get ucontext */
	pushl	%eax
	testl	$PSL_VM,UC_EFLAGS(%eax)
	jne	1f
	mov	UC_GS(%eax), %gs	/* restore %gs */
1:
	movl	$SYS_sigreturn,%eax
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b

#ifdef COMPAT_FREEBSD4
	ALIGN_TEXT
freebsd4_sigcode:
	calll	*SIGF_HANDLER(%esp)
	leal	SIGF_UC4(%esp),%eax	/* get ucontext */
	pushl	%eax
	testl	$PSL_VM,UC4_EFLAGS(%eax)
	jne	1f
	mov	UC4_GS(%eax),%gs	/* restore %gs */
1:
	movl	$344,%eax		/* 4.x SYS_sigreturn */
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b
#endif

#ifdef COMPAT_43
	ALIGN_TEXT
osigcode:
	call	*SIGF_HANDLER(%esp)	/* call signal handler */
	lea	SIGF_SC(%esp),%eax	/* get sigcontext */
	pushl	%eax
	testl	$PSL_VM,SC_PS(%eax)
	jne	9f
	movl	SC_GS(%eax),%gs		/* restore %gs */
9:
	movl	$103,%eax		/* 3.x SYS_sigreturn */
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
0:	jmp	0b
#endif /* COMPAT_43 */

	ALIGN_TEXT
esigcode:

	.data
	.globl	szsigcode
szsigcode:
	.long	esigcode-sigcode
#ifdef COMPAT_FREEBSD4
	.globl	szfreebsd4_sigcode
szfreebsd4_sigcode:
	.long	esigcode-freebsd4_sigcode
#endif
#ifdef COMPAT_43
	.globl	szosigcode
szosigcode:
	.long	esigcode-osigcode
#endif
