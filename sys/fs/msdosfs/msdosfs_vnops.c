/*	$Id: msdosfs_vnops.c,v 1.46 1997/10/15 09:21:39 phk Exp $ */
/*	$NetBSD: msdosfs_vnops.c,v 1.20 1994/08/21 18:44:13 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <miscfs/specfs/specdev.h> /* XXX */	/* defines v_rdev */
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <msdosfs/bpb.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/fat.h>

/*
 * Prototypes for MSDOSFS vnode operations
 */
static int msdosfs_create __P((struct vop_create_args *));
static int msdosfs_mknod __P((struct vop_mknod_args *));
static int msdosfs_open __P((struct vop_open_args *));
static int msdosfs_close __P((struct vop_close_args *));
static int msdosfs_access __P((struct vop_access_args *));
static int msdosfs_getattr __P((struct vop_getattr_args *));
static int msdosfs_setattr __P((struct vop_setattr_args *));
static int msdosfs_read __P((struct vop_read_args *));
static int msdosfs_write __P((struct vop_write_args *));
static int msdosfs_ioctl __P((struct vop_ioctl_args *));
static int msdosfs_poll __P((struct vop_poll_args *));
static int msdosfs_mmap __P((struct vop_mmap_args *));
static int msdosfs_fsync __P((struct vop_fsync_args *));
static int msdosfs_seek __P((struct vop_seek_args *));
static int msdosfs_remove __P((struct vop_remove_args *));
static int msdosfs_link __P((struct vop_link_args *));
static int msdosfs_rename __P((struct vop_rename_args *));
static int msdosfs_mkdir __P((struct vop_mkdir_args *));
static int msdosfs_rmdir __P((struct vop_rmdir_args *));
static int msdosfs_symlink __P((struct vop_symlink_args *));
static int msdosfs_readdir __P((struct vop_readdir_args *));
static int msdosfs_readlink __P((struct vop_readlink_args *));
static int msdosfs_abortop __P((struct vop_abortop_args *));
static int msdosfs_lock __P((struct vop_lock_args *));
static int msdosfs_unlock __P((struct vop_unlock_args *));
static int msdosfs_bmap __P((struct vop_bmap_args *));
static int msdosfs_strategy __P((struct vop_strategy_args *));
static int msdosfs_print __P((struct vop_print_args *));
static int msdosfs_islocked __P((struct vop_islocked_args *));
static int msdosfs_advlock __P((struct vop_advlock_args *));
static int msdosfs_pathconf __P((struct vop_pathconf_args *ap));
static int msdosfs_reallocblks __P((struct vop_reallocblks_args *));

/*
 * Some general notes:
 *
 * In the ufs filesystem the inodes, superblocks, and indirect blocks are
 * read/written using the vnode for the filesystem. Blocks that represent
 * the contents of a file are read/written using the vnode for the file
 * (including directories when they are read/written as files). This
 * presents problems for the dos filesystem because data that should be in
 * an inode (if dos had them) resides in the directory itself.  Since we
 * must update directory entries without the benefit of having the vnode
 * for the directory we must use the vnode for the filesystem.  This means
 * that when a directory is actually read/written (via read, write, or
 * readdir, or seek) we must use the vnode for the filesystem instead of
 * the vnode for the directory as would happen in ufs. This is to insure we
 * retreive the correct block from the buffer cache since the hash value is
 * based upon the vnode address and the desired block number.
 */

/*
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. We must also free
 * the pathname buffer pointed at by cnp->cn_pnbuf, always on error, or
 * only if the SAVESTART bit in cn_flags is clear on success.
 */
static int
msdosfs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct denode ndirent;
	struct denode *dep;
	struct denode *pdep = VTODE(ap->a_dvp);
	struct timespec ts;
	int error;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_create(cnp %08x, vap %08x\n", cnp, ap->a_vap);
#endif

	/*
	 * Create a directory entry for the file, then call createde() to
	 * have it installed. NOTE: DOS files are always executable.  We
	 * use the absence of the owner write bit to make the file
	 * readonly.
	 */
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & SAVENAME) == 0)
		panic("msdosfs_create: no name");
#endif
	bzero(&ndirent, sizeof(ndirent));
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	unix2dostime(&ts, &ndirent.de_Date, &ndirent.de_Time);
	unix2dosfn((u_char *)cnp->cn_nameptr, ndirent.de_Name, cnp->cn_namelen);
	ndirent.de_Attributes = (ap->a_vap->va_mode & VWRITE)
				? ATTR_ARCHIVE : ATTR_ARCHIVE | ATTR_READONLY;
	ndirent.de_StartCluster = 0;
	ndirent.de_FileSize = 0;
	ndirent.de_dev = pdep->de_dev;
	ndirent.de_devvp = pdep->de_devvp;
	if ((error = createde(&ndirent, pdep, &dep)) == 0) {
		*ap->a_vpp = DETOV(dep);
		if ((cnp->cn_flags & SAVESTART) == 0)
			zfree(namei_zone, cnp->cn_pnbuf);
	} else {
		zfree(namei_zone, cnp->cn_pnbuf);
	}
	vput(ap->a_dvp);		/* release parent dir */
	return error;
}

static int
msdosfs_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int error;

	switch (ap->a_vap->va_type) {
	case VDIR:
		error = msdosfs_mkdir((struct vop_mkdir_args *)ap);
		break;

	case VREG:
		error = msdosfs_create((struct vop_create_args *)ap);
		break;

	default:
		error = EINVAL;
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
		vput(ap->a_dvp);
		break;
	}
	return error;
}

static int
msdosfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return 0;
}

static int
msdosfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);

	simple_lock(&vp->v_interlock);
	if (vp->v_usecount > 1)
		DE_TIMES(dep, &time);
	simple_unlock(&vp->v_interlock);
	return 0;
}

static int
msdosfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct ucred *cred = ap->a_cred;
	mode_t mask, file_mode, mode = ap->a_mode;
	register gid_t *gp;
	int i;

	file_mode = (S_IXUSR|S_IXGRP|S_IXOTH) | (S_IRUSR|S_IRGRP|S_IROTH) |
	    ((dep->de_Attributes & ATTR_READONLY) ? 0 : (S_IWUSR|S_IWGRP|S_IWOTH));
	file_mode &= pmp->pm_mask;

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		}
	}

	/* User id 0 always gets access. */
	if (cred->cr_uid == 0)
		return 0;

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == pmp->pm_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return (file_mode & mask) == mask ? 0 : EACCES;
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (pmp->pm_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return (file_mode & mask) == mask ? 0 : EACCES;
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return (file_mode & mask) == mask ? 0 : EACCES;
}

static int
msdosfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	u_int cn;
	struct denode *dep = VTODE(ap->a_vp);
	struct vattr *vap = ap->a_vap;

	DE_TIMES(dep, &time);
	vap->va_fsid = dep->de_dev;
	/*
	 * The following computation of the fileid must be the same as that
	 * used in msdosfs_readdir() to compute d_fileno. If not, pwd
	 * doesn't work.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY) {
		if ((cn = dep->de_StartCluster) == MSDOSFSROOT)
			cn = 1;
	} else {
		if ((cn = dep->de_dirclust) == MSDOSFSROOT)
			cn = 1;
		cn = (cn << 16) | (dep->de_diroffset & 0xffff);
	}
	vap->va_fileid = cn;
	vap->va_mode = (S_IXUSR|S_IXGRP|S_IXOTH) | (S_IRUSR|S_IRGRP|S_IROTH) |
		((dep->de_Attributes & ATTR_READONLY) ? 0 : (S_IWUSR|S_IWGRP|S_IWOTH));
	vap->va_mode &= dep->de_pmp->pm_mask;
	if (dep->de_Attributes & ATTR_DIRECTORY)
		vap->va_mode |= S_IFDIR;
	vap->va_nlink = 1;
	vap->va_gid = dep->de_pmp->pm_gid;
	vap->va_uid = dep->de_pmp->pm_uid;
	vap->va_rdev = 0;
	vap->va_size = dep->de_FileSize;
	dos2unixtime(dep->de_Date, dep->de_Time, &vap->va_atime);
	vap->va_mtime = vap->va_atime;
#if 0
#ifndef MSDOSFS_NODIRMOD
	if (vap->va_mode & S_IFDIR)
		TIMEVAL_TO_TIMESPEC(&time, &vap->va_mtime);
#endif
#endif
	vap->va_ctime = vap->va_atime;
	vap->va_flags = (dep->de_Attributes & ATTR_ARCHIVE) ? 0 : SF_ARCHIVED;
	vap->va_gen = 0;
	vap->va_blocksize = dep->de_pmp->pm_bpcluster;
	vap->va_bytes = (dep->de_FileSize + dep->de_pmp->pm_crbomask) &
	    			~(dep->de_pmp->pm_crbomask);
	vap->va_type = ap->a_vp->v_type;
	vap->va_filerev = dep->de_modrev;
	return 0;
}

static int
msdosfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(ap->a_vp);
	struct vattr *vap = ap->a_vap;
	struct ucred *cred = ap->a_cred;
	int error = 0;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    (vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != dep->de_pmp->pm_uid &&
		    (error = suser(cred, &ap->a_p->p_acflag)))
			return (error);
		/*
		 * We are very inconsistent about handling unsupported
		 * attributes.  We ignored the the access time and the
		 * read and execute bits.  We were strict for the other
		 * attributes.
		 *
		 * Here we are strict, stricter than ufs in not allowing
		 * users to attempt to set SF_SETTABLE bits or anyone to
		 * set unsupported bits.  However, we ignore attempts to
		 * set ATTR_ARCHIVE for directories `cp -pr' from a more
		 * sensible file system attempts it a lot.
		 */
		if (cred->cr_uid != 0) {
			if (vap->va_flags & SF_SETTABLE)
				return EPERM;
		}
		if (vap->va_flags & ~SF_ARCHIVED)
			return EINVAL;
		if (vap->va_flags & SF_ARCHIVED)
			dep->de_Attributes &= ~ATTR_ARCHIVE;
		else if (!(dep->de_Attributes & ATTR_DIRECTORY))
			dep->de_Attributes |= ATTR_ARCHIVE;
		dep->de_flag |= DE_MODIFIED;
	}

	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (uid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if ((cred->cr_uid != dep->de_pmp->pm_uid ||
		     vap->va_uid != dep->de_pmp->pm_uid ||
		     (vap->va_gid != dep->de_pmp->pm_gid &&
		      !groupmember(vap->va_gid, cred))) &&
		    (error = suser(cred, &ap->a_p->p_acflag)))
			return error;
		if (vap->va_uid != dep->de_pmp->pm_uid ||
		    vap->va_gid != dep->de_pmp->pm_gid)
			return EINVAL;
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		}
		error = detrunc(dep, vap->va_size, 0, cred, ap->a_p);
		if (error)
			return error;
	}
	if (vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != dep->de_pmp->pm_uid &&
		    (error = suser(cred, &ap->a_p->p_acflag)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 ||
		    (error = VOP_ACCESS(vp, VWRITE, cred, ap->a_p))))
			return error;
		dep->de_flag |= DE_UPDATE;
		error = deupdat(dep, &vap->va_mtime, 1);
		if (error)
			return error;
	}

	/*
	 * DOS files only have the ability to have their writability
	 * attribute set, so we use the owner write bit to set the readonly
	 * attribute.
	 */
	error = 0;
	if (vap->va_mode != (u_short) VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != dep->de_pmp->pm_uid &&
		    (error = suser(cred, &ap->a_p->p_acflag)))
			return error;

		/* We ignore the read and execute bits */
		if (vap->va_mode & VWRITE)
			dep->de_Attributes &= ~ATTR_READONLY;
		else
			dep->de_Attributes |= ATTR_READONLY;
		dep->de_flag |= DE_MODIFIED;
	}
	return error;
}

static int
msdosfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = 0;
	int diff;
	int isadir;
	long n;
	long on;
	daddr_t lbn;
	daddr_t rablock;
	int rasize;
	struct buf *bp;
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct uio *uio = ap->a_uio;

	/*
	 * If they didn't ask for any data, then we are done.
	 */
	if (uio->uio_resid == 0)
		return 0;
	if (uio->uio_offset < 0)
		return EINVAL;

	isadir = dep->de_Attributes & ATTR_DIRECTORY;
	do {
		lbn = uio->uio_offset >> pmp->pm_cnshift;
		on = uio->uio_offset & pmp->pm_crbomask;
		n = min((u_long) (pmp->pm_bpcluster - on), uio->uio_resid);
		diff = dep->de_FileSize - uio->uio_offset;
		if (diff <= 0)
			return 0;
		/* convert cluster # to block # if a directory */
		if (isadir) {
			error = pcbmap(dep, lbn, &lbn, 0);
			if (error)
				return error;
		}
		if (diff < n)
			n = diff;
		/*
		 * If we are operating on a directory file then be sure to
		 * do i/o with the vnode for the filesystem instead of the
		 * vnode for the directory.
		 */
		if (isadir) {
			error = bread(pmp->pm_devvp, lbn, pmp->pm_bpcluster,
			    NOCRED, &bp);
		} else {
			rablock = lbn + 1;
#ifdef	PC98
			/*
			 * 1024byte/sector support
			 */
			if (pmp->pm_BytesPerSec == 1024)
					vp->v_flag |= 0x10000;
#endif
			if (vp->v_lastr + 1 == lbn &&
			    rablock * pmp->pm_bpcluster < dep->de_FileSize) {
				rasize = pmp->pm_bpcluster;
				error = breadn(vp, lbn, pmp->pm_bpcluster,
					       &rablock, &rasize, 1,
					       NOCRED, &bp);
			} else {
				error = bread(vp, lbn, pmp->pm_bpcluster, NOCRED,
					      &bp);
			}
			vp->v_lastr = lbn;
		}
		n = min(n, pmp->pm_bpcluster - bp->b_resid);
		if (error) {
			brelse(bp);
			return error;
		}
		error = uiomove(bp->b_data + on, (int) n, uio);
		/*
		 * If we have read everything from this block or have read
		 * to end of file then we are done with this block.  Mark
		 * it to say the buffer can be reused if need be.
		 */
#if 0
		if (n + on == pmp->pm_bpcluster ||
		    uio->uio_offset == dep->de_FileSize)
			bp->b_flags |= B_AGE;
#endif
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return error;
}

/*
 * Write data to a file or directory.
 */
static int
msdosfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int n;
	int isadir;
	int croffset;
	int resid;
	int osize;
	int error = 0;
	u_long count;
	daddr_t bn, lastcn;
	struct buf *bp;
	int ioflag = ap->a_ioflag;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct vnode *vp = ap->a_vp;
	struct vnode *thisvp;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct ucred *cred = ap->a_cred;
	struct timespec ts;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_write(vp %08x, uio %08x, ioflag %08x, cred %08x\n",
	       vp, uio, ioflag, cred);
	printf("msdosfs_write(): diroff %d, dirclust %d, startcluster %d\n",
	       dep->de_diroffset, dep->de_dirclust, dep->de_StartCluster);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = dep->de_FileSize;
		isadir = 0;
		thisvp = vp;
		break;

	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("msdosfs_write(): non-sync directory update");
		isadir = 1;
		thisvp = pmp->pm_devvp;
		break;

	default:
		panic("msdosfs_write(): bad file type");
		break;
	}

	if (uio->uio_offset < 0)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	/*
	 * If they've exceeded their filesize limit, tell them about it.
	 */
	if (vp->v_type == VREG && p &&
	    ((uio->uio_offset + uio->uio_resid) >
		p->p_rlimit[RLIMIT_FSIZE].rlim_cur)) {
		psignal(p, SIGXFSZ);
		return EFBIG;
	}

	/*
	 * If attempting to write beyond the end of the root directory we
	 * stop that here because the root directory can not grow.
	 */
	if ((dep->de_Attributes & ATTR_DIRECTORY) &&
	    dep->de_StartCluster == MSDOSFSROOT &&
	    (uio->uio_offset + uio->uio_resid) > dep->de_FileSize)
		return ENOSPC;

	/*
	 * If the offset we are starting the write at is beyond the end of
	 * the file, then they've done a seek.  Unix filesystems allow
	 * files with holes in them, DOS doesn't so we must fill the hole
	 * with zeroed blocks.
	 */
	if (uio->uio_offset > dep->de_FileSize) {
		error = deextend(dep, uio->uio_offset, cred);
		if (error)
			return error;
	}

	/*
	 * Remember some values in case the write fails.
	 */
	resid = uio->uio_resid;
	osize = dep->de_FileSize;


#ifdef	PC98
	/*
	 * 1024byte/sector support
	 */
	if (pmp->pm_BytesPerSec == 1024)
		thisvp->v_flag |= 0x10000;
#endif
	/*
	 * If we write beyond the end of the file, extend it to its ultimate
	 * size ahead of the time to hopefully get a contiguous area.
	 */
	if (uio->uio_offset + resid > osize) {
		count = de_clcount(pmp, uio->uio_offset + resid) - de_clcount(pmp, osize);
		if ((error = extendfile(dep, count, NULL, NULL, 0))
		    && (error != ENOSPC || (ioflag & IO_UNIT)))
			goto errexit;
		lastcn = dep->de_fc[FC_LASTFC].fc_frcn;
	} else
		lastcn = de_clcount(pmp, osize) - 1;

	do {
		bn = de_blk(pmp, uio->uio_offset);
		if (isadir) {
			error = pcbmap(dep, bn, &bn, 0);
			if (error)
				break;
		} else if (bn > lastcn) {
			error = ENOSPC;
			break;
		}

		croffset = uio->uio_offset & pmp->pm_crbomask;
		n = min(uio->uio_resid, pmp->pm_bpcluster - croffset);
		if (uio->uio_offset + n > dep->de_FileSize) {
			dep->de_FileSize = uio->uio_offset + n;
			/* The object size needs to be set before buffer is allocated */
			vnode_pager_setsize(vp, dep->de_FileSize);
		}

		if ((uio->uio_offset & pmp->pm_crbomask) == 0
		    && (de_blk(pmp, uio->uio_offset + uio->uio_resid) > de_blk(pmp, uio->uio_offset)
			|| uio->uio_offset + uio->uio_resid >= dep->de_FileSize)) {
			/*
			 * If either the whole cluster gets written,
			 * or we write the cluster from its start beyond EOF,
			 * then no need to read data from disk.
			 */
			bp = getblk(thisvp, bn, pmp->pm_bpcluster, 0, 0);
			clrbuf(bp);
			/*
			 * Do the bmap now, since pcbmap needs buffers
			 * for the fat table. (see msdosfs_strategy)
			 */
			if (!isadir) {
				if (bp->b_blkno == bp->b_lblkno) {
					error = pcbmap(dep, bp->b_lblkno,
							   &bp->b_blkno, 0);
					if (error)
						bp->b_blkno = -1;
				}
				if (bp->b_blkno == -1) {
					brelse(bp);
					if (!error)
						error = EIO;		/* XXX */
					break;
				}
			}
		} else {
			/*
			 * The block we need to write into exists, so read it in.
			 */
			error = bread(thisvp, bn, pmp->pm_bpcluster, cred, &bp);
			if (error)
				break;
		}

		/*
		 * Should these vnode_pager_* functions be done on dir
		 * files?
		 */

		/*
		 * Copy the data from user space into the buf header.
		 */
		error = uiomove(bp->b_data + croffset, n, uio);

		/*
		 * If they want this synchronous then write it and wait for
		 * it.  Otherwise, if on a cluster boundary write it
		 * asynchronously so we can move on to the next block
		 * without delay.  Otherwise do a delayed write because we
		 * may want to write somemore into the block later.
		 */
		if (ioflag & IO_SYNC)
			(void) bwrite(bp);
		else if (n + croffset == pmp->pm_bpcluster) {
			bawrite(bp);
		} else
			bdwrite(bp);
		dep->de_flag |= DE_UPDATE;
	} while (error == 0 && uio->uio_resid > 0);

	/*
	 * If the write failed and they want us to, truncate the file back
	 * to the size it was before the write was attempted.
	 */
errexit:
	if (error) {
		if (ioflag & IO_UNIT) {
			detrunc(dep, osize, ioflag & IO_SYNC, NOCRED, NULL);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		} else {
			detrunc(dep, dep->de_FileSize, ioflag & IO_SYNC, NOCRED, NULL);
			if (uio->uio_resid != resid)
				error = 0;
		}
	} else if (ioflag & IO_SYNC) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		error = deupdat(dep, &ts, 1);
	}
	return error;
}

static int
msdosfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int a_command;
		caddr_t a_data;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return ENOTTY;
}

static int
msdosfs_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	/* DOS filesystems never block? */
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

static int
msdosfs_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return EINVAL;
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
msdosfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	int wait = ap->a_waitfor == MNT_WAIT;
	struct timespec ts;
	struct buf *nbp;
	int s;

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("msdosfs_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		(void) bwrite(bp);
		goto loop;
	}
	while (vp->v_numoutput) {
		vp->v_flag |= VBWAIT;
		(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "msdosfsn", 0);
	}
#ifdef DIAGNOSTIC
	if (vp->v_dirtyblkhd.lh_first) {
		vprint("msdosfs_fsync: dirty", vp);
		goto loop;
	}
#endif
	splx(s);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	return deupdat(VTODE(vp), &ts, wait);
}

/*
 * Now the whole work of extending a file is done in the write function.
 * So nothing to do here.
 */
static int
msdosfs_seek(ap)
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		struct ucred *a_cred;
	} */ *ap;
{
	return 0;
}

static int
msdosfs_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	struct denode *dep = VTODE(ap->a_vp);
	struct denode *ddep = VTODE(ap->a_dvp);

	error = removede(ddep,dep);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_remove(), dep %08x, v_usecount %d\n", dep, ap->a_vp->v_usecount);
#endif
	if (ddep == dep)
		vrele(ap->a_vp);
	else
		vput(ap->a_vp);	/* causes msdosfs_inactive() to be called
				 * via vrele() */
	vput(ap->a_dvp);
	return error;
}

/*
 * DOS filesystems don't know what links are. But since we already called
 * msdosfs_lookup() with create and lockparent, the parent is locked so we
 * have to free it before we return the error.
 */
static int
msdosfs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	VOP_ABORTOP(ap->a_tdvp, ap->a_cnp);
	vput(ap->a_tdvp);
	return EOPNOTSUPP;
}

/*
 * Renames on files require moving the denode to a new hash queue since the
 * denode's location is used to compute which hash queue to put the file
 * in. Unless it is a rename in place.  For example "mv a b".
 *
 * What follows is the basic algorithm:
 *
 * if (file move) {
 *	if (dest file exists) {
 *		remove dest file
 *	}
 *	if (dest and src in same directory) {
 *		rewrite name in existing directory slot
 *	} else {
 *		write new entry in dest directory
 *		update offset and dirclust in denode
 *		move denode to new hash chain
 *		clear old directory entry
 *	}
 * } else {
 *	directory move
 *	if (dest directory exists) {
 *		if (dest is not empty) {
 *			return ENOTEMPTY
 *		}
 *		remove dest directory
 *	}
 *	if (dest and src in same directory) {
 *		rewrite name in existing entry
 *	} else {
 *		be sure dest is not a child of src directory
 *		write entry in dest directory
 *		update "." and ".." in moved directory
 *		clear old directory entry for moved directory
 *	}
 * }
 *
 * On entry:
 *	source's parent directory is unlocked
 *	source file or directory is unlocked
 *	destination's parent directory is locked
 *	destination file or directory is locked if it exists
 *
 * On exit:
 *	all denodes should be released
 *
 * Notes:
 * I'm not sure how the memory containing the pathnames pointed at by the
 * componentname structures is freed, there may be some memory bleeding
 * for each rename done.
 */
static int
msdosfs_rename(ap)
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	u_char toname[11];
	int error;
	int newparent = 0;
	int sourceisadirectory = 0;
	u_long cn;
	daddr_t bn;
	struct vnode *tvp = ap->a_tvp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = fcnp->cn_proc;
	struct denode *fddep;	/* from file's parent directory	 */
	struct denode *fdep;	/* from file or directory	 */
	struct denode *tddep;	/* to file's parent directory	 */
	struct denode *tdep;	/* to file or directory		 */
	struct msdosfsmount *pmp;
	struct direntry *dotdotp;
	struct direntry *ep;
	struct buf *bp;

	fddep = VTODE(ap->a_fdvp);
	fdep = VTODE(ap->a_fvp);
	tddep = VTODE(ap->a_tdvp);
	tdep = tvp ? VTODE(tvp) : NULL;
	pmp = fddep->de_pmp;

	/* Check for cross-device rename */
	if ((ap->a_fvp->v_mount != ap->a_tdvp->v_mount) ||
	    (tvp && (ap->a_fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto bad;
	}

	/*
	 * Convert the filename in tcnp into a dos filename. We copy this
	 * into the denode and directory entry for the destination
	 * file/directory.
	 */
	unix2dosfn((u_char *) ap->a_tcnp->cn_nameptr,
		   toname, ap->a_tcnp->cn_namelen);

	/*
	 * At this point this is the lock state of the denodes:
	 *	fddep referenced
	 *	fdep  referenced
	 *	tddep locked
	 *	tdep  locked if it exists
	 */

	/*
	 * Be sure we are not renaming ".", "..", or an alias of ".". This
	 * leads to a crippled directory tree.  It's pretty tough to do a
	 * "ls" or "pwd" with the "." directory entry missing, and "cd .."
	 * doesn't work if the ".." entry is missing.
	 */
	if (fdep->de_Attributes & ATTR_DIRECTORY) {
		if ((ap->a_fcnp->cn_namelen == 1
		     && ap->a_fcnp->cn_nameptr[0] == '.')
		    || fddep == fdep
		    || (ap->a_fcnp->cn_flags | ap->a_tcnp->cn_flags)
		       & ISDOTDOT) {
			VOP_ABORTOP(ap->a_tdvp, ap->a_tcnp);
			vput(ap->a_tdvp);
			if (tvp)
				vput(tvp);
			VOP_ABORTOP(ap->a_fdvp, ap->a_fcnp);
			vrele(ap->a_fdvp);
			vrele(ap->a_fvp);
			return EINVAL;
		}
		sourceisadirectory = 1;
	}

	/*
	 * If we are renaming a directory, and the directory is being moved
	 * to another directory, then we must be sure the destination
	 * directory is not in the subtree of the source directory.  This
	 * could orphan everything under the source directory.
	 * doscheckpath() unlocks the destination's parent directory so we
	 * must look it up again to relock it.
	 */
	if (fddep->de_StartCluster != tddep->de_StartCluster)
		newparent = 1;
	if (sourceisadirectory && newparent) {
		if (tdep) {
			vput(ap->a_tvp);
			tdep = NULL;
		}
		/* doscheckpath() vput()'s tddep */
		error = doscheckpath(fdep, tddep);
		tddep = NULL;
		if (error)
			goto bad;
		if ((ap->a_tcnp->cn_flags & SAVESTART) == 0)
			panic("msdosfs_rename(): lost to startdir");
		error = relookup(ap->a_tdvp, &tvp, ap->a_tcnp);
		if (error)
			goto bad;
		tddep = VTODE(ap->a_tdvp);
		tdep = tvp ? VTODE(tvp) : NULL;
	}

	/*
	 * If the destination exists, then be sure its type (file or dir)
	 * matches that of the source.  And, if it is a directory make sure
	 * it is empty.  Then delete the destination.
	 */
	if (tdep) {
		if (tdep->de_Attributes & ATTR_DIRECTORY) {
			if (!sourceisadirectory) {
				error = ENOTDIR;
				goto bad;
			}
			if (!dosdirempty(tdep)) {
				error = ENOTEMPTY;
				goto bad;
			}
			cache_purge(DETOV(tddep));
		} else {		/* destination is file */
			if (sourceisadirectory) {
				error = EISDIR;
				goto bad;
			}
		}
		error = removede(tddep,tdep);
		if (error)
			goto bad;
		vput(ap->a_tvp);
		tdep = NULL;
	}

	/*
	 * If the source and destination are in the same directory then
	 * just read in the directory entry, change the name in the
	 * directory entry and write it back to disk.
	 */
	if (newparent == 0) {
		/* tddep and fddep point to the same denode here */
		vn_lock(ap->a_fvp, LK_EXCLUSIVE, p);	/* ap->a_fdvp is already locked */
		error = readep(fddep->de_pmp, fdep->de_dirclust,
				   fdep->de_diroffset, &bp, &ep);
		if (error) {
			VOP_UNLOCK(ap->a_fvp, 0, p);
			goto bad;
		}
		bcopy(toname, ep->deName, 11);
		error = bwrite(bp);
		if (error) {
			VOP_UNLOCK(ap->a_fvp, 0, p);
			goto bad;
		}
		bcopy(toname, fdep->de_Name, 11);	/* update denode */
		/*
		 * fdep locked fddep and tddep point to the same denode
		 * which is locked tdep is NULL
		 */
	} else {
		u_long dirsize = 0L;

		/*
		 * If the source and destination are in different
		 * directories, then mark the entry in the source directory
		 * as deleted and write a new entry in the destination
		 * directory.  Then move the denode to the correct hash
		 * chain for its new location in the filesystem.  And, if
		 * we moved a directory, then update its .. entry to point
		 * to the new parent directory. If we moved a directory
		 * will also insure that the directory entry on disk has a
		 * filesize of zero.
		 */
		vn_lock(ap->a_fvp, LK_EXCLUSIVE, p);
		bcopy(toname, fdep->de_Name, 11);	/* update denode */
		if (fdep->de_Attributes & ATTR_DIRECTORY) {
			dirsize = fdep->de_FileSize;
			fdep->de_FileSize = 0;
		}
		error = createde(fdep, tddep, (struct denode **) 0);
		if (fdep->de_Attributes & ATTR_DIRECTORY) {
			fdep->de_FileSize = dirsize;
		}
		if (error) {
			/* should put back filename */
			VOP_UNLOCK(ap->a_fvp, 0, p);
			goto bad;
		}
		vn_lock(ap->a_fdvp, LK_EXCLUSIVE, p);
		error = readep(fddep->de_pmp, fddep->de_fndclust,
				   fddep->de_fndoffset, &bp, &ep);
		if (error) {
			VOP_UNLOCK(ap->a_fvp, 0, p);
			VOP_UNLOCK(ap->a_fdvp, 0, p);
			goto bad;
		}
		ep->deName[0] = SLOT_DELETED;
		error = bwrite(bp);
		if (error) {
			VOP_UNLOCK(ap->a_fvp, 0, p);
			VOP_UNLOCK(ap->a_fdvp, 0, p);
			goto bad;
		}
		if (!sourceisadirectory) {
			fdep->de_dirclust = tddep->de_fndclust;
			fdep->de_diroffset = tddep->de_fndoffset;
			reinsert(fdep);
		}
		VOP_UNLOCK(ap->a_fdvp, 0, p);
	}
	/* fdep is still locked here */

	/*
	 * If we moved a directory to a new parent directory, then we must
	 * fixup the ".." entry in the moved directory.
	 */
	if (sourceisadirectory && newparent) {
		cn = fdep->de_StartCluster;
		if (cn == MSDOSFSROOT) {
			/* this should never happen */
			panic("msdosfs_rename(): updating .. in root directory?");
		} else {
			bn = cntobn(pmp, cn);
		}
		error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
			      NOCRED, &bp);
		if (error) {
			/* should really panic here, fs is corrupt */
			VOP_UNLOCK(ap->a_fvp, 0, p);
			goto bad;
		}
		dotdotp = (struct direntry *) bp->b_data + 1;
		putushort(dotdotp->deStartCluster, tddep->de_StartCluster);
		error = bwrite(bp);
		VOP_UNLOCK(ap->a_fvp, 0, p);
		if (error) {
			/* should really panic here, fs is corrupt */
			goto bad;
		}
	} else
		VOP_UNLOCK(ap->a_fvp, 0, p);
bad:	;
	vrele(DETOV(fdep));
	vrele(DETOV(fddep));
	if (tdep)
		vput(DETOV(tdep));
	if (tddep)
		vput(DETOV(tddep));
	return error;
}

static struct {
	struct direntry dot;
	struct direntry dotdot;
}      dosdirtemplate = {
    {
	".       ", "   ",		/* the . entry */
	ATTR_DIRECTORY,			/* file attribute */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* resevered */
	{210, 4}, {210, 4},		/* time and date */
	{0, 0},				/* startcluster */
	{0, 0, 0, 0},			/* filesize */
    },{
	"..      ", "   ",		/* the .. entry */
	ATTR_DIRECTORY,			/* file attribute */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* resevered */
	{210, 4}, {210, 4},		/* time and date */
	{0, 0},				/* startcluster */
	{0, 0, 0, 0},			/* filesize */
    }
};

static int
msdosfs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struvt vnode **a_vpp;
		struvt componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int bn;
	int error;
	u_long newcluster;
	struct denode *pdep;
	struct denode *ndep;
	struct direntry *denp;
	struct denode ndirent;
	struct msdosfsmount *pmp;
	struct buf *bp;
	struct timespec ts;
	u_short dDate, dTime;

	pdep = VTODE(ap->a_dvp);

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.
	 */
	if (pdep->de_StartCluster == MSDOSFSROOT && pdep->de_fndclust == (u_long)-1) {
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
		vput(ap->a_dvp);
		return ENOSPC;
	}

	pmp = pdep->de_pmp;

	/*
	 * Allocate a cluster to hold the about to be created directory.
	 */
	error = clusteralloc(pmp, 0, 1, CLUST_EOFE, &newcluster, NULL);
	if (error) {
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
		vput(ap->a_dvp);
		return error;
	}

	/*
	 * Now fill the cluster with the "." and ".." entries. And write
	 * the cluster to disk.  This way it is there for the parent
	 * directory to be pointing at if there were a crash.
	 */
	bn = cntobn(pmp, newcluster);
	/* always succeeds */
	bp = getblk(pmp->pm_devvp, bn, pmp->pm_bpcluster, 0, 0);
	bzero(bp->b_data, pmp->pm_bpcluster);
	bcopy(&dosdirtemplate, bp->b_data, sizeof dosdirtemplate);
	denp = (struct direntry *) bp->b_data;
	putushort(denp->deStartCluster, newcluster);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	unix2dostime(&ts, &dDate, &dTime);
	putushort(denp->deDate, dDate);
	putushort(denp->deTime, dTime);
	denp++;
	putushort(denp->deStartCluster, pdep->de_StartCluster);
	putushort(denp->deDate, dDate);
	putushort(denp->deTime, dTime);
	error = bwrite(bp);
	if (error) {
		clusterfree(pmp, newcluster, NULL);
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
		vput(ap->a_dvp);
		return error;
	}

	/*
	 * Now build up a directory entry pointing to the newly allocated
	 * cluster.  This will be written to an empty slot in the parent
	 * directory.
	 */
	ndep = &ndirent;
	bzero(ndep, sizeof(*ndep));
	unix2dosfn((u_char *)ap->a_cnp->cn_nameptr,
		   ndep->de_Name, ap->a_cnp->cn_namelen);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	unix2dostime(&ts, &ndep->de_Date, &ndep->de_Time);
	ndep->de_StartCluster = newcluster;
	ndep->de_Attributes = ATTR_DIRECTORY;

	error = createde(ndep, pdep, &ndep);
	if (error) {
		clusterfree(pmp, newcluster, NULL);
	} else {
		*ap->a_vpp = DETOV(ndep);
	}
	zfree(namei_zone, ap->a_cnp->cn_pnbuf);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_mkdir(): vput(%08x)\n", ap->a_dvp);
#endif
	vput(ap->a_dvp);
	return error;
}

static int
msdosfs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct denode *ddep;
	struct denode *dep;
	int error = 0;

	ddep = VTODE(ap->a_dvp);	/* parent dir of dir to delete	 */
	dep = VTODE(ap->a_vp);/* directory to delete	 */

	/*
	 * Be sure the directory being deleted is empty.
	 */
	if (dosdirempty(dep) == 0) {
		error = ENOTEMPTY;
		goto out;
	}

	/*
	 * Delete the entry from the directory.  For dos filesystems this
	 * gets rid of the directory entry on disk, the in memory copy
	 * still exists but the de_refcnt is <= 0.  This prevents it from
	 * being found by deget().  When the vput() on dep is done we give
	 * up access and eventually msdosfs_reclaim() will be called which
	 * will remove it from the denode cache.
	 */
	error = removede(ddep,dep);
	if (error)
		goto out;

	/*
	 * This is where we decrement the link count in the parent
	 * directory.  Since dos filesystems don't do this we just purge
	 * the name cache and let go of the parent directory denode.
	 */
	cache_purge(DETOV(ddep));
	vput(ap->a_dvp);
	ap->a_dvp = NULL;

	/*
	 * Truncate the directory that is being deleted.
	 */
	error = detrunc(dep, (u_long) 0, IO_SYNC, NOCRED, NULL);
	cache_purge(DETOV(dep));

out:	;
	if (ap->a_dvp)
		vput(ap->a_dvp);
	vput(ap->a_vp);
	return error;
}

/*
 * DOS filesystems don't know what symlinks are.
 */
static int
msdosfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	zfree(namei_zone, ap->a_cnp->cn_pnbuf);
	vput(ap->a_dvp);
	return EINVAL;
}

/*
 * Dummy dirents to simulate the "." and ".." entries of the root directory
 * in a dos filesystem.  Dos doesn't provide these. Note that each entry
 * must be the same size as a dos directory entry (32 bytes).
 */
static struct dos_dirent {
	u_long d_fileno;
	u_short d_reclen;
	u_char d_type;
	u_char d_namlen;
	u_char d_name[24];
}          rootdots[2] = {

	{
		1,		/* d_fileno			 */
		sizeof(struct direntry),	/* d_reclen			 */
		DT_DIR,		/* d_type			 */
		1,		/* d_namlen			 */
		"."		/* d_name			 */
	},
	{
		1,		/* d_fileno			 */
		sizeof(struct direntry),	/* d_reclen			 */
		DT_DIR,		/* d_type			 */
		2,		/* d_namlen			 */
		".."		/* d_name			 */
	}
};

static int
msdosfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	int error = 0;
	int diff;
	char pushout;
	long n;
	long on;
	long lost;
	long count;
	u_long cn;
	u_long fileno;
	long bias = 0;
	daddr_t bn;
	daddr_t lbn;
	struct buf *bp;
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
	struct dirent *prev;
	struct dirent *crnt;
	u_char dirbuf[512];	/* holds converted dos directories */
	struct uio *uio = ap->a_uio;
	off_t off;
	int ncookies = 0;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_readdir(): vp %08x, uio %08x, cred %08x, eofflagp %08x\n",
	       ap->a_vp, uio, ap->a_cred, ap->a_eofflag);
#endif

	/*
	 * msdosfs_readdir() won't operate properly on regular files since
	 * it does i/o only with the the filesystem vnode, and hence can
	 * retrieve the wrong block from the buffer cache for a plain file.
	 * So, fail attempts to readdir() on a plain file.
	 */
	if ((dep->de_Attributes & ATTR_DIRECTORY) == 0)
		return ENOTDIR;

	/*
	 * If the user buffer is smaller than the size of one dos directory
	 * entry or the file offset is not a multiple of the size of a
	 * directory entry, then we fail the read.
	 */
	count = uio->uio_resid & ~(sizeof(struct direntry) - 1);
	lost = uio->uio_resid - count;
	if (count < sizeof(struct direntry) ||
	    (uio->uio_offset & (sizeof(struct direntry) - 1)))
		return EINVAL;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
	off = uio->uio_offset;

	/*
	 * If they are reading from the root directory then, we simulate
	 * the . and .. entries since these don't exist in the root
	 * directory.  We also set the offset bias to make up for having to
	 * simulate these entries. By this I mean that at file offset 64 we
	 * read the first entry in the root directory that lives on disk.
	 */
	if (dep->de_StartCluster == MSDOSFSROOT) {
		/*
		 * printf("msdosfs_readdir(): going after . or .. in root dir, offset %d\n",
		 *	  uio->uio_offset);
		 */
		bias = 2 * sizeof(struct direntry);
		if (uio->uio_offset < 2 * sizeof(struct direntry)) {
			if (uio->uio_offset
			    && uio->uio_offset != sizeof(struct direntry)) {
				error = EINVAL;
				goto out;
			}
			n = 1;
			if (!uio->uio_offset) {
				n = 2;
				ncookies++;
			}
			ncookies++;
			error = uiomove((char *) rootdots + uio->uio_offset,
					n * sizeof(struct direntry), uio);
		}
	}
	while (!error && uio->uio_resid > 0) {
		lbn = (uio->uio_offset - bias) >> pmp->pm_cnshift;
		on = (uio->uio_offset - bias) & pmp->pm_crbomask;
		n = min((u_long) (pmp->pm_bpcluster - on), uio->uio_resid);
		diff = dep->de_FileSize - (uio->uio_offset - bias);
		if (diff <= 0)
			break;
		if (diff < n)
			n = diff;
		error = pcbmap(dep, lbn, &bn, &cn);
		if (error)
			break;
		error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED, &bp);
		n = min(n, pmp->pm_bpcluster - bp->b_resid);
		if (error) {
			brelse(bp);
			return error;
		}

		/*
		 * code to convert from dos directory entries to ufs
		 * directory entries
		 */
		pushout = 0;
		dentp = (struct direntry *)(bp->b_data + on);
		prev = 0;
		crnt = (struct dirent *) dirbuf;
		while ((char *) dentp < bp->b_data + on + n) {
			/*
			 * printf("rd: dentp %08x prev %08x crnt %08x deName %02x attr %02x\n",
			 *	  dentp, prev, crnt, dentp->deName[0], dentp->deAttributes);
			 */
			/*
			 * If we have an empty entry or a slot from a
			 * deleted file, or a volume label entry just
			 * concatenate its space onto the end of the
			 * previous entry or, manufacture an empty entry if
			 * there is no previous entry.
			 */
			if (dentp->deName[0] == SLOT_EMPTY ||
			    dentp->deName[0] == SLOT_DELETED ||
			    (dentp->deAttributes & ATTR_VOLUME)) {
				if (prev) {
					prev->d_reclen += sizeof(struct direntry);
				} else {
					prev = crnt;
					prev->d_fileno = 0;
					prev->d_reclen = sizeof(struct direntry);
					prev->d_type = DT_UNKNOWN;
					prev->d_namlen = 0;
					prev->d_name[0] = 0;
					ncookies++;
				}
			} else {
				/*
				 * this computation of d_fileno must match
				 * the computation of va_fileid in
				 * msdosfs_getattr
				 */
				if (dentp->deAttributes & ATTR_DIRECTORY) {
					/* if this is the root directory */
					fileno = getushort(dentp->deStartCluster);
					if (fileno == MSDOSFSROOT)
						fileno = 1;
				} else {
					/*
					 * if the file's dirent lives in
					 * root dir
					 */
					if ((fileno = cn) == MSDOSFSROOT)
						fileno = 1;
					fileno = (fileno << 16) |
					    ((dentp - (struct direntry *) bp->b_data) & 0xffff);
				}
				crnt->d_fileno = fileno;
				crnt->d_reclen = sizeof(struct direntry);
				crnt->d_type = (dentp->deAttributes & ATTR_DIRECTORY)
					         ? DT_DIR : DT_REG;
				crnt->d_namlen = dos2unixfn(dentp->deName,
							    (u_char *)crnt->d_name);
				/*
				 * printf("readdir: file %s, fileno %08x, attr %02x, start %08x\n",
				 *	  crnt->d_name, crnt->d_fileno, dentp->deAttributes,
				 *	  dentp->deStartCluster);
				 */
				prev = crnt;
				ncookies++;
			}
			dentp++;

			crnt = (struct dirent *) ((char *) crnt + sizeof(struct direntry));
			pushout = 1;

			/*
			 * If our intermediate buffer is full then copy its
			 * contents to user space.  I would just use the
			 * buffer the buf header points to but, I'm afraid
			 * that when we brelse() it someone else might find
			 * it in the cache and think its contents are
			 * valid.  Maybe there is a way to invalidate the
			 * buffer before brelse()'ing it.
			 */
			if ((u_char *) crnt >= &dirbuf[sizeof dirbuf]) {
				pushout = 0;
				error = uiomove(dirbuf, sizeof(dirbuf), uio);
				if (error)
					break;
				prev = 0;
				crnt = (struct dirent *) dirbuf;
			}
		}
		if (pushout) {
			pushout = 0;
			error = uiomove(dirbuf, (char *) crnt - (char *) dirbuf,
			    uio);
		}

#if 0
		/*
		 * If we have read everything from this block or have read
		 * to end of file then we are done with this block.  Mark
		 * it to say the buffer can be reused if need be.
		 */
		if (n + on == pmp->pm_bpcluster ||
		    (uio->uio_offset - bias) == dep->de_FileSize)
			bp->b_flags |= B_AGE;
#endif /* if 0 */
		brelse(bp);
		if (n == 0)
			break;
	}
out:	;
	uio->uio_resid += lost;
	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dpEnd;
		struct dirent* dp;
		u_long *cookies;
		u_long *cookiep;

		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("msdosfs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		     (uio->uio_iov->iov_base - (uio->uio_offset - off));
		dpEnd = (struct dirent *) uio->uio_iov->iov_base;
		cookies = malloc(ncookies * sizeof(*cookies), M_TEMP, M_WAITOK);
		for (dp = dpStart, cookiep = cookies;
		     dp < dpEnd;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen)) {
			off += dp->d_reclen;
			*cookiep++ = (u_long) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}

	/*
	 * Set the eofflag (NFS uses it)
	 */
	if (ap->a_eofflag)
		if (dep->de_FileSize - (uio->uio_offset - bias) <= 0)
			*ap->a_eofflag = 1;
		else
			*ap->a_eofflag = 0;

	return error;
}

/*
 * DOS filesystems don't know what symlinks are.
 */
static int
msdosfs_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	return EINVAL;
}

static int
msdosfs_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
	return 0;
}

static int
msdosfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTODE(vp)->de_lock, ap->a_flags, &vp->v_interlock,
		ap->a_p));
}

int
msdosfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTODE(vp)->de_lock, ap->a_flags | LK_RELEASE,
		&vp->v_interlock, ap->a_p));
}

int
msdosfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (lockstatus(&VTODE(ap->a_vp)->de_lock));
}

/*
 * vp  - address of vnode file the file
 * bn  - which cluster we are interested in mapping to a filesystem block number.
 * vpp - returns the vnode for the block special file holding the filesystem
 *	 containing the file of interest
 * bnp - address of where to return the filesystem relative block number
 */
static int
msdosfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	struct denode *dep = VTODE(ap->a_vp);

	if (ap->a_vpp != NULL)
		*ap->a_vpp = dep->de_devvp;
	if (ap->a_bnp == NULL)
		return 0;
	if (ap->a_runp) {
		/*
		 * Sequential clusters should be counted here.
		 */
		*ap->a_runp = 0;
	}
	if (ap->a_runb) {
		*ap->a_runb = 0;
	}
	return pcbmap(dep, ap->a_bn, ap->a_bnp, 0);
}

static int
msdosfs_reallocblks(ap)
	struct vop_reallocblks_args /* {
		struct vnode *a_vp;
		struct cluster_save *a_buflist;
	} */ *ap;
{
	/* Currently no support for clustering */		/* XXX */
	return ENOSPC;
}

static int
msdosfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct denode *dep = VTODE(bp->b_vp);
	struct vnode *vp;
	int error = 0;

	if (bp->b_vp->v_type == VBLK || bp->b_vp->v_type == VCHR)
		panic("msdosfs_strategy: spec");
	/*
	 * If we don't already know the filesystem relative block number
	 * then get it using pcbmap().  If pcbmap() returns the block
	 * number as -1 then we've got a hole in the file.  DOS filesystems
	 * don't allow files with holes, so we shouldn't ever see this.
	 */
	if (bp->b_blkno == bp->b_lblkno) {
		error = pcbmap(dep, bp->b_lblkno, &bp->b_blkno, 0);
		if (error)
			bp->b_blkno = -1;
		if (bp->b_blkno == -1)
			clrbuf(bp);
	}
	if (bp->b_blkno == -1) {
		biodone(bp);
		return error;
	}
#ifdef DIAGNOSTIC
#endif
	/*
	 * Read/write the block from/to the disk that contains the desired
	 * file block.
	 */
	vp = dep->de_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL(vp->v_op, VOFFSET(vop_strategy), ap);
	return 0;
}

static int
msdosfs_print(ap)
	struct vop_print_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct denode *dep = VTODE(ap->a_vp);

	printf(
	    "tag VT_MSDOSFS, startcluster %d, dircluster %ld, diroffset %ld ",
	       dep->de_StartCluster, dep->de_dirclust, dep->de_diroffset);
	printf(" dev %d, %d", major(dep->de_dev), minor(dep->de_dev));
	lockmgr_printinfo(&dep->de_lock);
	printf("\n");
	return 0;
}

static int
msdosfs_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap;
{
	return EINVAL;		/* we don't do locking yet		 */
}

static int
msdosfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return 0;
	case _PC_NAME_MAX:
		*ap->a_retval = 12;
		return 0;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX; /* 255? */
		return 0;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return 0;
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		return 0;
	default:
		return EINVAL;
	}
}

/* Global vfs data structures for msdosfs */
vop_t **msdosfs_vnodeop_p;
static struct vnodeopv_entry_desc msdosfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vn_default_error },
	{ &vop_abortop_desc,		(vop_t *) msdosfs_abortop },
	{ &vop_access_desc,		(vop_t *) msdosfs_access },
	{ &vop_advlock_desc,		(vop_t *) msdosfs_advlock },
	{ &vop_bmap_desc,		(vop_t *) msdosfs_bmap },
	{ &vop_bwrite_desc,		(vop_t *) vn_bwrite },
	{ &vop_cachedlookup_desc,	(vop_t *) msdosfs_lookup },
	{ &vop_close_desc,		(vop_t *) msdosfs_close },
	{ &vop_create_desc,		(vop_t *) msdosfs_create },
	{ &vop_fsync_desc,		(vop_t *) msdosfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) msdosfs_getattr },
	{ &vop_inactive_desc,		(vop_t *) msdosfs_inactive },
	{ &vop_ioctl_desc,		(vop_t *) msdosfs_ioctl },
	{ &vop_islocked_desc,		(vop_t *) msdosfs_islocked },
	{ &vop_link_desc,		(vop_t *) msdosfs_link },
	{ &vop_lock_desc,		(vop_t *) msdosfs_lock },
	{ &vop_lookup_desc,		(vop_t *) vfs_cache_lookup },
	{ &vop_mkdir_desc,		(vop_t *) msdosfs_mkdir },
	{ &vop_mknod_desc,		(vop_t *) msdosfs_mknod },
	{ &vop_mmap_desc,		(vop_t *) msdosfs_mmap },
	{ &vop_open_desc,		(vop_t *) msdosfs_open },
	{ &vop_pathconf_desc,		(vop_t *) msdosfs_pathconf },
	{ &vop_poll_desc,		(vop_t *) msdosfs_poll },
	{ &vop_print_desc,		(vop_t *) msdosfs_print },
	{ &vop_read_desc,		(vop_t *) msdosfs_read },
	{ &vop_readdir_desc,		(vop_t *) msdosfs_readdir },
	{ &vop_readlink_desc,		(vop_t *) msdosfs_readlink },
	{ &vop_reallocblks_desc,	(vop_t *) msdosfs_reallocblks },
	{ &vop_reclaim_desc,		(vop_t *) msdosfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) msdosfs_remove },
	{ &vop_rename_desc,		(vop_t *) msdosfs_rename },
	{ &vop_rmdir_desc,		(vop_t *) msdosfs_rmdir },
	{ &vop_seek_desc,		(vop_t *) msdosfs_seek },
	{ &vop_setattr_desc,		(vop_t *) msdosfs_setattr },
	{ &vop_strategy_desc,		(vop_t *) msdosfs_strategy },
	{ &vop_symlink_desc,		(vop_t *) msdosfs_symlink },
	{ &vop_unlock_desc,		(vop_t *) msdosfs_unlock },
	{ &vop_write_desc,		(vop_t *) msdosfs_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc msdosfs_vnodeop_opv_desc =
	{ &msdosfs_vnodeop_p, msdosfs_vnodeop_entries };

VNODEOP_SET(msdosfs_vnodeop_opv_desc);
