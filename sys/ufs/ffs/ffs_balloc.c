/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
 *
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
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
 *	@(#)ffs_balloc.c	8.8 (Berkeley) 6/16/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

/*
 * Balloc defines the structure of filesystem storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 * This is the allocation strategy for UFS1. Below is
 * the allocation strategy for UFS2.
 */
int
ffs_balloc_ufs1(struct vnode *vp, off_t startoffset, int size,
    struct ucred *cred, int flags, struct buf **bpp)
{
	struct inode *ip;
	struct ufs1_dinode *dp;
	ufs_lbn_t lbn, lastlbn;
	struct fs *fs;
	ufs1_daddr_t nb;
	struct buf *bp, *nbp;
	struct indir indirs[NIADDR + 2];
	int deallocated, osize, nsize, num, i, error;
	ufs2_daddr_t newb;
	ufs1_daddr_t *bap, pref;
	ufs1_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	int unwindidx = -1;
	struct thread *td = curthread;	/* XXX */

	ip = VTOI(vp);
	dp = ip->i_din1;
	fs = ip->i_fs;
	lbn = lblkno(fs, startoffset);
	size = blkoff(fs, startoffset) + size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc_ufs1: blk too big");
	*bpp = NULL;
	if (flags & IO_EXT)
		return (EOPNOTSUPP);
	if (lbn < 0)
		return (EFBIG);

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	lastlbn = lblkno(fs, ip->i_size);
	if (lastlbn < NDADDR && lastlbn < lbn) {
		nb = lastlbn;
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = ffs_realloccg(ip, nb, dp->di_db[nb],
			   ffs_blkpref_ufs1(ip, lastlbn, (int)nb,
			   &dp->di_db[0]), osize, (int)fs->fs_bsize, cred, &bp);
			if (error)
				return (error);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, nb,
				    dbtofsb(fs, bp->b_blkno), dp->di_db[nb],
				    fs->fs_bsize, osize, bp);
			ip->i_size = smalllblktosize(fs, nb + 1);
			dp->di_size = ip->i_size;
			dp->di_db[nb] = dbtofsb(fs, bp->b_blkno);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (flags & IO_SYNC)
				bwrite(bp);
			else
				bawrite(bp);
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {
		if (flags & BA_METAONLY)
			panic("ffs_balloc_ufs1: BA_METAONLY for direct block");
		nb = dp->di_db[lbn];
		if (nb != 0 && ip->i_size >= smalllblktosize(fs, lbn + 1)) {
			error = bread(vp, lbn, fs->fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, nb);
			*bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, lbn, osize, NOCRED, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				bp->b_blkno = fsbtodb(fs, nb);
			} else {
				error = ffs_realloccg(ip, lbn, dp->di_db[lbn],
				    ffs_blkpref_ufs1(ip, lbn, (int)lbn,
				    &dp->di_db[0]), osize, nsize, cred, &bp);
				if (error)
					return (error);
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocdirect(ip, lbn,
					    dbtofsb(fs, bp->b_blkno), nb,
					    nsize, osize, bp);
			}
		} else {
			if (ip->i_size < smalllblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref_ufs1(ip, lbn, (int)lbn, &dp->di_db[0]),
			    nsize, cred, &newb);
			if (error)
				return (error);
			bp = getblk(vp, lbn, nsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & BA_CLRBUF)
				vfs_bio_clrbuf(bp);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, lbn, newb, 0,
				    nsize, 0, bp);
		}
		dp->di_db[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ffs_balloc_ufs1: ufs_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = dp->di_ib[indirs[0].in_off];
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ffs_blkpref_ufs1(ip, lbn, 0, (ufs1_daddr_t *)0);
	        if ((error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    cred, &newb)) != 0)
			return (error);
		nb = newb;
		*allocblk++ = nb;
		bp = getblk(vp, indirs[1].in_lbn, fs->fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(bp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocdirect(ip, NDADDR + indirs[0].in_off,
			    newb, 0, fs->fs_bsize, 0, bp);
			bdwrite(bp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if (DOINGASYNC(vp))
				bdwrite(bp);
			else if ((error = bwrite(bp)) != 0)
				goto fail;
		}
		allocib = &dp->di_ib[indirs[0].in_off];
		*allocib = nb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs1_daddr_t *)bp->b_data;
		nb = bap[indirs[i].in_off];
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			bqrelse(bp);
			continue;
		}
		if (pref == 0)
			pref = ffs_blkpref_ufs1(ip, lbn, 0, (ufs1_daddr_t *)0);
		if ((error =
		    ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred, &newb)) != 0) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(nbp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocindir_meta(nbp, ip, bp,
			    indirs[i - 1].in_off, nb);
			bdwrite(nbp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if ((error = bwrite(nbp)) != 0) {
				brelse(bp);
				goto fail;
			}
		}
		bap[indirs[i - 1].in_off] = nb;
		if (allocib == NULL && unwindidx < 0)
			unwindidx = i - 1;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & IO_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
	}
	/*
	 * If asked only for the indirect block, then return it.
	 */
	if (flags & BA_METAONLY) {
		*bpp = bp;
		return (0);
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ffs_blkpref_ufs1(ip, lbn, indirs[i].in_off, &bap[0]);
		error = ffs_alloc(ip,
		    lbn, pref, (int)fs->fs_bsize, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		if (flags & BA_CLRBUF)
			vfs_bio_clrbuf(nbp);
		if (DOINGSOFTDEP(vp))
			softdep_setup_allocindir_page(ip, lbn, bp,
			    indirs[i].in_off, nb, 0, nbp);
		bap[indirs[i].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & IO_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & BA_CLRBUF) {
		error = bread(vp, lbn, (int)fs->fs_bsize, NOCRED, &nbp);
		if (error) {
			brelse(nbp);
			goto fail;
		}
	} else {
		nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
	}
	*bpp = nbp;
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 * We have to fsync the file before we start to get rid of all
	 * of its dependencies so that we do not leave them dangling.
	 * We have to sync it at the end so that the soft updates code
	 * does not find any untracked changes. Although this is really
	 * slow, running out of disk space is not expected to be a common
	 * occurence. The error return from fsync is ignored as we already
	 * have an error to return to the user.
	 */
	(void) VOP_FSYNC(vp, cred, MNT_WAIT, td);
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(fs, ip->i_devvp, *blkp, fs->fs_bsize, ip->i_number);
		deallocated += fs->fs_bsize;
	}
	if (allocib != NULL) {
		*allocib = 0;
	} else if (unwindidx >= 0) {
		int r;

		r = bread(vp, indirs[unwindidx].in_lbn, 
		    (int)fs->fs_bsize, NOCRED, &bp);
		if (r) {
			panic("Could not unwind indirect block, error %d", r);
			brelse(bp);
		} else {
			bap = (ufs1_daddr_t *)bp->b_data;
			bap[indirs[unwindidx].in_off] = 0;
			if (flags & IO_SYNC) {
				bwrite(bp);
			} else {
				if (bp->b_bufsize == fs->fs_bsize)
					bp->b_flags |= B_CLUSTEROK;
				bdwrite(bp);
			}
		}
	}
	if (deallocated) {
#ifdef QUOTA
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void) chkdq(ip, -btodb(deallocated), cred, FORCE);
#endif
		dp->di_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	(void) VOP_FSYNC(vp, cred, MNT_WAIT, td);
	return (error);
}

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 * This is the allocation strategy for UFS2. Above is
 * the allocation strategy for UFS1.
 */
int
ffs_balloc_ufs2(struct vnode *vp, off_t startoffset, int size,
    struct ucred *cred, int flags, struct buf **bpp)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	ufs_lbn_t lbn, lastlbn;
	struct fs *fs;
	struct buf *bp, *nbp;
	struct indir indirs[NIADDR + 2];
	ufs2_daddr_t nb, newb, *bap, pref;
	ufs2_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	int deallocated, osize, nsize, num, i, error;
	int unwindidx = -1;
	struct thread *td = curthread;	/* XXX */

	ip = VTOI(vp);
	dp = ip->i_din2;
	fs = ip->i_fs;
	lbn = lblkno(fs, startoffset);
	size = blkoff(fs, startoffset) + size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc_ufs2: blk too big");
	*bpp = NULL;
	if (lbn < 0)
		return (EFBIG);

	/*
	 * Check for allocating external data.
	 */
	if (flags & IO_EXT) {
		if (lbn >= NXADDR)
			return (EFBIG);
		/*
		 * If the next write will extend the data into a new block,
		 * and the data is currently composed of a fragment
		 * this fragment has to be extended to be a full block.
		 */
		lastlbn = lblkno(fs, dp->di_extsize);
		if (lastlbn < lbn) {
			nb = lastlbn;
			osize = sblksize(fs, dp->di_extsize, nb);
			if (osize < fs->fs_bsize && osize > 0) {
				error = ffs_realloccg(ip, -1 - nb,
				    dp->di_extb[nb],
				    ffs_blkpref_ufs2(ip, lastlbn, (int)nb,
				    &dp->di_extb[0]), osize,
				    (int)fs->fs_bsize, cred, &bp);
				if (error)
					return (error);
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocext(ip, nb,
					    dbtofsb(fs, bp->b_blkno),
					    dp->di_extb[nb],
					    fs->fs_bsize, osize, bp);
				dp->di_extsize = smalllblktosize(fs, nb + 1);
				dp->di_extb[nb] = dbtofsb(fs, bp->b_blkno);
				bp->b_xflags |= BX_ALTDATA;
				ip->i_flag |= IN_CHANGE | IN_UPDATE;
				if (flags & IO_SYNC)
					bwrite(bp);
				else
					bawrite(bp);
			}
		}
		/*
		 * All blocks are direct blocks
		 */
		if (flags & BA_METAONLY)
			panic("ffs_balloc_ufs2: BA_METAONLY for ext block");
		nb = dp->di_extb[lbn];
		if (nb != 0 && dp->di_extsize >= smalllblktosize(fs, lbn + 1)) {
			error = bread(vp, -1 - lbn, fs->fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, nb);
			bp->b_xflags |= BX_ALTDATA;
			*bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, dp->di_extsize));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, -1 - lbn, osize, NOCRED, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				bp->b_blkno = fsbtodb(fs, nb);
				bp->b_xflags |= BX_ALTDATA;
			} else {
				error = ffs_realloccg(ip, -1 - lbn,
				    dp->di_extb[lbn],
				    ffs_blkpref_ufs2(ip, lbn, (int)lbn,
				    &dp->di_extb[0]), osize, nsize, cred, &bp);
				if (error)
					return (error);
				bp->b_xflags |= BX_ALTDATA;
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocext(ip, lbn,
					    dbtofsb(fs, bp->b_blkno), nb,
					    nsize, osize, bp);
			}
		} else {
			if (dp->di_extsize < smalllblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip, lbn,
			   ffs_blkpref_ufs2(ip, lbn, (int)lbn, &dp->di_extb[0]),
			   nsize, cred, &newb);
			if (error)
				return (error);
			bp = getblk(vp, -1 - lbn, nsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			bp->b_xflags |= BX_ALTDATA;
			if (flags & BA_CLRBUF)
				vfs_bio_clrbuf(bp);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocext(ip, lbn, newb, 0,
				    nsize, 0, bp);
		}
		dp->di_extb[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	lastlbn = lblkno(fs, ip->i_size);
	if (lastlbn < NDADDR && lastlbn < lbn) {
		nb = lastlbn;
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = ffs_realloccg(ip, nb, dp->di_db[nb],
				ffs_blkpref_ufs2(ip, lastlbn, (int)nb,
				    &dp->di_db[0]), osize, (int)fs->fs_bsize,
				    cred, &bp);
			if (error)
				return (error);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, nb,
				    dbtofsb(fs, bp->b_blkno),
				    dp->di_db[nb],
				    fs->fs_bsize, osize, bp);
			ip->i_size = smalllblktosize(fs, nb + 1);
			dp->di_size = ip->i_size;
			dp->di_db[nb] = dbtofsb(fs, bp->b_blkno);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (flags & IO_SYNC)
				bwrite(bp);
			else
				bawrite(bp);
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {
		if (flags & BA_METAONLY)
			panic("ffs_balloc_ufs2: BA_METAONLY for direct block");
		nb = dp->di_db[lbn];
		if (nb != 0 && ip->i_size >= smalllblktosize(fs, lbn + 1)) {
			error = bread(vp, lbn, fs->fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, nb);
			*bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, lbn, osize, NOCRED, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				bp->b_blkno = fsbtodb(fs, nb);
			} else {
				error = ffs_realloccg(ip, lbn, dp->di_db[lbn],
				    ffs_blkpref_ufs2(ip, lbn, (int)lbn,
				       &dp->di_db[0]), osize, nsize, cred, &bp);
				if (error)
					return (error);
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocdirect(ip, lbn,
					    dbtofsb(fs, bp->b_blkno), nb,
					    nsize, osize, bp);
			}
		} else {
			if (ip->i_size < smalllblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref_ufs2(ip, lbn, (int)lbn,
				&dp->di_db[0]), nsize, cred, &newb);
			if (error)
				return (error);
			bp = getblk(vp, lbn, nsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & BA_CLRBUF)
				vfs_bio_clrbuf(bp);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, lbn, newb, 0,
				    nsize, 0, bp);
		}
		dp->di_db[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ffs_balloc_ufs2: ufs_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = dp->di_ib[indirs[0].in_off];
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ffs_blkpref_ufs2(ip, lbn, 0, (ufs2_daddr_t *)0);
	        if ((error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    cred, &newb)) != 0)
			return (error);
		nb = newb;
		*allocblk++ = nb;
		bp = getblk(vp, indirs[1].in_lbn, fs->fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(bp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocdirect(ip, NDADDR + indirs[0].in_off,
			    newb, 0, fs->fs_bsize, 0, bp);
			bdwrite(bp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if (DOINGASYNC(vp))
				bdwrite(bp);
			else if ((error = bwrite(bp)) != 0)
				goto fail;
		}
		allocib = &dp->di_ib[indirs[0].in_off];
		*allocib = nb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs2_daddr_t *)bp->b_data;
		nb = bap[indirs[i].in_off];
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			bqrelse(bp);
			continue;
		}
		if (pref == 0)
			pref = ffs_blkpref_ufs2(ip, lbn, 0, (ufs2_daddr_t *)0);
		if ((error =
		    ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred, &newb)) != 0) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(nbp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocindir_meta(nbp, ip, bp,
			    indirs[i - 1].in_off, nb);
			bdwrite(nbp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if ((error = bwrite(nbp)) != 0) {
				brelse(bp);
				goto fail;
			}
		}
		bap[indirs[i - 1].in_off] = nb;
		if (allocib == NULL && unwindidx < 0)
			unwindidx = i - 1;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & IO_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
	}
	/*
	 * If asked only for the indirect block, then return it.
	 */
	if (flags & BA_METAONLY) {
		*bpp = bp;
		return (0);
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ffs_blkpref_ufs2(ip, lbn, indirs[i].in_off, &bap[0]);
		error = ffs_alloc(ip,
		    lbn, pref, (int)fs->fs_bsize, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		if (flags & BA_CLRBUF)
			vfs_bio_clrbuf(nbp);
		if (DOINGSOFTDEP(vp))
			softdep_setup_allocindir_page(ip, lbn, bp,
			    indirs[i].in_off, nb, 0, nbp);
		bap[indirs[i].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & IO_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & BA_CLRBUF) {
		error = bread(vp, lbn, (int)fs->fs_bsize, NOCRED, &nbp);
		if (error) {
			brelse(nbp);
			goto fail;
		}
	} else {
		nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
	}
	*bpp = nbp;
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 * We have to fsync the file before we start to get rid of all
	 * of its dependencies so that we do not leave them dangling.
	 * We have to sync it at the end so that the soft updates code
	 * does not find any untracked changes. Although this is really
	 * slow, running out of disk space is not expected to be a common
	 * occurence. The error return from fsync is ignored as we already
	 * have an error to return to the user.
	 */
	(void) VOP_FSYNC(vp, cred, MNT_WAIT, td);
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(fs, ip->i_devvp, *blkp, fs->fs_bsize, ip->i_number);
		deallocated += fs->fs_bsize;
	}
	if (allocib != NULL) {
		*allocib = 0;
	} else if (unwindidx >= 0) {
		int r;

		r = bread(vp, indirs[unwindidx].in_lbn, 
		    (int)fs->fs_bsize, NOCRED, &bp);
		if (r) {
			panic("Could not unwind indirect block, error %d", r);
			brelse(bp);
		} else {
			bap = (ufs2_daddr_t *)bp->b_data;
			bap[indirs[unwindidx].in_off] = 0;
			if (flags & IO_SYNC) {
				bwrite(bp);
			} else {
				if (bp->b_bufsize == fs->fs_bsize)
					bp->b_flags |= B_CLUSTEROK;
				bdwrite(bp);
			}
		}
	}
	if (deallocated) {
#ifdef QUOTA
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void) chkdq(ip, -btodb(deallocated), cred, FORCE);
#endif
		dp->di_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	(void) VOP_FSYNC(vp, cred, MNT_WAIT, td);
	return (error);
}
