/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include "lomacfs.h"
#include "kernel_mediate.h"

MALLOC_DEFINE(M_LOMACFS, "LOMACFS", "LOMAC filesystem objects");

static int lomacfs_mount(struct mount *mp, char *path, caddr_t data,
    struct nameidata *ndp, struct thread *td);
static int lomacfs_statfs(struct mount *mp, struct statfs *sbp,
    struct thread *td);
static int lomacfs_unmount(struct mount *mp, int mntflags, struct thread *td);

static int
lomacfs_mount(struct mount *mp, char *path, caddr_t data,
    struct nameidata *ndp, struct thread *td) {

	if (mp->mnt_flag & MNT_UPDATE || VISLOMAC(mp->mnt_vnodecovered))
		return (EOPNOTSUPP);

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_flag &= ~MNT_RDONLY;
	mp->mnt_data = malloc(sizeof(struct lomac_mount), M_LOMACFS,
	    M_WAITOK | M_ZERO);
	vfs_getnewfsid(mp);

	strncpy(mp->mnt_stat.f_mntfromname, "lomacfs", MNAMELEN);
	(void)lomacfs_statfs(mp, &mp->mnt_stat, td);

	/*
	 * Keep around an extra ref for dounmount() to vrele() after the
	 * VFS_UNMOUNT()... (who knows...)
	 */
	vref(mp->mnt_vnodecovered);
	if (VOP_ISLOCKED(mp->mnt_vnodecovered, NULL))
		VOP_UNLOCK(mp->mnt_vnodecovered, 0, td);

	return (0);
}

static int
lomacfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td) {
	struct statfs tmpstat;
	int error;

	bzero(&tmpstat, sizeof(tmpstat));

	error = VFS_STATFS(mp->mnt_vnodecovered->v_mount, &tmpstat, td);
	if (error)
		return (error);

	sbp->f_type = tmpstat.f_type;
	sbp->f_flags = tmpstat.f_flags;
	sbp->f_bsize = tmpstat.f_bsize;
	sbp->f_iosize = tmpstat.f_iosize;
	sbp->f_blocks = tmpstat.f_blocks;
	sbp->f_bfree = tmpstat.f_bfree;
	sbp->f_bavail = tmpstat.f_bavail;
	sbp->f_files = tmpstat.f_files;
	sbp->f_ffree = tmpstat.f_ffree;
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

static int
lomacfs_unmount(struct mount *mp, int mntflags, struct thread *td) {
	struct vnode *crootvp = VFSTOLOMAC(mp)->lm_rootvp;
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	ASSERT_VOP_LOCKED(crootvp, "lomacfs_unmount");

	if (VFSTOLOMAC(mp)->lm_flags & LM_TOOKROOT)
		crootvp->v_vflag |= VV_ROOT;

	error = vflush(mp, 1, flags);	/* have an extra root ref */
	if (error)
		return (error);

	free(VFSTOLOMAC(mp), M_LOMACFS);

	/* bye, lomacfs. */
	return (0);
}

static int
lomacfs_root(struct mount *mp, struct vnode **vpp) {
	int error;

	if (VFSTOLOMAC(mp)->lm_rootvp == NULL) {
		struct vnode *rootvp, *crootvp;

		crootvp = mp->mnt_vnodecovered;
		error = lomacfs_node_alloc(mp, NULL, NULL, crootvp, &rootvp);
		if (error)
			return (error);
		/*
		 * Reference twice to match the rest of the lomacfs vnodes.
		 */
		vref(crootvp);
		vref(crootvp);
		VFSTOLOMAC(mp)->lm_rootvp = rootvp;
		/*
		 * This releases the lock on root, but it doesn't release
		 * the reference so that root won't "disappear" until
		 * unmount.
		 */
		error = VOP_UNLOCK(rootvp, 0, curthread);
		if (error)
			return (error);
		/*
		 * This is some strange magic here...  I need to pretend
		 * that the mounted-on directory isn't a root vnode if I
		 * want things like __getcwd() to just fail and not crash.
		 */
		mp_fixme("This code needs the vn lock, not interlock.");
		mtx_lock(&crootvp->v_interlock);
		if (crootvp->v_vflag & VV_ROOT && crootvp == rootvnode) {
			crootvp->v_vflag &= ~VV_ROOT;
			VFSTOLOMAC(mp)->lm_flags |= LM_TOOKROOT;
		}
		mtx_unlock(&crootvp->v_interlock);
	}
	*vpp = VFSTOLOMAC(mp)->lm_rootvp;
	return (vget(*vpp, LK_EXCLUSIVE, curthread));
}

static struct vfsops lomacfs_vfsops = {
	lomacfs_mount,
	vfs_stdstart,
	lomacfs_unmount,
	lomacfs_root,
	vfs_stdquotactl,
	lomacfs_statfs,
	vfs_stdsync,
	vfs_stdvget,
	vfs_stdfhtovp,
	vfs_stdcheckexp,
	vfs_stdvptofh,
	vfs_stdinit,
	vfs_stduninit,
	vfs_stdextattrctl
};
VFS_SET(lomacfs_vfsops, lomacfs, VFCF_LOOPBACK);
MODULE_VERSION(lomacfs, 1);
MODULE_DEPEND(lomacfs, lomac_plm, 1, 1, 1);
