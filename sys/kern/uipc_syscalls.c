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

#include "opt_capsicum.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_sctp.h"
#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
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
#include <sys/jail.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/sf_buf.h>
#include <sys/sysent.h>
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
#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32_util.h>
#endif

#include <net/vnet.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#if defined(INET) || defined(INET6)
#ifdef SCTP
#include <netinet/sctp.h>
#include <netinet/sctp_peeloff.h>
#endif /* SCTP */
#endif /* INET || INET6 */

/*
 * Flags for accept1() and kern_accept4(), in addition to SOCK_CLOEXEC
 * and SOCK_NONBLOCK.
 */
#define	ACCEPT4_INHERIT	0x1
#define	ACCEPT4_COMPAT	0x2

static int sendit(struct thread *td, int s, struct msghdr *mp, int flags);
static int recvit(struct thread *td, int s, struct msghdr *mp, void *namelenp);

static int accept1(struct thread *td, int s, struct sockaddr *uname,
		   socklen_t *anamelen, int flags);
static int do_sendfile(struct thread *td, struct sendfile_args *uap,
		   int compat);
static int getsockname1(struct thread *td, struct getsockname_args *uap,
			int compat);
static int getpeername1(struct thread *td, struct getpeername_args *uap,
			int compat);

counter_u64_t sfstat[sizeof(struct sfstat) / sizeof(uint64_t)];

/*
 * sendfile(2)-related variables and associated sysctls
 */
static SYSCTL_NODE(_kern_ipc, OID_AUTO, sendfile, CTLFLAG_RW, 0,
    "sendfile(2) tunables");
static int sfreadahead = 1;
SYSCTL_INT(_kern_ipc_sendfile, OID_AUTO, readahead, CTLFLAG_RW,
    &sfreadahead, 0, "Number of sendfile(2) read-ahead MAXBSIZE blocks");


static void
sfstat_init(const void *unused)
{

	COUNTER_ARRAY_ALLOC(sfstat, sizeof(struct sfstat) / sizeof(uint64_t),
	    M_WAITOK);
}
SYSINIT(sfstat, SI_SUB_MBUF, SI_ORDER_FIRST, sfstat_init, NULL);

static int
sfstat_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct sfstat s;

	COUNTER_ARRAY_COPY(sfstat, &s, sizeof(s) / sizeof(uint64_t));
	if (req->newptr)
		COUNTER_ARRAY_ZERO(sfstat, sizeof(s) / sizeof(uint64_t));
	return (SYSCTL_OUT(req, &s, sizeof(s)));
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, sfstat, CTLTYPE_OPAQUE | CTLFLAG_RW,
    NULL, 0, sfstat_sysctl, "I", "sendfile statistics");

/*
 * Convert a user file descriptor to a kernel file entry and check if required
 * capability rights are present.
 * A reference on the file entry is held upon returning.
 */
static int
getsock_cap(struct filedesc *fdp, int fd, cap_rights_t *rightsp,
    struct file **fpp, u_int *fflagp)
{
	struct file *fp;
	int error;

	error = fget_unlocked(fdp, fd, rightsp, 0, &fp, NULL);
	if (error != 0)
		return (error);
	if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		return (ENOTSOCK);
	}
	if (fflagp != NULL)
		*fflagp = fp->f_flag;
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
sys_socket(td, uap)
	struct thread *td;
	struct socket_args /* {
		int	domain;
		int	type;
		int	protocol;
	} */ *uap;
{
	struct socket *so;
	struct file *fp;
	int fd, error, type, oflag, fflag;

	AUDIT_ARG_SOCKET(uap->domain, uap->type, uap->protocol);

	type = uap->type;
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
	error = mac_socket_check_create(td->td_ucred, uap->domain, type,
	    uap->protocol);
	if (error != 0)
		return (error);
#endif
	error = falloc(td, &fp, &fd, oflag);
	if (error != 0)
		return (error);
	/* An extra reference on `fp' has been held for us by falloc(). */
	error = socreate(uap->domain, &so, type, uap->protocol,
	    td->td_ucred, td);
	if (error != 0) {
		fdclose(td->td_proc->p_fd, fp, fd, td);
	} else {
		finit(fp, FREAD | FWRITE | fflag, DTYPE_SOCKET, so, &socketops);
		if ((fflag & FNONBLOCK) != 0)
			(void) fo_ioctl(fp, FIONBIO, &fflag, td->td_ucred, td);
		td->td_retval[0] = fd;
	}
	fdrop(fp, td);
	return (error);
}

/* ARGSUSED */
int
sys_bind(td, uap)
	struct thread *td;
	struct bind_args /* {
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
{
	struct sockaddr *sa;
	int error;

	error = getsockaddr(&sa, uap->name, uap->namelen);
	if (error == 0) {
		error = kern_bind(td, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

static int
kern_bindat(struct thread *td, int dirfd, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	cap_rights_t rights;
	int error;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_SOCKADDR(td, dirfd, sa);
	error = getsock_cap(td->td_proc->p_fd, fd,
	    cap_rights_init(&rights, CAP_BIND), &fp, NULL);
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
kern_bind(struct thread *td, int fd, struct sockaddr *sa)
{

	return (kern_bindat(td, AT_FDCWD, fd, sa));
}

/* ARGSUSED */
int
sys_bindat(td, uap)
	struct thread *td;
	struct bindat_args /* {
		int	fd;
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
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

/* ARGSUSED */
int
sys_listen(td, uap)
	struct thread *td;
	struct listen_args /* {
		int	s;
		int	backlog;
	} */ *uap;
{
	struct socket *so;
	struct file *fp;
	cap_rights_t rights;
	int error;

	AUDIT_ARG_FD(uap->s);
	error = getsock_cap(td->td_proc->p_fd, uap->s,
	    cap_rights_init(&rights, CAP_LISTEN), &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
#ifdef MAC
		error = mac_socket_check_listen(td->td_ucred, so);
		if (error == 0)
#endif
			error = solisten(so, uap->backlog, td);
		fdrop(fp, td);
	}
	return(error);
}

/*
 * accept1()
 */
static int
accept1(td, s, uname, anamelen, flags)
	struct thread *td;
	int s;
	struct sockaddr *uname;
	socklen_t *anamelen;
	int flags;
{
	struct sockaddr *name;
	socklen_t namelen;
	struct file *fp;
	int error;

	if (uname == NULL)
		return (kern_accept4(td, s, NULL, NULL, flags, NULL));

	error = copyin(anamelen, &namelen, sizeof (namelen));
	if (error != 0)
		return (error);

	error = kern_accept4(td, s, &name, &namelen, flags, &fp);

	/*
	 * return a namelen of zero for older code which might
	 * ignore the return value from accept.
	 */
	if (error != 0) {
		(void) copyout(&namelen, anamelen, sizeof(*anamelen));
		return (error);
	}

	if (error == 0 && uname != NULL) {
#ifdef COMPAT_OLDSOCK
		if (flags & ACCEPT4_COMPAT)
			((struct osockaddr *)name)->sa_family =
			    name->sa_family;
#endif
		error = copyout(name, uname, namelen);
	}
	if (error == 0)
		error = copyout(&namelen, anamelen,
		    sizeof(namelen));
	if (error != 0)
		fdclose(td->td_proc->p_fd, fp, td->td_retval[0], td);
	fdrop(fp, td);
	free(name, M_SONAME);
	return (error);
}

int
kern_accept(struct thread *td, int s, struct sockaddr **name,
    socklen_t *namelen, struct file **fp)
{
	return (kern_accept4(td, s, name, namelen, ACCEPT4_INHERIT, fp));
}

int
kern_accept4(struct thread *td, int s, struct sockaddr **name,
    socklen_t *namelen, int flags, struct file **fp)
{
	struct filedesc *fdp;
	struct file *headfp, *nfp = NULL;
	struct sockaddr *sa = NULL;
	struct socket *head, *so;
	cap_rights_t rights;
	u_int fflag;
	pid_t pgid;
	int error, fd, tmp;

	if (name != NULL)
		*name = NULL;

	AUDIT_ARG_FD(s);
	fdp = td->td_proc->p_fd;
	error = getsock_cap(fdp, s, cap_rights_init(&rights, CAP_ACCEPT),
	    &headfp, &fflag);
	if (error != 0)
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
	error = falloc(td, &nfp, &fd, (flags & SOCK_CLOEXEC) ? O_CLOEXEC : 0);
	if (error != 0)
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
		if (error != 0) {
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
	if (flags & ACCEPT4_INHERIT)
		so->so_state |= (head->so_state & SS_NBIO);
	else
		so->so_state |= (flags & SOCK_NONBLOCK) ? SS_NBIO : 0;
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;

	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();

	/* An extra reference on `nfp' has been held for us by falloc(). */
	td->td_retval[0] = fd;

	/* connection has been removed from the listen queue */
	KNOTE_UNLOCKED(&head->so_rcv.sb_sel.si_note, 0);

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
	sa = 0;
	error = soaccept(so, &sa);
	if (error != 0) {
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
	AUDIT_ARG_SOCKADDR(td, AT_FDCWD, sa);
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
	free(sa, M_SONAME);

	/*
	 * close the new descriptor, assuming someone hasn't ripped it
	 * out from under us.
	 */
	if (error != 0)
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
sys_accept(td, uap)
	struct thread *td;
	struct accept_args *uap;
{

	return (accept1(td, uap->s, uap->name, uap->anamelen, ACCEPT4_INHERIT));
}

int
sys_accept4(td, uap)
	struct thread *td;
	struct accept4_args *uap;
{

	if (uap->flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return (EINVAL);

	return (accept1(td, uap->s, uap->name, uap->anamelen, uap->flags));
}

#ifdef COMPAT_OLDSOCK
int
oaccept(td, uap)
	struct thread *td;
	struct accept_args *uap;
{

	return (accept1(td, uap->s, uap->name, uap->anamelen,
	    ACCEPT4_INHERIT | ACCEPT4_COMPAT));
}
#endif /* COMPAT_OLDSOCK */

/* ARGSUSED */
int
sys_connect(td, uap)
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
	if (error == 0) {
		error = kern_connect(td, uap->s, sa);
		free(sa, M_SONAME);
	}
	return (error);
}

static int
kern_connectat(struct thread *td, int dirfd, int fd, struct sockaddr *sa)
{
	struct socket *so;
	struct file *fp;
	cap_rights_t rights;
	int error, interrupted = 0;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_SOCKADDR(td, dirfd, sa);
	error = getsock_cap(td->td_proc->p_fd, fd,
	    cap_rights_init(&rights, CAP_CONNECT), &fp, NULL);
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
	if (dirfd == AT_FDCWD)
		error = soconnect(so, sa, td);
	else
		error = soconnectat(dirfd, so, sa, td);
	if (error != 0)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		error = EINPROGRESS;
		goto done1;
	}
	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = msleep(&so->so_timeo, SOCK_MTX(so), PSOCK | PCATCH,
		    "connec", 0);
		if (error != 0) {
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
kern_connect(struct thread *td, int fd, struct sockaddr *sa)
{

	return (kern_connectat(td, AT_FDCWD, fd, sa));
}

/* ARGSUSED */
int
sys_connectat(td, uap)
	struct thread *td;
	struct connectat_args /* {
		int	fd;
		int	s;
		caddr_t	name;
		int	namelen;
	} */ *uap;
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
	struct filedesc *fdp = td->td_proc->p_fd;
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
sendit(td, s, mp, flags)
	struct thread *td;
	int s;
	struct msghdr *mp;
	int flags;
{
	struct mbuf *control;
	struct sockaddr *to;
	int error;

#ifdef CAPABILITY_MODE
	if (IN_CAPABILITY_MODE(td) && (mp->msg_name != NULL))
		return (ECAPMODE);
#endif

	if (mp->msg_name != NULL) {
		error = getsockaddr(&to, mp->msg_name, mp->msg_namelen);
		if (error != 0) {
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
		if (error != 0)
			goto bad;
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags == MSG_COMPAT) {
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
	cap_rights_t rights;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	ssize_t len;
	int i, error;

	AUDIT_ARG_FD(s);
	cap_rights_init(&rights, CAP_SEND);
	if (mp->msg_name != NULL) {
		AUDIT_ARG_SOCKADDR(td, AT_FDCWD, mp->msg_name);
		cap_rights_set(&rights, CAP_CONNECT);
	}
	error = getsock_cap(td->td_proc->p_fd, s, &rights, &fp, NULL);
	if (error != 0)
		return (error);
	so = (struct socket *)fp->f_data;

#ifdef KTRACE
	if (mp->msg_name != NULL && KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(mp->msg_name);
#endif
#ifdef MAC
	if (mp->msg_name != NULL) {
		error = mac_socket_check_connect(td->td_ucred, so,
		    mp->msg_name);
		if (error != 0)
			goto bad;
	}
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0)
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
	if (error != 0) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			tdsignal(td, SIGPIPE);
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
sys_sendto(td, uap)
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
	return (sendit(td, uap->s, &msg, uap->flags));
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

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(td, uap->s, &msg, uap->flags));
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
sys_sendmsg(td, uap)
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
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
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
	struct mbuf *m, *control = NULL;
	caddr_t ctlbuf;
	struct file *fp;
	struct socket *so;
	struct sockaddr *fromsa = NULL;
	cap_rights_t rights;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	ssize_t len;
	int error, i;

	if (controlp != NULL)
		*controlp = NULL;

	AUDIT_ARG_FD(s);
	error = getsock_cap(td->td_proc->p_fd, s,
	    cap_rights_init(&rights, CAP_RECV), &fp, NULL);
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
		ktruio->uio_resid = len - auio.uio_resid;
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
			if (mp->msg_flags & MSG_COMPAT)
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
	if (error != 0)
		return (error);
	if (namelenp != NULL) {
		error = copyout(&mp->msg_namelen, namelenp, sizeof (socklen_t));
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags & MSG_COMPAT)
			error = 0;	/* old recvfrom didn't check */
#endif
	}
	return (error);
}

int
sys_recvfrom(td, uap)
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
		if (error != 0)
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
	return (error);
}

#ifdef COMPAT_OLDSOCK
int
orecvfrom(td, uap)
	struct thread *td;
	struct recvfrom_args *uap;
{

	uap->flags |= MSG_COMPAT;
	return (sys_recvfrom(td, uap));
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
sys_recvmsg(td, uap)
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
	if (error != 0)
		return (error);
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error != 0)
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
sys_shutdown(td, uap)
	struct thread *td;
	struct shutdown_args /* {
		int	s;
		int	how;
	} */ *uap;
{
	struct socket *so;
	struct file *fp;
	cap_rights_t rights;
	int error;

	AUDIT_ARG_FD(uap->s);
	error = getsock_cap(td->td_proc->p_fd, uap->s,
	    cap_rights_init(&rights, CAP_SHUTDOWN), &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		error = soshutdown(so, uap->how);
		fdrop(fp, td);
	}
	return (error);
}

/* ARGSUSED */
int
sys_setsockopt(td, uap)
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
	struct socket *so;
	struct file *fp;
	struct sockopt sopt;
	cap_rights_t rights;
	int error;

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
	error = getsock_cap(td->td_proc->p_fd, s,
	    cap_rights_init(&rights, CAP_SETSOCKOPT), &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		error = sosetopt(so, &sopt);
		fdrop(fp, td);
	}
	return(error);
}

/* ARGSUSED */
int
sys_getsockopt(td, uap)
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
kern_getsockopt(td, s, level, name, val, valseg, valsize)
	struct thread *td;
	int s;
	int level;
	int name;
	void *val;
	enum uio_seg valseg;
	socklen_t *valsize;
{
	struct socket *so;
	struct file *fp;
	struct sockopt sopt;
	cap_rights_t rights;
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
	error = getsock_cap(td->td_proc->p_fd, s,
	    cap_rights_init(&rights, CAP_GETSOCKOPT), &fp, NULL);
	if (error == 0) {
		so = fp->f_data;
		error = sogetopt(so, &sopt);
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
	if (error != 0)
		return (error);

	error = kern_getsockname(td, uap->fdes, &sa, &len);
	if (error != 0)
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
	cap_rights_t rights;
	socklen_t len;
	int error;

	AUDIT_ARG_FD(fd);
	error = getsock_cap(td->td_proc->p_fd, fd,
	    cap_rights_init(&rights, CAP_GETSOCKNAME), &fp, NULL);
	if (error != 0)
		return (error);
	so = fp->f_data;
	*sa = NULL;
	CURVNET_SET(so->so_vnet);
	error = (*so->so_proto->pr_usrreqs->pru_sockaddr)(so, sa);
	CURVNET_RESTORE();
	if (error != 0)
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
	if (error != 0 && *sa != NULL) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}
	return (error);
}

int
sys_getsockname(td, uap)
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
	if (error != 0)
		return (error);

	error = kern_getpeername(td, uap->fdes, &sa, &len);
	if (error != 0)
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
	cap_rights_t rights;
	socklen_t len;
	int error;

	AUDIT_ARG_FD(fd);
	error = getsock_cap(td->td_proc->p_fd, fd,
	    cap_rights_init(&rights, CAP_GETPEERNAME), &fp, NULL);
	if (error != 0)
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
	if (error != 0)
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
	if (error != 0 && *sa != NULL) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}
done:
	fdrop(fp, td);
	return (error);
}

int
sys_getpeername(td, uap)
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

	if (buflen < 0)
		return (EINVAL);

	if (buflen > MLEN) {
#ifdef COMPAT_OLDSOCK
		if (type == MT_SONAME && buflen <= 112)
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
			if (buflen > MCLBYTES)
				return (EINVAL);
	}
	m = m_get2(buflen, M_WAITOK, type, 0);
	m->m_len = buflen;
	error = copyin(buf, mtod(m, caddr_t), (u_int)buflen);
	if (error != 0)
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
	if (error != 0) {
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

struct sendfile_sync {
	struct mtx	mtx;
	struct cv	cv;
	unsigned	count;
};

/*
 * Detach mapped page and release resources back to the system.
 */
int
sf_buf_mext(struct mbuf *mb, void *addr, void *args)
{
	vm_page_t m;
	struct sendfile_sync *sfs;

	m = sf_buf_page(args);
	sf_buf_free(args);
	vm_page_lock(m);
	vm_page_unwire(m, 0);
	/*
	 * Check for the object going away on us. This can
	 * happen since we don't hold a reference to it.
	 * If so, we're responsible for freeing the page.
	 */
	if (m->wire_count == 0 && m->object == NULL)
		vm_page_free(m);
	vm_page_unlock(m);
	if (addr == NULL)
		return (EXT_FREE_OK);
	sfs = addr;
	mtx_lock(&sfs->mtx);
	KASSERT(sfs->count> 0, ("Sendfile sync botchup count == 0"));
	if (--sfs->count == 0)
		cv_signal(&sfs->cv);
	mtx_unlock(&sfs->mtx);
	return (EXT_FREE_OK);
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
sys_sendfile(struct thread *td, struct sendfile_args *uap)
{

	return (do_sendfile(td, uap, 0));
}

static int
do_sendfile(struct thread *td, struct sendfile_args *uap, int compat)
{
	struct sf_hdtr hdtr;
	struct uio *hdr_uio, *trl_uio;
	struct file *fp;
	cap_rights_t rights;
	int error;

	/*
	 * File offset must be positive.  If it goes beyond EOF
	 * we send only the header/trailer and no payload data.
	 */
	if (uap->offset < 0)
		return (EINVAL);

	hdr_uio = trl_uio = NULL;

	if (uap->hdtr != NULL) {
		error = copyin(uap->hdtr, &hdtr, sizeof(hdtr));
		if (error != 0)
			goto out;
		if (hdtr.headers != NULL) {
			error = copyinuio(hdtr.headers, hdtr.hdr_cnt, &hdr_uio);
			if (error != 0)
				goto out;
		}
		if (hdtr.trailers != NULL) {
			error = copyinuio(hdtr.trailers, hdtr.trl_cnt, &trl_uio);
			if (error != 0)
				goto out;

		}
	}

	AUDIT_ARG_FD(uap->fd);

	/*
	 * sendfile(2) can start at any offset within a file so we require
	 * CAP_READ+CAP_SEEK = CAP_PREAD.
	 */
	if ((error = fget_read(td, uap->fd,
	    cap_rights_init(&rights, CAP_PREAD), &fp)) != 0) {
		goto out;
	}

	error = fo_sendfile(fp, uap->s, hdr_uio, trl_uio, uap->offset,
	    uap->nbytes, uap->sbytes, uap->flags, compat ? SFK_COMPAT : 0, td);
	fdrop(fp, td);

out:
	free(hdr_uio, M_IOV);
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

static int
sendfile_readpage(vm_object_t obj, struct vnode *vp, int nd,
    off_t off, int xfsize, int bsize, struct thread *td, vm_page_t *res)
{
	vm_page_t m;
	vm_pindex_t pindex;
	ssize_t resid;
	int error, readahead, rv;

	pindex = OFF_TO_IDX(off);
	VM_OBJECT_WLOCK(obj);
	m = vm_page_grab(obj, pindex, (vp != NULL ? VM_ALLOC_NOBUSY |
	    VM_ALLOC_IGN_SBUSY : 0) | VM_ALLOC_WIRED | VM_ALLOC_NORMAL);

	/*
	 * Check if page is valid for what we need, otherwise initiate I/O.
	 *
	 * The non-zero nd argument prevents disk I/O, instead we
	 * return the caller what he specified in nd.  In particular,
	 * if we already turned some pages into mbufs, nd == EAGAIN
	 * and the main function send them the pages before we come
	 * here again and block.
	 */
	if (m->valid != 0 && vm_page_is_valid(m, off & PAGE_MASK, xfsize)) {
		if (vp == NULL)
			vm_page_xunbusy(m);
		VM_OBJECT_WUNLOCK(obj);
		*res = m;
		return (0);
	} else if (nd != 0) {
		if (vp == NULL)
			vm_page_xunbusy(m);
		error = nd;
		goto free_page;
	}

	/*
	 * Get the page from backing store.
	 */
	error = 0;
	if (vp != NULL) {
		VM_OBJECT_WUNLOCK(obj);
		readahead = sfreadahead * MAXBSIZE;

		/*
		 * Use vn_rdwr() instead of the pager interface for
		 * the vnode, to allow the read-ahead.
		 *
		 * XXXMAC: Because we don't have fp->f_cred here, we
		 * pass in NOCRED.  This is probably wrong, but is
		 * consistent with our original implementation.
		 */
		error = vn_rdwr(UIO_READ, vp, NULL, readahead, trunc_page(off),
		    UIO_NOCOPY, IO_NODELOCKED | IO_VMIO | ((readahead /
		    bsize) << IO_SEQSHIFT), td->td_ucred, NOCRED, &resid, td);
		SFSTAT_INC(sf_iocnt);
		VM_OBJECT_WLOCK(obj);
	} else {
		if (vm_pager_has_page(obj, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(obj, &m, 1, 0);
			SFSTAT_INC(sf_iocnt);
			m = vm_page_lookup(obj, pindex);
			if (m == NULL)
				error = EIO;
			else if (rv != VM_PAGER_OK) {
				vm_page_lock(m);
				vm_page_free(m);
				vm_page_unlock(m);
				m = NULL;
				error = EIO;
			}
		} else {
			pmap_zero_page(m);
			m->valid = VM_PAGE_BITS_ALL;
			m->dirty = 0;
		}
		if (m != NULL)
			vm_page_xunbusy(m);
	}
	if (error == 0) {
		*res = m;
	} else if (m != NULL) {
free_page:
		vm_page_lock(m);
		vm_page_unwire(m, 0);

		/*
		 * See if anyone else might know about this page.  If
		 * not and it is not valid, then free it.
		 */
		if (m->wire_count == 0 && m->valid == 0 && !vm_page_busied(m))
			vm_page_free(m);
		vm_page_unlock(m);
	}
	KASSERT(error != 0 || (m->wire_count > 0 &&
	    vm_page_is_valid(m, off & PAGE_MASK, xfsize)),
	    ("wrong page state m %p off %#jx xfsize %d", m, (uintmax_t)off,
	    xfsize));
	VM_OBJECT_WUNLOCK(obj);
	return (error);
}

static int
sendfile_getobj(struct thread *td, struct file *fp, vm_object_t *obj_res,
    struct vnode **vp_res, struct shmfd **shmfd_res, off_t *obj_size,
    int *bsize)
{
	struct vattr va;
	vm_object_t obj;
	struct vnode *vp;
	struct shmfd *shmfd;
	int error;

	vp = *vp_res = NULL;
	obj = NULL;
	shmfd = *shmfd_res = NULL;
	*bsize = 0;

	/*
	 * The file descriptor must be a regular file and have a
	 * backing VM object.
	 */
	if (fp->f_type == DTYPE_VNODE) {
		vp = fp->f_vnode;
		vn_lock(vp, LK_SHARED | LK_RETRY);
		if (vp->v_type != VREG) {
			error = EINVAL;
			goto out;
		}
		*bsize = vp->v_mount->mnt_stat.f_iosize;
		error = VOP_GETATTR(vp, &va, td->td_ucred);
		if (error != 0)
			goto out;
		*obj_size = va.va_size;
		obj = vp->v_object;
		if (obj == NULL) {
			error = EINVAL;
			goto out;
		}
	} else if (fp->f_type == DTYPE_SHM) {
		shmfd = fp->f_data;
		obj = shmfd->shm_object;
		*obj_size = shmfd->shm_size;
	} else {
		error = EINVAL;
		goto out;
	}

	VM_OBJECT_WLOCK(obj);
	if ((obj->flags & OBJ_DEAD) != 0) {
		VM_OBJECT_WUNLOCK(obj);
		error = EBADF;
		goto out;
	}

	/*
	 * Temporarily increase the backing VM object's reference
	 * count so that a forced reclamation of its vnode does not
	 * immediately destroy it.
	 */
	vm_object_reference_locked(obj);
	VM_OBJECT_WUNLOCK(obj);
	*obj_res = obj;
	*vp_res = vp;
	*shmfd_res = shmfd;

out:
	if (vp != NULL)
		VOP_UNLOCK(vp, 0);
	return (error);
}

static int
kern_sendfile_getsock(struct thread *td, int s, struct file **sock_fp,
    struct socket **so)
{
	cap_rights_t rights;
	int error;

	*sock_fp = NULL;
	*so = NULL;

	/*
	 * The socket must be a stream socket and connected.
	 */
	error = getsock_cap(td->td_proc->p_fd, s, cap_rights_init(&rights,
	    CAP_SEND), sock_fp, NULL);
	if (error != 0)
		return (error);
	*so = (*sock_fp)->f_data;
	if ((*so)->so_type != SOCK_STREAM)
		return (EINVAL);
	if (((*so)->so_state & SS_ISCONNECTED) == 0)
		return (ENOTCONN);
	return (0);
}

int
vn_sendfile(struct file *fp, int sockfd, struct uio *hdr_uio,
    struct uio *trl_uio, off_t offset, size_t nbytes, off_t *sent, int flags,
    int kflags, struct thread *td)
{
	struct file *sock_fp;
	struct vnode *vp;
	struct vm_object *obj;
	struct socket *so;
	struct mbuf *m;
	struct sf_buf *sf;
	struct vm_page *pg;
	struct shmfd *shmfd;
	struct sendfile_sync *sfs;
	struct vattr va;
	off_t off, xfsize, fsbytes, sbytes, rem, obj_size;
	int error, bsize, nd, hdrlen, mnw;
	bool inflight_called;

	pg = NULL;
	obj = NULL;
	so = NULL;
	m = NULL;
	sfs = NULL;
	fsbytes = sbytes = 0;
	hdrlen = mnw = 0;
	rem = nbytes;
	obj_size = 0;
	inflight_called = false;

	error = sendfile_getobj(td, fp, &obj, &vp, &shmfd, &obj_size, &bsize);
	if (error != 0)
		return (error);
	if (rem == 0)
		rem = obj_size;

	error = kern_sendfile_getsock(td, sockfd, &sock_fp, &so);
	if (error != 0)
		goto out;

	/*
	 * Do not wait on memory allocations but return ENOMEM for
	 * caller to retry later.
	 * XXX: Experimental.
	 */
	if (flags & SF_MNOWAIT)
		mnw = 1;

	if (flags & SF_SYNC) {
		sfs = malloc(sizeof *sfs, M_TEMP, M_WAITOK | M_ZERO);
		mtx_init(&sfs->mtx, "sendfile", NULL, MTX_DEF);
		cv_init(&sfs->cv, "sendfile");
	}

#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0)
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
			if (kflags & SFK_COMPAT) {
				if (nbytes > hdr_uio->uio_resid)
					nbytes -= hdr_uio->uio_resid;
				else
					nbytes = 0;
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
	for (off = offset; ; ) {
		struct mbuf *mtail;
		int loopbytes;
		int space;
		int done;

		if ((nbytes != 0 && nbytes == fsbytes) ||
		    (nbytes == 0 && obj_size == fsbytes))
			break;

		mtail = NULL;
		loopbytes = 0;
		space = 0;
		done = 0;

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
			if (error != 0) {
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

		if (vp != NULL) {
			error = vn_lock(vp, LK_SHARED);
			if (error != 0)
				goto done;
			error = VOP_GETATTR(vp, &va, td->td_ucred);
			if (error != 0 || off >= va.va_size) {
				VOP_UNLOCK(vp, 0);
				goto done;
			}
			obj_size = va.va_size;
		}

		/*
		 * Loop and construct maximum sized mbuf chain to be bulk
		 * dumped into socket buffer.
		 */
		while (space > loopbytes) {
			vm_offset_t pgoff;
			struct mbuf *m0;

			/*
			 * Calculate the amount to transfer.
			 * Not to exceed a page, the EOF,
			 * or the passed in nbytes.
			 */
			pgoff = (vm_offset_t)(off & PAGE_MASK);
			rem = obj_size - offset;
			if (nbytes != 0)
				rem = omin(rem, nbytes);
			rem -= fsbytes + loopbytes;
			xfsize = omin(PAGE_SIZE - pgoff, rem);
			xfsize = omin(space - loopbytes, xfsize);
			if (xfsize <= 0) {
				done = 1;		/* all data sent */
				break;
			}

			/*
			 * Attempt to look up the page.  Allocate
			 * if not found or wait and loop if busy.
			 */
			if (m != NULL)
				nd = EAGAIN; /* send what we already got */
			else if ((flags & SF_NODISKIO) != 0)
				nd = EBUSY;
			else
				nd = 0;
			error = sendfile_readpage(obj, vp, nd, off,
			    xfsize, bsize, td, &pg);
			if (error != 0) {
				if (error == EAGAIN)
					error = 0;	/* not a real error */
				break;
			}

			/*
			 * Get a sendfile buf.  When allocating the
			 * first buffer for mbuf chain, we usually
			 * wait as long as necessary, but this wait
			 * can be interrupted.  For consequent
			 * buffers, do not sleep, since several
			 * threads might exhaust the buffers and then
			 * deadlock.
			 */
			sf = sf_buf_alloc(pg, (mnw || m != NULL) ? SFB_NOWAIT :
			    SFB_CATCH);
			if (sf == NULL) {
				SFSTAT_INC(sf_allocfail);
				vm_page_lock(pg);
				vm_page_unwire(pg, 0);
				KASSERT(pg->object != NULL,
				    ("%s: object disappeared", __func__));
				vm_page_unlock(pg);
				if (m == NULL)
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
				(void)sf_buf_mext(NULL, NULL, sf);
				break;
			}
			if (m_extadd(m0, (caddr_t )sf_buf_kva(sf), PAGE_SIZE,
			    sf_buf_mext, sfs, sf, M_RDONLY, EXT_SFBUF,
			    (mnw ? M_NOWAIT : M_WAITOK)) != 0) {
				error = (mnw ? EAGAIN : ENOBUFS);
				(void)sf_buf_mext(NULL, NULL, sf);
				m_freem(m0);
				break;
			}
			m0->m_data = (char *)sf_buf_kva(sf) + pgoff;
			m0->m_len = xfsize;

			/* Append to mbuf chain. */
			if (mtail != NULL)
				mtail->m_next = m0;
			else if (m != NULL)
				m_last(m)->m_next = m0;
			else
				m = m0;
			mtail = m0;

			/* Keep track of bits processed. */
			loopbytes += xfsize;
			off += xfsize;

			if (sfs != NULL) {
				mtx_lock(&sfs->mtx);
				sfs->count++;
				mtx_unlock(&sfs->mtx);
			}
		}

		if (vp != NULL)
			VOP_UNLOCK(vp, 0);

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
		if (error != 0)
			goto done;
	}

	/*
	 * Send trailers. Wimp out and use writev(2).
	 */
	if (trl_uio != NULL) {
		sbunlock(&so->so_snd);
		error = kern_writev(td, sockfd, trl_uio);
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
	if (sent != NULL) {
		copyout(&sbytes, sent, sizeof(off_t));
	}
	if (obj != NULL)
		vm_object_deallocate(obj);
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
sys_sctp_peeloff(td, uap)
	struct thread *td;
	struct sctp_peeloff_args /* {
		int	sd;
		caddr_t	name;
	} */ *uap;
{
#if (defined(INET) || defined(INET6)) && defined(SCTP)
	struct file *nfp = NULL;
	struct socket *head, *so;
	cap_rights_t rights;
	u_int fflag;
	int error, fd;

	AUDIT_ARG_FD(uap->sd);
	error = fgetsock(td, uap->sd, cap_rights_init(&rights, CAP_PEELOFF),
	    &head, &fflag);
	if (error != 0)
		goto done2;
	if (head->so_proto->pr_protocol != IPPROTO_SCTP) {
		error = EOPNOTSUPP;
		goto done;
	}
	error = sctp_can_peel_off(head, (sctp_assoc_t)uap->name);
	if (error != 0)
		goto done;
	/*
	 * At this point we know we do have a assoc to pull
	 * we proceed to get the fd setup. This may block
	 * but that is ok.
	 */

	error = falloc(td, &nfp, &fd, 0);
	if (error != 0)
		goto done;
	td->td_retval[0] = fd;

	CURVNET_SET(head->so_vnet);
	so = sonewconn(head, SS_ISCONNECTED);
	if (so == NULL) {
		error = ENOMEM;
		goto noconnection;
	}
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
	if (error != 0)
		goto noconnection;
	if (head->so_sigio != NULL)
		fsetown(fgetown(&head->so_sigio), &so->so_sigio);

noconnection:
	/*
	 * close the new descriptor, assuming someone hasn't ripped it
	 * out from under us.
	 */
	if (error != 0)
		fdclose(td->td_proc->p_fd, nfp, fd, td);

	/*
	 * Release explicitly held references before returning.
	 */
	CURVNET_RESTORE();
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
sys_sctp_generic_sendmsg (td, uap)
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
	struct sockaddr *to = NULL;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	struct uio auio;
	struct iovec iov[1];
	cap_rights_t rights;
	int error = 0, len;

	if (uap->sinfo != NULL) {
		error = copyin(uap->sinfo, &sinfo, sizeof (sinfo));
		if (error != 0)
			return (error);
		u_sinfo = &sinfo;
	}

	cap_rights_init(&rights, CAP_SEND);
	if (uap->tolen != 0) {
		error = getsockaddr(&to, uap->to, uap->tolen);
		if (error != 0) {
			to = NULL;
			goto sctp_bad2;
		}
		cap_rights_set(&rights, CAP_CONNECT);
	}

	AUDIT_ARG_FD(uap->sd);
	error = getsock_cap(td->td_proc->p_fd, uap->sd, &rights, &fp, NULL);
	if (error != 0)
		goto sctp_bad;
#ifdef KTRACE
	if (to && (KTRPOINT(td, KTR_STRUCT)))
		ktrsockaddr(to);
#endif

	iov[0].iov_base = uap->msg;
	iov[0].iov_len = uap->mlen;

	so = (struct socket *)fp->f_data;
	if (so->so_proto->pr_protocol != IPPROTO_SCTP) {
		error = EOPNOTSUPP;
		goto sctp_bad;
	}
#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0)
		goto sctp_bad;
#endif /* MAC */

	auio.uio_iov =  iov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif /* KTRACE */
	len = auio.uio_resid = uap->mlen;
	CURVNET_SET(so->so_vnet);
	error = sctp_lower_sosend(so, to, &auio, (struct mbuf *)NULL,
	    (struct mbuf *)NULL, uap->flags, u_sinfo, td);
	CURVNET_RESTORE();
	if (error != 0) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket. */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(uap->flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			tdsignal(td, SIGPIPE);
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
	if (fp != NULL)
		fdrop(fp, td);
sctp_bad2:
	free(to, M_SONAME);
	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}

int
sys_sctp_generic_sendmsg_iov(td, uap)
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
	struct sockaddr *to = NULL;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	struct uio auio;
	struct iovec *iov, *tiov;
	cap_rights_t rights;
	ssize_t len;
	int error, i;

	if (uap->sinfo != NULL) {
		error = copyin(uap->sinfo, &sinfo, sizeof (sinfo));
		if (error != 0)
			return (error);
		u_sinfo = &sinfo;
	}
	cap_rights_init(&rights, CAP_SEND);
	if (uap->tolen != 0) {
		error = getsockaddr(&to, uap->to, uap->tolen);
		if (error != 0) {
			to = NULL;
			goto sctp_bad2;
		}
		cap_rights_set(&rights, CAP_CONNECT);
	}

	AUDIT_ARG_FD(uap->sd);
	error = getsock_cap(td->td_proc->p_fd, uap->sd, &rights, &fp, NULL);
	if (error != 0)
		goto sctp_bad1;

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		error = freebsd32_copyiniov((struct iovec32 *)uap->iov,
		    uap->iovlen, &iov, EMSGSIZE);
	else
#endif
		error = copyiniov(uap->iov, uap->iovlen, &iov, EMSGSIZE);
	if (error != 0)
		goto sctp_bad1;
#ifdef KTRACE
	if (to && (KTRPOINT(td, KTR_STRUCT)))
		ktrsockaddr(to);
#endif

	so = (struct socket *)fp->f_data;
	if (so->so_proto->pr_protocol != IPPROTO_SCTP) {
		error = EOPNOTSUPP;
		goto sctp_bad;
	}
#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0)
		goto sctp_bad;
#endif /* MAC */

	auio.uio_iov = iov;
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
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(&auio);
#endif /* KTRACE */
	len = auio.uio_resid;
	CURVNET_SET(so->so_vnet);
	error = sctp_lower_sosend(so, to, &auio,
		    (struct mbuf *)NULL, (struct mbuf *)NULL,
		    uap->flags, u_sinfo, td);
	CURVNET_RESTORE();
	if (error != 0) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket */
		if (error == EPIPE && !(so->so_options & SO_NOSIGPIPE) &&
		    !(uap->flags & MSG_NOSIGNAL)) {
			PROC_LOCK(td->td_proc);
			tdsignal(td, SIGPIPE);
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
	if (fp != NULL)
		fdrop(fp, td);
sctp_bad2:
	free(to, M_SONAME);
	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}

int
sys_sctp_generic_recvmsg(td, uap)
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
	uint8_t sockbufstore[256];
	struct uio auio;
	struct iovec *iov, *tiov;
	struct sctp_sndrcvinfo sinfo;
	struct socket *so;
	struct file *fp = NULL;
	struct sockaddr *fromsa;
	cap_rights_t rights;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif
	ssize_t len;
	int error, fromlen, i, msg_flags;

	AUDIT_ARG_FD(uap->sd);
	error = getsock_cap(td->td_proc->p_fd, uap->sd,
	    cap_rights_init(&rights, CAP_RECV), &fp, NULL);
	if (error != 0)
		return (error);
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		error = freebsd32_copyiniov((struct iovec32 *)uap->iov,
		    uap->iovlen, &iov, EMSGSIZE);
	else
#endif
		error = copyiniov(uap->iov, uap->iovlen, &iov, EMSGSIZE);
	if (error != 0)
		goto out1;

	so = fp->f_data;
	if (so->so_proto->pr_protocol != IPPROTO_SCTP) {
		error = EOPNOTSUPP;
		goto out;
	}
#ifdef MAC
	error = mac_socket_check_receive(td->td_ucred, so);
	if (error != 0)
		goto out;
#endif /* MAC */

	if (uap->fromlenaddr != NULL) {
		error = copyin(uap->fromlenaddr, &fromlen, sizeof (fromlen));
		if (error != 0)
			goto out;
	} else {
		fromlen = 0;
	}
	if (uap->msg_flags) {
		error = copyin(uap->msg_flags, &msg_flags, sizeof (int));
		if (error != 0)
			goto out;
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
	memset(&sinfo, 0, sizeof(struct sctp_sndrcvinfo));
	CURVNET_SET(so->so_vnet);
	error = sctp_sorecvmsg(so, &auio, (struct mbuf **)NULL,
		    fromsa, fromlen, &msg_flags,
		    (struct sctp_sndrcvinfo *)&sinfo, 1);
	CURVNET_RESTORE();
	if (error != 0) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	} else {
		if (uap->sinfo)
			error = copyout(&sinfo, uap->sinfo, sizeof (sinfo));
	}
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = len - auio.uio_resid;
		ktrgenio(uap->sd, UIO_READ, ktruio, error);
	}
#endif /* KTRACE */
	if (error != 0)
		goto out;
	td->td_retval[0] = len - auio.uio_resid;

	if (fromlen && uap->from) {
		len = fromlen;
		if (len <= 0 || fromsa == 0)
			len = 0;
		else {
			len = MIN(len, fromsa->sa_len);
			error = copyout(fromsa, uap->from, (size_t)len);
			if (error != 0)
				goto out;
		}
		error = copyout(&len, uap->fromlenaddr, sizeof (socklen_t));
		if (error != 0)
			goto out;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrsockaddr(fromsa);
#endif
	if (uap->msg_flags) {
		error = copyout(&msg_flags, uap->msg_flags, sizeof (int));
		if (error != 0)
			goto out;
	}
out:
	free(iov, M_IOV);
out1:
	if (fp != NULL)
		fdrop(fp, td);

	return (error);
#else  /* SCTP */
	return (EOPNOTSUPP);
#endif /* SCTP */
}
