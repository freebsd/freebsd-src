/*
 *  include/asm-s390/pgalloc.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com)
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/pgalloc.h"
 *    Copyright (C) 1994  Linus Torvalds
 */

#ifndef _S390_PGALLOC_H
#define _S390_PGALLOC_H

#include <linux/config.h>
#include <asm/processor.h>
#include <linux/threads.h>

#define pgd_quicklist (S390_lowcore.cpu_data.pgd_quick)
#define pmd_quicklist ((unsigned long *)0)
#define pte_quicklist (S390_lowcore.cpu_data.pte_quick)
#define pgtable_cache_size (S390_lowcore.cpu_data.pgtable_cache_sz)

extern void diag10(unsigned long addr);

/*
 * Allocate and free page tables. The xxx_kernel() versions are
 * used to allocate a kernel page table - this turns on ASN bits
 * if any.
 */

extern __inline__ pgd_t* get_pgd_slow(void)
{
	pgd_t *ret;
        int i;

	ret = (pgd_t *) __get_free_pages(GFP_KERNEL,1);
        if (ret != NULL)
		for (i = 0; i < USER_PTRS_PER_PGD; i++)
			pmd_clear(pmd_offset(ret + i, i*PGDIR_SIZE));
	return ret;
}

extern __inline__ pgd_t* get_pgd_fast(void)
{
        unsigned long *ret = pgd_quicklist;
	
        if (ret != NULL) {
                pgd_quicklist = (unsigned long *)(*ret);
                ret[0] = ret[1];
                pgtable_cache_size -= 2;
        }
        return (pgd_t *)ret;
}

extern __inline__ pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;

	pgd = get_pgd_fast();
	if (!pgd)
		pgd = get_pgd_slow();
	return pgd;
}

extern __inline__ void free_pgd_fast(pgd_t *pgd)
{
        *(unsigned long *)pgd = (unsigned long) pgd_quicklist;
        pgd_quicklist = (unsigned long *) pgd;
        pgtable_cache_size += 2;
}

extern __inline__ void free_pgd_slow(pgd_t *pgd)
{
        free_pages((unsigned long) pgd, 1);
}

#define pgd_free(pgd)           free_pgd_fast(pgd)

/*
 * page middle directory allocation/free routines.
 * We don't use pmd cache, so these are dummy routines. This
 * code never triggers because the pgd will always be present.
 */
#define pmd_alloc_one_fast(mm, address) ({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm,address)       ({ BUG(); ((pmd_t *)2); })
#define pmd_free(x)                     do { } while (0)
#define pmd_free_slow(x)		do { } while (0)
#define pmd_free_fast(x)                do { } while (0)
#define pgd_populate(mm, pmd, pte)      BUG()

extern inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	pmd_val(pmd[0]) = _PAGE_TABLE + __pa(pte);
	pmd_val(pmd[1]) = _PAGE_TABLE + __pa(pte+256);
	pmd_val(pmd[2]) = _PAGE_TABLE + __pa(pte+512);
	pmd_val(pmd[3]) = _PAGE_TABLE + __pa(pte+768);
}

/*
 * page table entry allocation/free routines.
 */
extern inline pte_t * pte_alloc_one(struct mm_struct *mm, unsigned long vmaddr)
{
	pte_t *pte;
        int i;

	pte = (pte_t *) __get_free_page(GFP_KERNEL);
	if (pte != NULL) {
		for (i=0; i < PTRS_PER_PTE; i++)
			pte_clear(pte+i);
	}
	return pte;
}

extern __inline__ pte_t *
pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
        unsigned long *ret = (unsigned long *) pte_quicklist;

        if (ret != NULL) {
                pte_quicklist = (unsigned long *)(*ret);
                ret[0] = ret[1];
                pgtable_cache_size--;
        }
        return (pte_t *)ret;
}

extern __inline__ void pte_free_fast(pte_t *pte)
{
        *(unsigned long *)pte = (unsigned long) pte_quicklist;
        pte_quicklist = (unsigned long *) pte;
        pgtable_cache_size++;
}

extern __inline__ void pte_free_slow(pte_t *pte)
{
        free_page((unsigned long) pte);
}

#define pte_free(pte)           pte_free_fast(pte)

extern int do_check_pgt_cache(int, int);

/*
 * This establishes kernel virtual mappings (e.g., as a result of a
 * vmalloc call).  Since s390-esame uses a separate kernel page table,
 * there is nothing to do here... :)
 */
#define set_pgdir(addr,entry) do { } while(0)

/*
 * TLB flushing:
 *
 *  - flush_tlb() flushes the current mm struct TLBs
 *  - flush_tlb_all() flushes all processes TLBs 
 *    called only from vmalloc/vfree
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *  - flush_tlb_pgtables(mm, start, end) flushes a range of page tables
 */

/*
 * S/390 has three ways of flushing TLBs
 * 'ptlb' does a flush of the local processor
 * 'csp' flushes the TLBs on all PUs of a SMP
 * 'ipte' invalidates a pte in a page table and flushes that out of
 * the TLBs of all PUs of a SMP
 */

#define local_flush_tlb() \
do {  __asm__ __volatile__("ptlb": : :"memory"); } while (0)


#ifndef CONFIG_SMP

/*
 * We always need to flush, since s390 does not flush tlb
 * on each context switch
 */

static inline void flush_tlb(void)
{
	local_flush_tlb();
}
static inline void flush_tlb_all(void)
{
	local_flush_tlb();
}
static inline void flush_tlb_mm(struct mm_struct *mm) 
{
	local_flush_tlb();
}
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long addr)
{
	local_flush_tlb();
}
static inline void flush_tlb_range(struct mm_struct *mm,
				   unsigned long start, unsigned long end)
{
	local_flush_tlb();
}

#else

#include <asm/smp.h>

extern void smp_ptlb_all(void);
static inline void global_flush_tlb_csp(void)
{
	int cs1=0,dum=0;
	int *adr;
	long long dummy=0;
	adr = (int*) (((int)(((int*) &dummy)+1) & 0xfffffffc)|1);
	__asm__ __volatile__("lr    2,%0\n\t"
                             "lr    3,%1\n\t"
                             "lr    4,%2\n\t"
                             "csp   2,4" :
                             : "d" (cs1), "d" (dum), "d" (adr)
                             : "2", "3", "4");
}
static inline void global_flush_tlb(void)
{
	if (MACHINE_HAS_CSP)
		global_flush_tlb_csp();
	else
		smp_ptlb_all();
}

/*
 * We only have to do global flush of tlb if process run since last
 * flush on any other pu than current. 
 * If we have threads (mm->count > 1) we always do a global flush, 
 * since the process runs on more than one processor at the same time.
 */

static inline void __flush_tlb_mm(struct mm_struct * mm)
{
	if (mm->cpu_vm_mask != (1UL << smp_processor_id())) {
		/* mm was active on more than one cpu. */
		if (mm == current->active_mm &&
		    atomic_read(&mm->mm_users) == 1)
			/* this cpu is the only one using the mm. */
			mm->cpu_vm_mask = 1UL << smp_processor_id();
		global_flush_tlb();
	} else
		local_flush_tlb();
}

static inline void flush_tlb(void)
{
	__flush_tlb_mm(current->mm);
}
static inline void flush_tlb_all(void)
{
	global_flush_tlb();
}
static inline void flush_tlb_mm(struct mm_struct *mm) 
{
	__flush_tlb_mm(mm); 
}
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long addr)
{
	__flush_tlb_mm(vma->vm_mm);
}
static inline void flush_tlb_range(struct mm_struct *mm,
				   unsigned long start, unsigned long end)
{
	__flush_tlb_mm(mm); 
}

#endif

extern inline void flush_tlb_pgtables(struct mm_struct *mm,
                                      unsigned long start, unsigned long end)
{
        /* S/390 does not keep any page table caches in TLB */
}


static inline int ptep_test_and_clear_and_flush_young(struct vm_area_struct *vma, 
                                                      unsigned long address, pte_t *ptep)
{
	/* No need to flush TLB; bits are in storage key */
	return ptep_test_and_clear_young(ptep);
}

static inline int ptep_test_and_clear_and_flush_dirty(struct vm_area_struct *vma, 
                                                      unsigned long address, pte_t *ptep)
{
	/* No need to flush TLB; bits are in storage key */
	return ptep_test_and_clear_dirty(ptep);
}

static inline pte_t ptep_invalidate(struct vm_area_struct *vma, 
                                    unsigned long address, pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!(pte_val(pte) & _PAGE_INVALID)) {
		/* S390 has 1mb segments, we are emulating 4MB segments */
		pte_t *pto = (pte_t *) (((unsigned long) ptep) & 0x7ffffc00);
		__asm__ __volatile__ ("ipte %0,%1" : : "a" (pto), "a" (address));
	}
	pte_clear(ptep);
	return pte;
}

static inline void ptep_establish(struct vm_area_struct *vma, 
                                  unsigned long address, pte_t *ptep, pte_t entry)
{
	ptep_invalidate(vma, address, ptep);
	set_pte(ptep, entry);
}

#endif /* _S390_PGALLOC_H */
