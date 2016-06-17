#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

#ifdef __KERNEL__

#include <asm/atomic.h>
#include <linux/mount.h>
#include <linux/kernel.h>

/*
 * linux/include/linux/dcache.h
 *
 * Dirent cache data structures
 *
 * (C) Copyright 1997 Thomas Schoebel-Theuer,
 * with heavy changes by Linus Torvalds
 */

#define IS_ROOT(x) ((x) == (x)->d_parent)

/*
 * "quick string" -- eases parameter passing, but more importantly
 * saves "metadata" about the string (ie length and the hash).
 */
struct qstr {
	const unsigned char * name;
	unsigned int len;
	unsigned int hash;
};

struct dentry_stat_t {
	int nr_dentry;
	int nr_unused;
	int age_limit;          /* age in seconds */
	int want_pages;         /* pages requested by system */
	int dummy[2];
};
extern struct dentry_stat_t dentry_stat;

/* Name hashing routines. Initial hash value */
/* Hash courtesy of the R5 hash in reiserfs modulo sign bits */
#define init_name_hash()		0

/* partial hash update function. Assume roughly 4 bits per character */
static __inline__ unsigned long partial_name_hash(unsigned long c, unsigned long prevhash)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}

/* Finally: cut down the number of bits to a int value (and try to avoid losing bits) */
static __inline__ unsigned long end_name_hash(unsigned long hash)
{
	return (unsigned int) hash;
}

/* Compute the hash for a name string. */
static __inline__ unsigned int full_name_hash(const unsigned char * name, unsigned int len)
{
	unsigned long hash = init_name_hash();
	while (len--)
		hash = partial_name_hash(*name++, hash);
	return end_name_hash(hash);
}

#define DNAME_INLINE_LEN 16

struct dentry {
	atomic_t d_count;
	unsigned int d_flags;
	struct inode  * d_inode;	/* Where the name belongs to - NULL is negative */
	struct dentry * d_parent;	/* parent directory */
	struct list_head d_hash;	/* lookup hash list */
	struct list_head d_lru;		/* d_count = 0 LRU list */
	struct list_head d_child;	/* child of parent list */
	struct list_head d_subdirs;	/* our children */
	struct list_head d_alias;	/* inode alias list */
	int d_mounted;
	struct qstr d_name;
	unsigned long d_time;		/* used by d_revalidate */
	struct dentry_operations  *d_op;
	struct super_block * d_sb;	/* The root of the dentry tree */
	unsigned long d_vfs_flags;
	void * d_fsdata;		/* fs-specific data */
	unsigned char d_iname[DNAME_INLINE_LEN]; /* small names */
};

struct dentry_operations {
	int (*d_revalidate)(struct dentry *, int);
	int (*d_hash) (struct dentry *, struct qstr *);
	int (*d_compare) (struct dentry *, struct qstr *, struct qstr *);
	int (*d_delete)(struct dentry *);
	void (*d_release)(struct dentry *);
	void (*d_iput)(struct dentry *, struct inode *);
};

/* the dentry parameter passed to d_hash and d_compare is the parent
 * directory of the entries to be compared. It is used in case these
 * functions need any directory specific information for determining
 * equivalency classes.  Using the dentry itself might not work, as it
 * might be a negative dentry which has no information associated with
 * it */

/*
locking rules:
		big lock	dcache_lock	may block
d_revalidate:	no		no		yes
d_hash		no		no		yes
d_compare:	no		yes		no
d_delete:	no		yes		no
d_release:	no		no		yes
d_iput:		no		no		yes
 */

/* d_flags entries */
#define DCACHE_AUTOFS_PENDING 0x0001    /* autofs: "under construction" */
#define DCACHE_NFSFS_RENAMED  0x0002    /* this dentry has been "silly
					 * renamed" and has to be
					 * deleted on the last dput()
					 */
#define	DCACHE_NFSD_DISCONNECTED 0x0004	/* This dentry is not currently connected to the
					 * dcache tree. Its parent will either be itself,
					 * or will have this flag as well.
					 * If this dentry points to a directory, then
					 * s_nfsd_free_path semaphore will be down
					 */
#define DCACHE_REFERENCED	0x0008  /* Recently used, don't discard. */

extern spinlock_t dcache_lock;

/**
 * d_drop - drop a dentry
 * @dentry: dentry to drop
 *
 * d_drop() unhashes the entry from the parent
 * dentry hashes, so that it won't be found through
 * a VFS lookup any more. Note that this is different
 * from deleting the dentry - d_delete will try to
 * mark the dentry negative if possible, giving a
 * successful _negative_ lookup, while d_drop will
 * just make the cache lookup fail.
 *
 * d_drop() is used mainly for stuff that wants
 * to invalidate a dentry for some reason (NFS
 * timeouts or autofs deletes).
 */

static __inline__ void d_drop(struct dentry * dentry)
{
	spin_lock(&dcache_lock);
	list_del(&dentry->d_hash);
	INIT_LIST_HEAD(&dentry->d_hash);
	spin_unlock(&dcache_lock);
}

static __inline__ int dname_external(struct dentry *d)
{
	return d->d_name.name != d->d_iname; 
}

/*
 * These are the low-level FS interfaces to the dcache..
 */
extern void d_instantiate(struct dentry *, struct inode *);
extern void d_delete(struct dentry *);

/* allocate/de-allocate */
extern struct dentry * d_alloc(struct dentry *, const struct qstr *);
extern void shrink_dcache_sb(struct super_block *);
extern void shrink_dcache_parent(struct dentry *);
extern int d_invalidate(struct dentry *);

#define shrink_dcache() prune_dcache(0)
struct zone_struct;
/* dcache memory management */
extern int shrink_dcache_memory(int, unsigned int);
extern void prune_dcache(int);

/* icache memory management (defined in linux/fs/inode.c) */
extern int shrink_icache_memory(int, int);
extern void prune_icache(int);

/* quota cache memory management (defined in linux/fs/dquot.c) */
extern int shrink_dqcache_memory(int, unsigned int);

/* only used at mount-time */
extern struct dentry * d_alloc_root(struct inode *);

/* <clickety>-<click> the ramfs-type tree */
extern void d_genocide(struct dentry *);

extern struct dentry *d_find_alias(struct inode *);
extern void d_prune_aliases(struct inode *);

/* test whether we have any submounts in a subdir tree */
extern int have_submounts(struct dentry *);

/*
 * This adds the entry to the hash queues.
 */
extern void d_rehash(struct dentry *);

/**
 * d_add - add dentry to hash queues
 * @entry: dentry to add
 * @inode: The inode to attach to this dentry
 *
 * This adds the entry to the hash queues and initializes @inode.
 * The entry was actually filled in earlier during d_alloc().
 */
 
static __inline__ void d_add(struct dentry * entry, struct inode * inode)
{
	d_instantiate(entry, inode);
	d_rehash(entry);
}

/* used for rename() and baskets */
extern void d_move(struct dentry *, struct dentry *);

/* appendix may either be NULL or be used for transname suffixes */
extern struct dentry * d_lookup(struct dentry *, struct qstr *);

/* validate "insecure" dentry pointer */
extern int d_validate(struct dentry *, struct dentry *);

extern char * __d_path(struct dentry *, struct vfsmount *, struct dentry *,
	struct vfsmount *, char *, int);
  
/* Allocation counts.. */

/**
 *	dget, dget_locked	-	get a reference to a dentry
 *	@dentry: dentry to get a reference to
 *
 *	Given a dentry or %NULL pointer increment the reference count
 *	if appropriate and return the dentry. A dentry will not be 
 *	destroyed when it has references. dget() should never be
 *	called for dentries with zero reference counter. For these cases
 *	(preferably none, functions in dcache.c are sufficient for normal
 *	needs and they take necessary precautions) you should hold dcache_lock
 *	and call dget_locked() instead of dget().
 */
 
static __inline__ struct dentry * dget(struct dentry *dentry)
{
	if (dentry) {
		if (!atomic_read(&dentry->d_count))
			out_of_line_bug();
		atomic_inc(&dentry->d_count);
	}
	return dentry;
}

extern struct dentry * dget_locked(struct dentry *);

/**
 *	d_unhashed -	is dentry hashed
 *	@dentry: entry to check
 *
 *	Returns true if the dentry passed is not currently hashed.
 */
 
static __inline__ int d_unhashed(struct dentry *dentry)
{
	return list_empty(&dentry->d_hash);
}

extern void dput(struct dentry *);

static __inline__ int d_mountpoint(struct dentry *dentry)
{
	return dentry->d_mounted;
}

extern struct vfsmount *lookup_mnt(struct vfsmount *, struct dentry *);
#endif /* __KERNEL__ */

#endif	/* __LINUX_DCACHE_H */
