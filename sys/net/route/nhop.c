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
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/route/route_var.h>
#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#include <net/vnet.h>

#define	DEBUG_MOD_NAME	nhop
#define	DEBUG_MAX_LEVEL	LOG_DEBUG
#include <net/route/route_debug.h>
_DECLARE_DEBUG(LOG_INFO);

/*
 * This file contains data structures management logic for the nexthop ("nhop")
 *   route subsystem.
 *
 * Nexthops in the original sense are the objects containing all the necessary
 * information to forward the packet to the selected destination.
 * In particular, nexthop is defined by a combination of
 *  ifp, ifa, aifp, mtu, gw addr(if set), nh_type, nh_family, mask of rt_flags and
 *    NHF_DEFAULT
 *
 * All nexthops are stored in the resizable hash table.
 * Additionally, each nexthop gets assigned its unique index (nexthop index)
 * so userland programs can interact with the nexthops easier. Index allocation
 * is backed by the bitmask array.
 */

MALLOC_DEFINE(M_NHOP, "nhops", "nexthops data");

/* Hash management functions */

int
nhops_init_rib(struct rib_head *rh)
{
	struct nh_control *ctl;
	size_t alloc_size;
	uint32_t num_buckets, num_items;
	void *ptr;

	ctl = malloc(sizeof(struct nh_control), M_NHOP, M_WAITOK | M_ZERO);

	/*
	 * Allocate nexthop hash. Start with 16 items by default (128 bytes).
	 * This will be enough for most of the cases.
	 */
	num_buckets = 16;
	alloc_size = CHT_SLIST_GET_RESIZE_SIZE(num_buckets);
	ptr = malloc(alloc_size, M_NHOP, M_WAITOK | M_ZERO);
	CHT_SLIST_INIT(&ctl->nh_head, ptr, num_buckets);

	/*
	 * Allocate nexthop index bitmask.
	 */
	num_items = 128 * 8; /* 128 bytes */
	ptr = malloc(bitmask_get_size(num_items), M_NHOP, M_WAITOK | M_ZERO);
	bitmask_init(&ctl->nh_idx_head, ptr, num_items);

	NHOPS_LOCK_INIT(ctl);

	rh->nh_control = ctl;
	ctl->ctl_rh = rh;

	FIB_CTL_LOG(LOG_DEBUG2, ctl, "nhops init: ctl %p rh %p", ctl, rh);

	return (0);
}

static void
destroy_ctl(struct nh_control *ctl)
{

	NHOPS_LOCK_DESTROY(ctl);
	free(ctl->nh_head.ptr, M_NHOP);
	free(ctl->nh_idx_head.idx, M_NHOP);
#ifdef ROUTE_MPATH
	nhgrp_ctl_free(ctl);
#endif
	free(ctl, M_NHOP);
}

/*
 * Epoch callback indicating ctl is safe to destroy
 */
static void
destroy_ctl_epoch(epoch_context_t ctx)
{
	struct nh_control *ctl;

	ctl = __containerof(ctx, struct nh_control, ctl_epoch_ctx);

	destroy_ctl(ctl);
}

void
nhops_destroy_rib(struct rib_head *rh)
{
	struct nh_control *ctl;
	struct nhop_priv *nh_priv;

	ctl = rh->nh_control;

	/*
	 * All routes should have been deleted in rt_table_destroy().
	 * However, TCP stack or other consumers may store referenced
	 *  nexthop pointers. When these references go to zero,
	 *  nhop_free() will try to unlink these records from the
	 *  datastructures, most likely leading to panic.
	 *
	 * Avoid that by explicitly marking all of the remaining
	 *  nexthops as unlinked by removing a reference from a special
	 *  counter. Please see nhop_free() comments for more
	 *  details.
	 */

	NHOPS_WLOCK(ctl);
	CHT_SLIST_FOREACH(&ctl->nh_head, nhops, nh_priv) {
		FIB_RH_LOG(LOG_DEBUG3, rh, "marking nhop %u unlinked", nh_priv->nh_idx);
		refcount_release(&nh_priv->nh_linked);
	} CHT_SLIST_FOREACH_END;
#ifdef ROUTE_MPATH
	nhgrp_ctl_unlink_all(ctl);
#endif
	NHOPS_WUNLOCK(ctl);

	/*
	 * Postpone destruction till the end of current epoch
	 * so nhop_free() can safely use nh_control pointer.
	 */
	NET_EPOCH_CALL(destroy_ctl_epoch, &ctl->ctl_epoch_ctx);
}

/*
 * Nexhop hash calculation:
 *
 * Nexthops distribution:
 * 2 "mandatory" nexthops per interface ("interface route", "loopback").
 * For direct peering: 1 nexthop for the peering router per ifp/af.
 * For Ix-like peering: tens to hundreds nexthops of neghbors per ifp/af.
 * IGP control plane & broadcast segment: tens of nexthops per ifp/af.
 *
 * Each fib/af combination has its own hash table.
 * With that in mind, hash nexthops by the combination of the interface
 *  and GW IP address.
 *
 * To optimize hash calculation, ignore lower bits of ifnet pointer,
 * as they  give very little entropy.
 * Similarly, use lower 4 bytes of IPv6 address to distinguish between the
 *  neighbors.
 */
struct _hash_data {
	uint16_t	ifentropy;
	uint8_t		family;
	uint8_t		nh_type;
	uint32_t	gw_addr;
};

static unsigned
djb_hash(const unsigned char *h, const int len)
{
	unsigned int result = 0;
	int i;

	for (i = 0; i < len; i++)
		result = 33 * result ^ h[i];

	return (result);
}

static uint32_t
hash_priv(const struct nhop_priv *priv)
{
	struct nhop_object *nh = priv->nh;
	struct _hash_data key = {
	    .ifentropy = (uint16_t)((((uintptr_t)nh->nh_ifp) >> 6) & 0xFFFF),
	    .family = nh->gw_sa.sa_family,
	    .nh_type = priv->nh_type & 0xFF,
	    .gw_addr = (nh->gw_sa.sa_family == AF_INET6) ?
		nh->gw6_sa.sin6_addr.s6_addr32[3] :
		nh->gw4_sa.sin_addr.s_addr
	};

	return (uint32_t)(djb_hash((const unsigned char *)&key, sizeof(key)));
}

/*
 * Checks if hash needs resizing and performs this resize if necessary
 *
 */
static void
consider_resize(struct nh_control *ctl, uint32_t new_nh_buckets, uint32_t new_idx_items)
{
	void *nh_ptr, *nh_idx_ptr;
	void *old_idx_ptr;
	size_t alloc_size;

	nh_ptr = NULL;
	if (new_nh_buckets != 0) {
		alloc_size = CHT_SLIST_GET_RESIZE_SIZE(new_nh_buckets);
		nh_ptr = malloc(alloc_size, M_NHOP, M_NOWAIT | M_ZERO);
	}

	nh_idx_ptr = NULL;
	if (new_idx_items != 0) {
		alloc_size = bitmask_get_size(new_idx_items);
		nh_idx_ptr = malloc(alloc_size, M_NHOP, M_NOWAIT | M_ZERO);
	}

	if (nh_ptr == NULL && nh_idx_ptr == NULL) {
		/* Either resize is not required or allocations have failed. */
		return;
	}

	FIB_CTL_LOG(LOG_DEBUG, ctl,
	    "going to resize: nh:[ptr:%p sz:%u] idx:[ptr:%p sz:%u]",
	    nh_ptr, new_nh_buckets, nh_idx_ptr, new_idx_items);

	old_idx_ptr = NULL;

	NHOPS_WLOCK(ctl);
	if (nh_ptr != NULL) {
		CHT_SLIST_RESIZE(&ctl->nh_head, nhops, nh_ptr, new_nh_buckets);
	}
	if (nh_idx_ptr != NULL) {
		if (bitmask_copy(&ctl->nh_idx_head, nh_idx_ptr, new_idx_items) == 0)
			bitmask_swap(&ctl->nh_idx_head, nh_idx_ptr, new_idx_items, &old_idx_ptr);
	}
	NHOPS_WUNLOCK(ctl);

	if (nh_ptr != NULL)
		free(nh_ptr, M_NHOP);
	if (old_idx_ptr != NULL)
		free(old_idx_ptr, M_NHOP);
}

/*
 * Links nextop @nh_priv to the nexhop hash table and allocates
 *  nexhop index.
 * Returns allocated index or 0 on failure.
 */
int
link_nhop(struct nh_control *ctl, struct nhop_priv *nh_priv)
{
	uint16_t idx;
	uint32_t num_buckets_new, num_items_new;

	KASSERT((nh_priv->nh_idx == 0), ("nhop index is already allocated"));
	NHOPS_WLOCK(ctl);

	/*
	 * Check if we need to resize hash and index.
	 * The following 2 functions returns either new size or 0
	 *  if resize is not required.
	 */
	num_buckets_new = CHT_SLIST_GET_RESIZE_BUCKETS(&ctl->nh_head);
	num_items_new = bitmask_get_resize_items(&ctl->nh_idx_head);

	if (bitmask_alloc_idx(&ctl->nh_idx_head, &idx) != 0) {
		NHOPS_WUNLOCK(ctl);
		FIB_CTL_LOG(LOG_INFO, ctl, "Unable to allocate nhop index");
		RTSTAT_INC(rts_nh_idx_alloc_failure);
		consider_resize(ctl, num_buckets_new, num_items_new);
		return (0);
	}

	nh_priv->nh_idx = idx;
	nh_priv->nh_control = ctl;
	nh_priv->nh_finalized = 1;

	CHT_SLIST_INSERT_HEAD(&ctl->nh_head, nhops, nh_priv);

	NHOPS_WUNLOCK(ctl);

	FIB_RH_LOG(LOG_DEBUG2, ctl->ctl_rh,
	    "Linked nhop priv %p to %d, hash %u, ctl %p",
	    nh_priv, idx, hash_priv(nh_priv), ctl);
	consider_resize(ctl, num_buckets_new, num_items_new);

	return (idx);
}

/*
 * Unlinks nexthop specified by @nh_priv data from the hash.
 *
 * Returns found nexthop or NULL.
 */
struct nhop_priv *
unlink_nhop(struct nh_control *ctl, struct nhop_priv *nh_priv_del)
{
	struct nhop_priv *priv_ret;
	int idx;
	uint32_t num_buckets_new, num_items_new;

	idx = 0;

	NHOPS_WLOCK(ctl);
	CHT_SLIST_REMOVE(&ctl->nh_head, nhops, nh_priv_del, priv_ret);

	if (priv_ret != NULL) {
		idx = priv_ret->nh_idx;
		priv_ret->nh_idx = 0;

		KASSERT((idx != 0), ("bogus nhop index 0"));
		if ((bitmask_free_idx(&ctl->nh_idx_head, idx)) != 0) {
			FIB_CTL_LOG(LOG_DEBUG, ctl,
			    "Unable to remove index %d from fib %u af %d",
			    idx, ctl->ctl_rh->rib_fibnum, ctl->ctl_rh->rib_family);
		}
	}

	/* Check if hash or index needs to be resized */
	num_buckets_new = CHT_SLIST_GET_RESIZE_BUCKETS(&ctl->nh_head);
	num_items_new = bitmask_get_resize_items(&ctl->nh_idx_head);

	NHOPS_WUNLOCK(ctl);

	if (priv_ret == NULL) {
		FIB_CTL_LOG(LOG_INFO, ctl,
		    "Unable to unlink nhop priv %p from hash, hash %u ctl %p",
		    nh_priv_del, hash_priv(nh_priv_del), ctl);
	} else {
		FIB_CTL_LOG(LOG_DEBUG2, ctl, "Unlinked nhop %p priv idx %d",
		    priv_ret, idx);
	}

	consider_resize(ctl, num_buckets_new, num_items_new);

	return (priv_ret);
}

/*
 * Searches for the nexthop by data specifcied in @nh_priv.
 * Returns referenced nexthop or NULL.
 */
struct nhop_priv *
find_nhop(struct nh_control *ctl, const struct nhop_priv *nh_priv)
{
	struct nhop_priv *nh_priv_ret;

	NHOPS_RLOCK(ctl);
	CHT_SLIST_FIND_BYOBJ(&ctl->nh_head, nhops, nh_priv, nh_priv_ret);
	if (nh_priv_ret != NULL) {
		if (refcount_acquire_if_not_zero(&nh_priv_ret->nh_refcnt) == 0){
			/* refcount was 0 -> nhop is being deleted */
			nh_priv_ret = NULL;
		}
	}
	NHOPS_RUNLOCK(ctl);

	return (nh_priv_ret);
}
