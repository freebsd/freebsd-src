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
 *	from: @(#)sys_generic.c	7.30 (Berkeley) 5/30/91
 *	$Id: sys_generic.c,v 1.6 1993/12/19 00:51:34 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "filedesc.h"
#include "ioctl.h"
#include "file.h"
#include "socketvar.h"
#include "proc.h"
#include "uio.h"
#include "kernel.h"
#include "stat.h"
#include "malloc.h"
#include "signalvar.h"
#ifdef KTRACE
#include "ktrace.h"
#endif

struct read_args {
	int	fdes;
	char	*cbuf;
	unsigned count;
};

/*
 * Read system call.
 */
/* ARGSUSED */
int
read(p, uap, retval)
	struct proc *p;
	register struct read_args *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);
	aiov.iov_base = (caddr_t)uap->cbuf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->count;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif
	cnt = uap->count;
	if (error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred))
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, uap->fdes, UIO_READ, &ktriov, cnt, error);
#endif
	*retval = cnt;
	return (error);
}

/*
 * Scatter read system call.
 */

struct readv_args {
	int	fdes;
	struct	iovec *iovp;
	unsigned iovcnt;
};

/* ARGSUSED */
int
readv(p, uap, retval)
	struct proc *p;
	register struct readv_args *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	register struct iovec *iov;
	struct iovec *saveiov = 0;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	unsigned iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);
	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		saveiov = iov;
	} else
		iov = aiov;
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	if (error = copyin((caddr_t)uap->iovp, (caddr_t)iov, iovlen))
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len < 0) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		if (auio.uio_resid < 0) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	if (error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred))
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, uap->fdes, UIO_READ, ktriov,
			    cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	*retval = cnt;
done:
	if (uap->iovcnt > UIO_SMALLIOV)
		FREE(saveiov, M_IOV);
	return (error);
}

/*
 * Write system call
 */

struct write_args {
	int	fdes;
	char	*cbuf;
	unsigned count;
};

int
write(p, uap, retval)
	struct proc *p;
	register struct write_args *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);
	aiov.iov_base = (caddr_t)uap->cbuf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->count;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif
	cnt = uap->count;
	if (error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred)) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, uap->fdes, UIO_WRITE,
		    &ktriov, cnt, error);
#endif
	*retval = cnt;
	return (error);
}

/*
 * Gather write system call
 */

struct writev_args {
	int	fdes;
	struct	iovec *iovp;
	unsigned iovcnt;
};

int
writev(p, uap, retval)
	struct proc *p;
	register struct writev_args *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	register struct iovec *iov;
	struct iovec *saveiov = 0;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	unsigned iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);
	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		saveiov = iov;
	} else
		iov = aiov;
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	if (error = copyin((caddr_t)uap->iovp, (caddr_t)iov, iovlen))
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len < 0) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		if (auio.uio_resid < 0) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	if (error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred)) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, uap->fdes, UIO_WRITE,
				ktriov, cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	*retval = cnt;
done:
	if (uap->iovcnt > UIO_SMALLIOV)
		FREE(saveiov, M_IOV);
	return (error);
}

/*
 * Ioctl system call
 */

struct ioctl_args {
	int	fdes;
	int	cmd;
	caddr_t	cmarg;
};

/* ARGSUSED */
int
ioctl(p, uap, retval)
	struct proc *p;
	register struct ioctl_args *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	register int com, error;
	register u_int size;
	caddr_t memp = 0;
#define STK_PARAMS	128
	char stkbuf[STK_PARAMS];
	caddr_t data = stkbuf;
	int tmp;

	if ((unsigned)uap->fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL)
		return (EBADF);
	if ((fp->f_flag & (FREAD|FWRITE)) == 0)
		return (EBADF);
	com = uap->cmd;

	if (com == FIOCLEX) {
		fdp->fd_ofileflags[uap->fdes] |= UF_EXCLOSE;
		return (0);
	}
	if (com == FIONCLEX) {
		fdp->fd_ofileflags[uap->fdes] &= ~UF_EXCLOSE;
		return (0);
	}

	/*
	 * Interpret high order word to find
	 * amount of data to be copied to/from the
	 * user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);
	if (size > sizeof (stkbuf)) {
		memp = (caddr_t)malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	}
	if (com&IOC_IN) {
		if (size) {
			error = copyin(uap->cmarg, data, (u_int)size);
			if (error) {
				if (memp)
					free(memp, M_IOCTLOPS);
				return (error);
			}
		} else
			*(caddr_t *)data = uap->cmarg;
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(data, size);
	else if (com&IOC_VOID)
		*(caddr_t *)data = uap->cmarg;

	switch (com) {

	case FIONBIO:
		if (tmp = *(int *)data)
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		break;

	case FIOASYNC:
		if (tmp = *(int *)data)
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		break;

	case FIOSETOWN:
		tmp = *(int *)data;
		if (fp->f_type == DTYPE_SOCKET) {
			((struct socket *)fp->f_data)->so_pgid = tmp;
			error = 0;
			break;
		}
		if (tmp <= 0) {
			tmp = -tmp;
		} else {
			struct proc *p1 = pfind(tmp);
			if (p1 == 0) {
				error = ESRCH;
				break;
			}
			tmp = p1->p_pgrp->pg_id;
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, (int)TIOCSPGRP, (caddr_t)&tmp, p);
		break;

	case FIOGETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			error = 0;
			*(int *)data = ((struct socket *)fp->f_data)->so_pgid;
			break;
		}
		error = (*fp->f_ops->fo_ioctl)(fp, (int)TIOCGPGRP, data, p);
		*(int *)data = -*(int *)data;
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, p);
		/*
		 * Copy any data to user, size was
		 * already set and checked above.
		 */
		if (error == 0 && (com&IOC_OUT) && size)
			error = copyout(data, uap->cmarg, (u_int)size);
		break;
	}
	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}

int	selwait, nselcoll;

/*
 * Select system call.
 */

struct select_args {
		u_int	nd;
		fd_set	*in, *ou, *ex;
		struct	timeval *tv;
};

int
select(p, uap, retval)
	register struct proc *p;
	register struct select_args *uap;
	int *retval;
{
	fd_set ibits[3], obits[3];
	struct timeval atv;
	int s, ncoll, error = 0, timo;
	u_int ni;

	bzero((caddr_t)ibits, sizeof(ibits));
	bzero((caddr_t)obits, sizeof(obits));
	if (uap->nd > p->p_fd->fd_nfiles)
		uap->nd = p->p_fd->fd_nfiles;	/* forgiving; slightly wrong */
	ni = howmany(uap->nd, NFDBITS);

#define	getbits(name, x) \
	if (uap->name) { \
		error = copyin((caddr_t)uap->name, (caddr_t)&ibits[x], \
		    (unsigned)(ni * sizeof(fd_mask))); \
		if (error) \
			goto done; \
	}
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits

	if (uap->tv) {
		error = copyin((caddr_t)uap->tv, (caddr_t)&atv,
			sizeof (atv));
		if (error)
			goto done;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		s = splhigh(); timevaladd(&atv, &time); splx(s);
		timo = hzto(&atv);
	} else
		timo = 0;
retry:
	ncoll = nselcoll;
	p->p_flag |= SSEL;
	error = selscan(p, ibits, obits, uap->nd, retval);
	if (error || *retval)
		goto done;
	s = splhigh();
	/* this should be timercmp(&time, &atv, >=) */
	if (uap->tv && (time.tv_sec > atv.tv_sec ||
	    time.tv_sec == atv.tv_sec && time.tv_usec >= atv.tv_usec)) {
		splx(s);
		goto done;
	}
	if ((p->p_flag & SSEL) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~SSEL;
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "select", timo);
	splx(s);
	if (error == 0)
		goto retry;
done:
	p->p_flag &= ~SSEL;
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
#define	putbits(name, x) \
	if (uap->name) { \
		int error2 = copyout((caddr_t)&obits[x], (caddr_t)uap->name, \
		    (unsigned)(ni * sizeof(fd_mask))); \
		if (error2) \
			error = error2; \
	}
	if (error == 0) {
		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
#undef putbits
	}
	return (error);
}

int
selscan(struct proc *p, fd_set *ibits, fd_set *obits, int nfd, int *retval)
{
	register struct filedesc *fdp = p->p_fd;
	register int which, i, j;
	register fd_mask bits;
	int flag;
	struct file *fp;
	int error = 0, n = 0;

	for (which = 0; which < 3; which++) {
		switch (which) {

		case 0:
			flag = FREAD; break;

		case 1:
			flag = FWRITE; break;

		default:	/* pacify GCC */
		case 2:
			flag = 0; break;
		}
		for (i = 0; i < nfd; i += NFDBITS) {
			bits = ibits[which].fds_bits[i/NFDBITS];
			while ((j = ffs(bits)) && i + --j < nfd) {
				bits &= ~(1 << j);
				fp = fdp->fd_ofiles[i + j];
				if (fp == NULL) {
					error = EBADF;
					break;
				}
				if ((*fp->f_ops->fo_select)(fp, flag, p)) {
					FD_SET(i + j, &obits[which]);
					n++;
				}
			}
		}
	}
	*retval = n;
	return (error);
}

/*ARGSUSED*/
int
seltrue(int /*dev_t*/ dev, int which, struct proc *p)
{

	return (1);
}

void
selwakeup(int /*pid_t*/ pid, int coll)
{
	register struct proc *p;

	if (coll) {
		nselcoll++;
		wakeup((caddr_t)&selwait);
	}
	if (pid && (p = pfind(pid))) {
		int s = splhigh();
		if (p->p_wchan == (caddr_t)&selwait) {
			if (p->p_stat == SSLEEP)
				setrun(p);
			else
				unsleep(p);
		} else if (p->p_flag & SSEL)
			p->p_flag &= ~SSEL;
		splx(s);
	}
}
