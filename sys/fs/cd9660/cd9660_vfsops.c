/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_vfsops.c	8.3 (Berkeley) 1/31/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>

extern int enodev ();

struct vfsops cd9660_vfsops = {
	cd9660_mount,
	cd9660_start,
	cd9660_unmount,
	cd9660_root,
	cd9660_quotactl,
	cd9660_statfs,
	cd9660_sync,
	cd9660_vget,
	cd9660_fhtovp,
	cd9660_vptofh,
	cd9660_init,
};

/*
 * Called by vfs_mountroot when iso is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

static iso_mountfs();

cd9660_mountroot()
{
	register struct mount *mp;
	extern struct vnode *rootvp;
	struct proc *p = curproc;	/* XXX */
	struct iso_mnt *imp;
	register struct fs *fs;
	u_int size;
	int error;
	struct iso_args args;
	
	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if (bdevvp(swapdev, &swapdev_vp) || bdevvp(rootdev, &rootvp))
		panic("cd9660_mountroot: can't setup bdevvp's");

	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &cd9660_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	args.flags = ISOFSMNT_ROOT;
	if (error = iso_mountfs(rootvp, mp, p, &args)) {
		free(mp, M_MOUNT);
		return (error);
	}
	if (error = vfs_lock(mp)) {
		(void)cd9660_unmount(mp, 0, p);
		free(mp, M_MOUNT);
		return (error);
	}
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_flag |= MNT_ROOTFS;
	mp->mnt_vnodecovered = NULLVP;
	imp = VFSTOISOFS(mp);
	bzero(imp->im_fsmnt, sizeof(imp->im_fsmnt));
	imp->im_fsmnt[0] = '/';
	bcopy((caddr_t)imp->im_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
	    MNAMELEN);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void) cd9660_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	return (0);
}

/*
 * Flag to allow forcible unmounting.
 */
int iso_doforce = 1;

/*
 * VFS Operations.
 *
 * mount system call
 */
cd9660_mount(mp, path, data, ndp, p)
	register struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct iso_args args;
	u_int size;
	int error;
	struct iso_mnt *imp;
	
	if (error = copyin(data, (caddr_t)&args, sizeof (struct iso_args)))
		return (error);
	
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EROFS);
	
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		imp = VFSTOISOFS(mp);
		if (args.fspec == 0)
			return (vfs_export(mp, &imp->im_export, &args.export));
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	if (error = namei(ndp))
		return (error);
	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return ENOTBLK;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return ENXIO;
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = iso_mountfs(devvp, mp, p, &args);
	else {
		if (devvp != imp->im_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return error;
	}
	imp = VFSTOISOFS(mp);
	(void) copyinstr(path, imp->im_fsmnt, sizeof(imp->im_fsmnt)-1, &size);
	bzero(imp->im_fsmnt + size, sizeof(imp->im_fsmnt) - size);
	bcopy((caddr_t)imp->im_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
	    MNAMELEN);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void) cd9660_statfs(mp, &mp->mnt_stat, p);
	return 0;
}

/*
 * Common code for mount and mountroot
 */
static iso_mountfs(devvp, mp, p, argp)
	register struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
	struct iso_args *argp;
{
	register struct iso_mnt *isomp = (struct iso_mnt *)0;
	struct buf *bp = NULL;
	dev_t dev = devvp->v_rdev;
	caddr_t base, space;
	int havepart = 0, blks;
	int error = EINVAL, i, size;
	int needclose = 0;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	extern struct vnode *rootvp;
	int j;
	int iso_bsize;
	int iso_blknum;
	struct iso_volume_descriptor *vdp;
	struct iso_primary_descriptor *pri;
	struct iso_directory_record *rootp;
	int logical_block_size;
	
	if (!ronly)
		return EROFS;
	
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if (error = vfs_mountedon(devvp))
		return error;
	if (vcount(devvp) > 1 && devvp != rootvp)
		return EBUSY;
	if (error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0))
		return (error);

	if (error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p))
		return error;
	needclose = 1;
	
	/* This is the "logical sector size".  The standard says this
	 * should be 2048 or the physical sector size on the device,
	 * whichever is greater.  For now, we'll just use a constant.
	 */
	iso_bsize = ISO_DEFAULT_BLOCK_SIZE;
	
	for (iso_blknum = 16; iso_blknum < 100; iso_blknum++) {
		if (error = bread (devvp, btodb(iso_blknum * iso_bsize),
				   iso_bsize, NOCRED, &bp))
			goto out;
		
		vdp = (struct iso_volume_descriptor *)bp->b_un.b_addr;
		if (bcmp (vdp->id, ISO_STANDARD_ID, sizeof vdp->id) != 0) {
			error = EINVAL;
			goto out;
		}
		
		if (isonum_711 (vdp->type) == ISO_VD_END) {
			error = EINVAL;
			goto out;
		}
		
		if (isonum_711 (vdp->type) == ISO_VD_PRIMARY)
			break;
		brelse(bp);
	}
	
	if (isonum_711 (vdp->type) != ISO_VD_PRIMARY) {
		error = EINVAL;
		goto out;
	}
	
	pri = (struct iso_primary_descriptor *)vdp;
	
	logical_block_size = isonum_723 (pri->logical_block_size);
	
	if (logical_block_size < DEV_BSIZE || logical_block_size > MAXBSIZE
	    || (logical_block_size & (logical_block_size - 1)) != 0) {
		error = EINVAL;
		goto out;
	}
	
	rootp = (struct iso_directory_record *)pri->root_directory_record;
	
	isomp = malloc(sizeof *isomp, M_ISOFSMNT, M_WAITOK);
	bzero((caddr_t)isomp, sizeof *isomp);
	isomp->logical_block_size = logical_block_size;
	isomp->volume_space_size = isonum_733 (pri->volume_space_size);
	bcopy (rootp, isomp->root, sizeof isomp->root);
	isomp->root_extent = isonum_733 (rootp->extent);
	isomp->root_size = isonum_733 (rootp->size);
	
	isomp->im_bmask = logical_block_size - 1;
	isomp->im_bshift = 0;
	while ((1 << isomp->im_bshift) < isomp->logical_block_size)
		isomp->im_bshift++;
	
	bp->b_flags |= B_AGE;
	brelse(bp);
	bp = NULL;
	
	mp->mnt_data = (qaddr_t)isomp;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_CD9660;
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	isomp->im_mountp = mp;
	isomp->im_dev = dev;
	isomp->im_devvp = devvp;
	
	devvp->v_specflags |= SI_MOUNTEDON;
	
	/* Check the Rock Ridge Extention support */
	if (!(argp->flags & ISOFSMNT_NORRIP)) {
		if (error = bread (isomp->im_devvp,
				   (isomp->root_extent + isonum_711(rootp->ext_attr_length))
				   * isomp->logical_block_size / DEV_BSIZE,
				   isomp->logical_block_size,NOCRED,&bp))
		    goto out;
		
		rootp = (struct iso_directory_record *)bp->b_un.b_addr;
		
		if ((isomp->rr_skip = cd9660_rrip_offset(rootp,isomp)) < 0) {
		    argp->flags  |= ISOFSMNT_NORRIP;
		} else {
		    argp->flags  &= ~ISOFSMNT_GENS;
		}
		
		/*
		 * The contents are valid,
		 * but they will get reread as part of another vnode, so...
		 */
		bp->b_flags |= B_AGE;
		brelse(bp);
		bp = NULL;
	}
	isomp->im_flags = argp->flags&(ISOFSMNT_NORRIP|ISOFSMNT_GENS|ISOFSMNT_EXTATT);
	switch (isomp->im_flags&(ISOFSMNT_NORRIP|ISOFSMNT_GENS)) {
	default:
	    isomp->iso_ftype = ISO_FTYPE_DEFAULT;
	    break;
	case ISOFSMNT_GENS|ISOFSMNT_NORRIP:
	    isomp->iso_ftype = ISO_FTYPE_9660;
	    break;
	case 0:
	    isomp->iso_ftype = ISO_FTYPE_RRIP;
	    break;
	}
	
	return 0;
out:
	if (bp)
		brelse(bp);
	if (needclose)
		(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	if (isomp) {
		free((caddr_t)isomp, M_ISOFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return error;
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
cd9660_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return 0;
}

/*
 * unmount system call
 */
int
cd9660_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct iso_mnt *isomp;
	int i, error, ronly, flags = 0;
	
	if (mntflags & MNT_FORCE) {
		if (!iso_doforce || (mp->mnt_flag & MNT_ROOTFS))
			return (EINVAL);
		flags |= FORCECLOSE;
	}
#if 0
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp))
		return EBUSY;
#endif
	if (error = vflush(mp, NULLVP, flags))
		return (error);

	isomp = VFSTOISOFS(mp);

#ifdef	ISODEVMAP
	if (isomp->iso_ftype == ISO_FTYPE_RRIP)
		iso_dunmap(isomp->im_dev);
#endif
	
	isomp->im_devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(isomp->im_devvp, FREAD, NOCRED, p);
	vrele(isomp->im_devvp);
	free((caddr_t)isomp, M_ISOFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Return root of a filesystem
 */
cd9660_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	register struct iso_node *ip;
	struct iso_node tip, *nip;
	struct vnode tvp;
	int error;
	struct iso_mnt *imp = VFSTOISOFS (mp);
	struct iso_directory_record *dp;
	
	tvp.v_mount = mp;
	tvp.v_data = &tip;
	ip = VTOI(&tvp);
	ip->i_vnode = &tvp;
	ip->i_dev = imp->im_dev;
	ip->i_diroff = 0;
	dp = (struct iso_directory_record *)imp->root;
	isodirino(&ip->i_number,dp,imp);
	
	/*
	 * With RRIP we must use the `.' entry of the root directory.
	 * Simply tell iget, that it's a relocated directory.
	 */
	error = iso_iget(ip,ip->i_number,
			 imp->iso_ftype == ISO_FTYPE_RRIP,
			 &nip,dp);
	if (error)
		return error;
	*vpp = ITOV(nip);
	return 0;
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
int
cd9660_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return (EOPNOTSUPP);
}

/*
 * Get file system statistics.
 */
cd9660_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	register struct iso_mnt *isomp;
	register struct fs *fs;
	
	isomp = VFSTOISOFS(mp);
	
	sbp->f_type = MOUNT_CD9660;
	sbp->f_bsize = isomp->logical_block_size;
	sbp->f_iosize = sbp->f_bsize;	/* XXX */
	sbp->f_blocks = isomp->volume_space_size;
	sbp->f_bfree = 0; /* total free blocks */
	sbp->f_bavail = 0; /* blocks free for non superuser */
	sbp->f_files =  0; /* total files */
	sbp->f_ffree = 0; /* free file nodes */
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	/* Use the first spare for flags: */
	sbp->f_spare[0] = isomp->im_flags;
	return 0;
}

/* ARGSUSED */
int
cd9660_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	return (0);
}

/*
 * Flat namespace lookup.
 * Currently unsupported.
 */
/* ARGSUSED */
int
cd9660_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */

struct ifid {
	ushort	ifid_len;
	ushort	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

/* ARGSUSED */
int
cd9660_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct vnode			tvp;
	int				error;
	int				lbn, off;
	struct ifid			*ifhp;
	struct iso_mnt			*imp;
	struct buf			*bp;
	struct iso_directory_record	*dirp;
	struct iso_node 		tip, *ip, *nip;
	struct netcred			*np;
	
	imp = VFSTOISOFS (mp);
	ifhp = (struct ifid *)fhp;
	
#ifdef	ISOFS_DBG
	printf("fhtovp: ino %d, start %ld\n",
	       ifhp->ifid_ino, ifhp->ifid_start);
#endif
	
	np = vfs_export_lookup(mp, &imp->im_export, nam);
	if (np == NULL)
		return (EACCES);

	lbn = iso_lblkno(imp, ifhp->ifid_ino);
	if (lbn >= imp->volume_space_size) {
		printf("fhtovp: lbn exceed volume space %d\n", lbn);
		return (ESTALE);
	}
	
	off = iso_blkoff(imp, ifhp->ifid_ino);
	if (off + ISO_DIRECTORY_RECORD_SIZE > imp->logical_block_size) {
		printf("fhtovp: crosses block boundary %d\n",
		       off + ISO_DIRECTORY_RECORD_SIZE);
		return (ESTALE);
	}
	
	error = bread(imp->im_devvp, btodb(lbn * imp->logical_block_size),
		      imp->logical_block_size, NOCRED, &bp);
	if (error) {
		printf("fhtovp: bread error %d\n",error);
		brelse(bp);
		return (error);
	}
	
	dirp = (struct iso_directory_record *)(bp->b_un.b_addr + off);
	if (off + isonum_711(dirp->length) > imp->logical_block_size) {
		brelse(bp);
		printf("fhtovp: directory crosses block boundary %d[off=%d/len=%d]\n",
		       off+isonum_711(dirp->length), off,
		       isonum_711(dirp->length));
		return (ESTALE);
	}
	
	if (isonum_733(dirp->extent) + isonum_711(dirp->ext_attr_length) !=
	    ifhp->ifid_start) {
		brelse(bp);
		printf("fhtovp: file start miss %d vs %d\n",
		       isonum_733(dirp->extent)+isonum_711(dirp->ext_attr_length),
		       ifhp->ifid_start);
		return (ESTALE);
	}
	brelse(bp);
	
	ip = &tip;
	tvp.v_mount = mp;
	tvp.v_data = ip;
	ip->i_vnode = &tvp;
	ip->i_dev = imp->im_dev;
	if (error = iso_iget(ip, ifhp->ifid_ino, 0, &nip, dirp)) {
		*vpp = NULLVP;
		printf("fhtovp: failed to get inode\n");
		return (error);
	}
	ip = nip;
	/*
	 * XXX need generation number?
	 */
	if (ip->inode.iso_mode == 0) {
		iso_iput(ip);
		*vpp = NULLVP;
		printf("fhtovp: inode mode == 0\n");
		return (ESTALE);
	}
	*vpp = ITOV(ip);
	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return 0;
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
cd9660_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	register struct iso_node *ip = VTOI(vp);
	register struct ifid *ifhp;
	register struct iso_mnt *mp = ip->i_mnt;
	
	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);
	
	ifhp->ifid_ino = ip->i_number;
	ifhp->ifid_start = ip->iso_start;
	
#ifdef	ISOFS_DBG
	printf("vptofh: ino %d, start %ld\n",
	       ifhp->ifid_ino,ifhp->ifid_start);
#endif
	return 0;
}
