/*-
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
 *	from: @(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 * from: $FreeBSD: .../ufs/ufs_readwrite.c,v 1.96 2002/08/12 09:22:11 phk ...
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
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

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include "opt_directio.h"
#include "opt_ffs.h"

#ifdef DIRECTIO
extern int	ffs_rawread(struct vnode *vp, struct uio *uio, int *workdone);
#endif
static vop_fsync_t	ffs_fsync;
static vop_lock_t	ffs_lock;
static vop_getpages_t	ffs_getpages;
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


/* Global vfs data structures for ufs. */
struct vop_vector ffs_vnodeops = {
	.vop_default =		&ufs_vnodeops,
	.vop_fsync =		ffs_fsync,
	.vop_getpages =		ffs_getpages,
	.vop_lock =		ffs_lock,
	.vop_read =		ffs_read,
	.vop_reallocblks =	ffs_reallocblks,
	.vop_write =		ffs_write,
	.vop_closeextattr =	ffs_closeextattr,
	.vop_deleteextattr =	ffs_deleteextattr,
	.vop_getextattr =	ffs_getextattr,
	.vop_listextattr =	ffs_listextattr,
	.vop_openextattr =	ffs_openextattr,
	.vop_setextattr =	ffs_setextattr,
};

struct vop_vector ffs_fifoops = {
	.vop_default =		&ufs_fifoops,
	.vop_fsync =		ffs_fsync,
	.vop_lock =		ffs_lock,
	.vop_reallocblks =	ffs_reallocblks,
	.vop_strategy =		ffsext_strategy,
	.vop_closeextattr =	ffs_closeextattr,
	.vop_deleteextattr =	ffs_deleteextattr,
	.vop_getextattr =	ffs_getextattr,
	.vop_listextattr =	ffs_listextattr,
	.vop_openextattr =	ffs_openextattr,
	.vop_setextattr =	ffs_setextattr,
};

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ffs_fsync(struct vop_fsync_args *ap)
{
	int error;

	error = ffs_syncvnode(ap->a_vp, ap->a_waitfor);
	return (error);
}

int
ffs_syncvnode(struct vnode *vp, int waitfor)
{
	struct inode *ip = VTOI(vp);
	struct buf *bp;
	struct buf *nbp;
	int s, error, wait, passes, skipmeta;
	ufs_lbn_t lbn;

	wait = (waitfor == MNT_WAIT);
	lbn = lblkno(ip->i_fs, (ip->i_size + ip->i_fs->fs_bsize - 1));

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	passes = NIADDR + 1;
	skipmeta = 0;
	if (wait)
		skipmeta = 1;
	s = splbio();
	VI_LOCK(vp);
loop:
	TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs)
		bp->b_vflags &= ~BV_SCANNED;
	TAILQ_FOREACH_SAFE(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs, nbp) {
		/* 
		 * Reasons to skip this buffer: it has already been considered
		 * on this pass, this pass is the first time through on a
		 * synchronous flush request and the buffer being considered
		 * is metadata, the buffer has dependencies that will cause
		 * it to be redirtied and it has not already been deferred,
		 * or it is already being written.
		 */
		if ((bp->b_vflags & BV_SCANNED) != 0)
			continue;
		bp->b_vflags |= BV_SCANNED;
		if ((skipmeta == 1 && bp->b_lblkno < 0))
			continue;
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;
		VI_UNLOCK(vp);
		if (!wait && LIST_FIRST(&bp->b_dep) != NULL &&
		    (bp->b_flags & B_DEFERRED) == 0 &&
		    buf_countdeps(bp, 0)) {
			bp->b_flags |= B_DEFERRED;
			BUF_UNLOCK(bp);
			VI_LOCK(vp);
			continue;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ffs_fsync: not dirty");
		/*
		 * If this is a synchronous flush request, or it is not a
		 * file or device, start the write on this buffer immediatly.
		 */
		if (wait || (vp->v_type != VREG && vp->v_type != VBLK)) {

			/*
			 * On our final pass through, do all I/O synchronously
			 * so that we can find out if our flush is failing
			 * because of write errors.
			 */
			if (passes > 0 || !wait) {
				if ((bp->b_flags & B_CLUSTEROK) && !wait) {
					(void) vfs_bio_awrite(bp);
				} else {
					bremfree(bp);
					splx(s);
					(void) bawrite(bp);
					s = splbio();
				}
			} else {
				bremfree(bp);
				splx(s);
				if ((error = bwrite(bp)) != 0)
					return (error);
				s = splbio();
			}
		} else if ((vp->v_type == VREG) && (bp->b_lblkno >= lbn)) {
			/* 
			 * If the buffer is for data that has been truncated
			 * off the file, then throw it away.
			 */
			bremfree(bp);
			bp->b_flags |= B_INVAL | B_NOCACHE;
			splx(s);
			brelse(bp);
			s = splbio();
		} else
			vfs_bio_awrite(bp);

		/*
		 * Since we may have slept during the I/O, we need 
		 * to start from a known point.
		 */
		VI_LOCK(vp);
		nbp = TAILQ_FIRST(&vp->v_bufobj.bo_dirty.bv_hd);
	}
	/*
	 * If we were asked to do this synchronously, then go back for
	 * another pass, this time doing the metadata.
	 */
	if (skipmeta) {
		skipmeta = 0;
		goto loop;
	}

	if (wait) {
		bufobj_wwait(&vp->v_bufobj, 3, 0);
		VI_UNLOCK(vp);

		/* 
		 * Ensure that any filesystem metatdata associated
		 * with the vnode has been written.
		 */
		splx(s);
		if ((error = softdep_sync_metadata(vp)) != 0)
			return (error);
		s = splbio();

		VI_LOCK(vp);
		if (vp->v_bufobj.bo_dirty.bv_cnt > 0) {
			/*
			 * Block devices associated with filesystems may
			 * have new I/O requests posted for them even if
			 * the vnode is locked, so no amount of trying will
			 * get them clean. Thus we give block devices a
			 * good effort, then just give up. For all other file
			 * types, go around and try again until it is clean.
			 */
			if (passes > 0) {
				passes -= 1;
				goto loop;
			}
#ifdef DIAGNOSTIC
			if (!vn_isdisk(vp, NULL))
				vprint("ffs_fsync: dirty", vp);
#endif
		}
	}
	VI_UNLOCK(vp);
	splx(s);
	return (UFS_UPDATE(vp, wait));
}

/*
 * Snapshots require all lock requests to be exclusive.
 */
static int
ffs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if ((VTOI(vp)->i_flags & SF_SNAPSHOT) &&
	    ((ap->a_flags & LK_TYPE_MASK) == LK_SHARED)) {
		ap->a_flags &= ~LK_TYPE_MASK;
		ap->a_flags |= LK_EXCLUSIVE;
	}
	return (VOP_LOCK_APV(&ufs_vnodeops, ap));
}

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
static int
ffs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error, orig_resid;
	int seqcount;
	int ioflag;

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

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("ffs_read: mode");

	if (vp->v_type == VLNK) {
		if ((int)ip->i_size < vp->v_mount->mnt_maxsymlinklen)
			panic("ffs_read: short symlink");
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("ffs_read: type %d",  vp->v_type);
#endif
	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0, ("ffs_read: uio->uio_resid < 0"));
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0, ("ffs_read: uio->uio_offset < 0"));
	fs = ip->i_fs;
	if (uio->uio_offset < ip->i_size &&
	    uio->uio_offset >= fs->fs_maxfilesize)
		return (EOVERFLOW);

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
			error = bread(vp, lbn, size, NOCRED, &bp);
		} else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			/* 
			 * Otherwise if we are allowed to cluster,
			 * grab as much as we can.
			 *
			 * XXX  This may not be a win if we are not
			 * doing sequential access.
			 */
			error = cluster_read(vp, ip->i_size, lbn,
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
			int nextsize = blksize(fs, ip, nextlbn);
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

		error = uiomove((char *)bp->b_data + blkoffset,
		    (int)xfersize, uio);
		if (error)
			break;

		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
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
		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if ((error == 0 || uio->uio_resid != orig_resid) &&
	    (vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
		ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Vnode op for writing.
 */
static int
ffs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp;
	struct uio *uio;
	struct inode *ip;
	struct fs *fs;
	struct buf *bp;
	struct thread *td;
	ufs_lbn_t lbn;
	off_t osize;
	int seqcount;
	int blkoffset, error, extended, flags, ioflag, resid, size, xfersize;

	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	if (ap->a_ioflag & IO_EXT)
#ifdef notyet
		return (ffs_extwrite(vp, uio, ioflag, ap->a_cred));
#else
		panic("ffs_write+IO_EXT");
#endif

	extended = 0;
	seqcount = ap->a_ioflag >> IO_SEQSHIFT;
	ip = VTOI(vp);

#ifdef DIAGNOSTIC
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
	fs = ip->i_fs;
	if ((uoff_t)uio->uio_offset + uio->uio_resid > fs->fs_maxfilesize)
		return (EFBIG);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	td = uio->uio_td;
	if (vp->v_type == VREG && td != NULL) {
		PROC_LOCK(td->td_proc);
		if (uio->uio_offset + uio->uio_resid >
		    lim_cur(td->td_proc, RLIMIT_FSIZE)) {
			psignal(td->td_proc, SIGXFSZ);
			PROC_UNLOCK(td->td_proc);
			return (EFBIG);
		}
		PROC_UNLOCK(td->td_proc);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	if (seqcount > BA_SEQMAX)
		flags = BA_SEQMAX << BA_SEQSHIFT;
	else
		flags = seqcount << BA_SEQSHIFT;
	if ((ioflag & IO_SYNC) && !DOINGASYNC(vp))
		flags |= IO_SYNC;

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
		if (ioflag & IO_DIRECT)
			bp->b_flags |= B_DIRECT;
		if ((ioflag & (IO_SYNC|IO_INVAL)) == (IO_SYNC|IO_INVAL))
			bp->b_flags |= B_NOCACHE;

		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
			DIP_SET(ip, i_size, ip->i_size);
			extended = 1;
		}

		size = blksize(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			bp->b_flags |= B_RELBUF;
		}

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
				cluster_write(vp, bp, ip->i_size, seqcount);
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
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ap->a_cred && 
	    suser_cred(ap->a_cred, SUSER_ALLOWJAIL)) {
		ip->i_mode &= ~(ISUID | ISGID);
		DIP_SET(ip, i_mode, ip->i_mode);
	}
	if (resid > uio->uio_resid)
		VN_KNOTE_UNLOCKED(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)UFS_TRUNCATE(vp, osize,
			    IO_NORMAL | (ioflag & IO_SYNC),
			    ap->a_cred, uio->uio_td);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = UFS_UPDATE(vp, 1);
	return (error);
}

/*
 * get page routine
 */
static int
ffs_getpages(ap)
	struct vop_getpages_args *ap;
{
	int i;
	vm_page_t mreq;
	int pcount;

	pcount = round_page(ap->a_count) / PAGE_SIZE;
	mreq = ap->a_m[ap->a_reqpage];

	/*
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block,
	 * then the entire page is valid.  Since the page may be mapped,
	 * user programs might reference data beyond the actual end of file
	 * occuring within the page.  We have to zero that data.
	 */
	VM_OBJECT_LOCK(mreq->object);
	if (mreq->valid) {
		if (mreq->valid != VM_PAGE_BITS_ALL)
			vm_page_zero_invalid(mreq, TRUE);
		vm_page_lock_queues();
		for (i = 0; i < pcount; i++) {
			if (i != ap->a_reqpage) {
				vm_page_free(ap->a_m[i]);
			}
		}
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(mreq->object);
		return VM_PAGER_OK;
	}
	VM_OBJECT_UNLOCK(mreq->object);

	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
					    ap->a_count,
					    ap->a_reqpage);
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
	int error, orig_resid;

	ip = VTOI(vp);
	fs = ip->i_fs;
	dp = ip->i_din2;

#ifdef DIAGNOSTIC
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

		error = uiomove((char *)bp->b_data + blkoffset,
					(int)xfersize, uio);
		if (error)
			break;

		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
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
		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if ((error == 0 || uio->uio_resid != orig_resid) &&
	    (vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
		ip->i_flag |= IN_ACCESS;
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
	int blkoffset, error, flags, resid, size, xfersize;

	ip = VTOI(vp);
	fs = ip->i_fs;
	dp = ip->i_din2;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE || fs->fs_magic != FS_UFS2_MAGIC)
		panic("ffs_extwrite: mode");
#endif

	if (ioflag & IO_APPEND)
		uio->uio_offset = dp->di_extsize;
	KASSERT(uio->uio_offset >= 0, ("ffs_extwrite: uio->uio_offset < 0"));
	KASSERT(uio->uio_resid >= 0, ("ffs_extwrite: uio->uio_resid < 0"));
	if ((uoff_t)uio->uio_offset + uio->uio_resid > NXADDR * fs->fs_bsize)
		return (EFBIG);

	resid = uio->uio_resid;
	osize = dp->di_extsize;
	flags = IO_EXT;
	if ((ioflag & IO_SYNC) && !DOINGASYNC(vp))
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
		if (ioflag & IO_DIRECT)
			bp->b_flags |= B_DIRECT;

		if (uio->uio_offset + xfersize > dp->di_extsize)
			dp->di_extsize = uio->uio_offset + xfersize;

		size = sblksize(fs, dp->di_extsize, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if ((ioflag & (IO_VMIO|IO_DIRECT)) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			bp->b_flags |= B_RELBUF;
		}

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
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ucred && 
	    suser_cred(ucred, SUSER_ALLOWJAIL)) {
		ip->i_mode &= ~(ISUID | ISGID);
		dp->di_mode = ip->i_mode;
	}
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)UFS_TRUNCATE(vp, osize,
			    IO_EXT | (ioflag&IO_SYNC), ucred, uio->uio_td);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = UFS_UPDATE(vp, 1);
	return (error);
}


/*
 * Vnode operating to retrieve a named extended attribute.
 *
 * Locate a particular EA (nspace:name) in the area (ptr:length), and return
 * the length of the EA, and possibly the pointer to the entry and to the data.
 */
static int
ffs_findextattr(u_char *ptr, u_int length, int nspace, const char *name, u_char **eap, u_char **eac)
{
	u_char *p, *pe, *pn, *p0;
	int eapad1, eapad2, ealength, ealen, nlen;
	uint32_t ul;

	pe = ptr + length;
	nlen = strlen(name);

	for (p = ptr; p < pe; p = pn) {
		p0 = p;
		bcopy(p, &ul, sizeof(ul));
		pn = p + ul;
		/* make sure this entry is complete */
		if (pn > pe)
			break;
		p += sizeof(uint32_t);
		if (*p != nspace)
			continue;
		p++;
		eapad2 = *p++;
		if (*p != nlen)
			continue;
		p++;
		if (bcmp(p, name, nlen))
			continue;
		ealength = sizeof(uint32_t) + 3 + nlen;
		eapad1 = 8 - (ealength % 8);
		if (eapad1 == 8)
			eapad1 = 0;
		ealength += eapad1;
		ealen = ul - ealength - eapad2;
		p += nlen + eapad1;
		if (eap != NULL)
			*eap = p0;
		if (eac != NULL)
			*eac = p;
		return (ealen);
	}
	return(-1);
}

static int
ffs_rdextattr(u_char **p, struct vnode *vp, struct thread *td, int extra)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct uio luio;
	struct iovec liovec;
	int easize, error;
	u_char *eae;

	ip = VTOI(vp);
	dp = ip->i_din2;
	easize = dp->di_extsize;

	eae = malloc(easize + extra, M_TEMP, M_WAITOK);

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
		return(error);
	}
	*p = eae;
	return (0);
}

static int
ffs_open_ea(struct vnode *vp, struct ucred *cred, struct thread *td)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	int error;

	ip = VTOI(vp);

	if (ip->i_ea_area != NULL)
		return (EBUSY);
	dp = ip->i_din2;
	error = ffs_rdextattr(&ip->i_ea_area, vp, td, 0);
	if (error)
		return (error);
	ip->i_ea_len = dp->di_extsize;
	ip->i_ea_error = 0;
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
	struct iovec liovec;
	int error;
	struct ufs2_dinode *dp;

	ip = VTOI(vp);
	if (ip->i_ea_area == NULL)
		return (EINVAL);
	dp = ip->i_din2;
	error = ip->i_ea_error;
	if (commit && error == 0) {
		if (cred == NOCRED)
			cred =  vp->v_mount->mnt_cred;
		liovec.iov_base = ip->i_ea_area;
		liovec.iov_len = ip->i_ea_len;
		luio.uio_iov = &liovec;
		luio.uio_iovcnt = 1;
		luio.uio_offset = 0;
		luio.uio_resid = ip->i_ea_len;
		luio.uio_segflg = UIO_SYSSPACE;
		luio.uio_rw = UIO_WRITE;
		luio.uio_td = td;
		/* XXX: I'm not happy about truncating to zero size */
		if (ip->i_ea_len < dp->di_extsize)
			error = ffs_truncate(vp, 0, IO_EXT, cred, td);
		error = ffs_extwrite(vp, &luio, IO_EXT | IO_SYNC, cred);
	}
	free(ip->i_ea_area, M_TEMP);
	ip->i_ea_area = NULL;
	ip->i_ea_len = 0;
	ip->i_ea_error = 0;
	return (error);
}

/*
 * Vnode extattr strategy routine for fifos.
 *
 * We need to check for a read or write of the external attributes.
 * Otherwise we just fall through and do the usual thing.
 */
static int
ffsext_strategy(struct vop_strategy_args *ap)
/*
struct vop_strategy_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct buf *a_bp;
};
*/
{
	struct vnode *vp;
	daddr_t lbn;

	vp = ap->a_vp;
	lbn = ap->a_bp->b_lblkno;
	if (VTOI(vp)->i_fs->fs_magic == FS_UFS2_MAGIC &&
	    lbn < 0 && lbn >= -NXADDR)
		return (VOP_STRATEGY_APV(&ufs_vnodeops, ap));
	if (vp->v_type == VFIFO)
		return (VOP_STRATEGY_APV(&ufs_fifoops, ap));
	panic("spec nodes went here");
}

/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_openextattr(struct vop_openextattr_args *ap)
/*
struct vop_openextattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_openextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	return (ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td));
}


/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_closeextattr(struct vop_closeextattr_args *ap)
/*
struct vop_closeextattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_commit;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_closeextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	return (ffs_close_ea(ap->a_vp, ap->a_commit, ap->a_cred, ap->a_td));
}

/*
 * Vnode operation to remove a named attribute.
 */
static int
ffs_deleteextattr(struct vop_deleteextattr_args *ap)
/*
vop_deleteextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;
	uint32_t ealength, ul;
	int ealen, olen, eapad1, eapad2, error, i, easize;
	u_char *eae, *p;
	int stand_alone;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_deleteextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	if (strlen(ap->a_name) == 0)
		return (EINVAL);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, IWRITE);
	if (error) {
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	if (ip->i_ea_area == NULL) {
		error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		stand_alone = 1;
	} else {
		stand_alone = 0;
	}

	ealength = eapad1 = ealen = eapad2 = 0;

	eae = malloc(ip->i_ea_len, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    &p, NULL);
	if (olen == -1) {
		/* delete but nonexistent */
		free(eae, M_TEMP);
		if (stand_alone)
			ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
		return(ENOATTR);
	}
	bcopy(p, &ul, sizeof ul);
	i = p - eae + ul;
	if (ul != ealength) {
		bcopy(p + ul, p + ealength, easize - i);
		easize += (ealength - ul);
	}
	if (easize > NXADDR * fs->fs_bsize) {
		free(eae, M_TEMP);
		if (stand_alone)
			ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
		else if (ip->i_ea_error == 0)
			ip->i_ea_error = ENOSPC;
		return(ENOSPC);
	}
	p = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(p, M_TEMP);
	if (stand_alone)
		error = ffs_close_ea(ap->a_vp, 1, ap->a_cred, ap->a_td);
	return(error);
}

/*
 * Vnode operation to retrieve a named extended attribute.
 */
static int
ffs_getextattr(struct vop_getextattr_args *ap)
/*
vop_getextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	INOUT struct uio *a_uio;
	OUT size_t *a_size;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;
	u_char *eae, *p;
	unsigned easize;
	int error, ealen, stand_alone;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_getextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, IREAD);
	if (error)
		return (error);

	if (ip->i_ea_area == NULL) {
		error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		stand_alone = 1;
	} else {
		stand_alone = 0;
	}
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
	if (stand_alone)
		ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
	return(error);
}

/*
 * Vnode operation to retrieve extended attributes on a vnode.
 */
static int
ffs_listextattr(struct vop_listextattr_args *ap)
/*
vop_listextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	INOUT struct uio *a_uio;
	OUT size_t *a_size;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;
	u_char *eae, *p, *pe, *pn;
	unsigned easize;
	uint32_t ul;
	int error, ealen, stand_alone;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_listextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, IREAD);
	if (error)
		return (error);

	if (ip->i_ea_area == NULL) {
		error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		stand_alone = 1;
	} else {
		stand_alone = 0;
	}
	eae = ip->i_ea_area;
	easize = ip->i_ea_len;

	error = 0;
	if (ap->a_size != NULL)
		*ap->a_size = 0;
	pe = eae + easize;
	for(p = eae; error == 0 && p < pe; p = pn) {
		bcopy(p, &ul, sizeof(ul));
		pn = p + ul;
		if (pn > pe)
			break;
		p += sizeof(ul);
		if (*p++ != ap->a_attrnamespace)
			continue;
		p++;	/* pad2 */
		ealen = *p;
		if (ap->a_size != NULL) {
			*ap->a_size += ealen + 1;
		} else if (ap->a_uio != NULL) {
			error = uiomove(p, ealen + 1, ap->a_uio);
		}
	}
	if (stand_alone)
		ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
	return(error);
}

/*
 * Vnode operation to set a named attribute.
 */
static int
ffs_setextattr(struct vop_setextattr_args *ap)
/*
vop_setextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	INOUT struct uio *a_uio;
	IN struct ucred *a_cred;
	IN struct thread *a_td;
};
*/
{
	struct inode *ip;
	struct fs *fs;
	uint32_t ealength, ul;
	int ealen, olen, eapad1, eapad2, error, i, easize;
	u_char *eae, *p;
	int stand_alone;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (ufs_vnodeops.vop_setextattr(ap));

	if (ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	if (strlen(ap->a_name) == 0)
		return (EINVAL);

	/* XXX Now unsupported API to delete EAs using NULL uio. */
	if (ap->a_uio == NULL)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, IWRITE);
	if (error) {
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	if (ip->i_ea_area == NULL) {
		error = ffs_open_ea(ap->a_vp, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		stand_alone = 1;
	} else {
		stand_alone = 0;
	}

	ealen = ap->a_uio->uio_resid;
	ealength = sizeof(uint32_t) + 3 + strlen(ap->a_name);
	eapad1 = 8 - (ealength % 8);
	if (eapad1 == 8)
		eapad1 = 0;
	eapad2 = 8 - (ealen % 8);
	if (eapad2 == 8)
		eapad2 = 0;
	ealength += eapad1 + ealen + eapad2;

	eae = malloc(ip->i_ea_len + ealength, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize,
	    ap->a_attrnamespace, ap->a_name, &p, NULL);
        if (olen == -1) {
		/* new, append at end */
		p = eae + easize;
		easize += ealength;
	} else {
		bcopy(p, &ul, sizeof ul);
		i = p - eae + ul;
		if (ul != ealength) {
			bcopy(p + ul, p + ealength, easize - i);
			easize += (ealength - ul);
		}
	}
	if (easize > NXADDR * fs->fs_bsize) {
		free(eae, M_TEMP);
		if (stand_alone)
			ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
		else if (ip->i_ea_error == 0)
			ip->i_ea_error = ENOSPC;
		return(ENOSPC);
	}
	bcopy(&ealength, p, sizeof(ealength));
	p += sizeof(ealength);
	*p++ = ap->a_attrnamespace;
	*p++ = eapad2;
	*p++ = strlen(ap->a_name);
	strcpy(p, ap->a_name);
	p += strlen(ap->a_name);
	bzero(p, eapad1);
	p += eapad1;
	error = uiomove(p, ealen, ap->a_uio);
	if (error) {
		free(eae, M_TEMP);
		if (stand_alone)
			ffs_close_ea(ap->a_vp, 0, ap->a_cred, ap->a_td);
		else if (ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return(error);
	}
	p += ealen;
	bzero(p, eapad2);

	p = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(p, M_TEMP);
	if (stand_alone)
		error = ffs_close_ea(ap->a_vp, 1, ap->a_cred, ap->a_td);
	return(error);
}
