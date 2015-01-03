/*-
 * Copyright (c) 2014
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

/*
 * Temporary file. In future it should be split between net/route.c
 * and per-AF files like netinet/in_rmx.c | netinet6/in6_rmx.c
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
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/scope6_var.h>

#include <net/if_llatbl.h>
#include <net/if_llatbl_var.h>

#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <net/rt_nhops.h>

#include <vm/uma.h>

struct fwd_info {
	fib_lookup_t	*lookup;
	void		*state;
};

#define	FWD_FSM_NONE	0
#define	FWD_FSM_INIT	1
#define	FWD_FSM_FWD	2
struct fwd_control {
	int		fwd_state;	/* FSM */
	struct fwd_module	*fm;
};

#if 0
static struct fwd_info *fwd_db[FWD_SIZE];
static struct fwd_control *fwd_ctl[FWD_SIZE];

static TAILQ_HEAD(fwd_module_list, fwd_module)	modulehead = TAILQ_HEAD_INITIALIZER(modulehead);
static struct fwd_module_list fwd_modules[FWD_SIZE];

static uint8_t fwd_map_af[] = {
	AF_INET,
	AF_INET6,
};

static struct rwlock fwd_lock;
#define	FWD_LOCK_INIT()	rw_init(&fwd_lock, "fwd_lock")
#define	FWD_RLOCK()	rw_rlock(&fwd_lock)
#define	FWD_RUNLOCK()	rw_runlock(&fwd_lock)
#define	FWD_WLOCK()	rw_wlock(&fwd_lock)
#define	FWD_WUNLOCK()	rw_wunlock(&fwd_lock)

int fwd_attach_fib(struct fwd_module *fm, u_int fib);
int fwd_destroy_fib(struct fwd_module *fm, u_int fib);
#endif

static inline uint16_t fib_rte_to_nh_flags(int rt_flags);
#ifdef INET
static void rib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct rt4_extended *prt4);
static void fib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    uint32_t flags, struct nhop4_extended *pnh4);
static void fib4_rte_to_nh(struct rtentry *rte, struct in_addr dst,
    uint32_t flags, struct nhop4_basic *pnh4);
#endif
#ifdef INET6
static void fib6_rte_to_nh_extended(struct rtentry *rte, struct in6_addr *dst,
    uint32_t flags, struct nhop6_extended *pnh6);
static void fib6_rte_to_nh(struct rtentry *rte, struct in6_addr *dst,
    uint32_t flags, struct nhop6_basic *pnh6);
static int fib6_storelladdr(struct ifnet *ifp, struct in6_addr *dst,
    int mm_flags, u_char *desten);
static uint16_t fib6_get_ifa(struct rtentry *rte);
static int fib6_lla_to_nh_basic(struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, struct nhop6_basic *pnh6);
static int fib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, struct nhop6_extended *pnh6);
static int fib6_lla_to_nh(struct in6_addr *dst, uint32_t scopeid,
    struct nhop_prepend *nh, struct ifnet **lifp);
#endif

MALLOC_DEFINE(M_RTFIB, "rtfib", "routing fwd");



/*
 * Per-AF fast routines returning minimal needed info.
 * It is not safe to dereference any pointers since it
 * may end up with use-after-free case.
 * Typically it may be used to check if outgoing
 * interface matches or to calculate proper MTU.
 *
 * Note that returned interface pointer is logical one,
 * e.g. actual transmit ifp may be different.
 * Difference may be triggered by
 * 1) loopback routes installed for interface addresses.
 *  e.g. for address 10.0.0.1 with prefix /24 bound to
 *  interface ix0, "logical" interface will be "ix0",
 *  while "trasmit" interface will be "lo0" since this is
 *  loopback route. You should consider using other
 *  functions if you need "transmit" interface or both.
 *
 *
 * Returns 0 on match, error code overwise.
 */

//#define	NHOP_DIRECT	
#define RNTORT(p)	((struct rtentry *)(p))


/*
 * Copies proper nexthop data based on @nh_src nexthop.
 *
 * For non-ECMP nexthop function simply copies @nh_src.
 * For ECMP nexthops flowid is used to select proper
 * nexthop.
 *
 */
static inline void
fib_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, int af)
{
	struct nhop_multi *nh_multi;
	int idx;

	if ((nh_src->nh_flags & NHF_RECURSE) != 0) {

		/*
		 * Recursive nexthop. Choose direct nexthop
		 * based on flowid.
		 */
		nh_multi = (struct nhop_multi *)nh_src;
		idx = nh_multi->nh_nhops[flowid % nh_multi->nh_count];
#if 0
		KASSERT((fibnum < rt_numfibs), ("fib4_lookup_prepend§: bad fibnum"));
		rh = rt_tables_get_rnh(fibnum, AF_INET);
		//nh_src = &rh->nhops[i];
#endif
	}

	*nh = *nh_src; 
	/* TODO: Do some light-weight refcounting on egress ifp's */
}

static inline void
fib_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh, int af)
{

	/* TODO: Do some light-weight refcounting on egress ifp's */
}

#ifdef INET
void
fib4_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh)
{

	fib_free_nh_prepend(fibnum, nh, AF_INET);
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
		fib4_rte_to_nh_extended(rte, dst, 0, nh_ext);
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

static inline uint16_t
fib_rte_to_nh_flags(int rt_flags)
{
	uint16_t res;

	res = (rt_flags & RTF_REJECT) ? NHF_REJECT : 0;
	res |= (rt_flags & RTF_BLACKHOLE) ? NHF_BLACKHOLE : 0;
	res |= (rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) ? NHF_REDIRECT : 0;
	res |= (rt_flags & RTF_BROADCAST) ? NHF_BROADCAST : 0;
	res |= (rt_flags & RTF_GATEWAY) ? NHF_GATEWAY : 0;

	return (res);
}

static void
fib4_rte_to_nh(struct rtentry *rte, struct in_addr dst,
    uint32_t flags, struct nhop4_basic *pnh4)
{
	struct sockaddr_in *gw;

	if ((flags & NHOP_LOOKUP_AIFP) == 0)
		pnh4->nh_ifp = rte->rt_ifp;
	else
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
    uint32_t flags, struct nhop4_extended *pnh4)
{
	struct sockaddr_in *gw;
	struct in_ifaddr *ia;

	if ((flags & NHOP_LOOKUP_AIFP) == 0)
		pnh4->nh_ifp = rte->rt_ifp;
	else
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
fib4_lookup_nh(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    uint32_t flags, struct nhop4_basic *pnh4)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;
	RIB_LOCK_READER;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh: bad fibnum"));
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
			fib4_rte_to_nh(rte, dst, flags, pnh4);
			RIB_RUNLOCK(rh);

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
			fib4_rte_to_nh_extended(rte, dst, flags, pnh4);
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

#ifdef INET6
void
fib6_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh)
{

	fib_free_nh_prepend(fibnum, nh, AF_INET6);
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
	IF_AFDATA_RUN_TRACKER;

	if (mm_flags & M_MCAST) {
		ETHER_MAP_IPV6_MULTICAST(&dst, desten);
		return (0);
	}

	/*
	 * the entry should have been created in nd6_store_lladdr
	 */
	IF_AFDATA_RUN_RLOCK(ifp);
	ln = lltable_lookup_lle6(ifp, LLE_UNLOCKED, dst);

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
		IF_AFDATA_RUN_RUNLOCK(ifp);
		return (1);
	}
	bcopy(&ln->ll_addr, desten, ifp->if_addrlen);
	IF_AFDATA_RUN_RUNLOCK(ifp);

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

		error = nd6_output(ifp, origifp, m, &gw_out, NULL);
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
fib6_lla_to_nh_basic(struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, struct nhop6_basic *pnh6)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_mtu = IN6_LINKMTU(ifp);
	pnh6->nh_ifp = ifp;
	if ((flags & NHOP_LOOKUP_AIFP)!=0 && in6_ifawithifp_lla(ifp, dst) != 0){
		if ((ifp = V_loif) != NULL)
			pnh6->nh_ifp = ifp;
	}

	/* No flags set */
	pnh6->nh_addr = *dst;

	return (0);
}

static int
fib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, struct nhop6_extended *pnh6)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex_locked(scopeid);
	if (ifp == NULL)
		return (ENOENT);

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	pnh6->nh_mtu = IN6_LINKMTU(ifp);
	pnh6->nh_ifp = ifp;
	if ((flags & NHOP_LOOKUP_AIFP)!=0 && in6_ifawithifp_lla(ifp, dst) != 0){
		if ((ifp = V_loif) != NULL)
			pnh6->nh_ifp = ifp;
	}
	/* No flags set */
	pnh6->nh_addr = *dst;

	return (0);
}

static int
rib6_lla_to_nh_extended(struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, struct rt6_extended *prt6)
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
fib6_rte_to_nh(struct rtentry *rte, struct in6_addr *dst,
    uint32_t flags, struct nhop6_basic *pnh6)
{
	struct sockaddr_in6 *gw;

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	if ((flags & NHOP_LOOKUP_AIFP) == 0)
		pnh6->nh_ifp = rte->rt_ifp;
	else
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
    uint32_t flags, struct nhop6_extended *pnh6)
{
	struct sockaddr_in6 *gw;
	struct in6_ifaddr *ia;

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	if ((flags & NHOP_LOOKUP_AIFP) == 0)
		pnh6->nh_ifp = rte->rt_ifp;
	else
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
    uint32_t flags, struct rt6_extended *prt6)
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
fib6_lookup_nh(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, uint32_t flags, struct nhop6_basic *pnh6)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	RIB_LOCK_READER;

	if (IN6_IS_SCOPE_LINKLOCAL(dst)) {
		/* Do not lookup link-local addresses in rtable */
		return (fib6_lla_to_nh_basic(dst, scopeid, flags, pnh6));
	}

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh: bad fibnum"));
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
			fib6_rte_to_nh(rte, dst, flags, pnh6);
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
		return (fib6_lla_to_nh_extended(dst, scopeid, flags, pnh6));
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
			fib6_rte_to_nh_extended(rte, dst, flags, pnh6);
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
		return (rib6_lla_to_nh_extended(dst, scopeid, flags, prt6));
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
			rib6_rte_to_nh_extended(rte, dst, flags, prt6);
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

void
fib_free_nh_ext(uint32_t fibnum, struct nhopu_extended *pnhu)
{

}


#if 0
typedef void nhop_change_cb_t(void *state);


struct nhop_tracker {
	TAILQ_ENTRY(nhop_tracker)	next;
	nhop_change_cb_t	*f;
	void		*state;
	uint32_t	fibnum;
	struct sockaddr_storage	ss;
};

struct nhop_tracker *
nhop_alloc_tracked(uint32_t fibnum, struct sockaddr *sa, nhop_change_cb_t *f,
    void *state)
{
	struct nhop_tracker *nt;

	nt = malloc(sizeof(struct nhop_tracker), M_RTFIB, M_WAITOK | M_ZERO);

	nt->f = f;
	nt-state = state;
	nt->fibnum = fibnum;
	memcpy(&nt->ss, sa, sa->sa_len);

	return (nt);
}


int
nhop_bind(struct nhop_tracker *nt)
{
	NHOP_LOCK(nnh);

	NHOP_UNLOCK(nnh);

	return (0);
}
#endif








