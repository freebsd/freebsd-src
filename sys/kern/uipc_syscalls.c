/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)uipc_syscalls.c	8.4 (Berkeley) 2/21/94
 * $Id: uipc_syscalls.c,v 1.30 1997/09/02 20:05:58 bde Exp $
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

extern int sendit __P((struct proc *p, int s, struct msghdr *mp, int flags,
		       int *retsize));
extern int recvit __P((struct proc *p, int s, struct msghdr *mp,
		       caddr_t namelenp, int *retsize));
  
static int accept1 __P((struct proc *p, struct accept_args *uap, int *retval,
			int compat));
static int getsockname1 __P((struct proc *p, struct getsockname_args *uap,
			     int *retval, int compat));
static int getpeername1 __P((struct proc *p, struct getpeername_args *uap,
			     int *retval, int compat));

/*
 * System call interface to the socket abstraction.
 */
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#define COMPAT_OLDSOCK
#endif

extern	struct fileops socketops;

int
socket(p, uap, retval)
	struct proc *p;
	register struct socket_args /* {
		int	domain;
		int	type;
		int	protocol;
	} */ *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct socket *so;
	struct file *fp;
	int fd, error;

	error = falloc(p, &fp, &fd);
	if (error)
		return (error);
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	error = socreate(uap->domain, &so, uap->type, uap->protocol, p);
	if (error) {
		fdp->fd_ofiles[fd] = 0;
		ffree(fp);
	} else {
		fp->f_data = (caddr_t)so;
		*retval = fd;
	}
	return (error);
}

/* ARGSUSED */
int
bind(p, uap, retval)
	struct proc *p;
	register struct bind_args /* {
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	struct sockaddr *sa;
	int error;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error)
		return (error);
	error = sobind((struct socket *)fp->f_data, sa, p);
	FREE(sa, M_SONAME);
	return (error);
}

/* ARGSUSED */
int
listen(p, uap, retval)
	struct proc *p;
	register struct listen_args /* {
		int	s;
		int	backlog;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	int error;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	return (solisten((struct socket *)fp->f_data, uap->backlog, p));
}

static int
accept1(p, uap, retval, compat)
	struct proc *p;
	register struct accept_args /* {
		int	s;
		caddr_t	name;
		int	*anamelen;
	} */ *uap;
	int *retval;
	int compat;
{
	struct file *fp;
	struct sockaddr *sa;
	int namelen, error, s;
	struct socket *head, *so;
	short fflag;		/* type must match fp->f_flag */

	if (uap->name) {
		error = copyin((caddr_t)uap->anamelen, (caddr_t)&namelen,
			sizeof (namelen));
		if(error)
			return (error);
	}
	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	s = splnet();
	head = (struct socket *)fp->f_data;
	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		splx(s);
		return (EINVAL);
	}
	if ((head->so_state & SS_NBIO) && head->so_comp.tqh_first == NULL) {
		splx(s);
		return (EWOULDBLOCK);
	}
	while (head->so_comp.tqh_first == NULL && head->so_error == 0) {
		if (head->so_state & SS_CANTRCVMORE) {
			head->so_error = ECONNABORTED;
			break;
		}
		error = tsleep((caddr_t)&head->so_timeo, PSOCK | PCATCH,
		    "accept", 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	if (head->so_error) {
		error = head->so_error;
		head->so_error = 0;
		splx(s);
		return (error);
	}

	/*
	 * At this point we know that there is at least one connection
	 * ready to be accepted. Remove it from the queue prior to
	 * allocating the file descriptor for it since falloc() may
	 * block allowing another process to accept the connection
	 * instead.
	 */
	so = head->so_comp.tqh_first;
	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;

	fflag = fp->f_flag;
	error = falloc(p, &fp, retval);
	if (error) {
		/*
		 * Probably ran out of file descriptors. Put the
		 * unaccepted connection back onto the queue and
		 * do another wakeup so some other process might
		 * have a chance at it.
		 */
		TAILQ_INSERT_HEAD(&head->so_comp, so, so_list);
		head->so_qlen++;
		wakeup_one(&head->so_timeo);
		splx(s);
		return (error);
	}

	so->so_state &= ~SS_COMP;
	so->so_head = NULL;

	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = fflag;
	fp->f_ops = &socketops;
	fp->f_data = (caddr_t)so;
	sa = 0;
	(void) soaccept(so, &sa);
	if (sa == 0) {
		namelen = 0;
		if (uap->name)
			goto gotnoname;
		return 0;
	}
	if (uap->name) {
#ifdef COMPAT_OLDSOCK
		if (compat)
			((struct osockaddr *)sa)->sa_family =
			    sa->sa_family;
#endif
		if (namelen > sa->sa_len)
			namelen = sa->sa_len;
		error = copyout(sa, (caddr_t)uap->name, (u_int)namelen);
		if (!error)
gotnoname:
			error = copyout((caddr_t)&namelen,
			    (caddr_t)uap->anamelen, sizeof (*uap->anamelen));
	}
	FREE(sa, M_SONAME);
	splx(s);
	return (error);
}

int
accept(p, uap, retval)
	struct proc *p;
	struct accept_args *uap;
	int *retval;
{

	return (accept1(p, uap, retval, 0));
}

#ifdef COMPAT_OLDSOCK
int
oaccept(p, uap, retval)
	struct proc *p;
	struct accept_args *uap;
	int *retval;
{

	return (accept1(p, uap, retval, 1));
}
#endif /* COMPAT_OLDSOCK */

/* ARGSUSED */
int
connect(p, uap, retval)
	struct proc *p;
	register struct connect_args /* {
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	register struct socket *so;
	struct sockaddr *sa;
	int error, s;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
		return (EALREADY);
	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error)
		return (error);
	error = soconnect(so, sa, p);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		FREE(sa, M_SONAME);
		return (EINPROGRESS);
	}
	s = splnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		    "connec", 0);
		if (error)
			break;
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	splx(s);
bad:
	so->so_state &= ~SS_ISCONNECTING;
	FREE(sa, M_SONAME);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

int
socketpair(p, uap, retval)
	struct proc *p;
	register struct socketpair_args /* {
		int	domain;
		int	type;
		int	protocol;
		int	*rsv;
	} */ *uap;
	int retval[];
{
	register struct filedesc *fdp = p->p_fd;
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, sv[2];

	error = socreate(uap->domain, &so1, uap->type, uap->protocol, p);
	if (error)
		return (error);
	error = socreate(uap->domain, &so2, uap->type, uap->protocol, p);
	if (error)
		goto free1;
	error = falloc(p, &fp1, &fd);
	if (error)
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = (caddr_t)so1;
	error = falloc(p, &fp2, &fd);
	if (error)
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = (caddr_t)so2;
	sv[1] = fd;
	error = soconnect2(so1, so2);
	if (error)
		goto free4;
	if (uap->type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 error = soconnect2(so2, so1);
		 if (error)
			goto free4;
	}
	error = copyout((caddr_t)sv, (caddr_t)uap->rsv, 2 * sizeof (int));
#if 0	/* old pipe(2) syscall compatability, unused these days */
	retval[0] = sv[0];		/* XXX ??? */
	retval[1] = sv[1];		/* XXX ??? */
#endif
	return (error);
free4:
	ffree(fp2);
	fdp->fd_ofiles[sv[1]] = 0;
free3:
	ffree(fp1);
	fdp->fd_ofiles[sv[0]] = 0;
free2:
	(void)soclose(so2);
free1:
	(void)soclose(so1);
	return (error);
}

int
sendit(p, s, mp, flags, retsize)
	register struct proc *p;
	int s;
	register struct msghdr *mp;
	int flags, *retsize;
{
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *control;
	struct sockaddr *to;
	int len, error;
	struct socket *so;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	error = getsock(p->p_fd, s, &fp);
	if (error)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = p;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if ((auio.uio_resid += iov->iov_len) < 0)
			return (EINVAL);
	}
	if (mp->msg_name) {
		error = getsockaddr(&to, mp->msg_name, mp->msg_namelen);
		if (error)
			return (error);
	} else
		to = 0;
	if (mp->msg_control) {
		if (mp->msg_controllen < sizeof(struct cmsghdr)
#ifdef COMPAT_OLDSOCK
		    && mp->msg_flags != MSG_COMPAT
#endif
		) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL);
		if (error)
			goto bad;
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags == MSG_COMPAT) {
			register struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAIT);
			if (control == 0) {
				error = ENOBUFS;
				goto bad;
			} else {
				cm = mtod(control, struct cmsghdr *);
				cm->cmsg_len = control->m_len;
				cm->cmsg_level = SOL_SOCKET;
				cm->cmsg_type = SCM_RIGHTS;
			}
		}
#endif
	} else
		control = 0;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	so = (struct socket *)fp->f_data;
	error = so->so_proto->pr_usrreqs->pru_sosend(so, to, &auio, 0, control,
						     flags, p);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	if (error == 0)
		*retsize = len - auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, s, UIO_WRITE,
				ktriov, *retsize, error);
		FREE(ktriov, M_TEMP);
	}
#endif
bad:
	if (to)
		FREE(to, M_SONAME);
	return (error);
}

int
sendto(p, uap, retval)
	struct proc *p;
	register struct sendto_args /* {
		int	s;
		caddr_t	buf;
		size_t	len;
		int	flags;
		caddr_t	to;
		int	tolen;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = uap->to;
	msg.msg_namelen = uap->tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	return (sendit(p, uap->s, &msg, uap->flags, retval));
}

#ifdef COMPAT_OLDSOCK
int
osend(p, uap, retval)
	struct proc *p;
	register struct osend_args /* {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(p, uap->s, &msg, uap->flags, retval));
}

int
osendmsg(p, uap, retval)
	struct proc *p;
	register struct osendmsg_args /* {
		int	s;
		caddr_t	msg;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin(uap->msg, (caddr_t)&msg, sizeof (struct omsghdr));
	if (error)
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		      M_WAITOK);
	} else
		iov = aiov;
	error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	msg.msg_flags = MSG_COMPAT;
	msg.msg_iov = iov;
	error = sendit(p, uap->s, &msg, uap->flags, retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

int
sendmsg(p, uap, retval)
	struct proc *p;
	register struct sendmsg_args /* {
		int	s;
		caddr_t	msg;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin(uap->msg, (caddr_t)&msg, sizeof (msg));
	if (error)
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else
		iov = aiov;
	if (msg.msg_iovlen &&
	    (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)))))
		goto done;
	msg.msg_iov = iov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	error = sendit(p, uap->s, &msg, uap->flags, retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

int
recvit(p, s, mp, namelenp, retsize)
	register struct proc *p;
	int s;
	register struct msghdr *mp;
	caddr_t namelenp;
	int *retsize;
{
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	int len, error;
	struct mbuf *m, *control = 0;
	caddr_t ctlbuf;
	struct socket *so;
	struct sockaddr *fromsa = 0;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	error = getsock(p->p_fd, s, &fp);
	if (error)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = p;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if ((auio.uio_resid += iov->iov_len) < 0)
			return (EINVAL);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	so = (struct socket *)fp->f_data;
	error = so->so_proto->pr_usrreqs->pru_soreceive(so, &fromsa, &auio,
	    (struct mbuf **)0, mp->msg_control ? &control : (struct mbuf **)0,
	    &mp->msg_flags);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, s, UIO_READ,
				ktriov, len - auio.uio_resid, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	if (error)
		goto out;
	*retsize = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || fromsa == 0)
			len = 0;
		else {
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				((struct osockaddr *)fromsa)->sa_family =
				    fromsa->sa_family;
#endif
#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif
			len = MIN(len, fromsa->sa_len);
			error = copyout(fromsa,
			    (caddr_t)mp->msg_name, (unsigned)len);
			if (error)
				goto out;
		}
		mp->msg_namelen = len;
		if (namelenp &&
		    (error = copyout((caddr_t)&len, namelenp, sizeof (int)))) {
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				error = 0;	/* old recvfrom didn't check */
			else
#endif
			goto out;
		}
	}
	if (mp->msg_control) {
#ifdef COMPAT_OLDSOCK
		/*
		 * We assume that old recvmsg calls won't receive access
		 * rights and other control info, esp. as control info
		 * is always optional and those options didn't exist in 4.3.
		 * If we receive rights, trim the cmsghdr; anything else
		 * is tossed.
		 */
		if (control && mp->msg_flags & MSG_COMPAT) {
			if (mtod(control, struct cmsghdr *)->cmsg_level !=
			    SOL_SOCKET ||
			    mtod(control, struct cmsghdr *)->cmsg_type !=
			    SCM_RIGHTS) {
				mp->msg_controllen = 0;
				goto out;
			}
			control->m_len -= sizeof (struct cmsghdr);
			control->m_data += sizeof (struct cmsghdr);
		}
#endif
		len = mp->msg_controllen;
		m = control;
		mp->msg_controllen = 0;
		ctlbuf = (caddr_t) mp->msg_control;

		while (m && len > 0) {
			unsigned int tocopy;

			if (len >= m->m_len) 
				tocopy = m->m_len;
			else {
				mp->msg_flags |= MSG_CTRUNC;
				tocopy = len;
			}
		
			if (error = copyout((caddr_t)mtod(m, caddr_t),
					ctlbuf, tocopy))
				goto out;

			ctlbuf += tocopy;
			len -= tocopy;
			m = m->m_next;
		}
		mp->msg_controllen = ctlbuf - mp->msg_control;
	}
out:
	if (fromsa)
		FREE(fromsa, M_SONAME);
	if (control)
		m_freem(control);
	return (error);
}

int
recvfrom(p, uap, retval)
	struct proc *p;
	register struct recvfrom_args /* {
		int	s;
		caddr_t	buf;
		size_t	len;
		int	flags;
		caddr_t	from;
		int	*fromlenaddr;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (uap->fromlenaddr) {
		error = copyin((caddr_t)uap->fromlenaddr,
		    (caddr_t)&msg.msg_namelen, sizeof (msg.msg_namelen));
		if (error)
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = uap->from;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	return (recvit(p, uap->s, &msg, (caddr_t)uap->fromlenaddr, retval));
}

#ifdef COMPAT_OLDSOCK
int
orecvfrom(p, uap, retval)
	struct proc *p;
	struct recvfrom_args *uap;
	int *retval;
{

	uap->flags |= MSG_COMPAT;
	return (recvfrom(p, uap, retval));
}
#endif


#ifdef COMPAT_OLDSOCK
int
orecv(p, uap, retval)
	struct proc *p;
	register struct orecv_args /* {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	return (recvit(p, uap->s, &msg, (caddr_t)0, retval));
}

/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
int
orecvmsg(p, uap, retval)
	struct proc *p;
	register struct orecvmsg_args /* {
		int	s;
		struct	omsghdr *msg;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin((caddr_t)uap->msg, (caddr_t)&msg,
	    sizeof (struct omsghdr));
	if (error)
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		      M_WAITOK);
	} else
		iov = aiov;
	msg.msg_flags = uap->flags | MSG_COMPAT;
	error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	msg.msg_iov = iov;
	error = recvit(p, uap->s, &msg, (caddr_t)&uap->msg->msg_namelen, retval);

	if (msg.msg_controllen && error == 0)
		error = copyout((caddr_t)&msg.msg_controllen,
		    (caddr_t)&uap->msg->msg_accrightslen, sizeof (int));
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

int
recvmsg(p, uap, retval)
	struct proc *p;
	register struct recvmsg_args /* {
		int	s;
		struct	msghdr *msg;
		int	flags;
	} */ *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	register int error;

	error = copyin((caddr_t)uap->msg, (caddr_t)&msg, sizeof (msg));
	if (error)
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else
		iov = aiov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = uap->flags &~ MSG_COMPAT;
#else
	msg.msg_flags = uap->flags;
#endif
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	error = copyin((caddr_t)uiov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	error = recvit(p, uap->s, &msg, (caddr_t)0, retval);
	if (!error) {
		msg.msg_iov = uiov;
		error = copyout((caddr_t)&msg, (caddr_t)uap->msg, sizeof(msg));
	}
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

/* ARGSUSED */
int
shutdown(p, uap, retval)
	struct proc *p;
	register struct shutdown_args /* {
		int	s;
		int	how;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	int error;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	return (soshutdown((struct socket *)fp->f_data, uap->how));
}

/* ARGSUSED */
int
setsockopt(p, uap, retval)
	struct proc *p;
	register struct setsockopt_args /* {
		int	s;
		int	level;
		int	name;
		caddr_t	val;
		int	valsize;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	if (uap->valsize > MLEN)
		return (EINVAL);
	if (uap->val) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		error = copyin(uap->val, mtod(m, caddr_t), (u_int)uap->valsize);
		if (error) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = uap->valsize;
	}
	return (sosetopt((struct socket *)fp->f_data, uap->level,
	    uap->name, m, p));
}

/* ARGSUSED */
int
getsockopt(p, uap, retval)
	struct proc *p;
	register struct getsockopt_args /* {
		int	s;
		int	level;
		int	name;
		caddr_t	val;
		int	*avalsize;
	} */ *uap;
	int *retval;
{
	struct file *fp;
	struct mbuf *m = NULL, *m0;
	int op, i, valsize, error;

	error = getsock(p->p_fd, uap->s, &fp);
	if (error)
		return (error);
	if (uap->val) {
		error = copyin((caddr_t)uap->avalsize, (caddr_t)&valsize,
		    sizeof (valsize));
		if (error)
			return (error);
	} else
		valsize = 0;
	if ((error = sogetopt((struct socket *)fp->f_data, uap->level,
	    uap->name, &m, p)) == 0 && uap->val && valsize && m != NULL) {
		op = 0;
		while (m && !error && op < valsize) {
			i = min(m->m_len, (valsize - op));
			error = copyout(mtod(m, caddr_t), uap->val, (u_int)i);
			op += i;
			uap->val += i;
			m0 = m;
			MFREE(m0,m);
		}
		valsize = op;
		if (error == 0)
			error = copyout((caddr_t)&valsize,
			    (caddr_t)uap->avalsize, sizeof (valsize));
	}
	if (m != NULL)
		(void) m_free(m);
	return (error);
}

/*
 * Get socket name.
 */
/* ARGSUSED */
static int
getsockname1(p, uap, retval, compat)
	struct proc *p;
	register struct getsockname_args /* {
		int	fdes;
		caddr_t	asa;
		int	*alen;
	} */ *uap;
	int *retval;
	int compat;
{
	struct file *fp;
	register struct socket *so;
	struct sockaddr *sa;
	int len, error;

	error = getsock(p->p_fd, uap->fdes, &fp);
	if (error)
		return (error);
	error = copyin((caddr_t)uap->alen, (caddr_t)&len, sizeof (len));
	if (error)
		return (error);
	so = (struct socket *)fp->f_data;
	sa = 0;
	error = (*so->so_proto->pr_usrreqs->pru_sockaddr)(so, &sa);
	if (error)
		goto bad;
	if (sa == 0) {
		len = 0;
		goto gotnothing;
	}

	len = MIN(len, sa->sa_len);
#ifdef COMPAT_OLDSOCK
	if (compat)
		((struct osockaddr *)sa)->sa_family = sa->sa_family;
#endif
	error = copyout(sa, (caddr_t)uap->asa, (u_int)len);
	if (error == 0)
gotnothing:
		error = copyout((caddr_t)&len, (caddr_t)uap->alen,
		    sizeof (len));
bad:
	if (sa)
		FREE(sa, M_SONAME);
	return (error);
}

int
getsockname(p, uap, retval)
	struct proc *p;
	struct getsockname_args *uap;
	int *retval;
{

	return (getsockname1(p, uap, retval, 0));
}

#ifdef COMPAT_OLDSOCK
int
ogetsockname(p, uap, retval)
	struct proc *p;
	struct getsockname_args *uap;
	int *retval;
{

	return (getsockname1(p, uap, retval, 1));
}
#endif /* COMPAT_OLDSOCK */

/*
 * Get name of peer for connected socket.
 */
/* ARGSUSED */
static int
getpeername1(p, uap, retval, compat)
	struct proc *p;
	register struct getpeername_args /* {
		int	fdes;
		caddr_t	asa;
		int	*alen;
	} */ *uap;
	int *retval;
	int compat;
{
	struct file *fp;
	register struct socket *so;
	struct sockaddr *sa;
	int len, error;

	error = getsock(p->p_fd, uap->fdes, &fp);
	if (error)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
		return (ENOTCONN);
	error = copyin((caddr_t)uap->alen, (caddr_t)&len, sizeof (len));
	if (error)
		return (error);
	sa = 0;
	error = (*so->so_proto->pr_usrreqs->pru_peeraddr)(so, &sa);
	if (error)
		goto bad;
	if (sa == 0) {
		len = 0;
		goto gotnothing;
	}
	len = MIN(len, sa->sa_len);
#ifdef COMPAT_OLDSOCK
	if (compat)
		((struct osockaddr *)sa)->sa_family =
		    sa->sa_family;
#endif
	error = copyout(sa, (caddr_t)uap->asa, (u_int)len);
	if (error)
		goto bad;
gotnothing:
	error = copyout((caddr_t)&len, (caddr_t)uap->alen, sizeof (len));
bad:
	if (sa) FREE(sa, M_SONAME);
	return (error);
}

int
getpeername(p, uap, retval)
	struct proc *p;
	struct getpeername_args *uap;
	int *retval;
{

	return (getpeername1(p, uap, retval, 0));
}

#ifdef COMPAT_OLDSOCK
int
ogetpeername(p, uap, retval)
	struct proc *p;
	struct ogetpeername_args *uap;
	int *retval;
{

	/* XXX uap should have type `getpeername_args *' to begin with. */
	return (getpeername1(p, (struct getpeername_args *)uap, retval, 1));
}
#endif /* COMPAT_OLDSOCK */

int
sockargs(mp, buf, buflen, type)
	struct mbuf **mp;
	caddr_t buf;
	int buflen, type;
{
	register struct sockaddr *sa;
	register struct mbuf *m;
	int error;

	if ((u_int)buflen > MLEN) {
#ifdef COMPAT_OLDSOCK
		if (type == MT_SONAME && (u_int)buflen <= 112)
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
		return (EINVAL);
	}
	m = m_get(M_WAIT, type);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = buflen;
	error = copyin(buf, mtod(m, caddr_t), (u_int)buflen);
	if (error)
		(void) m_free(m);
	else {
		*mp = m;
		if (type == MT_SONAME) {
			sa = mtod(m, struct sockaddr *);

#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
			if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
				sa->sa_family = sa->sa_len;
#endif
			sa->sa_len = buflen;
		}
	}
	return (error);
}

int
getsockaddr(namp, uaddr, len)
	struct sockaddr **namp;
	caddr_t uaddr;
	size_t len;
{
	struct sockaddr *sa;
	int error;

	if (len > SOCK_MAXADDRLEN)
		return ENAMETOOLONG;
	MALLOC(sa, struct sockaddr *, len, M_SONAME, M_WAITOK);
	error = copyin(uaddr, sa, len);
	if (error) {
		FREE(sa, M_SONAME);
	} else {
#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = len;
		*namp = sa;
	}
	return error;
}

int
getsock(fdp, fdes, fpp)
	struct filedesc *fdp;
	int fdes;
	struct file **fpp;
{
	register struct file *fp;

	if ((unsigned)fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fdes]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);
	*fpp = fp;
	return (0);
}
