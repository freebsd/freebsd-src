/*
 *  linux/arch/m68k/mm/init.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *
 *  Contains common initialization routines, specific init code moved
 *  to motorola.c and sun3mmu.c
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif

#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/system.h>
#include <asm/machdep.h>
#include <asm/io.h>
#ifdef CONFIG_ATARI
#include <asm/atari_stram.h>
#endif
#include <asm/tlb.h>

mmu_gather_t mmu_gathers[NR_CPUS];

unsigned long totalram_pages = 0;

int do_check_pgt_cache(int low, int high)
{
	int freed = 0;
	if(pgtable_cache_size > high) {
		do {
			if(pmd_quicklist)
				freed += free_pmd_slow(get_pmd_fast());
			if(pte_quicklist)
				free_pte_slow(get_pte_fast()), freed++;
		} while(pgtable_cache_size > low);
	}
	return freed;
}

/*
 * BAD_PAGE is the page that is used for page faults when linux
 * is out-of-memory. Older versions of linux just did a
 * do_exit(), but using this instead means there is less risk
 * for a process dying in kernel mode, possibly leaving an inode
 * unused etc..
 *
 * BAD_PAGETABLE is the accompanying page-table: it is initialized
 * to point to BAD_PAGE entries.
 *
 * ZERO_PAGE is a special page that is used for zero-initialized
 * data and COW.
 */
unsigned long empty_bad_page_table;

pte_t *__bad_pagetable(void)
{
    memset((void *)empty_bad_page_table, 0, PAGE_SIZE);
    return (pte_t *)empty_bad_page_table;
}

unsigned long empty_bad_page;

pte_t __bad_page(void)
{
    memset ((void *)empty_bad_page, 0, PAGE_SIZE);
    return pte_mkdirty(__mk_pte(empty_bad_page, PAGE_SHARED));
}

unsigned long empty_zero_page;

void show_mem(void)
{
    unsigned long i;
    int free = 0, total = 0, reserved = 0, shared = 0;
    int cached = 0;

    printk("\nMem-info:\n");
    show_free_areas();
    printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
    i = max_mapnr;
    while (i-- > 0) {
	total++;
	if (PageReserved(mem_map+i))
	    reserved++;
	else if (PageSwapCache(mem_map+i))
	    cached++;
	else if (!page_count(mem_map+i))
	    free++;
	else
	    shared += page_count(mem_map+i) - 1;
    }
    printk("%d pages of RAM\n",total);
    printk("%d free pages\n",free);
    printk("%d reserved pages\n",reserved);
    printk("%d pages shared\n",shared);
    printk("%d pages swap cached\n",cached);
    printk("%ld pages in page table cache\n",pgtable_cache_size);
    show_buffers();
}

extern void init_pointer_table(unsigned long ptable);

/* References to section boundaries */

extern char _text, _etext, _edata, __bss_start, _end;
extern char __init_begin, __init_end;

extern pmd_t *zero_pgtable;

void __init mem_init(void)
{
	int codepages = 0;
	int datapages = 0;
	int initpages = 0;
	unsigned long tmp;
	int i;

	max_mapnr = num_physpages = MAP_NR(high_memory);

#ifdef CONFIG_ATARI
	if (MACH_IS_ATARI)
		atari_stram_mem_init_hook();
#endif

	/* this will put all memory onto the freelists */
	totalram_pages = free_all_bootmem();

	for (tmp = PAGE_OFFSET ; tmp < (unsigned long)high_memory; tmp += PAGE_SIZE) {
#if 0
#ifndef CONFIG_SUN3
		if (virt_to_phys ((void *)tmp) >= mach_max_dma_address)
			clear_bit(PG_DMA, &virt_to_page(tmp)->flags);
#endif
#endif
		if (PageReserved(virt_to_page(tmp))) {
			if (tmp >= (unsigned long)&_text
			    && tmp < (unsigned long)&_etext)
				codepages++;
			else if (tmp >= (unsigned long) &__init_begin
				 && tmp < (unsigned long) &__init_end)
				initpages++;
			else
				datapages++;
			continue;
		}
#if 0
		set_page_count(virt_to_page(tmp), 1);
#ifdef CONFIG_BLK_DEV_INITRD
		if (!initrd_start ||
		    (tmp < (initrd_start & PAGE_MASK) || tmp >= initrd_end))
#endif
			free_page(tmp);
#endif
	}
	
#ifndef CONFIG_SUN3
	/* insert pointer tables allocated so far into the tablelist */
	init_pointer_table((unsigned long)kernel_pg_dir);
	for (i = 0; i < PTRS_PER_PGD; i++) {
		if (pgd_present(kernel_pg_dir[i]))
			init_pointer_table(__pgd_page(kernel_pg_dir[i]));
	}

	/* insert also pointer table that we used to unmap the zero page */
	if (zero_pgtable)
		init_pointer_table((unsigned long)zero_pgtable);
#endif

	printk("Memory: %luk/%luk available (%dk kernel code, %dk data, %dk init)\n",
	       (unsigned long)nr_free_pages() << (PAGE_SHIFT-10),
	       max_mapnr << (PAGE_SHIFT-10),
	       codepages << (PAGE_SHIFT-10),
	       datapages << (PAGE_SHIFT-10),
	       initpages << (PAGE_SHIFT-10));
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	int pages = 0;
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		set_page_count(virt_to_page(start), 1);
		free_page(start);
		totalram_pages++;
		pages++;
	}
	printk ("Freeing initrd memory: %dk freed\n", pages);
}
#endif

void si_meminfo(struct sysinfo *val)
{
    unsigned long i;

    i = max_mapnr;
    val->totalram = totalram_pages;
    val->sharedram = 0;
    val->freeram = nr_free_pages();
    val->bufferram = atomic_read(&buffermem_pages);
    val->totalhigh = 0;
    val->freehigh = 0;
    val->mem_unit = PAGE_SIZE;
    return;
}
