/*
 *  linux/arch/arm/mm/proc-arm720.S: MMU functions for ARM720
 *
 *  Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *                     Rob Scott (rscott@mtrob.fdns.net)
 *  Copyright (C) 2000 ARM Limited, Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * These are the low level assembler for performing cache and TLB
 * functions on the ARM720T.  The ARM720T has a writethrough IDC
 * cache, so we don't need to clean it.
 *
 *  Changelog:
 *   05-09-2000 SJH	Created by moving 720 specific functions
 *			out of 'proc-arm6,7.S' per RMK discussion
 *   07-25-2000 SJH	Added idle function.
 *   08-25-2000	DBS	Updated for integration of ARM Ltd version.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

/*
 * and the page size
 */
#define PAGESIZE	4096

/*
 * Function: arm720_cache_clean_invalidate_all (void)
 *	   : arm720_cache_clean_invalidate_page (unsigned long address, int size,
 *                                    int flags)
 *
 * Params  : address	Area start address
 *	   : size	size of area
 *	   : flags	b0 = I cache as well
 *
 * Purpose : Flush all cache lines
 */
ENTRY(cpu_arm720_cache_clean_invalidate_all)
ENTRY(cpu_arm720_cache_clean_invalidate_range)
ENTRY(cpu_arm720_icache_invalidate_range)
ENTRY(cpu_arm720_icache_invalidate_page)
ENTRY(cpu_arm720_dcache_invalidate_range)
		mov	r0, #0
		mcr	p15, 0, r0, c7, c7, 0		@ flush cache
		mov	pc, lr

/*
 * These just expect cache lines to be cleaned.  Since we have a writethrough
 * cache, we never have any dirty cachelines to worry about.
 */
ENTRY(cpu_arm720_dcache_clean_range)
ENTRY(cpu_arm720_dcache_clean_page)
ENTRY(cpu_arm720_dcache_clean_entry)
ENTRY(cpu_arm720_flush_ram_page)
		mov	pc, lr

/*
 * Function: arm720_tlb_invalidate_all (void)
 *
 * Purpose : flush all TLB entries in all caches
 */
ENTRY(cpu_arm720_tlb_invalidate_all)
		mov	r0, #0
		mcr	p15, 0, r0, c8, c7, 0		@ flush TLB (v4)
		mov	pc, lr

/*
 * Function: arm720_tlb_invalidate_page (unsigned long address, int end, int flags)
 *
 * Params  : address	Area start address
 *	   : end	Area end address
 *	   : flags	b0 = I cache as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_arm720_tlb_invalidate_range)
		sub	r3, r1, r0
		cmp	r3, #256 * PAGESIZE		@ arbitary, should be tuned
		bhi	cpu_arm720_tlb_invalidate_all
1:		mcr	p15, 0, r0, c8, c7, 1		@ flush TLB (v4)
		add	r0, r0, #PAGESIZE
		cmp	r0, r1
		blt	1b
		mov	pc, lr

/*
 * Function: arm720_tlb_invalidate_page (unsigned long address, int flags)
 *
 * Params  : address	Address
 *	   : flags	b0 = I-TLB as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_arm720_tlb_invalidate_page)
		mcr	p15, 0, r0, c8, c7, 1		@ flush TLB (v4)
		mov	pc, lr

/*
 * Function: arm720_data_abort ()
 *
 * Params  : r0 = address of aborted instruction
 *         : r3 = saved SPSR
 *
 * Purpose : obtain information about current aborted instruction
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it.
 *
 * Returns : r0 = address of abort
 *	   : r1 != 0 if writing
 *	   : r3 = FSR
 *	   : sp = pointer to registers
 */

Ldata_ldmstm:	tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
		mov	r7, #0x11
		orr	r7, r7, r7, lsl #8
		and	r0, r4, r7
		and	r2, r4, r7, lsl #1
		add	r0, r0, r2, lsr #1
		and	r2, r4, r7, lsl #2
		add	r0, r0, r2, lsr #2
		and	r2, r4, r7, lsl #3
		add	r0, r0, r2, lsr #3
		add	r0, r0, r0, lsr #8
		add	r0, r0, r0, lsr #4
		and	r7, r0, #15			@ r7 = no. of registers to transfer.
		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]		@ Get register
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r7, lsl #2
		addeq	r7, r0, r7, lsl #2		@ Do correction (signed)
Ldata_saver7:	str	r7, [sp, r5, lsr #14]		@ Put register
Ldata_simple:	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
		mrc	p15, 0, r3, c5, c0, 0		@ get FSR
		and	r3, r3, #255
		mov	pc, lr

ENTRY(cpu_arm720_data_abort)
		tst	r3, #T_BIT
		bne	.data_thumb_abort
		ldr	r4, [r0]			@ read instruction causing problem
		tst	r4, r4, lsr #21			@ C = bit 20
		sbc	r1, r1, r1			@ r1 = C - 1
		and	r2, r4, #15 << 24
		add	pc, pc, r2, lsr #22		@ Now branch to the relevent processing routine
		movs	pc, lr

		b	Ldata_lateldrhpost		@ ldrh	rd, [rn], #m/rm
		b	Ldata_lateldrhpre		@ ldrh	rd, [rn, #m/rm]
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_lateldrpostconst		@ ldr	rd, [rn], #m
		b	Ldata_lateldrpreconst		@ ldr	rd, [rn, #m] 
		b	Ldata_lateldrpostreg		@ ldr	rd, [rn], rm
		b	Ldata_lateldrprereg		@ ldr	rd, [rn, rm]
		b	Ldata_ldmstm			@ ldm*a	rn, <rlist>
		b	Ldata_ldmstm			@ ldm*b	rn, <rlist>
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_simple			@ ldc	rd, [rn], #m	@ Same as ldr	rd, [rn], #m
		b	Ldata_simple			@ ldc	rd, [rn, #m]
		b	Ldata_unknown

Ldata_unknown:	@ Part of jumptable
		mov	r0, r2
		mov	r1, r4
		mov	r2, sp
		bl	baddataabort
		b	ret_from_exception

Ldata_lateldrhpre:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrhpost:
		and	r5, r4, #0x00f			@ get Rm / low nibble of immediate value
		tst	r4, #1 << 22			@ if (immediate offset)
		andne	r2, r4, #0xf00			@ { immediate high nibble
		orrne	r2, r5, r2, lsr #4		@   combine nibbles } else
		ldreq	r2, [sp, r5, lsl #2]		@ { load Rm value }
		and	r5, r4, #15 << 16		@ get Rn
		ldr	r0, [sp, r5, lsr #14]		@ load Rn value
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2
		addeq	r7, r0, r2
		b	Ldata_saver7

Ldata_lateldrpreconst:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrpostconst:
		movs	r2, r4, lsl #20			@ Get offset
		beq	Ldata_simple
		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2, lsr #20
		addeq	r7, r0, r2, lsr #20
		b	Ldata_saver7

Ldata_lateldrprereg:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrpostreg:
		and	r5, r4, #15
		ldr	r2, [sp, r5, lsl #2]		@ Get Rm
		mov	r3, r4, lsr #7
		ands	r3, r3, #31
		and	r6, r4, #0x70
		orreq	r6, r6, #8
		add	pc, pc, r6
		mov	r0, r0

		mov	r2, r2, lsl r3			@ 0: LSL #!0
		b	1f
		b	1f				@ 1: LSL #0
		mov	r0, r0
		b	1f				@ 2: MUL?
		mov	r0, r0
		b	1f				@ 3: MUL?
		mov	r0, r0
		mov	r2, r2, lsr r3			@ 4: LSR #!0
		b	1f
		mov	r2, r2, lsr #32			@ 5: LSR #32
		b	1f
		b	1f				@ 6: MUL?
		mov	r0, r0
		b	1f				@ 7: MUL?
		mov	r0, r0
		mov	r2, r2, asr r3			@ 8: ASR #!0
		b	1f
		mov	r2, r2, asr #32			@ 9: ASR #32
		b	1f
		b	1f				@ A: MUL?
		mov	r0, r0
		b	1f				@ B: MUL?
		mov	r0, r0
		mov	r2, r2, ror r3			@ C: ROR #!0
		b	1f
		mov	r2, r2, rrx			@ D: RRX
		b	1f
		mov	r0, r0				@ E: MUL?
		mov	r0, r0
		mov	r0, r0				@ F: MUL?


1:		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2
		addeq	r7, r0, r2
		b	Ldata_saver7

.data_thumb_abort:
		ldrh	r4, [r0]			@ read instruction
		tst	r4, r4, lsr #12			@ C = bit 11
		sbc	r1, r1, r1			@ r1 = C - 1
		and	r2, r4, #15 << 12
		add	pc, pc, r2, lsr #10		@ lookup in table
		nop

/* 0 */		b	Ldata_unknown
/* 1 */		b	Ldata_unknown
/* 2 */		b	Ldata_unknown
/* 3 */		b	Ldata_unknown
/* 4 */		b	Ldata_unknown
/* 5 */		b	.data_thumb_reg
/* 6 */		b	Ldata_simple
/* 7 */		b	Ldata_simple
/* 8 */		b	Ldata_simple
/* 9 */		b	Ldata_simple
/* A */		b	Ldata_unknown
/* B */		b	.data_thumb_pushpop
/* C */		b	.data_thumb_ldmstm
/* D */		b	Ldata_unknown
/* E */		b	Ldata_unknown
/* F */		b	Ldata_unknown

.data_thumb_reg:
		tst	r4, #1 << 9
		beq	Ldata_simple
		tst	r4, #1 << 10		@ If 'S' (signed) bit is set
		movne	r1, #0			@ it must be a load instr
		b	Ldata_simple

.data_thumb_pushpop:
		tst	r4, #1 << 10
		beq	Ldata_unknown
		mov	r7, #0x11
		and	r0, r4, r7
		and	r2, r4, r7, lsl #1
		add	r0, r0, r2, lsr #1
		and	r2, r4, r7, lsl #2
		add	r0, r0, r2, lsr #2
		and	r2, r4, r7, lsl #3
		add	r0, r0, r2, lsr #3
		add	r0, r0, r0, lsr #4
		and	r2, r4, #0x0100			@ catch 'R' bit for push/pop
		add	r0, r0, r2, lsr #8
		and	r0, r0, #15			@ number of regs to transfer
		ldr	r7, [sp, #13 << 2]
		tst	r4, #1 << 11
		addne	r7, r7, r0, lsl #2		@ increment SP if PUSH
		subeq	r7, r7, r0, lsr #2		@ decrement SP if POP
		str	r7, [sp, #13 << 2]
		b	Ldata_simple

.data_thumb_ldmstm:
		mov	r7, #0x11
		and	r0, r4, r7
		and	r2, r4, r7, lsl #1
		add	r0, r0, r2, lsr #1
		and	r2, r4, r7, lsl #2
		add	r0, r0, r2, lsr #2
		and	r2, r4, r7, lsl #3
		add	r0, r0, r2, lsr #3
		add	r0, r0, r0, lsr #4
		and	r0, r0, #15			@ number of regs to transfer
		and	r5, r4, #7 << 8
		ldr	r7, [sp, r5, lsr #6]
		sub	r7, r7, r0, lsr #2		@ always decrement
		str	r7, [sp, r5, lsr #6]
		b	Ldata_simple

/*
 * Function: arm720_check_bugs (void)
 *	   : arm720_proc_init (void)
 *	   : arm720_proc_fin (void)
 *
 * Notes   : This processor does not require these
 */
ENTRY(cpu_arm720_check_bugs)
		mrs	ip, cpsr
		bic	ip, ip, #F_BIT
		msr	cpsr, ip
		mov	pc, lr

ENTRY(cpu_arm720_proc_init)
		mov	pc, lr

ENTRY(cpu_arm720_proc_fin)
		stmfd	sp!, {lr}
		mov	ip, #F_BIT | I_BIT | SVC_MODE
		msr	cpsr_c, ip
		mrc	p15, 0, r0, c1, c0, 0
		bic	r0, r0, #0x1000			@ ...i............
		bic	r0, r0, #0x000e			@ ............wca.
		mcr	p15, 0, r0, c1, c0, 0		@ disable caches
		mcr	p15, 0, r1, c7, c7, 0		@ invalidate cache
		ldmfd	sp!, {pc}

/*
 * Function: arm720_proc_do_idle(void)
 * Params  : r0 = unused
 * Purpose : put the processer in proper idle mode
 */
ENTRY(cpu_arm720_do_idle)
		mov	pc, lr

/*
 * Function: arm720_set_pgd(unsigned long pgd_phys)
 * Params  : pgd_phys	Physical address of page table
 * Purpose : Perform a task switch, saving the old process' state and restoring
 *	     the new.
 */
ENTRY(cpu_arm720_set_pgd)
		mov	r1, #0
		mcr	p15, 0, r1, c7, c7, 0		@ invalidate cache
		mcr	p15, 0, r0, c2, c0, 0		@ update page table ptr
		mcr	p15, 0, r1, c8, c7, 0		@ flush TLB (v4)
		mov	pc, lr

/*
 * Function: arm720_set_pmd ()
 *
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 *
 * Purpose : Set a PMD and flush it out of any WB cache
 */
ENTRY(cpu_arm720_set_pmd)
		tst	r1, #3
		orrne	r1, r1, #16			@ Updatable bit is
		str	r1, [r0]			@ always set on ARM720
		mov	pc, lr

/*
 * Function: arm720_set_pte(pte_t *ptep, pte_t pte)
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 * Purpose : Set a PTE and flush it out of any WB cache
 */
		.align	5
ENTRY(cpu_arm720_set_pte)
		str	r1, [r0], #-1024		@ linux version

		eor	r1, r1, #LPTE_PRESENT | LPTE_YOUNG | LPTE_WRITE | LPTE_DIRTY

		bic	r2, r1, #0xff0
		bic	r2, r2, #3
		orr	r2, r2, #HPTE_TYPE_SMALL

		tst	r1, #LPTE_USER | LPTE_EXEC	@ User or Exec?
		orrne	r2, r2, #HPTE_AP_READ

		tst	r1, #LPTE_WRITE | LPTE_DIRTY	@ Write and Dirty?
		orreq	r2, r2, #HPTE_AP_WRITE

		tst	r1, #LPTE_PRESENT | LPTE_YOUNG	@ Present and Young
		movne	r2, #0

		str	r2, [r0]			@ hardware version
		mov	pc, lr

/*
 * Function: arm720_reset
 * Params  : r0 = address to jump to
 * Notes   : This sets up everything for a reset
 */
ENTRY(cpu_arm720_reset)
		mov	ip, #0
		mcr	p15, 0, ip, c7, c7, 0		@ invalidate cache
		mcr	p15, 0, ip, c8, c7, 0		@ flush TLB (v4)
		mrc	p15, 0, ip, c1, c0, 0		@ get ctrl register
		bic	ip, ip, #0x000f			@ ............wcam
		bic	ip, ip, #0x2100			@ ..v....s........
		mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
		mov	pc, r0

	
cpu_arm720_name:
		.asciz	"ARM720T"
		.align

		.section ".text.init", #alloc, #execinstr

__arm720_setup:	mov	r0, #0
		mcr	p15, 0, r0, c7, c7, 0		@ invalidate caches
		mcr	p15, 0, r0, c8, c7, 0		@ flush TLB (v4)
		mcr	p15, 0, r4, c2, c0		@ load page table pointer
		mov	r0, #0x1f			@ Domains 0, 1 = client
		mcr	p15, 0, r0, c3, c0		@ load domain access register

		mrc	p15, 0, r0, c1, c0		@ get control register
		bic	r0, r0, #0x0e00			@ ..V. ..RS BLDP WCAM
		orr	r0, r0, #0x2100			@ .... .... .111 .... (old)
		orr	r0, r0, #0x003d			@ ..1. ..01 ..11 1101 (new)
		mov	pc, lr				@ __ret (head-armv.S)

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
		.type	arm720_processor_functions, #object
ENTRY(arm720_processor_functions)
		.word	cpu_arm720_data_abort
		.word	cpu_arm720_check_bugs
		.word	cpu_arm720_proc_init
		.word	cpu_arm720_proc_fin
		.word	cpu_arm720_reset
		.word	cpu_arm720_do_idle

		/* cache */
		.word	cpu_arm720_cache_clean_invalidate_all
		.word	cpu_arm720_cache_clean_invalidate_range
		.word	cpu_arm720_flush_ram_page

		/* dcache */
		.word	cpu_arm720_dcache_invalidate_range
		.word	cpu_arm720_dcache_clean_range
		.word	cpu_arm720_dcache_clean_page
		.word	cpu_arm720_dcache_clean_entry

		/* icache */
		.word	cpu_arm720_icache_invalidate_range
		.word	cpu_arm720_icache_invalidate_page

		/* tlb */
		.word	cpu_arm720_tlb_invalidate_all
		.word	cpu_arm720_tlb_invalidate_range
		.word	cpu_arm720_tlb_invalidate_page

		/* pgtable */
		.word	cpu_arm720_set_pgd
		.word	cpu_arm720_set_pmd
		.word	cpu_arm720_set_pte
		.size	arm720_processor_functions, . - arm720_processor_functions

		.type	cpu_arm720_info, #object
cpu_arm720_info:
		.long	0
		.long	cpu_arm720_name
		.size	cpu_arm720_info, . - cpu_arm720_info
	
		.type	cpu_arch_name, #object
cpu_arch_name:	.asciz	"armv4"
		.size	cpu_arch_name, . - cpu_arch_name

		.type	cpu_elf_name, #object
cpu_elf_name:	.asciz	"v4"
		.size	cpu_elf_name, . - cpu_elf_name
		.align

/*
 * See linux/include/asm-arm/procinfo.h for a definition of this structure.
 */
	
		.section ".proc.info", #alloc, #execinstr

		.type	__arm720_proc_info, #object
__arm720_proc_info:
		.long	0x41807200				@ cpu_val
		.long	0xffffff00				@ cpu_mask
		.long	0x00000c1e				@ section_mmu_flags
		b	__arm720_setup				@ cpu_flush
		.long	cpu_arch_name				@ arch_name
		.long	cpu_elf_name				@ elf_name
		.long	HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB	@ elf_hwcap
		.long	cpu_arm720_info				@ info
		.long	arm720_processor_functions
		.size	__arm720_proc_info, . - __arm720_proc_info
