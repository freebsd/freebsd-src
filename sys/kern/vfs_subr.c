/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_subr.c	8.13 (Berkeley) 4/18/94
 * $Id: vfs_subr.c,v 1.48 1995/12/11 04:56:09 dyson Exp $
 */

/*
 * External virtual filesystem routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/namei.h>
#include <sys/ucred.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <sys/sysctl.h>

#include <miscfs/specfs/specdev.h>

#ifdef DDB
extern void	printlockedvnodes __P((void));
#endif
extern void	vclean __P((struct vnode *vp, int flags));
extern void	vfs_unmountroot __P((struct mount *rootfs));

enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};
int vttoif_tab[9] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK,
	S_IFSOCK, S_IFIFO, S_IFMT,
};

/*
 * Insq/Remq for the vnode usage lists.
 */
#define	bufinsvn(bp, dp)	LIST_INSERT_HEAD(dp, bp, b_vnbufs)
#define	bufremvn(bp) {  \
	LIST_REMOVE(bp, b_vnbufs); \
	(bp)->b_vnbufs.le_next = NOLIST; \
}

TAILQ_HEAD(freelst, vnode) vnode_free_list;	/* vnode free list */
u_long freevnodes	= 0;

struct mntlist mountlist;	/* mounted filesystem list */

int desiredvnodes;
SYSCTL_INT(_kern, KERN_MAXVNODES, maxvnodes, CTLFLAG_RD, &desiredvnodes, 0, "");

static void	vfs_free_addrlist __P((struct netexport *nep));
static int	vfs_free_netcred __P((struct radix_node *rn, void *w));
static int	vfs_hang_addrlist __P((struct mount *mp, struct netexport *nep,
				       struct export_args *argp));

/*
 * Initialize the vnode management data structures.
 */
void
vntblinit()
{
	desiredvnodes = maxproc + vm_object_cache_max;

	TAILQ_INIT(&vnode_free_list);
	CIRCLEQ_INIT(&mountlist);
}

/*
 * Lock a filesystem.
 * Used to prevent access to it while mounting and unmounting.
 */
int
vfs_lock(mp)
	register struct mount *mp;
{

	while (mp->mnt_flag & MNT_MLOCK) {
		mp->mnt_flag |= MNT_MWAIT;
		(void) tsleep((caddr_t) mp, PVFS, "vfslck", 0);
	}
	mp->mnt_flag |= MNT_MLOCK;
	return (0);
}

/*
 * Unlock a locked filesystem.
 * Panic if filesystem is not locked.
 */
void
vfs_unlock(mp)
	register struct mount *mp;
{

	if ((mp->mnt_flag & MNT_MLOCK) == 0)
		panic("vfs_unlock: not locked");
	mp->mnt_flag &= ~MNT_MLOCK;
	if (mp->mnt_flag & MNT_MWAIT) {
		mp->mnt_flag &= ~MNT_MWAIT;
		wakeup((caddr_t) mp);
	}
}

/*
 * Mark a mount point as busy.
 * Used to synchronize access and to delay unmounting.
 */
int
vfs_busy(mp)
	register struct mount *mp;
{

	while (mp->mnt_flag & MNT_MPBUSY) {
		mp->mnt_flag |= MNT_MPWANT;
		(void) tsleep((caddr_t) &mp->mnt_flag, PVFS, "vfsbsy", 0);
	}
	if (mp->mnt_flag & MNT_UNMOUNT)
		return (1);
	mp->mnt_flag |= MNT_MPBUSY;
	return (0);
}

/*
 * Free a busy filesystem.
 * Panic if filesystem is not busy.
 */
void
vfs_unbusy(mp)
	register struct mount *mp;
{

	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("vfs_unbusy: not busy");
	mp->mnt_flag &= ~MNT_MPBUSY;
	if (mp->mnt_flag & MNT_MPWANT) {
		mp->mnt_flag &= ~MNT_MPWANT;
		wakeup((caddr_t) &mp->mnt_flag);
	}
}

void
vfs_unmountroot(struct mount *rootfs)
{
	struct mount *mp = rootfs;
	int error;

	if (vfs_busy(mp)) {
		printf("failed to unmount root\n");
		return;
	}
	mp->mnt_flag |= MNT_UNMOUNT;
	if ((error = vfs_lock(mp))) {
		printf("lock of root filesystem failed (%d)\n", error);
		return;
	}
	vnode_pager_umount(mp);	/* release cached vnodes */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */

	if ((error = VFS_SYNC(mp, MNT_WAIT, initproc->p_ucred, initproc)))
		printf("sync of root filesystem failed (%d)\n", error);

	if ((error = VFS_UNMOUNT(mp, MNT_FORCE, initproc))) {
		printf("unmount of root filesystem failed (");
		if (error == EBUSY)
			printf("BUSY)\n");
		else
			printf("%d)\n", error);
	}
	mp->mnt_flag &= ~MNT_UNMOUNT;
	vfs_unbusy(mp);
}

/*
 * Unmount all filesystems.  Should only be called by halt().
 */
void
vfs_unmountall()
{
	struct mount *mp, *nmp, *rootfs = NULL;
	int error;

	/* unmount all but rootfs */
	for (mp = mountlist.cqh_last; mp != (void *)&mountlist; mp = nmp) {
		nmp = mp->mnt_list.cqe_prev;

		if (mp->mnt_flag & MNT_ROOTFS) {
			rootfs = mp;
			continue;
		}
		error = dounmount(mp, MNT_FORCE, initproc);
		if (error) {
			printf("unmount of %s failed (", mp->mnt_stat.f_mntonname);
			if (error == EBUSY)
				printf("BUSY)\n");
			else
				printf("%d)\n", error);
		}
	}

	/* and finally... */
	if (rootfs) {
		vfs_unmountroot(rootfs);
	} else {
		printf("no root filesystem\n");
	}
}

/*
 * Lookup a mount point by filesystem identifier.
 */
struct mount *
getvfs(fsid)
	fsid_t *fsid;
{
	register struct mount *mp;

	for (mp = mountlist.cqh_first; mp != (void *)&mountlist;
	    mp = mp->mnt_list.cqe_next) {
		if (mp->mnt_stat.f_fsid.val[0] == fsid->val[0] &&
		    mp->mnt_stat.f_fsid.val[1] == fsid->val[1])
			return (mp);
	}
	return ((struct mount *) 0);
}

/*
 * Get a new unique fsid
 */
void
getnewfsid(mp, mtype)
	struct mount *mp;
	int mtype;
{
	static u_short xxxfs_mntid;

	fsid_t tfsid;

	mp->mnt_stat.f_fsid.val[0] = makedev(nblkdev + mtype, 0);
	mp->mnt_stat.f_fsid.val[1] = mtype;
	if (xxxfs_mntid == 0)
		++xxxfs_mntid;
	tfsid.val[0] = makedev(nblkdev + mtype, xxxfs_mntid);
	tfsid.val[1] = mtype;
	if (mountlist.cqh_first != (void *)&mountlist) {
		while (getvfs(&tfsid)) {
			tfsid.val[0]++;
			xxxfs_mntid++;
		}
	}
	mp->mnt_stat.f_fsid.val[0] = tfsid.val[0];
}

/*
 * Set vnode attributes to VNOVAL
 */
void
vattr_null(vap)
	register struct vattr *vap;
{

	vap->va_type = VNON;
	vap->va_size = VNOVAL;
	vap->va_bytes = VNOVAL;
	vap->va_mode = vap->va_nlink = vap->va_uid = vap->va_gid =
	    vap->va_fsid = vap->va_fileid =
	    vap->va_blocksize = vap->va_rdev =
	    vap->va_atime.ts_sec = vap->va_atime.ts_nsec =
	    vap->va_mtime.ts_sec = vap->va_mtime.ts_nsec =
	    vap->va_ctime.ts_sec = vap->va_ctime.ts_nsec =
	    vap->va_flags = vap->va_gen = VNOVAL;
	vap->va_vaflags = 0;
}

/*
 * Routines having to do with the management of the vnode table.
 */
extern vop_t **dead_vnodeop_p;

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(tag, mp, vops, vpp)
	enum vtagtype tag;
	struct mount *mp;
	vop_t **vops;
	struct vnode **vpp;
{
	register struct vnode *vp;

	vp = vnode_free_list.tqh_first;
	/*
	 * we allocate a new vnode if
	 * 	1. we don't have any free
	 *		Pretty obvious, we actually used to panic, but that
	 *		is a silly thing to do.
	 *	2. we havn't filled our pool yet
	 *		We don't want to trash the incore (VM-)vnodecache.
	 *	3. if less that 1/4th of our vnodes are free.
	 *		We don't want to trash the namei cache either.
	 */
	if (freevnodes < (numvnodes >> 2) ||
	    numvnodes < desiredvnodes ||
	    vp == NULL) {
		vp = (struct vnode *) malloc((u_long) sizeof *vp,
		    M_VNODE, M_WAITOK);
		bzero((char *) vp, sizeof *vp);
		numvnodes++;
	} else {
		TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		freevnodes--;

		if (vp->v_usecount)
			panic("free vnode isn't");

		/* see comment on why 0xdeadb is set at end of vgone (below) */
		vp->v_freelist.tqe_prev = (struct vnode **) 0xdeadb;
		vp->v_lease = NULL;
		if (vp->v_type != VBAD)
			vgone(vp);
#ifdef DIAGNOSTIC
		{
			int s;

			if (vp->v_data)
				panic("cleaned vnode isn't");
			s = splbio();
			if (vp->v_numoutput)
				panic("Clean vnode has pending I/O's");
			splx(s);
		}
#endif
		vp->v_flag = 0;
		vp->v_lastr = 0;
		vp->v_ralen = 0;
		vp->v_maxra = 0;
		vp->v_lastw = 0;
		vp->v_lasta = 0;
		vp->v_cstart = 0;
		vp->v_clen = 0;
		vp->v_socket = 0;
		vp->v_writecount = 0;	/* XXX */
	}
	vp->v_type = VNON;
	cache_purge(vp);
	vp->v_tag = tag;
	vp->v_op = vops;
	insmntque(vp, mp);
	*vpp = vp;
	vp->v_usecount = 1;
	vp->v_data = 0;
	return (0);
}

/*
 * Move a vnode from one mount queue to another.
 */
void
insmntque(vp, mp)
	register struct vnode *vp;
	register struct mount *mp;
{

	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mount != NULL)
		LIST_REMOVE(vp, v_mntvnodes);
	/*
	 * Insert into list of vnodes for the new mount point, if available.
	 */
	if ((vp->v_mount = mp) == NULL)
		return;
	LIST_INSERT_HEAD(&mp->mnt_vnodelist, vp, v_mntvnodes);
}

/*
 * Update outstanding I/O count and do wakeup if requested.
 */
void
vwakeup(bp)
	register struct buf *bp;
{
	register struct vnode *vp;

	bp->b_flags &= ~B_WRITEINPROG;
	if ((vp = bp->b_vp)) {
		vp->v_numoutput--;
		if (vp->v_numoutput < 0)
			panic("vwakeup: neg numoutput");
		if ((vp->v_numoutput == 0) && (vp->v_flag & VBWAIT)) {
			vp->v_flag &= ~VBWAIT;
			wakeup((caddr_t) &vp->v_numoutput);
		}
	}
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(vp, flags, cred, p, slpflag, slptimeo)
	register struct vnode *vp;
	int flags;
	struct ucred *cred;
	struct proc *p;
	int slpflag, slptimeo;
{
	register struct buf *bp;
	struct buf *nbp, *blist;
	int s, error;
	vm_object_t object;

	if (flags & V_SAVE) {
		if ((error = VOP_FSYNC(vp, cred, MNT_WAIT, p)))
			return (error);
		if (vp->v_dirtyblkhd.lh_first != NULL)
			panic("vinvalbuf: dirty bufs");
	}
	for (;;) {
		if ((blist = vp->v_cleanblkhd.lh_first) && (flags & V_SAVEMETA))
			while (blist && blist->b_lblkno < 0)
				blist = blist->b_vnbufs.le_next;
		if (!blist && (blist = vp->v_dirtyblkhd.lh_first) &&
		    (flags & V_SAVEMETA))
			while (blist && blist->b_lblkno < 0)
				blist = blist->b_vnbufs.le_next;
		if (!blist)
			break;

		for (bp = blist; bp; bp = nbp) {
			nbp = bp->b_vnbufs.le_next;
			if ((flags & V_SAVEMETA) && bp->b_lblkno < 0)
				continue;
			s = splbio();
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				error = tsleep((caddr_t) bp,
				    slpflag | (PRIBIO + 1), "vinvalbuf",
				    slptimeo);
				splx(s);
				if (error)
					return (error);
				break;
			}
			bremfree(bp);
			bp->b_flags |= B_BUSY;
			splx(s);
			/*
			 * XXX Since there are no node locks for NFS, I
			 * believe there is a slight chance that a delayed
			 * write will occur while sleeping just above, so
			 * check for it.
			 */
			if ((bp->b_flags & B_DELWRI) && (flags & V_SAVE)) {
				(void) VOP_BWRITE(bp);
				break;
			}
			bp->b_flags |= (B_INVAL|B_NOCACHE|B_RELBUF);
			brelse(bp);
		}
	}

	s = splbio();
	while (vp->v_numoutput > 0) {
		vp->v_flag |= VBWAIT;
		tsleep(&vp->v_numoutput, PVM, "vnvlbv", 0);
	}
	splx(s);

	/*
	 * Destroy the copy in the VM cache, too.
	 */
	object = vp->v_object;
	if (object != NULL) {
		vm_object_page_remove(object, 0, object->size,
		    (flags & V_SAVE) ? TRUE : FALSE);
	}
	if (!(flags & V_SAVEMETA) &&
	    (vp->v_dirtyblkhd.lh_first || vp->v_cleanblkhd.lh_first))
		panic("vinvalbuf: flush failed");
	return (0);
}

/*
 * Associate a buffer with a vnode.
 */
void
bgetvp(vp, bp)
	register struct vnode *vp;
	register struct buf *bp;
{
	int s;

	if (bp->b_vp)
		panic("bgetvp: not free");
	VHOLD(vp);
	bp->b_vp = vp;
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		bp->b_dev = vp->v_rdev;
	else
		bp->b_dev = NODEV;
	/*
	 * Insert onto list for new vnode.
	 */
	s = splbio();
	bufinsvn(bp, &vp->v_cleanblkhd);
	splx(s);
}

/*
 * Disassociate a buffer from a vnode.
 */
void
brelvp(bp)
	register struct buf *bp;
{
	struct vnode *vp;
	int s;

	if (bp->b_vp == (struct vnode *) 0)
		panic("brelvp: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	s = splbio();
	if (bp->b_vnbufs.le_next != NOLIST)
		bufremvn(bp);
	splx(s);

	vp = bp->b_vp;
	bp->b_vp = (struct vnode *) 0;
	HOLDRELE(vp);
}

/*
 * Associate a p-buffer with a vnode.
 */
void
pbgetvp(vp, bp)
	register struct vnode *vp;
	register struct buf *bp;
{
	if (bp->b_vp)
		panic("pbgetvp: not free");
	VHOLD(vp);
	bp->b_vp = vp;
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		bp->b_dev = vp->v_rdev;
	else
		bp->b_dev = NODEV;
}

/*
 * Disassociate a p-buffer from a vnode.
 */
void
pbrelvp(bp)
	register struct buf *bp;
{
	struct vnode *vp;

	if (bp->b_vp == (struct vnode *) 0)
		panic("brelvp: NULL");

	vp = bp->b_vp;
	bp->b_vp = (struct vnode *) 0;
	HOLDRELE(vp);
}

/*
 * Reassign a buffer from one vnode to another.
 * Used to assign file specific control information
 * (indirect blocks) to the vnode to which they belong.
 */
void
reassignbuf(bp, newvp)
	register struct buf *bp;
	register struct vnode *newvp;
{
	register struct buflists *listheadp;

	if (newvp == NULL) {
		printf("reassignbuf: NULL");
		return;
	}
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (bp->b_vnbufs.le_next != NOLIST)
		bufremvn(bp);
	/*
	 * If dirty, put on list of dirty buffers; otherwise insert onto list
	 * of clean buffers.
	 */
	if (bp->b_flags & B_DELWRI) {
		struct buf *tbp;

		tbp = newvp->v_dirtyblkhd.lh_first;
		if (!tbp || (tbp->b_lblkno > bp->b_lblkno)) {
			bufinsvn(bp, &newvp->v_dirtyblkhd);
		} else {
			while (tbp->b_vnbufs.le_next && (tbp->b_vnbufs.le_next->b_lblkno < bp->b_lblkno)) {
				tbp = tbp->b_vnbufs.le_next;
			}
			LIST_INSERT_AFTER(tbp, bp, b_vnbufs);
		}
	} else {
		listheadp = &newvp->v_cleanblkhd;
		bufinsvn(bp, listheadp);
	}
}

/*
 * Create a vnode for a block device.
 * Used for root filesystem, argdev, and swap areas.
 * Also used for memory file system special devices.
 */
int
bdevvp(dev, vpp)
	dev_t dev;
	struct vnode **vpp;
{
	register struct vnode *vp;
	struct vnode *nvp;
	int error;

	if (dev == NODEV)
		return (0);
	error = getnewvnode(VT_NON, (struct mount *) 0, spec_vnodeop_p, &nvp);
	if (error) {
		*vpp = 0;
		return (error);
	}
	vp = nvp;
	vp->v_type = VBLK;
	if ((nvp = checkalias(vp, dev, (struct mount *) 0))) {
		vput(vp);
		vp = nvp;
	}
	*vpp = vp;
	return (0);
}

/*
 * Check to see if the new vnode represents a special device
 * for which we already have a vnode (either because of
 * bdevvp() or because of a different vnode representing
 * the same block device). If such an alias exists, deallocate
 * the existing contents and return the aliased vnode. The
 * caller is responsible for filling it with its new contents.
 */
struct vnode *
checkalias(nvp, nvp_rdev, mp)
	register struct vnode *nvp;
	dev_t nvp_rdev;
	struct mount *mp;
{
	register struct vnode *vp;
	struct vnode **vpp;

	if (nvp->v_type != VBLK && nvp->v_type != VCHR)
		return (NULLVP);

	vpp = &speclisth[SPECHASH(nvp_rdev)];
loop:
	for (vp = *vpp; vp; vp = vp->v_specnext) {
		if (nvp_rdev != vp->v_rdev || nvp->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			goto loop;
		}
		if (vget(vp, 1))
			goto loop;
		break;
	}
	if (vp == NULL || vp->v_tag != VT_NON) {
		MALLOC(nvp->v_specinfo, struct specinfo *,
		    sizeof(struct specinfo), M_VNODE, M_WAITOK);
		nvp->v_rdev = nvp_rdev;
		nvp->v_hashchain = vpp;
		nvp->v_specnext = *vpp;
		nvp->v_specflags = 0;
		*vpp = nvp;
		if (vp != NULL) {
			nvp->v_flag |= VALIASED;
			vp->v_flag |= VALIASED;
			vput(vp);
		}
		return (NULLVP);
	}
	VOP_UNLOCK(vp);
	vclean(vp, 0);
	vp->v_op = nvp->v_op;
	vp->v_tag = nvp->v_tag;
	nvp->v_type = VNON;
	insmntque(vp, mp);
	return (vp);
}

/*
 * Grab a particular vnode from the free list, increment its
 * reference count and lock it. The vnode lock bit is set the
 * vnode is being eliminated in vgone. The process is awakened
 * when the transition is completed, and an error returned to
 * indicate that the vnode is no longer usable (possibly having
 * been changed to a new file system type).
 */
int
vget(vp, lockflag)
	register struct vnode *vp;
	int lockflag;
{

	/*
	 * If the vnode is in the process of being cleaned out for another
	 * use, we wait for the cleaning to finish and then return failure.
	 * Cleaning is determined either by checking that the VXLOCK flag is
	 * set, or that the use count is zero with the back pointer set to
	 * show that it has been removed from the free list by getnewvnode.
	 * The VXLOCK flag may not have been set yet because vclean is blocked
	 * in the VOP_LOCK call waiting for the VOP_INACTIVE to complete.
	 */
	if ((vp->v_flag & VXLOCK) ||
	    (vp->v_usecount == 0 &&
		vp->v_freelist.tqe_prev == (struct vnode **) 0xdeadb)) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t) vp, PINOD, "vget", 0);
		return (1);
	}
	if (vp->v_usecount == 0) {
		TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		freevnodes--;
	}
	vp->v_usecount++;
	if (lockflag)
		VOP_LOCK(vp);
	return (0);
}

/*
 * Vnode reference, just increment the count
 */
void
vref(vp)
	struct vnode *vp;
{

	if (vp->v_usecount <= 0)
		panic("vref used where vget required");
	vp->v_usecount++;
}

/*
 * vput(), just unlock and vrele()
 */
void
vput(vp)
	register struct vnode *vp;
{

	VOP_UNLOCK(vp);
	vrele(vp);
}

/*
 * Vnode release.
 * If count drops to zero, call inactive routine and return to freelist.
 */
void
vrele(vp)
	register struct vnode *vp;
{

#ifdef DIAGNOSTIC
	if (vp == NULL)
		panic("vrele: null vp");
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0)
		return;
#ifdef DIAGNOSTIC
	if (vp->v_usecount < 0 /* || vp->v_writecount < 0 */ ) {
		vprint("vrele: negative ref count", vp);
		panic("vrele: negative reference cnt");
	}
#endif
	if (vp->v_flag & VAGE) {
		TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
		vp->v_flag &= ~VAGE;
	} else {
		TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
	}
	freevnodes++;

	VOP_INACTIVE(vp);
}

#ifdef DIAGNOSTIC
/*
 * Page or buffer structure gets a reference.
 */
void
vhold(vp)
	register struct vnode *vp;
{

	vp->v_holdcnt++;
}

/*
 * Page or buffer structure frees a reference.
 */
void
holdrele(vp)
	register struct vnode *vp;
{

	if (vp->v_holdcnt <= 0)
		panic("holdrele: holdcnt");
	vp->v_holdcnt--;
}
#endif /* DIAGNOSTIC */

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If MNT_NOFORCE is specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If MNT_FORCE is specified, detach any active vnodes
 * that are found.
 */
#ifdef DIAGNOSTIC
static int busyprt = 0;		/* print out busy vnodes */
SYSCTL_INT(_debug, 1, busyprt, CTLFLAG_RW, &busyprt, 0, "");
#endif

int
vflush(mp, skipvp, flags)
	struct mount *mp;
	struct vnode *skipvp;
	int flags;
{
	register struct vnode *vp, *nvp;
	int busy = 0;

	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("vflush: not busy");
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp; vp = nvp) {
		/*
		 * Make sure this vnode wasn't reclaimed in getnewvnode().
		 * Start over if it has (it won't be on the list anymore).
		 */
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		/*
		 * Skip over a selected vnode.
		 */
		if (vp == skipvp)
			continue;
		/*
		 * Skip over a vnodes marked VSYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_flag & VSYSTEM))
			continue;
		/*
		 * If WRITECLOSE is set, only flush out regular file vnodes
		 * open for writing.
		 */
		if ((flags & WRITECLOSE) &&
		    (vp->v_writecount == 0 || vp->v_type != VREG))
			continue;
		/*
		 * With v_usecount == 0, all we need to do is clear out the
		 * vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * If FORCECLOSE is set, forcibly close the vnode. For block
		 * or character devices, revert to an anonymous device. For
		 * all other files, just kill them.
		 */
		if (flags & FORCECLOSE) {
			if (vp->v_type != VBLK && vp->v_type != VCHR) {
				vgone(vp);
			} else {
				vclean(vp, 0);
				vp->v_op = spec_vnodeop_p;
				insmntque(vp, (struct mount *) 0);
			}
			continue;
		}
#ifdef DIAGNOSTIC
		if (busyprt)
			vprint("vflush: busy vnode", vp);
#endif
		busy++;
	}
	if (busy)
		return (EBUSY);
	return (0);
}

/*
 * Disassociate the underlying file system from a vnode.
 */
void
vclean(struct vnode *vp, int flags)
{
	int active;

	/*
	 * Check to see if the vnode is in use. If so we have to reference it
	 * before we clean it out so that its count cannot fall to zero and
	 * generate a race against ourselves to recycle it.
	 */
	if ((active = vp->v_usecount))
		VREF(vp);
	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still have
	 * the object locked while it cleans it out. The VOP_LOCK ensures that
	 * the VOP_INACTIVE routine is done with its work. For active vnodes,
	 * it ensures that no other activity can occur while the underlying
	 * object is being cleaned out.
	 */
	VOP_LOCK(vp);
	/*
	 * Prevent the vnode from being recycled or brought into use while we
	 * clean it out.
	 */
	if (vp->v_flag & VXLOCK)
		panic("vclean: deadlock");
	vp->v_flag |= VXLOCK;
	/*
	 * Clean out any buffers associated with the vnode.
	 */
	if (flags & DOCLOSE)
		vinvalbuf(vp, V_SAVE, NOCRED, NULL, 0, 0);
	/*
	 * Any other processes trying to obtain this lock must first wait for
	 * VXLOCK to clear, then call the new lock operation.
	 */
	VOP_UNLOCK(vp);
	/*
	 * If purging an active vnode, it must be closed and deactivated
	 * before being reclaimed.
	 */
	if (active) {
		if (flags & DOCLOSE)
			VOP_CLOSE(vp, FNONBLOCK, NOCRED, NULL);
		VOP_INACTIVE(vp);
	}
	/*
	 * Reclaim the vnode.
	 */
	if (VOP_RECLAIM(vp))
		panic("vclean: cannot reclaim");
	if (active)
		vrele(vp);

	/*
	 * Done with purge, notify sleepers of the grim news.
	 */
	vp->v_op = dead_vnodeop_p;
	vp->v_tag = VT_NON;
	vp->v_flag &= ~VXLOCK;
	if (vp->v_flag & VXWANT) {
		vp->v_flag &= ~VXWANT;
		wakeup((caddr_t) vp);
	}
}

/*
 * Eliminate all activity associated with  the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
void
vgoneall(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;

	if (vp->v_flag & VALIASED) {
		/*
		 * If a vgone (or vclean) is already in progress, wait until
		 * it is done and return.
		 */
		if (vp->v_flag & VXLOCK) {
			vp->v_flag |= VXWANT;
			(void) tsleep((caddr_t) vp, PINOD, "vgall", 0);
			return;
		}
		/*
		 * Ensure that vp will not be vgone'd while we are eliminating
		 * its aliases.
		 */
		vp->v_flag |= VXLOCK;
		while (vp->v_flag & VALIASED) {
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type || vp == vq)
					continue;
				vgone(vq);
				break;
			}
		}
		/*
		 * Remove the lock so that vgone below will really eliminate
		 * the vnode after which time vgone will awaken any sleepers.
		 */
		vp->v_flag &= ~VXLOCK;
	}
	vgone(vp);
}

/*
 * Eliminate all activity associated with a vnode
 * in preparation for reuse.
 */
void
vgone(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;
	struct vnode *vx;

	/*
	 * If a vgone (or vclean) is already in progress, wait until it is
	 * done and return.
	 */
	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t) vp, PINOD, "vgone", 0);
		return;
	}
	/*
	 * Clean out the filesystem specific data.
	 */
	vclean(vp, DOCLOSE);
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mount != NULL) {
		LIST_REMOVE(vp, v_mntvnodes);
		vp->v_mount = NULL;
	}
	/*
	 * If special device, remove it from special device alias list.
	 */
	if (vp->v_type == VBLK || vp->v_type == VCHR) {
		if (*vp->v_hashchain == vp) {
			*vp->v_hashchain = vp->v_specnext;
		} else {
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_specnext != vp)
					continue;
				vq->v_specnext = vp->v_specnext;
				break;
			}
			if (vq == NULL)
				panic("missing bdev");
		}
		if (vp->v_flag & VALIASED) {
			vx = NULL;
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type)
					continue;
				if (vx)
					break;
				vx = vq;
			}
			if (vx == NULL)
				panic("missing alias");
			if (vq == NULL)
				vx->v_flag &= ~VALIASED;
			vp->v_flag &= ~VALIASED;
		}
		FREE(vp->v_specinfo, M_VNODE);
		vp->v_specinfo = NULL;
	}
	/*
	 * If it is on the freelist and not already at the head, move it to
	 * the head of the list. The test of the back pointer and the
	 * reference count of zero is because it will be removed from the free
	 * list by getnewvnode, but will not have its reference count
	 * incremented until after calling vgone. If the reference count were
	 * incremented first, vgone would (incorrectly) try to close the
	 * previous instance of the underlying object. So, the back pointer is
	 * explicitly set to `0xdeadb' in getnewvnode after removing it from
	 * the freelist to ensure that we do not try to move it here.
	 */
	if (vp->v_usecount == 0 &&
	    vp->v_freelist.tqe_prev != (struct vnode **) 0xdeadb &&
	    vnode_free_list.tqh_first != vp) {
		TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
	}
	vp->v_type = VBAD;
}

/*
 * Lookup a vnode by device number.
 */
int
vfinddev(dev, type, vpp)
	dev_t dev;
	enum vtype type;
	struct vnode **vpp;
{
	register struct vnode *vp;

	for (vp = speclisth[SPECHASH(dev)]; vp; vp = vp->v_specnext) {
		if (dev != vp->v_rdev || type != vp->v_type)
			continue;
		*vpp = vp;
		return (1);
	}
	return (0);
}

/*
 * Calculate the total number of references to a special device.
 */
int
vcount(vp)
	register struct vnode *vp;
{
	register struct vnode *vq, *vnext;
	int count;

loop:
	if ((vp->v_flag & VALIASED) == 0)
		return (vp->v_usecount);
	for (count = 0, vq = *vp->v_hashchain; vq; vq = vnext) {
		vnext = vq->v_specnext;
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vq->v_usecount == 0 && vq != vp) {
			vgone(vq);
			goto loop;
		}
		count += vq->v_usecount;
	}
	return (count);
}

/*
 * Print out a description of a vnode.
 */
static char *typename[] =
{"VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD"};

void
vprint(label, vp)
	char *label;
	register struct vnode *vp;
{
	char buf[64];

	if (label != NULL)
		printf("%s: ", label);
	printf("type %s, usecount %d, writecount %d, refcount %ld,",
	    typename[vp->v_type], vp->v_usecount, vp->v_writecount,
	    vp->v_holdcnt);
	buf[0] = '\0';
	if (vp->v_flag & VROOT)
		strcat(buf, "|VROOT");
	if (vp->v_flag & VTEXT)
		strcat(buf, "|VTEXT");
	if (vp->v_flag & VSYSTEM)
		strcat(buf, "|VSYSTEM");
	if (vp->v_flag & VXLOCK)
		strcat(buf, "|VXLOCK");
	if (vp->v_flag & VXWANT)
		strcat(buf, "|VXWANT");
	if (vp->v_flag & VBWAIT)
		strcat(buf, "|VBWAIT");
	if (vp->v_flag & VALIASED)
		strcat(buf, "|VALIASED");
	if (buf[0] != '\0')
		printf(" flags (%s)", &buf[1]);
	if (vp->v_data == NULL) {
		printf("\n");
	} else {
		printf("\n\t");
		VOP_PRINT(vp);
	}
}

#ifdef DDB
/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
void
printlockedvnodes(void)
{
	register struct mount *mp;
	register struct vnode *vp;

	printf("Locked vnodes\n");
	for (mp = mountlist.cqh_first; mp != (void *)&mountlist;
	    mp = mp->mnt_list.cqe_next) {
		for (vp = mp->mnt_vnodelist.lh_first;
		    vp != NULL;
		    vp = vp->v_mntvnodes.le_next)
			if (VOP_ISLOCKED(vp))
				vprint((char *) 0, vp);
	}
}
#endif

int kinfo_vdebug = 1;
int kinfo_vgetfailed;

#define KINFO_VNODESLOP	10
/*
 * Dump vnode list (via sysctl).
 * Copyout address of vnode followed by vnode.
 */
/* ARGSUSED */
static int
sysctl_vnode SYSCTL_HANDLER_ARGS
{
	register struct mount *mp, *nmp;
	struct vnode *vp;
	int error;

#define VPTRSZ	sizeof (struct vnode *)
#define VNODESZ	sizeof (struct vnode)

	req->lock = 0;
	if (!req->oldptr) /* Make an estimate */
		return (SYSCTL_OUT(req, 0,
			(numvnodes + KINFO_VNODESLOP) * (VPTRSZ + VNODESZ)));

	for (mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = nmp) {
		nmp = mp->mnt_list.cqe_next;
		if (vfs_busy(mp))
			continue;
again:
		for (vp = mp->mnt_vnodelist.lh_first;
		    vp != NULL;
		    vp = vp->v_mntvnodes.le_next) {
			/*
			 * Check that the vp is still associated with this
			 * filesystem.  RACE: could have been recycled onto
			 * the same filesystem.
			 */
			if (vp->v_mount != mp) {
				if (kinfo_vdebug)
					printf("kinfo: vp changed\n");
				goto again;
			}
			if ((error = SYSCTL_OUT(req, &vp, VPTRSZ)) ||
			    (error = SYSCTL_OUT(req, vp, VNODESZ))) {
				vfs_unbusy(mp);
				return (error);
			}
		}
		vfs_unbusy(mp);
	}

	return (0);
}

SYSCTL_PROC(_kern, KERN_VNODE, vnode, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, 0, sysctl_vnode, "S,vnode", "");

/*
 * Check to see if a filesystem is mounted on a block device.
 */
int
vfs_mountedon(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;

	if (vp->v_specflags & SI_MOUNTEDON)
		return (EBUSY);
	if (vp->v_flag & VALIASED) {
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type)
				continue;
			if (vq->v_specflags & SI_MOUNTEDON)
				return (EBUSY);
		}
	}
	return (0);
}

/*
 * Build hash lists of net addresses and hang them off the mount point.
 * Called by ufs_mount() to set up the lists of export addresses.
 */
static int
vfs_hang_addrlist(struct mount *mp, struct netexport *nep, 
	struct export_args *argp)
{
	register struct netcred *np;
	register struct radix_node_head *rnh;
	register int i;
	struct radix_node *rn;
	struct sockaddr *saddr, *smask = 0;
	struct domain *dom;
	int error;

	if (argp->ex_addrlen == 0) {
		if (mp->mnt_flag & MNT_DEFEXPORTED)
			return (EPERM);
		np = &nep->ne_defexported;
		np->netc_exflags = argp->ex_flags;
		np->netc_anon = argp->ex_anon;
		np->netc_anon.cr_ref = 1;
		mp->mnt_flag |= MNT_DEFEXPORTED;
		return (0);
	}
	i = sizeof(struct netcred) + argp->ex_addrlen + argp->ex_masklen;
	np = (struct netcred *) malloc(i, M_NETADDR, M_WAITOK);
	bzero((caddr_t) np, i);
	saddr = (struct sockaddr *) (np + 1);
	if ((error = copyin(argp->ex_addr, (caddr_t) saddr, argp->ex_addrlen)))
		goto out;
	if (saddr->sa_len > argp->ex_addrlen)
		saddr->sa_len = argp->ex_addrlen;
	if (argp->ex_masklen) {
		smask = (struct sockaddr *) ((caddr_t) saddr + argp->ex_addrlen);
		error = copyin(argp->ex_addr, (caddr_t) smask, argp->ex_masklen);
		if (error)
			goto out;
		if (smask->sa_len > argp->ex_masklen)
			smask->sa_len = argp->ex_masklen;
	}
	i = saddr->sa_family;
	if ((rnh = nep->ne_rtable[i]) == 0) {
		/*
		 * Seems silly to initialize every AF when most are not used,
		 * do so on demand here
		 */
		for (dom = domains; dom; dom = dom->dom_next)
			if (dom->dom_family == i && dom->dom_rtattach) {
				dom->dom_rtattach((void **) &nep->ne_rtable[i],
				    dom->dom_rtoffset);
				break;
			}
		if ((rnh = nep->ne_rtable[i]) == 0) {
			error = ENOBUFS;
			goto out;
		}
	}
	rn = (*rnh->rnh_addaddr) ((caddr_t) saddr, (caddr_t) smask, rnh,
	    np->netc_rnodes);
	if (rn == 0 || np != (struct netcred *) rn) {	/* already exists */
		error = EPERM;
		goto out;
	}
	np->netc_exflags = argp->ex_flags;
	np->netc_anon = argp->ex_anon;
	np->netc_anon.cr_ref = 1;
	return (0);
out:
	free(np, M_NETADDR);
	return (error);
}

/* ARGSUSED */
static int
vfs_free_netcred(struct radix_node *rn, void *w)
{
	register struct radix_node_head *rnh = (struct radix_node_head *) w;

	(*rnh->rnh_deladdr) (rn->rn_key, rn->rn_mask, rnh);
	free((caddr_t) rn, M_NETADDR);
	return (0);
}

/*
 * Free the net address hash lists that are hanging off the mount points.
 */
static void
vfs_free_addrlist(struct netexport *nep)
{
	register int i;
	register struct radix_node_head *rnh;

	for (i = 0; i <= AF_MAX; i++)
		if ((rnh = nep->ne_rtable[i])) {
			(*rnh->rnh_walktree) (rnh, vfs_free_netcred,
			    (caddr_t) rnh);
			free((caddr_t) rnh, M_RTABLE);
			nep->ne_rtable[i] = 0;
		}
}

int
vfs_export(mp, nep, argp)
	struct mount *mp;
	struct netexport *nep;
	struct export_args *argp;
{
	int error;

	if (argp->ex_flags & MNT_DELEXPORT) {
		vfs_free_addrlist(nep);
		mp->mnt_flag &= ~(MNT_EXPORTED | MNT_DEFEXPORTED);
	}
	if (argp->ex_flags & MNT_EXPORTED) {
		if ((error = vfs_hang_addrlist(mp, nep, argp)))
			return (error);
		mp->mnt_flag |= MNT_EXPORTED;
	}
	return (0);
}

struct netcred *
vfs_export_lookup(mp, nep, nam)
	register struct mount *mp;
	struct netexport *nep;
	struct mbuf *nam;
{
	register struct netcred *np;
	register struct radix_node_head *rnh;
	struct sockaddr *saddr;

	np = NULL;
	if (mp->mnt_flag & MNT_EXPORTED) {
		/*
		 * Lookup in the export list first.
		 */
		if (nam != NULL) {
			saddr = mtod(nam, struct sockaddr *);
			rnh = nep->ne_rtable[saddr->sa_family];
			if (rnh != NULL) {
				np = (struct netcred *)
				    (*rnh->rnh_matchaddr) ((caddr_t) saddr,
				    rnh);
				if (np && np->netc_rnodes->rn_flags & RNF_ROOT)
					np = NULL;
			}
		}
		/*
		 * If no address match, use the default if it exists.
		 */
		if (np == NULL && mp->mnt_flag & MNT_DEFEXPORTED)
			np = &nep->ne_defexported;
	}
	return (np);
}


/*
 * perform msync on all vnodes under a mount point
 * the mount point must be locked.
 */
void
vfs_msync(struct mount *mp, int flags) {
	struct vnode *vp, *nvp;
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {

		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		if (VOP_ISLOCKED(vp) && (flags != MNT_WAIT))
			continue;
		if (vp->v_object &&
		   (((vm_object_t) vp->v_object)->flags & OBJ_MIGHTBEDIRTY)) {
			vm_object_page_clean(vp->v_object, 0, 0, TRUE, TRUE);
		}
	}
}
