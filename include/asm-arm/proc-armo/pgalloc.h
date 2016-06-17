/*
 *  linux/include/asm-arm/proc-armo/pgalloc.h
 *
 *  Copyright (C) 2001 Russell King
 *
 * Page table allocation/freeing primitives for 26-bit ARM processors.
 */

/* unfortunately, this includes linux/mm.h and the rest of the universe. */
#include <linux/slab.h>

extern kmem_cache_t *pte_cache;

/*
 * Allocate one PTE table.
 *
 * Note that we keep the processor copy of the PTE entries separate
 * from the Linux copy.  The processor copies are offset by -PTRS_PER_PTE
 * words from the Linux copy.
 */
static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	return kmem_cache_alloc(pte_cache, GFP_KERNEL);
}

/*
 * Free one PTE table.
 */
static inline void pte_free_slow(pte_t *pte)
{
	if (pte)
		kmem_cache_free(pte_cache, pte);
}

/*
 * Populate the pmdp entry with a pointer to the pte.  This pmd is part
 * of the mm address space.
 *
 * If 'mm' is the init tasks mm, then we are doing a vmalloc, and we
 * need to set stuff up correctly for it.
 */
#define pmd_populate(mm,pmdp,pte)				\
	do {							\
		set_pmd(pmdp, __mk_pmd(pte, _PAGE_TABLE));	\
	} while (0)


