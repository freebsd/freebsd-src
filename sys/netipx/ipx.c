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
 *	@(#)ipx.c
 *
 * $Id: ipx.c,v 1.2 1995/10/31 23:36:19 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>

#ifdef IPX

struct ipx_ifaddr *ipx_ifaddr;
int ipx_interfaces;

/*
 * Generic internet control operations (ioctl's).
 */
/* ARGSUSED */
int
ipx_control(so, cmd, data, ifp)
	struct socket *so;
	int cmd;
	caddr_t data;
	register struct ifnet *ifp;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct ipx_aliasreq *ifra = (struct ipx_aliasreq *)data;
	register struct ipx_ifaddr *ia;
	struct ifaddr *ifa;
	struct ipx_ifaddr *oia;
	int dstIsNew, hostIsNew;
	int error = 0;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp == 0)
		return (EADDRNOTAVAIL);
	for (ia = ipx_ifaddr; ia; ia = ia->ia_next)
		if (ia->ia_ifp == ifp)
			break;

	switch (cmd) {

	case SIOCGIFADDR:
		if (ia == (struct ipx_ifaddr *)0)
			return (EADDRNOTAVAIL);
		*(struct sockaddr_ipx *)&ifr->ifr_addr = ia->ia_addr;
		return (0);

	case SIOCGIFBRDADDR:
		if (ia == (struct ipx_ifaddr *)0)
			return (EADDRNOTAVAIL);
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*(struct sockaddr_ipx *)&ifr->ifr_dstaddr = ia->ia_broadaddr;
		return (0);

	case SIOCGIFDSTADDR:
		if (ia == (struct ipx_ifaddr *)0)
			return (EADDRNOTAVAIL);
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*(struct sockaddr_ipx *)&ifr->ifr_dstaddr = ia->ia_dstaddr;
		return (0);
	}

	if ((so->so_state & SS_PRIV) == 0)
		return (EPERM);

	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sipx_family == AF_IPX)
		    for (oia = ia; ia; ia = ia->ia_next) {
			if (ia->ia_ifp == ifp  &&
			    ipx_neteq(ia->ia_addr.sipx_addr,
				  ifra->ifra_addr.sipx_addr))
			    break;
		    }
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */

	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
		if (ia == (struct ipx_ifaddr *)0) {
			oia = (struct ipx_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK);
			if (oia == (struct ipx_ifaddr *)NULL)
				return (ENOBUFS);
			bzero((caddr_t)oia, sizeof(*oia));
			if ((ia = ipx_ifaddr)) {
				for ( ; ia->ia_next; ia = ia->ia_next)
					;
				ia->ia_next = oia;
			} else
				ipx_ifaddr = oia;
			ia = oia;
			if ((ifa = ifp->if_addrlist)) {
				for ( ; ifa->ifa_next; ifa = ifa->ifa_next)
					;
				ifa->ifa_next = (struct ifaddr *) ia;
			} else
				ifp->if_addrlist = (struct ifaddr *) ia;
			ia->ia_ifp = ifp;
			ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;

			ia->ia_ifa.ifa_netmask =
				(struct sockaddr *)&ipx_netmask;

			ia->ia_ifa.ifa_dstaddr =
				(struct sockaddr *)&ia->ia_dstaddr;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sipx_family = AF_IPX;
				ia->ia_broadaddr.sipx_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sipx_addr.x_host = ipx_broadhost;
			}
			ipx_interfaces++;
		}
	}

	switch (cmd) {

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		if (ia->ia_flags & IFA_ROUTE) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_flags &= ~IFA_ROUTE;
		}
		if (ifp->if_ioctl) {
			error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR, (void *)ia);
			if (error)
				return (error);
		}
		*(struct sockaddr *)&ia->ia_dstaddr = ifr->ifr_dstaddr;
		return (0);

	case SIOCSIFADDR:
		return (ipx_ifinit(ifp, ia,
				(struct sockaddr_ipx *)&ifr->ifr_addr, 1));

	case SIOCDIFADDR:
		ipx_ifscrub(ifp, ia);
		if ((ifa = ifp->if_addrlist) == (struct ifaddr *)ia)
			ifp->if_addrlist = ifa->ifa_next;
		else {
			while (ifa->ifa_next &&
			       (ifa->ifa_next != (struct ifaddr *)ia))
				    ifa = ifa->ifa_next;
			if (ifa->ifa_next)
			    ifa->ifa_next = ((struct ifaddr *)ia)->ifa_next;
			else
				printf("Couldn't unlink ipxifaddr from ifp\n");
		}
		oia = ia;
		if (oia == (ia = ipx_ifaddr)) {
			ipx_ifaddr = ia->ia_next;
		} else {
			while (ia->ia_next && (ia->ia_next != oia)) {
				ia = ia->ia_next;
			}
			if (ia->ia_next)
			    ia->ia_next = oia->ia_next;
			else
				printf("Didn't unlink ipxifadr from list\n");
		}
		IFAFREE((&oia->ia_ifa));
		if (0 == --ipx_interfaces) {
			/*
			 * We reset to virginity and start all over again
			 */
			ipx_thishost = ipx_zerohost;
		}
		return (0);
	
	case SIOCAIFADDR:
		dstIsNew = 0; hostIsNew = 1;
		if (ia->ia_addr.sipx_family == AF_IPX) {
			if (ifra->ifra_addr.sipx_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ipx_neteq(ifra->ifra_addr.sipx_addr,
					 ia->ia_addr.sipx_addr))
				hostIsNew = 0;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sipx_family == AF_IPX)) {
			if (hostIsNew == 0)
				ipx_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			dstIsNew  = 1;
		}
		if (ifra->ifra_addr.sipx_family == AF_IPX &&
					    (hostIsNew || dstIsNew))
			error = ipx_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		return (error);

	default:
		if (ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
}

/*
* Delete any previous route for an old address.
*/
void
ipx_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct ipx_ifaddr *ia; 
{
	if (ia->ia_flags & IFA_ROUTE) {
		if (ifp->if_flags & IFF_POINTOPOINT) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
		} else
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
		ia->ia_flags &= ~IFA_ROUTE;
	}
}
/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
ipx_ifinit(ifp, ia, sipx, scrub)
	register struct ifnet *ifp;
	register struct ipx_ifaddr *ia;
	register struct sockaddr_ipx *sipx;
	int scrub;
{
	struct sockaddr_ipx oldaddr;
	register union ipx_host *h = &ia->ia_addr.sipx_addr.x_host;
	int s = splimp(), error;

	/*
	 * Set up new addresses.
	 */
	oldaddr = ia->ia_addr;
	ia->ia_addr = *sipx;
	/*
	 * The convention we shall adopt for naming is that
	 * a supplied address of zero means that "we don't care".
	 * if there is a single interface, use the address of that
	 * interface as our 6 byte host address.
	 * if there are multiple interfaces, use any address already
	 * used.
	 *
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ipx_hosteqnh(ipx_thishost, ipx_zerohost)) {
		if (ifp->if_ioctl &&
		     (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (void *)ia))) {
			ia->ia_addr = oldaddr;
			splx(s);
			return (error);
		}
		ipx_thishost = *h;
	} else if (ipx_hosteqnh(sipx->sipx_addr.x_host, ipx_zerohost)
	    || ipx_hosteqnh(sipx->sipx_addr.x_host, ipx_thishost)) {
		*h = ipx_thishost;
		if (ifp->if_ioctl &&
		     (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (void *)ia))) {
			ia->ia_addr = oldaddr;
			splx(s);
			return (error);
		}
		if (!ipx_hosteqnh(ipx_thishost,*h)) {
			ia->ia_addr = oldaddr;
			splx(s);
			return (EINVAL);
		}
	} else {
		ia->ia_addr = oldaddr;
		splx(s);
		return (EINVAL);
	}
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	/*
	 * Add route for the network.
	 */
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		ipx_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (ifp->if_flags & IFF_POINTOPOINT)
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
	else {
		ia->ia_broadaddr.sipx_addr.x_net = ia->ia_addr.sipx_addr.x_net;
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_UP);
	}
	ia->ia_flags |= IFA_ROUTE;
	return (0);
}

/*
 * Return address info for specified internet network.
 */
struct ipx_ifaddr *
ipx_iaonnetof(dst)
	register struct ipx_addr *dst;
{
	register struct ipx_ifaddr *ia;
	register struct ipx_addr *compare;
	register struct ifnet *ifp;
	struct ipx_ifaddr *ia_maybe = 0;
	union ipx_net net = dst->x_net;

	for (ia = ipx_ifaddr; ia; ia = ia->ia_next) {
		if ((ifp = ia->ia_ifp)) {
			if (ifp->if_flags & IFF_POINTOPOINT) {
				compare = &satoipx_addr(ia->ia_dstaddr);
				if (ipx_hosteq(*dst, *compare))
					return (ia);
				if (ipx_neteqnn(net, ia->ia_addr.sipx_addr.x_net))
					ia_maybe = ia;
			} else {
				if (ipx_neteqnn(net, ia->ia_addr.sipx_addr.x_net))
					return (ia);
			}
		}
	}
	return (ia_maybe);
}
#endif
