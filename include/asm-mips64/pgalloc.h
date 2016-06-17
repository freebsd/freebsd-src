/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2001 by Ralf Baechle at alii
 * Copyright (C) 1999, 2000, 2001 Silicon Graphics, Inc.
 */
#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H

#include <linux/config.h>

/* TLB flushing:
 *
 *  - flush_tlb_all() flushes all processes TLB entries
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB entries
 *  - flush_tlb_page(mm, vmaddr) flushes a single page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *  - flush_tlb_pgtables(mm, start, end) flushes a range of page tables
 *  - flush_tlb_one(page) flushes a single kernel page
 */
extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *mm);
extern void local_flush_tlb_range(struct mm_struct *mm, unsigned long start,
			       unsigned long end);
extern void local_flush_tlb_page(struct vm_area_struct *vma,
                                 unsigned long page);
extern void local_flush_tlb_one(unsigned long page);

#ifdef CONFIG_SMP

extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_range(struct mm_struct *, unsigned long, unsigned long);
extern void flush_tlb_page(struct vm_area_struct *, unsigned long);

#else /* CONFIG_SMP */

#define flush_tlb_all()			local_flush_tlb_all()
#define flush_tlb_mm(mm)		local_flush_tlb_mm(mm)
#define flush_tlb_range(mm,vmaddr,end)	local_flush_tlb_range(mm, vmaddr, end)
#define flush_tlb_page(vma,page)	local_flush_tlb_page(vma, page)

#endif /* CONFIG_SMP */

static inline void flush_tlb_pgtables(struct mm_struct *mm,
                                      unsigned long start, unsigned long end)
{
	/* Nothing to do on MIPS.  */
}


/*
 * Allocate and free page tables.
 */

#define pgd_quicklist (current_cpu_data.pgd_quick)
#define pmd_quicklist (current_cpu_data.pmd_quick)
#define pte_quicklist (current_cpu_data.pte_quick)
#define pgtable_cache_size (current_cpu_data.pgtable_cache_sz)

#define pmd_populate(mm, pmd, pte)	pmd_set(pmd, pte)
#define pgd_populate(mm, pgd, pmd)	pgd_set(pgd, pmd)

extern pgd_t *get_pgd_slow(void);

static inline pgd_t *get_pgd_fast(void)
{
	unsigned long *ret;

	if((ret = pgd_quicklist) != NULL) {
		pgd_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
		return (pgd_t *)ret;
	}

	ret = (unsigned long *) get_pgd_slow();
	return (pgd_t *)ret;
}

static inline void free_pgd_fast(pgd_t *pgd)
{
	*(unsigned long *)pgd = (unsigned long) pgd_quicklist;
	pgd_quicklist = (unsigned long *) pgd;
	pgtable_cache_size++;
}

static inline void free_pgd_slow(pgd_t *pgd)
{
	free_pages((unsigned long)pgd, PGD_ORDER);
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte;

	pte = (pte_t *) __get_free_pages(GFP_KERNEL, PTE_ORDER);
	if (pte)
		clear_page(pte);
	return pte;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
	}
	return (pte_t *)ret;
}

extern pte_t *get_pte_slow(pmd_t *pmd, unsigned long address_preadjusted);

static inline pte_t *get_pte_fast(void)
{
	unsigned long *ret;

	if((ret = (unsigned long *)pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
	}
	return (pte_t *)ret;
}

static inline void free_pte_fast(pte_t *pte)
{
	*(unsigned long *)pte = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pte;
	pgtable_cache_size++;
}

static inline void free_pte_slow(pte_t *pte)
{
	free_pages((unsigned long)pte, PTE_ORDER);
}

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pmd_t *pmd;

	pmd = (pmd_t *) __get_free_pages(GFP_KERNEL, PMD_ORDER);
	if (pmd)
		pmd_init((unsigned long)pmd, (unsigned long)invalid_pte_table);
	return pmd;
}

static inline pmd_t *pmd_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pmd_quicklist) != NULL) {
		pmd_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
	}
	return (pmd_t *)ret;
}

extern pmd_t *get_pmd_slow(pgd_t *pgd, unsigned long address_preadjusted);

static inline pmd_t *get_pmd_fast(void)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pmd_quicklist) != NULL) {
		pmd_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
		return (pmd_t *)ret;
	}

	return (pmd_t *)ret;
}

static inline void free_pmd_fast(pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long) pmd_quicklist;
	pmd_quicklist = (unsigned long *) pmd;
	pgtable_cache_size++;
}

static inline void free_pmd_slow(pmd_t *pmd)
{
	free_pages((unsigned long)pmd, PMD_ORDER);
}

#define pte_free(pte)           free_pte_fast(pte)
#define pmd_free(pte)           free_pmd_fast(pte)
#define pgd_free(pgd)           free_pgd_fast(pgd)
#define pgd_alloc(mm)           get_pgd_fast()

extern pte_t kptbl[(PAGE_SIZE << PGD_ORDER)/sizeof(pte_t)];
extern pmd_t kpmdtbl[PTRS_PER_PMD];

extern int do_check_pgt_cache(int, int);

#endif /* _ASM_PGALLOC_H */
