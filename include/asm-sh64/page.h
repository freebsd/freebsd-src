#ifndef __ASM_SH64_PAGE_H
#define __ASM_SH64_PAGE_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/page.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 * benedict.gaster@superh.com 19th, 24th July 2002.
 * 
 * Modified to take account of enabling for D-CACHE support.
 *
 */

#include <linux/config.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#ifdef __ASSEMBLY__
#define PAGE_SIZE	4096
#else
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PTE_MASK	PAGE_MASK

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

extern void sh64_page_clear(void *page);
extern void sh64_page_copy(void *from, void *to);

#define clear_page(page)		sh64_page_clear(page)
#define copy_page(to,from)		sh64_page_copy(from, to)

#if defined(CONFIG_DCACHE_DISABLED)

#define clear_user_page(page, vaddr)	clear_page(page)
#define copy_user_page(to, from, vaddr)	copy_page(to, from)

#else

extern void clear_user_page(void *to, unsigned long address);
extern void copy_user_page(void *to, void *from, unsigned long address);

#endif /* defined(CONFIG_DCACHE_DISABLED) */

/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)	((x).pte)
#define pmd_val(x)	((x).pmd)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x) ((pte_t) { (x) } )
#define __pmd(x) ((pmd_t) { (x) } )
#define __pgd(x) ((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

/*
 * config.in defined.
 */
#define __MEMORY_START		(CONFIG_MEMORY_START)
#define PAGE_OFFSET		(CONFIG_CACHED_MEMORY_OFFSET)

#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))
#define MAP_NR(addr)		((__pa(addr)-__MEMORY_START) >> PAGE_SHIFT)

#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)
#define phys_to_page(phys)	(mem_map + (((phys)-__MEMORY_START) >> PAGE_SHIFT))
#define virt_to_page(kaddr)  (mem_map + MAP_NR(kaddr))
#define page_to_phys(page)	(((page - mem_map) << PAGE_SHIFT) + __MEMORY_START)


#ifndef __ASSEMBLY__


/*
 * Tell the user there is some problem.
 */
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	asm volatile("nop"); \
} while (0)

#define PAGE_BUG(page) do { \
	BUG(); \
} while (0)

/* Pure 2^n version of get_order */
extern __inline__ int get_order(unsigned long size)
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


#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE | VM_EXEC | \
                                 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif

#endif /* __KERNEL__ */

#endif /* __ASM_SH64_PAGE_H */
