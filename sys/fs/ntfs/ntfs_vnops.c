/*	$NetBSD: ntfs_vnops.c,v 1.23 1999/10/31 19:45:27 jdolecek Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/dirent.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>

#include <sys/sysctl.h>

/*#define NTFS_DEBUG 1*/
#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_subr.h>

#include <sys/unistd.h> /* for pathconf(2) constants */

static int	ntfs_read(struct vop_read_args *);
static int	ntfs_write(struct vop_write_args *ap);
static int	ntfs_getattr(struct vop_getattr_args *ap);
static int	ntfs_inactive(struct vop_inactive_args *ap);
static int	ntfs_reclaim(struct vop_reclaim_args *ap);
static int	ntfs_bmap(struct vop_bmap_args *ap);
static int	ntfs_strategy(struct vop_strategy_args *ap);
static int	ntfs_access(struct vop_access_args *ap);
static int	ntfs_open(struct vop_open_args *ap);
static int	ntfs_close(struct vop_close_args *ap);
static int	ntfs_readdir(struct vop_readdir_args *ap);
static int	ntfs_lookup(struct vop_lookup_args *ap);
static int	ntfs_fsync(struct vop_fsync_args *ap);
static int	ntfs_pathconf(void *);

int	ntfs_prtactive = 1;	/* 1 => print out reclaim of active vnodes */

/*
 * This is a noop, simply returning what one has been given.
 */
int
ntfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	dprintf(("ntfs_bmap: vn: %p, blk: %d\n", ap->a_vp,(u_int32_t)ap->a_bn));
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

static int
ntfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	struct buf *bp;
	daddr_t cn;
	int resid, off, toread;
	int error;

	dprintf(("ntfs_read: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));

	dprintf(("ntfs_read: filesize: %d",(u_int32_t)fp->f_size));

	/* don't allow reading after end of file */
	if (uio->uio_offset > fp->f_size)
		return (0);

	resid = min(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", resid: %d\n", resid));

	error = 0;
	while (resid) {
		cn = ntfs_btocn(uio->uio_offset);
		off = ntfs_btocnoff(uio->uio_offset);

		toread = min(off + resid, ntfs_cntob(1));

		error = bread(vp, cn, ntfs_cntob(1), NOCRED, &bp);
		if (error) {
			brelse(bp);
			break;
		}

		error = uiomove(bp->b_data + off, toread - off, uio);
		if(error) {
			brelse(bp);
			break;
		}
		brelse(bp);

		resid -= toread - off;
	}

	return (error);
}

static int
ntfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	register struct vattr *vap = ap->a_vap;

	dprintf(("ntfs_getattr: %d, flags: %d\n",ip->i_number,ip->i_flag));

	vap->va_fsid = dev2udev(ip->i_dev);
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mp->ntm_mode;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_mp->ntm_uid;
	vap->va_gid = ip->i_mp->ntm_gid;
	vap->va_rdev = 0;				/* XXX UNODEV ? */
	vap->va_size = fp->f_size;
	vap->va_bytes = fp->f_allocated;
	vap->va_atime = ntfs_nttimetounix(fp->f_times.t_access);
	vap->va_mtime = ntfs_nttimetounix(fp->f_times.t_write);
	vap->va_ctime = ntfs_nttimetounix(fp->f_times.t_create);
	vap->va_flags = ip->i_flag;
	vap->va_gen = 0;
	vap->va_blocksize = ip->i_mp->ntm_spc * ip->i_mp->ntm_bps;
	vap->va_type = vp->v_type;
	vap->va_filerev = 0;
	return (0);
}

/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
#ifdef NTFS_DEBUG
	register struct ntnode *ip = VTONT(vp);
#endif

	dprintf(("ntfs_inactive: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vrefcnt(vp) != 0)
		vprint("ntfs_inactive: pushing active", vp);

	VOP_UNLOCK(vp, 0, ap->a_td);

	/* XXX since we don't support any filesystem changes
	 * right now, nothing more needs to be done
	 */
	return (0);
}

/*
 * Reclaim an fnode/ntnode so that it can be used for other purposes.
 */
int
ntfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	int error;

	dprintf(("ntfs_reclaim: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vrefcnt(vp) != 0)
		vprint("ntfs_reclaim: pushing active", vp);

	if ((error = ntfs_ntget(ip)) != 0)
		return (error);
	
	/* Purge old data structures associated with the inode. */
	cache_purge(vp);

	ntfs_frele(fp);
	ntfs_ntput(ip);
	vp->v_data = NULL;

	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct ntfsmount *ntmp = ip->i_mp;
	int error;

	dprintf(("ntfs_strategy: offset: %d, blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_offset,(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));

	dprintf(("strategy: bcount: %d flags: 0x%lx\n", 
		(u_int32_t)bp->b_bcount,bp->b_flags));

	if (bp->b_iocmd == BIO_READ) {
		u_int32_t toread;

		if (ntfs_cntob(bp->b_blkno) >= fp->f_size) {
			clrbuf(bp);
			error = 0;
		} else {
			toread = min(bp->b_bcount,
				 fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: toread: %d, fsize: %d\n",
				toread,(u_int32_t)fp->f_size));

			error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, ntfs_cntob(bp->b_blkno),
				toread, bp->b_data, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_readattr failed\n");
				bp->b_error = error;
				bp->b_ioflags |= BIO_ERROR;
			}

			bzero(bp->b_data + toread, bp->b_bcount - toread);
		}
	} else {
		size_t tmp;
		u_int32_t towrite;

		if (ntfs_cntob(bp->b_blkno) + bp->b_bcount >= fp->f_size) {
			printf("ntfs_strategy: CAN'T EXTEND FILE\n");
			bp->b_error = error = EFBIG;
			bp->b_ioflags |= BIO_ERROR;
		} else {
			towrite = min(bp->b_bcount,
				fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: towrite: %d, fsize: %d\n",
				towrite,(u_int32_t)fp->f_size));

			error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,	
				fp->f_attrname, ntfs_cntob(bp->b_blkno),towrite,
				bp->b_data, &tmp, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_writeattr fail\n");
				bp->b_error = error;
				bp->b_ioflags |= BIO_ERROR;
			}
		}
	}
	bufdone(bp);
	return (error);
}

static int
ntfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t towrite;
	size_t written;
	int error;

	dprintf(("ntfs_write: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));
	dprintf(("ntfs_write: filesize: %d",(u_int32_t)fp->f_size));

	if (uio->uio_resid + uio->uio_offset > fp->f_size) {
		printf("ntfs_write: CAN'T WRITE BEYOND END OF FILE\n");
		return (EFBIG);
	}

	towrite = min(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", towrite: %d\n",(u_int32_t)towrite));

	error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, towrite, NULL, &written, uio);
#ifdef NTFS_DEBUG
	if (error)
		printf("ntfs_write: ntfs_writeattr failed: %d\n", error);
#endif

	return (error);
}

int
ntfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);
	mode_t mode = ap->a_mode;
#ifdef QUOTA
	int error;
#endif

	dprintf(("ntfs_access: %d\n",ip->i_number));

	/*
	 * Disallow write attempts on read-only filesystems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (mode & VWRITE) {
		switch ((int)vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (error = getinoquota(ip))
				return (error);
#endif
			break;
		}
	}

	return (vaccess(vp->v_type, ip->i_mp->ntm_mode, ip->i_mp->ntm_uid,
	    ip->i_mp->ntm_gid, ap->a_mode, ap->a_cred, NULL));
} 

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_open: %d\n",ip->i_number);
#endif

	/*
	 * Files marked append-only must be opened for appending.
	 */

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ntfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_close: %d\n",ip->i_number);
#endif

	return (0);
}

int
ntfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_ncookies;
		u_int **cookies;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int i, error = 0;
	u_int32_t faked = 0, num;
	int ncookies = 0;
	struct dirent cde;
	off_t off;

	dprintf(("ntfs_readdir %d off: %d resid: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid));

	off = uio->uio_offset;

	/* Simulate . in every dir except ROOT */
	if( ip->i_number != NTFS_ROOTINO ) {
		struct dirent dot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 1, "." };

		if( uio->uio_offset < sizeof(struct dirent) ) {
			dot.d_fileno = ip->i_number;
			error = uiomove((char *)&dot,sizeof(struct dirent),uio);
			if(error)
				return (error);

			ncookies ++;
		}
	}

	/* Simulate .. in every dir including ROOT */
	if( uio->uio_offset < 2 * sizeof(struct dirent) ) {
		struct dirent dotdot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 2, ".." };

		error = uiomove((char *)&dotdot,sizeof(struct dirent),uio);
		if(error)
			return (error);

		ncookies ++;
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio->uio_offset / sizeof(struct dirent) - faked;

	while( uio->uio_resid >= sizeof(struct dirent) ) {
		struct attr_indexentry *iep;

		error = ntfs_ntreaddir(ntmp, fp, num, &iep);

		if(error)
			return (error);

		if( NULL == iep )
			break;

		for(; !(iep->ie_flag & NTFS_IEFLAG_LAST) && (uio->uio_resid >= sizeof(struct dirent));
			iep = NTFS_NEXTREC(iep, struct attr_indexentry *))
		{
			if(!ntfs_isnamepermitted(ntmp,iep))
				continue;

			for(i=0; i<iep->ie_fnamelen; i++) {
				cde.d_name[i] = NTFS_U28(iep->ie_fname[i]);
			}
			cde.d_name[i] = '\0';
			dprintf(("ntfs_readdir: elem: %d, fname:[%s] type: %d, flag: %d, ",
				num, cde.d_name, iep->ie_fnametype,
				iep->ie_flag));
			cde.d_namlen = iep->ie_fnamelen;
			cde.d_fileno = iep->ie_number;
			cde.d_type = (iep->ie_fflag & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
			cde.d_reclen = sizeof(struct dirent);
			dprintf(("%s\n", (cde.d_type == DT_DIR) ? "dir":"reg"));

			error = uiomove((char *)&cde, sizeof(struct dirent), uio);
			if(error)
				return (error);

			ncookies++;
			num++;
		}
	}

	dprintf(("ntfs_readdir: %d entries (%d bytes) read\n",
		ncookies,(u_int)(uio->uio_offset - off)));
	dprintf(("ntfs_readdir: off: %d resid: %d\n",
		(u_int32_t)uio->uio_offset,uio->uio_resid));

	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dp;
		u_long *cookies;
		u_long *cookiep;

		ddprintf(("ntfs_readdir: %d cookies\n",ncookies));
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ntfs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		     ((caddr_t)uio->uio_iov->iov_base -
			 (uio->uio_offset - off));
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long),
		       M_TEMP, 0);
		for (dp = dpStart, cookiep = cookies, i=0;
		     i < ncookies;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen), i++) {
			off += dp->d_reclen;
			*cookiep++ = (u_int) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
/*
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTONT(ap->a_vp)->i_size <= uio->uio_offset;
*/
	return (error);
}

int
ntfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int error;
	int lockparent = cnp->cn_flags & LOCKPARENT;
#if NTFS_DEBUG
	int wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);
#endif
	dprintf(("ntfs_lookup: \"%.*s\" (%ld bytes) in %d, lp: %d, wp: %d \n",
		(int)cnp->cn_namelen, cnp->cn_nameptr, cnp->cn_namelen,
		dip->i_number, lockparent, wantparent));

	error = VOP_ACCESS(dvp, VEXEC, cred, cnp->cn_thread);
	if(error)
		return (error);

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	if(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		dprintf(("ntfs_lookup: faking . directory in %d\n",
			dip->i_number));

		VREF(dvp);
		*ap->a_vpp = dvp;
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		struct ntvattr *vap;

		dprintf(("ntfs_lookup: faking .. directory in %d\n",
			 dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, &vap);
		if(error)
			return (error);

		VOP_UNLOCK(dvp,0,cnp->cn_thread);
		cnp->cn_flags |= PDIRUNLOCK;

		dprintf(("ntfs_lookup: parentdir: %d\n",
			 vap->va_a_name->n_pnumber));
		error = VFS_VGET(ntmp->ntm_mountp, vap->va_a_name->n_pnumber,
				 LK_EXCLUSIVE, ap->a_vpp); 
		ntfs_ntvattrrele(vap);
		if (error) {
			if (vn_lock(dvp,LK_EXCLUSIVE|LK_RETRY,cnp->cn_thread)==0)
				cnp->cn_flags &= ~PDIRUNLOCK;
			return (error);
		}

		if (lockparent && (cnp->cn_flags & ISLASTCN)) {
			error = vn_lock(dvp, LK_EXCLUSIVE, cnp->cn_thread);
			if (error) {
				vput( *(ap->a_vpp) );
				return (error);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
	} else {
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, ap->a_vpp);
		if (error) {
			dprintf(("ntfs_ntlookupfile: returned %d\n", error));
			return (error);
		}

		dprintf(("ntfs_lookup: found ino: %d\n", 
			VTONT(*ap->a_vpp)->i_number));

		if(!lockparent || !(cnp->cn_flags & ISLASTCN))
			VOP_UNLOCK(dvp, 0, cnp->cn_thread);
	}

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *ap->a_vpp, cnp);

	return (error);
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	return (0);
}

/*
 * Return POSIX pathconf information applicable to NTFS filesystem
 */
int
ntfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NTFS_MAXFILENAME;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Global vfs data structures
 */
vop_t **ntfs_vnodeop_p;
static
struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vop_defaultop },

	{ &vop_getattr_desc, (vop_t *)ntfs_getattr },
	{ &vop_inactive_desc, (vop_t *)ntfs_inactive },
	{ &vop_reclaim_desc, (vop_t *)ntfs_reclaim },
	{ &vop_print_desc, (vop_t *)vop_null },
	{ &vop_pathconf_desc, ntfs_pathconf },

	{ &vop_cachedlookup_desc, (vop_t *)ntfs_lookup },
	{ &vop_lookup_desc, (vop_t *)vfs_cache_lookup },

	{ &vop_access_desc, (vop_t *)ntfs_access },
	{ &vop_close_desc, (vop_t *)ntfs_close },
	{ &vop_open_desc, (vop_t *)ntfs_open },
	{ &vop_readdir_desc, (vop_t *)ntfs_readdir },
	{ &vop_fsync_desc, (vop_t *)ntfs_fsync },

	{ &vop_bmap_desc, (vop_t *)ntfs_bmap },
	{ &vop_strategy_desc, (vop_t *)ntfs_strategy },
	{ &vop_read_desc, (vop_t *)ntfs_read },
	{ &vop_write_desc, (vop_t *)ntfs_write },

	{ NULL, NULL }
};

static
struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

VNODEOP_SET(ntfs_vnodeop_opv_desc);
