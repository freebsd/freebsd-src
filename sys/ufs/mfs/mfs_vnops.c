/*
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
 *	@(#)mfs_vnops.c	8.11 (Berkeley) 5/22/95
 * $Id: mfs_vnops.c,v 1.31 1997/10/27 13:33:47 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

MALLOC_DECLARE(M_MFSNODE);

static int	mfs_badop __P((struct vop_generic_args *));
static int	mfs_bmap __P((struct vop_bmap_args *));
static int	mfs_close __P((struct vop_close_args *));
static int	mfs_fsync __P((struct vop_fsync_args *));
static int	mfs_inactive __P((struct vop_inactive_args *)); /* XXX */
static int	mfs_open __P((struct vop_open_args *));
static int	mfs_reclaim __P((struct vop_reclaim_args *)); /* XXX */
static int	mfs_print __P((struct vop_print_args *)); /* XXX */
static int	mfs_strategy __P((struct vop_strategy_args *)); /* XXX */
/*
 * mfs vnode operations.
 */
vop_t **mfs_vnodeop_p;
static struct vnodeopv_entry_desc mfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) mfs_badop },
	{ &vop_bmap_desc,		(vop_t *) mfs_bmap },
	{ &vop_bwrite_desc,		(vop_t *) vop_defaultop },
	{ &vop_close_desc,		(vop_t *) mfs_close },
	{ &vop_fsync_desc,		(vop_t *) mfs_fsync },
	{ &vop_getpages_desc,		(vop_t *) vop_defaultop },
	{ &vop_inactive_desc,		(vop_t *) mfs_inactive },
	{ &vop_ioctl_desc,		(vop_t *) vop_enotty },
	{ &vop_islocked_desc,		(vop_t *) vop_defaultop },
	{ &vop_lock_desc,		(vop_t *) vop_defaultop },
	{ &vop_open_desc,		(vop_t *) mfs_open },
	{ &vop_print_desc,		(vop_t *) mfs_print },
	{ &vop_reclaim_desc,		(vop_t *) mfs_reclaim },
	{ &vop_strategy_desc,		(vop_t *) mfs_strategy },
	{ &vop_unlock_desc,		(vop_t *) vop_defaultop },
	{ NULL, NULL }
};
static struct vnodeopv_desc mfs_vnodeop_opv_desc =
	{ &mfs_vnodeop_p, mfs_vnodeop_entries };

VNODEOP_SET(mfs_vnodeop_opv_desc);

/*
 * Vnode Operations.
 *
 * Open called to allow memory filesystem to initialize and
 * validate before actual IO. Record our process identifier
 * so we can tell when we are doing I/O to ourself.
 */
/* ARGSUSED */
static int
mfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	if (ap->a_vp->v_type != VBLK) {
		panic("mfs_open not VBLK");
		/* NOTREACHED */
	}
	return (0);
}

static int
mfs_fsync(ap)
	struct vop_fsync_args *ap;
{

	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_fsync), ap));
}

/*
 * Pass I/O requests to the memory filesystem process.
 */
static int
mfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct mfsnode *mfsp;
	struct vnode *vp;
	struct proc *p = curproc;		/* XXX */

	if (!vfinddev(bp->b_dev, VBLK, &vp) || vp->v_usecount == 0)
		panic("mfs_strategy: bad dev");
	mfsp = VTOMFS(vp);
	/* check for mini-root access */
	if (mfsp->mfs_pid == 0) {
		caddr_t base;

		base = mfsp->mfs_baseoff + (bp->b_blkno << DEV_BSHIFT);
		if (bp->b_flags & B_READ)
			bcopy(base, bp->b_data, bp->b_bcount);
		else
			bcopy(bp->b_data, base, bp->b_bcount);
		biodone(bp);
	} else if (mfsp->mfs_pid == p->p_pid) {
		mfs_doio(bp, mfsp->mfs_baseoff);
	} else {
		bufq_insert_tail(&mfsp->buf_queue, bp);
		wakeup((caddr_t)vp);
	}
	return (0);
}

/*
 * Memory file system I/O.
 *
 * Trivial on the HP since buffer has already been mapping into KVA space.
 */
void
mfs_doio(bp, base)
	register struct buf *bp;
	caddr_t base;
{

	base += (bp->b_blkno << DEV_BSHIFT);
	if (bp->b_flags & B_READ)
		bp->b_error = copyin(base, bp->b_data, bp->b_bcount);
	else
		bp->b_error = copyout(bp->b_data, base, bp->b_bcount);
	if (bp->b_error)
		bp->b_flags |= B_ERROR;
	biodone(bp);
}

/*
 * This is a noop, simply returning what one has been given.
 */
static int
mfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		ufs_daddr_t  a_bn;
		struct vnode **a_vpp;
		ufs_daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

/*
 * Memory filesystem close routine
 */
/* ARGSUSED */
static int
mfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct mfsnode *mfsp = VTOMFS(vp);
	register struct buf *bp;
	int error;

	/*
	 * Finish any pending I/O requests.
	 */
	while (bp = bufq_first(&mfsp->buf_queue)) {
		bufq_remove(&mfsp->buf_queue, bp);
		mfs_doio(bp, mfsp->mfs_baseoff);
		wakeup((caddr_t)bp);
	}
	/*
	 * On last close of a memory filesystem
	 * we must invalidate any in core blocks, so that
	 * we can, free up its vnode.
	 */
	if (error = vinvalbuf(vp, 1, ap->a_cred, ap->a_p, 0, 0))
		return (error);
	/*
	 * There should be no way to have any more uses of this
	 * vnode, so if we find any other uses, it is a panic.
	 */
	if (vp->v_usecount > 1)
		printf("mfs_close: ref count %d > 1\n", vp->v_usecount);
	if (vp->v_usecount > 1 || (bufq_first(&mfsp->buf_queue) != NULL))
		panic("mfs_close");
	/*
	 * Send a request to the filesystem server to exit.
	 */
	mfsp->mfs_active = 0;
	wakeup((caddr_t)vp);
	return (0);
}

/*
 * Memory filesystem inactive routine
 */
/* ARGSUSED */
static int
mfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct mfsnode *mfsp = VTOMFS(vp);

	if (bufq_first(&mfsp->buf_queue) != NULL)
		panic("mfs_inactive: not inactive (next buffer %p)",
			bufq_first(&mfsp->buf_queue));
	VOP_UNLOCK(vp, 0, ap->a_p);
	return (0);
}

/*
 * Reclaim a memory filesystem devvp so that it can be reused.
 */
static int
mfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;

	FREE(vp->v_data, M_MFSNODE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Print out the contents of an mfsnode.
 */
static int
mfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct mfsnode *mfsp = VTOMFS(ap->a_vp);

	printf("tag VT_MFS, pid %ld, base %p, size %ld\n", mfsp->mfs_pid,
		mfsp->mfs_baseoff, mfsp->mfs_size);
	return (0);
}

/*
 * Block device bad operation
 */
static int
mfs_badop(struct vop_generic_args *ap)
{
	int i;

	printf("mfs_badop[%s]\n", ap->a_desc->vdesc_name);
	i = vop_defaultop(ap);
	printf("mfs_badop[%s] = %d\n", ap->a_desc->vdesc_name,i);
	return (i);
}
