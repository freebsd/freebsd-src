/*
 *  linux/arch/x86_64/mm/init.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000  Pavel Machek <pavel@suse.cz>
 *  Copyright (C) 2002  Andi Kleen <ak@suse.de>
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
#include <linux/blk.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/fixmap.h>
#include <asm/e820.h>
#include <asm/apic.h>
#include <asm/tlb.h>
#include <asm/pda.h>
#include <asm/mmu_context.h>
#include <asm/proto.h>

mmu_gather_t mmu_gathers[NR_CPUS];

static unsigned long totalram_pages;

int do_check_pgt_cache(int low, int high)
{
	int freed = 0;
	if(read_pda(pgtable_cache_sz) > high) {
		do {
			if (read_pda(pgd_quick)) {
				pgd_free_slow(pgd_alloc_one_fast());
				freed++;
			}
			if (read_pda(pmd_quick)) {
				pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
				freed++;
			}
			if (read_pda(pte_quick)) {
				pte_free_slow(pte_alloc_one_fast(NULL, 0));
				freed++;
			}
		} while(read_pda(pgtable_cache_sz) > low);
	}
	return freed;
}

#ifndef CONFIG_DISCONTIGMEM
/*
 * NOTE: pagetable_init alloc all the fixmap pagetables contiguous on the
 * physical space so we can cache the place of the first one and move
 * around without checking the pgd every time.
 */

void show_mem(void)
{
	int i, total = 0, reserved = 0;
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
			shared += page_count(mem_map+i) - 1;
	}
	printk("%d pages of RAM\n", total);
	printk("%d reserved pages\n",reserved);
	printk("%d pages shared\n",shared);
	printk("%d pages swap cached\n",cached);
	printk("%ld pages in page table cache\n",read_pda(pgtable_cache_sz));
	show_buffers();
}
#endif

/* References to section boundaries */

extern char _text, _etext, _edata, __bss_start, _end;
extern char __init_begin, __init_end;

int after_bootmem;

static void *spp_getpage(void)
{ 
	void *ptr;
	if (after_bootmem)
		ptr = (void *) get_free_page(GFP_ATOMIC); 
	else
		ptr = alloc_bootmem_low_pages(PAGE_SIZE); 
	if (!ptr)
		panic("set_pte_phys: cannot allocate page data %s\n", after_bootmem?"after bootmem":"");
	return ptr;
} 

static void set_pte_phys(unsigned long vaddr,
			 unsigned long phys, pgprot_t prot)
{
	pml4_t *level4;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	level4 = pml4_offset_k(vaddr);
	if (pml4_none(*level4)) {
		printk("PML4 FIXMAP MISSING, it should be setup in head.S!\n");
		return;
	}
	pgd = level3_offset_k(level4, vaddr);
	if (pgd_none(*pgd)) {
		pmd = (pmd_t *) spp_getpage(); 
		set_pgd(pgd, __pgd(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));
		if (pmd != pmd_offset(pgd, 0)) {
			printk("PAGETABLE BUG #01!\n");
			return;
		}
	}
	pmd = pmd_offset(pgd, vaddr);
	if (pmd_none(*pmd)) {
		pte = (pte_t *) spp_getpage();
		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		if (pte != pte_offset(pmd, 0)) {
			printk("PAGETABLE BUG #02!\n");
			return;
		}
	}
	pte = pte_offset(pmd, vaddr);
	set_pte(pte, mk_pte_phys(phys, prot));

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

void __set_fixmap (enum fixed_addresses idx, unsigned long phys, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		printk("Invalid __set_fixmap\n");
		return;
	}
	set_pte_phys(address, phys, prot);
}

extern pmd_t temp_boot_pmds[]; 

unsigned long __initdata table_start, table_end; 

static  struct temp_map { 
	pmd_t *pmd; 
	void  *address; 
	int    allocated; 
} temp_mappings[] __initdata = { 
	{ &temp_boot_pmds[0], (void *)(40UL * 1024 * 1024) },
	{ &temp_boot_pmds[1], (void *)(42UL * 1024 * 1024) }, 
	{}
}; 

static __init void *alloc_low_page(int *index, unsigned long *phys) 
{ 
	struct temp_map *ti;
	int i; 
	unsigned long pfn = table_end++, paddr; 
	void *adr;

	if (table_end >= end_pfn_map) 
		panic("alloc_low_page: ran out of page mappings"); 
	for (i = 0; temp_mappings[i].allocated; i++) {
		if (!temp_mappings[i].pmd) 
			panic("alloc_low_page: ran out of temp mappings"); 
	} 
	ti = &temp_mappings[i];
	paddr = (pfn << PAGE_SHIFT) & PMD_MASK; 
	set_pmd(ti->pmd, __pmd(paddr | _KERNPG_TABLE | _PAGE_PSE)); 
	ti->allocated = 1; 
	__flush_tlb(); 	       
	adr = ti->address + ((pfn << PAGE_SHIFT) & ~PMD_MASK); 
	*index = i; 
	*phys  = pfn * PAGE_SIZE;  
	return adr; 
} 

static __init void unmap_low_page(int i)
{ 
	struct temp_map *ti = &temp_mappings[i];
	set_pmd(ti->pmd, __pmd(0));
	ti->allocated = 0; 
} 

static void __init phys_pgd_init(pgd_t *pgd, unsigned long address, unsigned long end)
{ 
	long i, j; 

	i = pgd_index(address);
	pgd = pgd + i;
	for (; i < PTRS_PER_PGD; pgd++, i++) {
		int map; 
		unsigned long paddr, pmd_phys;
		pmd_t *pmd;

		paddr = (address & PML4_MASK) + i*PGDIR_SIZE; 
		if (paddr >= end) { 
			for (; i < PTRS_PER_PGD; i++, pgd++) 
				set_pgd(pgd, __pgd(0)); 
			break;
		} 

		if (!e820_mapped(paddr, paddr+PGDIR_SIZE, 0)) { 
			set_pgd(pgd, __pgd(0)); 
			continue;
		} 

		pmd = alloc_low_page(&map, &pmd_phys);
		set_pgd(pgd, __pgd(pmd_phys | _KERNPG_TABLE));
		for (j = 0; j < PTRS_PER_PMD; pmd++, j++ , paddr += PMD_SIZE) {
			unsigned long pe;

			if (paddr >= end) { 
				for (; j < PTRS_PER_PMD; j++, pmd++)
					set_pmd(pmd,  __pmd(0)); 
				break;
			}
			pe = _PAGE_PSE | _KERNPG_TABLE | _PAGE_NX | _PAGE_GLOBAL | paddr;
			pe &= __supported_pte_mask; 
			set_pmd(pmd, __pmd(pe));
		}
		unmap_low_page(map);
	}
	__flush_tlb();
} 

/* Setup the direct mapping of the physical memory at PAGE_OFFSET.
   This runs before bootmem is initialized and gets pages directly from the 
   physical memory. To access them they are temporarily mapped. */
void __init init_memory_mapping(void) 
{ 
	unsigned long adr;	       
	unsigned long end;
	unsigned long next; 
	unsigned long pgds, pmds, tables; 

	end = end_pfn_map << PAGE_SHIFT; 

	/* 
	 * Find space for the kernel direct mapping tables.
	 * Later we should allocate these tables in the local node of the memory
	 * mapped.  Unfortunately this is done currently before the nodes are 
	 * discovered.
	 */

	pgds = (end + PGDIR_SIZE - 1) >> PGDIR_SHIFT;
	pmds = (end + PMD_SIZE - 1) >> PMD_SHIFT; 
	tables = round_up(pgds*8, PAGE_SIZE) + round_up(pmds * 8, PAGE_SIZE); 

	/* Direct mapping must currently fit below the kernel in the first MB.
	   This is because we have no way to tell the later passes to not reuse
	   the memory, until bootmem is initialised */
	/* Should limit MAXMEM for this */
	table_start = find_e820_area(/*0*/ 0x8000, __pa_symbol(&_text), tables); 
	if (table_start == -1UL) 
		panic("Cannot find space for the kernel page tables"); 

	table_start >>= PAGE_SHIFT; 
	table_end = table_start;
       
	end += __PAGE_OFFSET; /* turn virtual */  

	for (adr = PAGE_OFFSET; adr < end; adr = next) { 
		int map;
		unsigned long pgd_phys; 
		pgd_t *pgd = alloc_low_page(&map, &pgd_phys);
		next = adr + PML4_SIZE;
		if (next > end) 
			next = end; 

		phys_pgd_init(pgd, adr-PAGE_OFFSET, next-PAGE_OFFSET); 
		set_pml4(init_level4_pgt + pml4_index(adr), 
			 mk_kernel_pml4(pgd_phys, KERNPG_TABLE));
		unmap_low_page(map);   
	} 
	asm volatile("movq %%cr4,%0" : "=r" (mmu_cr4_features));
	__flush_tlb_all();
	printk("kernel direct mapping tables upto %lx @ %lx-%lx\n", end, 
	       table_start<<PAGE_SHIFT, 
	       table_end<<PAGE_SHIFT);
} 

void __init zap_low_mappings (void)
{
	int i;
	for (i = 0; i < NR_CPUS; i++) {
		if (cpu_pda[i].level4_pgt) 
			cpu_pda[i].level4_pgt[0] = 0; 
	}

	flush_tlb_all();
}

#ifndef CONFIG_DISCONTIGMEM
void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = {0, 0, 0};
	unsigned int max_dma;
	
	max_dma = virt_to_phys((char *)MAX_DMA_ADDRESS) >> PAGE_SHIFT;
	if (end_pfn < max_dma)
		zones_size[ZONE_DMA] = end_pfn;
	else {
		zones_size[ZONE_DMA] = max_dma;
		zones_size[ZONE_NORMAL] = end_pfn - max_dma;
	}
	free_area_init(zones_size);
}

static inline int page_is_ram (unsigned long pagenr)
{
	int i;

	for (i = 0; i < e820.nr_map; i++) {
		unsigned long addr, end;

		if (e820.map[i].type != E820_RAM)	/* not usable memory */
			continue;
		/*
		 *	!!!FIXME!!! Some BIOSen report areas as RAM that
		 *	are not. Notably the 640->1Mb area. We need a sanity
		 *	check here.
		 */
		addr = (e820.map[i].addr+PAGE_SIZE-1) >> PAGE_SHIFT;
		end = (e820.map[i].addr+e820.map[i].size) >> PAGE_SHIFT;
		if  ((pagenr >= addr) && (pagenr < end))
			return 1;
	}
	return 0;
}
#endif

void __init mem_init(void)
{
	unsigned long codesize, reservedpages, datasize, initsize;
	unsigned long tmp;

	max_mapnr = end_pfn; 
	num_physpages = end_pfn; /* XXX not true because of holes */
	high_memory = (void *) __va(end_pfn << PAGE_SHIFT);

	/* clear the zero-page */
	memset(empty_zero_page, 0, PAGE_SIZE);

	reservedpages = 0;

	/* this will put all low memory onto the freelists */
#ifdef CONFIG_DISCONTIGMEM
	totalram_pages += numa_free_all_bootmem();
	tmp = 0;
	/* should count reserved pages here for all nodes */ 
#else
	if (!mem_map) BUG();

	totalram_pages += free_all_bootmem();

	for (tmp = 0; tmp < end_pfn; tmp++)
		/*
		 * Only count reserved RAM pages
		 */
		if (page_is_ram(tmp) && PageReserved(mem_map+tmp))
			reservedpages++;
#endif

	after_bootmem = 1;

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk("Memory: %luk/%luk available (%ldk kernel code, %ldk reserved, %ldk data, %ldk init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		max_mapnr << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		initsize >> 10);

	/*
	 * Subtle. SMP is doing its boot stuff late (because it has to
	 * fork idle threads) - but it also needs low mappings for the
	 * protected-mode entry to work. We zap these entries only after
	 * the WP-bit has been tested.
	 */
#ifndef CONFIG_SMP
	zap_low_mappings();
#endif
}

/* Unmap a kernel mapping if it exists. This is useful to avoid prefetches
   from the CPU leading to inconsistent cache lines. address and size
   must be aligned to 2MB boundaries. 
   Does nothing when the mapping doesn't exist. */
void __init clear_kernel_mapping(unsigned long address, unsigned long size) 
{ 
	unsigned long end = address + size;

	BUG_ON(address & ~LARGE_PAGE_MASK);
	BUG_ON(size & ~LARGE_PAGE_MASK); 
	
	for (; address < end; address += LARGE_PAGE_SIZE) { 
		pgd_t *pgd = pgd_offset_k(address);
		if (!pgd || pgd_none(*pgd))
			continue; 
		pmd_t *pmd = pmd_offset(pgd, address);
		if (!pmd || pmd_none(*pmd))
			continue; 
		if (0 == (pmd_val(*pmd) & _PAGE_PSE)) { 
			/* Could handle this, but it should not happen currently. */
			printk(KERN_ERR 
		"clear_kernel_mapping: mapping has been split. will leak memory\n"); 
			pmd_ERROR(*pmd); 
		} 
		set_pmd(pmd, __pmd(0)); 		
	} 
	__flush_tlb_all(); 
} 

void free_initmem(void)
{
	void *addr;

	addr = (&__init_begin);
	for (; addr < (void *)(&__init_end); addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		set_page_count(virt_to_page(addr), 1);
#ifdef CONFIG_INIT_DEBUG
		memset((unsigned long)addr & ~(PAGE_SIZE-1), 0xcc, PAGE_SIZE); 
#endif
		free_page((unsigned long)addr);
		totalram_pages++;
	}
	printk ("Freeing unused kernel memory: %luk freed\n", (&__init_end - &__init_begin) >> 10);
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (start < (unsigned long)&_end)
		return;
	printk ("Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
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
	val->freehigh = nr_free_highpages();
	val->mem_unit = PAGE_SIZE;
	return;
}

void __init reserve_bootmem_generic(unsigned long phys, unsigned len) 
{ 
	/* Should check here against the e820 map to avoid double free */ 
#ifdef CONFIG_DISCONTIGMEM
	reserve_bootmem_node(NODE_DATA(phys_to_nid(phys)), phys, len);
#else       		
	reserve_bootmem(phys, len);    
#endif
}


void free_bootmem_generic(unsigned long phys, unsigned len) 
{ 
#ifdef CONFIG_DISCONTIGMEM
	free_bootmem_node(NODE_DATA(phys_to_nid(phys)), phys, len);
#else       		
	free_bootmem(phys, len);    
#endif
}
