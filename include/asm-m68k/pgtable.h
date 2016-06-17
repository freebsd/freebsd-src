#ifndef _M68K_PGTABLE_H
#define _M68K_PGTABLE_H

#include <linux/config.h>
#include <asm/setup.h>

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <linux/threads.h>

/*
 * This file contains the functions and defines necessary to modify and use
 * the m68k page table tree.
 */

#include <asm/virtconvert.h>

/* Certain architectures need to do special things when pte's
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#define set_pte(pteptr, pteval)					\
	do{							\
		*(pteptr) = (pteval);				\
	} while(0)


/* PMD_SHIFT determines the size of the area a second-level page table can map */
#ifdef CONFIG_SUN3
#define PMD_SHIFT       17
#else
#define PMD_SHIFT	22
#endif
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a third-level page table entry can map */
#ifdef CONFIG_SUN3
#define PGDIR_SHIFT     17
#else
#define PGDIR_SHIFT	25
#endif
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/*
 * entries per page directory level: the m68k is configured as three-level,
 * so we do have PMD level physically.
 */
#ifdef CONFIG_SUN3
#define PTRS_PER_PTE   16
#define PTRS_PER_PMD   1
#define PTRS_PER_PGD   2048
#else
#define PTRS_PER_PTE	1024
#define PTRS_PER_PMD	8
#define PTRS_PER_PGD	128
#endif
#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

/* Virtual address region for use by kernel_map() */
#ifdef CONFIG_SUN3
#define KMAP_START     0x0DC00000
#define KMAP_END       0x0E000000
#else
#define	KMAP_START	0xd0000000
#define	KMAP_END	0xf0000000
#endif

#ifndef CONFIG_SUN3
/* Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
#define VMALLOC_OFFSET	(8*1024*1024)
#define VMALLOC_START (((unsigned long) high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END KMAP_START
#else
extern unsigned long vmalloc_end;
#define VMALLOC_START 0x0f800000
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END vmalloc_end
#endif /* CONFIG_SUN3 */

/* zero page used for uninitialized stuff */
extern unsigned long empty_zero_page;

/*
 * BAD_PAGETABLE is used when we need a bogus page-table, while
 * BAD_PAGE is used for a bogus page.
 *
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern pte_t __bad_page(void);
extern pte_t * __bad_pagetable(void);

#define BAD_PAGETABLE __bad_pagetable()
#define BAD_PAGE __bad_page()
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))

/* number of bits that fit into a memory pointer */
#define BITS_PER_PTR			(8*sizeof(unsigned long))

/* to align the pointer to a pointer address */
#define PTR_MASK			(~(sizeof(void*)-1))

/* sizeof(void*)==1<<SIZEOF_PTR_LOG2 */
/* 64-bit machines, beware!  SRB. */
#define SIZEOF_PTR_LOG2			       2

/*
 * Check if the addr/len goes up to the end of a physical
 * memory chunk.  Used for DMA functions.
 */
#ifdef CONFIG_SINGLE_MEMORY_CHUNK
/*
 * It makes no sense to consider whether we cross a memory boundary if
 * we support just one physical chunk of memory.
 */
static inline int mm_end_of_chunk(unsigned long addr, int len)
{
	return 0;
}
#else
int mm_end_of_chunk (unsigned long addr, int len);
#endif

extern void kernel_set_cachemode(void *addr, unsigned long size, int cmode);

/*
 * The m68k doesn't have any external MMU info: the kernel page
 * tables contain all the necessary information.  The Sun3 does, but
 * they are updated on demand.
 */
static inline void update_mmu_cache(struct vm_area_struct *vma,
				    unsigned long address, pte_t pte)
{
}

#ifdef CONFIG_SUN3
/* Macros to (de)construct the fake PTEs representing swap pages. */
#define SWP_TYPE(x)                ((x).val & 0x7F)
#define SWP_OFFSET(x)      (((x).val) >> 7)
#define SWP_ENTRY(type,offset) ((swp_entry_t) { ((type) | ((offset) << 7)) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

#else

/* Encode and de-code a swap entry (must be !pte_none(e) && !pte_present(e)) */
#define SWP_TYPE(x)			(((x).val >> 3) & 0x1ff)
#define SWP_OFFSET(x)			((x).val >> 12)
#define SWP_ENTRY(type, offset)		((swp_entry_t) { ((type) << 3) | ((offset) << 12) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

#endif /* CONFIG_SUN3 */

#endif /* !__ASSEMBLY__ */

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range remap_page_range

/* MMU-specific headers */

#ifdef CONFIG_SUN3
#include <asm/sun3_pgtable.h>
#else
#include <asm/motorola_pgtable.h>
#endif

#ifndef __ASSEMBLY__
#include <asm-generic/pgtable.h>
#endif /* !__ASSEMBLY__ */

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

#endif /* _M68K_PGTABLE_H */
