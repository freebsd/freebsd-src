#ifndef _LINUX_MMZONE_H
#define _LINUX_MMZONE_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>

/*
 * Free memory management - zoned buddy allocator.
 */

#ifndef CONFIG_FORCE_MAX_ZONEORDER
#define MAX_ORDER 10
#else
#define MAX_ORDER CONFIG_FORCE_MAX_ZONEORDER
#endif

#define ZONE_DMA               0
#define ZONE_NORMAL            1
#define ZONE_HIGHMEM           2
#define MAX_NR_ZONES           3

typedef struct free_area_struct {
	struct list_head	free_list;
	unsigned long		*map;
} free_area_t;

struct pglist_data;

typedef struct zone_watermarks_s {
	unsigned long min, low, high;
} zone_watermarks_t;


/*
 * On machines where it is needed (eg PCs) we divide physical memory
 * into multiple physical zones. On a PC we have 3 zones:
 *
 * ZONE_DMA	  < 16 MB	ISA DMA capable memory
 * ZONE_NORMAL	16-896 MB	direct mapped by the kernel
 * ZONE_HIGHMEM	 > 896 MB	only page cache and user processes
 */
typedef struct zone_struct {
	/*
	 * Commonly accessed fields:
	 */
	spinlock_t		lock;
	unsigned long		free_pages;
	/*
	 * We don't know if the memory that we're going to allocate will be freeable
	 * or/and it will be released eventually, so to avoid totally wasting several
	 * GB of ram we must reserve some of the lower zone memory (otherwise we risk
	 * to run OOM on the lower zones despite there's tons of freeable ram
	 * on the higher zones).
	 */
	zone_watermarks_t       watermarks[MAX_NR_ZONES];

	/*
	 * The below fields are protected by different locks (or by
	 * no lock at all like need_balance), so they're longs to
	 * provide an atomic granularity against each other on
	 * all architectures.
	 */
	unsigned long           need_balance;
	/* protected by the pagemap_lru_lock */
	unsigned long           nr_active_pages, nr_inactive_pages;
	/* protected by the pagecache_lock */
	unsigned long           nr_cache_pages;


	/*
	 * free areas of different sizes
	 */
	free_area_t		free_area[MAX_ORDER];

	/*
	 * wait_table		-- the array holding the hash table
	 * wait_table_size	-- the size of the hash table array
	 * wait_table_shift	-- wait_table_size
	 * 				== BITS_PER_LONG (1 << wait_table_bits)
	 *
	 * The purpose of all these is to keep track of the people
	 * waiting for a page to become available and make them
	 * runnable again when possible. The trouble is that this
	 * consumes a lot of space, especially when so few things
	 * wait on pages at a given time. So instead of using
	 * per-page waitqueues, we use a waitqueue hash table.
	 *
	 * The bucket discipline is to sleep on the same queue when
	 * colliding and wake all in that wait queue when removing.
	 * When something wakes, it must check to be sure its page is
	 * truly available, a la thundering herd. The cost of a
	 * collision is great, but given the expected load of the
	 * table, they should be so rare as to be outweighed by the
	 * benefits from the saved space.
	 *
	 * __wait_on_page() and unlock_page() in mm/filemap.c, are the
	 * primary users of these fields, and in mm/page_alloc.c
	 * free_area_init_core() performs the initialization of them.
	 */
	wait_queue_head_t	* wait_table;
	unsigned long		wait_table_size;
	unsigned long		wait_table_shift;

	/*
	 * Discontig memory support fields.
	 */
	struct pglist_data	*zone_pgdat;
	struct page		*zone_mem_map;
	unsigned long		zone_start_paddr;
	unsigned long		zone_start_mapnr;

	/*
	 * rarely used fields:
	 */
	char			*name;
	unsigned long		size;
	unsigned long		realsize;
} zone_t;

/*
 * One allocation request operates on a zonelist. A zonelist
 * is a list of zones, the first one is the 'goal' of the
 * allocation, the other zones are fallback zones, in decreasing
 * priority.
 *
 * Right now a zonelist takes up less than a cacheline. We never
 * modify it apart from boot-up, and only a few indices are used,
 * so despite the zonelist table being relatively big, the cache
 * footprint of this construct is very small.
 */
typedef struct zonelist_struct {
	zone_t * zones [MAX_NR_ZONES+1]; // NULL delimited
} zonelist_t;

#define GFP_ZONEMASK	0x0f

/*
 * The pg_data_t structure is used in machines with CONFIG_DISCONTIGMEM
 * (mostly NUMA machines?) to denote a higher-level memory zone than the
 * zone_struct denotes.
 *
 * On NUMA machines, each NUMA node would have a pg_data_t to describe
 * it's memory layout.
 *
 * XXX: we need to move the global memory statistics (active_list, ...)
 *      into the pg_data_t to properly support NUMA.
 */
struct bootmem_data;
typedef struct pglist_data {
	zone_t node_zones[MAX_NR_ZONES];
	zonelist_t node_zonelists[GFP_ZONEMASK+1];
	int nr_zones;
	struct page *node_mem_map;
	unsigned long *valid_addr_bitmap;
	struct bootmem_data *bdata;
	unsigned long node_start_paddr;
	unsigned long node_start_mapnr;
	unsigned long node_size;
	int node_id;
	struct pglist_data *node_next;
} pg_data_t;

extern int numnodes;
extern pg_data_t *pgdat_list;

#define zone_idx(zone)                 ((zone) - (zone)->zone_pgdat->node_zones)
#define memclass(pgzone, classzone)    (zone_idx(pgzone) <= zone_idx(classzone))

/*
 * The following two are not meant for general usage. They are here as
 * prototypes for the discontig memory code.
 */
struct page;
extern void show_free_areas_core(pg_data_t *pgdat);
extern void free_area_init_core(int nid, pg_data_t *pgdat, struct page **gmap,
  unsigned long *zones_size, unsigned long paddr, unsigned long *zholes_size,
  struct page *pmap);

extern pg_data_t contig_page_data;

/**
 * for_each_pgdat - helper macro to iterate over all nodes
 * @pgdat - pg_data_t * variable
 *
 * Meant to help with common loops of the form
 * pgdat = pgdat_list;
 * while(pgdat) {
 * 	...
 * 	pgdat = pgdat->node_next;
 * }
 */
#define for_each_pgdat(pgdat) \
	for (pgdat = pgdat_list; pgdat; pgdat = pgdat->node_next)


/*
 * next_zone - helper magic for for_each_zone()
 * Thanks to William Lee Irwin III for this piece of ingenuity.
 */
static inline zone_t *next_zone(zone_t *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone - pgdat->node_zones < MAX_NR_ZONES - 1)
		zone++;

	else if (pgdat->node_next) {
		pgdat = pgdat->node_next;
		zone = pgdat->node_zones;
	} else
		zone = NULL;

	return zone;
}

/**
 * for_each_zone - helper macro to iterate over all memory zones
 * @zone - zone_t * variable
 *
 * The user only needs to declare the zone variable, for_each_zone
 * fills it in. This basically means for_each_zone() is an
 * easier to read version of this piece of code:
 *
 * for(pgdat = pgdat_list; pgdat; pgdat = pgdat->node_next)
 * 	for(i = 0; i < MAX_NR_ZONES; ++i) {
 * 		zone_t * z = pgdat->node_zones + i;
 * 		...
 * 	}
 * }
 */
#define for_each_zone(zone) \
	for(zone = pgdat_list->node_zones; zone; zone = next_zone(zone))


#ifndef CONFIG_DISCONTIGMEM

#define NODE_DATA(nid)		(&contig_page_data)
#define NODE_MEM_MAP(nid)	mem_map
#define MAX_NR_NODES		1

#else /* !CONFIG_DISCONTIGMEM */

#include <asm/mmzone.h>

/* page->zone is currently 8 bits ... */
#ifndef MAX_NR_NODES
#define MAX_NR_NODES		(255 / MAX_NR_ZONES)
#endif

#endif /* !CONFIG_DISCONTIGMEM */

#define MAP_ALIGN(x)	((((x) % sizeof(mem_map_t)) == 0) ? (x) : ((x) + \
		sizeof(mem_map_t) - ((x) % sizeof(mem_map_t))))

#endif /* !__ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* _LINUX_MMZONE_H */
