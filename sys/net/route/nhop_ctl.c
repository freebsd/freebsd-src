/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/epoch.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#include <net/vnet.h>

#define	DEBUG_MOD_NAME	nhop_ctl
#define	DEBUG_MAX_LEVEL	LOG_DEBUG
#include <net/route/route_debug.h>
_DECLARE_DEBUG(LOG_INFO);

/*
 * This file contains core functionality for the nexthop ("nhop") route subsystem.
 * The business logic needed to create nexhop objects is implemented here.
 *
 * Nexthops in the original sense are the objects containing all the necessary
 * information to forward the packet to the selected destination.
 * In particular, nexthop is defined by a combination of
 *  ifp, ifa, aifp, mtu, gw addr(if set), nh_type, nh_upper_family, mask of rt_flags and
 *    NHF_DEFAULT
 *
 * Additionally, each nexthop gets assigned its unique index (nexthop index).
 * It serves two purposes: first one is to ease the ability of userland programs to
 *  reference nexthops by their index. The second one allows lookup algorithms to
 *  to store index instead of pointer (2 bytes vs 8) as a lookup result.
 * All nexthops are stored in the resizable hash table.
 *
 * Basically, this file revolves around supporting 3 functions:
 * 1) nhop_create_from_info / nhop_create_from_nhop, which contains all
 *  business logic on filling the nexthop fields based on the provided request.
 * 2) nhop_get(), which gets a usable referenced nexthops.
 *
 * Conventions:
 * 1) non-exported functions start with verb
 * 2) exported function starts with the subsystem prefix: "nhop"
 */

static int dump_nhop_entry(struct rib_head *rh, struct nhop_object *nh, struct sysctl_req *w);

static struct nhop_priv *alloc_nhop_structure(void);
static int get_nhop(struct rib_head *rnh, struct rt_addrinfo *info,
    struct nhop_priv **pnh_priv);
static int finalize_nhop(struct nh_control *ctl, struct rt_addrinfo *info,
    struct nhop_priv *nh_priv);
static struct ifnet *get_aifp(const struct nhop_object *nh);
static void fill_sdl_from_ifp(struct sockaddr_dl_short *sdl, const struct ifnet *ifp);

static void destroy_nhop_epoch(epoch_context_t ctx);
static void destroy_nhop(struct nhop_priv *nh_priv);

_Static_assert(__offsetof(struct nhop_object, nh_ifp) == 32,
    "nhop_object: wrong nh_ifp offset");
_Static_assert(sizeof(struct nhop_object) <= 128,
    "nhop_object: size exceeds 128 bytes");

static uma_zone_t nhops_zone;	/* Global zone for each and every nexthop */

#define	NHOP_OBJECT_ALIGNED_SIZE	roundup2(sizeof(struct nhop_object), \
							2 * CACHE_LINE_SIZE)
#define	NHOP_PRIV_ALIGNED_SIZE		roundup2(sizeof(struct nhop_priv), \
							2 * CACHE_LINE_SIZE)
void
nhops_init(void)
{

	nhops_zone = uma_zcreate("routing nhops",
	    NHOP_OBJECT_ALIGNED_SIZE + NHOP_PRIV_ALIGNED_SIZE,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

/*
 * Fetches the interface of source address used by the route.
 * In all cases except interface-address-route it would be the
 * same as the transmit interfaces.
 * However, for the interface address this function will return
 * this interface ifp instead of loopback. This is needed to support
 * link-local IPv6 loopback communications.
 *
 * Returns found ifp.
 */
static struct ifnet *
get_aifp(const struct nhop_object *nh)
{
	struct ifnet *aifp = NULL;

	/*
	 * Adjust the "outgoing" interface.  If we're going to loop
	 * the packet back to ourselves, the ifp would be the loopback
	 * interface. However, we'd rather know the interface associated
	 * to the destination address (which should probably be one of
	 * our own addresses).
	 */
	if ((nh->nh_ifp->if_flags & IFF_LOOPBACK) &&
			nh->gw_sa.sa_family == AF_LINK) {
		aifp = ifnet_byindex(nh->gwl_sa.sdl_index);
		if (aifp == NULL) {
			FIB_NH_LOG(LOG_WARNING, nh, "unable to get aifp for %s index %d",
				if_name(nh->nh_ifp), nh->gwl_sa.sdl_index);
		}
	}

	if (aifp == NULL)
		aifp = nh->nh_ifp;

	return (aifp);
}

int
cmp_priv(const struct nhop_priv *_one, const struct nhop_priv *_two)
{

	if (memcmp(_one->nh, _two->nh, NHOP_END_CMP) != 0)
		return (0);

	if (memcmp(_one, _two, NH_PRIV_END_CMP) != 0)
		return (0);

	return (1);
}

/*
 * Conditionally sets @nh mtu data based on the @info data.
 */
static void
set_nhop_mtu_from_info(struct nhop_object *nh, const struct rt_addrinfo *info)
{

	if (info->rti_mflags & RTV_MTU) {
		if (info->rti_rmx->rmx_mtu != 0) {
			/*
			 * MTU was explicitly provided by user.
			 * Keep it.
			 */

			nh->nh_priv->rt_flags |= RTF_FIXEDMTU;
		} else {
			/*
			 * User explicitly sets MTU to 0.
			 * Assume rollback to default.
			 */
			nh->nh_priv->rt_flags &= ~RTF_FIXEDMTU;
		}
		nh->nh_mtu = info->rti_rmx->rmx_mtu;
	}
}

/*
 * Fills in shorted link-level sockadd version suitable to be stored inside the
 *  nexthop gateway buffer.
 */
static void
fill_sdl_from_ifp(struct sockaddr_dl_short *sdl, const struct ifnet *ifp)
{

	bzero(sdl, sizeof(struct sockaddr_dl_short));
	sdl->sdl_family = AF_LINK;
	sdl->sdl_len = sizeof(struct sockaddr_dl_short);
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;
}

static int
set_nhop_gw_from_info(struct nhop_object *nh, struct rt_addrinfo *info)
{
	struct sockaddr *gw;

	gw = info->rti_info[RTAX_GATEWAY];
	KASSERT(gw != NULL, ("gw is NULL"));

	if ((gw->sa_family == AF_LINK) && !(info->rti_flags & RTF_GATEWAY)) {

		/*
		 * Interface route with interface specified by the interface
		 * index in sockadd_dl structure. It is used in the IPv6 loopback
		 * output code, where we need to preserve the original interface
		 * to maintain proper scoping.
		 * Despite the fact that nexthop code stores original interface
		 * in the separate field (nh_aifp, see below), write AF_LINK
		 * compatible sa with shorter total length.
		 */
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)gw;
		struct ifnet *ifp = ifnet_byindex(sdl->sdl_index);
		if (ifp == NULL) {
			FIB_NH_LOG(LOG_WARNING, nh, "invalid ifindex %d", sdl->sdl_index);
			return (EINVAL);
		}
		fill_sdl_from_ifp(&nh->gwl_sa, ifp);
	} else {

		/*
		 * Multiple options here:
		 *
		 * 1) RTF_GATEWAY with IPv4/IPv6 gateway data
		 * 2) Interface route with IPv4/IPv6 address of the
		 *   matching interface. Some routing daemons do that
		 *   instead of specifying ifindex in AF_LINK.
		 *
		 * In both cases, save the original nexthop to make the callers
		 *   happy.
		 */
		if (gw->sa_len > sizeof(struct sockaddr_in6)) {
			FIB_NH_LOG(LOG_WARNING, nh, "nhop SA size too big: AF %d len %u",
			    gw->sa_family, gw->sa_len);
			return (ENOMEM);
		}
		memcpy(&nh->gw_sa, gw, gw->sa_len);
	}
	return (0);
}

static uint16_t
convert_rt_to_nh_flags(int rt_flags)
{
	uint16_t res;

	res = (rt_flags & RTF_REJECT) ? NHF_REJECT : 0;
	res |= (rt_flags & RTF_HOST) ? NHF_HOST : 0;
	res |= (rt_flags & RTF_BLACKHOLE) ? NHF_BLACKHOLE : 0;
	res |= (rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) ? NHF_REDIRECT : 0;
	res |= (rt_flags & RTF_BROADCAST) ? NHF_BROADCAST : 0;
	res |= (rt_flags & RTF_GATEWAY) ? NHF_GATEWAY : 0;

	return (res);
}

static int
fill_nhop_from_info(struct nhop_priv *nh_priv, struct rt_addrinfo *info)
{
	int error, rt_flags;
	struct nhop_object *nh;

	nh = nh_priv->nh;

	rt_flags = info->rti_flags & NHOP_RT_FLAG_MASK;

	nh->nh_priv->rt_flags = rt_flags;
	nh_priv->nh_upper_family = info->rti_info[RTAX_DST]->sa_family;
	nh_priv->nh_type = 0; // hook responsibility to set nhop type
	nh->nh_flags = convert_rt_to_nh_flags(rt_flags);

	set_nhop_mtu_from_info(nh, info);
	if ((error = set_nhop_gw_from_info(nh, info)) != 0)
		return (error);
	if (nh->gw_sa.sa_family == AF_LINK)
		nh_priv->nh_neigh_family = nh_priv->nh_upper_family;
	else
		nh_priv->nh_neigh_family = nh->gw_sa.sa_family;

	nh->nh_ifp = (info->rti_ifp != NULL) ? info->rti_ifp : info->rti_ifa->ifa_ifp;
	nh->nh_ifa = info->rti_ifa;
	/* depends on the gateway */
	nh->nh_aifp = get_aifp(nh);

	/*
	 * Note some of the remaining data is set by the
	 * per-address-family pre-add hook.
	 */

	return (0);
}

/*
 * Creates a new nexthop based on the information in @info.
 *
 * Returns:
 * 0 on success, filling @nh_ret with the desired nexthop object ptr
 * errno otherwise
 */
int
nhop_create_from_info(struct rib_head *rnh, struct rt_addrinfo *info,
    struct nhop_object **nh_ret)
{
	struct nhop_priv *nh_priv;
	int error;

	NET_EPOCH_ASSERT();

	if (info->rti_info[RTAX_GATEWAY] == NULL)
		return (EINVAL);

	nh_priv = alloc_nhop_structure();

	error = fill_nhop_from_info(nh_priv, info);
	if (error != 0) {
		uma_zfree(nhops_zone, nh_priv->nh);
		return (error);
	}

	error = get_nhop(rnh, info, &nh_priv);
	if (error == 0)
		*nh_ret = nh_priv->nh;

	return (error);
}

/*
 * Gets linked nhop using the provided @pnh_priv nexhop data.
 * If linked nhop is found, returns it, freeing the provided one.
 * If there is no such nexthop, attaches the remaining data to the
 *  provided nexthop and links it.
 *
 * Returns 0 on success, storing referenced nexthop in @pnh_priv.
 * Otherwise, errno is returned.
 */
static int
get_nhop(struct rib_head *rnh, struct rt_addrinfo *info,
    struct nhop_priv **pnh_priv)
{
	const struct sockaddr *dst, *gateway, *netmask;
	struct nhop_priv *nh_priv, *tmp_priv;
	int error;

	nh_priv = *pnh_priv;

	/* Give the protocols chance to augment the request data */
	dst = info->rti_info[RTAX_DST];
	netmask = info->rti_info[RTAX_NETMASK];
	gateway = info->rti_info[RTAX_GATEWAY];

	error = rnh->rnh_preadd(rnh->rib_fibnum, dst, netmask, nh_priv->nh);
	if (error != 0) {
		uma_zfree(nhops_zone, nh_priv->nh);
		return (error);
	}

	tmp_priv = find_nhop(rnh->nh_control, nh_priv);
	if (tmp_priv != NULL) {
		uma_zfree(nhops_zone, nh_priv->nh);
		*pnh_priv = tmp_priv;
		return (0);
	}

	/*
	 * Existing nexthop not found, need to create new one.
	 * Note: multiple simultaneous get_nhop() requests
	 *  can result in multiple equal nexhops existing in the
	 *  nexthop table. This is not a not a problem until the
	 *  relative number of such nexthops is significant, which
	 *  is extremely unlikely.
	 */

	error = finalize_nhop(rnh->nh_control, info, nh_priv);
	if (error != 0)
		return (error);

	return (0);
}

/*
 * Update @nh with data supplied in @info.
 * This is a helper function to support route changes.
 *
 * It limits the changes that can be done to the route to the following:
 * 1) all combination of gateway changes (gw, interface, blackhole/reject)
 * 2) route flags (FLAG[123],STATIC,BLACKHOLE,REJECT)
 * 3) route MTU
 *
 * Returns:
 * 0 on success
 */
static int
alter_nhop_from_info(struct nhop_object *nh, struct rt_addrinfo *info)
{
	struct nhop_priv *nh_priv = nh->nh_priv;
	struct sockaddr *info_gw;
	int error;

	/* Update MTU if set in the request*/
	set_nhop_mtu_from_info(nh, info);

	/* XXX: allow only one of BLACKHOLE,REJECT,GATEWAY */

	/* Allow some flags (FLAG1,STATIC,BLACKHOLE,REJECT) to be toggled on change. */
	nh_priv->rt_flags &= ~RTF_FMASK;
	nh_priv->rt_flags |= info->rti_flags & RTF_FMASK;

	/* Consider gateway change */
	info_gw = info->rti_info[RTAX_GATEWAY];
	if (info_gw != NULL) {
		error = set_nhop_gw_from_info(nh, info);
		if (error != 0)
			return (error);
		if (nh->gw_sa.sa_family == AF_LINK)
			nh_priv->nh_neigh_family = nh_priv->nh_upper_family;
		else
			nh_priv->nh_neigh_family = nh->gw_sa.sa_family;
		/* Update RTF_GATEWAY flag status */
		nh_priv->rt_flags &= ~RTF_GATEWAY;
		nh_priv->rt_flags |= (RTF_GATEWAY & info->rti_flags);
	}
	/* Update datapath flags */
	nh->nh_flags = convert_rt_to_nh_flags(nh_priv->rt_flags);

	if (info->rti_ifa != NULL)
		nh->nh_ifa = info->rti_ifa;
	if (info->rti_ifp != NULL)
		nh->nh_ifp = info->rti_ifp;
	nh->nh_aifp = get_aifp(nh);

	return (0);
}

/*
 * Creates new nexthop based on @nh_orig and augmentation data from @info.
 * Helper function used in the route changes, please see
 *   alter_nhop_from_info() comments for more details.
 *
 * Returns:
 * 0 on success, filling @nh_ret with the desired nexthop object
 * errno otherwise
 */
int
nhop_create_from_nhop(struct rib_head *rnh, const struct nhop_object *nh_orig,
    struct rt_addrinfo *info, struct nhop_object **pnh)
{
	struct nhop_priv *nh_priv;
	struct nhop_object *nh;
	int error;

	NET_EPOCH_ASSERT();

	nh_priv = alloc_nhop_structure();
	nh = nh_priv->nh;

	/* Start with copying data from original nexthop */
	nh_priv->nh_upper_family = nh_orig->nh_priv->nh_upper_family;
	nh_priv->nh_neigh_family = nh_orig->nh_priv->nh_neigh_family;
	nh_priv->rt_flags = nh_orig->nh_priv->rt_flags;
	nh_priv->nh_type = nh_orig->nh_priv->nh_type;

	nh->nh_ifp = nh_orig->nh_ifp;
	nh->nh_ifa = nh_orig->nh_ifa;
	nh->nh_aifp = nh_orig->nh_aifp;
	nh->nh_mtu = nh_orig->nh_mtu;
	nh->nh_flags = nh_orig->nh_flags;
	memcpy(&nh->gw_sa, &nh_orig->gw_sa, nh_orig->gw_sa.sa_len);

	error = alter_nhop_from_info(nh, info);
	if (error != 0) {
		uma_zfree(nhops_zone, nh_priv->nh);
		return (error);
	}

	error = get_nhop(rnh, info, &nh_priv);
	if (error == 0)
		*pnh = nh_priv->nh;

	return (error);
}

/*
 * Allocates memory for public/private nexthop structures.
 *
 * Returns pointer to nhop_priv or NULL.
 */
static struct nhop_priv *
alloc_nhop_structure()
{
	struct nhop_object *nh;
	struct nhop_priv *nh_priv;

	nh = (struct nhop_object *)uma_zalloc(nhops_zone, M_NOWAIT | M_ZERO);
	if (nh == NULL)
		return (NULL);
	nh_priv = (struct nhop_priv *)((char *)nh + NHOP_OBJECT_ALIGNED_SIZE);

	nh->nh_priv = nh_priv;
	nh_priv->nh = nh;

	return (nh_priv);
}

static bool
reference_nhop_deps(struct nhop_object *nh)
{
	if (!ifa_try_ref(nh->nh_ifa))
		return (false);
	nh->nh_aifp = get_aifp(nh);
	if (!if_try_ref(nh->nh_aifp)) {
		ifa_free(nh->nh_ifa);
		return (false);
	}
	FIB_NH_LOG(LOG_DEBUG, nh, "AIFP: %p nh_ifp %p", nh->nh_aifp, nh->nh_ifp);
	if (!if_try_ref(nh->nh_ifp)) {
		ifa_free(nh->nh_ifa);
		if_rele(nh->nh_aifp);
		return (false);
	}

	return (true);
}

/*
 * Alocates/references the remaining bits of nexthop data and links
 *  it to the hash table.
 * Returns 0 if successful,
 *  errno otherwise. @nh_priv is freed in case of error.
 */
static int
finalize_nhop(struct nh_control *ctl, struct rt_addrinfo *info,
    struct nhop_priv *nh_priv)
{
	struct nhop_object *nh = nh_priv->nh;

	/* Allocate per-cpu packet counter */
	nh->nh_pksent = counter_u64_alloc(M_NOWAIT);
	if (nh->nh_pksent == NULL) {
		uma_zfree(nhops_zone, nh);
		RTSTAT_INC(rts_nh_alloc_failure);
		FIB_NH_LOG(LOG_WARNING, nh, "counter_u64_alloc() failed");
		return (ENOMEM);
	}

	if (!reference_nhop_deps(nh)) {
		counter_u64_free(nh->nh_pksent);
		uma_zfree(nhops_zone, nh);
		RTSTAT_INC(rts_nh_alloc_failure);
		FIB_NH_LOG(LOG_WARNING, nh, "interface reference failed");
		return (EAGAIN);
	}

	/* Save vnet to ease destruction */
	nh_priv->nh_vnet = curvnet;

	refcount_init(&nh_priv->nh_refcnt, 1);

	/* Please see nhop_free() comments on the initial value */
	refcount_init(&nh_priv->nh_linked, 2);

	nh_priv->nh_fibnum = ctl->ctl_rh->rib_fibnum;

	if (link_nhop(ctl, nh_priv) == 0) {
		/*
		 * Adding nexthop to the datastructures
		 *  failed. Call destructor w/o waiting for
		 *  the epoch end, as nexthop is not used
		 *  and return.
		 */
		char nhbuf[48];
		FIB_NH_LOG(LOG_WARNING, nh, "failed to link %s",
		    nhop_print_buf(nh, nhbuf, sizeof(nhbuf)));
		destroy_nhop(nh_priv);

		return (ENOBUFS);
	}

#if DEBUG_MAX_LEVEL >= LOG_DEBUG
	char nhbuf[48];
	FIB_NH_LOG(LOG_DEBUG, nh, "finalized: %s", nhop_print_buf(nh, nhbuf, sizeof(nhbuf)));
#endif

	return (0);
}

static void
destroy_nhop(struct nhop_priv *nh_priv)
{
	struct nhop_object *nh;

	nh = nh_priv->nh;

	if_rele(nh->nh_ifp);
	if_rele(nh->nh_aifp);
	ifa_free(nh->nh_ifa);
	counter_u64_free(nh->nh_pksent);

	uma_zfree(nhops_zone, nh);
}

/*
 * Epoch callback indicating nhop is safe to destroy
 */
static void
destroy_nhop_epoch(epoch_context_t ctx)
{
	struct nhop_priv *nh_priv;

	nh_priv = __containerof(ctx, struct nhop_priv, nh_epoch_ctx);

	destroy_nhop(nh_priv);
}

void
nhop_ref_object(struct nhop_object *nh)
{
	u_int old;

	old = refcount_acquire(&nh->nh_priv->nh_refcnt);
	KASSERT(old > 0, ("%s: nhop object %p has 0 refs", __func__, nh));
}

int
nhop_try_ref_object(struct nhop_object *nh)
{

	return (refcount_acquire_if_not_zero(&nh->nh_priv->nh_refcnt));
}

void
nhop_free(struct nhop_object *nh)
{
	struct nh_control *ctl;
	struct nhop_priv *nh_priv = nh->nh_priv;
	struct epoch_tracker et;

	if (!refcount_release(&nh_priv->nh_refcnt))
		return;

#if DEBUG_MAX_LEVEL >= LOG_DEBUG
	char nhbuf[48];
	FIB_NH_LOG(LOG_DEBUG, nh, "deleting %s", nhop_print_buf(nh, nhbuf, sizeof(nhbuf)));
#endif

	/*
	 * There are only 2 places, where nh_linked can be decreased:
	 *  rib destroy (nhops_destroy_rib) and this function.
	 * nh_link can never be increased.
	 *
	 * Hence, use initial value of 2 to make use of
	 *  refcount_release_if_not_last().
	 *
	 * There can be two scenarious when calling this function:
	 *
	 * 1) nh_linked value is 2. This means that either
	 *  nhops_destroy_rib() has not been called OR it is running,
	 *  but we are guaranteed that nh_control won't be freed in
	 *  this epoch. Hence, nexthop can be safely unlinked.
	 *
	 * 2) nh_linked value is 1. In that case, nhops_destroy_rib()
	 *  has been called and nhop unlink can be skipped.
	 */

	NET_EPOCH_ENTER(et);
	if (refcount_release_if_not_last(&nh_priv->nh_linked)) {
		ctl = nh_priv->nh_control;
		if (unlink_nhop(ctl, nh_priv) == NULL) {
			/* Do not try to reclaim */
			char nhbuf[48];
			FIB_NH_LOG(LOG_WARNING, nh, "failed to unlink %s",
			    nhop_print_buf(nh, nhbuf, sizeof(nhbuf)));
			NET_EPOCH_EXIT(et);
			return;
		}
	}
	NET_EPOCH_EXIT(et);

	epoch_call(net_epoch_preempt, destroy_nhop_epoch,
	    &nh_priv->nh_epoch_ctx);
}

void
nhop_ref_any(struct nhop_object *nh)
{
#ifdef ROUTE_MPATH
	if (!NH_IS_NHGRP(nh))
		nhop_ref_object(nh);
	else
		nhgrp_ref_object((struct nhgrp_object *)nh);
#else
	nhop_ref_object(nh);
#endif
}

void
nhop_free_any(struct nhop_object *nh)
{

#ifdef ROUTE_MPATH
	if (!NH_IS_NHGRP(nh))
		nhop_free(nh);
	else
		nhgrp_free((struct nhgrp_object *)nh);
#else
	nhop_free(nh);
#endif
}

/* Helper functions */

uint32_t
nhop_get_idx(const struct nhop_object *nh)
{

	return (nh->nh_priv->nh_idx);
}

enum nhop_type
nhop_get_type(const struct nhop_object *nh)
{

	return (nh->nh_priv->nh_type);
}

void
nhop_set_type(struct nhop_object *nh, enum nhop_type nh_type)
{

	nh->nh_priv->nh_type = nh_type;
}

int
nhop_get_rtflags(const struct nhop_object *nh)
{

	return (nh->nh_priv->rt_flags);
}

void
nhop_set_rtflags(struct nhop_object *nh, int rt_flags)
{

	nh->nh_priv->rt_flags = rt_flags;
}

struct vnet *
nhop_get_vnet(const struct nhop_object *nh)
{

	return (nh->nh_priv->nh_vnet);
}

struct nhop_object *
nhop_select_func(struct nhop_object *nh, uint32_t flowid)
{

	return (nhop_select(nh, flowid));
}

/*
 * Returns address family of the traffic uses the nexthop.
 */
int
nhop_get_upper_family(const struct nhop_object *nh)
{
	return (nh->nh_priv->nh_upper_family);
}

/*
 * Returns address family of the LLE or gateway that is used
 * to forward the traffic to.
 */
int
nhop_get_neigh_family(const struct nhop_object *nh)
{
	return (nh->nh_priv->nh_neigh_family);
}

uint32_t
nhop_get_fibnum(const struct nhop_object *nh)
{
	return (nh->nh_priv->nh_fibnum);
}

void
nhops_update_ifmtu(struct rib_head *rh, struct ifnet *ifp, uint32_t mtu)
{
	struct nh_control *ctl;
	struct nhop_priv *nh_priv;
	struct nhop_object *nh;

	ctl = rh->nh_control;

	NHOPS_WLOCK(ctl);
	CHT_SLIST_FOREACH(&ctl->nh_head, nhops, nh_priv) {
		nh = nh_priv->nh;
		if (nh->nh_ifp == ifp) {
			if ((nh_priv->rt_flags & RTF_FIXEDMTU) == 0 ||
			    nh->nh_mtu > mtu) {
				/* Update MTU directly */
				nh->nh_mtu = mtu;
			}
		}
	} CHT_SLIST_FOREACH_END;
	NHOPS_WUNLOCK(ctl);

}

/*
 * Prints nexthop @nh data in the provided @buf.
 * Example: nh#33/inet/em0/192.168.0.1
 */
char *
nhop_print_buf(const struct nhop_object *nh, char *buf, size_t bufsize)
{
	char abuf[INET6_ADDRSTRLEN];
	struct nhop_priv *nh_priv = nh->nh_priv;
	const char *upper_str = rib_print_family(nh->nh_priv->nh_upper_family);

	switch (nh->gw_sa.sa_family) {
#ifdef INET
	case AF_INET:
		inet_ntop(AF_INET, &nh->gw4_sa.sin_addr, abuf, sizeof(abuf));
		snprintf(buf, bufsize, "nh#%d/%s/%s/%s", nh_priv->nh_idx, upper_str,
		    if_name(nh->nh_ifp), abuf);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		inet_ntop(AF_INET6, &nh->gw6_sa.sin6_addr, abuf, sizeof(abuf));
		snprintf(buf, bufsize, "nh#%d/%s/%s/%s", nh_priv->nh_idx, upper_str,
		    if_name(nh->nh_ifp), abuf);
		break;
#endif
	case AF_LINK:
		snprintf(buf, bufsize, "nh#%d/%s/%s/resolve", nh_priv->nh_idx, upper_str,
		    if_name(nh->nh_ifp));
		break;
	default:
		snprintf(buf, bufsize, "nh#%d/%s/%s/????", nh_priv->nh_idx, upper_str,
		    if_name(nh->nh_ifp));
		break;
	}

	return (buf);
}

/*
 * Dumps a single entry to sysctl buffer.
 *
 * Layout:
 *  rt_msghdr - generic RTM header to allow users to skip non-understood messages
 *  nhop_external - nexhop description structure (with length)
 *  nhop_addrs - structure encapsulating GW/SRC sockaddrs
 */
static int
dump_nhop_entry(struct rib_head *rh, struct nhop_object *nh, struct sysctl_req *w)
{
	struct {
		struct rt_msghdr	rtm;
		struct nhop_external	nhe;
		struct nhop_addrs	na;
	} arpc;
	struct nhop_external *pnhe;
	struct sockaddr *gw_sa, *src_sa;
	struct sockaddr_storage ss;
	size_t addrs_len;
	int error;

	memset(&arpc, 0, sizeof(arpc));

	arpc.rtm.rtm_msglen = sizeof(arpc);
	arpc.rtm.rtm_version = RTM_VERSION;
	arpc.rtm.rtm_type = RTM_GET;
	//arpc.rtm.rtm_flags = RTF_UP;
	arpc.rtm.rtm_flags = nh->nh_priv->rt_flags;

	/* nhop_external */
	pnhe = &arpc.nhe;
	pnhe->nh_len = sizeof(struct nhop_external);
	pnhe->nh_idx = nh->nh_priv->nh_idx;
	pnhe->nh_fib = rh->rib_fibnum;
	pnhe->ifindex = nh->nh_ifp->if_index;
	pnhe->aifindex = nh->nh_aifp->if_index;
	pnhe->nh_family = nh->nh_priv->nh_upper_family;
	pnhe->nh_type = nh->nh_priv->nh_type;
	pnhe->nh_mtu = nh->nh_mtu;
	pnhe->nh_flags = nh->nh_flags;

	memcpy(pnhe->nh_prepend, nh->nh_prepend, sizeof(nh->nh_prepend));
	pnhe->prepend_len = nh->nh_prepend_len;
	pnhe->nh_refcount = nh->nh_priv->nh_refcnt;
	pnhe->nh_pksent = counter_u64_fetch(nh->nh_pksent);

	/* sockaddr container */
	addrs_len = sizeof(struct nhop_addrs);
	arpc.na.gw_sa_off = addrs_len;
	gw_sa = (struct sockaddr *)&nh->gw4_sa;
	addrs_len += gw_sa->sa_len;

	src_sa = nh->nh_ifa->ifa_addr;
	if (src_sa->sa_family == AF_LINK) {
		/* Shorten structure */
		memset(&ss, 0, sizeof(struct sockaddr_storage));
		fill_sdl_from_ifp((struct sockaddr_dl_short *)&ss,
		    nh->nh_ifa->ifa_ifp);
		src_sa = (struct sockaddr *)&ss;
	}
	arpc.na.src_sa_off = addrs_len;
	addrs_len += src_sa->sa_len;

	/* Write total container length */
	arpc.na.na_len = addrs_len;

	arpc.rtm.rtm_msglen += arpc.na.na_len - sizeof(struct nhop_addrs);

	error = SYSCTL_OUT(w, &arpc, sizeof(arpc));
	if (error == 0)
		error = SYSCTL_OUT(w, gw_sa, gw_sa->sa_len);
	if (error == 0)
		error = SYSCTL_OUT(w, src_sa, src_sa->sa_len);

	return (error);
}

uint32_t
nhops_get_count(struct rib_head *rh)
{
	struct nh_control *ctl;
	uint32_t count;

	ctl = rh->nh_control;

	NHOPS_RLOCK(ctl);
	count = ctl->nh_head.items_count;
	NHOPS_RUNLOCK(ctl);

	return (count);
}

int
nhops_dump_sysctl(struct rib_head *rh, struct sysctl_req *w)
{
	struct nh_control *ctl;
	struct nhop_priv *nh_priv;
	int error;

	ctl = rh->nh_control;

	NHOPS_RLOCK(ctl);
#if DEBUG_MAX_LEVEL >= LOG_DEBUG
	FIB_LOG(LOG_DEBUG, rh->rib_fibnum, rh->rib_family, "dump %u items",
	    ctl->nh_head.items_count);
#endif
	CHT_SLIST_FOREACH(&ctl->nh_head, nhops, nh_priv) {
		error = dump_nhop_entry(rh, nh_priv->nh, w);
		if (error != 0) {
			NHOPS_RUNLOCK(ctl);
			return (error);
		}
	} CHT_SLIST_FOREACH_END;
	NHOPS_RUNLOCK(ctl);

	return (0);
}
