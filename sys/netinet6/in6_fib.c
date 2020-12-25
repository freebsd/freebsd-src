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
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/fib_algo.h>
#include <net/route/nhop.h>
#include <net/toeplitz.h>
#include <net/vnet.h>

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

CHK_STRUCT_ROUTE_COMPAT(struct route_in6, ro_dst);

#ifdef FIB_ALGO
VNET_DEFINE(struct fib_dp *, inet6_dp);
#endif

#ifdef ROUTE_MPATH
struct _hash_5tuple_ipv6 {
	struct in6_addr src;
	struct in6_addr dst;
	unsigned short src_port;
	unsigned short dst_port;
	char proto;
	char spare[3];
};
_Static_assert(sizeof(struct _hash_5tuple_ipv6) == 40,
    "_hash_5tuple_ipv6 size is wrong");

uint32_t
fib6_calc_software_hash(const struct in6_addr *src, const struct in6_addr *dst,
    unsigned short src_port, unsigned short dst_port, char proto,
    uint32_t *phashtype)
{
	struct _hash_5tuple_ipv6 data;

	data.src = *src;
	data.dst = *dst;
	data.src_port = src_port;
	data.dst_port = dst_port;
	data.proto = proto;
	data.spare[0] = data.spare[1] = data.spare[2] = 0;

	*phashtype = M_HASHTYPE_OPAQUE_HASH;

	return (toeplitz_hash(MPATH_ENTROPY_KEY_LEN, mpath_entropy_key,
	  sizeof(data), (uint8_t *)&data));
}
#endif

/*
 * Looks up path in fib @fibnum specified by @dst.
 * Assumes scope is deembedded and provided in @scopeid.
 *
 * Returns path nexthop on success. Nexthop is safe to use
 *  within the current network epoch. If longer lifetime is required,
 *  one needs to pass NHR_REF as a flag. This will return referenced
 *  nexthop.
 */
#ifdef FIB_ALGO
struct nhop_object *
fib6_lookup(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags, uint32_t flowid)
{
	struct nhop_object *nh;
	struct fib_dp *dp = &V_inet6_dp[fibnum];
	struct flm_lookup_key key = {.addr6 = dst6 };

	nh = dp->f(dp->arg, key, scopeid);
	if (nh != NULL) {
		nh = nhop_select(nh, flowid);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp)) {
			if (flags & NHR_REF)
				nhop_ref_object(nh);
			return (nh);
		}
	}
	RTSTAT_INC(rts_unreach);
	return (NULL);
}
#else
struct nhop_object *
fib6_lookup(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags, uint32_t flowid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (NULL);

	struct sockaddr_in6 sin6 = {
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_addr = *dst6,
	};

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		nh = nhop_select((RNTORT(rn))->rt_nhop, flowid);
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
#endif

inline static int
check_urpf_nhop(const struct nhop_object *nh, uint32_t flags,
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

static int
check_urpf(struct nhop_object *nh, uint32_t flags,
    const struct ifnet *src_if)
{
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		struct weightened_nhop *wn;
		uint32_t num_nhops;
		wn = nhgrp_get_nhops((struct nhgrp_object *)nh, &num_nhops);
		for (int i = 0; i < num_nhops; i++) {
			if (check_urpf_nhop(wn[i].nh, flags, src_if) != 0)
				return (1);
		}
		return (0);
	} else
#endif
		return (check_urpf_nhop(nh, flags, src_if));
}

static struct nhop_object *
lookup_nhop(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib6_check_urpf: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (NULL);

	/* Prepare lookup key */
	struct sockaddr_in6 sin6 = {
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_addr = *dst6,
	};

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	nh = NULL;
	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0))
		nh = RNTORT(rn)->rt_nhop;
	RIB_RUNLOCK(rh);

	return (nh);
}

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
	struct nhop_object *nh;
#ifdef FIB_ALGO
	struct fib_dp *dp = &V_inet6_dp[fibnum];
	struct flm_lookup_key key = {.addr6 = dst6 };

	nh = dp->f(dp->arg, key, scopeid);
#else
	nh = lookup_nhop(fibnum, dst6, scopeid);
#endif
	if (nh != NULL)
		return (check_urpf(nh, flags, src_if));
	return (0);
}

/*
 * Function returning prefix match data along with the nexthop data.
 * Intended to be used by the control plane code.
 * Supported flags:
 *  NHR_UNLOCKED: do not lock radix during lookup.
 * Returns pointer to rtentry and raw nexthop in @rnd. Both rtentry
 *  and nexthop are safe to use within current epoch. Note:
 * Note: rnd_nhop can actually be the nexthop group.
 */
struct rtentry *
fib6_lookup_rt(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags, struct route_nhop_data *rnd)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;

	KASSERT((fibnum < rt_numfibs), ("fib6_lookup: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET6);
	if (rh == NULL)
		return (NULL);

	struct sockaddr_in6 sin6 = {
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_addr = *dst6,
	};

	/* Assume scopeid is valid and embed it directly */
	if (IN6_IS_SCOPE_LINKLOCAL(dst6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	rt = NULL;
	if (!(flags & NHR_UNLOCKED))
		RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = (struct rtentry *)rn;
		rnd->rnd_nhop = rt->rt_nhop;
		rnd->rnd_weight = rt->rt_weight;
	}
	if (!(flags & NHR_UNLOCKED))
		RIB_RUNLOCK(rh);

	return (rt);
}

struct nhop_object *
fib6_lookup_debugnet(uint32_t fibnum, const struct in6_addr *dst6,
    uint32_t scopeid, uint32_t flags)
{
	struct rtentry *rt;
	struct route_nhop_data rnd;

	rt = fib6_lookup_rt(fibnum, dst6, scopeid, NHR_UNLOCKED, &rnd);
	if (rt != NULL) {
		struct nhop_object *nh = nhop_select(rnd.rnd_nhop, 0);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(nh->nh_ifp))
			return (nh);
	}

	return (NULL);
}

#endif
