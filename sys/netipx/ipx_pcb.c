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
 *	@(#)ipx_pcb.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netipx/ipx_pcb.c,v 1.49.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>

static struct	ipx_addr zeroipx_addr;
static u_short	ipxpcb_lport_cache;

int
ipx_pcballoc(struct socket *so, struct ipxpcbhead *head, struct thread *td)
{
	struct ipxpcb *ipxp;

	KASSERT(so->so_pcb == NULL, ("ipx_pcballoc: so_pcb != NULL"));
	IPX_LIST_LOCK_ASSERT();

	MALLOC(ipxp, struct ipxpcb *, sizeof *ipxp, M_PCB, M_NOWAIT | M_ZERO);
	if (ipxp == NULL)
		return (ENOBUFS);
	IPX_LOCK_INIT(ipxp);
	ipxp->ipxp_socket = so;
	if (ipxcksum)
		ipxp->ipxp_flags |= IPXP_CHECKSUM;
	LIST_INSERT_HEAD(head, ipxp, ipxp_list);
	so->so_pcb = (caddr_t)ipxp;
	return (0);
}

int
ipx_pcbbind(struct ipxpcb *ipxp, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_ipx *sipx;
	u_short lport = 0;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	if (ipxp->ipxp_lport || !ipx_nullhost(ipxp->ipxp_laddr))
		return (EINVAL);
	if (nam == NULL)
		goto noname;
	sipx = (struct sockaddr_ipx *)nam;
	if (!ipx_nullhost(sipx->sipx_addr)) {
		int tport = sipx->sipx_port;

		sipx->sipx_port = 0;		/* yech... */
		if (ifa_ifwithaddr((struct sockaddr *)sipx) == NULL)
			return (EADDRNOTAVAIL);
		sipx->sipx_port = tport;
	}
	lport = sipx->sipx_port;
	if (lport) {
		u_short aport = ntohs(lport);

		if (aport < IPXPORT_RESERVED && td != NULL &&
		    priv_check(td, PRIV_NETIPX_RESERVEDPORT))
			return (EACCES);
		if (ipx_pcblookup(&zeroipx_addr, lport, 0))
			return (EADDRINUSE);
	}
	ipxp->ipxp_laddr = sipx->sipx_addr;
noname:
	if (lport == 0)
		do {
			ipxpcb_lport_cache++;
			if ((ipxpcb_lport_cache < IPXPORT_RESERVED) ||
			    (ipxpcb_lport_cache >= IPXPORT_WELLKNOWN))
				ipxpcb_lport_cache = IPXPORT_RESERVED;
			lport = htons(ipxpcb_lport_cache);
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
ipx_pcbconnect(struct ipxpcb *ipxp, struct sockaddr *nam, struct thread *td)
{
	struct ipx_ifaddr *ia;
	struct sockaddr_ipx *sipx = (struct sockaddr_ipx *)nam;
	struct ipx_addr *dst;
	struct route *ro;
	struct ifnet *ifp;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	ia = NULL;

	if (sipx->sipx_family != AF_IPX)
		return (EAFNOSUPPORT);
	if (sipx->sipx_port == 0 || ipx_nullhost(sipx->sipx_addr))
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
		if (ro->ro_rt != NULL && !(ro->ro_rt->rt_flags & RTF_HOST)) {
			/* can patch route to avoid rtalloc */
			*dst = sipx->sipx_addr;
		} else {
	flush:
			if (ro->ro_rt != NULL)
				RTFREE(ro->ro_rt);
			ro->ro_rt = NULL;
		}
	}/* else cached route is ok; do nothing */
	ipxp->ipxp_lastdst = sipx->sipx_addr;
	if ((ipxp->ipxp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
	    (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL)) {
		    /* No route yet, so try to acquire one */
		    ro->ro_dst.sa_family = AF_IPX;
		    ro->ro_dst.sa_len = sizeof(ro->ro_dst);
		    *dst = sipx->sipx_addr;
		    dst->x_port = 0;
		    rtalloc_ign(ro, 0);
	}
	if (ipx_neteqnn(ipxp->ipxp_laddr.x_net, ipx_zeronet)) {
		/*
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */

		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 */
		if (ro->ro_rt != NULL && (ifp = ro->ro_rt->rt_ifp) != NULL)
			for (ia = ipx_ifaddr; ia != NULL; ia = ia->ia_next)
				if (ia->ia_ifp == ifp)
					break;
		if (ia == NULL) {
			u_short fport = sipx->sipx_addr.x_port;
			sipx->sipx_addr.x_port = 0;
			ia = (struct ipx_ifaddr *)
				ifa_ifwithdstaddr((struct sockaddr *)sipx);
			sipx->sipx_addr.x_port = fport;
			if (ia == NULL)
				ia = ipx_iaonnetof(&sipx->sipx_addr);
			if (ia == NULL)
				ia = ipx_ifaddr;
			if (ia == NULL)
				return (EADDRNOTAVAIL);
		}
		ipxp->ipxp_laddr.x_net = satoipx_addr(ia->ia_addr).x_net;
	}
	if (ipx_nullhost(ipxp->ipxp_laddr)) {
		/*
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */

		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 */
		if (ro->ro_rt != NULL && (ifp = ro->ro_rt->rt_ifp) != NULL)
			for (ia = ipx_ifaddr; ia != NULL; ia = ia->ia_next)
				if (ia->ia_ifp == ifp)
					break;
		if (ia == NULL) {
			u_short fport = sipx->sipx_addr.x_port;
			sipx->sipx_addr.x_port = 0;
			ia = (struct ipx_ifaddr *)
				ifa_ifwithdstaddr((struct sockaddr *)sipx);
			sipx->sipx_addr.x_port = fport;
			if (ia == NULL)
				ia = ipx_iaonnetof(&sipx->sipx_addr);
			if (ia == NULL)
				ia = ipx_ifaddr;
			if (ia == NULL)
				return (EADDRNOTAVAIL);
		}
		ipxp->ipxp_laddr.x_host = satoipx_addr(ia->ia_addr).x_host;
	}
	if (ipx_pcblookup(&sipx->sipx_addr, ipxp->ipxp_lport, 0))
		return (EADDRINUSE);
	if (ipxp->ipxp_lport == 0)
		ipx_pcbbind(ipxp, (struct sockaddr *)NULL, td);

	/* XXX just leave it zero if we can't find a route */

	ipxp->ipxp_faddr = sipx->sipx_addr;
	/* Includes ipxp->ipxp_fport = sipx->sipx_port; */
	return (0);
}

void
ipx_pcbdisconnect(struct ipxpcb *ipxp)
{

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	ipxp->ipxp_faddr = zeroipx_addr;
}

void
ipx_pcbdetach(struct ipxpcb *ipxp)
{
	struct socket *so = ipxp->ipxp_socket;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	so->so_pcb = NULL;
	ipxp->ipxp_socket = NULL;
}

void
ipx_pcbfree(struct ipxpcb *ipxp)
{

	KASSERT(ipxp->ipxp_socket == NULL,
	    ("ipx_pcbfree: ipxp_socket != NULL"));
	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	if (ipxp->ipxp_route.ro_rt != NULL)
		RTFREE(ipxp->ipxp_route.ro_rt);
	LIST_REMOVE(ipxp, ipxp_list);
	IPX_LOCK_DESTROY(ipxp);
	FREE(ipxp, M_PCB);
}

void
ipx_getsockaddr(struct ipxpcb *ipxp, struct sockaddr **nam)
{
	struct sockaddr_ipx *sipx, ssipx;

	sipx = &ssipx;
	bzero((caddr_t)sipx, sizeof(*sipx));
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_family = AF_IPX;
	IPX_LOCK(ipxp);
	sipx->sipx_addr = ipxp->ipxp_laddr;
	IPX_UNLOCK(ipxp);
	*nam = sodupsockaddr((struct sockaddr *)sipx, M_WAITOK);
}

void
ipx_getpeeraddr(struct ipxpcb *ipxp, struct sockaddr **nam)
{
	struct sockaddr_ipx *sipx, ssipx;

	sipx = &ssipx;
	bzero(sipx, sizeof(*sipx));
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_family = AF_IPX;
	IPX_LOCK(ipxp);
	sipx->sipx_addr = ipxp->ipxp_faddr;
	IPX_UNLOCK(ipxp);
	*nam = sodupsockaddr((struct sockaddr *)sipx, M_WAITOK);
}

struct ipxpcb *
ipx_pcblookup(struct ipx_addr *faddr, u_short lport, int wildp)
{
	struct ipxpcb *ipxp, *match = NULL;
	int matchwild = 3, wildcard;
	u_short fport;

	IPX_LIST_LOCK_ASSERT();

	fport = faddr->x_port;
	LIST_FOREACH(ipxp, &ipxpcb_list, ipxp_list) {
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
		if (wildcard && wildp == 0)
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
