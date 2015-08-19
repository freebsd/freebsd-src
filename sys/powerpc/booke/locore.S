/*-
 * Copyright (C) 2007-2009 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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

#include "opt_hwpmc_hooks.h"

#include <machine/asm.h>
#include <machine/hid.h>
#include <machine/param.h>
#include <machine/spr.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>

#define TMPSTACKSZ	16384

	.text
	.globl	btext
btext:

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.globl	kernel_text
kernel_text:

/*
 * Startup entry.  Note, this must be the first thing in the text segment!
 */
	.text
	.globl	__start
__start:

/*
 * Assumptions on the boot loader:
 *  - system memory starts from physical address 0
 *  - it's mapped by a single TBL1 entry
 *  - TLB1 mapping is 1:1 pa to va
 *  - kernel is loaded at 16MB boundary
 *  - all PID registers are set to the same value
 *  - CPU is running in AS=0
 *
 * Registers contents provided by the loader(8):
 *	r1	: stack pointer
 *	r3	: metadata pointer
 *
 * We rearrange the TLB1 layout as follows:
 *  - find TLB1 entry we started in
 *  - make sure it's protected, ivalidate other entries
 *  - create temp entry in the second AS (make sure it's not TLB[1])
 *  - switch to temp mapping
 *  - map 16MB of RAM in TLB1[1]
 *  - use AS=1, set EPN to KERNBASE and RPN to kernel load address
 *  - switch to to TLB1[1] mapping
 *  - invalidate temp mapping
 *
 * locore registers use:
 *	r1	: stack pointer
 *	r2	: trace pointer (AP only, for early diagnostics)
 *	r3-r27	: scratch registers
 *	r28	: temp TLB1 entry
 *	r29	: initial TLB1 entry we started in
 *	r30-r31	: arguments (metadata pointer)
 */

/*
 * Keep arguments in r30 & r31 for later use.
 */
	mr	%r30, %r3
	mr	%r31, %r4

/*
 * Initial cleanup
 */
	li	%r3, PSL_DE	/* Keep debug exceptions for CodeWarrior. */
	mtmsr	%r3
	isync

	lis	%r3, HID0_E500_DEFAULT_SET@h
	ori	%r3, %r3, HID0_E500_DEFAULT_SET@l
	mtspr	SPR_HID0, %r3
	isync
	lis	%r3, HID1_E500_DEFAULT_SET@h
	ori	%r3, %r3, HID1_E500_DEFAULT_SET@l
	mtspr	SPR_HID1, %r3
	isync

	/* Invalidate all entries in TLB0 */
	li	%r3, 0
	bl	tlb_inval_all

	cmpwi	%r30, 0
	beq	done_mapping

/*
 * Locate the TLB1 entry that maps this code
 */
	bl	1f
1:	mflr	%r3
	bl	tlb1_find_current	/* the entry found is returned in r29 */

	bl	tlb1_inval_all_but_current

/*
 * Create temporary mapping in AS=1 and switch to it
 */
	addi	%r3, %r29, 1
	bl	tlb1_temp_mapping_as1

	mfmsr	%r3
	ori	%r3, %r3, (PSL_IS | PSL_DS)
	bl	2f
2:	mflr	%r4
	addi	%r4, %r4, 20
	mtspr	SPR_SRR0, %r4
	mtspr	SPR_SRR1, %r3
	rfi				/* Switch context */

/*
 * Invalidate initial entry
 */
	mr	%r3, %r29
	bl	tlb1_inval_entry

/*
 * Setup final mapping in TLB1[1] and switch to it
 */
	/* Final kernel mapping, map in 16 MB of RAM */
	lis	%r3, MAS0_TLBSEL1@h	/* Select TLB1 */
	li	%r4, 0			/* Entry 0 */
	rlwimi	%r3, %r4, 16, 12, 15
	mtspr	SPR_MAS0, %r3
	isync

	li	%r3, (TLB_SIZE_64M << MAS1_TSIZE_SHIFT)@l
	oris	%r3, %r3, (MAS1_VALID | MAS1_IPROT)@h
	mtspr	SPR_MAS1, %r3		/* note TS was not filled, so it's TS=0 */
	isync

	lis	%r3, KERNBASE@h
	ori	%r3, %r3, KERNBASE@l	/* EPN = KERNBASE */
#ifdef SMP
	ori	%r3, %r3, MAS2_M@l	/* WIMGE = 0b00100 */
#endif
	mtspr	SPR_MAS2, %r3
	isync

	/* Discover phys load address */
	bl	3f
3:	mflr	%r4			/* Use current address */
	rlwinm	%r4, %r4, 0, 0, 7	/* 16MB alignment mask */
	ori	%r4, %r4, (MAS3_SX | MAS3_SW | MAS3_SR)@l
	mtspr	SPR_MAS3, %r4		/* Set RPN and protection */
	isync
	tlbwe
	isync
	msync

	/* Switch to the above TLB1[1] mapping */
	bl	4f
4:	mflr	%r4
	rlwinm	%r4, %r4, 0, 8, 31	/* Current offset from kernel load address */
	rlwinm	%r3, %r3, 0, 0, 19
	add	%r4, %r4, %r3		/* Convert to kernel virtual address */
	addi	%r4, %r4, 36
	li	%r3, PSL_DE		/* Note AS=0 */
	mtspr   SPR_SRR0, %r4
	mtspr   SPR_SRR1, %r3
	rfi

/*
 * Invalidate temp mapping
 */
	mr	%r3, %r28
	bl	tlb1_inval_entry

done_mapping:

/*
 * Setup a temporary stack
 */
	bl	1f
	.long tmpstack-.
1:	mflr	%r1
	lwz	%r2,0(%r1)
	add	%r1,%r1,%r2
	addi	%r1, %r1, (TMPSTACKSZ - 16)

/*
 * Relocate kernel
 */
	bl      1f
	.long   _DYNAMIC-.
	.long   _GLOBAL_OFFSET_TABLE_-.
1:	mflr    %r5
	lwz	%r3,0(%r5)	/* _DYNAMIC in %r3 */
	add	%r3,%r3,%r5
	lwz	%r4,4(%r5)	/* GOT pointer */
	add	%r4,%r4,%r5
	lwz	%r4,4(%r4)	/* got[0] is _DYNAMIC link addr */
	subf	%r4,%r4,%r3	/* subtract to calculate relocbase */
	bl	elf_reloc_self

/*
 * Initialise exception vector offsets
 */
	bl	ivor_setup

/*
 * Set up arguments and jump to system initialization code
 */
	mr	%r3, %r30
	mr	%r4, %r31

	/* Prepare core */
	bl	booke_init

	/* Switch to thread0.td_kstack now */
	mr	%r1, %r3
	li	%r3, 0
	stw	%r3, 0(%r1)

	/* Machine independet part, does not return */
	bl	mi_startup
	/* NOT REACHED */
5:	b	5b


#ifdef SMP
/************************************************************************/
/* AP Boot page */
/************************************************************************/
	.text
	.globl	__boot_page
	.align	12
__boot_page:
	bl	1f

	.globl	bp_ntlb1s
bp_ntlb1s:
	.long	0

	.globl	bp_tlb1
bp_tlb1:
	.space	4 * 3 * 16

	.globl	bp_tlb1_end
bp_tlb1_end:

/*
 * Initial configuration
 */
1:	mflr	%r31		/* r31 hold the address of bp_ntlb1s */

	/* Set HIDs */
	lis	%r3, HID0_E500_DEFAULT_SET@h
	ori	%r3, %r3, HID0_E500_DEFAULT_SET@l
	mtspr	SPR_HID0, %r3
	isync
	lis	%r3, HID1_E500_DEFAULT_SET@h
	ori	%r3, %r3, HID1_E500_DEFAULT_SET@l
	mtspr	SPR_HID1, %r3
	isync

	/* Enable branch prediction */
	li	%r3, BUCSR_BPEN
	mtspr	SPR_BUCSR, %r3
	isync

	/* Invalidate all entries in TLB0 */
	li	%r3, 0
	bl	tlb_inval_all

/*
 * Find TLB1 entry which is translating us now
 */
	bl	2f
2:	mflr	%r3
	bl	tlb1_find_current	/* the entry number found is in r29 */

	bl	tlb1_inval_all_but_current

/*
 * Create temporary translation in AS=1 and switch to it
 */
	lwz	%r3, 0(%r31)
	bl	tlb1_temp_mapping_as1

	mfmsr	%r3
	ori	%r3, %r3, (PSL_IS | PSL_DS)
	bl	3f
3:	mflr	%r4
	addi	%r4, %r4, 20
	mtspr	SPR_SRR0, %r4
	mtspr	SPR_SRR1, %r3
	rfi				/* Switch context */

/*
 * Invalidate initial entry
 */
	mr	%r3, %r29
	bl	tlb1_inval_entry

/*
 * Setup final mapping in TLB1[1] and switch to it
 */
	lwz	%r6, 0(%r31)
	addi	%r5, %r31, 4
	li	%r4, 0

4:	lis	%r3, MAS0_TLBSEL1@h
	rlwimi	%r3, %r4, 16, 12, 15
	mtspr	SPR_MAS0, %r3
	isync
	lwz	%r3, 0(%r5)
	mtspr	SPR_MAS1, %r3
	isync
	lwz	%r3, 4(%r5)
	mtspr	SPR_MAS2, %r3
	isync
	lwz	%r3, 8(%r5)
	mtspr	SPR_MAS3, %r3
	isync
	tlbwe
	isync
	msync
	addi	%r5, %r5, 12
	addi	%r4, %r4, 1
	cmpw	%r4, %r6
	blt	4b

	/* Switch to the final mapping */
	bl	5f
	.long __boot_page-.
5:	mflr	%r5
	lwz	%r3,0(%r3)
	add	%r5,%r5,%r3		/* __boot_page in r5 */
	bl	6f
6:	mflr	%r3
	rlwinm	%r3, %r3, 0, 0xfff	/* Offset from boot page start */
	add	%r3, %r3, %r5		/* Make this virtual address */
	addi	%r3, %r3, 32
	li	%r4, 0			/* Note AS=0 */
	mtspr	SPR_SRR0, %r3
	mtspr	SPR_SRR1, %r4
	rfi

/*
 * At this point we're running at virtual addresses KERNBASE and beyond so
 * it's allowed to directly access all locations the kernel was linked
 * against.
 */

/*
 * Invalidate temp mapping
 */
	mr	%r3, %r28
	bl	tlb1_inval_entry

/*
 * Setup a temporary stack
 */
	bl	1f
	.long tmpstack-.
1:	mflr	%r1
	lwz	%r2,0(%r1)
	add	%r1,%r1,%r2
	addi	%r1, %r1, (TMPSTACKSZ - 16)

/*
 * Initialise exception vector offsets
 */
	bl	ivor_setup

	/*
	 * Assign our pcpu instance
	 */
	bl	1f
	.long ap_pcpu-.
1:	mflr	%r4
	lwz	%r3, 0(%r4)
	add	%r3, %r3, %r4
	lwz	%r3, 0(%r3)
	mtsprg0	%r3

	bl	pmap_bootstrap_ap

	bl	cpudep_ap_bootstrap
	/* Switch to the idle thread's kstack */
	mr	%r1, %r3
	
	bl	machdep_ap_bootstrap

	/* NOT REACHED */
6:	b	6b
#endif /* SMP */

/*
 * Invalidate all entries in the given TLB.
 *
 * r3	TLBSEL
 */
tlb_inval_all:
	rlwinm	%r3, %r3, 3, 0x18	/* TLBSEL */
	ori	%r3, %r3, 0x4		/* INVALL */
	tlbivax	0, %r3
	isync
	msync

	tlbsync
	msync
	blr

/*
 * expects address to look up in r3, returns entry number in r29
 *
 * FIXME: the hidden assumption is we are now running in AS=0, but we should
 * retrieve actual AS from MSR[IS|DS] and put it in MAS6[SAS]
 */
tlb1_find_current:
	mfspr	%r17, SPR_PID0
	slwi	%r17, %r17, MAS6_SPID0_SHIFT
	mtspr	SPR_MAS6, %r17
	isync
	tlbsx	0, %r3
	mfspr	%r17, SPR_MAS0
	rlwinm	%r29, %r17, 16, 20, 31		/* MAS0[ESEL] -> r29 */

	/* Make sure we have IPROT set on the entry */
	mfspr	%r17, SPR_MAS1
	oris	%r17, %r17, MAS1_IPROT@h
	mtspr	SPR_MAS1, %r17
	isync
	tlbwe
	isync
	msync
	blr

/*
 * Invalidates a single entry in TLB1.
 *
 * r3		ESEL
 * r4-r5	scratched
 */
tlb1_inval_entry:
	lis	%r4, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r4, %r3, 16, 12, 15	/* Select our entry */
	mtspr	SPR_MAS0, %r4
	isync
	tlbre
	li	%r5, 0			/* MAS1[V] = 0 */
	mtspr	SPR_MAS1, %r5
	isync
	tlbwe
	isync
	msync
	blr

/*
 * r3		entry of temp translation
 * r29		entry of current translation
 * r28		returns temp entry passed in r3
 * r4-r5	scratched
 */
tlb1_temp_mapping_as1:
	mr	%r28, %r3

	/* Read our current translation */
	lis	%r3, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r3, %r29, 16, 12, 15	/* Select our current entry */
	mtspr	SPR_MAS0, %r3
	isync
	tlbre

	/* Prepare and write temp entry */
	lis	%r3, MAS0_TLBSEL1@h	/* Select TLB1 */
	rlwimi	%r3, %r28, 16, 12, 15	/* Select temp entry */
	mtspr	SPR_MAS0, %r3
	isync
	mfspr	%r5, SPR_MAS1
	li	%r4, 1			/* AS=1 */
	rlwimi	%r5, %r4, 12, 19, 19
	li	%r4, 0			/* Global mapping, TID=0 */
	rlwimi	%r5, %r4, 16, 8, 15
	oris	%r5, %r5, (MAS1_VALID | MAS1_IPROT)@h
	mtspr	SPR_MAS1, %r5
	isync
	tlbwe
	isync
	msync
	blr

/*
 * Loops over TLB1, invalidates all entries skipping the one which currently
 * maps this code.
 *
 * r29		current entry
 * r3-r5	scratched
 */
tlb1_inval_all_but_current:
	mr	%r6, %r3
	mfspr	%r3, SPR_TLB1CFG	/* Get number of entries */
	andi.	%r3, %r3, TLBCFG_NENTRY_MASK@l
	li	%r4, 0			/* Start from Entry 0 */
1:	lis	%r5, MAS0_TLBSEL1@h
	rlwimi	%r5, %r4, 16, 12, 15
	mtspr	SPR_MAS0, %r5
	isync
	tlbre
	mfspr	%r5, SPR_MAS1
	cmpw	%r4, %r29		/* our current entry? */
	beq	2f
	rlwinm	%r5, %r5, 0, 2, 31	/* clear VALID and IPROT bits */
	mtspr	SPR_MAS1, %r5
	isync
	tlbwe
	isync
	msync
2:	addi	%r4, %r4, 1
	cmpw	%r4, %r3		/* Check if this is the last entry */
	bne	1b
	blr

#ifdef SMP
__boot_page_padding:
	/*
	 * Boot page needs to be exactly 4K, with the last word of this page
	 * acting as the reset vector, so we need to stuff the remainder.
	 * Upon release from holdoff CPU fetches the last word of the boot
	 * page.
	 */
	.space	4092 - (__boot_page_padding - __boot_page)
	b	__boot_page
#endif /* SMP */

/************************************************************************/
/* locore subroutines */
/************************************************************************/

/*
 * void tid_flush(tlbtid_t tid);
 *
 * Invalidate all TLB0 entries which match the given TID. Note this is
 * dedicated for cases when invalidation(s) should NOT be propagated to other
 * CPUs.
 *
 * void tid_flush(tlbtid_t tid, int tlb0_ways, int tlb0_entries_per_way);
 *
 * XXX: why isn't this in C?
 */
ENTRY(tid_flush)
	cmpwi	%r3, TID_KERNEL
	beq	tid_flush_end	/* don't evict kernel translations */

	/* Disable interrupts */
	mfmsr	%r10
	wrteei	0

	li	%r6, 0		/* ways counter */
loop_ways:
	li	%r7, 0		/* entries [per way] counter */
loop_entries:
	/* Select TLB0 and ESEL (way) */
	lis	%r8, MAS0_TLBSEL0@h
	rlwimi	%r8, %r6, 16, 14, 15
	mtspr	SPR_MAS0, %r8
	isync

	/* Select EPN (entry within the way) */
	rlwinm	%r8, %r7, 12, 13, 19
	mtspr	SPR_MAS2, %r8
	isync
	tlbre

	/* Check if valid entry */
	mfspr	%r8, SPR_MAS1
	andis.	%r9, %r8, MAS1_VALID@h
	beq	next_entry	/* invalid entry */

	/* Check if this is our TID */
	rlwinm	%r9, %r8, 16, 24, 31

	cmplw	%r9, %r3
	bne	next_entry	/* not our TID */

	/* Clear VALID bit */
	rlwinm	%r8, %r8, 0, 1, 31
	mtspr	SPR_MAS1, %r8
	isync
	tlbwe
	isync
	msync

next_entry:
	addi	%r7, %r7, 1
	cmpw	%r7, %r5
	bne	loop_entries

	/* Next way */
	addi	%r6, %r6, 1
	cmpw	%r6, %r4
	bne	loop_ways

	/* Restore MSR (possibly re-enable interrupts) */
	mtmsr	%r10
	isync

tid_flush_end:
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
1:	mfspr	%r3, SPR_L1CSR0
	andi.	%r3, %r3, L1CSR0_DCFI
	bne	1b
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
1:	mfspr	%r3, SPR_L1CSR1
	andi.	%r3, %r3, L1CSR1_ICFI
	bne	1b
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
	lwz	%r4, TD_PCB(%r2)
	stw	%r3, PCB_ONFAULT(%r4)
	mfcr	%r4
	stw	%r0, 0(%r3)
	stw	%r1, 4(%r3)
	stw	%r2, 8(%r3)
	stw	%r4, 8(%r3)
	stmw	%r13, 16(%r3)		/* store CR, CTR, XER, [r13 .. r31] */
	li	%r3, 0			/* return FALSE */
	blr

/************************************************************************/
/* Data section								*/
/************************************************************************/
	.data
	.align 3
GLOBAL(__startkernel)
	.long   begin
GLOBAL(__endkernel)
	.long   end
	.align	4
tmpstack:
	.space	TMPSTACKSZ
tmpstackbound:
	.space 10240	/* XXX: this really should not be necessary */

/*
 * Compiled KERNBASE locations
 */
	.globl	kernbase
	.set	kernbase, KERNBASE

#include <powerpc/booke/trap_subr.S>
