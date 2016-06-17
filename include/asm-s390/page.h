/*
 *  include/asm-s390/page.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com)
 */

#ifndef _S390_PAGE_H
#define _S390_PAGE_H

#include <asm/setup.h>
#include <asm/types.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE-1))

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

static inline void clear_page(void *page)
{
	register_pair rp;

	rp.subreg.even = (unsigned long) page;
	rp.subreg.odd = (unsigned long) 4096;
        asm volatile ("   slr  1,1\n"
		      "   mvcl %0,0"
		      : "+&a" (rp) : : "memory", "cc", "1" );
}

static inline void copy_page(void *to, void *from)
{
        if (MACHINE_HAS_MVPG)
		asm volatile ("   sr   0,0\n"
			      "   mvpg %0,%1"
			      : : "a" ((void *)(to)), "a" ((void *)(from))
			      : "memory", "cc", "0" );
	else
		asm volatile ("   mvc  0(256,%0),0(%1)\n"
			      "   mvc  256(256,%0),256(%1)\n"
			      "   mvc  512(256,%0),512(%1)\n"
			      "   mvc  768(256,%0),768(%1)\n"
			      "   mvc  1024(256,%0),1024(%1)\n"
			      "   mvc  1280(256,%0),1280(%1)\n"
			      "   mvc  1536(256,%0),1536(%1)\n"
			      "   mvc  1792(256,%0),1792(%1)\n"
			      "   mvc  2048(256,%0),2048(%1)\n"
			      "   mvc  2304(256,%0),2304(%1)\n"
			      "   mvc  2560(256,%0),2560(%1)\n"
			      "   mvc  2816(256,%0),2816(%1)\n"
			      "   mvc  3072(256,%0),3072(%1)\n"
			      "   mvc  3328(256,%0),3328(%1)\n"
			      "   mvc  3584(256,%0),3584(%1)\n"
			      "   mvc  3840(256,%0),3840(%1)\n"
			      : : "a"((void *)(to)),"a"((void *)(from)) 
			      : "memory" );
}

#define clear_user_page(page, vaddr)	clear_page(page)
#define copy_user_page(to, from, vaddr)	copy_page(to, from)

#define BUG() do { \
        printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
        __asm__ __volatile__(".long 0"); \
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

/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct {
        unsigned long pgd0;
        unsigned long pgd1;
        unsigned long pgd2;
        unsigned long pgd3;
        } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)      ((x).pte)
#define pmd_val(x)      ((x).pmd)
#define pgd_val(x)      ((x).pgd0)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgd(x)        ((pgd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)        (((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define __PAGE_OFFSET           0x0UL
#define PAGE_OFFSET             0x0UL
#define __pa(x)                 (unsigned long)(x)
#define __va(x)                 (void *)(x)
#define virt_to_page(kaddr)	(mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

unsigned int get_storage_key(void);
unsigned long pfix_get_page_addr(void *);
unsigned long pfix_get_addr(void *);

#endif /* __KERNEL__ */

#endif /* _S390_PAGE_H */
