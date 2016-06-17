#ifndef _MOTOROLA_PGALLOC_H
#define _MOTOROLA_PGALLOC_H

extern struct pgtable_cache_struct {
	unsigned long *pmd_cache;
	unsigned long *pte_cache;
/* This counts in units of pointer tables, of which can be eight per page. */
	unsigned long pgtable_cache_sz;
} quicklists;

#define pgd_quicklist ((unsigned long *)0)
#define pmd_quicklist (quicklists.pmd_cache)
#define pte_quicklist (quicklists.pte_cache)
/* This isn't accurate because of fragmentation of allocated pages for
   pointer tables, but that should not be a problem. */
#define pgtable_cache_size ((quicklists.pgtable_cache_sz+7)/8)

extern pte_t *get_pte_slow(pmd_t *pmd, unsigned long offset);
extern pmd_t *get_pmd_slow(pgd_t *pgd, unsigned long offset);

extern pmd_t *get_pointer_table(void);
extern int free_pointer_table(pmd_t *);


static inline void flush_tlb_kernel_page(unsigned long addr)
{
	if (CPU_IS_040_OR_060) {
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);
		__asm__ __volatile__(".chip 68040\n\t"
				     "pflush (%0)\n\t"
				     ".chip 68k"
				     : : "a" (addr));
		set_fs(old_fs);
	} else
		__asm__ __volatile__("pflush #4,#4,(%0)" : : "a" (addr));
}


static inline pte_t *get_pte_fast(void)
{
	unsigned long *ret;

	ret = pte_quicklist;
	if (ret) {
		pte_quicklist = (unsigned long *)*ret;
		ret[0] = 0;
		quicklists.pgtable_cache_sz -= 8;
	}
	return (pte_t *)ret;
}
#define pte_alloc_one_fast(mm,addr)  get_pte_fast()

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
 	pte_t *pte;

	pte = (pte_t *) __get_free_page(GFP_KERNEL);
	if (pte) {
	         clear_page(pte);
		 __flush_page_to_ram((unsigned long)pte);
		 flush_tlb_kernel_page((unsigned long)pte);
		 nocache_page((unsigned long)pte);
		}

	return pte;
}


static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
        return get_pointer_table();
}


static inline void free_pte_fast(pte_t *pte)
{
	*(unsigned long *)pte = (unsigned long)pte_quicklist;
	pte_quicklist = (unsigned long *)pte;
	quicklists.pgtable_cache_sz += 8;
}

static inline void free_pte_slow(pte_t *pte)
{
	cache_page((unsigned long)pte);
	free_page((unsigned long) pte);
}

static inline pmd_t *get_pmd_fast(void)
{
	unsigned long *ret;

	ret = pmd_quicklist;
	if (ret) {
		pmd_quicklist = (unsigned long *)*ret;
		ret[0] = 0;
		quicklists.pgtable_cache_sz--;
	}
	return (pmd_t *)ret;
}
#define pmd_alloc_one_fast(mm,addr) get_pmd_fast()

static inline void free_pmd_fast(pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long)pmd_quicklist;
	pmd_quicklist = (unsigned long *) pmd;
	quicklists.pgtable_cache_sz++;
}

static inline int free_pmd_slow(pmd_t *pmd)
{
	return free_pointer_table(pmd);
}

/* The pgd cache is folded into the pmd cache, so these are dummy routines. */
static inline pgd_t *get_pgd_fast(void)
{
	return (pgd_t *)0;
}

static inline void free_pgd_fast(pgd_t *pgd)
{
}

static inline void free_pgd_slow(pgd_t *pgd)
{
}

extern void __bad_pte(pmd_t *pmd);
extern void __bad_pmd(pgd_t *pgd);

static inline void pte_free(pte_t *pte)
{
	free_pte_fast(pte);
}

static inline void pmd_free(pmd_t *pmd)
{
	free_pmd_fast(pmd);
}


static inline void pte_free_kernel(pte_t *pte)
{
	free_pte_fast(pte);
}

static inline pte_t *pte_alloc_kernel(pmd_t *pmd, unsigned long address)
{
	return pte_alloc(&init_mm,pmd, address);
}

static inline void pmd_free_kernel(pmd_t *pmd)
{
	free_pmd_fast(pmd);
}

static inline pmd_t *pmd_alloc_kernel(pgd_t *pgd, unsigned long address)
{
	return pmd_alloc(&init_mm,pgd, address);
}

static inline void pgd_free(pgd_t *pgd)
{
	free_pmd_fast((pmd_t *)pgd);
}

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = (pgd_t *)get_pmd_fast();
	if (!pgd)
		pgd = (pgd_t *)get_pointer_table();
	return pgd;
}


#define pmd_populate(MM, PMD, PTE)	pmd_set(PMD, PTE)
#define pgd_populate(MM, PGD, PMD)	pgd_set(PGD, PMD)


extern int do_check_pgt_cache(int, int);

static inline void set_pgdir(unsigned long address, pgd_t entry)
{
}


/*
 * flush all user-space atc entries.
 */
static inline void __flush_tlb(void)
{
	if (CPU_IS_040_OR_060)
		__asm__ __volatile__(".chip 68040\n\t"
				     "pflushan\n\t"
				     ".chip 68k");
	else
		__asm__ __volatile__("pflush #0,#4");
}

static inline void __flush_tlb040_one(unsigned long addr)
{
	__asm__ __volatile__(".chip 68040\n\t"
			     "pflush (%0)\n\t"
			     ".chip 68k"
			     : : "a" (addr));
}

static inline void __flush_tlb_one(unsigned long addr)
{
	if (CPU_IS_040_OR_060)
		__flush_tlb040_one(addr);
	else
		__asm__ __volatile__("pflush #0,#4,(%0)" : : "a" (addr));
}

#define flush_tlb() __flush_tlb()

/*
 * flush all atc entries (both kernel and user-space entries).
 */
static inline void flush_tlb_all(void)
{
	if (CPU_IS_040_OR_060)
		__asm__ __volatile__(".chip 68040\n\t"
				     "pflusha\n\t"
				     ".chip 68k");
	else
		__asm__ __volatile__("pflusha");
}

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (mm == current->mm)
		__flush_tlb();
}

static inline void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	if (vma->vm_mm == current->mm)
		__flush_tlb_one(addr);
}

static inline void flush_tlb_range(struct mm_struct *mm,
				   unsigned long start, unsigned long end)
{
	if (mm == current->mm)
		__flush_tlb();
}


static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
}

#endif /* _MOTOROLA_PGALLOC_H */
