/*
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
 * 
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
/*#include <sys/ioctl.h>*/
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <sys/sysproto.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_fcntl.h>

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

	if ((error = VOP_GETATTR(vp, &vattr, td->td_ucred, td)) != 0)
		goto out;

	if (td->td_ucred->cr_uid != vattr.va_uid &&
	    (error = suser_td(td)) != 0)
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

	vp = (struct vnode *) fp->f_data;

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

	SCARG(&ft, fd) = fd;
	SCARG(&ft, length) = start;

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
	int			error, retval;
	struct open_args	cup;

	caddr_t sg = stackgap_init();
	CHECKALTEXIST(td, &sg, SCARG(uap, path));

	(&cup)->path = uap->path;
	(&cup)->flags = svr4_to_bsd_flags(uap->flags);
	(&cup)->mode = uap->mode;
	error = open(td, &cup);

	if (error) {
	  /*	        uprintf("svr4_open(%s, 0x%0x, 0%o): %d\n", uap->path,
			uap->flags, uap->mode, error);*/
		return error;
	}

	retval = td->td_retval[0];

	PROC_LOCK(p);
	if (!(SCARG(&cup, flags) & O_NOCTTY) && SESS_LEADER(p) &&
	    !(td->td_proc->p_flag & P_CONTROLT)) {
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
			fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, td);
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
	struct open_args cup;

	caddr_t sg = stackgap_init();
	CHECKALTEXIST(td, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, mode) = SCARG(uap, mode);
	SCARG(&cup, flags) = O_WRONLY | O_CREAT | O_TRUNC;

	return open(td, &cup);
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

	SCARG(&ap, fd) = SCARG(uap, fd);

#if BYTE_ORDER == BIG_ENDIAN
	SCARG(&ap, offset) = (((u_int64_t) SCARG(uap, offset1)) << 32) | 
		SCARG(uap, offset2);
#else
	SCARG(&ap, offset) = (((u_int64_t) SCARG(uap, offset2)) << 32) | 
		SCARG(uap, offset1);
#endif
	SCARG(&ap, whence) = SCARG(uap, whence);

	return lseek(td, &ap);
}

int
svr4_sys_access(td, uap)
	register struct thread *td;
	struct svr4_sys_access_args *uap;
{
	struct access_args cup;
	int *retval;

	caddr_t sg = stackgap_init();
	CHECKALTEXIST(td, &sg, SCARG(uap, path));

	retval = td->td_retval;

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, flags) = SCARG(uap, flags);

	return access(td, &cup);
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
	SCARG(&pra, fd) = SCARG(uap, fd);
	SCARG(&pra, buf) = SCARG(uap, buf);
	SCARG(&pra, nbyte) = SCARG(uap, nbyte);
	SCARG(&pra, offset) = SCARG(uap, off);

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
	SCARG(&pra, fd) = SCARG(uap, fd);
	SCARG(&pra, buf) = SCARG(uap, buf);
	SCARG(&pra, nbyte) = SCARG(uap, nbyte);
	SCARG(&pra, offset) = SCARG(uap, off);

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
	SCARG(&pwa, fd) = SCARG(uap, fd);
	SCARG(&pwa, buf) = SCARG(uap, buf);
	SCARG(&pwa, nbyte) = SCARG(uap, nbyte);
	SCARG(&pwa, offset) = SCARG(uap, off);

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
	SCARG(&pwa, fd) = SCARG(uap, fd);
	SCARG(&pwa, buf) = SCARG(uap, buf);
	SCARG(&pwa, nbyte) = SCARG(uap, nbyte);
	SCARG(&pwa, offset) = SCARG(uap, off);

	return (sys_pwrite(td, &pwa, retval));
}
#endif /* NOTYET */

int
svr4_sys_fcntl(td, uap)
	register struct thread *td;
	struct svr4_sys_fcntl_args *uap;
{
	int				error;
	struct fcntl_args		fa;
	int                             *retval;

	retval = td->td_retval;

	SCARG(&fa, fd) = SCARG(uap, fd);
	SCARG(&fa, cmd) = svr4_to_bsd_cmd(SCARG(uap, cmd));

	switch (SCARG(&fa, cmd)) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
		SCARG(&fa, arg) = (long) SCARG(uap, arg);
		return fcntl(td, &fa);

	case F_GETFL:
		SCARG(&fa, arg) = (long) SCARG(uap, arg);
		error = fcntl(td, &fa);
		if (error)
			return error;
		*retval = bsd_to_svr4_flags(*retval);
		return error;

	case F_SETFL:
		{
			/*
			 * we must save the O_ASYNC flag, as that is
			 * handled by ioctl(_, I_SETSIG, _) emulation.
			 */
			long cmd;
			int flags;

			DPRINTF(("Setting flags %p\n", SCARG(uap, arg)));
			cmd = SCARG(&fa, cmd); /* save it for a while */

			SCARG(&fa, cmd) = F_GETFL;
			if ((error = fcntl(td, &fa)) != 0)
				return error;
			flags = *retval;
			flags &= O_ASYNC;
			flags |= svr4_to_bsd_flags((u_long) SCARG(uap, arg));
			SCARG(&fa, cmd) = cmd;
			SCARG(&fa, arg) = (long) flags;
			return fcntl(td, &fa);
		}

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		{
			struct svr4_flock	 ifl;
			struct flock		*flp, fl;
			caddr_t sg = stackgap_init();

			flp = stackgap_alloc(&sg, sizeof(struct flock));
			SCARG(&fa, arg) = (long) flp;

			error = copyin(SCARG(uap, arg), &ifl, sizeof ifl);
			if (error)
				return error;

			svr4_to_bsd_flock(&ifl, &fl);

			error = copyout(&fl, flp, sizeof fl);
			if (error)
				return error;

			error = fcntl(td, &fa);
			if (error || SCARG(&fa, cmd) != F_GETLK)
				return error;

			error = copyin(flp, &fl, sizeof fl);
			if (error)
				return error;

			bsd_to_svr4_flock(&fl, &ifl);

			return copyout(&ifl, SCARG(uap, arg), sizeof ifl);
		}
	case -1:
		switch (SCARG(uap, cmd)) {
		case SVR4_F_DUP2FD:
			{
				struct dup2_args du;

				SCARG(&du, from) = SCARG(uap, fd);
				SCARG(&du, to) = (int)SCARG(uap, arg);
				error = dup2(td, &du);
				if (error)
					return error;
				*retval = SCARG(&du, to);
				return 0;
			}

		case SVR4_F_FREESP:
			{
				struct svr4_flock	 ifl;
				struct flock		 fl;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock(&ifl, &fl);
				return fd_truncate(td, SCARG(uap, fd), &fl);
			}

		case SVR4_F_GETLK64:
		case SVR4_F_SETLK64:
		case SVR4_F_SETLKW64:
			{
				struct svr4_flock64	 ifl;
				struct flock		*flp, fl;
				caddr_t sg = stackgap_init();

				flp = stackgap_alloc(&sg, sizeof(struct flock));
				SCARG(&fa, arg) = (long) flp;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;

				svr4_to_bsd_flock64(&ifl, &fl);

				error = copyout(&fl, flp, sizeof fl);
				if (error)
					return error;

				error = fcntl(td, &fa);
				if (error || SCARG(&fa, cmd) != F_GETLK)
					return error;

				error = copyin(flp, &fl, sizeof fl);
				if (error)
					return error;

				bsd_to_svr4_flock64(&fl, &ifl);

				return copyout(&ifl, SCARG(uap, arg),
				    sizeof ifl);
			}

		case SVR4_F_FREESP64:
			{
				struct svr4_flock64	 ifl;
				struct flock		 fl;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock64(&ifl, &fl);
				return fd_truncate(td, SCARG(uap, fd), &fl);
			}

		case SVR4_F_REVOKE:
			return fd_revoke(td, SCARG(uap, fd));

		default:
			return ENOSYS;
		}

	default:
		return ENOSYS;
	}
}
