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
#include <net/vnet.h>

#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip6.h>

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

#ifdef INET
static void fib4_rte_to_nh_extended(struct rtentry *rte, struct in_addr dst,
    struct nhop4_extended *pnh4);
static void fib4_rte_to_nh_basic(struct rtentry *rte, struct in_addr dst,
    struct nhop4_basic *pnh4);
#endif
#ifdef INET
static void fib6_rte_to_nh_basic(struct rtentry *rte, struct in6_addr dst,
    struct nhop6_basic *pnh6);
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

#define	NHOP_FLAGS_MASK	(RTF_REJECT|RTF_BLACKHOLE)
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
fib_choose_prepend(uint32_t fibnum, struct nhop_data *nh_src,
    uint32_t flowid, struct nhop_data *nh, int af)
{
	struct nhop_multi *nh_multi;
	int idx;

	if ((nh_src->nh_flags & NH_FLAGS_RECURSE) != 0) {

		/*
		 * Recursive nexthop. Choose direct nexthop
		 * based on flowid.
		 */
		nh_multi = (struct nhop_multi *)nh_src;
		idx = nh_multi->nh_nhops[flowid % nh_multi->nh_count];
#if 0
		KASSERT((fibnum < rt_numfibs), ("fib4_lookup_prepend§: bad fibnum"));
		rnh = rt_tables_get_rnh(fibnum, AF_INET);
		//nh_src = &rnh->nhops[i];
#endif
	}

	*nh = *nh_src; 
	/* TODO: Do some light-weight refcounting on egress ifp's */
}

static inline void
fib_free_nh(uint32_t fibnum, struct nhop_data *nh, int af)
{

	/* TODO: Do some light-weight refcounting on egress ifp's */
}

#ifdef INET
void
fib4_free_nh(uint32_t fibnum, struct nhop_data *nh)
{

	fib_free_nh(fibnum, nh, AF_INET);
}

void
fib4_choose_prepend(uint32_t fibnum, struct nhop_data *nh_src,
    uint32_t flowid, struct nhop_data *nh, struct nhop4_extended *nh_ext)
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
 * If no valid ARP record is present, NH_FLAGS_L2_INCOMPLETE flag
 * is set and gateway address is stored into nh->d.gw4
 *
 * If @nh_ext is not NULL, additional nexthop data is stored there.
 *
 * Returns 0 on success.
 *
 */
int
fib4_lookup_prepend(uint32_t fibnum, struct in_addr dst, struct mbuf *m,
    struct nhop_data *nh, struct nhop4_extended *nh_ext)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct sockaddr_in *gw_sa, sin;
	struct ifnet *lifp;
	struct in_addr gw;
	struct ether_header *eh;
	int error, flags;
	//uint32_t flowid;
	struct rtentry *rte;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_prepend: bad fibnum"));
	rnh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rnh == NULL)
		return (EHOSTUNREACH);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr = dst;

	RADIX_NODE_HEAD_RLOCK(rnh);
	rn = rnh->rnh_matchaddr((void *)&sin, rnh);
	rte = RNTORT(rn);
	if (rn == NULL || ((rn->rn_flags & RNF_ROOT) != 0) ||
	    RT_LINK_IS_UP(rte->rt_ifp) == 0) {
		RADIX_NODE_HEAD_RUNLOCK(rnh);
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
	flags = rte->rt_flags & NHOP_FLAGS_MASK;
	gw_sa = (struct sockaddr_in *)rt_key(rte);
	if (gw_sa->sin_addr.s_addr == 0)
		flags |= NHOP_DEFAULT;

	/*
	 * TODO: nh L2/L3 resolve.
	 * Currently all we have is rte ifp.
	 * Simply use it.
	 */
	lifp = rte->rt_ifp;
	/* Save both logical and transmit interface indexes */
	nh->lifp_idx = lifp->if_index;
	nh->i.ifp_idx = nh->lifp_idx;

	if (nh_ext != NULL) {
		/* Fill in extended info */
		fib4_rte_to_nh_extended(rte, dst, nh_ext);
	}

	RADIX_NODE_HEAD_RUNLOCK(rnh);

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
		error = arpresolve_fast(lifp, gw, m->m_flags, eh->ether_dhost);
		if (error == 0) {
			memcpy(&eh->ether_shost, IF_LLADDR(lifp), ETHER_ADDR_LEN);
			eh->ether_type = htons(ETHERTYPE_IP);
			nh->nh_count = ETHER_HDR_LEN;
			return (0);
		}
	}

	/* Notify caller that no L2 info is linked */
	nh->nh_count = 0;
	nh->nh_flags |= NH_FLAGS_L2_INCOMPLETE;
	/* ..And save gateway address */
	nh->d.gw4 = gw;
	return (0);
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

	ia = ifatoia(rte->rt_ifa);
	pnh4->nh_src = IA_SIN(ia)->sin_addr;

	/* Set flags */
	pnh4->nh_flags = rte->rt_flags & NHOP_FLAGS_MASK;
	gw = (struct sockaddr_in *)rt_key(rte);
	if (gw->sin_addr.s_addr == 0)
		pnh4->nh_flags |= NHOP_DEFAULT;
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
	pnh4->nh_flags = rte->rt_flags & NHOP_FLAGS_MASK;
	gw = (struct sockaddr_in *)rt_key(rte);
	if (gw->sin_addr.s_addr == 0)
		pnh4->nh_flags |= NHOP_DEFAULT;
}

int
fib4_lookup_nh_basic(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct sockaddr_in sin;
	struct rtentry *rte;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_nh_basic: bad fibnum"));
	rnh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rnh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin, 0, sizeof(sin));
	sin.sin_addr = dst;

	RADIX_NODE_HEAD_RLOCK(rnh);
	rn = rnh->rnh_matchaddr((void *)&sin, rnh);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib4_rte_to_nh_basic(rte, dst, pnh4);
			RADIX_NODE_HEAD_RUNLOCK(rnh);

			return (0);
		}
	}
	RADIX_NODE_HEAD_RUNLOCK(rnh);

	return (ENOENT);
}
#endif

#ifdef INET6
void
fib6_free_nh(uint32_t fibnum, struct nhop_data *nh)
{

	fib_free_nh(fibnum, nh, AF_INET6);
}

void
fib6_choose_prepend(uint32_t fibnum, struct nhop_data *nh_src,
    uint32_t flowid, struct nhop_data *nh, struct nhop6_extended *nh_ext)
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


static void
fib6_rte_to_nh_basic(struct rtentry *rte, struct in6_addr dst,
    struct nhop6_basic *pnh6)
{
	struct sockaddr_in6 *gw;

	pnh6->nh_ifp = rte->rt_ifa->ifa_ifp;
	pnh6->nh_mtu = min(rte->rt_mtu, rte->rt_ifp->if_mtu);
	if (rte->rt_flags & RTF_GATEWAY) {
		gw = (struct sockaddr_in6 *)rte->rt_gateway;
		pnh6->nh_addr = gw->sin6_addr;
	} else
		pnh6->nh_addr = dst;
	/* Set flags */
	pnh6->nh_flags = rte->rt_flags & NHOP_FLAGS_MASK;
	gw = (struct sockaddr_in6 *)rt_key(rte);
	if (IN6_IS_ADDR_UNSPECIFIED(&gw->sin6_addr))
		pnh6->nh_flags |= NHOP_DEFAULT;
}

int
fib6_lookup_nh_basic(uint32_t fibnum, struct in6_addr dst, uint32_t flowid,
    struct nhop6_basic *pnh6)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh_basic: bad fibnum"));
	rnh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rnh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_addr = dst;

	RADIX_NODE_HEAD_RLOCK(rnh);
	rn = rnh->rnh_matchaddr((void *)&sin6, rnh);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rte->rt_ifp)) {
			fib6_rte_to_nh_basic(rte, dst, pnh6);
			RADIX_NODE_HEAD_RUNLOCK(rnh);
			return (0);
		}
	}
	RADIX_NODE_HEAD_RUNLOCK(rnh);

	return (ENOENT);
}
#endif


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








