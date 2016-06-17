/*
 * Implementation of the diskquota system for the LINUX operating
 * system. QUOTA is implemented using the BSD system call interface as
 * the means of communication with the user level. Currently only the
 * ext2 filesystem has support for disk quotas. Other filesystems may
 * be added in the future. This file contains the generic routines
 * called by the different filesystems on allocation of an inode or
 * block. These routines take care of the administration needed to
 * have a consistent diskquota tracking system. The ideas of both
 * user and group quotas are based on the Melbourne quota system as
 * used on BSD derived systems. The internal implementation is 
 * based on one of the several variants of the LINUX inode-subsystem
 * with added complexity of the diskquota system.
 * 
 * Version: $Id: dquot.c,v 6.3 1996/11/17 18:35:34 mvw Exp mvw $
 * 
 * Author:	Marco van Wieringen <mvw@planets.elm.net>
 *
 * Fixes:   Dmitry Gorodchanin <pgmdsg@ibi.com>, 11 Feb 96
 *
 *		Revised list management to avoid races
 *		-- Bill Hawes, <whawes@star.net>, 9/98
 *
 *		Fixed races in dquot_transfer(), dqget() and dquot_alloc_...().
 *		As the consequence the locking was moved from dquot_decr_...(),
 *		dquot_incr_...() to calling functions.
 *		invalidate_dquots() now writes modified dquots.
 *		Serialized quota_off() and quota_on() for mount point.
 *		Fixed a few bugs in grow_dquots().
 *		Fixed deadlock in write_dquot() - we no longer account quotas on
 *		quota files
 *		remove_dquot_ref() moved to inode.c - it now traverses through inodes
 *		add_dquot_ref() restarts after blocking
 *		Added check for bogus uid and fixed check for group in quotactl.
 *		Jan Kara, <jack@suse.cz>, sponsored by SuSE CR, 10-11/99
 *
 *		Used struct list_head instead of own list struct
 *		Invalidation of referenced dquots is no longer possible
 *		Improved free_dquots list management
 *		Quota and i_blocks are now updated in one place to avoid races
 *		Warnings are now delayed so we won't block in critical section
 *		Write updated not to require dquot lock
 *		Jan Kara, <jack@suse.cz>, 9/2000
 *
 *		Added dynamic quota structure allocation
 *		Jan Kara <jack@suse.cz> 12/2000
 *
 *		Rewritten quota interface. Implemented new quota format and
 *		formats registering.
 *		Jan Kara, <jack@suse.cz>, 2001,2002
 *
 * (C) Copyright 1994 - 1997 Marco van Wieringen 
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/tty.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <asm/uaccess.h>

static char *quotatypes[] = INITQFNAMES;
static struct quota_format_type *quota_formats;	/* List of registered formats */

int register_quota_format(struct quota_format_type *fmt)
{
	lock_kernel();
	fmt->qf_next = quota_formats;
	quota_formats = fmt;
	unlock_kernel();
	return 0;
}

void unregister_quota_format(struct quota_format_type *fmt)
{
	struct quota_format_type **actqf;

	lock_kernel();
	for (actqf = &quota_formats; *actqf && *actqf != fmt; actqf = &(*actqf)->qf_next);
	if (*actqf)
		*actqf = (*actqf)->qf_next;
	unlock_kernel();
}

static struct quota_format_type *find_quota_format(int id)
{
	struct quota_format_type *actqf;

	lock_kernel();
	for (actqf = quota_formats; actqf && actqf->qf_fmt_id != id; actqf = actqf->qf_next);
	if (actqf && !try_inc_mod_count(actqf->qf_owner))
		actqf = NULL;
	unlock_kernel();
	return actqf;
}

static void put_quota_format(struct quota_format_type *fmt)
{
	if (fmt->qf_owner)
		__MOD_DEC_USE_COUNT(fmt->qf_owner);
}

/*
 * Dquot List Management:
 * The quota code uses three lists for dquot management: the inuse_list,
 * free_dquots, and dquot_hash[] array. A single dquot structure may be
 * on all three lists, depending on its current state.
 *
 * All dquots are placed to the end of inuse_list when first created, and this
 * list is used for the sync and invalidate operations, which must look
 * at every dquot.
 *
 * Unused dquots (dq_count == 0) are added to the free_dquots list when freed,
 * and this list is searched whenever we need an available dquot.  Dquots are
 * removed from the list as soon as they are used again, and
 * dqstats.free_dquots gives the number of dquots on the list. When
 * dquot is invalidated it's completely released from memory.
 *
 * Dquots with a specific identity (device, type and id) are placed on
 * one of the dquot_hash[] hash chains. The provides an efficient search
 * mechanism to locate a specific dquot.
 */

/*
 * Note that any operation which operates on dquot data (ie. dq_dqb) mustn't
 * block while it's updating/reading it. Otherwise races would occur.
 *
 * Locked dquots might not be referenced in inodes - operations like
 * add_dquot_space() does dqduplicate() and would complain. Currently
 * dquot it locked only once in its existence - when it's being read
 * to memory on first dqget() and at that time it can't be referenced
 * from inode. Write operations on dquots don't hold dquot lock as they
 * copy data to internal buffers before writing anyway and copying as well
 * as any data update should be atomic. Also nobody can change used
 * entries in dquot structure as this is done only when quota is destroyed
 * and invalidate_dquots() waits for dquot to have dq_count == 0.
 */

static LIST_HEAD(inuse_list);
static LIST_HEAD(free_dquots);
static struct list_head dquot_hash[NR_DQHASH];

static void dqput(struct dquot *);
static struct dquot *dqduplicate(struct dquot *);

static inline void get_dquot_ref(struct dquot *dquot)
{
	dquot->dq_count++;
}

static inline void put_dquot_ref(struct dquot *dquot)
{
	dquot->dq_count--;
}

static inline void get_dquot_dup_ref(struct dquot *dquot)
{
	dquot->dq_dup_ref++;
}

static inline void put_dquot_dup_ref(struct dquot *dquot)
{
	dquot->dq_dup_ref--;
}

static inline int const hashfn(struct super_block *sb, unsigned int id, int type)
{
	return((HASHDEV(sb->s_dev) ^ id) * (MAXQUOTAS - type)) % NR_DQHASH;
}

static inline void insert_dquot_hash(struct dquot *dquot)
{
	struct list_head *head = dquot_hash + hashfn(dquot->dq_sb, dquot->dq_id, dquot->dq_type);
	list_add(&dquot->dq_hash, head);
}

static inline void remove_dquot_hash(struct dquot *dquot)
{
	list_del(&dquot->dq_hash);
	INIT_LIST_HEAD(&dquot->dq_hash);
}

static inline struct dquot *find_dquot(unsigned int hashent, struct super_block *sb, unsigned int id, int type)
{
	struct list_head *head;
	struct dquot *dquot;

	for (head = dquot_hash[hashent].next; head != dquot_hash+hashent; head = head->next) {
		dquot = list_entry(head, struct dquot, dq_hash);
		if (dquot->dq_sb == sb && dquot->dq_id == id && dquot->dq_type == type)
			return dquot;
	}
	return NODQUOT;
}

/* Add a dquot to the head of the free list */
static inline void put_dquot_head(struct dquot *dquot)
{
	list_add(&dquot->dq_free, &free_dquots);
	dqstats.free_dquots++;
}

/* Add a dquot to the tail of the free list */
static inline void put_dquot_last(struct dquot *dquot)
{
	list_add(&dquot->dq_free, free_dquots.prev);
	dqstats.free_dquots++;
}

/* Move dquot to the head of free list (it must be already on it) */
static inline void move_dquot_head(struct dquot *dquot)
{
	list_del(&dquot->dq_free);
	list_add(&dquot->dq_free, &free_dquots);
}

static inline void remove_free_dquot(struct dquot *dquot)
{
	if (list_empty(&dquot->dq_free))
		return;
	list_del(&dquot->dq_free);
	INIT_LIST_HEAD(&dquot->dq_free);
	dqstats.free_dquots--;
}

static inline void put_inuse(struct dquot *dquot)
{
	/* We add to the back of inuse list so we don't have to restart
	 * when traversing this list and we block */
	list_add(&dquot->dq_inuse, inuse_list.prev);
	dqstats.allocated_dquots++;
}

static inline void remove_inuse(struct dquot *dquot)
{
	dqstats.allocated_dquots--;
	list_del(&dquot->dq_inuse);
}

static void __wait_on_dquot(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_lock, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_flags & DQ_LOCKED) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_lock, &wait);
	current->state = TASK_RUNNING;
}

static inline void wait_on_dquot(struct dquot *dquot)
{
	if (dquot->dq_flags & DQ_LOCKED)
		__wait_on_dquot(dquot);
}

static inline void lock_dquot(struct dquot *dquot)
{
	wait_on_dquot(dquot);
	dquot->dq_flags |= DQ_LOCKED;
}

static inline void unlock_dquot(struct dquot *dquot)
{
	dquot->dq_flags &= ~DQ_LOCKED;
	wake_up(&dquot->dq_wait_lock);
}

/* Wait for dquot to be unused */
static void __wait_dquot_unused(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_free, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_count) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_free, &wait);
	current->state = TASK_RUNNING;
}

/* Wait for all duplicated dquot references to be dropped */
static void __wait_dup_drop(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_free, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_dup_ref) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_free, &wait);
	current->state = TASK_RUNNING;
}

static int read_dqblk(struct dquot *dquot)
{
	int ret;
	struct quota_info *dqopt = sb_dqopt(dquot->dq_sb);

	lock_dquot(dquot);
	down(&dqopt->dqio_sem);
	ret = dqopt->ops[dquot->dq_type]->read_dqblk(dquot);
	up(&dqopt->dqio_sem);
	unlock_dquot(dquot);
	return ret;
}

static int commit_dqblk(struct dquot *dquot)
{
	int ret;
	struct quota_info *dqopt = sb_dqopt(dquot->dq_sb);

	down(&dqopt->dqio_sem);
	ret = dqopt->ops[dquot->dq_type]->commit_dqblk(dquot);
	up(&dqopt->dqio_sem);
	return ret;
}

/* Invalidate all dquots on the list, wait for all users. Note that this function is called
 * after quota is disabled so no new quota might be created. As we only insert to the end of
 * inuse list, we don't have to restart searching... */
static void invalidate_dquots(struct super_block *sb, int type)
{
	struct dquot *dquot;
	struct list_head *head;

restart:
	list_for_each(head, &inuse_list) {
		dquot = list_entry(head, struct dquot, dq_inuse);
		if (dquot->dq_sb != sb)
			continue;
		if (dquot->dq_type != type)
			continue;
		dquot->dq_flags |= DQ_INVAL;
		if (dquot->dq_count)
			/*
			 *  Wait for any users of quota. As we have already cleared the flags in
			 *  superblock and cleared all pointers from inodes we are assured
			 *  that there will be no new users of this quota.
			 */
			__wait_dquot_unused(dquot);
		/* Quota now have no users and it has been written on last dqput() */
		remove_dquot_hash(dquot);
		remove_free_dquot(dquot);
		remove_inuse(dquot);
		kmem_cache_free(dquot_cachep, dquot);
		goto restart;
	}
}

static int vfs_quota_sync(struct super_block *sb, int type)
{
	struct list_head *head;
	struct dquot *dquot;
	struct quota_info *dqopt = sb_dqopt(sb);
	int cnt;

restart:
	list_for_each(head, &inuse_list) {
		dquot = list_entry(head, struct dquot, dq_inuse);
		if (sb && dquot->dq_sb != sb)
			continue;
                if (type != -1 && dquot->dq_type != type)
			continue;
		if (!dquot->dq_sb)	/* Invalidated? */
			continue;
		if (!dquot_dirty(dquot) && !(dquot->dq_flags & DQ_LOCKED))
			continue;
		/* Get reference to quota so it won't be invalidated. get_dquot_ref()
		 * is enough since if dquot is locked/modified it can't be
		 * on the free list */
		get_dquot_ref(dquot);
		dqstats.lookups++;
		if (dquot->dq_flags & DQ_LOCKED)
			wait_on_dquot(dquot);
		if (dquot_dirty(dquot))
			sb->dq_op->write_dquot(dquot);
		dqput(dquot);
		goto restart;
	}
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if ((cnt == type || type == -1) && sb_has_quota_enabled(sb, cnt))
			dqopt->info[cnt].dqi_flags &= ~DQF_ANY_DQUOT_DIRTY;
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if ((cnt == type || type == -1) && sb_has_quota_enabled(sb, cnt) && info_dirty(&dqopt->info[cnt]))
			dqopt->ops[cnt]->write_file_info(sb, cnt);
	dqstats.syncs++;

	return 0;
}

static struct super_block *get_super_to_sync(int type)
{
	struct list_head *head;
	int cnt, dirty;

restart:
	spin_lock(&sb_lock);
	list_for_each(head, &super_blocks) {
		struct super_block *sb = list_entry(head, struct super_block, s_list);

		for (cnt = 0, dirty = 0; cnt < MAXQUOTAS; cnt++)
			if ((type == cnt || type == -1) && sb_has_quota_enabled(sb, cnt)
			    && sb_dqopt(sb)->info[cnt].dqi_flags & DQF_ANY_DQUOT_DIRTY)
				dirty = 1;
		if (!dirty)
			continue;
		sb->s_count++;
		spin_unlock(&sb_lock);
		down_read(&sb->s_umount);
		if (!sb->s_root) {
			drop_super(sb);
			goto restart;
		}
		return sb;
	}
	spin_unlock(&sb_lock);
	return NULL;
}

void sync_dquots_dev(kdev_t dev, int type)
{
	struct super_block *sb;

	if (dev) {
		if ((sb = get_super(dev))) {
			lock_kernel();
			if (sb->s_qcop->quota_sync)
				sb->s_qcop->quota_sync(sb, type);
			unlock_kernel();
			drop_super(sb);
		}
	}
	else {
		while ((sb = get_super_to_sync(type))) {
			lock_kernel();
			if (sb->s_qcop->quota_sync)
				sb->s_qcop->quota_sync(sb, type);
			unlock_kernel();
			drop_super(sb);
		}
	}
}

void sync_dquots_sb(struct super_block *sb, int type)
{
	lock_kernel();
	if (sb->s_qcop->quota_sync)
		sb->s_qcop->quota_sync(sb, type);
	unlock_kernel();
}

/* Free unused dquots from cache */
static void prune_dqcache(int count)
{
	struct list_head *head;
	struct dquot *dquot;

	head = free_dquots.prev;
	while (head != &free_dquots && count) {
		dquot = list_entry(head, struct dquot, dq_free);
		remove_dquot_hash(dquot);
		remove_free_dquot(dquot);
		remove_inuse(dquot);
		kmem_cache_free(dquot_cachep, dquot);
		count--;
		head = free_dquots.prev;
	}
}

/*
 * This is called from kswapd when we think we need some
 * more memory, but aren't really sure how much. So we
 * carefully try to free a _bit_ of our dqcache, but not
 * too much.
 *
 * Priority:
 *   1 - very urgent: shrink everything
 *   ...
 *   6 - base-level: try to shrink a bit.
 */

int shrink_dqcache_memory(int priority, unsigned int gfp_mask)
{
	int count = 0;

	lock_kernel();
	count = dqstats.free_dquots / priority;
	prune_dqcache(count);
	unlock_kernel();
	return kmem_cache_shrink(dquot_cachep);
}

/*
 * Put reference to dquot
 * NOTE: If you change this function please check whether dqput_blocks() works right...
 */
static void dqput(struct dquot *dquot)
{
	if (!dquot)
		return;
#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_count) {
		printk("VFS: dqput: trying to free free dquot\n");
		printk("VFS: device %s, dquot of %s %d\n",
			kdevname(dquot->dq_dev), quotatypes[dquot->dq_type],
			dquot->dq_id);
		return;
	}
#endif

	dqstats.drops++;
we_slept:
	if (dquot->dq_dup_ref && dquot->dq_count - dquot->dq_dup_ref <= 1) {	/* Last unduplicated reference? */
		__wait_dup_drop(dquot);
		goto we_slept;
	}
	if (dquot->dq_count > 1) {
		/* We have more than one user... We can simply decrement use count */
		put_dquot_ref(dquot);
		return;
	}
	if (dquot_dirty(dquot)) {
		dquot->dq_sb->dq_op->write_dquot(dquot);
		goto we_slept;
	}

	/* sanity check */
	if (!list_empty(&dquot->dq_free)) {
		printk(KERN_ERR "dqput: dquot already on free list??\n");
		put_dquot_ref(dquot);
		return;
	}
	put_dquot_ref(dquot);
	/* If dquot is going to be invalidated invalidate_dquots() is going to free it so */
	if (!(dquot->dq_flags & DQ_INVAL))
		put_dquot_last(dquot);	/* Place at end of LRU free queue */
	wake_up(&dquot->dq_wait_free);
}

static struct dquot *get_empty_dquot(struct super_block *sb, int type)
{
	struct dquot *dquot;

	dquot = kmem_cache_alloc(dquot_cachep, SLAB_KERNEL);
	if(!dquot)
		return NODQUOT;

	memset((caddr_t)dquot, 0, sizeof(struct dquot));
	init_waitqueue_head(&dquot->dq_wait_free);
	init_waitqueue_head(&dquot->dq_wait_lock);
	INIT_LIST_HEAD(&dquot->dq_free);
	INIT_LIST_HEAD(&dquot->dq_inuse);
	INIT_LIST_HEAD(&dquot->dq_hash);
	dquot->dq_sb = sb;
	dquot->dq_dev = sb->s_dev;
	dquot->dq_type = type;
	dquot->dq_count = 1;
	/* all dquots go on the inuse_list */
	put_inuse(dquot);

	return dquot;
}

static struct dquot *dqget(struct super_block *sb, unsigned int id, int type)
{
	unsigned int hashent = hashfn(sb, id, type);
	struct dquot *dquot, *empty = NODQUOT;
	struct quota_info *dqopt = sb_dqopt(sb);

we_slept:
        if (!is_enabled(dqopt, type)) {
		if (empty)
			dqput(empty);
                return NODQUOT;
	}

	if ((dquot = find_dquot(hashent, sb, id, type)) == NODQUOT) {
		if (empty == NODQUOT) {
			if ((empty = get_empty_dquot(sb, type)) == NODQUOT)
				schedule();	/* Try to wait for a moment... */
			goto we_slept;
		}
		dquot = empty;
		dquot->dq_id = id;
		/* hash it first so it can be found */
		insert_dquot_hash(dquot);
		read_dqblk(dquot);
	} else {
		if (!dquot->dq_count)
			remove_free_dquot(dquot);
		get_dquot_ref(dquot);
		dqstats.cache_hits++;
		wait_on_dquot(dquot);
		if (empty)
			dqput(empty);
	}

	if (!dquot->dq_sb) {	/* Has somebody invalidated entry under us? */
		printk(KERN_ERR "VFS: dqget(): Quota invalidated in dqget()!\n");
		dqput(dquot);
		return NODQUOT;
	}
	dqstats.lookups++;

	return dquot;
}

/* Duplicate reference to dquot got from inode */
static struct dquot *dqduplicate(struct dquot *dquot)
{
	if (dquot == NODQUOT)
		return NODQUOT;
	get_dquot_ref(dquot);
	if (!dquot->dq_sb) {
		printk(KERN_ERR "VFS: dqduplicate(): Invalidated quota to be duplicated!\n");
		put_dquot_ref(dquot);
		return NODQUOT;
	}
	if (dquot->dq_flags & DQ_LOCKED)
		printk(KERN_ERR "VFS: dqduplicate(): Locked quota to be duplicated!\n");
	get_dquot_dup_ref(dquot);
	dqstats.lookups++;

	return dquot;
}

/* Put duplicated reference */
static void dqputduplicate(struct dquot *dquot)
{
	if (!dquot->dq_dup_ref) {
		printk(KERN_ERR "VFS: dqputduplicate(): Duplicated dquot put without duplicate reference.\n");
		return;
	}
	put_dquot_dup_ref(dquot);
	if (!dquot->dq_dup_ref)
		wake_up(&dquot->dq_wait_free);
	put_dquot_ref(dquot);
	dqstats.drops++;
}

static int dqinit_needed(struct inode *inode, int type)
{
	int cnt;

	if (IS_NOQUOTA(inode))
		return 0;
	if (type != -1)
		return inode->i_dquot[type] == NODQUOT;
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (inode->i_dquot[cnt] == NODQUOT)
			return 1;
	return 0;
}

static void add_dquot_ref(struct super_block *sb, int type)
{
	struct list_head *p;

restart:
	file_list_lock();
	list_for_each(p, &sb->s_files) {
		struct file *filp = list_entry(p, struct file, f_list);
		struct inode *inode = filp->f_dentry->d_inode;
		if (filp->f_mode & FMODE_WRITE && dqinit_needed(inode, type)) {
			struct vfsmount *mnt = mntget(filp->f_vfsmnt);
			struct dentry *dentry = dget(filp->f_dentry);
			file_list_unlock();
			sb->dq_op->initialize(inode, type);
			dput(dentry);
			mntput(mnt);
			/* As we may have blocked we had better restart... */
			goto restart;
		}
	}
	file_list_unlock();
}

/* Return 0 if dqput() won't block (note that 1 doesn't necessarily mean blocking) */
static inline int dqput_blocks(struct dquot *dquot)
{
	if (dquot->dq_dup_ref && dquot->dq_count - dquot->dq_dup_ref <= 1)
		return 1;
	if (dquot->dq_count <= 1 && dquot->dq_flags & DQ_MOD)
		return 1;
	return 0;
}

/* Remove references to dquots from inode - add dquot to list for freeing if needed */
int remove_inode_dquot_ref(struct inode *inode, int type, struct list_head *tofree_head)
{
	struct dquot *dquot = inode->i_dquot[type];
	int cnt;

	inode->i_dquot[type] = NODQUOT;
	/* any other quota in use? */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (inode->i_dquot[cnt] != NODQUOT)
			goto put_it;
	}
	inode->i_flags &= ~S_QUOTA;
put_it:
	if (dquot != NODQUOT) {
		if (dqput_blocks(dquot)) {
			if (dquot->dq_count != 1)
				printk(KERN_WARNING "VFS: Adding dquot with dq_count %d to dispose list.\n", dquot->dq_count);
			list_add(&dquot->dq_free, tofree_head);	/* As dquot must have currently users it can't be on the free list... */
			return 1;
		}
		else
			dqput(dquot);   /* We have guaranteed we won't block */
	}
	return 0;
}

/* Free list of dquots - called from inode.c */
void put_dquot_list(struct list_head *tofree_head)
{
	struct list_head *act_head;
	struct dquot *dquot;

	lock_kernel();
	act_head = tofree_head->next;
	/* So now we have dquots on the list... Just free them */
	while (act_head != tofree_head) {
		dquot = list_entry(act_head, struct dquot, dq_free);
		act_head = act_head->next;
		list_del(&dquot->dq_free);	/* Remove dquot from the list so we won't have problems... */
		INIT_LIST_HEAD(&dquot->dq_free);
		dqput(dquot);
	}
	unlock_kernel();
}

static inline void dquot_incr_inodes(struct dquot *dquot, unsigned long number)
{
	dquot->dq_dqb.dqb_curinodes += number;
	mark_dquot_dirty(dquot);
}

static inline void dquot_incr_space(struct dquot *dquot, qsize_t number)
{
	dquot->dq_dqb.dqb_curspace += number;
	mark_dquot_dirty(dquot);
}

static inline void dquot_decr_inodes(struct dquot *dquot, unsigned long number)
{
	if (dquot->dq_dqb.dqb_curinodes > number)
		dquot->dq_dqb.dqb_curinodes -= number;
	else
		dquot->dq_dqb.dqb_curinodes = 0;
	if (dquot->dq_dqb.dqb_curinodes < dquot->dq_dqb.dqb_isoftlimit)
		dquot->dq_dqb.dqb_itime = (time_t) 0;
	dquot->dq_flags &= ~DQ_INODES;
	mark_dquot_dirty(dquot);
}

static inline void dquot_decr_space(struct dquot *dquot, qsize_t number)
{
	if (dquot->dq_dqb.dqb_curspace > number)
		dquot->dq_dqb.dqb_curspace -= number;
	else
		dquot->dq_dqb.dqb_curspace = 0;
	if (toqb(dquot->dq_dqb.dqb_curspace) < dquot->dq_dqb.dqb_bsoftlimit)
		dquot->dq_dqb.dqb_btime = (time_t) 0;
	dquot->dq_flags &= ~DQ_BLKS;
	mark_dquot_dirty(dquot);
}

static inline int need_print_warning(struct dquot *dquot, int flag)
{
	switch (dquot->dq_type) {
		case USRQUOTA:
			return current->fsuid == dquot->dq_id && !(dquot->dq_flags & flag);
		case GRPQUOTA:
			return in_group_p(dquot->dq_id) && !(dquot->dq_flags & flag);
	}
	return 0;
}

/* Values of warnings */
#define NOWARN 0
#define IHARDWARN 1
#define ISOFTLONGWARN 2
#define ISOFTWARN 3
#define BHARDWARN 4
#define BSOFTLONGWARN 5
#define BSOFTWARN 6

/* Print warning to user which exceeded quota */
static void print_warning(struct dquot *dquot, const char warntype)
{
	char *msg = NULL;
	int flag = (warntype == BHARDWARN || warntype == BSOFTLONGWARN) ? DQ_BLKS :
	  ((warntype == IHARDWARN || warntype == ISOFTLONGWARN) ? DQ_INODES : 0);

	if (!need_print_warning(dquot, flag))
		return;
	dquot->dq_flags |= flag;
	tty_write_message(current->tty, (char *)bdevname(dquot->dq_sb->s_dev));
	if (warntype == ISOFTWARN || warntype == BSOFTWARN)
		tty_write_message(current->tty, ": warning, ");
	else
		tty_write_message(current->tty, ": write failed, ");
	tty_write_message(current->tty, quotatypes[dquot->dq_type]);
	switch (warntype) {
		case IHARDWARN:
			msg = " file limit reached.\r\n";
			break;
		case ISOFTLONGWARN:
			msg = " file quota exceeded too long.\r\n";
			break;
		case ISOFTWARN:
			msg = " file quota exceeded.\r\n";
			break;
		case BHARDWARN:
			msg = " block limit reached.\r\n";
			break;
		case BSOFTLONGWARN:
			msg = " block quota exceeded too long.\r\n";
			break;
		case BSOFTWARN:
			msg = " block quota exceeded.\r\n";
			break;
	}
	tty_write_message(current->tty, msg);
}

static inline void flush_warnings(struct dquot **dquots, char *warntype)
{
	int i;

	for (i = 0; i < MAXQUOTAS; i++)
		if (dquots[i] != NODQUOT && warntype[i] != NOWARN)
			print_warning(dquots[i], warntype[i]);
}

static inline char ignore_hardlimit(struct dquot *dquot)
{
	struct mem_dqinfo *info = &sb_dqopt(dquot->dq_sb)->info[dquot->dq_type];

	return capable(CAP_SYS_RESOURCE) &&
	    (info->dqi_format->qf_fmt_id != QFMT_VFS_OLD || !(info->dqi_flags & V1_DQF_RSQUASH));
}

static int check_idq(struct dquot *dquot, ulong inodes, char *warntype)
{
	*warntype = NOWARN;
	if (inodes <= 0 || dquot->dq_flags & DQ_FAKE)
		return QUOTA_OK;

	if (dquot->dq_dqb.dqb_ihardlimit &&
	   (dquot->dq_dqb.dqb_curinodes + inodes) > dquot->dq_dqb.dqb_ihardlimit &&
            !ignore_hardlimit(dquot)) {
		*warntype = IHARDWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_dqb.dqb_isoftlimit &&
	   (dquot->dq_dqb.dqb_curinodes + inodes) > dquot->dq_dqb.dqb_isoftlimit &&
	    dquot->dq_dqb.dqb_itime && CURRENT_TIME >= dquot->dq_dqb.dqb_itime &&
            !ignore_hardlimit(dquot)) {
		*warntype = ISOFTLONGWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_dqb.dqb_isoftlimit &&
	   (dquot->dq_dqb.dqb_curinodes + inodes) > dquot->dq_dqb.dqb_isoftlimit &&
	    dquot->dq_dqb.dqb_itime == 0) {
		*warntype = ISOFTWARN;
		dquot->dq_dqb.dqb_itime = CURRENT_TIME + sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_igrace;
	}

	return QUOTA_OK;
}

static int check_bdq(struct dquot *dquot, qsize_t space, int prealloc, char *warntype)
{
	*warntype = 0;
	if (space <= 0 || dquot->dq_flags & DQ_FAKE)
		return QUOTA_OK;

	if (dquot->dq_dqb.dqb_bhardlimit &&
	   toqb(dquot->dq_dqb.dqb_curspace + space) > dquot->dq_dqb.dqb_bhardlimit &&
            !ignore_hardlimit(dquot)) {
		if (!prealloc)
			*warntype = BHARDWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_dqb.dqb_bsoftlimit &&
	   toqb(dquot->dq_dqb.dqb_curspace + space) > dquot->dq_dqb.dqb_bsoftlimit &&
	    dquot->dq_dqb.dqb_btime && CURRENT_TIME >= dquot->dq_dqb.dqb_btime &&
            !ignore_hardlimit(dquot)) {
		if (!prealloc)
			*warntype = BSOFTLONGWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_dqb.dqb_bsoftlimit &&
	   toqb(dquot->dq_dqb.dqb_curspace + space) > dquot->dq_dqb.dqb_bsoftlimit &&
	    dquot->dq_dqb.dqb_btime == 0) {
		if (!prealloc) {
			*warntype = BSOFTWARN;
			dquot->dq_dqb.dqb_btime = CURRENT_TIME + sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_bgrace;
		}
		else
			/*
			 * We don't allow preallocation to exceed softlimit so exceeding will
			 * be always printed
			 */
			return NO_QUOTA;
	}

	return QUOTA_OK;
}

/*
 * Externally referenced functions through dquot_operations in inode.
 *
 * Note: this is a blocking operation.
 */
void dquot_initialize(struct inode *inode, int type)
{
	struct dquot *dquot[MAXQUOTAS];
	unsigned int id = 0;
	int cnt;

	if (IS_NOQUOTA(inode))
		return;
	/* Build list of quotas to initialize... We can block here */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		if (type != -1 && cnt != type)
			continue;
		if (!sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		if (inode->i_dquot[cnt] == NODQUOT) {
			switch (cnt) {
				case USRQUOTA:
					id = inode->i_uid;
					break;
				case GRPQUOTA:
					id = inode->i_gid;
					break;
			}
			dquot[cnt] = dqget(inode->i_sb, id, cnt);
		}
	}
	/* NOBLOCK START: Here we shouldn't block */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT || !sb_has_quota_enabled(inode->i_sb, cnt) || inode->i_dquot[cnt] != NODQUOT)
			continue;
		inode->i_dquot[cnt] = dquot[cnt];
		dquot[cnt] = NODQUOT;
		inode->i_flags |= S_QUOTA;
	}
	/* NOBLOCK END */
	/* Put quotas which we didn't use */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqput(dquot[cnt]);
}

/*
 * Release all quota for the specified inode.
 *
 * Note: this is a blocking operation.
 */
void dquot_drop(struct inode *inode)
{
	struct dquot *dquot;
	int cnt;

	inode->i_flags &= ~S_QUOTA;
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (inode->i_dquot[cnt] == NODQUOT)
			continue;
		dquot = inode->i_dquot[cnt];
		inode->i_dquot[cnt] = NODQUOT;
		dqput(dquot);
	}
}

/*
 * This operation can block, but only after everything is updated
 */
int dquot_alloc_space(struct inode *inode, qsize_t number, int warn)
{
	int cnt, ret = NO_QUOTA;
	struct dquot *dquot[MAXQUOTAS];
	char warntype[MAXQUOTAS];

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = dqduplicate(inode->i_dquot[cnt]);
		if (dquot[cnt] == NODQUOT)
			continue;
		if (check_bdq(dquot[cnt], number, warn, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT)
			continue;
		dquot_incr_space(dquot[cnt], number);
	}
	inode_add_bytes(inode, number);
	/* NOBLOCK End */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(dquot, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqputduplicate(dquot[cnt]);
	return ret;
}

/*
 * This operation can block, but only after everything is updated
 */
int dquot_alloc_inode(const struct inode *inode, unsigned long number)
{
	int cnt, ret = NO_QUOTA;
	struct dquot *dquot[MAXQUOTAS];
	char warntype[MAXQUOTAS];

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = dqduplicate(inode -> i_dquot[cnt]);
		if (dquot[cnt] == NODQUOT)
			continue;
		if (check_idq(dquot[cnt], number, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT)
			continue;
		dquot_incr_inodes(dquot[cnt], number);
	}
	/* NOBLOCK End */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(dquot, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqputduplicate(dquot[cnt]);
	return ret;
}

/*
 * This is a non-blocking operation.
 */
void dquot_free_space(struct inode *inode, qsize_t number)
{
	unsigned int cnt;
	struct dquot *dquot;

	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot = dqduplicate(inode->i_dquot[cnt]);
		if (dquot == NODQUOT)
			continue;
		dquot_decr_space(dquot, number);
		dqputduplicate(dquot);
	}
	inode_sub_bytes(inode, number);
	/* NOBLOCK End */
}

/*
 * This is a non-blocking operation.
 */
void dquot_free_inode(const struct inode *inode, unsigned long number)
{
	unsigned int cnt;
	struct dquot *dquot;

	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot = dqduplicate(inode->i_dquot[cnt]);
		if (dquot == NODQUOT)
			continue;
		dquot_decr_inodes(dquot, number);
		dqputduplicate(dquot);
	}
	/* NOBLOCK End */
}

/*
 * Transfer the number of inode and blocks from one diskquota to an other.
 *
 * This operation can block, but only after everything is updated
 */
int dquot_transfer(struct inode *inode, struct iattr *iattr)
{
	qsize_t space;
	struct dquot *transfer_from[MAXQUOTAS];
	struct dquot *transfer_to[MAXQUOTAS];
	int cnt, ret = NO_QUOTA, chuid = (iattr->ia_valid & ATTR_UID) && inode->i_uid != iattr->ia_uid,
	    chgid = (iattr->ia_valid & ATTR_GID) && inode->i_gid != iattr->ia_gid;
	char warntype[MAXQUOTAS];

	/* Clear the arrays */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		transfer_to[cnt] = transfer_from[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* First build the transfer_to list - here we can block on reading of dquots... */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (!sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		switch (cnt) {
			case USRQUOTA:
				if (!chuid)
					continue;
				transfer_to[cnt] = dqget(inode->i_sb, iattr->ia_uid, cnt);
				break;
			case GRPQUOTA:
				if (!chgid)
					continue;
				transfer_to[cnt] = dqget(inode->i_sb, iattr->ia_gid, cnt);
				break;
		}
	}
	/* NOBLOCK START: From now on we shouldn't block */
	space = inode_get_bytes(inode);
	/* Build the transfer_from list and check the limits */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		/* The second test can fail when quotaoff is in progress... */
		if (transfer_to[cnt] == NODQUOT || !sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		transfer_from[cnt] = dqduplicate(inode->i_dquot[cnt]);
		if (transfer_from[cnt] == NODQUOT)	/* Can happen on quotafiles (quota isn't initialized on them)... */
			continue;
		if (check_idq(transfer_to[cnt], 1, warntype+cnt) == NO_QUOTA ||
		    check_bdq(transfer_to[cnt], space, 0, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}

	/*
	 * Finally perform the needed transfer from transfer_from to transfer_to
	 */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		/*
		 * Skip changes for same uid or gid or for non-existing quota-type.
		 */
		if (transfer_from[cnt] == NODQUOT || transfer_to[cnt] == NODQUOT)
			continue;

		dquot_decr_inodes(transfer_from[cnt], 1);
		dquot_decr_space(transfer_from[cnt], space);

		dquot_incr_inodes(transfer_to[cnt], 1);
		dquot_incr_space(transfer_to[cnt], space);

		if (inode->i_dquot[cnt] == NODQUOT)
			BUG();
		inode->i_dquot[cnt] = transfer_to[cnt];
		/*
		 * We've got to release transfer_from[] twice - once for dquot_transfer() and
		 * once for inode. We don't want to release transfer_to[] as it's now placed in inode
		 */
		transfer_to[cnt] = transfer_from[cnt];
	}
	/* NOBLOCK END. From now on we can block as we wish */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(transfer_to, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		/* First we must put duplicate - otherwise we might deadlock */
		if (transfer_from[cnt] != NODQUOT)
			dqputduplicate(transfer_from[cnt]);
		if (transfer_to[cnt] != NODQUOT)
			dqput(transfer_to[cnt]);
	}
	return ret;
}

/*
 * Definitions of diskquota operations.
 */
struct dquot_operations dquot_operations = {
	initialize:	dquot_initialize,		/* mandatory */
	drop:		dquot_drop,			/* mandatory */
	alloc_space:	dquot_alloc_space,
	alloc_inode:	dquot_alloc_inode,
	free_space:	dquot_free_space,
	free_inode:	dquot_free_inode,
	transfer:	dquot_transfer,
	write_dquot:	commit_dqblk
};

/* Function used by filesystems for initializing the dquot_operations structure */
void init_dquot_operations(struct dquot_operations *fsdqops)
{
	memcpy(fsdqops, &dquot_operations, sizeof(dquot_operations));
}

static inline void set_enable_flags(struct quota_info *dqopt, int type)
{
	switch (type) {
		case USRQUOTA:
			dqopt->flags |= DQUOT_USR_ENABLED;
			break;
		case GRPQUOTA:
			dqopt->flags |= DQUOT_GRP_ENABLED;
			break;
	}
}

static inline void reset_enable_flags(struct quota_info *dqopt, int type)
{
	switch (type) {
		case USRQUOTA:
			dqopt->flags &= ~DQUOT_USR_ENABLED;
			break;
		case GRPQUOTA:
			dqopt->flags &= ~DQUOT_GRP_ENABLED;
			break;
	}
}

/* Function in inode.c - remove pointers to dquots in icache */
extern void remove_dquot_ref(struct super_block *, int);

/*
 * Turn quota off on a device. type == -1 ==> quotaoff for all types (umount)
 */
int vfs_quota_off(struct super_block *sb, int type)
{
	int cnt;
	struct quota_info *dqopt = sb_dqopt(sb);

	lock_kernel();
	if (!sb)
		goto out;

	/* We need to serialize quota_off() for device */
	down(&dqopt->dqoff_sem);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (type != -1 && cnt != type)
			continue;
		if (!is_enabled(dqopt, cnt))
			continue;
		reset_enable_flags(dqopt, cnt);

		/* Note: these are blocking operations */
		remove_dquot_ref(sb, cnt);
		invalidate_dquots(sb, cnt);
		if (info_dirty(&dqopt->info[cnt]))
			dqopt->ops[cnt]->write_file_info(sb, cnt);
		if (dqopt->ops[cnt]->free_file_info)
			dqopt->ops[cnt]->free_file_info(sb, cnt);
		put_quota_format(dqopt->info[cnt].dqi_format);

		fput(dqopt->files[cnt]);
		dqopt->files[cnt] = (struct file *)NULL;
		dqopt->info[cnt].dqi_flags = 0;
		dqopt->info[cnt].dqi_igrace = 0;
		dqopt->info[cnt].dqi_bgrace = 0;
		dqopt->ops[cnt] = NULL;
	}
	up(&dqopt->dqoff_sem);
out:
	unlock_kernel();
	return 0;
}

int vfs_quota_on(struct super_block *sb, int type, int format_id, char *path)
{
	struct file *f = NULL;
	struct inode *inode;
	struct quota_info *dqopt = sb_dqopt(sb);
	struct quota_format_type *fmt = find_quota_format(format_id);
	int error;

	if (!fmt)
		return -ESRCH;
	if (is_enabled(dqopt, type)) {
		error = -EBUSY;
		goto out_fmt;
	}

	down(&dqopt->dqoff_sem);

	f = filp_open(path, O_RDWR, 0600);

	error = PTR_ERR(f);
	if (IS_ERR(f))
		goto out_lock;
	dqopt->files[type] = f;
	error = -EIO;
	if (!f->f_op || !f->f_op->read || !f->f_op->write)
		goto out_f;
	inode = f->f_dentry->d_inode;
	error = -EACCES;
	if (!S_ISREG(inode->i_mode))
		goto out_f;
	error = -EINVAL;
	if (!fmt->qf_ops->check_quota_file(sb, type))
		goto out_f;
	/* We don't want quota and atime on quota files */
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA | S_NOATIME;

	dqopt->ops[type] = fmt->qf_ops;
	dqopt->info[type].dqi_format = fmt;
	if ((error = dqopt->ops[type]->read_file_info(sb, type)) < 0)
		goto out_f;
	set_enable_flags(dqopt, type);

	add_dquot_ref(sb, type);

	up(&dqopt->dqoff_sem);
	return 0;

out_f:
	if (f)
		filp_close(f, NULL);
	dqopt->files[type] = NULL;
out_lock:
	up(&dqopt->dqoff_sem);
out_fmt:
	put_quota_format(fmt);

	return error; 
}

/* Generic routine for getting common part of quota structure */
static void do_get_dqblk(struct dquot *dquot, struct if_dqblk *di)
{
	struct mem_dqblk *dm = &dquot->dq_dqb;

	di->dqb_bhardlimit = dm->dqb_bhardlimit;
	di->dqb_bsoftlimit = dm->dqb_bsoftlimit;
	di->dqb_curspace = dm->dqb_curspace;
	di->dqb_ihardlimit = dm->dqb_ihardlimit;
	di->dqb_isoftlimit = dm->dqb_isoftlimit;
	di->dqb_curinodes = dm->dqb_curinodes;
	di->dqb_btime = dm->dqb_btime;
	di->dqb_itime = dm->dqb_itime;
	di->dqb_valid = QIF_ALL;
}

int vfs_get_dqblk(struct super_block *sb, int type, qid_t id, struct if_dqblk *di)
{
	struct dquot *dquot = dqget(sb, id, type);

	if (!dquot)
		return -EINVAL;
	do_get_dqblk(dquot, di);
	dqput(dquot);
	return 0;
}

/* Generic routine for setting common part of quota structure */
static void do_set_dqblk(struct dquot *dquot, struct if_dqblk *di)
{
	struct mem_dqblk *dm = &dquot->dq_dqb;
	int check_blim = 0, check_ilim = 0;

	if (di->dqb_valid & QIF_SPACE) {
		dm->dqb_curspace = di->dqb_curspace;
		check_blim = 1;
	}
	if (di->dqb_valid & QIF_BLIMITS) {
		dm->dqb_bsoftlimit = di->dqb_bsoftlimit;
		dm->dqb_bhardlimit = di->dqb_bhardlimit;
		check_blim = 1;
	}
	if (di->dqb_valid & QIF_INODES) {
		dm->dqb_curinodes = di->dqb_curinodes;
		check_ilim = 1;
	}
	if (di->dqb_valid & QIF_ILIMITS) {
		dm->dqb_isoftlimit = di->dqb_isoftlimit;
		dm->dqb_ihardlimit = di->dqb_ihardlimit;
		check_ilim = 1;
	}
	if (di->dqb_valid & QIF_BTIME)
		dm->dqb_btime = di->dqb_btime;
	if (di->dqb_valid & QIF_ITIME)
		dm->dqb_itime = di->dqb_itime;

	if (check_blim) {
		if (!dm->dqb_bsoftlimit || toqb(dm->dqb_curspace) < dm->dqb_bsoftlimit) {
			dm->dqb_btime = 0;
			dquot->dq_flags &= ~DQ_BLKS;
		}
		else if (!(di->dqb_valid & QIF_BTIME))	/* Set grace only if user hasn't provided his own... */
			dm->dqb_btime = CURRENT_TIME + sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_bgrace;
	}
	if (check_ilim) {
		if (!dm->dqb_isoftlimit || dm->dqb_curinodes < dm->dqb_isoftlimit) {
			dm->dqb_itime = 0;
			dquot->dq_flags &= ~DQ_INODES;
		}
		else if (!(di->dqb_valid & QIF_ITIME))	/* Set grace only if user hasn't provided his own... */
			dm->dqb_itime = CURRENT_TIME + sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_igrace;
	}
	if (dm->dqb_bhardlimit || dm->dqb_bsoftlimit || dm->dqb_ihardlimit || dm->dqb_isoftlimit)
		dquot->dq_flags &= ~DQ_FAKE;
	else
		dquot->dq_flags |= DQ_FAKE;
	dquot->dq_flags |= DQ_MOD;
}

int vfs_set_dqblk(struct super_block *sb, int type, qid_t id, struct if_dqblk *di)
{
	struct dquot *dquot = dqget(sb, id, type);

	if (!dquot)
		return -EINVAL;
	do_set_dqblk(dquot, di);
	dqput(dquot);
	return 0;
}

/* Generic routine for getting common part of quota file information */
int vfs_get_dqinfo(struct super_block *sb, int type, struct if_dqinfo *ii)
{
	struct mem_dqinfo *mi = sb_dqopt(sb)->info + type;

	ii->dqi_bgrace = mi->dqi_bgrace;
	ii->dqi_igrace = mi->dqi_igrace;
	ii->dqi_flags = mi->dqi_flags & DQF_MASK;
	ii->dqi_valid = IIF_ALL;
	return 0;
}

/* Generic routine for setting common part of quota file information */
int vfs_set_dqinfo(struct super_block *sb, int type, struct if_dqinfo *ii)
{
	struct mem_dqinfo *mi = sb_dqopt(sb)->info + type;

	if (ii->dqi_valid & IIF_BGRACE)
		mi->dqi_bgrace = ii->dqi_bgrace;
	if (ii->dqi_valid & IIF_IGRACE)
		mi->dqi_igrace = ii->dqi_igrace;
	if (ii->dqi_valid & IIF_FLAGS)
		mi->dqi_flags = (mi->dqi_flags & ~DQF_MASK) | (ii->dqi_flags & DQF_MASK);
	mark_info_dirty(mi);
	return 0;
}

struct quotactl_ops vfs_quotactl_ops = {
	quota_on:	vfs_quota_on,
	quota_off:	vfs_quota_off,
	quota_sync:	vfs_quota_sync,
	get_info:	vfs_get_dqinfo,
	set_info:	vfs_set_dqinfo,
	get_dqblk:	vfs_get_dqblk,
	set_dqblk:	vfs_set_dqblk
};

static ctl_table fs_dqstats_table[] = {
	{FS_DQ_LOOKUPS, "lookups", &dqstats.lookups, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_DROPS, "drops", &dqstats.drops, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_READS, "reads", &dqstats.reads, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_WRITES, "writes", &dqstats.writes, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_CACHE_HITS, "cache_hits", &dqstats.cache_hits, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_ALLOCATED, "allocated_dquots", &dqstats.allocated_dquots, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_FREE, "free_dquots", &dqstats.free_dquots, sizeof(int), 0444, NULL, &proc_dointvec},
	{FS_DQ_SYNCS, "syncs", &dqstats.syncs, sizeof(int), 0444, NULL, &proc_dointvec},
	{},
};

static ctl_table fs_table[] = {
	{FS_DQSTATS, "quota", NULL, 0, 0555, fs_dqstats_table},
	{},
};

static ctl_table sys_table[] = {
	{CTL_FS, "fs", NULL, 0, 0555, fs_table},
	{},
};

static int __init dquot_init(void)
{
	int i;

	register_sysctl_table(sys_table, 0);
	for (i = 0; i < NR_DQHASH; i++)
		INIT_LIST_HEAD(dquot_hash + i);
	printk(KERN_NOTICE "VFS: Disk quotas v%s\n", __DQUOT_VERSION__);

	return 0;
}
__initcall(dquot_init);

EXPORT_SYMBOL(register_quota_format);
EXPORT_SYMBOL(unregister_quota_format);
EXPORT_SYMBOL(dqstats);
EXPORT_SYMBOL(init_dquot_operations);
