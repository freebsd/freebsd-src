/*
 *  linux/arch/arm/mm/proc-sa110.S
 *
 *  Copyright (C) 1997-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  MMU functions for SA110
 *
 *  These are the low level assembler for performing cache and TLB
 *  functions on the StrongARM-110, StrongARM-1100 and StrongARM-1110.
 * 
 *  Note that SA1100 and SA1110 share everything but their name and CPU ID.
 *
 *  12-jun-2000, Erik Mouw (J.A.K.Mouw@its.tudelft.nl):
 *    Flush the read buffer at context switches
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

/* This is the maximum size of an area which will be flushed.  If the area
 * is larger than this, then we flush the whole cache
 */
#define MAX_AREA_SIZE	32768

/*
 * the cache line size of the I and D cache
 */
#define DCACHELINESIZE	32

/*
 * and the page size
 */
#define PAGESIZE	4096

#define FLUSH_OFFSET	32768

		.macro flush_110_dcache	rd, ra, re
		add	\re, \ra, #16384		@ only necessary for 16k
1001:		ldr	\rd, [\ra], #DCACHELINESIZE
		teq	\re, \ra
		bne	1001b
		.endm

		.macro flush_1100_dcache	rd, ra, re
		add	\re, \ra, #8192			@ only necessary for 8k
1001:		ldr	\rd, [\ra], #DCACHELINESIZE
		teq	\re, \ra
		bne	1001b
#ifdef FLUSH_BASE_MINICACHE
		add	\ra, \ra, #FLUSH_BASE_MINICACHE - FLUSH_BASE
		add	\re, \ra, #512			@ only 512 bytes
1002:		ldr	\rd, [\ra], #DCACHELINESIZE
		teq	\re, \ra
		bne	1002b
#endif
		.endm

		.data
Lclean_switch:	.long	0
		.text


/*
 * cpu_sa110_data_abort()
 *
 * obtain information about current aborted instruction.
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it.
 *
 *  r2 = address of aborted instruction
 *  r3 = cpsr
 *
 * Returns:
 *  r0 = address of abort
 *  r1 != 0 if writing
 *  r3 = FSR
 *  r4 = corrupted
 */
	.align	5
ENTRY(cpu_sa110_data_abort)
ENTRY(cpu_sa1100_data_abort)
	mrc	p15, 0, r3, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
	ldr	r1, [r2]			@ read aborted instruction
	and	r3, r3, #255
	tst	r1, r1, lsr #21			@ C = bit 20
	sbc	r1, r1, r1			@ r1 = C - 1
	mov	pc, lr

/*
 * cpu_sa110_check_bugs()
 */
ENTRY(cpu_sa110_check_bugs)
ENTRY(cpu_sa1100_check_bugs)
	mrs	ip, cpsr
	bic	ip, ip, #F_BIT
	msr	cpsr, ip
	mov	pc, lr

/*
 * cpu_sa110_proc_init()
 */
ENTRY(cpu_sa110_proc_init)
ENTRY(cpu_sa1100_proc_init)
	mov	r0, #0
	mcr	p15, 0, r0, c15, c1, 2		@ Enable clock switching
	mov	pc, lr

/*
 * cpu_sa110_proc_fin()
 */
ENTRY(cpu_sa110_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #F_BIT | I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	cpu_sa110_cache_clean_invalidate_all	@ clean caches
1:	mov	r0, #0
	mcr	p15, 0, r0, c15, c2, 2		@ Disable clock switching
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000			@ ...i............
	bic	r0, r0, #0x000e			@ ............wca.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

ENTRY(cpu_sa1100_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #F_BIT | I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	cpu_sa1100_cache_clean_invalidate_all	@ clean caches
	b	1b

/*
 * cpu_sa110_reset(loc)
 *
 * Perform a soft reset of the system.  Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_sa110_reset)
ENTRY(cpu_sa1100_reset)
	mov	ip, #0
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I,D caches
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	mrc	p15, 0, ip, c1, c0, 0		@ ctrl register
	bic	ip, ip, #0x000f			@ ............wcam
	bic	ip, ip, #0x1100			@ ...i...s........
	mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
	mov	pc, r0

/*
 * cpu_sa110_do_idle()
 *
 * Cause the processor to idle
 */
	.align	5
ENTRY(cpu_sa110_do_idle)
ENTRY(cpu_sa1100_do_idle)
	mov	r0, r0				@ 4 nop padding
	mov	r0, r0
	mov	r0, r0
	mov	r0, #0
	ldr	r1, =UNCACHEABLE_ADDR		@ ptr to uncacheable address
	mrs	r2, cpsr
	orr	r3, r2, #192			@ disallow interrupts
	msr	cpsr_c, r3
	@ --- aligned to a cache line
	mcr	p15, 0, r0, c15, c2, 2		@ disable clock switching
	ldr	r1, [r1, #0]			@ force switch to MCLK
	mcr	p15, 0, r0, c15, c8, 2		@ wait for interrupt
	mov	r0, r0				@ safety
	mcr	p15, 0, r0, c15, c1, 2		@ enable clock switching
	msr	cpsr_c, r2			@ allow interrupts
	mov	pc, lr

/* ================================= CACHE ================================ */


/*
 * cpu_sa110_cache_clean_invalidate_all (void)
 *
 * clean and invalidate all cache lines
 *
 * Note:
 *  1. we should preserve r0 at all times
 */
	.align	5
ENTRY(cpu_sa110_cache_clean_invalidate_all)
	mov	r2, #1
cpu_sa110_cache_clean_invalidate_all_r2:
	ldr	r3, =Lclean_switch
	ldr	ip, =FLUSH_BASE
	ldr	r1, [r3]
	ands	r1, r1, #1
	eor	r1, r1, #1
	str	r1, [r3]
	addne	ip, ip, #FLUSH_OFFSET
	flush_110_dcache	r3, ip, r1
	mov	ip, #0
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

	.align	5
ENTRY(cpu_sa1100_cache_clean_invalidate_all)
	mov	r2, #1
cpu_sa1100_cache_clean_invalidate_all_r2:
	ldr	r3, =Lclean_switch
	ldr	ip, =FLUSH_BASE
	ldr	r1, [r3]
	ands	r1, r1, #1
	eor	r1, r1, #1
	str	r1, [r3]
	addne	ip, ip, #FLUSH_OFFSET
	flush_1100_dcache	r3, ip, r1
	mov	ip, #0
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, r1, c9, c0, 0		@ invalidate RB
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

/*
 * cpu_sa110_cache_clean_invalidate_range(start, end, flags)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * start: Area start address
 * end:   Area end address
 * flags: nonzero for I cache as well
 */
	.align	5
ENTRY(cpu_sa110_cache_clean_invalidate_range)
	bic	r0, r0, #DCACHELINESIZE - 1
	sub	r3, r1, r0
	cmp	r3, #MAX_AREA_SIZE
	bhi	cpu_sa110_cache_clean_invalidate_all_r2
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D entry
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D entry
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blo	1b
	teq	r2, #0
	movne	r0, #0
	mcrne	p15, 0, r0, c7, c5, 0		@ invalidate I cache
	mov	pc, lr

ENTRY(cpu_sa1100_cache_clean_invalidate_range)
	sub	r3, r1, r0
	cmp	r3, #MAX_AREA_SIZE
	bhi	cpu_sa1100_cache_clean_invalidate_all_r2
	b	1b

/*
 * cpu_sa110_flush_ram_page(page)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * page: page to clean and invalidate
 */
	.align	5
ENTRY(cpu_sa110_flush_ram_page)
ENTRY(cpu_sa1100_flush_ram_page)
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	subs	r1, r1, #2 * DCACHELINESIZE
	bne	1b
	mcr	p15, 0, r1, c7, c10, 4		@ drain WB
	mov	pc, lr

/* ================================ D-CACHE =============================== */

/*
 * cpu_sa110_dcache_invalidate_range(start, end)
 *
 * throw away all D-cached data in specified region without an obligation
 * to write them back.  Note however that we must clean the D-cached entries
 * around the boundaries if the start and/or end address are not cache
 * aligned.
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_sa110_dcache_invalidate_range)
ENTRY(cpu_sa1100_dcache_invalidate_range)
	tst	r0, #DCACHELINESIZE - 1
	bic	r0, r0, #DCACHELINESIZE - 1
	mcrne	p15, 0, r0, c7, c10, 1		@ clean D entry
	tst	r1, #DCACHELINESIZE - 1
	mcrne	p15, 0, r1, c7, c10, 1		@ clean D entry
1:	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D entry
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blo	1b
	mov	pc, lr

/*
 * cpu_sa110_dcache_clean_range(start, end)
 *
 * For the specified virtual address range, ensure that all caches contain
 * clean data, such that peripheral accesses to the physical RAM fetch
 * correct data.
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_sa110_dcache_clean_range)
	bic	r0, r0, #DCACHELINESIZE - 1
	sub	r1, r1, r0
	cmp	r1, #MAX_AREA_SIZE
	mov	r2, #0
	bhi	cpu_sa110_cache_clean_invalidate_all_r2
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	subs	r1, r1, #2 * DCACHELINESIZE
	bpl	1b
	mcr	p15, 0, r2, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(cpu_sa1100_dcache_clean_range)
	bic	r0, r0, #DCACHELINESIZE - 1
	sub	r1, r1, r0
	cmp	r1, #MAX_AREA_SIZE
	mov	r2, #0
	bhi	cpu_sa1100_cache_clean_invalidate_all_r2
	b	1b

/*
 * cpu_sa110_clean_dcache_page(page)
 *
 * Cleans a single page of dcache so that if we have any future aliased
 * mappings, they will be consistent at the time that they are created.
 *
 * Note:
 *  1. we don't need to flush the write buffer in this case.
 *  2. we don't invalidate the entries since when we write the page
 *     out to disk, the entries may get reloaded into the cache.
 */
	.align	5
ENTRY(cpu_sa110_dcache_clean_page)
ENTRY(cpu_sa1100_dcache_clean_page)
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #DCACHELINESIZE
	subs	r1, r1, #2 * DCACHELINESIZE
	bne	1b
	mov	pc, lr

/*
 * cpu_sa110_dcache_clean_entry(addr)
 *
 * Clean the specified entry of any caches such that the MMU
 * translation fetches will obtain correct data.
 *
 * addr: cache-unaligned virtual address
 */
	.align	5
ENTRY(cpu_sa110_dcache_clean_entry)
ENTRY(cpu_sa1100_dcache_clean_entry)
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mov	pc, lr

/* ================================ I-CACHE =============================== */

/*
 * cpu_sa110_icache_invalidate_range(start, end)
 *
 * invalidate a range of virtual addresses from the Icache
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_sa110_icache_invalidate_range)
ENTRY(cpu_sa1100_icache_invalidate_range)
	bic	r0, r0, #DCACHELINESIZE - 1
1:	mcr	p15, 0, r0, c7, c10, 1		@ Clean D entry
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blo	1b
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
ENTRY(cpu_sa110_icache_invalidate_page)
ENTRY(cpu_sa1100_icache_invalidate_page)
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate I cache
	mov	pc, lr

/* ================================== TLB ================================= */

/*
 * cpu_sa110_tlb_invalidate_all()
 *
 * Invalidate all TLB entries
 */
	.align	5
ENTRY(cpu_sa110_tlb_invalidate_all)
ENTRY(cpu_sa1100_tlb_invalidate_all)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I & D TLBs
	mov	pc, lr

/*
 * cpu_sa110_tlb_invalidate_range(start, end)
 *
 * invalidate TLB entries covering the specified range
 *
 * start: range start address
 * end:   range end address
 */
	.align	5
ENTRY(cpu_sa110_tlb_invalidate_range)
ENTRY(cpu_sa1100_tlb_invalidate_range)
	bic	r0, r0, #0x0ff
	bic	r0, r0, #0xf00
	sub	r3, r1, r0
	cmp	r3, #256 * PAGESIZE		@ arbitary, should be tuned
	bhi	cpu_sa110_tlb_invalidate_all
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
1:	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	add	r0, r0, #PAGESIZE
	cmp	r0, r1
	blo	1b
	mcr	p15, 0, r3, c8, c5, 0		@ invalidate I TLB
	mov	pc, lr

/*
 * cpu_sa110_tlb_invalidate_page(page, flags)
 *
 * invalidate the TLB entries for the specified page.
 *
 * page:  page to invalidate
 * flags: non-zero if we include the I TLB
 */
	.align	5
ENTRY(cpu_sa110_tlb_invalidate_page)
ENTRY(cpu_sa1100_tlb_invalidate_page)
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
	teq	r1, #0
	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	mcrne	p15, 0, r3, c8, c5, 0		@ invalidate I TLB
	mov	pc, lr

/* =============================== PageTable ============================== */

/*
 * cpu_sa110_set_pgd(pgd)
 *
 * Set the translation base pointer to be as described by pgd.
 *
 * pgd: new page tables
 */
	.align	5
ENTRY(cpu_sa110_set_pgd)
	ldr	r3, =Lclean_switch
	ldr	ip, =FLUSH_BASE
	ldr	r2, [r3]
	ands	r2, r2, #1
	eor	r2, r2, #1
	str	r2, [r3]
	addne	ip, ip, #FLUSH_OFFSET
	flush_110_dcache	r3, ip, r1
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, r1, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, r1, c8, c7, 0		@ invalidate I & D TLBs
	mov	pc, lr

/*
 * cpu_sa1100_set_pgd(pgd)
 *
 * Set the translation base pointer to be as described by pgd.
 *
 * pgd: new page tables
 */
	.align	5
ENTRY(cpu_sa1100_set_pgd)
	ldr	r3, =Lclean_switch
	ldr	ip, =FLUSH_BASE
	ldr	r2, [r3]
	ands	r2, r2, #1
	eor	r2, r2, #1
	str	r2, [r3]
	addne	ip, ip, #FLUSH_OFFSET
	flush_1100_dcache	r3, ip, r1
	mov	ip, #0
	mcr	p15, 0, ip, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, ip, c9, c0, 0		@ invalidate RB
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	mov	pc, lr

/*
 * cpu_sa110_set_pmd(pmdp, pmd)
 *
 * Set a level 1 translation table entry, and clean it out of
 * any caches such that the MMUs can load it correctly.
 *
 * pmdp: pointer to PMD entry
 * pmd:  PMD value to store
 */
	.align	5
ENTRY(cpu_sa110_set_pmd)
ENTRY(cpu_sa1100_set_pmd)
	str	r1, [r0]
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mov	pc, lr

/*
 * cpu_sa110_set_pte(ptep, pte)
 *
 * Set a PTE and flush it out
 */
	.align	5
ENTRY(cpu_sa110_set_pte)
ENTRY(cpu_sa1100_set_pte)
	str	r1, [r0], #-1024		@ linux version

	eor	r1, r1, #LPTE_PRESENT | LPTE_YOUNG | LPTE_WRITE | LPTE_DIRTY

	bic	r2, r1, #0xff0
	bic	r2, r2, #3
	orr	r2, r2, #HPTE_TYPE_SMALL

	tst	r1, #LPTE_USER | LPTE_EXEC	@ User or Exec?
	orrne	r2, r2, #HPTE_AP_READ

	tst	r1, #LPTE_WRITE | LPTE_DIRTY	@ Write and Dirty?
	orreq	r2, r2, #HPTE_AP_WRITE

	tst	r1, #LPTE_PRESENT | LPTE_YOUNG	@ Present and Young?
	movne	r2, #0

	str	r2, [r0]			@ hardware version
	mov	r0, r0
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mov	pc, lr


cpu_sa110_name:
	.asciz	"StrongARM-110"
cpu_sa1100_name:
	.asciz	"StrongARM-1100"
cpu_sa1110_name:
	.asciz	"StrongARM-1110"
	.align

	.section ".text.init", #alloc, #execinstr

__sa1100_setup:	@ Allow read-buffer operations from userland
	mcr	p15, 0, r0, c9, c0, 5
	mrc	p15, 0, r0, c1, c0		@ get control register v4
	bic	r0, r0, #0x0e00			@ ..VI ZFRS BLDP WCAM
	bic	r0, r0, #0x0002			@ .... 000. .... ..0.
	orr	r0, r0, #0x003d
	orr	r0, r0, #0x3100			@ ..11 ...1 ..11 11.1
	b	__setup_common

__sa110_setup:
	mrc	p15, 0, r0, c1, c0		@ get control register v4
	bic	r0, r0, #0x2e00			@ ..VI ZFRS BLDP WCAM
	bic	r0, r0, #0x0002			@ ..0. 000. .... ..0.
	orr	r0, r0, #0x003d
	orr	r0, r0, #0x1100			@ ...1 ...1 ..11 11.1
__setup_common:
	mov	r10, #0
	mcr	p15, 0, r10, c7, c7		@ invalidate I,D caches on v4
	mcr	p15, 0, r10, c7, c10, 4		@ drain write buffer on v4
	mcr	p15, 0, r10, c8, c7		@ invalidate I,D TLBs on v4
	mcr	p15, 0, r4, c2, c0		@ load page table pointer
	mov	r10, #0x1f			@ Domains 0, 1 = client
	mcr	p15, 0, r10, c3, c0		@ load domain access register
	mov	pc, lr

	.text

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */

	.type	sa110_processor_functions, #object
ENTRY(sa110_processor_functions)
	.word	cpu_sa110_data_abort
	.word	cpu_sa110_check_bugs
	.word	cpu_sa110_proc_init
	.word	cpu_sa110_proc_fin
	.word	cpu_sa110_reset
	.word	cpu_sa110_do_idle

	/* cache */
	.word	cpu_sa110_cache_clean_invalidate_all
	.word	cpu_sa110_cache_clean_invalidate_range
	.word	cpu_sa110_flush_ram_page

	/* dcache */
	.word	cpu_sa110_dcache_invalidate_range
	.word	cpu_sa110_dcache_clean_range
	.word	cpu_sa110_dcache_clean_page
	.word	cpu_sa110_dcache_clean_entry

	/* icache */
	.word	cpu_sa110_icache_invalidate_range
	.word	cpu_sa110_icache_invalidate_page

	/* tlb */
	.word	cpu_sa110_tlb_invalidate_all
	.word	cpu_sa110_tlb_invalidate_range
	.word	cpu_sa110_tlb_invalidate_page

	/* pgtable */
	.word	cpu_sa110_set_pgd
	.word	cpu_sa110_set_pmd
	.word	cpu_sa110_set_pte
	.size	sa110_processor_functions, . - sa110_processor_functions

	.type	cpu_sa110_info, #object
cpu_sa110_info:
	.long	0
	.long	cpu_sa110_name
	.size	cpu_sa110_info, . - cpu_sa110_info


/*
 * SA1100 and SA1110 share the same function calls
 */
	.type	sa1100_processor_functions, #object
ENTRY(sa1100_processor_functions)
	.word	cpu_sa1100_data_abort
	.word	cpu_sa1100_check_bugs
	.word	cpu_sa1100_proc_init
	.word	cpu_sa1100_proc_fin
	.word	cpu_sa1100_reset
	.word	cpu_sa1100_do_idle

	/* cache */
	.word	cpu_sa1100_cache_clean_invalidate_all
	.word	cpu_sa1100_cache_clean_invalidate_range
	.word	cpu_sa1100_flush_ram_page

	/* dcache */
	.word	cpu_sa1100_dcache_invalidate_range
	.word	cpu_sa1100_dcache_clean_range
	.word	cpu_sa1100_dcache_clean_page
	.word	cpu_sa1100_dcache_clean_entry

	/* icache */
	.word	cpu_sa1100_icache_invalidate_range
	.word	cpu_sa1100_icache_invalidate_page

	/* tlb */
	.word	cpu_sa1100_tlb_invalidate_all
	.word	cpu_sa1100_tlb_invalidate_range
	.word	cpu_sa1100_tlb_invalidate_page

	/* pgtable */
	.word	cpu_sa1100_set_pgd
	.word	cpu_sa1100_set_pmd
	.word	cpu_sa1100_set_pte
	.size	sa1100_processor_functions, . - sa1100_processor_functions

cpu_sa1100_info:
	.long	0
	.long	cpu_sa1100_name
	.size	cpu_sa1100_info, . - cpu_sa1100_info

cpu_sa1110_info:
	.long	0
	.long	cpu_sa1110_name
	.size	cpu_sa1110_info, . - cpu_sa1110_info

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv4"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v4"
	.size	cpu_elf_name, . - cpu_elf_name
	.align

	.section ".proc.info", #alloc, #execinstr

	.type	__sa110_proc_info,#object
__sa110_proc_info:
	.long	0x4401a100
	.long	0xfffffff0
	.long	0x00000c0e
	b	__sa110_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT | HWCAP_FAST_MULT
	.long	cpu_sa110_info
	.long	sa110_processor_functions
	.size	__sa110_proc_info, . - __sa110_proc_info

	.type	__sa1100_proc_info,#object
__sa1100_proc_info:
	.long	0x4401a110
	.long	0xfffffff0
	.long	0x00000c0e
	b	__sa1100_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT | HWCAP_FAST_MULT
	.long	cpu_sa1100_info
	.long	sa1100_processor_functions
	.size	__sa1100_proc_info, . - __sa1100_proc_info

	.type	__sa1110_proc_info,#object
__sa1110_proc_info:
	.long	0x6901b110
	.long	0xfffffff0
	.long	0x00000c0e
	b	__sa1100_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT | HWCAP_FAST_MULT
	.long	cpu_sa1110_info
	.long	sa1100_processor_functions
	.size	__sa1110_proc_info, . - __sa1110_proc_info
