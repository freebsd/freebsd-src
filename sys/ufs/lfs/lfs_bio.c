/*
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs_bio.c	8.4 (Berkeley) 12/30/93
 * $Id: lfs_bio.c,v 1.8 1995/12/17 21:09:46 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/kernel.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

static void	lfs_flush __P((void));

/*
 * LFS block write function.
 *
 * XXX
 * No write cost accounting is done.
 * This is almost certainly wrong for synchronous operations and NFS.
 */
int	lfs_allclean_wakeup;		/* Cleaner wakeup address. */
int	locked_queue_count;		/* XXX Count of locked-down buffers. */
static int lfs_writing;			/* Set if already kicked off a writer
					   because of buffer space */
/*
#define WRITE_THRESHHOLD	((nbuf >> 2) - 10)
#define WAIT_THRESHHOLD		((nbuf >> 1) - 10)
*/
#define WAIT_THRESHHOLD         (nbuf - (nbuf >> 2) - 10)
#define WRITE_THRESHHOLD        ((nbuf >> 1) - 10)
#define LFS_BUFWAIT	2

int
lfs_bwrite(ap)
	struct vop_bwrite_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	struct lfs *fs;
	struct inode *ip;
	int error, s;

	/*
	 * Set the delayed write flag and use reassignbuf to move the buffer
	 * from the clean list to the dirty one.
	 *
	 * Set the B_LOCKED flag and unlock the buffer, causing brelse to move
	 * the buffer onto the LOCKED free list.  This is necessary, otherwise
	 * getnewbuf() would try to reclaim the buffers using bawrite, which
	 * isn't going to work.
	 *
	 * XXX we don't let meta-data writes run out of space because they can
	 * come from the segment writer.  We need to make sure that there is
	 * enough space reserved so that there's room to write meta-data
	 * blocks.
	 */
	if (!(bp->b_flags & B_LOCKED)) {
		fs = VFSTOUFS(bp->b_vp->v_mount)->um_lfs;
		while (!LFS_FITS(fs, fsbtodb(fs, 1)) && !IS_IFILE(bp) &&
		    bp->b_lblkno > 0) {
			/* Out of space, need cleaner to run */
			wakeup(&lfs_allclean_wakeup);
			error = tsleep(&fs->lfs_avail, PCATCH | PUSER,
			    "cleaner", 0);
			if (error) {
				brelse(bp);
				return (error);
			}
		}
		ip = VTOI((bp)->b_vp);
		if (!(ip->i_flag & IN_MODIFIED))
			++fs->lfs_uinodes;
		ip->i_flag |= IN_CHANGE | IN_MODIFIED | IN_UPDATE;
		fs->lfs_avail -= fsbtodb(fs, 1);
		++locked_queue_count;
		bp->b_flags |= B_DELWRI | B_LOCKED;
		bp->b_flags &= ~(B_READ | B_ERROR);
		s = splbio();
		reassignbuf(bp, bp->b_vp);
		splx(s);
	}
	brelse(bp);
	return (0);
}

/*
 * XXX
 * This routine flushes buffers out of the B_LOCKED queue when LFS has too
 * many locked down.  Eventually the pageout daemon will simply call LFS
 * when pages need to be reclaimed.  Note, we have one static count of locked
 * buffers, so we can't have more than a single file system.  To make this
 * work for multiple file systems, put the count into the mount structure.
 */
static void
lfs_flush()
{
	register struct mount *mp;

#ifdef DOSTATS
	++lfs_stats.write_exceeded;
#endif
	if (lfs_writing)
		return;
	lfs_writing = 1;
	for (mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = mp->mnt_list.cqe_next) {
		/* The lock check below is to avoid races with unmount. */
		if (mp->mnt_stat.f_type == MOUNT_LFS &&
		    (mp->mnt_flag & (MNT_MLOCK|MNT_RDONLY|MNT_UNMOUNT)) == 0 &&
		    !((((struct ufsmount *)mp->mnt_data))->ufsmount_u.lfs)->lfs_dirops ) {
			/*
			 * We set the queue to 0 here because we are about to
			 * write all the dirty buffers we have.  If more come
			 * in while we're writing the segment, they may not
			 * get written, so we want the count to reflect these
			 * new writes after the segwrite completes.
			 */
#ifdef DOSTATS
			++lfs_stats.flush_invoked;
#endif
			lfs_segwrite(mp, 0);
		}
	}
	lfs_writing = 0;
}

int
lfs_check(vp, blkno)
	struct vnode *vp;
	daddr_t blkno;
{
	int error;

	error = 0;
	if (incore(vp, blkno))
		return (0);
	if (locked_queue_count > WRITE_THRESHHOLD)
		lfs_flush();

	/* If out of buffers, wait on writer */
	while (locked_queue_count > WAIT_THRESHHOLD) {
#ifdef DOSTATS
	    ++lfs_stats.wait_exceeded;
#endif
	    error = tsleep(&locked_queue_count, PCATCH | PUSER, "buffers",
	        hz * LFS_BUFWAIT);
	}

	return (error);
}
