/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket2.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_param.h"

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>

/*
 * Primitive routines for operating on sockets.
 */

/*
 * Procedures to manipulate state flags of socket
 * and do appropriate wakeups.  Normal sequence from the
 * active (originating) side is that soisconnecting() is
 * called during processing of connect() call,
 * resulting in an eventual call to soisconnected() if/when the
 * connection is established.  When the connection is torn down
 * soisdisconnecting() is called during processing of disconnect() call,
 * and soisdisconnected() is called when the connection to the peer
 * is totally severed.  The semantics of these routines are such that
 * connectionless protocols can call soisconnected() and soisdisconnected()
 * only, bypassing the in-progress calls when setting up a ``connection''
 * takes no time.
 *
 * From the passive side, a socket is created with
 * two queues of sockets: so_incomp for connections in progress
 * and so_comp for connections already made and awaiting user acceptance.
 * As a protocol is preparing incoming connections, it creates a socket
 * structure queued on so_incomp by calling sonewconn().  When the connection
 * is established, soisconnected() is called, and transfers the
 * socket structure to so_comp, making it available to accept().
 *
 * If a socket is closed with sockets on either
 * so_incomp or so_comp, these sockets are dropped.
 *
 * If higher level protocols are implemented in
 * the kernel, the wakeups done here will sometimes
 * cause software-interrupt process scheduling.
 */

void
soisconnecting(so)
	register struct socket *so;
{

	SOCK_LOCK(so);
	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
	SOCK_UNLOCK(so);
}

void
soisconnected(so)
	struct socket *so;
{
	struct socket *head;

	ACCEPT_LOCK();
	SOCK_LOCK(so);
	so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING|SS_ISCONFIRMING);
	so->so_state |= SS_ISCONNECTED;
	head = so->so_head;
	if (head != NULL && (so->so_qstate & SQ_INCOMP)) {
		if ((so->so_options & SO_ACCEPTFILTER) == 0) {
			SOCK_UNLOCK(so);
			TAILQ_REMOVE(&head->so_incomp, so, so_list);
			head->so_incqlen--;
			so->so_qstate &= ~SQ_INCOMP;
			TAILQ_INSERT_TAIL(&head->so_comp, so, so_list);
			head->so_qlen++;
			so->so_qstate |= SQ_COMP;
			ACCEPT_UNLOCK();
			sorwakeup(head);
			wakeup_one(&head->so_timeo);
		} else {
			ACCEPT_UNLOCK();
			so->so_upcall =
			    head->so_accf->so_accept_filter->accf_callback;
			so->so_upcallarg = head->so_accf->so_accept_filter_arg;
			so->so_rcv.sb_flags |= SB_UPCALL;
			so->so_options &= ~SO_ACCEPTFILTER;
			SOCK_UNLOCK(so);
			so->so_upcall(so, so->so_upcallarg, M_DONTWAIT);
		}
		return;
	}
	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();
	wakeup(&so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
}

void
soisdisconnecting(so)
	register struct socket *so;
{

	/*
	 * Note: This code assumes that SOCK_LOCK(so) and
	 * SOCKBUF_LOCK(&so->so_rcv) are the same.
	 */
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |= SS_ISDISCONNECTING;
	so->so_rcv.sb_state |= SBS_CANTRCVMORE;
	sorwakeup_locked(so);
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_state |= SBS_CANTSENDMORE;
	sowwakeup_locked(so);
	wakeup(&so->so_timeo);
}

void
soisdisconnected(so)
	register struct socket *so;
{

	/*
	 * Note: This code assumes that SOCK_LOCK(so) and
	 * SOCKBUF_LOCK(&so->so_rcv) are the same.
	 */
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISDISCONNECTED;
	so->so_rcv.sb_state |= SBS_CANTRCVMORE;
	sorwakeup_locked(so);
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_state |= SBS_CANTSENDMORE;
	sbdrop_locked(&so->so_snd, so->so_snd.sb_cc);
	sowwakeup_locked(so);
	wakeup(&so->so_timeo);
}

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation on a socket buffer.
 */
struct mbuf *
sbcreatecontrol(p, size, type, level)
	caddr_t p;
	register int size;
	int type, level;
{
	register struct cmsghdr *cp;
	struct mbuf *m;

	if (CMSG_SPACE((u_int)size) > MCLBYTES)
		return ((struct mbuf *) NULL);
	if (CMSG_SPACE((u_int)size) > MLEN)
		m = m_getcl(M_DONTWAIT, MT_CONTROL, 0);
	else
		m = m_get(M_DONTWAIT, MT_CONTROL);
	if (m == NULL)
		return ((struct mbuf *) NULL);
	cp = mtod(m, struct cmsghdr *);
	m->m_len = 0;
	KASSERT(CMSG_SPACE((u_int)size) <= M_TRAILINGSPACE(m),
	    ("sbcreatecontrol: short mbuf"));
	if (p != NULL)
		(void)memcpy(CMSG_DATA(cp), p, size);
	m->m_len = CMSG_SPACE(size);
	cp->cmsg_len = CMSG_LEN(size);
	cp->cmsg_level = level;
	cp->cmsg_type = type;
	return (m);
}

/*
 * Some routines that return EOPNOTSUPP for entry points that are not
 * supported by a protocol.  Fill in as needed.
 */
int
pru_accept_notsupp(struct socket *so, struct sockaddr **nam)
{
	return EOPNOTSUPP;
}

int
pru_attach_notsupp(struct socket *so, int proto, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_bind_notsupp(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_connect_notsupp(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_connect2_notsupp(struct socket *so1, struct socket *so2)
{
	return EOPNOTSUPP;
}

int
pru_control_notsupp(struct socket *so, u_long cmd, caddr_t data,
	struct ifnet *ifp, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_disconnect_notsupp(struct socket *so)
{
	return EOPNOTSUPP;
}

int
pru_listen_notsupp(struct socket *so, int backlog, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_peeraddr_notsupp(struct socket *so, struct sockaddr **nam)
{
	return EOPNOTSUPP;
}

int
pru_rcvd_notsupp(struct socket *so, int flags)
{
	return EOPNOTSUPP;
}

int
pru_rcvoob_notsupp(struct socket *so, struct mbuf *m, int flags)
{
	return EOPNOTSUPP;
}

int
pru_send_notsupp(struct socket *so, int flags, struct mbuf *m,
	struct sockaddr *addr, struct mbuf *control, struct thread *td)
{
	return EOPNOTSUPP;
}

/*
 * This isn't really a ``null'' operation, but it's the default one
 * and doesn't do anything destructive.
 */
int
pru_sense_null(struct socket *so, struct stat *sb)
{
	sb->st_blksize = so->so_snd.sb_hiwat;
	return 0;
}

int
pru_shutdown_notsupp(struct socket *so)
{
	return EOPNOTSUPP;
}

int
pru_sockaddr_notsupp(struct socket *so, struct sockaddr **nam)
{
	return EOPNOTSUPP;
}

int
pru_sosend_notsupp(struct socket *so, struct sockaddr *addr, struct uio *uio,
	struct mbuf *top, struct mbuf *control, int flags, struct thread *td)
{
	return EOPNOTSUPP;
}

int
pru_soreceive_notsupp(struct socket *so, struct sockaddr **paddr,
	struct uio *uio, struct mbuf **mp0, struct mbuf **controlp,
	int *flagsp)
{
	return EOPNOTSUPP;
}

int
pru_sopoll_notsupp(struct socket *so, int events, struct ucred *cred,
	struct thread *td)
{
	return EOPNOTSUPP;
}

/*
 * Make a copy of a sockaddr in a malloced buffer of type M_SONAME.
 */
struct sockaddr *
sodupsockaddr(const struct sockaddr *sa, int mflags)
{
	struct sockaddr *sa2;

	sa2 = malloc(sa->sa_len, M_SONAME, mflags);
	if (sa2)
		bcopy(sa, sa2, sa->sa_len);
	return sa2;
}

/*
 * Create an external-format (``xsocket'') structure using the information
 * in the kernel-format socket structure pointed to by so.  This is done
 * to reduce the spew of irrelevant information over this interface,
 * to isolate user code from changes in the kernel structure, and
 * potentially to provide information-hiding if we decide that
 * some of this information should be hidden from users.
 */
void
sotoxsocket(struct socket *so, struct xsocket *xso)
{
	xso->xso_len = sizeof *xso;
	xso->xso_so = so;
	xso->so_type = so->so_type;
	xso->so_options = so->so_options;
	xso->so_linger = so->so_linger;
	xso->so_state = so->so_state;
	xso->so_pcb = so->so_pcb;
	xso->xso_protocol = so->so_proto->pr_protocol;
	xso->xso_family = so->so_proto->pr_domain->dom_family;
	xso->so_qlen = so->so_qlen;
	xso->so_incqlen = so->so_incqlen;
	xso->so_qlimit = so->so_qlimit;
	xso->so_timeo = so->so_timeo;
	xso->so_error = so->so_error;
	xso->so_pgid = so->so_sigio ? so->so_sigio->sio_pgid : 0;
	xso->so_oobmark = so->so_oobmark;
	sbtoxsockbuf(&so->so_snd, &xso->so_snd);
	sbtoxsockbuf(&so->so_rcv, &xso->so_rcv);
	xso->so_uid = so->so_cred->cr_uid;
}

/*
 * This does the same for sockbufs.  Note that the xsockbuf structure,
 * since it is always embedded in a socket, does not include a self
 * pointer nor a length.  We make this entry point public in case
 * some other mechanism needs it.
 */
void
sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb)
{
	xsb->sb_cc = sb->sb_cc;
	xsb->sb_hiwat = sb->sb_hiwat;
	xsb->sb_mbcnt = sb->sb_mbcnt;
	xsb->sb_mbmax = sb->sb_mbmax;
	xsb->sb_lowat = sb->sb_lowat;
	xsb->sb_flags = sb->sb_flags;
	xsb->sb_timeo = sb->sb_timeo;
}
