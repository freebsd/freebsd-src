/*
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
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 * $FreeBSD$
 */

#include "opt_ffs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/conf.h>

#include <machine/limits.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int	ffs_fsync __P((struct vop_fsync_args *));
static int	ffs_getpages __P((struct vop_getpages_args *));
static int	ffs_putpages __P((struct vop_putpages_args *));
static int	ffs_read __P((struct vop_read_args *));
static int	ffs_write __P((struct vop_write_args *));

/* Global vfs data structures for ufs. */
vop_t **ffs_vnodeop_p;
static struct vnodeopv_entry_desc ffs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ &vop_getpages_desc,		(vop_t *) ffs_getpages },
	{ &vop_putpages_desc,		(vop_t *) ffs_putpages },
	{ &vop_read_desc,		(vop_t *) ffs_read },
	{ &vop_balloc_desc,		(vop_t *) ffs_balloc },
	{ &vop_reallocblks_desc,	(vop_t *) ffs_reallocblks },
	{ &vop_write_desc,		(vop_t *) ffs_write },
#ifdef FFS_EXTATTR
	{ &vop_getextattr_desc, 	(vop_t *) ufs_vop_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_vop_setextattr },
#endif
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_vnodeop_opv_desc =
	{ &ffs_vnodeop_p, ffs_vnodeop_entries };

vop_t **ffs_specop_p;
static struct vnodeopv_entry_desc ffs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatespec },
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ &vop_copyonwrite_desc,	(vop_t *) ffs_copyonwrite },
#ifdef FFS_EXTATTR
	{ &vop_getextattr_desc,		(vop_t *) ufs_vop_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_vop_setextattr },
#endif
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_specop_opv_desc =
	{ &ffs_specop_p, ffs_specop_entries };

vop_t **ffs_fifoop_p;
static struct vnodeopv_entry_desc ffs_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatefifo },
#ifdef FFS_EXTATTR
	{ &vop_getextattr_desc,		(vop_t *) ufs_vop_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_vop_setextattr },
#endif
	{ &vop_fsync_desc,		(vop_t *) ffs_fsync },
	{ NULL, NULL }
};
static struct vnodeopv_desc ffs_fifoop_opv_desc =
	{ &ffs_fifoop_p, ffs_fifoop_entries };

VNODEOP_SET(ffs_vnodeop_opv_desc);
VNODEOP_SET(ffs_specop_opv_desc);
VNODEOP_SET(ffs_fifoop_opv_desc);

#include <ufs/ufs/ufs_readwrite.c>

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ffs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct buf *bp;
	struct buf *nbp;
	int s, error, wait, passes, skipmeta;
	daddr_t lbn;

	/*
	 * Snapshots have to be unlocked so they do not deadlock
	 * checking whether they need to copy their written buffers.
	 * We always hold a reference, so they cannot be removed
	 * out from underneath us.
	 */
	if (ip->i_flags & SF_SNAPSHOT)
		VOP_UNLOCK(vp, 0, ap->a_p);
	wait = (ap->a_waitfor == MNT_WAIT);
	if (vn_isdisk(vp, NULL)) {
		lbn = INT_MAX;
		if (vp->v_specmountpoint != NULL &&
		    (vp->v_specmountpoint->mnt_flag & MNT_SOFTDEP))
			softdep_fsync_mountdev(vp);
	} else {
		lbn = lblkno(ip->i_fs, (ip->i_size + ip->i_fs->fs_bsize - 1));
	}

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	passes = NIADDR + 1;
	skipmeta = 0;
	if (wait)
		skipmeta = 1;
	s = splbio();
loop:
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp;
	     bp = TAILQ_NEXT(bp, b_vnbufs))
		bp->b_flags &= ~B_SCANNED;
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = TAILQ_NEXT(bp, b_vnbufs);
		/* 
		 * Reasons to skip this buffer: it has already been considered
		 * on this pass, this pass is the first time through on a
		 * synchronous flush request and the buffer being considered
		 * is metadata, the buffer has dependencies that will cause
		 * it to be redirtied and it has not already been deferred,
		 * or it is already being written.
		 */
		if ((bp->b_flags & B_SCANNED) != 0)
			continue;
		bp->b_flags |= B_SCANNED;
		if ((skipmeta == 1 && bp->b_lblkno < 0))
			continue;
		if (!wait && LIST_FIRST(&bp->b_dep) != NULL &&
		    (bp->b_flags & B_DEFERRED) == 0 &&
		    buf_countdeps(bp, 0)) {
			bp->b_flags |= B_DEFERRED;
			continue;
		}
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ffs_fsync: not dirty");
		if (vp != bp->b_vp)
			panic("ffs_fsync: vp != vp->b_vp");
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
					BUF_UNLOCK(bp);
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
		} else {
			BUF_UNLOCK(bp);
			vfs_bio_awrite(bp);
		}
		/*
		 * Since we may have slept during the I/O, we need 
		 * to start from a known point.
		 */
		nbp = TAILQ_FIRST(&vp->v_dirtyblkhd);
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
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			(void) tsleep((caddr_t)&vp->v_numoutput,
					PRIBIO + 4, "ffsfsn", 0);
  		}

		/* 
		 * Ensure that any filesystem metatdata associated
		 * with the vnode has been written.
		 */
		splx(s);
		if ((error = softdep_sync_metadata(ap)) != 0)
			return (error);
		s = splbio();

		if (!TAILQ_EMPTY(&vp->v_dirtyblkhd)) {
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
	splx(s);
	error = UFS_UPDATE(vp, wait);
	if (ip->i_flags & SF_SNAPSHOT)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
	return (error);
}
