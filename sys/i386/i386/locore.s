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
 *	$Id: locore.s,v 1.75.2.1 1996/11/12 09:07:49 phk Exp $
 *
 *		originally from: locore.s, by William F. Jolitz
 *
 *		Substantially rewritten by David Greenman, Rod Grimes,
 *			Bruce Evans, Wolfgang Solfrank, Poul-Henning Kamp
 *			and many others.
 */

#include "apm.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_userconfig.h"

#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/reboot.h>

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.s"

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
	.set	_PTmap,(PTDPTDI << PDRSHIFT)
	.set	_PTD,_PTmap + (PTDPTDI * PAGE_SIZE)
	.set	_PTDpde,_PTD + (PTDPTDI * PDESIZE)

/*
 * APTmap, APTD is the alternate recursive pagemap.
 * It's used when modifying another process's page tables.
 */
	.globl	_APTmap,_APTD,_APTDpde
	.set	_APTmap,APTDPTDI << PDRSHIFT
	.set	_APTD,_APTmap + (APTDPTDI * PAGE_SIZE)
	.set	_APTDpde,_PTD + (APTDPTDI * PDESIZE)

/*
 * Access to each processes kernel stack is via a region of
 * per-process address space (at the beginning), immediately above
 * the user process stack.
 */
	.set	_kstack,USRSTACK
	.globl	_kstack

/*
 * Globals
 */
	.data
	ALIGN_DATA		/* just to be sure */

	.globl	tmpstk
	.space	0x2000		/* space for tmpstk - temporary stack */
tmpstk:

	.globl	_boothowto,_bootdev

	.globl	_cpu,_cpu_vendor,_cpu_id,_bootinfo
	.globl	_cpu_high, _cpu_feature

_cpu:	.long	0				/* are we 386, 386sx, or 486 */
_cpu_id:	.long	0			/* stepping ID */
_cpu_high:	.long	0			/* highest arg to CPUID */
_cpu_feature:	.long	0			/* features */
_cpu_vendor:	.space	20			/* CPU origin code */
_bootinfo:	.space	BOOTINFO_SIZE		/* bootinfo that we can handle */

_KERNend:	.long	0			/* phys addr end of kernel (just after bss) */
physfree:	.long	0			/* phys addr of next free page */
p0upa:	.long	0				/* phys addr of proc0's UPAGES */
p0upt:	.long	0				/* phys addr of proc0's UPAGES page table */

	.globl	_IdlePTD
_IdlePTD:	.long	0			/* phys addr of kernel PTD */

_KPTphys:	.long	0			/* phys addr of kernel page tables */

	.globl	_proc0paddr
_proc0paddr:	.long	0			/* address of proc 0 address space */

#ifdef BDE_DEBUGGER
	.globl	_bdb_exists			/* flag to indicate BDE debugger is present */
_bdb_exists:	.long	0
#endif


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
	shll	$2,%ebx			; \
	addl	base,%ebx		; \
	orl	$PG_V,%eax		; \
	orl	prot,%eax		; \
1:	movl	%eax,(%ebx)		; \
	addl	$PAGE_SIZE,%eax		; /* increment physical address */ \
	addl	$4,%ebx			; /* next pte */ \
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
	fillkpt(R(_KPTphys), prot)

	.text
/**********************************************************************
 *
 * This is where the bootblocks start us, set the ball rolling...
 *
 */
NON_GPROF_ENTRY(btext)

#ifdef BDE_DEBUGGER
#ifdef BIOS_STEALS_3K
	cmpl	$0x0375c339,0x95504
#else
	cmpl	$0x0375c339,0x96104	/* XXX - debugger signature */
#endif
	jne	1f
	movb	$1,R(_bdb_exists)
1:
#endif

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

	call	recover_bootinfo

/* Get onto a stack that we can trust. */
/*
 * XXX this step is delayed in case recover_bootinfo needs to return via
 * the old stack, but it need not be, since recover_bootinfo actually
 * returns via the old frame.
 */
	movl	$R(tmpstk),%esp

	call	identify_cpu

/* clear bss */
/*
 * XXX this should be done a little earlier.
 *
 * XXX we don't check that there is memory for our bss and page tables
 * before using it.
 *
 * XXX the boot program somewhat bogusly clears the bss.  We still have
 * to do it in case we were unzipped by kzipboot.  Then the boot program
 * only clears kzipboot's bss.
 *
 * XXX the gdt and idt are still somewhere in the boot program.  We
 * depend on the convention that the boot program is below 1MB and we
 * are above 1MB to keep the gdt and idt  away from the bss and page
 * tables.  The idt is only used if BDE_DEBUGGER is enabled.
 */
	movl	$R(_end),%ecx
	movl	$R(_edata),%edi
	subl	%edi,%ecx
	xorl	%eax,%eax
	cld
	rep
	stosb

#if NAPM > 0
/*
 * XXX it's not clear that APM can live in the current environonment.
 * Only pc-relative addressing works.
 */
	call	_apm_setup
#endif

	call	create_pagetables

#ifdef BDE_DEBUGGER
/*
 * Adjust as much as possible for paging before enabling paging so that the
 * adjustments can be traced.
 */
	call	bdb_prepare_paging
#endif

/* Now enable paging */
	movl	R(_IdlePTD), %eax
	movl	%eax,%cr3			/* load ptd addr into mmu */
	movl	%cr0,%eax			/* get control word */
	orl	$CR0_PE|CR0_PG,%eax		/* enable paging */
	movl	%eax,%cr0			/* and let's page NOW! */

#ifdef BDE_DEBUGGER
/*
 * Complete the adjustments for paging so that we can keep tracing through
 * initi386() after the low (physical) addresses for the gdt and idt become
 * invalid.
 */
	call	bdb_commit_paging
#endif

	pushl	$begin				/* jump to high virtualized address */
	ret

/* now running relocated at KERNBASE where the system is linked to run */
begin:
	/* set up bootstrap stack */
	movl	$_kstack+UPAGES*PAGE_SIZE,%esp	/* bootstrap stack end location */
	xorl	%eax,%eax			/* mark end of frames */
	movl	%eax,%ebp
	movl	_proc0paddr,%eax
	movl	_IdlePTD, %esi
	movl	%esi,PCB_CR3(%eax)

	movl	physfree, %esi
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
1:
#endif
	/*
	 * on return from main(), we are process 1
	 * set up address space and stack so that we can 'return' to user mode
	 */
	movl	__ucodesel,%eax
	movl	__udatasel,%ecx

	movl	%cx,%ds
	movl	%cx,%es
	movl	%ax,%fs				/* double map cs to fs */
	movl	%cx,%gs				/* and ds to gs */
	iret					/* goto user! */

#define LCALL(x,y)	.byte 0x9a ; .long y ; .word x

/*
 * Signal trampoline, copied to top of user stack
 */
NON_GPROF_ENTRY(sigcode)
	call	SIGF_HANDLER(%esp)
	lea	SIGF_SC(%esp),%eax		/* scp (the call may have clobbered the */
						/* copy at 8(%esp)) */
	pushl	%eax
	pushl	%eax				/* junk to fake return address */
	movl	$SYS_sigreturn,%eax		/* sigreturn() */
	LCALL(0x7,0)				/* enter kernel with args on stack */
	hlt					/* never gets here */
	.align	2,0x90				/* long word text-align */
_esigcode:

	.data
	.globl	_szsigcode
_szsigcode:
	.long	_esigcode-_sigcode
	.text

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
	movl	$R(_kernelname),%edi
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
	movl	$R(_bootinfo),%edi
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
	je	olddiskboot
	movl	$R(_nfs_diskless),%edi
	movl	$NFSDISKLESS_SIZE,%ecx
	cld
	rep
	movsb
	movl	$R(_nfs_diskless_valid),%edi
	movl	$1,(%edi)
#endif

	/*
	 * The old style disk boot.
	 *	(*btext)(howto, bootdev, cyloffset, esym);
	 * Note that the newer boot code just falls into here to pick
	 * up howto and bootdev, cyloffset and esym are no longer used
	 */
olddiskboot:
	movl	8(%ebp),%eax
	movl	%eax,R(_boothowto)
	movl	12(%ebp),%eax
	movl	%eax,R(_bootdev)

#if defined(USERCONFIG_BOOT) && defined(USERCONFIG)
	movl	$0x10200, %esi
	movl	$R(_userconfig_from_boot),%edi
	movl	$512,%ecx
	cld
	rep
	movsb
#endif /* USERCONFIG_BOOT */

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
	jnz	1f
	movl	$CPU_386,R(_cpu)
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
	movl	$CPU_486,R(_cpu)

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

	movl	$CPU_486DLC,R(_cpu) # set CPU value for Cyrix
	movl	$0x69727943,R(_cpu_vendor)	# store vendor string
	movw	$0x0078,R(_cpu_vendor+4)

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
#else /* CYRIX_CACHE_REALLY_WORKS */
	orb	$CCR0_NC1,%al
#endif /* !CYRIX_CACHE_REALLY_WORKS */
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
#endif /* !CYRIX_CACHE_WORKS */
	jmp	3f

1:	/* Use the `cpuid' instruction. */
	xorl	%eax,%eax
	.byte	0x0f,0xa2			# cpuid 0
	movl	%eax,R(_cpu_high)		# highest capability
	movl	%ebx,R(_cpu_vendor)		# store vendor string
	movl	%edx,R(_cpu_vendor+4)
	movl	%ecx,R(_cpu_vendor+8)
	movb	$0,R(_cpu_vendor+12)

	movl	$1,%eax
	.byte	0x0f,0xa2			# cpuid 1
	movl	%eax,R(_cpu_id)			# store cpu_id
	movl	%edx,R(_cpu_feature)		# store cpu_feature
	rorl	$8,%eax				# extract family type
	andl	$15,%eax
	cmpl	$5,%eax
	jae	1f

	/* less than Pentium; must be 486 */
	movl	$CPU_486,R(_cpu)
	jmp	3f
1:
	/* a Pentium? */
	cmpl	$5,%eax
	jne	2f
	movl	$CPU_586,R(_cpu)
	jmp	3f
2:
	/* Greater than Pentium...call it a Pentium Pro */
	movl	$CPU_686,R(_cpu)
3:
	ret


/**********************************************************************
 *
 * Create the first page directory and its page tables.
 *
 */

create_pagetables:

	testl	$CPUID_PGE, R(_cpu_feature)
	jz	1f
	movl	%cr4, %eax
	orl	$CR4_PGE, %eax
	movl	%eax, %cr4
1:

/* Find end of kernel image (rounded up to a page boundary). */
	movl	$R(_end),%esi

/* include symbols in "kernel image" if they are loaded and useful */
#ifdef DDB
	movl	R(_bootinfo+BI_ESYMTAB),%edi
	testl	%edi,%edi
	je	over_symalloc
	movl	%edi,%esi
	movl	$KERNBASE,%edi
	addl	%edi,R(_bootinfo+BI_SYMTAB)
	addl	%edi,R(_bootinfo+BI_ESYMTAB)
over_symalloc:
#endif

	addl	$PAGE_MASK,%esi
	andl	$~PAGE_MASK,%esi
	movl	%esi,R(_KERNend)	/* save end of kernel */
	movl	%esi,R(physfree)	/* next free page is at end of kernel */

/* Allocate Kernel Page Tables */
	ALLOCPAGES(NKPT)
	movl	%esi,R(_KPTphys)

/* Allocate Page Table Directory */
	ALLOCPAGES(1)
	movl	%esi,R(_IdlePTD)

/* Allocate UPAGES */
	ALLOCPAGES(UPAGES)
	movl	%esi,R(p0upa)
	addl	$KERNBASE, %esi
	movl	%esi, R(_proc0paddr)

/* Allocate proc0's page table for the UPAGES. */
	ALLOCPAGES(1)
	movl	%esi,R(p0upt)

/* Map read-only from zero to the end of the kernel text section */
	xorl	%eax, %eax
#ifdef BDE_DEBUGGER
/* If the debugger is present, actually map everything read-write. */
	cmpl	$0,R(_bdb_exists)
	jne	map_read_write
#endif
	xorl	%edx,%edx
	testl	$CPUID_PGE, R(_cpu_feature)
	jz	2f
	orl	$PG_G,%edx
	
2:	movl	$R(_etext),%ecx
	addl	$PAGE_MASK,%ecx
	shrl	$PAGE_SHIFT,%ecx
	fillkptphys(%edx)

/* Map read-write, data, bss and symbols */
	movl	$R(_etext),%eax
	addl	$PAGE_MASK, %eax
	andl	$~PAGE_MASK, %eax
map_read_write:
	movl	$PG_RW,%edx
	testl	$CPUID_PGE, R(_cpu_feature)
	jz	1f
	orl	$PG_G,%edx
	
1:	movl	R(_KERNend),%ecx
	subl	%eax,%ecx
	shrl	$PAGE_SHIFT,%ecx
	fillkptphys(%edx)

/* Map page directory. */
	movl	R(_IdlePTD), %eax
	movl	$1, %ecx
	fillkptphys($PG_RW)

/* Map proc0's page table for the UPAGES. */
	movl	R(p0upt), %eax
	movl	$1, %ecx
	fillkptphys($PG_RW)

/* Map proc0's UPAGES in the physical way ... */
	movl	R(p0upa), %eax
	movl	$UPAGES, %ecx
	fillkptphys($PG_RW)

/* Map ISA hole */
	movl	$ISA_HOLE_START, %eax
	movl	$ISA_HOLE_LENGTH>>PAGE_SHIFT, %ecx
	fillkptphys($PG_RW)

/* Map proc0s UPAGES in the special page table for this purpose ... */
	movl	R(p0upa), %eax
	movl	$KSTKPTEOFF, %ebx
	movl	$UPAGES, %ecx
	fillkpt(R(p0upt), $PG_RW)

/* ... and put the page table in the pde. */
	movl	R(p0upt), %eax
	movl	$KSTKPTDI, %ebx
	movl	$1, %ecx
	fillkpt(R(_IdlePTD), $PG_RW)

/* install a pde for temporary double map of bottom of VA */
	movl	R(_KPTphys), %eax
	xorl	%ebx, %ebx
	movl	$1, %ecx
	fillkpt(R(_IdlePTD), $PG_RW)

/* install pde's for pt's */
	movl	R(_KPTphys), %eax
	movl	$KPTDI, %ebx
	movl	$NKPT, %ecx
	fillkpt(R(_IdlePTD), $PG_RW)

/* install a pde recursively mapping page directory as a page table */
	movl	R(_IdlePTD), %eax
	movl	$PTDPTDI, %ebx
	movl	$1,%ecx
	fillkpt(R(_IdlePTD), $PG_RW)

	ret

#ifdef BDE_DEBUGGER
bdb_prepare_paging:
	cmpl	$0,R(_bdb_exists)
	je	bdb_prepare_paging_exit

	subl	$6,%esp

	/*
	 * Copy and convert debugger entries from the bootstrap gdt and idt
	 * to the kernel gdt and idt.  Everything is still in low memory.
	 * Tracing continues to work after paging is enabled because the
	 * low memory addresses remain valid until everything is relocated.
	 * However, tracing through the setidt() that initializes the trace
	 * trap will crash.
	 */
	sgdt	(%esp)
	movl	2(%esp),%esi		/* base address of bootstrap gdt */
	movl	$R(_gdt),%edi
	movl	%edi,2(%esp)		/* prepare to load kernel gdt */
	movl	$8*18/4,%ecx
	cld
	rep				/* copy gdt */
	movsl
	movl	$R(_gdt),-8+2(%edi)	/* adjust gdt self-ptr */
	movb	$0x92,-8+5(%edi)
	lgdt	(%esp)

	sidt	(%esp)
	movl	2(%esp),%esi		/* base address of current idt */
	movl	8+4(%esi),%eax		/* convert dbg descriptor to ... */
	movw	8(%esi),%ax
	movl	%eax,R(bdb_dbg_ljmp+1)	/* ... immediate offset ... */
	movl	8+2(%esi),%eax
	movw	%ax,R(bdb_dbg_ljmp+5)	/* ... and selector for ljmp */
	movl	24+4(%esi),%eax		/* same for bpt descriptor */
	movw	24(%esi),%ax
	movl	%eax,R(bdb_bpt_ljmp+1)
	movl	24+2(%esi),%eax
	movw	%ax,R(bdb_bpt_ljmp+5)
	movl	$R(_idt),%edi
	movl	%edi,2(%esp)		/* prepare to load kernel idt */
	movl	$8*4/4,%ecx
	cld
	rep				/* copy idt */
	movsl
	lidt	(%esp)

	addl	$6,%esp

bdb_prepare_paging_exit:
	ret

/* Relocate debugger gdt entries and gdt and idt pointers. */
bdb_commit_paging:
	cmpl	$0,_bdb_exists
	je	bdb_commit_paging_exit

	movl	$_gdt+8*9,%eax		/* adjust slots 9-17 */
	movl	$9,%ecx
reloc_gdt:
	movb	$KERNBASE>>24,7(%eax)	/* top byte of base addresses, was 0, */
	addl	$8,%eax			/* now KERNBASE>>24 */
	loop	reloc_gdt

	subl	$6,%esp
	sgdt	(%esp)
	addl	$KERNBASE,2(%esp)
	lgdt	(%esp)
	sidt	(%esp)
	addl	$KERNBASE,2(%esp)
	lidt	(%esp)
	addl	$6,%esp

	int	$3

bdb_commit_paging_exit:
	ret

#endif /* BDE_DEBUGGER */
