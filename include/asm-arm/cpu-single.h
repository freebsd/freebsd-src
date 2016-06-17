/*
 *  linux/include/asm-arm/cpu-single.h
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*
 * Single CPU
 */
#ifdef __STDC__
#define __cpu_fn(name,x)	cpu_##name##x
#else
#define __cpu_fn(name,x)	cpu_/**/name/**/x
#endif
#define cpu_fn(name,x)		__cpu_fn(name,x)

/*
 * If we are supporting multiple CPUs, then we must use a table of
 * function pointers for this lot.  Otherwise, we can optimise the
 * table away.
 */
#define cpu_data_abort			cpu_fn(CPU_NAME,_data_abort)
#define cpu_check_bugs			cpu_fn(CPU_NAME,_check_bugs)
#define cpu_proc_init			cpu_fn(CPU_NAME,_proc_init)
#define cpu_proc_fin			cpu_fn(CPU_NAME,_proc_fin)
#define cpu_reset			cpu_fn(CPU_NAME,_reset)
#define cpu_do_idle			cpu_fn(CPU_NAME,_do_idle)

#define cpu_cache_clean_invalidate_all	cpu_fn(CPU_NAME,_cache_clean_invalidate_all)
#define cpu_cache_clean_invalidate_range cpu_fn(CPU_NAME,_cache_clean_invalidate_range)
#define cpu_flush_ram_page		cpu_fn(CPU_NAME,_flush_ram_page)

#define cpu_dcache_invalidate_range	cpu_fn(CPU_NAME,_dcache_invalidate_range)
#define cpu_dcache_clean_range		cpu_fn(CPU_NAME,_dcache_clean_range)
#define cpu_dcache_clean_page		cpu_fn(CPU_NAME,_dcache_clean_page)
#define cpu_dcache_clean_entry		cpu_fn(CPU_NAME,_dcache_clean_entry)

#define cpu_icache_invalidate_range	cpu_fn(CPU_NAME,_icache_invalidate_range)
#define cpu_icache_invalidate_page	cpu_fn(CPU_NAME,_icache_invalidate_page)

#define cpu_tlb_invalidate_all		cpu_fn(CPU_NAME,_tlb_invalidate_all)
#define cpu_tlb_invalidate_range	cpu_fn(CPU_NAME,_tlb_invalidate_range)
#define cpu_tlb_invalidate_page		cpu_fn(CPU_NAME,_tlb_invalidate_page)

#define cpu_set_pgd			cpu_fn(CPU_NAME,_set_pgd)
#define cpu_set_pmd			cpu_fn(CPU_NAME,_set_pmd)
#define cpu_set_pte			cpu_fn(CPU_NAME,_set_pte)

#ifndef __ASSEMBLY__

#include <asm/memory.h>
#include <asm/page.h>

/* forward declare task_struct */
struct task_struct;

/* declare all the functions as extern */
extern void cpu_data_abort(unsigned long pc);
extern void cpu_check_bugs(void);
extern void cpu_proc_init(void);
extern void cpu_proc_fin(void);
extern int cpu_do_idle(void);

extern void cpu_cache_clean_invalidate_all(void);
extern void cpu_cache_clean_invalidate_range(unsigned long address, unsigned long end, int flags);
extern void cpu_flush_ram_page(void *virt_page);

extern void cpu_dcache_invalidate_range(unsigned long start, unsigned long end);
extern void cpu_dcache_clean_range(unsigned long start, unsigned long end);
extern void cpu_dcache_clean_page(void *virt_page);
extern void cpu_dcache_clean_entry(unsigned long address);

extern void cpu_icache_invalidate_range(unsigned long start, unsigned long end);
extern void cpu_icache_invalidate_page(void *virt_page);

extern void cpu_tlb_invalidate_all(void);
extern void cpu_tlb_invalidate_range(unsigned long address, unsigned long end);
extern void cpu_tlb_invalidate_page(unsigned long address, int flags);

extern void cpu_set_pgd(unsigned long pgd_phys);
extern void cpu_set_pmd(pmd_t *pmdp, pmd_t pmd);
extern void cpu_set_pte(pte_t *ptep, pte_t pte);
extern volatile void cpu_reset(unsigned long addr);

#define cpu_switch_mm(pgd,tsk) cpu_set_pgd(__virt_to_phys((unsigned long)(pgd)))

#define cpu_get_pgd()	\
	({						\
		unsigned long pg;			\
		__asm__("mrc p15, 0, %0, c2, c0, 0"	\
			 : "=r" (pg));			\
		pg &= ~0x3fff;				\
		(pgd_t *)phys_to_virt(pg);		\
	})

#endif
