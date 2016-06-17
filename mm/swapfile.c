/*
 *  linux/mm/swapfile.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 */

#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/blkdev.h> /* for blk_size */
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/shm.h>

#include <asm/pgtable.h>

spinlock_t swaplock = SPIN_LOCK_UNLOCKED;
unsigned int nr_swapfiles;
int total_swap_pages;
static int swap_overflow;

static const char Bad_file[] = "Bad swap file entry ";
static const char Unused_file[] = "Unused swap file entry ";
static const char Bad_offset[] = "Bad swap offset entry ";
static const char Unused_offset[] = "Unused swap offset entry ";

struct swap_list_t swap_list = {-1, -1};

struct swap_info_struct swap_info[MAX_SWAPFILES];

#define SWAPFILE_CLUSTER 256

static inline int scan_swap_map(struct swap_info_struct *si)
{
	unsigned long offset;
	/* 
	 * We try to cluster swap pages by allocating them
	 * sequentially in swap.  Once we've allocated
	 * SWAPFILE_CLUSTER pages this way, however, we resort to
	 * first-free allocation, starting a new cluster.  This
	 * prevents us from scattering swap pages all over the entire
	 * swap partition, so that we reduce overall disk seek times
	 * between swap pages.  -- sct */
	if (si->cluster_nr) {
		while (si->cluster_next <= si->highest_bit) {
			offset = si->cluster_next++;
			if (si->swap_map[offset])
				continue;
			si->cluster_nr--;
			goto got_page;
		}
	}
	si->cluster_nr = SWAPFILE_CLUSTER;

	/* try to find an empty (even not aligned) cluster. */
	offset = si->lowest_bit;
 check_next_cluster:
	if (offset+SWAPFILE_CLUSTER-1 <= si->highest_bit)
	{
		int nr;
		for (nr = offset; nr < offset+SWAPFILE_CLUSTER; nr++)
			if (si->swap_map[nr])
			{
				offset = nr+1;
				goto check_next_cluster;
			}
		/* We found a completly empty cluster, so start
		 * using it.
		 */
		goto got_page;
	}
	/* No luck, so now go finegrined as usual. -Andrea */
	for (offset = si->lowest_bit; offset <= si->highest_bit ; offset++) {
		if (si->swap_map[offset])
			continue;
		si->lowest_bit = offset+1;
	got_page:
		if (offset == si->lowest_bit)
			si->lowest_bit++;
		if (offset == si->highest_bit)
			si->highest_bit--;
		if (si->lowest_bit > si->highest_bit) {
			si->lowest_bit = si->max;
			si->highest_bit = 0;
		}
		si->swap_map[offset] = 1;
		nr_swap_pages--;
		si->cluster_next = offset+1;
		return offset;
	}
	si->lowest_bit = si->max;
	si->highest_bit = 0;
	return 0;
}

swp_entry_t get_swap_page(void)
{
	struct swap_info_struct * p;
	unsigned long offset;
	swp_entry_t entry;
	int type, wrapped = 0;

	entry.val = 0;	/* Out of memory */
	swap_list_lock();
	type = swap_list.next;
	if (type < 0)
		goto out;
	if (nr_swap_pages <= 0)
		goto out;

	while (1) {
		p = &swap_info[type];
		if ((p->flags & SWP_WRITEOK) == SWP_WRITEOK) {
			swap_device_lock(p);
			offset = scan_swap_map(p);
			swap_device_unlock(p);
			if (offset) {
				entry = SWP_ENTRY(type,offset);
				type = swap_info[type].next;
				if (type < 0 ||
					p->prio != swap_info[type].prio) {
						swap_list.next = swap_list.head;
				} else {
					swap_list.next = type;
				}
				goto out;
			}
		}
		type = p->next;
		if (!wrapped) {
			if (type < 0 || p->prio != swap_info[type].prio) {
				type = swap_list.head;
				wrapped = 1;
			}
		} else
			if (type < 0)
				goto out;	/* out of swap space */
	}
out:
	swap_list_unlock();
	return entry;
}

static struct swap_info_struct * swap_info_get(swp_entry_t entry)
{
	struct swap_info_struct * p;
	unsigned long offset, type;

	if (!entry.val)
		goto out;
	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles)
		goto bad_nofile;
	p = & swap_info[type];
	if (!(p->flags & SWP_USED))
		goto bad_device;
	offset = SWP_OFFSET(entry);
	if (offset >= p->max)
		goto bad_offset;
	if (!p->swap_map[offset])
		goto bad_free;
	swap_list_lock();
	if (p->prio > swap_info[swap_list.next].prio)
		swap_list.next = type;
	swap_device_lock(p);
	return p;

bad_free:
	printk(KERN_ERR "swap_free: %s%08lx\n", Unused_offset, entry.val);
	goto out;
bad_offset:
	printk(KERN_ERR "swap_free: %s%08lx\n", Bad_offset, entry.val);
	goto out;
bad_device:
	printk(KERN_ERR "swap_free: %s%08lx\n", Unused_file, entry.val);
	goto out;
bad_nofile:
	printk(KERN_ERR "swap_free: %s%08lx\n", Bad_file, entry.val);
out:
	return NULL;
}	

static void swap_info_put(struct swap_info_struct * p)
{
	swap_device_unlock(p);
	swap_list_unlock();
}

static int swap_entry_free(struct swap_info_struct *p, unsigned long offset)
{
	int count = p->swap_map[offset];

	if (count < SWAP_MAP_MAX) {
		count--;
		p->swap_map[offset] = count;
		if (!count) {
			if (offset < p->lowest_bit)
				p->lowest_bit = offset;
			if (offset > p->highest_bit)
				p->highest_bit = offset;
			nr_swap_pages++;
		}
	}
	return count;
}

/*
 * Caller has made sure that the swapdevice corresponding to entry
 * is still around or has not been recycled.
 */
void swap_free(swp_entry_t entry)
{
	struct swap_info_struct * p;

	p = swap_info_get(entry);
	if (p) {
		swap_entry_free(p, SWP_OFFSET(entry));
		swap_info_put(p);
	}
}

/*
 * Check if we're the only user of a swap page,
 * when the page is locked.
 */
static int exclusive_swap_page(struct page *page)
{
	int retval = 0;
	struct swap_info_struct * p;
	swp_entry_t entry;

	entry.val = page->index;
	p = swap_info_get(entry);
	if (p) {
		/* Is the only swap cache user the cache itself? */
		if (p->swap_map[SWP_OFFSET(entry)] == 1) {
			/* Recheck the page count with the pagecache lock held.. */
			spin_lock(&pagecache_lock);
			if (page_count(page) - !!page->buffers == 2)
				retval = 1;
			spin_unlock(&pagecache_lock);
		}
		swap_info_put(p);
	}
	return retval;
}

/*
 * We can use this swap cache entry directly
 * if there are no other references to it.
 *
 * Here "exclusive_swap_page()" does the real
 * work, but we opportunistically check whether
 * we need to get all the locks first..
 */
int can_share_swap_page(struct page *page)
{
	int retval = 0;

	if (!PageLocked(page))
		BUG();
	switch (page_count(page)) {
	case 3:
		if (!page->buffers)
			break;
		/* Fallthrough */
	case 2:
		if (!PageSwapCache(page))
			break;
		retval = exclusive_swap_page(page);
		break;
	case 1:
		if (PageReserved(page))
			break;
		retval = 1;
	}
	return retval;
}

/*
 * Work out if there are any other processes sharing this
 * swap cache page. Free it if you can. Return success.
 */
int remove_exclusive_swap_page(struct page *page)
{
	int retval;
	struct swap_info_struct * p;
	swp_entry_t entry;

	if (!PageLocked(page))
		BUG();
	if (!PageSwapCache(page))
		return 0;
	if (page_count(page) - !!page->buffers != 2)	/* 2: us + cache */
		return 0;

	entry.val = page->index;
	p = swap_info_get(entry);
	if (!p)
		return 0;

	/* Is the only swap cache user the cache itself? */
	retval = 0;
	if (p->swap_map[SWP_OFFSET(entry)] == 1) {
		/* Recheck the page count with the pagecache lock held.. */
		spin_lock(&pagecache_lock);
		if (page_count(page) - !!page->buffers == 2) {
			__delete_from_swap_cache(page);
			SetPageDirty(page);
			retval = 1;
		}
		spin_unlock(&pagecache_lock);
	}
	swap_info_put(p);

	if (retval) {
		block_flushpage(page, 0);
		swap_free(entry);
		page_cache_release(page);
	}

	return retval;
}

/*
 * Free the swap entry like above, but also try to
 * free the page cache entry if it is the last user.
 */
void free_swap_and_cache(swp_entry_t entry)
{
	struct swap_info_struct * p;
	struct page *page = NULL;

	p = swap_info_get(entry);
	if (p) {
		if (swap_entry_free(p, SWP_OFFSET(entry)) == 1)
			page = find_trylock_page(&swapper_space, entry.val);
		swap_info_put(p);
	}
	if (page) {
		page_cache_get(page);
		/* Only cache user (+us), or swap space full? Free it! */
		if (page_count(page) - !!page->buffers == 2 || vm_swap_full()) {
			delete_from_swap_cache(page);
			SetPageDirty(page);
		}
		UnlockPage(page);
		page_cache_release(page);
	}
}

/*
 * The swap entry has been read in advance, and we return 1 to indicate
 * that the page has been used or is no longer needed.
 *
 * Always set the resulting pte to be nowrite (the same as COW pages
 * after one process has exited).  We don't know just how many PTEs will
 * share this swap entry, so be cautious and let do_wp_page work out
 * what to do if a write is requested later.
 */
/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pte(struct vm_area_struct * vma, unsigned long address,
	pte_t *dir, swp_entry_t entry, struct page* page)
{
	pte_t pte = *dir;

	if (likely(pte_to_swp_entry(pte).val != entry.val))
		return;
	if (unlikely(pte_none(pte) || pte_present(pte)))
		return;
	get_page(page);
	set_pte(dir, pte_mkold(mk_pte(page, vma->vm_page_prot)));
	swap_free(entry);
	++vma->vm_mm->rss;
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pmd(struct vm_area_struct * vma, pmd_t *dir,
	unsigned long address, unsigned long size, unsigned long offset,
	swp_entry_t entry, struct page* page)
{
	pte_t * pte;
	unsigned long end;

	if (pmd_none(*dir))
		return;
	if (pmd_bad(*dir)) {
		pmd_ERROR(*dir);
		pmd_clear(dir);
		return;
	}
	pte = pte_offset(dir, address);
	offset += address & PMD_MASK;
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		unuse_pte(vma, offset+address-vma->vm_start, pte, entry, page);
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static inline void unuse_pgd(struct vm_area_struct * vma, pgd_t *dir,
	unsigned long address, unsigned long size,
	swp_entry_t entry, struct page* page)
{
	pmd_t * pmd;
	unsigned long offset, end;

	if (pgd_none(*dir))
		return;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return;
	}
	pmd = pmd_offset(dir, address);
	offset = address & PGDIR_MASK;
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	if (address >= end)
		BUG();
	do {
		unuse_pmd(vma, pmd, address, end - address, offset, entry,
			  page);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
}

/* mmlist_lock and vma->vm_mm->page_table_lock are held */
static void unuse_vma(struct vm_area_struct * vma, pgd_t *pgdir,
			swp_entry_t entry, struct page* page)
{
	unsigned long start = vma->vm_start, end = vma->vm_end;

	if (start >= end)
		BUG();
	do {
		unuse_pgd(vma, pgdir, start, end - start, entry, page);
		start = (start + PGDIR_SIZE) & PGDIR_MASK;
		pgdir++;
	} while (start && (start < end));
}

static void unuse_process(struct mm_struct * mm,
			swp_entry_t entry, struct page* page)
{
	struct vm_area_struct* vma;

	/*
	 * Go through process' page directory.
	 */
	spin_lock(&mm->page_table_lock);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		pgd_t * pgd = pgd_offset(mm, vma->vm_start);
		unuse_vma(vma, pgd, entry, page);
	}
	spin_unlock(&mm->page_table_lock);
	return;
}

/*
 * Scan swap_map from current position to next entry still in use.
 * Recycle to start on reaching the end, returning 0 when empty.
 */
static int find_next_to_unuse(struct swap_info_struct *si, int prev)
{
	int max = si->max;
	int i = prev;
	int count;

	/*
	 * No need for swap_device_lock(si) here: we're just looking
	 * for whether an entry is in use, not modifying it; false
	 * hits are okay, and sys_swapoff() has already prevented new
	 * allocations from this area (while holding swap_list_lock()).
	 */
	for (;;) {
		if (++i >= max) {
			if (!prev) {
				i = 0;
				break;
			}
			/*
			 * No entries in use at top of swap_map,
			 * loop back to start and recheck there.
			 */
			max = prev + 1;
			prev = 0;
			i = 1;
		}
		count = si->swap_map[i];
		if (count && count != SWAP_MAP_BAD)
			break;
	}
	return i;
}

/*
 * We completely avoid races by reading each swap page in advance,
 * and then search for the process using it.  All the necessary
 * page table adjustments can then be made atomically.
 */
static int try_to_unuse(unsigned int type)
{
	struct swap_info_struct * si = &swap_info[type];
	struct mm_struct *start_mm;
	unsigned short *swap_map;
	unsigned short swcount;
	struct page *page;
	swp_entry_t entry;
	int i = 0;
	int retval = 0;
	int reset_overflow = 0;
	int shmem;

	/*
	 * When searching mms for an entry, a good strategy is to
	 * start at the first mm we freed the previous entry from
	 * (though actually we don't notice whether we or coincidence
	 * freed the entry).  Initialize this start_mm with a hold.
	 *
	 * A simpler strategy would be to start at the last mm we
	 * freed the previous entry from; but that would take less
	 * advantage of mmlist ordering (now preserved by swap_out()),
	 * which clusters forked address spaces together, most recent
	 * child immediately after parent.  If we race with dup_mmap(),
	 * we very much want to resolve parent before child, otherwise
	 * we may miss some entries: using last mm would invert that.
	 */
	start_mm = &init_mm;
	atomic_inc(&init_mm.mm_users);

	/*
	 * Keep on scanning until all entries have gone.  Usually,
	 * one pass through swap_map is enough, but not necessarily:
	 * mmput() removes mm from mmlist before exit_mmap() and its
	 * zap_page_range().  That's not too bad, those entries are
	 * on their way out, and handled faster there than here.
	 * do_munmap() behaves similarly, taking the range out of mm's
	 * vma list before zap_page_range().  But unfortunately, when
	 * unmapping a part of a vma, it takes the whole out first,
	 * then reinserts what's left after (might even reschedule if
	 * open() method called) - so swap entries may be invisible
	 * to swapoff for a while, then reappear - but that is rare.
	 */
	while ((i = find_next_to_unuse(si, i))) {
		/* 
		 * Get a page for the entry, using the existing swap
		 * cache page if there is one.  Otherwise, get a clean
		 * page and read the swap into it. 
		 */
		swap_map = &si->swap_map[i];
		entry = SWP_ENTRY(type, i);
		page = read_swap_cache_async(entry);
		if (!page) {
			/*
			 * Either swap_duplicate() failed because entry
			 * has been freed independently, and will not be
			 * reused since sys_swapoff() already disabled
			 * allocation from here, or alloc_page() failed.
			 */
			if (!*swap_map)
				continue;
			retval = -ENOMEM;
			break;
		}

		/*
		 * Don't hold on to start_mm if it looks like exiting.
		 */
		if (atomic_read(&start_mm->mm_users) == 1) {
			mmput(start_mm);
			start_mm = &init_mm;
			atomic_inc(&init_mm.mm_users);
		}

		/*
		 * Wait for and lock page.  When do_swap_page races with
		 * try_to_unuse, do_swap_page can handle the fault much
		 * faster than try_to_unuse can locate the entry.  This
		 * apparently redundant "wait_on_page" lets try_to_unuse
		 * defer to do_swap_page in such a case - in some tests,
		 * do_swap_page and try_to_unuse repeatedly compete.
		 */
		wait_on_page(page);
		lock_page(page);

		/*
		 * Remove all references to entry, without blocking.
		 * Whenever we reach init_mm, there's no address space
		 * to search, but use it as a reminder to search shmem.
		 */
		shmem = 0;
		swcount = *swap_map;
		if (swcount > 1) {
			flush_page_to_ram(page);
			if (start_mm == &init_mm)
				shmem = shmem_unuse(entry, page);
			else
				unuse_process(start_mm, entry, page);
		}
		if (*swap_map > 1) {
			int set_start_mm = (*swap_map >= swcount);
			struct list_head *p = &start_mm->mmlist;
			struct mm_struct *new_start_mm = start_mm;
			struct mm_struct *mm;

			spin_lock(&mmlist_lock);
			while (*swap_map > 1 &&
					(p = p->next) != &start_mm->mmlist) {
				mm = list_entry(p, struct mm_struct, mmlist);
				swcount = *swap_map;
				if (mm == &init_mm) {
					set_start_mm = 1;
					spin_unlock(&mmlist_lock);
					shmem = shmem_unuse(entry, page);
					spin_lock(&mmlist_lock);
				} else
					unuse_process(mm, entry, page);
				if (set_start_mm && *swap_map < swcount) {
					new_start_mm = mm;
					set_start_mm = 0;
				}
			}
			atomic_inc(&new_start_mm->mm_users);
			spin_unlock(&mmlist_lock);
			mmput(start_mm);
			start_mm = new_start_mm;
		}

		/*
		 * How could swap count reach 0x7fff when the maximum
		 * pid is 0x7fff, and there's no way to repeat a swap
		 * page within an mm (except in shmem, where it's the
		 * shared object which takes the reference count)?
		 * We believe SWAP_MAP_MAX cannot occur in Linux 2.4.
		 *
		 * If that's wrong, then we should worry more about
		 * exit_mmap() and do_munmap() cases described above:
		 * we might be resetting SWAP_MAP_MAX too early here.
		 * We know "Undead"s can happen, they're okay, so don't
		 * report them; but do report if we reset SWAP_MAP_MAX.
		 */
		if (*swap_map == SWAP_MAP_MAX) {
			swap_list_lock();
			swap_device_lock(si);
			nr_swap_pages++;
			*swap_map = 1;
			swap_device_unlock(si);
			swap_list_unlock();
			reset_overflow = 1;
		}

		/*
		 * If a reference remains (rare), we would like to leave
		 * the page in the swap cache; but try_to_swap_out could
		 * then re-duplicate the entry once we drop page lock,
		 * so we might loop indefinitely; also, that page could
		 * not be swapped out to other storage meanwhile.  So:
		 * delete from cache even if there's another reference,
		 * after ensuring that the data has been saved to disk -
		 * since if the reference remains (rarer), it will be
		 * read from disk into another page.  Splitting into two
		 * pages would be incorrect if swap supported "shared
		 * private" pages, but they are handled by tmpfs files.
		 *
		 * Note shmem_unuse already deleted swappage from cache,
		 * unless corresponding filepage found already in cache:
		 * in which case it left swappage in cache, lowered its
		 * swap count to pass quickly through the loops above,
		 * and now we must reincrement count to try again later.
		 */
		if ((*swap_map > 1) && PageDirty(page) && PageSwapCache(page)) {
			rw_swap_page(WRITE, page);
			lock_page(page);
		}
		if (PageSwapCache(page)) {
			if (shmem)
				swap_duplicate(entry);
			else
				delete_from_swap_cache(page);
		}

		/*
		 * So we could skip searching mms once swap count went
		 * to 1, we did not mark any present ptes as dirty: must
		 * mark page dirty so try_to_swap_out will preserve it.
		 */
		SetPageDirty(page);
		UnlockPage(page);
		page_cache_release(page);

		/*
		 * Make sure that we aren't completely killing
		 * interactive performance.  Interruptible check on
		 * signal_pending() would be nice, but changes the spec?
		 */
		if (current->need_resched)
			schedule();
	}

	mmput(start_mm);
	if (reset_overflow) {
		printk(KERN_WARNING "swapoff: cleared swap entry overflow\n");
		swap_overflow = 0;
	}
	return retval;
}

asmlinkage long sys_swapoff(const char * specialfile)
{
	struct swap_info_struct * p = NULL;
	unsigned short *swap_map;
	struct nameidata nd;
	int i, type, prev;
	int err;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = user_path_walk(specialfile, &nd);
	if (err)
		goto out;

	lock_kernel();
	prev = -1;
	swap_list_lock();
	for (type = swap_list.head; type >= 0; type = swap_info[type].next) {
		p = swap_info + type;
		if ((p->flags & SWP_WRITEOK) == SWP_WRITEOK) {
			if (p->swap_file == nd.dentry)
			  break;
		}
		prev = type;
	}
	err = -EINVAL;
	if (type < 0) {
		swap_list_unlock();
		goto out_dput;
	}

	if (prev < 0) {
		swap_list.head = p->next;
	} else {
		swap_info[prev].next = p->next;
	}
	if (type == swap_list.next) {
		/* just pick something that's safe... */
		swap_list.next = swap_list.head;
	}
	nr_swap_pages -= p->pages;
	total_swap_pages -= p->pages;
	p->flags = SWP_USED;
	swap_list_unlock();
	unlock_kernel();
	err = try_to_unuse(type);
	lock_kernel();
	if (err) {
		/* re-insert swap space back into swap_list */
		swap_list_lock();
		for (prev = -1, i = swap_list.head; i >= 0; prev = i, i = swap_info[i].next)
			if (p->prio >= swap_info[i].prio)
				break;
		p->next = i;
		if (prev < 0)
			swap_list.head = swap_list.next = p - swap_info;
		else
			swap_info[prev].next = p - swap_info;
		nr_swap_pages += p->pages;
		total_swap_pages += p->pages;
		p->flags = SWP_WRITEOK;
		swap_list_unlock();
		goto out_dput;
	}
	if (p->swap_device)
		blkdev_put(p->swap_file->d_inode->i_bdev, BDEV_SWAP);
	path_release(&nd);

	swap_list_lock();
	swap_device_lock(p);
	nd.mnt = p->swap_vfsmnt;
	nd.dentry = p->swap_file;
	p->swap_vfsmnt = NULL;
	p->swap_file = NULL;
	p->swap_device = 0;
	p->max = 0;
	swap_map = p->swap_map;
	p->swap_map = NULL;
	p->flags = 0;
	swap_device_unlock(p);
	swap_list_unlock();
	vfree(swap_map);
	err = 0;

out_dput:
	unlock_kernel();
	path_release(&nd);
out:
	return err;
}

int get_swaparea_info(char *buf)
{
	char * page = (char *) __get_free_page(GFP_KERNEL);
	struct swap_info_struct *ptr = swap_info;
	int i, j, len = 0, usedswap;

	if (!page)
		return -ENOMEM;

	len += sprintf(buf, "Filename\t\t\tType\t\tSize\tUsed\tPriority\n");
	for (i = 0 ; i < nr_swapfiles ; i++, ptr++) {
		if ((ptr->flags & SWP_USED) && ptr->swap_map) {
			char * path = d_path(ptr->swap_file, ptr->swap_vfsmnt,
						page, PAGE_SIZE);

			len += sprintf(buf + len, "%-31s ", path);

			if (!ptr->swap_device)
				len += sprintf(buf + len, "file\t\t");
			else
				len += sprintf(buf + len, "partition\t");

			usedswap = 0;
			for (j = 0; j < ptr->max; ++j)
				switch (ptr->swap_map[j]) {
					case SWAP_MAP_BAD:
					case 0:
						continue;
					default:
						usedswap++;
				}
			len += sprintf(buf + len, "%d\t%d\t%d\n", ptr->pages << (PAGE_SHIFT - 10), 
				usedswap << (PAGE_SHIFT - 10), ptr->prio);
		}
	}
	free_page((unsigned long) page);
	return len;
}

int is_swap_partition(kdev_t dev) {
	struct swap_info_struct *ptr = swap_info;
	int i;

	for (i = 0 ; i < nr_swapfiles ; i++, ptr++) {
		if (ptr->flags & SWP_USED)
			if (ptr->swap_device == dev)
				return 1;
	}
	return 0;
}

/*
 * Written 01/25/92 by Simmule Turner, heavily changed by Linus.
 *
 * The swapon system call
 */
asmlinkage long sys_swapon(const char * specialfile, int swap_flags)
{
	struct swap_info_struct * p;
	struct nameidata nd;
	struct inode * swap_inode;
	unsigned int type;
	int i, j, prev;
	int error;
	static int least_priority = 0;
	union swap_header *swap_header = 0;
	int swap_header_version;
	int nr_good_pages = 0;
	unsigned long maxpages = 1;
	int swapfilesize;
	struct block_device *bdev = NULL;
	unsigned short *swap_map;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	lock_kernel();
	swap_list_lock();
	p = swap_info;
	for (type = 0 ; type < nr_swapfiles ; type++,p++)
		if (!(p->flags & SWP_USED))
			break;
	error = -EPERM;
	if (type >= MAX_SWAPFILES) {
		swap_list_unlock();
		goto out;
	}
	if (type >= nr_swapfiles)
		nr_swapfiles = type+1;
	p->flags = SWP_USED;
	p->swap_file = NULL;
	p->swap_vfsmnt = NULL;
	p->swap_device = 0;
	p->swap_map = NULL;
	p->lowest_bit = 0;
	p->highest_bit = 0;
	p->cluster_nr = 0;
	p->sdev_lock = SPIN_LOCK_UNLOCKED;
	p->next = -1;
	if (swap_flags & SWAP_FLAG_PREFER) {
		p->prio =
		  (swap_flags & SWAP_FLAG_PRIO_MASK)>>SWAP_FLAG_PRIO_SHIFT;
	} else {
		p->prio = --least_priority;
	}
	swap_list_unlock();
	error = user_path_walk(specialfile, &nd);
	if (error)
		goto bad_swap_2;

	p->swap_file = nd.dentry;
	p->swap_vfsmnt = nd.mnt;
	swap_inode = nd.dentry->d_inode;
	error = -EINVAL;

	if (S_ISBLK(swap_inode->i_mode)) {
		kdev_t dev = swap_inode->i_rdev;
		struct block_device_operations *bdops;
		devfs_handle_t de;

		if (is_mounted(dev)) {
			error = -EBUSY;
			goto bad_swap_2;
		}

		p->swap_device = dev;
		set_blocksize(dev, PAGE_SIZE);
		
		bd_acquire(swap_inode);
		bdev = swap_inode->i_bdev;
		de = devfs_get_handle_from_inode(swap_inode);
		bdops = devfs_get_ops(de);  /*  Increments module use count  */
		if (bdops) bdev->bd_op = bdops;

		error = blkdev_get(bdev, FMODE_READ|FMODE_WRITE, 0, BDEV_SWAP);
		devfs_put_ops(de);/*Decrement module use count now we're safe*/
		if (error)
			goto bad_swap_2;
		set_blocksize(dev, PAGE_SIZE);
		error = -ENODEV;
		if (!dev || (blk_size[MAJOR(dev)] &&
		     !blk_size[MAJOR(dev)][MINOR(dev)]))
			goto bad_swap;
		swapfilesize = 0;
		if (blk_size[MAJOR(dev)])
			swapfilesize = blk_size[MAJOR(dev)][MINOR(dev)]
				>> (PAGE_SHIFT - 10);
	} else if (S_ISREG(swap_inode->i_mode))
		swapfilesize = swap_inode->i_size >> PAGE_SHIFT;
	else
		goto bad_swap;

	error = -EBUSY;
	for (i = 0 ; i < nr_swapfiles ; i++) {
		struct swap_info_struct *q = &swap_info[i];
		if (i == type || !q->swap_file)
			continue;
		if (swap_inode->i_mapping == q->swap_file->d_inode->i_mapping)
			goto bad_swap;
	}

	swap_header = (void *) __get_free_page(GFP_USER);
	if (!swap_header) {
		printk("Unable to start swapping: out of memory :-)\n");
		error = -ENOMEM;
		goto bad_swap;
	}

	lock_page(virt_to_page(swap_header));
	rw_swap_page_nolock(READ, SWP_ENTRY(type,0), (char *) swap_header);

	if (!memcmp("SWAP-SPACE",swap_header->magic.magic,10))
		swap_header_version = 1;
	else if (!memcmp("SWAPSPACE2",swap_header->magic.magic,10))
		swap_header_version = 2;
	else {
		printk("Unable to find swap-space signature\n");
		error = -EINVAL;
		goto bad_swap;
	}
	
	switch (swap_header_version) {
	case 1:
		memset(((char *) swap_header)+PAGE_SIZE-10,0,10);
		j = 0;
		p->lowest_bit = 0;
		p->highest_bit = 0;
		for (i = 1 ; i < 8*PAGE_SIZE ; i++) {
			if (test_bit(i,(char *) swap_header)) {
				if (!p->lowest_bit)
					p->lowest_bit = i;
				p->highest_bit = i;
				maxpages = i+1;
				j++;
			}
		}
		nr_good_pages = j;
		p->swap_map = vmalloc(maxpages * sizeof(short));
		if (!p->swap_map) {
			error = -ENOMEM;		
			goto bad_swap;
		}
		for (i = 1 ; i < maxpages ; i++) {
			if (test_bit(i,(char *) swap_header))
				p->swap_map[i] = 0;
			else
				p->swap_map[i] = SWAP_MAP_BAD;
		}
		break;

	case 2:
		/* Check the swap header's sub-version and the size of
                   the swap file and bad block lists */
		if (swap_header->info.version != 1) {
			printk(KERN_WARNING
			       "Unable to handle swap header version %d\n",
			       swap_header->info.version);
			error = -EINVAL;
			goto bad_swap;
		}

		p->lowest_bit  = 1;
		maxpages = SWP_OFFSET(SWP_ENTRY(0,~0UL)) - 1;
		if (maxpages > swap_header->info.last_page)
			maxpages = swap_header->info.last_page;
		p->highest_bit = maxpages - 1;

		error = -EINVAL;
		if (swap_header->info.nr_badpages > MAX_SWAP_BADPAGES)
			goto bad_swap;
		
		/* OK, set up the swap map and apply the bad block list */
		if (!(p->swap_map = vmalloc(maxpages * sizeof(short)))) {
			error = -ENOMEM;
			goto bad_swap;
		}

		error = 0;
		memset(p->swap_map, 0, maxpages * sizeof(short));
		for (i=0; i<swap_header->info.nr_badpages; i++) {
			int page = swap_header->info.badpages[i];
			if (page <= 0 || page >= swap_header->info.last_page)
				error = -EINVAL;
			else
				p->swap_map[page] = SWAP_MAP_BAD;
		}
		nr_good_pages = swap_header->info.last_page -
				swap_header->info.nr_badpages -
				1 /* header page */;
		if (error) 
			goto bad_swap;
	}
	
	if (swapfilesize && maxpages > swapfilesize) {
		printk(KERN_WARNING
		       "Swap area shorter than signature indicates\n");
		error = -EINVAL;
		goto bad_swap;
	}
	if (!nr_good_pages) {
		printk(KERN_WARNING "Empty swap-file\n");
		error = -EINVAL;
		goto bad_swap;
	}
	p->swap_map[0] = SWAP_MAP_BAD;
	swap_list_lock();
	swap_device_lock(p);
	p->max = maxpages;
	p->flags = SWP_WRITEOK;
	p->pages = nr_good_pages;
	nr_swap_pages += nr_good_pages;
	total_swap_pages += nr_good_pages;
	printk(KERN_INFO "Adding Swap: %dk swap-space (priority %d)\n",
	       nr_good_pages<<(PAGE_SHIFT-10), p->prio);

	/* insert swap space into swap_list: */
	prev = -1;
	for (i = swap_list.head; i >= 0; i = swap_info[i].next) {
		if (p->prio >= swap_info[i].prio) {
			break;
		}
		prev = i;
	}
	p->next = i;
	if (prev < 0) {
		swap_list.head = swap_list.next = p - swap_info;
	} else {
		swap_info[prev].next = p - swap_info;
	}
	swap_device_unlock(p);
	swap_list_unlock();
	error = 0;
	goto out;
bad_swap:
	if (bdev)
		blkdev_put(bdev, BDEV_SWAP);
bad_swap_2:
	swap_list_lock();
	swap_map = p->swap_map;
	nd.mnt = p->swap_vfsmnt;
	nd.dentry = p->swap_file;
	p->swap_device = 0;
	p->swap_file = NULL;
	p->swap_vfsmnt = NULL;
	p->swap_map = NULL;
	p->flags = 0;
	if (!(swap_flags & SWAP_FLAG_PREFER))
		++least_priority;
	swap_list_unlock();
	if (swap_map)
		vfree(swap_map);
	path_release(&nd);
out:
	if (swap_header)
		free_page((long) swap_header);
	unlock_kernel();
	return error;
}

void si_swapinfo(struct sysinfo *val)
{
	unsigned int i;
	unsigned long nr_to_be_unused = 0;

	swap_list_lock();
	for (i = 0; i < nr_swapfiles; i++) {
		unsigned int j;
		if (swap_info[i].flags != SWP_USED)
			continue;
		for (j = 0; j < swap_info[i].max; ++j) {
			switch (swap_info[i].swap_map[j]) {
				case 0:
				case SWAP_MAP_BAD:
					continue;
				default:
					nr_to_be_unused++;
			}
		}
	}
	val->freeswap = nr_swap_pages + nr_to_be_unused;
	val->totalswap = total_swap_pages + nr_to_be_unused;
	swap_list_unlock();
}

/*
 * Verify that a swap entry is valid and increment its swap map count.
 *
 * Note: if swap_map[] reaches SWAP_MAP_MAX the entries are treated as
 * "permanent", but will be reclaimed by the next swapoff.
 */
int swap_duplicate(swp_entry_t entry)
{
	struct swap_info_struct * p;
	unsigned long offset, type;
	int result = 0;

	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles)
		goto bad_file;
	p = type + swap_info;
	offset = SWP_OFFSET(entry);

	swap_device_lock(p);
	if (offset < p->max && p->swap_map[offset]) {
		if (p->swap_map[offset] < SWAP_MAP_MAX - 1) {
			p->swap_map[offset]++;
			result = 1;
		} else if (p->swap_map[offset] <= SWAP_MAP_MAX) {
			if (swap_overflow++ < 5)
				printk(KERN_WARNING "swap_dup: swap entry overflow\n");
			p->swap_map[offset] = SWAP_MAP_MAX;
			result = 1;
		}
	}
	swap_device_unlock(p);
out:
	return result;

bad_file:
	printk(KERN_ERR "swap_dup: %s%08lx\n", Bad_file, entry.val);
	goto out;
}

/*
 * Prior swap_duplicate protects against swap device deletion.
 */
void get_swaphandle_info(swp_entry_t entry, unsigned long *offset, 
			kdev_t *dev, struct inode **swapf)
{
	unsigned long type;
	struct swap_info_struct *p;

	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles) {
		printk(KERN_ERR "rw_swap_page: %s%08lx\n", Bad_file, entry.val);
		return;
	}

	p = &swap_info[type];
	*offset = SWP_OFFSET(entry);
	if (*offset >= p->max && *offset != 0) {
		printk(KERN_ERR "rw_swap_page: %s%08lx\n", Bad_offset, entry.val);
		return;
	}
	if (p->swap_map && !p->swap_map[*offset]) {
		printk(KERN_ERR "rw_swap_page: %s%08lx\n", Unused_offset, entry.val);
		return;
	}
	if (!(p->flags & SWP_USED)) {
		printk(KERN_ERR "rw_swap_page: %s%08lx\n", Unused_file, entry.val);
		return;
	}

	if (p->swap_device) {
		*dev = p->swap_device;
	} else if (p->swap_file) {
		*swapf = p->swap_file->d_inode;
	} else {
		printk(KERN_ERR "rw_swap_page: no swap file or device\n");
	}
	return;
}

/*
 * swap_device_lock prevents swap_map being freed. Don't grab an extra
 * reference on the swaphandle, it doesn't matter if it becomes unused.
 */
int valid_swaphandles(swp_entry_t entry, unsigned long *offset)
{
	int ret = 0, i = 1 << page_cluster;
	unsigned long toff;
	struct swap_info_struct *swapdev = SWP_TYPE(entry) + swap_info;

	if (!page_cluster)	/* no readahead */
		return 0;
	toff = (SWP_OFFSET(entry) >> page_cluster) << page_cluster;
	if (!toff)		/* first page is swap header */
		toff++, i--;
	*offset = toff;

	swap_device_lock(swapdev);
	do {
		/* Don't read-ahead past the end of the swap area */
		if (toff >= swapdev->max)
			break;
		/* Don't read in free or bad pages */
		if (!swapdev->swap_map[toff])
			break;
		if (swapdev->swap_map[toff] == SWAP_MAP_BAD)
			break;
		toff++;
		ret++;
	} while (--i);
	swap_device_unlock(swapdev);
	return ret;
}
