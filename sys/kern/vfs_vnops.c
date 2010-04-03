/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)vfs_vnops.c	8.2 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/kdb.h>
#include <sys/stat.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/filio.h>
#include <sys/sx.h>
#include <sys/ttycom.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/unistd.h>

#include <security/mac/mac_framework.h>

static fo_rdwr_t	vn_read;
static fo_rdwr_t	vn_write;
static fo_truncate_t	vn_truncate;
static fo_ioctl_t	vn_ioctl;
static fo_poll_t	vn_poll;
static fo_kqfilter_t	vn_kqfilter;
static fo_stat_t	vn_statfile;
static fo_close_t	vn_closefile;

struct 	fileops vnops = {
	.fo_read = vn_read,
	.fo_write = vn_write,
	.fo_truncate = vn_truncate,
	.fo_ioctl = vn_ioctl,
	.fo_poll = vn_poll,
	.fo_kqfilter = vn_kqfilter,
	.fo_stat = vn_statfile,
	.fo_close = vn_closefile,
	.fo_flags = DFLAG_PASSABLE | DFLAG_SEEKABLE
};

int
vn_open(ndp, flagp, cmode, fp)
	struct nameidata *ndp;
	int *flagp, cmode;
	struct file *fp;
{
	struct thread *td = ndp->ni_cnd.cn_thread;

	return (vn_open_cred(ndp, flagp, cmode, 0, td->td_ucred, fp));
}

/*
 * Common code for vnode open operations.
 * Check permissions, and call the VOP_OPEN or VOP_CREATE routine.
 * 
 * Note that this does NOT free nameidata for the successful case,
 * due to the NDINIT being done elsewhere.
 */
int
vn_open_cred(struct nameidata *ndp, int *flagp, int cmode, u_int vn_open_flags,
    struct ucred *cred, struct file *fp)
{
	struct vnode *vp;
	struct mount *mp;
	struct thread *td = ndp->ni_cnd.cn_thread;
	struct vattr vat;
	struct vattr *vap = &vat;
	int fmode, error;
	accmode_t accmode;
	int vfslocked, mpsafe;

	mpsafe = ndp->ni_cnd.cn_flags & MPSAFE;
restart:
	vfslocked = 0;
	fmode = *flagp;
	if (fmode & O_CREAT) {
		ndp->ni_cnd.cn_nameiop = CREATE;
		ndp->ni_cnd.cn_flags = ISOPEN | LOCKPARENT | LOCKLEAF |
		    MPSAFE;
		if ((fmode & O_EXCL) == 0 && (fmode & O_NOFOLLOW) == 0)
			ndp->ni_cnd.cn_flags |= FOLLOW;
		if (!(vn_open_flags & VN_OPEN_NOAUDIT))
			ndp->ni_cnd.cn_flags |= AUDITVNODE1;
		bwillwrite();
		if ((error = namei(ndp)) != 0)
			return (error);
		vfslocked = NDHASGIANT(ndp);
		if (!mpsafe)
			ndp->ni_cnd.cn_flags &= ~MPSAFE;
		if (ndp->ni_vp == NULL) {
			VATTR_NULL(vap);
			vap->va_type = VREG;
			vap->va_mode = cmode;
			if (fmode & O_EXCL)
				vap->va_vaflags |= VA_EXCLUSIVE;
			if (vn_start_write(ndp->ni_dvp, &mp, V_NOWAIT) != 0) {
				NDFREE(ndp, NDF_ONLY_PNBUF);
				vput(ndp->ni_dvp);
				VFS_UNLOCK_GIANT(vfslocked);
				if ((error = vn_start_write(NULL, &mp,
				    V_XSLEEP | PCATCH)) != 0)
					return (error);
				goto restart;
			}
#ifdef MAC
			error = mac_vnode_check_create(cred, ndp->ni_dvp,
			    &ndp->ni_cnd, vap);
			if (error == 0)
#endif
				error = VOP_CREATE(ndp->ni_dvp, &ndp->ni_vp,
						   &ndp->ni_cnd, vap);
			vput(ndp->ni_dvp);
			vn_finished_write(mp);
			if (error) {
				VFS_UNLOCK_GIANT(vfslocked);
				NDFREE(ndp, NDF_ONLY_PNBUF);
				return (error);
			}
			fmode &= ~O_TRUNC;
			vp = ndp->ni_vp;
		} else {
			if (ndp->ni_dvp == ndp->ni_vp)
				vrele(ndp->ni_dvp);
			else
				vput(ndp->ni_dvp);
			ndp->ni_dvp = NULL;
			vp = ndp->ni_vp;
			if (fmode & O_EXCL) {
				error = EEXIST;
				goto bad;
			}
			fmode &= ~O_CREAT;
		}
	} else {
		ndp->ni_cnd.cn_nameiop = LOOKUP;
		ndp->ni_cnd.cn_flags = ISOPEN |
		    ((fmode & O_NOFOLLOW) ? NOFOLLOW : FOLLOW) |
		    LOCKLEAF | MPSAFE;
		if (!(fmode & FWRITE))
			ndp->ni_cnd.cn_flags |= LOCKSHARED;
		if (!(vn_open_flags & VN_OPEN_NOAUDIT))
			ndp->ni_cnd.cn_flags |= AUDITVNODE1;
		if ((error = namei(ndp)) != 0)
			return (error);
		if (!mpsafe)
			ndp->ni_cnd.cn_flags &= ~MPSAFE;
		vfslocked = NDHASGIANT(ndp);
		vp = ndp->ni_vp;
	}
	if (vp->v_type == VLNK) {
		error = EMLINK;
		goto bad;
	}
	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if (vp->v_type != VDIR && fmode & O_DIRECTORY) {
		error = ENOTDIR;
		goto bad;
	}
	accmode = 0;
	if (fmode & (FWRITE | O_TRUNC)) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto bad;
		}
		accmode |= VWRITE;
	}
	if (fmode & FREAD)
		accmode |= VREAD;
	if (fmode & FEXEC)
		accmode |= VEXEC;
	if ((fmode & O_APPEND) && (fmode & FWRITE))
		accmode |= VAPPEND;
#ifdef MAC
	error = mac_vnode_check_open(cred, vp, accmode);
	if (error)
		goto bad;
#endif
	if ((fmode & O_CREAT) == 0) {
		if (accmode & VWRITE) {
			error = vn_writechk(vp);
			if (error)
				goto bad;
		}
		if (accmode) {
		        error = VOP_ACCESS(vp, accmode, cred, td);
			if (error)
				goto bad;
		}
	}
	if ((error = VOP_OPEN(vp, fmode, cred, td, fp)) != 0)
		goto bad;

	if (fmode & FWRITE)
		vp->v_writecount++;
	*flagp = fmode;
	ASSERT_VOP_LOCKED(vp, "vn_open_cred");
	if (!mpsafe)
		VFS_UNLOCK_GIANT(vfslocked);
	return (0);
bad:
	NDFREE(ndp, NDF_ONLY_PNBUF);
	vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
	*flagp = fmode;
	ndp->ni_vp = NULL;
	return (error);
}

/*
 * Check for write permissions on the specified vnode.
 * Prototype text segments cannot be written.
 */
int
vn_writechk(vp)
	register struct vnode *vp;
{

	ASSERT_VOP_LOCKED(vp, "vn_writechk");
	/*
	 * If there's shared text associated with
	 * the vnode, try to free it up once.  If
	 * we fail, we can't allow writing.
	 */
	if (vp->v_vflag & VV_TEXT)
		return (ETXTBSY);

	return (0);
}

/*
 * Vnode close call
 */
int
vn_close(vp, flags, file_cred, td)
	register struct vnode *vp;
	int flags;
	struct ucred *file_cred;
	struct thread *td;
{
	struct mount *mp;
	int error, lock_flags;

	if (!(flags & FWRITE) && vp->v_mount != NULL &&
	    vp->v_mount->mnt_kern_flag & MNTK_EXTENDED_SHARED)
		lock_flags = LK_SHARED;
	else
		lock_flags = LK_EXCLUSIVE;

	VFS_ASSERT_GIANT(vp->v_mount);

	vn_start_write(vp, &mp, V_WAIT);
	vn_lock(vp, lock_flags | LK_RETRY);
	if (flags & FWRITE) {
		VNASSERT(vp->v_writecount > 0, vp, 
		    ("vn_close: negative writecount"));
		vp->v_writecount--;
	}
	error = VOP_CLOSE(vp, flags, file_cred, td);
	vput(vp);
	vn_finished_write(mp);
	return (error);
}

/*
 * Heuristic to detect sequential operation.
 */
static int
sequential_heuristic(struct uio *uio, struct file *fp)
{

	if (atomic_load_acq_int(&(fp->f_flag)) & FRDAHEAD)
		return (fp->f_seqcount << IO_SEQSHIFT);

	/*
	 * Offset 0 is handled specially.  open() sets f_seqcount to 1 so
	 * that the first I/O is normally considered to be slightly
	 * sequential.  Seeking to offset 0 doesn't change sequentiality
	 * unless previous seeks have reduced f_seqcount to 0, in which
	 * case offset 0 is not special.
	 */
	if ((uio->uio_offset == 0 && fp->f_seqcount > 0) ||
	    uio->uio_offset == fp->f_nextoff) {
		/*
		 * f_seqcount is in units of fixed-size blocks so that it
		 * depends mainly on the amount of sequential I/O and not
		 * much on the number of sequential I/O's.  The fixed size
		 * of 16384 is hard-coded here since it is (not quite) just
		 * a magic size that works well here.  This size is more
		 * closely related to the best I/O size for real disks than
		 * to any block size used by software.
		 */
		fp->f_seqcount += howmany(uio->uio_resid, 16384);
		if (fp->f_seqcount > IO_SEQMAX)
			fp->f_seqcount = IO_SEQMAX;
		return (fp->f_seqcount << IO_SEQSHIFT);
	}

	/* Not sequential.  Quickly draw-down sequentiality. */
	if (fp->f_seqcount > 1)
		fp->f_seqcount = 1;
	else
		fp->f_seqcount = 0;
	return (0);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.
 */
int
vn_rdwr(rw, vp, base, len, offset, segflg, ioflg, active_cred, file_cred,
    aresid, td)
	enum uio_rw rw;
	struct vnode *vp;
	void *base;
	int len;
	off_t offset;
	enum uio_seg segflg;
	int ioflg;
	struct ucred *active_cred;
	struct ucred *file_cred;
	int *aresid;
	struct thread *td;
{
	struct uio auio;
	struct iovec aiov;
	struct mount *mp;
	struct ucred *cred;
	int error, lock_flags;

	VFS_ASSERT_GIANT(vp->v_mount);

	if ((ioflg & IO_NODELOCKED) == 0) {
		mp = NULL;
		if (rw == UIO_WRITE) { 
			if (vp->v_type != VCHR &&
			    (error = vn_start_write(vp, &mp, V_WAIT | PCATCH))
			    != 0)
				return (error);
			if (MNT_SHARED_WRITES(mp) ||
			    ((mp == NULL) && MNT_SHARED_WRITES(vp->v_mount))) {
				lock_flags = LK_SHARED;
			} else {
				lock_flags = LK_EXCLUSIVE;
			}
			vn_lock(vp, lock_flags | LK_RETRY);
		} else
			vn_lock(vp, LK_SHARED | LK_RETRY);

	}
	ASSERT_VOP_LOCKED(vp, "IO_NODELOCKED with no vp lock held");
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_resid = len;
	auio.uio_offset = offset;
	auio.uio_segflg = segflg;
	auio.uio_rw = rw;
	auio.uio_td = td;
	error = 0;
#ifdef MAC
	if ((ioflg & IO_NOMACCHECK) == 0) {
		if (rw == UIO_READ)
			error = mac_vnode_check_read(active_cred, file_cred,
			    vp);
		else
			error = mac_vnode_check_write(active_cred, file_cred,
			    vp);
	}
#endif
	if (error == 0) {
		if (file_cred)
			cred = file_cred;
		else
			cred = active_cred;
		if (rw == UIO_READ)
			error = VOP_READ(vp, &auio, ioflg, cred);
		else
			error = VOP_WRITE(vp, &auio, ioflg, cred);
	}
	if (aresid)
		*aresid = auio.uio_resid;
	else
		if (auio.uio_resid && error == 0)
			error = EIO;
	if ((ioflg & IO_NODELOCKED) == 0) {
		if (rw == UIO_WRITE && vp->v_type != VCHR)
			vn_finished_write(mp);
		VOP_UNLOCK(vp, 0);
	}
	return (error);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.  The I/O
 * request is split up into smaller chunks and we try to avoid saturating
 * the buffer cache while potentially holding a vnode locked, so we 
 * check bwillwrite() before calling vn_rdwr().  We also call uio_yield()
 * to give other processes a chance to lock the vnode (either other processes
 * core'ing the same binary, or unrelated processes scanning the directory).
 */
int
vn_rdwr_inchunks(rw, vp, base, len, offset, segflg, ioflg, active_cred,
    file_cred, aresid, td)
	enum uio_rw rw;
	struct vnode *vp;
	void *base;
	size_t len;
	off_t offset;
	enum uio_seg segflg;
	int ioflg;
	struct ucred *active_cred;
	struct ucred *file_cred;
	size_t *aresid;
	struct thread *td;
{
	int error = 0;
	int iaresid;

	VFS_ASSERT_GIANT(vp->v_mount);

	do {
		int chunk;

		/*
		 * Force `offset' to a multiple of MAXBSIZE except possibly
		 * for the first chunk, so that filesystems only need to
		 * write full blocks except possibly for the first and last
		 * chunks.
		 */
		chunk = MAXBSIZE - (uoff_t)offset % MAXBSIZE;

		if (chunk > len)
			chunk = len;
		if (rw != UIO_READ && vp->v_type == VREG)
			bwillwrite();
		iaresid = 0;
		error = vn_rdwr(rw, vp, base, chunk, offset, segflg,
		    ioflg, active_cred, file_cred, &iaresid, td);
		len -= chunk;	/* aresid calc already includes length */
		if (error)
			break;
		offset += chunk;
		base = (char *)base + chunk;
		uio_yield();
	} while (len);
	if (aresid)
		*aresid = len + iaresid;
	return (error);
}

/*
 * File table vnode read routine.
 */
static int
vn_read(fp, uio, active_cred, flags, td)
	struct file *fp;
	struct uio *uio;
	struct ucred *active_cred;
	struct thread *td;
	int flags;
{
	struct vnode *vp;
	int error, ioflag;
	struct mtx *mtxp;
	int vfslocked;

	KASSERT(uio->uio_td == td, ("uio_td %p is not td %p",
	    uio->uio_td, td));
	mtxp = NULL;
	vp = fp->f_vnode;
	ioflag = 0;
	if (fp->f_flag & FNONBLOCK)
		ioflag |= IO_NDELAY;
	if (fp->f_flag & O_DIRECT)
		ioflag |= IO_DIRECT;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	/*
	 * According to McKusick the vn lock was protecting f_offset here.
	 * It is now protected by the FOFFSET_LOCKED flag.
	 */
	if ((flags & FOF_OFFSET) == 0) {
		mtxp = mtx_pool_find(mtxpool_sleep, fp);
		mtx_lock(mtxp);
		while(fp->f_vnread_flags & FOFFSET_LOCKED) {
			fp->f_vnread_flags |= FOFFSET_LOCK_WAITING;
			msleep(&fp->f_vnread_flags, mtxp, PUSER -1,
			    "vnread offlock", 0);
		}
		fp->f_vnread_flags |= FOFFSET_LOCKED;
		mtx_unlock(mtxp);
		vn_lock(vp, LK_SHARED | LK_RETRY);
		uio->uio_offset = fp->f_offset;
	} else
		vn_lock(vp, LK_SHARED | LK_RETRY);

	ioflag |= sequential_heuristic(uio, fp);

#ifdef MAC
	error = mac_vnode_check_read(active_cred, fp->f_cred, vp);
	if (error == 0)
#endif
		error = VOP_READ(vp, uio, ioflag, fp->f_cred);
	if ((flags & FOF_OFFSET) == 0) {
		fp->f_offset = uio->uio_offset;
		mtx_lock(mtxp);
		if (fp->f_vnread_flags & FOFFSET_LOCK_WAITING)
			wakeup(&fp->f_vnread_flags);
		fp->f_vnread_flags = 0;
		mtx_unlock(mtxp);
	}
	fp->f_nextoff = uio->uio_offset;
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * File table vnode write routine.
 */
static int
vn_write(fp, uio, active_cred, flags, td)
	struct file *fp;
	struct uio *uio;
	struct ucred *active_cred;
	struct thread *td;
	int flags;
{
	struct vnode *vp;
	struct mount *mp;
	int error, ioflag, lock_flags;
	int vfslocked;

	KASSERT(uio->uio_td == td, ("uio_td %p is not td %p",
	    uio->uio_td, td));
	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if (vp->v_type == VREG)
		bwillwrite();
	ioflag = IO_UNIT;
	if (vp->v_type == VREG && (fp->f_flag & O_APPEND))
		ioflag |= IO_APPEND;
	if (fp->f_flag & FNONBLOCK)
		ioflag |= IO_NDELAY;
	if (fp->f_flag & O_DIRECT)
		ioflag |= IO_DIRECT;
	if ((fp->f_flag & O_FSYNC) ||
	    (vp->v_mount && (vp->v_mount->mnt_flag & MNT_SYNCHRONOUS)))
		ioflag |= IO_SYNC;
	mp = NULL;
	if (vp->v_type != VCHR &&
	    (error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto unlock;
 
	if ((MNT_SHARED_WRITES(mp) ||
	    ((mp == NULL) && MNT_SHARED_WRITES(vp->v_mount))) &&
	    (flags & FOF_OFFSET) != 0) {
		lock_flags = LK_SHARED;
	} else {
		lock_flags = LK_EXCLUSIVE;
	}

	vn_lock(vp, lock_flags | LK_RETRY);
	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;
	ioflag |= sequential_heuristic(uio, fp);
#ifdef MAC
	error = mac_vnode_check_write(active_cred, fp->f_cred, vp);
	if (error == 0)
#endif
		error = VOP_WRITE(vp, uio, ioflag, fp->f_cred);
	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	VOP_UNLOCK(vp, 0);
	if (vp->v_type != VCHR)
		vn_finished_write(mp);
unlock:
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * File table truncate routine.
 */
static int
vn_truncate(fp, length, active_cred, td)
	struct file *fp;
	off_t length;
	struct ucred *active_cred;
	struct thread *td;
{
	struct vattr vattr;
	struct mount *mp;
	struct vnode *vp;
	int vfslocked;
	int error;

	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error) {
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}
#ifdef MAC
	error = mac_vnode_check_write(active_cred, fp->f_cred, vp);
	if (error)
		goto out;
#endif
	error = vn_writechk(vp);
	if (error == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = length;
		error = VOP_SETATTR(vp, &vattr, fp->f_cred);
	}
out:
	VOP_UNLOCK(vp, 0);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * File table vnode stat routine.
 */
static int
vn_statfile(fp, sb, active_cred, td)
	struct file *fp;
	struct stat *sb;
	struct ucred *active_cred;
	struct thread *td;
{
	struct vnode *vp = fp->f_vnode;
	int vfslocked;
	int error;

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = vn_stat(vp, sb, active_cred, fp->f_cred, td);
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);

	return (error);
}

/*
 * Stat a vnode; implementation for the stat syscall
 */
int
vn_stat(vp, sb, active_cred, file_cred, td)
	struct vnode *vp;
	register struct stat *sb;
	struct ucred *active_cred;
	struct ucred *file_cred;
	struct thread *td;
{
	struct vattr vattr;
	register struct vattr *vap;
	int error;
	u_short mode;

#ifdef MAC
	error = mac_vnode_check_stat(active_cred, file_cred, vp);
	if (error)
		return (error);
#endif

	vap = &vattr;

	/*
	 * Initialize defaults for new and unusual fields, so that file
	 * systems which don't support these fields don't need to know
	 * about them.
	 */
	vap->va_birthtime.tv_sec = -1;
	vap->va_birthtime.tv_nsec = 0;
	vap->va_fsid = VNOVAL;
	vap->va_rdev = NODEV;

	error = VOP_GETATTR(vp, vap, active_cred);
	if (error)
		return (error);

	/*
	 * Zero the spare stat fields
	 */
	bzero(sb, sizeof *sb);

	/*
	 * Copy from vattr table
	 */
	if (vap->va_fsid != VNOVAL)
		sb->st_dev = vap->va_fsid;
	else
		sb->st_dev = vp->v_mount->mnt_stat.f_fsid.val[0];
	sb->st_ino = vap->va_fileid;
	mode = vap->va_mode;
	switch (vap->va_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return (EBADF);
	};
	sb->st_mode = mode;
	sb->st_nlink = vap->va_nlink;
	sb->st_uid = vap->va_uid;
	sb->st_gid = vap->va_gid;
	sb->st_rdev = vap->va_rdev;
	if (vap->va_size > OFF_MAX)
		return (EOVERFLOW);
	sb->st_size = vap->va_size;
	sb->st_atim = vap->va_atime;
	sb->st_mtim = vap->va_mtime;
	sb->st_ctim = vap->va_ctime;
	sb->st_birthtim = vap->va_birthtime;

        /*
	 * According to www.opengroup.org, the meaning of st_blksize is 
	 *   "a filesystem-specific preferred I/O block size for this 
	 *    object.  In some filesystem types, this may vary from file
	 *    to file"
	 * Use miminum/default of PAGE_SIZE (e.g. for VCHR).
	 */

	sb->st_blksize = max(PAGE_SIZE, vap->va_blocksize);
	
	sb->st_flags = vap->va_flags;
	if (priv_check(td, PRIV_VFS_GENERATION))
		sb->st_gen = 0;
	else
		sb->st_gen = vap->va_gen;

	sb->st_blocks = vap->va_bytes / S_BLKSIZE;
	return (0);
}

/*
 * File table vnode ioctl routine.
 */
static int
vn_ioctl(fp, com, data, active_cred, td)
	struct file *fp;
	u_long com;
	void *data;
	struct ucred *active_cred;
	struct thread *td;
{
	struct vnode *vp = fp->f_vnode;
	struct vattr vattr;
	int vfslocked;
	int error;

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	error = ENOTTY;
	switch (vp->v_type) {
	case VREG:
	case VDIR:
		if (com == FIONREAD) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			error = VOP_GETATTR(vp, &vattr, active_cred);
			VOP_UNLOCK(vp, 0);
			if (!error)
				*(int *)data = vattr.va_size - fp->f_offset;
		}
		if (com == FIONBIO || com == FIOASYNC)	/* XXX */
			error = 0;
		else
			error = VOP_IOCTL(vp, com, data, fp->f_flag,
			    active_cred, td);
		break;

	default:
		break;
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * File table vnode poll routine.
 */
static int
vn_poll(fp, events, active_cred, td)
	struct file *fp;
	int events;
	struct ucred *active_cred;
	struct thread *td;
{
	struct vnode *vp;
	int vfslocked;
	int error;

	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
#ifdef MAC
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = mac_vnode_check_poll(active_cred, fp->f_cred, vp);
	VOP_UNLOCK(vp, 0);
	if (!error)
#endif

	error = VOP_POLL(vp, events, fp->f_cred, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Acquire the requested lock and then check for validity.  LK_RETRY
 * permits vn_lock to return doomed vnodes.
 */
int
_vn_lock(struct vnode *vp, int flags, char *file, int line)
{
	int error;

	VNASSERT((flags & LK_TYPE_MASK) != 0, vp,
	    ("vn_lock called with no locktype."));
	do {
#ifdef DEBUG_VFS_LOCKS
		KASSERT(vp->v_holdcnt != 0,
		    ("vn_lock %p: zero hold count", vp));
#endif
		error = VOP_LOCK1(vp, flags, file, line);
		flags &= ~LK_INTERLOCK;	/* Interlock is always dropped. */
		KASSERT((flags & LK_RETRY) == 0 || error == 0,
		    ("LK_RETRY set with incompatible flags (0x%x) or an error occured (%d)",
		    flags, error));
		/*
		 * Callers specify LK_RETRY if they wish to get dead vnodes.
		 * If RETRY is not set, we return ENOENT instead.
		 */
		if (error == 0 && vp->v_iflag & VI_DOOMED &&
		    (flags & LK_RETRY) == 0) {
			VOP_UNLOCK(vp, 0);
			error = ENOENT;
			break;
		}
	} while (flags & LK_RETRY && error != 0);
	return (error);
}

/*
 * File table vnode close routine.
 */
static int
vn_closefile(fp, td)
	struct file *fp;
	struct thread *td;
{
	struct vnode *vp;
	struct flock lf;
	int vfslocked;
	int error;

	vp = fp->f_vnode;

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if (fp->f_type == DTYPE_VNODE && fp->f_flag & FHASLOCK) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		(void) VOP_ADVLOCK(vp, fp, F_UNLCK, &lf, F_FLOCK);
	}

	fp->f_ops = &badfileops;

	error = vn_close(vp, fp->f_flag, fp->f_cred, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Preparing to start a filesystem write operation. If the operation is
 * permitted, then we bump the count of operations in progress and
 * proceed. If a suspend request is in progress, we wait until the
 * suspension is over, and then proceed.
 */
int
vn_start_write(vp, mpp, flags)
	struct vnode *vp;
	struct mount **mpp;
	int flags;
{
	struct mount *mp;
	int error;

	error = 0;
	/*
	 * If a vnode is provided, get and return the mount point that
	 * to which it will write.
	 */
	if (vp != NULL) {
		if ((error = VOP_GETWRITEMOUNT(vp, mpp)) != 0) {
			*mpp = NULL;
			if (error != EOPNOTSUPP)
				return (error);
			return (0);
		}
	}
	if ((mp = *mpp) == NULL)
		return (0);

	/*
	 * VOP_GETWRITEMOUNT() returns with the mp refcount held through
	 * a vfs_ref().
	 * As long as a vnode is not provided we need to acquire a
	 * refcount for the provided mountpoint too, in order to
	 * emulate a vfs_ref().
	 */
	MNT_ILOCK(mp);
	if (vp == NULL)
		MNT_REF(mp);

	/*
	 * Check on status of suspension.
	 */
	if ((curthread->td_pflags & TDP_IGNSUSP) == 0 ||
	    mp->mnt_susp_owner != curthread) {
		while ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0) {
			if (flags & V_NOWAIT) {
				error = EWOULDBLOCK;
				goto unlock;
			}
			error = msleep(&mp->mnt_flag, MNT_MTX(mp),
			    (PUSER - 1) | (flags & PCATCH), "suspfs", 0);
			if (error)
				goto unlock;
		}
	}
	if (flags & V_XSLEEP)
		goto unlock;
	mp->mnt_writeopcount++;
unlock:
	if (error != 0 || (flags & V_XSLEEP) != 0)
		MNT_REL(mp);
	MNT_IUNLOCK(mp);
	return (error);
}

/*
 * Secondary suspension. Used by operations such as vop_inactive
 * routines that are needed by the higher level functions. These
 * are allowed to proceed until all the higher level functions have
 * completed (indicated by mnt_writeopcount dropping to zero). At that
 * time, these operations are halted until the suspension is over.
 */
int
vn_start_secondary_write(vp, mpp, flags)
	struct vnode *vp;
	struct mount **mpp;
	int flags;
{
	struct mount *mp;
	int error;

 retry:
	if (vp != NULL) {
		if ((error = VOP_GETWRITEMOUNT(vp, mpp)) != 0) {
			*mpp = NULL;
			if (error != EOPNOTSUPP)
				return (error);
			return (0);
		}
	}
	/*
	 * If we are not suspended or have not yet reached suspended
	 * mode, then let the operation proceed.
	 */
	if ((mp = *mpp) == NULL)
		return (0);

	/*
	 * VOP_GETWRITEMOUNT() returns with the mp refcount held through
	 * a vfs_ref().
	 * As long as a vnode is not provided we need to acquire a
	 * refcount for the provided mountpoint too, in order to
	 * emulate a vfs_ref().
	 */
	MNT_ILOCK(mp);
	if (vp == NULL)
		MNT_REF(mp);
	if ((mp->mnt_kern_flag & (MNTK_SUSPENDED | MNTK_SUSPEND2)) == 0) {
		mp->mnt_secondary_writes++;
		mp->mnt_secondary_accwrites++;
		MNT_IUNLOCK(mp);
		return (0);
	}
	if (flags & V_NOWAIT) {
		MNT_REL(mp);
		MNT_IUNLOCK(mp);
		return (EWOULDBLOCK);
	}
	/*
	 * Wait for the suspension to finish.
	 */
	error = msleep(&mp->mnt_flag, MNT_MTX(mp),
		       (PUSER - 1) | (flags & PCATCH) | PDROP, "suspfs", 0);
	vfs_rel(mp);
	if (error == 0)
		goto retry;
	return (error);
}

/*
 * Filesystem write operation has completed. If we are suspending and this
 * operation is the last one, notify the suspender that the suspension is
 * now in effect.
 */
void
vn_finished_write(mp)
	struct mount *mp;
{
	if (mp == NULL)
		return;
	MNT_ILOCK(mp);
	MNT_REL(mp);
	mp->mnt_writeopcount--;
	if (mp->mnt_writeopcount < 0)
		panic("vn_finished_write: neg cnt");
	if ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0 &&
	    mp->mnt_writeopcount <= 0)
		wakeup(&mp->mnt_writeopcount);
	MNT_IUNLOCK(mp);
}


/*
 * Filesystem secondary write operation has completed. If we are
 * suspending and this operation is the last one, notify the suspender
 * that the suspension is now in effect.
 */
void
vn_finished_secondary_write(mp)
	struct mount *mp;
{
	if (mp == NULL)
		return;
	MNT_ILOCK(mp);
	MNT_REL(mp);
	mp->mnt_secondary_writes--;
	if (mp->mnt_secondary_writes < 0)
		panic("vn_finished_secondary_write: neg cnt");
	if ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0 &&
	    mp->mnt_secondary_writes <= 0)
		wakeup(&mp->mnt_secondary_writes);
	MNT_IUNLOCK(mp);
}



/*
 * Request a filesystem to suspend write operations.
 */
int
vfs_write_suspend(mp)
	struct mount *mp;
{
	int error;

	MNT_ILOCK(mp);
	if (mp->mnt_susp_owner == curthread) {
		MNT_IUNLOCK(mp);
		return (EALREADY);
	}
	while (mp->mnt_kern_flag & MNTK_SUSPEND)
		msleep(&mp->mnt_flag, MNT_MTX(mp), PUSER - 1, "wsuspfs", 0);
	mp->mnt_kern_flag |= MNTK_SUSPEND;
	mp->mnt_susp_owner = curthread;
	if (mp->mnt_writeopcount > 0)
		(void) msleep(&mp->mnt_writeopcount, 
		    MNT_MTX(mp), (PUSER - 1)|PDROP, "suspwt", 0);
	else
		MNT_IUNLOCK(mp);
	if ((error = VFS_SYNC(mp, MNT_SUSPEND)) != 0)
		vfs_write_resume(mp);
	return (error);
}

/*
 * Request a filesystem to resume write operations.
 */
void
vfs_write_resume(mp)
	struct mount *mp;
{

	MNT_ILOCK(mp);
	if ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0) {
		KASSERT(mp->mnt_susp_owner == curthread, ("mnt_susp_owner"));
		mp->mnt_kern_flag &= ~(MNTK_SUSPEND | MNTK_SUSPEND2 |
				       MNTK_SUSPENDED);
		mp->mnt_susp_owner = NULL;
		wakeup(&mp->mnt_writeopcount);
		wakeup(&mp->mnt_flag);
		curthread->td_pflags &= ~TDP_IGNSUSP;
		MNT_IUNLOCK(mp);
		VFS_SUSP_CLEAN(mp);
	} else
		MNT_IUNLOCK(mp);
}

/*
 * Implement kqueues for files by translating it to vnode operation.
 */
static int
vn_kqfilter(struct file *fp, struct knote *kn)
{
	int vfslocked;
	int error;

	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
	error = VOP_KQFILTER(fp->f_vnode, kn);
	VFS_UNLOCK_GIANT(vfslocked);

	return error;
}

/*
 * Simplified in-kernel wrapper calls for extended attribute access.
 * Both calls pass in a NULL credential, authorizing as "kernel" access.
 * Set IO_NODELOCKED in ioflg if the vnode is already locked.
 */
int
vn_extattr_get(struct vnode *vp, int ioflg, int attrnamespace,
    const char *attrname, int *buflen, char *buf, struct thread *td)
{
	struct uio	auio;
	struct iovec	iov;
	int	error;

	iov.iov_len = *buflen;
	iov.iov_base = buf;

	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	auio.uio_offset = 0;
	auio.uio_resid = *buflen;

	if ((ioflg & IO_NODELOCKED) == 0)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	ASSERT_VOP_LOCKED(vp, "IO_NODELOCKED with no vp lock held");

	/* authorize attribute retrieval as kernel */
	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, &auio, NULL, NULL,
	    td);

	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_UNLOCK(vp, 0);

	if (error == 0) {
		*buflen = *buflen - auio.uio_resid;
	}

	return (error);
}

/*
 * XXX failure mode if partially written?
 */
int
vn_extattr_set(struct vnode *vp, int ioflg, int attrnamespace,
    const char *attrname, int buflen, char *buf, struct thread *td)
{
	struct uio	auio;
	struct iovec	iov;
	struct mount	*mp;
	int	error;

	iov.iov_len = buflen;
	iov.iov_base = buf;

	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	auio.uio_offset = 0;
	auio.uio_resid = buflen;

	if ((ioflg & IO_NODELOCKED) == 0) {
		if ((error = vn_start_write(vp, &mp, V_WAIT)) != 0)
			return (error);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}

	ASSERT_VOP_LOCKED(vp, "IO_NODELOCKED with no vp lock held");

	/* authorize attribute setting as kernel */
	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio, NULL, td);

	if ((ioflg & IO_NODELOCKED) == 0) {
		vn_finished_write(mp);
		VOP_UNLOCK(vp, 0);
	}

	return (error);
}

int
vn_extattr_rm(struct vnode *vp, int ioflg, int attrnamespace,
    const char *attrname, struct thread *td)
{
	struct mount	*mp;
	int	error;

	if ((ioflg & IO_NODELOCKED) == 0) {
		if ((error = vn_start_write(vp, &mp, V_WAIT)) != 0)
			return (error);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}

	ASSERT_VOP_LOCKED(vp, "IO_NODELOCKED with no vp lock held");

	/* authorize attribute removal as kernel */
	error = VOP_DELETEEXTATTR(vp, attrnamespace, attrname, NULL, td);
	if (error == EOPNOTSUPP)
		error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL,
		    NULL, td);

	if ((ioflg & IO_NODELOCKED) == 0) {
		vn_finished_write(mp);
		VOP_UNLOCK(vp, 0);
	}

	return (error);
}

int
vn_vget_ino(struct vnode *vp, ino_t ino, int lkflags, struct vnode **rvp)
{
	struct mount *mp;
	int ltype, error;

	mp = vp->v_mount;
	ltype = VOP_ISLOCKED(vp);
	KASSERT(ltype == LK_EXCLUSIVE || ltype == LK_SHARED,
	    ("vn_vget_ino: vp not locked"));
	error = vfs_busy(mp, MBF_NOWAIT);
	if (error != 0) {
		vfs_ref(mp);
		VOP_UNLOCK(vp, 0);
		error = vfs_busy(mp, 0);
		vn_lock(vp, ltype | LK_RETRY);
		vfs_rel(mp);
		if (error != 0)
			return (ENOENT);
		if (vp->v_iflag & VI_DOOMED) {
			vfs_unbusy(mp);
			return (ENOENT);
		}
	}
	VOP_UNLOCK(vp, 0);
	error = VFS_VGET(mp, ino, lkflags, rvp);
	vfs_unbusy(mp);
	vn_lock(vp, ltype | LK_RETRY);
	if (vp->v_iflag & VI_DOOMED) {
		if (error == 0)
			vput(*rvp);
		error = ENOENT;
	}
	return (error);
}
