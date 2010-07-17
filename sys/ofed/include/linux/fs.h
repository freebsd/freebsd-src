/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_FS_H_
#define	_LINUX_FS_H_

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

struct module;
struct kiocb;
struct iovec;
struct dentry;
struct page;
struct file_lock;
struct pipe_inode_info;
struct vm_area_struct;
struct poll_table_struct;
struct files_struct;

#define	inode	vnode
#define	i_cdev	v_rdev

#define	S_IRUGO	(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWUGO	(S_IWUSR | S_IWGRP | S_IWOTH)


typedef struct files_struct *fl_owner_t;

struct dentry {
	struct inode	*d_inode;
};

struct file_operations;

struct linux_file {
	struct file	*_file;
	struct file_operations	*f_op;
	void 		*private_data;
	int		f_flags;
	struct dentry	*f_dentry;
	struct dentry	f_dentry_store;
};

#define	file	linux_file

typedef int (*filldir_t)(void *, const char *, int, loff_t, u64, unsigned);

struct file_operations {
	struct module *owner;
	loff_t (*llseek)(struct file *, loff_t, int);
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*aio_read)(struct kiocb *, const struct iovec *,
	    unsigned long, loff_t);
	ssize_t (*aio_write)(struct kiocb *, const struct iovec *,
	    unsigned long, loff_t);
	int (*readdir)(struct file *, void *, filldir_t);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*ioctl)(struct inode *, struct file *, unsigned int,
	    unsigned long);
	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
	int (*mmap)(struct file *, struct vm_area_struct *);
	int (*open)(struct inode *, struct file *);
	int (*flush)(struct file *, fl_owner_t id);
	int (*release)(struct inode *, struct file *);
	int (*fsync)(struct file *, struct dentry *, int datasync);
	int (*aio_fsync)(struct kiocb *, int datasync);
	int (*fasync)(int, struct file *, int);
	int (*lock)(struct file *, int, struct file_lock *);
	ssize_t (*sendpage)(struct file *, struct page *, int, size_t,
	    loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long,
	    unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock)(struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
	    loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *,
	    struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **);
};

static inline int
linux_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	return 0;
}

static inline int
linux_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	return 0;
}

static inline int
linux_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	return 0;
}

static inline int
linux_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	return 0;
}

static inline int
linux_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	return 0;
}

static inline int
linux_poll(struct cdev *dev, int events, struct thread *td)
{
	return 0;
}

static inline int
linux_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	return 0;
}

static inline int
register_chrdev_region(dev_t dev, unsigned range, const char *name)
{
	return 0;
}

static inline void
unregister_chrdev_region(dev_t dev, unsigned range)
{
	return;
}

static inline dev_t
iminor(struct inode *inode)
{
	return dev2udev(inode->v_rdev);
}

static inline struct inode *
igrab(struct inode *inode)
{
	int error;

	error = vget(inode, 0, curthread);
	if (error)
		return (NULL);

	return (inode);
}

static inline void
iput(struct inode *inode)
{

	vrele(inode);
}

#endif	/* _LINUX_FS_H_ */
