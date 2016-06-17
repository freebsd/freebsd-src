/*
 *  linux/mm/swap_state.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *
 *  Rewritten to use page cache, (C) 1998 Stephen Tweedie
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>

#include <asm/pgtable.h>

/*
 * We may have stale swap cache pages in memory: notice
 * them here and get rid of the unnecessary final write.
 */
static int swap_writepage(struct page *page)
{
	if (remove_exclusive_swap_page(page)) {
		UnlockPage(page);
		return 0;
	}
	rw_swap_page(WRITE, page);
	return 0;
}

static struct address_space_operations swap_aops = {
	writepage: swap_writepage,
	sync_page: block_sync_page,
};

struct address_space swapper_space = {
	LIST_HEAD_INIT(swapper_space.clean_pages),
	LIST_HEAD_INIT(swapper_space.dirty_pages),
	LIST_HEAD_INIT(swapper_space.locked_pages),
	0,				/* nrpages	*/
	&swap_aops,
};

#ifdef SWAP_CACHE_INFO
#define INC_CACHE_INFO(x)	(swap_cache_info.x++)

static struct {
	unsigned long add_total;
	unsigned long del_total;
	unsigned long find_success;
	unsigned long find_total;
	unsigned long noent_race;
	unsigned long exist_race;
} swap_cache_info;

void show_swap_cache_info(void)
{
	printk("Swap cache: add %lu, delete %lu, find %lu/%lu, race %lu+%lu\n",
		swap_cache_info.add_total, swap_cache_info.del_total,
		swap_cache_info.find_success, swap_cache_info.find_total,
		swap_cache_info.noent_race, swap_cache_info.exist_race);
}
#else
#define INC_CACHE_INFO(x)	do { } while (0)
#endif

int add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	if (page->mapping)
		BUG();
	if (!swap_duplicate(entry)) {
		INC_CACHE_INFO(noent_race);
		return -ENOENT;
	}
	if (add_to_page_cache_unique(page, &swapper_space, entry.val,
			page_hash(&swapper_space, entry.val)) != 0) {
		swap_free(entry);
		INC_CACHE_INFO(exist_race);
		return -EEXIST;
	}
	if (!PageLocked(page))
		BUG();
	if (!PageSwapCache(page))
		BUG();
	INC_CACHE_INFO(add_total);
	return 0;
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache.
 */
void __delete_from_swap_cache(struct page *page)
{
	if (!PageLocked(page))
		BUG();
	if (!PageSwapCache(page))
		BUG();
	ClearPageDirty(page);
	__remove_inode_page(page);
	INC_CACHE_INFO(del_total);
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache and locked.
 * It will never put the page into the free list,
 * the caller has a reference on the page.
 */
void delete_from_swap_cache(struct page *page)
{
	swp_entry_t entry;

	if (!PageLocked(page))
		BUG();

	if (unlikely(!block_flushpage(page, 0)))
		BUG();	/* an anonymous page cannot have page->buffers set */

	entry.val = page->index;

	spin_lock(&pagecache_lock);
	__delete_from_swap_cache(page);
	spin_unlock(&pagecache_lock);

	swap_free(entry);
	page_cache_release(page);
}

/* 
 * Perform a free_page(), also freeing any swap cache associated with
 * this page if it is the last user of the page. Can not do a lock_page,
 * as we are holding the page_table_lock spinlock.
 */
void free_page_and_swap_cache(struct page *page)
{
	/* 
	 * If we are the only user, then try to free up the swap cache. 
	 * 
	 * Its ok to check for PageSwapCache without the page lock
	 * here because we are going to recheck again inside 
	 * exclusive_swap_page() _with_ the lock. 
	 * 					- Marcelo
	 */
	if (PageSwapCache(page) && !TryLockPage(page)) {
		remove_exclusive_swap_page(page);
		UnlockPage(page);
	}
	page_cache_release(page);
}

/*
 * Lookup a swap entry in the swap cache. A found page will be returned
 * unlocked and with its refcount incremented - we rely on the kernel
 * lock getting page table operations atomic even if we drop the page
 * lock before returning.
 */
struct page * lookup_swap_cache(swp_entry_t entry)
{
	struct page *found;

	found = find_get_page(&swapper_space, entry.val);
	/*
	 * Unsafe to assert PageSwapCache and mapping on page found:
	 * if SMP nothing prevents swapoff from deleting this page from
	 * the swap cache at this moment.  find_lock_page would prevent
	 * that, but no need to change: we _have_ got the right page.
	 */
	INC_CACHE_INFO(find_total);
	if (found)
		INC_CACHE_INFO(find_success);
	return found;
}

/* 
 * Locate a page of swap in physical memory, reserving swap cache space
 * and reading the disk if it is not already cached.
 * A failure return means that either the page allocation failed or that
 * the swap entry is no longer in use.
 */
struct page * read_swap_cache_async(swp_entry_t entry)
{
	struct page *found_page, *new_page = NULL;
	int err;

	do {
		/*
		 * First check the swap cache.  Since this is normally
		 * called after lookup_swap_cache() failed, re-calling
		 * that would confuse statistics: use find_get_page()
		 * directly.
		 */
		found_page = find_get_page(&swapper_space, entry.val);
		if (found_page)
			break;

		/*
		 * Get a new page to read into from swap.
		 */
		if (!new_page) {
			new_page = alloc_page(GFP_HIGHUSER);
			if (!new_page)
				break;		/* Out of memory */
		}

		/*
		 * Associate the page with swap entry in the swap cache.
		 * May fail (-ENOENT) if swap entry has been freed since
		 * our caller observed it.  May fail (-EEXIST) if there
		 * is already a page associated with this entry in the
		 * swap cache: added by a racing read_swap_cache_async,
		 * or by try_to_swap_out (or shmem_writepage) re-using
		 * the just freed swap entry for an existing page.
		 */
		err = add_to_swap_cache(new_page, entry);
		if (!err) {
			/*
			 * Initiate read into locked page and return.
			 */
			rw_swap_page(READ, new_page);
			return new_page;
		}
	} while (err != -ENOENT);

	if (new_page)
		page_cache_release(new_page);
	return found_page;
}
