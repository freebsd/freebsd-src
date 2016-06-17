#ifndef _PARISC_PAGE_H
#define _PARISC_PAGE_H

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <asm/cache.h>

#define clear_page(page)	memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)      copy_user_page_asm((void *)(to), (void *)(from))

extern void purge_kernel_dcache_page(unsigned long);
extern void copy_user_page_asm(void *to, void *from);
extern void clear_user_page_asm(void *page, unsigned long vaddr);

static inline void
copy_user_page(void *to, void *from, unsigned long vaddr)
{
	copy_user_page_asm(to, from);
	flush_kernel_dcache_page(to);
}

static inline void
clear_user_page(void *page, unsigned long vaddr)
{
	purge_kernel_dcache_page((unsigned long)page);
	clear_user_page_asm(page, vaddr);
}

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

#ifdef __LP64__
#define MAX_PHYSMEM_RANGES 8 /* Fix the size for now (current known max is 3) */
#else
#define MAX_PHYSMEM_RANGES 1 /* First range is only range that fits in 32 bits */
#endif

typedef struct __physmem_range {
	unsigned long start_pfn;
	unsigned long pages;       /* PAGE_SIZE pages */
} physmem_range_t;

extern physmem_range_t pmem_ranges[];
extern int npmem_ranges;

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

/*
 * Tell the user there is some problem. Beep too, so we can
 * see^H^H^Hhear bugs in early bootup as well!
 *
 * We don't beep yet.  prumpf
 */
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
} while (0)

#define PAGE_BUG(page) do { \
	BUG(); \
} while (0)


#define LINUX_GATEWAY_SPACE     0
#define __PAGE_OFFSET           (0x10000000)

#define PAGE_OFFSET		((unsigned long)__PAGE_OFFSET)
/* These macros don't work for 64-bit C code -- don't allow in C at all */
#ifdef __ASSEMBLY__
#   define PA(x)	((x)-__PAGE_OFFSET)
#   define VA(x)	((x)+__PAGE_OFFSET)
#endif
#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))
#ifndef CONFIG_DISCONTIGMEM
#define virt_to_page(kaddr)     (mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)
#endif  /* !CONFIG_DISCONTIGMEM */

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */

#endif /* _PARISC_PAGE_H */
