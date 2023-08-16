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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/ip.h>

#include <netinet6/in6_fib.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/fib_algo.h>

#define	CHUNK_SIZE	10000

VNET_DEFINE_STATIC(struct in_addr *, inet_addr_list);
#define	V_inet_addr_list	VNET(inet_addr_list)
VNET_DEFINE_STATIC(int, inet_list_size);
#define	V_inet_list_size	VNET(inet_list_size)

VNET_DEFINE_STATIC(struct in6_addr *, inet6_addr_list);
#define	V_inet6_addr_list	VNET(inet6_addr_list)
VNET_DEFINE_STATIC(int, inet6_list_size);
#define	V_inet6_list_size	VNET(inet6_list_size)

SYSCTL_DECL(_net_route);
SYSCTL_NODE(_net_route, OID_AUTO, test, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Route algorithm lookups");

static int
add_addr(int family, char *addr_str)
{

	if (family == AF_INET) {
		struct in_addr *paddr_old = V_inet_addr_list;
		int size_old = V_inet_list_size;
		struct in_addr addr;

		if (inet_pton(AF_INET, addr_str, &addr) != 1)
			return (EINVAL);

		struct in_addr *paddr = mallocarray(size_old + 1,
		    sizeof(struct in_addr), M_TEMP, M_ZERO | M_WAITOK);

		if (paddr_old != NULL) {
			memcpy(paddr, paddr_old, size_old * sizeof(struct in_addr));
			free(paddr_old, M_TEMP);
		}
		paddr[size_old] = addr;

		V_inet_addr_list = paddr;
		V_inet_list_size = size_old + 1;
		inet_ntop(AF_INET, &addr, addr_str, sizeof(addr_str));
	} else if (family == AF_INET6) {
		struct in6_addr *paddr_old = V_inet6_addr_list;
		int size_old = V_inet6_list_size;
		struct in6_addr addr6;

		if (inet_pton(AF_INET6, addr_str, &addr6) != 1)
			return (EINVAL);

		struct in6_addr *paddr = mallocarray(size_old + 1,
		    sizeof(struct in6_addr), M_TEMP, M_ZERO | M_WAITOK);

		if (paddr_old != NULL) {
			memcpy(paddr, paddr_old, size_old * sizeof(struct in6_addr));
			free(paddr_old, M_TEMP);
		}
		paddr[size_old] = addr6;

		V_inet6_addr_list = paddr;
		V_inet6_list_size = size_old + 1;
		inet_ntop(AF_INET6, &addr6, addr_str, sizeof(addr_str));
	}

	return (0);
}

static int
add_addr_sysctl_handler(struct sysctl_oid *oidp, struct sysctl_req *req, int family)
{
	char addr_str[INET6_ADDRSTRLEN];
	int error;

	bzero(addr_str, sizeof(addr_str));

	error = sysctl_handle_string(oidp, addr_str, sizeof(addr_str), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = add_addr(family, addr_str);

	return (0);
}

static int
add_inet_addr_sysctl_handler(SYSCTL_HANDLER_ARGS)
{

	return (add_addr_sysctl_handler(oidp, req, AF_INET));
}
SYSCTL_PROC(_net_route_test, OID_AUTO, add_inet_addr,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    add_inet_addr_sysctl_handler, "A", "Set");

static int
add_inet6_addr_sysctl_handler(SYSCTL_HANDLER_ARGS)
{

	return (add_addr_sysctl_handler(oidp, req, AF_INET6));
}
SYSCTL_PROC(_net_route_test, OID_AUTO, add_inet6_addr,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    add_inet6_addr_sysctl_handler, "A", "Set");

static uint64_t
run_test_inet_one_pass(uint32_t fibnum)
{
	/* Assume epoch */
	int sz = V_inet_list_size;
	int tries = CHUNK_SIZE / sz;
	const struct in_addr *a = V_inet_addr_list;
	uint64_t count = 0;

	for (int pass = 0; pass < tries; pass++) {
		for (int i = 0; i < sz; i++) {
			fib4_lookup(fibnum, a[i], 0, NHR_NONE, 0);
			count++;
		}
	}
	return (count);
}

static int
run_test_inet(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;

	int count = 0;
	int error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);

	if (count == 0)
		return (0);

	if (V_inet_list_size <= 0)
		return (ENOENT);

	printf("run: %d packets vnet %p\n", count, curvnet);
	if (count < CHUNK_SIZE)
		count = CHUNK_SIZE;

	struct timespec ts_pre, ts_post;
	int64_t pass_diff, total_diff = 0;
	uint64_t pass_packets, total_packets = 0;
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet_one_pass(fibnum);
		nanouptime(&ts_post);
		NET_EPOCH_EXIT(et);

		pass_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
		    (ts_post.tv_nsec - ts_pre.tv_nsec);
		total_diff += pass_diff;
		total_packets += pass_packets;
	}

	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet, "I", "Execute fib4_lookup test");

static uint64_t
run_test_inet6_one_pass(uint32_t fibnum)
{
	/* Assume epoch */
	int sz = V_inet6_list_size;
	int tries = CHUNK_SIZE / sz;
	const struct in6_addr *a = V_inet6_addr_list;
	uint64_t count = 0;

	for (int pass = 0; pass < tries; pass++) {
		for (int i = 0; i < sz; i++) {
			fib6_lookup(fibnum, &a[i], 0, NHR_NONE, 0);
			count++;
		}
	}
	return (count);
}

static int
run_test_inet6(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;

	int count = 0;
	int error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);

	if (count == 0)
		return (0);

	if (V_inet6_list_size <= 0)
		return (ENOENT);

	printf("run: %d packets vnet %p\n", count, curvnet);
	if (count < CHUNK_SIZE)
		count = CHUNK_SIZE;

	struct timespec ts_pre, ts_post;
	int64_t pass_diff, total_diff = 0;
	uint64_t pass_packets, total_packets = 0;
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet6_one_pass(fibnum);
		nanouptime(&ts_post);
		NET_EPOCH_EXIT(et);

		pass_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
		    (ts_post.tv_nsec - ts_pre.tv_nsec);
		total_diff += pass_diff;
		total_packets += pass_packets;
	}

	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet6,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet6, "I", "Execute fib6_lookup test");

static bool
cmp_dst(uint32_t fibnum, struct in_addr a)
{
	struct nhop_object *nh_fib;
	struct rtentry *rt;
	struct route_nhop_data rnd = {};

	nh_fib = fib4_lookup(fibnum, a, 0, NHR_NONE, 0);
	rt = fib4_lookup_rt(fibnum, a, 0, NHR_NONE, &rnd);

	if (nh_fib == NULL && rt == NULL) {
		return (true);
	} else if (nh_fib == nhop_select(rnd.rnd_nhop, 0)) {
		return (true);
	}

	struct in_addr dst;
	int plen;
	uint32_t scopeid;
	char key_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &a, key_str, sizeof(key_str));
	if (rnd.rnd_nhop == NULL) {
		printf("[RT BUG] lookup for %s: RIB: ENOENT FIB: nh=%u\n",
		    key_str, nhop_get_idx(nh_fib));
	} else {
		rt_get_inet_prefix_plen(rt, &dst, &plen, &scopeid);
		inet_ntop(AF_INET, &dst, dst_str, sizeof(dst_str));
		printf("[RT BUG] lookup for %s: RIB: %s/%d,nh=%u FIB: nh=%u\n",
		    key_str, dst_str, plen,
		    nhop_get_idx(nhop_select(rnd.rnd_nhop, 0)),
		    nh_fib ? nhop_get_idx(nh_fib) : 0);
	}

	return (false);
}

static bool
cmp_dst6(uint32_t fibnum, const struct in6_addr *a)
{
	struct nhop_object *nh_fib;
	struct rtentry *rt;
	struct route_nhop_data rnd = {};

	nh_fib = fib6_lookup(fibnum, a, 0, NHR_NONE, 0);
	rt = fib6_lookup_rt(fibnum, a, 0, NHR_NONE, &rnd);

	if (nh_fib == NULL && rt == NULL) {
		return (true);
	} else if (nh_fib == nhop_select(rnd.rnd_nhop, 0)) {
		return (true);
	}

	struct in6_addr dst;
	int plen;
	uint32_t scopeid;
	char key_str[INET6_ADDRSTRLEN], dst_str[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, a, key_str, sizeof(key_str));
	if (rnd.rnd_nhop == NULL) {
		printf("[RT BUG] lookup for %s: RIB: ENOENT FIB: nh=%u\n",
		    key_str, nhop_get_idx(nh_fib));
	} else {
		rt_get_inet6_prefix_plen(rt, &dst, &plen, &scopeid);
		inet_ntop(AF_INET6, &dst, dst_str, sizeof(dst_str));
		printf("[RT BUG] lookup for %s: RIB: %s/%d,nh=%u FIB: nh=%u\n",
		    key_str, dst_str, plen,
		    nhop_get_idx(nhop_select(rnd.rnd_nhop, 0)),
		    nh_fib ? nhop_get_idx(nh_fib) : 0);
	}

	return (false);
}

/* Random lookups: correctness verification */
static uint64_t
run_test_inet_one_pass_random(uint32_t fibnum)
{
	/* Assume epoch */
	struct in_addr a[64];
	int sz = 64;
	uint64_t count = 0;

	for (int pass = 0; pass < CHUNK_SIZE / sz; pass++) {
		arc4random_buf(a, sizeof(a));
		for (int i = 0; i < sz; i++) {
			if (!cmp_dst(fibnum, a[i]))
				return (0);
			count++;
		}
	}
	return (count);
}

static int
run_test_inet_random(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;

	int count = 0;
	int error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);

	if (count == 0)
		return (0);

	if (count < CHUNK_SIZE)
		count = CHUNK_SIZE;

	struct timespec ts_pre, ts_post;
	int64_t pass_diff, total_diff = 1;
	uint64_t pass_packets, total_packets = 0;
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet_one_pass_random(fibnum);
		nanouptime(&ts_post);
		NET_EPOCH_EXIT(et);

		pass_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
		    (ts_post.tv_nsec - ts_pre.tv_nsec);
		total_diff += pass_diff;
		total_packets += pass_packets;

		if (pass_packets == 0)
			break;
	}

	/* Signal error to userland */
	if (pass_packets == 0)
		return (EINVAL);

	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet_random,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet_random, "I", "Execute fib4_lookup random check tests");


struct inet_array {
	uint32_t alloc_items;
	uint32_t num_items;
	uint32_t rnh_prefixes;
	int error;
	struct in_addr *arr;
};

/*
 * For each prefix, add the following records to the lookup array:
 * * prefix-1, prefix, prefix + 1, prefix_end, prefix_end + 1
 */
static int
add_prefix(struct rtentry *rt, void *_data)
{
	struct inet_array *pa = (struct inet_array *)_data;
	struct in_addr addr;
	int plen;
	uint32_t scopeid, haddr;

	pa->rnh_prefixes++;

	if (pa->num_items + 5 >= pa->alloc_items) {
		if (pa->error == 0)
			pa->error = ENOSPC;
		return (0);
	}

	rt_get_inet_prefix_plen(rt, &addr, &plen, &scopeid);

	pa->arr[pa->num_items++] = addr;
	haddr = ntohl(addr.s_addr);
	if (haddr > 0) {
		pa->arr[pa->num_items++].s_addr = htonl(haddr - 1);
		pa->arr[pa->num_items++].s_addr = htonl(haddr + 1);
		/* assume mask != 0 */
		uint32_t mlen = (1 << (32 - plen)) - 1;
		pa->arr[pa->num_items++].s_addr = htonl(haddr + mlen);
		/* can overflow, but who cares */
		pa->arr[pa->num_items++].s_addr = htonl(haddr + mlen + 1);
	}

	return (0);
}

static bool
prepare_list(uint32_t fibnum, struct inet_array *pa)
{
	struct rib_head *rh;

	rh = rt_tables_get_rnh(fibnum, AF_INET);

	uint32_t num_prefixes = rh->rnh_prefixes;
	bzero(pa, sizeof(struct inet_array));
	pa->alloc_items = (num_prefixes + 10) * 5;
	pa->arr = mallocarray(pa->alloc_items, sizeof(struct in_addr),
	    M_TEMP, M_ZERO | M_WAITOK);

	rib_walk(fibnum, AF_INET, false, add_prefix, pa);

	if (pa->error != 0) {
		printf("prefixes: old: %u, current: %u, walked: %u, allocated: %u\n",
		    num_prefixes, rh->rnh_prefixes, pa->rnh_prefixes, pa->alloc_items);
	}

	return (pa->error == 0);
}

static int
run_test_inet_scan(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;

	int count = 0;
	int error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);

	if (count == 0)
		return (0);

	struct inet_array pa = {};
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	if (!prepare_list(fibnum, &pa))
		return (pa.error);

	struct timespec ts_pre, ts_post;
	int64_t total_diff = 1;
	uint64_t total_packets = 0;
	int failure_count = 0;

	NET_EPOCH_ENTER(et);
	nanouptime(&ts_pre);
	for (int i = 0; i < pa.num_items; i++) {
		if (!cmp_dst(fibnum, pa.arr[i])) {
			failure_count++;
		}
		total_packets++;
	}
	nanouptime(&ts_post);
	NET_EPOCH_EXIT(et);

	if (pa.arr != NULL)
		free(pa.arr, M_TEMP);

	/* Signal error to userland */
	if (failure_count > 0) {
		printf("[RT ERROR] total failures: %d\n", failure_count);
		return (EINVAL);
	}

	total_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
	    (ts_post.tv_nsec - ts_pre.tv_nsec);
	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet_scan,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet_scan, "I", "Execute fib4_lookup scan tests");

struct inet6_array {
	uint32_t alloc_items;
	uint32_t num_items;
	uint32_t rnh_prefixes;
	int error;
	struct in6_addr *arr;
};

static bool
safe_add(uint32_t *v, uint32_t inc)
{
	if (*v < (UINT32_MAX - inc)) {
		*v += inc;
		return (true);
	} else {
		*v -= (UINT32_MAX - inc + 1);
		return (false);
	}
}

static bool
safe_dec(uint32_t *v, uint32_t inc)
{
	if (*v >= inc) {
		*v -= inc;
		return (true);
	} else {
		*v += (UINT32_MAX - inc + 1);
		return (false);
	}
}

static void
inc_prefix6(struct in6_addr *addr, int inc)
{
	for (int i = 0; i < 4; i++) {
		uint32_t v = ntohl(addr->s6_addr32[3 - i]);
		bool ret = safe_add(&v, inc);
		addr->s6_addr32[3 - i] = htonl(v);
		if (ret)
			return;
		inc = 1;
	}
}

static void
dec_prefix6(struct in6_addr *addr, int dec)
{
	for (int i = 0; i < 4; i++) {
		uint32_t v = ntohl(addr->s6_addr32[3 - i]);
		bool ret = safe_dec(&v, dec);
		addr->s6_addr32[3 - i] = htonl(v);
		if (ret)
			return;
		dec = 1;
	}
}

static void
ipv6_writemask(struct in6_addr *addr6, uint8_t mask)
{
	uint32_t *cp;

	for (cp = (uint32_t *)addr6; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	if (mask > 0)
		*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}

/*
 * For each prefix, add the following records to the lookup array:
 * * prefix-1, prefix, prefix + 1, prefix_end, prefix_end + 1
 */
static int
add_prefix6(struct rtentry *rt, void *_data)
{
	struct inet6_array *pa = (struct inet6_array *)_data;
	struct in6_addr addr, naddr;
	int plen;
	uint32_t scopeid;

	pa->rnh_prefixes++;

	if (pa->num_items + 5 >= pa->alloc_items) {
		if (pa->error == 0)
			pa->error = ENOSPC;
		return (0);
	}

	rt_get_inet6_prefix_plen(rt, &addr, &plen, &scopeid);

	pa->arr[pa->num_items++] = addr;
	if (!IN6_ARE_ADDR_EQUAL(&addr, &in6addr_any)) {
		naddr = addr;
		dec_prefix6(&naddr, 1);
		pa->arr[pa->num_items++] = naddr;
		naddr = addr;
		inc_prefix6(&naddr, 1);
		pa->arr[pa->num_items++] = naddr;

		/* assume mask != 0 */
		struct in6_addr mask6;
		ipv6_writemask(&mask6, plen);
		naddr = addr;
		for (int i = 0; i < 3; i++)
			naddr.s6_addr32[i] = htonl(ntohl(naddr.s6_addr32[i]) | ~ntohl(mask6.s6_addr32[i]));

		pa->arr[pa->num_items++] = naddr;
		inc_prefix6(&naddr, 1);
		pa->arr[pa->num_items++] = naddr;
	}

	return (0);
}

static bool
prepare_list6(uint32_t fibnum, struct inet6_array *pa)
{
	struct rib_head *rh;

	rh = rt_tables_get_rnh(fibnum, AF_INET6);

	uint32_t num_prefixes = rh->rnh_prefixes;
	bzero(pa, sizeof(struct inet6_array));
	pa->alloc_items = (num_prefixes + 10) * 5;
	pa->arr = mallocarray(pa->alloc_items, sizeof(struct in6_addr),
	    M_TEMP, M_ZERO | M_WAITOK);

	rib_walk(fibnum, AF_INET6, false, add_prefix6, pa);

	if (pa->error != 0) {
		printf("prefixes: old: %u, current: %u, walked: %u, allocated: %u\n",
		    num_prefixes, rh->rnh_prefixes, pa->rnh_prefixes, pa->alloc_items);
	}

	return (pa->error == 0);
}

static int
run_test_inet6_scan(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;

	int count = 0;
	int error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);

	if (count == 0)
		return (0);

	struct inet6_array pa = {};
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	if (!prepare_list6(fibnum, &pa))
		return (pa.error);

	struct timespec ts_pre, ts_post;
	int64_t total_diff = 1;
	uint64_t total_packets = 0;
	int failure_count = 0;

	NET_EPOCH_ENTER(et);
	nanouptime(&ts_pre);
	for (int i = 0; i < pa.num_items; i++) {
		if (!cmp_dst6(fibnum, &pa.arr[i])) {
			failure_count++;
		}
		total_packets++;
	}
	nanouptime(&ts_post);
	NET_EPOCH_EXIT(et);

	if (pa.arr != NULL)
		free(pa.arr, M_TEMP);

	/* Signal error to userland */
	if (failure_count > 0) {
		printf("[RT ERROR] total failures: %d\n", failure_count);
		return (EINVAL);
	}

	total_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
	    (ts_post.tv_nsec - ts_pre.tv_nsec);
	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet6_scan,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet6_scan, "I", "Execute fib6_lookup scan tests");

#define	LPS_SEQ		0x1
#define	LPS_ANN		0x2
#define	LPS_REP		0x4

struct lps_walk_state {
	uint32_t *keys;
	int pos;
	int lim;
};

static int
reduce_keys(struct rtentry *rt, void *_data)
{
        struct lps_walk_state *wa = (struct lps_walk_state *) _data;
	struct in_addr addr;
	uint32_t scopeid;
	int plen;

	rt_get_inet_prefix_plen(rt, &addr, &plen, &scopeid);
	wa->keys[wa->pos] = ntohl(addr.s_addr) |
	    (wa->keys[wa->pos] & ~(0xffffffffU << (32 - plen)));

	wa->pos++;
	return (wa->pos == wa->lim);
}

static int
rnd_lps(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;
	struct in_addr key;
	struct lps_walk_state wa;
	struct timespec ts_pre, ts_post;
	struct nhop_object *nh_fib;
	uint64_t total_diff, lps;
	uint32_t *keys, fibnum;
	uint32_t t, p;
	uintptr_t acc = 0;
	int i, pos, count = 0;
	int seq = 0, rep = 0;
	int error;

	error = sysctl_handle_int(oidp, &count, 0, req);
	if (error != 0)
		return (error);
	if (count <= 0)
		return (0);
	fibnum = curthread->td_proc->p_fibnum;

	keys = malloc(sizeof(*keys) * count, M_TEMP, M_NOWAIT);
	if (keys == NULL)
		return (ENOMEM);
	printf("Preparing %d random keys...\n", count);
	arc4random_buf(keys, sizeof(*keys) * count);
	if (arg2 & LPS_ANN) {
		wa.keys = keys;
		wa.pos = 0;
		wa.lim = count;
		printf("Reducing keys to announced address space...\n");
		do {
			rib_walk(fibnum, AF_INET, false, reduce_keys,
			    &wa);
		} while (wa.pos < wa.lim);
		printf("Reshuffling keys...\n");
		for (int i = 0; i < count; i++) {
			p = random() % count;
			t = keys[p];
			keys[p] = keys[i];
			keys[i] = t;
		}
	}

	if (arg2 & LPS_REP) {
		rep = 1;
		printf("REP ");
	}
	if (arg2 & LPS_SEQ) {
		seq = 1;
		printf("SEQ");
	} else if (arg2 & LPS_ANN)
		printf("ANN");
	else
		printf("RND");
	printf(" LPS test starting...\n");

	NET_EPOCH_ENTER(et);
	nanouptime(&ts_pre);
	for (i = 0, pos = 0; i < count; i++) {
		key.s_addr = keys[pos++] ^ ((acc >> 10) & 0xff);
		nh_fib = fib4_lookup(fibnum, key, 0, NHR_NONE, 0);
		if (seq) {
			if (nh_fib != NULL) {
				acc += (uintptr_t) nh_fib + 123;
				if (acc & 0x1000)
					acc += (uintptr_t) nh_fib->nh_ifp;
				else
					acc -= (uintptr_t) nh_fib->nh_ifp;
			} else
				acc ^= (acc >> 3) + (acc << 2) + i;
			if (acc & 0x800)
				pos++;
			if (pos >= count)
				pos = 0;
		}
		if (rep && ((i & 0xf) == 0xf)) {
			pos -= 0xf;
			if (pos < 0)
				pos += 0xf;
		}
	}
	nanouptime(&ts_post);
	NET_EPOCH_EXIT(et);

	free(keys, M_TEMP);

	total_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
	    (ts_post.tv_nsec - ts_pre.tv_nsec);
	lps = 1000000000ULL * count / total_diff;
	printf("%d lookups in %zu.%06zu milliseconds, %lu.%06lu MLPS\n",
	    count, total_diff / 1000000, total_diff % 1000000,
	    lps / 1000000, lps % 1000000);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_rnd,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, rnd_lps, "I",
    "Measure lookups per second, uniformly random keys, independent lookups");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_rnd_ann,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_ANN, rnd_lps, "I",
    "Measure lookups per second, random keys from announced address space, "
    "independent lookups");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_seq,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_SEQ, rnd_lps, "I",
    "Measure lookups per second, uniformly random keys, "
    "artificial dependencies between lookups");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_seq_ann,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_SEQ | LPS_ANN, rnd_lps, "I",
    "Measure lookups per second, random keys from announced address space, "
    "artificial dependencies between lookups");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_rnd_rep,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_REP, rnd_lps, "I",
    "Measure lookups per second, uniformly random keys, independent lookups, "
    "repeated keys");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_rnd_ann_rep,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_ANN | LPS_REP, rnd_lps, "I",
    "Measure lookups per second, random keys from announced address space, "
    "independent lookups, repeated keys");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_seq_rep,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_SEQ | LPS_REP, rnd_lps, "I",
    "Measure lookups per second, uniformly random keys, "
    "artificial dependencies between lookups, repeated keys");
SYSCTL_PROC(_net_route_test, OID_AUTO, run_lps_seq_ann_rep,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, LPS_SEQ | LPS_ANN | LPS_REP, rnd_lps, "I",
    "Measure lookups per second, random keys from announced address space, "
    "artificial dependencies between lookups, repeated keys");

static int
test_fib_lookup_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		if (V_inet_addr_list != NULL)
			free(V_inet_addr_list, M_TEMP);
		if (V_inet6_addr_list != NULL)
			free(V_inet6_addr_list, M_TEMP);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t testfiblookupmod = {
        "test_fib_lookup",
        test_fib_lookup_modevent,
        0
};

DECLARE_MODULE(testfiblookupmod, testfiblookupmod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(testfiblookup, 1);
