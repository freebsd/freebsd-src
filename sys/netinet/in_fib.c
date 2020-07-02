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
#include <netinet/in_fib.h>

#ifdef INET

/* Verify struct route compatiblity */
/* Assert 'struct route_in' is compatible with 'struct route' */
CHK_STRUCT_ROUTE_COMPAT(struct route_in, ro_dst4);

/*
 * Looks up path in fib @fibnum specified by @dst.
 * Returns path nexthop on success. Nexthop is safe to use
 *  within the current network epoch. If longer lifetime is required,
 *  one needs to pass NHR_REF as a flag. This will return referenced
 *  nexthop.
 */
struct nhop_object *
fib4_lookup(uint32_t fibnum, struct in_addr dst, uint32_t scopeid,
    uint32_t flags, uint32_t flowid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (NULL);

	/* Prepare lookup key */
	struct sockaddr_in sin4;
	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_len = sizeof(struct sockaddr_in);
	sin4.sin_addr = dst;

	nh = NULL;
	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin4, &rh->head);
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
fib4_check_urpf(uint32_t fibnum, struct in_addr dst, uint32_t scopeid,
  uint32_t flags, const struct ifnet *src_if)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	int ret;

	KASSERT((fibnum < rt_numfibs), ("fib4_check_urpf: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (0);

	/* Prepare lookup key */
	struct sockaddr_in sin4;
	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_len = sizeof(struct sockaddr_in);
	sin4.sin_addr = dst;

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin4, &rh->head);
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
fib4_lookup_debugnet(uint32_t fibnum, struct in_addr dst, uint32_t scopeid,
    uint32_t flags)
{
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	struct nhop_object *nh;

	KASSERT((fibnum < rt_numfibs), ("fib4_lookup_debugnet: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, AF_INET);
	if (rh == NULL)
		return (NULL);

	/* Prepare lookup key */
	struct sockaddr_in sin4;
	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_len = sizeof(struct sockaddr_in);
	sin4.sin_addr = dst;

	nh = NULL;
	/* unlocked lookup */
	rn = rh->rnh_matchaddr((void *)&sin4, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = RNTORT(rn);
#ifdef RADIX_MPATH
		if (rt_mpath_next(rt) != NULL)
			rt = rt_mpath_selectrte(rt, 0);
#endif
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
