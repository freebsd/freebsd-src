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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_fib.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/fib_algo.h>
#define	RTDEBUG

#include "rte_lpm6.h"

#define	LPM6_MIN_TBL8	8		/* 2 pages of memory */
#define	LPM6_MAX_TBL8	65536 * 16	/* 256M */

struct fib_algo_calldata {
	void *lookup;
	void *arg;
};

struct dpdk_lpm6_data {
	struct rte_lpm6 *lpm6;
	uint64_t routes_added;
	uint64_t routes_failed;
	uint32_t number_tbl8s;
	uint32_t fibnum;
	uint8_t hit_tables;
	struct fib_data *fd;
};

static struct nhop_object *
lookup_ptr_ll(const struct rte_lpm6 *lpm6, const struct in6_addr *dst6,
    uint32_t scopeid)
{
	const struct rte_lpm6_external *rte_ext;

	rte_ext = (const struct rte_lpm6_external *)lpm6;

	return (fib6_radix_lookup_nh(rte_ext->fibnum, dst6, scopeid));
}

/*
 * Main datapath routing
 */
static struct nhop_object *
lookup_ptr(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	const struct rte_lpm6 *lpm6;
	const struct rte_lpm6_external *rte_ext;
	const struct in6_addr *addr6;
	uint32_t nhidx = 0;
	int ret;

	lpm6 = (const struct rte_lpm6 *)algo_data;
	addr6 = key.addr6;
	rte_ext = (const struct rte_lpm6_external *)lpm6;

	if (!IN6_IS_SCOPE_LINKLOCAL(addr6)) {
		ret = rte_lpm6_lookup(lpm6, (const uint8_t *)addr6, &nhidx);
		if (ret == 0) {
			/* Success! */
			return (rte_ext->nh_idx[nhidx]);
		} else {
			/* Not found. Check default route */
			if (rte_ext->default_idx > 0)
				return (rte_ext->nh_idx[rte_ext->default_idx]);
			else
				return (NULL);
		}
	} else {
		/* LL */
		return (lookup_ptr_ll(lpm6, addr6, scopeid));
	}
}

static uint8_t
rte6_get_pref(const struct rib_rtable_info *rinfo)
{

	if (rinfo->num_prefixes < 10)
		return (1);
	else if (rinfo->num_prefixes < 1000)
		return (rinfo->num_prefixes / 10);
	else if (rinfo->num_prefixes < 500000)
		return (100 + rinfo->num_prefixes / 3334);
	else
		return (250);
}

static enum flm_op_result
handle_default_change(struct dpdk_lpm6_data *dd, struct rib_cmd_info *rc)
{
	struct rte_lpm6_external *rte_ext;
	rte_ext = (struct rte_lpm6_external *)dd->lpm6;

	if (rc->rc_cmd != RTM_DELETE) {
		/* Reference new */
		uint32_t nhidx = fib_get_nhop_idx(dd->fd, rc->rc_nh_new);

		if (nhidx == 0)
			return (FLM_REBUILD);
		rte_ext->default_idx = nhidx;
	} else {
		/* No default route */
		rte_ext->default_idx = 0;
	}

	return (FLM_SUCCESS);
}

static enum flm_op_result
handle_ll_change(struct dpdk_lpm6_data *dd, struct rib_cmd_info *rc,
    const struct in6_addr addr6, int plen, uint32_t scopeid)
{

	return (FLM_SUCCESS);
}

static struct rte_lpm6_rule *
pack_parent_rule(struct dpdk_lpm6_data *dd, const struct in6_addr *addr6,
    char *buffer)
{
	struct rte_lpm6_rule *lsp_rule = NULL;
	struct route_nhop_data rnd;
	struct rtentry *rt;
	int plen;

	rt = fib6_lookup_rt(dd->fibnum, addr6, 0, NHR_UNLOCKED, &rnd);
	/* plen = 0 means default route and it's out of scope */
	if (rt != NULL) {
		uint32_t scopeid;
		struct in6_addr new_addr6;
		rt_get_inet6_prefix_plen(rt, &new_addr6, &plen, &scopeid);
		if (plen > 0) {
			uint32_t nhidx = fib_get_nhop_idx(dd->fd, rnd.rnd_nhop);
			if (nhidx == 0) {
				/*
				 * shouldn't happen as we already have parent route.
				 * It will trigger rebuild automatically.
				 */
				return (NULL);
			}
			lsp_rule = fill_rule6(buffer, (uint8_t *)&new_addr6, plen, nhidx);
		}
	}

	return (lsp_rule);
}

static enum flm_op_result
handle_gu_change(struct dpdk_lpm6_data *dd, const struct rib_cmd_info *rc,
    const struct in6_addr *addr6, int plen)
{
	int ret;
	char abuf[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, addr6, abuf, sizeof(abuf));

	/* So we get sin6, plen and nhidx */
	if (rc->rc_cmd != RTM_DELETE) {
		/*
		 * Addition or change. Save nhop in the internal table
		 * and get index.
		 */
		uint32_t nhidx = fib_get_nhop_idx(dd->fd, rc->rc_nh_new);
		if (nhidx == 0) {
			FIB_PRINTF(LOG_INFO, dd->fd, "nhop limit reached, need rebuild");
			return (FLM_REBUILD);
		}

		ret = rte_lpm6_add(dd->lpm6, (const uint8_t *)addr6,
				   plen, nhidx, (rc->rc_cmd == RTM_ADD) ? 1 : 0);
		FIB_PRINTF(LOG_DEBUG, dd->fd, "DPDK GU: %s %s/%d nhop %u = %d",
		    (rc->rc_cmd == RTM_ADD) ? "ADD" : "UPDATE",
		    abuf, plen, nhidx, ret);
	} else {
		/*
		 * Need to lookup parent. Assume deletion happened already
		 */
		char buffer[RTE_LPM6_RULE_SIZE];
		struct rte_lpm6_rule *lsp_rule = NULL;
		lsp_rule = pack_parent_rule(dd, addr6, buffer);

		ret = rte_lpm6_delete(dd->lpm6, (const uint8_t *)addr6, plen, lsp_rule);
		FIB_PRINTF(LOG_DEBUG, dd->fd, "DPDK GU: %s %s/%d nhop ? = %d",
		    "DEL", abuf, plen, ret);
	}

	if (ret != 0) {
		FIB_PRINTF(LOG_INFO, dd->fd, "error: %d", ret);
		if (ret == -ENOSPC)
			return (FLM_REBUILD);
		return (FLM_ERROR);
	}
	return (FLM_SUCCESS);
}

static enum flm_op_result
handle_any_change(struct dpdk_lpm6_data *dd, struct rib_cmd_info *rc)
{
	enum flm_op_result ret;
	struct in6_addr addr6;
	uint32_t scopeid;
	int plen;

	rt_get_inet6_prefix_plen(rc->rc_rt, &addr6, &plen, &scopeid);

	if (IN6_IS_SCOPE_LINKLOCAL(&addr6))
		ret = handle_ll_change(dd, rc, addr6, plen, scopeid);
	else if (plen == 0)
		ret = handle_default_change(dd, rc);
	else
		ret = handle_gu_change(dd, rc, &addr6, plen);

	if (ret != 0)
		FIB_PRINTF(LOG_INFO, dd->fd, "error handling route");
	return (ret);
}

static enum flm_op_result
handle_rtable_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{
	struct dpdk_lpm6_data *dd;

	dd = (struct dpdk_lpm6_data *)_data;

	return (handle_any_change(dd, rc));
}

static void
destroy_dd(struct dpdk_lpm6_data *dd)
{

	FIB_PRINTF(LOG_INFO, dd->fd, "destroy dd %p", dd);
	if (dd->lpm6 != NULL)
		rte_lpm6_free(dd->lpm6);
	free(dd, M_TEMP);
}

static void
destroy_table(void *_data)
{

	destroy_dd((struct dpdk_lpm6_data *)_data);
}

static enum flm_op_result
add_route_cb(struct rtentry *rt, void *_data)
{
	struct dpdk_lpm6_data *dd = (struct dpdk_lpm6_data *)_data;
	struct in6_addr addr6;
	struct nhop_object *nh;
	uint32_t scopeid;
	int plen;
	int ret;

	rt_get_inet6_prefix_plen(rt, &addr6, &plen, &scopeid);
	nh = rt_get_raw_nhop(rt);

	if (IN6_IS_SCOPE_LINKLOCAL(&addr6)) {

		/*
		 * We don't operate on LL directly, however
		 * reference them to maintain guarantee on
		 * ability to refcount nhops in epoch.
		 */
		fib_get_nhop_idx(dd->fd, nh);
		return (FLM_SUCCESS);
	}

	char abuf[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &addr6, abuf, sizeof(abuf));
	FIB_PRINTF(LOG_DEBUG, dd->fd, "Operating on %s/%d", abuf, plen);

	if (plen == 0) {
		struct rib_cmd_info rc = {
			.rc_cmd = RTM_ADD,
			.rc_nh_new = nh,
		};

		FIB_PRINTF(LOG_DEBUG, dd->fd, "Adding default route");
		return (handle_default_change(dd, &rc));
	}

	uint32_t nhidx = fib_get_nhop_idx(dd->fd, nh);
	if (nhidx == 0) {
		FIB_PRINTF(LOG_INFO, dd->fd, "unable to get nhop index");
		return (FLM_REBUILD);
	}
	ret = rte_lpm6_add(dd->lpm6, (const uint8_t *)&addr6, plen, nhidx, 1);
	FIB_PRINTF(LOG_DEBUG, dd->fd, "ADD %p %s/%d nh %u = %d",
	    dd->lpm6, abuf, plen, nhidx, ret);

	if (ret != 0) {
		FIB_PRINTF(LOG_INFO, dd->fd, "rte_lpm6_add() returned %d", ret);
		if (ret == -ENOSPC) {
			dd->hit_tables = 1;
			return (FLM_REBUILD);
		}
		dd->routes_failed++;
		return (FLM_ERROR);
	} else
		dd->routes_added++;

	return (FLM_SUCCESS);
}

static enum flm_op_result
check_dump_success(void *_data, struct fib_dp *dp)
{
	struct dpdk_lpm6_data *dd;

	dd = (struct dpdk_lpm6_data *)_data;

	FIB_PRINTF(LOG_INFO, dd->fd, "scan completed. added: %zu failed: %zu",
	    dd->routes_added, dd->routes_failed);
	if (dd->hit_tables || dd->routes_failed > 0)
		return (FLM_REBUILD);

	FIB_PRINTF(LOG_INFO, dd->fd,
	    "DPDK lookup engine synced with IPv6 RIB id %u, %zu routes",
	    dd->fibnum, dd->routes_added);

	dp->f = lookup_ptr;
	dp->arg = dd->lpm6;

	return (FLM_SUCCESS);
}

static void
estimate_scale(const struct dpdk_lpm6_data *dd_src, struct dpdk_lpm6_data *dd)
{

	/* XXX: update at 75% capacity */
	if (dd_src->hit_tables)
		dd->number_tbl8s = dd_src->number_tbl8s * 2;
	else
		dd->number_tbl8s = dd_src->number_tbl8s;

	/* TODO: look into the appropriate RIB to adjust */
}

static struct dpdk_lpm6_data *
build_table(struct dpdk_lpm6_data *dd_prev, struct fib_data *fd)
{
	struct dpdk_lpm6_data *dd;
	struct rte_lpm6 *lpm6;

	dd = malloc(sizeof(struct dpdk_lpm6_data), M_TEMP, M_NOWAIT | M_ZERO);
	if (dd == NULL) {
		FIB_PRINTF(LOG_INFO, fd, "Unable to allocate base datastructure");
		return (NULL);
	}
	dd->fibnum = dd_prev->fibnum;
	dd->fd = fd;

	estimate_scale(dd_prev, dd);

	struct rte_lpm6_config cfg = {.number_tbl8s = dd->number_tbl8s};
	lpm6 = rte_lpm6_create("test", 0, &cfg);
	if (lpm6 == NULL) {
		FIB_PRINTF(LOG_INFO, fd, "unable to create lpm6");
		free(dd, M_TEMP);
		return (NULL);
	}
	dd->lpm6 = lpm6;
	struct rte_lpm6_external *ext = (struct rte_lpm6_external *)lpm6;
	ext->nh_idx = fib_get_nhop_array(dd->fd);

	FIB_PRINTF(LOG_INFO, fd, "allocated %u tbl8s", dd->number_tbl8s);

	return (dd);
}

static enum flm_op_result
init_table(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **data)
{
	struct dpdk_lpm6_data *dd, dd_base;

	if (_old_data == NULL) {
		bzero(&dd_base, sizeof(struct dpdk_lpm6_data));
		dd_base.fibnum = fibnum;
		/* TODO: get rib statistics */
		dd_base.number_tbl8s = LPM6_MIN_TBL8;
		dd = &dd_base;
	} else {
		FIB_PRINTF(LOG_INFO, fd, "Starting with old data");
		dd = (struct dpdk_lpm6_data *)_old_data;
	}

	/* Guaranteed to be in epoch */
	dd = build_table(dd, fd);
	if (dd == NULL) {
		FIB_PRINTF(LOG_INFO, fd, "table creation failed");
		return (FLM_REBUILD);
	}

	*data = dd;
	return (FLM_SUCCESS);
}

static struct fib_lookup_module dpdk_lpm6 = {
	.flm_name = "dpdk_lpm6",
	.flm_family = AF_INET6,
	.flm_init_cb = init_table,
	.flm_destroy_cb = destroy_table,
	.flm_dump_rib_item_cb = add_route_cb,
	.flm_dump_end_cb = check_dump_success,
	.flm_change_rib_item_cb = handle_rtable_change_cb,
	.flm_get_pref = rte6_get_pref,
};

static int
lpm6_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		fib_module_register(&dpdk_lpm6);
		break;
	case MOD_UNLOAD:
		error = fib_module_unregister(&dpdk_lpm6);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t lpm6mod = {
        "dpdk_lpm6",
        lpm6_modevent,
        0
};

DECLARE_MODULE(lpm6mod, lpm6mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(lpm6mod, 1);
