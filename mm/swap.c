/*
 *  linux/mm/swap.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 */

/*
 * This file contains the default values for the opereation of the
 * Linux VM subsystem. Fine-tuning documentation can be found in
 * linux/Documentation/sysctl/vm.txt.
 * Started 18.12.91
 * Swap aging added 23.2.95, Stephen Tweedie.
 * Buffermem limits added 12.3.98, Rik van Riel.
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/pagemap.h>
#include <linux/init.h>

#include <asm/dma.h>
#include <asm/uaccess.h> /* for copy_to/from_user */
#include <asm/pgtable.h>

/* How many pages do we try to swap or page in/out together? */
int page_cluster;

pager_daemon_t pager_daemon = {
	512,	/* base number for calculating the number of tries */
	SWAP_CLUSTER_MAX,	/* minimum number of tries */
	8,	/* do swap I/O in clusters of this size */
};

/*
 * Move an inactive page to the active list.
 */
static inline void activate_page_nolock(struct page * page)
{
	if (PageLRU(page) && !PageActive(page)) {
		del_page_from_inactive_list(page);
		add_page_to_active_list(page);
	}
}

void activate_page(struct page * page)
{
	spin_lock(&pagemap_lru_lock);
	activate_page_nolock(page);
	spin_unlock(&pagemap_lru_lock);
}

/**
 * lru_cache_add: add a page to the page lists
 * @page: the page to add
 */
void lru_cache_add(struct page * page)
{
	if (!PageLRU(page)) {
		spin_lock(&pagemap_lru_lock);
		if (!TestSetPageLRU(page))
			add_page_to_inactive_list(page);
		spin_unlock(&pagemap_lru_lock);
	}
}

/**
 * __lru_cache_del: remove a page from the page lists
 * @page: the page to add
 *
 * This function is for when the caller already holds
 * the pagemap_lru_lock.
 */
void __lru_cache_del(struct page * page)
{
	if (TestClearPageLRU(page)) {
		if (PageActive(page)) {
			del_page_from_active_list(page);
		} else {
			del_page_from_inactive_list(page);
		}
	}
}

/**
 * lru_cache_del: remove a page from the page lists
 * @page: the page to remove
 */
void lru_cache_del(struct page * page)
{
	spin_lock(&pagemap_lru_lock);
	__lru_cache_del(page);
	spin_unlock(&pagemap_lru_lock);
}

/**
 * delta_nr_active_pages: alter the number of active pages.
 *
 * @page: the page which is being activated/deactivated
 * @delta: +1 for activation, -1 for deactivation
 *
 * Called under pagecache_lock
 */
void delta_nr_active_pages(struct page *page, long delta)
{
	pg_data_t *pgdat;
	zone_t *classzone, *overflow;

	classzone = page_zone(page);
	pgdat = classzone->zone_pgdat;
	overflow = pgdat->node_zones + pgdat->nr_zones;

	while (classzone < overflow) {
		classzone->nr_active_pages += delta;
		classzone++;
	}
	nr_active_pages += delta;
}

/**
 * delta_nr_inactive_pages: alter the number of inactive pages.
 *
 * @page: the page which is being deactivated/activated
 * @delta: +1 for deactivation, -1 for activation
 *
 * Called under pagecache_lock
 */
void delta_nr_inactive_pages(struct page *page, long delta)
{
	pg_data_t *pgdat;
	zone_t *classzone, *overflow;

	classzone = page_zone(page);
	pgdat = classzone->zone_pgdat;
	overflow = pgdat->node_zones + pgdat->nr_zones;

	while (classzone < overflow) {
		classzone->nr_inactive_pages += delta;
		classzone++;
	}
	nr_inactive_pages += delta;
}

/**
 * delta_nr_cache_pages: alter the number of pages in the pagecache
 *
 * @page: the page which is being added/removed
 * @delta: +1 for addition, -1 for removal
 *
 * Called under pagecache_lock
 */
void delta_nr_cache_pages(struct page *page, long delta)
{
	pg_data_t *pgdat;
	zone_t *classzone, *overflow;

	classzone = page_zone(page);
	pgdat = classzone->zone_pgdat;
	overflow = pgdat->node_zones + pgdat->nr_zones;

	while (classzone < overflow) {
		classzone->nr_cache_pages += delta;
		classzone++;
	}
	page_cache_size += delta;
}

/*
 * Perform any setup for the swap system
 */
void __init swap_setup(void)
{
	unsigned long megs = num_physpages >> (20 - PAGE_SHIFT);

	/* Use a smaller cluster for small-memory machines */
	if (megs < 16)
		page_cluster = 2;
	else
		page_cluster = 3;
	/*
	 * Right now other parts of the system means that we
	 * _really_ don't want to cluster much more
	 */
}
