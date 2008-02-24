/*-
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2004-2006 Robert N. M. Watson
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
 * Copyright (c) 1995, Mike Mitchell
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
 *	@(#)ipx_usrreq.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netipx/ipx_usrreq.c,v 1.62 2007/06/13 14:01:43 rwatson Exp $");

#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>

/*
 * IPX protocol implementation.
 */

static int ipxsendspace = IPXSNDQ;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxsendspace, CTLFLAG_RW,
            &ipxsendspace, 0, "");
static int ipxrecvspace = IPXRCVQ;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxrecvspace, CTLFLAG_RW,
            &ipxrecvspace, 0, "");

static	void ipx_usr_abort(struct socket *so);
static	int ipx_attach(struct socket *so, int proto, struct thread *td);
static	int ipx_bind(struct socket *so, struct sockaddr *nam, struct thread *td);
static	int ipx_connect(struct socket *so, struct sockaddr *nam,
			struct thread *td);
static	void ipx_detach(struct socket *so);
static	int ipx_disconnect(struct socket *so);
static	int ipx_send(struct socket *so, int flags, struct mbuf *m,
		     struct sockaddr *addr, struct mbuf *control,
		     struct thread *td);
static	int ipx_shutdown(struct socket *so);
static	int ripx_attach(struct socket *so, int proto, struct thread *td);
static	int ipx_output(struct ipxpcb *ipxp, struct mbuf *m0);
static	void ipx_usr_close(struct socket *so);

struct	pr_usrreqs ipx_usrreqs = {
	.pru_abort =		ipx_usr_abort,
	.pru_attach =		ipx_attach,
	.pru_bind =		ipx_bind,
	.pru_connect =		ipx_connect,
	.pru_control =		ipx_control,
	.pru_detach =		ipx_detach,
	.pru_disconnect =	ipx_disconnect,
	.pru_peeraddr =		ipx_peeraddr,
	.pru_send =		ipx_send,
	.pru_shutdown =		ipx_shutdown,
	.pru_sockaddr =		ipx_sockaddr,
	.pru_close =		ipx_usr_close,
};

struct	pr_usrreqs ripx_usrreqs = {
	.pru_abort =		ipx_usr_abort,
	.pru_attach =		ripx_attach,
	.pru_bind =		ipx_bind,
	.pru_connect =		ipx_connect,
	.pru_control =		ipx_control,
	.pru_detach =		ipx_detach,
	.pru_disconnect =	ipx_disconnect,
	.pru_peeraddr =		ipx_peeraddr,
	.pru_send =		ipx_send,
	.pru_shutdown =		ipx_shutdown,
	.pru_sockaddr =		ipx_sockaddr,
	.pru_close =		ipx_usr_close,
};

/*
 *  This may also be called for raw listeners.
 */
void
ipx_input(struct mbuf *m, struct ipxpcb *ipxp)
{
	struct ipx *ipx = mtod(m, struct ipx *);
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct sockaddr_ipx ipx_ipx;

	KASSERT(ipxp != NULL, ("ipx_input: NULL ipxpcb"));
	IPX_LOCK_ASSERT(ipxp);
	/*
	 * Construct sockaddr format source address.
	 * Stuff source address and datagram in user buffer.
	 */
	ipx_ipx.sipx_len = sizeof(ipx_ipx);
	ipx_ipx.sipx_family = AF_IPX;
	ipx_ipx.sipx_addr = ipx->ipx_sna;
	ipx_ipx.sipx_zero[0] = '\0';
	ipx_ipx.sipx_zero[1] = '\0';
	if (ipx_neteqnn(ipx->ipx_sna.x_net, ipx_zeronet) && ifp != NULL) {
		struct ifaddr *ifa;

		for (ifa = TAILQ_FIRST(&ifp->if_addrhead); ifa != NULL;
		     ifa = TAILQ_NEXT(ifa, ifa_link)) {
			if (ifa->ifa_addr->sa_family == AF_IPX) {
				ipx_ipx.sipx_addr.x_net =
					IA_SIPX(ifa)->sipx_addr.x_net;
				break;
			}
		}
	}
	ipxp->ipxp_rpt = ipx->ipx_pt;
	if ((ipxp->ipxp_flags & IPXP_RAWIN) == 0) {
		m->m_len -= sizeof(struct ipx);
		m->m_pkthdr.len -= sizeof(struct ipx);
		m->m_data += sizeof(struct ipx);
	}
	if (sbappendaddr(&ipxp->ipxp_socket->so_rcv,
	    (struct sockaddr *)&ipx_ipx, m, NULL) == 0)
		m_freem(m);
	else
		sorwakeup(ipxp->ipxp_socket);
}

/*
 * Drop connection, reporting
 * the specified error.
 */
void
ipx_drop(struct ipxpcb *ipxp, int errno)
{
	struct socket *so = ipxp->ipxp_socket;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	/*
	 * someday, in the IPX world
	 * we will generate error protocol packets
	 * announcing that the socket has gone away.
	 *
	 * XXX Probably never. IPX does not have error packets.
	 */
	/*if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		tcp_output(tp);
	}*/
	so->so_error = errno;
	ipx_pcbdisconnect(ipxp);
	soisdisconnected(so);
}

static int
ipx_output(struct ipxpcb *ipxp, struct mbuf *m0)
{
	struct ipx *ipx;
	struct socket *so;
	int len = 0;
	struct route *ro;
	struct mbuf *m;
	struct mbuf *mprev = NULL;

	IPX_LOCK_ASSERT(ipxp);

	/*
	 * Calculate data length.
	 */
	for (m = m0; m != NULL; m = m->m_next) {
		mprev = m;
		len += m->m_len;
	}
	/*
	 * Make sure packet is actually of even length.
	 */

	if (len & 1) {
		m = mprev;
		if ((m->m_flags & M_EXT) == 0 &&
			(m->m_len + m->m_data < &m->m_dat[MLEN])) {
			mtod(m, char*)[m->m_len++] = 0;
		} else {
			struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);

			if (m1 == NULL) {
				m_freem(m0);
				return (ENOBUFS);
			}
			m1->m_len = 1;
			* mtod(m1, char *) = 0;
			m->m_next = m1;
		}
		m0->m_pkthdr.len++;
	}

	/*
	 * Fill in mbuf with extended IPX header
	 * and addresses and length put into network format.
	 */
	m = m0;
	if (ipxp->ipxp_flags & IPXP_RAWOUT) {
		ipx = mtod(m, struct ipx *);
	} else {
		M_PREPEND(m, sizeof(struct ipx), M_DONTWAIT);
		if (m == NULL)
			return (ENOBUFS);
		ipx = mtod(m, struct ipx *);
		ipx->ipx_tc = 0;
		ipx->ipx_pt = ipxp->ipxp_dpt;
		ipx->ipx_sna = ipxp->ipxp_laddr;
		ipx->ipx_dna = ipxp->ipxp_faddr;
		len += sizeof(struct ipx);
	}

	ipx->ipx_len = htons((u_short)len);

	if (ipxp->ipxp_flags & IPXP_CHECKSUM) {
		ipx->ipx_sum = ipx_cksum(m, len);
	} else
		ipx->ipx_sum = 0xffff;

	/*
	 * Output datagram.
	 */
	so = ipxp->ipxp_socket;
	if (so->so_options & SO_DONTROUTE)
		return (ipx_outputfl(m, (struct route *)NULL,
		    (so->so_options & SO_BROADCAST) | IPX_ROUTETOIF));
	/*
	 * Use cached route for previous datagram if
	 * possible.  If the previous net was the same
	 * and the interface was a broadcast medium, or
	 * if the previous destination was identical,
	 * then we are ok.
	 *
	 * NB: We don't handle broadcasts because that
	 *     would require 3 subroutine calls.
	 */
	ro = &ipxp->ipxp_route;
#ifdef ancient_history
	/*
	 * I think that this will all be handled in ipx_pcbconnect!
	 */
	if (ro->ro_rt != NULL) {
		if(ipx_neteq(ipxp->ipxp_lastdst, ipx->ipx_dna)) {
			/*
			 * This assumes we have no GH type routes
			 */
			if (ro->ro_rt->rt_flags & RTF_HOST) {
				if (!ipx_hosteq(ipxp->ipxp_lastdst, ipx->ipx_dna))
					goto re_route;

			}
			if ((ro->ro_rt->rt_flags & RTF_GATEWAY) == 0) {
				struct ipx_addr *dst =
						&satoipx_addr(ro->ro_dst);
				dst->x_host = ipx->ipx_dna.x_host;
			}
			/*
			 * Otherwise, we go through the same gateway
			 * and dst is already set up.
			 */
		} else {
		re_route:
			RTFREE(ro->ro_rt);
			ro->ro_rt = NULL;
		}
	}
	ipxp->ipxp_lastdst = ipx->ipx_dna;
#endif /* ancient_history */
	return (ipx_outputfl(m, ro, so->so_options & SO_BROADCAST));
}

int
ipx_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);
	int mask, error, optval;
	short soptval;
	struct ipx ioptval;
	long seq;

	KASSERT(ipxp != NULL, ("ipx_ctloutput: ipxp == NULL"));
	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case SO_ALL_PACKETS:
			mask = IPXP_ALL_PACKETS;
			goto get_flags;

		case SO_HEADERS_ON_INPUT:
			mask = IPXP_RAWIN;
			goto get_flags;

		case SO_IPX_CHECKSUM:
			mask = IPXP_CHECKSUM;
			goto get_flags;

		case SO_HEADERS_ON_OUTPUT:
			mask = IPXP_RAWOUT;
		get_flags:
			/* Unlocked read. */
			soptval = ipxp->ipxp_flags & mask;
			error = sooptcopyout(sopt, &soptval, sizeof soptval);
			break;

		case SO_DEFAULT_HEADERS:
			ioptval.ipx_len = 0;
			ioptval.ipx_sum = 0;
			ioptval.ipx_tc = 0;
			IPX_LOCK(ipxp);
			ioptval.ipx_pt = ipxp->ipxp_dpt;
			ioptval.ipx_dna = ipxp->ipxp_faddr;
			ioptval.ipx_sna = ipxp->ipxp_laddr;
			IPX_UNLOCK(ipxp);
			error = sooptcopyout(sopt, &soptval, sizeof soptval);
			break;

		case SO_SEQNO:
			IPX_LIST_LOCK();
			seq = ipx_pexseq;
			ipx_pexseq++;
			IPX_LIST_UNLOCK();
			error = sooptcopyout(sopt, &seq, sizeof seq);
			break;

		default:
			error = EINVAL;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case SO_ALL_PACKETS:
			mask = IPXP_ALL_PACKETS;
			goto set_head;

		case SO_HEADERS_ON_INPUT:
			mask = IPXP_RAWIN;
			goto set_head;

		case SO_IPX_CHECKSUM:
			mask = IPXP_CHECKSUM;

		case SO_HEADERS_ON_OUTPUT:
			mask = IPXP_RAWOUT;
		set_head:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;
			IPX_LOCK(ipxp);
			if (optval)
				ipxp->ipxp_flags |= mask;
			else
				ipxp->ipxp_flags &= ~mask;
			IPX_UNLOCK(ipxp);
			break;

		case SO_DEFAULT_HEADERS:
			error = sooptcopyin(sopt, &ioptval, sizeof ioptval,
					    sizeof ioptval);
			if (error)
				break;
			/* Unlocked write. */
			ipxp->ipxp_dpt = ioptval.ipx_pt;
			break;
		default:
			error = EINVAL;
		}
		break;
	}
	return (error);
}

static void
ipx_usr_abort(struct socket *so)
{

	/* XXXRW: Possibly ipx_disconnect() here? */
	soisdisconnected(so);
}

static int
ipx_attach(struct socket *so, int proto, struct thread *td)
{
#ifdef INVARIANTS
	struct ipxpcb *ipxp = sotoipxpcb(so);
#endif
	int error;

	KASSERT(ipxp == NULL, ("ipx_attach: ipxp != NULL"));
	error = soreserve(so, ipxsendspace, ipxrecvspace);
	if (error != 0)
		return (error);
	IPX_LIST_LOCK();
	error = ipx_pcballoc(so, &ipxpcb_list, td);
	IPX_LIST_UNLOCK();
	return (error);
}

static int
ipx_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);
	int error;

	KASSERT(ipxp != NULL, ("ipx_bind: ipxp == NULL"));
	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	error = ipx_pcbbind(ipxp, nam, td);
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static void
ipx_usr_close(struct socket *so)
{

	/* XXXRW: Possibly ipx_disconnect() here? */
	soisdisconnected(so);
}

static int
ipx_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);
	int error;

	KASSERT(ipxp != NULL, ("ipx_connect: ipxp == NULL"));
	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (!ipx_nullhost(ipxp->ipxp_faddr)) {
		error = EISCONN;
		goto out;
	}
	error = ipx_pcbconnect(ipxp, nam, td);
	if (error == 0)
		soisconnected(so);
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static void
ipx_detach(struct socket *so)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	/* XXXRW: Should assert detached. */
	KASSERT(ipxp != NULL, ("ipx_detach: ipxp == NULL"));
	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	ipx_pcbdetach(ipxp);
	ipx_pcbfree(ipxp);
	IPX_LIST_UNLOCK();
}

static int
ipx_disconnect(struct socket *so)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);
	int error;

	KASSERT(ipxp != NULL, ("ipx_disconnect: ipxp == NULL"));
	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	error = 0;
	if (ipx_nullhost(ipxp->ipxp_faddr)) {
		error = ENOTCONN;
		goto out;
	}
	ipx_pcbdisconnect(ipxp);
	soisdisconnected(so);
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (0);
}

int
ipx_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	KASSERT(ipxp != NULL, ("ipx_peeraddr: ipxp == NULL"));
	ipx_getpeeraddr(ipxp, nam);
	return (0);
}

static int
ipx_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	int error;
	struct ipxpcb *ipxp = sotoipxpcb(so);
	struct ipx_addr laddr;

	KASSERT(ipxp != NULL, ("ipxp_send: ipxp == NULL"));
	/*
	 * Attempt to only acquire the necessary locks: if the socket is
	 * already connected, we don't need to hold the IPX list lock to be
	 * used by ipx_pcbconnect() and ipx_pcbdisconnect(), just the IPX
	 * pcb lock.
	 */
	if (nam != NULL) {
		IPX_LIST_LOCK();
		IPX_LOCK(ipxp);
		laddr = ipxp->ipxp_laddr;
		if (!ipx_nullhost(ipxp->ipxp_faddr)) {
			IPX_UNLOCK(ipxp);
			IPX_LIST_UNLOCK();
			error = EISCONN;
			goto send_release;
		}
		/*
		 * Must block input while temporarily connected.
		 */
		error = ipx_pcbconnect(ipxp, nam, td);
		if (error) {
			IPX_UNLOCK(ipxp);
			IPX_LIST_UNLOCK();
			goto send_release;
		}
	} else {
		IPX_LOCK(ipxp);
		if (ipx_nullhost(ipxp->ipxp_faddr)) {
			IPX_UNLOCK(ipxp);
			error = ENOTCONN;
			goto send_release;
		}
	}
	error = ipx_output(ipxp, m);
	m = NULL;
	if (nam != NULL) {
		ipx_pcbdisconnect(ipxp);
		ipxp->ipxp_laddr = laddr;
		IPX_UNLOCK(ipxp);
		IPX_LIST_UNLOCK();
	} else
		IPX_UNLOCK(ipxp);

send_release:
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
ipx_shutdown(so)
	struct socket *so;
{

	KASSERT(so->so_pcb != NULL, ("ipx_shutdown: so_pcb == NULL"));
	socantsendmore(so);
	return (0);
}

int
ipx_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	KASSERT(ipxp != NULL, ("ipx_sockaddr: ipxp == NULL"));
	ipx_getsockaddr(ipxp, nam);
	return (0);
}

static int
ripx_attach(struct socket *so, int proto, struct thread *td)
{
	int error = 0;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	KASSERT(ipxp == NULL, ("ripx_attach: ipxp != NULL"));

	if (td != NULL) {
		error = priv_check(td, PRIV_NETIPX_RAW);
		if (error)
			return (error);
	}

	/*
	 * We hold the IPX list lock for the duration as address parameters
	 * of the IPX pcb are changed.  Since no one else holds a reference
	 * to the ipxpcb yet, we don't need the ipxpcb lock here.
	 */
	IPX_LIST_LOCK();
	error = ipx_pcballoc(so, &ipxrawpcb_list, td);
	if (error)
		goto out;
	ipxp = sotoipxpcb(so);
	error = soreserve(so, ipxsendspace, ipxrecvspace);
	if (error)
		goto out;
	ipxp->ipxp_faddr.x_host = ipx_broadhost;
	ipxp->ipxp_flags = IPXP_RAWIN | IPXP_RAWOUT;
out:
	IPX_LIST_UNLOCK();
	return (error);
}
