
/*
 * Copyright (c) 1997 John S. Dyson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. John S. Dyson's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * DISCLAIMER:  This code isn't warranted to do anything useful.  Anything
 * bad that happens because of using this software isn't the responsibility
 * of the author.  This software is distributed AS-IS.
 *
 * $Id$
 */

/*
 * This file contains support for the POSIX.4 AIO facility.
 *
 * The initial version provides only the (bogus) synchronous semantics
 * but will support async in the future.  Note that a bit
 * in a private field allows the user mode subroutine to adapt
 * the kernel operations to true POSIX.4 for future compatibility.
 *
 * This code is used to support true POSIX.4 AIO/LIO with the help
 * of a user mode subroutine package.  Note that eventually more support
 * will be pushed into the kernel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <sys/sysctl.h>
#include <sys/aio.h>


/*
 * aio_cancel at the kernel level is a NOOP right now.  It
 * might be possible to support it partially in user mode, or
 * in kernel mode later on.
 */
int
aio_cancel(struct proc *p, struct aio_cancel_args *uap, int *retval) {
	return AIO_NOTCANCELLED;
}


/*
 * aio_error is implemented in the kernel level for compatibility
 * purposes only.  For a user mode async implementation, it would be
 * best to do it in a userland subroutine.
 */
int
aio_error(struct proc *p, struct aio_error_args *uap, int *retval) {
	int activeflag, errorcode;
	struct aiocb iocb;
	int error;

	/*
	 * Get control block
	 */
	if (error = copyin((caddr_t) uap->aiocbp, (caddr_t) &iocb, sizeof iocb))
		return error;
	if (iocb._aiocb_private.active == -1)
		return EFAULT;

	if (iocb._aiocb_private.active != AIO_PMODE_ACTIVE) {
		retval[0] = EINVAL;
		return(0);
	}

	retval[0] = iocb._aiocb_private.error;
	return(0);
}

int
aio_read(struct proc *p, struct aio_read_args *uap, int *retval) {
	struct filedesc *fdp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	struct aiocb iocb;
	int error;


	/*
	 * Get control block
	 */
	if (error = copyin((caddr_t) uap->aiocbp, (caddr_t) &iocb, sizeof iocb))
		return error;

	/*
	 * We support sync only for now.
	 */
	if ((iocb._aiocb_private.privatemodes & AIO_PMODE_SYNC) == 0)
		return ENOSYS;

	/*
	 * Get the fd info for process
	 */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor
	 */
	fd = iocb.aio_fildes;
	if (fd >= fdp->fd_nfiles)
		return EBADF;
	fp = fdp->fd_ofiles[fd];
	if ((fp == NULL) || ((fp->f_flag & FREAD) == 0))
		return EBADF;
	if (((int) iocb.aio_offset) == -1)
		return EINVAL;

	aiov.iov_base = iocb.aio_buf;
	aiov.iov_len = iocb.aio_nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = iocb.aio_offset;

	auio.uio_resid = iocb.aio_nbytes;
	if (auio.uio_resid < 0)
		return (EINVAL);

	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	cnt = iocb.aio_nbytes;
	error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	if (error &&
		(auio.uio_resid != cnt) &&
		(error == ERESTART || error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
	*retval = cnt;
	return error;
}


/*
 * Return and suspend aren't supported (yet).
 */
int
aio_return(struct proc *p, struct aio_return_args *uap, int *retval) {
	return (0);
}

int
aio_suspend(struct proc *p, struct aio_suspend_args *uap, int *retval) {
	return (0);
}

int
aio_write(struct proc *p, struct aio_write_args *uap, int *retval) {
	struct filedesc *fdp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	struct aiocb iocb;
	int error;

	if (error = copyin((caddr_t) uap->aiocbp, (caddr_t) &iocb, sizeof iocb))
		return error;

	/*
	 * We support sync only for now.
	 */
	if ((iocb._aiocb_private.privatemodes & AIO_PMODE_SYNC) == 0)
		return ENOSYS;

	/*
	 * Get the fd info for process
	 */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor
	 */
	fd = iocb.aio_fildes;
	if (fd >= fdp->fd_nfiles)
		return EBADF;
	fp = fdp->fd_ofiles[fd];
	if ((fp == NULL) || ((fp->f_flag & FWRITE) == 0))
		return EBADF;
	if (((int) iocb.aio_offset) == -1)
		return EINVAL;

	aiov.iov_base = iocb.aio_buf;
	aiov.iov_len = iocb.aio_nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = iocb.aio_offset;

	auio.uio_resid = iocb.aio_nbytes;
	if (auio.uio_resid < 0)
		return (EINVAL);

	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	cnt = iocb.aio_nbytes;
	error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred);
	if (error) {
		if (auio.uio_resid != cnt) {
			if (error == ERESTART || error == EINTR || error == EWOULDBLOCK)
				error = 0;
			if (error == EPIPE)
				psignal(p, SIGPIPE);
		}
	}
	cnt -= auio.uio_resid;
	*retval = cnt;
	return error;
}

int
lio_listio(struct proc *p, struct lio_listio_args *uap, int *retval) {
	struct filedesc *fdp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	unsigned int iocblen, iocbcnt;
	struct aiocb *iocb;
	int error;
	int i;

	if (uap->mode == LIO_NOWAIT)
		return ENOSYS;
	iocbcnt = uap->nent;
	if (iocbcnt > AIO_LISTIO_MAX)
		return EINVAL;
	return ENOSYS;
}
