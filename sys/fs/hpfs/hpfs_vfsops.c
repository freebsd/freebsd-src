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

static int	hpfs_root(struct mount *, struct vnode **);
static int	hpfs_statfs(struct mount *, struct statfs *, struct thread *);
static int	hpfs_unmount(struct mount *, int, struct thread *);
static int	hpfs_vget(struct mount *mp, ino_t ino, int flags,
			       struct vnode **vpp);
static int	hpfs_mountfs(register struct vnode *, struct mount *, 
				  struct hpfs_args *, struct thread *);
static int	hpfs_vptofh(struct vnode *, struct fid *);
static int	hpfs_fhtovp(struct mount *, struct fid *, struct vnode **);
static int	hpfs_mount(struct mount *, char *, caddr_t,
				struct nameidata *, struct thread *);
static int	hpfs_init(struct vfsconf *);
static int	hpfs_uninit(struct vfsconf *);

static int
hpfs_init (
	struct vfsconf *vcp )
{
	dprintf(("hpfs_init():\n"));
	
	hpfs_hphashinit();
	return 0;
}

static int
hpfs_uninit (vfsp)
	struct vfsconf *vfsp;
{
	hpfs_hphashdestroy();
	return 0;;
}

static int
hpfs_mount ( 
	struct mount *mp,
	char *path,
	caddr_t data,
	struct nameidata *ndp,
	struct thread *td )
{
	size_t		size;
	int		err = 0;
	struct vnode	*devvp;
	struct hpfs_args args;
	struct hpfsmount *hpmp = 0;

	dprintf(("hpfs_mount():\n"));
	/*
	 ***
	 * Mounting non-root filesystem or updating a filesystem
	 ***
	 */

	/* copy in user arguments*/
	err = copyin(data, (caddr_t)&args, sizeof (struct hpfs_args));
	if (err)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		dprintf(("hpfs_mount: MNT_UPDATE: "));

		hpmp = VFSTOHPFS(mp);

		if (args.fspec == 0) {
			dprintf(("export 0x%x\n",args.export.ex_flags));
			err = vfs_export(mp, &args.export);
			if (err) {
				printf("hpfs_mount: vfs_export failed %d\n",
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
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, td);
	err = namei(ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}

	devvp = ndp->ni_vp;

	if (!vn_isdisk(devvp, &err)) 
		goto error_2;

	/*
	 ********************
	 * NEW MOUNT
	 ********************
	 */

	/*
	 * Since this is a new mount, we want the names for
	 * the device and the mount point copied in.  If an
	 * error occurs, the mountpoint is discarded by the
	 * upper level code.  Note that vfs_mount() handles
	 * copying the mountpoint f_mntonname for us, so we
	 * don't have to do it here unless we want to set it
	 * to something other than "path" for some rason.
	 */
	/* Save "mounted from" info for mount point (NULL pad)*/
	copyinstr(	args.fspec,			/* device name*/
			mp->mnt_stat.f_mntfromname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

	err = hpfs_mountfs(devvp, mp, &args, td);
	if (err)
		goto error_2;

	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void)VFS_STATFS(mp, &mp->mnt_stat, td);

	goto success;


error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return( err);
}

/*
 * Common code for mount and mountroot
 */
int
hpfs_mountfs(devvp, mp, argsp, td)
	register struct vnode *devvp;
	struct mount *mp;
	struct hpfs_args *argsp;
	struct thread *td;
{
	int error, ncount, ronly;
	struct sublock *sup;
	struct spblock *spp;
	struct hpfsmount *hpmp;
	struct buf *bp = NULL;
	struct vnode *vp;
	dev_t dev = devvp->v_rdev;

	dprintf(("hpfs_mountfs():\n"));
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	ncount = vcount(devvp);
	if (devvp->v_object)
		ncount -= 1;
	if (ncount > 1 && devvp != rootvp)
		return (EBUSY);

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = vinvalbuf(devvp, V_SAVE, td->td_ucred, td, 0, 0);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, td);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	/*
	 * Do actual mount
	 */
	hpmp = malloc(sizeof(struct hpfsmount), M_HPFSMNT, M_ZERO);

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
	hpmp->hpm_uid = argsp->uid;
	hpmp->hpm_gid = argsp->gid;
	hpmp->hpm_mode = argsp->mode;

	error = hpfs_bminit(hpmp);
	if (error)
		goto failed;

	error = hpfs_cpinit(hpmp, argsp);
	if (error) {
		hpfs_bmdeinit(hpmp);
		goto failed;
	}

	error = hpfs_root(mp, &vp);
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
	devvp->v_rdev->si_mountpoint = mp;
	return (0);

failed:
	if (bp)
		brelse (bp);
	mp->mnt_data = (qaddr_t)NULL;
	devvp->v_rdev->si_mountpoint = NULL;
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, td);
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
	
	error = vflush(mp, 0, flags);
	if (error) {
		printf("hpfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

	hpmp->hpm_devvp->v_rdev->si_mountpoint = NULL;

	vinvalbuf(hpmp->hpm_devvp, V_SAVE, NOCRED, td, 0, 0);
	error = VOP_CLOSE(hpmp->hpm_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, td);

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
	struct vnode **vpp )
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
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
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
	struct thread *td = curthread;	/* XXX */
	int error;

	dprintf(("hpfs_vget(0x%x): ",ino));

	*vpp = NULL;
	hp = NULL;
	vp = NULL;

	if ((error = hpfs_hphashvget(hpmp->hpm_dev, ino, flags, vpp, td)) != 0)
		return (error);
	if (*vpp != NULL) {
		dprintf(("hashed\n"));
		return (0);
	}

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
		M_HPFSNO, 0);

	error = getnewvnode("hpfs", hpmp->hpm_mp, hpfs_vnodeop_p, &vp);
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
	VREF(hp->h_devvp);

	error = vn_lock(vp, LK_EXCLUSIVE, td);
	if (error) {
		vput(vp);
		return (error);
	}

	do {
		if ((error =
		     hpfs_hphashvget(hpmp->hpm_dev, ino, flags, vpp, td))) {
			vput(vp);
			return (error);
		}
		if (*vpp != NULL) {
			dprintf(("hashed2\n"));
			vput(vp);
			return (0);
		}
	} while(lockmgr(&hpfs_hphash_lock,LK_EXCLUSIVE|LK_SLEEPFAIL,NULL,NULL));

	hpfs_hphashins(hp);

	lockmgr(&hpfs_hphash_lock, LK_RELEASE, NULL, NULL);

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
	hpfs_mount,
	vfs_stdstart,
	hpfs_unmount,
	hpfs_root,
	vfs_stdquotactl,
	hpfs_statfs,
	vfs_stdsync,
	hpfs_vget,
	hpfs_fhtovp,
	vfs_stdcheckexp,
	hpfs_vptofh,
	hpfs_init,
	hpfs_uninit,
	vfs_stdextattrctl,
};
VFS_SET(hpfs_vfsops, hpfs, 0);
