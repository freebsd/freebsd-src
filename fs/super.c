/*
 *  linux/fs/super.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  super.c contains code to handle: - mount structures
 *                                   - super-block tables
 *                                   - filesystem drivers list
 *                                   - mount system call
 *                                   - umount system call
 *                                   - ustat system call
 *
 * GK 2/5/95  -  Changed to support mounting the root fs via NFS
 *
 *  Added kerneld support: Jacques Gelinas and Bjorn Ekwall
 *  Added change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Added options to /proc/mounts:
 *    Torbjörn Lindh (torbjorn.lindh@gopta.se), April 14, 1996.
 *  Added devfs support: Richard Gooch <rgooch@atnf.csiro.au>, 13-JAN-1998
 *  Heavily rewritten for 'one fs - one tree' dcache architecture. AV, Mar 2000
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/major.h>
#include <linux/acct.h>
#include <linux/quotaops.h>

#include <asm/uaccess.h>

#include <linux/kmod.h>
#define __NO_VERSION__
#include <linux/module.h>

LIST_HEAD(super_blocks);
spinlock_t sb_lock = SPIN_LOCK_UNLOCKED;

/*
 * Handling of filesystem drivers list.
 * Rules:
 *	Inclusion to/removals from/scanning of list are protected by spinlock.
 *	During the unload module must call unregister_filesystem().
 *	We can access the fields of list element if:
 *		1) spinlock is held or
 *		2) we hold the reference to the module.
 *	The latter can be guaranteed by call of try_inc_mod_count(); if it
 *	returned 0 we must skip the element, otherwise we got the reference.
 *	Once the reference is obtained we can drop the spinlock.
 */

static struct file_system_type *file_systems;
static rwlock_t file_systems_lock = RW_LOCK_UNLOCKED;

/* WARNING: This can be used only if we _already_ own a reference */
static void get_filesystem(struct file_system_type *fs)
{
	if (fs->owner)
		__MOD_INC_USE_COUNT(fs->owner);
}

static void put_filesystem(struct file_system_type *fs)
{
	if (fs->owner)
		__MOD_DEC_USE_COUNT(fs->owner);
}

static struct file_system_type **find_filesystem(const char *name)
{
	struct file_system_type **p;
	for (p=&file_systems; *p; p=&(*p)->next)
		if (strcmp((*p)->name,name) == 0)
			break;
	return p;
}

/**
 *	register_filesystem - register a new filesystem
 *	@fs: the file system structure
 *
 *	Adds the file system passed to the list of file systems the kernel
 *	is aware of for mount and other syscalls. Returns 0 on success,
 *	or a negative errno code on an error.
 *
 *	The &struct file_system_type that is passed is linked into the kernel 
 *	structures and must not be freed until the file system has been
 *	unregistered.
 */
 
int register_filesystem(struct file_system_type * fs)
{
	int res = 0;
	struct file_system_type ** p;

	if (!fs)
		return -EINVAL;
	if (fs->next)
		return -EBUSY;
	INIT_LIST_HEAD(&fs->fs_supers);
	write_lock(&file_systems_lock);
	p = find_filesystem(fs->name);
	if (*p)
		res = -EBUSY;
	else
		*p = fs;
	write_unlock(&file_systems_lock);
	return res;
}

/**
 *	unregister_filesystem - unregister a file system
 *	@fs: filesystem to unregister
 *
 *	Remove a file system that was previously successfully registered
 *	with the kernel. An error is returned if the file system is not found.
 *	Zero is returned on a success.
 *	
 *	Once this function has returned the &struct file_system_type structure
 *	may be freed or reused.
 */
 
int unregister_filesystem(struct file_system_type * fs)
{
	struct file_system_type ** tmp;

	write_lock(&file_systems_lock);
	tmp = &file_systems;
	while (*tmp) {
		if (fs == *tmp) {
			*tmp = fs->next;
			fs->next = NULL;
			write_unlock(&file_systems_lock);
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	write_unlock(&file_systems_lock);
	return -EINVAL;
}

static int fs_index(const char * __name)
{
	struct file_system_type * tmp;
	char * name;
	int err, index;

	name = getname(__name);
	err = PTR_ERR(name);
	if (IS_ERR(name))
		return err;

	err = -EINVAL;
	read_lock(&file_systems_lock);
	for (tmp=file_systems, index=0 ; tmp ; tmp=tmp->next, index++) {
		if (strcmp(tmp->name,name) == 0) {
			err = index;
			break;
		}
	}
	read_unlock(&file_systems_lock);
	putname(name);
	return err;
}

static int fs_name(unsigned int index, char * buf)
{
	struct file_system_type * tmp;
	int len, res;

	read_lock(&file_systems_lock);
	for (tmp = file_systems; tmp; tmp = tmp->next, index--)
		if (index <= 0 && try_inc_mod_count(tmp->owner))
				break;
	read_unlock(&file_systems_lock);
	if (!tmp)
		return -EINVAL;

	/* OK, we got the reference, so we can safely block */
	len = strlen(tmp->name) + 1;
	res = copy_to_user(buf, tmp->name, len) ? -EFAULT : 0;
	put_filesystem(tmp);
	return res;
}

static int fs_maxindex(void)
{
	struct file_system_type * tmp;
	int index;

	read_lock(&file_systems_lock);
	for (tmp = file_systems, index = 0 ; tmp ; tmp = tmp->next, index++)
		;
	read_unlock(&file_systems_lock);
	return index;
}

/*
 * Whee.. Weird sysv syscall. 
 */
asmlinkage long sys_sysfs(int option, unsigned long arg1, unsigned long arg2)
{
	int retval = -EINVAL;

	switch (option) {
		case 1:
			retval = fs_index((const char *) arg1);
			break;

		case 2:
			retval = fs_name(arg1, (char *) arg2);
			break;

		case 3:
			retval = fs_maxindex();
			break;
	}
	return retval;
}

int get_filesystem_list(char * buf)
{
	int len = 0;
	struct file_system_type * tmp;

	read_lock(&file_systems_lock);
	tmp = file_systems;
	while (tmp && len < PAGE_SIZE - 80) {
		len += sprintf(buf+len, "%s\t%s\n",
			(tmp->fs_flags & FS_REQUIRES_DEV) ? "" : "nodev",
			tmp->name);
		tmp = tmp->next;
	}
	read_unlock(&file_systems_lock);
	return len;
}

struct file_system_type *get_fs_type(const char *name)
{
	struct file_system_type *fs;
	
	read_lock(&file_systems_lock);
	fs = *(find_filesystem(name));
	if (fs && !try_inc_mod_count(fs->owner))
		fs = NULL;
	read_unlock(&file_systems_lock);
	if (!fs && (request_module(name) == 0)) {
		read_lock(&file_systems_lock);
		fs = *(find_filesystem(name));
		if (fs && !try_inc_mod_count(fs->owner))
			fs = NULL;
		read_unlock(&file_systems_lock);
	}
	return fs;
}

/**
 *	alloc_super	-	create new superblock
 *
 *	Allocates and initializes a new &struct super_block.  alloc_super()
 *	returns a pointer new superblock or %NULL if allocation had failed.
 */
static struct super_block *alloc_super(void)
{
	static struct super_operations empty_sops = {};
	struct super_block *s = kmalloc(sizeof(struct super_block),  GFP_USER);
	if (s) {
		memset(s, 0, sizeof(struct super_block));
		INIT_LIST_HEAD(&s->s_dirty);
		INIT_LIST_HEAD(&s->s_locked_inodes);
		INIT_LIST_HEAD(&s->s_files);
		INIT_LIST_HEAD(&s->s_instances);
		init_rwsem(&s->s_umount);
		sema_init(&s->s_lock, 1);
		down_write(&s->s_umount);
		s->s_count = S_BIAS;
		atomic_set(&s->s_active, 1);
		sema_init(&s->s_vfs_rename_sem,1);
		sema_init(&s->s_nfsd_free_path_sem,1);
		sema_init(&s->s_dquot.dqio_sem, 1);
		sema_init(&s->s_dquot.dqoff_sem, 1);
		s->s_maxbytes = MAX_NON_LFS;
		s->s_op = &empty_sops;
		s->dq_op = sb_dquot_ops;
		s->s_qcop = sb_quotactl_ops;
	}
	return s;
}

/**
 *	destroy_super	-	frees a superblock
 *	@s: superblock to free
 *
 *	Frees a superblock.
 */
static inline void destroy_super(struct super_block *s)
{
	kfree(s);
}

/* Superblock refcounting  */

/**
 *	deactivate_super	-	turn an active reference into temporary
 *	@s: superblock to deactivate
 *
 *	Turns an active reference into temporary one.  Returns 0 if there are
 *	other active references, 1 if we had deactivated the last one.
 */
static inline int deactivate_super(struct super_block *s)
{
	if (!atomic_dec_and_lock(&s->s_active, &sb_lock))
		return 0;
	s->s_count -= S_BIAS-1;
	spin_unlock(&sb_lock);
	return 1;
}

/**
 *	put_super	-	drop a temporary reference to superblock
 *	@s: superblock in question
 *
 *	Drops a temporary reference, frees superblock if there's no
 *	references left.
 */
static inline void put_super(struct super_block *s)
{
	spin_lock(&sb_lock);
	if (!--s->s_count)
		destroy_super(s);
	spin_unlock(&sb_lock);
}

/**
 *	grab_super	- acquire an active reference
 *	@s	- reference we are trying to make active
 *
 *	Tries to acquire an active reference.  grab_super() is used when we
 * 	had just found a superblock in super_blocks or fs_type->fs_supers
 *	and want to turn it into a full-blown active reference.  grab_super()
 *	is called with sb_lock held and drops it.  Returns 1 in case of
 *	success, 0 if we had failed (superblock contents was already dead or
 *	dying when grab_super() had been called).
 */
static int grab_super(struct super_block *s)
{
	s->s_count++;
	spin_unlock(&sb_lock);
	down_write(&s->s_umount);
	if (s->s_root) {
		spin_lock(&sb_lock);
		if (s->s_count > S_BIAS) {
			atomic_inc(&s->s_active);
			s->s_count--;
			spin_unlock(&sb_lock);
			return 1;
		}
		spin_unlock(&sb_lock);
	}
	up_write(&s->s_umount);
	put_super(s);
	return 0;
}
 
/**
 *	insert_super	-	put superblock on the lists
 *	@s:	superblock in question
 *	@type:	filesystem type it will belong to
 *
 *	Associates superblock with fs type and puts it on per-type and global
 *	superblocks' lists.  Should be called with sb_lock held; drops it.
 */
static void insert_super(struct super_block *s, struct file_system_type *type)
{
	s->s_type = type;
	list_add(&s->s_list, super_blocks.prev);
	list_add(&s->s_instances, &type->fs_supers);
	spin_unlock(&sb_lock);
	get_filesystem(type);
}

static void put_anon_dev(kdev_t dev);

/**
 *	remove_super	-	makes superblock unreachable
 *	@s:	superblock in question
 *
 *	Removes superblock from the lists, unlocks it, drop the reference
 *	and releases the hosting device.  @s should have no active
 *	references by that time and after remove_super() it's essentially
 *	in rundown mode - all remaining references are temporary, no new
 *	reference of any sort are going to appear and all holders of
 *	temporary ones will eventually drop them.  At that point superblock
 *	itself will be destroyed; all its contents is already gone.
 */
static void remove_super(struct super_block *s)
{
	kdev_t dev = s->s_dev;
	struct block_device *bdev = s->s_bdev;
	struct file_system_type *fs = s->s_type;

	spin_lock(&sb_lock);
	list_del(&s->s_list);
	list_del(&s->s_instances);
	spin_unlock(&sb_lock);
	up_write(&s->s_umount);
	put_super(s);
	put_filesystem(fs);
	if (bdev)
		blkdev_put(bdev, BDEV_FS);
	else
		put_anon_dev(dev);
}

struct vfsmount *alloc_vfsmnt(char *name);
void free_vfsmnt(struct vfsmount *mnt);

static inline struct super_block * find_super(kdev_t dev)
{
	struct list_head *p;

	list_for_each(p, &super_blocks) {
		struct super_block * s = sb_entry(p);
		if (s->s_dev == dev) {
			s->s_count++;
			return s;
		}
	}
	return NULL;
}

void drop_super(struct super_block *sb)
{
	up_read(&sb->s_umount);
	put_super(sb);
}

static inline void write_super(struct super_block *sb)
{
	lock_super(sb);
	if (sb->s_root && sb->s_dirt)
		if (sb->s_op && sb->s_op->write_super)
			sb->s_op->write_super(sb);
	unlock_super(sb);
}

/*
 * Note: check the dirty flag before waiting, so we don't
 * hold up the sync while mounting a device. (The newly
 * mounted device won't need syncing.)
 */
void sync_supers(kdev_t dev, int wait)
{
	struct super_block * sb;

	if (dev) {
		sb = get_super(dev);
		if (sb) {
			if (sb->s_dirt)
				write_super(sb);
			if (wait && sb->s_op && sb->s_op->sync_fs)
				sb->s_op->sync_fs(sb);
			drop_super(sb);
		}
		return;
	}
restart:
	spin_lock(&sb_lock);
	sb = sb_entry(super_blocks.next);
	while (sb != sb_entry(&super_blocks))
		if (sb->s_dirt) {
			sb->s_count++;
			spin_unlock(&sb_lock);
			down_read(&sb->s_umount);
			write_super(sb);
			if (wait && sb->s_root && sb->s_op && sb->s_op->sync_fs)
				sb->s_op->sync_fs(sb);
			drop_super(sb);
			goto restart;
		} else
			sb = sb_entry(sb->s_list.next);
	spin_unlock(&sb_lock);
}

/**
 *	get_super	-	get the superblock of a device
 *	@dev: device to get the superblock for
 *	
 *	Scans the superblock list and finds the superblock of the file system
 *	mounted on the device given. %NULL is returned if no match is found.
 */
 
struct super_block * get_super(kdev_t dev)
{
	struct super_block * s;

	if (!dev)
		return NULL;
restart:
	spin_lock(&sb_lock);
	s = find_super(dev);
	if (s) {
		spin_unlock(&sb_lock);
		down_read(&s->s_umount);
		if (s->s_root)
			return s;
		drop_super(s);
		goto restart;
	}
	spin_unlock(&sb_lock);
	return NULL;
}

asmlinkage long sys_ustat(dev_t dev, struct ustat * ubuf)
{
        struct super_block *s;
        struct ustat tmp;
        struct statfs sbuf;
	int err = -EINVAL;

        s = get_super(to_kdev_t(dev));
        if (s == NULL)
                goto out;
	err = vfs_statfs(s, &sbuf);
	drop_super(s);
	if (err)
		goto out;

        memset(&tmp,0,sizeof(struct ustat));
        tmp.f_tfree = sbuf.f_bfree;
        tmp.f_tinode = sbuf.f_ffree;

        err = copy_to_user(ubuf,&tmp,sizeof(struct ustat)) ? -EFAULT : 0;
out:
	return err;
}

/**
 *	do_remount_sb	-	asks filesystem to change mount options.
 *	@sb:	superblock in question
 *	@flags:	numeric part of options
 *	@data:	the rest of options
 *
 *	Alters the mount options of a mounted file system.
 */
int do_remount_sb(struct super_block *sb, int flags, void *data)
{
	int retval;
	
	if (!(flags & MS_RDONLY) && sb->s_dev && is_read_only(sb->s_dev))
		return -EACCES;
		/*flags |= MS_RDONLY;*/
	if (flags & MS_RDONLY)
		acct_auto_close(sb->s_dev);
	shrink_dcache_sb(sb);
	fsync_super(sb);
	/* If we are remounting RDONLY, make sure there are no rw files open */
	if ((flags & MS_RDONLY) && !(sb->s_flags & MS_RDONLY))
		if (!fs_may_remount_ro(sb))
			return -EBUSY;
	if (sb->s_op && sb->s_op->remount_fs) {
		lock_super(sb);
		retval = sb->s_op->remount_fs(sb, &flags, data);
		unlock_super(sb);
		if (retval)
			return retval;
	}
	sb->s_flags = (sb->s_flags & ~MS_RMT_MASK) | (flags & MS_RMT_MASK);
	return 0;
}

/*
 * Unnamed block devices are dummy devices used by virtual
 * filesystems which don't use real block-devices.  -- jrs
 */

enum {Max_anon = 256};
static unsigned long unnamed_dev_in_use[Max_anon/(8*sizeof(unsigned long))];
static spinlock_t unnamed_dev_lock = SPIN_LOCK_UNLOCKED;/* protects the above */

/**
 *	put_anon_dev	-	release anonymous device number.
 *	@dev:	device in question
 */
static void put_anon_dev(kdev_t dev)
{
	spin_lock(&unnamed_dev_lock);
	clear_bit(MINOR(dev), unnamed_dev_in_use);
	spin_unlock(&unnamed_dev_lock);
}

/**
 *	get_anon_super	-	allocate a superblock for non-device fs
 *	@type:		filesystem type
 *	@compare:	check if existing superblock is what we want
 *	@data:		argument for @compare.
 *
 *	get_anon_super is a helper for non-blockdevice filesystems.
 *	It either finds and returns one of the superblocks of given type
 *	(if it can find one that would satisfy caller) or creates a new
 *	one.  In the either case we return an active reference to superblock
 *	with ->s_umount locked.  If superblock is new it gets a new
 *	anonymous device allocated for it and is inserted into lists -
 *	other initialization is left to caller.
 *
 *	Rather than duplicating all that logics every time when
 *	we want something that doesn't fit "nodev" and "single" we pull
 *	the relevant code into common helper and let get_sb_...() call
 *	it.
 *
 *	NB: get_sb_...() is going to become an fs type method, with
 *	current ->read_super() becoming a callback used by common instances.
 */
struct super_block *get_anon_super(struct file_system_type *type,
	int (*compare)(struct super_block *,void *), void *data)
{
	struct super_block *s = alloc_super();
	kdev_t dev;
	struct list_head *p;

	if (!s)
		return ERR_PTR(-ENOMEM);

retry:
	spin_lock(&sb_lock);
	if (compare) list_for_each(p, &type->fs_supers) {
		struct super_block *old;
		old = list_entry(p, struct super_block, s_instances);
		if (!compare(old, data))
			continue;
		if (!grab_super(old))
			goto retry;
		destroy_super(s);
		return old;
	}

	spin_lock(&unnamed_dev_lock);
	dev = find_first_zero_bit(unnamed_dev_in_use, Max_anon);
	if (dev == Max_anon) {
		spin_unlock(&unnamed_dev_lock);
		spin_unlock(&sb_lock);
		destroy_super(s);
		return ERR_PTR(-EMFILE);
	}
	set_bit(dev, unnamed_dev_in_use);
	spin_unlock(&unnamed_dev_lock);

	s->s_dev = dev;
	insert_super(s, type);
	return s;
}

static struct super_block *get_sb_bdev(struct file_system_type *fs_type,
	int flags, char *dev_name, void * data)
{
	struct inode *inode;
	struct block_device *bdev;
	struct block_device_operations *bdops;
	devfs_handle_t de;
	struct super_block * s;
	struct nameidata nd;
	struct list_head *p;
	kdev_t dev;
	int error = 0;
	mode_t mode = FMODE_READ; /* we always need it ;-) */

	/* What device it is? */
	if (!dev_name || !*dev_name)
		return ERR_PTR(-EINVAL);
	error = path_lookup(dev_name, LOOKUP_FOLLOW|LOOKUP_POSITIVE, &nd);
	if (error)
		return ERR_PTR(error);
	inode = nd.dentry->d_inode;
	error = -ENOTBLK;
	if (!S_ISBLK(inode->i_mode))
		goto out;
	error = -EACCES;
	if (nd.mnt->mnt_flags & MNT_NODEV)
		goto out;
	bd_acquire(inode);
	bdev = inode->i_bdev;
	de = devfs_get_handle_from_inode (inode);
	bdops = devfs_get_ops (de);         /*  Increments module use count  */
	if (bdops) bdev->bd_op = bdops;
	/* Done with lookups, semaphore down */
	dev = to_kdev_t(bdev->bd_dev);
	if (!(flags & MS_RDONLY))
		mode |= FMODE_WRITE;
	error = blkdev_get(bdev, mode, 0, BDEV_FS);
	devfs_put_ops (de);   /*  Decrement module use count now we're safe  */
	if (error)
		goto out;
	check_disk_change(dev);
	error = -EACCES;
	if (!(flags & MS_RDONLY) && is_read_only(dev))
		goto out1;

	error = -ENOMEM;
	s = alloc_super();
	if (!s)
		goto out1;

	error = -EBUSY;
restart:
	spin_lock(&sb_lock);

	list_for_each(p, &super_blocks) {
		struct super_block *old = sb_entry(p);
		if (old->s_dev != dev)
			continue;
		if (old->s_type != fs_type ||
		    ((flags ^ old->s_flags) & MS_RDONLY)) {
			spin_unlock(&sb_lock);
			destroy_super(s);
			goto out1;
		}
		if (!grab_super(old))
			goto restart;
		destroy_super(s);
		blkdev_put(bdev, BDEV_FS);
		path_release(&nd);
		return old;
	}
	s->s_dev = dev;
	s->s_bdev = bdev;
	s->s_flags = flags;
	insert_super(s, fs_type);
	if (!fs_type->read_super(s, data, flags & MS_VERBOSE ? 1 : 0))
		goto Einval;
	s->s_flags |= MS_ACTIVE;
	path_release(&nd);
	return s;

Einval:
	deactivate_super(s);
	remove_super(s);
	error = -EINVAL;
	goto out;
out1:
	blkdev_put(bdev, BDEV_FS);
out:
	path_release(&nd);
	return ERR_PTR(error);
}

static struct super_block *get_sb_nodev(struct file_system_type *fs_type,
	int flags, char *dev_name, void *data)
{
	struct super_block *s = get_anon_super(fs_type, NULL, NULL);

	if (IS_ERR(s))
		return s;

	s->s_flags = flags;
	if (!fs_type->read_super(s, data, flags & MS_VERBOSE ? 1 : 0)) {
		deactivate_super(s);
		remove_super(s);
		return ERR_PTR(-EINVAL);
	}
	s->s_flags |= MS_ACTIVE;
	return s;
}

static int compare_single(struct super_block *s, void *p)
{
	return 1;
}

static struct super_block *get_sb_single(struct file_system_type *fs_type,
	int flags, char *dev_name, void *data)
{
	struct super_block *s = get_anon_super(fs_type, compare_single, NULL);

	if (IS_ERR(s))
		return s;
	if (!s->s_root) {
		s->s_flags = flags;
		if (!fs_type->read_super(s, data, flags & MS_VERBOSE ? 1 : 0)) {
			deactivate_super(s);
			remove_super(s);
			return ERR_PTR(-EINVAL);
		}
		s->s_flags |= MS_ACTIVE;
	}
	do_remount_sb(s, flags, data);
	return s;
}

struct vfsmount *
do_kern_mount(const char *fstype, int flags, char *name, void *data)
{
	struct file_system_type *type = get_fs_type(fstype);
	struct super_block *sb = ERR_PTR(-ENOMEM);
	struct vfsmount *mnt;

	if (!type)
		return ERR_PTR(-ENODEV);

	mnt = alloc_vfsmnt(name);
	if (!mnt)
		goto out;
	if (type->fs_flags & FS_REQUIRES_DEV)
		sb = get_sb_bdev(type, flags, name, data);
	else if (type->fs_flags & FS_SINGLE)
		sb = get_sb_single(type, flags, name, data);
	else
		sb = get_sb_nodev(type, flags, name, data);
	if (IS_ERR(sb))
		goto out_mnt;
	if (type->fs_flags & FS_NOMOUNT)
		sb->s_flags |= MS_NOUSER;
	mnt->mnt_sb = sb;
	mnt->mnt_root = dget(sb->s_root);
	mnt->mnt_mountpoint = sb->s_root;
	mnt->mnt_parent = mnt;
	up_write(&sb->s_umount);
	put_filesystem(type);
	return mnt;
out_mnt:
	free_vfsmnt(mnt);
out:
	put_filesystem(type);
	return (struct vfsmount *)sb;
}

void kill_super(struct super_block *sb)
{
	struct dentry *root = sb->s_root;
	struct file_system_type *fs = sb->s_type;
	struct super_operations *sop = sb->s_op;

	if (!deactivate_super(sb))
		return;

	down_write(&sb->s_umount);
	sb->s_root = NULL;
	/* Need to clean after the sucker */
	if (fs->fs_flags & FS_LITTER)
		d_genocide(root);
	shrink_dcache_parent(root);
	dput(root);
	fsync_super(sb);
	lock_super(sb);
	lock_kernel();
	sb->s_flags &= ~MS_ACTIVE;
	invalidate_inodes(sb);	/* bad name - it should be evict_inodes() */
	if (sop) {
		if (sop->write_super && sb->s_dirt)
			sop->write_super(sb);
		if (sop->put_super)
			sop->put_super(sb);
	}

	/* Forget any remaining inodes */
	if (invalidate_inodes(sb)) {
		printk(KERN_ERR "VFS: Busy inodes after unmount. "
			"Self-destruct in 5 seconds.  Have a nice day...\n");
	}

	unlock_kernel();
	unlock_super(sb);
	remove_super(sb);
}

struct vfsmount *kern_mount(struct file_system_type *type)
{
	return do_kern_mount(type->name, 0, (char *)type->name, NULL);
}
