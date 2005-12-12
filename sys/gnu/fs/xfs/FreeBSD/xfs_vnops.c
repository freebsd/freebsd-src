/*
 * Copyright (c) 2001, Alexander Kabaev
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/extattr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <fs/fifofs/fifo.h>

#define NO_VFS_MACROS
#include "xfs.h"
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_imap.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_attr.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_acl.h"
#include "xfs_cap.h"
#include "xfs_mac.h"
#include "xfs_iomap.h"
#include "xfs_clnt.h"
#include "xfs_mountops.h"

/*
 * Prototypes for XFS vnode operations.
 */
static vop_access_t		_xfs_access;
static vop_advlock_t		_xfs_advlock;
static vop_bmap_t		_xfs_bmap;
static vop_cachedlookup_t	_xfs_cachedlookup;
static vop_close_t		_xfs_close;
static vop_create_t		_xfs_create;
static vop_fsync_t		_xfs_fsync;
static vop_getattr_t		_xfs_getattr;
static vop_getextattr_t		_xfs_getextattr;
static vop_inactive_t		_xfs_inactive;
static vop_ioctl_t		_xfs_ioctl;
static vop_link_t		_xfs_link;
static vop_listextattr_t	_xfs_listextattr;
static vop_mkdir_t		_xfs_mkdir;
static vop_mknod_t		_xfs_mknod;
static vop_open_t		_xfs_open;
static vop_read_t		_xfs_read;
static vop_readdir_t		_xfs_readdir;
static vop_readlink_t		_xfs_readlink;
static vop_reclaim_t		_xfs_reclaim;
static vop_remove_t		_xfs_remove;
static vop_rename_t		_xfs_rename;
static vop_rmdir_t		_xfs_rmdir;
static vop_setattr_t		_xfs_setattr;
static vop_strategy_t		_xfs_strategy;
static vop_symlink_t		_xfs_symlink;
static vop_write_t		_xfs_write;

struct vop_vector xfs_vnops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		_xfs_access,
	.vop_advlock =		_xfs_advlock,
	.vop_bmap =		_xfs_bmap,
	.vop_cachedlookup =	_xfs_cachedlookup,
	.vop_close =		_xfs_close,
	.vop_create =		_xfs_create,
	.vop_fsync =		_xfs_fsync,
	.vop_getattr =		_xfs_getattr,
	.vop_getextattr =	_xfs_getextattr,
	.vop_inactive =		_xfs_inactive,
	.vop_ioctl =		_xfs_ioctl,
	.vop_link =		_xfs_link,
	.vop_listextattr =	_xfs_listextattr,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mkdir =		_xfs_mkdir,
	.vop_mknod =		_xfs_mknod,
	.vop_open =		_xfs_open,
	.vop_read =		_xfs_read,
	.vop_readdir =		_xfs_readdir,
	.vop_readlink =		_xfs_readlink,
	.vop_reclaim =		_xfs_reclaim,
	.vop_remove =		_xfs_remove,
	.vop_rename =		_xfs_rename,
	.vop_rmdir =		_xfs_rmdir,
	.vop_setattr =		_xfs_setattr,
	.vop_strategy =		_xfs_strategy,
	.vop_symlink =		_xfs_symlink,
	.vop_write =		_xfs_write,
};

/*
 *  FIFO's specific operations.
 */

static vop_close_t	_xfsfifo_close;
static vop_read_t	_xfsfifo_read;
static vop_kqfilter_t	_xfsfifo_kqfilter;
static vop_write_t	_xfsfifo_write;

struct vop_vector xfs_fifoops = {
	.vop_default =		&fifo_specops,
	.vop_access =		_xfs_access,
	.vop_close =		_xfsfifo_close,
	.vop_fsync =		_xfs_fsync,
	.vop_getattr =		_xfs_getattr,
	.vop_inactive =		_xfs_inactive,
	.vop_kqfilter =		_xfsfifo_kqfilter,
	.vop_read =		_xfsfifo_read,
	.vop_reclaim =		_xfs_reclaim,
	.vop_setattr =		_xfs_setattr,
	.vop_write =		_xfsfifo_write,
};

static int
_xfs_access(
    	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	int error;

	XVOP_ACCESS(VPTOXFSVP(ap->a_vp), ap->a_mode, ap->a_cred, error);
	return (error);
}

static int
_xfs_open(
    	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
		int  a_fdidx;
	} */ *ap)
{
	int error;

	XVOP_OPEN(VPTOXFSVP(ap->a_vp), ap->a_cred, error);
	if (error == 0)
		vnode_create_vobject(ap->a_vp, 0, ap->a_td);
	return (error);
}

static int
_xfs_close(
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	int error = 0;
	/* XVOP_CLOSE(VPTOXFSVP(ap->a_vp), NULL, error); */
	return (error);
}

static int
_xfs_getattr(
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode	*vp = ap->a_vp;
	struct vattr	*vap = ap->a_vap;
	struct mount	*mp;
	xfs_vattr_t	va;
	int		error;


	VATTR_NULL(vap);
	memset(&va,0,sizeof(xfs_vattr_t));
	va.va_mask = XFS_AT_STAT|XFS_AT_GENCOUNT|XFS_AT_XFLAGS;

	XVOP_GETATTR(VPTOXFSVP(vp), &va, 0, ap->a_cred, error);
	if (error) return (error);

	mp  = vp->v_mount;

	vap->va_type = va.va_type;
	vap->va_mode = va.va_mode;
	vap->va_nlink = va.va_nlink;
	vap->va_uid = va.va_uid;
	vap->va_gid = va.va_gid;
	vap->va_fsid = mp->mnt_stat.f_fsid.val[0];
	vap->va_fileid = va.va_nodeid;
	vap->va_size = va.va_size;
	vap->va_blocksize = va.va_blocksize;
	vap->va_atime = va.va_atime;
	vap->va_mtime = va.va_mtime;
	vap->va_ctime = va.va_ctime;
	vap->va_gen = va.va_gen;
	vap->va_rdev = va.va_rdev;
	vap->va_bytes = (va.va_nblocks << BBSHIFT);

	/* XFS now supports devices that have block sizes
	 * other than 512 so BBSHIFT will work for now
	 * but need to get this value from the super block
	 */

	/*
	 * Fields with no direct equivalent in XFS
	 * leave initialized by VATTR_NULL
	 */
#if 0
	vap->va_filerev = 0;
	vap->va_birthtime = va.va_ctime;
	vap->va_vaflags = 0;
	vap->va_flags = 0;
	vap->va_spare = 0;
#endif

	return (0);
}

static int
_xfs_setattr(
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	xfs_vattr_t   va;
	int error;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL))
		return (EINVAL);

	memset(&va, 0, sizeof(va));

	if (vap->va_uid != (uid_t)VNOVAL) {
		va.va_mask |= XFS_AT_UID;
		va.va_uid = vap->va_uid;
	}
	if (vap->va_gid != (gid_t)VNOVAL) {
		va.va_mask |= XFS_AT_GID;
		va.va_gid = vap->va_gid;
	}
	if (vap->va_size != VNOVAL) {
		va.va_mask |= XFS_AT_SIZE;
		va.va_size = vap->va_size;
	}
	if (vap->va_atime.tv_sec != VNOVAL) {
		va.va_mask |= XFS_AT_ATIME;
		va.va_atime = vap->va_atime;
	}
	if (vap->va_mtime.tv_sec != VNOVAL) {
		va.va_mask |= XFS_AT_MTIME;
		va.va_mtime = vap->va_mtime;
	}
	if (vap->va_ctime.tv_sec != VNOVAL) {
		va.va_mask |= XFS_AT_CTIME;
		va.va_ctime = vap->va_ctime;
	}
	if (vap->va_mode != (mode_t)VNOVAL) {
		va.va_mask |= XFS_AT_MODE;
		va.va_mode = vap->va_mode;
	}

	XVOP_SETATTR(VPTOXFSVP(vp), &va, 0, ap->a_cred, error);
	return (error);
}

static int
_xfs_inactive(
	struct vop_inactive_args  /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct thread *td = ap->a_td;
	int error;

	XVOP_INACTIVE(VPTOXFSVP(vp), td->td_ucred, error);
	return (error);
}

static int
_xfs_read(
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
 	struct uio *uio = ap->a_uio;
	int error;

	switch (vp->v_type) {
	case VREG:
		break;
	case VDIR:
		return (EISDIR);
	default:
		return (EPERM);
	};

	XVOP_READ(VPTOXFSVP(vp), uio, ap->a_ioflag, ap->a_cred, error);
	return error;
}

int
xfs_read_file(xfs_mount_t *mp, xfs_inode_t *ip, struct uio *uio, int ioflag);

int
xfs_read_file(xfs_mount_t *mp, xfs_inode_t *ip, struct uio *uio, int ioflag)
{
	xfs_fileoff_t lbn, nextlbn;
	xfs_fsize_t bytesinfile;
	long size, xfersize, blkoffset;
	struct buf *bp;
	struct vnode *vp;
	int error, orig_resid;
	int seqcount;

	seqcount = ioflag >> IO_SEQSHIFT;

	orig_resid = uio->uio_resid;
	if (orig_resid <= 0)
		return (0);

	vp = XFS_ITOV(ip)->v_vnode;

	/*
	 * Ok so we couldn't do it all in one vm trick...
	 * so cycle around trying smaller bites..
	 */
	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_d.di_size - uio->uio_offset) <= 0)
			break;

		lbn = XFS_B_TO_FSBT(mp, uio->uio_offset);
		nextlbn = lbn + 1;

		/*
		 * size of buffer.  The buffer representing the
		 * end of the file is rounded up to the size of
		 * the block type ( fragment or full block,
		 * depending ).
		 */
		size = mp->m_sb.sb_blocksize;
		blkoffset = XFS_B_FSB_OFFSET(mp, uio->uio_offset);

		/*
		 * The amount we want to transfer in this iteration is
		 * one FS block less the amount of the data before
		 * our startpoint (duh!)
		 */
		xfersize = mp->m_sb.sb_blocksize - blkoffset;

		/*
		 * But if we actually want less than the block,
		 * or the file doesn't have a whole block more of data,
		 * then use the lesser number.
		 */
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (XFS_FSB_TO_B(mp, nextlbn) >= ip->i_d.di_size ) {
			/*
			 * Don't do readahead if this is the end of the file.
			 */
			error = bread(vp, lbn, size, NOCRED, &bp);
		} else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			/*
			 * Otherwise if we are allowed to cluster,
			 * grab as much as we can.
			 *
			 * XXX  This may not be a win if we are not
			 * doing sequential access.
			 */
			error = cluster_read(vp, ip->i_d.di_size, lbn,
				size, NOCRED, uio->uio_resid, seqcount, &bp);
		} else if (seqcount > 1) {
			/*
			 * If we are NOT allowed to cluster, then
			 * if we appear to be acting sequentially,
			 * fire off a request for a readahead
			 * as well as a read. Note that the 4th and 5th
			 * arguments point to arrays of the size specified in
			 * the 6th argument.
			 */
			int nextsize = mp->m_sb.sb_blocksize;
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else {
			/*
			 * Failing all of the above, just read what the
			 * user asked for. Interestingly, the same as
			 * the first option above.
			 */
			error = bread(vp, lbn, size, NOCRED, &bp);
		}
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}

		/*
		 * If IO_DIRECT then set B_DIRECT for the buffer.  This
		 * will cause us to attempt to release the buffer later on
		 * and will cause the buffer cache to attempt to free the
		 * underlying pages.
		 */
		if (ioflag & IO_DIRECT)
			bp->b_flags |= B_DIRECT;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}

		/*
		 * otherwise use the general form
		 */
		error = uiomove((char *)bp->b_data + blkoffset,
			    (int)xfersize, uio);

		if (error)
			break;

		if (ioflag & (IO_VMIO|IO_DIRECT) ) {
			/*
			 * If there are no dependencies, and it's VMIO,
			 * then we don't need the buf, mark it available
			 * for freeing. The VM has the data.
			 */
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			/*
			 * Otherwise let whoever
			 * made the request take care of
			 * freeing it. We just queue
			 * it onto another list.
			 */
			bqrelse(bp);
		}
	}

	/*
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL) {
		if (ioflag & (IO_VMIO|IO_DIRECT)) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else
			bqrelse(bp);
	}

	return (error);
}

static int
_xfs_write(struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
/* 	struct uio *uio = ap->a_uio; */

	if (vp->v_type != VREG)
		return (EPERM);
	return (EPERM);
}

static int
_xfs_create(
    	struct vop_create_args  /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	struct vnode *dvp = ap->a_dvp;
 	struct vattr *vap = ap->a_vap;
	struct thread *td = curthread;
	struct ucred *credp = td->td_ucred;
	struct componentname *cnp = ap->a_cnp;
	xfs_vnode_t *xvp;
	xfs_vattr_t va;
	int error;

	memset(&va, 0, sizeof (va));
	va.va_mask |= XFS_AT_MODE;
	va.va_mode = vap->va_mode;
	va.va_mask |= XFS_AT_TYPE;
	va.va_type = vap->va_type;

	xvp = NULL;
	XVOP_CREATE(VPTOXFSVP(dvp), cnp, &va, &xvp, credp, error);

	if (error == 0) {
		*ap->a_vpp = xvp->v_vnode;
		VOP_LOCK(xvp->v_vnode, LK_EXCLUSIVE, td);
	}

	return (error);
}

static int
_xfs_remove(
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	/*
	struct vnode *dvp = ap->a_dvp; 
 	struct componentname *cnp = ap->a_cnp;
	*/
	int error;

	if (vp->v_type == VDIR || vp->v_usecount != 1)
		return (EPERM);

	error = 0;
	cache_purge(vp);
	return (error);
}

static int
_xfs_rename(
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap)
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
/* 	struct componentname *tcnp = ap->a_tcnp; */
/*	struct componentname *fcnp = ap->a_fcnp;*/
	int error = EPERM;

	if (error)
		goto out;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (tvp && tvp->v_usecount > 1) {
		error = EBUSY;
		goto out;
	}

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	vgone(fvp);
	if (tvp)
		vgone(tvp);
	return (error);
}

static int
_xfs_link(
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	xfs_vnode_t *tdvp, *vp;
	int error;

	tdvp = VPTOXFSVP(ap->a_tdvp);
	vp = VPTOXFSVP(ap->a_vp);
	XVOP_LINK(tdvp, vp, ap->a_cnp, NULL, error);
	return (error);
}

static int
_xfs_symlink(
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap)
{
	struct thread *td = curthread;
	struct ucred  *credp = td->td_ucred;
	xfs_vnode_t *xvp;
	xfs_vattr_t va;
	int error;

	memset(&va, 0, sizeof (va));

	va.va_mask |= XFS_AT_MODE;
	va.va_mode = ap->a_vap->va_mode;
	va.va_mask |= XFS_AT_TYPE;
	va.va_type = ap->a_vap->va_type;

	XVOP_SYMLINK(VPTOXFSVP(ap->a_dvp), ap->a_cnp, &va, ap->a_target,
	    &xvp, credp, error);

	if (error == 0) {
		*ap->a_vpp = xvp->v_vnode;
		VOP_LOCK(xvp->v_vnode, LK_EXCLUSIVE, td);
	}

	return (error);
}

static int
_xfs_mknod(
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	struct vnode *dvp = ap->a_dvp;
 	struct vattr *vap = ap->a_vap;
	struct thread *td = curthread;
	struct ucred *credp = td->td_ucred;
	struct componentname *cnp = ap->a_cnp;
	xfs_vnode_t *xvp;
	xfs_vattr_t va;
	int error;

	memset(&va, 0, sizeof (va));
	va.va_mask |= XFS_AT_MODE;
	va.va_mode = vap->va_mode;
	va.va_mask |= XFS_AT_TYPE;
	va.va_type = vap->va_type;
	va.va_mask |= XFS_AT_RDEV;
	va.va_rdev = vap->va_rdev;

	xvp = NULL;
	XVOP_CREATE(VPTOXFSVP(dvp), cnp, &va, &xvp, credp, error);

	if (error == 0) {
		*ap->a_vpp = xvp->v_vnode;
		VOP_LOCK(xvp->v_vnode, LK_EXCLUSIVE, td);
	}

	return (error);
}

static int
_xfs_mkdir(
	struct vop_mkdir_args /* {
		 struct vnode *a_dvp;
		 struct vnode **a_vpp;
		 struct componentname *a_cnp;
		 struct vattr *a_vap;
	} */ *ap)
{
	struct vnode *dvp = ap->a_dvp;
 	struct vattr *vap = ap->a_vap;
	struct thread *td = curthread;
	struct ucred *credp = td->td_ucred;
	struct componentname *cnp = ap->a_cnp;
	xfs_vnode_t *xvp;
	xfs_vattr_t va;
	int error;

	memset(&va, 0, sizeof (va));
	va.va_mask |= XFS_AT_MODE;
	va.va_mode = vap->va_mode;
	va.va_mask |= XFS_AT_TYPE;
	va.va_type = vap->va_type;

	xvp = NULL;
	XVOP_MKDIR(VPTOXFSVP(dvp), cnp, &va, &xvp, credp, error);

	if (error == 0) {
		*ap->a_vpp = xvp->v_vnode;
		VOP_LOCK(xvp->v_vnode, LK_EXCLUSIVE, td);
	}

	return (error);
}

static int
_xfs_rmdir(
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
/* 	struct componentname *cnp = ap->a_cnp; */
	int error;

	if (dvp == vp)
		return (EINVAL);

	error = EPERM;

	return (error);
}

static int
_xfs_readdir(
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;
	off_t	off;
	int	eof = 0;

	if (vp->v_type != VDIR)
		return (EPERM);
	if (ap->a_ncookies) {
		return (EOPNOTSUPP);
	}

	error = 0;
	while (!eof){
		off = (int)uio->uio_offset;

		XVOP_READDIR(VPTOXFSVP(vp), uio, NULL, &eof, error);
		if ((uio->uio_offset == off) || error) {
			break;
		}
	}

	if (ap->a_eofflag)
		*ap->a_eofflag = (eof != 0);

        return (error);
}


static int
_xfs_readlink(
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct ucred *cred = ap->a_cred;
	int error;

	XVOP_READLINK(VPTOXFSVP(vp), uio, 0, cred, error);
	return (error);
}

static int
_xfs_fsync(
	struct vop_fsync_args /* {
		struct vnode * a_vp;
		int  a_waitfor;
		struct thread * a_td;
	} */ *ap)
{
	xfs_vnode_t  *vp = VPTOXFSVP(ap->a_vp);
	int flags = FSYNC_DATA;
	int error;

	if (ap->a_waitfor == MNT_WAIT)
		flags |= FSYNC_WAIT;
	XVOP_FSYNC(vp, flags, ap->a_td->td_ucred, (xfs_off_t)-1, (xfs_off_t)-1,
	    error);

	return (error);
}

static int
_xfs_bmap(
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct bufobj **a_bop;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap)
{
	xfs_iomap_t iomap;
	xfs_off_t offset;
	ssize_t   size;
	struct mount *mp;
	struct xfs_mount *xmp;
	struct xfs_vnode *xvp;
	int error, maxrun, retbm;

	mp  = ap->a_vp->v_mount;
	xmp = XFS_VFSTOM(MNTTOVFS(mp));
	if (ap->a_bop != NULL)
		*ap->a_bop = &xmp->m_ddev_targp->specvp->v_bufobj;
	if (ap->a_bnp == NULL)
		return (0);

	xvp = VPTOXFSVP(ap->a_vp);
	retbm = 1;

	offset = XFS_FSB_TO_B(xmp, ap->a_bn);
	size = XFS_FSB_TO_B(xmp, 1);
	XVOP_BMAP(xvp, offset, size, BMAPI_READ, &iomap, &retbm, error);
	if (error)
		return (error);
	if (retbm == 0 || iomap.iomap_bn == IOMAP_DADDR_NULL) {
		*ap->a_bnp = (daddr_t)-1;
		if (ap->a_runb)
			*ap->a_runb = 0;
		if (ap->a_runp)
			*ap->a_runp = 0;
	} else {
		*ap->a_bnp = iomap.iomap_bn + btodb(iomap.iomap_delta);
		maxrun = mp->mnt_iosize_max / mp->mnt_stat.f_iosize - 1;
		if (ap->a_runb) {
			*ap->a_runb = XFS_B_TO_FSB(xmp, iomap.iomap_delta);
			if (*ap->a_runb > maxrun)
				*ap->a_runb  = maxrun;
		}
		if (ap->a_runp) {
			*ap->a_runp =
			    XFS_B_TO_FSB(xmp, iomap.iomap_bsize
				- iomap.iomap_delta - size);
			if (*ap->a_runp > maxrun)
				*ap->a_runp  = maxrun;
		}
	}
	return (0);
}

static int
_xfs_strategy(
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap)
{
	daddr_t blkno;
	struct buf *bp;;
	struct bufobj *bo;
	struct vnode *vp;
	struct xfs_mount *xmp;
	int error;

	bp = ap->a_bp;
	vp = ap->a_vp;

	KASSERT(ap->a_vp == ap->a_bp->b_vp, ("%s(%p != %p)",
	    __func__, ap->a_vp, ap->a_bp->b_vp));
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &blkno, NULL, NULL);
		bp->b_blkno = blkno;
		bp->b_iooffset = (blkno << BBSHIFT);
		if (error) {
			bp->b_error = error;
			bp->b_ioflags |= BIO_ERROR;
			bufdone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			vfs_bio_clrbuf(bp);
        }
	if ((long)bp->b_blkno == -1) {
		bufdone(bp);
		return (0);
	}

	xmp = XFS_VFSTOM(MNTTOVFS(vp->v_mount));
	bo = &xmp->m_ddev_targp->specvp->v_bufobj;
	bo->bo_ops->bop_strategy(bo, bp);
	return (0);
}

int
_xfs_ioctl(
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int fflag;
		struct ucred *cred;
		struct thread *a_td;
	} */ *ap)
{
/* 	struct vnode *vp = ap->a_vp; */
/* 	struct thread *p = ap->a_td; */
/* 	struct file *fp; */
	int error;

	switch (ap->a_command) {
	default:
		error = EINVAL;
	}
	return (error);
}

int
_xfs_advlock(
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap)
{
/* 	struct vnode *vp = ap->a_vp;*/
	struct flock *fl = ap->a_fl;
/* 	caddr_t id = (caddr_t)1 */ /* ap->a_id */;
/* 	int flags = ap->a_flags; */
	off_t start, end, size;
	int error/* , lkop */;

	/*KAN: temp */
	return (EOPNOTSUPP);

	size = 0;
	error = 0;
	switch (fl->l_whence) {
	    case SEEK_SET:
	    case SEEK_CUR:
		start = fl->l_start;
		break;
	    case SEEK_END:
		start = fl->l_start + size;
	    default:
		return (EINVAL);
	}
	if (start < 0)
		return (EINVAL);
	if (fl->l_len == 0)
		end = -1;
	else {
		end = start + fl->l_len - 1;
		if (end < start)
			return (EINVAL);
	}
#ifdef notyet
	switch (ap->a_op) {
	    case F_SETLK:
		error = lf_advlock(ap, &np->n_lockf, size);
		break;
	    case F_UNLCK:
		lf_advlock(ap, &np->n_lockf, size);
		break;
	    case F_GETLK:
		error = lf_advlock(ap, &np->n_lockf, size);
		break;
	    default:
		return (EINVAL);
	}
#endif
	return (error);
}

static int
_xfs_cachedlookup(
	struct vop_cachedlookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap)
{
	struct vnode *dvp, *tvp;
	struct xfs_vnode *cvp;
	int islastcn;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	struct thread *td = cnp->cn_thread;

	char *pname = cnp->cn_nameptr;
	int namelen = cnp->cn_namelen;

	*vpp = NULL;
	dvp = ap->a_dvp;
	islastcn = flags & ISLASTCN;

	XVOP_LOOKUP(VPTOXFSVP(dvp), cnp, &cvp, 0, NULL, cred, error);

	if (error == ENOENT) {
		if ((nameiop == CREATE || nameiop == RENAME ||
		     nameiop == DELETE) && islastcn)
		{
			error = VOP_ACCESS(dvp, VWRITE, cred, td);
			if (error)
				return (error);
			cnp->cn_flags |= SAVENAME;
			return (EJUSTRETURN);
		}
		if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
			cache_enter(dvp, *vpp, cnp);
		return (error);
	}
	if (error)
		return (error);

	tvp = cvp->v_vnode;

	if (nameiop == DELETE && islastcn) {
		if ((error = vn_lock(tvp, LK_EXCLUSIVE, td))) {
			vrele(tvp);
			goto err_out;
		}
		*vpp = tvp;

		/* Directory should be writable for deletes. */
	        error = VOP_ACCESS(dvp, VWRITE, cred, td);
         	if (error)
		 	goto err_out;

		/* XXXKAN: Permission checks for sticky dirs? */
		return (0);
	 }

	if (nameiop == RENAME && islastcn) {
		if ((error = vn_lock(tvp, LK_EXCLUSIVE, td))) {
			vrele(tvp);
			goto err_out;
		}
		*vpp = tvp;

		if ((error = VOP_ACCESS(dvp, VWRITE, cred, td)))
			goto err_out;
		return (0);
	}

	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, td);
		error = vn_lock(tvp, cnp->cn_lkflags, td);
		if (error) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
			vrele(tvp);
			goto err_out;
		}
		*vpp = tvp;
	} else if (namelen == 1 && pname[0] == '.') {
		*vpp = tvp;
		KASSERT(tvp == dvp, ("not same directory"));
	} else {
		if ((error = vn_lock(tvp, cnp->cn_lkflags, td))) {
			vrele(tvp);
			goto err_out;
		}
		*vpp = tvp;
	}

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *vpp, cnp);
	return (0);

err_out:
	if (*vpp != 0)
		vput(*vpp);
	return (error);
}

static int
_xfs_reclaim(
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread  *a_td;
	} */ *ap)
{

	struct vnode *vp = ap->a_vp;
	struct xfs_vnode *xfs_vp = VPTOXFSVP(vp);
	int error;

	XVOP_RECLAIM(xfs_vp, error);
	kmem_free(xfs_vp, sizeof(*xfs_vp));
	vp->v_data = NULL;
	return (error);
}

static int
_xfs_kqfilter(
	struct vop_kqfilter_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap)
{
	return (0);
}

static __inline
struct xfs_inode *
xfs_vtoi(struct vnode *vp)
{
	if (VPTOXFSVP(vp) != 0)
		return (XFS_BHVTOI(VPTOXFSVP(vp)->v_fbhv));
	return (NULL);
}

/*
 * Read wrapper for fifos.
 */
static int
_xfsfifo_read(
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	int error, resid;
	struct xfs_inode *ip;
	struct uio *uio;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = fifo_specops.vop_read(ap);
	ip = xfs_vtoi(ap->a_vp);
	if ((ap->a_vp->v_mount->mnt_flag & MNT_NOATIME) == 0 && ip != NULL &&
	    (uio->uio_resid != resid || (error == 0 && resid != 0)))
		xfs_ichgtime(ip, XFS_ICHGTIME_ACC);
	return (error);
}

/*
 * Write wrapper for fifos.
 */
static int
_xfsfifo_write(
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	int error, resid;
	struct uio *uio;
	struct xfs_inode *ip;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = fifo_specops.vop_write(ap);
	ip = xfs_vtoi(ap->a_vp);
	if (ip != NULL && (uio->uio_resid != resid ||
	    (error == 0 && resid != 0)))
		xfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	return (error);
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the inode then do device close.
 */
static int
_xfsfifo_close(
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{

	return (fifo_specops.vop_close(ap));
}

/*
 * Kqfilter wrapper for fifos.
 *
 * Fall through to ufs kqfilter routines if needed
 */
static int
_xfsfifo_kqfilter(
	struct vop_kqfilter_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap)
{
	int error;

	error = fifo_specops.vop_kqfilter(ap);
	if (error)
		error = _xfs_kqfilter(ap);
	return (error);
}

static int
_xfs_getextattr(
	struct vop_getextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	int error;
	char *value;
	int size;

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
        if (error)
		return (error);

	size = ATTR_MAX_VALUELEN;
	value = (char *)kmem_zalloc(size, KM_SLEEP);
	if (value == NULL)
		return (ENOMEM);

	XVOP_ATTR_GET(VPTOXFSVP(ap->a_vp), ap->a_name, value, &size, 1,
	    ap->a_cred, error);

	if (ap->a_uio != NULL) {
		if (ap->a_uio->uio_iov->iov_len < size)
			error = ERANGE;
		else
			uiomove(value, size, ap->a_uio);
	}

	if (ap->a_size != NULL)
		*ap->a_size = size;

	kmem_free(value, ATTR_MAX_VALUELEN);
	return (error);
}		

static int
_xfs_listextattr(
	struct vop_listextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		struct uio *a_uio;
		size_t *a_size;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	int error;
	char *buf = NULL;
	int buf_len = 0;
	attrlist_cursor_kern_t  cursor = { 0 };
	int i;
	char name_len;
	int attrnames_len = 0;
	int xfs_flags = ATTR_KERNAMELS;

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
        if (error)
		return (error);

	if (ap->a_attrnamespace & EXTATTR_NAMESPACE_USER)
		xfs_flags |= ATTR_KERNORMALS;

	if (ap->a_attrnamespace & EXTATTR_NAMESPACE_SYSTEM)
		xfs_flags |= ATTR_KERNROOTLS;

	if (ap->a_uio == NULL || ap->a_uio->uio_iov[0].iov_base == NULL) {
		xfs_flags |= ATTR_KERNOVAL;
		buf_len = 0;
	} else {
		buf = ap->a_uio->uio_iov[0].iov_base;
		buf_len = ap->a_uio->uio_iov[0].iov_len;
	}

	XVOP_ATTR_LIST(VPTOXFSVP(ap->a_vp), buf, buf_len, xfs_flags,
		    &cursor, ap->a_cred, error);
	if (error < 0) {
		attrnames_len = -error;
		error = 0;
	}
	if (buf == NULL)
		goto done;

	/*
	 * extattr_list expects a list of names.  Each list
	 * entry consists of one byte for the name length, followed
	 * by the name (not null terminated)
	 */
	name_len=0;
	for(i=attrnames_len-1; i > 0 ; --i) {
		buf[i] = buf[i-1];
		if (buf[i])
			++name_len;
		else {
			buf[i] = name_len;
			name_len = 0;
		}
	} 
	buf[0] = name_len;

	if (ap->a_uio != NULL)
		ap->a_uio->uio_resid -= attrnames_len;

done:
	if (ap->a_size != NULL)
		*ap->a_size = attrnames_len;

	return (error);
}		
