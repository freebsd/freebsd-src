/* $FreeBSD$ */
/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_FILE_H_
#define	_LINUX_FILE_H_

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/refcount.h>
#include <sys/capsicum.h>
#include <sys/proc.h>

#include <linux/fs.h>

struct linux_file;

#undef file

extern struct fileops linuxfileops;

static inline struct linux_file *
linux_fget(unsigned int fd)
{
	cap_rights_t rights;
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd,
	    cap_rights_init(&rights), &file, NULL) != 0) {
		return (NULL);
	}
	return (struct linux_file *)file->f_data;
}

static inline void
fput(struct linux_file *filp)
{
	if (filp->_file == NULL) {
		kfree(filp);
		return;
	}
	if (refcount_release(&filp->_file->f_count)) {
		_fdrop(filp->_file, curthread);
		kfree(filp);
	}
}

static inline void
put_unused_fd(unsigned int fd)
{
	cap_rights_t rights;
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd,
	    cap_rights_init(&rights), &file, NULL) != 0) {
		return;
	}
	/*
	 * NOTE: We should only get here when the "fd" has not been
	 * installed, so no need to free the associated Linux file
	 * structure.
	 */
	fdclose(curthread, file, fd);

	/* drop extra reference */
	fdrop(file, curthread);
}

static inline void
fd_install(unsigned int fd, struct linux_file *filp)
{
	cap_rights_t rights;
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd,
	    cap_rights_init(&rights), &file, NULL) != 0) {
		file = NULL;
	}
	filp->_file = file;
	finit(file, filp->f_mode, DTYPE_DEV, filp, &linuxfileops);

	/* drop the extra reference */
	fput(filp);
}

static inline int
get_unused_fd(void)
{
	struct file *file;
	int error;
	int fd;

	error = falloc(curthread, &file, &fd, 0);
	if (error)
		return -error;
	/* drop the extra reference */
	fdrop(file, curthread);
	return fd;
}

static inline struct linux_file *
alloc_file(int mode, const struct file_operations *fops)
{
	struct linux_file *filp;

	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	if (filp == NULL)
		return (NULL);
	filp->f_op = fops;
	filp->f_mode = mode;

	return filp;
}

struct fd {
	struct linux_file *linux_file;
};

static inline void fdput(struct fd fd)
{
	fput(fd.linux_file);
}

static inline struct fd fdget(unsigned int fd)
{
	struct linux_file *f = linux_fget(fd);
	return (struct fd){f};
}

#define	file	linux_file
#define	fget	linux_fget

#endif	/* _LINUX_FILE_H_ */
