#ifndef _MIPS_PGTABLE_32_H
#define _MIPS_PGTABLE_32_H

/*
 * traditional mips two-level paging structure:
 */

#ifdef CONFIG_64BIT_PHYS_ADDR
#define PGD_ORDER	1
#define PTE_ORDER	0
#else
#define PGD_ORDER	0
#define PTE_ORDER	0
#endif

#define PMD_SHIFT       (2 * PAGE_SHIFT - PTE_T_LOG2)

#if !defined (_LANGUAGE_ASSEMBLY)
#ifdef CONFIG_64BIT_PHYS_ADDR
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016Lx.\n", __FILE__, __LINE__, pte_val(e))
#else
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, (e).pte_low)
#endif
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

static inline int pte_none(pte_t pte)    { return !(pte_val(pte) & ~_PAGE_GLOBAL); }

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_rdprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_READ | _PAGE_SILENT_READ);
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_MODIFIED|_PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED|_PAGE_SILENT_READ);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	if (pte_val(pte) & _PAGE_MODIFIED)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkread(pte_t pte)
{
	pte_val(pte) |= _PAGE_READ;
	if (pte_val(pte) & _PAGE_ACCESSED)
		pte_val(pte) |= _PAGE_SILENT_READ;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_MODIFIED;
	if (pte_val(pte) & _PAGE_WRITE)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

/*
 * Macro to make mark a page protection value as "uncacheable".  Note
 * that "protection" is really a misnomer here as the protection value
 * contains the memory attribute bits, dirty bits, and various other
 * bits as well.
 */
#define pgprot_noncached pgprot_noncached

static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot = (prot & ~_CACHE_MASK) | _CACHE_UNCACHED;

	return __pgprot(prot);
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	if (pte_val(pte) & _PAGE_READ)
		pte_val(pte) |= _PAGE_SILENT_READ;
	return pte;
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

#ifdef CONFIG_CPU_VR41XX
#define mk_pte(page, pgprot)                                            \
({                                                                      \
        pte_t   __pte;                                                  \
                                                                        \
        pte_val(__pte) = ((phys_t)(page - mem_map) << (PAGE_SHIFT + 2)) | \
                         pgprot_val(pgprot);                            \
                                                                        \
        __pte;                                                          \
})
#else
#define mk_pte(page, pgprot)						\
({									\
	pte_t   __pte;							\
									\
	pte_val(__pte) = ((phys_t)(page - mem_map) << PAGE_SHIFT) | \
	                 pgprot_val(pgprot);				\
									\
	__pte;								\
})
#endif

static inline pte_t mk_pte_phys(phys_t physpage, pgprot_t pgprot)
{
#ifdef CONFIG_CPU_VR41XX
        return __pte((physpage << 2) | pgprot_val(pgprot));
#else
	return __pte(physpage | pgprot_val(pgprot));
#endif
}

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

/* Certain architectures need to do special things when pte's
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	*ptep = pteval;
#if !defined(CONFIG_CPU_R3000) && !defined(CONFIG_CPU_TX39XX)
	if (pte_val(pteval) & _PAGE_GLOBAL) {
		pte_t *buddy = ptep_buddy(ptep);
		/*
		 * Make sure the buddy is global too (if it's !none,
		 * it better already be global)
		 */
		if (pte_none(*buddy))
			pte_val(*buddy) = pte_val(*buddy) | _PAGE_GLOBAL;
	}
#endif
}

static inline void pte_clear(pte_t *ptep)
{
#if !defined(CONFIG_CPU_R3000) && !defined(CONFIG_CPU_TX39XX)
	/* Preserve global status for the pair */
	if (pte_val(*ptep_buddy(ptep)) & _PAGE_GLOBAL)
		set_pte(ptep, __pte(_PAGE_GLOBAL));
	else
#endif
		set_pte(ptep, __pte(0));
}

#ifdef CONFIG_CPU_VR41XX
#define pte_page(x)  (mem_map+((unsigned long)(((x).pte_low >> (PAGE_SHIFT+2)))))
#define __mk_pte(page_nr,pgprot) __pte(((page_nr) << (PAGE_SHIFT+2)) | pgprot_val(pgprot))
#else
#define pte_page(x)  (mem_map+((unsigned long)(((x).pte_low >> PAGE_SHIFT))))
#define __mk_pte(page_nr,pgprot) __pte(((page_nr) << PAGE_SHIFT) | pgprot_val(pgprot))
#endif

#endif

#endif /* _MIPS_PGTABLE_32_H */
