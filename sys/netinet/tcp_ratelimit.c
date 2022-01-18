/*-
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2020
 *	Netflix Inc.
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
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#define TCPSTATES		/* for logging */
#include <netinet/tcp_var.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_ratelimit.h>
#ifndef USECS_IN_SECOND
#define USECS_IN_SECOND 1000000
#endif
/*
 * For the purposes of each send, what is the size
 * of an ethernet frame.
 */
MALLOC_DEFINE(M_TCPPACE, "tcp_hwpace", "TCP Hardware pacing memory");
#ifdef RATELIMIT

/*
 * The following preferred table will seem weird to
 * the casual viewer. Why do we not have any rates below
 * 1Mbps? Why do we have a rate at 1.44Mbps called common?
 * Why do the rates cluster in the 1-100Mbps range more
 * than others? Why does the table jump around at the beginnign
 * and then be more consistently raising?
 *
 * Let me try to answer those questions. A lot of
 * this is dependant on the hardware. We have three basic
 * supporters of rate limiting
 *
 * Chelsio - Supporting 16 configurable rates.
 * Mlx  - c4 supporting 13 fixed rates.
 * Mlx  - c5 & c6 supporting 127 configurable rates.
 *
 * The c4 is why we have a common rate that is available
 * in all rate tables. This is a selected rate from the
 * c4 table and we assure its available in all ratelimit
 * tables. This way the tcp_ratelimit code has an assured
 * rate it should always be able to get. This answers a
 * couple of the questions above.
 *
 * So what about the rest, well the table is built to
 * try to get the most out of a joint hardware/software
 * pacing system.  The software pacer will always pick
 * a rate higher than the b/w that it is estimating
 *
 * on the path. This is done for two reasons.
 * a) So we can discover more b/w
 * and
 * b) So we can send a block of MSS's down and then
 *    have the software timer go off after the previous
 *    send is completely out of the hardware.
 *
 * But when we do <b> we don't want to have the delay
 * between the last packet sent by the hardware be
 * excessively long (to reach our desired rate).
 *
 * So let me give an example for clarity.
 *
 * Lets assume that the tcp stack sees that 29,110,000 bps is
 * what the bw of the path is. The stack would select the
 * rate 31Mbps. 31Mbps means that each send that is done
 * by the hardware will cause a 390 micro-second gap between
 * the packets sent at that rate. For 29,110,000 bps we
 * would need 416 micro-seconds gap between each send.
 *
 * Note that are calculating a complete time for pacing
 * which includes the ethernet, IP and TCP overhead. So
 * a full 1514 bytes is used for the above calculations.
 * My testing has shown that both cards are also using this
 * as their basis i.e. full payload size of the ethernet frame.
 * The TCP stack caller needs to be aware of this and make the
 * appropriate overhead calculations be included in its choices.
 *
 * Now, continuing our example, we pick a MSS size based on the
 * delta between the two rates (416 - 390) divided into the rate
 * we really wish to send at rounded up.  That results in a MSS
 * send of 17 mss's at once. The hardware then will
 * run out of data in a single 17MSS send in 6,630 micro-seconds.
 *
 * On the other hand the software pacer will send more data
 * in 7,072 micro-seconds. This means that we will refill
 * the hardware 52 microseconds after it would have sent
 * next if it had not ran out of data. This is a win since we are
 * only sending every 7ms or so and yet all the packets are spaced on
 * the wire with 94% of what they should be and only
 * the last packet is delayed extra to make up for the
 * difference.
 *
 * Note that the above formula has two important caveat.
 * If we are above (b/w wise) over 100Mbps we double the result
 * of the MSS calculation. The second caveat is if we are 500Mbps
 * or more we just send the maximum MSS at once i.e. 45MSS. At
 * the higher b/w's even the cards have limits to what times (timer granularity)
 * they can insert between packets and start to send more than one
 * packet at a time on the wire.
 *
 */
#define COMMON_RATE 180500
const uint64_t desired_rates[] = {
	122500,			/* 1Mbps  - rate 1 */
	180500,			/* 1.44Mpbs - rate 2  common rate */
	375000,			/* 3Mbps    - rate 3 */
	625000,			/* 5Mbps    - rate 4 */
	1250000,		/* 10Mbps   - rate 5 */
	1875000,		/* 15Mbps   - rate 6 */
	2500000,		/* 20Mbps   - rate 7 */
	3125000,	       	/* 25Mbps   - rate 8 */
	3750000,		/* 30Mbps   - rate 9 */
	4375000,		/* 35Mbps   - rate 10 */
	5000000,		/* 40Meg    - rate 11 */
	6250000,		/* 50Mbps   - rate 12 */
	12500000,		/* 100Mbps  - rate 13 */
	25000000,		/* 200Mbps  - rate 14 */
	50000000,		/* 400Mbps  - rate 15 */
	100000000,		/* 800Mbps  - rate 16 */
	5625000,		/* 45Mbps   - rate 17 */
	6875000,		/* 55Mbps   - rate 19 */
	7500000,		/* 60Mbps   - rate 20 */
	8125000,		/* 65Mbps   - rate 21 */
	8750000,		/* 70Mbps   - rate 22 */
	9375000,		/* 75Mbps   - rate 23 */
	10000000,		/* 80Mbps   - rate 24 */
	10625000,		/* 85Mbps   - rate 25 */
	11250000,		/* 90Mbps   - rate 26 */
	11875000,		/* 95Mbps   - rate 27 */
	12500000,		/* 100Mbps  - rate 28 */
	13750000,		/* 110Mbps  - rate 29 */
	15000000,		/* 120Mbps  - rate 30 */
	16250000,		/* 130Mbps  - rate 31 */
	17500000,		/* 140Mbps  - rate 32 */
	18750000,		/* 150Mbps  - rate 33 */
	20000000,		/* 160Mbps  - rate 34 */
	21250000,		/* 170Mbps  - rate 35 */
	22500000,		/* 180Mbps  - rate 36 */
	23750000,		/* 190Mbps  - rate 37 */
	26250000,		/* 210Mbps  - rate 38 */
	27500000,		/* 220Mbps  - rate 39 */
	28750000,		/* 230Mbps  - rate 40 */
	30000000,	       	/* 240Mbps  - rate 41 */
	31250000,		/* 250Mbps  - rate 42 */
	34375000,		/* 275Mbps  - rate 43 */
	37500000,		/* 300Mbps  - rate 44 */
	40625000,		/* 325Mbps  - rate 45 */
	43750000,		/* 350Mbps  - rate 46 */
	46875000,		/* 375Mbps  - rate 47 */
	53125000,		/* 425Mbps  - rate 48 */
	56250000,		/* 450Mbps  - rate 49 */
	59375000,		/* 475Mbps  - rate 50 */
	62500000,		/* 500Mbps  - rate 51 */
	68750000,		/* 550Mbps  - rate 52 */
	75000000,		/* 600Mbps  - rate 53 */
	81250000,		/* 650Mbps  - rate 54 */
	87500000,		/* 700Mbps  - rate 55 */
	93750000,		/* 750Mbps  - rate 56 */
	106250000,		/* 850Mbps  - rate 57 */
	112500000,		/* 900Mbps  - rate 58 */
	125000000,		/* 1Gbps    - rate 59 */
	156250000,		/* 1.25Gps  - rate 60 */
	187500000,		/* 1.5Gps   - rate 61 */
	218750000,		/* 1.75Gps  - rate 62 */
	250000000,		/* 2Gbps    - rate 63 */
	281250000,		/* 2.25Gps  - rate 64 */
	312500000,		/* 2.5Gbps  - rate 65 */
	343750000,		/* 2.75Gbps - rate 66 */
	375000000,		/* 3Gbps    - rate 67 */
	500000000,		/* 4Gbps    - rate 68 */
	625000000,		/* 5Gbps    - rate 69 */
	750000000,		/* 6Gbps    - rate 70 */
	875000000,		/* 7Gbps    - rate 71 */
	1000000000,		/* 8Gbps    - rate 72 */
	1125000000,		/* 9Gbps    - rate 73 */
	1250000000,		/* 10Gbps   - rate 74 */
	1875000000,		/* 15Gbps   - rate 75 */
	2500000000		/* 20Gbps   - rate 76 */
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
static uint32_t rs_floor_mss = 0;
static uint32_t wait_time_floor = 8000;	/* 8 ms */
static uint32_t rs_hw_floor_mss = 16;
static uint32_t num_of_waits_allowed = 1; /* How many time blocks are we willing to wait */

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, rl, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP Ratelimit stats");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, alive, CTLFLAG_RW,
    &rs_number_alive, 0,
    "Number of interfaces initialized for ratelimiting");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, dead, CTLFLAG_RW,
    &rs_number_dead, 0,
    "Number of interfaces departing from ratelimiting");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, floor_mss, CTLFLAG_RW,
    &rs_floor_mss, 0,
    "Number of MSS that will override the normal minimums (0 means don't enforce)");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, wait_floor, CTLFLAG_RW,
    &wait_time_floor, 2000,
    "Has b/w increases what is the wait floor we are willing to wait at the end?");
SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, time_blocks, CTLFLAG_RW,
    &num_of_waits_allowed, 1,
    "How many time blocks on the end should software pacing be willing to wait?");

SYSCTL_UINT(_net_inet_tcp_rl, OID_AUTO, hw_floor_mss, CTLFLAG_RW,
    &rs_hw_floor_mss, 16,
    "Number of mss that are a minum for hardware pacing?");


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
					    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
					    "Ratelist");
		for( i = 0; i < rs->rs_rate_cnt; i++) {
			sprintf(rate_num, "%d", i);
			rl_rate_num = SYSCTL_ADD_NODE(&rs->sysctl_ctx,
					    SYSCTL_CHILDREN(rl_rates),
					    OID_AUTO,
					    rate_num,
					    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
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
			SYSCTL_ADD_LONG(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "rate", CTLFLAG_RD,
				       &rs->rs_rlt[i].rate,
				       "Rate in bytes per second");
			SYSCTL_ADD_LONG(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "using", CTLFLAG_RD,
				       &rs->rs_rlt[i].using,
				       "Number of flows using");
			SYSCTL_ADD_LONG(&rs->sysctl_ctx,
				       SYSCTL_CHILDREN(rl_rate_num),
				       OID_AUTO, "enobufs", CTLFLAG_RD,
				       &rs->rs_rlt[i].rs_num_enobufs,
				       "Number of enobufs logged on this rate");

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
	NET_EPOCH_CALL(rs_destroy, &rs->rs_epoch_ctx);
}

#ifdef INET
extern counter_u64_t rate_limit_new;
extern counter_u64_t rate_limit_chg;
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

	error = m_snd_tag_alloc(ifp, &params, tag);
#ifdef INET
	if (error == 0) {
		counter_u64_add(rate_limit_set_ok, 1);
		counter_u64_add(rate_limit_active, 1);
	} else if (error != EOPNOTSUPP)
		counter_u64_add(rate_limit_alloc_fail, 1);
#endif
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
	struct epoch_tracker et;
	/*
	 * We expect to enter with the
	 * mutex locked.
	 */

	if (ifp->if_ratelimit_query == NULL) {
		/*
		 * We can do nothing if we cannot
		 * get a query back from the driver.
		 */
		printf("Warning:No query functions for %s:%d-- failed\n",
		       ifp->if_dname, ifp->if_dunit);
		return (NULL);
	}
	rs = malloc(sizeof(struct tcp_rate_set), M_TCPPACE, M_NOWAIT | M_ZERO);
	if (rs == NULL) {
		if (error)
			*error = ENOMEM;
		printf("Warning:No memory for malloc of tcp_rate_set\n");
		return (NULL);
	}
	memset(&rl, 0, sizeof(rl));
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
		    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
		    "");
		rl_add_syctl_entries(rl_sysctl_root, rs);
		NET_EPOCH_ENTER(et);
		mtx_lock(&rs_mtx);
		CK_LIST_INSERT_HEAD(&int_rs, rs, next);
		mtx_unlock(&rs_mtx);
		NET_EPOCH_EXIT(et);
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
		    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
		    "");
		rl_add_syctl_entries(rl_sysctl_root, rs);
		NET_EPOCH_ENTER(et);
		mtx_lock(&rs_mtx);
		CK_LIST_INSERT_HEAD(&int_rs, rs, next);
		mtx_unlock(&rs_mtx);
		NET_EPOCH_EXIT(et);
		return (rs);
	} else if ((rl.flags & RT_IS_FIXED_TABLE) == RT_IS_FIXED_TABLE) {
		/* Mellanox C4 likely */
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
		/* Chelsio, C5 and C6 of Mellanox? */
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
		rs->rs_rlt[i].using = 0;
		rs->rs_rlt[i].rs_num_enobufs = 0;
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

			if ((rl.flags & RT_IS_SETUP_REQ)  &&
			    (ifp->if_ratelimit_query)) {
				err = ifp->if_ratelimit_setup(ifp,
  				         rs->rs_rlt[i].rate, i);
				if (err)
					goto handle_err;
			}
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
handle_err:
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
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "");
	rl_add_syctl_entries(rl_sysctl_root, rs);
	NET_EPOCH_ENTER(et);
	mtx_lock(&rs_mtx);
	CK_LIST_INSERT_HEAD(&int_rs, rs, next);
	mtx_unlock(&rs_mtx);
	NET_EPOCH_EXIT(et);
	return (rs);
}

/*
 * For an explanation of why the argument is volatile please
 * look at the comments around rt_setup_rate().
 */
static const struct tcp_hwrate_limit_table *
tcp_int_find_suitable_rate(const volatile struct tcp_rate_set *rs,
    uint64_t bytes_per_sec, uint32_t flags, uint64_t *lower_rate)
{
	struct tcp_hwrate_limit_table *arte = NULL, *rte = NULL;
	uint64_t mbits_per_sec, ind_calc, previous_rate = 0;
	int i;

	mbits_per_sec = (bytes_per_sec * 8);
	if (flags & RS_PACING_LT) {
		if ((mbits_per_sec < RS_ONE_MEGABIT_PERSEC) &&
		    (rs->rs_lowest_valid <= 2)){
			/*
			 * Smaller than 1Meg, only
			 * 3 entries can match it.
			 */
			previous_rate = 0;
			for(i = rs->rs_lowest_valid; i < 3; i++) {
				if (bytes_per_sec <= rs->rs_rlt[i].rate) {
					rte = &rs->rs_rlt[i];
					break;
				} else if (rs->rs_rlt[i].flags & HDWRPACE_INITED) {
					arte = &rs->rs_rlt[i];
				}
				previous_rate = rs->rs_rlt[i].rate;
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
			previous_rate = rs->rs_rlt[(ALL_HARDWARE_RATES-2)].rate;
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
		    (ind_calc <= rs->rs_highest_valid)) {
			rte = &rs->rs_rlt[ind_calc];
			if (ind_calc >= 1)
				previous_rate = rs->rs_rlt[(ind_calc-1)].rate;
		}
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
					if (i >= 1) {
						previous_rate = rs->rs_rlt[(i-1)].rate;
					}
					break;
				} else if ((flags & RS_PACING_GEQ) &&
					   (bytes_per_sec == rs->rs_rlt[i].rate)) {
					rte = &rs->rs_rlt[i];
					if (i >= 1) {
						previous_rate = rs->rs_rlt[(i-1)].rate;
					}
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
			previous_rate = rs->rs_rlt[(ALL_HARDWARE_RATES-2)].rate;
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
					if (ind_calc >= 1)
						previous_rate = rs->rs_rlt[(ind_calc-1)].rate;
				}
				goto done;
			}
			ind_calc = (mbits_per_sec + (RS_ONE_MEGABIT_PERSEC-1))/RS_ONE_MEGABIT_PERSEC;
			ind_calc += 2;
			if (ind_calc > (ALL_HARDWARE_RATES-1)) {
				/* This should not happen */
				ind_calc = ALL_HARDWARE_RATES-1;
			}
			if (rs->rs_rlt[ind_calc].flags & HDWRPACE_INITED) {
				rte = &rs->rs_rlt[ind_calc];
				if (ind_calc >= 1)
					previous_rate = rs->rs_rlt[(ind_calc-1)].rate;
			}
		}
	}
done:
	if ((rte == NULL) &&
	    (arte != NULL) &&
	    (flags & RS_PACING_SUB_OK)) {
		/* We can use the substitute */
		rte = arte;
	}
	if (lower_rate)
		*lower_rate = previous_rate;
	return (rte);
}

/*
 * For an explanation of why the argument is volatile please
 * look at the comments around rt_setup_rate().
 */
static const struct tcp_hwrate_limit_table *
tcp_find_suitable_rate(const volatile struct tcp_rate_set *rs, uint64_t bytes_per_sec, uint32_t flags, uint64_t *lower_rate)
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
	uint64_t previous_rate = 0;

	if ((rs->rs_flags & RS_INT_TBL) &&
	    (rs->rs_rate_cnt >= ALL_HARDWARE_RATES)) {
		/*
		 * Here we don't want to paw thru
		 * a big table, we have everything
		 * from 1Meg - 1000Meg in 1Meg increments.
		 * Use an alternate method to "lookup".
		 */
		return (tcp_int_find_suitable_rate(rs, bytes_per_sec, flags, lower_rate));
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
				if (lower_rate != NULL)
					*lower_rate = previous_rate;
				break;
			} else if ((flags & RS_PACING_LT) &&
			    (bytes_per_sec <= rs->rs_rlt[i].rate)) {
				rte = &rs->rs_rlt[i];
				matched = 1;
				if (lower_rate != NULL)
					*lower_rate = previous_rate;
				break;
			}
			previous_rate = rs->rs_rlt[i].rate;
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
				if (lower_rate != NULL)
					*lower_rate = rs->rs_rlt[i].rate;
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
	struct m_snd_tag *tag, *ntag;
	union if_snd_tag_alloc_params params = {
		.rate_limit.hdr.type = IF_SND_TAG_TYPE_RATE_LIMIT,
		.rate_limit.hdr.flowid = inp->inp_flowid,
		.rate_limit.hdr.numa_domain = inp->inp_numa_domain,
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
	err = m_snd_tag_alloc(ifp, &params, &tag);
	if (err) {
		/* Failed to setup a tag? */
		if (error)
			*error = err;
		return (NULL);
	}
	ntag = tag;
	while (ntag->sw->next_snd_tag != NULL) {
		ntag = ntag->sw->next_snd_tag(ntag);
	}
	tifp = ntag->ifp;
	m_snd_tag_rele(tag);
	return (tifp);
}

static void
rl_increment_using(const struct tcp_hwrate_limit_table *rte)
{
	struct tcp_hwrate_limit_table *decon_rte;

	decon_rte = __DECONST(struct tcp_hwrate_limit_table *, rte);
	atomic_add_long(&decon_rte->using, 1);
}

static void
rl_decrement_using(const struct tcp_hwrate_limit_table *rte)
{
	struct tcp_hwrate_limit_table *decon_rte;

	decon_rte = __DECONST(struct tcp_hwrate_limit_table *, rte);
	atomic_subtract_long(&decon_rte->using, 1);
}

void
tcp_rl_log_enobuf(const struct tcp_hwrate_limit_table *rte)
{
	struct tcp_hwrate_limit_table *decon_rte;

	decon_rte = __DECONST(struct tcp_hwrate_limit_table *, rte);
	atomic_add_long(&decon_rte->rs_num_enobufs, 1);
}

/*
 * Do NOT take the __noinline out of the
 * find_rs_for_ifp() function. If you do the inline
 * of it for the rt_setup_rate() will show you a
 * compiler bug. For some reason the compiler thinks
 * the list can never be empty. The consequence of
 * this will be a crash when we dereference NULL
 * if an ifp is removed just has a hw rate limit
 * is attempted. If you are working on the compiler
 * and want to "test" this go ahead and take the noinline
 * out otherwise let sleeping dogs ly until such time
 * as we get a compiler fix 10/2/20 -- RRS
 */
static __noinline struct tcp_rate_set *
find_rs_for_ifp(struct ifnet *ifp)
{
	struct tcp_rate_set *rs;

	CK_LIST_FOREACH(rs, &int_rs, next) {
		if ((rs->rs_ifp == ifp) &&
		    (rs->rs_if_dunit == ifp->if_dunit)) {
			/* Ok we found it */
			return (rs);
		}
	}
	return (NULL);
}


static const struct tcp_hwrate_limit_table *
rt_setup_rate(struct inpcb *inp, struct ifnet *ifp, uint64_t bytes_per_sec,
    uint32_t flags, int *error, uint64_t *lower_rate)
{
	/* First lets find the interface if it exists */
	const struct tcp_hwrate_limit_table *rte;
	/*
	 * So why is rs volatile? This is to defeat a
	 * compiler bug where in the compiler is convinced
	 * that rs can never be NULL (which is not true). Because
	 * of its conviction it nicely optimizes out the if ((rs == NULL
	 * below which means if you get a NULL back you dereference it.
	 */
	volatile struct tcp_rate_set *rs;
	struct epoch_tracker et;
	struct ifnet *oifp = ifp;
	int err;

	NET_EPOCH_ENTER(et);
use_real_interface:
	rs = find_rs_for_ifp(ifp);
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
		if (error)
			*error = ENODEV;
		NET_EPOCH_EXIT(et);
		return (NULL);
	}

	if ((rs == NULL) || (rs->rs_disable != 0)) {
		if (error)
			*error = ENOSPC;
		NET_EPOCH_EXIT(et);
		return (NULL);
	}
	if (rs->rs_flags & RS_IS_DEFF) {
		/* We need to find the real interface */
		struct ifnet *tifp;

		tifp = rt_find_real_interface(ifp, inp, error);
		if (tifp == NULL) {
			if (rs->rs_disable && error)
				*error = ENOTSUP;
			NET_EPOCH_EXIT(et);
			return (NULL);
		}
		KASSERT((tifp != ifp),
			("Lookup failure ifp:%p inp:%p rt_find_real_interface() returns the same interface tifp:%p?\n",
			 ifp, inp, tifp));
		ifp = tifp;
		goto use_real_interface;
	}
	if (rs->rs_flow_limit &&
	    ((rs->rs_flows_using + 1) > rs->rs_flow_limit)) {
		if (error)
			*error = ENOSPC;
		NET_EPOCH_EXIT(et);
		return (NULL);
	}
	rte = tcp_find_suitable_rate(rs, bytes_per_sec, flags, lower_rate);
	if (rte) {
		err = in_pcbattach_txrtlmt(inp, oifp,
		    inp->inp_flowtype,
		    inp->inp_flowid,
		    rte->rate,
		    &inp->inp_snd_tag);
		if (err) {
			/* Failed to attach */
			if (error)
				*error = err;
			rte = NULL;
		} else {
			KASSERT((inp->inp_snd_tag != NULL) ,
				("Setup rate has no snd_tag inp:%p rte:%p rate:%llu rs:%p",
				 inp, rte, (unsigned long long)rte->rate, rs));
#ifdef INET
			counter_u64_add(rate_limit_new, 1);
#endif
		}
	}
	if (rte) {
		/*
		 * We use an atomic here for accounting so we don't have to
		 * use locks when freeing.
		 */
		atomic_add_64(&rs->rs_flows_using, 1);
	}
	NET_EPOCH_EXIT(et);
	return (rte);
}

static void
tcp_rl_ifnet_link(void *arg __unused, struct ifnet *ifp, int link_state)
{
	int error;
	struct tcp_rate_set *rs;
	struct epoch_tracker et;

	if (((ifp->if_capenable & IFCAP_TXRTLMT) == 0) ||
	    (link_state != LINK_STATE_UP)) {
		/*
		 * We only care on an interface going up that is rate-limit
		 * capable.
		 */
		return;
	}
	NET_EPOCH_ENTER(et);
	mtx_lock(&rs_mtx);
	rs = find_rs_for_ifp(ifp);
	if (rs) {
		/* We already have initialized this guy */
		mtx_unlock(&rs_mtx);
		NET_EPOCH_EXIT(et);
		return;
	}
	mtx_unlock(&rs_mtx);
	NET_EPOCH_EXIT(et);
	rt_setup_new_rs(ifp, &error);
}

static void
tcp_rl_ifnet_departure(void *arg __unused, struct ifnet *ifp)
{
	struct tcp_rate_set *rs;
	struct epoch_tracker et;
	int i;

	NET_EPOCH_ENTER(et);
	mtx_lock(&rs_mtx);
	rs = find_rs_for_ifp(ifp);
	if (rs) {
		CK_LIST_REMOVE(rs, next);
		rs_number_alive--;
		rs->rs_flags |= RS_IS_DEAD;
		for (i = 0; i < rs->rs_rate_cnt; i++) {
			if (rs->rs_rlt[i].flags & HDWRPACE_TAGPRESENT) {
				in_pcbdetach_tag(rs->rs_rlt[i].tag);
				rs->rs_rlt[i].tag = NULL;
			}
			rs->rs_rlt[i].flags = HDWRPACE_IFPDEPARTED;
		}
		if (rs->rs_flows_using == 0)
			rs_defer_destroy(rs);
	}
	mtx_unlock(&rs_mtx);
	NET_EPOCH_EXIT(et);
}

static void
tcp_rl_shutdown(void *arg __unused, int howto __unused)
{
	struct tcp_rate_set *rs, *nrs;
	struct epoch_tracker et;
	int i;

	NET_EPOCH_ENTER(et);
	mtx_lock(&rs_mtx);
	CK_LIST_FOREACH_SAFE(rs, &int_rs, next, nrs) {
		CK_LIST_REMOVE(rs, next);
		rs_number_alive--;
		rs->rs_flags |= RS_IS_DEAD;
		for (i = 0; i < rs->rs_rate_cnt; i++) {
			if (rs->rs_rlt[i].flags & HDWRPACE_TAGPRESENT) {
				in_pcbdetach_tag(rs->rs_rlt[i].tag);
				rs->rs_rlt[i].tag = NULL;
			}
			rs->rs_rlt[i].flags = HDWRPACE_IFPDEPARTED;
		}
		if (rs->rs_flows_using == 0)
			rs_defer_destroy(rs);
	}
	mtx_unlock(&rs_mtx);
	NET_EPOCH_EXIT(et);
}

const struct tcp_hwrate_limit_table *
tcp_set_pacing_rate(struct tcpcb *tp, struct ifnet *ifp,
    uint64_t bytes_per_sec, int flags, int *error, uint64_t *lower_rate)
{
	const struct tcp_hwrate_limit_table *rte;
#ifdef KERN_TLS
	struct ktls_session *tls;
#endif

	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_inpcb->inp_snd_tag == NULL) {
		/*
		 * We are setting up a rate for the first time.
		 */
		if ((ifp->if_capenable & IFCAP_TXRTLMT) == 0) {
			/* Not supported by the egress */
			if (error)
				*error = ENODEV;
			return (NULL);
		}
#ifdef KERN_TLS
		tls = NULL;
		if (tp->t_inpcb->inp_socket->so_snd.sb_flags & SB_TLS_IFNET) {
			tls = tp->t_inpcb->inp_socket->so_snd.sb_tls_info;

			if ((ifp->if_capenable & IFCAP_TXTLS_RTLMT) == 0 ||
			    tls->mode != TCP_TLS_MODE_IFNET) {
				if (error)
					*error = ENODEV;
				return (NULL);
			}
		}
#endif
		rte = rt_setup_rate(tp->t_inpcb, ifp, bytes_per_sec, flags, error, lower_rate);
		if (rte)
			rl_increment_using(rte);
#ifdef KERN_TLS
		if (rte != NULL && tls != NULL && tls->snd_tag != NULL) {
			/*
			 * Fake a route change error to reset the TLS
			 * send tag.  This will convert the existing
			 * tag to a TLS ratelimit tag.
			 */
			MPASS(tls->snd_tag->sw->type == IF_SND_TAG_TYPE_TLS);
			ktls_output_eagain(tp->t_inpcb, tls);
		}
#endif
	} else {
		/*
		 * We are modifying a rate, wrong interface?
		 */
		if (error)
			*error = EINVAL;
		rte = NULL;
	}
	if (rte != NULL) {
		tp->t_pacing_rate = rte->rate;
		*error = 0;
	}
	return (rte);
}

const struct tcp_hwrate_limit_table *
tcp_chg_pacing_rate(const struct tcp_hwrate_limit_table *crte,
    struct tcpcb *tp, struct ifnet *ifp,
    uint64_t bytes_per_sec, int flags, int *error, uint64_t *lower_rate)
{
	const struct tcp_hwrate_limit_table *nrte;
	const struct tcp_rate_set *rs;
#ifdef KERN_TLS
	struct ktls_session *tls = NULL;
#endif
	int err;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (crte == NULL) {
		/* Wrong interface */
		if (error)
			*error = EINVAL;
		return (NULL);
	}

#ifdef KERN_TLS
	if (tp->t_inpcb->inp_socket->so_snd.sb_flags & SB_TLS_IFNET) {
		tls = tp->t_inpcb->inp_socket->so_snd.sb_tls_info;
		MPASS(tls->mode == TCP_TLS_MODE_IFNET);
		if (tls->snd_tag != NULL &&
		    tls->snd_tag->sw->type != IF_SND_TAG_TYPE_TLS_RATE_LIMIT) {
			/*
			 * NIC probably doesn't support ratelimit TLS
			 * tags if it didn't allocate one when an
			 * existing rate was present, so ignore.
			 */
			if (error)
				*error = EOPNOTSUPP;
			return (NULL);
		}
	}
#endif
	if (tp->t_inpcb->inp_snd_tag == NULL) {
		/* Wrong interface */
		if (error)
			*error = EINVAL;
		return (NULL);
	}
	rs = crte->ptbl;
	if ((rs->rs_flags & RS_IS_DEAD) ||
	    (crte->flags & HDWRPACE_IFPDEPARTED)) {
		/* Release the rate, and try anew */

		tcp_rel_pacing_rate(crte, tp);
		nrte = tcp_set_pacing_rate(tp, ifp,
		    bytes_per_sec, flags, error, lower_rate);
		return (nrte);
	}
	nrte = tcp_find_suitable_rate(rs, bytes_per_sec, flags, lower_rate);
	if (nrte == crte) {
		/* No change */
		if (error)
			*error = 0;
		return (crte);
	}
	if (nrte == NULL) {
		/* Release the old rate */
		if (error)
			*error = ENOENT;
		tcp_rel_pacing_rate(crte, tp);
		return (NULL);
	}
	rl_decrement_using(crte);
	rl_increment_using(nrte);
	/* Change rates to our new entry */
#ifdef KERN_TLS
	if (tls != NULL)
		err = ktls_modify_txrtlmt(tls, nrte->rate);
	else
#endif
		err = in_pcbmodify_txrtlmt(tp->t_inpcb, nrte->rate);
	if (err) {
		rl_decrement_using(nrte);
		/* Do we still have a snd-tag attached? */
		if (tp->t_inpcb->inp_snd_tag)
			in_pcbdetach_txrtlmt(tp->t_inpcb);
		if (error)
			*error = err;
		return (NULL);
	} else {
#ifdef INET
		counter_u64_add(rate_limit_chg, 1);
#endif
	}
	if (error)
		*error = 0;
	tp->t_pacing_rate = nrte->rate;
	return (nrte);
}

void
tcp_rel_pacing_rate(const struct tcp_hwrate_limit_table *crte, struct tcpcb *tp)
{
	const struct tcp_rate_set *crs;
	struct tcp_rate_set *rs;
	uint64_t pre;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	tp->t_pacing_rate = -1;
	crs = crte->ptbl;
	/*
	 * Now we must break the const
	 * in order to release our refcount.
	 */
	rs = __DECONST(struct tcp_rate_set *, crs);
	rl_decrement_using(crte);
	pre = atomic_fetchadd_64(&rs->rs_flows_using, -1);
	if (pre == 1) {
		struct epoch_tracker et;

		NET_EPOCH_ENTER(et);
		mtx_lock(&rs_mtx);
		/*
		 * Is it dead?
		 */
		if (rs->rs_flags & RS_IS_DEAD)
			rs_defer_destroy(rs);
		mtx_unlock(&rs_mtx);
		NET_EPOCH_EXIT(et);
	}

	/*
	 * XXX: If this connection is using ifnet TLS, should we
	 * switch it to using an unlimited rate, or perhaps use
	 * ktls_output_eagain() to reset the send tag to a plain
	 * TLS tag?
	 */
	in_pcbdetach_txrtlmt(tp->t_inpcb);
}

#define ONE_POINT_TWO_MEG 150000 /* 1.2 megabits in bytes */
#define ONE_HUNDRED_MBPS 12500000	/* 100Mbps in bytes per second */
#define FIVE_HUNDRED_MBPS 62500000	/* 500Mbps in bytes per second */
#define MAX_MSS_SENT 43	/* 43 mss = 43 x 1500 = 64,500 bytes */

static void
tcp_log_pacing_size(struct tcpcb *tp, uint64_t bw, uint32_t segsiz, uint32_t new_tso,
		    uint64_t hw_rate, uint32_t time_between, uint32_t calc_time_between,
		    uint32_t segs, uint32_t res_div, uint16_t mult, uint8_t mod)
{
	if (tp->t_logstate != TCP_LOG_STATE_OFF) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = segsiz;
		log.u_bbr.flex2 = new_tso;
		log.u_bbr.flex3 = time_between;
		log.u_bbr.flex4 = calc_time_between;
		log.u_bbr.flex5 = segs;
		log.u_bbr.flex6 = res_div;
		log.u_bbr.flex7 = mult;
		log.u_bbr.flex8 = mod;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.cur_del_rate = bw;
		log.u_bbr.delRate = hw_rate;
		TCP_LOG_EVENTP(tp, NULL,
		    &tp->t_inpcb->inp_socket->so_rcv,
		    &tp->t_inpcb->inp_socket->so_snd,
		    TCP_HDWR_PACE_SIZE, 0,
		    0, &log, false, &tv);
	}
}

uint32_t
tcp_get_pacing_burst_size (struct tcpcb *tp, uint64_t bw, uint32_t segsiz, int can_use_1mss,
   const struct tcp_hwrate_limit_table *te, int *err)
{
	/*
	 * We use the google formula to calculate the
	 * TSO size. I.E.
	 * bw < 24Meg
	 *   tso = 2mss
	 * else
	 *   tso = min(bw/1000, 64k)
	 *
	 * Note for these calculations we ignore the
	 * packet overhead (enet hdr, ip hdr and tcp hdr).
	 */
	uint64_t lentim, res, bytes;
	uint32_t new_tso, min_tso_segs;

	bytes = bw / 1000;
	if (bytes > (64 * 1000))
		bytes = 64 * 1000;
	/* Round up */
	new_tso = (bytes + segsiz - 1) / segsiz;
	if (can_use_1mss && (bw < ONE_POINT_TWO_MEG))
		min_tso_segs = 1;
	else
		min_tso_segs = 2;
	if (rs_floor_mss && (new_tso < rs_floor_mss))
		new_tso = rs_floor_mss;
	else if (new_tso < min_tso_segs)
		new_tso = min_tso_segs;
	if (new_tso > MAX_MSS_SENT)
		new_tso = MAX_MSS_SENT;
	new_tso *= segsiz;
 	tcp_log_pacing_size(tp, bw, segsiz, new_tso,
			    0, 0, 0, 0, 0, 0, 1);
	/*
	 * If we are not doing hardware pacing
	 * then we are done.
	 */
	if (te == NULL) {
		if (err)
			*err = 0;
		return(new_tso);
	}
	/*
	 * For hardware pacing we look at the
	 * rate you are sending at and compare
	 * that to the rate you have in hardware.
	 *
	 * If the hardware rate is slower than your
	 * software rate then you are in error and
	 * we will build a queue in our hardware whic
	 * is probably not desired, in such a case
	 * just return the non-hardware TSO size.
	 *
	 * If the rate in hardware is faster (which
	 * it should be) then look at how long it
	 * takes to send one ethernet segment size at
	 * your b/w and compare that to the time it
	 * takes to send at the rate you had selected.
	 *
	 * If your time is greater (which we hope it is)
	 * we get the delta between the two, and then
	 * divide that into your pacing time. This tells
	 * us how many MSS you can send down at once (rounded up).
	 *
	 * Note we also double this value if the b/w is over
	 * 100Mbps. If its over 500meg we just set you to the
	 * max (43 segments).
	 */
	if (te->rate > FIVE_HUNDRED_MBPS)
		goto max;
	if (te->rate == bw) {
		/* We are pacing at exactly the hdwr rate */
max:
		tcp_log_pacing_size(tp, bw, segsiz, new_tso,
				    te->rate, te->time_between, (uint32_t)0,
				    (segsiz * MAX_MSS_SENT), 0, 0, 3);
		return (segsiz * MAX_MSS_SENT);
	}
	lentim = ETHERNET_SEGMENT_SIZE * USECS_IN_SECOND;
	res = lentim / bw;
	if (res > te->time_between) {
		uint32_t delta, segs, res_div;

		res_div = ((res * num_of_waits_allowed) + wait_time_floor);
		delta = res - te->time_between;
		segs = (res_div + delta - 1)/delta;
		if (segs < min_tso_segs)
			segs = min_tso_segs;
		if (segs < rs_hw_floor_mss)
			segs = rs_hw_floor_mss;
		if (segs > MAX_MSS_SENT)
			segs = MAX_MSS_SENT;
		segs *= segsiz;
		tcp_log_pacing_size(tp, bw, segsiz, new_tso,
				    te->rate, te->time_between, (uint32_t)res,
				    segs, res_div, 1, 3);
		if (err)
			*err = 0;
		if (segs < new_tso) {
			/* unexpected ? */
			return(new_tso);
		} else {
			return (segs);
		}
	} else {
		/*
		 * Your time is smaller which means
		 * we will grow a queue on our
		 * hardware. Send back the non-hardware
		 * rate.
		 */
		tcp_log_pacing_size(tp, bw, segsiz, new_tso,
				    te->rate, te->time_between, (uint32_t)res,
				    0, 0, 0, 4);
		if (err)
			*err = -1;
		return (new_tso);
	}
}

uint64_t
tcp_hw_highest_rate_ifp(struct ifnet *ifp, struct inpcb *inp)
{
	struct epoch_tracker et;
	struct tcp_rate_set *rs;
	uint64_t rate_ret;

	NET_EPOCH_ENTER(et);
use_next_interface:
	rs = find_rs_for_ifp(ifp);
	if (rs == NULL) {
		/* This interface does not do ratelimiting */
		rate_ret = 0;
	} else if (rs->rs_flags & RS_IS_DEFF) {
		/* We need to find the real interface */
		struct ifnet *tifp;

		tifp = rt_find_real_interface(ifp, inp, NULL);
		if (tifp == NULL) {
			NET_EPOCH_EXIT(et);
			return (0);
		}
		ifp = tifp;
		goto use_next_interface;
	} else {
		/* Lets return the highest rate this guy has */
		rate_ret = rs->rs_rlt[rs->rs_highest_valid].rate;
	}
	NET_EPOCH_EXIT(et);
	return(rate_ret);
}

static eventhandler_tag rl_ifnet_departs;
static eventhandler_tag rl_ifnet_arrives;
static eventhandler_tag rl_shutdown_start;

static void
tcp_rs_init(void *st __unused)
{
	CK_LIST_INIT(&int_rs);
	rs_number_alive = 0;
	rs_number_dead = 0;
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
