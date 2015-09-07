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
#include <netinet/in_fib.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip6.h>

#include <net/if_llatbl.h>

#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <net/rt_nhops.h>

#include <vm/uma.h>

#ifdef INET
static void rib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct rt4_extended *prt4);
static void fib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct nhop4_extended *pnh4);
static void fib4_rte_to_nh_basic(struct rtentry *rte, struct in_addr dst,
    struct nhop4_basic *pnh4);

#define RNTORT(p)	((struct rtentry *)(p))

void
fib4_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh)
{

	/* TODO: Do some light-weight refcounting on egress ifp's */
	//fib_free_nh_prepend(fibnum, nh, AF_INET);
}

void
fib4_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop4_extended *nh_ext)
{

	fib_choose_prepend(fibnum, nh_src, flowid, nh, AF_INET);
	if (nh_ext == NULL)
		return;

	nh_ext->nh_ifp = NH_LIFP(nh);
	nh_ext->nh_mtu = nh->nh_mtu;
	nh_ext->nh_flags = nh->nh_flags;
#if 0
	/* TODO: copy source/gw address from extended nexthop data */
	nh_ext->nh_addr = ;
	nh_ext->nh_src= ;
#endif
}

/*
 * Function performs lookup in IPv4 table fib @fibnum.
 *
 * In case of successful lookup @nh header is filled with
 * appropriate interface info and full L2 header to prepend.
 *
 * If no valid ARP record is present, NHF_L2_INCOMPLETE flag
 * is set and gateway address is stored into nh->d.gw4
 *
 * If @nh_ext is not NULL, additional nexthop data is stored there.
 *
 * Returns 0 on success.
 *
 */
int
fib4_lookup_prepend(uint32_t fibnum, struct in_addr dst, struct mbuf *m,
    struct nhop_prepend *nh, struct nhop4_extended *nh_ext)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in *gw_sa, sin;
	struct ifnet *lifp;
	struct in_addr gw;
	struct ether_header *eh;
	int error, flags;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_prepend: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (EHOSTUNREACH);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin, &rh->head);
	rte = RNTORT(rn);
	if (rn == NULL || ((rn->rn_flags & RNF_ROOT) != 0) ||
	    RT_LINK_IS_UP(rte->rt_ifp) == 0) {
		RIB_RUNLOCK(rh);
		return (EHOSTUNREACH);
	}

	/*
	 * Currently we fill in @nh ourselves.
	 * In near future rte will have nhop index to copy from.
	 */

	/* Calculate L3 info */
	flags = 0;
	nh->nh_mtu = min(rte->rt_mtu, rte->rt_ifp->if_mtu);
	if (rte->rt_flags & RTF_GATEWAY) {
		gw_sa = (struct sockaddr_in *)rte->rt_gateway;
		gw = gw_sa->sin_addr;
	} else
		gw = dst;
	/* Set flags */
	flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw_sa = (struct sockaddr_in *)rt_key(rte);
	if (gw_sa->sin_addr.s_addr == 0)
		flags |= NHF_DEFAULT;

	/*
	 * TODO: nh L2/L3 resolve.
	 * Currently all we have is rte ifp.
	 * Simply use it.
	 */
	/* Save interface address ifp */
	lifp = rte->rt_ifa->ifa_ifp;
	nh->aifp_idx = lifp->if_index;
	/* Save both logical and transmit interface indexes */
	lifp = rte->rt_ifp;
	nh->lifp_idx = lifp->if_index;
	nh->i.ifp_idx = nh->lifp_idx;

	if (nh_ext != NULL) {
		/* Fill in extended info */
		fib4_rte_to_nh_extended(rte, dst, nh_ext);
	}

	RIB_RUNLOCK(rh);

	nh->nh_flags = flags;
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
		error = arpresolve_fast(lifp, gw, m ? m->m_flags : 0,
		    eh->ether_dhost);
		if (error == 0) {
			memcpy(&eh->ether_shost, IF_LLADDR(lifp), ETHER_ADDR_LEN);
			eh->ether_type = htons(ETHERTYPE_IP);
			nh->nh_count = ETHER_HDR_LEN;
			return (0);
		}
	}

	/* Notify caller that no L2 info is linked */
	nh->nh_count = 0;
	nh->nh_flags |= NHF_L2_INCOMPLETE;
	/* ..And save gateway address */
	nh->d.gw4 = gw;
	return (0);
}

int
fib4_sendmbuf(struct ifnet *ifp, struct mbuf *m, struct nhop_prepend *nh,
    struct in_addr dst)
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
		ni.ni_flags = RT_NHOP;
		ni.ni_family = AF_INET;
		ni.ni_nh = nh;

		M_PREPEND(m, nh->nh_count, M_NOWAIT);
		if (m == NULL)
			return (ENOBUFS);
		eh = mtod(m, struct ether_header *);
		memcpy(eh, nh->d.data, nh->nh_count);
		error = (*ifp->if_output)(ifp, m, NULL, &ni);
	} else {
		struct sockaddr_in gw_out;
		memset(&gw_out, 0, sizeof(gw_out));
		gw_out.sin_len = sizeof(gw_out);
		gw_out.sin_family = AF_INET;
		gw_out.sin_addr = nh ? nh->d.gw4 : dst;
		error = (*ifp->if_output)(ifp, m,
		    (const struct sockaddr *)&gw_out, NULL);
	}

	return (error);
}

static void
fib4_rte_to_nh_basic(struct rtentry *rte, struct in_addr dst,
    struct nhop4_basic *pnh4)
{
	struct sockaddr_in *gw;

	pnh4->nh_ifp = rte->rt_ifa->ifa_ifp;
	pnh4->nh_mtu = min(rte->rt_mtu, rte->rt_ifp->if_mtu);
	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in *)rte->rt_gateway;
		pnh4->nh_addr = gw->sin_addr;
	} else
		pnh4->nh_addr = dst;
	/* Set flags */
	pnh4->nh_flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw = (struct sockaddr_in *)rt_key(rte);
	if (gw->sin_addr.s_addr == 0)
		pnh4->nh_flags |= NHF_DEFAULT;
	/* XXX: Set RTF_BROADCAST if GW address is broadcast */
}

static void
fib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct nhop4_extended *pnh4)
{
	struct sockaddr_in *gw;
	struct in_ifaddr *ia;

	pnh4->nh_ifp = rte->rt_ifa->ifa_ifp;
	pnh4->nh_mtu = min(rte->rt_mtu, rte->rt_ifp->if_mtu);
	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in *)rte->rt_gateway;
		pnh4->nh_addr = gw->sin_addr;
	} else
		pnh4->nh_addr = dst;
	/* Set flags */
	pnh4->nh_flags = fib_rte_to_nh_flags(rte->rt_flags);
	gw = (struct sockaddr_in *)rt_key(rte);
	if (gw->sin_addr.s_addr == 0)
		pnh4->nh_flags |= NHF_DEFAULT;
	/* XXX: Set RTF_BROADCAST if GW address is broadcast */

	ia = ifatoia(rte->rt_ifa);
	pnh4->nh_src = IA_SIN(ia)->sin_addr;
}

static void
rib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct rt4_extended *prt4)
{
	struct sockaddr_in *gw;
	struct in_ifaddr *ia;

	/* Do explicit nexthop zero unless we're copying it */
	memset(prt4, 0, sizeof(*prt4));

    	gw = ((struct sockaddr_in *)rt_key(rte));
	prt4->rt_addr = gw->sin_addr;
    	gw = ((struct sockaddr_in *)rt_mask(rte));
	prt4->rt_mask.s_addr = (gw != NULL) ?
	    gw->sin_addr.s_addr : INADDR_BROADCAST;

	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in *)rte->rt_gateway;
		prt4->rt_gateway = gw->sin_addr;
	} else
		prt4->rt_gateway = dst;

	prt4->rt_lifp = rte->rt_ifp;
	prt4->rt_aifp = rte->rt_ifa->ifa_ifp;
	prt4->rt_flags = rte->rt_flags;
	prt4->rt_mtu = min(rte->rt_mtu, rte->rt_ifp->if_mtu);

	prt4->rt_nhop = 0; /* XXX: fill real nexthop */

	ia = ifatoia(rte->rt_ifa);
	prt4->rt_src = IA_SIN(ia)->sin_addr;
}

/*
 * Performs IPv4 route table lookup on @dst. Returns 0 on success.
 * Stores nexthop info provided @pnh4 structure.
 * Note that
 * - nh_ifp cannot be safely dereferenced
 * - nh_ifp represents ifaddr ifp (e.g. if looking up address on
 *   interface "ix0" pointer to "ix0" interface will be returned instead
 *   of "lo0")
 * - howewer mtu from "transmit" interface will be returned.
 */
int
fib4_lookup_nh_basic(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh_basic: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib4_rte_to_nh_basic(rte, dst, pnh4);
			RIB_RUNLOCK(rh);

			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

int
fib4_lookup_nh_ifp(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh_ifp: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib4_rte_to_nh_basic(rte, dst, pnh4);
			RIB_RUNLOCK(rh);
			pnh4->nh_ifp = rte->rt_ifp;
			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

/*
 * Performs IPv4 route table lookup on @dst. Returns 0 on success.
 * Stores extende nexthop info provided @pnh4 structure.
 * Note that
 * - nh_ifp cannot be safely dereferenced unless NHOP_LOOKUP_REF is specified.
 * - in that case you need to call fib4_free_nh_ext()
 * - nh_ifp represents logical transmit interface (rt_ifp)
 * - mtu from logical transmit interface will be returned.
 */
int
fib4_lookup_nh_ext(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    uint32_t flags, struct nhop4_extended *pnh4)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh_ext: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib4_rte_to_nh_extended(rte, dst, pnh4);
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
fib4_free_nh_ext(uint32_t fibnum, struct nhop4_extended *pnh4)
{

}

void
fib4_source_to_sa_ext(const struct nhopu_extended *pnhu, struct sockaddr_in *sin)
{

	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = pnhu->u.nh4.nh_src;
}

int
rib4_lookup_nh_ext(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    uint32_t flags, struct rt4_extended *prt4)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("rib4_lookup_nh_ext: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			rib4_rte_to_nh_extended(rte, dst, prt4);
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
rib4_free_nh_ext(uint32_t fibnum, struct rt4_extended *prt4)
{

}

#endif
