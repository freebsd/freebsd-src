/*	$NetBSD: ntfs_vfsops.c,v 1.23 1999/11/15 19:38:14 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
#include <sys/systm.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#if defined(__NetBSD__)
#include <vm/vm_prot.h>
#endif
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#if defined(__NetBSD__)
#include <miscfs/specfs/specdev.h>
#endif

/*#define NTFS_DEBUG 1*/
#include <ntfs/ntfs.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_subr.h>
#include <ntfs/ntfs_vfsops.h>
#include <ntfs/ntfs_ihash.h>
#include <ntfs/ntfsmount.h>

#if defined(__FreeBSD__)
MALLOC_DEFINE(M_NTFSMNT, "NTFS mount", "NTFS mount structure");
MALLOC_DEFINE(M_NTFSNTNODE,"NTFS ntnode",  "NTFS ntnode information");
MALLOC_DEFINE(M_NTFSFNODE,"NTFS fnode",  "NTFS fnode information");
MALLOC_DEFINE(M_NTFSDIR,"NTFS dir",  "NTFS dir buffer");
#endif

static int	ntfs_root __P((struct mount *, struct vnode **));
static int	ntfs_statfs __P((struct mount *, struct statfs *,
				 struct proc *));
static int	ntfs_unmount __P((struct mount *, int, struct proc *));
static int	ntfs_vget __P((struct mount *mp, ino_t ino,
			       struct vnode **vpp));
static int	ntfs_mountfs __P((register struct vnode *, struct mount *, 
				  struct ntfs_args *, struct proc *));
static int	ntfs_vptofh __P((struct vnode *, struct fid *));
static int	ntfs_fhtovp __P((struct mount *, struct fid *,
				 struct vnode **));

#if !defined (__FreeBSD__)
static int	ntfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
				   struct proc *));
static int	ntfs_start __P((struct mount *, int, struct proc *));
static int	ntfs_sync __P((struct mount *, int, struct ucred *,
			       struct proc *));
#endif

#if defined(__FreeBSD__)
struct sockaddr;
static int	ntfs_mount __P((struct mount *, char *, caddr_t,
				struct nameidata *, struct proc *));
static int	ntfs_init __P((struct vfsconf *));
static int	ntfs_checkexp __P((struct mount *, struct sockaddr *,
				   int *, struct ucred **));
#elif defined(__NetBSD__)
static int	ntfs_mount __P((struct mount *, const char *, void *,
				struct nameidata *, struct proc *));
static void	ntfs_init __P((void));
static int	ntfs_mountroot __P((void));
static int	ntfs_sysctl __P((int *, u_int, void *, size_t *, void *,
				 size_t, struct proc *));
static int	ntfs_checkexp __P((struct mount *, struct mbuf *,
				   int *, struct ucred **));
#endif

/*
 * Verify a remote client has export rights and return these rights via.
 * exflagsp and credanonp.
 */
static int
ntfs_checkexp(mp, nam, exflagsp, credanonp)
#if defined(__FreeBSD__)
	register struct mount *mp;
	struct sockaddr *nam;
	int *exflagsp;
	struct ucred **credanonp;
#else /* defined(__NetBSD__) */
	register struct mount *mp;
	struct mbuf *nam;
	int *exflagsp;
	struct ucred **credanonp;
#endif
{
	register struct netcred *np;
	register struct ntfsmount *ntm = VFSTONTFS(mp);

	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &ntm->ntm_export, nam);
	if (np == NULL)
		return (EACCES);

	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

#if defined(__NetBSD__)
/*ARGSUSED*/
static int
ntfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	return (EINVAL);
}

static int
ntfs_mountroot()
{
	struct mount *mp;
	extern struct vnode *rootvp;
	struct proc *p = curproc;	/* XXX */
	int error;
	struct ntfs_args args;

	if (root_device->dv_class != DV_DISK)
		return (ENODEV);

	/*
	 * Get vnodes for rootdev.
	 */
	if (bdevvp(rootdev, &rootvp))
		panic("ntfs_mountroot: can't setup rootvp");

	if ((error = vfs_rootmountalloc(MOUNT_NTFS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}

	args.flag = 0;
	args.uid = 0;
	args.gid = 0;
	args.mode = 0777;

	if ((error = ntfs_mountfs(rootvp, mp, &args, p)) != 0) {
		mp->mnt_op->vfs_refcount--;
		vfs_unbusy(mp);
		free(mp, M_MOUNT);
		vrele(rootvp);
		return (error);
	}

	mtx_enter(&mountlist_mtx, MTX_DEF);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_exit(&mountlist_mtx, MTX_DEF);
	(void)ntfs_statfs(mp, &mp->mnt_stat, p);
	vfs_unbusy(mp);
	return (0);
}

static void
ntfs_init ()
{
	ntfs_nthashinit();
	ntfs_toupper_init();
}

#elif defined(__FreeBSD__)

static int
ntfs_init (
	struct vfsconf *vcp )
{
	ntfs_nthashinit();
	ntfs_toupper_init();
	return 0;
}

static int
ntfs_uninit (
	struct vfsconf *vcp )
{
	ntfs_toupper_destroy();
	ntfs_nthashdestroy();
	return 0;
}

#endif /* NetBSD */

static int
ntfs_mount ( 
	struct mount *mp,
#if defined(__FreeBSD__)
	char *path,
	caddr_t data,
#else
	const char *path,
	void *data,
#endif
	struct nameidata *ndp,
	struct proc *p )
{
	size_t		size;
	int		err = 0;
	struct vnode	*devvp;
	struct ntfs_args args;

#ifdef __FreeBSD__
	/*
	 * Use NULL path to flag a root mount
	 */
	if( path == NULL) {
		/*
		 ***
		 * Mounting root file system
		 ***
		 */
	
		/* Get vnode for root device*/
		if( bdevvp( rootdev, &rootvp))
			panic("ffs_mountroot: can't setup bdevvp for root");

		/*
		 * FS specific handling
		 */
		mp->mnt_flag |= MNT_RDONLY;	/* XXX globally applicable?*/

		/*
		 * Attempt mount
		 */
		if( ( err = ntfs_mountfs(rootvp, mp, &args, p)) != 0) {
			/* fs specific cleanup (if any)*/
			goto error_1;
		}

		goto dostatfs;		/* success*/

	}
#endif /* FreeBSD */

	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	err = copyin(data, (caddr_t)&args, sizeof (struct ntfs_args));
	if (err)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code.
			 */
			struct ntfsmount *ntm = VFSTONTFS(mp);
			err = vfs_export(mp, &ntm->ntm_export, &args.export);
			goto success;
		}

		printf("ntfs_mount(): MNT_UPDATE not supported\n");
		err = EINVAL;
		goto error_1;
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	err = namei(ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}
	NDFREE(ndp, NDF_ONLY_PNBUF);
	devvp = ndp->ni_vp;

#if defined(__FreeBSD__)
	if (!vn_isdisk(devvp, &err)) 
		goto error_2;
#else
	if (devvp->v_type != VBLK) {
		err = ENOTBLK;
		goto error_2;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		err = ENXIO;
		goto error_2;
	}
#endif
	if (mp->mnt_flag & MNT_UPDATE) {
#if 0
		/*
		 ********************
		 * UPDATE
		 ********************
		 */

		if (devvp != ntmp->um_devvp)
			err = EINVAL;	/* needs translation */
		else
			vrele(devvp);
		/*
		 * Update device name only on success
		 */
		if( !err) {
			/* Save "mounted from" info for mount point (NULL pad)*/
			copyinstr(	args.fspec,
					mp->mnt_stat.f_mntfromname,
					MNAMELEN - 1,
					&size);
			bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
		}
#endif
	} else {
		/*
		 ********************
		 * NEW MOUNT
		 ********************
		 */

		/*
		 * Since this is a new mount, we want the names for
		 * the device and the mount point copied in.  If an
		 * error occurs,  the mountpoint is discarded by the
		 * upper level code.
		 */
		/* Save "last mounted on" info for mount point (NULL pad)*/
		copyinstr(	path,				/* mount point*/
				mp->mnt_stat.f_mntonname,	/* save area*/
				MNAMELEN - 1,			/* max size*/
				&size);				/* real size*/
		bzero( mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

		/* Save "mounted from" info for mount point (NULL pad)*/
		copyinstr(	args.fspec,			/* device name*/
				mp->mnt_stat.f_mntfromname,	/* save area*/
				MNAMELEN - 1,			/* max size*/
				&size);				/* real size*/
		bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

		err = ntfs_mountfs(devvp, mp, &args, p);
	}
	if (err) {
		goto error_2;
	}

#ifdef __FreeBSD__
dostatfs:
#endif
	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void)VFS_STATFS(mp, &mp->mnt_stat, p);

	goto success;


error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return(err);
}

/*
 * Common code for mount and mountroot
 */
int
ntfs_mountfs(devvp, mp, argsp, p)
	register struct vnode *devvp;
	struct mount *mp;
	struct ntfs_args *argsp;
	struct proc *p;
{
	struct buf *bp;
	struct ntfsmount *ntmp;
	dev_t dev = devvp->v_rdev;
	int error, ronly, ncount, i;
	struct vnode *vp;

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
#if defined(__FreeBSD__)
	if (devvp->v_object)
		ncount -= 1;
#endif
	if (ncount > 1 && devvp != rootvp)
		return (EBUSY);
#if defined(__FreeBSD__)
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	VOP__UNLOCK(devvp, 0, p);
#else
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
#endif
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	VOP__UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	bp = NULL;

	error = bread(devvp, BBLOCK, BBSIZE, NOCRED, &bp);
	if (error)
		goto out;
	ntmp = malloc( sizeof *ntmp, M_NTFSMNT, M_WAITOK );
	bzero( ntmp, sizeof *ntmp );
	bcopy( bp->b_data, &ntmp->ntm_bootfile, sizeof(struct bootfile) );
	brelse( bp );
	bp = NULL;

	if (strncmp(ntmp->ntm_bootfile.bf_sysid, NTFS_BBID, NTFS_BBIDLEN)) {
		error = EINVAL;
		dprintf(("ntfs_mountfs: invalid boot block\n"));
		goto out;
	}

	{
		int8_t cpr = ntmp->ntm_mftrecsz;
		if( cpr > 0 )
			ntmp->ntm_bpmftrec = ntmp->ntm_spc * cpr;
		else
			ntmp->ntm_bpmftrec = (1 << (-cpr)) / ntmp->ntm_bps;
	}
	dprintf(("ntfs_mountfs(): bps: %d, spc: %d, media: %x, mftrecsz: %d (%d sects)\n",
		ntmp->ntm_bps,ntmp->ntm_spc,ntmp->ntm_bootfile.bf_media,
		ntmp->ntm_mftrecsz,ntmp->ntm_bpmftrec));
	dprintf(("ntfs_mountfs(): mftcn: 0x%x|0x%x\n",
		(u_int32_t)ntmp->ntm_mftcn,(u_int32_t)ntmp->ntm_mftmirrcn));

	ntmp->ntm_mountp = mp;
	ntmp->ntm_dev = dev;
	ntmp->ntm_devvp = devvp;
	ntmp->ntm_uid = argsp->uid;
	ntmp->ntm_gid = argsp->gid;
	ntmp->ntm_mode = argsp->mode;
	ntmp->ntm_flag = argsp->flag;
	mp->mnt_data = (qaddr_t)ntmp;

	dprintf(("ntfs_mountfs(): case-%s,%s uid: %d, gid: %d, mode: %o\n",
		(ntmp->ntm_flag & NTFS_MFLAG_CASEINS)?"insens.":"sens.",
		(ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)?" allnames,":"",
		ntmp->ntm_uid, ntmp->ntm_gid, ntmp->ntm_mode));

	/*
	 * We read in some system nodes to do not allow 
	 * reclaim them and to have everytime access to them.
	 */ 
	{
		int pi[3] = { NTFS_MFTINO, NTFS_ROOTINO, NTFS_BITMAPINO };
		for (i=0; i<3; i++) {
			error = VFS_VGET(mp, pi[i], &(ntmp->ntm_sysvn[pi[i]]));
			if(error)
				goto out1;
			ntmp->ntm_sysvn[pi[i]]->v_flag |= VSYSTEM;
			VREF(ntmp->ntm_sysvn[pi[i]]);
			vput(ntmp->ntm_sysvn[pi[i]]);
		}
	}

	/* read the Unicode lowercase --> uppercase translation table,
	 * if necessary */
	if ((error = ntfs_toupper_use(mp, ntmp)))
		goto out1;

	/*
	 * Scan $BitMap and count free clusters
	 */
	error = ntfs_calccfree(ntmp, &ntmp->ntm_cfree);
	if(error)
		goto out1;

	/*
	 * Read and translate to internal format attribute
	 * definition file. 
	 */
	{
		int num,j;
		struct attrdef ad;

		/* Open $AttrDef */
		error = VFS_VGET(mp, NTFS_ATTRDEFINO, &vp );
		if(error) 
			goto out1;

		/* Count valid entries */
		for(num=0;;num++) {
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					num * sizeof(ad), sizeof(ad),
					&ad, NULL);
			if (error)
				goto out1;
			if (ad.ad_name[0] == 0)
				break;
		}

		/* Alloc memory for attribute definitions */
		MALLOC(ntmp->ntm_ad, struct ntvattrdef *,
			num * sizeof(struct ntvattrdef),
			M_NTFSMNT, M_WAITOK);

		ntmp->ntm_adnum = num;

		/* Read them and translate */
		for(i=0;i<num;i++){
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					i * sizeof(ad), sizeof(ad),
					&ad, NULL);
			if (error)
				goto out1;
			j = 0;
			do {
				ntmp->ntm_ad[i].ad_name[j] = ad.ad_name[j];
			} while(ad.ad_name[j++]);
			ntmp->ntm_ad[i].ad_namelen = j - 1;
			ntmp->ntm_ad[i].ad_type = ad.ad_type;
		}

		vput(vp);
	}

#if defined(__FreeBSD__)
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
#else
	mp->mnt_stat.f_fsid.val[0] = dev;
	mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_NTFS);
#endif
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_rdev->si_mountpoint = mp;
	return (0);

out1:
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		if(ntmp->ntm_sysvn[i]) vrele(ntmp->ntm_sysvn[i]);

	if (vflush(mp,NULLVP,0))
		dprintf(("ntfs_mountfs: vflush failed\n"));

out:
	devvp->v_rdev->si_mountpoint = NULL;
	if (bp)
		brelse(bp);

#if defined __NetBSD__
	/* lock the device vnode before calling VOP_CLOSE() */
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	VOP__UNLOCK(devvp, 0, p);
#else
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
#endif
	
	return (error);
}

#if !defined(__FreeBSD__)
static int
ntfs_start (
	struct mount *mp,
	int flags,
	struct proc *p )
{
	return (0);
}
#endif

static int
ntfs_unmount( 
	struct mount *mp,
	int mntflags,
	struct proc *p)
{
	register struct ntfsmount *ntmp;
	int error, ronly = 0, flags, i;

	dprintf(("ntfs_unmount: unmounting...\n"));
	ntmp = VFSTONTFS(mp);

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("ntfs_unmount: vflushing...\n"));
	error = vflush(mp,NULLVP,flags | SKIPSYSTEM);
	if (error) {
		printf("ntfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

	/* Check if only system vnodes are rest */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if((ntmp->ntm_sysvn[i]) && 
		    (ntmp->ntm_sysvn[i]->v_usecount > 1)) return (EBUSY);

	/* Dereference all system vnodes */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if(ntmp->ntm_sysvn[i]) vrele(ntmp->ntm_sysvn[i]);

	/* vflush system vnodes */
	error = vflush(mp,NULLVP,flags);
	if (error)
		printf("ntfs_unmount: vflush failed(sysnodes): %d\n",error);

	/* Check if the type of device node isn't VBAD before
	 * touching v_specinfo.  If the device vnode is revoked, the
	 * field is NULL and touching it causes null pointer derefercence.
	 */
	if (ntmp->ntm_devvp->v_type != VBAD)
		ntmp->ntm_devvp->v_rdev->si_mountpoint = NULL;

	vinvalbuf(ntmp->ntm_devvp, V_SAVE, NOCRED, p, 0, 0);

#if defined(__NetBSD__)
	/* lock the device vnode before calling VOP_CLOSE() */
	VOP_LOCK(ntmp->ntm_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ntmp->ntm_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	VOP__UNLOCK(ntmp->ntm_devvp, 0, p);
#else
	error = VOP_CLOSE(ntmp->ntm_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
#endif

	vrele(ntmp->ntm_devvp);

	/* free the toupper table, if this has been last mounted ntfs volume */
	ntfs_toupper_unuse();

	dprintf(("ntfs_umount: freeing memory...\n"));
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	FREE(ntmp->ntm_ad, M_NTFSMNT);
	FREE(ntmp, M_NTFSMNT);
	return (error);
}

static int
ntfs_root(
	struct mount *mp,
	struct vnode **vpp )
{
	struct vnode *nvp;
	int error = 0;

	dprintf(("ntfs_root(): sysvn: %p\n",
		VFSTONTFS(mp)->ntm_sysvn[NTFS_ROOTINO]));
	error = VFS_VGET(mp, (ino_t)NTFS_ROOTINO, &nvp);
	if(error) {
		printf("ntfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	*vpp = nvp;
	return (0);
}

#if !defined(__FreeBSD__)
static int
ntfs_quotactl ( 
	struct mount *mp,
	int cmds,
	uid_t uid,
	caddr_t arg,
	struct proc *p)
{
	printf("\nntfs_quotactl():\n");
	return EOPNOTSUPP;
}
#endif

int
ntfs_calccfree(
	struct ntfsmount *ntmp,
	cn_t *cfreep)
{
	struct vnode *vp;
	u_int8_t *tmp;
	int j, error;
	long cfree = 0;
	size_t bmsize, i;

	vp = ntmp->ntm_sysvn[NTFS_BITMAPINO];

	bmsize = VTOF(vp)->f_size;

	MALLOC(tmp, u_int8_t *, bmsize, M_TEMP, M_WAITOK);

	error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       0, bmsize, tmp, NULL);
	if (error)
		goto out;

	for(i=0;i<bmsize;i++)
		for(j=0;j<8;j++)
			if(~tmp[i] & (1 << j)) cfree++;
	*cfreep = cfree;

    out:
	FREE(tmp, M_TEMP);
	return(error);
}

static int
ntfs_statfs(
	struct mount *mp,
	struct statfs *sbp,
	struct proc *p)
{
	struct ntfsmount *ntmp = VFSTONTFS(mp);
	u_int64_t mftsize,mftallocated;

	dprintf(("ntfs_statfs():\n"));

	mftsize = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_size;
	mftallocated = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_allocated;

#if defined(__FreeBSD__)
	sbp->f_type = mp->mnt_vfc->vfc_typenum;
#elif defined(__NetBSD__)
	sbp->f_type = 0;
#else
	sbp->f_type = MOUNT_NTFS;
#endif
	sbp->f_bsize = ntmp->ntm_bps;
	sbp->f_iosize = ntmp->ntm_bps * ntmp->ntm_spc;
	sbp->f_blocks = ntmp->ntm_bootfile.bf_spv;
	sbp->f_bfree = sbp->f_bavail = ntfs_cntobn(ntmp->ntm_cfree);
	sbp->f_ffree = sbp->f_bfree / ntmp->ntm_bpmftrec;
	sbp->f_files = mftallocated / ntfs_bntob(ntmp->ntm_bpmftrec) +
		       sbp->f_ffree;
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	sbp->f_flags = mp->mnt_flag;
#ifdef __NetBSD__
	strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name, MFSNAMELEN);
#endif
	
	return (0);
}

#if !defined(__FreeBSD__)
static int
ntfs_sync (
	struct mount *mp,
	int waitfor,
	struct ucred *cred,
	struct proc *p)
{
	/*dprintf(("ntfs_sync():\n"));*/
	return (0);
}
#endif

/*ARGSUSED*/
static int
ntfs_fhtovp(
	struct mount *mp,
	struct fid *fhp,
	struct vnode **vpp)
{
	struct vnode *nvp;
	struct ntfid *ntfhp = (struct ntfid *)fhp;
	int error;

	ddprintf(("ntfs_fhtovp(): %s: %d\n", mp->mnt_stat->f_mntonname,
		ntfhp->ntfid_ino));

	if ((error = VFS_VGET(mp, ntfhp->ntfid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with NTFS, we don't need to check anything else for now */
	*vpp = nvp;

	return (0);
}

static int
ntfs_vptofh(
	struct vnode *vp,
	struct fid *fhp)
{
	register struct ntnode *ntp;
	register struct ntfid *ntfhp;

	ddprintf(("ntfs_fhtovp(): %s: %p\n", vp->v_mount->mnt_stat->f_mntonname,
		vp));

	ntp = VTONT(vp);
	ntfhp = (struct ntfid *)fhp;
	ntfhp->ntfid_len = sizeof(struct ntfid);
	ntfhp->ntfid_ino = ntp->i_number;
	/* ntfhp->ntfid_gen = ntp->i_gen; */
	return (0);
}

int
ntfs_vgetex(
	struct mount *mp,
	ino_t ino,
	u_int32_t attrtype,
	char *attrname,
	u_long lkflags,
	u_long flags,
	struct proc *p,
	struct vnode **vpp) 
{
	int error;
	register struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct fnode *fp;
	struct vnode *vp;
	enum vtype f_type;

	dprintf(("ntfs_vgetex: ino: %d, attr: 0x%x:%s, lkf: 0x%lx, f: 0x%lx\n",
		ino, attrtype, attrname?attrname:"", (u_long)lkflags,
		(u_long)flags ));

	ntmp = VFSTONTFS(mp);
	*vpp = NULL;

	/* Get ntnode */
	error = ntfs_ntlookup(ntmp, ino, &ip);
	if (error) {
		printf("ntfs_vget: ntfs_ntget failed\n");
		return (error);
	}

	/* It may be not initialized fully, so force load it */
	if (!(flags & VG_DONTLOADIN) && !(ip->i_flag & IN_LOADED)) {
		error = ntfs_loadntnode(ntmp, ip);
		if(error) {
			printf("ntfs_vget: CAN'T LOAD ATTRIBUTES FOR INO: %d\n",
			       ip->i_number);
			ntfs_ntput(ip);
			return (error);
		}
	}

	error = ntfs_fget(ntmp, ip, attrtype, attrname, &fp);
	if (error) {
		printf("ntfs_vget: ntfs_fget failed\n");
		ntfs_ntput(ip);
		return (error);
	}

	f_type = VNON;
	if (!(flags & VG_DONTVALIDFN) && !(fp->f_flag & FN_VALID)) {
		if ((ip->i_frflag & NTFS_FRFLAG_DIR) &&
		    (fp->f_attrtype == NTFS_A_DATA && fp->f_attrname == NULL)) {
			f_type = VDIR;
		} else if (flags & VG_EXT) {
			f_type = VNON;
			fp->f_size = fp->f_allocated = 0;
		} else {
			f_type = VREG;	

			error = ntfs_filesize(ntmp, fp, 
					      &fp->f_size, &fp->f_allocated);
			if (error) {
				ntfs_ntput(ip);
				return (error);
			}
		}

		fp->f_flag |= FN_VALID;
	}

	if (FTOV(fp)) {
		VGET(FTOV(fp), lkflags, p);
		*vpp = FTOV(fp);
		ntfs_ntput(ip);
		return (0);
	}

	error = getnewvnode(VT_NTFS, ntmp->ntm_mountp, ntfs_vnodeop_p, &vp);
	if(error) {
		ntfs_frele(fp);
		ntfs_ntput(ip);
		return (error);
	}
	dprintf(("ntfs_vget: vnode: %p for ntnode: %d\n", vp,ino));

#ifdef __FreeBSD__
	lockinit(&fp->f_lock, PINOD, "fnode", 0, 0);
#endif
	fp->f_vp = vp;
	vp->v_data = fp;
	vp->v_type = f_type;

	if (ino == NTFS_ROOTINO)
		vp->v_flag |= VROOT;

	ntfs_ntput(ip);

	if (lkflags & LK_TYPE_MASK) {
		error = VN_LOCK(vp, lkflags, p);
		if (error) {
			vput(vp);
			return (error);
		}
	}

	VREF(ip->i_devvp);
	*vpp = vp;
	return (0);
	
}

static int
ntfs_vget(
	struct mount *mp,
	ino_t ino,
	struct vnode **vpp) 
{
	return ntfs_vgetex(mp, ino, NTFS_A_DATA, NULL,
			LK_EXCLUSIVE | LK_RETRY, 0, curproc, vpp);
}

#if defined(__FreeBSD__)
static struct vfsops ntfs_vfsops = {
	ntfs_mount,
	vfs_stdstart,
	ntfs_unmount,
	ntfs_root,
	vfs_stdquotactl,
	ntfs_statfs,
	vfs_stdsync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_checkexp,
	ntfs_vptofh,
	ntfs_init,
	ntfs_uninit,
	vfs_stdextattrctl,
};
VFS_SET(ntfs_vfsops, ntfs, 0);
#elif defined(__NetBSD__)
extern struct vnodeopv_desc ntfs_vnodeop_opv_desc;

struct vnodeopv_desc *ntfs_vnodeopv_descs[] = {
	&ntfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops ntfs_vfsops = {
	MOUNT_NTFS,
	ntfs_mount,
	ntfs_start,
	ntfs_unmount,
	ntfs_root,
	ntfs_quotactl,
	ntfs_statfs,
	ntfs_sync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_vptofh,
	ntfs_init,
	ntfs_sysctl,
	ntfs_mountroot,
	ntfs_checkexp,
	ntfs_vnodeopv_descs,
};
#else /* !NetBSD && !FreeBSD */
static struct vfsops ntfs_vfsops = {
	ntfs_mount,
	ntfs_start,
	ntfs_unmount,
	ntfs_root,
	ntfs_quotactl,
	ntfs_statfs,
	ntfs_sync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_vptofh,
	ntfs_init,
};
VFS_SET(ntfs_vfsops, ntfs, MOUNT_NTFS, 0);
#endif


