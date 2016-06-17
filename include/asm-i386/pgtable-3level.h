#ifndef _I386_PGTABLE_3LEVEL_H
#define _I386_PGTABLE_3LEVEL_H

/*
 * Intel Physical Address Extension (PAE) Mode - three-level page
 * tables on PPro+ CPUs.
 *
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 */

/*
 * PGDIR_SHIFT determines what a top-level page table entry can map
 */
#define PGDIR_SHIFT	30
#define PTRS_PER_PGD	4

/*
 * PMD_SHIFT determines the size of the area a middle-level
 * page table can map
 */
#define PMD_SHIFT	21
#define PTRS_PER_PMD	512

/*
 * entries per page directory level
 */
#define PTRS_PER_PTE	512

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %p(%08lx%08lx).\n", __FILE__, __LINE__, &(e), (e).pte_high, (e).pte_low)
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %p(%016Lx).\n", __FILE__, __LINE__, &(e), pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %p(%016Lx).\n", __FILE__, __LINE__, &(e), pgd_val(e))

static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }

/* Rules for using set_pte: the pte being assigned *must* be
 * either not present or in a state where the hardware will
 * not attempt to update the pte.  In places where this is
 * not possible, use pte_get_and_clear to obtain the old pte
 * value and then use set_pte to update it.  -ben
 */
static inline void set_pte(pte_t *ptep, pte_t pte)
{
	ptep->pte_high = pte.pte_high;
	smp_wmb();
	ptep->pte_low = pte.pte_low;
}
#define set_pmd(pmdptr,pmdval) \
		set_64bit((unsigned long long *)(pmdptr),pmd_val(pmdval))
#define set_pgd(pgdptr,pgdval) \
		set_64bit((unsigned long long *)(pgdptr),pgd_val(pgdval))
#define set_pte_atomic(pteptr,pteval) \
		set_64bit((unsigned long long *)(pteptr),pte_val(pteval))


/*
 * Pentium-II erratum A13: in PAE mode we explicitly have to flush
 * the TLB via cr3 if the top-level pgd is changed...
 * We do not let the generic code free and clear pgd entries due to
 * this erratum.
 */
static inline void pgd_clear (pgd_t * pgd) { }

#define pgd_page(pgd) \
((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir, address) ((pmd_t *) pgd_page(*(dir)) + \
			__pmd_offset(address))

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	pte_t res;

	/* xchg acts as a barrier before the setting of the high bits */
	res.pte_low = xchg(&ptep->pte_low, 0);
	res.pte_high = ptep->pte_high;
	ptep->pte_high = 0;

	return res;
}

static inline int pte_same(pte_t a, pte_t b)
{
	return a.pte_low == b.pte_low && a.pte_high == b.pte_high;
}

#define pte_page(x)	(mem_map+(((x).pte_low >> PAGE_SHIFT) | ((x).pte_high << (32 - PAGE_SHIFT))))
#define pte_none(x)	(!(x).pte_low && !(x).pte_high)

static inline pte_t __mk_pte(unsigned long page_nr, pgprot_t pgprot)
{
	pte_t pte;

	pte.pte_high = page_nr >> (32 - PAGE_SHIFT);
	pte.pte_low = (page_nr << PAGE_SHIFT) | pgprot_val(pgprot);
	return pte;
}

#endif /* _I386_PGTABLE_3LEVEL_H */
