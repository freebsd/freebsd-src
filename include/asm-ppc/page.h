#ifndef _PPC_PAGE_H
#define _PPC_PAGE_H

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
/*
 * Subtle: this is an int (not an unsigned long) and so it
 * gets extended to 64 bits the way want (i.e. with 1s).  -- paulus
 */
#define PAGE_MASK	(~((1 << PAGE_SHIFT) - 1))

#ifdef __KERNEL__
#include <linux/config.h>

#define PAGE_OFFSET	CONFIG_KERNEL_START
#define KERNELBASE	PAGE_OFFSET

#ifndef __ASSEMBLY__
/*
 * The basic type of a PTE - 64 bits for those CPUs with > 32 bit
 * physical addressing.  For now this just the IBM PPC440.
 */
#ifdef CONFIG_PTE_64BIT
typedef unsigned long long pte_basic_t;
#define PTE_SHIFT	(PAGE_SHIFT - 3)	/* 512 ptes per page */
#define PTE_FMT		"%16Lx"
#else
typedef unsigned long pte_basic_t;
#define PTE_SHIFT	(PAGE_SHIFT - 2)	/* 1024 ptes per page */
#define PTE_FMT		"%.8lx"
#endif

#include <asm/system.h> /* for xmon definition */

#ifdef CONFIG_XMON
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	xmon(0); \
} while (0)
#else
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	__asm__ __volatile__(".long 0x0"); \
} while (0)
#endif
#define PAGE_BUG(page) do { BUG(); } while (0)

#define STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { pte_basic_t pte; } pte_t;
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


/* align addr on a size boundry - adjust address up if needed -- Cort */
#define _ALIGN(addr,size)	(((addr)+(size)-1)&(~((size)-1)))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

extern void clear_page(void *page);
extern void copy_page(void *to, void *from);
extern void clear_user_page(void *page, unsigned long vaddr);
extern void copy_user_page(void *to, void *from, unsigned long vaddr);

extern unsigned long ppc_memstart;
extern unsigned long ppc_memoffset;
#ifndef CONFIG_APUS
#define PPC_MEMSTART	0
#define PPC_MEMOFFSET	PAGE_OFFSET
#else
#define PPC_MEMSTART	ppc_memstart
#define PPC_MEMOFFSET	ppc_memoffset
#endif

#if defined(CONFIG_APUS) && !defined(MODULE)
/* map phys->virtual and virtual->phys for RAM pages */
static inline unsigned long ___pa(unsigned long v)
{
	unsigned long p;
	asm volatile ("1: addis %0, %1, %2;"
		      ".section \".vtop_fixup\",\"aw\";"
		      ".align  1;"
		      ".long   1b;"
		      ".previous;"
		      : "=r" (p)
		      : "b" (v), "K" (((-PAGE_OFFSET) >> 16) & 0xffff));

	return p;
}
static inline void* ___va(unsigned long p)
{
	unsigned long v;
	asm volatile ("1: addis %0, %1, %2;"
		      ".section \".ptov_fixup\",\"aw\";"
		      ".align  1;"
		      ".long   1b;"
		      ".previous;"
		      : "=r" (v)
		      : "b" (p), "K" (((PAGE_OFFSET) >> 16) & 0xffff));

	return (void*) v;
}
#else
#define ___pa(vaddr) ((vaddr)-PPC_MEMOFFSET)
#define ___va(paddr) ((paddr)+PPC_MEMOFFSET)
#endif

#define __pa(x) ___pa((unsigned long)(x))
#define __va(x) ((void *)(___va((unsigned long)(x))))

#define MAP_PAGE_RESERVED	(1<<15)
#define virt_to_page(kaddr)	(mem_map + (((unsigned long)(kaddr)-PAGE_OFFSET) >> PAGE_SHIFT))
#define VALID_PAGE(page)	(((page) - mem_map) < max_mapnr)

extern unsigned long get_zero_page_fast(void);

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

#endif /* __ASSEMBLY__ */

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */
#endif /* _PPC_PAGE_H */
