/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>

struct rib_subscription {
	CK_STAILQ_ENTRY(rib_subscription)	next;
	rib_subscription_cb_t			*func;
	void					*arg;
	struct rib_head				*rnh;
	enum rib_subscription_type		type;
	struct epoch_context			epoch_ctx;
};

static void destroy_subscription_epoch(epoch_context_t ctx);

void
rib_notify(struct rib_head *rnh, enum rib_subscription_type type,
    struct rib_cmd_info *rc)
{
	struct rib_subscription *rs;

	CK_STAILQ_FOREACH(rs, &rnh->rnh_subscribers, next) {
		if (rs->type == type)
			rs->func(rnh, rc, rs->arg);
	}
}

static struct rib_subscription *
allocate_subscription(rib_subscription_cb_t *f, void *arg,
    enum rib_subscription_type type, bool waitok)
{
	struct rib_subscription *rs;
	int flags = M_ZERO | (waitok ? M_WAITOK : M_NOWAIT);

	rs = malloc(sizeof(struct rib_subscription), M_RTABLE, flags);
	if (rs == NULL)
		return (NULL);

	rs->func = f;
	rs->arg = arg;
	rs->type = type;

	return (rs);
}

/*
 * Subscribe for the changes in the routing table specified by @fibnum and
 *  @family.
 *
 * Returns pointer to the subscription structure on success.
 */
struct rib_subscription *
rib_subscribe(uint32_t fibnum, int family, rib_subscription_cb_t *f, void *arg,
    enum rib_subscription_type type, bool waitok)
{
	struct rib_head *rnh;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	KASSERT((fibnum < rt_numfibs), ("%s: bad fibnum", __func__));
	rnh = rt_tables_get_rnh(fibnum, family);
	NET_EPOCH_EXIT(et);

	return (rib_subscribe_internal(rnh, f, arg, type, waitok));
}

struct rib_subscription *
rib_subscribe_internal(struct rib_head *rnh, rib_subscription_cb_t *f, void *arg,
    enum rib_subscription_type type, bool waitok)
{
	struct rib_subscription *rs;
	struct epoch_tracker et;

	if ((rs = allocate_subscription(f, arg, type, waitok)) == NULL)
		return (NULL);
	rs->rnh = rnh;

	NET_EPOCH_ENTER(et);
	RIB_WLOCK(rnh);
	CK_STAILQ_INSERT_HEAD(&rnh->rnh_subscribers, rs, next);
	RIB_WUNLOCK(rnh);
	NET_EPOCH_EXIT(et);

	return (rs);
}

struct rib_subscription *
rib_subscribe_locked(struct rib_head *rnh, rib_subscription_cb_t *f, void *arg,
    enum rib_subscription_type type)
{
	struct rib_subscription *rs;

	NET_EPOCH_ASSERT();
	RIB_WLOCK_ASSERT(rnh);

	if ((rs = allocate_subscription(f, arg, type, false)) == NULL)
		return (NULL);
	rs->rnh = rnh;

	CK_STAILQ_INSERT_HEAD(&rnh->rnh_subscribers, rs, next);

	return (rs);
}

/*
 * Remove rtable subscription @rs from the routing table.
 * Needs to be run in network epoch.
 */
void
rib_unsubscribe(struct rib_subscription *rs)
{
	struct rib_head *rnh = rs->rnh;

	NET_EPOCH_ASSERT();

	RIB_WLOCK(rnh);
	CK_STAILQ_REMOVE(&rnh->rnh_subscribers, rs, rib_subscription, next);
	RIB_WUNLOCK(rnh);

	NET_EPOCH_CALL(destroy_subscription_epoch, &rs->epoch_ctx);
}

void
rib_unsubscribe_locked(struct rib_subscription *rs)
{
	struct rib_head *rnh = rs->rnh;

	NET_EPOCH_ASSERT();
	RIB_WLOCK_ASSERT(rnh);

	CK_STAILQ_REMOVE(&rnh->rnh_subscribers, rs, rib_subscription, next);

	NET_EPOCH_CALL(destroy_subscription_epoch, &rs->epoch_ctx);
}

/*
 * Epoch callback indicating subscription is safe to destroy
 */
static void
destroy_subscription_epoch(epoch_context_t ctx)
{
	struct rib_subscription *rs;

	rs = __containerof(ctx, struct rib_subscription, epoch_ctx);

	free(rs, M_RTABLE);
}

void
rib_init_subscriptions(struct rib_head *rnh)
{

	CK_STAILQ_INIT(&rnh->rnh_subscribers);
}

void
rib_destroy_subscriptions(struct rib_head *rnh)
{
	struct rib_subscription *rs;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	RIB_WLOCK(rnh);
	while ((rs = CK_STAILQ_FIRST(&rnh->rnh_subscribers)) != NULL) {
		CK_STAILQ_REMOVE_HEAD(&rnh->rnh_subscribers, next);
		NET_EPOCH_CALL(destroy_subscription_epoch, &rs->epoch_ctx);
	}
	RIB_WUNLOCK(rnh);
	NET_EPOCH_EXIT(et);
}
