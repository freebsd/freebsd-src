/*
 *  linux/include/asm-arm/proc-armo/pgtable.h
 *
 *  Copyright (C) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  18-Oct-1997	RMK	Now two-level (32x32)
 */
#ifndef __ASM_PROC_PGTABLE_H
#define __ASM_PROC_PGTABLE_H

/*
 * entries per page directory level: they are two-level, so
 * we don't really have any PMD directory.
 */
#define PTRS_PER_PTE		32
#define PTRS_PER_PMD		1
#define PTRS_PER_PGD		32

/*
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
#define VMALLOC_START	  0x01a00000
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END	  0x01c00000

#define _PAGE_TABLE     (0x01)

#define pmd_bad(pmd)		((pmd_val(pmd) & 0xfc000002))
#define set_pmd(pmdp,pmd)	((*(pmdp)) = (pmd))

static inline pmd_t __mk_pmd(pte_t *ptep, unsigned long prot)
{
	unsigned long pte_ptr = (unsigned long)ptep;
	pmd_t pmd;

	pmd_val(pmd) = __virt_to_phys(pte_ptr) | prot;

	return pmd;
}

static inline unsigned long pmd_page(pmd_t pmd)
{
	return __phys_to_virt(pmd_val(pmd) & ~_PAGE_TABLE);
}

#define set_pte(pteptr, pteval)	((*(pteptr)) = (pteval))

#define _PAGE_PRESENT	0x01
#define _PAGE_READONLY	0x02
#define _PAGE_NOT_USER	0x04
#define _PAGE_OLD	0x08
#define _PAGE_CLEAN	0x10

/*                               -- present --   -- !dirty --  --- !write ---   ---- !user --- */
#define PAGE_NONE       __pgprot(_PAGE_PRESENT | _PAGE_CLEAN | _PAGE_READONLY | _PAGE_NOT_USER)
#define PAGE_SHARED     __pgprot(_PAGE_PRESENT | _PAGE_CLEAN                                  )
#define PAGE_COPY       __pgprot(_PAGE_PRESENT | _PAGE_CLEAN | _PAGE_READONLY                 )
#define PAGE_READONLY   __pgprot(_PAGE_PRESENT | _PAGE_CLEAN | _PAGE_READONLY                 )
#define PAGE_KERNEL     __pgprot(_PAGE_PRESENT                                | _PAGE_NOT_USER)

#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_OLD | _PAGE_CLEAN)


/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
#define pte_present(pte)		(pte_val(pte) & _PAGE_PRESENT)
#define pte_read(pte)			(!(pte_val(pte) & _PAGE_NOT_USER))
#define pte_write(pte)			(!(pte_val(pte) & _PAGE_READONLY))
#define pte_exec(pte)			(!(pte_val(pte) & _PAGE_NOT_USER))
#define pte_dirty(pte)			(!(pte_val(pte) & _PAGE_CLEAN))
#define pte_young(pte)			(!(pte_val(pte) & _PAGE_OLD))

static inline pte_t pte_wrprotect(pte_t pte)    { pte_val(pte) |= _PAGE_READONLY;  return pte; }
static inline pte_t pte_rdprotect(pte_t pte)    { pte_val(pte) |= _PAGE_NOT_USER;  return pte; }
static inline pte_t pte_exprotect(pte_t pte)    { pte_val(pte) |= _PAGE_NOT_USER;  return pte; }
static inline pte_t pte_mkclean(pte_t pte)      { pte_val(pte) |= _PAGE_CLEAN;     return pte; }
static inline pte_t pte_mkold(pte_t pte)        { pte_val(pte) |= _PAGE_OLD;       return pte; }

static inline pte_t pte_mkwrite(pte_t pte)      { pte_val(pte) &= ~_PAGE_READONLY; return pte; }
static inline pte_t pte_mkread(pte_t pte)       { pte_val(pte) &= ~_PAGE_NOT_USER; return pte; }
static inline pte_t pte_mkexec(pte_t pte)       { pte_val(pte) &= ~_PAGE_NOT_USER; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)      { pte_val(pte) &= ~_PAGE_CLEAN;    return pte; }
static inline pte_t pte_mkyoung(pte_t pte)      { pte_val(pte) &= ~_PAGE_OLD;      return pte; }

#define pte_alloc_kernel        pte_alloc

/*
 * We don't store cache state bits in the page table here.
 */
#define pgprot_noncached(prot)	(prot)

#endif /* __ASM_PROC_PGTABLE_H */
