/*
 * arch/ppc/kernel/head_44x.S
 *
 * Kernel execution entry point code.
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2002-2003 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/ibm44x.h>
#include <asm/cputable.h>
#include <asm/ppc_asm.h>
#include "ppc_defs.h"

/*
 * Preprocessor Defines
 */

#define STND_EXC        0
#define CRIT_EXC        1

#ifdef CONFIG_440A
#define MCHK_EXC	2
#define __SPRN_MCSRR0	SPRN_MCSRR0
#define __SPRN_MCSRR1	SPRN_MCSRR1
#else
#define MCHK_EXC	CRIT_EXC
#define __SPRN_MCSRR0	SPRN_CSRR0
#define __SPRN_MCSRR1	SPRN_CSRR1
#endif

/*
 * Macros
 */

#define SET_IVOR(vector_number, vector_label)		\
		li	r26,vector_label@l; 		\
		mtspr	SPRN_IVOR##vector_number,r26;	\
		sync
					
/* As with the other PowerPC ports, it is expected that when code
 * execution begins here, the following registers contain valid, yet
 * optional, information:
 *
 *   r3 - Board info structure pointer (DRAM, frequency, MAC address, etc.)
 *   r4 - Starting address of the init RAM disk
 *   r5 - Ending address of the init RAM disk
 *   r6 - Start of kernel command line string (e.g. "mem=128")
 *   r7 - End of kernel command line string
 *
 */ 
	.text
_GLOBAL(_stext)
_GLOBAL(_start)
	/*
	 * Reserve a word at a fixed location to store the address
	 * of abatron_pteptrs
	 */
	nop

/*
 * Save parameters we are passed
 */
	mr	r31,r3
	mr	r30,r4
	mr	r29,r5
	mr	r28,r6
	mr	r27,r7
	li	r24,0		/* CPU number */

/*
 * Set up the initial MMU state
 *
 * We are still executing code at the virtual address
 * mappings set by the firmware for the base of RAM.
 *
 * We first invalidate all TLB entries but the one
 * we are running from.  We then load the KERNELBASE
 * mappings so we can begin to use kernel addresses
 * natively and so the interrupt vector locations are
 * permanently pinned (necessary since Book E
 * implementations always have translation enabled).
 *
 * TODO: Use the known TLB entry we are running from to
 *	 determine which physical region we are located
 *	 in.  This can be used to determine where in RAM
 *	 (on a shared CPU system) or PCI memory space
 *	 (on a DRAMless system) we are located.
 *       For now, we assume a perfect world which means
 *	 we are located at the base of DRAM (physical 0).
 */

/*
 * Search TLB for entry that we are currently using.
 * Invalidate all entries but the one we are using.
 */
	/* Load our current PID->MMUCR TID and MSR IS->MMUCR STS */
 	mfspr	r3,SPRN_MMUCR			/* Get MMUCR */
	lis	r4,PPC44x_MMUCR_STS@h
	ori	r4,r4,PPC44x_MMUCR_TID@l	/* Create mask */
	andc	r3,r3,r4			/* Clear out TID/STS bits */
	mfspr	r4,SPRN_PID			/* Get PID */
	or	r3,r3,r4			/* Set TID bits */
	mfmsr	r5				/* Get MSR */
	andi.	r5,r5,MSR_IS@l			/* TS=1? */
	beq	wmmucr				/* If not, leave STS=0 */
	oris	r3,r3,PPC44x_MMUCR_STS@h	/* Set STS=1 */
wmmucr:	mtspr	SPRN_MMUCR,r3			/* Put MMUCR */
	sync
	
	bl	invstr				/* Find our address */
invstr:	mflr	r5				/* Make it accessible */
	tlbsx	r23,0,r5			/* Find entry we are in */
	li	r4,0				/* Start at TLB entry 0 */
	li	r3,0				/* Set PAGEID inval value */
1:	cmpw	r23,r4				/* Is this our entry? */
	beq	skpinv				/* If so, skip the inval */
	tlbwe	r3,r4,PPC44x_TLB_PAGEID		/* If not, inval the entry */
skpinv:	addi	r4,r4,1				/* Increment */
	cmpwi	r4,64				/* Are we done? */
	bne	1b				/* If not, repeat */
	isync					/* If so, context change */

/*
 * Configure and load pinned entries into TLB slots 62 and 63.
 */

	lis	r3,KERNELBASE@h		/* Load the kernel virtual address */
	ori	r3,r3,KERNELBASE@l

	/* Kernel is at the base of RAM */
	li r4, 0			/* Load the kernel physical address */

	/* Load the kernel PID = 0 */
	li	r0,0
	mtspr	SPRN_PID,r0
	sync

	/* Load the kernel TID  = 0 */
	mfspr	r5,SPRN_MMUCR
	lis	r6, PPC44x_MMUCR_TID@h
	ori	r6,r6,PPC44x_MMUCR_TID@l
	andc	r5,r5,r6
	mtspr	SPRN_MMUCR,r5
	sync

 	/* pageid fields */
	clrrwi	r3,r3,10		/* Mask off the effective page number */
	ori	r3,r3,(PPC44x_TLB_VALID | PPC44x_TLB_PAGESZ(PPC44x_PAGESZ_256M))

	/* xlat fields */
	clrrwi	r4,r4,10		/* Mask off the real page number */
					/* ERPN is 0 for first 4GB page */

	/* attrib fields */
	/* Added guarded bit to protect against speculative loads/stores */
	li	r5,0
	ori	r5,r5,(PPC44x_TLB_SW | PPC44x_TLB_SR | PPC44x_TLB_SX | PPC44x_TLB_G)

        li      r0,62                    /* TLB slot 62 */

	tlbwe	r3,r0,PPC44x_TLB_PAGEID	/* Load the pageid fields */
	tlbwe	r4,r0,PPC44x_TLB_XLAT	/* Load the translation fields */
	tlbwe	r5,r0,PPC44x_TLB_ATTRIB	/* Load the attrib/access fields */

	/* Force context change */
	mfmsr	r0
	mtspr	SRR1, r0
	lis	r0,3f@h
	ori	r0,r0,3f@l
	mtspr	SRR0,r0
	sync
	rfi

	/* If necessary, invalidate original entry we used */
3:	cmpwi	r23,62
	beq	4f
	li	r6,0
	tlbwe   r6,r23,PPC44x_TLB_PAGEID
	sync

4:	ori	r3,r3,PPC44x_TLB_TS	/* TS = 1 */

        li      r0,63                   /* TLB slot 63 */

	tlbwe	r3,r0,PPC44x_TLB_PAGEID	/* Load the pageid fields */
	tlbwe	r4,r0,PPC44x_TLB_XLAT	/* Load the translation fields */
	tlbwe	r5,r0,PPC44x_TLB_ATTRIB	/* Load the attrib/access fields */

#ifdef CONFIG_SERIAL_TEXT_DEBUG
	/*
	 * Add temporary UART mapping for early debug.  This
	 * mapping must be identical to that used by the early
	 * bootloader code since the same asm/serial.h parameters
	 * are used for polled operation.
	 */
 	/* pageid fields */
	lis	r3,0xe000		
	ori	r3,r3,(PPC44x_TLB_VALID | PPC44x_TLB_PAGESZ(PPC44x_PAGESZ_256M))

	/* xlat fields */
	lis	r4,0x4000		/* RPN is 0x40000000 */
	ori	r4,r4,0x0001		/* ERPN is 1 for second 4GB page */

	/* attrib fields */
	li	r5,0
	ori	r5,r5,(PPC44x_TLB_SW | PPC44x_TLB_SR | PPC44x_TLB_I | PPC44x_TLB_G)

        li      r0,60                    /* TLB slot 60 */

	tlbwe	r3,r0,PPC44x_TLB_PAGEID	/* Load the pageid fields */
	tlbwe	r4,r0,PPC44x_TLB_XLAT	/* Load the translation fields */
	tlbwe	r5,r0,PPC44x_TLB_ATTRIB	/* Load the attrib/access fields */

	ori	r3,r3,PPC44x_TLB_TS	/* Translation state 1 */

        li      r0,61			/* TLB slot 61 */

	tlbwe	r3,r0,PPC44x_TLB_PAGEID	/* Load the pageid fields */
	tlbwe	r4,r0,PPC44x_TLB_XLAT	/* Load the translation fields */
	tlbwe	r5,r0,PPC44x_TLB_ATTRIB	/* Load the attrib/access fields */
#endif /* CONFIG_SERIAL_TEXT_DEBUG */

	/* Force context change */
	isync

	/* Establish the interrupt vector offsets */
	SET_IVOR(0,  CriticalInput);
	SET_IVOR(1,  MachineCheck);
	SET_IVOR(2,  DataStorage);
	SET_IVOR(3,  InstructionStorage);
	SET_IVOR(4,  ExternalInput);
	SET_IVOR(5,  Alignment);
	SET_IVOR(6,  Program);
	SET_IVOR(7,  FloatingPointUnavailable);
	SET_IVOR(8,  SystemCall);
	SET_IVOR(9,  AuxillaryProcessorUnavailable);
	SET_IVOR(10, Decrementer);
	SET_IVOR(11, FixedIntervalTimer);
	SET_IVOR(12, WatchdogTimer);
	SET_IVOR(13, DataTLBError);
	SET_IVOR(14, InstructionTLBError);
	SET_IVOR(15, Debug);

	/* Establish the interrupt vector base */
	lis	r4,interrupt_base@h	/* IVPR only uses the high 16-bits */
	mtspr	SPRN_IVPR,r4

	/*
	 * This is where the main kernel code starts.
	 */ 

	/* ptr to current */
	lis	r2,init_task_union@h
	ori	r2,r2,init_task_union@l

	/* ptr to current thread */
	addi	r4,r2,THREAD	/* init task's THREAD */
	mtspr	SPRG3,r4
	li	r3,0
	mtspr	SPRG2,r3	/* 0 => r1 has kernel sp */

	/* stack */
	addi	r1,r2,TASK_UNION_SIZE
	li	r0,0
	stwu	r0,-STACK_FRAME_OVERHEAD(r1)

	bl	early_init

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

	/* Setup PTE pointers for the Abatron bdiGDB */
	lis	r6, swapper_pg_dir@h
	ori	r6, r6, swapper_pg_dir@l
	lis	r5, abatron_pteptrs@h
	ori	r5, r5, abatron_pteptrs@l
	lis	r4, KERNELBASE@h
	ori	r4, r4, KERNELBASE@l
	stw	r5, 0(r4)	/* Save abatron_pteptrs at a fixed location */
	stw	r6, 0(r5)

	/* Let's move on */
	lis	r4,start_kernel@h
	ori	r4,r4,start_kernel@l
	lis	r3,MSR_KERNEL@h
	ori	r3,r3,MSR_KERNEL@l
	mtspr	SRR0,r4
	mtspr	SRR1,r3
	rfi			/* change context and jump to start_kernel */

/*
 * Interrupt vector entry code
 *
 * The Book E MMUs are always on so we don't need to handle
 * interrupts in real mode as with previous PPC processors. In
 * this case we handle interrupts in the kernel virtual address
 * space.
 *
 * Interrupt vectors are dynamically placed relative to the 
 * interrupt prefix as determined by the address of interrupt_base.
 * The interrupt vectors offsets are programmed using the labels
 * for each interrupt vector entry.
 *
 * Interrupt vectors must be aligned on a 16 byte boundary.
 * We align on a 32 byte cache line boundary for good measure.
 */

#define COMMON_PROLOG                                                        \
0:	mtspr	SPRN_SPRG0,r20;         /* We need r20, move it to SPRG0   */\
	mtspr	SPRN_SPRG1,r21;         /* We need r21, move it to SPRG1   */\
	mfcr	r20;                    /* We need the CR, move it to r20  */\
	mfspr	r21,SPRN_SPRG2;         /* Exception stack to use          */\
	cmpwi	cr0,r21,0;              /* From user mode or RTAS?         */\
	bne	1f;                     /* Not RTAS, branch                */\
	mr	r21, r1;                /* Move vka in r1 to r21           */\
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
	mr	r1,r21;			/* Set-up new kernel stack pointer */\
	SAVE_4GPRS(3, r21);		/* Save r3 through r6 on the stack */\
	SAVE_GPR(7, r21);		/* Save r7 on the stack		   */

#define	STND_EXCEPTION_PROLOG						     \
	COMMON_PROLOG;							     \
	mfspr	r22,SPRN_SRR0;		/* Faulting instruction address	   */\
	lis	r20,MSR_WE@h;						     \
	mfspr	r23,SPRN_SRR1;		/* MSR at the time of fault	   */\
	andc	r23,r23,r20;		/* disable processor wait state    */\
	COMMON_EPILOG;

#define CRIT_EXCEPTION_PROLOG	                                             \
	COMMON_PROLOG;	                                                     \
	mfspr   r22,SPRN_CSRR0;         /* Faulting instruction address    */\
	lis     r20,MSR_WE@h;                                                \
	mfspr   r23,SPRN_CSRR1;         /* MSR at the time of fault        */\
	andc    r23,r23,r20;            /* disable processor wait state    */\
	COMMON_EPILOG;

#define START_EXCEPTION(label) \
        .align 5;              \
label:

#define FINISH_EXCEPTION(n, func)					     \
	bl	transfer_to_handler;					     \
	.long	func;							     \
	.long	ret_from_except;					     \
	.long	n

#define STND_EXCEPTION(n, label, func)					     \
	START_EXCEPTION(label)						     \
	STND_EXCEPTION_PROLOG;						     \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				     \
	li	r7,STND_EXC;						     \
	li	r20,MSR_KERNEL;						     \
	FINISH_EXCEPTION(n, func)

#define	CRIT_EXCEPTION(n, label, func)					     \
	START_EXCEPTION(label)						     \
	CRIT_EXCEPTION_PROLOG;						     \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				     \
	li	r7,CRIT_EXC;						     \
	li	r20,MSR_KERNEL;						     \
	FINISH_EXCEPTION(n, func)

interrupt_base:

	/* Critical Input Interrupt */
	CRIT_EXCEPTION(0x100, CriticalInput,UnknownException);

	/* Machine Check Interrupt */
	/* TODO: provide bus error register status */
	START_EXCEPTION(MachineCheck)
	COMMON_PROLOG;
	mfspr   r22,__SPRN_MCSRR0       /* Faulting instruction address    */
	lis     r20,MSR_WE@h
	mfspr   r23,__SPRN_MCSRR1       /* MSR at the time of fault        */
	andc    r23,r23,r20             /* disable processor wait state    */
	COMMON_EPILOG;
#ifdef CONFIG_440A
	lis	r20,MCSR_MCS@h
	mfspr	r4,SPRN_MCSR		/* We may want to access original
					   MCSR as arg2 in the future. --ebs */
	mtspr	SPRN_MCSR,r20		/* Clear Machine Check Summary field */
#endif	
	mfspr	r5,SPRN_ESR		/* Grab the ESR, save it */
	stw	r5,_ESR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,MCHK_EXC
	li	r20,MSR_KERNEL
	FINISH_EXCEPTION(0x200, MachineCheckException)

	/* Data Storage Interrupt */
	START_EXCEPTION(DataStorage)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
	mtspr	SPRG4W, r22
	mtspr	SPRG5W, r23
	mtspr	SPRG6W, r24
	mfcr	r21
	mtspr	SPRG7W, r21

	/*
	 * Check if it was a store fault, if not then bail
	 * because a user tried to access a kernel or
	 * read-protected page.  Otherwise, get the
	 * offending address and handle it.
	 */
	mfspr	r20, SPRN_ESR
	andis.	r20, r20, ESR_DST@h
	beq	2f

	mfspr	r20, SPRN_DEAR		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l

	mfspr   r22,SPRN_MMUCR          /* Set TID to 0 */
	li      r23,PPC44x_MMUCR_TID@l
	andc    r22,r22,r23
	mtspr   SPRN_MMUCR,r22

	b	4f

	/* Get the PGD for the current thread */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)

	/* Load MMUCR with our PID and STS=<current TS> */
	mfspr	r22,SPRN_MMUCR			/* Get MMUCR */
	lis     r23,PPC44x_MMUCR_STS@h
	ori     r23,r23,PPC44x_MMUCR_TID@l      /* Create mask */
	andc    r22,r22,r23                     /* Clear out TID/STS bits */
	mfspr   r23,SPRN_PID                    /* Get PID */
	or      r22,r22,r23			/* Set TID bits */
	mfspr	r24,SPRN_SRR1			/* Get SRR1 */
	andi.	r24,r24,MSR_IS@l		/* TS=1? */
	beq	4f				/* If not, leave STS=0 */
	oris	r22,r22,PPC44x_MMUCR_STS@h	/* Set STS=1 */
	mtspr   SPRN_MMUCR,r22
4:
	rlwinm  r22, r20, 13, 19, 29    /* Compute pgdir/pmd offset */
	lwzx    r21, r22, r21           /* Get pgd/pmd entry */
	rlwinm. r22, r21, 0, 0, 20      /* Extract pt base address */
	beq     2f                      /* Bail if no table */

	rlwimi  r22, r20, 23, 20, 28    /* Compute pte address */
	lwz     r21, 4(r22)             /* Get pte entry */
	
	andi.	r23, r21, _PAGE_RW	/* Is it writeable? */
	beq	2f			/* Bail if not */

	/* Update 'changed'.
	*/
	ori	r21, r21, _PAGE_DIRTY|_PAGE_ACCESSED|_PAGE_HWWRITE
	stw	r21, 4(r22)		/* Update Linux page table */

	/* FIXME: Staticly setting some permissions */
	li	r23, 0x003f		/* Set UX,UW,UR,SX,SW,SR */
	andi.	r21,r21,0xffff		/* Clear MS 16 bits */
	/* FIXME: Force attributes */
	ori	r21,r21, 0x0100		/* Set G */
	/* FIXME: Already set in PTE */
	rlwimi	r21,r23,0,26,31		/* Insert static perms */

	lis	r23,0xffff
	ori	r23,r23,0x0fff			/* Set U0-U3 mask */
	and	r21,r21,r23			/* Clear U0-U3 */

	/* find the TLB index that caused the fault.  It has to be here. */
	tlbsx	r24, 0, r20

	tlbwe	r21, r24, PPC44x_TLB_ATTRIB		/* Write ATTRIB */

	/* Done...restore registers and get out of here.
	*/
	mfspr	r21, SPRG7R
	mtcr	r21
	mfspr	r24, SPRG6R
	mfspr	r23, SPRG5R
	mfspr	r22, SPRG4R

	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	rfi			/* Force context change */

2:
	/*
	 * The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
	mfspr	r21, SPRG7R
	mtcr	r21
	mfspr	r24, SPRG6R
	mfspr	r23, SPRG5R
	mfspr	r22, SPRG4R

	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	data_access

	/* Instruction Storage Interrupt */
	START_EXCEPTION(InstructionStorage)
	STND_EXCEPTION_PROLOG
	mfspr	r5,SPRN_ESR		/* Grab the ESR, save it */
	stw	r5,_ESR(r21)
	mr      r4,r22                  /* Pass SRR0 as arg2 */
	li      r5,0                    /* Pass zero as arg3 */
	addi    r3,r1,STACK_FRAME_OVERHEAD
	li      r7,STND_EXC
	li      r20,MSR_KERNEL
	rlwimi  r20,r23,0,16,16         /* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(0x400, do_page_fault)/* do_page_fault(regs, SRR0, SRR1) */

	/* External Input Interrupt */
	START_EXCEPTION(ExternalInput)
	STND_EXCEPTION_PROLOG
	addi    r3,r1,STACK_FRAME_OVERHEAD
	li      r7,STND_EXC
	li      r20,MSR_KERNEL
	li      r4,0
	bl      transfer_to_handler
_GLOBAL(do_IRQ_intercept)
	.long   do_IRQ
	.long   ret_from_intercept
	.long	0x500

	/* Alignment Interrupt */
	START_EXCEPTION(Alignment)
	STND_EXCEPTION_PROLOG
	mfspr   r4,SPRN_DEAR            /* Grab the DEAR and save it */
	stw     r4,_DEAR(r21)
	addi    r3,r1,STACK_FRAME_OVERHEAD
	li      r7,STND_EXC
	li      r20,MSR_KERNEL
	rlwimi  r20,r23,0,16,16         /* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(0x600, AlignmentException)

	/* Program Interrupt */
	START_EXCEPTION(Program)
	STND_EXCEPTION_PROLOG
	mfspr	r4,SPRN_ESR		/* Grab the ESR, save it */
	stw	r4,_ESR(r21)
	addi    r3,r1,STACK_FRAME_OVERHEAD
	li      r7,STND_EXC
	li      r20,MSR_KERNEL
	rlwimi  r20,r23,0,16,16         /* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(0x700, ProgramCheckException)

	/* Floating Point Unavailable Interrupt */
	STND_EXCEPTION(0x2010, FloatingPointUnavailable,UnknownException);

	/* System Call Interrupt */
	START_EXCEPTION(SystemCall)
	STND_EXCEPTION_PROLOG
	stw	r3,ORIG_GPR3(r21)
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(0xc00, DoSyscall)

	/* Auxillary Processor Unavailable */
	STND_EXCEPTION(0x2020, AuxillaryProcessorUnavailable,UnknownException);

	/* Decrementer Interrupt */
	START_EXCEPTION(Decrementer)
	STND_EXCEPTION_PROLOG
	lis     r0,TSR_DIS@h            /* Setup the DEC interrupt mask */
	mtspr   SPRN_TSR,r0            /* Clear the DEC interrupt */
	addi    r3,r1,STACK_FRAME_OVERHEAD
	li      r7,STND_EXC
	li      r20,MSR_KERNEL
	bl      transfer_to_handler
_GLOBAL(timer_interrupt_intercept)
	.long   timer_interrupt
	.long   ret_from_intercept
	.long	0x1000

	/* Fixed Internal Timer Interrupt */
	/* TODO: Add FIT support */
	STND_EXCEPTION(0x1010, FixedIntervalTimer,UnknownException);

	/* Watchdog Timer Interrupt */
	/* TODO: Add watchdog support */
	CRIT_EXCEPTION(0x1020, WatchdogTimer,UnknownException);

	/* Data TLB Error Interrupt */
	START_EXCEPTION(DataTLBError)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
	mtspr	SPRG4W, r22
	mtspr	SPRG5W, r23
	mtspr	SPRG6W, r24
	mfcr	r21
	mtspr	SPRG7W, r21
	mfspr	r20, SPRN_DEAR		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l

	mfspr	r22,SPRN_MMUCR		/* Set TID to 0 */
	li	r23,PPC44x_MMUCR_TID@l
	andc	r22,r22,r23
	mtspr	SPRN_MMUCR,r22

	b	4f

	/* Get the PGD for the current thread */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)

	/* Load PID into MMUCR TID */
	li      r23,PPC44x_MMUCR_TID@l       	 /* Create mask */
	andc    r22,r22,r23                      /* Clear out TID/STS bits */
	mfspr   r23,SPRN_PID                     /* Get PID */
	or      r22,r22,r23
	mtspr	SPRN_MMUCR,r22
4:
	rlwinm 	r22, r20, 13, 19, 29	/* Compute pgdir/pmd offset */
	lwzx	r21, r22, r21		/* Get pgd/pmd entry */
	rlwinm.	r22, r21, 0, 0, 20	/* Extract pt base address */
	beq	2f			/* Bail if no table */

	rlwimi	r22, r20, 23, 20, 28	/* Compute pte address */
	lwz	r21, 4(r22)		/* Get pte entry */
	andi.	r23, r21, _PAGE_PRESENT	/* Is the page present? */
	beq	2f			/* Bail if not present */

	ori	r21, r21, _PAGE_ACCESSED
	stw	r21, 4(r22)

	 /* Jump to common tlb load */
	b	finish_tlb_load

2:
	/* The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
	mfspr	r21, SPRG7R
	mtcr	r21
	mfspr	r24, SPRG6R
	mfspr	r23, SPRG5R
	mfspr	r22, SPRG4R
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	data_access

	/* Instruction TLB Error Interrupt */
	/*
	 * Nearly the same as above, except we get our
	 * information from different registers and bailout
	 * to a different point.
	 */
	START_EXCEPTION(InstructionTLBError)
	mtspr	SPRG0, r20		/* Save some working registers */
	mtspr	SPRG1, r21
	mtspr	SPRG4W, r22
	mtspr	SPRG5W, r23
	mtspr	SPRG6W, r24
	mfcr	r21
	mtspr	SPRG7W, r21
	mfspr	r20, SRR0		/* Get faulting address */

	/* If we are faulting a kernel address, we have to use the
	 * kernel page tables.
	 */
	andis.	r21, r20, 0x8000
	beq	3f
	lis	r21, swapper_pg_dir@h
	ori	r21, r21, swapper_pg_dir@l

	mfspr	r22,SPRN_MMUCR		/* Set TID to 0 */
	li	r23,PPC44x_MMUCR_TID@l
	andc	r22,r22,r23
	mtspr	SPRN_MMUCR,r22

	b	4f

	/* Get the PGD for the current thread */
3:
	mfspr	r21,SPRG3
	lwz	r21,PGDIR(r21)

	/* Load PID into MMUCR TID */
	li      r23,PPC44x_MMUCR_TID@l       	 /* Create mask */
	andc    r22,r23,r23                      /* Clear out TID/STS bits */
	mfspr   r23,SPRN_PID                     /* Get PID */
	or      r22,r22,r23
	mtspr	SPRN_MMUCR,r22

4:
	rlwinm	r22, r20, 13, 19, 29	/* Compute pgdir/pmd offset */
	lwzx	r21, r22, r21		/* Get pgd/pmd entry */
	rlwinm.	r22, r21, 0, 0, 20	/* Extract pt base address */
	beq	2f			/* Bail if no table */

	rlwimi	r22, r20, 23, 20, 28	/* Compute pte address */
	lwz	r21, 4(r22)		/* Get pte entry */
	andi.	r23, r21, _PAGE_PRESENT	/* Is the page present? */
	beq	2f			/* Bail if not present */

	ori	r21, r21, _PAGE_ACCESSED
	stw	r21, 4(r22)

	/* Jump to common TLB load point */
	b	finish_tlb_load

2:
	/* The bailout.  Restore registers to pre-exception conditions
	 * and call the heavyweights to help us out.
	 */
	mfspr	r21, SPRG7R
	mtcr	r21
	mfspr	r24, SPRG6R
	mfspr	r23, SPRG5R
	mfspr	r22, SPRG4R
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	b	InstructionStorage

/* Check for a single step debug exception while in an exception
 * handler before state has been saved.  This is to catch the case
 * where an instruction that we are trying to single step causes
 * an exception (eg ITLB/DTLB miss) and thus the first instruction of
 * the exception handler generates a single step debug exception.
 *
 * If we get a debug trap on the first instruction of an exception handler,
 * we reset the MSR_DE in the _exception handlers_ MSR (the debug trap is
 * a critical exception, so we are using SPRN_CSRR1 to manipulate the MSR).
 * The exception handler was handling a non-critical interrupt, so it will
 * save (and later restore) the MSR via SPRN_SRR1, which will still have
 * the MSR_DE bit set.
 */
	/* Debug Interrupt */
	START_EXCEPTION(Debug)
	/* This first instruction was already executed by the exception
	 * handler and must be the first instruction of every exception
	 * handler.
	 */
	mtspr	SPRN_SPRG0,r20		/* Save some working registers... */
	mtspr	SPRN_SPRG1,r21
	mtspr	SPRN_SPRG4W,r22
	mfcr	r20			/* ..and the cr because we change it */

	mfspr   r21,SPRN_CSRR1		/* MSR at the time of fault */
	andi.   r21,r21,MSR_PR
	bne+    2f			/* trapped from problem state */

	mfspr   r21,SPRN_CSRR0		/* Faulting instruction address */
	lis	r22, KERNELBASE@h
	ori	r22, r22, KERNELBASE@l
	cmplw   r21,r22
	blt+    2f			/* addr below exception vectors */

	lis	r22, Debug@h
	ori	r22, r22, Debug@l
	cmplw   r21,r22
	bgt+    2f			/* addr above TLB exception vectors */

	lis     r21,DBSR_IC@h           /* Remove the trap status */
	mtspr   SPRN_DBSR,r21

	mfspr	r21,SPRN_CSRR1
	rlwinm	r21,r21,0,23,21		/* clear MSR_DE */
	mtspr	SPRN_CSRR1, r21		/* restore MSR at rcfi without DE */

	mtcrf   0xff,r20                /* restore registers */
	mfspr	r22,SPRN_SPRG4R
	mfspr   r21,SPRN_SPRG1
	mfspr   r20,SPRN_SPRG0

	sync
	rfci                            /* return to the exception handler  */
	b	.			/* prevent prefetch past rfci */

2:
	mtcrf   0xff,r20                /* restore registers */
	mfspr	r22,SPRN_SPRG4R
	mfspr   r21,SPRN_SPRG1
	mfspr   r20,SPRN_SPRG0

	CRIT_EXCEPTION_PROLOG
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,CRIT_EXC;
        li      r20,MSR_KERNEL
	FINISH_EXCEPTION(0x2000, DebugException)

/*
 * Local functions
 */

	/*
	 * Data TLB exceptions will bail out to this point
	 * if they can't resolve the lightweight TLB fault.
	 */
data_access:
	STND_EXCEPTION_PROLOG
	mfspr	r5,SPRN_ESR		/* Grab the ESR, save it, pass arg3 */
	stw	r5,_ESR(r21)
	mfspr	r4,SPRN_DEAR		/* Grab the DEAR, save it, pass arg2 */
	stw	r4,_DEAR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r7,STND_EXC
	li	r20,MSR_KERNEL
	rlwimi	r20,r23,0,16,16		/* Copy EE bit from the saved MSR */
	FINISH_EXCEPTION(0x800, do_page_fault) /* do_page_fault(regs, ESR, DEAR) */

/*
 * Both the instruction and data TLB miss get to this
 * point to load the TLB.
 * 	r20 - EA of fault
 * 	r21 - available to use
 *	r22 - Pointer to the 64-bit PTE
 *	r23 - available to use
 *	r24 - available to use
 *	MMUCR - loaded with proper value when we get here
 *	Upon exit, we reload everything and RFI.
 */
finish_tlb_load:
	/*
	 * We set execute, because we don't have the granularity to
	 * properly set this at the page level (Linux problem).
	 * If shared is set, we cause a zero PID->TID load.
	 * Many of these bits are software only.  Bits we don't set
	 * here we (properly should) assume have the appropriate value.
	 */

	/* Load the next available TLB index */
	lis	r23, tlb_44x_index@h
	ori	r23, r23, tlb_44x_index@l
	lwz	r24, 0(r23)
	/* Load the TLB high watermark */
	lis	r23, tlb_44x_hwater@h
	ori	r23, r23, tlb_44x_hwater@l
	lwz	r21, 0(r23)

	
	/* Increment, rollover, and store TLB index */
	addi	r24, r24, 1
	cmpw	0, r24, r21		/* reserve entries 62-63 for kernel */
	ble	7f
	li	r24, 0
7:
	/* Load the next available TLB index */
	lis     r23, tlb_44x_index@h
	ori     r23, r23, tlb_44x_index@l
	stw	r24, 0(r23)

6:
	lwz	r23, 0(r22)			/* Get MS word of PTE */
	lwz	r21, 4(r22)			/* Get LS word of PTE */
	rlwimi	r23, r21, 0, 0 , 19		/* Insert RPN */
	tlbwe	r23, r24, PPC44x_TLB_XLAT	/* Write XLAT */

	/*
	 * Create PAGEID. This is the faulting address plus
	 * a set of static bits. The static bits are page
	 * size and valid. Bits 20  and 21 should be zero
	 * for a page size of 4KB.
	 */
	li	r22, 0x0210			/* Set size and valid */
	mfspr	r23, SPRN_SRR1			/* Get SRR1 */
	andi.	r23, r23, MSR_IS@l
	beq	7f
	ori	r22, r22, PPC44x_TLB_TS@l	/* Set TS=1 */
7:	rlwimi	r20, r22, 0, 20, 31		/* Insert statics */
	tlbwe	r20, r24, PPC44x_TLB_PAGEID	/* Write PAGEID */

	/* FIXME: Staticly setting some permissions */
	li	r23, 0x002d			/* Set UX,UR,SX,SR */
	andi.	r21, r21, 0xffff		/* Clear MS 16 bits */
	andi.	r22, r21, 0x0002		/* _PAGE_HWWRITE? */
	beq	8f
	ori	r23, r23, 0x0002		/* Set SW */
	/* FIXME: Force attributes */
8:	ori	r21, r21, 0x0100		/* Set G */
	/* FIXME: Already set in PTE */
	rlwimi	r21, r23, 0, 26, 31		/* Insert static perms */

	lis	r23,0xffff
	ori	r23,r23,0x0fff			/* Set U0-U3 mask */
	and	r21,r21,r23			/* Clear U0-U3 */
	tlbwe	r21, r24, PPC44x_TLB_ATTRIB	/* Write ATTRIB */

	/* Done...restore registers and get out of here.
	*/
	mfspr	r21, SPRG7R
	mtcr	r21
	mfspr	r24, SPRG6R
	mfspr	r23, SPRG5R
	mfspr	r22, SPRG4R
	mfspr	r21, SPRG1
	mfspr	r20, SPRG0
	rfi					/* Force context change */

/*
 * Global functions
 */

/*
 * extern void giveup_altivec(struct task_struct *prev)
 *
 * The 44x core does not have an AltiVec unit.
 */
_GLOBAL(giveup_altivec)
	blr

/*
 * extern void giveup_fpu(struct task_struct *prev)
 *
 * The 44x core does not have an FPU.
 */
_GLOBAL(giveup_fpu)
	blr

/*
 * extern void abort(void)
 *
 * At present, this routine just applies a system reset.
 */ 
_GLOBAL(abort)
        mfspr   r13,SPRN_DBCR0
        oris    r13,r13,DBCR_RST(DBCR_RST_SYSTEM)@h
        mtspr   SPRN_DBCR0,r13

_GLOBAL(set_context)

#ifdef CONFIG_BDI_SWITCH
	/* Context switch the PTE pointer for the Abatron BDI2000.
	 * The PGDIR is the second parameter.
	 */
	lis	r5, abatron_pteptrs@h
	ori	r5, r5, abatron_pteptrs@l
	stw	r4, 0x4(r5)
#endif
	mtspr	SPRN_PID,r3
	isync			/* Force context change */
	blr

/*
 * This code finishes saving the registers to the exception frame
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
	mfspr   r24,SPRN_DBCR0
	stw     r24,THREAD_DBCR0(r23)	/* Save Debug Control in thread_struct */
	addi	r24,r1,STACK_FRAME_OVERHEAD
	stw	r24,PT_REGS(r23)
2:	addi	r2,r23,-THREAD		/* Set r2 to current thread */
	mflr	r23
	lwz	r24,8(r23)		/* Emulate classic PPC vectors */
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
	bne	3f			/* It is a critical/machine check exception... */

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
	rfi

	/* Critical/Machine check exception jump path
	*/
3:	cmpwi	r7,CRIT_EXC		/* Critical or machine check ? */
	mtlr	r23			/* Set up the return pointer */
	bne	4f
	mtspr	SPRN_CSRR0,r24		/* Set up the instruction pointer */
	mtspr	SPRN_CSRR1,r20		/* Set up the machine state register */
	SYNC
	rfci

4:	mtspr	SPRN_MCSRR0,r24		/* Set up the instruction pointer */
	mtspr	SPRN_MCSRR1,r20		/* Set up the machine state register */
	SYNC	
	RFMCI

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

/*
 * We put a few things here that have to be page-aligned. This stuff
 * goes at the beginning of the data segment, which is page-aligned.
 */
	.data
_GLOBAL(sdata)
_GLOBAL(empty_zero_page)
	.space	4096

/*
 * To support >32-bit physical addresses, we use an 8KB pgdir.
 */
_GLOBAL(swapper_pg_dir)
	.space	8192

/*
 * This space gets a copy of optional info passed to us by the bootstrap
 * which is used to pass parameters into the kernel like root=/dev/sda1, etc.
 */
_GLOBAL(cmd_line)
	.space	512

/*
 * Room for two PTE pointers, usually the kernel and current user pointers
 * to their respective root page table.
 */
abatron_pteptrs:
	.space	8
