/*
 *  linux/arch/arm/mm/mm-armo.c
 *
 *  Copyright (C) 1998-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Page table sludge for older ARM processor architectures.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/page.h>
#include <asm/arch/memory.h>

#include <asm/mach/map.h>

#define MEMC_TABLE_SIZE (256*sizeof(unsigned long))

kmem_cache_t *pte_cache, *pgd_cache;
int page_nr;

/*
 * Allocate a page table.  Note that we place the MEMC
 * table before the page directory.  This means we can
 * easily get to both tightly-associated data structures
 * with a single pointer.
 */
static inline pgd_t *alloc_pgd_table(int priority)
{
	void *pg2k = kmem_cache_alloc(pgd_cache, GFP_KERNEL);

	if (pg2k)
		pg2k += MEMC_TABLE_SIZE;

	return (pgd_t *)pg2k;
}

void free_pgd_slow(pgd_t *pgd)
{
	unsigned long tbl = (unsigned long)pgd;

	/*
	 * CHECKME: are we leaking pte tables here???
	 */

	tbl -= MEMC_TABLE_SIZE;

	kmem_cache_free(pgd_cache, (void *)tbl);
}

pgd_t *get_pgd_slow(struct mm_struct *mm)
{
	pgd_t *new_pgd, *init_pgd;
	pmd_t *new_pmd, *init_pmd;
	pte_t *new_pte, *init_pte;

	new_pgd = alloc_pgd_table(GFP_KERNEL);
	if (!new_pgd)
		goto no_pgd;

	/*
	 * This lock is here just to satisfy pmd_alloc and pte_lock
	 */
	spin_lock(&mm->page_table_lock);

	/*
	 * On ARM, first page must always be allocated since it contains
	 * the machine vectors.
	 */
	new_pmd = pmd_alloc(mm, new_pgd, 0);
	if (!new_pmd)
		goto no_pmd;

	new_pte = pte_alloc(mm, new_pmd, 0);
	if (!new_pte)
		goto no_pte;

	init_pgd = pgd_offset_k(0);
	init_pmd = pmd_offset(init_pgd, 0);
	init_pte = pte_offset(init_pmd, 0);

	set_pte(new_pte, *init_pte);

	/*
	 * most of the page table entries are zeroed
	 * wne the table is created.
	 */
	memcpy(new_pgd + USER_PTRS_PER_PGD, init_pgd + USER_PTRS_PER_PGD,
		(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	spin_unlock(&mm->page_table_lock);

	/* update MEMC tables */
	cpu_memc_update_all(new_pgd);
	return new_pgd;

no_pte:
	spin_unlock(&mm->page_table_lock);
	pmd_free(new_pmd);
	check_pgt_cache();
	free_pgd_slow(new_pgd);
	return NULL;

no_pmd:
	spin_unlock(&mm->page_table_lock);
	free_pgd_slow(new_pgd);
	return NULL;

no_pgd:
	return NULL;
}

/*
 * No special code is required here.
 */
void setup_mm_for_reboot(char mode)
{
}

/*
 * This contains the code to setup the memory map on an ARM2/ARM250/ARM3
 * machine. This is both processor & architecture specific, and requires
 * some more work to get it to fit into our separate processor and
 * architecture structure.
 */
void __init memtable_init(struct meminfo *mi)
{
	pte_t *pte;
	int i;

	page_nr = max_low_pfn;

	pte = alloc_bootmem_low_pages(PTRS_PER_PTE * sizeof(pte_t));
	pte[0] = mk_pte_phys(PAGE_OFFSET + 491520, PAGE_READONLY);
	pmd_populate(&init_mm, pmd_offset(swapper_pg_dir, 0), pte);

	for (i = 1; i < PTRS_PER_PGD; i++)
		pgd_val(swapper_pg_dir[i]) = 0;
}

void __init iotable_init(struct map_desc *io_desc)
{
	/* nothing to do */
}

/*
 * We never have holes in the memmap
 */
void __init create_memmap_holes(struct meminfo *mi)
{
}

static void pte_cache_ctor(void *pte, kmem_cache_t *cache, unsigned long flags)
{
	memzero(pte, sizeof(pte_t) * PTRS_PER_PTE);
}

static void pgd_cache_ctor(void *pte, kmem_cache_t *cache, unsigned long flags)
{
	pgd_t *pgd = (pte + MEMC_TABLE_SIZE);

	memzero(pgd, USER_PTRS_PER_PGD * sizeof(pgd_t));
}

void __init pgtable_cache_init(void)
{
	pte_cache = kmem_cache_create("pte-cache",
				sizeof(pte_t) * PTRS_PER_PTE,
				0, 0, pte_cache_ctor, NULL);
	if (!pte_cache)
		BUG();

	pgd_cache = kmem_cache_create("pgd-cache", MEMC_TABLE_SIZE +
				sizeof(pgd_t) * PTRS_PER_PGD,
				0, 0, pgd_cache_ctor, NULL);
	if (!pgd_cache)
		BUG();
}
