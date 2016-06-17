/*
 *    Copyright (c) 1995-1996 Gary Thomas <gdt@linuxppc.org>
 *      Initial PowerPC version.
 *    Copyright (c) 1996 Cort Dougan <cort@cs.nmt.edu>
 *      Rewritten for PReP
 *    Copyright (c) 1996 Paul Mackerras <paulus@cs.anu.edu.au>
 *      Low-level exception handers, MMU support, and rewrite.
 *    Copyright (c) 1997 Dan Malek <dmalek@jlc.net>
 *      PowerPC 8xx modifications.
 *    Copyright (c) 1998-1999 TiVo, Inc.
 *      PowerPC 403GCX modifications.
 *    Copyright (c) 1999 Grant Erickson <grant@lcse.umn.edu>
 *      PowerPC 403GCX/405GP modifications.
 *    Copyright 2000 MontaVista Software Inc.
 *	PPC405 modifications
 *      PowerPC 403GCX/405GP modifications.
 * 	Author: MontaVista Software, Inc.
 *         	frank_rowand@mvista.com or source@mvista.com
 * 	   	debbie_chu@mvista.com
 *
 *
 *    Module name: head_4xx.S
 *
 *    Description:
 *      Kernel execution entry point code.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/ibm4xx.h>
#include <asm/cputable.h>
#include <asm/ppc_asm.h>
#include "ppc_defs.h"

/* Preprocessor Defines */

#define	STND_EXC	0
#define	CRIT_EXC	1

/* As with the other PowerPC ports, it is expected that when code
 * execution begins here, the following registers contain valid, yet
 * optional, information:
 *
 *   r3 - Board info structure pointer (DRAM, frequency, MAC address, etc.)
 *   r4 - Starting address of the init RAM disk
 *   r5 - Ending address of the init RAM disk
 *   r6 - Start of kernel command line string (e.g. "mem=96m")
 *   r7 - End of kernel command line string
 *
 * This is all going to change RSN when we add bi_recs.......  -- Dan
 */
	.text
_GLOBAL(_stext)
_GLOBAL(_start)

	/* To accomodate some SMP systems that overwrite the first few
	 * locations before cpu 0 starts, the bootloader starts us at 0xc.
	 */
	nop
	nop
	nop

	/* Save parameters we are passed.
	*/
	mr	r31,r3
	mr	r30,r4
	mr	r29,r5
	mr	r28,r6
	mr	r27,r7
	li	r24,0		/* CPU number */

	/* We have to turn on the MMU right away so we get cache modes
	 * set correctly.
	 */
	bl	initial_mmu

/* We now have the lower 16 Meg mapped into TLB entries, and the caches
 * ready to work.
 */
turn_on_mmu:
	li	r0,MSR_KERNEL
	mtspr	SRR1,r0
	lis	r0,start_here@h
	ori	r0,r0,start_here@l
	mtspr	SRR0,r0
	SYNC
	rfi				/* enables MMU */
	b	.			/* prevent prefetch past rfi */

#ifdef CONFIG_405LP
	/* Reserve space for ibm405lp_wakeup_info.  This contains the physical
	 * address of a struct ibm405lp_wakeup_info which is used by the
	 * firmware to pass control back to the kernel on cryo wakeup.  Because
	 * the firmware uses this, the absolute address is fixed: don't try
	 * changing it. */
	. = 0xfc
	.global ibm405lp_wakeup_info
ibm405lp_wakeup_info:
	.long	0x0
#endif

/* Exception vector entry code. This code runs with address translation
 * turned off (i.e. using physical addresses). We assume SPRG3 has the
 * physical address of the current task thread_struct.
 */

#define COMMON_PROLOG							     \
0:	mtspr	SPRN_SPRG0,r20;         /* We need r20, move it to SPRG0   */\
	mtspr	SPRN_SPRG1,r21;         /* We need r21, move it to SPRG1   */\
	mfcr	r20;                    /* We need the CR, move it to r20  */\
	mfspr	r21,SPRN_SPRG2;         /* Exception stack to use          */\
	cmpwi	cr0,r21,0;              /* From user mode or RTAS?         */\
	bne	1f;                     /* Not RTAS, branch                */\
	tophys(r21, r1);                /* Convert vka in r1 to pka in r21 */\
	subi	r21,r21,INT_FRAME_SIZE; /* Allocate an exception frame     */\
1:	stw	r20,_CCR(r21);          /* Save CR on the stack            */\
	stw	r22,GPR22(r21);         /* Save r22 on the stack           */\
	stw	r23,GPR23(r21);         /* r23 Save on the stack           */\
	mfspr	r20,SPRN_SPRG0;         /* Get r20 back out of SPRG0       */\
	stw	r20,GPR20(r21);         /* Save r20 on the stack           */\
	mfspr	r22,SPRN_SPRG1;         /* Get r21 back out of SPRG0       */\
	stw	r22,GPR21(r21);         /* Save r21 on the stack           */\
	mflr	r20;                                                         \
	stw	r20,_LINK(r21);         /* Save LR on the stack            */\
	mfctr	r22;                                                         \
	stw	r22,_CTR(r21);          /* Save CTR on the stack           */\
	mfspr	r20,XER;                                                     \
	stw	r20,_XER(r21);          /* Save XER on the stack           */

#define	COMMON_EPILOG							     \
	stw	r0,GPR0(r21);		/* Save r0 on the stack		   */\
	stw	r1,GPR1(r21);		/* Save r1 on the stack		   */\
	stw	r2,GPR2(r21);		/* Save r2 on the stack		   */\
	stw	r1,0(r21);						     \
	tovirt(r1,r21);			/* Set-up new kernel stack pointer */\
	SAVE_4GPRS(3, r21);		/* Save r3 through r6 on the stack */\
	SAVE_GPR(7, r21);		/* Save r7 on the stack		   */

#define	STND_EXCEPTION_PROLOG						     \
	COMMON_PROLOG;							     \
	mfspr	r22,SPRN_SRR0;		/* Faulting instruction address	   */\
	lis	r20,MSR_WE@h;						     \
	mfspr	r23,SPRN_SRR1;		/* MSR at the time of fault	   */\
	andc	r23,r23,r20;		/* disable processor wait state    */\
	COMMON_EPILOG;

#define	CRIT_EXCEPTION_PROLOG						     \
	COMMON_PROLOG;							     \
	mfspr	r22,SPRN_SRR2;		/* Faulting instruction address	   */\
	lis	r20,MSR_WE@h;						     \
	mfspr	r23,SPRN_SRR3;		/* MSR at the time of fault	   */\
	andc	r23,r23,r20;		/* disable processor wait state    */\
	COMMON_EPILOG;


#define	START_EXCEPTION(n, label)					     \
	. = n;								     \
label:


#define FINISH_EXCEPTION(func)						     \
	bl	transfer_to_handler;					     \
	.long	func;							     \
	.long	ret_from_except


#define STND_EXCEPTION(n, label, func)					     \
	START_EXCEPTION(n, label);					     \
	STND_EXCEPTION_PROLOG;						     \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				     \
	li	r7,STND_EXC;						     \
	li	r20,MSR_KERNEL;						     \
	FINISH_EXCEPTION(func)


#define	CRIT_EXCEPTION(n, label, func)					     \
	START_EXCEPTION(n, label);					     \
	CRIT_EXCEPTION_PROLOG;						     \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				     \
	li	r7,CRIT_EXC;						     \
	li	r20,MSR_KERNEL;						     \
	FINISH_EXCEPTION(func)


/* Exception vectors.
*/

/* 0x0100 - Critical Interrupt Exception
*/
	CRIT_EXCEPTION(0x0100,	CriticalInterrupt,	UnknownException)

/* 0x0200 - Machine Check Exception
*/
	START_EXCEPTION(0x0200, MachineCheck)
	CRIT_EXCEPTION_PROLOG

	/*
	lis	r4,0x0400
	mtdcr	DCRN_POB0_BESR0,r4
	*/
#ifdef DCRN_POB0_BEAR
	mfdcr	r4,DCRN_POB0_BEAR
	mfdcr	r4,DCRN_POB0_BESR0
	mfdcr	r4,DCRN_POB0_BESR1
#endif

#ifdef DCRN_PLB0_BEAR
	mfdcr	r4,DCRN_PLB0_ACR
	mfdcr	r4,DCRN_PLB0_BEAR
	mfdcr	r4,DCRN_PLB0_BESR
#endif

	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,CRIT_EXC
	li	r20,MSR_KERNEL
	FINISH_EXCEPTION(MachineCheckException)

/* 0x0300 - Data Storage Exception
 * This happens for just a few reasons.  U0 set (but we don't do that),
 * or zone protection fault (user violation, write to protected page).
 * If this is just an update of modified status, we do that quickly
 * and exit.  Otherwise, we call heavywight functions to do the work.
 */
	START_EXCEPTION(0x0300,	DataStore)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
#ifdef CONFIG_403GCX
	stw     r22, 0(r0)
	stw     r23, 4(r0)
	mfcr    r21
	mfspr   r22, SPRN_PID
	stw     r21, 8(r0)
	stw     r22, 12(r0)
#else
	mtspr	SPRG4, r22
	mtspr	SPRG5, r23
	mfcr	r21
	mfspr	r22, SPRN_PID
	mtspr	SPRG7, r21
	mtspr	SPRG6, r22
#endif

	/* First, check if it was a zone fault (which means a user
	* tried to access a kernel or read-protected page - always
	* a SEGV).  All other faults here must be stores, so no
	* need to check ESR_DST as well. */
	mfspr	r20, SPRN_ESR
	andis.	r20, r20, ESR_DIZ@h
	bne	2f

	mfspr	r20, SPRN_DEAR		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l
	li	r23, 0
	mtspr	SPRN_PID, r23		/* TLB will have 0 TID */
	b	4f

	/* Get the PGD for the current thread.
	 */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)
4:
	tophys(r21, r21)
	rlwimi	r21, r20, 12, 20, 29	/* Create L1 (pgdir/pmd) address */
	lwz	r21, 0(r21)		/* Get L1 entry */
	rlwinm.	r22, r21, 0, 0, 19	/* Extract L2 (pte) base address */
	beq	2f			/* Bail if no table */

	tophys(r22, r22)
	rlwimi	r22, r20, 22, 20, 29	/* Compute PTE address */
	lwz	r21, 0(r22)		/* Get Linux PTE */

	andi.	r23, r21, _PAGE_RW	/* Is it writeable? */
	beq	2f			/* Bail if not */

	/* Update 'changed'.
	*/
	ori	r21, r21, _PAGE_DIRTY|_PAGE_ACCESSED|_PAGE_HWWRITE
	stw	r21, 0(r22)		/* Update Linux page table */

	/* Most of the Linux PTE is ready to load into the TLB LO.
	 * We set ZSEL, where only the LS-bit determines user access.
	 * We set execute, because we don't have the granularity to
	 * properly set this at the page level (Linux problem).
	 * If shared is set, we cause a zero PID->TID load.
	 * Many of these bits are software only.  Bits we don't set
	 * here we (properly should) assume have the appropriate value.
	 */
	li	r22, 0x0ce2
	andc	r21, r21, r22		/* Make sure 20, 21 are zero */
	ori	r21, r21, _PAGE_HWEXEC	/* make it executable */

	/* find the TLB index that caused the fault.  It has to be here.
	*/
	tlbsx	r23, 0, r20

	tlbwe	r21, r23, TLB_DATA		/* Load TLB LO */

	/* Done...restore registers and get out of here.
	*/
#ifdef CONFIG_403GCX
	lwz     r22, 12(r0)
	lwz     r21, 8(r0)
	mtspr   SPRN_PID, r22
	mtcr    r21
	lwz     r23, 4(r0)
	lwz     r22, 0(r0)
#else
	mfspr	r22, SPRG6
	mfspr	r21, SPRG7
	mtspr	SPRN_PID, r22
	mtcr	r21
	mfspr	r23, SPRG5
	mfspr	r22, SPRG4
#endif
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	PPC405_ERR77_SYNC
	rfi			/* Should sync shadow TLBs */
	b	.		/* prevent prefetch past rfi */

2:
	/* The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
#ifdef CONFIG_403GCX
	lwz     r22, 12(r0)
	lwz     r21, 8(r0)
	mtspr   SPRN_PID, r22
	mtcr    r21
	lwz     r23, 4(r0)
	lwz     r22, 0(r0)
#else
	mfspr	r22, SPRG6
	mfspr	r21, SPRG7
	mtspr	SPRN_PID, r22
	mtcr	r21
	mfspr	r23, SPRG5
	mfspr	r22, SPRG4
#endif
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	DataAccess

/* 0x0400 - Instruction Storage Exception
 * I don't know why it is called "Storage"....This is caused by a fetch
 * from non-execute or guarded pages.
 */
	START_EXCEPTION(0x0400, InstructionAccess)
	STND_EXCEPTION_PROLOG
	mfspr	r5,SPRN_ESR		/* Grab the ESR, save it, not really needed */
	stw	r5,_ESR(r21)
	mr	r4,r22			/* Pass SRR0 as arg2 */
	li	r5,0			/* Pass zero as arg3 */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(do_page_fault)	/* do_page_fault(regs, SRR0, SRR1) */

/* 0x0500 - External Interrupt Exception
*/
	START_EXCEPTION(0x0500, HardwareInterrupt)
	STND_EXCEPTION_PROLOG
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	li	r4,0
	bl	transfer_to_handler
_GLOBAL(do_IRQ_intercept)
	.long	do_IRQ
	.long	ret_from_intercept

/* 0x0600 - Alignment Exception
*/
	START_EXCEPTION(0x0600, Alignment)
	STND_EXCEPTION_PROLOG
	mfspr	r4,SPRN_DEAR		/* Grab the DEAR and save it */
	stw	r4,_DEAR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(AlignmentException)

/* 0x0700 - Program Exception
*/
	START_EXCEPTION(0x0700,	ProgramCheck)
	STND_EXCEPTION_PROLOG
	mfspr	r4,SPRN_ESR		/* Grab the ESR, save it */
	stw	r4,_ESR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(ProgramCheckException)


/* I'm stealing this unused vector location to build a standard exception
 * frame for Data TLB Access errors.  The other Data TLB exceptions will bail
 * out to this point if they can't resolve the lightweight TLB fault.
 */
	START_EXCEPTION(0x0800,	DataAccess)
	STND_EXCEPTION_PROLOG
	mfspr	r5,SPRN_ESR		/* Grab the ESR, save it, pass arg3 */
	stw	r5,_ESR(r21)
	mfspr	r4,SPRN_DEAR		/* Grab the DEAR, save it, pass arg2 */
	stw	r4,_DEAR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(do_page_fault)	/* do_page_fault(regs, ESR, DEAR) */

	STND_EXCEPTION(0x0900,	Trap_09,		UnknownException)
	STND_EXCEPTION(0x0A00,	Trap_0A,		UnknownException)
	STND_EXCEPTION(0x0B00,	Trap_0B,		UnknownException)
/* 0x0C00 - System Call Exception
*/
	START_EXCEPTION(0x0C00,	SystemCall)
	STND_EXCEPTION_PROLOG
	stw	r3,ORIG_GPR3(r21)
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(DoSyscall)

	STND_EXCEPTION(0x0D00,	Trap_0D,		UnknownException)
	STND_EXCEPTION(0x0E00,	Trap_0E,		UnknownException)
	STND_EXCEPTION(0x0F00,	Trap_0F,		UnknownException)

/* 0x1000 - Programmable Interval Timer (PIT) Exception
*/
	START_EXCEPTION(0x1000,	Decrementer)
	STND_EXCEPTION_PROLOG
	lis	r0,TSR_PIS@h		/* Set-up the PIT exception mask */
	mtspr	SPRN_TSR,r0		/* Clear the PIT exception */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	bl	transfer_to_handler
_GLOBAL(timer_interrupt_intercept)
	.long	timer_interrupt
	.long	ret_from_intercept

#if 0
/* NOTE:
 * FIT and WDT handlers are not implemented yet.
 */

/* 0x1010 - Fixed Interval Timer (FIT) Exception
*/
	STND_EXCEPTION(0x1010,	FITException,		UnknownException)

/* 0x1020 - Watchdog Timer (WDT) Exception
*/

	CRIT_EXCEPTION(0x1020,	WDTException,		UnknownException)
#endif

/* 0x1100 - Data TLB Miss Exception
 * As the name implies, translation is not in the MMU, so search the
 * page tables and fix it.  The only purpose of this function is to
 * load TLB entries from the page table if they exist.
 */
	START_EXCEPTION(0x1100,	DTLBMiss)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
#ifdef CONFIG_403GCX
	stw     r22, 0(r0)
	stw     r23, 4(r0)
	mfcr    r21
	mfspr   r22, SPRN_PID
	stw     r21, 8(r0)
	stw     r22, 12(r0)
#else
	mtspr	SPRG4, r22
	mtspr	SPRG5, r23
	mfcr	r21
	mfspr	r22, SPRN_PID
	mtspr	SPRG7, r21
	mtspr	SPRG6, r22
#endif
	mfspr	r20, SPRN_DEAR		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l
	li	r23, 0
	mtspr	SPRN_PID, r23		/* TLB will have 0 TID */
	b	4f

	/* Get the PGD for the current thread.
	 */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)
4:
	tophys(r21, r21)
	rlwimi	r21, r20, 12, 20, 29	/* Create L1 (pgdir/pmd) address */
	lwz	r21, 0(r21)		/* Get L1 entry */
	rlwinm.	r22, r21, 0, 0, 19	/* Extract L2 (pte) base address */
	beq	2f			/* Bail if no table */

	tophys(r22, r22)
	rlwimi	r22, r20, 22, 20, 29	/* Compute PTE address */
	lwz	r21, 0(r22)		/* Get Linux PTE */
	andi.	r23, r21, _PAGE_PRESENT
	beq	2f

	ori	r21, r21, _PAGE_ACCESSED
	stw	r21, 0(r22)

	/* Most of the Linux PTE is ready to load into the TLB LO.
	 * We set ZSEL, where only the LS-bit determines user access.
	 * We set execute, because we don't have the granularity to
	 * properly set this at the page level (Linux problem).
	 * If shared is set, we cause a zero PID->TID load.
	 * Many of these bits are software only.  Bits we don't set
	 * here we (properly should) assume have the appropriate value.
	 */
	li	r22, 0x0ce2
	andc	r21, r21, r22		/* Make sure 20, 21 are zero */

	b	finish_tlb_load


2:
	/* The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
#ifdef CONFIG_403GCX
	lwz     r22, 12(r0)
	lwz     r21, 8(r0)
	mtspr   SPRN_PID, r22
	mtcr    r21
	lwz     r23, 4(r0)
	lwz     r22, 0(r0)
#else
	mfspr	r22, SPRG6
	mfspr	r21, SPRG7
	mtspr	SPRN_PID, r22
	mtcr	r21
	mfspr	r23, SPRG5
	mfspr	r22, SPRG4
#endif
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	DataAccess

/* 0x1200 - Instruction TLB Miss Exception
 * Nearly the same as above, except we get our information from different
 * registers and bailout to a different point.
 */
	START_EXCEPTION(0x1200,	ITLBMiss)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
#ifdef CONFIG_403GCX
	stw     r22, 0(r0)
	stw     r23, 4(r0)
	mfcr    r21
	mfspr   r22, SPRN_PID
	stw     r21, 8(r0)
	stw     r22, 12(r0)
#else
	mtspr	SPRG4, r22
	mtspr	SPRG5, r23
	mfcr	r21
	mfspr	r22, SPRN_PID
	mtspr	SPRG7, r21
	mtspr	SPRG6, r22
#endif
	mfspr	r20, SRR0		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l
	li	r23, 0
	mtspr	SPRN_PID, r23		/* TLB will have 0 TID */
	b	4f

	/* Get the PGD for the current thread.
	 */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)
4:
	tophys(r21, r21)
	rlwimi	r21, r20, 12, 20, 29	/* Create L1 (pgdir/pmd) address */
	lwz	r21, 0(r21)		/* Get L1 entry */
	rlwinm.	r22, r21, 0, 0, 19	/* Extract L2 (pte) base address */
	beq	2f			/* Bail if no table */

	tophys(r22, r22)
	rlwimi	r22, r20, 22, 20, 29	/* Compute PTE address */
	lwz	r21, 0(r22)		/* Get Linux PTE */
	andi.	r23, r21, _PAGE_PRESENT
	beq	2f

	ori	r21, r21, _PAGE_ACCESSED
	stw	r21, 0(r22)

	/* Most of the Linux PTE is ready to load into the TLB LO.
	 * We set ZSEL, where only the LS-bit determines user access.
	 * We set execute, because we don't have the granularity to
	 * properly set this at the page level (Linux problem).
	 * If shared is set, we cause a zero PID->TID load.
	 * Many of these bits are software only.  Bits we don't set
	 * here we (properly should) assume have the appropriate value.
	 */
	li	r22, 0x0ce2
	andc	r21, r21, r22		/* Make sure 20, 21 are zero */

	b	finish_tlb_load

2:
	/* The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
#ifdef CONFIG_403GCX
	lwz     r22, 12(r0)
	lwz     r21, 8(r0)
	mtspr   SPRN_PID, r22
	mtcr    r21
	lwz     r23, 4(r0)
	lwz     r22, 0(r0)
#else
	mfspr	r22, SPRG6
	mfspr	r21, SPRG7
	mtspr	SPRN_PID, r22
	mtcr	r21
	mfspr	r23, SPRG5
	mfspr	r22, SPRG4
#endif
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	InstructionAccess

	STND_EXCEPTION(0x1300,	Trap_13,		UnknownException)
	STND_EXCEPTION(0x1400,	Trap_14,		UnknownException)
	STND_EXCEPTION(0x1500,	Trap_15,		UnknownException)
	STND_EXCEPTION(0x1600,	Trap_16,		UnknownException)
#ifdef CONFIG_IBM405_ERR51
	/* 405GP errata 51 */
	START_EXCEPTION(0x1700, Trap_17)
	b DTLBMiss
#else
	STND_EXCEPTION(0x1700,	Trap_17,		UnknownException)
#endif
	STND_EXCEPTION(0x1800,	Trap_18,		UnknownException)
	STND_EXCEPTION(0x1900,	Trap_19,		UnknownException)
	STND_EXCEPTION(0x1A00,	Trap_1A,		UnknownException)
	STND_EXCEPTION(0x1B00,	Trap_1B,		UnknownException)
	STND_EXCEPTION(0x1C00,	Trap_1C,		UnknownException)
	STND_EXCEPTION(0x1D00,	Trap_1D,		UnknownException)
	STND_EXCEPTION(0x1E00,	Trap_1E,		UnknownException)
	STND_EXCEPTION(0x1F00,	Trap_1F,		UnknownException)

/* 0x2000 - Debug Exception
*/
	START_EXCEPTION(0x2000, DebugTrap)
	b	check_single_step_in_exception
ret_to_debug_exception:
	CRIT_EXCEPTION_PROLOG
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,CRIT_EXC;
        li      r20,MSR_KERNEL
	FINISH_EXCEPTION(DebugException)

/* Make sure the final interrupt handler has not spilled past the
 * end of its allotted space.
 */
        .=0x2100

/* Check for a single step debug exception while in an exception
 * handler before state has been saved.  This is to catch the case
 * where an instruction that we are trying to single step causes
 * an exception (eg ITLB miss) and thus the first instruction of
 * the exception handler generates a single step debug exception.
 *
 * If we get a debug trap on the first instruction of an exception handler,
 * we reset the MSR_DE in the _exception handlers_ MSR (the debug trap is
 * a critical exception, so we are using SPRN_SRR3 to manipulate the MSR).
 * The exception handler was handling a non-critical interrupt, so it will
 * save (and later restore) the MSR via SPRN_SRR1, which will still have
 * the MSR_DE bit set.
 */
check_single_step_in_exception:

	/* This first instruction was already executed by the exception
	 * handler and must be the first instruction of every exception
	 * handler.
	 */
	mtspr	SPRN_SPRG0,r20		/* Save some working registers... */
	mtspr	SPRN_SPRG1,r21
	mfcr	r20			/* ..and the cr because we change it */

	mfspr   r21,SPRN_SRR3		/* MSR at the time of fault */
	andi.   r21,r21,MSR_PR
	bne+    2f			/* trapped from problem state */

	mfspr   r21,SPRN_SRR2		/* Faulting instruction address */
	cmplwi  r21,0x2100
	bgt+    2f			/* address above exception vectors */

	lis     r21,DBSR_IC@h           /* Remove the trap status */
	mtspr   SPRN_DBSR,r21

	mfspr	r21,SPRN_SRR3
	rlwinm	r21,r21,0,23,21		/* clear MSR_DE */
	mtspr	SPRN_SRR3, r21		/* restore MSR at rcfi without DE */

	mtcrf   0xff,r20                /* restore registers */
	mfspr   r21,SPRN_SPRG1
	mfspr   r20,SPRN_SPRG0

	sync
	rfci                            /* return to the exception handler  */
	b	.			/* prevent prefetch past rfci */

2:
	mtcrf   0xff,r20                /* restore registers */
	mfspr   r21,SPRN_SPRG1
	mfspr   r20,SPRN_SPRG0
	b       ret_to_debug_exception

/* Other PowerPC processors, namely those derived from the 6xx-series
 * have vectors from 0x2100 through 0x2F00 defined, but marked as reserved.
 * However, for the 4xx-series processors these are neither defined nor
 * reserved.
 */

	/* Damn, I came up one instruction too many to fit into the
	 * exception space :-).  Both the instruction and data TLB
	 * miss get to this point to load the TLB.
	 * 	r20 - EA of fault
	 * 	r21 - TLB LO (info from Linux PTE)
	 *	r22, r23 - avilable to use
	 *	PID - loaded with proper value when we get here
	 *	Upon exit, we reload everything and RFI.
	 * Actually, it will fit now, but oh well.....a common place
	 * to load the TLB.
	 */
tlb_4xx_index:
	.long	0
finish_tlb_load:
	/* load the next available TLB index.
	*/
	lwz	r23, tlb_4xx_index@l(0)
	addi	r23, r23, 1
#ifdef CONFIG_PIN_TLB
	cmpwi	0, r23, 61		/* reserve entries 62, 63 for kernel */
	ble	7f
	li	r23, 0
7:
#else
	andi.	r23, r23, (PPC4XX_TLB_SIZE-1)
#endif
	stw	r23, tlb_4xx_index@l(0)

6:
	ori	r21, r21, _PAGE_HWEXEC		/* make it executable */
	tlbwe	r21, r23, TLB_DATA		/* Load TLB LO */

	/* Create EPN.  This is the faulting address plus a static
	 * set of bits.  These are size, valid, E, U0, and ensure
	 * bits 20 and 21 are zero.
	 */
	li	r22, 0x00c0
	rlwimi	r20, r22, 0, 20, 31
	tlbwe	r20, r23, TLB_TAG		/* Load TLB HI */

	/* Done...restore registers and get out of here.
	*/
#ifdef CONFIG_403GCX
	lwz     r22, 12(r0)
	lwz     r21, 8(r0)
	mtspr   SPRN_PID, r22
	mtcr    r21
	lwz     r23, 4(r0)
	lwz     r22, 0(r0)
#else
	mfspr	r22, SPRG6
	mfspr	r21, SPRG7
	mtspr	SPRN_PID, r22
	mtcr	r21
	mfspr	r23, SPRG5
	mfspr	r22, SPRG4
#endif
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	PPC405_ERR77_SYNC
	rfi			/* Should sync shadow TLBs */
	b	.		/* prevent prefetch past rfi */

/* This code finishes saving the registers to the exception frame
 * and jumps to the appropriate handler for the exception, turning
 * on address translation.
 */
_GLOBAL(transfer_to_handler)
	stw	r22,_NIP(r21)		/* Save the faulting IP on the stack */
	stw	r23,_MSR(r21)		/* Save the exception MSR on stack */
	SAVE_4GPRS(8, r21)		/* Save r8 through r11 on the stack */
	SAVE_8GPRS(12, r21)		/* Save r12 through r19 on the stack */
	SAVE_8GPRS(24, r21)		/* Save r24 through r31 on the stack */
	andi.	r23,r23,MSR_PR		/* Is this from user space? */
	mfspr	r23,SPRN_SPRG3		/* If from user, fix up THREAD.regs */
	beq	2f			/* No, it is from the kernel; branch. */
	mfspr	r24,SPRN_DBCR0
	stw	r24,THREAD_DBCR0(r23)	/* Save Debug Control in thread_struct */
	addi	r24,r1,STACK_FRAME_OVERHEAD
	stw	r24,PT_REGS(r23)
2:	addi	r2,r23,-THREAD		/* Set r2 to current thread */
	tovirt(r2,r2)
	mflr	r23
	andi.	r24,r23,0x3f00		/* Get vector offset */
	stw	r24,TRAP(r21)
	li	r22,RESULT
	/* No need to put an erratum #77 workaround here
		because interrupts are currently disabled */
	stwcx.	r22,r22,r21		/* Clear the reservation */
	li	r22,0
	stw	r22,RESULT(r21)
	mtspr	SPRN_SPRG2,r22		/* r1 is now the kernel stack pointer */
	addi	r24,r2,TASK_STRUCT_SIZE	/* Check for kernel stack overflow */
	cmplw	cr0,r1,r2
	cmplw	cr1,r1,r24
	crand	cr1,cr1,cr4
	bgt-	stack_ovf		/* If r2 < r1 < r2 + TASK_STRUCT_SIZE */
	lwz	r24,0(r23)		/* Virtual address of the handler */
	lwz	r23,4(r23)		/* Handler return pointer */
	cmpwi	cr0,r7,STND_EXC		/* What type of exception is this? */
	bne	3f			/* It is a critical exception... */

	/* Standard exception jump path
	*/

	/* We have to recover r7 from the register save stack.
	 * It was used to indicate standard/critical exception.  In
	 * the case of a standard exception that is the system call
	 * trap, it may have originally contained one of the syscall
	 * parameters and we have to get it back now.
	 */
	lwz	r7,GPR7(r21)
	mtspr	SPRN_SRR0,r24		/* Set up the instruction pointer */
	mtspr	SPRN_SRR1,r20		/* Set up the machine state register */
	mtlr	r23			/* Set up the return pointer */
	SYNC
	/* We shouldn't need a 405 erratum #77 workaround here, because we're not
	 * actually returning to the interrupted instruction yet. */
	rfi
	b	.		/* prevent prefetch past rfi */

	/* Critical exception jump path
	*/

3:	mtspr	SPRN_SRR2,r24		/* Set up the instruction pointer */
	mtspr	SPRN_SRR3,r20		/* Set up the machine state register */
	mtlr	r23			/* Set up the return pointer */
	SYNC
	rfci
	b	.			/* prevent prefetch past rfci */

/* On kernel stack overlow, load up an initial stack pointer and call
 * StackOverflow(regs), which should NOT return.
 */

stack_ovf:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	lis	r1,init_task_union@ha
	addi	r1,r1,init_task_union@l
	addi	r1,r1,TASK_UNION_SIZE - STACK_FRAME_OVERHEAD
	lis	r24,StackOverflow@ha
	addi	r24,r24,StackOverflow@l
	li	r20,MSR_KERNEL
	mtspr	SPRN_SRR0,r24
	mtspr	SPRN_SRR1,r20
	SYNC
	rfi
	b	.		/* prevent prefetch past rfi */

/* extern void giveup_altivec(struct task_struct *prev)
 *
 * The PowerPC 4xx family of processors do not have AltiVec capabilities, so
 * this just returns.
 */
_GLOBAL(giveup_altivec)
	blr

/* extern void giveup_fpu(struct task_struct *prev)
 *
 * The PowerPC 4xx family of processors do not have an FPU, so this just
 * returns.
 */
_GLOBAL(giveup_fpu)
	blr

/* extern void abort(void)
 *
 * At present, this routine just applies a system reset.
 */
_GLOBAL(abort)
        mfspr   r13,SPRN_DBCR0
        oris    r13,r13,DBCR_RST(DBCR_RST_SYSTEM)@h
        mtspr   SPRN_DBCR0,r13


/* This is where the main kernel code starts.
 */
start_here:

	/* ptr to current */
	lis	r2,init_task_union@h
	ori	r2,r2,init_task_union@l

	/* ptr to phys current thread */
	tophys(r4,r2)
	addi	r4,r4,THREAD	/* init task's THREAD */
	mtspr	SPRG3,r4
	li	r3,0
	mtspr	SPRG2,r3	/* 0 => r1 has kernel sp */

	/* stack */
	addi	r1,r2,TASK_UNION_SIZE
	li	r0,0
	stwu	r0,-STACK_FRAME_OVERHEAD(r1)

	bl	early_init	/* We have to do this with MMU on */

/*
 * Decide what sort of machine this is and initialize the MMU.
 */
	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27
	bl	machine_init
	bl	MMU_init

/* Go back to running unmapped so we can load up new values
 * and change to using our exception vectors.
 * On the 4xx, all we have to do is invalidate the TLB to clear
 * the old 16M byte TLB mappings.
 */
	lis	r4,2f@h
	ori	r4,r4,2f@l
	tophys(r4,r4)
	li	r3,MSR_KERNEL & ~(MSR_IR|MSR_DR)
	mtspr	SRR0,r4
	mtspr	SRR1,r3
	rfi
	b	.		/* prevent prefetch past rfi */

/* Load up the kernel context */
2:
	SYNC			/* Force all PTE updates to finish */
#ifndef CONFIG_PIN_TLB
	tlbia			/* Clear all TLB entries */
	sync			/* wait for tlbia/tlbie to finish */
#endif

	/* set up the PTE pointers for the Abatron bdiGDB.
	*/
	lis	r6, swapper_pg_dir@h
	ori	r6, r6, swapper_pg_dir@l
	lis	r5, abatron_pteptrs@h
	ori	r5, r5, abatron_pteptrs@l
	stw	r5, 0xf0(r0)	/* Must match your Abatron config file */
	tophys(r5,r5)
	stw	r6, 0(r5)

/* Now turn on the MMU for real! */
	li	r4,MSR_KERNEL
	lis	r3,start_kernel@h
	ori	r3,r3,start_kernel@l
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfi			/* enable MMU and jump to start_kernel */
	b	.		/* prevent prefetch past rfi */

/* Set up the initial MMU state so we can do the first level of
 * kernel initialization.  This maps the first 16 MBytes of memory 1:1
 * virtual to physical and more importantly sets the cache mode.
 */
initial_mmu:
	tlbia			/* Invalidate all TLB entries */
	sync

	/* We should still be executing code at physical address 0x0000xxxx
	 * at this point. However, start_here is at virtual address
	 * 0xC000xxxx. So, set up a TLB mapping to cover this once
	 * translation is enabled.
	 */

	lis	r3,KERNELBASE@h		/* Load the kernel virtual address */
	ori	r3,r3,KERNELBASE@l
	tophys(r4,r3)			/* Load the kernel physical address */

	iccci	r0,r3			/* Invalidate the i-cache before use */

	/* Load the kernel PID.
	*/
	li	r0,0
	mtspr	SPRN_PID,r0
	sync

	/* Configure and load two entries into TLB slots 62 and 63.
	 * In case we are pinning TLBs, these are reserved in by the
	 * other TLB functions.  If not reserving, then it doesn't
	 * matter where they are loaded.
	 */
	clrrwi	r4,r4,10		/* Mask off the real page number */
	ori	r4,r4,(TLB_WR | TLB_EX)	/* Set the write and execute bits */

	clrrwi	r3,r3,10		/* Mask off the effective page number */
	ori	r3,r3,(TLB_VALID | TLB_PAGESZ(PAGESZ_16M))

        li      r0,62                    /* TLB slot 62 */

	tlbwe	r4,r0,TLB_DATA		/* Load the data portion of the entry */
	tlbwe	r3,r0,TLB_TAG		/* Load the tag portion of the entry */

	addis	r4, r4, 0x0100		/* Map next 16 M entries */
	addis	r3, r3, 0x0100

        li      r0,63                    /* TLB slot 63 */

	tlbwe	r4,r0,TLB_DATA
	tlbwe	r3,r0,TLB_TAG

#if defined(CONFIG_SERIAL_TEXT_DEBUG) && defined(SERIAL_DEBUG_IO_BASE)

	/* Load a TLB entry for the UART, so that ppc4xx_progress() can use
	 * the UARTs nice and early.  We use a 4k real==virtual mapping. */

	lis	r3,SERIAL_DEBUG_IO_BASE@h
	ori	r3,r3,SERIAL_DEBUG_IO_BASE@l
	mr	r4,r3
	clrrwi	r4,r4,12
	ori	r4,r4,(TLB_WR|TLB_I|TLB_M|TLB_G)

	clrrwi	r3,r3,12
	ori	r3,r3,(TLB_VALID | TLB_PAGESZ(PAGESZ_4K))

	li	r0,0			/* TLB slot 0 */
	tlbwe	r4,r0,TLB_DATA
	tlbwe	r3,r0,TLB_TAG
#endif /* CONFIG_SERIAL_DEBUG_TEXT && SERIAL_DEBUG_IO_BASE */

	isync

	/* Establish the exception vector base
	*/
	lis	r4,KERNELBASE@h		/* EVPR only uses the high 16-bits */
	tophys(r0,r4)			/* Use the physical address */
	mtspr	SPRN_EVPR,r0

	blr


_GLOBAL(set_context)

#ifdef CONFIG_BDI_SWITCH
	/* Context switch the PTE pointer for the Abatron BDI2000.
	 * The PGDIR is the second parameter.
	 */
	lis	r5, KERNELBASE@h
	lwz	r5, 0xf0(r5)
	stw	r4, 0x4(r5)
#endif
	sync
	mtspr	SPRN_PID,r3
	isync				/* Need an isync to flush shadow */
					/* TLBs after changing PID */
	blr

/* We put a few things here that have to be page-aligned. This stuff
 * goes at the beginning of the data segment, which is page-aligned.
 */
	.data
_GLOBAL(sdata)
_GLOBAL(empty_zero_page)
	.space	4096
_GLOBAL(swapper_pg_dir)
	.space	4096

/* This space gets a copy of optional info passed to us by the bootstrap
 * which is used to pass parameters into the kernel like root=/dev/sda1, etc.
 */
_GLOBAL(cmd_line)
	.space	512

/* Room for two PTE pointers, usually the kernel and current user pointers
 * to their respective root page table.
 */
abatron_pteptrs:
	.space	8
