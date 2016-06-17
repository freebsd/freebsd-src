/*
 * fs/dcache.c
 *
 * Complete reimplementation
 * (C) 1997 Thomas Schoebel-Theuer,
 * with heavy changes by Linus Torvalds
 */

/*
 * Notes on the allocation strategy:
 *
 * The dcache is a master of the icache - whenever a dcache entry
 * exists, the inode will always exist. "iput()" is done either when
 * the dcache entry is deleted or garbage collected.
 */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/cache.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#define DCACHE_PARANOIA 1
/* #define DCACHE_DEBUG 1 */

spinlock_t dcache_lock __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;

/* Right now the dcache depends on the kernel lock */
#define check_lock()	if (!kernel_locked()) BUG()

static kmem_cache_t *dentry_cache; 

/*
 * This is the single most critical data structure when it comes
 * to the dcache: the hashtable for lookups. Somebody should try
 * to make this good - I've just made it work.
 *
 * This hash-function tries to avoid losing too many bits of hash
 * information, yet avoid using a prime hash-size or similar.
 */
#define D_HASHBITS     d_hash_shift
#define D_HASHMASK     d_hash_mask

static unsigned int d_hash_mask;
static unsigned int d_hash_shift;
static struct list_head *dentry_hashtable;
static LIST_HEAD(dentry_unused);

/* Statistics gathering. */
struct dentry_stat_t dentry_stat = {0, 0, 45, 0,};

/* no dcache_lock, please */
static inline void d_free(struct dentry *dentry)
{
	if (dentry->d_op && dentry->d_op->d_release)
		dentry->d_op->d_release(dentry);
	if (dname_external(dentry)) 
		kfree(dentry->d_name.name);
	kmem_cache_free(dentry_cache, dentry); 
	dentry_stat.nr_dentry--;
}

/*
 * Release the dentry's inode, using the filesystem
 * d_iput() operation if defined.
 * Called with dcache_lock held, drops it.
 */
static inline void dentry_iput(struct dentry * dentry)
{
	struct inode *inode = dentry->d_inode;
	if (inode) {
		dentry->d_inode = NULL;
		list_del_init(&dentry->d_alias);
		spin_unlock(&dcache_lock);
		if (dentry->d_op && dentry->d_op->d_iput)
			dentry->d_op->d_iput(dentry, inode);
		else
			iput(inode);
	} else
		spin_unlock(&dcache_lock);
}

/* 
 * This is dput
 *
 * This is complicated by the fact that we do not want to put
 * dentries that are no longer on any hash chain on the unused
 * list: we'd much rather just get rid of them immediately.
 *
 * However, that implies that we have to traverse the dentry
 * tree upwards to the parents which might _also_ now be
 * scheduled for deletion (it may have been only waiting for
 * its last child to go away).
 *
 * This tail recursion is done by hand as we don't want to depend
 * on the compiler to always get this right (gcc generally doesn't).
 * Real recursion would eat up our stack space.
 */

/*
 * dput - release a dentry
 * @dentry: dentry to release 
 *
 * Release a dentry. This will drop the usage count and if appropriate
 * call the dentry unlink method as well as removing it from the queues and
 * releasing its resources. If the parent dentries were scheduled for release
 * they too may now get deleted.
 *
 * no dcache lock, please.
 */

void dput(struct dentry *dentry)
{
	if (!dentry)
		return;

repeat:
	if (!atomic_dec_and_lock(&dentry->d_count, &dcache_lock))
		return;

	/* dput on a free dentry? */
	if (!list_empty(&dentry->d_lru))
		BUG();
	/*
	 * AV: ->d_delete() is _NOT_ allowed to block now.
	 */
	if (dentry->d_op && dentry->d_op->d_delete) {
		if (dentry->d_op->d_delete(dentry))
			goto unhash_it;
	}
	/* Unreachable? Get rid of it */
	if (list_empty(&dentry->d_hash))
		goto kill_it;
	list_add(&dentry->d_lru, &dentry_unused);
	dentry_stat.nr_unused++;
	spin_unlock(&dcache_lock);
	return;

unhash_it:
	list_del_init(&dentry->d_hash);

kill_it: {
		struct dentry *parent;
		list_del(&dentry->d_child);
		/* drops the lock, at that point nobody can reach this dentry */
		dentry_iput(dentry);
		parent = dentry->d_parent;
		d_free(dentry);
		if (dentry == parent)
			return;
		dentry = parent;
		goto repeat;
	}
}

/**
 * d_invalidate - invalidate a dentry
 * @dentry: dentry to invalidate
 *
 * Try to invalidate the dentry if it turns out to be
 * possible. If there are other dentries that can be
 * reached through this one we can't delete it and we
 * return -EBUSY. On success we return 0.
 *
 * no dcache lock.
 */
 
int d_invalidate(struct dentry * dentry)
{
	/*
	 * If it's already been dropped, return OK.
	 */
	spin_lock(&dcache_lock);
	if (list_empty(&dentry->d_hash)) {
		spin_unlock(&dcache_lock);
		return 0;
	}
	/*
	 * Check whether to do a partial shrink_dcache
	 * to get rid of unused child entries.
	 */
	if (!list_empty(&dentry->d_subdirs)) {
		spin_unlock(&dcache_lock);
		shrink_dcache_parent(dentry);
		spin_lock(&dcache_lock);
	}

	/*
	 * Somebody else still using it?
	 *
	 * If it's a directory, we can't drop it
	 * for fear of somebody re-populating it
	 * with children (even though dropping it
	 * would make it unreachable from the root,
	 * we might still populate it if it was a
	 * working directory or similar).
	 */
	if (atomic_read(&dentry->d_count) > 1) {
		if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode)) {
			spin_unlock(&dcache_lock);
			return -EBUSY;
		}
	}

	list_del_init(&dentry->d_hash);
	spin_unlock(&dcache_lock);
	return 0;
}

/* This should be called _only_ with dcache_lock held */

static inline struct dentry * __dget_locked(struct dentry *dentry)
{
	atomic_inc(&dentry->d_count);
	if (atomic_read(&dentry->d_count) == 1) {
		dentry_stat.nr_unused--;
		list_del_init(&dentry->d_lru);
	}
	return dentry;
}

struct dentry * dget_locked(struct dentry *dentry)
{
	return __dget_locked(dentry);
}

/**
 * d_find_alias - grab a hashed alias of inode
 * @inode: inode in question
 *
 * If inode has a hashed alias - acquire the reference to alias and
 * return it. Otherwise return NULL. Notice that if inode is a directory
 * there can be only one alias and it can be unhashed only if it has
 * no children.
 */

struct dentry * d_find_alias(struct inode *inode)
{
	struct list_head *head, *next, *tmp;
	struct dentry *alias;

	spin_lock(&dcache_lock);
	head = &inode->i_dentry;
	next = inode->i_dentry.next;
	while (next != head) {
		tmp = next;
		next = tmp->next;
		alias = list_entry(tmp, struct dentry, d_alias);
		if (!list_empty(&alias->d_hash)) {
			__dget_locked(alias);
			spin_unlock(&dcache_lock);
			return alias;
		}
	}
	spin_unlock(&dcache_lock);
	return NULL;
}

/*
 *	Try to kill dentries associated with this inode.
 * WARNING: you must own a reference to inode.
 */
void d_prune_aliases(struct inode *inode)
{
	struct list_head *tmp, *head = &inode->i_dentry;
restart:
	spin_lock(&dcache_lock);
	tmp = head;
	while ((tmp = tmp->next) != head) {
		struct dentry *dentry = list_entry(tmp, struct dentry, d_alias);
		if (!atomic_read(&dentry->d_count)) {
			__dget_locked(dentry);
			spin_unlock(&dcache_lock);
			d_drop(dentry);
			dput(dentry);
			goto restart;
		}
	}
	spin_unlock(&dcache_lock);
}

/*
 * Throw away a dentry - free the inode, dput the parent.
 * This requires that the LRU list has already been
 * removed.
 * Called with dcache_lock, drops it and then regains.
 */
static inline void prune_one_dentry(struct dentry * dentry)
{
	struct dentry * parent;

	list_del_init(&dentry->d_hash);
	list_del(&dentry->d_child);
	dentry_iput(dentry);
	parent = dentry->d_parent;
	d_free(dentry);
	if (parent != dentry)
		dput(parent);
	spin_lock(&dcache_lock);
}

/**
 * prune_dcache - shrink the dcache
 * @count: number of entries to try and free
 *
 * Shrink the dcache. This is done when we need
 * more memory, or simply when we need to unmount
 * something (at which point we need to unuse
 * all dentries).
 *
 * This function may fail to free any resources if
 * all the dentries are in use.
 */
 
void prune_dcache(int count)
{
	spin_lock(&dcache_lock);
	for (;;) {
		struct dentry *dentry;
		struct list_head *tmp;

		tmp = dentry_unused.prev;

		if (tmp == &dentry_unused)
			break;
		list_del_init(tmp);
		dentry = list_entry(tmp, struct dentry, d_lru);

		/* If the dentry was recently referenced, don't free it. */
		if (dentry->d_vfs_flags & DCACHE_REFERENCED) {
			dentry->d_vfs_flags &= ~DCACHE_REFERENCED;
			list_add(&dentry->d_lru, &dentry_unused);
			continue;
		}
		dentry_stat.nr_unused--;

		/* Unused dentry with a count? */
		if (atomic_read(&dentry->d_count))
			BUG();

		prune_one_dentry(dentry);
		if (!--count)
			break;
	}
	spin_unlock(&dcache_lock);
}

/*
 * Shrink the dcache for the specified super block.
 * This allows us to unmount a device without disturbing
 * the dcache for the other devices.
 *
 * This implementation makes just two traversals of the
 * unused list.  On the first pass we move the selected
 * dentries to the most recent end, and on the second
 * pass we free them.  The second pass must restart after
 * each dput(), but since the target dentries are all at
 * the end, it's really just a single traversal.
 */

/**
 * shrink_dcache_sb - shrink dcache for a superblock
 * @sb: superblock
 *
 * Shrink the dcache for the specified super block. This
 * is used to free the dcache before unmounting a file
 * system
 */

void shrink_dcache_sb(struct super_block * sb)
{
	struct list_head *tmp, *next;
	struct dentry *dentry;

	/*
	 * Pass one ... move the dentries for the specified
	 * superblock to the most recent end of the unused list.
	 */
	spin_lock(&dcache_lock);
	next = dentry_unused.next;
	while (next != &dentry_unused) {
		tmp = next;
		next = tmp->next;
		dentry = list_entry(tmp, struct dentry, d_lru);
		if (dentry->d_sb != sb)
			continue;
		list_del(tmp);
		list_add(tmp, &dentry_unused);
	}

	/*
	 * Pass two ... free the dentries for this superblock.
	 */
repeat:
	next = dentry_unused.next;
	while (next != &dentry_unused) {
		tmp = next;
		next = tmp->next;
		dentry = list_entry(tmp, struct dentry, d_lru);
		if (dentry->d_sb != sb)
			continue;
		if (atomic_read(&dentry->d_count))
			continue;
		dentry_stat.nr_unused--;
		list_del_init(tmp);
		prune_one_dentry(dentry);
		goto repeat;
	}
	spin_unlock(&dcache_lock);
}

/*
 * Search for at least 1 mount point in the dentry's subdirs.
 * We descend to the next level whenever the d_subdirs
 * list is non-empty and continue searching.
 */
 
/**
 * have_submounts - check for mounts over a dentry
 * @parent: dentry to check.
 *
 * Return true if the parent or its subdirectories contain
 * a mount point
 */
 
int have_submounts(struct dentry *parent)
{
	struct dentry *this_parent = parent;
	struct list_head *next;

	spin_lock(&dcache_lock);
	if (d_mountpoint(parent))
		goto positive;
repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);
		next = tmp->next;
		/* Have we found a mount point ? */
		if (d_mountpoint(dentry))
			goto positive;
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}
	}
	/*
	 * All done at this level ... ascend and resume the search.
	 */
	if (this_parent != parent) {
		next = this_parent->d_child.next; 
		this_parent = this_parent->d_parent;
		goto resume;
	}
	spin_unlock(&dcache_lock);
	return 0; /* No mount points found in tree */
positive:
	spin_unlock(&dcache_lock);
	return 1;
}

/*
 * Search the dentry child list for the specified parent,
 * and move any unused dentries to the end of the unused
 * list for prune_dcache(). We descend to the next level
 * whenever the d_subdirs list is non-empty and continue
 * searching.
 */
static int select_parent(struct dentry * parent)
{
	struct dentry *this_parent = parent;
	struct list_head *next;
	int found = 0;

	spin_lock(&dcache_lock);
repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);
		next = tmp->next;
		if (!atomic_read(&dentry->d_count)) {
			list_del(&dentry->d_lru);
			list_add(&dentry->d_lru, dentry_unused.prev);
			found++;
		}
		/*
		 * Descend a level if the d_subdirs list is non-empty.
		 */
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
#ifdef DCACHE_DEBUG
printk(KERN_DEBUG "select_parent: descending to %s/%s, found=%d\n",
dentry->d_parent->d_name.name, dentry->d_name.name, found);
#endif
			goto repeat;
		}
	}
	/*
	 * All done at this level ... ascend and resume the search.
	 */
	if (this_parent != parent) {
		next = this_parent->d_child.next; 
		this_parent = this_parent->d_parent;
#ifdef DCACHE_DEBUG
printk(KERN_DEBUG "select_parent: ascending to %s/%s, found=%d\n",
this_parent->d_parent->d_name.name, this_parent->d_name.name, found);
#endif
		goto resume;
	}
	spin_unlock(&dcache_lock);
	return found;
}

/**
 * shrink_dcache_parent - prune dcache
 * @parent: parent of entries to prune
 *
 * Prune the dcache to remove unused children of the parent dentry.
 */
 
void shrink_dcache_parent(struct dentry * parent)
{
	int found;

	while ((found = select_parent(parent)) != 0)
		prune_dcache(found);
}

/*
 * This is called from kswapd when we think we need some
 * more memory, but aren't really sure how much. So we
 * carefully try to free a _bit_ of our dcache, but not
 * too much.
 *
 * Priority:
 *   0 - very urgent: shrink everything
 *  ...
 *   6 - base-level: try to shrink a bit.
 */
int shrink_dcache_memory(int priority, unsigned int gfp_mask)
{
	int count = 0;

	/*
	 * Nasty deadlock avoidance.
	 *
	 * ext2_new_block->getblk->GFP->shrink_dcache_memory->prune_dcache->
	 * prune_one_dentry->dput->dentry_iput->iput->inode->i_sb->s_op->
	 * put_inode->ext2_discard_prealloc->ext2_free_blocks->lock_super->
	 * DEADLOCK.
	 *
	 * We should make sure we don't hold the superblock lock over
	 * block allocations, but for now:
	 */
	if (!(gfp_mask & __GFP_FS))
		return 0;

	count = dentry_stat.nr_unused / priority;

	prune_dcache(count);
	return kmem_cache_shrink(dentry_cache);
}

#define NAME_ALLOC_LEN(len)	((len+16) & ~15)

/**
 * d_alloc	-	allocate a dcache entry
 * @parent: parent of entry to allocate
 * @name: qstr of the name
 *
 * Allocates a dentry. It returns %NULL if there is insufficient memory
 * available. On a success the dentry is returned. The name passed in is
 * copied and the copy passed in may be reused after this call.
 */
 
struct dentry * d_alloc(struct dentry * parent, const struct qstr *name)
{
	char * str;
	struct dentry *dentry;

	dentry = kmem_cache_alloc(dentry_cache, GFP_KERNEL); 
	if (!dentry)
		return NULL;

	if (name->len > DNAME_INLINE_LEN-1) {
		str = kmalloc(NAME_ALLOC_LEN(name->len), GFP_KERNEL);
		if (!str) {
			kmem_cache_free(dentry_cache, dentry); 
			return NULL;
		}
	} else
		str = dentry->d_iname; 

	memcpy(str, name->name, name->len);
	str[name->len] = 0;

	atomic_set(&dentry->d_count, 1);
	dentry->d_vfs_flags = 0;
	dentry->d_flags = 0;
	dentry->d_inode = NULL;
	dentry->d_parent = NULL;
	dentry->d_sb = NULL;
	dentry->d_name.name = str;
	dentry->d_name.len = name->len;
	dentry->d_name.hash = name->hash;
	dentry->d_op = NULL;
	dentry->d_fsdata = NULL;
	dentry->d_mounted = 0;
	INIT_LIST_HEAD(&dentry->d_hash);
	INIT_LIST_HEAD(&dentry->d_lru);
	INIT_LIST_HEAD(&dentry->d_subdirs);
	INIT_LIST_HEAD(&dentry->d_alias);
	if (parent) {
		dentry->d_parent = dget(parent);
		dentry->d_sb = parent->d_sb;
		spin_lock(&dcache_lock);
		list_add(&dentry->d_child, &parent->d_subdirs);
		spin_unlock(&dcache_lock);
	} else
		INIT_LIST_HEAD(&dentry->d_child);

	dentry_stat.nr_dentry++;
	return dentry;
}

/**
 * d_instantiate - fill in inode information for a dentry
 * @entry: dentry to complete
 * @inode: inode to attach to this dentry
 *
 * Fill in inode information in the entry.
 *
 * This turns negative dentries into productive full members
 * of society.
 *
 * NOTE! This assumes that the inode count has been incremented
 * (or otherwise set) by the caller to indicate that it is now
 * in use by the dcache.
 */
 
void d_instantiate(struct dentry *entry, struct inode * inode)
{
	if (!list_empty(&entry->d_alias)) BUG();
	spin_lock(&dcache_lock);
	if (inode)
		list_add(&entry->d_alias, &inode->i_dentry);
	entry->d_inode = inode;
	spin_unlock(&dcache_lock);
}

/**
 * d_alloc_root - allocate root dentry
 * @root_inode: inode to allocate the root for
 *
 * Allocate a root ("/") dentry for the inode given. The inode is
 * instantiated and returned. %NULL is returned if there is insufficient
 * memory or the inode passed is %NULL.
 */
 
struct dentry * d_alloc_root(struct inode * root_inode)
{
	struct dentry *res = NULL;

	if (root_inode) {
		res = d_alloc(NULL, &(const struct qstr) { "/", 1, 0 });
		if (res) {
			res->d_sb = root_inode->i_sb;
			res->d_parent = res;
			d_instantiate(res, root_inode);
		}
	}
	return res;
}

static inline struct list_head * d_hash(struct dentry * parent, unsigned long hash)
{
	hash += (unsigned long) parent / L1_CACHE_BYTES;
	hash = hash ^ (hash >> D_HASHBITS);
	return dentry_hashtable + (hash & D_HASHMASK);
}

/**
 * d_lookup - search for a dentry
 * @parent: parent dentry
 * @name: qstr of name we wish to find
 *
 * Searches the children of the parent dentry for the name in question. If
 * the dentry is found its reference count is incremented and the dentry
 * is returned. The caller must use d_put to free the entry when it has
 * finished using it. %NULL is returned on failure.
 */
 
struct dentry * d_lookup(struct dentry * parent, struct qstr * name)
{
	unsigned int len = name->len;
	unsigned int hash = name->hash;
	const unsigned char *str = name->name;
	struct list_head *head = d_hash(parent,hash);
	struct list_head *tmp;

	spin_lock(&dcache_lock);
	tmp = head->next;
	for (;;) {
		struct dentry * dentry = list_entry(tmp, struct dentry, d_hash);
		if (tmp == head)
			break;
		tmp = tmp->next;
		if (dentry->d_name.hash != hash)
			continue;
		if (dentry->d_parent != parent)
			continue;
		if (parent->d_op && parent->d_op->d_compare) {
			if (parent->d_op->d_compare(parent, &dentry->d_name, name))
				continue;
		} else {
			if (dentry->d_name.len != len)
				continue;
			if (memcmp(dentry->d_name.name, str, len))
				continue;
		}
		__dget_locked(dentry);
		dentry->d_vfs_flags |= DCACHE_REFERENCED;
		spin_unlock(&dcache_lock);
		return dentry;
	}
	spin_unlock(&dcache_lock);
	return NULL;
}

/**
 * d_validate - verify dentry provided from insecure source
 * @dentry: The dentry alleged to be valid child of @dparent
 * @dparent: The parent dentry (known to be valid)
 * @hash: Hash of the dentry
 * @len: Length of the name
 *
 * An insecure source has sent us a dentry, here we verify it and dget() it.
 * This is used by ncpfs in its readdir implementation.
 * Zero is returned in the dentry is invalid.
 */
 
int d_validate(struct dentry *dentry, struct dentry *dparent)
{
	unsigned long dent_addr = (unsigned long) dentry;
	unsigned long min_addr = PAGE_OFFSET;
	unsigned long align_mask = 0x0F;
	struct list_head *base, *lhp;

	if (dent_addr < min_addr)
		goto out;
	if (dent_addr > (unsigned long)high_memory - sizeof(struct dentry))
		goto out;
	if (dent_addr & align_mask)
		goto out;
	if ((!kern_addr_valid(dent_addr)) || (!kern_addr_valid(dent_addr -1 +
						sizeof(struct dentry))))
		goto out;

	if (dentry->d_parent != dparent)
		goto out;

	spin_lock(&dcache_lock);
	lhp = base = d_hash(dparent, dentry->d_name.hash);
	while ((lhp = lhp->next) != base) {
		if (dentry == list_entry(lhp, struct dentry, d_hash)) {
			__dget_locked(dentry);
			spin_unlock(&dcache_lock);
			return 1;
		}
	}
	spin_unlock(&dcache_lock);
out:
	return 0;
}

/*
 * When a file is deleted, we have two options:
 * - turn this dentry into a negative dentry
 * - unhash this dentry and free it.
 *
 * Usually, we want to just turn this into
 * a negative dentry, but if anybody else is
 * currently using the dentry or the inode
 * we can't do that and we fall back on removing
 * it from the hash queues and waiting for
 * it to be deleted later when it has no users
 */
 
/**
 * d_delete - delete a dentry
 * @dentry: The dentry to delete
 *
 * Turn the dentry into a negative dentry if possible, otherwise
 * remove it from the hash queues so it can be deleted later
 */
 
void d_delete(struct dentry * dentry)
{
	/*
	 * Are we the only user?
	 */
	spin_lock(&dcache_lock);
	if (atomic_read(&dentry->d_count) == 1) {
		dentry_iput(dentry);
		return;
	}
	spin_unlock(&dcache_lock);

	/*
	 * If not, just drop the dentry and let dput
	 * pick up the tab..
	 */
	d_drop(dentry);
}

/**
 * d_rehash	- add an entry back to the hash
 * @entry: dentry to add to the hash
 *
 * Adds a dentry to the hash according to its name.
 */
 
void d_rehash(struct dentry * entry)
{
	struct list_head *list = d_hash(entry->d_parent, entry->d_name.hash);
	if (!list_empty(&entry->d_hash)) BUG();
	spin_lock(&dcache_lock);
	list_add(&entry->d_hash, list);
	spin_unlock(&dcache_lock);
}

#define do_switch(x,y) do { \
	__typeof__ (x) __tmp = x; \
	x = y; y = __tmp; } while (0)

/*
 * When switching names, the actual string doesn't strictly have to
 * be preserved in the target - because we're dropping the target
 * anyway. As such, we can just do a simple memcpy() to copy over
 * the new name before we switch.
 *
 * Note that we have to be a lot more careful about getting the hash
 * switched - we have to switch the hash value properly even if it
 * then no longer matches the actual (corrupted) string of the target.
 * The hash value has to match the hash queue that the dentry is on..
 */
static inline void switch_names(struct dentry * dentry, struct dentry * target)
{
	const unsigned char *old_name, *new_name;

	check_lock();
	memcpy(dentry->d_iname, target->d_iname, DNAME_INLINE_LEN); 
	old_name = target->d_name.name;
	new_name = dentry->d_name.name;
	if (old_name == target->d_iname)
		old_name = dentry->d_iname;
	if (new_name == dentry->d_iname)
		new_name = target->d_iname;
	target->d_name.name = new_name;
	dentry->d_name.name = old_name;
}

/*
 * We cannibalize "target" when moving dentry on top of it,
 * because it's going to be thrown away anyway. We could be more
 * polite about it, though.
 *
 * This forceful removal will result in ugly /proc output if
 * somebody holds a file open that got deleted due to a rename.
 * We could be nicer about the deleted file, and let it show
 * up under the name it got deleted rather than the name that
 * deleted it.
 *
 * Careful with the hash switch. The hash switch depends on
 * the fact that any list-entry can be a head of the list.
 * Think about it.
 */
 
/**
 * d_move - move a dentry
 * @dentry: entry to move
 * @target: new dentry
 *
 * Update the dcache to reflect the move of a file name. Negative
 * dcache entries should not be moved in this way.
 */
  
void d_move(struct dentry * dentry, struct dentry * target)
{
	check_lock();

	if (!dentry->d_inode)
		printk(KERN_WARNING "VFS: moving negative dcache entry\n");

	spin_lock(&dcache_lock);
	/* Move the dentry to the target hash queue */
	list_del(&dentry->d_hash);
	list_add(&dentry->d_hash, &target->d_hash);

	/* Unhash the target: dput() will then get rid of it */
	list_del_init(&target->d_hash);

	list_del(&dentry->d_child);
	list_del(&target->d_child);

	/* Switch the parents and the names.. */
	switch_names(dentry, target);
	do_switch(dentry->d_parent, target->d_parent);
	do_switch(dentry->d_name.len, target->d_name.len);
	do_switch(dentry->d_name.hash, target->d_name.hash);

	/* And add them back to the (new) parent lists */
	list_add(&target->d_child, &target->d_parent->d_subdirs);
	list_add(&dentry->d_child, &dentry->d_parent->d_subdirs);
	spin_unlock(&dcache_lock);
}

/**
 * d_path - return the path of a dentry
 * @dentry: dentry to report
 * @vfsmnt: vfsmnt to which the dentry belongs
 * @root: root dentry
 * @rootmnt: vfsmnt to which the root dentry belongs
 * @buffer: buffer to return value in
 * @buflen: buffer length
 *
 * Convert a dentry into an ASCII path name. If the entry has been deleted
 * the string " (deleted)" is appended. Note that this is ambiguous. Returns
 * the buffer.
 *
 * "buflen" should be %PAGE_SIZE or more. Caller holds the dcache_lock.
 */
char * __d_path(struct dentry *dentry, struct vfsmount *vfsmnt,
		struct dentry *root, struct vfsmount *rootmnt,
		char *buffer, int buflen)
{
	char * end = buffer+buflen;
	char * retval;
	int namelen;

	*--end = '\0';
	buflen--;
	if (!IS_ROOT(dentry) && list_empty(&dentry->d_hash)) {
		buflen -= 10;
		end -= 10;
		memcpy(end, " (deleted)", 10);
	}

	/* Get '/' right */
	retval = end-1;
	*retval = '/';

	for (;;) {
		struct dentry * parent;

		if (dentry == root && vfsmnt == rootmnt)
			break;
		if (dentry == vfsmnt->mnt_root || IS_ROOT(dentry)) {
			/* Global root? */
			if (vfsmnt->mnt_parent == vfsmnt)
				goto global_root;
			dentry = vfsmnt->mnt_mountpoint;
			vfsmnt = vfsmnt->mnt_parent;
			continue;
		}
		parent = dentry->d_parent;
		namelen = dentry->d_name.len;
		buflen -= namelen + 1;
		if (buflen < 0)
			return ERR_PTR(-ENAMETOOLONG);
		end -= namelen;
		memcpy(end, dentry->d_name.name, namelen);
		*--end = '/';
		retval = end;
		dentry = parent;
	}

	return retval;

global_root:
	namelen = dentry->d_name.len;
	buflen -= namelen;
	if (buflen >= 0) {
		retval -= namelen-1;	/* hit the slash */
		memcpy(retval, dentry->d_name.name, namelen);
	} else
		retval = ERR_PTR(-ENAMETOOLONG);
	return retval;
}

/*
 * NOTE! The user-level library version returns a
 * character pointer. The kernel system call just
 * returns the length of the buffer filled (which
 * includes the ending '\0' character), or a negative
 * error value. So libc would do something like
 *
 *	char *getcwd(char * buf, size_t size)
 *	{
 *		int retval;
 *
 *		retval = sys_getcwd(buf, size);
 *		if (retval >= 0)
 *			return buf;
 *		errno = -retval;
 *		return NULL;
 *	}
 */
asmlinkage long sys_getcwd(char *buf, unsigned long size)
{
	int error;
	struct vfsmount *pwdmnt, *rootmnt;
	struct dentry *pwd, *root;
	char *page = (char *) __get_free_page(GFP_USER);

	if (!page)
		return -ENOMEM;

	read_lock(&current->fs->lock);
	pwdmnt = mntget(current->fs->pwdmnt);
	pwd = dget(current->fs->pwd);
	rootmnt = mntget(current->fs->rootmnt);
	root = dget(current->fs->root);
	read_unlock(&current->fs->lock);

	error = -ENOENT;
	/* Has the current directory has been unlinked? */
	spin_lock(&dcache_lock);
	if (pwd->d_parent == pwd || !list_empty(&pwd->d_hash)) {
		unsigned long len;
		char * cwd;

		cwd = __d_path(pwd, pwdmnt, root, rootmnt, page, PAGE_SIZE);
		spin_unlock(&dcache_lock);

		error = PTR_ERR(cwd);
		if (IS_ERR(cwd))
			goto out;

		error = -ERANGE;
		len = PAGE_SIZE + page - cwd;
		if (len <= size) {
			error = len;
			if (copy_to_user(buf, cwd, len))
				error = -EFAULT;
		}
	} else
		spin_unlock(&dcache_lock);

out:
	dput(pwd);
	mntput(pwdmnt);
	dput(root);
	mntput(rootmnt);
	free_page((unsigned long) page);
	return error;
}

/*
 * Test whether new_dentry is a subdirectory of old_dentry.
 *
 * Trivially implemented using the dcache structure
 */

/**
 * is_subdir - is new dentry a subdirectory of old_dentry
 * @new_dentry: new dentry
 * @old_dentry: old dentry
 *
 * Returns 1 if new_dentry is a subdirectory of the parent (at any depth).
 * Returns 0 otherwise.
 */
  
int is_subdir(struct dentry * new_dentry, struct dentry * old_dentry)
{
	int result;

	result = 0;
	for (;;) {
		if (new_dentry != old_dentry) {
			struct dentry * parent = new_dentry->d_parent;
			if (parent == new_dentry)
				break;
			new_dentry = parent;
			continue;
		}
		result = 1;
		break;
	}
	return result;
}

void d_genocide(struct dentry *root)
{
	struct dentry *this_parent = root;
	struct list_head *next;

	spin_lock(&dcache_lock);
repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);
		next = tmp->next;
		if (d_unhashed(dentry)||!dentry->d_inode)
			continue;
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}
		atomic_dec(&dentry->d_count);
	}
	if (this_parent != root) {
		next = this_parent->d_child.next; 
		atomic_dec(&this_parent->d_count);
		this_parent = this_parent->d_parent;
		goto resume;
	}
	spin_unlock(&dcache_lock);
}

/**
 * find_inode_number - check for dentry with name
 * @dir: directory to check
 * @name: Name to find.
 *
 * Check whether a dentry already exists for the given name,
 * and return the inode number if it has an inode. Otherwise
 * 0 is returned.
 *
 * This routine is used to post-process directory listings for
 * filesystems using synthetic inode numbers, and is necessary
 * to keep getcwd() working.
 */
 
ino_t find_inode_number(struct dentry *dir, struct qstr *name)
{
	struct dentry * dentry;
	ino_t ino = 0;

	/*
	 * Check for a fs-specific hash function. Note that we must
	 * calculate the standard hash first, as the d_op->d_hash()
	 * routine may choose to leave the hash value unchanged.
	 */
	name->hash = full_name_hash(name->name, name->len);
	if (dir->d_op && dir->d_op->d_hash)
	{
		if (dir->d_op->d_hash(dir, name) != 0)
			goto out;
	}

	dentry = d_lookup(dir, name);
	if (dentry)
	{
		if (dentry->d_inode)
			ino = dentry->d_inode->i_ino;
		dput(dentry);
	}
out:
	return ino;
}

static void __init dcache_init(unsigned long mempages)
{
	struct list_head *d;
	unsigned long order;
	unsigned int nr_hash;
	int i;

	/* 
	 * A constructor could be added for stable state like the lists,
	 * but it is probably not worth it because of the cache nature
	 * of the dcache. 
	 * If fragmentation is too bad then the SLAB_HWCACHE_ALIGN
	 * flag could be removed here, to hint to the allocator that
	 * it should not try to get multiple page regions.  
	 */
	dentry_cache = kmem_cache_create("dentry_cache",
					 sizeof(struct dentry),
					 0,
					 SLAB_HWCACHE_ALIGN,
					 NULL, NULL);
	if (!dentry_cache)
		panic("Cannot create dentry cache");

#if PAGE_SHIFT < 13
	mempages >>= (13 - PAGE_SHIFT);
#endif
	mempages *= sizeof(struct list_head);
	for (order = 0; ((1UL << order) << PAGE_SHIFT) < mempages; order++)
		;

	do {
		unsigned long tmp;

		nr_hash = (1UL << order) * PAGE_SIZE /
			sizeof(struct list_head);
		d_hash_mask = (nr_hash - 1);

		tmp = nr_hash;
		d_hash_shift = 0;
		while ((tmp >>= 1UL) != 0UL)
			d_hash_shift++;

		dentry_hashtable = (struct list_head *)
			__get_free_pages(GFP_ATOMIC, order);
	} while (dentry_hashtable == NULL && --order >= 0);

	printk(KERN_INFO "Dentry cache hash table entries: %d (order: %ld, %ld bytes)\n",
			nr_hash, order, (PAGE_SIZE << order));

	if (!dentry_hashtable)
		panic("Failed to allocate dcache hash table\n");

	d = dentry_hashtable;
	i = nr_hash;
	do {
		INIT_LIST_HEAD(d);
		d++;
		i--;
	} while (i);
}

static void init_buffer_head(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
	{
		struct buffer_head * bh = (struct buffer_head *) foo;

		memset(bh, 0, sizeof(*bh));
		init_waitqueue_head(&bh->b_wait);
	}
}

/* SLAB cache for __getname() consumers */
kmem_cache_t *names_cachep;

/* SLAB cache for file structures */
kmem_cache_t *filp_cachep;

/* SLAB cache for dquot structures */
kmem_cache_t *dquot_cachep;

/* SLAB cache for buffer_head structures */
kmem_cache_t *bh_cachep;
EXPORT_SYMBOL(bh_cachep);

extern void bdev_cache_init(void);
extern void cdev_cache_init(void);
extern void iobuf_cache_init(void);

void __init vfs_caches_init(unsigned long mempages)
{
	bh_cachep = kmem_cache_create("buffer_head",
			sizeof(struct buffer_head), 0,
			SLAB_HWCACHE_ALIGN, init_buffer_head, NULL);
	if(!bh_cachep)
		panic("Cannot create buffer head SLAB cache");

	names_cachep = kmem_cache_create("names_cache", 
			PATH_MAX, 0, 
			SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!names_cachep)
		panic("Cannot create names SLAB cache");

	filp_cachep = kmem_cache_create("filp", 
			sizeof(struct file), 0,
			SLAB_HWCACHE_ALIGN, NULL, NULL);
	if(!filp_cachep)
		panic("Cannot create filp SLAB cache");

#if defined (CONFIG_QUOTA)
	dquot_cachep = kmem_cache_create("dquot", 
			sizeof(struct dquot), sizeof(unsigned long) * 4,
			SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!dquot_cachep)
		panic("Cannot create dquot SLAB cache");
#endif

	dcache_init(mempages);
	inode_init(mempages);
	files_init(mempages); 
	mnt_init(mempages);
	bdev_cache_init();
	cdev_cache_init();
	iobuf_cache_init();
}
