/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ipx_usrreq.c
 *
 * $Id: ipx_usrreq.c,v 1.15 1997/06/26 19:35:58 jhay Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netipx/ipx.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_var.h>
#include <netipx/ipx_ip.h>

/*
 * IPX protocol implementation.
 */

int ipxsendspace = IPXSNDQ;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxsendspace, CTLFLAG_RW,
            &ipxsendspace, 0, "");
int ipxrecvspace = IPXRCVQ;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxrecvspace, CTLFLAG_RW,
            &ipxrecvspace, 0, "");

static	int ipx_usr_abort(struct socket *so);
static	int ipx_attach(struct socket *so, int proto, struct proc *p);
static	int ipx_bind(struct socket *so, struct sockaddr *nam, struct proc *p);
static	int ipx_connect(struct socket *so, struct sockaddr *nam,
			struct proc *p);
static	int ipx_detach(struct socket *so);
static	int ipx_disconnect(struct socket *so);
static	int ipx_send(struct socket *so, int flags, struct mbuf *m,
		     struct sockaddr *addr, struct mbuf *control, 
		     struct proc *p);
static	int ipx_shutdown(struct socket *so);
static	int ripx_attach(struct socket *so, int proto, struct proc *p);
static	int ipx_output(struct ipxpcb *ipxp, struct mbuf *m0);

struct	pr_usrreqs ipx_usrreqs = {
	ipx_usr_abort, pru_accept_notsupp, ipx_attach, ipx_bind,
	ipx_connect, pru_connect2_notsupp, ipx_control, ipx_detach,
	ipx_disconnect, pru_listen_notsupp, ipx_peeraddr, pru_rcvd_notsupp,
	pru_rcvoob_notsupp, ipx_send, pru_sense_null, ipx_shutdown,
	ipx_sockaddr, sosend, soreceive, soselect
};

struct	pr_usrreqs ripx_usrreqs = {
	ipx_usr_abort, pru_accept_notsupp, ripx_attach, ipx_bind,
	ipx_connect, pru_connect2_notsupp, ipx_control, ipx_detach,
	ipx_disconnect, pru_listen_notsupp, ipx_peeraddr, pru_rcvd_notsupp,
	pru_rcvoob_notsupp, ipx_send, pru_sense_null, ipx_shutdown,
	ipx_sockaddr, sosend, soreceive, soselect
};

/*
 *  This may also be called for raw listeners.
 */
void
ipx_input(m, ipxp)
	struct mbuf *m;
	register struct ipxpcb *ipxp;
{
	register struct ipx *ipx = mtod(m, struct ipx *);
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct sockaddr_ipx ipx_ipx;

	if (ipxp == NULL)
		panic("No ipxpcb");
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
		register struct ifaddr *ifa;

		for (ifa = ifp->if_addrhead.tqh_first; ifa != NULL; 
		     ifa = ifa->ifa_link.tqe_next) {
			if (ifa->ifa_addr->sa_family == AF_IPX) {
				ipx_ipx.sipx_addr.x_net =
					IA_SIPX(ifa)->sipx_addr.x_net;
				break;
			}
		}
	}
	ipxp->ipxp_rpt = ipx->ipx_pt;
	if (!(ipxp->ipxp_flags & IPXP_RAWIN) ) {
		m->m_len -= sizeof(struct ipx);
		m->m_pkthdr.len -= sizeof(struct ipx);
		m->m_data += sizeof(struct ipx);
	}
	if (sbappendaddr(&ipxp->ipxp_socket->so_rcv, (struct sockaddr *)&ipx_ipx,
	    m, (struct mbuf *)NULL) == 0)
		goto bad;
	sorwakeup(ipxp->ipxp_socket);
	return;
bad:
	m_freem(m);
}

void
ipx_abort(ipxp)
	struct ipxpcb *ipxp;
{
	struct socket *so = ipxp->ipxp_socket;

	ipx_pcbdisconnect(ipxp);
	soisdisconnected(so);
}

/*
 * Drop connection, reporting
 * the specified error.
 */
void
ipx_drop(ipxp, errno)
	register struct ipxpcb *ipxp;
	int errno;
{
	struct socket *so = ipxp->ipxp_socket;

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
ipx_output(ipxp, m0)
	struct ipxpcb *ipxp;
	struct mbuf *m0;
{
	register struct mbuf *m;
	register struct ipx *ipx;
	register struct socket *so;
	register int len = 0;
	register struct route *ro;
	struct mbuf *mprev = NULL;

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
			m->m_len++;
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

	if (ipxcksum) {
		ipx->ipx_sum = 0;
		len = ((len - 1) | 1) + 1;
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
				register struct ipx_addr *dst =
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
ipx_ctloutput(req, so, level, name, value, p)
	int req, level;
	struct socket *so;
	int name;
	struct mbuf **value;
	struct proc *p;
{
	register struct mbuf *m;
	struct ipxpcb *ipxp = sotoipxpcb(so);
	int mask, error = 0;

	if (ipxp == NULL)
		return (EINVAL);

	switch (req) {

	case PRCO_GETOPT:
		if (value == NULL)
			return (EINVAL);
		m = m_get(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		switch (name) {

		case SO_ALL_PACKETS:
			mask = IPXP_ALL_PACKETS;
			goto get_flags;

		case SO_HEADERS_ON_INPUT:
			mask = IPXP_RAWIN;
			goto get_flags;
			
		case SO_HEADERS_ON_OUTPUT:
			mask = IPXP_RAWOUT;
		get_flags:
			m->m_len = sizeof(short);
			*mtod(m, short *) = ipxp->ipxp_flags & mask;
			break;

		case SO_DEFAULT_HEADERS:
			m->m_len = sizeof(struct ipx);
			{
				register struct ipx *ipx = mtod(m, struct ipx *);
				ipx->ipx_len = 0;
				ipx->ipx_sum = 0;
				ipx->ipx_tc = 0;
				ipx->ipx_pt = ipxp->ipxp_dpt;
				ipx->ipx_dna = ipxp->ipxp_faddr;
				ipx->ipx_sna = ipxp->ipxp_laddr;
			}
			break;

		case SO_SEQNO:
			m->m_len = sizeof(long);
			*mtod(m, long *) = ipx_pexseq++;
			break;

		default:
			error = EINVAL;
		}
		*value = m;
		break;

	case PRCO_SETOPT:
		switch (name) {
			int *ok;

		case SO_ALL_PACKETS:
			mask = IPXP_ALL_PACKETS;
			goto set_head;

		case SO_HEADERS_ON_INPUT:
			mask = IPXP_RAWIN;
			goto set_head;

		case SO_HEADERS_ON_OUTPUT:
			mask = IPXP_RAWOUT;
		set_head:
			if (value && *value) {
				ok = mtod(*value, int *);
				if (*ok)
					ipxp->ipxp_flags |= mask;
				else
					ipxp->ipxp_flags &= ~mask;
			} else error = EINVAL;
			break;

		case SO_DEFAULT_HEADERS:
			{
				register struct ipx *ipx
				    = mtod(*value, struct ipx *);
				ipxp->ipxp_dpt = ipx->ipx_pt;
			}
			break;
#ifdef IPXIP
		case SO_IPXIP_ROUTE:
			error = ipxip_route(so, *value, p);
			break;
#endif /* IPXIP */
#ifdef IPXTUNNEL
		case SO_IPXTUNNEL_ROUTE
			error = ipxtun_route(so, *value, p);
			break;
#endif
		default:
			error = EINVAL;
		}
		if (value && *value)
			m_freem(*value);
		break;
	}
	return (error);
}

static int
ipx_usr_abort(so)
	struct socket *so;
{
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	s = splnet();
	ipx_pcbdetach(ipxp);
	splx(s);
	sofree(so);
	soisdisconnected(so);
	return (0);
}

static int
ipx_attach(so, proto, p)
	struct socket *so;
	int proto;
	struct proc *p;
{
	int error;
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	if (ipxp != NULL)
		return (EINVAL);
	s = splnet();
	error = ipx_pcballoc(so, &ipxpcb, p);
	splx(s);
	if (error == 0)
		error = soreserve(so, ipxsendspace, ipxrecvspace);
	return (error);
}

static int
ipx_bind(so, nam, p)
	struct socket *so;
	struct sockaddr *nam;
	struct proc *p;
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	return (ipx_pcbbind(ipxp, nam, p));
}

static int
ipx_connect(so, nam, p)
	struct socket *so;
	struct sockaddr *nam;
	struct proc *p;
{
	int error;
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	if (!ipx_nullhost(ipxp->ipxp_faddr))
		return (EISCONN);
	s = splnet();
	error = ipx_pcbconnect(ipxp, nam, p);
	splx(s);
	if (error == 0)
		soisconnected(so);
	return (error);
}

static int
ipx_detach(so)
	struct socket *so;
{
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	if (ipxp == NULL)
		return (ENOTCONN);
	s = splnet();
	ipx_pcbdetach(ipxp);
	splx(s);
	return (0);
}

static int
ipx_disconnect(so)
	struct socket *so;
{
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	if (ipx_nullhost(ipxp->ipxp_faddr))
		return (ENOTCONN);
	s = splnet();
	ipx_pcbdisconnect(ipxp);
	splx(s);
	soisdisconnected(so);
	return (0);
}

int
ipx_peeraddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	ipx_setpeeraddr(ipxp, nam); /* XXX what if alloc fails? */
	return (0);
}

static int
ipx_send(so, flags, m, nam, control, p)
	struct socket *so;
	int flags;
	struct mbuf *m;
	struct sockaddr *nam;
	struct mbuf *control;
	struct proc *p;
{
	int error;
	struct ipxpcb *ipxp = sotoipxpcb(so);
	struct ipx_addr laddr;
	int s = 0;

	if (nam != NULL) {
		laddr = ipxp->ipxp_laddr;
		if (!ipx_nullhost(ipxp->ipxp_faddr)) {
			error = EISCONN;
			goto send_release;
		}
		/*
		 * Must block input while temporarily connected.
		 */
		s = splnet();
		error = ipx_pcbconnect(ipxp, nam, p);
		if (error) {
			splx(s);
			goto send_release;
		}
	} else {
		if (ipx_nullhost(ipxp->ipxp_faddr)) {
			error = ENOTCONN;
			goto send_release;
		}
	}
	error = ipx_output(ipxp, m);
	m = NULL;
	if (nam != NULL) {
		ipx_pcbdisconnect(ipxp);
		splx(s);
		ipxp->ipxp_laddr.x_host = laddr.x_host;
		ipxp->ipxp_laddr.x_port = laddr.x_port;
	}

send_release:
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
ipx_shutdown(so)
	struct socket *so;
{
	socantsendmore(so);
	return (0);
}

int
ipx_sockaddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	struct ipxpcb *ipxp = sotoipxpcb(so);

	ipx_setsockaddr(ipxp, nam); /* XXX what if alloc fails? */
	return (0);
}

static int
ripx_attach(so, proto, p)
	struct socket *so;
	int proto;
	struct proc *p;
{
	int error = 0;
	int s;
	struct ipxpcb *ipxp = sotoipxpcb(so);

	if (p != NULL && (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	s = splnet();
	error = ipx_pcballoc(so, &ipxrawpcb, p);
	splx(s);
	if (error)
		return (error);
	error = soreserve(so, ipxsendspace, ipxrecvspace);
	if (error)
		return (error);
	ipxp = sotoipxpcb(so);
	ipxp->ipxp_faddr.x_host = ipx_broadhost;
	ipxp->ipxp_flags = IPXP_RAWIN | IPXP_RAWOUT;
	return (error);
}
