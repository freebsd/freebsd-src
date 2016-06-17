/*
 * Definitions for page handling
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2003 by Ralf Baechle
 */
#ifndef __ASM_PAGE_H
#define __ASM_PAGE_H

#include <linux/config.h>
#include <asm/break.h>

#ifdef __KERNEL__

/*
 * PAGE_SHIFT determines the page size
 */
#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT	12
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT	14
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT	16
#endif
#define PAGE_SIZE	(1L << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#ifndef __ASSEMBLY__

#include <asm/cacheflush.h>

#define BUG()								\
do {									\
	__asm__ __volatile__("break %0" : : "i" (BRK_BUG));		\
} while (0)

#define PAGE_BUG(page) do {  BUG(); } while (0)

extern void clear_page(void * page);
extern void copy_page(void * to, void * from);

extern unsigned long shm_align_mask;

static inline unsigned long pages_do_alias(unsigned long addr1,
	unsigned long addr2)
{
	return (addr1 ^ addr2) & shm_align_mask;
}

static inline void clear_user_page(void *page, unsigned long vaddr)
{
	unsigned long kaddr = (unsigned long) page;

	clear_page(page);
	if (pages_do_alias(kaddr, vaddr))
		flush_data_cache_page(kaddr);
}

static inline void copy_user_page(void * to, void * from, unsigned long vaddr)
{
	unsigned long kto = (unsigned long) to;

	copy_page(to, from);
	if (pages_do_alias(kto, vaddr))
		flush_data_cache_page(kto);
}

/*
 * These are used to make use of C type-checking..
 */
#ifdef CONFIG_64BIT_PHYS_ADDR
  #ifdef CONFIG_CPU_MIPS32
    typedef struct { unsigned long pte_low, pte_high; } pte_t;
    #define pte_val(x)    ((x).pte_low | ((unsigned long long)(x).pte_high << 32))
  #else
    typedef struct { unsigned long long pte_low; } pte_t;
    #define pte_val(x)    ((x).pte_low)
  #endif
#else
typedef struct { unsigned long pte_low; } pte_t;
#define pte_val(x)    ((x).pte_low)
#endif

typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pmd_val(x)	((x).pmd)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define ptep_buddy(x)	((pte_t *)((unsigned long)(x) ^ sizeof(pte_t)))

#define __pte(x)	((pte_t) { (x) } )
#define __pmd(x)	((pmd_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

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

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr) + PAGE_SIZE - 1) & PAGE_MASK)

/*
 * This handles the memory map.
 * We handle pages at KSEG0 for kernels with 32 bit address space.
 */
#define PAGE_OFFSET	0x80000000UL
#define UNCAC_BASE	0xa0000000UL

#define __pa(x)		((unsigned long) (x) - PAGE_OFFSET)
#define __va(x)		((void *)((unsigned long) (x) + PAGE_OFFSET))
#define virt_to_page(kaddr)	(mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#define VM_DATA_DEFAULT_FLAGS  (VM_READ | VM_WRITE | VM_EXEC | \
				VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define UNCAC_ADDR(addr)	((addr) - PAGE_OFFSET + UNCAC_BASE)
#define CAC_ADDR(addr)		((addr) - UNCAC_BASE + PAGE_OFFSET)

/*
 * Memory above this physical address will be considered highmem.
 */
#define HIGHMEM_START	0x20000000UL

#endif /* defined (__KERNEL__) */

#endif /* __ASM_PAGE_H */
