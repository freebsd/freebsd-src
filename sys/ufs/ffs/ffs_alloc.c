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
 *	@(#)ffs_alloc.c	8.18 (Berkeley) 5/26/95
 * $FreeBSD$
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

typedef ufs_daddr_t allocfcn_t __P((struct inode *ip, int cg, ufs_daddr_t bpref,
				  int size));

static ufs_daddr_t ffs_alloccg __P((struct inode *, int, ufs_daddr_t, int));
static ufs_daddr_t
	      ffs_alloccgblk __P((struct inode *, struct buf *, ufs_daddr_t));
#ifdef DIAGNOSTIC
static int	ffs_checkblk __P((struct inode *, ufs_daddr_t, long));
#endif
static ufs_daddr_t ffs_clusteralloc __P((struct inode *, int, ufs_daddr_t,
	    int));
static ino_t	ffs_dirpref __P((struct inode *));
static ufs_daddr_t ffs_fragextend __P((struct inode *, int, long, int, int));
static void	ffs_fserr __P((struct fs *, u_int, char *));
static u_long	ffs_hashalloc
		    __P((struct inode *, int, long, int, allocfcn_t *));
static ino_t	ffs_nodealloccg __P((struct inode *, int, ufs_daddr_t, int));
static ufs_daddr_t ffs_mapsearch __P((struct fs *, struct cg *, ufs_daddr_t,
	    int));

/*
 * Allocate a block in the file system.
 *
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following heirarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 */
int
ffs_alloc(ip, lbn, bpref, size, cred, bnp)
	register struct inode *ip;
	ufs_daddr_t lbn, bpref;
	int size;
	struct ucred *cred;
	ufs_daddr_t *bnp;
{
	register struct fs *fs;
	ufs_daddr_t bno;
	int cg;
#ifdef QUOTA
	int error;
#endif

	*bnp = 0;
	fs = ip->i_fs;
#ifdef DIAGNOSTIC
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("dev = %s, bsize = %ld, size = %d, fs = %s\n",
		    devtoname(ip->i_dev), (long)fs->fs_bsize, size,
		    fs->fs_fsmnt);
		panic("ffs_alloc: bad size");
	}
	if (cred == NOCRED)
		panic("ffs_alloc: missing credential");
#endif /* DIAGNOSTIC */
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (suser_xxx(cred, NULL, PRISON_ROOT) &&
	    freespace(fs, fs->fs_minfree) - numfrags(fs, size) < 0)
		goto nospace;
#ifdef QUOTA
	error = chkdq(ip, (long)btodb(size), cred, 0);
	if (error)
		return (error);
#endif
	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = ino_to_cg(fs, ip->i_number);
	else
		cg = dtog(fs, bpref);
	bno = (ufs_daddr_t)ffs_hashalloc(ip, cg, (long)bpref, size,
					 ffs_alloccg);
	if (bno > 0) {
		ip->i_blocks += btodb(size);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bnp = bno;
		return (0);
	}
#ifdef QUOTA
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, (long)-btodb(size), cred, FORCE);
#endif
nospace:
	ffs_fserr(fs, cred->cr_uid, "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Reallocate a fragment to a bigger size
 *
 * The number and size of the old block is given, and a preference
 * and new size is also specified. The allocator attempts to extend
 * the original block. Failing that, the regular block allocator is
 * invoked to get an appropriate block.
 */
int
ffs_realloccg(ip, lbprev, bpref, osize, nsize, cred, bpp)
	register struct inode *ip;
	ufs_daddr_t lbprev;
	ufs_daddr_t bpref;
	int osize, nsize;
	struct ucred *cred;
	struct buf **bpp;
{
	register struct fs *fs;
	struct buf *bp;
	int cg, request, error;
	ufs_daddr_t bprev, bno;

	*bpp = 0;
	fs = ip->i_fs;
#ifdef DIAGNOSTIC
	if (ITOV(ip)->v_mount->mnt_kern_flag & MNTK_SUSPENDED)
		panic("ffs_realloccg: allocation on suspended filesystem");
	if ((u_int)osize > fs->fs_bsize || fragoff(fs, osize) != 0 ||
	    (u_int)nsize > fs->fs_bsize || fragoff(fs, nsize) != 0) {
		printf(
		"dev = %s, bsize = %ld, osize = %d, nsize = %d, fs = %s\n",
		    devtoname(ip->i_dev), (long)fs->fs_bsize, osize,
		    nsize, fs->fs_fsmnt);
		panic("ffs_realloccg: bad size");
	}
	if (cred == NOCRED)
		panic("ffs_realloccg: missing credential");
#endif /* DIAGNOSTIC */
	if (suser_xxx(cred, NULL, PRISON_ROOT) &&
	    freespace(fs, fs->fs_minfree) -  numfrags(fs, nsize - osize) < 0)
		goto nospace;
	if ((bprev = ip->i_db[lbprev]) == 0) {
		printf("dev = %s, bsize = %ld, bprev = %ld, fs = %s\n",
		    devtoname(ip->i_dev), (long)fs->fs_bsize, (long)bprev,
		    fs->fs_fsmnt);
		panic("ffs_realloccg: bad bprev");
	}
	/*
	 * Allocate the extra space in the buffer.
	 */
	error = bread(ITOV(ip), lbprev, osize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}

	if( bp->b_blkno == bp->b_lblkno) {
		if( lbprev >= NDADDR)
			panic("ffs_realloccg: lbprev out of range");
		bp->b_blkno = fsbtodb(fs, bprev);
	}

#ifdef QUOTA
	error = chkdq(ip, (long)btodb(nsize - osize), cred, 0);
	if (error) {
		brelse(bp);
		return (error);
	}
#endif
	/*
	 * Check for extension in the existing location.
	 */
	cg = dtog(fs, bprev);
	bno = ffs_fragextend(ip, cg, (long)bprev, osize, nsize);
	if (bno) {
		if (bp->b_blkno != fsbtodb(fs, bno))
			panic("ffs_realloccg: bad blockno");
		ip->i_blocks += btodb(nsize - osize);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		allocbuf(bp, nsize);
		bp->b_flags |= B_DONE;
		bzero((char *)bp->b_data + osize, (u_int)nsize - osize);
		*bpp = bp;
		return (0);
	}
	/*
	 * Allocate a new disk location.
	 */
	if (bpref >= fs->fs_size)
		bpref = 0;
	switch ((int)fs->fs_optim) {
	case FS_OPTSPACE:
		/*
		 * Allocate an exact sized fragment. Although this makes
		 * best use of space, we will waste time relocating it if
		 * the file continues to grow. If the fragmentation is
		 * less than half of the minimum free reserve, we choose
		 * to begin optimizing for time.
		 */
		request = nsize;
		if (fs->fs_minfree <= 5 ||
		    fs->fs_cstotal.cs_nffree >
		    (off_t)fs->fs_dsize * fs->fs_minfree / (2 * 100))
			break;
		log(LOG_NOTICE, "%s: optimization changed from SPACE to TIME\n",
			fs->fs_fsmnt);
		fs->fs_optim = FS_OPTTIME;
		break;
	case FS_OPTTIME:
		/*
		 * At this point we have discovered a file that is trying to
		 * grow a small fragment to a larger fragment. To save time,
		 * we allocate a full sized block, then free the unused portion.
		 * If the file continues to grow, the `ffs_fragextend' call
		 * above will be able to grow it in place without further
		 * copying. If aberrant programs cause disk fragmentation to
		 * grow within 2% of the free reserve, we choose to begin
		 * optimizing for space.
		 */
		request = fs->fs_bsize;
		if (fs->fs_cstotal.cs_nffree <
		    (off_t)fs->fs_dsize * (fs->fs_minfree - 2) / 100)
			break;
		log(LOG_NOTICE, "%s: optimization changed from TIME to SPACE\n",
			fs->fs_fsmnt);
		fs->fs_optim = FS_OPTSPACE;
		break;
	default:
		printf("dev = %s, optim = %ld, fs = %s\n",
		    devtoname(ip->i_dev), (long)fs->fs_optim, fs->fs_fsmnt);
		panic("ffs_realloccg: bad optim");
		/* NOTREACHED */
	}
	bno = (ufs_daddr_t)ffs_hashalloc(ip, cg, (long)bpref, request,
					 ffs_alloccg);
	if (bno > 0) {
		bp->b_blkno = fsbtodb(fs, bno);
		if (!DOINGSOFTDEP(ITOV(ip)))
			ffs_blkfree(ip, bprev, (long)osize);
		if (nsize < request)
			ffs_blkfree(ip, bno + numfrags(fs, nsize),
			    (long)(request - nsize));
		ip->i_blocks += btodb(nsize - osize);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		allocbuf(bp, nsize);
		bp->b_flags |= B_DONE;
		bzero((char *)bp->b_data + osize, (u_int)nsize - osize);
		*bpp = bp;
		return (0);
	}
#ifdef QUOTA
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, (long)-btodb(nsize - osize), cred, FORCE);
#endif
	brelse(bp);
nospace:
	/*
	 * no space available
	 */
	ffs_fserr(fs, cred->cr_uid, "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Reallocate a sequence of blocks into a contiguous sequence of blocks.
 *
 * The vnode and an array of buffer pointers for a range of sequential
 * logical blocks to be made contiguous is given. The allocator attempts
 * to find a range of sequential blocks starting as close as possible to
 * an fs_rotdelay offset from the end of the allocation for the logical
 * block immediately preceding the current range. If successful, the
 * physical block numbers in the buffer pointers and in the inode are
 * changed to reflect the new allocation. If unsuccessful, the allocation
 * is left unchanged. The success in doing the reallocation is returned.
 * Note that the error return is not reflected back to the user. Rather
 * the previous block allocation will be used.
 */

SYSCTL_NODE(_vfs, OID_AUTO, ffs, CTLFLAG_RW, 0, "FFS filesystem");

static int doasyncfree = 1;
SYSCTL_INT(_vfs_ffs, OID_AUTO, doasyncfree, CTLFLAG_RW, &doasyncfree, 0, "");

static int doreallocblks = 1;
SYSCTL_INT(_vfs_ffs, OID_AUTO, doreallocblks, CTLFLAG_RW, &doreallocblks, 0, "");

#ifdef DEBUG
static volatile int prtrealloc = 0;
#endif

int
ffs_reallocblks(ap)
	struct vop_reallocblks_args /* {
		struct vnode *a_vp;
		struct cluster_save *a_buflist;
	} */ *ap;
{
	struct fs *fs;
	struct inode *ip;
	struct vnode *vp;
	struct buf *sbp, *ebp;
	ufs_daddr_t *bap, *sbap, *ebap = 0;
	struct cluster_save *buflist;
	ufs_daddr_t start_lbn, end_lbn, soff, newblk, blkno;
	struct indir start_ap[NIADDR + 1], end_ap[NIADDR + 1], *idp;
	int i, len, start_lvl, end_lvl, pref, ssize;

	if (doreallocblks == 0)
		return (ENOSPC);
	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_fs;
	if (fs->fs_contigsumsize <= 0)
		return (ENOSPC);
	buflist = ap->a_buflist;
	len = buflist->bs_nchildren;
	start_lbn = buflist->bs_children[0]->b_lblkno;
	end_lbn = start_lbn + len - 1;
#ifdef DIAGNOSTIC
	for (i = 0; i < len; i++)
		if (!ffs_checkblk(ip,
		   dbtofsb(fs, buflist->bs_children[i]->b_blkno), fs->fs_bsize))
			panic("ffs_reallocblks: unallocated block 1");
	for (i = 1; i < len; i++)
		if (buflist->bs_children[i]->b_lblkno != start_lbn + i)
			panic("ffs_reallocblks: non-logical cluster");
	blkno = buflist->bs_children[0]->b_blkno;
	ssize = fsbtodb(fs, fs->fs_frag);
	for (i = 1; i < len - 1; i++)
		if (buflist->bs_children[i]->b_blkno != blkno + (i * ssize))
			panic("ffs_reallocblks: non-physical cluster %d", i);
#endif
	/*
	 * If the latest allocation is in a new cylinder group, assume that
	 * the filesystem has decided to move and do not force it back to
	 * the previous cylinder group.
	 */
	if (dtog(fs, dbtofsb(fs, buflist->bs_children[0]->b_blkno)) !=
	    dtog(fs, dbtofsb(fs, buflist->bs_children[len - 1]->b_blkno)))
		return (ENOSPC);
	if (ufs_getlbns(vp, start_lbn, start_ap, &start_lvl) ||
	    ufs_getlbns(vp, end_lbn, end_ap, &end_lvl))
		return (ENOSPC);
	/*
	 * Get the starting offset and block map for the first block.
	 */
	if (start_lvl == 0) {
		sbap = &ip->i_db[0];
		soff = start_lbn;
	} else {
		idp = &start_ap[start_lvl - 1];
		if (bread(vp, idp->in_lbn, (int)fs->fs_bsize, NOCRED, &sbp)) {
			brelse(sbp);
			return (ENOSPC);
		}
		sbap = (ufs_daddr_t *)sbp->b_data;
		soff = idp->in_off;
	}
	/*
	 * Find the preferred location for the cluster.
	 */
	pref = ffs_blkpref(ip, start_lbn, soff, sbap);
	/*
	 * If the block range spans two block maps, get the second map.
	 */
	if (end_lvl == 0 || (idp = &end_ap[end_lvl - 1])->in_off + 1 >= len) {
		ssize = len;
	} else {
#ifdef DIAGNOSTIC
		if (start_ap[start_lvl-1].in_lbn == idp->in_lbn)
			panic("ffs_reallocblk: start == end");
#endif
		ssize = len - (idp->in_off + 1);
		if (bread(vp, idp->in_lbn, (int)fs->fs_bsize, NOCRED, &ebp))
			goto fail;
		ebap = (ufs_daddr_t *)ebp->b_data;
	}
	/*
	 * Search the block map looking for an allocation of the desired size.
	 */
	if ((newblk = (ufs_daddr_t)ffs_hashalloc(ip, dtog(fs, pref), (long)pref,
	    len, ffs_clusteralloc)) == 0)
		goto fail;
	/*
	 * We have found a new contiguous block.
	 *
	 * First we have to replace the old block pointers with the new
	 * block pointers in the inode and indirect blocks associated
	 * with the file.
	 */
#ifdef DEBUG
	if (prtrealloc)
		printf("realloc: ino %d, lbns %d-%d\n\told:", ip->i_number,
		    start_lbn, end_lbn);
#endif
	blkno = newblk;
	for (bap = &sbap[soff], i = 0; i < len; i++, blkno += fs->fs_frag) {
		if (i == ssize) {
			bap = ebap;
			soff = -i;
		}
#ifdef DIAGNOSTIC
		if (!ffs_checkblk(ip,
		   dbtofsb(fs, buflist->bs_children[i]->b_blkno), fs->fs_bsize))
			panic("ffs_reallocblks: unallocated block 2");
		if (dbtofsb(fs, buflist->bs_children[i]->b_blkno) != *bap)
			panic("ffs_reallocblks: alloc mismatch");
#endif
#ifdef DEBUG
		if (prtrealloc)
			printf(" %d,", *bap);
#endif
		if (DOINGSOFTDEP(vp)) {
			if (sbap == &ip->i_db[0] && i < ssize)
				softdep_setup_allocdirect(ip, start_lbn + i,
				    blkno, *bap, fs->fs_bsize, fs->fs_bsize,
				    buflist->bs_children[i]);
			else
				softdep_setup_allocindir_page(ip, start_lbn + i,
				    i < ssize ? sbp : ebp, soff + i, blkno,
				    *bap, buflist->bs_children[i]);
		}
		*bap++ = blkno;
	}
	/*
	 * Next we must write out the modified inode and indirect blocks.
	 * For strict correctness, the writes should be synchronous since
	 * the old block values may have been written to disk. In practise
	 * they are almost never written, but if we are concerned about
	 * strict correctness, the `doasyncfree' flag should be set to zero.
	 *
	 * The test on `doasyncfree' should be changed to test a flag
	 * that shows whether the associated buffers and inodes have
	 * been written. The flag should be set when the cluster is
	 * started and cleared whenever the buffer or inode is flushed.
	 * We can then check below to see if it is set, and do the
	 * synchronous write only when it has been cleared.
	 */
	if (sbap != &ip->i_db[0]) {
		if (doasyncfree)
			bdwrite(sbp);
		else
			bwrite(sbp);
	} else {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (!doasyncfree)
			UFS_UPDATE(vp, 1);
	}
	if (ssize < len) {
		if (doasyncfree)
			bdwrite(ebp);
		else
			bwrite(ebp);
	}
	/*
	 * Last, free the old blocks and assign the new blocks to the buffers.
	 */
#ifdef DEBUG
	if (prtrealloc)
		printf("\n\tnew:");
#endif
	for (blkno = newblk, i = 0; i < len; i++, blkno += fs->fs_frag) {
		if (!DOINGSOFTDEP(vp))
			ffs_blkfree(ip,
			    dbtofsb(fs, buflist->bs_children[i]->b_blkno),
			    fs->fs_bsize);
		buflist->bs_children[i]->b_blkno = fsbtodb(fs, blkno);
#ifdef DIAGNOSTIC
		if (!ffs_checkblk(ip,
		   dbtofsb(fs, buflist->bs_children[i]->b_blkno), fs->fs_bsize))
			panic("ffs_reallocblks: unallocated block 3");
#endif
#ifdef DEBUG
		if (prtrealloc)
			printf(" %d,", blkno);
#endif
	}
#ifdef DEBUG
	if (prtrealloc) {
		prtrealloc--;
		printf("\n");
	}
#endif
	return (0);

fail:
	if (ssize < len)
		brelse(ebp);
	if (sbap != &ip->i_db[0])
		brelse(sbp);
	return (ENOSPC);
}

/*
 * Allocate an inode in the file system.
 *
 * If allocating a directory, use ffs_dirpref to select the inode.
 * If allocating in a directory, the following hierarchy is followed:
 *   1) allocate the preferred inode.
 *   2) allocate an inode in the same cylinder group.
 *   3) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 * If no inode preference is given the following heirarchy is used
 * to allocate an inode:
 *   1) allocate an inode in cylinder group 0.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 */
int
ffs_valloc(pvp, mode, cred, vpp)
	struct vnode *pvp;
	int mode;
	struct ucred *cred;
	struct vnode **vpp;
{
	register struct inode *pip;
	register struct fs *fs;
	register struct inode *ip;
	ino_t ino, ipref;
	int cg, error;

	*vpp = NULL;
	pip = VTOI(pvp);
	fs = pip->i_fs;
	if (fs->fs_cstotal.cs_nifree == 0)
		goto noinodes;

	if ((mode & IFMT) == IFDIR)
		ipref = ffs_dirpref(pip);
	else
		ipref = pip->i_number;
	if (ipref >= fs->fs_ncg * fs->fs_ipg)
		ipref = 0;
	cg = ino_to_cg(fs, ipref);
	/*
	 * Track number of dirs created one after another
	 * in a same cg without intervening by files.
	 */
	if ((mode & IFMT) == IFDIR) {
		if (fs->fs_contigdirs[cg] < 255)
			fs->fs_contigdirs[cg]++;
	} else {
		if (fs->fs_contigdirs[cg] > 0)
			fs->fs_contigdirs[cg]--;
	}
	ino = (ino_t)ffs_hashalloc(pip, cg, (long)ipref, mode,
					(allocfcn_t *)ffs_nodealloccg);
	if (ino == 0)
		goto noinodes;
	error = VFS_VGET(pvp->v_mount, ino, vpp);
	if (error) {
		UFS_VFREE(pvp, ino, mode);
		return (error);
	}
	ip = VTOI(*vpp);
	if (ip->i_mode) {
		printf("mode = 0%o, inum = %lu, fs = %s\n",
		    ip->i_mode, (u_long)ip->i_number, fs->fs_fsmnt);
		panic("ffs_valloc: dup alloc");
	}
	if (ip->i_blocks && (fs->fs_flags & FS_UNCLEAN) == 0) {	    /* XXX */
		printf("free inode %s/%lu had %ld blocks\n",
		    fs->fs_fsmnt, (u_long)ino, (long)ip->i_blocks);
		ip->i_blocks = 0;
	}
	ip->i_flags = 0;
	/*
	 * Set up a new generation number for this inode.
	 */
	if (ip->i_gen == 0 || ++ip->i_gen == 0)
		ip->i_gen = random() / 2 + 1;
	return (0);
noinodes:
	ffs_fserr(fs, cred->cr_uid, "out of inodes");
	uprintf("\n%s: create/symlink failed, no inodes free\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Find a cylinder group to place a directory.
 *
 * The policy implemented by this algorithm is to allocate a
 * directory inode in the same cylinder group as its parent
 * directory, but also to reserve space for its files inodes
 * and data. Restrict the number of directories which may be
 * allocated one after another in the same cylinder group
 * without intervening allocation of files.
 *
 * If we allocate a first level directory then force allocation
 * in another cylinder group.
 */
static ino_t
ffs_dirpref(pip)
	struct inode *pip;
{
	register struct fs *fs;
	int cg, prefcg, dirsize, cgsize;
	int avgifree, avgbfree, avgndir, curdirsize;
	int minifree, minbfree, maxndir;
	int mincg, minndir;
	int maxcontigdirs;

	fs = pip->i_fs;

	avgifree = fs->fs_cstotal.cs_nifree / fs->fs_ncg;
	avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
	avgndir = fs->fs_cstotal.cs_ndir / fs->fs_ncg;

	/*
	 * Force allocation in another cg if creating a first level dir.
	 */
	if (ITOV(pip)->v_flag & VROOT) {
		prefcg = arc4random() % fs->fs_ncg;
		mincg = prefcg;
		minndir = fs->fs_ipg;
		for (cg = prefcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		for (cg = 0; cg < prefcg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		return ((ino_t)(fs->fs_ipg * mincg));
	}

	/*
	 * Count various limits which used for
	 * optimal allocation of a directory inode.
	 */
	maxndir = min(avgndir + fs->fs_ipg / 16, fs->fs_ipg);
	minifree = avgifree - fs->fs_ipg / 4;
	if (minifree < 0)
		minifree = 0;
	minbfree = avgbfree - fs->fs_fpg / fs->fs_frag / 4;
	if (minbfree < 0)
		minbfree = 0;
	cgsize = fs->fs_fsize * fs->fs_fpg;
	dirsize = fs->fs_avgfilesize * fs->fs_avgfpdir;
	curdirsize = avgndir ? (cgsize - avgbfree * fs->fs_bsize) / avgndir : 0;
	if (dirsize < curdirsize)
		dirsize = curdirsize;
	maxcontigdirs = min(cgsize / dirsize, 255);
	if (fs->fs_avgfpdir > 0)
		maxcontigdirs = min(maxcontigdirs,
				    fs->fs_ipg / fs->fs_avgfpdir);
	if (maxcontigdirs == 0)
		maxcontigdirs = 1;

	/*
	 * Limit number of dirs in one cg and reserve space for 
	 * regular files, but only if we have no deficit in
	 * inodes or space.
	 */
	prefcg = ino_to_cg(fs, pip->i_number);
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	/*
	 * This is a backstop when we have deficit in space.
	 */
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			return ((ino_t)(fs->fs_ipg * cg));
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			break;
	return ((ino_t)(fs->fs_ipg * cg));
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 *
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. If no blocks have been allocated in any other section, the
 * policy is to place the section in a cylinder group with a greater than
 * average number of free blocks.  An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 *
 * If a section is already partially allocated, the policy is to
 * contiguously allocate fs_maxcontig blocks.  The end of one of these
 * contiguous blocks and the beginning of the next is physically separated
 * so that the disk head will be in transit between them for at least
 * fs_rotdelay milliseconds.  This is to allow time for the processor to
 * schedule another I/O transfer.
 */
ufs_daddr_t
ffs_blkpref(ip, lbn, indx, bap)
	struct inode *ip;
	ufs_daddr_t lbn;
	int indx;
	ufs_daddr_t *bap;
{
	register struct fs *fs;
	register int cg;
	int avgbfree, startcg;
	ufs_daddr_t nextblk;

	fs = ip->i_fs;
	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < NDADDR + NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (fs->fs_fpg * cg + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs, bap[indx - 1]) + 1;
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		for (cg = 0; cg <= startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		return (0);
	}
	/*
	 * One or more previous blocks have been laid out. If less
	 * than fs_maxcontig previous blocks are contiguous, the
	 * next block is requested contiguously, otherwise it is
	 * requested rotationally delayed by fs_rotdelay milliseconds.
	 */
	nextblk = bap[indx - 1] + fs->fs_frag;
	if (fs->fs_rotdelay == 0 || indx < fs->fs_maxcontig ||
	    bap[indx - fs->fs_maxcontig] +
	    blkstofrags(fs, fs->fs_maxcontig) != nextblk)
		return (nextblk);
	/*
	 * Here we convert ms of delay to frags as:
	 * (frags) = (ms) * (rev/sec) * (sect/rev) /
	 *	((sect/frag) * (ms/sec))
	 * then round up to the next block.
	 */
	nextblk += roundup(fs->fs_rotdelay * fs->fs_rps * fs->fs_nsect /
	    (NSPF(fs) * 1000), fs->fs_frag);
	return (nextblk);
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 */
/*VARARGS5*/
static u_long
ffs_hashalloc(ip, cg, pref, size, allocator)
	struct inode *ip;
	int cg;
	long pref;
	int size;	/* size for data blocks, mode for inodes */
	allocfcn_t *allocator;
{
	register struct fs *fs;
	long result;	/* XXX why not same type as we return? */
	int i, icg = cg;

#ifdef DIAGNOSTIC
	if (ITOV(ip)->v_mount->mnt_kern_flag & MNTK_SUSPENDED)
		panic("ffs_hashalloc: allocation on suspended filesystem");
#endif
	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (0);
}

/*
 * Determine whether a fragment can be extended.
 *
 * Check to see if the necessary fragments are available, and
 * if they are, allocate them.
 */
static ufs_daddr_t
ffs_fragextend(ip, cg, bprev, osize, nsize)
	struct inode *ip;
	int cg;
	long bprev;
	int osize, nsize;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	long bno;
	int frags, bbase;
	int i, error;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nffree < numfrags(fs, nsize - osize))
		return (0);
	frags = numfrags(fs, nsize);
	bbase = fragnum(fs, bprev);
	if (bbase > fragnum(fs, (bprev + frags - 1))) {
		/* cannot extend across a block boundary */
		return (0);
	}
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (0);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return (0);
	}
	bp->b_xflags |= BX_BKGRDWRITE;
	cgp->cg_time = time_second;
	bno = dtogd(fs, bprev);
	blksfree = cg_blksfree(cgp);
	for (i = numfrags(fs, osize); i < frags; i++)
		if (isclr(blksfree, bno + i)) {
			brelse(bp);
			return (0);
		}
	/*
	 * the current fragment can be extended
	 * deduct the count on fragment being extended into
	 * increase the count on the remaining fragment (if any)
	 * allocate the extended piece
	 */
	for (i = frags; i < fs->fs_frag - bbase; i++)
		if (isclr(blksfree, bno + i))
			break;
	cgp->cg_frsum[i - numfrags(fs, osize)]--;
	if (i != frags)
		cgp->cg_frsum[i - frags]++;
	for (i = numfrags(fs, osize); i < frags; i++) {
		clrbit(blksfree, bno + i);
		cgp->cg_cs.cs_nffree--;
		fs->fs_cstotal.cs_nffree--;
		fs->fs_cs(fs, cg).cs_nffree--;
	}
	fs->fs_fmod = 1;
	if (DOINGSOFTDEP(ITOV(ip)))
		softdep_setup_blkmapdep(bp, fs, bprev);
	if (fs->fs_active != 0)
		atomic_clear_char(&fs->fs_active[cg / NBBY], 1 << (cg % NBBY));
	bdwrite(bp);
	return (bprev);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static ufs_daddr_t
ffs_alloccg(ip, cg, bpref, size)
	struct inode *ip;
	int cg;
	ufs_daddr_t bpref;
	int size;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	register int i;
	ufs_daddr_t bno, blkno;
	int allocsiz, error, frags;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (0);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (0);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize)) {
		brelse(bp);
		return (0);
	}
	bp->b_xflags |= BX_BKGRDWRITE;
	cgp->cg_time = time_second;
	if (size == fs->fs_bsize) {
		bno = ffs_alloccgblk(ip, bp, bpref);
		if (fs->fs_active != 0)
			atomic_clear_char(&fs->fs_active[cg / NBBY],
			    1 << (cg % NBBY));
		bdwrite(bp);
		return (bno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	blksfree = cg_blksfree(cgp);
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0) {
			brelse(bp);
			return (0);
		}
		bno = ffs_alloccgblk(ip, bp, bpref);
		bpref = dtogd(fs, bno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(blksfree, bpref + i);
		i = fs->fs_frag - frags;
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		cgp->cg_frsum[i]++;
		if (fs->fs_active != 0)
			atomic_clear_char(&fs->fs_active[cg / NBBY],
			    1 << (cg % NBBY));
		bdwrite(bp);
		return (bno);
	}
	bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
	if (bno < 0) {
		brelse(bp);
		return (0);
	}
	for (i = 0; i < frags; i++)
		clrbit(blksfree, bno + i);
	cgp->cg_cs.cs_nffree -= frags;
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	cgp->cg_frsum[allocsiz]--;
	if (frags != allocsiz)
		cgp->cg_frsum[allocsiz - frags]++;
	blkno = cg * fs->fs_fpg + bno;
	if (DOINGSOFTDEP(ITOV(ip)))
		softdep_setup_blkmapdep(bp, fs, blkno);
	if (fs->fs_active != 0)
		atomic_clear_char(&fs->fs_active[cg / NBBY], 1 << (cg % NBBY));
	bdwrite(bp);
	return ((u_long)blkno);
}

/*
 * Allocate a block in a cylinder group.
 *
 * This algorithm implements the following policy:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate the next available block on the block rotor for the
 *      specified cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
static ufs_daddr_t
ffs_alloccgblk(ip, bp, bpref)
	struct inode *ip;
	struct buf *bp;
	ufs_daddr_t bpref;
{
	struct fs *fs;
	struct cg *cgp;
	ufs_daddr_t bno, blkno;
	int cylno, pos, delta;
	short *cylbp;
	register int i;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	cgp = (struct cg *)bp->b_data;
	blksfree = cg_blksfree(cgp);
	if (bpref == 0 || dtog(fs, bpref) != cgp->cg_cgx) {
		bpref = cgp->cg_rotor;
		goto norot;
	}
	bpref = blknum(fs, bpref);
	bpref = dtogd(fs, bpref);
	/*
	 * if the requested block is available, use it
	 */
	if (ffs_isblock(fs, blksfree, fragstoblks(fs, bpref))) {
		bno = bpref;
		goto gotit;
	}
	if (fs->fs_nrpos <= 1 || fs->fs_cpc == 0) {
		/*
		 * Block layout information is not available.
		 * Leaving bpref unchanged means we take the
		 * next available free block following the one
		 * we just allocated. Hopefully this will at
		 * least hit a track cache on drives of unknown
		 * geometry (e.g. SCSI).
		 */
		goto norot;
	}
	/*
	 * check for a block available on the same cylinder
	 */
	cylno = cbtocylno(fs, bpref);
	if (cg_blktot(cgp)[cylno] == 0)
		goto norot;
	/*
	 * check the summary information to see if a block is
	 * available in the requested cylinder starting at the
	 * requested rotational position and proceeding around.
	 */
	cylbp = cg_blks(fs, cgp, cylno);
	pos = cbtorpos(fs, bpref);
	for (i = pos; i < fs->fs_nrpos; i++)
		if (cylbp[i] > 0)
			break;
	if (i == fs->fs_nrpos)
		for (i = 0; i < pos; i++)
			if (cylbp[i] > 0)
				break;
	if (cylbp[i] > 0) {
		/*
		 * found a rotational position, now find the actual
		 * block. A panic if none is actually there.
		 */
		pos = cylno % fs->fs_cpc;
		bno = (cylno - pos) * fs->fs_spc / NSPB(fs);
		if (fs_postbl(fs, pos)[i] == -1) {
			printf("pos = %d, i = %d, fs = %s\n",
			    pos, i, fs->fs_fsmnt);
			panic("ffs_alloccgblk: cyl groups corrupted");
		}
		for (i = fs_postbl(fs, pos)[i];; ) {
			if (ffs_isblock(fs, blksfree, bno + i)) {
				bno = blkstofrags(fs, (bno + i));
				goto gotit;
			}
			delta = fs_rotbl(fs)[i];
			if (delta <= 0 ||
			    delta + i > fragstoblks(fs, fs->fs_fpg))
				break;
			i += delta;
		}
		printf("pos = %d, i = %d, fs = %s\n", pos, i, fs->fs_fsmnt);
		panic("ffs_alloccgblk: can't find blk in cyl");
	}
norot:
	/*
	 * no blocks in the requested cylinder, so take next
	 * available one in this cylinder group.
	 */
	bno = ffs_mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (0);
	cgp->cg_rotor = bno;
gotit:
	blkno = fragstoblks(fs, bno);
	ffs_clrblock(fs, blksfree, (long)blkno);
	ffs_clusteracct(fs, cgp, blkno, -1);
	cgp->cg_cs.cs_nbfree--;
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, cgp->cg_cgx).cs_nbfree--;
	cylno = cbtocylno(fs, bno);
	cg_blks(fs, cgp, cylno)[cbtorpos(fs, bno)]--;
	cg_blktot(cgp)[cylno]--;
	fs->fs_fmod = 1;
	blkno = cgp->cg_cgx * fs->fs_fpg + bno;
	if (DOINGSOFTDEP(ITOV(ip)))
		softdep_setup_blkmapdep(bp, fs, blkno);
	return (blkno);
}

/*
 * Determine whether a cluster can be allocated.
 *
 * We do not currently check for optimal rotational layout if there
 * are multiple choices in the same cylinder group. Instead we just
 * take the first one that we find following bpref.
 */
static ufs_daddr_t
ffs_clusteralloc(ip, cg, bpref, len)
	struct inode *ip;
	int cg;
	ufs_daddr_t bpref;
	int len;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	int i, got, run, bno, bit, map;
	u_char *mapp;
	int32_t *lp;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	if (fs->fs_maxcluster[cg] < len)
		return (0);
	if (bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)), (int)fs->fs_cgsize,
	    NOCRED, &bp))
		goto fail;
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp))
		goto fail;
	bp->b_xflags |= BX_BKGRDWRITE;
	/*
	 * Check to see if a cluster of the needed size (or bigger) is
	 * available in this cylinder group.
	 */
	lp = &cg_clustersum(cgp)[len];
	for (i = len; i <= fs->fs_contigsumsize; i++)
		if (*lp++ > 0)
			break;
	if (i > fs->fs_contigsumsize) {
		/*
		 * This is the first time looking for a cluster in this
		 * cylinder group. Update the cluster summary information
		 * to reflect the true maximum sized cluster so that
		 * future cluster allocation requests can avoid reading
		 * the cylinder group map only to find no clusters.
		 */
		lp = &cg_clustersum(cgp)[len - 1];
		for (i = len - 1; i > 0; i--)
			if (*lp-- > 0)
				break;
		fs->fs_maxcluster[cg] = i;
		goto fail;
	}
	/*
	 * Search the cluster map to find a big enough cluster.
	 * We take the first one that we find, even if it is larger
	 * than we need as we prefer to get one close to the previous
	 * block allocation. We do not search before the current
	 * preference point as we do not want to allocate a block
	 * that is allocated before the previous one (as we will
	 * then have to wait for another pass of the elevator
	 * algorithm before it will be read). We prefer to fail and
	 * be recalled to try an allocation in the next cylinder group.
	 */
	if (dtog(fs, bpref) != cg)
		bpref = 0;
	else
		bpref = fragstoblks(fs, dtogd(fs, blknum(fs, bpref)));
	mapp = &cg_clustersfree(cgp)[bpref / NBBY];
	map = *mapp++;
	bit = 1 << (bpref % NBBY);
	for (run = 0, got = bpref; got < cgp->cg_nclusterblks; got++) {
		if ((map & bit) == 0) {
			run = 0;
		} else {
			run++;
			if (run == len)
				break;
		}
		if ((got & (NBBY - 1)) != (NBBY - 1)) {
			bit <<= 1;
		} else {
			map = *mapp++;
			bit = 1;
		}
	}
	if (got >= cgp->cg_nclusterblks)
		goto fail;
	/*
	 * Allocate the cluster that we have found.
	 */
	blksfree = cg_blksfree(cgp);
	for (i = 1; i <= len; i++)
		if (!ffs_isblock(fs, blksfree, got - run + i))
			panic("ffs_clusteralloc: map mismatch");
	bno = cg * fs->fs_fpg + blkstofrags(fs, got - run + 1);
	if (dtog(fs, bno) != cg)
		panic("ffs_clusteralloc: allocated out of group");
	len = blkstofrags(fs, len);
	for (i = 0; i < len; i += fs->fs_frag)
		if ((got = ffs_alloccgblk(ip, bp, bno + i)) != bno + i)
			panic("ffs_clusteralloc: lost block");
	if (fs->fs_active != 0)
		atomic_clear_char(&fs->fs_active[cg / NBBY], 1 << (cg % NBBY));
	bdwrite(bp);
	return (bno);

fail:
	brelse(bp);
	return (0);
}

/*
 * Determine whether an inode can be allocated.
 *
 * Check to see if an inode is available, and if it is,
 * allocate it using the following policy:
 *   1) allocate the requested inode.
 *   2) allocate the next available inode after the requested
 *      inode in the specified cylinder group.
 */
static ino_t
ffs_nodealloccg(ip, cg, ipref, mode)
	struct inode *ip;
	int cg;
	ufs_daddr_t ipref;
	int mode;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	u_int8_t *inosused;
	int error, start, len, loc, map, i;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		return (0);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (0);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp) || cgp->cg_cs.cs_nifree == 0) {
		brelse(bp);
		return (0);
	}
	bp->b_xflags |= BX_BKGRDWRITE;
	cgp->cg_time = time_second;
	inosused = cg_inosused(cgp);
	if (ipref) {
		ipref %= fs->fs_ipg;
		if (isclr(inosused, ipref))
			goto gotit;
	}
	start = cgp->cg_irotor / NBBY;
	len = howmany(fs->fs_ipg - cgp->cg_irotor, NBBY);
	loc = skpc(0xff, len, &inosused[start]);
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &inosused[0]);
		if (loc == 0) {
			printf("cg = %d, irotor = %ld, fs = %s\n",
			    cg, (long)cgp->cg_irotor, fs->fs_fsmnt);
			panic("ffs_nodealloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	i = start + len - loc;
	map = inosused[i];
	ipref = i * NBBY;
	for (i = 1; i < (1 << NBBY); i <<= 1, ipref++) {
		if ((map & i) == 0) {
			cgp->cg_irotor = ipref;
			goto gotit;
		}
	}
	printf("fs = %s\n", fs->fs_fsmnt);
	panic("ffs_nodealloccg: block not in map");
	/* NOTREACHED */
gotit:
	if (DOINGSOFTDEP(ITOV(ip)))
		softdep_setup_inomapdep(bp, ip, cg * fs->fs_ipg + ipref);
	setbit(inosused, ipref);
	cgp->cg_cs.cs_nifree--;
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	fs->fs_fmod = 1;
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir++;
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}
	bdwrite(bp);
	return (cg * fs->fs_ipg + ipref);
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible
 * block reassembly is checked.
 */
void
ffs_blkfree(ip, bno, size)
	register struct inode *ip;
	ufs_daddr_t bno;
	long size;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	ufs_daddr_t fragno, cgbno;
	int i, error, cg, blk, frags, bbase;
	u_int8_t *blksfree;
#ifdef DIAGNOSTIC
	struct vnode *vp;
#endif

	fs = ip->i_fs;
#ifdef DIAGNOSTIC
	if ((vp = ITOV(ip)) != NULL && vp->v_mount != NULL &&
	    (vp->v_mount->mnt_kern_flag & MNTK_SUSPENDED))
		panic("ffs_blkfree: deallocation on suspended filesystem");
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0 ||
	    fragnum(fs, bno) + numfrags(fs, size) > fs->fs_frag) {
		printf("dev=%s, bno = %ld, bsize = %ld, size = %ld, fs = %s\n",
		    devtoname(ip->i_dev), (long)bno, (long)fs->fs_bsize, size,
		    fs->fs_fsmnt);
		panic("ffs_blkfree: bad size");
	}
#endif
	if ((ip->i_devvp->v_flag & VCOPYONWRITE) &&
	    ffs_snapblkfree(ip, bno, size))
		return;
	VOP_FREEBLKS(ip->i_devvp, fsbtodb(fs, bno), size);
	cg = dtog(fs, bno);
	if ((u_int)bno >= fs->fs_size) {
		printf("bad block %ld, ino %lu\n",
		    (long)bno, (u_long)ip->i_number);
		ffs_fserr(fs, ip->i_uid, "bad block");
		return;
	}
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return;
	}
	bp->b_xflags |= BX_BKGRDWRITE;
	cgp->cg_time = time_second;
	cgbno = dtogd(fs, bno);
	blksfree = cg_blksfree(cgp);
	if (size == fs->fs_bsize) {
		fragno = fragstoblks(fs, cgbno);
		if (!ffs_isfreeblock(fs, blksfree, fragno)) {
			printf("dev = %s, block = %ld, fs = %s\n",
			    devtoname(ip->i_dev), (long)bno, fs->fs_fsmnt);
			panic("ffs_blkfree: freeing free block");
		}
		ffs_setblock(fs, blksfree, fragno);
		ffs_clusteracct(fs, cgp, fragno, 1);
		cgp->cg_cs.cs_nbfree++;
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
		i = cbtocylno(fs, cgbno);
		cg_blks(fs, cgp, i)[cbtorpos(fs, cgbno)]++;
		cg_blktot(cgp)[i]++;
	} else {
		bbase = cgbno - fragnum(fs, cgbno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(blksfree, cgbno + i)) {
				printf("dev = %s, block = %ld, fs = %s\n",
				    devtoname(ip->i_dev), (long)(bno + i),
				    fs->fs_fsmnt);
				panic("ffs_blkfree: freeing free frag");
			}
			setbit(blksfree, cgbno + i);
		}
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		fragno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, blksfree, fragno)) {
			cgp->cg_cs.cs_nffree -= fs->fs_frag;
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			ffs_clusteracct(fs, cgp, fragno, 1);
			cgp->cg_cs.cs_nbfree++;
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
			i = cbtocylno(fs, bbase);
			cg_blks(fs, cgp, i)[cbtorpos(fs, bbase)]++;
			cg_blktot(cgp)[i]++;
		}
	}
	fs->fs_fmod = 1;
	if (fs->fs_active != 0)
		atomic_clear_char(&fs->fs_active[cg / NBBY], 1 << (cg % NBBY));
	bdwrite(bp);
}

#ifdef DIAGNOSTIC
/*
 * Verify allocation of a block or fragment. Returns true if block or
 * fragment is allocated, false if it is free.
 */
static int
ffs_checkblk(ip, bno, size)
	struct inode *ip;
	ufs_daddr_t bno;
	long size;
{
	struct fs *fs;
	struct cg *cgp;
	struct buf *bp;
	int i, error, frags, free;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("bsize = %ld, size = %ld, fs = %s\n",
		    (long)fs->fs_bsize, size, fs->fs_fsmnt);
		panic("ffs_checkblk: bad size");
	}
	if ((u_int)bno >= fs->fs_size)
		panic("ffs_checkblk: bad block %d", bno);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, dtog(fs, bno))),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error)
		panic("ffs_checkblk: cg bread failed");
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp))
		panic("ffs_checkblk: cg magic mismatch");
	bp->b_xflags |= BX_BKGRDWRITE;
	blksfree = cg_blksfree(cgp);
	bno = dtogd(fs, bno);
	if (size == fs->fs_bsize) {
		free = ffs_isblock(fs, blksfree, fragstoblks(fs, bno));
	} else {
		frags = numfrags(fs, size);
		for (free = 0, i = 0; i < frags; i++)
			if (isset(blksfree, bno + i))
				free++;
		if (free != 0 && free != frags)
			panic("ffs_checkblk: partially free fragment");
	}
	brelse(bp);
	return (!free);
}
#endif /* DIAGNOSTIC */

/*
 * Free an inode.
 */
int
ffs_vfree(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	if (DOINGSOFTDEP(pvp)) {
		softdep_freefile(pvp, ino, mode);
		return (0);
	}
	return (ffs_freefile(VTOI(pvp), ino, mode));
}

/*
 * Do the actual free operation.
 * The specified inode is placed back in the free map.
 */
int
ffs_freefile(pip, ino, mode)
	struct inode *pip;
	ino_t ino;
	int mode;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	int error, cg;
	u_int8_t *inosused;

	fs = pip->i_fs;
	if ((u_int)ino >= fs->fs_ipg * fs->fs_ncg)
		panic("ffs_vfree: range: dev = (%d,%d), ino = %d, fs = %s",
		    major(pip->i_dev), minor(pip->i_dev), ino, fs->fs_fsmnt);
	cg = ino_to_cg(fs, ino);
	error = bread(pip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return (0);
	}
	bp->b_xflags |= BX_BKGRDWRITE;
	cgp->cg_time = time_second;
	inosused = cg_inosused(cgp);
	ino %= fs->fs_ipg;
	if (isclr(inosused, ino)) {
		printf("dev = %s, ino = %lu, fs = %s\n", devtoname(pip->i_dev),
		    (u_long)ino + cg * fs->fs_ipg, fs->fs_fsmnt);
		if (fs->fs_ronly == 0)
			panic("ffs_vfree: freeing free inode");
	}
	clrbit(inosused, ino);
	if (ino < cgp->cg_irotor)
		cgp->cg_irotor = ino;
	cgp->cg_cs.cs_nifree++;
	fs->fs_cstotal.cs_nifree++;
	fs->fs_cs(fs, cg).cs_nifree++;
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir--;
		fs->fs_cstotal.cs_ndir--;
		fs->fs_cs(fs, cg).cs_ndir--;
	}
	fs->fs_fmod = 1;
	bdwrite(bp);
	return (0);
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
static ufs_daddr_t
ffs_mapsearch(fs, cgp, bpref, allocsiz)
	register struct fs *fs;
	register struct cg *cgp;
	ufs_daddr_t bpref;
	int allocsiz;
{
	ufs_daddr_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;
	u_int8_t *blksfree;

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = cgp->cg_frotor / NBBY;
	blksfree = cg_blksfree(cgp);
	len = howmany(fs->fs_fpg, NBBY) - start;
	loc = scanc((u_int)len, (u_char *)&blksfree[start],
		(u_char *)fragtbl[fs->fs_frag],
		(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len, (u_char *)&blksfree[0],
			(u_char *)fragtbl[fs->fs_frag],
			(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			printf("start = %d, len = %d, fs = %s\n",
			    start, len, fs->fs_fsmnt);
			panic("ffs_alloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = bno;
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, blksfree, bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	printf("bno = %lu, fs = %s\n", (u_long)bno, fs->fs_fsmnt);
	panic("ffs_alloccg: block not in map");
	return (-1);
}

/*
 * Update the cluster map because of an allocation or free.
 *
 * Cnt == 1 means free; cnt == -1 means allocating.
 */
void
ffs_clusteracct(fs, cgp, blkno, cnt)
	struct fs *fs;
	struct cg *cgp;
	ufs_daddr_t blkno;
	int cnt;
{
	int32_t *sump;
	int32_t *lp;
	u_char *freemapp, *mapp;
	int i, start, end, forw, back, map, bit;

	if (fs->fs_contigsumsize <= 0)
		return;
	freemapp = cg_clustersfree(cgp);
	sump = cg_clustersum(cgp);
	/*
	 * Allocate or clear the actual block.
	 */
	if (cnt > 0)
		setbit(freemapp, blkno);
	else
		clrbit(freemapp, blkno);
	/*
	 * Find the size of the cluster going forward.
	 */
	start = blkno + 1;
	end = start + fs->fs_contigsumsize;
	if (end >= cgp->cg_nclusterblks)
		end = cgp->cg_nclusterblks;
	mapp = &freemapp[start / NBBY];
	map = *mapp++;
	bit = 1 << (start % NBBY);
	for (i = start; i < end; i++) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != (NBBY - 1)) {
			bit <<= 1;
		} else {
			map = *mapp++;
			bit = 1;
		}
	}
	forw = i - start;
	/*
	 * Find the size of the cluster going backward.
	 */
	start = blkno - 1;
	end = start - fs->fs_contigsumsize;
	if (end < 0)
		end = -1;
	mapp = &freemapp[start / NBBY];
	map = *mapp--;
	bit = 1 << (start % NBBY);
	for (i = start; i > end; i--) {
		if ((map & bit) == 0)
			break;
		if ((i & (NBBY - 1)) != 0) {
			bit >>= 1;
		} else {
			map = *mapp--;
			bit = 1 << (NBBY - 1);
		}
	}
	back = start - i;
	/*
	 * Account for old cluster and the possibly new forward and
	 * back clusters.
	 */
	i = back + forw + 1;
	if (i > fs->fs_contigsumsize)
		i = fs->fs_contigsumsize;
	sump[i] += cnt;
	if (back > 0)
		sump[back] -= cnt;
	if (forw > 0)
		sump[forw] -= cnt;
	/*
	 * Update cluster summary information.
	 */
	lp = &sump[fs->fs_contigsumsize];
	for (i = fs->fs_contigsumsize; i > 0; i--)
		if (*lp-- > 0)
			break;
	fs->fs_maxcluster[cgp->cg_cgx] = i;
}

/*
 * Fserr prints the name of a file system with an error diagnostic.
 *
 * The form of the error message is:
 *	fs: error message
 */
static void
ffs_fserr(fs, uid, cp)
	struct fs *fs;
	u_int uid;
	char *cp;
{
	struct proc *p = curproc;	/* XXX */

	log(LOG_ERR, "pid %d (%s), uid %d on %s: %s\n", p ? p->p_pid : -1,
			p ? p->p_comm : "-", uid, fs->fs_fsmnt, cp);
}

/*
 * This function provides the capability for the fsck program to
 * update an active filesystem. Six operations are provided:
 *
 * adjrefcnt(inode, amt) - adjusts the reference count on the
 *	specified inode by the specified amount. Under normal
 *	operation the count should always go down. Decrementing
 *	the count to zero will cause the inode to be freed.
 * adjblkcnt(inode, amt) - adjust the number of blocks used to
 *	by the specifed amount.
 * freedirs(inode, count) - directory inodes [inode..inode + count - 1]
 *	are marked as free. Inodes should never have to be marked
 *	as in use.
 * freefiles(inode, count) - file inodes [inode..inode + count - 1]
 *	are marked as free. Inodes should never have to be marked
 *	as in use.
 * freeblks(blockno, size) - blocks [blockno..blockno + size - 1]
 *	are marked as free. Blocks should never have to be marked
 *	as in use.
 * setflags(flags, set/clear) - the fs_flags field has the specified
 *	flags set (second parameter +1) or cleared (second parameter -1).
 */

static int sysctl_ffs_fsck __P((SYSCTL_HANDLER_ARGS));

SYSCTL_PROC(_vfs_ffs, FFS_ADJ_REFCNT, adjrefcnt, CTLFLAG_WR|CTLTYPE_STRUCT,
	0, 0, sysctl_ffs_fsck, "S,fsck", "Adjust Inode Reference Count");

SYSCTL_NODE(_vfs_ffs, FFS_ADJ_BLKCNT, adjblkcnt, CTLFLAG_WR,
	sysctl_ffs_fsck, "Adjust Inode Used Blocks Count");

SYSCTL_NODE(_vfs_ffs, FFS_DIR_FREE, freedirs, CTLFLAG_WR,
	sysctl_ffs_fsck, "Free Range of Directory Inodes");

SYSCTL_NODE(_vfs_ffs, FFS_FILE_FREE, freefiles, CTLFLAG_WR,
	sysctl_ffs_fsck, "Free Range of File Inodes");

SYSCTL_NODE(_vfs_ffs, FFS_BLK_FREE, freeblks, CTLFLAG_WR,
	sysctl_ffs_fsck, "Free Range of Blocks");

SYSCTL_NODE(_vfs_ffs, FFS_SET_FLAGS, setflags, CTLFLAG_WR,
	sysctl_ffs_fsck, "Change Filesystem Flags");

#ifdef DEBUG
static int fsckcmds = 0;
SYSCTL_INT(_debug, OID_AUTO, fsckcmds, CTLFLAG_RW, &fsckcmds, 0, "");
#endif /* DEBUG */

static int
sysctl_ffs_fsck(SYSCTL_HANDLER_ARGS)
{
	struct fsck_cmd cmd;
	struct inode tip;
	struct ufsmount *ump;
	struct vnode *vp;
	struct inode *ip;
	struct mount *mp;
	struct fs *fs;
	ufs_daddr_t blkno;
	long blkcnt, blksize;
	struct file *fp;
	int filetype, error;

	if (req->newlen > sizeof cmd)
		return (EBADRPC);
	if ((error = SYSCTL_IN(req, &cmd, sizeof cmd)) != 0)
		return (error);
	if (cmd.version != FFS_CMD_VERSION)
		return (ERPCMISMATCH);
	if ((error = getvnode(curproc->p_fd, cmd.handle, &fp)) != 0)
		return (error);
	vn_start_write((struct vnode *)fp->f_data, &mp, V_WAIT);
	if (mp == 0 || strncmp(mp->mnt_stat.f_fstypename, "ufs", MFSNAMELEN)) {
		vn_finished_write(mp);
		return (EINVAL);
	}
	if (mp->mnt_flag & MNT_RDONLY) {
		vn_finished_write(mp);
		return (EROFS);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	filetype = IFREG;

	switch (oidp->oid_number) {

	case FFS_SET_FLAGS:
#ifdef DEBUG
		if (fsckcmds)
			printf("%s: %s flags\n", mp->mnt_stat.f_mntonname,
			    cmd.size > 0 ? "set" : "clear");
#endif /* DEBUG */
		if (cmd.size > 0)
			fs->fs_flags |= (long)cmd.value;
		else
			fs->fs_flags &= ~(long)cmd.value;
		break;

	case FFS_ADJ_REFCNT:
#ifdef DEBUG
		if (fsckcmds) {
			printf("%s: adjust inode %d count by %ld\n",
			    mp->mnt_stat.f_mntonname, (ino_t)cmd.value,
			    cmd.size);
		}
#endif /* DEBUG */
		if ((error = VFS_VGET(mp, (ino_t)cmd.value, &vp)) != 0)
			break;
		ip = VTOI(vp);
		ip->i_nlink += cmd.size;
		ip->i_effnlink += cmd.size;
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
		vput(vp);
		break;

	case FFS_ADJ_BLKCNT:
#ifdef DEBUG
		if (fsckcmds) {
			printf("%s: adjust inode %d block count by %ld\n",
			    mp->mnt_stat.f_mntonname, (ino_t)cmd.value,
			    cmd.size);
		}
#endif /* DEBUG */
		if ((error = VFS_VGET(mp, (ino_t)cmd.value, &vp)) != 0)
			break;
		ip = VTOI(vp);
		ip->i_blocks += cmd.size;
		ip->i_flag |= IN_CHANGE;
		vput(vp);
		break;

	case FFS_DIR_FREE:
		filetype = IFDIR;
		/* fall through */

	case FFS_FILE_FREE:
#ifdef DEBUG
		if (fsckcmds) {
			if (cmd.size == 1)
				printf("%s: free %s inode %d\n",
				    mp->mnt_stat.f_mntonname,
				    filetype == IFDIR ? "directory" : "file",
				    (ino_t)cmd.value);
			else
				printf("%s: free %s inodes %d-%d\n",
				    mp->mnt_stat.f_mntonname,
				    filetype == IFDIR ? "directory" : "file",
				    (ino_t)cmd.value,
				    (ino_t)(cmd.value + cmd.size - 1));
		}
#endif /* DEBUG */
		tip.i_devvp = ump->um_devvp;
		tip.i_dev = ump->um_dev;
		tip.i_fs = fs;
		while (cmd.size > 0) {
			if ((error = ffs_freefile(&tip, cmd.value, filetype)))
				break;
			cmd.size -= 1;
			cmd.value += 1;
		}
		break;

	case FFS_BLK_FREE:
#ifdef DEBUG
		if (fsckcmds) {
			if (cmd.size == 1)
				printf("%s: free block %d\n",
				    mp->mnt_stat.f_mntonname,
				    (ufs_daddr_t)cmd.value);
			else
				printf("%s: free blocks %d-%ld\n",
				    mp->mnt_stat.f_mntonname, 
				    (ufs_daddr_t)cmd.value,
				    (ufs_daddr_t)cmd.value + cmd.size - 1);
		}
#endif /* DEBUG */
		tip.i_number = ROOTINO;
		tip.i_devvp = ump->um_devvp;
		tip.i_dev = ump->um_dev;
		tip.i_fs = fs;
		tip.i_size = cmd.size * fs->fs_fsize;
		tip.i_uid = 0;
		tip.i_vnode = NULL;
		blkno = (ufs_daddr_t)cmd.value;
		blkcnt = cmd.size;
		blksize = fs->fs_frag - (blkno % fs->fs_frag);
		while (blkcnt > 0) {
			if (blksize > blkcnt)
				blksize = blkcnt;
			ffs_blkfree(&tip, blkno, blksize * fs->fs_fsize);
			blkno += blksize;
			blkcnt -= blksize;
			blksize = fs->fs_frag;
		}
		break;

	default:
#ifdef DEBUG
		if (fsckcmds) {
			printf("Invalid request %d from fsck\n",
			    oidp->oid_number);
		}
#endif /* DEBUG */
		error = EINVAL;
		break;

	}
	vn_finished_write(mp);
	return (error);
}
