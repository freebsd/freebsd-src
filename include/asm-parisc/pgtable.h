#ifndef _PARISC_PGTABLE_H
#define _PARISC_PGTABLE_H

#include <asm/fixmap.h>

#ifndef __ASSEMBLY__
/*
 * we simulate an x86-style page table for the linux mm code
 */

#include <linux/spinlock.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/bitops.h>

#define ARCH_STACK_GROWSUP

/*
 * kern_addr_valid(ADDR) tests if ADDR is pointing to valid kernel
 * memory.  For the return value to be meaningful, ADDR must be >=
 * PAGE_OFFSET.  This operation can be relatively expensive (e.g.,
 * require a hash-, or multi-level tree-lookup or something of that
 * sort) but it guarantees to return TRUE only if accessing the page
 * at that address does not cause an error.  Note that there may be
 * addresses for which kern_addr_valid() returns FALSE even though an
 * access would not cause an error (e.g., this is typically true for
 * memory mapped I/O regions.
 *
 * XXX Need to implement this for parisc.
 */
#define kern_addr_valid(addr)	(1)

/* Certain architectures need to do special things when PTEs
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#define set_pte(pteptr, pteval)                                 \
        do{                                                     \
                *(pteptr) = (pteval);                           \
        } while(0)

#endif /* !__ASSEMBLY__ */

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

 /* Note: If you change ISTACK_SIZE, you need to change the corresponding
  * values in vmlinux.lds and vmlinux64.lds (init_istack section). Also,
  * the "order" and size need to agree.
  */

#define  ISTACK_SIZE  32768 /* Interrupt Stack Size */
#define  ISTACK_ORDER 3

/*
 * NOTE: Many of the below macros use PT_NLEVELS because
 *       it is convenient that PT_NLEVELS == LOG2(pte size in bytes),
 *       i.e. we use 3 level page tables when we use 8 byte pte's
 *       (for 64 bit) and 2 level page tables when we use 4 byte pte's
 */

#ifdef __LP64__
#define PT_NLEVELS 3
#define PT_INITIAL 4 /* Number of initial page tables */
#else
#define PT_NLEVELS 2
#define PT_INITIAL 2 /* Number of initial page tables */
#endif

#define MAX_ADDRBITS (PAGE_SHIFT + (PT_NLEVELS)*(PAGE_SHIFT - PT_NLEVELS))
#define MAX_ADDRESS (1UL << MAX_ADDRBITS)

#define SPACEID_SHIFT (MAX_ADDRBITS - 32)

/* Definitions for 1st level */

#define PGDIR_SHIFT  (PAGE_SHIFT + (PT_NLEVELS - 1)*(PAGE_SHIFT - PT_NLEVELS))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PTRS_PER_PGD    (1UL << (PAGE_SHIFT - PT_NLEVELS))
#define USER_PTRS_PER_PGD       PTRS_PER_PGD

/* Definitions for 2nd level */
#define pgtable_cache_init()	do { } while (0)

#define PMD_SHIFT       (PAGE_SHIFT + (PAGE_SHIFT - PT_NLEVELS))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#if PT_NLEVELS == 3
#define PTRS_PER_PMD    (1UL << (PAGE_SHIFT - PT_NLEVELS))
#else
#define PTRS_PER_PMD    1
#endif

/* Definitions for 3rd level */

#define PTRS_PER_PTE    (1UL << (PAGE_SHIFT - PT_NLEVELS))

/*
 * pgd entries used up by user/kernel:
 */

#define FIRST_USER_PGD_NR	0

#ifndef __ASSEMBLY__
extern  void *vmalloc_start;
#define PCXL_DMA_MAP_SIZE   (8*1024*1024)
#define VMALLOC_START   ((unsigned long)vmalloc_start)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END	(FIXADDR_START)
#endif

/* NB: The tlb miss handlers make certain assumptions about the order */
/*     of the following bits, so be careful (One example, bits 25-31  */
/*     are moved together in one instruction).                        */

#define _PAGE_READ_BIT     31   /* (0x001) read access allowed */
#define _PAGE_WRITE_BIT    30   /* (0x002) write access allowed */
#define _PAGE_EXEC_BIT     29   /* (0x004) execute access allowed */
#define _PAGE_GATEWAY_BIT  28   /* (0x008) privilege promotion allowed */
#define _PAGE_DMB_BIT      27   /* (0x010) Data Memory Break enable (B bit) */
#define _PAGE_DIRTY_BIT    26   /* (0x020) Page Dirty (D bit) */
#define _PAGE_REFTRAP_BIT  25   /* (0x040) Page Ref. Trap enable (T bit) */
#define _PAGE_NO_CACHE_BIT 24   /* (0x080) Uncached Page (U bit) */
#define _PAGE_ACCESSED_BIT 23   /* (0x100) Software: Page Accessed */
#define _PAGE_PRESENT_BIT  22   /* (0x200) Software: translation valid */
#define _PAGE_FLUSH_BIT    21   /* (0x400) Software: translation valid */
				/*             for cache flushing only */
#define _PAGE_USER_BIT     20   /* (0x800) Software: User accessable page */

/* N.B. The bits are defined in terms of a 32 bit word above, so the */
/*      following macro is ok for both 32 and 64 bit.                */

#define xlate_pabit(x) (31 - x)

#define _PAGE_READ     (1 << xlate_pabit(_PAGE_READ_BIT))
#define _PAGE_WRITE    (1 << xlate_pabit(_PAGE_WRITE_BIT))
#define _PAGE_RW       (_PAGE_READ | _PAGE_WRITE)
#define _PAGE_EXEC     (1 << xlate_pabit(_PAGE_EXEC_BIT))
#define _PAGE_GATEWAY  (1 << xlate_pabit(_PAGE_GATEWAY_BIT))
#define _PAGE_DMB      (1 << xlate_pabit(_PAGE_DMB_BIT))
#define _PAGE_DIRTY    (1 << xlate_pabit(_PAGE_DIRTY_BIT))
#define _PAGE_REFTRAP  (1 << xlate_pabit(_PAGE_REFTRAP_BIT))
#define _PAGE_NO_CACHE (1 << xlate_pabit(_PAGE_NO_CACHE_BIT))
#define _PAGE_ACCESSED (1 << xlate_pabit(_PAGE_ACCESSED_BIT))
#define _PAGE_PRESENT  (1 << xlate_pabit(_PAGE_PRESENT_BIT))
#define _PAGE_FLUSH    (1 << xlate_pabit(_PAGE_FLUSH_BIT))
#define _PAGE_USER     (1 << xlate_pabit(_PAGE_USER_BIT))

#define _PAGE_TABLE	(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |  _PAGE_DIRTY | _PAGE_ACCESSED)
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_KERNEL	(_PAGE_PRESENT | _PAGE_EXEC | _PAGE_READ | _PAGE_WRITE | _PAGE_DIRTY | _PAGE_ACCESSED)

#ifndef __ASSEMBLY__

#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_WRITE | _PAGE_ACCESSED)
/* Others seem to make this executable, I don't know if that's correct
   or not.  The stack is mapped this way though so this is necessary
   in the short term - dhd@linuxcare.com, 2000-08-08 */
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_ACCESSED)
#define PAGE_WRITEONLY  __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITE | _PAGE_ACCESSED)
#define PAGE_EXECREAD   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_EXEC |_PAGE_ACCESSED)
#define PAGE_COPY       PAGE_EXECREAD
#define PAGE_RWX        __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_ACCESSED)
#define PAGE_KERNEL	__pgprot(_PAGE_KERNEL)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_PRESENT | _PAGE_EXEC | _PAGE_READ | _PAGE_DIRTY | _PAGE_ACCESSED)
#define PAGE_KERNEL_UNC	__pgprot(_PAGE_KERNEL | _PAGE_NO_CACHE)
#define PAGE_GATEWAY    __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_GATEWAY| _PAGE_READ)
#define PAGE_FLUSH      __pgprot(_PAGE_FLUSH)


/*
 * We could have an execute only page using "gateway - promote to priv
 * level 3", but that is kind of silly. So, the way things are defined
 * now, we must always have read permission for pages with execute
 * permission. For the fun of it we'll go ahead and support write only
 * pages.
 */

	 /*xwr*/
#define __P000  PAGE_NONE
#define __P001  PAGE_READONLY
#define __P010  __P000 /* copy on write */
#define __P011  __P001 /* copy on write */
#define __P100  PAGE_EXECREAD
#define __P101  PAGE_EXECREAD
#define __P110  __P100 /* copy on write */
#define __P111  __P101 /* copy on write */

#define __S000  PAGE_NONE
#define __S001  PAGE_READONLY
#define __S010  PAGE_WRITEONLY
#define __S011  PAGE_SHARED
#define __S100  PAGE_EXECREAD
#define __S101  PAGE_EXECREAD
#define __S110  PAGE_RWX
#define __S111  PAGE_RWX

extern pgd_t swapper_pg_dir[]; /* declared in init_task.c */

/* initial page tables for 0-8MB for kernel */

extern unsigned long pg0[];

/* zero page used for uninitialized stuff */

extern unsigned long *empty_zero_page;

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */

#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#define pte_none(x)     ((pte_val(x) == 0) || (pte_val(x) & _PAGE_FLUSH))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(xp)	do { pte_val(*(xp)) = 0; } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_bad(x)	((pmd_val(x) & ~PAGE_MASK) != _PAGE_TABLE)
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)



#ifdef __LP64__
#define pgd_page(pgd) ((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

/* For 64 bit we have three level tables */

#define pgd_none(x)     (!pgd_val(x))
#define pgd_bad(x)      ((pgd_val(x) & ~PAGE_MASK) != _PAGE_TABLE)
#define pgd_present(x)  (pgd_val(x) & _PAGE_PRESENT)
#define pgd_clear(xp)   do { pgd_val(*(xp)) = 0; } while (0)
#else
/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
extern inline int pgd_none(pgd_t pgd)		{ return 0; }
extern inline int pgd_bad(pgd_t pgd)		{ return 0; }
extern inline int pgd_present(pgd_t pgd)	{ return 1; }
extern inline void pgd_clear(pgd_t * pgdp)	{ }
#endif

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
extern inline int pte_read(pte_t pte)		{ return pte_val(pte) & _PAGE_READ; }
extern inline int pte_dirty(pte_t pte)		{ return pte_val(pte) & _PAGE_DIRTY; }
extern inline int pte_young(pte_t pte)		{ return pte_val(pte) & _PAGE_ACCESSED; }
extern inline int pte_write(pte_t pte)		{ return pte_val(pte) & _PAGE_WRITE; }

extern inline pte_t pte_rdprotect(pte_t pte)	{ pte_val(pte) &= ~_PAGE_READ; return pte; }
extern inline pte_t pte_mkclean(pte_t pte)	{ pte_val(pte) &= ~_PAGE_DIRTY; return pte; }
extern inline pte_t pte_mkold(pte_t pte)	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
extern inline pte_t pte_wrprotect(pte_t pte)	{ pte_val(pte) &= ~_PAGE_WRITE; return pte; }
extern inline pte_t pte_mkread(pte_t pte)	{ pte_val(pte) |= _PAGE_READ; return pte; }
extern inline pte_t pte_mkdirty(pte_t pte)	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
extern inline pte_t pte_mkyoung(pte_t pte)	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
extern inline pte_t pte_mkwrite(pte_t pte)	{ pte_val(pte) |= _PAGE_WRITE; return pte; }

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
#define __mk_pte(addr,pgprot) \
({									\
	pte_t __pte;							\
									\
	pte_val(__pte) = ((addr)+pgprot_val(pgprot));			\
									\
	__pte;								\
})

/* 
 * Change "struct page" to physical address.
 */
#define page_to_phys(page)      PAGE_TO_PA(page)

#ifdef CONFIG_DISCONTIGMEM
#define PAGE_TO_PA(page) \
		((((page)-(page)->zone->zone_mem_map) << PAGE_SHIFT) \
		+ ((page)->zone->zone_start_paddr))
#else
#define PAGE_TO_PA(page) ((page - mem_map) << PAGE_SHIFT)
#endif

#define mk_pte(page, pgprot)						\
({									\
	pte_t __pte;                                                    \
									\
	pte_val(__pte) = ((unsigned long)(PAGE_TO_PA(page))) |		\
						pgprot_val(pgprot);	\
									\
	__pte;								\
})

/* This takes a physical page address that is used by the remapping functions */
#define mk_pte_phys(physpage, pgprot) \
({ pte_t __pte; pte_val(__pte) = physpage + pgprot_val(pgprot); __pte; })

extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot); return pte; }

/* Permanent address of a page.  On parisc we don't have highmem. */

#ifdef CONFIG_DISCONTIGMEM
#define pte_page(x) (phys_to_page(pte_val(x)))
#else
#define pte_page(x) (mem_map+(pte_val(x) >> PAGE_SHIFT))
#endif


#define pmd_page(pmd) ((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#define pgd_index(address) ((address) >> PGDIR_SHIFT)

/* to find an entry in a page-table-directory */
#define pgd_offset(mm, address) \
((mm)->pgd + ((address) >> PGDIR_SHIFT))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* Find an entry in the second-level page table.. */

#ifdef __LP64__
#define pmd_offset(dir,address) \
((pmd_t *) pgd_page(*(dir)) + (((address)>>PMD_SHIFT) & (PTRS_PER_PMD-1)))
#else
#define pmd_offset(dir,addr) ((pmd_t *) dir)
#endif

/* Find an entry in the third-level page table.. */ 
#define pte_offset(pmd, address) \
((pte_t *) pmd_page(*(pmd)) + (((address)>>PAGE_SHIFT) & (PTRS_PER_PTE-1)))

extern void paging_init (void);

/* Used for deferring calls to flush_dcache_page() */

#define PG_dcache_dirty         PG_arch_1

struct vm_area_struct; /* forward declaration (include/linux/mm.h) */
extern void update_mmu_cache(struct vm_area_struct *, unsigned long, pte_t);

/* Encode and de-code a swap entry */

#define SWP_TYPE(x)                     ((x).val & 0x1f)
#define SWP_OFFSET(x)                   ( (((x).val >> 5) &  0xf) | \
					  (((x).val >> 7) & ~0xf) )
#define SWP_ENTRY(type, offset)         ((swp_entry_t) { (type) | \
					    ((offset &  0xf) << 5) | \
					    ((offset & ~0xf) << 7) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
#ifdef CONFIG_SMP
	return test_and_clear_bit(xlate_pabit(_PAGE_ACCESSED_BIT), ptep);
#else
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	set_pte(ptep, pte_mkold(pte));
	return 1;
#endif
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
#ifdef CONFIG_SMP
	return test_and_clear_bit(xlate_pabit(_PAGE_DIRTY_BIT), ptep);
#else
	pte_t pte = *ptep;
	if (!pte_dirty(pte))
		return 0;
	set_pte(ptep, pte_mkclean(pte));
	return 1;
#endif
}

#ifdef CONFIG_SMP
extern spinlock_t pa_dbit_lock;
#else
static int pa_dbit_lock; /* dummy to keep the compilers happy */
#endif

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	pte_t old_pte;
	pte_t pte;

	spin_lock(&pa_dbit_lock);
	pte = old_pte = *ptep;
	pte_val(pte) &= ~_PAGE_PRESENT;
	pte_val(pte) |= _PAGE_FLUSH;
	set_pte(ptep,pte);
	spin_unlock(&pa_dbit_lock);

	return old_pte;
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
#ifdef CONFIG_SMP
	unsigned long new, old;

	do {
		old = pte_val(*ptep);
		new = pte_val(pte_wrprotect(__pte (old)));
	} while (cmpxchg((unsigned long *) ptep, old, new) != old);
#else
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_wrprotect(old_pte));
#endif
}

static inline void ptep_mkdirty(pte_t *ptep)
{
#ifdef CONFIG_SMP
	set_bit(xlate_pabit(_PAGE_DIRTY_BIT), ptep);
#else
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_mkdirty(old_pte));
#endif
}

#define pte_same(A,B)	(pte_val(A) == pte_val(B))


#endif /* !__ASSEMBLY__ */

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)

#define io_remap_page_range remap_page_range

/* We provide our own get_unmapped_area to provide cache coherency */

#define HAVE_ARCH_UNMAPPED_AREA

#endif /* _PARISC_PGTABLE_H */
