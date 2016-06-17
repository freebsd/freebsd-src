/*
 *  arch/s390/mm/init.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hpenner@de.ibm.com)
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1995  Linus Torvalds
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/init.h>
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/blk.h>
#endif
#include <linux/pagemap.h>
#include <linux/bootmem.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/lowcore.h>
#include <asm/tlb.h>

mmu_gather_t mmu_gathers[NR_CPUS];

static unsigned long totalram_pages;
extern unsigned long memory_size;

pgd_t swapper_pg_dir[PTRS_PER_PGD] __attribute__((__aligned__(PAGE_SIZE)));
char  empty_zero_page[PAGE_SIZE] __attribute__((__aligned__(PAGE_SIZE)));

static inline int
__pgd_populate(unsigned long *pgd_slot, unsigned long offset, pmd_t *pmd)
{
	if (offset == 0 && 
	    ((*pgd_slot & _PGD_ENTRY_INV) != 0 ||
	     (*pgd_slot & _PGD_ENTRY_LEN(2)) == 0)) {
		/* Set lower pmd, upper pmd is empty. */
		*pgd_slot = __pa(pmd) | _PGD_ENTRY_MASK |
				_PGD_ENTRY_OFF(0) | _PGD_ENTRY_LEN(1);
		return 1;
	}
	if (offset == 4 &&
	    ((*pgd_slot & _PGD_ENTRY_INV) != 0 ||
	     (*pgd_slot & _PGD_ENTRY_OFF(2)) != 0)) {
		/* Lower pmd empty, set upper pmd. */
		*pgd_slot = (__pa(pmd) - 0x2000) | _PGD_ENTRY_MASK |
				_PGD_ENTRY_OFF(2) | _PGD_ENTRY_LEN(3);
		return 1;
	}
        return 0;
}

pmd_t *pgd_populate(struct mm_struct *mm, pgd_t *pgd, pmd_t *pmd)
{
        unsigned long addr = (unsigned long) pgd;
        unsigned long *pgd_slot =  (unsigned long *) (addr & -8);
        unsigned long offset = addr & 4;
	pmd_t *new, *pmd2;
	int i;

        /* Check if we can get away with a half-sized pmd. */
        if (__pgd_populate(pgd_slot, offset, pmd))
                return pmd;

        /* We have to enlarge the pmd to 16K if we arrive here. */
        spin_unlock(&mm->page_table_lock);
	new = (pmd_t *) __get_free_pages(GFP_KERNEL, 2);
        spin_lock(&mm->page_table_lock);

        /*
         * Because we dropped the lock, we should re-check the
         * entry, as somebody else could have populated it..
         */
        if (!pgd_none(*pgd)) {
                if (new)
                        free_pages((unsigned long) new, 2);
                pmd_free(pmd);
                return (pmd_t *) pgd_val(*pgd);
        }

	if (!new) {
		pmd_free(pmd);
		return NULL;
	}

        /*
         * Re-check if we can get away with a half-sized pmd. We
         * dropped the lock so somebody else could have freed the
         * other half of the pmd.
         */
        if (__pgd_populate(pgd_slot, offset, pmd)) {
                free_pages((unsigned long) new, 2);
                return pmd;
        }

	/* Set the PG_arch_1 bit on the first and the third pmd page
           so that pmd_free_fast can recognize pmds that have been
           allocated with an order 2 allocation.  */
	set_bit(PG_arch_1, &virt_to_page(new)->flags);
	set_bit(PG_arch_1, &virt_to_page(new+PTRS_PER_PMD)->flags);
	/* Now copy the two pmds to the new memory area. */
	if (offset == 0) {
		pmd2 = (pmd_t *)(*pgd_slot & PAGE_MASK) + PTRS_PER_PMD;
		memcpy(new, pmd, sizeof(pmd_t)*PTRS_PER_PMD);
		memcpy(new + PTRS_PER_PMD, pmd2, sizeof(pmd_t)*PTRS_PER_PMD);
	} else {
		pmd2 = (pmd_t *)(*pgd_slot & PAGE_MASK);
		memcpy(new, pmd2, sizeof(pmd_t)*PTRS_PER_PMD);
		memcpy(new + PTRS_PER_PMD, pmd, sizeof(pmd_t)*PTRS_PER_PMD);
	}
	*pgd_slot = __pa(new) | _PGD_ENTRY_MASK |
			_PGD_ENTRY_OFF(0) | _PGD_ENTRY_LEN(3);
	for (i = 0; i < PTRS_PER_PMD; i++) {
		pmd_clear(pmd + i);
		pmd_clear(pmd2 + i);
	}
	pmd_free(pmd);
	pmd_free(pmd2);
	return new;
}

void pmd_free_order2(pmd_t *pmd)
{
	pmd_t *pmd2 = (pmd_t *) ((unsigned long) pmd ^ 8192);

	clear_bit(PG_arch_1, &virt_to_page(pmd)->flags);
	if (test_bit(PG_arch_1, &virt_to_page(pmd2)->flags) == 0) {
		/* The other pmd of the order 2 allocation has already
		   been freed. Now we can release the order 2 allocation.  */
		free_pages((unsigned long) pmd & ~8192, 2);
	}
}

int do_check_pgt_cache(int low, int high)
{
        int freed = 0;
        if(pgtable_cache_size > high) {
                do {
                        if(pgd_quicklist) {
				free_pgd_slow(get_pgd_fast());
				freed += 2;
			}
                        if(pmd_quicklist) {
				pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
				freed += 2;
			}
                        if(pte_quicklist) {
				pte_free_slow(pte_alloc_one_fast(NULL, 0));
				freed += 1;
			}
                } while(pgtable_cache_size > low);
        }
        return freed;
}

void diag10(unsigned long addr)
{
        if (addr >= 0x80000000)
                return;
        asm volatile ("sam31\n\t"
                      "diag %0,%0,0x10\n\t"
                      "sam64" : : "a" (addr) );
}

void show_mem(void)
{
        int i, total = 0,reserved = 0;
        int shared = 0, cached = 0;

        printk("Mem-info:\n");
        show_free_areas();
        printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
        i = max_mapnr;
        while (i-- > 0) {
                total++;
                if (PageReserved(mem_map+i))
                        reserved++;
                else if (PageSwapCache(mem_map+i))
                        cached++;
                else if (page_count(mem_map+i))
                        shared += atomic_read(&mem_map[i].count) - 1;
        }
        printk("%d pages of RAM\n",total);
        printk("%d reserved pages\n",reserved);
        printk("%d pages shared\n",shared);
        printk("%d pages swap cached\n",cached);
        printk("%ld pages in page table cache\n",pgtable_cache_size);
        show_buffers();
}

/* References to section boundaries */

extern unsigned long _text;
extern unsigned long _etext;
extern unsigned long _edata;
extern unsigned long __bss_start;
extern unsigned long _end;

extern unsigned long __init_begin;
extern unsigned long __init_end;

/*
 * paging_init() sets up the page tables
 */

unsigned long last_valid_pfn;

void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = {0, 0, 0};
	static const int ssm_mask = 0x04000000L;
	unsigned long dma_pfn, address, end_mem;
        pgd_t * pg_dir;
	int     i,j,k;

	dma_pfn = MAX_DMA_ADDRESS >> PAGE_SHIFT;

	if (dma_pfn > max_low_pfn)
		zones_size[ZONE_DMA] = max_low_pfn;
	else {
		zones_size[ZONE_DMA] = dma_pfn;
		zones_size[ZONE_NORMAL] = max_low_pfn - dma_pfn;
	}

	/* Initialize mem_map[].  */
	free_area_init(zones_size);

	/*
	 * map whole physical memory to virtual memory (identity mapping) 
	 */
        pg_dir = swapper_pg_dir;
	address = 0;
	end_mem = (unsigned long) __va(max_low_pfn*PAGE_SIZE);
        for (i = 0 ; i < PTRS_PER_PGD/2 ; i++, pg_dir += 2) {
		pmd_t *pm_dir;

                if (address >= end_mem) {
                        pgd_clear(pg_dir);
                        continue;
                }
        
	        pm_dir = (pmd_t *) alloc_bootmem_low_pages(PAGE_SIZE*4);
		*((unsigned long *) pg_dir) = __pa(pm_dir) | _PGD_ENTRY_MASK |
			_PGD_ENTRY_LEN(3) | _PGD_ENTRY_OFF(0);

                for (j = 0 ; j < PTRS_PER_PMD*2 ; j++, pm_dir++) {
			pte_t *pt_dir;

                        if (address >= end_mem) {
                                pmd_clear(pm_dir);
                                continue; 
                        }          
                        
                        pt_dir = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
                        pmd_populate(&init_mm, pm_dir, pt_dir);
	
                        for (k = 0 ; k < PTRS_PER_PTE ; k++,pt_dir++) {
                                pte_t pte = mk_pte_phys(address, PAGE_KERNEL);
                                if (address >= end_mem) {
                                        pte_clear(&pte); 
                                        continue;
                                }
                                set_pte(pt_dir, pte);
                                address += PAGE_SIZE;
                        }
                }
        }
        
        /* enable virtual mapping in kernel mode */
        __asm__ __volatile__("lctlg 1,1,%0\n\t"
                             "lctlg 7,7,%0\n\t"
                             "lctlg 13,13,%0\n\t"
                             "ssm   %1"
			     : :"m" (__pa(swapper_pg_dir) | _KERN_REGION_TABLE),
			        "m" (ssm_mask));
        local_flush_tlb();

        return;
}

void __init mem_init(void)
{
	unsigned long codesize, reservedpages, datasize, initsize;

        max_mapnr = num_physpages = max_low_pfn;
        high_memory = (void *) __va(max_low_pfn * PAGE_SIZE);

        /* clear the zero-page */
        memset(empty_zero_page, 0, PAGE_SIZE);

	/* this will put all low memory onto the freelists */
	totalram_pages += free_all_bootmem();

	reservedpages = 0;

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;
        printk("Memory: %luk/%luk available (%ldk kernel code, %ldk reserved, %ldk data, %ldk init)\n",
                (unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
                max_mapnr << (PAGE_SHIFT-10),
                codesize >> 10,
                reservedpages << (PAGE_SHIFT-10),
                datasize >>10,
                initsize >> 10);
}

void free_initmem(void)
{
        unsigned long addr;

        addr = (unsigned long)(&__init_begin);
        for (; addr < (unsigned long)(&__init_end); addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		set_page_count(virt_to_page(addr), 1);
		free_page(addr);
		totalram_pages++;
        }
        printk (KERN_INFO "Freeing unused kernel memory: %ldk freed\n",
		(&__init_end - &__init_begin) >> 10);
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
        if (start < end)
                printk (KERN_INFO "Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
	for (; start < end; start += PAGE_SIZE) {
                ClearPageReserved(virt_to_page(start));
                set_page_count(virt_to_page(start), 1);
		free_page(start);
		totalram_pages++;
	}
}
#endif

void si_meminfo(struct sysinfo *val)
{
        val->totalram = totalram_pages;
	val->sharedram = 0;
	val->freeram = nr_free_pages();
	val->bufferram = atomic_read(&buffermem_pages);
	val->totalhigh = 0;
	val->freehigh = 0;
	val->mem_unit = PAGE_SIZE;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4 
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
