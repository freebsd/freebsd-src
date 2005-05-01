/*-
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	From: @(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 * $FreeBSD$
 */

#include "opt_ipsec.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif /* INET6 */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/jail.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

/*
 * TCP protocol interface to socket abstraction.
 */
extern	char *tcpstates[];	/* XXX ??? */

static int	tcp_attach(struct socket *);
static int	tcp_connect(struct tcpcb *, struct sockaddr *,
		    struct thread *td);
#ifdef INET6
static int	tcp6_connect(struct tcpcb *, struct sockaddr *,
		    struct thread *td);
#endif /* INET6 */
static struct tcpcb *
		tcp_disconnect(struct tcpcb *);
static struct tcpcb *
		tcp_usrclosed(struct tcpcb *);
static void	tcp_fill_info(struct tcpcb *, struct tcp_info *);

#ifdef TCPDEBUG
#define	TCPDEBUG0	int ostate = 0
#define	TCPDEBUG1()	ostate = tp ? tp->t_state : 0
#define	TCPDEBUG2(req)	if (tp && (so->so_options & SO_DEBUG)) \
				tcp_trace(TA_USER, ostate, tp, 0, 0, req)
#else
#define	TCPDEBUG0
#define	TCPDEBUG1()
#define	TCPDEBUG2(req)
#endif

/*
 * TCP attaches to socket via pru_attach(), reserving space,
 * and an internet control block.
 */
static int
tcp_usr_attach(struct socket *so, int proto, struct thread *td)
{
	int error;
	struct inpcb *inp;
	struct tcpcb *tp = 0;
	TCPDEBUG0;

	INP_INFO_WLOCK(&tcbinfo);
	TCPDEBUG1();
	inp = sotoinpcb(so);
	if (inp) {
		error = EISCONN;
		goto out;
	}

	error = tcp_attach(so);
	if (error)
		goto out;

	if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		so->so_linger = TCP_LINGERTIME;

	inp = sotoinpcb(so);
	tp = intotcpcb(inp);
out:
	TCPDEBUG2(PRU_ATTACH);
	INP_INFO_WUNLOCK(&tcbinfo);
	return error;
}

/*
 * pru_detach() detaches the TCP protocol from the socket.
 * If the protocol state is non-embryonic, then can't
 * do this directly: have to initiate a pru_disconnect(),
 * which may finish later; embryonic TCB's can just
 * be discarded here.
 */
static int
tcp_usr_detach(struct socket *so)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	TCPDEBUG0;

	INP_INFO_WLOCK(&tcbinfo);
	inp = sotoinpcb(so);
	if (inp == NULL) {
		INP_INFO_WUNLOCK(&tcbinfo);
		return error;
	}
	INP_LOCK(inp);
	tp = intotcpcb(inp);
	TCPDEBUG1();
	tp = tcp_disconnect(tp);

	TCPDEBUG2(PRU_DETACH);
	if (tp)
		INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&tcbinfo);
	return error;
}

#define INI_NOLOCK	0
#define INI_READ	1
#define INI_WRITE	2

#define	COMMON_START()						\
	TCPDEBUG0;						\
	do {							\
		if (inirw == INI_READ)				\
			INP_INFO_RLOCK(&tcbinfo);		\
		else if (inirw == INI_WRITE)			\
			INP_INFO_WLOCK(&tcbinfo);		\
		inp = sotoinpcb(so);				\
		if (inp == 0) {					\
			if (inirw == INI_READ)			\
				INP_INFO_RUNLOCK(&tcbinfo);	\
			else if (inirw == INI_WRITE)		\
				INP_INFO_WUNLOCK(&tcbinfo);	\
			return EINVAL;				\
		}						\
		INP_LOCK(inp);					\
		if (inirw == INI_READ)				\
			INP_INFO_RUNLOCK(&tcbinfo);		\
		tp = intotcpcb(inp);				\
		TCPDEBUG1();					\
} while(0)

#define COMMON_END(req)						\
out:	TCPDEBUG2(req);						\
	do {							\
		if (tp)						\
			INP_UNLOCK(inp);			\
		if (inirw == INI_WRITE)				\
			INP_INFO_WUNLOCK(&tcbinfo);		\
		return error;					\
		goto out;					\
} while(0)

/*
 * Give the socket an address.
 */
static int
tcp_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;
	const int inirw = INI_WRITE;

	sinp = (struct sockaddr_in *)nam;
	if (nam->sa_len != sizeof (*sinp))
		return (EINVAL);
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (sinp->sin_family == AF_INET &&
	    IN_MULTICAST(ntohl(sinp->sin_addr.s_addr)))
		return (EAFNOSUPPORT);

	COMMON_START();
	error = in_pcbbind(inp, nam, td->td_ucred);
	if (error)
		goto out;
	COMMON_END(PRU_BIND);
}

#ifdef INET6
static int
tcp6_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6p;
	const int inirw = INI_WRITE;

	sin6p = (struct sockaddr_in6 *)nam;
	if (nam->sa_len != sizeof (*sin6p))
		return (EINVAL);
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (sin6p->sin6_family == AF_INET6 &&
	    IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);

	COMMON_START();
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6p->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
		else if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6p);
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, (struct sockaddr *)&sin,
			    td->td_ucred);
			goto out;
		}
	}
	error = in6_pcbbind(inp, nam, td->td_ucred);
	if (error)
		goto out;
	COMMON_END(PRU_BIND);
}
#endif /* INET6 */

/*
 * Prepare to accept connections.
 */
static int
tcp_usr_listen(struct socket *so, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;

	COMMON_START();
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0 && inp->inp_lport == 0)
		error = in_pcbbind(inp, (struct sockaddr *)0, td->td_ucred);
	if (error == 0) {
		tp->t_state = TCPS_LISTEN;
		solisten_proto(so);
	}
	SOCK_UNLOCK(so);
	COMMON_END(PRU_LISTEN);
}

#ifdef INET6
static int
tcp6_usr_listen(struct socket *so, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;

	COMMON_START();
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0 && inp->inp_lport == 0) {
		inp->inp_vflag &= ~INP_IPV4;
		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0)
			inp->inp_vflag |= INP_IPV4;
		error = in6_pcbbind(inp, (struct sockaddr *)0, td->td_ucred);
	}
	if (error == 0) {
		tp->t_state = TCPS_LISTEN;
		solisten_proto(so);
	}
	SOCK_UNLOCK(so);
	COMMON_END(PRU_LISTEN);
}
#endif /* INET6 */

/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
static int
tcp_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;
	const int inirw = INI_WRITE;

	sinp = (struct sockaddr_in *)nam;
	if (nam->sa_len != sizeof (*sinp))
		return (EINVAL);
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (sinp->sin_family == AF_INET
	    && IN_MULTICAST(ntohl(sinp->sin_addr.s_addr)))
		return (EAFNOSUPPORT);
	if (jailed(td->td_ucred))
		prison_remote_ip(td->td_ucred, 0, &sinp->sin_addr.s_addr);

	COMMON_START();
	if ((error = tcp_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output(tp);
	COMMON_END(PRU_CONNECT);
}

#ifdef INET6
static int
tcp6_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6p;
	const int inirw = INI_WRITE;

	sin6p = (struct sockaddr_in6 *)nam;
	if (nam->sa_len != sizeof (*sin6p))
		return (EINVAL);
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (sin6p->sin6_family == AF_INET6
	    && IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);

	COMMON_START();
	if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
		struct sockaddr_in sin;

		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
			error = EINVAL;
			goto out;
		}

		in6_sin6_2_sin(&sin, sin6p);
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		if ((error = tcp_connect(tp, (struct sockaddr *)&sin, td)) != 0)
			goto out;
		error = tcp_output(tp);
		goto out;
	}
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	inp->inp_inc.inc_isipv6 = 1;
	if ((error = tcp6_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output(tp);
	COMMON_END(PRU_CONNECT);
}
#endif /* INET6 */

/*
 * Initiate disconnect from peer.
 * If connection never passed embryonic stage, just drop;
 * else if don't need to let data drain, then can just drop anyways,
 * else have to begin TCP shutdown process: mark socket disconnecting,
 * drain unread data, state switch to reflect user close, and
 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
 * when peer sends FIN and acks ours.
 *
 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
 */
static int
tcp_usr_disconnect(struct socket *so)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;

	COMMON_START();
	tp = tcp_disconnect(tp);
	COMMON_END(PRU_DISCONNECT);
}

/*
 * Accept a connection.  Essentially all the work is
 * done at higher levels; just return the address
 * of the peer, storing through addr.
 */
static int
tcp_usr_accept(struct socket *so, struct sockaddr **nam)
{
	int error = 0;
	struct inpcb *inp = NULL;
	struct tcpcb *tp = NULL;
	struct in_addr addr;
	in_port_t port = 0;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED) {
		error = ECONNABORTED;
		goto out;
	}

	INP_INFO_RLOCK(&tcbinfo);
	inp = sotoinpcb(so);
	if (!inp) {
		INP_INFO_RUNLOCK(&tcbinfo);
		return (EINVAL);
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&tcbinfo);
	tp = intotcpcb(inp);
	TCPDEBUG1();

	/*
	 * We inline in_setpeeraddr and COMMON_END here, so that we can
	 * copy the data of interest and defer the malloc until after we
	 * release the lock.
	 */
	port = inp->inp_fport;
	addr = inp->inp_faddr;

out:	TCPDEBUG2(PRU_ACCEPT);
	if (tp)
		INP_UNLOCK(inp);
	if (error == 0)
		*nam = in_sockaddr(port, &addr);
	return error;
}

#ifdef INET6
static int
tcp6_usr_accept(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *inp = NULL;
	int error = 0;
	struct tcpcb *tp = NULL;
	struct in_addr addr;
	struct in6_addr addr6;
	in_port_t port = 0;
	int v4 = 0;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED) {
		error = ECONNABORTED;
		goto out;
	}

	INP_INFO_RLOCK(&tcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_RUNLOCK(&tcbinfo);
		return (EINVAL);
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&tcbinfo);
	tp = intotcpcb(inp);
	TCPDEBUG1();
	/*
	 * We inline in6_mapped_peeraddr and COMMON_END here, so that we can
	 * copy the data of interest and defer the malloc until after we
	 * release the lock.
	 */
	if (inp->inp_vflag & INP_IPV4) {
		v4 = 1;
		port = inp->inp_fport;
		addr = inp->inp_faddr;
	} else {
		port = inp->inp_fport;
		addr6 = inp->in6p_faddr;
	}

out:	TCPDEBUG2(PRU_ACCEPT);
	if (tp)
		INP_UNLOCK(inp);
	if (error == 0) {
		if (v4)
			*nam = in6_v4mapsin6_sockaddr(port, &addr);
		else
			*nam = in6_sockaddr(port, &addr6);
	}
	return error;
}
#endif /* INET6 */

/*
 * This is the wrapper function for in_setsockaddr. We just pass down
 * the pcbinfo for in_setsockaddr to lock. We don't want to do the locking
 * here because in_setsockaddr will call malloc and can block.
 */
static int
tcp_sockaddr(struct socket *so, struct sockaddr **nam)
{
	return (in_setsockaddr(so, nam, &tcbinfo));
}

/*
 * This is the wrapper function for in_setpeeraddr. We just pass down
 * the pcbinfo for in_setpeeraddr to lock.
 */
static int
tcp_peeraddr(struct socket *so, struct sockaddr **nam)
{
	return (in_setpeeraddr(so, nam, &tcbinfo));
}

/*
 * Mark the connection as being incapable of further output.
 */
static int
tcp_usr_shutdown(struct socket *so)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;

	COMMON_START();
	socantsendmore(so);
	tp = tcp_usrclosed(tp);
	if (tp)
		error = tcp_output(tp);
	COMMON_END(PRU_SHUTDOWN);
}

/*
 * After a receive, possibly send window update to peer.
 */
static int
tcp_usr_rcvd(struct socket *so, int flags)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_READ;

	COMMON_START();
	tcp_output(tp);
	COMMON_END(PRU_RCVD);
}

/*
 * Do a send by putting data in output queue and updating urgent
 * marker if URG set.  Possibly send more data.  Unlike the other
 * pru_*() routines, the mbuf chains are our responsibility.  We
 * must either enqueue them or free them.  The other pru_* routines
 * generally are caller-frees.
 */
static int
tcp_usr_send(struct socket *so, int flags, struct mbuf *m,
	     struct sockaddr *nam, struct mbuf *control, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;
	int unlocked = 0;
#ifdef INET6
	int isipv6;
#endif
	TCPDEBUG0;

	/*
	 * Need write lock here because this function might call
	 * tcp_connect or tcp_usrclosed.
	 * We really want to have to this function upgrade from read lock
	 * to write lock.  XXX
	 */
	INP_INFO_WLOCK(&tcbinfo);
	inp = sotoinpcb(so);
	if (inp == NULL) {
		/*
		 * OOPS! we lost a race, the TCP session got reset after
		 * we checked SBS_CANTSENDMORE, eg: while doing uiomove or a
		 * network interrupt in the non-splnet() section of sosend().
		 */
		if (m)
			m_freem(m);
		if (control)
			m_freem(control);
		error = ECONNRESET;	/* XXX EPIPE? */
		tp = NULL;
		TCPDEBUG1();
		goto out;
	}
	INP_LOCK(inp);
#ifdef INET6
	isipv6 = nam && nam->sa_family == AF_INET6;
#endif /* INET6 */
	tp = intotcpcb(inp);
	TCPDEBUG1();
	if (control) {
		/* TCP doesn't do control messages (rights, creds, etc) */
		if (control->m_len) {
			m_freem(control);
			if (m)
				m_freem(m);
			error = EINVAL;
			goto out;
		}
		m_freem(control);	/* empty control, just free it */
	}
	if (!(flags & PRUS_OOB)) {
		sbappendstream(&so->so_snd, m);
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
			else
#endif /* INET6 */
			error = tcp_connect(tp, nam, td);
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}

		if (flags & PRUS_EOF) {
			/*
			 * Close the send side of the connection after
			 * the data is sent.
			 */
			socantsendmore(so);
			tp = tcp_usrclosed(tp);
		}
		INP_INFO_WUNLOCK(&tcbinfo);
		unlocked = 1;
		if (tp != NULL) {
			if (flags & PRUS_MORETOCOME)
				tp->t_flags |= TF_MORETOCOME;
			error = tcp_output(tp);
			if (flags & PRUS_MORETOCOME)
				tp->t_flags &= ~TF_MORETOCOME;
		}
	} else {
		SOCKBUF_LOCK(&so->so_snd);
		if (sbspace(&so->so_snd) < -512) {
			SOCKBUF_UNLOCK(&so->so_snd);
			m_freem(m);
			error = ENOBUFS;
			goto out;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappendstream_locked(&so->so_snd, m);
		SOCKBUF_UNLOCK(&so->so_snd);
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
			else
#endif /* INET6 */
			error = tcp_connect(tp, nam, td);
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}
		INP_INFO_WUNLOCK(&tcbinfo);
		unlocked = 1;
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_force = 1;
		error = tcp_output(tp);
		tp->t_force = 0;
	}
out:
	TCPDEBUG2((flags & PRUS_OOB) ? PRU_SENDOOB :
		  ((flags & PRUS_EOF) ? PRU_SEND_EOF : PRU_SEND));
	if (tp)
		INP_UNLOCK(inp);
	if (!unlocked)
		INP_INFO_WUNLOCK(&tcbinfo);
	return (error);
}

/*
 * Abort the TCP.
 */
static int
tcp_usr_abort(struct socket *so)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_WRITE;

	COMMON_START();
	tp = tcp_drop(tp, ECONNABORTED);
	COMMON_END(PRU_ABORT);
}

/*
 * Receive out-of-band data.
 */
static int
tcp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	const int inirw = INI_READ;

	COMMON_START();
	if ((so->so_oobmark == 0 &&
	     (so->so_rcv.sb_state & SBS_RCVATMARK) == 0) ||
	    so->so_options & SO_OOBINLINE ||
	    tp->t_oobflags & TCPOOB_HADDATA) {
		error = EINVAL;
		goto out;
	}
	if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
		error = EWOULDBLOCK;
		goto out;
	}
	m->m_len = 1;
	*mtod(m, caddr_t) = tp->t_iobc;
	if ((flags & MSG_PEEK) == 0)
		tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
	COMMON_END(PRU_RCVOOB);
}

struct pr_usrreqs tcp_usrreqs = {
	.pru_abort =		tcp_usr_abort,
	.pru_accept =		tcp_usr_accept,
	.pru_attach =		tcp_usr_attach,
	.pru_bind =		tcp_usr_bind,
	.pru_connect =		tcp_usr_connect,
	.pru_control =		in_control,
	.pru_detach =		tcp_usr_detach,
	.pru_disconnect =	tcp_usr_disconnect,
	.pru_listen =		tcp_usr_listen,
	.pru_peeraddr =		tcp_peeraddr,
	.pru_rcvd =		tcp_usr_rcvd,
	.pru_rcvoob =		tcp_usr_rcvoob,
	.pru_send =		tcp_usr_send,
	.pru_shutdown =		tcp_usr_shutdown,
	.pru_sockaddr =		tcp_sockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel
};

#ifdef INET6
struct pr_usrreqs tcp6_usrreqs = {
	.pru_abort =		tcp_usr_abort,
	.pru_accept =		tcp6_usr_accept,
	.pru_attach =		tcp_usr_attach,
	.pru_bind =		tcp6_usr_bind,
	.pru_connect =		tcp6_usr_connect,
	.pru_control =		in6_control,
	.pru_detach =		tcp_usr_detach,
	.pru_disconnect =	tcp_usr_disconnect,
	.pru_listen =		tcp6_usr_listen,
	.pru_peeraddr =		in6_mapped_peeraddr,
	.pru_rcvd =		tcp_usr_rcvd,
	.pru_rcvoob =		tcp_usr_rcvoob,
	.pru_send =		tcp_usr_send,
	.pru_shutdown =		tcp_usr_shutdown,
	.pru_sockaddr =		in6_mapped_sockaddr,
 	.pru_sosetlabel =	in_pcbsosetlabel
};
#endif /* INET6 */

/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct sockaddr_in in mbuf *nam.  Call in_pcbbind to assign a local
 * port number if needed.  Call in_pcbconnect_setup to do the routing and
 * to choose a local host address (interface).  If there is an existing
 * incarnation of the same connection in TIME-WAIT state and if the remote
 * host was sending CC options and if the connection duration was < MSL, then
 * truncate the previous TIME-WAIT state and proceed.
 * Initialize connection parameters and enter SYN-SENT state.
 */
static int
tcp_connect(tp, nam, td)
	register struct tcpcb *tp;
	struct sockaddr *nam;
	struct thread *td;
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct in_addr laddr;
	u_short lport;
	int error;

	if (inp->inp_lport == 0) {
		error = in_pcbbind(inp, (struct sockaddr *)0, td->td_ucred);
		if (error)
			return error;
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 */
	laddr = inp->inp_laddr;
	lport = inp->inp_lport;
	error = in_pcbconnect_setup(inp, nam, &laddr.s_addr, &lport,
	    &inp->inp_faddr.s_addr, &inp->inp_fport, &oinp, td->td_ucred);
	if (error && oinp == NULL)
		return error;
	if (oinp)
		return EADDRINUSE;
	inp->inp_laddr = laddr;
	in_pcbrehash(inp);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	tcpstat.tcps_connattempt++;
	tp->t_state = TCPS_SYN_SENT;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);
	tp->iss = tcp_new_isn(tp);
	tp->t_bw_rtseq = tp->iss;
	tcp_sendseqinit(tp);

	return 0;
}

#ifdef INET6
static int
tcp6_connect(tp, nam, td)
	register struct tcpcb *tp;
	struct sockaddr *nam;
	struct thread *td;
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
	struct in6_addr *addr6;
	int error;

	if (inp->inp_lport == 0) {
		error = in6_pcbbind(inp, (struct sockaddr *)0, td->td_ucred);
		if (error)
			return error;
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 */
	error = in6_pcbladdr(inp, nam, &addr6);
	if (error)
		return error;
	oinp = in6_pcblookup_hash(inp->inp_pcbinfo,
				  &sin6->sin6_addr, sin6->sin6_port,
				  IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)
				  ? addr6
				  : &inp->in6p_laddr,
				  inp->inp_lport,  0, NULL);
	if (oinp)
		return EADDRINUSE;
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		inp->in6p_laddr = *addr6;
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	/* update flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
	if (inp->in6p_flags & IN6P_AUTOFLOWLABEL)
		inp->in6p_flowinfo |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
	in_pcbrehash(inp);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	tcpstat.tcps_connattempt++;
	tp->t_state = TCPS_SYN_SENT;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);
	tp->iss = tcp_new_isn(tp);
	tp->t_bw_rtseq = tp->iss;
	tcp_sendseqinit(tp);

	return 0;
}
#endif /* INET6 */

/*
 * Export TCP internal state information via a struct tcp_info, based on the
 * Linux 2.6 API.  Not ABI compatible as our constants are mapped differently
 * (TCP state machine, etc).  We export all information using FreeBSD-native
 * constants -- for example, the numeric values for tcpi_state will differ
 * from Linux.
 */
static void
tcp_fill_info(tp, ti)
	struct tcpcb *tp;
	struct tcp_info *ti;
{

	INP_LOCK_ASSERT(tp->t_inpcb);
	bzero(ti, sizeof(*ti));

	ti->tcpi_state = tp->t_state;
	if ((tp->t_flags & TF_REQ_TSTMP) && (tp->t_flags & TF_RCVD_TSTMP))
		ti->tcpi_options |= TCPI_OPT_TIMESTAMPS;
	if (tp->sack_enable)
		ti->tcpi_options |= TCPI_OPT_SACK;
	if ((tp->t_flags & TF_REQ_SCALE) && (tp->t_flags & TF_RCVD_SCALE)) {
		ti->tcpi_options |= TCPI_OPT_WSCALE;
		ti->tcpi_snd_wscale = tp->snd_scale;
		ti->tcpi_rcv_wscale = tp->rcv_scale;
	}
	ti->tcpi_snd_ssthresh = tp->snd_ssthresh;
	ti->tcpi_snd_cwnd = tp->snd_cwnd;

	/*
	 * FreeBSD-specific extension fields for tcp_info.
	 */
	ti->tcpi_rcv_space = tp->rcv_wnd;
	ti->tcpi_snd_wnd = tp->snd_wnd;
	ti->tcpi_snd_bwnd = tp->snd_bwnd;
}

/*
 * The new sockopt interface makes it possible for us to block in the
 * copyin/out step (if we take a page fault).  Taking a page fault at
 * splnet() is probably a Bad Thing.  (Since sockets and pcbs both now
 * use TSM, there probably isn't any need for this function to run at
 * splnet() any more.  This needs more examination.)
 *
 * XXXRW: The locking here is wrong; we may take a page fault while holding
 * the inpcb lock.
 */
int
tcp_ctloutput(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int	error, opt, optval;
	struct	inpcb *inp;
	struct	tcpcb *tp;
	struct	tcp_info ti;

	error = 0;
	INP_INFO_RLOCK(&tcbinfo);
	inp = sotoinpcb(so);
	if (inp == NULL) {
		INP_INFO_RUNLOCK(&tcbinfo);
		return (ECONNRESET);
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&tcbinfo);
	if (sopt->sopt_level != IPPROTO_TCP) {
		INP_UNLOCK(inp);
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6))
			error = ip6_ctloutput(so, sopt);
		else
#endif /* INET6 */
		error = ip_ctloutput(so, sopt);
		return (error);
	}
	tp = intotcpcb(inp);

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			if (optval > 0)
				tp->t_flags |= TF_SIGNATURE;
			else
				tp->t_flags &= ~TF_SIGNATURE;
			break;
#endif /* TCP_SIGNATURE */
		case TCP_NODELAY:
		case TCP_NOOPT:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			switch (sopt->sopt_name) {
			case TCP_NODELAY:
				opt = TF_NODELAY;
				break;
			case TCP_NOOPT:
				opt = TF_NOOPT;
				break;
			default:
				opt = 0; /* dead code to fool gcc */
				break;
			}

			if (optval)
				tp->t_flags |= opt;
			else
				tp->t_flags &= ~opt;
			break;

		case TCP_NOPUSH:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			if (optval)
				tp->t_flags |= TF_NOPUSH;
			else {
				tp->t_flags &= ~TF_NOPUSH;
				error = tcp_output(tp);
			}
			break;

		case TCP_MAXSEG:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			if (optval > 0 && optval <= tp->t_maxseg &&
			    optval + 40 >= tcp_minmss)
				tp->t_maxseg = optval;
			else
				error = EINVAL;
			break;

		case TCP_INFO:
			error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		switch (sopt->sopt_name) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			optval = (tp->t_flags & TF_SIGNATURE) ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
#endif
		case TCP_NODELAY:
			optval = tp->t_flags & TF_NODELAY;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_MAXSEG:
			optval = tp->t_maxseg;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOOPT:
			optval = tp->t_flags & TF_NOOPT;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOPUSH:
			optval = tp->t_flags & TF_NOPUSH;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_INFO:
			tcp_fill_info(tp, &ti);
			error = sooptcopyout(sopt, &ti, sizeof ti);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	INP_UNLOCK(inp);
	return (error);
}

/*
 * tcp_sendspace and tcp_recvspace are the default send and receive window
 * sizes, respectively.  These are obsolescent (this information should
 * be set by the route).
 */
u_long	tcp_sendspace = 1024*32;
SYSCTL_INT(_net_inet_tcp, TCPCTL_SENDSPACE, sendspace, CTLFLAG_RW,
    &tcp_sendspace , 0, "Maximum outgoing TCP datagram size");
u_long	tcp_recvspace = 1024*64;
SYSCTL_INT(_net_inet_tcp, TCPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &tcp_recvspace , 0, "Maximum incoming TCP datagram size");

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
static int
tcp_attach(so)
	struct socket *so;
{
	register struct tcpcb *tp;
	struct inpcb *inp;
	int error;
#ifdef INET6
	int isipv6 = INP_CHECK_SOCKAF(so, AF_INET6) != 0;
#endif

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return (error);
	}
	error = in_pcballoc(so, &tcbinfo, "tcpinp");
	if (error)
		return (error);
	inp = sotoinpcb(so);
#ifdef INET6
	if (isipv6) {
		inp->inp_vflag |= INP_IPV6;
		inp->in6p_hops = -1;	/* use kernel default */
	}
	else
#endif
	inp->inp_vflag |= INP_IPV4;
	tp = tcp_newtcpcb(inp);
	if (tp == 0) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
#ifdef INET6
		if (isipv6)
			in6_pcbdetach(inp);
		else
#endif
		in_pcbdetach(inp);
		so->so_state |= nofd;
		return (ENOBUFS);
	}
	tp->t_state = TCPS_CLOSED;
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
static struct tcpcb *
tcp_disconnect(tp)
	register struct tcpcb *tp;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (tp->t_state < TCPS_ESTABLISHED)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
static struct tcpcb *
tcp_usrclosed(tp)
	register struct tcpcb *tp;
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_SENT:
	case TCPS_SYN_RECEIVED:
		tp->t_flags |= TF_NEEDFIN;
		break;

	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2) {
		soisdisconnected(tp->t_inpcb->inp_socket);
		/* To prevent the connection hanging in FIN_WAIT_2 forever. */
		if (tp->t_state == TCPS_FIN_WAIT_2)
			callout_reset(tp->tt_2msl, tcp_maxidle,
				      tcp_timer_2msl, tp);
	}
	return (tp);
}
