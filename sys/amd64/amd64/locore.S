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
 *	@(#)locore.s	7.3 (Berkeley) 5/13/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         5       00158
 * --------------------         -----   ----------------------
 *
 * 06 Aug 92	Pace Willisson		Allow VGA memory to be mapped
 * 28 Nov 92	Frank MacLachlan	Aligned addresses and data
 *					on 32bit boundaries.
 * 25 Mar 93	Kevin Lahey		Add syscall counter for vmstat
 * 20 Apr 93	Bruce Evans		New npx-0.5 code
 * 25 Apr 93	Bruce Evans		Support new interrupt code (intr-0.1)
 */


/*
 * locore.s:	4BSD machine support for the Intel 386
 *		Preliminary version
 *		Written by William F. Jolitz, 386BSD Project
 */

#include "assym.s"
#include "machine/psl.h"
#include "machine/pte.h"

#include "errno.h"

#include "machine/trap.h"

#include "machine/specialreg.h"
#include "i386/isa/debug.h"

#define	KDSEL		0x10
#define	SEL_RPL_MASK	0x0003
#define	TRAPF_CS_OFF	(13 * 4)

/*
 * Note: This version greatly munged to avoid various assembler errors
 * that may be fixed in newer versions of gas. Perhaps newer versions
 * will have more pleasant appearance.
 */

	.set	IDXSHIFT,10
	.set	SYSTEM,0xFE000000	# virtual address of system start
	/*note: gas copys sign bit (e.g. arithmetic >>), can't do SYSTEM>>22! */
	.set	SYSPDROFF,0x3F8		# Page dir index of System Base
	.set	SYSPDREND,0x3FA		# Page dir index of System End


/*
 * Macros
 */
#define	ALIGN_DATA	.align	2
#define	ALIGN_TEXT	.align	2,0x90	/* 4-byte boundaries, NOP-filled */
#define	SUPERALIGN_TEXT	.align	4,0x90	/* 16-byte boundaries better for 486 */

#define	GEN_ENTRY(name)		ALIGN_TEXT; .globl name; name:
#define	NON_GPROF_ENTRY(name)	GEN_ENTRY(_/**/name)

#ifdef GPROF
/*
 * ALTENTRY() must be before a corresponding ENTRY() so that it can jump
 * over the mcounting.
 */
#define	ALTENTRY(name)		GEN_ENTRY(_/**/name); MCOUNT; jmp 2f
#define	ENTRY(name)		GEN_ENTRY(_/**/name); MCOUNT; 2:
/*
 * The call to mcount supports the usual (bad) conventions.  We allocate
 * some data and pass a pointer to it although the 386BSD doesn't use
 * the data.  We set up a frame before calling mcount because that is
 * the standard convention although it makes work for both mcount and
 * callers.
 */
#define MCOUNT			.data; ALIGN_DATA; 1:; .long 0; .text; \
				pushl %ebp; movl %esp, %ebp; \
				movl $1b,%eax; call mcount; popl %ebp
#else
/*
 * ALTENTRY() has to align because it is before a corresponding ENTRY().
 * ENTRY() has to align to because there may be no ALTENTRY() before it.
 * If there is a previous ALTENTRY() then the alignment code is empty.
 */
#define	ALTENTRY(name)		GEN_ENTRY(_/**/name)
#define	ENTRY(name)		GEN_ENTRY(_/**/name)
#endif

/* NB: NOP now preserves registers so NOPs can be inserted anywhere */
/* XXX: NOP and FASTER_NOP are misleadingly named */
#ifdef BROKEN_HARDWARE_AND_OR_SOFTWARE /* XXX - rarely necessary */
#define	FASTER_NOP	pushl %eax ; inb $0x84,%al ; popl %eax
#define	NOP	pushl %eax ; inb $0x84,%al ; inb $0x84,%al ; popl %eax
#else
#define	FASTER_NOP
#define	NOP
#endif

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.set	PDRPDROFF,0x3F7		# Page dir index of Page dir
	.globl	_PTmap, _PTD, _PTDpde, _Sysmap
	.set	_PTmap,0xFDC00000
	.set	_PTD,0xFDFF7000
	.set	_Sysmap,0xFDFF8000
	.set	_PTDpde,0xFDFF7000+4*PDRPDROFF

/*
 * APTmap, APTD is the alternate recursive pagemap.
 * It's used when modifying another process's page tables.
 */
	.set	APDRPDROFF,0x3FE		# Page dir index of Page dir
	.globl	_APTmap, _APTD, _APTDpde
	.set	_APTmap,0xFF800000
	.set	_APTD,0xFFBFE000
	.set	_APTDpde,0xFDFF7000+4*APDRPDROFF

/*
 * Access to each processes kernel stack is via a region of
 * per-process address space (at the beginning), immediatly above
 * the user process stack.
 */
	.globl	_kstack
	.set	_kstack, USRSTACK
	.set	PPDROFF,0x3F6
	.set	PPTEOFF,0x400-UPAGES	# 0x3FE


/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

	.data
	.globl	_boothowto, _bootdev, _curpcb
	.globl	__ucodesel,__udatasel

	.globl	_cpu, _cold, _atdevbase
_cpu:	.long	0		# are we 386, 386sx, or 486
_cold:	.long	1		# cold till we are not
_atdevbase:	.long	0	# location of start of iomem in virtual
	# .nonglobl _atdevphys (should be register or something)
_atdevphys:	.long	0	# location of device mapping ptes (phys)

	.globl	_IdlePTD, _KPTphys
_IdlePTD:	.long	0
_KPTphys:	.long	0

	.globl	_cyloffset, _proc0paddr
_cyloffset:	.long	0
_proc0paddr:	.long	0

#ifdef SHOW_A_LOT
bit_colors:
	.byte	GREEN,RED,0,0
#endif

	.space 512
tmpstk:


	.text
/*****************************************************************************/
/* System Initialisation                                                     */
/*****************************************************************************/

/*
 * btext: beginning of text section.
 * Also the entry point (jumped to directly from the boot blocks).
 */
ENTRY(btext)
	movw	$0x1234, 0x472	# warm boot
	jmp	1f
	.space	0x500		# skip over warm boot shit

	/*
	 * pass parameters on stack (howto, bootdev, unit, cyloffset)
	 * note: (%esp) is return address of boot
	 * ( if we want to hold onto /boot, it's physical %esp up to _end)
	 */

 1:	movl	4(%esp),%eax
	movl	%eax,_boothowto-SYSTEM
	movl	8(%esp),%eax
	movl	%eax,_bootdev-SYSTEM
	movl	12(%esp),%eax
	movl	%eax, _cyloffset-SYSTEM

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
	movl	$ tmpstk-SYSTEM,%esp	# bootstrap stack end location

#ifdef garbage
	/* count up memory */

	xorl	%eax,%eax		# start with base memory at 0x0
	#movl	$ 0xA0000/NBPG,%ecx	# look every 4K up to 640K
	movl	$ 0xA0,%ecx		# look every 4K up to 640K
1:	movl	(%eax),%ebx		# save location to check
	movl	$0xa55a5aa5,(%eax)	# write test pattern
	/* flush stupid cache here! (with bcopy (0,0,512*1024) ) */
	cmpl	$0xa55a5aa5,(%eax)	# does not check yet for rollover
	jne	2f
	movl	%ebx,(%eax)		# restore memory
	addl	$ NBPG,%eax
	loop	1b
2:	shrl	$12,%eax
	movl	%eax,_Maxmem-SYSTEM

	movl	$0x100000,%eax		# next, talley remaining memory
	#movl	$((0xFFF000-0x100000)/NBPG),%ecx
	movl	$(0xFFF-0x100),%ecx
1:	movl	(%eax),%ebx		# save location to check
	movl	$0xa55a5aa5,(%eax)	# write test pattern
	cmpl	$0xa55a5aa5,(%eax)	# does not check yet for rollover
	jne	2f
	movl	%ebx,(%eax)		# restore memory
	addl	$ NBPG,%eax
	loop	1b
2:	shrl	$12,%eax
	movl	%eax,_Maxmem-SYSTEM
#endif

/* find end of kernel image */
	movl	$_end-SYSTEM,%ecx
	addl	$ NBPG-1,%ecx
	andl	$~(NBPG-1),%ecx
	movl	%ecx,%esi

/* clear bss and memory for bootstrap pagetables. */
	movl	$_edata-SYSTEM,%edi
	subl	%edi,%ecx
	addl	$(UPAGES+5)*NBPG,%ecx
/*
 * Virtual address space of kernel:
 *
 *	text | data | bss | page dir | proc0 kernel stack | usr stk map | Sysmap
 *			     0               1       2       3             4
 */
	xorl	%eax,%eax	# pattern
	cld
	rep
	stosb

	movl	%esi,_IdlePTD-SYSTEM /*physical address of Idle Address space */

#define	fillkpt		\
1:	movl	%eax,(%ebx)	; \
	addl	$ NBPG,%eax	; /* increment physical address */ \
	addl	$4,%ebx		; /* next pte */ \
	loop	1b		;

/*
 * Map Kernel
 * N.B. don't bother with making kernel text RO, as 386
 * ignores R/W AND U/S bits on kernel access (only v works) !
 *
 * First step - build page tables
 */
	movl	%esi,%ecx		# this much memory,
	shrl	$ PGSHIFT,%ecx		# for this many pte s
	addl	$ UPAGES+4,%ecx		# including our early context
	cmpl	$0xa0,%ecx		# XXX - cover debugger pages
	jae	1f
	movl	$0xa0,%ecx
1:
	movl	$PG_V|PG_KW,%eax	#  having these bits set,
	lea	(4*NBPG)(%esi),%ebx	#   physical address of KPT in proc 0,
	movl	%ebx,_KPTphys-SYSTEM	#    in the kernel page table,
	fillkpt

/* map I/O memory map */

	movl	$0x100-0xa0,%ecx	# for this many pte s,
	movl	$(0xa0000|PG_V|PG_UW),%eax # having these bits set,(perhaps URW?) XXX 06 Aug 92
	movl	%ebx,_atdevphys-SYSTEM	#   remember phys addr of ptes
	fillkpt

 /* map proc 0's kernel stack into user page table page */

	movl	$ UPAGES,%ecx		# for this many pte s,
	lea	(1*NBPG)(%esi),%eax	# physical address in proc 0
	lea	(SYSTEM)(%eax),%edx
	movl	%edx,_proc0paddr-SYSTEM  # remember VA for 0th process init
	orl	$PG_V|PG_KW,%eax	#  having these bits set,
	lea	(3*NBPG)(%esi),%ebx	# physical address of stack pt in proc 0
	addl	$(PPTEOFF*4),%ebx
	fillkpt

/*
 * Construct a page table directory
 * (of page directory elements - pde's)
 */
	/* install a pde for temporary double map of bottom of VA */
	lea	(4*NBPG)(%esi),%eax	# physical address of kernel page table
	orl     $ PG_V|PG_UW,%eax	# pde entry is valid XXX 06 Aug 92
	movl	%eax,(%esi)		# which is where temp maps!

	/* kernel pde's */
	movl	$(SYSPDREND-SYSPDROFF+1), %ecx		# for this many pde s,
	lea	(SYSPDROFF*4)(%esi), %ebx		# offset of pde for kernel
	fillkpt

	/* install a pde recursively mapping page directory as a page table! */
	movl	%esi,%eax		# phys address of ptd in proc 0
	orl	$ PG_V|PG_UW,%eax	# pde entry is valid XXX 06 Aug 92
	movl	%eax, PDRPDROFF*4(%esi)	# which is where PTmap maps!

	/* install a pde to map kernel stack for proc 0 */
	lea	(3*NBPG)(%esi),%eax	# physical address of pt in proc 0
	orl	$PG_V|PG_KW,%eax	# pde entry is valid
	movl	%eax,PPDROFF*4(%esi)	# which is where kernel stack maps!

	/* copy and convert stuff from old gdt and idt for debugger */

	cmpl	$0x0375c339,0x96104	# XXX - debugger signature
	jne	1f
	movb	$1,_bdb_exists-SYSTEM
1:
	pushal
	subl	$2*6,%esp

	sgdt	(%esp)
	movl	2(%esp),%esi		# base address of current gdt
	movl	$_gdt-SYSTEM,%edi
	movl	%edi,2(%esp)
	movl	$8*18/4,%ecx
	rep				# copy gdt
	movsl
	movl	$_gdt-SYSTEM,-8+2(%edi)	# adjust gdt self-ptr
	movb	$0x92,-8+5(%edi)

	sidt	6(%esp)
	movl	6+2(%esp),%esi		# base address of current idt
	movl	8+4(%esi),%eax		# convert dbg descriptor to ...
	movw	8(%esi),%ax
	movl	%eax,bdb_dbg_ljmp+1-SYSTEM	# ... immediate offset ...
	movl	8+2(%esi),%eax
	movw	%ax,bdb_dbg_ljmp+5-SYSTEM	# ... and selector for ljmp
	movl	24+4(%esi),%eax		# same for bpt descriptor
	movw	24(%esi),%ax
	movl	%eax,bdb_bpt_ljmp+1-SYSTEM
	movl	24+2(%esi),%eax
	movw	%ax,bdb_bpt_ljmp+5-SYSTEM

	movl	$_idt-SYSTEM,%edi
	movl	%edi,6+2(%esp)
	movl	$8*4/4,%ecx
	rep				# copy idt
	movsl

	lgdt	(%esp)
	lidt	6(%esp)

	addl	$2*6,%esp
	popal

	/* load base of page directory, and enable mapping */
	movl	%esi,%eax		# phys address of ptd in proc 0
	orl	$ I386_CR3PAT,%eax
	movl	%eax,%cr3		# load ptd addr into mmu
	movl	%cr0,%eax		# get control word
/*
 * XXX it is now safe to always (attempt to) set CR0_WP and to set up
 * the page tables assuming it works, so USE_486_WRITE_PROTECT will go
 * away.  The special 386 PTE checking needs to be conditional on
 * whatever distingiushes 486-only kernels from 386-486 kernels.
 */
#ifdef USE_486_WRITE_PROTECT
	orl	$CR0_PE|CR0_PG|CR0_WP,%eax	# and let s page!
#else
	orl	$CR0_PE|CR0_PG,%eax	# and let s page!
#endif
	movl	%eax,%cr0		# NOW!

	pushl	$begin				# jump to high mem!
	ret

begin: /* now running relocated at SYSTEM where the system is linked to run */

	.globl _Crtat			# XXX - locore should not know about
	movl	_Crtat,%eax		# variables of device drivers (pccons)!
	subl	$0xfe0a0000,%eax
	movl	_atdevphys,%edx	# get pte PA
	subl	_KPTphys,%edx	# remove base of ptes, now have phys offset
	shll	$ PGSHIFT-2,%edx  # corresponding to virt offset
	addl	$ SYSTEM,%edx	# add virtual base
	movl	%edx, _atdevbase
	addl	%eax,%edx
	movl	%edx,_Crtat

	/* set up bootstrap stack */
	movl	$ _kstack+UPAGES*NBPG-4*12,%esp	# bootstrap stack end location
	xorl	%eax,%eax		# mark end of frames
	movl	%eax,%ebp
	movl	_proc0paddr, %eax
	movl	%esi, PCB_CR3(%eax)

	lea	7*NBPG(%esi),%esi	# skip past stack.
	pushl	%esi

	/* relocate debugger gdt entries */

	movl	$_gdt+8*9,%eax		# adjust slots 9-17
	movl	$9,%ecx
reloc_gdt:
	movb	$0xfe,7(%eax)		# top byte of base addresses, was 0,
	addl	$8,%eax			# now SYSTEM>>24
	loop	reloc_gdt

	cmpl	$0,_bdb_exists
	je	1f
	int	$3
1:

	call	_init386		# wire 386 chip for unix operation

	movl	$0,_PTD
	call	_main			# autoconfiguration, mountroot etc
	popl	%esi

	/*
	 * on return from main(), we are process 1
	 * set up address space and stack so that we can 'return' to user mode
	 */

	movl	__ucodesel,%eax
	movl	__udatasel,%ecx
	# build outer stack frame
	pushl	%ecx		# user ss
	pushl	$ USRSTACK	# user esp
	pushl	%eax		# user cs
	pushl	$0		# user ip
	movl	%cx,%ds
	movl	%cx,%es
	movl	%ax,%fs		# double map cs to fs
	movl	%cx,%gs		# and ds to gs
	lret	# goto user!

	pushl	$lretmsg1	/* "should never get here!" */
	call	_panic
lretmsg1:
	.asciz	"lret: toinit\n"


	.set	exec,59
	.set	exit,1

#define	LCALL(x,y)	.byte 0x9a ; .long y; .word x
/*
 * Icode is copied out to process 1 and executed in user mode:
 *	execve("/sbin/init", argv, envp); exit(0);
 * If the execve fails, process 1 exits and the system panics.
 */
NON_GPROF_ENTRY(icode)
	pushl	$0		# envp

	# pushl	$argv-_icode	# gas fucks up again
	movl	$argv,%eax
	subl	$_icode,%eax
	pushl	%eax

	# pushl	$init-_icode
	movl	$init,%eax
	subl	$_icode,%eax
	pushl	%eax

	pushl	%eax		# junk to fake return address

	movl	$exec,%eax
	LCALL(0x7,0x0)

	pushl	%eax		# execve failed, the errno will do for an
				# exit code because errnos are < 128
	pushl	%eax		# junk to fake return address

	movl	$exit,%eax
	LCALL(0x7,0x0)

init:
	.asciz	"/sbin/init"
	ALIGN_DATA
argv:
	.long	init+6-_icode		# argv[0] = "init" ("/sbin/init" + 6)
	.long	eicode-_icode		# argv[1] follows icode after copyout
	.long	0
eicode:

	.globl	_szicode
_szicode:
	.long	_szicode-_icode

NON_GPROF_ENTRY(sigcode)
	call	12(%esp)
	lea	28(%esp),%eax	# scp (the call may have clobbered the
				# copy at 8(%esp))
				# XXX - use genassym
	pushl	%eax
	pushl	%eax		# junk to fake return address
	movl	$103,%eax	# sigreturn()
	LCALL(0x7,0)		# enter kernel with args on stack
	hlt			# never gets here

	.globl	_szsigcode
_szsigcode:
	.long	_szsigcode-_sigcode


/*****************************************************************************/
/* support routines for GCC, general C-callable functions                    */
/*****************************************************************************/

ENTRY(__udivsi3)
	movl 4(%esp),%eax
	xorl %edx,%edx
	divl 8(%esp)
	ret

ENTRY(__divsi3)
	movl 4(%esp),%eax
	cltd
	idivl 8(%esp)
	ret


	/*
	 * I/O bus instructions via C
	 */
ENTRY(outb)				# outb (port, val)
	movl	4(%esp),%edx
	NOP
	movl	8(%esp),%eax
	outb	%al,%dx
	NOP
	ret


ENTRY(outw)				# outw (port, val)
	movl	4(%esp),%edx
	NOP
	movl	8(%esp),%eax
	outw	%ax,%dx
	NOP
	ret


ENTRY(outsb)			# outsb(port,addr,cnt)
	pushl	%esi
	movw	8(%esp),%dx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	outsb
	NOP
	movl	%esi,%eax
	popl	%esi
	ret


ENTRY(outsw)			# outsw(port,addr,cnt)
	pushl	%esi
	movw	8(%esp),%dx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	outsw
	NOP
	movl	%esi,%eax
	popl	%esi
	ret


ENTRY(inb)			# val = inb (port)
	movl	4(%esp),%edx
	subl	%eax,%eax	# clr eax
	NOP
	inb	%dx,%al
	ret


ENTRY(inw)			# val = inw (port)
	movl	4(%esp),%edx
	subl	%eax,%eax	# clr eax
	NOP
	inw	%dx,%ax
	ret


ENTRY(insb)			# insb(port,addr,cnt)
	pushl	%edi
	movw	8(%esp),%dx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	insb
	NOP
	movl	%edi,%eax
	popl	%edi
	ret


ENTRY(insw)			# insw(port,addr,cnt)
	pushl	%edi
	movw	8(%esp),%dx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	insw
	NOP
	movl	%edi,%eax
	popl	%edi
	ret


ENTRY(rtcin)
	movl	4(%esp),%eax
	outb	%al,$0x70
	subl	%eax,%eax	# clr eax
	inb	$0x71,%al
	ret


	/*
	 * bcopy family
	 */
ENTRY(bzero)			# void bzero(void *base, u_int cnt)
	pushl	%edi
	movl	8(%esp),%edi
	movl	12(%esp),%ecx
	xorl	%eax,%eax
	shrl	$2,%ecx
	cld
	rep
	stosl
	movl	12(%esp),%ecx
	andl	$3,%ecx
	rep
	stosb
	popl	%edi
	ret


ENTRY(fillw)			# fillw (pat,base,cnt)
	pushl	%edi
	movl	8(%esp),%eax
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	rep
	stosw
	popl	%edi
	ret

ENTRY(bcopyb)
bcopyb:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi	/* potentially overlapping? */
	jnb	1f
	cld			/* nope, copy forwards */
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi	/* copy backwards. */
	addl	%ecx,%esi
	std
	decl	%edi
	decl	%esi
	rep
	movsb
	popl	%edi
	popl	%esi
	cld
	ret

ENTRY(bcopyw)
bcopyw:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi	/* potentially overlapping? */
	jnb	1f
	cld			/* nope, copy forwards */
	shrl	$1,%ecx		/* copy by 16-bit words */
	rep
	movsw
	adc	%ecx,%ecx	/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi	/* copy backwards */
	addl	%ecx,%esi
	std
	andl	$1,%ecx		/* any fractional bytes? */
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx	/* copy remainder by 16-bit words */
	shrl	$1,%ecx
	decl	%esi
	decl	%edi
	rep
	movsw
	popl	%edi
	popl	%esi
	cld
	ret

ENTRY(bcopyx)
	movl	16(%esp),%eax
	cmpl	$2,%eax
	je	bcopyw		/* not _bcopyw, to avoid multiple mcounts */
	cmpl	$4,%eax
	je	bcopy
	jmp	bcopyb

	/*
	 * (ov)bcopy (src,dst,cnt)
	 *  ws@tools.de     (Wolfgang Solfrank, TooLs GmbH) +49-228-985800
	 */
ALTENTRY(ovbcopy)
ENTRY(bcopy)
bcopy:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi	/* potentially overlapping? */
	jnb	1f
	cld			/* nope, copy forwards */
	shrl	$2,%ecx		/* copy by 32-bit words */
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx		/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi	/* copy backwards */
	addl	%ecx,%esi
	std
	andl	$3,%ecx		/* any fractional bytes? */
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx	/* copy remainder by 32-bit words */
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	cld
	ret

ALTENTRY(ntohl)
ENTRY(htonl)
	movl	4(%esp),%eax
#ifdef i486
	/* XXX */
	/* Since Gas 1.38 does not grok bswap this has been coded as the
	 * equivalent bytes.  This can be changed back to bswap when we
	 * upgrade to a newer version of Gas */
	/* bswap	%eax */
	.byte	0x0f
	.byte	0xc8
#else
	xchgb	%al,%ah
	roll	$16,%eax
	xchgb	%al,%ah
#endif
	ret

ALTENTRY(ntohs)
ENTRY(htons)
	movzwl	4(%esp),%eax
	xchgb	%al,%ah
	ret


#ifdef SHOW_A_LOT
/*
 * 'show_bits' was too big when defined as a macro.  The line length for some
 * enclosing macro was too big for gas.  Perhaps the code would have blown
 * the cache anyway.
 */
	ALIGN_TEXT
show_bits:
	pushl	%eax
	SHOW_BIT(0)
	SHOW_BIT(1)
	SHOW_BIT(2)
	SHOW_BIT(3)
	SHOW_BIT(4)
	SHOW_BIT(5)
	SHOW_BIT(6)
	SHOW_BIT(7)
	SHOW_BIT(8)
	SHOW_BIT(9)
	SHOW_BIT(10)
	SHOW_BIT(11)
	SHOW_BIT(12)
	SHOW_BIT(13)
	SHOW_BIT(14)
	SHOW_BIT(15)
	popl	%eax
	ret
#endif /* SHOW_A_LOT */


/*****************************************************************************/
/* copyout and fubyte family                                                 */
/*****************************************************************************/
/*
 * Access user memory from inside the kernel. These routines and possibly
 * the math- and DOS emulators should be the only places that do this.
 *
 * We have to access the memory with user's permissions, so use a segment
 * selector with RPL 3. For writes to user space we have to additionally
 * check the PTE for write permission, because the 386 does not check
 * write permissions when we are executing with EPL 0. The 486 does check
 * this if the WP bit is set in CR0, so we can use a simpler version here.
 *
 * These routines set curpcb->onfault for the time they execute. When a
 * protection violation occurs inside the functions, the trap handler
 * returns to *curpcb->onfault instead of the function.
 */


ENTRY(copyout)			# copyout (from_kernel, to_user, len)
	movl	_curpcb, %eax
	movl	$copyout_fault, PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	16(%esp), %esi
	movl	20(%esp), %edi
	movl	24(%esp), %ebx
	orl	%ebx, %ebx	# nothing to do?
	jz	done_copyout

	/*
	 * Check explicitly for non-user addresses.  If 486 write protection
	 * is being used, this check is essential because we are in kernel
	 * mode so the h/w does not provide any protection against writing
	 * kernel addresses.
	 *
	 * Otherwise, it saves having to load and restore %es to get the
	 * usual segment-based protection (the destination segment for movs
	 * is always %es).  The other explicit checks for user-writablility
	 * are not quite sufficient.  They fail for the user area because
	 * we mapped the user area read/write to avoid having an #ifdef in
	 * vm_machdep.c.  They fail for user PTEs and/or PTDs!  (107
	 * addresses including 0xff800000 and 0xfc000000).  I'm not sure if
	 * this can be fixed.  Marking the PTEs supervisor mode and the
	 * PDE's user mode would almost work, but there may be a problem
	 * with the self-referential PDE.
	 */
	movl	%edi, %eax
	addl	%ebx, %eax
	jc	copyout_fault
#define VM_END_USER_ADDRESS	0xFDBFE000	/* XXX */
	cmpl	$VM_END_USER_ADDRESS, %eax
	ja	copyout_fault

#ifndef USE_486_WRITE_PROTECT
	/*
	 * We have to check each PTE for user write permission.
	 * The checking may cause a page fault, so it is important to set
	 * up everything for return via copyout_fault before here.
	 */
			/* compute number of pages */
	movl	%edi, %ecx
	andl	$0x0fff, %ecx
	addl	%ebx, %ecx
	decl	%ecx
	shrl	$IDXSHIFT+2, %ecx
	incl	%ecx

			/* compute PTE offset for start address */
	movl	%edi, %edx
	shrl	$IDXSHIFT, %edx
	andb	$0xfc, %dl

1:			/* check PTE for each page */
	movb	_PTmap(%edx), %al
	andb	$0x07, %al	/* Pages must be VALID + USERACC + WRITABLE */
	cmpb	$0x07, %al
	je	2f

				/* simulate a trap */
	pushl	%edx
	pushl	%ecx
	shll	$IDXSHIFT, %edx
	pushl	%edx
	call	_trapwrite	/* XXX trapwrite(addr) */
	popl	%edx
	popl	%ecx
	popl	%edx

	orl	%eax, %eax	/* if not ok, return EFAULT */
	jnz	copyout_fault

2:
	addl	$4, %edx
	decl	%ecx
	jnz	1b		/* check next page */
#endif /* ndef USE_486_WRITE_PROTECT */

			/* now copy it over */
			/* bcopy (%esi, %edi, %ebx) */
	cld
	movl	%ebx, %ecx
	shrl	$2, %ecx
	rep
	movsl
	movb	%bl, %cl
	andb	$3, %cl
	rep
	movsb

done_copyout:
	popl	%ebx
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	movl	_curpcb,%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret

	ALIGN_TEXT
copyout_fault:
	popl	%ebx
	popl	%edi
	popl	%esi
	movl	_curpcb, %edx
	movl	$0, PCB_ONFAULT(%edx)
	movl	$EFAULT, %eax
	ret


ENTRY(copyin)			# copyin (from_user, to_kernel, len)
	movl	_curpcb,%eax
	movl	$copyin_fault, PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi		# caddr_t from
	movl	16(%esp),%edi		# caddr_t to
	movl	20(%esp),%ecx		# size_t  len

	movb	%cl,%al
	shrl	$2,%ecx			# copy longword-wise
	cld
	gs
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl			# copy remaining bytes
	gs
	rep
	movsb

	popl	%edi
	popl	%esi
	xorl	%eax, %eax
	movl	_curpcb, %edx
	movl	%eax, PCB_ONFAULT(%edx)
	ret

	ALIGN_TEXT
copyin_fault:
	popl	%edi
	popl	%esi
	movl	_curpcb, %edx
	movl	$0, PCB_ONFAULT(%edx)
	movl	$EFAULT, %eax
	ret

	/*
	 * fu{byte,sword,word} : fetch a byte (sword, word) from user memory
	 */
ALTENTRY(fuiword)
ENTRY(fuword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

ENTRY(fusword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(fuibyte)
ENTRY(fubyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
	movzbl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

	ALIGN_TEXT
fusufault:
	movl	_curpcb,%ecx
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	decl	%eax
	ret

	/*
	 * su{byte,sword,word}: write a byte (word, longword) to user memory
	 */
#ifdef USE_486_WRITE_PROTECT
	/*
	 * we only have to set the right segment selector.
	 */
ALTENTRY(suiword)
ENTRY(suword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	gs
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(susword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movw	8(%esp),%ax
	gs
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(suibyte)
ENTRY(subyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movb	8(%esp),%al
	gs
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret


#else /* USE_486_WRITE_PROTECT */
	/*
	 * here starts the trouble again: check PTE, twice if word crosses
	 * a page boundary.
	 */
	# XXX - page boundary crossing is not handled yet

ALTENTRY(suibyte)
ENTRY(subyte)
	movl	_curpcb, %ecx
	movl	$fusufault, PCB_ONFAULT(%ecx)
	movl	4(%esp), %edx
	movl	%edx, %eax
	shrl	$IDXSHIFT, %edx
	andb	$0xfc, %dl
	movb	_PTmap(%edx), %dl
	andb	$0x7, %dl		/* must be VALID + USERACC + WRITE */
	cmpb	$0x7, %dl
	je	1f
					/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx
	orl	%eax, %eax
	jnz	fusufault
1:
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	gs
	movb	%al, (%edx)
	xorl	%eax, %eax
	movl	_curpcb, %ecx
	movl	%eax, PCB_ONFAULT(%ecx)
	ret

ENTRY(susword)
	movl	_curpcb, %ecx
	movl	$fusufault, PCB_ONFAULT(%ecx)
	movl	4(%esp), %edx
	movl	%edx, %eax
	shrl	$IDXSHIFT, %edx
	andb	$0xfc, %dl
	movb	_PTmap(%edx), %dl
	andb	$0x7, %dl		/* must be VALID + USERACC + WRITE */
	cmpb	$0x7, %dl
	je	1f
					/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx
	orl	%eax, %eax
	jnz	fusufault
1:
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	gs
	movw	%ax, (%edx)
	xorl	%eax, %eax
	movl	_curpcb, %ecx
	movl	%eax, PCB_ONFAULT(%ecx)
	ret

ALTENTRY(suiword)
ENTRY(suword)
	movl	_curpcb, %ecx
	movl	$fusufault, PCB_ONFAULT(%ecx)
	movl	4(%esp), %edx
	movl	%edx, %eax
	shrl	$IDXSHIFT, %edx
	andb	$0xfc, %dl
	movb	_PTmap(%edx), %dl
	andb	$0x7, %dl		/* must be VALID + USERACC + WRITE */
	cmpb	$0x7, %dl
	je	1f
					/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx
	orl	%eax, %eax
	jnz	fusufault
1:
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	gs
	movl	%eax, 0(%edx)
	xorl	%eax, %eax
	movl	_curpcb, %ecx
	movl	%eax, PCB_ONFAULT(%ecx)
	ret

#endif /* USE_486_WRITE_PROTECT */

/*
 * copyoutstr(from, to, maxlen, int *lencopied)
 *	copy a string from from to to, stop when a 0 character is reached.
 *	return ENAMETOOLONG if string is longer than maxlen, and
 *	EFAULT on protection violations. If lencopied is non-zero,
 *	return the actual length in *lencopied.
 */
#ifdef USE_486_WRITE_PROTECT

ENTRY(copyoutstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb, %ecx
	movl	$cpystrflt, PCB_ONFAULT(%ecx)

	movl	12(%esp), %esi			# %esi = from
	movl	16(%esp), %edi			# %edi = to
	movl	20(%esp), %edx			# %edx = maxlen
	incl	%edx

1:
	decl	%edx
	jz	4f
	/*
	 * gs override doesn't work for stosb.  Use the same explicit check
	 * as in copyout().  It's much slower now because it is per-char.
	 * XXX - however, it would be faster to rewrite this function to use
	 * strlen() and copyout().
	 */
	cmpl	$VM_END_USER_ADDRESS, %edi
	jae	cpystrflt
	lodsb
	gs
	stosb
	orb	%al,%al
	jnz	1b
			/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax, %eax
	jmp	6f
4:
			/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG, %eax
	jmp	6f

#else	/* ndef USE_486_WRITE_PROTECT */

ENTRY(copyoutstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb, %ecx
	movl	$cpystrflt, PCB_ONFAULT(%ecx)

	movl	12(%esp), %esi			# %esi = from
	movl	16(%esp), %edi			# %edi = to
	movl	20(%esp), %edx			# %edx = maxlen
1:
	/*
	 * It suffices to check that the first byte is in user space, because
	 * we look at a page at a time and the end address is on a page
	 * boundary.
	 */
	cmpl	$VM_END_USER_ADDRESS, %edi
	jae	cpystrflt
	movl	%edi, %eax
	shrl	$IDXSHIFT, %eax
	andb	$0xfc, %al
	movb	_PTmap(%eax), %al
	andb	$7, %al
	cmpb	$7, %al
	je	2f

			/* simulate trap */
	pushl	%edx
	pushl	%edi
	call	_trapwrite
	popl	%edi
	popl	%edx
	orl	%eax, %eax
	jnz	cpystrflt

2:			/* copy up to end of this page */
	movl	%edi, %eax
	andl	$0x0fff, %eax
	movl	$NBPG, %ecx
	subl	%eax, %ecx	/* ecx = NBPG - (src % NBPG) */
	cmpl	%ecx, %edx
	jge	3f
	movl	%edx, %ecx	/* ecx = min (ecx, edx) */
3:
	orl	%ecx, %ecx
	jz	4f
	decl	%ecx
	decl	%edx
	lodsb
	stosb
	orb	%al, %al
	jnz	3b

			/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax, %eax
	jmp	6f

4:			/* next page */
	orl	%edx, %edx
	jnz	1b
			/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG, %eax
	jmp	6f

#endif /* USE_486_WRITE_PROTECT */

/*
 * copyinstr(from, to, maxlen, int *lencopied)
 *	copy a string from from to to, stop when a 0 character is reached.
 *	return ENAMETOOLONG if string is longer than maxlen, and
 *	EFAULT on protection violations. If lencopied is non-zero,
 *	return the actual length in *lencopied.
 */
ENTRY(copyinstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb, %ecx
	movl	$cpystrflt, PCB_ONFAULT(%ecx)

	movl	12(%esp), %esi			# %esi = from
	movl	16(%esp), %edi			# %edi = to
	movl	20(%esp), %edx			# %edx = maxlen
	incl	%edx

1:
	decl	%edx
	jz	4f
	gs
	lodsb
	stosb
	orb	%al,%al
	jnz	1b
			/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax, %eax
	jmp	6f
4:
			/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG, %eax
	jmp	6f

cpystrflt:
	movl	$EFAULT, %eax
6:			/* set *lencopied and return %eax */
	movl	_curpcb, %ecx
	movl	$0, PCB_ONFAULT(%ecx)
	movl	20(%esp), %ecx
	subl	%edx, %ecx
	movl	24(%esp), %edx
	orl	%edx, %edx
	jz	7f
	movl	%ecx, (%edx)
7:
	popl	%edi
	popl	%esi
	ret


/*
 * copystr(from, to, maxlen, int *lencopied)
 */
ENTRY(copystr)
	pushl	%esi
	pushl	%edi

	movl	12(%esp), %esi			# %esi = from
	movl	16(%esp), %edi			# %edi = to
	movl	20(%esp), %edx			# %edx = maxlen
	incl	%edx

1:
	decl	%edx
	jz	4f
	lodsb
	stosb
	orb	%al,%al
	jnz	1b
			/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax, %eax
	jmp	6f
4:
			/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG, %eax

6:			/* set *lencopied and return %eax */
	movl	20(%esp), %ecx
	subl	%edx, %ecx
	movl	24(%esp), %edx
	orl	%edx, %edx
	jz	7f
	movl	%ecx, (%edx)
7:
	popl	%edi
	popl	%esi
	ret

/*****************************************************************************/
/* Handling of special 386 registers and descriptor tables etc               */
/*****************************************************************************/
	/*
	 * void lgdt(struct region_descriptor *rdp);
	 */
ENTRY(lgdt)
	/* reload the descriptor table */
	movl	4(%esp),%eax
	lgdt	(%eax)
	/* flush the prefetch q */
	jmp	1f
	nop
1:
	/* reload "stale" selectors */
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss

	/* reload code selector by turning return into intersegmental return */
	movl	(%esp),%eax
	pushl	%eax
	# movl	$KCSEL,4(%esp)
	movl	$8,4(%esp)
	lret

	/*
	 * void lidt(struct region_descriptor *rdp);
	 */
ENTRY(lidt)
	movl	4(%esp),%eax
	lidt	(%eax)
	ret

	/*
	 * void lldt(u_short sel)
	 */
ENTRY(lldt)
	lldt	4(%esp)
	ret

	/*
	 * void ltr(u_short sel)
	 */
ENTRY(ltr)
	ltr	4(%esp)
	ret

ENTRY(ssdtosd)				# ssdtosd(*ssdp,*sdp)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	8(%ecx),%ebx
	shll	$16,%ebx
	movl	(%ecx),%edx
	roll	$16,%edx
	movb	%dh,%bl
	movb	%dl,%bh
	rorl	$8,%ebx
	movl	4(%ecx),%eax
	movw	%ax,%dx
	andl	$0xf0000,%eax
	orl	%eax,%ebx
	movl	12(%esp),%ecx
	movl	%edx,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%ebx
	ret


ENTRY(tlbflush)				# tlbflush()
	movl	%cr3,%eax
	orl	$ I386_CR3PAT,%eax
	movl	%eax,%cr3
	ret


ENTRY(load_cr0)				# load_cr0(cr0)
	movl	4(%esp),%eax
	movl	%eax,%cr0
	ret


ENTRY(rcr0)				# rcr0()
	movl	%cr0,%eax
	ret


ENTRY(rcr2)				# rcr2()
	movl	%cr2,%eax
	ret


ENTRY(rcr3)				# rcr3()
	movl	%cr3,%eax
	ret


ENTRY(load_cr3)				# void load_cr3(caddr_t cr3)
	movl	4(%esp),%eax
	orl	$ I386_CR3PAT,%eax
	movl	%eax,%cr3
	ret


/*****************************************************************************/
/* setjump, longjump                                                         */
/*****************************************************************************/

ENTRY(setjmp)
	movl	4(%esp),%eax
	movl	%ebx,  (%eax)		# save ebx
	movl	%esp, 4(%eax)		# save esp
	movl	%ebp, 8(%eax)		# save ebp
	movl	%esi,12(%eax)		# save esi
	movl	%edi,16(%eax)		# save edi
	movl	(%esp),%edx		# get rta
	movl	%edx,20(%eax)		# save eip
	xorl	%eax,%eax		# return (0);
	ret

ENTRY(longjmp)
	movl	4(%esp),%eax
	movl	  (%eax),%ebx		# restore ebx
	movl	 4(%eax),%esp		# restore esp
	movl	 8(%eax),%ebp		# restore ebp
	movl	12(%eax),%esi		# restore esi
	movl	16(%eax),%edi		# restore edi
	movl	20(%eax),%edx		# get rta
	movl	%edx,(%esp)		# put in return frame
	xorl	%eax,%eax		# return (1);
	incl	%eax
	ret


/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrq puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_pri, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.comm	_noproc,4
	.comm	_runrun,4

/*
 * Setrq(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrq)
	movl	4(%esp),%eax
	cmpl	$0,P_RLINK(%eax)	# should not be on q already
	je	set1
	pushl	$set2
	call	_panic
set1:
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btsl	%edx,_whichqs		# set q full bit
	shll	$3,%edx
	addl	$_qs,%edx		# locate q hdr
	movl	%edx,P_LINK(%eax)	# link process on tail of q
	movl	P_RLINK(%edx),%ecx
	movl	%ecx,P_RLINK(%eax)
	movl	%eax,P_RLINK(%edx)
	movl	%eax,P_LINK(%ecx)
	ret

set2:	.asciz	"setrq"

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrq)
	movl	4(%esp),%eax
	movzbl	P_PRI(%eax),%edx
	shrl	$2,%edx
	btrl	%edx,_whichqs		# clear full bit, panic if clear already
	jb	rem1
	pushl	$rem3
	call	_panic
rem1:
	pushl	%edx
	movl	P_LINK(%eax),%ecx	# unlink process
	movl	P_RLINK(%eax),%edx
	movl	%edx,P_RLINK(%ecx)
	movl	P_RLINK(%eax),%ecx
	movl	P_LINK(%eax),%edx
	movl	%edx,P_LINK(%ecx)
	popl	%edx
	movl	$_qs,%ecx
	shll	$3,%edx
	addl	%edx,%ecx
	cmpl	P_LINK(%ecx),%ecx	# q still has something?
	je	rem2
	shrl	$3,%edx			# yes, set bit as still full
	btsl	%edx,_whichqs
rem2:
	movl	$0,P_RLINK(%eax)	# zap reverse link to indicate off list
	ret

rem3:	.asciz	"remrq"
sw0:	.asciz	"swtch"

/*
 * When no processes are on the runq, Swtch branches to idle
 * to wait for something to come ready.
 */
	ALIGN_TEXT
Idle:
	sti
	SHOW_STI

	ALIGN_TEXT
idle_loop:
	call	_spl0
	cmpl	$0,_whichqs
	jne	sw1
	hlt				# wait for interrupt
	jmp	idle_loop

badsw:
	pushl	$sw0
	call	_panic
	/*NOTREACHED*/

/*
 * Swtch()
 */
	SUPERALIGN_TEXT	/* so profiling doesn't lump Idle with swtch().. */
ENTRY(swtch)

	incl	_cnt+V_SWTCH

	/* switch to new process. first, save context as needed */

	movl	_curproc,%ecx

	/* if no process to save, don't bother */
	testl	%ecx,%ecx
	je	sw1

	movl	P_ADDR(%ecx),%ecx

	movl	(%esp),%eax		# Hardware registers
	movl	%eax, PCB_EIP(%ecx)
	movl	%ebx, PCB_EBX(%ecx)
	movl	%esp, PCB_ESP(%ecx)
	movl	%ebp, PCB_EBP(%ecx)
	movl	%esi, PCB_ESI(%ecx)
	movl	%edi, PCB_EDI(%ecx)

#ifdef NPX
	/* have we used fp, and need a save? */
	mov	_curproc,%eax
	cmp	%eax,_npxproc
	jne	1f
	pushl	%ecx			/* h/w bugs make saving complicated */
	leal	PCB_SAVEFPU(%ecx),%eax
	pushl	%eax
	call	_npxsave		/* do it in a big C function */
	popl	%eax
	popl	%ecx
1:
#endif

	movl	_CMAP2,%eax		# save temporary map PTE
	movl	%eax,PCB_CMAP2(%ecx)	# in our context
	movl	$0,_curproc		#  out of process

	# movw	_cpl, %ax
	# movw	%ax, PCB_IML(%ecx)	# save ipl

	/* save is done, now choose a new process or idle */
sw1:
	cli
	SHOW_CLI
	movl	_whichqs,%edi
2:
	# XXX - bsf is sloow
	bsfl	%edi,%eax		# find a full q
	je	Idle			# if none, idle
	# XX update whichqs?
swfnd:
	btrl	%eax,%edi		# clear q full status
	jnb	2b		# if it was clear, look for another
	movl	%eax,%ebx		# save which one we are using

	shll	$3,%eax
	addl	$_qs,%eax		# select q
	movl	%eax,%esi

#ifdef	DIAGNOSTIC
	cmpl	P_LINK(%eax),%eax # linked to self? (e.g. not on list)
	je	badsw			# not possible
#endif

	movl	P_LINK(%eax),%ecx	# unlink from front of process q
	movl	P_LINK(%ecx),%edx
	movl	%edx,P_LINK(%eax)
	movl	P_RLINK(%ecx),%eax
	movl	%eax,P_RLINK(%edx)

	cmpl	P_LINK(%ecx),%esi	# q empty
	je	3f
	btsl	%ebx,%edi		# nope, set to indicate full
3:
	movl	%edi,_whichqs		# update q status

	movl	$0,%eax
	movl	%eax,_want_resched

#ifdef	DIAGNOSTIC
	cmpl	%eax,P_WCHAN(%ecx)
	jne	badsw
	cmpb	$ SRUN,P_STAT(%ecx)
	jne	badsw
#endif

	movl	%eax,P_RLINK(%ecx) /* isolate process to run */
	movl	P_ADDR(%ecx),%edx
	movl	PCB_CR3(%edx),%ebx

	/* switch address space */
	movl	%ebx,%cr3

	/* restore context */
	movl	PCB_EBX(%edx), %ebx
	movl	PCB_ESP(%edx), %esp
	movl	PCB_EBP(%edx), %ebp
	movl	PCB_ESI(%edx), %esi
	movl	PCB_EDI(%edx), %edi
	movl	PCB_EIP(%edx), %eax
	movl	%eax, (%esp)

	movl	PCB_CMAP2(%edx),%eax	# get temporary map
	movl	%eax,_CMAP2		# reload temporary map PTE

	movl	%ecx,_curproc		# into next process
	movl	%edx,_curpcb

	pushl	%edx			# save p to return
/*
 * XXX - 0.0 forgot to save it - is that why this was commented out in 0.1?
 * I think restoring the cpl is unnecessary, but we must turn off the cli
 * now that spl*() don't do it as a side affect.
 */
	pushl	PCB_IML(%edx)
	sti
	SHOW_STI
#if 0
	call	_splx
#endif
	addl	$4,%esp
/*
 * XXX - 0.0 gets here via swtch_to_inactive().  I think 0.1 gets here in the
 * same way.  Better return a value.
 */
	popl	%eax			# return (p);
	ret

ENTRY(mvesp)
	movl	%esp,%eax
	ret
/*
 * struct proc *swtch_to_inactive(p) ; struct proc *p;
 *
 * At exit of a process, move off the address space of the
 * process and onto a "safe" one. Then, on a temporary stack
 * return and run code that disposes of the old state.
 * Since this code requires a parameter from the "old" stack,
 * pass it back as a return value.
 */
ENTRY(swtch_to_inactive)
	popl	%edx			# old pc
	popl	%eax			# arg, our return value
	movl	_IdlePTD,%ecx
	movl	%ecx,%cr3		# good bye address space
 #write buffer?
	movl	$tmpstk-4,%esp		# temporary stack, compensated for call
	jmp	%edx			# return, execute remainder of cleanup

/*
 * savectx(pcb, altreturn)
 * Update pcb, saving current processor state and arranging
 * for alternate return ala longjmp in swtch if altreturn is true.
 */
ENTRY(savectx)
	movl	4(%esp), %ecx
	movw	_cpl, %ax
	movw	%ax,  PCB_IML(%ecx)
	movl	(%esp), %eax
	movl	%eax, PCB_EIP(%ecx)
	movl	%ebx, PCB_EBX(%ecx)
	movl	%esp, PCB_ESP(%ecx)
	movl	%ebp, PCB_EBP(%ecx)
	movl	%esi, PCB_ESI(%ecx)
	movl	%edi, PCB_EDI(%ecx)

#ifdef NPX
	/*
	 * If npxproc == NULL, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If npxproc != NULL, then we have to save the npx h/w state to
	 * npxproc's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's npx state for forks by forgetting to reload.
	 */
	mov	_npxproc,%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	P_ADDR(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	_npxsave
	popl	%eax
	popl	%eax
	popl	%ecx

	pushl	%ecx
	pushl	$108+8*2	/* XXX h/w state size + padding */
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	_bcopy
	addl	$12,%esp
	popl	%ecx
1:
#endif

	movl	_CMAP2, %edx		# save temporary map PTE
	movl	%edx, PCB_CMAP2(%ecx)	# in our context

	cmpl	$0, 8(%esp)
	je	1f
	movl	%esp, %edx		# relocate current sp relative to pcb
	subl	$_kstack, %edx		#   (sp is relative to kstack):
	addl	%edx, %ecx		#   pcb += sp - kstack;
	movl	%eax, (%ecx)		# write return pc at (relocated) sp@
	# this mess deals with replicating register state gcc hides
	movl	12(%esp),%eax
	movl	%eax,12(%ecx)
	movl	16(%esp),%eax
	movl	%eax,16(%ecx)
	movl	20(%esp),%eax
	movl	%eax,20(%ecx)
	movl	24(%esp),%eax
	movl	%eax,24(%ecx)
1:
	xorl	%eax, %eax		# return 0
	ret

/*
 * addupc(int pc, struct uprof *up, int ticks):
 * update profiling information for the user process.
 */
ENTRY(addupc)
	pushl %ebp
	movl %esp,%ebp
	movl 12(%ebp),%edx		/* up */
	movl 8(%ebp),%eax		/* pc */

	subl PR_OFF(%edx),%eax		/* pc -= up->pr_off */
	jl L1				/* if (pc < 0) return */

	shrl $1,%eax			/* praddr = pc >> 1 */
	imull PR_SCALE(%edx),%eax	/* praddr *= up->pr_scale */
	shrl $15,%eax			/* praddr = praddr << 15 */
	andl $-2,%eax			/* praddr &= ~1 */

	cmpl PR_SIZE(%edx),%eax		/* if (praddr > up->pr_size) return */
	ja L1

/*	addl %eax,%eax			/* praddr -> word offset */
	addl PR_BASE(%edx),%eax		/* praddr += up-> pr_base */
	movl 16(%ebp),%ecx		/* ticks */

	movl _curpcb,%edx
	movl $proffault,PCB_ONFAULT(%edx)
	addl %ecx,(%eax)		/* storage location += ticks */
	movl $0,PCB_ONFAULT(%edx)
L1:
	leave
	ret

	ALIGN_TEXT
proffault:
	/* if we get a fault, then kill profiling all together */
	movl $0,PCB_ONFAULT(%edx)	/* squish the fault handler */
	movl 12(%ebp),%ecx
	movl $0,PR_SCALE(%ecx)		/* up->pr_scale = 0 */
	leave
	ret

# To be done:
ENTRY(astoff)
	ret


/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines
 *
 * XXX - debugger traps are now interrupt gates so at least bdb doesn't lose
 * control.  The sti's give the standard losing behaviour for ddb and kgdb.
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl _X/**/name; _X/**/name:
#define	TRAP(a)		pushl $(a) ; jmp alltraps
#ifdef KGDB
#  define BPTTRAP(a)	sti; pushl $(a) ; jmp bpttraps
#else
#  define BPTTRAP(a)	sti; TRAP(a)
#endif

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
#ifdef BDBTRAP
	BDBTRAP(dbg)
#endif
	pushl $0; BPTTRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
#ifdef BDBTRAP
	BDBTRAP(bpt)
#endif
	pushl $0; BPTTRAP(T_BPTFLT)
IDTVEC(ofl)
	pushl $0; TRAP(T_OFLOW)
IDTVEC(bnd)
	pushl $0; TRAP(T_BOUND)
IDTVEC(ill)
	pushl $0; TRAP(T_PRIVINFLT)
IDTVEC(dna)
	pushl $0; TRAP(T_DNA)
IDTVEC(dble)
	TRAP(T_DOUBLEFLT)
	/*PANIC("Double Fault");*/
IDTVEC(fpusegm)
	pushl $0; TRAP(T_FPOPFLT)
IDTVEC(tss)
	TRAP(T_TSSFLT)
	/*PANIC("TSS not valid");*/
IDTVEC(missing)
	TRAP(T_SEGNPFLT)
IDTVEC(stk)
	TRAP(T_STKFLT)
IDTVEC(prot)
	TRAP(T_PROTFLT)
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(rsvd)
	pushl $0; TRAP(T_RESERVED)
IDTVEC(fpu)
#ifdef NPX
	/*
	 * Handle like an interrupt so that we can call npxintr to clear the
	 * error.  It would be better to handle npx interrupts as traps but
	 * this is difficult for nested interrupts.
	 */
	pushl	$0		/* dummy error code */
	pushl	$T_ASTFLT
	pushal
	nop			/* silly, the bug is for popal and it only
				 * bites when the next instruction has a
				 * complicated address mode */
	pushl	%ds
	pushl	%es		/* now the stack frame is a trap frame */
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	pushl	_cpl
	pushl	$0		/* dummy unit to finish building intr frame */
	incl	_cnt+V_TRAP
	call	_npxintr
	jmp	doreti
#else
	pushl $0; TRAP(T_ARITHTRAP)
#endif
	/* 17 - 31 reserved for future exp */
IDTVEC(rsvd0)
	pushl $0; TRAP(17)
IDTVEC(rsvd1)
	pushl $0; TRAP(18)
IDTVEC(rsvd2)
	pushl $0; TRAP(19)
IDTVEC(rsvd3)
	pushl $0; TRAP(20)
IDTVEC(rsvd4)
	pushl $0; TRAP(21)
IDTVEC(rsvd5)
	pushl $0; TRAP(22)
IDTVEC(rsvd6)
	pushl $0; TRAP(23)
IDTVEC(rsvd7)
	pushl $0; TRAP(24)
IDTVEC(rsvd8)
	pushl $0; TRAP(25)
IDTVEC(rsvd9)
	pushl $0; TRAP(26)
IDTVEC(rsvd10)
	pushl $0; TRAP(27)
IDTVEC(rsvd11)
	pushl $0; TRAP(28)
IDTVEC(rsvd12)
	pushl $0; TRAP(29)
IDTVEC(rsvd13)
	pushl $0; TRAP(30)
IDTVEC(rsvd14)
	pushl $0; TRAP(31)

	SUPERALIGN_TEXT
alltraps:
	pushal
	nop
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
calltrap:
	incl	_cnt+V_TRAP
	call	_trap
	/*
	 * Return through doreti to handle ASTs.  Have to change trap frame
	 * to interrupt frame.
	 */
	movl	$T_ASTFLT,4+4+32(%esp)	/* new trap type (err code not used) */
	pushl	_cpl
	pushl	$0			/* dummy unit */
	jmp	doreti

#ifdef KGDB
/*
 * This code checks for a kgdb trap, then falls through
 * to the regular trap code.
 */
	SUPERALIGN_TEXT
bpttraps:
	pushal
	nop
	pushl	%es
	pushl	%ds
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp)
					# non-kernel mode?
	jne	calltrap		# yes
	call	_kgdb_trap_glue
	jmp	calltrap
#endif

/*
 * Call gate entry for syscall
 */
	SUPERALIGN_TEXT
IDTVEC(syscall)
	pushfl	# only for stupid carry bit and more stupid wait3 cc kludge
		# XXX - also for direction flag (bzero, etc. clear it)
	pushal	# only need eax,ecx,edx - trap resaves others
	nop
	movl	$KDSEL,%eax		# switch to kernel segments
	movl	%ax,%ds
	movl	%ax,%es
	incl	_cnt+V_SYSCALL	# kml 3/25/93
	call	_syscall
	/*
	 * Return through doreti to handle ASTs.  Have to change syscall frame
	 * to interrupt frame.
	 *
	 * XXX - we should have set up the frame earlier to avoid the
	 * following popal/pushal (not much can be done to avoid shuffling
	 * the flags).  Consistent frames would simplify things all over.
	 */
	movl	32+0(%esp),%eax	/* old flags, shuffle to above cs:eip */
	movl	32+4(%esp),%ebx	/* `int' frame should have been ef, eip, cs */
	movl	32+8(%esp),%ecx
	movl	%ebx,32+0(%esp)
	movl	%ecx,32+4(%esp)
	movl	%eax,32+8(%esp)
	popal
	nop
	pushl	$0		/* dummy error code */
	pushl	$T_ASTFLT
	pushal
	nop
	movl	__udatasel,%eax	/* switch back to user segments */
	pushl	%eax		/* XXX - better to preserve originals? */
	pushl	%eax
	pushl	_cpl
	pushl	$0
	jmp	doreti


/*****************************************************************************/
/* include generated interrupt vectors and ISA intr code                     */
/*****************************************************************************/

#include "i386/isa/vector.s"
#include "i386/isa/icu.s"
