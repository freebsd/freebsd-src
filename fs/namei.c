/*
 *  linux/fs/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

/* [Feb 1997 T. Schoebel-Theuer] Complete rewrite of the pathname
 * lookup logic.
 */
/* [Feb-Apr 2000, AV] Rewrite to the new namespace architecture.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/quotaops.h>
#include <linux/pagemap.h>
#include <linux/dnotify.h>
#include <linux/smp_lock.h>
#include <linux/personality.h>

#include <asm/namei.h>
#include <asm/uaccess.h>

#define ACC_MODE(x) ("\000\004\002\006"[(x)&O_ACCMODE])

/* [Feb-1997 T. Schoebel-Theuer]
 * Fundamental changes in the pathname lookup mechanisms (namei)
 * were necessary because of omirr.  The reason is that omirr needs
 * to know the _real_ pathname, not the user-supplied one, in case
 * of symlinks (and also when transname replacements occur).
 *
 * The new code replaces the old recursive symlink resolution with
 * an iterative one (in case of non-nested symlink chains).  It does
 * this with calls to <fs>_follow_link().
 * As a side effect, dir_namei(), _namei() and follow_link() are now 
 * replaced with a single function lookup_dentry() that can handle all 
 * the special cases of the former code.
 *
 * With the new dcache, the pathname is stored at each inode, at least as
 * long as the refcount of the inode is positive.  As a side effect, the
 * size of the dcache depends on the inode cache and thus is dynamic.
 *
 * [29-Apr-1998 C. Scott Ananian] Updated above description of symlink
 * resolution to correspond with current state of the code.
 *
 * Note that the symlink resolution is not *completely* iterative.
 * There is still a significant amount of tail- and mid- recursion in
 * the algorithm.  Also, note that <fs>_readlink() is not used in
 * lookup_dentry(): lookup_dentry() on the result of <fs>_readlink()
 * may return different results than <fs>_follow_link().  Many virtual
 * filesystems (including /proc) exhibit this behavior.
 */

/* [24-Feb-97 T. Schoebel-Theuer] Side effects caused by new implementation:
 * New symlink semantics: when open() is called with flags O_CREAT | O_EXCL
 * and the name already exists in form of a symlink, try to create the new
 * name indicated by the symlink. The old code always complained that the
 * name already exists, due to not following the symlink even if its target
 * is nonexistent.  The new semantics affects also mknod() and link() when
 * the name is a symlink pointing to a non-existant name.
 *
 * I don't know which semantics is the right one, since I have no access
 * to standards. But I found by trial that HP-UX 9.0 has the full "new"
 * semantics implemented, while SunOS 4.1.1 and Solaris (SunOS 5.4) have the
 * "old" one. Personally, I think the new semantics is much more logical.
 * Note that "ln old new" where "new" is a symlink pointing to a non-existing
 * file does succeed in both HP-UX and SunOs, but not in Solaris
 * and in the old Linux semantics.
 */

/* [16-Dec-97 Kevin Buhr] For security reasons, we change some symlink
 * semantics.  See the comments in "open_namei" and "do_link" below.
 *
 * [10-Sep-98 Alan Modra] Another symlink change.
 */

/* [Feb-Apr 2000 AV] Complete rewrite. Rules for symlinks:
 *	inside the path - always follow.
 *	in the last component in creation/removal/renaming - never follow.
 *	if LOOKUP_FOLLOW passed - follow.
 *	if the pathname has trailing slashes - follow.
 *	otherwise - don't follow.
 * (applied in that order).
 *
 * [Jun 2000 AV] Inconsistent behaviour of open() in case if flags==O_CREAT
 * restored for 2.4. This is the last surviving part of old 4.2BSD bug.
 * During the 2.4 we need to fix the userland stuff depending on it -
 * hopefully we will be able to get rid of that wart in 2.5. So far only
 * XEmacs seems to be relying on it...
 */

/* In order to reduce some races, while at the same time doing additional
 * checking and hopefully speeding things up, we copy filenames to the
 * kernel data space before using them..
 *
 * POSIX.1 2.4: an empty pathname is invalid (ENOENT).
 * PATH_MAX includes the nul terminator --RR.
 */
static inline int do_getname(const char *filename, char *page)
{
	int retval;
	unsigned long len = PATH_MAX;

	if ((unsigned long) filename >= TASK_SIZE) {
		if (!segment_eq(get_fs(), KERNEL_DS))
			return -EFAULT;
	} else if (TASK_SIZE - (unsigned long) filename < PATH_MAX)
		len = TASK_SIZE - (unsigned long) filename;

	retval = strncpy_from_user((char *)page, filename, len);
	if (retval > 0) {
		if (retval < len)
			return 0;
		return -ENAMETOOLONG;
	} else if (!retval)
		retval = -ENOENT;
	return retval;
}

char * getname(const char * filename)
{
	char *tmp, *result;

	result = ERR_PTR(-ENOMEM);
	tmp = __getname();
	if (tmp)  {
		int retval = do_getname(filename, tmp);

		result = tmp;
		if (retval < 0) {
			putname(tmp);
			result = ERR_PTR(retval);
		}
	}
	return result;
}

/*
 *	vfs_permission()
 *
 * is used to check for read/write/execute permissions on a file.
 * We use "fsuid" for this, letting us set arbitrary permissions
 * for filesystem access without changing the "normal" uids which
 * are used for other things..
 */
int vfs_permission(struct inode * inode, int mask)
{
	umode_t			mode = inode->i_mode;

	if (mask & MAY_WRITE) {
		/*
		 * Nobody gets write access to a read-only fs.
		 */
		if (IS_RDONLY(inode) &&
		    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
			return -EROFS;

		/*
		 * Nobody gets write access to an immutable file.
		 */
		if (IS_IMMUTABLE(inode))
			return -EACCES;
	}

	if (current->fsuid == inode->i_uid)
		mode >>= 6;
	else if (in_group_p(inode->i_gid))
		mode >>= 3;

	/*
	 * If the DACs are ok we don't need any capability check.
	 */
	if (((mode & mask & (MAY_READ|MAY_WRITE|MAY_EXEC)) == mask))
		return 0;

	/*
	 * Read/write DACs are always overridable.
	 * Executable DACs are overridable if at least one exec bit is set.
	 */
	if ((mask & (MAY_READ|MAY_WRITE)) || (inode->i_mode & S_IXUGO))
		if (capable(CAP_DAC_OVERRIDE))
			return 0;

	/*
	 * Searching includes executable on directories, else just read.
	 */
	if (mask == MAY_READ || (S_ISDIR(inode->i_mode) && !(mask & MAY_WRITE)))
		if (capable(CAP_DAC_READ_SEARCH))
			return 0;

	return -EACCES;
}

int permission(struct inode * inode,int mask)
{
	if (inode->i_op && inode->i_op->permission) {
		int retval;
		lock_kernel();
		retval = inode->i_op->permission(inode, mask);
		unlock_kernel();
		return retval;
	}
	return vfs_permission(inode, mask);
}

/*
 * get_write_access() gets write permission for a file.
 * put_write_access() releases this write permission.
 * This is used for regular files.
 * We cannot support write (and maybe mmap read-write shared) accesses and
 * MAP_DENYWRITE mmappings simultaneously. The i_writecount field of an inode
 * can have the following values:
 * 0: no writers, no VM_DENYWRITE mappings
 * < 0: (-i_writecount) vm_area_structs with VM_DENYWRITE set exist
 * > 0: (i_writecount) users are writing to the file.
 *
 * Normally we operate on that counter with atomic_{inc,dec} and it's safe
 * except for the cases where we don't hold i_writecount yet. Then we need to
 * use {get,deny}_write_access() - these functions check the sign and refuse
 * to do the change if sign is wrong. Exclusion between them is provided by
 * spinlock (arbitration_lock) and I'll rip the second arsehole to the first
 * who will try to move it in struct inode - just leave it here.
 */
static spinlock_t arbitration_lock = SPIN_LOCK_UNLOCKED;
int get_write_access(struct inode * inode)
{
	spin_lock(&arbitration_lock);
	if (atomic_read(&inode->i_writecount) < 0) {
		spin_unlock(&arbitration_lock);
		return -ETXTBSY;
	}
	atomic_inc(&inode->i_writecount);
	spin_unlock(&arbitration_lock);
	return 0;
}
int deny_write_access(struct file * file)
{
	spin_lock(&arbitration_lock);
	if (atomic_read(&file->f_dentry->d_inode->i_writecount) > 0) {
		spin_unlock(&arbitration_lock);
		return -ETXTBSY;
	}
	atomic_dec(&file->f_dentry->d_inode->i_writecount);
	spin_unlock(&arbitration_lock);
	return 0;
}

void path_release(struct nameidata *nd)
{
	dput(nd->dentry);
	mntput(nd->mnt);
}

/*
 * Internal lookup() using the new generic dcache.
 * SMP-safe
 */
static struct dentry * cached_lookup(struct dentry * parent, struct qstr * name, int flags)
{
	struct dentry * dentry = d_lookup(parent, name);

	if (dentry && dentry->d_op && dentry->d_op->d_revalidate) {
		if (!dentry->d_op->d_revalidate(dentry, flags) && !d_invalidate(dentry)) {
			dput(dentry);
			dentry = NULL;
		}
	}
	return dentry;
}

/*
 * This is called when everything else fails, and we actually have
 * to go to the low-level filesystem to find out what we should do..
 *
 * We get the directory semaphore, and after getting that we also
 * make sure that nobody added the entry to the dcache in the meantime..
 * SMP-safe
 */
static struct dentry * real_lookup(struct dentry * parent, struct qstr * name, int flags)
{
	struct dentry * result;
	struct inode *dir = parent->d_inode;

	down(&dir->i_sem);
	/*
	 * First re-do the cached lookup just in case it was created
	 * while we waited for the directory semaphore..
	 *
	 * FIXME! This could use version numbering or similar to
	 * avoid unnecessary cache lookups.
	 */
	result = d_lookup(parent, name);
	if (!result) {
		struct dentry * dentry = d_alloc(parent, name);
		result = ERR_PTR(-ENOMEM);
		if (dentry) {
			lock_kernel();
			result = dir->i_op->lookup(dir, dentry);
			unlock_kernel();
			if (result)
				dput(dentry);
			else
				result = dentry;
		}
		up(&dir->i_sem);
		return result;
	}

	/*
	 * Uhhuh! Nasty case: the cache was re-populated while
	 * we waited on the semaphore. Need to revalidate.
	 */
	up(&dir->i_sem);
	if (result->d_op && result->d_op->d_revalidate) {
		if (!result->d_op->d_revalidate(result, flags) && !d_invalidate(result)) {
			dput(result);
			result = ERR_PTR(-ENOENT);
		}
	}
	return result;
}

/*
 * This limits recursive symlink follows to 8, while
 * limiting consecutive symlinks to 40.
 *
 * Without that kind of total limit, nasty chains of consecutive
 * symlinks can cause almost arbitrarily long lookups. 
 */
static inline int do_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	int err;
	if (current->link_count >= 5)
		goto loop;
	if (current->total_link_count >= 40)
		goto loop;
	if (current->need_resched) {
		current->state = TASK_RUNNING;
		schedule();
	}
	current->link_count++;
	current->total_link_count++;
	UPDATE_ATIME(dentry->d_inode);
	err = dentry->d_inode->i_op->follow_link(dentry, nd);
	current->link_count--;
	return err;
loop:
	path_release(nd);
	return -ELOOP;
}

static inline int __follow_up(struct vfsmount **mnt, struct dentry **base)
{
	struct vfsmount *parent;
	struct dentry *dentry;
	spin_lock(&dcache_lock);
	parent=(*mnt)->mnt_parent;
	if (parent == *mnt) {
		spin_unlock(&dcache_lock);
		return 0;
	}
	mntget(parent);
	dentry=dget((*mnt)->mnt_mountpoint);
	spin_unlock(&dcache_lock);
	dput(*base);
	*base = dentry;
	mntput(*mnt);
	*mnt = parent;
	return 1;
}

int follow_up(struct vfsmount **mnt, struct dentry **dentry)
{
	return __follow_up(mnt, dentry);
}

static inline int __follow_down(struct vfsmount **mnt, struct dentry **dentry)
{
	struct vfsmount *mounted;

	spin_lock(&dcache_lock);
	mounted = lookup_mnt(*mnt, *dentry);
	if (mounted) {
		*mnt = mntget(mounted);
		spin_unlock(&dcache_lock);
		dput(*dentry);
		mntput(mounted->mnt_parent);
		*dentry = dget(mounted->mnt_root);
		return 1;
	}
	spin_unlock(&dcache_lock);
	return 0;
}

int follow_down(struct vfsmount **mnt, struct dentry **dentry)
{
	return __follow_down(mnt,dentry);
}
 
static inline void follow_dotdot(struct nameidata *nd)
{
	while(1) {
		struct vfsmount *parent;
		struct dentry *dentry;
		read_lock(&current->fs->lock);
		if (nd->dentry == current->fs->root &&
		    nd->mnt == current->fs->rootmnt)  {
			read_unlock(&current->fs->lock);
			break;
		}
		read_unlock(&current->fs->lock);
		spin_lock(&dcache_lock);
		if (nd->dentry != nd->mnt->mnt_root) {
			dentry = dget(nd->dentry->d_parent);
			spin_unlock(&dcache_lock);
			dput(nd->dentry);
			nd->dentry = dentry;
			break;
		}
		parent=nd->mnt->mnt_parent;
		if (parent == nd->mnt) {
			spin_unlock(&dcache_lock);
			break;
		}
		mntget(parent);
		dentry=dget(nd->mnt->mnt_mountpoint);
		spin_unlock(&dcache_lock);
		dput(nd->dentry);
		nd->dentry = dentry;
		mntput(nd->mnt);
		nd->mnt = parent;
	}
	while (d_mountpoint(nd->dentry) && __follow_down(&nd->mnt, &nd->dentry))
		;
}

/*
 * Name resolution.
 *
 * This is the basic name resolution function, turning a pathname
 * into the final dentry.
 *
 * We expect 'base' to be positive and a directory.
 */
int link_path_walk(const char * name, struct nameidata *nd)
{
	struct dentry *dentry;
	struct inode *inode;
	int err;
	unsigned int lookup_flags = nd->flags;

	while (*name=='/')
		name++;
	if (!*name)
		goto return_reval;

	inode = nd->dentry->d_inode;
	if (current->link_count)
		lookup_flags = LOOKUP_FOLLOW;

	/* At this point we know we have a real path component. */
	for(;;) {
		unsigned long hash;
		struct qstr this;
		unsigned int c;

		err = permission(inode, MAY_EXEC);
		dentry = ERR_PTR(err);
 		if (err)
			break;

		this.name = name;
		c = *(const unsigned char *)name;

		hash = init_name_hash();
		do {
			name++;
			hash = partial_name_hash(c, hash);
			c = *(const unsigned char *)name;
		} while (c && (c != '/'));
		this.len = name - (const char *) this.name;
		this.hash = end_name_hash(hash);

		/* remove trailing slashes? */
		if (!c)
			goto last_component;
		while (*++name == '/');
		if (!*name)
			goto last_with_slashes;

		/*
		 * "." and ".." are special - ".." especially so because it has
		 * to be able to know about the current root directory and
		 * parent relationships.
		 */
		if (this.name[0] == '.') switch (this.len) {
			default:
				break;
			case 2:	
				if (this.name[1] != '.')
					break;
				follow_dotdot(nd);
				inode = nd->dentry->d_inode;
				/* fallthrough */
			case 1:
				continue;
		}
		/*
		 * See if the low-level filesystem might want
		 * to use its own hash..
		 */
		if (nd->dentry->d_op && nd->dentry->d_op->d_hash) {
			err = nd->dentry->d_op->d_hash(nd->dentry, &this);
			if (err < 0)
				break;
		}
		/* This does the actual lookups.. */
		dentry = cached_lookup(nd->dentry, &this, LOOKUP_CONTINUE);
		if (!dentry) {
			dentry = real_lookup(nd->dentry, &this, LOOKUP_CONTINUE);
			err = PTR_ERR(dentry);
			if (IS_ERR(dentry))
				break;
		}
		/* Check mountpoints.. */
		while (d_mountpoint(dentry) && __follow_down(&nd->mnt, &dentry))
			;

		err = -ENOENT;
		inode = dentry->d_inode;
		if (!inode)
			goto out_dput;
		err = -ENOTDIR; 
		if (!inode->i_op)
			goto out_dput;

		if (inode->i_op->follow_link) {
			err = do_follow_link(dentry, nd);
			dput(dentry);
			if (err)
				goto return_err;
			err = -ENOENT;
			inode = nd->dentry->d_inode;
			if (!inode)
				break;
			err = -ENOTDIR; 
			if (!inode->i_op)
				break;
		} else {
			dput(nd->dentry);
			nd->dentry = dentry;
		}
		err = -ENOTDIR; 
		if (!inode->i_op->lookup)
			break;
		continue;
		/* here ends the main loop */

last_with_slashes:
		lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
last_component:
		if (lookup_flags & LOOKUP_PARENT)
			goto lookup_parent;
		if (this.name[0] == '.') switch (this.len) {
			default:
				break;
			case 2:	
				if (this.name[1] != '.')
					break;
				follow_dotdot(nd);
				inode = nd->dentry->d_inode;
				/* fallthrough */
			case 1:
				goto return_reval;
		}
		if (nd->dentry->d_op && nd->dentry->d_op->d_hash) {
			err = nd->dentry->d_op->d_hash(nd->dentry, &this);
			if (err < 0)
				break;
		}
		dentry = cached_lookup(nd->dentry, &this, 0);
		if (!dentry) {
			dentry = real_lookup(nd->dentry, &this, 0);
			err = PTR_ERR(dentry);
			if (IS_ERR(dentry))
				break;
		}
		while (d_mountpoint(dentry) && __follow_down(&nd->mnt, &dentry))
			;
		inode = dentry->d_inode;
		if ((lookup_flags & LOOKUP_FOLLOW)
		    && inode && inode->i_op && inode->i_op->follow_link) {
			err = do_follow_link(dentry, nd);
			dput(dentry);
			if (err)
				goto return_err;
			inode = nd->dentry->d_inode;
		} else {
			dput(nd->dentry);
			nd->dentry = dentry;
		}
		err = -ENOENT;
		if (!inode)
			goto no_inode;
		if (lookup_flags & LOOKUP_DIRECTORY) {
			err = -ENOTDIR; 
			if (!inode->i_op || !inode->i_op->lookup)
				break;
		}
		goto return_base;
no_inode:
		err = -ENOENT;
		if (lookup_flags & (LOOKUP_POSITIVE|LOOKUP_DIRECTORY))
			break;
		goto return_base;
lookup_parent:
		nd->last = this;
		nd->last_type = LAST_NORM;
		if (this.name[0] != '.')
			goto return_base;
		if (this.len == 1)
			nd->last_type = LAST_DOT;
		else if (this.len == 2 && this.name[1] == '.')
			nd->last_type = LAST_DOTDOT;
		else
			goto return_base;
return_reval:
		/*
		 * We bypassed the ordinary revalidation routines.
		 * Check the cached dentry for staleness.
		 */
		dentry = nd->dentry;
		if (dentry && dentry->d_op && dentry->d_op->d_revalidate) {
			err = -ESTALE;
			if (!dentry->d_op->d_revalidate(dentry, 0)) {
				d_invalidate(dentry);
				break;
			}
		}
return_base:
		return 0;
out_dput:
		dput(dentry);
		break;
	}
	path_release(nd);
return_err:
	return err;
}

int path_walk(const char * name, struct nameidata *nd)
{
	current->total_link_count = 0;
	return link_path_walk(name, nd);
}

/* SMP-safe */
/* returns 1 if everything is done */
static int __emul_lookup_dentry(const char *name, struct nameidata *nd)
{
	if (path_walk(name, nd))
		return 0;		/* something went wrong... */

	if (!nd->dentry->d_inode || S_ISDIR(nd->dentry->d_inode->i_mode)) {
		struct nameidata nd_root;
		/*
		 * NAME was not found in alternate root or it's a directory.  Try to find
		 * it in the normal root:
		 */
		nd_root.last_type = LAST_ROOT;
		nd_root.flags = nd->flags;
		read_lock(&current->fs->lock);
		nd_root.mnt = mntget(current->fs->rootmnt);
		nd_root.dentry = dget(current->fs->root);
		read_unlock(&current->fs->lock);
		if (path_walk(name, &nd_root))
			return 1;
		if (nd_root.dentry->d_inode) {
			path_release(nd);
			nd->dentry = nd_root.dentry;
			nd->mnt = nd_root.mnt;
			nd->last = nd_root.last;
			return 1;
		}
		path_release(&nd_root);
	}
	return 1;
}

void set_fs_altroot(void)
{
	char *emul = __emul_prefix();
	struct nameidata nd;
	struct vfsmount *mnt = NULL, *oldmnt;
	struct dentry *dentry = NULL, *olddentry;
	if (emul) {
		read_lock(&current->fs->lock);
		nd.mnt = mntget(current->fs->rootmnt);
		nd.dentry = dget(current->fs->root);
		read_unlock(&current->fs->lock);
		nd.flags = LOOKUP_FOLLOW|LOOKUP_DIRECTORY|LOOKUP_POSITIVE;
		if (path_walk(emul,&nd) == 0) {
			mnt = nd.mnt;
			dentry = nd.dentry;
		}
	}
	write_lock(&current->fs->lock);
	oldmnt = current->fs->altrootmnt;
	olddentry = current->fs->altroot;
	current->fs->altrootmnt = mnt;
	current->fs->altroot = dentry;
	write_unlock(&current->fs->lock);
	if (olddentry) {
		dput(olddentry);
		mntput(oldmnt);
	}
}

/* SMP-safe */
static inline int
walk_init_root(const char *name, struct nameidata *nd)
{
	read_lock(&current->fs->lock);
	if (current->fs->altroot && !(nd->flags & LOOKUP_NOALT)) {
		nd->mnt = mntget(current->fs->altrootmnt);
		nd->dentry = dget(current->fs->altroot);
		read_unlock(&current->fs->lock);
		if (__emul_lookup_dentry(name,nd))
			return 0;
		read_lock(&current->fs->lock);
	}
	nd->mnt = mntget(current->fs->rootmnt);
	nd->dentry = dget(current->fs->root);
	read_unlock(&current->fs->lock);
	return 1;
}

/* SMP-safe */
int path_lookup(const char *path, unsigned flags, struct nameidata *nd)
{
	int error = 0;
	if (path_init(path, flags, nd))
		error = path_walk(path, nd);
	return error;
}


/* SMP-safe */
int path_init(const char *name, unsigned int flags, struct nameidata *nd)
{
	nd->last_type = LAST_ROOT; /* if there are only slashes... */
	nd->flags = flags;
	if (*name=='/')
		return walk_init_root(name,nd);
	read_lock(&current->fs->lock);
	nd->mnt = mntget(current->fs->pwdmnt);
	nd->dentry = dget(current->fs->pwd);
	read_unlock(&current->fs->lock);
	return 1;
}

/*
 * Restricted form of lookup. Doesn't follow links, single-component only,
 * needs parent already locked. Doesn't follow mounts.
 * SMP-safe.
 */
struct dentry * lookup_hash(struct qstr *name, struct dentry * base)
{
	struct dentry * dentry;
	struct inode *inode;
	int err;

	inode = base->d_inode;
	err = permission(inode, MAY_EXEC);
	dentry = ERR_PTR(err);
	if (err)
		goto out;

	/*
	 * See if the low-level filesystem might want
	 * to use its own hash..
	 */
	if (base->d_op && base->d_op->d_hash) {
		err = base->d_op->d_hash(base, name);
		dentry = ERR_PTR(err);
		if (err < 0)
			goto out;
	}

	dentry = cached_lookup(base, name, 0);
	if (!dentry) {
		struct dentry *new = d_alloc(base, name);
		dentry = ERR_PTR(-ENOMEM);
		if (!new)
			goto out;
		lock_kernel();
		dentry = inode->i_op->lookup(inode, new);
		unlock_kernel();
		if (!dentry)
			dentry = new;
		else
			dput(new);
	}
out:
	return dentry;
}

/* SMP-safe */
struct dentry * lookup_one_len(const char * name, struct dentry * base, int len)
{
	unsigned long hash;
	struct qstr this;
	unsigned int c;

	this.name = name;
	this.len = len;
	if (!len)
		goto access;

	hash = init_name_hash();
	while (len--) {
		c = *(const unsigned char *)name++;
		if (c == '/' || c == '\0')
			goto access;
		hash = partial_name_hash(c, hash);
	}
	this.hash = end_name_hash(hash);

	return lookup_hash(&this, base);
access:
	return ERR_PTR(-EACCES);
}

/*
 *	namei()
 *
 * is used by most simple commands to get the inode of a specified name.
 * Open, link etc use their own routines, but this is enough for things
 * like 'chmod' etc.
 *
 * namei exists in two versions: namei/lnamei. The only difference is
 * that namei follows links, while lnamei does not.
 * SMP-safe
 */
int __user_walk(const char *name, unsigned flags, struct nameidata *nd)
{
	char *tmp;
	int err;

	tmp = getname(name);
	err = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {
		err = 0;
		err = path_lookup(tmp, flags, nd);
		putname(tmp);
	}
	return err;
}

/*
 * It's inline, so penalty for filesystems that don't use sticky bit is
 * minimal.
 */
static inline int check_sticky(struct inode *dir, struct inode *inode)
{
	if (!(dir->i_mode & S_ISVTX))
		return 0;
	if (inode->i_uid == current->fsuid)
		return 0;
	if (dir->i_uid == current->fsuid)
		return 0;
	return !capable(CAP_FOWNER);
}

/*
 *	Check whether we can remove a link victim from directory dir, check
 *  whether the type of victim is right.
 *  1. We can't do it if dir is read-only (done in permission())
 *  2. We should have write and exec permissions on dir
 *  3. We can't remove anything from append-only dir
 *  4. We can't do anything with immutable dir (done in permission())
 *  5. If the sticky bit on dir is set we should either
 *	a. be owner of dir, or
 *	b. be owner of victim, or
 *	c. have CAP_FOWNER capability
 *  6. If the victim is append-only or immutable we can't do antyhing with
 *     links pointing to it.
 *  7. If we were asked to remove a directory and victim isn't one - ENOTDIR.
 *  8. If we were asked to remove a non-directory and victim isn't one - EISDIR.
 *  9. We can't remove a root or mountpoint.
 */
static inline int may_delete(struct inode *dir,struct dentry *victim, int isdir)
{
	int error;
	if (!victim->d_inode || victim->d_parent->d_inode != dir)
		return -ENOENT;
	error = permission(dir,MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (IS_APPEND(dir))
		return -EPERM;
	if (check_sticky(dir, victim->d_inode)||IS_APPEND(victim->d_inode)||
	    IS_IMMUTABLE(victim->d_inode))
		return -EPERM;
	if (isdir) {
		if (!S_ISDIR(victim->d_inode->i_mode))
			return -ENOTDIR;
		if (IS_ROOT(victim))
			return -EBUSY;
	} else if (S_ISDIR(victim->d_inode->i_mode))
		return -EISDIR;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	return 0;
}

/*	Check whether we can create an object with dentry child in directory
 *  dir.
 *  1. We can't do it if child already exists (open has special treatment for
 *     this case, but since we are inlined it's OK)
 *  2. We can't do it if dir is read-only (done in permission())
 *  3. We should have write and exec permissions on dir
 *  4. We can't do it if dir is immutable (done in permission())
 */
static inline int may_create(struct inode *dir, struct dentry *child) {
	if (child->d_inode)
		return -EEXIST;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	return permission(dir,MAY_WRITE | MAY_EXEC);
}

/* 
 * Special case: O_CREAT|O_EXCL implies O_NOFOLLOW for security
 * reasons.
 *
 * O_DIRECTORY translates into forcing a directory lookup.
 */
static inline int lookup_flags(unsigned int f)
{
	unsigned long retval = LOOKUP_FOLLOW;

	if (f & O_NOFOLLOW)
		retval &= ~LOOKUP_FOLLOW;
	
	if ((f & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL))
		retval &= ~LOOKUP_FOLLOW;
	
	if (f & O_DIRECTORY)
		retval |= LOOKUP_DIRECTORY;

	return retval;
}

int vfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
	int error;

	mode &= S_IALLUGO;
	mode |= S_IFREG;

	down(&dir->i_zombie);
	error = may_create(dir, dentry);
	if (error)
		goto exit_lock;

	error = -EACCES;	/* shouldn't it be ENOSYS? */
	if (!dir->i_op || !dir->i_op->create)
		goto exit_lock;

	DQUOT_INIT(dir);
	lock_kernel();
	error = dir->i_op->create(dir, dentry, mode);
	unlock_kernel();
exit_lock:
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
	return error;
}

/*
 *	open_namei()
 *
 * namei for open - this is in fact almost the whole open-routine.
 *
 * Note that the low bits of "flag" aren't the same as in the open
 * system call - they are 00 - no permissions needed
 *			  01 - read permission needed
 *			  10 - write permission needed
 *			  11 - read/write permissions needed
 * which is a lot more logical, and also allows the "no perm" needed
 * for symlinks (where the permissions are checked later).
 * SMP-safe
 */
int open_namei(const char * pathname, int flag, int mode, struct nameidata *nd)
{
	int acc_mode, error = 0;
	struct inode *inode;
	struct dentry *dentry;
	struct dentry *dir;
	int count = 0;

	acc_mode = ACC_MODE(flag);

	/*
	 * The simplest case - just a plain lookup.
	 */
	if (!(flag & O_CREAT)) {
		error = path_lookup(pathname, lookup_flags(flag), nd);
		if (error)
			return error;
		dentry = nd->dentry;
		goto ok;
	}

	/*
	 * Create - we need to know the parent.
	 */
	error = path_lookup(pathname, LOOKUP_PARENT, nd);
	if (error)
		return error;

	/*
	 * We have the parent and last component. First of all, check
	 * that we are not asked to creat(2) an obvious directory - that
	 * will not do.
	 */
	error = -EISDIR;
	if (nd->last_type != LAST_NORM || nd->last.name[nd->last.len])
		goto exit;

	dir = nd->dentry;
	down(&dir->d_inode->i_sem);
	dentry = lookup_hash(&nd->last, nd->dentry);

do_last:
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry)) {
		up(&dir->d_inode->i_sem);
		goto exit;
	}

	/* Negative dentry, just create the file */
	if (!dentry->d_inode) {
		error = vfs_create(dir->d_inode, dentry,
				   mode & ~current->fs->umask);
		up(&dir->d_inode->i_sem);
		dput(nd->dentry);
		nd->dentry = dentry;
		if (error)
			goto exit;
		/* Don't check for write permission, don't truncate */
		acc_mode = 0;
		flag &= ~O_TRUNC;
		goto ok;
	}

	/*
	 * It already exists.
	 */
	up(&dir->d_inode->i_sem);

	error = -EEXIST;
	if (flag & O_EXCL)
		goto exit_dput;

	if (d_mountpoint(dentry)) {
		error = -ELOOP;
		if (flag & O_NOFOLLOW)
			goto exit_dput;
		while (__follow_down(&nd->mnt,&dentry) && d_mountpoint(dentry));
	}
	error = -ENOENT;
	if (!dentry->d_inode)
		goto exit_dput;
	if (dentry->d_inode->i_op && dentry->d_inode->i_op->follow_link)
		goto do_link;

	dput(nd->dentry);
	nd->dentry = dentry;
	error = -EISDIR;
	if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode))
		goto exit;
ok:
	error = -ENOENT;
	inode = dentry->d_inode;
	if (!inode)
		goto exit;

	error = -ELOOP;
	if (S_ISLNK(inode->i_mode))
		goto exit;
	
	error = -EISDIR;
	if (S_ISDIR(inode->i_mode) && (flag & FMODE_WRITE))
		goto exit;

	error = permission(inode,acc_mode);
	if (error)
		goto exit;

	/*
	 * FIFO's, sockets and device files are special: they don't
	 * actually live on the filesystem itself, and as such you
	 * can write to them even if the filesystem is read-only.
	 */
	if (S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
	    	flag &= ~O_TRUNC;
	} else if (S_ISBLK(inode->i_mode) || S_ISCHR(inode->i_mode)) {
		error = -EACCES;
		if (nd->mnt->mnt_flags & MNT_NODEV)
			goto exit;

		flag &= ~O_TRUNC;
	} else {
		error = -EROFS;
		if (IS_RDONLY(inode) && (flag & 2))
			goto exit;
	}
	/*
	 * An append-only file must be opened in append mode for writing.
	 */
	error = -EPERM;
	if (IS_APPEND(inode)) {
		if  ((flag & FMODE_WRITE) && !(flag & O_APPEND))
			goto exit;
		if (flag & O_TRUNC)
			goto exit;
	}

	/*
	 * Ensure there are no outstanding leases on the file.
	 */
	error = get_lease(inode, flag);
	if (error)
		goto exit;

	if (flag & O_TRUNC) {
		error = get_write_access(inode);
		if (error)
			goto exit;

		/*
		 * Refuse to truncate files with mandatory locks held on them.
		 */
		error = locks_verify_locked(inode);
		if (!error) {
			DQUOT_INIT(inode);
			
			error = do_truncate(dentry, 0);
		}
		put_write_access(inode);
		if (error)
			goto exit;
	} else
		if (flag & FMODE_WRITE)
			DQUOT_INIT(inode);

	return 0;

exit_dput:
	dput(dentry);
exit:
	path_release(nd);
	return error;

do_link:
	error = -ELOOP;
	if (flag & O_NOFOLLOW)
		goto exit_dput;
	/*
	 * This is subtle. Instead of calling do_follow_link() we do the
	 * thing by hands. The reason is that this way we have zero link_count
	 * and path_walk() (called from ->follow_link) honoring LOOKUP_PARENT.
	 * After that we have the parent and last component, i.e.
	 * we are in the same situation as after the first path_walk().
	 * Well, almost - if the last component is normal we get its copy
	 * stored in nd->last.name and we will have to putname() it when we
	 * are done. Procfs-like symlinks just set LAST_BIND.
	 */
	UPDATE_ATIME(dentry->d_inode);
	error = dentry->d_inode->i_op->follow_link(dentry, nd);
	dput(dentry);
	if (error)
		return error;
	if (nd->last_type == LAST_BIND) {
		dentry = nd->dentry;
		goto ok;
	}
	error = -EISDIR;
	if (nd->last_type != LAST_NORM)
		goto exit;
	if (nd->last.name[nd->last.len]) {
		putname(nd->last.name);
		goto exit;
	}
	error = -ELOOP;
	if (count++==32) {
		putname(nd->last.name);
		goto exit;
	}
	dir = nd->dentry;
	down(&dir->d_inode->i_sem);
	dentry = lookup_hash(&nd->last, nd->dentry);
	putname(nd->last.name);
	goto do_last;
}

/* SMP-safe */
static struct dentry *lookup_create(struct nameidata *nd, int is_dir)
{
	struct dentry *dentry;

	down(&nd->dentry->d_inode->i_sem);
	dentry = ERR_PTR(-EEXIST);
	if (nd->last_type != LAST_NORM)
		goto fail;
	dentry = lookup_hash(&nd->last, nd->dentry);
	if (IS_ERR(dentry))
		goto fail;
	if (!is_dir && nd->last.name[nd->last.len] && !dentry->d_inode)
		goto enoent;
	return dentry;
enoent:
	dput(dentry);
	dentry = ERR_PTR(-ENOENT);
fail:
	return dentry;
}

int vfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	int error = -EPERM;

	down(&dir->i_zombie);
	if ((S_ISCHR(mode) || S_ISBLK(mode)) && !capable(CAP_MKNOD))
		goto exit_lock;

	error = may_create(dir, dentry);
	if (error)
		goto exit_lock;

	error = -EPERM;
	if (!dir->i_op || !dir->i_op->mknod)
		goto exit_lock;

	DQUOT_INIT(dir);
	lock_kernel();
	error = dir->i_op->mknod(dir, dentry, mode, dev);
	unlock_kernel();
exit_lock:
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
	return error;
}

asmlinkage long sys_mknod(const char * filename, int mode, dev_t dev)
{
	int error = 0;
	char * tmp;
	struct dentry * dentry;
	struct nameidata nd;

	if (S_ISDIR(mode))
		return -EPERM;
	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	error = path_lookup(tmp, LOOKUP_PARENT, &nd);
	if (error)
		goto out;
	dentry = lookup_create(&nd, 0);
	error = PTR_ERR(dentry);

	mode &= ~current->fs->umask;
	if (!IS_ERR(dentry)) {
		switch (mode & S_IFMT) {
		case 0: case S_IFREG:
			error = vfs_create(nd.dentry->d_inode,dentry,mode);
			break;
		case S_IFCHR: case S_IFBLK: case S_IFIFO: case S_IFSOCK:
			error = vfs_mknod(nd.dentry->d_inode,dentry,mode,dev);
			break;
		case S_IFDIR:
			error = -EPERM;
			break;
		default:
			error = -EINVAL;
		}
		dput(dentry);
	}
	up(&nd.dentry->d_inode->i_sem);
	path_release(&nd);
out:
	putname(tmp);

	return error;
}

int vfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int error;

	down(&dir->i_zombie);
	error = may_create(dir, dentry);
	if (error)
		goto exit_lock;

	error = -EPERM;
	if (!dir->i_op || !dir->i_op->mkdir)
		goto exit_lock;

	DQUOT_INIT(dir);
	mode &= (S_IRWXUGO|S_ISVTX);
	lock_kernel();
	error = dir->i_op->mkdir(dir, dentry, mode);
	unlock_kernel();

exit_lock:
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
	return error;
}

asmlinkage long sys_mkdir(const char * pathname, int mode)
{
	int error = 0;
	char * tmp;

	tmp = getname(pathname);
	error = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {
		struct dentry *dentry;
		struct nameidata nd;

		error = path_lookup(tmp, LOOKUP_PARENT, &nd);
		if (error)
			goto out;
		dentry = lookup_create(&nd, 1);
		error = PTR_ERR(dentry);
		if (!IS_ERR(dentry)) {
			error = vfs_mkdir(nd.dentry->d_inode, dentry,
					  mode & ~current->fs->umask);
			dput(dentry);
		}
		up(&nd.dentry->d_inode->i_sem);
		path_release(&nd);
out:
		putname(tmp);
	}

	return error;
}

/*
 * We try to drop the dentry early: we should have
 * a usage count of 2 if we're the only user of this
 * dentry, and if that is true (possibly after pruning
 * the dcache), then we drop the dentry now.
 *
 * A low-level filesystem can, if it choses, legally
 * do a
 *
 *	if (!d_unhashed(dentry))
 *		return -EBUSY;
 *
 * if it cannot handle the case of removing a directory
 * that is still in use by something else..
 */
static void d_unhash(struct dentry *dentry)
{
	dget(dentry);
	spin_lock(&dcache_lock);
	switch (atomic_read(&dentry->d_count)) {
	default:
		spin_unlock(&dcache_lock);
		shrink_dcache_parent(dentry);
		spin_lock(&dcache_lock);
		if (atomic_read(&dentry->d_count) != 2)
			break;
	case 2:
		list_del_init(&dentry->d_hash);
	}
	spin_unlock(&dcache_lock);
}

int vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;

	error = may_delete(dir, dentry, 1);
	if (error)
		return error;

	if (!dir->i_op || !dir->i_op->rmdir)
		return -EPERM;

	DQUOT_INIT(dir);

	double_down(&dir->i_zombie, &dentry->d_inode->i_zombie);
	d_unhash(dentry);
	if (d_mountpoint(dentry))
		error = -EBUSY;
	else {
		lock_kernel();
		error = dir->i_op->rmdir(dir, dentry);
		unlock_kernel();
		if (!error)
			dentry->d_inode->i_flags |= S_DEAD;
	}
	double_up(&dir->i_zombie, &dentry->d_inode->i_zombie);
	if (!error) {
		inode_dir_notify(dir, DN_DELETE);
		d_delete(dentry);
	}
	dput(dentry);

	return error;
}

asmlinkage long sys_rmdir(const char * pathname)
{
	int error = 0;
	char * name;
	struct dentry *dentry;
	struct nameidata nd;

	name = getname(pathname);
	if(IS_ERR(name))
		return PTR_ERR(name);

	error = path_lookup(name, LOOKUP_PARENT, &nd);
	if (error)
		goto exit;

	switch(nd.last_type) {
		case LAST_DOTDOT:
			error = -ENOTEMPTY;
			goto exit1;
		case LAST_DOT:
			error = -EINVAL;
			goto exit1;
		case LAST_ROOT:
			error = -EBUSY;
			goto exit1;
	}
	down(&nd.dentry->d_inode->i_sem);
	dentry = lookup_hash(&nd.last, nd.dentry);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		error = vfs_rmdir(nd.dentry->d_inode, dentry);
		dput(dentry);
	}
	up(&nd.dentry->d_inode->i_sem);
exit1:
	path_release(&nd);
exit:
	putname(name);
	return error;
}

int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;

	down(&dir->i_zombie);
	error = may_delete(dir, dentry, 0);
	if (!error) {
		error = -EPERM;
		if (dir->i_op && dir->i_op->unlink) {
			DQUOT_INIT(dir);
			if (d_mountpoint(dentry))
				error = -EBUSY;
			else {
				lock_kernel();
				error = dir->i_op->unlink(dir, dentry);
				unlock_kernel();
				if (!error)
					d_delete(dentry);
			}
		}
	}
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_DELETE);
	return error;
}

asmlinkage long sys_unlink(const char * pathname)
{
	int error = 0;
	char * name;
	struct dentry *dentry;
	struct nameidata nd;

	name = getname(pathname);
	if(IS_ERR(name))
		return PTR_ERR(name);

	error = path_lookup(name, LOOKUP_PARENT, &nd);
	if (error)
		goto exit;
	error = -EISDIR;
	if (nd.last_type != LAST_NORM)
		goto exit1;
	down(&nd.dentry->d_inode->i_sem);
	dentry = lookup_hash(&nd.last, nd.dentry);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		/* Why not before? Because we want correct error value */
		if (nd.last.name[nd.last.len])
			goto slashes;
		error = vfs_unlink(nd.dentry->d_inode, dentry);
	exit2:
		dput(dentry);
	}
	up(&nd.dentry->d_inode->i_sem);
exit1:
	path_release(&nd);
exit:
	putname(name);

	return error;

slashes:
	error = !dentry->d_inode ? -ENOENT :
		S_ISDIR(dentry->d_inode->i_mode) ? -EISDIR : -ENOTDIR;
	goto exit2;
}

int vfs_symlink(struct inode *dir, struct dentry *dentry, const char *oldname)
{
	int error;

	down(&dir->i_zombie);
	error = may_create(dir, dentry);
	if (error)
		goto exit_lock;

	error = -EPERM;
	if (!dir->i_op || !dir->i_op->symlink)
		goto exit_lock;

	DQUOT_INIT(dir);
	lock_kernel();
	error = dir->i_op->symlink(dir, dentry, oldname);
	unlock_kernel();

exit_lock:
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
	return error;
}

asmlinkage long sys_symlink(const char * oldname, const char * newname)
{
	int error = 0;
	char * from;
	char * to;

	from = getname(oldname);
	if(IS_ERR(from))
		return PTR_ERR(from);
	to = getname(newname);
	error = PTR_ERR(to);
	if (!IS_ERR(to)) {
		struct dentry *dentry;
		struct nameidata nd;

		error = path_lookup(to, LOOKUP_PARENT, &nd);
		if (error)
			goto out;
		dentry = lookup_create(&nd, 0);
		error = PTR_ERR(dentry);
		if (!IS_ERR(dentry)) {
			error = vfs_symlink(nd.dentry->d_inode, dentry, from);
			dput(dentry);
		}
		up(&nd.dentry->d_inode->i_sem);
		path_release(&nd);
out:
		putname(to);
	}
	putname(from);
	return error;
}

int vfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	struct inode *inode;
	int error;

	down(&dir->i_zombie);
	error = -ENOENT;
	inode = old_dentry->d_inode;
	if (!inode)
		goto exit_lock;

	error = may_create(dir, new_dentry);
	if (error)
		goto exit_lock;

	error = -EXDEV;
	if (dir->i_dev != inode->i_dev)
		goto exit_lock;

	/*
	 * A link to an append-only or immutable file cannot be created.
	 */
	error = -EPERM;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		goto exit_lock;
	if (!dir->i_op || !dir->i_op->link)
		goto exit_lock;

	DQUOT_INIT(dir);
	lock_kernel();
	error = dir->i_op->link(old_dentry, dir, new_dentry);
	unlock_kernel();

exit_lock:
	up(&dir->i_zombie);
	if (!error)
		inode_dir_notify(dir, DN_CREATE);
	return error;
}

/*
 * Hardlinks are often used in delicate situations.  We avoid
 * security-related surprises by not following symlinks on the
 * newname.  --KAB
 *
 * We don't follow them on the oldname either to be compatible
 * with linux 2.0, and to avoid hard-linking to directories
 * and other special files.  --ADM
 */
asmlinkage long sys_link(const char * oldname, const char * newname)
{
	int error;
	char * to;

	to = getname(newname);
	error = PTR_ERR(to);
	if (!IS_ERR(to)) {
		struct dentry *new_dentry;
		struct nameidata nd, old_nd;

		error = __user_walk(oldname, LOOKUP_POSITIVE, &old_nd);
		if (error)
			goto exit;
		error = path_lookup(to, LOOKUP_PARENT, &nd);
		if (error)
			goto out;
		error = -EXDEV;
		if (old_nd.mnt != nd.mnt)
			goto out_release;
		new_dentry = lookup_create(&nd, 0);
		error = PTR_ERR(new_dentry);
		if (!IS_ERR(new_dentry)) {
			error = vfs_link(old_nd.dentry, nd.dentry->d_inode, new_dentry);
			dput(new_dentry);
		}
		up(&nd.dentry->d_inode->i_sem);
out_release:
		path_release(&nd);
out:
		path_release(&old_nd);
exit:
		putname(to);
	}
	return error;
}

/*
 * The worst of all namespace operations - renaming directory. "Perverted"
 * doesn't even start to describe it. Somebody in UCB had a heck of a trip...
 * Problems:
 *	a) we can get into loop creation. Check is done in is_subdir().
 *	b) race potential - two innocent renames can create a loop together.
 *	   That's where 4.4 screws up. Current fix: serialization on
 *	   sb->s_vfs_rename_sem. We might be more accurate, but that's another
 *	   story.
 *	c) we have to lock _three_ objects - parents and victim (if it exists).
 *	   And that - after we got ->i_sem on parents (until then we don't know
 *	   whether the target exists at all, let alone whether it is a directory
 *	   or not). Solution: ->i_zombie. Taken only after ->i_sem. Always taken
 *	   on link creation/removal of any kind. And taken (without ->i_sem) on
 *	   directory that will be removed (both in rmdir() and here).
 *	d) some filesystems don't support opened-but-unlinked directories,
 *	   either because of layout or because they are not ready to deal with
 *	   all cases correctly. The latter will be fixed (taking this sort of
 *	   stuff into VFS), but the former is not going away. Solution: the same
 *	   trick as in rmdir().
 *	e) conversion from fhandle to dentry may come in the wrong moment - when
 *	   we are removing the target. Solution: we will have to grab ->i_zombie
 *	   in the fhandle_to_dentry code. [FIXME - current nfsfh.c relies on
 *	   ->i_sem on parents, which works but leads to some truely excessive
 *	   locking].
 */
int vfs_rename_dir(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	int error;
	struct inode *target;

	if (old_dentry->d_inode == new_dentry->d_inode)
		return 0;

	error = may_delete(old_dir, old_dentry, 1);
	if (error)
		return error;

	if (new_dir->i_dev != old_dir->i_dev)
		return -EXDEV;

	if (!new_dentry->d_inode)
		error = may_create(new_dir, new_dentry);
	else
		error = may_delete(new_dir, new_dentry, 1);
	if (error)
		return error;

	if (!old_dir->i_op || !old_dir->i_op->rename)
		return -EPERM;

	/*
	 * If we are going to change the parent - check write permissions,
	 * we'll need to flip '..'.
	 */
	if (new_dir != old_dir) {
		error = permission(old_dentry->d_inode, MAY_WRITE);
	}
	if (error)
		return error;

	DQUOT_INIT(old_dir);
	DQUOT_INIT(new_dir);
	down(&old_dir->i_sb->s_vfs_rename_sem);
	error = -EINVAL;
	if (is_subdir(new_dentry, old_dentry))
		goto out_unlock;
	/* Don't eat your daddy, dear... */
	/* This also avoids locking issues */
	if (old_dentry->d_parent == new_dentry)
		goto out_unlock;
	target = new_dentry->d_inode;
	if (target) { /* Hastur! Hastur! Hastur! */
		triple_down(&old_dir->i_zombie,
			    &new_dir->i_zombie,
			    &target->i_zombie);
		d_unhash(new_dentry);
	} else
		double_down(&old_dir->i_zombie,
			    &new_dir->i_zombie);
	if (d_mountpoint(old_dentry)||d_mountpoint(new_dentry))
		error = -EBUSY;
	else 
		error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	if (target) {
		if (!error)
			target->i_flags |= S_DEAD;
		triple_up(&old_dir->i_zombie,
			  &new_dir->i_zombie,
			  &target->i_zombie);
		if (d_unhashed(new_dentry))
			d_rehash(new_dentry);
		dput(new_dentry);
	} else
		double_up(&old_dir->i_zombie,
			  &new_dir->i_zombie);
		
	if (!error)
		d_move(old_dentry,new_dentry);
out_unlock:
	up(&old_dir->i_sb->s_vfs_rename_sem);
	return error;
}

int vfs_rename_other(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	int error;

	if (old_dentry->d_inode == new_dentry->d_inode)
		return 0;

	error = may_delete(old_dir, old_dentry, 0);
	if (error)
		return error;

	if (new_dir->i_dev != old_dir->i_dev)
		return -EXDEV;

	if (!new_dentry->d_inode)
		error = may_create(new_dir, new_dentry);
	else
		error = may_delete(new_dir, new_dentry, 0);
	if (error)
		return error;

	if (!old_dir->i_op || !old_dir->i_op->rename)
		return -EPERM;

	DQUOT_INIT(old_dir);
	DQUOT_INIT(new_dir);
	double_down(&old_dir->i_zombie, &new_dir->i_zombie);
	if (d_mountpoint(old_dentry)||d_mountpoint(new_dentry))
		error = -EBUSY;
	else
		error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	double_up(&old_dir->i_zombie, &new_dir->i_zombie);
	if (error)
		return error;
	/* The following d_move() should become unconditional */
	if (!(old_dir->i_sb->s_type->fs_flags & FS_ODD_RENAME)) {
		d_move(old_dentry, new_dentry);
	}
	return 0;
}

int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	int error;
	if (S_ISDIR(old_dentry->d_inode->i_mode))
		error = vfs_rename_dir(old_dir,old_dentry,new_dir,new_dentry);
	else
		error = vfs_rename_other(old_dir,old_dentry,new_dir,new_dentry);
	if (!error) {
		if (old_dir == new_dir)
			inode_dir_notify(old_dir, DN_RENAME);
		else {
			inode_dir_notify(old_dir, DN_DELETE);
			inode_dir_notify(new_dir, DN_CREATE);
		}
	}
	return error;
}

static inline int do_rename(const char * oldname, const char * newname)
{
	int error = 0;
	struct dentry * old_dir, * new_dir;
	struct dentry * old_dentry, *new_dentry;
	struct nameidata oldnd, newnd;

	error = path_lookup(oldname, LOOKUP_PARENT, &oldnd);
	if (error)
		goto exit;

	error = path_lookup(newname, LOOKUP_PARENT, &newnd);
	if (error)
		goto exit1;

	error = -EXDEV;
	if (oldnd.mnt != newnd.mnt)
		goto exit2;

	old_dir = oldnd.dentry;
	error = -EBUSY;
	if (oldnd.last_type != LAST_NORM)
		goto exit2;

	new_dir = newnd.dentry;
	if (newnd.last_type != LAST_NORM)
		goto exit2;

	double_lock(new_dir, old_dir);

	old_dentry = lookup_hash(&oldnd.last, old_dir);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	/* source must exist */
	error = -ENOENT;
	if (!old_dentry->d_inode)
		goto exit4;
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!S_ISDIR(old_dentry->d_inode->i_mode)) {
		error = -ENOTDIR;
		if (oldnd.last.name[oldnd.last.len])
			goto exit4;
		if (newnd.last.name[newnd.last.len])
			goto exit4;
	}
	new_dentry = lookup_hash(&newnd.last, new_dir);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;

	lock_kernel();
	error = vfs_rename(old_dir->d_inode, old_dentry,
				   new_dir->d_inode, new_dentry);
	unlock_kernel();

	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	double_up(&new_dir->d_inode->i_sem, &old_dir->d_inode->i_sem);
exit2:
	path_release(&newnd);
exit1:
	path_release(&oldnd);
exit:
	return error;
}

asmlinkage long sys_rename(const char * oldname, const char * newname)
{
	int error;
	char * from;
	char * to;

	from = getname(oldname);
	if(IS_ERR(from))
		return PTR_ERR(from);
	to = getname(newname);
	error = PTR_ERR(to);
	if (!IS_ERR(to)) {
		error = do_rename(from,to);
		putname(to);
	}
	putname(from);
	return error;
}

int vfs_readlink(struct dentry *dentry, char *buffer, int buflen, const char *link)
{
	int len;

	len = PTR_ERR(link);
	if (IS_ERR(link))
		goto out;

	len = strlen(link);
	if (len > (unsigned) buflen)
		len = buflen;
	if (copy_to_user(buffer, link, len))
		len = -EFAULT;
out:
	return len;
}

static inline int
__vfs_follow_link(struct nameidata *nd, const char *link)
{
	int res = 0;
	char *name;
	if (IS_ERR(link))
		goto fail;

	if (*link == '/') {
		path_release(nd);
		if (!walk_init_root(link, nd))
			/* weird __emul_prefix() stuff did it */
			goto out;
	}
	res = link_path_walk(link, nd);
out:
	if (current->link_count || res || nd->last_type!=LAST_NORM)
		return res;
	/*
	 * If it is an iterative symlinks resolution in open_namei() we
	 * have to copy the last component. And all that crap because of
	 * bloody create() on broken symlinks. Furrfu...
	 */
	name = __getname();
	if (!name) {
		path_release(nd);
		return -ENOMEM;
	}
	strcpy(name, nd->last.name);
	nd->last.name = name;
	return 0;
fail:
	path_release(nd);
	return PTR_ERR(link);
}

int vfs_follow_link(struct nameidata *nd, const char *link)
{
	return __vfs_follow_link(nd, link);
}

/* get the link contents into pagecache */
static char *page_getlink(struct dentry * dentry, struct page **ppage)
{
	struct page * page;
	struct address_space *mapping = dentry->d_inode->i_mapping;
	page = read_cache_page(mapping, 0, (filler_t *)mapping->a_ops->readpage,
				NULL);
	if (IS_ERR(page))
		goto sync_fail;
	wait_on_page(page);
	if (!Page_Uptodate(page))
		goto async_fail;
	*ppage = page;
	return kmap(page);

async_fail:
	page_cache_release(page);
	return ERR_PTR(-EIO);

sync_fail:
	return (char*)page;
}

int page_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	struct page *page = NULL;
	char *s = page_getlink(dentry, &page);
	int res = vfs_readlink(dentry,buffer,buflen,s);
	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
	return res;
}

int page_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct page *page = NULL;
	char *s = page_getlink(dentry, &page);
	int res = __vfs_follow_link(nd, s);
	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
	return res;
}

struct inode_operations page_symlink_inode_operations = {
	readlink:	page_readlink,
	follow_link:	page_follow_link,
};
