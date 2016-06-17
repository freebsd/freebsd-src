#ifndef __ASM_SH_PAGE_H
#define __ASM_SH_PAGE_H

/*
 * Copyright (C) 1999  Niibe Yutaka
 */

/*
   [ P0/U0 (virtual) ]		0x00000000     <------ User space
   [ P1 (fixed)   cached ]	0x80000000     <------ Kernel space
   [ P2 (fixed)  non-cachable]	0xA0000000     <------ Physical access
   [ P3 (virtual) cached]	0xC0000000     <------ vmalloced area
   [ P4 control   ]		0xE0000000
 */

#include <linux/config.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PTE_MASK	PAGE_MASK

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

extern void clear_page(void *to);
extern void copy_page(void *to, void *from);

#if defined(__sh3__)
#define clear_user_page(page, vaddr)	clear_page(page)
#define copy_user_page(to, from, vaddr)	copy_page(to, from)
#elif defined(__SH4__)
extern void clear_user_page(void *to, unsigned long address);
extern void copy_user_page(void *to, void *from, unsigned long address);
extern void __clear_user_page(void *to, void *orig_to);
extern void __copy_user_page(void *to, void *from, void *orig_to);
#endif

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

#define __pte(x) ((pte_t) { (x) } )
#define __pmd(x) ((pmd_t) { (x) } )
#define __pgd(x) ((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

/*
 * IF YOU CHANGE THIS, PLEASE ALSO CHANGE
 *
 *	arch/sh/vmlinux.lds.S
 *
 * which has the same constant encoded..
 */

#define __MEMORY_START		CONFIG_MEMORY_START
#define __MEMORY_SIZE		CONFIG_MEMORY_SIZE
#ifdef CONFIG_DISCONTIGMEM
/* Just for HP690, for now.. */
#define __MEMORY_START_2ND	(__MEMORY_START+0x02000000)
#define __MEMORY_SIZE_2ND	0x001000000 /* 16MB */
#endif

#define PAGE_OFFSET		(0x80000000UL)
#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))

#ifndef CONFIG_DISCONTIGMEM
#define phys_to_page(phys)	(mem_map + (((phys)-__MEMORY_START) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)
#define page_to_phys(page)	(((page - mem_map) << PAGE_SHIFT) + __MEMORY_START)
#endif

#define virt_to_page(kaddr)	phys_to_page(__pa(kaddr))

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

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

#endif

#endif /* __KERNEL__ */

#endif /* __ASM_SH_PAGE_H */
