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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"
#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>
#include <net/route/shared.h>
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

#include <net/if_types.h>

#ifdef INET6
static void fib6_rte_to_nh_extended(const struct nhop_object *nh,
    const struct in6_addr *dst, uint32_t flags, struct nhop6_extended *pnh6);
static void fib6_rte_to_nh_basic(const struct nhop_object *nh, const struct in6_addr *dst,
    uint32_t flags, struct nhop6_basic *pnh6);

#define	ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))

CHK_STRUCT_ROUTE_COMPAT(struct route_in6, ro_dst);



static void
fib6_rte_to_nh_basic(const struct nhop_object *nh, const struct in6_addr *dst,
    uint32_t flags, struct nhop6_basic *pnh6)
{

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	if ((flags & NHR_IFAIF) != 0)
		pnh6->nh_ifp = nh->nh_aifp;
	else
		pnh6->nh_ifp = nh->nh_ifp;

	pnh6->nh_mtu = nh->nh_mtu;
	if (nh->nh_flags & NHF_GATEWAY) {
		/* Return address with embedded scope. */
		pnh6->nh_addr = nh->gw6_sa.sin6_addr;
	} else
		pnh6->nh_addr = *dst;
	/* Set flags */
	pnh6->nh_flags = nh->nh_flags;
}

static void
fib6_rte_to_nh_extended(const struct nhop_object *nh, const struct in6_addr *dst,
    uint32_t flags, struct nhop6_extended *pnh6)
{

	/* Do explicit nexthop zero unless we're copying it */
	memset(pnh6, 0, sizeof(*pnh6));

	if ((flags & NHR_IFAIF) != 0)
		pnh6->nh_ifp = nh->nh_aifp;
	else
		pnh6->nh_ifp = nh->nh_ifp;

	pnh6->nh_mtu = nh->nh_mtu;
	if (nh->nh_flags & NHF_GATEWAY) {
		/* Return address with embedded scope. */
		pnh6->nh_addr = nh->gw6_sa.sin6_addr;
	} else
		pnh6->nh_addr = *dst;
	/* Set flags */
	pnh6->nh_flags = nh->nh_flags;
	pnh6->nh_ia = ifatoia6(nh->nh_ifa);
}

/*
 * Performs IPv6 route table lookup on @dst. Returns 0 on success.
 * Stores basic nexthop info into provided @pnh6 structure.
 * Note that
 * - nh_ifp represents logical transmit interface (rt_ifp) by default
 * - nh_ifp represents "address" interface if NHR_IFAIF flag is passed
 * - mtu from logical transmit interface will be returned.
 * - nh_ifp cannot be safely dereferenced
 * - nh_ifp represents rt_ifp (e.g. if looking up address on
 *   interface "ix0" pointer to "ix0" interface will be returned instead
 *   of "lo0")
 * - howewer mtu from "transmit" interface will be returned.
 * - scope will be embedded in nh_addr
 */
int
fib6_lookup_nh_basic(uint32_t fibnum, const struct in6_addr *dst, uint32_t scopeid,
    uint32_t flags, uint32_t flowid, struct nhop6_basic *pnh6)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh_basic: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_addr = *dst;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		nh = RNTORT(rn)->rt_nhop;
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp)) {
			fib6_rte_to_nh_basic(nh, &sin6.sin6_addr, flags, pnh6);
			RIB_RUNLOCK(rh);
			return (0);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

/*
 * Performs IPv6 route table lookup on @dst. Returns 0 on success.
 * Stores extended nexthop info into provided @pnh6 structure.
 * Note that
 * - nh_ifp cannot be safely dereferenced unless NHR_REF is specified.
 * - in that case you need to call fib6_free_nh_ext()
 * - nh_ifp represents logical transmit interface (rt_ifp) by default
 * - nh_ifp represents "address" interface if NHR_IFAIF flag is passed
 * - mtu from logical transmit interface will be returned.
 * - scope will be embedded in nh_addr
 */
int
fib6_lookup_nh_ext(uint32_t fibnum, const struct in6_addr *dst,uint32_t scopeid,
    uint32_t flags, uint32_t flowid, struct nhop6_extended *pnh6)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct sockaddr_in6 sin6;
	struct rtentry *rte;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup_nh_ext: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (ENOENT);

	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst;
	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rte = RNTORT(rn);
#ifdef RADIX_MPATH
		rte = rt_mpath_select(rte, flowid);
		if (rte == NULL) {
			RIB_RUNLOCK(rh);
			return (ENOENT);
		}
#endif
		nh = rte->rt_nhop;
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp)) {
			fib6_rte_to_nh_extended(nh, &sin6.sin6_addr, flags,
			    pnh6);
			if ((flags & NHR_REF) != 0) {
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

/*
 * Looks up path in fib @fibnum specified by @dst.
 * Assumes scope is deembedded and provided in @scopeid.
 *
 * Returns path nexthop on success. Nexthop is safe to use
 *  within the current network epoch. If longer lifetime is required,
 *  one needs to pass NHR_REF as a flag. This will return referenced
 *  nexthop.
 */
struct nhop_object *
fib6_lookup(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags, uint32_t flowid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	struct nhop_object *nh;
	struct sockaddr_in6 sin6;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (NULL);

	/* TODO: radix changes */
	//addr = *dst6;
	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst6;

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = RNTORT(rn);
#ifdef RADIX_MPATH
		if (rt_mpath_next(rt) != NULL)
			rt = rt_mpath_selectrte(rt, flowid);
#endif
		nh = rt->rt_nhop;
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp)) {
			if (flags & NHR_REF)
				nhop_ref_object(nh);
			RIB_RUNLOCK(rh);
			return (nh);
		}
	}
	RIB_RUNLOCK(rh);

	RTSTAT_INC(rts_unreach);
	return (NULL);
}

inline static int
check_urpf(const struct nhop_object *nh, uint32_t flags,
    const struct ifnet *src_if)
{

	if (src_if != NULL && nh->nh_aifp == src_if) {
		return (1);
	}
	if (src_if == NULL) {
		if ((flags & NHR_NODEFAULT) == 0)
			return (1);
		else if ((nh->nh_flags & NHF_DEFAULT) == 0)
			return (1);
	}

	return (0);
}

#ifdef RADIX_MPATH
inline static int
check_urpf_mpath(struct rtentry *rt, uint32_t flags,
    const struct ifnet *src_if)
{
	
	while (rt != NULL) {
		if (check_urpf(rt->rt_nhop, flags, src_if) != 0)
			return (1);
		rt = rt_mpath_next(rt);
	}

	return (0);
}
#endif

/*
 * Performs reverse path forwarding lookup.
 * If @src_if is non-zero, verifies that at least 1 path goes via
 *   this interface.
 * If @src_if is zero, verifies that route exist.
 * if @flags contains NHR_NOTDEFAULT, do not consider default route.
 *
 * Returns 1 if route matching conditions is found, 0 otherwise.
 */
int
fib6_check_urpf(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags, const struct ifnet *src_if)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	struct sockaddr_in6 sin6;
	int ret;

	KASSERT((fibnum < rt_numfibs), ("fib6_check_urpf: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (0);

	/* TODO: radix changes */
	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst6;

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = RNTORT(rn);
#ifdef	RADIX_MPATH
		ret = check_urpf_mpath(rt, flags, src_if);
#else
		ret = check_urpf(rt->rt_nhop, flags, src_if);
#endif
		RIB_RUNLOCK(rh);
		return (ret);
	}
	RIB_RUNLOCK(rh);

	return (0);
}

struct nhop_object *
fib6_lookup_debugnet(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	struct nhop_object *nh;
	struct sockaddr_in6 sin6;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (NULL);

	/* TODO: radix changes */
	//addr = *dst6;
	/* Prepare lookup key */
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst6;

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = RNTORT(rn);
		nh = rt->rt_nhop;
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp)) {
			if (flags & NHR_REF)
				nhop_ref_object(nh);
			return (nh);
		}
	}

	return (NULL);
}

#endif

