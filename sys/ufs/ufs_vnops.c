/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	from: @(#)ufs_vnops.c	7.64 (Berkeley) 5/16/91
 *	$Id: ufs_vnops.c,v 1.14 1994/01/19 21:09:26 jtc Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "resourcevar.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "buf.h"
#include "proc.h"
#include "conf.h"
#include "mount.h"
#include "vnode.h"
#include "specdev.h"
#include "fifo.h"
#include "malloc.h"

#include "lockf.h"
#include "quota.h"
#include "inode.h"
#include "dir.h"
#include "fs.h"

/* Get the current value of _POSIX_CHOWN_RESTRICTED */
#include "sys/unistd.h"		/* make sure to get sys/ version. */

static int maknode(int, struct nameidata *, struct inode **);

/* XXX - don't want to change ucred.h again */
extern int groupmember(int /*gid_t*/, struct ucred *);

/*
 * Create a regular file
 */
int
ufs_create(ndp, vap, p)
	struct nameidata *ndp;
	struct vattr *vap;
	struct proc *p;
{
	struct inode *ip;
	int error;

	if (error = maknode(MAKEIMODE(vap->va_type, vap->va_mode), ndp, &ip))
		return (error);
	ndp->ni_vp = ITOV(ip);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ufs_mknod(ndp, vap, cred, p)
	struct nameidata *ndp;
	struct ucred *cred;
	struct vattr *vap;
	struct proc *p;
{
	register struct vnode *vp;
	struct inode *ip;
	int error;

	if (error = maknode(MAKEIMODE(vap->va_type, vap->va_mode), ndp, &ip))
		return (error);
	ip->i_flag |= IACC|IUPD|ICHG;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_rdev = vap->va_rdev;
	}
	/*
	 * Remove inode so that it will be reloaded by iget and
	 * checked to see if it is an alias of an existing entry
	 * in the inode cache.
	 */
	vp = ITOV(ip);
	vput(vp);
	vp->v_type = VNON;
	vgone(vp);
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ufs_open(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{

	return (0);
}

/*
 * Close called
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
int
ufs_close(vp, fflag, cred, p)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);

	if (vp->v_usecount > 1 && !(ip->i_flag & ILOCKED))
		ITIMES(ip, &time, &time);
	return (0);
}

/*
 * Check mode permission on inode pointer. Mode is READ, WRITE or EXEC.
 * The mode is shifted to select the owner/group/other fields. The
 * super user is granted all permissions.
 */
int
ufs_access(vp, mode, cred, p)
	struct vnode *vp;
	register int mode;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	register gid_t *gp;
	int i, error;

#ifdef DIAGNOSTIC
	if (!VOP_ISLOCKED(vp)) {
		vprint("ufs_access: not locked", vp);
		panic("ufs_access: not locked");
	}
#endif
#ifdef QUOTA
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VREG: case VDIR: case VLNK:
			if (error = getinoquota(ip))
				return (error);
		}
	}
#endif /* QUOTA */
	/*
	 * If you're the super-user, you always get access.
	 */
	if (cred->cr_uid == 0)
		return (0);
	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (cred->cr_uid != ip->i_uid) {
		mode >>= 3;
		gp = cred->cr_groups;
		for (i = 0; i < cred->cr_ngroups; i++, gp++)
			if (ip->i_gid == *gp)
				goto found;
		mode >>= 3;
found:
		;
	}
	if ((ip->i_mode & mode) == mode)
		return (0);
	return (EACCES);
}

/* ARGSUSED */
int
ufs_getattr(vp, vap, cred, p)
	struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);

	ITIMES(ip, &time, &time);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)ip->i_rdev;
#ifdef tahoe
	vap->va_size = ip->i_size;
	vap->va_size_rsv = 0;
#else
	vap->va_qsize.v = ip->i_din.di_qsize.v;
#endif
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_usec = 0;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_usec = 0;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_usec = 0;
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = ip->i_fs->fs_bsize;
	vap->va_bytes = dbtob(ip->i_blocks);
	vap->va_bytes_rsv = 0;
	vap->va_type = vp->v_type;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ufs_setattr(vp, vap, cred, p)
	register struct vnode *vp;
	register struct vattr *vap;
	register struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	int error = 0;

	/*
	 * Check for unsetable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (u_short)VNOVAL || vap->va_gid != (u_short)VNOVAL)
		if (error = chown1(vp, vap->va_uid, vap->va_gid, p))
			return (error);
	if (vap->va_size != VNOVAL) {
		if (vp->v_type == VDIR)
			return (EISDIR);
		if (error = itrunc(ip, vap->va_size, 0)) /* XXX IO_SYNC? */
			return (error);
	}
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (cred->cr_uid != ip->i_uid &&
		    (error = suser(cred, &p->p_acflag)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 || 
	            (error = VOP_ACCESS(vp, VWRITE, cred, p))))
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			ip->i_flag |= IACC;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IUPD;
		ip->i_flag |= ICHG;
		if (error = iupdat(ip, &vap->va_atime, &vap->va_mtime, 1))
			return (error);
	}
	if (vap->va_mode != (u_short)VNOVAL)
		error = chmod1(vp, (int)vap->va_mode, p);
	if (vap->va_flags != VNOVAL) {
		if (cred->cr_uid != ip->i_uid &&
		    (error = suser(cred, &p->p_acflag)))
			return (error);
		if (cred->cr_uid == 0) {
			ip->i_flags = vap->va_flags;
		} else {
			ip->i_flags &= 0xffff0000ul;
			ip->i_flags |= (vap->va_flags & 0xffff);
		}
		ip->i_flag |= ICHG;
	}
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
int
chmod1(vp, mode, p)
	register struct vnode *vp;
	register int mode;
	struct proc *p;
{
	register struct ucred *cred = p->p_ucred;
	register struct inode *ip = VTOI(vp);
	int error;

	if (cred->cr_uid != ip->i_uid &&
	    (error = suser(cred, &p->p_acflag)))
		return (error);
	if (cred->cr_uid) {
		if (vp->v_type != VDIR && (mode & ISVTX))
			return (EFTYPE);
		if (!groupmember(ip->i_gid, cred) && (mode & ISGID))
			return (EPERM);
	}
	ip->i_mode &= ~07777;
	ip->i_mode |= mode & 07777;
	ip->i_flag |= ICHG;
	if ((vp->v_flag & VTEXT) && (ip->i_mode & ISVTX) == 0)
		(void) vnode_pager_uncache(vp);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
int
chown1(vp, uid, gid, p)
	register struct vnode *vp;
	uid_t uid;
	gid_t gid;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	register struct ucred *cred = p->p_ucred;
	uid_t ouid;
	gid_t ogid;
	int error = 0;
#ifdef QUOTA
	register int i;
	long change;
#endif

	if (uid == (u_short)VNOVAL)
		uid = ip->i_uid;
	if (gid == (u_short)VNOVAL)
		gid = ip->i_gid;
	/*
	 * If we don't own the file, are trying to change the owner
	 * of the file, or are not a member of the target group,
	 * the caller must be superuser or the call fails.
	 */
#ifdef _POSIX_CHOWN_RESTRICTED
	if ((cred->cr_uid != ip->i_uid || uid != ip->i_uid ||
	    !groupmember((gid_t)gid, cred)) &&
	    (error = suser(cred, &p->p_acflag)))
		return (error);
#else
	if ((cred->cr_uid != ip->i_uid || !groupmember((gid_t)gid, cred))
	    && (error = suser(cred, &p->p_acflag)))
		return error;
#endif
	ouid = ip->i_uid;
	ogid = ip->i_gid;
#ifdef QUOTA
	if (error = getinoquota(ip))
		return (error);
	if (ouid == uid) {
		dqrele(vp, ip->i_dquot[USRQUOTA]);
		ip->i_dquot[USRQUOTA] = NODQUOT;
	}
	if (ogid == gid) {
		dqrele(vp, ip->i_dquot[GRPQUOTA]);
		ip->i_dquot[GRPQUOTA] = NODQUOT;
	}
	change = ip->i_blocks;
	(void) chkdq(ip, -change, cred, CHOWN);
	(void) chkiq(ip, -1, cred, CHOWN);
	for (i = 0; i < MAXQUOTAS; i++) {
		dqrele(vp, ip->i_dquot[i]);
		ip->i_dquot[i] = NODQUOT;
	}
#endif
	ip->i_uid = uid;
	ip->i_gid = gid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		if ((error = chkdq(ip, change, cred, CHOWN)) == 0) {
			if ((error = chkiq(ip, 1, cred, CHOWN)) == 0)
				goto good;
			else
				(void) chkdq(ip, -change, cred, CHOWN|FORCE);
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dqrele(vp, ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
	}
	ip->i_uid = ouid;
	ip->i_gid = ogid;
	if (getinoquota(ip) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		(void) chkdq(ip, change, cred, FORCE|CHOWN);
		(void) chkiq(ip, 1, cred, FORCE|CHOWN);
		(void) getinoquota(ip);
	}
	return (error);
good:
	if (getinoquota(ip))
		panic("chown: lost quota");
#endif /* QUOTA */
	if (ouid != uid || ogid != gid)
		ip->i_flag |= ICHG;
	if (ouid != uid && cred->cr_uid != 0)
		ip->i_mode &= ~ISUID;
	if (ogid != gid && cred->cr_uid != 0)
		ip->i_mode &= ~ISGID;
	return (0);
}

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
ufs_read(vp, uio, ioflag, cred)
	struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	register struct inode *ip = VTOI(vp);
	register struct fs *fs;
	struct buf *bp;
	daddr_t lbn, bn, rablock;
	int size, diff, error = 0;
	long n, on, type;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("ufs_read mode");
	type = ip->i_mode & IFMT;
	if (type != IFDIR && type != IFREG && type != IFLNK)
		panic("ufs_read type");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	ip->i_flag |= IACC;
	fs = ip->i_fs;
	do {
		lbn = lblkno(fs, uio->uio_offset);
		on = blkoff(fs, uio->uio_offset);
		n = MIN((unsigned)(fs->fs_bsize - on), uio->uio_resid);
		diff = ip->i_size - uio->uio_offset;
		if (diff <= 0)
			return (0);
		if (diff < n)
			n = diff;
		size = blksize(fs, ip, lbn);
		rablock = lbn + 1;
		if (vp->v_lastr + 1 == lbn &&
		    lblktosize(fs, rablock) < ip->i_size)
			error = breada(ITOV(ip), lbn, size, rablock,
				blksize(fs, ip, rablock), NOCRED, &bp);
		else
			error = bread(ITOV(ip), lbn, size, NOCRED, &bp);
		vp->v_lastr = lbn;
		n = MIN(n, size - bp->b_resid);
		if (error) {
			brelse(bp);
			return (error);
		}
		error = uiomove(bp->b_un.b_addr + on, (int)n, uio);
#if OMIT	/* 20 Aug 92*/
		if (n + on == fs->fs_bsize || uio->uio_offset == ip->i_size)
			bp->b_flags |= B_AGE;
#endif	/* OMIT*/
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
}

/*
 * Vnode op for writing.
 */
int
ufs_write(vp, uio, ioflag, cred)
	register struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct proc *p = uio->uio_procp;
	register struct inode *ip = VTOI(vp);
	register struct fs *fs;
	struct buf *bp;
	daddr_t lbn, bn;
	u_long osize;
	int n, on, flags;
	int size, resid, error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("ufs_write mode");
#endif
	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		/* fall through */
	case VLNK:
		break;

	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("ufs_write nonsync dir write");
		break;

	default:
		panic("ufs_write type");
	}
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (vp->v_type == VREG && p &&
	    uio->uio_offset + uio->uio_resid >
	      p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}
	resid = uio->uio_resid;
	osize = ip->i_size;
	fs = ip->i_fs;
	flags = 0;
	if (ioflag & IO_SYNC)
		flags = B_SYNC;

	if ((ioflag & IO_PAGER) == 0)
		(void) vnode_pager_uncache(vp);

	do {
		lbn = lblkno(fs, uio->uio_offset);
		on = blkoff(fs, uio->uio_offset);
		n = MIN((unsigned)(fs->fs_bsize - on), uio->uio_resid);
		if (n < fs->fs_bsize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;
		if (error = balloc(ip, lbn, (int)(on + n), &bp, flags))
			break;
		bn = bp->b_blkno;
		if (uio->uio_offset + n > ip->i_size) {
			ip->i_size = uio->uio_offset + n;
			vnode_pager_setsize(vp, ip->i_size);
		}
		size = blksize(fs, ip, lbn);
		n = MIN(n, size - bp->b_resid);
		error = uiomove(bp->b_un.b_addr + on, n, uio);
		if (ioflag & IO_SYNC)
			(void) bwrite(bp);
		else if (n + on == fs->fs_bsize) {
			bp->b_flags |= B_AGE;
			bawrite(bp);
		} else
			bdwrite(bp);
		ip->i_flag |= IUPD|ICHG;
		if (cred->cr_uid != 0)
			ip->i_mode &= ~(ISUID|ISGID);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	if (error == EFAULT || error && (ioflag & IO_UNIT)) {
		(void) itrunc(ip, osize, ioflag & IO_SYNC);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	}
	if (!error && (ioflag & IO_SYNC))
		error = iupdat(ip, &time, &time, 1);
	return (error);
}

/* ARGSUSED */
int
ufs_ioctl(vp, com, data, fflag, cred, p)
	struct vnode *vp;
	int com;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{

	return (ENOTTY);
}

/* ARGSUSED */
int
ufs_select(vp, which, fflags, cred, p)
	struct vnode *vp;
	int which, fflags;
	struct ucred *cred;
	struct proc *p;
{

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (1);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
ufs_mmap(vp, fflags, cred, p)
	struct vnode *vp;
	int fflags;
	struct ucred *cred;
	struct proc *p;
{

	return (EINVAL);
}

/*
 * Synch an open file.
 */
/* ARGSUSED */
int
ufs_fsync(vp, fflags, cred, waitfor, p)
	struct vnode *vp;
	int fflags;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
{
	struct inode *ip = VTOI(vp);

	if (fflags & FWRITE)
		ip->i_flag |= ICHG;
	vflushbuf(vp, waitfor == MNT_WAIT ? B_SYNC : 0);
	return (iupdat(ip, &time, &time, waitfor == MNT_WAIT));
}

/*
 * Seek on a file
 *
 * Nothing to do, so just return.
 */
/* ARGSUSED */
int
ufs_seek(vp, oldoff, newoff, cred)
	struct vnode *vp;
	off_t oldoff, newoff;
	struct ucred *cred;
{

	return (0);
}

/*
 * ufs remove
 * Hard to avoid races here, especially
 * in unlinking directories.
 */
int
ufs_remove(ndp, p)
	struct nameidata *ndp;
	struct proc *p;
{
	register struct inode *ip, *dp;
	int error;

	ip = VTOI(ndp->ni_vp);
	dp = VTOI(ndp->ni_dvp);
	error = dirremove(ndp);
	if (!error) {
		ip->i_nlink--;
		ip->i_flag |= ICHG;
	}
	if (dp == ip)
		vrele(ITOV(ip));
	else
		iput(ip);
	iput(dp);
	return (error);
}

/*
 * link vnode call
 */
int
ufs_link(vp, ndp, p)
	register struct vnode *vp;
	register struct nameidata *ndp;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	int error;

#ifdef DIAGNOSTIC
	if ((ndp->ni_nameiop & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if ((unsigned short)ip->i_nlink >= LINK_MAX) {
		free(ndp->ni_pnbuf, M_NAMEI);
		return (EMLINK);
	}
	if (ndp->ni_dvp != vp)
		ILOCK(ip);
	ip->i_nlink++;
	ip->i_flag |= ICHG;
	error = iupdat(ip, &time, &time, 1);
	if (!error)
		error = direnter(ip, ndp);
	if (ndp->ni_dvp != vp)
		IUNLOCK(ip);
	FREE(ndp->ni_pnbuf, M_NAMEI);
	vput(ndp->ni_dvp);
	if (error) {
		ip->i_nlink--;
		ip->i_flag |= ICHG;
	}
	return (error);
}

/*
 * Rename system call.
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
int
ufs_rename(fndp, tndp, p)
	register struct nameidata *fndp, *tndp;
	struct proc *p;
{
	register struct inode *ip, *xp, *dp;
	struct dirtemplate dirbuf;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;

#ifdef DIAGNOSTIC
	if ((tndp->ni_nameiop & HASBUF) == 0 ||
	    (fndp->ni_nameiop & HASBUF) == 0)
		panic("ufs_rename: no name");
#endif
	dp = VTOI(fndp->ni_dvp);
	ip = VTOI(fndp->ni_vp);
	/*
	 * Check if just deleting a link name.
	 */
	if (fndp->ni_vp == tndp->ni_vp) {
		VOP_ABORTOP(tndp);
		vput(tndp->ni_dvp);
		vput(tndp->ni_vp);
		vrele(fndp->ni_dvp);
		if ((ip->i_mode&IFMT) == IFDIR) {
			VOP_ABORTOP(fndp);
			vrele(fndp->ni_vp);
			return (EINVAL);
		}
		doingdirectory = 0;
		goto unlinkit;
	}
	ILOCK(ip);
	if ((ip->i_mode&IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fndp->ni_namelen == 1 && fndp->ni_ptr[0] == '.') ||
		    dp == ip || fndp->ni_isdotdot || (ip->i_flag & IRENAME)) {
			VOP_ABORTOP(tndp);
			vput(tndp->ni_dvp);
			if (tndp->ni_vp)
				vput(tndp->ni_vp);
			VOP_ABORTOP(fndp);
			vrele(fndp->ni_dvp);
			vput(fndp->ni_vp);
			return (EINVAL);
		}
		ip->i_flag |= IRENAME;
		oldparent = dp->i_number;
		doingdirectory++;
	}
	vrele(fndp->ni_dvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_nlink++;
	ip->i_flag |= ICHG;
	error = iupdat(ip, &time, &time, 1);
	IUNLOCK(ip);

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tndp->ni_dvp);
	xp = NULL;
	if (tndp->ni_vp)
		xp = VTOI(tndp->ni_vp);
	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call 
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		VOP_LOCK(fndp->ni_vp);
		error = ufs_access(fndp->ni_vp, VWRITE, tndp->ni_cred, p);
		VOP_UNLOCK(fndp->ni_vp);
		if (error)
			goto bad;
		if (xp != NULL)
			iput(xp);
		if (error = checkpath(ip, dp, tndp->ni_cred))
			goto out;
		if ((tndp->ni_nameiop & SAVESTART) == 0)
			panic("ufs_rename: lost to startdir");
		if (error = lookup(tndp, p))
			goto out;
		dp = VTOI(tndp->ni_dvp);
		xp = NULL;
		if (tndp->ni_vp)
			xp = VTOI(tndp->ni_vp);
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source. 
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (xp == NULL) {
		if (dp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((unsigned short)dp->i_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_nlink++;
			dp->i_flag |= ICHG;
			if (error = iupdat(dp, &time, &time, 1))
				goto bad;
		}
		if (error = direnter(ip, tndp)) {
			if (doingdirectory && newparent) {
				dp->i_nlink--;
				dp->i_flag |= ICHG;
				(void) iupdat(dp, &time, &time, 1);
			}
			goto bad;
		}
		iput(dp);
	} else {
		if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("rename: same file");
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((dp->i_mode & ISVTX) && tndp->ni_cred->cr_uid != 0 &&
		    tndp->ni_cred->cr_uid != dp->i_uid &&
		    xp->i_uid != tndp->ni_cred->cr_uid) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((xp->i_mode&IFMT) == IFDIR) {
			if (!dirempty(xp, dp->i_number, tndp->ni_cred) || 
			    xp->i_nlink > 2) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(ITOV(dp));
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		if (error = dirrewrite(dp, ip, tndp))
			goto bad;
		/*
		 * If the target directory is in the same
		 * directory as the source directory,
		 * decrement the link count on the parent
		 * of the target directory.
		 */
		 if (doingdirectory && !newparent) {
			dp->i_nlink--;
			dp->i_flag |= ICHG;
		}
		vput(ITOV(dp));
		/*
		 * Adjust the link count of the target to
		 * reflect the dirrewrite above.  If this is
		 * a directory it is empty and there are
		 * no links to it, so we can squash the inode and
		 * any space associated with it.  We disallowed
		 * renaming over top of a directory with links to
		 * it above, as the remaining link would point to
		 * a directory without "." or ".." entries.
		 */
		xp->i_nlink--;
		if (doingdirectory) {
			if (--xp->i_nlink != 0)
				panic("rename: linked directory");
			error = itrunc(xp, (u_long)0, IO_SYNC);
		}
		xp->i_flag |= ICHG;
		iput(xp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
unlinkit:
	fndp->ni_nameiop &= ~MODMASK;
	fndp->ni_nameiop |= LOCKPARENT | LOCKLEAF;
	if ((fndp->ni_nameiop & SAVESTART) == 0)
		panic("ufs_rename: lost from startdir");
	(void) lookup(fndp, p);
	if (fndp->ni_vp != NULL) {
		xp = VTOI(fndp->ni_vp);
		dp = VTOI(fndp->ni_dvp);
	} else {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("rename: lost dir entry");
		vrele(ITOV(ip));
		return (0);
	}
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; its link
	 * count of three would cause a rmdir to fail with ENOTEMPTY.
	 * The IRENAME flag ensures that it cannot be moved by another
	 * rename.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			dp->i_nlink--;
			dp->i_flag |= ICHG;
			error = vn_rdwr(UIO_READ, ITOV(xp), (caddr_t)&dirbuf,
				sizeof (struct dirtemplate), (off_t)0,
				UIO_SYSSPACE, IO_NODELOCKED, 
				tndp->ni_cred, (int *)0, (struct proc *)0);
			if (error == 0) {
				if (dirbuf.dotdot_namlen != 2 ||
				    dirbuf.dotdot_name[0] != '.' ||
				    dirbuf.dotdot_name[1] != '.') {
					dirbad(xp, 12, "rename: mangled dir");
				} else {
					dirbuf.dotdot_ino = newparent;
					(void) vn_rdwr(UIO_WRITE, ITOV(xp),
					    (caddr_t)&dirbuf,
					    sizeof (struct dirtemplate),
					    (off_t)0, UIO_SYSSPACE,
					    IO_NODELOCKED|IO_SYNC,
					    tndp->ni_cred, (int *)0,
					    (struct proc *)0);
					cache_purge(ITOV(dp));
				}
			}
		}
		error = dirremove(fndp);
		if (!error) {
			xp->i_nlink--;
			xp->i_flag |= ICHG;
		}
		xp->i_flag &= ~IRENAME;
	}
	if (dp)
		vput(ITOV(dp));
	if (xp)
		vput(ITOV(xp));
	vrele(ITOV(ip));
	return (error);

bad:
	if (xp)
		vput(ITOV(xp));
	vput(ITOV(dp));
out:
	ip->i_nlink--;
	ip->i_flag |= ICHG;
	vrele(ITOV(ip));
	return (error);
}

/*
 * A virgin directory (no blushing please).
 */
struct dirtemplate mastertemplate = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

/*
 * Mkdir system call
 */
int
ufs_mkdir(ndp, vap, p)
	struct nameidata *ndp;
	struct vattr *vap;
	struct proc *p;
{
	register struct inode *ip, *dp;
	struct inode *tip;
	struct vnode *dvp;
	struct dirtemplate dirtemplate;
	int error;
	int dmode;

#ifdef DIAGNOSTIC
	if ((ndp->ni_nameiop & HASBUF) == 0)
		panic("ufs_mkdir: no name");
#endif
	dvp = ndp->ni_dvp;
	dp = VTOI(dvp);
	if ((unsigned short)dp->i_nlink >= LINK_MAX) {
		free(ndp->ni_pnbuf, M_NAMEI);
		iput(dp);
		return (EMLINK);
	}
	dmode = vap->va_mode&0777;
	dmode |= IFDIR;
	/*
	 * Must simulate part of maknode here to acquire the inode, but
	 * not have it entered in the parent directory. The entry is made
	 * later after writing "." and ".." entries.
	 */
	if (error = ialloc(dp, dirpref(dp->i_fs), dmode, ndp->ni_cred, &tip)) {
		free(ndp->ni_pnbuf, M_NAMEI);
		iput(dp);
		return (error);
	}
	ip = tip;
	ip->i_uid = ndp->ni_cred->cr_uid;
	ip->i_gid = dp->i_gid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, ndp->ni_cred, 0))) {
		free(ndp->ni_pnbuf, M_NAMEI);
		ifree(ip, ip->i_number, dmode);
		iput(ip);
		iput(dp);
		return (error);
	}
#endif
	ip->i_flag |= IACC|IUPD|ICHG;
	ip->i_mode = dmode;
	ITOV(ip)->v_type = VDIR;	/* Rest init'd in iget() */
	ip->i_nlink = 2;
	error = iupdat(ip, &time, &time, 1);

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	dp->i_nlink++;
	dp->i_flag |= ICHG;
	if (error = iupdat(dp, &time, &time, 1))
		goto bad;

	/*
	 * Initialize directory with "."
	 * and ".." from static template.
	 */
	dirtemplate = mastertemplate;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;
	error = vn_rdwr(UIO_WRITE, ITOV(ip), (caddr_t)&dirtemplate,
	    sizeof (dirtemplate), (off_t)0, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_SYNC, ndp->ni_cred, (int *)0, (struct proc *)0);
	if (error) {
		dp->i_nlink--;
		dp->i_flag |= ICHG;
		goto bad;
	}
	if (DIRBLKSIZ > dp->i_fs->fs_fsize) {
		panic("mkdir: blksize");     /* XXX - should grow w/balloc() */
	} else {
		ip->i_size = DIRBLKSIZ;
		ip->i_flag |= ICHG;
	}
	/*
	 * Directory all set up, now
	 * install the entry for it in
	 * the parent directory.
	 */
	if (error = direnter(ip, ndp)) {
		dp->i_nlink--;
		dp->i_flag |= ICHG;
	}
bad:
	/*
	 * No need to do an explicit itrunc here,
	 * vrele will do this for us because we set
	 * the link count to 0.
	 */
	if (error) {
		ip->i_nlink = 0;
		ip->i_flag |= ICHG;
		iput(ip);
	} else
		ndp->ni_vp = ITOV(ip);
	FREE(ndp->ni_pnbuf, M_NAMEI);
	iput(dp);
	return (error);
}

/*
 * Rmdir system call.
 */
int
ufs_rmdir(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	register struct inode *ip, *dp;
	int error = 0;

	ip = VTOI(ndp->ni_vp);
	dp = VTOI(ndp->ni_dvp);
	/*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		vrele(ITOV(dp));
		iput(ip);
		return (EINVAL);
	}
	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	if (ip->i_nlink != 2 || !dirempty(ip, dp->i_number, ndp->ni_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	if (error = dirremove(ndp))
		goto out;
	dp->i_nlink--;
	dp->i_flag |= ICHG;
	cache_purge(ITOV(dp));
	iput(dp);
	ndp->ni_dvp = NULL;
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
	ip->i_nlink -= 2;
	error = itrunc(ip, (u_long)0, IO_SYNC);
	cache_purge(ITOV(ip));
out:
	if (ndp->ni_dvp)
		iput(dp);
	iput(ip);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
ufs_symlink(ndp, vap, target, p)
	struct nameidata *ndp;
	struct vattr *vap;
	char *target;
	struct proc *p;
{
	struct inode *ip;
	int len = strlen(target);
	int error;

	error = maknode(IFLNK | vap->va_mode, ndp, &ip);
	if (error)
		return (error);
#ifdef FASTLINKS
	if (len <= MAXFASTLINK) {
		ip->i_din.di_spare[0] = len;
		ip->i_size = len;
		bcopy(target, ip->i_symlink, len);
		ip->i_flag |= ICHG;
		error = iupdat(ip, &time, &time, 1);
	} else
#endif
	error = vn_rdwr(UIO_WRITE, ITOV(ip), target, len, (off_t)0,
		UIO_SYSSPACE, IO_NODELOCKED, ndp->ni_cred, (int *)0,
		(struct proc *)0);
	iput(ip);
	return (error);
}

/*
 * Vnode op for read and write
 */
int
ufs_readdir(vp, uio, cred, eofflagp)
	struct vnode *vp;
	register struct uio *uio;
	struct ucred *cred;
	int *eofflagp;
{
	int count, lost, error;

	count = uio->uio_resid;
	count &= ~(DIRBLKSIZ - 1);
	lost = uio->uio_resid - count;
	if (count < DIRBLKSIZ || (uio->uio_offset & (DIRBLKSIZ -1)))
		return (EINVAL);
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
	error = ufs_read(vp, uio, 0, cred);
	uio->uio_resid += lost;
	if ((VTOI(vp)->i_size - uio->uio_offset) <= 0)
		*eofflagp = 1;
	else
		*eofflagp = 0;
	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
ufs_readlink(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	struct inode *ip = VTOI(vp);
	if (FASTLINK(ip))
		return (uiomove(ip->i_symlink, ip->i_size, uiop));
	else
		return (ufs_read(vp, uiop, 0, cred));
}

/*
 * Ufs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. If a buffer has been saved in anticipation of a CREATE, delete it.
 */
/* ARGSUSED */
int
ufs_abortop(ndp)
	struct nameidata *ndp;
{

	if ((ndp->ni_nameiop & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ndp->ni_pnbuf, M_NAMEI);
	return (0);
}

/*
 * Lock an inode.
 */
int
ufs_lock(vp)
	struct vnode *vp;
{
	register struct inode *ip = VTOI(vp);

	ILOCK(ip);
	return (0);
}

/*
 * Unlock an inode.
 */
int
ufs_unlock(vp)
	struct vnode *vp;
{
	register struct inode *ip = VTOI(vp);

	if (!(ip->i_flag & ILOCKED))
		panic("ufs_unlock NOT LOCKED");
	IUNLOCK(ip);
	return (0);
}

/*
 * Check for a locked inode.
 */
int
ufs_islocked(vp)
	struct vnode *vp;
{

	if (VTOI(vp)->i_flag & ILOCKED)
		return (1);
	return (0);
}

/*
 * Get access to bmap
 */
int
ufs_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{
	struct inode *ip = VTOI(vp);

	if (vpp != NULL)
		*vpp = ip->i_devvp;
	if (bnp == NULL)
		return (0);
	return (bmap(ip, bn, bnp));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int checkoverlap = 0;

int
ufs_strategy(bp)
	register struct buf *bp;
{
	register struct inode *ip = VTOI(bp->b_vp);
	struct vnode *vp;
	int error;

	if (bp->b_vp->v_type == VBLK || bp->b_vp->v_type == VCHR)
		panic("ufs_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		if (error = bmap(ip, bp->b_lblkno, &bp->b_blkno))
			return (error);
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
#ifdef DIAGNOSTIC
	if (checkoverlap) {
		register struct buf *ep;
		struct buf *ebp;
		daddr_t start, last;

		ebp = &buf[nbuf];
		start = bp->b_blkno;
		last = start + btodb(bp->b_bcount) - 1;
		for (ep = buf; ep < ebp; ep++) {
			if (ep == bp || (ep->b_flags & B_INVAL) ||
			    ep->b_vp == NULLVP)
				continue;
			if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0))
				continue;
			if (vp != ip->i_devvp)
				continue;
			/* look for overlap */
			if (ep->b_bcount == 0 || ep->b_blkno > last ||
			    ep->b_blkno + btodb(ep->b_bcount) <= start)
				continue;
			vprint("Disk overlap", vp);
			printf("\tstart %d, end %d overlap start %d, end %d\n",
				start, last, ep->b_blkno,
				ep->b_blkno + btodb(ep->b_bcount) - 1);
			panic("Disk buffer overlap");
		}
	}
#endif /* DIAGNOSTIC */
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	(*(vp->v_op->vop_strategy))(bp);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
void
ufs_print(vp)
	struct vnode *vp;
{
	register struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %d, on dev %d, %d", ip->i_number,
		major(ip->i_dev), minor(ip->i_dev));
#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif /* FIFO */
	printf("%s\n", (ip->i_flag & ILOCKED) ? " (LOCKED)" : "");
	if (ip->i_spare0 == 0)
		return;
	printf("\towner pid %d", ip->i_spare0);
	if (ip->i_spare1)
		printf(" waiting pid %d", ip->i_spare1);
	printf("\n");
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	/*
	 * Set access flag.
	 */
	VTOI(vp)->i_flag |= IACC;
	return (spec_read(vp, uio, ioflag, cred));
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	/*
	 * Set update and change flags.
	 */
	VTOI(vp)->i_flag |= IUPD|ICHG;
	return (spec_write(vp, uio, ioflag, cred));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(vp, fflag, cred, p)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);

	if (vp->v_usecount > 1 && !(ip->i_flag & ILOCKED))
		ITIMES(ip, &time, &time);
	return (spec_close(vp, fflag, cred, p));
}

#ifdef FIFO
/*
 * Read wrapper for fifo's
 */
int
ufsfifo_read(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	/*
	 * Set access flag.
	 */
	VTOI(vp)->i_flag |= IACC;
	return (fifo_read(vp, uio, ioflag, cred));
}

/*
 * Write wrapper for fifo's.
 */
int
ufsfifo_write(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{

	/*
	 * Set update and change flags.
	 */
	VTOI(vp)->i_flag |= IUPD|ICHG;
	return (fifo_write(vp, uio, ioflag, cred));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(vp, fflag, cred, p)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);

	if (vp->v_usecount > 1 && !(ip->i_flag & ILOCKED))
		ITIMES(ip, &time, &time);
	return (fifo_close(vp, fflag, cred, p));
}
#endif /* FIFO */

/*
 * Allocate a new inode.
 */
static int
maknode(mode, ndp, ipp)
	int mode;
	register struct nameidata *ndp;
	struct inode **ipp;
{
	register struct inode *ip;
	struct inode *tip;
	register struct inode *pdir = VTOI(ndp->ni_dvp);
	ino_t ipref;
	int error;

#ifdef DIAGNOSTIC
	if ((ndp->ni_nameiop & HASBUF) == 0)
		panic("maknode: no name");
#endif
	*ipp = 0;
	if ((mode & IFMT) == 0)
		mode |= IFREG;
	if ((mode & IFMT) == IFDIR)
		ipref = dirpref(pdir->i_fs);
	else
		ipref = pdir->i_number;
	if (error = ialloc(pdir, ipref, mode, ndp->ni_cred, &tip)) {
		free(ndp->ni_pnbuf, M_NAMEI);
		iput(pdir);
		return (error);
	}
	ip = tip;
	ip->i_uid = ndp->ni_cred->cr_uid;
	ip->i_gid = pdir->i_gid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, ndp->ni_cred, 0))) {
		free(ndp->ni_pnbuf, M_NAMEI);
		ifree(ip, ip->i_number, mode);
		iput(ip);
		iput(pdir);
		return (error);
	}
#endif
	ip->i_flag |= IACC|IUPD|ICHG;
	ip->i_mode = mode;
	ITOV(ip)->v_type = IFTOVT(mode);	/* Rest init'd in iget() */
	ip->i_nlink = 1;
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, ndp->ni_cred) &&
	    suser(ndp->ni_cred, NULL))
		ip->i_mode &= ~ISGID;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	if (error = iupdat(ip, &time, &time, 1))
		goto bad;
	if (error = direnter(ip, ndp))
		goto bad;
	if ((ndp->ni_nameiop & SAVESTART) == 0)
		FREE(ndp->ni_pnbuf, M_NAMEI);
	iput(pdir);
	*ipp = ip;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	free(ndp->ni_pnbuf, M_NAMEI);
	iput(pdir);
	ip->i_nlink = 0;
	ip->i_flag |= ICHG;
	iput(ip);
	return (error);
}

/*
 * Advisory record locking support
 */
int
ufs_advlock(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	register struct flock *fl;
	int flags;
{
	register struct inode *ip = VTOI(vp);

	return (lf_advlock(&(ip->i_lockf), ip->i_size, id, op, fl, flags));
}

/*
 * Global vfs data structures for ufs
 */
struct vnodeops ufs_vnodeops = {
	ufs_lookup,		/* lookup */
	ufs_create,		/* create */
	ufs_mknod,		/* mknod */
	ufs_open,		/* open */
	ufs_close,		/* close */
	ufs_access,		/* access */
	ufs_getattr,		/* getattr */
	ufs_setattr,		/* setattr */
	ufs_read,		/* read */
	ufs_write,		/* write */
	ufs_ioctl,		/* ioctl */
	ufs_select,		/* select */
	ufs_mmap,		/* mmap */
	ufs_fsync,		/* fsync */
	ufs_seek,		/* seek */
	ufs_remove,		/* remove */
	ufs_link,		/* link */
	ufs_rename,		/* rename */
	ufs_mkdir,		/* mkdir */
	ufs_rmdir,		/* rmdir */
	ufs_symlink,		/* symlink */
	ufs_readdir,		/* readdir */
	ufs_readlink,		/* readlink */
	ufs_abortop,		/* abortop */
	ufs_inactive,		/* inactive */
	ufs_reclaim,		/* reclaim */
	ufs_lock,		/* lock */
	ufs_unlock,		/* unlock */
	ufs_bmap,		/* bmap */
	ufs_strategy,		/* strategy */
	ufs_print,		/* print */
	ufs_islocked,		/* islocked */
	ufs_advlock,		/* advlock */
};

struct vnodeops spec_inodeops = {
	spec_lookup,		/* lookup */
	spec_create,		/* create */
	spec_mknod,		/* mknod */
	spec_open,		/* open */
	ufsspec_close,		/* close */
	ufs_access,		/* access */
	ufs_getattr,		/* getattr */
	ufs_setattr,		/* setattr */
	ufsspec_read,		/* read */
	ufsspec_write,		/* write */
	spec_ioctl,		/* ioctl */
	spec_select,		/* select */
	spec_mmap,		/* mmap */
	spec_fsync,		/* fsync */
	spec_seek,		/* seek */
	spec_remove,		/* remove */
	spec_link,		/* link */
	spec_rename,		/* rename */
	spec_mkdir,		/* mkdir */
	spec_rmdir,		/* rmdir */
	spec_symlink,		/* symlink */
	spec_readdir,		/* readdir */
	spec_readlink,		/* readlink */
	spec_abortop,		/* abortop */
	ufs_inactive,		/* inactive */
	ufs_reclaim,		/* reclaim */
	ufs_lock,		/* lock */
	ufs_unlock,		/* unlock */
	spec_bmap,		/* bmap */
	spec_strategy,		/* strategy */
	ufs_print,		/* print */
	ufs_islocked,		/* islocked */
	spec_advlock,		/* advlock */
};

#ifdef FIFO
struct vnodeops fifo_inodeops = {
	fifo_lookup,		/* lookup */
	fifo_create,		/* create */
	fifo_mknod,		/* mknod */
	fifo_open,		/* open */
	ufsfifo_close,		/* close */
	ufs_access,		/* access */
	ufs_getattr,		/* getattr */
	ufs_setattr,		/* setattr */
	ufsfifo_read,		/* read */
	ufsfifo_write,		/* write */
	fifo_ioctl,		/* ioctl */
	fifo_select,		/* select */
	fifo_mmap,		/* mmap */
	fifo_fsync,		/* fsync */
	fifo_seek,		/* seek */
	fifo_remove,		/* remove */
	fifo_link,		/* link */
	fifo_rename,		/* rename */
	fifo_mkdir,		/* mkdir */
	fifo_rmdir,		/* rmdir */
	fifo_symlink,		/* symlink */
	fifo_readdir,		/* readdir */
	fifo_readlink,		/* readlink */
	fifo_abortop,		/* abortop */
	ufs_inactive,		/* inactive */
	ufs_reclaim,		/* reclaim */
	ufs_lock,		/* lock */
	ufs_unlock,		/* unlock */
	fifo_bmap,		/* bmap */
	fifo_strategy,		/* strategy */
	ufs_print,		/* print */
	ufs_islocked,		/* islocked */
	fifo_advlock,		/* advlock */
};
#endif /* FIFO */

enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};
int	vttoif_tab[9] = {
	0, IFREG, IFDIR, IFBLK, IFCHR, IFLNK, IFSOCK, IFIFO, IFMT,
};
