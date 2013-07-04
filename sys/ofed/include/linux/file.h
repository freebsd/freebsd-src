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
#ifndef	_LINUX_FILE_H_
#define	_LINUX_FILE_H_

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/refcount.h>
#include <sys/proc.h>

#include <linux/fs.h>

struct linux_file;

#undef file

extern struct fileops linuxfileops;

static inline struct linux_file *
linux_fget(unsigned int fd)
{
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd, 0, 0, &file, NULL) != 0)
		return (NULL);
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
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd, 0, 0, &file, NULL) != 0)
		return;
	fdclose(curthread->td_proc->p_fd, file, fd, curthread);
}

static inline void
fd_install(unsigned int fd, struct linux_file *filp)
{
	struct file *file;

	if (fget_unlocked(curthread->td_proc->p_fd, fd, 0, 0, &file, NULL) != 0)
		file = NULL;
	filp->_file = file;
        finit(file, filp->f_mode, DTYPE_DEV, filp, &linuxfileops);
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
	return fd;
}

static inline struct linux_file *
_alloc_file(int mode, const struct file_operations *fops)
{
	struct linux_file *filp;

	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	if (filp == NULL) 
		return (NULL);
	filp->f_op = fops;
	filp->f_mode = mode;

	return filp;
}

#define	alloc_file(mnt, root, mode, fops)	_alloc_file((mode), (fops))

#define	file	linux_file
#define	fget	linux_fget

#endif	/* _LINUX_FILE_H_ */
