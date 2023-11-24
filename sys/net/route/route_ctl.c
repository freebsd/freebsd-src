/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_dl.h>
#include <net/vnet.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#include <netinet/in.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_var.h>

#define	DEBUG_MOD_NAME	route_ctl
#define	DEBUG_MAX_LEVEL	LOG_DEBUG
#include <net/route/route_debug.h>
_DECLARE_DEBUG(LOG_INFO);

/*
 * This file contains control plane routing tables functions.
 *
 * All functions assumes they are called in net epoch.
 */

union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	char			_buf[32];
};

static int add_route_byinfo(struct rib_head *rnh, struct rt_addrinfo *info,
    struct rib_cmd_info *rc);
static int change_route_byinfo(struct rib_head *rnh, struct rtentry *rt,
    struct rt_addrinfo *info, struct route_nhop_data *nhd_orig,
    struct rib_cmd_info *rc);

static int add_route_flags(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd_add, int op_flags, struct rib_cmd_info *rc);
#ifdef ROUTE_MPATH
static int add_route_flags_mpath(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd_add, struct route_nhop_data *rnd_orig,
    int op_flags, struct rib_cmd_info *rc);
#endif

static int add_route(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd, struct rib_cmd_info *rc);
static int delete_route(struct rib_head *rnh, struct rtentry *rt,
    struct rib_cmd_info *rc);
static int rt_delete_conditional(struct rib_head *rnh, struct rtentry *rt,
    int prio, rib_filter_f_t *cb, void *cbdata, struct rib_cmd_info *rc);

static bool fill_pxmask_family(int family, int plen, struct sockaddr *_dst,
    struct sockaddr **pmask);
static int get_prio_from_info(const struct rt_addrinfo *info);
static int nhop_get_prio(const struct nhop_object *nh);

#ifdef ROUTE_MPATH
static bool rib_can_multipath(struct rib_head *rh);
#endif

/* Per-vnet multipath routing configuration */
SYSCTL_DECL(_net_route);
#define	V_rib_route_multipath	VNET(rib_route_multipath)
#ifdef ROUTE_MPATH
#define _MP_FLAGS	CTLFLAG_RW
#else
#define _MP_FLAGS	CTLFLAG_RD
#endif
VNET_DEFINE(u_int, rib_route_multipath) = 1;
SYSCTL_UINT(_net_route, OID_AUTO, multipath, _MP_FLAGS | CTLFLAG_VNET,
    &VNET_NAME(rib_route_multipath), 0, "Enable route multipath");
#undef _MP_FLAGS

#ifdef ROUTE_MPATH
VNET_DEFINE(u_int, fib_hash_outbound) = 0;
SYSCTL_UINT(_net_route, OID_AUTO, hash_outbound, CTLFLAG_RD | CTLFLAG_VNET,
    &VNET_NAME(fib_hash_outbound), 0,
    "Compute flowid for locally-originated packets");

/* Default entropy to add to the hash calculation for the outbound connections*/
uint8_t mpath_entropy_key[MPATH_ENTROPY_KEY_LEN] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};
#endif

#if defined(INET) && defined(INET6)
FEATURE(ipv4_rfc5549_support, "Route IPv4 packets via IPv6 nexthops");
#define V_rib_route_ipv6_nexthop VNET(rib_route_ipv6_nexthop)
VNET_DEFINE_STATIC(u_int, rib_route_ipv6_nexthop) = 1;
SYSCTL_UINT(_net_route, OID_AUTO, ipv6_nexthop, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(rib_route_ipv6_nexthop), 0, "Enable IPv4 route via IPv6 Next Hop address");
#endif

/* Debug bits */
SYSCTL_NODE(_net_route, OID_AUTO, debug, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

static struct rib_head *
get_rnh(uint32_t fibnum, const struct rt_addrinfo *info)
{
	struct rib_head *rnh;
	struct sockaddr *dst;

	KASSERT((fibnum < rt_numfibs), ("rib_add_route: bad fibnum"));

	dst = info->rti_info[RTAX_DST];
	rnh = rt_tables_get_rnh(fibnum, dst->sa_family);

	return (rnh);
}

#if defined(INET) && defined(INET6)
bool
rib_can_4o6_nhop(void)
{
	return (!!V_rib_route_ipv6_nexthop);
}
#endif

#ifdef ROUTE_MPATH
static bool
rib_can_multipath(struct rib_head *rh)
{
	int result;

	CURVNET_SET(rh->rib_vnet);
	result = !!V_rib_route_multipath;
	CURVNET_RESTORE();

	return (result);
}

/*
 * Check is nhop is multipath-eligible.
 * Avoid nhops without gateways and redirects.
 *
 * Returns 1 for multipath-eligible nexthop,
 * 0 otherwise.
 */
bool
nhop_can_multipath(const struct nhop_object *nh)
{

	if ((nh->nh_flags & NHF_MULTIPATH) != 0)
		return (1);
	if ((nh->nh_flags & NHF_GATEWAY) == 0)
		return (0);
	if ((nh->nh_flags & NHF_REDIRECT) != 0)
		return (0);

	return (1);
}
#endif

static int
get_info_weight(const struct rt_addrinfo *info, uint32_t default_weight)
{
	uint32_t weight;

	if (info->rti_mflags & RTV_WEIGHT)
		weight = info->rti_rmx->rmx_weight;
	else
		weight = default_weight;
	/* Keep upper 1 byte for adm distance purposes */
	if (weight > RT_MAX_WEIGHT)
		weight = RT_MAX_WEIGHT;
	else if (weight == 0)
		weight = default_weight;

	return (weight);
}

/*
 * File-local concept for distingushing between the normal and
 * RTF_PINNED routes tha can override the "normal" one.
 */
#define	NH_PRIORITY_HIGH	2
#define	NH_PRIORITY_NORMAL	1
static int
get_prio_from_info(const struct rt_addrinfo *info)
{
	if (info->rti_flags & RTF_PINNED)
		return (NH_PRIORITY_HIGH);
	return (NH_PRIORITY_NORMAL);
}

static int
nhop_get_prio(const struct nhop_object *nh)
{
	if (NH_IS_PINNED(nh))
		return (NH_PRIORITY_HIGH);
	return (NH_PRIORITY_NORMAL);
}

/*
 * Check if specified @gw matches gw data in the nexthop @nh.
 *
 * Returns true if matches, false otherwise.
 */
bool
match_nhop_gw(const struct nhop_object *nh, const struct sockaddr *gw)
{

	if (nh->gw_sa.sa_family != gw->sa_family)
		return (false);

	switch (gw->sa_family) {
	case AF_INET:
		return (nh->gw4_sa.sin_addr.s_addr ==
		    ((const struct sockaddr_in *)gw)->sin_addr.s_addr);
	case AF_INET6:
		{
			const struct sockaddr_in6 *gw6;
			gw6 = (const struct sockaddr_in6 *)gw;

			/*
			 * Currently (2020-09) IPv6 gws in kernel have their
			 * scope embedded. Once this becomes false, this code
			 * has to be revisited.
			 */
			if (IN6_ARE_ADDR_EQUAL(&nh->gw6_sa.sin6_addr,
			    &gw6->sin6_addr))
				return (true);
			return (false);
		}
	case AF_LINK:
		{
			const struct sockaddr_dl *sdl;
			sdl = (const struct sockaddr_dl *)gw;
			return (nh->gwl_sa.sdl_index == sdl->sdl_index);
		}
	default:
		return (memcmp(&nh->gw_sa, gw, nh->gw_sa.sa_len) == 0);
	}

	/* NOTREACHED */
	return (false);
}

/*
 * Matches all nexthop with given @gw.
 * Can be used as rib_filter_f callback.
 */
int
rib_match_gw(const struct rtentry *rt, const struct nhop_object *nh, void *gw_sa)
{
	const struct sockaddr *gw = (const struct sockaddr *)gw_sa;

	return (match_nhop_gw(nh, gw));
}

struct gw_filter_data {
	const struct sockaddr *gw;
	int count;
};

/*
 * Matches first occurence of the gateway provided in @gwd
 */
static int
match_gw_one(const struct rtentry *rt, const struct nhop_object *nh, void *_data)
{
	struct gw_filter_data *gwd = (struct gw_filter_data *)_data;

	/* Return only first match to make rtsock happy */
	if (match_nhop_gw(nh, gwd->gw) && gwd->count++ == 0)
		return (1);
	return (0);
}

/*
 * Checks if data in @info matches nexhop @nh.
 *
 * Returns 0 on success,
 * ESRCH if not matched,
 * ENOENT if filter function returned false
 */
int
check_info_match_nhop(const struct rt_addrinfo *info, const struct rtentry *rt,
    const struct nhop_object *nh)
{
	const struct sockaddr *gw = info->rti_info[RTAX_GATEWAY];

	if (info->rti_filter != NULL) {
	    if (info->rti_filter(rt, nh, info->rti_filterdata) == 0)
		    return (ENOENT);
	    else
		    return (0);
	}
	if ((gw != NULL) && !match_nhop_gw(nh, gw))
		return (ESRCH);

	return (0);
}

/*
 * Runs exact prefix match based on @dst and @netmask.
 * Returns matched @rtentry if found or NULL.
 * If rtentry was found, saves nexthop / weight value into @rnd.
 */
static struct rtentry *
lookup_prefix_bysa(struct rib_head *rnh, const struct sockaddr *dst,
    const struct sockaddr *netmask, struct route_nhop_data *rnd)
{
	struct rtentry *rt;

	RIB_LOCK_ASSERT(rnh);

	rt = (struct rtentry *)rnh->rnh_lookup(dst, netmask, &rnh->head);
	if (rt != NULL) {
		rnd->rnd_nhop = rt->rt_nhop;
		rnd->rnd_weight = rt->rt_weight;
	} else {
		rnd->rnd_nhop = NULL;
		rnd->rnd_weight = 0;
	}

	return (rt);
}

struct rtentry *
lookup_prefix_rt(struct rib_head *rnh, const struct rtentry *rt,
    struct route_nhop_data *rnd)
{
	return (lookup_prefix_bysa(rnh, rt_key_const(rt), rt_mask_const(rt), rnd));
}

/*
 * Runs exact prefix match based on dst/netmask from @info.
 * Assumes RIB lock is held.
 * Returns matched @rtentry if found or NULL.
 * If rtentry was found, saves nexthop / weight value into @rnd.
 */
struct rtentry *
lookup_prefix(struct rib_head *rnh, const struct rt_addrinfo *info,
    struct route_nhop_data *rnd)
{
	struct rtentry *rt;

	rt = lookup_prefix_bysa(rnh, info->rti_info[RTAX_DST],
	    info->rti_info[RTAX_NETMASK], rnd);

	return (rt);
}

const struct rtentry *
rib_lookup_prefix_plen(struct rib_head *rnh, struct sockaddr *dst, int plen,
    struct route_nhop_data *rnd)
{
	union sockaddr_union mask_storage;
	struct sockaddr *netmask = &mask_storage.sa;

	if (fill_pxmask_family(dst->sa_family, plen, dst, &netmask))
		return (lookup_prefix_bysa(rnh, dst, netmask, rnd));
	return (NULL);
}

static bool
fill_pxmask_family(int family, int plen, struct sockaddr *_dst,
    struct sockaddr **pmask)
{
	if (plen == -1) {
		*pmask = NULL;
		return (true);
	}

	switch (family) {
#ifdef INET
	case AF_INET:
		{
			struct sockaddr_in *mask = (struct sockaddr_in *)(*pmask);
			struct sockaddr_in *dst= (struct sockaddr_in *)_dst;

			memset(mask, 0, sizeof(*mask));
			mask->sin_family = family;
			mask->sin_len = sizeof(*mask);
			if (plen == 32)
				*pmask = NULL;
			else if (plen > 32 || plen < 0)
				return (false);
			else {
				uint32_t daddr, maddr;
				maddr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0);
				mask->sin_addr.s_addr = maddr;
				daddr = dst->sin_addr.s_addr;
				daddr = htonl(ntohl(daddr) & ntohl(maddr));
				dst->sin_addr.s_addr = daddr;
			}
			return (true);
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		{
			struct sockaddr_in6 *mask = (struct sockaddr_in6 *)(*pmask);
			struct sockaddr_in6 *dst = (struct sockaddr_in6 *)_dst;

			memset(mask, 0, sizeof(*mask));
			mask->sin6_family = family;
			mask->sin6_len = sizeof(*mask);
			if (plen == 128)
				*pmask = NULL;
			else if (plen > 128 || plen < 0)
				return (false);
			else {
				ip6_writemask(&mask->sin6_addr, plen);
				IN6_MASK_ADDR(&dst->sin6_addr, &mask->sin6_addr);
			}
			return (true);
		}
		break;
#endif
	}
	return (false);
}

/*
 * Attempts to add @dst/plen prefix with nexthop/nexhopgroup data @rnd
 * to the routing table.
 *
 * @fibnum: verified kernel rtable id to insert route to
 * @dst: verified kernel-originated sockaddr, can be masked if plen non-empty
 * @plen: prefix length (or -1 if host route or not applicable for AF)
 * @op_flags: combination of RTM_F_ flags
 * @rc: storage to report operation result
 *
 * Returns 0 on success.
 */
int
rib_add_route_px(uint32_t fibnum, struct sockaddr *dst, int plen,
    struct route_nhop_data *rnd, int op_flags, struct rib_cmd_info *rc)
{
	union sockaddr_union mask_storage;
	struct sockaddr *netmask = &mask_storage.sa;
	struct rtentry *rt = NULL;

	NET_EPOCH_ASSERT();

	bzero(rc, sizeof(struct rib_cmd_info));
	rc->rc_cmd = RTM_ADD;

	struct rib_head *rnh = rt_tables_get_rnh(fibnum, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	if (!fill_pxmask_family(dst->sa_family, plen, dst, &netmask)) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: invalid plen %d", plen);
		return (EINVAL);
	}

	if (op_flags & RTM_F_CREATE) {
		if ((rt = rt_alloc(rnh, dst, netmask)) == NULL) {
			FIB_RH_LOG(LOG_INFO, rnh, "rtentry allocation failed");
			return (ENOMEM);
		}
	} else {
		struct route_nhop_data rnd_tmp;
		RIB_RLOCK_TRACKER;

		RIB_RLOCK(rnh);
		rt = lookup_prefix_bysa(rnh, dst, netmask, &rnd_tmp);
		RIB_RUNLOCK(rnh);

		if (rt == NULL)
			return (ESRCH);
	}

	return (add_route_flags(rnh, rt, rnd, op_flags, rc));
}

/*
 * Attempts to delete @dst/plen prefix matching gateway @gw from the
 *  routing rable.
 *
 * @fibnum: rtable id to remove route from
 * @dst: verified kernel-originated sockaddr, can be masked if plen non-empty
 * @plen: prefix length (or -1 if host route or not applicable for AF)
 * @gw: gateway to match
 * @op_flags: combination of RTM_F_ flags
 * @rc: storage to report operation result
 *
 * Returns 0 on success.
 */
int
rib_del_route_px_gw(uint32_t fibnum, struct sockaddr *dst, int plen,
    const struct sockaddr *gw, int op_flags, struct rib_cmd_info *rc)
{
	struct gw_filter_data gwd = { .gw = gw };

	return (rib_del_route_px(fibnum, dst, plen, match_gw_one, &gwd, op_flags, rc));
}

/*
 * Attempts to delete @dst/plen prefix matching @filter_func from the
 *  routing rable.
 *
 * @fibnum: rtable id to remove route from
 * @dst: verified kernel-originated sockaddr, can be masked if plen non-empty
 * @plen: prefix length (or -1 if host route or not applicable for AF)
 * @filter_func: func to be called for each nexthop of the prefix for matching
 * @filter_arg: argument to pass to @filter_func
 * @op_flags: combination of RTM_F_ flags
 * @rc: storage to report operation result
 *
 * Returns 0 on success.
 */
int
rib_del_route_px(uint32_t fibnum, struct sockaddr *dst, int plen,
    rib_filter_f_t *filter_func, void *filter_arg, int op_flags,
    struct rib_cmd_info *rc)
{
	union sockaddr_union mask_storage;
	struct sockaddr *netmask = &mask_storage.sa;
	int error;

	NET_EPOCH_ASSERT();

	bzero(rc, sizeof(struct rib_cmd_info));
	rc->rc_cmd = RTM_DELETE;

	struct rib_head *rnh = rt_tables_get_rnh(fibnum, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	if (dst->sa_len > sizeof(mask_storage)) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: dst->sa_len too big: %d", dst->sa_len);
		return (EINVAL);
	}

	if (!fill_pxmask_family(dst->sa_family, plen, dst, &netmask)) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: invalid plen %d", plen);
		return (EINVAL);
	}

	int prio = (op_flags & RTM_F_FORCE) ? NH_PRIORITY_HIGH : NH_PRIORITY_NORMAL;

	RIB_WLOCK(rnh);
	struct route_nhop_data rnd;
	struct rtentry *rt = lookup_prefix_bysa(rnh, dst, netmask, &rnd);
	if (rt != NULL) {
		error = rt_delete_conditional(rnh, rt, prio, filter_func,
		    filter_arg, rc);
	} else
		error = ESRCH;
	RIB_WUNLOCK(rnh);

	if (error != 0)
		return (error);

	rib_notify(rnh, RIB_NOTIFY_DELAYED, rc);

	if (rc->rc_cmd == RTM_DELETE)
		rt_free(rc->rc_rt);
#ifdef ROUTE_MPATH
	else {
		/*
		 * Deleting 1 path may result in RTM_CHANGE to
		 * a different mpath group/nhop.
		 * Free old mpath group.
		 */
		nhop_free_any(rc->rc_nh_old);
	}
#endif

	return (0);
}

/*
 * Tries to copy route @rt from one rtable to the rtable specified by @dst_rh.
 * @rt: route to copy.
 * @rnd_src: nhop and weight. Multipath routes are not supported
 * @rh_dst: target rtable.
 * @rc: operation result storage
 *
 * Return 0 on success.
 */
int
rib_copy_route(struct rtentry *rt, const struct route_nhop_data *rnd_src,
    struct rib_head *rh_dst, struct rib_cmd_info *rc)
{
	struct nhop_object __diagused *nh_src = rnd_src->rnd_nhop;
	int error;

	MPASS((nh_src->nh_flags & NHF_MULTIPATH) == 0);

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char nhbuf[NHOP_PRINT_BUFSIZE], rtbuf[NHOP_PRINT_BUFSIZE];
		nhop_print_buf_any(nh_src, nhbuf, sizeof(nhbuf));
		rt_print_buf(rt, rtbuf, sizeof(rtbuf));
		FIB_RH_LOG(LOG_DEBUG2, rh_dst, "copying %s -> %s from fib %u",
		    rtbuf, nhbuf, nhop_get_fibnum(nh_src));
	}
	struct nhop_object *nh = nhop_alloc(rh_dst->rib_fibnum, rh_dst->rib_family);
	if (nh == NULL) {
		FIB_RH_LOG(LOG_INFO, rh_dst, "unable to allocate new nexthop");
		return (ENOMEM);
	}
	nhop_copy(nh, rnd_src->rnd_nhop);
	nhop_set_origin(nh, nhop_get_origin(rnd_src->rnd_nhop));
	nhop_set_fibnum(nh, rh_dst->rib_fibnum);
	nh = nhop_get_nhop_internal(rh_dst, nh, &error);
	if (error != 0) {
		FIB_RH_LOG(LOG_INFO, rh_dst,
		    "unable to finalize new nexthop: error %d", error);
		return (ENOMEM);
	}

	struct rtentry *rt_new = rt_alloc(rh_dst, rt_key(rt), rt_mask(rt));
	if (rt_new == NULL) {
		FIB_RH_LOG(LOG_INFO, rh_dst, "unable to create new rtentry");
		nhop_free(nh);
		return (ENOMEM);
	}

	struct route_nhop_data rnd = {
		.rnd_nhop = nh,
		.rnd_weight = rnd_src->rnd_weight
	};
	int op_flags = RTM_F_CREATE | (NH_IS_PINNED(nh) ? RTM_F_FORCE : 0);
	error = add_route_flags(rh_dst, rt_new, &rnd, op_flags, rc);

	if (error != 0) {
		IF_DEBUG_LEVEL(LOG_DEBUG2) {
			char buf[NHOP_PRINT_BUFSIZE];
			rt_print_buf(rt_new, buf, sizeof(buf));
			FIB_RH_LOG(LOG_DEBUG, rh_dst,
			    "Unable to add route %s: error %d", buf, error);
		}
		nhop_free(nh);
		rt_free_immediate(rt_new);
	}
	return (error);
}

/*
 * Adds route defined by @info into the kernel table specified by @fibnum and
 * sa_family in @info->rti_info[RTAX_DST].
 *
 * Returns 0 on success and fills in operation metadata into @rc.
 */
int
rib_add_route(uint32_t fibnum, struct rt_addrinfo *info,
    struct rib_cmd_info *rc)
{
	struct rib_head *rnh;
	int error;

	NET_EPOCH_ASSERT();

	rnh = get_rnh(fibnum, info);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	/*
	 * Check consistency between RTF_HOST flag and netmask
	 * existence.
	 */
	if (info->rti_flags & RTF_HOST)
		info->rti_info[RTAX_NETMASK] = NULL;
	else if (info->rti_info[RTAX_NETMASK] == NULL) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: no RTF_HOST and empty netmask");
		return (EINVAL);
	}

	bzero(rc, sizeof(struct rib_cmd_info));
	rc->rc_cmd = RTM_ADD;

	error = add_route_byinfo(rnh, info, rc);
	if (error == 0)
		rib_notify(rnh, RIB_NOTIFY_DELAYED, rc);

	return (error);
}

static int
add_route_byinfo(struct rib_head *rnh, struct rt_addrinfo *info,
    struct rib_cmd_info *rc)
{
	struct route_nhop_data rnd_add;
	struct nhop_object *nh;
	struct rtentry *rt;
	struct sockaddr *dst, *gateway, *netmask;
	int error;

	dst = info->rti_info[RTAX_DST];
	gateway = info->rti_info[RTAX_GATEWAY];
	netmask = info->rti_info[RTAX_NETMASK];

	if ((info->rti_flags & RTF_GATEWAY) && !gateway) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: RTF_GATEWAY set with empty gw");
		return (EINVAL);
	}
	if (dst && gateway && !nhop_check_gateway(dst->sa_family, gateway->sa_family)) {
		FIB_RH_LOG(LOG_DEBUG, rnh,
		    "error: invalid dst/gateway family combination (%d, %d)",
		    dst->sa_family, gateway->sa_family);
		return (EINVAL);
	}

	if (dst->sa_len > sizeof(((struct rtentry *)NULL)->rt_dstb)) {
		FIB_RH_LOG(LOG_DEBUG, rnh, "error: dst->sa_len too large: %d",
		    dst->sa_len);
		return (EINVAL);
	}

	if (info->rti_ifa == NULL) {
		error = rt_getifa_fib(info, rnh->rib_fibnum);
		if (error)
			return (error);
	}

	if ((rt = rt_alloc(rnh, dst, netmask)) == NULL)
		return (ENOBUFS);

	error = nhop_create_from_info(rnh, info, &nh);
	if (error != 0) {
		rt_free_immediate(rt);
		return (error);
	}

	rnd_add.rnd_nhop = nh;
	rnd_add.rnd_weight = get_info_weight(info, RT_DEFAULT_WEIGHT);

	int op_flags = RTM_F_CREATE;
	if (get_prio_from_info(info) == NH_PRIORITY_HIGH)
		op_flags |= RTM_F_FORCE;
	else
		op_flags |= RTM_F_APPEND;
	return (add_route_flags(rnh, rt, &rnd_add, op_flags, rc));

}

static int
add_route_flags(struct rib_head *rnh, struct rtentry *rt, struct route_nhop_data *rnd_add,
    int op_flags, struct rib_cmd_info *rc)
{
	struct route_nhop_data rnd_orig;
	struct nhop_object *nh;
	struct rtentry *rt_orig;
	int error = 0;

	MPASS(rt != NULL);

	nh = rnd_add->rnd_nhop;

	RIB_WLOCK(rnh);

	rt_orig = lookup_prefix_rt(rnh, rt, &rnd_orig);

	if (rt_orig == NULL) {
		if (op_flags & RTM_F_CREATE)
			error = add_route(rnh, rt, rnd_add, rc);
		else
			error = ESRCH; /* no entry but creation was not required */
		RIB_WUNLOCK(rnh);
		if (error != 0)
			goto out;
		return (0);
	}

	if (op_flags & RTM_F_EXCL) {
		/* We have existing route in the RIB but not allowed to replace. */
		RIB_WUNLOCK(rnh);
		error = EEXIST;
		goto out;
	}

	/* Now either append or replace */
	if (op_flags & RTM_F_REPLACE) {
		if (nhop_get_prio(rnd_orig.rnd_nhop) > nhop_get_prio(rnd_add->rnd_nhop)) {
			/* Old path is "better" (e.g. has PINNED flag set) */
			RIB_WUNLOCK(rnh);
			error = EEXIST;
			goto out;
		}
		change_route(rnh, rt_orig, rnd_add, rc);
		RIB_WUNLOCK(rnh);
		nh = rc->rc_nh_old;
		goto out;
	}

	RIB_WUNLOCK(rnh);

#ifdef ROUTE_MPATH
	if ((op_flags & RTM_F_APPEND) && rib_can_multipath(rnh) &&
	    nhop_can_multipath(rnd_add->rnd_nhop) &&
	    nhop_can_multipath(rnd_orig.rnd_nhop)) {

		for (int i = 0; i < RIB_MAX_RETRIES; i++) {
			error = add_route_flags_mpath(rnh, rt_orig, rnd_add, &rnd_orig,
			    op_flags, rc);
			if (error != EAGAIN)
				break;
			RTSTAT_INC(rts_add_retry);
		}

		/*
		 *  Original nhop reference is unused in any case.
		 */
		nhop_free_any(rnd_add->rnd_nhop);
		if (op_flags & RTM_F_CREATE) {
			if (error != 0 || rc->rc_cmd != RTM_ADD)
				rt_free_immediate(rt);
		}
		return (error);
	}
#endif
	/* Out of options - free state and return error */
	error = EEXIST;
out:
	if (op_flags & RTM_F_CREATE)
		rt_free_immediate(rt);
	nhop_free_any(nh);

	return (error);
}

#ifdef ROUTE_MPATH
static int
add_route_flags_mpath(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd_add, struct route_nhop_data *rnd_orig,
    int op_flags, struct rib_cmd_info *rc)
{
	RIB_RLOCK_TRACKER;
	struct route_nhop_data rnd_new;
	int error = 0;

	error = nhgrp_get_addition_group(rnh, rnd_orig, rnd_add, &rnd_new);
	if (error != 0) {
		if (error == EAGAIN) {
			/*
			 * Group creation failed, most probably because
			 * @rnd_orig data got scheduled for deletion.
			 * Refresh @rnd_orig data and retry.
			 */
			RIB_RLOCK(rnh);
			lookup_prefix_rt(rnh, rt, rnd_orig);
			RIB_RUNLOCK(rnh);
			if (rnd_orig == NULL && !(op_flags & RTM_F_CREATE)) {
				/* In this iteration route doesn't exist */
				error = ENOENT;
			}
		}
		return (error);
	}
	error = change_route_conditional(rnh, rt, rnd_orig, &rnd_new, rc);
	if (error != 0)
		return (error);

	if (V_fib_hash_outbound == 0 && NH_IS_NHGRP(rc->rc_nh_new)) {
		/*
		 * First multipath route got installed. Enable local
		 * outbound connections hashing.
		 */
		if (bootverbose)
			printf("FIB: enabled flowid calculation for locally-originated packets\n");
		V_fib_hash_outbound = 1;
	}

	return (0);
}
#endif

/*
 * Removes route defined by @info from the kernel table specified by @fibnum and
 * sa_family in @info->rti_info[RTAX_DST].
 *
 * Returns 0 on success and fills in operation metadata into @rc.
 */
int
rib_del_route(uint32_t fibnum, struct rt_addrinfo *info, struct rib_cmd_info *rc)
{
	struct rib_head *rnh;
	struct sockaddr *dst, *netmask;
	struct sockaddr_storage mdst;
	int error;

	NET_EPOCH_ASSERT();

	rnh = get_rnh(fibnum, info);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	bzero(rc, sizeof(struct rib_cmd_info));
	rc->rc_cmd = RTM_DELETE;

	dst = info->rti_info[RTAX_DST];
	netmask = info->rti_info[RTAX_NETMASK];

	if (netmask != NULL) {
		/* Ensure @dst is always properly masked */
		if (dst->sa_len > sizeof(mdst)) {
			FIB_RH_LOG(LOG_DEBUG, rnh, "error: dst->sa_len too large");
			return (EINVAL);
		}
		rt_maskedcopy(dst, (struct sockaddr *)&mdst, netmask);
		dst = (struct sockaddr *)&mdst;
	}

	rib_filter_f_t *filter_func = NULL;
	void *filter_arg = NULL;
	struct gw_filter_data gwd = { .gw = info->rti_info[RTAX_GATEWAY] };

	if (info->rti_filter != NULL) {
		filter_func = info->rti_filter;
		filter_arg = info->rti_filterdata;
	} else if (gwd.gw != NULL) {
		filter_func = match_gw_one;
		filter_arg = &gwd;
	}

	int prio = get_prio_from_info(info);

	RIB_WLOCK(rnh);
	struct route_nhop_data rnd;
	struct rtentry *rt = lookup_prefix_bysa(rnh, dst, netmask, &rnd);
	if (rt != NULL) {
		error = rt_delete_conditional(rnh, rt, prio, filter_func,
		    filter_arg, rc);
	} else
		error = ESRCH;
	RIB_WUNLOCK(rnh);

	if (error != 0)
		return (error);

	rib_notify(rnh, RIB_NOTIFY_DELAYED, rc);

	if (rc->rc_cmd == RTM_DELETE)
		rt_free(rc->rc_rt);
#ifdef ROUTE_MPATH
	else {
		/*
		 * Deleting 1 path may result in RTM_CHANGE to
		 * a different mpath group/nhop.
		 * Free old mpath group.
		 */
		nhop_free_any(rc->rc_nh_old);
	}
#endif

	return (0);
}

/*
 * Conditionally unlinks rtentry paths from @rnh matching @cb.
 * Returns 0 on success with operation result stored in @rc.
 * On error, returns:
 * ESRCH - if prefix was not found or filter function failed to match
 * EADDRINUSE - if trying to delete higher priority route.
 */
static int
rt_delete_conditional(struct rib_head *rnh, struct rtentry *rt,
    int prio, rib_filter_f_t *cb, void *cbdata, struct rib_cmd_info *rc)
{
	struct nhop_object *nh = rt->rt_nhop;

#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		struct nhgrp_object *nhg = (struct nhgrp_object *)nh;
		struct route_nhop_data rnd;
		int error;

		if (cb == NULL)
			return (ESRCH);
		error = nhgrp_get_filtered_group(rnh, rt, nhg, cb, cbdata, &rnd);
		if (error == 0) {
			if (rnd.rnd_nhgrp == nhg) {
				/* No match, unreference new group and return. */
				nhop_free_any(rnd.rnd_nhop);
				return (ESRCH);
			}
			error = change_route(rnh, rt, &rnd, rc);
		}
		return (error);
	}
#endif
	if (cb != NULL && !cb(rt, nh, cbdata))
		return (ESRCH);

	if (prio < nhop_get_prio(nh))
		return (EADDRINUSE);

	return (delete_route(rnh, rt, rc));
}

int
rib_change_route(uint32_t fibnum, struct rt_addrinfo *info,
    struct rib_cmd_info *rc)
{
	RIB_RLOCK_TRACKER;
	struct route_nhop_data rnd_orig;
	struct rib_head *rnh;
	struct rtentry *rt;
	int error;

	NET_EPOCH_ASSERT();

	rnh = get_rnh(fibnum, info);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	bzero(rc, sizeof(struct rib_cmd_info));
	rc->rc_cmd = RTM_CHANGE;

	/* Check if updated gateway exists */
	if ((info->rti_flags & RTF_GATEWAY) &&
	    (info->rti_info[RTAX_GATEWAY] == NULL)) {

		/*
		 * route(8) adds RTF_GATEWAY flag if -interface is not set.
		 * Remove RTF_GATEWAY to enforce consistency and maintain
		 * compatibility..
		 */
		info->rti_flags &= ~RTF_GATEWAY;
	}

	/*
	 * route change is done in multiple steps, with dropping and
	 * reacquiring lock. In the situations with multiple processes
	 * changes the same route in can lead to the case when route
	 * is changed between the steps. Address it by retrying the operation
	 * multiple times before failing.
	 */

	RIB_RLOCK(rnh);
	rt = (struct rtentry *)rnh->rnh_lookup(info->rti_info[RTAX_DST],
	    info->rti_info[RTAX_NETMASK], &rnh->head);

	if (rt == NULL) {
		RIB_RUNLOCK(rnh);
		return (ESRCH);
	}

	rnd_orig.rnd_nhop = rt->rt_nhop;
	rnd_orig.rnd_weight = rt->rt_weight;

	RIB_RUNLOCK(rnh);

	for (int i = 0; i < RIB_MAX_RETRIES; i++) {
		error = change_route_byinfo(rnh, rt, info, &rnd_orig, rc);
		if (error != EAGAIN)
			break;
	}

	return (error);
}

static int
change_nhop(struct rib_head *rnh, struct rt_addrinfo *info,
    struct nhop_object *nh_orig, struct nhop_object **nh_new)
{
	int error;

	/*
	 * New gateway could require new ifaddr, ifp;
	 * flags may also be different; ifp may be specified
	 * by ll sockaddr when protocol address is ambiguous
	 */
	if (((nh_orig->nh_flags & NHF_GATEWAY) &&
	    info->rti_info[RTAX_GATEWAY] != NULL) ||
	    info->rti_info[RTAX_IFP] != NULL ||
	    (info->rti_info[RTAX_IFA] != NULL &&
	     !sa_equal(info->rti_info[RTAX_IFA], nh_orig->nh_ifa->ifa_addr))) {
		error = rt_getifa_fib(info, rnh->rib_fibnum);

		if (error != 0) {
			info->rti_ifa = NULL;
			return (error);
		}
	}

	error = nhop_create_from_nhop(rnh, nh_orig, info, nh_new);
	info->rti_ifa = NULL;

	return (error);
}

#ifdef ROUTE_MPATH
static int
change_mpath_route(struct rib_head *rnh, struct rtentry *rt,
    struct rt_addrinfo *info, struct route_nhop_data *rnd_orig,
    struct rib_cmd_info *rc)
{
	int error = 0, found_idx = 0;
	struct nhop_object *nh_orig = NULL, *nh_new;
	struct route_nhop_data rnd_new = {};
	const struct weightened_nhop *wn = NULL;
	struct weightened_nhop *wn_new;
	uint32_t num_nhops;

	wn = nhgrp_get_nhops(rnd_orig->rnd_nhgrp, &num_nhops);
	for (int i = 0; i < num_nhops; i++) {
		if (check_info_match_nhop(info, NULL, wn[i].nh) == 0) {
			nh_orig = wn[i].nh;
			found_idx = i;
			break;
		}
	}

	if (nh_orig == NULL)
		return (ESRCH);

	error = change_nhop(rnh, info, nh_orig, &nh_new);
	if (error != 0)
		return (error);

	wn_new = mallocarray(num_nhops, sizeof(struct weightened_nhop),
	    M_TEMP, M_NOWAIT | M_ZERO);
	if (wn_new == NULL) {
		nhop_free(nh_new);
		return (EAGAIN);
	}

	memcpy(wn_new, wn, num_nhops * sizeof(struct weightened_nhop));
	wn_new[found_idx].nh = nh_new;
	wn_new[found_idx].weight = get_info_weight(info, wn[found_idx].weight);

	error = nhgrp_get_group(rnh, wn_new, num_nhops, 0, &rnd_new.rnd_nhgrp);
	nhop_free(nh_new);
	free(wn_new, M_TEMP);

	if (error != 0)
		return (error);

	error = change_route_conditional(rnh, rt, rnd_orig, &rnd_new, rc);

	return (error);
}
#endif

static int
change_route_byinfo(struct rib_head *rnh, struct rtentry *rt,
    struct rt_addrinfo *info, struct route_nhop_data *rnd_orig,
    struct rib_cmd_info *rc)
{
	int error = 0;
	struct nhop_object *nh_orig;
	struct route_nhop_data rnd_new;

	nh_orig = rnd_orig->rnd_nhop;
	if (nh_orig == NULL)
		return (ESRCH);

#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh_orig))
		return (change_mpath_route(rnh, rt, info, rnd_orig, rc));
#endif

	rnd_new.rnd_weight = get_info_weight(info, rnd_orig->rnd_weight);
	error = change_nhop(rnh, info, nh_orig, &rnd_new.rnd_nhop);
	if (error != 0)
		return (error);
	error = change_route_conditional(rnh, rt, rnd_orig, &rnd_new, rc);

	return (error);
}

/*
 * Insert @rt with nhop data from @rnd_new to @rnh.
 * Returns 0 on success and stores operation results in @rc.
 */
static int
add_route(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd, struct rib_cmd_info *rc)
{
	struct radix_node *rn;

	RIB_WLOCK_ASSERT(rnh);

	rt->rt_nhop = rnd->rnd_nhop;
	rt->rt_weight = rnd->rnd_weight;
	rn = rnh->rnh_addaddr(rt_key(rt), rt_mask_const(rt), &rnh->head, rt->rt_nodes);

	if (rn != NULL) {
		if (!NH_IS_NHGRP(rnd->rnd_nhop) && nhop_get_expire(rnd->rnd_nhop))
			tmproutes_update(rnh, rt, rnd->rnd_nhop);

		/* Finalize notification */
		rib_bump_gen(rnh);
		rnh->rnh_prefixes++;

		rc->rc_cmd = RTM_ADD;
		rc->rc_rt = rt;
		rc->rc_nh_old = NULL;
		rc->rc_nh_new = rnd->rnd_nhop;
		rc->rc_nh_weight = rnd->rnd_weight;

		rib_notify(rnh, RIB_NOTIFY_IMMEDIATE, rc);
		return (0);
	}

	/* Existing route or memory allocation failure. */
	return (EEXIST);
}

/*
 * Unconditionally deletes @rt from @rnh.
 */
static int
delete_route(struct rib_head *rnh, struct rtentry *rt, struct rib_cmd_info *rc)
{
	RIB_WLOCK_ASSERT(rnh);

	/* Route deletion requested. */
	struct radix_node *rn;

	rn = rnh->rnh_deladdr(rt_key_const(rt), rt_mask_const(rt), &rnh->head);
	if (rn == NULL)
		return (ESRCH);
	rt = RNTORT(rn);
	rt->rte_flags &= ~RTF_UP;

	rib_bump_gen(rnh);
	rnh->rnh_prefixes--;

	rc->rc_cmd = RTM_DELETE;
	rc->rc_rt = rt;
	rc->rc_nh_old = rt->rt_nhop;
	rc->rc_nh_new = NULL;
	rc->rc_nh_weight = rt->rt_weight;

	rib_notify(rnh, RIB_NOTIFY_IMMEDIATE, rc);

	return (0);
}

/*
 * Switch @rt nhop/weigh to the ones specified in @rnd.
 * Returns 0 on success.
 */
int
change_route(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd, struct rib_cmd_info *rc)
{
	struct nhop_object *nh_orig;

	RIB_WLOCK_ASSERT(rnh);

	nh_orig = rt->rt_nhop;

	if (rnd->rnd_nhop == NULL)
		return (delete_route(rnh, rt, rc));

	/* Changing nexthop & weight to a new one */
	rt->rt_nhop = rnd->rnd_nhop;
	rt->rt_weight = rnd->rnd_weight;
	if (!NH_IS_NHGRP(rnd->rnd_nhop) && nhop_get_expire(rnd->rnd_nhop))
		tmproutes_update(rnh, rt, rnd->rnd_nhop);

	/* Finalize notification */
	rib_bump_gen(rnh);
	rc->rc_cmd = RTM_CHANGE;
	rc->rc_rt = rt;
	rc->rc_nh_old = nh_orig;
	rc->rc_nh_new = rnd->rnd_nhop;
	rc->rc_nh_weight = rnd->rnd_weight;

	rib_notify(rnh, RIB_NOTIFY_IMMEDIATE, rc);

	return (0);
}

/*
 * Conditionally update route nhop/weight IFF data in @nhd_orig is
 *  consistent with the current route data.
 * Nexthop in @nhd_new is consumed.
 */
int
change_route_conditional(struct rib_head *rnh, struct rtentry *rt,
    struct route_nhop_data *rnd_orig, struct route_nhop_data *rnd_new,
    struct rib_cmd_info *rc)
{
	struct rtentry *rt_new;
	int error = 0;

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char buf_old[NHOP_PRINT_BUFSIZE], buf_new[NHOP_PRINT_BUFSIZE];
		nhop_print_buf_any(rnd_orig->rnd_nhop, buf_old, NHOP_PRINT_BUFSIZE);
		nhop_print_buf_any(rnd_new->rnd_nhop, buf_new, NHOP_PRINT_BUFSIZE);
		FIB_LOG(LOG_DEBUG2, rnh->rib_fibnum, rnh->rib_family,
		    "trying change %s -> %s", buf_old, buf_new);
	}
	RIB_WLOCK(rnh);

	struct route_nhop_data rnd;
	rt_new = lookup_prefix_rt(rnh, rt, &rnd);

	if (rt_new == NULL) {
		if (rnd_orig->rnd_nhop == NULL)
			error = add_route(rnh, rt, rnd_new, rc);
		else {
			/*
			 * Prefix does not exist, which was not our assumption.
			 * Update @rnd_orig with the new data and return
			 */
			rnd_orig->rnd_nhop = NULL;
			rnd_orig->rnd_weight = 0;
			error = EAGAIN;
		}
	} else {
		/* Prefix exists, try to update */
		if (rnd_orig->rnd_nhop == rt_new->rt_nhop) {
			/*
			 * Nhop/mpath group hasn't changed. Flip
			 * to the new precalculated one and return
			 */
			error = change_route(rnh, rt_new, rnd_new, rc);
		} else {
			/* Update and retry */
			rnd_orig->rnd_nhop = rt_new->rt_nhop;
			rnd_orig->rnd_weight = rt_new->rt_weight;
			error = EAGAIN;
		}
	}

	RIB_WUNLOCK(rnh);

	if (error == 0) {
		rib_notify(rnh, RIB_NOTIFY_DELAYED, rc);

		if (rnd_orig->rnd_nhop != NULL)
			nhop_free_any(rnd_orig->rnd_nhop);

	} else {
		if (rnd_new->rnd_nhop != NULL)
			nhop_free_any(rnd_new->rnd_nhop);
	}

	return (error);
}

/*
 * Performs modification of routing table specificed by @action.
 * Table is specified by @fibnum and sa_family in @info->rti_info[RTAX_DST].
 * Needs to be run in network epoch.
 *
 * Returns 0 on success and fills in @rc with action result.
 */
int
rib_action(uint32_t fibnum, int action, struct rt_addrinfo *info,
    struct rib_cmd_info *rc)
{
	int error;

	switch (action) {
	case RTM_ADD:
		error = rib_add_route(fibnum, info, rc);
		break;
	case RTM_DELETE:
		error = rib_del_route(fibnum, info, rc);
		break;
	case RTM_CHANGE:
		error = rib_change_route(fibnum, info, rc);
		break;
	default:
		error = ENOTSUP;
	}

	return (error);
}

struct rt_delinfo
{
	struct rib_head *rnh;
	struct rtentry *head;
	rib_filter_f_t *filter_f;
	void *filter_arg;
	int prio;
	struct rib_cmd_info rc;
};

/*
 * Conditionally unlinks rtenties or paths from radix tree based
 * on the callback data passed in @arg.
 */
static int
rt_checkdelroute(struct radix_node *rn, void *arg)
{
	struct rt_delinfo *di = (struct rt_delinfo *)arg;
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt_delete_conditional(di->rnh, rt, di->prio,
	    di->filter_f, di->filter_arg, &di->rc) != 0)
		return (0);

	/*
	 * Add deleted rtentries to the list to GC them
	 *  after dropping the lock.
	 *
	 * XXX: Delayed notifications not implemented
	 *  for nexthop updates.
	 */
	if (di->rc.rc_cmd == RTM_DELETE) {
		/* Add to the list and return */
		rt->rt_chain = di->head;
		di->head = rt;
#ifdef ROUTE_MPATH
	} else {
		/*
		 * RTM_CHANGE to a different nexthop or nexthop group.
		 * Free old multipath group.
		 */
		nhop_free_any(di->rc.rc_nh_old);
#endif
	}

	return (0);
}

/*
 * Iterates over a routing table specified by @fibnum and @family and
 *  deletes elements marked by @filter_f.
 * @fibnum: rtable id
 * @family: AF_ address family
 * @filter_f: function returning non-zero value for items to delete
 * @arg: data to pass to the @filter_f function
 * @report: true if rtsock notification is needed.
 */
void
rib_walk_del(u_int fibnum, int family, rib_filter_f_t *filter_f, void *filter_arg,
    bool report)
{
	struct rib_head *rnh;
	struct rtentry *rt;
	struct nhop_object *nh;
	struct epoch_tracker et;

	rnh = rt_tables_get_rnh(fibnum, family);
	if (rnh == NULL)
		return;

	struct rt_delinfo di = {
		.rnh = rnh,
		.filter_f = filter_f,
		.filter_arg = filter_arg,
		.prio = NH_PRIORITY_NORMAL,
	};

	NET_EPOCH_ENTER(et);

	RIB_WLOCK(rnh);
	rnh->rnh_walktree(&rnh->head, rt_checkdelroute, &di);
	RIB_WUNLOCK(rnh);

	/* We might have something to reclaim. */
	bzero(&di.rc, sizeof(di.rc));
	di.rc.rc_cmd = RTM_DELETE;
	while (di.head != NULL) {
		rt = di.head;
		di.head = rt->rt_chain;
		rt->rt_chain = NULL;
		nh = rt->rt_nhop;

		di.rc.rc_rt = rt;
		di.rc.rc_nh_old = nh;
		rib_notify(rnh, RIB_NOTIFY_DELAYED, &di.rc);

		if (report) {
#ifdef ROUTE_MPATH
			struct nhgrp_object *nhg;
			const struct weightened_nhop *wn;
			uint32_t num_nhops;
			if (NH_IS_NHGRP(nh)) {
				nhg = (struct nhgrp_object *)nh;
				wn = nhgrp_get_nhops(nhg, &num_nhops);
				for (int i = 0; i < num_nhops; i++)
					rt_routemsg(RTM_DELETE, rt, wn[i].nh, fibnum);
			} else
#endif
			rt_routemsg(RTM_DELETE, rt, nh, fibnum);
		}
		rt_free(rt);
	}

	NET_EPOCH_EXIT(et);
}

static int
rt_delete_unconditional(struct radix_node *rn, void *arg)
{
	struct rtentry *rt = RNTORT(rn);
	struct rib_head *rnh = (struct rib_head *)arg;

	rn = rnh->rnh_deladdr(rt_key(rt), rt_mask(rt), &rnh->head);
	if (RNTORT(rn) == rt)
		rt_free(rt);

	return (0);
}

/*
 * Removes all routes from the routing table without executing notifications.
 * rtentres will be removed after the end of a current epoch.
 */
static void
rib_flush_routes(struct rib_head *rnh)
{
	RIB_WLOCK(rnh);
	rnh->rnh_walktree(&rnh->head, rt_delete_unconditional, rnh);
	RIB_WUNLOCK(rnh);
}

void
rib_flush_routes_family(int family)
{
	struct rib_head *rnh;

	for (uint32_t fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		if ((rnh = rt_tables_get_rnh(fibnum, family)) != NULL)
			rib_flush_routes(rnh);
	}
}

const char *
rib_print_family(int family)
{
	switch (family) {
	case AF_INET:
		return ("inet");
	case AF_INET6:
		return ("inet6");
	case AF_LINK:
		return ("link");
	}
	return ("unknown");
}

