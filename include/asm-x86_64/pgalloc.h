#ifndef _X86_64_PGALLOC_H
#define _X86_64_PGALLOC_H

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/fixmap.h>
#include <asm/pda.h>
#include <linux/threads.h>
#include <linux/mm.h>
#include <asm/page.h>

#define inc_pgcache_size() add_pda(pgtable_cache_sz,1UL)
#define dec_pgcache_size() sub_pda(pgtable_cache_sz,1UL)

#define pmd_populate(mm, pmd, pte) \
		set_pmd(pmd, __pmd(_PAGE_TABLE | __pa(pte)))
#define pgd_populate(mm, pgd, pmd) \
		set_pgd(pgd, __pgd(_PAGE_TABLE | __pa(pmd)))

extern __inline__ pmd_t *get_pmd_slow(void)
{
	return (pmd_t *)get_zeroed_page(GFP_KERNEL);
}

extern __inline__ pmd_t *get_pmd_fast(void)
{
	unsigned long *ret;

	if ((ret = read_pda(pmd_quick)) != NULL) {
		write_pda(pmd_quick, (unsigned long *)(*ret));
		ret[0] = 0;
		dec_pgcache_size();
	} else
		ret = (unsigned long *)get_pmd_slow();
	return (pmd_t *)ret;
}

extern __inline__ void pmd_free(pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long) read_pda(pmd_quick);
	write_pda(pmd_quick,(unsigned long *) pmd);
	inc_pgcache_size();
}

extern __inline__ void pmd_free_slow(pmd_t *pmd)
{
	if ((unsigned long)pmd & (PAGE_SIZE-1)) 
		out_of_line_bug(); 
	free_page((unsigned long)pmd);
}

static inline pmd_t *pmd_alloc_one_fast (struct mm_struct *mm, unsigned long addr)
{
	unsigned long *ret = (unsigned long *)read_pda(pmd_quick);

	if (ret != NULL) {
		write_pda(pmd_quick, (unsigned long *)(*ret));
		ret[0] = 0;
		dec_pgcache_size();
	}
	return (pmd_t *)ret;
}

static inline pmd_t *pmd_alloc_one (struct mm_struct *mm, unsigned long addr)
{
	return (pmd_t *)get_zeroed_page(GFP_KERNEL); 
}

static inline pgd_t *pgd_alloc_one_fast (void)
{
	unsigned long *ret = read_pda(pgd_quick);

	if (ret) {
		write_pda(pgd_quick,(unsigned long *)(*ret));
		ret[0] = 0;
		dec_pgcache_size();
	}
	return (pgd_t *) ret;
}

static inline pgd_t *pgd_alloc (struct mm_struct *mm)
{
	/* the VM system never calls pgd_alloc_one_fast(), so we do it here. */
	pgd_t *pgd = pgd_alloc_one_fast();

	if (pgd == NULL)
		pgd = (pgd_t *)get_zeroed_page(GFP_KERNEL); 
	return pgd;
}

static inline void pgd_free (pgd_t *pgd)
{
	*(unsigned long *)pgd = (unsigned long) read_pda(pgd_quick);
	write_pda(pgd_quick,(unsigned long *) pgd);
	inc_pgcache_size();
}


static inline void pgd_free_slow (pgd_t *pgd)
{
	if ((unsigned long)pgd & (PAGE_SIZE-1)) 
		out_of_line_bug(); 
	free_page((unsigned long)pgd);
}


static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	return (pte_t *)get_zeroed_page(GFP_KERNEL); 
}

extern __inline__ pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = read_pda(pte_quick)) != NULL) {  
		write_pda(pte_quick, (unsigned long *)(*ret));
		ret[0] = ret[1];
		dec_pgcache_size();
	}
	return (pte_t *)ret;
}

/* Should really implement gc for free page table pages. This could be done with 
   a reference count in struct page. */

extern __inline__ void pte_free(pte_t *pte)
{	
	*(unsigned long *)pte = (unsigned long) read_pda(pte_quick);
	write_pda(pte_quick, (unsigned long *) pte); 
	inc_pgcache_size();
}

extern __inline__ void pte_free_slow(pte_t *pte)
{
	if ((unsigned long)pte & (PAGE_SIZE-1))
		out_of_line_bug();
	free_page((unsigned long)pte); 
}


extern int do_check_pgt_cache(int, int);

/*
 * TLB flushing:
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *  - flush_tlb_pgtables(mm, start, end) flushes a range of page tables
 */

#ifndef CONFIG_SMP

#define flush_tlb() __flush_tlb()
#define flush_tlb_all() __flush_tlb_all()
#define local_flush_tlb() __flush_tlb()

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (mm == current->active_mm)
		__flush_tlb();
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	if (vma->vm_mm == current->active_mm)
		__flush_tlb_one(addr);
}

static inline void flush_tlb_range(struct mm_struct *mm,
	unsigned long start, unsigned long end)
{
	if (mm == current->active_mm)
		__flush_tlb();
}

#else

#include <asm/smp.h>

#define local_flush_tlb() \
	__flush_tlb()

extern void flush_tlb_all(void);
extern void flush_tlb_current_task(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_page(struct vm_area_struct *, unsigned long);

#define flush_tlb()	flush_tlb_current_task()

static inline void flush_tlb_range(struct mm_struct * mm, unsigned long start, unsigned long end)
{
	flush_tlb_mm(mm);
}

#define TLBSTATE_OK	1
#define TLBSTATE_LAZY	2

struct tlb_state
{
	struct mm_struct *active_mm;
	int state;
} ____cacheline_aligned;
extern struct tlb_state cpu_tlbstate[NR_CPUS];


#endif

extern inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
	flush_tlb_mm(mm);
}

#endif /* _X86_64_PGALLOC_H */
