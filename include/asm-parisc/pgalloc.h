#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H

/* The usual comment is "Caches aren't brain-dead on the <architecture>".
 * Unfortunately, that doesn't apply to PA-RISC. */

#include <asm/processor.h>
#include <asm/fixmap.h>
#include <linux/threads.h>

#include <asm/pgtable.h>
#include <asm/cache.h>

#define flush_kernel_dcache_range(start,size) \
	flush_kernel_dcache_range_asm((start), (start)+(size));

static inline void
flush_page_to_ram(struct page *page)
{
}

extern void flush_cache_all_local(void);

#ifdef CONFIG_SMP
static inline void flush_cache_all(void)
{
	smp_call_function((void (*)(void *))flush_cache_all_local, NULL, 1, 1);
	flush_cache_all_local();
}
#else
#define flush_cache_all flush_cache_all_local
#endif

#ifdef CONFIG_SMP
#define flush_cache_mm(mm) flush_cache_all()
#else
#define flush_cache_mm(mm) flush_cache_all_local()
#endif

/* The following value needs to be tuned and probably scaled with the
 * cache size.
 */

#define FLUSH_THRESHOLD 0x80000

static inline void
flush_user_dcache_range(unsigned long start, unsigned long end)
{
#ifdef CONFIG_SMP
	flush_user_dcache_range_asm(start,end);
#else
	if ((end - start) < FLUSH_THRESHOLD)
		flush_user_dcache_range_asm(start,end);
	else
		flush_data_cache();
#endif
}

static inline void
flush_user_icache_range(unsigned long start, unsigned long end)
{
#ifdef CONFIG_SMP
	flush_user_icache_range_asm(start,end);
#else
	if ((end - start) < FLUSH_THRESHOLD)
		flush_user_icache_range_asm(start,end);
	else
		flush_instruction_cache();
#endif
}

static inline void
flush_cache_range(struct mm_struct *mm, unsigned long start, unsigned long end)
{
	int sr3;

	if (!mm->context) {
		BUG();
		return;
	}

	sr3 = mfsp(3);
	if (mm->context == sr3) {
		flush_user_dcache_range(start,end);
		flush_user_icache_range(start,end);
	} else {
		flush_cache_all();
	}
}

static inline void
flush_cache_page(struct vm_area_struct *vma, unsigned long vmaddr)
{
	int sr3;

	if (!vma->vm_mm->context) {
		BUG();
		return;
	}

	sr3 = mfsp(3);
	if (vma->vm_mm->context == sr3) {
		flush_user_dcache_range(vmaddr,vmaddr + PAGE_SIZE);
		if (vma->vm_flags & VM_EXEC)
			flush_user_icache_range(vmaddr,vmaddr + PAGE_SIZE);
	} else {
		if (vma->vm_flags & VM_EXEC)
			flush_cache_all();
		else
			flush_data_cache();
	}
}

extern void __flush_dcache_page(struct page *page);
static inline void flush_dcache_page(struct page *page)
{
	if (page->mapping && !page->mapping->i_mmap &&
			!page->mapping->i_mmap_shared) {
		set_bit(PG_dcache_dirty, &page->flags);
	} else {
		__flush_dcache_page(page);
	}
}

#define flush_icache_page(vma,page)	do { flush_kernel_dcache_page(page_address(page)); flush_kernel_icache_page(page_address(page)); } while (0)

#define flush_icache_user_range(vma, page, addr, len) \
	flush_user_icache_range(addr, addr + len);

#define flush_icache_range(s,e)		do { flush_kernel_dcache_range_asm(s,e); flush_kernel_icache_range_asm(s,e); } while (0)

/* TLB flushing routines.... */

extern void flush_tlb_all(void);

static inline void load_context(mm_context_t context)
{
	mtsp(context, 3);
#if SPACEID_SHIFT == 0
	mtctl(context << 1,8);
#else
	mtctl(context >> (SPACEID_SHIFT - 1),8);
#endif
}

/*
 * flush_tlb_mm()
 *
 * XXX This code is NOT valid for HP-UX compatibility processes,
 * (although it will probably work 99% of the time). HP-UX
 * processes are free to play with the space id's and save them
 * over long periods of time, etc. so we have to preserve the
 * space and just flush the entire tlb. We need to check the
 * personality in order to do that, but the personality is not
 * currently being set correctly.
 *
 * Of course, Linux processes could do the same thing, but
 * we don't support that (and the compilers, dynamic linker,
 * etc. do not do that).
 */

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (mm == &init_mm) BUG(); /* Should never happen */

#ifdef CONFIG_SMP
	flush_tlb_all();
#else
	if (mm) {
		if (mm->context != 0)
			free_sid(mm->context);
		mm->context = alloc_sid();
		if (mm == current->active_mm)
			load_context(mm->context);
	}
#endif
}

extern __inline__ void flush_tlb_pgtables(struct mm_struct *mm, unsigned long start, unsigned long end)
{
}
 
static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	/* For one page, it's not worth testing the split_tlb variable */

	mtsp(vma->vm_mm->context,1);
	pdtlb(addr);
	pitlb(addr);
}

static inline void flush_tlb_range(struct mm_struct *mm,
	unsigned long start, unsigned long end)
{
	unsigned long npages;

	npages = ((end - (start & PAGE_MASK)) + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
	if (npages >= 512)  /* XXX arbitrary, should be tuned */
		flush_tlb_all();
	else {

		mtsp(mm->context,1);
		if (split_tlb) {
			while (npages--) {
				pdtlb(start);
				pitlb(start);
				start += PAGE_SIZE;
			}
		} else {
			while (npages--) {
				pdtlb(start);
				start += PAGE_SIZE;
			}
		}
	}
}

static inline pgd_t *pgd_alloc_one_fast (void)
{
	return NULL; /* not implemented */
}

static inline pgd_t *pgd_alloc (struct mm_struct *mm)
{
	/* the VM system never calls pgd_alloc_one_fast(), so we do it here. */
	pgd_t *pgd = pgd_alloc_one_fast();
	if (!pgd) {
		pgd = (pgd_t *)__get_free_page(GFP_KERNEL);
		if (pgd)
			clear_page(pgd);
	}
	return pgd;
}

static inline void pgd_free(pgd_t *pgd)
{
	free_page((unsigned long)pgd);
}

#ifdef __LP64__

/* Three Level Page Table Support for pmd's */

static inline void pgd_populate(struct mm_struct *mm, pgd_t *pgd, pmd_t *pmd)
{
	pgd_val(*pgd) = _PAGE_TABLE + __pa((unsigned long)pmd);
}

static inline pmd_t *pmd_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	return NULL; /* la la */
}

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pmd_t *pmd = (pmd_t *) __get_free_page(GFP_KERNEL);
	if (pmd)
		clear_page(pmd);
	return pmd;
}

static inline void pmd_free(pmd_t *pmd)
{
	free_page((unsigned long)pmd);
}

#else

/* Two Level Page Table Support for pmd's */

/*
 * allocating and freeing a pmd is trivial: the 1-entry pmd is
 * inside the pgd, so has no extra memory associated with it.
 */

#define pmd_alloc_one_fast(mm, addr)	({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm, addr)		({ BUG(); ((pmd_t *)2); })
#define pmd_free(x)			do { } while (0)
#define pgd_populate(mm, pmd, pte)	BUG()

#endif

static inline void pmd_populate (struct mm_struct *mm, pmd_t *pmd_entry, pte_t *pte)
{
	pmd_val(*pmd_entry) = _PAGE_TABLE + __pa((unsigned long)pte);
}

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	return NULL; /* la la */
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *) __get_free_page(GFP_KERNEL);
	if (pte)
		clear_page(pte);
	return pte;
}

static inline void pte_free(pte_t *pte)
{
	free_page((unsigned long)pte);
}

extern int do_check_pgt_cache(int, int);

#endif
