/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
/* #include "gif.h" */
#if NGIF > 0
#include <net/if_gif.h>
#endif
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#include <netinet6/nd6.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/mld6_var.h>
#include <netinet6/in6_ifattach.h>

#include <net/net_osdep.h>

MALLOC_DEFINE(M_IPMADDR, "in6_multi", "internet multicast address");

/*
 * Definitions of some costant IP6 addresses.
 */
const struct	in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct	in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
const struct	in6_addr in6addr_nodelocal_allnodes =
	IN6ADDR_NODELOCAL_ALLNODES_INIT;
const struct	in6_addr in6addr_linklocal_allnodes =
	IN6ADDR_LINKLOCAL_ALLNODES_INIT;
const struct	in6_addr in6addr_linklocal_allrouters =
	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;

const struct	in6_addr in6mask0 = IN6MASK0;
const struct	in6_addr in6mask32 = IN6MASK32;
const struct	in6_addr in6mask64 = IN6MASK64;
const struct	in6_addr in6mask96 = IN6MASK96;
const struct	in6_addr in6mask128 = IN6MASK128;

static int	in6_lifaddr_ioctl __P((struct socket *, u_long, caddr_t,
	struct ifnet *, struct proc *));
struct	in6_multihead in6_multihead;	/* XXX BSS initialization */

/*
 * Determine whether an IP6 address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in6_canforward(src, dst)
	struct	in6_addr *src, *dst;
{
	if (IN6_IS_ADDR_LINKLOCAL(src) ||
	   IN6_IS_ADDR_LINKLOCAL(dst) ||
	   IN6_IS_ADDR_MULTICAST(dst))
		return(0);
	return(1);
}

/*
 * Check if the loopback entry will be automatically generated.
 *   if 0 returned, will not be automatically generated.
 *   if 1 returned, will be automatically generated.
 */
static int
in6_is_ifloop_auto(struct ifaddr *ifa)
{
#define SIN6(s) ((struct sockaddr_in6 *)s)
	/*
	 * If RTF_CLONING is unset, or (IFF_LOOPBACK | IFF_POINTOPOINT),
	 * or netmask is all0 or all1, then cloning will not happen,
	 * then we can't rely on its loopback entry generation.
	 */
	if ((ifa->ifa_flags & RTF_CLONING) == 0 ||
	    (ifa->ifa_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) ||
	    (SIN6(ifa->ifa_netmask)->sin6_len == sizeof(struct sockaddr_in6)
	     &&
	     IN6_ARE_ADDR_EQUAL(&SIN6(ifa->ifa_netmask)->sin6_addr,
				&in6mask128)) ||
	    ((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_len == 0)
		return 0;
	else
		return 1;
#undef SIN6
}

/*
 * Subroutine for in6_ifaddloop() and in6_ifremloop().
 * This routine does actual work.
 */
static void
in6_ifloop_request(int cmd, struct ifaddr *ifa)
{
	struct sockaddr_in6 lo_sa;
	struct sockaddr_in6 all1_sa;
	struct rtentry *nrt = NULL;
	
	bzero(&lo_sa, sizeof(lo_sa));
	bzero(&all1_sa, sizeof(all1_sa));
	lo_sa.sin6_family = AF_INET6;
	lo_sa.sin6_len = sizeof(struct sockaddr_in6);
	all1_sa = lo_sa;
	lo_sa.sin6_addr = in6addr_loopback;
	all1_sa.sin6_addr = in6mask128;
	
	/* So we add or remove static loopback entry, here. */
	rtrequest(cmd, ifa->ifa_addr,
		  (struct sockaddr *)&lo_sa,
		  (struct sockaddr *)&all1_sa,
		  RTF_UP|RTF_HOST, &nrt);

	/*
	 * Make sure rt_ifa be equal to IFA, the second argument of the
	 * function.
	 * We need this because when we refer rt_ifa->ia6_flags in ip6_input,
	 * we assume that the rt_ifa points to the address instead of the
	 * loopback address.
	 */
	if (cmd == RTM_ADD && nrt && ifa != nrt->rt_ifa) {
		nrt->rt_ifa->ifa_refcnt--;
		ifa->ifa_refcnt++;
		nrt->rt_ifa = ifa;
	}
	if (nrt)
		nrt->rt_refcnt--;
}

/*
 * Add ownaddr as loopback rtentry, if necessary(ex. on p2p link).
 * Because, KAME needs loopback rtentry for ownaddr check in
 * ip6_input().
 */
static void
in6_ifaddloop(struct ifaddr *ifa)
{
	if (!in6_is_ifloop_auto(ifa)) {
		struct rtentry *rt;

		/* If there is no loopback entry, allocate one. */
		rt = rtalloc1(ifa->ifa_addr, 0, 0);
		if (rt == 0 || (rt->rt_ifp->if_flags & IFF_LOOPBACK) == 0)
			in6_ifloop_request(RTM_ADD, ifa);
		if (rt)
			rt->rt_refcnt--;
	}
}

/*
 * Remove loopback rtentry of ownaddr generated by in6_ifaddloop(),
 * if it exists.
 */
static void
in6_ifremloop(struct ifaddr *ifa)
{
	if (!in6_is_ifloop_auto(ifa)) {
		struct in6_ifaddr *ia;
		int ia_count = 0;

		/* If only one ifa for the loopback entry, delete it. */
		for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
			if (IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa),
					       &ia->ia_addr.sin6_addr)) {
				ia_count++;
				if (ia_count > 1)
					break;
			}
		}
		if (ia_count == 1)
			in6_ifloop_request(RTM_DELETE, ifa);
	}
}

/*
 * Subroutine for in6_ifaddproxy() and in6_ifremproxy().
 * This routine does actual work.
 * call in6_addmulti() when cmd == 1.
 * call in6_delmulti() when cmd == 2.
 */
static int
in6_ifproxy_request(int cmd, struct in6_ifaddr *ia)
{
	int error = 0;

	/*
	 * If we have an IPv6 dstaddr on adding p2p interface,
	 * join dstaddr's solicited multicast on necessary interface.
	 */
	if ((ia->ia_ifp->if_flags & IFF_POINTOPOINT) &&
	    ia->ia_dstaddr.sin6_family == AF_INET6 &&
	    !IN6_IS_ADDR_LINKLOCAL(&ia->ia_dstaddr.sin6_addr)) {
		struct in6_ifaddr *ia_lan;

		/*
		 * TODO: Join only on some specified interfaces by some
		 * configuration.
		 * Unsolicited Neighbor Advertisements will be also necessary.
		 *
		 * Now, join on interfaces which meets following.
		 *   -IFF_BROADCAST and IFF_MULTICAST
		 *    (NBMA is out of scope)
		 *   -the prefix value is same as p2p dstaddr
		 */
		for (ia_lan = in6_ifaddr; ia_lan; ia_lan = ia_lan->ia_next) {
			struct in6_addr llsol;

			if ((ia_lan->ia_ifp->if_flags &
			     (IFF_BROADCAST|IFF_MULTICAST)) !=
			    (IFF_BROADCAST|IFF_MULTICAST))
				continue;
			if (!IN6_ARE_MASKED_ADDR_EQUAL(IA6_IN6(ia),
						       IA6_IN6(ia_lan),
						       IA6_MASKIN6(ia_lan)))
				continue;
			if (ia_lan->ia_ifp == ia->ia_ifp)
				continue;

			/* init llsol */
			bzero(&llsol, sizeof(struct in6_addr));
			llsol.s6_addr16[0] = htons(0xff02);
			llsol.s6_addr16[1] = htons(ia_lan->ia_ifp->if_index);
			llsol.s6_addr32[1] = 0;
			llsol.s6_addr32[2] = htonl(1);
			llsol.s6_addr32[3] =
				ia->ia_dstaddr.sin6_addr.s6_addr32[3];
			llsol.s6_addr8[12] = 0xff;

			if (cmd == 1)
				(void)in6_addmulti(&llsol,
						   ia_lan->ia_ifp,
						   &error);
			else if (cmd == 2) {
				struct in6_multi *in6m;

				IN6_LOOKUP_MULTI(llsol,
						 ia_lan->ia_ifp,
						 in6m);
				if (in6m)
					in6_delmulti(in6m);
			}
		}
	}
	return error;
}

static int
in6_ifaddproxy(struct in6_ifaddr *ia)
{
	return(in6_ifproxy_request(1, ia));
}

static void
in6_ifremproxy(struct in6_ifaddr *ia)
{
	in6_ifproxy_request(2, ia);
}

int
in6_ifindex2scopeid(idx)
	int idx;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_in6 *sin6;

	if (idx < 0 || if_index < idx)
		return -1;
	ifp = ifindex2ifnet[idx];

	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))
			return sin6->sin6_scope_id & 0xffff;
	}

	return -1;
}

int
in6_mask2len(mask)
	struct in6_addr *mask;
{
	int x, y;

	for (x = 0; x < sizeof(*mask); x++) {
		if (mask->s6_addr8[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < 8; y++) {
			if ((mask->s6_addr8[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * 8 + y;
}

void
in6_len2mask(mask, len)
	struct in6_addr *mask;
	int len;
{
	int i;

	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		mask->s6_addr8[i] = 0xff;
	if (len % 8)
		mask->s6_addr8[i] = (0xff00 >> (len % 8)) & 0xff;
}

int	in6_interfaces;		/* number of external internet interfaces */

#define ifa2ia6(ifa)	((struct in6_ifaddr *)(ifa))
#define ia62ifa(ia6)	((struct ifaddr *)(ia6))

int
in6_control(so, cmd, data, ifp, p)
	struct	socket *so;
	u_long cmd;
	caddr_t	data;
	struct ifnet *ifp;
	struct proc *p;
{
	struct	in6_ifreq *ifr = (struct in6_ifreq *)data;
	struct	in6_ifaddr *ia, *oia;
	struct	in6_aliasreq *ifra = (struct in6_aliasreq *)data;
	struct	sockaddr_in6 oldaddr, net;
	int	error = 0, hostIsNew, prefixIsNew;
	int privileged;

	privileged = 0;
	if (p && !suser(p))
		privileged++;

	/*
	 * xxx should prevent processes for link-local addresses?
	 */
#if NGIF > 0
	if (ifp && ifp->if_type == IFT_GIF) {
		switch (cmd) {
		case SIOCSIFPHYADDR_IN6:
			if (!privileged)
				return(EPERM);
			/*fall through*/
		case SIOCGIFPSRCADDR_IN6:
		case SIOCGIFPDSTADDR_IN6:
			return gif_ioctl(ifp, cmd, data);
		}
	}
#endif

	if (ifp == 0)
		return(EOPNOTSUPP);

	switch (cmd) {
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
		if (!privileged)
			return(EPERM);
		/*fall through*/
	case SIOCGIFINFO_IN6:
	case SIOCGDRLST_IN6:
	case SIOCGPRLST_IN6:
	case SIOCGNBRINFO_IN6:
		return(nd6_ioctl(cmd, data, ifp));
	}

	switch (cmd) {
	case SIOCSIFPREFIX_IN6:
	case SIOCDIFPREFIX_IN6:
	case SIOCAIFPREFIX_IN6:
	case SIOCCIFPREFIX_IN6:
	case SIOCSGIFPREFIX_IN6:
		if (!privileged)
			return(EPERM);
		/*fall through*/
	case SIOCGIFPREFIX_IN6:
		return(in6_prefix_ioctl(so, cmd, data, ifp));
	}

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if (!privileged)
			return(EPERM);
		/*fall through*/
	case SIOCGLIFADDR:
		return in6_lifaddr_ioctl(so, cmd, data, ifp, p);
	}

	/*
	 * Find address for this interface, if it exists.
	 */
	{

		struct sockaddr_in6 *sa6 =
			(struct sockaddr_in6 *)&ifra->ifra_addr;

		if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr)) {
			if (sa6->sin6_addr.s6_addr16[1] == 0) {
				/* interface ID is not embedded by the user */
				sa6->sin6_addr.s6_addr16[1] =
					htons(ifp->if_index);
			} else
				if (sa6->sin6_addr.s6_addr16[1] !=
				    htons(ifp->if_index))
					return(EINVAL);	/* ifid is contradict */
			if (sa6->sin6_scope_id) {
				if (sa6->sin6_scope_id !=
				    (u_int32_t)ifp->if_index)
					return(EINVAL);
				sa6->sin6_scope_id = 0; /* XXX: good way? */
			}
		}
	}
 	ia = in6ifa_ifpwithaddr(ifp, &ifra->ifra_addr.sin6_addr);

	switch (cmd) {

	case SIOCDIFADDR_IN6:
		if (ia == 0)
			return(EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCAIFADDR_IN6:
	case SIOCSIFADDR_IN6:
	case SIOCSIFNETMASK_IN6:
	case SIOCSIFDSTADDR_IN6:
		if (!privileged)
			return(EPERM);
		if (ia == 0) {
			ia = (struct in6_ifaddr *)
				malloc(sizeof(*ia), M_IFADDR, M_WAITOK);
			if (ia == NULL)
				return (ENOBUFS);
			bzero((caddr_t)ia, sizeof(*ia));
			ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ia->ia_ifa.ifa_dstaddr
				= (struct sockaddr *)&ia->ia_dstaddr;
			ia->ia_ifa.ifa_netmask
				= (struct sockaddr *)&ia->ia_prefixmask;

			ia->ia_ifp = ifp;
			if ((oia = in6_ifaddr) != NULL) {
				for ( ; oia->ia_next; oia = oia->ia_next)
					continue;
				oia->ia_next = ia;
			} else
				in6_ifaddr = ia;
			TAILQ_INSERT_TAIL(&ifp->if_addrlist,
				(struct ifaddr *)ia, ifa_list);
			if ((ifp->if_flags & IFF_LOOPBACK) == 0)
				in6_interfaces++;	/*XXX*/
		}

		if (cmd == SIOCAIFADDR_IN6) {
			/* sanity for overflow - beware unsigned */
			struct in6_addrlifetime *lt;
			lt = &ifra->ifra_lifetime;
			if (lt->ia6t_vltime != ND6_INFINITE_LIFETIME
			 && lt->ia6t_vltime + time_second < time_second) {
				return EINVAL;
			}
			if (lt->ia6t_pltime != ND6_INFINITE_LIFETIME
			 && lt->ia6t_pltime + time_second < time_second) {
				return EINVAL;
			}
		}
		break;

	case SIOCGIFADDR_IN6:
		/* This interface is basically deprecated. use SIOCGIFCONF. */
		/* fall through */
	case SIOCGIFAFLAG_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFALIFETIME_IN6:
		/* must think again about its semantics */
		if (ia == 0)
			return(EADDRNOTAVAIL);
		break;
	case SIOCSIFALIFETIME_IN6:
	    {
		struct in6_addrlifetime *lt;

		if (!privileged)
			return(EPERM);
		if (ia == 0)
			return(EADDRNOTAVAIL);
		/* sanity for overflow - beware unsigned */
		lt = &ifr->ifr_ifru.ifru_lifetime;
		if (lt->ia6t_vltime != ND6_INFINITE_LIFETIME
		 && lt->ia6t_vltime + time_second < time_second) {
			return EINVAL;
		}
		if (lt->ia6t_pltime != ND6_INFINITE_LIFETIME
		 && lt->ia6t_pltime + time_second < time_second) {
			return EINVAL;
		}
		break;
	    }
	}

	switch (cmd) {

	case SIOCGIFADDR_IN6:
		ifr->ifr_addr = ia->ia_addr;
		break;

	case SIOCGIFDSTADDR_IN6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return(EINVAL);
		ifr->ifr_dstaddr = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK_IN6:
		ifr->ifr_addr = ia->ia_prefixmask;
		break;

	case SIOCGIFAFLAG_IN6:
		ifr->ifr_ifru.ifru_flags6 = ia->ia6_flags;
		break;

	case SIOCGIFSTAT_IN6:
		if (ifp == NULL)
			return EINVAL;
		if (in6_ifstat == NULL || ifp->if_index >= in6_ifstatmax
		 || in6_ifstat[ifp->if_index] == NULL) {
			/* return EAFNOSUPPORT? */
			bzero(&ifr->ifr_ifru.ifru_stat,
				sizeof(ifr->ifr_ifru.ifru_stat));
		} else
			ifr->ifr_ifru.ifru_stat = *in6_ifstat[ifp->if_index];
		break;

	case SIOCGIFSTAT_ICMP6:
		if (ifp == NULL)
			return EINVAL;
		if (icmp6_ifstat == NULL || ifp->if_index >= icmp6_ifstatmax ||
		    icmp6_ifstat[ifp->if_index] == NULL) {
			/* return EAFNOSUPPORT? */
			bzero(&ifr->ifr_ifru.ifru_stat,
				sizeof(ifr->ifr_ifru.ifru_icmp6stat));
		} else
			ifr->ifr_ifru.ifru_icmp6stat =
				*icmp6_ifstat[ifp->if_index];
		break;

	case SIOCSIFDSTADDR_IN6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return(EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = ifr->ifr_dstaddr;

		/* link-local index check */
		if (IN6_IS_ADDR_LINKLOCAL(&ia->ia_dstaddr.sin6_addr)) {
			if (ia->ia_dstaddr.sin6_addr.s6_addr16[1] == 0) {
				/* interface ID is not embedded by the user */
				ia->ia_dstaddr.sin6_addr.s6_addr16[1]
					= htons(ifp->if_index);
			} else
				if (ia->ia_dstaddr.sin6_addr.s6_addr16[1] !=
				    htons(ifp->if_index)) {
					ia->ia_dstaddr = oldaddr;
					return(EINVAL);	/* ifid is contradict */
				}
		}

		if (ifp->if_ioctl && (error = (ifp->if_ioctl)
				      (ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			return(error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
				(struct sockaddr *)&ia->ia_dstaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		break;

	case SIOCGIFALIFETIME_IN6:
		ifr->ifr_ifru.ifru_lifetime = ia->ia6_lifetime;
		break;

	case SIOCSIFALIFETIME_IN6:
		ia->ia6_lifetime = ifr->ifr_ifru.ifru_lifetime;
		/* for sanity */
		if (ia->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
			ia->ia6_lifetime.ia6t_expire =
				time_second + ia->ia6_lifetime.ia6t_vltime;
		} else
			ia->ia6_lifetime.ia6t_expire = 0;
		if (ia->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
			ia->ia6_lifetime.ia6t_preferred =
				time_second + ia->ia6_lifetime.ia6t_pltime;
		} else
			ia->ia6_lifetime.ia6t_preferred = 0;
		break;

	case SIOCSIFADDR_IN6:
		return(in6_ifinit(ifp, ia, &ifr->ifr_addr, 1));

	case SIOCSIFNETMASK_IN6:
		ia->ia_prefixmask = ifr->ifr_addr;
		bzero(&net, sizeof(net));
		net.sin6_len = sizeof(struct sockaddr_in6);
		net.sin6_family = AF_INET6;
		net.sin6_port = htons(0);
		net.sin6_flowinfo = htonl(0);
		net.sin6_addr.s6_addr32[0]
			= ia->ia_addr.sin6_addr.s6_addr32[0] &
				ia->ia_prefixmask.sin6_addr.s6_addr32[0];
		net.sin6_addr.s6_addr32[1]
			= ia->ia_addr.sin6_addr.s6_addr32[1] &
				ia->ia_prefixmask.sin6_addr.s6_addr32[1];
		net.sin6_addr.s6_addr32[2]
			= ia->ia_addr.sin6_addr.s6_addr32[2] &
				ia->ia_prefixmask.sin6_addr.s6_addr32[2];
		net.sin6_addr.s6_addr32[3]
			= ia->ia_addr.sin6_addr.s6_addr32[3] &
				ia->ia_prefixmask.sin6_addr.s6_addr32[3];
		ia->ia_net = net;
		break;

	case SIOCAIFADDR_IN6:
		prefixIsNew = 0;
		hostIsNew = 1;

		if (ifra->ifra_addr.sin6_len == 0) {
			ifra->ifra_addr = ia->ia_addr;
			hostIsNew = 0;
		} else if (IN6_ARE_ADDR_EQUAL(&ifra->ifra_addr.sin6_addr,
					      &ia->ia_addr.sin6_addr))
			hostIsNew = 0;

		if (ifra->ifra_prefixmask.sin6_len) {
			in6_ifscrub(ifp, ia);
			ia->ia_prefixmask = ifra->ifra_prefixmask;
			prefixIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin6_family == AF_INET6)) {
			in6_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			/* link-local index check: should be a separate function? */
			if (IN6_IS_ADDR_LINKLOCAL(&ia->ia_dstaddr.sin6_addr)) {
				if (ia->ia_dstaddr.sin6_addr.s6_addr16[1] == 0) {
					/*
					 * interface ID is not embedded by
					 * the user
					 */
					ia->ia_dstaddr.sin6_addr.s6_addr16[1]
						= htons(ifp->if_index);
				} else
					if (ia->ia_dstaddr.sin6_addr.s6_addr16[1] !=
					    htons(ifp->if_index)) {
						ia->ia_dstaddr = oldaddr;
						return(EINVAL);	/* ifid is contradict */
					}
			}
			prefixIsNew = 1; /* We lie; but effect's the same */
		}
		if (ifra->ifra_addr.sin6_family == AF_INET6 &&
		    (hostIsNew || prefixIsNew))
			error = in6_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if (ifra->ifra_addr.sin6_family == AF_INET6
		    && hostIsNew && (ifp->if_flags & IFF_MULTICAST)) {
			int error_local = 0;

			/*
			 * join solicited multicast addr for new host id
			 */
			struct in6_addr llsol;
			bzero(&llsol, sizeof(struct in6_addr));
			llsol.s6_addr16[0] = htons(0xff02);
			llsol.s6_addr16[1] = htons(ifp->if_index);
			llsol.s6_addr32[1] = 0;
			llsol.s6_addr32[2] = htonl(1);
			llsol.s6_addr32[3] =
				ifra->ifra_addr.sin6_addr.s6_addr32[3];
			llsol.s6_addr8[12] = 0xff;
			(void)in6_addmulti(&llsol, ifp, &error_local);
			if (error == 0)
				error = error_local;
		}
		/* Join dstaddr's solicited multicast if necessary. */
		if (nd6_proxyall && hostIsNew) {
			int error_local;

			error_local = in6_ifaddproxy(ia);
			if (error == 0)
				error = error_local;
		}

		ia->ia6_flags = ifra->ifra_flags;
		ia->ia6_flags &= ~IN6_IFF_DUPLICATED;	/*safety*/

		ia->ia6_lifetime = ifra->ifra_lifetime;
		/* for sanity */
		if (ia->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
			ia->ia6_lifetime.ia6t_expire =
				time_second + ia->ia6_lifetime.ia6t_vltime;
		} else
			ia->ia6_lifetime.ia6t_expire = 0;
		if (ia->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
			ia->ia6_lifetime.ia6t_preferred =
				time_second + ia->ia6_lifetime.ia6t_pltime;
		} else
			ia->ia6_lifetime.ia6t_preferred = 0;

		/*
		 * Perform DAD, if needed.
		 * XXX It may be of use, if we can administratively
		 * disable DAD.
		 */
		switch (ifp->if_type) {
		case IFT_ARCNET:
		case IFT_ETHER:
		case IFT_FDDI:
			ia->ia6_flags |= IN6_IFF_TENTATIVE;
			nd6_dad_start((struct ifaddr *)ia, NULL);
			break;
#ifdef IFT_DUMMY
		case IFT_DUMMY:
#endif
		case IFT_FAITH:
		case IFT_GIF:
		case IFT_LOOP:
		default:
			break;
		}

		if (hostIsNew) {
			int iilen;
			int error_local = 0;

			iilen = (sizeof(ia->ia_prefixmask.sin6_addr) << 3) -
				in6_mask2len(&ia->ia_prefixmask.sin6_addr);
			error_local = in6_prefix_add_ifid(iilen, ia);
			if (error == 0)
				error = error_local;
		}

		return(error);

	case SIOCDIFADDR_IN6:
		in6_ifscrub(ifp, ia);

		if (ifp->if_flags & IFF_MULTICAST) {
			/*
			 * delete solicited multicast addr for deleting host id
			 */
			struct in6_multi *in6m;
			struct in6_addr llsol;
			bzero(&llsol, sizeof(struct in6_addr));
			llsol.s6_addr16[0] = htons(0xff02);
			llsol.s6_addr16[1] = htons(ifp->if_index);
			llsol.s6_addr32[1] = 0;
			llsol.s6_addr32[2] = htonl(1);
			llsol.s6_addr32[3] =
				ia->ia_addr.sin6_addr.s6_addr32[3];
			llsol.s6_addr8[12] = 0xff;

			IN6_LOOKUP_MULTI(llsol, ifp, in6m);
			if (in6m)
				in6_delmulti(in6m);
		}
		/* Leave dstaddr's solicited multicast if necessary. */
		if (nd6_proxyall)
			in6_ifremproxy(ia);

		TAILQ_REMOVE(&ifp->if_addrlist, (struct ifaddr *)ia, ifa_list);
		oia = ia;
		if (oia == (ia = in6_ifaddr))
			in6_ifaddr = ia->ia_next;
		else {
			while (ia->ia_next && (ia->ia_next != oia))
				ia = ia->ia_next;
			if (ia->ia_next)
				ia->ia_next = oia->ia_next;
			else
				printf("Didn't unlink in6_ifaddr from list\n");
		}
		{
			int iilen;

			iilen = (sizeof(oia->ia_prefixmask.sin6_addr) << 3) -
				in6_mask2len(&oia->ia_prefixmask.sin6_addr);
			in6_prefix_remove_ifid(iilen, oia);
		}
		IFAFREE((&oia->ia_ifa));
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return(EOPNOTSUPP);
		return((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return(0);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (???)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		add the specified prefix, filling hostid part from
 *		the first link-local address.  prefixlen must be <= 64.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in6_ioctl()
 *
 * NOTE: SIOCALIFADDR(with IFLR_PREFIX set) allows prefixlen less than 64.
 * this is to accomodate address naming scheme other than RFC2374,
 * in the future.
 * RFC2373 defines interface id to be 64bit, but it allows non-RFC2374
 * address encoding scheme. (see figure on page 8)
 */
static int
in6_lifaddr_ioctl(so, cmd, data, ifp, p)
	struct socket *so;
	u_long cmd;
	caddr_t	data;
	struct ifnet *ifp;
	struct proc *p;
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in6_lifaddr_ioctl");
		/*NOTRECHED*/
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/*FALLTHROUGH*/
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		if (iflr->addr.__ss_family != AF_INET6)
			return EINVAL;
		if (iflr->addr.__ss_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		/* XXX need improvement */
		if (iflr->dstaddr.__ss_family
		 && iflr->dstaddr.__ss_family != AF_INET6)
			return EINVAL;
		if (iflr->dstaddr.__ss_family
		 && iflr->dstaddr.__ss_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		break;
	default: /*shouldn't happen*/
		return EOPNOTSUPP;
	}
	if (sizeof(struct in6_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in6_aliasreq ifra;
		struct in6_addr *hostid = NULL;
		int prefixlen;

		if ((iflr->flags & IFLR_PREFIX) != 0) {
			struct sockaddr_in6 *sin6;

			/*
			 * hostid is to fill in the hostid part of the
			 * address.  hostid points to the first link-local
			 * address attached to the interface.
			 */
			ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp);
			if (!ifa)
				return EADDRNOTAVAIL;
			hostid = IFA_IN6(ifa);

		 	/* prefixlen must be <= 64. */
			if (64 < iflr->prefixlen)
				return EINVAL;
			prefixlen = iflr->prefixlen;

			/* hostid part must be zero. */
			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			if (sin6->sin6_addr.s6_addr32[2] != 0
			 || sin6->sin6_addr.s6_addr32[3] != 0) {
				return EINVAL;
			}
		} else
			prefixlen = iflr->prefixlen;

		/* copy args to in6_aliasreq, perform ioctl(SIOCAIFADDR_IN6). */
		bzero(&ifra, sizeof(ifra));
		bcopy(iflr->iflr_name, ifra.ifra_name,
			sizeof(ifra.ifra_name));

		bcopy(&iflr->addr, &ifra.ifra_addr, iflr->addr.__ss_len);
		if (hostid) {
			/* fill in hostid part */
			ifra.ifra_addr.sin6_addr.s6_addr32[2] =
				hostid->s6_addr32[2];
			ifra.ifra_addr.sin6_addr.s6_addr32[3] =
				hostid->s6_addr32[3];
		}

		if (iflr->dstaddr.__ss_family) {	/*XXX*/
			bcopy(&iflr->dstaddr, &ifra.ifra_dstaddr,
				iflr->dstaddr.__ss_len);
			if (hostid) {
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[2] =
					hostid->s6_addr32[2];
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[3] =
					hostid->s6_addr32[3];
			}
		}

		ifra.ifra_prefixmask.sin6_family = AF_INET6;
		ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
		in6_len2mask(&ifra.ifra_prefixmask.sin6_addr, prefixlen);

		ifra.ifra_flags = iflr->flags & ~IFLR_PREFIX;
		return in6_control(so, SIOCAIFADDR_IN6, (caddr_t)&ifra, ifp, p);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in6_ifaddr *ia;
		struct in6_addr mask, candidate, match;
		struct sockaddr_in6 *sin6;
		int cmp;

		bzero(&mask, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in6_len2mask(&mask, iflr->prefixlen);

			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			bcopy(&sin6->sin6_addr, &match, sizeof(match));
			match.s6_addr32[0] &= mask.s6_addr32[0];
			match.s6_addr32[1] &= mask.s6_addr32[1];
			match.s6_addr32[2] &= mask.s6_addr32[2];
			match.s6_addr32[3] &= mask.s6_addr32[3];

			/* if you set extra bits, that's wrong */
			if (bcmp(&match, &sin6->sin6_addr, sizeof(match)))
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/*XXX*/
			} else {
				/* on deleting an address, do exact match */
				in6_len2mask(&mask, 128);
				sin6 = (struct sockaddr_in6 *)&iflr->addr;
				bcopy(&sin6->sin6_addr, &match, sizeof(match));

				cmp = 1;
			}
		}

		for (ifa = ifp->if_addrlist.tqh_first;
		     ifa;
		     ifa = ifa->ifa_list.tqe_next)
		{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			if (!cmp)
				break;
			bcopy(IFA_IN6(ifa), &candidate, sizeof(candidate));
			candidate.s6_addr32[0] &= mask.s6_addr32[0];
			candidate.s6_addr32[1] &= mask.s6_addr32[1];
			candidate.s6_addr32[2] &= mask.s6_addr32[2];
			candidate.s6_addr32[3] &= mask.s6_addr32[3];
			if (IN6_ARE_ADDR_EQUAL(&candidate, &match))
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = ifa2ia6(ifa);

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			bcopy(&ia->ia_addr, &iflr->addr, ia->ia_addr.sin6_len);

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &iflr->dstaddr,
					ia->ia_dstaddr.sin6_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
				in6_mask2len(&ia->ia_prefixmask.sin6_addr);

			iflr->flags = ia->ia6_flags;	/*XXX*/

			return 0;
		} else {
			struct in6_aliasreq ifra;

			/* fill in6_aliasreq and do ioctl(SIOCDIFADDR_IN6) */
			bzero(&ifra, sizeof(ifra));
			bcopy(iflr->iflr_name, ifra.ifra_name,
				sizeof(ifra.ifra_name));

			bcopy(&ia->ia_addr, &ifra.ifra_addr,
				ia->ia_addr.sin6_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &ifra.ifra_dstaddr,
					ia->ia_dstaddr.sin6_len);
			}
			bcopy(&ia->ia_prefixmask, &ifra.ifra_dstaddr,
				ia->ia_prefixmask.sin6_len);

			ifra.ifra_flags = ia->ia6_flags;
			return in6_control(so, SIOCDIFADDR_IN6, (caddr_t)&ifra,
				ifp, p);
		}
	    }
	}

	return EOPNOTSUPP;	/*just for safety*/
}

/*
 * Delete any existing route for an interface.
 */
void
in6_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct in6_ifaddr *ia;
{
	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	if (ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT))
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
	else
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
	ia->ia_flags &= ~IFA_ROUTE;

	/* Remove ownaddr's loopback rtentry, if it exists. */
	in6_ifremloop(&(ia->ia_ifa));
}

/*
 * Initialize an interface's intetnet6 address
 * and routing table entry.
 */
int
in6_ifinit(ifp, ia, sin6, scrub)
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	struct sockaddr_in6 *sin6;
	int scrub;
{
	struct	sockaddr_in6 oldaddr;
	int	error, flags = RTF_UP;
	int	s = splimp();

	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin6;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	   (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		splx(s);
		ia->ia_addr = oldaddr;
		return(error);
	}

	switch (ifp->if_type) {
	case IFT_ARCNET:
	case IFT_ETHER:
	case IFT_FDDI:
		ia->ia_ifa.ifa_rtrequest = nd6_rtrequest;
		ia->ia_ifa.ifa_flags |= RTF_CLONING;
		break;
	case IFT_PPP:
		ia->ia_ifa.ifa_rtrequest = nd6_p2p_rtrequest;
		ia->ia_ifa.ifa_flags |= RTF_CLONING;
		break;
	}

	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in6_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	/* xxx
	 * in_socktrim
	 */
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin6_family != AF_INET6)
			return(0);
		flags |= RTF_HOST;
	}
	if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags)) == 0)
		ia->ia_flags |= IFA_ROUTE;

	/* Add ownaddr as loopback rtentry, if necessary(ex. on p2p link). */
	in6_ifaddloop(&(ia->ia_ifa));

	return(error);
}

/*
 * Add an address to the list of IP6 multicast addresses for a
 * given interface.
 */
struct	in6_multi *
in6_addmulti(maddr6, ifp, errorp)
	register struct in6_addr *maddr6;
	register struct ifnet *ifp;
	int *errorp;
{
	struct	in6_multi *in6m;
	struct sockaddr_in6 sin6;
	struct ifmultiaddr *ifma;
	int	s = splnet();

	*errorp = 0;

	/*
	 * Call generic routine to add membership or increment
	 * refcount.  It wants addresses in the form of a sockaddr,
	 * so we build one here (being careful to zero the unused bytes).
	 */
	bzero(&sin6, sizeof sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof sin6;
	sin6.sin6_addr = *maddr6;
	*errorp = if_addmulti(ifp, (struct sockaddr *)&sin6, &ifma);
	if (*errorp) {
		splx(s);
		return 0;
	}

	/*
	 * If ifma->ifma_protospec is null, then if_addmulti() created
	 * a new record.  Otherwise, we are done.
	 */
	if (ifma->ifma_protospec != 0)
		return ifma->ifma_protospec;

	/* XXX - if_addmulti uses M_WAITOK.  Can this really be called
	   at interrupt time?  If so, need to fix if_addmulti. XXX */
	in6m = (struct in6_multi *)malloc(sizeof(*in6m), M_IPMADDR, M_NOWAIT);
	if (in6m == NULL) {
		splx(s);
		return (NULL);
	}

	bzero(in6m, sizeof *in6m);
	in6m->in6m_addr = *maddr6;
	in6m->in6m_ifp = ifp;
	in6m->in6m_ifma = ifma;
	ifma->ifma_protospec = in6m;
	LIST_INSERT_HEAD(&in6_multihead, in6m, in6m_entry);

	/*
	 * Let MLD6 know that we have joined a new IP6 multicast
	 * group.
	 */
	mld6_start_listening(in6m);
	splx(s);
	return(in6m);
}

/*
 * Delete a multicast address record.
 */
void
in6_delmulti(in6m)
	struct in6_multi *in6m;
{
	struct ifmultiaddr *ifma = in6m->in6m_ifma;
	int	s = splnet();

	if (ifma->ifma_refcount == 1) {
		/*
		 * No remaining claims to this record; let MLD6 know
		 * that we are leaving the multicast group.
		 */
		mld6_stop_listening(in6m);
		ifma->ifma_protospec = 0;
		LIST_REMOVE(in6m, in6m_entry);
		free(in6m, M_IPMADDR);
	}
	/* XXX - should be separate API for when we have an ifma? */
	if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
	splx(s);
}

/*
 * Find an IPv6 interface link-local address specific to an interface.
 */
struct in6_ifaddr *
in6ifa_ifpforlinklocal(ifp)
	struct ifnet *ifp;
{
	register struct ifaddr *ifa;

	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_IS_ADDR_LINKLOCAL(IFA_IN6(ifa)))
			break;
	}

	return((struct in6_ifaddr *)ifa);
}


/*
 * find the internet address corresponding to a given interface and address.
 */
struct in6_ifaddr *
in6ifa_ifpwithaddr(ifp, addr)
	struct ifnet *ifp;
	struct in6_addr *addr;
{
	register struct ifaddr *ifa;

	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_ARE_ADDR_EQUAL(addr, IFA_IN6(ifa)))
			break;
	}

	return((struct in6_ifaddr *)ifa);
}

/*
 * Convert IP6 address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
static int ip6round = 0;
char *
ip6_sprintf(addr)
register struct in6_addr *addr;
{
	static char ip6buf[8][48];
	register int i;
	register char *cp;
	register u_short *a = (u_short *)addr;
	register u_char *d;
	int dcolon = 0;

	ip6round = (ip6round + 1) & 7;
	cp = ip6buf[ip6round];

	for (i = 0; i < 8; i++) {
		if (dcolon == 1) {
			if (*a == 0) {
				if (i == 7)
					*cp++ = ':';
				a++;
				continue;
			} else
				dcolon = 2;
		}
		if (*a == 0) {
			if (dcolon == 0 && *(a + 1) == 0) {
				if (i == 0)
					*cp++ = ':';
				*cp++ = ':';
				dcolon = 1;
			} else {
				*cp++ = '0';
				*cp++ = ':';
			}
			a++;
			continue;
		}
		d = (u_char *)a;
		*cp++ = digits[*d >> 4];
		*cp++ = digits[*d++ & 0xf];
		*cp++ = digits[*d >> 4];
		*cp++ = digits[*d & 0xf];
		*cp++ = ':';
		a++;
	}
	*--cp = 0;
	return(ip6buf[ip6round]);
}

int
in6_localaddr(in6)
	struct in6_addr *in6;
{
	struct in6_ifaddr *ia;

	if (IN6_IS_ADDR_LOOPBACK(in6) || IN6_IS_ADDR_LINKLOCAL(in6))
		return 1;

	for (ia = in6_ifaddr; ia; ia = ia->ia_next)
		if (IN6_ARE_MASKED_ADDR_EQUAL(in6, &ia->ia_addr.sin6_addr,
					      &ia->ia_prefixmask.sin6_addr))
			return 1;

	return (0);
}

/*
 * Get a scope of the address. Node-local, link-local, site-local or global.
 */
int
in6_addrscope (addr)
struct in6_addr *addr;
{
	int scope;

	if (addr->s6_addr8[0] == 0xfe) {
		scope = addr->s6_addr8[1] & 0xc0;

		switch (scope) {
		case 0x80:
			return IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case 0xc0:
			return IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return IPV6_ADDR_SCOPE_GLOBAL; /* just in case */
			break;
		}
	}


	if (addr->s6_addr8[0] == 0xff) {
		scope = addr->s6_addr8[1] & 0x0f;

		/*
		 * due to other scope such as reserved,
		 * return scope doesn't work.
		 */
		switch (scope) {
		case IPV6_ADDR_SCOPE_NODELOCAL:
			return IPV6_ADDR_SCOPE_NODELOCAL;
			break;
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			return IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case IPV6_ADDR_SCOPE_SITELOCAL:
			return IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return IPV6_ADDR_SCOPE_GLOBAL;
			break;
		}
	}

	if (bcmp(&in6addr_loopback, addr, sizeof(addr) - 1) == 0) {
		if (addr->s6_addr8[15] == 1) /* loopback */
			return IPV6_ADDR_SCOPE_NODELOCAL;
		if (addr->s6_addr8[15] == 0) /* unspecified */
			return IPV6_ADDR_SCOPE_LINKLOCAL;
	}

	return IPV6_ADDR_SCOPE_GLOBAL;
}

/*
 * return length of part which dst and src are equal
 * hard coding...
 */

int
in6_matchlen(src, dst)
struct in6_addr *src, *dst;
{
	int match = 0;
	u_char *s = (u_char *)src, *d = (u_char *)dst;
	u_char *lim = s + 16, r;

	while (s < lim)
		if ((r = (*d++ ^ *s++)) != 0) {
			while (r < 128) {
				match++;
				r <<= 1;
			}
			break;
		} else
			match += 8;
	return match;
}

int
in6_are_prefix_equal(p1, p2, len)
	struct in6_addr *p1, *p2;
	int len;
{
	int bytelen, bitlen;

	/* sanity check */
	if (0 > len || len > 128) {
		log(LOG_ERR, "in6_are_prefix_equal: invalid prefix length(%d)\n",
		    len);
		return(0);
	}

	bytelen = len / 8;
	bitlen = len % 8;

	if (bcmp(&p1->s6_addr, &p2->s6_addr, bytelen))
		return(0);
	if (p1->s6_addr[bytelen] >> (8 - bitlen) !=
	    p2->s6_addr[bytelen] >> (8 - bitlen))
		return(0);

	return(1);
}

void
in6_prefixlen2mask(maskp, len)
	struct in6_addr *maskp;
	int len;
{
	u_char maskarray[8] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	int bytelen, bitlen, i;

	/* sanity check */
	if (0 > len || len > 128) {
		log(LOG_ERR, "in6_prefixlen2mask: invalid prefix length(%d)\n",
		    len);
		return;
	}

	bzero(maskp, sizeof(*maskp));
	bytelen = len / 8;
	bitlen = len % 8;
	for (i = 0; i < bytelen; i++)
		maskp->s6_addr[i] = 0xff;
	if (bitlen)
		maskp->s6_addr[bytelen] = maskarray[bitlen - 1];
}

/*
 * return the best address out of the same scope
 */

struct in6_ifaddr *
in6_ifawithscope(ifp, dst)
	register struct ifnet *ifp;
	register struct in6_addr *dst;
{
	int dst_scope =	in6_addrscope(dst), blen = -1, tlen;
	struct ifaddr *ifa;
	struct in6_ifaddr *besta = NULL, *ia;
	struct in6_ifaddr *dep[2];	/*last-resort: deprecated*/

	dep[0] = dep[1] = NULL;

	/*
	 * We first look for addresses in the same scope.
	 * If there is one, return it.
	 * If two or more, return one which matches the dst longest.
	 * If none, return one of global addresses assigned other ifs.
	 */
	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_ANYCAST)
			continue; /* XXX: is there any case to allow anycast? */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_NOTREADY)
			continue; /* don't use this interface */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DETACHED)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DEPRECATED) {
			if (ip6_use_deprecated)
				dep[0] = (struct in6_ifaddr *)ifa;
			continue;
		}

		if (dst_scope == in6_addrscope(IFA_IN6(ifa))) {
			/*
			 * call in6_matchlen() as few as possible
			 */
			if (besta) {
				if (blen == -1)
					blen = in6_matchlen(&besta->ia_addr.sin6_addr, dst);
				tlen = in6_matchlen(IFA_IN6(ifa), dst);
				if (tlen > blen) {
					blen = tlen;
					besta = (struct in6_ifaddr *)ifa;
				}
			} else
				besta = (struct in6_ifaddr *)ifa;
		}
	}
	if (besta)
		return besta;

	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		if (IPV6_ADDR_SCOPE_GLOBAL !=
		    in6_addrscope(&(ia->ia_addr.sin6_addr)))
			continue;
		/* XXX: is there any case to allow anycast? */
		if ((ia->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;
		if ((ia->ia6_flags & IN6_IFF_NOTREADY) != 0)
			continue;
		if ((ia->ia6_flags & IN6_IFF_DETACHED) != 0)
			continue;
		if ((ia->ia6_flags & IN6_IFF_DEPRECATED) != 0) {
			if (ip6_use_deprecated)
				dep[1] = (struct in6_ifaddr *)ifa;
			continue;
		}
		return ia;
	}

	/* use the last-resort values, that are, deprecated addresses */
	if (dep[0])
		return dep[0];
	if (dep[1])
		return dep[1];

	return NULL;
}

/*
 * return the best address out of the same scope. if no address was
 * found, return the first valid address from designated IF.
 */

struct in6_ifaddr *
in6_ifawithifp(ifp, dst)
	register struct ifnet *ifp;
	register struct in6_addr *dst;
{
	int dst_scope =	in6_addrscope(dst), blen = -1, tlen;
	struct ifaddr *ifa;
	struct in6_ifaddr *besta = 0;
	struct in6_ifaddr *dep[2];	/*last-resort: deprecated*/

	dep[0] = dep[1] = NULL;

	/*
	 * We first look for addresses in the same scope.
	 * If there is one, return it.
	 * If two or more, return one which matches the dst longest.
	 * If none, return one of global addresses assigned other ifs.
	 */
	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_ANYCAST)
			continue; /* XXX: is there any case to allow anycast? */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_NOTREADY)
			continue; /* don't use this interface */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DETACHED)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DEPRECATED) {
			if (ip6_use_deprecated)
				dep[0] = (struct in6_ifaddr *)ifa;
			continue;
		}

		if (dst_scope == in6_addrscope(IFA_IN6(ifa))) {
			/*
			 * call in6_matchlen() as few as possible
			 */
			if (besta) {
				if (blen == -1)
					blen = in6_matchlen(&besta->ia_addr.sin6_addr, dst);
				tlen = in6_matchlen(IFA_IN6(ifa), dst);
				if (tlen > blen) {
					blen = tlen;
					besta = (struct in6_ifaddr *)ifa;
				}
			} else
				besta = (struct in6_ifaddr *)ifa;
		}
	}
	if (besta)
		return(besta);

	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_ANYCAST)
			continue; /* XXX: is there any case to allow anycast? */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_NOTREADY)
			continue; /* don't use this interface */
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DETACHED)
			continue;
		if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DEPRECATED) {
			if (ip6_use_deprecated)
				dep[1] = (struct in6_ifaddr *)ifa;
			continue;
		}

		return (struct in6_ifaddr *)ifa;
	}

	/* use the last-resort values, that are, deprecated addresses */
	if (dep[0])
		return dep[0];
	if (dep[1])
		return dep[1];

	return NULL;
}

/*
 * perform DAD when interface becomes IFF_UP.
 */
void
in6_if_up(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	struct in6_ifaddr *ia;
	struct sockaddr_dl *sdl;
	int type;
	struct ether_addr ea;
	int off;
	int dad_delay;		/* delay ticks before DAD output */

	bzero(&ea, sizeof(ea));
	sdl = NULL;

	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family == AF_INET6
		 && IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr)) {
			goto dad;
		}
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		break;
	}

	switch (ifp->if_type) {
	case IFT_SLIP:
	case IFT_PPP:
#ifdef IFT_DUMMY
	case IFT_DUMMY:
#endif
	case IFT_GIF:
	case IFT_FAITH:
		type = IN6_IFT_P2P;
		in6_ifattach(ifp, type, 0, 1);
		break;
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_ATM:
		type = IN6_IFT_802;
		if (sdl == NULL)
			break;
		off = sdl->sdl_nlen;
		if (bcmp(&sdl->sdl_data[off], &ea, sizeof(ea)) != 0)
			in6_ifattach(ifp, type, LLADDR(sdl), 0);
		break;
	case IFT_ARCNET:
		type = IN6_IFT_ARCNET;
		if (sdl == NULL)
			break;
		off = sdl->sdl_nlen;
		if (sdl->sdl_data[off] != 0)	/* XXX ?: */
			in6_ifattach(ifp, type, LLADDR(sdl), 0);
		break;
	default:
		break;
	}

dad:
	dad_delay = 0;
	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ia = (struct in6_ifaddr *)ifa;
		if (ia->ia6_flags & IN6_IFF_TENTATIVE)
			nd6_dad_start(ifa, &dad_delay);
	}
}

/*
 * Calculate max IPv6 MTU through all the interfaces and store it
 * to in6_maxmtu.
 */
void
in6_setmaxmtu()
{
	unsigned long maxmtu = 0;
	struct ifnet *ifp;

	for (ifp = TAILQ_FIRST(&ifnet); ifp; ifp = TAILQ_NEXT(ifp, if_list))
	{
		if ((ifp->if_flags & IFF_LOOPBACK) == 0 &&
		    nd_ifinfo[ifp->if_index].linkmtu > maxmtu)
			maxmtu =  nd_ifinfo[ifp->if_index].linkmtu;
	}
	if (maxmtu)	/* update only when maxmtu is positive */
		in6_maxmtu = maxmtu;
}

/*
 * Convert sockaddr_in6 to sockaddr_in. Original sockaddr_in6 must be
 * v4 mapped addr or v4 compat addr
 */
void
in6_sin6_2_sin(struct sockaddr_in *sin, struct sockaddr_in6 *sin6)
{
	bzero(sin, sizeof(*sin));
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	sin->sin_port = sin6->sin6_port;
	sin->sin_addr.s_addr = sin6->sin6_addr.s6_addr32[3];	
}

/* Convert sockaddr_in to sockaddr_in6 in v4 mapped addr format. */
void
in6_sin_2_v4mapsin6(struct sockaddr_in *sin, struct sockaddr_in6 *sin6)
{
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = sin->sin_port;
	sin6->sin6_addr.s6_addr32[0] = 0;
	sin6->sin6_addr.s6_addr32[1] = 0;
	sin6->sin6_addr.s6_addr32[2] = IPV6_ADDR_INT32_SMP;
	sin6->sin6_addr.s6_addr32[3] = sin->sin_addr.s_addr;
}

/* Convert sockaddr_in6 into sockaddr_in. */
void
in6_sin6_2_sin_in_sock(struct sockaddr *nam)
{
	struct sockaddr_in *sin_p;
	struct sockaddr_in6 sin6;

	/*
	 * Save original sockaddr_in6 addr and convert it
	 * to sockaddr_in.
	 */
	sin6 = *(struct sockaddr_in6 *)nam;
	sin_p = (struct sockaddr_in *)nam;
	in6_sin6_2_sin(sin_p, &sin6);
}

/* Convert sockaddr_in into sockaddr_in6 in v4 mapped addr format. */
void
in6_sin_2_v4mapsin6_in_sock(struct sockaddr **nam)
{
	struct sockaddr_in *sin_p;
	struct sockaddr_in6 *sin6_p;

	MALLOC(sin6_p, struct sockaddr_in6 *, sizeof *sin6_p, M_SONAME,
	       M_WAITOK);
	sin_p = (struct sockaddr_in *)*nam;
	in6_sin_2_v4mapsin6(sin_p, sin6_p);
	FREE(*nam, M_SONAME);
	*nam = (struct sockaddr *)sin6_p;
}
