/*
 * Resizable virtual memory filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *		 2000 Transmeta Corp.
 *		 2000-2001 Christoph Rohland
 *		 2000-2001 SAP AG
 *		 2002 Red Hat Inc.
 * Copyright (C) 2002-2003 Hugh Dickins.
 * Copyright (C) 2002-2003 VERITAS Software Corporation.
 *
 * This file is released under the GPL.
 */

/*
 * This virtual memory filesystem is heavily based on the ramfs. It
 * extends ramfs by the ability to use swap and honor resource limits
 * which makes it a completely usable filesystem.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/div64.h>

/* This magic number is used in glibc for posix shared memory */
#define TMPFS_MAGIC	0x01021994

#define ENTRIES_PER_PAGE (PAGE_CACHE_SIZE/sizeof(unsigned long))
#define ENTRIES_PER_PAGEPAGE (ENTRIES_PER_PAGE*ENTRIES_PER_PAGE)
#define BLOCKS_PER_PAGE  (PAGE_CACHE_SIZE/512)

#define SHMEM_MAX_INDEX  (SHMEM_NR_DIRECT + (ENTRIES_PER_PAGEPAGE/2) * (ENTRIES_PER_PAGE+1))
#define SHMEM_MAX_BYTES  ((unsigned long long)SHMEM_MAX_INDEX << PAGE_CACHE_SHIFT)

#define VM_ACCT(size)    (PAGE_CACHE_ALIGN(size) >> PAGE_SHIFT)

/* info->flags needs VM_flags to handle pagein/truncate race efficiently */
#define SHMEM_PAGEIN	 VM_READ
#define SHMEM_TRUNCATE	 VM_WRITE

/* Pretend that each entry is of this size in directory's i_size */
#define BOGO_DIRENT_SIZE 20

#define SHMEM_SB(sb) (&sb->u.shmem_sb)

/* Flag allocation requirements to shmem_getpage and shmem_swp_alloc */
enum sgp_type {
	SGP_READ,	/* don't exceed i_size, don't allocate page */
	SGP_CACHE,	/* don't exceed i_size, may allocate page */
	SGP_WRITE,	/* may exceed i_size, may allocate page */
};

static int shmem_getpage(struct inode *inode, unsigned long idx,
			 struct page **pagep, enum sgp_type sgp);

static struct super_operations shmem_ops;
static struct address_space_operations shmem_aops;
static struct file_operations shmem_file_operations;
static struct inode_operations shmem_inode_operations;
static struct inode_operations shmem_dir_inode_operations;
static struct vm_operations_struct shmem_vm_ops;

LIST_HEAD(shmem_inodes);
static spinlock_t shmem_ilock = SPIN_LOCK_UNLOCKED;

static void shmem_free_block(struct inode *inode)
{
	struct shmem_sb_info *sbinfo = SHMEM_SB(inode->i_sb);
	spin_lock(&sbinfo->stat_lock);
	sbinfo->free_blocks++;
	inode->i_blocks -= BLOCKS_PER_PAGE;
	spin_unlock(&sbinfo->stat_lock);
}

static void shmem_removepage(struct page *page)
{
	if (!PageLaunder(page))
		shmem_free_block(page->mapping->host);
}

/*
 * shmem_swp_entry - find the swap vector position in the info structure
 *
 * @info:  info structure for the inode
 * @index: index of the page to find
 * @page:  optional page to add to the structure. Has to be preset to
 *         all zeros
 *
 * If there is no space allocated yet it will return NULL when
 * page is 0, else it will use the page for the needed block,
 * setting it to 0 on return to indicate that it has been used.
 *
 * The swap vector is organized the following way:
 *
 * There are SHMEM_NR_DIRECT entries directly stored in the
 * shmem_inode_info structure. So small files do not need an addional
 * allocation.
 *
 * For pages with index > SHMEM_NR_DIRECT there is the pointer
 * i_indirect which points to a page which holds in the first half
 * doubly indirect blocks, in the second half triple indirect blocks:
 *
 * For an artificial ENTRIES_PER_PAGE = 4 this would lead to the
 * following layout (for SHMEM_NR_DIRECT == 16):
 *
 * i_indirect -> dir --> 16-19
 * 	      |	     +-> 20-23
 * 	      |
 * 	      +-->dir2 --> 24-27
 * 	      |	       +-> 28-31
 * 	      |	       +-> 32-35
 * 	      |	       +-> 36-39
 * 	      |
 * 	      +-->dir3 --> 40-43
 * 	       	       +-> 44-47
 * 	      	       +-> 48-51
 * 	      	       +-> 52-55
 */
static swp_entry_t *shmem_swp_entry(struct shmem_inode_info *info, unsigned long index, unsigned long *page)
{
	unsigned long offset;
	void **dir;

	if (index < SHMEM_NR_DIRECT)
		return info->i_direct+index;
	if (!info->i_indirect) {
		if (page) {
			info->i_indirect = (void **) *page;
			*page = 0;
		}
		return NULL;			/* need another page */
	}

	index -= SHMEM_NR_DIRECT;
	offset = index % ENTRIES_PER_PAGE;
	index /= ENTRIES_PER_PAGE;
	dir = info->i_indirect;

	if (index >= ENTRIES_PER_PAGE/2) {
		index -= ENTRIES_PER_PAGE/2;
		dir += ENTRIES_PER_PAGE/2 + index/ENTRIES_PER_PAGE;
		index %= ENTRIES_PER_PAGE;
		if (!*dir) {
			if (page) {
				*dir = (void *) *page;
				*page = 0;
			}
			return NULL;		/* need another page */
		}
		dir = (void **) *dir;
	}

	dir += index;
	if (!*dir) {
		if (!page || !*page)
			return NULL;		/* need a page */
		*dir = (void *) *page;
		*page = 0;
	}
	return (swp_entry_t *) *dir + offset;
}

/*
 * shmem_swp_alloc - get the position of the swap entry for the page.
 *                   If it does not exist allocate the entry.
 *
 * @info:	info structure for the inode
 * @index:	index of the page to find
 * @sgp:	check and recheck i_size? skip allocation?
 */
static swp_entry_t *shmem_swp_alloc(struct shmem_inode_info *info, unsigned long index, enum sgp_type sgp)
{
	struct inode *inode = info->inode;
	struct shmem_sb_info *sbinfo = SHMEM_SB(inode->i_sb);
	unsigned long page = 0;
	swp_entry_t *entry;
	static const swp_entry_t unswapped = {0};

	if (sgp != SGP_WRITE &&
	    ((loff_t) index << PAGE_CACHE_SHIFT) >= inode->i_size)
		return ERR_PTR(-EINVAL);

	while (!(entry = shmem_swp_entry(info, index, &page))) {
		if (sgp == SGP_READ)
			return (swp_entry_t *) &unswapped;
		/*
		 * Test free_blocks against 1 not 0, since we have 1 data
		 * page (and perhaps indirect index pages) yet to allocate:
		 * a waste to allocate index if we cannot allocate data.
		 */
		spin_lock(&sbinfo->stat_lock);
		if (sbinfo->free_blocks <= 1) {
			spin_unlock(&sbinfo->stat_lock);
			return ERR_PTR(-ENOSPC);
		}
		sbinfo->free_blocks--;
		inode->i_blocks += BLOCKS_PER_PAGE;
		spin_unlock(&sbinfo->stat_lock);

		spin_unlock(&info->lock);
		page = get_zeroed_page(GFP_USER);
		spin_lock(&info->lock);

		if (!page) {
			shmem_free_block(inode);
			return ERR_PTR(-ENOMEM);
		}
		if (sgp != SGP_WRITE &&
		    ((loff_t) index << PAGE_CACHE_SHIFT) >= inode->i_size) {
			entry = ERR_PTR(-EINVAL);
			break;
		}
		if (info->next_index <= index)
			info->next_index = index + 1;
	}
	if (page) {
		/* another task gave its page, or truncated the file */
		shmem_free_block(inode);
		free_page(page);
	}
	if (info->next_index <= index && !IS_ERR(entry))
		info->next_index = index + 1;
	return entry;
}

/*
 * shmem_free_swp - free some swap entries in a directory
 *
 * @dir:   pointer to the directory
 * @edir:  pointer after last entry of the directory
 */
static int shmem_free_swp(swp_entry_t *dir, swp_entry_t *edir)
{
	swp_entry_t *ptr;
	int freed = 0;

	for (ptr = dir; ptr < edir; ptr++) {
		if (ptr->val) {
			free_swap_and_cache(*ptr);
			*ptr = (swp_entry_t){0};
			freed++;
		}
	}
	return freed;
}

/*
 * shmem_truncate_direct - free the swap entries of a whole doubly
 *                         indirect block
 *
 * @info:	the info structure of the inode
 * @dir:	pointer to the pointer to the block
 * @start:	offset to start from (in pages)
 * @len:	how many pages are stored in this block
 */
static inline unsigned long
shmem_truncate_direct(struct shmem_inode_info *info, swp_entry_t ***dir, unsigned long start, unsigned long len)
{
	swp_entry_t **last, **ptr;
	unsigned long off, freed_swp, freed = 0;

	last = *dir + (len + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
	off = start % ENTRIES_PER_PAGE;

	for (ptr = *dir + start/ENTRIES_PER_PAGE; ptr < last; ptr++, off = 0) {
		if (!*ptr)
			continue;

		if (info->swapped) {
			freed_swp = shmem_free_swp(*ptr + off,
						*ptr + ENTRIES_PER_PAGE);
			info->swapped -= freed_swp;
			freed += freed_swp;
		}

		if (!off) {
			freed++;
			free_page((unsigned long) *ptr);
			*ptr = 0;
		}
	}

	if (!start) {
		freed++;
		free_page((unsigned long) *dir);
		*dir = 0;
	}
	return freed;
}

/*
 * shmem_truncate_indirect - truncate an inode
 *
 * @info:  the info structure of the inode
 * @index: the index to truncate
 *
 * This function locates the last doubly indirect block and calls
 * then shmem_truncate_direct to do the real work
 */
static inline unsigned long
shmem_truncate_indirect(struct shmem_inode_info *info, unsigned long index)
{
	swp_entry_t ***base;
	unsigned long baseidx, start;
	unsigned long len = info->next_index;
	unsigned long freed;

	if (len <= SHMEM_NR_DIRECT) {
		info->next_index = index;
		if (!info->swapped)
			return 0;
		freed = shmem_free_swp(info->i_direct + index,
					info->i_direct + len);
		info->swapped -= freed;
		return freed;
	}

	if (len <= ENTRIES_PER_PAGEPAGE/2 + SHMEM_NR_DIRECT) {
		len -= SHMEM_NR_DIRECT;
		base = (swp_entry_t ***) &info->i_indirect;
		baseidx = SHMEM_NR_DIRECT;
	} else {
		len -= ENTRIES_PER_PAGEPAGE/2 + SHMEM_NR_DIRECT;
		BUG_ON(len > ENTRIES_PER_PAGEPAGE*ENTRIES_PER_PAGE/2);
		baseidx = len - 1;
		baseidx -= baseidx % ENTRIES_PER_PAGEPAGE;
		base = (swp_entry_t ***) info->i_indirect +
			ENTRIES_PER_PAGE/2 + baseidx/ENTRIES_PER_PAGEPAGE;
		len -= baseidx;
		baseidx += ENTRIES_PER_PAGEPAGE/2 + SHMEM_NR_DIRECT;
	}

	if (index > baseidx) {
		info->next_index = index;
		start = index - baseidx;
	} else {
		info->next_index = baseidx;
		start = 0;
	}
	return *base? shmem_truncate_direct(info, base, start, len): 0;
}

static void shmem_truncate(struct inode *inode)
{
	struct shmem_inode_info *info = SHMEM_I(inode);
	struct shmem_sb_info *sbinfo = SHMEM_SB(inode->i_sb);
	unsigned long freed = 0;
	unsigned long index;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	index = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	if (index >= info->next_index)
		return;

	spin_lock(&info->lock);
	while (index < info->next_index)
		freed += shmem_truncate_indirect(info, index);
	BUG_ON(info->swapped > info->next_index);

	if (inode->i_mapping->nrpages && (info->flags & SHMEM_PAGEIN)) {
		/*
		 * Call truncate_inode_pages again: racing shmem_unuse_inode
		 * may have swizzled a page in from swap since vmtruncate or
		 * generic_delete_inode did it, before we lowered next_index.
		 * Also, though shmem_getpage checks i_size before adding to
		 * cache, no recheck after: so fix the narrow window there too.
		 */
		info->flags |= SHMEM_TRUNCATE;
		spin_unlock(&info->lock);
		truncate_inode_pages(inode->i_mapping, inode->i_size);
		spin_lock(&info->lock);
		info->flags &= ~SHMEM_TRUNCATE;
	}

	spin_unlock(&info->lock);
	spin_lock(&sbinfo->stat_lock);
	sbinfo->free_blocks += freed;
	inode->i_blocks -= freed*BLOCKS_PER_PAGE;
	spin_unlock(&sbinfo->stat_lock);
}

static int shmem_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct page *page = NULL;
	int error;

	if (attr->ia_valid & ATTR_SIZE) {
		if (attr->ia_size < inode->i_size) {
			/*
			 * If truncating down to a partial page, then
			 * if that page is already allocated, hold it
			 * in memory until the truncation is over, so
			 * truncate_partial_page cannnot miss it were
			 * it assigned to swap.
			 */
			if (attr->ia_size & (PAGE_CACHE_SIZE-1)) {
				(void) shmem_getpage(inode,
					attr->ia_size>>PAGE_CACHE_SHIFT,
						&page, SGP_READ);
			}
			/*
			 * Reset SHMEM_PAGEIN flag so that shmem_truncate can
			 * detect if any pages might have been added to cache
			 * after truncate_inode_pages.  But we needn't bother
			 * if it's being fully truncated to zero-length: the
			 * nrpages check is efficient enough in that case.
			 */
			if (attr->ia_size) {
				struct shmem_inode_info *info = SHMEM_I(inode);
				spin_lock(&info->lock);
				info->flags &= ~SHMEM_PAGEIN;
				spin_unlock(&info->lock);
			}
		}
	}

	error = inode_change_ok(inode, attr);
	if (!error)
		error = inode_setattr(inode, attr);
	if (page)
		page_cache_release(page);
	return error;
}

static void shmem_delete_inode(struct inode *inode)
{
	struct shmem_sb_info *sbinfo = SHMEM_SB(inode->i_sb);
	struct shmem_inode_info *info = SHMEM_I(inode);

	if (inode->i_op->truncate == shmem_truncate) {
		spin_lock(&shmem_ilock);
		list_del(&info->list);
		spin_unlock(&shmem_ilock);
		inode->i_size = 0;
		shmem_truncate(inode);
	}
	BUG_ON(inode->i_blocks);
	spin_lock(&sbinfo->stat_lock);
	sbinfo->free_inodes++;
	spin_unlock(&sbinfo->stat_lock);
	clear_inode(inode);
}

static inline int shmem_find_swp(swp_entry_t entry, swp_entry_t *dir, swp_entry_t *edir)
{
	swp_entry_t *ptr;

	for (ptr = dir; ptr < edir; ptr++) {
		if (ptr->val == entry.val)
			return ptr - dir;
	}
	return -1;
}

static int shmem_unuse_inode(struct shmem_inode_info *info, swp_entry_t entry, struct page *page)
{
	struct inode *inode;
	struct address_space *mapping;
	swp_entry_t *ptr;
	unsigned long idx;
	int offset;

	idx = 0;
	ptr = info->i_direct;
	spin_lock(&info->lock);
	offset = info->next_index;
	if (offset > SHMEM_NR_DIRECT)
		offset = SHMEM_NR_DIRECT;
	offset = shmem_find_swp(entry, ptr, ptr + offset);
	if (offset >= 0)
		goto found;

	for (idx = SHMEM_NR_DIRECT; idx < info->next_index;
	     idx += ENTRIES_PER_PAGE) {
		ptr = shmem_swp_entry(info, idx, NULL);
		if (!ptr)
			continue;
		offset = info->next_index - idx;
		if (offset > ENTRIES_PER_PAGE)
			offset = ENTRIES_PER_PAGE;
		offset = shmem_find_swp(entry, ptr, ptr + offset);
		if (offset >= 0)
			goto found;
	}
	spin_unlock(&info->lock);
	return 0;
found:
	idx += offset;
	inode = info->inode;
	mapping = inode->i_mapping;
	delete_from_swap_cache(page);
	if (add_to_page_cache_unique(page,
			mapping, idx, page_hash(mapping, idx)) == 0) {
		info->flags |= SHMEM_PAGEIN;
		ptr[offset].val = 0;
		info->swapped--;
	} else if (add_to_swap_cache(page, entry) != 0)
		BUG();
	spin_unlock(&info->lock);
	SetPageUptodate(page);
	/*
	 * Decrement swap count even when the entry is left behind:
	 * try_to_unuse will skip over mms, then reincrement count.
	 */
	swap_free(entry);
	return 1;
}

/*
 * shmem_unuse() search for an eventually swapped out shmem page.
 */
int shmem_unuse(swp_entry_t entry, struct page *page)
{
	struct list_head *p;
	struct shmem_inode_info *info;
	int found = 0;

	spin_lock(&shmem_ilock);
	list_for_each(p, &shmem_inodes) {
		info = list_entry(p, struct shmem_inode_info, list);

		if (info->swapped && shmem_unuse_inode(info, entry, page)) {
			/* move head to start search for next from here */
			list_move_tail(&shmem_inodes, &info->list);
			found = 1;
			break;
		}
	}
	spin_unlock(&shmem_ilock);
	return found;
}

/*
 * Move the page from the page cache to the swap cache.
 */
static int shmem_writepage(struct page *page)
{
	struct shmem_inode_info *info;
	swp_entry_t *entry, swap;
	struct address_space *mapping;
	unsigned long index;
	struct inode *inode;

	BUG_ON(!PageLocked(page));
	if (!PageLaunder(page))
		goto fail;

	mapping = page->mapping;
	index = page->index;
	inode = mapping->host;
	info = SHMEM_I(inode);
	if (info->flags & VM_LOCKED)
		goto fail;
getswap:
	swap = get_swap_page();
	if (!swap.val)
		goto fail;

	spin_lock(&info->lock);
	if (index >= info->next_index) {
		BUG_ON(!(info->flags & SHMEM_TRUNCATE));
		spin_unlock(&info->lock);
		swap_free(swap);
		goto fail;
	}
	entry = shmem_swp_entry(info, index, NULL);
	BUG_ON(!entry);
	BUG_ON(entry->val);

	/* Remove it from the page cache */
	remove_inode_page(page);
	page_cache_release(page);

	/* Add it to the swap cache */
	if (add_to_swap_cache(page, swap) != 0) {
		/*
		 * Raced with "speculative" read_swap_cache_async.
		 * Add page back to page cache, unref swap, try again.
		 */
		add_to_page_cache_locked(page, mapping, index);
		info->flags |= SHMEM_PAGEIN;
		spin_unlock(&info->lock);
		swap_free(swap);
		goto getswap;
	}

	*entry = swap;
	info->swapped++;
	spin_unlock(&info->lock);
	SetPageUptodate(page);
	set_page_dirty(page);
	UnlockPage(page);
	return 0;
fail:
	return fail_writepage(page);
}

/*
 * shmem_getpage - either get the page from swap or allocate a new one
 *
 * If we allocate a new one we do not mark it dirty. That's up to the
 * vm. If we swap it in we mark it dirty since we also free the swap
 * entry since a page cannot live in both the swap and page cache
 */
static int shmem_getpage(struct inode *inode, unsigned long idx, struct page **pagep, enum sgp_type sgp)
{
	struct address_space *mapping = inode->i_mapping;
	struct shmem_inode_info *info = SHMEM_I(inode);
	struct shmem_sb_info *sbinfo;
	struct page *filepage = *pagep;
	struct page *swappage;
	swp_entry_t *entry;
	swp_entry_t swap;
	int error = 0;

	if (idx >= SHMEM_MAX_INDEX)
		return -EFBIG;
	/*
	 * Normally, filepage is NULL on entry, and either found
	 * uptodate immediately, or allocated and zeroed, or read
	 * in under swappage, which is then assigned to filepage.
	 * But shmem_readpage and shmem_prepare_write pass in a locked
	 * filepage, which may be found not uptodate by other callers
	 * too, and may need to be copied from the swappage read in.
	 */
repeat:
	if (!filepage)
		filepage = find_lock_page(mapping, idx);
	if (filepage && Page_Uptodate(filepage))
		goto done;

	spin_lock(&info->lock);
	entry = shmem_swp_alloc(info, idx, sgp);
	if (IS_ERR(entry)) {
		spin_unlock(&info->lock);
		error = PTR_ERR(entry);
		goto failed;
	}
	swap = *entry;

	if (swap.val) {
		/* Look it up and read it in.. */
		swappage = lookup_swap_cache(swap);
		if (!swappage) {
			spin_unlock(&info->lock);
			swapin_readahead(swap);
			swappage = read_swap_cache_async(swap);
			if (!swappage) {
				spin_lock(&info->lock);
				entry = shmem_swp_alloc(info, idx, sgp);
				if (IS_ERR(entry))
					error = PTR_ERR(entry);
				else if (entry->val == swap.val)
					error = -ENOMEM;
				spin_unlock(&info->lock);
				if (error)
					goto failed;
				goto repeat;
			}
			wait_on_page(swappage);
			page_cache_release(swappage);
			goto repeat;
		}

		/* We have to do this with page locked to prevent races */
		if (TryLockPage(swappage)) {
			spin_unlock(&info->lock);
			wait_on_page(swappage);
			page_cache_release(swappage);
			goto repeat;
		}
		if (!Page_Uptodate(swappage)) {
			spin_unlock(&info->lock);
			UnlockPage(swappage);
			page_cache_release(swappage);
			error = -EIO;
			goto failed;
		}

		delete_from_swap_cache(swappage);
		if (filepage) {
			entry->val = 0;
			info->swapped--;
			spin_unlock(&info->lock);
			flush_page_to_ram(swappage);
			copy_highpage(filepage, swappage);
			UnlockPage(swappage);
			page_cache_release(swappage);
			flush_dcache_page(filepage);
			SetPageUptodate(filepage);
			SetPageDirty(filepage);
			swap_free(swap);
		} else if (add_to_page_cache_unique(swappage,
			mapping, idx, page_hash(mapping, idx)) == 0) {
			info->flags |= SHMEM_PAGEIN;
			entry->val = 0;
			info->swapped--;
			spin_unlock(&info->lock);
			filepage = swappage;
			SetPageUptodate(filepage);
			SetPageDirty(filepage);
			swap_free(swap);
		} else {
			if (add_to_swap_cache(swappage, swap) != 0)
				BUG();
			spin_unlock(&info->lock);
			SetPageUptodate(swappage);
			SetPageDirty(swappage);
			UnlockPage(swappage);
			page_cache_release(swappage);
			goto repeat;
		}
	} else if (sgp == SGP_READ && !filepage) {
		filepage = find_get_page(mapping, idx);
		if (filepage &&
		    (!Page_Uptodate(filepage) || TryLockPage(filepage))) {
			spin_unlock(&info->lock);
			wait_on_page(filepage);
			page_cache_release(filepage);
			filepage = NULL;
			goto repeat;
		}
		spin_unlock(&info->lock);
	} else {
		sbinfo = SHMEM_SB(inode->i_sb);
		spin_lock(&sbinfo->stat_lock);
		if (sbinfo->free_blocks == 0) {
			spin_unlock(&sbinfo->stat_lock);
			spin_unlock(&info->lock);
			error = -ENOSPC;
			goto failed;
		}
		sbinfo->free_blocks--;
		inode->i_blocks += BLOCKS_PER_PAGE;
		spin_unlock(&sbinfo->stat_lock);

		if (!filepage) {
			spin_unlock(&info->lock);
			filepage = page_cache_alloc(mapping);
			if (!filepage) {
				shmem_free_block(inode);
				error = -ENOMEM;
				goto failed;
			}

			spin_lock(&info->lock);
			entry = shmem_swp_alloc(info, idx, sgp);
			if (IS_ERR(entry))
				error = PTR_ERR(entry);
			if (error || entry->val ||
			    add_to_page_cache_unique(filepage,
			    mapping, idx, page_hash(mapping, idx)) != 0) {
				spin_unlock(&info->lock);
				page_cache_release(filepage);
				shmem_free_block(inode);
				filepage = NULL;
				if (error)
					goto failed;
				goto repeat;
			}
			info->flags |= SHMEM_PAGEIN;
		}

		spin_unlock(&info->lock);
		clear_highpage(filepage);
		flush_dcache_page(filepage);
		SetPageUptodate(filepage);
	}
done:
	if (!*pagep) {
		if (filepage) {
			UnlockPage(filepage);
			*pagep = filepage;
		} else
			*pagep = ZERO_PAGE(0);
	}
	return 0;

failed:
	if (*pagep != filepage) {
		UnlockPage(filepage);
		page_cache_release(filepage);
	}
	return error;
}

struct page *shmem_nopage(struct vm_area_struct *vma, unsigned long address, int unused)
{
	struct inode *inode = vma->vm_file->f_dentry->d_inode;
	struct page *page = NULL;
	unsigned long idx;
	int error;

	idx = (address - vma->vm_start) >> PAGE_SHIFT;
	idx += vma->vm_pgoff;
	idx >>= PAGE_CACHE_SHIFT - PAGE_SHIFT;

	error = shmem_getpage(inode, idx, &page, SGP_CACHE);
	if (error)
		return (error == -ENOMEM)? NOPAGE_OOM: NOPAGE_SIGBUS;

	mark_page_accessed(page);
	flush_page_to_ram(page);
	return page;
}

void shmem_lock(struct file *file, int lock)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct shmem_inode_info *info = SHMEM_I(inode);

	spin_lock(&info->lock);
	if (lock)
		info->flags |= VM_LOCKED;
	else
		info->flags &= ~VM_LOCKED;
	spin_unlock(&info->lock);
}

static int shmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vm_operations_struct *ops;
	struct inode *inode = file->f_dentry->d_inode;

	ops = &shmem_vm_ops;
	if (!S_ISREG(inode->i_mode))
		return -EACCES;
	UPDATE_ATIME(inode);
	vma->vm_ops = ops;
	return 0;
}

static struct inode *shmem_get_inode(struct super_block *sb, int mode, int dev)
{
	struct inode *inode;
	struct shmem_inode_info *info;
	struct shmem_sb_info *sbinfo = SHMEM_SB(sb);

	spin_lock(&sbinfo->stat_lock);
	if (!sbinfo->free_inodes) {
		spin_unlock(&sbinfo->stat_lock);
		return NULL;
	}
	sbinfo->free_inodes--;
	spin_unlock(&sbinfo->stat_lock);

	inode = new_inode(sb);
	if (inode) {
		inode->i_mode = mode;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_rdev = NODEV;
		inode->i_mapping->a_ops = &shmem_aops;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		info = SHMEM_I(inode);
		info->inode = inode;
		spin_lock_init(&info->lock);
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &shmem_inode_operations;
			inode->i_fop = &shmem_file_operations;
			spin_lock(&shmem_ilock);
			list_add_tail(&info->list, &shmem_inodes);
			spin_unlock(&shmem_ilock);
			break;
		case S_IFDIR:
			inode->i_nlink++;
			/* Some things misbehave if size == 0 on a directory */
			inode->i_size = 2 * BOGO_DIRENT_SIZE;
			inode->i_op = &shmem_dir_inode_operations;
			inode->i_fop = &dcache_dir_ops;
			break;
		case S_IFLNK:
			break;
		}
	}
	return inode;
}

static int shmem_set_size(struct shmem_sb_info *info,
			  unsigned long max_blocks, unsigned long max_inodes)
{
	int error;
	unsigned long blocks, inodes;

	spin_lock(&info->stat_lock);
	blocks = info->max_blocks - info->free_blocks;
	inodes = info->max_inodes - info->free_inodes;
	error = -EINVAL;
	if (max_blocks < blocks)
		goto out;
	if (max_inodes < inodes)
		goto out;
	error = 0;
	info->max_blocks  = max_blocks;
	info->free_blocks = max_blocks - blocks;
	info->max_inodes  = max_inodes;
	info->free_inodes = max_inodes - inodes;
out:
	spin_unlock(&info->stat_lock);
	return error;
}

#ifdef CONFIG_TMPFS

static struct inode_operations shmem_symlink_inode_operations;
static struct inode_operations shmem_symlink_inline_operations;

/*
 * tmpfs itself makes no use of generic_file_read, generic_file_mmap
 * or generic_file_write; but shmem_readpage, shmem_prepare_write and
 * shmem_commit_write let a tmpfs file be used below the loop driver,
 * and shmem_readpage lets a tmpfs file be used by sendfile.
 */
static int
shmem_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	int error = shmem_getpage(inode, page->index, &page, SGP_CACHE);
	UnlockPage(page);
	return error;
}

static int
shmem_prepare_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	struct inode *inode = page->mapping->host;
	return shmem_getpage(inode, page->index, &page, SGP_WRITE);
}

static int
shmem_commit_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	struct inode *inode = page->mapping->host;
	loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;

	if (pos > inode->i_size)
		inode->i_size = pos;
	SetPageDirty(page);
	return 0;
}

static ssize_t
shmem_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct inode	*inode = file->f_dentry->d_inode;
	loff_t		pos;
	unsigned long	written;
	int		err;

	if ((ssize_t) count < 0)
		return -EINVAL;

	if (!access_ok(VERIFY_READ, buf, count))
		return -EFAULT;

	down(&inode->i_sem);

	pos = *ppos;
	written = 0;

	err = precheck_file_write(file, inode, &count, &pos);
	if (err || !count)
		goto out;

	remove_suid(inode);
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;

	do {
		struct page *page = NULL;
		unsigned long bytes, index, offset;
		char *kaddr;
		int left;

		offset = (pos & (PAGE_CACHE_SIZE -1)); /* Within page */
		index = pos >> PAGE_CACHE_SHIFT;
		bytes = PAGE_CACHE_SIZE - offset;
		if (bytes > count)
			bytes = count;

		/*
		 * We don't hold page lock across copy from user -
		 * what would it guard against? - so no deadlock here.
		 */

		err = shmem_getpage(inode, index, &page, SGP_WRITE);
		if (err)
			break;

		kaddr = kmap(page);
		left = __copy_from_user(kaddr + offset, buf, bytes);
		kunmap(page);

		written += bytes;
		count -= bytes;
		pos += bytes;
		buf += bytes;
		if (pos > inode->i_size)
			inode->i_size = pos;

		flush_dcache_page(page);
		SetPageDirty(page);
		SetPageReferenced(page);
		page_cache_release(page);

		if (left) {
			pos -= left;
			written -= left;
			err = -EFAULT;
			break;
		}
	} while (count);

	*ppos = pos;
	if (written)
		err = written;
out:
	up(&inode->i_sem);
	return err;
}

static void do_shmem_file_read(struct file *filp, loff_t *ppos, read_descriptor_t *desc)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct address_space *mapping = inode->i_mapping;
	unsigned long index, offset;

	index = *ppos >> PAGE_CACHE_SHIFT;
	offset = *ppos & ~PAGE_CACHE_MASK;

	for (;;) {
		struct page *page = NULL;
		unsigned long end_index, nr, ret;

		end_index = inode->i_size >> PAGE_CACHE_SHIFT;
		if (index > end_index)
			break;
		if (index == end_index) {
			nr = inode->i_size & ~PAGE_CACHE_MASK;
			if (nr <= offset)
				break;
		}

		desc->error = shmem_getpage(inode, index, &page, SGP_READ);
		if (desc->error) {
			if (desc->error == -EINVAL)
				desc->error = 0;
			break;
		}

		/*
		 * We must evaluate after, since reads (unlike writes)
		 * are called without i_sem protection against truncate
		 */
		nr = PAGE_CACHE_SIZE;
		end_index = inode->i_size >> PAGE_CACHE_SHIFT;
		if (index == end_index) {
			nr = inode->i_size & ~PAGE_CACHE_MASK;
			if (nr <= offset) {
				page_cache_release(page);
				break;
			}
		}
		nr -= offset;

		if (page != ZERO_PAGE(0)) {
			/*
			 * If users can be writing to this page using arbitrary
			 * virtual addresses, take care about potential aliasing
			 * before reading the page on the kernel side.
			 */
			if (mapping->i_mmap_shared != NULL)
				flush_dcache_page(page);
			/*
			 * Mark the page accessed if we read the
			 * beginning or we just did an lseek.
			 */
			if (!offset || !filp->f_reada)
				mark_page_accessed(page);
		}

		/*
		 * Ok, we have the page, and it's up-to-date, so
		 * now we can copy it to user space...
		 *
		 * The actor routine returns how many bytes were actually used..
		 * NOTE! This may not be the same as how much of a user buffer
		 * we filled up (we may be padding etc), so we can only update
		 * "pos" here (the actor routine has to update the user buffer
		 * pointers and the remaining count).
		 */
		ret = file_read_actor(desc, page, offset, nr);
		offset += ret;
		index += offset >> PAGE_CACHE_SHIFT;
		offset &= ~PAGE_CACHE_MASK;

		page_cache_release(page);
		if (ret != nr || !desc->count)
			break;
	}

	*ppos = ((loff_t) index << PAGE_CACHE_SHIFT) + offset;
	filp->f_reada = 1;
	UPDATE_ATIME(inode);
}

static ssize_t shmem_file_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	read_descriptor_t desc;

	if ((ssize_t) count < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EFAULT;
	if (!count)
		return 0;

	desc.written = 0;
	desc.count = count;
	desc.buf = buf;
	desc.error = 0;

	do_shmem_file_read(filp, ppos, &desc);
	if (desc.written)
		return desc.written;
	return desc.error;
}

static int shmem_statfs(struct super_block *sb, struct statfs *buf)
{
	struct shmem_sb_info *sbinfo = SHMEM_SB(sb);

	buf->f_type = TMPFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	spin_lock(&sbinfo->stat_lock);
	buf->f_blocks = sbinfo->max_blocks;
	buf->f_bavail = buf->f_bfree = sbinfo->free_blocks;
	buf->f_files = sbinfo->max_inodes;
	buf->f_ffree = sbinfo->free_inodes;
	spin_unlock(&sbinfo->stat_lock);
	buf->f_namelen = NAME_MAX;
	return 0;
}

/*
 * Lookup the data. This is trivial - if the dentry didn't already
 * exist, we know it is negative.
 */
static struct dentry *shmem_lookup(struct inode *dir, struct dentry *dentry)
{
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	d_add(dentry, NULL);
	return NULL;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
static int shmem_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev)
{
	struct inode *inode = shmem_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
		dir->i_size += BOGO_DIRENT_SIZE;
		dir->i_ctime = dir->i_mtime = CURRENT_TIME;
		d_instantiate(dentry, inode);
		dget(dentry); /* Extra count - pin the dentry in core */
		error = 0;
	}
	return error;
}

static int shmem_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int error;

	if ((error = shmem_mknod(dir, dentry, mode | S_IFDIR, 0)))
		return error;
	dir->i_nlink++;
	return 0;
}

static int shmem_create(struct inode *dir, struct dentry *dentry, int mode)
{
	return shmem_mknod(dir, dentry, mode | S_IFREG, 0);
}

/*
 * Link a file..
 */
static int shmem_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	dir->i_size += BOGO_DIRENT_SIZE;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	inode->i_nlink++;
	atomic_inc(&inode->i_count);	/* New dentry reference */
	dget(dentry);		/* Extra pinning count for the created dentry */
	d_instantiate(dentry, inode);
	return 0;
}

static inline int shmem_positive(struct dentry *dentry)
{
	return dentry->d_inode && !d_unhashed(dentry);
}

/*
 * Check that a directory is empty (this works
 * for regular files too, they'll just always be
 * considered empty..).
 *
 * Note that an empty directory can still have
 * children, they just all have to be negative..
 */
static int shmem_empty(struct dentry *dentry)
{
	struct list_head *list;

	spin_lock(&dcache_lock);
	list = dentry->d_subdirs.next;

	while (list != &dentry->d_subdirs) {
		struct dentry *de = list_entry(list, struct dentry, d_child);

		if (shmem_positive(de)) {
			spin_unlock(&dcache_lock);
			return 0;
		}
		list = list->next;
	}
	spin_unlock(&dcache_lock);
	return 1;
}

static int shmem_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	dir->i_size -= BOGO_DIRENT_SIZE;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	inode->i_nlink--;
	dput(dentry);	/* Undo the count from "create" - this does all the work */
	return 0;
}

static int shmem_rmdir(struct inode *dir, struct dentry *dentry)
{
	if (!shmem_empty(dentry))
		return -ENOTEMPTY;

	dir->i_nlink--;
	return shmem_unlink(dir, dentry);
}

/*
 * The VFS layer already does all the dentry stuff for rename,
 * we just have to decrement the usage count for the target if
 * it exists so that the VFS layer correctly free's it when it
 * gets overwritten.
 */
static int shmem_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *inode = old_dentry->d_inode;
	int they_are_dirs = S_ISDIR(inode->i_mode);

	if (!shmem_empty(new_dentry))
		return -ENOTEMPTY;

	if (new_dentry->d_inode) {
		(void) shmem_unlink(new_dir, new_dentry);
		if (they_are_dirs)
			old_dir->i_nlink--;
	} else if (they_are_dirs) {
		old_dir->i_nlink--;
		new_dir->i_nlink++;
	}

	old_dir->i_size -= BOGO_DIRENT_SIZE;
	new_dir->i_size += BOGO_DIRENT_SIZE;
	old_dir->i_ctime = old_dir->i_mtime =
	new_dir->i_ctime = new_dir->i_mtime =
	inode->i_ctime = CURRENT_TIME;
	return 0;
}

static int shmem_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	int error;
	int len;
	struct inode *inode;
	struct page *page = NULL;
	char *kaddr;
	struct shmem_inode_info *info;

	len = strlen(symname) + 1;
	if (len > PAGE_CACHE_SIZE)
		return -ENAMETOOLONG;

	inode = shmem_get_inode(dir->i_sb, S_IFLNK|S_IRWXUGO, 0);
	if (!inode)
		return -ENOSPC;

	info = SHMEM_I(inode);
	inode->i_size = len-1;
	if (len <= sizeof(struct shmem_inode_info)) {
		/* do it inline */
		memcpy(info, symname, len);
		inode->i_op = &shmem_symlink_inline_operations;
	} else {
		error = shmem_getpage(inode, 0, &page, SGP_WRITE);
		if (error) {
			iput(inode);
			return error;
		}
		inode->i_op = &shmem_symlink_inode_operations;
		spin_lock(&shmem_ilock);
		list_add_tail(&info->list, &shmem_inodes);
		spin_unlock(&shmem_ilock);
		kaddr = kmap(page);
		memcpy(kaddr, symname, len);
		kunmap(page);
		SetPageDirty(page);
		page_cache_release(page);
	}
	if (dir->i_mode & S_ISGID)
		inode->i_gid = dir->i_gid;
	dir->i_size += BOGO_DIRENT_SIZE;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	d_instantiate(dentry, inode);
	dget(dentry);
	return 0;
}

static int shmem_readlink_inline(struct dentry *dentry, char *buffer, int buflen)
{
	return vfs_readlink(dentry, buffer, buflen, (const char *)SHMEM_I(dentry->d_inode));
}

static int shmem_follow_link_inline(struct dentry *dentry, struct nameidata *nd)
{
	return vfs_follow_link(nd, (const char *)SHMEM_I(dentry->d_inode));
}

static int shmem_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	struct page *page = NULL;
	int res = shmem_getpage(dentry->d_inode, 0, &page, SGP_READ);
	if (res)
		return res;
	res = vfs_readlink(dentry, buffer, buflen, kmap(page));
	kunmap(page);
	mark_page_accessed(page);
	page_cache_release(page);
	return res;
}

static int shmem_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct page *page = NULL;
	int res = shmem_getpage(dentry->d_inode, 0, &page, SGP_READ);
	if (res)
		return res;
	res = vfs_follow_link(nd, kmap(page));
	kunmap(page);
	mark_page_accessed(page);
	page_cache_release(page);
	return res;
}

static struct inode_operations shmem_symlink_inline_operations = {
	readlink:	shmem_readlink_inline,
	follow_link:	shmem_follow_link_inline,
};

static struct inode_operations shmem_symlink_inode_operations = {
	truncate:	shmem_truncate,
	readlink:	shmem_readlink,
	follow_link:	shmem_follow_link,
};

static int shmem_parse_options(char *options, int *mode, uid_t *uid, gid_t *gid, unsigned long *blocks, unsigned long *inodes)
{
	char *this_char, *value, *rest;

	while ((this_char = strsep(&options, ",")) != NULL) {
		if (!*this_char)
			continue;
		if ((value = strchr(this_char,'=')) != NULL) {
			*value++ = 0;
		} else {
			printk(KERN_ERR
			    "tmpfs: No value for mount option '%s'\n",
			    this_char);
			return 1;
		}

		if (!strcmp(this_char,"size")) {
			unsigned long long size;
			size = memparse(value,&rest);
			if (*rest == '%') {
				struct sysinfo si;
				si_meminfo(&si);
				size <<= PAGE_SHIFT;
				size *= si.totalram;
				do_div(size, 100);
				rest++;
			}
			if (*rest)
				goto bad_val;
			*blocks = size >> PAGE_CACHE_SHIFT;
		} else if (!strcmp(this_char,"nr_blocks")) {
			*blocks = memparse(value,&rest);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"nr_inodes")) {
			*inodes = memparse(value,&rest);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"mode")) {
			if (!mode)
				continue;
			*mode = simple_strtoul(value,&rest,8);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"uid")) {
			if (!uid)
				continue;
			*uid = simple_strtoul(value,&rest,0);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"gid")) {
			if (!gid)
				continue;
			*gid = simple_strtoul(value,&rest,0);
			if (*rest)
				goto bad_val;
		} else {
			printk(KERN_ERR "tmpfs: Bad mount option %s\n",
			       this_char);
			return 1;
		}
	}
	return 0;

bad_val:
	printk(KERN_ERR "tmpfs: Bad value '%s' for mount option '%s'\n",
	       value, this_char);
	return 1;
}

static int shmem_remount_fs(struct super_block *sb, int *flags, char *data)
{
	struct shmem_sb_info *sbinfo = SHMEM_SB(sb);
	unsigned long max_blocks = sbinfo->max_blocks;
	unsigned long max_inodes = sbinfo->max_inodes;

	if (shmem_parse_options(data, NULL, NULL, NULL, &max_blocks, &max_inodes))
		return -EINVAL;
	return shmem_set_size(sbinfo, max_blocks, max_inodes);
}

static int shmem_sync_file(struct file *file, struct dentry *dentry, int datasync)
{
	return 0;
}
#endif

static struct super_block *shmem_read_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;
	unsigned long blocks, inodes;
	int mode   = S_IRWXUGO | S_ISVTX;
	uid_t uid = current->fsuid;
	gid_t gid = current->fsgid;
	struct shmem_sb_info *sbinfo = SHMEM_SB(sb);
	struct sysinfo si;

	/*
	 * Per default we only allow half of the physical ram per
	 * tmpfs instance
	 */
	si_meminfo(&si);
	blocks = inodes = si.totalram / 2;

#ifdef CONFIG_TMPFS
	if (shmem_parse_options(data, &mode, &uid, &gid, &blocks, &inodes))
		return NULL;
#endif

	spin_lock_init(&sbinfo->stat_lock);
	sbinfo->max_blocks = blocks;
	sbinfo->free_blocks = blocks;
	sbinfo->max_inodes = inodes;
	sbinfo->free_inodes = inodes;
	sb->s_maxbytes = SHMEM_MAX_BYTES;
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = TMPFS_MAGIC;
	sb->s_op = &shmem_ops;
	inode = shmem_get_inode(sb, S_IFDIR | mode, 0);
	if (!inode)
		return NULL;

	inode->i_uid = uid;
	inode->i_gid = gid;
	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return NULL;
	}
	sb->s_root = root;
	return sb;
}

static struct address_space_operations shmem_aops = {
	removepage:	shmem_removepage,
	writepage:	shmem_writepage,
#ifdef CONFIG_TMPFS
	readpage:	shmem_readpage,
	prepare_write:	shmem_prepare_write,
	commit_write:	shmem_commit_write,
#endif
};

static struct file_operations shmem_file_operations = {
	mmap:		shmem_mmap,
#ifdef CONFIG_TMPFS
	read:		shmem_file_read,
	write:		shmem_file_write,
	fsync:		shmem_sync_file,
#endif
};

static struct inode_operations shmem_inode_operations = {
	truncate:	shmem_truncate,
	setattr:	shmem_notify_change,
};

static struct inode_operations shmem_dir_inode_operations = {
#ifdef CONFIG_TMPFS
	create:		shmem_create,
	lookup:		shmem_lookup,
	link:		shmem_link,
	unlink:		shmem_unlink,
	symlink:	shmem_symlink,
	mkdir:		shmem_mkdir,
	rmdir:		shmem_rmdir,
	mknod:		shmem_mknod,
	rename:		shmem_rename,
#endif
};

static struct super_operations shmem_ops = {
#ifdef CONFIG_TMPFS
	statfs:		shmem_statfs,
	remount_fs:	shmem_remount_fs,
#endif
	delete_inode:	shmem_delete_inode,
	put_inode:	force_delete,
};

static struct vm_operations_struct shmem_vm_ops = {
	nopage:		shmem_nopage,
};

#ifdef CONFIG_TMPFS
/* type "shm" will be tagged obsolete in 2.5 */
static DECLARE_FSTYPE(shmem_fs_type, "shm", shmem_read_super, FS_LITTER);
static DECLARE_FSTYPE(tmpfs_fs_type, "tmpfs", shmem_read_super, FS_LITTER);
#else
static DECLARE_FSTYPE(tmpfs_fs_type, "tmpfs", shmem_read_super, FS_LITTER|FS_NOMOUNT);
#endif
static struct vfsmount *shm_mnt;

static int __init init_tmpfs(void)
{
	int error;

	error = register_filesystem(&tmpfs_fs_type);
	if (error) {
		printk(KERN_ERR "Could not register tmpfs\n");
		goto out3;
	}
#ifdef CONFIG_TMPFS
	error = register_filesystem(&shmem_fs_type);
	if (error) {
		printk(KERN_ERR "Could not register shm fs\n");
		goto out2;
	}
	devfs_mk_dir(NULL, "shm", NULL);
#endif
	shm_mnt = kern_mount(&tmpfs_fs_type);
	if (IS_ERR(shm_mnt)) {
		error = PTR_ERR(shm_mnt);
		printk(KERN_ERR "Could not kern_mount tmpfs\n");
		goto out1;
	}

	/* The internal instance should not do size checking */
	shmem_set_size(SHMEM_SB(shm_mnt->mnt_sb), ULONG_MAX, ULONG_MAX);
	return 0;

out1:
#ifdef CONFIG_TMPFS
	unregister_filesystem(&shmem_fs_type);
out2:
#endif
	unregister_filesystem(&tmpfs_fs_type);
out3:
	shm_mnt = ERR_PTR(error);
	return error;
}
module_init(init_tmpfs)

/*
 * shmem_file_setup - get an unlinked file living in tmpfs
 *
 * @name: name for dentry (to be seen in /proc/<pid>/maps
 * @size: size to be set for the file
 *
 */
struct file *shmem_file_setup(char *name, loff_t size)
{
	int error;
	struct file *file;
	struct inode *inode;
	struct dentry *dentry, *root;
	struct qstr this;
	int vm_enough_memory(long pages);

	if (IS_ERR(shm_mnt))
		return (void *)shm_mnt;

	if (size > SHMEM_MAX_BYTES)
		return ERR_PTR(-EINVAL);

	if (!vm_enough_memory(VM_ACCT(size)))
		return ERR_PTR(-ENOMEM);

	this.name = name;
	this.len = strlen(name);
	this.hash = 0; /* will go */
	root = shm_mnt->mnt_root;
	dentry = d_alloc(root, &this);
	if (!dentry)
		return ERR_PTR(-ENOMEM);

	error = -ENFILE;
	file = get_empty_filp();
	if (!file)
		goto put_dentry;

	error = -ENOSPC;
	inode = shmem_get_inode(root->d_sb, S_IFREG | S_IRWXUGO, 0);
	if (!inode)
		goto close_file;

	d_instantiate(dentry, inode);
	inode->i_size = size;
	inode->i_nlink = 0;	/* It is unlinked */
	file->f_vfsmnt = mntget(shm_mnt);
	file->f_dentry = dentry;
	file->f_op = &shmem_file_operations;
	file->f_mode = FMODE_WRITE | FMODE_READ;
	return file;

close_file:
	put_filp(file);
put_dentry:
	dput(dentry);
	return ERR_PTR(error);
}

/*
 * shmem_zero_setup - setup a shared anonymous mapping
 *
 * @vma: the vma to be mmapped is prepared by do_mmap_pgoff
 */
int shmem_zero_setup(struct vm_area_struct *vma)
{
	struct file *file;
	loff_t size = vma->vm_end - vma->vm_start;

	file = shmem_file_setup("dev/zero", size);
	if (IS_ERR(file))
		return PTR_ERR(file);

	if (vma->vm_file)
		fput(vma->vm_file);
	vma->vm_file = file;
	vma->vm_ops = &shmem_vm_ops;
	return 0;
}

EXPORT_SYMBOL(shmem_file_setup);
