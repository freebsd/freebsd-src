/*
 * Copyright 2000 Marshall Kirk McKusick. All Rights Reserved.
 *
 * Further information about snapshots can be obtained from:
 *
 *	Marshall Kirk McKusick		http://www.mckusick.com/softdep/
 *	1614 Oxford Street		mckusick@mckusick.com
 *	Berkeley, CA 94709-1608		+1-510-843-9542
 *	USA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ffs_snapshot.c	8.11 (McKusick) 7/23/00
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#define KERNCRED proc0.p_ucred
#define DEBUG 1

static int indiracct __P((struct vnode *, struct vnode *, int, ufs_daddr_t,
	int, int, int, int));
static int snapacct __P((struct vnode *, ufs_daddr_t *, ufs_daddr_t *));
static int readblock __P((struct buf *, daddr_t));

#ifdef DEBUG
#include <sys/sysctl.h>
int snapdebug = 0;
SYSCTL_INT(_debug, OID_AUTO, snapdebug, CTLFLAG_RW, &snapdebug, 0, "");
#endif /* DEBUG */

/*
 * Create a snapshot file and initialize it for the filesystem.
 */
int
ffs_snapshot(mp, snapfile)
	struct mount *mp;
	char *snapfile;
{
	ufs_daddr_t rlbn;
	ufs_daddr_t lbn, blkno, copyblkno, inoblks[FSMAXSNAP];
	int error, cg, snaploc, indiroff, numblks;
	int i, size, base, len, loc, inoblkcnt;
	int blksperindir, flag = mp->mnt_flag;
	struct fs *fs = VFSTOUFS(mp)->um_fs;
	struct proc *p = CURPROC;
	struct inode *devip, *ip, *xp;
	struct buf *bp, *nbp, *ibp;
	struct vnode *vp, *devvp;
	struct nameidata nd;
	struct mount *wrtmp;
	struct dinode *dip;
	struct vattr vat;
	struct cg *cgp;

	/*
	 * Need to serialize access to snapshot code per filesystem.
	 */
	/*
	 * Assign a snapshot slot in the superblock.
	 */
	for (snaploc = 0; snaploc < FSMAXSNAP; snaploc++)
		if (fs->fs_snapinum[snaploc] == 0)
			break;
	if (snaploc == FSMAXSNAP)
		return (ENOSPC);
	/*
	 * Create the snapshot file.
	 */
restart:
	NDINIT(&nd, CREATE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE, snapfile, p);
	if ((error = namei(&nd)) != 0)
		return (error);
	if (nd.ni_vp != NULL) {
		vput(nd.ni_vp);
		error = EEXIST;
	}
	if (nd.ni_dvp->v_mount != mp)
		error = EXDEV;
	if (error) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		return (error);
	}
	VATTR_NULL(&vat);
	vat.va_type = VREG;
	vat.va_mode = S_IRUSR;
	vat.va_vaflags |= VA_EXCLUSIVE;
	if (VOP_GETWRITEMOUNT(nd.ni_dvp, &wrtmp))
		wrtmp = NULL;
	if (wrtmp != mp)
		panic("ffs_snapshot: mount mismatch");
	if (vn_start_write(NULL, &wrtmp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &wrtmp,
		    V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VOP_LEASE(nd.ni_dvp, p, KERNCRED, LEASE_WRITE);
	error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vat);
	vput(nd.ni_dvp);
	if (error) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vn_finished_write(wrtmp);
		return (error);
	}
	vp = nd.ni_vp;
	ip = VTOI(vp);
	devvp = ip->i_devvp;
	devip = VTOI(devvp);
	/*
	 * Allocate and copy the last block contents so as to be able
	 * to set size to that of the filesystem.
	 */
	numblks = howmany(fs->fs_size, fs->fs_frag);
	error = VOP_BALLOC(vp, lblktosize(fs, (off_t)(numblks - 1)),
	    fs->fs_bsize, KERNCRED, B_CLRBUF, &bp);
	if (error)
		goto out;
	ip->i_size = lblktosize(fs, (off_t)numblks);
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	if ((error = readblock(bp, numblks - 1)) != 0)
		goto out;
	bawrite(bp);
	/*
	 * Preallocate critical data structures so that we can copy
	 * them in without further allocation after we suspend all
	 * operations on the filesystem. We would like to just release
	 * the allocated buffers without writing them since they will
	 * be filled in below once we are ready to go, but this upsets
	 * the soft update code, so we go ahead and write the new buffers.
	 *
	 * Allocate all indirect blocks. Also allocate shadow copies
	 * for each of the indirect blocks.
	 */
	for (blkno = NDADDR; blkno < numblks; blkno += NINDIR(fs)) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
		    fs->fs_bsize, p->p_ucred, B_METAONLY, &ibp);
		if (error)
			goto out;
		copyblkno = fragstoblks(fs, dbtofsb(fs, ibp->b_blkno));
		bdwrite(ibp);
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)copyblkno),
		    fs->fs_bsize, p->p_ucred, 0, &nbp);
		if (error)
			goto out;
		bawrite(nbp);
	}
	/*
	 * Allocate shadow blocks to copy all of the other snapshot inodes
	 * so that we will be able to expunge them from this snapshot.
	 */
	for (loc = 0, inoblkcnt = 0; loc < snaploc; loc++) {
		blkno = fragstoblks(fs, ino_to_fsba(fs, fs->fs_snapinum[loc]));
		for (i = 0; i < inoblkcnt; i++)
			if (inoblks[i] == blkno)
				break;
		if (i == inoblkcnt) {
			inoblks[inoblkcnt++] = blkno;
			error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
			    fs->fs_bsize, KERNCRED, 0, &nbp);
			if (error)
				goto out;
			bawrite(nbp);
		}
	}
	/*
	 * Allocate all cylinder group blocks.
	 */
	for (cg = 0; cg < fs->fs_ncg; cg++) {
		error = VOP_BALLOC(vp, (off_t)(cgtod(fs, cg)) << fs->fs_fshift,
		    fs->fs_bsize, KERNCRED, 0, &nbp);
		if (error)
			goto out;
		bawrite(nbp);
	}
	/*
	 * Allocate copies for the superblock and its summary information.
	 */
	error = VOP_BALLOC(vp, (off_t)(SBOFF), fs->fs_bsize, KERNCRED,
	    0, &nbp);
	if (error)
		goto out;
	bawrite(nbp);
	blkno = fragstoblks(fs, fs->fs_csaddr);
	len = howmany(fs->fs_cssize, fs->fs_bsize);
	for (loc = 0; loc < len; loc++) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)(blkno + loc)),
		    fs->fs_bsize, KERNCRED, 0, &nbp);
		if (error)
			goto out;
		bawrite(nbp);
	}
	/*
	 * Change inode to snapshot type file.
	 */
	ip->i_flags |= SF_SNAPSHOT;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	/*
	 * Ensure that the snapshot is completely on disk.
	 */
	if ((error = VOP_FSYNC(vp, KERNCRED, MNT_WAIT, p)) != 0)
		goto out;
	/*
	 * All allocations are done, so we can now snapshot the system.
	 *
	 * Suspend operation on filesystem.
	 */
	for (;;) {
		vn_finished_write(wrtmp);
		vfs_write_suspend(vp->v_mount);
		if (mp->mnt_kern_flag & MNTK_SUSPENDED)
			break;
		vn_start_write(NULL, &wrtmp, V_WAIT);
	}
	/*
	 * First, copy all the cylinder group maps. All the unallocated
	 * blocks are marked BLK_NOCOPY so that the snapshot knows that
	 * it need not copy them if they are later written.
	 */
	len = howmany(fs->fs_fpg, fs->fs_frag);
	for (cg = 0; cg < fs->fs_ncg; cg++) {
		error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
			(int)fs->fs_cgsize, KERNCRED, &bp);
		if (error) {
			brelse(bp);
			goto out1;
		}
		cgp = (struct cg *)bp->b_data;
		if (!cg_chkmagic(cgp)) {
			brelse(bp);
			error = EIO;
			goto out1;
		}
		error = bread(vp, fragstoblks(fs, cgtod(fs, cg)), fs->fs_bsize,
			KERNCRED, &nbp);
		if (error) {
			brelse(bp);
			brelse(nbp);
			goto out1;
		}
		bcopy(bp->b_data, nbp->b_data, fs->fs_cgsize);
		if (fs->fs_cgsize < fs->fs_bsize)
			bzero(&nbp->b_data[fs->fs_cgsize],
			    fs->fs_bsize - fs->fs_cgsize);
		nbp->b_flags |= B_VALIDSUSPWRT;
		bawrite(nbp);
		base = cg * fs->fs_fpg / fs->fs_frag;
		if (base + len > numblks)
			len = numblks - base;
		loc = 0;
		if (base < NDADDR) {
			for ( ; loc < NDADDR; loc++) {
				if (!ffs_isblock(fs, cg_blksfree(cgp), loc))
					continue;
				ip->i_db[loc] = BLK_NOCOPY;
			}
		}
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)(base + loc)),
		    fs->fs_bsize, KERNCRED, B_METAONLY, &ibp);
		if (error) {
			brelse(bp);
			goto out1;
		}
		indiroff = (base + loc - NDADDR) % NINDIR(fs);
		for ( ; loc < len; loc++, indiroff++) {
			if (indiroff >= NINDIR(fs)) {
				ibp->b_flags |= B_VALIDSUSPWRT;
				bawrite(ibp);
				error = VOP_BALLOC(vp,
				    lblktosize(fs, (off_t)(base + loc)),
				    fs->fs_bsize, KERNCRED, B_METAONLY, &ibp);
				if (error) {
					brelse(bp);
					goto out1;
				}
				indiroff = 0;
			}
			if (!ffs_isblock(fs, cg_blksfree(cgp), loc))
				continue;
			((ufs_daddr_t *)(ibp->b_data))[indiroff] = BLK_NOCOPY;
		}
		bqrelse(bp);
		ibp->b_flags |= B_VALIDSUSPWRT;
		bdwrite(ibp);
	}
	/*
	 * Snapshot the superblock and its summary information.
	 */
	error = VOP_BALLOC(vp, (off_t)(SBOFF), fs->fs_bsize, KERNCRED,
	    0, &nbp);
	if (error)
		goto out1;
	bcopy(fs, nbp->b_data, fs->fs_sbsize);
	((struct fs *)(nbp->b_data))->fs_clean = 1;
	if (fs->fs_sbsize < fs->fs_bsize)
		bzero(&nbp->b_data[fs->fs_sbsize],
		    fs->fs_bsize - fs->fs_sbsize);
	nbp->b_flags |= B_VALIDSUSPWRT;
	bawrite(nbp);
	blkno = fragstoblks(fs, fs->fs_csaddr);
	len = howmany(fs->fs_cssize, fs->fs_bsize) - 1;
	size = fs->fs_bsize;
	for (loc = 0; loc <= len; loc++) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)(blkno + loc)),
		    fs->fs_bsize, KERNCRED, 0, &nbp);
		if (error)
			goto out1;
		if (loc == len) {
			readblock(nbp, blkno + loc);
			size = fs->fs_cssize % fs->fs_bsize;
		}
		bcopy(fs->fs_csp[loc], nbp->b_data, size);
		nbp->b_flags |= B_VALIDSUSPWRT;
		bawrite(nbp);
	}
	/*
	 * Copy the shadow blocks for the snapshot inodes so that
	 * the copies can can be expunged.
	 */
	for (loc = 0; loc < inoblkcnt; loc++) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)inoblks[loc]),
		    fs->fs_bsize, KERNCRED, 0, &nbp);
		if (error)
			goto out1;
		readblock(nbp, inoblks[loc]);
		nbp->b_flags |= B_VALIDSUSPWRT;
		bdwrite(nbp);
	}
	/*
	 * Copy allocation information from other snapshots and then
	 * expunge them from the view of the current snapshot.
	 */
	for (xp = devip->i_copyonwrite; xp; xp = xp->i_copyonwrite) {
		/*
		 * Before expunging a snapshot inode, note all the
		 * blocks that it claims with BLK_SNAP so that fsck will
		 * be able to account for those blocks properly and so
		 * that this snapshot knows that it need not copy them
		 * if the other snapshot holding them is freed.
		 */
		if ((error = snapacct(vp, &xp->i_db[0], &xp->i_ib[NIADDR])) !=0)
			goto out1;
		blksperindir = 1;
		lbn = -NDADDR;
		len = numblks - NDADDR;
		rlbn = NDADDR;
		for (i = 0; len > 0 && i < NIADDR; i++) {
			error = indiracct(vp, ITOV(xp), i, xp->i_ib[i], lbn,
			    rlbn, len, blksperindir);
			if (error)
				goto out1;
			blksperindir *= NINDIR(fs);
			lbn -= blksperindir + 1;
			len -= blksperindir;
			rlbn += blksperindir;
		}
		/*
		 * Set copied snapshot inode to be a zero length file.
		 */
		blkno = fragstoblks(fs, ino_to_fsba(fs, xp->i_number));
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
		    fs->fs_bsize, KERNCRED, 0, &nbp);
		if (error)
			goto out1;
		dip = (struct dinode *)nbp->b_data +
		    ino_to_fsbo(fs, xp->i_number);
		dip->di_size = 0;
		dip->di_blocks = 0;
		dip->di_flags &= ~SF_SNAPSHOT;
		bzero(&dip->di_db[0], (NDADDR + NIADDR) * sizeof(ufs_daddr_t));
		nbp->b_flags |= B_VALIDSUSPWRT;
		bdwrite(nbp);
	}
	/*
	 * Copy all indirect blocks to their shadows (allocated above)
	 * to avoid deadlock in ffs_copyonwrite.
	 */
	for (blkno = NDADDR; blkno < numblks; blkno += NINDIR(fs)) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
		    fs->fs_bsize, p->p_ucred, B_METAONLY, &ibp);
		if (error)
			goto out1;
		copyblkno = fragstoblks(fs, dbtofsb(fs, ibp->b_blkno));
		bqrelse(ibp);
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)copyblkno),
		    fs->fs_bsize, p->p_ucred, 0, &nbp);
		if (error)
			goto out1;
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
		    fs->fs_bsize, p->p_ucred, B_METAONLY, &ibp);
		if (error) {
			brelse(nbp);
			goto out1;
		}
		bcopy(ibp->b_data, nbp->b_data, fs->fs_bsize);
		bqrelse(ibp);
		nbp->b_flags |= B_VALIDSUSPWRT;
		bawrite(nbp);
	}
	/*
	 * Record snapshot inode. Since this is the newest snapshot,
	 * it must be placed at the end of the list.
	 */
	fs->fs_snapinum[snaploc] = ip->i_number;
	if (ip->i_copyonwrite != 0)
		panic("ffs_snapshot: %d already on list", ip->i_number);
	if (devip->i_copyonwrite == 0) {
		devvp->v_flag |= VCOPYONWRITE;
		devip->i_copyonwrite = ip;
	} else {
		for (xp = devip->i_copyonwrite; xp->i_copyonwrite != 0; )
			xp = xp->i_copyonwrite;
		xp->i_copyonwrite = ip;
	}
	vp->v_flag |= VSYSTEM;
	/*
	 * Resume operation on filesystem.
	 */
out1:
	vfs_write_resume(vp->v_mount);
	vn_start_write(NULL, &wrtmp, V_WAIT);
out:
	mp->mnt_flag = flag;
	(void) VOP_FSYNC(vp, KERNCRED, MNT_WAIT, p);
	if (error)
		vput(vp);
	else
		VOP_UNLOCK(vp, 0, p);
	vn_finished_write(wrtmp);
	return (error);
}

/*
 * Descend an indirect block chain for vnode cancelvp accounting for all
 * its indirect blocks in snapvp.
 */ 
static int
indiracct(snapvp, cancelvp, level, blkno, lbn, rlbn, remblks, blksperindir)
	struct vnode *snapvp;
	struct vnode *cancelvp;
	int level;
	ufs_daddr_t blkno;
	int lbn;
	int rlbn;
	int remblks;
	int blksperindir;
{
	int subblksperindir, error, last, num, i;
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t *bap;
	struct buf *bp;
	struct fs *fs;

	if ((error = ufs_getlbns(cancelvp, rlbn, indirs, &num)) != 0)
		return (error);
	if (lbn != indirs[num - 1 - level].in_lbn || blkno == 0 || num < 2)
		panic("indiracct: botched params");
	/*
	 * We have to expand bread here since it will deadlock looking
	 * up the block number for any blocks that are not in the cache.
	 */
	fs = VTOI(cancelvp)->i_fs;
	bp = getblk(cancelvp, lbn, fs->fs_bsize, 0, 0);
	bp->b_blkno = fsbtodb(fs, blkno);
	if ((bp->b_flags & (B_DONE | B_DELWRI)) == 0 &&
	    (error = readblock(bp, fragstoblks(fs, blkno)))) {
		brelse(bp);
		return (error);
	}
	/*
	 * Account for the block pointers in this indirect block.
	 */
	last = howmany(remblks, blksperindir);
	if (last > NINDIR(fs))
		last = NINDIR(fs);
	if (snapvp != cancelvp) {
		bap = (ufs_daddr_t *)bp->b_data;
	} else {
		MALLOC(bap, ufs_daddr_t *, fs->fs_bsize, M_DEVBUF, M_WAITOK);
		bcopy(bp->b_data, (caddr_t)bap, fs->fs_bsize);
		bqrelse(bp);
	}
	error = snapacct(snapvp, &bap[0], &bap[last]);
	if (error || level == 0)
		goto out;
	/*
	 * Account for the block pointers in each of the indirect blocks
	 * in the levels below us.
	 */
	subblksperindir = blksperindir / NINDIR(fs);
	for (lbn++, level--, i = 0; i < last; i++) {
		error = indiracct(snapvp, cancelvp, level, bap[i], lbn,
		    rlbn, remblks, subblksperindir);
		if (error)
			goto out;
		rlbn += blksperindir;
		lbn -= blksperindir;
		remblks -= blksperindir;
	}
out:
	if (snapvp != cancelvp)
		bqrelse(bp);
	else
		FREE(bap, M_DEVBUF);
	return (error);
}

/*
 * Account for a set of blocks allocated in a snapshot inode.
 */
static int
snapacct(vp, oldblkp, lastblkp)
	struct vnode *vp;
	ufs_daddr_t *oldblkp, *lastblkp;
{
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	ufs_daddr_t lbn, blkno, *blkp;
	struct buf *ibp;
	int error;

	for ( ; oldblkp < lastblkp; oldblkp++) {
		blkno = *oldblkp;
		if (blkno == 0 || blkno == BLK_NOCOPY || blkno == BLK_SNAP)
			continue;
		lbn = fragstoblks(fs, blkno);
		if (lbn < NDADDR) {
			blkp = &ip->i_db[lbn];
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		} else {
			error = VOP_BALLOC(vp, lblktosize(fs, (off_t)lbn),
			    fs->fs_bsize, KERNCRED, B_METAONLY, &ibp);
			if (error)
				return (error);
			blkp = &((ufs_daddr_t *)(ibp->b_data))
			    [(lbn - NDADDR) % NINDIR(fs)];
		}
		if (*blkp != 0)
			panic("snapacct: bad block");
		*blkp = BLK_SNAP;
		if (lbn >= NDADDR) {
			ibp->b_flags |= B_VALIDSUSPWRT;
			bdwrite(ibp);
		}
	}
	return (0);
}

/*
 * Prepare a snapshot file for being removed.
 */
void
ffs_snapremove(vp)
	struct vnode *vp;
{
	struct inode *ip, *xp;
	struct vnode *devvp;
	struct buf *ibp;
	struct fs *fs;
	ufs_daddr_t blkno, dblk;
	int error, snaploc, loc, last;

	ip = VTOI(vp);
	fs = ip->i_fs;
	/*
	 * Delete snapshot inode from superblock. Keep list dense.
	 */
	for (snaploc = 0; snaploc < FSMAXSNAP; snaploc++)
		if (fs->fs_snapinum[snaploc] == ip->i_number)
			break;
	if (snaploc < FSMAXSNAP) {
		for (snaploc++; snaploc < FSMAXSNAP; snaploc++) {
			if (fs->fs_snapinum[snaploc] == 0)
				break;
			fs->fs_snapinum[snaploc - 1] = fs->fs_snapinum[snaploc];
		}
		fs->fs_snapinum[snaploc - 1] = 0;
	}
	/*
	 * Delete from incore list.
	 * Clear copy-on-write flag if last snapshot.
	 */
	devvp = ip->i_devvp;
	for (xp = VTOI(devvp); xp; xp = xp->i_copyonwrite) {
		if (xp->i_copyonwrite != ip)
			continue;
		xp->i_copyonwrite = ip->i_copyonwrite;
		ip->i_copyonwrite = 0;
		break;
	}
	if (xp == 0)
		printf("ffs_snapremove: lost snapshot vnode %d\n",
		    ip->i_number);
	if (VTOI(devvp)->i_copyonwrite == 0)
		devvp->v_flag &= ~VCOPYONWRITE;
	/*
	 * Clear all BLK_NOCOPY fields. Pass any block claims to other
	 * snapshots that want them (see ffs_snapblkfree below).
	 */
	for (blkno = 1; blkno < NDADDR; blkno++) {
		dblk = ip->i_db[blkno];
		if (dblk == BLK_NOCOPY || dblk == BLK_SNAP ||
		    (dblk == blkstofrags(fs, blkno) &&
		     ffs_snapblkfree(ip, dblk, fs->fs_bsize)))
			ip->i_db[blkno] = 0;
	}
	for (blkno = NDADDR; blkno < fs->fs_size; blkno += NINDIR(fs)) {
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)blkno),
		    fs->fs_bsize, KERNCRED, B_METAONLY, &ibp);
		if (error)
			continue;
		if ((last = fs->fs_size - blkno) > NINDIR(fs))
			last = NINDIR(fs);
		for (loc = 0; loc < last; loc++) {
			dblk = ((ufs_daddr_t *)(ibp->b_data))[loc];
			if (dblk == BLK_NOCOPY || dblk == BLK_SNAP ||
			    (dblk == blkstofrags(fs, blkno) &&
			     ffs_snapblkfree(ip, dblk, fs->fs_bsize)))
				((ufs_daddr_t *)(ibp->b_data))[loc] = 0;
		}
		bawrite(ibp);
	}
	/*
	 * Clear snapshot flag and drop reference.
	 */
	ip->i_flags &= ~SF_SNAPSHOT;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
}

/*
 * Notification that a block is being freed. Return zero if the free
 * should be allowed to proceed. Return non-zero if the snapshot file
 * wants to claim the block. The block will be claimed if it is an
 * uncopied part of one of the snapshots. It will be freed if it is
 * either a BLK_NOCOPY or has already been copied in all of the snapshots.
 * If a fragment is being freed, then all snapshots that care about
 * it must make a copy since a snapshot file can only claim full sized
 * blocks. Note that if more than one snapshot file maps the block,
 * we can pick one at random to claim it. Since none of the snapshots
 * can change, we are assurred that they will all see the same unmodified
 * image. When deleting a snapshot file (see ffs_snapremove above), we
 * must push any of these claimed blocks to one of the other snapshots
 * that maps it. These claimed blocks are easily identified as they will
 * have a block number equal to their logical block number within the
 * snapshot. A copied block can never have this property because they
 * must always have been allocated from a BLK_NOCOPY location.
 */
int
ffs_snapblkfree(freeip, bno, size)
	struct inode *freeip;
	ufs_daddr_t bno;
	long size;
{
	struct buf *ibp, *cbp, *savedcbp = 0;
	struct fs *fs = freeip->i_fs;
	struct proc *p = CURPROC;
	struct inode *ip;
	struct vnode *vp;
	ufs_daddr_t lbn, blkno;
	int indiroff = 0, error = 0, claimedblk = 0;

	lbn = fragstoblks(fs, bno);
	for (ip = VTOI(freeip->i_devvp)->i_copyonwrite; ip;
	     ip = ip->i_copyonwrite) {
		vp = ITOV(ip);
		/*
		 * Lookup block being written.
		 */
		if (lbn < NDADDR) {
			blkno = ip->i_db[lbn];
		} else {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			p->p_flag |= P_COWINPROGRESS;
			error = VOP_BALLOC(vp, lblktosize(fs, (off_t)lbn),
			    fs->fs_bsize, KERNCRED, B_METAONLY, &ibp);
			p->p_flag &= ~P_COWINPROGRESS;
			VOP_UNLOCK(vp, 0, p);
			if (error)
				break;
			indiroff = (lbn - NDADDR) % NINDIR(fs);
			blkno = ((ufs_daddr_t *)(ibp->b_data))[indiroff];
		}
		/*
		 * Check to see if block needs to be copied.
		 */
		switch (blkno) {
		/*
		 * If the snapshot has already copied the block (default),
		 * or does not care about the block, it is not needed.
		 */
		default:
		case BLK_NOCOPY:
			if (lbn >= NDADDR)
				bqrelse(ibp);
			continue;
		/*
		 * No previous snapshot claimed the block, so it will be
		 * freed and become a BLK_NOCOPY (don't care) for us.
		 */
		case BLK_SNAP:
			if (claimedblk)
				panic("snapblkfree: inconsistent block type");
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			if (lbn < NDADDR) {
				ip->i_db[lbn] = BLK_NOCOPY;
				ip->i_flag |= IN_CHANGE | IN_UPDATE;
			} else {
				((ufs_daddr_t *)(ibp->b_data))[indiroff] =
				    BLK_NOCOPY;
				bdwrite(ibp);
			}
			VOP_UNLOCK(vp, 0, p);
			continue;
		/*
		 * A block that we map is being freed. If it has not been
		 * claimed yet, we will claim or copy it (below).
		 */
		case 0:
			claimedblk = 1;
			break;
		}
		/*
		 * If this is a full size block, we will just grab it
		 * and assign it to the snapshot inode. Otherwise we
		 * will proceed to copy it. See explanation for this
		 * routine as to why only a single snapshot needs to
		 * claim this block.
		 */
		if (size == fs->fs_bsize) {
#ifdef DEBUG
			if (snapdebug)
				printf("%s %d lbn %d from inum %d\n",
				    "Grabonremove: snapino", ip->i_number, lbn,
				    freeip->i_number);
#endif
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			if (lbn < NDADDR) {
				ip->i_db[lbn] = bno;
			} else {
				((ufs_daddr_t *)(ibp->b_data))[indiroff] = bno;
				bdwrite(ibp);
			}
			ip->i_blocks += btodb(size);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			VOP_UNLOCK(vp, 0, p);
			return (1);
		}
		if (lbn >= NDADDR)
			bqrelse(ibp);
		/*
		 * Allocate the block into which to do the copy. Note that this
		 * allocation will never require any additional allocations for
		 * the snapshot inode.
		 */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		p->p_flag |= P_COWINPROGRESS;
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)lbn),
		    fs->fs_bsize, KERNCRED, 0, &cbp);
		p->p_flag &= ~P_COWINPROGRESS;
		VOP_UNLOCK(vp, 0, p);
		if (error)
			break;
#ifdef DEBUG
		if (snapdebug)
			printf("%s%d lbn %d for inum %d size %ld to blkno %d\n",
			    "Copyonremove: snapino ", ip->i_number, lbn,
			    freeip->i_number, size, cbp->b_blkno);
#endif
		/*
		 * If we have already read the old block contents, then
		 * simply copy them to the new block.
		 */
		if (savedcbp != 0) {
			bcopy(savedcbp->b_data, cbp->b_data, fs->fs_bsize);
			bawrite(cbp);
			continue;
		}
		/*
		 * Otherwise, read the old block contents into the buffer.
		 */
		if ((error = readblock(cbp, lbn)) != 0)
			break;
		savedcbp = cbp;
	}
	if (savedcbp)
		bawrite(savedcbp);
	/*
	 * If we have been unable to allocate a block in which to do
	 * the copy, then return non-zero so that the fragment will
	 * not be freed. Although space will be lost, the snapshot
	 * will stay consistent.
	 */
	return (error);
}

/*
 * Associate snapshot files when mounting.
 */
void
ffs_snapshot_mount(mp)
	struct mount *mp;
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	struct proc *p = CURPROC;
	struct inode *ip, **listtailp;
	struct vnode *vp;
	int error, snaploc, loc;

	listtailp = &VTOI(ump->um_devvp)->i_copyonwrite;
	for (snaploc = 0; snaploc < FSMAXSNAP; snaploc++) {
		if (fs->fs_snapinum[snaploc] == 0)
			return;
		if ((error = VFS_VGET(mp, fs->fs_snapinum[snaploc], &vp)) != 0){
			printf("ffs_snapshot_mount: vget failed %d\n", error);
			continue;
		}
		ip = VTOI(vp);
		if ((ip->i_flags & SF_SNAPSHOT) == 0) {
			printf("ffs_snapshot_mount: non-snapshot inode %d\n",
			    fs->fs_snapinum[snaploc]);
			vput(vp);
			for (loc = snaploc + 1; loc < FSMAXSNAP; loc++) {
				if (fs->fs_snapinum[loc] == 0)
					break;
				fs->fs_snapinum[loc - 1] = fs->fs_snapinum[loc];
			}
			fs->fs_snapinum[loc - 1] = 0;
			snaploc--;
			continue;
		}
		if (ip->i_copyonwrite != 0)
			panic("ffs_snapshot_mount: %d already on list",
			    ip->i_number);
		*listtailp = ip;
		listtailp = &ip->i_copyonwrite;
		vp->v_flag |= VSYSTEM;
		VOP_UNLOCK(vp, 0, p);
		ump->um_devvp->v_flag |= VCOPYONWRITE;
	}
}

/*
 * Disassociate snapshot files when unmounting.
 */
void
ffs_snapshot_unmount(mp)
	struct mount *mp;
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct inode *devip = VTOI(ump->um_devvp);
	struct inode *xp;

	while ((xp = devip->i_copyonwrite) != 0) {
		devip->i_copyonwrite = xp->i_copyonwrite;
		xp->i_copyonwrite = 0;
		if (xp->i_effnlink > 0)
			vrele(ITOV(xp));
	}
	ump->um_devvp->v_flag &= ~VCOPYONWRITE;
}

/*
 * Check for need to copy block that is about to be written,
 * copying the block if necessary.
 */
int
ffs_copyonwrite(ap)
	struct vop_copyonwrite_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *ibp, *cbp, *savedcbp = 0, *bp = ap->a_bp;
	struct fs *fs = VTOI(bp->b_vp)->i_fs;
	struct proc *p = CURPROC;
	struct inode *ip;
	struct vnode *vp;
	ufs_daddr_t lbn, blkno;
	int indiroff, error = 0;

	lbn = fragstoblks(fs, dbtofsb(fs, bp->b_blkno));
	if (p->p_flag & P_COWINPROGRESS)
		panic("ffs_copyonwrite: recursive call");
	for (ip = VTOI(ap->a_vp)->i_copyonwrite; ip; ip = ip->i_copyonwrite) {
		vp = ITOV(ip);
		/*
		 * We ensure that everything of our own that needs to be
		 * copied will be done at the time that ffs_snapshot is
		 * called. Thus we can skip the check here which can
		 * deadlock in doing the lookup in VOP_BALLOC.
		 */
		if (bp->b_vp == vp)
			continue;
		/*
		 * Check to see if block needs to be copied. We have to
		 * be able to do the VOP_BALLOC without blocking, otherwise
		 * we may get in a deadlock with another process also
		 * trying to allocate. If we find outselves unable to
		 * get the buffer lock, we unlock the snapshot vnode,
		 * sleep briefly, and try again.
		 */
retry:
		vn_lock(vp, LK_SHARED | LK_RETRY, p);
		if (lbn < NDADDR) {
			blkno = ip->i_db[lbn];
		} else {
			p->p_flag |= P_COWINPROGRESS;
			error = VOP_BALLOC(vp, lblktosize(fs, (off_t)lbn),
			   fs->fs_bsize, KERNCRED, B_METAONLY | B_NOWAIT, &ibp);
			p->p_flag &= ~P_COWINPROGRESS;
			if (error) {
				VOP_UNLOCK(vp, 0, p);
				if (error != EWOULDBLOCK)
					break;
				tsleep(vp, p->p_usrpri, "nap", 1);
				goto retry;
			}
			indiroff = (lbn - NDADDR) % NINDIR(fs);
			blkno = ((ufs_daddr_t *)(ibp->b_data))[indiroff];
			bqrelse(ibp);
		}
#ifdef DIAGNOSTIC
		if (blkno == BLK_SNAP && bp->b_lblkno >= 0)
			panic("ffs_copyonwrite: bad copy block");
#endif
		if (blkno != 0) {
			VOP_UNLOCK(vp, 0, p);
			continue;
		}
		/*
		 * Allocate the block into which to do the copy. Note that this
		 * allocation will never require any additional allocations for
		 * the snapshot inode.
		 */
		p->p_flag |= P_COWINPROGRESS;
		error = VOP_BALLOC(vp, lblktosize(fs, (off_t)lbn),
		    fs->fs_bsize, KERNCRED, B_NOWAIT, &cbp);
		p->p_flag &= ~P_COWINPROGRESS;
		VOP_UNLOCK(vp, 0, p);
		if (error) {
			if (error != EWOULDBLOCK)
				break;
			tsleep(vp, p->p_usrpri, "nap", 1);
			goto retry;
		}
#ifdef DEBUG
		if (snapdebug) {
			printf("Copyonwrite: snapino %d lbn %d for ",
			    ip->i_number, lbn);
			if (bp->b_vp == ap->a_vp)
				printf("fs metadata");
			else
				printf("inum %d", VTOI(bp->b_vp)->i_number);
			printf(" lblkno %d to blkno %d\n", bp->b_lblkno,
			    cbp->b_blkno);
		}
#endif
		/*
		 * If we have already read the old block contents, then
		 * simply copy them to the new block.
		 */
		if (savedcbp != 0) {
			bcopy(savedcbp->b_data, cbp->b_data, fs->fs_bsize);
			bawrite(cbp);
			continue;
		}
		/*
		 * Otherwise, read the old block contents into the buffer.
		 */
		if ((error = readblock(cbp, lbn)) != 0)
			break;
		savedcbp = cbp;
	}
	if (savedcbp)
		bawrite(savedcbp);
	return (error);
}

/*
 * Read the specified block into the given buffer.
 * Much of this boiler-plate comes from bwrite().
 */
static int
readblock(bp, lbn)
	struct buf *bp;
	daddr_t lbn;
{
	struct uio auio;
	struct iovec aiov;
	struct proc *p = CURPROC;
	struct inode *ip = VTOI(bp->b_vp);

	aiov.iov_base = bp->b_data;
	aiov.iov_len = bp->b_bcount;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = dbtob(fsbtodb(ip->i_fs, blkstofrags(ip->i_fs, lbn)));
	auio.uio_resid = bp->b_bcount;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	return (physio(ip->i_devvp->v_rdev, &auio, 0));
}
