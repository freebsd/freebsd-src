#ifndef _ALPHA_PGALLOC_H
#define _ALPHA_PGALLOC_H

#include <linux/config.h>

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __MMU_EXTERN_INLINE
#endif

extern void __load_new_mm_context(struct mm_struct *);


/* Caches aren't brain-dead on the Alpha. */
#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_range(mm, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr)		do { } while (0)
#define flush_page_to_ram(page)			do { } while (0)
#define flush_dcache_page(page)			do { } while (0)

/* Note that the following two definitions are _highly_ dependent
   on the contexts in which they are used in the kernel.  I personally
   think it is criminal how loosely defined these macros are.  */

/* We need to flush the kernel's icache after loading modules.  The
   only other use of this macro is in load_aout_interp which is not
   used on Alpha. 

   Note that this definition should *not* be used for userspace
   icache flushing.  While functional, it is _way_ overkill.  The
   icache is tagged with ASNs and it suffices to allocate a new ASN
   for the process.  */
#ifndef CONFIG_SMP
#define flush_icache_range(start, end)		imb()
#else
#define flush_icache_range(start, end)		smp_imb()
extern void smp_imb(void);
#endif


/*
 * Use a few helper functions to hide the ugly broken ASN
 * numbers on early Alphas (ev4 and ev45)
 */

__EXTERN_INLINE void
ev4_flush_tlb_current(struct mm_struct *mm)
{
	__load_new_mm_context(mm);
	tbiap();
}

__EXTERN_INLINE void
ev5_flush_tlb_current(struct mm_struct *mm)
{
	__load_new_mm_context(mm);
}

static inline void
flush_tlb_other(struct mm_struct *mm)
{
	long * mmc = &mm->context[smp_processor_id()];
	/*
	 * Check it's not zero first to avoid cacheline ping pong when
	 * possible.
	 */
	if (*mmc)
		*mmc = 0;
}

/* We need to flush the userspace icache after setting breakpoints in
   ptrace.

   Instead of indiscriminately using imb, take advantage of the fact
   that icache entries are tagged with the ASN and load a new mm context.  */
/* ??? Ought to use this in arch/alpha/kernel/signal.c too.  */

#ifndef CONFIG_SMP
static inline void
flush_icache_user_range(struct vm_area_struct *vma, struct page *page,
			unsigned long addr, int len)
{
	if (vma->vm_flags & VM_EXEC) {
		struct mm_struct *mm = vma->vm_mm;
		if (current->active_mm == mm)
			__load_new_mm_context(mm);
		else
			mm->context[smp_processor_id()] = 0;
	}
}
#else
extern void flush_icache_user_range(struct vm_area_struct *vma,
		struct page *page, unsigned long addr, int len);
#endif

/* this is used only in do_no_page and do_swap_page */
#define flush_icache_page(vma, page)	flush_icache_user_range((vma), (page), 0, 0)

/*
 * Flush just one page in the current TLB set.
 * We need to be very careful about the icache here, there
 * is no way to invalidate a specific icache page..
 */

__EXTERN_INLINE void
ev4_flush_tlb_current_page(struct mm_struct * mm,
			   struct vm_area_struct *vma,
			   unsigned long addr)
{
	int tbi_flag = 2;
	if (vma->vm_flags & VM_EXEC) {
		__load_new_mm_context(mm);
		tbi_flag = 3;
	}
	tbi(tbi_flag, addr);
}

__EXTERN_INLINE void
ev5_flush_tlb_current_page(struct mm_struct * mm,
			   struct vm_area_struct *vma,
			   unsigned long addr)
{
	if (vma->vm_flags & VM_EXEC)
		__load_new_mm_context(mm);
	else
		tbi(2, addr);
}


#ifdef CONFIG_ALPHA_GENERIC
# define flush_tlb_current		alpha_mv.mv_flush_tlb_current
# define flush_tlb_current_page		alpha_mv.mv_flush_tlb_current_page
#else
# ifdef CONFIG_ALPHA_EV4
#  define flush_tlb_current		ev4_flush_tlb_current
#  define flush_tlb_current_page	ev4_flush_tlb_current_page
# else
#  define flush_tlb_current		ev5_flush_tlb_current
#  define flush_tlb_current_page	ev5_flush_tlb_current_page
# endif
#endif

#ifdef __MMU_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __MMU_EXTERN_INLINE
#endif

/*
 * Flush current user mapping.
 */
static inline void flush_tlb(void)
{
	flush_tlb_current(current->active_mm);
}

/*
 * Flush a specified range of user mapping page tables
 * from TLB.
 * Although Alpha uses VPTE caches, this can be a nop, as Alpha does
 * not have finegrained tlb flushing, so it will flush VPTE stuff
 * during next flush_tlb_range.
 */
static inline void flush_tlb_pgtables(struct mm_struct *mm,
	unsigned long start, unsigned long end)
{
}

#ifndef CONFIG_SMP
/*
 * Flush everything (kernel mapping may also have
 * changed due to vmalloc/vfree)
 */
static inline void flush_tlb_all(void)
{
	tbia();
}

/*
 * Flush a specified user mapping
 */
static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (mm == current->active_mm)
		flush_tlb_current(mm);
	else
		flush_tlb_other(mm);
}

/*
 * Page-granular tlb flush.
 *
 * do a tbisd (type = 2) normally, and a tbis (type = 3)
 * if it is an executable mapping.  We want to avoid the
 * itlb flush, because that potentially also does a
 * icache flush.
 */
static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	struct mm_struct * mm = vma->vm_mm;

	if (mm == current->active_mm)
		flush_tlb_current_page(mm, vma, addr);
	else
		flush_tlb_other(mm);
}

/*
 * Flush a specified range of user mapping:  on the
 * Alpha we flush the whole user tlb.
 */
static inline void flush_tlb_range(struct mm_struct *mm,
	unsigned long start, unsigned long end)
{
	flush_tlb_mm(mm);
}

#else /* CONFIG_SMP */

extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_page(struct vm_area_struct *, unsigned long);
extern void flush_tlb_range(struct mm_struct *, unsigned long, unsigned long);

#endif /* CONFIG_SMP */

/*      
 * Allocate and free page tables. The xxx_kernel() versions are
 * used to allocate a kernel page table - this turns on ASN bits
 * if any.
 */
#ifndef CONFIG_SMP
extern struct pgtable_cache_struct {
	unsigned long *pgd_cache;
	unsigned long *pmd_cache;
	unsigned long *pte_cache;
	unsigned long pgtable_cache_sz;
} quicklists;
#else
#include <asm/smp.h>
#define quicklists cpu_data[smp_processor_id()]
#endif
#define pgd_quicklist (quicklists.pgd_cache)
#define pmd_quicklist (quicklists.pmd_cache)
#define pte_quicklist (quicklists.pte_cache)
#define pgtable_cache_size (quicklists.pgtable_cache_sz)

#define pmd_populate(mm, pmd, pte)	pmd_set(pmd, pte)
#define pgd_populate(mm, pgd, pmd)	pgd_set(pgd, pmd)

extern pgd_t *get_pgd_slow(void);

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
	free_page((unsigned long)pgd);
}

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pmd_t *ret = (pmd_t *)__get_free_page(GFP_KERNEL);
	if (ret)
		clear_page(ret);
	return ret;
}

static inline pmd_t *pmd_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		pgtable_cache_size--;
	}
	return (pmd_t *)ret;
}

static inline void pmd_free_fast(pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pmd;
	pgtable_cache_size++;
}

static inline void pmd_free_slow(pmd_t *pmd)
{
	free_page((unsigned long)pmd);
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *)__get_free_page(GFP_KERNEL);
	if (pte)
		clear_page(pte);
	return pte;
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *)pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
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

static inline void pte_free_slow(pte_t *pte)
{
	free_page((unsigned long)pte);
}

#define pte_free(pte)		pte_free_fast(pte)
#define pmd_free(pmd)		pmd_free_fast(pmd)
#define pgd_free(pgd)		free_pgd_fast(pgd)
#define pgd_alloc(mm)		get_pgd_fast()

extern int do_check_pgt_cache(int, int);

#endif /* _ALPHA_PGALLOC_H */
