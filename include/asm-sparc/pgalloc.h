/* $Id: pgalloc.h,v 1.15 2001/10/18 09:06:37 davem Exp $ */
#ifndef _SPARC_PGALLOC_H
#define _SPARC_PGALLOC_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/page.h>
#include <asm/btfixup.h>

/* Fine grained cache/tlb flushing. */
#ifdef CONFIG_SMP
BTFIXUPDEF_CALL(void, local_flush_cache_all, void)
BTFIXUPDEF_CALL(void, local_flush_cache_mm, struct mm_struct *)
BTFIXUPDEF_CALL(void, local_flush_cache_range, struct mm_struct *, unsigned long, unsigned long)
BTFIXUPDEF_CALL(void, local_flush_cache_page, struct vm_area_struct *, unsigned long)

#define local_flush_cache_all() BTFIXUP_CALL(local_flush_cache_all)()
#define local_flush_cache_mm(mm) BTFIXUP_CALL(local_flush_cache_mm)(mm)
#define local_flush_cache_range(mm,start,end) BTFIXUP_CALL(local_flush_cache_range)(mm,start,end)
#define local_flush_cache_page(vma,addr) BTFIXUP_CALL(local_flush_cache_page)(vma,addr)

BTFIXUPDEF_CALL(void, local_flush_tlb_all, void)
BTFIXUPDEF_CALL(void, local_flush_tlb_mm, struct mm_struct *)
BTFIXUPDEF_CALL(void, local_flush_tlb_range, struct mm_struct *, unsigned long, unsigned long)
BTFIXUPDEF_CALL(void, local_flush_tlb_page, struct vm_area_struct *, unsigned long)

#define local_flush_tlb_all() BTFIXUP_CALL(local_flush_tlb_all)()
#define local_flush_tlb_mm(mm) BTFIXUP_CALL(local_flush_tlb_mm)(mm)
#define local_flush_tlb_range(mm,start,end) BTFIXUP_CALL(local_flush_tlb_range)(mm,start,end)
#define local_flush_tlb_page(vma,addr) BTFIXUP_CALL(local_flush_tlb_page)(vma,addr)

BTFIXUPDEF_CALL(void, local_flush_page_to_ram, unsigned long)
BTFIXUPDEF_CALL(void, local_flush_sig_insns, struct mm_struct *, unsigned long)

#define local_flush_page_to_ram(addr) BTFIXUP_CALL(local_flush_page_to_ram)(addr)
#define local_flush_sig_insns(mm,insn_addr) BTFIXUP_CALL(local_flush_sig_insns)(mm,insn_addr)

extern void smp_flush_cache_all(void);
extern void smp_flush_cache_mm(struct mm_struct *mm);
extern void smp_flush_cache_range(struct mm_struct *mm,
				  unsigned long start,
				  unsigned long end);
extern void smp_flush_cache_page(struct vm_area_struct *vma, unsigned long page);

extern void smp_flush_tlb_all(void);
extern void smp_flush_tlb_mm(struct mm_struct *mm);
extern void smp_flush_tlb_range(struct mm_struct *mm,
				  unsigned long start,
				  unsigned long end);
extern void smp_flush_tlb_page(struct vm_area_struct *mm, unsigned long page);
extern void smp_flush_page_to_ram(unsigned long page);
extern void smp_flush_sig_insns(struct mm_struct *mm, unsigned long insn_addr);
#endif

BTFIXUPDEF_CALL(void, flush_cache_all, void)
BTFIXUPDEF_CALL(void, flush_cache_mm, struct mm_struct *)
BTFIXUPDEF_CALL(void, flush_cache_range, struct mm_struct *, unsigned long, unsigned long)
BTFIXUPDEF_CALL(void, flush_cache_page, struct vm_area_struct *, unsigned long)

#define flush_cache_all() BTFIXUP_CALL(flush_cache_all)()
#define flush_cache_mm(mm) BTFIXUP_CALL(flush_cache_mm)(mm)
#define flush_cache_range(mm,start,end) BTFIXUP_CALL(flush_cache_range)(mm,start,end)
#define flush_cache_page(vma,addr) BTFIXUP_CALL(flush_cache_page)(vma,addr)
#define flush_icache_range(start, end)		do { } while (0)

BTFIXUPDEF_CALL(void, flush_tlb_all, void)
BTFIXUPDEF_CALL(void, flush_tlb_mm, struct mm_struct *)
BTFIXUPDEF_CALL(void, flush_tlb_range, struct mm_struct *, unsigned long, unsigned long)
BTFIXUPDEF_CALL(void, flush_tlb_page, struct vm_area_struct *, unsigned long)

extern __inline__ void flush_tlb_pgtables(struct mm_struct *mm, unsigned long start, unsigned long end)
{
}

#define flush_tlb_all() BTFIXUP_CALL(flush_tlb_all)()
#define flush_tlb_mm(mm) BTFIXUP_CALL(flush_tlb_mm)(mm)
#define flush_tlb_range(mm,start,end) BTFIXUP_CALL(flush_tlb_range)(mm,start,end)
#define flush_tlb_page(vma,addr) BTFIXUP_CALL(flush_tlb_page)(vma,addr)

BTFIXUPDEF_CALL(void, __flush_page_to_ram, unsigned long)
BTFIXUPDEF_CALL(void, flush_sig_insns, struct mm_struct *, unsigned long)

#define __flush_page_to_ram(addr) BTFIXUP_CALL(__flush_page_to_ram)(addr)
#define flush_sig_insns(mm,insn_addr) BTFIXUP_CALL(flush_sig_insns)(mm,insn_addr)

extern void flush_page_to_ram(struct page *page);

#define flush_dcache_page(page)			do { } while (0)

extern struct pgtable_cache_struct {
	unsigned long *pgd_cache;
	unsigned long *pte_cache;
	unsigned long pgtable_cache_sz;
	unsigned long pgd_cache_sz;
} pgt_quicklists;
#define pgd_quicklist           (pgt_quicklists.pgd_cache)
#define pmd_quicklist           ((unsigned long *)0)
#define pte_quicklist           (pgt_quicklists.pte_cache)
#define pgtable_cache_size      (pgt_quicklists.pgtable_cache_sz)
#define pgd_cache_size		(pgt_quicklists.pgd_cache_sz)

BTFIXUPDEF_CALL(int,	 do_check_pgt_cache, int, int)
#define do_check_pgt_cache(low,high) BTFIXUP_CALL(do_check_pgt_cache)(low,high)

BTFIXUPDEF_CALL(pgd_t *, get_pgd_fast, void)
#define get_pgd_fast()		BTFIXUP_CALL(get_pgd_fast)()

BTFIXUPDEF_CALL(void, free_pgd_fast, pgd_t *)
#define free_pgd_fast(pgd)	BTFIXUP_CALL(free_pgd_fast)(pgd)

#define pgd_free(pgd)	free_pgd_fast(pgd)
#define pgd_alloc(mm)	get_pgd_fast()

#define pgd_populate(MM, PGD, PMD)      pgd_set(PGD, PMD)

static __inline__ pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
	return 0;
}

BTFIXUPDEF_CALL(pmd_t *, pmd_alloc_one_fast, struct mm_struct *, unsigned long)
#define pmd_alloc_one_fast(mm, address)	BTFIXUP_CALL(pmd_alloc_one_fast)(mm, address)

BTFIXUPDEF_CALL(void, free_pmd_fast, pmd_t *)
#define free_pmd_fast(pmd)	BTFIXUP_CALL(free_pmd_fast)(pmd)

#define pmd_free(pmd)           free_pmd_fast(pmd)

#define pmd_populate(MM, PMD, PTE)      pmd_set(PMD, PTE)

BTFIXUPDEF_CALL(pte_t *, pte_alloc_one, struct mm_struct *, unsigned long)
#define pte_alloc_one(mm, address)	BTFIXUP_CALL(pte_alloc_one)(mm, address)

BTFIXUPDEF_CALL(pte_t *, pte_alloc_one_fast, struct mm_struct *, unsigned long)
#define pte_alloc_one_fast(mm, address)	BTFIXUP_CALL(pte_alloc_one_fast)(mm, address)

BTFIXUPDEF_CALL(void, free_pte_fast, pte_t *)
#define free_pte_fast(pte)	BTFIXUP_CALL(free_pte_fast)(pte)

#define pte_free(pte)		free_pte_fast(pte)

#endif /* _SPARC_PGALLOC_H */
