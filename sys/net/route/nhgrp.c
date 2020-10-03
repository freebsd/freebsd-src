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
 *
 * $FreeBSD$
 */

#include "opt_inet.h"
#include "opt_route.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_fib.h>

#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#include <net/route/nhgrp_var.h>

/*
 * This file contains data structures management logic for the nexthop
 * groups ("nhgrp") route subsystem.
 *
 * Nexthop groups are used to store multiple routes available for the specific
 *  prefix. Nexthop groups are immutable and can be shared across multiple
 *  prefixes.
 *
 * Each group consists of a control plane part and a dataplane part.
 * Control plane is basically a collection of nexthop objects with
 *  weights and refcount.
 *
 * Datapath consists of a array of nexthop pointers, compiled from control
 *  plane data to support O(1) nexthop selection.
 *
 * For example, consider the following group:
 *  [(nh1, weight=100), (nh2, weight=200)]
 * It will compile to the following array:
 *  [nh1, nh2, nh2]
 *
 */

static void consider_resize(struct nh_control *ctl, uint32_t new_nh_buckets,
    uint32_t new_idx_items);

static int cmp_nhgrp(const struct nhgrp_priv *a, const struct nhgrp_priv *b);
static unsigned int hash_nhgrp(const struct nhgrp_priv *obj);

static unsigned
djb_hash(const unsigned char *h, const int len)
{
	unsigned int result = 0;
	int i;

	for (i = 0; i < len; i++)
		result = 33 * result ^ h[i];

	return (result);
}

static int
cmp_nhgrp(const struct nhgrp_priv *a, const struct nhgrp_priv *b)
{

	/*
	 * In case of consistent hashing, there can be multiple nexthop groups
	 * with the same "control plane" list of nexthops with weights and a
	 * different set of "data plane" nexthops.
	 * For now, ignore the data plane and focus on the control plane list.
	 */
	if (a->nhg_nh_count != b->nhg_nh_count)
		return (0);
	return !memcmp(a->nhg_nh_weights, b->nhg_nh_weights,
	    sizeof(struct weightened_nhop) * a->nhg_nh_count);
}

/*
 * Hash callback: calculate hash of an object
 */
static unsigned int
hash_nhgrp(const struct nhgrp_priv *obj)
{
	const unsigned char *key;

	key = (const unsigned char *)obj->nhg_nh_weights;

	return (djb_hash(key, sizeof(struct weightened_nhop) * obj->nhg_nh_count));
}

/*
 * Returns object referenced and unlocked
 */
struct nhgrp_priv *
find_nhgrp(struct nh_control *ctl, const struct nhgrp_priv *key)
{
	struct nhgrp_priv *priv_ret;

	NHOPS_RLOCK(ctl);
	CHT_SLIST_FIND_BYOBJ(&ctl->gr_head, mpath, key, priv_ret);
	if (priv_ret != NULL) {
		if (refcount_acquire_if_not_zero(&priv_ret->nhg_refcount) == 0) {
			/* refcount is 0 -> group is being deleted */
			priv_ret = NULL;
		}
	}
	NHOPS_RUNLOCK(ctl);

	return (priv_ret);
}

int
link_nhgrp(struct nh_control *ctl, struct nhgrp_priv *grp_priv)
{
	uint16_t idx;
	uint32_t new_num_buckets, new_num_items;

	NHOPS_WLOCK(ctl);
	/* Check if we need to resize hash and index */
	new_num_buckets = CHT_SLIST_GET_RESIZE_BUCKETS(&ctl->gr_head);
	new_num_items = bitmask_get_resize_items(&ctl->gr_idx_head);

	if (bitmask_alloc_idx(&ctl->gr_idx_head, &idx) != 0) {
		NHOPS_WUNLOCK(ctl);
		DPRINTF("Unable to allocate mpath index");
		consider_resize(ctl, new_num_buckets, new_num_items);
		return (0);
	}

	grp_priv->nhg_idx = idx;
	grp_priv->nh_control = ctl;
	CHT_SLIST_INSERT_HEAD(&ctl->gr_head, mpath, grp_priv);

	NHOPS_WUNLOCK(ctl);

	consider_resize(ctl, new_num_buckets, new_num_items);

	return (1);
}

struct nhgrp_priv *
unlink_nhgrp(struct nh_control *ctl, struct nhgrp_priv *key)
{
	struct nhgrp_priv *nhg_priv_ret;
	int ret, idx;

	NHOPS_WLOCK(ctl);

	CHT_SLIST_REMOVE_BYOBJ(&ctl->gr_head, mpath, key, nhg_priv_ret);

	if (nhg_priv_ret == NULL) {
		DPRINTF("Unable to find nhop group!");
		NHOPS_WUNLOCK(ctl);
		return (NULL);
	}

	idx = nhg_priv_ret->nhg_idx;
	ret = bitmask_free_idx(&ctl->gr_idx_head, idx);
	nhg_priv_ret->nhg_idx = 0;
	nhg_priv_ret->nh_control = NULL;

	NHOPS_WUNLOCK(ctl);

	return (nhg_priv_ret);
}

/*
 * Checks if hash needs resizing and performs this resize if necessary
 *
 */
__noinline static void
consider_resize(struct nh_control *ctl, uint32_t new_nh_buckets, uint32_t new_idx_items)
{
	void *nh_ptr, *nh_idx_ptr;
	void *old_idx_ptr;
	size_t alloc_size;

	nh_ptr = NULL ;
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

	DPRINTF("mp: going to resize: nh:[ptr:%p sz:%u] idx:[ptr:%p sz:%u]",
	    nh_ptr, new_nh_buckets, nh_idx_ptr, new_idx_items);

	old_idx_ptr = NULL;

	NHOPS_WLOCK(ctl);
	if (nh_ptr != NULL) {
		CHT_SLIST_RESIZE(&ctl->gr_head, mpath, nh_ptr, new_nh_buckets);
	}
	if (nh_idx_ptr != NULL) {
		if (bitmask_copy(&ctl->gr_idx_head, nh_idx_ptr, new_idx_items))
			bitmask_swap(&ctl->nh_idx_head, nh_idx_ptr, new_idx_items, &old_idx_ptr);
	}
	NHOPS_WUNLOCK(ctl);

	if (nh_ptr != NULL)
		free(nh_ptr, M_NHOP);
	if (old_idx_ptr != NULL)
		free(old_idx_ptr, M_NHOP);
}

/*
 * Function allocating the necessary group data structures.
 */
bool
nhgrp_ctl_alloc_default(struct nh_control *ctl, int malloc_flags)
{
	size_t alloc_size;
	uint32_t num_buckets, num_items;
	void *cht_ptr, *mask_ptr;

	malloc_flags = (malloc_flags & (M_NOWAIT | M_WAITOK)) | M_ZERO;

	num_buckets = 8;
	alloc_size = CHT_SLIST_GET_RESIZE_SIZE(num_buckets);
	cht_ptr = malloc(alloc_size, M_NHOP, malloc_flags);

	if (cht_ptr == NULL) {
		DPRINTF("mpath init failed");
		return (false);
	}

	/*
	 * Allocate nexthop index bitmask.
	 */
	num_items = 128;
	mask_ptr = malloc(bitmask_get_size(num_items), M_NHOP, malloc_flags);
	if (mask_ptr == NULL) {
		DPRINTF("mpath bitmask init failed");
		free(cht_ptr, M_NHOP);
		return (false);
	}

	NHOPS_WLOCK(ctl);

	if (ctl->gr_head.hash_size == 0) {
		/* Init hash and bitmask */
		CHT_SLIST_INIT(&ctl->gr_head, cht_ptr, num_buckets);
		bitmask_init(&ctl->gr_idx_head, mask_ptr, num_items);
		NHOPS_WUNLOCK(ctl);
	} else {
		/* Other thread has already initiliazed hash/bitmask */
		NHOPS_WUNLOCK(ctl);
		free(cht_ptr, M_NHOP);
		free(mask_ptr, M_NHOP);
	}

	DPRINTF("mpath init done for fib/af %d/%d", ctl->rh->rib_fibnum,
	    ctl->rh->rib_family);

	return (true);
}

int
nhgrp_ctl_init(struct nh_control *ctl)
{

	/*
	 * By default, do not allocate datastructures as multipath
	 * routes will not be necessarily used.
	 */
	CHT_SLIST_INIT(&ctl->gr_head, NULL, 0);
	bitmask_init(&ctl->gr_idx_head, NULL, 0);
	return (0);
}

void
nhgrp_ctl_free(struct nh_control *ctl)
{

	if (ctl->gr_head.ptr != NULL)
		free(ctl->gr_head.ptr, M_NHOP);
	if (ctl->gr_idx_head.idx != NULL)
		free(ctl->gr_idx_head.idx, M_NHOP);
}

void
nhgrp_ctl_unlink_all(struct nh_control *ctl)
{
	struct nhgrp_priv *nhg_priv;

	NHOPS_WLOCK_ASSERT(ctl);

	CHT_SLIST_FOREACH(&ctl->gr_head, mpath, nhg_priv) {
		DPRINTF("Marking nhgrp %u unlinked", nhg_priv->nhg_idx);
		refcount_release(&nhg_priv->nhg_linked);
	} CHT_SLIST_FOREACH_END;
}

