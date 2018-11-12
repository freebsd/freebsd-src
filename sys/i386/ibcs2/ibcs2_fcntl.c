/*-
 * Copyright (c) 1995 Scott Bartram
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "opt_spx_hack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/ttycom.h>

#include <i386/ibcs2/ibcs2_fcntl.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_util.h>

static void cvt_iflock2flock(struct ibcs2_flock *, struct flock *);
static void cvt_flock2iflock(struct flock *, struct ibcs2_flock *);
static int  cvt_o_flags(int);
static int  oflags2ioflags(int);
static int  ioflags2oflags(int);

static int
cvt_o_flags(flags)
	int flags;
{
	int r = 0;

        /* convert mode into NetBSD mode */
	if (flags & IBCS2_O_WRONLY) r |= O_WRONLY;
	if (flags & IBCS2_O_RDWR)   r |= O_RDWR;
	if (flags & (IBCS2_O_NDELAY | IBCS2_O_NONBLOCK)) r |= O_NONBLOCK;
	if (flags & IBCS2_O_APPEND) r |= O_APPEND;
	if (flags & IBCS2_O_SYNC)   r |= O_FSYNC;
	if (flags & IBCS2_O_CREAT)  r |= O_CREAT;
	if (flags & IBCS2_O_TRUNC)  r |= O_TRUNC /* | O_CREAT ??? */;
	if (flags & IBCS2_O_EXCL)   r |= O_EXCL;
	if (flags & IBCS2_O_RDONLY) r |= O_RDONLY;
	if (flags & IBCS2_O_PRIV)   r |= O_EXLOCK;
	if (flags & IBCS2_O_NOCTTY) r |= O_NOCTTY;
	return r;
}

static void
cvt_flock2iflock(flp, iflp)
	struct flock *flp;
	struct ibcs2_flock *iflp;
{
	switch (flp->l_type) {
	case F_RDLCK:
		iflp->l_type = IBCS2_F_RDLCK;
		break;
	case F_WRLCK:
		iflp->l_type = IBCS2_F_WRLCK;
		break;
	case F_UNLCK:
		iflp->l_type = IBCS2_F_UNLCK;
		break;
	}
	iflp->l_whence = (short)flp->l_whence;
	iflp->l_start = (ibcs2_off_t)flp->l_start;
	iflp->l_len = (ibcs2_off_t)flp->l_len;
	iflp->l_sysid = flp->l_sysid;
	iflp->l_pid = (ibcs2_pid_t)flp->l_pid;
}

#ifdef DEBUG_IBCS2
static void
print_flock(struct flock *flp)
{
  printf("flock: start=%x len=%x pid=%d type=%d whence=%d\n",
	 (int)flp->l_start, (int)flp->l_len, (int)flp->l_pid,
	 flp->l_type, flp->l_whence);
}
#endif

static void
cvt_iflock2flock(iflp, flp)
	struct ibcs2_flock *iflp;
	struct flock *flp;
{
	flp->l_start = (off_t)iflp->l_start;
	flp->l_len = (off_t)iflp->l_len;
	flp->l_pid = (pid_t)iflp->l_pid;
	switch (iflp->l_type) {
	case IBCS2_F_RDLCK:
		flp->l_type = F_RDLCK;
		break;
	case IBCS2_F_WRLCK:
		flp->l_type = F_WRLCK;
		break;
	case IBCS2_F_UNLCK:
		flp->l_type = F_UNLCK;
		break;
	}
	flp->l_whence = iflp->l_whence;
	flp->l_sysid = iflp->l_sysid;
}

/* convert iBCS2 mode into NetBSD mode */
static int
ioflags2oflags(flags)
	int flags;
{
	int r = 0;
	
	if (flags & IBCS2_O_RDONLY) r |= O_RDONLY;
	if (flags & IBCS2_O_WRONLY) r |= O_WRONLY;
	if (flags & IBCS2_O_RDWR) r |= O_RDWR;
	if (flags & IBCS2_O_NDELAY) r |= O_NONBLOCK;
	if (flags & IBCS2_O_APPEND) r |= O_APPEND;
	if (flags & IBCS2_O_SYNC) r |= O_FSYNC;
	if (flags & IBCS2_O_NONBLOCK) r |= O_NONBLOCK;
	if (flags & IBCS2_O_CREAT) r |= O_CREAT;
	if (flags & IBCS2_O_TRUNC) r |= O_TRUNC;
	if (flags & IBCS2_O_EXCL) r |= O_EXCL;
	if (flags & IBCS2_O_NOCTTY) r |= O_NOCTTY;
	return r;
}

/* convert NetBSD mode into iBCS2 mode */
static int
oflags2ioflags(flags)
	int flags;
{
	int r = 0;
	
	if (flags & O_RDONLY) r |= IBCS2_O_RDONLY;
	if (flags & O_WRONLY) r |= IBCS2_O_WRONLY;
	if (flags & O_RDWR) r |= IBCS2_O_RDWR;
	if (flags & O_NDELAY) r |= IBCS2_O_NONBLOCK;
	if (flags & O_APPEND) r |= IBCS2_O_APPEND;
	if (flags & O_FSYNC) r |= IBCS2_O_SYNC;
	if (flags & O_NONBLOCK) r |= IBCS2_O_NONBLOCK;
	if (flags & O_CREAT) r |= IBCS2_O_CREAT;
	if (flags & O_TRUNC) r |= IBCS2_O_TRUNC;
	if (flags & O_EXCL) r |= IBCS2_O_EXCL;
	if (flags & O_NOCTTY) r |= IBCS2_O_NOCTTY;
	return r;
}

int
ibcs2_open(td, uap)
	struct thread *td;
	struct ibcs2_open_args *uap;
{
	struct proc *p;
	char *path;
	int flags, noctty, ret;

	p = td->td_proc;
	noctty = uap->flags & IBCS2_O_NOCTTY;
	flags = cvt_o_flags(uap->flags);
	if (uap->flags & O_CREAT)
		CHECKALTCREAT(td, uap->path, &path);
	else
		CHECKALTEXIST(td, uap->path, &path);
	ret = kern_openat(td, AT_FDCWD, path, UIO_SYSSPACE, flags, uap->mode);

#ifdef SPX_HACK
	if (ret == ENXIO) {
		if (!strcmp(path, "/compat/ibcs2/dev/spx"))
			ret = spx_open(td);
		free(path, M_TEMP);
	} else
#endif /* SPX_HACK */
	free(path, M_TEMP);
	PROC_LOCK(p);
	if (!ret && !noctty && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
		cap_rights_t rights;
		struct file *fp;
		int error;

		error = fget(td, td->td_retval[0],
		    cap_rights_init(&rights, CAP_IOCTL), &fp);
		PROC_UNLOCK(p);
		if (error)
			return (EBADF);

		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, td->td_ucred,
			    td);
		fdrop(fp, td);
	} else
		PROC_UNLOCK(p);
	return ret;
}

int
ibcs2_creat(td, uap)
        struct thread *td;  
	struct ibcs2_creat_args *uap;
{
	char *path;
	int error;

	CHECKALTCREAT(td, uap->path, &path);
	error = kern_openat(td, AT_FDCWD, path, UIO_SYSSPACE,
	    O_WRONLY | O_CREAT | O_TRUNC, uap->mode);
	free(path, M_TEMP);
	return (error);
}       

int
ibcs2_access(td, uap)
        struct thread *td;
        struct ibcs2_access_args *uap;
{
	char *path;
	int error;

        CHECKALTEXIST(td, uap->path, &path);
	error = kern_accessat(td, AT_FDCWD, path, UIO_SYSSPACE, 0, uap->amode);
	free(path, M_TEMP);
	return (error);
}

int
ibcs2_fcntl(td, uap)
	struct thread *td;
	struct ibcs2_fcntl_args *uap;
{
	intptr_t arg;
	int error;
	struct flock fl;
	struct ibcs2_flock ifl;

	arg = (intptr_t)uap->arg;
	switch(uap->cmd) {
	case IBCS2_F_DUPFD:
		return (kern_fcntl(td, uap->fd, F_DUPFD, arg));
	case IBCS2_F_GETFD:
		return (kern_fcntl(td, uap->fd, F_GETFD, arg));
	case IBCS2_F_SETFD:
		return (kern_fcntl(td, uap->fd, F_SETFD, arg));
	case IBCS2_F_GETFL:
		error = kern_fcntl(td, uap->fd, F_GETFL, arg);
		if (error)
			return error;
		td->td_retval[0] = oflags2ioflags(td->td_retval[0]);
		return error;
	case IBCS2_F_SETFL:
		return (kern_fcntl(td, uap->fd, F_SETFL,
		    ioflags2oflags(arg)));

	case IBCS2_F_GETLK:
	    {
		error = copyin((caddr_t)uap->arg, (caddr_t)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, &fl);
		error = kern_fcntl(td, uap->fd, F_GETLK, (intptr_t)&fl);
		if (error)
			return error;
		cvt_flock2iflock(&fl, &ifl);
		return copyout((caddr_t)&ifl, (caddr_t)uap->arg,
			       ibcs2_flock_len);
	    }

	case IBCS2_F_SETLK:
	    {
		error = copyin((caddr_t)uap->arg, (caddr_t)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, &fl);
		return (kern_fcntl(td, uap->fd, F_SETLK, (intptr_t)&fl));
	    }

	case IBCS2_F_SETLKW:
	    {
		error = copyin((caddr_t)uap->arg, (caddr_t)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, &fl);
		return (kern_fcntl(td, uap->fd, F_SETLKW, (intptr_t)&fl));
	    }
	}
	return ENOSYS;
}
