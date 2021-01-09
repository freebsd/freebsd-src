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
run_test_inet_one_pass()
{
	/* Assume epoch */
	int sz = V_inet_list_size;
	int tries = CHUNK_SIZE / sz;
	const struct in_addr *a = V_inet_addr_list;
	uint64_t count = 0;

	for (int pass = 0; pass < tries; pass++) {
		for (int i = 0; i < sz; i++) {
			fib4_lookup(RT_DEFAULT_FIB, a[i], 0, NHR_NONE, 0);
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

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet_one_pass();
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
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet, "I", "Execute fib4_lookup test");

static uint64_t
run_test_inet6_one_pass()
{
	/* Assume epoch */
	int sz = V_inet6_list_size;
	int tries = CHUNK_SIZE / sz;
	const struct in6_addr *a = V_inet6_addr_list;
	uint64_t count = 0;

	for (int pass = 0; pass < tries; pass++) {
		for (int i = 0; i < sz; i++) {
			fib6_lookup(RT_DEFAULT_FIB, &a[i], 0, NHR_NONE, 0);
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

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet6_one_pass();
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
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
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
		    nhop_get_idx(nh_fib));
	}

	return (false);
}

/* Random lookups: correctness verification */
static uint64_t
run_test_inet_one_pass_random()
{
	/* Assume epoch */
	struct in_addr a[64];
	int sz = 64;
	uint64_t count = 0;

	for (int pass = 0; pass < CHUNK_SIZE / sz; pass++) {
		arc4random_buf(a, sizeof(a));
		for (int i = 0; i < sz; i++) {
			if (!cmp_dst(RT_DEFAULT_FIB, a[i]))
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

	for (int pass = 0; pass < count / CHUNK_SIZE; pass++) {
		NET_EPOCH_ENTER(et);
		nanouptime(&ts_pre);
		pass_packets = run_test_inet_one_pass_random();
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
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet_random, "I", "Execute fib4_lookup random check tests");


struct inet_array {
	uint32_t alloc_items;
	uint32_t num_items;
	struct in_addr *arr;
	int error;
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

	if (pa->num_items + 5 >= pa->alloc_items) {
		if (pa->error == 0)
			pa->error = EINVAL;
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

	uint32_t num_prefixes = (rh->rnh_prefixes + 10) * 5;
	bzero(pa, sizeof(struct inet_array));
	pa->alloc_items = num_prefixes;
	pa->arr = mallocarray(num_prefixes, sizeof(struct in_addr),
	    M_TEMP, M_ZERO | M_WAITOK);

	rib_walk(RT_DEFAULT_FIB, AF_INET, false, add_prefix, pa);

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

	if (!prepare_list(RT_DEFAULT_FIB, &pa))
		return (pa.error);

	struct timespec ts_pre, ts_post;
	int64_t total_diff = 1;
	uint64_t total_packets = 0;

	NET_EPOCH_ENTER(et);
	nanouptime(&ts_pre);
	for (int i = 0; i < pa.num_items; i++) {
		if (!cmp_dst(RT_DEFAULT_FIB, pa.arr[i])) {
			error = EINVAL;
			break;
		}
		total_packets++;
	}
	nanouptime(&ts_post);
	NET_EPOCH_EXIT(et);

	if (pa.arr != NULL)
		free(pa.arr, M_TEMP);

	/* Signal error to userland */
	if (error != 0)
		return (error);

	total_diff = (ts_post.tv_sec - ts_pre.tv_sec) * 1000000000 +
	    (ts_post.tv_nsec - ts_pre.tv_nsec);
	printf("%zu packets in %zu nanoseconds, %zu pps\n",
	    total_packets, total_diff, total_packets * 1000000000 / total_diff);

	return (0);
}
SYSCTL_PROC(_net_route_test, OID_AUTO, run_inet_scan,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, run_test_inet_scan, "I", "Execute fib4_lookup scan tests");


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
