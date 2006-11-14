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
 *	@(#)null_vfsops.c	8.2 (Berkeley) 1/21/94
 *
 * @(#)lofs_vfsops.c	1.2 (Berkeley) 6/18/92
 * $FreeBSD$
 */

/*
 * Null Layer
 * (See null_vnops.c for a description of what this does.)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <fs/nullfs/null.h>

static MALLOC_DEFINE(M_NULLFSMNT, "NULLFS mount", "NULLFS mount structure");

static vfs_fhtovp_t	nullfs_fhtovp;
static vfs_mount_t	nullfs_mount;
static vfs_quotactl_t	nullfs_quotactl;
static vfs_root_t	nullfs_root;
static vfs_sync_t	nullfs_sync;
static vfs_statfs_t	nullfs_statfs;
static vfs_unmount_t	nullfs_unmount;
static vfs_vget_t	nullfs_vget;
static vfs_vptofh_t	nullfs_vptofh;
static vfs_extattrctl_t	nullfs_extattrctl;

/*
 * Mount null layer
 */
static int
nullfs_mount(struct mount *mp, struct thread *td)
{
	int error = 0;
	struct vnode *lowerrootvp, *vp;
	struct vnode *nullm_rootvp;
	struct null_mount *xmp;
	char *target;
	int isvnunlocked = 0, len;
	struct nameidata nd, *ndp = &nd;

	NULLFSDEBUG("nullfs_mount(mp = %p)\n", (void *)mp);

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
	error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target, &len);
	if (error || target[len - 1] != '\0')
		return (EINVAL);

	/*
	 * Unlock lower node to avoid deadlock.
	 * (XXX) VOP_ISLOCKED is needed?
	 */
	if ((mp->mnt_vnodecovered->v_op == &null_vnodeops) &&
		VOP_ISLOCKED(mp->mnt_vnodecovered, NULL)) {
		VOP_UNLOCK(mp->mnt_vnodecovered, 0, td);
		isvnunlocked = 1;
	}
	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|LOCKLEAF,
		UIO_SYSSPACE, target, td);
	error = namei(ndp);
	/*
	 * Re-lock vnode.
	 */
	if (isvnunlocked && !VOP_ISLOCKED(mp->mnt_vnodecovered, NULL))
		vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY, td);

	if (error)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;

	/*
	 * Check multi null mount to avoid `lock against myself' panic.
	 */
	if (lowerrootvp == VTONULL(mp->mnt_vnodecovered)->null_lowervp) {
		NULLFSDEBUG("nullfs_mount: multi null mount?\n");
		vput(lowerrootvp);
		return (EDEADLK);
	}

	xmp = (struct null_mount *) malloc(sizeof(struct null_mount),
				M_NULLFSMNT, M_WAITOK);	/* XXX */

	/*
	 * Save reference to underlying FS
	 */
	xmp->nullm_vfs = lowerrootvp->v_mount;

	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = null_nodeget(mp, lowerrootvp, &vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		VOP_UNLOCK(vp, 0, td);
		vrele(lowerrootvp);
		free(xmp, M_NULLFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in nullfs_unmount.
	 */
	nullm_rootvp = vp;
	nullm_rootvp->v_vflag |= VV_ROOT;
	xmp->nullm_rootvp = nullm_rootvp;

	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0, td);

	if (NULLVPTOLOWERVP(nullm_rootvp)->v_mount->mnt_flag & MNT_LOCAL) {
		MNT_ILOCK(mp);
		mp->mnt_flag |= MNT_LOCAL;
		MNT_IUNLOCK(mp);
	}
	MNT_ILOCK(mp);
	mp->mnt_kern_flag |= lowerrootvp->v_mount->mnt_kern_flag & MNTK_MPSAFE;
	MNT_IUNLOCK(mp);
	mp->mnt_data = (qaddr_t) xmp;
	vfs_getnewfsid(mp);

	vfs_mountedfrom(mp, target);

	NULLFSDEBUG("nullfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
	return (0);
}

/*
 * Free reference to null layer
 */
static int
nullfs_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	void *mntdata;
	int error;
	int flags = 0;

	NULLFSDEBUG("nullfs_unmount: mp = %p\n", (void *)mp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* There is 1 extra root vnode reference (nullm_rootvp). */
	error = vflush(mp, 1, flags, td);
	if (error)
		return (error);

	/*
	 * Finally, throw away the null_mount structure
	 */
	mntdata = mp->mnt_data;
	mp->mnt_data = 0;
	free(mntdata, M_NULLFSMNT);
	return 0;
}

static int
nullfs_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	struct vnode *vp;

	NULLFSDEBUG("nullfs_root(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTONULLMOUNT(mp)->nullm_rootvp,
	    (void *)NULLVPTOLOWERVP(MOUNTTONULLMOUNT(mp)->nullm_rootvp));

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTONULLMOUNT(mp)->nullm_rootvp;
	VREF(vp);

#ifdef NULLFS_DEBUG
	if (VOP_ISLOCKED(vp, NULL)) {
		kdb_enter("root vnode is locked.\n");
		vrele(vp);
		return (EDEADLK);
	}
#endif
	vn_lock(vp, flags | LK_RETRY, td);
	*vpp = vp;
	return 0;
}

static int
nullfs_quotactl(mp, cmd, uid, arg, td)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{
	return VFS_QUOTACTL(MOUNTTONULLMOUNT(mp)->nullm_vfs, cmd, uid, arg, td);
}

static int
nullfs_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	int error;
	struct statfs mstat;

	NULLFSDEBUG("nullfs_statfs(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTONULLMOUNT(mp)->nullm_rootvp,
	    (void *)NULLVPTOLOWERVP(MOUNTTONULLMOUNT(mp)->nullm_rootvp));

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTONULLMOUNT(mp)->nullm_vfs, &mstat, td);
	if (error)
		return (error);

	/* now copy across the "interesting" information and fake the rest */
	sbp->f_type = mstat.f_type;
	sbp->f_flags = mstat.f_flags;
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
	sbp->f_blocks = mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files = mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;
	return (0);
}

static int
nullfs_sync(mp, waitfor, td)
	struct mount *mp;
	int waitfor;
	struct thread *td;
{
	/*
	 * XXX - Assumes no data cached at null layer.
	 */
	return (0);
}

static int
nullfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	int error;
	error = VFS_VGET(MOUNTTONULLMOUNT(mp)->nullm_vfs, ino, flags, vpp);
	if (error)
		return (error);

	return (null_nodeget(mp, *vpp, vpp));
}

static int
nullfs_fhtovp(mp, fidp, vpp)
	struct mount *mp;
	struct fid *fidp;
	struct vnode **vpp;
{
	int error;
	error = VFS_FHTOVP(MOUNTTONULLMOUNT(mp)->nullm_vfs, fidp, vpp);
	if (error)
		return (error);

	return (null_nodeget(mp, *vpp, vpp));
}

static int
nullfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct vnode *lvp;

	lvp = NULLVPTOLOWERVP(vp);
	return VFS_VPTOFH(lvp, fhp);
}

static int                        
nullfs_extattrctl(mp, cmd, filename_vp, namespace, attrname, td)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int namespace;
	const char *attrname;
	struct thread *td;            
{
	return VFS_EXTATTRCTL(MOUNTTONULLMOUNT(mp)->nullm_vfs, cmd, filename_vp,
	    namespace, attrname, td);
}


static struct vfsops null_vfsops = {
	.vfs_extattrctl =	nullfs_extattrctl,
	.vfs_fhtovp =		nullfs_fhtovp,
	.vfs_init =		nullfs_init,
	.vfs_mount =		nullfs_mount,
	.vfs_quotactl =		nullfs_quotactl,
	.vfs_root =		nullfs_root,
	.vfs_statfs =		nullfs_statfs,
	.vfs_sync =		nullfs_sync,
	.vfs_uninit =		nullfs_uninit,
	.vfs_unmount =		nullfs_unmount,
	.vfs_vget =		nullfs_vget,
	.vfs_vptofh =		nullfs_vptofh,
};

VFS_SET(null_vfsops, nullfs, VFCF_LOOPBACK);
