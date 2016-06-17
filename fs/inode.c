/*
 * linux/fs/inode.c
 *
 * (C) 1997 Linus Torvalds
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/dcache.h>
#include <linux/init.h>
#include <linux/quotaops.h>
#include <linux/slab.h>
#include <linux/cache.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/prefetch.h>
#include <linux/locks.h>

/*
 * New inode.c implementation.
 *
 * This implementation has the basic premise of trying
 * to be extremely low-overhead and SMP-safe, yet be
 * simple enough to be "obviously correct".
 *
 * Famous last words.
 */

/* inode dynamic allocation 1999, Andrea Arcangeli <andrea@suse.de> */

/* #define INODE_PARANOIA 1 */
/* #define INODE_DEBUG 1 */

/*
 * Inode lookup is no longer as critical as it used to be:
 * most of the lookups are going to be through the dcache.
 */
#define I_HASHBITS	i_hash_shift
#define I_HASHMASK	i_hash_mask

static unsigned int i_hash_mask;
static unsigned int i_hash_shift;

/*
 * Each inode can be on two separate lists. One is
 * the hash list of the inode, used for lookups. The
 * other linked list is the "type" list:
 *  "in_use" - valid inode, i_count > 0, i_nlink > 0
 *  "dirty"  - as "in_use" but also dirty
 *  "unused" - valid inode, i_count = 0, no pages in the pagecache
 *  "unused_pagecache" - valid inode, i_count = 0, data in the pagecache
 *
 * A "dirty" list is maintained for each super block,
 * allowing for low-overhead inode sync() operations.
 */

static LIST_HEAD(inode_in_use);
static LIST_HEAD(inode_unused);
static LIST_HEAD(inode_unused_pagecache);
static struct list_head *inode_hashtable;
static LIST_HEAD(anon_hash_chain); /* for inodes with NULL i_sb */

/*
 * A simple spinlock to protect the list manipulations.
 *
 * NOTE! You also have to own the lock if you change
 * the i_state of an inode while it is in use..
 */
static spinlock_t inode_lock = SPIN_LOCK_UNLOCKED;

/*
 * Statistics gathering..
 */
struct inodes_stat_t inodes_stat;

static kmem_cache_t * inode_cachep;

static struct inode *alloc_inode(struct super_block *sb)
{
	static struct address_space_operations empty_aops;
	static struct inode_operations empty_iops;
	static struct file_operations empty_fops;
	struct inode *inode;

	if (sb->s_op->alloc_inode)
		inode = sb->s_op->alloc_inode(sb);
	else {
		inode = (struct inode *) kmem_cache_alloc(inode_cachep, SLAB_KERNEL);
		/* will die */
		if (inode)
			memset(&inode->u, 0, sizeof(inode->u));
	}

	if (inode) {
		struct address_space * const mapping = &inode->i_data;

		inode->i_sb = sb;
		inode->i_dev = sb->s_dev;
		inode->i_blkbits = sb->s_blocksize_bits;
		inode->i_flags = 0;
		atomic_set(&inode->i_count, 1);
		inode->i_sock = 0;
		inode->i_op = &empty_iops;
		inode->i_fop = &empty_fops;
		inode->i_nlink = 1;
		atomic_set(&inode->i_writecount, 0);
		inode->i_size = 0;
		inode->i_blocks = 0;
		inode->i_bytes = 0;
		inode->i_generation = 0;
		memset(&inode->i_dquot, 0, sizeof(inode->i_dquot));
		inode->i_pipe = NULL;
		inode->i_bdev = NULL;
		inode->i_cdev = NULL;

		mapping->a_ops = &empty_aops;
		mapping->host = inode;
		mapping->gfp_mask = GFP_HIGHUSER;
		inode->i_mapping = mapping;
	}
	return inode;
}

static void destroy_inode(struct inode *inode) 
{
	if (inode_has_buffers(inode))
		BUG();
	/* Reinitialise the waitqueue head because __wait_on_freeing_inode()
	   may have left stale entries on it which it can't remove (since
	   it knows we're freeing the inode right now */
	init_waitqueue_head(&inode->i_wait);
	if (inode->i_sb->s_op->destroy_inode)
		inode->i_sb->s_op->destroy_inode(inode);
	else
		kmem_cache_free(inode_cachep, inode);
}


/*
 * These are initializations that only need to be done
 * once, because the fields are idempotent across use
 * of the inode, so let the slab aware of that.
 */
void inode_init_once(struct inode *inode)
{
	memset(inode, 0, sizeof(*inode));
	__inode_init_once(inode);
}

void __inode_init_once(struct inode *inode)
{
	init_waitqueue_head(&inode->i_wait);
	INIT_LIST_HEAD(&inode->i_hash);
	INIT_LIST_HEAD(&inode->i_data.clean_pages);
	INIT_LIST_HEAD(&inode->i_data.dirty_pages);
	INIT_LIST_HEAD(&inode->i_data.locked_pages);
	INIT_LIST_HEAD(&inode->i_dentry);
	INIT_LIST_HEAD(&inode->i_dirty_buffers);
	INIT_LIST_HEAD(&inode->i_dirty_data_buffers);
	INIT_LIST_HEAD(&inode->i_devices);
	sema_init(&inode->i_sem, 1);
	sema_init(&inode->i_zombie, 1);
	init_rwsem(&inode->i_alloc_sem);
	spin_lock_init(&inode->i_data.i_shared_lock);
}

static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct inode * inode = (struct inode *) foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
		inode_init_once(inode);
}

/*
 * Put the inode on the super block's dirty list.
 *
 * CAREFUL! We mark it dirty unconditionally, but
 * move it onto the dirty list only if it is hashed.
 * If it was not hashed, it will never be added to
 * the dirty list even if it is later hashed, as it
 * will have been marked dirty already.
 *
 * In short, make sure you hash any inodes _before_
 * you start marking them dirty..
 */
 
/**
 *	__mark_inode_dirty -	internal function
 *	@inode: inode to mark
 *	@flags: what kind of dirty (i.e. I_DIRTY_SYNC)
 *	Mark an inode as dirty. Callers should use mark_inode_dirty or
 *  	mark_inode_dirty_sync.
 */
 
void __mark_inode_dirty(struct inode *inode, int flags)
{
	struct super_block * sb = inode->i_sb;

	if (!sb)
		return;

	/* Don't do this for I_DIRTY_PAGES - that doesn't actually dirty the inode itself */
	if (flags & (I_DIRTY_SYNC | I_DIRTY_DATASYNC)) {
		if (sb->s_op && sb->s_op->dirty_inode)
			sb->s_op->dirty_inode(inode);
	}

	/* avoid the locking if we can */
	if ((inode->i_state & flags) == flags)
		return;

	spin_lock(&inode_lock);
	if ((inode->i_state & flags) != flags) {
		inode->i_state |= flags;
		/* Only add valid (ie hashed) inodes to the dirty list */
		if (!(inode->i_state & (I_LOCK|I_FREEING|I_CLEAR)) &&
		    !list_empty(&inode->i_hash)) {
			list_del(&inode->i_list);
			list_add(&inode->i_list, &sb->s_dirty);
		}
	}
	spin_unlock(&inode_lock);
}

static void __wait_on_inode(struct inode * inode)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&inode->i_wait, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (inode->i_state & I_LOCK) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&inode->i_wait, &wait);
	current->state = TASK_RUNNING;
}

static inline void wait_on_inode(struct inode *inode)
{
	if (inode->i_state & I_LOCK)
		__wait_on_inode(inode);
}

/*
 * If we try to find an inode in the inode hash while it is being deleted, we
 * have to wait until the filesystem completes its deletion before reporting
 * that it isn't found.  This is because iget will immediately call
 * ->read_inode, and we want to be sure that evidence of the deletion is found
 * by ->read_inode.
 *
 * Unlike the 2.6 version, this call call cannot return early, since inodes
 * do not share wait queue. Therefore, we don't call remove_wait_queue(); it
 * would be dangerous to do so since the inode may have already been freed, 
 * and it's unnecessary, since the inode is definitely going to get freed.
 *
 * This is called with inode_lock held.
 */
static void __wait_on_freeing_inode(struct inode *inode)
{
        DECLARE_WAITQUEUE(wait, current);

        add_wait_queue(&inode->i_wait, &wait);
        set_current_state(TASK_UNINTERRUPTIBLE);
        spin_unlock(&inode_lock);
        schedule();

        spin_lock(&inode_lock);
}

static inline void write_inode(struct inode *inode, int sync)
{
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->write_inode && !is_bad_inode(inode))
		inode->i_sb->s_op->write_inode(inode, sync);
}

static inline void __iget(struct inode * inode)
{
	if (atomic_read(&inode->i_count)) {
		atomic_inc(&inode->i_count);
		return;
	}
	atomic_inc(&inode->i_count);
	if (!(inode->i_state & (I_DIRTY|I_LOCK))) {
		list_del(&inode->i_list);
		list_add(&inode->i_list, &inode_in_use);
	}
	inodes_stat.nr_unused--;
}

static inline void __refile_inode(struct inode *inode)
{
	struct list_head *to;

	if (inode->i_state & I_FREEING)
		return;
	if (list_empty(&inode->i_hash))
		return;

	if (inode->i_state & I_DIRTY)
		to = &inode->i_sb->s_dirty;
	else if (atomic_read(&inode->i_count))
		to = &inode_in_use;
	else if (inode->i_data.nrpages)
		to = &inode_unused_pagecache;
	else
		to = &inode_unused;
	list_del(&inode->i_list);
	list_add(&inode->i_list, to);
}

void refile_inode(struct inode *inode)
{
	if (!inode)
		return;
	spin_lock(&inode_lock);
	if (!(inode->i_state & I_LOCK))
		__refile_inode(inode);
	spin_unlock(&inode_lock);
}

static inline void __sync_one(struct inode *inode, int sync)
{
	unsigned dirty;

	list_del(&inode->i_list);
	list_add(&inode->i_list, &inode->i_sb->s_locked_inodes);

	if (inode->i_state & (I_LOCK|I_FREEING))
		BUG();

	/* Set I_LOCK, reset I_DIRTY */
	dirty = inode->i_state & I_DIRTY;
	inode->i_state |= I_LOCK;
	inode->i_state &= ~I_DIRTY;
	spin_unlock(&inode_lock);

	filemap_fdatasync(inode->i_mapping);

	/* Don't write the inode if only I_DIRTY_PAGES was set */
	if (dirty & (I_DIRTY_SYNC | I_DIRTY_DATASYNC))
		write_inode(inode, sync);

	filemap_fdatawait(inode->i_mapping);

	spin_lock(&inode_lock);
	inode->i_state &= ~I_LOCK;
	__refile_inode(inode);
	wake_up(&inode->i_wait);
}

static inline void sync_one(struct inode *inode, int sync)
{
	while (inode->i_state & I_LOCK) {
		__iget(inode);
		spin_unlock(&inode_lock);
		__wait_on_inode(inode);
		iput(inode);
		spin_lock(&inode_lock);
	}

	__sync_one(inode, sync);
}

static inline void sync_list(struct list_head *head)
{
	struct list_head * tmp;

	while ((tmp = head->prev) != head) 
		__sync_one(list_entry(tmp, struct inode, i_list), 0);
}

static inline void wait_on_locked(struct list_head *head)
{
	struct list_head * tmp;
	while ((tmp = head->prev) != head) {
		struct inode *inode = list_entry(tmp, struct inode, i_list);
		__iget(inode);
		spin_unlock(&inode_lock);
		__wait_on_inode(inode);
		iput(inode);
		spin_lock(&inode_lock);
	}
}

static inline int try_to_sync_unused_list(struct list_head *head, int nr_inodes)
{
	struct list_head *tmp = head;
	struct inode *inode;

	while (nr_inodes && (tmp = tmp->prev) != head) {
		inode = list_entry(tmp, struct inode, i_list);

		if (!atomic_read(&inode->i_count)) {
			__sync_one(inode, 0);
			nr_inodes--;

			/* 
			 * __sync_one moved the inode to another list,
			 * so we have to start looking from the list head.
			 */
			tmp = head;
		}
	}

	return nr_inodes;
}

void sync_inodes_sb(struct super_block *sb)
{
	spin_lock(&inode_lock);
	while (!list_empty(&sb->s_dirty)||!list_empty(&sb->s_locked_inodes)) {
		sync_list(&sb->s_dirty);
		wait_on_locked(&sb->s_locked_inodes);
	}
	spin_unlock(&inode_lock);
}

/*
 * Note:
 * We don't need to grab a reference to superblock here. If it has non-empty
 * ->s_dirty it's hadn't been killed yet and kill_super() won't proceed
 * past sync_inodes_sb() until both ->s_dirty and ->s_locked_inodes are
 * empty. Since __sync_one() regains inode_lock before it finally moves
 * inode from superblock lists we are OK.
 */

void sync_unlocked_inodes(void)
{
	struct super_block * sb;
	spin_lock(&inode_lock);
	spin_lock(&sb_lock);
	sb = sb_entry(super_blocks.next);
	for (; sb != sb_entry(&super_blocks); sb = sb_entry(sb->s_list.next)) {
		if (!list_empty(&sb->s_dirty)) {
			spin_unlock(&sb_lock);
			sync_list(&sb->s_dirty);
			spin_lock(&sb_lock);
		}
	}
	spin_unlock(&sb_lock);
	spin_unlock(&inode_lock);
}

/*
 * Find a superblock with inodes that need to be synced
 */

static struct super_block *get_super_to_sync(void)
{
	struct list_head *p;
restart:
	spin_lock(&inode_lock);
	spin_lock(&sb_lock);
	list_for_each(p, &super_blocks) {
		struct super_block *s = list_entry(p,struct super_block,s_list);
		if (list_empty(&s->s_dirty) && list_empty(&s->s_locked_inodes))
			continue;
		s->s_count++;
		spin_unlock(&sb_lock);
		spin_unlock(&inode_lock);
		down_read(&s->s_umount);
		if (!s->s_root) {
			drop_super(s);
			goto restart;
		}
		return s;
	}
	spin_unlock(&sb_lock);
	spin_unlock(&inode_lock);
	return NULL;
}

/**
 *	sync_inodes
 *	@dev: device to sync the inodes from.
 *
 *	sync_inodes goes through the super block's dirty list, 
 *	writes them out, and puts them back on the normal list.
 */

void sync_inodes(kdev_t dev)
{
	struct super_block * s;

	/*
	 * Search the super_blocks array for the device(s) to sync.
	 */
	if (dev) {
		if ((s = get_super(dev)) != NULL) {
			sync_inodes_sb(s);
			drop_super(s);
		}
	} else {
		while ((s = get_super_to_sync()) != NULL) {
			sync_inodes_sb(s);
			drop_super(s);
		}
	}
}

static void try_to_sync_unused_inodes(void * arg)
{
	struct super_block * sb;
	int nr_inodes = inodes_stat.nr_unused;

	spin_lock(&inode_lock);
	spin_lock(&sb_lock);
	sb = sb_entry(super_blocks.next);
	for (; nr_inodes && sb != sb_entry(&super_blocks); sb = sb_entry(sb->s_list.next)) {
		if (list_empty(&sb->s_dirty))
			continue;
		spin_unlock(&sb_lock);
		nr_inodes = try_to_sync_unused_list(&sb->s_dirty, nr_inodes);
		spin_lock(&sb_lock);
	}
	spin_unlock(&sb_lock);
	spin_unlock(&inode_lock);
}

static struct tq_struct unused_inodes_flush_task;

/**
 *	write_inode_now	-	write an inode to disk
 *	@inode: inode to write to disk
 *	@sync: whether the write should be synchronous or not
 *
 *	This function commits an inode to disk immediately if it is
 *	dirty. This is primarily needed by knfsd.
 */
 
void write_inode_now(struct inode *inode, int sync)
{
	struct super_block * sb = inode->i_sb;

	if (sb) {
		spin_lock(&inode_lock);
		while (inode->i_state & I_DIRTY)
			sync_one(inode, sync);
		spin_unlock(&inode_lock);
		if (sync)
			wait_on_inode(inode);
	}
	else
		printk(KERN_ERR "write_inode_now: no super block\n");
}

/**
 * generic_osync_inode - flush all dirty data for a given inode to disk
 * @inode: inode to write
 * @datasync: if set, don't bother flushing timestamps
 *
 * This can be called by file_write functions for files which have the
 * O_SYNC flag set, to flush dirty writes to disk.  
 */

int generic_osync_inode(struct inode *inode, int what)
{
	int err = 0, err2 = 0, need_write_inode_now = 0;
	
	/* 
	 * WARNING
	 *
	 * Currently, the filesystem write path does not pass the
	 * filp down to the low-level write functions.  Therefore it
	 * is impossible for (say) __block_commit_write to know if
	 * the operation is O_SYNC or not.
	 *
	 * Ideally, O_SYNC writes would have the filesystem call
	 * ll_rw_block as it went to kick-start the writes, and we
	 * could call osync_inode_buffers() here to wait only for
	 * those IOs which have already been submitted to the device
	 * driver layer.  As it stands, if we did this we'd not write
	 * anything to disk since our writes have not been queued by
	 * this point: they are still on the dirty LRU.
	 * 
	 * So, currently we will call fsync_inode_buffers() instead,
	 * to flush _all_ dirty buffers for this inode to disk on 
	 * every O_SYNC write, not just the synchronous I/Os.  --sct
	 */

	if (what & OSYNC_METADATA)
		err = fsync_inode_buffers(inode);
	if (what & OSYNC_DATA)
		err2 = fsync_inode_data_buffers(inode);
	if (!err)
		err = err2;

	spin_lock(&inode_lock);
	if ((inode->i_state & I_DIRTY) &&
	    ((what & OSYNC_INODE) || (inode->i_state & I_DIRTY_DATASYNC)))
		need_write_inode_now = 1;
	spin_unlock(&inode_lock);

	if (need_write_inode_now)
		write_inode_now(inode, 1);
	else
		wait_on_inode(inode);

	return err;
}

/**
 * clear_inode - clear an inode
 * @inode: inode to clear
 *
 * This is called by the filesystem to tell us
 * that the inode is no longer useful. We just
 * terminate it with extreme prejudice.
 */
 
void clear_inode(struct inode *inode)
{
	invalidate_inode_buffers(inode);
       
	if (inode->i_data.nrpages)
		BUG();
	if (!(inode->i_state & I_FREEING))
		BUG();
	if (inode->i_state & I_CLEAR)
		BUG();
	wait_on_inode(inode);
	DQUOT_DROP(inode);
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->clear_inode)
		inode->i_sb->s_op->clear_inode(inode);
	if (inode->i_bdev)
		bd_forget(inode);
	else if (inode->i_cdev) {
		cdput(inode->i_cdev);
		inode->i_cdev = NULL;
	}
	inode->i_state = I_CLEAR;
}

/*
 * Dispose-list gets a local list with local inodes in it, so it doesn't
 * need to worry about list corruption and SMP locks.
 */
static void dispose_list(struct list_head *head)
{
	int nr_disposed = 0;

	while (!list_empty(head)) {
		struct inode *inode;

		inode = list_entry(head->next, struct inode, i_list);
		list_del(&inode->i_list);

		if (inode->i_data.nrpages)
			truncate_inode_pages(&inode->i_data, 0);
		clear_inode(inode);
		spin_lock(&inode_lock);
		list_del(&inode->i_hash);
		INIT_LIST_HEAD(&inode->i_hash);
		spin_unlock(&inode_lock);
		wake_up(&inode->i_wait);
		destroy_inode(inode);
		nr_disposed++;
	}
	spin_lock(&inode_lock);
	inodes_stat.nr_inodes -= nr_disposed;
	spin_unlock(&inode_lock);
}

/*
 * Invalidate all inodes for a device.
 */
static int invalidate_list(struct list_head *head, struct super_block * sb, struct list_head * dispose)
{
	struct list_head *next;
	int busy = 0, count = 0;

	next = head->next;
	for (;;) {
		struct list_head * tmp = next;
		struct inode * inode;

		next = next->next;
		if (tmp == head)
			break;
		inode = list_entry(tmp, struct inode, i_list);
		if (inode->i_sb != sb)
			continue;
		invalidate_inode_buffers(inode);
		if (!atomic_read(&inode->i_count)) {
			list_del_init(&inode->i_hash);
			list_del(&inode->i_list);
			list_add(&inode->i_list, dispose);
			inode->i_state |= I_FREEING;
			count++;
			continue;
		}
		busy = 1;
	}
	/* only unused inodes may be cached with i_count zero */
	inodes_stat.nr_unused -= count;
	return busy;
}

/*
 * This is a two-stage process. First we collect all
 * offending inodes onto the throw-away list, and in
 * the second stage we actually dispose of them. This
 * is because we don't want to sleep while messing
 * with the global lists..
 */
 
/**
 *	invalidate_inodes	- discard the inodes on a device
 *	@sb: superblock
 *
 *	Discard all of the inodes for a given superblock. If the discard
 *	fails because there are busy inodes then a non zero value is returned.
 *	If the discard is successful all the inodes have been discarded.
 */
 
int invalidate_inodes(struct super_block * sb)
{
	int busy;
	LIST_HEAD(throw_away);

	spin_lock(&inode_lock);
	busy = invalidate_list(&inode_in_use, sb, &throw_away);
	busy |= invalidate_list(&inode_unused, sb, &throw_away);
	busy |= invalidate_list(&inode_unused_pagecache, sb, &throw_away);
	busy |= invalidate_list(&sb->s_dirty, sb, &throw_away);
	busy |= invalidate_list(&sb->s_locked_inodes, sb, &throw_away);
	spin_unlock(&inode_lock);

	dispose_list(&throw_away);

	return busy;
}
 
int invalidate_device(kdev_t dev, int do_sync)
{
	struct super_block *sb;
	int res;

	if (do_sync)
		fsync_dev(dev);

	res = 0;
	sb = get_super(dev);
	if (sb) {
		/*
		 * no need to lock the super, get_super holds the
		 * read semaphore so the filesystem cannot go away
		 * under us (->put_super runs with the write lock
		 * hold).
		 */
		shrink_dcache_sb(sb);
		res = invalidate_inodes(sb);
		drop_super(sb);
	}
	invalidate_buffers(dev);
	return res;
}


/*
 * This is called with the inode lock held. It searches
 * the in-use for freeable inodes, which are moved to a
 * temporary list and then placed on the unused list by
 * dispose_list. 
 *
 * We don't expect to have to call this very often.
 *
 * We leave the inode in the inode hash table until *after* 
 * the filesystem's ->delete_inode (in dispose_list) completes.
 * This ensures that an iget (such as nfsd might instigate) will 
 * always find up-to-date information either in the hash or on disk.
 *
 * I_FREEING is set so that no-one will take a new reference
 * to the inode while it is being deleted.
 *
 * N.B. The spinlock is released during the call to
 *      dispose_list.
 */
#define CAN_UNUSE(inode) \
	((((inode)->i_state | (inode)->i_data.nrpages) == 0)  && \
	 !inode_has_buffers(inode))
#define INODE(entry)	(list_entry(entry, struct inode, i_list))

void prune_icache(int goal)
{
	LIST_HEAD(list);
	struct list_head *entry, *freeable = &list;
	int count;
#ifdef CONFIG_HIGHMEM
	int avg_pages;
#endif
	struct inode * inode;

	spin_lock(&inode_lock);

	count = 0;
	entry = inode_unused.prev;
	while (entry != &inode_unused)
	{
		struct list_head *tmp = entry;

		entry = entry->prev;
		inode = INODE(tmp);
		if (inode->i_state & (I_FREEING|I_CLEAR|I_LOCK))
			continue;
		if (!CAN_UNUSE(inode))
			continue;
		if (atomic_read(&inode->i_count))
			continue;
		list_del(tmp);
		list_add(tmp, freeable);
		inode->i_state |= I_FREEING;
		count++;
		if (--goal <= 0)
			break;
	}
	inodes_stat.nr_unused -= count;
	spin_unlock(&inode_lock);

	dispose_list(freeable);

	/* 
	 * If we didn't freed enough clean inodes schedule
	 * a sync of the dirty inodes, we cannot do it
	 * from here or we're either synchronously dogslow
	 * or we deadlock with oom.
	 */
	if (goal > 0)
		schedule_task(&unused_inodes_flush_task);

#ifdef CONFIG_HIGHMEM
	/*
	 * On highmem machines it is possible to have low memory
	 * filled with inodes that cannot be reclaimed because they
	 * have page cache pages in highmem attached to them.
	 * This could deadlock the system if the memory used by
	 * inodes is significant compared to the amount of freeable
	 * low memory.  In that case we forcefully remove the page
	 * cache pages from the inodes we want to reclaim.
	 *
	 * Note that this loop doesn't actually reclaim the inodes;
	 * once the last pagecache pages belonging to the inode is
	 * gone it will be placed on the inode_unused list and the
	 * loop above will prune it the next time prune_icache() is
	 * called.
	 */
	if (goal <= 0)
		return;
	if (inodes_stat.nr_unused * sizeof(struct inode) * 10 <
				freeable_lowmem() * PAGE_SIZE)
		return;

	wakeup_bdflush();

	avg_pages = page_cache_size;
	avg_pages -= atomic_read(&buffermem_pages) + swapper_space.nrpages;
	avg_pages = avg_pages / (inodes_stat.nr_inodes + 1);
	spin_lock(&inode_lock);
	while (goal-- > 0) {
		if (list_empty(&inode_unused_pagecache))
			break;
		entry = inode_unused_pagecache.prev;
		list_del(entry);
		list_add(entry, &inode_unused_pagecache);

		inode = INODE(entry);
		/* Don't nuke inodes with lots of page cache attached. */
		if (inode->i_mapping->nrpages > 5 * avg_pages)
			continue;
		/* Because of locking we grab the inode and unlock the list .*/
		if (inode->i_state & I_LOCK)
			continue;
		inode->i_state |= I_LOCK;
		spin_unlock(&inode_lock);

		/*
		 * If the inode has clean pages only, we can free all its
		 * pagecache memory; the inode will automagically be refiled
		 * onto the unused_list.  The wakeup_bdflush above makes
		 * sure that all inodes become clean eventually.
		 */
		if (list_empty(&inode->i_mapping->dirty_pages) &&
				!inode_has_buffers(inode))
			invalidate_inode_pages(inode);

		/* Release the inode again. */
		spin_lock(&inode_lock);
		inode->i_state &= ~I_LOCK;
		wake_up(&inode->i_wait);
	}
	spin_unlock(&inode_lock);
#endif /* CONFIG_HIGHMEM */
}

int shrink_icache_memory(int priority, int gfp_mask)
{
	int count = 0;

	/*
	 * Nasty deadlock avoidance..
	 *
	 * We may hold various FS locks, and we don't
	 * want to recurse into the FS that called us
	 * in clear_inode() and friends..
	 */
	if (!(gfp_mask & __GFP_FS))
		return 0;

	count = inodes_stat.nr_unused / priority;

	prune_icache(count);
	return kmem_cache_shrink(inode_cachep);
}

/*
 * Called with the inode lock held.
 * NOTE: we are not increasing the inode-refcount, you must call __iget()
 * by hand after calling find_inode now! This simplifies iunique and won't
 * add any additional branch in the common code.
 */
static struct inode * find_inode(struct super_block * sb, unsigned long ino, struct list_head *head, find_inode_t find_actor, void *opaque)
{
	struct list_head *tmp;
	struct inode * inode;

repeat:
	tmp = head;
	for (;;) {
		tmp = tmp->next;
		inode = NULL;
		if (tmp == head)
			break;
		inode = list_entry(tmp, struct inode, i_hash);
		if (inode->i_ino != ino)
			continue;
		if (inode->i_sb != sb)
			continue;
		if (find_actor && !find_actor(inode, ino, opaque))
			continue;
		if (inode->i_state & (I_FREEING|I_CLEAR)) {
			__wait_on_freeing_inode(inode);
			goto repeat;
		}
		break;
	}
	return inode;
}

/**
 *	new_inode 	- obtain an inode
 *	@sb: superblock
 *
 *	Allocates a new inode for given superblock.
 */
 
struct inode * new_inode(struct super_block *sb)
{
	static unsigned long last_ino;
	struct inode * inode;

	spin_lock_prefetch(&inode_lock);
	
	inode = alloc_inode(sb);
	if (inode) {
		spin_lock(&inode_lock);
		inodes_stat.nr_inodes++;
		list_add(&inode->i_list, &inode_in_use);
		inode->i_ino = ++last_ino;
		inode->i_state = 0;
		spin_unlock(&inode_lock);
	}
	return inode;
}

void unlock_new_inode(struct inode *inode)
{
	/*
	 * This is special!  We do not need the spinlock
	 * when clearing I_LOCK, because we're guaranteed
	 * that nobody else tries to do anything about the
	 * state of the inode when it is locked, as we
	 * just created it (so there can be no old holders
	 * that haven't tested I_LOCK).
	 */
	inode->i_state &= ~(I_LOCK|I_NEW);
	wake_up(&inode->i_wait);
}

/*
 * This is called without the inode lock held.. Be careful.
 *
 * We no longer cache the sb_flags in i_flags - see fs.h
 *	-- rmk@arm.uk.linux.org
 */
static struct inode * get_new_inode(struct super_block *sb, unsigned long ino, struct list_head *head, find_inode_t find_actor, void *opaque)
{
	struct inode * inode;

	inode = alloc_inode(sb);
	if (inode) {
		struct inode * old;

		spin_lock(&inode_lock);
		/* We released the lock, so.. */
		old = find_inode(sb, ino, head, find_actor, opaque);
		if (!old) {
			inodes_stat.nr_inodes++;
			list_add(&inode->i_list, &inode_in_use);
			list_add(&inode->i_hash, head);
			inode->i_ino = ino;
			inode->i_state = I_LOCK|I_NEW;
			spin_unlock(&inode_lock);

			/*
			 * Return the locked inode with I_NEW set, the
			 * caller is responsible for filling in the contents
			 */
			return inode;
		}

		/*
		 * Uhhuh, somebody else created the same inode under
		 * us. Use the old inode instead of the one we just
		 * allocated.
		 */
		__iget(old);
		spin_unlock(&inode_lock);
		destroy_inode(inode);
		inode = old;
		wait_on_inode(inode);
	}
	return inode;
}

static inline unsigned long hash(struct super_block *sb, unsigned long i_ino)
{
	unsigned long tmp = i_ino + ((unsigned long) sb / L1_CACHE_BYTES);
	tmp = tmp + (tmp >> I_HASHBITS);
	return tmp & I_HASHMASK;
}

/* Yeah, I know about quadratic hash. Maybe, later. */

/**
 *	iunique - get a unique inode number
 *	@sb: superblock
 *	@max_reserved: highest reserved inode number
 *
 *	Obtain an inode number that is unique on the system for a given
 *	superblock. This is used by file systems that have no natural
 *	permanent inode numbering system. An inode number is returned that
 *	is higher than the reserved limit but unique.
 *
 *	BUGS:
 *	With a large number of inodes live on the file system this function
 *	currently becomes quite slow.
 */
 
ino_t iunique(struct super_block *sb, ino_t max_reserved)
{
	static ino_t counter = 0;
	struct inode *inode;
	struct list_head * head;
	ino_t res;
	spin_lock(&inode_lock);
retry:
	if (counter > max_reserved) {
		head = inode_hashtable + hash(sb,counter);
		inode = find_inode(sb, res = counter++, head, NULL, NULL);
		if (!inode) {
			spin_unlock(&inode_lock);
			return res;
		}
	} else {
		counter = max_reserved + 1;
	}
	goto retry;
	
}

/**
 *	ilookup - search for an inode in the inode cache
 *	@sb:         super block of file system to search
 *	@ino:        inode number to search for
 *
 *	If the inode is in the cache, the inode is returned with an
 *	incremented reference count.
 *
 *	Otherwise, %NULL is returned.
 *
 *	This is almost certainly not the function you are looking for.
 *	If you think you need to use this, consult an expert first.
 */
struct inode *ilookup(struct super_block *sb, unsigned long ino)
{
	struct list_head * head = inode_hashtable + hash(sb,ino);
	struct inode * inode;

	spin_lock(&inode_lock);
	inode = find_inode(sb, ino, head, NULL, NULL);
	if (inode) {
		__iget(inode);
		spin_unlock(&inode_lock);
		wait_on_inode(inode);
		return inode;
	}
	spin_unlock(&inode_lock);

	return inode;
}

struct inode *igrab(struct inode *inode)
{
	spin_lock(&inode_lock);
	if (!(inode->i_state & I_FREEING))
		__iget(inode);
	else
		/*
		 * Handle the case where s_op->clear_inode is not been
		 * called yet, and somebody is calling igrab
		 * while the inode is getting freed.
		 */
		inode = NULL;
	spin_unlock(&inode_lock);
	return inode;
}

struct inode *iget4_locked(struct super_block *sb, unsigned long ino, find_inode_t find_actor, void *opaque)
{
	struct list_head * head = inode_hashtable + hash(sb,ino);
	struct inode * inode;

	spin_lock(&inode_lock);
	inode = find_inode(sb, ino, head, find_actor, opaque);
	if (inode) {
		__iget(inode);
		spin_unlock(&inode_lock);
		wait_on_inode(inode);
		return inode;
	}
	spin_unlock(&inode_lock);

	/*
	 * get_new_inode() will do the right thing, re-trying the search
	 * in case it had to block at any point.
	 */
	return get_new_inode(sb, ino, head, find_actor, opaque);
}

/**
 *	insert_inode_hash - hash an inode
 *	@inode: unhashed inode
 *
 *	Add an inode to the inode hash for this superblock. If the inode
 *	has no superblock it is added to a separate anonymous chain.
 */
 
void insert_inode_hash(struct inode *inode)
{
	struct list_head *head = &anon_hash_chain;
	if (inode->i_sb)
		head = inode_hashtable + hash(inode->i_sb, inode->i_ino);
	spin_lock(&inode_lock);
	list_add(&inode->i_hash, head);
	spin_unlock(&inode_lock);
}

/**
 *	remove_inode_hash - remove an inode from the hash
 *	@inode: inode to unhash
 *
 *	Remove an inode from the superblock or anonymous hash.
 */
 
void remove_inode_hash(struct inode *inode)
{
	spin_lock(&inode_lock);
	list_del(&inode->i_hash);
	INIT_LIST_HEAD(&inode->i_hash);
	spin_unlock(&inode_lock);
}

/**
 *	iput	- put an inode 
 *	@inode: inode to put
 *
 *	Puts an inode, dropping its usage count. If the inode use count hits
 *	zero the inode is also then freed and may be destroyed.
 */
 
void iput(struct inode *inode)
{
	if (inode) {
		struct super_block *sb = inode->i_sb;
		struct super_operations *op = NULL;

		if (inode->i_state == I_CLEAR)
			BUG();

		if (sb && sb->s_op)
			op = sb->s_op;
		if (op && op->put_inode)
			op->put_inode(inode);

		if (!atomic_dec_and_lock(&inode->i_count, &inode_lock))
			return;

		if (!inode->i_nlink) {
			list_del(&inode->i_list);
			INIT_LIST_HEAD(&inode->i_list);
			inode->i_state|=I_FREEING;
			inodes_stat.nr_inodes--;
			spin_unlock(&inode_lock);

			if (inode->i_data.nrpages)
				truncate_inode_pages(&inode->i_data, 0);

			if (op && op->delete_inode) {
				void (*delete)(struct inode *) = op->delete_inode;
				if (!is_bad_inode(inode))
					DQUOT_INIT(inode);
				/* s_op->delete_inode internally recalls clear_inode() */
				delete(inode);
			} else
				clear_inode(inode);
			spin_lock(&inode_lock);
			list_del(&inode->i_hash);
			INIT_LIST_HEAD(&inode->i_hash);
			spin_unlock(&inode_lock);
			wake_up(&inode->i_wait);
			if (inode->i_state != I_CLEAR)
				BUG();
		} else {
			if (!list_empty(&inode->i_hash)) {
				if (!(inode->i_state & (I_DIRTY|I_LOCK))) 
					__refile_inode(inode);
				inodes_stat.nr_unused++;
				spin_unlock(&inode_lock);
				if (!sb || (sb->s_flags & MS_ACTIVE))
					return;
				write_inode_now(inode, 1);
				spin_lock(&inode_lock);
				inodes_stat.nr_unused--;
				list_del_init(&inode->i_hash);
			}
			list_del_init(&inode->i_list);
			inode->i_state|=I_FREEING;
			inodes_stat.nr_inodes--;
			spin_unlock(&inode_lock);
			if (inode->i_data.nrpages)
				truncate_inode_pages(&inode->i_data, 0);
			clear_inode(inode);
		}
		destroy_inode(inode);
	}
}

void force_delete(struct inode *inode)
{
	/*
	 * Kill off unused inodes ... iput() will unhash and
	 * delete the inode if we set i_nlink to zero.
	 */
	if (atomic_read(&inode->i_count) == 1)
		inode->i_nlink = 0;
}

/**
 *	bmap	- find a block number in a file
 *	@inode: inode of file
 *	@block: block to find
 *
 *	Returns the block number on the device holding the inode that
 *	is the disk block number for the block of the file requested.
 *	That is, asked for block 4 of inode 1 the function will return the
 *	disk block relative to the disk start that holds that block of the 
 *	file.
 */
 
int bmap(struct inode * inode, int block)
{
	int res = 0;
	if (inode->i_mapping->a_ops->bmap)
		res = inode->i_mapping->a_ops->bmap(inode->i_mapping, block);
	return res;
}

/*
 * Initialize the hash tables.
 */
void __init inode_init(unsigned long mempages)
{
	struct list_head *head;
	unsigned long order;
	unsigned int nr_hash;
	int i;

	mempages >>= (14 - PAGE_SHIFT);
	mempages *= sizeof(struct list_head);
	for (order = 0; ((1UL << order) << PAGE_SHIFT) < mempages; order++)
		;

	do {
		unsigned long tmp;

		nr_hash = (1UL << order) * PAGE_SIZE /
			sizeof(struct list_head);
		i_hash_mask = (nr_hash - 1);

		tmp = nr_hash;
		i_hash_shift = 0;
		while ((tmp >>= 1UL) != 0UL)
			i_hash_shift++;

		inode_hashtable = (struct list_head *)
			__get_free_pages(GFP_ATOMIC, order);
	} while (inode_hashtable == NULL && --order >= 0);

	printk(KERN_INFO "Inode cache hash table entries: %d (order: %ld, %ld bytes)\n",
			nr_hash, order, (PAGE_SIZE << order));

	if (!inode_hashtable)
		panic("Failed to allocate inode hash table\n");

	head = inode_hashtable;
	i = nr_hash;
	do {
		INIT_LIST_HEAD(head);
		head++;
		i--;
	} while (i);

	/* inode slab cache */
	inode_cachep = kmem_cache_create("inode_cache", sizeof(struct inode),
					 0, SLAB_HWCACHE_ALIGN, init_once,
					 NULL);
	if (!inode_cachep)
		panic("cannot create inode slab cache");

	unused_inodes_flush_task.routine = try_to_sync_unused_inodes;
}

/**
 *	update_atime	-	update the access time
 *	@inode: inode accessed
 *
 *	Update the accessed time on an inode and mark it for writeback.
 *	This function automatically handles read only file systems and media,
 *	as well as the "noatime" flag and inode specific "noatime" markers.
 */
 
void update_atime (struct inode *inode)
{
	if (inode->i_atime == CURRENT_TIME)
		return;
	if (IS_NOATIME(inode))
		return;
	if (IS_NODIRATIME(inode) && S_ISDIR(inode->i_mode)) 
		return;
	if (IS_RDONLY(inode)) 
		return;
	inode->i_atime = CURRENT_TIME;
	mark_inode_dirty_sync (inode);
}

/**
 *	update_mctime	-	update the mtime and ctime
 *	@inode: inode accessed
 *
 *	Update the modified and changed times on an inode for writes to special
 *	files such as fifos.  No change is forced if the timestamps are already
 *	up-to-date or if the filesystem is readonly.
 */
 
void update_mctime (struct inode *inode)
{
	if (inode->i_mtime == CURRENT_TIME && inode->i_ctime == CURRENT_TIME)
		return;
	if (IS_RDONLY(inode))
		return;
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	mark_inode_dirty (inode);
}


/*
 *	Quota functions that want to walk the inode lists..
 */
#ifdef CONFIG_QUOTA

/* Functions back in dquot.c */
void put_dquot_list(struct list_head *);
int remove_inode_dquot_ref(struct inode *, short, struct list_head *);

void remove_dquot_ref(struct super_block *sb, short type)
{
	struct inode *inode;
	struct list_head *act_head;
	LIST_HEAD(tofree_head);

	if (!sb->dq_op)
		return;	/* nothing to do */
	/* We have to be protected against other CPUs */
	lock_kernel();		/* This lock is for quota code */
	spin_lock(&inode_lock);	/* This lock is for inodes code */
 
	list_for_each(act_head, &inode_in_use) {
		inode = list_entry(act_head, struct inode, i_list);
		if (inode->i_sb == sb && IS_QUOTAINIT(inode))
			remove_inode_dquot_ref(inode, type, &tofree_head);
	}
	list_for_each(act_head, &inode_unused) {
		inode = list_entry(act_head, struct inode, i_list);
		if (inode->i_sb == sb && IS_QUOTAINIT(inode))
			remove_inode_dquot_ref(inode, type, &tofree_head);
	}
	list_for_each(act_head, &inode_unused_pagecache) {
		inode = list_entry(act_head, struct inode, i_list);
		if (inode->i_sb == sb && IS_QUOTAINIT(inode))
			remove_inode_dquot_ref(inode, type, &tofree_head);
	}
	list_for_each(act_head, &sb->s_dirty) {
		inode = list_entry(act_head, struct inode, i_list);
		if (IS_QUOTAINIT(inode))
			remove_inode_dquot_ref(inode, type, &tofree_head);
	}
	list_for_each(act_head, &sb->s_locked_inodes) {
		inode = list_entry(act_head, struct inode, i_list);
		if (IS_QUOTAINIT(inode))
			remove_inode_dquot_ref(inode, type, &tofree_head);
	}
	spin_unlock(&inode_lock);
	unlock_kernel();

	put_dquot_list(&tofree_head);
}

#endif
