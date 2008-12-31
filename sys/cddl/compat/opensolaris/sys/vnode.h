/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/sys/vnode.h,v 1.6.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _OPENSOLARIS_SYS_VNODE_H_
#define	_OPENSOLARIS_SYS_VNODE_H_

#include_next <sys/vnode.h>
#include <sys/mount.h>
#include <sys/cred.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/syscallsubr.h>

typedef	struct vnode	vnode_t;
typedef	struct vattr	vattr_t;
typedef	void		caller_context_t;

typedef	struct vop_vector	vnodeops_t;
#define	vop_fid		vop_vptofh
#define	vop_fid_args	vop_vptofh_args
#define	a_fid		a_fhp

#define	v_count	v_usecount

static __inline int
vn_is_readonly(vnode_t *vp)
{
	return (vp->v_mount->mnt_flag & MNT_RDONLY);
}
#define	vn_vfswlock(vp)		(0)
#define	vn_vfsunlock(vp)	do { } while (0)
#define	vn_ismntpt(vp)		((vp)->v_type == VDIR && (vp)->v_mountedhere != NULL)
#define	vn_mountedvfs(vp)	((vp)->v_mountedhere)
#define	vn_has_cached_data(vp)	((vp)->v_object != NULL && (vp)->v_object->resident_page_count > 0)

#define	VN_HOLD(v)	vref(v)
#define	VN_RELE(v)	vrele(v)
#define	VN_URELE(v)	vput(v)

#define	VOP_REALVP(vp, vpp)	(*(vpp) = (vp), 0)

#define	vnevent_remove(vp)	do { } while (0)
#define	vnevent_rmdir(vp)	do { } while (0)
#define	vnevent_rename_src(vp)	do { } while (0)
#define	vnevent_rename_dest(vp)	do { } while (0)


#define	IS_DEVVP(vp)	\
	((vp)->v_type == VCHR || (vp)->v_type == VBLK || (vp)->v_type == VFIFO)

#define	MODEMASK	ALLPERMS

#define	specvp(vp, rdev, type, cr)	(VN_HOLD(vp), (vp))
#define	MANDMODE(mode)	(0)
#define	chklock(vp, op, offset, size, mode, ct)	(0)
#define	cleanlocks(vp, pid, foo)	do { } while (0)
#define	cleanshares(vp, pid)		do { } while (0)

/*
 * We will use va_spare is place of Solaris' va_mask.
 * This field is initialized in zfs_setattr().
 */
#define	va_mask		va_spare
/* TODO: va_fileid is shorter than va_nodeid !!! */
#define	va_nodeid	va_fileid
/* TODO: This field needs conversion! */
#define	va_nblocks	va_bytes
#define	va_blksize	va_blocksize
#define	va_seq		va_gen

#define	MAXOFFSET_T	OFF_MAX
#define	EXCL		0

#define	AT_TYPE		0x0001
#define	AT_MODE		0x0002
#define	AT_UID		0x0004
#define	AT_GID		0x0008
#define	AT_FSID		0x0010
#define	AT_NODEID	0x0020
#define	AT_NLINK	0x0040
#define	AT_SIZE		0x0080
#define	AT_ATIME	0x0100
#define	AT_MTIME	0x0200
#define	AT_CTIME	0x0400
#define	AT_RDEV		0x0800
#define	AT_BLKSIZE	0x1000
#define	AT_NBLOCKS	0x2000
#define	AT_SEQ		0x4000
#define	AT_NOSET	(AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
			 AT_BLKSIZE|AT_NBLOCKS|AT_SEQ)

#define	ACCESSED		(AT_ATIME)
#define	STATE_CHANGED		(AT_CTIME)
#define	CONTENT_MODIFIED	(AT_MTIME | AT_CTIME)

static __inline void
vattr_init_mask(vattr_t *vap)
{

	vap->va_mask = 0;

	if (vap->va_type != VNON)
		vap->va_mask |= AT_TYPE;
	if (vap->va_uid != (uid_t)VNOVAL)
		vap->va_mask |= AT_UID;
	if (vap->va_gid != (gid_t)VNOVAL)
		vap->va_mask |= AT_GID;
	if (vap->va_size != (u_quad_t)VNOVAL)
		vap->va_mask |= AT_SIZE;
	if (vap->va_atime.tv_sec != VNOVAL)
		vap->va_mask |= AT_ATIME;
	if (vap->va_mtime.tv_sec != VNOVAL)
		vap->va_mask |= AT_MTIME;
	if (vap->va_mode != (u_short)VNOVAL)
		vap->va_mask |= AT_MODE;
}

#define	FCREAT	O_CREAT
#define	FTRUNC	O_TRUNC
#define	FDSYNC	FFSYNC
#define	FRSYNC	FFSYNC
#define	FSYNC	FFSYNC
#define	FOFFMAX	0x00

enum create	{ CRCREAT };

static __inline int
zfs_vn_open(char *pnamep, enum uio_seg seg, int filemode, int createmode,
    vnode_t **vpp, enum create crwhy, mode_t umask)
{
	struct thread *td = curthread;
	struct nameidata nd;
	int error;

	ASSERT(seg == UIO_SYSSPACE);
	ASSERT(filemode == (FWRITE | FCREAT | FTRUNC | FOFFMAX));
	ASSERT(crwhy == CRCREAT);
	ASSERT(umask == 0);

	if (td->td_proc->p_fd->fd_rdir == NULL)
		td->td_proc->p_fd->fd_rdir = rootvnode;
	if (td->td_proc->p_fd->fd_cdir == NULL)
		td->td_proc->p_fd->fd_cdir = rootvnode;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, pnamep, td);
	error = vn_open_cred(&nd, &filemode, createmode, td->td_ucred, NULL);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (error == 0) {
		/* We just unlock so we hold a reference. */
		VN_HOLD(nd.ni_vp);
		VOP_UNLOCK(nd.ni_vp, 0, td);
		*vpp = nd.ni_vp;
	}
	return (error);
}
#define	vn_open(pnamep, seg, filemode, createmode, vpp, crwhy, umask)	\
	zfs_vn_open((pnamep), (seg), (filemode), (createmode), (vpp), (crwhy), (umask))

#define	RLIM64_INFINITY	0
static __inline int
zfs_vn_rdwr(enum uio_rw rw, vnode_t *vp, caddr_t base, ssize_t len,
    offset_t offset, enum uio_seg seg, int ioflag, int ulimit, cred_t *cr,
    ssize_t *residp)
{
	struct thread *td = curthread;
	int error, vfslocked, resid;

	ASSERT(rw == UIO_WRITE);
	ASSERT(ioflag == 0);
	ASSERT(ulimit == RLIM64_INFINITY);

	ioflag = IO_APPEND | IO_UNIT;

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	error = vn_rdwr(rw, vp, base, len, offset, seg, ioflag, cr, NOCRED,
	    &resid, td);
	VFS_UNLOCK_GIANT(vfslocked);
	if (residp != NULL)
		*residp = (ssize_t)resid;
	return (error);
}
#define	vn_rdwr(rw, vp, base, len, offset, seg, ioflag, ulimit, cr, residp) \
	zfs_vn_rdwr((rw), (vp), (base), (len), (offset), (seg), (ioflag), (ulimit), (cr), (residp))

static __inline int
zfs_vop_fsync(vnode_t *vp, int flag, cred_t *cr)
{
	struct thread *td = curthread;
	struct mount *mp;
	int error, vfslocked;

	ASSERT(flag == FSYNC);

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto drop;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_FSYNC(vp, MNT_WAIT, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
drop:
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}
#define	VOP_FSYNC(vp, flag, cr)	zfs_vop_fsync((vp), (flag), (cr))

static __inline int
zfs_vop_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr)
{

	ASSERT(flag == (FWRITE | FCREAT | FTRUNC | FOFFMAX));
	ASSERT(count == 1);
	ASSERT(offset == 0);

	return (vn_close(vp, flag, cr, curthread));
}
#define	VOP_CLOSE(vp, oflags, count, offset, cr)			\
	zfs_vop_close((vp), (oflags), (count), (offset), (cr))

static __inline int
vn_rename(char *from, char *to, enum uio_seg seg)
{

	ASSERT(seg == UIO_SYSSPACE);

	return (kern_rename(curthread, from, to, seg));
}

enum rm	{ RMFILE };
static __inline int
vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag)
{

	ASSERT(seg == UIO_SYSSPACE);
	ASSERT(dirflag == RMFILE);

	return (kern_unlink(curthread, fnamep, seg));
}

#endif	/* _OPENSOLARIS_SYS_VNODE_H_ */
