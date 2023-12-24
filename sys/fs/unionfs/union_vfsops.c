/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1994, 1995 The Regents of the University of California.
 * Copyright (c) 1994, 1995 Jan-Simon Pendry.
 * Copyright (c) 2005, 2006, 2012 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006, 2012 Daichi Goto <daichi@freebsd.org>
 * All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)union_vfsops.c	8.20 (Berkeley) 5/20/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/stat.h>

#include <fs/unionfs/union.h>

static MALLOC_DEFINE(M_UNIONFSMNT, "UNIONFS mount", "UNIONFS mount structure");

static vfs_fhtovp_t	unionfs_fhtovp;
static vfs_checkexp_t	unionfs_checkexp;
static vfs_mount_t	unionfs_domount;
static vfs_quotactl_t	unionfs_quotactl;
static vfs_root_t	unionfs_root;
static vfs_sync_t	unionfs_sync;
static vfs_statfs_t	unionfs_statfs;
static vfs_unmount_t	unionfs_unmount;
static vfs_vget_t	unionfs_vget;
static vfs_extattrctl_t	unionfs_extattrctl;

static struct vfsops unionfs_vfsops;

/*
 * Mount unionfs layer.
 */
static int
unionfs_domount(struct mount *mp)
{
	struct mount   *lowermp, *uppermp;
	struct vnode   *lowerrootvp;
	struct vnode   *upperrootvp;
	struct unionfs_mount *ump;
	char           *target;
	char           *tmp;
	char           *ep;
	struct nameidata nd, *ndp;
	struct vattr	va;
	unionfs_copymode copymode;
	unionfs_whitemode whitemode;
	int		below;
	int		error;
	int		len;
	uid_t		uid;
	gid_t		gid;
	u_short		udir;
	u_short		ufile;

	UNIONFSDEBUG("unionfs_mount(mp = %p)\n", mp);

	error = 0;
	below = 0;
	uid = 0;
	gid = 0;
	udir = 0;
	ufile = 0;
	copymode = UNIONFS_TRANSPARENT;	/* default */
	whitemode = UNIONFS_WHITE_ALWAYS;
	ndp = &nd;

	if (mp->mnt_flag & MNT_ROOTFS) {
		vfs_mount_error(mp, "Cannot union mount root filesystem");
		return (EOPNOTSUPP);
	}

	/*
	 * Update is a no operation.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		vfs_mount_error(mp, "unionfs does not support mount update");
		return (EOPNOTSUPP);
	}

	/*
	 * Get argument
	 */
	error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target, &len);
	if (error)
		error = vfs_getopt(mp->mnt_optnew, "from", (void **)&target,
		    &len);
	if (error || target[len - 1] != '\0') {
		vfs_mount_error(mp, "Invalid target");
		return (EINVAL);
	}
	if (vfs_getopt(mp->mnt_optnew, "below", NULL, NULL) == 0)
		below = 1;
	if (vfs_getopt(mp->mnt_optnew, "udir", (void **)&tmp, NULL) == 0) {
		if (tmp != NULL)
			udir = (mode_t)strtol(tmp, &ep, 8);
		if (tmp == NULL || *ep) {
			vfs_mount_error(mp, "Invalid udir");
			return (EINVAL);
		}
		udir &= S_IRWXU | S_IRWXG | S_IRWXO;
	}
	if (vfs_getopt(mp->mnt_optnew, "ufile", (void **)&tmp, NULL) == 0) {
		if (tmp != NULL)
			ufile = (mode_t)strtol(tmp, &ep, 8);
		if (tmp == NULL || *ep) {
			vfs_mount_error(mp, "Invalid ufile");
			return (EINVAL);
		}
		ufile &= S_IRWXU | S_IRWXG | S_IRWXO;
	}
	/* check umask, uid and gid */
	if (udir == 0 && ufile != 0)
		udir = ufile;
	if (ufile == 0 && udir != 0)
		ufile = udir;

	vn_lock(mp->mnt_vnodecovered, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, mp->mnt_cred);
	if (!error) {
		if (udir == 0)
			udir = va.va_mode;
		if (ufile == 0)
			ufile = va.va_mode;
		uid = va.va_uid;
		gid = va.va_gid;
	}
	VOP_UNLOCK(mp->mnt_vnodecovered);
	if (error)
		return (error);

	if (mp->mnt_cred->cr_ruid == 0) {	/* root only */
		if (vfs_getopt(mp->mnt_optnew, "uid", (void **)&tmp,
		    NULL) == 0) {
			if (tmp != NULL)
				uid = (uid_t)strtol(tmp, &ep, 10);
			if (tmp == NULL || *ep) {
				vfs_mount_error(mp, "Invalid uid");
				return (EINVAL);
			}
		}
		if (vfs_getopt(mp->mnt_optnew, "gid", (void **)&tmp,
		    NULL) == 0) {
			if (tmp != NULL)
				gid = (gid_t)strtol(tmp, &ep, 10);
			if (tmp == NULL || *ep) {
				vfs_mount_error(mp, "Invalid gid");
				return (EINVAL);
			}
		}
		if (vfs_getopt(mp->mnt_optnew, "copymode", (void **)&tmp,
		    NULL) == 0) {
			if (tmp == NULL) {
				vfs_mount_error(mp, "Invalid copymode");
				return (EINVAL);
			} else if (strcasecmp(tmp, "traditional") == 0)
				copymode = UNIONFS_TRADITIONAL;
			else if (strcasecmp(tmp, "transparent") == 0)
				copymode = UNIONFS_TRANSPARENT;
			else if (strcasecmp(tmp, "masquerade") == 0)
				copymode = UNIONFS_MASQUERADE;
			else {
				vfs_mount_error(mp, "Invalid copymode");
				return (EINVAL);
			}
		}
		if (vfs_getopt(mp->mnt_optnew, "whiteout", (void **)&tmp,
		    NULL) == 0) {
			if (tmp == NULL) {
				vfs_mount_error(mp, "Invalid whiteout mode");
				return (EINVAL);
			} else if (strcasecmp(tmp, "always") == 0)
				whitemode = UNIONFS_WHITE_ALWAYS;
			else if (strcasecmp(tmp, "whenneeded") == 0)
				whitemode = UNIONFS_WHITE_WHENNEEDED;
			else {
				vfs_mount_error(mp, "Invalid whiteout mode");
				return (EINVAL);
			}
		}
	}
	/* If copymode is UNIONFS_TRADITIONAL, uid/gid is mounted user. */
	if (copymode == UNIONFS_TRADITIONAL) {
		uid = mp->mnt_cred->cr_ruid;
		gid = mp->mnt_cred->cr_rgid;
	}

	UNIONFSDEBUG("unionfs_mount: uid=%d, gid=%d\n", uid, gid);
	UNIONFSDEBUG("unionfs_mount: udir=0%03o, ufile=0%03o\n", udir, ufile);
	UNIONFSDEBUG("unionfs_mount: copymode=%d\n", copymode);

	/*
	 * Find upper node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, target);
	if ((error = namei(ndp)))
		return (error);

	NDFREE_PNBUF(ndp);

	/* get root vnodes */
	lowerrootvp = mp->mnt_vnodecovered;
	upperrootvp = ndp->ni_vp;
	KASSERT(lowerrootvp != NULL, ("%s: NULL lower root vp", __func__));
	KASSERT(upperrootvp != NULL, ("%s: NULL upper root vp", __func__));

	/* create unionfs_mount */
	ump = malloc(sizeof(struct unionfs_mount), M_UNIONFSMNT,
	    M_WAITOK | M_ZERO);

	/*
	 * Save reference
	 */
	if (below) {
		VOP_UNLOCK(upperrootvp);
		vn_lock(lowerrootvp, LK_EXCLUSIVE | LK_RETRY);
		ump->um_lowervp = upperrootvp;
		ump->um_uppervp = lowerrootvp;
	} else {
		ump->um_lowervp = lowerrootvp;
		ump->um_uppervp = upperrootvp;
	}
	ump->um_rootvp = NULLVP;
	ump->um_uid = uid;
	ump->um_gid = gid;
	ump->um_udir = udir;
	ump->um_ufile = ufile;
	ump->um_copymode = copymode;
	ump->um_whitemode = whitemode;

	mp->mnt_data = ump;

	/*
	 * Copy upper layer's RDONLY flag.
	 */
	mp->mnt_flag |= ump->um_uppervp->v_mount->mnt_flag & MNT_RDONLY;

	/*
	 * Unlock the node
	 */
	VOP_UNLOCK(ump->um_uppervp);

	/*
	 * Get the unionfs root vnode.
	 */
	error = unionfs_nodeget(mp, ump->um_uppervp, ump->um_lowervp,
	    NULLVP, &(ump->um_rootvp), NULL);
	if (error != 0) {
		vrele(upperrootvp);
		free(ump, M_UNIONFSMNT);
		mp->mnt_data = NULL;
		return (error);
	}
	KASSERT(ump->um_rootvp != NULL, ("rootvp cannot be NULL"));
	KASSERT((ump->um_rootvp->v_vflag & VV_ROOT) != 0,
	    ("%s: rootvp without VV_ROOT", __func__));

	/*
	 * Do not release the namei() reference on upperrootvp until after
	 * we attempt to register the upper mounts.  A concurrent unmount
	 * of the upper or lower FS may have caused unionfs_nodeget() to
	 * create a unionfs node with a NULL upper or lower vp and with
	 * no reference held on upperrootvp or lowerrootvp.
	 * vfs_register_upper() should subsequently fail, which is what
	 * we want, but we must ensure neither underlying vnode can be
	 * reused until that happens.  We assume the caller holds a reference
	 * to lowerrootvp as it is the mount's covered vnode.
	 */
	lowermp = vfs_register_upper_from_vp(ump->um_lowervp, mp,
	    &ump->um_lower_link);
	uppermp = vfs_register_upper_from_vp(ump->um_uppervp, mp,
	    &ump->um_upper_link);

	vrele(upperrootvp);

	if (lowermp == NULL || uppermp == NULL) {
		if (lowermp != NULL)
			vfs_unregister_upper(lowermp, &ump->um_lower_link);
		if (uppermp != NULL)
			vfs_unregister_upper(uppermp, &ump->um_upper_link);
		vflush(mp, 1, FORCECLOSE, curthread);
		free(ump, M_UNIONFSMNT);
		mp->mnt_data = NULL;
		return (ENOENT);
	}

	/*
	 * Specify that the covered vnode lock should remain held while
	 * lookup() performs the cross-mount walk.  This prevents a lock-order
	 * reversal between the covered vnode lock (which is also locked by
	 * unionfs_lock()) and the mountpoint's busy count.  Without this,
	 * unmount will lock the covered vnode lock (directly through the
	 * covered vnode) and wait for the busy count to drain, while a
	 * concurrent lookup will increment the busy count and then lock
	 * the covered vnode lock (indirectly through unionfs_lock()).
	 *
	 * Note that we can't yet use this facility for the 'below' case
	 * in which the upper vnode is the covered vnode, because that would
	 * introduce a different LOR in which the cross-mount lookup would
	 * effectively hold the upper vnode lock before acquiring the lower
	 * vnode lock, while an unrelated lock operation would still acquire
	 * the lower vnode lock before the upper vnode lock, which is the
	 * order unionfs currently requires.
	 */
	if (!below) {
		vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE);
		mp->mnt_vnodecovered->v_vflag |= VV_CROSSLOCK;
		VOP_UNLOCK(mp->mnt_vnodecovered);
	}

	MNT_ILOCK(mp);
	if ((lowermp->mnt_flag & MNT_LOCAL) != 0 &&
	    (uppermp->mnt_flag & MNT_LOCAL) != 0)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_NOMSYNC | MNTK_UNIONFS |
	    (ump->um_uppermp->mnt_kern_flag & MNTK_SHARED_WRITES);
	MNT_IUNLOCK(mp);

	/*
	 * Get new fsid
	 */
	vfs_getnewfsid(mp);

	snprintf(mp->mnt_stat.f_mntfromname, MNAMELEN, "<%s>:%s",
	    below ? "below" : "above", target);

	UNIONFSDEBUG("unionfs_mount: from %s, on %s\n",
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);

	return (0);
}

/*
 * Free reference to unionfs layer
 */
static int
unionfs_unmount(struct mount *mp, int mntflags)
{
	struct unionfs_mount *ump;
	int		error;
	int		num;
	int		freeing;
	int		flags;

	UNIONFSDEBUG("unionfs_unmount: mp = %p\n", mp);

	ump = MOUNTTOUNIONFSMOUNT(mp);
	flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* vflush (no need to call vrele) */
	for (freeing = 0; (error = vflush(mp, 1, flags, curthread)) != 0;) {
		num = mp->mnt_nvnodelistsize;
		if (num == freeing)
			break;
		freeing = num;
	}

	if (error)
		return (error);

	vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE);
	mp->mnt_vnodecovered->v_vflag &= ~VV_CROSSLOCK;
	VOP_UNLOCK(mp->mnt_vnodecovered);
	vfs_unregister_upper(ump->um_lowervp->v_mount, &ump->um_lower_link);
	vfs_unregister_upper(ump->um_uppervp->v_mount, &ump->um_upper_link);
	free(ump, M_UNIONFSMNT);
	mp->mnt_data = NULL;

	return (0);
}

static int
unionfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct unionfs_mount *ump;
	struct vnode *vp;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	vp = ump->um_rootvp;

	UNIONFSDEBUG("unionfs_root: rootvp=%p locked=%x\n",
	    vp, VOP_ISLOCKED(vp));

	vref(vp);
	if (flags & LK_TYPE_MASK)
		vn_lock(vp, flags);

	*vpp = vp;

	return (0);
}

static int
unionfs_quotactl(struct mount *mp, int cmd, uid_t uid, void *arg,
    bool *mp_busy)
{
	struct mount *uppermp;
	struct unionfs_mount *ump;
	int error;
	bool unbusy;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	uppermp = atomic_load_ptr(&ump->um_uppervp->v_mount);
	KASSERT(*mp_busy == true, ("upper mount not busy"));
	/*
	 * See comment in sys_quotactl() for an explanation of why the
	 * lower mount needs to be busied by the caller of VFS_QUOTACTL()
	 * but may be unbusied by the implementation.  We must unbusy
	 * the upper mount for the same reason; otherwise a namei lookup
	 * issued by the VFS_QUOTACTL() implementation could traverse the
	 * upper mount and deadlock.
	 */
	vfs_unbusy(mp);
	*mp_busy = false;
	unbusy = true;
	error = vfs_busy(uppermp, 0);
	/*
	 * Writing is always performed to upper vnode.
	 */
	if (error == 0)
		error = VFS_QUOTACTL(uppermp, cmd, uid, arg, &unbusy);
	if (unbusy)
		vfs_unbusy(uppermp);

	return (error);
}

static int
unionfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct unionfs_mount *ump;
	struct statfs	*mstat;
	uint64_t	lbsize;
	int		error;

	ump = MOUNTTOUNIONFSMOUNT(mp);

	UNIONFSDEBUG("unionfs_statfs(mp = %p, lvp = %p, uvp = %p)\n",
	    mp, ump->um_lowervp, ump->um_uppervp);

	mstat = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK | M_ZERO);

	error = VFS_STATFS(ump->um_lowervp->v_mount, mstat);
	if (error) {
		free(mstat, M_STATFS);
		return (error);
	}

	/* now copy across the "interesting" information and fake the rest */
	sbp->f_blocks = mstat->f_blocks;
	sbp->f_files = mstat->f_files;

	lbsize = mstat->f_bsize;

	error = VFS_STATFS(ump->um_uppervp->v_mount, mstat);
	if (error) {
		free(mstat, M_STATFS);
		return (error);
	}

	/*
	 * The FS type etc is copy from upper vfs.
	 * (write able vfs have priority)
	 */
	sbp->f_type = mstat->f_type;
	sbp->f_flags = mstat->f_flags;
	sbp->f_bsize = mstat->f_bsize;
	sbp->f_iosize = mstat->f_iosize;

	if (mstat->f_bsize != lbsize)
		sbp->f_blocks = ((off_t)sbp->f_blocks * lbsize) /
		    mstat->f_bsize;

	sbp->f_blocks += mstat->f_blocks;
	sbp->f_bfree = mstat->f_bfree;
	sbp->f_bavail = mstat->f_bavail;
	sbp->f_files += mstat->f_files;
	sbp->f_ffree = mstat->f_ffree;

	free(mstat, M_STATFS);
	return (0);
}

static int
unionfs_sync(struct mount *mp, int waitfor)
{
	/* nothing to do */
	return (0);
}

static int
unionfs_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

static int
unionfs_fhtovp(struct mount *mp, struct fid *fidp, int flags,
    struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

static int
unionfs_checkexp(struct mount *mp, struct sockaddr *nam, uint64_t *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int *secflavors)
{
	return (EOPNOTSUPP);
}

static int
unionfs_extattrctl(struct mount *mp, int cmd, struct vnode *filename_vp,
    int namespace, const char *attrname)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	unp = VTOUNIONFS(filename_vp);

	if (unp->un_uppervp != NULLVP) {
		return (VFS_EXTATTRCTL(ump->um_uppervp->v_mount, cmd,
		    unp->un_uppervp, namespace, attrname));
	} else {
		return (VFS_EXTATTRCTL(ump->um_lowervp->v_mount, cmd,
		    unp->un_lowervp, namespace, attrname));
	}
}

static struct vfsops unionfs_vfsops = {
	.vfs_checkexp =		unionfs_checkexp,
	.vfs_extattrctl =	unionfs_extattrctl,
	.vfs_fhtovp =		unionfs_fhtovp,
	.vfs_init =		unionfs_init,
	.vfs_mount =		unionfs_domount,
	.vfs_quotactl =		unionfs_quotactl,
	.vfs_root =		unionfs_root,
	.vfs_statfs =		unionfs_statfs,
	.vfs_sync =		unionfs_sync,
	.vfs_uninit =		unionfs_uninit,
	.vfs_unmount =		unionfs_unmount,
	.vfs_vget =		unionfs_vget,
};

VFS_SET(unionfs_vfsops, unionfs, VFCF_LOOPBACK);
