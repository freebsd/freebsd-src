/*
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
 *	@(#)vfs_vnops.c	8.2 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mac.h>
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

#include <machine/limits.h>

static int vn_closefile(struct file *fp, struct thread *td);
static int vn_ioctl(struct file *fp, u_long com, void *data, 
		struct ucred *active_cred, struct thread *td);
static int vn_read(struct file *fp, struct uio *uio, 
		struct ucred *active_cred, int flags, struct thread *td);
static int vn_poll(struct file *fp, int events, struct ucred *active_cred,
		struct thread *td);
static int vn_kqfilter(struct file *fp, struct knote *kn);
static int vn_statfile(struct file *fp, struct stat *sb,
		struct ucred *active_cred, struct thread *td);
static int vn_write(struct file *fp, struct uio *uio, 
		struct ucred *active_cred, int flags, struct thread *td);

struct 	fileops vnops = {
	vn_read, vn_write, vn_ioctl, vn_poll, vn_kqfilter,
	vn_statfile, vn_closefile
};

int
vn_open(ndp, flagp, cmode)
	register struct nameidata *ndp;
	int *flagp, cmode;
{
	struct thread *td = ndp->ni_cnd.cn_thread;

	return (vn_open_cred(ndp, flagp, cmode, td->td_ucred));
}

/*
 * Common code for vnode open operations.
 * Check permissions, and call the VOP_OPEN or VOP_CREATE routine.
 * 
 * Note that this does NOT free nameidata for the successful case,
 * due to the NDINIT being done elsewhere.
 */
int
vn_open_cred(ndp, flagp, cmode, cred)
	register struct nameidata *ndp;
	int *flagp, cmode;
	struct ucred *cred;
{
	struct vnode *vp;
	struct mount *mp;
	struct thread *td = ndp->ni_cnd.cn_thread;
	struct vattr vat;
	struct vattr *vap = &vat;
	int mode, fmode, error;
#ifdef LOOKUP_SHARED
	int exclusive;	/* The current intended lock state */

	exclusive = 0;
#endif

restart:
	fmode = *flagp;
	if (fmode & O_CREAT) {
		ndp->ni_cnd.cn_nameiop = CREATE;
		ndp->ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
		if ((fmode & O_EXCL) == 0 && (fmode & O_NOFOLLOW) == 0)
			ndp->ni_cnd.cn_flags |= FOLLOW;
		bwillwrite();
		if ((error = namei(ndp)) != 0)
			return (error);
		if (ndp->ni_vp == NULL) {
			VATTR_NULL(vap);
			vap->va_type = VREG;
			vap->va_mode = cmode;
			if (fmode & O_EXCL)
				vap->va_vaflags |= VA_EXCLUSIVE;
			if (vn_start_write(ndp->ni_dvp, &mp, V_NOWAIT) != 0) {
				NDFREE(ndp, NDF_ONLY_PNBUF);
				vput(ndp->ni_dvp);
				if ((error = vn_start_write(NULL, &mp,
				    V_XSLEEP | PCATCH)) != 0)
					return (error);
				goto restart;
			}
#ifdef MAC
			error = mac_check_vnode_create(cred, ndp->ni_dvp,
			    &ndp->ni_cnd, vap);
			if (error == 0) {
#endif
				VOP_LEASE(ndp->ni_dvp, td, cred, LEASE_WRITE);
				error = VOP_CREATE(ndp->ni_dvp, &ndp->ni_vp,
						   &ndp->ni_cnd, vap);
#ifdef MAC
			}
#endif
			vput(ndp->ni_dvp);
			vn_finished_write(mp);
			if (error) {
				NDFREE(ndp, NDF_ONLY_PNBUF);
				return (error);
			}
			ASSERT_VOP_UNLOCKED(ndp->ni_dvp, "create");
			ASSERT_VOP_LOCKED(ndp->ni_vp, "create");
			fmode &= ~O_TRUNC;
			vp = ndp->ni_vp;
#ifdef LOOKUP_SHARED
			exclusive = 1;
#endif
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
#ifdef LOOKUP_SHARED
		ndp->ni_cnd.cn_flags =
		    ((fmode & O_NOFOLLOW) ? NOFOLLOW : FOLLOW) |
		    LOCKSHARED | LOCKLEAF;
#else
		ndp->ni_cnd.cn_flags =
		    ((fmode & O_NOFOLLOW) ? NOFOLLOW : FOLLOW) | LOCKLEAF;
#endif
		if ((error = namei(ndp)) != 0)
			return (error);
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
	mode = 0;
	if (fmode & (FWRITE | O_TRUNC)) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto bad;
		}
		mode |= VWRITE;
	}
	if (fmode & FREAD)
		mode |= VREAD;
	if (fmode & O_APPEND)
		mode |= VAPPEND;
#ifdef MAC
	error = mac_check_vnode_open(cred, vp, mode);
	if (error)
		goto bad;
#endif
	if ((fmode & O_CREAT) == 0) {
		if (mode & VWRITE) {
			error = vn_writechk(vp);
			if (error)
				goto bad;
		}
		if (mode) {
		        error = VOP_ACCESS(vp, mode, cred, td);
			if (error)
				goto bad;
		}
	}
	if ((error = VOP_GETATTR(vp, vap, cred, td)) == 0) {
		vp->v_cachedfs = vap->va_fsid;
		vp->v_cachedid = vap->va_fileid;
	}
	if ((error = VOP_OPEN(vp, fmode, cred, td)) != 0)
		goto bad;
	/*
	 * Make sure that a VM object is created for VMIO support.
	 */
	if (vn_canvmio(vp) == TRUE) {
#ifdef LOOKUP_SHARED
		int flock;

		if (!exclusive && VOP_GETVOBJECT(vp, NULL) != 0)
			VOP_LOCK(vp, LK_UPGRADE, td);
		/*
		 * In cases where the object is marked as dead object_create
		 * will unlock and relock exclusive.  It is safe to call in
		 * here with a shared lock because we only examine fields that
		 * the shared lock guarantees will be stable.  In the UPGRADE
		 * case it is not likely that anyone has used this vnode yet
		 * so there will be no contention.  The logic after this call
		 * restores the requested locking state.
		 */
#endif
		if ((error = vfs_object_create(vp, td, cred)) != 0) {
			VOP_UNLOCK(vp, 0, td);
			VOP_CLOSE(vp, fmode, cred, td);
			NDFREE(ndp, NDF_ONLY_PNBUF);
			vrele(vp);
			*flagp = fmode;
			return (error);
		}
#ifdef LOOKUP_SHARED
		flock = VOP_ISLOCKED(vp, td);
		if (!exclusive && flock == LK_EXCLUSIVE)
			VOP_LOCK(vp, LK_DOWNGRADE, td);
#endif
	}

	if (fmode & FWRITE)
		vp->v_writecount++;
	*flagp = fmode;
	return (0);
bad:
	NDFREE(ndp, NDF_ONLY_PNBUF);
	vput(vp);
	*flagp = fmode;
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
	int error;

	if (flags & FWRITE)
		vp->v_writecount--;
	error = VOP_CLOSE(vp, flags, file_cred, td);
	/*
	 * XXX - In certain instances VOP_CLOSE has to do the vrele
	 * itself. If the vrele has been done, it will return EAGAIN
	 * to indicate that the vrele should not be done again. When
	 * this happens, we just return success. The correct thing to
	 * do would be to have all VOP_CLOSE instances do the vrele.
	 */
	if (error == EAGAIN)
		return (0);
	vrele(vp);
	return (error);
}

/*
 * Sequential heuristic - detect sequential operation
 */
static __inline
int
sequential_heuristic(struct uio *uio, struct file *fp)
{

	if ((uio->uio_offset == 0 && fp->f_seqcount > 0) ||
	    uio->uio_offset == fp->f_nextoff) {
		/*
		 * XXX we assume that the filesystem block size is
		 * the default.  Not true, but still gives us a pretty
		 * good indicator of how sequential the read operations
		 * are.
		 */
		fp->f_seqcount += (uio->uio_resid + BKVASIZE - 1) / BKVASIZE;
		if (fp->f_seqcount >= 127)
			fp->f_seqcount = 127;
		return(fp->f_seqcount << 16);
	}

	/*
	 * Not sequential, quick draw-down of seqcount
	 */
	if (fp->f_seqcount > 1)
		fp->f_seqcount = 1;
	else
		fp->f_seqcount = 0;
	return(0);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.
 */
int
vn_rdwr(rw, vp, base, len, offset, segflg, ioflg, active_cred, file_cred,
    aresid, td)
	enum uio_rw rw;
	struct vnode *vp;
	caddr_t base;
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
	int error;

	if ((ioflg & IO_NODELOCKED) == 0) {
		mp = NULL;
		if (rw == UIO_WRITE) { 
			if (vp->v_type != VCHR &&
			    (error = vn_start_write(vp, &mp, V_WAIT | PCATCH))
			    != 0)
				return (error);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		} else {
			/*
			 * XXX This should be LK_SHARED but I don't trust VFS
			 * enough to leave it like that until it has been
			 * reviewed further.
			 */
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		}

	}
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
			error = mac_check_vnode_read(active_cred, file_cred,
			    vp);
		else
			error = mac_check_vnode_write(active_cred, file_cred,
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
		if (rw == UIO_WRITE)
			vn_finished_write(mp);
		VOP_UNLOCK(vp, 0, td);
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
	caddr_t base;
	int len;
	off_t offset;
	enum uio_seg segflg;
	int ioflg;
	struct ucred *active_cred;
	struct ucred *file_cred;
	int *aresid;
	struct thread *td;
{
	int error = 0;

	do {
		int chunk = (len > MAXBSIZE) ? MAXBSIZE : len;

		if (rw != UIO_READ && vp->v_type == VREG)
			bwillwrite();
		error = vn_rdwr(rw, vp, base, chunk, offset, segflg,
		    ioflg, active_cred, file_cred, aresid, td);
		len -= chunk;	/* aresid calc already includes length */
		if (error)
			break;
		offset += chunk;
		base += chunk;
		uio_yield();
	} while (len);
	if (aresid)
		*aresid += len;
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

	mtx_lock(&Giant);
	KASSERT(uio->uio_td == td, ("uio_td %p is not td %p",
	    uio->uio_td, td));
	vp = (struct vnode *)fp->f_data;
	ioflag = 0;
	if (fp->f_flag & FNONBLOCK)
		ioflag |= IO_NDELAY;
	if (fp->f_flag & O_DIRECT)
		ioflag |= IO_DIRECT;
	VOP_LEASE(vp, td, fp->f_cred, LEASE_READ);
	/*
	 * According to McKusick the vn lock is protecting f_offset here.
	 * Once this field has it's own lock we can acquire this shared.
	 */
	vn_lock(vp, LK_EXCLUSIVE | LK_NOPAUSE | LK_RETRY, td);
	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	ioflag |= sequential_heuristic(uio, fp);

#ifdef MAC
	error = mac_check_vnode_read(active_cred, fp->f_cred, vp);
	if (error == 0)
#endif
		error = VOP_READ(vp, uio, ioflag, fp->f_cred);
	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	VOP_UNLOCK(vp, 0, td);
	mtx_unlock(&Giant);
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
	int error, ioflag;

	mtx_lock(&Giant);
	KASSERT(uio->uio_td == td, ("uio_td %p is not td %p",
	    uio->uio_td, td));
	vp = (struct vnode *)fp->f_data;
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
	    (error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		mtx_unlock(&Giant);
		return (error);
	}
	VOP_LEASE(vp, td, fp->f_cred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;
	ioflag |= sequential_heuristic(uio, fp);
#ifdef MAC
	error = mac_check_vnode_write(active_cred, fp->f_cred, vp);
	if (error == 0)
#endif
		error = VOP_WRITE(vp, uio, ioflag, fp->f_cred);
	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	mtx_unlock(&Giant);
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
	struct vnode *vp = (struct vnode *)fp->f_data;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = vn_stat(vp, sb, active_cred, fp->f_cred, td);
	VOP_UNLOCK(vp, 0, td);

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
	error = mac_check_vnode_stat(active_cred, file_cred, vp);
	if (error)
		return (error);
#endif

	vap = &vattr;
	error = VOP_GETATTR(vp, vap, active_cred, td);
	if (error)
		return (error);

	vp->v_cachedfs = vap->va_fsid;
	vp->v_cachedid = vap->va_fileid;

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
		/* This is a cosmetic change, symlinks do not have a mode. */
		if (vp->v_mount->mnt_flag & MNT_NOSYMFOLLOW)
			sb->st_mode &= ~ACCESSPERMS;	/* 0000 */
		else
			sb->st_mode |= ACCESSPERMS;	/* 0777 */
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
	sb->st_atimespec = vap->va_atime;
	sb->st_mtimespec = vap->va_mtime;
	sb->st_ctimespec = vap->va_ctime;
	sb->st_birthtimespec = vap->va_birthtime;

        /*
	 * According to www.opengroup.org, the meaning of st_blksize is 
	 *   "a filesystem-specific preferred I/O block size for this 
	 *    object.  In some filesystem types, this may vary from file
	 *    to file"
	 * Default to PAGE_SIZE after much discussion.
	 */

	if (vap->va_type == VREG) {
		sb->st_blksize = vap->va_blocksize;
	} else if (vn_isdisk(vp, NULL)) {
		sb->st_blksize = vp->v_rdev->si_bsize_best;
		if (sb->st_blksize < vp->v_rdev->si_bsize_phys)
			sb->st_blksize = vp->v_rdev->si_bsize_phys;
		if (sb->st_blksize < BLKDEV_IOSIZE)
			sb->st_blksize = BLKDEV_IOSIZE;
	} else {
		sb->st_blksize = PAGE_SIZE;
	}
	
	sb->st_flags = vap->va_flags;
	if (suser(td))
		sb->st_gen = 0;
	else
		sb->st_gen = vap->va_gen;

#if (S_BLKSIZE == 512)
	/* Optimize this case */
	sb->st_blocks = vap->va_bytes >> 9;
#else
	sb->st_blocks = vap->va_bytes / S_BLKSIZE;
#endif
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
	register struct vnode *vp = ((struct vnode *)fp->f_data);
	struct vnode *vpold;
	struct vattr vattr;
	int error;

	switch (vp->v_type) {

	case VREG:
	case VDIR:
		if (com == FIONREAD) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			error = VOP_GETATTR(vp, &vattr, active_cred, td);
			VOP_UNLOCK(vp, 0, td);
			if (error)
				return (error);
			*(int *)data = vattr.va_size - fp->f_offset;
			return (0);
		}
		if (com == FIONBIO || com == FIOASYNC)	/* XXX */
			return (0);			/* XXX */
		/* FALLTHROUGH */

	default:
#if 0
		return (ENOTTY);
#endif
	case VFIFO:
	case VCHR:
	case VBLK:
		if (com == FIODTYPE) {
			if (vp->v_type != VCHR && vp->v_type != VBLK)
				return (ENOTTY);
			*(int *)data = devsw(vp->v_rdev)->d_flags & D_TYPEMASK;
			return (0);
		}
		error = VOP_IOCTL(vp, com, data, fp->f_flag, active_cred, td);
		if (error == ENOIOCTL) {
#ifdef DIAGNOSTIC
			Debugger("ENOIOCTL leaked through");
#endif
			error = ENOTTY;
		}
		if (error == 0 && com == TIOCSCTTY) {

			/* Do nothing if reassigning same control tty */
			sx_slock(&proctree_lock);
			if (td->td_proc->p_session->s_ttyvp == vp) {
				sx_sunlock(&proctree_lock);
				return (0);
			}

			vpold = td->td_proc->p_session->s_ttyvp;
			VREF(vp);
			SESS_LOCK(td->td_proc->p_session);
			td->td_proc->p_session->s_ttyvp = vp;
			SESS_UNLOCK(td->td_proc->p_session);

			sx_sunlock(&proctree_lock);

			/* Get rid of reference to old control tty */
			if (vpold)
				vrele(vpold);
		}
		return (error);
	}
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
#ifdef MAC
	int error;
#endif

	vp = (struct vnode *)fp->f_data;
#ifdef MAC
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = mac_check_vnode_poll(active_cred, fp->f_cred, vp);
	VOP_UNLOCK(vp, 0, td);
	if (error)
		return (error);
#endif

	return (VOP_POLL(vp, events, fp->f_cred, td));
}

/*
 * Check that the vnode is still valid, and if so
 * acquire requested lock.
 */
int
#ifndef	DEBUG_LOCKS
vn_lock(vp, flags, td)
#else
debug_vn_lock(vp, flags, td, filename, line)
#endif
	struct vnode *vp;
	int flags;
	struct thread *td;
#ifdef	DEBUG_LOCKS
	const char *filename;
	int line;
#endif
{
	int error;

	do {
		if ((flags & LK_INTERLOCK) == 0)
			VI_LOCK(vp);
		if ((vp->v_iflag & VI_XLOCK) && vp->v_vxproc != curthread) {
			vp->v_iflag |= VI_XWANT;
			msleep(vp, VI_MTX(vp), PINOD, "vn_lock", 0);
			error = ENOENT;
			if ((flags & LK_RETRY) == 0) {
				VI_UNLOCK(vp);
				return (error);
			}
		} 
#ifdef	DEBUG_LOCKS
		vp->filename = filename;
		vp->line = line;
#endif
		/*
		 * lockmgr drops interlock before it will return for
		 * any reason.  So force the code above to relock it.
		 */
		error = VOP_LOCK(vp, flags | LK_NOPAUSE | LK_INTERLOCK, td);
		flags &= ~LK_INTERLOCK;
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

	fp->f_ops = &badfileops;
	return (vn_close(((struct vnode *)fp->f_data), fp->f_flag,
		fp->f_cred, td));
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
	 * Check on status of suspension.
	 */
	while ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0) {
		if (flags & V_NOWAIT)
			return (EWOULDBLOCK);
		error = tsleep(&mp->mnt_flag, (PUSER - 1) | (flags & PCATCH),
		    "suspfs", 0);
		if (error)
			return (error);
	}
	if (flags & V_XSLEEP)
		return (0);
	mp->mnt_writeopcount++;
	return (0);
}

/*
 * Secondary suspension. Used by operations such as vop_inactive
 * routines that are needed by the higher level functions. These
 * are allowed to proceed until all the higher level functions have
 * completed (indicated by mnt_writeopcount dropping to zero). At that
 * time, these operations are halted until the suspension is over.
 */
int
vn_write_suspend_wait(vp, mp, flags)
	struct vnode *vp;
	struct mount *mp;
	int flags;
{
	int error;

	if (vp != NULL) {
		if ((error = VOP_GETWRITEMOUNT(vp, &mp)) != 0) {
			if (error != EOPNOTSUPP)
				return (error);
			return (0);
		}
	}
	/*
	 * If we are not suspended or have not yet reached suspended
	 * mode, then let the operation proceed.
	 */
	if (mp == NULL || (mp->mnt_kern_flag & MNTK_SUSPENDED) == 0)
		return (0);
	if (flags & V_NOWAIT)
		return (EWOULDBLOCK);
	/*
	 * Wait for the suspension to finish.
	 */
	return (tsleep(&mp->mnt_flag, (PUSER - 1) | (flags & PCATCH),
	    "suspfs", 0));
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
	mp->mnt_writeopcount--;
	if (mp->mnt_writeopcount < 0)
		panic("vn_finished_write: neg cnt");
	if ((mp->mnt_kern_flag & MNTK_SUSPEND) != 0 &&
	    mp->mnt_writeopcount <= 0)
		wakeup(&mp->mnt_writeopcount);
}

/*
 * Request a filesystem to suspend write operations.
 */
void
vfs_write_suspend(mp)
	struct mount *mp;
{
	struct thread *td = curthread;

	if (mp->mnt_kern_flag & MNTK_SUSPEND)
		return;
	mp->mnt_kern_flag |= MNTK_SUSPEND;
	if (mp->mnt_writeopcount > 0)
		(void) tsleep(&mp->mnt_writeopcount, PUSER - 1, "suspwt", 0);
	VFS_SYNC(mp, MNT_WAIT, td->td_ucred, td);
	mp->mnt_kern_flag |= MNTK_SUSPENDED;
}

/*
 * Request a filesystem to resume write operations.
 */
void
vfs_write_resume(mp)
	struct mount *mp;
{

	if ((mp->mnt_kern_flag & MNTK_SUSPEND) == 0)
		return;
	mp->mnt_kern_flag &= ~(MNTK_SUSPEND | MNTK_SUSPENDED);
	wakeup(&mp->mnt_writeopcount);
	wakeup(&mp->mnt_flag);
}

/*
 * Implement kqueues for files by translating it to vnode operation.
 */
static int
vn_kqfilter(struct file *fp, struct knote *kn)
{

	return (VOP_KQFILTER(((struct vnode *)fp->f_data), kn));
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
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	/* authorize attribute retrieval as kernel */
	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, &auio, NULL, NULL,
	    td);

	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_UNLOCK(vp, 0, td);

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
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	}

	/* authorize attribute setting as kernel */
	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio, NULL, td);

	if ((ioflg & IO_NODELOCKED) == 0) {
		vn_finished_write(mp);
		VOP_UNLOCK(vp, 0, td);
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
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	}

	/* authorize attribute removal as kernel */
	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL, NULL, td);

	if ((ioflg & IO_NODELOCKED) == 0) {
		vn_finished_write(mp);
		VOP_UNLOCK(vp, 0, td);
	}

	return (error);
}
