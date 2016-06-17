/*
 *  linux/mm/page_alloc.c
 *
 *  Manages the free list, the system allocates free pages here.
 *  Note that kmalloc() lives in slab.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  Reshaped it to be a zoned allocator, Ingo Molnar, Red Hat, 1999
 *  Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 *  Zone balancing, Kanoj Sarcar, SGI, Jan 2000
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/module.h>

int nr_swap_pages;
int nr_active_pages;
int nr_inactive_pages;
LIST_HEAD(inactive_list);
LIST_HEAD(active_list);
pg_data_t *pgdat_list;

/*
 *
 * The zone_table array is used to look up the address of the
 * struct zone corresponding to a given zone number (ZONE_DMA,
 * ZONE_NORMAL, or ZONE_HIGHMEM).
 */
zone_t *zone_table[MAX_NR_ZONES*MAX_NR_NODES];
EXPORT_SYMBOL(zone_table);

static char *zone_names[MAX_NR_ZONES] = { "DMA", "Normal", "HighMem" };
static int zone_balance_ratio[MAX_NR_ZONES] __initdata = { 128, 128, 128, };
static int zone_balance_min[MAX_NR_ZONES] __initdata = { 20 , 20, 20, };
static int zone_balance_max[MAX_NR_ZONES] __initdata = { 255 , 255, 255, };
static int lower_zone_reserve_ratio[MAX_NR_ZONES-1] = { 256, 32 };

int vm_gfp_debug = 0;

/*
 * Temporary debugging check.
 */
#define BAD_RANGE(zone, page)						\
(									\
	(((page) - mem_map) >= ((zone)->zone_start_mapnr+(zone)->size))	\
	|| (((page) - mem_map) < (zone)->zone_start_mapnr)		\
	|| ((zone) != page_zone(page))					\
)

/*
 * Freeing function for a buddy system allocator.
 * Contrary to prior comments, this is *NOT* hairy, and there
 * is no reason for anyone not to understand it.
 *
 * The concept of a buddy system is to maintain direct-mapped tables
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep one bit for each pair of blocks, which
 * is set to 1 iff only one of the pair is allocated.  So when we
 * are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were   
 * free, the remainder of the region must be split into blocks.   
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.            
 *
 * -- wli
 */

static void FASTCALL(__free_pages_ok (struct page *page, unsigned int order));
static void __free_pages_ok (struct page *page, unsigned int order)
{
	unsigned long index, page_idx, mask, flags;
	free_area_t *area;
	struct page *base;
	zone_t *zone;

	/*
	 * Yes, think what happens when other parts of the kernel take 
	 * a reference to a page in order to pin it for io. -ben
	 */
	if (PageLRU(page)) {
		if (unlikely(in_interrupt()))
			BUG();
		lru_cache_del(page);
	}

	if (page->buffers)
		BUG();
	if (page->mapping)
		BUG();
	if (!VALID_PAGE(page))
		BUG();
	if (PageLocked(page))
		BUG();
	if (PageActive(page))
		BUG();
	ClearPageReferenced(page);
	ClearPageDirty(page);

	if (current->flags & PF_FREE_PAGES)
		goto local_freelist;
 back_local_freelist:

	zone = page_zone(page);

	mask = (~0UL) << order;
	base = zone->zone_mem_map;
	page_idx = page - base;
	if (page_idx & ~mask)
		BUG();
	index = page_idx >> (1 + order);

	area = zone->free_area + order;

	spin_lock_irqsave(&zone->lock, flags);

	zone->free_pages -= mask;

	while (mask + (1 << (MAX_ORDER-1))) {
		struct page *buddy1, *buddy2;

		if (area >= zone->free_area + MAX_ORDER)
			BUG();
		if (!__test_and_change_bit(index, area->map))
			/*
			 * the buddy page is still allocated.
			 */
			break;
		/*
		 * Move the buddy up one level.
		 * This code is taking advantage of the identity:
		 * 	-mask = 1+~mask
		 */
		buddy1 = base + (page_idx ^ -mask);
		buddy2 = base + page_idx;
		if (BAD_RANGE(zone,buddy1))
			BUG();
		if (BAD_RANGE(zone,buddy2))
			BUG();

		list_del(&buddy1->list);
		mask <<= 1;
		area++;
		index >>= 1;
		page_idx &= mask;
	}
	list_add(&(base + page_idx)->list, &area->free_list);

	spin_unlock_irqrestore(&zone->lock, flags);
	return;

 local_freelist:
	if (current->nr_local_pages)
		goto back_local_freelist;
	if (in_interrupt())
		goto back_local_freelist;		

	list_add(&page->list, &current->local_pages);
	page->index = order;
	current->nr_local_pages++;
}

#define MARK_USED(index, order, area) \
	__change_bit((index) >> (1+(order)), (area)->map)

static inline struct page * expand (zone_t *zone, struct page *page,
	 unsigned long index, int low, int high, free_area_t * area)
{
	unsigned long size = 1 << high;

	while (high > low) {
		if (BAD_RANGE(zone,page))
			BUG();
		area--;
		high--;
		size >>= 1;
		list_add(&(page)->list, &(area)->free_list);
		MARK_USED(index, high, area);
		index += size;
		page += size;
	}
	if (BAD_RANGE(zone,page))
		BUG();
	return page;
}

static FASTCALL(struct page * rmqueue(zone_t *zone, unsigned int order));
static struct page * rmqueue(zone_t *zone, unsigned int order)
{
	free_area_t * area = zone->free_area + order;
	unsigned int curr_order = order;
	struct list_head *head, *curr;
	unsigned long flags;
	struct page *page;

	spin_lock_irqsave(&zone->lock, flags);
	do {
		head = &area->free_list;
		curr = head->next;

		if (curr != head) {
			unsigned int index;

			page = list_entry(curr, struct page, list);
			if (BAD_RANGE(zone,page))
				BUG();
			list_del(curr);
			index = page - zone->zone_mem_map;
			if (curr_order != MAX_ORDER-1)
				MARK_USED(index, curr_order, area);
			zone->free_pages -= 1UL << order;

			page = expand(zone, page, index, order, curr_order, area);
			spin_unlock_irqrestore(&zone->lock, flags);

			set_page_count(page, 1);
			if (BAD_RANGE(zone,page))
				BUG();
			if (PageLRU(page))
				BUG();
			if (PageActive(page))
				BUG();
			return page;	
		}
		curr_order++;
		area++;
	} while (curr_order < MAX_ORDER);
	spin_unlock_irqrestore(&zone->lock, flags);

	return NULL;
}

#ifndef CONFIG_DISCONTIGMEM
struct page *_alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	return __alloc_pages(gfp_mask, order,
		contig_page_data.node_zonelists+(gfp_mask & GFP_ZONEMASK));
}
#endif

static struct page * FASTCALL(balance_classzone(zone_t *, unsigned int, unsigned int, int *));
static struct page * balance_classzone(zone_t * classzone, unsigned int gfp_mask, unsigned int order, int * freed)
{
	struct page * page = NULL;
	int __freed;

	if (in_interrupt())
		BUG();

	current->allocation_order = order;
	current->flags |= PF_MEMALLOC | PF_FREE_PAGES;

	__freed = try_to_free_pages_zone(classzone, gfp_mask);

	current->flags &= ~(PF_MEMALLOC | PF_FREE_PAGES);

	if (current->nr_local_pages) {
		struct list_head * entry, * local_pages;
		struct page * tmp;
		int nr_pages;

		local_pages = &current->local_pages;

		if (likely(__freed)) {
			/* pick from the last inserted so we're lifo */
			entry = local_pages->next;
			do {
				tmp = list_entry(entry, struct page, list);
				if (tmp->index == order && memclass(page_zone(tmp), classzone)) {
					list_del(entry);
					current->nr_local_pages--;
					set_page_count(tmp, 1);
					page = tmp;

					if (page->buffers)
						BUG();
					if (page->mapping)
						BUG();
					if (!VALID_PAGE(page))
						BUG();
					if (PageLocked(page))
						BUG();
					if (PageLRU(page))
						BUG();
					if (PageActive(page))
						BUG();
					if (PageDirty(page))
						BUG();

					break;
				}
			} while ((entry = entry->next) != local_pages);
		}

		nr_pages = current->nr_local_pages;
		/* free in reverse order so that the global order will be lifo */
		while ((entry = local_pages->prev) != local_pages) {
			list_del(entry);
			tmp = list_entry(entry, struct page, list);
			__free_pages_ok(tmp, tmp->index);
			if (!nr_pages--)
				BUG();
		}
		current->nr_local_pages = 0;
	}

	*freed = __freed;
	return page;
}

static inline unsigned long zone_free_pages(zone_t * zone, unsigned int order)
{
	long free = zone->free_pages - (1UL << order);
	return free >= 0 ? free : 0;
}

/*
 * This is the 'heart' of the zoned buddy allocator:
 */
struct page * __alloc_pages(unsigned int gfp_mask, unsigned int order, zonelist_t *zonelist)
{
	zone_t **zone, * classzone;
	struct page * page;
	int freed, class_idx;

	zone = zonelist->zones;
	classzone = *zone;
	class_idx = zone_idx(classzone);

	for (;;) {
		zone_t *z = *(zone++);
		if (!z)
			break;

		if (zone_free_pages(z, order) > z->watermarks[class_idx].low) {
			page = rmqueue(z, order);
			if (page)
				return page;
		}
	}

	classzone->need_balance = 1;
	mb();
	if (waitqueue_active(&kswapd_wait))
		wake_up_interruptible(&kswapd_wait);

	zone = zonelist->zones;
	for (;;) {
		unsigned long min;
		zone_t *z = *(zone++);
		if (!z)
			break;

		min = z->watermarks[class_idx].min;
		if (!(gfp_mask & __GFP_WAIT))
			min >>= 2;
		if (zone_free_pages(z, order) > min) {
			page = rmqueue(z, order);
			if (page)
				return page;
		}
	}

	/* here we're in the low on memory slow path */

	if ((current->flags & PF_MEMALLOC) && 
			(!in_interrupt() || (current->flags & PF_MEMDIE))) {
		zone = zonelist->zones;
		for (;;) {
			zone_t *z = *(zone++);
			if (!z)
				break;

			page = rmqueue(z, order);
			if (page)
				return page;
		}
		return NULL;
	}

	/* Atomic allocations - we can't balance anything */
	if (!(gfp_mask & __GFP_WAIT))
		goto out;

 rebalance:
	page = balance_classzone(classzone, gfp_mask, order, &freed);
	if (page)
		return page;

	zone = zonelist->zones;
	if (likely(freed)) {
		for (;;) {
			zone_t *z = *(zone++);
			if (!z)
				break;

			if (zone_free_pages(z, order) > z->watermarks[class_idx].min) {
				page = rmqueue(z, order);
				if (page)
					return page;
			}
		}
		goto rebalance;
	} else {
		/* 
		 * Check that no other task is been killed meanwhile,
		 * in such a case we can succeed the allocation.
		 */
		for (;;) {
			zone_t *z = *(zone++);
			if (!z)
				break;

			if (zone_free_pages(z, order) > z->watermarks[class_idx].high) {
				page = rmqueue(z, order);
				if (page)
					return page;
			}
		}
	}

 out:
	printk(KERN_NOTICE "__alloc_pages: %u-order allocation failed (gfp=0x%x/%i)\n",
	       order, gfp_mask, !!(current->flags & PF_MEMALLOC));
	if (unlikely(vm_gfp_debug))
		dump_stack();
	return NULL;
}

/*
 * Common helper functions.
 */
unsigned long __get_free_pages(unsigned int gfp_mask, unsigned int order)
{
	struct page * page;

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return 0;
	return (unsigned long) page_address(page);
}

unsigned long get_zeroed_page(unsigned int gfp_mask)
{
	struct page * page;

	page = alloc_pages(gfp_mask, 0);
	if (page) {
		void *address = page_address(page);
		clear_page(address);
		return (unsigned long) address;
	}
	return 0;
}

void __free_pages(struct page *page, unsigned int order)
{
	if (!PageReserved(page) && put_page_testzero(page))
		__free_pages_ok(page, order);
}

void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0)
		__free_pages(virt_to_page(addr), order);
}

/*
 * Total amount of free (allocatable) RAM:
 */
unsigned int nr_free_pages (void)
{
	unsigned int sum = 0;
	zone_t *zone;

	for_each_zone(zone)
		sum += zone->free_pages;

	return sum;
}

/*
 * Amount of free RAM allocatable as buffer memory:
 */
unsigned int nr_free_buffer_pages (void)
{
	pg_data_t *pgdat;
	unsigned int sum = 0;
	zonelist_t *zonelist;
	zone_t **zonep, *zone;

	for_each_pgdat(pgdat) {
		int class_idx;
		zonelist = pgdat->node_zonelists + (GFP_USER & GFP_ZONEMASK);
		zonep = zonelist->zones;
		zone = *zonep;
		class_idx = zone_idx(zone);

		sum += zone->nr_cache_pages;
		for (zone = pgdat->node_zones; zone < pgdat->node_zones + MAX_NR_ZONES; zone++) {
			int free = zone->free_pages - zone->watermarks[class_idx].high;
			if (free <= 0)
				continue;
			sum += free;
		}
	}

	return sum;
}

#if CONFIG_HIGHMEM
unsigned int nr_free_highpages (void)
{
	pg_data_t *pgdat;
	unsigned int pages = 0;

	for_each_pgdat(pgdat)
		pages += pgdat->node_zones[ZONE_HIGHMEM].free_pages;

	return pages;
}

unsigned int freeable_lowmem(void)
{
	unsigned int pages = 0;
	pg_data_t *pgdat;

	for_each_pgdat(pgdat) {
		pages += pgdat->node_zones[ZONE_DMA].free_pages;
		pages += pgdat->node_zones[ZONE_DMA].nr_active_pages;
		pages += pgdat->node_zones[ZONE_DMA].nr_inactive_pages;
		pages += pgdat->node_zones[ZONE_NORMAL].free_pages;
		pages += pgdat->node_zones[ZONE_NORMAL].nr_active_pages;
		pages += pgdat->node_zones[ZONE_NORMAL].nr_inactive_pages;
	}

	return pages;
}
#endif

#define K(x) ((x) << (PAGE_SHIFT-10))

/*
 * Show free area list (used inside shift_scroll-lock stuff)
 * We also calculate the percentage fragmentation. We do this by counting the
 * memory on each free list with the exception of the first item on the list.
 */
void show_free_areas_core(pg_data_t *pgdat)
{
 	unsigned int order;
	unsigned type;
	pg_data_t *tmpdat = pgdat;

	printk("Free pages:      %6dkB (%6dkB HighMem)\n",
		K(nr_free_pages()),
		K(nr_free_highpages()));

	while (tmpdat) {
		zone_t *zone;
		for (zone = tmpdat->node_zones;
			       	zone < tmpdat->node_zones + MAX_NR_ZONES; zone++)
			printk("Zone:%s freepages:%6lukB\n", 
					zone->name,
					K(zone->free_pages));
			
		tmpdat = tmpdat->node_next;
	}

	printk("( Active: %d, inactive: %d, free: %d )\n",
	       nr_active_pages,
	       nr_inactive_pages,
	       nr_free_pages());

	for (type = 0; type < MAX_NR_ZONES; type++) {
		struct list_head *head, *curr;
		zone_t *zone = pgdat->node_zones + type;
 		unsigned long nr, total, flags;

		total = 0;
		if (zone->size) {
			spin_lock_irqsave(&zone->lock, flags);
		 	for (order = 0; order < MAX_ORDER; order++) {
				head = &(zone->free_area + order)->free_list;
				curr = head;
				nr = 0;
				for (;;) {
					if ((curr = curr->next) == head)
						break;
					nr++;
				}
				total += nr * (1 << order);
				printk("%lu*%lukB ", nr, K(1UL) << order);
			}
			spin_unlock_irqrestore(&zone->lock, flags);
		}
		printk("= %lukB)\n", K(total));
	}

#ifdef SWAP_CACHE_INFO
	show_swap_cache_info();
#endif	
}

void show_free_areas(void)
{
	show_free_areas_core(pgdat_list);
}

/*
 * Builds allocation fallback zone lists.
 */
static inline void build_zonelists(pg_data_t *pgdat)
{
	int i, j, k;

	for (i = 0; i <= GFP_ZONEMASK; i++) {
		zonelist_t *zonelist;
		zone_t *zone;

		zonelist = pgdat->node_zonelists + i;
		memset(zonelist, 0, sizeof(*zonelist));

		j = 0;
		k = ZONE_NORMAL;
		if (i & __GFP_HIGHMEM)
			k = ZONE_HIGHMEM;
		if (i & __GFP_DMA)
			k = ZONE_DMA;

		switch (k) {
			default:
				BUG();
			/*
			 * fallthrough:
			 */
			case ZONE_HIGHMEM:
				zone = pgdat->node_zones + ZONE_HIGHMEM;
				if (zone->size) {
#ifndef CONFIG_HIGHMEM
					BUG();
#endif
					zonelist->zones[j++] = zone;
				}
			case ZONE_NORMAL:
				zone = pgdat->node_zones + ZONE_NORMAL;
				if (zone->size)
					zonelist->zones[j++] = zone;
			case ZONE_DMA:
				zone = pgdat->node_zones + ZONE_DMA;
				if (zone->size)
					zonelist->zones[j++] = zone;
		}
		zonelist->zones[j++] = NULL;
	} 
}

/*
 * Helper functions to size the waitqueue hash table.
 * Essentially these want to choose hash table sizes sufficiently
 * large so that collisions trying to wait on pages are rare.
 * But in fact, the number of active page waitqueues on typical
 * systems is ridiculously low, less than 200. So this is even
 * conservative, even though it seems large.
 *
 * The constant PAGES_PER_WAITQUEUE specifies the ratio of pages to
 * waitqueues, i.e. the size of the waitq table given the number of pages.
 */
#define PAGES_PER_WAITQUEUE	256

static inline unsigned long wait_table_size(unsigned long pages)
{
	unsigned long size = 1;

	pages /= PAGES_PER_WAITQUEUE;

	while (size < pages)
		size <<= 1;

	/*
	 * Once we have dozens or even hundreds of threads sleeping
	 * on IO we've got bigger problems than wait queue collision.
	 * Limit the size of the wait table to a reasonable size.
	 */
	size = min(size, 4096UL);

	return size;
}

/*
 * This is an integer logarithm so that shifts can be used later
 * to extract the more random high bits from the multiplicative
 * hash function before the remainder is taken.
 */
static inline unsigned long wait_table_bits(unsigned long size)
{
	return ffz(~size);
}

#define LONG_ALIGN(x) (((x)+(sizeof(long))-1)&~((sizeof(long))-1))

/*
 * Set up the zone data structures:
 *   - mark all pages reserved
 *   - mark all memory queues empty
 *   - clear the memory bitmaps
 */
void __init free_area_init_core(int nid, pg_data_t *pgdat, struct page **gmap,
	unsigned long *zones_size, unsigned long zone_start_paddr, 
	unsigned long *zholes_size, struct page *lmem_map)
{
	unsigned long i, j;
	unsigned long map_size;
	unsigned long totalpages, offset, realtotalpages;
	const unsigned long zone_required_alignment = 1UL << (MAX_ORDER-1);

	if (zone_start_paddr & ~PAGE_MASK)
		BUG();

	totalpages = 0;
	for (i = 0; i < MAX_NR_ZONES; i++) {
		unsigned long size = zones_size[i];
		totalpages += size;
	}
	realtotalpages = totalpages;
	if (zholes_size)
		for (i = 0; i < MAX_NR_ZONES; i++)
			realtotalpages -= zholes_size[i];
			
	printk("On node %d totalpages: %lu\n", nid, realtotalpages);

	/*
	 * Some architectures (with lots of mem and discontinous memory
	 * maps) have to search for a good mem_map area:
	 * For discontigmem, the conceptual mem map array starts from 
	 * PAGE_OFFSET, we need to align the actual array onto a mem map 
	 * boundary, so that MAP_NR works.
	 */
	map_size = (totalpages + 1)*sizeof(struct page);
	if (lmem_map == (struct page *)0) {
		lmem_map = (struct page *) alloc_bootmem_node(pgdat, map_size);
		lmem_map = (struct page *)(PAGE_OFFSET + 
			MAP_ALIGN((unsigned long)lmem_map - PAGE_OFFSET));
	}
	*gmap = pgdat->node_mem_map = lmem_map;
	pgdat->node_size = totalpages;
	pgdat->node_start_paddr = zone_start_paddr;
	pgdat->node_start_mapnr = (lmem_map - mem_map);
	pgdat->nr_zones = 0;

	offset = lmem_map - mem_map;	
	for (j = 0; j < MAX_NR_ZONES; j++) {
		zone_t *zone = pgdat->node_zones + j;
		unsigned long mask;
		unsigned long size, realsize;
		int idx;

		zone_table[nid * MAX_NR_ZONES + j] = zone;
		realsize = size = zones_size[j];
		if (zholes_size)
			realsize -= zholes_size[j];

		printk("zone(%lu): %lu pages.\n", j, size);
		zone->size = size;
		zone->realsize = realsize;
		zone->name = zone_names[j];
		zone->lock = SPIN_LOCK_UNLOCKED;
		zone->zone_pgdat = pgdat;
		zone->free_pages = 0;
		zone->need_balance = 0;
		 zone->nr_active_pages = zone->nr_inactive_pages = 0;


		if (!size)
			continue;

		/*
		 * The per-page waitqueue mechanism uses hashed waitqueues
		 * per zone.
		 */
		zone->wait_table_size = wait_table_size(size);
		zone->wait_table_shift =
			BITS_PER_LONG - wait_table_bits(zone->wait_table_size);
		zone->wait_table = (wait_queue_head_t *)
			alloc_bootmem_node(pgdat, zone->wait_table_size
						* sizeof(wait_queue_head_t));

		for(i = 0; i < zone->wait_table_size; ++i)
			init_waitqueue_head(zone->wait_table + i);

		pgdat->nr_zones = j+1;

		mask = (realsize / zone_balance_ratio[j]);
		if (mask < zone_balance_min[j])
			mask = zone_balance_min[j];
		else if (mask > zone_balance_max[j])
			mask = zone_balance_max[j];
		zone->watermarks[j].min = mask;
		zone->watermarks[j].low = mask*2;
		zone->watermarks[j].high = mask*3;
		/* now set the watermarks of the lower zones in the "j" classzone */
		for (idx = j-1; idx >= 0; idx--) {
			zone_t * lower_zone = pgdat->node_zones + idx;
			unsigned long lower_zone_reserve;
			if (!lower_zone->size)
				continue;

			mask = lower_zone->watermarks[idx].min;
			lower_zone->watermarks[j].min = mask;
			lower_zone->watermarks[j].low = mask*2;
			lower_zone->watermarks[j].high = mask*3;

			/* now the brainer part */
			lower_zone_reserve = realsize / lower_zone_reserve_ratio[idx];
			lower_zone->watermarks[j].min += lower_zone_reserve;
			lower_zone->watermarks[j].low += lower_zone_reserve;
			lower_zone->watermarks[j].high += lower_zone_reserve;

			realsize += lower_zone->realsize;
		}

		zone->zone_mem_map = mem_map + offset;
		zone->zone_start_mapnr = offset;
		zone->zone_start_paddr = zone_start_paddr;

		if ((zone_start_paddr >> PAGE_SHIFT) & (zone_required_alignment-1))
			printk("BUG: wrong zone alignment, it will crash\n");

		/*
		 * Initially all pages are reserved - free ones are freed
		 * up by free_all_bootmem() once the early boot process is
		 * done. Non-atomic initialization, single-pass.
		 */
		for (i = 0; i < size; i++) {
			struct page *page = mem_map + offset + i;
			set_page_zone(page, nid * MAX_NR_ZONES + j);
			set_page_count(page, 0);
			SetPageReserved(page);
			INIT_LIST_HEAD(&page->list);
			if (j != ZONE_HIGHMEM)
				set_page_address(page, __va(zone_start_paddr));
			zone_start_paddr += PAGE_SIZE;
		}

		offset += size;
		for (i = 0; ; i++) {
			unsigned long bitmap_size;

			INIT_LIST_HEAD(&zone->free_area[i].free_list);
			if (i == MAX_ORDER-1) {
				zone->free_area[i].map = NULL;
				break;
			}

			/*
			 * Page buddy system uses "index >> (i+1)",
			 * where "index" is at most "size-1".
			 *
			 * The extra "+3" is to round down to byte
			 * size (8 bits per byte assumption). Thus
			 * we get "(size-1) >> (i+4)" as the last byte
			 * we can access.
			 *
			 * The "+1" is because we want to round the
			 * byte allocation up rather than down. So
			 * we should have had a "+7" before we shifted
			 * down by three. Also, we have to add one as
			 * we actually _use_ the last bit (it's [0,n]
			 * inclusive, not [0,n[).
			 *
			 * So we actually had +7+1 before we shift
			 * down by 3. But (n+8) >> 3 == (n >> 3) + 1
			 * (modulo overflows, which we do not have).
			 *
			 * Finally, we LONG_ALIGN because all bitmap
			 * operations are on longs.
			 */
			bitmap_size = (size-1) >> (i+4);
			bitmap_size = LONG_ALIGN(bitmap_size+1);
			zone->free_area[i].map = 
			  (unsigned long *) alloc_bootmem_node(pgdat, bitmap_size);
		}
	}
	build_zonelists(pgdat);
}

void __init free_area_init(unsigned long *zones_size)
{
	free_area_init_core(0, &contig_page_data, &mem_map, zones_size, 0, 0, 0);
}

static int __init setup_mem_frac(char *str)
{
	int j = 0;

	while (get_option(&str, &zone_balance_ratio[j++]) == 2);
	printk("setup_mem_frac: ");
	for (j = 0; j < MAX_NR_ZONES; j++) printk("%d  ", zone_balance_ratio[j]);
	printk("\n");
	return 1;
}

__setup("memfrac=", setup_mem_frac);

static int __init setup_lower_zone_reserve(char *str)
{
	int j = 0;

	while (get_option(&str, &lower_zone_reserve_ratio[j++]) == 2);
	printk("setup_lower_zone_reserve: ");
	for (j = 0; j < MAX_NR_ZONES-1; j++) printk("%d  ", lower_zone_reserve_ratio[j]);
	printk("\n");
	return 1;
}

__setup("lower_zone_reserve=", setup_lower_zone_reserve);
