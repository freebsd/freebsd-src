/*
 * Initialize MMU support.
 *
 * Copyright (C) 1998-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/personality.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/efi.h>
#include <linux/mmzone.h>

#include <asm/bitops.h>
#include <asm/dma.h>
#include <asm/ia32.h>
#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/numa.h>
#include <asm/pgalloc.h>
#include <asm/sal.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/mca.h>

/* References to section boundaries: */
extern char _stext, _etext, _edata, __init_begin, __init_end;

extern void ia64_tlb_init (void);
extern int  filter_rsvd_memory (unsigned long, unsigned long, void *);

/* Note - may be changed by platform_setup */
unsigned long MAX_DMA_ADDRESS = PAGE_OFFSET + 0x100000000UL;
#define LARGE_GAP 0x40000000 /* Use virtual mem map if a hole is > than this */

static unsigned long totalram_pages, reserved_pages;
struct page *zero_page_memmap_ptr;		/* map entry for zero page */

unsigned long vmalloc_end = VMALLOC_END_INIT;

static struct page *vmem_map;
static unsigned long num_dma_physpages;

int
do_check_pgt_cache (int low, int high)
{
	int freed = 0;

	if (pgtable_cache_size > high) {
		do {
			if (pgd_quicklist)
				free_page((unsigned long)pgd_alloc_one_fast(0)), ++freed;
			if (pmd_quicklist)
				free_page((unsigned long)pmd_alloc_one_fast(0, 0)), ++freed;
			if (pte_quicklist)
				free_page((unsigned long)pte_alloc_one_fast(0, 0)), ++freed;
		} while (pgtable_cache_size > low);
	}
	return freed;
}

inline void
ia64_set_rbs_bot (void)
{
	unsigned long stack_size = current->rlim[RLIMIT_STACK].rlim_max & -16;

	if (stack_size > MAX_USER_STACK_SIZE)
		stack_size = MAX_USER_STACK_SIZE;
	current->thread.rbs_bot = STACK_TOP - stack_size;
}

/*
 * This performs some platform-dependent address space initialization.
 * On IA-64, we want to setup the VM area for the register backing
 * store (which grows upwards) and install the gateway page which is
 * used for signal trampolines, etc.
 */
void
ia64_init_addr_space (void)
{
	struct vm_area_struct *vma;

	ia64_set_rbs_bot();

	/*
	 * If we're out of memory and kmem_cache_alloc() returns NULL, we simply ignore
	 * the problem.  When the process attempts to write to the register backing store
	 * for the first time, it will get a SEGFAULT in this case.
	 */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (vma) {
		vma->vm_mm = current->mm;
		vma->vm_start = current->thread.rbs_bot & PAGE_MASK;
		vma->vm_end = vma->vm_start + PAGE_SIZE;
		vma->vm_page_prot = PAGE_COPY;
		vma->vm_flags = VM_READ|VM_WRITE|VM_MAYREAD|VM_MAYWRITE|VM_GROWSUP;
		vma->vm_ops = NULL;
		vma->vm_pgoff = 0;
		vma->vm_file = NULL;
		vma->vm_private_data = NULL;
		insert_vm_struct(current->mm, vma);
	}

	/* map NaT-page at address zero to speed up speculative dereferencing of NULL: */
	if (!(current->personality & MMAP_PAGE_ZERO)) {
		vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
		if (vma) {
			memset(vma, 0, sizeof(*vma));
			vma->vm_mm = current->mm;
			vma->vm_end = PAGE_SIZE;
			vma->vm_page_prot = __pgprot(pgprot_val(PAGE_READONLY) | _PAGE_MA_NAT);
			vma->vm_flags = VM_READ | VM_MAYREAD | VM_IO | VM_RESERVED;
			insert_vm_struct(current->mm, vma);
		}
	}
}

void
free_initmem (void)
{
	unsigned long addr, eaddr;

	addr = (unsigned long) ia64_imva(&__init_begin);
	eaddr = (unsigned long) ia64_imva(&__init_end);
	for (; addr < eaddr; addr += PAGE_SIZE) {
		clear_bit(PG_reserved, &virt_to_page((void *)addr)->flags);
		set_page_count(virt_to_page((void *)addr), 1);
		free_page(addr);
		++totalram_pages;
	}
	printk(KERN_INFO "Freeing unused kernel memory: %ldkB freed\n",
		(&__init_end - &__init_begin) >> 10);
}

void
free_initrd_mem(unsigned long start, unsigned long end)
{
	/*
	 * EFI uses 4KB pages while the kernel can use 4KB  or bigger.
	 * Thus EFI and the kernel may have different page sizes. It is
	 * therefore possible to have the initrd share the same page as
	 * the end of the kernel (given current setup).
	 *
	 * To avoid freeing/using the wrong page (kernel sized) we:
	 *	- align up the beginning of initrd
	 *	- align down the end of initrd
	 *
	 *  |             |
	 *  |=============| a000
	 *  |             |
	 *  |             |
	 *  |             | 9000
	 *  |/////////////|
	 *  |/////////////|
	 *  |=============| 8000
	 *  |///INITRD////|
	 *  |/////////////|
	 *  |/////////////| 7000
	 *  |             |
	 *  |KKKKKKKKKKKKK|
	 *  |=============| 6000
	 *  |KKKKKKKKKKKKK|
	 *  |KKKKKKKKKKKKK|
	 *  K=kernel using 8KB pages
	 *
	 * In this example, we must free page 8000 ONLY. So we must align up
	 * initrd_start and keep initrd_end as is.
	 */
	start = PAGE_ALIGN(start);
	end = end & PAGE_MASK;

	if (start < end)
		printk(KERN_INFO "Freeing initrd memory: %ldkB freed\n", (end - start) >> 10);

	for (; start < end; start += PAGE_SIZE) {
		if (!VALID_PAGE(virt_to_page((void *)start)))
			continue;
		clear_bit(PG_reserved, &virt_to_page((void *)start)->flags);
		set_page_count(virt_to_page((void *)start), 1);
		free_page(start);
		++totalram_pages;
	}
}

void
si_meminfo (struct sysinfo *val)
{
	val->totalram = totalram_pages;
	val->sharedram = 0;
	val->freeram = nr_free_pages();
	val->bufferram = atomic_read(&buffermem_pages);
	val->totalhigh = 0;
	val->freehigh = 0;
	val->mem_unit = PAGE_SIZE;
	return;
}

void
show_mem(void)
{
	int i, reserved;
	int shared, cached;
	pg_data_t *pgdat;
	char *tchar = (numnodes > 1) ? "\t" : "";

	printk("Mem-info:\n");
	show_free_areas();

	printk("Free swap:       %6dkB\n", nr_swap_pages<<(PAGE_SHIFT-10));
	for_each_pgdat(pgdat) {
		reserved=0;
		cached=0;
		shared=0;
		if (numnodes > 1)
			printk("Node ID: %d\n", pgdat->node_id);
		for(i = 0; i < pgdat->node_size; i++) {
			if (!VALID_PAGE(pgdat->node_mem_map+i))
				continue;
			if (PageReserved(pgdat->node_mem_map+i))
				reserved++;
			else if (PageSwapCache(pgdat->node_mem_map+i))
				cached++;
			else if (page_count(pgdat->node_mem_map + i))
				shared += page_count(pgdat->node_mem_map + i) - 1;
		}
		printk("%s%ld pages of RAM\n", tchar, pgdat->node_size);
		printk("%s%d reserved pages\n", tchar, reserved);
		printk("%s%d pages shared\n", tchar, shared);
		printk("%s%d pages swap cached\n", tchar, cached);
	}
	printk("Total of %ld pages in page table cache\n", pgtable_cache_size);
	show_buffers();
	printk("%d free buffer pages\n", nr_free_buffer_pages());
}

/*
 * This is like put_dirty_page() but installs a clean page with PAGE_GATE protection
 * (execute-only, typically).
 */
struct page *
put_gate_page (struct page *page, unsigned long address)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	if (!PageReserved(page))
		printk(KERN_ERR "put_gate_page: gate page at 0x%p not in reserved memory\n",
		       page_address(page));

	pgd = pgd_offset_k(address);		/* note: this is NOT pgd_offset()! */

	spin_lock(&init_mm.page_table_lock);
	{
		pmd = pmd_alloc(&init_mm, pgd, address);
		if (!pmd)
			goto out;
		pte = pte_alloc(&init_mm, pmd, address);
		if (!pte)
			goto out;
		if (!pte_none(*pte)) {
			pte_ERROR(*pte);
			goto out;
		}
		flush_page_to_ram(page);
		set_pte(pte, mk_pte(page, PAGE_GATE));
	}
  out:	spin_unlock(&init_mm.page_table_lock);
	/* no need for flush_tlb */
	return page;
}

void __init
ia64_mmu_init (void *my_cpu_data)
{
	unsigned long psr, rid, pta, impl_va_bits;
	extern void __init tlb_init (void);
#ifdef CONFIG_IA64_MCA
	int cpu;
#endif

#ifdef CONFIG_DISABLE_VHPT
#	define VHPT_ENABLE_BIT	0
#else
#	define VHPT_ENABLE_BIT	1
#endif

	/*
	 * Set up the kernel identity mapping for regions 6 and 5.  The mapping for region
	 * 7 is setup up in _start().
	 */
	psr = ia64_clear_ic();

	rid = ia64_rid(IA64_REGION_ID_KERNEL, __IA64_UNCACHED_OFFSET);
	ia64_set_rr(__IA64_UNCACHED_OFFSET, (rid << 8) | (IA64_GRANULE_SHIFT << 2));

	rid = ia64_rid(IA64_REGION_ID_KERNEL, VMALLOC_START);
	ia64_set_rr(VMALLOC_START, (rid << 8) | (PAGE_SHIFT << 2) | 1);

	/* ensure rr6 is up-to-date before inserting the PERCPU_ADDR translation: */
	ia64_srlz_d();

	ia64_itr(0x2, IA64_TR_PERCPU_DATA, PERCPU_ADDR,
		 pte_val(mk_pte_phys(__pa(my_cpu_data), PAGE_KERNEL)), PAGE_SHIFT);

	ia64_set_psr(psr);
	ia64_srlz_i();

	/*
	 * Check if the virtually mapped linear page table (VMLPT) overlaps with a mapped
	 * address space.  The IA-64 architecture guarantees that at least 50 bits of
	 * virtual address space are implemented but if we pick a large enough page size
	 * (e.g., 64KB), the mapped address space is big enough that it will overlap with
	 * VMLPT.  I assume that once we run on machines big enough to warrant 64KB pages,
	 * IMPL_VA_MSB will be significantly bigger, so this is unlikely to become a
	 * problem in practice.  Alternatively, we could truncate the top of the mapped
	 * address space to not permit mappings that would overlap with the VMLPT.
	 * --davidm 00/12/06
	 */
#	define pte_bits			3
#	define mapped_space_bits	(3*(PAGE_SHIFT - pte_bits) + PAGE_SHIFT)
	/*
	 * The virtual page table has to cover the entire implemented address space within
	 * a region even though not all of this space may be mappable.  The reason for
	 * this is that the Access bit and Dirty bit fault handlers perform
	 * non-speculative accesses to the virtual page table, so the address range of the
	 * virtual page table itself needs to be covered by virtual page table.
	 */
#	define vmlpt_bits		(impl_va_bits - PAGE_SHIFT + pte_bits)
#	define POW2(n)			(1ULL << (n))

	impl_va_bits = ffz(~(local_cpu_data->unimpl_va_mask | (7UL << 61)));

	if (impl_va_bits < 51 || impl_va_bits > 61)
		panic("CPU has bogus IMPL_VA_MSB value of %lu!\n", impl_va_bits - 1);

	/* place the VMLPT at the end of each page-table mapped region: */
	pta = POW2(61) - POW2(vmlpt_bits);

	if (POW2(mapped_space_bits) >= pta)
		panic("mm/init: overlap between virtually mapped linear page table and "
		      "mapped kernel space!");
	/*
	 * Set the (virtually mapped linear) page table address.  Bit
	 * 8 selects between the short and long format, bits 2-7 the
	 * size of the table, and bit 0 whether the VHPT walker is
	 * enabled.
	 */
	ia64_set_pta(pta | (0 << 8) | (vmlpt_bits << 2) | VHPT_ENABLE_BIT);

	ia64_tlb_init();

#ifdef	CONFIG_IA64_MCA
	cpu = smp_processor_id();

	/* mca handler uses cr.lid as key to pick the right entry */
	ia64_mca_tlb_list[cpu].cr_lid = ia64_get_lid();

	/* insert this percpu data information into our list for MCA recovery purposes */
	ia64_mca_tlb_list[cpu].percpu_paddr = pte_val(mk_pte_phys(__pa(my_cpu_data), PAGE_KERNEL));
	/* Also save per-cpu tlb flush recipe for use in physical mode mca handler */
	ia64_mca_tlb_list[cpu].ptce_base = local_cpu_data->ptce_base;
	ia64_mca_tlb_list[cpu].ptce_count[0] = local_cpu_data->ptce_count[0];
	ia64_mca_tlb_list[cpu].ptce_count[1] = local_cpu_data->ptce_count[1];
	ia64_mca_tlb_list[cpu].ptce_stride[0] = local_cpu_data->ptce_stride[0];
	ia64_mca_tlb_list[cpu].ptce_stride[1] = local_cpu_data->ptce_stride[1];
#endif
}

static int
create_mem_map_page_table (u64 start, u64 end, void *arg)
{
	unsigned long address, start_page, end_page, next_blk_page;
	unsigned long blk_start;
	struct page *map_start, *map_end;
	int node=0;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	/* should we use platform_map_nr here? */

	map_start = vmem_map + MAP_NR_DENSE(start);
	map_end   = vmem_map + MAP_NR_DENSE(end);

	start_page = (unsigned long) map_start & PAGE_MASK;
	end_page = PAGE_ALIGN((unsigned long) map_end);

	/* force the first iteration to get node id */
	blk_start = start;
	next_blk_page = 0;

	for (address = start_page; address < end_page; address += PAGE_SIZE) {

		/* if we went across a node boundary, get new nid */
		if (address >= next_blk_page) {
			struct page *map_next_blk;

			node = paddr_to_nid(__pa(blk_start));

			/* get end addr of this memblk as next blk_start */
			blk_start = (unsigned long) __va(min(end, memblk_endpaddr(__pa(blk_start))));
			map_next_blk = vmem_map + MAP_NR_DENSE(blk_start);
			next_blk_page = PAGE_ALIGN((unsigned long) map_next_blk);
		}

		pgd = pgd_offset_k(address);
		if (pgd_none(*pgd))
			pgd_populate(&init_mm, pgd, alloc_bootmem_pages_node(NODE_DATA(node), PAGE_SIZE));
		pmd = pmd_offset(pgd, address);

		if (pmd_none(*pmd))
			pmd_populate(&init_mm, pmd, alloc_bootmem_pages_node(NODE_DATA(node), PAGE_SIZE));
		pte = pte_offset(pmd, address);

		if (pte_none(*pte))
			set_pte(pte, mk_pte_phys(__pa(alloc_bootmem_pages_node(NODE_DATA(node), PAGE_SIZE)),
						 PAGE_KERNEL));
 	}
 	return 0;
}

struct memmap_init_callback_data {
	memmap_init_callback_t *memmap_init;
	struct page *start;
	struct page *end;
	int zone;
	int highmem;
};

struct memmap_count_callback_data {
	int node;
	unsigned long num_physpages;
	unsigned long num_dma_physpages;
	unsigned long min_pfn;
	unsigned long max_pfn;
} cdata;

static int
virtual_memmap_init (u64 start, u64 end, void *arg)
{
	struct memmap_init_callback_data *args;
	struct page *map_start, *map_end;

	args = (struct memmap_init_callback_data *) arg;

	/* Should we use platform_map_nr here? */

	map_start = mem_map + MAP_NR_DENSE(start);
	map_end   = mem_map + MAP_NR_DENSE(end);

	if (map_start < args->start)
		map_start = args->start;
	if (map_end > args->end)
		map_end = args->end;

	/*
	 * We have to initialize "out of bounds" struct page elements
	 * that fit completely on the same pages that were allocated
	 * for the "in bounds" elements because they may be referenced
	 * later (and found to be "reserved").
	 */
	map_start -= ((unsigned long) map_start & (PAGE_SIZE - 1))
			/ sizeof(struct page);
	map_end += ((PAGE_ALIGN((unsigned long) map_end) -
				(unsigned long) map_end)
			/ sizeof(struct page));

	if (map_start < map_end)
		(*args->memmap_init)(map_start, map_end, args->zone,
				     page_to_phys(map_start), args->highmem);

	return 0;
}

unsigned long
arch_memmap_init (memmap_init_callback_t *memmap_init, struct page *start,
	struct page *end, int zone, unsigned long start_paddr, int highmem)
{
	if (!vmem_map) 
		memmap_init(start,end,zone,page_to_phys(start),highmem);
	else {
		struct memmap_init_callback_data args;

		args.memmap_init = memmap_init;
		args.start = start;
		args.end = end;
		args.zone = zone;
		args.highmem = highmem;

		efi_memmap_walk(virtual_memmap_init, &args);
	}

	return page_to_phys(end-1) + PAGE_SIZE;;
}

int
ia64_page_valid (struct page *page)
{
	char byte;

	return     (__get_user(byte, (char *) page) == 0)
		&& (__get_user(byte, (char *) (page + 1) - 1) == 0);
}

#define GRANULEROUNDDOWN(n) ((n) & ~(IA64_GRANULE_SIZE-1))
#define GRANULEROUNDUP(n) (((n)+IA64_GRANULE_SIZE-1) & ~(IA64_GRANULE_SIZE-1))
#define ORDERROUNDDOWN(n) ((n) & ~((PAGE_SIZE<<MAX_ORDER)-1))
static int
count_pages (u64 start, u64 end, int node)
{
	start = __pa(start);
	end = __pa(end);
	if (node == cdata.node) {
		cdata.num_physpages += (end - start) >> PAGE_SHIFT;
		if (start <= __pa(MAX_DMA_ADDRESS))
			cdata.num_dma_physpages += (min(end, __pa(MAX_DMA_ADDRESS)) - start) >> PAGE_SHIFT;
		start = GRANULEROUNDDOWN(__pa(start));
		start = ORDERROUNDDOWN(start);
		end = GRANULEROUNDUP(__pa(end));
		cdata.max_pfn = max(cdata.max_pfn, end >> PAGE_SHIFT);
		cdata.min_pfn = min(cdata.min_pfn, start >> PAGE_SHIFT);
	}
	return 0;
}

static int
find_largest_hole(u64 start, u64 end, void *arg)
{
	u64 *max_gap = arg;
	static u64 last_end = PAGE_OFFSET;

	/* NOTE: this algorithm assumes efi memmap table is ordered */

	if (*max_gap < (start - last_end))
		*max_gap = start - last_end;
	last_end = end;
	return 0;
}

/*
 * Set up the page tables.
 */
void
paging_init (void)
{
	unsigned long max_dma;
	unsigned long zones_size[MAX_NR_ZONES];
	unsigned long zholes_size[MAX_NR_ZONES];
	unsigned long max_gap;
	int node;

	/* initialize mem_map[] */

	max_dma = virt_to_phys((void *) MAX_DMA_ADDRESS) >> PAGE_SHIFT;
	max_gap = 0;
	efi_memmap_walk(find_largest_hole, (u64 *)&max_gap);

	for (node=0; node < numnodes; node++) {
		memset(zones_size, 0, sizeof(zones_size));
		memset(zholes_size, 0, sizeof(zholes_size));
		memset(&cdata, 0, sizeof(cdata));

		cdata.node = node;
		cdata.min_pfn = ~0;

		efi_memmap_walk(filter_rsvd_memory, count_pages);
		num_dma_physpages += cdata.num_dma_physpages;
		num_physpages += cdata.num_physpages;

		if (cdata.min_pfn >= max_dma) {
			zones_size[ZONE_NORMAL] = cdata.max_pfn - cdata.min_pfn;
			zholes_size[ZONE_NORMAL] = cdata.max_pfn - cdata.min_pfn - cdata.num_physpages;
		} else if (cdata.max_pfn < max_dma) {
			zones_size[ZONE_DMA] = cdata.max_pfn - cdata.min_pfn;
			zholes_size[ZONE_DMA] = cdata.max_pfn - cdata.min_pfn - cdata.num_dma_physpages;
		} else {
			zones_size[ZONE_DMA] = max_dma - cdata.min_pfn;
			zholes_size[ZONE_DMA] = zones_size[ZONE_DMA] - cdata.num_dma_physpages;
			zones_size[ZONE_NORMAL] = cdata.max_pfn - max_dma;
			zholes_size[ZONE_NORMAL] = zones_size[ZONE_NORMAL] - (cdata.num_physpages - cdata.num_dma_physpages);
		}
	
		if (numnodes == 1 && max_gap < LARGE_GAP) {
			vmem_map = (struct page *)0;
			zones_size[ZONE_DMA] += cdata.min_pfn;
			zholes_size[ZONE_DMA] += cdata.min_pfn;
			free_area_init_core(0, NODE_DATA(node), &mem_map, zones_size, 0, zholes_size, NULL);
		} else {
	
			/* allocate virtual mem_map */
	
			if (node == 0) {
				unsigned long map_size;
				map_size = PAGE_ALIGN(max_low_pfn*sizeof(struct page));
				vmalloc_end -= map_size;
				mem_map = vmem_map = (struct page *) vmalloc_end;
				efi_memmap_walk(create_mem_map_page_table, 0);
				printk(KERN_INFO "Virtual mem_map starts at 0x%p\n", mem_map);
			}
	
			free_area_init_node(node, NODE_DATA(node), vmem_map+cdata.min_pfn, zones_size, 
				cdata.min_pfn<<PAGE_SHIFT, zholes_size);
		}
	}

	zero_page_memmap_ptr = virt_to_page(ia64_imva(empty_zero_page));
}

static int
count_reserved_pages (u64 start, u64 end, void *arg)
{
	unsigned long num_reserved = 0;
	struct page *pg;

	for (pg = virt_to_page((void *)start); pg < virt_to_page((void *)end); ++pg)
		if (PageReserved(pg))
			++num_reserved;
	reserved_pages += num_reserved;
	return 0;
}

void
mem_init (void)
{
	extern char __start_gate_section[];
	long codesize, datasize, initsize;
	unsigned long num_pgt_pages;
	pg_data_t *pgdat;


#ifdef CONFIG_PCI
	/*
	 * This needs to be called _after_ the command line has been parsed but _before_
	 * any drivers that may need the PCI DMA interface are initialized or bootmem has
	 * been freed.
	 */
	platform_pci_dma_init();
#endif

	if (!mem_map)
		BUG();

	max_mapnr = max_low_pfn;
	high_memory = __va(max_low_pfn * PAGE_SIZE);

	for_each_pgdat(pgdat)
		totalram_pages += free_all_bootmem_node(pgdat);

	reserved_pages = 0;
	efi_memmap_walk(filter_rsvd_memory, count_reserved_pages);

	codesize =  (unsigned long) &_etext - (unsigned long) &_stext;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk(KERN_INFO "Memory: %luk/%luk available (%luk code, %luk reserved, %luk data, %luk init)\n",
	       (unsigned long) nr_free_pages() << (PAGE_SHIFT - 10),
	       num_physpages << (PAGE_SHIFT - 10), codesize >> 10,
	       reserved_pages << (PAGE_SHIFT - 10), datasize >> 10, initsize >> 10);

	/*
	 * Allow for enough (cached) page table pages so that we can map the entire memory
	 * at least once.  Each task also needs a couple of page tables pages, so add in a
	 * fudge factor for that (don't use "threads-max" here; that would be wrong!).
	 * Don't allow the cache to be more than 10% of total memory, though.
	 */
#	define NUM_TASKS	500	/* typical number of tasks */
	num_pgt_pages = nr_free_pages() / PTRS_PER_PGD + NUM_TASKS;
	if (num_pgt_pages > nr_free_pages() / 10)
		num_pgt_pages = nr_free_pages() / 10;
	if (num_pgt_pages > pgt_cache_water[1])
		pgt_cache_water[1] = num_pgt_pages;

	/* install the gate page in the global page table: */
	put_gate_page(virt_to_page(ia64_imva(__start_gate_section)), GATE_ADDR);

#ifdef CONFIG_IA32_SUPPORT
	ia32_gdt_init();
#endif
}
