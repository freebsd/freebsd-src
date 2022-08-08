/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021-2022 Alexander V. Chernikov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>
#include <netinet/in.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_var.h>

#include <vm/uma.h>

/* Routing table UMA zone */
VNET_DEFINE_STATIC(uma_zone_t, rtzone);
#define	V_rtzone	VNET(rtzone)

void
vnet_rtzone_init(void)
{

	V_rtzone = uma_zcreate("rtentry", sizeof(struct rtentry),
		NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

#ifdef VIMAGE
void
vnet_rtzone_destroy(void)
{

	uma_zdestroy(V_rtzone);
}
#endif

/*
 * Creates rtentry and based on @dst/@netmask data.
 * Return 0 and fills in rtentry into @prt on success,
 * Note: rtentry mask ptr will be set to @netmask , thus its pointer is required
 *  to be stable till the end of the operation (radix rt insertion/change/removal).
 */
struct rtentry *
rt_alloc(struct rib_head *rnh, const struct sockaddr *dst,
    struct sockaddr *netmask)
{
	MPASS(dst->sa_len <= sizeof(((struct rtentry *)NULL)->rt_dstb));

	struct rtentry *rt = uma_zalloc(V_rtzone, M_NOWAIT | M_ZERO);
	if (rt == NULL)
		return (NULL);
	rt->rte_flags = RTF_UP | (netmask == NULL ? RTF_HOST : 0);

	/* Fill in dst, ensuring it's masked if needed. */
	if (netmask != NULL) {
		rt_maskedcopy(dst, &rt->rt_dst, netmask);
	} else
		bcopy(dst, &rt->rt_dst, dst->sa_len);
	rt_key(rt) = &rt->rt_dst;
	/* Set netmask to the storage from info. It will be updated upon insertion */
	rt_mask(rt) = netmask;

	return (rt);
}

static void
destroy_rtentry(struct rtentry *rt)
{
#ifdef VIMAGE
	struct nhop_object *nh = rt->rt_nhop;

	/*
	 * At this moment rnh, nh_control may be already freed.
	 * nhop interface may have been migrated to a different vnet.
	 * Use vnet stored in the nexthop to delete the entry.
	 */
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		const struct weightened_nhop *wn;
		uint32_t num_nhops;
		wn = nhgrp_get_nhops((struct nhgrp_object *)nh, &num_nhops);
		nh = wn[0].nh;
	}
#endif
	CURVNET_SET(nhop_get_vnet(nh));
#endif

	/* Unreference nexthop */
	nhop_free_any(rt->rt_nhop);

	rt_free_immediate(rt);

	CURVNET_RESTORE();
}

/*
 * Epoch callback indicating rtentry is safe to destroy
 */
static void
destroy_rtentry_epoch(epoch_context_t ctx)
{
	struct rtentry *rt;

	rt = __containerof(ctx, struct rtentry, rt_epoch_ctx);

	destroy_rtentry(rt);
}

/*
 * Schedule rtentry deletion
 */
void
rt_free(struct rtentry *rt)
{

	KASSERT(rt != NULL, ("%s: NULL rt", __func__));

	epoch_call(net_epoch_preempt, destroy_rtentry_epoch,
	    &rt->rt_epoch_ctx);
}

void
rt_free_immediate(struct rtentry *rt)
{
	uma_zfree(V_rtzone, rt);
}

bool
rt_is_host(const struct rtentry *rt)
{

	return (rt->rte_flags & RTF_HOST);
}

sa_family_t
rt_get_family(const struct rtentry *rt)
{
	const struct sockaddr *dst;

	dst = (const struct sockaddr *)rt_key_const(rt);

	return (dst->sa_family);
}

/*
 * Returns pointer to nexthop or nexthop group
 * associated with @rt
 */
struct nhop_object *
rt_get_raw_nhop(const struct rtentry *rt)
{

	return (rt->rt_nhop);
}

#ifdef INET
/*
 * Stores IPv4 address and prefix length of @rt inside
 *  @paddr and @plen.
 * @pscopeid is currently always set to 0.
 */
void
rt_get_inet_prefix_plen(const struct rtentry *rt, struct in_addr *paddr,
    int *plen, uint32_t *pscopeid)
{
	const struct sockaddr_in *dst;

	dst = (const struct sockaddr_in *)rt_key_const(rt);
	KASSERT((dst->sin_family == AF_INET),
	    ("rt family is %d, not inet", dst->sin_family));
	*paddr = dst->sin_addr;
	dst = (const struct sockaddr_in *)rt_mask_const(rt);
	if (dst == NULL)
		*plen = 32;
	else
		*plen = bitcount32(dst->sin_addr.s_addr);
	*pscopeid = 0;
}

/*
 * Stores IPv4 address and prefix mask of @rt inside
 *  @paddr and @pmask. Sets mask to INADDR_ANY for host routes.
 * @pscopeid is currently always set to 0.
 */
void
rt_get_inet_prefix_pmask(const struct rtentry *rt, struct in_addr *paddr,
    struct in_addr *pmask, uint32_t *pscopeid)
{
	const struct sockaddr_in *dst;

	dst = (const struct sockaddr_in *)rt_key_const(rt);
	KASSERT((dst->sin_family == AF_INET),
	    ("rt family is %d, not inet", dst->sin_family));
	*paddr = dst->sin_addr;
	dst = (const struct sockaddr_in *)rt_mask_const(rt);
	if (dst == NULL)
		pmask->s_addr = INADDR_BROADCAST;
	else
		*pmask = dst->sin_addr;
	*pscopeid = 0;
}
#endif

#ifdef INET6
static int
inet6_get_plen(const struct in6_addr *addr)
{

	return (bitcount32(addr->s6_addr32[0]) + bitcount32(addr->s6_addr32[1]) +
	    bitcount32(addr->s6_addr32[2]) + bitcount32(addr->s6_addr32[3]));
}

/*
 * Stores IPv6 address and prefix length of @rt inside
 *  @paddr and @plen. Addresses are returned in de-embedded form.
 * Scopeid is set to 0 for non-LL addresses.
 */
void
rt_get_inet6_prefix_plen(const struct rtentry *rt, struct in6_addr *paddr,
    int *plen, uint32_t *pscopeid)
{
	const struct sockaddr_in6 *dst;

	dst = (const struct sockaddr_in6 *)rt_key_const(rt);
	KASSERT((dst->sin6_family == AF_INET6),
	    ("rt family is %d, not inet6", dst->sin6_family));
	if (IN6_IS_SCOPE_LINKLOCAL(&dst->sin6_addr))
		in6_splitscope(&dst->sin6_addr, paddr, pscopeid);
	else
		*paddr = dst->sin6_addr;
	dst = (const struct sockaddr_in6 *)rt_mask_const(rt);
	if (dst == NULL)
		*plen = 128;
	else
		*plen = inet6_get_plen(&dst->sin6_addr);
}

/*
 * Stores IPv6 address and prefix mask of @rt inside
 *  @paddr and @pmask. Addresses are returned in de-embedded form.
 * Scopeid is set to 0 for non-LL addresses.
 */
void
rt_get_inet6_prefix_pmask(const struct rtentry *rt, struct in6_addr *paddr,
    struct in6_addr *pmask, uint32_t *pscopeid)
{
	const struct sockaddr_in6 *dst;

	dst = (const struct sockaddr_in6 *)rt_key_const(rt);
	KASSERT((dst->sin6_family == AF_INET6),
	    ("rt family is %d, not inet", dst->sin6_family));
	if (IN6_IS_SCOPE_LINKLOCAL(&dst->sin6_addr))
		in6_splitscope(&dst->sin6_addr, paddr, pscopeid);
	else
		*paddr = dst->sin6_addr;
	dst = (const struct sockaddr_in6 *)rt_mask_const(rt);
	if (dst == NULL)
		memset(pmask, 0xFF, sizeof(struct in6_addr));
	else
		*pmask = dst->sin6_addr;
}
#endif


