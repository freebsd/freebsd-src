/*-
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "assym.s"

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/spr.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>
#include <machine/bootinfo.h>

#define TMPSTACKSZ	16384

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.text
	.globl	kernel_text
kernel_text:

/*
 * Startup entry.  Note, this must be the first thing in the text segment!
 */
	.text
	.globl	__start
__start:

/*
 * Assumption on a boot loader:
 *  - system memory starts from physical address 0
 *  - kernel is loaded at 16MB boundary
 *  - it's mapped by a single TBL1 entry
 *  - TLB1 mapping is 1:1 pa to va
 *  - all PID registers are set to the same value
 *
 * Loader register use:
 *	r1	: stack pointer
 *	r3	: metadata pointer
 *
 * We rearrange the TLB1 layout as follows:
 *  - find AS and entry kernel started in
 *  - make sure it's protected, ivalidate other entries
 *  - create temp entry in the second AS (make sure it's not TLB[15])
 *  - switch to temp mapping
 *  - map 16MB of RAM in TLB1[15]
 *  - use AS=1, set EPN to KERNBASE and RPN to kernel load address
 *  - switch to to TLB1[15] mapping
 *  - invalidate temp mapping
 *
 * locore register use:
 *	r1	: stack pointer
 *	r2	: unused
 *	r3	: kernel_text
 *	r4	: _end
 *	r5	: metadata pointer
 *	r6-r9	: unused
 *	r10	: entry we started in
 *	r11	: temp entry
 *	r12	: AS we started in
 *	r13-r31 : auxiliary registers
 */

/*
 * Move metadata ptr to r5
 */
	mr	%r5, %r3

/*
 * Initial cleanup
 */
	li	%r16, 0x200		/* Keep debug exceptions for CodeWarrior. */
	mtmsr	%r16
	isync
#if 0
	mtspr	SPR_HID0, %r16
	isync
	msync
	mtspr	SPR_HID1, %r16
	isync
#endif

	/* Issue INV_ALL Invalidate on TLB0 */
	li      %r16, 0x04
	tlbivax	0, %r16
	isync
	msync

/*
 * Use tblsx to locate the TLB1 entry that maps kernel code
 */
	bl	1f			/* Current address */
1:	mflr	%r15

	/* Find entry that maps current address */
	mfspr	%r17, SPR_PID0
	slwi	%r17, %r17, MAS6_SPID0_SHIFT
	mtspr	SPR_MAS6, %r17
	isync
	tlbsx	0, %r15

	/* Copy entry number to r10 */
	mfspr	%r17, SPR_MAS0
	rlwinm	%r10, %r17, 16, 28, 31

	/* Invalidate TLB1, skipping our entry. */
	mfspr	%r17, SPR_TLB1CFG	/* Get number of entries */
	andi.	%r17, %r17, TLBCFG_NENTRY_MASK@l
	li	%r16, 0			/* Start from Entry 0 */

2:	lis	%r15, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r15, %r16, 16, 12, 15
	mtspr	SPR_MAS0, %r15
	isync
	tlbre
	mfspr	%r15, SPR_MAS1
	cmpw	%r16, %r10
	beq	3f
	/* Clear VALID and IPROT bits for other entries */
	rlwinm	%r15, %r15, 0, 2, 31
	mtspr	SPR_MAS1, %r15
	isync
	tlbwe
	isync
	msync
3:	addi	%r16, %r16, 1
	cmpw	%r16, %r17		/* Check if this is the last entry */
	bne	2b

/*
 * Create temporary mapping in the other Address Space
 */
	lis	%r17, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r17, %r10, 16, 12, 15	/* Select our entry */
	mtspr	SPR_MAS0, %r17
	isync
	tlbre				/* Read it in */

	/* Prepare and write temp entry */
	lis	%r17, MAS0_TLBSEL1@h	/* Select TLB1 */
	addi	%r11, %r10, 0x1		/* Use next entry. */
	rlwimi	%r17, %r11, 16, 12, 15	/* Select temp entry */
	mtspr	SPR_MAS0, %r17
	isync

	mfspr	%r16, SPR_MAS1
	li	%r15, 1			/* AS 1 */
	rlwimi	%r16, %r15, 12, 19, 19
	mtspr	SPR_MAS1, %r16
	li	%r17, 0
	rlwimi	%r16, %r17, 0, 8, 15	/* Global mapping, TID=0 */
	isync

	tlbwe
	isync
	msync

	mfmsr	%r16
	ori	%r16, %r16, 0x30	/* Switch to AS 1. */

	bl	4f			/* Find current execution address */
4:	mflr	%r15
	addi	%r15, %r15, 20		/* Increment to instruction after rfi */
	mtspr	SPR_SRR0, %r15
	mtspr	SPR_SRR1, %r16
	rfi				/* Switch context */

/*
 * Invalidate initial entry
 */
	mr	%r22, %r10
	bl	tlb1_inval_entry

/*
 * Setup final mapping in TLB1[1] and switch to it
 */
	/* Final kernel mapping, map in 16 MB of RAM */
	lis	%r16, MAS0_TLBSEL1@h	/* Select TLB1 */
	li	%r17, 1			/* Entry 1 */
	rlwimi	%r16, %r17, 16, 12, 15
	mtspr	SPR_MAS0, %r16
	isync

	li	%r16, (TLB_SIZE_16M << MAS1_TSIZE_SHIFT)@l
	oris	%r16, %r16, (MAS1_VALID | MAS1_IPROT)@h
	mtspr	SPR_MAS1, %r16
	isync

	lis	%r19, KERNBASE@h
	ori	%r19, %r19, KERNBASE@l
	mtspr	SPR_MAS2, %r19		/* Set final EPN, clear WIMG */
	isync

	bl	5f
5:	mflr	%r16			/* Use current address */
	lis	%r18, 0xff00		/* 16MB alignment mask */
	and	%r16, %r16, %r18
	mr	%r25, %r16		/* Copy kernel load address */
	ori	%r16, %r16, (MAS3_SX | MAS3_SW | MAS3_SR)@l
	mtspr	SPR_MAS3, %r16		/* Set RPN and protection */
	isync
	tlbwe
	isync
	msync

	/* Switch to the above TLB1[1] mapping */
	lis	%r18, 0x00ff		/* 16MB offset mask */
	ori	%r18, %r18, 0xffff
	bl	6f
6:	mflr	%r20			/* Use current address */
	and	%r20, %r20, %r18	/* Offset from kernel load address */
	add	%r20, %r20, %r19	/* Move to kernel virtual address */
	addi	%r20, %r20, 32		/* Increment to instr. after rfi  */
	li	%r21, 0x200
	mtspr   SPR_SRR0, %r20
	mtspr   SPR_SRR1, %r21
	rfi

	/* Save kernel load address for later use */
	lis	%r24, kernload@ha
	addi	%r24, %r24, kernload@l
	stw	%r25, 0(%r24)

/*
 * Invalidate temp mapping
 */
	mr	%r22, %r11
	bl	tlb1_inval_entry

/*
 * Setup a temporary stack
 */
	lis	%r1, tmpstack@ha
	addi	%r1, %r1, tmpstack@l
	addi	%r1, %r1, (TMPSTACKSZ - 8)

/*
 * Intialise exception vector offsets
 */
	bl	ivor_setup

/*
 * Jump to system initialization code
 *
 * Setup first two arguments for e500_init, metadata (r5) is already in place.
 */
	lis	%r3, kernel_text@ha
	addi	%r3, %r3, kernel_text@l
	lis	%r4, _end@ha
	addi	%r4, %r4, _end@l

	bl	e500_init

	/* Switch to thread0.td_kstack */
	mr	%r1, %r3
	li	%r3, 0
	stw	%r3, 0(%r1)

	bl	mi_startup  /* Machine independet part, does not return */

/************************************************************************/
/* locore subroutines */
/************************************************************************/

tlb1_inval_entry:
	lis	%r17, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r17, %r22, 16, 12, 15	/* Select our entry */
	mtspr	SPR_MAS0, %r17
	isync
	tlbre				/* Read it in */

	li	%r16, 0
	mtspr	SPR_MAS1, %r16
	isync
	tlbwe
	isync
	msync
	blr

ivor_setup:
	/* Set base address of interrupt handler routines */
	lis	%r21, interrupt_vector_base@h
	mtspr	SPR_IVPR, %r21

	/* Assign interrupt handler routines offsets */
	li	%r21, int_critical_input@l
	mtspr	SPR_IVOR0, %r21
	li	%r21, int_machine_check@l
	mtspr	SPR_IVOR1, %r21
	li	%r21, int_data_storage@l
	mtspr	SPR_IVOR2, %r21
	li	%r21, int_instr_storage@l
	mtspr	SPR_IVOR3, %r21
	li	%r21, int_external_input@l
	mtspr	SPR_IVOR4, %r21
	li	%r21, int_alignment@l
	mtspr	SPR_IVOR5, %r21
	li	%r21, int_program@l
	mtspr	SPR_IVOR6, %r21
	li	%r21, int_syscall@l
	mtspr	SPR_IVOR8, %r21
	li	%r21, int_decrementer@l
	mtspr	SPR_IVOR10, %r21
	li	%r21, int_fixed_interval_timer@l
	mtspr	SPR_IVOR11, %r21
	li	%r21, int_watchdog@l
	mtspr	SPR_IVOR12, %r21
	li	%r21, int_data_tlb_error@l
	mtspr	SPR_IVOR13, %r21
	li	%r21, int_inst_tlb_error@l
	mtspr	SPR_IVOR14, %r21
	li	%r21, int_debug@l
	mtspr	SPR_IVOR15, %r21
	blr

/*
 * void tlb1_inval_va(vm_offset_t va)
 *
 * r3 - va to invalidate
 */
ENTRY(tlb1_inval_va)
	/* EA mask */
	lis	%r6, 0xffff
	ori	%r6, %r6, 0xf000
	and	%r3, %r3, %r6

	/* Select TLB1 */
	ori	%r3, %r3, 0x08

	isync
	tlbivax 0, %r3
	isync
	msync
	blr

/*
 * void tlb0_inval_va(vm_offset_t va)
 *
 * r3 - va to invalidate
 */
ENTRY(tlb0_inval_va)
	/* EA mask, this also clears TLBSEL, selecting TLB0 */
	lis	%r6, 0xffff
	ori	%r6, %r6, 0xf000
	and	%r3, %r3, %r6

	isync
	tlbivax 0, %r3
	isync
	msync
	blr

/*
 * Cache disable/enable/inval sequences according
 * to section 2.16 of E500CORE RM.
 */
ENTRY(dcache_inval)
	/* Invalidate d-cache */
	mfspr	%r3, SPR_L1CSR0
	ori	%r3, %r3, (L1CSR0_DCFI | L1CSR0_DCLFR)@l
	msync
	isync
	mtspr	SPR_L1CSR0, %r3
	isync
	blr

ENTRY(dcache_disable)
	/* Disable d-cache */
	mfspr	%r3, SPR_L1CSR0
	li	%r4, L1CSR0_DCE@l
	not	%r4, %r4
	and	%r3, %r3, %r4
	msync
	isync
	mtspr	SPR_L1CSR0, %r3
	isync
	blr

ENTRY(dcache_enable)
	/* Enable d-cache */
	mfspr	%r3, SPR_L1CSR0
	oris	%r3, %r3, (L1CSR0_DCPE | L1CSR0_DCE)@h
	ori	%r3, %r3, (L1CSR0_DCPE | L1CSR0_DCE)@l
	msync
	isync
	mtspr	SPR_L1CSR0, %r3
	isync
	blr

ENTRY(icache_inval)
	/* Invalidate i-cache */
	mfspr	%r3, SPR_L1CSR1
	ori	%r3, %r3, (L1CSR1_ICFI | L1CSR1_ICLFR)@l
	isync
	mtspr	SPR_L1CSR1, %r3
	isync
	blr

ENTRY(icache_disable)
	/* Disable i-cache */
	mfspr	%r3, SPR_L1CSR1
	li	%r4, L1CSR1_ICE@l
	not	%r4, %r4
	and	%r3, %r3, %r4
	isync
	mtspr	SPR_L1CSR1, %r3
	isync
	blr

ENTRY(icache_enable)
	/* Enable i-cache */
	mfspr	%r3, SPR_L1CSR1
	oris	%r3, %r3, (L1CSR1_ICPE | L1CSR1_ICE)@h
	ori	%r3, %r3, (L1CSR1_ICPE | L1CSR1_ICE)@l
	isync
	mtspr	SPR_L1CSR1, %r3
	isync
	blr

/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
	.globl	setfault
setfault:
	mflr	%r0
	mfsprg0	%r4
	lwz	%r4, PC_CURTHREAD(%r4)
	lwz	%r4, TD_PCB(%r4)
	stw	%r3, PCB_ONFAULT(%r4)
	mfcr	%r10
	mfctr	%r11
	mfxer	%r12
	stw	%r0, 0(%r3)
	stw	%r1, 4(%r3)
	stw	%r2, 8(%r3)
	stmw	%r10, 12(%r3)		/* store CR, CTR, XER, [r13 .. r31] */
	li	%r3, 0			/* return FALSE */
	blr

/************************************************************************/
/* Data section								*/
/************************************************************************/
	.data
	.align	4
tmpstack:
	.space	TMPSTACKSZ

/*
 * Compiled KERNBASE locations
 */
	.globl	kernbase
	.set	kernbase, KERNBASE

/*
 * Globals
 */
#define	INTRCNT_COUNT	256		/* max(HROWPIC_IRQMAX,OPENPIC_IRQMAX) */

GLOBAL(kernload)
	.long
GLOBAL(intrnames)
	.space	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2
GLOBAL(eintrnames)
	.align 4
GLOBAL(intrcnt)
	.space	INTRCNT_COUNT * 4 * 2
GLOBAL(eintrcnt)

#include <powerpc/booke/trap_subr.S>
