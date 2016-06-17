/* $Id: page.h,v 1.55 2000/10/30 21:01:41 davem Exp $
 * page.h:  Various defines and such for MMU operations on the Sparc for
 *          the Linux kernel.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_PAGE_H
#define _SPARC_PAGE_H

#include <linux/config.h>
#ifdef CONFIG_SUN4
#define PAGE_SHIFT   13
#else
#define PAGE_SHIFT   12
#endif
#ifndef __ASSEMBLY__
/* I have my suspicions... -DaveM */
#define PAGE_SIZE    (1UL << PAGE_SHIFT)
#else
#define PAGE_SIZE    (1 << PAGE_SHIFT)
#endif
#define PAGE_MASK    (~(PAGE_SIZE-1))

#ifdef __KERNEL__

#include <asm/head.h>       /* for KERNBASE */
#include <asm/btfixup.h>

/* This is always 2048*sizeof(long), doesn't change with PAGE_SIZE */
#define TASK_UNION_SIZE		8192

#ifndef __ASSEMBLY__

/*
 * XXX I am hitting compiler bugs with __builtin_trap. This has
 * hit me before and rusty was blaming his netfilter bugs on
 * this so lets disable it. - Anton
 */
#if 0
/* We need the mb()'s so we don't trigger a compiler bug - Anton */
#define BUG() do { \
	mb(); \
	__builtin_trap(); \
	mb(); \
} while(0)
#else
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); *(int *)0=0; \
} while (0)
#endif

#define PAGE_BUG(page)	BUG()

#define clear_page(page)	 memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from) 	memcpy((void *)(to), (void *)(from), PAGE_SIZE)
#define clear_user_page(page, vaddr)	clear_page(page)
#define copy_user_page(to, from, vaddr)	copy_page(to, from)

/* The following structure is used to hold the physical
 * memory configuration of the machine.  This is filled in
 * probe_memory() and is later used by mem_init() to set up
 * mem_map[].  We statically allocate SPARC_PHYS_BANKS of
 * these structs, this is arbitrary.  The entry after the
 * last valid one has num_bytes==0.
 */

struct sparc_phys_banks {
  unsigned long base_addr;
  unsigned long num_bytes;
};

#define SPARC_PHYS_BANKS 32

extern struct sparc_phys_banks sp_banks[SPARC_PHYS_BANKS];

/* Cache alias structure.  Entry is valid if context != -1. */
struct cache_palias {
	unsigned long vaddr;
	int context;
};

extern struct cache_palias *sparc_aliases;

/* passing structs on the Sparc slow us down tremendously... */

/* #define STRICT_MM_TYPECHECKS */

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long iopte; } iopte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long ctxd; } ctxd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct { unsigned long iopgprot; } iopgprot_t;

#define pte_val(x)	((x).pte)
#define iopte_val(x)	((x).iopte)
#define pmd_val(x)      ((x).pmd)
#define pgd_val(x)	((x).pgd)
#define ctxd_val(x)	((x).ctxd)
#define pgprot_val(x)	((x).pgprot)
#define iopgprot_val(x)	((x).iopgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __iopte(x)	((iopte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __ctxd(x)	((ctxd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )
#define __iopgprot(x)	((iopgprot_t) { (x) } )

#else
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t;
typedef unsigned long iopte_t;
typedef unsigned long pmd_t;
typedef unsigned long pgd_t;
typedef unsigned long ctxd_t;
typedef unsigned long pgprot_t;
typedef unsigned long iopgprot_t;

#define pte_val(x)	(x)
#define iopte_val(x)	(x)
#define pmd_val(x)      (x)
#define pgd_val(x)	(x)
#define ctxd_val(x)	(x)
#define pgprot_val(x)	(x)
#define iopgprot_val(x)	(x)

#define __pte(x)	(x)
#define __iopte(x)	(x)
#define __pmd(x)        (x)
#define __pgd(x)	(x)
#define __ctxd(x)	(x)
#define __pgprot(x)	(x)
#define __iopgprot(x)	(x)

#endif

extern unsigned long sparc_unmapped_base;

BTFIXUPDEF_SETHI(sparc_unmapped_base)

#define TASK_UNMAPPED_BASE	BTFIXUP_SETHI(sparc_unmapped_base)

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

#else /* !(__ASSEMBLY__) */

#define __pgprot(x)	(x)

#endif /* !(__ASSEMBLY__) */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)  (((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define PAGE_OFFSET	0xf0000000
#define __pa(x)                 ((unsigned long)(x) - PAGE_OFFSET)
#define __va(x)                 ((void *)((unsigned long) (x) + PAGE_OFFSET))
#define virt_to_page(kaddr)	(mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */

#endif /* _SPARC_PAGE_H */
