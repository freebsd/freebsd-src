/* $Id: page.h,v 1.38 2001/11/30 01:04:10 davem Exp $ */

#ifndef _SPARC64_PAGE_H
#define _SPARC64_PAGE_H

#define PAGE_SHIFT   13
#ifndef __ASSEMBLY__
/* I have my suspicions... -DaveM */
#define PAGE_SIZE    (1UL << PAGE_SHIFT)
#else
#define PAGE_SIZE    (1 << PAGE_SHIFT)
#endif

#define PAGE_MASK    (~(PAGE_SIZE-1))


#ifdef __KERNEL__

#ifndef __ASSEMBLY__

#ifdef CONFIG_DEBUG_BUGVERBOSE
extern void do_BUG(const char *file, int line);
#define BUG() do {					\
	do_BUG(__FILE__, __LINE__);			\
	__builtin_trap();				\
} while (0)
#else
#define BUG()		__builtin_trap()
#endif

#define PAGE_BUG(page)	BUG()

/* Sparc64 is slow at multiplication, we prefer to use some extra space. */
#define WANT_PAGE_VIRTUAL 1

extern void _clear_page(void *page);
#define clear_page(X)	_clear_page((void *)(X))
extern void clear_user_page(void *page, unsigned long vaddr);
#define copy_page(X,Y)	__memcpy((void *)(X), (void *)(Y), PAGE_SIZE)
extern void copy_user_page(void *to, void *from, unsigned long vaddr);

/* GROSS, defining this makes gcc pass these types as aggregates,
 * and thus on the stack, turn this crap off... -DaveM
 */

/* #define STRICT_MM_TYPECHECKS */

#ifdef STRICT_MM_TYPECHECKS
/* These are used to make use of C type-checking.. */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long iopte; } iopte_t;
typedef struct { unsigned int pmd; } pmd_t;
typedef struct { unsigned int pgd; } pgd_t;
typedef struct { unsigned long ctxd; } ctxd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct { unsigned long iopgprot; } iopgprot_t;

#define pte_val(x)	((x).pte)
#define iopte_val(x)	((x).iopte)
#define pmd_val(x)      ((unsigned long)(x).pmd)
#define pgd_val(x)	((unsigned long)(x).pgd)
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
/* .. while these make it easier on the compiler */
typedef unsigned long pte_t;
typedef unsigned long iopte_t;
typedef unsigned int pmd_t;
typedef unsigned int pgd_t;
typedef unsigned long ctxd_t;
typedef unsigned long pgprot_t;
typedef unsigned long iopgprot_t;

#define pte_val(x)	(x)
#define iopte_val(x)	(x)
#define pmd_val(x)      ((unsigned long)(x))
#define pgd_val(x)	((unsigned long)(x))
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

#endif /* (STRICT_MM_TYPECHECKS) */

#define TASK_UNMAPPED_BASE	((current->thread.flags & SPARC_FLAG_32BIT) ? \
				 (0x0000000070000000UL) : (PAGE_OFFSET))

#endif /* !(__ASSEMBLY__) */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

#ifndef __ASSEMBLY__
/* Do prdele, look what happens to be in %g4... */
register unsigned long PAGE_OFFSET asm("g4");
#else
#define PAGE_OFFSET		0xFFFFF80000000000
#endif

#define __pa(x)			((unsigned long)(x) - PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long) (x) + PAGE_OFFSET))
#define virt_to_page(kaddr)	(mem_map + ((__pa(kaddr)-phys_base) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#define virt_to_phys __pa
#define phys_to_virt __va

#ifndef __ASSEMBLY__

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

#endif /* !(__ASSEMBLY__) */

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* !(__KERNEL__) */

#endif /* !(_SPARC64_PAGE_H) */
