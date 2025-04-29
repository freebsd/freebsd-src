/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_capsicum.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/filio.h>
#include <sys/jail.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#ifdef COMPAT_43
#include <sys/sysent.h>
#endif
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32_util.h>
#endif

#include <net/vnet.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

static int sendit(struct thread *td, int s, struct msghdr *mp, int flags);
static int recvit(struct thread *td, int s, struct msghdr *mp, void *namelenp);

static int accept1(struct thread *td, int s, struct sockaddr *uname,
		   socklen_t *anamelen, int flags);
static int sockargs(struct mbuf **, char *, socklen_t, int);

/*
 * Convert a user file descriptor to a kernel file entry and check if required
 * capability rights are present.
 * If required copy of current set of capability rights is returned.
 * A reference on the file entry is held upon returning.
 */
int
getsock_cap(struct thread *td, int fd, cap_rights_t *rightsp,
    struct file **fpp, struct filecaps *havecapsp)
{
	struct file *fp;
	int error;

	error = fget_cap(td, fd, rightsp, &fp, havecapsp);
	if (__predict_false(error != 0))
		return (error);
	if (__predict_false(fp->f_type != DTYPE_SOCKET)) {
		fdrop(fp, td);
		if (havecapsp != NULL)
			filecaps_free(havecapsp);
		return (ENOTSOCK);
	}
	*fpp = fp;
	return (0);
}

int
getsock(struct thread *td, int fd, cap_rights_t *rightsp, struct file **fpp)
{
	struct file *fp;
	int error;

	error = fget_unlocked(td, fd, rightsp, &fp);
	if (__predict_false(error != 0))
		return (error);
	if (__predict_false(fp->f_type != DTYPE_SOCKET)) {
		fdrop(fp, td);
		return (ENOTSOCK);
	}
	*fpp = fp;
	return (0);
}

/*
 * System call interface to the socket abstraction.
 */
#if defined(COMPAT_43)
#define COMPAT_OLDSOCK
#endif

int
sys_socket(struct thread *td, struct socket_args *uap)
{

	return (kern_socket(td, uap->domain, uap->type, uap->protocol));
}

int
kern_socket(struct thread *td, int domain, int type, int protocol)
{
	struct socket *so;
	struct file *fp;
	int fd, error, oflag, fflag;

	AUDIT_ARG_SOCKET(domain, type, protocol);

	oflag = 0;
	fflag = 0;
	if ((type & SOCK_CLOEXEC) != 0) {
		type &= ~SOCK_CLOEXEC;
		oflag |= O_CLOEXEC;
	}
	if ((type & SOCK_NONBLOCK) != 0) {
		type &= ~SOCK_NONBLOCK;
		fflag |= FNONBLOCK;
	}

#ifdef MAC
	error = mac_socket_check_create(td->td_ucred, domain, type, protocol);
	if (error != 0)
		return (error);
#endif
	error = falloc(td, &fp, &fd, oflag);
	if (error != 0)
		return (error);
	/* An extra reference on `fp' has been held for us by falloc(). */
	error = socreate(domain, &so, type, protocol, td->td_ucred, td);
	if (error != 0) {
		fdclose(td, fp, fd);
	} else {
		finit(fp, FREAD | FWRITE | fflag, DTYPE_SOCKET, so, &socketops);
		if ((fflag & FNONBLOCK) != 0)
			(void) fo_ioctl(fp, FIONBIO, &fflag, td->td_ucred, td);
		td->td_retval[0] = fd;
	}
	fdrop(fp, td);
	return (error);
}

int
sys_bind(struct thread *td, struct bind_args *uap)
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error == 0) {
		error = kern_bindat(td, AT_FDCWD, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

int
kern_bindat(struct thread *td, int dirfd, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	int error;

#ifdef CAPABILITY_MODE
	if (dirfd == AT_FDCWD) {
		if (CAP_TRACING(td))
			ktrcapfail(CAPFAIL_NAMEI, "AT_FDCWD");
		if (IN_CAPABILITY_MODE(td))
			return (ECAPMODE);
	}
#endif

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_SOCKADDR(td, dirfd, sa);
	error = getsock(td, fd, &cap_bind_rights, &fp);
	if (error != 0)
		return (error);
	so = fp->f_data;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(sa);
#endif
#ifdef MAC
	error = mac_socket_check_bind(td->td_ucred, so, sa);
	if (error == 0) {
#endif
		if (dirfd == AT_FDCWD)
			error = sobind(so, sa, td);
		else
			error = sobindat(dirfd, so, sa, td);
#ifdef MAC
	}
#endif
	fdrop(fp, td);
	return (error);
}

int
sys_bindat(struct thread *td, struct bindat_args *uap)
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error == 0) {
		error = kern_bindat(td, uap->fd, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

int
sys_listen(struct thread *td, struct listen_args *uap)
{

	return (kern_listen(td, uap->s, uap->backlog));
}

int
kern_listen(struct thread *td, int s, int backlog)
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(s);
	error = getsock(td, s, &cap_listen_rights, &fp);
	if (error == 0) {
		so = fp->f_data;
#ifdef MAC
		error = mac_socket_check_listen(td->td_ucred, so);
		if (error == 0)
#endif
			error = solisten(so, backlog, td);
		fdrop(fp, td);
	}
	return (error);
}

/*
 * accept1()
 */
static int
accept1(struct thread *td, int s, struct sockaddr *uname, socklen_t *anamelen,
    int flags)
{
	struct sockaddr_storage ss = { .ss_len = sizeof(ss) };
	socklen_t addrlen;
	struct file *fp;
	int error;

	if (uname != NULL) {
		error = copyin(anamelen, &addrlen, sizeof(addrlen));
		if (error != 0)
			return (error);
	}

	error = kern_accept4(td, s, (struct sockaddr *)&ss, flags, &fp);

	if (error != 0)
		return (error);

#ifdef COMPAT_OLDSOCK
	if (SV_PROC_FLAG(td->td_proc, SV_AOUT) &&
	    (flags & ACCEPT4_COMPAT) != 0)
		((struct osockaddr *)&ss)->sa_family = ss.ss_family;
#endif
	if (uname != NULL) {
		addrlen = min(ss.ss_len, addrlen);
		error = copyout(&ss, uname, addrlen);
		if (error == 0) {
			addrlen = ss.ss_len;
			error = copyout(&addrlen, anamelen, sizeof(addrlen));
		}
	}
	if (error != 0)
		fdclose(td, fp, td->td_retval[0]);
	fdrop(fp, td);

	return (error);
}

int
kern_accept(struct thread *td, int s, struct sockaddr *sa, struct file **fp)
{
	return (kern_accept4(td, s, sa, ACCEPT4_INHERIT, fp));
}

int
kern_accept4(struct thread *td, int s, struct sockaddr *sa, int flags,
    struct file **fp)
{
	struct file *headfp, *nfp = NULL;
	struct socket *head, *so;
	struct filecaps fcaps;
	u_int fflag;
	pid_t pgid;
	int error, fd, tmp;

	AUDIT_ARG_FD(s);
	error = getsock_cap(td, s, &cap_accept_rights,
	    &headfp, &fcaps);
	if (error != 0)
		return (error);
	fflag = atomic_load_int(&headfp->f_flag);
	head = headfp->f_data;
	if (!SOLISTENING(head)) {
		error = EINVAL;
		goto done;
	}
#ifdef MAC
	error = mac_socket_check_accept(td->td_ucred, head);
	if (error != 0)
		goto done;
#endif
	error = falloc_caps(td, &nfp, &fd,
	    (flags & SOCK_CLOEXEC) ? O_CLOEXEC : 0, &fcaps);
	if (error != 0)
		goto done;
	SOCK_LOCK(head);
	if (!SOLISTENING(head)) {
		SOCK_UNLOCK(head);
		error = EINVAL;
		goto noconnection;
	}

	error = solisten_dequeue(head, &so, flags);
	if (error != 0)
		goto noconnection;

	/* An extra reference on `nfp' has been held for us by falloc(). */
	td->td_retval[0] = fd;

	/* Connection has been removed from the listen queue. */
	KNOTE_UNLOCKED(&head->so_rdsel.si_note, 0);

	if (flags & ACCEPT4_INHERIT) {
		pgid = fgetown(&head->so_sigio);
		if (pgid != 0)
			fsetown(pgid, &so->so_sigio);
	} else {
		fflag &= ~(FNONBLOCK | FASYNC);
		if (flags & SOCK_NONBLOCK)
			fflag |= FNONBLOCK;
	}

	finit(nfp, fflag, DTYPE_SOCKET, so, &socketops);
	/* Sync socket nonblocking/async state with file flags */
	tmp = fflag & FNONBLOCK;
	(void) fo_ioctl(nfp, FIONBIO, &tmp, td->td_ucred, td);
	tmp = fflag & FASYNC;
	(void) fo_ioctl(nfp, FIOASYNC, &tmp, td->td_ucred, td);

	if ((error = soaccept(so, sa)) == 0) {
		AUDIT_ARG_SOCKADDR(td, AT_FDCWD, sa);
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			ktrsockaddr(sa);
#endif
	}
noconnection:
	/*
	 * close the new descriptor, assuming someone hasn't ripped it
	 * out from under us.
	 */
	if (error != 0)
		fdclose(td, nfp, fd);

	/*
	 * Release explicitly held references before returning.  We return
	 * a reference on nfp to the caller on success if they request it.
	 */
done:
	if (nfp == NULL)
		filecaps_free(&fcaps);
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
sys_accept(struct thread *td, struct accept_args *uap)
{

	return (accept1(td, uap->s, uap->name, uap->anamelen, ACCEPT4_INHERIT));
}

int
sys_accept4(struct thread *td, struct accept4_args *uap)
{

	if (uap->flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return (EINVAL);

	return (accept1(td, uap->s, uap->name, uap->anamelen, uap->flags));
}

#ifdef COMPAT_OLDSOCK
int
oaccept(struct thread *td, struct oaccept_args *uap)
{

	return (accept1(td, uap->s, uap->name, uap->anamelen,
	    ACCEPT4_INHERIT | ACCEPT4_COMPAT));
}
#endif /* COMPAT_OLDSOCK */

int
sys_connect(struct thread *td, struct connect_args *uap)
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error == 0) {
		error = kern_connectat(td, AT_FDCWD, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

int
kern_connectat(struct thread *td, int dirfd, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	int error;

#ifdef CAPABILITY_MODE
	if (dirfd == AT_FDCWD) {
		if (CAP_TRACING(td))
			ktrcapfail(CAPFAIL_NAMEI, "AT_FDCWD");
		if (IN_CAPABILITY_MODE(td))
			return (ECAPMODE);
	}
#endif

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_SOCKADDR(td, dirfd, sa);
	error = getsock(td, fd, &cap_connect_rights, &fp);
	if (error != 0)
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
	if (error != 0)
		goto bad;
#endif
	error = soconnectat(dirfd, so, sa, td);
	if (error != 0)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		error = EINPROGRESS;
		goto done1;
	}
	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = msleep(&so->so_timeo, &so->so_lock, PSOCK | PCATCH,
		    "connec", 0);
		if (error != 0)
			break;
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	SOCK_UNLOCK(so);
bad:
	if (error == ERESTART)
		error = EINTR;
done1:
	fdrop(fp, td);
	return (error);
}

int
sys_connectat(struct thread *td, struct connectat_args *uap)
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error == 0) {
		error = kern_connectat(td, uap->fd, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

int
kern_socketpair(struct thread *td, int domain, int type, int protocol,
    int *rsv)
{
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, oflag, fflag;

	AUDIT_ARG_SOCKET(domain, type, protocol);

	oflag = 0;
	fflag = 0;
	if ((type & SOCK_CLOEXEC) != 0) {
		type &= ~SOCK_CLOEXEC;
		oflag |= O_CLOEXEC;
	}
	if ((type & SOCK_NONBLOCK) != 0) {
		type &= ~SOCK_NONBLOCK;
		fflag |= FNONBLOCK;
	}
#ifdef MAC
	/* We might want to have a separate check for socket pairs. */
	error = mac_socket_check_create(td->td_ucred, domain, type,
	    protocol);
	if (error != 0)
		return (error);
#endif
	error = socreate(domain, &so1, type, protocol, td->td_ucred, td);
	if (error != 0)
		return (error);
	error = socreate(domain, &so2, type, protocol, td->td_ucred, td);
	if (error != 0)
		goto free1;
	/* On success extra reference to `fp1' and 'fp2' is set by falloc. */
	error = falloc(td, &fp1, &fd, oflag);
	if (error != 0)
		goto free2;
	rsv[0] = fd;
	fp1->f_data = so1;	/* so1 already has ref count */
	error = falloc(td, &fp2, &fd, oflag);
	if (error != 0)
		goto free3;
	fp2->f_data = so2;	/* so2 already has ref count */
	rsv[1] = fd;
	error = soconnect2(so1, so2);
	if (error != 0)
		goto free4;
	if (type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 error = soconnect2(so2, so1);
		 if (error != 0)
			goto free4;
	} else if (so1->so_proto->pr_flags & PR_CONNREQUIRED) {
		struct unpcb *unp, *unp2;
		unp = sotounpcb(so1);
		unp2 = sotounpcb(so2);
		/* 
		 * No need to lock the unps, because the sockets are brand-new.
		 * No other threads can be using them yet
		 */
		unp_copy_peercred(td, unp, unp2, unp);
	}
	finit(fp1, FREAD | FWRITE | fflag, DTYPE_SOCKET, fp1->f_data,
	    &socketops);
	finit(fp2, FREAD | FWRITE | fflag, DTYPE_SOCKET, fp2->f_data,
	    &socketops);
	if ((fflag & FNONBLOCK) != 0) {
		(void) fo_ioctl(fp1, FIONBIO, &fflag, td->td_ucred, td);
		(void) fo_ioctl(fp2, FIONBIO, &fflag, td->td_ucred, td);
	}
	fdrop(fp1, td);
	fdrop(fp2, td);
	return (0);
free4:
	fdclose(td, fp2, rsv[1]);
	fdrop(fp2, td);
free3:
	fdclose(td, fp1, rsv[0]);
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
sys_socketpair(struct thread *td, struct socketpair_args *uap)
{
	int error, sv[2];

	error = kern_socketpair(td, uap->domain, uap->type,
	    uap->protocol, sv);
	if (error != 0)
		return (error);
	error = copyout(sv, uap->rsv, 2 * sizeof(int));
	if (error != 0) {
		(void)kern_close(td, sv[0]);
		(void)kern_close(td, sv[1]);
	}
	return (error);
}

static int
sendit(struct thread *td, int s, struct msghdr *mp, int flags)
{
	struct mbuf *control;
	struct sockaddr *to;
	int error;

	if (mp->msg_name != NULL) {
		error = getsockaddr(&to, mp->msg_name, mp->msg_namelen);
		if (error != 0) {
			to = NULL;
			goto bad;
		}
		mp->msg_name = to;
#ifdef CAPABILITY_MODE
		if (CAP_TRACING(td))
			ktrcapfail(CAPFAIL_SOCKADDR, mp->msg_name);
		if (IN_CAPABILITY_MODE(td)) {
			error = ECAPMODE;
			goto bad;
		}
#endif
	} else {
		to = NULL;
	}

	if (mp->msg_control) {
		if (mp->msg_controllen < sizeof(struct cmsghdr)
#ifdef COMPAT_OLDSOCK
		    && (mp->msg_flags != MSG_COMPAT ||
		    !SV_PROC_FLAG(td->td_proc, SV_AOUT))
#endif
		) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL);
		if (error != 0)
			goto bad;
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags == MSG_COMPAT &&
		    SV_PROC_FLAG(td->td_proc, SV_AOUT)) {
			struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAITOK);
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
	free(to, M_SONAME);
	return (error);
}

int
kern_sendit(struct thread *td, int s, struct msghdr *mp, int flags,
    struct mbuf *control, enum uio_seg segflg)
{
	struct file *fp;
	struct uio auio;
	struct iovec *iov;
	struct socket *so;
	cap_rights_t *rights;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	ssize_t len;
	int i, error;

	AUDIT_ARG_FD(s);
	rights = &cap_send_rights;
	if (mp->msg_name != NULL) {
		AUDIT_ARG_SOCKADDR(td, AT_FDCWD, mp->msg_name);
		rights = &cap_send_connect_rights;
	}
	error = getsock(td, s, rights, &fp);
	if (error != 0) {
		m_freem(control);
		return (error);
	}
	so = (struct socket *)fp->f_data;

#ifdef KTRACE
	if (mp->msg_name != NULL && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(mp->msg_name);
#endif
#ifdef MAC
	if (mp->msg_name != NULL) {
		error = mac_socket_check_connect(td->td_ucred, so,
		    mp->msg_name);
		if (error != 0) {
			m_freem(control);
			goto bad;
		}
	}
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0) {
		m_freem(control);
		goto bad;
	}
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
			m_freem(control);
			goto bad;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif
	len = auio.uio_resid;
	error = sousrsend(so, mp->msg_name, &auio, control, flags, NULL);
	if (error == 0)
		td->td_retval[0] = len - auio.uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		if (error == 0)
			ktruio->uio_resid = td->td_retval[0];
		ktrgenio(s, UIO_WRITE, ktruio, error);
	}
#endif
bad:
	fdrop(fp, td);
	return (error);
}

int
sys_sendto(struct thread *td, struct sendto_args *uap)
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = __DECONST(void *, uap->to);
	msg.msg_namelen = uap->tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_OLDSOCK
	if (SV_PROC_FLAG(td->td_proc, SV_AOUT))
		msg.msg_flags = 0;
#endif
	aiov.iov_base = __DECONST(void *, uap->buf);
	aiov.iov_len = uap->len;
	return (sendit(td, uap->s, &msg, uap->flags));
}

#ifdef COMPAT_OLDSOCK
int
osend(struct thread *td, struct osend_args *uap)
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = __DECONST(void *, uap->buf);
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(td, uap->s, &msg, uap->flags));
}

int
osendmsg(struct thread *td, struct osendmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (struct omsghdr));
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
		return (error);
	msg.msg_iov = iov;
	msg.msg_flags = MSG_COMPAT;
	error = sendit(td, uap->s, &msg, uap->flags);
	free(iov, M_IOV);
	return (error);
}
#endif

int
sys_sendmsg(struct thread *td, struct sendmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (msg));
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
		return (error);
	msg.msg_iov = iov;
#ifdef COMPAT_OLDSOCK
	if (SV_PROC_FLAG(td->td_proc, SV_AOUT))
		msg.msg_flags = 0;
#endif
	error = sendit(td, uap->s, &msg, uap->flags);
	free(iov, M_IOV);
	return (error);
}

int
kern_recvit(struct thread *td, int s, struct msghdr *mp, enum uio_seg fromseg,
    struct mbuf **controlp)
{
	struct uio auio;
	struct iovec *iov;
	struct mbuf *control, *m;
	caddr_t ctlbuf;
	struct file *fp;
	struct socket *so;
	struct sockaddr *fromsa = NULL;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	ssize_t len;
	int error, i;

	if (controlp != NULL)
		*controlp = NULL;

	AUDIT_ARG_FD(s);
	error = getsock(td, s, &cap_recv_rights, &fp);
	if (error != 0)
		return (error);
	so = fp->f_data;

#ifdef MAC
	error = mac_socket_check_receive(td->td_ucred, so);
	if (error != 0) {
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
	control = NULL;
	len = auio.uio_resid;
	error = soreceive(so, &fromsa, &auio, NULL,
	    (mp->msg_control || controlp) ? &control : NULL,
	    &mp->msg_flags);
	if (error != 0) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	if (fromsa != NULL)
		AUDIT_ARG_SOCKADDR(td, AT_FDCWD, fromsa);
#ifdef KTRACE
	if (ktruio != NULL) {
		/* MSG_TRUNC can trigger underflow of uio_resid. */
		ktruio->uio_resid = MIN(len - auio.uio_resid, len);
		ktrgenio(s, UIO_READ, ktruio, error);
	}
#endif
	if (error != 0)
		goto out;
	td->td_retval[0] = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || fromsa == NULL)
			len = 0;
		else {
			/* save sa_len before it is destroyed by MSG_COMPAT */
			len = MIN(len, fromsa->sa_len);
#ifdef COMPAT_OLDSOCK
			if ((mp->msg_flags & MSG_COMPAT) != 0 &&
			    SV_PROC_FLAG(td->td_proc, SV_AOUT))
				((struct osockaddr *)fromsa)->sa_family =
				    fromsa->sa_family;
#endif
			if (fromseg == UIO_USERSPACE) {
				error = copyout(fromsa, mp->msg_name,
				    (unsigned)len);
				if (error != 0)
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
		if (control && (mp->msg_flags & MSG_COMPAT) != 0 &&
		    SV_PROC_FLAG(td->td_proc, SV_AOUT)) {
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
		ctlbuf = mp->msg_control;
		len = mp->msg_controllen;
		mp->msg_controllen = 0;
		for (m = control; m != NULL && len >= m->m_len; m = m->m_next) {
			if ((error = copyout(mtod(m, caddr_t), ctlbuf,
			    m->m_len)) != 0)
				goto out;

			ctlbuf += m->m_len;
			len -= m->m_len;
			mp->msg_controllen += m->m_len;
		}
		if (m != NULL) {
			mp->msg_flags |= MSG_CTRUNC;
			m_dispose_extcontrolm(m);
		}
	}
out:
	fdrop(fp, td);
#ifdef KTRACE
	if (fromsa && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(fromsa);
#endif
	free(fromsa, M_SONAME);

	if (error == 0 && controlp != NULL)
		*controlp = control;
	else if (control != NULL) {
		if (error != 0)
			m_dispose_extcontrolm(control);
		m_freem(control);
	}

	return (error);
}

static int
recvit(struct thread *td, int s, struct msghdr *mp, void *namelenp)
{
	int error;

	error = kern_recvit(td, s, mp, UIO_USERSPACE, NULL);
	if (error != 0)
		return (error);
	if (namelenp != NULL) {
		error = copyout(&mp->msg_namelen, namelenp, sizeof (socklen_t));
#ifdef COMPAT_OLDSOCK
		if ((mp->msg_flags & MSG_COMPAT) != 0 &&
		    SV_PROC_FLAG(td->td_proc, SV_AOUT))
			error = 0;	/* old recvfrom didn't check */
#endif
	}
	return (error);
}

static int
kern_recvfrom(struct thread *td, int s, void *buf, size_t len, int flags,
    struct sockaddr *from, socklen_t *fromlenaddr)
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (fromlenaddr != NULL) {
		error = copyin(fromlenaddr, &msg.msg_namelen,
		    sizeof (msg.msg_namelen));
		if (error != 0)
			goto done2;
	} else {
		msg.msg_namelen = 0;
	}
	msg.msg_name = from;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = buf;
	aiov.iov_len = len;
	msg.msg_control = 0;
	msg.msg_flags = flags;
	error = recvit(td, s, &msg, fromlenaddr);
done2:
	return (error);
}

int
sys_recvfrom(struct thread *td, struct recvfrom_args *uap)
{
	return (kern_recvfrom(td, uap->s, uap->buf, uap->len,
	    uap->flags, uap->from, uap->fromlenaddr));
}


#ifdef COMPAT_OLDSOCK
int
orecvfrom(struct thread *td, struct orecvfrom_args *uap)
{
	return (kern_recvfrom(td, uap->s, uap->buf, uap->len,
	    uap->flags | MSG_COMPAT, uap->from, uap->fromlenaddr));
}
#endif

#ifdef COMPAT_OLDSOCK
int
orecv(struct thread *td, struct orecv_args *uap)
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
	return (recvit(td, uap->s, &msg, NULL));
}

/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
int
orecvmsg(struct thread *td, struct orecvmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (struct omsghdr));
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
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
sys_recvmsg(struct thread *td, struct recvmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *uiov, *iov;
	int error;

	error = copyin(uap->msg, &msg, sizeof (msg));
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
		return (error);
	msg.msg_flags = uap->flags;
#ifdef COMPAT_OLDSOCK
	if (SV_PROC_FLAG(td->td_proc, SV_AOUT))
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

int
sys_shutdown(struct thread *td, struct shutdown_args *uap)
{

	return (kern_shutdown(td, uap->s, uap->how));
}

int
kern_shutdown(struct thread *td, int s, int how)
{
	struct socket *so;
	struct file *fp;
	int error;

	if (__predict_false(how < SHUT_RD || how > SHUT_RDWR))
		return (EINVAL);

	AUDIT_ARG_FD(s);
	error = getsock(td, s, &cap_shutdown_rights, &fp);
	if (error == 0) {
		so = fp->f_data;
		error = soshutdown(so, how);
		/*
		 * Previous versions did not return ENOTCONN, but 0 in
		 * case the socket was not connected. Some important
		 * programs like syslogd up to r279016, 2015-02-19,
		 * still depend on this behavior.
		 */
		if (error == ENOTCONN &&
		    td->td_proc->p_osrel < P_OSREL_SHUTDOWN_ENOTCONN)
			error = 0;
		fdrop(fp, td);
	}
	return (error);
}

int
sys_setsockopt(struct thread *td, struct setsockopt_args *uap)
{

	return (kern_setsockopt(td, uap->s, uap->level, uap->name,
	    uap->val, UIO_USERSPACE, uap->valsize));
}

int
kern_setsockopt(struct thread *td, int s, int level, int name, const void *val,
    enum uio_seg valseg, socklen_t valsize)
{
	struct socket *so;
	struct file *fp;
	struct filecaps fcaps;
	struct sockopt sopt;
	int error;

	if (val == NULL && valsize != 0)
		return (EFAULT);
	if ((int)valsize < 0)
		return (EINVAL);

	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = level;
	sopt.sopt_name = name;
	sopt.sopt_val = __DECONST(void *, val);
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
	error = getsock_cap(td, s, &cap_setsockopt_rights, &fp,
	    &fcaps);
	if (error == 0) {
		sopt.sopt_rights = &fcaps.fc_rights;
		so = fp->f_data;
		error = sosetopt(so, &sopt);
		fdrop(fp, td);
	}
	return(error);
}

int
sys_getsockopt(struct thread *td, struct getsockopt_args *uap)
{
	socklen_t valsize;
	int error;

	if (uap->val) {
		error = copyin(uap->avalsize, &valsize, sizeof (valsize));
		if (error != 0)
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
kern_getsockopt(struct thread *td, int s, int level, int name, void *val,
    enum uio_seg valseg, socklen_t *valsize)
{
	struct socket *so;
	struct file *fp;
	struct filecaps fcaps;
	struct sockopt sopt;
	int error;

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
	error = getsock_cap(td, s, &cap_getsockopt_rights, &fp, &fcaps);
	if (error == 0) {
		sopt.sopt_rights = &fcaps.fc_rights;
		so = fp->f_data;
		error = sogetopt(so, &sopt);
		*valsize = sopt.sopt_valsize;
		fdrop(fp, td);
	}
	return (error);
}

static int
user_getsockname(struct thread *td, int fdes, struct sockaddr *asa,
    socklen_t *alen, bool compat)
{
	struct sockaddr_storage ss = { .ss_len = sizeof(ss) };
	socklen_t len;
	int error;

	error = copyin(alen, &len, sizeof(len));
	if (error != 0)
		return (error);

	error = kern_getsockname(td, fdes, (struct sockaddr *)&ss);
	if (error != 0)
		return (error);

#ifdef COMPAT_OLDSOCK
	if (compat && SV_PROC_FLAG(td->td_proc, SV_AOUT))
		((struct osockaddr *)&ss)->sa_family = ss.ss_family;
#endif
	len = min(ss.ss_len, len);
	error = copyout(&ss, asa, len);
	if (error == 0) {
		len = ss.ss_len;
		error = copyout(&len, alen, sizeof(len));
	}
	return (error);
}

int
kern_getsockname(struct thread *td, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);
	error = getsock(td, fd, &cap_getsockname_rights, &fp);
	if (error != 0)
		return (error);
	so = fp->f_data;
	error = sosockaddr(so, sa);
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(sa);
#endif
	fdrop(fp, td);
	return (error);
}

int
sys_getsockname(struct thread *td, struct getsockname_args *uap)
{
	return (user_getsockname(td, uap->fdes, uap->asa, uap->alen, false));
}

#ifdef COMPAT_OLDSOCK
int
ogetsockname(struct thread *td, struct ogetsockname_args *uap)
{
	return (user_getsockname(td, uap->fdes, uap->asa, uap->alen, true));
}
#endif /* COMPAT_OLDSOCK */

static int
user_getpeername(struct thread *td, int fdes, struct sockaddr *asa,
    socklen_t *alen, bool compat)
{
	struct sockaddr_storage ss = { .ss_len = sizeof(ss) };
	socklen_t len;
	int error;

	error = copyin(alen, &len, sizeof (len));
	if (error != 0)
		return (error);

	error = kern_getpeername(td, fdes, (struct sockaddr *)&ss);
	if (error != 0)
		return (error);

#ifdef COMPAT_OLDSOCK
	if (compat && SV_PROC_FLAG(td->td_proc, SV_AOUT))
		((struct osockaddr *)&ss)->sa_family = ss.ss_family;
#endif
	len = min(ss.ss_len, len);
	error = copyout(&ss, asa, len);
	if (error == 0) {
		len = ss.ss_len;
		error = copyout(&len, alen, sizeof(len));
	}
	return (error);
}

int
kern_getpeername(struct thread *td, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);
	error = getsock(td, fd, &cap_getpeername_rights, &fp);
	if (error != 0)
		return (error);
	so = fp->f_data;
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto done;
	}
	CURVNET_SET(so->so_vnet);
	error = sopeeraddr(so, sa);
	CURVNET_RESTORE();
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(sa);
#endif
done:
	fdrop(fp, td);
	return (error);
}

int
sys_getpeername(struct thread *td, struct getpeername_args *uap)
{
	return (user_getpeername(td, uap->fdes, uap->asa, uap->alen, false));
}

#ifdef COMPAT_OLDSOCK
int
ogetpeername(struct thread *td, struct ogetpeername_args *uap)
{
	return (user_getpeername(td, uap->fdes, uap->asa, uap->alen, true));
}
#endif /* COMPAT_OLDSOCK */

static int
sockargs(struct mbuf **mp, char *buf, socklen_t buflen, int type)
{
	struct sockaddr *sa;
	struct mbuf *m;
	int error;

	if (buflen > MLEN) {
#ifdef COMPAT_OLDSOCK
		if (type == MT_SONAME && buflen <= 112 &&
		    SV_CURPROC_FLAG(SV_AOUT))
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
			if (buflen > MCLBYTES)
				return (EMSGSIZE);
	}
	m = m_get2(buflen, M_WAITOK, type, 0);
	m->m_len = buflen;
	error = copyin(buf, mtod(m, void *), buflen);
	if (error != 0)
		(void) m_free(m);
	else {
		*mp = m;
		if (type == MT_SONAME) {
			sa = mtod(m, struct sockaddr *);

#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
			if (sa->sa_family == 0 && sa->sa_len < AF_MAX &&
			    SV_CURPROC_FLAG(SV_AOUT))
				sa->sa_family = sa->sa_len;
#endif
			sa->sa_len = buflen;
		}
	}
	return (error);
}

int
getsockaddr(struct sockaddr **namp, const struct sockaddr *uaddr, size_t len)
{
	struct sockaddr *sa;
	int error;

	if (len > SOCK_MAXADDRLEN)
		return (ENAMETOOLONG);
	if (len < offsetof(struct sockaddr, sa_data[0]))
		return (EINVAL);
	sa = malloc(len, M_SONAME, M_WAITOK);
	error = copyin(uaddr, sa, len);
	if (error != 0) {
		free(sa, M_SONAME);
	} else {
#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX &&
		    SV_CURPROC_FLAG(SV_AOUT))
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = len;
		*namp = sa;
	}
	return (error);
}

/*
 * Dispose of externalized rights from an SCM_RIGHTS message.  This function
 * should be used in error or truncation cases to avoid leaking file descriptors
 * into the recipient's (the current thread's) table.
 */
void
m_dispose_extcontrolm(struct mbuf *m)
{
	struct cmsghdr *cm;
	struct file *fp;
	struct thread *td;
	socklen_t clen, datalen;
	int error, fd, *fds, nfd;

	td = curthread;
	for (; m != NULL; m = m->m_next) {
		if (m->m_type != MT_EXTCONTROL)
			continue;
		cm = mtod(m, struct cmsghdr *);
		clen = m->m_len;
		while (clen > 0) {
			if (clen < sizeof(*cm))
				panic("%s: truncated mbuf %p", __func__, m);
			datalen = CMSG_SPACE(cm->cmsg_len - CMSG_SPACE(0));
			if (clen < datalen)
				panic("%s: truncated mbuf %p", __func__, m);

			if (cm->cmsg_level == SOL_SOCKET &&
			    cm->cmsg_type == SCM_RIGHTS) {
				fds = (int *)CMSG_DATA(cm);
				nfd = (cm->cmsg_len - CMSG_SPACE(0)) /
				    sizeof(int);

				while (nfd-- > 0) {
					fd = *fds++;
					error = fget(td, fd, &cap_no_rights,
					    &fp);
					if (error == 0) {
						fdclose(td, fp, fd);
						fdrop(fp, td);
					}
				}
			}
			clen -= datalen;
			cm = (struct cmsghdr *)((uint8_t *)cm + datalen);
		}
		m_chtype(m, MT_CONTROL);
	}
}
