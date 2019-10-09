/*-
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019
 *	Netflix Inc.
 *      All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * Author: Randall Stewart <rrs@netflix.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"
#include "opt_ratelimit.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/eventhandler.h>
#include <sys/mutex.h>
#include <sys/ck.h>
#define TCPSTATES		/* for logging */
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcp_ratelimit.h>
#ifndef USECS_IN_SECOND
#define USECS_IN_SECOND 1000000
#endif
/*
 * For the purposes of each send, what is the size
 * of an ethernet frame.
 */
#ifndef ETHERNET_SEGMENT_SIZE
#define ETHERNET_SEGMENT_SIZE 1500
#endif
MALLOC_DEFINE(M_TCPPACE, "tcp_hwpace", "TCP Hardware pacing memory");
#ifdef RATELIMIT

#define COMMON_RATE 180500
uint64_t desired_rates[] = {
	62500,			/* 500Kbps */
	180500,			/* 1.44Mpbs */
	375000,			/* 3Mbps */
	500000,			/* 4Mbps */
	625000,			/* 5Mbps */
	750000,			/* 6Mbps */
	1000000,		/* 8Mbps */
	1250000,		/* 10Mbps */
	2500000,		/* 20Mbps */
	3750000,		/* 30Mbps */
	5000000,		/* 40Meg */
	6250000,		/* 50Mbps */
	12500000,		/* 100Mbps */
	25000000,		/* 200Mbps */
	50000000,		/* 400Mbps */
	100000000,		/* 800Mbps */
	12500,			/* 100kbps */
	25000,			/* 200kbps */
	875000,			/* 7Mbps */
	1125000,		/* 9Mbps */
	1875000,		/* 15Mbps */
	3125000,		/* 25Mbps */
	8125000,		/* 65Mbps */
	10000000,		/* 80Mbps */
	18750000,		/* 150Mbps */
	20000000,		/* 250Mbps */
	37500000,		/* 350Mbps */
	62500000,		/* 500Mbps */
	78125000,		/* 625Mbps */
	125000000,		/* 1Gbps */
};
#define MAX_HDWR_RATES (sizeof(desired_rates)/sizeof(uint64_t))
#define RS_ORDERED_COUNT 16	/*
				 * Number that are in order
				 * at the beginning of the table,
				 * over this a sort is required.
				 */
#define RS_NEXT_ORDER_GROUP 16	/*
				 * The point in our table where
				 * we come fill in a second ordered
				 * group (index wise means -1).
				 */
#define ALL_HARDWARE_RATES 1004 /*
				 * 1Meg - 1Gig in 1 Meg steps
				 * plus 100, 200k  and 500k and
				 * 10Gig
				 */

#define RS_ONE_MEGABIT_PERSEC 1000000
#define RS_ONE_GIGABIT_PERSEC 1000000000
#define RS_TEN_GIGABIT_PERSEC 10000000000

static struct head_tcp_rate_set int_rs;
static struct mtx rs_mtx;
uint32_t rs_number_alive;
uint32_t rs_number_dead;

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, rl, CTLFLAG_RW, 0,
    "TCP Ratelimit stats");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, alive, CTLFLAG_RW,
    &rs_number_alive, 0,
    "Number of interfaces initialized for ratelimiting");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, dead, CTLFLAG_RW,
    &rs_number_dead, 0,
    "Number of interfaces departing from ratelimiting");

static void
rl_add_syctl_entries(struct sysctl_oid *rl_sysctl_root, struct tcp_rate_set *rs)
{
	/*
	 * Add sysctl entries for thus interface.
	 */
	if (rs->rs_flags & RS_INTF_NO_SUP) {
		SYSCTL_ADD_S32(&rs->sysctl_ctx,
		   SYSCTL_CHILDREN(rl_sysctl_root),
		   OID_AUTO, "disable", CTLFLAG_RD,
		   &rs->rs_disable, 0,
		   "Disable this interface from new hdwr limiting?");
	} else {
		SYSCTL_ADD_S32(&rs->sysctl_ctx,
		   SYSCTL_CHILDREN(rl_sysctl_root),
		   OID_AUTO, "disable", CTLFLAG_RW,
		   &rs->rs_disable, 0,
		   "Disable this interface from new hdwr limiting?");
	}
	SYSCTL_ADD_S32(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "minseg", CTLFLAG_RW,
	    &rs->rs_min_seg, 0,
	    "What is the minimum we need to send on this interface?");
	SYSCTL_ADD_U64(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "flow_limit", CTLFLAG_RW,
	    &rs->rs_flow_limit, 0,
	    "What is the limit for number of flows (0=unlimited)?");
	SYSCTL_ADD_S32(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "highest", CTLFLAG_RD,
	    &rs->rs_highest_valid, 0,
	    "Highest valid rate");
	SYSCTL_ADD_S32(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "lowest", CTLFLAG_RD,
	    &rs->rs_lowest_valid, 0,
	    "Lowest valid rate");
	SYSCTL_ADD_S32(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "flags", CTLFLAG_RD,
	    &rs->rs_flags, 0,
	    "What lags are on the entry?");
	SYSCTL_ADD_S32(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "numrates", CTLFLAG_RD,
	    &rs->rs_rate_cnt, 0,
	    "How many rates re there?");
	SYSCTL_ADD_U64(&rs->sysctl_ctx,
	    SYSCTL_CHILDREN(rl_sysctl_root),
	    OID_AUTO, "flows_using", CTLFLAG_RD,
	    &rs->rs_flows_using, 0,
	    "How many flows are using this interface now?");
#ifdef DETAILED_RATELIMIT_SYSCTL
	if (rs->rs_rlt && rs->rs_rate_cnt > 0) {
		/*  Lets display the rates */
		int i;
		struct sysctl_oid *rl_rates;
		struct sysctl_oid *rl_rate_num;
		char rate_num[16];
		rl_rates = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
					    SYSCTL_CHILDREN(rl_sysctl_root),
					    OID_AUTO,
					    "rate",
					    CTLFLAG_RW, 0,
					    "Ratelist");
		for( i = 0; i < rs->rs_rate_cnt; i++) {
			sprintf(rate_num, "%d", i);
			rl_rate_num = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
					    SYSCTL_CHILDREN(rl_rates),
					    OID_AUTO,
					    rate_num,
					    CTLFLAG_RW, 0,
					    "Individual Rate");
			SYSCTL_ADD_U32(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "flags", CTLFLAG_RD,
				       &rs->rs_rlt[i].flags, 0,
				       "Flags on this rate");
			SYSCTL_ADD_U32(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "pacetime", CTLFLAG_RD,
				       &rs->rs_rlt[i].time_between, 0,
				       "Time hardware inserts between 1500 byte sends");
			SYSCTL_ADD_U64(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "rate", CTLFLAG_RD,
				       &rs->rs_rlt[i].rate, 0,
				       "Rate in bytes per second");
		}
	}
#endif
}

static void
rs_destroy(epoch_context_t ctx)
{
	struct tcp_rate_set *rs;
	bool do_free_rs;

	rs = __containerof(ctx, struct tcp_rate_set, rs_epoch_ctx);

	mtx_lock(&rs_mtx);
	rs->rs_flags &= ~RS_FUNERAL_SCHD;
	/*
	 * In theory its possible (but unlikely)
	 * that while the delete was occuring
	 * and we were applying the DEAD flag
	 * someone slipped in and found the
	 * interface in a lookup. While we
	 * decided rs_flows_using were 0 and
	 * scheduling the epoch_call, the other
	 * thread incremented rs_flow_using. This
	 * is because users have a pointer and
	 * we only use the rs_flows_using in an
	 * atomic fashion, i.e. the other entities
	 * are not protected. To assure this did
	 * not occur, we check rs_flows_using here
	 * before deleting.
	 */
	do_free_rs = (rs->rs_flows_using == 0);
	rs_number_dead--;
	mtx_unlock(&rs_mtx);

	if (do_free_rs) {
		sysctl_ctx_free(&rs->sysctl_ctx);
		free(rs->rs_rlt, M_TCPPACE);
		free(rs, M_TCPPACE);
	}
}

static void
rs_defer_destroy(struct tcp_rate_set *rs)
{

	mtx_assert(&rs_mtx, MA_OWNED);

	/* Check if already pending. */
	if (rs->rs_flags & RS_FUNERAL_SCHD)
		return;

	rs_number_dead++;

	/* Set flag to only defer once. */
	rs->rs_flags |= RS_FUNERAL_SCHD;
	epoch_call(net_epoch, &rs->rs_epoch_ctx, rs_destroy);
}

#ifdef INET
extern counter_u64_t rate_limit_set_ok;
extern counter_u64_t rate_limit_active;
extern counter_u64_t rate_limit_alloc_fail;
#endif

static int
rl_attach_txrtlmt(struct ifnet *ifp,
    uint32_t flowtype,
    int flowid,
    uint64_t cfg_rate,
    struct m_snd_tag **tag)
{
	int error;
	union if_snd_tag_alloc_params params = {
		.rate_limit.hdr.type = IF_SND_TAG_TYPE_RATE_LIMIT,
		.rate_limit.hdr.flowid = flowid,
		.rate_limit.hdr.flowtype = flowtype,
		.rate_limit.max_rate = cfg_rate,
		.rate_limit.flags = M_NOWAIT,
	};

	if (ifp->if_snd_tag_alloc == NULL) {
		error = EOPNOTSUPP;
	} else {
		error = ifp->if_snd_tag_alloc(ifp, &params, tag);
#ifdef INET
		if (error == 0) {
			if_ref((*tag)->ifp);
			counter_u64_add(rate_limit_set_ok, 1);
			counter_u64_add(rate_limit_active, 1);
		} else
			counter_u64_add(rate_limit_alloc_fail, 1);
#endif
	}
	return (error);
}

static void
populate_canned_table(struct tcp_rate_set *rs, const uint64_t *rate_table_act)
{
	/*
	 * The internal table is "special", it
	 * is two seperate ordered tables that
	 * must be merged. We get here when the
	 * adapter specifies a number of rates that
	 * covers both ranges in the table in some
	 * form.
	 */
	int i, at_low, at_high;
	uint8_t low_disabled = 0, high_disabled = 0;

	for(i = 0, at_low = 0, at_high = RS_NEXT_ORDER_GROUP; i < rs->rs_rate_cnt; i++) {
		rs->rs_rlt[i].flags = 0;
		rs->rs_rlt[i].time_between = 0;
		if ((low_disabled == 0) &&
		    (high_disabled ||
		     (rate_table_act[at_low] < rate_table_act[at_high]))) {
			rs->rs_rlt[i].rate = rate_table_act[at_low];
			at_low++;
			if (at_low == RS_NEXT_ORDER_GROUP)
				low_disabled = 1;
		} else if (high_disabled == 0) {
			rs->rs_rlt[i].rate = rate_table_act[at_high];
			at_high++;
			if (at_high == MAX_HDWR_RATES)
				high_disabled = 1;
		}
	}
}

static struct tcp_rate_set *
rt_setup_new_rs(struct ifnet *ifp, int *error)
{
	struct tcp_rate_set *rs;
	const uint64_t *rate_table_act;
	uint64_t lentim, res;
	size_t sz;
	uint32_t hash_type;
	int i;
	struct if_ratelimit_query_results rl;
	struct sysctl_oid *rl_sysctl_root;
	/*
	 * We expect to enter with the 
	 * mutex locked.
	 */

	if (ifp->if_ratelimit_query == NULL) {
		/*
		 * We can do nothing if we cannot
		 * get a query back from the driver.
		 */
		return (NULL);
	}
	rs = malloc(sizeof(struct tcp_rate_set), M_TCPPACE, M_NOWAIT | M_ZERO);
	if (rs == NULL) {
		if (error)
			*error = ENOMEM;
		return (NULL);
	}
	rl.flags = RT_NOSUPPORT;
	ifp->if_ratelimit_query(ifp, &rl);
	if (rl.flags & RT_IS_UNUSABLE) {
		/* 
		 * The interface does not really support 
		 * the rate-limiting.
		 */
		memset(rs, 0, sizeof(struct tcp_rate_set));
		rs->rs_ifp = ifp;
		rs->rs_if_dunit = ifp->if_dunit;
		rs->rs_flags = RS_INTF_NO_SUP;
		rs->rs_disable = 1;
		rs_number_alive++;
		sysctl_ctx_init(&rs->sysctl_ctx);
		rl_sysctl_root = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_net_inet_tcp_rl),
		    OID_AUTO,
		    rs->rs_ifp->if_xname,
		    CTLFLAG_RW, 0,
		    "");
		rl_add_syctl_entries(rl_sysctl_root, rs);
		mtx_lock(&rs_mtx);
		CK_LIST_INSERT_HEAD(&int_rs, rs, next);
		mtx_unlock(&rs_mtx);
		return (rs);
	} else if ((rl.flags & RT_IS_INDIRECT) == RT_IS_INDIRECT) {
		memset(rs, 0, sizeof(struct tcp_rate_set));
		rs->rs_ifp = ifp;
		rs->rs_if_dunit = ifp->if_dunit;
		rs->rs_flags = RS_IS_DEFF;
		rs_number_alive++;
		sysctl_ctx_init(&rs->sysctl_ctx);
		rl_sysctl_root = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_net_inet_tcp_rl),
		    OID_AUTO,
		    rs->rs_ifp->if_xname,
		    CTLFLAG_RW, 0,
		    "");
		rl_add_syctl_entries(rl_sysctl_root, rs);
		mtx_lock(&rs_mtx);
		CK_LIST_INSERT_HEAD(&int_rs, rs, next);
		mtx_unlock(&rs_mtx);
		return (rs);
	} else if ((rl.flags & RT_IS_FIXED_TABLE) == RT_IS_FIXED_TABLE) {
		/* Mellanox most likely */
		rs->rs_ifp = ifp;
		rs->rs_if_dunit = ifp->if_dunit;
		rs->rs_rate_cnt = rl.number_of_rates;
		rs->rs_min_seg = rl.min_segment_burst;
		rs->rs_highest_valid = 0;
		rs->rs_flow_limit = rl.max_flows;
		rs->rs_flags = RS_IS_INTF | RS_NO_PRE;
		rs->rs_disable = 0;
		rate_table_act = rl.rate_table;
	} else if ((rl.flags & RT_IS_SELECTABLE) == RT_IS_SELECTABLE) {
		/* Chelsio */
		rs->rs_ifp = ifp;
		rs->rs_if_dunit = ifp->if_dunit;
		rs->rs_rate_cnt = rl.number_of_rates;
		rs->rs_min_seg = rl.min_segment_burst;
		rs->rs_disable = 0;
		rs->rs_flow_limit = rl.max_flows;
		rate_table_act = desired_rates;
		if ((rs->rs_rate_cnt > MAX_HDWR_RATES) &&
		    (rs->rs_rate_cnt < ALL_HARDWARE_RATES)) {
			/*
			 * Our desired table is not big
			 * enough, do what we can.
			 */
			rs->rs_rate_cnt = MAX_HDWR_RATES;
		 }
		if (rs->rs_rate_cnt <= RS_ORDERED_COUNT)
			rs->rs_flags = RS_IS_INTF;
		else
			rs->rs_flags = RS_IS_INTF | RS_INT_TBL;
		if (rs->rs_rate_cnt >= ALL_HARDWARE_RATES)
			rs->rs_rate_cnt = ALL_HARDWARE_RATES;
	} else {
		printf("Interface:%s unit:%d not one known to have rate-limits\n",
		    ifp->if_dname,
		    ifp->if_dunit);
		free(rs, M_TCPPACE);
		return (NULL);
	}
	sz = sizeof(struct tcp_hwrate_limit_table) * rs->rs_rate_cnt;
	rs->rs_rlt = malloc(sz, M_TCPPACE, M_NOWAIT);
	if (rs->rs_rlt == NULL) {
		if (error)
			*error = ENOMEM;
bail:
		free(rs, M_TCPPACE);
		return (NULL);
	}
	if (rs->rs_rate_cnt >= ALL_HARDWARE_RATES) {
		/*
		 * The interface supports all
		 * the rates we could possibly want.
		 */
		uint64_t rat;

		rs->rs_rlt[0].rate = 12500;	/* 100k */
		rs->rs_rlt[1].rate = 25000;	/* 200k */
		rs->rs_rlt[2].rate = 62500;	/* 500k */
		/* Note 125000 == 1Megabit
		 * populate 1Meg - 1000meg.
		 */
		for(i = 3, rat = 125000; i< (ALL_HARDWARE_RATES-1); i++) {
			rs->rs_rlt[i].rate = rat;
			rat += 125000;
		}
		rs->rs_rlt[(ALL_HARDWARE_RATES-1)].rate = 1250000000;
	} else if (rs->rs_flags & RS_INT_TBL) {
		/* We populate this in a special way */
		populate_canned_table(rs, rate_table_act);
	} else {
		/*
		 * Just copy in the rates from
		 * the table, it is in order.
		 */
		for (i=0; i<rs->rs_rate_cnt; i++) {
			rs->rs_rlt[i].rate = rate_table_act[i];
			rs->rs_rlt[i].time_between = 0;
			rs->rs_rlt[i].flags = 0;
		}
	}
	for (i = (rs->rs_rate_cnt - 1); i >= 0; i--) {
		/*
		 * We go backwards through the list so that if we can't get
		 * a rate and fail to init one, we have at least a chance of
		 * getting the highest one.
		 */
		rs->rs_rlt[i].ptbl = rs;
		rs->rs_rlt[i].tag = NULL;
		/*
		 * Calculate the time between.
		 */
		lentim = ETHERNET_SEGMENT_SIZE * USECS_IN_SECOND;
		res = lentim / rs->rs_rlt[i].rate;
		if (res > 0)
			rs->rs_rlt[i].time_between = res;
		else
			rs->rs_rlt[i].time_between = 1;
		if (rs->rs_flags & RS_NO_PRE) {
			rs->rs_rlt[i].flags = HDWRPACE_INITED;
			rs->rs_lowest_valid = i;
		} else {
			int err;
#ifdef RSS
			hash_type = M_HASHTYPE_RSS_TCP_IPV4;
#else
			hash_type = M_HASHTYPE_OPAQUE_HASH;
#endif
			err = rl_attach_txrtlmt(ifp,
			    hash_type,
			    (i + 1),
			    rs->rs_rlt[i].rate,
			    &rs->rs_rlt[i].tag);
			if (err) {
				if (i == (rs->rs_rate_cnt - 1)) {
					/*
					 * Huh - first rate and we can't get
					 * it?
					 */
					free(rs->rs_rlt, M_TCPPACE);
					if (error)
						*error = err;
					goto bail;
				} else {
					if (error)
						*error = err;
				}
				break;
			} else {
				rs->rs_rlt[i].flags = HDWRPACE_INITED | HDWRPACE_TAGPRESENT;
				rs->rs_lowest_valid = i;
			}
		}
	}
	/* Did we get at least 1 rate? */
	if (rs->rs_rlt[(rs->rs_rate_cnt - 1)].flags & HDWRPACE_INITED)
		rs->rs_highest_valid = rs->rs_rate_cnt - 1;
	else {
		free(rs->rs_rlt, M_TCPPACE);
		goto bail;
	}
	rs_number_alive++;
	sysctl_ctx_init(&rs->sysctl_ctx);
	rl_sysctl_root = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_net_inet_tcp_rl),
	    OID_AUTO,
	    rs->rs_ifp->if_xname,
	    CTLFLAG_RW, 0,
	    "");
	rl_add_syctl_entries(rl_sysctl_root, rs);
	mtx_lock(&rs_mtx);
	CK_LIST_INSERT_HEAD(&int_rs, rs, next);
	mtx_unlock(&rs_mtx);
	return (rs);
}

static const struct tcp_hwrate_limit_table *
tcp_int_find_suitable_rate(const struct tcp_rate_set *rs,
    uint64_t bytes_per_sec, uint32_t flags)
{
	struct tcp_hwrate_limit_table *arte = NULL, *rte = NULL;
	uint64_t mbits_per_sec, ind_calc;
	int i;

	mbits_per_sec = (bytes_per_sec * 8);
	if (flags & RS_PACING_LT) {
		if ((mbits_per_sec < RS_ONE_MEGABIT_PERSEC) &&
		    (rs->rs_lowest_valid <= 2)){
			/*
			 * Smaller than 1Meg, only
			 * 3 entries can match it.
			 */
			for(i = rs->rs_lowest_valid; i < 3; i++) {
				if (bytes_per_sec <= rs->rs_rlt[i].rate) {
					rte = &rs->rs_rlt[i];
					break;
				} else if (rs->rs_rlt[i].flags & HDWRPACE_INITED) {
					arte = &rs->rs_rlt[i];
				}
			}
			goto done;
		} else if ((mbits_per_sec > RS_ONE_GIGABIT_PERSEC) &&
			   (rs->rs_rlt[(ALL_HARDWARE_RATES-1)].flags & HDWRPACE_INITED)){
			/*
			 * Larger than 1G (the majority of
			 * our table.
			 */
			if (mbits_per_sec < RS_TEN_GIGABIT_PERSEC)
				rte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			else
				arte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			goto done;
		}
		/*
		 * If we reach here its in our table (between 1Meg - 1000Meg),
		 * just take the rounded down mbits per second, and add
		 * 1Megabit to it, from this we can calculate
		 * the index in the table.
		 */
		ind_calc = mbits_per_sec/RS_ONE_MEGABIT_PERSEC;
		if ((ind_calc * RS_ONE_MEGABIT_PERSEC) != mbits_per_sec)
			ind_calc++;
		/* our table is offset by 3, we add 2 */
		ind_calc += 2;
		if (ind_calc > (ALL_HARDWARE_RATES-1)) {
			/* This should not happen */
			ind_calc = ALL_HARDWARE_RATES-1;
		}
		if ((ind_calc >= rs->rs_lowest_valid) &&
		    (ind_calc <= rs->rs_highest_valid))
		rte = &rs->rs_rlt[ind_calc];
	} else if (flags & RS_PACING_EXACT_MATCH) {
		if ((mbits_per_sec < RS_ONE_MEGABIT_PERSEC) &&
		    (rs->rs_lowest_valid <= 2)){
			for(i = rs->rs_lowest_valid; i < 3; i++) {
				if (bytes_per_sec == rs->rs_rlt[i].rate) {
					rte = &rs->rs_rlt[i];
					break;
				}
			}
		} else if ((mbits_per_sec > RS_ONE_GIGABIT_PERSEC) &&
			   (rs->rs_rlt[(ALL_HARDWARE_RATES-1)].flags & HDWRPACE_INITED)) {
			/* > 1Gbps only one rate */
			if (bytes_per_sec == rs->rs_rlt[(ALL_HARDWARE_RATES-1)].rate) {
				/* Its 10G wow */
				rte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			}
		} else {
			/* Ok it must be a exact meg (its between 1G and 1Meg) */
			ind_calc = mbits_per_sec/RS_ONE_MEGABIT_PERSEC;
			if ((ind_calc * RS_ONE_MEGABIT_PERSEC) == mbits_per_sec) {
				/* its an exact Mbps */
				ind_calc += 2;
				if (ind_calc > (ALL_HARDWARE_RATES-1)) {
					/* This should not happen */
					ind_calc = ALL_HARDWARE_RATES-1;
				}
				if (rs->rs_rlt[ind_calc].flags & HDWRPACE_INITED)
					rte = &rs->rs_rlt[ind_calc];
			}
		}
	} else {
		/* we want greater than the requested rate */
		if ((mbits_per_sec < RS_ONE_MEGABIT_PERSEC) &&
		    (rs->rs_lowest_valid <= 2)){
			arte = &rs->rs_rlt[3]; /* set alternate to 1Meg */
			for (i=2; i>=rs->rs_lowest_valid; i--) {
				if (bytes_per_sec < rs->rs_rlt[i].rate) {
					rte = &rs->rs_rlt[i];
					break;
				} else if ((flags & RS_PACING_GEQ) &&
					   (bytes_per_sec == rs->rs_rlt[i].rate)) {
					rte = &rs->rs_rlt[i];
					break;
				} else {
					arte = &rs->rs_rlt[i]; /* new alternate */
				}
			}
		} else if (mbits_per_sec > RS_ONE_GIGABIT_PERSEC) {
			if ((bytes_per_sec < rs->rs_rlt[(ALL_HARDWARE_RATES-1)].rate) &&
			    (rs->rs_rlt[(ALL_HARDWARE_RATES-1)].flags & HDWRPACE_INITED)){
				/* Our top rate is larger than the request */
				rte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			} else if ((flags & RS_PACING_GEQ) &&
				   (bytes_per_sec == rs->rs_rlt[(ALL_HARDWARE_RATES-1)].rate) &&
				   (rs->rs_rlt[(ALL_HARDWARE_RATES-1)].flags & HDWRPACE_INITED)) {
				/* It matches our top rate */
				rte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			} else if (rs->rs_rlt[(ALL_HARDWARE_RATES-1)].flags & HDWRPACE_INITED) {
				/* The top rate is an alternative */
				arte = &rs->rs_rlt[(ALL_HARDWARE_RATES-1)];
			}
		} else {
			/* Its in our range 1Meg - 1Gig */
			if (flags & RS_PACING_GEQ) {
				ind_calc = mbits_per_sec/RS_ONE_MEGABIT_PERSEC;
				if ((ind_calc * RS_ONE_MEGABIT_PERSEC) == mbits_per_sec) {
					if (ind_calc > (ALL_HARDWARE_RATES-1)) {
						/* This should not happen */
						ind_calc = (ALL_HARDWARE_RATES-1);
					}
					rte = &rs->rs_rlt[ind_calc];
				}
				goto done;
			}
			ind_calc = (mbits_per_sec + (RS_ONE_MEGABIT_PERSEC-1))/RS_ONE_MEGABIT_PERSEC;
			ind_calc += 2;
			if (ind_calc > (ALL_HARDWARE_RATES-1)) {
				/* This should not happen */
				ind_calc = ALL_HARDWARE_RATES-1;
			}
			if (rs->rs_rlt[ind_calc].flags & HDWRPACE_INITED)
				rte = &rs->rs_rlt[ind_calc];
		}
	}
done:
	if ((rte == NULL) &&
	    (arte != NULL) &&
	    (flags & RS_PACING_SUB_OK)) {
		/* We can use the substitute */
		rte = arte;
	}
	return (rte);
}

static const struct tcp_hwrate_limit_table *
tcp_find_suitable_rate(const struct tcp_rate_set *rs, uint64_t bytes_per_sec, uint32_t flags)
{
	/**
	 * Hunt the rate table with the restrictions in flags and find a
	 * suitable rate if possible.
	 * RS_PACING_EXACT_MATCH - look for an exact match to rate.
	 * RS_PACING_GT     - must be greater than.
	 * RS_PACING_GEQ    - must be greater than or equal.
	 * RS_PACING_LT     - must be less than.
	 * RS_PACING_SUB_OK - If we don't meet criteria a
	 *                    substitute is ok.
	 */
	int i, matched;
	struct tcp_hwrate_limit_table *rte = NULL;


	if ((rs->rs_flags & RS_INT_TBL) &&
	    (rs->rs_rate_cnt >= ALL_HARDWARE_RATES)) {
		/*
		 * Here we don't want to paw thru
		 * a big table, we have everything
		 * from 1Meg - 1000Meg in 1Meg increments.
		 * Use an alternate method to "lookup".
		 */
		return (tcp_int_find_suitable_rate(rs, bytes_per_sec, flags));
	}
	if ((flags & RS_PACING_LT) ||
	    (flags & RS_PACING_EXACT_MATCH)) {
		/*
		 * For exact and less than we go forward through the table.
		 * This way when we find one larger we stop (exact was a
		 * toss up).
		 */
		for (i = rs->rs_lowest_valid, matched = 0; i <= rs->rs_highest_valid; i++) {
			if ((flags & RS_PACING_EXACT_MATCH) &&
			    (bytes_per_sec == rs->rs_rlt[i].rate)) {
				rte = &rs->rs_rlt[i];
				matched = 1;
				break;
			} else if ((flags & RS_PACING_LT) &&
			    (bytes_per_sec <= rs->rs_rlt[i].rate)) {
				rte = &rs->rs_rlt[i];
				matched = 1;
				break;
			}
			if (bytes_per_sec > rs->rs_rlt[i].rate)
				break;
		}
		if ((matched == 0) &&
		    (flags & RS_PACING_LT) &&
		    (flags & RS_PACING_SUB_OK)) {
			/* Kick in a substitute (the lowest) */
			rte = &rs->rs_rlt[rs->rs_lowest_valid];
		}
	} else {
		/*
		 * Here we go backward through the table so that we can find
		 * the one greater in theory faster (but its probably a
		 * wash).
		 */
		for (i = rs->rs_highest_valid, matched = 0; i >= rs->rs_lowest_valid; i--) {
			if (rs->rs_rlt[i].rate > bytes_per_sec) {
				/* A possible candidate */
				rte = &rs->rs_rlt[i];
			}
			if ((flags & RS_PACING_GEQ) &&
			    (bytes_per_sec == rs->rs_rlt[i].rate)) {
				/* An exact match and we want equal */
				matched = 1;
				rte = &rs->rs_rlt[i];
				break;
			} else if (rte) {
				/*
				 * Found one that is larger than but don't
				 * stop, there may be a more closer match.
				 */
				matched = 1;
			}
			if (rs->rs_rlt[i].rate < bytes_per_sec) {
				/*
				 * We found a table entry that is smaller,
				 * stop there will be none greater or equal.
				 */
				break;
			}
		}
		if ((matched == 0) &&
		    (flags & RS_PACING_SUB_OK)) {
			/* Kick in a substitute (the highest) */
			rte = &rs->rs_rlt[rs->rs_highest_valid];
		}
	}
	return (rte);
}

static struct ifnet *
rt_find_real_interface(struct ifnet *ifp, struct inpcb *inp, int *error)
{
	struct ifnet *tifp;
	struct m_snd_tag *tag;
	union if_snd_tag_alloc_params params = {
		.rate_limit.hdr.type = IF_SND_TAG_TYPE_RATE_LIMIT,
		.rate_limit.hdr.flowid = 1,
		.rate_limit.max_rate = COMMON_RATE,
		.rate_limit.flags = M_NOWAIT,
	};
	int err;
#ifdef RSS
	params.rate_limit.hdr.flowtype = ((inp->inp_vflag & INP_IPV6) ?
	    M_HASHTYPE_RSS_TCP_IPV6 : M_HASHTYPE_RSS_TCP_IPV4);
#else
	params.rate_limit.hdr.flowtype = M_HASHTYPE_OPAQUE_HASH;
#endif
	tag = NULL;
	if (ifp->if_snd_tag_alloc) {
		if (error)
			*error = ENODEV;
		return (NULL);
	}
	err = ifp->if_snd_tag_alloc(ifp, &params, &tag);
	if (err) {
		/* Failed to setup a tag? */
		if (error)
			*error = err;
		return (NULL);
	}
	tifp = tag->ifp;
	tifp->if_snd_tag_free(tag);
	return (tifp);
}

static const struct tcp_hwrate_limit_table *
rt_setup_rate(struct inpcb *inp, struct ifnet *ifp, uint64_t bytes_per_sec,
    uint32_t flags, int *error)
{
	/* First lets find the interface if it exists */
	const struct tcp_hwrate_limit_table *rte;
	struct tcp_rate_set *rs;
	struct epoch_tracker et;
	int err;

	epoch_enter_preempt(net_epoch_preempt, &et);
use_real_interface:
	CK_LIST_FOREACH(rs, &int_rs, next) {
		/*
		 * Note we don't look with the lock since we either see a
		 * new entry or will get one when we try to add it.
		 */
		if (rs->rs_flags & RS_IS_DEAD) {
			/* The dead are not looked at */
			continue;
		}
		if ((rs->rs_ifp == ifp) &&
		    (rs->rs_if_dunit == ifp->if_dunit)) {
			/* Ok we found it */
			break;
		}
	}
	if ((rs == NULL) ||
	    (rs->rs_flags & RS_INTF_NO_SUP) ||
	    (rs->rs_flags & RS_IS_DEAD)) {
		/*
		 * This means we got a packet *before*
		 * the IF-UP was processed below, <or>
		 * while or after we already received an interface
		 * departed event. In either case we really don't
		 * want to do anything with pacing, in
		 * the departing case the packet is not
		 * going to go very far. The new case
		 * might be arguable, but its impossible
		 * to tell from the departing case.
		 */
		if (rs->rs_disable && error)
			*error = ENODEV;
		epoch_exit_preempt(net_epoch_preempt, &et);
		return (NULL);
	}

	if ((rs == NULL) || (rs->rs_disable != 0)) {
		if (rs->rs_disable && error)
			*error = ENOSPC;
		epoch_exit_preempt(net_epoch_preempt, &et);
		return (NULL);
	}
	if (rs->rs_flags & RS_IS_DEFF) {
		/* We need to find the real interface */
		struct ifnet *tifp;

		tifp = rt_find_real_interface(ifp, inp, error);
		if (tifp == NULL) {
			if (rs->rs_disable && error)
				*error = ENOTSUP;
			epoch_exit_preempt(net_epoch_preempt, &et);
			return (NULL);
		}
		goto use_real_interface;
	}
	if (rs->rs_flow_limit &&
	    ((rs->rs_flows_using + 1) > rs->rs_flow_limit)) {
		if (error)
			*error = ENOSPC;
		epoch_exit_preempt(net_epoch_preempt, &et);
		return (NULL);
	}
	rte = tcp_find_suitable_rate(rs, bytes_per_sec, flags);
	if (rte) {
		err = in_pcbattach_txrtlmt(inp, rs->rs_ifp,
		    inp->inp_flowtype,
		    inp->inp_flowid,
		    rte->rate,
		    &inp->inp_snd_tag);
		if (err) {
			/* Failed to attach */
			if (error)
				*error = err;
			rte = NULL;
		}
	}
	if (rte) {
		/*
		 * We use an atomic here for accounting so we don't have to
		 * use locks when freeing.
		 */
		atomic_add_64(&rs->rs_flows_using, 1);
	}
	epoch_exit_preempt(net_epoch_preempt, &et);
	return (rte);
}

static void
tcp_rl_ifnet_link(void *arg __unused, struct ifnet *ifp, int link_state)
{
	int error;
	struct tcp_rate_set *rs;

	if (((ifp->if_capabilities & IFCAP_TXRTLMT) == 0) ||
	    (link_state != LINK_STATE_UP)) {
		/*
		 * We only care on an interface going up that is rate-limit
		 * capable.
		 */
		return;
	}
	mtx_lock(&rs_mtx);
	CK_LIST_FOREACH(rs, &int_rs, next) {
		if ((rs->rs_ifp == ifp) &&
		    (rs->rs_if_dunit == ifp->if_dunit)) {
			/* We already have initialized this guy */
			mtx_unlock(&rs_mtx);
			return;
		}
	}
	mtx_unlock(&rs_mtx);
	rt_setup_new_rs(ifp, &error);
}

static void
tcp_rl_ifnet_departure(void *arg __unused, struct ifnet *ifp)
{
	struct tcp_rate_set *rs, *nrs;
	struct ifnet *tifp;
	int i;

	mtx_lock(&rs_mtx);
	CK_LIST_FOREACH_SAFE(rs, &int_rs, next, nrs) {
		if ((rs->rs_ifp == ifp) &&
		    (rs->rs_if_dunit == ifp->if_dunit)) {
			CK_LIST_REMOVE(rs, next);
			rs_number_alive--;
			rs->rs_flags |= RS_IS_DEAD;
			for (i = 0; i < rs->rs_rate_cnt; i++) {
				if (rs->rs_rlt[i].flags & HDWRPACE_TAGPRESENT) {
					tifp = rs->rs_rlt[i].tag->ifp;
					in_pcbdetach_tag(tifp, rs->rs_rlt[i].tag);
					rs->rs_rlt[i].tag = NULL;
				}
				rs->rs_rlt[i].flags = HDWRPACE_IFPDEPARTED;
			}
			if (rs->rs_flows_using == 0)
				rs_defer_destroy(rs);
			break;
		}
	}
	mtx_unlock(&rs_mtx);
}

static void
tcp_rl_shutdown(void *arg __unused, int howto __unused)
{
	struct tcp_rate_set *rs, *nrs;
	struct ifnet *tifp;
	int i;

	mtx_lock(&rs_mtx);
	CK_LIST_FOREACH_SAFE(rs, &int_rs, next, nrs) {
		CK_LIST_REMOVE(rs, next);
		rs_number_alive--;
		rs->rs_flags |= RS_IS_DEAD;
		for (i = 0; i < rs->rs_rate_cnt; i++) {
			if (rs->rs_rlt[i].flags & HDWRPACE_TAGPRESENT) {
				tifp = rs->rs_rlt[i].tag->ifp;
				in_pcbdetach_tag(tifp, rs->rs_rlt[i].tag);
				rs->rs_rlt[i].tag = NULL;
			}
			rs->rs_rlt[i].flags = HDWRPACE_IFPDEPARTED;
		}
		if (rs->rs_flows_using == 0)
			rs_defer_destroy(rs);
	}
	mtx_unlock(&rs_mtx);
}

const struct tcp_hwrate_limit_table *
tcp_set_pacing_rate(struct tcpcb *tp, struct ifnet *ifp,
    uint64_t bytes_per_sec, int flags, int *error)
{
	const struct tcp_hwrate_limit_table *rte;

	if (tp->t_inpcb->inp_snd_tag == NULL) {
		/*
		 * We are setting up a rate for the first time.
		 */
		if ((ifp->if_capabilities & IFCAP_TXRTLMT) == 0) {
			/* Not supported by the egress */
			if (error)
				*error = ENODEV;
			return (NULL);
		}
#ifdef KERN_TLS
		if (tp->t_inpcb->inp_socket->so_snd.sb_flags & SB_TLS_IFNET) {
			/*
			 * We currently can't do both TLS and hardware
			 * pacing
			 */
			if (error)
				*error = EINVAL;
			return (NULL);
		}
#endif
		rte = rt_setup_rate(tp->t_inpcb, ifp, bytes_per_sec, flags, error);
	} else {
		/*
		 * We are modifying a rate, wrong interface?
		 */
		if (error)
			*error = EINVAL;
		rte = NULL;
	}
	return (rte);
}

const struct tcp_hwrate_limit_table *
tcp_chg_pacing_rate(const struct tcp_hwrate_limit_table *crte,
    struct tcpcb *tp, struct ifnet *ifp,
    uint64_t bytes_per_sec, int flags, int *error)
{
	const struct tcp_hwrate_limit_table *nrte;
	const struct tcp_rate_set *rs;
	int is_indirect = 0;
	int err;


	if ((tp->t_inpcb->inp_snd_tag == NULL) ||
	    (crte == NULL)) {
		/* Wrong interface */
		if (error)
			*error = EINVAL;
		return (NULL);
	}
	rs = crte->ptbl;
	if ((rs->rs_flags & RS_IS_DEAD) ||
	    (crte->flags & HDWRPACE_IFPDEPARTED)) {
		/* Release the rate, and try anew */
re_rate:
		tcp_rel_pacing_rate(crte, tp);
		nrte = tcp_set_pacing_rate(tp, ifp,
		    bytes_per_sec, flags, error);
		return (nrte);
	}
	if ((rs->rs_flags & RT_IS_INDIRECT ) == RT_IS_INDIRECT)
		is_indirect = 1;
	else
		is_indirect = 0;
	if ((is_indirect == 0) &&
	    ((ifp != rs->rs_ifp) ||
	    (ifp->if_dunit != rs->rs_if_dunit))) {
		/*
		 * Something changed, the user is not pointing to the same
		 * ifp? Maybe a route updated on this guy?
		 */
		goto re_rate;
	} else if (is_indirect) {
		/*
		 * For indirect we have to dig in and find the real interface.
		 */
		struct ifnet *rifp;

		rifp = rt_find_real_interface(ifp, tp->t_inpcb, error);
		if (rifp == NULL) {
			/* Can't find it? */
			goto re_rate;
		}
		if ((rifp != rs->rs_ifp) ||
		    (ifp->if_dunit != rs->rs_if_dunit)) {
			goto re_rate;
		}
	}
	nrte = tcp_find_suitable_rate(rs, bytes_per_sec, flags);
	if (nrte == crte) {
		/* No change */
		if (error)
			*error = 0;
		return (crte);
	}
	if (nrte == NULL) {
		/* Release the old rate */
		tcp_rel_pacing_rate(crte, tp);
		return (NULL);
	}
	/* Change rates to our new entry */
	err = in_pcbmodify_txrtlmt(tp->t_inpcb, nrte->rate);
	if (err) {
		if (error)
			*error = err;
		return (NULL);
	}
	if (error)
		*error = 0;
	return (nrte);
}

void
tcp_rel_pacing_rate(const struct tcp_hwrate_limit_table *crte, struct tcpcb *tp)
{
	const struct tcp_rate_set *crs;
	struct tcp_rate_set *rs;
	uint64_t pre;

	crs = crte->ptbl;
	/*
	 * Now we must break the const
	 * in order to release our refcount.
	 */
	rs = __DECONST(struct tcp_rate_set *, crs);
	pre = atomic_fetchadd_64(&rs->rs_flows_using, -1);
	if (pre == 1) {
		mtx_lock(&rs_mtx);
		/*
		 * Is it dead?
		 */
		if (rs->rs_flags & RS_IS_DEAD)
			rs_defer_destroy(rs);
		mtx_unlock(&rs_mtx);
	}
	in_pcbdetach_txrtlmt(tp->t_inpcb);
}

static eventhandler_tag rl_ifnet_departs;
static eventhandler_tag rl_ifnet_arrives;
static eventhandler_tag rl_shutdown_start;

static void
tcp_rs_init(void *st __unused)
{
	CK_LIST_INIT(&int_rs);
	rs_number_alive = 0;
	rs_number_dead = 0;;
	mtx_init(&rs_mtx, "tcp_rs_mtx", "rsmtx", MTX_DEF);
	rl_ifnet_departs = EVENTHANDLER_REGISTER(ifnet_departure_event,
	    tcp_rl_ifnet_departure,
	    NULL, EVENTHANDLER_PRI_ANY);
	rl_ifnet_arrives = EVENTHANDLER_REGISTER(ifnet_link_event,
	    tcp_rl_ifnet_link,
	    NULL, EVENTHANDLER_PRI_ANY);
	rl_shutdown_start = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    tcp_rl_shutdown, NULL,
	    SHUTDOWN_PRI_FIRST);
	printf("TCP_ratelimit: Is now initialized\n");
}

SYSINIT(tcp_rl_init, SI_SUB_SMP + 1, SI_ORDER_ANY, tcp_rs_init, NULL);
#endif
