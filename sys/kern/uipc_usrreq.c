/*-
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.
 * Copyright 2004-2005 Robert N. M. Watson
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
 *	From: @(#)uipc_usrreq.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>		/* XXX must be before <sys/file.h> */
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/vnode.h>

#include <vm/uma.h>

static uma_zone_t unp_zone;
static	unp_gen_t unp_gencnt;
static	u_int unp_count;

static	struct unp_head unp_shead, unp_dhead;

/*
 * Unix communications domain.
 *
 * TODO:
 *	SEQPACKET, RDM
 *	rethink name space problems
 *	need a proper out-of-band
 *	lock pushdown
 */
static const struct	sockaddr sun_noname = { sizeof(sun_noname), AF_LOCAL };
static ino_t	unp_ino;		/* prototype for fake inode numbers */
struct mbuf *unp_addsockcred(struct thread *, struct mbuf *);

/*
 * Currently, UNIX domain sockets are protected by a single subsystem lock,
 * which covers global data structures and variables, the contents of each
 * per-socket unpcb structure, and the so_pcb field in sockets attached to
 * the UNIX domain.  This provides for a moderate degree of paralellism, as
 * receive operations on UNIX domain sockets do not need to acquire the
 * subsystem lock.  Finer grained locking to permit send() without acquiring
 * a global lock would be a logical next step.
 *
 * The UNIX domain socket lock preceds all socket layer locks, including the
 * socket lock and socket buffer lock, permitting UNIX domain socket code to
 * call into socket support routines without releasing its locks.
 *
 * Some caution is required in areas where the UNIX domain socket code enters
 * VFS in order to create or find rendezvous points.  This results in
 * dropping of the UNIX domain socket subsystem lock, acquisition of the
 * Giant lock, and potential sleeping.  This increases the chances of races,
 * and exposes weaknesses in the socket->protocol API by offering poor
 * failure modes.
 */
static struct mtx unp_mtx;
#define	UNP_LOCK_INIT() \
	mtx_init(&unp_mtx, "unp", NULL, MTX_DEF)
#define	UNP_LOCK()		mtx_lock(&unp_mtx)
#define	UNP_UNLOCK()		mtx_unlock(&unp_mtx)
#define	UNP_LOCK_ASSERT()	mtx_assert(&unp_mtx, MA_OWNED)
#define	UNP_UNLOCK_ASSERT()	mtx_assert(&unp_mtx, MA_NOTOWNED)

/*
 * Garbage collection of cyclic file descriptor/socket references occurs
 * asynchronously in a taskqueue context in order to avoid recursion and
 * reentrance in the UNIX domain socket, file descriptor, and socket layer
 * code.  See unp_gc() for a full description.
 */
static struct task	unp_gc_task;

static int     unp_attach(struct socket *);
static void    unp_detach(struct unpcb *);
static int     unp_bind(struct unpcb *,struct sockaddr *, struct thread *);
static int     unp_connect(struct socket *,struct sockaddr *, struct thread *);
static int     unp_connect2(struct socket *so, struct socket *so2, int);
static void    unp_disconnect(struct unpcb *);
static void    unp_shutdown(struct unpcb *);
static void    unp_drop(struct unpcb *, int);
static void    unp_gc(__unused void *, int);
static void    unp_scan(struct mbuf *, void (*)(struct file *));
static void    unp_mark(struct file *);
static void    unp_discard(struct file *);
static void    unp_freerights(struct file **, int);
static int     unp_internalize(struct mbuf **, struct thread *);
static int     unp_listen(struct socket *, struct unpcb *, int,
		   struct thread *);

static int
uipc_abort(struct socket *so)
{
	struct unpcb *unp;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	unp_drop(unp, ECONNABORTED);
	unp_detach(unp);
	UNP_UNLOCK_ASSERT();
	ACCEPT_LOCK();
	SOCK_LOCK(so);
	sotryfree(so);
	return (0);
}

static int
uipc_accept(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp;
	const struct sockaddr *sa;

	/*
	 * Pass back name of connected socket,
	 * if it was bound and we are still connected
	 * (our peer may have closed already!).
	 */
	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		free(*nam, M_SONAME);
		*nam = NULL;
		return (EINVAL);
	}
	if (unp->unp_conn != NULL && unp->unp_conn->unp_addr != NULL)
		sa = (struct sockaddr *) unp->unp_conn->unp_addr;
	else
		sa = &sun_noname;
	bcopy(sa, *nam, sa->sa_len);
	UNP_UNLOCK();
	return (0);
}

static int
uipc_attach(struct socket *so, int proto, struct thread *td)
{
	struct unpcb *unp = sotounpcb(so);

	if (unp != NULL)
		return (EISCONN);
	return (unp_attach(so));
}

static int
uipc_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct unpcb *unp;
	int error;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	error = unp_bind(unp, nam, td);
	UNP_UNLOCK();
	return (error);
}

static int
uipc_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct unpcb *unp;
	int error;

	KASSERT(td == curthread, ("uipc_connect: td != curthread"));

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	error = unp_connect(so, nam, td);
	UNP_UNLOCK();
	return (error);
}

int
uipc_connect2(struct socket *so1, struct socket *so2)
{
	struct unpcb *unp;
	int error;

	UNP_LOCK();
	unp = sotounpcb(so1);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	error = unp_connect2(so1, so2, PRU_CONNECT2);
	UNP_UNLOCK();
	return (error);
}

/* control is EOPNOTSUPP */

static int
uipc_detach(struct socket *so)
{
	struct unpcb *unp;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	unp_detach(unp);
	UNP_UNLOCK_ASSERT();
	return (0);
}

static int
uipc_disconnect(struct socket *so)
{
	struct unpcb *unp;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	unp_disconnect(unp);
	UNP_UNLOCK();
	return (0);
}

static int
uipc_listen(struct socket *so, int backlog, struct thread *td)
{
	struct unpcb *unp;
	int error;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL || unp->unp_vnode == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	error = unp_listen(so, unp, backlog, td);
	UNP_UNLOCK();
	return (error);
}

static int
uipc_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp;
	const struct sockaddr *sa;

	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		free(*nam, M_SONAME);
		*nam = NULL;
		return (EINVAL);
	}
	if (unp->unp_conn != NULL && unp->unp_conn->unp_addr!= NULL)
		sa = (struct sockaddr *) unp->unp_conn->unp_addr;
	else {
		/*
		 * XXX: It seems that this test always fails even when
		 * connection is established.  So, this else clause is
		 * added as workaround to return PF_LOCAL sockaddr.
		 */
		sa = &sun_noname;
	}
	bcopy(sa, *nam, sa->sa_len);
	UNP_UNLOCK();
	return (0);
}

static int
uipc_rcvd(struct socket *so, int flags)
{
	struct unpcb *unp;
	struct socket *so2;
	u_long newhiwat;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	switch (so->so_type) {
	case SOCK_DGRAM:
		panic("uipc_rcvd DGRAM?");
		/*NOTREACHED*/

	case SOCK_STREAM:
		if (unp->unp_conn == NULL)
			break;
		so2 = unp->unp_conn->unp_socket;
		SOCKBUF_LOCK(&so2->so_snd);
		SOCKBUF_LOCK(&so->so_rcv);
		/*
		 * Adjust backpressure on sender
		 * and wakeup any waiting to write.
		 */
		so2->so_snd.sb_mbmax += unp->unp_mbcnt - so->so_rcv.sb_mbcnt;
		unp->unp_mbcnt = so->so_rcv.sb_mbcnt;
		newhiwat = so2->so_snd.sb_hiwat + unp->unp_cc -
		    so->so_rcv.sb_cc;
		(void)chgsbsize(so2->so_cred->cr_uidinfo, &so2->so_snd.sb_hiwat,
		    newhiwat, RLIM_INFINITY);
		unp->unp_cc = so->so_rcv.sb_cc;
		SOCKBUF_UNLOCK(&so->so_rcv);
		sowwakeup_locked(so2);
		break;

	default:
		panic("uipc_rcvd unknown socktype");
	}
	UNP_UNLOCK();
	return (0);
}

/* pru_rcvoob is EOPNOTSUPP */

static int
uipc_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	int error = 0;
	struct unpcb *unp;
	struct socket *so2;
	u_long newhiwat;

	unp = sotounpcb(so);
	if (unp == NULL) {
		error = EINVAL;
		goto release;
	}
	if (flags & PRUS_OOB) {
		error = EOPNOTSUPP;
		goto release;
	}

	if (control != NULL && (error = unp_internalize(&control, td)))
		goto release;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		error = EINVAL;
		goto dispose_release;
	}

	switch (so->so_type) {
	case SOCK_DGRAM:
	{
		const struct sockaddr *from;

		if (nam != NULL) {
			if (unp->unp_conn != NULL) {
				error = EISCONN;
				break;
			}
			error = unp_connect(so, nam, td);
			if (error)
				break;
		} else {
			if (unp->unp_conn == NULL) {
				error = ENOTCONN;
				break;
			}
		}
		so2 = unp->unp_conn->unp_socket;
		if (unp->unp_addr != NULL)
			from = (struct sockaddr *)unp->unp_addr;
		else
			from = &sun_noname;
		if (unp->unp_conn->unp_flags & UNP_WANTCRED)
			control = unp_addsockcred(td, control);
		SOCKBUF_LOCK(&so2->so_rcv);
		if (sbappendaddr_locked(&so2->so_rcv, from, m, control)) {
			sorwakeup_locked(so2);
			m = NULL;
			control = NULL;
		} else {
			SOCKBUF_UNLOCK(&so2->so_rcv);
			error = ENOBUFS;
		}
		if (nam != NULL)
			unp_disconnect(unp);
		break;
	}

	case SOCK_STREAM:
		/* Connect if not connected yet. */
		/*
		 * Note: A better implementation would complain
		 * if not equal to the peer's address.
		 */
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (nam != NULL) {
				error = unp_connect(so, nam, td);
				if (error)
					break;	/* XXX */
			} else {
				error = ENOTCONN;
				break;
			}
		}

		SOCKBUF_LOCK(&so->so_snd);
		if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
			SOCKBUF_UNLOCK(&so->so_snd);
			error = EPIPE;
			break;
		}
		if (unp->unp_conn == NULL)
			panic("uipc_send connected but no connection?");
		so2 = unp->unp_conn->unp_socket;
		SOCKBUF_LOCK(&so2->so_rcv);
		if (unp->unp_conn->unp_flags & UNP_WANTCRED) {
			/*
			 * Credentials are passed only once on
			 * SOCK_STREAM.
			 */
			unp->unp_conn->unp_flags &= ~UNP_WANTCRED;
			control = unp_addsockcred(td, control);
		}
		/*
		 * Send to paired receive port, and then reduce
		 * send buffer hiwater marks to maintain backpressure.
		 * Wake up readers.
		 */
		if (control != NULL) {
			if (sbappendcontrol_locked(&so2->so_rcv, m, control))
				control = NULL;
		} else {
			sbappend_locked(&so2->so_rcv, m);
		}
		so->so_snd.sb_mbmax -=
			so2->so_rcv.sb_mbcnt - unp->unp_conn->unp_mbcnt;
		unp->unp_conn->unp_mbcnt = so2->so_rcv.sb_mbcnt;
		newhiwat = so->so_snd.sb_hiwat -
		    (so2->so_rcv.sb_cc - unp->unp_conn->unp_cc);
		(void)chgsbsize(so->so_cred->cr_uidinfo, &so->so_snd.sb_hiwat,
		    newhiwat, RLIM_INFINITY);
		SOCKBUF_UNLOCK(&so->so_snd);
		unp->unp_conn->unp_cc = so2->so_rcv.sb_cc;
		sorwakeup_locked(so2);
		m = NULL;
		break;

	default:
		panic("uipc_send unknown socktype");
	}

	/*
	 * SEND_EOF is equivalent to a SEND followed by
	 * a SHUTDOWN.
	 */
	if (flags & PRUS_EOF) {
		socantsendmore(so);
		unp_shutdown(unp);
	}
	UNP_UNLOCK();

dispose_release:
	if (control != NULL && error != 0)
		unp_dispose(control);

release:
	if (control != NULL)
		m_freem(control);
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
uipc_sense(struct socket *so, struct stat *sb)
{
	struct unpcb *unp;
	struct socket *so2;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	sb->st_blksize = so->so_snd.sb_hiwat;
	if (so->so_type == SOCK_STREAM && unp->unp_conn != NULL) {
		so2 = unp->unp_conn->unp_socket;
		sb->st_blksize += so2->so_rcv.sb_cc;
	}
	sb->st_dev = NODEV;
	if (unp->unp_ino == 0)
		unp->unp_ino = (++unp_ino == 0) ? ++unp_ino : unp_ino;
	sb->st_ino = unp->unp_ino;
	UNP_UNLOCK();
	return (0);
}

static int
uipc_shutdown(struct socket *so)
{
	struct unpcb *unp;

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	socantsendmore(so);
	unp_shutdown(unp);
	UNP_UNLOCK();
	return (0);
}

static int
uipc_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp;
	const struct sockaddr *sa;

	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		free(*nam, M_SONAME);
		*nam = NULL;
		return (EINVAL);
	}
	if (unp->unp_addr != NULL)
		sa = (struct sockaddr *) unp->unp_addr;
	else
		sa = &sun_noname;
	bcopy(sa, *nam, sa->sa_len);
	UNP_UNLOCK();
	return (0);
}

struct pr_usrreqs uipc_usrreqs = {
	.pru_abort = 		uipc_abort,
	.pru_accept =		uipc_accept,
	.pru_attach =		uipc_attach,
	.pru_bind =		uipc_bind,
	.pru_connect =		uipc_connect,
	.pru_connect2 =		uipc_connect2,
	.pru_detach =		uipc_detach,
	.pru_disconnect =	uipc_disconnect,
	.pru_listen =		uipc_listen,
	.pru_peeraddr =		uipc_peeraddr,
	.pru_rcvd =		uipc_rcvd,
	.pru_send =		uipc_send,
	.pru_sense =		uipc_sense,
	.pru_shutdown =		uipc_shutdown,
	.pru_sockaddr =		uipc_sockaddr,
	.pru_sosend =		sosend,
	.pru_soreceive =	soreceive,
	.pru_sopoll =		sopoll,
};

int
uipc_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct unpcb *unp;
	struct xucred xu;
	int error, optval;

	if (sopt->sopt_level != 0)
		return (EINVAL);

	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		UNP_UNLOCK();
		return (EINVAL);
	}
	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case LOCAL_PEERCRED:
			if (unp->unp_flags & UNP_HAVEPC)
				xu = unp->unp_peercred;
			else {
				if (so->so_type == SOCK_STREAM)
					error = ENOTCONN;
				else
					error = EINVAL;
			}
			if (error == 0)
				error = sooptcopyout(sopt, &xu, sizeof(xu));
			break;
		case LOCAL_CREDS:
			optval = unp->unp_flags & UNP_WANTCRED ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		case LOCAL_CONNWAIT:
			optval = unp->unp_flags & UNP_CONNWAIT ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		default:
			error = EOPNOTSUPP;
			break;
		}
		break;
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case LOCAL_CREDS:
		case LOCAL_CONNWAIT:
			error = sooptcopyin(sopt, &optval, sizeof(optval),
					    sizeof(optval));
			if (error)
				break;

#define	OPTSET(bit) \
	if (optval) \
		unp->unp_flags |= bit; \
	else \
		unp->unp_flags &= ~bit;

			switch (sopt->sopt_name) {
			case LOCAL_CREDS:
				OPTSET(UNP_WANTCRED);
				break;
			case LOCAL_CONNWAIT:
				OPTSET(UNP_CONNWAIT);
				break;
			default:
				break;
			}
			break;
#undef	OPTSET
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	UNP_UNLOCK();
	return (error);
}

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering
 * for stream sockets, although the total for sender and receiver is
 * actually only PIPSIZ.
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should
 * be large enough for at least one max-size datagram plus address.
 */
#ifndef PIPSIZ
#define	PIPSIZ	8192
#endif
static u_long	unpst_sendspace = PIPSIZ;
static u_long	unpst_recvspace = PIPSIZ;
static u_long	unpdg_sendspace = 2*1024;	/* really max datagram size */
static u_long	unpdg_recvspace = 4*1024;

static int	unp_rights;			/* file descriptors in flight */

SYSCTL_DECL(_net_local_stream);
SYSCTL_INT(_net_local_stream, OID_AUTO, sendspace, CTLFLAG_RW,
	   &unpst_sendspace, 0, "");
SYSCTL_INT(_net_local_stream, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpst_recvspace, 0, "");
SYSCTL_DECL(_net_local_dgram);
SYSCTL_INT(_net_local_dgram, OID_AUTO, maxdgram, CTLFLAG_RW,
	   &unpdg_sendspace, 0, "");
SYSCTL_INT(_net_local_dgram, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpdg_recvspace, 0, "");
SYSCTL_DECL(_net_local);
SYSCTL_INT(_net_local, OID_AUTO, inflight, CTLFLAG_RD, &unp_rights, 0, "");

static int
unp_attach(struct socket *so)
{
	struct unpcb *unp;
	int error;

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {

		case SOCK_STREAM:
			error = soreserve(so, unpst_sendspace, unpst_recvspace);
			break;

		case SOCK_DGRAM:
			error = soreserve(so, unpdg_sendspace, unpdg_recvspace);
			break;

		default:
			panic("unp_attach");
		}
		if (error)
			return (error);
	}
	unp = uma_zalloc(unp_zone, M_WAITOK | M_ZERO);
	if (unp == NULL)
		return (ENOBUFS);
	LIST_INIT(&unp->unp_refs);
	unp->unp_socket = so;
	so->so_pcb = unp;

	UNP_LOCK();
	unp->unp_gencnt = ++unp_gencnt;
	unp_count++;
	LIST_INSERT_HEAD(so->so_type == SOCK_DGRAM ? &unp_dhead
			 : &unp_shead, unp, unp_link);
	UNP_UNLOCK();

	return (0);
}

static void
unp_detach(struct unpcb *unp)
{
	struct vnode *vp;
	int local_unp_rights;

	UNP_LOCK_ASSERT();

	LIST_REMOVE(unp, unp_link);
	unp->unp_gencnt = ++unp_gencnt;
	--unp_count;
	if ((vp = unp->unp_vnode) != NULL) {
		/*
		 * XXXRW: should v_socket be frobbed only while holding
		 * Giant?
		 */
		unp->unp_vnode->v_socket = NULL;
		unp->unp_vnode = NULL;
	}
	if (unp->unp_conn != NULL)
		unp_disconnect(unp);
	while (!LIST_EMPTY(&unp->unp_refs)) {
		struct unpcb *ref = LIST_FIRST(&unp->unp_refs);
		unp_drop(ref, ECONNRESET);
	}
	soisdisconnected(unp->unp_socket);
	unp->unp_socket->so_pcb = NULL;
	local_unp_rights = unp_rights;
	UNP_UNLOCK();
	if (unp->unp_addr != NULL)
		FREE(unp->unp_addr, M_SONAME);
	uma_zfree(unp_zone, unp);
	if (vp) {
		mtx_lock(&Giant);
		vrele(vp);
		mtx_unlock(&Giant);
	}
	if (local_unp_rights)
		taskqueue_enqueue(taskqueue_thread, &unp_gc_task);
}

static int
unp_bind(struct unpcb *unp, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	struct vnode *vp;
	struct mount *mp;
	struct vattr vattr;
	int error, namelen;
	struct nameidata nd;
	char *buf;

	UNP_LOCK_ASSERT();

	/*
	 * XXXRW: This test-and-set of unp_vnode is non-atomic; the
	 * unlocked read here is fine, but the value of unp_vnode needs
	 * to be tested again after we do all the lookups to see if the
	 * pcb is still unbound?
	 */
	if (unp->unp_vnode != NULL)
		return (EINVAL);

	namelen = soun->sun_len - offsetof(struct sockaddr_un, sun_path);
	if (namelen <= 0)
		return (EINVAL);

	UNP_UNLOCK();

	buf = malloc(namelen + 1, M_TEMP, M_WAITOK);
	strlcpy(buf, soun->sun_path, namelen + 1);

	mtx_lock(&Giant);
restart:
	mtx_assert(&Giant, MA_OWNED);
	NDINIT(&nd, CREATE, NOFOLLOW | LOCKPARENT | SAVENAME, UIO_SYSSPACE,
	    buf, td);
/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	error = namei(&nd);
	if (error)
		goto done;
	vp = nd.ni_vp;
	if (vp != NULL || vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp != NULL) {
			vrele(vp);
			error = EADDRINUSE;
			goto done;
		}
		error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH);
		if (error)
			goto done;
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = (ACCESSPERMS & ~td->td_proc->p_fd->fd_cmask);
#ifdef MAC
	error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
#endif
	if (error == 0) {
		VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	if (error) {
		vn_finished_write(mp);
		goto done;
	}
	vp = nd.ni_vp;
	ASSERT_VOP_LOCKED(vp, "unp_bind");
	soun = (struct sockaddr_un *)sodupsockaddr(nam, M_WAITOK);
	UNP_LOCK();
	vp->v_socket = unp->unp_socket;
	unp->unp_vnode = vp;
	unp->unp_addr = soun;
	UNP_UNLOCK();
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
done:
	mtx_unlock(&Giant);
	free(buf, M_TEMP);
	UNP_LOCK();
	return (error);
}

static int
unp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	struct vnode *vp;
	struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	int error, len;
	struct nameidata nd;
	char buf[SOCK_MAXADDRLEN];
	struct sockaddr *sa;

	UNP_LOCK_ASSERT();
	unp = sotounpcb(so);

	len = nam->sa_len - offsetof(struct sockaddr_un, sun_path);
	if (len <= 0)
		return (EINVAL);
	strlcpy(buf, soun->sun_path, len + 1);
	UNP_UNLOCK();
	sa = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	mtx_lock(&Giant);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, buf, td);
	error = namei(&nd);
	if (error)
		vp = NULL;
	else
		vp = nd.ni_vp;
	ASSERT_VOP_LOCKED(vp, "unp_connect");
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (error)
		goto bad;

	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
	error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td);
	if (error)
		goto bad;
	mtx_unlock(&Giant);
	UNP_LOCK();
	unp = sotounpcb(so);
	if (unp == NULL) {
		error = EINVAL;
		goto bad2;
	}
	so2 = vp->v_socket;
	if (so2 == NULL) {
		error = ECONNREFUSED;
		goto bad2;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad2;
	}
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		if (so2->so_options & SO_ACCEPTCONN) {
			/*
			 * NB: drop locks here so unp_attach is entered
			 *     w/o locks; this avoids a recursive lock
			 *     of the head and holding sleep locks across
			 *     a (potentially) blocking malloc.
			 */
			UNP_UNLOCK();
			so3 = sonewconn(so2, 0);
			UNP_LOCK();
		} else
			so3 = NULL;
		if (so3 == NULL) {
			error = ECONNREFUSED;
			goto bad2;
		}
		unp = sotounpcb(so);
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		if (unp2->unp_addr != NULL) {
			bcopy(unp2->unp_addr, sa, unp2->unp_addr->sun_len);
			unp3->unp_addr = (struct sockaddr_un *) sa;
			sa = NULL;
		}
		/*
		 * unp_peercred management:
		 *
		 * The connecter's (client's) credentials are copied
		 * from its process structure at the time of connect()
		 * (which is now).
		 */
		cru2x(td->td_ucred, &unp3->unp_peercred);
		unp3->unp_flags |= UNP_HAVEPC;
		/*
		 * The receiver's (server's) credentials are copied
		 * from the unp_peercred member of socket on which the
		 * former called listen(); unp_listen() cached that
		 * process's credentials at that time so we can use
		 * them now.
		 */
		KASSERT(unp2->unp_flags & UNP_HAVEPCCACHED,
		    ("unp_connect: listener without cached peercred"));
		memcpy(&unp->unp_peercred, &unp2->unp_peercred,
		    sizeof(unp->unp_peercred));
		unp->unp_flags |= UNP_HAVEPC;
#ifdef MAC
		SOCK_LOCK(so);
		mac_set_socket_peer_from_socket(so, so3);
		mac_set_socket_peer_from_socket(so3, so);
		SOCK_UNLOCK(so);
#endif

		so2 = so3;
	}
	error = unp_connect2(so, so2, PRU_CONNECT);
bad2:
	UNP_UNLOCK();
	mtx_lock(&Giant);
bad:
	mtx_assert(&Giant, MA_OWNED);
	if (vp != NULL)
		vput(vp);
	mtx_unlock(&Giant);
	free(sa, M_SONAME);
	UNP_LOCK();
	return (error);
}

static int
unp_connect2(struct socket *so, struct socket *so2, int req)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;

	UNP_LOCK_ASSERT();

	if (so2->so_type != so->so_type)
		return (EPROTOTYPE);
	unp2 = sotounpcb(so2);
	unp->unp_conn = unp2;
	switch (so->so_type) {

	case SOCK_DGRAM:
		LIST_INSERT_HEAD(&unp2->unp_refs, unp, unp_reflink);
		soisconnected(so);
		break;

	case SOCK_STREAM:
		unp2->unp_conn = unp;
		if (req == PRU_CONNECT &&
		    ((unp->unp_flags | unp2->unp_flags) & UNP_CONNWAIT))
			soisconnecting(so);
		else
			soisconnected(so);
		soisconnected(so2);
		break;

	default:
		panic("unp_connect2");
	}
	return (0);
}

static void
unp_disconnect(struct unpcb *unp)
{
	struct unpcb *unp2 = unp->unp_conn;
	struct socket *so;

	UNP_LOCK_ASSERT();

	if (unp2 == NULL)
		return;
	unp->unp_conn = NULL;
	switch (unp->unp_socket->so_type) {

	case SOCK_DGRAM:
		LIST_REMOVE(unp, unp_reflink);
		so = unp->unp_socket;
		SOCK_LOCK(so);
		so->so_state &= ~SS_ISCONNECTED;
		SOCK_UNLOCK(so);
		break;

	case SOCK_STREAM:
		soisdisconnected(unp->unp_socket);
		unp2->unp_conn = NULL;
		soisdisconnected(unp2->unp_socket);
		break;
	}
}

#ifdef notdef
void
unp_abort(struct unpcb *unp)
{

	unp_detach(unp);
	UNP_UNLOCK_ASSERT();
}
#endif

/*
 * unp_pcblist() assumes that UNIX domain socket memory is never reclaimed
 * by the zone (UMA_ZONE_NOFREE), and as such potentially stale pointers
 * are safe to reference.  It first scans the list of struct unpcb's to
 * generate a pointer list, then it rescans its list one entry at a time to
 * externalize and copyout.  It checks the generation number to see if a
 * struct unpcb has been reused, and will skip it if so.
 */
static int
unp_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, n;
	struct unpcb *unp, **unp_list;
	unp_gen_t gencnt;
	struct xunpgen *xug;
	struct unp_head *head;
	struct xunpcb *xu;

	head = ((intptr_t)arg1 == SOCK_DGRAM ? &unp_dhead : &unp_shead);

	/*
	 * The process of preparing the PCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == NULL) {
		n = unp_count;
		req->oldidx = 2 * (sizeof *xug)
			+ (n + n/8) * sizeof(struct xunpcb);
		return (0);
	}

	if (req->newptr != NULL)
		return (EPERM);

	/*
	 * OK, now we're committed to doing something.
	 */
	xug = malloc(sizeof(*xug), M_TEMP, M_WAITOK);
	UNP_LOCK();
	gencnt = unp_gencnt;
	n = unp_count;
	UNP_UNLOCK();

	xug->xug_len = sizeof *xug;
	xug->xug_count = n;
	xug->xug_gen = gencnt;
	xug->xug_sogen = so_gencnt;
	error = SYSCTL_OUT(req, xug, sizeof *xug);
	if (error) {
		free(xug, M_TEMP);
		return (error);
	}

	unp_list = malloc(n * sizeof *unp_list, M_TEMP, M_WAITOK);

	UNP_LOCK();
	for (unp = LIST_FIRST(head), i = 0; unp && i < n;
	     unp = LIST_NEXT(unp, unp_link)) {
		if (unp->unp_gencnt <= gencnt) {
			if (cr_cansee(req->td->td_ucred,
			    unp->unp_socket->so_cred))
				continue;
			unp_list[i++] = unp;
		}
	}
	UNP_UNLOCK();
	n = i;			/* in case we lost some during malloc */

	error = 0;
	xu = malloc(sizeof(*xu), M_TEMP, M_WAITOK | M_ZERO);
	for (i = 0; i < n; i++) {
		unp = unp_list[i];
		if (unp->unp_gencnt <= gencnt) {
			xu->xu_len = sizeof *xu;
			xu->xu_unpp = unp;
			/*
			 * XXX - need more locking here to protect against
			 * connect/disconnect races for SMP.
			 */
			if (unp->unp_addr != NULL)
				bcopy(unp->unp_addr, &xu->xu_addr,
				      unp->unp_addr->sun_len);
			if (unp->unp_conn != NULL &&
			    unp->unp_conn->unp_addr != NULL)
				bcopy(unp->unp_conn->unp_addr,
				      &xu->xu_caddr,
				      unp->unp_conn->unp_addr->sun_len);
			bcopy(unp, &xu->xu_unp, sizeof *unp);
			sotoxsocket(unp->unp_socket, &xu->xu_socket);
			error = SYSCTL_OUT(req, xu, sizeof *xu);
		}
	}
	free(xu, M_TEMP);
	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		xug->xug_gen = unp_gencnt;
		xug->xug_sogen = so_gencnt;
		xug->xug_count = unp_count;
		error = SYSCTL_OUT(req, xug, sizeof *xug);
	}
	free(unp_list, M_TEMP);
	free(xug, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_local_dgram, OID_AUTO, pcblist, CTLFLAG_RD,
	    (caddr_t)(long)SOCK_DGRAM, 0, unp_pcblist, "S,xunpcb",
	    "List of active local datagram sockets");
SYSCTL_PROC(_net_local_stream, OID_AUTO, pcblist, CTLFLAG_RD,
	    (caddr_t)(long)SOCK_STREAM, 0, unp_pcblist, "S,xunpcb",
	    "List of active local stream sockets");

static void
unp_shutdown(struct unpcb *unp)
{
	struct socket *so;

	UNP_LOCK_ASSERT();

	if (unp->unp_socket->so_type == SOCK_STREAM && unp->unp_conn &&
	    (so = unp->unp_conn->unp_socket))
		socantrcvmore(so);
}

static void
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;

	UNP_LOCK_ASSERT();

	so->so_error = errno;
	unp_disconnect(unp);
}

#ifdef notdef
void
unp_drain(void)
{

}
#endif

static void
unp_freerights(struct file **rp, int fdcount)
{
	int i;
	struct file *fp;

	for (i = 0; i < fdcount; i++) {
		fp = *rp;
		/*
		 * zero the pointer before calling
		 * unp_discard since it may end up
		 * in unp_gc()..
		 */
		*rp++ = 0;
		unp_discard(fp);
	}
}

int
unp_externalize(struct mbuf *control, struct mbuf **controlp)
{
	struct thread *td = curthread;		/* XXX */
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	int i;
	int *fdp;
	struct file **rp;
	struct file *fp;
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error, newfds;
	int f;
	u_int newlen;

	UNP_UNLOCK_ASSERT();

	error = 0;
	if (controlp != NULL) /* controlp == NULL => free control messages */
		*controlp = NULL;

	while (cm != NULL) {
		if (sizeof(*cm) > clen || cm->cmsg_len > clen) {
			error = EINVAL;
			break;
		}

		data = CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;

		if (cm->cmsg_level == SOL_SOCKET
		    && cm->cmsg_type == SCM_RIGHTS) {
			newfds = datalen / sizeof(struct file *);
			rp = data;

			/* If we're not outputting the descriptors free them. */
			if (error || controlp == NULL) {
				unp_freerights(rp, newfds);
				goto next;
			}
			FILEDESC_LOCK(td->td_proc->p_fd);
			/* if the new FD's will not fit free them.  */
			if (!fdavail(td, newfds)) {
				FILEDESC_UNLOCK(td->td_proc->p_fd);
				error = EMSGSIZE;
				unp_freerights(rp, newfds);
				goto next;
			}
			/*
			 * now change each pointer to an fd in the global
			 * table to an integer that is the index to the
			 * local fd table entry that we set up to point
			 * to the global one we are transferring.
			 */
			newlen = newfds * sizeof(int);
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET);
			if (*controlp == NULL) {
				FILEDESC_UNLOCK(td->td_proc->p_fd);
				error = E2BIG;
				unp_freerights(rp, newfds);
				goto next;
			}

			fdp = (int *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			for (i = 0; i < newfds; i++) {
				if (fdalloc(td, 0, &f))
					panic("unp_externalize fdalloc failed");
				fp = *rp++;
				td->td_proc->p_fd->fd_ofiles[f] = fp;
				FILE_LOCK(fp);
				fp->f_msgcount--;
				FILE_UNLOCK(fp);
				unp_rights--;
				*fdp++ = f;
			}
			FILEDESC_UNLOCK(td->td_proc->p_fd);
		} else { /* We can just copy anything else across */
			if (error || controlp == NULL)
				goto next;
			*controlp = sbcreatecontrol(NULL, datalen,
			    cm->cmsg_type, cm->cmsg_level);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto next;
			}
			bcopy(data,
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *)),
			    datalen);
		}

		controlp = &(*controlp)->m_next;

next:
		if (CMSG_SPACE(datalen) < clen) {
			clen -= CMSG_SPACE(datalen);
			cm = (struct cmsghdr *)
			    ((caddr_t)cm + CMSG_SPACE(datalen));
		} else {
			clen = 0;
			cm = NULL;
		}
	}

	m_freem(control);

	return (error);
}

void
unp_init(void)
{
	unp_zone = uma_zcreate("unpcb", sizeof(struct unpcb), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	if (unp_zone == NULL)
		panic("unp_init");
	uma_zone_set_max(unp_zone, nmbclusters);
	LIST_INIT(&unp_dhead);
	LIST_INIT(&unp_shead);
	TASK_INIT(&unp_gc_task, 0, unp_gc, NULL);
	UNP_LOCK_INIT();
}

static int
unp_internalize(struct mbuf **controlp, struct thread *td)
{
	struct mbuf *control = *controlp;
	struct proc *p = td->td_proc;
	struct filedesc *fdescp = p->p_fd;
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	struct cmsgcred *cmcred;
	struct file **rp;
	struct file *fp;
	struct timeval *tv;
	int i, fd, *fdp;
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error, oldfds;
	u_int newlen;

	UNP_UNLOCK_ASSERT();

	error = 0;
	*controlp = NULL;

	while (cm != NULL) {
		if (sizeof(*cm) > clen || cm->cmsg_level != SOL_SOCKET
		    || cm->cmsg_len > clen) {
			error = EINVAL;
			goto out;
		}

		data = CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;

		switch (cm->cmsg_type) {
		/*
		 * Fill in credential information.
		 */
		case SCM_CREDS:
			*controlp = sbcreatecontrol(NULL, sizeof(*cmcred),
			    SCM_CREDS, SOL_SOCKET);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto out;
			}

			cmcred = (struct cmsgcred *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			cmcred->cmcred_pid = p->p_pid;
			cmcred->cmcred_uid = td->td_ucred->cr_ruid;
			cmcred->cmcred_gid = td->td_ucred->cr_rgid;
			cmcred->cmcred_euid = td->td_ucred->cr_uid;
			cmcred->cmcred_ngroups = MIN(td->td_ucred->cr_ngroups,
							CMGROUP_MAX);
			for (i = 0; i < cmcred->cmcred_ngroups; i++)
				cmcred->cmcred_groups[i] =
				    td->td_ucred->cr_groups[i];
			break;

		case SCM_RIGHTS:
			oldfds = datalen / sizeof (int);
			/*
			 * check that all the FDs passed in refer to legal files
			 * If not, reject the entire operation.
			 */
			fdp = data;
			FILEDESC_LOCK(fdescp);
			for (i = 0; i < oldfds; i++) {
				fd = *fdp++;
				if ((unsigned)fd >= fdescp->fd_nfiles ||
				    fdescp->fd_ofiles[fd] == NULL) {
					FILEDESC_UNLOCK(fdescp);
					error = EBADF;
					goto out;
				}
				fp = fdescp->fd_ofiles[fd];
				if (!(fp->f_ops->fo_flags & DFLAG_PASSABLE)) {
					FILEDESC_UNLOCK(fdescp);
					error = EOPNOTSUPP;
					goto out;
				}

			}
			/*
			 * Now replace the integer FDs with pointers to
			 * the associated global file table entry..
			 */
			newlen = oldfds * sizeof(struct file *);
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET);
			if (*controlp == NULL) {
				FILEDESC_UNLOCK(fdescp);
				error = E2BIG;
				goto out;
			}

			fdp = data;
			rp = (struct file **)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			for (i = 0; i < oldfds; i++) {
				fp = fdescp->fd_ofiles[*fdp++];
				*rp++ = fp;
				FILE_LOCK(fp);
				fp->f_count++;
				fp->f_msgcount++;
				FILE_UNLOCK(fp);
				unp_rights++;
			}
			FILEDESC_UNLOCK(fdescp);
			break;

		case SCM_TIMESTAMP:
			*controlp = sbcreatecontrol(NULL, sizeof(*tv),
			    SCM_TIMESTAMP, SOL_SOCKET);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto out;
			}
			tv = (struct timeval *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			microtime(tv);
			break;

		default:
			error = EINVAL;
			goto out;
		}

		controlp = &(*controlp)->m_next;

		if (CMSG_SPACE(datalen) < clen) {
			clen -= CMSG_SPACE(datalen);
			cm = (struct cmsghdr *)
			    ((caddr_t)cm + CMSG_SPACE(datalen));
		} else {
			clen = 0;
			cm = NULL;
		}
	}

out:
	m_freem(control);

	return (error);
}

struct mbuf *
unp_addsockcred(struct thread *td, struct mbuf *control)
{
	struct mbuf *m, *n;
	struct sockcred *sc;
	int ngroups;
	int i;

	ngroups = MIN(td->td_ucred->cr_ngroups, CMGROUP_MAX);

	m = sbcreatecontrol(NULL, SOCKCREDSIZE(ngroups), SCM_CREDS, SOL_SOCKET);
	if (m == NULL)
		return (control);
	m->m_next = NULL;

	sc = (struct sockcred *) CMSG_DATA(mtod(m, struct cmsghdr *));
	sc->sc_uid = td->td_ucred->cr_ruid;
	sc->sc_euid = td->td_ucred->cr_uid;
	sc->sc_gid = td->td_ucred->cr_rgid;
	sc->sc_egid = td->td_ucred->cr_gid;
	sc->sc_ngroups = ngroups;
	for (i = 0; i < sc->sc_ngroups; i++)
		sc->sc_groups[i] = td->td_ucred->cr_groups[i];

	/*
	 * If a control message already exists, append us to the end.
	 */
	if (control != NULL) {
		for (n = control; n->m_next != NULL; n = n->m_next)
			;
		n->m_next = m;
	} else
		control = m;

	return (control);
}

/*
 * unp_defer indicates whether additional work has been defered for a future
 * pass through unp_gc().  It is thread local and does not require explicit
 * synchronization.
 */
static int	unp_defer;

static int unp_taskcount;
SYSCTL_INT(_net_local, OID_AUTO, taskcount, CTLFLAG_RD, &unp_taskcount, 0, "");

static int unp_recycled;
SYSCTL_INT(_net_local, OID_AUTO, recycled, CTLFLAG_RD, &unp_recycled, 0, "");

static void
unp_gc(__unused void *arg, int pending)
{
	struct file *fp, *nextfp;
	struct socket *so;
	struct file **extra_ref, **fpp;
	int nunref, i;
	int nfiles_snap;
	int nfiles_slack = 20;

	unp_taskcount++;
	unp_defer = 0;
	/*
	 * before going through all this, set all FDs to
	 * be NOT defered and NOT externally accessible
	 */
	sx_slock(&filelist_lock);
	LIST_FOREACH(fp, &filehead, f_list)
		fp->f_gcflag &= ~(FMARK|FDEFER);
	do {
		KASSERT(unp_defer >= 0, ("unp_gc: unp_defer %d", unp_defer));
		LIST_FOREACH(fp, &filehead, f_list) {
			FILE_LOCK(fp);
			/*
			 * If the file is not open, skip it -- could be a
			 * file in the process of being opened, or in the
			 * process of being closed.  If the file is
			 * "closing", it may have been marked for deferred
			 * consideration.  Clear the flag now if so.
			 */
			if (fp->f_count == 0) {
				if (fp->f_gcflag & FDEFER)
					unp_defer--;
				fp->f_gcflag &= ~(FMARK|FDEFER);
				FILE_UNLOCK(fp);
				continue;
			}
			/*
			 * If we already marked it as 'defer'  in a
			 * previous pass, then try process it this time
			 * and un-mark it
			 */
			if (fp->f_gcflag & FDEFER) {
				fp->f_gcflag &= ~FDEFER;
				unp_defer--;
			} else {
				/*
				 * if it's not defered, then check if it's
				 * already marked.. if so skip it
				 */
				if (fp->f_gcflag & FMARK) {
					FILE_UNLOCK(fp);
					continue;
				}
				/*
				 * If all references are from messages
				 * in transit, then skip it. it's not
				 * externally accessible.
				 */
				if (fp->f_count == fp->f_msgcount) {
					FILE_UNLOCK(fp);
					continue;
				}
				/*
				 * If it got this far then it must be
				 * externally accessible.
				 */
				fp->f_gcflag |= FMARK;
			}
			/*
			 * either it was defered, or it is externally
			 * accessible and not already marked so.
			 * Now check if it is possibly one of OUR sockets.
			 */
			if (fp->f_type != DTYPE_SOCKET ||
			    (so = fp->f_data) == NULL) {
				FILE_UNLOCK(fp);
				continue;
			}
			FILE_UNLOCK(fp);
			if (so->so_proto->pr_domain != &localdomain ||
			    (so->so_proto->pr_flags&PR_RIGHTS) == 0)
				continue;
			/*
			 * So, Ok, it's one of our sockets and it IS externally
			 * accessible (or was defered). Now we look
			 * to see if we hold any file descriptors in its
			 * message buffers. Follow those links and mark them
			 * as accessible too.
			 */
			SOCKBUF_LOCK(&so->so_rcv);
			unp_scan(so->so_rcv.sb_mb, unp_mark);
			SOCKBUF_UNLOCK(&so->so_rcv);
		}
	} while (unp_defer);
	sx_sunlock(&filelist_lock);
	/*
	 * XXXRW: The following comments need updating for a post-SMPng and
	 * deferred unp_gc() world, but are still generally accurate.
	 *
	 * We grab an extra reference to each of the file table entries
	 * that are not otherwise accessible and then free the rights
	 * that are stored in messages on them.
	 *
	 * The bug in the orginal code is a little tricky, so I'll describe
	 * what's wrong with it here.
	 *
	 * It is incorrect to simply unp_discard each entry for f_msgcount
	 * times -- consider the case of sockets A and B that contain
	 * references to each other.  On a last close of some other socket,
	 * we trigger a gc since the number of outstanding rights (unp_rights)
	 * is non-zero.  If during the sweep phase the gc code unp_discards,
	 * we end up doing a (full) closef on the descriptor.  A closef on A
	 * results in the following chain.  Closef calls soo_close, which
	 * calls soclose.   Soclose calls first (through the switch
	 * uipc_usrreq) unp_detach, which re-invokes unp_gc.  Unp_gc simply
	 * returns because the previous instance had set unp_gcing, and
	 * we return all the way back to soclose, which marks the socket
	 * with SS_NOFDREF, and then calls sofree.  Sofree calls sorflush
	 * to free up the rights that are queued in messages on the socket A,
	 * i.e., the reference on B.  The sorflush calls via the dom_dispose
	 * switch unp_dispose, which unp_scans with unp_discard.  This second
	 * instance of unp_discard just calls closef on B.
	 *
	 * Well, a similar chain occurs on B, resulting in a sorflush on B,
	 * which results in another closef on A.  Unfortunately, A is already
	 * being closed, and the descriptor has already been marked with
	 * SS_NOFDREF, and soclose panics at this point.
	 *
	 * Here, we first take an extra reference to each inaccessible
	 * descriptor.  Then, we call sorflush ourself, since we know
	 * it is a Unix domain socket anyhow.  After we destroy all the
	 * rights carried in messages, we do a last closef to get rid
	 * of our extra reference.  This is the last close, and the
	 * unp_detach etc will shut down the socket.
	 *
	 * 91/09/19, bsy@cs.cmu.edu
	 */
again:
	nfiles_snap = openfiles + nfiles_slack;	/* some slack */
	extra_ref = malloc(nfiles_snap * sizeof(struct file *), M_TEMP,
	    M_WAITOK);
	sx_slock(&filelist_lock);
	if (nfiles_snap < openfiles) {
		sx_sunlock(&filelist_lock);
		free(extra_ref, M_TEMP);
		nfiles_slack += 20;
		goto again;
	}
	for (nunref = 0, fp = LIST_FIRST(&filehead), fpp = extra_ref;
	    fp != NULL; fp = nextfp) {
		nextfp = LIST_NEXT(fp, f_list);
		FILE_LOCK(fp);
		/*
		 * If it's not open, skip it
		 */
		if (fp->f_count == 0) {
			FILE_UNLOCK(fp);
			continue;
		}
		/*
		 * If all refs are from msgs, and it's not marked accessible
		 * then it must be referenced from some unreachable cycle
		 * of (shut-down) FDs, so include it in our
		 * list of FDs to remove
		 */
		if (fp->f_count == fp->f_msgcount && !(fp->f_gcflag & FMARK)) {
			*fpp++ = fp;
			nunref++;
			fp->f_count++;
		}
		FILE_UNLOCK(fp);
	}
	sx_sunlock(&filelist_lock);
	/*
	 * for each FD on our hit list, do the following two things
	 */
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp) {
		struct file *tfp = *fpp;
		FILE_LOCK(tfp);
		if (tfp->f_type == DTYPE_SOCKET &&
		    tfp->f_data != NULL) {
			FILE_UNLOCK(tfp);
			sorflush(tfp->f_data);
		} else {
			FILE_UNLOCK(tfp);
		}
	}
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp) {
		closef(*fpp, (struct thread *) NULL);
		unp_recycled++;
	}
	free(extra_ref, M_TEMP);
}

void
unp_dispose(struct mbuf *m)
{

	if (m)
		unp_scan(m, unp_discard);
}

static int
unp_listen(struct socket *so, struct unpcb *unp, int backlog,
    struct thread *td)
{
	int error;

	UNP_LOCK_ASSERT();

	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0) {
		cru2x(td->td_ucred, &unp->unp_peercred);
		unp->unp_flags |= UNP_HAVEPCCACHED;
		solisten_proto(so, backlog);
	}
	SOCK_UNLOCK(so);
	return (error);
}

static void
unp_scan(struct mbuf *m0, void (*op)(struct file *))
{
	struct mbuf *m;
	struct file **rp;
	struct cmsghdr *cm;
	void *data;
	int i;
	socklen_t clen, datalen;
	int qfds;

	while (m0 != NULL) {
		for (m = m0; m; m = m->m_next) {
			if (m->m_type != MT_CONTROL)
				continue;

			cm = mtod(m, struct cmsghdr *);
			clen = m->m_len;

			while (cm != NULL) {
				if (sizeof(*cm) > clen || cm->cmsg_len > clen)
					break;

				data = CMSG_DATA(cm);
				datalen = (caddr_t)cm + cm->cmsg_len
				    - (caddr_t)data;

				if (cm->cmsg_level == SOL_SOCKET &&
				    cm->cmsg_type == SCM_RIGHTS) {
					qfds = datalen / sizeof (struct file *);
					rp = data;
					for (i = 0; i < qfds; i++)
						(*op)(*rp++);
				}

				if (CMSG_SPACE(datalen) < clen) {
					clen -= CMSG_SPACE(datalen);
					cm = (struct cmsghdr *)
					    ((caddr_t)cm + CMSG_SPACE(datalen));
				} else {
					clen = 0;
					cm = NULL;
				}
			}
		}
		m0 = m0->m_act;
	}
}

static void
unp_mark(struct file *fp)
{
	if (fp->f_gcflag & FMARK)
		return;
	unp_defer++;
	fp->f_gcflag |= (FMARK|FDEFER);
}

static void
unp_discard(struct file *fp)
{
	UNP_LOCK();
	FILE_LOCK(fp);
	fp->f_msgcount--;
	unp_rights--;
	FILE_UNLOCK(fp);
	UNP_UNLOCK();
	(void) closef(fp, (struct thread *)NULL);
}
