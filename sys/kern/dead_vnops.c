/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	from: @(#)dead_vnops.c	7.13 (Berkeley) 4/15/91
 *	$Id: dead_vnops.c,v 1.2 1993/10/16 15:23:59 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "time.h"
#include "vnode.h"
#include "errno.h"
#include "namei.h"
#include "buf.h"

/*
 * Prototypes for dead operations on vnodes.
 */
int	dead_badop(),
	dead_ebadf();
int	dead_lookup __P((
		struct vnode *vp,
		struct nameidata *ndp,
		struct proc *p));
#define dead_create ((int (*) __P(( \
		struct nameidata *ndp, \
		struct vattr *vap, \
		struct proc *p))) dead_badop)
#define dead_mknod ((int (*) __P(( \
		struct nameidata *ndp, \
		struct vattr *vap, \
		struct ucred *cred, \
		struct proc *p))) dead_badop)
int	dead_open __P((
		struct vnode *vp,
		int mode,
		struct ucred *cred,
		struct proc *p));
#define dead_close ((int (*) __P(( \
		struct vnode *vp, \
		int fflag, \
		struct ucred *cred, \
		struct proc *p))) nullop)
#define dead_access ((int (*) __P(( \
		struct vnode *vp, \
		int mode, \
		struct ucred *cred, \
		struct proc *p))) dead_ebadf)
#define dead_getattr ((int (*) __P(( \
		struct vnode *vp, \
		struct vattr *vap, \
		struct ucred *cred, \
		struct proc *p))) dead_ebadf)
#define dead_setattr ((int (*) __P(( \
		struct vnode *vp, \
		struct vattr *vap, \
		struct ucred *cred, \
		struct proc *p))) dead_ebadf)
int	dead_read __P((
		struct vnode *vp,
		struct uio *uio,
		int ioflag,
		struct ucred *cred));
int	dead_write __P((
		struct vnode *vp,
		struct uio *uio,
		int ioflag,
		struct ucred *cred));
int	dead_ioctl __P((
		struct vnode *vp,
		int command,
		caddr_t data,
		int fflag,
		struct ucred *cred,
		struct proc *p));
int	dead_select __P((
		struct vnode *vp,
		int which,
		int fflags,
		struct ucred *cred,
		struct proc *p));
#define dead_mmap ((int (*) __P(( \
		struct vnode *vp, \
		int fflags, \
		struct ucred *cred, \
		struct proc *p))) dead_badop)
#define dead_fsync ((int (*) __P(( \
		struct vnode *vp, \
		int fflags, \
		struct ucred *cred, \
		int waitfor, \
		struct proc *p))) nullop)
#define dead_seek ((int (*) __P(( \
		struct vnode *vp, \
		off_t oldoff, \
		off_t newoff, \
		struct ucred *cred))) nullop)
#define dead_remove ((int (*) __P(( \
		struct nameidata *ndp, \
		struct proc *p))) dead_badop)
#define dead_link ((int (*) __P(( \
		struct vnode *vp, \
		struct nameidata *ndp, \
		struct proc *p))) dead_badop)
#define dead_rename ((int (*) __P(( \
		struct nameidata *fndp, \
		struct nameidata *tdnp, \
		struct proc *p))) dead_badop)
#define dead_mkdir ((int (*) __P(( \
		struct nameidata *ndp, \
		struct vattr *vap, \
		struct proc *p))) dead_badop)
#define dead_rmdir ((int (*) __P(( \
		struct nameidata *ndp, \
		struct proc *p))) dead_badop)
#define dead_symlink ((int (*) __P(( \
		struct nameidata *ndp, \
		struct vattr *vap, \
		char *target, \
		struct proc *p))) dead_badop)
#define dead_readdir ((int (*) __P(( \
		struct vnode *vp, \
		struct uio *uio, \
		struct ucred *cred, \
		int *eofflagp))) dead_ebadf)
#define dead_readlink ((int (*) __P(( \
		struct vnode *vp, \
		struct uio *uio, \
		struct ucred *cred))) dead_ebadf)
#define dead_abortop ((int (*) __P(( \
		struct nameidata *ndp))) dead_badop)
#define dead_inactive ((int (*) __P(( \
		struct vnode *vp, \
		struct proc *p))) nullop)
#define dead_reclaim ((int (*) __P(( \
		struct vnode *vp))) nullop)
int	dead_lock __P((
		struct vnode *vp));
#define dead_unlock ((int (*) __P(( \
		struct vnode *vp))) nullop)
int	dead_bmap __P((
		struct vnode *vp,
		daddr_t bn,
		struct vnode **vpp,
		daddr_t *bnp));
int	dead_strategy __P((
		struct buf *bp));
int	dead_print __P((
		struct vnode *vp));
#define dead_islocked ((int (*) __P(( \
		struct vnode *vp))) nullop)
#define dead_advlock ((int (*) __P(( \
		struct vnode *vp, \
		caddr_t id, \
		int op, \
		struct flock *fl, \
		int flags))) dead_ebadf)

struct vnodeops dead_vnodeops = {
	dead_lookup,	/* lookup */
	dead_create,	/* create */
	dead_mknod,	/* mknod */
	dead_open,	/* open */
	dead_close,	/* close */
	dead_access,	/* access */
	dead_getattr,	/* getattr */
	dead_setattr,	/* setattr */
	dead_read,	/* read */
	dead_write,	/* write */
	dead_ioctl,	/* ioctl */
	dead_select,	/* select */
	dead_mmap,	/* mmap */
	dead_fsync,	/* fsync */
	dead_seek,	/* seek */
	dead_remove,	/* remove */
	dead_link,	/* link */
	dead_rename,	/* rename */
	dead_mkdir,	/* mkdir */
	dead_rmdir,	/* rmdir */
	dead_symlink,	/* symlink */
	dead_readdir,	/* readdir */
	dead_readlink,	/* readlink */
	dead_abortop,	/* abortop */
	dead_inactive,	/* inactive */
	dead_reclaim,	/* reclaim */
	dead_lock,	/* lock */
	dead_unlock,	/* unlock */
	dead_bmap,	/* bmap */
	dead_strategy,	/* strategy */
	dead_print,	/* print */
	dead_islocked,	/* islocked */
	dead_advlock,	/* advlock */
};

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
dead_lookup(vp, ndp, p)
	struct vnode *vp;
	struct nameidata *ndp;
	struct proc *p;
{

	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	return (ENOTDIR);
}

/*
 * Open always fails as if device did not exist.
 */
/* ARGSUSED */
dead_open(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{

	return (ENXIO);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
dead_read(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	if (chkvnlock(vp))
		panic("dead_read: lock");
	/*
	 * Return EOF for character devices, EIO for others
	 */
	if (vp->v_type != VCHR)
		return (EIO);
	return (0);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
dead_write(vp, uio, ioflag, cred)
	register struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	if (chkvnlock(vp))
		panic("dead_write: lock");
	return (EIO);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
dead_ioctl(vp, com, data, fflag, cred, p)
	struct vnode *vp;
	register int com;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{

	if (!chkvnlock(vp))
		return (EBADF);
	return (VOP_IOCTL(vp, com, data, fflag, cred, p));
}

/* ARGSUSED */
dead_select(vp, which, fflags, cred, p)
	struct vnode *vp;
	int which, fflags;
	struct ucred *cred;
	struct proc *p;
{

	/*
	 * Let the user find out that the descriptor is gone.
	 */
	return (1);
}

/*
 * Just call the device strategy routine
 */
dead_strategy(bp)
	register struct buf *bp;
{

	if (bp->b_vp == NULL || !chkvnlock(bp->b_vp)) {
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return (EIO);
	}
	return (VOP_STRATEGY(bp));
}

/*
 * Wait until the vnode has finished changing state.
 */
dead_lock(vp)
	struct vnode *vp;
{

	if (!chkvnlock(vp))
		return (0);
	return (VOP_LOCK(vp));
}

/*
 * Wait until the vnode has finished changing state.
 */
dead_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{

	if (!chkvnlock(vp))
		return (EIO);
	return (VOP_BMAP(vp, bn, vpp, bnp));
}

/*
 * Print out the contents of a dead vnode.
 */
/* ARGSUSED */
dead_print(vp)
	struct vnode *vp;
{

	printf("tag VT_NON, dead vnode\n");
}

/*
 * Empty vnode failed operation
 */
dead_ebadf()
{

	return (EBADF);
}

/*
 * Empty vnode bad operation
 */
dead_badop()
{

	panic("dead_badop called");
	/* NOTREACHED */
}

/*
 * Empty vnode null operation
 */
dead_nullop()
{

	return (0);
}

/*
 * We have to wait during times when the vnode is
 * in a state of change.
 */
chkvnlock(vp)
	register struct vnode *vp;
{
	int locked = 0;

	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		sleep((caddr_t)vp, PINOD);
		locked = 1;
	}
	return (locked);
}
