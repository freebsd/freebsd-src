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
#include <sys/jail.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>
#ifdef INET
#include <netinet/in_fib.h>
#endif
#ifdef INET6
#include <netinet6/in6_fib.h>
#endif
#include <net/vnet.h>

/*
 * RIB helper functions.
 */

/*
 * Calls @wa_f with @arg for each entry in the table specified by
 * @af and @fibnum.
 *
 * @ss_t callback is called before and after the tree traversal
 *  while holding table lock.
 *
 * Table is traversed under read lock unless @wlock is set.
 */
void
rib_walk_ext(uint32_t fibnum, int family, bool wlock, rib_walktree_f_t *wa_f,
    rib_walk_hook_f_t *hook_f, void *arg)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rnh;

	if ((rnh = rt_tables_get_rnh(fibnum, family)) == NULL)
		return;

	if (wlock)
		RIB_WLOCK(rnh);
	else
		RIB_RLOCK(rnh);
	if (hook_f != NULL)
		hook_f(rnh, RIB_WALK_HOOK_PRE, arg);
	rnh->rnh_walktree(&rnh->head, (walktree_f_t *)wa_f, arg);
	if (hook_f != NULL)
		hook_f(rnh, RIB_WALK_HOOK_POST, arg);
	if (wlock)
		RIB_WUNLOCK(rnh);
	else
		RIB_RUNLOCK(rnh);
}

/*
 * Calls @wa_f with @arg for each entry in the table specified by
 * @af and @fibnum.
 *
 * Table is traversed under read lock unless @wlock is set.
 */
void
rib_walk(uint32_t fibnum, int family, bool wlock, rib_walktree_f_t *wa_f,
    void *arg)
{

	rib_walk_ext(fibnum, family, wlock, wa_f, NULL, arg);
}

/*
 * Iterates over all existing fibs in system calling
 *  @hook_f function before/after traversing each fib.
 *  Calls @wa_f function for each element in current fib.
 * If af is not AF_UNSPEC, iterates over fibs in particular
 * address family.
 */
void
rib_foreach_table_walk(int family, bool wlock, rib_walktree_f_t *wa_f,
    rib_walk_hook_f_t *hook_f, void *arg)
{

	for (uint32_t fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		/* Do we want some specific family? */
		if (family != AF_UNSPEC) {
			rib_walk_ext(fibnum, family, wlock, wa_f, hook_f, arg); 
			continue;
		}

		for (int i = 1; i <= AF_MAX; i++)
			rib_walk_ext(fibnum, i, wlock, wa_f, hook_f, arg);
	}
}

/*
 * Iterates over all existing fibs in system and deletes each element
 *  for which @filter_f function returns non-zero value.
 * If @family is not AF_UNSPEC, iterates over fibs in particular
 * address family.
 */
void
rib_foreach_table_walk_del(int family, rib_filter_f_t *filter_f, void *arg)
{

	for (uint32_t fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		/* Do we want some specific family? */
		if (family != AF_UNSPEC) {
			rib_walk_del(fibnum, family, filter_f, arg, 0);
			continue;
		}

		for (int i = 1; i <= AF_MAX; i++)
			rib_walk_del(fibnum, i, filter_f, arg, 0);
	}
}


/*
 * Wrapper for the control plane functions for performing af-agnostic
 *  lookups.
 * @fibnum: fib to perform the lookup.
 * @dst: sockaddr with family and addr filled in. IPv6 addresses needs to be in
 *  deembedded from.
 * @flags: fib(9) flags.
 * @flowid: flow id for path selection in multipath use case.
 *
 * Returns nhop_object or NULL.
 *
 * Requires NET_EPOCH.
 *
 */
struct nhop_object *
rib_lookup(uint32_t fibnum, const struct sockaddr *dst, uint32_t flags,
    uint32_t flowid)
{
	struct nhop_object *nh;

	nh = NULL;

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
	{
		const struct sockaddr_in *a = (const struct sockaddr_in *)dst;
		nh = fib4_lookup(fibnum, a->sin_addr, 0, flags, flowid);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		const struct sockaddr_in6 *a = (const struct sockaddr_in6*)dst;
		nh = fib6_lookup(fibnum, &a->sin6_addr, a->sin6_scope_id,
		    flags, flowid);
		break;
	}
#endif
	}

	return (nh);
}

#ifdef ROUTE_MPATH
static void
decompose_change_notification(struct rib_cmd_info *rc, route_notification_t *cb,
    void *cbdata)
{
	uint32_t num_old, num_new;
	uint32_t nh_idx_old, nh_idx_new;
	struct weightened_nhop *wn_old, *wn_new;
	struct weightened_nhop tmp = { NULL, 0 };
	uint32_t idx_old = 0, idx_new = 0;

	struct rib_cmd_info rc_del = { .rc_cmd = RTM_DELETE, .rc_rt = rc->rc_rt };
	struct rib_cmd_info rc_add = { .rc_cmd = RTM_ADD, .rc_rt = rc->rc_rt };

	if (NH_IS_NHGRP(rc->rc_nh_old)) {
		wn_old = nhgrp_get_nhops((struct nhgrp_object *)rc->rc_nh_old, &num_old);
	} else {
		tmp.nh = rc->rc_nh_old;
		tmp.weight = rc->rc_nh_weight;
		wn_old = &tmp;
		num_old = 1;
	}
	if (NH_IS_NHGRP(rc->rc_nh_new)) {
		wn_new = nhgrp_get_nhops((struct nhgrp_object *)rc->rc_nh_new, &num_new);
	} else {
		tmp.nh = rc->rc_nh_new;
		tmp.weight = rc->rc_nh_weight;
		wn_new = &tmp;
		num_new = 1;
	}

	/* Use the fact that each @wn array is sorted */
	/*
	 * Want to convert into set of add and delete operations
	 * [1] -> [1, 2] = A{2}
	 * [2] -> [1, 2] = A{1}
	 * [1, 2, 4]->[1, 3, 4] = A{2}, D{3}
	 * [1, 2, 4]->[1, 4] = D{2}
	 * [1, 2, 4] -> [3, 4] = D{1}, C{2,3} OR C{1,3}, D{2} OR D{1},D{2},A{3}
	 * [1, 2] -> [3, 4] =
	 *
	 */
	idx_old = 0;
	while ((idx_old < num_old) && (idx_new < num_new)) {
		nh_idx_old = wn_old[idx_old].nh->nh_priv->nh_idx;
		nh_idx_new = wn_new[idx_new].nh->nh_priv->nh_idx;

		if (nh_idx_old == nh_idx_new) {
			if (wn_old[idx_old].weight != wn_new[idx_new].weight) {
				/* Update weight by providing del/add notifications */
				rc_del.rc_nh_old = wn_old[idx_old].nh;
				rc_del.rc_nh_weight = wn_old[idx_old].weight;
				cb(&rc_del, cbdata);

				rc_add.rc_nh_new = wn_new[idx_new].nh;
				rc_add.rc_nh_weight = wn_new[idx_new].weight;
				cb(&rc_add, cbdata);
			}
			idx_old++;
			idx_new++;
		} else if (nh_idx_old < nh_idx_new) {
			/*
			 * [1, ~2~, 4], [1, ~3~, 4]
			 * [1, ~2~, 5], [1, ~3~, 4]
			 * [1, ~2~], [1, ~3~, 4]
			 */
			if ((idx_old + 1 >= num_old) ||
			    (wn_old[idx_old + 1].nh->nh_priv->nh_idx > nh_idx_new)) {
				/* Add new unless the next old item is still <= new */
				rc_add.rc_nh_new = wn_new[idx_new].nh;
				rc_add.rc_nh_weight = wn_new[idx_new].weight;
				cb(&rc_add, cbdata);
				idx_new++;
			}
			/* In any case, delete current old */
			rc_del.rc_nh_old = wn_old[idx_old].nh;
			rc_del.rc_nh_weight = wn_old[idx_old].weight;
			cb(&rc_del, cbdata);
			idx_old++;
		} else {
			/*
			 * nh_idx_old > nh_idx_new
			 *
			 * [1, ~3~, 4], [1, ~2~, 4]
			 * [1, ~3~, 5], [1, ~2~, 4]
			 * [1, ~3~, 4], [1, ~2~]
			 */
			if ((idx_new + 1 >= num_new) ||
			    (wn_new[idx_new + 1].nh->nh_priv->nh_idx > nh_idx_old)) {
				/* No next item or next item is > current one */
				rc_add.rc_nh_new = wn_new[idx_new].nh;
				rc_add.rc_nh_weight = wn_new[idx_new].weight;
				cb(&rc_add, cbdata);
				idx_new++;
			}
			/* In any case, delete current old */
			rc_del.rc_nh_old = wn_old[idx_old].nh;
			rc_del.rc_nh_weight = wn_old[idx_old].weight;
			cb(&rc_del, cbdata);
			idx_old++;
		}
	}

	while (idx_old < num_old) {
		rc_del.rc_nh_old = wn_old[idx_old].nh;
		rc_del.rc_nh_weight = wn_old[idx_old].weight;
		cb(&rc_del, cbdata);
		idx_old++;
	}

	while (idx_new < num_new) {
		rc_add.rc_nh_new = wn_new[idx_new].nh;
		rc_add.rc_nh_weight = wn_new[idx_new].weight;
		cb(&rc_add, cbdata);
		idx_new++;
	}
}

/*
 * Decompose multipath cmd info @rc into a list of add/del/change
 *  single-path operations, calling @cb callback for each operation.
 * Assumes at least one of the nexthops in @rc is multipath.
 */
void
rib_decompose_notification(struct rib_cmd_info *rc, route_notification_t *cb,
    void *cbdata)
{
	struct weightened_nhop *wn;
	uint32_t num_nhops;
	struct rib_cmd_info rc_new;

	rc_new = *rc;
	DPRINTF("cb=%p cmd=%d nh_old=%p nh_new=%p",
	    cb, rc->cmd, rc->nh_old, rc->nh_new);
	switch (rc->rc_cmd) {
	case RTM_ADD:
		if (!NH_IS_NHGRP(rc->rc_nh_new))
			return;
		wn = nhgrp_get_nhops((struct nhgrp_object *)rc->rc_nh_new, &num_nhops);
		for (uint32_t i = 0; i < num_nhops; i++) {
			rc_new.rc_nh_new = wn[i].nh;
			rc_new.rc_nh_weight = wn[i].weight;
			cb(&rc_new, cbdata);
		}
		break;
	case RTM_DELETE:
		if (!NH_IS_NHGRP(rc->rc_nh_old))
			return;
		wn = nhgrp_get_nhops((struct nhgrp_object *)rc->rc_nh_old, &num_nhops);
		for (uint32_t i = 0; i < num_nhops; i++) {
			rc_new.rc_nh_old = wn[i].nh;
			rc_new.rc_nh_weight = wn[i].weight;
			cb(&rc_new, cbdata);
		}
		break;
	case RTM_CHANGE:
		if (!NH_IS_NHGRP(rc->rc_nh_old) && !NH_IS_NHGRP(rc->rc_nh_new))
			return;
		decompose_change_notification(rc, cb, cbdata);
		break;
	}
}
#endif
