/*-
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * sendfile(2) and related extensions:
 * Copyright (c) 1998, David Greenman. All rights reserved.
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
 *	@(#)uipc_syscalls.c	8.4 (Berkeley) 2/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_sctp.h"
#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sf_buf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <net/vnet.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#if defined(INET) || defined(INET6)
#ifdef SCTP
#include <netinet/sctp.h>
#include <netinet/sctp_peeloff.h>
#endif /* SCTP */
#endif /* INET || INET6 */

static int sendit(struct thread *td, int s, struct msghdr *mp, int flags);
static int recvit(struct thread *td, int s, struct msghdr *mp, void *namelenp);

static int accept1(struct thread *td, struct accept_args *uap, int compat);
static int do_sendfile(struct thread *td, struct sendfile_args *uap, int compat);
static int getsockname1(struct thread *td, struct getsockname_args *uap,
			int compat);
static int getpeername1(struct thread *td, struct getpeername_args *uap,
			int compat);

/*
 * NSFBUFS-related variables and associated sysctls
 */
int nsfbufs;
int nsfbufspeak;
int nsfbufsused;

SYSCTL_INT(_kern_ipc, OID_AUTO, nsfbufs, CTLFLAG_RDTUN, &nsfbufs, 0,
    "Maximum number of sendfile(2) sf_bufs available");
SYSCTL_INT(_kern_ipc, OID_AUTO, nsfbufspeak, CTLFLAG_RD, &nsfbufspeak, 0,
    "Number of sendfile(2) sf_bufs at peak usage");
SYSCTL_INT(_kern_ipc, OID_AUTO, nsfbufsused, CTLFLAG_RD, &nsfbufsused, 0,
    "Number of sendfile(2) sf_bufs in use");

/*
 * Convert a user file descriptor to a kernel file entry.  A reference on the
 * file entry is held upon returning.  This is lighter weight than
 * fgetsock(), which bumps the socket reference drops the file reference
 * count instead, as this approach avoids several additional mutex operations
 * associated with the additional reference count.  If requested, return the
 * open file flags.
 */
static int
getsock(struct filedesc *fdp, int fd, struct file **fpp, u_int *fflagp)
{
	struct file *fp;
	int error;

	fp = NULL;
	if (fdp == NULL || (fp = fget_unlocked(fdp, fd)) == NULL) {
		error = EBADF;
	} else if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		fp = NULL;
		error = ENOTSOCK;
	} else {
		if (fflagp != NULL)
			*fflagp = fp->f_flag;
		error = 0;
	}
	*fpp = fp;
	return (error);
}

/*
 * System call interface to the socket abstraction.
 */
#if defined(COMPAT_43)
#define COMPAT_OLDSOCK
#endif

int
socket(td, uap)
	struct thread *td;
	struct socket_args /* {
		int	domain;
		int	type;
		int	protocol;
	} */ *uap;
{
	struct filedesc *fdp;
	struct socket *so;
	struct file *fp;
	int fd, error;

	AUDIT_ARG_SOCKET(uap->domain, uap->type, uap->protocol);
#ifdef MAC
	error = mac_socket_check_create(td->td_ucred, uap->domain, uap->type,
	    uap->protocol);
	if (error)
		return (error);
#endif
	fdp = td->td_proc->p_fd;
	error = falloc(td, &fp, &fd);
	if (error)
		return (error);
	/* An extra reference on `fp' has been held for us by falloc(). */
	error = socreate(uap->domain, &so, uap->type, uap->protocol,
	    td->td_ucred, td);
	if (error) {
		fdclose(fdp, fp, fd, td);
	} else {
		finit(fp, FREAD | FWRITE, DTYPE_SOCKET, so, &socketops);
		td->td_retval[0] = fd;
	}
	fdrop(fp, td);
	return (error);
}

/* ARGSUSED */
int
bind(td, uap)
	struct thread *td;
	struct bind_args /* {
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
{
	struct sockaddr *sa;
	int error;

	if ((error = getsockaddr(&sa, uap->name, uap->namelen)) != 0)
		return (error);

	error = kern_bind(td, uap->s, sa);
	free(sa, M_SONAME);
	return (error);
}

int
kern_bind(td, fd, sa)
	struct thread *td;
	int fd;
	struct sockaddr *sa;
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);
	error = getsock(td->td_proc->p_fd, fd, &fp, NULL);
	if (error)
		return (error);
	so = fp->f_data;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(sa);
#endif
#ifdef MAC
	error = mac_socket_check_bind(td->td_ucred, so, sa);
	if (error == 0)
#endif
		error = sobind(so, sa, td);
	fdrop(fp, td);
	return (error);
}

/* ARGSUSED */
int
listen(td, uap)
	struct thread *td;
	struct listen_args /* {
		int	s;
		int	backlog;
	} */ *uap;
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(uap->s);
	error = getsock(td->td_proc->p_fd, uap->s, &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
#ifdef MAC
		error = mac_socket_check_listen(td->td_ucred, so);
		if (error == 0) {
#endif
			CURVNET_SET(so->so_vnet);
			error = solisten(so, uap->backlog, td);
			CURVNET_RESTORE();
#ifdef MAC
		}
#endif
		fdrop(fp, td);
	}
	return(error);
}

/*
 * accept1()
 */
static int
accept1(td, uap, compat)
	struct thread *td;
	struct accept_args /* {
		int	s;
		struct sockaddr	* __restrict name;
		socklen_t	* __restrict anamelen;
	} */ *uap;
	int compat;
{
	struct sockaddr *name;
	socklen_t namelen;
	struct file *fp;
	int error;

	if (uap->name == NULL)
		return (kern_accept(td, uap->s, NULL, NULL, NULL));

	error = copyin(uap->anamelen, &namelen, sizeof (namelen));
	if (error)
		return (error);

	error = kern_accept(td, uap->s, &name, &namelen, &fp);

	/*
	 * return a namelen of zero for older code which might
	 * ignore the return value from accept.
	 */
	if (error) {
		(void) copyout(&namelen,
		    uap->anamelen, sizeof(*uap->anamelen));
		return (error);
	}

	if (error == 0 && name != NULL) {
#ifdef COMPAT_OLDSOCK
		if (compat)
			((struct osockaddr *)name)->sa_family =
			    name->sa_family;
#endif
		error = copyout(name, uap->name, namelen);
	}
	if (error == 0)
		error = copyout(&namelen, uap->anamelen,
		    sizeof(namelen));
	if (error)
		fdclose(td->td_proc->p_fd, fp, td->td_retval[0], td);
	fdrop(fp, td);
	free(name, M_SONAME);
	return (error);
}

int
kern_accept(struct thread *td, int s, struct sockaddr **name,
    socklen_t *namelen, struct file **fp)
{
	struct filedesc *fdp;
	struct file *headfp, *nfp = NULL;
	struct sockaddr *sa = NULL;
	int error;
	struct socket *head, *so;
	int fd;
	u_int fflag;
	pid_t pgid;
	int tmp;

	if (name) {
		*name = NULL;
		if (*namelen < 0)
			return (EINVAL);
	}

	AUDIT_ARG_FD(s);
	fdp = td->td_proc->p_fd;
	error = getsock(fdp, s, &headfp, &fflag);
	if (error)
		return (error);
	head = headfp->f_data;
	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto done;
	}
#ifdef MAC
	error = mac_socket_check_accept(td->td_ucred, head);
	if (error != 0)
		goto done;
#endif
	error = falloc(td, &nfp, &fd);
	if (error)
		goto done;
	ACCEPT_LOCK();
	if ((head->so_state & SS_NBIO) && TAILQ_EMPTY(&head->so_comp)) {
		ACCEPT_UNLOCK();
		error = EWOULDBLOCK;
		goto noconnection;
	}
	while (TAILQ_EMPTY(&head->so_comp) && head->so_error == 0) {
		if (head->so_rcv.sb_state & SBS_CANTRCVMORE) {
			head->so_error = ECONNABORTED;
			break;
		}
		error = msleep(&head->so_timeo, &accept_mtx, PSOCK | PCATCH,
		    "accept", 0);
		if (error) {
			ACCEPT_UNLOCK();
			goto noconnection;
		}
	}
	if (head->so_error) {
		error = head->so_error;
		head->so_error = 0;
		ACCEPT_UNLOCK();
		goto noconnection;
	}
	so = TAILQ_FIRST(&head->so_comp);
	KASSERT(!(so->so_qstate & SQ_INCOMP), ("accept1: so SQ_INCOMP"));
	KASSERT(so->so_qstate & SQ_COMP, ("accept1: so not SQ_COMP"));

	/*
	 * Before changing the flags on the socket, we have to bump the
	 * reference count.  Otherwise, if the protocol calls sofree(),
	 * the socket will be released due to a zero refcount.
	 */
	SOCK_LOCK(so);			/* soref() and so_state update */
	soref(so);			/* file descriptor reference */

	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;
	so->so_state |= (head->so_state & SS_NBIO);
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;

	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();

	/* An extra reference on `nfp' has been held for us by falloc(). */
	td->td_retval[0] = fd;

	/* connection has been removed from the listen queue */
	KNOTE_UNLOCKED(&head->so_rcv.sb_sel.si_note, 0);

	pgid = fgetown(&head->so_sigio);
	if (pgid != 0)
		fsetown(pgid, &so->so_sigio);

	finit(nfp, fflag, DTYPE_SOCKET, so, &socketops);
	/* Sync socket nonblocking/async state with file flags */
	tmp = fflag & FNONBLOCK;
	(void) fo_ioctl(nfp, FIONBIO, &tmp, td->td_ucred, td);
	tmp = fflag & FASYNC;
	(void) fo_ioctl(nfp, FIOASYNC, &tmp, td->td_ucred, td);
	sa = 0;
	CURVNET_SET(so->so_vnet);
	error = soaccept(so, &sa);
	CURVNET_RESTORE();
	if (error) {
		/*
		 * return a namelen of zero for older code which might
		 * ignore the return value from accept.
		 */
		if (name)
			*namelen = 0;
		goto noconnection;
	}
	if (sa == NULL) {
		if (name)
			*namelen = 0;
		goto done;
	}
	if (name) {
		/* check sa_len before it is destroyed */
		if (*namelen > sa->sa_len)
			*namelen = sa->sa_len;
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			ktrsockaddr(sa);
#endif
		*name = sa;
		sa = NULL;
	}
noconnection:
	if (sa)
		free(sa, M_SONAME);

	/*
	 * close the new descriptor, assuming someone hasn't ripped it
	 * out from under us.
	 */
	if (error)
		fdclose(fdp, nfp, fd, td);

	/*
	 * Release explicitly held references before returning.  We return
	 * a reference on nfp to the caller on success if they request it.
	 */
done:
	if (fp != NULL) {
		if (error == 0) {
			*fp = nfp;
			nfp = NULL;
		} else
			*fp = NULL;
	}
	if (nfp != NULL)
		fdrop(nfp, td);
	fdrop(headfp, td);
	return (error);
}

int
accept(td, uap)
	struct thread *td;
	struct accept_args *uap;
{

	return (accept1(td, uap, 0));
}

#ifdef COMPAT_OLDSOCK
int
oaccept(td, uap)
	struct thread *td;
	struct accept_args *uap;
{

	return (accept1(td, uap, 1));
}
#endif /* COMPAT_OLDSOCK */

/* ARGSUSED */
int
connect(td, uap)
	struct thread *td;
	struct connect_args /* {
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error)
		return (error);

	error = kern_connect(td, uap->s, sa);
	free(sa, M_SONAME);
	return (error);
}


int
kern_connect(td, fd, sa)
	struct thread *td;
	int fd;
	struct sockaddr *sa;
{
	struct socket *so;
	struct file *fp;
	int error;
	int interrupted = 0;

	AUDIT_ARG_FD(fd);
	error = getsock(td->td_proc->p_fd, fd, &fp, NULL);
	if (error)
		return (error);
	so = fp->f_data;
	if (so->so_state & SS_ISCONNECTING) {
		error = EALREADY;
		goto done1;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(sa);
#endif
#ifdef MAC
	error = mac_socket_check_connect(td->td_ucred, so, sa);
	if (error)
		goto bad;
#endif
	error = soconnect(so, sa, td);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		error = EINPROGRESS;
		goto done1;
	}
	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = msleep(&so->so_timeo, SOCK_MTX(so), PSOCK | PCATCH,
		    "connec", 0);
		if (error) {
			if (error == EINTR || error == ERESTART)
				interrupted = 1;
			break;
		}
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	SOCK_UNLOCK(so);
bad:
	if (!interrupted)
		so->so_state &= ~SS_ISCONNECTING;
	if (error == ERESTART)
		error = EINTR;
done1:
	fdrop(fp, td);
	return (error);
}

int
kern_socketpair(struct thread *td, int domain, int type, int protocol,
    int *rsv)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error;

	AUDIT_ARG_SOCKET(domain, type, protocol);
#ifdef MAC
	/* We might want to have a separate check for socket pairs. */
	error = mac_socket_check_create(td->td_ucred, domain, type,
	    protocol);
	if (error)
		return (error);
#endif
	error = socreate(domain, &so1, type, protocol, td->td_ucred, td);
	if (error)
		return (error);
	error = socreate(domain, &so2, type, protocol, td->td_ucred, td);
	if (error)
		goto free1;
	/* On success extra reference to `fp1' and 'fp2' is set by falloc. */
	error = falloc(td, &fp1, &fd);
	if (error)
		goto free2;
	rsv[0] = fd;
	fp1->f_data = so1;	/* so1 already has ref count */
	error = falloc(td, &fp2, &fd);
	if (error)
		goto free3;
	fp2->f_data = so2;	/* so2 already has ref count */
	rsv[1] = fd;
	error = soconnect2(so1, so2);
	if (error)
		goto free4;
	if (type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 error = soconnect2(so2, so1);
		 if (error)
			goto free4;
	}
	finit(fp1, FREAD | FWRITE, DTYPE_SOCKET, fp1->f_data, &socketops);
	finit(fp2, FREAD | FWRITE, DTYPE_SOCKET, fp2->f_data, &socketops);
	fdrop(fp1, td);
	fdrop(fp2, td);
	return (0);
free4:
	fdclose(fdp, fp2, rsv[1], td);
	fdrop(fp2, td);
free3:
	fdclose(fdp, fp1, rsv[0], td);
	fdrop(fp1, td);
free2:
	if (so2 != NULL)
		(void)soclose(so2);
free1:
	if (so1 != NULL)
		(void)soclose(so1);
	return (error);
}

int
socketpair(struct thread *td, struct socketpair_args *uap)
{
	int error, sv[2];

	error = kern_socketpair(td, uap->domain, uap->type,
	    uap->protocol, sv);
	if (error)
		return (error);
	error = copyout(sv, uap->rsv, 2 * sizeof(int));
	if (error) {
		(void)kern_close(td, sv[0]);
		(void)kern_close(td, sv[1]);
	}
	return (error);
}

static int
sendit(td, s, mp, flags)
	struct thread *td;
	int s;
	struct msghdr *mp;
	int flags;
{
	struct mbuf *control;
	struct sockaddr *to;
	int error;

	if (mp->msg_name != NULL) {
		error = getsockaddr(&to, mp->msg_name, mp->msg_namelen);
		if (error) {
			to = NULL;
			goto bad;
		}
		mp->msg_name = to;
	} else {
		to = NULL;
	}

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
			struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAIT);
			cm = mtod(control, struct cmsghdr *);
			cm->cmsg_len = control->m_len;
			cm->cmsg_level = SOL_SOCKET;
			cm->cmsg_type = SCM_RIGHTS;
		}
#endif
	} else {
		control = NULL;
	}

	error = kern_sendit(td, s, mp, flags, control, UIO_USERSPACE);

bad:
	if (to)
		free(to, M_SONAME);
	return (error);
}

int
kern_sendit(td, s, mp, flags, control, segflg)
	struct thread *td;
	int s;
	struct msghdr *mp;
	int flags;
	struct mbuf *control;
	enum uio_seg segflg;
{
	struct file *fp;
	struct uio auio;
	struct iovec *iov;
	struct socket *so;
	int i;
	int len, error;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif

	AUDIT_ARG_FD(s);
	error = getsock(td->td_proc->p_fd, s, &fp, NULL);
	if (error)
		return (error);
	so = (struct socket *)fp->f_data;

#ifdef MAC
	if (mp->msg_name != NULL) {
		error = mac_socket_check_connect(td->td_ucred, so,
		    mp->msg_name);
		if (error)
			goto bad;
	}
	error = mac_socket_check_send(td->td_ucred, so);
	if (error)
		goto bad;
#endif

	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = segflg;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if ((auio.uio_resid += iov->iov_len) < 0) {
			error = EINVAL;
			goto bad;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif
	len = auio.uio_resid;
	error = sosend(so, mp->msg_name, &auio, 0, control, flags, td);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			psignal(td->td_proc, SIGPIPE);
			PROC_UNLOCK(td->td_proc);
		}
	}
	if (error == 0)
		td->td_retval[0] = len - auio.uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = td->td_retval[0];
		ktrgenio(s, UIO_WRITE, ktruio, error);
	}
#endif
bad:
	fdrop(fp, td);
	return (error);
}

int
sendto(td, uap)
	struct thread *td;
	struct sendto_args /* {
		int	s;
		caddr_t	buf;
		size_t	len;
		int	flags;
		caddr_t	to;
		int	tolen;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

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
	error = sendit(td, uap->s, &msg, uap->flags);
	return (error);
}

#ifdef COMPAT_OLDSOCK
int
osend(td, uap)
	struct thread *td;
	struct osend_args /* {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	error = sendit(td, uap->s, &msg, uap->flags);
	return (error);
}

int
osendmsg(td, uap)
	struct thread *td;
	struct osendmsg_args /* {
		int	s;
		caddr_t	msg;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (struct omsghdr));
	if (error)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error)
		return (error);
	msg.msg_iov = iov;
	msg.msg_flags = MSG_COMPAT;
	error = sendit(td, uap->s, &msg, uap->flags);
	free(iov, M_IOV);
	return (error);
}
#endif

int
sendmsg(td, uap)
	struct thread *td;
	struct sendmsg_args /* {
		int	s;
		caddr_t	msg;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (msg));
	if (error)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error)
		return (error);
	msg.msg_iov = iov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	error = sendit(td, uap->s, &msg, uap->flags);
	free(iov, M_IOV);
	return (error);
}

int
kern_recvit(td, s, mp, fromseg, controlp)
	struct thread *td;
	int s;
	struct msghdr *mp;
	enum uio_seg fromseg;
	struct mbuf **controlp;
{
	struct uio auio;
	struct iovec *iov;
	int i;
	socklen_t len;
	int error;
	struct mbuf *m, *control = 0;
	caddr_t ctlbuf;
	struct file *fp;
	struct socket *so;
	struct sockaddr *fromsa = 0;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif

	if(controlp != NULL)
		*controlp = 0;

	AUDIT_ARG_FD(s);
	error = getsock(td->td_proc->p_fd, s, &fp, NULL);
	if (error)
		return (error);
	so = fp->f_data;

#ifdef MAC
	error = mac_socket_check_receive(td->td_ucred, so);
	if (error) {
		fdrop(fp, td);
		return (error);
	}
#endif

	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if ((auio.uio_resid += iov->iov_len) < 0) {
			fdrop(fp, td);
			return (EINVAL);
		}
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif
	len = auio.uio_resid;
	CURVNET_SET(so->so_vnet);
	error = soreceive(so, &fromsa, &auio, (struct mbuf **)0,
	    (mp->msg_control || controlp) ? &control : (struct mbuf **)0,
	    &mp->msg_flags);
	CURVNET_RESTORE();
	if (error) {
		if (auio.uio_resid != (int)len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = (int)len - auio.uio_resid;
		ktrgenio(s, UIO_READ, ktruio, error);
	}
#endif
	if (error)
		goto out;
	td->td_retval[0] = (int)len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || fromsa == 0)
			len = 0;
		else {
			/* save sa_len before it is destroyed by MSG_COMPAT */
			len = MIN(len, fromsa->sa_len);
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				((struct osockaddr *)fromsa)->sa_family =
				    fromsa->sa_family;
#endif
			if (fromseg == UIO_USERSPACE) {
				error = copyout(fromsa, mp->msg_name,
				    (unsigned)len);
				if (error)
					goto out;
			} else
				bcopy(fromsa, mp->msg_name, len);
		}
		mp->msg_namelen = len;
	}
	if (mp->msg_control && controlp == NULL) {
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
		ctlbuf = mp->msg_control;

		while (m && len > 0) {
			unsigned int tocopy;

			if (len >= m->m_len)
				tocopy = m->m_len;
			else {
				mp->msg_flags |= MSG_CTRUNC;
				tocopy = len;
			}

			if ((error = copyout(mtod(m, caddr_t),
					ctlbuf, tocopy)) != 0)
				goto out;

			ctlbuf += tocopy;
			len -= tocopy;
			m = m->m_next;
		}
		mp->msg_controllen = ctlbuf - (caddr_t)mp->msg_control;
	}
out:
	fdrop(fp, td);
#ifdef KTRACE
	if (fromsa && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(fromsa);
#endif
	if (fromsa)
		free(fromsa, M_SONAME);

	if (error == 0 && controlp != NULL)  
		*controlp = control;
	else  if (control)
		m_freem(control);

	return (error);
}

static int
recvit(td, s, mp, namelenp)
	struct thread *td;
	int s;
	struct msghdr *mp;
	void *namelenp;
{
	int error;

	error = kern_recvit(td, s, mp, UIO_USERSPACE, NULL);
	if (error)
		return (error);
	if (namelenp) {
		error = copyout(&mp->msg_namelen, namelenp, sizeof (socklen_t));
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags & MSG_COMPAT)
			error = 0;	/* old recvfrom didn't check */
#endif
	}
	return (error);
}

int
recvfrom(td, uap)
	struct thread *td;
	struct recvfrom_args /* {
		int	s;
		caddr_t	buf;
		size_t	len;
		int	flags;
		struct sockaddr * __restrict	from;
		socklen_t * __restrict fromlenaddr;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (uap->fromlenaddr) {
		error = copyin(uap->fromlenaddr,
		    &msg.msg_namelen, sizeof (msg.msg_namelen));
		if (error)
			goto done2;
	} else {
		msg.msg_namelen = 0;
	}
	msg.msg_name = uap->from;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	error = recvit(td, uap->s, &msg, uap->fromlenaddr);
done2:
	return(error);
}

#ifdef COMPAT_OLDSOCK
int
orecvfrom(td, uap)
	struct thread *td;
	struct recvfrom_args *uap;
{

	uap->flags |= MSG_COMPAT;
	return (recvfrom(td, uap));
}
#endif

#ifdef COMPAT_OLDSOCK
int
orecv(td, uap)
	struct thread *td;
	struct orecv_args /* {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	error = recvit(td, uap->s, &msg, NULL);
	return (error);
}

/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
int
orecvmsg(td, uap)
	struct thread *td;
	struct orecvmsg_args /* {
		int	s;
		struct	omsghdr *msg;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (struct omsghdr));
	if (error)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error)
		return (error);
	msg.msg_flags = uap->flags | MSG_COMPAT;
	msg.msg_iov = iov;
	error = recvit(td, uap->s, &msg, &uap->msg->msg_namelen);
	if (msg.msg_controllen && error == 0)
		error = copyout(&msg.msg_controllen,
		    &uap->msg->msg_accrightslen, sizeof (int));
	free(iov, M_IOV);
	return (error);
}
#endif

int
recvmsg(td, uap)
	struct thread *td;
	struct recvmsg_args /* {
		int	s;
		struct	msghdr *msg;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct iovec *uiov, *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (msg));
	if (error)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error)
		return (error);
	msg.msg_flags = uap->flags;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags &= ~MSG_COMPAT;
#endif
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	error = recvit(td, uap->s, &msg, NULL);
	if (error == 0) {
		msg.msg_iov = uiov;
		error = copyout(&msg, uap->msg, sizeof(msg));
	}
	free(iov, M_IOV);
	return (error);
}

/* ARGSUSED */
int
shutdown(td, uap)
	struct thread *td;
	struct shutdown_args /* {
		int	s;
		int	how;
	} */ *uap;
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(uap->s);
	error = getsock(td->td_proc->p_fd, uap->s, &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		error = soshutdown(so, uap->how);
		fdrop(fp, td);
	}
	return (error);
}

/* ARGSUSED */
int
setsockopt(td, uap)
	struct thread *td;
	struct setsockopt_args /* {
		int	s;
		int	level;
		int	name;
		caddr_t	val;
		int	valsize;
	} */ *uap;
{

	return (kern_setsockopt(td, uap->s, uap->level, uap->name,
	    uap->val, UIO_USERSPACE, uap->valsize));
}

int
kern_setsockopt(td, s, level, name, val, valseg, valsize)
	struct thread *td;
	int s;
	int level;
	int name;
	void *val;
	enum uio_seg valseg;
	socklen_t valsize;
{
	int error;
	struct socket *so;
	struct file *fp;
	struct sockopt sopt;

	if (val == NULL && valsize != 0)
		return (EFAULT);
	if ((int)valsize < 0)
		return (EINVAL);

	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = level;
	sopt.sopt_name = name;
	sopt.sopt_val = val;
	sopt.sopt_valsize = valsize;
	switch (valseg) {
	case UIO_USERSPACE:
		sopt.sopt_td = td;
		break;
	case UIO_SYSSPACE:
		sopt.sopt_td = NULL;
		break;
	default:
		panic("kern_setsockopt called with bad valseg");
	}

	AUDIT_ARG_FD(s);
	error = getsock(td->td_proc->p_fd, s, &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		CURVNET_SET(so->so_vnet);
		error = sosetopt(so, &sopt);
		CURVNET_RESTORE();
		fdrop(fp, td);
	}
	return(error);
}

/* ARGSUSED */
int
getsockopt(td, uap)
	struct thread *td;
	struct getsockopt_args /* {
		int	s;
		int	level;
		int	name;
		void * __restrict	val;
		socklen_t * __restrict avalsize;
	} */ *uap;
{
	socklen_t valsize;
	int	error;

	if (uap->val) {
		error = copyin(uap->avalsize, &valsize, sizeof (valsize));
		if (error)
			return (error);
	}

	error = kern_getsockopt(td, uap->s, uap->level, uap->name,
	    uap->val, UIO_USERSPACE, &valsize);

	if (error == 0)
		error = copyout(&valsize, uap->avalsize, sizeof (valsize));
	return (error);
}

/*
 * Kernel version of getsockopt.
 * optval can be a userland or userspace. optlen is always a kernel pointer.
 */
int
kern_getsockopt(td, s, level, name, val, valseg, valsize)
	struct thread *td;
	int s;
	int level;
	int name;
	void *val;
	enum uio_seg valseg;
	socklen_t *valsize;
{
	int error;
	struct  socket *so;
	struct file *fp;
	struct	sockopt sopt;

	if (val == NULL)
		*valsize = 0;
	if ((int)*valsize < 0)
		return (EINVAL);

	sopt.sopt_dir = SOPT_GET;
	sopt.sopt_level = level;
	sopt.sopt_name = name;
	sopt.sopt_val = val;
	sopt.sopt_valsize = (size_t)*valsize; /* checked non-negative above */
	switch (valseg) {
	case UIO_USERSPACE:
		sopt.sopt_td = td;
		break;
	case UIO_SYSSPACE:
		sopt.sopt_td = NULL;
		break;
	default:
		panic("kern_getsockopt called with bad valseg");
	}

	AUDIT_ARG_FD(s);
	error = getsock(td->td_proc->p_fd, s, &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		CURVNET_SET(so->so_vnet);
		error = sogetopt(so, &sopt);
		CURVNET_RESTORE();
		*valsize = sopt.sopt_valsize;
		fdrop(fp, td);
	}
	return (error);
}

/*
 * getsockname1() - Get socket name.
 */
/* ARGSUSED */
static int
getsockname1(td, uap, compat)
	struct thread *td;
	struct getsockname_args /* {
		int	fdes;
		struct sockaddr * __restrict asa;
		socklen_t * __restrict alen;
	} */ *uap;
	int compat;
{
	struct sockaddr *sa;
	socklen_t len;
	int error;

	error = copyin(uap->alen, &len, sizeof(len));
	if (error)
		return (error);

	error = kern_getsockname(td, uap->fdes, &sa, &len);
	if (error)
		return (error);

	if (len != 0) {
#ifdef COMPAT_OLDSOCK
		if (compat)
			((struct osockaddr *)sa)->sa_family = sa->sa_family;
#endif
		error = copyout(sa, uap->asa, (u_int)len);
	}
	free(sa, M_SONAME);
	if (error == 0)
		error = copyout(&len, uap->alen, sizeof(len));
	return (error);
}

int
kern_getsockname(struct thread *td, int fd, struct sockaddr **sa,
    socklen_t *alen)
{
	struct socket *so;
	struct file *fp;
	socklen_t len;
	int error;

	if (*alen < 0)
		return (EINVAL);

	AUDIT_ARG_FD(fd);
	error = getsock(td->td_proc->p_fd, fd, &fp, NULL);
	if (error)
		return (error);
	so = fp->f_data;
	*sa = NULL;
	CURVNET_SET(so->so_vnet);
	error = (*so->so_proto->pr_usrreqs->pru_sockaddr)(so, sa);
	CURVNET_RESTORE();
	if (error)
		goto bad;
	if (*sa == NULL)
		len = 0;
	else
		len = MIN(*alen, (*sa)->sa_len);
	*alen = len;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(*sa);
#endif
bad:
	fdrop(fp, td);
	if (error && *sa) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}
	return (error);
}

int
getsockname(td, uap)
	struct thread *td;
	struct getsockname_args *uap;
{

	return (getsockname1(td, uap, 0));
}

#ifdef COMPAT_OLDSOCK
int
ogetsockname(td, uap)
	struct thread *td;
	struct getsockname_args *uap;
{

	return (getsockname1(td, uap, 1));
}
#endif /* COMPAT_OLDSOCK */

/*
 * getpeername1() - Get name of peer for connected socket.
 */
/* ARGSUSED */
static int
getpeername1(td, uap, compat)
	struct thread *td;
	struct getpeername_args /* {
		int	fdes;
		struct sockaddr * __restrict	asa;
		socklen_t * __restrict	alen;
	} */ *uap;
	int compat;
{
	struct sockaddr *sa;
	socklen_t len;
	int error;

	error = copyin(uap->alen, &len, sizeof (len));
	if (error)
		return (error);

	error = kern_getpeername(td, uap->fdes, &sa, &len);
	if (error)
		return (error);

	if (len != 0) {
#ifdef COMPAT_OLDSOCK
		if (compat)
			((struct osockaddr *)sa)->sa_family = sa->sa_family;
#endif
		error = copyout(sa, uap->asa, (u_int)len);
	}
	free(sa, M_SONAME);
	if (error == 0)
		error = copyout(&len, uap->alen, sizeof(len));
	return (error);
}

int
kern_getpeername(struct thread *td, int fd, struct sockaddr **sa,
    socklen_t *alen)
{
	struct socket *so;
	struct file *fp;
	socklen_t len;
	int error;

	if (*alen < 0)
		return (EINVAL);

	AUDIT_ARG_FD(fd);
	error = getsock(td->td_proc->p_fd, fd, &fp, NULL);
	if (error)
		return (error);
	so = fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0) {
		error = ENOTCONN;
		goto done;
	}
	*sa = NULL;
	CURVNET_SET(so->so_vnet);
	error = (*so->so_proto->pr_usrreqs->pru_peeraddr)(so, sa);
	CURVNET_RESTORE();
	if (error)
		goto bad;
	if (*sa == NULL)
		len = 0;
	else
		len = MIN(*alen, (*sa)->sa_len);
	*alen = len;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(*sa);
#endif
bad:
	if (error && *sa) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}
done:
	fdrop(fp, td);
	return (error);
}

int
getpeername(td, uap)
	struct thread *td;
	struct getpeername_args *uap;
{

	return (getpeername1(td, uap, 0));
}

#ifdef COMPAT_OLDSOCK
int
ogetpeername(td, uap)
	struct thread *td;
	struct ogetpeername_args *uap;
{

	/* XXX uap should have type `getpeername_args *' to begin with. */
	return (getpeername1(td, (struct getpeername_args *)uap, 1));
}
#endif /* COMPAT_OLDSOCK */

int
sockargs(mp, buf, buflen, type)
	struct mbuf **mp;
	caddr_t buf;
	int buflen, type;
{
	struct sockaddr *sa;
	struct mbuf *m;
	int error;

	if ((u_int)buflen > MLEN) {
#ifdef COMPAT_OLDSOCK
		if (type == MT_SONAME && (u_int)buflen <= 112)
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
			if ((u_int)buflen > MCLBYTES)
				return (EINVAL);
	}
	m = m_get(M_WAIT, type);
	if ((u_int)buflen > MLEN)
		MCLGET(m, M_WAIT);
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
		return (ENAMETOOLONG);
	if (len < offsetof(struct sockaddr, sa_data[0]))
		return (EINVAL);
	sa = malloc(len, M_SONAME, M_WAITOK);
	error = copyin(uaddr, sa, len);
	if (error) {
		free(sa, M_SONAME);
	} else {
#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = len;
		*namp = sa;
	}
	return (error);
}

#include <sys/condvar.h>

struct sendfile_sync {
	struct mtx	mtx;
	struct cv	cv;
	unsigned 	count;
};

/*
 * Detach mapped page and release resources back to the system.
 */
void
sf_buf_mext(void *addr, void *args)
{
	vm_page_t m;
	struct sendfile_sync *sfs;

	m = sf_buf_page(args);
	sf_buf_free(args);
	vm_page_lock_queues();
	vm_page_unwire(m, 0);
	/*
	 * Check for the object going away on us. This can
	 * happen since we don't hold a reference to it.
	 * If so, we're responsible for freeing the page.
	 */
	if (m->wire_count == 0 && m->object == NULL)
		vm_page_free(m);
	vm_page_unlock_queues();
	if (addr == NULL)
		return;
	sfs = addr;
	mtx_lock(&sfs->mtx);
	KASSERT(sfs->count> 0, ("Sendfile sync botchup count == 0"));
	if (--sfs->count == 0)
		cv_signal(&sfs->cv);
	mtx_unlock(&sfs->mtx);
}

/*
 * sendfile(2)
 *
 * int sendfile(int fd, int s, off_t offset, size_t nbytes,
 *	 struct sf_hdtr *hdtr, off_t *sbytes, int flags)
 *
 * Send a file specified by 'fd' and starting at 'offset' to a socket
 * specified by 's'. Send only 'nbytes' of the file or until EOF if nbytes ==
 * 0.  Optionally add a header and/or trailer to the socket output.  If
 * specified, write the total number of bytes sent into *sbytes.
 */
int
sendfile(struct thread *td, struct sendfile_args *uap)
{

	return (do_sendfile(td, uap, 0));
}

static int
do_sendfile(struct thread *td, struct sendfile_args *uap, int compat)
{
	struct sf_hdtr hdtr;
	struct uio *hdr_uio, *trl_uio;
	int error;

	hdr_uio = trl_uio = NULL;

	if (uap->hdtr != NULL) {
		error = copyin(uap->hdtr, &hdtr, sizeof(hdtr));
		if (error)
			goto out;
		if (hdtr.headers != NULL) {
			error = copyinuio(hdtr.headers, hdtr.hdr_cnt, &hdr_uio);
			if (error)
				goto out;
		}
		if (hdtr.trailers != NULL) {
			error = copyinuio(hdtr.trailers, hdtr.trl_cnt, &trl_uio);
			if (error)
				goto out;

		}
	}

	error = kern_sendfile(td, uap, hdr_uio, trl_uio, compat);
out:
	if (hdr_uio)
		free(hdr_uio, M_IOV);
	if (trl_uio)
		free(trl_uio, M_IOV);
	return (error);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sendfile(struct thread *td, struct freebsd4_sendfile_args *uap)
{
	struct sendfile_args args;

	args.fd = uap->fd;
	args.s = uap->s;
	args.offset = uap->offset;
	args.nbytes = uap->nbytes;
	args.hdtr = uap->hdtr;
	args.sbytes = uap->sbytes;
	args.flags = uap->flags;

	return (do_sendfile(td, &args, 1));
}
#endif /* COMPAT_FREEBSD4 */

int
kern_sendfile(struct thread *td, struct sendfile_args *uap,
    struct uio *hdr_uio, struct uio *trl_uio, int compat)
{
	struct file *sock_fp;
	struct vnode *vp;
	struct vm_object *obj = NULL;
	struct socket *so = NULL;
	struct mbuf *m = NULL;
	struct sf_buf *sf;
	struct vm_page *pg;
	off_t off, xfsize, fsbytes = 0, sbytes = 0, rem = 0;
	int error, hdrlen = 0, mnw = 0;
	int vfslocked;
	struct sendfile_sync *sfs = NULL;

	/*
	 * The file descriptor must be a regular file and have a
	 * backing VM object.
	 * File offset must be positive.  If it goes beyond EOF
	 * we send only the header/trailer and no payload data.
	 */
	AUDIT_ARG_FD(uap->fd);
	if ((error = fgetvp_read(td, uap->fd, &vp)) != 0)
		goto out;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_lock(vp, LK_SHARED | LK_RETRY);
	if (vp->v_type == VREG) {
		obj = vp->v_object;
		if (obj != NULL) {
			/*
			 * Temporarily increase the backing VM
			 * object's reference count so that a forced
			 * reclamation of its vnode does not
			 * immediately destroy it.
			 */
			VM_OBJECT_LOCK(obj);
			if ((obj->flags & OBJ_DEAD) == 0) {
				vm_object_reference_locked(obj);
				VM_OBJECT_UNLOCK(obj);
			} else {
				VM_OBJECT_UNLOCK(obj);
				obj = NULL;
			}
		}
	}
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	if (obj == NULL) {
		error = EINVAL;
		goto out;
	}
	if (uap->offset < 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * The socket must be a stream socket and connected.
	 * Remember if it a blocking or non-blocking socket.
	 */
	if ((error = getsock(td->td_proc->p_fd, uap->s, &sock_fp,
	    NULL)) != 0)
		goto out;
	so = sock_fp->f_data;
	if (so->so_type != SOCK_STREAM) {
		error = EINVAL;
		goto out;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto out;
	}
	/*
	 * Do not wait on memory allocations but return ENOMEM for
	 * caller to retry later.
	 * XXX: Experimental.
	 */
	if (uap->flags & SF_MNOWAIT)
		mnw = 1;

	if (uap->flags & SF_SYNC) {
		sfs = malloc(sizeof *sfs, M_TEMP, M_WAITOK);
		memset(sfs, 0, sizeof *sfs);
		mtx_init(&sfs->mtx, "sendfile", MTX_DEF, 0);
		cv_init(&sfs->cv, "sendfile");
	}

#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error)
		goto out;
#endif

	/* If headers are specified copy them into mbufs. */
	if (hdr_uio != NULL) {
		hdr_uio->uio_td = td;
		hdr_uio->uio_rw = UIO_WRITE;
		if (hdr_uio->uio_resid > 0) {
			/*
			 * In FBSD < 5.0 the nbytes to send also included
			 * the header.  If compat is specified subtract the
			 * header size from nbytes.
			 */
			if (compat) {
				if (uap->nbytes > hdr_uio->uio_resid)
					uap->nbytes -= hdr_uio->uio_resid;
				else
					uap->nbytes = 0;
			}
			m = m_uiotombuf(hdr_uio, (mnw ? M_NOWAIT : M_WAITOK),
			    0, 0, 0);
			if (m == NULL) {
				error = mnw ? EAGAIN : ENOBUFS;
				goto out;
			}
			hdrlen = m_length(m, NULL);
		}
	}

	/*
	 * Protect against multiple writers to the socket.
	 *
	 * XXXRW: Historically this has assumed non-interruptibility, so now
	 * we implement that, but possibly shouldn't.
	 */
	(void)sblock(&so->so_snd, SBL_WAIT | SBL_NOINTR);

	/*
	 * Loop through the pages of the file, starting with the requested
	 * offset. Get a file page (do I/O if necessary), map the file page
	 * into an sf_buf, attach an mbuf header to the sf_buf, and queue
	 * it on the socket.
	 * This is done in two loops.  The inner loop turns as many pages
	 * as it can, up to available socket buffer space, without blocking
	 * into mbufs to have it bulk delivered into the socket send buffer.
	 * The outer loop checks the state and available space of the socket
	 * and takes care of the overall progress.
	 */
	for (off = uap->offset, rem = uap->nbytes; ; ) {
		int loopbytes = 0;
		int space = 0;
		int done = 0;

		/*
		 * Check the socket state for ongoing connection,
		 * no errors and space in socket buffer.
		 * If space is low allow for the remainder of the
		 * file to be processed if it fits the socket buffer.
		 * Otherwise block in waiting for sufficient space
		 * to proceed, or if the socket is nonblocking, return
		 * to userland with EAGAIN while reporting how far
		 * we've come.
		 * We wait until the socket buffer has significant free
		 * space to do bulk sends.  This makes good use of file
		 * system read ahead and allows packet segmentation
		 * offloading hardware to take over lots of work.  If
		 * we were not careful here we would send off only one
		 * sfbuf at a time.
		 */
		SOCKBUF_LOCK(&so->so_snd);
		if (so->so_snd.sb_lowat < so->so_snd.sb_hiwat / 2)
			so->so_snd.sb_lowat = so->so_snd.sb_hiwat / 2;
retry_space:
		if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
			error = EPIPE;
			SOCKBUF_UNLOCK(&so->so_snd);
			goto done;
		} else if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			SOCKBUF_UNLOCK(&so->so_snd);
			goto done;
		}
		space = sbspace(&so->so_snd);
		if (space < rem &&
		    (space <= 0 ||
		     space < so->so_snd.sb_lowat)) {
			if (so->so_state & SS_NBIO) {
				SOCKBUF_UNLOCK(&so->so_snd);
				error = EAGAIN;
				goto done;
			}
			/*
			 * sbwait drops the lock while sleeping.
			 * When we loop back to retry_space the
			 * state may have changed and we retest
			 * for it.
			 */
			error = sbwait(&so->so_snd);
			/*
			 * An error from sbwait usually indicates that we've
			 * been interrupted by a signal. If we've sent anything
			 * then return bytes sent, otherwise return the error.
			 */
			if (error) {
				SOCKBUF_UNLOCK(&so->so_snd);
				goto done;
			}
			goto retry_space;
		}
		SOCKBUF_UNLOCK(&so->so_snd);

		/*
		 * Reduce space in the socket buffer by the size of
		 * the header mbuf chain.
		 * hdrlen is set to 0 after the first loop.
		 */
		space -= hdrlen;

		/*
		 * Loop and construct maximum sized mbuf chain to be bulk
		 * dumped into socket buffer.
		 */
		while(space > loopbytes) {
			vm_pindex_t pindex;
			vm_offset_t pgoff;
			struct mbuf *m0;

			VM_OBJECT_LOCK(obj);
			/*
			 * Calculate the amount to transfer.
			 * Not to exceed a page, the EOF,
			 * or the passed in nbytes.
			 */
			pgoff = (vm_offset_t)(off & PAGE_MASK);
			xfsize = omin(PAGE_SIZE - pgoff,
			    obj->un_pager.vnp.vnp_size - uap->offset -
			    fsbytes - loopbytes);
			if (uap->nbytes)
				rem = (uap->nbytes - fsbytes - loopbytes);
			else
				rem = obj->un_pager.vnp.vnp_size -
				    uap->offset - fsbytes - loopbytes;
			xfsize = omin(rem, xfsize);
			if (xfsize <= 0) {
				VM_OBJECT_UNLOCK(obj);
				done = 1;		/* all data sent */
				break;
			}
			/*
			 * Don't overflow the send buffer.
			 * Stop here and send out what we've
			 * already got.
			 */
			if (space < loopbytes + xfsize) {
				VM_OBJECT_UNLOCK(obj);
				break;
			}

			/*
			 * Attempt to look up the page.  Allocate
			 * if not found or wait and loop if busy.
			 */
			pindex = OFF_TO_IDX(off);
			pg = vm_page_grab(obj, pindex, VM_ALLOC_NOBUSY |
			    VM_ALLOC_NORMAL | VM_ALLOC_WIRED | VM_ALLOC_RETRY);

			/*
			 * Check if page is valid for what we need,
			 * otherwise initiate I/O.
			 * If we already turned some pages into mbufs,
			 * send them off before we come here again and
			 * block.
			 */
			if (pg->valid && vm_page_is_valid(pg, pgoff, xfsize))
				VM_OBJECT_UNLOCK(obj);
			else if (m != NULL)
				error = EAGAIN;	/* send what we already got */
			else if (uap->flags & SF_NODISKIO)
				error = EBUSY;
			else {
				int bsize, resid;

				/*
				 * Ensure that our page is still around
				 * when the I/O completes.
				 */
				vm_page_io_start(pg);
				VM_OBJECT_UNLOCK(obj);

				/*
				 * Get the page from backing store.
				 */
				vfslocked = VFS_LOCK_GIANT(vp->v_mount);
				error = vn_lock(vp, LK_SHARED);
				if (error != 0)
					goto after_read;
				bsize = vp->v_mount->mnt_stat.f_iosize;

				/*
				 * XXXMAC: Because we don't have fp->f_cred
				 * here, we pass in NOCRED.  This is probably
				 * wrong, but is consistent with our original
				 * implementation.
				 */
				error = vn_rdwr(UIO_READ, vp, NULL, MAXBSIZE,
				    trunc_page(off), UIO_NOCOPY, IO_NODELOCKED |
				    IO_VMIO | ((MAXBSIZE / bsize) << IO_SEQSHIFT),
				    td->td_ucred, NOCRED, &resid, td);
				VOP_UNLOCK(vp, 0);
			after_read:
				VFS_UNLOCK_GIANT(vfslocked);
				VM_OBJECT_LOCK(obj);
				vm_page_io_finish(pg);
				if (!error)
					VM_OBJECT_UNLOCK(obj);
				mbstat.sf_iocnt++;
			}
			if (error) {
				vm_page_lock_queues();
				vm_page_unwire(pg, 0);
				/*
				 * See if anyone else might know about
				 * this page.  If not and it is not valid,
				 * then free it.
				 */
				if (pg->wire_count == 0 && pg->valid == 0 &&
				    pg->busy == 0 && !(pg->oflags & VPO_BUSY) &&
				    pg->hold_count == 0) {
					vm_page_free(pg);
				}
				vm_page_unlock_queues();
				VM_OBJECT_UNLOCK(obj);
				if (error == EAGAIN)
					error = 0;	/* not a real error */
				break;
			}

			/*
			 * Get a sendfile buf.  We usually wait as long
			 * as necessary, but this wait can be interrupted.
			 */
			if ((sf = sf_buf_alloc(pg,
			    (mnw ? SFB_NOWAIT : SFB_CATCH))) == NULL) {
				mbstat.sf_allocfail++;
				vm_page_lock_queues();
				vm_page_unwire(pg, 0);
				/*
				 * XXX: Not same check as above!?
				 */
				if (pg->wire_count == 0 && pg->object == NULL)
					vm_page_free(pg);
				vm_page_unlock_queues();
				error = (mnw ? EAGAIN : EINTR);
				break;
			}

			/*
			 * Get an mbuf and set it up as having
			 * external storage.
			 */
			m0 = m_get((mnw ? M_NOWAIT : M_WAITOK), MT_DATA);
			if (m0 == NULL) {
				error = (mnw ? EAGAIN : ENOBUFS);
				sf_buf_mext((void *)sf_buf_kva(sf), sf);
				break;
			}
			MEXTADD(m0, sf_buf_kva(sf), PAGE_SIZE, sf_buf_mext,
			    sfs, sf, M_RDONLY, EXT_SFBUF);
			m0->m_data = (char *)sf_buf_kva(sf) + pgoff;
			m0->m_len = xfsize;

			/* Append to mbuf chain. */
			if (m != NULL)
				m_cat(m, m0);
			else
				m = m0;

			/* Keep track of bits processed. */
			loopbytes += xfsize;
			off += xfsize;

			if (sfs != NULL) {
				mtx_lock(&sfs->mtx);
				sfs->count++;
				mtx_unlock(&sfs->mtx);
			}
		}

		/* Add the buffer chain to the socket buffer. */
		if (m != NULL) {
			int mlen, err;

			mlen = m_length(m, NULL);
			SOCKBUF_LOCK(&so->so_snd);
			if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
				error = EPIPE;
				SOCKBUF_UNLOCK(&so->so_snd);
				goto done;
			}
			SOCKBUF_UNLOCK(&so->so_snd);
			CURVNET_SET(so->so_vnet);
			/* Avoid error aliasing. */
			err = (*so->so_proto->pr_usrreqs->pru_send)
				    (so, 0, m, NULL, NULL, td);
			CURVNET_RESTORE();
			if (err == 0) {
				/*
				 * We need two counters to get the
				 * file offset and nbytes to send
				 * right:
				 * - sbytes contains the total amount
				 *   of bytes sent, including headers.
				 * - fsbytes contains the total amount
				 *   of bytes sent from the file.
				 */
				sbytes += mlen;
				fsbytes += mlen;
				if (hdrlen) {
					fsbytes -= hdrlen;
					hdrlen = 0;
				}
			} else if (error == 0)
				error = err;
			m = NULL;	/* pru_send always consumes */
		}

		/* Quit outer loop on error or when we're done. */
		if (done) 
			break;
		if (error)
			goto done;
	}

	/*
	 * Send trailers. Wimp out and use writev(2).
	 */
	if (trl_uio != NULL) {
		sbunlock(&so->so_snd);
		error = kern_writev(td, uap->s, trl_uio);
		if (error == 0)
			sbytes += td->td_retval[0];
		goto out;
	}

done:
	sbunlock(&so->so_snd);
out:
	/*
	 * If there was no error we have to clear td->td_retval[0]
	 * because it may have been set by writev.
	 */
	if (error == 0) {
		td->td_retval[0] = 0;
	}
	if (uap->sbytes != NULL) {
		copyout(&sbytes, uap->sbytes, sizeof(off_t));
	}
	if (obj != NULL)
		vm_object_deallocate(obj);
	if (vp != NULL) {
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	if (so)
		fdrop(sock_fp, td);
	if (m)
		m_freem(m);

	if (sfs != NULL) {
		mtx_lock(&sfs->mtx);
		if (sfs->count != 0)
			cv_wait(&sfs->cv, &sfs->mtx);
		KASSERT(sfs->count == 0, ("sendfile sync still busy"));
		cv_destroy(&sfs->cv);
		mtx_destroy(&sfs->mtx);
		free(sfs, M_TEMP);
	}

	if (error == ERESTART)
		error = EINTR;

	return (error);
}

/*
 * SCTP syscalls.
 * Functionality only compiled in if SCTP is defined in the kernel Makefile,
 * otherwise all return EOPNOTSUPP.
 * XXX: We should make this loadable one day.
 */
int
sctp_peeloff(td, uap)
	struct thread *td;
	struct sctp_peeloff_args /* {
		int	sd;
		caddr_t	name;
	} */ *uap;
{
#if (defined(INET) || defined(INET6)) && defined(SCTP)
	struct filedesc *fdp;
	struct file *nfp = NULL;
	int error;
	struct socket *head, *so;
	int fd;
	u_int fflag;

	fdp = td->td_proc->p_fd;
	AUDIT_ARG_FD(uap->sd);
	error = fgetsock(td, uap->sd, &head, &fflag);
	if (error)
		goto done2;
	error = sctp_can_peel_off(head, (sctp_assoc_t)uap->name);
	if (error)
		goto done2;
	/*
	 * At this point we know we do have a assoc to pull
	 * we proceed to get the fd setup. This may block
	 * but that is ok.
	 */

	error = falloc(td, &nfp, &fd);
	if (error)
		goto done;
	td->td_retval[0] = fd;

	so = sonewconn(head, SS_ISCONNECTED);
	if (so == NULL) 
		goto noconnection;
	/*
	 * Before changing the flags on the socket, we have to bump the
	 * reference count.  Otherwise, if the protocol calls sofree(),
	 * the socket will be released due to a zero refcount.
	 */
        SOCK_LOCK(so);
        soref(so);                      /* file descriptor reference */
        SOCK_UNLOCK(so);

	ACCEPT_LOCK();

	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;
	so->so_state |= (head->so_state & SS_NBIO);
	so->so_state &= ~SS_NOFDREF;
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;
	ACCEPT_UNLOCK();
	finit(nfp, fflag, DTYPE_SOCKET, so, &socketops);
	error = sctp_do_peeloff(head, so, (sctp_assoc_t)uap->name);
	if (error)
		goto noconnection;
	if (head->so_sigio != NULL)
		fsetown(fgetown(&head->so_sigio), &so->so_sigio);

noconnection:
	/*
	 * close the new descriptor, assuming someone hasn't ripped it
	 * out from under us.
	 */
	if (error)
		fdclose(fdp, nfp, fd, td);

	/*
	 * Release explicitly held references before returning.
	 */
done:
	if (nfp != NULL)
		fdrop(nfp, td);
	fputsock(head);
done2:
	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}

int
sctp_generic_sendmsg (td, uap)
	struct thread *td;
	struct sctp_generic_sendmsg_args /* {
		int sd, 
		caddr_t msg, 
		int mlen, 
		caddr_t to, 
		__socklen_t tolen, 
		struct sctp_sndrcvinfo *sinfo, 
		int flags
	} */ *uap;
{
#if (defined(INET) || defined(INET6)) && defined(SCTP)
	struct sctp_sndrcvinfo sinfo, *u_sinfo = NULL;
	struct socket *so;
	struct file *fp = NULL;
	int use_rcvinfo = 1;
	int error = 0, len;
	struct sockaddr *to = NULL;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	struct uio auio;
	struct iovec iov[1];

	if (uap->sinfo) {
		error = copyin(uap->sinfo, &sinfo, sizeof (sinfo));
		if (error)
			return (error);
		u_sinfo = &sinfo;
	}
	if (uap->tolen) {
		error = getsockaddr(&to, uap->to, uap->tolen);
		if (error) {
			to = NULL;
			goto sctp_bad2;
		}
	}

	AUDIT_ARG_FD(uap->sd);
	error = getsock(td->td_proc->p_fd, uap->sd, &fp, NULL);
	if (error)
		goto sctp_bad;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(to);
#endif

	iov[0].iov_base = uap->msg;
	iov[0].iov_len = uap->mlen;

	so = (struct socket *)fp->f_data;
#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error)
		goto sctp_bad;
#endif /* MAC */

	auio.uio_iov =  iov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	len = auio.uio_resid = uap->mlen;
	error = sctp_lower_sosend(so, to, &auio,
		    (struct mbuf *)NULL, (struct mbuf *)NULL,
		    uap->flags, use_rcvinfo, u_sinfo, td);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket. */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(uap->flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			psignal(td->td_proc, SIGPIPE);
			PROC_UNLOCK(td->td_proc);
		}
	}
	if (error == 0)
		td->td_retval[0] = len - auio.uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = td->td_retval[0];
		ktrgenio(uap->sd, UIO_WRITE, ktruio, error);
	}
#endif /* KTRACE */
sctp_bad:
	if (fp)
		fdrop(fp, td);
sctp_bad2:
	if (to)
		free(to, M_SONAME);
	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}

int
sctp_generic_sendmsg_iov(td, uap)
	struct thread *td;
	struct sctp_generic_sendmsg_iov_args /* {
		int sd, 
		struct iovec *iov, 
		int iovlen, 
		caddr_t to, 
		__socklen_t tolen, 
		struct sctp_sndrcvinfo *sinfo, 
		int flags
	} */ *uap;
{
#if (defined(INET) || defined(INET6)) && defined(SCTP)
	struct sctp_sndrcvinfo sinfo, *u_sinfo = NULL;
	struct socket *so;
	struct file *fp = NULL;
	int use_rcvinfo = 1;
	int error=0, len, i;
	struct sockaddr *to = NULL;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	struct uio auio;
	struct iovec *iov, *tiov;

	if (uap->sinfo) {
		error = copyin(uap->sinfo, &sinfo, sizeof (sinfo));
		if (error)
			return (error);
		u_sinfo = &sinfo;
	}
	if (uap->tolen) {
		error = getsockaddr(&to, uap->to, uap->tolen);
		if (error) {
			to = NULL;
			goto sctp_bad2;
		}
	}

	AUDIT_ARG_FD(uap->sd);
	error = getsock(td->td_proc->p_fd, uap->sd, &fp, NULL);
	if (error)
		goto sctp_bad1;

	error = copyiniov(uap->iov, uap->iovlen, &iov, EMSGSIZE);
	if (error)
		goto sctp_bad1;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(to);
#endif

	so = (struct socket *)fp->f_data;
#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error)
		goto sctp_bad;
#endif /* MAC */

	auio.uio_iov =  iov;
	auio.uio_iovcnt = uap->iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	tiov = iov;
	for (i = 0; i <uap->iovlen; i++, tiov++) {
		if ((auio.uio_resid += tiov->iov_len) < 0) {
			error = EINVAL;
			goto sctp_bad;
		}
	}
	len = auio.uio_resid;
	error = sctp_lower_sosend(so, to, &auio,
		    (struct mbuf *)NULL, (struct mbuf *)NULL,
		    uap->flags, use_rcvinfo, u_sinfo, td);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(uap->flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			psignal(td->td_proc, SIGPIPE);
			PROC_UNLOCK(td->td_proc);
		}
	}
	if (error == 0)
		td->td_retval[0] = len - auio.uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = td->td_retval[0];
		ktrgenio(uap->sd, UIO_WRITE, ktruio, error);
	}
#endif /* KTRACE */
sctp_bad:
	free(iov, M_IOV);
sctp_bad1:
	if (fp)
		fdrop(fp, td);
sctp_bad2:
	if (to)
		free(to, M_SONAME);
	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}

int
sctp_generic_recvmsg(td, uap)
	struct thread *td;
	struct sctp_generic_recvmsg_args /* {
		int sd, 
		struct iovec *iov, 
		int iovlen,
		struct sockaddr *from, 
		__socklen_t *fromlenaddr,
		struct sctp_sndrcvinfo *sinfo, 
		int *msg_flags
	} */ *uap;
{
#if (defined(INET) || defined(INET6)) && defined(SCTP)
	u_int8_t sockbufstore[256];
	struct uio auio;
	struct iovec *iov, *tiov;
	struct sctp_sndrcvinfo sinfo;
	struct socket *so;
	struct file *fp = NULL;
	struct sockaddr *fromsa;
	int fromlen;
	int len, i, msg_flags;
	int error = 0;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif

	AUDIT_ARG_FD(uap->sd);
	error = getsock(td->td_proc->p_fd, uap->sd, &fp, NULL);
	if (error) {
		return (error);
	}
	error = copyiniov(uap->iov, uap->iovlen, &iov, EMSGSIZE);
	if (error) {
		goto out1;
	}

	so = fp->f_data;
#ifdef MAC
	error = mac_socket_check_receive(td->td_ucred, so);
	if (error) {
		goto out;
		return (error);
	}
#endif /* MAC */

	if (uap->fromlenaddr) {
		error = copyin(uap->fromlenaddr,
		    &fromlen, sizeof (fromlen));
		if (error) {
			goto out;
		}
	} else {
		fromlen = 0;
	}
	if(uap->msg_flags) {
		error = copyin(uap->msg_flags, &msg_flags, sizeof (int));
		if (error) {
			goto out;
		}
	} else {
		msg_flags = 0;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovlen;
  	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	tiov = iov;
	for (i = 0; i <uap->iovlen; i++, tiov++) {
		if ((auio.uio_resid += tiov->iov_len) < 0) {
			error = EINVAL;
			goto out;
		}
	}
	len = auio.uio_resid;
	fromsa = (struct sockaddr *)sockbufstore;

#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif /* KTRACE */
	error = sctp_sorecvmsg(so, &auio, (struct mbuf **)NULL,
		    fromsa, fromlen, &msg_flags,
		    (struct sctp_sndrcvinfo *)&sinfo, 1);
	if (error) {
		if (auio.uio_resid != (int)len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	} else {
		if (uap->sinfo)
			error = copyout(&sinfo, uap->sinfo, sizeof (sinfo));
	}
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = (int)len - auio.uio_resid;
		ktrgenio(uap->sd, UIO_READ, ktruio, error);
	}
#endif /* KTRACE */
	if (error)
		goto out;
	td->td_retval[0] = (int)len - auio.uio_resid;

	if (fromlen && uap->from) {
		len = fromlen;
		if (len <= 0 || fromsa == 0)
			len = 0;
		else {
			len = MIN(len, fromsa->sa_len);
			error = copyout(fromsa, uap->from, (unsigned)len);
			if (error)
				goto out;
		}
		error = copyout(&len, uap->fromlenaddr, sizeof (socklen_t));
		if (error) {
			goto out;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(fromsa);
#endif
	if (uap->msg_flags) {
		error = copyout(&msg_flags, uap->msg_flags, sizeof (int));
		if (error) {
			goto out;
		}
	}
out:
	free(iov, M_IOV);
out1:
	if (fp) 
		fdrop(fp, td);

	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}
