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
 *	@(#)uipc_syscalls.c	8.6 (Berkeley) 2/14/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

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
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
	} */ *uap;
	register_t *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct socket *so;
	struct file *fp;
	int fd, error;

	if (error = falloc(p, &fp, &fd))
		return (error);
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	if (error = socreate(SCARG(uap, domain), &so, SCARG(uap, type),
	    SCARG(uap, protocol))) {
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
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int) namelen;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	struct mbuf *nam;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	if (error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME))
		return (error);
	error = sobind((struct socket *)fp->f_data, nam);
	m_freem(nam);
	return (error);
}

/* ARGSUSED */
int
listen(p, uap, retval)
	struct proc *p;
	register struct listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	return (solisten((struct socket *)fp->f_data, SCARG(uap, backlog)));
}

#ifdef COMPAT_OLDSOCK
int
accept(p, uap, retval)
	struct proc *p;
	struct accept_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int *) anamelen;
	} */ *uap;
	register_t *retval;
{

	return (accept1(p, uap, retval, 0));
}

int
compat_43_accept(p, uap, retval)
	struct proc *p;
	struct accept_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int *) anamelen;
	} */ *uap;
	register_t *retval;
{

	return (accept1(p, uap, retval, 1));
}
#else /* COMPAT_OLDSOCK */

#define	accept1	accept
#endif

int
accept1(p, uap, retval, compat_43)
	struct proc *p;
	register struct accept_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int *) anamelen;
	} */ *uap;
	register_t *retval;
	int compat_43;
{
	struct file *fp;
	struct mbuf *nam;
	int namelen, error, s, tmpfd;
	register struct socket *so;

	if (SCARG(uap, name) && (error = copyin((caddr_t)SCARG(uap, anamelen),
	    (caddr_t)&namelen, sizeof (namelen))))
		return (error);
	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	s = splnet();
	so = (struct socket *)fp->f_data;
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		splx(s);
		return (EINVAL);
	}
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		splx(s);
		return (EWOULDBLOCK);
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		    netcon, 0)) {
			splx(s);
			return (error);
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		return (error);
	}
	if (error = falloc(p, &fp, &tmpfd)) {
		splx(s);
		return (error);
	}
	*retval = tmpfd;
	{ struct socket *aso = so->so_q;
	  if (soqremque(aso, 1) == 0)
		panic("accept");
	  so = aso;
	}
	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = FREAD|FWRITE;
	fp->f_ops = &socketops;
	fp->f_data = (caddr_t)so;
	nam = m_get(M_WAIT, MT_SONAME);
	(void) soaccept(so, nam);
	if (SCARG(uap, name)) {
#ifdef COMPAT_OLDSOCK
		if (compat_43)
			mtod(nam, struct osockaddr *)->sa_family =
			    mtod(nam, struct sockaddr *)->sa_family;
#endif
		if (namelen > nam->m_len)
			namelen = nam->m_len;
		/* SHOULD COPY OUT A CHAIN HERE */
		if ((error = copyout(mtod(nam, caddr_t),
		    (caddr_t)SCARG(uap, name), (u_int)namelen)) == 0)
			error = copyout((caddr_t)&namelen,
			    (caddr_t)SCARG(uap, anamelen),
			    sizeof (*SCARG(uap, anamelen)));
	}
	m_freem(nam);
	splx(s);
	return (error);
}

/* ARGSUSED */
int
connect(p, uap, retval)
	struct proc *p;
	register struct connect_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) name;
		syscallarg(int) namelen;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	register struct socket *so;
	struct mbuf *nam;
	int error, s;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
		return (EALREADY);
	if (error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME))
		return (error);
	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		m_freem(nam);
		return (EINPROGRESS);
	}
	s = splnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
		if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		    netcon, 0))
			break;
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	splx(s);
bad:
	so->so_state &= ~SS_ISCONNECTING;
	m_freem(nam);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

int
socketpair(p, uap, retval)
	struct proc *p;
	register struct socketpair_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
		syscallarg(int *) rsv;
	} */ *uap;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, sv[2];

	if (error = socreate(SCARG(uap, domain), &so1, SCARG(uap, type),
	    SCARG(uap, protocol)))
		return (error);
	if (error = socreate(SCARG(uap, domain), &so2, SCARG(uap, type),
	    SCARG(uap, protocol)))
		goto free1;
	if (error = falloc(p, &fp1, &fd))
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = (caddr_t)so1;
	if (error = falloc(p, &fp2, &fd))
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = (caddr_t)so2;
	sv[1] = fd;
	if (error = soconnect2(so1, so2))
		goto free4;
	if (SCARG(uap, type) == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 if (error = soconnect2(so2, so1))
			goto free4;
	}
	error = copyout((caddr_t)sv, (caddr_t)SCARG(uap, rsv),
	    2 * sizeof (int));
	retval[0] = sv[0];		/* XXX ??? */
	retval[1] = sv[1];		/* XXX ??? */
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
sendto(p, uap, retval)
	struct proc *p;
	register struct sendto_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(caddr_t) to;
		syscallarg(int) tolen;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = SCARG(uap, to);
	msg.msg_namelen = SCARG(uap, tolen);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	return (sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
}

#ifdef COMPAT_OLDSOCK
int
compat_43_send(p, uap, retval)
	struct proc *p;
	register struct compat_43_send_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
}

#define MSG_COMPAT	0x8000
int
compat_43_sendmsg(p, uap, retval)
	struct proc *p;
	register struct compat_43_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) msg;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin(SCARG(uap, msg), (caddr_t)&msg,
	    sizeof (struct omsghdr)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV, 
		      M_WAITOK);
	} else
		iov = aiov;
	if (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	msg.msg_flags = MSG_COMPAT;
	msg.msg_iov = iov;
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
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
		syscallarg(int) s;
		syscallarg(caddr_t) msg;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin(SCARG(uap, msg), (caddr_t)&msg, sizeof (msg)))
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
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

int
sendit(p, s, mp, flags, retsize)
	register struct proc *p;
	int s;
	register struct msghdr *mp;
	int flags;
	register_t *retsize;
{
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *to, *control;
	int len, error;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	if (error = getsock(p->p_fd, s, &fp))
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
		if (auio.uio_resid + iov->iov_len < auio.uio_resid)
			return (EINVAL);
		auio.uio_resid += iov->iov_len;
	}
	if (mp->msg_name) {
		if (error = sockargs(&to, mp->msg_name, mp->msg_namelen,
		    MT_SONAME))
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
		if (error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL))
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
	if (error = sosend((struct socket *)fp->f_data, to, &auio,
	    (struct mbuf *)0, control, flags)) {
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
		m_freem(to);
	return (error);
}

#ifdef COMPAT_OLDSOCK
int
compat_43_recvfrom(p, uap, retval)
	struct proc *p;
	struct recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(caddr_t) from;
		syscallarg(int *) fromlenaddr;
	} */ *uap;
	register_t *retval;
{

	SCARG(uap, flags) |= MSG_COMPAT;
	return (recvfrom(p, uap, retval));
}
#endif

int
recvfrom(p, uap, retval)
	struct proc *p;
	register struct recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(caddr_t) from;
		syscallarg(int *) fromlenaddr;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (SCARG(uap, fromlenaddr)) {
		if (error = copyin((caddr_t)SCARG(uap, fromlenaddr),
		    (caddr_t)&msg.msg_namelen, sizeof (msg.msg_namelen)))
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = SCARG(uap, from);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
	return (recvit(p, SCARG(uap, s), &msg,
	    (caddr_t)SCARG(uap, fromlenaddr), retval));
}

#ifdef COMPAT_OLDSOCK
int
compat_43_recv(p, uap, retval)
	struct proc *p;
	register struct compat_43_recv_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(int) len;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
	return (recvit(p, SCARG(uap, s), &msg, (caddr_t)0, retval));
}

/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
int
compat_43_recvmsg(p, uap, retval)
	struct proc *p;
	register struct compat_43_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(struct omsghdr *) msg;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin((caddr_t)SCARG(uap, msg), (caddr_t)&msg,
	    sizeof (struct omsghdr)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		      M_WAITOK);
	} else
		iov = aiov;
	msg.msg_flags = SCARG(uap, flags) | MSG_COMPAT;
	if (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	msg.msg_iov = iov;
	error = recvit(p, SCARG(uap, s), &msg,
	    (caddr_t)&SCARG(uap, msg)->msg_namelen, retval);

	if (msg.msg_controllen && error == 0)
		error = copyout((caddr_t)&msg.msg_controllen,
		    (caddr_t)&SCARG(uap, msg)->msg_accrightslen, sizeof (int));
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
		syscallarg(int) s;
		syscallarg(struct msghdr *) msg;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	register int error;

	if (error = copyin((caddr_t)SCARG(uap, msg), (caddr_t)&msg,
	    sizeof (msg)))
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
	msg.msg_flags = SCARG(uap, flags) &~ MSG_COMPAT;
#else
	msg.msg_flags = SCARG(uap, flags);
#endif
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	if (error = copyin((caddr_t)uiov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	if ((error = recvit(p, SCARG(uap, s), &msg, (caddr_t)0, retval)) == 0) {
		msg.msg_iov = uiov;
		error = copyout((caddr_t)&msg, (caddr_t)SCARG(uap, msg),
		    sizeof(msg));
	}
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
	register_t *retsize;
{
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	int len, error;
	struct mbuf *from = 0, *control = 0;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	if (error = getsock(p->p_fd, s, &fp))
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
		if (auio.uio_resid + iov->iov_len < auio.uio_resid)
			return (EINVAL);
		auio.uio_resid += iov->iov_len;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	if (error = soreceive((struct socket *)fp->f_data, &from, &auio,
	    (struct mbuf **)0, mp->msg_control ? &control : (struct mbuf **)0,
	    &mp->msg_flags)) {
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
		if (len <= 0 || from == 0)
			len = 0;
		else {
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				mtod(from, struct osockaddr *)->sa_family =
				    mtod(from, struct sockaddr *)->sa_family;
#endif
			if (len > from->m_len)
				len = from->m_len;
			/* else if len < from->m_len ??? */
			if (error = copyout(mtod(from, caddr_t),
			    (caddr_t)mp->msg_name, (unsigned)len))
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
		if (len <= 0 || control == 0)
			len = 0;
		else {
			if (len >= control->m_len)
				len = control->m_len;
			else
				mp->msg_flags |= MSG_CTRUNC;
			error = copyout((caddr_t)mtod(control, caddr_t),
			    (caddr_t)mp->msg_control, (unsigned)len);
		}
		mp->msg_controllen = len;
	}
out:
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);
	return (error);
}

/* ARGSUSED */
int
shutdown(p, uap, retval)
	struct proc *p;
	register struct shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	return (soshutdown((struct socket *)fp->f_data, SCARG(uap, how)));
}

/* ARGSUSED */
int
setsockopt(p, uap, retval)
	struct proc *p;
	register struct setsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(caddr_t) val;
		syscallarg(int) valsize;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	if (SCARG(uap, valsize) > MLEN)
		return (EINVAL);
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		if (error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    (u_int)SCARG(uap, valsize))) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
}

/* ARGSUSED */
int
getsockopt(p, uap, retval)
	struct proc *p;
	register struct getsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(caddr_t) val;
		syscallarg(int *) avalsize;
	} */ *uap;
	register_t *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int valsize, error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
	if (SCARG(uap, val)) {
		if (error = copyin((caddr_t)SCARG(uap, avalsize),
		    (caddr_t)&valsize, sizeof (valsize)))
			return (error);
	} else
		valsize = 0;
	if ((error = sogetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), &m)) == 0 && SCARG(uap, val) && valsize &&
	    m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), SCARG(uap, val),
		    (u_int)valsize);
		if (error == 0)
			error = copyout((caddr_t)&valsize,
			    (caddr_t)SCARG(uap, avalsize), sizeof (valsize));
	}
	if (m != NULL)
		(void) m_free(m);
	return (error);
}

/* ARGSUSED */
int
pipe(p, uap, retval)
	struct proc *p;
	void *uap;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct socket *rso, *wso;
	int fd, error;

	if (error = socreate(AF_UNIX, &rso, SOCK_STREAM, 0))
		return (error);
	if (error = socreate(AF_UNIX, &wso, SOCK_STREAM, 0))
		goto free1;
	if (error = falloc(p, &rf, &fd))
		goto free2;
	retval[0] = fd;
	rf->f_flag = FREAD;
	rf->f_type = DTYPE_SOCKET;
	rf->f_ops = &socketops;
	rf->f_data = (caddr_t)rso;
	if (error = falloc(p, &wf, &fd))
		goto free3;
	wf->f_flag = FWRITE;
	wf->f_type = DTYPE_SOCKET;
	wf->f_ops = &socketops;
	wf->f_data = (caddr_t)wso;
	retval[1] = fd;
	if (error = unp_connect2(wso, rso))
		goto free4;
	return (0);
free4:
	ffree(wf);
	fdp->fd_ofiles[retval[1]] = 0;
free3:
	ffree(rf);
	fdp->fd_ofiles[retval[0]] = 0;
free2:
	(void)soclose(wso);
free1:
	(void)soclose(rso);
	return (error);
}

/*
 * Get socket name.
 */
#ifdef COMPAT_OLDSOCK
int
getsockname(p, uap, retval)
	struct proc *p;
	struct getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
{

	return (getsockname1(p, uap, retval, 0));
}

int
compat_43_getsockname(p, uap, retval)
	struct proc *p;
	struct getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
{

	return (getsockname1(p, uap, retval, 1));
}
#else /* COMPAT_OLDSOCK */

#define	getsockname1	getsockname
#endif

/* ARGSUSED */
int
getsockname1(p, uap, retval, compat_43)
	struct proc *p;
	register struct getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
	int compat_43;
{
	struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	int len, error;

	if (error = getsock(p->p_fd, SCARG(uap, fdes), &fp))
		return (error);
	if (error = copyin((caddr_t)SCARG(uap, alen), (caddr_t)&len,
	    sizeof (len)))
		return (error);
	so = (struct socket *)fp->f_data;
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	if (error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0))
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
#ifdef COMPAT_OLDSOCK
	if (compat_43)
		mtod(m, struct osockaddr *)->sa_family =
		    mtod(m, struct sockaddr *)->sa_family;
#endif
	error = copyout(mtod(m, caddr_t), (caddr_t)SCARG(uap, asa), (u_int)len);
	if (error == 0)
		error = copyout((caddr_t)&len, (caddr_t)SCARG(uap, alen),
		    sizeof (len));
bad:
	m_freem(m);
	return (error);
}

/*
 * Get name of peer for connected socket.
 */
#ifdef COMPAT_OLDSOCK
int
getpeername(p, uap, retval)
	struct proc *p;
	struct getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
{

	return (getpeername1(p, uap, retval, 0));
}

int
compat_43_getpeername(p, uap, retval)
	struct proc *p;
	struct getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
{

	return (getpeername1(p, uap, retval, 1));
}
#else /* COMPAT_OLDSOCK */

#define	getpeername1	getpeername
#endif

/* ARGSUSED */
int
getpeername1(p, uap, retval, compat_43)
	struct proc *p;
	register struct getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(int *) alen;
	} */ *uap;
	register_t *retval;
	int compat_43;
{
	struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	int len, error;

	if (error = getsock(p->p_fd, SCARG(uap, fdes), &fp))
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
		return (ENOTCONN);
	if (error =
	    copyin((caddr_t)SCARG(uap, alen), (caddr_t)&len, sizeof (len)))
		return (error);
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	if (error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0))
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
#ifdef COMPAT_OLDSOCK
	if (compat_43)
		mtod(m, struct osockaddr *)->sa_family =
		    mtod(m, struct sockaddr *)->sa_family;
#endif
	if (error =
	    copyout(mtod(m, caddr_t), (caddr_t)SCARG(uap, asa), (u_int)len))
		goto bad;
	error = copyout((caddr_t)&len, (caddr_t)SCARG(uap, alen), sizeof (len));
bad:
	m_freem(m);
	return (error);
}

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
	if (error) {
		(void) m_free(m);
		return (error);
	}
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);

#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = buflen;
	}
	return (0);
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
