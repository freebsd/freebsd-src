/*
 * Copyright (c) 1999, 2000
 *	Adrian Chadd <adrian@FreeBSD.org>
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
 *	@(#)ffs_vfsops.c	8.31 (Berkeley) 5/20/95
 * $FreeBSD$
 */


#include "opt_ffs.h"
#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ifs/ifs_extern.h>

#include <vm/vm.h>
#include <vm/vm_page.h>



static MALLOC_DEFINE(M_IFSNODE, "IFS node", "IFS vnode private part");

static int	ifs_init (struct vfsconf *);
static int	ifs_mount (struct mount *, char *, caddr_t,
		    struct nameidata *, struct thread *);
extern int	ifs_vget (struct mount *, ino_t, int, struct vnode **);



static struct vfsops ifs_vfsops = {
	ifs_mount,
	ufs_start,
	ffs_unmount,
	ufs_root,
	ufs_quotactl,
	ffs_statfs,
	ffs_sync,
	ifs_vget,
	ffs_fhtovp,
	vfs_stdcheckexp,
	ffs_vptofh,
	ifs_init,
        vfs_stduninit,
        vfs_stdextattrctl,
};

VFS_SET(ifs_vfsops, ifs, 0);

/*
 * ifs_mount
 *
 * A simple wrapper around ffs_mount - IFS filesystems right now can't
 * deal with softupdates so we make sure the user isn't trying to use it.
 */
static int
ifs_mount(mp, path, data, ndp, td)
	struct mount		*mp;
	char			*path;
	caddr_t			data;
	struct nameidata	*ndp;
	struct thread		*td;
{
	/* Clear the softdep flag */
	mp->mnt_flag &= ~MNT_SOFTDEP;
	return (ffs_mount(mp, path, data, ndp, td));
}



/*
 * Look up a IFS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
static int ifs_inode_hash_lock;
/*
 * ifs_inode_hash_lock is a variable to manage mutual exclusion
 * of vnode allocation and intertion to the hash, especially to
 * avoid holding more than one vnodes for the same inode in the
 * hash table. ifs_inode_hash_lock must hence be tested-and-set
 * or cleared atomically, accomplished by ifs_inode_hash_mtx.
 * 
 * As vnode allocation may block during MALLOC() and zone
 * allocation, we should also do msleep() to give away the CPU
 * if anyone else is allocating a vnode. lockmgr is not suitable
 * here because someone else may insert to the hash table the
 * vnode we are trying to allocate during our sleep, in which
 * case the hash table needs to be examined once again after
 * waking up.
 */
static struct mtx ifs_inode_hash_mtx;

/*
 * Initialize the filesystem; just use ufs_init.
 */
static int
ifs_init(vfsp)
	struct vfsconf *vfsp;
{
	mtx_init(&ifs_inode_hash_mtx, "ifsvgt", MTX_DEF);
	return (ufs_init(vfsp));
}

int
ifs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	struct fs *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int error, want_wakeup;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
restart:
	if ((error = ufs_ihashget(dev, ino, flags, vpp)) != 0)
		return (error);
	if (*vpp != NULL)
		return (0);

	/*
	 * Lock out the creation of new entries in the FFS hash table in
	 * case getnewvnode() or MALLOC() blocks, otherwise a duplicate
	 * may occur!
	 */
	mtx_lock(&ifs_inode_hash_mtx);
	if (ifs_inode_hash_lock) {
		while (ifs_inode_hash_lock) {
			ifs_inode_hash_lock = -1;
			msleep(&ifs_inode_hash_lock, &ifs_inode_hash_mtx, PVM, "ifsvgt", 0);
		}
		mtx_unlock(&ifs_inode_hash_mtx);
		goto restart;
	}
	ifs_inode_hash_lock = 1;
	mtx_unlock(&ifs_inode_hash_mtx);

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ffs_sync() if a sync happens to fire right then,
	 * which will cause a panic because ffs_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), 
	    ump->um_malloctype, M_WAITOK);

	/* Allocate a new vnode/inode. */
	error = getnewvnode(VT_UFS, mp, ifs_vnodeop_p, &vp);
	if (error) {
		/*
		 * Do not wake up processes while holding the mutex,
		 * otherwise the processes waken up immediately hit
		 * themselves into the mutex.
		 */
		mtx_lock(&ifs_inode_hash_mtx);
		want_wakeup = ifs_inode_hash_lock < 0;
		ifs_inode_hash_lock = 0;
		mtx_unlock(&ifs_inode_hash_mtx);
		if (want_wakeup)
			wakeup(&ifs_inode_hash_lock);
		*vpp = NULL;
		FREE(ip, ump->um_malloctype);
		return (error);
	}
	bzero((caddr_t)ip, sizeof(struct inode));
	/*
	 * IFS supports lock sharing in the stack of vnodes
	 */
	vp->v_vnlock = &vp->v_lock;
	lockinit(vp->v_vnlock, PINOD, "inode", VLKTIMEOUT, LK_CANRECURSE);
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_fs = fs = ump->um_fs;
	ip->i_dev = dev;
	ip->i_number = ino;
#ifdef QUOTA
	{
		int i;
		for (i = 0; i < MAXQUOTAS; i++)
			ip->i_dquot[i] = NODQUOT;
	}
#endif
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ufs_ihashins(ip);

	/*
	 * Do not wake up processes while holding the mutex,
	 * otherwise the processes waken up immediately hit
	 * themselves into the mutex.
	 */
	mtx_lock(&ifs_inode_hash_mtx);
	want_wakeup = ifs_inode_hash_lock < 0;
	ifs_inode_hash_lock = 0;
	mtx_unlock(&ifs_inode_hash_mtx);
	if (want_wakeup)
		wakeup(&ifs_inode_hash_lock);

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		brelse(bp);
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	ip->i_din = *((struct dinode *)bp->b_data + ino_to_fsbo(fs, ino));
	if (DOINGSOFTDEP(vp))
		softdep_load_inodeblock(ip);
	else
		ip->i_effnlink = ip->i_nlink;
	bqrelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	error = ufs_vinit(mp, ifs_specop_p, ifs_fifoop_p, &vp);
	if (error) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		ip->i_gen = random() / 2 + 1;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {		/* XXX */
		ip->i_uid = ip->i_din.di_ouid;		/* XXX */
		ip->i_gid = ip->i_din.di_ogid;		/* XXX */
	}						/* XXX */

	*vpp = vp;
	return (0);
}
