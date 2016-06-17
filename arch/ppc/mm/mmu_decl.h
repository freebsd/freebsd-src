/*
 * Declarations of procedures and variables shared between files
 * in arch/ppc/mm/.
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <asm/pgtable.h>
#include <asm/mmu.h>

extern void mapin_ram(void);
extern void bat_mapin_ram(unsigned long bat2, unsigned long bat3);
extern void adjust_total_lowmem(void);
extern int map_page(unsigned long va, phys_addr_t pa, int flags);
extern void setbat(int index, unsigned long virt, unsigned long phys,
		   unsigned int size, int flags);
extern void reserve_phys_mem(unsigned long start, unsigned long size);

extern int __map_without_bats;
extern unsigned long ioremap_base;
extern unsigned long ioremap_bot;
extern unsigned int rtas_data, rtas_size;

extern unsigned long total_memory;
extern unsigned long total_lowmem;
extern int mem_init_done;

/* ...and now those things that may be slightly different between processor
 * architectures.  -- Dan
 */
#if defined(CONFIG_8xx)
#define flush_HPTE(X, va, pg)	_tlbie(va)
#define MMU_init_hw()		do { } while(0)

#elif defined(CONFIG_4xx)
#define flush_HPTE(X, va, pg)	_tlbie(va)
extern void MMU_init_hw(void);

#else
/* anything except 4xx or 8xx */
extern void MMU_init_hw(void);

/* Be careful....this needs to be updated if we ever encounter 603 SMPs,
 * which includes all new 82xx processors.  We need tlbie/tlbsync here
 * in that case (I think). -- Dan.
 */
static inline void flush_HPTE(unsigned context, unsigned long va, pte_t *pg)
{
	if ((Hash != 0) &&
	    (cur_cpu_spec[0]->cpu_features & CPU_FTR_HPTE_TABLE))
		flush_hash_page(0, va, pg);
	else
		_tlbie(va);
}
#endif
