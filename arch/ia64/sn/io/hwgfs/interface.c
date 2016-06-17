/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/sched.h>		/* needed for smp_lock.h :( */
#include <linux/smp_lock.h>

#include <linux/slab.h>

#include <asm/sn/hwgfs.h>

extern struct vfsmount *hwgfs_vfsmount;

/* TODO: Move this to some .h file or, more likely, use a slightly
   different interface from lookup_create. */
extern struct dentry *lookup_create(struct nameidata *nd, int is_dir);

static int
walk_parents_mkdir(
	const char		**path,
	struct nameidata	*nd,
	int			is_dir)
{
	char			*slash;
	char			buf[strlen(*path)+1];
	int			error;

	while ((slash = strchr(*path, '/')) != NULL) {
		int len = slash - *path;
		memcpy(buf, *path, len);
		buf[len] = '\0';

		error = path_walk(buf, nd); 
		if (unlikely(error))
			return error;

		nd->dentry = lookup_create(nd, is_dir);
		if (unlikely(IS_ERR(nd->dentry)))
			return PTR_ERR(nd->dentry);

		if (!nd->dentry->d_inode)
			error = vfs_mkdir(nd->dentry->d_parent->d_inode,
					nd->dentry, 0755);
		
		up(&nd->dentry->d_parent->d_inode->i_sem);
		if (unlikely(error))
			return error;

		*path += len + 1;
	}

	return 0;
}

/* On success, returns with parent_inode->i_sem taken. */
static int
hwgfs_decode(
	hwgfs_handle_t		dir,
	const char		*name,
	int			is_dir,
	struct inode		**parent_inode,
	struct dentry		**dentry)
{
	struct nameidata	nd;
	int			error;

	if (!dir)
		dir = hwgfs_vfsmount->mnt_sb->s_root;

	memset(&nd, 0, sizeof(nd));
	nd.flags = LOOKUP_PARENT;
	nd.mnt = mntget(hwgfs_vfsmount);
	nd.dentry = dget(dir);

	error = walk_parents_mkdir(&name, &nd, is_dir);
	if (unlikely(error))
		return error;

	error = path_walk(name, &nd);
	if (unlikely(error))
		return error;

	*dentry = lookup_create(&nd, is_dir);

	if (unlikely(IS_ERR(*dentry)))
		return PTR_ERR(*dentry);
	*parent_inode = (*dentry)->d_parent->d_inode;
	return 0;
}

static int
path_len(
	struct dentry		*de,
	struct dentry		*root)
{
	int			len = 0;

	while (de != root) {
		len += de->d_name.len + 1;	/* count the '/' */
		de = de->d_parent;
	}
	return len;		/* -1 because we omit the leading '/',
				   +1 because we include trailing '\0' */
}

int
hwgfs_generate_path(
	hwgfs_handle_t		de,
	char			*path,
	int			buflen)
{
	struct dentry		*hwgfs_root;
	int			len;
	char			*path_orig = path;

	if (unlikely(de == NULL))
		return -EINVAL;

	hwgfs_root = hwgfs_vfsmount->mnt_sb->s_root;
	if (unlikely(de == hwgfs_root))
		return -EINVAL;

	spin_lock(&dcache_lock);
	len = path_len(de, hwgfs_root);
	if (len > buflen) {
		spin_unlock(&dcache_lock);
		return -ENAMETOOLONG;
	}

	path += len - 1;
	*path = '\0';

	for (;;) {
		path -= de->d_name.len;
		memcpy(path, de->d_name.name, de->d_name.len);
		de = de->d_parent;
		if (de == hwgfs_root)
			break;
		*(--path) = '/';
	}
		
	spin_unlock(&dcache_lock);
	BUG_ON(path != path_orig);
	return 0;
}

hwgfs_handle_t
hwgfs_register(
	hwgfs_handle_t		dir,
	const char		*name,
	unsigned int		flags,
	unsigned int		major,
	unsigned int		minor,
	umode_t			mode,
	void			*ops,
	void			*info)
{
	dev_t			devnum = MKDEV(major, minor);
	struct inode		*parent_inode;
	struct dentry		*dentry;
	int			error;

	error = hwgfs_decode(dir, name, 0, &parent_inode, &dentry);
	if (likely(!error)) {
		error = vfs_mknod(parent_inode, dentry, mode, devnum);
		if (likely(!error)) {
			/*
			 * Do this inside parents i_sem to avoid racing
			 * with lookups.
			 */
			if (S_ISCHR(mode))
				dentry->d_inode->i_fop = ops;
			dentry->d_fsdata = info;
			up(&parent_inode->i_sem);
		} else {
			up(&parent_inode->i_sem);
			dput(dentry);
			dentry = NULL;
                }
	}

	return dentry;
}

int
hwgfs_mk_symlink(
	hwgfs_handle_t		dir,
	const char		*name,
	unsigned int		flags,
	const char		*link,
	hwgfs_handle_t		*handle,
	void			*info)
{
	struct inode		*parent_inode;
	struct dentry		*dentry;
	int			error;

	error = hwgfs_decode(dir, name, 0, &parent_inode, &dentry);
	if (likely(!error)) {
		error = vfs_symlink(parent_inode, dentry, link);
		dentry->d_fsdata = info;
		if (handle)
			*handle = dentry;
		up(&parent_inode->i_sem);
		/* dput(dentry); */
	}
	return error;
}

hwgfs_handle_t
hwgfs_mk_dir(
	hwgfs_handle_t		dir,
	const char		*name,
	void			*info)
{
	struct inode		*parent_inode;
	struct dentry		*dentry;
	int			error;

	error = hwgfs_decode(dir, name, 1, &parent_inode, &dentry);
	if (likely(!error)) {
		error = vfs_mkdir(parent_inode, dentry, 0755);
		up(&parent_inode->i_sem);

		if (unlikely(error)) {
			dput(dentry);
			dentry = NULL;
		} else {
			dentry->d_fsdata = info;
		}
	}
	return dentry;
}

void
hwgfs_unregister(
	hwgfs_handle_t		de)
{
	struct inode		*parent_inode = de->d_parent->d_inode;

	if (S_ISDIR(de->d_inode->i_mode))
		vfs_rmdir(parent_inode, de);
	else
		vfs_unlink(parent_inode, de);
}

/* XXX: this function is utterly bogus.  Every use of it is racy and the
        prototype is stupid.  You have been warned.  --hch.  */
hwgfs_handle_t
hwgfs_find_handle(
	hwgfs_handle_t		base,
	const char		*name,
	unsigned int		major,		/* IGNORED */
	unsigned int		minor,		/* IGNORED */
	char			type,		/* IGNORED */
	int			traverse_symlinks)
{
	struct dentry		*dentry = NULL;
	struct nameidata	nd;
	int			error;

	BUG_ON(*name=='/');

	memset(&nd, 0, sizeof(nd));

	nd.mnt = mntget(hwgfs_vfsmount);
	nd.dentry = dget(base ? base : hwgfs_vfsmount->mnt_sb->s_root);
	nd.flags = LOOKUP_POSITIVE | (traverse_symlinks ? LOOKUP_FOLLOW : 0);

	error = path_walk(name, &nd);
	if (likely(!error)) {
		dentry = nd.dentry;
		path_release(&nd);		/* stale data from here! */
	}

	return dentry;
}

hwgfs_handle_t
hwgfs_get_parent(
	hwgfs_handle_t		de)
{
	struct dentry		*parent;

	lock_kernel();		/* XXX: for LBS this must be dparent_lock */
	parent = de->d_parent;
	unlock_kernel();

	return parent;
}

int
hwgfs_set_info(
	hwgfs_handle_t		de,
	void			*info)
{
	if (unlikely(de == NULL))
		return -EINVAL;
	de->d_fsdata = info;
	return 0;
}

void *
hwgfs_get_info(
	hwgfs_handle_t		de)
{
	return de->d_fsdata;
}

EXPORT_SYMBOL(hwgfs_generate_path);
EXPORT_SYMBOL(hwgfs_register);
EXPORT_SYMBOL(hwgfs_unregister);
EXPORT_SYMBOL(hwgfs_mk_symlink);
EXPORT_SYMBOL(hwgfs_mk_dir);
EXPORT_SYMBOL(hwgfs_find_handle);
EXPORT_SYMBOL(hwgfs_get_parent);
EXPORT_SYMBOL(hwgfs_set_info);
EXPORT_SYMBOL(hwgfs_get_info);
