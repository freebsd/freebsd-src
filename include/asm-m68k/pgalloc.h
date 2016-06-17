
#ifndef M68K_PGALLOC_H
#define M68K_PGALLOC_H

#include <linux/config.h>
#include <asm/setup.h>
#include <asm/virtconvert.h>

/*
 * Cache handling functions
 */

#define flush_icache()						\
({								\
	if (CPU_IS_040_OR_060)					\
		__asm__ __volatile__("nop\n\t"			\
				     ".chip 68040\n\t"		\
				     "cinva %%ic\n\t"		\
				     ".chip 68k" : );		\
	else {							\
		unsigned long _tmp;				\
		__asm__ __volatile__("movec %%cacr,%0\n\t"	\
				     "orw %1,%0\n\t"		\
				     "movec %0,%%cacr"		\
				     : "=&d" (_tmp)		\
				     : "id" (FLUSH_I));	\
	}							\
})

/*
 * invalidate the cache for the specified memory range.
 * It starts at the physical address specified for
 * the given number of bytes.
 */
extern void cache_clear(unsigned long paddr, int len);
/*
 * push any dirty cache in the specified memory range.
 * It starts at the physical address specified for
 * the given number of bytes.
 */
extern void cache_push(unsigned long paddr, int len);

/*
 * push and invalidate pages in the specified user virtual
 * memory range.
 */
extern void cache_push_v(unsigned long vaddr, int len);

/* cache code */
#define FLUSH_I_AND_D	(0x00000808)
#define FLUSH_I 	(0x00000008)

/* This is needed whenever the virtual mapping of the current
   process changes.  */
#define __flush_cache_all()					\
({								\
	if (CPU_IS_040_OR_060)					\
		__asm__ __volatile__("nop\n\t"			\
				     ".chip 68040\n\t"		\
				     "cpusha %dc\n\t"		\
				     ".chip 68k");		\
	else {							\
		unsigned long _tmp;				\
		__asm__ __volatile__("movec %%cacr,%0\n\t"	\
				     "orw %1,%0\n\t"		\
				     "movec %0,%%cacr"		\
				     : "=&d" (_tmp)		\
				     : "di" (FLUSH_I_AND_D));	\
	}							\
})

#define __flush_cache_030()					\
({								\
	if (CPU_IS_020_OR_030) {				\
		unsigned long _tmp;				\
		__asm__ __volatile__("movec %%cacr,%0\n\t"	\
				     "orw %1,%0\n\t"		\
				     "movec %0,%%cacr"		\
				     : "=&d" (_tmp)		\
				     : "di" (FLUSH_I_AND_D));	\
	}							\
})

#define flush_cache_all() __flush_cache_all()

static inline void flush_cache_mm(struct mm_struct *mm)
{
	if (mm == current->mm)
		__flush_cache_030();
}

static inline void flush_cache_range(struct mm_struct *mm, unsigned long start,
				     unsigned long end)
{
	if (mm == current->mm)
	        __flush_cache_030();
}

static inline void flush_cache_page(struct vm_area_struct *vma,
				    unsigned long vmaddr)
{
	if (vma->vm_mm == current->mm)
	        __flush_cache_030();
}

/* Push the page at kernel virtual address and clear the icache */
/* RZ: use cpush %bc instead of cpush %dc, cinv %ic */
#define flush_page_to_ram(page) __flush_page_to_ram((unsigned long) page_address(page))
static inline void __flush_page_to_ram(unsigned long address)
{
	if (CPU_IS_040_OR_060) {
		__asm__ __volatile__("nop\n\t"
				     ".chip 68040\n\t"
				     "cpushp %%bc,(%0)\n\t"
				     ".chip 68k"
				     : : "a" (__pa((void *)address)));
	} else {
		unsigned long _tmp;
		__asm__ __volatile__("movec %%cacr,%0\n\t"
				     "orw %1,%0\n\t"
				     "movec %0,%%cacr"
				     : "=&d" (_tmp)
				     : "di" (FLUSH_I));
	}
}

#define flush_dcache_page(page)			do { } while (0)
#define flush_icache_page(vma,pg)              do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

/* Push n pages at kernel virtual address and clear the icache */
/* RZ: use cpush %bc instead of cpush %dc, cinv %ic */
extern inline void flush_icache_range (unsigned long address,
				       unsigned long endaddr)
{
	if (CPU_IS_040_OR_060) {
		short n = (endaddr - address + PAGE_SIZE - 1) / PAGE_SIZE;

		while (--n >= 0) {
			__asm__ __volatile__("nop\n\t"
					     ".chip 68040\n\t"
					     "cpushp %%bc,(%0)\n\t"
					     ".chip 68k"
					     : : "a" (virt_to_phys((void *)address)));
			address += PAGE_SIZE;
		}
	} else {
		unsigned long tmp;
		__asm__ __volatile__("movec %%cacr,%0\n\t"
				     "orw %1,%0\n\t"
				     "movec %0,%%cacr"
				     : "=&d" (tmp)
				     : "di" (FLUSH_I));
	}
}




#ifdef CONFIG_SUN3
#include <asm/sun3_pgalloc.h>
#else
#include <asm/motorola_pgalloc.h>
#endif

#endif /* M68K_PGALLOC_H */
