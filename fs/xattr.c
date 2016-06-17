/*
  File: fs/xattr.c

  Extended attribute handling.

  Copyright (C) 2001 by Andreas Gruenbacher <a.gruenbacher@computer.org>
  Copyright (C) 2001 SGI - Silicon Graphics, Inc <linux-xfs@oss.sgi.com>
 */
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/smp_lock.h>
#include <linux/file.h>
#include <linux/xattr.h>
#include <asm/uaccess.h>

/*
 * Extended attribute memory allocation wrappers, originally
 * based on the Intermezzo PRESTO_ALLOC/PRESTO_FREE macros.
 * The vmalloc use here is very uncommon - extended attributes
 * are supposed to be small chunks of metadata, and it is quite
 * unusual to have very many extended attributes, so lists tend
 * to be quite short as well.  The 64K upper limit is derived
 * from the extended attribute size limit used by XFS.
 * Intentionally allow zero @size for value/list size requests.
 */
static void *
xattr_alloc(size_t size, size_t limit)
{
	void *ptr;

	if (size > limit)
		return ERR_PTR(-E2BIG);

	if (!size)	/* size request, no buffer is needed */
		return NULL;
	else if (size <= PAGE_SIZE)
		ptr = kmalloc((unsigned long) size, GFP_KERNEL);
	else
		ptr = vmalloc((unsigned long) size);
	if (!ptr)
		return ERR_PTR(-ENOMEM);
	return ptr;
}

static void
xattr_free(void *ptr, size_t size)
{
	if (!size)	/* size request, no buffer was needed */
		return;
	else if (size <= PAGE_SIZE)
		kfree(ptr);
	else
		vfree(ptr);
}

/*
 * Extended attribute SET operations
 */
static long
setxattr(struct dentry *d, char *name, void *value, size_t size, int flags)
{
	int error;
	void *kvalue;
	char kname[XATTR_NAME_MAX + 1];

	if (flags & ~(XATTR_CREATE|XATTR_REPLACE))
		return -EINVAL;

	error = strncpy_from_user(kname, name, sizeof(kname));
	if (error == 0 || error == sizeof(kname))
		error = -ERANGE;
	if (error < 0)
		return error;

	kvalue = xattr_alloc(size, XATTR_SIZE_MAX);
	if (IS_ERR(kvalue))
		return PTR_ERR(kvalue);

	if (size > 0 && copy_from_user(kvalue, value, size)) {
		xattr_free(kvalue, size);
		return -EFAULT;
	}

	error = -EOPNOTSUPP;
	if (d->d_inode->i_op && d->d_inode->i_op->setxattr) {
		down(&d->d_inode->i_sem);
		lock_kernel();
		error = d->d_inode->i_op->setxattr(d, kname, kvalue, size, flags);
		unlock_kernel();
		up(&d->d_inode->i_sem);
	}

	xattr_free(kvalue, size);
	return error;
}

asmlinkage long
sys_setxattr(char *path, char *name, void *value, size_t size, int flags)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(path, &nd);
	if (error)
		return error;
	error = setxattr(nd.dentry, name, value, size, flags);
	path_release(&nd);
	return error;
}

asmlinkage long
sys_lsetxattr(char *path, char *name, void *value, size_t size, int flags)
{
	struct nameidata nd;
	int error;

	error = user_path_walk_link(path, &nd);
	if (error)
		return error;
	error = setxattr(nd.dentry, name, value, size, flags);
	path_release(&nd);
	return error;
}

asmlinkage long
sys_fsetxattr(int fd, char *name, void *value, size_t size, int flags)
{
	struct file *f;
	int error = -EBADF;

	f = fget(fd);
	if (!f)
		return error;
	error = setxattr(f->f_dentry, name, value, size, flags);
	fput(f);
	return error;
}

/*
 * Extended attribute GET operations
 */
static ssize_t
getxattr(struct dentry *d, char *name, void *value, size_t size)
{
	ssize_t error;
	void *kvalue;
	char kname[XATTR_NAME_MAX + 1];

	error = strncpy_from_user(kname, name, sizeof(kname));
	if (error == 0 || error == sizeof(kname))
		error = -ERANGE;
	if (error < 0)
		return error;

	kvalue = xattr_alloc(size, XATTR_SIZE_MAX);
	if (IS_ERR(kvalue))
		return PTR_ERR(kvalue);

	error = -EOPNOTSUPP;
	if (d->d_inode->i_op && d->d_inode->i_op->getxattr) {
		down(&d->d_inode->i_sem);
		lock_kernel();
		error = d->d_inode->i_op->getxattr(d, kname, kvalue, size);
		unlock_kernel();
		up(&d->d_inode->i_sem);
	}

	if (kvalue && error > 0)
		if (copy_to_user(value, kvalue, error))
			error = -EFAULT;
	xattr_free(kvalue, size);
	return error;
}

asmlinkage ssize_t
sys_getxattr(char *path, char *name, void *value, size_t size)
{
	struct nameidata nd;
	ssize_t error;

	error = user_path_walk(path, &nd);
	if (error)
		return error;
	error = getxattr(nd.dentry, name, value, size);
	path_release(&nd);
	return error;
}

asmlinkage ssize_t
sys_lgetxattr(char *path, char *name, void *value, size_t size)
{
	struct nameidata nd;
	ssize_t error;

	error = user_path_walk_link(path, &nd);
	if (error)
		return error;
	error = getxattr(nd.dentry, name, value, size);
	path_release(&nd);
	return error;
}

asmlinkage ssize_t
sys_fgetxattr(int fd, char *name, void *value, size_t size)
{
	struct file *f;
	ssize_t error = -EBADF;

	f = fget(fd);
	if (!f)
		return error;
	error = getxattr(f->f_dentry, name, value, size);
	fput(f);
	return error;
}

/*
 * Extended attribute LIST operations
 */
static ssize_t
listxattr(struct dentry *d, char *list, size_t size)
{
	ssize_t error;
	char *klist;

	klist = (char *)xattr_alloc(size, XATTR_LIST_MAX);
	if (IS_ERR(klist))
		return PTR_ERR(klist);

	error = -EOPNOTSUPP;
	if (d->d_inode->i_op && d->d_inode->i_op->listxattr) {
		down(&d->d_inode->i_sem);
		lock_kernel();
		error = d->d_inode->i_op->listxattr(d, klist, size);
		unlock_kernel();
		up(&d->d_inode->i_sem);
	}

	if (klist && error > 0)
		if (copy_to_user(list, klist, error))
			error = -EFAULT;
	xattr_free(klist, size);
	return error;
}

asmlinkage ssize_t
sys_listxattr(char *path, char *list, size_t size)
{
	struct nameidata nd;
	ssize_t error;

	error = user_path_walk(path, &nd);
	if (error)
		return error;
	error = listxattr(nd.dentry, list, size);
	path_release(&nd);
	return error;
}

asmlinkage ssize_t
sys_llistxattr(char *path, char *list, size_t size)
{
	struct nameidata nd;
	ssize_t error;

	error = user_path_walk_link(path, &nd);
	if (error)
		return error;
	error = listxattr(nd.dentry, list, size);
	path_release(&nd);
	return error;
}

asmlinkage ssize_t
sys_flistxattr(int fd, char *list, size_t size)
{
	struct file *f;
	ssize_t error = -EBADF;

	f = fget(fd);
	if (!f)
		return error;
	error = listxattr(f->f_dentry, list, size);
	fput(f);
	return error;
}

/*
 * Extended attribute REMOVE operations
 */
static long
removexattr(struct dentry *d, char *name)
{
	int error;
	char kname[XATTR_NAME_MAX + 1];

	error = strncpy_from_user(kname, name, sizeof(kname));
	if (error == 0 || error == sizeof(kname))
		error = -ERANGE;
	if (error < 0)
		return error;

	error = -EOPNOTSUPP;
	if (d->d_inode->i_op && d->d_inode->i_op->removexattr) {
		down(&d->d_inode->i_sem);
		lock_kernel();
		error = d->d_inode->i_op->removexattr(d, kname);
		unlock_kernel();
		up(&d->d_inode->i_sem);
	}
	return error;
}

asmlinkage long
sys_removexattr(char *path, char *name)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(path, &nd);
	if (error)
		return error;
	error = removexattr(nd.dentry, name);
	path_release(&nd);
	return error;
}

asmlinkage long
sys_lremovexattr(char *path, char *name)
{
	struct nameidata nd;
	int error;

	error = user_path_walk_link(path, &nd);
	if (error)
		return error;
	error = removexattr(nd.dentry, name);
	path_release(&nd);
	return error;
}

asmlinkage long
sys_fremovexattr(int fd, char *name)
{
	struct file *f;
	int error = -EBADF;

	f = fget(fd);
	if (!f)
		return error;
	error = removexattr(f->f_dentry, name);
	fput(f);
	return error;
}
