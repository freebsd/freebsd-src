/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	@(#)umap_vfsops.c	8.8 (Berkeley) 5/14/95
 *
 * $FreeBSD$
 */

/*
 * Umap Layer
 * (See mount_umapfs(8) for a description of this layer.)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include <fs/umapfs/umap.h>

static MALLOC_DEFINE(M_UMAPFSMNT, "UMAP mount", "UMAP mount structure");

static int	umapfs_fhtovp(struct mount *mp, struct fid *fidp,
				   struct vnode **vpp);
static int	umapfs_checkexp(struct mount *mp, struct sockaddr *nam,
				    int *extflagsp, struct ucred **credanonp);
static int	umapfs_mount(struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct thread *td);
static int	umapfs_quotactl(struct mount *mp, int cmd, uid_t uid,
				     caddr_t arg, struct thread *td);
static int	umapfs_root(struct mount *mp, struct vnode **vpp);
static int	umapfs_start(struct mount *mp, int flags, struct thread *td);
static int	umapfs_statfs(struct mount *mp, struct statfs *sbp,
				   struct thread *td);
static int	umapfs_unmount(struct mount *mp, int mntflags,
				    struct thread *td);
static int	umapfs_vget(struct mount *mp, ino_t ino, int flags,
				 struct vnode **vpp);
static int	umapfs_vptofh(struct vnode *vp, struct fid *fhp);
static int	umapfs_extattrctl(struct mount *mp, int cmd,
				       struct vnode *filename_vp,
				       int namespace, const char *attrname,
				       struct thread *td);

/*
 * Mount umap layer
 */
static int
umapfs_mount(mp, path, data, ndp, td)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct thread *td;
{
	struct umap_args args;
	struct vnode *lowerrootvp, *vp;
	struct vnode *umapm_rootvp;
	struct umap_mount *amp;
	size_t size;
	int error;
#ifdef DEBUG
	int	i;
#endif

	/*
	 * Only for root
	 */
	if ((error = suser(td)) != 0)
		return (error);

#ifdef DEBUG
	printf("umapfs_mount(mp = %p)\n", (void *)mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		return (EOPNOTSUPP);
		/* return (VFS_MOUNT(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, path, data, ndp, td));*/
	}

	/*
	 * Get argument
	 */
	error = copyin(data, (caddr_t)&args, sizeof(struct umap_args));
	if (error)
		return (error);

	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|WANTPARENT|LOCKLEAF,
		UIO_USERSPACE, args.target, td);
	error = namei(ndp);
	if (error)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;
#ifdef DEBUG
	printf("vp = %p, check for VDIR...\n", (void *)lowerrootvp);
#endif
	vrele(ndp->ni_dvp);
	ndp->ni_dvp = 0;

	if (lowerrootvp->v_type != VDIR) {
		vput(lowerrootvp);
		return (EINVAL);
	}

#ifdef DEBUG
	printf("mp = %p\n", (void *)mp);
#endif

	amp = (struct umap_mount *) malloc(sizeof(struct umap_mount),
				M_UMAPFSMNT, M_WAITOK);	/* XXX */

	/*
	 * Save reference to underlying FS
	 */
	amp->umapm_vfs = lowerrootvp->v_mount;

	/*
	 * Now copy in the number of entries and maps for umap mapping.
	 */
	if (args.nentries > MAPFILEENTRIES || args.gnentries >
	    GMAPFILEENTRIES) {
		vput(lowerrootvp);
		free(amp, M_UMAPFSMNT);
		/* XXX missing error = EINVAL ? */
		return (error);
	}

	amp->info_nentries = args.nentries;
	amp->info_gnentries = args.gnentries;
	error = copyin(args.mapdata, (caddr_t)amp->info_mapdata,
	    2*sizeof(u_long)*args.nentries);
	if (error) {
		free(amp, M_UMAPFSMNT);
		return (error);
	}

#ifdef DEBUG
	printf("umap_mount:nentries %d\n",args.nentries);
	for (i = 0; i < args.nentries; i++)
		printf("   %lu maps to %lu\n", amp->info_mapdata[i][0],
	 	    amp->info_mapdata[i][1]);
#endif

	error = copyin(args.gmapdata, (caddr_t)amp->info_gmapdata,
	    2*sizeof(u_long)*args.gnentries);
	if (error) {
		free(amp, M_UMAPFSMNT);
		return (error);
	}

#ifdef DEBUG
	printf("umap_mount:gnentries %d\n",args.gnentries);
	for (i = 0; i < args.gnentries; i++)
		printf("	group %lu maps to %lu\n",
		    amp->info_gmapdata[i][0],
	 	    amp->info_gmapdata[i][1]);
#endif


	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = umap_node_create(mp, lowerrootvp, &vp);
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0, td);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vrele(lowerrootvp);
		free(amp, M_UMAPFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in umapfs_unmount.
	 */
	ASSERT_VOP_LOCKED(vp, "umapfs_mount");
	umapm_rootvp = vp;
	umapm_rootvp->v_vflag |= VV_ROOT;
	amp->umapm_rootvp = umapm_rootvp;
	if (UMAPVPTOLOWERVP(umapm_rootvp)->v_mount->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) amp;
	vfs_getnewfsid(mp);

	(void) copyinstr(args.target, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)umapfs_statfs(mp, &mp->mnt_stat, td);
#ifdef DEBUG
	printf("umapfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif
	return (0);
}

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem will have been called
 * when that filesystem was mounted.
 */
static int
umapfs_start(mp, flags, td)
	struct mount *mp;
	int flags;
	struct thread *td;
{

	return (0);
	/* return (VFS_START(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, flags, td)); */
}

/*
 * Free reference to umap layer
 */
static int
umapfs_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	int error;
	int flags = 0;

#ifdef DEBUG
	printf("umapfs_unmount(mp = %p)\n", (void *)mp);
#endif

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef notyet
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	/* There is 1 extra root vnode reference (umapm_rootvp). */
	error = vflush(mp, 1, flags);
	if (error)
		return (error);

	/*
	 * Finally, throw away the umap_mount structure
	 */
	free(mp->mnt_data, M_UMAPFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

static int
umapfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct thread *td = curthread;	/* XXX */
	struct vnode *vp;

#ifdef DEBUG
	printf("umapfs_root(mp = %p, vp = %p->%p)\n",
	    (void *)mp, (void *)MOUNTTOUMAPMOUNT(mp)->umapm_rootvp,
	    (void *)UMAPVPTOLOWERVP(MOUNTTOUMAPMOUNT(mp)->umapm_rootvp));
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTOUMAPMOUNT(mp)->umapm_rootvp;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	*vpp = vp;
	return (0);
}

static int
umapfs_quotactl(mp, cmd, uid, arg, td)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{

	return (VFS_QUOTACTL(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, cmd, uid, arg, td));
}

static int
umapfs_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	int error;
	struct statfs mstat;

#ifdef DEBUG
	printf("umapfs_statfs(mp = %p, vp = %p->%p)\n",
	    (void *)mp, (void *)MOUNTTOUMAPMOUNT(mp)->umapm_rootvp,
	    (void *)UMAPVPTOLOWERVP(MOUNTTOUMAPMOUNT(mp)->umapm_rootvp));
#endif

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, &mstat, td);
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
umapfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{

	return (VFS_VGET(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, ino, flags, vpp));
}

static int
umapfs_fhtovp(mp, fidp, vpp)
	struct mount *mp;
	struct fid *fidp;
	struct vnode **vpp;
{

	return (VFS_FHTOVP(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, fidp, vpp));
}

static int
umapfs_checkexp(mp, nam, exflagsp, credanonp)
	struct mount *mp;
	struct sockaddr *nam;
	int *exflagsp;
	struct ucred **credanonp;
{

	return (VFS_CHECKEXP(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, nam,
		exflagsp, credanonp));
}

static int
umapfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (VFS_VPTOFH(UMAPVPTOLOWERVP(vp), fhp));
}

static int
umapfs_extattrctl(mp, cmd, filename_vp, namespace, attrname, td)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int namespace;
	const char *attrname;
	struct thread *td;
{

	return (VFS_EXTATTRCTL(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, cmd,
	    filename_vp, namespace, attrname, td));
}

static struct vfsops umap_vfsops = {
	umapfs_mount,
	umapfs_start,
	umapfs_unmount,
	umapfs_root,
	umapfs_quotactl,
	umapfs_statfs,
	vfs_stdsync,
	umapfs_vget,
	umapfs_fhtovp,
	umapfs_checkexp,
	umapfs_vptofh,
	umapfs_init,
	vfs_stduninit,
	umapfs_extattrctl,
};

VFS_SET(umap_vfsops, umapfs, VFCF_LOOPBACK);
