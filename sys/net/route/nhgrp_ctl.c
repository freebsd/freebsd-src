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
#include "opt_inet.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/epoch.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
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

#define	DEBUG_MOD_NAME	nhgrp_ctl
#define	DEBUG_MAX_LEVEL	LOG_DEBUG
#include <net/route/route_debug.h>
_DECLARE_DEBUG(LOG_INFO);

/*
 * This file contains the supporting functions for creating multipath groups
 *  and compiling their dataplane parts.
 */

/* MPF_MULTIPATH must be the same as NHF_MULTIPATH for nhop selection to work */
_Static_assert(MPF_MULTIPATH == NHF_MULTIPATH,
    "MPF_MULTIPATH must be the same as NHF_MULTIPATH");
/* Offset and size of flags field has to be the same for nhop/nhop groups */
CHK_STRUCT_FIELD_GENERIC(struct nhop_object, nh_flags, struct nhgrp_object, nhg_flags);
/* Cap multipath to 64, as the larger values would break rib_cmd_info bmasks */
CTASSERT(RIB_MAX_MPATH_WIDTH <= 64);

static int wn_cmp_idx(const void *a, const void *b);
static void sort_weightened_nhops(struct weightened_nhop *wn, int num_nhops);

static struct nhgrp_priv *get_nhgrp(struct nh_control *ctl,
    struct weightened_nhop *wn, int num_nhops, uint32_t uidx, int *perror);
static void destroy_nhgrp(struct nhgrp_priv *nhg_priv);
static void destroy_nhgrp_epoch(epoch_context_t ctx);
static void free_nhgrp_nhops(struct nhgrp_priv *nhg_priv);

static int
wn_cmp_idx(const void *a, const void *b)
{
	const struct weightened_nhop *w_a = a;
	const struct weightened_nhop *w_b = b;
	uint32_t a_idx = w_a->nh->nh_priv->nh_idx;
	uint32_t b_idx = w_b->nh->nh_priv->nh_idx;

	if (a_idx < b_idx)
		return (-1);
	else if (a_idx > b_idx)
		return (1);
	else
		return (0);
}

/*
 * Perform in-place sorting for array of nexthops in @wn.
 * Sort by nexthop index ascending.
 */
static void
sort_weightened_nhops(struct weightened_nhop *wn, int num_nhops)
{

	qsort(wn, num_nhops, sizeof(struct weightened_nhop), wn_cmp_idx);
}

/*
 * In order to determine the minimum weight difference in the array
 * of weights, create a sorted array of weights, using spare "storage"
 * field in the `struct weightened_nhop`.
 * Assume weights to be (mostly) the same and use insertion sort to
 * make it sorted.
 */
static void
sort_weightened_nhops_weights(struct weightened_nhop *wn, int num_items)
{
	wn[0].storage = wn[0].weight;
	for (int i = 1, j = 0; i < num_items; i++) {
		uint32_t weight = wn[i].weight; // read from 'weight' as it's not reordered
		/* Move all weights > weight 1 position right */
		for (j = i - 1; j >= 0 && wn[j].storage > weight; j--)
			wn[j + 1].storage = wn[j].storage;
		wn[j + 1].storage = weight;
	}
}

/*
 * Calculate minimum number of slots required to fit the existing
 * set of weights in the common use case where weights are "easily"
 * comparable.
 * Assumes @wn is sorted by weight ascending and each weight is > 0.
 * Returns number of slots or 0 if precise calculation failed.
 *
 * Some examples:
 * note: (i, X) pair means (nhop=i, weight=X):
 * (1, 1) (2, 2) -> 3 slots [1, 2, 2]
 * (1, 100), (2, 200) -> 3 slots [1, 2, 2]
 * (1, 100), (2, 200), (3, 400) -> 7 slots [1, 2, 2, 3, 3, 3]
 */
static uint32_t
calc_min_mpath_slots_fast(struct weightened_nhop *wn, size_t num_items,
    uint64_t *ptotal)
{
	uint32_t i, last, xmin;
	uint64_t total = 0;

	// Get sorted array of weights in .storage field
	sort_weightened_nhops_weights(wn, num_items);

	last = 0;
	xmin = wn[0].storage;
	for (i = 0; i < num_items; i++) {
		total += wn[i].storage;
		if ((wn[i].storage != last) &&
		    ((wn[i].storage - last < xmin) || xmin == 0)) {
			xmin = wn[i].storage - last;
		}
		last = wn[i].storage;
	}
	*ptotal = total;
	/* xmin is the minimum unit of desired capacity */
	if ((total % xmin) != 0)
		return (0);
	for (i = 0; i < num_items; i++) {
		if ((wn[i].weight % xmin) != 0)
			return (0);
	}

	return ((uint32_t)(total / xmin));
}

/*
 * Calculate minimum number of slots required to fit the existing
 * set of weights while maintaining weight coefficients.
 *
 * Assume @wn is sorted by weight ascending and each weight is > 0.
 *
 * Tries to find simple precise solution first and falls back to
 *  RIB_MAX_MPATH_WIDTH in case of any failure.
 */
static uint32_t
calc_min_mpath_slots(struct weightened_nhop *wn, size_t num_items)
{
	uint32_t v;
	uint64_t total;

	v = calc_min_mpath_slots_fast(wn, num_items, &total);
	if (total == 0)
		return (0);
	if ((v == 0) || (v > RIB_MAX_MPATH_WIDTH))
		v = RIB_MAX_MPATH_WIDTH;

	return (v);
}

/*
 * Nexthop group data consists of
 * 1) dataplane part, with nhgrp_object as a header followed by an
 *   arbitrary number of nexthop pointers.
 * 2) control plane part, with nhgrp_priv as a header, followed by
 *   an arbirtrary number of 'struct weightened_nhop' object.
 *
 * Given nexthop groups are (mostly) immutable, allocate all data
 * in one go.
 *
 */
__noinline static size_t
get_nhgrp_alloc_size(uint32_t nhg_size, uint32_t num_nhops)
{
	size_t sz;

	sz = sizeof(struct nhgrp_object);
	sz += nhg_size * sizeof(struct nhop_object *);
	sz += sizeof(struct nhgrp_priv);
	sz += num_nhops * sizeof(struct weightened_nhop);
	return (sz);
}

/*
 * Compile actual list of nexthops to be used by datapath from
 *  the nexthop group @dst.
 *
 * For example, compiling control plane list of 2 nexthops
 *  [(200, A), (100, B)] would result in the datapath array
 *  [A, A, B]
 */
static void
compile_nhgrp(struct nhgrp_priv *dst_priv, const struct weightened_nhop *x,
    uint32_t num_slots)
{
	struct nhgrp_object *dst;
	int i, slot_idx, remaining_slots;
	uint64_t remaining_sum, nh_weight, nh_slots;

	slot_idx  = 0;
	dst = dst_priv->nhg;
	/* Calculate sum of all weights */
	remaining_sum = 0;
	for (i = 0; i < dst_priv->nhg_nh_count; i++)
		remaining_sum += x[i].weight;
	remaining_slots = num_slots;
	FIB_NH_LOG(LOG_DEBUG3, x[0].nh, "sum: %lu, slots: %d",
	    remaining_sum, remaining_slots);
	for (i = 0; i < dst_priv->nhg_nh_count; i++) {
		/* Calculate number of slots for the current nexthop */
		if (remaining_sum > 0) {
			nh_weight = (uint64_t)x[i].weight;
			nh_slots = (nh_weight * remaining_slots / remaining_sum);
		} else
			nh_slots = 0;

		remaining_sum -= x[i].weight;
		remaining_slots -= nh_slots;

		FIB_NH_LOG(LOG_DEBUG3, x[0].nh,
		    " rem_sum: %lu, rem_slots: %d nh_slots: %d, slot_idx: %d",
		    remaining_sum, remaining_slots, (int)nh_slots, slot_idx);

		KASSERT((slot_idx + nh_slots <= num_slots),
		    ("index overflow during nhg compilation"));
		while (nh_slots-- > 0)
			dst->nhops[slot_idx++] = x[i].nh;
	}
}

/*
 * Allocates new nexthop group for the list of weightened nexthops.
 * Assume sorted list.
 * Does NOT reference any nexthops in the group.
 * Returns group with refcount=1 or NULL.
 */
static struct nhgrp_priv *
alloc_nhgrp(struct weightened_nhop *wn, int num_nhops)
{
	uint32_t nhgrp_size;
	struct nhgrp_object *nhg;
	struct nhgrp_priv *nhg_priv;

	nhgrp_size = calc_min_mpath_slots(wn, num_nhops);
	if (nhgrp_size == 0) {
		/* Zero weights, abort */
		return (NULL);
	}

	size_t sz = get_nhgrp_alloc_size(nhgrp_size, num_nhops);
	nhg = malloc(sz, M_NHOP, M_NOWAIT | M_ZERO);
	if (nhg == NULL) {
		FIB_NH_LOG(LOG_INFO, wn[0].nh,
		    "unable to allocate group with num_nhops %d (compiled %u)",
		    num_nhops, nhgrp_size);
		return (NULL);
	}

	/* Has to be the first to make NHGRP_PRIV() work */
	nhg->nhg_size = nhgrp_size;
	nhg->nhg_flags = MPF_MULTIPATH;

	nhg_priv = NHGRP_PRIV(nhg);
	nhg_priv->nhg_nh_count = num_nhops;
	refcount_init(&nhg_priv->nhg_refcount, 1);

	/* Please see nhgrp_free() comments on the initial value */
	refcount_init(&nhg_priv->nhg_linked, 2);

	nhg_priv->nhg = nhg;
	memcpy(&nhg_priv->nhg_nh_weights[0], wn,
	  num_nhops * sizeof(struct weightened_nhop));

	FIB_NH_LOG(LOG_DEBUG, wn[0].nh, "num_nhops: %d, compiled_nhop: %u",
	    num_nhops, nhgrp_size);

	compile_nhgrp(nhg_priv, wn, nhg->nhg_size);

	return (nhg_priv);
}

void
nhgrp_ref_object(struct nhgrp_object *nhg)
{
	struct nhgrp_priv *nhg_priv;
	u_int old __diagused;

	nhg_priv = NHGRP_PRIV(nhg);
	old = refcount_acquire(&nhg_priv->nhg_refcount);
	KASSERT(old > 0, ("%s: nhgrp object %p has 0 refs", __func__, nhg));
}

void
nhgrp_free(struct nhgrp_object *nhg)
{
	struct nhgrp_priv *nhg_priv;
	struct nh_control *ctl;
	struct epoch_tracker et;

	nhg_priv = NHGRP_PRIV(nhg);

	if (!refcount_release(&nhg_priv->nhg_refcount))
		return;

	/*
	 * group objects don't have an explicit lock attached to it.
	 * As groups are reclaimed based on reference count, it is possible
	 * that some groups will persist after vnet destruction callback
	 * called. Given that, handle scenario with nhgrp_free_group() being
	 * called either after or simultaneously with nhgrp_ctl_unlink_all()
	 * by using another reference counter: nhg_linked.
	 *
	 * There are only 2 places, where nhg_linked can be decreased:
	 *  rib destroy (nhgrp_ctl_unlink_all) and this function.
	 * nhg_link can never be increased.
	 *
	 * Hence, use initial value of 2 to make use of
	 *  refcount_release_if_not_last().
	 *
	 * There can be two scenarious when calling this function:
	 *
	 * 1) nhg_linked value is 2. This means that either
	 *  nhgrp_ctl_unlink_all() has not been called OR it is running,
	 *  but we are guaranteed that nh_control won't be freed in
	 *  this epoch. Hence, nexthop can be safely unlinked.
	 *
	 * 2) nh_linked value is 1. In that case, nhgrp_ctl_unlink_all()
	 *  has been called and nhgrp unlink can be skipped.
	 */

	NET_EPOCH_ENTER(et);
	if (refcount_release_if_not_last(&nhg_priv->nhg_linked)) {
		ctl = nhg_priv->nh_control;
		if (unlink_nhgrp(ctl, nhg_priv) == NULL) {
			/* Do not try to reclaim */
			RT_LOG(LOG_INFO, "Failed to unlink nexhop group %p",
			    nhg_priv);
			NET_EPOCH_EXIT(et);
			return;
		}
		MPASS((nhg_priv->nhg_idx == 0));
		MPASS((nhg_priv->nhg_refcount == 0));
	}
	NET_EPOCH_EXIT(et);

	NET_EPOCH_CALL(destroy_nhgrp_epoch, &nhg_priv->nhg_epoch_ctx);
}

/*
 * Destroys all local resources belonging to @nhg_priv.
 */
__noinline static void
destroy_nhgrp_int(struct nhgrp_priv *nhg_priv)
{

	free(nhg_priv->nhg, M_NHOP);
}

__noinline static void
destroy_nhgrp(struct nhgrp_priv *nhg_priv)
{
	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char nhgbuf[NHOP_PRINT_BUFSIZE] __unused;
		FIB_NH_LOG(LOG_DEBUG2, nhg_priv->nhg_nh_weights[0].nh,
		    "destroying %s", nhgrp_print_buf(nhg_priv->nhg,
		    nhgbuf, sizeof(nhgbuf)));
	}

	free_nhgrp_nhops(nhg_priv);
	destroy_nhgrp_int(nhg_priv);
}

/*
 * Epoch callback indicating group is safe to destroy
 */
static void
destroy_nhgrp_epoch(epoch_context_t ctx)
{
	struct nhgrp_priv *nhg_priv;

	nhg_priv = __containerof(ctx, struct nhgrp_priv, nhg_epoch_ctx);

	destroy_nhgrp(nhg_priv);
}

static bool
ref_nhgrp_nhops(struct nhgrp_priv *nhg_priv)
{

	for (int i = 0; i < nhg_priv->nhg_nh_count; i++) {
		if (nhop_try_ref_object(nhg_priv->nhg_nh_weights[i].nh) != 0)
			continue;

		/*
		 * Failed to ref the nexthop, b/c it's deleted.
		 * Need to rollback references back.
		 */
		for (int j = 0; j < i; j++)
			nhop_free(nhg_priv->nhg_nh_weights[j].nh);
		return (false);
	}

	return (true);
}

static void
free_nhgrp_nhops(struct nhgrp_priv *nhg_priv)
{

	for (int i = 0; i < nhg_priv->nhg_nh_count; i++)
		nhop_free(nhg_priv->nhg_nh_weights[i].nh);
}

/*
 * Allocate nexthop group of size @num_nhops with nexthops specified by
 * @wn. Nexthops have to be unique and match the fibnum/family of the group.
 * Returns unlinked nhgrp object on success or NULL and non-zero perror.
 */
struct nhgrp_object *
nhgrp_alloc(uint32_t fibnum, int family, struct weightened_nhop *wn, int num_nhops,
    int *perror)
{
	struct rib_head *rh = rt_tables_get_rnh(fibnum, family);
	struct nhgrp_priv *nhg_priv;
	struct nh_control *ctl;

	if (rh == NULL) {
		*perror = E2BIG;
		return (NULL);
	}

	ctl = rh->nh_control;

	if (num_nhops > RIB_MAX_MPATH_WIDTH) {
		*perror = E2BIG;
		return (NULL);
	}

	if (ctl->gr_head.hash_size == 0) {
		/* First multipath request. Bootstrap mpath datastructures. */
		if (nhgrp_ctl_alloc_default(ctl, M_NOWAIT) == 0) {
			*perror = ENOMEM;
			return (NULL);
		}
	}

	/* Sort nexthops & check there are no duplicates */
	sort_weightened_nhops(wn, num_nhops);
	uint32_t last_id = 0;
	for (int i = 0; i < num_nhops; i++) {
		if (wn[i].nh->nh_priv->nh_control != ctl) {
			*perror = EINVAL;
			return (NULL);
		}
		if (wn[i].nh->nh_priv->nh_idx == last_id) {
			*perror = EEXIST;
			return (NULL);
		}
		last_id = wn[i].nh->nh_priv->nh_idx;
	}

	if ((nhg_priv = alloc_nhgrp(wn, num_nhops)) == NULL) {
		*perror = ENOMEM;
		return (NULL);
	}
	nhg_priv->nh_control = ctl;

	*perror = 0;
	return (nhg_priv->nhg);
}

/*
 * Finds an existing group matching @nhg or links @nhg to the tree.
 * Returns the referenced group or NULL and non-zero @perror.
 */
struct nhgrp_object *
nhgrp_get_nhgrp(struct nhgrp_object *nhg, int *perror)
{
	struct nhgrp_priv *nhg_priv, *key = NHGRP_PRIV(nhg);
	struct nh_control *ctl = key->nh_control;

	nhg_priv = find_nhgrp(ctl, key);
	if (nhg_priv != NULL) {
		/*
		 * Free originally-created group. As it hasn't been linked
		 *  and the dependent nexhops haven't been referenced, just free
		 *  the group.
		 */
		destroy_nhgrp_int(key);
		*perror = 0;
		return (nhg_priv->nhg);
	} else {
		/* No existing group, try to link the new one */
		if (!ref_nhgrp_nhops(key)) {
			/*
			 * Some of the nexthops have been scheduled for deletion.
			 * As the group hasn't been linked / no nexhops have been
			 *  referenced, call the final destructor immediately.
			 */
			destroy_nhgrp_int(key);
			*perror = EAGAIN;
			return (NULL);
		}
		if (link_nhgrp(ctl, key) == 0) {
			/* Unable to allocate index? */
			*perror = EAGAIN;
			free_nhgrp_nhops(key);
			destroy_nhgrp_int(key);
			return (NULL);
		}
		*perror = 0;
		return (nhg);
	}

	/* NOTREACHED */
}

/*
 * Creates or looks up an existing nexthop group based on @wn and @num_nhops.
 *
 * Returns referenced nhop group or NULL, passing error code in @perror.
 */
struct nhgrp_priv *
get_nhgrp(struct nh_control *ctl, struct weightened_nhop *wn, int num_nhops,
    uint32_t uidx, int *perror)
{
	struct nhgrp_object *nhg;

	nhg = nhgrp_alloc(ctl->ctl_rh->rib_fibnum, ctl->ctl_rh->rib_family,
	    wn, num_nhops, perror);
	if (nhg == NULL)
		return (NULL);
	nhgrp_set_uidx(nhg, uidx);
	nhg = nhgrp_get_nhgrp(nhg, perror);
	if (nhg != NULL)
		return (NHGRP_PRIV(nhg));
	return (NULL);
}


/*
 * Appends one or more nexthops denoted by @wm to the nexthop group @gr_orig.
 *
 * Returns referenced nexthop group or NULL. In the latter case, @perror is
 *  filled with an error code.
 * Note that function does NOT care if the next nexthops already exists
 * in the @gr_orig. As a result, they will be added, resulting in the
 * same nexthop being present multiple times in the new group.
 */
static struct nhgrp_priv *
append_nhops(struct nh_control *ctl, const struct nhgrp_object *gr_orig,
    struct weightened_nhop *wn, int num_nhops, int *perror)
{
	char storage[64];
	struct weightened_nhop *pnhops;
	struct nhgrp_priv *nhg_priv;
	const struct nhgrp_priv *src_priv;
	size_t sz;
	int curr_nhops;

	src_priv = NHGRP_PRIV_CONST(gr_orig);
	curr_nhops = src_priv->nhg_nh_count;

	*perror = 0;

	sz = (src_priv->nhg_nh_count + num_nhops) * (sizeof(struct weightened_nhop));
	/* optimize for <= 4 paths, each path=16 bytes */
	if (sz <= sizeof(storage))
		pnhops = (struct weightened_nhop *)&storage[0];
	else {
		pnhops = malloc(sz, M_TEMP, M_NOWAIT);
		if (pnhops == NULL) {
			*perror = ENOMEM;
			return (NULL);
		}
	}

	/* Copy nhops from original group first */
	memcpy(pnhops, src_priv->nhg_nh_weights,
	  curr_nhops * sizeof(struct weightened_nhop));
	memcpy(&pnhops[curr_nhops], wn, num_nhops * sizeof(struct weightened_nhop));
	curr_nhops += num_nhops;

	nhg_priv = get_nhgrp(ctl, pnhops, curr_nhops, 0, perror);

	if (pnhops != (struct weightened_nhop *)&storage[0])
		free(pnhops, M_TEMP);

	if (nhg_priv == NULL)
		return (NULL);

	return (nhg_priv);
}


/*
 * Creates/finds nexthop group based on @wn and @num_nhops.
 * Returns 0 on success with referenced group in @rnd, or
 * errno.
 *
 * If the error is EAGAIN, then the operation can be retried.
 */
int
nhgrp_get_group(struct rib_head *rh, struct weightened_nhop *wn, int num_nhops,
    uint32_t uidx, struct nhgrp_object **pnhg)
{
	struct nh_control *ctl = rh->nh_control;
	struct nhgrp_priv *nhg_priv;
	int error;

	nhg_priv = get_nhgrp(ctl, wn, num_nhops, uidx, &error);
	if (nhg_priv != NULL)
		*pnhg = nhg_priv->nhg;

	return (error);
}

/*
 * Creates new nexthop group based on @src group without the nexthops
 * chosen by @flt_func.
 * Returns 0 on success, storring the reference nhop group/object in @rnd.
 */
int
nhgrp_get_filtered_group(struct rib_head *rh, const struct rtentry *rt,
    const struct nhgrp_object *src, rib_filter_f_t flt_func, void *flt_data,
    struct route_nhop_data *rnd)
{
	char storage[64];
	struct nh_control *ctl = rh->nh_control;
	struct weightened_nhop *pnhops;
	const struct nhgrp_priv *mp_priv, *src_priv;
	size_t sz;
	int error, i, num_nhops;

	src_priv = NHGRP_PRIV_CONST(src);

	sz = src_priv->nhg_nh_count * (sizeof(struct weightened_nhop));
	/* optimize for <= 4 paths, each path=16 bytes */
	if (sz <= sizeof(storage))
		pnhops = (struct weightened_nhop *)&storage[0];
	else {
		if ((pnhops = malloc(sz, M_TEMP, M_NOWAIT)) == NULL)
			return (ENOMEM);
	}

	/* Filter nexthops */
	error = 0;
	num_nhops = 0;
	for (i = 0; i < src_priv->nhg_nh_count; i++) {
		if (flt_func(rt, src_priv->nhg_nh_weights[i].nh, flt_data))
			continue;
		memcpy(&pnhops[num_nhops++], &src_priv->nhg_nh_weights[i],
		  sizeof(struct weightened_nhop));
	}

	if (num_nhops == 0) {
		rnd->rnd_nhgrp = NULL;
		rnd->rnd_weight = 0;
	} else if (num_nhops == 1) {
		rnd->rnd_nhop = pnhops[0].nh;
		rnd->rnd_weight = pnhops[0].weight;
		if (nhop_try_ref_object(rnd->rnd_nhop) == 0)
			error = EAGAIN;
	} else {
		mp_priv = get_nhgrp(ctl, pnhops, num_nhops, 0, &error);
		if (mp_priv != NULL)
			rnd->rnd_nhgrp = mp_priv->nhg;
		rnd->rnd_weight = 0;
	}

	if (pnhops != (struct weightened_nhop *)&storage[0])
		free(pnhops, M_TEMP);

	return (error);
}

/*
 * Creates new multipath group based on existing group/nhop in @rnd_orig and
 *  to-be-added nhop @wn_add.
 * Returns 0 on success and stores result in @rnd_new.
 */
int
nhgrp_get_addition_group(struct rib_head *rh, struct route_nhop_data *rnd_orig,
    struct route_nhop_data *rnd_add, struct route_nhop_data *rnd_new)
{
	struct nh_control *ctl = rh->nh_control;
	struct nhgrp_priv *nhg_priv;
	struct weightened_nhop wn[2] = {};
	int error;

	if (rnd_orig->rnd_nhop == NULL) {
		/* No paths to add to, just reference current nhop */
		*rnd_new = *rnd_add;
		if (nhop_try_ref_object(rnd_new->rnd_nhop) == 0)
			return (EAGAIN);
		return (0);
	}

	wn[0].nh = rnd_add->rnd_nhop;
	wn[0].weight = rnd_add->rnd_weight;

	if (!NH_IS_NHGRP(rnd_orig->rnd_nhop)) {
		/* Simple merge of 2 non-multipath nexthops */
		wn[1].nh = rnd_orig->rnd_nhop;
		wn[1].weight = rnd_orig->rnd_weight;
		nhg_priv = get_nhgrp(ctl, wn, 2, 0, &error);
	} else {
		/* Get new nhop group with @rt->rt_nhop as an additional nhop */
		nhg_priv = append_nhops(ctl, rnd_orig->rnd_nhgrp, &wn[0], 1,
		    &error);
	}

	if (nhg_priv == NULL)
		return (error);
	rnd_new->rnd_nhgrp = nhg_priv->nhg;
	rnd_new->rnd_weight = 0;

	return (0);
}

/*
 * Returns pointer to array of nexthops with weights for
 * given @nhg. Stores number of items in the array into @pnum_nhops.
 */
const struct weightened_nhop *
nhgrp_get_nhops(const struct nhgrp_object *nhg, uint32_t *pnum_nhops)
{
	const struct nhgrp_priv *nhg_priv;

	KASSERT(((nhg->nhg_flags & MPF_MULTIPATH) != 0), ("nhop is not mpath"));

	nhg_priv = NHGRP_PRIV_CONST(nhg);
	*pnum_nhops = nhg_priv->nhg_nh_count;

	return (nhg_priv->nhg_nh_weights);
}

void
nhgrp_set_uidx(struct nhgrp_object *nhg, uint32_t uidx)
{
	struct nhgrp_priv *nhg_priv;

	KASSERT(((nhg->nhg_flags & MPF_MULTIPATH) != 0), ("nhop is not mpath"));

	nhg_priv = NHGRP_PRIV(nhg);

	nhg_priv->nhg_uidx = uidx;
}

uint32_t
nhgrp_get_uidx(const struct nhgrp_object *nhg)
{
	const struct nhgrp_priv *nhg_priv;

	KASSERT(((nhg->nhg_flags & MPF_MULTIPATH) != 0), ("nhop is not mpath"));

	nhg_priv = NHGRP_PRIV_CONST(nhg);
	return (nhg_priv->nhg_uidx);
}

/*
 * Prints nexhop group @nhg data in the provided @buf.
 * Example: nhg#33/sz=3:[#1:100,#2:100,#3:100]
 * Example: nhg#33/sz=5:[#1:100,#2:100,..]
 */
char *
nhgrp_print_buf(const struct nhgrp_object *nhg, char *buf, size_t bufsize)
{
	const struct nhgrp_priv *nhg_priv = NHGRP_PRIV_CONST(nhg);

	int off = snprintf(buf, bufsize, "nhg#%u/sz=%u:[", nhg_priv->nhg_idx,
	    nhg_priv->nhg_nh_count);

	for (int i = 0; i < nhg_priv->nhg_nh_count; i++) {
		const struct weightened_nhop *wn = &nhg_priv->nhg_nh_weights[i];
		int len = snprintf(&buf[off], bufsize - off, "#%u:%u,",
		    wn->nh->nh_priv->nh_idx, wn->weight);
		if (len + off + 3 >= bufsize) {
			int len = snprintf(&buf[off], bufsize - off, "...");
			off += len;
			break;
		}
		off += len;
	}
	if (off > 0)
		off--; // remove last ","
	if (off + 1 < bufsize)
		snprintf(&buf[off], bufsize - off, "]");
	return buf;
}

__noinline static int
dump_nhgrp_entry(struct rib_head *rh, const struct nhgrp_priv *nhg_priv,
    char *buffer, size_t buffer_size, struct sysctl_req *w)
{
	struct rt_msghdr *rtm;
	struct nhgrp_external *nhge;
	struct nhgrp_container *nhgc;
	const struct nhgrp_object *nhg;
	struct nhgrp_nhop_external *ext;
	int error;
	size_t sz;

	nhg = nhg_priv->nhg;

	sz = sizeof(struct rt_msghdr) + sizeof(struct nhgrp_external);
	/* controlplane nexthops */
	sz += sizeof(struct nhgrp_container);
	sz += sizeof(struct nhgrp_nhop_external) * nhg_priv->nhg_nh_count;
	/* dataplane nexthops */
	sz += sizeof(struct nhgrp_container);
	sz += sizeof(struct nhgrp_nhop_external) * nhg->nhg_size;

	KASSERT(sz <= buffer_size, ("increase nhgrp buffer size"));

	bzero(buffer, sz);

	rtm = (struct rt_msghdr *)buffer;
	rtm->rtm_msglen = sz;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = RTM_GET;

	nhge = (struct nhgrp_external *)(rtm + 1);

	nhge->nhg_idx = nhg_priv->nhg_idx;
	nhge->nhg_refcount = nhg_priv->nhg_refcount;

	/* fill in control plane nexthops firs */
	nhgc = (struct nhgrp_container *)(nhge + 1);
	nhgc->nhgc_type = NHG_C_TYPE_CNHOPS;
	nhgc->nhgc_subtype = 0;
	nhgc->nhgc_len = sizeof(struct nhgrp_container);
	nhgc->nhgc_len += sizeof(struct nhgrp_nhop_external) * nhg_priv->nhg_nh_count;
	nhgc->nhgc_count = nhg_priv->nhg_nh_count;

	ext = (struct nhgrp_nhop_external *)(nhgc + 1);
	for (int i = 0; i < nhg_priv->nhg_nh_count; i++) {
		ext[i].nh_idx = nhg_priv->nhg_nh_weights[i].nh->nh_priv->nh_idx;
		ext[i].nh_weight = nhg_priv->nhg_nh_weights[i].weight;
	}

	/* fill in dataplane nexthops */
	nhgc = (struct nhgrp_container *)(&ext[nhg_priv->nhg_nh_count]);
	nhgc->nhgc_type = NHG_C_TYPE_DNHOPS;
	nhgc->nhgc_subtype = 0;
	nhgc->nhgc_len = sizeof(struct nhgrp_container);
	nhgc->nhgc_len += sizeof(struct nhgrp_nhop_external) * nhg->nhg_size;
	nhgc->nhgc_count = nhg->nhg_size;

	ext = (struct nhgrp_nhop_external *)(nhgc + 1);
	for (int i = 0; i < nhg->nhg_size; i++) {
		ext[i].nh_idx = nhg->nhops[i]->nh_priv->nh_idx;
		ext[i].nh_weight = 0;
	}

	error = SYSCTL_OUT(w, buffer, sz);

	return (error);
}

uint32_t
nhgrp_get_idx(const struct nhgrp_object *nhg)
{
	const struct nhgrp_priv *nhg_priv;

	nhg_priv = NHGRP_PRIV_CONST(nhg);
	return (nhg_priv->nhg_idx);
}

uint8_t
nhgrp_get_origin(const struct nhgrp_object *nhg)
{
	return (NHGRP_PRIV_CONST(nhg)->nhg_origin);
}

void
nhgrp_set_origin(struct nhgrp_object *nhg, uint8_t origin)
{
	NHGRP_PRIV(nhg)->nhg_origin = origin;
}

uint32_t
nhgrp_get_count(struct rib_head *rh)
{
	struct nh_control *ctl;
	uint32_t count;

	ctl = rh->nh_control;

	NHOPS_RLOCK(ctl);
	count = ctl->gr_head.items_count;
	NHOPS_RUNLOCK(ctl);

	return (count);
}

int
nhgrp_dump_sysctl(struct rib_head *rh, struct sysctl_req *w)
{
	struct nh_control *ctl = rh->nh_control;
	struct epoch_tracker et;
	struct nhgrp_priv *nhg_priv;
	char *buffer;
	size_t sz;
	int error = 0;

	if (ctl->gr_head.items_count == 0)
		return (0);

	/* Calculate the maximum nhop group size in bytes */
	sz = sizeof(struct rt_msghdr) + sizeof(struct nhgrp_external);
	sz += 2 * sizeof(struct nhgrp_container);
	sz += 2 * sizeof(struct nhgrp_nhop_external) * RIB_MAX_MPATH_WIDTH;
	buffer = malloc(sz, M_TEMP, M_NOWAIT);
	if (buffer == NULL)
		return (ENOMEM);

	NET_EPOCH_ENTER(et);
	NHOPS_RLOCK(ctl);
	CHT_SLIST_FOREACH(&ctl->gr_head, mpath, nhg_priv) {
		error = dump_nhgrp_entry(rh, nhg_priv, buffer, sz, w);
		if (error != 0)
			break;
	} CHT_SLIST_FOREACH_END;
	NHOPS_RUNLOCK(ctl);
	NET_EPOCH_EXIT(et);

	free(buffer, M_TEMP);

	return (error);
}
