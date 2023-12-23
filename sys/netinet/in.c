/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_inet.h"

#define IN_HISTORICAL_NETS		/* include class masks */

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llatbl.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/vnet.h>

#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_carp.h>
#include <netinet/igmp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

static int in_aifaddr_ioctl(u_long, caddr_t, struct ifnet *, struct ucred *);
static int in_difaddr_ioctl(u_long, caddr_t, struct ifnet *, struct ucred *);
static int in_gifaddr_ioctl(u_long, caddr_t, struct ifnet *, struct ucred *);

static void	in_socktrim(struct sockaddr_in *);
static void	in_purgemaddrs(struct ifnet *);

static bool	ia_need_loopback_route(const struct in_ifaddr *);

VNET_DEFINE_STATIC(int, nosameprefix);
#define	V_nosameprefix			VNET(nosameprefix)
SYSCTL_INT(_net_inet_ip, OID_AUTO, no_same_prefix, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(nosameprefix), 0,
	"Refuse to create same prefixes on different interfaces");

VNET_DEFINE_STATIC(bool, broadcast_lowest);
#define	V_broadcast_lowest		VNET(broadcast_lowest)
SYSCTL_BOOL(_net_inet_ip, OID_AUTO, broadcast_lowest, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(broadcast_lowest), 0,
	"Treat lowest address on a subnet (host 0) as broadcast");

VNET_DEFINE(bool, ip_allow_net240) = false;
#define	V_ip_allow_net240		VNET(ip_allow_net240)
SYSCTL_BOOL(_net_inet_ip, OID_AUTO, allow_net240,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip_allow_net240), 0,
	"Allow use of Experimental addresses, aka Class E (240/4)");
/* see https://datatracker.ietf.org/doc/draft-schoen-intarea-unicast-240 */

VNET_DEFINE(bool, ip_allow_net0) = false;
SYSCTL_BOOL(_net_inet_ip, OID_AUTO, allow_net0,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip_allow_net0), 0,
	"Allow use of addresses in network 0/8");
/* see https://datatracker.ietf.org/doc/draft-schoen-intarea-unicast-0 */

VNET_DEFINE(uint32_t, in_loopback_mask) = IN_LOOPBACK_MASK_DFLT;
#define	V_in_loopback_mask	VNET(in_loopback_mask)
static int sysctl_loopback_prefixlen(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_net_inet_ip, OID_AUTO, loopback_prefixlen,
	CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW,
	NULL, 0, sysctl_loopback_prefixlen, "I",
	"Prefix length of address space reserved for loopback");
/* see https://datatracker.ietf.org/doc/draft-schoen-intarea-unicast-127 */

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
	u_long i = ntohl(in.s_addr);
	struct in_ifaddr *ia;

	NET_EPOCH_ASSERT();

	CK_STAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		if ((i & ia->ia_subnetmask) == ia->ia_subnet)
			return (1);
	}

	return (0);
}

/*
 * Return 1 if an internet address is for the local host and configured
 * on one of its interfaces.
 */
bool
in_localip(struct in_addr in)
{
	struct in_ifaddr *ia;

	NET_EPOCH_ASSERT();

	CK_LIST_FOREACH(ia, INADDR_HASH(in.s_addr), ia_hash)
		if (IA_SIN(ia)->sin_addr.s_addr == in.s_addr)
			return (true);

	return (false);
}

/*
 * Like in_localip(), but FIB-aware.
 */
bool
in_localip_fib(struct in_addr in, uint16_t fib)
{
	struct in_ifaddr *ia;

	NET_EPOCH_ASSERT();

	CK_LIST_FOREACH(ia, INADDR_HASH(in.s_addr), ia_hash)
		if (IA_SIN(ia)->sin_addr.s_addr == in.s_addr &&
		    ia->ia_ifa.ifa_ifp->if_fib == fib)
			return (true);

	return (false);
}

/*
 * Return 1 if an internet address is configured on an interface.
 */
int
in_ifhasaddr(struct ifnet *ifp, struct in_addr in)
{
	struct ifaddr *ifa;
	struct in_ifaddr *ia;

	NET_EPOCH_ASSERT();

	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		ia = (struct in_ifaddr *)ifa;
		if (ia->ia_addr.sin_addr.s_addr == in.s_addr)
			return (1);
	}

	return (0);
}

/*
 * Return a reference to the interface address which is different to
 * the supplied one but with same IP address value.
 */
static struct in_ifaddr *
in_localip_more(struct in_ifaddr *original_ia)
{
	struct epoch_tracker et;
	in_addr_t original_addr = IA_SIN(original_ia)->sin_addr.s_addr;
	uint32_t original_fib = original_ia->ia_ifa.ifa_ifp->if_fib;
	struct in_ifaddr *ia;

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(ia, INADDR_HASH(original_addr), ia_hash) {
		in_addr_t addr = IA_SIN(ia)->sin_addr.s_addr;
		uint32_t fib = ia->ia_ifa.ifa_ifp->if_fib;
		if (!V_rt_add_addr_allfibs && (original_fib != fib))
			continue;
		if ((original_ia != ia) && (original_addr == addr)) {
			ifa_ref(&ia->ia_ifa);
			NET_EPOCH_EXIT(et);
			return (ia);
		}
	}
	NET_EPOCH_EXIT(et);

	return (NULL);
}

/*
 * Tries to find first IPv4 address in the provided fib.
 * Prefers non-loopback addresses and return loopback IFF
 * @loopback_ok is set.
 *
 * Returns ifa or NULL.
 */
struct in_ifaddr *
in_findlocal(uint32_t fibnum, bool loopback_ok)
{
	struct in_ifaddr *ia = NULL, *ia_lo = NULL;

	NET_EPOCH_ASSERT();

	CK_STAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		uint32_t ia_fib = ia->ia_ifa.ifa_ifp->if_fib;
		if (!V_rt_add_addr_allfibs && (fibnum != ia_fib))
			continue;

		if (!IN_LOOPBACK(ntohl(IA_SIN(ia)->sin_addr.s_addr)))
			break;
		if (loopback_ok)
			ia_lo = ia;
	}

	if (ia == NULL)
		ia = ia_lo;

	return (ia);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	u_long i = ntohl(in.s_addr);

	if (IN_MULTICAST(i) || IN_LINKLOCAL(i) || IN_LOOPBACK(i))
		return (0);
	if (IN_EXPERIMENTAL(i) && !V_ip_allow_net240)
		return (0);
	if (IN_ZERONET(i) && !V_ip_allow_net0)
		return (0);
	return (1);
}

/*
 * Sysctl to manage prefix of reserved loopback network; translate
 * to/from mask.  The mask is always contiguous high-order 1 bits
 * followed by all 0 bits.
 */
static int
sysctl_loopback_prefixlen(SYSCTL_HANDLER_ARGS)
{
	int error, preflen;

	/* ffs is 1-based; compensate. */
	preflen = 33 - ffs(V_in_loopback_mask);
	error = sysctl_handle_int(oidp, &preflen, 0, req);
	if (error || !req->newptr)
		return (error);
	if (preflen < 8 || preflen > 31)
		return (EINVAL);
	V_in_loopback_mask = 0xffffffff << (32 - preflen);
	return (0);
}

/*
 * Trim a mask in a sockaddr
 */
static void
in_socktrim(struct sockaddr_in *ap)
{
    char *cplim = (char *) &ap->sin_addr;
    char *cp = (char *) (&ap->sin_addr + 1);

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
in_control_ioctl(u_long cmd, void *data, struct ifnet *ifp,
    struct ucred *cred)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr_in *addr = (struct sockaddr_in *)&ifr->ifr_addr;
	struct epoch_tracker et;
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
	case SIOCGIFALIAS:
		sx_xlock(&in_control_sx);
		error = in_gifaddr_ioctl(cmd, data, ifp, cred);
		sx_xunlock(&in_control_sx);
		return (error);
	case SIOCDIFADDR:
		sx_xlock(&in_control_sx);
		error = in_difaddr_ioctl(cmd, data, ifp, cred);
		sx_xunlock(&in_control_sx);
		return (error);
	case OSIOCAIFADDR:	/* 9.x compat */
	case SIOCAIFADDR:
		sx_xlock(&in_control_sx);
		error = in_aifaddr_ioctl(cmd, data, ifp, cred);
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
	    prison_check_ip4(cred, &addr->sin_addr) != 0)
		return (EADDRNOTAVAIL);

	/*
	 * Find address for this interface, if it exists.  If an
	 * address was specified, find that one instead of the
	 * first one on the interface, if possible.
	 */
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		ia = (struct in_ifaddr *)ifa;
		if (ia->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr)
			break;
	}
	if (ifa == NULL)
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
			if (ifa->ifa_addr->sa_family == AF_INET) {
				ia = (struct in_ifaddr *)ifa;
				if (prison_check_ip4(cred,
				    &ia->ia_addr.sin_addr) == 0)
					break;
			}

	if (ifa == NULL) {
		NET_EPOCH_EXIT(et);
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

	NET_EPOCH_EXIT(et);

	return (error);
}

int
in_control(struct socket *so, u_long cmd, void *data, struct ifnet *ifp,
    struct thread *td)
{
	return (in_control_ioctl(cmd, data, ifp, td ? td->td_ucred : NULL));
}

static int
in_aifaddr_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp, struct ucred *cred)
{
	const struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	const struct sockaddr_in *addr = &ifra->ifra_addr;
	const struct sockaddr_in *broadaddr = &ifra->ifra_broadaddr;
	const struct sockaddr_in *mask = &ifra->ifra_mask;
	const struct sockaddr_in *dstaddr = &ifra->ifra_dstaddr;
	const int vhid = (cmd == SIOCAIFADDR) ? ifra->ifra_vhid : 0;
	struct epoch_tracker et;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	bool iaIsFirst;
	int error = 0;

	error = priv_check_cred(cred, PRIV_NET_ADDIFADDR);
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
	if (vhid != 0 && carp_attach_p == NULL)
		return (EPROTONOSUPPORT);

#ifdef MAC
	/* Check if a MAC policy disallows setting the IPv4 address. */
	error = mac_inet_check_add_addr(cred, &addr->sin_addr, ifp);
	if (error != 0)
		return (error);
#endif

	/*
	 * See whether address already exist.
	 */
	iaIsFirst = true;
	ia = NULL;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *it;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		it = (struct in_ifaddr *)ifa;
		if (it->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		    prison_check_ip4(cred, &addr->sin_addr) == 0)
			ia = it;
		else
			iaIsFirst = false;
	}
	NET_EPOCH_EXIT(et);

	if (ia != NULL)
		(void )in_difaddr_ioctl(cmd, data, ifp, cred);

	ifa = ifa_alloc(sizeof(struct in_ifaddr), M_WAITOK);
	ia = (struct in_ifaddr *)ifa;
	ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
	ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
	ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
	callout_init_rw(&ia->ia_garp_timer, &ifp->if_addr_lock,
	    CALLOUT_RETURNUNLOCKED);

	ia->ia_ifp = ifp;
	ia->ia_addr = *addr;
	if (mask->sin_len != 0) {
		ia->ia_sockmask = *mask;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr);
	} else {
		in_addr_t i = ntohl(addr->sin_addr.s_addr);

		/*
	 	 * If netmask isn't supplied, use historical default.
		 * This is deprecated for interfaces other than loopback
		 * or point-to-point; warn in other cases.  In the future
		 * we should return an error rather than warning.
	 	 */
		if ((ifp->if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) == 0)
			printf("%s: set address: WARNING: network mask "
			     "should be specified; using historical default\n",
			     ifp->if_xname);
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

	if (vhid != 0) {
		error = (*carp_attach_p)(&ia->ia_ifa, vhid);
		if (error)
			return (error);
	}

	/* if_addrhead is already referenced by ifa_alloc() */
	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);
	IF_ADDR_WUNLOCK(ifp);

	ifa_ref(ifa);			/* in_ifaddrhead */
	sx_assert(&in_control_sx, SA_XLOCKED);
	CK_STAILQ_INSERT_TAIL(&V_in_ifaddrhead, ia, ia_link);
	CK_LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr), ia,
	    ia_hash);

	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl != NULL) {
		error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia);
		if (error)
			goto fail1;
	}

	/*
	 * Add route for the network.
	 */
	if (vhid == 0) {
		error = in_addprefix(ia);
		if (error)
			goto fail1;
	}

	/*
	 * Add a loopback route to self.
	 */
	if (vhid == 0 && ia_need_loopback_route(ia)) {
		struct in_ifaddr *eia;

		eia = in_localip_more(ia);

		if (eia == NULL) {
			error = ifa_add_loopback_route((struct ifaddr *)ia,
			    (struct sockaddr *)&ia->ia_addr);
			if (error)
				goto fail2;
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

	/*
	 * Note: we don't need extra reference for ifa, since we called
	 * with sx lock held, and ifaddr can not be deleted in concurrent
	 * thread.
	 */
	EVENTHANDLER_INVOKE(ifaddr_event_ext, ifp, ifa, IFADDR_EVENT_ADD);

	return (error);

fail2:
	if (vhid == 0)
		(void )in_scrubprefix(ia, LLE_STATIC);

fail1:
	if (ia->ia_ifa.ifa_carp)
		(*carp_detach_p)(&ia->ia_ifa, false);

	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifaddr, ifa_link);
	IF_ADDR_WUNLOCK(ifp);
	ifa_free(&ia->ia_ifa);		/* if_addrhead */

	sx_assert(&in_control_sx, SA_XLOCKED);
	CK_STAILQ_REMOVE(&V_in_ifaddrhead, ia, in_ifaddr, ia_link);
	CK_LIST_REMOVE(ia, ia_hash);
	ifa_free(&ia->ia_ifa);		/* in_ifaddrhead */

	return (error);
}

static int
in_difaddr_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp, struct ucred *cred)
{
	const struct ifreq *ifr = (struct ifreq *)data;
	const struct sockaddr_in *addr = (const struct sockaddr_in *)
	    &ifr->ifr_addr;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	bool deleteAny, iaIsLast;
	int error;

	if (cred != NULL) {
		error = priv_check_cred(cred, PRIV_NET_DELIFADDR);
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
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *it;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		it = (struct in_ifaddr *)ifa;
		if (deleteAny && ia == NULL && (cred == NULL ||
		    prison_check_ip4(cred, &it->ia_addr.sin_addr) == 0))
			ia = it;

		if (it->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		    (cred == NULL || prison_check_ip4(cred,
		    &addr->sin_addr) == 0))
			ia = it;

		if (it != ia)
			iaIsLast = false;
	}

	if (ia == NULL) {
		IF_ADDR_WUNLOCK(ifp);
		return (EADDRNOTAVAIL);
	}

	CK_STAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifaddr, ifa_link);
	IF_ADDR_WUNLOCK(ifp);
	ifa_free(&ia->ia_ifa);		/* if_addrhead */

	sx_assert(&in_control_sx, SA_XLOCKED);
	CK_STAILQ_REMOVE(&V_in_ifaddrhead, ia, in_ifaddr, ia_link);
	CK_LIST_REMOVE(ia, ia_hash);

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
		(*carp_detach_p)(&ia->ia_ifa, cmd == SIOCAIFADDR);

	/*
	 * If this is the last IPv4 address configured on this
	 * interface, leave the all-hosts group.
	 * No state-change report need be transmitted.
	 */
	if (iaIsLast && (ifp->if_flags & IFF_MULTICAST)) {
		struct in_ifinfo *ii;

		ii = ((struct in_ifinfo *)ifp->if_afdata[AF_INET]);
		if (ii->ii_allhosts) {
			(void)in_leavegroup(ii->ii_allhosts, NULL);
			ii->ii_allhosts = NULL;
		}
	}

	IF_ADDR_WLOCK(ifp);
	if (callout_stop(&ia->ia_garp_timer) == 1) {
		ifa_free(&ia->ia_ifa);
	}
	IF_ADDR_WUNLOCK(ifp);

	EVENTHANDLER_INVOKE(ifaddr_event_ext, ifp, &ia->ia_ifa,
	    IFADDR_EVENT_DEL);
	ifa_free(&ia->ia_ifa);		/* in_ifaddrhead */

	return (0);
}

static int
in_gifaddr_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp, struct ucred *cred)
{
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	const struct sockaddr_in *addr = &ifra->ifra_addr;
	struct epoch_tracker et;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;

	/*
	 * ifra_addr must be present and be of INET family.
	 */
	if (addr->sin_len != sizeof(struct sockaddr_in) ||
	    addr->sin_family != AF_INET)
		return (EINVAL);

	/*
	 * See whether address exist.
	 */
	ia = NULL;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *it;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		it = (struct in_ifaddr *)ifa;
		if (it->ia_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		    prison_check_ip4(cred, &addr->sin_addr) == 0) {
			ia = it;
			break;
		}
	}
	if (ia == NULL) {
		NET_EPOCH_EXIT(et);
		return (EADDRNOTAVAIL);
	}

	ifra->ifra_mask = ia->ia_sockmask;
	if ((ifp->if_flags & IFF_POINTOPOINT) &&
	    ia->ia_dstaddr.sin_family == AF_INET)
		ifra->ifra_dstaddr = ia->ia_dstaddr;
	else if ((ifp->if_flags & IFF_BROADCAST) &&
	    ia->ia_broadaddr.sin_family == AF_INET)
		ifra->ifra_broadaddr = ia->ia_broadaddr;
	else
		memset(&ifra->ifra_broadaddr, 0,
		    sizeof(ifra->ifra_broadaddr));

	NET_EPOCH_EXIT(et);
	return (0);
}

static int
in_match_ifaddr(const struct rtentry *rt, const struct nhop_object *nh, void *arg)
{

	if (nh->nh_ifa == (struct ifaddr *)arg)
		return (1);

	return (0);
}

static int
in_handle_prefix_route(uint32_t fibnum, int cmd,
    struct sockaddr_in *dst, struct sockaddr_in *netmask, struct ifaddr *ifa,
    struct ifnet *ifp)
{

	NET_EPOCH_ASSERT();

	/* Prepare gateway */
	struct sockaddr_dl_short sdl = {
		.sdl_family = AF_LINK,
		.sdl_len = sizeof(struct sockaddr_dl_short),
		.sdl_type = ifa->ifa_ifp->if_type,
		.sdl_index = ifa->ifa_ifp->if_index,
	};

	struct rt_addrinfo info = {
		.rti_ifa = ifa,
		.rti_ifp = ifp,
		.rti_flags = RTF_PINNED | ((netmask != NULL) ? 0 : RTF_HOST),
		.rti_info = {
			[RTAX_DST] = (struct sockaddr *)dst,
			[RTAX_NETMASK] = (struct sockaddr *)netmask,
			[RTAX_GATEWAY] = (struct sockaddr *)&sdl,
		},
		/* Ensure we delete the prefix IFF prefix ifa matches */
		.rti_filter = in_match_ifaddr,
		.rti_filterdata = ifa,
	};

	return (rib_handle_ifaddr_info(fibnum, cmd, &info));
}

/*
 * Routing table interaction with interface addresses.
 *
 * In general, two types of routes needs to be installed:
 * a) "interface" or "prefix" route, telling user that the addresses
 *   behind the ifa prefix are reached directly.
 * b) "loopback" route installed for the ifa address, telling user that
 *   the address belongs to local system.
 *
 * Handling for (a) and (b) differs in multi-fib aspects, hence they
 *  are implemented in different functions below.
 *
 * The cases above may intersect - /32 interface aliases results in
 *  the same prefix produced by (a) and (b). This blurs the definition
 *  of the "loopback" route and complicate interactions. The interaction
 *  table is defined below. The case numbers are used in the multiple
 *  functions below to refer to the particular test case.
 *
 * There can be multiple options:
 * 1) Adding address with prefix on non-p2p/non-loopback interface.
 *  Example: 192.0.2.1/24. Action:
 *  * add "prefix" route towards 192.0.2.0/24 via @ia interface,
 *    using @ia as an address source.
 *  * add "loopback" route towards 192.0.2.1 via V_loif, saving
 *   @ia ifp in the gateway and using @ia as an address source.
 *
 * 2) Adding address with /32 mask to non-p2p/non-loopback interface.
 *  Example: 192.0.2.2/32. Action:
 *  * add "prefix" host route via V_loif, using @ia as an address source.
 *
 * 3) Adding address with or without prefix to p2p interface.
 *  Example: 10.0.0.1/24->10.0.0.2. Action:
 *  * add "prefix" host route towards 10.0.0.2 via this interface, using @ia
 *    as an address source. Note: no sense in installing full /24 as the interface
 *    is point-to-point.
 *  * add "loopback" route towards 10.0.9.1 via V_loif, saving
 *   @ia ifp in the gateway and using @ia as an address source.
 *
 * 4) Adding address with or without prefix to loopback interface.
 *  Example: 192.0.2.1/24. Action:
 *  * add "prefix" host route via @ia interface, using @ia as an address source.
 *    Note: Skip installing /24 prefix as it would introduce TTL loop
 *    for the traffic destined to these addresses.
 */

/*
 * Checks if @ia needs to install loopback route to @ia address via
 *  ifa_maintain_loopback_route().
 *
 * Return true on success.
 */
static bool
ia_need_loopback_route(const struct in_ifaddr *ia)
{
	struct ifnet *ifp = ia->ia_ifp;

	/* Case 4: Skip loopback interfaces */
	if ((ifp->if_flags & IFF_LOOPBACK) ||
	    (ia->ia_addr.sin_addr.s_addr == INADDR_ANY))
		return (false);

	/* Clash avoidance: Skip p2p interfaces with both addresses are equal */
	if ((ifp->if_flags & IFF_POINTOPOINT) &&
	    ia->ia_dstaddr.sin_addr.s_addr == ia->ia_addr.sin_addr.s_addr)
		return (false);

	/* Case 2: skip /32 prefixes */
	if (!(ifp->if_flags & IFF_POINTOPOINT) &&
	    (ia->ia_sockmask.sin_addr.s_addr == INADDR_BROADCAST))
		return (false);

	return (true);
}

/*
 * Calculate "prefix" route corresponding to @ia.
 */
static void
ia_getrtprefix(const struct in_ifaddr *ia, struct in_addr *prefix, struct in_addr *mask)
{

	if (ia->ia_ifp->if_flags & IFF_POINTOPOINT) {
		/* Case 3: return host route for dstaddr */
		*prefix = ia->ia_dstaddr.sin_addr;
		mask->s_addr = INADDR_BROADCAST;
	} else if (ia->ia_ifp->if_flags & IFF_LOOPBACK) {
		/* Case 4: return host route for ifaddr */
		*prefix = ia->ia_addr.sin_addr;
		mask->s_addr = INADDR_BROADCAST;
	} else {
		/* Cases 1,2: return actual ia prefix */
		*prefix = ia->ia_addr.sin_addr;
		*mask = ia->ia_sockmask.sin_addr;
		prefix->s_addr &= mask->s_addr;
	}
}

/*
 * Adds or delete interface "prefix" route corresponding to @ifa.
 *  Returns 0 on success or errno.
 */
static int
in_handle_ifaddr_route(int cmd, struct in_ifaddr *ia)
{
	struct ifaddr *ifa = &ia->ia_ifa;
	struct in_addr daddr, maddr;
	struct sockaddr_in *pmask;
	struct epoch_tracker et;
	int error;

	ia_getrtprefix(ia, &daddr, &maddr);

	struct sockaddr_in mask = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr = maddr,
	};

	pmask = (maddr.s_addr != INADDR_BROADCAST) ? &mask : NULL;

	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr.s_addr = daddr.s_addr & maddr.s_addr,
	};

	struct ifnet *ifp = ia->ia_ifp;

	if ((maddr.s_addr == INADDR_BROADCAST) &&
	    (!(ia->ia_ifp->if_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)))) {
		/* Case 2: host route on broadcast interface */
		ifp = V_loif;
	}

	uint32_t fibnum = ifa->ifa_ifp->if_fib;
	NET_EPOCH_ENTER(et);
	error = in_handle_prefix_route(fibnum, cmd, &dst, pmask, ifa, ifp);
	NET_EPOCH_EXIT(et);

	return (error);
}

/*
 * Check if we have a route for the given prefix already.
 */
static bool
in_hasrtprefix(struct in_ifaddr *target)
{
	struct epoch_tracker et;
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	bool result = false;

	ia_getrtprefix(target, &prefix, &mask);

	/* Look for an existing address with the same prefix, mask, and fib */
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		ia_getrtprefix(ia, &p, &m);

		if (prefix.s_addr != p.s_addr ||
		    mask.s_addr != m.s_addr)
			continue;

		if (target->ia_ifp->if_fib != ia->ia_ifp->if_fib)
			continue;

		/*
		 * If we got a matching prefix route inserted by other
		 * interface address, we are done here.
		 */
		if (ia->ia_flags & IFA_ROUTE) {
			result = true;
			break;
		}
	}
	NET_EPOCH_EXIT(et);

	return (result);
}

int
in_addprefix(struct in_ifaddr *target)
{
	int error;

	if (in_hasrtprefix(target)) {
		if (V_nosameprefix)
			return (EEXIST);
		else {
			rt_addrmsg(RTM_ADD, &target->ia_ifa,
			    target->ia_ifp->if_fib);
			return (0);
		}
	}

	/*
	 * No-one seem to have this prefix route, so we try to insert it.
	 */
	rt_addrmsg(RTM_ADD, &target->ia_ifa, target->ia_ifp->if_fib);
	error = in_handle_ifaddr_route(RTM_ADD, target);
	if (!error)
		target->ia_flags |= IFA_ROUTE;
	return (error);
}

/*
 * Removes either all lle entries for given @ia, or lle
 * corresponding to @ia address.
 */
static void
in_scrubprefixlle(struct in_ifaddr *ia, int all, u_int flags)
{
	struct sockaddr_in addr, mask;
	struct sockaddr *saddr, *smask;
	struct ifnet *ifp;

	saddr = (struct sockaddr *)&addr;
	bzero(&addr, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	smask = (struct sockaddr *)&mask;
	bzero(&mask, sizeof(mask));
	mask.sin_len = sizeof(mask);
	mask.sin_family = AF_INET;
	mask.sin_addr.s_addr = ia->ia_subnetmask;
	ifp = ia->ia_ifp;

	if (all) {
		/*
		 * Remove all L2 entries matching given prefix.
		 * Convert address to host representation to avoid
		 * doing this on every callback. ia_subnetmask is already
		 * stored in host representation.
		 */
		addr.sin_addr.s_addr = ntohl(ia->ia_addr.sin_addr.s_addr);
		lltable_prefix_free(AF_INET, saddr, smask, flags);
	} else {
		/* Remove interface address only */
		addr.sin_addr.s_addr = ia->ia_addr.sin_addr.s_addr;
		lltable_delete_addr(LLTABLE(ifp), LLE_IFADDR, saddr);
	}
}

/*
 * If there is no other address in the system that can serve a route to the
 * same prefix, remove the route.  Hand over the route to the new address
 * otherwise.
 */
int
in_scrubprefix(struct in_ifaddr *target, u_int flags)
{
	struct epoch_tracker et;
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error = 0;

	/*
	 * Remove the loopback route to the interface address.
	 */
	if (ia_need_loopback_route(target) && (flags & LLE_STATIC)) {
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
	}

	ia_getrtprefix(target, &prefix, &mask);

	if ((target->ia_flags & IFA_ROUTE) == 0) {
		rt_addrmsg(RTM_DELETE, &target->ia_ifa, target->ia_ifp->if_fib);

		/*
		 * Removing address from !IFF_UP interface or
		 * prefix which exists on other interface (along with route).
		 * No entries should exist here except target addr.
		 * Given that, delete this entry only.
		 */
		in_scrubprefixlle(target, 0, flags);
		return (0);
	}

	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
		ia_getrtprefix(ia, &p, &m);

		if (prefix.s_addr != p.s_addr ||
		    mask.s_addr != m.s_addr)
			continue;

		if ((ia->ia_ifp->if_flags & IFF_UP) == 0)
			continue;

		/*
		 * If we got a matching prefix address, move IFA_ROUTE and
		 * the route itself to it.  Make sure that routing daemons
		 * get a heads-up.
		 */
		if ((ia->ia_flags & IFA_ROUTE) == 0) {
			ifa_ref(&ia->ia_ifa);
			NET_EPOCH_EXIT(et);
			error = in_handle_ifaddr_route(RTM_DELETE, target);
			if (error == 0)
				target->ia_flags &= ~IFA_ROUTE;
			else
				log(LOG_INFO, "in_scrubprefix: err=%d, old prefix delete failed\n",
					error);
			/* Scrub all entries IFF interface is different */
			in_scrubprefixlle(target, target->ia_ifp != ia->ia_ifp,
			    flags);
			error = in_handle_ifaddr_route(RTM_ADD, ia);
			if (error == 0)
				ia->ia_flags |= IFA_ROUTE;
			else
				log(LOG_INFO, "in_scrubprefix: err=%d, new prefix add failed\n",
					error);
			ifa_free(&ia->ia_ifa);
			return (error);
		}
	}
	NET_EPOCH_EXIT(et);

	/*
	 * remove all L2 entries on the given prefix
	 */
	in_scrubprefixlle(target, 1, flags);

	/*
	 * As no-one seem to have this prefix, we can remove the route.
	 */
	rt_addrmsg(RTM_DELETE, &target->ia_ifa, target->ia_ifp->if_fib);
	error = in_handle_ifaddr_route(RTM_DELETE, target);
	if (error == 0)
		target->ia_flags &= ~IFA_ROUTE;
	else
		log(LOG_INFO, "in_scrubprefix: err=%d, prefix delete failed\n", error);
	return (error);
}

void
in_ifscrub_all(void)
{
	struct ifnet *ifp;
	struct ifaddr *ifa, *nifa;
	struct ifaliasreq ifr;

	IFNET_RLOCK();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		/* Cannot lock here - lock recursion. */
		/* NET_EPOCH_ENTER(et); */
		CK_STAILQ_FOREACH_SAFE(ifa, &ifp->if_addrhead, ifa_link, nifa) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;

			/*
			 * This is ugly but the only way for legacy IP to
			 * cleanly remove addresses and everything attached.
			 */
			bzero(&ifr, sizeof(ifr));
			ifr.ifra_addr = *ifa->ifa_addr;
			if (ifa->ifa_dstaddr)
			ifr.ifra_broadaddr = *ifa->ifa_dstaddr;
			(void)in_control(NULL, SIOCDIFADDR, (caddr_t)&ifr,
			    ifp, NULL);
		}
		/* NET_EPOCH_EXIT(et); */
		in_purgemaddrs(ifp);
		igmp_domifdetach(ifp);
	}
	IFNET_RUNLOCK();
}

int
in_ifaddr_broadcast(struct in_addr in, struct in_ifaddr *ia)
{

	return ((in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
	     /*
	      * Optionally check for old-style (host 0) broadcast, but
	      * taking into account that RFC 3021 obsoletes it.
	      */
	    (V_broadcast_lowest && ia->ia_subnetmask != IN_RFC3021_MASK &&
	    ntohl(in.s_addr) == ia->ia_subnet)) &&
	     /*
	      * Check for an all one subnetmask. These
	      * only exist when an interface gets a secondary
	      * address.
	      */
	    ia->ia_subnetmask != (u_long)0xffffffff);
}

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	int found;

	NET_EPOCH_ASSERT();

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return (1);
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return (0);
	found = 0;
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    in_ifaddr_broadcast(in, (struct in_ifaddr *)ifa)) {
			found = 1;
			break;
		}
	return (found);
}

/*
 * On interface removal, clean up IPv4 data structures hung off of the ifnet.
 */
void
in_ifdetach(struct ifnet *ifp)
{
	IN_MULTI_LOCK();
	in_pcbpurgeif0(&V_ripcbinfo, ifp);
	in_pcbpurgeif0(&V_udbinfo, ifp);
	in_pcbpurgeif0(&V_ulitecbinfo, ifp);
	in_purgemaddrs(ifp);
	IN_MULTI_UNLOCK();

	/*
	 * Make sure all multicast deletions invoking if_ioctl() are
	 * completed before returning. Else we risk accessing a freed
	 * ifnet structure pointer.
	 */
	inm_release_wait(NULL);
}

static void
in_ifnet_event(void *arg __unused, struct ifnet *ifp, int event)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	int error;

	NET_EPOCH_ENTER(et);
	switch (event) {
	case IFNET_EVENT_DOWN:
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = (struct in_ifaddr *)ifa;
			if ((ia->ia_flags & IFA_ROUTE) == 0)
				continue;
			ifa_ref(ifa);
			/*
			 * in_scrubprefix() kills the interface route.
			 */
			in_scrubprefix(ia, 0);
			/*
			 * in_ifadown gets rid of all the rest of the
			 * routes.  This is not quite the right thing
			 * to do, but at least if we are running a
			 * routing process they will come back.
			 */
			in_ifadown(ifa, 0);
			ifa_free(ifa);
		}
		break;

	case IFNET_EVENT_UP:
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = (struct in_ifaddr *)ifa;
			if (ia->ia_flags & IFA_ROUTE)
				continue;
			ifa_ref(ifa);
			error = ifa_del_loopback_route(ifa, ifa->ifa_addr);
			rt_addrmsg(RTM_ADD, ifa, ifa->ifa_ifp->if_fib);
			error = in_handle_ifaddr_route(RTM_ADD, ia);
			if (error == 0)
				ia->ia_flags |= IFA_ROUTE;
			error = ifa_add_loopback_route(ifa, ifa->ifa_addr);
			ifa_free(ifa);
		}
		break;
	}
	NET_EPOCH_EXIT(et);
}
EVENTHANDLER_DEFINE(ifnet_event, in_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);

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
	struct epoch_tracker	 et;
	struct in_multi_head purgeinms;
	struct in_multi		*inm;
	struct ifmultiaddr	*ifma;

	SLIST_INIT(&purgeinms);
	IN_MULTI_LIST_LOCK();

	/*
	 * Extract list of in_multi associated with the detaching ifp
	 * which the PF_INET layer is about to release.
	 * We need to do this as IF_ADDR_LOCK() may be re-acquired
	 * by code further down.
	 */
	IF_ADDR_WLOCK(ifp);
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		inm = inm_ifmultiaddr_get_inm(ifma);
		if (inm == NULL)
			continue;
		inm_rele_locked(&purgeinms, inm);
	}
	NET_EPOCH_EXIT(et);
	IF_ADDR_WUNLOCK(ifp);

	inm_release_list_deferred(&purgeinms);
	igmp_ifdetach(ifp);
	IN_MULTI_LIST_UNLOCK();
}

struct in_llentry {
	struct llentry		base;
};

#define	IN_LLTBL_DEFAULT_HSIZE	32
#define	IN_LLTBL_HASH(k, h) \
	(((((((k >> 8) ^ k) >> 8) ^ k) >> 8) ^ k) & ((h) - 1))

/*
 * Do actual deallocation of @lle.
 */
static void
in_lltable_destroy_lle_unlocked(epoch_context_t ctx)
{
	struct llentry *lle;

	lle = __containerof(ctx, struct llentry, lle_epoch_ctx);
	LLE_LOCK_DESTROY(lle);
	LLE_REQ_DESTROY(lle);
	free(lle, M_LLTABLE);
}

/*
 * Called by LLE_FREE_LOCKED when number of references
 * drops to zero.
 */
static void
in_lltable_destroy_lle(struct llentry *lle)
{

	LLE_WUNLOCK(lle);
	NET_EPOCH_CALL(in_lltable_destroy_lle_unlocked, &lle->lle_epoch_ctx);
}

static struct llentry *
in_lltable_new(struct in_addr addr4, u_int flags)
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
	lle->base.r_l3addr.addr4 = addr4;
	lle->base.lle_refcnt = 1;
	lle->base.lle_free = in_lltable_destroy_lle;
	LLE_LOCK_INIT(&lle->base);
	LLE_REQ_INIT(&lle->base);
	callout_init(&lle->base.lle_timer, 1);

	return (&lle->base);
}

#define IN_ARE_MASKED_ADDR_EQUAL(d, a, m)	(		\
	((((d).s_addr ^ (a).s_addr) & (m).s_addr)) == 0 )

static int
in_lltable_match_prefix(const struct sockaddr *saddr,
    const struct sockaddr *smask, u_int flags, struct llentry *lle)
{
	struct in_addr addr, mask, lle_addr;

	addr = ((const struct sockaddr_in *)saddr)->sin_addr;
	mask = ((const struct sockaddr_in *)smask)->sin_addr;
	lle_addr.s_addr = ntohl(lle->r_l3addr.addr4.s_addr);

	if (IN_ARE_MASKED_ADDR_EQUAL(lle_addr, addr, mask) == 0)
		return (0);

	if (lle->la_flags & LLE_IFADDR) {
		/*
		 * Delete LLE_IFADDR records IFF address & flag matches.
		 * Note that addr is the interface address within prefix
		 * being matched.
		 * Note also we should handle 'ifdown' cases without removing
		 * ifaddr macs.
		 */
		if (addr.s_addr == lle_addr.s_addr && (flags & LLE_STATIC) != 0)
			return (1);
		return (0);
	}

	/* flags & LLE_STATIC means deleting both dynamic and static entries */
	if ((flags & LLE_STATIC) || !(lle->la_flags & LLE_STATIC))
		return (1);

	return (0);
}

static void
in_lltable_free_entry(struct lltable *llt, struct llentry *lle)
{
	size_t pkts_dropped;

	LLE_WLOCK_ASSERT(lle);
	KASSERT(llt != NULL, ("lltable is NULL"));

	/* Unlink entry from table if not already */
	if ((lle->la_flags & LLE_LINKED) != 0) {
		IF_AFDATA_WLOCK_ASSERT(llt->llt_ifp);
		lltable_unlink_entry(llt, lle);
	}

	/* Drop hold queue */
	pkts_dropped = llentry_free(lle);
	ARPSTAT_ADD(dropped, pkts_dropped);
}

static int
in_lltable_rtcheck(struct ifnet *ifp, u_int flags, const struct sockaddr *l3addr)
{
	struct nhop_object *nh;
	struct in_addr addr;

	KASSERT(l3addr->sa_family == AF_INET,
	    ("sin_family %d", l3addr->sa_family));

	addr = ((const struct sockaddr_in *)l3addr)->sin_addr;

	nh = fib4_lookup(ifp->if_fib, addr, 0, NHR_NONE, 0);
	if (nh == NULL)
		return (EINVAL);

	/*
	 * If the gateway for an existing host route matches the target L3
	 * address, which is a special route inserted by some implementation
	 * such as MANET, and the interface is of the correct type, then
	 * allow for ARP to proceed.
	 */
	if (nh->nh_flags & NHF_GATEWAY) {
		if (!(nh->nh_flags & NHF_HOST) || nh->nh_ifp->if_type != IFT_ETHER ||
		    (nh->nh_ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) != 0 ||
		    memcmp(nh->gw_sa.sa_data, l3addr->sa_data,
		    sizeof(in_addr_t)) != 0) {
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
	if ((nh->nh_ifp != ifp) && (nh->nh_flags & NHF_HOST) == 0) {
		struct in_ifaddr *ia = (struct in_ifaddr *)ifaof_ifpforaddr(l3addr, ifp);
		struct in_addr dst_addr, mask_addr;

		if (ia == NULL)
			return (EINVAL);

		/*
		 * ifaof_ifpforaddr() returns _best matching_ IFA.
		 * It is possible that ifa prefix does not cover our address.
		 * Explicitly verify and fail if that's the case.
		 */
		dst_addr = IA_SIN(ia)->sin_addr;
		mask_addr.s_addr = htonl(ia->ia_subnetmask);

		if (!IN_ARE_MASKED_ADDR_EQUAL(dst_addr, addr, mask_addr))
			return (EINVAL);
	}

	return (0);
}

static inline uint32_t
in_lltable_hash_dst(const struct in_addr dst, uint32_t hsize)
{

	return (IN_LLTBL_HASH(dst.s_addr, hsize));
}

static uint32_t
in_lltable_hash(const struct llentry *lle, uint32_t hsize)
{

	return (in_lltable_hash_dst(lle->r_l3addr.addr4, hsize));
}

static void
in_lltable_fill_sa_entry(const struct llentry *lle, struct sockaddr *sa)
{
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in *)sa;
	bzero(sin, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = lle->r_l3addr.addr4;
}

static inline struct llentry *
in_lltable_find_dst(struct lltable *llt, struct in_addr dst)
{
	struct llentry *lle;
	struct llentries *lleh;
	u_int hashidx;

	hashidx = in_lltable_hash_dst(dst, llt->llt_hsize);
	lleh = &llt->lle_head[hashidx];
	CK_LIST_FOREACH(lle, lleh, lle_next) {
		if (lle->la_flags & LLE_DELETED)
			continue;
		if (lle->r_l3addr.addr4.s_addr == dst.s_addr)
			break;
	}

	return (lle);
}

static void
in_lltable_delete_entry(struct lltable *llt, struct llentry *lle)
{

	lle->la_flags |= LLE_DELETED;
	EVENTHANDLER_INVOKE(lle_event, lle, LLENTRY_DELETED);
#ifdef DIAGNOSTIC
	log(LOG_INFO, "ifaddr cache = %p is deleted\n", lle);
#endif
	llentry_free(lle);
}

static struct llentry *
in_lltable_alloc(struct lltable *llt, u_int flags, const struct sockaddr *l3addr)
{
	const struct sockaddr_in *sin = (const struct sockaddr_in *)l3addr;
	struct ifnet *ifp = llt->llt_ifp;
	struct llentry *lle;
	char linkhdr[LLE_MAX_LINKHDR];
	size_t linkhdrsize;
	int lladdr_off;

	KASSERT(l3addr->sa_family == AF_INET,
	    ("sin_family %d", l3addr->sa_family));

	/*
	 * A route that covers the given address must have
	 * been installed 1st because we are doing a resolution,
	 * verify this.
	 */
	if (!(flags & LLE_IFADDR) &&
	    in_lltable_rtcheck(ifp, flags, l3addr) != 0)
		return (NULL);

	lle = in_lltable_new(sin->sin_addr, flags);
	if (lle == NULL) {
		log(LOG_INFO, "lla_lookup: new lle malloc failed\n");
		return (NULL);
	}
	lle->la_flags = flags;
	if (flags & LLE_STATIC)
		lle->r_flags |= RLLE_VALID;
	if ((flags & LLE_IFADDR) == LLE_IFADDR) {
		linkhdrsize = LLE_MAX_LINKHDR;
		if (lltable_calc_llheader(ifp, AF_INET, IF_LLADDR(ifp),
		    linkhdr, &linkhdrsize, &lladdr_off) != 0) {
			in_lltable_free_entry(llt, lle);
			return (NULL);
		}
		lltable_set_entry_addr(ifp, lle, linkhdr, linkhdrsize,
		    lladdr_off);
		lle->la_flags |= LLE_STATIC;
		lle->r_flags |= (RLLE_VALID | RLLE_IFADDR);
		lle->la_expire = 0;
	}

	return (lle);
}

/*
 * Return NULL if not found or marked for deletion.
 * If found return lle read locked.
 */
static struct llentry *
in_lltable_lookup(struct lltable *llt, u_int flags, const struct sockaddr *l3addr)
{
	const struct sockaddr_in *sin = (const struct sockaddr_in *)l3addr;
	struct llentry *lle;

	IF_AFDATA_LOCK_ASSERT(llt->llt_ifp);
	KASSERT(l3addr->sa_family == AF_INET,
	    ("sin_family %d", l3addr->sa_family));
	KASSERT((flags & (LLE_UNLOCKED | LLE_EXCLUSIVE)) !=
	    (LLE_UNLOCKED | LLE_EXCLUSIVE),
	    ("wrong lle request flags: %#x", flags));

	lle = in_lltable_find_dst(llt, sin->sin_addr);
	if (lle == NULL)
		return (NULL);
	if (flags & LLE_UNLOCKED)
		return (lle);

	if (flags & LLE_EXCLUSIVE)
		LLE_WLOCK(lle);
	else
		LLE_RLOCK(lle);

	/*
	 * If the afdata lock is not held, the LLE may have been unlinked while
	 * we were blocked on the LLE lock.  Check for this case.
	 */
	if (__predict_false((lle->la_flags & LLE_LINKED) == 0)) {
		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(lle);
		else
			LLE_RUNLOCK(lle);
		return (NULL);
	}
	return (lle);
}

static int
in_lltable_dump_entry(struct lltable *llt, struct llentry *lle,
    struct sysctl_req *wr)
{
	struct ifnet *ifp = llt->llt_ifp;
	/* XXX stack use */
	struct {
		struct rt_msghdr	rtm;
		struct sockaddr_in	sin;
		struct sockaddr_dl	sdl;
	} arpc;
	struct sockaddr_dl *sdl;
	int error;

	bzero(&arpc, sizeof(arpc));
	/* skip deleted entries */
	if ((lle->la_flags & LLE_DELETED) == LLE_DELETED)
		return (0);
	/* Skip if jailed and not a valid IP of the prison. */
	lltable_fill_sa_entry(lle,(struct sockaddr *)&arpc.sin);
	if (prison_if(wr->td->td_ucred, (struct sockaddr *)&arpc.sin) != 0)
		return (0);
	/*
	 * produce a msg made of:
	 *  struct rt_msghdr;
	 *  struct sockaddr_in; (IPv4)
	 *  struct sockaddr_dl;
	 */
	arpc.rtm.rtm_msglen = sizeof(arpc);
	arpc.rtm.rtm_version = RTM_VERSION;
	arpc.rtm.rtm_type = RTM_GET;
	arpc.rtm.rtm_flags = RTF_UP;
	arpc.rtm.rtm_addrs = RTA_DST | RTA_GATEWAY;

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
		bcopy(lle->ll_addr, LLADDR(sdl), ifp->if_addrlen);
	} else {
		sdl->sdl_alen = 0;
		bzero(LLADDR(sdl), ifp->if_addrlen);
	}

	arpc.rtm.rtm_rmx.rmx_expire =
	    lle->la_flags & LLE_STATIC ? 0 : lle->la_expire;
	arpc.rtm.rtm_flags |= (RTF_HOST | RTF_LLDATA);
	if (lle->la_flags & LLE_STATIC)
		arpc.rtm.rtm_flags |= RTF_STATIC;
	if (lle->la_flags & LLE_IFADDR)
		arpc.rtm.rtm_flags |= RTF_PINNED;
	arpc.rtm.rtm_index = ifp->if_index;
	error = SYSCTL_OUT(wr, &arpc, sizeof(arpc));

	return (error);
}

static void
in_lltable_post_resolved(struct lltable *llt, struct llentry *lle)
{
	struct ifnet *ifp = llt->llt_ifp;

	/* gratuitous ARP */
	if ((lle->la_flags & LLE_PUB) != 0)
		arprequest(ifp, &lle->r_l3addr.addr4, &lle->r_l3addr.addr4,
		    lle->ll_addr);
}

static struct lltable *
in_lltattach(struct ifnet *ifp)
{
	struct lltable *llt;

	llt = lltable_allocate_htbl(IN_LLTBL_DEFAULT_HSIZE);
 	llt->llt_af = AF_INET;
 	llt->llt_ifp = ifp;

	llt->llt_lookup = in_lltable_lookup;
	llt->llt_alloc_entry = in_lltable_alloc;
	llt->llt_delete_entry = in_lltable_delete_entry;
	llt->llt_dump_entry = in_lltable_dump_entry;
	llt->llt_hash = in_lltable_hash;
	llt->llt_fill_sa_entry = in_lltable_fill_sa_entry;
	llt->llt_free_entry = in_lltable_free_entry;
	llt->llt_match_prefix = in_lltable_match_prefix;
	llt->llt_mark_used = llentry_mark_used;
	llt->llt_post_resolved = in_lltable_post_resolved;
 	lltable_link(llt);

	return (llt);
}

struct lltable *
in_lltable_get(struct ifnet *ifp)
{
	struct lltable *llt = NULL;

	void *afdata_ptr = ifp->if_afdata[AF_INET];
	if (afdata_ptr != NULL)
		llt = ((struct in_ifinfo *)afdata_ptr)->ii_llt;
	return (llt);
}

void *
in_domifattach(struct ifnet *ifp)
{
	struct in_ifinfo *ii;

	ii = malloc(sizeof(struct in_ifinfo), M_IFADDR, M_WAITOK|M_ZERO);

	ii->ii_llt = in_lltattach(ifp);
	ii->ii_igmp = igmp_domifattach(ifp);

	return (ii);
}

void
in_domifdetach(struct ifnet *ifp, void *aux)
{
	struct in_ifinfo *ii = (struct in_ifinfo *)aux;

	igmp_domifdetach(ifp);
	lltable_free(ii->ii_llt);
	free(ii, M_IFADDR);
}
