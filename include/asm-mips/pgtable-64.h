#ifndef _MIPS_PGTABLE_64_H
#define _MIPS_PGTABLE_64_H

/*
 * Not really a 3 level page table but we follow most of the x86 PAE code.
 */

#define PMD_SHIFT	21
#define PGD_ORDER	1
#define PTE_ORDER	0

#if !defined (_LANGUAGE_ASSEMBLY)
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, (e).pte_low)
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

static inline int pte_none(pte_t pte)    { return !(pte_val(pte) & ~_PAGE_GLOBAL); }

static inline int pte_same(pte_t a, pte_t b)
{
	return a.pte_low == b.pte_low && a.pte_high == b.pte_high;
}


static inline pte_t pte_wrprotect(pte_t pte)
{
	(pte).pte_low &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
	(pte).pte_high &= ~_PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_rdprotect(pte_t pte)
{
	(pte).pte_low &= ~(_PAGE_READ | _PAGE_SILENT_READ);
	(pte).pte_high &= ~_PAGE_SILENT_READ;
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	(pte).pte_low &= ~(_PAGE_MODIFIED|_PAGE_SILENT_WRITE);
	(pte).pte_high &= ~_PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	(pte).pte_low &= ~(_PAGE_ACCESSED|_PAGE_SILENT_READ);
	(pte).pte_high &= ~_PAGE_SILENT_READ;
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	(pte).pte_low |= _PAGE_WRITE;
	if ((pte).pte_low & _PAGE_MODIFIED) {
		(pte).pte_low |= _PAGE_SILENT_WRITE;
		(pte).pte_high |= _PAGE_SILENT_WRITE;
	}
	return pte;
}

static inline pte_t pte_mkread(pte_t pte)
{
	(pte).pte_low |= _PAGE_READ;
	if ((pte).pte_low & _PAGE_ACCESSED) {
		(pte).pte_low |= _PAGE_SILENT_READ;
		(pte).pte_high |= _PAGE_SILENT_READ;
	}
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	(pte).pte_low |= _PAGE_MODIFIED;
	if ((pte).pte_low & _PAGE_WRITE) {
		(pte).pte_low |= _PAGE_SILENT_WRITE;
		(pte).pte_high |= _PAGE_SILENT_WRITE;
	}
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
	(pte).pte_low |= _PAGE_ACCESSED;
	if ((pte).pte_low & _PAGE_READ)
		(pte).pte_low |= _PAGE_SILENT_READ;
		(pte).pte_high |= _PAGE_SILENT_READ;
	return pte;
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
#define mk_pte(page, pgprot) __mk_pte((page) - mem_map, (pgprot))
#define mk_pte_phys(physpage, pgprot)	__mk_pte((physpage) >> PAGE_SHIFT, pgprot)

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte.pte_low &= _PAGE_CHG_MASK;
	pte.pte_low |= pgprot_val(newprot);
	pte.pte_high |= pgprot_val(newprot) & 0x3f;
	return pte;
}

#define pte_page(x)    (mem_map+(((x).pte_high >> 6)))

/*
 * MIPS32 Note
 * pte_low contains the 12 low bits only.  This includes the 6 lsb bits
 * which contain software control bits, and the next 6 attribute bits 
 * which are actually written in the entrylo[0,1] registers (G,V,D,Cache Mask).
 * pte_high contains the 36 bit physical address and the 6 hardware 
 * attribute bits (G,V,D, Cache Mask). The entry is already fully setup
 * so in the tlb refill handler we do not need to shift right 6.
 */

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

	if (pte_val(pte) & _PAGE_GLOBAL) {
		pte_t *buddy = ptep_buddy(ptep);
		/*
		 * Make sure the buddy is global too (if it's !none,
		 * it better already be global)
		 */
		if (pte_none(*buddy))
			buddy->pte_low |= _PAGE_GLOBAL;
	}
}

static inline pte_t 
__mk_pte(unsigned long page_nr, pgprot_t pgprot)
{
	pte_t pte;

	pte.pte_high = (page_nr << 6) | (pgprot_val(pgprot) & 0x3f);
	pte.pte_low = pgprot_val(pgprot);
	return pte;
}

static inline void pte_clear(pte_t *ptep)
{
	/* Preserve global status for the pair */
	if (pte_val(*ptep_buddy(ptep)) & _PAGE_GLOBAL)
		set_pte(ptep, __pte(_PAGE_GLOBAL));
	else
		set_pte(ptep, __pte(0));
}

#endif

#endif /* _MIPS_PGTABLE_64_H */
