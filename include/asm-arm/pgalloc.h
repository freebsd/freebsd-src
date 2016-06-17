/*
 *  linux/include/asm-arm/pgalloc.h
 *
 *  Copyright (C) 2000-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_PGALLOC_H
#define _ASMARM_PGALLOC_H

#include <linux/config.h>

#include <asm/processor.h>

/*
 * Get the cache handling stuff now.
 */
#include <asm/proc/cache.h>

/*
 * ARM processors do not cache TLB tables in RAM.
 */
#define flush_tlb_pgtables(mm,start,end)	do { } while (0)

/*
 * Processor specific parts...
 */
#include <asm/proc/pgalloc.h>

/*
 * Page table cache stuff
 */
#ifndef CONFIG_NO_PGT_CACHE

#ifdef CONFIG_SMP
#error Pgtable caches have to be per-CPU, so that no locking is needed.
#endif	/* CONFIG_SMP */

extern struct pgtable_cache_struct {
	unsigned long *pgd_cache;
	unsigned long *pte_cache;
	unsigned long pgtable_cache_sz;
} quicklists;

#define pgd_quicklist		(quicklists.pgd_cache)
#define pmd_quicklist		((unsigned long *)0)
#define pte_quicklist		(quicklists.pte_cache)
#define pgtable_cache_size	(quicklists.pgtable_cache_sz)

/* used for quicklists */
#define __pgd_next(pgd) (((unsigned long *)pgd)[1])
#define __pte_next(pte)	(((unsigned long *)pte)[0])

static inline pgd_t *get_pgd_fast(void)
{
	unsigned long *ret;

	if ((ret = pgd_quicklist) != NULL) {
		pgd_quicklist = (unsigned long *)__pgd_next(ret);
		ret[1] = ret[2];
		clean_dcache_entry(ret + 1);
		pgtable_cache_size--;
	}
	return (pgd_t *)ret;
}

static inline void free_pgd_fast(pgd_t *pgd)
{
	__pgd_next(pgd) = (unsigned long) pgd_quicklist;
	pgd_quicklist = (unsigned long *) pgd;
	pgtable_cache_size++;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if((ret = pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)__pte_next(ret);
		ret[0] = 0;
		clean_dcache_entry(ret);
		pgtable_cache_size--;
	}
	return (pte_t *)ret;
}

static inline void free_pte_fast(pte_t *pte)
{
	__pte_next(pte) = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pte;
	pgtable_cache_size++;
}

#else	/* CONFIG_NO_PGT_CACHE */

#define pgd_quicklist			((unsigned long *)0)
#define pmd_quicklist			((unsigned long *)0)
#define pte_quicklist			((unsigned long *)0)

#define get_pgd_fast()			((pgd_t *)0)
#define pte_alloc_one_fast(mm,addr)	((pte_t *)0)

#define free_pgd_fast(pgd)		free_pgd_slow(pgd)
#define free_pte_fast(pte)		pte_free_slow(pte)

#endif	/* CONFIG_NO_PGT_CACHE */

#define pte_free(pte)			free_pte_fast(pte)


/*
 * Since we have only two-level page tables, these are trivial
 */
#define pmd_alloc_one_fast(mm,addr)	({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm,addr)		({ BUG(); ((pmd_t *)2); })
#define pmd_free_slow(pmd)		do { } while (0)
#define pmd_free_fast(pmd)		do { } while (0)
#define pmd_free(pmd)			do { } while (0)
#define pgd_populate(mm,pmd,pte)	BUG()

extern pgd_t *get_pgd_slow(struct mm_struct *mm);
extern void free_pgd_slow(pgd_t *pgd);

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;

	pgd = get_pgd_fast();
	if (!pgd)
		pgd = get_pgd_slow(mm);

	return pgd;
}

#define pgd_free(pgd)			free_pgd_fast(pgd)

extern int do_check_pgt_cache(int, int);

#endif
