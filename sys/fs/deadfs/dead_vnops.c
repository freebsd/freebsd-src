/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)dead_vnops.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/fs/deadfs/dead_vnops.c,v 1.50.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/vnode.h>

/*
 * Prototypes for dead operations on vnodes.
 */
static vop_bmap_t	dead_bmap;
static vop_ioctl_t	dead_ioctl;
static vop_lookup_t	dead_lookup;
static vop_open_t	dead_open;
static vop_poll_t	dead_poll;
static vop_read_t	dead_read;
static vop_write_t	dead_write;
static vop_getwritemount_t dead_getwritemount;
static vop_rename_t	dead_rename;

struct vop_vector dead_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		VOP_EBADF,
	.vop_advlock =		VOP_EBADF,
	.vop_bmap =		dead_bmap,
	.vop_create =		VOP_PANIC,
	.vop_getattr =		VOP_EBADF,
	.vop_getwritemount =	dead_getwritemount,
	.vop_inactive =		VOP_NULL,
	.vop_ioctl =		dead_ioctl,
	.vop_link =		VOP_PANIC,
	.vop_lookup =		dead_lookup,
	.vop_mkdir =		VOP_PANIC,
	.vop_mknod =		VOP_PANIC,
	.vop_open =		dead_open,
	.vop_pathconf =		VOP_EBADF,	/* per pathconf(2) */
	.vop_poll =		dead_poll,
	.vop_read =		dead_read,
	.vop_readdir =		VOP_EBADF,
	.vop_readlink =		VOP_EBADF,
	.vop_reclaim =		VOP_NULL,
	.vop_remove =		VOP_PANIC,
	.vop_rename =		dead_rename,
	.vop_rmdir =		VOP_PANIC,
	.vop_setattr =		VOP_EBADF,
	.vop_symlink =		VOP_PANIC,
	.vop_write =		dead_write,
};

/* ARGSUSED */
static int
dead_getwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{
	*(ap->a_mpp) = NULL;
	return (0);
}

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
static int
dead_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open always fails as if device did not exist.
 */
/* ARGSUSED */
static int
dead_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (ENXIO);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
static int
dead_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	/*
	 * Return EOF for tty devices, EIO for others
	 */
	if ((ap->a_vp->v_vflag & VV_ISTTY) == 0)
		return (EIO);
	return (0);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
static int
dead_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	return (EIO);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
static int
dead_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	/* XXX: Doesn't this just recurse back here ? */
	return (VOP_IOCTL_AP(ap));
}

/*
 * Wait until the vnode has finished changing state.
 */
static int
dead_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct bufobj **a_bop;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	return (VOP_BMAP(ap->a_vp, ap->a_bn, ap->a_bop, ap->a_bnp, ap->a_runp, ap->a_runb));
}

/*
 * Trivial poll routine that always returns POLLHUP.
 * This is necessary so that a process which is polling a file
 * gets notified when that file is revoke()d.
 */
static int
dead_poll(ap)
	struct vop_poll_args *ap;
{
	return (POLLHUP);
}

static int
dead_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	if (ap->a_tvp)
		vput(ap->a_tvp);
	if (ap->a_tdvp == ap->a_tvp)
		vrele(ap->a_tdvp);
	else
		vput(ap->a_tdvp);
	vrele(ap->a_fdvp);
	vrele(ap->a_fvp);
	return (EXDEV);
}
