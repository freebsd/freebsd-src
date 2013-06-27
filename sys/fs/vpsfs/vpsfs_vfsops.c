/*-
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)vpsfs_vfsops.c	8.2 (Berkeley) 1/21/94
 *
 * @(#)lofs_vfsops.c	1.2 (Berkeley) 6/18/92
 * $FreeBSD: head/sys/fs/vpsfs/vpsfs_vfsops.c 250505 2013-05-11 11:17:44Z kib $
 */

#ifdef VPS

/*
 * Null Layer
 * (See vpsfs_vnops.c for a description of what this does.)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/jail.h>

#include <fs/vpsfs/vpsfs.h>

static MALLOC_DEFINE(M_VPSFSMNT, "vpsfs_mount", "VPSFS mount structure");

static vfs_fhtovp_t	vpsfs_fhtovp;
static vfs_mount_t	vpsfs_mount;
static vfs_quotactl_t	vpsfs_quotactl;
static vfs_root_t	vpsfs_root;
static vfs_sync_t	vpsfs_sync;
static vfs_statfs_t	vpsfs_statfs;
static vfs_unmount_t	vpsfs_unmount;
static vfs_vget_t	vpsfs_vget;
static vfs_extattrctl_t	vpsfs_extattrctl;

/*
 * Mount vpsfs layer
 */
static int
vpsfs_mount(struct mount *mp)
{
	int error = 0;
	struct vnode *lowerrootvp, *vp;
	struct vnode *vpsfsm_rootvp;
	struct vpsfs_mount *xmp;
	/*
	struct thread *td = curthread;
	*/
	char *target;
	int isvnunlocked = 0, len;
	struct nameidata nd, *ndp = &nd;

	VPSFSDEBUG("vpsfs_mount(mp = %p)\n", (void *)mp);

	/*
	if (!prison_allow(td->td_ucred, PR_ALLOW_MOUNT_VPSFS))
		return (EPERM);
	*/
	if (mp->mnt_flag & MNT_ROOTFS)
		return (EOPNOTSUPP);

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/*
		 * Only support update mounts for NFS export.
		 */
		if (vfs_flagopt(mp->mnt_optnew, "export", NULL, 0))
			return (0);
		else
			return (EOPNOTSUPP);
	}

	/*
	 * Get argument
	 */
	error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target,
	    &len);
	if (error || target[len - 1] != '\0')
		return (EINVAL);

	/*
	 * Unlock lower node to avoid possible deadlock.
	 */
	if ((mp->mnt_vnodecovered->v_op == &vpsfs_vnodeops) &&
	    VOP_ISLOCKED(mp->mnt_vnodecovered) == LK_EXCLUSIVE) {
		VOP_UNLOCK(mp->mnt_vnodecovered, 0);
		isvnunlocked = 1;
	}
	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|LOCKLEAF, UIO_SYSSPACE, target,
	    curthread);
	error = namei(ndp);

	/*
	 * Re-lock vnode.
	 * XXXKIB This is deadlock-prone as well.
	 */
	if (isvnunlocked)
		vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY);

	if (error)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;

	/*
	 * Check multi vpsfs mount to avoid `lock against myself' panic.
	 */
	if (lowerrootvp == VTOVPSFS(mp->mnt_vnodecovered)->vpsfs_lowervp) {
		VPSFSDEBUG("vpsfs_mount: multi vpsfs mount?\n");
		vput(lowerrootvp);
		return (EDEADLK);
	}

	xmp = (struct vpsfs_mount *) malloc(sizeof(struct vpsfs_mount),
	    M_VPSFSMNT, M_WAITOK | M_ZERO);

	/*
	 * Save reference to underlying FS
	 */
	xmp->vpsfsm_vfs = lowerrootvp->v_mount;

	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = vpsfs_nodeget(mp, lowerrootvp, &vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		free(xmp, M_VPSFSMNT);
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in vpsfs_unmount.
	 */
	vpsfsm_rootvp = vp;
	vpsfsm_rootvp->v_vflag |= VV_ROOT;
	xmp->vpsfsm_rootvp = vpsfsm_rootvp;

	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0);

	if (VPSFSVPTOLOWERVP(vpsfsm_rootvp)->v_mount->mnt_flag &
	    MNT_LOCAL) {
		MNT_ILOCK(mp);
		mp->mnt_flag |= MNT_LOCAL;
		MNT_IUNLOCK(mp);
	}

	xmp->vpsfsm_flags |= VPSFSM_CACHE;
	if (vfs_getopt(mp->mnt_optnew, "nocache", NULL, NULL) == 0)
		xmp->vpsfsm_flags &= ~VPSFSM_CACHE;

	MNT_ILOCK(mp);
	if ((xmp->vpsfsm_flags & VPSFSM_CACHE) != 0) {
		mp->mnt_kern_flag |= lowerrootvp->v_mount->mnt_kern_flag &
		    (MNTK_SHARED_WRITES | MNTK_LOOKUP_SHARED |
		    MNTK_EXTENDED_SHARED);
	}
	mp->mnt_kern_flag |= MNTK_LOOKUP_EXCL_DOTDOT;
	MNT_IUNLOCK(mp);
	mp->mnt_data = xmp;
	vfs_getnewfsid(mp);
	if ((xmp->vpsfsm_flags & VPSFSM_CACHE) != 0) {
		MNT_ILOCK(xmp->vpsfsm_vfs);
		TAILQ_INSERT_TAIL(&xmp->vpsfsm_vfs->mnt_uppers, mp,
		    mnt_upper_link);
		MNT_IUNLOCK(xmp->vpsfsm_vfs);
	}

	vfs_mountedfrom(mp, target);

	mtx_init(&MOUNTTOVPSFSMOUNT(mp)->vpsfs_mtx, "vpsfs mtx", NULL,
	    MTX_DEF | MTX_DUPOK);
	MOUNTTOVPSFSMOUNT(mp)->limits_last_sync = 0;
	MOUNTTOVPSFSMOUNT(mp)->limits_sync_task_enqueued = 0;
	MOUNTTOVPSFSMOUNT(mp)->vpsfsm_limits =
	    malloc(sizeof(struct vpsfs_limits),
	    M_VPSFSMNT, M_WAITOK | M_ZERO);
	vpsfs_read_usage(MOUNTTOVPSFSMOUNT(mp),
	    MOUNTTOVPSFSMOUNT(mp)->vpsfsm_limits);

	VPSFSDEBUG("vpsfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
	return (0);
}

/*
 * Free reference to vpsfs layer
 */
static int
vpsfs_unmount(mp, mntflags)
	struct mount *mp;
	int mntflags;
{
	struct vpsfs_mount *mntdata;
	struct mount *ump;
	int error, flags;

	VPSFSDEBUG("vpsfs_unmount: mp = %p\n", (void *)mp);

	if (mntflags & MNT_FORCE)
		flags = FORCECLOSE;
	else
		flags = 0;

	vpsfs_write_usage(MOUNTTOVPSFSMOUNT(mp),
	    MOUNTTOVPSFSMOUNT(mp)->vpsfsm_limits);

	mtx_destroy(&MOUNTTOVPSFSMOUNT(mp)->vpsfs_mtx);

	/* There is 1 extra root vnode reference (vpsfsm_rootvp). */
	error = vflush(mp, 1, flags, curthread);
	if (error)
		return (error);

	/*
	 * Finally, throw away the vpsfs_mount structure
	 */
	mntdata = mp->mnt_data;
	ump = mntdata->vpsfsm_vfs;
	if ((mntdata->vpsfsm_flags & VPSFSM_CACHE) != 0) {
		MNT_ILOCK(ump);
		while ((ump->mnt_kern_flag & MNTK_VGONE_UPPER) != 0) {
			ump->mnt_kern_flag |= MNTK_VGONE_WAITER;
			msleep(&ump->mnt_uppers, &ump->mnt_mtx, 0,
			    "vgnupw", 0);
		}
		TAILQ_REMOVE(&ump->mnt_uppers, mp, mnt_upper_link);
		MNT_IUNLOCK(ump);
	}
	mp->mnt_data = NULL;
	free(mntdata, M_VPSFSMNT);
	return (0);
}

static int
vpsfs_root(mp, flags, vpp)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
{
	struct vnode *vp;

	VPSFSDEBUG("vpsfs_root(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp,
	    (void *)VPSFSVPTOLOWERVP(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp));

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp;
	VREF(vp);

	ASSERT_VOP_UNLOCKED(vp, "root vnode is locked");
	vn_lock(vp, flags | LK_RETRY);
	*vpp = vp;
	return 0;
}

static int
vpsfs_quotactl(mp, cmd, uid, arg)
	struct mount *mp;
	int cmd;
	uid_t uid;
	void *arg;
{

	/*
	return VFS_QUOTACTL(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, cmd,
	    uid, arg);
	*/

	VPSFSDEBUG("vpsfs_quotactl()\n");

	return (ENOTSUP);
}

static int
vpsfs_statfs(mp, sbp)
	struct mount *mp;
	struct statfs *sbp;
{
#if 0
	int error;
	struct statfs mstat;

	VPSFSDEBUG("vpsfs_statfs(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp,
	    (void *)VPSFSVPTOLOWERVP(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp));

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, &mstat);
	if (error)
		return (error);

	/* now copy across the "interesting" information and
	   fake the rest */
	sbp->f_type = mstat.f_type;
	sbp->f_flags = (sbp->f_flags & (MNT_RDONLY | MNT_NOEXEC |
	    MNT_NOSUID | MNT_UNION | MNT_NOSYMFOLLOW)) |
	    (mstat.f_flags & ~MNT_ROOTFS);
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
	sbp->f_blocks = mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files = mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;
	return (0);
#endif /* 0 */

#if 1
	int error;
	struct statfs mstat;
	struct vpsfs_mount *nmp;
	struct vpsfs_limits *lp;

	nmp = MOUNTTOVPSFSMOUNT(mp);
	lp = nmp->vpsfsm_limits;

	VPSFSDEBUG("vpsfs_statfs(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp,
	    (void *)VPSFSVPTOLOWERVP(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_rootvp));

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, &mstat);
	if (error)
		return (error);

	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
	sbp->f_flags = mstat.f_flags;
	sbp->f_type = mstat.f_type;

	if (lp->space_soft == 0) {
		sbp->f_blocks = mstat.f_bavail +
		    (lp->space_used / mstat.f_bsize);
		sbp->f_bavail = mstat.f_bavail;
		sbp->f_bfree = mstat.f_bavail;
	} else {
		sbp->f_blocks = lp->space_soft / mstat.f_bsize;
		sbp->f_bavail = (lp->space_soft - lp->space_used) /
		    mstat.f_bsize;
		sbp->f_bfree = (lp->space_soft - lp->space_used) /
		    mstat.f_bsize;
	}

	if (lp->nodes_soft == 0) {
		sbp->f_files = mstat.f_ffree + lp->nodes_used;
		sbp->f_ffree = mstat.f_ffree;
	} else {
		sbp->f_files = lp->nodes_soft;
		sbp->f_ffree = lp->nodes_soft - lp->nodes_used;
	}

	VPSFSDEBUG("lp: space_soft=%zu space_used=%zu\n",
	    lp->space_soft, lp->space_used);
	VPSFSDEBUG("sbp: f_blocks=%016llu f_bavail=%016llu "
	    "f_bfree=%016llu\n",
	    (long long unsigned int)sbp->f_blocks,
	    (long long unsigned int)sbp->f_bavail,
	    (long long unsigned int)sbp->f_bfree);

	/*
	vpsfs_calcusage(MOUNTTOVPSFSMOUNT(mp));
	*/

	VPSFSDEBUG("sbp: f_flags=%016llx f_type=%08x\n",
		(long long unsigned int)sbp->f_flags, sbp->f_type);

	return (0);
#endif /* 1 */
}

static int
vpsfs_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	/*
	 * XXX - Assumes no data cached at vpsfs layer.
	 */
	return (0);
}

static int
vpsfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	int error;

	KASSERT((flags & LK_TYPE_MASK) != 0,
	    ("vpsfs_vget: no lock requested"));

	error = VFS_VGET(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, ino,
	    flags, vpp);
	if (error != 0)
		return (error);
	return (vpsfs_nodeget(mp, *vpp, vpp));
}

static int
vpsfs_fhtovp(mp, fidp, flags, vpp)
	struct mount *mp;
	struct fid *fidp;
	int flags;
	struct vnode **vpp;
{
	int error;

	error = VFS_FHTOVP(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, fidp, flags,
	    vpp);
	if (error != 0)
		return (error);
	return (vpsfs_nodeget(mp, *vpp, vpp));
}

static int			   
vpsfs_extattrctl(mp, cmd, filename_vp, namespace, attrname)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int namespace;
	const char *attrname;
{

	return (VFS_EXTATTRCTL(MOUNTTOVPSFSMOUNT(mp)->vpsfsm_vfs, cmd,
	    filename_vp, namespace, attrname));
}

static void
vpsfs_reclaim_lowervp(struct mount *mp, struct vnode *lowervp)
{
	struct vnode *vp;

	vp = vpsfs_hashget(mp, lowervp);
	if (vp == NULL)
		return;
	VTOVPSFS(vp)->vpsfs_flags |= VPSFSV_NOUNLOCK;
	vgone(vp);
	vput(vp);
}

static void
vpsfs_unlink_lowervp(struct mount *mp, struct vnode *lowervp)
{
	struct vnode *vp;
	struct vpsfs_node *xp;

	vp = vpsfs_hashget(mp, lowervp);
	if (vp == NULL)
		return;
	xp = VTOVPSFS(vp);
	xp->vpsfs_flags |= VPSFSV_DROP | VPSFSV_NOUNLOCK;
	vhold(vp);
	vunref(vp);

	/*
	 * If vunref() dropped the last use reference on the vpsfs
	 * vnode, it must be reclaimed, and its lock was split from
	 * the lower vnode lock.  Need to do extra unlock before
	 * allowing the final vdrop() to free the vnode.
	 */
	if (vp->v_usecount == 0) {
		KASSERT((vp->v_iflag & VI_DOOMED) != 0,
		    ("not reclaimed %p", vp));
		VOP_UNLOCK(vp, 0);
	}
	vdrop(vp);
}

static struct vfsops vpsfs_vfsops = {
	.vfs_extattrctl =	vpsfs_extattrctl,
	.vfs_fhtovp =		vpsfs_fhtovp,
	.vfs_init =		vpsfs_init,
	.vfs_mount =		vpsfs_mount,
	.vfs_quotactl =		vpsfs_quotactl,
	.vfs_root =		vpsfs_root,
	.vfs_statfs =		vpsfs_statfs,
	.vfs_sync =		vpsfs_sync,
	.vfs_uninit =		vpsfs_uninit,
	.vfs_unmount =		vpsfs_unmount,
	.vfs_vget =		vpsfs_vget,
	.vfs_reclaim_lowervp =	vpsfs_reclaim_lowervp,
	.vfs_unlink_lowervp =	vpsfs_unlink_lowervp,
};

int
vpsfs_mount_is_vpsfs(struct mount *mp)
{

	if (mp->mnt_op == &vpsfs_vfsops)
		return (1);
	else
		return (0);
}

VFS_SET(vpsfs_vfsops, vpsfs, VFCF_LOOPBACK | VFCF_JAIL);

#endif /* VPS */
