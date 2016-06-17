#ifndef _I386_PGALLOC_H
#define _I386_PGALLOC_H

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/fixmap.h>
#include <linux/threads.h>

#define pgd_quicklist (current_cpu_data.pgd_quick)
#define pmd_quicklist (current_cpu_data.pmd_quick)
#define pte_quicklist (current_cpu_data.pte_quick)
#define pgtable_cache_size (current_cpu_data.pgtable_cache_sz)

#define pmd_populate(mm, pmd, pte) \
		set_pmd(pmd, __pmd(_PAGE_TABLE + __pa(pte)))

/*
 * Allocate and free page tables.
 */

#if defined (CONFIG_X86_PAE)
/*
 * We can't include <linux/slab.h> here, thus these uglinesses.
 */
struct kmem_cache_s;

extern struct kmem_cache_s *pae_pgd_cachep;
extern void *kmem_cache_alloc(struct kmem_cache_s *, int);
extern void kmem_cache_free(struct kmem_cache_s *, void *);


static inline pgd_t *get_pgd_slow(void)
{
	int i;
	pgd_t *pgd = kmem_cache_alloc(pae_pgd_cachep, GFP_KERNEL);

	if (pgd) {
		for (i = 0; i < USER_PTRS_PER_PGD; i++) {
			unsigned long pmd = __get_free_page(GFP_KERNEL);
			if (!pmd)
				goto out_oom;
			clear_page(pmd);
			set_pgd(pgd + i, __pgd(1 + __pa(pmd)));
		}
		memcpy(pgd + USER_PTRS_PER_PGD,
			swapper_pg_dir + USER_PTRS_PER_PGD,
			(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));
	}
	return pgd;
out_oom:
	for (i--; i >= 0; i--)
		free_page((unsigned long)__va(pgd_val(pgd[i])-1));
	kmem_cache_free(pae_pgd_cachep, pgd);
	return NULL;
}

#else

static inline pgd_t *get_pgd_slow(void)
{
	pgd_t *pgd = (pgd_t *)__get_free_page(GFP_KERNEL);

	if (pgd) {
		memset(pgd, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));
		memcpy(pgd + USER_PTRS_PER_PGD,
			swapper_pg_dir + USER_PTRS_PER_PGD,
			(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));
	}
	return pgd;
}

#endif /* CONFIG_X86_PAE */

static inline pgd_t *get_pgd_fast(void)
{
	unsigned long *ret;

	if ((ret = pgd_quicklist) != NULL) {
		pgd_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		pgtable_cache_size--;
	} else
		ret = (unsigned long *)get_pgd_slow();
	return (pgd_t *)ret;
}

static inline void free_pgd_fast(pgd_t *pgd)
{
	*(unsigned long *)pgd = (unsigned long) pgd_quicklist;
	pgd_quicklist = (unsigned long *) pgd;
	pgtable_cache_size++;
}

static inline void free_pgd_slow(pgd_t *pgd)
{
#if defined(CONFIG_X86_PAE)
	int i;

	for (i = 0; i < USER_PTRS_PER_PGD; i++)
		free_page((unsigned long)__va(pgd_val(pgd[i])-1));
	kmem_cache_free(pae_pgd_cachep, pgd);
#else
	free_page((unsigned long)pgd);
#endif
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte;

	pte = (pte_t *) __get_free_page(GFP_KERNEL);
	if (pte)
		clear_page(pte);
	return pte;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm,
					unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = ret[1];
		pgtable_cache_size--;
	}
	return (pte_t *)ret;
}

static inline void pte_free_fast(pte_t *pte)
{
	*(unsigned long *)pte = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pte;
	pgtable_cache_size++;
}

static __inline__ void pte_free_slow(pte_t *pte)
{
	free_page((unsigned long)pte);
}

#define pte_free(pte)		pte_free_fast(pte)
#define pgd_free(pgd)		free_pgd_slow(pgd)
#define pgd_alloc(mm)		get_pgd_fast()

/*
 * allocating and freeing a pmd is trivial: the 1-entry pmd is
 * inside the pgd, so has no extra memory associated with it.
 * (In the PAE case we free the pmds as part of the pgd.)
 */

#define pmd_alloc_one_fast(mm, addr)	({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm, addr)		({ BUG(); ((pmd_t *)2); })
#define pmd_free_slow(x)		do { } while (0)
#define pmd_free_fast(x)		do { } while (0)
#define pmd_free(x)			do { } while (0)
#define pgd_populate(mm, pmd, pte)	BUG()

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
 *
 * ..but the i386 has somewhat limited tlb flushing capabilities,
 * and page-granular flushes are available only on i486 and up.
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

#endif /* CONFIG_SMP */

static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
	flush_tlb_mm(mm);
}

#endif /* _I386_PGALLOC_H */
