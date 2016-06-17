/* sun3_pgalloc.h --
 * reorganization around 2.3.39, routines moved from sun3_pgtable.h 
 *
 * moved 1/26/2000 Sam Creasey
 */

#ifndef _SUN3_PGALLOC_H
#define _SUN3_PGALLOC_H

/* Pagetable caches. */
//todo: should implement for at least ptes. --m
#define pgd_quicklist ((unsigned long *) 0)
#define pmd_quicklist ((unsigned long *) 0)
#define pte_quicklist ((unsigned long *) 0)
#define pgtable_cache_size (0L)

/* Allocation and deallocation of various flavours of pagetables. */
static inline int free_pmd_fast(pmd_t *pmdp) { return 0; }
static inline int free_pmd_slow(pmd_t *pmdp) { return 0; }
static inline pmd_t *get_pmd_fast (void) { return (pmd_t *) 0; }

//todo: implement the following properly.
#define get_pte_fast() ((pte_t *) 0)
#define get_pte_slow pte_alloc
#define free_pte_fast(pte)
#define free_pte_slow pte_free

/* FIXME - when we get this compiling */
/* erm, now that it's compiling, what do we do with it? */
#define _KERNPG_TABLE 0

static inline void pte_free_kernel(pte_t *pte)
{
        free_page((unsigned long) pte);
}

extern const char bad_pmd_string[];

static inline pte_t *pte_alloc_kernel(pmd_t *pmd, unsigned long address)
{
        address = (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
        if (pmd_none(*pmd)) {
                pte_t * page = (pte_t *) get_free_page(GFP_KERNEL);
                if (pmd_none(*pmd)) {
                        if (page) {
                                pmd_val(*pmd) = _KERNPG_TABLE + __pa(page);
                                return page + address;
                        }
                        pmd_val(*pmd) = _KERNPG_TABLE + __pa((unsigned long)BAD_PAGETABLE);
                        return NULL;
                }
                free_page((unsigned long) page);
        }
        if (pmd_bad(*pmd)) {
                printk(bad_pmd_string, pmd_val(*pmd));
		printk("at kernel pgd off %08x\n", (unsigned int)pmd);
                pmd_val(*pmd) = _KERNPG_TABLE + __pa((unsigned long)BAD_PAGETABLE);
                return NULL;
        }
        return (pte_t *) __pmd_page(*pmd) + address;
}

/*
 * allocating and freeing a pmd is trivial: the 1-entry pmd is
 * inside the pgd, so has no extra memory associated with it.
 */
static inline void pmd_free_kernel(pmd_t *pmd)
{
//        pmd_val(*pmd) = 0;
}

static inline pmd_t *pmd_alloc_kernel(pgd_t *pgd, unsigned long address)
{
        return (pmd_t *) pgd;
}

#define pmd_alloc_one_fast(mm, address) ({ BUG(); ((pmd_t *)1); })
#define pmd_alloc_one(mm,address)       ({ BUG(); ((pmd_t *)2); })

static inline void pte_free(pte_t *pte)
{
        free_page((unsigned long) pte);
}

static inline pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	unsigned long page = __get_free_page(GFP_KERNEL);

	if (!page)
		return NULL;
		
	memset((void *)page, 0, PAGE_SIZE);
//	pmd_val(*pmd) = SUN3_PMD_MAGIC + __pa(page);
/*	pmd_val(*pmd) = __pa(page); */
	return (pte_t *) (page);
}

#define pte_alloc_one_fast(mm,addr) pte_alloc_one(mm,addr)

#define pmd_populate(mm, pmd, pte) (pmd_val(*pmd) = __pa((unsigned long)pte))

/*
 * allocating and freeing a pmd is trivial: the 1-entry pmd is
 * inside the pgd, so has no extra memory associated with it.
 */
static inline void pmd_free(pmd_t *pmd)
{
        pmd_val(*pmd) = 0;
}

static inline void pgd_free(pgd_t *pgd)
{
        free_page((unsigned long) pgd);
}

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
     pgd_t *new_pgd;

     new_pgd = (pgd_t *)get_free_page(GFP_KERNEL);
     memcpy(new_pgd, swapper_pg_dir, PAGE_SIZE);
     memset(new_pgd, 0, (PAGE_OFFSET >> PGDIR_SHIFT));
     return new_pgd;
}

#define pgd_populate(mm, pmd, pte) BUG()

/* FIXME: the sun3 doesn't have a page table cache! 
   (but the motorola routine should just return 0) */

extern int do_check_pgt_cache(int, int);

static inline void set_pgdir(unsigned long address, pgd_t entry)
{
}

/* Reserved PMEGs. */
extern char sun3_reserved_pmeg[SUN3_PMEGS_NUM];
extern unsigned long pmeg_vaddr[SUN3_PMEGS_NUM];
extern unsigned char pmeg_alloc[SUN3_PMEGS_NUM];
extern unsigned char pmeg_ctx[SUN3_PMEGS_NUM];

/* Flush all userspace mappings one by one...  (why no flush command,
   sun?) */
static inline void flush_tlb_all(void)
{
       unsigned long addr;
       unsigned char ctx, oldctx;

       oldctx = sun3_get_context();
       for(addr = 0x00000000; addr < TASK_SIZE; addr += SUN3_PMEG_SIZE) {
	       for(ctx = 0; ctx < 8; ctx++) {
		       sun3_put_context(ctx);
		       sun3_put_segmap(addr, SUN3_INVALID_PMEG);
	       }
       }

       sun3_put_context(oldctx);
       /* erase all of the userspace pmeg maps, we've clobbered them
	  all anyway */
       for(addr = 0; addr < SUN3_INVALID_PMEG; addr++) {
	       if(pmeg_alloc[addr] == 1) {
		       pmeg_alloc[addr] = 0;
		       pmeg_ctx[addr] = 0;
		       pmeg_vaddr[addr] = 0;
	       }
       }

}

/* Clear user TLB entries within the context named in mm */
static inline void flush_tlb_mm (struct mm_struct *mm)
{
     unsigned char oldctx;
     unsigned char seg;
     unsigned long i;

     oldctx = sun3_get_context();
     sun3_put_context(mm->context);

     for(i = 0; i < TASK_SIZE; i += SUN3_PMEG_SIZE) {
	     seg = sun3_get_segmap(i);
	     if(seg == SUN3_INVALID_PMEG)
		     continue;
	     
	     sun3_put_segmap(i, SUN3_INVALID_PMEG);
	     pmeg_alloc[seg] = 0;
	     pmeg_ctx[seg] = 0;
	     pmeg_vaddr[seg] = 0;
     }

     sun3_put_context(oldctx);
     		     
}

/* Flush a single TLB page. In this case, we're limited to flushing a
   single PMEG */
static inline void flush_tlb_page (struct vm_area_struct *vma,
				   unsigned long addr)
{
	unsigned char oldctx;
	unsigned char i;

	oldctx = sun3_get_context();
	sun3_put_context(vma->vm_mm->context);
	addr &= ~SUN3_PMEG_MASK;
	if((i = sun3_get_segmap(addr)) != SUN3_INVALID_PMEG)
	{
		pmeg_alloc[i] = 0;
		pmeg_ctx[i] = 0;
		pmeg_vaddr[i] = 0;
		sun3_put_segmap (addr,  SUN3_INVALID_PMEG);     
	}
	sun3_put_context(oldctx);

}
/* Flush a range of pages from TLB. */

static inline void flush_tlb_range (struct mm_struct *mm,
		      unsigned long start, unsigned long end)
{
	unsigned char seg, oldctx;
	
	start &= ~SUN3_PMEG_MASK;

	oldctx = sun3_get_context();
	sun3_put_context(mm->context);

	while(start < end)
	{
		if((seg = sun3_get_segmap(start)) == SUN3_INVALID_PMEG) 
		     goto next;
		if(pmeg_ctx[seg] == mm->context) {
			pmeg_alloc[seg] = 0;
			pmeg_ctx[seg] = 0;
			pmeg_vaddr[seg] = 0;
		}
		sun3_put_segmap(start, SUN3_INVALID_PMEG);
	next:
		start += SUN3_PMEG_SIZE;
	}
}

/* Flush kernel page from TLB. */
static inline void flush_tlb_kernel_page (unsigned long addr)
{
	sun3_put_segmap (addr & ~(SUN3_PMEG_SIZE - 1), SUN3_INVALID_PMEG);
}

static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
}

#endif /* SUN3_PGALLOC_H */
