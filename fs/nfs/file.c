/*
 *  linux/fs/nfs/file.c
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  Changes Copyright (C) 1994 by Florian La Roche
 *   - Do not copy data too often around in the kernel.
 *   - In nfs_file_read the return value of kmalloc wasn't checked.
 *   - Put in a better version of read look-ahead buffering. Original idea
 *     and implementation by Wai S Kok elekokws@ee.nus.sg.
 *
 *  Expire cache on write to a file by Wai S Kok (Oct 1994).
 *
 *  Total rewrite of read side for new NFS buffer cache.. Linus.
 *
 *  nfs regular file handling functions
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/lockd/bind.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#define NFSDBG_FACILITY		NFSDBG_FILE

static int  nfs_file_mmap(struct file *, struct vm_area_struct *);
static ssize_t nfs_file_read(struct file *, char *, size_t, loff_t *);
static ssize_t nfs_file_write(struct file *, const char *, size_t, loff_t *);
static int  nfs_file_flush(struct file *);
static int  nfs_fsync(struct file *, struct dentry *dentry, int datasync);

struct file_operations nfs_file_operations = {
	llseek:		generic_file_llseek,
	read:		nfs_file_read,
	write:		nfs_file_write,
	mmap:		nfs_file_mmap,
	open:		nfs_open,
	flush:		nfs_file_flush,
	release:	nfs_release,
	fsync:		nfs_fsync,
	lock:		nfs_lock,
};

struct inode_operations nfs_file_inode_operations = {
	permission:	nfs_permission,
	revalidate:	nfs_revalidate,
	setattr:	nfs_notify_change,
};

/* Hack for future NFS swap support */
#ifndef IS_SWAPFILE
# define IS_SWAPFILE(inode)	(0)
#endif

/*
 * Flush all dirty pages, and check for write errors.
 *
 */
static int
nfs_file_flush(struct file *file)
{
	struct inode	*inode = file->f_dentry->d_inode;
	int		status;

	dfprintk(VFS, "nfs: flush(%x/%ld)\n", inode->i_dev, inode->i_ino);

	/* Make sure all async reads have been sent off. We don't bother
	 * waiting on them though... */
	if (file->f_mode & FMODE_READ)
		nfs_pagein_inode(inode, 0, 0);

	status = nfs_wb_all(inode);
	if (!status) {
		status = file->f_error;
		file->f_error = 0;
	}
	return status;
}

static ssize_t
nfs_file_read(struct file * file, char * buf, size_t count, loff_t *ppos)
{
	struct dentry * dentry = file->f_dentry;
	struct inode * inode = dentry->d_inode;
	ssize_t result;

	dfprintk(VFS, "nfs: read(%s/%s, %lu@%lu)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name,
		(unsigned long) count, (unsigned long) *ppos);

	result = nfs_revalidate_inode(NFS_SERVER(inode), inode);
	if (!result)
		result = generic_file_read(file, buf, count, ppos);
	return result;
}

static int
nfs_file_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	int	status;

	dfprintk(VFS, "nfs: mmap(%s/%s)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);

	status = nfs_revalidate_inode(NFS_SERVER(inode), inode);
	if (!status)
		status = generic_file_mmap(file, vma);
	return status;
}

/*
 * Flush any dirty pages for this process, and check for write errors.
 * The return status from this call provides a reliable indication of
 * whether any write errors occurred for this process.
 */
static int
nfs_fsync(struct file *file, struct dentry *dentry, int datasync)
{
	struct inode *inode = dentry->d_inode;
	int status;

	dfprintk(VFS, "nfs: fsync(%x/%ld)\n", inode->i_dev, inode->i_ino);

	lock_kernel();
	status = nfs_wb_all(inode);
	if (!status) {
		status = file->f_error;
		file->f_error = 0;
	}
	unlock_kernel();
	return status;
}

/*
 * This does the "real" work of the write. The generic routine has
 * allocated the page, locked it, done all the page alignment stuff
 * calculations etc. Now we should just copy the data from user
 * space and write it back to the real medium..
 *
 * If the writer ends up delaying the write, the writer needs to
 * increment the page use counts until he is done with the page.
 */
static int nfs_prepare_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	return nfs_flush_incompatible(file, page);
}

static int nfs_commit_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	long status;

	lock_kernel();
	status = nfs_updatepage(file, page, offset, to-offset);
	unlock_kernel();
	return status;
}

/*
 * The following is used by wait_on_page(), generic_file_readahead()
 * to initiate the completion of any page readahead operations.
 */
static int nfs_sync_page(struct page *page)
{
	struct address_space *mapping;
	struct inode	*inode;
	unsigned long	index = page_index(page);
	unsigned int	rpages;
	int		result;

	mapping = page->mapping;
	if (!mapping)
		return 0;
	inode = mapping->host;
	if (!inode)
		return 0;

	NFS_SetPageSync(page);
	rpages = NFS_SERVER(inode)->rpages;
	result = nfs_pagein_inode(inode, index, rpages);
	if (result < 0)
		return result;
	return 0;
}

struct address_space_operations nfs_file_aops = {
	readpage: nfs_readpage,
	sync_page: nfs_sync_page,
	writepage: nfs_writepage,
#ifdef CONFIG_NFS_DIRECTIO
	direct_fileIO: nfs_direct_IO,
#endif
	prepare_write: nfs_prepare_write,
	commit_write: nfs_commit_write
};

/* 
 * Write to a file (through the page cache).
 */
static ssize_t
nfs_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct dentry * dentry = file->f_dentry;
	struct inode * inode = dentry->d_inode;
	ssize_t result;

	dfprintk(VFS, "nfs: write(%s/%s(%ld), %lu@%lu)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name,
		inode->i_ino, (unsigned long) count, (unsigned long) *ppos);

	result = -EBUSY;
	if (IS_SWAPFILE(inode))
		goto out_swapfile;
	result = nfs_revalidate_inode(NFS_SERVER(inode), inode);
	if (result)
		goto out;

	result = count;
	if (!count)
		goto out;

	result = generic_file_write(file, buf, count, ppos);
out:
	return result;

out_swapfile:
	printk(KERN_INFO "NFS: attempt to write to active swap file!\n");
	goto out;
}

static int
do_getlk(struct inode *inode, int cmd, struct file_lock *fl)
{
	int status;

	lock_kernel();
	status = nlmclnt_proc(inode, cmd, fl);
	unlock_kernel();
	return status;
}

static int
do_unlk(struct inode *inode, int cmd, struct file_lock *fl)
{
	sigset_t oldset;
	int status;

	rpc_clnt_sigmask(NFS_CLIENT(inode), &oldset);
	/*
	 * Flush all pending writes before doing anything
	 * with locks..
	 */
	filemap_fdatasync(inode->i_mapping);
	down(&inode->i_sem);
	nfs_wb_all(inode);
	up(&inode->i_sem);
	filemap_fdatawait(inode->i_mapping);

	/* NOTE: special case
	 *	If we're signalled while cleaning up locks on process exit, we
	 *	still need to complete the unlock.
	 */
	lock_kernel();
	status = nlmclnt_proc(inode, cmd, fl);
	unlock_kernel();
	rpc_clnt_sigunmask(NFS_CLIENT(inode), &oldset);
	return status;
}

static int
do_setlk(struct file *filp, int cmd, struct file_lock *fl)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int status;

	/*
	 * Flush all pending writes before doing anything
	 * with locks..
	 */
	status = filemap_fdatasync(inode->i_mapping);
	if (status == 0) {
		down(&inode->i_sem);
		status = nfs_wb_all(inode);
		up(&inode->i_sem);
		if (status == 0)
			status = filemap_fdatawait(inode->i_mapping);
	}
	if (status < 0)
		return status;

	lock_kernel();
	status = nlmclnt_proc(inode, cmd, fl);
	/* If we were signalled we still need to ensure that
	 * we clean up any state on the server. We therefore
	 * record the lock call as having succeeded in order to
	 * ensure that locks_remove_posix() cleans it out when
	 * the process exits.
	 */
	if (status == -EINTR || status == -ERESTARTSYS)
		posix_lock_file(filp, fl, 0);
	unlock_kernel();
	if (status < 0)
		return status;

	/*
	 * Make sure we clear the cache whenever we try to get the lock.
	 * This makes locking act as a cache coherency point.
	 */
	filemap_fdatasync(inode->i_mapping);
	down(&inode->i_sem);
	nfs_wb_all(inode);      /* we may have slept */
	up(&inode->i_sem);
	filemap_fdatawait(inode->i_mapping);
	nfs_zap_caches(inode);
	return 0;
}

/*
 * Lock a (portion of) a file
 */
int
nfs_lock(struct file *filp, int cmd, struct file_lock *fl)
{
	struct inode * inode = filp->f_dentry->d_inode;

	dprintk("NFS: nfs_lock(f=%4x/%ld, t=%x, fl=%x, r=%Ld:%Ld)\n",
			inode->i_dev, inode->i_ino,
			fl->fl_type, fl->fl_flags,
			(long long)fl->fl_start, (long long)fl->fl_end);

	if (!inode)
		return -EINVAL;

	/* No mandatory locks over NFS */
	if ((inode->i_mode & (S_ISGID | S_IXGRP)) == S_ISGID)
		return -ENOLCK;

	/* Fake OK code if mounted without NLM support */
	if (NFS_SERVER(inode)->flags & NFS_MOUNT_NONLM) {
		if (IS_GETLK(cmd))
			return LOCK_USE_CLNT;
		return 0;
	}

	/*
	 * No BSD flocks over NFS allowed.
	 * Note: we could try to fake a POSIX lock request here by
	 * using ((u32) filp | 0x80000000) or some such as the pid.
	 * Not sure whether that would be unique, though, or whether
	 * that would break in other places.
	 */
	if (!fl->fl_owner || (fl->fl_flags & (FL_POSIX|FL_BROKEN)) != FL_POSIX)
		return -ENOLCK;

	if (IS_GETLK(cmd))
		return do_getlk(inode, cmd, fl);
	if (fl->fl_type == F_UNLCK)
		return do_unlk(inode, cmd, fl);
	return do_setlk(filp, cmd, fl);
}
