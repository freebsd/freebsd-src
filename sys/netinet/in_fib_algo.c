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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/vnet.h>

#include <net/if.h>
#include <netinet/in.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/fib_algo.h>

/*
 * Binary search lookup algo.
 *
 * Compiles route table into a sorted array.
 * Used with small amount of routes (< 16).
 * As array is immutable, it is rebuild on each rtable change.
 *
 * Example:
 *
 * 0.0.0.0/0 -> nh1
 * 10.0.0.0/24 -> nh2
 * 10.0.0.1/32 -> nh3
 *
 * gets compiled to:
 *
 * 0.0.0.0 -> nh1
 * 10.0.0.0 -> nh2
 * 10.0.0.1 -> nh3
 * 10.0.0.2 -> nh2
 * 10.0.1.0 -> nh1
 *
 */

struct bsearch4_record {
	uint32_t		addr4;
	uint32_t		mask4;
	struct nhop_object	*nh;
};

struct bsearch4_data {
	struct fib_data		*fd;
	uint32_t		alloc_items;
	uint32_t		num_items;
	void			*mem;
	struct bsearch4_record	*rr;
	struct bsearch4_record	br[0];
};

/*
 * Main IPv4 address lookup function.
 *
 * Finds array record with maximum index that is <= provided key.
 * Assumes 0.0.0.0/0 always exists (may be with NULL nhop)
 */
static struct nhop_object *
bsearch4_lookup(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	const struct bsearch4_data *bd = (const struct bsearch4_data *)algo_data;
	const struct bsearch4_record *br;
	uint32_t addr4 = ntohl(key.addr4.s_addr);

	int start = 0;
	int end = bd->num_items;

	int i = (start + end) / 2;
	while (start + 1 < end) {
		i = (start + end) / 2;
		br = &bd->br[i];
		if (addr4 < br->addr4) {
			/* key < average, reduce right boundary */
			end = i;
			continue;
		} else if (addr4 > br->addr4) {
			/* key > average, increase left aboundary */
			start = i;
			continue;
		} else {
			/* direct match */
			return (br->nh);
		}
	}
	/* start + 1 == end */
	return (bd->br[start].nh);
}

/*
 * Preference function.
 * Assume ideal for < 10 (typical single-interface setup has 5)
 * Then gradually degrade.
 * Assume 30 prefixes is at least 60 records, so it will require 8 lookup,
 *  which is even worse than radix.
 */
static uint8_t
bsearch4_get_pref(const struct rib_rtable_info *rinfo)
{

	if (rinfo->num_prefixes < 10)
		return (253);
	else if (rinfo->num_prefixes < 30)
		return (255 - rinfo->num_prefixes * 8);
	else
		return (1);
}

static enum flm_op_result
bsearch4_init(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **_data)
{
	struct bsearch4_data *bd;
	struct rib_rtable_info rinfo;
	uint32_t count;
	size_t sz;
	void *mem;

	fib_get_rtable_info(fib_get_rh(fd), &rinfo);
	count = rinfo.num_prefixes * 11 / 10 + 64;

	sz = sizeof(struct bsearch4_data) + sizeof(struct bsearch4_record) * count;
	/* add cache line sz to ease alignment */
	sz += CACHE_LINE_SIZE;
	mem = malloc(sz, M_RTABLE, M_NOWAIT | M_ZERO);
	if (mem == NULL)
		return (FLM_REBUILD);
	/* Align datapath-usable structure to cache line boundary */
	bd = (struct bsearch4_data *)roundup2((uintptr_t)mem, CACHE_LINE_SIZE);
	bd->mem = mem;
	bd->alloc_items = count;
	bd->fd = fd;

	*_data = bd;

	/*
	 * Allocate temporary array to store all rtable data.
	 * This step is required to provide the required prefix iteration order.
	 */
	bd->rr = mallocarray(count, sizeof(struct bsearch4_record), M_TEMP, M_NOWAIT | M_ZERO);
	if (bd->rr == NULL)
		return (FLM_REBUILD);

	return (FLM_SUCCESS);
}

static void
bsearch4_destroy(void *_data)
{
	struct bsearch4_data *bd = (struct bsearch4_data *)_data;

	if (bd->rr != NULL)
		free(bd->rr, M_TEMP);
	free(bd->mem, M_RTABLE);
}

/*
 * Callback storing converted rtable prefixes in the temporary array.
 * Addresses are converted to a host order.
 */
static enum flm_op_result
bsearch4_add_route_cb(struct rtentry *rt, void *_data)
{
	struct bsearch4_data *bd = (struct bsearch4_data *)_data;
	struct bsearch4_record *rr;
	struct in_addr addr4, mask4;
	uint32_t scopeid;

	if (bd->num_items >= bd->alloc_items)
		return (FLM_REBUILD);

	rr = &bd->rr[bd->num_items++];
	rt_get_inet_prefix_pmask(rt, &addr4, &mask4, &scopeid);
	rr->addr4 = ntohl(addr4.s_addr);
	rr->mask4 = ntohl(mask4.s_addr);
	rr->nh = rt_get_raw_nhop(rt);

	return (FLM_SUCCESS);
}

/*
 * Prefix comparison function.
 * 10.0.0.0/24 < 10.0.0.0/25 <- less specific wins
 * 10.0.0.0/25 < 10.0.0.1/32 <- bigger base wins
 */
static int
rr_cmp(const void *_rec1, const void *_rec2)
{
	const struct bsearch4_record *rec1, *rec2;
	rec1 = _rec1;
	rec2 = _rec2;

	if (rec1->addr4 < rec2->addr4)
		return (-1);
	else if (rec1->addr4 > rec2->addr4)
		return (1);

	/*
	 * wider mask value is lesser mask
	 * we want less specific come first, e.g. <
	 */
	if (rec1->mask4 < rec2->mask4)
		return (-1);
	else if (rec1->mask4 > rec2->mask4)
		return (1);
	return (0);
}

struct bsearch4_array {
	uint32_t		alloc_items;
	uint32_t		num_items;
	struct bsearch4_record	*arr;
};

static bool
add_array_entry(struct bsearch4_array *ba, struct bsearch4_record *br_new)
{

	if (ba->num_items < ba->alloc_items) {
		ba->arr[ba->num_items++] = *br_new;
		return (true);
	}
	return (false);
}

static struct bsearch4_record *
get_last_entry(struct bsearch4_array *ba)
{

	return (&ba->arr[ba->num_items - 1]);
}

/*
 *
 * Example:
 *  stack: 10.0.1.0/24,nh=3 array: 10.0.1.0/25,nh=4 -> ++10.0.1.128/24,nh=3
 *
 *
 */
static bool
pop_stack_entry(struct bsearch4_array *dst_array, struct bsearch4_array *stack)
{
	uint32_t last_stack_addr, last_array_addr;

	struct bsearch4_record *br_prev = get_last_entry(dst_array);
	struct bsearch4_record *pstack = get_last_entry(stack);

	/* Regardless of the result, pop stack entry */
	stack->num_items--;

	/* Prefix last address for the last entry in lookup array */
	last_array_addr = (br_prev->addr4 | ~br_prev->mask4);
	/* Prefix last address for the stack record entry */
	last_stack_addr = (pstack->addr4 | ~pstack->mask4);

	if (last_stack_addr > last_array_addr) {
		/*
		 * Stack record covers > address space than
		 * the last entry in the lookup array.
		 * Add the remaining parts of a stack record to
		 * the lookup array.
		 */
		struct bsearch4_record br_new = {
			.addr4 = last_array_addr + 1,
			.mask4 = pstack->mask4,
			.nh = pstack->nh,
		};
		return (add_array_entry(dst_array, &br_new));
	}

	return (true);
}

/*
 * Updates resulting array @dst_array with a rib entry @rib_entry.
 */
static bool
bsearch4_process_record(struct bsearch4_array *dst_array,
    struct bsearch4_array *stack, struct bsearch4_record *rib_entry)
{

	/*
	 * Maintain invariant: current rib_entry is always contained
	 *  in the top stack entry.
	 * Note we always have 0.0.0.0/0.
	 */
	while (stack->num_items > 0) {
		struct bsearch4_record *pst = get_last_entry(stack);

		/*
		 * Check if we need to pop stack.
		 * Rely on the ordering - larger prefixes comes up first
		 * Pop any entry that doesn't contain current prefix.
		 */
		if (pst->addr4 == (rib_entry->addr4 & pst->mask4))
			break;

		if (!pop_stack_entry(dst_array, stack))
			return (false);
	}

	 if (dst_array->num_items > 0) {

		 /*
		  * Check if there is a gap between previous entry and a
		  *  current entry. Code above guarantees that both previous
		  *  and current entry are contained in the top stack entry.
		  *
		  * Example: last: 10.0.0.1(/32,nh=3) cur: 10.0.0.3(/32,nh=4),
		  *  stack: 10.0.0.0/24,nh=2.
		  * Cover a gap between previous and current by adding stack
		  *  nexthop.
		  */
		 struct bsearch4_record *br_tmp = get_last_entry(dst_array);
		 uint32_t last_declared_addr = br_tmp->addr4 | ~br_tmp->mask4;
		 if (last_declared_addr < rib_entry->addr4 - 1) {
			 /* Cover a hole */
			struct bsearch4_record *pst = get_last_entry(stack);
			struct bsearch4_record new_entry = {
				.addr4 = last_declared_addr + 1,
				.mask4 = pst->mask4,
				.nh = pst->nh,
			};
			if (!add_array_entry(dst_array, &new_entry))
				return (false);
		 }
	 }

	if (!add_array_entry(dst_array, rib_entry))
		return (false);
	add_array_entry(stack, rib_entry);

	return (true);
}

static enum flm_op_result
bsearch4_build_array(struct bsearch4_array *dst_array, struct bsearch4_array *src_array)
{

	/*
	 * During iteration, we keep track of all prefixes in rtable
	 * we currently match, by maintaining stack. As there can be only
	 * 32 prefixes for a single address, pre-allocate stack of size 32.
	 */
	struct bsearch4_array stack = {
		.alloc_items = 32,
		.arr = mallocarray(32, sizeof(struct bsearch4_record), M_TEMP, M_NOWAIT | M_ZERO),
	};
	if (stack.arr == NULL)
		return (FLM_REBUILD);

	for (int i = 0; i < src_array->num_items; i++) {
		struct bsearch4_record *rib_entry = &src_array->arr[i];

		if (!bsearch4_process_record(dst_array, &stack, rib_entry)) {
			free(stack.arr, M_TEMP);
			return (FLM_REBUILD);
		}
	}

	/*
	 * We know that last record is contained in the top stack entry.
	 */
	while (stack.num_items > 0) {
		if (!pop_stack_entry(dst_array, &stack))
			return (FLM_REBUILD);
	}
	free(stack.arr, M_TEMP);

	return (FLM_SUCCESS);
}

static enum flm_op_result
bsearch4_build(struct bsearch4_data *bd)
{
	enum flm_op_result ret;

	struct bsearch4_array prefixes_array = {
		.alloc_items = bd->alloc_items,
		.num_items = bd->num_items,
		.arr = bd->rr,
	};

	/* Add default route if not exists */
	bool default_found = false;
	for (int i = 0; i < prefixes_array.num_items; i++) {
		if (prefixes_array.arr[i].mask4 == 0) {
			default_found = true;
			break;
		}
	}
	if (!default_found) {
		 /* Add default route with NULL nhop */
		struct bsearch4_record default_entry = {};
		if (!add_array_entry(&prefixes_array, &default_entry))
			 return (FLM_REBUILD);
	}

	/* Sort prefixes */
	qsort(prefixes_array.arr, prefixes_array.num_items, sizeof(struct bsearch4_record), rr_cmp);

	struct bsearch4_array dst_array = {
		.alloc_items = bd->alloc_items,
		.arr = bd->br,
	};

	ret = bsearch4_build_array(&dst_array, &prefixes_array);
	bd->num_items = dst_array.num_items;

	free(bd->rr, M_TEMP);
	bd->rr = NULL;
	return (ret);
}


static enum flm_op_result
bsearch4_end_dump(void *_data, struct fib_dp *dp)
{
	struct bsearch4_data *bd = (struct bsearch4_data *)_data;
	enum flm_op_result ret;

	ret = bsearch4_build(bd);
	if (ret == FLM_SUCCESS) {
		dp->f = bsearch4_lookup;
		dp->arg = bd;
	}

	return (ret);
}

static enum flm_op_result
bsearch4_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{

	return (FLM_REBUILD);
}

struct fib_lookup_module flm_bsearch4= {
	.flm_name = "bsearch4",
	.flm_family = AF_INET,
	.flm_init_cb = bsearch4_init,
	.flm_destroy_cb = bsearch4_destroy,
	.flm_dump_rib_item_cb = bsearch4_add_route_cb,
	.flm_dump_end_cb = bsearch4_end_dump,
	.flm_change_rib_item_cb = bsearch4_change_cb,
	.flm_get_pref = bsearch4_get_pref,
};

/*
 * Lockless radix lookup algo.
 *
 * Compiles immutable radix from the current routing table.
 * Used with small amount of routes (<1000).
 * As datastructure is immutable, it gets rebuild on each rtable change.
 *
 * Lookups are slightly faster as shorter lookup keys are used
 *  (4 bytes instead of 8 in stock radix).
 */

#define KEY_LEN_INET	(offsetof(struct sockaddr_in, sin_addr) + sizeof(in_addr_t))
#define OFF_LEN_INET	(8 * offsetof(struct sockaddr_in, sin_addr))
struct radix4_addr_entry {
	struct radix_node	rn[2];
	struct sockaddr_in	addr;
	struct nhop_object	*nhop;
};
#define	LRADIX4_ITEM_SZ	roundup2(sizeof(struct radix4_addr_entry), 64)

struct lradix4_data {
	struct radix_node_head	*rnh;
	struct fib_data		*fd;
	void			*mem;
	char			*rt_base;
	uint32_t		alloc_items;
	uint32_t		num_items;
};

static struct nhop_object *
lradix4_lookup(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	struct radix_node_head *rnh = (struct radix_node_head *)algo_data;
	struct radix4_addr_entry *ent;
	struct sockaddr_in addr4 = {
		.sin_len = KEY_LEN_INET,
		.sin_addr = key.addr4,
	};
	ent = (struct radix4_addr_entry *)(rn_match(&addr4, &rnh->rh));
	if (ent != NULL)
		return (ent->nhop);
	return (NULL);
}

/*
 * Preference function.
 * Assume close-to-ideal of < 10 routes (though worse than bsearch), then
 * gradually degrade until 1000 routes are reached.
 */
static uint8_t
lradix4_get_pref(const struct rib_rtable_info *rinfo)
{

	if (rinfo->num_prefixes < 10)
		return (250);
	else if (rinfo->num_prefixes < 1000)
		return (254 - rinfo->num_prefixes / 4);
	else
		return (1);
}

static enum flm_op_result
lradix4_init(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **_data)
{
	struct lradix4_data *lr;
	struct rib_rtable_info rinfo;
	uint32_t count;
	size_t sz;

	lr = malloc(sizeof(struct lradix4_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (lr == NULL || !rn_inithead((void **)&lr->rnh, OFF_LEN_INET))
		return (FLM_REBUILD);
	fib_get_rtable_info(fib_get_rh(fd), &rinfo);

	count = rinfo.num_prefixes * 11 / 10;
	sz = count * LRADIX4_ITEM_SZ + CACHE_LINE_SIZE;
	lr->mem = malloc(sz, M_RTABLE, M_NOWAIT | M_ZERO);
	if (lr->mem == NULL)
		return (FLM_REBUILD);
	/* Align all rtentries to a cacheline boundary */
	lr->rt_base = (char *)roundup2((uintptr_t)lr->mem, CACHE_LINE_SIZE);
	lr->alloc_items = count;
	lr->fd = fd;

	*_data = lr;

	return (FLM_SUCCESS);
}

static void
lradix4_destroy(void *_data)
{
	struct lradix4_data *lr = (struct lradix4_data *)_data;

	if (lr->rnh != NULL)
		rn_detachhead((void **)&lr->rnh);
	if (lr->mem != NULL)
		free(lr->mem, M_RTABLE);
	free(lr, M_RTABLE);
}

static enum flm_op_result
lradix4_add_route_cb(struct rtentry *rt, void *_data)
{
	struct lradix4_data *lr = (struct lradix4_data *)_data;
	struct radix4_addr_entry *ae;
	struct sockaddr_in mask;
	struct sockaddr *rt_mask;
	struct radix_node *rn;
	struct in_addr addr4, mask4;
	uint32_t scopeid;

	if (lr->num_items >= lr->alloc_items)
		return (FLM_REBUILD);

	ae = (struct radix4_addr_entry *)(lr->rt_base + lr->num_items * LRADIX4_ITEM_SZ);
	lr->num_items++;

	ae->nhop = rt_get_raw_nhop(rt);

	rt_get_inet_prefix_pmask(rt, &addr4, &mask4, &scopeid);
	ae->addr.sin_len = KEY_LEN_INET;
	ae->addr.sin_addr = addr4;

	if (mask4.s_addr != INADDR_BROADCAST) {
		bzero(&mask, sizeof(mask));
		mask.sin_len = KEY_LEN_INET;
		mask.sin_addr = mask4;
		rt_mask = (struct sockaddr *)&mask;
	} else
		rt_mask = NULL;

	rn = lr->rnh->rnh_addaddr((struct sockaddr *)&ae->addr, rt_mask,
	    &lr->rnh->rh, ae->rn);
	if (rn == NULL)
		return (FLM_REBUILD);

	return (FLM_SUCCESS);
}

static enum flm_op_result
lradix4_end_dump(void *_data, struct fib_dp *dp)
{
	struct lradix4_data *lr = (struct lradix4_data *)_data;

	dp->f = lradix4_lookup;
	dp->arg = lr->rnh;

	return (FLM_SUCCESS);
}

static enum flm_op_result
lradix4_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{

	return (FLM_REBUILD);
}

struct fib_lookup_module flm_radix4_lockless = {
	.flm_name = "radix4_lockless",
	.flm_family = AF_INET,
	.flm_init_cb = lradix4_init,
	.flm_destroy_cb = lradix4_destroy,
	.flm_dump_rib_item_cb = lradix4_add_route_cb,
	.flm_dump_end_cb = lradix4_end_dump,
	.flm_change_rib_item_cb = lradix4_change_cb,
	.flm_get_pref = lradix4_get_pref,
};

/*
 * Fallback lookup algorithm.
 * This is a simple wrapper around system radix.
 */

struct radix4_data {
	struct fib_data *fd;
	struct rib_head *rh;
};

static struct nhop_object *
radix4_lookup(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh = (struct rib_head *)algo_data;
	struct radix_node *rn;
	struct nhop_object *nh;

	/* Prepare lookup key */
	struct sockaddr_in sin4 = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr = key.addr4,
	};

	nh = NULL;
	RIB_RLOCK(rh);
	rn = rn_match((void *)&sin4, &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0))
		nh = (RNTORT(rn))->rt_nhop;
	RIB_RUNLOCK(rh);

	return (nh);
}

static uint8_t
radix4_get_pref(const struct rib_rtable_info *rinfo)
{

	return (50);
}

static enum flm_op_result
radix4_init(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **_data)
{
	struct radix4_data *r4;

	r4 = malloc(sizeof(struct radix4_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (r4 == NULL)
		return (FLM_REBUILD);
	r4->fd = fd;
	r4->rh = fib_get_rh(fd);

	*_data = r4;

	return (FLM_SUCCESS);
}

static void
radix4_destroy(void *_data)
{

	free(_data, M_RTABLE);
}

static enum flm_op_result
radix4_add_route_cb(struct rtentry *rt, void *_data)
{

	return (FLM_SUCCESS);
}

static enum flm_op_result
radix4_end_dump(void *_data, struct fib_dp *dp)
{
	struct radix4_data *r4 = (struct radix4_data *)_data;

	dp->f = radix4_lookup;
	dp->arg = r4->rh;

	return (FLM_SUCCESS);
}

static enum flm_op_result
radix4_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{

	return (FLM_SUCCESS);
}

struct fib_lookup_module flm_radix4 = {
	.flm_name = "radix4",
	.flm_family = AF_INET,
	.flm_init_cb = radix4_init,
	.flm_destroy_cb = radix4_destroy,
	.flm_dump_rib_item_cb = radix4_add_route_cb,
	.flm_dump_end_cb = radix4_end_dump,
	.flm_change_rib_item_cb = radix4_change_cb,
	.flm_get_pref = radix4_get_pref,
};

static void
fib4_algo_init(void)
{

	fib_module_register(&flm_bsearch4);
	fib_module_register(&flm_radix4_lockless);
	fib_module_register(&flm_radix4);
}
SYSINIT(fib4_algo_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, fib4_algo_init, NULL);
