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
 *	$Id: locore.s,v 1.13 1994/01/16 02:21:58 martin Exp $
 */

/*
 * locore.s:	FreeBSD machine support for the Intel 386
 *		originally from: locore.s, by William F. Jolitz
 *
 *		Substantially rewritten by David Greenman, Rod Grimes,
 *			Bruce Evans, Wolfgang Solfrank, and many others.
 */

#include "npx.h"				/* for NNPX */

#include "assym.s"				/* system definitions */
#include "machine/psl.h"			/* processor status longword defs */
#include "machine/pte.h"			/* page table entry definitions */

#include "errno.h"				/* error return codes */

#include "machine/specialreg.h"			/* x86 special registers */
#include "i386/isa/debug.h"			/* BDE debugging macros */
#include "machine/cputypes.h"			/* x86 cpu type definitions */

#include "syscall.h"				/* system call numbers */

#include "machine/asmacros.h"			/* miscellaneous asm macros */

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
	.globl	_PTmap,_PTD,_PTDpde,_Sysmap
	.set	_PTmap,PTDPTDI << PDRSHIFT
	.set	_PTD,_PTmap + (PTDPTDI * NBPG)
	.set	_PTDpde,_PTD + (PTDPTDI * PDESIZE)

/* Sysmap is the base address of the kernel page tables */
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
	.globl	_esym
_esym:	.long	0				/* ptr to end of syms */

	.globl	_boothowto,_bootdev,_curpcb

	.globl	_cpu,_cold,_atdevbase
_cpu:	.long	0				/* are we 386, 386sx, or 486 */
_cold:	.long	1				/* cold till we are not */
_atdevbase:	.long	0			/* location of start of iomem in virtual */
_atdevphys:	.long	0			/* location of device mapping ptes (phys) */

	.globl	_KERNend
_KERNend:	.long	0			/* phys addr end of kernel (just after bss) */

	.globl	_IdlePTD,_KPTphys
_IdlePTD:	.long	0			/* phys addr of kernel PTD */
_KPTphys:	.long	0			/* phys addr of kernel page tables */

	.globl	_cyloffset
_cyloffset:	.long	0			/* cylinder offset from boot blocks */

	.globl	_proc0paddr
_proc0paddr:	.long	0			/* address of proc 0 address space */

#ifdef BDE_DEBUGGER
	.globl	_bdb_exists			/* flag to indicate BDE debugger is available */
_bde_exists:	.long	0
#endif

	.globl	tmpstk
	.space	0x1000
tmpstk:


/*
 * System Initialization
 */
	.text

/*
 * btext: beginning of text section.
 * Also the entry point (jumped to directly from the boot blocks).
 */
ENTRY(btext)
	movw	$0x1234,0x472			/* warm boot */
	jmp	1f
	.space	0x500				/* skip over warm boot shit */

	/*
	 * pass parameters on stack (howto, bootdev, unit, cyloffset, esym)
	 * note: (%esp) is return address of boot
	 * ( if we want to hold onto /boot, it's physical %esp up to _end)
	 */

 1:	movl	4(%esp),%eax
	movl	%eax,_boothowto-KERNBASE
	movl	8(%esp),%eax
	movl	%eax,_bootdev-KERNBASE
	movl	12(%esp),%eax
	movl	%eax,_cyloffset-KERNBASE
	movl	16(%esp),%eax
	addl	$KERNBASE,%eax
	movl	%eax,_esym-KERNBASE
#ifdef DISKLESS					/* Copy diskless structure */
	movl	_nfs_diskless_size-KERNBASE,%ecx
	movl	20(%esp),%esi
	movl	$(_nfs_diskless-KERNBASE),%edi
	rep
	movsb
#endif

	/* find out our CPU type. */
        pushfl
        popl    %eax
        movl    %eax,%ecx
        xorl    $0x40000,%eax
        pushl   %eax
        popfl
        pushfl
        popl    %eax
        xorl    %ecx,%eax
        shrl    $18,%eax
        andl    $1,%eax
        push    %ecx
        popfl
      
        cmpl    $0,%eax
        jne     1f
        movl    $CPU_386,_cpu-KERNBASE
	jmp	2f
1:      movl    $CPU_486,_cpu-KERNBASE
2:

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
	movl	%ecx,%esi			/* esi=start of tables */
	movl	%ecx,_KERNend-KERNBASE		/* save end of kernel */

/* clear bss */
	movl	$_edata-KERNBASE,%edi
	subl	%edi,%ecx			/* get amount to clear */
	xorl	%eax,%eax			/* specify zero fill */
	cld
	rep
	stosb

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
	movl	$PG_V|PG_KW,%eax		/* having these bits set, */
	lea	((1+UPAGES+1)*NBPG)(%esi),%ebx	/* phys addr of kernel PT base */
	movl	%ebx,_KPTphys-KERNBASE		/* save in global */
	fillkpt

#else /* !KGDB && !BDE_DEBUGGER */
	/* write protect kernel text (doesn't do a thing for 386's - only 486's) */
	movl	$_etext-KERNBASE,%ecx		/* get size of text */
	shrl	$PGSHIFT,%ecx			/* for this many PTEs */
	movl	$PG_V|PG_KR,%eax		/* specify read only */
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
#endif

/* now initialize the page dir, upages, p0stack PT, and page tables */

	movl	$(1+UPAGES+1+NKPT),%ecx	/* number of PTEs */
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
	rep					/* copy idt */
	movsl

	lgdt	(%esp)
	lidt	6(%esp)

	addl	$2*6,%esp
	popal
#endif

	/* load base of page directory and enable mapping */
	movl	%esi,%eax			/* phys address of ptd in proc 0 */
	orl	$I386_CR3PAT,%eax
	movl	%eax,%cr3			/* load ptd addr into mmu */
	movl	%cr0,%eax			/* get control word */
/*
 * XXX it is now safe to always (attempt to) set CR0_WP and to set up
 * the page tables assuming it works, so USE_486_WRITE_PROTECT will go
 * away.  The special 386 PTE checking needs to be conditional on
 * whatever distingiushes 486-only kernels from 386-486 kernels.
 */
#ifdef USE_486_WRITE_PROTECT
	orl	$CR0_PE|CR0_PG|CR0_WP,%eax	/* enable paging */
#else
	orl	$CR0_PE|CR0_PG,%eax		/* enable paging */
#endif
	movl	%eax,%cr0			/* and let's page NOW! */

	pushl	$begin				/* jump to high mem */
	ret

begin: /* now running relocated at KERNBASE where the system is linked to run */

	.globl _Crtat				/* XXX - locore should not know about */
	movl	_Crtat,%eax			/* variables of device drivers (pccons)! */
	subl	$(KERNBASE+0xA0000),%eax
	movl	_atdevphys,%edx			/* get pte PA */
	subl	_KPTphys,%edx			/* remove base of ptes, now have phys offset */
	shll	$PGSHIFT-2,%edx			/* corresponding to virt offset */
	addl	$KERNBASE,%edx			/* add virtual base */
	movl	%edx,_atdevbase
	addl	%eax,%edx
	movl	%edx,_Crtat

	/* set up bootstrap stack - 48 bytes */
	movl	$_kstack+UPAGES*NBPG-4*12,%esp	/* bootstrap stack end location */
	xorl	%eax,%eax			/* mark end of frames */
	movl	%eax,%ebp
	movl	_proc0paddr,%eax
	movl	%esi,PCB_CR3(%eax)

#ifdef BDE_DEBUGGER
	/* relocate debugger gdt entries */

	movl	$_gdt+8*9,%eax			/* adjust slots 9-17 */
	movl	$9,%ecx
reloc_gdt:
	movb	$0xfe,7(%eax)			/* top byte of base addresses, was 0, */
	addl	$8,%eax				/* now KERNBASE>>24 */
	loop	reloc_gdt

	cmpl	$0,_bdb_exists
	je	1f
	int	$3
1:
#endif

	/*
	 * Skip over the page tables and the kernel stack
	 */
	lea	((1+UPAGES+1+NKPT)*NBPG)(%esi),%esi

	pushl	%esi				/* value of first for init386(first) */
	call	_init386			/* wire 386 chip for unix operation */

	movl	$0,_PTD
	call	_main				/* autoconfiguration, mountroot etc */
	popl	%esi

	/*
	 * now we've run main() and determined what cpu-type we are, we can
	 * enable WP mode on i486 cpus and above.
	 * on return from main(), we are process 1
	 * set up address space and stack so that we can 'return' to user mode
	 */

	.globl	__ucodesel,__udatasel
	movl	__ucodesel,%eax
	movl	__udatasel,%ecx
	/* build outer stack frame */
	pushl	%ecx				/* user ss */
	pushl	$USRSTACK			/* user esp */
	pushl	%eax				/* user cs */
	pushl	$0				/* user ip */
	movl	%cx,%ds
	movl	%cx,%es
	movl	%ax,%fs				/* double map cs to fs */
	movl	%cx,%gs				/* and ds to gs */
	lret					/* goto user! */

	pushl	$lretmsg1			/* "should never get here!" */
	call	_panic
lretmsg1:
	.asciz	"lret: toinit\n"


#define	LCALL(x,y)	.byte 0x9a ; .long y; .word x
/*
 * Icode is copied out to process 1 and executed in user mode:
 *	execve("/sbin/init", argv, envp); exit(0);
 * If the execve fails, process 1 exits and the system panics.
 */
NON_GPROF_ENTRY(icode)
	pushl	$0				/* envp for execve() */

#	pushl	$argv-_icode			/* can't do this 'cos gas 1.38 is broken */
	movl	$argv,%eax
	subl	$_icode,%eax
	pushl	%eax				/* argp for execve() */

#	pushl	$init-_icode
	movl	$init,%eax
	subl	$_icode,%eax
	pushl	%eax				/* fname for execve() */

	pushl	%eax				/* dummy return address */

	movl	$SYS_execve,%eax
	LCALL(0x7,0x0)

	/* exit if something botches up in the above execve() */
	pushl	%eax				/* execve failed, the errno will do for an */
						/* exit code because errnos are < 128 */
	pushl	%eax				/* dummy return address */
	movl	$SYS_exit,%eax
	LCALL(0x7,0x0)

init:
	.asciz	"/sbin/init"
	ALIGN_DATA
argv:
	.long	init+6-_icode			/* argv[0] = "init" ("/sbin/init" + 6) */
	.long	eicode-_icode			/* argv[1] follows icode after copyout */
	.long	0
eicode:

	.globl	_szicode
_szicode:
	.long	_szicode-_icode

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

