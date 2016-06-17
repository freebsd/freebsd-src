#ifndef __ASM_SH64_PGALLOC_H
#define __ASM_SH64_PGALLOC_H

#include <asm/processor.h>
#include <linux/threads.h>
#include <linux/slab.h>

#define pgd_quicklist ((unsigned long *)0)
#define pmd_quicklist ((unsigned long *)0)
#define pte_quicklist ((unsigned long *)0)
#define pgtable_cache_size 0L

/*
#define pmd_populate(mm, pmd, pte) \
		set_pmd(pmd, __pmd(_PAGE_TABLE + __pa(pte)))

*/

/*
 * Allocate and free page tables.
 */

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	unsigned int pgd_size = (USER_PTRS_PER_PGD * sizeof(pgd_t));
	pgd_t *pgd = (pgd_t *)kmalloc(pgd_size, GFP_KERNEL);

	if (pgd)
		memset(pgd, 0, pgd_size);

	return pgd;
}

static inline void pgd_free(pgd_t *pgd)
{
	kfree(pgd);
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *) __get_free_page(GFP_KERNEL);
	if (pte)
		clear_page(pte);
	return pte;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	return 0;
}

static inline void pte_free_slow(pte_t *pte)
{
	free_page((unsigned long)pte);
}

#define pte_free(pte)		pte_free_slow(pte)

#define pgd_set(pgd,pmd) pgd_val(*pgd) = ( ((unsigned long)pmd) & PAGE_MASK)

#define pgd_populate(mm, pgd, pmd)	pgd_set(pgd,pmd)


static inline pmd_t*
pmd_alloc_one_fast (struct mm_struct *mm, unsigned long addr)
{
#if defined(CONFIG_SH64_PGTABLE_2_LEVEL)
	BUG();
#endif
	return 0;
}
 
static inline pmd_t*
pmd_alloc_one (struct mm_struct *mm, unsigned long addr)
{
        pmd_t *pmd = NULL;
	
#if defined(CONFIG_SH64_PGTABLE_2_LEVEL)
	BUG();
#elif defined(CONFIG_SH64_PGTABLE_3_LEVEL)
	pmd = (pmd_t *) __get_free_page(GFP_KERNEL);
 
        if (pmd != NULL )
                clear_page(pmd);
#endif
        return pmd;
}
 
#if defined(CONFIG_SH64_PGTABLE_2_LEVEL)
static inline void
pmd_free (pmd_t *pmd)
{
	return;
}
#elif defined(CONFIG_SH64_PGTABLE_3_LEVEL)
static inline void
pmd_free (pmd_t *pmd)
{
 free_page((unsigned long)pmd);      
}
#endif


static inline void
pmd_populate (struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{

  pmd_set(pmd,pte);


    //     pmd_val(*pmd_entry) = __pa(pte);
}
 

/* Do nothing */
#define do_check_pgt_cache(low, high)	(0)

/*
 * TLB flushing:
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *  - flush_tlb_pgtables(mm, start, end) flushes a range of page tables
 */

extern void flush_tlb(void);
extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_range(struct mm_struct *mm, unsigned long start,
			    unsigned long end);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long page);

static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{ /* Nothing to do */
}

/* These are generic functions. These will need to change once D cache 
 * aliasing support has been added. 
 */

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(ptep);
	return pte;
}

/*
 * Following functions are same as generic ones.
 */
static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	set_pte(ptep, pte_mkold(pte));
	return 1;
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_dirty(pte))
		return 0;
	set_pte(ptep, pte_mkclean(pte));
	return 1;
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_wrprotect(old_pte));
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_mkdirty(old_pte));
}
#endif /* __ASM_SH64_PGALLOC_H */

