/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 * $FreeBSD$
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <fs/hpfs/hpfs.h>
#include <fs/hpfs/hpfsmount.h>
#include <fs/hpfs/hpfs_subr.h>

MALLOC_DEFINE(M_HPFSMNT, "HPFS mount", "HPFS mount structure");
MALLOC_DEFINE(M_HPFSNO, "HPFS node", "HPFS node structure");

struct sockaddr;

static int	hpfs_mountfs(register struct vnode *, struct mount *, 
				  struct thread *);

static vfs_fhtovp_t     hpfs_fhtovp;
static vfs_vget_t       hpfs_vget;
static vfs_cmount_t     hpfs_cmount;
static vfs_mount_t      hpfs_mount;
static vfs_root_t       hpfs_root;
static vfs_statfs_t     hpfs_statfs;
static vfs_unmount_t    hpfs_unmount;
static vfs_vptofh_t     hpfs_vptofh;

static int
hpfs_cmount ( 
	struct mntarg *ma,
	void *data,
	int flags,
	struct thread *td )
{
	struct hpfs_args args;
	int error;

	error = copyin(data, (caddr_t)&args, sizeof (struct hpfs_args));
	if (error)
		return (error);

	ma = mount_argsu(ma, "from", args.fspec, MAXPATHLEN);
	ma = mount_arg(ma, "export", &args.export, sizeof args.export);
	ma = mount_argf(ma, "uid", "%d", args.uid);
	ma = mount_argf(ma, "gid", "%d", args.gid);
	ma = mount_argf(ma, "mode", "%d", args.mode);
	if (args.flags & HPFSMNT_TABLES) {
		ma = mount_arg(ma, "d2u", args.d2u, sizeof args.d2u);
		ma = mount_arg(ma, "u2d", args.u2d, sizeof args.u2d);
	}

	error = kernel_mount(ma, flags);

	return (error);
}

static const char *hpfs_opts[] = {
	"from", "export", "uid", "gid", "mode", "d2u", "u2d", NULL
};

static int
hpfs_mount ( 
	struct mount *mp,
	struct thread *td )
{
	int		err = 0, error;
	struct vnode	*devvp;
	struct hpfsmount *hpmp = 0;
	struct nameidata ndp;
	struct export_args export;
	char *from;

	dprintf(("hpfs_omount():\n"));
	/*
	 ***
	 * Mounting non-root filesystem or updating a filesystem
	 ***
	 */
	if (vfs_filteropt(mp->mnt_optnew, hpfs_opts))
		return (EINVAL);

	from = vfs_getopts(mp->mnt_optnew, "from", &error);
	if (error)
		return (error);

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		dprintf(("hpfs_omount: MNT_UPDATE: "));

		hpmp = VFSTOHPFS(mp);

		if (from == NULL) {
			error = vfs_copyopt(mp->mnt_optnew, "export",
			    &export, sizeof export);
			if (error)
				return (error);
			dprintf(("export 0x%x\n",args.export.ex_flags));
			err = vfs_export(mp, &export);
			if (err) {
				printf("hpfs_omount: vfs_export failed %d\n",
					err);
			}
			goto success;
		} else {
			dprintf(("name [FAILED]\n"));
			err = EINVAL;
			goto success;
		}
		dprintf(("\n"));
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(&ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, from, td);
	err = namei(&ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}

	devvp = ndp.ni_vp;

	if (!vn_isdisk(devvp, &err)) {
		vput(devvp);
		return (err);
	}

	/*
	 ********************
	 * NEW MOUNT
	 ********************
	 */

	/*
	 * Since this is a new mount, we want the names for
	 * the device and the mount point copied in.  If an
	 * error occurs, the mountpoint is discarded by the
	 * upper level code.  Note that vfs_omount() handles
	 * copying the mountpoint f_mntonname for us, so we
	 * don't have to do it here unless we want to set it
	 * to something other than "path" for some rason.
	 */
	/* Save "mounted from" info for mount point (NULL pad)*/
	vfs_mountedfrom(mp, from);

	err = hpfs_mountfs(devvp, mp, td);
	if (err) {
		vrele(devvp);
		goto error_1;
	}

	goto success;

error_1:	/* no state to back out*/
	/* XXX: Missing NDFREE(&ndp, ...) */

success:
	return( err);
}

/*
 * Common code for mount and mountroot
 */
int
hpfs_mountfs(devvp, mp, td)
	register struct vnode *devvp;
	struct mount *mp;
	struct thread *td;
{
	int error, ronly, v;
	struct sublock *sup;
	struct spblock *spp;
	struct hpfsmount *hpmp;
	struct buf *bp = NULL;
	struct vnode *vp;
	struct cdev *dev = devvp->v_rdev;
	struct g_consumer *cp;
	struct bufobj *bo;

	if (mp->mnt_flag & MNT_ROOTFS)
		return (EOPNOTSUPP);
	dprintf(("hpfs_mountfs():\n"));
	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	/* XXX: use VOP_ACCESS to check FS perms */
	DROP_GIANT();
	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "hpfs", ronly ? 0 : 1);
	g_topology_unlock();
	PICKUP_GIANT();
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	bo = &devvp->v_bufobj;
	bo->bo_private = cp;
	bo->bo_ops = g_vfs_bufops;

	/*
	 * Do actual mount
	 */
	hpmp = malloc(sizeof(struct hpfsmount), M_HPFSMNT, M_WAITOK | M_ZERO);

	hpmp->hpm_cp = cp;
	hpmp->hpm_bo = bo;

	/* Read in SuperBlock */
	error = bread(devvp, SUBLOCK, SUSIZE, NOCRED, &bp);
	if (error)
		goto failed;
	bcopy(bp->b_data, &hpmp->hpm_su, sizeof(struct sublock));
	brelse(bp); bp = NULL;

	/* Read in SpareBlock */
	error = bread(devvp, SPBLOCK, SPSIZE, NOCRED, &bp);
	if (error)
		goto failed;
	bcopy(bp->b_data, &hpmp->hpm_sp, sizeof(struct spblock));
	brelse(bp); bp = NULL;

	sup = &hpmp->hpm_su;
	spp = &hpmp->hpm_sp;

	/* Check magic */
	if (sup->su_magic != SU_MAGIC) {
		printf("hpfs_mountfs: SuperBlock MAGIC DOESN'T MATCH\n");
		error = EINVAL;
		goto failed;
	}
	if (spp->sp_magic != SP_MAGIC) {
		printf("hpfs_mountfs: SpareBlock MAGIC DOESN'T MATCH\n");
		error = EINVAL;
		goto failed;
	}

	mp->mnt_data = (qaddr_t)hpmp;
	hpmp->hpm_devvp = devvp;
	hpmp->hpm_dev = devvp->v_rdev;
	hpmp->hpm_mp = mp;
	if (1 == vfs_scanopt(mp->mnt_optnew, "uid", "%d", &v))
		hpmp->hpm_uid = v;
	if (1 == vfs_scanopt(mp->mnt_optnew, "gid", "%d", &v))
		hpmp->hpm_gid = v;
	if (1 == vfs_scanopt(mp->mnt_optnew, "mode", "%d", &v))
		hpmp->hpm_mode = v;

	error = hpfs_bminit(hpmp);
	if (error)
		goto failed;

	error = hpfs_cpinit(mp, hpmp);
	if (error) {
		hpfs_bmdeinit(hpmp);
		goto failed;
	}

	error = hpfs_root(mp, LK_EXCLUSIVE, &vp, td);
	if (error) {
		hpfs_cpdeinit(hpmp);
		hpfs_bmdeinit(hpmp);
		goto failed;
	}

	vput(vp);

	mp->mnt_stat.f_fsid.val[0] = (long)dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	return (0);

failed:
	if (bp)
		brelse (bp);
	mp->mnt_data = (qaddr_t)NULL;
	g_vfs_close(cp, td);
	return (error);
}

static int
hpfs_unmount( 
	struct mount *mp,
	int mntflags,
	struct thread *td)
{
	int error, flags, ronly;
	register struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_unmount():\n"));

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("hpfs_unmount: vflushing...\n"));
	
	error = vflush(mp, 0, flags, td);
	if (error) {
		printf("hpfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

	vinvalbuf(hpmp->hpm_devvp, V_SAVE, td, 0, 0);
	g_vfs_close(hpmp->hpm_cp, td);
	vrele(hpmp->hpm_devvp);

	dprintf(("hpfs_umount: freeing memory...\n"));
	hpfs_cpdeinit(hpmp);
	hpfs_bmdeinit(hpmp);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	FREE(hpmp, M_HPFSMNT);

	return (0);
}

static int
hpfs_root(
	struct mount *mp,
	int flags,
	struct vnode **vpp,
	struct thread *td )
{
	int error = 0;
	struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_root():\n"));
	error = VFS_VGET(mp, (ino_t)hpmp->hpm_su.su_rootfno, LK_EXCLUSIVE, vpp);
	if(error) {
		printf("hpfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	return (error);
}

static int
hpfs_statfs(
	struct mount *mp,
	struct statfs *sbp,
	struct thread *td)
{
	struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_statfs(): HPFS%d.%d\n",
		hpmp->hpm_su.su_hpfsver, hpmp->hpm_su.su_fnctver));

	sbp->f_type = mp->mnt_vfc->vfc_typenum;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = hpmp->hpm_su.su_btotal;
	sbp->f_bfree = sbp->f_bavail = hpmp->hpm_bavail;
	sbp->f_ffree = 0;
	sbp->f_files = 0;
	sbp->f_flags = mp->mnt_flag;
	
	return (0);
}

/*ARGSUSED*/
static int
hpfs_fhtovp(
	struct mount *mp,
	struct fid *fhp,
	struct vnode **vpp)
{
	struct vnode *nvp;
	struct hpfid *hpfhp = (struct hpfid *)fhp;
	int error;

	if ((error = VFS_VGET(mp, hpfhp->hpfid_ino, LK_EXCLUSIVE, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with HPFS, we don't need to check anything else for now */
	*vpp = nvp;

	return (0);
}

static int
hpfs_vptofh(
	struct vnode *vp,
	struct fid *fhp)
{
	register struct hpfsnode *hpp;
	register struct hpfid *hpfhp;

	hpp = VTOHP(vp);
	hpfhp = (struct hpfid *)fhp;
	hpfhp->hpfid_len = sizeof(struct hpfid);
	hpfhp->hpfid_ino = hpp->h_no;
	/* hpfhp->hpfid_gen = hpp->h_gen; */
	return (0);
}

static int
hpfs_vget(
	struct mount *mp,
	ino_t ino,
	int flags,
	struct vnode **vpp) 
{
	struct hpfsmount *hpmp = VFSTOHPFS(mp);
	struct vnode *vp;
	struct hpfsnode *hp;
	struct buf *bp;
	int error;

	dprintf(("hpfs_vget(0x%x): ",ino));

	error = vfs_hash_get(mp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	*vpp = NULL;
	hp = NULL;
	vp = NULL;

	/*
	 * We have to lock node creation for a while,
	 * but then we have to call getnewvnode(), 
	 * this may cause hpfs_reclaim() to be called,
	 * this may need to VOP_VGET() parent dir for
	 * update reasons, and if parent is not in
	 * hash, we have to lock node creation...
	 * To solve this, we MALLOC, getnewvnode and init while
	 * not locked (probability of node appearence
	 * at that time is little, and anyway - we'll
	 * check for it).
	 */
	MALLOC(hp, struct hpfsnode *, sizeof(struct hpfsnode), 
		M_HPFSNO, M_WAITOK);

	error = getnewvnode("hpfs", hpmp->hpm_mp, &hpfs_vnodeops, &vp);
	if (error) {
		printf("hpfs_vget: can't get new vnode\n");
		FREE(hp, M_HPFSNO);
		return (error);
	}

	dprintf(("prenew "));

	vp->v_data = hp;

	if (ino == (ino_t)hpmp->hpm_su.su_rootfno) 
		vp->v_vflag |= VV_ROOT;


	mtx_init(&hp->h_interlock, "hpfsnode interlock", NULL, MTX_DEF);

	hp->h_flag = H_INVAL;
	hp->h_vp = vp;
	hp->h_hpmp = hpmp;
	hp->h_no = ino;
	hp->h_dev = hpmp->hpm_dev;
	hp->h_uid = hpmp->hpm_uid;
	hp->h_gid = hpmp->hpm_uid;
	hp->h_mode = hpmp->hpm_mode;
	hp->h_devvp = hpmp->hpm_devvp;

	error = vfs_hash_insert(vp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	error = bread(hpmp->hpm_devvp, ino, FNODESIZE, NOCRED, &bp);
	if (error) {
		printf("hpfs_vget: can't read ino %d\n",ino);
		vput(vp);
		return (error);
	}
	bcopy(bp->b_data, &hp->h_fn, sizeof(struct fnode));
	brelse(bp);

	if (hp->h_fn.fn_magic != FN_MAGIC) {
		printf("hpfs_vget: MAGIC DOESN'T MATCH\n");
		vput(vp);
		return (EINVAL);
	}

	vp->v_type = hp->h_fn.fn_flag ? VDIR:VREG;
	hp->h_flag &= ~H_INVAL;

	*vpp = vp;

	return (0);
}

static struct vfsops hpfs_vfsops = {
	.vfs_fhtovp =		hpfs_fhtovp,
	.vfs_cmount =		hpfs_cmount,
	.vfs_mount =		hpfs_mount,
	.vfs_root =		hpfs_root,
	.vfs_statfs =		hpfs_statfs,
	.vfs_unmount =		hpfs_unmount,
	.vfs_vget =		hpfs_vget,
	.vfs_vptofh =		hpfs_vptofh,
};
VFS_SET(hpfs_vfsops, hpfs, 0);
