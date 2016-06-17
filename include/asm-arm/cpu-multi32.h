/*
 *  linux/include/asm-arm/cpu-multi32.h
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASSEMBLY__

#include <asm/memory.h>
#include <asm/page.h>

/* forward-declare task_struct */
struct task_struct;

/*
 * Don't change this structure - ASM code
 * relies on it.
 */
extern struct processor {
	/* MISC
	 * get data abort address/flags
	 */
	void (*_data_abort)(unsigned long pc);
	/*
	 * check for any bugs
	 */
	void (*_check_bugs)(void);
	/*
	 * Set up any processor specifics
	 */
	void (*_proc_init)(void);
	/*
	 * Disable any processor specifics
	 */
	void (*_proc_fin)(void);
	/*
	 * Special stuff for a reset
	 */
	volatile void (*reset)(unsigned long addr);
	/*
	 * Idle the processor
	 */
	int (*_do_idle)(void);
	/*
	 * Processor architecture specific
	 */
	struct {	/* CACHE */
		/*
		 * flush all caches
		 */
		void (*clean_invalidate_all)(void);
		/*
		 * flush a specific page or pages
		 */
		void (*clean_invalidate_range)(unsigned long address, unsigned long end, int flags);
		/*
		 * flush a page to RAM
		 */
		void (*_flush_ram_page)(void *virt_page);
	} cache;

	struct {	/* D-cache */
		/*
		 * invalidate the specified data range
		 */
		void (*invalidate_range)(unsigned long start, unsigned long end);
		/*
		 * clean specified data range
		 */
		void (*clean_range)(unsigned long start, unsigned long end);
		/*
		 * obsolete flush cache entry
		 */
		void (*clean_page)(void *virt_page);
		/*
		 * clean a virtual address range from the
		 * D-cache without flushing the cache.
		 */
		void (*clean_entry)(unsigned long start);
	} dcache;

	struct {	/* I-cache */
		/*
		 * invalidate the I-cache for the specified range
		 */
		void (*invalidate_range)(unsigned long start, unsigned long end);
		/*
		 * invalidate the I-cache for the specified virtual page
		 */
		void (*invalidate_page)(void *virt_page);
	} icache;

	struct {	/* TLB */
		/*
		 * flush all TLBs
		 */
		void (*invalidate_all)(void);
		/*
		 * flush a specific TLB
		 */
		void (*invalidate_range)(unsigned long address, unsigned long end);
		/*
		 * flush a specific TLB
		 */
		void (*invalidate_page)(unsigned long address, int flags);
	} tlb;

	struct {	/* PageTable */
		/*
		 * Set the page table
		 */
		void (*set_pgd)(unsigned long pgd_phys);
		/*
		 * Set a PMD (handling IMP bit 4)
		 */
		void (*set_pmd)(pmd_t *pmdp, pmd_t pmd);
		/*
		 * Set a PTE
		 */
		void (*set_pte)(pte_t *ptep, pte_t pte);
	} pgtable;
} processor;

extern const struct processor arm6_processor_functions;
extern const struct processor arm7_processor_functions;
extern const struct processor sa110_processor_functions;

#define cpu_data_abort(pc)			processor._data_abort(pc)
#define cpu_check_bugs()			processor._check_bugs()
#define cpu_proc_init()				processor._proc_init()
#define cpu_proc_fin()				processor._proc_fin()
#define cpu_reset(addr)				processor.reset(addr)
#define cpu_do_idle()				processor._do_idle()

#define cpu_cache_clean_invalidate_all()	processor.cache.clean_invalidate_all()
#define cpu_cache_clean_invalidate_range(s,e,f)	processor.cache.clean_invalidate_range(s,e,f)
#define cpu_flush_ram_page(vp)			processor.cache._flush_ram_page(vp)

#define cpu_dcache_clean_page(vp)		processor.dcache.clean_page(vp)
#define cpu_dcache_clean_entry(addr)		processor.dcache.clean_entry(addr)
#define cpu_dcache_clean_range(s,e)		processor.dcache.clean_range(s,e)
#define cpu_dcache_invalidate_range(s,e)	processor.dcache.invalidate_range(s,e)

#define cpu_icache_invalidate_range(s,e)	processor.icache.invalidate_range(s,e)
#define cpu_icache_invalidate_page(vp)		processor.icache.invalidate_page(vp)

#define cpu_tlb_invalidate_all()		processor.tlb.invalidate_all()
#define cpu_tlb_invalidate_range(s,e)		processor.tlb.invalidate_range(s,e)
#define cpu_tlb_invalidate_page(vp,f)		processor.tlb.invalidate_page(vp,f)

#define cpu_set_pgd(pgd)			processor.pgtable.set_pgd(pgd)
#define cpu_set_pmd(pmdp, pmd)			processor.pgtable.set_pmd(pmdp, pmd)
#define cpu_set_pte(ptep, pte)			processor.pgtable.set_pte(ptep, pte)

#define cpu_switch_mm(pgd,tsk)			cpu_set_pgd(__virt_to_phys((unsigned long)(pgd)))

#define cpu_get_pgd()	\
	({						\
		unsigned long pg;			\
		__asm__("mrc p15, 0, %0, c2, c0, 0"	\
			 : "=r" (pg));			\
		pg &= ~0x3fff;				\
		(pgd_t *)phys_to_virt(pg);		\
	})

#endif
