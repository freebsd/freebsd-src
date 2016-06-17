#ifndef _PPC64_PGALLOC_H
#define _PPC64_PGALLOC_H

#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/naca.h>
#include <asm/paca.h>

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#define quicklists      get_paca()

#define pgd_quicklist 		(quicklists->pgd_cache)
#define pmd_quicklist 		(quicklists->pmd_cache)
#define pte_quicklist 		(quicklists->pte_cache)
#define pgtable_cache_size 	(quicklists->pgtable_cache_sz)

static inline pgd_t*
pgd_alloc_one_fast (struct mm_struct *mm)
{
	unsigned long *ret = pgd_quicklist;

	if (ret != NULL) {
		pgd_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		--pgtable_cache_size;
	} else
		ret = NULL;
	return (pgd_t *) ret;
}

static inline pgd_t*
pgd_alloc (struct mm_struct *mm)
{
	/* the VM system never calls pgd_alloc_one_fast(), so we do it here. */
	pgd_t *pgd = pgd_alloc_one_fast(mm);

	if (pgd == NULL) {
		pgd = (pgd_t *)__get_free_page(GFP_KERNEL);
		if (pgd != NULL)
			clear_page(pgd);
	}
	return pgd;
}

static inline void
pgd_free (pgd_t *pgd)
{
	*(unsigned long *)pgd = (unsigned long) pgd_quicklist;
	pgd_quicklist = (unsigned long *) pgd;
	++pgtable_cache_size;
}

#define pgd_populate(MM, PGD, PMD)	pgd_set(PGD, PMD)

static inline pmd_t*
pmd_alloc_one_fast (struct mm_struct *mm, unsigned long addr)
{
	unsigned long *ret = (unsigned long *)pmd_quicklist;

	if (ret != NULL) {
		pmd_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		--pgtable_cache_size;
	}
	return (pmd_t *)ret;
}

static inline pmd_t*
pmd_alloc_one (struct mm_struct *mm, unsigned long addr)
{
	pmd_t *pmd = (pmd_t *) __get_free_page(GFP_KERNEL);

	if (pmd != NULL)
		clear_page(pmd);
	return pmd;
}

static inline void
pmd_free (pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long) pmd_quicklist;
	pmd_quicklist = (unsigned long *) pmd;
	++pgtable_cache_size;
}

#define pmd_populate(MM, PMD, PTE)	pmd_set(PMD, PTE)

static inline pte_t*
pte_alloc_one_fast (struct mm_struct *mm, unsigned long addr)
{
	unsigned long *ret = (unsigned long *)pte_quicklist;

	if (ret != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		--pgtable_cache_size;
	}
	return (pte_t *)ret;
}


static inline pte_t*
pte_alloc_one (struct mm_struct *mm, unsigned long addr)
{
	pte_t *pte = (pte_t *) __get_free_page(GFP_KERNEL);

	if (pte != NULL)
		clear_page(pte);
	return pte;
}

static inline void
pte_free (pte_t *pte)
{
	*(unsigned long *)pte = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pte;
	++pgtable_cache_size;
}

extern int do_check_pgt_cache(int, int);

#endif /* _PPC64_PGALLOC_H */
