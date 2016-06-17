/*
 *  linux/arch/arm/mm/arm1020.S: MMU functions for ARM1020
 *
 *  Copyright (C) 2000 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
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
 * functions on the arm1020.
 */
#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

/*
 * This is the maximum size of an area which will be invalidated
 * using the single invalidate entry instructions.  Anything larger
 * than this, and we go for the whole cache.
 *
 * This value should be chosen such that we choose the cheapest
 * alternative.
 */
#define MAX_AREA_SIZE	32768

/*
 * the cache line size of the I and D cache
 */
#define DCACHELINESIZE	32
#define ICACHELINESIZE	32

/*
 * and the page size
 */
#define PAGESIZE	4096

	.text

/*
 * cpu_arm1020_data_abort()
 *
 * obtain information about current aborted instruction
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
ENTRY(cpu_arm1020_data_abort)
	mrc	p15, 0, r3, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
	ldr	r1, [r2]			@ read aborted instruction
	and	r3, r3, #255
	tst	r1, r1, lsr #21 		@ C = bit 20
	sbc	r1, r1, r1			@ r1 = C - 1
	mov	pc, lr

/*
 * cpu_arm1020_check_bugs()
 */
ENTRY(cpu_arm1020_check_bugs)
	mrs	ip, cpsr
	bic	ip, ip, #F_BIT
	msr	cpsr, ip
	mov	pc, lr

/*
 * cpu_arm1020_proc_init()
 */
ENTRY(cpu_arm1020_proc_init)
	mov	pc, lr

/*
 * cpu_arm1020_proc_fin()
 */
ENTRY(cpu_arm1020_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #F_BIT | I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	cpu_arm1020_cache_clean_invalidate_all
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000 		@ ...i............
	bic	r0, r0, #0x000e 		@ ............wca.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

/*
 * cpu_arm1020_reset(loc)
 *
 * Perform a soft reset of the system.	Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_arm1020_reset)
	mov	ip, #0
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I,D caches
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	mrc	p15, 0, ip, c1, c0, 0		@ ctrl register
	bic	ip, ip, #0x000f 		@ ............wcam
	bic	ip, ip, #0x1100 		@ ...i...s........
	mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
	mov	pc, r0

/*
 * cpu_arm1020_do_idle()
 */
	.align	5
ENTRY(cpu_arm1020_do_idle)
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/* ================================= CACHE ================================ */


/*
 * cpu_arm1020_cache_clean_invalidate_all()
 *
 * clean and invalidate all cache lines
 *
 * Note:
 *  1. we should preserve r0 at all times
 */
	.align	5
ENTRY(cpu_arm1020_cache_clean_invalidate_all)
	mov	r2, #1
cpu_arm1020_cache_clean_invalidate_all_r2:
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, ip, c7, c10, 4

	mov	r1, #0xf			@ 16 segments
1:	mov	r3, #0x3F			@ 64 entries
2:	mov	ip, r3, LSL #26 		@ shift up entry
	orr	ip, ip, r1, LSL #5		@ shift in/up index
	mcr	p15, 0, ip, c7, c14, 2		@ Clean & Inval DCache entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB 
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	ip, ip
#endif
	subs	r3, r3, #1
	cmp	r3, #0
	bge	2b				@ entries 3F to 0
	subs	r1, r1, #1
	cmp	r1, #0
	bge	1b				@ segments 7 to 0
#endif
        
#ifndef CONFIG_CPU_ICACHE_DISABLE
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate I cache
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	ip, ip
	mov	ip, ip
#endif
	mov	pc, lr

/*
 * cpu_arm1020_cache_clean_invalidate_range(start, end, flags)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * start: Area start address
 * end:   Area end address
 * flags: nonzero for I cache as well
 */
	.align	5
ENTRY(cpu_arm1020_cache_clean_invalidate_range)
	bic	r0, r0, #DCACHELINESIZE - 1
	sub	r3, r1, r0
	cmp	r3, #MAX_AREA_SIZE
	bgt	cpu_arm1020_cache_clean_invalidate_all_r2
	mcr	p15, 0, r3, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
1:	mcr	p15, 0, r0, c7, c14, 1		@ clean and invalidate D entry
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c14, 1		@ clean and invalidate D entry
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blt	1b
#endif

#ifndef CONFIG_CPU_ICACHE_DISABLE
	teq	r2, #0
	movne	r0, #0
	mcrne	p15, 0, r0, c7, c5, 0		@ invalidate I cache
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
#endif
	mov	pc, lr

/*
 * cpu_arm1020_flush_ram_page(page)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * page: page to clean and invalidate
 */
	.align	5
ENTRY(cpu_arm1020_flush_ram_page)
	mcr	p15, 0, r1, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	subs	r1, r1, #2 * DCACHELINESIZE
	bne	1b
	mov	r0, #0

#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/* ================================ D-CACHE =============================== */

/*
 * cpu_arm1020_dcache_invalidate_range(start, end)
 *
 * throw away all D-cached data in specified region without an obligation
 * to write them back.	Note however that we must clean the D-cached entries
 * around the boundaries if the start and/or end address are not cache
 * aligned.
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_arm1020_dcache_invalidate_range)
#ifndef CONFIG_CPU_DCACHE_DISABLE
	/* D cache are on */
	tst	r0, #DCACHELINESIZE - 1
	bic	r0, r0, #DCACHELINESIZE - 1
	mcrne	p15, 0, r0, c7, c10, 4
	mcrne	p15, 0, r0, c7, c10, 1		@ clean D entry at start
	mcrne	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	tst	r1, #DCACHELINESIZE - 1
	mcrne	p15, 0, r1, c7, c10, 4
	mcrne	p15, 0, r1, c7, c10, 1		@ clean D entry at end
	mcrne	p15, 0, r1, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r1, r1
	mov	r1, r1
	mov	r1, r1
#endif
        
1:	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D entry
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blt	1b
#else
	/* D cache off, but still drain the write buffer */
	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
#endif
	mov	pc, lr

/*
 * cpu_arm1020_dcache_clean_range(start, end)
 *
 * For the specified virtual address range, ensure that all caches contain
 * clean data, such that peripheral accesses to the physical RAM fetch
 * correct data.
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_arm1020_dcache_clean_range)
	bic	r0, r0, #DCACHELINESIZE - 1
	sub	r3, r1, r0
	cmp	r3, #MAX_AREA_SIZE
	bgt	cpu_arm1020_cache_clean_invalidate_all_r2
	mcr	p15, 0, r3, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
1:	mcr	p15, 0, r0, c7, c14, 1		@ clean and invalidate D entry
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c14, 1		@ clean and invalidate D entry
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blt	1b
#endif

#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/*
 * cpu_arm1020_dcache_clean_page(page)
 *
 * Cleans a single page of dcache so that if we have any future aliased
 * mappings, they will be consistent at the time that they are created.
 *
 * page: virtual address of page to clean from dcache
 *
 * Note:
 *  1. we don't need to flush the write buffer in this case.
 *  2. we don't invalidate the entries since when we write the page
 *     out to disk, the entries may get reloaded into the cache.
 */
	.align	5
ENTRY(cpu_arm1020_dcache_clean_page)
	mov	r1, #PAGESIZE
	mcr	p15, 0, r0, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry  (drain is done by TLB fns)
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	subs	r1, r1, #2 * DCACHELINESIZE
	bhi	1b
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/*
 * cpu_arm1020_dcache_clean_entry(addr)
 *
 * Clean the specified entry of any caches such that the MMU
 * translation fetches will obtain correct data.
 *
 * addr: cache-unaligned virtual address
 */
	.align	5
ENTRY(cpu_arm1020_dcache_clean_entry)
	mov	r1, #0
	mcr	p15, 0, r1, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 1		@ clean single D entry
	mcr	p15, 0, r1, c7, c10, 4		@ drain WB
#endif
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcr	p15, 0, r1, c7, c5, 1		@ invalidate I entry
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r1, r1
	mov	r1, r1
#endif
	mov	pc, lr

/* ================================ I-CACHE =============================== */

/*
 * cpu_arm1020_icache_invalidate_range(start, end)
 *
 * invalidate a range of virtual addresses from the Icache
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_arm1020_icache_invalidate_range)
1:	mcr	p15, 0, r0, c7, c10, 4
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	add	r0, r0, #DCACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D entry
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	add	r0, r0, #DCACHELINESIZE
	cmp	r0, r1
	blo	1b
ENTRY(cpu_arm1020_icache_invalidate_page)
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate I cache
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/* ================================== TLB ================================= */

/*
 * cpu_arm1020_tlb_invalidate_all()
 *
 * Invalidate all TLB entries
 */
	.align	5
ENTRY(cpu_arm1020_tlb_invalidate_all)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I & D tlbs
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/*
 * cpu_arm1020_tlb_invalidate_range(start, end)
 *
 * invalidate TLB entries covering the specified range
 *
 * start: range start address
 * end:   range end address
 */
	.align	5
ENTRY(cpu_arm1020_tlb_invalidate_range)
	sub	r3, r1, r0
	cmp	r3, #256 * PAGESIZE
	bhi	cpu_arm1020_tlb_invalidate_all
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
1:	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	mcr	p15, 0, r0, c8, c5, 1		@ invalidate I TLB entry
	add	r0, r0, #PAGESIZE
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
#endif
	cmp	r0, r1
	blt	1b
	mov	pc, lr

/*
 * cpu_arm1020_tlb_invalidate_page(page, flags)
 *
 * invalidate the TLB entries for the specified page.
 *
 * page:  page to invalidate
 * flags: non-zero if we include the I TLB
 */
	.align	5
ENTRY(cpu_arm1020_tlb_invalidate_page)
	mov	r3, #0
	mcr	p15, 0, r3, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	teq	r1, #0
	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	mcrne	p15, 0, r0, c8, c5, 1		@ invalidate I TLB entry
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/* =============================== PageTable ============================== */

/*
 * cpu_arm1020_set_pgd(pgd)
 *
 * Set the translation base pointer to be as described by pgd.
 *
 * pgd: new page tables
 */
	.align	5
ENTRY(cpu_arm1020_set_pgd)
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r3, c7, c10, 4
	mov	r1, #0xF			@ 16 segments
1:	mov	r3, #0x3F			@ 64 entries
2:	mov	ip, r3, LSL #26 		@ shift up entry
	orr	ip, ip, r1, LSL #5		@ shift in/up index
	mcr	p15, 0, ip, c7, c14, 2		@ Clean & Inval DCache entry
	mov	ip, #0
	mcr	p15, 0, ip, c7, c10, 4
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	ip, ip
#endif
	subs	r3, r3, #1
	cmp	r3, #0
	bge	2b				@ entries 3F to 0
	subs	r1, r1, #1
	cmp	r1, #0
	bge	1b				@ segments 15 to 0

#endif
	mov	r1, #0
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate I cache
#endif
	mcr	p15, 0, r1, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, r1, c8, c7, 0		@ invalidate I & D TLBs
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	ip, ip
	mov	ip, ip
#endif
	mov	pc, lr
        
/*
 * cpu_arm1020_set_pmd(pmdp, pmd)
 *
 * Set a level 1 translation table entry, and clean it out of
 * any caches such that the MMUs can load it correctly.
 *
 * pmdp: pointer to PMD entry
 * pmd:  PMD value to store
 */
	.align	5
ENTRY(cpu_arm1020_set_pmd)
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	eor	r2, r1, #0x0a			@ C & Section
	tst	r2, #0x0b
	biceq	r1, r1, #4			@ clear bufferable bit
#endif
	str	r1, [r0]
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry  (drain is done by TLB fns)
#endif
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr

/*
 * cpu_arm1020_set_pte(ptep, pte)
 *
 * Set a PTE and flush it out
 */
	.align	5
ENTRY(cpu_arm1020_set_pte)
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

#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	eor	r3, r1, #0x0a			@ C & small page?
	tst	r3, #0x0b
	biceq	r2, r2, #4
#endif
	str	r2, [r0]			@ hardware version
	mov	r0, r0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
#endif
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	mov	r0, r0
	mov	r0, r0
#endif
	mov	pc, lr


ENTRY(cpu_arm1020_name)
	.ascii	"Arm1020"
#ifndef CONFIG_CPU_ICACHE_DISABLE
	.ascii	"i"
#endif
#ifndef CONFIG_CPU_DCACHE_DISABLE
	.ascii	"d"
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	.ascii	"(wt)"
#else
	.ascii	"(wb)"
#endif
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	.ascii	"B"
#endif
#ifdef CONFIG_CPU_CACHE_ROUND_ROBIN
	.ascii	"RR"
#endif
	.ascii	"\0"
	.align

	.section ".text.init", #alloc, #execinstr

__arm1020_setup:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7		@ invalidate I,D caches on v4
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
	mcr	p15, 0, r0, c8, c7		@ invalidate I,D TLBs on v4
	mcr	p15, 0, r4, c2, c0		@ load page table pointer
	mov	r0, #0x1f			@ Domains 0, 1 = client
	mcr	p15, 0, r0, c3, c0		@ load domain access register
	mrc	p15, 0, r0, c1, c0		@ get control register v4
/*
 * Clear out 'unwanted' bits (then put them in if we need them)
 */
	bic	r0, r0, #0x0e00 		@ ....??r.........
	bic	r0, r0, #0x0002 		@ ..............a.
	bic	r0, r0, #0x000c 		@ W,D
	bic	r0, r0, #0x1000 		@ I
/*
 * Turn on what we want
 */
	orr	r0, r0, #0x0031 		@ ..........DP...M
	orr	r0, r0, #0x0100 		@ .......S........

#ifdef CONFIG_CPU_CACHE_ROUND_ROBIN
	orr	r0, r0, #0x4000 		@ .R..............
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	orr	r0, r0, #0x0800 		@ ....Z...........
#endif
#ifndef CONFIG_CPU_DCACHE_DISABLE
	orr	r0, r0, #0x0004 		@ Enable D cache
#endif
#ifndef CONFIG_CPU_ICACHE_DISABLE
	orr	r0, r0, #0x1000 		@ I Cache on
#endif
	mov	pc, lr

	.text

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
	.type	arm1020_processor_functions, #object
arm1020_processor_functions:
	.word	cpu_arm1020_data_abort
	.word	cpu_arm1020_check_bugs
	.word	cpu_arm1020_proc_init
	.word	cpu_arm1020_proc_fin
	.word	cpu_arm1020_reset
	.word	cpu_arm1020_do_idle

	/* cache */
	.word	cpu_arm1020_cache_clean_invalidate_all
	.word	cpu_arm1020_cache_clean_invalidate_range
	.word	cpu_arm1020_flush_ram_page

	/* dcache */
	.word	cpu_arm1020_dcache_invalidate_range
	.word	cpu_arm1020_dcache_clean_range
	.word	cpu_arm1020_dcache_clean_page
	.word	cpu_arm1020_dcache_clean_entry

	/* icache */
	.word	cpu_arm1020_icache_invalidate_range
	.word	cpu_arm1020_icache_invalidate_page

	/* tlb */
	.word	cpu_arm1020_tlb_invalidate_all
	.word	cpu_arm1020_tlb_invalidate_range
	.word	cpu_arm1020_tlb_invalidate_page

	/* pgtable */
	.word	cpu_arm1020_set_pgd
	.word	cpu_arm1020_set_pmd
	.word	cpu_arm1020_set_pte
	.size	arm1020_processor_functions, . - arm1020_processor_functions

	.type	cpu_arm1020_info, #object
cpu_arm1020_info:
	.long	0
	.long	cpu_arm1020_name
	.size	cpu_arm1020_info, . - cpu_arm1020_info

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

	.type	__arm1020_proc_info,#object
__arm1020_proc_info:
	.long	0x4100a200
	.long	0xff00fff0
	.long	0x00000c02			@ mmuflags
	b	__arm1020_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB
	.long	cpu_arm1020_info
	.long	arm1020_processor_functions
	.size	__arm1020_proc_info, . - __arm1020_proc_info
