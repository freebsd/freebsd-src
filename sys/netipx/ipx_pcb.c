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
 *	@(#)ipx_pcb.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>

struct	ipx_addr zeroipx_addr;

int
ipx_pcballoc(so, head)
	struct socket *so;
	struct ipxpcb *head;
{
	struct mbuf *m;
	register struct ipxpcb *ipxp;

	m = m_getclr(M_DONTWAIT, MT_PCB);
	if (m == NULL)
		return (ENOBUFS);
	ipxp = mtod(m, struct ipxpcb *);
	ipxp->ipxp_socket = so;
	insque(ipxp, head);
	so->so_pcb = (caddr_t)ipxp;
	return (0);
}
	
int
ipx_pcbbind(ipxp, nam)
	register struct ipxpcb *ipxp;
	struct mbuf *nam;
{
	register struct sockaddr_ipx *sipx;
	u_short lport = 0;

	if (ipxp->ipxp_lport || !ipx_nullhost(ipxp->ipxp_laddr))
		return (EINVAL);
	if (nam == 0)
		goto noname;
	sipx = mtod(nam, struct sockaddr_ipx *);
	if (nam->m_len != sizeof (*sipx))
		return (EINVAL);
	if (!ipx_nullhost(sipx->sipx_addr)) {
		int tport = sipx->sipx_port;

		sipx->sipx_port = 0;		/* yech... */
		if (ifa_ifwithaddr((struct sockaddr *)sipx) == 0)
			return (EADDRNOTAVAIL);
		sipx->sipx_port = tport;
	}
	lport = sipx->sipx_port;
	if (lport) {
		u_short aport = ntohs(lport);

		if (aport < IPXPORT_MAX &&
		    (ipxp->ipxp_socket->so_state & SS_PRIV) == 0)
			return (EACCES);
		if (ipx_pcblookup(&zeroipx_addr, lport, 0))
			return (EADDRINUSE);
	}
	ipxp->ipxp_laddr = sipx->sipx_addr;
noname:
	if (lport == 0)
		do {
			if (ipxpcb.ipxp_lport++ < IPXPORT_MAX)
				ipxpcb.ipxp_lport = IPXPORT_MAX;
			lport = htons(ipxpcb.ipxp_lport);
		} while (ipx_pcblookup(&zeroipx_addr, lport, 0));
	ipxp->ipxp_lport = lport;
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sipx.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
ipx_pcbconnect(ipxp, nam)
	struct ipxpcb *ipxp;
	struct mbuf *nam;
{
	struct ipx_ifaddr *ia;
	register struct sockaddr_ipx *sipx = mtod(nam, struct sockaddr_ipx *);
	register struct ipx_addr *dst;
	register struct route *ro;
	struct ifnet *ifp;

	if (nam->m_len != sizeof (*sipx))
		return (EINVAL);
	if (sipx->sipx_family != AF_IPX)
		return (EAFNOSUPPORT);
	if (sipx->sipx_port==0 || ipx_nullhost(sipx->sipx_addr))
		return (EADDRNOTAVAIL);
	/*
	 * If we haven't bound which network number to use as ours,
	 * we will use the number of the outgoing interface.
	 * This depends on having done a routing lookup, which
	 * we will probably have to do anyway, so we might
	 * as well do it now.  On the other hand if we are
	 * sending to multiple destinations we may have already
	 * done the lookup, so see if we can use the route
	 * from before.  In any case, we only
	 * chose a port number once, even if sending to multiple
	 * destinations.
	 */
	ro = &ipxp->ipxp_route;
	dst = &satoipx_addr(ro->ro_dst);
	if (ipxp->ipxp_socket->so_options & SO_DONTROUTE)
		goto flush;
	if (!ipx_neteq(ipxp->ipxp_lastdst, sipx->sipx_addr))
		goto flush;
	if (!ipx_hosteq(ipxp->ipxp_lastdst, sipx->sipx_addr)) {
		if (ro->ro_rt && ! (ro->ro_rt->rt_flags & RTF_HOST)) {
			/* can patch route to avoid rtalloc */
			*dst = sipx->sipx_addr;
		} else {
	flush:
			if (ro->ro_rt)
				RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
			ipxp->ipxp_laddr.x_net = ipx_zeronet;
		}
	}/* else cached route is ok; do nothing */
	ipxp->ipxp_lastdst = sipx->sipx_addr;
	if ((ipxp->ipxp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
	    (ro->ro_rt == (struct rtentry *)0 ||
	     ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
		    /* No route yet, so try to acquire one */
		    ro->ro_dst.sa_family = AF_IPX;
		    ro->ro_dst.sa_len = sizeof(ro->ro_dst);
		    *dst = sipx->sipx_addr;
		    dst->x_port = 0;
		    rtalloc(ro);
	}
	if (ipx_neteqnn(ipxp->ipxp_laddr.x_net, ipx_zeronet)) {
		/* 
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */

		ia = (struct ipx_ifaddr *)0;
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 */
		if (ro->ro_rt && (ifp = ro->ro_rt->rt_ifp))
			for (ia = ipx_ifaddr; ia; ia = ia->ia_next)
				if (ia->ia_ifp == ifp)
					break;
		if (ia == 0) {
			u_short fport = sipx->sipx_addr.x_port;
			sipx->sipx_addr.x_port = 0;
			ia = (struct ipx_ifaddr *)
				ifa_ifwithdstaddr((struct sockaddr *)sipx);
			sipx->sipx_addr.x_port = fport;
			if (ia == 0)
				ia = ipx_iaonnetof(&sipx->sipx_addr);
			if (ia == 0)
				ia = ipx_ifaddr;
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		ipxp->ipxp_laddr.x_net = satoipx_addr(ia->ia_addr).x_net;
	}
	if (ipx_pcblookup(&sipx->sipx_addr, ipxp->ipxp_lport, 0))
		return (EADDRINUSE);
	if (ipx_nullhost(ipxp->ipxp_laddr)) {
		if (ipxp->ipxp_lport == 0)
			(void) ipx_pcbbind(ipxp, (struct mbuf *)0);
		ipxp->ipxp_laddr.x_host = ipx_thishost;
	}
	ipxp->ipxp_faddr = sipx->sipx_addr;
	/* Includes ipxp->ipxp_fport = sipx->sipx_port; */
	return (0);
}

void
ipx_pcbdisconnect(ipxp)
	struct ipxpcb *ipxp;
{

	ipxp->ipxp_faddr = zeroipx_addr;
	if (ipxp->ipxp_socket->so_state & SS_NOFDREF)
		ipx_pcbdetach(ipxp);
}

void
ipx_pcbdetach(ipxp)
	struct ipxpcb *ipxp;
{
	struct socket *so = ipxp->ipxp_socket;

	so->so_pcb = 0;
	sofree(so);
	if (ipxp->ipxp_route.ro_rt)
		rtfree(ipxp->ipxp_route.ro_rt);
	remque(ipxp);
	(void) m_free(dtom(ipxp));
}

void
ipx_setsockaddr(ipxp, nam)
	register struct ipxpcb *ipxp;
	struct mbuf *nam;
{
	register struct sockaddr_ipx *sipx = mtod(nam, struct sockaddr_ipx *);
	
	nam->m_len = sizeof (*sipx);
	sipx = mtod(nam, struct sockaddr_ipx *);
	bzero((caddr_t)sipx, sizeof (*sipx));
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_family = AF_IPX;
	sipx->sipx_addr = ipxp->ipxp_laddr;
}

void
ipx_setpeeraddr(ipxp, nam)
	register struct ipxpcb *ipxp;
	struct mbuf *nam;
{
	register struct sockaddr_ipx *sipx = mtod(nam, struct sockaddr_ipx *);
	
	nam->m_len = sizeof (*sipx);
	sipx = mtod(nam, struct sockaddr_ipx *);
	bzero((caddr_t)sipx, sizeof (*sipx));
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_family = AF_IPX;
	sipx->sipx_addr = ipxp->ipxp_faddr;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  Call the
 * protocol specific routine to handle each connection.
 * Also pass an extra paramter via the ipxpcb. (which may in fact
 * be a parameter list!)
 */
void
ipx_pcbnotify(dst, errno, notify, param)
	register struct ipx_addr *dst;
	long param;
	int errno, (*notify)();
{
	register struct ipxpcb *ipxp, *oinp;
	int s = splimp();

	for (ipxp = (&ipxpcb)->ipxp_next; ipxp != (&ipxpcb);) {
		if (!ipx_hosteq(*dst,ipxp->ipxp_faddr)) {
	next:
			ipxp = ipxp->ipxp_next;
			continue;
		}
		if (ipxp->ipxp_socket == 0)
			goto next;
		if (errno) 
			ipxp->ipxp_socket->so_error = errno;
		oinp = ipxp;
		ipxp = ipxp->ipxp_next;
		oinp->ipxp_notify_param = param;
		(*notify)(oinp);
	}
	splx(s);
}

#ifdef notdef
/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
ipx_rtchange(ipxp)
	struct ipxpcb *ipxp;
{
	if (ipxp->ipxp_route.ro_rt) {
		rtfree(ipxp->ipxp_route.ro_rt);
		ipxp->ipxp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
	/* SHOULD NOTIFY HIGHER-LEVEL PROTOCOLS */
}
#endif

struct ipxpcb *
ipx_pcblookup(faddr, lport, wildp)
	struct ipx_addr *faddr;
	u_short lport;
	int wildp;
{
	register struct ipxpcb *ipxp, *match = 0;
	int matchwild = 3, wildcard;
	u_short fport;

	fport = faddr->x_port;
	for (ipxp = (&ipxpcb)->ipxp_next; ipxp != (&ipxpcb); ipxp = ipxp->ipxp_next) {
		if (ipxp->ipxp_lport != lport)
			continue;
		wildcard = 0;
		if (ipx_nullhost(ipxp->ipxp_faddr)) {
			if (!ipx_nullhost(*faddr))
				wildcard++;
		} else {
			if (ipx_nullhost(*faddr))
				wildcard++;
			else {
				if (!ipx_hosteq(ipxp->ipxp_faddr, *faddr))
					continue;
				if (ipxp->ipxp_fport != fport) {
					if (ipxp->ipxp_fport != 0)
						continue;
					else
						wildcard++;
				}
			}
		}
		if (wildcard && wildp==0)
			continue;
		if (wildcard < matchwild) {
			match = ipxp;
			matchwild = wildcard;
			if (wildcard == 0)
				break;
		}
	}
	return (match);
}
