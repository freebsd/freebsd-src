#ifdef __KERNEL__
#ifndef _PPC_PGALLOC_H
#define _PPC_PGALLOC_H

#include <linux/config.h>
#include <linux/threads.h>
#include <asm/processor.h>

#ifdef CONFIG_PTE_64BIT
/* 44x uses an 8kB pgdir because it has 8-byte Linux PTEs. */
#define PGDIR_ORDER	1
#else
#define PGDIR_ORDER	0
#endif

/*
 * This is handled very differently on the PPC since out page tables
 * are all 0's and I want to be able to use these zero'd pages elsewhere
 * as well - it gives us quite a speedup.
 *
 * Note that the SMP/UP versions are the same but we don't need a
 * per cpu list of zero pages because we do the zero-ing with the cache
 * off and the access routines are lock-free but the pgt cache stuff
 * is per-cpu since it isn't done with any lock-free access routines
 * (although I think we need arch-specific routines so I can do lock-free).
 *
 * I need to generalize this so we can use it for other arch's as well.
 * -- Cort
 */
#ifdef CONFIG_SMP
#define quicklists	cpu_data[smp_processor_id()]
#else
extern struct pgtable_cache_struct {
	unsigned long *pgd_cache;
	unsigned long *pte_cache;
	unsigned long pgtable_cache_sz;
} quicklists;
#endif

#define pgd_quicklist 		(quicklists.pgd_cache)
#define pmd_quicklist 		((unsigned long *)0)
#define pte_quicklist 		(quicklists.pte_cache)
#define pgtable_cache_size 	(quicklists.pgtable_cache_sz)

extern unsigned long *zero_cache;    /* head linked list of pre-zero'd pages */
extern atomic_t zero_sz;	     /* # currently pre-zero'd pages */
extern atomic_t zeropage_hits;	     /* # zero'd pages request that we've done */
extern atomic_t zeropage_calls;      /* # zero'd pages request that've been made */
extern atomic_t zerototal;	     /* # pages zero'd over time */

#define zero_quicklist     	(zero_cache)
#define zero_cache_sz  	 	(zero_sz)
#define zero_cache_calls 	(zeropage_calls)
#define zero_cache_hits  	(zeropage_hits)
#define zero_cache_total 	(zerototal)

/* return a pre-zero'd page from the list, return NULL if none available -- Cort */
extern unsigned long get_zero_page_fast(void);

extern void __bad_pte(pmd_t *pmd);

extern __inline__ pgd_t *get_pgd_slow(void)
{
	pgd_t *ret;

	if ((ret = (pgd_t *)__get_free_pages(GFP_KERNEL, PGDIR_ORDER)) != NULL)
		clear_page(ret);
	return ret;
}

extern __inline__ pgd_t *get_pgd_fast(void)
{
        unsigned long *ret;

        if ((ret = pgd_quicklist) != NULL) {
                pgd_quicklist = (unsigned long *)(*ret);
                ret[0] = 0;
                pgtable_cache_size--;
        } else
                ret = (unsigned long *)get_pgd_slow();
        return (pgd_t *)ret;
}

extern __inline__ void free_pgd_fast(pgd_t *pgd)
{
        *(unsigned long **)pgd = pgd_quicklist;
        pgd_quicklist = (unsigned long *) pgd;
        pgtable_cache_size++;
}

extern __inline__ void free_pgd_slow(pgd_t *pgd)
{
	free_page((unsigned long)pgd);
}

#define pgd_free(pgd)		free_pgd_fast(pgd)
#define pgd_alloc(mm)		get_pgd_fast()

/*
 * We don't have any real pmd's, and this code never triggers because
 * the pgd will always be present..
 */
#define pmd_alloc_one_fast(mm, address) ({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm,address)       ({ BUG(); ((pmd_t *)2); })
#define pmd_free(x)                     do { } while (0)
#define pgd_populate(mm, pmd, pte)      BUG()

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte;
	extern int mem_init_done;
	extern void *early_get_page(void);

	if (mem_init_done)
		pte = (pte_t *) __get_free_page(GFP_KERNEL);
	else
		pte = (pte_t *) early_get_page();
	if (pte != NULL)
		clear_page(pte);
	return pte;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
        unsigned long *ret;

        if ((ret = pte_quicklist) != NULL) {
                pte_quicklist = (unsigned long *)(*ret);
                ret[0] = 0;
                pgtable_cache_size--;
	}
        return (pte_t *)ret;
}

extern __inline__ void pte_free_fast(pte_t *pte)
{
        *(unsigned long **)pte = pte_quicklist;
        pte_quicklist = (unsigned long *) pte;
        pgtable_cache_size++;
}

extern __inline__ void pte_free_slow(pte_t *pte)
{
	free_page((unsigned long)pte);
}

#define pte_free(pte)    pte_free_slow(pte)

#define pmd_populate(mm, pmd, pte)	(pmd_val(*(pmd)) = (unsigned long) (pte))

extern int do_check_pgt_cache(int, int);

#endif /* _PPC_PGALLOC_H */
#endif /* __KERNEL__ */
