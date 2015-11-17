/*-
 * Copyright (c) 2015
 * 	Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"
#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route_internal.h>
#include <net/vnet.h>

#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/scope6_var.h>

#include <net/if_llatbl.h>

#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <net/rt_nhops.h>

#include <vm/uma.h>

#ifdef INET6
static void fib6_rte_to_nh_extended(struct rtentry *rte, struct in6_addr *dst,
    struct nhop6_extended *pnh6);
static void fib6_rte_to_nh_basic(struct rtentry *rte, const struct in6_addr *dst,
    struct nhop6_basic *pnh6);
static int fib6_storelladdr(struct ifnet *ifp, struct in6_addr *dst,
    int mm_flags, u_char *desten);
static uint16_t fib6_get_ifa(struct rtentry *rte);
static int fib6_lla_to_nh_basic(const struct in6_addr *dst, uint32_t scopeid,
    struct nhop6_basic *pnh6);
static int fib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    struct nhop6_extended *pnh6);
static int fib6_lla_to_nh(struct in6_addr *dst, uint32_t scopeid,
    struct nhop_prepend *nh, struct ifnet **lifp);

#define RNTORT(p)	((struct rtentry *)(p))
void
fib6_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh)
{

	/* TODO: Do some light-weight refcounting on egress ifp's */
	//fib_free_nh_prepend(fibnum, nh, AF_INET6);
}

void
fib6_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop6_extended *nh_ext)
{

	fib_choose_prepend(fibnum, nh_src, flowid, nh, AF_INET6);
	if (nh_ext == NULL)
		return;

	nh_ext->nh_ifp = NH_LIFP(nh);
	nh_ext->nh_mtu = nh->nh_mtu;
	nh_ext->nh_flags = nh->nh_flags;
/*
	nh_ext->nh_addr = ;
	nh_ext->nh_src= ;
*/
}

/*
 * Temporary function to copy ethernet address from valid lle
 */
static int
fib6_storelladdr(struct ifnet *ifp, struct in6_addr *dst, int mm_flags,
    u_char *desten)
{
	struct llentry *ln;
	struct sockaddr_in6 dst_sa;

	if (mm_flags & M_MCAST) {
		ETHER_MAP_IPV6_MULTICAST(&dst, desten);
		return (0);
	}

	memset(&dst_sa, 0, sizeof(dst_sa));
	dst_sa.sin6_family = AF_INET6;
	dst_sa.sin6_len = sizeof(dst_sa);
	dst_sa.sin6_addr = *dst;
	dst_sa.sin6_scope_id = ifp->if_index;
	

	/*
	 * the entry should have been created in nd6_store_lladdr
	 */
	IF_AFDATA_RLOCK(ifp);
	ln = lla_lookup(LLTABLE6(ifp), 0, (struct sockaddr *)&dst_sa);

	/*
	 * Perform fast path for the following cases:
	 * 1) lle state is REACHABLE
	 * 2) lle state is DELAY (NS message sentNS message sent)
	 *
	 * Every other case involves lle modification, so we handle
	 * them separately.
	 */
	if (ln == NULL || (ln->ln_state != ND6_LLINFO_REACHABLE &&
	    ln->ln_state != ND6_LLINFO_DELAY)) {
		if (ln != NULL)
			LLE_RUNLOCK(ln);
		IF_AFDATA_RUNLOCK(ifp);
		return (1);
	}
	bcopy(&ln->ll_addr, desten, ifp->if_addrlen);
	LLE_RUNLOCK(ln);
	IF_AFDATA_RUNLOCK(ifp);

	return (0);
}

int
fib6_lookup_prepend(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    struct mbuf *m, struct nhop_prepend *nh, struct nhop6_extended *nh_ext)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6, *gw_sa;
	struct in6_addr gw6;
	struct rtentry *rte;
	struct ifnet *lifp;
	struct ether_header *eh;
	RIB_LOCK_READER;
	uint32_t flags;
	int error;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		error = fib6_lla_to_nh(dst, scopeid, nh, &lifp);
		if (error != 0)
			return (error);
		/* */
		gw6 = *dst;
		goto do_l2;
	}


	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_prepend: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst;
	sin6.sin6_scope_id = scopeid;
	sa6_embedscope(&sin6, 0);
	

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	rte = RNTORT(rn);
	if (rn == NULL || ((rn->rn_flags & RNF_ROOT) != 0) ||
	    RT_LINK_IS_UP(rte->rt_ifp) == 0) {
		RIB_RUNLOCK(rh);
		return (EHOSTUNREACH);
	}

	/* Explicitly zero nexthop */
	memset(nh, 0, sizeof(*nh));
	flags = 0;
	nh->nh_mtu = min(rte->rt_mtu, IN6_LINKMTU(rte->rt_ifp));
	if (rte->rt_flags & RTF_GATEWAY) {
		gw_sa = (struct sockaddr_in6 *)rte->rt_gateway;
		gw6 = gw_sa->sin6_addr;
		in6_clearscope(&gw6);
	} else
		gw6 = *dst;
	/* Set flags */
	flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw_sa = (struct sockaddr_in6 *)rt_key(rte);
	if (IN6_IS_ADDR_UNSPECIFIED(&gw_sa->sin6_addr))
		flags |= NHF_DEFAULT;

	/*
	 * TODO: nh L2/L3 resolve.
	 * Currently all we have is rte ifp.
	 * Simply use it.
	 */
	/* Save interface address ifp */
	nh->aifp_idx = fib6_get_ifa(rte);
	/* Save both logical and transmit interface indexes */
	lifp = rte->rt_ifp;
	nh->lifp_idx = lifp->if_index;
	nh->i.ifp_idx = nh->lifp_idx;

	RIB_RUNLOCK(rh);

	nh->nh_flags = flags;
do_l2:
	/*
	 * Try to lookup L2 info.
	 * Do this using separate LLE locks.
	 * TODO: move this under radix lock.
	 */
	if (lifp->if_type == IFT_ETHER) {
		eh = (struct ether_header *)nh->d.data;

		/*
		 * Fill in ethernet header.
		 * It should be already presented if we're
		 * sending data via known gateway.
		 */
		error = fib6_storelladdr(lifp, &gw6, m ? m->m_flags : 0,
		    eh->ether_dhost);
		if (error == 0) {
			memcpy(&eh->ether_shost, IF_LLADDR(lifp), ETHER_ADDR_LEN);
			eh->ether_type = htons(ETHERTYPE_IPV6);
			nh->nh_count = ETHER_HDR_LEN;
			return (0);
		}
	}

	/* Notify caller that no L2 info is linked */
	nh->nh_count = 0;
	nh->nh_flags |= NHF_L2_INCOMPLETE;
	/* ..And save gateway address */
	nh->d.gw6 = gw6;
	return (0);
}

int
fib6_sendmbuf(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m,
    struct nhop_prepend *nh)
{
	int error;

	if (nh != NULL && (nh->nh_flags & NHF_L2_INCOMPLETE) == 0) {

		/*
		 * Fast path case. Most packets should
		 * be sent from here.
		 * TODO: Make special ifnet
		 * 'if_output_frame' handler for that.
		 */
		struct nhop_info ni;
		struct ether_header *eh;
		bzero(&ni, sizeof(ni));
		ni.ni_family = AF_INET6;
		ni.ni_flags = RT_NHOP;
		ni.ni_nh = nh;

		M_PREPEND(m, nh->nh_count, M_NOWAIT);
		if (m == NULL)
			return (ENOBUFS);
		eh = mtod(m, struct ether_header *);
		memcpy(eh, nh->d.data, nh->nh_count);
		error = (*ifp->if_output)(ifp, m, NULL, &ni);
	} else {
		/* We need to perform ND lookup */
		struct sockaddr_in6 gw_out;

		memset(&gw_out, 0, sizeof(gw_out));
		gw_out.sin6_family = AF_INET6;
		gw_out.sin6_len = sizeof(gw_out);
		gw_out.sin6_addr = nh->d.gw6;
		gw_out.sin6_scope_id = ifp->if_index;
		sa6_embedscope(&gw_out, 0);

		error = nd6_output_ifp(ifp, origifp, m, &gw_out, NULL);
	}

	return (error);
}

static uint16_t
fib6_get_ifa(struct rtentry *rte)
{
	struct ifnet *ifp;
	struct sockaddr_dl *sdl;

	ifp = rte->rt_ifp;
	if ((ifp->if_flags & IFF_LOOPBACK) &&
	    rte->rt_gateway->sa_family == AF_LINK) {
		sdl = (struct sockaddr_dl *)rte->rt_gateway;
		return (sdl->sdl_index);
	}

	return (ifp->if_index);
#if 0
	/* IPv6 case */
	/* Alternative way to get interface address ifp */
	/*
	 * Adjust the "outgoing" interface.  If we're going to loop 
	 * the packet back to ourselves, the ifp would be the loopback 
	 * interface. However, we'd rather know the interface associated 
	 * to the destination address (which should probably be one of 
	 * our own addresses.)
	 */
	if (rt) {
		if ((rt->rt_ifp->if_flags & IFF_LOOPBACK) &&
		    (rt->rt_gateway->sa_family == AF_LINK))
			*retifp = 
				ifnet_byindex(((struct sockaddr_dl *)
					       rt->rt_gateway)->sdl_index);
	}
	/* IPv4 case */
	//pnh6->nh_ifp = rte->rt_ifa->ifa_ifp;
#endif
}

static int
fib6_lla_to_nh_basic(const struct in6_addr *dst, uint32_t scopeid,
    struct nhop6_basic *pnh6)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_ifp = ifp;
	pnh6->nh_mtu = IN6_LINKMTU(ifp);
	/* No flags set */
	pnh6->nh_addr = *dst;

	return (0);
}

static int
fib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    struct nhop6_extended *pnh6)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_ifp = ifp;
	pnh6->nh_mtu = IN6_LINKMTU(ifp);
	/* No flags set */
	pnh6->nh_addr = *dst;

	return (0);
}

static int
rib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    struct rt6_extended *prt6)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(prt6, 0, sizeof(*prt6));

	prt6->rt_addr.s6_addr16[0] = htons(0xFE80);
	prt6->rt_mask = 64; /* XXX check RFC */

	prt6->rt_aifp = ifp;
	prt6->rt_lifp = ifp;
	/* Check id this is for-us address */
	if (in6_ifawithifp_lla(ifp, dst)) {
		if ((ifp = V_loif) != NULL)
			prt6->rt_lifp = ifp;
	}

	prt6->rt_mtu = IN6_LINKMTU(ifp);
	/* No flags set */

	return (0);
}

static int
fib6_lla_to_nh(struct in6_addr *dst, uint32_t scopeid,
    struct nhop_prepend *nh, struct ifnet **lifp)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(nh, 0, sizeof(*nh));
	/* No flags set */
	nh->nh_mtu = IN6_LINKMTU(ifp);

	/* Save lifp */
	*lifp = ifp;

	nh->aifp_idx = scopeid;
	nh->lifp_idx = scopeid;
	/* Check id this is for-us address */
	if (in6_ifawithifp_lla(ifp, dst)) {
		if ((ifp = V_loif) != NULL)
			nh->lifp_idx = ifp->if_index;
	}

	return (0);
}


static void
fib6_rte_to_nh_basic(struct rtentry *rte, const struct in6_addr *dst,
    struct nhop6_basic *pnh6)
{
	struct sockaddr_in6 *gw;

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_ifp = ifnet_byindex(fib6_get_ifa(rte));

	pnh6->nh_mtu = min(rte->rt_mtu, IN6_LINKMTU(rte->rt_ifp));
	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in6 *)rte->rt_gateway;
		pnh6->nh_addr = gw->sin6_addr;
		in6_clearscope(&pnh6->nh_addr);
	} else
		pnh6->nh_addr = *dst;
	/* Set flags */
	pnh6->nh_flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw = (struct sockaddr_in6 *)rt_key(rte);
	if (IN6_IS_ADDR_UNSPECIFIED(&gw->sin6_addr))
		pnh6->nh_flags |= NHF_DEFAULT;
}

static void
fib6_rte_to_nh_extended(struct rtentry *rte, struct in6_addr *dst,
    struct nhop6_extended *pnh6)
{
	struct sockaddr_in6 *gw;
	struct in6_ifaddr *ia;

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_ifp = ifnet_byindex(fib6_get_ifa(rte));
	pnh6->nh_mtu = min(rte->rt_mtu, IN6_LINKMTU(rte->rt_ifp));
	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in6 *)rte->rt_gateway;
		pnh6->nh_addr = gw->sin6_addr;
		in6_clearscope(&pnh6->nh_addr);
	} else
		pnh6->nh_addr = *dst;
	/* Set flags */
	pnh6->nh_flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw = (struct sockaddr_in6 *)rt_key(rte);
	if (IN6_IS_ADDR_UNSPECIFIED(&gw->sin6_addr))
		pnh6->nh_flags |= NHF_DEFAULT;

	ia = ifatoia6(rte->rt_ifa);
}

#define ipv6_masklen(x)		bitcount32((x).__u6_addr.__u6_addr32[0]) + \
				bitcount32((x).__u6_addr.__u6_addr32[1]) + \
				bitcount32((x).__u6_addr.__u6_addr32[2]) + \
				bitcount32((x).__u6_addr.__u6_addr32[3])
static void
rib6_rte_to_nh_extended(struct rtentry *rte, struct in6_addr *dst,
    struct rt6_extended *prt6)
{
	struct sockaddr_in6 *gw;

	/* Do explicit nexthop zero unless we're copying it */
	memset(prt6, 0, sizeof(*prt6));

    	gw = ((struct sockaddr_in6 *)rt_key(rte));
	prt6->rt_addr = gw->sin6_addr;
    	gw = ((struct sockaddr_in6 *)rt_mask(rte));
	prt6->rt_mask = (gw != NULL) ? ipv6_masklen(gw->sin6_addr) : 128;

	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in6 *)rte->rt_gateway;
		prt6->rt_gateway = gw->sin6_addr;
		in6_clearscope(&prt6->rt_gateway);
	} else
		prt6->rt_gateway = *dst;

	prt6->rt_lifp = rte->rt_ifp;
	prt6->rt_aifp = ifnet_byindex(fib6_get_ifa(rte));
	prt6->rt_flags = fib_rte_to_nh_flags(rte->rt_flags);
	prt6->rt_mtu = min(rte->rt_mtu, IN6_LINKMTU(rte->rt_ifp));
}

int
fib6_lookup_nh_ifp(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, struct nhop6_basic *pnh6)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	RIB_LOCK_READER;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		/* XXX: Check if dst is local */
		return (fib6_lla_to_nh_basic(dst, scopeid, pnh6));
	}

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh_basic: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_addr = *dst;
	sin6.sin6_scope_id = scopeid;
	sa6_embedscope(&sin6, 0);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib6_rte_to_nh_basic(rte, dst, pnh6);
			pnh6->nh_ifp = rte->rt_ifp;
			RIB_RUNLOCK(rh);
			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

int
fib6_lookup_nh_basic(uint32_t fibnum, const struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, struct nhop6_basic *pnh6)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	RIB_LOCK_READER;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		return (fib6_lla_to_nh_basic(dst, scopeid, pnh6));
	}

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh_basic: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_addr = *dst;
	sin6.sin6_scope_id = scopeid;
	sa6_embedscope(&sin6, 0);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib6_rte_to_nh_basic(rte, dst, pnh6);
			RIB_RUNLOCK(rh);
			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

/*
 * Performs IPv6 route table lookup on @dst. Returns 0 on success.
 * Stores extende nexthop info provided @pnh4 structure.
 * Note that
 * - nh_ifp cannot be safely dereferenced unless NHOP_LOOKUP_REF is specified.
 * - in that case you need to call fib6_free_nh_ext()
 * - nh_ifp represents logical transmit interface (rt_ifp)
 * - mtu from logical transmit interface will be returned.
 */
int
fib6_lookup_nh_ext(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, uint32_t flags, struct nhop6_extended *pnh6)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	RIB_LOCK_READER;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		/* XXX: Do lwref on egress ifp */
		return (fib6_lla_to_nh_extended(dst, scopeid, pnh6));
	}

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh_ext: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst;
	sin6.sin6_scope_id = scopeid;
	sa6_embedscope(&sin6, 0);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib6_rte_to_nh_extended(rte, dst, pnh6);
			if ((flags & NHOP_LOOKUP_REF) != 0) {
				/* TODO: Do lwref on egress ifp's */
			}
			RIB_RUNLOCK(rh);

			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

void
fib6_free_nh_ext(uint32_t fibnum, struct nhop6_extended *pnh6)
{

}

int
rib6_lookup_nh_ext(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, uint32_t flags, struct rt6_extended *prt6)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	RIB_LOCK_READER;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		/* XXX: Do lwref on egress ifp */
		return (rib6_lla_to_nh_extended(dst, scopeid, prt6));
	}

	KASSERT((fibnum < rt_numfibs), ("rib6_lookup_nh_ext: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst;
	sin6.sin6_scope_id = scopeid;
	sa6_embedscope(&sin6, 0);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			rib6_rte_to_nh_extended(rte, dst, prt6);
			if ((flags & NHOP_LOOKUP_REF) != 0) {
				/* TODO: Do lwref on egress ifp's */
			}
			RIB_RUNLOCK(rh);

			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

void
rib6_free_nh_ext(uint32_t fibnum, struct nhop6_extended *prt6)
{

}

#endif

