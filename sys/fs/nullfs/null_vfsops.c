/*
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
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <miscfs/nullfs/null.h>

static MALLOC_DEFINE(M_NULLFSMNT, "NULLFS mount", "NULLFS mount structure");

static int	nullfs_fhtovp(struct mount *mp, struct fid *fidp,
				   struct vnode **vpp);
static int	nullfs_checkexp(struct mount *mp, struct sockaddr *nam,
				    int *extflagsp, struct ucred **credanonp);
static int	nullfs_mount(struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct proc *p);
static int	nullfs_quotactl(struct mount *mp, int cmd, uid_t uid,
				     caddr_t arg, struct proc *p);
static int	nullfs_root(struct mount *mp, struct vnode **vpp);
static int	nullfs_start(struct mount *mp, int flags, struct proc *p);
static int	nullfs_statfs(struct mount *mp, struct statfs *sbp,
				   struct proc *p);
static int	nullfs_sync(struct mount *mp, int waitfor,
				 struct ucred *cred, struct proc *p);
static int	nullfs_unmount(struct mount *mp, int mntflags, struct proc *p);
static int	nullfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp);
static int	nullfs_vptofh(struct vnode *vp, struct fid *fhp);

/*
 * Mount null layer
 */
static int
nullfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	struct null_args args;
	struct vnode *lowerrootvp, *vp;
	struct vnode *nullm_rootvp;
	struct null_mount *xmp;
	u_int size;
	int isvnunlocked = 0;

	NULLFSDEBUG("nullfs_mount(mp = %p)\n", (void *)mp);

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		return (EOPNOTSUPP);
		/* return VFS_MOUNT(MOUNTTONULLMOUNT(mp)->nullm_vfs, path, data, ndp, p);*/
	}

	/*
	 * Get argument
	 */
	error = copyin(data, (caddr_t)&args, sizeof(struct null_args));
	if (error)
		return (error);

	/*
	 * Unlock lower node to avoid deadlock.
	 * (XXX) VOP_ISLOCKED is needed?
	 */
	if ((mp->mnt_vnodecovered->v_op == null_vnodeop_p) &&
		VOP_ISLOCKED(mp->mnt_vnodecovered, NULL)) {
		VOP_UNLOCK(mp->mnt_vnodecovered, 0, p);
		isvnunlocked = 1;
	}
	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|WANTPARENT|LOCKLEAF,
		UIO_USERSPACE, args.target, p);
	error = namei(ndp);
	/*
	 * Re-lock vnode.
	 */
	if (isvnunlocked && !VOP_ISLOCKED(mp->mnt_vnodecovered, NULL))
		vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY, p);

	if (error)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;

	vrele(ndp->ni_dvp);
	ndp->ni_dvp = NULLVP;

	/*
	 * Check multi null mount to avoid `lock against myself' panic.
	 */
	if (lowerrootvp == VTONULL(mp->mnt_vnodecovered)->null_lowervp) {
		NULLFSDEBUG("nullfs_mount: multi null mount?\n");
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
	error = null_node_create(mp, lowerrootvp, &vp);
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0, p);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vrele(lowerrootvp);
		free(xmp, M_NULLFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in nullfs_unmount.
	 */
	nullm_rootvp = vp;
	nullm_rootvp->v_flag |= VROOT;
	xmp->nullm_rootvp = nullm_rootvp;
	if (NULLVPTOLOWERVP(nullm_rootvp)->v_mount->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) xmp;
	vfs_getnewfsid(mp);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.target, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)nullfs_statfs(mp, &mp->mnt_stat, p);
	NULLFSDEBUG("nullfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
	return (0);
}

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem will have been called
 * when that filesystem was mounted.
 */
static int
nullfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
	/* return VFS_START(MOUNTTONULLMOUNT(mp)->nullm_vfs, flags, p); */
}

/*
 * Free reference to null layer
 */
static int
nullfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *nullm_rootvp = MOUNTTONULLMOUNT(mp)->nullm_rootvp;
	void *mntdata;
	int error;
	int flags = 0;

	NULLFSDEBUG("nullfs_unmount: mp = %p\n", (void *)mp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#if 0
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (nullm_rootvp->v_usecount > 1)
		return (EBUSY);
	error = vflush(mp, nullm_rootvp, flags);
	if (error)
		return (error);

#ifdef NULLFS_DEBUG
	vprint("alias root of lower", nullm_rootvp);
#endif
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(nullm_rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(nullm_rootvp);
	/*
	 * Finally, throw away the null_mount structure
	 */
	mntdata = mp->mnt_data;
	mp->mnt_data = 0;
	free(mntdata, M_NULLFSMNT);	/* XXX */
	return 0;
}

static int
nullfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct proc *p = curproc;	/* XXX */
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
		/*
		 * XXX
		 * Should we check type of node?
		 */
		printf("nullfs_root: multi null mount?\n");
		vrele(vp);
		return (EDEADLK);
	}
#endif
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	return 0;
}

static int
nullfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return VFS_QUOTACTL(MOUNTTONULLMOUNT(mp)->nullm_vfs, cmd, uid, arg, p);
}

static int
nullfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;
	struct statfs mstat;

	NULLFSDEBUG("nullfs_statfs(mp = %p, vp = %p->%p)\n", (void *)mp,
	    (void *)MOUNTTONULLMOUNT(mp)->nullm_rootvp,
	    (void *)NULLVPTOLOWERVP(MOUNTTONULLMOUNT(mp)->nullm_rootvp));

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTONULLMOUNT(mp)->nullm_vfs, &mstat, p);
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
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

static int
nullfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	/*
	 * XXX - Assumes no data cached at null layer.
	 */
	return (0);
}

static int
nullfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{

	return VFS_VGET(MOUNTTONULLMOUNT(mp)->nullm_vfs, ino, vpp);
}

static int
nullfs_fhtovp(mp, fidp, vpp)
	struct mount *mp;
	struct fid *fidp;
	struct vnode **vpp;
{

	return VFS_FHTOVP(MOUNTTONULLMOUNT(mp)->nullm_vfs, fidp, vpp);
}

static int
nullfs_checkexp(mp, nam, extflagsp, credanonp)
	struct mount *mp;
	struct sockaddr *nam;
	int *extflagsp; 
	struct ucred **credanonp;
{

	return VFS_CHECKEXP(MOUNTTONULLMOUNT(mp)->nullm_vfs, nam, 
		extflagsp, credanonp);
}

static int
nullfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return VFS_VPTOFH(NULLVPTOLOWERVP(vp), fhp);
}

static int                        
nullfs_extattrctl(mp, cmd, attrname, arg, p)
	struct mount *mp;
	int cmd;
	const char *attrname;
	caddr_t arg;
	struct proc *p;            
{
	return VFS_EXTATTRCTL(MOUNTTONULLMOUNT(mp)->nullm_vfs, cmd, attrname,
	    arg, p);
}


static struct vfsops null_vfsops = {
	nullfs_mount,
	nullfs_start,
	nullfs_unmount,
	nullfs_root,
	nullfs_quotactl,
	nullfs_statfs,
	nullfs_sync,
	nullfs_vget,
	nullfs_fhtovp,
	nullfs_checkexp,
	nullfs_vptofh,
	nullfs_init,
	nullfs_uninit,
	nullfs_extattrctl,
};

VFS_SET(null_vfsops, nullfs, VFCF_LOOPBACK);
