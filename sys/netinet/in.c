/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
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
 *	@(#)in.c	8.4 (Berkeley) 1/9/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llatbl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_carp.h>
#include <netinet/igmp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

static int in_aifaddr_ioctl(u_long, caddr_t, struct ifnet *, struct thread *);
static int in_difaddr_ioctl(caddr_t, struct ifnet *, struct thread *);

static void	in_socktrim(struct sockaddr_in *);
static void	in_purgemaddrs(struct ifnet *);

static VNET_DEFINE(int, nosameprefix);
#define	V_nosameprefix			VNET(nosameprefix)
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, no_same_prefix, CTLFLAG_RW,
	&VNET_NAME(nosameprefix), 0,
	"Refuse to create same prefixes on different interfaces");

VNET_DECLARE(struct inpcbinfo, ripcbinfo);
#define	V_ripcbinfo			VNET(ripcbinfo)

static struct sx in_control_sx;
SX_SYSINIT(in_control_sx, &in_control_sx, "in_control");

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).
 */
int
in_localaddr(struct in_addr in)
{
	register u_long i = ntohl(in.s_addr);
	register struct in_ifaddr *ia;

	IN_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		if ((i & ia->ia_subnetmask) == ia->ia_subnet) {
			IN_IFADDR_RUNLOCK();
			return (1);
		}
	}
	IN_IFADDR_RUNLOCK();
	return (0);
}

/*
 * Return 1 if an internet address is for the local host and configured
 * on one of its interfaces.
 */
int
in_localip(struct in_addr in)
{
	struct in_ifaddr *ia;

	IN_IFADDR_RLOCK();
	LIST_FOREACH(ia, INADDR_HASH(in.s_addr), ia_hash) {
		if (IA_SIN(ia)->sin_addr.s_addr == in.s_addr) {
			IN_IFADDR_RUNLOCK();
			return (1);
		}
	}
	IN_IFADDR_RUNLOCK();
	return (0);
}

/*
 * Return a reference to the interface address which is different to
 * the supplied one but with same IP address value.
 */
static struct in_ifaddr *
in_localip_more(struct in_ifaddr *ia)
{
	in_addr_t in = IA_SIN(ia)->sin_addr.s_addr;
	struct in_ifaddr *it;

	IN_IFADDR_RLOCK();
	LIST_FOREACH(it, INADDR_HASH(in), ia_hash) {
		if (it != ia && IA_SIN(it)->sin_addr.s_addr == in) {
			ifa_ref(&it->ia_ifa);
			IN_IFADDR_RUNLOCK();
			return (it);
		}
	}
	IN_IFADDR_RUNLOCK();

	return (NULL);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	register u_long i = ntohl(in.s_addr);
	register u_long net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i) || IN_LINKLOCAL(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
static void
in_socktrim(struct sockaddr_in *ap)
{
    register char *cplim = (char *) &ap->sin_addr;
    register char *cp = (char *) (&ap->sin_addr + 1);

    ap->sin_len = 0;
    while (--cp >= cplim)
	if (*cp) {
	    (ap)->sin_len = cp - (char *) (ap) + 1;
	    break;
	}
}

/*
 * Generic internet control operations (ioctl's).
 */
int
in_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr_in *addr = (struct sockaddr_in *)&ifr->ifr_addr;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	int error;

	if (ifp == NULL)
		return (EADDRNOTAVAIL);

	/*
	 * Filter out 4 ioctls we implement directly.  Forward the rest
	 * to specific functions and ifp->if_ioctl().
	 */
	switch (cmd) {
	case SIOCGIFADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFNETMASK:
		break;
	case SIOCDIFADDR:
		sx_xlock(&in_control_sx);
		error = in_difaddr_ioctl(data, ifp, td);
		sx_xunlock(&in_control_sx);
		return (error);
	case OSIOCAIFADDR:	/* 9.x compat */
	case SIOCAIFADDR:
		sx_xlock(&in_control_sx);
		error = in_aifaddr_ioctl(cmd, data, ifp, td);
		sx_xunlock(&in_control_sx);
		return (error);
	case SIOCSIFADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFNETMASK:
		/* We no longer support that old commands. */
		return (EINVAL);
	default:
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}

	if (addr->sin_addr.s_addr != INADDR_ANY &&
	    prison_check_ip4(td->td_ucred, &addr->sin_addr) != 0)
		return (EADDRNOTAVAIL);

	/*
	 * For SIOCGIFADDR, pick the first address.  For the rest of
	 * ioctls, try to find specified address.
	 */
	IF_ADDR_RLOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		ia = (struct in_ifaddr *)ifa;
		if (cmd == SIOCGIFADDR || addr->sin_addr.s_addr == INADDR_ANY)
			break;
		if (ia->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr)
			break;
	}

	if (ifa == NULL) {
		IF_ADDR_RUNLOCK(ifp);
		return (EADDRNOTAVAIL);
	}

	error = 0;
	switch (cmd) {
	case SIOCGIFADDR:
		*addr = ia->ia_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EINVAL;
			break;
		}
		*addr = ia->ia_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			break;
		}
		*addr = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK:
		*addr = ia->ia_sockmask;
		break;
	}

	IF_ADDR_RUNLOCK(ifp);

	return (error);
}

static int
in_aifaddr_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp, struct thread *td)
{
	const struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	const struct sockaddr_in *addr = &ifra->ifra_addr;
	const struct sockaddr_in *broadaddr = &ifra->ifra_broadaddr;
	const struct sockaddr_in *mask = &ifra->ifra_mask;
	const struct sockaddr_in *dstaddr = &ifra->ifra_dstaddr;
	const int vhid = (cmd == SIOCAIFADDR) ? ifra->ifra_vhid : 0;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	bool iaIsFirst;
	int error = 0;

	error = priv_check(td, PRIV_NET_ADDIFADDR);
	if (error)
		return (error);

	/*
	 * ifra_addr must be present and be of INET family.
	 * ifra_broadaddr/ifra_dstaddr and ifra_mask are optional.
	 */
	if (addr->sin_len != sizeof(struct sockaddr_in) ||
	    addr->sin_family != AF_INET)
		return (EINVAL);
	if (broadaddr->sin_len != 0 &&
	    (broadaddr->sin_len != sizeof(struct sockaddr_in) ||
	    broadaddr->sin_family != AF_INET))
		return (EINVAL);
	if (mask->sin_len != 0 &&
	    (mask->sin_len != sizeof(struct sockaddr_in) ||
	    mask->sin_family != AF_INET))
		return (EINVAL);
	if ((ifp->if_flags & IFF_POINTOPOINT) &&
	    (dstaddr->sin_len != sizeof(struct sockaddr_in) ||
	     dstaddr->sin_addr.s_addr == INADDR_ANY))
		return (EDESTADDRREQ);
	if (vhid > 0 && carp_attach_p == NULL)
		return (EPROTONOSUPPORT);

	/*
	 * See whether address already exist.
	 */
	iaIsFirst = true;
	ia = NULL;
	IF_ADDR_RLOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *it;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		it = (struct in_ifaddr *)ifa;
		iaIsFirst = false;
		if (it->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		    prison_check_ip4(td->td_ucred, &addr->sin_addr) == 0)
			ia = it;
	}
	IF_ADDR_RUNLOCK(ifp);

	if (ia != NULL)
		(void )in_difaddr_ioctl(data, ifp, td);

	ifa = ifa_alloc(sizeof(struct in_ifaddr), M_WAITOK);
	ia = (struct in_ifaddr *)ifa;
	ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
	ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
	ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;

	ia->ia_ifp = ifp;
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	ia->ia_addr = *addr;
	if (mask->sin_len != 0) {
		ia->ia_sockmask = *mask;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr);
	} else {
		in_addr_t i = ntohl(addr->sin_addr.s_addr);

		/*
	 	 * Be compatible with network classes, if netmask isn't
		 * supplied, guess it based on classes.
	 	 */
		if (IN_CLASSA(i))
			ia->ia_subnetmask = IN_CLASSA_NET;
		else if (IN_CLASSB(i))
			ia->ia_subnetmask = IN_CLASSB_NET;
		else
			ia->ia_subnetmask = IN_CLASSC_NET;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	}
	ia->ia_subnet = ntohl(addr->sin_addr.s_addr) & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);

	if (ifp->if_flags & IFF_BROADCAST) {
		if (broadaddr->sin_len != 0) {
			ia->ia_broadaddr = *broadaddr;
		} else if (ia->ia_subnetmask == IN_RFC3021_MASK) {
			ia->ia_broadaddr.sin_addr.s_addr = INADDR_BROADCAST;
			ia->ia_broadaddr.sin_len = sizeof(struct sockaddr_in);
			ia->ia_broadaddr.sin_family = AF_INET;
		} else {
			ia->ia_broadaddr.sin_addr.s_addr =
			    htonl(ia->ia_subnet | ~ia->ia_subnetmask);
			ia->ia_broadaddr.sin_len = sizeof(struct sockaddr_in);
			ia->ia_broadaddr.sin_family = AF_INET;
		}
	}

	if (ifp->if_flags & IFF_POINTOPOINT)
		ia->ia_dstaddr = *dstaddr;

	/* XXXGL: rtinit() needs this strange assignment. */
	if (ifp->if_flags & IFF_LOOPBACK)
                ia->ia_dstaddr = ia->ia_addr;

	ifa_ref(ifa);			/* if_addrhead */
	IF_ADDR_WLOCK(ifp);
	TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);
	IF_ADDR_WUNLOCK(ifp);

	ifa_ref(ifa);			/* in_ifaddrhead */
	IN_IFADDR_WLOCK();
	TAILQ_INSERT_TAIL(&V_in_ifaddrhead, ia, ia_link);
	LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr), ia, ia_hash);
	IN_IFADDR_WUNLOCK();

	if (vhid != 0)
		error = (*carp_attach_p)(&ia->ia_ifa, vhid);
	if (error)
		goto fail1;

	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl != NULL)
		error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia);
	if (error)
		goto fail2;

	/*
	 * Add route for the network.
	 */
	if (vhid == 0) {
		int flags = RTF_UP;

		if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
			flags |= RTF_HOST;

		error = in_addprefix(ia, flags);
		if (error)
			goto fail2;
	}

	/*
	 * Add a loopback route to self.
	 */
	if (vhid == 0 && (ifp->if_flags & IFF_LOOPBACK) == 0 &&
	    ia->ia_addr.sin_addr.s_addr != INADDR_ANY &&
	    !((ifp->if_flags & IFF_POINTOPOINT) &&
	     ia->ia_dstaddr.sin_addr.s_addr == ia->ia_addr.sin_addr.s_addr)) {
		struct in_ifaddr *eia;

		eia = in_localip_more(ia);

		if (eia == NULL) {
			error = ifa_add_loopback_route((struct ifaddr *)ia,
			    (struct sockaddr *)&ia->ia_addr);
			if (error)
				goto fail3;
		} else
			ifa_free(&eia->ia_ifa);
	}

	if (iaIsFirst && (ifp->if_flags & IFF_MULTICAST)) {
		struct in_addr allhosts_addr;
		struct in_ifinfo *ii;

		ii = ((struct in_ifinfo *)ifp->if_afdata[AF_INET]);
		allhosts_addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);

		error = in_joingroup(ifp, &allhosts_addr, NULL,
			&ii->ii_allhosts);
	}

	EVENTHANDLER_INVOKE(ifaddr_event, ifp);

	return (error);

fail3:
	if (vhid == 0)
		(void )in_scrubprefix(ia, LLE_STATIC);

fail2:
	if (ia->ia_ifa.ifa_carp)
		(*carp_detach_p)(&ia->ia_ifa);

fail1:
	IF_ADDR_WLOCK(ifp);
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	IF_ADDR_WUNLOCK(ifp);
	ifa_free(&ia->ia_ifa);

	IN_IFADDR_WLOCK();
	TAILQ_REMOVE(&V_in_ifaddrhead, ia, ia_link);
	LIST_REMOVE(ia, ia_hash);
	IN_IFADDR_WUNLOCK();
	ifa_free(&ia->ia_ifa);

	return (error);
}

static int
in_difaddr_ioctl(caddr_t data, struct ifnet *ifp, struct thread *td)
{
	const struct ifreq *ifr = (struct ifreq *)data;
	const struct sockaddr_in *addr = (const struct sockaddr_in *)
	    &ifr->ifr_addr;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	bool deleteAny, iaIsLast;
	int error;

	if (td != NULL) {
		error = priv_check(td, PRIV_NET_DELIFADDR);
		if (error)
			return (error);
	}

	if (addr->sin_len != sizeof(struct sockaddr_in) ||
	    addr->sin_family != AF_INET)
		deleteAny = true;
	else
		deleteAny = false;

	iaIsLast = true;
	ia = NULL;
	IF_ADDR_WLOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *it;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		it = (struct in_ifaddr *)ifa;
		if (deleteAny && ia == NULL && (td == NULL ||
		    prison_check_ip4(td->td_ucred, &it->ia_addr.sin_addr) == 0))
			ia = it;

		if (it->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		    (td == NULL || prison_check_ip4(td->td_ucred,
		    &addr->sin_addr) == 0))
			ia = it;

		if (it != ia)
			iaIsLast = false;
	}

	if (ia == NULL) {
		IF_ADDR_WUNLOCK(ifp);
		return (EADDRNOTAVAIL);
	}

	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	IF_ADDR_WUNLOCK(ifp);
	ifa_free(&ia->ia_ifa);		/* if_addrhead */

	IN_IFADDR_WLOCK();
	TAILQ_REMOVE(&V_in_ifaddrhead, ia, ia_link);
	LIST_REMOVE(ia, ia_hash);
	IN_IFADDR_WUNLOCK();
	ifa_free(&ia->ia_ifa);		/* in_ifaddrhead */

	/*
	 * in_scrubprefix() kills the interface route.
	 */
	in_scrubprefix(ia, LLE_STATIC);

	/*
	 * in_ifadown gets rid of all the rest of
	 * the routes.  This is not quite the right
	 * thing to do, but at least if we are running
	 * a routing process they will come back.
	 */
	in_ifadown(&ia->ia_ifa, 1);

	if (ia->ia_ifa.ifa_carp)
		(*carp_detach_p)(&ia->ia_ifa);

	/*
	 * If this is the last IPv4 address configured on this
	 * interface, leave the all-hosts group.
	 * No state-change report need be transmitted.
	 */
	if (iaIsLast && (ifp->if_flags & IFF_MULTICAST)) {
		struct in_ifinfo *ii;

		ii = ((struct in_ifinfo *)ifp->if_afdata[AF_INET]);
		IN_MULTI_LOCK();
		if (ii->ii_allhosts) {
			(void)in_leavegroup_locked(ii->ii_allhosts, NULL);
			ii->ii_allhosts = NULL;
		}
		IN_MULTI_UNLOCK();
	}

	EVENTHANDLER_INVOKE(ifaddr_event, ifp);

	return (0);
}

#define rtinitflags(x) \
	((((x)->ia_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) != 0) \
	    ? RTF_HOST : 0)

/*
 * Generate a routing message when inserting or deleting
 * an interface address alias.
 */
static void in_addralias_rtmsg(int cmd, struct in_addr *prefix,
    struct in_ifaddr *target)
{
	struct route pfx_ro;
	struct sockaddr_in *pfx_addr;
	struct rtentry msg_rt;

	/* QL: XXX
	 * This is a bit questionable because there is no
	 * additional route entry added/deleted for an address
	 * alias. Therefore this route report is inaccurate.
	 */
	bzero(&pfx_ro, sizeof(pfx_ro));
	pfx_addr = (struct sockaddr_in *)(&pfx_ro.ro_dst);
	pfx_addr->sin_len = sizeof(*pfx_addr);
	pfx_addr->sin_family = AF_INET;
	pfx_addr->sin_addr = *prefix;
	rtalloc_ign_fib(&pfx_ro, 0, 0);
	if (pfx_ro.ro_rt != NULL) {
		msg_rt = *pfx_ro.ro_rt;

		/* QL: XXX
		 * Point the gateway to the new interface
		 * address as if a new prefix route entry has
		 * been added through the new address alias.
		 * All other parts of the rtentry is accurate,
		 * e.g., rt_key, rt_mask, rt_ifp etc.
		 */
		msg_rt.rt_gateway = (struct sockaddr *)&target->ia_addr;
		rt_newaddrmsg(cmd, (struct ifaddr *)target, 0, &msg_rt);
		RTFREE(pfx_ro.ro_rt);
	}
	return;
}

/*
 * Check if we have a route for the given prefix already or add one accordingly.
 */
int
in_addprefix(struct in_ifaddr *target, int flags)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error;

	if ((flags & RTF_HOST) != 0) {
		prefix = target->ia_dstaddr.sin_addr;
		mask.s_addr = 0;
	} else {
		prefix = target->ia_addr.sin_addr;
		mask = target->ia_sockmask.sin_addr;
		prefix.s_addr &= mask.s_addr;
	}

	IN_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		if (rtinitflags(ia)) {
			p = ia->ia_dstaddr.sin_addr;

			if (prefix.s_addr != p.s_addr)
				continue;
		} else {
			p = ia->ia_addr.sin_addr;
			m = ia->ia_sockmask.sin_addr;
			p.s_addr &= m.s_addr;

			if (prefix.s_addr != p.s_addr ||
			    mask.s_addr != m.s_addr)
				continue;
		}

		/*
		 * If we got a matching prefix route inserted by other
		 * interface address, we are done here.
		 */
		if (ia->ia_flags & IFA_ROUTE) {
#ifdef RADIX_MPATH
			if (ia->ia_addr.sin_addr.s_addr ==
			    target->ia_addr.sin_addr.s_addr) {
				IN_IFADDR_RUNLOCK();
				return (EEXIST);
			} else
				break;
#endif
			if (V_nosameprefix) {
				IN_IFADDR_RUNLOCK();
				return (EEXIST);
			} else {
				in_addralias_rtmsg(RTM_ADD, &prefix, target);
				IN_IFADDR_RUNLOCK();
				return (0);
			}
		}
	}
	IN_IFADDR_RUNLOCK();

	/*
	 * No-one seem to have this prefix route, so we try to insert it.
	 */
	error = rtinit(&target->ia_ifa, (int)RTM_ADD, flags);
	if (!error)
		target->ia_flags |= IFA_ROUTE;
	return (error);
}

/*
 * If there is no other address in the system that can serve a route to the
 * same prefix, remove the route.  Hand over the route to the new address
 * otherwise.
 */
int
in_scrubprefix(struct in_ifaddr *target, u_int flags)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error = 0;
	struct sockaddr_in prefix0, mask0;

	/*
	 * Remove the loopback route to the interface address.
	 */
	if ((target->ia_addr.sin_addr.s_addr != INADDR_ANY) &&
	    !(target->ia_ifp->if_flags & IFF_LOOPBACK) &&
	    (flags & LLE_STATIC)) {
		struct in_ifaddr *eia;

		eia = in_localip_more(target);

		if (eia != NULL) {
			error = ifa_switch_loopback_route((struct ifaddr *)eia,
			    (struct sockaddr *)&target->ia_addr);
			ifa_free(&eia->ia_ifa);
		} else {
			error = ifa_del_loopback_route((struct ifaddr *)target,
			    (struct sockaddr *)&target->ia_addr);
		}

		if (!(target->ia_ifp->if_flags & IFF_NOARP))
			/* remove arp cache */
			arp_ifscrub(target->ia_ifp,
			    IA_SIN(target)->sin_addr.s_addr);
	}

	if (rtinitflags(target)) {
		prefix = target->ia_dstaddr.sin_addr;
		mask.s_addr = 0;
	} else {
		prefix = target->ia_addr.sin_addr;
		mask = target->ia_sockmask.sin_addr;
		prefix.s_addr &= mask.s_addr;
	}

	if ((target->ia_flags & IFA_ROUTE) == 0) {
		in_addralias_rtmsg(RTM_DELETE, &prefix, target);
		return (0);
	}

	IN_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		if (rtinitflags(ia)) {
			p = ia->ia_dstaddr.sin_addr;

			if (prefix.s_addr != p.s_addr)
				continue;
		} else {
			p = ia->ia_addr.sin_addr;
			m = ia->ia_sockmask.sin_addr;
			p.s_addr &= m.s_addr;

			if (prefix.s_addr != p.s_addr ||
			    mask.s_addr != m.s_addr)
				continue;
		}

		if ((ia->ia_ifp->if_flags & IFF_UP) == 0)
			continue;

		/*
		 * If we got a matching prefix address, move IFA_ROUTE and
		 * the route itself to it.  Make sure that routing daemons
		 * get a heads-up.
		 */
		if ((ia->ia_flags & IFA_ROUTE) == 0) {
			ifa_ref(&ia->ia_ifa);
			IN_IFADDR_RUNLOCK();
			error = rtinit(&(target->ia_ifa), (int)RTM_DELETE,
			    rtinitflags(target));
			if (error == 0)
				target->ia_flags &= ~IFA_ROUTE;
			else
				log(LOG_INFO, "in_scrubprefix: err=%d, old prefix delete failed\n",
					error);
			error = rtinit(&ia->ia_ifa, (int)RTM_ADD,
			    rtinitflags(ia) | RTF_UP);
			if (error == 0)
				ia->ia_flags |= IFA_ROUTE;
			else
				log(LOG_INFO, "in_scrubprefix: err=%d, new prefix add failed\n",
					error);
			ifa_free(&ia->ia_ifa);
			return (error);
		}
	}
	IN_IFADDR_RUNLOCK();

	/*
	 * remove all L2 entries on the given prefix
	 */
	bzero(&prefix0, sizeof(prefix0));
	prefix0.sin_len = sizeof(prefix0);
	prefix0.sin_family = AF_INET;
	prefix0.sin_addr.s_addr = target->ia_subnet;
	bzero(&mask0, sizeof(mask0));
	mask0.sin_len = sizeof(mask0);
	mask0.sin_family = AF_INET;
	mask0.sin_addr.s_addr = target->ia_subnetmask;
	lltable_prefix_free(AF_INET, (struct sockaddr *)&prefix0,
	    (struct sockaddr *)&mask0, flags);

	/*
	 * As no-one seem to have this prefix, we can remove the route.
	 */
	error = rtinit(&(target->ia_ifa), (int)RTM_DELETE, rtinitflags(target));
	if (error == 0)
		target->ia_flags &= ~IFA_ROUTE;
	else
		log(LOG_INFO, "in_scrubprefix: err=%d, prefix delete failed\n", error);
	return (error);
}

#undef rtinitflags

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	register struct ifaddr *ifa;
	u_long t;

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return (1);
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return (0);
	t = ntohl(in.s_addr);
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
#define ia ((struct in_ifaddr *)ifa)
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
		     /*
		      * Check for old-style (host 0) broadcast, but
		      * taking into account that RFC 3021 obsoletes it.
		      */
		    (ia->ia_subnetmask != IN_RFC3021_MASK &&
		    t == ia->ia_subnet)) &&
		     /*
		      * Check for an all one subnetmask. These
		      * only exist when an interface gets a secondary
		      * address.
		      */
		    ia->ia_subnetmask != (u_long)0xffffffff)
			    return (1);
	return (0);
#undef ia
}

/*
 * On interface removal, clean up IPv4 data structures hung off of the ifnet.
 */
void
in_ifdetach(struct ifnet *ifp)
{

	in_pcbpurgeif0(&V_ripcbinfo, ifp);
	in_pcbpurgeif0(&V_udbinfo, ifp);
	in_purgemaddrs(ifp);
}

/*
 * Delete all IPv4 multicast address records, and associated link-layer
 * multicast address records, associated with ifp.
 * XXX It looks like domifdetach runs AFTER the link layer cleanup.
 * XXX This should not race with ifma_protospec being set during
 * a new allocation, if it does, we have bigger problems.
 */
static void
in_purgemaddrs(struct ifnet *ifp)
{
	LIST_HEAD(,in_multi) purgeinms;
	struct in_multi		*inm, *tinm;
	struct ifmultiaddr	*ifma;

	LIST_INIT(&purgeinms);
	IN_MULTI_LOCK();

	/*
	 * Extract list of in_multi associated with the detaching ifp
	 * which the PF_INET layer is about to release.
	 * We need to do this as IF_ADDR_LOCK() may be re-acquired
	 * by code further down.
	 */
	IF_ADDR_RLOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_INET ||
		    ifma->ifma_protospec == NULL)
			continue;
#if 0
		KASSERT(ifma->ifma_protospec != NULL,
		    ("%s: ifma_protospec is NULL", __func__));
#endif
		inm = (struct in_multi *)ifma->ifma_protospec;
		LIST_INSERT_HEAD(&purgeinms, inm, inm_link);
	}
	IF_ADDR_RUNLOCK(ifp);

	LIST_FOREACH_SAFE(inm, &purgeinms, inm_link, tinm) {
		LIST_REMOVE(inm, inm_link);
		inm_release_locked(inm);
	}
	igmp_ifdetach(ifp);

	IN_MULTI_UNLOCK();
}

struct in_llentry {
	struct llentry		base;
	struct sockaddr_in	l3_addr4;
};

/*
 * Deletes an address from the address table.
 * This function is called by the timer functions
 * such as arptimer() and nd6_llinfo_timer(), and
 * the caller does the locking.
 */
static void
in_lltable_free(struct lltable *llt, struct llentry *lle)
{
	LLE_WUNLOCK(lle);
	LLE_LOCK_DESTROY(lle);
	free(lle, M_LLTABLE);
}

static struct llentry *
in_lltable_new(const struct sockaddr *l3addr, u_int flags)
{
	struct in_llentry *lle;

	lle = malloc(sizeof(struct in_llentry), M_LLTABLE, M_NOWAIT | M_ZERO);
	if (lle == NULL)		/* NB: caller generates msg */
		return NULL;

	/*
	 * For IPv4 this will trigger "arpresolve" to generate
	 * an ARP request.
	 */
	lle->base.la_expire = time_uptime; /* mark expired */
	lle->l3_addr4 = *(const struct sockaddr_in *)l3addr;
	lle->base.lle_refcnt = 1;
	lle->base.lle_free = in_lltable_free;
	LLE_LOCK_INIT(&lle->base);
	callout_init_rw(&lle->base.la_timer, &lle->base.lle_lock,
	    CALLOUT_RETURNUNLOCKED);

	return (&lle->base);
}

#define IN_ARE_MASKED_ADDR_EQUAL(d, a, m)	(			\
	    (((ntohl((d)->sin_addr.s_addr) ^ (a)->sin_addr.s_addr) & (m)->sin_addr.s_addr)) == 0 )

static void
in_lltable_prefix_free(struct lltable *llt, const struct sockaddr *prefix,
    const struct sockaddr *mask, u_int flags)
{
	const struct sockaddr_in *pfx = (const struct sockaddr_in *)prefix;
	const struct sockaddr_in *msk = (const struct sockaddr_in *)mask;
	struct llentry *lle, *next;
	int i;
	size_t pkts_dropped;

	IF_AFDATA_WLOCK(llt->llt_ifp);
	for (i = 0; i < LLTBL_HASHTBL_SIZE; i++) {
		LIST_FOREACH_SAFE(lle, &llt->lle_head[i], lle_next, next) {
			/*
			 * (flags & LLE_STATIC) means deleting all entries
			 * including static ARP entries.
			 */
			if (IN_ARE_MASKED_ADDR_EQUAL(satosin(L3_ADDR(lle)),
			    pfx, msk) && ((flags & LLE_STATIC) ||
			    !(lle->la_flags & LLE_STATIC))) {
				LLE_WLOCK(lle);
				if (callout_stop(&lle->la_timer))
					LLE_REMREF(lle);
				pkts_dropped = llentry_free(lle);
				ARPSTAT_ADD(dropped, pkts_dropped);
			}
		}
	}
	IF_AFDATA_WUNLOCK(llt->llt_ifp);
}


static int
in_lltable_rtcheck(struct ifnet *ifp, u_int flags, const struct sockaddr *l3addr)
{
	struct rtentry *rt;

	KASSERT(l3addr->sa_family == AF_INET,
	    ("sin_family %d", l3addr->sa_family));

	/* XXX rtalloc1 should take a const param */
	rt = rtalloc1(__DECONST(struct sockaddr *, l3addr), 0, 0);

	if (rt == NULL)
		return (EINVAL);

	/*
	 * If the gateway for an existing host route matches the target L3
	 * address, which is a special route inserted by some implementation
	 * such as MANET, and the interface is of the correct type, then
	 * allow for ARP to proceed.
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (!(rt->rt_flags & RTF_HOST) || !rt->rt_ifp ||
		    rt->rt_ifp->if_type != IFT_ETHER ||
		    (rt->rt_ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) != 0 ||
		    memcmp(rt->rt_gateway->sa_data, l3addr->sa_data,
		    sizeof(in_addr_t)) != 0) {
			RTFREE_LOCKED(rt);
			return (EINVAL);
		}
	}

	/*
	 * Make sure that at least the destination address is covered
	 * by the route. This is for handling the case where 2 or more
	 * interfaces have the same prefix. An incoming packet arrives
	 * on one interface and the corresponding outgoing packet leaves
	 * another interface.
	 */
	if (!(rt->rt_flags & RTF_HOST) && rt->rt_ifp != ifp) {
		const char *sa, *mask, *addr, *lim;
		int len;

		mask = (const char *)rt_mask(rt);
		/*
		 * Just being extra cautious to avoid some custom
		 * code getting into trouble.
		 */
		if (mask == NULL) {
			RTFREE_LOCKED(rt);
			return (EINVAL);
		}

		sa = (const char *)rt_key(rt);
		addr = (const char *)l3addr;
		len = ((const struct sockaddr_in *)l3addr)->sin_len;
		lim = addr + len;

		for ( ; addr < lim; sa++, mask++, addr++) {
			if ((*sa ^ *addr) & *mask) {
#ifdef DIAGNOSTIC
				log(LOG_INFO, "IPv4 address: \"%s\" is not on the network\n",
				    inet_ntoa(((const struct sockaddr_in *)l3addr)->sin_addr));
#endif
				RTFREE_LOCKED(rt);
				return (EINVAL);
			}
		}
	}

	RTFREE_LOCKED(rt);
	return (0);
}

/*
 * Return NULL if not found or marked for deletion.
 * If found return lle read locked.
 */
static struct llentry *
in_lltable_lookup(struct lltable *llt, u_int flags, const struct sockaddr *l3addr)
{
	const struct sockaddr_in *sin = (const struct sockaddr_in *)l3addr;
	struct ifnet *ifp = llt->llt_ifp;
	struct llentry *lle;
	struct llentries *lleh;
	u_int hashkey;

	IF_AFDATA_LOCK_ASSERT(ifp);
	KASSERT(l3addr->sa_family == AF_INET,
	    ("sin_family %d", l3addr->sa_family));

	hashkey = sin->sin_addr.s_addr;
	lleh = &llt->lle_head[LLATBL_HASH(hashkey, LLTBL_HASHMASK)];
	LIST_FOREACH(lle, lleh, lle_next) {
		struct sockaddr_in *sa2 = satosin(L3_ADDR(lle));
		if (lle->la_flags & LLE_DELETED)
			continue;
		if (sa2->sin_addr.s_addr == sin->sin_addr.s_addr)
			break;
	}
	if (lle == NULL) {
#ifdef DIAGNOSTIC
		if (flags & LLE_DELETE)
			log(LOG_INFO, "interface address is missing from cache = %p  in delete\n", lle);
#endif
		if (!(flags & LLE_CREATE))
			return (NULL);
		/*
		 * A route that covers the given address must have
		 * been installed 1st because we are doing a resolution,
		 * verify this.
		 */
		if (!(flags & LLE_IFADDR) &&
		    in_lltable_rtcheck(ifp, flags, l3addr) != 0)
			goto done;

		lle = in_lltable_new(l3addr, flags);
		if (lle == NULL) {
			log(LOG_INFO, "lla_lookup: new lle malloc failed\n");
			goto done;
		}
		lle->la_flags = flags & ~LLE_CREATE;
		if ((flags & (LLE_CREATE | LLE_IFADDR)) == (LLE_CREATE | LLE_IFADDR)) {
			bcopy(IF_LLADDR(ifp), &lle->ll_addr, ifp->if_addrlen);
			lle->la_flags |= (LLE_VALID | LLE_STATIC);
		}

		lle->lle_tbl  = llt;
		lle->lle_head = lleh;
		lle->la_flags |= LLE_LINKED;
		LIST_INSERT_HEAD(lleh, lle, lle_next);
	} else if (flags & LLE_DELETE) {
		if (!(lle->la_flags & LLE_IFADDR) || (flags & LLE_IFADDR)) {
			LLE_WLOCK(lle);
			lle->la_flags |= LLE_DELETED;
			EVENTHANDLER_INVOKE(lle_event, lle, LLENTRY_DELETED);
#ifdef DIAGNOSTIC
			log(LOG_INFO, "ifaddr cache = %p is deleted\n", lle);
#endif
			if ((lle->la_flags &
			    (LLE_STATIC | LLE_IFADDR)) == LLE_STATIC)
				llentry_free(lle);
			else
				LLE_WUNLOCK(lle);
		}
		lle = (void *)-1;

	}
	if (LLE_IS_VALID(lle)) {
		if (flags & LLE_EXCLUSIVE)
			LLE_WLOCK(lle);
		else
			LLE_RLOCK(lle);
	}
done:
	return (lle);
}

static int
in_lltable_dump(struct lltable *llt, struct sysctl_req *wr)
{
#define	SIN(lle)	((struct sockaddr_in *) L3_ADDR(lle))
	struct ifnet *ifp = llt->llt_ifp;
	struct llentry *lle;
	/* XXX stack use */
	struct {
		struct rt_msghdr	rtm;
		struct sockaddr_in	sin;
		struct sockaddr_dl	sdl;
	} arpc;
	int error, i;

	LLTABLE_LOCK_ASSERT();

	error = 0;
	for (i = 0; i < LLTBL_HASHTBL_SIZE; i++) {
		LIST_FOREACH(lle, &llt->lle_head[i], lle_next) {
			struct sockaddr_dl *sdl;

			/* skip deleted entries */
			if ((lle->la_flags & LLE_DELETED) == LLE_DELETED)
				continue;
			/* Skip if jailed and not a valid IP of the prison. */
			if (prison_if(wr->td->td_ucred, L3_ADDR(lle)) != 0)
				continue;
			/*
			 * produce a msg made of:
			 *  struct rt_msghdr;
			 *  struct sockaddr_in; (IPv4)
			 *  struct sockaddr_dl;
			 */
			bzero(&arpc, sizeof(arpc));
			arpc.rtm.rtm_msglen = sizeof(arpc);
			arpc.rtm.rtm_version = RTM_VERSION;
			arpc.rtm.rtm_type = RTM_GET;
			arpc.rtm.rtm_flags = RTF_UP;
			arpc.rtm.rtm_addrs = RTA_DST | RTA_GATEWAY;
			arpc.sin.sin_family = AF_INET;
			arpc.sin.sin_len = sizeof(arpc.sin);
			arpc.sin.sin_addr.s_addr = SIN(lle)->sin_addr.s_addr;

			/* publish */
			if (lle->la_flags & LLE_PUB)
				arpc.rtm.rtm_flags |= RTF_ANNOUNCE;

			sdl = &arpc.sdl;
			sdl->sdl_family = AF_LINK;
			sdl->sdl_len = sizeof(*sdl);
			sdl->sdl_index = ifp->if_index;
			sdl->sdl_type = ifp->if_type;
			if ((lle->la_flags & LLE_VALID) == LLE_VALID) {
				sdl->sdl_alen = ifp->if_addrlen;
				bcopy(&lle->ll_addr, LLADDR(sdl), ifp->if_addrlen);
			} else {
				sdl->sdl_alen = 0;
				bzero(LLADDR(sdl), ifp->if_addrlen);
			}

			arpc.rtm.rtm_rmx.rmx_expire =
			    lle->la_flags & LLE_STATIC ? 0 : lle->la_expire;
			arpc.rtm.rtm_flags |= (RTF_HOST | RTF_LLDATA);
			if (lle->la_flags & LLE_STATIC)
				arpc.rtm.rtm_flags |= RTF_STATIC;
			arpc.rtm.rtm_index = ifp->if_index;
			error = SYSCTL_OUT(wr, &arpc, sizeof(arpc));
			if (error)
				break;
		}
	}
	return error;
#undef SIN
}

void *
in_domifattach(struct ifnet *ifp)
{
	struct in_ifinfo *ii;
	struct lltable *llt;

	ii = malloc(sizeof(struct in_ifinfo), M_IFADDR, M_WAITOK|M_ZERO);

	llt = lltable_init(ifp, AF_INET);
	if (llt != NULL) {
		llt->llt_prefix_free = in_lltable_prefix_free;
		llt->llt_lookup = in_lltable_lookup;
		llt->llt_dump = in_lltable_dump;
	}
	ii->ii_llt = llt;

	ii->ii_igmp = igmp_domifattach(ifp);

	return ii;
}

void
in_domifdetach(struct ifnet *ifp, void *aux)
{
	struct in_ifinfo *ii = (struct in_ifinfo *)aux;

	igmp_domifdetach(ifp);
	lltable_free(ii->ii_llt);
	free(ii, M_IFADDR);
}
