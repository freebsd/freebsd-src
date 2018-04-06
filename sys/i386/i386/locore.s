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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "opt_nfsroot.h"
#include "opt_pmap.h"

#include <sys/reboot.h>

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.inc"

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
physfree:	.long	0		/* phys addr of next free page */

	.globl	IdlePTD
IdlePTD:	.long	0		/* phys addr of kernel PTD */

#if defined(PAE) || defined(PAE_TABLES)
	.globl	IdlePDPT
IdlePDPT:	.long	0		/* phys addr of kernel PDPT */
#endif

	.globl	KPTmap
KPTmap:		.long	0		/* address of kernel page tables */

	.globl	KPTphys
KPTphys:	.long	0		/* phys addr of kernel page tables */

	.globl	proc0kstack
proc0kstack:	.long	0		/* address of proc 0 kstack space */
p0kpa:		.long	0		/* phys addr of proc0's STACK */

vm86phystk:	.long	0		/* PA of vm86/bios stack */

	.globl	vm86paddr, vm86pa
vm86paddr:	.long	0		/* address of vm86 region */
vm86pa:		.long	0		/* phys addr of vm86 region */

/**********************************************************************
 *
 * Some handy macros
 *
 */

#define R(foo) ((foo)-KERNBASE)

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

	.text
/**********************************************************************
 *
 * This is where the bootblocks start us, set the ball rolling...
 *
 */
NON_GPROF_ENTRY(btext)

/* Tell the bios to warmboot next time */
	movw	$0x1234,0x472

/* Set up a real frame in case the double return in newboot is executed. */
	pushl	%ebp
	movl	%esp, %ebp

/* Don't trust what the BIOS gives for eflags. */
	pushl	$PSL_KERNEL
	popfl

/*
 * Don't trust what the BIOS gives for %fs and %gs.  Trust the bootstrap
 * to set %cs, %ds, %es and %ss.
 */
	mov	%ds, %ax
	mov	%ax, %fs
	mov	%ax, %gs

/*
 * Clear the bss.  Not all boot programs do it, and it is our job anyway.
 *
 * XXX we don't check that there is memory for our bss and page tables
 * before using it.
 *
 * Note: we must be careful to not overwrite an active gdt or idt.  They
 * inactive from now until we switch to new ones, since we don't load any
 * more segment registers or permit interrupts until after the switch.
 */
	movl	$R(end),%ecx
	movl	$R(edata),%edi
	subl	%edi,%ecx
	xorl	%eax,%eax
	cld
	rep
	stosb

	call	recover_bootinfo

/* Get onto a stack that we can trust. */
/*
 * XXX this step is delayed in case recover_bootinfo needs to return via
 * the old stack, but it need not be, since recover_bootinfo actually
 * returns via the old frame.
 */
	movl	$R(tmpstk),%esp

	call	identify_cpu
	call	create_pagetables

/*
 * If the CPU has support for VME, turn it on.
 */ 
	testl	$CPUID_VME, R(cpu_feature)
	jz	1f
	movl	%cr4, %eax
	orl	$CR4_VME, %eax
	movl	%eax, %cr4
1:

/* Now enable paging */
#if defined(PAE) || defined(PAE_TABLES)
	movl	R(IdlePDPT), %eax
	movl	%eax, %cr3
	movl	%cr4, %edx
	orl	$CR4_PAE, %edx
	movl	%edx, %cr4
#else
	movl	R(IdlePTD), %eax
	movl	%eax,%cr3		/* load ptd addr into mmu */
#endif
	movl	%cr0,%edx		/* get control word */
	orl	$CR0_PE|CR0_PG,%edx	/* enable paging */
	movl	%edx,%cr0		/* and let's page NOW! */

	pushl	$begin			/* jump to high virtualized address */
	ret

begin:
	/*
	 * Now running relocated at KERNBASE where the system is linked to run.
	 *
	 * Remove the lowest part of the double mapping of low memory to get
	 * some null pointer checks.
	 */
	movl	$0,PTD
	movl	%eax,%cr3		/* invalidate TLB */

	/* set up bootstrap stack */
	movl	proc0kstack,%eax	/* location of in-kernel stack */

	/*
	 * Only use bottom page for init386().  init386() calculates the
	 * PCB + FPU save area size and returns the true top of stack.
	 */
	leal	PAGE_SIZE(%eax),%esp

	xorl	%ebp,%ebp		/* mark end of frames */

	pushl	physfree		/* value of first for init386(first) */
	call	init386			/* wire 386 chip for unix operation */

	/*
	 * Clean up the stack in a way that db_numargs() understands, so
	 * that backtraces in ddb don't underrun the stack.  Traps for
	 * inaccessible memory are more fatal than usual this early.
	 */
	addl	$4,%esp

	/* Switch to true top of stack. */
	movl	%eax,%esp

	call	mi_startup		/* autoconfiguration, mountroot etc */
	/* NOTREACHED */
	addl	$0,%esp			/* for db_numargs() again */

/**********************************************************************
 *
 * Recover the bootinfo passed to us from the boot program
 *
 */
recover_bootinfo:
	/*
	 * This code is called in different ways depending on what loaded
	 * and started the kernel.  This is used to detect how we get the
	 * arguments from the other code and what we do with them.
	 *
	 * Old disk boot blocks:
	 *	(*btext)(howto, bootdev, cyloffset, esym);
	 *	[return address == 0, and can NOT be returned to]
	 *	[cyloffset was not supported by the FreeBSD boot code
	 *	 and always passed in as 0]
	 *	[esym is also known as total in the boot code, and
	 *	 was never properly supported by the FreeBSD boot code]
	 *
	 * Old diskless netboot code:
	 *	(*btext)(0,0,0,0,&nfsdiskless,0,0,0);
	 *	[return address != 0, and can NOT be returned to]
	 *	If we are being booted by this code it will NOT work,
	 *	so we are just going to halt if we find this case.
	 *
	 * New uniform boot code:
	 *	(*btext)(howto, bootdev, 0, 0, 0, &bootinfo)
	 *	[return address != 0, and can be returned to]
	 *
	 * There may seem to be a lot of wasted arguments in here, but
	 * that is so the newer boot code can still load very old kernels
	 * and old boot code can load new kernels.
	 */

	/*
	 * The old style disk boot blocks fake a frame on the stack and
	 * did an lret to get here.  The frame on the stack has a return
	 * address of 0.
	 */
	cmpl	$0,4(%ebp)
	je	olddiskboot

	/*
	 * We have some form of return address, so this is either the
	 * old diskless netboot code, or the new uniform code.  That can
	 * be detected by looking at the 5th argument, if it is 0
	 * we are being booted by the new uniform boot code.
	 */
	cmpl	$0,24(%ebp)
	je	newboot

	/*
	 * Seems we have been loaded by the old diskless boot code, we
	 * don't stand a chance of running as the diskless structure
	 * changed considerably between the two, so just halt.
	 */
	 hlt

	/*
	 * We have been loaded by the new uniform boot code.
	 * Let's check the bootinfo version, and if we do not understand
	 * it we return to the loader with a status of 1 to indicate this error
	 */
newboot:
	movl	28(%ebp),%ebx		/* &bootinfo.version */
	movl	BI_VERSION(%ebx),%eax
	cmpl	$1,%eax			/* We only understand version 1 */
	je	1f
	movl	$1,%eax			/* Return status */
	leave
	/*
	 * XXX this returns to our caller's caller (as is required) since
	 * we didn't set up a frame and our caller did.
	 */
	ret

1:
	/*
	 * If we have a kernelname copy it in
	 */
	movl	BI_KERNELNAME(%ebx),%esi
	cmpl	$0,%esi
	je	2f			/* No kernelname */
	movl	$MAXPATHLEN,%ecx	/* Brute force!!! */
	movl	$R(kernelname),%edi
	cmpb	$'/',(%esi)		/* Make sure it starts with a slash */
	je	1f
	movb	$'/',(%edi)
	incl	%edi
	decl	%ecx
1:
	cld
	rep
	movsb

2:
	/*
	 * Determine the size of the boot loader's copy of the bootinfo
	 * struct.  This is impossible to do properly because old versions
	 * of the struct don't contain a size field and there are 2 old
	 * versions with the same version number.
	 */
	movl	$BI_ENDCOMMON,%ecx	/* prepare for sizeless version */
	testl	$RB_BOOTINFO,8(%ebp)	/* bi_size (and bootinfo) valid? */
	je	got_bi_size		/* no, sizeless version */
	movl	BI_SIZE(%ebx),%ecx
got_bi_size:

	/*
	 * Copy the common part of the bootinfo struct
	 */
	movl	%ebx,%esi
	movl	$R(bootinfo),%edi
	cmpl	$BOOTINFO_SIZE,%ecx
	jbe	got_common_bi_size
	movl	$BOOTINFO_SIZE,%ecx
got_common_bi_size:
	cld
	rep
	movsb

#ifdef NFS_ROOT
#ifndef BOOTP_NFSV3
	/*
	 * If we have a nfs_diskless structure copy it in
	 */
	movl	BI_NFS_DISKLESS(%ebx),%esi
	cmpl	$0,%esi
	je	olddiskboot
	movl	$R(nfs_diskless),%edi
	movl	$NFSDISKLESS_SIZE,%ecx
	cld
	rep
	movsb
	movl	$R(nfs_diskless_valid),%edi
	movl	$1,(%edi)
#endif
#endif

	/*
	 * The old style disk boot.
	 *	(*btext)(howto, bootdev, cyloffset, esym);
	 * Note that the newer boot code just falls into here to pick
	 * up howto and bootdev, cyloffset and esym are no longer used
	 */
olddiskboot:
	movl	8(%ebp),%eax
	movl	%eax,R(boothowto)
	movl	12(%ebp),%eax
	movl	%eax,R(bootdev)

	ret


/**********************************************************************
 *
 * Identify the CPU and initialize anything special about it
 *
 */
identify_cpu:

	/* Try to toggle alignment check flag; does not exist on 386. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	orl	$PSL_AC,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_AC,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	try486

	/* NexGen CPU does not have aligment check flag. */
	pushfl
	movl	$0x5555, %eax
	xorl	%edx, %edx
	movl	$2, %ecx
	clc
	divl	%ecx
	jz	trynexgen
	popfl
	movl	$CPU_386,R(cpu)
	jmp	3f

trynexgen:
	popfl
	movl	$CPU_NX586,R(cpu)
	movl	$0x4778654e,R(cpu_vendor)	# store vendor string
	movl	$0x72446e65,R(cpu_vendor+4)
	movl	$0x6e657669,R(cpu_vendor+8)
	movl	$0,R(cpu_vendor+12)
	jmp	3f

try486:	/* Try to toggle identification flag; does not exist on early 486s. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	xorl	$PSL_ID,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_ID,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	trycpuid
	movl	$CPU_486,R(cpu)

	/*
	 * Check Cyrix CPU
	 * Cyrix CPUs do not change the undefined flags following
	 * execution of the divide instruction which divides 5 by 2.
	 *
	 * Note: CPUID is enabled on M2, so it passes another way.
	 */
	pushfl
	movl	$0x5555, %eax
	xorl	%edx, %edx
	movl	$2, %ecx
	clc
	divl	%ecx
	jnc	trycyrix
	popfl
	jmp	3f		/* You may use Intel CPU. */

trycyrix:
	popfl
	/*
	 * IBM Bluelighting CPU also doesn't change the undefined flags.
	 * Because IBM doesn't disclose the information for Bluelighting
	 * CPU, we couldn't distinguish it from Cyrix's (including IBM
	 * brand of Cyrix CPUs).
	 */
	movl	$0x69727943,R(cpu_vendor)	# store vendor string
	movl	$0x736e4978,R(cpu_vendor+4)
	movl	$0x64616574,R(cpu_vendor+8)
	jmp	3f

trycpuid:	/* Use the `cpuid' instruction. */
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
	jae	1f

	/* less than Pentium; must be 486 */
	movl	$CPU_486,R(cpu)
	jmp	3f
1:
	/* a Pentium? */
	cmpl	$5,%eax
	jne	2f
	movl	$CPU_586,R(cpu)
	jmp	3f
2:
	/* Greater than Pentium...call it a Pentium Pro */
	movl	$CPU_686,R(cpu)
3:
	ret


/**********************************************************************
 *
 * Create the first page directory and its page tables.
 *
 */

create_pagetables:

/* Find end of kernel image (rounded up to a page boundary). */
	movl	$R(_end),%esi

/* Include symbols, if any. */
	movl	R(bootinfo+BI_ESYMTAB),%edi
	testl	%edi,%edi
	je	over_symalloc
	movl	%edi,%esi
	movl	$KERNBASE,%edi
	addl	%edi,R(bootinfo+BI_SYMTAB)
	addl	%edi,R(bootinfo+BI_ESYMTAB)
over_symalloc:

/* If we are told where the end of the kernel space is, believe it. */
	movl	R(bootinfo+BI_KERNEND),%edi
	testl	%edi,%edi
	je	no_kernend
	movl	%edi,%esi
no_kernend:

	addl	$PDRMASK,%esi		/* Play conservative for now, and */
	andl	$~PDRMASK,%esi		/* ... round up to PDR boundary */
	movl	%esi,R(KERNend)		/* save end of kernel */
	movl	%esi,R(physfree)	/* next free page is at end of kernel */

/* Allocate Kernel Page Tables */
	ALLOCPAGES(NKPT)
	movl	%esi,R(KPTphys)
	addl	$(KERNBASE-(KPTDI<<(PDRSHIFT-PAGE_SHIFT+PTESHIFT))),%esi
	movl	%esi,R(KPTmap)

/* Allocate Page Table Directory */
#if defined(PAE) || defined(PAE_TABLES)
	/* XXX only need 32 bytes (easier for now) */
	ALLOCPAGES(1)
	movl	%esi,R(IdlePDPT)
#endif
	ALLOCPAGES(NPGPTD)
	movl	%esi,R(IdlePTD)

/* Allocate KSTACK */
	ALLOCPAGES(TD0_KSTACK_PAGES)
	movl	%esi,R(p0kpa)
	addl	$KERNBASE, %esi
	movl	%esi, R(proc0kstack)

	ALLOCPAGES(1)			/* vm86/bios stack */
	movl	%esi,R(vm86phystk)

	ALLOCPAGES(3)			/* pgtable + ext + IOPAGES */
	movl	%esi,R(vm86pa)
	addl	$KERNBASE, %esi
	movl	%esi, R(vm86paddr)

/*
 * Enable PSE and PGE.
 */
#ifndef DISABLE_PSE
	testl	$CPUID_PSE, R(cpu_feature)
	jz	1f
	movl	$PG_PS, R(pseflag)
	movl	%cr4, %eax
	orl	$CR4_PSE, %eax
	movl	%eax, %cr4
1:
#endif
#ifndef DISABLE_PG_G
	testl	$CPUID_PGE, R(cpu_feature)
	jz	2f
	movl	$PG_G, R(pgeflag)
	movl	%cr4, %eax
	orl	$CR4_PGE, %eax
	movl	%eax, %cr4
2:
#endif

/*
 * Initialize page table pages mapping physical address zero through the
 * (physical) end of the kernel.  Many of these pages must be reserved,
 * and we reserve them all and map them linearly for convenience.  We do
 * this even if we've enabled PSE above; we'll just switch the corresponding
 * kernel PDEs before we turn on paging.
 *
 * XXX: We waste some pages here in the PSE case!
 *
 * This and all other page table entries allow read and write access for
 * various reasons.  Kernel mappings never have any access restrictions.
 */
	xorl	%eax, %eax
	movl	R(KERNend),%ecx
	shrl	$PAGE_SHIFT,%ecx
	fillkptphys($PG_RW)

/* Map page table pages. */
	movl	R(KPTphys),%eax
	movl	$NKPT,%ecx
	fillkptphys($PG_RW)

/* Map page directory. */
#if defined(PAE) || defined(PAE_TABLES)
	movl	R(IdlePDPT), %eax
	movl	$1, %ecx
	fillkptphys($PG_RW)
#endif

	movl	R(IdlePTD), %eax
	movl	$NPGPTD, %ecx
	fillkptphys($PG_RW)

/* Map proc0's KSTACK in the physical way ... */
	movl	R(p0kpa), %eax
	movl	$(TD0_KSTACK_PAGES), %ecx
	fillkptphys($PG_RW)

/* Map ISA hole */
	movl	$ISA_HOLE_START, %eax
	movl	$ISA_HOLE_LENGTH>>PAGE_SHIFT, %ecx
	fillkptphys($PG_RW)

/* Map space for the vm86 region */
	movl	R(vm86phystk), %eax
	movl	$4, %ecx
	fillkptphys($PG_RW)

/* Map page 0 into the vm86 page table */
	movl	$0, %eax
	movl	$0, %ebx
	movl	$1, %ecx
	fillkpt(R(vm86pa), $PG_RW|PG_U)

/* ...likewise for the ISA hole */
	movl	$ISA_HOLE_START, %eax
	movl	$ISA_HOLE_START>>PAGE_SHIFT, %ebx
	movl	$ISA_HOLE_LENGTH>>PAGE_SHIFT, %ecx
	fillkpt(R(vm86pa), $PG_RW|PG_U)

/*
 * Create an identity mapping for low physical memory, including the kernel.
 * This is only used to map the 2 instructions for jumping to 'begin' in
 * locore (we map everything to avoid having to determine where these
 * instructions are).  ACPI resume will transiently restore the first PDE in
 * this mapping (and depend on this PDE's page table created here not being
 * destroyed).  See pmap_bootstrap() for more details.
 *
 * Note:  There are errata concerning large pages and physical address zero,
 * so a PG_PS mapping should not be used for PDE 0.  Our double mapping
 * avoids this automatically by not using PG_PS for PDE #KPDI so that PAT
 * bits can be set at the page level for i/o pages below 1 MB.
 */
	movl	R(KPTphys), %eax
	xorl	%ebx, %ebx
	movl	$NKPT, %ecx
	fillkpt(R(IdlePTD), $PG_RW)

/*
 * Install PDEs for PTs covering enough kva to bootstrap.  Then for the PSE
 * case, replace the PDEs whose coverage is strictly within the kernel
 * (between KERNLOAD (rounded up) and KERNend) by large-page PDEs.
 */
	movl	R(KPTphys), %eax
	movl	$KPTDI, %ebx
	movl	$NKPT, %ecx
	fillkpt(R(IdlePTD), $PG_RW)
	cmpl	$0,R(pseflag)
	je	done_pde

	movl	R(KERNend), %ecx
	movl	$(KERNLOAD + PDRMASK) & ~PDRMASK, %eax
	subl	%eax, %ecx
	shrl	$PDRSHIFT, %ecx
	movl	$KPTDI + ((KERNLOAD + PDRMASK) >> PDRSHIFT), %ebx
	shll	$PDESHIFT, %ebx
	addl	R(IdlePTD), %ebx
	orl	$(PG_V|PG_RW|PG_PS), %eax
1:	movl	%eax, (%ebx)
	addl	$(1 << PDRSHIFT), %eax
	addl	$PDESIZE, %ebx
	loop	1b

done_pde:
/* install a pde recursively mapping page directory as a page table */
	movl	R(IdlePTD), %eax
	movl	$PTDPTDI, %ebx
	movl	$NPGPTD,%ecx
	fillkpt(R(IdlePTD), $PG_RW)

#if defined(PAE) || defined(PAE_TABLES)
	movl	R(IdlePTD), %eax
	xorl	%ebx, %ebx
	movl	$NPGPTD, %ecx
	fillkpt(R(IdlePDPT), $0x0)
#endif

	ret

#ifdef XENHVM
/* Xen Hypercall page */
	.text
.p2align PAGE_SHIFT, 0x90	/* Hypercall_page needs to be PAGE aligned */

NON_GPROF_ENTRY(hypercall_page)
	.skip	0x1000, 0x90	/* Fill with "nop"s */
#endif
