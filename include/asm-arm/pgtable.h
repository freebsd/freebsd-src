/*
 *  linux/include/asm-arm/pgtable.h
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_PGTABLE_H
#define _ASMARM_PGTABLE_H

#include <linux/config.h>
#include <asm/memory.h>
#include <asm/proc-fns.h>

/*
 * PMD_SHIFT determines the size of the area a second-level page table can map
 * PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT		20
#define PGDIR_SHIFT		20

#define LIBRARY_TEXT_START	0x0c000000

#ifndef __ASSEMBLY__
extern void __pte_error(const char *file, int line, unsigned long val);
extern void __pmd_error(const char *file, int line, unsigned long val);
extern void __pgd_error(const char *file, int line, unsigned long val);

#define pte_ERROR(pte)		__pte_error(__FILE__, __LINE__, pte_val(pte))
#define pmd_ERROR(pmd)		__pmd_error(__FILE__, __LINE__, pmd_val(pmd))
#define pgd_ERROR(pgd)		__pgd_error(__FILE__, __LINE__, pgd_val(pgd))
#endif /* !__ASSEMBLY__ */

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))

#define FIRST_USER_PGD_NR	1
#define USER_PTRS_PER_PGD	((TASK_SIZE/PGDIR_SIZE) - FIRST_USER_PGD_NR)

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions
 */
#define __P000  PAGE_NONE
#define __P001  PAGE_READONLY
#define __P010  PAGE_COPY
#define __P011  PAGE_COPY
#define __P100  PAGE_READONLY
#define __P101  PAGE_READONLY
#define __P110  PAGE_COPY
#define __P111  PAGE_COPY

#define __S000  PAGE_NONE
#define __S001  PAGE_READONLY
#define __S010  PAGE_SHARED
#define __S011  PAGE_SHARED
#define __S100  PAGE_READONLY
#define __S101  PAGE_READONLY
#define __S110  PAGE_SHARED
#define __S111  PAGE_SHARED

#ifndef __ASSEMBLY__
/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern struct page *empty_zero_page;
#define ZERO_PAGE(vaddr)	(empty_zero_page)

#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)
#define pfn_pte(pfn,prot)	(__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot)))

#define pte_none(pte)		(!pte_val(pte))
#define pte_clear(ptep)		set_pte((ptep), __pte(0))

#define pte_page(pte)		(pfn_to_page(pte_pfn(pte)))

#define pmd_none(pmd)		(!pmd_val(pmd))
#define pmd_present(pmd)	(pmd_val(pmd))
#define pmd_clear(pmdp)		set_pmd(pmdp, __pmd(0))

/*
 * Permanent address of a page. We never have highmem, so this is trivial.
 */
#define pages_to_mb(x)		((x) >> (20 - PAGE_SHIFT))

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
static inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{
	pte_t pte;
	pte_val(pte) = physpage | pgprot_val(pgprot);
	return pte;
}

#define mk_pte(page,prot)	pfn_pte(page_to_pfn(page),prot)

/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
#define pgd_none(pgd)		(0)
#define pgd_bad(pgd)		(0)
#define pgd_present(pgd)	(1)
#define pgd_clear(pgdp)		do { } while (0)

#define page_pte_prot(page,prot)	mk_pte(page, prot)
#define page_pte(page)		mk_pte(page, __pgprot(0))

/* to find an entry in a page-table-directory */
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)
#define __pgd_offset(addr)	pgd_index(addr)

#define pgd_offset(mm, addr)	((mm)->pgd+pgd_index(addr))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(addr)	pgd_offset(&init_mm, addr)

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir, addr)	((pmd_t *)(dir))

/* Find an entry in the third-level page table.. */
#define __pte_offset(addr)	(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, addr)	((pte_t *)pmd_page(*(dir)) + __pte_offset(addr))

#include <asm/proc/pgtable.h>

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot);
	return pte;
}

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

/* Encode and decode a swap entry.
 *
 * We support up to 32GB of swap on 4k machines
 */
#define SWP_TYPE(x)		(((x).val >> 2) & 0x7f)
#define SWP_OFFSET(x)		((x).val >> 9)
#define SWP_ENTRY(type,offset)	((swp_entry_t) { ((type) << 2) | ((offset) << 9) })
#define pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(swp)	((pte_t) { (swp).val })

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
/* FIXME: this is not correct */
#define kern_addr_valid(addr)	(1)

#include <asm-generic/pgtable.h>

extern void pgtable_cache_init(void);

/*
 * remap a physical address `phys' of size `size' with page protection `prot'
 * into virtual address `from'
 */
#define io_remap_page_range(from,phys,size,prot) \
		remap_page_range(from,phys,size,prot)

#endif /* !__ASSEMBLY__ */

#endif /* _ASMARM_PGTABLE_H */
