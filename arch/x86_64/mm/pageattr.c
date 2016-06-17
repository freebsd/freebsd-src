/* 
 * Copyright 2002 Andi Kleen, SuSE Labs. 
 * Thanks to Ben LaHaise for precious feedback.
 */ 

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/io.h>

static inline pte_t *lookup_address(unsigned long address) 
{ 
	pgd_t *pgd = pgd_offset_k(address);
	pmd_t *pmd;
	
	if (!pgd) return NULL; 
	pmd = pmd_offset(pgd, address);
	if (!pmd) return NULL; 
	if ((pmd_val(*pmd) & PAGE_LARGE) == PAGE_LARGE)
		return (pte_t *)pmd; 

        return pte_offset(pmd, address);
} 

static struct page *split_large_page(unsigned long address, pgprot_t prot)
{ 
	int i; 
	unsigned long addr;
	struct page *base = alloc_pages(GFP_KERNEL, 0);
	pte_t *pbase;
	if (!base) 
		return NULL;
	address = __pa(address);
	addr = address & LARGE_PAGE_MASK; 
	pbase = (pte_t *)page_address(base);
	for (i = 0; i < PTRS_PER_PTE; i++, addr += PAGE_SIZE) {
		pbase[i] = mk_pte_phys(addr, 
				      addr == address ? prot : PAGE_KERNEL);
	}
	return base;
} 

static void flush_kernel_map(void * address) 
{
	struct cpuinfo_x86 *cpu = &cpu_data[smp_processor_id()]; 
	wmb(); 
	if (0 && test_bit(X86_FEATURE_CLFLSH, &cpu->x86_capability)) { 
		/* is this worth it? */ 
		int i;
		for (i = 0; i < PAGE_SIZE; i += cpu->x86_clflush_size) 
			asm volatile("clflush (%0)" :: "r" (address + i)); 
	} else
		asm volatile("wbinvd":::"memory"); 
	__flush_tlb_one(address);
}

/* no more special protections in this 2MB area - revert to a
   large page again. */
static inline void revert_page(struct page *kpte_page, unsigned long address)
{
	pgd_t *pgd;
	pmd_t *pmd; 
	pte_t large_pte; 

	pgd = pgd_offset_k(address); 
	if (!pgd) BUG(); 
	pmd = pmd_offset(pgd, address);
	if (!pmd) BUG(); 
	if ((pmd_val(*pmd) & _PAGE_GLOBAL) == 0) BUG(); 
	
	large_pte = mk_pte_phys(__pa(address) & LARGE_PAGE_MASK, PAGE_KERNEL_LARGE); 
	set_pte((pte_t *)pmd, large_pte);
}	
 
/*
 * Change the page attributes of an page in the linear mapping.
 *
 * This should be used when a page is mapped with a different caching policy
 * than write-back somewhere - some CPUs do not like it when mappings with
 * different caching policies exist. This changes the page attributes of the
 * in kernel linear mapping too.
 * 
 * The caller needs to ensure that there are no conflicting mappings elsewhere.
 * This function only deals with the kernel linear map.
 * When page is in highmem it must never be kmap'ed.
 */
static int 
__change_page_attr(unsigned long address, struct page *page, pgprot_t prot, 
		   struct page **oldpage) 
{ 
	pte_t *kpte; 
	struct page *kpte_page;

	kpte = lookup_address(address);
	if (!kpte) 
		return 0; /* not mapped in kernel */
	kpte_page = virt_to_page(((unsigned long)kpte) & PAGE_MASK);
	if (pgprot_val(prot) != pgprot_val(PAGE_KERNEL)) { 
		if ((pte_val(*kpte) & _PAGE_PSE) == 0) { 
			pte_t old = *kpte;
			pte_t standard = mk_pte(page, PAGE_KERNEL); 

			set_pte(kpte, mk_pte(page, prot)); 
			if (pte_same(old,standard))
				atomic_inc(&kpte_page->count);
		} else {
			struct page *split = split_large_page(address, prot); 
			if (!split)
				return -ENOMEM;
			set_pte(kpte,mk_pte(split, PAGE_KERNEL));
		}	
	} else if ((pte_val(*kpte) & _PAGE_PSE) == 0) { 
		set_pte(kpte, mk_pte(page, PAGE_KERNEL));
		atomic_dec(&kpte_page->count); 
	}

	if (atomic_read(&kpte_page->count) == 1) { 
		*oldpage = kpte_page;
		revert_page(kpte_page, address);
	} 
	return 0;
} 

static inline void flush_and_free(void *address, struct page *fpage)
{
#ifdef CONFIG_SMP			
	smp_call_function(flush_kernel_map, address, 1, 1);
#endif				
	flush_kernel_map(address); 
	if (fpage)
		__free_page(fpage); 
}

int change_page_attr(struct page *page, int numpages, pgprot_t prot)
{
	int err = 0; 
	struct page *fpage, *fpage2; 
	int i; 

	down_write(&init_mm.mmap_sem);
	for (i = 0; i < numpages; i++, page++) { 
		fpage = fpage2 = NULL;
		err = __change_page_attr((unsigned long)page_address(page), 
					 page, prot, &fpage); 
		
		/* Handle kernel mapping too which aliases part of the lowmem */
		if (!err && page_to_phys(page) < KERNEL_TEXT_SIZE) { 
			err = __change_page_attr((unsigned long) __START_KERNEL_map + 
						 page_to_phys(page),
						 page, prot, &fpage2); 
		} 

		if (err) 
			break; 
		
		if (fpage || fpage2 || i == numpages-1) { 
			flush_and_free(page_address(page), fpage); 
			if (unlikely(fpage2 != NULL))
				flush_and_free((char *)__START_KERNEL_map + 
					       page_to_phys(page), fpage2);
		} 
	} 	
	up_write(&init_mm.mmap_sem); 
	return err;
}

EXPORT_SYMBOL(change_page_attr);
