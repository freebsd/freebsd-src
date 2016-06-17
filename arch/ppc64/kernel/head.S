/*
 *  arch/ppc64/kernel/head.S
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *
 *  Adapted for 64bit PowerPC by Dave Engebretsen, Peter Bergner, and
 *    Mike Corrigan {engebret|bergner|mikejc}@us.ibm.com
 *
 *  VMX/Altivec port from ppc32 (C) IBM 2003
 *   Denis Joseph Barrow (dj@de.ibm.com,barrow_dj@yahoo.com)
 *   additional debugging & 2.4-2.5 VMX port
 *   Ben Herrenschmidt 	(benh@kernel.crashing.org)
 *  This file contains the low-level support and setup for the
 *  PowerPC-64 platform, including trap and interrupt dispatch.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#define SECONDARY_PROCESSORS

#include "ppc_asm.h"
#include "ppc_defs.h"
#include <asm/processor.h>
#include <asm/page.h>
#include <linux/config.h>
#include <asm/mmu.h>
#include <asm/perfmon.h>
#include <asm/cputable.h>

#ifdef CONFIG_PPC_ISERIES
#define DO_SOFT_DISABLE
#endif

/* copy saved SOFTE bit or EE bit from saved MSR depending
 * if we are doing soft-disable or not
 */
#ifdef DO_SOFT_DISABLE
#define DO_COPY_EE()	ld	r20,SOFTE(r1)
#else
#define DO_COPY_EE()	rldicl	r20,r23,49,63
#endif

/*
 * hcall interface to pSeries LPAR
 */
#define HSC .long 0x44000022
#define H_SET_ASR		0x30

/*
 * We layout physical memory as follows:
 * 0x0000 - 0x00ff : Secondary processor spin code
 * 0x0100 - 0x2fff : pSeries Interrupt prologs
 * 0x3000 - 0x3fff : Interrupt support
 * 0x4000 - 0x4fff : NACA
 * 0x5000 - 0x5fff : SystemCfg
 * 0x6000          : iSeries and common interrupt prologs
 * 0x9000 - 0x9fff : Initial segment table
 */

/*
 *   SPRG Usage
 *
 *   Register          Definition
 *
 *   SPRG0             reserved for hypervisor
 *   SPRG1             temp - used to save gpr
 *   SPRG2             temp - used to save gpr
 *   SPRG3             virt addr of paca
 */

/*
 * Entering into this code we make the following assumptions:
 *  For pSeries:
 *   1. The MMU is off & open firmware is running in real mode.
 *   2. The kernel is entered at __start
 *
 *  For iSeries:
 *   1. The MMU is on (as it always is for iSeries)
 *   2. The kernel is entered at SystemReset_Iseries
 */

	.text
	.globl  _stext
_stext:
_STATIC(__start)
	b .__start_initialization_pSeries
	.long  0x0

	. = 0x8
	.globl	__zero
__zero:
	.llong	0

	/* At offset 0x20, there is a pointer to iSeries LPAR data.
	 * This is required by the hypervisor */
	. = 0x20
	.llong hvReleaseData-KERNELBASE

	/* At offset 0x28 and 0x30 are offsets to the msChunks
	 * array (used by the iSeries LPAR debugger to do translation
	 * between physical addresses and absolute addresses) and
	 * to the pidhash table (also used by the debugger) */
	.llong msChunks-KERNELBASE
	.llong pidhash-KERNELBASE

	/* Offset 0x38 - Pointer to start of embedded System.map */
	.globl	embedded_sysmap_start
embedded_sysmap_start:
	.llong	0
	/* Offset 0x40 - Pointer to end of embedded System.map */
	.globl	embedded_sysmap_end
embedded_sysmap_end:
	.llong	0

	/* Secondary processors spin on this value until it goes to 1. */
	.globl  __secondary_hold_spinloop
__secondary_hold_spinloop:
	.llong  0x0

	/* Secondary processors write this value with their cpu # */
	/* after they enter the spin loop immediatly below.       */
	.globl  __secondary_hold_acknowledge
__secondary_hold_acknowledge:
	.llong  0x0

	. = 0x60
/*
 * The following code is used on pSeries to hold secondary processors
 * in a spin loop after they have been freed from OpenFirmware, but
 * before the bulk of the kernel has been relocated.  This code
 * is relocated to physical address 0x60 before prom_init is run.
 * All of it must fit below the first exception vector at 0x100.
 */
_GLOBAL(__secondary_hold)
	mfmsr	r24
	ori	r24,r24,MSR_RI
	mtmsrd	r24			/* RI on */

	/* Grab our linux cpu number */
	mr      r24,r3

	/* Tell the master cpu we're here */
	/* Relocation is off & we are located at an address less */
	/* than 0x100, so only need to grab low order offset.    */
	std     r24,__secondary_hold_acknowledge@l(0)

	/* All secondary cpu's wait here until told to start. */
100:    ld      r4,__secondary_hold_spinloop@l(0)
	cmpdi   0,r4,1
	bne     100b

#ifdef CONFIG_HMT
	b	.hmt_init
#else
#ifdef CONFIG_SMP
	mr      r3,r24
	b       .pseries_secondary_smp_init
#else
	BUG_OPCODE
#endif
#endif

/*
 * The following macros define the code that appears as
 * the prologue to each of the exception handlers.  They
 * are split into two parts to allow a single kernel binary
 * to be used for pSeries, and iSeries.
 */

/*
 * We make as much of the exception code common between native
 * exception handlers (including pSeries LPAR) and iSeries LPAR
 * implementations as possible.
 */

/*
 * This is the start of the interrupt handlers for pSeries
 * This code runs with relocation off.
 */
#define EX_SRR0		0
#define EX_SRR1		8
#define EX_R20		16
#define EX_R21		24
#define EX_R22		32
#define EX_R23		40
#define EX_DAR		48
#define EX_DSISR	56
#define EX_CCR   	60
#define EX_TRAP   	60

#define EXCEPTION_PROLOG_PSERIES(n,label)                                \
	mtspr   SPRG2,r20;              /* use SPRG2 as scratch reg   */ \
	mtspr   SPRG1,r21;              /* save r21                   */ \
	mfspr   r20,SPRG3;              /* get paca virt addr         */ \
	ld      r21,PACAEXCSP(r20);     /* get exception stack ptr    */ \
	addi    r21,r21,EXC_FRAME_SIZE; /* make exception frame       */ \
	std	r22,EX_R22(r21);	/* Save r22 in exc. frame     */ \
	li	r22,n;                  /* Save the ex # in exc. frame*/ \
	stw	r22,EX_TRAP(r21);	/*                            */ \
	std	r23,EX_R23(r21);	/* Save r23 in exc. frame     */ \
	mfspr   r22,SRR0;               /* EA of interrupted instr    */ \
	std	r22,EX_SRR0(r21);	/* Save SRR0 in exc. frame    */ \
	mfspr   r23,SRR1;               /* machine state at interrupt */ \
	std	r23,EX_SRR1(r21);	/* Save SRR1 in exc. frame    */ \
	clrrdi  r22,r20,60;             /* Get 0xc part of the vaddr  */ \
	ori	r22,r22,(label)@l;      /* add in the vaddr offset    */ \
		                        /*   assumes *_common < 16b   */ \
	mfmsr   r23;                                                     \
	rotldi  r23,r23,4;                                               \
	ori     r23,r23,0x32B;          /* Set IR, DR, RI, SF, ISF, HV*/ \
	rotldi  r23,r23,60;             /* for generic handlers       */ \
	mtspr   SRR0,r22;                                                \
	mtspr   SRR1,r23;                                                \
	mfcr    r23;                    /* save CR in r23             */ \
	rfid

/*
 * This is the start of the interrupt handlers for iSeries
 * This code runs with relocation on.
 */
#define EXCEPTION_PROLOG_ISERIES(n)	                                      \
	mtspr	SPRG2,r20;		    /* use SPRG2 as scratch reg    */ \
	mtspr   SPRG1,r21;                  /* save r21                    */ \
	mfspr	r20,SPRG3;		    /* get paca                    */ \
	ld      r21,PACAEXCSP(r20);         /* get exception stack ptr     */ \
	addi    r21,r21,EXC_FRAME_SIZE;     /* make exception frame        */ \
	std	r22,EX_R22(r21);	    /* save r22 on exception frame */ \
	li	r22,n;                      /* Save the ex # in exc. frame */ \
	stw	r22,EX_TRAP(r21);	    /*                             */ \
	std	r23,EX_R23(r21);	    /* Save r23 in exc. frame      */ \
	ld      r22,LPPACA+LPPACASRR0(r20); /* Get SRR0 from ItLpPaca      */ \
	std	r22,EX_SRR0(r21);	    /* save SRR0 in exc. frame     */ \
	ld      r23,LPPACA+LPPACASRR1(r20); /* Get SRR1 from ItLpPaca      */ \
	std	r23,EX_SRR1(r21);	    /* save SRR1 in exc. frame     */ \
	mfcr    r23;                        /* save CR in r23              */

/*
 * The common exception prolog is used for all except a few exceptions
 * such as a segment miss on a kernel address.  We have to be prepared
 * to take another exception from the point where we first touch the
 * kernel stack onwards.
 *
 * On entry r20 points to the paca and r21 points to the exception
 * frame on entry, r23 contains the saved CR, and relocation is on.
 */
#define EXCEPTION_PROLOG_COMMON                                           \
	mfspr	r22,SPRG2;		/* Save r20 in exc. frame      */ \
	std	r22,EX_R20(r21);	                                  \
	mfspr	r22,SPRG1;		/* Save r21 in exc. frame      */ \
	std	r22,EX_R21(r21);	                                  \
	mfspr   r22,DAR;                /* Save DAR in exc. frame      */ \
	std	r22,EX_DAR(r21);	                                  \
	std     r21,PACAEXCSP(r20);     /* update exception stack ptr  */ \
		                        /*   iff no protection flt     */ \
	mfspr	r22,DSISR;		/* Save DSISR in exc. frame    */ \
	stw	r22,EX_DSISR(r21);	                                  \
	ld	r22,EX_SRR1(r21);	/* Get SRR1 from exc. frame    */ \
	andi.   r22,r22,MSR_PR;         /* Set CR for later branch     */ \
	mr      r22,r1;                 /* Save r1                     */ \
	subi    r1,r1,INT_FRAME_SIZE;   /* alloc frame on kernel stack */ \
	beq-    1f;                                                       \
	ld      r1,PACAKSAVE(r20);      /* kernel stack to use         */ \
1:      std     r22,GPR1(r1);           /* save r1 in stackframe       */ \
	std     r22,0(r1);              /* make stack chain pointer    */ \
	std     r23,_CCR(r1);           /* save CR in stackframe       */ \
	ld	r22,EX_R20(r21);	/* move r20 to stackframe      */ \
	std	r22,GPR20(r1);		                                  \
	ld	r23,EX_R21(r21);	/* move r21 to stackframe      */ \
	std	r23,GPR21(r1);		                                  \
	ld	r22,EX_R22(r21);	/* move r22 to stackframe      */ \
	std	r22,GPR22(r1);		                                  \
	ld	r23,EX_R23(r21);	/* move r23 to stackframe      */ \
	std	r23,GPR23(r1);		                                  \
	mflr    r22;                    /* save LR in stackframe       */ \
	std     r22,_LINK(r1);                                            \
	mfctr   r23;                    /* save CTR in stackframe      */ \
	std     r23,_CTR(r1);                                             \
	mfspr   r22,XER;                /* save XER in stackframe      */ \
	std     r22,_XER(r1);                                             \
	ld	r23,EX_DAR(r21);	/* move DAR to stackframe      */ \
	std	r23,_DAR(r1);		                                  \
	lwz     r22,EX_DSISR(r21);	/* move DSISR to stackframe    */ \
	std	r22,_DSISR(r1);		                                  \
	lbz	r22,PACAPROCENABLED(r20);                                 \
	std	r22,SOFTE(r1);		                                  \
	ld	r22,EX_SRR0(r21);	/* get SRR0 from exc. frame    */ \
	ld	r23,EX_SRR1(r21);	/* get SRR1 from exc. frame    */ \
	addi    r21,r21,-EXC_FRAME_SIZE;/* pop off exception frame     */ \
	std     r21,PACAEXCSP(r20);                                       \
	SAVE_GPR(0, r1);                /* save r0 in stackframe       */ \
	SAVE_8GPRS(2, r1);              /* save r2 - r13 in stackframe */ \
	SAVE_4GPRS(10, r1);                                               \
	ld      r2,PACATOC(r20);	                                  \
	mr	r13,r20

/*
 * Note: code which follows this uses cr0.eq (set if from kernel),
 * r1, r22 (SRR0), and r23 (SRR1).
 */

/*
 * Exception vectors.
 */
#define STD_EXCEPTION_PSERIES(n, label )	\
	. = n;					\
	.globl label##_Pseries;			\
label##_Pseries:				\
	EXCEPTION_PROLOG_PSERIES( n, label##_common )

#define STD_EXCEPTION_ISERIES( n, label )	\
	.globl label##_Iseries;			\
label##_Iseries:				\
	EXCEPTION_PROLOG_ISERIES( n );          \
	b	label##_common

#define MASKABLE_EXCEPTION_ISERIES( n, label )	\
	.globl label##_Iseries;			\
label##_Iseries:				\
	EXCEPTION_PROLOG_ISERIES( n );		\
	lbz	r22,PACAPROFMODE(r20);		\
	cmpi	0,r22,PMC_STATE_DECR_PROFILE;	\
	beq-	label##_Iseries_profile;	\
label##_Iseries_prof_ret:			\
	lbz	r22,PACAPROCENABLED(r20);	\
	cmpi	0,r22,0;			\
	beq-	label##_Iseries_masked;		\
	b	label##_common;			\
label##_Iseries_profile:			\
	std	r24,48(r21);			\
	std	r25,56(r21);			\
	mflr	r24;				\
	bl	do_profile;			\
	mtlr	r24;				\
	ld	r24,48(r21);			\
	ld	r25,56(r21);			\
	b	label##_Iseries_prof_ret

#define STD_EXCEPTION_COMMON( trap, label, hdlr )	\
	.globl label##_common;			\
label##_common:					\
	EXCEPTION_PROLOG_COMMON;		\
	addi	r3,r1,STACK_FRAME_OVERHEAD;	\
	li	r20,0;				\
	li	r6,trap;			\
	bl      .save_remaining_regs;           \
	bl      hdlr;                           \
	b       .ret_from_except

/*
 * Start of pSeries system interrupt routines
 */
	. = 0x100
	.globl __start_interupts
__start_interupts:

	STD_EXCEPTION_PSERIES( 0x100, SystemReset )
	STD_EXCEPTION_PSERIES( 0x200, MachineCheck )
	STD_EXCEPTION_PSERIES( 0x300, DataAccess )
	STD_EXCEPTION_PSERIES( 0x380, DataAccessSLB )
	STD_EXCEPTION_PSERIES( 0x400, InstructionAccess )
	STD_EXCEPTION_PSERIES( 0x480, InstructionAccessSLB )
	STD_EXCEPTION_PSERIES( 0x500, HardwareInterrupt )
	STD_EXCEPTION_PSERIES( 0x600, Alignment )
	STD_EXCEPTION_PSERIES( 0x700, ProgramCheck )
	STD_EXCEPTION_PSERIES( 0x800, FPUnavailable )
	STD_EXCEPTION_PSERIES( 0x900, Decrementer )
	STD_EXCEPTION_PSERIES( 0xa00, Trap_0a )
	STD_EXCEPTION_PSERIES( 0xb00, Trap_0b )
	STD_EXCEPTION_PSERIES( 0xc00, SystemCall )
	STD_EXCEPTION_PSERIES( 0xd00, SingleStep )
	STD_EXCEPTION_PSERIES( 0xe00, Trap_0e )
	. = 0xf00
	b   PerformanceMonitor_Pseries
	STD_EXCEPTION_PSERIES( 0xf20, AltiVecUnavailable )
	. = 0xf90
	.globl PerformanceMonitor_Pseries
PerformanceMonitor_Pseries:
	EXCEPTION_PROLOG_PSERIES( 0xf00, PerformanceMonitor_common )
	STD_EXCEPTION_PSERIES( 0x1300, InstructionBreakpoint )
	STD_EXCEPTION_PSERIES( 0x1700, AltiVecAssist )
	STD_EXCEPTION_PSERIES( 0x1800, ThermalInterrupt)


	/* Space for the naca.  Architected to be located at real address
	 * 0x4000.  Various tools rely on this location being fixed.
	 * The first dword of the naca is required by iSeries LPAR to
	 * point to itVpdAreas.  On pSeries native, this value is not used.
	 */
	. = 0x4000
	.globl __end_interupts
	.globl __start_naca
__end_interupts:
__start_naca:
	.llong itVpdAreas
	.llong 0x0
	.llong 0x0
	.llong paca

	. = 0x5000
	.globl __end_naca
	.globl __start_systemcfg
__end_naca:
__start_systemcfg:
	. = 0x6000
	.globl __end_systemcfg
__end_systemcfg:

	/*
	 * The iSeries LPAR map is at this fixed address
	 * so that the HvReleaseData structure can address
	 * it with a 32-bit offset.
	 *
	 * The VSID values below are dependent on the
	 * VSID generation algorithm.  See include/asm/mmu_context.h.
	 */

	.llong	1		/* # ESIDs to be mapped by hypervisor         */
	.llong	1		/* # memory ranges to be mapped by hypervisor */
	.llong	STAB0_PAGE	/* Page # of segment table within load area   */
	.llong	0		/* Reserved */
	.llong  0		/* Reserved */
	.llong  0		/* Reserved */
	.llong	0		/* Reserved */
	.llong	0		/* Reserved */
	.llong	0x0c00000000	/* ESID to map (Kernel at EA = 0xC000000000000000) */
	.llong	0x06a99b4b14    /* VSID to map (Kernel at VA = 0x6a99b4b140000000) */
	.llong	8192		/* # pages to map (32 MB) */
	.llong	0		/* Offset from start of loadarea to start of map */
	.llong	0x0006a99b4b140000	/* VPN of first page to map */

	. = 0x6100

/***  ISeries-LPAR interrupt handlers ***/

	STD_EXCEPTION_ISERIES( 0x200, MachineCheck )
	STD_EXCEPTION_ISERIES( 0x300, DataAccess )
	STD_EXCEPTION_ISERIES( 0x380, DataAccessSLB )
	STD_EXCEPTION_ISERIES( 0x400, InstructionAccess )
	STD_EXCEPTION_ISERIES( 0x480, InstructionAccessSLB )
	MASKABLE_EXCEPTION_ISERIES( 0x500, HardwareInterrupt )
	STD_EXCEPTION_ISERIES( 0x600, Alignment )
	STD_EXCEPTION_ISERIES( 0x700, ProgramCheck )
	STD_EXCEPTION_ISERIES( 0x800, FPUnavailable )
	MASKABLE_EXCEPTION_ISERIES( 0x900, Decrementer )
	STD_EXCEPTION_ISERIES( 0xa00, Trap_0a )
	STD_EXCEPTION_ISERIES( 0xb00, Trap_0b )
	STD_EXCEPTION_ISERIES( 0xc00, SystemCall )
	STD_EXCEPTION_ISERIES( 0xd00, SingleStep )
	STD_EXCEPTION_ISERIES( 0xe00, Trap_0e )
	MASKABLE_EXCEPTION_ISERIES( 0xf00, PerformanceMonitor )

	.globl SystemReset_Iseries
SystemReset_Iseries:
	mfspr	13,SPRG3		/* Get paca address */
	lhz	r24,PACAPACAINDEX(r13)	/* Get processor # */
	cmpi	0,r24,0			/* Are we processor 0? */
	beq	.__start_initialization_iSeries	/* Start up the first processor */
	mfspr	r4,CTRLF
	li	r5,RUNLATCH		/* Turn off the run light */
	andc	r4,r4,r5
	mtspr	CTRLT,r4

1:
	HMT_LOW
#ifdef CONFIG_SMP
	lbz	r23,PACAPROCSTART(r13)	/* Test if this processor
					 * should start */
	sync
	LOADADDR(r3,current_set)
	sldi	r28,r24,4		/* get current_set[cpu#] */
	ldx	r3,r3,r28
	addi	r1,r3,TASK_UNION_SIZE
	subi	r1,r1,STACK_FRAME_OVERHEAD

	cmpi	0,r23,0
	beq	iseries_secondary_smp_loop	/* Loop until told to go */
#ifdef SECONDARY_PROCESSORS
	bne	.__secondary_start		/* Loop until told to go */
#endif
iseries_secondary_smp_loop:
	/* Let the Hypervisor know we are alive */
	/* 8002 is a call to HvCallCfg::getLps, a harmless Hypervisor function */
	lis	r3,0x8002
	rldicr	r3,r3,32,15		/* r0 = (r3 << 32) & 0xffff000000000000 */
#else /* CONFIG_SMP */
	/* Yield the processor.  This is required for non-SMP kernels
	   which are running on multi-threaded machines. */
	lis	r3,0x8000
	rldicr	r3,r3,32,15		/* r3 = (r3 << 32) & 0xffff000000000000 */
	addi	r3,r3,18		/* r3 = 0x8000000000000012 which is "yield" */
	li	r4,0			/* "yield timed" */
	li	r5,-1			/* "yield forever" */
#endif /* CONFIG_SMP */
	li	r0,-1			/* r0=-1 indicates a Hypervisor call */
	sc				/* Invoke the hypervisor via a system call */
	mfspr	r13,SPRG3		/* Put r13 back - why???? */
	b	1b			/* If SMP not configured, secondaries
					 * loop forever */

	.globl HardwareInterrupt_Iseries_masked
HardwareInterrupt_Iseries_masked:
	b	maskable_exception_exit

	.globl PerformanceMonitor_Iseries_masked
PerformanceMonitor_Iseries_masked:
	li	r22,1
	stb	r22,PACALPPACA+LPPACAPDCINT(r20)
	b	maskable_exception_exit

	.globl Decrementer_Iseries_masked
Decrementer_Iseries_masked:
	li	r22,1
	stb	r22,PACALPPACA+LPPACADECRINT(r20)
	lwz	r22,PACADEFAULTDECR(r20)
	mtspr	DEC,r22
maskable_exception_exit:
	mtcrf	0xff,r23		/* Restore regs and free exception frame */
	ld	r22,EX_SRR0(r21)
	ld	r23,EX_SRR1(r21)
	mtspr	SRR0,r22
	mtspr	SRR1,r23
	ld	r22,EX_R22(r21)
	ld	r23,EX_R23(r21)
	mfspr	r21,SPRG1
	mfspr	r20,SPRG2
	rfid

/*
 * Data area reserved for FWNMI option.
 */
        .= 0x7000
	.globl fwnmi_data_area
fwnmi_data_area:

/*
 * Vectors for the FWNMI option.  Share common code.
 */
	. = 0x8000
	.globl SystemReset_FWNMI
SystemReset_FWNMI:
	EXCEPTION_PROLOG_PSERIES(0x100, SystemReset_common)
	.globl MachineCheck_FWNMI
MachineCheck_FWNMI:
	EXCEPTION_PROLOG_PSERIES(0x200, MachineCheck_common)

	/*
	 * Space for the initial segment table
	 * For LPAR, the hypervisor must fill in at least one entry
	 * before we get control (with relocate on)
	 */
	. = STAB0_PHYS_ADDR
	.globl __start_stab
__start_stab:

	. = (STAB0_PHYS_ADDR + PAGE_SIZE)
	.globl __end_stab
__end_stab:


/*** Common interrupt handlers ***/

	STD_EXCEPTION_COMMON( 0x100, SystemReset, .SystemResetException )
	STD_EXCEPTION_COMMON( 0x200, MachineCheck, .MachineCheckException )
	STD_EXCEPTION_COMMON( 0x900, Decrementer, .timer_interrupt )
	STD_EXCEPTION_COMMON( 0xa00, Trap_0a, .UnknownException )
	STD_EXCEPTION_COMMON( 0xb00, Trap_0b, .UnknownException )
	STD_EXCEPTION_COMMON( 0xd00, SingleStep, .SingleStepException )
	STD_EXCEPTION_COMMON( 0xe00, Trap_0e, .UnknownException )

	.globl AltiVecUnavailable_common
AltiVecUnavailable_common:
	EXCEPTION_PROLOG_COMMON
#ifdef CONFIG_ALTIVEC
	bne	.load_up_altivec	/* if from user, just load it up */
#endif
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0xf20
	bl      .save_remaining_regs
#ifndef CONFIG_ALTIVEC
	beq    1f
	bl     .IllegalAltiVecInstruction
	b      .ret_from_except
1:
#endif
	bl      .KernelAltiVecUnavailableException
	BUG_OPCODE

	.global AltiVecAssist_common
AltiVecAssist_common:
	EXCEPTION_PROLOG_COMMON
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x1700
	bl	.save_remaining_regs
	bl	.AltiVecAssistException
	b	.ret_from_except

	.global ThermalInterrupt_common
ThermalInterrupt_common:
	EXCEPTION_PROLOG_COMMON
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x1800
	bl      .save_remaining_regs
	bl	.ThermalInterrupt
	BUG_OPCODE


	STD_EXCEPTION_COMMON(0x1300, InstructionBreakpoint, .InstructionBreakpointException )

/*
 * Return from an exception which is handled without calling
 * save_remaining_regs.  The caller is assumed to have done
 * EXCEPTION_PROLOG_COMMON.
 */
fast_exception_return:
	ld      r3,_CCR(r1)
	ld      r4,_LINK(r1)
	ld      r5,_CTR(r1)
	ld      r6,_XER(r1)
	mtcr    r3
	mtlr    r4
	mtctr   r5
	mtspr   XER,r6
	REST_GPR(0, r1)
	REST_8GPRS(2, r1)
	REST_4GPRS(10, r1)

	mfmsr	r20
	li	r21, MSR_RI
	andc	r20,r20,r21
	mtmsrd	r20,1

	mtspr   SRR1,r23
	mtspr   SRR0,r22
	REST_4GPRS(20, r1)
	ld      r1,GPR1(r1)
	rfid

/*
 * Here r20 points to the PACA, r21 to the exception frame,
 * r23 contains the saved CR.
 * r20 - r23, SRR0 and SRR1 are saved in the exception frame.
 */
	.globl DataAccess_common
DataAccess_common:
	mfspr   r22,DSISR
	andis.  r22,r22,0x0020
	beq	1f
	mfspr   r22,DAR			/* if it's a segment table miss, */
	srdi    r22,r22,60		/* check if it is in kernel region */
	cmpi    0,r22,0xc		/* and call do_stab_bolted if so */
	beq     .do_stab_bolted
	cmpi    0,r22,0xb
	beq     .do_stab_bolted

1:
	EXCEPTION_PROLOG_COMMON
	ld      r3,_DSISR(r1)
	andis.	r0,r3,0xa450		/* weird error? */
	bne	1f			/* if not, try to put a PTE */
	andis.	r0,r3,0x0020		/* Is it a page table fault? */
	rlwinm	r4,r3,32-23,29,29	/* DSISR_STORE -> _PAGE_RW */
	ld      r3,_DAR(r1)             /* into the hash table */

	beq	2f			/* If so handle it */
	li	r4,0x300                /* Trap number */
	bl	.do_stab_SI
	b	1f

2:	li	r5,0x300
	bl	.do_hash_page_DSI 	/* Try to handle as hpte fault */
1:
	ld      r4,_DAR(r1)
	ld      r5,_DSISR(r1)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x300
	bl      .save_remaining_regs
	bl      .do_page_fault
	b       .ret_from_except

	.globl DataAccessSLB_common
DataAccessSLB_common:
	mfspr   r22,DAR
	srdi    r22,r22,60
	cmpi    0,r22,0xc
	beq     .do_slb_bolted
	cmpi    0,r22,0xb
	beq     .do_slb_bolted

	EXCEPTION_PROLOG_COMMON
	ld      r3,_DAR(r1)
	li      r4,0x380                /* Exception vector  */
	bl	.ste_allocate
	or.	r3,r3,r3		/* Check return code */
	beq     fast_exception_return   /* Return if we succeeded */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x380
	li	r5,0
	bl      .save_remaining_regs
	bl      .do_page_fault
	b       .ret_from_except

	.globl InstructionAccess_common
InstructionAccess_common:
	EXCEPTION_PROLOG_COMMON

	andis.	r0,r23,0x0020		/* no ste found? */
	beq	2f
	mr	r3,r22			/* SRR0 at interrupt */
	li	r4,0x400		/* Trap number       */
	bl	.do_stab_SI
	b	1f

2:	andis.	r0,r23,0x4000		/* no pte found? */
	beq	1f			/* if so, try to put a PTE */
	mr	r3,r22			/* into the hash table */
	bl	.do_hash_page_ISI	/* Try to handle as hpte fault */
1:
	mr	r4,r22
	rlwinm	r5,r23,0,4,4		/* We only care about PR in error_code */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x400
	bl      .save_remaining_regs
	bl      .do_page_fault
	b       .ret_from_except

	.globl InstructionAccessSLB_common
InstructionAccessSLB_common:
	EXCEPTION_PROLOG_COMMON
	mr      r3,r22                  /* SRR0 = NIA        */
	li	r4,0x480                /* Exception vector  */
	bl	.ste_allocate
	or.	r3,r3,r3		/* Check return code */
	beq     fast_exception_return   /* Return if we succeeded */

	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x480
	li	r5,0
	bl      .save_remaining_regs
	bl      .do_page_fault
	b       .ret_from_except

	.globl HardwareInterrupt_common
HardwareInterrupt_common:
	EXCEPTION_PROLOG_COMMON
HardwareInterrupt_entry:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r20,0
	li	r6,0x500
	bl      .save_remaining_regs
	/* Determine if need to run do_irq on a hardware interrupt stack  */
	/*   The first invocation of do_irq will occur on the kernel      */
	/*   stack in the current stack                                   */
	/*   All other invocations of do_irq will run on the hardware     */
	/*   interrupt stack associated with the PACA of the current      */
	/*   processor.                                                   */
	/*                                                                */
	/*  The call to do_irq will preserve the value of r14 - r31       */
	/*                                                                */
	mfspr	r20,SPRG3 		    /* get paca                   */
	lbz     r21,PACAHRDWINTCOUNT(r20)    /* get hardware interrupt cnt */
	cmpi    0,r21,0                     /*                            */
	addi    r21,r21,1                   /* incr hardware interrupt cnt*/
	stb     r21,PACAHRDWINTCOUNT(r20)   /*                            */
	bne     2f                          /*                            */

	mr      r14,r1                      /* preserve current r1        */
	ld      r1,PACAHRDWINTSTACK(r20)    /*                            */
	std     r14,0(r1)                   /* set the back chain         */
	bl      .do_IRQ
	lbz     r22,PACAHRDWINTCOUNT(r20)   /* get hardware interrupt cnt */
	cmp     0,r22,r21                   /* debug test                 */
	bne     3f
	subi    r21,r21,1
	stb     r21,PACAHRDWINTCOUNT(r20)   /*                            */
	mr      r1,r14                      /*                            */
	b       .ret_from_except

2:
	bl      .do_IRQ

	lbz     r22,PACAHRDWINTCOUNT(r20)   /* get hardware interrupt cnt */
	cmp     0,r22,r21                   /* debug test                 */
	bne     3f                          /*                            */
	subi    r21,r21,1                   /* decr hardware interrupt cnt*/
	stb     r21,PACAHRDWINTCOUNT(r20)   /*                            */

	b       .ret_from_except

3:
	/* error - counts out of sync                                      */
#ifdef CONFIG_XMON
	bl	.xmon
#endif
#ifdef CONFIG_KDB
        /*          kdb(KDB_REASON_FAULT,regs->trap,regs); */
	li      r3,1
	li 	r4,0x200
	li      r5,0
	bl	.kdb
#endif
4:	b	4b


	.globl Alignment_common
Alignment_common:
	EXCEPTION_PROLOG_COMMON
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x600
	bl      .save_remaining_regs
	bl      .AlignmentException
	b       .ret_from_except

	.globl ProgramCheck_common
ProgramCheck_common:
	EXCEPTION_PROLOG_COMMON
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x700
	bl      .save_remaining_regs
	bl      .ProgramCheckException
	b       .ret_from_except

	.globl FPUnavailable_common
FPUnavailable_common:
	EXCEPTION_PROLOG_COMMON
	bne	.load_up_fpu		/* if from user, just load it up */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	DO_COPY_EE()
	li	r6,0x800
	bl      .save_remaining_regs
	bl      .KernelFPUnavailableException
	BUG_OPCODE

	.globl SystemCall_common
SystemCall_common:
	EXCEPTION_PROLOG_COMMON
#ifdef CONFIG_PPC_ISERIES
	cmpi	0,r0,0x5555		/* Special syscall to handle pending */
	bne+	1f			/* interrupts */
	andi.	r6,r23,MSR_PR		/* Only allowed from kernel */
	beq+	HardwareInterrupt_entry
1:
#endif
	std	r3,ORIG_GPR3(r1)
	DO_COPY_EE()
	li	r6,0xC00
	bl      .save_remaining_regs
	bl      .DoSyscall
	b       .ret_from_except

	.globl PerformanceMonitor_common
PerformanceMonitor_common:	
	EXCEPTION_PROLOG_COMMON
	bl	.PerformanceMonitorException
	b	fast_exception_return
	
_GLOBAL(PerformanceMonitorException)
	sync
	mfspr	r7,SPRG3
	lbz	r8,PACAPROFMODE(r7)
	cmpi	0,r8,PMC_STATE_PROFILE_KERN
	beq 	5f
	cmpi	0,r8,PMC_STATE_TRACE_KERN
	beq 	6f
	cmpi	0,r8,PMC_STATE_TRACE_USER
	beq 	9f
	cmpi	0,r8,PMC_STATE_TIMESLICE
	beq 	10f
	blr

	/* PMC Profile Kernel */
5:	mfspr   r9,SIAR	
	srdi    r8,r9,60
	cmpi    0,r8,0xc
	beq     3f
	li	r9,0xc
	sldi	r9,r9,60
3:      ld	r8,PACAPROFSTEXT(r7)	/* _stext */
	subf	r9,r8,r9		/* offset into kernel */
	lwz	r8,PACAPROFSHIFT(r7)
	srd	r9,r9,r8
	lwz	r8,PACAPROFLEN(r7)	/* length of profile table (-1) */
	srdi	r8,r8,2
	cmpd	r9,r8		/* off end? */
	ble	1f
	mr	r9,r8			/* force into last entry */
	srdi	r9,r9,2
1:	sldi	r9,r9,2		/* convert to offset into buffer */
	ld	r8,PACAPROFBUFFER(r7)	/* profile buffer */
	add	r8,r8,r9
2:	lwarx	r9,0,r8		/* atomically increment */
	addi	r9,r9,1
	stwcx.	r9,0,r8
	bne-	2b
	addi	r10,r7,PACAPMC1
	addi	r7,r7,PACAPMCC1
	b	7f

	/* PMC Trace Kernel */
6:	LOADADDR(r11, perfmon_base)
  	addi	r8,r11,32
	ld	r12,24(r11)
	subi	r12,r12,1
8:	ldarx	r10,0,r8
	addi	r9,r10,16
	and	r9,r9,r12
	stdcx.	r9,0,r8
	bne-	8b
	ld	r9,16(r11)	/* profile buffer */
	add	r8,r9,r10
  	mfspr   r9,SIAR	
	std	r9,0(r8)
	mfspr   r9,SDAR	
	std	r9,8(r8)
	addi	r10,r7,PACAPMC1
	addi	r7,r7,PACAPMCC1
	b	7f

	/* PMC Trace User */
9:	LOADADDR(r11, perfmon_base)
#if 0
  	addi	r8,r11,32
	ld	r12,24(r11)
	subi	r12,r12,1
8:	ldarx	r10,0,r8
	addi	r9,r10,16
	and	r9,r9,r12
	stdcx.	r9,0,r8
	bne-	8b
	ld	r9,16(r11)	/* profile buffer */
	add	r8,r9,r10
  	mfspr   r9,SIAR	
	std	r9,0(r8)
	mfspr   r9,SDAR	
	std	r9,8(r8)
	addi	r10,r13,THREAD+THREAD_PMC1
	addi	r7,r13,THREAD+THREAD_PMCC1
#endif
	b	7f

	/* PMC Timeslice */
10:	addi	r10,r7,PACAPMC1
	addi	r7,r7,PACAPMCC1
	b	7f

	/* Accumulate counter values for kernel traces */
7:	ld	r9,0(r7)
	mfspr   r8,PMC1	
	add	r9,r9,r8
	std	r9,0(r7)
	ld	r9,8(r7)
	mfspr	r8,PMC2
	add	r9,r9,r8
	std	r9,8(r7)
	ld	r9,16(r7)
	mfspr	r8,PMC3
	add	r9,r9,r8
	std	r9,16(r7)
	ld	r9,24(r7)
	mfspr	r8,PMC4
	add	r9,r9,r8
	std	r9,24(r7)
	ld	r9,32(r7)
	mfspr	r8,PMC5
	add	r9,r9,r8
	std	r9,32(r7)
	ld	r9,40(r7)
	mfspr	r8,PMC6
	add	r9,r9,r8
	std	r9,40(r7)
	ld	r9,48(r7)
	mfspr	r8,PMC7
	add	r9,r9,r8
	std	r9,48(r7)
	ld	r9,56(r7)
	mfspr	r8,PMC8
	add	r9,r9,r8
	std	r9,56(r7)

	/* Reset all counters for kernel traces */
	ld	r9,0(r10)
	mtspr	PMC1,r9
	ld	r9,8(r10)
	mtspr	PMC2,r9
	ld	r9,16(r10)
	mtspr	PMC3,r9
	ld	r9,24(r10)
	mtspr	PMC4,r9
	ld	r9,32(r10)
	mtspr	PMC5,r9
	ld	r9,40(r10)
	mtspr	PMC6,r9
	ld	r9,48(r10)
	mtspr	PMC7,r9
	ld	r9,56(r10)
	mtspr	PMC8,r9
	ld	r9,64(r10)
	mtspr	MMCR0,r9
	ld	r9,72(r10)
	mtspr	MMCR1,r9
	ld	r9,80(r10)
	mtspr	MMCRA,r9
	blr

_GLOBAL(do_hash_page_ISI)
	li	r4,0
_GLOBAL(do_hash_page_DSI)
	rlwimi	r4,r23,32-13,30,30	/* Insert MSR_PR as _PAGE_USER */
	ori	r4,r4,1			/* add _PAGE_PRESENT */

	mflr	r21			/* Save LR in r21 */

#ifdef DO_SOFT_DISABLE
	/*
	 * We hard enable here (but first soft disable) so that the hash_page
	 * code can spin on the hash_table_lock with problem on a shared
	 * processor.
	 */
	li	r0,0
	stb	r0,PACAPROCENABLED(r20)	/* Soft Disabled */

	mfmsr	r0
	ori	r0,r0,MSR_EE+MSR_RI
	mtmsrd	r0			/* Hard Enable, RI on */
#endif

	/*
	 * r3 contains the faulting address
	 * r4 contains the required access permissions
	 * r5 contains the trap number
	 *
	 * at return r3 = 0 for success
	 */

	bl	.hash_page		/* build HPTE if possible */

#ifdef DO_SOFT_DISABLE
	/*
	 * Now go back to hard disabled.
	 */
	mfmsr	r0
	li	r4,0
	ori	r4,r4,MSR_EE+MSR_RI
	andc	r0,r0,r4
	mtmsrd	r0			/* Hard Disable, RI off */

	ld	r0,SOFTE(r1)
	cmpdi	0,r0,0			/* See if we will soft enable in */
					/* save_remaining_regs */
	beq	5f
	CHECKANYINT(r4,r5)
	bne-	HardwareInterrupt_entry	/* Convert this DSI into an External */
					/* to process interrupts which occurred */
					/* during hash_page */
5:
	stb	r0,PACAPROCENABLED(r20)	/* Restore soft enable/disable status */
#endif
	or.	r3,r3,r3		/* Check return code */
	beq     fast_exception_return   /* Return from exception on success */

	mtlr    r21                     /* restore LR */
	blr                             /* Return to DSI or ISI on failure */

/*
 * r20 points to the PACA, r21 to the exception frame,
 * r23 contains the saved CR.
 * r20 - r23, SRR0 and SRR1 are saved in the exception frame.
 * We assume we aren't going to take any exceptions during this procedure.
 */
_GLOBAL(do_stab_bolted)
	stw	r23,EX_CCR(r21)	/* save CR in exc. frame */

	/* (((ea >> 28) & 0x1fff) << 15) | (ea >> 60) */
	mfspr	r21,DAR
	rldicl  r20,r21,36,32   /* Permits a full 32b of ESID */
	rldicr  r20,r20,15,48
	rldicl  r21,r21,4,60
	or      r20,r20,r21

	li      r21,9           /* VSID_RANDOMIZER */
	sldi    r21,r21,32
	oris    r21,r21,58231
	ori     r21,r21,39831

	mulld   r20,r20,r21
	clrldi  r20,r20,28      /* r20 = vsid */

	mfsprg  r21,3
	ld      r21,PACASTABVIRT(r21)

	/* Hash to the primary group */
	mfspr	r22,DAR
	rldicl  r22,r22,36,59
	rldicr  r22,r22,7,56
	or      r21,r21,r22     /* r21 = first ste of the group */

	/* Search the primary group for a free entry */
	li      r22,0
1:
	ld      r23,0(r21)      /* Test valid bit of the current ste   */
	rldicl  r23,r23,57,63
	cmpwi   r23,0
	bne     2f
	ld      r23,8(r21)      /* Get the current vsid part of the ste */
	rldimi  r23,r20,12,0    /* Insert the new vsid value            */
	std     r23,8(r21)      /* Put new entry back into the stab     */
	eieio                  /* Order vsid update                    */
	ld      r23,0(r21)      /* Get the esid part of the ste         */
	mfspr	r20,DAR        /* Get the new esid                     */
	rldicl  r20,r20,36,28  /* Permits a full 36b of ESID           */
	rldimi  r23,r20,28,0    /* Insert the new esid value            */
	ori     r23,r23,144      /* Turn on valid and kp                 */
	std     r23,0(r21)      /* Put new entry back into the stab     */
	sync                   /* Order the update                     */
	b       3f
2:
	addi    r22,r22,1
	addi    r21,r21,16
	cmpldi  r22,7
	ble     1b

	/* Stick for only searching the primary group for now.          */
	/* At least for now, we use a very simple random castout scheme */
	/* Use the TB as a random number ;  OR in 1 to avoid entry 0    */
	mftb    r22
	andi.   r22,r22,7
	ori	r22,r22,1
	sldi	r22,r22,4

	/* r21 currently points to and ste one past the group of interest */
	/* make it point to the randomly selected entry                   */
	subi	r21,r21,128
	or 	r21,r21,r22      /* r21 is the entry to invalidate        */

	isync                    /* mark the entry invalid                */
	ld      r23,0(r21)
	li      r22,-129
	and     r23,r23,r22
	std     r23,0(r21)
	sync

	ld      r23,8(r21)
	rldimi  r23,r20,12,0
	std     r23,8(r21)
	eieio

	ld      r23,0(r21)      /* Get the esid part of the ste         */
	mr      r22,r23
	mfspr	r20,DAR         /* Get the new esid                     */
	rldicl  r20,r20,36,28   /* Permits a full 32b of ESID           */
	rldimi  r23,r20,28,0    /* Insert the new esid value            */
	ori     r23,r23,144     /* Turn on valid and kp                 */
	std     r23,0(r21)      /* Put new entry back into the stab     */

	rldicl  r22,r22,36,28
	rldicr  r22,r22,28,35
	slbie   r22
	sync

3:
	/* All done -- return from exception. */
	mfsprg  r20,3                   /* Load the PACA pointer  */
	ld      r21,PACAEXCSP(r20)      /* Get the exception frame pointer */
	addi    r21,r21,EXC_FRAME_SIZE
	lwz	r23,EX_CCR(r21)		/* get saved CR */
	/* note that this is almost identical to maskable_exception_exit */
	mtcr    r23                     /* restore CR */

	mfmsr	r22
	li	r23, MSR_RI
	andc	r22,r22,r23
	mtmsrd	r22,1

	ld	r22,EX_SRR0(r21)	/* Get SRR0 from exc. frame */
	ld	r23,EX_SRR1(r21)	/* Get SRR1 from exc. frame */
	mtspr	SRR0,r22
	mtspr   SRR1,r23
	ld	r22,EX_R22(r21)		/* restore r22 and r23 */
	ld	r23,EX_R23(r21)
	mfspr	r20,SPRG2
	mfspr	r21,SPRG1
	rfid
_TRACEBACK(do_stab_bolted)

/*
 * r20 points to the PACA, r21 to the exception frame,
 * r23 contains the saved CR.
 * r20 - r23, SRR0 and SRR1 are saved in the exception frame.
 * We assume we aren't going to take any exceptions during this procedure.
 */
_GLOBAL(do_slb_bolted)
	stw     r23,EX_CCR(r21) /* save CR in exc. frame */

	/* (((ea >> 28) & 0x1fff) << 15) | (ea >> 60) */
	mfspr	r21,DAR
	rldicl  r20,r21,36,32   /* Permits a full 32b of ESID */
	rldicr  r20,r20,15,48
	rldicl  r21,r21,4,60
	or      r20,r20,r21

	li      r21,9           /* VSID_RANDOMIZER */
	sldi    r21,r21,32
	oris    r21,r21,58231
	ori     r21,r21,39831

	mulld   r20,r20,r21
	clrldi  r20,r20,28      /* r20 = vsid */

	/* Search the SLB for a free entry */
	li      r22,1
1:
	slbmfee	r23,r22
	rldicl  r23,r23,37,63
	cmpwi   r23,0
	beq     4f              /* Found an invalid entry              */

	addi	r22,r22,1
	cmpldi	r22,64
	blt	1b

	/* No free entry - just take the next entry, round-robin */
	/* XXX we should get the number of SLB entries from the naca */
SLB_NUM_ENTRIES = 64
2:	mfspr	r21,SPRG3
	ld	r22,PACASTABRR(r21)
	addi	r23,r22,1
	cmpdi	r23,SLB_NUM_ENTRIES
	blt	3f
	li	r23,1
3:	std	r23,PACASTABRR(r21)

	/* r20 = vsid, r22 = entry */

	/*
	 * Never cast out the segment for our kernel stack. Since we
	 * dont invalidate the ERAT we could have a valid translation
	 * for the kernel stack during the first part of exception exit
	 * which gets invalidated due to a tlbie from another cpu at a
	 * non recoverable point (after setting srr0/1) - Anton
	 */
	slbmfee	r23,r22
	srdi	r23,r23,28
	/*
	 * This is incorrect (r1 is not the kernel stack) if we entered
	 * from userspace but there is no critical window from userspace
	 * so this should be OK. Also if we cast out the userspace stack
	 * segment while in userspace we will fault it straight back in.
	 */
	srdi	r21,r1,28
	cmpd	r21,r23
	beq-	2b
	
4:
	/* Put together the vsid portion of the entry. */
	li      r21,0
	rldimi  r21,r20,12,0
	ori     r20,r21,1024
#ifndef CONFIG_PPC_ISERIES
	ori	r20,r20,256    /* map kernel region with large ptes */
#endif
	
	/* Invalidate the old entry */
	slbmfee	r21,r22
	lis	r23,-2049
	ori	r23,r23,65535
	and	r21,r21,r23
	slbie	r21

	/* Put together the esid portion of the entry. */
	mfspr	r21,DAR        /* Get the new esid                     */
	rldicl  r21,r21,36,28  /* Permits a full 36b of ESID           */
	li      r23,0
	rldimi  r23,r21,28,0   /* Insert esid  */
	oris    r21,r23,2048   /* valid bit    */
	rldimi  r21,r22,0,52   /* Insert entry */

	isync
	slbmte  r20,r21
	isync

	/* All done -- return from exception. */
	mfsprg  r20,3                   /* Load the PACA pointer  */
	ld      r21,PACAEXCSP(r20)      /* Get the exception frame pointer */
	addi    r21,r21,EXC_FRAME_SIZE
	lwz	r23,EX_CCR(r21)		/* get saved CR */
	/* note that this is almost identical to maskable_exception_exit */
	mtcr    r23                     /* restore CR */

	mfmsr	r22
	li	r23, MSR_RI
	andc	r22,r22,r23
	mtmsrd	r22,1

	ld	r22,EX_SRR0(r21)	/* Get SRR0 from exc. frame */
	ld	r23,EX_SRR1(r21)	/* Get SRR1 from exc. frame */
	mtspr	SRR0,r22
	mtspr   SRR1,r23
	ld	r22,EX_R22(r21)		/* restore r22 and r23 */
	ld	r23,EX_R23(r21)
	mfspr	r20,SPRG2
	mfspr	r21,SPRG1
	rfid
_TRACEBACK(do_slb_bolted)

_GLOBAL(do_stab_SI)
	mflr	r21			/* Save LR in r21 */

	/*
	 * r3 contains the faulting address
	 * r4 contains the required access permissions
	 *
	 * at return r3 = 0 for success
	 */

	bl	.ste_allocate		/* build STE if possible */
	or.	r3,r3,r3		/* Check return code */
	beq     fast_exception_return   /* Return from exception on success */
	mtlr    r21                     /* restore LR */
	blr                             /* Return to DSI or ISI on failure */

/*
 * This code finishes saving the registers to the exception frame.
 * Address translation is already on.
 */
_GLOBAL(save_remaining_regs)
	/*
	 * Save the rest of the registers into the pt_regs structure
	 */
	std     r22,_NIP(r1)
	std     r23,_MSR(r1)
	std     r6,TRAP(r1)
	ld      r6,GPR6(r1)
	SAVE_2GPRS(14, r1)
	SAVE_4GPRS(16, r1)
	SAVE_8GPRS(24, r1)

	/*
	 * Clear the RESULT field
	 */
	li	r22,0
	std	r22,RESULT(r1)

	/*
	 * Test if from user state; result will be tested later
	 */
	andi.	r23,r23,MSR_PR		/* Set CR for later branch */

	/*
	 * Indicate that r1 contains the kernel stack and
	 * get the Kernel TOC and CURRENT pointers from the paca
	 */
	std	r22,PACAKSAVE(r13)	/* r1 is now kernel sp */
	ld	r2,PACATOC(r13)		/* Get Kernel TOC pointer */

	/*
	 * If from user state, update THREAD.regs
	 */
	beq	2f			/* Modify THREAD.regs if from user */
	addi	r24,r1,STACK_FRAME_OVERHEAD
	ld	r22,PACACURRENT(r13)
	std	r24,THREAD+PT_REGS(r22)
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	mfspr	r24,SPRN_VRSAVE		/* if save vrsave register value */
	std	r24,THREAD+THREAD_VRSAVE(r22)
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
2:
	SET_REG_TO_CONST(r22, MSR_KERNEL)

#ifdef DO_SOFT_DISABLE
	stb	r20,PACAPROCENABLED(r13) /* possibly soft enable */
	ori	r22,r22,MSR_EE		/* always hard enable */
#else
	rldimi	r22,r20,15,48		/* Insert desired EE value */
#endif

	mtmsrd  r22
	blr

/*
 * Kernel profiling with soft disable on iSeries
 */
do_profile:
	ld	r22,8(r21)		/* Get SRR1 */
	andi.	r22,r22,MSR_PR		/* Test if in kernel */
	bnelr				/* return if not in kernel */
	ld	r22,0(r21)		/* Get SRR0 */
	ld	r25,PACAPROFSTEXT(r20)	/* _stext */
	subf	r22,r25,r22		/* offset into kernel */
	lwz	r25,PACAPROFSHIFT(r20)
	srd	r22,r22,r25
	lwz	r25,PACAPROFLEN(r20)	/* length of profile table (-1) */
	cmp	0,r22,r25		/* off end? */
	ble	1f
	mr	r22,r25			/* force into last entry */
1:	sldi	r22,r22,2		/* convert to offset into buffer */
	ld	r25,PACAPROFBUFFER(r20)	/* profile buffer */
	add	r25,r25,r22
2:	lwarx	r22,0,r25		/* atomically increment */
	addi	r22,r22,1
	stwcx.	r22,0,r25
	bne-	2b
	blr


/*
 * On pSeries, secondary processors spin in the following code.
 * At entry, r3 = this processor's number (in Linux terms, not hardware).
 */
_GLOBAL(pseries_secondary_smp_init)

	/* turn on 64-bit mode */
	bl	.enable_64b_mode
	isync

	/* Set up a paca value for this processor. */
	LOADADDR(r24, paca) 		 /* Get base vaddr of Paca array  */
	mulli	r13,r3,PACA_SIZE	 /* Calculate vaddr of right Paca */
	add	r13,r13,r24              /* for this processor.           */

	mtspr	SPRG3,r13		 /* Save vaddr of Paca in SPRG3   */
	mr	r24,r3			 /* __secondary_start needs cpu#  */

1:
	HMT_LOW
	lbz	r23,PACAPROCSTART(r13)	 /* Test if this processor should */
					 /* start.                        */
	sync

        /* Create a temp kernel stack for use before relocation is on.    */
	mr	r1,r13
        addi    r1,r1,PACAGUARD
        addi    r1,r1,0x1000
        subi    r1,r1,STACK_FRAME_OVERHEAD

	cmpi	0,r23,0
#ifdef CONFIG_SMP
#ifdef SECONDARY_PROCESSORS
	bne	.__secondary_start
#endif
#endif
	b 	1b			 /* Loop until told to go         */

_GLOBAL(__start_initialization_iSeries)

	LOADADDR(r1,init_task_union)
	addi	r1,r1,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

	LOADADDR(r3,cpu_specs)
	LOADADDR(r4,cur_cpu_spec)
	li	r5,0
	bl	.identify_cpu

	LOADADDR(r2,__toc_start)
	addi	r2,r2,0x4000
	addi	r2,r2,0x4000

	LOADADDR(r9,systemcfg)
	SET_REG_TO_CONST(r4, KERNELBASE+0x5000)
	std	r4,0(r9)		/* set the systemcfg pointer */

	LOADADDR(r9,naca)
	SET_REG_TO_CONST(r4, KERNELBASE+0x4000)
	std	r4,0(r9)		/* set the naca pointer */

#if 1 /* DRENG:PPPBBB:TIA This looks like dead code to me -Peter */
	/* Get the pointer to the segment table */
	ld	r6,PACA(r4)             /* Get the base paca pointer       */
	ld	r4,PACASTABVIRT(r6)
#endif

	bl      .iSeries_fixup_klimit

	b	.start_here_common

_GLOBAL(__start_initialization_pSeries)
	mr	r31,r3			/* save parameters */
	mr	r30,r4
	mr	r29,r5
	mr	r28,r6
	mr	r27,r7
	mr	r26,r8                  /* YABOOT: debug_print() routine */
	mr	r25,r9                  /* YABOOT: debug_delay() routine */
	mr	r24,r10                 /* YABOOT: debug_prom() routine */

	bl	.enable_64b_mode

	/* put a relocation offset into r3 */
	bl	.reloc_offset

	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	/* Relocate the TOC from a virt addr to a real addr */
	sub	r2,r2,r3

	/* setup the systemcfg pointer which is needed by prom_init       */
	LOADADDR(r9,systemcfg)
	sub	r9,r9,r3                /* addr of the variable systemcfg */
	SET_REG_TO_CONST(r4, KERNELBASE+0x5000)
	sub	r4,r4,r3
	std	r4,0(r9)		/* set the value of systemcfg     */

	/* setup the naca pointer which is needed by prom_init            */
	LOADADDR(r9,naca)
	sub	r9,r9,r3                /* addr of the variable naca      */
	SET_REG_TO_CONST(r4, KERNELBASE+0x4000)
	sub	r4,r4,r3
	std	r4,0(r9)		/* set the value of naca          */

	/* DRENG / PPPBBB Fix the following comment!!! -Peter */
	/* The following copies the first 0x100 bytes of code from the    */
	/* load addr to physical addr 0x0.  This code causes secondary    */
	/* processors to spin until a flag in the PACA is set.  This      */
	/* is done at this time rather than with the entire kernel        */
	/* relocation which is done below because we need to cause the    */
	/* processors to spin on code that is not going to move while OF  */
	/* is still alive. Although the spin code is not actually run on  */
	/* a uniprocessor, we always do this copy.                        */
	SET_REG_TO_CONST(r4, KERNELBASE)/* Src addr                       */
	sub	r4,r4,r3  		/* current address of __start     */
			                /*        the source addr         */
	li	r3,0                    /* Dest addr                      */
	li	r5,0x100 		/* # bytes of memory to copy      */
	li	r6,0			/* Destination offset             */
	bl	.copy_and_flush		/* copy the first 0x100 bytes     */

	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27
	mr	r8,r26
	mr	r9,r25
	mr	r10,r24

	bl	.prom_init

	li	r24,0			/* cpu # */

/*
 * At this point, r3 contains the physical address we are running at,
 * returned by prom_init()
 */
_STATIC(__after_prom_start)

/*
 * We need to run with __start at physical address 0.
 * This will leave some code in the first 256B of
 * real memory, which are reserved for software use.
 * The remainder of the first page is loaded with the fixed
 * interrupt vectors.  The next two pages are filled with
 * unknown exception placeholders.
 *
 * Note: This process overwrites the OF exception vectors.
 *       r26 == relocation offset
 *       r27 == KERNELBASE
 */
	bl	.reloc_offset
	mr	r26,r3
	SET_REG_TO_CONST(r27,KERNELBASE)

	li	r3,0                    /* target addr */

	sub	r4,r27,r26 		/* source addr */
					/* current address of _start   */
			                /*   i.e. where we are running */
			                /*        the source addr      */

	LOADADDR(r5,copy_to_here)	/* # bytes of memory to copy      */
	sub	r5,r5,r27

	li	r6,0x100		/* Start offset, the first 0x100  */
					/* bytes were copied earlier.	  */

	bl	.copy_and_flush		/* copy the first n bytes         */
					/* this includes the code being   */
					/* executed here.                 */

        LOADADDR(r0, 4f)                /* Jump to the copy of this code  */
	mtctr	r0			/* that we just made/relocated    */
	bctr

4:	LOADADDR(r5,klimit)
	sub	r5,r5,r26
	ld	r5,0(r5)		/* get the value of klimit */
	sub	r5,r5,r27
	bl	.copy_and_flush		/* copy the rest */
	b	.start_here_pSeries

/*
 * Copy routine used to copy the kernel to start at physical address 0
 * and flush and invalidate the caches as needed.
 * r3 = dest addr, r4 = source addr, r5 = copy limit, r6 = start offset
 * on exit, r3, r4, r5 are unchanged, r6 is updated to be >= r5.
 *
 * Note: this routine *only* clobbers r0, r6 and lr
 */
_STATIC(copy_and_flush)
	addi	r5,r5,-8
	addi	r6,r6,-8
4:	li	r0,16                   /* Use the least common      */
					/* denominator cache line    */
			                /* size.  This results in    */
					/* extra cache line flushes  */
					/* but operation is correct. */
					/* Can't get cache line size */
					/* from NACA as it is being  */
					/* moved too.                */

	mtctr	r0			/* put # words/line in ctr */
3:	addi	r6,r6,8			/* copy a cache line */
	ldx	r0,r6,r4
	stdx	r0,r6,r3
	bdnz	3b
	dcbst	r6,r3			/* write it to memory */
	sync
	icbi	r6,r3			/* flush the icache line */
	cmpld	0,r6,r5
	blt	4b
	sync
	addi	r5,r5,8
	addi	r6,r6,8
	blr

.align 8
copy_to_here:

/*
 * load_up_fpu(unused, unused, tsk)
 * Disable FP for the task which had the FPU previously,
 * and save its floating-point registers in its thread_struct.
 * Enables the FPU for use in the kernel on return.
 * On SMP we know the fpu is free, since we give it up every
 * switch (ie, no lazy save of the FP registers).
 * On entry: r13 == 'current' && last_task_used_math != 'current'
 */
_STATIC(load_up_fpu)
	mfmsr	r5                      /* grab the current MSR */
	ori	r5,r5,MSR_FP
	mtmsrd  r5			/* enable use of fpu now */
	isync
/*
 * For SMP, we don't do lazy FPU switching because it just gets too
 * horrendously complex, especially when a task switches from one CPU
 * to another.  Instead we call giveup_fpu in switch_to.
 *
 */
#ifndef CONFIG_SMP
	LOADBASE(r3,last_task_used_math)
	ld	r4,last_task_used_math@l(r3)
	cmpi	0,r4,0
	beq	1f
	/* Save FP state to last_task_used_math's THREAD struct */
	addi	r4,r4,THREAD
	SAVE_32FPRS(0, r4)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR(r4)
	/* Disable FP for last_task_used_math */
	ld	r5,PT_REGS(r4)
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r20,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r20
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#endif /* CONFIG_SMP */
	/* enable use of FP after return */
	ld	r4,PACACURRENT(r13)
	addi	r5,r4,THREAD		/* Get THREAD */
	ld	r20,THREAD_FPEXC_MODE(r5)
	ori	r23,r23,MSR_FP
	or	r23,r23,r20
	lfd	fr0,THREAD_FPSCR(r5)
	mtfsf	0xff,fr0
	REST_32FPRS(0, r5)
#ifndef CONFIG_SMP
	/* Update last_task_used_math to 'current' */
	std	r4,last_task_used_math@l(r3)
#endif /* CONFIG_SMP */
	/* restore registers and return */
	b	fast_exception_return


/*
 * disable_kernel_fp()
 * Disable the FPU.
 */
_GLOBAL(disable_kernel_fp)
	mfmsr   r3
	rldicl  r0,r3,(63-MSR_FP_LG),1
	rldicl  r3,r0,(MSR_FP_LG+1),0
	mtmsrd  r3			/* disable use of fpu now */
	isync
	blr


/*
 * giveup_fpu(tsk)
 * Disable FP for the task given as the argument,
 * and save the floating-point registers in its thread_struct.
 * Enables the FPU for use in the kernel on return.
 */
_GLOBAL(giveup_fpu)
	mfmsr	r5
	ori	r5,r5,MSR_FP
	mtmsrd	r5			/* enable use of fpu now */
	isync
	cmpi	0,r3,0
	beqlr-				/* if no previous owner, done */
	addi	r3,r3,THREAD		/* want THREAD of task */
	ld	r5,PT_REGS(r3)
	cmpi	0,r5,0
	SAVE_32FPRS(0, r3)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR(r3)
#if 0 /* PPPBBB: enable code below if we run with FE0/1 = 0,0 as default */
	clrrdi	r4,r13,60		/* r4 <- 0xC000000000000000 */
	lfd	fr0,__zero@l(r4)
	mtfsf	0xff,fr0
#endif
	beq	1f
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r3,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r3		/* disable FP for previous task */
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#ifndef CONFIG_SMP
	li	r5,0
	LOADBASE(r4,last_task_used_math)
	std	r5,last_task_used_math@l(r4)
#endif /* CONFIG_SMP */
	blr

#ifdef CONFIG_ALTIVEC
/*
 * load_up_altivec(unused, unused, tsk)
 * Disable Altivec for the task which used altivec upreviously,
 * and save its altivec registers in its thread_struct.
 * Enables Altivec for use in the kernel on return.
 * On SMP we know the fpu is free, since we give it up every
 * switch (ie, no lazy save of the altivec registers).
 * On entry: r13 == 'current' && last_task_used_altivec != 'current'
 */
_STATIC(load_up_altivec)
/*
 * Disable AltiVec for the task which had AltiVec previously,
 * and save its AltiVec registers in its thread_struct.
 * Enables AltiVec for use in the kernel on return.
 * On SMP we know the AltiVec units are free, since we give it up every
 * switch.  -- Kumar
 */
	mfmsr	r5
	oris	r5,r5,MSR_VEC@h
	mtmsrd	r5			/* enable use of AltiVec now */
	isync
/*
 * For SMP, we don't do lazy AltiVec switching because it just gets too
 * horrendously complex, especially when a task switches from one CPU
 * to another.  Instead we call giveup_altivec in switch_to.
 */
#ifndef CONFIG_SMP
	LOADBASE(r3,last_task_used_altivec)
	ld	r4,last_task_used_altivec@l(r3)
	cmpi	0,r4,0
	beq	1f
	addi	r4,r4,THREAD	/* want THREAD of last_task_used_altivec */
	SAVE_32VR(0,r20,r4)
	MFVSCR(vr0)
	li	r20,THREAD_VSCR
	STVX(vr0,r20,r4)
	ld	r5,PT_REGS(r4)
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	lis	r20,MSR_VEC@h
	andc	r4,r4,r20	/* disable altivec for previous task */
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#endif /* CONFIG_SMP */
	/* enable use of AltiVec after return */
	ld	r4,PACACURRENT(r13)
	addi	r5,r4,THREAD
	oris	r23,r23,MSR_VEC@h
	li	r20,THREAD_VSCR
	LVX(vr0,r20,r5)
	MTVSCR(vr0)
	REST_32VR(0,r20,r5)
#ifndef CONFIG_SMP
	/* Update last_task_used_altivec to 'current' */
	std	r4,last_task_used_altivec@l(r3)
#endif /* CONFIG_SMP */
	/* restore registers and return */
	b	fast_exception_return
/*
 * giveup_altivec(tsk)
 * Disable AltiVec for the task given as the argument,
 * and save the AltiVec registers in its thread_struct.
 * Enables AltiVec for use in the kernel on return.
 */
_GLOBAL(giveup_altivec)
	mfmsr	r5
	oris	r5,r5,MSR_VEC@h
	mtmsrd	r5			/* enable use of AltiVec now */
	isync
	cmpi	0,r3,0
	beqlr-				/* if no previous owner, done */
	addi	r3,r3,THREAD		/* want THREAD of task */
	ld	r5,PT_REGS(r3)
	cmpi	0,r5,0
	SAVE_32VR(0, r4, r3)
	MFVSCR(vr0)
	li	r4,THREAD_VSCR
	STVX(vr0, r4, r3)
	beq	1f
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	lis	r3,MSR_VEC@h
	andc	r4,r4,r3		/* disable AltiVec for previous task */
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#ifndef CONFIG_SMP
	li	r5,0
	LOADBASE(r4,last_task_used_altivec)
	std	r5,last_task_used_altivec@l(r4)
#endif /* CONFIG_SMP */
	blr
#endif /* CONFIG_ALTIVEC */



#ifdef CONFIG_SMP
/*
 * This function is called after the master CPU has released the
 * secondary processors.  The execution environment is relocation off.
 * The paca for this processor has the following fields initialized at
 * this point:
 *   1. Processor number
 *   2. Segment table pointer (virtual address)
 * On entry the following are set:
 *   r1    = stack pointer.  vaddr for iSeries, raddr (temp stack) for pSeries
 *   r24   = cpu# (in Linux terms)
 *   r13   = paca virtual address
 *   SPRG3 = paca virtual address
 */
_GLOBAL(__secondary_start)

	HMT_MEDIUM			/* Set thread priority to MEDIUM */

	/* set up the TOC (virtual address) */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	std	r2,PACATOC(r13)
	li	r6,0
	std	r6,PACAKSAVE(r13)
	stb	r6,PACAPROCENABLED(r13)

#ifndef CONFIG_PPC_ISERIES
	/* Initialize the page table pointer register. */
	LOADADDR(r6,_SDR1)
	ld	r6,0(r6)		/* get the value of _SDR1 */
	mtspr	SDR1,r6			/* set the htab location  */
#endif
	/* Initialize the first segment table (or SLB) entry                */
	ld	r3,PACASTABVIRT(r13)    /* get addr of segment table        */
	bl	.stab_initialize

	/* Initialize the kernel stack.  Just a repeat for iSeries.         */
	LOADADDR(r3,current_set)
	sldi	r28,r24,4		/* get current_set[cpu#] */
	ldx	r28,r3,r28
	std	r28,PACACURRENT(r13)
	addi	r1,r28,TASK_UNION_SIZE
	subi	r1,r1,STACK_FRAME_OVERHEAD

	ld	r3,PACASTABREAL(r13)    /* get raddr of segment table       */
	ori	r4,r3,1			/* turn on valid bit                */

#ifdef CONFIG_PPC_ISERIES
	li	r0,-1			/* hypervisor call */
	li	r3,1
	sldi	r3,r3,63		/* 0x8000000000000000 */
	ori	r3,r3,4			/* 0x8000000000000004 */
	sc				/* HvCall_setASR */
#else
	/* set the ASR */
	li	r3,0x5000	/* r3 = ptr to systemcfg  */
	lwz   	r3,PLATFORM(r3) /* r3 = platform flags */
	cmpldi 	r3,PLATFORM_PSERIES_LPAR
	bne   	98f
	mfspr	r3,PVR
	srwi	r3,r3,16
	cmpwi	r3,0x37         /* SStar  */
	beq	97f
	cmpwi	r3,0x36         /* IStar  */
	beq	97f
	cmpwi	r3,0x34         /* Pulsar */
	bne	98f
97:	li	r3,H_SET_ASR    /* hcall = H_SET_ASR */
	HSC     		/* Invoking hcall */
	b	99f
98:                             /* !(rpa hypervisor) || !(sstar) */
	mtasr	r4	        /* set the stab location         */
99:
#endif
	li	r7,0
	mtlr	r7

	/* enable MMU and jump to start_secondary */
	LOADADDR(r3,.start_secondary_prolog)
	SET_REG_TO_CONST(r4, MSR_KERNEL)
#ifdef DO_SOFT_DISABLE
	ori	r4,r4,MSR_EE
#endif
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid

/* 
 * Running with relocation on at this point.  All we want to do is
 * zero the stack back-chain pointer before going into C code.
 */
_GLOBAL(start_secondary_prolog)
	li	r3,0
	std	r3,0(r1)                /* Zero the stack frame pointer     */
	bl	.start_secondary
#endif /* CONFIG_SMP */

/*
 * This subroutine clobbers r11, r12 and the LR
 */
_GLOBAL(enable_64b_mode)
	mfmsr   r11                      /* grab the current MSR */
	li      r12,1
	rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
	or      r11,r11,r12
	li      r12,1
	rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
	or      r11,r11,r12
	mtmsrd  r11
	isync
	blr

/*
 * This subroutine clobbers r11, r12 and the LR
 */
_GLOBAL(enable_32b_mode)
	mfmsr   r11                      /* grab the current MSR */
	li      r12,1
	rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
	andc	r11,r11,r12
	li      r12,1
	rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
	andc	r11,r11,r12
	mtmsrd  r11
	isync
	blr

/*
 * This is where the main kernel code starts.
 */
_STATIC(start_here_pSeries)
	/* get a new offset, now that the kernel has moved. */
	bl	.reloc_offset
	mr	r26,r3

	mfmsr	r6
	ori	r6,r6,MSR_RI
	mtmsrd	r6			/* RI on */

	/* setup the systemcfg pointer which is needed by *tab_initialize  */
	LOADADDR(r6,systemcfg)
	sub	r6,r6,r26                /* addr of the variable systemcfg */
	li	r27,0x5000
	std	r27,0(r6)	 	 /* set the value of systemcfg     */

	/* setup the naca pointer which is needed by *tab_initialize       */
	LOADADDR(r6,naca)
	sub	r6,r6,r26                /* addr of the variable naca      */
	li	r27,0x4000
	std	r27,0(r6)	 	 /* set the value of naca          */

#ifdef CONFIG_HMT
	/* Start up the second thread on cpu 0 */
	mfspr	r3,PVR
	srwi	r3,r3,16
	cmpwi	r3,0x34                 /* Pulsar  */
	beq	90f
	cmpwi	r3,0x36                 /* Icestar */
	beq	90f
	cmpwi	r3,0x37                 /* SStar   */
	beq	90f
	b	91f                     /* HMT not supported */
90:	li      r3,0
	bl	.hmt_start_secondary
91:
#endif

#ifdef CONFIG_SMP
	/* All secondary cpus are now spinning on a common
	 * spinloop, release them all now so they can start
	 * to spin on their individual paca spinloops.
	 * For non SMP kernels, the secondary cpus never
	 * get out of the common spinloop.
	 */
	li	r3,1
	LOADADDR(r5,__secondary_hold_spinloop)
	tophys(r4,r5)
	std     r3,0(r4)
#endif

	/* The following gets the stack and TOC set up with the regs */
	/* pointing to the real addr of the kernel stack.  This is   */
	/* all done to support the C function call below which sets  */
	/* up the htab.  This is done because we have relocated the  */
	/* kernel but are still running in real mode. */

	/* real ptr to current */
	LOADADDR(r3,init_task_union)
	sub	r3,r3,r26

	/* set up a stack pointer (physical address) */
	addi	r1,r3,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

	LOADADDR(r3,cpu_specs)
	sub	r3,r3,r26
	LOADADDR(r4,cur_cpu_spec)
	sub	r4,r4,r26
	mr	r5,r26
	bl	.identify_cpu

	/* set up the TOC (physical address) */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000
	sub	r2,r2,r26

	/* Get the pointer to the segment table which is used by           */
	/* stab_initialize                                                 */
	li	r27,0x4000
	ld	r6,PACA(r27)            /* Get the base paca pointer       */
	sub	r13,r6,r26		/* convert to physical addr         */
	mtspr	SPRG3,r13		/* PPPBBB: Temp... -Peter */
	ld	r3,PACASTABREAL(r13)
	ori	r4,r3,1			/* turn on valid bit                */

	/* set the ASR */
	li	r3,0x5000	/* r3 = ptr to systemcfg */
	lwz   	r3,PLATFORM(r3) /* r3 = platform flags */
	cmpldi 	r3,PLATFORM_PSERIES_LPAR
	bne   	98f
	mfspr	r3,PVR
	srwi	r3,r3,16
	cmpwi	r3,0x37         /* SStar */
	beq	97f
	cmpwi	r3,0x36         /* IStar  */
	beq	97f
	cmpwi	r3,0x34         /* Pulsar */
	bne	98f
97:	li	r3,H_SET_ASR    /* hcall = H_SET_ASR */
	HSC     	        /* Invoking hcall */
	b     	99f
98:                 /* This is not a hypervisor machine */
	mtasr	r4			/* set the stab location            */
99:
	mfspr	r6,SPRG3
	ld	r3,PACASTABREAL(r6)     /* restore r3 for stab_initialize */

	/* Initialize an initial memory mapping and turn on relocation.   */
	bl	.stab_initialize
	bl	.htab_initialize

	li	r3,0x5000	/* r3 = ptr to systemcfg */
	lwz	r3,PLATFORM(r3) /* r3 = platform flags */
	cmpldi r3,PLATFORM_PSERIES
	bne    98f
	LOADADDR(r6,_SDR1)		/* Only if NOT LPAR */
	sub	r6,r6,r26
	ld	r6,0(r6)		/* get the value of _SDR1 */
	mtspr	SDR1,r6			/* set the htab location  */
98: 
	LOADADDR(r3,.start_here_common)
	SET_REG_TO_CONST(r4, MSR_KERNEL)
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid

	/* This is where all platforms converge execution */
_STATIC(start_here_common)
	/* relocation is on at this point */

	/* Clear out the BSS */
	LOADADDR(r11,_end)

	LOADADDR(r8,__bss_start)

	sub	r11,r11,r8        /* bss size                        */
	addi	r11,r11,7         /* round up to an even double word */
	rldicl. r11,r11,61,3      /* shift right by 3                */
	beq	4f
	addi	r8,r8,-8
	li	r0,0
	mtctr	r11		  /* zero this many doublewords      */
3:	stdu	r0,8(r8)
	bdnz	3b
4:

	/* The following code sets up the SP and TOC now that we are */
	/* running with translation enabled. */

	/* ptr to current */
	LOADADDR(r3,init_task_union)

	/* set up the stack */
	addi	r1,r3,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

	/* set up the TOC */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	li	r3,0
	bl	.do_cpu_ftr_fixups

	/* setup the systemcfg pointer                                       */
	LOADADDR(r9,systemcfg)
	SET_REG_TO_CONST(r8, KERNELBASE+0x5000)
	std	r8,0(r9)

	/* setup the naca pointer                                         */
	LOADADDR(r9,naca)
	SET_REG_TO_CONST(r8, KERNELBASE+0x4000)
	std	r8,0(r9)		/* set the value of the naca ptr  */

	LOADADDR(r4,naca)               /* Get naca ptr address           */
	ld	r4,0(r4)                /* Get the location of the naca   */
	ld	r13,PACA(r4)            /* Get the base paca pointer      */
	mtspr	SPRG3,r13

	/* ptr to current */
	LOADADDR(r4,init_task_union)
	std	r4,PACACURRENT(r13)

	std	r2,PACATOC(r13)
	li	r5,0
	std	r0,PACAKSAVE(r13)

	/* ptr to hardware interrupt stack for processor 0                */
	LOADADDR(r3, hardware_int_paca0)
	li      r5,0x1000
	sldi    r5,r5,3
	subi    r5,r5,STACK_FRAME_OVERHEAD

	add     r3,r3,r5
	std     r3,PACAHRDWINTSTACK(r13)

	li      r3,0
	stb     r3,PACAHRDWINTCOUNT(r13)

	/* Restore the parms passed in from the bootloader. */
	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27

	bl	.setup_system

	/* Load up the kernel context */
5:
#ifdef DO_SOFT_DISABLE
	mfspr	r4,SPRG3
	li	r5,0
	stb	r5,PACAPROCENABLED(r4)	/* Soft Disabled */
	mfmsr	r5
	ori	r5,r5,MSR_EE		/* Hard Enabled */
	mtmsrd	r5
#endif

	bl .start_kernel

_GLOBAL(__setup_cpu_power3)
	blr
_GLOBAL(__setup_cpu_power4)
	blr

_GLOBAL(hmt_init)
#ifdef CONFIG_HMT
	LOADADDR(r5, hmt_thread_data)
	mfspr	r7,PVR
	srwi	r7,r7,16
	cmpwi	r7,0x34                 /* Pulsar  */
	beq	90f
	cmpwi	r7,0x36                 /* Icestar */
	beq	91f
	cmpwi	r7,0x37                 /* SStar   */
	beq	91f
	b	101f
90:	mfspr	r6,PIR
	andi.	r6,r6,0x1f
	b	92f
91:	mfspr	r6,PIR
	andi.	r6,r6,0x3ff
92:	sldi	r4,r24,3
	stwx	r6,r5,r4
	bl	.hmt_start_secondary
	b	101f

__hmt_secondary_hold:
	LOADADDR(r5, hmt_thread_data)
	clrldi	r5,r5,4
	li	r7,0
	mfspr	r6,PIR
	mfspr	r8,PVR
	srwi	r8,r8,16
	cmpwi	r8,0x34
	bne	93f
	andi.	r6,r6,0x1f
	b	103f
93:	andi.	r6,r6,0x3f

103:	lwzx	r8,r5,r7
	cmpw	r8,r6
	beq	104f
	addi	r7,r7,8
	b	103b

104:	addi	r7,r7,4
	lwzx	r9,r5,r7
	mr      r24,r9
101:
#endif
	mr      r3,r24
	b       .pseries_secondary_smp_init

#ifdef CONFIG_HMT
_GLOBAL(hmt_start_secondary)
	LOADADDR(r4,__hmt_secondary_hold)
	clrldi	r4,r4,4
	mtspr   NIADORM, r4
	mfspr   r4, MSRDORM
	li      r5, -65
	and     r4, r4, r5
	mtspr   MSRDORM, r4
	lis	r4,0xffef
	ori	r4,r4,0x7403
	mtspr	TSC, r4
	li	r4,0x1f4
	mtspr	TST, r4
	mfspr   r4, HID0
	ori     r4, r4, 0x1
	mtspr   HID0, r4
	mfspr   r4, CTRLF
	oris    r4, r4, 0x40
	mtspr   CTRLT, r4
	blr
#endif

/*
 * We put a few things here that have to be page-aligned.
 * This stuff goes at the beginning of the data segment,
 * which is page-aligned.
 */
	.data
	.align  12
	.globl	sdata
sdata:
	.globl	empty_zero_page
empty_zero_page:
	.space	4096

	.globl	swapper_pg_dir
swapper_pg_dir:
	.space	4096

	.globl	ioremap_dir
ioremap_dir:
	.space	4096

	.globl	bolted_dir
bolted_dir:
	.space	4096

	.globl  hardware_int_paca0
hardware_int_paca0:
	.space	8*4096

/* 1 page segment table per cpu (max 48, cpu0 allocated at STAB0_PHYS_ADDR) */
	.globl	stab_array
stab_array:
        .space	4096 * (48 - 1)
