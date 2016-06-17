/* $Id: cache-sh4.c,v 1.1.1.1.2.8 2003/07/09 09:59:30 trent Exp $
 *
 *  linux/arch/sh/mm/cache-sh4.c
 *
 * Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>

#define CCR		 0xff00001c	/* Address of Cache Control Register */

#define CCR_CACHE_OCE	0x0001	/* Operand Cache Enable */
#define CCR_CACHE_WT	0x0002	/* Write-Through (for P0,U0,P3) (else writeback)*/
#define CCR_CACHE_CB	0x0004	/* Copy-Back (for P1) (else writethrough) */
#define CCR_CACHE_OCI	0x0008	/* OC Invalidate */
#define CCR_CACHE_ORA	0x0020	/* OC RAM Mode */
#define CCR_CACHE_OIX	0x0080	/* OC Index Enable */
#define CCR_CACHE_ICE	0x0100	/* Instruction Cache Enable */
#define CCR_CACHE_ICI	0x0800	/* IC Invalidate */
#define CCR_CACHE_IIX	0x8000	/* IC Index Enable */



#if defined(CONFIG_SH_CACHE_ASSOC)
#define CCR_CACHE_EMODE 0x80000000
/* CCR setup for associative mode: 16k+32k 2-way, P1 copy-back, enable */
#define CCR_CACHE_VAL	(CCR_CACHE_EMODE|CCR_CACHE_ENABLE|CCR_CACHE_CB)
#else
/* Default CCR setup: 8k+16k-byte cache, P1-copy-back, enable */
#define CCR_CACHE_VAL	(CCR_CACHE_ENABLE|CCR_CACHE_CB)
#endif

#define CCR_CACHE_INIT	(CCR_CACHE_VAL|CCR_CACHE_OCI|CCR_CACHE_ICI)
#define CCR_CACHE_ENABLE (CCR_CACHE_OCE|CCR_CACHE_ICE)

#define CACHE_IC_ADDRESS_ARRAY 0xf0000000
#define CACHE_OC_ADDRESS_ARRAY 0xf4000000
#define CACHE_VALID	  1
#define CACHE_UPDATED	  2
#define CACHE_ASSOC	  8

#define CACHE_OC_WAY_SHIFT       14
#define CACHE_IC_WAY_SHIFT       13
#define CACHE_OC_ENTRY_SHIFT      5
#define CACHE_IC_ENTRY_SHIFT      5
#define CACHE_OC_ENTRY_MASK		0x3fe0
#define CACHE_OC_ENTRY_PHYS_MASK	0x0fe0
#define CACHE_IC_ENTRY_MASK		0x1fe0
#define CACHE_IC_NUM_ENTRIES	256
#define CACHE_OC_NUM_ENTRIES	512

#define CACHE_NUM_WAYS 2

static void __init
detect_cpu_and_cache_system(void)
{
#ifdef CONFIG_CPU_SUBTYPE_ST40
	cpu_data->type = CPU_ST40;
#elif defined(CONFIG_CPU_SUBTYPE_SH7750) || defined(CONFIG_CPU_SUBTYPE_SH7751)
	cpu_data->type = CPU_SH7750;
#elif defined(CONFIG_CPU_SUBTYPE_SH4_202)
	cpu_data->type = CPU_SH4202;
#else
#error Unknown SH4 CPU type
#endif
}

void __init cache_init(void)
{
	unsigned long ccr;

	detect_cpu_and_cache_system();

	jump_to_P2();
	ccr = ctrl_inl(CCR);
	if (ccr & CCR_CACHE_ENABLE) {
		/*
		 * XXX: Should check RA here. 
		 * If RA was 1, we only need to flush the half of the caches.
		 */
		unsigned long addr, data;

#if defined(CONFIG_SH_CACHE_ASSOC)
                unsigned long way;

                for (way = 0; way <= CACHE_NUM_WAYS; ++way) {
                        unsigned long waybit = way << CACHE_OC_WAY_SHIFT;

		        for (addr = CACHE_OC_ADDRESS_ARRAY + waybit;
		             addr < (CACHE_OC_ADDRESS_ARRAY + waybit +
			             (CACHE_OC_NUM_ENTRIES << 
                                      CACHE_OC_ENTRY_SHIFT));
		             addr += (1 << CACHE_OC_ENTRY_SHIFT)) {

			        data = ctrl_inl(addr);

			        if ((data & (CACHE_UPDATED|CACHE_VALID))
			            == (CACHE_UPDATED|CACHE_VALID))
				        ctrl_outl(data & ~CACHE_UPDATED, addr);
		        }
                }
#else
		for (addr = CACHE_OC_ADDRESS_ARRAY;
		     addr < (CACHE_OC_ADDRESS_ARRAY+
			     (CACHE_OC_NUM_ENTRIES << CACHE_OC_ENTRY_SHIFT));
		     addr += (1 << CACHE_OC_ENTRY_SHIFT)) {
			data = ctrl_inl(addr);
			if ((data & (CACHE_UPDATED|CACHE_VALID))
			    == (CACHE_UPDATED|CACHE_VALID))
				ctrl_outl(data & ~CACHE_UPDATED, addr);
		}
#endif
	}

	ctrl_outl(CCR_CACHE_INIT, CCR);
	back_to_P1();
}

/*
 * SH-4 has virtually indexed and physically tagged cache.
 */

static struct semaphore p3map_sem[4];

void __init p3_cache_init(void)
{
	/* In ioremap.c */
	extern int remap_area_pages(unsigned long address,
				    unsigned long phys_addr,
				    unsigned long size, unsigned long flags);

	if (remap_area_pages(P3SEG, 0, PAGE_SIZE*4, _PAGE_CACHABLE))
		panic("%s failed.", __FUNCTION__);
	sema_init (&p3map_sem[0], 1);
	sema_init (&p3map_sem[1], 1);
	sema_init (&p3map_sem[2], 1);
	sema_init (&p3map_sem[3], 1);
}

/*
 * Write back the dirty D-caches, but not invalidate them.
 *
 * START: Virtual Address (U0, P1, or P3)
 * SIZE: Size of the region.
 */
void __flush_wback_region(void *start, int size)
{
	unsigned long v;
	unsigned long begin, end;

	begin = (unsigned long)start & ~(L1_CACHE_BYTES-1);
	end = ((unsigned long)start + size + L1_CACHE_BYTES-1)
		& ~(L1_CACHE_BYTES-1);
	for (v = begin; v < end; v+=L1_CACHE_BYTES) {
		asm volatile("ocbwb	%0"
			     : /* no output */
			     : "m" (__m(v)));
	}
}

/*
 * Write back the dirty D-caches and invalidate them.
 *
 * START: Virtual Address (U0, P1, or P3)
 * SIZE: Size of the region.
 */
void __flush_purge_region(void *start, int size)
{
	unsigned long v;
	unsigned long begin, end;

	begin = (unsigned long)start & ~(L1_CACHE_BYTES-1);
	end = ((unsigned long)start + size + L1_CACHE_BYTES-1)
		& ~(L1_CACHE_BYTES-1);
	for (v = begin; v < end; v+=L1_CACHE_BYTES) {
		asm volatile("ocbp	%0"
			     : /* no output */
			     : "m" (__m(v)));
	}
}


/*
 * No write back please
 */
void __flush_invalidate_region(void *start, int size)
{
	unsigned long v;
	unsigned long begin, end;

	begin = (unsigned long)start & ~(L1_CACHE_BYTES-1);
	end = ((unsigned long)start + size + L1_CACHE_BYTES-1)
		& ~(L1_CACHE_BYTES-1);
	for (v = begin; v < end; v+=L1_CACHE_BYTES) {
		asm volatile("ocbi	%0"
			     : /* no output */
			     : "m" (__m(v)));
	}
}

void __flush_icache_all(void)
{
	unsigned long flags;

	save_and_cli(flags);
	jump_to_P2();
	ctrl_outl(CCR_CACHE_VAL|CCR_CACHE_ICI, CCR);
	back_to_P1();
	restore_flags(flags);
}

/*
 * Write back the range of D-cache, and purge the I-cache.
 *
 * Called from kernel/module.c:sys_init_module and routine for a.out format.
 */
void flush_icache_range(unsigned long start, unsigned long end)
{
	flush_cache_all();
}

/*
 * Write back the D-cache and purge the I-cache for signal trampoline. 
 */
void flush_cache_sigtramp(unsigned long addr)
{
	unsigned long v, index;
	unsigned long flags; 

	v = addr & ~(L1_CACHE_BYTES-1);
	asm volatile("ocbwb	%0"
		     : /* no output */
		     : "m" (__m(v)));

	index = CACHE_IC_ADDRESS_ARRAY| (v&CACHE_IC_ENTRY_MASK);
	save_and_cli(flags);
	jump_to_P2();
	ctrl_outl(0, index);	/* Clear out Valid-bit */

#if defined(CONFIG_SH_CACHE_ASSOC)
	/* Must invalidate both ways for associative cache */
	ctrl_outl(0, index | (1 << CACHE_IC_WAY_SHIFT));
#endif

	back_to_P1();
	restore_flags(flags);
}

static inline void flush_cache_4096(unsigned long start,
				    unsigned long phys)
{
	unsigned long flags; 
	extern void __flush_cache_4096(unsigned long addr, unsigned long phys, unsigned long exec_offset);

#if defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_ST40) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
	if (start >= CACHE_OC_ADDRESS_ARRAY) {
		/*
		 * SH7751 and ST40 have no restriction to handle cache.
		 * (While SH7750 must do that at P2 area.)
		 */
		__flush_cache_4096(start | CACHE_ASSOC, phys | 0x80000000, 0);
	} else
#endif
	{
		save_and_cli(flags);
		__flush_cache_4096(start | CACHE_ASSOC, phys | 0x80000000, 0x20000000);
		restore_flags(flags);
	}
}

/*
 * Write back & invalidate the D-cache of the page.
 * (To avoid "alias" issues)
 */
void flush_dcache_page(struct page *page)
{
	if (test_bit(PG_mapped, &page->flags)) {
		unsigned long phys = PHYSADDR(page_address(page));

		/* Loop all the D-cache */
		flush_cache_4096(CACHE_OC_ADDRESS_ARRAY,          phys);
		flush_cache_4096(CACHE_OC_ADDRESS_ARRAY | 0x1000, phys);
		flush_cache_4096(CACHE_OC_ADDRESS_ARRAY | 0x2000, phys);
		flush_cache_4096(CACHE_OC_ADDRESS_ARRAY | 0x3000, phys);
	}
}

static inline void flush_icache_all(void)
{
	unsigned long flags;

	save_and_cli(flags);
	jump_to_P2();
	/* Flush I-cache */
	ctrl_outl(CCR_CACHE_VAL|CCR_CACHE_ICI, CCR);
	back_to_P1();
	restore_flags(flags);
}

void flush_cache_all(void)
{
	extern void __flush_dcache_all(void);

	__flush_dcache_all();
	flush_icache_all();
}

void flush_cache_mm(struct mm_struct *mm)
{
	/* Is there any good way? */
	/* XXX: possibly call flush_cache_range for each vm area */
	/* 
	 * FIXME: Really, the optimal solution here would be able to flush out
	 * individual lines created by the specified context, but this isn't
	 * feasible for a number of architectures (such as MIPS, and some
	 * SPARC) .. is this possible for SuperH?
	 *
	 * In the meantime, we'll just flush all of the caches.. this
	 * seems to be the simplest way to avoid at least a few wasted
	 * cache flushes. -Lethal
	 */
	flush_cache_all();
}

static void __flush_cache_page(struct vm_area_struct *vma,
			       unsigned long address,
			       unsigned long phys)
{
	/* We only need to flush D-cache when we have alias */
	if ((address^phys) & CACHE_ALIAS) {
		/* Loop 4K of the D-cache */
		flush_cache_4096(
			CACHE_OC_ADDRESS_ARRAY | (address & CACHE_ALIAS),
			phys);
		/* Loop another 4K of the D-cache */
		flush_cache_4096(
			CACHE_OC_ADDRESS_ARRAY | (phys & CACHE_ALIAS),
			phys);
	}

	if (vma->vm_flags & VM_EXEC)
		/* Loop 4K (half) of the I-cache */
		flush_cache_4096(
			CACHE_IC_ADDRESS_ARRAY | (address & 0x1000),
			phys);
}

/*
 * Write back and invalidate D-caches.
 *
 * START, END: Virtual Address (U0 address)
 *
 * NOTE: We need to flush the _physical_ page entry.
 * Flushing the cache lines for U0 only isn't enough.
 * We need to flush for P1 too, which may contain aliases.
 */
void flush_cache_range(struct mm_struct *mm, unsigned long start,
		       unsigned long end)
{
	extern void flush_cache_4096_all(unsigned long start);

	unsigned long p = start & PAGE_MASK;
	pgd_t *dir;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	unsigned long phys;
	unsigned long d = 0;

	dir = pgd_offset(mm, p);
	pmd = pmd_offset(dir, p);

	do {
		if (pmd_none(*pmd) || pmd_bad(*pmd)) {
			p &= ~((1 << PMD_SHIFT) -1);
			p += (1 << PMD_SHIFT);
			pmd++;
			continue;
		}
		pte = pte_offset(pmd, p);
		do {
			entry = *pte;
			if ((pte_val(entry) & _PAGE_PRESENT)) {
				phys = pte_val(entry)&PTE_PHYS_MASK;
				if ((p^phys) & CACHE_ALIAS) {
					d |= 1 << ((p & CACHE_ALIAS)>>12); 
					d |= 1 << ((phys & CACHE_ALIAS)>>12);
					if (d == 0x0f)
						goto loop_exit;
				}
			}
			pte++;
			p += PAGE_SIZE;
		} while (p < end && (unsigned long)pte & PAGE_MASK);
		pmd++;
	} while (p < end);
 loop_exit:
	if (d & 1)
		flush_cache_4096_all(0);
	if (d & 2)
		flush_cache_4096_all(0x1000);
	if (d & 4)
		flush_cache_4096_all(0x2000);
	if (d & 8)
		flush_cache_4096_all(0x3000);
	flush_icache_all();
}

/*
 * Write back and invalidate I/D-caches for the page.
 *
 * ADDR: Virtual Address (U0 address)
 */
void flush_cache_page(struct vm_area_struct *vma, unsigned long address)
{
	pgd_t *dir;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	unsigned long phys;

	dir = pgd_offset(vma->vm_mm, address);
	pmd = pmd_offset(dir, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return;
	pte = pte_offset(pmd, address);
	entry = *pte;
	if (!(pte_val(entry) & _PAGE_PRESENT))
		return;

	phys = pte_val(entry)&PTE_PHYS_MASK;
	__flush_cache_page(vma, address, phys);
}

/*
 * clear_user_page
 * @to: P1 address
 * @address: U0 address to be mapped
 */
void clear_user_page(void *to, unsigned long address)
{
	struct page *page = virt_to_page(to);

	__set_bit(PG_mapped, &page->flags);
	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		clear_page(to);
	else {
		pgprot_t pgprot = __pgprot(_PAGE_PRESENT | 
					   _PAGE_RW | _PAGE_CACHABLE |
					   _PAGE_DIRTY | _PAGE_ACCESSED | 
					   _PAGE_HW_SHARED | _PAGE_FLAGS_HARD);
		unsigned long phys_addr = PHYSADDR(to);
		unsigned long p3_addr = P3SEG + (address & CACHE_ALIAS);
		pgd_t *dir = pgd_offset_k(p3_addr);
		pmd_t *pmd = pmd_offset(dir, p3_addr);
		pte_t *pte = pte_offset(pmd, p3_addr);
		pte_t entry;
		unsigned long flags;

		entry = mk_pte_phys(phys_addr, pgprot);
		down(&p3map_sem[(address & CACHE_ALIAS)>>12]);
		set_pte(pte, entry);
		save_and_cli(flags);
		__flush_tlb_page(get_asid(), p3_addr);
		restore_flags(flags);
		update_mmu_cache(NULL, p3_addr, entry);
		__clear_user_page((void *)p3_addr, to);
		pte_clear(pte);
		up(&p3map_sem[(address & CACHE_ALIAS)>>12]);
	}
}

/*
 * copy_user_page
 * @to: P1 address
 * @from: P1 address
 * @address: U0 address to be mapped
 */
void copy_user_page(void *to, void *from, unsigned long address)
{
	struct page *page = virt_to_page(to);

	__set_bit(PG_mapped, &page->flags);
	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		copy_page(to, from);
	else {
		pgprot_t pgprot = __pgprot(_PAGE_PRESENT | 
					   _PAGE_RW | _PAGE_CACHABLE |
					   _PAGE_DIRTY | _PAGE_ACCESSED | 
					   _PAGE_HW_SHARED | _PAGE_FLAGS_HARD);
		unsigned long phys_addr = PHYSADDR(to);
		unsigned long p3_addr = P3SEG + (address & CACHE_ALIAS);
		pgd_t *dir = pgd_offset_k(p3_addr);
		pmd_t *pmd = pmd_offset(dir, p3_addr);
		pte_t *pte = pte_offset(pmd, p3_addr);
		pte_t entry;
		unsigned long flags;

		entry = mk_pte_phys(phys_addr, pgprot);
		down(&p3map_sem[(address & CACHE_ALIAS)>>12]);
		set_pte(pte, entry);
		save_and_cli(flags);
		__flush_tlb_page(get_asid(), p3_addr);
		restore_flags(flags);
		update_mmu_cache(NULL, p3_addr, entry);
		__copy_user_page((void *)p3_addr, from, to);
		pte_clear(pte);
		up(&p3map_sem[(address & CACHE_ALIAS)>>12]);
	}
}


/****************************************************************************/

#if defined(CONFIG_SH_CACHE_ASSOC)
/*
 * It is no possible to use the approach implement in clear_page.S when we 
 * are in 2-way set associative mode as it would only clear half the cache, in 
 * general. For the moment we simply implement it as a iteration through the 
 * cache flushing both ways, this in itself is not optimial as the delay latency 
 * for interupts is probably longer than necessary!
 *
 * benedict.gaster.superh.com
 */
void __flush_dcache_all(void)
{
	unsigned long flags;
	unsigned long addr;
        unsigned long way;

	save_and_cli(flags);
#if !defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
	jump_to_P2();
#endif
        /* Clear the U and V bits for each line and each way. On SH-4, this
         * causes write-back if both U and V are set before the address write.
         */
	for (way = 0; way <= 1; ++way) {
	        unsigned long waybit = way << CACHE_OC_WAY_SHIFT;

	        /* Loop all the D-cache */
                for (addr = CACHE_OC_ADDRESS_ARRAY + waybit;
	             addr < (CACHE_OC_ADDRESS_ARRAY + waybit
		             + (CACHE_OC_NUM_ENTRIES << CACHE_OC_ENTRY_SHIFT));
	             addr += (1 << CACHE_OC_ENTRY_SHIFT)) {
			ctrl_outl(0, addr);
                }
	}

#if !defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
	back_to_P1();
#endif 
	restore_flags(flags);
}

void flush_cache_4096_all(unsigned long start)
{
  unsigned long phys = PHYSADDR(start);

  /* Loop all the D-cache */
  flush_cache_4096(CACHE_OC_ADDRESS_ARRAY,          phys);
}
#endif






/****************************************************************************/

#if defined(CONFIG_SH_CACHE_ASSOC)
/*
 * It is no possible to use the approach implement in clear_page.S when we 
 * are in 2-way set associative mode as it would only clear half the cache, in 
 * general. For the moment we simply implement it as a iteration through the 
 * cache flushing both ways, this in itself is not optimial as the delay latency 
 * for interupts is probably longer than necessary!
 *
 * benedict.gaster.superh.com
 */
void __flush_dcache_all(void)
{
	unsigned long flags;
	unsigned long addr;
        unsigned long way;

	save_and_cli(flags);
#if !defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
	jump_to_P2();
#endif
        /* Clear the U and V bits for each line and each way. On SH-4, this
         * causes write-back if both U and V are set before the address write.
         */
	for (way = 0; way <= 1; ++way) {
	        unsigned long waybit = way << CACHE_OC_WAY_SHIFT;

	        /* Loop all the D-cache */
                for (addr = CACHE_OC_ADDRESS_ARRAY + waybit;
	             addr < (CACHE_OC_ADDRESS_ARRAY + waybit
		             + (CACHE_OC_NUM_ENTRIES << CACHE_OC_ENTRY_SHIFT));
	             addr += (1 << CACHE_OC_ENTRY_SHIFT)) {
			ctrl_outl(0, addr);
                }
	}

#if !defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
	back_to_P1();
#endif 
	restore_flags(flags);
}

void flush_cache_4096_all(unsigned long start)
{
  unsigned long phys = PHYSADDR(start);

  /* Loop all the D-cache */
  flush_cache_4096(CACHE_OC_ADDRESS_ARRAY,          phys);
}
#endif





