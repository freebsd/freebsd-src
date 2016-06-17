/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *  MPC8xx modifications Copyright (C) 1997 Dan Malek (dmalek@jlc.net).
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  This file contains the low-level support and setup for the
 *  PowerPC platform, including trap and interrupt dispatch.
 *  (The PPC 8xx embedded CPUs use head_8xx.S instead.)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/cputable.h>
#include <asm/cache.h>
#include <asm/ppc_asm.h>
#include "ppc_defs.h"

#ifdef CONFIG_APUS
#include <asm/amigappc.h>
#endif

#ifdef CONFIG_PPC64BRIDGE
#define LOAD_BAT(n, reg, RA, RB)	\
	ld	RA,(n*32)+0(reg);	\
	ld	RB,(n*32)+8(reg);	\
	mtspr	IBAT##n##U,RA;		\
	mtspr	IBAT##n##L,RB;		\
	ld	RA,(n*32)+16(reg);	\
	ld	RB,(n*32)+24(reg);	\
	mtspr	DBAT##n##U,RA;		\
	mtspr	DBAT##n##L,RB;		\

#else /* CONFIG_PPC64BRIDGE */

/* 601 only have IBAT; cr0.eq is set on 601 when using this macro */
#define LOAD_BAT(n, reg, RA, RB)	\
	/* see the comment for clear_bats() -- Cort */ \
	li	RA,0;			\
	mtspr	IBAT##n##U,RA;		\
	mtspr	DBAT##n##U,RA;		\
	lwz	RA,(n*16)+0(reg);	\
	lwz	RB,(n*16)+4(reg);	\
	mtspr	IBAT##n##U,RA;		\
	mtspr	IBAT##n##L,RB;		\
	beq	1f;			\
	lwz	RA,(n*16)+8(reg);	\
	lwz	RB,(n*16)+12(reg);	\
	mtspr	DBAT##n##U,RA;		\
	mtspr	DBAT##n##L,RB;		\
1:
#endif /* CONFIG_PPC64BRIDGE */

	.text
	.stabs	"arch/ppc/kernel/",N_SO,0,0,0f
	.stabs	"head.S",N_SO,0,0,0f
0:
	.globl	_stext
_stext:

/*
 * _start is defined this way because the XCOFF loader in the OpenFirmware
 * on the powermac expects the entry point to be a procedure descriptor.
 */
	.text
	.globl	_start
_start:
	/*
	 * These are here for legacy reasons, the kernel used to
	 * need to look like a coff function entry for the pmac
	 * but we're always started by some kind of bootloader now.
	 *  -- Cort
	 */
	nop
	nop
	nop

/* PMAC
 * Enter here with the kernel text, data and bss loaded starting at
 * 0, running with virtual == physical mapping.
 * r5 points to the prom entry point (the client interface handler
 * address).  Address translation is turned on, with the prom
 * managing the hash table.  Interrupts are disabled.  The stack
 * pointer (r1) points to just below the end of the half-meg region
 * from 0x380000 - 0x400000, which is mapped in already.
 *
 * If we are booted from MacOS via BootX, we enter with the kernel
 * image loaded somewhere, and the following values in registers:
 *  r3: 'BooX' (0x426f6f58)
 *  r4: virtual address of boot_infos_t
 *  r5: 0
 *
 * APUS
 *   r3: 'APUS'
 *   r4: physical address of memory base
 *   Linux/m68k style BootInfo structure at &_end.
 *
 * PREP
 * This is jumped to on prep systems right after the kernel is relocated
 * to its proper place in memory by the boot loader.  The expected layout
 * of the regs is:
 *   r3: ptr to residual data
 *   r4: initrd_start or if no initrd then 0
 *   r5: initrd_end - unused if r4 is 0
 *   r6: Start of command line string
 *   r7: End of command line string
 *
 * This just gets a minimal mmu environment setup so we can call
 * start_here() to do the real work.
 * -- Cort
 */

	.globl	__start
__start:
/*
 * We have to do any OF calls before we map ourselves to KERNELBASE,
 * because OF may have I/O devices mapped into that area
 * (particularly on CHRP).
 */
	mr	r31,r3			/* save parameters */
	mr	r30,r4
	mr	r29,r5
	mr	r28,r6
	mr	r27,r7
	li	r24,0			/* cpu # */

#ifdef CONFIG_POWER4
/*
 * On the PPC970, we have to turn off real-mode cache inhibit
 * early, before we first turn the MMU off.
 */
	mfspr	r0,SPRN_PVR
	srwi	r0,r0,16
	cmpwi	r0,0x39
	beql	ppc970_setup_hid
#endif

/*
 * early_init() does the early machine identification and does
 * the necessary low-level setup and clears the BSS
 *  -- Cort <cort@fsmlabs.com>
 */
	bl	early_init

#ifdef CONFIG_APUS
/* On APUS the __va/__pa constants need to be set to the correct
 * values before continuing.
 */
	mr	r4,r30
	bl	fix_mem_constants
#endif /* CONFIG_APUS */

/* Switch MMU off, clear BATs and flush TLB. At this point, r3 contains
 * the physical address we are running at, returned by early_init()
 */
 	bl	mmu_off
__after_mmu_off:

#ifndef CONFIG_POWER4
	bl	clear_bats
	bl	flush_tlbs
	bl	initial_bats
#if !defined(CONFIG_APUS) && defined(CONFIG_BOOTX_TEXT)
	bl	setup_disp_bat
#endif
#else /* CONFIG_POWER4 */
/*
 * Load up the SDR1 and segment register values now
 * since we don't have the BATs.
 * Also make sure we are running in 32-bit mode.
 */
	bl	reloc_offset
	addis	r14,r3,_SDR1@ha		/* get the value from _SDR1 */
	lwz	r14,_SDR1@l(r14)	/* assume hash table below 4GB */
	mtspr	SDR1,r14
	slbia
	lis	r4,0x2000		/* set pseudo-segment reg 12 */
	ori	r5,r4,0x0ccc
	mtsr	12,r5
	ori	r4,r4,0x0888		/* set pseudo-segment reg 8 */
	mtsr	8,r4			/* (for access to serial port) */
	mfmsr	r0
	clrldi	r0,r0,1
	sync
	mtmsr	r0
	isync
#endif /* CONFIG_POWER4 */

	/*
	 * Call setup_cpu for CPU 0
	 */
	bl	reloc_offset
	li	r24,0			/* cpu# */
	bl	call_setup_cpu		/* Call setup_cpu for this CPU */
#ifdef CONFIG_6xx
	bl	reloc_offset
	bl	init_idle_6xx
#endif /* CONFIG_6xx */

#ifndef CONFIG_APUS
/*
 * We need to run with _start at physical address 0.
 * On CHRP, we are loaded at 0x10000 since OF on CHRP uses
 * the exception vectors at 0 (and therefore this copy
 * overwrites OF's exception vectors with our own).
 * The MMU is off at this point.
 */
	bl	reloc_offset
	mr	r26,r3
	addis	r4,r3,KERNELBASE@h	/* current address of _start */
	cmpwi	0,r4,0			/* are we already running at 0? */
	bne	relocate_kernel
#endif /* CONFIG_APUS */
/*
 * we now have the 1st 16M of ram mapped with the bats.
 * prep needs the mmu to be turned on here, but pmac already has it on.
 * this shouldn't bother the pmac since it just gets turned on again
 * as we jump to our code at KERNELBASE. -- Cort
 * Actually no, pmac doesn't have it on any more. BootX enters with MMU
 * off, and in other cases, we now turn it off before changing BATs above.
 */
turn_on_mmu:
	mfmsr	r0
	ori	r0,r0,MSR_DR|MSR_IR
	mtspr	SRR1,r0
	lis	r0,start_here@h
	ori	r0,r0,start_here@l
	mtspr	SRR0,r0
	SYNC
	RFI				/* enables MMU */

/*
 * We need __secondary_hold as a place to hold the other cpus on
 * an SMP machine, even when we are running a UP kernel.
 */
	. = 0xc0			/* for prep bootloader */
	li	r3,1			/* MTX only has 1 cpu */
	.globl	__secondary_hold
__secondary_hold:
	/* tell the master we're here */
	stw	r3,4(0)
#ifdef CONFIG_SMP
100:	lwz	r4,0(0)
	/* wait until we're told to start */
	cmpw	0,r4,r3
	bne	100b
	/* our cpu # was at addr 0 - go */
	mr	r24,r3			/* cpu # */
	b	__secondary_start
#else
	b	.
#endif /* CONFIG_SMP */

/*
 * Exception entry code.  This code runs with address translation
 * turned off, i.e. using physical addresses.
 * We assume sprg3 has the physical address of the current
 * task's thread_struct.
 */
#define EXCEPTION_PROLOG	\
	mtspr	SPRG0,r20;	\
	mtspr	SPRG1,r21;	\
	mfcr	r20;		\
	mfspr	r21,SPRG2;		/* exception stack to use from */ \
	cmpwi	0,r21,0;		/* user mode or RTAS */ \
	bne	1f;		\
	tophys(r21,r1);			/* use tophys(kernel sp) otherwise */ \
	subi	r21,r21,INT_FRAME_SIZE;	/* alloc exc. frame */\
1:	CLR_TOP32(r21);		\
	stw	r20,_CCR(r21);		/* save registers */ \
	stw	r22,GPR22(r21);	\
	stw	r23,GPR23(r21);	\
	mfspr	r20,SPRG0;	\
	stw	r20,GPR20(r21);	\
	mfspr	r22,SPRG1;	\
	stw	r22,GPR21(r21);	\
	mflr	r20;		\
	stw	r20,_LINK(r21);	\
	mfctr	r22;		\
	stw	r22,_CTR(r21);	\
	mfspr	r20,XER;	\
	stw	r20,_XER(r21);	\
	mfspr	r22,SRR0;	\
	mfspr	r23,SRR1;	\
	stw	r0,GPR0(r21);	\
	stw	r1,GPR1(r21);	\
	stw	r2,GPR2(r21);	\
	stw	r1,0(r21);	\
	tovirt(r1,r21);			/* set new kernel sp */	\
	SAVE_4GPRS(3, r21);	\
	SAVE_GPR(7, r21);
/*
 * Note: code which follows this uses cr0.eq (set if from kernel),
 * r21, r22 (SRR0), and r23 (SRR1).
 */

/*
 * Exception vectors.
 */
#define STD_EXCEPTION(n, label, hdlr)		\
	. = n;					\
label:						\
	EXCEPTION_PROLOG;			\
	addi	r3,r1,STACK_FRAME_OVERHEAD;	\
	li	r20,MSR_KERNEL;			\
	bl	transfer_to_handler; 		\
i##n:						\
	.long	hdlr;				\
	.long	ret_from_except

/* System reset */
#ifdef CONFIG_SMP /* MVME/MTX and gemini start the secondary here */
#ifdef CONFIG_GEMINI
	. = 0x100
	b	__secondary_start_gemini
#else /* CONFIG_GEMINI */
	STD_EXCEPTION(0x100, Reset, __secondary_start_psurge)
#endif /* CONFIG_GEMINI */
#else
	STD_EXCEPTION(0x100, Reset, UnknownException)
#endif

/* Machine check */
BEGIN_FTR_SECTION
	DSSALL
	sync
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
	STD_EXCEPTION(0x200, MachineCheck, MachineCheckException)

/* Data access exception. */
	. = 0x300
#ifdef CONFIG_PPC64BRIDGE
	b	DataAccess
DataAccessCont:
#else
DataAccess:
	EXCEPTION_PROLOG
#endif /* CONFIG_PPC64BRIDGE */
	mfspr	r20,DSISR
BEGIN_FTR_SECTION
	andis.	r0,r20,0xa470		/* weird error? */
	bne	1f			/* if not, try to put a PTE */
	mfspr	r4,DAR			/* into the hash table */
	rlwinm	r3,r20,32-15,21,21	/* DSISR_STORE -> _PAGE_RW */
	bl	hash_page
END_FTR_SECTION_IFSET(CPU_FTR_HPTE_TABLE)
1:	stw	r20,_DSISR(r21)
	mr	r5,r20
	mfspr	r4,DAR
	stw	r4,_DAR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
i0x300:
	.long	do_page_fault
	.long	ret_from_except

#ifdef CONFIG_PPC64BRIDGE
/* SLB fault on data access. */
	. = 0x380
	b	DataSegment
DataSegmentCont:
	mfspr	r4,DAR
	stw	r4,_DAR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
	.long	UnknownException
	.long	ret_from_except
#endif /* CONFIG_PPC64BRIDGE */

/* Instruction access exception. */
	. = 0x400
#ifdef CONFIG_PPC64BRIDGE
	b	InstructionAccess
InstructionAccessCont:
#else
InstructionAccess:
	EXCEPTION_PROLOG
#endif /* CONFIG_PPC64BRIDGE */
BEGIN_FTR_SECTION
	andis.	r0,r23,0x4000		/* no pte found? */
	beq	1f			/* if so, try to put a PTE */
	li	r3,0			/* into the hash table */
	mr	r4,r22			/* SRR0 is fault address */
	bl	hash_page
END_FTR_SECTION_IFSET(CPU_FTR_HPTE_TABLE)
1:	addi	r3,r1,STACK_FRAME_OVERHEAD
	mr	r4,r22
	mr	r5,r23
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
i0x400:
	.long	do_page_fault
	.long	ret_from_except

#ifdef CONFIG_PPC64BRIDGE
/* SLB fault on instruction access. */
	. = 0x480
	b	InstructionSegment
InstructionSegmentCont:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
	.long	UnknownException
	.long	ret_from_except
#endif /* CONFIG_PPC64BRIDGE */

/* External interrupt */
	. = 0x500;
HardwareInterrupt:
	EXCEPTION_PROLOG;
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	li	r4,0
	bl	transfer_to_handler
	.globl do_IRQ_intercept
do_IRQ_intercept:
	.long	do_IRQ;
	.long	ret_from_intercept

/* Alignment exception */
	. = 0x600
Alignment:
	EXCEPTION_PROLOG
	mfspr	r4,DAR
	stw	r4,_DAR(r21)
	mfspr	r5,DSISR
	stw	r5,_DSISR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
i0x600:
	.long	AlignmentException
	.long	ret_from_except

/* Program check exception */
	. = 0x700
ProgramCheck:
	EXCEPTION_PROLOG
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
i0x700:
	.long	ProgramCheckException
	.long	ret_from_except

/* Floating-point unavailable */
	. = 0x800
FPUnavailable:
	EXCEPTION_PROLOG
	bne	load_up_fpu		/* if from user, just load it up */
	li	r20,MSR_KERNEL
	bl	transfer_to_handler	/* if from kernel, take a trap */
i0x800:
	.long	KernelFP
	.long	ret_from_except

	. = 0x900
Decrementer:
	EXCEPTION_PROLOG
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	bl	transfer_to_handler
	.globl timer_interrupt_intercept
timer_interrupt_intercept:
	.long	timer_interrupt
	.long	ret_from_intercept

	STD_EXCEPTION(0xa00, Trap_0a, UnknownException)
	STD_EXCEPTION(0xb00, Trap_0b, UnknownException)

/* System call */
	. = 0xc00
SystemCall:
	EXCEPTION_PROLOG
	stw	r3,ORIG_GPR3(r21)
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	bl	transfer_to_handler
	.long	DoSyscall
	.long	ret_from_except

/* Single step - not used on 601 */
	STD_EXCEPTION(0xd00, SingleStep, SingleStepException)
	STD_EXCEPTION(0xe00, Trap_0e, UnknownException)

/*
 * The Altivec unavailable trap is at 0x0f20.  Foo.
 * We effectively remap it to 0x3000.
 */
	. = 0xf00
	b	Trap_0f
trap_0f_cont:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,MSR_KERNEL
	bl	transfer_to_handler
	.long	UnknownException
	.long	ret_from_except

	. = 0xf20
#ifdef CONFIG_ALTIVEC
	b	AltiVecUnavailable
#endif
Trap_0f:
	EXCEPTION_PROLOG
	b	trap_0f_cont

/*
 * Handle TLB miss for instruction on 603/603e.
 * Note: we get an alternate set of r0 - r3 to use automatically.
 */
	. = 0x1000
InstructionTLBMiss:
/*
 * r0:	stored ctr
 * r1:	linux style pte ( later becomes ppc hardware pte )
 * r2:	ptr to linux-style pte
 * r3:	scratch
 */
	mfctr	r0
	/* Get PTE (linux-style) and check access */
	mfspr	r3,IMISS
	lis	r1,KERNELBASE@h		/* check if kernel address */
	cmplw	0,r3,r1
	mfspr	r2,SPRG3
	li	r1,_PAGE_USER|_PAGE_PRESENT /* low addresses tested as user */
	lwz	r2,PGDIR(r2)
	blt+	112f
	lis	r2,swapper_pg_dir@ha	/* if kernel address, use */
	addi	r2,r2,swapper_pg_dir@l	/* kernel page table */
	mfspr	r1,SRR1			/* and MSR_PR bit from SRR1 */
	rlwinm	r1,r1,32-12,29,29	/* shift MSR_PR to _PAGE_USER posn */
112:	tophys(r2,r2)
	rlwimi	r2,r3,12,20,29		/* insert top 10 bits of address */
	lwz	r2,0(r2)		/* get pmd entry */
	rlwinm.	r2,r2,0,0,19		/* extract address of pte page */
	beq-	InstructionAddressInvalid	/* return if no mapping */
	tophys(r2,r2)
	rlwimi	r2,r3,22,20,29		/* insert next 10 bits of address */
	lwz	r3,0(r2)		/* get linux-style pte */
	andc.	r1,r1,r3		/* check access & ~permission */
	bne-	InstructionAddressInvalid /* return if access not permitted */
	ori	r3,r3,_PAGE_ACCESSED	/* set _PAGE_ACCESSED in pte */
	/*
	 * NOTE! We are assuming this is not an SMP system, otherwise
	 * we would need to update the pte atomically with lwarx/stwcx.
	 */
	stw	r3,0(r2)		/* update PTE (accessed bit) */
	/* Convert linux-style PTE to low word of PPC-style PTE */
	rlwinm	r1,r3,32-10,31,31	/* _PAGE_RW -> PP lsb */
	rlwinm	r2,r3,32-7,31,31	/* _PAGE_DIRTY -> PP lsb */
	and	r1,r1,r2		/* writable if _RW and _DIRTY */
	rlwimi	r3,r3,32-1,30,30	/* _PAGE_USER -> PP msb */
	rlwimi	r3,r3,32-1,31,31	/* _PAGE_USER -> PP lsb */
	ori	r1,r1,0xe14		/* clear out reserved bits and M */
	andc	r1,r3,r1		/* PP = user? (rw&dirty? 2: 3): 0 */
	mtspr	RPA,r1
	mfspr	r3,IMISS
	tlbli	r3
	mfspr	r3,SRR1		/* Need to restore CR0 */
	mtcrf	0x80,r3
	rfi
InstructionAddressInvalid:
	mfspr	r3,SRR1
	rlwinm	r1,r3,9,6,6	/* Get load/store bit */

	addis	r1,r1,0x2000
	mtspr	DSISR,r1	/* (shouldn't be needed) */
	mtctr	r0		/* Restore CTR */
	andi.	r2,r3,0xFFFF	/* Clear upper bits of SRR1 */
	or	r2,r2,r1
	mtspr	SRR1,r2
	mfspr	r1,IMISS	/* Get failing address */
	rlwinm.	r2,r2,0,31,31	/* Check for little endian access */
	rlwimi	r2,r2,1,30,30	/* change 1 -> 3 */
	xor	r1,r1,r2
	mtspr	DAR,r1		/* Set fault address */
	mfmsr	r0		/* Restore "normal" registers */
	xoris	r0,r0,MSR_TGPR>>16
	mtcrf	0x80,r3		/* Restore CR0 */
	mtmsr	r0
	b	InstructionAccess

/*
 * Handle TLB miss for DATA Load operation on 603/603e
 */
	. = 0x1100
DataLoadTLBMiss:
/*
 * r0:	stored ctr
 * r1:	linux style pte ( later becomes ppc hardware pte )
 * r2:	ptr to linux-style pte
 * r3:	scratch
 */
	mfctr	r0
	/* Get PTE (linux-style) and check access */
	mfspr	r3,DMISS
	lis	r1,KERNELBASE@h		/* check if kernel address */
	cmplw	0,r3,r1
	mfspr	r2,SPRG3
	li	r1,_PAGE_USER|_PAGE_PRESENT /* low addresses tested as user */
	lwz	r2,PGDIR(r2)
	blt+	112f
	lis	r2,swapper_pg_dir@ha	/* if kernel address, use */
	addi	r2,r2,swapper_pg_dir@l	/* kernel page table */
	mfspr	r1,SRR1			/* and MSR_PR bit from SRR1 */
	rlwinm	r1,r1,32-12,29,29	/* shift MSR_PR to _PAGE_USER posn */
112:	tophys(r2,r2)
	rlwimi	r2,r3,12,20,29		/* insert top 10 bits of address */
	lwz	r2,0(r2)		/* get pmd entry */
	rlwinm.	r2,r2,0,0,19		/* extract address of pte page */
	beq-	DataAddressInvalid	/* return if no mapping */
	tophys(r2,r2)
	rlwimi	r2,r3,22,20,29		/* insert next 10 bits of address */
	lwz	r3,0(r2)		/* get linux-style pte */
	andc.	r1,r1,r3		/* check access & ~permission */
	bne-	DataAddressInvalid	/* return if access not permitted */
	ori	r3,r3,_PAGE_ACCESSED	/* set _PAGE_ACCESSED in pte */
	/*
	 * NOTE! We are assuming this is not an SMP system, otherwise
	 * we would need to update the pte atomically with lwarx/stwcx.
	 */
	stw	r3,0(r2)		/* update PTE (accessed bit) */
	/* Convert linux-style PTE to low word of PPC-style PTE */
	rlwinm	r1,r3,32-10,31,31	/* _PAGE_RW -> PP lsb */
	rlwinm	r2,r3,32-7,31,31	/* _PAGE_DIRTY -> PP lsb */
	and	r1,r1,r2		/* writable if _RW and _DIRTY */
	rlwimi	r3,r3,32-1,30,30	/* _PAGE_USER -> PP msb */
	rlwimi	r3,r3,32-1,31,31	/* _PAGE_USER -> PP lsb */
	ori	r1,r1,0xe14		/* clear out reserved bits and M */
	andc	r1,r3,r1		/* PP = user? (rw&dirty? 2: 3): 0 */
	mtspr	RPA,r1
	mfspr	r3,DMISS
	tlbld	r3
	mfspr	r3,SRR1		/* Need to restore CR0 */
	mtcrf	0x80,r3
	rfi
DataAddressInvalid:
	mfspr	r3,SRR1
	rlwinm	r1,r3,9,6,6	/* Get load/store bit */
	addis	r1,r1,0x2000
	mtspr	DSISR,r1
	mtctr	r0		/* Restore CTR */
	andi.	r2,r3,0xFFFF	/* Clear upper bits of SRR1 */
	mtspr	SRR1,r2
	mfspr	r1,DMISS	/* Get failing address */
	rlwinm.	r2,r2,0,31,31	/* Check for little endian access */
	beq	20f		/* Jump if big endian */
	xori	r1,r1,3
20:	mtspr	DAR,r1		/* Set fault address */
	mfmsr	r0		/* Restore "normal" registers */
	xoris	r0,r0,MSR_TGPR>>16
	mtcrf	0x80,r3		/* Restore CR0 */
	mtmsr	r0
	b	DataAccess

/*
 * Handle TLB miss for DATA Store on 603/603e
 */
	. = 0x1200
DataStoreTLBMiss:
/*
 * r0:	stored ctr
 * r1:	linux style pte ( later becomes ppc hardware pte )
 * r2:	ptr to linux-style pte
 * r3:	scratch
 */
	mfctr	r0
	/* Get PTE (linux-style) and check access */
	mfspr	r3,DMISS
	lis	r1,KERNELBASE@h		/* check if kernel address */
	cmplw	0,r3,r1
	mfspr	r2,SPRG3
	li	r1,_PAGE_RW|_PAGE_USER|_PAGE_PRESENT /* access flags */
	lwz	r2,PGDIR(r2)
	blt+	112f
	lis	r2,swapper_pg_dir@ha	/* if kernel address, use */
	addi	r2,r2,swapper_pg_dir@l	/* kernel page table */
	mfspr	r1,SRR1			/* and MSR_PR bit from SRR1 */
	rlwinm	r1,r1,32-12,29,29	/* shift MSR_PR to _PAGE_USER posn */
112:	tophys(r2,r2)
	rlwimi	r2,r3,12,20,29		/* insert top 10 bits of address */
	lwz	r2,0(r2)		/* get pmd entry */
	rlwinm.	r2,r2,0,0,19		/* extract address of pte page */
	beq-	DataAddressInvalid	/* return if no mapping */
	tophys(r2,r2)
	rlwimi	r2,r3,22,20,29		/* insert next 10 bits of address */
	lwz	r3,0(r2)		/* get linux-style pte */
	andc.	r1,r1,r3		/* check access & ~permission */
	bne-	DataAddressInvalid	/* return if access not permitted */
	ori	r3,r3,_PAGE_ACCESSED|_PAGE_DIRTY
	/*
	 * NOTE! We are assuming this is not an SMP system, otherwise
	 * we would need to update the pte atomically with lwarx/stwcx.
	 */
	stw	r3,0(r2)		/* update PTE (accessed/dirty bits) */
	/* Convert linux-style PTE to low word of PPC-style PTE */
	rlwimi	r3,r3,32-1,30,30	/* _PAGE_USER -> PP msb */
	li	r1,0xe15		/* clear out reserved bits and M */
	andc	r1,r3,r1		/* PP = user? 2: 0 */
	mtspr	RPA,r1
	mfspr	r3,DMISS
	tlbld	r3
	mfspr	r3,SRR1		/* Need to restore CR0 */
	mtcrf	0x80,r3
	rfi

#ifndef CONFIG_ALTIVEC
#define AltivecAssistException	UnknownException
#endif

	STD_EXCEPTION(0x1300, Trap_13, InstructionBreakpoint)
	STD_EXCEPTION(0x1400, SMI, SMIException)
	STD_EXCEPTION(0x1500, Trap_15, UnknownException)
#ifdef CONFIG_POWER4
	STD_EXCEPTION(0x1600, Trap_16, UnknownException)
	STD_EXCEPTION(0x1700, Trap_17, AltivecAssistException)
	STD_EXCEPTION(0x1800, Trap_18, TAUException)
#else /* !CONFIG_POWER4 */
	STD_EXCEPTION(0x1600, Trap_16, AltivecAssistException)
	STD_EXCEPTION(0x1700, Trap_17, TAUException)
	STD_EXCEPTION(0x1800, Trap_18, UnknownException)
#endif
	STD_EXCEPTION(0x1900, Trap_19, UnknownException)
	STD_EXCEPTION(0x1a00, Trap_1a, UnknownException)
	STD_EXCEPTION(0x1b00, Trap_1b, UnknownException)
	STD_EXCEPTION(0x1c00, Trap_1c, UnknownException)
	STD_EXCEPTION(0x1d00, Trap_1d, UnknownException)
	STD_EXCEPTION(0x1e00, Trap_1e, UnknownException)
	STD_EXCEPTION(0x1f00, Trap_1f, UnknownException)
	STD_EXCEPTION(0x2000, RunMode, RunModeException)
	STD_EXCEPTION(0x2100, Trap_21, UnknownException)
	STD_EXCEPTION(0x2200, Trap_22, UnknownException)
	STD_EXCEPTION(0x2300, Trap_23, UnknownException)
	STD_EXCEPTION(0x2400, Trap_24, UnknownException)
	STD_EXCEPTION(0x2500, Trap_25, UnknownException)
	STD_EXCEPTION(0x2600, Trap_26, UnknownException)
	STD_EXCEPTION(0x2700, Trap_27, UnknownException)
	STD_EXCEPTION(0x2800, Trap_28, UnknownException)
	STD_EXCEPTION(0x2900, Trap_29, UnknownException)
	STD_EXCEPTION(0x2a00, Trap_2a, UnknownException)
	STD_EXCEPTION(0x2b00, Trap_2b, UnknownException)
	STD_EXCEPTION(0x2c00, Trap_2c, UnknownException)
	STD_EXCEPTION(0x2d00, Trap_2d, UnknownException)
	STD_EXCEPTION(0x2e00, Trap_2e, UnknownException)
	STD_EXCEPTION(0x2f00, Trap_2f, UnknownException)

	. = 0x3000

#ifdef CONFIG_ALTIVEC
AltiVecUnavailable:
	EXCEPTION_PROLOG
	bne	load_up_altivec		/* if from user, just load it up */
	li	r20,MSR_KERNEL
	bl	transfer_to_handler	/* if from kernel, take a trap */
	.long	KernelAltiVec
	.long	ret_from_except
#endif /* CONFIG_ALTIVEC */

#ifdef CONFIG_PPC64BRIDGE
DataAccess:
	EXCEPTION_PROLOG
	b	DataAccessCont
InstructionAccess:
	EXCEPTION_PROLOG
	b	InstructionAccessCont
DataSegment:
	EXCEPTION_PROLOG
	b	DataSegmentCont
InstructionSegment:
	EXCEPTION_PROLOG
	b	InstructionSegmentCont
#endif /* CONFIG_PPC64BRIDGE */

/*
 * This code finishes saving the registers to the exception frame
 * and jumps to the appropriate handler for the exception, turning
 * on address translation.
 */
	.globl	transfer_to_handler
transfer_to_handler:
	stw	r22,_NIP(r21)
	stw	r23,_MSR(r21)
	SAVE_4GPRS(8, r21)
	SAVE_8GPRS(12, r21)
	SAVE_8GPRS(24, r21)
	andi.	r23,r23,MSR_PR
	mfspr	r23,SPRG3		/* if from user, fix up THREAD.regs */
	addi	r2,r23,-THREAD		/* set r2 to current */
	beq	2f
	addi	r24,r1,STACK_FRAME_OVERHEAD
	stw	r24,PT_REGS(r23)
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	mfspr	r22,SPRN_VRSAVE		/* if G4, save vrsave register value */
	stw	r22,THREAD_VRSAVE(r23)
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
#ifndef CONFIG_6xx
2:
#endif
	.globl transfer_to_handler_cont
transfer_to_handler_cont:
	tovirt(r2,r2)
	mflr	r23
	andi.	r24,r23,0x3f00		/* get vector offset */
	stw	r24,TRAP(r21)
	li	r22,0
	stw	r22,RESULT(r21)
	mtspr	SPRG2,r22		/* r1 is now kernel sp */
	addi	r24,r2,TASK_STRUCT_SIZE	/* check for kernel stack overflow */
	cmplw	0,r1,r2
	cmplw	1,r1,r24
	crand	1,1,4
	bgt-	stack_ovf		/* if r2 < r1 < r2+TASK_STRUCT_SIZE */
	lwz	r24,0(r23)		/* virtual address of handler */
	lwz	r23,4(r23)		/* where to go when done */
	FIX_SRR1(r20,r22)
	mtspr	SRR0,r24
	mtspr	SRR1,r20
	mtlr	r23
	SYNC
	RFI				/* jump to handler, enable MMU */

#ifdef CONFIG_6xx
2:
	/* Out of line case when returning to kernel,
	 * check return from power_save_6xx
	 */
	mfspr	r24,SPRN_HID0
	mtcr	r24
BEGIN_FTR_SECTION
	bt-	8,power_save_6xx_restore	/* Check DOZE */
END_FTR_SECTION_IFSET(CPU_FTR_CAN_DOZE)
BEGIN_FTR_SECTION
	bt-	9,power_save_6xx_restore	/* Check NAP */
END_FTR_SECTION_IFSET(CPU_FTR_CAN_NAP)
	b	transfer_to_handler_cont

#endif /* CONFIG_6xx */

/*
 * On kernel stack overflow, load up an initial stack pointer
 * and call StackOverflow(regs), which should not return.
 */
stack_ovf:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	lis	r1,init_task_union@ha
	addi	r1,r1,init_task_union@l
	addi	r1,r1,TASK_UNION_SIZE-STACK_FRAME_OVERHEAD
	lis	r24,StackOverflow@ha
	addi	r24,r24,StackOverflow@l
	li	r20,MSR_KERNEL
	FIX_SRR1(r20,r22)
	mtspr	SRR0,r24
	mtspr	SRR1,r20
	SYNC
	RFI

/*
 * This task wants to use the FPU now.
 * On UP, disable FP for the task which had the FPU previously,
 * and save its floating-point registers in its thread_struct.
 * Load up this task's FP registers from its thread_struct,
 * enable the FPU for the current task and return to the task.
 */
load_up_fpu:
	mfmsr	r5
	ori	r5,r5,MSR_FP
#ifdef CONFIG_PPC64BRIDGE
	clrldi	r5,r5,1			/* turn off 64-bit mode */
#endif /* CONFIG_PPC64BRIDGE */
	SYNC
	MTMSRD(r5)			/* enable use of fpu now */
	isync
/*
 * For SMP, we don't do lazy FPU switching because it just gets too
 * horrendously complex, especially when a task switches from one CPU
 * to another.  Instead we call giveup_fpu in switch_to.
 */
#ifndef CONFIG_SMP
	tophys(r6,0)			/* get __pa constant */
	addis	r3,r6,last_task_used_math@ha
	lwz	r4,last_task_used_math@l(r3)
	cmpi	0,r4,0
	beq	1f
	add	r4,r4,r6
	addi	r4,r4,THREAD		/* want last_task_used_math->thread */
	SAVE_32FPRS(0, r4)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR-4(r4)
	lwz	r5,PT_REGS(r4)
	add	r5,r5,r6
	lwz	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r20,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r20		/* disable FP for previous task */
	stw	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#endif /* CONFIG_SMP */
	/* enable use of FP after return */
	mfspr	r5,SPRG3		/* current task's THREAD (phys) */
	lwz	r4,THREAD_FPEXC_MODE(r5)
	ori	r23,r23,MSR_FP		/* enable FP for current */
	or	r23,r23,r4
	lfd	fr0,THREAD_FPSCR-4(r5)
	mtfsf	0xff,fr0
	REST_32FPRS(0, r5)
#ifndef CONFIG_SMP
	subi	r4,r5,THREAD
	sub	r4,r4,r6
	stw	r4,last_task_used_math@l(r3)
#endif /* CONFIG_SMP */
	/* restore registers and return */
	lwz	r3,_CCR(r21)
	lwz	r4,_LINK(r21)
	mtcrf	0xff,r3
	mtlr	r4
	REST_GPR(1, r21)
	REST_4GPRS(3, r21)
	/* we haven't used ctr or xer */
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	REST_GPR(20, r21)
	REST_2GPRS(22, r21)
	lwz	r21,GPR21(r21)
	SYNC
	RFI

/*
 * FP unavailable trap from kernel - print a message, but let
 * the task use FP in the kernel until it returns to user mode.
 */
KernelFP:
	lwz	r3,_MSR(r1)
	ori	r3,r3,MSR_FP
	stw	r3,_MSR(r1)		/* enable use of FP after return */
	lis	r3,86f@h
	ori	r3,r3,86f@l
	mr	r4,r2			/* current */
	lwz	r5,_NIP(r1)
	bl	printk
	b	ret_from_except
86:	.string	"floating point used in kernel (task=%p, pc=%x)\n"
	.align	4

#ifdef CONFIG_ALTIVEC
/* Note that the AltiVec support is closely modeled after the FP
 * support.  Changes to one are likely to be applicable to the
 * other!  */
load_up_altivec:
/*
 * Disable AltiVec for the task which had AltiVec previously,
 * and save its AltiVec registers in its thread_struct.
 * Enables AltiVec for use in the kernel on return.
 * On SMP we know the AltiVec units are free, since we give it up every
 * switch.  -- Kumar
 */
	mfmsr	r5
	oris	r5,r5,MSR_VEC@h
	mtmsr	r5			/* enable use of AltiVec now */
	isync
/*
 * For SMP, we don't do lazy AltiVec switching because it just gets too
 * horrendously complex, especially when a task switches from one CPU
 * to another.  Instead we call giveup_altivec in switch_to.
 */
#ifndef CONFIG_SMP
#ifndef CONFIG_APUS
	lis	r6,-KERNELBASE@h
#else
	lis	r6,CYBERBASEp@h
	lwz	r6,0(r6)
#endif
	addis	r3,r6,last_task_used_altivec@ha
	lwz	r4,last_task_used_altivec@l(r3)
	cmpi	0,r4,0
	beq	1f
	add	r4,r4,r6
	addi	r4,r4,THREAD	/* want THREAD of last_task_used_altivec */
	SAVE_32VR(0,r20,r4)
	MFVSCR(vr0)
	li	r20,THREAD_VSCR
	STVX(vr0,r20,r4)
	lwz	r5,PT_REGS(r4)
	add	r5,r5,r6
	lwz	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	lis	r20,MSR_VEC@h
	andc	r4,r4,r20	/* disable altivec for previous task */
	stw	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#endif /* CONFIG_SMP */
	/* enable use of AltiVec after return */
	oris	r23,r23,MSR_VEC@h
	mfspr	r5,SPRG3		/* current task's THREAD (phys) */
	li	r4,1
	li	r20,THREAD_VSCR
	stw	r4,THREAD_USED_VR(r5)
	LVX(vr0,r20,r5)
	MTVSCR(vr0)
	REST_32VR(0,r20,r5)
#ifndef CONFIG_SMP
	subi	r4,r5,THREAD
	sub	r4,r4,r6
	stw	r4,last_task_used_altivec@l(r3)
#endif /* CONFIG_SMP */
	/* restore registers and return */
	lwz	r3,_CCR(r21)
	lwz	r4,_LINK(r21)
	mtcrf	0xff,r3
	mtlr	r4
	REST_GPR(1, r21)
	REST_4GPRS(3, r21)
	/* we haven't used ctr or xer */
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	REST_GPR(20, r21)
	REST_2GPRS(22, r21)
	lwz	r21,GPR21(r21)
	SYNC
	RFI

/*
 * AltiVec unavailable trap from kernel - print a message, but let
 * the task use AltiVec in the kernel until it returns to user mode.
 */
KernelAltiVec:
	lwz	r3,_MSR(r1)
	oris	r3,r3,MSR_VEC@h
	stw	r3,_MSR(r1)	/* enable use of AltiVec after return */
	lis	r3,87f@h
	ori	r3,r3,87f@l
	mr	r4,r2		/* current */
	lwz	r5,_NIP(r1)
	bl	printk
	b	ret_from_except
87:	.string	"AltiVec used in kernel  (task=%p, pc=%x)  \n"
	.align	4

/*
 * giveup_altivec(tsk)
 * Disable AltiVec for the task given as the argument,
 * and save the AltiVec registers in its thread_struct.
 * Enables AltiVec for use in the kernel on return.
 */

	.globl	giveup_altivec
giveup_altivec:
	mfmsr	r5
	oris	r5,r5,MSR_VEC@h
	SYNC
	mtmsr	r5			/* enable use of AltiVec now */
	isync
	cmpi	0,r3,0
	beqlr-				/* if no previous owner, done */
	addi	r3,r3,THREAD		/* want THREAD of task */
	lwz	r5,PT_REGS(r3)
	cmpi	0,r5,0
	SAVE_32VR(0, r4, r3)
	MFVSCR(vr0)
	li	r4,THREAD_VSCR
	STVX(vr0, r4, r3)
	beq	1f
	lwz	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	lis	r3,MSR_VEC@h
	andc	r4,r4,r3		/* disable AltiVec for previous task */
	stw	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#ifndef CONFIG_SMP
	li	r5,0
	lis	r4,last_task_used_altivec@ha
	stw	r5,last_task_used_altivec@l(r4)
#endif /* CONFIG_SMP */
	blr
#endif /* CONFIG_ALTIVEC */

/*
 * giveup_fpu(tsk)
 * Disable FP for the task given as the argument,
 * and save the floating-point registers in its thread_struct.
 * Enables the FPU for use in the kernel on return.
 */
	.globl	giveup_fpu
giveup_fpu:
	mfmsr	r5
	ori	r5,r5,MSR_FP
	SYNC_601
	ISYNC_601
	mtmsr	r5			/* enable use of fpu now */
	SYNC_601
	isync
	cmpi	0,r3,0
	beqlr-				/* if no previous owner, done */
	addi	r3,r3,THREAD	        /* want THREAD of task */
	lwz	r5,PT_REGS(r3)
	cmpi	0,r5,0
	SAVE_32FPRS(0, r3)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR-4(r3)
	beq	1f
	lwz	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r3,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r3		/* disable FP for previous task */
	stw	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#ifndef CONFIG_SMP
	li	r5,0
	lis	r4,last_task_used_math@ha
	stw	r5,last_task_used_math@l(r4)
#endif /* CONFIG_SMP */
	blr

/*
 * This code is jumped to from the startup code to copy
 * the kernel image to physical address 0.
 */
relocate_kernel:
	addis	r9,r26,klimit@ha	/* fetch klimit */
	lwz	r25,klimit@l(r9)
	addis	r25,r25,-KERNELBASE@h
	li	r3,0			/* Destination base address */
	li	r6,0			/* Destination offset */
	li	r5,0x4000		/* # bytes of memory to copy */
	bl	copy_and_flush		/* copy the first 0x4000 bytes */
	addi	r0,r3,4f@l		/* jump to the address of 4f */
	mtctr	r0			/* in copy and do the rest. */
	bctr				/* jump to the copy */
4:	mr	r5,r25
	bl	copy_and_flush		/* copy the rest */
	b	turn_on_mmu

/*
 * Copy routine used to copy the kernel to start at physical address 0
 * and flush and invalidate the caches as needed.
 * r3 = dest addr, r4 = source addr, r5 = copy limit, r6 = start offset
 * on exit, r3, r4, r5 are unchanged, r6 is updated to be >= r5.
 */
copy_and_flush:
	addi	r5,r5,-4
	addi	r6,r6,-4
4:	li	r0,L1_CACHE_LINE_SIZE/4
	mtctr	r0
3:	addi	r6,r6,4			/* copy a cache line */
	lwzx	r0,r6,r4
	stwx	r0,r6,r3
	bdnz	3b
	dcbst	r6,r3			/* write it to memory */
	sync
	icbi	r6,r3			/* flush the icache line */
	cmplw	0,r6,r5
	blt	4b
	sync				/* additional sync needed on g4 */
	isync
	addi	r5,r5,4
	addi	r6,r6,4
	blr

#ifdef CONFIG_APUS
/*
 * On APUS the physical base address of the kernel is not known at compile
 * time, which means the __pa/__va constants used are incorrect. In the
 * __init section is recorded the virtual addresses of instructions using
 * these constants, so all that has to be done is fix these before
 * continuing the kernel boot.
 *
 * r4 = The physical address of the kernel base.
 */
fix_mem_constants:
	mr	r10,r4
	addis	r10,r10,-KERNELBASE@h    /* virt_to_phys constant */
	neg	r11,r10	                 /* phys_to_virt constant */

	lis	r12,__vtop_table_begin@h
	ori	r12,r12,__vtop_table_begin@l
	add	r12,r12,r10	         /* table begin phys address */
	lis	r13,__vtop_table_end@h
	ori	r13,r13,__vtop_table_end@l
	add	r13,r13,r10	         /* table end phys address */
	subi	r12,r12,4
	subi	r13,r13,4
1:	lwzu	r14,4(r12)               /* virt address of instruction */
	add     r14,r14,r10              /* phys address of instruction */
	lwz     r15,0(r14)               /* instruction, now insert top */
	rlwimi  r15,r10,16,16,31         /* half of vp const in low half */
	stw	r15,0(r14)               /* of instruction and restore. */
	dcbst	r0,r14			 /* write it to memory */
	sync
	icbi	r0,r14			 /* flush the icache line */
	cmpw	r12,r13
	bne     1b
	sync				/* additional sync needed on g4 */
	isync

/*
 * Map the memory where the exception handlers will
 * be copied to when hash constants have been patched.
 */
#ifdef CONFIG_APUS_FAST_EXCEPT
	lis	r8,0xfff0
#else
	lis	r8,0
#endif
	ori	r8,r8,0x2		/* 128KB, supervisor */
	mtspr	DBAT3U,r8
	mtspr	DBAT3L,r8

	lis	r12,__ptov_table_begin@h
	ori	r12,r12,__ptov_table_begin@l
	add	r12,r12,r10	         /* table begin phys address */
	lis	r13,__ptov_table_end@h
	ori	r13,r13,__ptov_table_end@l
	add	r13,r13,r10	         /* table end phys address */
	subi	r12,r12,4
	subi	r13,r13,4
1:	lwzu	r14,4(r12)               /* virt address of instruction */
	add     r14,r14,r10              /* phys address of instruction */
	lwz     r15,0(r14)               /* instruction, now insert top */
	rlwimi  r15,r11,16,16,31         /* half of pv const in low half*/
	stw	r15,0(r14)               /* of instruction and restore. */
	dcbst	r0,r14			 /* write it to memory */
	sync
	icbi	r0,r14			 /* flush the icache line */
	cmpw	r12,r13
	bne     1b

	sync				/* additional sync needed on g4 */
	isync				/* No speculative loading until now */
	blr

/***********************************************************************
 *  Please note that on APUS the exception handlers are located at the
 *  physical address 0xfff0000. For this reason, the exception handlers
 *  cannot use relative branches to access the code below.
 ***********************************************************************/
#endif /* CONFIG_APUS */

#ifdef CONFIG_SMP
#ifdef CONFIG_GEMINI
	.globl	__secondary_start_gemini
__secondary_start_gemini:
        mfspr   r4,HID0
        ori     r4,r4,HID0_ICFI
        li      r3,0
        ori     r3,r3,HID0_ICE
        andc    r4,r4,r3
        mtspr   HID0,r4
        sync
        b       __secondary_start
#endif /* CONFIG_GEMINI */
	.globl	__secondary_start_psurge
__secondary_start_psurge:
	li	r24,1			/* cpu # */
	b	__secondary_start_psurge99
	.globl	__secondary_start_psurge2
__secondary_start_psurge2:
	li	r24,2			/* cpu # */
	b	__secondary_start_psurge99
	.globl	__secondary_start_psurge3
__secondary_start_psurge3:
	li	r24,3			/* cpu # */
	b	__secondary_start_psurge99
__secondary_start_psurge99:
	/* we come in here with IR=0 and DR=1, and DBAT 0
	   set to map the 0xf0000000 - 0xffffffff region */
	mfmsr	r0
	rlwinm	r0,r0,0,28,26		/* clear DR (0x10) */
	SYNC
	mtmsr	r0
	isync

	.globl	__secondary_start
__secondary_start:
#ifdef CONFIG_PPC64BRIDGE
	mfmsr	r0
	clrldi	r0,r0,1		/* make sure it's in 32-bit mode */
	SYNC
	MTMSRD(r0)
	isync
#endif
	/* Copy some CPU settings from CPU 0 */
	bl	__restore_cpu_setup

	lis	r3,-KERNELBASE@h
	mr	r4,r24
	bl	identify_cpu
	bl	call_setup_cpu		/* Call setup_cpu for this CPU */
#ifdef CONFIG_6xx
	lis	r3,-KERNELBASE@h
	bl	init_idle_6xx
#endif /* CONFIG_6xx */

	/* get current */
	lis	r2,current_set@h
	ori	r2,r2,current_set@l
	tophys(r2,r2)
	slwi	r24,r24,2		/* get current_set[cpu#] */
	lwzx	r2,r2,r24

	/* stack */
	addi	r1,r2,TASK_UNION_SIZE-STACK_FRAME_OVERHEAD
	li	r0,0
	tophys(r3,r1)
	stw	r0,0(r3)

	/* load up the MMU */
	bl	load_up_mmu

	/* ptr to phys current thread */
	tophys(r4,r2)
	addi	r4,r4,THREAD	/* phys address of our thread_struct */
	CLR_TOP32(r4)
	mtspr	SPRG3,r4
	li	r3,0
	mtspr	SPRG2,r3	/* 0 => r1 has kernel sp */
	stw	r3,PT_REGS(r4)	/* set thread.regs to 0 for kernel thread */

	/* enable MMU and jump to start_secondary */
	li	r4,MSR_KERNEL
	FIX_SRR1(r4,r5)
	lis	r3,start_secondary@h
	ori	r3,r3,start_secondary@l
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	SYNC
	RFI
#endif /* CONFIG_SMP */

/*
 * Those generic dummy functions are kept for CPUs not
 * included in CONFIG_6xx
 */
_GLOBAL(__setup_cpu_power3)
	blr
_GLOBAL(__setup_cpu_power4)
	blr
_GLOBAL(__setup_cpu_ppc970)
	blr
_GLOBAL(__setup_cpu_generic)
	blr

#ifndef CONFIG_6xx
_GLOBAL(__save_cpu_setup)
	blr
_GLOBAL(__restore_cpu_setup)
#ifdef CONFIG_POWER4
	/* turn off real-mode cache inhibit on the PPC970 */
	mfspr	r0,SPRN_PVR
	srwi	r0,r0,16
	cmpwi	r0,0x39
	beq	ppc970_setup_hid
	blr
#endif
#endif /* CONFIG_6xx */

/*
 * Load stuff into the MMU.  Intended to be called with
 * IR=0 and DR=0.
 */
load_up_mmu:
	sync			/* Force all PTE updates to finish */
	isync
	tlbia			/* Clear all TLB entries */
	sync			/* wait for tlbia/tlbie to finish */
	TLBSYNC			/* ... on all CPUs */
	/* Load the SDR1 register (hash table base & size) */
	lis	r6,_SDR1@ha
	tophys(r6,r6)
	lwz	r6,_SDR1@l(r6)
	mtspr	SDR1,r6
#ifdef CONFIG_PPC64BRIDGE
	/* clear the ASR so we only use the pseudo-segment registers. */
	li	r6,0
	mtasr	r6
#endif /* CONFIG_PPC64BRIDGE */
	li	r0,16		/* load up segment register values */
	mtctr	r0		/* for context 0 */
	lis	r3,0x2000	/* Ku = 1, VSID = 0 */
	li	r4,0
3:	mtsrin	r3,r4
	addi	r3,r3,0x111	/* increment VSID */
	addis	r4,r4,0x1000	/* address of next segment */
	bdnz	3b
#ifndef CONFIG_POWER4
/* Load the BAT registers with the values set up by MMU_init.
   MMU_init takes care of whether we're on a 601 or not. */
	mfpvr	r3
	srwi	r3,r3,16
	cmpwi	r3,1
	lis	r3,BATS@ha
	addi	r3,r3,BATS@l
	tophys(r3,r3)
	LOAD_BAT(0,r3,r4,r5)
	LOAD_BAT(1,r3,r4,r5)
	LOAD_BAT(2,r3,r4,r5)
	LOAD_BAT(3,r3,r4,r5)
#endif /* CONFIG_POWER4 */
	blr

/*
 * This is where the main kernel code starts.
 */
start_here:
	/* ptr to current */
	lis	r2,init_task_union@h
	ori	r2,r2,init_task_union@l
	/* Set up for using our exception vectors */
	/* ptr to phys current thread */
	tophys(r4,r2)
	addi	r4,r4,THREAD	/* init task's THREAD */
	CLR_TOP32(r4)
	mtspr	SPRG3,r4
	li	r3,0
	mtspr	SPRG2,r3	/* 0 => r1 has kernel sp */

	/* stack */
	addi	r1,r2,TASK_UNION_SIZE
	li	r0,0
	stwu	r0,-STACK_FRAME_OVERHEAD(r1)
/*
 * Do early bootinfo parsing, platform-specific initialization,
 * and set up the MMU.
 */
	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27
	bl	machine_init
	bl	MMU_init

#ifdef CONFIG_APUS
	/* Copy exception code to exception vector base on APUS. */
	lis	r4,KERNELBASE@h
#ifdef CONFIG_APUS_FAST_EXCEPT
	lis	r3,0xfff0		/* Copy to 0xfff00000 */
#else
	lis	r3,0			/* Copy to 0x00000000 */
#endif
	li	r5,0x4000		/* # bytes of memory to copy */
	li	r6,0
	bl	copy_and_flush		/* copy the first 0x4000 bytes */
#endif  /* CONFIG_APUS */

/*
 * Go back to running unmapped so we can load up new values
 * for SDR1 (hash table pointer) and the segment registers
 * and change to using our exception vectors.
 */
	lis	r4,2f@h
	ori	r4,r4,2f@l
	tophys(r4,r4)
	li	r3,MSR_KERNEL & ~(MSR_IR|MSR_DR)
	FIX_SRR1(r3,r5)
	mtspr	SRR0,r4
	mtspr	SRR1,r3
	SYNC
	RFI
/* Load up the kernel context */
2:	bl	load_up_mmu

#ifdef CONFIG_BDI_SWITCH
	/* Add helper information for the Abatron bdiGDB debugger.
	 * We do this here because we know the mmu is disabled, and
	 * will be enabled for real in just a few instructions.
	 */
	lis	r5, abatron_pteptrs@h
	ori	r5, r5, abatron_pteptrs@l
	stw	r5, 0xf0(r0)	/* This much match your Abatron config */
	lis	r6, swapper_pg_dir@h
	ori	r6, r6, swapper_pg_dir@l
	tophys(r5, r5)
	stw	r6, 0(r5)
#endif

/* Now turn on the MMU for real! */
	li	r4,MSR_KERNEL
	FIX_SRR1(r4,r5)
	lis	r3,start_kernel@h
	ori	r3,r3,start_kernel@l
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	SYNC
	RFI

/*
 * Set up the segment registers for a new context.
 */
_GLOBAL(set_context)
	mulli	r3,r3,897	/* multiply context by skew factor */
	rlwinm	r3,r3,4,8,27	/* VSID = (context & 0xfffff) << 4 */
	addis	r3,r3,0x6000	/* Set Ks, Ku bits */
	li	r0,NUM_USER_SEGMENTS
	mtctr	r0

#ifdef CONFIG_BDI_SWITCH
	/* Context switch the PTE pointer for the Abatron BDI2000.
	 * The PGDIR is passed as second argument.
	 */
	lis	r5, KERNELBASE@h
	lwz	r5, 0xf0(r5)
	stw	r4, 0x4(r5)
#endif

	li	r4,0
BEGIN_FTR_SECTION
	DSSALL
	sync
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
3:	isync
#ifdef CONFIG_PPC64BRIDGE
	slbie	r4
#endif /* CONFIG_PPC64BRIDGE */
	mtsrin	r3,r4
	addi	r3,r3,0x111	/* next VSID */
	rlwinm	r3,r3,0,8,3	/* clear out any overflow from VSID field */
	addis	r4,r4,0x1000	/* address of next segment */
	bdnz	3b
	sync
	isync
	blr

/*
 * An undocumented "feature" of 604e requires that the v bit
 * be cleared before changing BAT values.
 *
 * Also, newer IBM firmware does not clear bat3 and 4 so
 * this makes sure it's done.
 *  -- Cort
 */
clear_bats:
	li	r20,0
	mfspr	r9,PVR
	rlwinm	r9,r9,16,16,31		/* r9 = 1 for 601, 4 for 604 */
	cmpwi	r9, 1
	beq	1f

	mtspr	DBAT0U,r20
	mtspr	DBAT0L,r20
	mtspr	DBAT1U,r20
	mtspr	DBAT1L,r20
	mtspr	DBAT2U,r20
	mtspr	DBAT2L,r20
	mtspr	DBAT3U,r20
	mtspr	DBAT3L,r20
1:
	mtspr	IBAT0U,r20
	mtspr	IBAT0L,r20
	mtspr	IBAT1U,r20
	mtspr	IBAT1L,r20
	mtspr	IBAT2U,r20
	mtspr	IBAT2L,r20
	mtspr	IBAT3U,r20
	mtspr	IBAT3L,r20
BEGIN_FTR_SECTION
	/* Here's a tweak: at this point, CPU setup have
	 * not been called yet, so HIGH_BAT_EN may not be
	 * set in HID0 for the 745x processors. However, it
	 * seems that doesn't affect our ability to actually
	 * write to these SPRs.
	 */
	mtspr	SPRN_DBAT4U,r20
	mtspr	SPRN_DBAT4L,r20
	mtspr	SPRN_DBAT5U,r20
	mtspr	SPRN_DBAT5L,r20
	mtspr	SPRN_DBAT6U,r20
	mtspr	SPRN_DBAT6L,r20
	mtspr	SPRN_DBAT7U,r20
	mtspr	SPRN_DBAT7L,r20
	mtspr	SPRN_IBAT4U,r20
	mtspr	SPRN_IBAT4L,r20
	mtspr	SPRN_IBAT5U,r20
	mtspr	SPRN_IBAT5L,r20
	mtspr	SPRN_IBAT6U,r20
	mtspr	SPRN_IBAT6L,r20
	mtspr	SPRN_IBAT7U,r20
	mtspr	SPRN_IBAT7L,r20
END_FTR_SECTION_IFSET(CPU_FTR_HAS_HIGH_BATS)
	blr

flush_tlbs:
	lis	r20, 0x40
1:	addic.	r20, r20, -0x1000
	tlbie	r20
	blt	1b
	sync
	blr

mmu_off:
 	addi	r4, r3, __after_mmu_off - _start
	mfmsr	r3
	andi.	r0,r3,MSR_DR|MSR_IR		/* MMU enabled? */
	beqlr
	andc	r3,r3,r0
	mtspr	SRR0,r4
	mtspr	SRR1,r3
	sync
	RFI

#ifndef CONFIG_POWER4
/*
 * Use the first pair of BAT registers to map the 1st 16MB
 * of RAM to KERNELBASE.  From this point on we can't safely
 * call OF any more.
 */
initial_bats:
	lis	r11,KERNELBASE@h
#ifndef CONFIG_PPC64BRIDGE
	mfspr	r9,PVR
	rlwinm	r9,r9,16,16,31		/* r9 = 1 for 601, 4 for 604 */
	cmpi	0,r9,1
	bne	4f
	ori	r11,r11,4		/* set up BAT registers for 601 */
	li	r8,0x7f			/* valid, block length = 8MB */
	oris	r9,r11,0x800000@h	/* set up BAT reg for 2nd 8M */
	oris	r10,r8,0x800000@h	/* set up BAT reg for 2nd 8M */
	mtspr	IBAT0U,r11		/* N.B. 601 has valid bit in */
	mtspr	IBAT0L,r8		/* lower BAT register */
	mtspr	IBAT1U,r9
	mtspr	IBAT1L,r10
	isync
	blr
#endif /* CONFIG_PPC64BRIDGE */

4:	tophys(r8,r11)
#ifdef CONFIG_SMP
	ori	r8,r8,0x12		/* R/W access, M=1 */
#else
	ori	r8,r8,2			/* R/W access */
#endif /* CONFIG_SMP */
#ifdef CONFIG_APUS
	ori	r11,r11,BL_8M<<2|0x2	/* set up 8MB BAT registers for 604 */
#else
	ori	r11,r11,BL_256M<<2|0x2	/* set up BAT registers for 604 */
#endif /* CONFIG_APUS */

#ifdef CONFIG_PPC64BRIDGE
	/* clear out the high 32 bits in the BAT */
	clrldi	r11,r11,32
	clrldi	r8,r8,32
#endif /* CONFIG_PPC64BRIDGE */
	mtspr	DBAT0L,r8		/* N.B. 6xx (not 601) have valid */
	mtspr	DBAT0U,r11		/* bit in upper BAT register */
	mtspr	IBAT0L,r8
	mtspr	IBAT0U,r11
	isync
	blr

#if !defined(CONFIG_APUS) && defined(CONFIG_BOOTX_TEXT)
setup_disp_bat:
	/*
	 * setup the display bat prepared for us in prom.c
	 */
	mflr	r8
	bl	reloc_offset
	mtlr	r8
	addis	r8,r3,disp_BAT@ha
	addi	r8,r8,disp_BAT@l
	lwz	r11,0(r8)
	lwz	r8,4(r8)
	mfspr	r9,PVR
	rlwinm	r9,r9,16,16,31		/* r9 = 1 for 601, 4 for 604 */
	cmpi	0,r9,1
	beq	1f
	mtspr	DBAT3L,r8
	mtspr	DBAT3U,r11
	blr
1:	mtspr	IBAT3L,r8
	mtspr	IBAT3U,r11
	blr

#endif /* !defined(CONFIG_APUS) && defined(CONFIG_BOOTX_TEXT) */

#else /* CONFIG_POWER4 */
ppc970_setup_hid:
	li	r0,0
	sync
	mtspr	0x3f4,r0
	isync
	sync
	mtspr	0x3f6,r0
	isync
	mfspr	r0,SPRN_HID0
	li	r11,5			/* clear DOZE and SLEEP */
	rldimi	r0,r11,52,8		/* and set NAP and DPM */
	mtspr	SPRN_HID0,r0
	mfspr	r0,SPRN_HID0
	mfspr	r0,SPRN_HID0
	mfspr	r0,SPRN_HID0
	mfspr	r0,SPRN_HID0
	mfspr	r0,SPRN_HID0
	mfspr	r0,SPRN_HID0
	sync
	isync
	mfspr	r0,SPRN_HID1
	li	r11,0x1200		/* enable i-fetch cacheability */
	sldi	r11,r11,44		/* and prefetch */
	or	r0,r0,r11
	mtspr	SPRN_HID1,r0
	mtspr	SPRN_HID1,r0
	isync
	li	r0,0
	sync
	mtspr	0x137,0
	isync
	blr
#endif /* CONFIG_POWER4 */

#ifdef CONFIG_8260
/* Jump into the system reset for the rom.
 * We first disable the MMU, and then jump to the ROM reset address.
 *
 * r3 is the board info structure, r4 is the location for starting.
 * I use this for building a small kernel that can load other kernels,
 * rather than trying to write or rely on a rom monitor that can tftp load.
 */
       .globl  m8260_gorom
m8260_gorom:
	mfmsr	r0
	rlwinm	r0,r0,0,17,15	/* clear MSR_EE in r0 */
	sync
	mtmsr	r0
	sync
	mfspr	r11, HID0
	lis	r10, 0
	ori	r10,r10,HID0_ICE|HID0_DCE
	andc	r11, r11, r10
	mtspr	HID0, r11
	isync
	li	r5, MSR_
	lis	r6,2f@h
	addis	r6,r6,-KERNELBASE@h
	ori	r6,r6,2f@l
	mtspr	SRR0,r6
	mtspr	SRR1,r5
	isync
	sync
	rfi
2:
	mtlr	r4
	blr
#endif


/*
 * We put a few things here that have to be page-aligned.
 * This stuff goes at the beginning of the data segment,
 * which is page-aligned.
 */
	.data
	.globl	sdata
sdata:
	.globl	empty_zero_page
empty_zero_page:
	.space	4096

	.globl	swapper_pg_dir
swapper_pg_dir:
	.space	4096

/*
 * This space gets a copy of optional info passed to us by the bootstrap
 * Used to pass parameters into the kernel like root=/dev/sda1, etc.
 */
	.globl	cmd_line
cmd_line:
	.space	512

	.globl intercept_table
intercept_table:
	.long 0, 0, i0x200, i0x300, i0x400, 0, i0x600, i0x700
	.long i0x800, 0, 0, 0, 0, i0xd00, 0, 0
	.long 0, 0, 0, i0x1300, 0, 0, 0, 0
	.long 0, 0, 0, 0, 0, 0, 0, 0
	.long 0, 0, 0, 0, 0, 0, 0, 0
	.long 0, 0, 0, 0, 0, 0, 0, 0

#ifdef CONFIG_BDI_SWITCH
/* Room for two PTE pointers, usually the kernel and current user pointers
 * to their respective root page table.
 */
abatron_pteptrs:
	.space	8
#endif
