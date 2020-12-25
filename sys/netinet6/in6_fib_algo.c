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
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_fib.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/fib_algo.h>

/*
 * Lockless radix lookup algo.
 *
 * Compiles immutable radix from the current routing table.
 * Used with small amount of routes (<1000).
 * As datastructure is immutable, it gets rebuild on each rtable change.
 *
 */

#define KEY_LEN_INET6	(offsetof(struct sa_in6, sin6_addr) + sizeof(struct in6_addr))
#define OFF_LEN_INET6	(8 * offsetof(struct sa_in6, sin6_addr))
struct sa_in6 {
	uint8_t			sin6_len;
	uint8_t			sin6_family;
	uint8_t			pad[6];
	struct in6_addr		sin6_addr;
};
struct radix6_addr_entry {
	struct radix_node	rn[2];
	struct sa_in6		addr;
	struct nhop_object	*nhop;
};
#define	LRADIX6_ITEM_SZ	roundup2(sizeof(struct radix6_addr_entry), CACHE_LINE_SIZE)

struct lradix6_data {
	struct radix_node_head	*rnh;
	struct fib_data		*fd;
	void			*mem; // raw radix_mem pointer to free
	void			*radix_mem;
	uint32_t		alloc_items;
	uint32_t		num_items;
};

static struct nhop_object *
lradix6_lookup(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	struct radix_node_head *rnh = (struct radix_node_head *)algo_data;
	struct radix6_addr_entry *ent;
	struct sa_in6 addr6 = {
		.sin6_len = KEY_LEN_INET6,
		.sin6_addr = *key.addr6,
	};
	if (IN6_IS_SCOPE_LINKLOCAL(key.addr6))
		addr6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);
	ent = (struct radix6_addr_entry *)(rnh->rnh_matchaddr(&addr6, &rnh->rh));
	if (ent != NULL)
		return (ent->nhop);
	return (NULL);
}

static uint8_t
lradix6_get_pref(const struct rib_rtable_info *rinfo)
{

	if (rinfo->num_prefixes < 10)
		return (255);
	else if (rinfo->num_prefixes < 100000)
		return (255 - rinfo->num_prefixes / 394);
	else
		return (1);
}

static enum flm_op_result
lradix6_init(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **_data)
{
	struct lradix6_data *lr;
	struct rib_rtable_info rinfo;
	uint32_t count;
	void *mem;
 
	lr = malloc(sizeof(struct lradix6_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (lr == NULL || !rn_inithead((void **)&lr->rnh, OFF_LEN_INET6))
		return (FLM_REBUILD);
	fib_get_rtable_info(fib_get_rh(fd), &rinfo);

	count = rinfo.num_prefixes * 11 / 10;
	// count+1 adds at least 1 cache line
	mem = malloc((count + 1) * LRADIX6_ITEM_SZ, M_RTABLE, M_NOWAIT | M_ZERO);
	if (mem == NULL)
		return (FLM_REBUILD);
	lr->mem = mem;
	lr->radix_mem = (void *)roundup2((uintptr_t)mem, CACHE_LINE_SIZE);
	lr->alloc_items = count;
	lr->fd = fd;

	*_data = lr;

	return (FLM_SUCCESS);
}

static void
lradix6_destroy(void *_data)
{
	struct lradix6_data *lr = (struct lradix6_data *)_data;

	if (lr->rnh != NULL)
		rn_detachhead((void **)&lr->rnh);
	if (lr->mem != NULL)
		free(lr->mem, M_RTABLE);
	free(lr, M_RTABLE);
}

static enum flm_op_result
lradix6_add_route_cb(struct rtentry *rt, void *_data)
{
	struct lradix6_data *lr = (struct lradix6_data *)_data;
	struct radix6_addr_entry *ae;
	struct sockaddr_in6 *rt_dst, *rt_mask;
	struct sa_in6 mask;
	struct radix_node *rn;
	struct nhop_object *nh;

	nh = rt_get_raw_nhop(rt);

	if (lr->num_items >= lr->alloc_items)
		return (FLM_REBUILD);

	ae = (struct radix6_addr_entry *)((char *)lr->radix_mem + lr->num_items * LRADIX6_ITEM_SZ);
	lr->num_items++;

	ae->nhop = nh;

	rt_dst = (struct sockaddr_in6 *)rt_key(rt);
	rt_mask = (struct sockaddr_in6 *)rt_mask(rt);

	ae->addr.sin6_len = KEY_LEN_INET6;
	ae->addr.sin6_addr = rt_dst->sin6_addr;

	if (rt_mask != NULL) {
		bzero(&mask, sizeof(mask));
		mask.sin6_len = KEY_LEN_INET6;
		mask.sin6_addr = rt_mask->sin6_addr;
		rt_mask = (struct sockaddr_in6 *)&mask;
	}

	rn = lr->rnh->rnh_addaddr((struct sockaddr *)&ae->addr,
	    (struct sockaddr *)rt_mask, &lr->rnh->rh, ae->rn);
	if (rn == NULL)
		return (FLM_REBUILD);

	return (FLM_SUCCESS);
}

static enum flm_op_result
lradix6_end_dump(void *_data, struct fib_dp *dp)
{
	struct lradix6_data *lr = (struct lradix6_data *)_data;

	dp->f = lradix6_lookup;
	dp->arg = lr->rnh;

	return (FLM_SUCCESS);
}

static enum flm_op_result
lradix6_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{

	return (FLM_REBUILD);
}

struct fib_lookup_module flm_radix6_lockless = {
	.flm_name = "radix6_lockless",
	.flm_family = AF_INET6,
	.flm_init_cb = lradix6_init,
	.flm_destroy_cb = lradix6_destroy,
	.flm_dump_rib_item_cb = lradix6_add_route_cb,
	.flm_dump_end_cb = lradix6_end_dump,
	.flm_change_rib_item_cb = lradix6_change_cb,
	.flm_get_pref = lradix6_get_pref,
};

/*
 * Fallback lookup algorithm.
 * This is a simple wrapper around system radix.
 */

struct radix6_data {
	struct fib_data *fd;
	struct rib_head *rh;
};

static struct nhop_object *
radix6_lookup(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh = (struct rib_head *)algo_data;
	struct radix_node *rn;
	struct nhop_object *nh;

	/* Prepare lookup key */
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_addr = *key.addr6,
	};
	if (IN6_IS_SCOPE_LINKLOCAL(key.addr6))
		sin6.sin6_addr.s6_addr16[1] = htons(scopeid & 0xffff);

	nh = NULL;
	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr((void *)&sin6, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0))
		nh = (RNTORT(rn))->rt_nhop;
	RIB_RUNLOCK(rh);

	return (nh);
}

struct nhop_object *
fib6_radix_lookup_nh(uint32_t fibnum, const struct in6_addr *dst6, uint32_t scopeid)
{
	struct rib_head *rh = rh = rt_tables_get_rnh(fibnum, AF_INET6);
	const struct flm_lookup_key key = { .addr6 = dst6 };

	if (rh == NULL)
		return (NULL);

	return (radix6_lookup(rh, key, scopeid));
}

static uint8_t
radix6_get_pref(const struct rib_rtable_info *rinfo)
{

	return (50);
}

static enum flm_op_result
radix6_init(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **_data)
{
	struct radix6_data *r6;

	r6 = malloc(sizeof(struct radix6_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (r6 == NULL)
		return (FLM_REBUILD);
	r6->fd = fd;
	r6->rh = fib_get_rh(fd);

	*_data = r6;

	return (FLM_SUCCESS);
}

static void
radix6_destroy(void *_data)
{

	free(_data, M_RTABLE);
}

static enum flm_op_result
radix6_add_route_cb(struct rtentry *rt, void *_data)
{

	return (FLM_SUCCESS);
}

static enum flm_op_result
radix6_end_dump(void *_data, struct fib_dp *dp)
{
	struct radix6_data *r6 = (struct radix6_data *)_data;

	dp->f = radix6_lookup;
	dp->arg = r6->rh;

	return (FLM_SUCCESS);
}

static enum flm_op_result
radix6_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{

	return (FLM_SUCCESS);
}

struct fib_lookup_module flm_radix6 = {
	.flm_name = "radix6",
	.flm_family = AF_INET6,
	.flm_init_cb = radix6_init,
	.flm_destroy_cb = radix6_destroy,
	.flm_dump_rib_item_cb = radix6_add_route_cb,
	.flm_dump_end_cb = radix6_end_dump,
	.flm_change_rib_item_cb = radix6_change_cb,
	.flm_get_pref = radix6_get_pref,
};

static void
fib6_algo_init(void)
{

	fib_module_register(&flm_radix6_lockless);
	fib_module_register(&flm_radix6);
}
SYSINIT(fib6_algo_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, fib6_algo_init, NULL);
