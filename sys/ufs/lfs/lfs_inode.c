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
 *	@(#)lfs_inode.c	8.5 (Berkeley) 12/30/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

int
lfs_init()
{
	return (ufs_init());
}

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

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);
	if ((ip->i_flag &
	    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0)
		return (0);
	if (ip->i_flag & IN_ACCESS)
		ip->i_atime.ts_sec = ap->a_access->tv_sec;
	if (ip->i_flag & IN_UPDATE) {
		ip->i_mtime.ts_sec = ap->a_modify->tv_sec;
		(ip)->i_modrev++;
	}
	if (ip->i_flag & IN_CHANGE)
		ip->i_ctime.ts_sec = time.tv_sec;
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);

	if (!(ip->i_flag & IN_MODIFIED))
		++(VFSTOUFS(vp->v_mount)->um_lfs->lfs_uinodes);
	ip->i_flag |= IN_MODIFIED;

	/* If sync, push back the vnode and any dirty blocks it may have. */
	return (ap->a_waitfor & LFS_SYNC ? lfs_vflush(vp) : 0);
}

/* Update segment usage information when removing a block. */
#define UPDATE_SEGUSE \
	if (lastseg != -1) { \
		LFS_SEGENTRY(sup, fs, lastseg, sup_bp); \
		if ((num << fs->lfs_bshift) > sup->su_nbytes) \
			panic("lfs_truncate: negative bytes in segment %d\n", \
			    lastseg); \
		sup->su_nbytes -= num << fs->lfs_bshift; \
		e1 = VOP_BWRITE(sup_bp); \
		blocksreleased += num; \
	}

#define SEGDEC { \
	if (daddr != 0) { \
		if (lastseg != (seg = datosn(fs, daddr))) { \
			UPDATE_SEGUSE; \
			num = 1; \
			lastseg = seg; \
		} else \
			++num; \
	} \
}

/*
 * Truncate the inode ip to at most length size.  Update segment usage
 * table information.
 */
/* ARGSUSED */
int
lfs_truncate(ap)
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct indir *inp;
	register int i;
	register daddr_t *daddrp;
	register struct vnode *vp = ap->a_vp;
	off_t length = ap->a_length;
	struct buf *bp, *sup_bp;
	struct timeval tv;
	struct ifile *ifp;
	struct inode *ip;
	struct lfs *fs;
	struct indir a[NIADDR + 2], a_end[NIADDR + 2];
	SEGUSE *sup;
	daddr_t daddr, lastblock, lbn, olastblock;
	long off, a_released, blocksreleased, i_released;
	int e1, e2, depth, lastseg, num, offset, seg, size;

	ip = VTOI(vp);
	tv = time;
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
	 * because of subsequent file growth.
	 */
	offset = blkoff(fs, length);
	if (offset == 0)
		ip->i_size = length;
	else {
		lbn = lblkno(fs, length);
#ifdef QUOTA
		if (e1 = getinoquota(ip))
			return (e1);
#endif	
		if (e1 = bread(vp, lbn, fs->lfs_bsize, NOCRED, &bp))
			return (e1);
		ip->i_size = length;
		size = blksize(fs);
		(void)vnode_pager_uncache(vp);
		bzero((char *)bp->b_data + offset, (u_int)(size - offset));
		allocbuf(bp, size);
		if (e1 = VOP_BWRITE(bp))
			return (e1);
	}
	/*
	 * Modify sup->su_nbyte counters for each deleted block; keep track
	 * of number of blocks removed for ip->i_blocks.
	 */
	blocksreleased = 0;
	num = 0;
	lastseg = -1;

	for (lbn = olastblock; lbn >= lastblock;) {
		/* XXX use run length from bmap array to make this faster */
		ufs_bmaparray(vp, lbn, &daddr, a, &depth, NULL);
		if (lbn == olastblock)
			for (i = NIADDR + 2; i--;)
				a_end[i] = a[i];
		switch (depth) {
		case 0:				/* Direct block. */
			daddr = ip->i_db[lbn];
			SEGDEC;
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
				daddrp = (daddr_t *)bp->b_data + inp->in_off;
				for (i = inp->in_off;
				    i++ <= a_end[depth].in_off;) {
					daddr = *daddrp++;
					SEGDEC;
				}
				a_end[depth].in_off = NINDIR(fs) - 1;
				if (inp->in_off == 0)
					brelse (bp);
				else {
					bzero((daddr_t *)bp->b_data +
					    inp->in_off, fs->lfs_bsize - 
					    inp->in_off * sizeof(daddr_t));
					if (e1 = VOP_BWRITE(bp)) 
						return (e1);
				}
			}
			if (depth == 0 && a[1].in_off == 0) {
				off = a[0].in_off;
				daddr = ip->i_ib[off];
				SEGDEC;
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
	if (ip->i_blocks < fsbtodb(fs, blocksreleased)) {
		printf("lfs_truncate: block count < 0\n");
		blocksreleased = ip->i_blocks;
	}
#endif
	ip->i_blocks -= fsbtodb(fs, blocksreleased);
	fs->lfs_bfree +=  fsbtodb(fs, blocksreleased);
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
			++a_released;
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
				++i_released;
		}
	blocksreleased = fsbtodb(fs, i_released);
#ifdef DIAGNOSTIC
	if (blocksreleased > ip->i_blocks) {
		printf("lfs_inode: Warning! %s\n",
		    "more blocks released from inode than are in inode");
		blocksreleased = ip->i_blocks;
	}
#endif
	fs->lfs_bfree += blocksreleased;
	ip->i_blocks -= blocksreleased;
#ifdef DIAGNOSTIC
	if (length == 0 && ip->i_blocks != 0)
		printf("lfs_inode: Warning! %s%d%s\n",
		    "Truncation to zero, but ", ip->i_blocks,
		    " blocks left on inode");
#endif
	fs->lfs_avail += fsbtodb(fs, a_released);
	e1 = vinvalbuf(vp, (length > 0) ? V_SAVE : 0, ap->a_cred, ap->a_p,
	    0, 0); 
	e2 = VOP_UPDATE(vp, &tv, &tv, 0);
	return (e1 ? e1 : e2 ? e2 : 0);
}
