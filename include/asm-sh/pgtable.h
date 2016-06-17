#ifndef __ASM_SH_PGTABLE_H
#define __ASM_SH_PGTABLE_H

/* Copyright (C) 1999 Niibe Yutaka */

#include <asm/pgtable-2level.h>

/*
 * This file contains the functions and defines necessary to modify and use
 * the SuperH page table tree.
 */
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/addrspace.h>
#include <linux/threads.h>

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void paging_init(void);

#if defined(__sh3__)
/* Cache flushing:
 *
 *  - flush_cache_all() flushes entire cache
 *  - flush_cache_mm(mm) flushes the specified mm context's cache lines
 *  - flush_cache_page(mm, vmaddr) flushes a single page
 *  - flush_cache_range(mm, start, end) flushes a range of pages
 *
 *  - flush_dcache_page(pg) flushes(wback&invalidates) a page for dcache
 *  - flush_page_to_ram(page) write back kernel page to ram
 *  - flush_icache_range(start, end) flushes(invalidates) a range for icache
 *  - flush_icache_page(vma, pg) flushes(invalidates) a page for icache
 *
 *  Caches are indexed (effectively) by physical address on SH-3, so
 *  we don't need them.
 */
#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_range(mm, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr)		do { } while (0)
#define flush_page_to_ram(page)			do { } while (0)
#define flush_dcache_page(page)			do { } while (0)
#define flush_icache_range(start, end)		do { } while (0)
#define flush_icache_page(vma,pg)		do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)
#define flush_cache_sigtramp(vaddr)		do { } while (0)
#define __flush_icache_all()			do { } while (0)

#define p3_cache_init()				do { } while (0)

#elif defined(__SH4__)
/*
 *  Caches are broken on SH-4, so we need them.
 */

/* Page is 4K, OC size is 16K, there are four lines. */
#define CACHE_ALIAS 0x00003000

extern void flush_cache_all(void);
extern void flush_cache_mm(struct mm_struct *mm);
extern void flush_cache_range(struct mm_struct *mm, unsigned long start,
			      unsigned long end);
extern void flush_cache_page(struct vm_area_struct *vma, unsigned long addr);
extern void flush_dcache_page(struct page *pg);
extern void flush_icache_range(unsigned long start, unsigned long end);
extern void flush_cache_sigtramp(unsigned long addr);

#define flush_page_to_ram(page)			do { } while (0)
#define flush_icache_page(vma,pg)		do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

/* Initialization of P3 area for copy_user_page */
extern void p3_cache_init(void);

#define PG_mapped	PG_arch_1

/* We provide our own get_unmapped_area to avoid cache alias issue */
#define HAVE_ARCH_UNMAPPED_AREA
#endif

/* Flush (write-back only) a region (smaller than a page) */
extern void __flush_wback_region(void *start, int size);
/* Flush (write-back & invalidate) a region (smaller than a page) */
extern void __flush_purge_region(void *start, int size);
/* Flush (invalidate only) a region (smaller than a page) */
extern void __flush_invalidate_region(void *start, int size);


/*
 * Basically we have the same two-level (which is the logical three level
 * Linux page table layout folded) page tables as the i386.
 */

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern unsigned long empty_zero_page[1024];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#endif /* !__ASSEMBLY__ */

#define __beep() asm("")

#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#define PTE_PHYS_MASK	0x1ffff000

#ifndef __ASSEMBLY__
/*
 * First 1MB map is used by fixed purpose.
 * Currently only 4-enty (16kB) is used (see arch/sh/mm/cache.c)
 */
#define VMALLOC_START	(P3SEG+0x00100000)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END	P4SEG

/*			0x001     WT-bit on SH-4, 0 on SH-3 */
#define _PAGE_HW_SHARED	0x002  /* SH-bit  : page is shared among processes */
#define _PAGE_DIRTY	0x004  /* D-bit   : page changed */
#define _PAGE_CACHABLE	0x008  /* C-bit   : cachable */
/*			0x010     SZ0-bit : Size of page */
#define _PAGE_RW	0x020  /* PR0-bit : write access allowed */
#define _PAGE_USER	0x040  /* PR1-bit : user space access allowed */
/*			0x080     SZ1-bit : Size of page (on SH-4) */
#define _PAGE_PRESENT	0x100  /* V-bit   : page is valid */
#define _PAGE_PROTNONE	0x200  /* software: if not present  */
#define _PAGE_ACCESSED 	0x400  /* software: page referenced */
#define _PAGE_U0_SHARED 0x800  /* software: page is shared in user space */


/* software: moves to PTEA.TC (Timing Control) */
#define _PAGE_PCC_AREA5	0x00000000	/* use BSC registers for area5 */
#define _PAGE_PCC_AREA6	0x80000000	/* use BSC registers for area6 */

/* software: moves to PTEA.SA[2:0] (Space Attributes) */
#define _PAGE_PCC_IODYN 0x00000001	/* IO space, dynamically sized bus */
#define _PAGE_PCC_IO8	0x20000000	/* IO space, 8 bit bus */
#define _PAGE_PCC_IO16	0x20000001	/* IO space, 16 bit bus */
#define _PAGE_PCC_COM8	0x40000000	/* Common Memory space, 8 bit bus */
#define _PAGE_PCC_COM16	0x40000001	/* Common Memory space, 16 bit bus */
#define _PAGE_PCC_ATR8	0x60000000	/* Attribute Memory space, 8 bit bus */
#define _PAGE_PCC_ATR16	0x60000001	/* Attribute Memory space, 6 bit bus */


/* Mask which drop software flags */
#if defined(__sh3__)
/*
 * MMU on SH-3 has bug on SH-bit: We can't use it if MMUCR.IX=1.
 * Work around: Just drop SH-bit.
 */
#define _PAGE_FLAGS_HARDWARE_MASK	0x1ffff1fc
#else
#define _PAGE_FLAGS_HARDWARE_MASK	0x1ffff1fe
#endif
/* Hardware flags: SZ=1 (4k-byte) */
#define _PAGE_FLAGS_HARD		0x00000010

#define _PAGE_SHARED	_PAGE_U0_SHARED

#define _PAGE_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _KERNPG_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_CHG_MASK	(PTE_MASK | _PAGE_ACCESSED | _PAGE_CACHABLE | _PAGE_DIRTY | _PAGE_SHARED)

#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_CACHABLE |_PAGE_ACCESSED | _PAGE_FLAGS_HARD)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_CACHABLE |_PAGE_ACCESSED | _PAGE_SHARED | _PAGE_FLAGS_HARD)
#define PAGE_COPY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_CACHABLE | _PAGE_ACCESSED | _PAGE_FLAGS_HARD)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_CACHABLE | _PAGE_ACCESSED | _PAGE_FLAGS_HARD)
#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_CACHABLE | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_HW_SHARED | _PAGE_FLAGS_HARD)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_PRESENT | _PAGE_CACHABLE | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_HW_SHARED | _PAGE_FLAGS_HARD)
#define PAGE_KERNEL_PCC(slot, type) \
			__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_FLAGS_HARD | (slot ? _PAGE_PCC_AREA5 : _PAGE_PCC_AREA6) | (type))

/*
 * As i386 and MIPS, SuperH can't do page protection for execute, and
 * considers that the same as a read.  Also, write permissions imply
 * read permissions. This is the closest we can get..  
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

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & (_PAGE_PRESENT | _PAGE_PROTNONE))
#define pte_clear(xp)	do { set_pte(xp, __pte(0)); } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { set_pmd(xp, __pmd(0)); } while (0)
#define	pmd_bad(x)	((pmd_val(x) & (~PAGE_MASK & ~_PAGE_USER)) != _KERNPG_TABLE)

#define pages_to_mb(x)	((x) >> (20-PAGE_SHIFT))
#define pte_page(x) 	phys_to_page(pte_val(x)&PTE_PHYS_MASK)

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte) { return pte_val(pte) & _PAGE_USER; }
static inline int pte_exec(pte_t pte) { return pte_val(pte) & _PAGE_USER; }
static inline int pte_dirty(pte_t pte){ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte){ return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_write(pte_t pte){ return pte_val(pte) & _PAGE_RW; }
static inline int pte_not_present(pte_t pte){ return !(pte_val(pte) & _PAGE_PRESENT); }

static inline pte_t pte_rdprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_USER)); return pte; }
static inline pte_t pte_exprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_USER)); return pte; }
static inline pte_t pte_mkclean(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_DIRTY)); return pte; }
static inline pte_t pte_mkold(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_ACCESSED)); return pte; }
static inline pte_t pte_wrprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_RW)); return pte; }
static inline pte_t pte_mkread(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_USER)); return pte; }
static inline pte_t pte_mkexec(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_USER)); return pte; }
static inline pte_t pte_mkdirty(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_DIRTY)); return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_ACCESSED)); return pte; }
static inline pte_t pte_mkwrite(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_RW)); return pte; }

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 *
 * extern pte_t mk_pte(struct page *page, pgprot_t pgprot)
 */
#define mk_pte(page,pgprot)						\
({	pte_t __pte;							\
									\
	set_pte(&__pte, __pte(PHYSADDR(page_address(page))		\
				+pgprot_val(pgprot)));			\
	__pte;								\
})

/* This takes a physical page address that is used by the remapping functions */
#define mk_pte_phys(physpage, pgprot) \
({ pte_t __pte; set_pte(&__pte, __pte(physpage + pgprot_val(pgprot))); __pte; })

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ set_pte(&pte, __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot))); return pte; }

#define page_pte(page) page_pte_prot(page, __pgprot(0))

#define pmd_page(pmd) \
((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

/* to find an entry in a page-table-directory. */
#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define __pgd_offset(address) pgd_index(address)
#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* Find an entry in the third-level page table.. */
#define __pte_offset(address) \
		((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, address) ((pte_t *) pmd_page(*(dir)) + \
			__pte_offset(address))

extern void update_mmu_cache(struct vm_area_struct * vma,
			     unsigned long address, pte_t pte);

/* Encode and de-code a swap entry */
/*
 * NOTE: We should set ZEROs at the position of _PAGE_PRESENT
 *       and _PAGE_PROTONOE bits
 */
#define SWP_TYPE(x)		((x).val & 0xff)
#define SWP_OFFSET(x)		((x).val >> 10)
#define SWP_ENTRY(type, offset)	((swp_entry_t) { (type) | ((offset) << 10) })
#define pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)	((pte_t) { (x).val })

/*
 * Routines for update of PTE 
 *
 * We just can use generic implementation, as SuperH has no SMP feature.
 * (We needed atomic implementation for SMP)
 *
 */

#define pte_same(A,B)	(pte_val(A) == pte_val(B))

#endif /* !__ASSEMBLY__ */

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range remap_page_range

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

/*
 * Set pg flags to non-cached
 */
#define pgprot_noncached(_prot) __pgprot(pgprot_val(_prot) &= ~_PAGE_CACHABLE)


#endif /* __ASM_SH_PAGE_H */
