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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	$Id: locore.s,v 1.58 1995/12/25 14:40:49 davidg Exp $
 */

/*
 * locore.s:	FreeBSD machine support for the Intel 386
 *		originally from: locore.s, by William F. Jolitz
 *
 *		Substantially rewritten by David Greenman, Rod Grimes,
 *			Bruce Evans, Wolfgang Solfrank, and many others.
 */

#include "assym.s"			/* system definitions */
#include <machine/psl.h>		/* processor status longword defs */
#include <machine/pte.h>		/* page table entry definitions */
#include <sys/errno.h>			/* error return codes */
#include <machine/specialreg.h>		/* x86 special registers */
#include <machine/cputypes.h>		/* x86 cpu type definitions */
#include <sys/syscall.h>		/* system call numbers */
#include <machine/asmacros.h>		/* miscellaneous asm macros */
#include <sys/reboot.h>
#include "apm.h"

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
	.globl	_PTmap,_PTD,_PTDpde
	.set	_PTmap,PTDPTDI << PDRSHIFT
	.set	_PTD,_PTmap + (PTDPTDI * NBPG)
	.set	_PTDpde,_PTD + (PTDPTDI * PDESIZE)

/*
 * Sysmap is the base address of the kernel page tables.
 * It is a bogus interface for kgdb and isn't used by the kernel itself.
 */
	.set	_Sysmap,_PTmap + (KPTDI * NBPG)

/*
 * APTmap, APTD is the alternate recursive pagemap.
 * It's used when modifying another process's page tables.
 */
	.globl	_APTmap,_APTD,_APTDpde
	.set	_APTmap,APTDPTDI << PDRSHIFT
	.set	_APTD,_APTmap + (APTDPTDI * NBPG)
	.set	_APTDpde,_PTD + (APTDPTDI * PDESIZE)

/*
 * Access to each processes kernel stack is via a region of
 * per-process address space (at the beginning), immediatly above
 * the user process stack.
 */
	.set	_kstack,USRSTACK
	.globl	_kstack

/*
 * Globals
 */
	.data

	.globl	tmpstk
	.space	0x1000		/* space for tmpstk - temporary stack */
tmpstk:
/*
 * Dummy frame at top of tmpstk to help debuggers print a nice stack trace.
 */
	.long	tmpstk+8	/* caller's %ebp */
	.long	_cpu_switch	/* caller */
	.long	0		/* %ebp == 0 should terminate trace */
	.long	_mvesp		/* in case %ebp == 0 doesn't work ... */
	.long	0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555

	.globl	_boothowto,_bootdev

	.globl	_cpu,_cold,_atdevbase,_cpu_vendor,_cpu_id,_bootinfo
	.globl	_cpu_high, _cpu_feature

_cpu:	.long	0				/* are we 386, 386sx, or 486 */
_cpu_id:	.long	0			/* stepping ID */
_cpu_high:	.long	0			/* highest arg to CPUID */
_cpu_feature:	.long	0			/* features */
_cpu_vendor:	.space	20			/* CPU origin code */
_bootinfo:	.space	BOOTINFO_SIZE		/* bootinfo that we can handle */
_cold:	.long	1				/* cold till we are not */
_atdevbase:	.long	0			/* location of start of iomem in virtual */
_atdevphys:	.long	0			/* location of device mapping ptes (phys) */

_KERNend:	.long	0			/* phys addr end of kernel (just after bss) */

	.globl	_IdlePTD
_IdlePTD:	.long	0			/* phys addr of kernel PTD */

_KPTphys:	.long	0			/* phys addr of kernel page tables */

	.globl	_proc0paddr
_proc0paddr:	.long	0			/* address of proc 0 address space */

#ifdef BDE_DEBUGGER
	.globl	_bdb_exists			/* flag to indicate BDE debugger is available */
_bdb_exists:	.long	0
#endif

/*
 * System Initialization
 */
	.text

/*
 * btext: beginning of text section.
 * Also the entry point (jumped to directly from the boot blocks).
 */
NON_GPROF_ENTRY(btext)
	movw	$0x1234,0x472			/* warm boot */

	/* Set up a real frame, some day we will be doing returns */
	pushl	%ebp
	movl	%esp, %ebp

	/* Don't trust what the BIOS gives for eflags. */
	pushl	$PSL_KERNEL
	popfl

	/* Don't trust what the BIOS gives for %fs and %gs. */
	mov	%ds, %ax
	mov	%ax, %fs
	mov	%ax, %gs

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
	je	2f				/* olddiskboot: */

	/*
	 * We have some form of return address, so this is either the
	 * old diskless netboot code, or the new uniform code.  That can
	 * be detected by looking at the 5th argument, it if is 0 we
	 * we are being booted by the new unifrom boot code.
	 */
	cmpl	$0,24(%ebp)
	je	1f				/* newboot: */

	/*
	 * Seems we have been loaded by the old diskless boot code, we
	 * don't stand a chance of running as the diskless structure
	 * changed considerably between the two, so just halt.
	 */
	 hlt

	/*
	 * We have been loaded by the new uniform boot code.
	 * Lets check the bootinfo version, and if we do not understand
	 * it we return to the loader with a status of 1 to indicate this error
	 */
1:	/* newboot: */
	movl	28(%ebp),%ebx		/* &bootinfo.version */
	movl	BI_VERSION(%ebx),%eax
	cmpl	$1,%eax			/* We only understand version 1 */
	je	1f
	movl	$1,%eax			/* Return status */
	leave
	ret

1:
	/*
	 * If we have a kernelname copy it in
	 */
	movl	BI_KERNELNAME(%ebx),%esi
	cmpl	$0,%esi
	je	2f			/* No kernelname */
	movl	$MAXPATHLEN,%ecx	/* Brute force!!! */
	lea	_kernelname-KERNBASE,%edi
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
	movl	$_bootinfo-KERNBASE,%edi
	cmpl	$BOOTINFO_SIZE,%ecx
	jbe	got_common_bi_size
	movl	$BOOTINFO_SIZE,%ecx
got_common_bi_size:
	cld
	rep
	movsb

#ifdef NFS
	/*
	 * If we have a nfs_diskless structure copy it in
	 */
	movl	BI_NFS_DISKLESS(%ebx),%esi
	cmpl	$0,%esi
	je	2f
	lea	_nfs_diskless-KERNBASE,%edi
	movl	$NFSDISKLESS_SIZE,%ecx
	cld
	rep
	movsb
	lea	_nfs_diskless_valid-KERNBASE,%edi
	movl	$1,(%edi)
#endif

	/*
	 * The old style disk boot.
	 *	(*btext)(howto, bootdev, cyloffset, esym);
	 * Note that the newer boot code just falls into here to pick
	 * up howto and bootdev, cyloffset and esym are no longer used
	 */
2:	/* olddiskboot: */
	movl	8(%ebp),%eax
	movl	%eax,_boothowto-KERNBASE
	movl	12(%ebp),%eax
	movl	%eax,_bootdev-KERNBASE

#if NAPM > 0
	/* call APM BIOS driver setup (i386/apm/apm_setup.s) */
	call	_apm_setup
#endif /* NAPM */

	/* Find out our CPU type. */

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
	jnz	1f
	movl	$CPU_386,_cpu-KERNBASE
	jmp	3f

1:	/* Try to toggle identification flag; does not exist on early 486s. */
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
	jnz	1f
	movl	$CPU_486,_cpu-KERNBASE

	/* check for Cyrix 486DLC -- based on check routine  */
	/* documented in "Cx486SLC/e SMM Programmer's Guide" */
	xorw	%dx,%dx
	cmpw	%dx,%dx			# set flags to known state
	pushfw
	popw	%cx			# store flags in ecx
	movw	$0xffff,%ax
	movw	$0x0004,%bx
	divw	%bx
	pushfw
	popw	%ax
	andw	$0x08d5,%ax		# mask off important bits
	andw	$0x08d5,%cx
	cmpw	%ax,%cx

	jnz	3f			# if flags changed, Intel chip

	movl	$CPU_486DLC,_cpu-KERNBASE # set CPU value for Cyrix
	movl	$0x69727943,_cpu_vendor-KERNBASE	# store vendor string
	movw	$0x0078,_cpu_vendor-KERNBASE+4

#ifndef CYRIX_CACHE_WORKS
	/* Disable caching of the ISA hole only. */
	invd
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	orb	$(CCR0_NC1|CCR0_BARB),%al
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	invd
#else /* CYRIX_CACHE_WORKS */
	/* Set cache parameters */
	invd				# Start with guaranteed clean cache
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	andb	$~CCR0_NC0,%al
#ifndef CYRIX_CACHE_REALLY_WORKS
	orb	$(CCR0_NC1|CCR0_BARB),%al
#else
	orb	$CCR0_NC1,%al
#endif
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	/* clear non-cacheable region 1	*/
	movb	$(NCR1+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 2	*/
	movb	$(NCR2+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 3	*/
	movb	$(NCR3+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 4	*/
	movb	$(NCR4+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* enable caching in CR0 */
	movl	%cr0,%eax
	andl	$~(CR0_CD|CR0_NW),%eax
	movl	%eax,%cr0
	invd
#endif /* CYRIX_CACHE_WORKS */
	jmp	3f

1:	/* Use the `cpuid' instruction. */
	xorl	%eax,%eax
	.byte	0x0f,0xa2			# cpuid 0
	movl	%eax,_cpu_high-KERNBASE		# highest capability
	movl	%ebx,_cpu_vendor-KERNBASE	# store vendor string
	movl	%edx,_cpu_vendor+4-KERNBASE
	movl	%ecx,_cpu_vendor+8-KERNBASE
	movb	$0,_cpu_vendor+12-KERNBASE

	movl	$1,%eax
	.byte	0x0f,0xa2			# cpuid 1
	movl	%eax,_cpu_id-KERNBASE		# store cpu_id
	movl	%edx,_cpu_feature-KERNBASE	# store cpu_feature
	rorl	$8,%eax				# extract family type
	andl	$15,%eax
	cmpl	$5,%eax
	jae	1f

	/* less than Pentium; must be 486 */
	movl	$CPU_486,_cpu-KERNBASE
	jmp	3f
1:
	/* a Pentium? */
	cmpl	$5,%eax
	jne	2f
	movl	$CPU_586,_cpu-KERNBASE
	jmp	3f
2:
	/* Greater than Pentium...call it a Pentium Pro */
	movl	$CPU_686,_cpu-KERNBASE
3:

	/*
	 * Finished with old stack; load new %esp now instead of later so
	 * we can trace this code without having to worry about the trace
	 * trap clobbering the memory test or the zeroing of the bss+bootstrap
	 * page tables.
	 *
	 * XXX - wdboot clears the bss after testing that this is safe.
	 * This is too wasteful - memory below 640K is scarce.  The boot
	 * program should check:
	 *	text+data <= &stack_variable - more_space_for_stack
	 *	text+data+bss+pad+space_for_page_tables <= end_of_memory
	 * Oops, the gdt is in the carcass of the boot program so clearing
	 * the rest of memory is still not possible.
	 */
	movl	$tmpstk-KERNBASE,%esp		/* bootstrap stack end location */

/*
 * Virtual address space of kernel:
 *
 *	text | data | bss | [syms] | page dir | proc0 kernel stack | usr stk map | Sysmap
 *      pages:                          1         UPAGES (2)             1         NKPT (7)
 */

/* find end of kernel image */
	movl	$_end-KERNBASE,%ecx
	addl	$NBPG-1,%ecx			/* page align up */
	andl	$~(NBPG-1),%ecx
	movl	%ecx,%esi			/* esi = start of free memory */
	movl	%ecx,_KERNend-KERNBASE		/* save end of kernel */

/* clear bss */
	movl	$_edata-KERNBASE,%edi
	subl	%edi,%ecx			/* get amount to clear */
	xorl	%eax,%eax			/* specify zero fill */
	cld
	rep
	stosb

#ifdef DDB
/* include symbols in "kernel image" if they are loaded */
	movl	_bootinfo+BI_ESYMTAB-KERNBASE,%edi
	testl	%edi,%edi
	je	over_symalloc
	addl	$NBPG-1,%edi
	andl	$~(NBPG-1),%edi
	movl	%edi,%esi
	movl	%esi,_KERNend-KERNBASE
	movl	$KERNBASE,%edi
	addl	%edi,_bootinfo+BI_SYMTAB-KERNBASE
	addl	%edi,_bootinfo+BI_ESYMTAB-KERNBASE
over_symalloc:
#endif

/*
 * The value in esi is both the end of the kernel bss and a pointer to
 * the kernel page directory, and is used by the rest of locore to build
 * the tables.
 * esi + 1(page dir) + 2(UPAGES) + 1(p0stack) + NKPT(number of kernel
 * page table pages) is then passed on the stack to init386(first) as
 * the value first. esi should ALWAYS be page aligned!!
 */
	movl	%esi,%ecx			/* Get current first availiable address */

/* clear pagetables, page directory, stack, etc... */
	movl	%esi,%edi			/* base (page directory) */
	movl	$((1+UPAGES+1+NKPT)*NBPG),%ecx	/* amount to clear */
	xorl	%eax,%eax			/* specify zero fill */
	cld
	rep
	stosb

/* physical address of Idle proc/kernel page directory */
	movl	%esi,_IdlePTD-KERNBASE

/*
 * fillkpt
 *	eax = (page frame address | control | status) == pte
 *	ebx = address of page table
 *	ecx = how many pages to map
 */
#define	fillkpt		\
1:	movl	%eax,(%ebx)	; \
	addl	$NBPG,%eax	; /* increment physical address */ \
	addl	$4,%ebx		; /* next pte */ \
	loop	1b		;

/*
 * Map Kernel
 *
 * First step - build page tables
 */
#if defined (KGDB) || defined (BDE_DEBUGGER)
	movl	_KERNend-KERNBASE,%ecx		/* this much memory, */
	shrl	$PGSHIFT,%ecx			/* for this many PTEs */
#ifdef BDE_DEBUGGER
	cmpl	$0xa0,%ecx			/* XXX - cover debugger pages */
	jae	1f
	movl	$0xa0,%ecx
1:
#endif /* BDE_DEBUGGER */
	movl	$PG_V|PG_KW,%eax		/* kernel R/W, valid */
	lea	((1+UPAGES+1)*NBPG)(%esi),%ebx	/* phys addr of kernel PT base */
	movl	%ebx,_KPTphys-KERNBASE		/* save in global */
	fillkpt

#else /* !KGDB && !BDE_DEBUGGER */
	/* write protect kernel text (doesn't do a thing for 386's - only 486's) */
	movl	$_etext-KERNBASE,%ecx		/* get size of text */
	addl	$NBPG-1,%ecx			/* round up to page */
	shrl	$PGSHIFT,%ecx			/* for this many PTEs */
	movl	$PG_V|PG_KR,%eax		/* specify read only */
#if 0
	movl	$_etext,%ecx			/* get size of text */
	subl	$_btext,%ecx
	addl	$NBPG-1,%ecx			/* round up to page */
	shrl	$PGSHIFT,%ecx			/* for this many PTEs */
	movl	$_btext-KERNBASE,%eax		/* get offset to physical memory */
	orl	$PG_V|PG_KR,%eax		/* specify read only */
#endif
	lea	((1+UPAGES+1)*NBPG)(%esi),%ebx	/* phys addr of kernel PT base */
	movl	%ebx,_KPTphys-KERNBASE		/* save in global */
	fillkpt

	/* data and bss are r/w */
	andl	$PG_FRAME,%eax			/* strip to just addr of bss */
	movl	_KERNend-KERNBASE,%ecx		/* calculate size */
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$PG_V|PG_KW,%eax		/* valid, kernel read/write */
	fillkpt
#endif /* KGDB || BDE_DEBUGGER */

/* now initialize the page dir, upages, and p0stack PT */

	movl	$(1+UPAGES+1),%ecx		/* number of PTEs */
	movl	%esi,%eax			/* phys address of PTD */
	andl	$PG_FRAME,%eax			/* convert to PFN, should be a NOP */
	orl	$PG_V|PG_KW,%eax		/* valid, kernel read/write */
	movl	%esi,%ebx			/* calculate pte offset to ptd */
	shrl	$PGSHIFT-2,%ebx
	addl	%esi,%ebx			/* address of page directory */
	addl	$((1+UPAGES+1)*NBPG),%ebx	/* offset to kernel page tables */
	fillkpt

/* map I/O memory map */

	movl    _KPTphys-KERNBASE,%ebx		/* base of kernel page tables */
	lea     (0xa0 * PTESIZE)(%ebx),%ebx	/* hardwire ISA hole at KERNBASE + 0xa0000 */
	movl	$0x100-0xa0,%ecx		/* for this many pte s, */
	movl	$(0xa0000|PG_V|PG_KW|PG_N),%eax	/* valid, kernel read/write, non-cacheable */
	movl	%ebx,_atdevphys-KERNBASE	/* save phys addr of ptes */
	fillkpt

 /* map proc 0's kernel stack into user page table page */

	movl	$UPAGES,%ecx			/* for this many pte s, */
	lea	(1*NBPG)(%esi),%eax		/* physical address in proc 0 */
	lea	(KERNBASE)(%eax),%edx		/* change into virtual addr */
	movl	%edx,_proc0paddr-KERNBASE	/* save VA for proc 0 init */
	orl	$PG_V|PG_KW,%eax		/* valid, kernel read/write */
	lea	((1+UPAGES)*NBPG)(%esi),%ebx	/* addr of stack page table in proc 0 */
	addl	$(KSTKPTEOFF * PTESIZE),%ebx	/* offset to kernel stack PTE */
	fillkpt

/*
 * Initialize kernel page table directory
 */
	/* install a pde for temporary double map of bottom of VA */
	movl	_KPTphys-KERNBASE,%eax
	orl     $PG_V|PG_KW,%eax		/* valid, kernel read/write */
	movl	%eax,(%esi)			/* which is where temp maps! */

	/* initialize kernel pde's */
	movl	$(NKPT),%ecx			/* for this many PDEs */
	lea	(KPTDI*PDESIZE)(%esi),%ebx	/* offset of pde for kernel */
	fillkpt

	/* install a pde recursively mapping page directory as a page table! */
	movl	%esi,%eax			/* phys address of ptd in proc 0 */
	orl	$PG_V|PG_KW,%eax		/* pde entry is valid */
	movl	%eax,PTDPTDI*PDESIZE(%esi)	/* which is where PTmap maps! */

	/* install a pde to map kernel stack for proc 0 */
	lea	((1+UPAGES)*NBPG)(%esi),%eax	/* physical address of pt in proc 0 */
	orl	$PG_V|PG_KW,%eax		/* pde entry is valid */
	movl	%eax,KSTKPTDI*PDESIZE(%esi)	/* which is where kernel stack maps! */

#ifdef BDE_DEBUGGER
	/* copy and convert stuff from old gdt and idt for debugger */

	cmpl	$0x0375c339,0x96104		/* XXX - debugger signature */
	jne	1f
	movb	$1,_bdb_exists-KERNBASE
1:
	pushal
	subl	$2*6,%esp

	sgdt	(%esp)
	movl	2(%esp),%esi			/* base address of current gdt */
	movl	$_gdt-KERNBASE,%edi
	movl	%edi,2(%esp)
	movl	$8*18/4,%ecx
	cld
	rep					/* copy gdt */
	movsl
	movl	$_gdt-KERNBASE,-8+2(%edi)	/* adjust gdt self-ptr */
	movb	$0x92,-8+5(%edi)

	sidt	6(%esp)
	movl	6+2(%esp),%esi			/* base address of current idt */
	movl	8+4(%esi),%eax			/* convert dbg descriptor to ... */
	movw	8(%esi),%ax
	movl	%eax,bdb_dbg_ljmp+1-KERNBASE	/* ... immediate offset ... */
	movl	8+2(%esi),%eax
	movw	%ax,bdb_dbg_ljmp+5-KERNBASE	/* ... and selector for ljmp */
	movl	24+4(%esi),%eax			/* same for bpt descriptor */
	movw	24(%esi),%ax
	movl	%eax,bdb_bpt_ljmp+1-KERNBASE
	movl	24+2(%esi),%eax
	movw	%ax,bdb_bpt_ljmp+5-KERNBASE

	movl	$_idt-KERNBASE,%edi
	movl	%edi,6+2(%esp)
	movl	$8*4/4,%ecx
	cld
	rep					/* copy idt */
	movsl

	lgdt	(%esp)
	lidt	6(%esp)

	addl	$2*6,%esp
	popal
#endif /* BDE_DEBUGGER */

	/* load base of page directory and enable mapping */
	movl	%esi,%eax			/* phys address of ptd in proc 0 */
	movl	%eax,%cr3			/* load ptd addr into mmu */
	movl	%cr0,%eax			/* get control word */
	orl	$CR0_PE|CR0_PG,%eax		/* enable paging */
	movl	%eax,%cr0			/* and let's page NOW! */

	pushl	$begin				/* jump to high mem */
	ret

begin: /* now running relocated at KERNBASE where the system is linked to run */
	movl	_atdevphys,%edx			/* get pte PA */
	subl	_KPTphys,%edx			/* remove base of ptes, now have phys offset */
	shll	$PGSHIFT-2,%edx			/* corresponding to virt offset */
	addl	$KERNBASE,%edx			/* add virtual base */
	movl	%edx,_atdevbase

	/* set up bootstrap stack */
	movl	$_kstack+UPAGES*NBPG,%esp	/* bootstrap stack end location */
	xorl	%eax,%eax			/* mark end of frames */
	movl	%eax,%ebp
	movl	_proc0paddr,%eax
	movl	%esi,PCB_CR3(%eax)

#ifdef BDE_DEBUGGER
	/* relocate debugger gdt entries */

	movl	$_gdt+8*9,%eax			/* adjust slots 9-17 */
	movl	$9,%ecx
reloc_gdt:
	movb	$KERNBASE>>24,7(%eax)		/* top byte of base addresses, was 0, */
	addl	$8,%eax				/* now KERNBASE>>24 */
	loop	reloc_gdt

	cmpl	$0,_bdb_exists
	je	1f
	int	$3
1:
#endif /* BDE_DEBUGGER */

	/*
	 * Prepare "first" - physical address of first available page
	 * after the kernel+pdir+upages+p0stack+page tables
	 */
	lea	((1+UPAGES+1+NKPT)*NBPG)(%esi),%esi

	pushl	%esi				/* value of first for init386(first) */
	call	_init386			/* wire 386 chip for unix operation */
	popl	%esi

	.globl	__ucodesel,__udatasel

	pushl	$0				/* unused */
	pushl	__udatasel			/* ss */
	pushl	$0				/* esp - filled in by execve() */
	pushl	$PSL_USER			/* eflags (IOPL 0, int enab) */
	pushl	__ucodesel			/* cs */
	pushl	$0				/* eip - filled in by execve() */
	subl	$(12*4),%esp			/* space for rest of registers */

	pushl	%esp				/* call main with frame pointer */
	call	_main				/* autoconfiguration, mountroot etc */

	addl	$(13*4),%esp			/* back to a frame we can return with */

	/*
	 * now we've run main() and determined what cpu-type we are, we can
	 * enable write protection and alignment checking on i486 cpus and
	 * above.
	 */
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl    $CPUCLASS_386,_cpu_class
	je	1f
	movl	%cr0,%eax			/* get control word */
	orl	$CR0_WP|CR0_AM,%eax		/* enable i486 features */
	movl	%eax,%cr0			/* and do it */
#endif
	/*
	 * on return from main(), we are process 1
	 * set up address space and stack so that we can 'return' to user mode
	 */
1:
	movl	__ucodesel,%eax
	movl	__udatasel,%ecx

	movl	%cx,%ds
	movl	%cx,%es
	movl	%ax,%fs				/* double map cs to fs */
	movl	%cx,%gs				/* and ds to gs */
	iret					/* goto user! */

#define LCALL(x,y)	.byte 0x9a ; .long y ; .word x

NON_GPROF_ENTRY(sigcode)
	call	SIGF_HANDLER(%esp)
	lea	SIGF_SC(%esp),%eax		/* scp (the call may have clobbered the */
						/* copy at 8(%esp)) */
	pushl	%eax
	pushl	%eax				/* junk to fake return address */
	movl	$103,%eax			/* XXX sigreturn() */
	LCALL(0x7,0)				/* enter kernel with args on stack */
	hlt					/* never gets here */

	.globl	_szsigcode
_szsigcode:
	.long	_szsigcode-_sigcode
