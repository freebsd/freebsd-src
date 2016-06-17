#ifndef _CRIS_PGALLOC_H
#define _CRIS_PGALLOC_H

#include <asm/page.h>
#include <linux/threads.h>

extern struct pgtable_cache_struct {
        unsigned long *pgd_cache;
        unsigned long *pte_cache;
        unsigned long pgtable_cache_sz;
} quicklists;

#define pgd_quicklist           (quicklists.pgd_cache)
#define pmd_quicklist           ((unsigned long *)0)
#define pte_quicklist           (quicklists.pte_cache)
#define pgtable_cache_size      (quicklists.pgtable_cache_sz)

#define pmd_populate(mm, pmd, pte) pmd_set(pmd, pte)

/*
 * Allocate and free page tables.
 */

extern __inline__ pgd_t *get_pgd_slow(void)
{
        pgd_t *ret = (pgd_t *)__get_free_page(GFP_KERNEL);

        if (ret) {
                memset(ret, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));
                memcpy(ret + USER_PTRS_PER_PGD, swapper_pg_dir + USER_PTRS_PER_PGD,
		       (PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));
        }
        return ret;
}

extern __inline__ void free_pgd_slow(pgd_t *pgd)
{
        free_page((unsigned long)pgd);
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
        *(unsigned long *)pgd = (unsigned long) pgd_quicklist;
        pgd_quicklist = (unsigned long *) pgd;
        pgtable_cache_size++;
}

extern inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
        pte_t *pte;

        pte = (pte_t *) __get_free_page(GFP_KERNEL);
        if (pte)
                clear_page(pte);
        return pte;
}

extern inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
        unsigned long *ret;

        if((ret = (unsigned long *)pte_quicklist) != NULL) {
                pte_quicklist = (unsigned long *)(*ret);
                ret[0] = ret[1];
                pgtable_cache_size--;
        }
        return (pte_t *)ret;
}

extern __inline__ void pte_free_fast(pte_t *pte)
{
        *(unsigned long *)pte = (unsigned long) pte_quicklist;
        pte_quicklist = (unsigned long *) pte;
        pgtable_cache_size++;
}

extern __inline__ void pte_free_slow(pte_t *pte)
{
        free_page((unsigned long)pte);
}

#define pte_free(pte)      pte_free_slow(pte)
#define pgd_free(pgd)      free_pgd_slow(pgd)
#define pgd_alloc(mm)      get_pgd_fast()

/*
 * We don't have any real pmd's, and this code never triggers because
 * the pgd will always be present..
 */

#define pmd_alloc_one_fast(mm, addr)    ({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm, addr)         ({ BUG(); ((pmd_t *)2); })
#define pmd_free_slow(x)                do { } while (0)
#define pmd_free_fast(x)                do { } while (0)
#define pmd_free(x)                     do { } while (0)
#define pgd_populate(mm, pmd, pte)      BUG()

/* other stuff */

extern int do_check_pgt_cache(int, int);

#endif
