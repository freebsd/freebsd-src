/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

static int	vop_nolookup(struct vop_lookup_args *);
static int	vop_nostrategy(struct vop_strategy_args *);

/*
 * This vnode table stores what we want to do if the filesystem doesn't
 * implement a particular VOP.
 *
 * If there is no specific entry here, we will return EOPNOTSUPP.
 *
 */

struct vop_vector default_vnodeops = {
	.vop_default =		NULL,
	.vop_bypass =		VOP_EOPNOTSUPP,

	.vop_advlock =		VOP_EINVAL,
	.vop_bmap =		vop_stdbmap,
	.vop_close =		VOP_NULL,
	.vop_fsync =		VOP_NULL,
	.vop_getpages =		vop_stdgetpages,
	.vop_getwritemount = 	vop_stdgetwritemount,
	.vop_inactive =		VOP_NULL,
	.vop_ioctl =		VOP_ENOTTY,
	.vop_islocked =		vop_stdislocked,
	.vop_lease =		VOP_NULL,
	.vop_lock =		vop_stdlock,
	.vop_lookup =		vop_nolookup,
	.vop_open =		VOP_NULL,
	.vop_pathconf =		VOP_EINVAL,
	.vop_poll =		vop_nopoll,
	.vop_putpages =		vop_stdputpages,
	.vop_readlink =		VOP_EINVAL,
	.vop_revoke =		VOP_PANIC,
	.vop_strategy =		vop_nostrategy,
	.vop_unlock =		vop_stdunlock,
};

/*
 * Series of placeholder functions for various error returns for
 * VOPs.
 */

int
vop_eopnotsupp(struct vop_generic_args *ap)
{
	/*
	printf("vop_notsupp[%s]\n", ap->a_desc->vdesc_name);
	*/

	return (EOPNOTSUPP);
}

int
vop_ebadf(struct vop_generic_args *ap)
{

	return (EBADF);
}

int
vop_enotty(struct vop_generic_args *ap)
{

	return (ENOTTY);
}

int
vop_einval(struct vop_generic_args *ap)
{

	return (EINVAL);
}

int
vop_null(struct vop_generic_args *ap)
{

	return (0);
}

/*
 * Helper function to panic on some bad VOPs in some filesystems.
 */
int
vop_panic(struct vop_generic_args *ap)
{

	panic("filesystem goof: vop_panic[%s]", ap->a_desc->vdesc_name);
}

/*
 * vop_std<something> and vop_no<something> are default functions for use by
 * filesystems that need the "default reasonable" implementation for a
 * particular operation.
 *
 * The documentation for the operations they implement exists (if it exists)
 * in the VOP_<SOMETHING>(9) manpage (all uppercase).
 */

/*
 * Default vop for filesystems that do not support name lookup
 */
static int
vop_nolookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 *	vop_nostrategy:
 *
 *	Strategy routine for VFS devices that have none.
 *
 *	BIO_ERROR and B_INVAL must be cleared prior to calling any strategy
 *	routine.  Typically this is done for a BIO_READ strategy call.
 *	Typically B_INVAL is assumed to already be clear prior to a write
 *	and should not be cleared manually unless you just made the buffer
 *	invalid.  BIO_ERROR should be cleared either way.
 */

static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("vnode", ap->a_vp);
	ap->a_bp->b_ioflags |= BIO_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	bufdone(ap->a_bp);
	return (EOPNOTSUPP);
}

/*
 * vop_stdpathconf:
 *
 * Standard implementation of POSIX pathconf, to get information about limits
 * for a filesystem.
 * Override per filesystem for the case where the filesystem has smaller
 * limits.
 */
int
vop_stdpathconf(ap)
	struct vop_pathconf_args /* {
	struct vnode *a_vp;
	int a_name;
	int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
		case _PC_LINK_MAX:
			*ap->a_retval = LINK_MAX;
			return (0);
		case _PC_MAX_CANON:
			*ap->a_retval = MAX_CANON;
			return (0);
		case _PC_MAX_INPUT:
			*ap->a_retval = MAX_INPUT;
			return (0);
		case _PC_PIPE_BUF:
			*ap->a_retval = PIPE_BUF;
			return (0);
		case _PC_CHOWN_RESTRICTED:
			*ap->a_retval = 1;
			return (0);
		case _PC_VDISABLE:
			*ap->a_retval = _POSIX_VDISABLE;
			return (0);
		default:
			return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Standard lock, unlock and islocked functions.
 */
int
vop_stdlock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

#ifndef	DEBUG_LOCKS
	return (lockmgr(vp->v_vnlock, ap->a_flags, VI_MTX(vp), ap->a_td));
#else
	return (debuglockmgr(vp->v_vnlock, ap->a_flags, VI_MTX(vp),
	    ap->a_td, "vop_stdlock", vp->filename, vp->line));
#endif
}

/* See above. */
int
vop_stdunlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(vp->v_vnlock, ap->a_flags | LK_RELEASE, VI_MTX(vp),
	    ap->a_td));
}

/* See above. */
int
vop_stdislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{

	return (lockstatus(ap->a_vp->v_vnlock, ap->a_td));
}

/*
 * Return true for select/poll.
 */
int
vop_nopoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	/*
	 * Return true for read/write.  If the user asked for something
	 * special, return POLLNVAL, so that clients have a way of
	 * determining reliably whether or not the extended
	 * functionality is present without hard-coding knowledge
	 * of specific filesystem implementations.
	 * Stay in sync with kern_conf.c::no_poll().
	 */
	if (ap->a_events & ~POLLSTANDARD)
		return (POLLNVAL);

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Implement poll for local filesystems that support it.
 */
int
vop_stdpoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	if (ap->a_events & ~POLLSTANDARD)
		return (vn_pollrecord(ap->a_vp, ap->a_td, ap->a_events));
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Return our mount point, as we will take charge of the writes.
 */
int
vop_stdgetwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{

	*(ap->a_mpp) = ap->a_vp->v_mount;
	return (0);
}

/* XXX Needs good comment and VOP_BMAP(9) manpage */
int
vop_stdbmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct bufobj **a_bop;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	if (ap->a_bop != NULL)
		*ap->a_bop = &ap->a_vp->v_bufobj;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(ap->a_vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

int
vop_stdfsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct buf *bp;
	struct bufobj *bo;
	struct buf *nbp;
	int s, error = 0;
	int maxretry = 100;     /* large, arbitrarily chosen */

	VI_LOCK(vp);
loop1:
	/*
	 * MARK/SCAN initialization to avoid infinite loops.
	 */
	s = splbio();
        TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs) {
                bp->b_vflags &= ~BV_SCANNED;
		bp->b_error = 0;
	}
	splx(s);

	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop2:
	s = splbio();
	TAILQ_FOREACH_SAFE(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs, nbp) {
		if ((bp->b_vflags & BV_SCANNED) != 0)
			continue;
		bp->b_vflags |= BV_SCANNED;
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;
		VI_UNLOCK(vp);
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("fsync: not dirty");
		if ((vp->v_object != NULL) && (bp->b_flags & B_CLUSTEROK)) {
			vfs_bio_awrite(bp);
			splx(s);
		} else {
			bremfree(bp);
			splx(s);
			bawrite(bp);
		}
		VI_LOCK(vp);
		goto loop2;
	}

	/*
	 * If synchronous the caller expects us to completely resolve all
	 * dirty buffers in the system.  Wait for in-progress I/O to
	 * complete (which could include background bitmap writes), then
	 * retry if dirty blocks still exist.
	 */
	if (ap->a_waitfor == MNT_WAIT) {
		bo = &vp->v_bufobj;
		bufobj_wwait(bo, 0, 0);
		if (bo->bo_dirty.bv_cnt > 0) {
			/*
			 * If we are unable to write any of these buffers
			 * then we fail now rather than trying endlessly
			 * to write them out.
			 */
			TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs)
				if ((error = bp->b_error) == 0)
					continue;
			if (error == 0 && --maxretry >= 0) {
				splx(s);
				goto loop1;
			}
			vprint("fsync: giving up on dirty", vp);
			error = EAGAIN;
		}
	}
	VI_UNLOCK(vp);
	splx(s);

	return (error);
}

/* XXX Needs good comment and more info in the manpage (VOP_GETPAGES(9)). */
int
vop_stdgetpages(ap)
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
	    ap->a_count, ap->a_reqpage);
}

/* XXX Needs good comment and more info in the manpage (VOP_PUTPAGES(9)). */
int
vop_stdputpages(ap)
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
	     ap->a_sync, ap->a_rtvals);
}

/*
 * vfs default ops
 * used to fill the vfs function table to get reasonable default return values.
 */
int
vfs_stdroot (mp, vpp, td)
	struct mount *mp;
	struct vnode **vpp;
	struct thread *td;
{

	return (EOPNOTSUPP);
}

int
vfs_stdstatfs (mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{

	return (EOPNOTSUPP);
}

int
vfs_stdvptofh (vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdquotactl (mp, cmds, uid, arg, td)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{

	return (EOPNOTSUPP);
}

int
vfs_stdsync(mp, waitfor, td)
	struct mount *mp;
	int waitfor;
	struct thread *td;
{
	struct vnode *vp, *nvp;
	int error, lockreq, allerror = 0;

	lockreq = LK_EXCLUSIVE | LK_INTERLOCK;
	if (waitfor != MNT_WAIT)
		lockreq |= LK_NOWAIT;
	/*
	 * Force stale buffer cache information to be flushed.
	 */
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, nvp) {

		VI_LOCK(vp);
		if (vp->v_bufobj.bo_dirty.bv_cnt == 0) {
			VI_UNLOCK(vp);
			continue;
		}
		MNT_IUNLOCK(mp);

		if ((error = vget(vp, lockreq, td)) != 0) {
			MNT_ILOCK(mp);
			if (error == ENOENT)
				goto loop;
			continue;
		}
		error = VOP_FSYNC(vp, waitfor, td);
		if (error)
			allerror = error;

		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (allerror);
}

int
vfs_stdnosync (mp, waitfor, td)
	struct mount *mp;
	int waitfor;
	struct thread *td;
{

	return (0);
}

int
vfs_stdvget (mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdfhtovp (mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdinit (vfsp)
	struct vfsconf *vfsp;
{

	return (0);
}

int
vfs_stduninit (vfsp)
	struct vfsconf *vfsp;
{

	return(0);
}

int
vfs_stdextattrctl(mp, cmd, filename_vp, attrnamespace, attrname, td)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int attrnamespace;
	const char *attrname;
	struct thread *td;
{

	if (filename_vp != NULL)
		VOP_UNLOCK(filename_vp, 0, td);
	return (EOPNOTSUPP);
}

int
vfs_stdsysctl(mp, op, req)
	struct mount *mp;
	fsctlop_t op;
	struct sysctl_req *req;
{

	return (EOPNOTSUPP);
}

/* end of vfs default ops */
