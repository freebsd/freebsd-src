/*
 *  linux/arch/cris/mm/init.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000,2001  Axis Communications AB
 *
 *  Authors:  Bjorn Wesen (bjornw@axis.com)
 *
 *  $Log: init.c,v $
 *  Revision 1.38  2003/04/01 14:12:08  starvik
 *  Added loglevel for lots of printks
 *
 *  Revision 1.37  2003/01/22 06:54:47  starvik
 *  Fixed warnings issued by GCC 3.2.1
 *
 *  Revision 1.36  2003/01/09 17:59:55  starvik
 *  Added init_ioremap to initcalls
 *
 *  Revision 1.35  2002/05/17 05:33:59  starvik
 *  Limit cache flush range to the size of the cache
 *
 *  Revision 1.34  2002/04/22 11:48:51  johana
 *  Added KERN_INFO (2.4.19-pre7)
 *
 *  Revision 1.33  2002/03/19 15:22:17  bjornw
 *  Added flush_etrax_cache
 *
 *  Revision 1.32  2002/03/15 17:09:31  bjornw
 *  Added prepare_rx_descriptor as a workaround for a bug
 *
 *  Revision 1.31  2001/11/13 16:22:00  bjornw
 *  Skip calculating totalram and sharedram in si_meminfo
 *
 *  Revision 1.30  2001/11/12 19:02:10  pkj
 *  Fixed compiler warnings.
 *
 *  Revision 1.29  2001/07/25 16:09:50  bjornw
 *  val->sharedram will stay 0
 *
 *  Revision 1.28  2001/06/28 16:30:17  bjornw
 *  Oops. This needs to wait until 2.4.6 is merged
 *
 *  Revision 1.27  2001/06/28 14:04:07  bjornw
 *  Fill in sharedram
 *
 *  Revision 1.26  2001/06/18 06:36:02  hp
 *  Enable free_initmem of __init-type pages
 *
 *  Revision 1.25  2001/06/13 00:02:23  bjornw
 *  Use a separate variable to store the current pgd to avoid races in schedule
 *
 *  Revision 1.24  2001/05/15 00:52:20  hp
 *  Only map segment 0xa as seg if CONFIG_JULIETTE
 *
 *  Revision 1.23  2001/04/04 14:35:40  bjornw
 *  * Removed get_pte_slow and friends (2.4.3 change)
 *  * Removed bad_pmd handling (2.4.3 change)
 *
 *  Revision 1.22  2001/04/04 13:38:04  matsfg
 *  Moved ioremap to a separate function instead
 *
 *  Revision 1.21  2001/03/27 09:28:33  bjornw
 *  ioremap used too early - lets try it in mem_init instead
 *
 *  Revision 1.20  2001/03/23 07:39:21  starvik
 *  Corrected according to review remarks
 *
 *  Revision 1.19  2001/03/15 14:25:17  bjornw
 *  More general shadow registers and ioremaped addresses for external I/O
 *
 *  Revision 1.18  2001/02/23 12:46:44  bjornw
 *  * 0xc was not CSE1; 0x8 is, same as uncached flash, so we move the uncached
 *    flash during CRIS_LOW_MAP from 0xe to 0x8 so both the flash and the I/O
 *    is mapped straight over (for !CRIS_LOW_MAP the uncached flash is still 0xe)
 *
 *  Revision 1.17  2001/02/22 15:05:21  bjornw
 *  Map 0x9 straight over during LOW_MAP to allow for memory mapped LEDs
 *
 *  Revision 1.16  2001/02/22 15:02:35  bjornw
 *  Map 0xc straight over during LOW_MAP to allow for memory mapped I/O
 *
 *  Revision 1.15  2001/01/10 21:12:10  bjornw
 *  loops_per_sec -> loops_per_jiffy
 *
 *  Revision 1.14  2000/11/22 16:23:20  bjornw
 *  Initialize totalhigh counters to 0 to make /proc/meminfo look nice.
 *
 *  Revision 1.13  2000/11/21 16:37:51  bjornw
 *  Temporarily disable initmem freeing
 *
 *  Revision 1.12  2000/11/21 13:55:07  bjornw
 *  Use CONFIG_CRIS_LOW_MAP for the low VM map instead of explicit CPU type
 *
 *  Revision 1.11  2000/10/06 12:38:22  bjornw
 *  Cast empty_bad_page correctly (should really be of * type from the start..
 *
 *  Revision 1.10  2000/10/04 16:53:57  bjornw
 *  Fix memory-map due to LX features
 *
 *  Revision 1.9  2000/09/13 15:47:49  bjornw
 *  Wrong count in reserved-pages loop
 *
 *  Revision 1.8  2000/09/13 14:35:10  bjornw
 *  2.4.0-test8 added a new arg to free_area_init_node
 *
 *  Revision 1.7  2000/08/17 15:35:55  bjornw
 *  2.4.0-test6 removed MAP_NR and inserted virt_to_page
 *
 *
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
#include <linux/bootmem.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/dma.h>
#include <asm/svinto.h>
#include <asm/io.h>
#include <asm/mmu_context.h>

static unsigned long totalram_pages;

struct pgtable_cache_struct quicklists;  /* see asm/pgalloc.h */

const char bad_pmd_string[] = "Bad pmd in pte_alloc: %08lx\n";

extern void die_if_kernel(char *,struct pt_regs *,long);
extern void show_net_buffers(void);
extern void tlb_init(void);


unsigned long empty_zero_page;

/* trim the page-table cache if necessary */

int 
do_check_pgt_cache(int low, int high)
{
        int freed = 0;

        if(pgtable_cache_size > high) {
                do {
                        if(pgd_quicklist) {
                                free_pgd_slow(get_pgd_fast());
                                freed++;
                        }
                        if(pmd_quicklist) {
                                pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
                                freed++;
                        }
                        if(pte_quicklist) {
                                pte_free_slow(pte_alloc_one_fast(NULL, 0));
				freed++;
                        }
                } while(pgtable_cache_size > low);
        }
        return freed;
}

void 
show_mem(void)
{
	int i,free = 0,total = 0,cached = 0, reserved = 0, nonshared = 0;
	int shared = 0;

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
		else if (page_count(mem_map+i) == 1)
			nonshared++;
		else
			shared += page_count(mem_map+i) - 1;
	}
	printk("%d pages of RAM\n",total);
	printk("%d free pages\n",free);
	printk("%d reserved pages\n",reserved);
	printk("%d pages nonshared\n",nonshared);
	printk("%d pages shared\n",shared);
	printk("%d pages swap cached\n",cached);
	printk("%ld pages in page table cache\n",pgtable_cache_size);
	show_buffers();
}

/*
 * The kernel is already mapped with a kernel segment at kseg_c so 
 * we don't need to map it with a page table. However head.S also
 * temporarily mapped it at kseg_4 so we should set up the ksegs again,
 * clear the TLB and do some other paging setup stuff.
 */

void __init 
paging_init(void)
{
	int i;
	unsigned long zones_size[MAX_NR_ZONES];

	printk(KERN_INFO "Setting up paging and the MMU.\n");
	
	/* clear out the init_mm.pgd that will contain the kernel's mappings */

	for(i = 0; i < PTRS_PER_PGD; i++)
		swapper_pg_dir[i] = __pgd(0);
	
	/* make sure the current pgd table points to something sane
	 * (even if it is most probably not used until the next 
	 *  switch_mm)
	 */

	current_pgd = init_mm.pgd;

	/* initialise the TLB (tlb.c) */

	tlb_init();

	/* see README.mm for details on the KSEG setup */

#ifndef CONFIG_CRIS_LOW_MAP
	/* This code is for the corrected Etrax-100 LX version 2... */

#define CACHED_BOOTROM (KSEG_A | 0x08000000UL)

	*R_MMU_KSEG = ( IO_STATE(R_MMU_KSEG, seg_f, seg  ) | /* cached flash */
			IO_STATE(R_MMU_KSEG, seg_e, seg  ) | /* uncached flash */
			IO_STATE(R_MMU_KSEG, seg_d, page ) | /* vmalloc area */
			IO_STATE(R_MMU_KSEG, seg_c, seg  ) | /* kernel area */
			IO_STATE(R_MMU_KSEG, seg_b, seg  ) | /* kernel reg area */
			IO_STATE(R_MMU_KSEG, seg_a, seg  ) | /* bootrom/regs cached */ 
			IO_STATE(R_MMU_KSEG, seg_9, page ) | /* user area */
			IO_STATE(R_MMU_KSEG, seg_8, page ) |
			IO_STATE(R_MMU_KSEG, seg_7, page ) |
			IO_STATE(R_MMU_KSEG, seg_6, page ) |
			IO_STATE(R_MMU_KSEG, seg_5, page ) |
			IO_STATE(R_MMU_KSEG, seg_4, page ) |
			IO_STATE(R_MMU_KSEG, seg_3, page ) |
			IO_STATE(R_MMU_KSEG, seg_2, page ) |
			IO_STATE(R_MMU_KSEG, seg_1, page ) |
			IO_STATE(R_MMU_KSEG, seg_0, page ) );

	*R_MMU_KBASE_HI = ( IO_FIELD(R_MMU_KBASE_HI, base_f, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_e, 0x8 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_d, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_c, 0x4 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_b, 0xb ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0x3 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_9, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_8, 0x0 ) );
	
	*R_MMU_KBASE_LO = ( IO_FIELD(R_MMU_KBASE_LO, base_7, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_6, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_5, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_4, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_3, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_2, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_1, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_0, 0x0 ) );
#else
	/* Etrax-100 LX version 1 has a bug so that we cannot map anything
	 * across the 0x80000000 boundary, so we need to shrink the user-virtual
	 * area to 0x50000000 instead of 0xb0000000 and map things slightly
	 * different. The unused areas are marked as paged so that we can catch
	 * freak kernel accesses there.
	 *
	 * The ARTPEC chip is mapped at 0xa so we pass that segment straight
	 * through. We cannot vremap it because the vmalloc area is below 0x8
	 * and Juliette needs an uncached area above 0x8.
	 *
	 * Same thing with 0xc and 0x9, which is memory-mapped I/O on some boards.
	 * We map them straight over in LOW_MAP, but use vremap in LX version 2.
	 */

#define CACHED_BOOTROM (KSEG_F | 0x08000000UL)

	*R_MMU_KSEG = ( IO_STATE(R_MMU_KSEG, seg_f, seg  ) |  /* cached bootrom/regs */ 
			IO_STATE(R_MMU_KSEG, seg_e, page ) |
			IO_STATE(R_MMU_KSEG, seg_d, page ) | 
			IO_STATE(R_MMU_KSEG, seg_c, page ) |   
			IO_STATE(R_MMU_KSEG, seg_b, seg  ) |  /* kernel reg area */
#ifdef CONFIG_JULIETTE
			IO_STATE(R_MMU_KSEG, seg_a, seg  ) |  /* ARTPEC etc. */
#else
			IO_STATE(R_MMU_KSEG, seg_a, page ) |
#endif
			IO_STATE(R_MMU_KSEG, seg_9, seg  ) |  /* LED's on some boards */
			IO_STATE(R_MMU_KSEG, seg_8, seg  ) |  /* CSE0/1, flash and I/O */
			IO_STATE(R_MMU_KSEG, seg_7, page ) |  /* kernel vmalloc area */
			IO_STATE(R_MMU_KSEG, seg_6, seg  ) |  /* kernel DRAM area */
			IO_STATE(R_MMU_KSEG, seg_5, seg  ) |  /* cached flash */
			IO_STATE(R_MMU_KSEG, seg_4, page ) |  /* user area */
			IO_STATE(R_MMU_KSEG, seg_3, page ) |  /* user area */
			IO_STATE(R_MMU_KSEG, seg_2, page ) |  /* user area */
			IO_STATE(R_MMU_KSEG, seg_1, page ) |  /* user area */
			IO_STATE(R_MMU_KSEG, seg_0, page ) ); /* user area */

	*R_MMU_KBASE_HI = ( IO_FIELD(R_MMU_KBASE_HI, base_f, 0x3 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_e, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_d, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_c, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_b, 0xb ) |
#ifdef CONFIG_JULIETTE
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0xa ) |
#else
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0x0 ) |
#endif
			    IO_FIELD(R_MMU_KBASE_HI, base_9, 0x9 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_8, 0x8 ) );
	
	*R_MMU_KBASE_LO = ( IO_FIELD(R_MMU_KBASE_LO, base_7, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_6, 0x4 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_5, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_4, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_3, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_2, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_1, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_0, 0x0 ) );
#endif

	*R_MMU_CONTEXT = ( IO_FIELD(R_MMU_CONTEXT, page_id, 0 ) );
	
	/* The MMU has been enabled ever since head.S but just to make
	 * it totally obvious we do it here as well.
	 */

	*R_MMU_CTRL = ( IO_STATE(R_MMU_CTRL, inv_excp, enable ) |
			IO_STATE(R_MMU_CTRL, acc_excp, enable ) |
			IO_STATE(R_MMU_CTRL, we_excp,  enable ) );
	
	*R_MMU_ENABLE = IO_STATE(R_MMU_ENABLE, mmu_enable, enable);

	/*
	 * initialize the bad page table and bad page to point
	 * to a couple of allocated pages
	 */

	empty_zero_page = (unsigned long)alloc_bootmem_pages(PAGE_SIZE);
	memset((void *)empty_zero_page, 0, PAGE_SIZE);

	/* All pages are DMA'able in Etrax, so put all in the DMA'able zone */

	zones_size[0] = ((unsigned long)high_memory - PAGE_OFFSET) >> PAGE_SHIFT;

	for (i = 1; i < MAX_NR_ZONES; i++)
		zones_size[i] = 0;

	/* Use free_area_init_node instead of free_area_init, because the former
	 * is designed for systems where the DRAM starts at an address substantially
	 * higher than 0, like us (we start at PAGE_OFFSET). This saves space in the
	 * mem_map page array.
	 */

	free_area_init_node(0, 0, 0, zones_size, PAGE_OFFSET, 0);

}

extern unsigned long loops_per_jiffy; /* init/main.c */
unsigned long loops_per_usec;

extern char _stext, _edata, _etext;
extern char __init_begin, __init_end;

void __init
mem_init(void)
{
	int codesize, reservedpages, datasize, initsize;
	unsigned long tmp;

	if(!mem_map)
		BUG();

	/* max/min_low_pfn was set by setup.c
	 * now we just copy it to some other necessary places...
	 *
	 * high_memory was also set in setup.c
	 */

	max_mapnr = num_physpages = max_low_pfn - min_low_pfn;
 
	/* this will put all memory onto the freelists */
        totalram_pages = free_all_bootmem();

	reservedpages = 0;
	for (tmp = 0; tmp < max_mapnr; tmp++) {
		/*
                 * Only count reserved RAM pages
                 */
		if (PageReserved(mem_map + tmp))
			reservedpages++;
	}

	codesize =  (unsigned long) &_etext - (unsigned long) &_stext;
        datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
        initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;
	
        printk(KERN_INFO
	       "Memory: %luk/%luk available (%dk kernel code, %dk reserved, %dk data, "
	       "%dk init)\n" ,
	       (unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
	       max_mapnr << (PAGE_SHIFT-10),
	       codesize >> 10,
	       reservedpages << (PAGE_SHIFT-10),
	       datasize >> 10,
	       initsize >> 10
               );

	/* HACK alert - calculate a loops_per_usec for asm/delay.h here
	 * since this is called just after calibrate_delay in init/main.c
	 * but before places which use udelay. cannot be in time.c since
	 * that is called _before_ calibrate_delay
	 */

	loops_per_usec = (loops_per_jiffy * HZ) / 1000000;

	return;
}

/* Initialize remaps of some I/O-ports. It is important that this
 * is called before any driver is initialized.
 */

static int 
__init init_ioremap(void)
{
  
	/* Give the external I/O-port addresses their values */

#ifdef CONFIG_CRIS_LOW_MAP
	/* Simply a linear map (see the KSEG map above in paging_init) */
	port_cse1_addr = (volatile unsigned long *)(MEM_CSE1_START |
	                                            MEM_NON_CACHEABLE);
	port_csp0_addr = (volatile unsigned long *)(MEM_CSP0_START |
	                                            MEM_NON_CACHEABLE);
	port_csp4_addr = (volatile unsigned long *)(MEM_CSP4_START |
	                                            MEM_NON_CACHEABLE);
#else						    
	/* Note that nothing blows up just because we do this remapping 
	 * it's ok even if the ports are not used or connected 
	 * to anything (or connected to a non-I/O thing) */        
	port_cse1_addr = (volatile unsigned long *)
	ioremap((unsigned long)(MEM_CSE1_START | MEM_NON_CACHEABLE), 16);
	port_csp0_addr = (volatile unsigned long *)
	ioremap((unsigned long)(MEM_CSP0_START | MEM_NON_CACHEABLE), 16);
	port_csp4_addr = (volatile unsigned long *)
	ioremap((unsigned long)(MEM_CSP4_START | MEM_NON_CACHEABLE), 16);
#endif
	return 0;
}

__initcall(init_ioremap);

/* Helper function for the two below */

static inline void
flush_etrax_cacherange(void *startadr, int length)
{
	/* CACHED_BOOTROM is mapped to the boot-rom area (cached) which
	 * we can use to get fast dummy-reads of cachelines
	 */

	volatile short *flushadr = (volatile short *)(((unsigned long)startadr & ~PAGE_MASK) |
						      CACHED_BOOTROM);

	length = length > 8192 ? 8192 : length;  /* No need to flush more than cache size */

	while(length > 0) {
		*flushadr; /* dummy read to flush */
		flushadr += (32/sizeof(short));  /* a cacheline is 32 bytes */
		length -= 32;
	}
}

/* Due to a bug in Etrax100(LX) all versions, receiving DMA buffers
 * will occationally corrupt certain CPU writes if the DMA buffers
 * happen to be hot in the cache.
 * 
 * As a workaround, we have to flush the relevant parts of the cache
 * before (re) inserting any receiving descriptor into the DMA HW.
 */

void
prepare_rx_descriptor(struct etrax_dma_descr *desc)
{
	flush_etrax_cacherange((void *)desc->buf, desc->sw_len ? desc->sw_len : 65536);
}

/* Do the same thing but flush the entire cache */

void
flush_etrax_cache(void)
{
	flush_etrax_cacherange(0, 8192);
}

/* free the pages occupied by initialization code */

void 
free_initmem(void)
{
        unsigned long addr;

        addr = (unsigned long)(&__init_begin);
        for (; addr < (unsigned long)(&__init_end); addr += PAGE_SIZE) {
                ClearPageReserved(virt_to_page(addr));
                set_page_count(virt_to_page(addr), 1);
                free_page(addr);
                totalram_pages++;
        }
        printk (KERN_INFO "Freeing unused kernel memory: %luk freed\n", 
		(unsigned long)((&__init_end - &__init_begin) >> 10));
}

void 
si_meminfo(struct sysinfo *val)
{
        val->totalram = totalram_pages;
        val->sharedram = 0;
        val->freeram = nr_free_pages();
        val->bufferram = atomic_read(&buffermem_pages);
        val->totalhigh = 0;
        val->freehigh = 0;
        val->mem_unit = PAGE_SIZE;
}
