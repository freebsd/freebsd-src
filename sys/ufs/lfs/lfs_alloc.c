/*
 * Copyright (c) 1991, 1993, 1995
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
 *	@(#)lfs_alloc.c	8.7 (Berkeley) 5/14/95
 * $Id: lfs_alloc.c,v 1.14 1997/03/23 00:45:07 bde Exp $
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

extern u_long nextgennumber;

/* Allocate a new inode. */
/* ARGSUSED */
int
lfs_valloc(ap)
	struct vop_valloc_args /* {
		struct vnode *a_pvp;
		int a_mode;
		struct ucred *a_cred;
		struct vnode **a_vpp;
	} */ *ap;
{
	struct lfs *fs;
	struct buf *bp;
	struct ifile *ifp;
	struct inode *ip;
	struct vnode *vp;
	ufs_daddr_t blkno;
	ino_t new_ino;
	u_long i, max;
	int error;

	/* Get the head of the freelist. */
	fs = VTOI(ap->a_pvp)->i_lfs;
	new_ino = fs->lfs_free;
#ifdef ALLOCPRINT
	printf("lfs_ialloc: allocate inode %d\n", new_ino);
#endif

	/*
	 * Remove the inode from the free list and write the new start
	 * of the free list into the superblock.
	 */
	LFS_IENTRY(ifp, fs, new_ino, bp);
	if (ifp->if_daddr != LFS_UNUSED_DADDR)
		panic("lfs_ialloc: inuse inode on the free list");
	fs->lfs_free = ifp->if_nextfree;
	brelse(bp);

	/* Extend IFILE so that the next lfs_valloc will succeed. */
	if (fs->lfs_free == LFS_UNUSED_INUM) {
		vp = fs->lfs_ivnode;
		ip = VTOI(vp);
		blkno = lblkno(fs, ip->i_size);
		lfs_balloc(vp, 0, fs->lfs_bsize, blkno, &bp);
		ip->i_size += fs->lfs_bsize;
		vnode_pager_setsize(vp, (u_long)ip->i_size);

		i = (blkno - fs->lfs_segtabsz - fs->lfs_cleansz) *
		    fs->lfs_ifpb;
		fs->lfs_free = i;
		max = i + fs->lfs_ifpb;
		for (ifp = (struct ifile *)bp->b_data; i < max; ++ifp) {
			ifp->if_version = 1;
			ifp->if_daddr = LFS_UNUSED_DADDR;
			ifp->if_nextfree = ++i;
		}
		ifp--;
		ifp->if_nextfree = LFS_UNUSED_INUM;
		if (error = VOP_BWRITE(bp))
			return (error);
	}

	/* Create a vnode to associate with the inode. */
	if (error = lfs_vcreate(ap->a_pvp->v_mount, new_ino, &vp))
		return (error);


	ip = VTOI(vp);
	/* Zero out the direct and indirect block addresses. */
	bzero(&ip->i_din, sizeof(struct dinode));
	ip->i_din.di_inumber = new_ino;

	/* Set a new generation number for this inode. */
	if (++nextgennumber < (u_long)time.tv_sec)
		nextgennumber = time.tv_sec;
	ip->i_gen = nextgennumber;

	/* Insert into the inode hash table. */
	ufs_ihashins(ip);

	if (error = ufs_vinit(vp->v_mount, lfs_specop_p, LFS_FIFOOPS, &vp)) {
		vput(vp);
		*ap->a_vpp = NULL;
		return (error);
	}

	*ap->a_vpp = vp;
	vp->v_flag |= VDIROP;
	VREF(ip->i_devvp);

	/* Set superblock modified bit and increment file count. */
	fs->lfs_fmod = 1;
	++fs->lfs_nfiles;
	return (0);
}

/* Create a new vnode/inode pair and initialize what fields we can. */
int
lfs_vcreate(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	struct inode *ip;
	struct ufsmount *ump;
	int error, i;

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), M_LFSNODE, M_WAITOK);

	/* Create the vnode. */
	if (error = getnewvnode(VT_LFS, mp, lfs_vnodeop_p, vpp)) {
		*vpp = NULL;
		FREE(ip, M_LFSNODE);
		return (error);
	}

	/* Get a pointer to the private mount structure. */
	ump = VFSTOUFS(mp);

	/* Initialize the inode. */
	lockinit(&ip->i_lock, PINOD, "lfsinode", 0, 0);
	(*vpp)->v_data = ip;
	ip->i_vnode = *vpp;
	ip->i_devvp = ump->um_devvp;
	ip->i_flag = IN_MODIFIED;
	ip->i_dev = ump->um_dev;
	ip->i_number = ip->i_din.di_inumber = ino;
	ip->i_lfs = ump->um_lfs;
#ifdef QUOTA
	for (i = 0; i < MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
#endif
	ip->i_lockf = 0;
	ip->i_diroff = 0;
	ip->i_mode = 0;
	ip->i_size = 0;
	ip->i_blocks = 0;
	++ump->um_lfs->lfs_uinodes;
	return (0);
}

/* Free an inode. */
/* ARGUSED */
int
lfs_vfree(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{
	SEGUSE *sup;
	struct buf *bp;
	struct ifile *ifp;
	struct inode *ip;
	struct lfs *fs;
	ufs_daddr_t old_iaddr;
	ino_t ino;

	/* Get the inode number and file system. */
	ip = VTOI(ap->a_pvp);
	fs = ip->i_lfs;
	ino = ip->i_number;
	if (ip->i_flag & IN_MODIFIED) {
		--fs->lfs_uinodes;
		ip->i_flag &=
		    ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);
	}
	/*
	 * Set the ifile's inode entry to unused, increment its version number
	 * and link it into the free chain.
	 */
	LFS_IENTRY(ifp, fs, ino, bp);
	old_iaddr = ifp->if_daddr;
	ifp->if_daddr = LFS_UNUSED_DADDR;
	++ifp->if_version;
	ifp->if_nextfree = fs->lfs_free;
	fs->lfs_free = ino;
	(void) VOP_BWRITE(bp);

	if (old_iaddr != LFS_UNUSED_DADDR) {
		LFS_SEGENTRY(sup, fs, datosn(fs, old_iaddr), bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes < sizeof(struct dinode))
			panic("lfs_vfree: negative byte count (segment %d)",
			    datosn(fs, old_iaddr));
#endif
		sup->su_nbytes -= sizeof(struct dinode);
		(void) VOP_BWRITE(bp);
	}

	/* Set superblock modified bit and decrement file count. */
	fs->lfs_fmod = 1;
	--fs->lfs_nfiles;
	return (0);
}
