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
 *	@(#)ipx_ip.c
 *
 * $Id: ipx_ip.c,v 1.20 1998/02/09 06:10:20 eivind Exp $
 */

/*
 * Software interface driver for encapsulating IPX in IP.
 */

#include "opt_inet.h"
#include "opt_ipx.h"

#ifdef IPXIP
#ifndef INET
#error The option IPXIP requires option INET.
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_ip.h>
#include <netipx/ipx_var.h>

static struct	ifnet ipxipif;

/* list of all hosts and gateways or broadcast addrs */
static struct	ifnet_en *ipxip_list;

static	struct ifnet_en *ipxipattach(void);
static	int ipxip_free(struct ifnet *ifp);
static	int ipxipioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static	int ipxipoutput(struct ifnet *ifp, struct mbuf *m,
			struct sockaddr *dst, struct rtentry *rt);
static	void ipxip_rtchange(struct in_addr *dst);
static	void ipxipstart(struct ifnet *ifp);

static struct ifnet_en *
ipxipattach()
{
	register struct ifnet_en *m;
	register struct ifnet *ifp;

	if (ipxipif.if_mtu == 0) {
		ifp = &ipxipif;
		ifp->if_name = "ipxip";
		ifp->if_mtu = LOMTU;
		ifp->if_ioctl = ipxipioctl;
		ifp->if_output = ipxipoutput;
		ifp->if_start = ipxipstart;
		ifp->if_flags = IFF_POINTOPOINT;
	}

	MALLOC((m), struct ifnet_en *, sizeof(*m), M_PCB, M_NOWAIT);
	if (m == NULL)
		return (NULL);
	bzero(m, sizeof(*m));
	m->ifen_next = ipxip_list;
	ipxip_list = m;
	ifp = &m->ifen_ifnet;

	ifp->if_name = "ipxip";
	ifp->if_mtu = LOMTU;
	ifp->if_ioctl = ipxipioctl;
	ifp->if_output = ipxipoutput;
	ifp->if_start = ipxipstart;
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_unit = ipxipif.if_unit++;
	if_attach(ifp);

	return (m);
}


/*
 * Process an ioctl request.
 */
static int
ipxipioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int error = 0;
	struct ifreq *ifr;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* fall into: */

	case SIOCSIFDSTADDR:
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCSIFFLAGS:
		ifr = (struct ifreq *)data;
		if ((ifr->ifr_flags & IFF_UP) == 0)
			error = ipxip_free(ifp);


	default:
		error = EINVAL;
	}
	return (error);
}

static struct mbuf *ipxip_badlen;
static struct mbuf *ipxip_lastin;
static int ipxip_hold_input;

void
ipxip_input(m, hlen)
	register struct mbuf *m;
	int hlen;
{
	register struct ip *ip;
	register struct ipx *ipx;
	register struct ifqueue *ifq = &ipxintrq;
	int len, s;

	if (ipxip_hold_input) {
		if (ipxip_lastin != NULL) {
			m_freem(ipxip_lastin);
		}
		ipxip_lastin = m_copym(m, 0, (int)M_COPYALL, M_DONTWAIT);
	}
	/*
	 * Get IP and IPX header together in first mbuf.
	 */
	ipxipif.if_ipackets++;
	s = sizeof(struct ip) + sizeof(struct ipx);
	if (((m->m_flags & M_EXT) || m->m_len < s) &&
	    (m = m_pullup(m, s)) == NULL) {
		ipxipif.if_ierrors++;
		return;
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_hl > (sizeof(struct ip) >> 2)) {
		ip_stripoptions(m, (struct mbuf *)NULL);
		if (m->m_len < s) {
			if ((m = m_pullup(m, s)) == NULL) {
				ipxipif.if_ierrors++;
				return;
			}
			ip = mtod(m, struct ip *);
		}
	}

	/*
	 * Make mbuf data length reflect IPX length.
	 * If not enough data to reflect IPX length, drop.
	 */
	m->m_data += sizeof(struct ip);
	m->m_len -= sizeof(struct ip);
	m->m_pkthdr.len -= sizeof(struct ip);
	ipx = mtod(m, struct ipx *);
	len = ntohs(ipx->ipx_len);
	if (len & 1)
		len++;		/* Preserve Garbage Byte */
	if (ip->ip_len != len) {
		if (len > ip->ip_len) {
			ipxipif.if_ierrors++;
			if (ipxip_badlen)
				m_freem(ipxip_badlen);
			ipxip_badlen = m;
			return;
		}
		/* Any extra will be trimmed off by the IPX routines */
	}

	/*
	 * Deliver to IPX
	 */
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IPX);
	splx(s);
	return;
}

static int
ipxipoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	register struct ifnet_en *ifn = (struct ifnet_en *)ifp;
	register struct ip *ip;
	register struct route *ro = &(ifn->ifen_route);
	register int len = 0;
	register struct ipx *ipx = mtod(m, struct ipx *);
	int error;

	ifn->ifen_ifnet.if_opackets++;
	ipxipif.if_opackets++;

	/*
	 * Calculate data length and make space
	 * for IP header.
	 */
	len =  ntohs(ipx->ipx_len);
	if (len & 1)
		len++;		/* Preserve Garbage Byte */
	/* following clause not necessary on vax */
	if (3 & (int)m->m_data) {
		/* force longword alignment of ip hdr */
		struct mbuf *m0 = m_gethdr(MT_HEADER, M_DONTWAIT);
		if (m0 == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		MH_ALIGN(m0, sizeof(struct ip));
		m0->m_flags = m->m_flags & M_COPYFLAGS;
		m0->m_next = m;
		m0->m_len = sizeof(struct ip);
		m0->m_pkthdr.len = m0->m_len + m->m_len;
		m->m_flags &= ~M_PKTHDR;
		m = m0;
	} else {
		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (m == NULL)
			return (ENOBUFS);
	}
	/*
	 * Fill in IP header.
	 */
	ip = mtod(m, struct ip *);
	*(long *)ip = 0;
	ip->ip_p = IPPROTO_IDP;
	ip->ip_src = ifn->ifen_src;
	ip->ip_dst = ifn->ifen_dst;
	ip->ip_len = (u_short)len + sizeof(struct ip);
	ip->ip_ttl = MAXTTL;

	/*
	 * Output final datagram.
	 */
	error =  (ip_output(m, (struct mbuf *)NULL, ro, SO_BROADCAST, NULL));
	if (error) {
		ifn->ifen_ifnet.if_oerrors++;
		ifn->ifen_ifnet.if_ierrors = error;
	}
	return (error);
	m_freem(m);
	return (ENETUNREACH);
}

static void
ipxipstart(ifp)
struct ifnet *ifp;
{
	panic("ipxip_start called\n");
}

static struct ifreq ifr_ipxip = {"ipxip0"};

int
ipxip_route(so, m, p)
	struct socket *so;
	register struct mbuf *m;
	struct proc *p;
{
	register struct ipxip_req *rq = mtod(m, struct ipxip_req *);
	struct sockaddr_ipx *ipx_dst = (struct sockaddr_ipx *)&rq->rq_ipx;
	struct sockaddr_in *ip_dst = (struct sockaddr_in *)&rq->rq_ip;
	struct route ro;
	struct ifnet_en *ifn;
	struct sockaddr_in *src;

	/*
	 * First, make sure we already have an IPX address:
	 */
	if (ipx_ifaddr == NULL)
		return (EADDRNOTAVAIL);
	/*
	 * Now, determine if we can get to the destination
	 */
	bzero((caddr_t)&ro, sizeof(ro));
	ro.ro_dst = *(struct sockaddr *)ip_dst;
	rtalloc(&ro);
	if (ro.ro_rt == NULL || ro.ro_rt->rt_ifp == NULL) {
		return (ENETUNREACH);
	}

	/*
	 * And see how he's going to get back to us:
	 * i.e., what return ip address do we use?
	 */
	{
		register struct in_ifaddr *ia;
		struct ifnet *ifp = ro.ro_rt->rt_ifp;

		for (ia = in_ifaddrhead.tqh_first; ia != NULL; 
		     ia = ia->ia_link.tqe_next)
			if (ia->ia_ifp == ifp)
				break;
		if (ia == NULL)
			ia = in_ifaddrhead.tqh_first;
		if (ia == NULL) {
			RTFREE(ro.ro_rt);
			return (EADDRNOTAVAIL);
		}
		src = (struct sockaddr_in *)&ia->ia_addr;
	}

	/*
	 * Is there a free (pseudo-)interface or space?
	 */
	for (ifn = ipxip_list; ifn != NULL; ifn = ifn->ifen_next) {
		if ((ifn->ifen_ifnet.if_flags & IFF_UP) == 0)
			break;
	}
	if (ifn == NULL)
		ifn = ipxipattach();
	if (ifn == NULL) {
		RTFREE(ro.ro_rt);
		return (ENOBUFS);
	}
	ifn->ifen_route = ro;
	ifn->ifen_dst =  ip_dst->sin_addr;
	ifn->ifen_src = src->sin_addr;

	/*
	 * now configure this as a point to point link
	 */
	ifr_ipxip.ifr_name[4] = '0' + ipxipif.if_unit - 1;
	ifr_ipxip.ifr_dstaddr = *(struct sockaddr *)ipx_dst;
	ipx_control(so, (int)SIOCSIFDSTADDR, (caddr_t)&ifr_ipxip,
			(struct ifnet *)ifn, p);

	/* use any of our addresses */
	satoipx_addr(ifr_ipxip.ifr_addr).x_host = 
			ipx_ifaddr->ia_addr.sipx_addr.x_host;

	return (ipx_control(so, (int)SIOCSIFADDR, (caddr_t)&ifr_ipxip,
			(struct ifnet *)ifn, p));
}

static int
ipxip_free(ifp)
struct ifnet *ifp;
{
	register struct ifnet_en *ifn = (struct ifnet_en *)ifp;
	struct route *ro = & ifn->ifen_route;

	if (ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = NULL;
	}
	ifp->if_flags &= ~IFF_UP;
	return (0);
}

void
ipxip_ctlinput(cmd, sa, dummy)
	int cmd;
	struct sockaddr *sa;
	void *dummy;
{
	struct sockaddr_in *sin;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (sa->sa_family != AF_INET && sa->sa_family != AF_IMPLINK)
		return;
	sin = (struct sockaddr_in *)sa;
	if (sin->sin_addr.s_addr == INADDR_ANY)
		return;

	switch (cmd) {

	case PRC_ROUTEDEAD:
	case PRC_REDIRECT_NET:
	case PRC_REDIRECT_HOST:
	case PRC_REDIRECT_TOSNET:
	case PRC_REDIRECT_TOSHOST:
		ipxip_rtchange(&sin->sin_addr);
		break;
	}
}

static void
ipxip_rtchange(dst)
	register struct in_addr *dst;
{
	register struct ifnet_en *ifn;

	for (ifn = ipxip_list; ifn != NULL; ifn = ifn->ifen_next) {
		if (ifn->ifen_dst.s_addr == dst->s_addr &&
			ifn->ifen_route.ro_rt != NULL) {
				RTFREE(ifn->ifen_route.ro_rt);
				ifn->ifen_route.ro_rt = NULL;
		}
	}
}
#endif /* IPXIP */
