/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)fifo_vnops.c	7.7 (Berkeley) 4/15/91
 *	$Id: fifo_vnops.c,v 1.4 1993/10/16 15:24:02 rgrimes Exp $
 */

#ifdef	FIFO
#include "param.h"
#include "time.h"
#include "namei.h"
#include "vnode.h"
#include "socket.h"
#include "socketvar.h"
#include "stat.h"
#include "systm.h"
#include "ioctl.h"
#include "file.h"
#include "fifo.h"
#include "errno.h"
#include "malloc.h"

/*
 * This structure is associated with the FIFO vnode and stores
 * the state associated with the FIFO.
 */
struct fifoinfo {
	struct socket	*fi_readsock;
	struct socket	*fi_writesock;
	long		fi_readers;
	long		fi_writers;
};

struct vnodeops fifo_vnodeops = {
	fifo_lookup,		/* lookup */
	fifo_create,		/* create */
	fifo_mknod,		/* mknod */
	fifo_open,		/* open */
	fifo_close,		/* close */
	fifo_access,		/* access */
	fifo_getattr,		/* getattr */
	fifo_setattr,		/* setattr */
	fifo_read,		/* read */
	fifo_write,		/* write */
	fifo_ioctl,		/* ioctl */
	fifo_select,		/* select */
	fifo_mmap,		/* mmap */
	fifo_fsync,		/* fsync */
	fifo_seek,		/* seek */
	fifo_remove,		/* remove */
	fifo_link,		/* link */
	fifo_rename,		/* rename */
	fifo_mkdir,		/* mkdir */
	fifo_rmdir,		/* rmdir */
	fifo_symlink,		/* symlink */
	fifo_readdir,		/* readdir */
	fifo_readlink,		/* readlink */
	fifo_abortop,		/* abortop */
	fifo_inactive,		/* inactive */
	fifo_reclaim,		/* reclaim */
	fifo_lock,		/* lock */
	fifo_unlock,		/* unlock */
	fifo_bmap,		/* bmap */
	fifo_strategy,		/* strategy */
	fifo_print,		/* print */
	fifo_islocked,		/* islocked */
	fifo_advlock,		/* advlock */
};

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
fifo_lookup(vp, ndp, p)
	struct vnode *vp;
	struct nameidata *ndp;
	struct proc *p;
{

	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	return (ENOTDIR);
}

/*
 * Open called to set up a new instance of a fifo or
 * to find an active instance of a fifo.
 */
/* ARGSUSED */
fifo_open(vp, mode, cred, p)
	register struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	register struct fifoinfo *fip;
	struct socket *rso, *wso;
	int error;
	static char openstr[] = "fifo";

	if ((mode & (FREAD|FWRITE)) == (FREAD|FWRITE))
		return (EINVAL);
	if ((fip = vp->v_fifoinfo) == NULL) {
		MALLOC(fip, struct fifoinfo *, sizeof(*fip), M_VNODE, M_WAITOK);
		vp->v_fifoinfo = fip;
		fip->fi_readers = fip->fi_writers = 0;
		if (error = socreate(AF_UNIX, &rso, SOCK_STREAM, 0)) {
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		fip->fi_readsock = rso;
		if (error = socreate(AF_UNIX, &wso, SOCK_STREAM, 0)) {
			(void)soclose(rso);
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		fip->fi_writesock = wso;
		if (error = unp_connect2(wso, rso)) {
			(void)soclose(wso);
			(void)soclose(rso);
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		wso->so_state |= SS_CANTRCVMORE;
		rso->so_state |= SS_CANTSENDMORE;
	}
	error = 0;
	if (mode & FREAD) {
		fip->fi_readers++;
		if (fip->fi_readers == 1) {
			fip->fi_writesock->so_state &= ~SS_CANTSENDMORE;
			if (fip->fi_writers > 0)
				wakeup((caddr_t)&fip->fi_writers);
		}
		if (mode & O_NONBLOCK)
			return (0);
		while (fip->fi_writers == 0) {
			VOP_UNLOCK(vp);
			error = tsleep((caddr_t)&fip->fi_readers, PSOCK | PCATCH,
			    		openstr, 0);
			VOP_LOCK(vp);
			if (error)
				break;
		}
	} else {
		fip->fi_writers++;
		if (fip->fi_readers == 0 && (mode & O_NONBLOCK)) {
			error = ENXIO;
		} else {
			if (fip->fi_writers == 1) {
				fip->fi_readsock->so_state &= ~SS_CANTRCVMORE;
				if (fip->fi_readers > 0)
					wakeup((caddr_t)&fip->fi_readers);
			}
			while (fip->fi_readers == 0) {
				VOP_UNLOCK(vp);
				error = tsleep((caddr_t)&fip->fi_writers,
				    		PSOCK | PCATCH, openstr, 0);
				VOP_LOCK(vp);
				if (error)
					break;
			}
		}
	}
	if (error)
		fifo_close(vp, mode, cred, p);
	return (error);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
fifo_read(vp, uio, ioflag, cred)
	struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	register struct socket *rso = vp->v_fifoinfo->fi_readsock;
	int error, startresid;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("fifo_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (ioflag & IO_NDELAY)
		rso->so_state |= SS_NBIO;
	startresid = uio->uio_resid;
	VOP_UNLOCK(vp);
	error = soreceive(rso, (struct mbuf **)0, uio, (int *)0,
		(struct mbuf **)0, (struct mbuf **)0);
	VOP_LOCK(vp);
	/*
	 * Clear EOF indication after first such return.
	 */
	if (uio->uio_resid == startresid)
		rso->so_state &= ~SS_CANTRCVMORE;
	if (ioflag & IO_NDELAY)
		rso->so_state &= ~SS_NBIO;
	return (error);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
fifo_write(vp, uio, ioflag, cred)
	struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct socket *wso = vp->v_fifoinfo->fi_writesock;
	int error;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("fifo_write mode");
#endif
	if (ioflag & IO_NDELAY)
		wso->so_state |= SS_NBIO;
	VOP_UNLOCK(vp);
	error = sosend(wso, (struct mbuf *)0, uio, 0, (struct mbuf *)0, 0);
	VOP_LOCK(vp);
	if (ioflag & IO_NDELAY)
		wso->so_state &= ~SS_NBIO;
	return (error);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
fifo_ioctl(vp, com, data, fflag, cred, p)
	struct vnode *vp;
	int com;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct file filetmp;
	int error;

	if (com == FIONBIO)
		return (0);
	if (fflag & FREAD)
		filetmp.f_data = (caddr_t)vp->v_fifoinfo->fi_readsock;
	else
		filetmp.f_data = (caddr_t)vp->v_fifoinfo->fi_writesock;
	return (soo_ioctl(&filetmp, com, data, p));
}

/* ARGSUSED */
fifo_select(vp, which, fflag, cred, p)
	struct vnode *vp;
	int which, fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct file filetmp;
	int error;

	if (fflag & FREAD)
		filetmp.f_data = (caddr_t)vp->v_fifoinfo->fi_readsock;
	else
		filetmp.f_data = (caddr_t)vp->v_fifoinfo->fi_writesock;
	return (soo_select(&filetmp, which, p));
}

/*
 * This is a noop, simply returning what one has been given.
 */
fifo_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{

	if (vpp != NULL)
		*vpp = vp;
	if (bnp != NULL)
		*bnp = bn;
	return (0);
}

/*
 * At the moment we do not do any locking.
 */
/* ARGSUSED */
fifo_lock(vp)
	struct vnode *vp;
{

	return (0);
}

/* ARGSUSED */
fifo_unlock(vp)
	struct vnode *vp;
{

	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
fifo_close(vp, fflag, cred, p)
	register struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	register struct fifoinfo *fip = vp->v_fifoinfo;
	int error1, error2;

	if (fflag & FWRITE) {
		fip->fi_writers--;
		if (fip->fi_writers == 0)
			socantrcvmore(fip->fi_readsock);
	} else {
		fip->fi_readers--;
		if (fip->fi_readers == 0)
			socantsendmore(fip->fi_writesock);
	}
	if (vp->v_usecount > 1)
		return (0);
	error1 = soclose(fip->fi_readsock);
	error2 = soclose(fip->fi_writesock);
	FREE(fip, M_VNODE);
	vp->v_fifoinfo = NULL;
	if (error1)
		return (error1);
	return (error2);
}

/*
 * Print out the contents of a fifo vnode.
 */
fifo_print(vp)
	struct vnode *vp;
{

	printf("tag VT_NON");
	fifo_printinfo(vp);
	printf("\n");
}

/*
 * Print out internal contents of a fifo vnode.
 */
fifo_printinfo(vp)
	struct vnode *vp;
{
	register struct fifoinfo *fip = vp->v_fifoinfo;

	printf(", fifo with %d readers and %d writers",
		fip->fi_readers, fip->fi_writers);
}

/*
 * Fifo failed operation
 */
fifo_ebadf()
{

	return (EBADF);
}

/*
 * Fifo advisory byte-level locks.
 */
/* ARGSUSED */
fifo_advlock(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	struct flock *fl;
	int flags;
{

	return (EOPNOTSUPP);
}

/*
 * Fifo bad operation
 */
fifo_badop()
{

	panic("fifo_badop called");
	/* NOTREACHED */
}
#endif	/*FIFO*/
