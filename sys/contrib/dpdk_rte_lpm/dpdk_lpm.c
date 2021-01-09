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
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/ip.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/fib_algo.h>

#include "rte_shim.h"
#include "rte_lpm.h"

#define	LPM_MIN_TBL8	8		/* 2 pages of memory */
#define	LPM_MAX_TBL8	65536 * 16	/* 256M */

MALLOC_DECLARE(M_RTABLE);

struct dpdk_lpm_data {
	struct rte_lpm *lpm;
	uint64_t routes_added;
	uint64_t routes_failed;
	uint32_t number_tbl8s;
	uint32_t fibnum;
	uint8_t hit_tables;
	uint8_t	hit_records;
	struct fib_data *fd;
};

/*
 * Main datapath routing
 */
static struct nhop_object *
lookup_ptr(void *algo_data, const struct flm_lookup_key key, uint32_t scopeid)
{
	struct rte_lpm *lpm;
	const struct rte_lpm_external *rte_ext;
	uint32_t nhidx = 0;
	int ret;

	lpm = (struct rte_lpm *)algo_data;
	rte_ext = (const struct rte_lpm_external *)lpm;

	ret = rte_lpm_lookup(lpm, ntohl(key.addr4.s_addr), &nhidx);
	if (ret == 0) {
		/* Success! */
		return (rte_ext->nh_idx[nhidx]);
	} else {
		/* Not found. Check default route */
		return (rte_ext->nh_idx[rte_ext->default_idx]);
	}

	return (NULL);
}

static uint8_t
rte_get_pref(const struct rib_rtable_info *rinfo)
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
handle_default_change(struct dpdk_lpm_data *dd, struct rib_cmd_info *rc)
{
	struct rte_lpm_external *rte_ext;
	rte_ext = (struct rte_lpm_external *)dd->lpm;

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

static void
get_parent_rule(struct dpdk_lpm_data *dd, struct in_addr addr, uint8_t *plen, uint32_t *nhop_idx)
{
	struct route_nhop_data rnd;
	struct rtentry *rt;

	rt = fib4_lookup_rt(dd->fibnum, addr, 0, NHR_UNLOCKED, &rnd);
	if (rt != NULL) {
		struct in_addr addr4;
		uint32_t scopeid;
		int inet_plen;
		rt_get_inet_prefix_plen(rt, &addr4, &inet_plen, &scopeid);
		if (inet_plen > 0) {
			*plen = inet_plen;
			*nhop_idx = fib_get_nhop_idx(dd->fd, rnd.rnd_nhop);
			return;
		}
	}

	*nhop_idx = 0;
	*plen = 0;
}

static enum flm_op_result
handle_gu_change(struct dpdk_lpm_data *dd, const struct rib_cmd_info *rc,
    const struct in_addr addr, int plen)
{
	uint32_t nhidx = 0;
	int ret;
	char abuf[INET_ADDRSTRLEN];
	uint32_t ip;

	ip = ntohl(addr.s_addr);
	inet_ntop(AF_INET, &addr, abuf, sizeof(abuf));

	/* So we get sin, plen and nhidx */
	if (rc->rc_cmd != RTM_DELETE) {
		/*
		 * Addition or change. Save nhop in the internal table
		 * and get index.
		 */
		nhidx = fib_get_nhop_idx(dd->fd, rc->rc_nh_new);
		if (nhidx == 0) {
			FIB_PRINTF(LOG_INFO, dd->fd, "nhop limit reached, need rebuild");
			return (FLM_REBUILD);
		}

		ret = rte_lpm_add(dd->lpm, ip, plen, nhidx);
		FIB_PRINTF(LOG_DEBUG, dd->fd, "DPDK GU: %s %s/%d nhop %u = %d",
		    (rc->rc_cmd == RTM_ADD) ? "ADD" : "UPDATE",
		    abuf, plen, nhidx, ret);
	} else {
		/*
		 * Need to lookup parent. Assume deletion happened already
		 */
		uint8_t parent_plen;
		uint32_t parent_nhop_idx;
		get_parent_rule(dd, addr, &parent_plen, &parent_nhop_idx);

		ret = rte_lpm_delete(dd->lpm, ip, plen, parent_plen, parent_nhop_idx);
		FIB_PRINTF(LOG_DEBUG, dd->fd, "DPDK: %s %s/%d nhop %u = %d",
		    "DEL", abuf, plen, nhidx, ret);
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
handle_rtable_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{
	struct dpdk_lpm_data *dd;
	enum flm_op_result ret;
	struct in_addr addr4;
	uint32_t scopeid;
	int plen;

	dd = (struct dpdk_lpm_data *)_data;
	rt_get_inet_prefix_plen(rc->rc_rt, &addr4, &plen, &scopeid);

	if (plen != 0)
		ret = handle_gu_change(dd, rc, addr4, plen);
	else
		ret = handle_default_change(dd, rc);

	if (ret != 0)
		FIB_PRINTF(LOG_INFO, dd->fd, "error handling route");
	return (ret);
}

static void
destroy_table(void *_data)
{
	struct dpdk_lpm_data *dd = (struct dpdk_lpm_data *)_data;

	if (dd->lpm != NULL)
		rte_lpm_free(dd->lpm);
	free(dd, M_RTABLE);
}

static enum flm_op_result
add_route_cb(struct rtentry *rt, void *_data)
{
	struct dpdk_lpm_data *dd = (struct dpdk_lpm_data *)_data;
	struct nhop_object *nh;
	int plen, ret;
	struct in_addr addr4;
	uint32_t scopeid;

	nh = rt_get_raw_nhop(rt);
	rt_get_inet_prefix_plen(rt, &addr4, &plen, &scopeid);

	char abuf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr4, abuf, sizeof(abuf));

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
	ret = rte_lpm_add(dd->lpm, ntohl(addr4.s_addr), plen, nhidx);
	FIB_PRINTF(LOG_DEBUG, dd->fd, "ADD %p %s/%d nh %u = %d",
	    dd->lpm, abuf, plen, nhidx, ret);

	if (ret != 0) {
		FIB_PRINTF(LOG_INFO, dd->fd, "rte_lpm_add() returned %d", ret);
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
	struct dpdk_lpm_data *dd;

	dd = (struct dpdk_lpm_data *)_data;

	FIB_PRINTF(LOG_INFO, dd->fd, "scan completed. added: %zu failed: %zu",
	    dd->routes_added, dd->routes_failed);
	if (dd->hit_tables || dd->routes_failed > 0)
		return (FLM_REBUILD);

	FIB_PRINTF(LOG_INFO, dd->fd,
	    "DPDK lookup engine synced with IPv4 RIB id %u, %zu routes",
	    dd->fibnum, dd->routes_added);

	dp->f = lookup_ptr;
	dp->arg = dd->lpm;

	return (FLM_SUCCESS);
}

static void
estimate_scale(const struct dpdk_lpm_data *dd_src, struct dpdk_lpm_data *dd)
{

	/* XXX: update at 75% capacity */
	if (dd_src->hit_tables)
		dd->number_tbl8s = dd_src->number_tbl8s * 2;
	else
		dd->number_tbl8s = dd_src->number_tbl8s;

	/* TODO: look into the appropriate RIB to adjust */
}

static struct dpdk_lpm_data *
build_table(struct dpdk_lpm_data *dd_prev, struct fib_data *fd)
{
	struct dpdk_lpm_data *dd;
	struct rte_lpm *lpm;

	dd = malloc(sizeof(struct dpdk_lpm_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (dd == NULL) {
		FIB_PRINTF(LOG_INFO, fd, "Unable to allocate base datastructure");
		return (NULL);
	}
	dd->fibnum = dd_prev->fibnum;
	dd->fd = fd;

	estimate_scale(dd_prev, dd);

	struct rte_lpm_config cfg = {.number_tbl8s = dd->number_tbl8s};
	lpm = rte_lpm_create("test", 0, &cfg);
	if (lpm == NULL) {
		FIB_PRINTF(LOG_INFO, fd, "unable to create lpm");
		free(dd, M_RTABLE);
		return (NULL);
	}
	dd->lpm = lpm;
	struct rte_lpm_external *ext = (struct rte_lpm_external *)lpm;
	ext->nh_idx = fib_get_nhop_array(dd->fd);

	FIB_PRINTF(LOG_INFO, fd, "allocated %u tbl8s", dd->number_tbl8s);

	return (dd);
}

static enum flm_op_result
init_table(uint32_t fibnum, struct fib_data *fd, void *_old_data, void **data)
{
	struct dpdk_lpm_data *dd, dd_base;

	if (_old_data == NULL) {
		bzero(&dd_base, sizeof(struct dpdk_lpm_data));
		dd_base.fibnum = fibnum;
		/* TODO: get rib statistics */
		dd_base.number_tbl8s = LPM_MIN_TBL8;
		dd = &dd_base;
	} else {
		FIB_PRINTF(LOG_DEBUG, fd, "Starting with old data");
		dd = (struct dpdk_lpm_data *)_old_data;
	}

	/* Guaranteed to be in epoch */
	dd = build_table(dd, fd);
	if (dd == NULL) {
		FIB_PRINTF(LOG_NOTICE, fd, "table creation failed");
		return (FLM_REBUILD);
	}

	*data = dd;
	return (FLM_SUCCESS);
}

static struct fib_lookup_module dpdk_lpm4 = {
	.flm_name = "dpdk_lpm4",
	.flm_family = AF_INET,
	.flm_init_cb = init_table,
	.flm_destroy_cb = destroy_table,
	.flm_dump_rib_item_cb = add_route_cb,
	.flm_dump_end_cb = check_dump_success,
	.flm_change_rib_item_cb = handle_rtable_change_cb,
	.flm_get_pref = rte_get_pref,
};

static int
lpm4_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		fib_module_register(&dpdk_lpm4);
		break;
	case MOD_UNLOAD:
		error = fib_module_unregister(&dpdk_lpm4);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t lpm4mod = {
        "dpdk_lpm4",
        lpm4_modevent,
        0
};

DECLARE_MODULE(lpm4mod, lpm4mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(lpm4mod, 1);
