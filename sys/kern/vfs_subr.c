/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vfs_subr.c	7.60 (Berkeley) 6/21/91
 *	$Id: vfs_subr.c,v 1.13 1994/06/14 03:41:00 davidg Exp $
 */

/*
 * External virtual filesystem routines
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "mount.h"
#include "time.h"
#include "vnode.h"
#include "specdev.h"
#include "namei.h"
#include "ucred.h"
#include "buf.h"
#include "errno.h"
#include "malloc.h"
#include "vm/vm.h"
#include "vm/vm_object.h"
#include "vm/vm_pager.h"

static void insmntque(struct vnode *, struct mount *);

struct vnode *speclisth[SPECHSZ]; /* device special file vnode hash table */

/*
 * Remove a mount point from the list of mounted filesystems.
 * Unmount of the root is illegal.
 */
void
vfs_remove(mp)
	register struct mount *mp;
{

	if (mp == rootfs)
		panic("vfs_remove: unmounting root");
	mp->mnt_prev->mnt_next = mp->mnt_next;
	mp->mnt_next->mnt_prev = mp->mnt_prev;
	mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
	vfs_unlock(mp);
}

/*
 * Lock a filesystem.
 * Used to prevent access to it while mounting and unmounting.
 */
int
vfs_lock(mp)
	register struct mount *mp;
{

	while(mp->mnt_flag & MNT_MLOCK) {
		mp->mnt_flag |= MNT_MWAIT;
		tsleep((caddr_t)mp, PVFS, "vfslock", 0);
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
		wakeup((caddr_t)mp);
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

	while(mp->mnt_flag & MNT_MPBUSY) {
		mp->mnt_flag |= MNT_MPWANT;
		tsleep((caddr_t)&mp->mnt_flag, PVFS, "vfsbusy", 0);
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
		wakeup((caddr_t)&mp->mnt_flag);
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

	mp = rootfs;
	do {
		if (mp->mnt_stat.f_fsid.val[0] == fsid->val[0] &&
		    mp->mnt_stat.f_fsid.val[1] == fsid->val[1]) {
			return (mp);
		}
		mp = mp->mnt_next;
	} while (mp != rootfs);
	return ((struct mount *)0);
}

/*
 * Set vnode attributes to VNOVAL
 */
void vattr_null(vap)
	register struct vattr *vap;
{

	vap->va_type = VNON;
	vap->va_mode = vap->va_nlink = vap->va_uid = vap->va_gid =
		vap->va_fsid = vap->va_fileid = vap->va_size =
		vap->va_size_rsv = vap->va_blocksize = vap->va_rdev =
		vap->va_bytes = vap->va_bytes_rsv =
		vap->va_atime.tv_sec = vap->va_atime.tv_usec =
		vap->va_mtime.tv_sec = vap->va_mtime.tv_usec =
		vap->va_ctime.tv_sec = vap->va_ctime.tv_usec =
		vap->va_flags = vap->va_gen = VNOVAL;
}

/*
 * Routines having to do with the management of the vnode table.
 */
struct vnode *vfreeh, **vfreet;
extern struct vnodeops dead_vnodeops, spec_vnodeops;
long numvnodes;
struct vattr va_null;

/*
 * Initialize the vnode structures and initialize each file system type.
 */
void
vfsinit()
{
	struct vfsops **vfsp;

	/*
	 * Initialize the vnode name cache
	 */
	nchinit();
	/*
	 * Initialize each file system type.
	 */
	vattr_null(&va_null);
	for (vfsp = &vfssw[0]; vfsp <= &vfssw[MOUNT_MAXTYPE]; vfsp++) {
		if (*vfsp == NULL)
			continue;
		(*(*vfsp)->vfs_init)();
	}
}

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(tag, mp, vops, vpp)
	enum vtagtype tag;
	struct mount *mp;
	struct vnodeops *vops;
	struct vnode **vpp;
{
	register struct vnode *vp, *vq;

	if (numvnodes < desiredvnodes) {
		vp = (struct vnode *)malloc((u_long)sizeof *vp,
		    M_VNODE, M_WAITOK);
		bzero((char *)vp, sizeof *vp);
		numvnodes++;
	} else {
		if ((vp = vfreeh) == NULL) {
			tablefull("vnode");
			*vpp = 0;
			return (ENFILE);
		}
		if (vp->v_usecount)
			panic("free vnode isn't");
		if (vq = vp->v_freef)
			vq->v_freeb = &vfreeh;
		else
			vfreet = &vfreeh;
		vfreeh = vq;
		vp->v_freef = NULL;
		vp->v_freeb = NULL;
		if (vp->v_type != VBAD)
			vgone(vp);
		vp->v_flag = 0;
		vp->v_lastr = 0;
		vp->v_socket = 0;
	}
	vp->v_type = VNON;
	cache_purge(vp);
	vp->v_tag = tag;
	vp->v_op = vops;
	insmntque(vp, mp);
	VREF(vp);
	*vpp = vp;
	return (0);
}

/*
 * Move a vnode from one mount queue to another.
 */
static void
insmntque(vp, mp)
	register struct vnode *vp;
	register struct mount *mp;
{
	register struct vnode *vq;

	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mountb) {
		if (vq = vp->v_mountf)
			vq->v_mountb = vp->v_mountb;
		*vp->v_mountb = vq;
	}
	/*
	 * Insert into list of vnodes for the new mount point, if available.
	 */
	vp->v_mount = mp;
	if (mp == NULL) {
		vp->v_mountf = NULL;
		vp->v_mountb = NULL;
		return;
	}
	if (vq = mp->mnt_mounth)
		vq->v_mountb = &vp->v_mountf;
	vp->v_mountf = vq;
	vp->v_mountb = &mp->mnt_mounth;
	mp->mnt_mounth = vp;
}

/*
 * Make sure all write-behind blocks associated
 * with mount point are flushed out (from sync).
 */
void
mntflushbuf(mountp, flags)
	struct mount *mountp;
	int flags;
{
	register struct vnode *vp;

	if ((mountp->mnt_flag & MNT_MPBUSY) == 0)
		panic("mntflushbuf: not busy");
loop:
	for (vp = mountp->mnt_mounth; vp; vp = vp->v_mountf) {
		if (VOP_ISLOCKED(vp))
			continue;
		if (vget(vp))
			goto loop;
		vflushbuf(vp, flags);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
}

/*
 * Flush all dirty buffers associated with a vnode.
 */
void
vflushbuf(vp, flags)
	register struct vnode *vp;
	int flags;
{
	register struct buf *bp;
	struct buf *nbp;
	int s;

loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd; bp; bp = nbp) {
		nbp = bp->b_blockf;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("vflushbuf: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 * NB: This is really specific to ufs, but is done here
		 * as it is easier and quicker.
		 */
		if (bp->b_vp == vp || (flags & B_SYNC) == 0)
			(void) bawrite(bp);
		else
			(void) bwrite(bp);
		goto loop;
	}
	splx(s);
	if ((flags & B_SYNC) == 0)
		return;
	s = splbio();
	while (vp->v_numoutput) {
		vp->v_flag |= VBWAIT;
		tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "vflushbf", 0);
	}
	splx(s);
	if (vp->v_dirtyblkhd) {
		vprint("vflushbuf: dirty", vp);
		goto loop;
	}
}

/*
 * Update outstanding I/O count and do wakeup if requested.
 */
void
vwakeup(bp)
	register struct buf *bp;
{
	register struct vnode *vp;

	bp->b_dirtyoff = bp->b_dirtyend = 0;
	if (vp = bp->b_vp) {
		vp->v_numoutput--;
		if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
			if (vp->v_numoutput < 0)
				panic("vwakeup: neg numoutput");
			vp->v_flag &= ~VBWAIT;
			wakeup((caddr_t)&vp->v_numoutput);
		}
	}
}

/*
 * Invalidate in core blocks belonging to closed or umounted filesystem
 *
 * Go through the list of vnodes associated with the file system;
 * for each vnode invalidate any buffers that it holds. Normally
 * this routine is preceeded by a bflush call, so that on a quiescent
 * filesystem there will be no dirty buffers when we are done. Binval
 * returns the count of dirty buffers when it is finished.
 */
int
mntinvalbuf(mountp)
	struct mount *mountp;
{
	register struct vnode *vp;
	int dirty = 0;

	if ((mountp->mnt_flag & MNT_MPBUSY) == 0)
		panic("mntinvalbuf: not busy");
loop:
	for (vp = mountp->mnt_mounth; vp; vp = vp->v_mountf) {
		if (vget(vp))
			goto loop;
		dirty += vinvalbuf(vp, 1);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
	return (dirty);
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(vp, save)
	register struct vnode *vp;
	int save;
{
	register struct buf *bp;
	struct buf *nbp, *blist;
	int s, dirty = 0;
	vm_pager_t pager;
	vm_object_t object;

	for (;;) {
		if (blist = vp->v_dirtyblkhd)
			/* void */;
		else if (blist = vp->v_cleanblkhd)
			/* void */;
		else
			break;
		for (bp = blist; bp; bp = nbp) {
			nbp = bp->b_blockf;
			s = splbio();
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				tsleep((caddr_t)bp, PRIBIO + 1, "vinvalbf", 0);
				splx(s);
				break;
			}
			bremfree(bp);
			bp->b_flags |= B_BUSY;
			splx(s);
			if (save && (bp->b_flags & B_DELWRI)) {
				dirty++;
				(void) bwrite(bp);
				break;
			}
			if (bp->b_vp != vp)
				reassignbuf(bp, bp->b_vp);
			else
				bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}

	pager = (vm_pager_t)vp->v_vmdata;
	if (pager != NULL) {
		object = vm_object_lookup(pager);
		if (object) {
			vm_object_lock(object);
			if (save)
				vm_object_page_clean(object, 0, 0);
			vm_object_page_remove(object, 0, object->size);
			vm_object_unlock(object);
			vm_object_deallocate(object);
		}
	}

	if (vp->v_dirtyblkhd || vp->v_cleanblkhd)
		panic("vinvalbuf: flush failed");
	return (dirty);
}

/*
 * Associate a buffer with a vnode.
 */
void
bgetvp(vp, bp)
	register struct vnode *vp;
	register struct buf *bp;
{
	register struct vnode *vq;
	register struct buf *bq;

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
	if (bq = vp->v_cleanblkhd)
		bq->b_blockb = &bp->b_blockf;
	bp->b_blockf = bq;
	bp->b_blockb = &vp->v_cleanblkhd;
	vp->v_cleanblkhd = bp;
}

/*
 * Disassociate a buffer from a vnode.
 */
void
brelvp(bp)
	register struct buf *bp;
{
	struct buf *bq;
	struct vnode *vp;

	if (bp->b_vp == (struct vnode *) 0)
		panic("brelvp: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (bp->b_blockb) {
		if (bq = bp->b_blockf)
			bq->b_blockb = bp->b_blockb;
		*bp->b_blockb = bq;
		bp->b_blockf = NULL;
		bp->b_blockb = NULL;
	}
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
	register struct buf *bq, **listheadp;

	if (newvp == NULL)
		panic("reassignbuf: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (bp->b_blockb) {
		if (bq = bp->b_blockf)
			bq->b_blockb = bp->b_blockb;
		*bp->b_blockb = bq;
	}
	/*
	 * If dirty, put on list of dirty buffers;
	 * otherwise insert onto list of clean buffers.
	 */
	if (bp->b_flags & B_DELWRI)
		listheadp = &newvp->v_dirtyblkhd;
	else
		listheadp = &newvp->v_cleanblkhd;
	if (bq = *listheadp)
		bq->b_blockb = &bp->b_blockf;
	bp->b_blockf = bq;
	bp->b_blockb = listheadp;
	*listheadp = bp;
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
	error = getnewvnode(VT_NON, (struct mount *)0, &spec_vnodeops, &nvp);
	if (error) {
		*vpp = 0;
		return (error);
	}
	vp = nvp;
	vp->v_type = VBLK;
	if (nvp = checkalias(vp, dev, (struct mount *)0)) {
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
		if (vget(vp))
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
		nvp->v_opencount = 0;
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
vget(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;

	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep((caddr_t)vp, PINOD, "vget", 0);
		return (1);
	}
	if (vp->v_usecount == 0) {
		if (vq = vp->v_freef)
			vq->v_freeb = vp->v_freeb;
		else
			vfreet = vp->v_freeb;
		*vp->v_freeb = vq;
		vp->v_freef = NULL;
		vp->v_freeb = NULL;
	}
	VREF(vp);
	VOP_LOCK(vp);
	return (0);
}

/*
 * Vnode reference, just increment the count
 */
void vref(vp)
	struct vnode *vp;
{

	vp->v_usecount++;
}

/*
 * vput(), just unlock and vrele()
 */
void vput(vp)
	register struct vnode *vp;
{
	VOP_UNLOCK(vp);
	vrele(vp);
}

/*
 * Vnode release.
 * If count drops to zero, call inactive routine and return to freelist.
 */
void vrele(vp)
	register struct vnode *vp;
{
	struct proc *p = curproc;		/* XXX */

#ifdef DIAGNOSTIC
	if (vp == NULL)
		panic("vrele: null vp");
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0)
		return;
#ifdef DIAGNOSTIC
	if (vp->v_usecount != 0 || vp->v_writecount != 0) {
		vprint("vrele: bad ref count", vp);
		panic("vrele: ref cnt");
	}
#endif
	vp->v_writecount = 0;	/* XXX */
	if (vfreeh == NULLVP) {
		/*
		 * insert into empty list
		 */
		vfreeh = vp;
		vp->v_freeb = &vfreeh;
	} else {
		/*
		 * insert at tail of list
		 */
		*vfreet = vp;
		vp->v_freeb = vfreet;
	}
	vp->v_freef = NULL;
	vfreet = &vp->v_freef;
	VOP_INACTIVE(vp, p);
}

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

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If MNT_NOFORCE is specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If MNT_FORCE is specified, detach any active vnodes
 * that are found.
 */
int busyprt = 0;	/* patch to print out busy vnodes */

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
	for (vp = mp->mnt_mounth; vp; vp = nvp) {
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mountf;
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
		 * With v_usecount == 0, all we need to do is clear
		 * out the vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * For block or character devices, revert to an
		 * anonymous device. For all other files, just kill them.
		 */
		if (flags & FORCECLOSE) {
			if (vp->v_type != VBLK && vp->v_type != VCHR) {
				vgone(vp);
			} else {
				vclean(vp, 0);
				vp->v_op = &spec_vnodeops;
				insmntque(vp, (struct mount *)0);
			}
			continue;
		}
		if (busyprt)
			vprint("vflush: busy vnode", vp);
		busy++;
	}
	if (busy)
		return (EBUSY);
	return (0);
}

/*
 * Disassociate the underlying file system from a vnode.
 */
void vclean(vp, flags)
	register struct vnode *vp;
	int flags;
{
	struct vnodeops *origops;
	int active;
	struct proc *p = curproc;	/* XXX */

	/*
	 * Check to see if the vnode is in use.
	 * If so we have to reference it before we clean it out
	 * so that its count cannot fall to zero and generate a
	 * race against ourselves to recycle it.
	 */
	if (active = vp->v_usecount)
		VREF(vp);
	/*
	 * Prevent the vnode from being recycled or
	 * brought into use while we clean it out.
	 */
	if (vp->v_flag & VXLOCK)
		panic("vclean: deadlock");
	vp->v_flag |= VXLOCK;
	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still
	 * have the object locked while it cleans it out. The VOP_LOCK
	 * ensures that the VOP_INACTIVE routine is done with its work.
	 * For active vnodes, it ensures that no other activity can
	 * occur while the buffer list is being cleaned out.
	 */
	VOP_LOCK(vp);
	if (flags & DOCLOSE)
		vinvalbuf(vp, 1);
	/*
	 * Prevent any further operations on the vnode from
	 * being passed through to the old file system.
	 */
	origops = vp->v_op;
	vp->v_op = &dead_vnodeops;
	vp->v_tag = VT_NON;
	/*
	 * If purging an active vnode, it must be unlocked, closed,
	 * and deactivated before being reclaimed.
	 */
	(*(origops->vop_unlock))(vp);
	if (active) {
		if (flags & DOCLOSE)
			(*(origops->vop_close))(vp, IO_NDELAY, NOCRED, p);
		(*(origops->vop_inactive))(vp, p);
	}
	/*
	 * Reclaim the vnode.
	 */
	if ((*(origops->vop_reclaim))(vp))
		panic("vclean: cannot reclaim");
	if (active)
		vrele(vp);
	/*
	 * Done with purge, notify sleepers in vget of the grim news.
	 */
	vp->v_flag &= ~VXLOCK;
	if (vp->v_flag & VXWANT) {
		vp->v_flag &= ~VXWANT;
		wakeup((caddr_t)vp);
	}
}

/*
 * Eliminate all activity associated with  the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
void vgoneall(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;

	if (vp->v_flag & VALIASED) {
		/*
		 * If a vgone (or vclean) is already in progress,
		 * wait until it is done and return.
		 */
		if (vp->v_flag & VXLOCK) {
			vp->v_flag |= VXWANT;
			tsleep((caddr_t)vp, PINOD, "vgoneall", 0);
			return;
		}
		/*
		 * Ensure that vp will not be vgone'd while we
		 * are eliminating its aliases.
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
		 * Remove the lock so that vgone below will
		 * really eliminate the vnode after which time
		 * vgone will awaken any sleepers.
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
	struct vnode *vx = 0;
	long count;

	/*
	 * If a vgone (or vclean) is already in progress,
	 * wait until it is done and return.
	 */
	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep((caddr_t)vp, PINOD, "vgone", 0);
		return;
	}
	/*
	 * Clean out the filesystem specific data.
	 */
	vclean(vp, DOCLOSE);
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mountb) {
		if (vq = vp->v_mountf)
			vq->v_mountb = vp->v_mountb;
		*vp->v_mountb = vq;
		vp->v_mountf = NULL;
		vp->v_mountb = NULL;
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
			count = 0;
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type)
					continue;
				count++;
				vx = vq;
			}
			if (count == 0)
				panic("missing alias");
			if (count == 1)
				vx->v_flag &= ~VALIASED;
			vp->v_flag &= ~VALIASED;
		}
		FREE(vp->v_specinfo, M_VNODE);
		vp->v_specinfo = NULL;
	}
	/*
	 * If it is on the freelist, move it to the head of the list.
	 */
	if (vp->v_freeb) {
		if (vq = vp->v_freef)
			vq->v_freeb = vp->v_freeb;
		else
			vfreet = vp->v_freeb;
		*vp->v_freeb = vq;
		vp->v_freef = vfreeh;
		vp->v_freeb = &vfreeh;
		vfreeh->v_freeb = &vp->v_freef;
		vfreeh = vp;
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
		return (0);
	}
	return (1);
}

/*
 * Calculate the total number of references to a special device.
 * Not counting sleeping openers.
 */
int
vcount(vp)
	register struct vnode *vp;
{
	register struct vnode *vq;
	int count;

	if ((vp->v_flag & VALIASED) == 0)
		return (vp->v_usecount);
loop:
	for (count = 0, vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vq->v_usecount == 0) {
			vgone(vq);
			goto loop;
		}
		count += vq->v_usecount - vq->v_opencount;
	}
	return (count);
}

/*
 * Print out a description of a vnode.
 */
static char *typename[] =
   { "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD" };

void
vprint(label, vp)
	const char *label;
	register struct vnode *vp;
{
	char buf[64];

	if (label != NULL)
		printf("%s: ", label);
	printf("type %s, usecount %d, writecount %d, refcount %d,",
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
	printf("\n\t");
	VOP_PRINT(vp);
}

#ifdef DEBUG
/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
printlockedvnodes()
{
	register struct mount *mp;
	register struct vnode *vp;

	printf("Locked vnodes\n");
	mp = rootfs;
	do {
		for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf)
			if (VOP_ISLOCKED(vp))
				vprint((char *)0, vp);
		mp = mp->mnt_next;
	} while (mp != rootfs);
}
#endif

int kinfo_vdebug = 1;
int kinfo_vgetfailed;
#define KINFO_VNODESLOP	10
/*
 * Dump vnode list (via kinfo).
 * Copyout address of vnode followed by vnode.
 */
/* ARGSUSED */
int
kinfo_vnode(op, where, acopysize, arg, aneeded)
	int op;
	char *where;
	int *acopysize, arg, *aneeded;
{
	register struct mount *mp = rootfs;
	struct mount *omp;
	struct vnode *vp;
	register char *bp = where, *savebp;
	char *ewhere = where + *acopysize;
	int error;

#define VPTRSZ	sizeof (struct vnode *)
#define VNODESZ	sizeof (struct vnode)
	if (where == NULL) {
		*aneeded = (numvnodes + KINFO_VNODESLOP) * (VPTRSZ + VNODESZ);
		return (0);
	}
		
	do {
		if (vfs_busy(mp)) {
			mp = mp->mnt_next;
			continue;
		}
		savebp = bp;
again:
		for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf) {
			/*
			 * Check that the vp is still associated with
			 * this filesystem.  RACE: could have been
			 * recycled onto the same filesystem.
			 */
			if (vp->v_mount != mp) {
				if (kinfo_vdebug)
					printf("kinfo: vp changed\n");
				bp = savebp;
				goto again;
			}
			if ((bp + VPTRSZ + VNODESZ <= ewhere) && 
			    ((error = copyout((caddr_t)&vp, bp, VPTRSZ)) ||
			     (error = copyout((caddr_t)vp, bp + VPTRSZ, 
			      VNODESZ))))
				return (error);
			bp += VPTRSZ + VNODESZ;
		}
		omp = mp;
		mp = mp->mnt_next;
		vfs_unbusy(omp);
	} while (mp != rootfs);

	*aneeded = bp - where;
	if (bp > ewhere)
		*acopysize = ewhere - where;
	else
		*acopysize = bp - where;
	return (0);
}
