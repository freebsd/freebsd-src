/*
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)lfs_inode.c	8.9 (Berkeley) 5/8/95
 * $Id: lfs_inode.c,v 1.18 1997/09/02 20:06:48 bde Exp $
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

/* Search a block for a specific dinode. */
struct dinode *
lfs_ifind(fs, ino, dip)
	struct lfs *fs;
	ino_t ino;
	register struct dinode *dip;
{
	register int cnt;
	register struct dinode *ldip;

	for (cnt = INOPB(fs), ldip = dip + (cnt - 1); cnt--; --ldip)
		if (ldip->di_inumber == ino)
			return (ldip);

	panic("lfs_ifind: dinode %u not found", ino);
	/* NOTREACHED */
}

int
lfs_update(ap)
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timeval *a_access;
		struct timeval *a_modify;
		int a_waitfor;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip;
	int error;

	if (vp->v_mount->mnt_flag & MNT_RDONLY){
		return (0);
	 }
	ip = VTOI(vp);
	/* XXX
	 * We used to just return here.  Now we make sure to check if
	 * we were called by lfs_fsync, since in this case, the inode
	 * may have been written to disk without all buffers connected
	 * with the vnode being flushed.  It seems really suspicious
	 * that this could happen since from what I understand of the
	 * intended semantics, one of these flags should be set if there
	 * are still dirty buffers.  Compare to how ffs_fsync/ffs_update
	 * work together and you'll see what I mean.
	 */
	if (((ip->i_flag & (IN_ACCESS|IN_CHANGE|IN_MODIFIED|IN_UPDATE)) == 0)
	    && (vp->v_dirtyblkhd.lh_first == NULL))
		return(0);

	if (ip->i_flag & IN_ACCESS)
		ip->i_atime = ap->a_access->tv_sec;
	if (ip->i_flag & IN_UPDATE) {
		ip->i_mtime = ap->a_modify->tv_sec;
		(ip)->i_modrev++;
	}
	if (ip->i_flag & IN_CHANGE)
		ip->i_ctime = time.tv_sec;
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);

	if (!(ip->i_flag & IN_MODIFIED))
		++(VFSTOUFS(vp->v_mount)->um_lfs->lfs_uinodes);
	ip->i_flag |= IN_MODIFIED;

	/* If sync, push back the vnode and any dirty blocks it may have. */
	error = (ap->a_waitfor & LFS_SYNC ? lfs_vflush(vp) : 0);
	if(ap->a_waitfor &  LFS_SYNC && vp->v_dirtyblkhd.lh_first != NULL)
	       panic("lfs_update: dirty bufs");
	return( error );

}

/* Update segment usage information when removing a block. */
#define UPDATE_SEGUSE \
	if (lastseg != -1) { \
		LFS_SEGENTRY(sup, fs, lastseg, sup_bp); \
		if (num > sup->su_nbytes) \
			panic("lfs_truncate: negative bytes in segment %d", \
			    lastseg); \
		sup->su_nbytes -= num; \
		e1 = VOP_BWRITE(sup_bp); \
		fragsreleased += numfrags(fs, num); \
	}

#define SEGDEC(S) { \
	if (daddr != 0) { \
		if (lastseg != (seg = datosn(fs, daddr))) { \
			UPDATE_SEGUSE; \
			num = (S); \
			lastseg = seg; \
		} else \
			num += (S); \
	} \
}

/*
 * Truncate the inode ip to at most length size.  Update segment usage
 * table information.
 */
/* ARGSUSED */
int
lfs_truncate(vp, length, flags, cred, p)
	struct vnode *vp;
	off_t length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	register struct indir *inp;
	register int i;
	register ufs_daddr_t *daddrp;
	struct buf *bp, *sup_bp;
	struct timeval tv;
	struct ifile *ifp;
	struct inode *ip;
	struct lfs *fs;
	struct indir a[NIADDR + 2], a_end[NIADDR + 2];
	SEGUSE *sup;
  	ufs_daddr_t daddr, lastblock, lbn, olastblock;
	ufs_daddr_t oldsize_lastblock, oldsize_newlast, newsize;
	long off, a_released, fragsreleased, i_released;
	int e1, e2, depth, lastseg, num, offset, seg, freesize;

	ip = VTOI(vp);
	gettime(&tv);
	if (vp->v_type == VLNK && vp->v_mount->mnt_maxsymlinklen > 0) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("lfs_truncate: partial truncate of symlink");
#endif
		bzero((char *)&ip->i_shortlink, (u_int)ip->i_size);
		ip->i_size = 0;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(vp, &tv, &tv, 0));
	}
	vnode_pager_setsize(vp, (u_long)length);

	fs = ip->i_lfs;

	/* If length is larger than the file, just update the times. */
	if (ip->i_size <= length) {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(vp, &tv, &tv, 0));
	}

	/*
	 * Calculate index into inode's block list of last direct and indirect
	 * blocks (if any) which we want to keep.  Lastblock is 0 when the
	 * file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->lfs_bsize - 1);
	olastblock = lblkno(fs, ip->i_size + fs->lfs_bsize - 1) - 1;

	/*
	 * Update the size of the file. If the file is not being truncated to
	 * a block boundry, the contents of the partial block following the end
	 * of the file must be zero'ed in case it ever become accessable again
	 * because of subsequent file growth.  For this part of the code,
	 * oldsize_newlast refers to the old size of the new last block in the file.
	 */
	offset = blkoff(fs, length);
	lbn = lblkno(fs, length);
	oldsize_newlast = blksize(fs, ip, lbn);

	/* Now set oldsize to the current size of the current last block */
	oldsize_lastblock = blksize(fs, ip, olastblock);
	if (offset == 0)
		ip->i_size = length;
	else {
#ifdef QUOTA
		if (e1 = getinoquota(ip))
			return (e1);
#endif
		if (e1 = bread(vp, lbn, oldsize_newlast, NOCRED, &bp))
			return (e1);
		ip->i_size = length;
		newsize = blksize(fs, ip, lbn);
		bzero((char *)bp->b_data + offset, (u_int)(newsize - offset));
		allocbuf(bp, newsize);
		if (e1 = VOP_BWRITE(bp))
			return (e1);
	}
	/*
	 * Modify sup->su_nbyte counters for each deleted block; keep track
	 * of number of blocks removed for ip->i_blocks.
	 */
	fragsreleased = 0;
	num = 0;
	lastseg = -1;

	for (lbn = olastblock; lbn >= lastblock;) {
		/* XXX use run length from bmap array to make this faster */
		ufs_bmaparray(vp, lbn, &daddr, a, &depth, NULL, NULL);
		if (lbn == olastblock) {
			for (i = NIADDR + 2; i--;)
				a_end[i] = a[i];
			freesize = oldsize_lastblock;
		} else
			freesize = fs->lfs_bsize;

		switch (depth) {
		case 0:				/* Direct block. */
			daddr = ip->i_db[lbn];
			SEGDEC(freesize);
			ip->i_db[lbn] = 0;
			--lbn;
			break;
#ifdef DIAGNOSTIC
		case 1:				/* An indirect block. */
			panic("lfs_truncate: ufs_bmaparray returned depth 1");
			/* NOTREACHED */
#endif
		default:			/* Chain of indirect blocks. */
			inp = a + --depth;
			if (inp->in_off > 0 && lbn != lastblock) {
				lbn -= inp->in_off < lbn - lastblock ?
				    inp->in_off : lbn - lastblock;
				break;
			}
			for (; depth && (inp->in_off == 0 || lbn == lastblock);
			    --inp, --depth) {
				if (bread(vp,
				    inp->in_lbn, fs->lfs_bsize, NOCRED, &bp))
					panic("lfs_truncate: bread bno %d",
					    inp->in_lbn);
				daddrp = (ufs_daddr_t *)bp->b_data +
				    inp->in_off;
				for (i = inp->in_off;
				    i++ <= a_end[depth].in_off;) {
					daddr = *daddrp++;
					SEGDEC(freesize);
				}
				a_end[depth].in_off = NINDIR(fs) - 1;
				if (inp->in_off == 0)
					brelse (bp);
				else {
					bzero((ufs_daddr_t *)bp->b_data +
					    inp->in_off, fs->lfs_bsize -
					    inp->in_off * sizeof(ufs_daddr_t));
					if (e1 = VOP_BWRITE(bp))
						return (e1);
				}
			}
			if (depth == 0 && a[1].in_off == 0) {
				off = a[0].in_off;
				daddr = ip->i_ib[off];
				SEGDEC(freesize);
				ip->i_ib[off] = 0;
			}
			if (lbn == lastblock || lbn <= NDADDR)
				--lbn;
			else {
				lbn -= NINDIR(fs);
				if (lbn < lastblock)
					lbn = lastblock;
			}
		}
	}
	UPDATE_SEGUSE;

	/* If truncating the file to 0, update the version number. */
	if (length == 0) {
		LFS_IENTRY(ifp, fs, ip->i_number, bp);
		++ifp->if_version;
		(void) VOP_BWRITE(bp);
	}

#ifdef DIAGNOSTIC
	if (ip->i_blocks < fragstodb(fs, fragsreleased)) {
		printf("lfs_truncate: frag count < 0\n");
		fragsreleased = dbtofrags(fs, ip->i_blocks);
		panic("lfs_truncate: frag count < 0\n");
	}
#endif
	ip->i_blocks -= fragstodb(fs, fragsreleased);
	fs->lfs_bfree +=  fragstodb(fs, fragsreleased);
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	/*
	 * Traverse dirty block list counting number of dirty buffers
	 * that are being deleted out of the cache, so that the lfs_avail
	 * field can be updated.
	 */
	a_released = 0;
	i_released = 0;
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = bp->b_vnbufs.le_next)
		if (bp->b_flags & B_LOCKED) {
			a_released += numfrags(fs, bp->b_bcount);
			/*
			 * XXX
			 * When buffers are created in the cache, their block
			 * number is set equal to their logical block number.
			 * If that is still true, we are assuming that the
			 * blocks are new (not yet on disk) and weren't
			 * counted above.  However, there is a slight chance
			 * that a block's disk address is equal to its logical
			 * block number in which case, we'll get an overcounting
			 * here.
			 */
			if (bp->b_blkno == bp->b_lblkno)
				i_released += numfrags(fs, bp->b_bcount);
		}
	fragsreleased = i_released;
#ifdef DIAGNOSTIC
	if (fragsreleased > dbtofrags(fs, ip->i_blocks)) {
		printf("lfs_inode: Warning! %s\n",
		    "more frags released from inode than are in inode");
		fragsreleased = dbtofrags(fs, ip->i_blocks);
		panic("lfs_inode: Warning.  More frags released\n");
	}
#endif
	fs->lfs_bfree += fragstodb(fs, fragsreleased);
	ip->i_blocks -= fragstodb(fs, fragsreleased);
#ifdef DIAGNOSTIC
	if (length == 0 && ip->i_blocks != 0) {
		printf("lfs_inode: Warning! %s%ld%s\n",
		    "Truncation to zero, but ", ip->i_blocks,
		    " blocks left on inode");
		panic("lfs_inode");
	}
#endif
	fs->lfs_avail += fragstodb(fs, a_released);
	e1 = vinvalbuf(vp, (length > 0) ? V_SAVE : 0, cred, p,
	    0, 0);
	e2 = VOP_UPDATE(vp, &tv, &tv, 0);
	return (e1 ? e1 : e2 ? e2 : 0);
}
