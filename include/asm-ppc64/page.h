#ifndef _PPC64_PAGE_H
#define _PPC64_PAGE_H

/*
 * Copyright (C) 2001 PPC64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#ifndef __ASSEMBLY__
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#else
# define PAGE_SIZE	(1 << PAGE_SHIFT)
#endif
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PAGE_OFFSET_MASK (PAGE_SIZE-1)

#define SID_SHIFT       28
#define SID_MASK        0xfffffffff
#define GET_ESID(x)     (((x) >> SID_SHIFT) & SID_MASK)

/* Define an illegal instr to trap on the bug.
 * We don't use 0 because that marks the end of a function
 * in the ELF ABI.  That's "Boo Boo" in case you wonder...
 */
#define BUG_OPCODE .long 0x00b00b00  /* For asm */
#define BUG_ILLEGAL_INSTR "0x00b00b00" /* For BUG macro */

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <asm/naca.h>
#include <asm/systemcfg.h>

#define STRICT_MM_TYPECHECKS

#define REGION_SIZE   4UL
#define OFFSET_SIZE   60UL
#define REGION_SHIFT  60UL
#define OFFSET_SHIFT  0UL
#define REGION_MASK   (((1UL<<REGION_SIZE)-1UL)<<REGION_SHIFT)
#define REGION_STRIDE (1UL << REGION_SHIFT)

#ifdef ___powerpc64__
typedef union ppc64_va {
        struct {
                unsigned long off : OFFSET_SIZE;  /* intra-region offset */
                unsigned long reg : REGION_SIZE;  /* region number */
        } f;
        unsigned long l;
        void *p;
} ppc64_va;
#endif /* ___powerpc64__ */
       
static __inline__ void clear_page(void *addr)
{
	unsigned long lines, line_size;

	line_size = systemcfg->dCacheL1LineSize; 
	lines = naca->dCacheL1LinesPerPage;

	__asm__ __volatile__(
"  	mtctr  	%1\n\
1:      dcbz  	0,%0\n\
	add	%0,%0,%3\n\
	bdnz+	1b"
        : "=r" (addr)
        : "r" (lines), "0" (addr), "r" (line_size)
	: "ctr", "memory");
}

extern void copy_page(void *to, void *from);
struct page;
extern void clear_user_page(void *page, unsigned long vaddr);
extern void copy_user_page(void *to, void *from, unsigned long vaddr);

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking.  
 * Entries in the pte table are 64b, while entries in the pgd & pmd are 32b.
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned int  pmd; } pmd_t;
typedef struct { unsigned int  pgd; } pgd_t;
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
typedef unsigned int  pmd_t;
typedef unsigned int  pgd_t;
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

#ifdef CONFIG_XMON
#include <asm/ptrace.h>
extern void xmon(struct pt_regs *excp);
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	xmon(0); \
} while (0)
#else
#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	__asm__ __volatile__(".long " BUG_ILLEGAL_INSTR); \
} while (0)
#endif

#define PAGE_BUG(page) do { BUG(); } while (0)

/* Pure 2^n version of get_order */
static inline int get_order(unsigned long size)
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

#define __pa(x) ((unsigned long)(x)-PAGE_OFFSET)

#endif /* __ASSEMBLY__ */

/* align addr on a size boundry - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

/* align addr on a size boundry - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)

/* to align the pointer to the (next) double word boundary */
#define DOUBLEWORD_ALIGN(addr)	_ALIGN(addr,sizeof(unsigned long))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	_ALIGN(addr, PAGE_SIZE)

#ifdef MODULE
#define __page_aligned __attribute__((__aligned__(PAGE_SIZE)))
#else
#define __page_aligned \
	__attribute__((__aligned__(PAGE_SIZE), \
		__section__(".data.page_aligned")))
#endif


/* This must match the -Ttext linker address            */
/* Note: tophys & tovirt make assumptions about how     */
/*       KERNELBASE is defined for performance reasons. */
/*       When KERNELBASE moves, those macros may have   */
/*             to change!                               */
#define PAGE_OFFSET     0xC000000000000000
#define KERNELBASE      PAGE_OFFSET
#define VMALLOCBASE     0xD000000000000000
#define IOREGIONBASE    0xE000000000000000
#define EEHREGIONBASE   0xA000000000000000
#define BOLTEDBASE      0xB000000000000000

#define IO_REGION_ID       (IOREGIONBASE>>REGION_SHIFT)
#define EEH_REGION_ID      (EEHREGIONBASE>>REGION_SHIFT)
#define VMALLOC_REGION_ID  (VMALLOCBASE>>REGION_SHIFT)
#define KERNEL_REGION_ID   (KERNELBASE>>REGION_SHIFT)
#define BOLTED_REGION_ID   (BOLTEDBASE>>REGION_SHIFT)
#define USER_REGION_ID     (0UL)
#define REGION_ID(X)	   (((unsigned long)(X))>>REGION_SHIFT)

/*
 * Define valid/invalid EA bits (for all ranges)
 */
#define VALID_EA_BITS   (0x000001ffffffffffUL)
#define INVALID_EA_BITS (~(REGION_MASK|VALID_EA_BITS))

#define IS_VALID_REGION_ID(x) \
        (((x) == USER_REGION_ID) || ((x) >= BOLTED_REGION_ID))
#define IS_VALID_EA(x) \
        ((!((x) & INVALID_EA_BITS)) && IS_VALID_REGION_ID(REGION_ID(x)))

#define __bpn_to_ba(x) ((((unsigned long)(x))<<PAGE_SHIFT) + KERNELBASE)
#define __ba_to_bpn(x) ((((unsigned long)(x)) & ~REGION_MASK) >> PAGE_SHIFT)

#define __va(x) ((void *)((unsigned long)(x) + KERNELBASE))

/* Given that physical addresses do not map 1-1 to absolute addresses, we
 * use these macros to better specify exactly what we want to do.
 * The only restriction on their use is that the absolute address
 * macros cannot be used until after the LMB structure has been
 * initialized in prom.c.  -Peter
 */
#define __v2p(x) ((void *) __pa(x))
#define __v2a(x) ((void *) phys_to_absolute(__pa(x)))
#define __p2a(x) ((void *) phys_to_absolute(x))
#define __p2v(x) ((void *) __va(x))
#define __a2p(x) ((void *) absolute_to_phys(x))
#define __a2v(x) ((void *) __va(absolute_to_phys(x)))

#define virt_to_page(kaddr) (mem_map+(__pa((unsigned long)kaddr) >> PAGE_SHIFT))

#define VALID_PAGE(page)    ((page - mem_map) < max_mapnr)

#define MAP_NR(addr)        (__pa(addr) >> PAGE_SHIFT)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */
#endif /* _PPC64_PAGE_H */
