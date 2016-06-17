/*
 *  include/asm-s390/pgtable.h
 *
 *  S390 64bit version
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com)
 *               Ulrich Weigand (weigand@de.ibm.com)
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/pgtable.h"
 */

#ifndef _ASM_S390_PGTABLE_H
#define _ASM_S390_PGTABLE_H

/*
 * The Linux memory management assumes a three-level page table setup. On
 * the S390, we use that, but "fold" the mid level into the top-level page
 * table, so that we physically have the same two-level page table as the
 * S390 mmu expects.
 *
 * This file contains the functions and defines necessary to modify and use
 * the S390 page table tree.
 */
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <linux/threads.h>

extern pgd_t swapper_pg_dir[] __attribute__ ((aligned (4096)));
extern void paging_init(void);

/* Caches aren't brain-dead on S390. */
#define flush_cache_all()                       do { } while (0)
#define flush_cache_mm(mm)                      do { } while (0)
#define flush_cache_range(mm, start, end)       do { } while (0)
#define flush_cache_page(vma, vmaddr)           do { } while (0)
#define flush_page_to_ram(page)                 do { } while (0)
#define flush_dcache_page(page)			do { } while (0)
#define flush_icache_range(start, end)          do { } while (0)
#define flush_icache_page(vma,pg)               do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

/*
 * The S390 doesn't have any external MMU info: the kernel page
 * tables contain all the necessary information.
 */
#define update_mmu_cache(vma, address, pte)     do { } while (0)

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern char empty_zero_page[PAGE_SIZE];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))
#endif /* !__ASSEMBLY__ */

/*
 * PMD_SHIFT determines the size of the area a second-level page
 * table can map
 */
#define PMD_SHIFT       21
#define PMD_SIZE        (1UL << PMD_SHIFT)
#define PMD_MASK        (~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a third-level page table entry can map */
#define PGDIR_SHIFT     30
#define PGDIR_SIZE      (1UL << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE-1))

/*
 * entries per page directory level: the S390 is two to five-level,
 * currently we use a 3 level lookup
 */
#define PTRS_PER_PTE    512
#define PTRS_PER_PMD    512
#define PTRS_PER_PGD    2048

/*
 * pgd entries used up by user/kernel:
 */
#define USER_PTRS_PER_PGD  2048
#define USER_PGD_PTRS      2048
#define KERNEL_PGD_PTRS    2048
#define FIRST_USER_PGD_NR  0

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %016lx.\n", __FILE__, __LINE__, pgd_val(e))

#ifndef __ASSEMBLY__
/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
#define VMALLOC_OFFSET  (8*1024*1024)
#define VMALLOC_START   (((unsigned long) high_memory + VMALLOC_OFFSET) \
                         & ~(VMALLOC_OFFSET-1))
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END     (0x20000000000L)


/*
 * A pagetable entry of S390 has following format:
 * |                     PFRA                         |0IP0|  OS  |
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Page-Invalid Bit:    Page is not available for address-translation
 * P Page-Protection Bit: Store access not possible for page
 *
 * A segmenttable entry of S390 has following format:
 * |        P-table origin                              |      TT
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Segment-Invalid Bit:    Segment is not available for address-translation
 * C Common-Segment Bit:     Segment is not private (PoP 3-30)
 * P Page-Protection Bit: Store access not possible for page
 * TT Type 00
 *
 * A region table entry of S390 has following format:
 * |        S-table origin                             |   TF  TTTL
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Segment-Invalid Bit:    Segment is not available for address-translation
 * TT Type 01
 * TF
 * TL Table lenght
 *
 * The regiontable origin of S390 has following format:
 * |      region table origon                          |       DTTL
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * X Space-Switch event:
 * G Segment-Invalid Bit:  
 * P Private-Space Bit:    
 * S Storage-Alteration:
 * R Real space
 * TL Table-Length:
 *
 * A storage key has the following format:
 * | ACC |F|R|C|0|
 *  0   3 4 5 6 7
 * ACC: access key
 * F  : fetch protection bit
 * R  : referenced bit
 * C  : changed bit
 */

/* Bits in the page table entry */
#define _PAGE_PRESENT   0x001          /* Software                         */
#define _PAGE_ISCLEAN   0x004          /* Software                         */
#define _PAGE_RO        0x200          /* HW read-only                     */
#define _PAGE_INVALID   0x400          /* HW invalid                       */

/* Bits in the segment table entry */
#define _PMD_ENTRY_INV	0x20		/* invalid segment table entry      */
#define _PMD_ENTRY	0x00        

/* Bits in the region third table entry */
#define _PGD_ENTRY_INV	0x20		/* region table entry invalid bit  */
#define _PGD_ENTRY_MASK 0x04		/* region third table entry mask   */
#define _PGD_ENTRY_LEN(x) ((x)&3)       /* region table length bits        */
#define _PGD_ENTRY_OFF(x) (((x)&3)<<6)  /* region table offset bits        */

/*
 * User and kernel page directory
 */
#define _REGION_THIRD       0x4
#define _REGION_THIRD_LEN   0x1 
#define _REGION_TABLE       (_REGION_THIRD|_REGION_THIRD_LEN|0x40|0x100)
#define _KERN_REGION_TABLE  (_REGION_THIRD|_REGION_THIRD_LEN)

/* Bits in the storage key */
#define _PAGE_CHANGED    0x02          /* HW changed bit                   */
#define _PAGE_REFERENCED 0x04          /* HW referenced bit                */

/*
 * No mapping available
 */
#define PAGE_INVALID	  __pgprot(_PAGE_INVALID)
#define PAGE_NONE_SHARED  __pgprot(_PAGE_PRESENT|_PAGE_INVALID)
#define PAGE_NONE_PRIVATE __pgprot(_PAGE_PRESENT|_PAGE_INVALID|_PAGE_ISCLEAN)
#define PAGE_RO_SHARED	  __pgprot(_PAGE_PRESENT|_PAGE_RO)
#define PAGE_RO_PRIVATE	  __pgprot(_PAGE_PRESENT|_PAGE_RO|_PAGE_ISCLEAN)
#define PAGE_COPY	  __pgprot(_PAGE_PRESENT|_PAGE_RO|_PAGE_ISCLEAN)
#define PAGE_SHARED	  __pgprot(_PAGE_PRESENT)
#define PAGE_KERNEL	  __pgprot(_PAGE_PRESENT)

/*
 * The S390 can't do page protection for execute, and considers that the
 * same are read. Also, write permissions imply read permissions. This is
 * the closest we can get..
 */
#define __P000  PAGE_NONE_PRIVATE
#define __P001  PAGE_RO_PRIVATE
#define __P010  PAGE_COPY
#define __P011  PAGE_COPY
#define __P100  PAGE_RO_PRIVATE
#define __P101  PAGE_RO_PRIVATE
#define __P110  PAGE_COPY
#define __P111  PAGE_COPY

#define __S000  PAGE_NONE_SHARED
#define __S001  PAGE_RO_SHARED
#define __S010  PAGE_SHARED
#define __S011  PAGE_SHARED
#define __S100  PAGE_RO_SHARED
#define __S101  PAGE_RO_SHARED
#define __S110  PAGE_SHARED
#define __S111  PAGE_SHARED

/*
 * Certain architectures need to do special things when PTEs
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
extern inline void set_pte(pte_t *pteptr, pte_t pteval)
{
	*pteptr = pteval;
}

#define pages_to_mb(x) ((x) >> (20-PAGE_SHIFT))

/*
 * pgd/pmd/pte query functions
 */
extern inline int __pgd_present(pgd_t *pgd)
{
	unsigned long addr = (unsigned long) pgd;
	unsigned long *pgd_slot = (unsigned long *) (addr & -8);
	unsigned long offset = (addr & 4) >> 1;

	if (*pgd_slot & _PGD_ENTRY_INV)
		return 0;
	if ((*pgd_slot & _PGD_ENTRY_OFF(3)) > _PGD_ENTRY_OFF(offset))
		return 0;
	if ((*pgd_slot & _PGD_ENTRY_LEN(3)) < _PGD_ENTRY_LEN(offset))
		return 0;
	return 1;
}
#define pgd_present(pgd) __pgd_present(&(pgd))

extern inline int __pgd_none(pgd_t *pgd)
{
	return !__pgd_present(pgd);
}
#define pgd_none(pgd) __pgd_none(&(pgd))

extern inline int __pgd_bad(pgd_t *pgd)
{
	unsigned long addr = (unsigned long) pgd;
	unsigned long *pgd_slot = (unsigned long *) (addr & -8);

	return (*pgd_slot & (~PAGE_MASK & ~_PGD_ENTRY_INV & ~_PGD_ENTRY_MASK &
		             ~_PGD_ENTRY_LEN(3) & ~_PGD_ENTRY_OFF(3))) != 0;
}
#define pgd_bad(pgd) __pgd_bad(&(pgd))

extern inline int pmd_present(pmd_t pmd)
{
	return (pmd_val(pmd) & ~PAGE_MASK) == _PMD_ENTRY;
}

extern inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) & _PMD_ENTRY_INV;
}

extern inline int pmd_bad(pmd_t pmd)
{
	return (pmd_val(pmd) & (~PAGE_MASK & ~_PMD_ENTRY_INV)) != _PMD_ENTRY;
}

extern inline int pte_present(pte_t pte)
{
	return pte_val(pte) & _PAGE_PRESENT;
}

extern inline int pte_none(pte_t pte)
{
	return ((pte_val(pte) & 
		 (_PAGE_INVALID | _PAGE_RO | _PAGE_PRESENT)) == _PAGE_INVALID);
} 

#define pte_same(a,b)	(pte_val(a) == pte_val(b))

/*
 * query functions pte_write/pte_dirty/pte_young only work if
 * pte_present() is true. Undefined behaviour if not..
 */
extern inline int pte_write(pte_t pte)
{
	return (pte_val(pte) & _PAGE_RO) == 0;
}

extern inline int pte_dirty(pte_t pte)
{
	int skey;

	if (pte_val(pte) & _PAGE_ISCLEAN)
		return 0;
	asm volatile ("iske %0,%1" : "=d" (skey) : "a" (pte_val(pte)));
	return skey & _PAGE_CHANGED;
}

extern inline int pte_young(pte_t pte)
{
	int skey;

	asm volatile ("iske %0,%1" : "=d" (skey) : "a" (pte_val(pte)));
	return skey & _PAGE_REFERENCED;
}

/*
 * pgd/pmd/pte modification functions
 */
extern inline void pgd_clear(pgd_t * pgdp)
{
	unsigned long addr = (unsigned long) pgdp;
	unsigned long *pgd_slot = (unsigned long *) (addr & -8);
	unsigned long offset = addr & 4;

	if (*pgd_slot & _PGD_ENTRY_INV) {
		*pgd_slot = _PGD_ENTRY_INV;
		return;
	}
	if (offset == 0 && (*pgd_slot & _PGD_ENTRY_LEN(2)) != 0) {
		/* Clear lower pmd, upper pmd still used. */
		*pgd_slot = (*pgd_slot & PAGE_MASK) | _PGD_ENTRY_MASK |
			    _PGD_ENTRY_OFF(2) | _PGD_ENTRY_LEN(3);
		return;
	}
	if (offset == 4 && (*pgd_slot & _PGD_ENTRY_OFF(2)) == 0) {
		/* Clear upped pmd, lower pmd still used. */
		*pgd_slot = (*pgd_slot & PAGE_MASK) | _PGD_ENTRY_MASK |
			    _PGD_ENTRY_OFF(0) | _PGD_ENTRY_LEN(1);
		return;
	}
	*pgd_slot = _PGD_ENTRY_INV;
}

extern inline void pmd_clear(pmd_t * pmdp)
{
	pmd_val(*pmdp) = _PMD_ENTRY_INV | _PMD_ENTRY;
	pmd_val1(*pmdp) = _PMD_ENTRY_INV | _PMD_ENTRY;
}

extern inline void pte_clear(pte_t *ptep)
{
	pte_val(*ptep) = _PAGE_INVALID;
}

#define PTE_INIT(x) pte_clear(x)

/*
 * The following pte_modification functions only work if 
 * pte_present() is true. Undefined behaviour if not..
 */
extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) &= PAGE_MASK | _PAGE_ISCLEAN;
	pte_val(pte) |= pgprot_val(newprot) & ~_PAGE_ISCLEAN;
	return pte; 
}

extern inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) |= _PAGE_RO;
	return pte;
}

extern inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_RO | _PAGE_ISCLEAN);
	return pte;
}

extern inline pte_t pte_mkclean(pte_t pte)
{
	/* The only user of pte_mkclean is the fork() code.
	   We must *not* clear the *physical* page dirty bit
	   just because fork() wants to clear the dirty bit in
	   *one* of the page's mappings.  So we just do nothing. */
	return pte;
}

extern inline pte_t pte_mkdirty(pte_t pte)
{ 
	/* We do not explicitly set the dirty bit because the
	 * sske instruction is slow. It is faster to let the
	 * next instruction set the dirty bit.
	 */
	pte_val(pte) &= ~_PAGE_ISCLEAN;
	return pte;
}

extern inline pte_t pte_mkold(pte_t pte)
{
	asm volatile ("rrbe 0,%0" : : "a" (pte_val(pte)) : "cc" );
	return pte;
}

extern inline pte_t pte_mkyoung(pte_t pte)
{
	/* To set the referenced bit we read the first word from the real
	 * page with a special instruction: load using real address (lura).
	 * Isn't S/390 a nice architecture ?! */
	asm volatile ("lura 0,%0" : : "a" (pte_val(pte) & PAGE_MASK) : "0" );
	return pte;
}

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	int ccode;

	asm volatile ("rrbe 0,%1\n\t"
		      "ipm  %0\n\t"
		      "srl  %0,28\n\t"
		      : "=d" (ccode) : "a" (pte_val(*ptep)) : "cc" );
	return ccode & 2;
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	int skey;

	if (pte_val(*ptep) & _PAGE_ISCLEAN)
		return 0;
	asm volatile ("iske %0,%1" : "=d" (skey) : "a" (*ptep));
	if ((skey & _PAGE_CHANGED) == 0)
		return 0;
	/* We can't clear the changed bit atomically. For now we
         * clear (!) the page referenced bit. */
	asm volatile ("sske %0,%1" 
	              : : "d" (0), "a" (*ptep));
	return 1;
}

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(ptep);
	return pte;
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_wrprotect(old_pte));
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	pte_mkdirty(*ptep);
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
extern inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{
	pte_t __pte;
	pte_val(__pte) = physpage + pgprot_val(pgprot);
	return __pte;
}

#define mk_pte(pg, pgprot)                                                \
({                                                                        \
	struct page *__page = (pg);                                       \
	pgprot_t __pgprot = (pgprot);					  \
	unsigned long __physpage = __pa((__page-mem_map) << PAGE_SHIFT);  \
	pte_t __pte = mk_pte_phys(__physpage, __pgprot);                  \
	__pte;                                                            \
})

#define arch_set_page_uptodate(__page)					  \
	do {								  \
		asm volatile ("sske %0,%1" : : "d" (0),			  \
			      "a" (__pa((__page-mem_map) << PAGE_SHIFT)));\
	} while (0)

#define pte_page(x) (mem_map+(unsigned long)((pte_val(x) >> PAGE_SHIFT)))

#define pmd_page(pmd) \
        ((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

/* to find an entry in a page-table-directory */
#define pgd_index(address) ((address >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

#define pgd_page(pgd) \
        ((unsigned long) __va(__pgd_val(pgd) & PAGE_MASK))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir,addr) \
	((pmd_t *) pgd_page(dir) + (((addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1)))

/* Find an entry in the third-level page table.. */
#define pte_offset(dir,addr) \
	((pte_t *) pmd_page(*(dir)) + (((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1)))

/*
 * A page-table entry has some bits we have to treat in a special way.
 * Bits 52 and bit 55 have to be zero, otherwise an specification
 * exception will occur instead of a page translation exception. The
 * specifiation exception has the bad habit not to store necessary
 * information in the lowcore.
 * Bit 53 and bit 54 are the page invalid bit and the page protection
 * bit. We set both to indicate a swapped page.
 * Bit 63 is used as the software page present bit. If a page is
 * swapped this obviously has to be zero.
 * This leaves the bits 0-51 and bits 56-62 to store type and offset.
 * We use the 7 bits from 56-62 for the type and the 52 bits from 0-51
 * for the offset.
 * |                     offset                       |0110|type |0
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 */
extern inline pte_t mk_swap_pte(unsigned long type, unsigned long offset)
{
	pte_t pte;
	pte_val(pte) = (type << 1) | (offset << 12) | _PAGE_INVALID | _PAGE_RO;
	pte_val(pte) &= 0xfffffffffffff6fe;  /* better to be paranoid */
	return pte;
}

#define SWP_TYPE(entry)		(((entry).val >> 1) & 0x3f)
#define SWP_OFFSET(entry)	((entry).val >> 12)
#define SWP_ENTRY(type,offset)	((swp_entry_t) { pte_val(mk_swap_pte((type),(offset))) })

#define pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)	((pte_t) { (x).val })

#endif /* !__ASSEMBLY__ */

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)          (0)
#define kern_addr_valid(addr)   (1)

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

#endif /* _S390_PAGE_H */

