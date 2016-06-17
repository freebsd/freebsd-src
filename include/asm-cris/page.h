#ifndef _CRIS_PAGE_H
#define _CRIS_PAGE_H

#include <linux/config.h>
#include <asm/mmu.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	13
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#ifdef __KERNEL__

#define clear_page(page)        memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)      memcpy((void *)(to), (void *)(from), PAGE_SIZE)

#define clear_user_page(page, vaddr)    clear_page(page)
#define copy_user_page(to, from, vaddr) copy_page(to, from)

#define STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)	((x).pte)
#define pmd_val(x)	((x).pmd)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __pmd(x)	((pmd_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#else
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t;
typedef unsigned long pmd_t;
typedef unsigned long pgd_t;
typedef unsigned long pgprot_t;

#define pte_val(x)	(x)
#define pmd_val(x)	(x)
#define pgd_val(x)	(x)
#define pgprot_val(x)	(x)

#define __pte(x)	(x)
#define __pmd(x)	(x)
#define __pgd(x)	(x)
#define __pgprot(x)	(x)

#endif

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

/* This handles the memory map.. */

#ifdef CONFIG_CRIS_LOW_MAP
#define PAGE_OFFSET		KSEG_6   /* kseg_6 is mapped to physical ram */
#else
#define PAGE_OFFSET		KSEG_C   /* kseg_c is mapped to physical ram */
#endif

#ifndef __ASSEMBLY__

#define BUG() do { \
  printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
} while (0)

#define PAGE_BUG(page) do { \
         BUG(); \
} while (0)

/* Pure 2^n version of get_order */
static __inline__ int get_order(unsigned long size)
{
	int order;

	size = (size-1) >> (PAGE_SHIFT-1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

#endif /* __ASSEMBLY__ */

/* macros to convert between really physical and virtual addresses
 * by stripping a selected bit, we can convert between KSEG_x and 0x40000000 where
 * the DRAM really resides
 */

#ifdef CONFIG_CRIS_LOW_MAP
/* we have DRAM virtually at 0x6 */
#define __pa(x)                 ((unsigned long)(x) & 0xdfffffff)
#define __va(x)                 ((void *)((unsigned long)(x) | 0x20000000))
#else
/* we have DRAM virtually at 0xc */
#define __pa(x)                 ((unsigned long)(x) & 0x7fffffff)
#define __va(x)                 ((void *)((unsigned long)(x) | 0x80000000))
#endif

/* to index into the page map. our pages all start at physical addr PAGE_OFFSET so
 * we can let the map start there. notice that we subtract PAGE_OFFSET because
 * we start our mem_map there - in other ports they map mem_map physically and
 * use __pa instead. in our system both the physical and virtual address of DRAM
 * is too high to let mem_map start at 0, so we do it this way instead (similar
 * to arm and m68k I think)
 */ 

#define virt_to_page(kaddr)    (mem_map + (((unsigned long)kaddr - PAGE_OFFSET) >> PAGE_SHIFT))
#define VALID_PAGE(page)       ((page - mem_map) < max_mapnr)

/* from linker script */

extern unsigned long dram_start, dram_end;

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */

#endif /* _CRIS_PAGE_H */

