/* CRIS pgtable.h - macros and functions to manipulate page tables
 *
 * HISTORY:
 *
 * $Log: pgtable.h,v $
 * Revision 1.17  2002/12/02 08:14:08  starvik
 * Merge of Linux 2.4.20
 *
 * Revision 1.16  2002/11/20 18:20:17  hp
 * Make all static inline functions extern inline.
 *
 * Revision 1.15  2002/04/23 15:37:52  bjornw
 * Removed page_address and added flush_icache_user_range
 *
 * Revision 1.14  2001/12/10 03:08:50  bjornw
 * Added pgtable_cache_init dummy
 *
 * Revision 1.13  2001/11/12 18:05:38  pkj
 * Added declaration of paging_init().
 *
 * Revision 1.12  2001/08/11 00:28:00  bjornw
 * PAGE_CHG_MASK and PAGE_NONE had somewhat untraditional values
 *
 * Revision 1.11  2001/04/04 14:38:36  bjornw
 * Removed bad_pagetable handling and the _kernel functions
 *
 * Revision 1.10  2001/03/23 07:46:42  starvik
 * Corrected according to review remarks
 *
 * Revision 1.9  2000/11/22 14:57:53  bjornw
 * * extern inline -> static inline
 * * include asm-generic/pgtable.h
 *
 * Revision 1.8  2000/11/21 13:56:16  bjornw
 * Use CONFIG_CRIS_LOW_MAP for the low VM map instead of explicit CPU type
 *
 * Revision 1.7  2000/10/06 15:05:32  bjornw
 * VMALLOC area changed in memory mapping change
 *
 * Revision 1.6  2000/10/04 16:59:14  bjornw
 * Changed comments
 *
 * Revision 1.5  2000/09/13 14:39:53  bjornw
 * New macros
 *
 * Revision 1.4  2000/08/17 15:38:48  bjornw
 * 2.4.0-test6 modifications:
 *   * flush_dcache_page added
 *   * MAP_NR removed
 *   * virt_to_page added
 *
 * Plus some comments and type-clarifications.
 *
 * Revision 1.3  2000/08/15 16:33:35  bjornw
 * pmd_bad should recognize both kernel and user page-tables
 *
 * Revision 1.2  2000/07/10 17:06:01  bjornw
 * Fixed warnings
 *
 * Revision 1.1.1.1  2000/07/10 16:32:31  bjornw
 * CRIS architecture, working draft
 *
 *
 * Revision 1.11  2000/05/29 14:55:56  bjornw
 * Small tweaks of pte_mk routines
 *
 * Revision 1.10  2000/01/27 01:49:06  bjornw
 * * Ooops. The physical frame number in a PTE entry needs to point to the
 *   DRAM directly, not to what the kernel thinks is DRAM (due to KSEG mapping).
 *   Hence we need to strip bit 31 so 0xcXXXXXXX -> 0x4XXXXXXX.
 *
 * Revision 1.9  2000/01/26 16:25:50  bjornw
 * Fixed PAGE_KERNEL bits
 *
 * Revision 1.8  2000/01/23 22:53:22  bjornw
 * Correct flush_tlb_* macros and externs
 *
 * Revision 1.7  2000/01/18 16:22:55  bjornw
 * Use PAGE_MASK instead of PFN_MASK.
 *
 * Revision 1.6  2000/01/17 02:42:53  bjornw
 * Added the pmd_set macro.
 *
 * Revision 1.5  2000/01/16 19:53:42  bjornw
 * Removed VMALLOC_OFFSET. Changed definitions of swapper_pg_dir and zero_page.
 *
 * Revision 1.4  2000/01/14 16:38:20  bjornw
 * PAGE_DIRTY -> PAGE_SILENT_WRITE, removed PAGE_COW from PAGE_COPY.
 *
 * Revision 1.3  1999/12/04 20:12:21  bjornw
 * * PTE bits have moved to asm/mmu.h
 * * Fixed definitions of the higher level page protection bits
 * * Added the pte_* functions, including dirty/accessed SW simulation
 *   (these are exactly the same as for the MIPS port)
 *
 * Revision 1.2  1999/12/04 00:41:54  bjornw
 * * Fixed page table offsets, sizes and shifts
 * * Removed reference to i386 SMP stuff
 * * Added stray comments about Linux/CRIS mm design
 * * Include asm/mmu.h which will contain MMU details
 *
 * Revision 1.1  1999/12/03 15:04:02  bjornw
 * Copied from include/asm-etrax100. For the new CRIS architecture.
 */

#ifndef _CRIS_PGTABLE_H
#define _CRIS_PGTABLE_H

#include <linux/config.h>
#include <asm/mmu.h>

/*
 * The Linux memory management assumes a three-level page table setup. On
 * CRIS, we use that, but "fold" the mid level into the top-level page
 * table. Since the MMU TLB is software loaded through an interrupt, it
 * supports any page table structure, so we could have used a three-level
 * setup, but for the amounts of memory we normally use, a two-level is
 * probably more efficient.
 *
 * This file contains the functions and defines necessary to modify and use
 * the CRIS page table tree.
 */

extern void paging_init(void);

/* The cache doesn't need to be flushed when TLB entries change because 
 * the cache is mapped to physical memory, not virtual memory
 */
#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_range(mm, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr)		do { } while (0)
#define flush_page_to_ram(page)			do { } while (0)
#define flush_dcache_page(page)                 do { } while (0)
#define flush_icache_range(start, end)          do { } while (0)
#define flush_icache_page(vma,pg)               do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

/*
 * TLB flushing (implemented in arch/cris/mm/tlb.c):
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *
 */

extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, 
			   unsigned long addr);
extern void flush_tlb_range(struct mm_struct *mm,
			    unsigned long start,
			    unsigned long end);

extern inline void flush_tlb_pgtables(struct mm_struct *mm,
                                      unsigned long start, unsigned long end)
{
        /* CRIS does not keep any page table caches in TLB */
}


extern inline void flush_tlb(void) 
{
	flush_tlb_mm(current->mm);
}

/* Certain architectures need to do special things when pte's
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
#define set_pte_atomic(pteptr, pteval) ((*(pteptr)) = (pteval))

/*
 * (pmds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)
#define set_pgd(pgdptr, pgdval) (*(pgdptr) = pgdval)

/* PMD_SHIFT determines the size of the area a second-level page table can
 * map. It is equal to the page size times the number of PTE's that fit in
 * a PMD page. A PTE is 4-bytes in CRIS. Hence the following number.
 */

#define PMD_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-2))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a third-level page table entry can map.
 * Since we fold into a two-level structure, this is the same as PMD_SHIFT.
 */

#define PGDIR_SHIFT	PMD_SHIFT
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/*
 * entries per page directory level: we use a two-level, so
 * we don't really have any PMD directory physically.
 * pointers are 4 bytes so we can use the page size and 
 * divide it by 4 (shift by 2).
 */
#define PTRS_PER_PTE	(1UL << (PAGE_SHIFT-2))
#define PTRS_PER_PMD	1
#define PTRS_PER_PGD	(1UL << (PAGE_SHIFT-2))

/* calculate how many PGD entries a user-level program can use
 * the first mappable virtual address is 0
 * (TASK_SIZE is the maximum virtual address space)
 */

#define USER_PTRS_PER_PGD       (TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_PGD_NR       0

/*
 * Kernels own virtual memory area. 
 */

#ifdef CONFIG_CRIS_LOW_MAP
#define VMALLOC_START     KSEG_7
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END       KSEG_8
#else
#define VMALLOC_START     KSEG_D
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END       KSEG_E
#endif

/* Define some higher level generic page attributes. The PTE bits are
 * defined in asm-cris/mmu.h, and these are just combinations of those.
 */

#define __READABLE      (_PAGE_READ | _PAGE_SILENT_READ | _PAGE_ACCESSED)
#define __WRITEABLE     (_PAGE_WRITE | _PAGE_SILENT_WRITE | _PAGE_MODIFIED)

#define _PAGE_TABLE     (_PAGE_PRESENT | __READABLE | __WRITEABLE)
#define _PAGE_CHG_MASK  (PAGE_MASK | _PAGE_ACCESSED | _PAGE_MODIFIED)

#define PAGE_NONE       __pgprot(_PAGE_PRESENT | _PAGE_ACCESSED)
#define PAGE_SHARED     __pgprot(_PAGE_PRESENT | __READABLE | _PAGE_WRITE | \
				 _PAGE_ACCESSED)
#define PAGE_COPY       __pgprot(_PAGE_PRESENT | __READABLE)  // | _PAGE_COW
#define PAGE_READONLY   __pgprot(_PAGE_PRESENT | __READABLE)
#define PAGE_KERNEL     __pgprot(_PAGE_GLOBAL | _PAGE_KERNEL | \
				 _PAGE_PRESENT | __READABLE | __WRITEABLE)
#define _KERNPG_TABLE   (_PAGE_TABLE | _PAGE_KERNEL)

/*
 * CRIS can't do page protection for execute, and considers read the same.
 * Also, write permissions imply read permissions. This is the closest we can
 * get..
 */

#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED

/* zero page used for uninitialized stuff */
extern unsigned long empty_zero_page;
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

/* number of bits that fit into a memory pointer */
#define BITS_PER_PTR			(8*sizeof(unsigned long))

/* to align the pointer to a pointer address */
#define PTR_MASK			(~(sizeof(void*)-1))

/* sizeof(void*)==1<<SIZEOF_PTR_LOG2 */
/* 64-bit machines, beware!  SRB. */
#define SIZEOF_PTR_LOG2			2

/* to find an entry in a page-table */
#define PAGE_PTR(address) \
((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)

/* to set the page-dir */
#define SET_PAGE_DIR(tsk,pgdir)

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(xp)	do { pte_val(*(xp)) = 0; } while (0)

#define pmd_none(x)	(!pmd_val(x))
/* by removing the _PAGE_KERNEL bit from the comparision, the same pmd_bad
 * works for both _PAGE_TABLE and _KERNPG_TABLE pmd entries.
 */
#define	pmd_bad(x)	((pmd_val(x) & (~PAGE_MASK & ~_PAGE_KERNEL)) != _PAGE_TABLE)
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)

/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
extern inline int pgd_none(pgd_t pgd)		{ return 0; }
extern inline int pgd_bad(pgd_t pgd)		{ return 0; }
extern inline int pgd_present(pgd_t pgd)	{ return 1; }
extern inline void pgd_clear(pgd_t * pgdp)	{ }

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */

extern inline int pte_read(pte_t pte)           { return pte_val(pte) & _PAGE_READ; }
extern inline int pte_write(pte_t pte)          { return pte_val(pte) & _PAGE_WRITE; }
extern inline int pte_exec(pte_t pte)           { return pte_val(pte) & _PAGE_READ; }
extern inline int pte_dirty(pte_t pte)          { return pte_val(pte) & _PAGE_MODIFIED; }
extern inline int pte_young(pte_t pte)          { return pte_val(pte) & _PAGE_ACCESSED; }

extern inline pte_t pte_wrprotect(pte_t pte)
{
        pte_val(pte) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
        return pte;
}

extern inline pte_t pte_rdprotect(pte_t pte)
{
        pte_val(pte) &= ~(_PAGE_READ | _PAGE_SILENT_READ);
	return pte;
}

extern inline pte_t pte_exprotect(pte_t pte)
{
        pte_val(pte) &= ~(_PAGE_READ | _PAGE_SILENT_READ);
	return pte;
}

extern inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_MODIFIED | _PAGE_SILENT_WRITE); 
	return pte; 
}

extern inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED | _PAGE_SILENT_READ);
	return pte;
}

extern inline pte_t pte_mkwrite(pte_t pte)
{
        pte_val(pte) |= _PAGE_WRITE;
        if (pte_val(pte) & _PAGE_MODIFIED)
                pte_val(pte) |= _PAGE_SILENT_WRITE;
        return pte;
}

extern inline pte_t pte_mkread(pte_t pte)
{
        pte_val(pte) |= _PAGE_READ;
        if (pte_val(pte) & _PAGE_ACCESSED)
                pte_val(pte) |= _PAGE_SILENT_READ;
        return pte;
}

extern inline pte_t pte_mkexec(pte_t pte)
{
        pte_val(pte) |= _PAGE_READ;
        if (pte_val(pte) & _PAGE_ACCESSED)
                pte_val(pte) |= _PAGE_SILENT_READ;
        return pte;
}

extern inline pte_t pte_mkdirty(pte_t pte)
{
        pte_val(pte) |= _PAGE_MODIFIED;
        if (pte_val(pte) & _PAGE_WRITE)
                pte_val(pte) |= _PAGE_SILENT_WRITE;
        return pte;
}

extern inline pte_t pte_mkyoung(pte_t pte)
{
        pte_val(pte) |= _PAGE_ACCESSED;
        if (pte_val(pte) & _PAGE_READ)
        {
                pte_val(pte) |= _PAGE_SILENT_READ;
                if ((pte_val(pte) & (_PAGE_WRITE | _PAGE_MODIFIED)) ==
		    (_PAGE_WRITE | _PAGE_MODIFIED))
                        pte_val(pte) |= _PAGE_SILENT_WRITE;
        }
        return pte;
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

/* What actually goes as arguments to the various functions is less than
 * obvious, but a rule of thumb is that struct page's goes as struct page *,
 * really physical DRAM addresses are unsigned long's, and DRAM "virtual"
 * addresses (the 0xc0xxxxxx's) goes as void *'s.
 */

extern inline pte_t __mk_pte(void * page, pgprot_t pgprot)
{
	pte_t pte;
	/* the PTE needs a physical address */
	pte_val(pte) = __pa(page) | pgprot_val(pgprot);
	return pte;
}

#define mk_pte(page, pgprot) __mk_pte(page_address(page), (pgprot))

#define mk_pte_phys(physpage, pgprot) \
({                                                                      \
        pte_t __pte;                                                    \
                                                                        \
        pte_val(__pte) = (physpage) + pgprot_val(pgprot);               \
        __pte;                                                          \
})

extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot); return pte; }


/* pte_val refers to a page in the 0x4xxxxxxx physical DRAM interval
 * __pte_page(pte_val) refers to the "virtual" DRAM interval
 * pte_pagenr refers to the page-number counted starting from the virtual DRAM start
 */

extern inline unsigned long __pte_page(pte_t pte)
{
	/* the PTE contains a physical address */
	return (unsigned long)__va(pte_val(pte) & PAGE_MASK);
}

#define pte_pagenr(pte)         ((__pte_page(pte) - PAGE_OFFSET) >> PAGE_SHIFT)

/* permanent address of a page */

#define __page_address(page)    (PAGE_OFFSET + (((page) - mem_map) << PAGE_SHIFT))
#define pte_page(pte)           (mem_map+pte_pagenr(pte))

/* only the pte's themselves need to point to physical DRAM (see above)
 * the pagetable links are purely handled within the kernel SW and thus
 * don't need the __pa and __va transformations.
 */

extern inline unsigned long pmd_page(pmd_t pmd)
{ return pmd_val(pmd) & PAGE_MASK; }

extern inline void pmd_set(pmd_t * pmdp, pte_t * ptep)
{ pmd_val(*pmdp) = _PAGE_TABLE | (unsigned long) ptep; }

/* to find an entry in a page-table-directory. */
#define pgd_index(address) ((address >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

/* to find an entry in a page-table-directory */
extern inline pgd_t * pgd_offset(struct mm_struct * mm, unsigned long address)
{
	return mm->pgd + pgd_index(address);
}

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* Find an entry in the second-level page table.. */
extern inline pmd_t * pmd_offset(pgd_t * dir, unsigned long address)
{
	return (pmd_t *) dir;
}

/* Find an entry in the third-level page table.. */ 
extern inline pte_t * pte_offset(pmd_t * dir, unsigned long address)
{
	return (pte_t *) pmd_page(*dir) + ((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));
}

#define pte_ERROR(e) \
        printk("%s:%d: bad pte %p(%08lx).\n", __FILE__, __LINE__, &(e), pte_val(e))
#define pmd_ERROR(e) \
        printk("%s:%d: bad pmd %p(%08lx).\n", __FILE__, __LINE__, &(e), pmd_val(e))
#define pgd_ERROR(e) \
        printk("%s:%d: bad pgd %p(%08lx).\n", __FILE__, __LINE__, &(e), pgd_val(e))

extern pgd_t swapper_pg_dir[PTRS_PER_PGD]; /* defined in head.S */

/*
 * CRIS doesn't have any external MMU info: the kernel page
 * tables contain all the necessary information.
 * 
 * Actually I am not sure on what this could be used for.
 */
extern inline void update_mmu_cache(struct vm_area_struct * vma,
	unsigned long address, pte_t pte)
{
}

/* Encode and de-code a swap entry (must be !pte_none(e) && !pte_present(e)) */
/* Since the PAGE_PRESENT bit is bit 4, we can use the bits above */

#define SWP_TYPE(x)                     (((x).val >> 5) & 0x7f)
#define SWP_OFFSET(x)                   ((x).val >> 12)
#define SWP_ENTRY(type, offset)         ((swp_entry_t) { ((type) << 5) | ((offset) << 12) })
#define pte_to_swp_entry(pte)           ((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)             ((pte_t) { (x).val })

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)          (0)
#define kern_addr_valid(addr)   (1)

#include <asm-generic/pgtable.h>

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()   do { } while (0)

#endif /* _CRIS_PGTABLE_H */
