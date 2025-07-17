/*-
 * SPDX-License-Identifier: (BSD-2-Clause AND BSD-3-Clause)
 *
 * Copyright (c) 2002, 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
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
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: $FreeBSD: .../ufs/ufs_readwrite.c,v 1.96 2002/08/12 09:22:11 phk ...
 */

#include <sys/cdefs.h>
#include "opt_directio.h"
#include "opt_ffs.h"
#include "opt_ufs.h"

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/extattr.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/rwlock.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dir.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#define	ALIGNED_TO(ptr, s)	\
	(((uintptr_t)(ptr) & (_Alignof(s) - 1)) == 0)

#ifdef DIRECTIO
extern int	ffs_rawread(struct vnode *vp, struct uio *uio, int *workdone);
#endif
static vop_fdatasync_t	ffs_fdatasync;
static vop_fsync_t	ffs_fsync;
static vop_getpages_t	ffs_getpages;
static vop_getpages_async_t	ffs_getpages_async;
static vop_lock1_t	ffs_lock;
#ifdef INVARIANTS
static vop_unlock_t	ffs_unlock_debug;
#endif
static vop_read_t	ffs_read;
static vop_write_t	ffs_write;
static int	ffs_extread(struct vnode *vp, struct uio *uio, int ioflag);
static int	ffs_extwrite(struct vnode *vp, struct uio *uio, int ioflag,
		    struct ucred *cred);
static vop_strategy_t	ffsext_strategy;
static vop_closeextattr_t	ffs_closeextattr;
static vop_deleteextattr_t	ffs_deleteextattr;
static vop_getextattr_t	ffs_getextattr;
static vop_listextattr_t	ffs_listextattr;
static vop_openextattr_t	ffs_openextattr;
static vop_setextattr_t	ffs_setextattr;
static vop_vptofh_t	ffs_vptofh;
static vop_vput_pair_t	ffs_vput_pair;

vop_fplookup_vexec_t ufs_fplookup_vexec;

/* Global vfs data structures for ufs. */
struct vop_vector ffs_vnodeops1 = {
	.vop_default =		&ufs_vnodeops,
	.vop_fsync =		ffs_fsync,
	.vop_fdatasync =	ffs_fdatasync,
	.vop_getpages =		ffs_getpages,
	.vop_getpages_async =	ffs_getpages_async,
	.vop_lock1 =		ffs_lock,
#ifdef INVARIANTS
	.vop_unlock =		ffs_unlock_debug,
#endif
	.vop_read =		ffs_read,
	.vop_reallocblks =	ffs_reallocblks,
	.vop_write =		ffs_write,
	.vop_vptofh =		ffs_vptofh,
	.vop_vput_pair =	ffs_vput_pair,
	.vop_fplookup_vexec =	ufs_fplookup_vexec,
	.vop_fplookup_symlink =	VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(ffs_vnodeops1);

struct vop_vector ffs_fifoops1 = {
	.vop_default =		&ufs_fifoops,
	.vop_fsync =		ffs_fsync,
	.vop_fdatasync =	ffs_fdatasync,
	.vop_lock1 =		ffs_lock,
#ifdef INVARIANTS
	.vop_unlock =		ffs_unlock_debug,
#endif
	.vop_vptofh =		ffs_vptofh,
	.vop_fplookup_vexec =   VOP_EAGAIN,
	.vop_fplookup_symlink = VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(ffs_fifoops1);

/* Global vfs data structures for ufs. */
struct vop_vector ffs_vnodeops2 = {
	.vop_default =		&ufs_vnodeops,
	.vop_fsync =		ffs_fsync,
	.vop_fdatasync =	ffs_fdatasync,
	.vop_getpages =		ffs_getpages,
	.vop_getpages_async =	ffs_getpages_async,
	.vop_lock1 =		ffs_lock,
#ifdef INVARIANTS
	.vop_unlock =		ffs_unlock_debug,
#endif
	.vop_read =		ffs_read,
	.vop_reallocblks =	ffs_reallocblks,
	.vop_write =		ffs_write,
	.vop_closeextattr =	ffs_closeextattr,
	.vop_deleteextattr =	ffs_deleteextattr,
	.vop_getextattr =	ffs_getextattr,
	.vop_listextattr =	ffs_listextattr,
	.vop_openextattr =	ffs_openextattr,
	.vop_setextattr =	ffs_setextattr,
	.vop_vptofh =		ffs_vptofh,
	.vop_vput_pair =	ffs_vput_pair,
	.vop_fplookup_vexec =	ufs_fplookup_vexec,
	.vop_fplookup_symlink =	VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(ffs_vnodeops2);

struct vop_vector ffs_fifoops2 = {
	.vop_default =		&ufs_fifoops,
	.vop_fsync =		ffs_fsync,
	.vop_fdatasync =	ffs_fdatasync,
	.vop_lock1 =		ffs_lock,
#ifdef INVARIANTS
	.vop_unlock =		ffs_unlock_debug,
#endif
	.vop_reallocblks =	ffs_reallocblks,
	.vop_strategy =		ffsext_strategy,
	.vop_closeextattr =	ffs_closeextattr,
	.vop_deleteextattr =	ffs_deleteextattr,
	.vop_getextattr =	ffs_getextattr,
	.vop_listextattr =	ffs_listextattr,
	.vop_openextattr =	ffs_openextattr,
	.vop_setextattr =	ffs_setextattr,
	.vop_vptofh =		ffs_vptofh,
	.vop_fplookup_vexec =   VOP_EAGAIN,
	.vop_fplookup_symlink = VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(ffs_fifoops2);

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ffs_fsync(struct vop_fsync_args *ap)
{
	struct vnode *vp;
	struct bufobj *bo;
	int error;

	vp = ap->a_vp;
	bo = &vp->v_bufobj;
retry:
	error = ffs_syncvnode(vp, ap->a_waitfor, 0);
	if (error)
		return (error);
	if (ap->a_waitfor == MNT_WAIT && DOINGSOFTDEP(vp)) {
		error = softdep_fsync(vp);
		if (error)
			return (error);

		/*
		 * The softdep_fsync() function may drop vp lock,
		 * allowing for dirty buffers to reappear on the
		 * bo_dirty list. Recheck and resync as needed.
		 */
		BO_LOCK(bo);
		if ((vp->v_type == VREG || vp->v_type == VDIR) &&
		    (bo->bo_numoutput > 0 || bo->bo_dirty.bv_cnt > 0)) {
			BO_UNLOCK(bo);
			goto retry;
		}
		BO_UNLOCK(bo);
	}
	if (ffs_fsfail_cleanup(VFSTOUFS(vp->v_mount), 0))
		return (ENXIO);
	return (0);
}

int
ffs_syncvnode(struct vnode *vp, int waitfor, int flags)
{
	struct inode *ip;
	struct bufobj *bo;
	struct ufsmount *ump;
	struct buf *bp, *nbp;
	ufs_lbn_t lbn;
	int error, passes, wflag;
	bool still_dirty, unlocked, wait;

	ip = VTOI(vp);
	bo = &vp->v_bufobj;
	ump = VFSTOUFS(vp->v_mount);
#ifdef WITNESS
	wflag = IS_SNAPSHOT(ip) ? LK_NOWITNESS : 0;
#else
	wflag = 0;
#endif

	/*
	 * When doing MNT_WAIT we must first flush all dependencies
	 * on the inode.
	 */
	if (DOINGSOFTDEP(vp) && waitfor == MNT_WAIT &&
	    (error = softdep_sync_metadata(vp)) != 0) {
		if (ffs_fsfail_cleanup(ump, error))
			error = 0;
		return (error);
	}

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	error = 0;
	passes = 0;
	wait = false;	/* Always do an async pass first. */
	unlocked = false;
	lbn = lblkno(ITOFS(ip), (ip->i_size + ITOFS(ip)->fs_bsize - 1));
	BO_LOCK(bo);
loop:
	TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs)
		bp->b_vflags &= ~BV_SCANNED;
	TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
		/*
		 * Reasons to skip this buffer: it has already been considered
		 * on this pass, the buffer has dependencies that will cause
		 * it to be redirtied and it has not already been deferred,
		 * or it is already being written.
		 */
		if ((bp->b_vflags & BV_SCANNED) != 0)
			continue;
		bp->b_vflags |= BV_SCANNED;
		/*
		 * Flush indirects in order, if requested.
		 *
		 * Note that if only datasync is requested, we can
		 * skip indirect blocks when softupdates are not
		 * active.  Otherwise we must flush them with data,
		 * since dependencies prevent data block writes.
		 */
		if (waitfor == MNT_WAIT && bp->b_lblkno <= -UFS_NDADDR &&
		    (lbn_level(bp->b_lblkno) >= passes ||
		    ((flags & DATA_ONLY) != 0 && !DOINGSOFTDEP(vp))))
			continue;
		if (bp->b_lblkno > lbn)
			panic("ffs_syncvnode: syncing truncated data.");
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL) == 0) {
			BO_UNLOCK(bo);
		} else if (wait) {
			if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL |
			    LK_INTERLOCK | wflag, BO_LOCKPTR(bo)) != 0) {
				BO_LOCK(bo);
				bp->b_vflags &= ~BV_SCANNED;
				goto next_locked;
			}
		} else
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ffs_fsync: not dirty");
		/*
		 * Check for dependencies and potentially complete them.
		 */
		if (!LIST_EMPTY(&bp->b_dep) &&
		    (error = softdep_sync_buf(vp, bp,
		    wait ? MNT_WAIT : MNT_NOWAIT)) != 0) {
			/*
			 * Lock order conflict, buffer was already unlocked,
			 * and vnode possibly unlocked.
			 */
			if (error == ERELOOKUP) {
				if (vp->v_data == NULL)
					return (EBADF);
				unlocked = true;
				if (DOINGSOFTDEP(vp) && waitfor == MNT_WAIT &&
				    (error = softdep_sync_metadata(vp)) != 0) {
					if (ffs_fsfail_cleanup(ump, error))
						error = 0;
					return (unlocked && error == 0 ?
					    ERELOOKUP : error);
				}
				/* Re-evaluate inode size */
				lbn = lblkno(ITOFS(ip), (ip->i_size +
				    ITOFS(ip)->fs_bsize - 1));
				goto next;
			}
			/* I/O error. */
			if (error != EBUSY) {
				BUF_UNLOCK(bp);
				return (error);
			}
			/* If we deferred once, don't defer again. */
		    	if ((bp->b_flags & B_DEFERRED) == 0) {
				bp->b_flags |= B_DEFERRED;
				BUF_UNLOCK(bp);
				goto next;
			}
		}
		if (wait) {
			bremfree(bp);
			error = bwrite(bp);
			if (ffs_fsfail_cleanup(ump, error))
				error = 0;
			if (error != 0)
				return (error);
		} else if ((bp->b_flags & B_CLUSTEROK)) {
			(void) vfs_bio_awrite(bp);
		} else {
			bremfree(bp);
			(void) bawrite(bp);
		}
next:
		/*
		 * Since we may have slept during the I/O, we need
		 * to start from a known point.
		 */
		BO_LOCK(bo);
next_locked:
		nbp = TAILQ_FIRST(&bo->bo_dirty.bv_hd);
	}
	if (waitfor != MNT_WAIT) {
		BO_UNLOCK(bo);
		if ((flags & NO_INO_UPDT) != 0)
			return (unlocked ? ERELOOKUP : 0);
		error = ffs_update(vp, 0);
		if (error == 0 && unlocked)
			error = ERELOOKUP;
		return (error);
	}
	/* Drain IO to see if we're done. */
	bufobj_wwait(bo, 0, 0);
	/*
	 * Block devices associated with filesystems may have new I/O
	 * requests posted for them even if the vnode is locked, so no
	 * amount of trying will get them clean.  We make several passes
	 * as a best effort.
	 *
	 * Regular files may need multiple passes to flush all dependency
	 * work as it is possible that we must write once per indirect
	 * level, once for the leaf, and once for the inode and each of
	 * these will be done with one sync and one async pass.
	 */
	if (bo->bo_dirty.bv_cnt > 0) {
		if ((flags & DATA_ONLY) == 0) {
			still_dirty = true;
		} else {
			/*
			 * For data-only sync, dirty indirect buffers
			 * are ignored.
			 */
			still_dirty = false;
			TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs) {
				if (bp->b_lblkno > -UFS_NDADDR) {
					still_dirty = true;
					break;
				}
			}
		}

		if (still_dirty) {
			/* Write the inode after sync passes to flush deps. */
			if (wait && DOINGSOFTDEP(vp) &&
			    (flags & NO_INO_UPDT) == 0) {
				BO_UNLOCK(bo);
				ffs_update(vp, 1);
				BO_LOCK(bo);
			}
			/* switch between sync/async. */
			wait = !wait;
			if (wait || ++passes < UFS_NIADDR + 2)
				goto loop;
		}
	}
	BO_UNLOCK(bo);
	error = 0;
	if ((flags & DATA_ONLY) == 0) {
		if ((flags & NO_INO_UPDT) == 0)
			error = ffs_update(vp, 1);
		if (DOINGSUJ(vp))
			softdep_journal_fsync(VTOI(vp));
	} else if ((ip->i_flags & (IN_SIZEMOD | IN_IBLKDATA)) != 0) {
		error = ffs_update(vp, 1);
	}
	if (error == 0 && unlocked)
		error = ERELOOKUP;
	if (error == 0)
		ip->i_flag &= ~IN_NEEDSYNC;
	return (error);
}

static int
ffs_fdatasync(struct vop_fdatasync_args *ap)
{

	return (ffs_syncvnode(ap->a_vp, MNT_WAIT, DATA_ONLY));
}

static int
ffs_lock(
	struct vop_lock1_args /* {
		struct vnode *a_vp;
		int a_flags;
		char *file;
		int line;
	} */ *ap)
{
#if !defined(NO_FFS_SNAPSHOT) || defined(DIAGNOSTIC)
	struct vnode *vp = ap->a_vp;
#endif	/* !NO_FFS_SNAPSHOT || DIAGNOSTIC */
#ifdef DIAGNOSTIC
	struct inode *ip;
#endif	/* DIAGNOSTIC */
	int result;
#ifndef NO_FFS_SNAPSHOT
	int flags;
	struct lock *lkp;

	/*
	 * Adaptive spinning mixed with SU leads to trouble. use a giant hammer
	 * and only use it when LK_NODDLKTREAT is set. Currently this means it
	 * is only used during path lookup.
	 */
	if ((ap->a_flags & LK_NODDLKTREAT) != 0)
		ap->a_flags |= LK_ADAPTIVE;
	switch (ap->a_flags & LK_TYPE_MASK) {
	case LK_SHARED:
	case LK_UPGRADE:
	case LK_EXCLUSIVE:
		flags = ap->a_flags;
		for (;;) {
#ifdef DEBUG_VFS_LOCKS
			VNPASS(vp->v_holdcnt != 0, vp);
#endif	/* DEBUG_VFS_LOCKS */
			lkp = vp->v_vnlock;
			result = lockmgr_lock_flags(lkp, flags,
			    &VI_MTX(vp)->lock_object, ap->a_file, ap->a_line);
			if (lkp == vp->v_vnlock || result != 0)
				break;
			/*
			 * Apparent success, except that the vnode
			 * mutated between snapshot file vnode and
			 * regular file vnode while this process
			 * slept.  The lock currently held is not the
			 * right lock.  Release it, and try to get the
			 * new lock.
			 */
			lockmgr_unlock(lkp);
			if ((flags & (LK_INTERLOCK | LK_NOWAIT)) ==
			    (LK_INTERLOCK | LK_NOWAIT))
				return (EBUSY);
			if ((flags & LK_TYPE_MASK) == LK_UPGRADE)
				flags = (flags & ~LK_TYPE_MASK) | LK_EXCLUSIVE;
			flags &= ~LK_INTERLOCK;
		}
#ifdef DIAGNOSTIC
		switch (ap->a_flags & LK_TYPE_MASK) {
		case LK_UPGRADE:
		case LK_EXCLUSIVE:
			if (result == 0 && vp->v_vnlock->lk_recurse == 0) {
				ip = VTOI(vp);
				if (ip != NULL)
					ip->i_lock_gen++;
			}
		}
#endif	/* DIAGNOSTIC */
		break;
	default:
#ifdef DIAGNOSTIC
		if ((ap->a_flags & LK_TYPE_MASK) == LK_DOWNGRADE) {
			ip = VTOI(vp);
			if (ip != NULL)
				ufs_unlock_tracker(ip);
		}
#endif	/* DIAGNOSTIC */
		result = VOP_LOCK1_APV(&ufs_vnodeops, ap);
		break;
	}
#else	/* NO_FFS_SNAPSHOT */
	/*
	 * See above for an explanation.
	 */
	if ((ap->a_flags & LK_NODDLKTREAT) != 0)
		ap->a_flags |= LK_ADAPTIVE;
#ifdef DIAGNOSTIC
	if ((ap->a_flags & LK_TYPE_MASK) == LK_DOWNGRADE) {
		ip = VTOI(vp);
		if (ip != NULL)
			ufs_unlock_tracker(ip);
	}
#endif	/* DIAGNOSTIC */
	result =  VOP_LOCK1_APV(&ufs_vnodeops, ap);
#endif	/* NO_FFS_SNAPSHOT */
#ifdef DIAGNOSTIC
	switch (ap->a_flags & LK_TYPE_MASK) {
	case LK_UPGRADE:
	case LK_EXCLUSIVE:
		if (result == 0 && vp->v_vnlock->lk_recurse == 0) {
			ip = VTOI(vp);
			if (ip != NULL)
				ip->i_lock_gen++;
		}
	}
#endif	/* DIAGNOSTIC */
	return (result);
}

#ifdef INVARIANTS
static int
ffs_unlock_debug(struct vop_unlock_args *ap)
{
	struct vnode *vp;
	struct inode *ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
	if (ip->i_flag & UFS_INODE_FLAG_LAZY_MASK_ASSERTABLE) {
		if ((vp->v_mflag & VMP_LAZYLIST) == 0) {
			VI_LOCK(vp);
			VNASSERT((vp->v_mflag & VMP_LAZYLIST), vp,
			    ("%s: modified vnode (%x) not on lazy list",
			    __func__, ip->i_flag));
			VI_UNLOCK(vp);
		}
	}
	KASSERT(vp->v_type != VDIR || vp->v_vnlock->lk_recurse != 0 ||
	    (ip->i_flag & IN_ENDOFF) == 0,
	    ("ufs dir vp %p ip %p flags %#x", vp, ip, ip->i_flag));
#ifdef DIAGNOSTIC
	if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE && ip != NULL &&
	    vp->v_vnlock->lk_recurse == 0)
		ufs_unlock_tracker(ip);
#endif
	return (VOP_UNLOCK_APV(&ufs_vnodeops, ap));
}
#endif

static int
ffs_read_hole(struct uio *uio, long xfersize, long *size)
{
	ssize_t saved_resid, tlen;
	int error;

	while (xfersize > 0) {
		tlen = min(xfersize, ZERO_REGION_SIZE);
		saved_resid = uio->uio_resid;
		error = vn_io_fault_uiomove(__DECONST(void *, zero_region),
		    tlen, uio);
		if (error != 0)
			return (error);
		tlen = saved_resid - uio->uio_resid;
		xfersize -= tlen;
		*size -= tlen;
	}
	return (0);
}

/*
 * Vnode op for reading.
 */
static int
ffs_read(
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	ssize_t orig_resid;
	int bflag, error, ioflag, seqcount;

	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	if (ap->a_ioflag & IO_EXT)
#ifdef notyet
		return (ffs_extread(vp, uio, ioflag));
#else
		panic("ffs_read+IO_EXT");
#endif
#ifdef DIRECTIO
	if ((ioflag & IO_DIRECT) != 0) {
		int workdone;

		error = ffs_rawread(vp, uio, &workdone);
		if (error != 0 || workdone != 0)
			return error;
	}
#endif

	seqcount = ap->a_ioflag >> IO_SEQSHIFT;
	ip = VTOI(vp);

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_READ)
		panic("ffs_read: mode");

	if (vp->v_type == VLNK) {
		if ((int)ip->i_size < VFSTOUFS(vp->v_mount)->um_maxsymlinklen)
			panic("ffs_read: short symlink");
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("ffs_read: type %d",  vp->v_type);
#endif
	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0, ("ffs_read: uio->uio_resid < 0"));
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0, ("ffs_read: uio->uio_offset < 0"));
	fs = ITOFS(ip);
	if (uio->uio_offset < ip->i_size &&
	    uio->uio_offset >= fs->fs_maxfilesize)
		return (EOVERFLOW);

	bflag = GB_UNMAPPED | (uio->uio_segflg == UIO_NOCOPY ? 0 : GB_NOSPARSE);
#ifdef WITNESS
	bflag |= IS_SNAPSHOT(ip) ? GB_NOWITNESS : 0;
#endif
	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;

		/*
		 * size of buffer.  The buffer representing the
		 * end of the file is rounded up to the size of
		 * the block type ( fragment or full block,
		 * depending ).
		 */
		size = blksize(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		/*
		 * The amount we want to transfer in this iteration is
		 * one FS block less the amount of the data before
		 * our startpoint (duh!)
		 */
		xfersize = fs->fs_bsize - blkoffset;

		/*
		 * But if we actually want less than the block,
		 * or the file doesn't have a whole block more of data,
		 * then use the lesser number.
		 */
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ip->i_size) {
			/*
			 * Don't do readahead if this is the end of the file.
			 */
			error = bread_gb(vp, lbn, size, NOCRED, bflag, &bp);
		} else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			/*
			 * Otherwise if we are allowed to cluster,
			 * grab as much as we can.
			 *
			 * XXX  This may not be a win if we are not
			 * doing sequential access.
			 */
			error = cluster_read(vp, ip->i_size, lbn,
			    size, NOCRED, blkoffset + uio->uio_resid,
			    seqcount, bflag, &bp);
		} else if (seqcount > 1) {
			/*
			 * If we are NOT allowed to cluster, then
			 * if we appear to be acting sequentially,
			 * fire off a request for a readahead
			 * as well as a read. Note that the 4th and 5th
			 * arguments point to arrays of the size specified in
			 * the 6th argument.
			 */
			int nextsize = blksize(fs, ip, nextlbn);
			error = breadn_flags(vp, lbn, lbn, size, &nextlbn,
			    &nextsize, 1, NOCRED, bflag, NULL, &bp);
		} else {
			/*
			 * Failing all of the above, just read what the
			 * user asked for. Interestingly, the same as
			 * the first option above.
			 */
			error = bread_gb(vp, lbn, size, NOCRED, bflag, &bp);
		}
		if (error == EJUSTRETURN) {
			error = ffs_read_hole(uio, xfersize, &size);
			if (error == 0)
				continue;
		}
		if (error != 0) {
			brelse(bp);
			bp = NULL;
			break;
		}

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

		if (buf_mapped(bp)) {
			error = vn_io_fault_uiomove((char *)bp->b_data +
			    blkoffset, (int)xfersize, uio);
		} else {
			error = vn_io_fault_pgmove(bp->b_pages,
			    blkoffset + (bp->b_offset & PAGE_MASK),
			    (int)xfersize, uio);
		}
		if (error)
			break;

		vfs_bio_brelse(bp, ioflag);
	}

	/*
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL)
		vfs_bio_brelse(bp, ioflag);

	if ((error == 0 || uio->uio_resid != orig_resid) &&
	    (vp->v_mount->mnt_flag & (MNT_NOATIME | MNT_RDONLY)) == 0)
		UFS_INODE_SET_FLAG_SHARED(ip, IN_ACCESS);
	return (error);
}

/*
 * Vnode op for writing.
 */
static int
ffs_write(
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp;
	struct uio *uio;
	struct inode *ip;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn;
	off_t osize;
	ssize_t resid, r;
	int seqcount;
	int blkoffset, error, flags, ioflag, size, xfersize;

	vp = ap->a_vp;
	if (DOINGSUJ(vp))
		softdep_prealloc(vp, MNT_WAIT);
	if (vp->v_data == NULL)
		return (EBADF);

	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	if (ap->a_ioflag & IO_EXT)
#ifdef notyet
		return (ffs_extwrite(vp, uio, ioflag, ap->a_cred));
#else
		panic("ffs_write+IO_EXT");
#endif

	seqcount = ap->a_ioflag >> IO_SEQSHIFT;
	ip = VTOI(vp);

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_WRITE)
		panic("ffs_write: mode");
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		panic("ffs_write: dir write");
		break;
	default:
		panic("ffs_write: type %p %d (%d,%d)", vp, (int)vp->v_type,
			(int)uio->uio_offset,
			(int)uio->uio_resid
		);
	}

	KASSERT(uio->uio_resid >= 0, ("ffs_write: uio->uio_resid < 0"));
	KASSERT(uio->uio_offset >= 0, ("ffs_write: uio->uio_offset < 0"));
	fs = ITOFS(ip);

	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	error = vn_rlimit_fsizex(vp, uio, fs->fs_maxfilesize, &r,
	    uio->uio_td);
	if (error != 0) {
		vn_rlimit_fsizex_res(uio, r);
		return (error);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	if (seqcount > BA_SEQMAX)
		flags = BA_SEQMAX << BA_SEQSHIFT;
	else
		flags = seqcount << BA_SEQSHIFT;
	if (ioflag & IO_SYNC)
		flags |= IO_SYNC;
	flags |= BA_UNMAPPED;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (uio->uio_offset + xfersize > ip->i_size)
			vnode_pager_setsize(vp, uio->uio_offset + xfersize);

		/*
		 * We must perform a read-before-write if the transfer size
		 * does not cover the entire buffer.
		 */
		if (fs->fs_bsize > xfersize)
			flags |= BA_CLRBUF;
		else
			flags &= ~BA_CLRBUF;
/* XXX is uio->uio_offset the right thing here? */
		error = UFS_BALLOC(vp, uio->uio_offset, xfersize,
		    ap->a_cred, flags, &bp);
		if (error != 0) {
			vnode_pager_setsize(vp, ip->i_size);
			break;
		}
		if ((ioflag & (IO_SYNC|IO_INVAL)) == (IO_SYNC|IO_INVAL))
			bp->b_flags |= B_NOCACHE;

		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
			DIP_SET(ip, i_size, ip->i_size);
			UFS_INODE_SET_FLAG(ip, IN_SIZEMOD | IN_CHANGE);
		}

		size = blksize(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		if (buf_mapped(bp)) {
			error = vn_io_fault_uiomove((char *)bp->b_data +
			    blkoffset, (int)xfersize, uio);
		} else {
			error = vn_io_fault_pgmove(bp->b_pages,
			    blkoffset + (bp->b_offset & PAGE_MASK),
			    (int)xfersize, uio);
		}
		/*
		 * If the buffer is not already filled and we encounter an
		 * error while trying to fill it, we have to clear out any
		 * garbage data from the pages instantiated for the buffer.
		 * If we do not, a failed uiomove() during a write can leave
		 * the prior contents of the pages exposed to a userland mmap.
		 *
		 * Note that we need only clear buffers with a transfer size
		 * equal to the block size because buffers with a shorter
		 * transfer size were cleared above by the call to UFS_BALLOC()
		 * with the BA_CLRBUF flag set.
		 *
		 * If the source region for uiomove identically mmaps the
		 * buffer, uiomove() performed the NOP copy, and the buffer
		 * content remains valid because the page fault handler
		 * validated the pages.
		 */
		if (error != 0 && (bp->b_flags & B_CACHE) == 0 &&
		    fs->fs_bsize == xfersize) {
			if (error == EFAULT && LIST_EMPTY(&bp->b_dep)) {
				bp->b_flags |= B_INVAL | B_RELBUF | B_NOCACHE;
				brelse(bp);
				break;
			} else {
				vfs_bio_clrbuf(bp);
			}
		}

		vfs_bio_set_flags(bp, ioflag);

		/*
		 * If IO_SYNC each buffer is written synchronously.  Otherwise
		 * if we have a severe page deficiency write the buffer
		 * asynchronously.  Otherwise try to cluster, and if that
		 * doesn't do it then either do an async write (if O_DIRECT),
		 * or a delayed write (if not).
		 */
		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (vm_page_count_severe() ||
			    buf_dirty_count_severe() ||
			    (ioflag & IO_ASYNC)) {
			bp->b_flags |= B_CLUSTEROK;
			bawrite(bp);
		} else if (xfersize + blkoffset == fs->fs_bsize) {
			if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERW) == 0) {
				bp->b_flags |= B_CLUSTEROK;
				cluster_write(vp, &ip->i_clusterw, bp,
				    ip->i_size, seqcount, GB_UNMAPPED);
			} else {
				bawrite(bp);
			}
		} else if (ioflag & IO_DIRECT) {
			bp->b_flags |= B_CLUSTEROK;
			bawrite(bp);
		} else {
			bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		if (error || xfersize == 0)
			break;
		UFS_INODE_SET_FLAG(ip, IN_CHANGE | IN_UPDATE);
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if ((ip->i_mode & (ISUID | ISGID)) && resid > uio->uio_resid &&
	    ap->a_cred) {
		if (priv_check_cred(ap->a_cred, PRIV_VFS_RETAINSUGID)) {
			vn_seqc_write_begin(vp);
			UFS_INODE_SET_MODE(ip, ip->i_mode & ~(ISUID | ISGID));
			DIP_SET(ip, i_mode, ip->i_mode);
			vn_seqc_write_end(vp);
		}
	}
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)ffs_truncate(vp, osize,
			    IO_NORMAL | (ioflag & IO_SYNC), ap->a_cred);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC)) {
		if (!(ioflag & IO_DATASYNC) ||
		    (ip->i_flags & (IN_SIZEMOD | IN_IBLKDATA)))
			error = ffs_update(vp, 1);
		if (ffs_fsfail_cleanup(VFSTOUFS(vp->v_mount), error))
			error = ENXIO;
	}
	vn_rlimit_fsizex_res(uio, r);
	return (error);
}

/*
 * Extended attribute area reading.
 */
static int
ffs_extread(struct vnode *vp, struct uio *uio, int ioflag)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	ssize_t orig_resid;
	int error;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_READ || fs->fs_magic != FS_UFS2_MAGIC)
		panic("ffs_extread: mode");

#endif
	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0, ("ffs_extread: uio->uio_resid < 0"));
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0, ("ffs_extread: uio->uio_offset < 0"));

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = dp->di_extsize - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;

		/*
		 * size of buffer.  The buffer representing the
		 * end of the file is rounded up to the size of
		 * the block type ( fragment or full block,
		 * depending ).
		 */
		size = sblksize(fs, dp->di_extsize, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		/*
		 * The amount we want to transfer in this iteration is
		 * one FS block less the amount of the data before
		 * our startpoint (duh!)
		 */
		xfersize = fs->fs_bsize - blkoffset;

		/*
		 * But if we actually want less than the block,
		 * or the file doesn't have a whole block more of data,
		 * then use the lesser number.
		 */
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= dp->di_extsize) {
			/*
			 * Don't do readahead if this is the end of the info.
			 */
			error = bread(vp, -1 - lbn, size, NOCRED, &bp);
		} else {
			/*
			 * If we have a second block, then
			 * fire off a request for a readahead
			 * as well as a read. Note that the 4th and 5th
			 * arguments point to arrays of the size specified in
			 * the 6th argument.
			 */
			int nextsize = sblksize(fs, dp->di_extsize, nextlbn);
			nextlbn = -1 - nextlbn;
			error = breadn(vp, -1 - lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		}
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}

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

		error = uiomove((char *)bp->b_data + blkoffset,
					(int)xfersize, uio);
		if (error)
			break;
		vfs_bio_brelse(bp, ioflag);
	}

	/*
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL)
		vfs_bio_brelse(bp, ioflag);
	return (error);
}

/*
 * Extended attribute area writing.
 */
static int
ffs_extwrite(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *ucred)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn;
	off_t osize;
	ssize_t resid;
	int blkoffset, error, flags, size, xfersize;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_WRITE || fs->fs_magic != FS_UFS2_MAGIC)
		panic("ffs_extwrite: mode");
#endif

	if (ioflag & IO_APPEND)
		uio->uio_offset = dp->di_extsize;
	KASSERT(uio->uio_offset >= 0, ("ffs_extwrite: uio->uio_offset < 0"));
	KASSERT(uio->uio_resid >= 0, ("ffs_extwrite: uio->uio_resid < 0"));
	if ((uoff_t)uio->uio_offset + uio->uio_resid >
	    UFS_NXADDR * fs->fs_bsize)
		return (EFBIG);

	resid = uio->uio_resid;
	osize = dp->di_extsize;
	flags = IO_EXT;
	if (ioflag & IO_SYNC)
		flags |= IO_SYNC;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;

		/*
		 * We must perform a read-before-write if the transfer size
		 * does not cover the entire buffer.
		 */
		if (fs->fs_bsize > xfersize)
			flags |= BA_CLRBUF;
		else
			flags &= ~BA_CLRBUF;
		error = UFS_BALLOC(vp, uio->uio_offset, xfersize,
		    ucred, flags, &bp);
		if (error != 0)
			break;
		/*
		 * If the buffer is not valid we have to clear out any
		 * garbage data from the pages instantiated for the buffer.
		 * If we do not, a failed uiomove() during a write can leave
		 * the prior contents of the pages exposed to a userland
		 * mmap().  XXX deal with uiomove() errors a better way.
		 */
		if ((bp->b_flags & B_CACHE) == 0 && fs->fs_bsize <= xfersize)
			vfs_bio_clrbuf(bp);

		if (uio->uio_offset + xfersize > dp->di_extsize) {
			dp->di_extsize = uio->uio_offset + xfersize;
			UFS_INODE_SET_FLAG(ip, IN_SIZEMOD | IN_CHANGE);
		}

		size = sblksize(fs, dp->di_extsize, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);

		vfs_bio_set_flags(bp, ioflag);

		/*
		 * If IO_SYNC each buffer is written synchronously.  Otherwise
		 * if we have a severe page deficiency write the buffer
		 * asynchronously.  Otherwise try to cluster, and if that
		 * doesn't do it then either do an async write (if O_DIRECT),
		 * or a delayed write (if not).
		 */
		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (vm_page_count_severe() ||
			    buf_dirty_count_severe() ||
			    xfersize + blkoffset == fs->fs_bsize ||
			    (ioflag & (IO_ASYNC | IO_DIRECT)))
			bawrite(bp);
		else
			bdwrite(bp);
		if (error || xfersize == 0)
			break;
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if ((ip->i_mode & (ISUID | ISGID)) && resid > uio->uio_resid && ucred) {
		if (priv_check_cred(ucred, PRIV_VFS_RETAINSUGID)) {
			vn_seqc_write_begin(vp);
			UFS_INODE_SET_MODE(ip, ip->i_mode & ~(ISUID | ISGID));
			dp->di_mode = ip->i_mode;
			vn_seqc_write_end(vp);
		}
	}
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)ffs_truncate(vp, osize,
			    IO_EXT | (ioflag&IO_SYNC), ucred);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = ffs_update(vp, 1);
	return (error);
}

/*
 * Vnode operating to retrieve a named extended attribute.
 *
 * Locate a particular EA (nspace:name) in the area (ptr:length), and return
 * the length of the EA, and possibly the pointer to the entry and to the data.
 */
static int
ffs_findextattr(uint8_t *ptr, uint64_t length, int nspace, const char *name,
    struct extattr **eapp, uint8_t **eac)
{
	struct extattr *eap, *eaend;
	size_t nlen;

	nlen = strlen(name);
	KASSERT(ALIGNED_TO(ptr, struct extattr), ("unaligned"));
	eap = (struct extattr *)ptr;
	eaend = (struct extattr *)(ptr + length);
	for (; eap < eaend; eap = EXTATTR_NEXT(eap)) {
		KASSERT(EXTATTR_NEXT(eap) <= eaend,
		    ("extattr next %p beyond %p", EXTATTR_NEXT(eap), eaend));
		if (eap->ea_namespace != nspace || eap->ea_namelength != nlen
		    || memcmp(eap->ea_name, name, nlen) != 0)
			continue;
		if (eapp != NULL)
			*eapp = eap;
		if (eac != NULL)
			*eac = EXTATTR_CONTENT(eap);
		return (EXTATTR_CONTENT_SIZE(eap));
	}
	return (-1);
}

static int
ffs_rdextattr(uint8_t **p, struct vnode *vp, struct thread *td)
{
	const struct extattr *eap, *eaend, *eapnext;
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct uio luio;
	struct iovec liovec;
	uint64_t easize;
	int error;
	uint8_t *eae;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;
	easize = dp->di_extsize;
	if ((uoff_t)easize > UFS_NXADDR * fs->fs_bsize)
		return (EFBIG);

	eae = malloc(easize, M_TEMP, M_WAITOK);

	liovec.iov_base = eae;
	liovec.iov_len = easize;
	luio.uio_iov = &liovec;
	luio.uio_iovcnt = 1;
	luio.uio_offset = 0;
	luio.uio_resid = easize;
	luio.uio_segflg = UIO_SYSSPACE;
	luio.uio_rw = UIO_READ;
	luio.uio_td = td;

	error = ffs_extread(vp, &luio, IO_EXT | IO_SYNC);
	if (error) {
		free(eae, M_TEMP);
		return (error);
	}
	/* Validate disk xattrfile contents. */
	for (eap = (void *)eae, eaend = (void *)(eae + easize); eap < eaend;
	    eap = eapnext) {
		/* Detect zeroed out tail */
		if (eap->ea_length < sizeof(*eap) || eap->ea_length == 0) {
			easize = (const uint8_t *)eap - eae;
			break;
		}
			
		eapnext = EXTATTR_NEXT(eap);
		/* Bogusly long entry. */
		if (eapnext > eaend) {
			free(eae, M_TEMP);
			return (EINTEGRITY);
		}
	}
	ip->i_ea_len = easize;
	*p = eae;
	return (0);
}

static void
ffs_lock_ea(struct vnode *vp)
{
	struct inode *ip;

	ip = VTOI(vp);
	VI_LOCK(vp);
	while (ip->i_flag & IN_EA_LOCKED) {
		UFS_INODE_SET_FLAG(ip, IN_EA_LOCKWAIT);
		msleep(&ip->i_ea_refs, &vp->v_interlock, PINOD, "ufs_ea", 0);
	}
	UFS_INODE_SET_FLAG(ip, IN_EA_LOCKED);
	VI_UNLOCK(vp);
}

static void
ffs_unlock_ea(struct vnode *vp)
{
	struct inode *ip;

	ip = VTOI(vp);
	VI_LOCK(vp);
	if (ip->i_flag & IN_EA_LOCKWAIT)
		wakeup(&ip->i_ea_refs);
	ip->i_flag &= ~(IN_EA_LOCKED | IN_EA_LOCKWAIT);
	VI_UNLOCK(vp);
}

static int
ffs_open_ea(struct vnode *vp, struct ucred *cred, struct thread *td)
{
	struct inode *ip;
	int error;

	ip = VTOI(vp);

	ffs_lock_ea(vp);
	if (ip->i_ea_area != NULL) {
		ip->i_ea_refs++;
		ffs_unlock_ea(vp);
		return (0);
	}
	error = ffs_rdextattr(&ip->i_ea_area, vp, td);
	if (error) {
		ffs_unlock_ea(vp);
		return (error);
	}
	ip->i_ea_error = 0;
	ip->i_ea_refs++;
	ffs_unlock_ea(vp);
	return (0);
}

/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_close_ea(struct vnode *vp, int commit, struct ucred *cred, struct thread *td)
{
	struct inode *ip;
	struct uio luio;
	struct iovec *liovec;
	struct ufs2_dinode *dp;
	size_t ea_len, tlen;
	int error, i, lcnt;
	bool truncate;

	ip = VTOI(vp);

	ffs_lock_ea(vp);
	if (ip->i_ea_area == NULL) {
		ffs_unlock_ea(vp);
		return (EINVAL);
	}
	dp = ip->i_din2;
	error = ip->i_ea_error;
	truncate = false;
	if (commit && error == 0) {
		ASSERT_VOP_ELOCKED(vp, "ffs_close_ea commit");
		if (cred == NOCRED)
			cred =  vp->v_mount->mnt_cred;

		ea_len = MAX(ip->i_ea_len, dp->di_extsize);
		for (lcnt = 1, tlen = ea_len - ip->i_ea_len; tlen > 0;) {
			tlen -= MIN(ZERO_REGION_SIZE, tlen);
			lcnt++;
		}

		liovec = __builtin_alloca(lcnt * sizeof(struct iovec));
		luio.uio_iovcnt = lcnt;

		liovec[0].iov_base = ip->i_ea_area;
		liovec[0].iov_len = ip->i_ea_len;
		for (i = 1, tlen = ea_len - ip->i_ea_len; i < lcnt; i++) {
			liovec[i].iov_base = __DECONST(void *, zero_region);
			liovec[i].iov_len = MIN(ZERO_REGION_SIZE, tlen);
			tlen -= liovec[i].iov_len;
		}
		MPASS(tlen == 0);

		luio.uio_iov = liovec;
		luio.uio_offset = 0;
		luio.uio_resid = ea_len;
		luio.uio_segflg = UIO_SYSSPACE;
		luio.uio_rw = UIO_WRITE;
		luio.uio_td = td;
		error = ffs_extwrite(vp, &luio, IO_EXT | IO_SYNC, cred);
		if (error == 0 && ip->i_ea_len == 0)
			truncate = true;
	}
	if (--ip->i_ea_refs == 0) {
		free(ip->i_ea_area, M_TEMP);
		ip->i_ea_area = NULL;
		ip->i_ea_len = 0;
		ip->i_ea_error = 0;
	}
	ffs_unlock_ea(vp);

	if (truncate)
		ffs_truncate(vp, 0, IO_EXT, cred);
	return (error);
}

/*
 * Vnode extattr strategy routine for fifos.
 *
 * We need to check for a read or write of the external attributes.
 * Otherwise we just fall through and do the usual thing.
 */
static int
ffsext_strategy(
	struct vop_strategy_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap)
{
	struct vnode *vp;
	daddr_t lbn;

	vp = ap->a_vp;
	lbn = ap->a_bp->b_lblkno;
	if (I_IS_UFS2(VTOI(vp)) && lbn < 0 && lbn >= -UFS_NXADDR)
		return (VOP_STRATEGY_APV(&ufs_vnodeops, ap));
	if (vp->v_type == VFIFO)
		return (VOP_STRATEGY_APV(&ufs_fifoops, ap));
	panic("spec nodes went here");
}

/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_openextattr(
	struct vop_openextattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{

	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);

	return (ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td));
}

/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_closeextattr(
	struct vop_closeextattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_commit;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp;

	vp = ap->a_vp;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);
	if (ap->a_commit && (vp->v_mount->mnt_flag & MNT_RDONLY) != 0)
		return (EROFS);

	if (ap->a_commit && DOINGSUJ(vp)) {
		ASSERT_VOP_ELOCKED(vp, "ffs_closeextattr commit");
		softdep_prealloc(vp, MNT_WAIT);
		if (vp->v_data == NULL)
			return (EBADF);
	}
	return (ffs_close_ea(vp, ap->a_commit, ap->a_cred, ap->a_td));
}

/*
 * Vnode operation to remove a named attribute.
 */
static int
ffs_deleteextattr(
	struct vop_deleteextattr_args /* {
		IN struct vnode *a_vp;
		IN int a_attrnamespace;
		IN const char *a_name;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct extattr *eap;
	uint32_t ul;
	int olen, error, i, easize;
	uint8_t *eae;
	void *tmp;

	vp = ap->a_vp;
	ip = VTOI(vp);

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);
	if (strlen(ap->a_name) == 0)
		return (EINVAL);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	error = extattr_check_cred(vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VWRITE);
	if (error) {
		/*
		 * ffs_lock_ea is not needed there, because the vnode
		 * must be exclusively locked.
		 */
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	if (DOINGSUJ(vp)) {
		ASSERT_VOP_ELOCKED(vp, "ffs_deleteextattr");
		softdep_prealloc(vp, MNT_WAIT);
		if (vp->v_data == NULL)
			return (EBADF);
	}

	error = ffs_open_ea(vp, ap->a_cred, ap->a_td);
	if (error)
		return (error);

	/* CEM: delete could be done in-place instead */
	eae = malloc(ip->i_ea_len, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    &eap, NULL);
	if (olen == -1) {
		/* delete but nonexistent */
		free(eae, M_TEMP);
		ffs_close_ea(vp, 0, ap->a_cred, ap->a_td);
		return (ENOATTR);
	}
	ul = eap->ea_length;
	i = (uint8_t *)EXTATTR_NEXT(eap) - eae;
	bcopy(EXTATTR_NEXT(eap), eap, easize - i);
	easize -= ul;

	tmp = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(tmp, M_TEMP);
	error = ffs_close_ea(vp, 1, ap->a_cred, ap->a_td);
	return (error);
}

/*
 * Vnode operation to retrieve a named extended attribute.
 */
static int
ffs_getextattr(
	struct vop_getextattr_args /* {
		IN struct vnode *a_vp;
		IN int a_attrnamespace;
		IN const char *a_name;
		INOUT struct uio *a_uio;
		OUT size_t *a_size;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{
	struct inode *ip;
	uint8_t *eae, *p;
	unsigned easize;
	int error, ealen;

	ip = VTOI(ap->a_vp);

	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
	if (error)
		return (error);

	error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
	if (error)
		return (error);

	eae = ip->i_ea_area;
	easize = ip->i_ea_len;

	ealen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    NULL, &p);
	if (ealen >= 0) {
		error = 0;
		if (ap->a_size != NULL)
			*ap->a_size = ealen;
		else if (ap->a_uio != NULL)
			error = uiomove(p, ealen, ap->a_uio);
	} else
		error = ENOATTR;

	ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
	return (error);
}

/*
 * Vnode operation to retrieve extended attributes on a vnode.
 */
static int
ffs_listextattr(
	struct vop_listextattr_args /* {
		IN struct vnode *a_vp;
		IN int a_attrnamespace;
		INOUT struct uio *a_uio;
		OUT size_t *a_size;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{
	struct inode *ip;
	struct extattr *eap, *eaend;
	int error, ealen;

	ip = VTOI(ap->a_vp);

	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
	if (error)
		return (error);

	error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
	if (error)
		return (error);

	error = 0;
	if (ap->a_size != NULL)
		*ap->a_size = 0;

	KASSERT(ALIGNED_TO(ip->i_ea_area, struct extattr), ("unaligned"));
	eap = (struct extattr *)ip->i_ea_area;
	eaend = (struct extattr *)(ip->i_ea_area + ip->i_ea_len);
	for (; error == 0 && eap < eaend; eap = EXTATTR_NEXT(eap)) {
		KASSERT(EXTATTR_NEXT(eap) <= eaend,
		    ("extattr next %p beyond %p", EXTATTR_NEXT(eap), eaend));
		if (eap->ea_namespace != ap->a_attrnamespace)
			continue;

		ealen = eap->ea_namelength;
		if (ap->a_size != NULL)
			*ap->a_size += ealen + 1;
		else if (ap->a_uio != NULL)
			error = uiomove(&eap->ea_namelength, ealen + 1,
			    ap->a_uio);
	}

	ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
	return (error);
}

/*
 * Vnode operation to set a named attribute.
 */
static int
ffs_setextattr(
	struct vop_setextattr_args /* {
		IN struct vnode *a_vp;
		IN int a_attrnamespace;
		IN const char *a_name;
		INOUT struct uio *a_uio;
		IN struct ucred *a_cred;
		IN struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct fs *fs;
	struct extattr *eap;
	uint32_t ealength, ul;
	ssize_t ealen;
	int olen, eapad1, eapad2, error, i, easize;
	uint8_t *eae;
	void *tmp;

	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ITOFS(ip);

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);
	if (strlen(ap->a_name) == 0)
		return (EINVAL);

	/* XXX Now unsupported API to delete EAs using NULL uio. */
	if (ap->a_uio == NULL)
		return (EOPNOTSUPP);

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	ealen = ap->a_uio->uio_resid;
	if (ealen < 0 || ealen > lblktosize(fs, UFS_NXADDR))
		return (EINVAL);

	error = extattr_check_cred(vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VWRITE);
	if (error) {
		/*
		 * ffs_lock_ea is not needed there, because the vnode
		 * must be exclusively locked.
		 */
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	if (DOINGSUJ(vp)) {
		ASSERT_VOP_ELOCKED(vp, "ffs_deleteextattr");
		softdep_prealloc(vp, MNT_WAIT);
		if (vp->v_data == NULL)
			return (EBADF);
	}

	error = ffs_open_ea(vp, ap->a_cred, ap->a_td);
	if (error)
		return (error);

	ealength = sizeof(uint32_t) + 3 + strlen(ap->a_name);
	eapad1 = roundup2(ealength, 8) - ealength;
	eapad2 = roundup2(ealen, 8) - ealen;
	ealength += eapad1 + ealen + eapad2;

	/*
	 * CEM: rewrites of the same size or smaller could be done in-place
	 * instead.  (We don't acquire any fine-grained locks in here either,
	 * so we could also do bigger writes in-place.)
	 */
	eae = malloc(ip->i_ea_len + ealength, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    &eap, NULL);
        if (olen == -1) {
		/* new, append at end */
		KASSERT(ALIGNED_TO(eae + easize, struct extattr),
		    ("unaligned"));
		eap = (struct extattr *)(eae + easize);
		easize += ealength;
	} else {
		ul = eap->ea_length;
		i = (uint8_t *)EXTATTR_NEXT(eap) - eae;
		if (ul != ealength) {
			bcopy(EXTATTR_NEXT(eap), (uint8_t *)eap + ealength,
			    easize - i);
			easize += (ealength - ul);
		}
	}
	if (easize > lblktosize(fs, UFS_NXADDR)) {
		free(eae, M_TEMP);
		ffs_close_ea(vp, 0, ap->a_cred, ap->a_td);
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = ENOSPC;
		return (ENOSPC);
	}
	eap->ea_length = ealength;
	eap->ea_namespace = ap->a_attrnamespace;
	eap->ea_contentpadlen = eapad2;
	eap->ea_namelength = strlen(ap->a_name);
	memcpy(eap->ea_name, ap->a_name, strlen(ap->a_name));
	bzero(&eap->ea_name[strlen(ap->a_name)], eapad1);
	error = uiomove(EXTATTR_CONTENT(eap), ealen, ap->a_uio);
	if (error) {
		free(eae, M_TEMP);
		ffs_close_ea(vp, 0, ap->a_cred, ap->a_td);
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}
	bzero((uint8_t *)EXTATTR_CONTENT(eap) + ealen, eapad2);

	tmp = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(tmp, M_TEMP);
	error = ffs_close_ea(vp, 1, ap->a_cred, ap->a_td);
	return (error);
}

/*
 * Vnode pointer to File handle
 */
static int
ffs_vptofh(
	struct vop_vptofh_args /* {
		IN struct vnode *a_vp;
		IN struct fid *a_fhp;
	} */ *ap)
{
	struct inode *ip;
	struct ufid *ufhp;
	_Static_assert(sizeof(struct ufid) <= sizeof(struct fid),
	    "struct ufid cannot be larger than struct fid");

	ip = VTOI(ap->a_vp);
	ufhp = (struct ufid *)ap->a_fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

SYSCTL_DECL(_vfs_ffs);
static int use_buf_pager = 1;
SYSCTL_INT(_vfs_ffs, OID_AUTO, use_buf_pager, CTLFLAG_RWTUN, &use_buf_pager, 0,
    "Always use buffer pager instead of bmap");

static daddr_t
ffs_gbp_getblkno(struct vnode *vp, vm_ooffset_t off)
{

	return (lblkno(VFSTOUFS(vp->v_mount)->um_fs, off));
}

static int
ffs_gbp_getblksz(struct vnode *vp, daddr_t lbn, long *sz)
{

	*sz = blksize(VFSTOUFS(vp->v_mount)->um_fs, VTOI(vp), lbn);
	return (0);
}

static int
ffs_getpages(struct vop_getpages_args *ap)
{
	struct vnode *vp;
	struct ufsmount *um;

	vp = ap->a_vp;
	um = VFSTOUFS(vp->v_mount);

	if (!use_buf_pager && um->um_devvp->v_bufobj.bo_bsize <= PAGE_SIZE)
		return (vnode_pager_generic_getpages(vp, ap->a_m, ap->a_count,
		    ap->a_rbehind, ap->a_rahead, NULL, NULL));
	return (vfs_bio_getpages(vp, ap->a_m, ap->a_count, ap->a_rbehind,
	    ap->a_rahead, ffs_gbp_getblkno, ffs_gbp_getblksz));
}

static int
ffs_getpages_async(struct vop_getpages_async_args *ap)
{
	struct vnode *vp;
	struct ufsmount *um;
	bool do_iodone;
	int error;

	vp = ap->a_vp;
	um = VFSTOUFS(vp->v_mount);
	do_iodone = true;

	if (um->um_devvp->v_bufobj.bo_bsize <= PAGE_SIZE) {
		error = vnode_pager_generic_getpages(vp, ap->a_m, ap->a_count,
		    ap->a_rbehind, ap->a_rahead, ap->a_iodone, ap->a_arg);
		if (error == 0)
			do_iodone = false;
	} else {
		error = vfs_bio_getpages(vp, ap->a_m, ap->a_count,
		    ap->a_rbehind, ap->a_rahead, ffs_gbp_getblkno,
		    ffs_gbp_getblksz);
	}
	if (do_iodone && ap->a_iodone != NULL)
		ap->a_iodone(ap->a_arg, ap->a_m, ap->a_count, error);

	return (error);
}

static int
ffs_vput_pair(struct vop_vput_pair_args *ap)
{
	struct mount *mp;
	struct vnode *dvp, *vp, *vp1, **vpp;
	struct inode *dp, *ip;
	ino_t ip_ino;
	uint64_t ip_gen;
	int error, vp_locked;

	dvp = ap->a_dvp;
	dp = VTOI(dvp);
	vpp = ap->a_vpp;
	vp = vpp != NULL ? *vpp : NULL;

	if ((dp->i_flag & (IN_NEEDSYNC | IN_ENDOFF)) == 0) {
		vput(dvp);
		if (vp != NULL && ap->a_unlock_vp)
			vput(vp);
		return (0);
	}

	mp = dvp->v_mount;
	if (vp != NULL) {
		if (ap->a_unlock_vp) {
			vput(vp);
		} else {
			MPASS(vp->v_type != VNON);
			vp_locked = VOP_ISLOCKED(vp);
			ip = VTOI(vp);
			ip_ino = ip->i_number;
			ip_gen = ip->i_gen;
			VOP_UNLOCK(vp);
		}
	}

	/*
	 * If compaction or fsync was requested do it in ffs_vput_pair()
	 * now that other locks are no longer held.
         */
	if ((dp->i_flag & IN_ENDOFF) != 0) {
		VNASSERT(I_ENDOFF(dp) != 0 && I_ENDOFF(dp) < dp->i_size, dvp,
		    ("IN_ENDOFF set but I_ENDOFF() is not"));
		dp->i_flag &= ~IN_ENDOFF;
		error = UFS_TRUNCATE(dvp, (off_t)I_ENDOFF(dp), IO_NORMAL |
		    (DOINGASYNC(dvp) ? 0 : IO_SYNC), curthread->td_ucred);
		if (error != 0 && error != ERELOOKUP) {
			if (!ffs_fsfail_cleanup(VFSTOUFS(mp), error)) {
				vn_printf(dvp,
				    "IN_ENDOFF: failed to truncate, "
				    "error %d\n", error);
			}
#ifdef UFS_DIRHASH
			ufsdirhash_free(dp);
#endif
		}
		SET_I_ENDOFF(dp, 0);
	}
	if ((dp->i_flag & IN_NEEDSYNC) != 0) {
		do {
			error = ffs_syncvnode(dvp, MNT_WAIT, 0);
		} while (error == ERELOOKUP);
	}

	vput(dvp);

	if (vp == NULL || ap->a_unlock_vp)
		return (0);
	MPASS(mp != NULL);

	/*
	 * It is possible that vp is reclaimed at this point. Only
	 * routines that call us with a_unlock_vp == false can find
	 * that their vp has been reclaimed. There are three areas
	 * that are affected:
	 * 1) vn_open_cred() - later VOPs could fail, but
	 *    dead_open() returns 0 to simulate successful open.
	 * 2) ffs_snapshot() - creation of snapshot fails with EBADF.
	 * 3) NFS server (several places) - code is prepared to detect
	 *    and respond to dead vnodes by returning ESTALE.
	 */
	VOP_LOCK(vp, vp_locked | LK_RETRY);
	if (IS_UFS(vp))
		return (0);

	/*
	 * Try harder to recover from reclaimed vp if reclaim was not
	 * because underlying inode was cleared.  We saved inode
	 * number and inode generation, so we can try to reinstantiate
	 * exactly same version of inode.  If this fails, return
	 * original doomed vnode and let caller to handle
	 * consequences.
	 *
	 * Note that callers must keep write started around
	 * VOP_VPUT_PAIR() calls, so it is safe to use mp without
	 * busying it.
	 */
	VOP_UNLOCK(vp);
	error = ffs_inotovp(mp, ip_ino, ip_gen, LK_EXCLUSIVE, &vp1,
	    FFSV_REPLACE_DOOMED);
	if (error != 0) {
		VOP_LOCK(vp, vp_locked | LK_RETRY);
	} else {
		vrele(vp);
		*vpp = vp1;
	}
	return (error);
}
