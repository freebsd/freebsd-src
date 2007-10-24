/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994, 1997 Christos Zoulas.  
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
/*#include <sys/ioctl.h>*/
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <sys/sysproto.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_fcntl.h>

#include <security/mac/mac_framework.h>

static int svr4_to_bsd_flags(int);
static u_long svr4_to_bsd_cmd(u_long);
static int fd_revoke(struct thread *, int);
static int fd_truncate(struct thread *, int, struct flock *);
static int bsd_to_svr4_flags(int);
static void bsd_to_svr4_flock(struct flock *, struct svr4_flock *);
static void svr4_to_bsd_flock(struct svr4_flock *, struct flock *);
static void bsd_to_svr4_flock64(struct flock *, struct svr4_flock64 *);
static void svr4_to_bsd_flock64(struct svr4_flock64 *, struct flock *);

static u_long
svr4_to_bsd_cmd(cmd)
	u_long	cmd;
{
	switch (cmd) {
	case SVR4_F_DUPFD:
		return F_DUPFD;
	case SVR4_F_GETFD:
		return F_GETFD;
	case SVR4_F_SETFD:
		return F_SETFD;
	case SVR4_F_GETFL:
		return F_GETFL;
	case SVR4_F_SETFL:
		return F_SETFL;
	case SVR4_F_GETLK:
		return F_GETLK;
	case SVR4_F_SETLK:
		return F_SETLK;
	case SVR4_F_SETLKW:
		return F_SETLKW;
	default:
		return -1;
	}
}

static int
svr4_to_bsd_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & SVR4_O_RDONLY) ? O_RDONLY : 0;
	r |= (l & SVR4_O_WRONLY) ? O_WRONLY : 0;
	r |= (l & SVR4_O_RDWR) ? O_RDWR : 0;
	r |= (l & SVR4_O_NDELAY) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_APPEND) ? O_APPEND : 0;
	r |= (l & SVR4_O_SYNC) ? O_FSYNC : 0;
	r |= (l & SVR4_O_NONBLOCK) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_PRIV) ? O_EXLOCK : 0;
	r |= (l & SVR4_O_CREAT) ? O_CREAT : 0;
	r |= (l & SVR4_O_TRUNC) ? O_TRUNC : 0;
	r |= (l & SVR4_O_EXCL) ? O_EXCL : 0;
	r |= (l & SVR4_O_NOCTTY) ? O_NOCTTY : 0;
	return r;
}

static int
bsd_to_svr4_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & O_RDONLY) ? SVR4_O_RDONLY : 0;
	r |= (l & O_WRONLY) ? SVR4_O_WRONLY : 0;
	r |= (l & O_RDWR) ? SVR4_O_RDWR : 0;
	r |= (l & O_NDELAY) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_APPEND) ? SVR4_O_APPEND : 0;
	r |= (l & O_FSYNC) ? SVR4_O_SYNC : 0;
	r |= (l & O_NONBLOCK) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_EXLOCK) ? SVR4_O_PRIV : 0;
	r |= (l & O_CREAT) ? SVR4_O_CREAT : 0;
	r |= (l & O_TRUNC) ? SVR4_O_TRUNC : 0;
	r |= (l & O_EXCL) ? SVR4_O_EXCL : 0;
	r |= (l & O_NOCTTY) ? SVR4_O_NOCTTY : 0;
	return r;
}


static void
bsd_to_svr4_flock(iflp, oflp)
	struct flock		*iflp;
	struct svr4_flock	*oflp;
{
	switch (iflp->l_type) {
	case F_RDLCK:
		oflp->l_type = SVR4_F_RDLCK;
		break;
	case F_WRLCK:
		oflp->l_type = SVR4_F_WRLCK;
		break;
	case F_UNLCK:
		oflp->l_type = SVR4_F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = (short) iflp->l_whence;
	oflp->l_start = (svr4_off_t) iflp->l_start;
	oflp->l_len = (svr4_off_t) iflp->l_len;
	oflp->l_sysid = 0;
	oflp->l_pid = (svr4_pid_t) iflp->l_pid;
}


static void
svr4_to_bsd_flock(iflp, oflp)
	struct svr4_flock	*iflp;
	struct flock		*oflp;
{
	switch (iflp->l_type) {
	case SVR4_F_RDLCK:
		oflp->l_type = F_RDLCK;
		break;
	case SVR4_F_WRLCK:
		oflp->l_type = F_WRLCK;
		break;
	case SVR4_F_UNLCK:
		oflp->l_type = F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = iflp->l_whence;
	oflp->l_start = (off_t) iflp->l_start;
	oflp->l_len = (off_t) iflp->l_len;
	oflp->l_pid = (pid_t) iflp->l_pid;

}

static void
bsd_to_svr4_flock64(iflp, oflp)
	struct flock		*iflp;
	struct svr4_flock64	*oflp;
{
	switch (iflp->l_type) {
	case F_RDLCK:
		oflp->l_type = SVR4_F_RDLCK;
		break;
	case F_WRLCK:
		oflp->l_type = SVR4_F_WRLCK;
		break;
	case F_UNLCK:
		oflp->l_type = SVR4_F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = (short) iflp->l_whence;
	oflp->l_start = (svr4_off64_t) iflp->l_start;
	oflp->l_len = (svr4_off64_t) iflp->l_len;
	oflp->l_sysid = 0;
	oflp->l_pid = (svr4_pid_t) iflp->l_pid;
}


static void
svr4_to_bsd_flock64(iflp, oflp)
	struct svr4_flock64	*iflp;
	struct flock		*oflp;
{
	switch (iflp->l_type) {
	case SVR4_F_RDLCK:
		oflp->l_type = F_RDLCK;
		break;
	case SVR4_F_WRLCK:
		oflp->l_type = F_WRLCK;
		break;
	case SVR4_F_UNLCK:
		oflp->l_type = F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = iflp->l_whence;
	oflp->l_start = (off_t) iflp->l_start;
	oflp->l_len = (off_t) iflp->l_len;
	oflp->l_pid = (pid_t) iflp->l_pid;

}


static int
fd_revoke(td, fd)
	struct thread *td;
	int fd;
{
	struct vnode *vp;
	struct mount *mp;
	struct vattr vattr;
	int error, *retval;

	retval = td->td_retval;
	if ((error = fgetvp(td, fd, &vp)) != 0)
		return (error);

	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		error = EINVAL;
		goto out;
	}

#ifdef MAC
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = mac_vnode_check_revoke(td->td_ucred, vp);
	VOP_UNLOCK(vp, 0, td);
	if (error)
		goto out;
#endif

	if ((error = VOP_GETATTR(vp, &vattr, td->td_ucred, td)) != 0)
		goto out;

	if (td->td_ucred->cr_uid != vattr.va_uid &&
	    (error = priv_check(td, PRIV_VFS_ADMIN)) != 0)
		goto out;

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto out;
	if (vcount(vp) > 1)
		VOP_REVOKE(vp, REVOKEALL);
	vn_finished_write(mp);
out:
	vrele(vp);
	return error;
}


static int
fd_truncate(td, fd, flp)
	struct thread *td;
	int fd;
	struct flock *flp;
{
	off_t start, length;
	struct file *fp;
	struct vnode *vp;
	struct vattr vattr;
	int error, *retval;
	struct ftruncate_args ft;

	retval = td->td_retval;

	/*
	 * We only support truncating the file.
	 */
	if ((error = fget(td, fd, &fp)) != 0)
		return (error);

	vp = fp->f_vnode;

	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		fdrop(fp, td);
		return ESPIPE;
	}

	if ((error = VOP_GETATTR(vp, &vattr, td->td_ucred, td)) != 0) {
		fdrop(fp, td);
		return error;
	}

	length = vattr.va_size;

	switch (flp->l_whence) {
	case SEEK_CUR:
		start = fp->f_offset + flp->l_start;
		break;

	case SEEK_END:
		start = flp->l_start + length;
		break;

	case SEEK_SET:
		start = flp->l_start;
		break;

	default:
		fdrop(fp, td);
		return EINVAL;
	}

	if (start + flp->l_len < length) {
		/* We don't support free'ing in the middle of the file */
		fdrop(fp, td);
		return EINVAL;
	}

	ft.fd = fd;
	ft.length = start;

	error = ftruncate(td, &ft);

	fdrop(fp, td);
	return (error);
}

int
svr4_sys_open(td, uap)
	register struct thread *td;
	struct svr4_sys_open_args *uap;
{
	struct proc *p = td->td_proc;
	char *newpath;
	int bsd_flags, error, retval;

	CHECKALTEXIST(td, uap->path, &newpath);

	bsd_flags = svr4_to_bsd_flags(uap->flags);
	error = kern_open(td, newpath, UIO_SYSSPACE, bsd_flags, uap->mode);
	free(newpath, M_TEMP);

	if (error) {
	  /*	        uprintf("svr4_open(%s, 0x%0x, 0%o): %d\n", uap->path,
			uap->flags, uap->mode, error);*/
		return error;
	}

	retval = td->td_retval[0];

	PROC_LOCK(p);
	if (!(bsd_flags & O_NOCTTY) && SESS_LEADER(p) &&
	    !(p->p_flag & P_CONTROLT)) {
#if defined(NOTYET)
		struct file	*fp;

		error = fget(td, retval, &fp);
		PROC_UNLOCK(p);
		/*
		 * we may have lost a race the above open() and
		 * another thread issuing a close()
		 */
		if (error) 
			return (EBADF);	/* XXX: correct errno? */
		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, td->td_ucred,
			    td);
		fdrop(fp, td);
	} else {
		PROC_UNLOCK(p);
	}
#else
	}
	PROC_UNLOCK(p);
#endif
	return error;
}

int
svr4_sys_open64(td, uap)
	register struct thread *td;
	struct svr4_sys_open64_args *uap;
{
	return svr4_sys_open(td, (struct svr4_sys_open_args *)uap);
}

int
svr4_sys_creat(td, uap)
	register struct thread *td;
	struct svr4_sys_creat_args *uap;
{
	char *newpath;
	int error;

	CHECKALTEXIST(td, uap->path, &newpath);

	error = kern_open(td, newpath, UIO_SYSSPACE, O_WRONLY | O_CREAT |
	    O_TRUNC, uap->mode);
	free(newpath, M_TEMP);
	return (error);
}

int
svr4_sys_creat64(td, uap)
	register struct thread *td;
	struct svr4_sys_creat64_args *uap;
{
	return svr4_sys_creat(td, (struct svr4_sys_creat_args *)uap);
}

int
svr4_sys_llseek(td, uap)
	register struct thread *td;
	struct svr4_sys_llseek_args *uap;
{
	struct lseek_args ap;

	ap.fd = uap->fd;

#if BYTE_ORDER == BIG_ENDIAN
	ap.offset = (((u_int64_t) uap->offset1) << 32) | 
		uap->offset2;
#else
	ap.offset = (((u_int64_t) uap->offset2) << 32) | 
		uap->offset1;
#endif
	ap.whence = uap->whence;

	return lseek(td, &ap);
}

int
svr4_sys_access(td, uap)
	register struct thread *td;
	struct svr4_sys_access_args *uap;
{
	char *newpath;
	int error;

	CHECKALTEXIST(td, uap->path, &newpath);
	error = kern_access(td, newpath, UIO_SYSSPACE, uap->flags);
	free(newpath, M_TEMP);
	return (error);
}

#if defined(NOTYET)
int
svr4_sys_pread(td, uap)
	register struct thread *td;
	struct svr4_sys_pread_args *uap;
{
	struct pread_args pra;

	/*
	 * Just translate the args structure and call the NetBSD
	 * pread(2) system call (offset type is 64-bit in NetBSD).
	 */
	pra.fd = uap->fd;
	pra.buf = uap->buf;
	pra.nbyte = uap->nbyte;
	pra.offset = uap->off;

	return pread(td, &pra);
}
#endif

#if defined(NOTYET)
int
svr4_sys_pread64(td, v, retval)
	register struct thread *td;
	void *v; 
	register_t *retval;
{

	struct svr4_sys_pread64_args *uap = v;
	struct sys_pread_args pra;

	/*
	 * Just translate the args structure and call the NetBSD
	 * pread(2) system call (offset type is 64-bit in NetBSD).
	 */
	pra.fd = uap->fd;
	pra.buf = uap->buf;
	pra.nbyte = uap->nbyte;
	pra.offset = uap->off;

	return (sys_pread(td, &pra, retval));
}
#endif /* NOTYET */

#if defined(NOTYET)
int
svr4_sys_pwrite(td, uap)
	register struct thread *td;
	struct svr4_sys_pwrite_args *uap;
{
	struct pwrite_args pwa;

	/*
	 * Just translate the args structure and call the NetBSD
	 * pwrite(2) system call (offset type is 64-bit in NetBSD).
	 */
	pwa.fd = uap->fd;
	pwa.buf = uap->buf;
	pwa.nbyte = uap->nbyte;
	pwa.offset = uap->off;

	return pwrite(td, &pwa);
}
#endif

#if defined(NOTYET)
int
svr4_sys_pwrite64(td, v, retval)
	register struct thread *td;
	void *v; 
	register_t *retval;
{
	struct svr4_sys_pwrite64_args *uap = v;
	struct sys_pwrite_args pwa;

	/*
	 * Just translate the args structure and call the NetBSD
	 * pwrite(2) system call (offset type is 64-bit in NetBSD).
	 */
	pwa.fd = uap->fd;
	pwa.buf = uap->buf;
	pwa.nbyte = uap->nbyte;
	pwa.offset = uap->off;

	return (sys_pwrite(td, &pwa, retval));
}
#endif /* NOTYET */

int
svr4_sys_fcntl(td, uap)
	register struct thread *td;
	struct svr4_sys_fcntl_args *uap;
{
	int cmd, error, *retval;

	retval = td->td_retval;

	cmd = svr4_to_bsd_cmd(uap->cmd);

	switch (cmd) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
		return (kern_fcntl(td, uap->fd, cmd, (intptr_t)uap->arg));

	case F_GETFL:
		error = kern_fcntl(td, uap->fd, cmd, (intptr_t)uap->arg);
		if (error)
			return (error);
		*retval = bsd_to_svr4_flags(*retval);
		return (error);

	case F_SETFL:
		{
			/*
			 * we must save the O_ASYNC flag, as that is
			 * handled by ioctl(_, I_SETSIG, _) emulation.
			 */
			int flags;

			DPRINTF(("Setting flags %p\n", uap->arg));

			error = kern_fcntl(td, uap->fd, F_GETFL, 0);
			if (error)
				return (error);
			flags = *retval;
			flags &= O_ASYNC;
			flags |= svr4_to_bsd_flags((u_long) uap->arg);
			return (kern_fcntl(td, uap->fd, F_SETFL, flags));
		}

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		{
			struct svr4_flock	ifl;
			struct flock		fl;

			error = copyin(uap->arg, &ifl, sizeof (ifl));
			if (error)
				return (error);

			svr4_to_bsd_flock(&ifl, &fl);

			error = kern_fcntl(td, uap->fd, cmd, (intptr_t)&fl);
			if (error || cmd != F_GETLK)
				return (error);

			bsd_to_svr4_flock(&fl, &ifl);

			return (copyout(&ifl, uap->arg, sizeof (ifl)));
		}
	case -1:
		switch (uap->cmd) {
		case SVR4_F_DUP2FD:
			{
				struct dup2_args du;

				du.from = uap->fd;
				du.to = (int)uap->arg;
				error = dup2(td, &du);
				if (error)
					return error;
				*retval = du.to;
				return 0;
			}

		case SVR4_F_FREESP:
			{
				struct svr4_flock	 ifl;
				struct flock		 fl;

				error = copyin(uap->arg, &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock(&ifl, &fl);
				return fd_truncate(td, uap->fd, &fl);
			}

		case SVR4_F_GETLK64:
		case SVR4_F_SETLK64:
		case SVR4_F_SETLKW64:
			{
				struct svr4_flock64	ifl;
				struct flock		fl;

				switch (uap->cmd) {
				case SVR4_F_GETLK64:
					cmd = F_GETLK;
					break;
				case SVR4_F_SETLK64:
					cmd = F_SETLK;
					break;					
				case SVR4_F_SETLKW64:
					cmd = F_SETLKW;
					break;
				}
				error = copyin(uap->arg, &ifl,
				    sizeof (ifl));
				if (error)
					return (error);

				svr4_to_bsd_flock64(&ifl, &fl);

				error = kern_fcntl(td, uap->fd, cmd,
				    (intptr_t)&fl);
				if (error || cmd != F_GETLK)
					return (error);

				bsd_to_svr4_flock64(&fl, &ifl);

				return (copyout(&ifl, uap->arg,
				    sizeof (ifl)));
			}

		case SVR4_F_FREESP64:
			{
				struct svr4_flock64	 ifl;
				struct flock		 fl;

				error = copyin(uap->arg, &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock64(&ifl, &fl);
				return fd_truncate(td, uap->fd, &fl);
			}

		case SVR4_F_REVOKE:
			return fd_revoke(td, uap->fd);

		default:
			return ENOSYS;
		}

	default:
		return ENOSYS;
	}
}
