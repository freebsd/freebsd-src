/*
 *  linux/arch/arm/mm/proc-arm6,7.S
 *
 *  Copyright (C) 1997-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  These are the low level assembler for performing cache and TLB
 *  functions on the ARM610 & ARM710.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>

/*
 * Function: arm6_7_cache_clean_invalidate_all (void)
 *	   : arm6_7_cache_clean_invalidate_page (unsigned long address, int size, int flags)
 *
 * Params  : address	Area start address
 *	   : size	size of area
 *	   : flags	b0 = I cache as well
 *
 * Purpose : Flush all cache lines
 */
ENTRY(cpu_arm6_cache_clean_invalidate_all)
ENTRY(cpu_arm7_cache_clean_invalidate_all)
ENTRY(cpu_arm6_cache_clean_invalidate_range)
ENTRY(cpu_arm7_cache_clean_invalidate_range)
ENTRY(cpu_arm6_icache_invalidate_range)
ENTRY(cpu_arm7_icache_invalidate_range)
ENTRY(cpu_arm6_icache_invalidate_page)
ENTRY(cpu_arm7_icache_invalidate_page)
ENTRY(cpu_arm6_dcache_clean_range)
ENTRY(cpu_arm7_dcache_clean_range)
ENTRY(cpu_arm6_dcache_invalidate_range)
ENTRY(cpu_arm7_dcache_invalidate_range)
		mov	r0, #0
		mcr	p15, 0, r0, c7, c0, 0		@ flush cache
ENTRY(cpu_arm6_dcache_clean_page)
ENTRY(cpu_arm7_dcache_clean_page)
ENTRY(cpu_arm6_dcache_clean_entry)
ENTRY(cpu_arm7_dcache_clean_entry)
ENTRY(cpu_arm6_flush_ram_page)
ENTRY(cpu_arm7_flush_ram_page)
		mov	pc, lr

/*
 * Function: arm6_7_tlb_invalidate_all (void)
 *
 * Purpose : flush all TLB entries in all caches
 */
ENTRY(cpu_arm6_tlb_invalidate_all)
ENTRY(cpu_arm7_tlb_invalidate_all)
		mov	r0, #0
		mcr	p15, 0, r0, c5, c0, 0		@ flush TLB
		mov	pc, lr

/*
 * Function: arm6_7_tlb_invalidate_page (unsigned long address, int end, int flags)
 *
 * Params  : address	Area start address
 *	   : end	Area end address
 *	   : flags	b0 = I cache as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_arm6_tlb_invalidate_range)
                @ arm 6 tbl purge reg is bits 12 - 31
1:		mcr	p15, 0, r0, c6, c0, 0		@ purge TLB
		add	r0, r0, #4096
		cmp	r0, r1
		blt	1b
		mov	pc, lr

ENTRY(cpu_arm7_tlb_invalidate_range)
#if defined(CONFIG_ARCH_RISCSTATION)
		mov	r0, #0
		mcr	p15, 0, r0, c5, c0, 0		@ flush TLB
		mov	pc, lr
#else
                @ arm 7 tbl purge reg is bits 14 - 31
1:		mcr	p15, 0, r0, c6, c0, 0		@ purge TLB
		add	r0, r0, #0x4000
		cmp	r0, r1
		blt	1b
		mov	pc, lr
#endif

/*
 * Function: arm6_7_tlb_invalidate_page (unsigned long address, int flags)
 *
 * Params  : address	Address
 *	   : flags	b0 = I-TLB as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_arm6_tlb_invalidate_page)
ENTRY(cpu_arm7_tlb_invalidate_page)
		mcr	p15, 0, r0, c6, c0, 0		@ flush TLB
		mov	pc, lr

/*
 * Function: arm6_7_data_abort ()
 *
 * Params  : r0 = address of aborted instruction
 *
 * Purpose : obtain information about current aborted instruction
 *
 * Returns : r0 = address of abort
 *	   : r1 != 0 if writing
 *	   : r3 = FSR
 *	   : sp = pointer to registers
 */

ENTRY(cpu_arm6_data_abort)
		ldr	r4, [r0]			@ read instruction causing problem
		tst	r4, r4, lsr #21			@ C = bit 20
		sbc	r1, r1, r1			@ r1 = C - 1
		and	r2, r4, #14 << 24
		teq	r2, #8 << 24			@ was it ldm/stm
		bne	Ldata_simple

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

ENTRY(cpu_arm7_data_abort)
		ldr	r4, [r0]			@ read instruction causing problem
		tst	r4, r4, lsr #21			@ C = bit 20
		sbc	r1, r1, r1			@ r1 = C - 1
		and	r2, r4, #15 << 24
		add	pc, pc, r2, lsr #22		@ Now branch to the relevent processing routine
		movs	pc, lr

		b	Ldata_unknown
		b	Ldata_simple			@ swp
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_lateldrpostconst		@ ldr	rd, [rn], #m
		b	Ldata_lateldrpreconst		@ ldr	rd, [rn, #m]	@ RegVal
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
		mov	r2, r5
		bl	baddataabort
		b	ret_from_exception

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

/*
 * Function: arm6_7_check_bugs (void)
 *	   : arm6_7_proc_init (void)
 *	   : arm6_7_proc_fin (void)
 *
 * Notes   : This processor does not require these
 */
ENTRY(cpu_arm6_check_bugs)
ENTRY(cpu_arm7_check_bugs)
		mrs	ip, cpsr
		bic	ip, ip, #F_BIT
		msr	cpsr, ip
		mov	pc, lr

ENTRY(cpu_arm6_proc_init)
ENTRY(cpu_arm7_proc_init)
		mov	pc, lr

ENTRY(cpu_arm6_proc_fin)
ENTRY(cpu_arm7_proc_fin)
		mov	r0, #F_BIT | I_BIT | SVC_MODE
		msr	cpsr_c, r0
		mov	r0, #0x31			@ ....S..DP...M
		mcr	p15, 0, r0, c1, c0, 0		@ disable caches
		mov	pc, lr

ENTRY(cpu_arm6_do_idle)
ENTRY(cpu_arm7_do_idle)
		mov	pc, lr

/*
 * Function: arm6_7_set_pgd(unsigned long pgd_phys)
 * Params  : pgd_phys	Physical address of page table
 * Purpose : Perform a task switch, saving the old processes state, and restoring
 *	     the new.
 */
ENTRY(cpu_arm6_set_pgd)
ENTRY(cpu_arm7_set_pgd)
		mov	r1, #0
		mcr	p15, 0, r1, c7, c0, 0		@ flush cache
		mcr	p15, 0, r0, c2, c0, 0		@ update page table ptr
		mcr	p15, 0, r1, c5, c0, 0		@ flush TLBs
		mov	pc, lr

/*
 * Function: arm6_set_pmd ()
 *
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 *
 * Purpose : Set a PMD and flush it out of any WB cache
 */
ENTRY(cpu_arm6_set_pmd)
		and	r2, r1, #11
		teq	r2, #1
		teqne	r2, #9
		teqne	r2, #10
		orreq	r1, r1, #16			@ Updatable = 1 if Page table/Cacheable section
		str	r1, [r0]
		mov	pc, lr

/*
 * Function: arm7_set_pmd ()
 *
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 *
 * Purpose : Set a PMD and flush it out of any WB cache
 */
ENTRY(cpu_arm7_set_pmd)
		tst	r1, #3
		orrne	r1, r1, #16			@ Updatable bit is always set on ARM7
		str	r1, [r0]
		mov	pc, lr

/*
 * Function: arm6_7_set_pte(pte_t *ptep, pte_t pte)
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 * Purpose : Set a PTE and flush it out of any WB cache
 */
		.align	5
ENTRY(cpu_arm6_set_pte)
ENTRY(cpu_arm7_set_pte)
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
 * Function: _arm6_7_reset
 * Params  : r0 = address to jump to
 * Notes   : This sets up everything for a reset
 */
ENTRY(cpu_arm6_reset)
ENTRY(cpu_arm7_reset)
		mov	r1, #0
		mcr	p15, 0, r1, c7, c0, 0		@ flush cache
		mcr	p15, 0, r1, c5, c0, 0		@ flush TLB
		mov	r1, #0x30
		mcr	p15, 0, r1, c1, c0, 0		@ turn off MMU etc
		mov	pc, r0

cpu_arm6_name:	.asciz	"ARM 6"
cpu_arm610_name:
		.asciz	"ARM 610"
cpu_arm7_name:	.asciz	"ARM 7"
cpu_arm710_name:
		.asciz	"ARM 710"
		.align

		.section ".text.init", #alloc, #execinstr

__arm6_setup:	mov	r0, #0
		mcr	p15, 0, r0, c7, c0		@ flush caches on v3
		mcr	p15, 0, r0, c5, c0		@ flush TLBs on v3
		mcr	p15, 0, r4, c2, c0		@ load page table pointer
		mov	r0, #0x1f			@ Domains 0, 1 = client
		mcr	p15, 0, r0, c3, c0		@ load domain access register
		mov	r0, #0x3d			@ . ..RS BLDP WCAM
		orr	r0, r0, #0x100			@ . ..01 0011 1101
		mov	pc, lr

__arm7_setup:	mov	r0, #0
		mcr	p15, 0, r0, c7, c0		@ flush caches on v3
		mcr	p15, 0, r0, c5, c0		@ flush TLBs on v3
		mcr	p15, 0, r4, c2, c0		@ load page table pointer
		mov	r0, #0x1f			@ Domains 0, 1 = client
		mcr	p15, 0, r0, c3, c0		@ load domain access register
		mov	r0, #0x7d			@ . ..RS BLDP WCAM
		orr	r0, r0, #0x100			@ . ..01 0111 1101
		mov	pc, lr

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
		.type	arm6_processor_functions, #object
ENTRY(arm6_processor_functions)
		.word	cpu_arm6_data_abort
		.word	cpu_arm6_check_bugs
		.word	cpu_arm6_proc_init
		.word	cpu_arm6_proc_fin
		.word	cpu_arm6_reset
		.word	cpu_arm6_do_idle

		/* cache */
		.word	cpu_arm6_cache_clean_invalidate_all
		.word	cpu_arm6_cache_clean_invalidate_range
		.word	cpu_arm6_flush_ram_page

		/* dcache */
		.word	cpu_arm6_dcache_invalidate_range
		.word	cpu_arm6_dcache_clean_range
		.word	cpu_arm6_dcache_clean_page
		.word	cpu_arm6_dcache_clean_entry

		/* icache */
		.word	cpu_arm6_icache_invalidate_range
		.word	cpu_arm6_icache_invalidate_page

		/* tlb */
		.word	cpu_arm6_tlb_invalidate_all
		.word	cpu_arm6_tlb_invalidate_range
		.word	cpu_arm6_tlb_invalidate_page

		/* pgtable */
		.word	cpu_arm6_set_pgd
		.word	cpu_arm6_set_pmd
		.word	cpu_arm6_set_pte
		.size	arm6_processor_functions, . - arm6_processor_functions

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
		.type	arm7_processor_functions, #object
ENTRY(arm7_processor_functions)
		.word	cpu_arm7_data_abort
		.word	cpu_arm7_check_bugs
		.word	cpu_arm7_proc_init
		.word	cpu_arm7_proc_fin
		.word	cpu_arm7_reset
		.word	cpu_arm7_do_idle

		/* cache */
		.word	cpu_arm7_cache_clean_invalidate_all
		.word	cpu_arm7_cache_clean_invalidate_range
		.word	cpu_arm7_flush_ram_page

		/* dcache */
		.word	cpu_arm7_dcache_invalidate_range
		.word	cpu_arm7_dcache_clean_range
		.word	cpu_arm7_dcache_clean_page
		.word	cpu_arm7_dcache_clean_entry

		/* icache */
		.word	cpu_arm7_icache_invalidate_range
		.word	cpu_arm7_icache_invalidate_page

		/* tlb */
		.word	cpu_arm7_tlb_invalidate_all
		.word	cpu_arm7_tlb_invalidate_range
		.word	cpu_arm7_tlb_invalidate_page

		/* pgtable */
		.word	cpu_arm7_set_pgd
		.word	cpu_arm7_set_pmd
		.word	cpu_arm7_set_pte
		.size	arm7_processor_functions, . - arm7_processor_functions

		.type	cpu_arm6_info, #object
cpu_arm6_info:
		.long	0
		.long	cpu_arm6_name
		.size	cpu_arm6_info, . - cpu_arm6_info

		.type	cpu_arm610_info, #object
cpu_arm610_info:
		.long	0
		.long	cpu_arm610_name
		.size	cpu_arm610_info, . - cpu_Arm610_info

		.type	cpu_arm7_info, #object
cpu_arm7_info:
		.long	0
		.long	cpu_arm7_name
		.size	cpu_arm7_info, . - cpu_arm7_info

		.type	cpu_arm710_info, #object
cpu_arm710_info:
		.long	0
		.long	cpu_arm710_name
		.size	cpu_arm710_info, . - cpu_arm710_info

		.type	cpu_arch_name, #object
cpu_arch_name:	.asciz	"armv3"
		.size	cpu_arch_name, . - cpu_arch_name

		.type	cpu_elf_name, #object
cpu_elf_name:	.asciz	"v3"
		.size	cpu_elf_name, . - cpu_elf_name
		.align

		.section ".proc.info", #alloc, #execinstr

		.type	__arm6_proc_info, #object
__arm6_proc_info:
		.long	0x41560600
		.long	0xfffffff0
		.long	0x00000c1e
		b	__arm6_setup
		.long	cpu_arch_name
		.long	cpu_elf_name
		.long	HWCAP_SWP | HWCAP_26BIT
		.long	cpu_arm6_info
		.long	arm6_processor_functions
		.size	__arm6_proc_info, . - __arm6_proc_info

		.type	__arm610_proc_info, #object
__arm610_proc_info:
		.long	0x41560610
		.long	0xfffffff0
		.long	0x00000c1e
		b	__arm6_setup
		.long	cpu_arch_name
		.long	cpu_elf_name
		.long	HWCAP_SWP | HWCAP_26BIT
		.long	cpu_arm610_info
		.long	arm6_processor_functions
		.size	__arm610_proc_info, . - __arm610_proc_info

		.type	__arm7_proc_info, #object
__arm7_proc_info:
		.long	0x41007000
		.long	0xffffff00
		.long	0x00000c1e
		b	__arm7_setup
		.long	cpu_arch_name
		.long	cpu_elf_name
		.long	HWCAP_SWP | HWCAP_26BIT
		.long	cpu_arm7_info
		.long	arm7_processor_functions
		.size	__arm7_proc_info, . - __arm7_proc_info

		.type	__arm710_proc_info, #object
__arm710_proc_info:
		.long	0x41007100
		.long	0xfff8ff00
		.long	0x00000c1e
		b	__arm7_setup
		.long	cpu_arch_name
		.long	cpu_elf_name
		.long	HWCAP_SWP | HWCAP_26BIT
		.long	cpu_arm710_info
		.long	arm7_processor_functions
		.size	__arm710_proc_info, . - __arm710_proc_info
