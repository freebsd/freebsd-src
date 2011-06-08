/*	$FreeBSD$	*/
/*	$KAME: if_stf.c,v 1.73 2001/12/03 11:08:30 keiichi Exp $	*/

/*-
 * Copyright (C) 2000 WIDE Project.
 * Copyright (c) 2010-2011 Hiroki Sato <hrs@FreeBSD.org>
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
 */

/*
 * 6to4 interface, based on RFC 3056 + 6rd (RFC 5969) support.
 *
 * 6to4 interface is NOT capable of link-layer (I mean, IPv4) multicasting.
 * There is no address mapping defined from IPv6 multicast address to IPv4
 * address.  Therefore, we do not have IFF_MULTICAST on the interface.
 *
 * Due to the lack of address mapping for link-local addresses, we cannot
 * throw packets toward link-local addresses (fe80::x).  Also, we cannot throw
 * packets to link-local multicast addresses (ff02::x).
 *
 * Here are interesting symptoms due to the lack of link-local address:
 *
 * Unicast routing exchange:
 * - RIPng: Impossible.  Uses link-local multicast packet toward ff02::9,
 *   and link-local addresses as nexthop.
 * - OSPFv6: Impossible.  OSPFv6 assumes that there's link-local address
 *   assigned to the link, and makes use of them.  Also, HELLO packets use
 *   link-local multicast addresses (ff02::5 and ff02::6).
 * - BGP4+: Maybe.  You can only use global address as nexthop, and global
 *   address as TCP endpoint address.
 *
 * Multicast routing protocols:
 * - PIM: Hello packet cannot be used to discover adjacent PIM routers.
 *   Adjacent PIM routers must be configured manually (is it really spec-wise
 *   correct thing to do?).
 *
 * ICMPv6:
 * - Redirects cannot be used due to the lack of link-local address.
 *
 * stf interface does not have, and will not need, a link-local address.
 * It seems to have no real benefit and does not help the above symptoms much.
 * Even if we assign link-locals to interface, we cannot really
 * use link-local unicast/multicast on top of 6to4 cloud (since there's no
 * encapsulation defined for link-local address), and the above analysis does
 * not change.  RFC3056 does not mandate the assignment of link-local address
 * either.
 *
 * 6to4 interface has security issues.  Refer to
 * http://playground.iijlab.net/i-d/draft-itojun-ipv6-transition-abuse-00.txt
 * for details.  The code tries to filter out some of malicious packets.
 * Note that there is no way to be 100% secure.
 *
 * 6rd (RFC 5969) extension is enabled when an IPv6 GUA other than
 * 2002::/16 is assigned.  The stf(4) calculates a 6rd delegated
 * prefix from a 6rd prefix and an IPv4 address.
 *
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_stf.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip_ecn.h>

#include <netinet/ip_encap.h>

#include <machine/stdarg.h>

#include <net/bpf.h>

#include <security/mac/mac_framework.h>

#define	STF_DEBUG 1
#define ip_sprintf(buf, a)						\
	sprintf(buf, "%d.%d.%d.%d",					\
		(ntohl((a)->s_addr)>>24)&0xFF,				\
		(ntohl((a)->s_addr)>>16)&0xFF,				\
		(ntohl((a)->s_addr)>>8)&0xFF,				\
		(ntohl((a)->s_addr))&0xFF);
#if STF_DEBUG
#define DEBUG_PRINTF(a, ...)						\
	do {								\
		if (V_stf_debug >= a)					\
			printf(__VA_ARGS__);				\
	} while (0)
#else
#define DEBUG_PRINTF(a, ...)
#endif

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_STF, stf, CTLFLAG_RW, 0, "6to4 Interface");

static VNET_DEFINE(int, stf_route_cache) = 1;
#define V_stf_route_cache	VNET(stf_route_cache)
SYSCTL_VNET_INT(_net_link_stf, OID_AUTO, route_cache, CTLFLAG_RW,
    &VNET_NAME(stf_route_cache), 0,
    "Enable caching of IPv4 routes for 6to4 output.");

#if STF_DEBUG
static VNET_DEFINE(int, stf_debug) = 0;
#define V_stf_debug	VNET(stf_debug)
SYSCTL_VNET_INT(_net_link_stf, OID_AUTO, stf_debug, CTLFLAG_RW,
    &VNET_NAME(stf_debug), 0,
    "Enable displaying verbose debug message of stf interfaces");
#endif

#define STFNAME		"stf"

#define IN6_IS_ADDR_6TO4(x)	(ntohs((x)->s6_addr16[0]) == 0x2002)

/*
 * XXX: Return a pointer with 16-bit aligned.  Don't cast it to
 * struct in_addr *; use bcopy() instead.
 */
#define GET_V4(x)	((caddr_t)(&(x)->s6_addr16[1]))

struct stf_softc {
	struct ifnet	*sc_ifp;
	union {
		struct route  __sc_ro4;
		struct route_in6 __sc_ro6; /* just for safety */
	} __sc_ro46;
#define sc_ro	__sc_ro46.__sc_ro4
	struct mtx	sc_mtx;
	u_int	sc_fibnum;
	const struct encaptab *encap_cookie;
	u_int	sc_flags;
	eventhandler_tag	sc_ifaddr_event_tag;
	LIST_ENTRY(stf_softc) stf_list;
};
#define STF2IFP(sc)	((sc)->sc_ifp)

static struct mtx stf_mtx;
static MALLOC_DEFINE(M_STF, STFNAME, "6to4 Tunnel Interface");
static VNET_DEFINE(LIST_HEAD(, stf_softc), stf_softc_list);
#define V_stf_softc_list	VNET(stf_softc_list)

#define STF_LOCK_INIT(sc)	mtx_init(&(sc)->sc_mtx, "stf softc",	\
				    NULL, MTX_DEF);
#define STF_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define STF_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define STF_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define STF_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

static const int ip_stf_ttl = 40;

extern  struct domain inetdomain;
struct protosw in_stf_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IPV6,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		in_stf_input,
	.pr_output =		(pr_output_t *)rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};

static int stfmodevent(module_t, int, void *);
static int stf_encapcheck(const struct mbuf *, int, int, void *);
static struct in6_ifaddr *stf_getsrcifa6(struct ifnet *);
static int stf_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct route *);
static int isrfc1918addr(struct in_addr *);
static int stf_checkaddr4(struct stf_softc *, struct in_addr *,
	struct ifnet *);
static int stf_checkaddr6(struct stf_softc *, struct in6_addr *,
	struct ifnet *);
static void stf_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static int stf_ioctl(struct ifnet *, u_long, caddr_t);
static int stf_is_up(struct ifnet *);

#define STF_GETIN4_USE_CACHE	1
static struct sockaddr_in *stf_getin4addr(struct sockaddr_in *,
					  struct ifaddr *,
					  int);
static struct sockaddr_in *stf_getin4addr_in6(struct sockaddr_in *,
					      struct ifaddr *,
					      struct in6_addr *);
static struct sockaddr_in *stf_getin4addr_sin6(struct sockaddr_in *,
					       struct ifaddr *,
					       struct sockaddr_in6 *);
static void stf_ifaddr_change(void *, struct ifnet *);

static int stf_clone_create(struct if_clone *, int, caddr_t);
static void stf_clone_destroy(struct ifnet *);

IFC_SIMPLE_DECLARE(stf, 0);

static int
stf_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct stf_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(struct stf_softc), M_STF, M_WAITOK | M_ZERO);
	sc->sc_fibnum = curthread->td_proc->p_fibnum;
	ifp = STF2IFP(sc) = if_alloc(IFT_STF);
	if (sc->sc_ifp == NULL) {
		free(sc, M_STF);
		return (ENOMEM);
	}
	STF_LOCK_INIT(sc);
	ifp->if_softc = sc;
	if_initname(ifp, ifc->ifc_name, unit);

	sc->encap_cookie = encap_attach_func(AF_INET, IPPROTO_IPV6,
	    stf_encapcheck, &in_stf_protosw, sc);
	if (sc->encap_cookie == NULL) {
		if_printf(ifp, "attach failed\n");
		free(sc, M_STF);
		return (ENOMEM);
	}

	ifp->if_mtu    = IPV6_MMTU;
	ifp->if_ioctl  = stf_ioctl;
	ifp->if_output = stf_output;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));

	mtx_lock(&stf_mtx);
	LIST_INSERT_HEAD(&V_stf_softc_list, sc, stf_list);
	mtx_unlock(&stf_mtx);

	sc->sc_ifaddr_event_tag =
	    EVENTHANDLER_REGISTER(ifaddr_event, stf_ifaddr_change, NULL,
		EVENTHANDLER_PRI_ANY);

	return (0);
}

static void
stf_clone_destroy(struct ifnet *ifp)
{
	struct stf_softc *sc = ifp->if_softc;
	int err;

	mtx_lock(&stf_mtx);
	LIST_REMOVE(sc, stf_list);
	mtx_unlock(&stf_mtx);

	EVENTHANDLER_DEREGISTER(ifaddr_event, sc->sc_ifaddr_event_tag);

	err = encap_detach(sc->encap_cookie);
	KASSERT(err == 0, ("Unexpected error detaching encap_cookie"));
	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);

	STF_LOCK_DESTROY(sc);
	free(sc, M_STF);

	return;
}

static void
vnet_stf_init(const void *unused __unused)
{

	LIST_INIT(&V_stf_softc_list);
}
VNET_SYSINIT(vnet_stf_init, SI_SUB_PSEUDO, SI_ORDER_MIDDLE, vnet_stf_init,
    NULL);

static int
stfmodevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&stf_mtx, "stf_mtx", NULL, MTX_DEF);
		if_clone_attach(&stf_cloner);
		break;
	case MOD_UNLOAD:
		if_clone_detach(&stf_cloner);
		mtx_destroy(&stf_mtx);
		break;
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t stf_mod = {
	"if_stf",
	stfmodevent,
	0
};

DECLARE_MODULE(if_stf, stf_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_stf, 1);

static int
stf_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	struct ip ip;
	struct in6_ifaddr *ia6;
	struct sockaddr_in ia6_in4addr;
	struct sockaddr_in ia6_in4mask;
	struct sockaddr_in *sin;
	struct stf_softc *sc;
	struct ifnet *ifp;
	int ret = 0;

	DEBUG_PRINTF(1, "%s: enter\n", __func__);
	sc = (struct stf_softc *)arg;
	if (sc == NULL)
		return 0;
	ifp = STF2IFP(sc);

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	/* IFF_LINK0 means "no decapsulation" */
	if ((ifp->if_flags & IFF_LINK0) != 0)
		return 0;

	if (proto != IPPROTO_IPV6)
		return 0;

	/* LINTED const cast */
	m_copydata((struct mbuf *)(uintptr_t)m, 0, sizeof(ip), (caddr_t)&ip);

	if (ip.ip_v != 4)
		return 0;

	/* Lookup an ia6 whose IPv4 addr encoded in the IPv6 addr is valid. */
	ia6 = stf_getsrcifa6(ifp);
	if (ia6 == NULL)
		return 0;
	sin = stf_getin4addr(&ia6_in4addr, &ia6->ia_ifa, STF_GETIN4_USE_CACHE);
	if (sin == NULL)
		return 0;

#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip6_sprintf(buf, &satosin6(ia6->ia_ifa.ifa_addr)->sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6->ia_ifa.ifa_addr = %s\n", __func__, buf);
		ip6_sprintf(buf, &ia6->ia_addr.sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6->ia_addr = %s\n", __func__, buf);
		ip6_sprintf(buf, &satosin6(ia6->ia_ifa.ifa_netmask)->sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6->ia_ifa.ifa_netmask = %s\n", __func__, buf);
		ip6_sprintf(buf, &ia6->ia_prefixmask.sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6->ia_prefixmask = %s\n", __func__, buf);

		ip_sprintf(buf, &ia6_in4addr.sin_addr);
		DEBUG_PRINTF(1, "%s: ia6_in4addr.sin_addr = %s\n", __func__, buf);
		ip_sprintf(buf, &ip.ip_src);
		DEBUG_PRINTF(1, "%s: ip.ip_src = %s\n", __func__, buf);
		ip_sprintf(buf, &ip.ip_dst);
		DEBUG_PRINTF(1, "%s: ip.ip_dst = %s\n", __func__, buf);
	}
#endif
	/*
	 * check if IPv4 dst matches the IPv4 address derived from the
	 * local 6to4 address.
	 * success on: dst = 10.1.1.1, ia6->ia_addr = 2002:0a01:0101:...
	 */
	DEBUG_PRINTF(1, "%s: check1: ia6_in4addr.sin_addr == ip.ip_dst?\n", __func__);
	if (ia6_in4addr.sin_addr.s_addr != ip.ip_dst.s_addr) {
		DEBUG_PRINTF(1, "%s: check1: false.  Ignore this packet.\n", __func__);
		goto freeit;
	}

	DEBUG_PRINTF(1, "%s: check2: ia6->ia_addr is 2002::/16?\n", __func__);
	if (IN6_IS_ADDR_6TO4(&ia6->ia_addr.sin6_addr)) {
		/* 6to4 (RFC 3056) */
		/*
		 * check if IPv4 src matches the IPv4 address derived
		 * from the local 6to4 address masked by prefixmask.
		 * success on: src = 10.1.1.1, ia6->ia_addr = 2002:0a00:.../24
		 * fail on: src = 10.1.1.1, ia6->ia_addr = 2002:0b00:.../24
		 */
		DEBUG_PRINTF(1, "%s: check2: true.\n", __func__);

		memcpy(&ia6_in4mask.sin_addr,
		       GET_V4(&ia6->ia_prefixmask.sin6_addr),
		       sizeof(ia6_in4mask));
#if STF_DEBUG
		{
			char buf[INET6_ADDRSTRLEN + 1];
			memset(&buf, 0, sizeof(buf));

			ip_sprintf(buf, &ia6_in4addr.sin_addr);
			DEBUG_PRINTF(1, "%s: ia6->ia_addr = %s\n",
				     __func__, buf);
			ip_sprintf(buf, &ip.ip_src);
			DEBUG_PRINTF(1, "%s: ip.ip_src = %s\n",
				     __func__, buf);
			ip_sprintf(buf, &ia6_in4mask.sin_addr);
			DEBUG_PRINTF(1, "%s: ia6->ia_prefixmask = %s\n",
				     __func__, buf);

			DEBUG_PRINTF(1, "%s: check3: ia6_in4addr.sin_addr & mask == ip.ip_src & mask\n",
				     __func__);
		}
#endif

		if ((ia6_in4addr.sin_addr.s_addr & ia6_in4mask.sin_addr.s_addr) !=
		    (ip.ip_src.s_addr & ia6_in4mask.sin_addr.s_addr)) {
			DEBUG_PRINTF(1, "%s: check3: false.  Ignore this packet.\n",
				     __func__);
			goto freeit;
		}
	} else {
		/* 6rd (RFC 5569) */
		DEBUG_PRINTF(1, "%s: check2: false.  6rd.\n", __func__);
		/*
		 * No restriction on the src address in the case of
		 * 6rd because the stf(4) interface always has a
		 * prefix which covers whole of IPv4 src address
		 * range.  So, stf_output() will catch all of
		 * 6rd-capsuled IPv4 traffic with suspicious inner dst
		 * IPv4 address (i.e. the IPv6 destination address is
		 * one the admin does not like to route to outside),
		 * and then it discard them silently.
		 */
	}
	DEBUG_PRINTF(1, "%s: all clear!\n", __func__);
	/* stf interface makes single side match only */
	ret = 32;
freeit:
	ifa_free(&ia6->ia_ifa);

	return (ret);
}

static struct in6_ifaddr *
stf_getsrcifa6(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct in_ifaddr *ia4;
	struct sockaddr_in *sin;
	struct sockaddr_in in4;

	if_addr_rlock(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if ((sin = stf_getin4addr(&in4, ifa,
					  STF_GETIN4_USE_CACHE)) == NULL)
			continue;
		LIST_FOREACH(ia4, INADDR_HASH(sin->sin_addr.s_addr), ia_hash)
			if (ia4->ia_addr.sin_addr.s_addr == sin->sin_addr.s_addr)
				break;
		if (ia4 == NULL)
			continue;

#if STF_DEBUG
		{
			char buf[INET6_ADDRSTRLEN + 1];
			memset(&buf, 0, sizeof(buf));

			ip6_sprintf(buf, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr);
			DEBUG_PRINTF(1, "%s: ifa->ifa_addr->sin6_addr = %s\n",
				     __func__, buf);
			ip_sprintf(buf, &ia4->ia_addr.sin_addr);
			DEBUG_PRINTF(1, "%s: ia4->ia_addr.sin_addr = %s\n",
				     __func__, buf);
		}
#endif
		ifa_ref(ifa);
		if_addr_runlock(ifp);
		return (ifatoia6(ifa));
	}
	if_addr_runlock(ifp);


	return NULL;
}

static int
stf_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst, struct route *ro)
{
	struct stf_softc *sc;
	struct sockaddr_in6 *dst6;
	struct route *cached_route;
	struct sockaddr_in *sin;
	struct sockaddr_in in4;
	struct sockaddr_in *dst4;
	u_int8_t tos;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct in6_ifaddr *ia6;
	u_int32_t af;
	int error;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		return (error);
	}
#endif

	sc = ifp->if_softc;
	dst6 = (struct sockaddr_in6 *)dst;

	/* just in case */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		ifp->if_oerrors++;
		return ENETDOWN;
	}

	/*
	 * If we don't have an ip4 address that match my inner ip6 address,
	 * we shouldn't generate output.  Without this check, we'll end up
	 * using wrong IPv4 source.
	 */
	ia6 = stf_getsrcifa6(ifp);
	if (ia6 == NULL) {
		m_freem(m);
		ifp->if_oerrors++;
		return ENETDOWN;
	}

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m) {
			ifa_free(&ia6->ia_ifa);
			ifp->if_oerrors++;
			return ENOBUFS;
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);
	tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;

	/*
	 * BPF writes need to be handled specially.
	 * This is a null operation, nothing here checks dst->sa_family.
	 */
	if (dst->sa_family == AF_UNSPEC) {
		bcopy(dst->sa_data, &af, sizeof(af));
		dst->sa_family = af;
	}

	/*
	 * Pickup the right outer dst addr from the list of candidates.
	 * ip6_dst has priority as it may be able to give us shorter IPv4 hops.
	 *   ip6_dst: destination addr in the packet header.
	 *   dst6: destination addr specified in function argument.
	 */
	DEBUG_PRINTF(1, "%s: dst addr selection\n", __func__);
	sin = stf_getin4addr_in6(&in4, &ia6->ia_ifa, &ip6->ip6_dst);
	if (sin == NULL)
		sin = stf_getin4addr_in6(&in4, &ia6->ia_ifa, &dst6->sin6_addr);
	if (sin == NULL) {
		ifa_free(&ia6->ia_ifa);
		m_freem(m);
		ifp->if_oerrors++;
		return ENETUNREACH;
	}
#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip_sprintf(buf, &sin->sin_addr);
		DEBUG_PRINTF(1, "%s: ip_dst = %s\n", __func__, buf);
	}
#endif
	if (bpf_peers_present(ifp->if_bpf)) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer a to it).
		 */
		af = AF_INET6;
		bpf_mtap2(ifp->if_bpf, &af, sizeof(af), m);
	}

	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL) {
		ifa_free(&ia6->ia_ifa);
		ifp->if_oerrors++;
		return ENOBUFS;
	}
	ip = mtod(m, struct ip *);

	bzero(ip, sizeof(*ip));
	bcopy(&in4.sin_addr, &ip->ip_dst, sizeof(ip->ip_dst));

	sin = stf_getin4addr_sin6(&in4, &ia6->ia_ifa, &ia6->ia_addr);
	if (sin == NULL) {
		ifa_free(&ia6->ia_ifa);
		m_freem(m);
		ifp->if_oerrors++;
		return ENETUNREACH;
	}
	bcopy(&in4.sin_addr, &ip->ip_src, sizeof(ip->ip_src));
#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip_sprintf(buf, &ip->ip_src);
		DEBUG_PRINTF(1, "%s: ip_src = %s\n", __func__, buf);
	}
#endif
	ifa_free(&ia6->ia_ifa);
	ip->ip_p = IPPROTO_IPV6;
	ip->ip_ttl = ip_stf_ttl;
	ip->ip_len = m->m_pkthdr.len;	/*host order*/
	if (ifp->if_flags & IFF_LINK1)
		ip_ecn_ingress(ECN_ALLOWED, &ip->ip_tos, &tos);
	else
		ip_ecn_ingress(ECN_NOCARE, &ip->ip_tos, &tos);

	if (!V_stf_route_cache) {
		cached_route = NULL;
		goto sendit;
	}

	/*
	 * Do we have a cached route?
	 */
	STF_LOCK(sc);
	dst4 = (struct sockaddr_in *)&sc->sc_ro.ro_dst;
	if (dst4->sin_family != AF_INET ||
	    bcmp(&dst4->sin_addr, &ip->ip_dst, sizeof(ip->ip_dst)) != 0) {
		/* cache route doesn't match */
		dst4->sin_family = AF_INET;
		dst4->sin_len = sizeof(struct sockaddr_in);
		bcopy(&ip->ip_dst, &dst4->sin_addr, sizeof(dst4->sin_addr));
		if (sc->sc_ro.ro_rt) {
			RTFREE(sc->sc_ro.ro_rt);
			sc->sc_ro.ro_rt = NULL;
		}
	}

	if (sc->sc_ro.ro_rt == NULL) {
		rtalloc_fib(&sc->sc_ro, sc->sc_fibnum);
		if (sc->sc_ro.ro_rt == NULL) {
			m_freem(m);
			ifp->if_oerrors++;
			STF_UNLOCK(sc);
			return ENETUNREACH;
		}
		if (sc->sc_ro.ro_rt->rt_ifp == ifp) {
			/* infinite loop detection */
			m_free(m);
			ifp->if_oerrors++;
			STF_UNLOCK(sc);
			return ENETUNREACH;
		}
	}
	cached_route = &sc->sc_ro;

sendit:
	M_SETFIB(m, sc->sc_fibnum);
	ifp->if_opackets++;
	DEBUG_PRINTF(1, "%s: ip_output dispatch.\n", __func__);
	error = ip_output(m, NULL, cached_route, 0, NULL, NULL);

	if (cached_route != NULL)
		STF_UNLOCK(sc);
	return error;
}

static int
isrfc1918addr(struct in_addr *in)
{
	/*
	 * returns 1 if private address range:
	 * 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16
	 */
	if ((ntohl(in->s_addr) & 0xff000000) == 10 << 24 ||
	    (ntohl(in->s_addr) & 0xfff00000) == (172 * 256 + 16) << 16 ||
	    (ntohl(in->s_addr) & 0xffff0000) == (192 * 256 + 168) << 16 )
		return 1;

	return 0;
}

static int
stf_checkaddr4(struct stf_softc *sc, struct in_addr *in, struct ifnet *inifp)
{
	struct in_ifaddr *ia4;

	/*
	 * reject packets with the following address:
	 * 224.0.0.0/4 0.0.0.0/8 127.0.0.0/8 255.0.0.0/8
	 */
	if (IN_MULTICAST(ntohl(in->s_addr)))
		return -1;
	switch ((ntohl(in->s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return -1;
	}

	/*
	 * reject packets with broadcast
	 */
	IN_IFADDR_RLOCK();
	TAILQ_FOREACH(ia4, &V_in_ifaddrhead, ia_link) {
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (in->s_addr == ia4->ia_broadaddr.sin_addr.s_addr) {
			IN_IFADDR_RUNLOCK();
			return -1;
		}
	}
	IN_IFADDR_RUNLOCK();

	/*
	 * perform ingress filter
	 */
	if (sc && (STF2IFP(sc)->if_flags & IFF_LINK2) == 0 && inifp) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr = *in;
		rt = rtalloc1_fib((struct sockaddr *)&sin, 0,
		    0UL, sc->sc_fibnum);
		if (!rt || rt->rt_ifp != inifp) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(STF2IFP(sc)),
			    (u_int32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt)
				RTFREE_LOCKED(rt);
			return -1;
		}
		RTFREE_LOCKED(rt);
	}

	return 0;
}

static int
stf_checkaddr6(struct stf_softc *sc, struct in6_addr *in6, struct ifnet *inifp)
{
	/*
	 * check 6to4 addresses
	 */
	if (IN6_IS_ADDR_6TO4(in6)) {
		struct in_addr in4;
		bcopy(GET_V4(in6), &in4, sizeof(in4));
		return stf_checkaddr4(sc, &in4, inifp);
	}

	/*
	 * reject anything that look suspicious.  the test is implemented
	 * in ip6_input too, but we check here as well to
	 * (1) reject bad packets earlier, and
	 * (2) to be safe against future ip6_input change.
	 */
	if (IN6_IS_ADDR_V4COMPAT(in6) || IN6_IS_ADDR_V4MAPPED(in6))
		return -1;

	return 0;
}

void
in_stf_input(struct mbuf *m, int off)
{
	int proto;
	struct stf_softc *sc;
	struct ip *ip;
	struct ip6_hdr *ip6;
	u_int8_t otos, itos;
	struct ifnet *ifp;
	struct route_in6 rin6;

	proto = mtod(m, struct ip *)->ip_p;

	if (proto != IPPROTO_IPV6) {
		m_freem(m);
		return;
	}

	ip = mtod(m, struct ip *);

	sc = (struct stf_softc *)encap_getarg(m);

	if (sc == NULL || (STF2IFP(sc)->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	ifp = STF2IFP(sc);

#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif

#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip_sprintf(buf, &ip->ip_dst);
		DEBUG_PRINTF(1, "%s: ip->ip_dst = %s\n", __func__, buf);
		ip_sprintf(buf, &ip->ip_src);
		DEBUG_PRINTF(1, "%s: ip->ip_src = %s\n", __func__, buf);
	}
#endif
	/*
	 * perform sanity check against outer src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr4(sc, &ip->ip_dst, NULL) < 0 ||
	    stf_checkaddr4(sc, &ip->ip_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	otos = ip->ip_tos;
	m_adj(m, off);

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m)
			return;
	}
	ip6 = mtod(m, struct ip6_hdr *);

#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip6_sprintf(buf, &ip6->ip6_dst);
		DEBUG_PRINTF(1, "%s: ip6->ip6_dst = %s\n", __func__, buf);
		ip6_sprintf(buf, &ip6->ip6_src);
		DEBUG_PRINTF(1, "%s: ip6->ip6_src = %s\n", __func__, buf);
	}
#endif
	/*
	 * perform sanity check against inner src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr6(sc, &ip6->ip6_dst, NULL) < 0 ||
	    stf_checkaddr6(sc, &ip6->ip6_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	/*
	 * reject packets with private address range.
	 * (requirement from RFC3056 section 2 1st paragraph)
	 */
	if ((IN6_IS_ADDR_6TO4(&ip6->ip6_src) && isrfc1918addr(&ip->ip_src)) ||
	    (IN6_IS_ADDR_6TO4(&ip6->ip6_dst) && isrfc1918addr(&ip->ip_dst))) {
		m_freem(m);
		return;
	}

	/*
	 * Ignore if the destination is the same stf interface because
	 * all of valid IPv6 outgoing traffic should go interfaces
	 * except for it.
	 */
	memset(&rin6, 0, sizeof(rin6));
	rin6.ro_dst.sin6_len = sizeof(rin6.ro_dst);
	rin6.ro_dst.sin6_family = AF_INET6;
	memcpy(&rin6.ro_dst.sin6_addr, &ip6->ip6_dst,
	       sizeof(rin6.ro_dst.sin6_addr));
	rtalloc((struct route *)&rin6);
	if (rin6.ro_rt == NULL) {
		DEBUG_PRINTF(1, "%s: no IPv6 dst.  Ignored.\n", __func__);
		m_free(m);
		return;
	}
	if ((rin6.ro_rt->rt_ifp == ifp) &&
	    (!IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &rin6.ro_dst.sin6_addr))) {
		DEBUG_PRINTF(1, "%s: IPv6 dst is the same stf.  Ignored.\n", __func__);
		RTFREE(rin6.ro_rt);
		m_free(m);
		return;
	}
	RTFREE(rin6.ro_rt);

	itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
	if ((ifp->if_flags & IFF_LINK1) != 0)
		ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
	else
		ip_ecn_egress(ECN_NOCARE, &otos, &itos);
	ip6->ip6_flow &= ~htonl(0xff << 20);
	ip6->ip6_flow |= htonl((u_int32_t)itos << 20);

	m->m_pkthdr.rcvif = ifp;

	if (bpf_peers_present(ifp->if_bpf)) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer a to it).
		 */
		u_int32_t af = AF_INET6;
		bpf_mtap2(ifp->if_bpf, &af, sizeof(af), m);
	}

	DEBUG_PRINTF(1, "%s: netisr_dispatch(NETISR_IPV6)\n", __func__);
	/*
	 * Put the packet to the network layer input queue according to the
	 * specified address family.
	 * See net/if_gif.c for possible issues with packet processing
	 * reorder due to extra queueing.
	 */
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_dispatch(NETISR_IPV6, m);
}

/* ARGSUSED */
static void
stf_rtrequest(int cmd, struct rtentry *rt, struct rt_addrinfo *info)
{

	RT_LOCK_ASSERT(rt);
	rt->rt_rmx.rmx_mtu = IPV6_MMTU;
}

/* Check whether we have at least one instance with IFF_UP. */
static int
stf_is_up(struct ifnet *ifp)
{
	struct stf_softc *scp;
	struct stf_softc *sc_cur;
	struct stf_softc *sc_is_up;

	sc_is_up = NULL;
	if ((sc_cur = ifp->if_softc) == NULL)
		return (EINVAL);

	mtx_lock(&stf_mtx);
	LIST_FOREACH(scp, &V_stf_softc_list, stf_list) {
		if (scp == sc_cur)
			continue;
		if ((STF2IFP(scp)->if_flags & IFF_UP) != 0) {
			sc_is_up = scp;
			break;
		}
	}
	mtx_unlock(&stf_mtx);

	/* We already has at least one instance with IFF_UP. */
	if (stf_is_up != NULL)
		return (ENOSPC);

	return (0);
}

static struct sockaddr_in *
stf_getin4addr_in6(struct sockaddr_in *sin,
		   struct ifaddr *ifa,
		   struct in6_addr *in6)
{
	struct sockaddr_in6 sin6;

	DEBUG_PRINTF(1, "%s: enter.\n", __func__);
	if (ifa == NULL || in6 == NULL)
		return NULL;

	memset(&sin6, 0, sizeof(sin6));
	memcpy(&sin6.sin6_addr, in6, sizeof(sin6.sin6_addr));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;

	return(stf_getin4addr_sin6(sin, ifa, &sin6));
}

static struct sockaddr_in *
stf_getin4addr_sin6(struct sockaddr_in *sin,
		    struct ifaddr *ifa,
		    struct sockaddr_in6 *sin6)
{
	struct in6_ifaddr ia6;
	int i;

	DEBUG_PRINTF(1, "%s: enter.\n", __func__);
	if (ifa == NULL || sin6 == NULL)
		return NULL;

	memset(&ia6, 0, sizeof(ia6));
	memcpy(&ia6, ifatoia6(ifa), sizeof(ia6));

	/*
	 * Use prefixmask information from ifa, and
	 * address information from sin6.
	 */
	ia6.ia_addr.sin6_family = AF_INET6;
	ia6.ia_ifa.ifa_addr = (struct sockaddr *)&ia6.ia_addr;
	ia6.ia_ifa.ifa_dstaddr = NULL;
	ia6.ia_ifa.ifa_netmask = (struct sockaddr *)&ia6.ia_prefixmask;

#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip6_sprintf(buf, &sin6->sin6_addr);
		DEBUG_PRINTF(1, "%s: sin6->sin6_addr = %s\n", __func__, buf);
		ip6_sprintf(buf, &ia6.ia_addr.sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6.ia_addr.sin6_addr = %s\n", __func__, buf);
		ip6_sprintf(buf, &ia6.ia_prefixmask.sin6_addr);
		DEBUG_PRINTF(1, "%s: ia6.ia_prefixmask.sin6_addr = %s\n", __func__, buf);
	}
#endif

	/*
	 * When (src addr & src mask) != (dst (sin6) addr & src mask),
	 * the dst is not in the 6rd domain.  The IPv4 address must
	 * not be used.
	 */
	for (i = 0; i < sizeof(ia6.ia_addr.sin6_addr); i++) {
		if ((((u_char *)&ia6.ia_addr.sin6_addr)[i] &
		     ((u_char *)&ia6.ia_prefixmask.sin6_addr)[i])
		    !=
		    (((u_char *)&sin6->sin6_addr)[i] &
		     ((u_char *)&ia6.ia_prefixmask.sin6_addr)[i]))
			return NULL;
	}

	/* After the mask check, overwrite ia6.ia_addr with sin6. */
	memcpy(&ia6.ia_addr, sin6, sizeof(ia6.ia_addr));
	return(stf_getin4addr(sin, (struct ifaddr *)&ia6, 0));
}

static struct sockaddr_in *
stf_getin4addr(struct sockaddr_in *sin,
	       struct ifaddr *ifa,
	       int flags)
{
	struct in_addr *in;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in6 *sin6d;
	struct in6_ifaddr *ia6;

	DEBUG_PRINTF(1, "%s: enter.\n", __func__);
	if (ifa == NULL ||
	    ifa->ifa_addr == NULL ||
	    ifa->ifa_addr->sa_family != AF_INET6)
		return NULL;

	sin6 = satosin6(ifa->ifa_addr);
	ia6 = ifatoia6(ifa);

	if (ifa->ifa_dstaddr != NULL) {
		switch (ifa->ifa_dstaddr->sa_family) {
		case AF_INET6:
			sin6d = satosin6(ifa->ifa_dstaddr);
			if (IN6_IS_ADDR_UNSPECIFIED(&sin6d->sin6_addr))
				break;
			if (IN6_IS_ADDR_V4COMPAT(&sin6d->sin6_addr)) {
				memset(sin, 0, sizeof(*sin));
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr = 
					*(const u_int32_t *)(const void *)(&sin6d->sin6_addr.s6_addr[12]);
				if (flags & STF_GETIN4_USE_CACHE) {
					/*
					 * XXX: ifa_dstaddr is used as a cache of the
					 * extracted IPv4 address.
					 */
					memcpy(sin, satosin(ifa->ifa_dstaddr), sizeof(*sin));
					ifa->ifa_dstaddr->sa_family = AF_INET;
				}
#if STF_DEBUG
				{
					char buf[INET6_ADDRSTRLEN + 1];
					memset(&buf, 0, sizeof(buf));
					
					ip_sprintf(buf, &sin->sin_addr);
					DEBUG_PRINTF(1, "%s: specified dst address was used = %s\n", __func__, buf);
				}
#endif
				return (sin);
			} else {
				DEBUG_PRINTF(1, "Not a V4COMPAT address!\n");
				return (NULL);
			}
			/* NOT REACHED */
			break;
		case AF_INET:
			if (flags & STF_GETIN4_USE_CACHE) {
				/*
				 * XXX: ifa_dstaddr is used as a cache of the
				 * extracted IPv4 address.
				 */
				memcpy(sin, satosin(ifa->ifa_dstaddr), sizeof(*sin));
				ifa->ifa_dstaddr->sa_family = AF_INET;
#if STF_DEBUG
				{
					char buf[INET6_ADDRSTRLEN + 1];
					memset(&buf, 0, sizeof(buf));
					
					ip_sprintf(buf, &sin->sin_addr);
					DEBUG_PRINTF(1, "%s: cached address was used = %s\n", __func__, buf);
				}
#endif
				return (sin);
			}
		}
	}
	memset(sin, 0, sizeof(*sin));
	in = &sin->sin_addr;

#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip6_sprintf(buf, &sin6->sin6_addr);
		DEBUG_PRINTF(1, "%s: sin6->sin6_addr = %s\n", __func__, buf);
	}
#endif

	if (IN6_IS_ADDR_6TO4(&sin6->sin6_addr)) {
		/* 6to4 (RFC 3056) */
		bcopy(GET_V4(&sin6->sin6_addr), in, sizeof(*in));
		if (isrfc1918addr(in))
			return NULL;
	} else {
		/* 6rd (RFC 5569) */
		struct in6_addr buf;
		u_char *p = (u_char *)&buf;
		u_char *q = (u_char *)in;
		u_int residue = 0;
		u_char mask;
		int i;
		u_int plen;

		/*
		 * 6rd-relays IPv6 prefix is located at a 32-bit just
		 * after the prefix edge.
		 */
		plen = in6_mask2len(&satosin6(ifa->ifa_netmask)->sin6_addr, NULL);
		if (32 < plen)
			return NULL;

		memcpy(&buf, &sin6->sin6_addr, sizeof(buf));
		p += plen / 8;
		residue = plen % 8;
		mask = ~((u_char)(-1) >> residue);

		/*
		 * The p points head of the IPv4 address part in
		 * bytes.  The residue is a bit-shift factor when
		 * prefixlen is not a multiple of 8.
		 */
		for (i = 0; i < 4; i++) {
			DEBUG_PRINTF(2, "p[%d] = %d\n", i, p[i]);
			DEBUG_PRINTF(2, "residue = %d\n", residue);
			if (residue) {
				p[i] <<= residue;
				DEBUG_PRINTF(2, "p[%d] << residue = %d\n",
					     i, p[i]);
				DEBUG_PRINTF(2, "mask = %x\n",
					     mask);
				DEBUG_PRINTF(2, "p[%d + 1] & mask = %d\n",
					     i, p[i + 1] & mask);
				DEBUG_PRINTF(2, "p[%d + 1] & mask >> (8 - residue) = %d\n",
					     i, (p[i + 1] & mask) >> (8-residue));
				p[i] |= ((p[i+1] & mask) >> (8 - residue));
			}
			q[i] = p[i];
		}
	}
#if STF_DEBUG
	{
		char buf[INET6_ADDRSTRLEN + 1];
		memset(&buf, 0, sizeof(buf));

		ip_sprintf(buf, in);
		DEBUG_PRINTF(1, "%s: in->in_addr = %s\n", __func__, buf);
		DEBUG_PRINTF(1, "%s: leave\n", __func__);
	}
#endif
	if (flags & STF_GETIN4_USE_CACHE) {
		DEBUG_PRINTF(1, "%s: try to memset 0 to ia_dstaddr.\n", __func__);
		memset(&ia6->ia_dstaddr, 0, sizeof(ia6->ia_dstaddr));
		DEBUG_PRINTF(1, "%s: try to access ifa->ifa_dstaddr.\n", __func__);
		ifa->ifa_dstaddr = (struct sockaddr *)&ia6->ia_dstaddr;
		DEBUG_PRINTF(1, "%s: try to memcpy ifa->ifa_dstaddr.\n", __func__);
		memcpy((struct sockaddr_in *)ifa->ifa_dstaddr,
		       sin, sizeof(struct sockaddr_in));
		DEBUG_PRINTF(1, "%s: try to set sa_family.\n", __func__);
		ifa->ifa_dstaddr->sa_family = AF_INET;
		DEBUG_PRINTF(1, "%s: in->in_addr is stored in ifa_dstaddr.\n",
			     __func__);
	}
	return (sin);
}

static void
stf_ifaddr_change(void *arg __unused, struct ifnet *ifp)
{
	struct sockaddr_in in4;
	struct ifaddr *ifa;

	DEBUG_PRINTF(1, "%s: enter.\n", __func__);

	if_addr_rlock(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr == NULL)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (ifa->ifa_dstaddr != NULL) {
			DEBUG_PRINTF(1, "%s: ifa->ifa_dstaddr != NULL!.\n", __func__);

#if STF_DEBUG
			{
				char buf[INET6_ADDRSTRLEN + 1];
				memset(&buf, 0, sizeof(buf));
				
				ip6_sprintf(buf, &satosin6(ifa->ifa_addr)->sin6_addr);
				DEBUG_PRINTF(1, "%s: ifa_addr = %s\n", __func__, buf);
			}
#endif
			switch (ifa->ifa_dstaddr->sa_family) {
			case AF_INET:
#if STF_DEBUG
			{
				char buf[INET6_ADDRSTRLEN + 1];
				memset(&buf, 0, sizeof(buf));
				
				ip_sprintf(buf, &satosin(ifa->ifa_dstaddr)->sin_addr);
				DEBUG_PRINTF(1, "%s: ifa_dstaddr = %s\n", __func__, buf);
			}
#endif
				continue;
			case AF_INET6:
#if STF_DEBUG
			{
				char buf[INET6_ADDRSTRLEN + 1];
				memset(&buf, 0, sizeof(buf));
				
				ip6_sprintf(buf, &satosin6(ifa->ifa_dstaddr)->sin6_addr);
				DEBUG_PRINTF(1, "%s: ifa_dstaddr = %s\n", __func__, buf);
			}
#endif
			if (IN6_IS_ADDR_V4COMPAT(&satosin6(ifa->ifa_dstaddr)->sin6_addr)) {
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(&satosin6(ifa->ifa_dstaddr)->sin6_addr))
				continue;
			}
		}
		DEBUG_PRINTF(1, "%s: ifa->ifa_dstaddr == NULL or ::!.\n", __func__);
		/*
		 * Extract IPv4 address from IPv6 address,
		 * then store it into ifa_dstaddr as the
		 * destination.
		 */
		if (stf_getin4addr(&in4, ifa, STF_GETIN4_USE_CACHE) == NULL) {
			ifatoia6(ifa)->ia_flags |=  IN6_IFF_DETACHED;
			continue;
		}
	}
	if_addr_runlock(ifp);
}

static int
stf_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifaddr *ifa;
	struct ifreq *ifr;
/*
	struct in6_aliasreq *ifra6;
*/
	struct in6_aliasreq ifra;
/*
	struct sockaddr_in6 *sa6;
*/
	int error;

	memset(&ifra, 0, sizeof(ifra));
	/*
	 * Sanity check: if more than two interfaces have IFF_UP, do
	 * if_down() for all of them except for the specified one.
	 */
	if (ifp->if_flags & IFF_UP) {
		struct stf_softc *sc_cur = ifp->if_softc;
		struct stf_softc *sc;

		mtx_lock(&stf_mtx);
		LIST_FOREACH(sc, &V_stf_softc_list, stf_list) {
			if (sc == sc_cur)
				continue;
			if ((STF2IFP(sc)->if_flags & IFF_UP) != 0) {
				if_printf(STF2IFP(sc),
					  "marked as DOWN because at least "
					  "one instance of stf(4) is already "
					  "working.\n");
				if_down(STF2IFP(sc));
			}
		}
		mtx_unlock(&stf_mtx);
	}

	error = 0;
	switch (cmd) {
	case SIOCSIFADDR:
		DEBUG_PRINTF(1, "enter SIOCSIFADDR.\n");
		ifa = (struct ifaddr *)data;
		if (ifa == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    ifa->ifa_dstaddr->sa_family == AF_INET &&
		    ifa->ifa_netmask->sa_family == AF_INET6) {
			ifa->ifa_rtrequest = stf_rtrequest;
			ifp->if_flags |= IFF_UP;
		} else {
			error = EINVAL;
			break;
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if (ifr && ifr->ifr_addr.sa_family == AF_INET6)
			;
		else
			error = EAFNOSUPPORT;
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}
