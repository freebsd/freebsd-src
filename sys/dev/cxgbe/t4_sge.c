/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"

#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/sglist.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <sys/socketvar.h>
#include <sys/counter.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_vlan_var.h>
#include <net/if_vxlan.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <machine/in_cksum.h>
#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#ifdef DEV_NETMAP
#include <machine/bus.h>
#include <sys/selinfo.h>
#include <net/if_var.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#endif

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_msg.h"
#include "t4_l2t.h"
#include "t4_mp_ring.h"

#ifdef T4_PKT_TIMESTAMP
#define RX_COPY_THRESHOLD (MINCLSIZE - 8)
#else
#define RX_COPY_THRESHOLD MINCLSIZE
#endif

/* Internal mbuf flags stored in PH_loc.eight[1]. */
#define	MC_NOMAP		0x01
#define	MC_RAW_WR		0x02
#define	MC_TLS			0x04

/*
 * Ethernet frames are DMA'd at this byte offset into the freelist buffer.
 * 0-7 are valid values.
 */
static int fl_pktshift = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, fl_pktshift, CTLFLAG_RDTUN, &fl_pktshift, 0,
    "payload DMA offset in rx buffer (bytes)");

/*
 * Pad ethernet payload up to this boundary.
 * -1: driver should figure out a good value.
 *  0: disable padding.
 *  Any power of 2 from 32 to 4096 (both inclusive) is also a valid value.
 */
int fl_pad = -1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, fl_pad, CTLFLAG_RDTUN, &fl_pad, 0,
    "payload pad boundary (bytes)");

/*
 * Status page length.
 * -1: driver should figure out a good value.
 *  64 or 128 are the only other valid values.
 */
static int spg_len = -1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, spg_len, CTLFLAG_RDTUN, &spg_len, 0,
    "status page size (bytes)");

/*
 * Congestion drops.
 * -1: no congestion feedback (not recommended).
 *  0: backpressure the channel instead of dropping packets right away.
 *  1: no backpressure, drop packets for the congested queue immediately.
 */
static int cong_drop = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, cong_drop, CTLFLAG_RDTUN, &cong_drop, 0,
    "Congestion control for RX queues (0 = backpressure, 1 = drop");

/*
 * Deliver multiple frames in the same free list buffer if they fit.
 * -1: let the driver decide whether to enable buffer packing or not.
 *  0: disable buffer packing.
 *  1: enable buffer packing.
 */
static int buffer_packing = -1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, buffer_packing, CTLFLAG_RDTUN, &buffer_packing,
    0, "Enable buffer packing");

/*
 * Start next frame in a packed buffer at this boundary.
 * -1: driver should figure out a good value.
 * T4: driver will ignore this and use the same value as fl_pad above.
 * T5: 16, or a power of 2 from 64 to 4096 (both inclusive) is a valid value.
 */
static int fl_pack = -1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, fl_pack, CTLFLAG_RDTUN, &fl_pack, 0,
    "payload pack boundary (bytes)");

/*
 * Largest rx cluster size that the driver is allowed to allocate.
 */
static int largest_rx_cluster = MJUM16BYTES;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, largest_rx_cluster, CTLFLAG_RDTUN,
    &largest_rx_cluster, 0, "Largest rx cluster (bytes)");

/*
 * Size of cluster allocation that's most likely to succeed.  The driver will
 * fall back to this size if it fails to allocate clusters larger than this.
 */
static int safest_rx_cluster = PAGE_SIZE;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, safest_rx_cluster, CTLFLAG_RDTUN,
    &safest_rx_cluster, 0, "Safe rx cluster (bytes)");

#ifdef RATELIMIT
/*
 * Knob to control TCP timestamp rewriting, and the granularity of the tick used
 * for rewriting.  -1 and 0-3 are all valid values.
 * -1: hardware should leave the TCP timestamps alone.
 * 0: 1ms
 * 1: 100us
 * 2: 10us
 * 3: 1us
 */
static int tsclk = -1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, tsclk, CTLFLAG_RDTUN, &tsclk, 0,
    "Control TCP timestamp rewriting when using pacing");

static int eo_max_backlog = 1024 * 1024;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, eo_max_backlog, CTLFLAG_RDTUN, &eo_max_backlog,
    0, "Maximum backlog of ratelimited data per flow");
#endif

/*
 * The interrupt holdoff timers are multiplied by this value on T6+.
 * 1 and 3-17 (both inclusive) are legal values.
 */
static int tscale = 1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, tscale, CTLFLAG_RDTUN, &tscale, 0,
    "Interrupt holdoff timer scale on T6+");

/*
 * Number of LRO entries in the lro_ctrl structure per rx queue.
 */
static int lro_entries = TCP_LRO_ENTRIES;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, lro_entries, CTLFLAG_RDTUN, &lro_entries, 0,
    "Number of LRO entries per RX queue");

/*
 * This enables presorting of frames before they're fed into tcp_lro_rx.
 */
static int lro_mbufs = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, lro_mbufs, CTLFLAG_RDTUN, &lro_mbufs, 0,
    "Enable presorting of LRO frames");

static counter_u64_t pullups;
SYSCTL_COUNTER_U64(_hw_cxgbe, OID_AUTO, pullups, CTLFLAG_RD, &pullups,
    "Number of mbuf pullups performed");

static counter_u64_t defrags;
SYSCTL_COUNTER_U64(_hw_cxgbe, OID_AUTO, defrags, CTLFLAG_RD, &defrags,
    "Number of mbuf defrags performed");


static int service_iq(struct sge_iq *, int);
static int service_iq_fl(struct sge_iq *, int);
static struct mbuf *get_fl_payload(struct adapter *, struct sge_fl *, uint32_t);
static int eth_rx(struct adapter *, struct sge_rxq *, const struct iq_desc *,
    u_int);
static inline void init_iq(struct sge_iq *, struct adapter *, int, int, int);
static inline void init_fl(struct adapter *, struct sge_fl *, int, int, char *);
static inline void init_eq(struct adapter *, struct sge_eq *, int, int, uint8_t,
    uint16_t, char *);
static int alloc_ring(struct adapter *, size_t, bus_dma_tag_t *, bus_dmamap_t *,
    bus_addr_t *, void **);
static int free_ring(struct adapter *, bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    void *);
static int alloc_iq_fl(struct vi_info *, struct sge_iq *, struct sge_fl *,
    int, int);
static int free_iq_fl(struct vi_info *, struct sge_iq *, struct sge_fl *);
static void add_iq_sysctls(struct sysctl_ctx_list *, struct sysctl_oid *,
    struct sge_iq *);
static void add_fl_sysctls(struct adapter *, struct sysctl_ctx_list *,
    struct sysctl_oid *, struct sge_fl *);
static int alloc_fwq(struct adapter *);
static int free_fwq(struct adapter *);
static int alloc_ctrlq(struct adapter *, struct sge_wrq *, int,
    struct sysctl_oid *);
static int alloc_rxq(struct vi_info *, struct sge_rxq *, int, int,
    struct sysctl_oid *);
static int free_rxq(struct vi_info *, struct sge_rxq *);
#ifdef TCP_OFFLOAD
static int alloc_ofld_rxq(struct vi_info *, struct sge_ofld_rxq *, int, int,
    struct sysctl_oid *);
static int free_ofld_rxq(struct vi_info *, struct sge_ofld_rxq *);
#endif
#ifdef DEV_NETMAP
static int alloc_nm_rxq(struct vi_info *, struct sge_nm_rxq *, int, int,
    struct sysctl_oid *);
static int free_nm_rxq(struct vi_info *, struct sge_nm_rxq *);
static int alloc_nm_txq(struct vi_info *, struct sge_nm_txq *, int, int,
    struct sysctl_oid *);
static int free_nm_txq(struct vi_info *, struct sge_nm_txq *);
#endif
static int ctrl_eq_alloc(struct adapter *, struct sge_eq *);
static int eth_eq_alloc(struct adapter *, struct vi_info *, struct sge_eq *);
#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
static int ofld_eq_alloc(struct adapter *, struct vi_info *, struct sge_eq *);
#endif
static int alloc_eq(struct adapter *, struct vi_info *, struct sge_eq *);
static int free_eq(struct adapter *, struct sge_eq *);
static int alloc_wrq(struct adapter *, struct vi_info *, struct sge_wrq *,
    struct sysctl_oid *);
static int free_wrq(struct adapter *, struct sge_wrq *);
static int alloc_txq(struct vi_info *, struct sge_txq *, int,
    struct sysctl_oid *);
static int free_txq(struct vi_info *, struct sge_txq *);
static void oneseg_dma_callback(void *, bus_dma_segment_t *, int, int);
static inline void ring_fl_db(struct adapter *, struct sge_fl *);
static int refill_fl(struct adapter *, struct sge_fl *, int);
static void refill_sfl(void *);
static int alloc_fl_sdesc(struct sge_fl *);
static void free_fl_sdesc(struct adapter *, struct sge_fl *);
static int find_refill_source(struct adapter *, int, bool);
static void add_fl_to_sfl(struct adapter *, struct sge_fl *);

static inline void get_pkt_gl(struct mbuf *, struct sglist *);
static inline u_int txpkt_len16(u_int, const u_int);
static inline u_int txpkt_vm_len16(u_int, const u_int);
static inline void calculate_mbuf_len16(struct mbuf *, bool);
static inline u_int txpkts0_len16(u_int);
static inline u_int txpkts1_len16(void);
static u_int write_raw_wr(struct sge_txq *, void *, struct mbuf *, u_int);
static u_int write_txpkt_wr(struct adapter *, struct sge_txq *, struct mbuf *,
    u_int);
static u_int write_txpkt_vm_wr(struct adapter *, struct sge_txq *,
    struct mbuf *);
static int add_to_txpkts_vf(struct adapter *, struct sge_txq *, struct mbuf *,
    int, bool *);
static int add_to_txpkts_pf(struct adapter *, struct sge_txq *, struct mbuf *,
    int, bool *);
static u_int write_txpkts_wr(struct adapter *, struct sge_txq *);
static u_int write_txpkts_vm_wr(struct adapter *, struct sge_txq *);
static void write_gl_to_txd(struct sge_txq *, struct mbuf *, caddr_t *, int);
static inline void copy_to_txd(struct sge_eq *, caddr_t, caddr_t *, int);
static inline void ring_eq_db(struct adapter *, struct sge_eq *, u_int);
static inline uint16_t read_hw_cidx(struct sge_eq *);
static inline u_int reclaimable_tx_desc(struct sge_eq *);
static inline u_int total_available_tx_desc(struct sge_eq *);
static u_int reclaim_tx_descs(struct sge_txq *, u_int);
static void tx_reclaim(void *, int);
static __be64 get_flit(struct sglist_seg *, int, int);
static int handle_sge_egr_update(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
static int handle_fw_msg(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
static int t4_handle_wrerr_rpl(struct adapter *, const __be64 *);
static void wrq_tx_drain(void *, int);
static void drain_wrq_wr_list(struct adapter *, struct sge_wrq *);

static int sysctl_uint16(SYSCTL_HANDLER_ARGS);
static int sysctl_bufsizes(SYSCTL_HANDLER_ARGS);
#ifdef RATELIMIT
static inline u_int txpkt_eo_len16(u_int, u_int, u_int);
static int ethofld_fw4_ack(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
#endif

static counter_u64_t extfree_refs;
static counter_u64_t extfree_rels;

an_handler_t t4_an_handler;
fw_msg_handler_t t4_fw_msg_handler[NUM_FW6_TYPES];
cpl_handler_t t4_cpl_handler[NUM_CPL_CMDS];
cpl_handler_t set_tcb_rpl_handlers[NUM_CPL_COOKIES];
cpl_handler_t l2t_write_rpl_handlers[NUM_CPL_COOKIES];
cpl_handler_t act_open_rpl_handlers[NUM_CPL_COOKIES];
cpl_handler_t abort_rpl_rss_handlers[NUM_CPL_COOKIES];
cpl_handler_t fw4_ack_handlers[NUM_CPL_COOKIES];

void
t4_register_an_handler(an_handler_t h)
{
	uintptr_t *loc;

	MPASS(h == NULL || t4_an_handler == NULL);

	loc = (uintptr_t *)&t4_an_handler;
	atomic_store_rel_ptr(loc, (uintptr_t)h);
}

void
t4_register_fw_msg_handler(int type, fw_msg_handler_t h)
{
	uintptr_t *loc;

	MPASS(type < nitems(t4_fw_msg_handler));
	MPASS(h == NULL || t4_fw_msg_handler[type] == NULL);
	/*
	 * These are dispatched by the handler for FW{4|6}_CPL_MSG using the CPL
	 * handler dispatch table.  Reject any attempt to install a handler for
	 * this subtype.
	 */
	MPASS(type != FW_TYPE_RSSCPL);
	MPASS(type != FW6_TYPE_RSSCPL);

	loc = (uintptr_t *)&t4_fw_msg_handler[type];
	atomic_store_rel_ptr(loc, (uintptr_t)h);
}

void
t4_register_cpl_handler(int opcode, cpl_handler_t h)
{
	uintptr_t *loc;

	MPASS(opcode < nitems(t4_cpl_handler));
	MPASS(h == NULL || t4_cpl_handler[opcode] == NULL);

	loc = (uintptr_t *)&t4_cpl_handler[opcode];
	atomic_store_rel_ptr(loc, (uintptr_t)h);
}

static int
set_tcb_rpl_handler(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_set_tcb_rpl *cpl = (const void *)(rss + 1);
	u_int tid;
	int cookie;

	MPASS(m == NULL);

	tid = GET_TID(cpl);
	if (is_hpftid(iq->adapter, tid) || is_ftid(iq->adapter, tid)) {
		/*
		 * The return code for filter-write is put in the CPL cookie so
		 * we have to rely on the hardware tid (is_ftid) to determine
		 * that this is a response to a filter.
		 */
		cookie = CPL_COOKIE_FILTER;
	} else {
		cookie = G_COOKIE(cpl->cookie);
	}
	MPASS(cookie > CPL_COOKIE_RESERVED);
	MPASS(cookie < nitems(set_tcb_rpl_handlers));

	return (set_tcb_rpl_handlers[cookie](iq, rss, m));
}

static int
l2t_write_rpl_handler(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_l2t_write_rpl *rpl = (const void *)(rss + 1);
	unsigned int cookie;

	MPASS(m == NULL);

	cookie = GET_TID(rpl) & F_SYNC_WR ? CPL_COOKIE_TOM : CPL_COOKIE_FILTER;
	return (l2t_write_rpl_handlers[cookie](iq, rss, m));
}

static int
act_open_rpl_handler(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_act_open_rpl *cpl = (const void *)(rss + 1);
	u_int cookie = G_TID_COOKIE(G_AOPEN_ATID(be32toh(cpl->atid_status)));

	MPASS(m == NULL);
	MPASS(cookie != CPL_COOKIE_RESERVED);

	return (act_open_rpl_handlers[cookie](iq, rss, m));
}

static int
abort_rpl_rss_handler(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	u_int cookie;

	MPASS(m == NULL);
	if (is_hashfilter(sc))
		cookie = CPL_COOKIE_HASHFILTER;
	else
		cookie = CPL_COOKIE_TOM;

	return (abort_rpl_rss_handlers[cookie](iq, rss, m));
}

static int
fw4_ack_handler(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw4_ack *cpl = (const void *)(rss + 1);
	unsigned int tid = G_CPL_FW4_ACK_FLOWID(be32toh(OPCODE_TID(cpl)));
	u_int cookie;

	MPASS(m == NULL);
	if (is_etid(sc, tid))
		cookie = CPL_COOKIE_ETHOFLD;
	else
		cookie = CPL_COOKIE_TOM;

	return (fw4_ack_handlers[cookie](iq, rss, m));
}

static void
t4_init_shared_cpl_handlers(void)
{

	t4_register_cpl_handler(CPL_SET_TCB_RPL, set_tcb_rpl_handler);
	t4_register_cpl_handler(CPL_L2T_WRITE_RPL, l2t_write_rpl_handler);
	t4_register_cpl_handler(CPL_ACT_OPEN_RPL, act_open_rpl_handler);
	t4_register_cpl_handler(CPL_ABORT_RPL_RSS, abort_rpl_rss_handler);
	t4_register_cpl_handler(CPL_FW4_ACK, fw4_ack_handler);
}

void
t4_register_shared_cpl_handler(int opcode, cpl_handler_t h, int cookie)
{
	uintptr_t *loc;

	MPASS(opcode < nitems(t4_cpl_handler));
	MPASS(cookie > CPL_COOKIE_RESERVED);
	MPASS(cookie < NUM_CPL_COOKIES);
	MPASS(t4_cpl_handler[opcode] != NULL);

	switch (opcode) {
	case CPL_SET_TCB_RPL:
		loc = (uintptr_t *)&set_tcb_rpl_handlers[cookie];
		break;
	case CPL_L2T_WRITE_RPL:
		loc = (uintptr_t *)&l2t_write_rpl_handlers[cookie];
		break;
	case CPL_ACT_OPEN_RPL:
		loc = (uintptr_t *)&act_open_rpl_handlers[cookie];
		break;
	case CPL_ABORT_RPL_RSS:
		loc = (uintptr_t *)&abort_rpl_rss_handlers[cookie];
		break;
	case CPL_FW4_ACK:
		loc = (uintptr_t *)&fw4_ack_handlers[cookie];
		break;
	default:
		MPASS(0);
		return;
	}
	MPASS(h == NULL || *loc == (uintptr_t)NULL);
	atomic_store_rel_ptr(loc, (uintptr_t)h);
}

/*
 * Called on MOD_LOAD.  Validates and calculates the SGE tunables.
 */
void
t4_sge_modload(void)
{

	if (fl_pktshift < 0 || fl_pktshift > 7) {
		printf("Invalid hw.cxgbe.fl_pktshift value (%d),"
		    " using 0 instead.\n", fl_pktshift);
		fl_pktshift = 0;
	}

	if (spg_len != 64 && spg_len != 128) {
		int len;

#if defined(__i386__) || defined(__amd64__)
		len = cpu_clflush_line_size > 64 ? 128 : 64;
#else
		len = 64;
#endif
		if (spg_len != -1) {
			printf("Invalid hw.cxgbe.spg_len value (%d),"
			    " using %d instead.\n", spg_len, len);
		}
		spg_len = len;
	}

	if (cong_drop < -1 || cong_drop > 1) {
		printf("Invalid hw.cxgbe.cong_drop value (%d),"
		    " using 0 instead.\n", cong_drop);
		cong_drop = 0;
	}

	if (tscale != 1 && (tscale < 3 || tscale > 17)) {
		printf("Invalid hw.cxgbe.tscale value (%d),"
		    " using 1 instead.\n", tscale);
		tscale = 1;
	}

	if (largest_rx_cluster != MCLBYTES &&
#if MJUMPAGESIZE != MCLBYTES
	    largest_rx_cluster != MJUMPAGESIZE &&
#endif
	    largest_rx_cluster != MJUM9BYTES &&
	    largest_rx_cluster != MJUM16BYTES) {
		printf("Invalid hw.cxgbe.largest_rx_cluster value (%d),"
		    " using %d instead.\n", largest_rx_cluster, MJUM16BYTES);
		largest_rx_cluster = MJUM16BYTES;
	}

	if (safest_rx_cluster != MCLBYTES &&
#if MJUMPAGESIZE != MCLBYTES
	    safest_rx_cluster != MJUMPAGESIZE &&
#endif
	    safest_rx_cluster != MJUM9BYTES &&
	    safest_rx_cluster != MJUM16BYTES) {
		printf("Invalid hw.cxgbe.safest_rx_cluster value (%d),"
		    " using %d instead.\n", safest_rx_cluster, MJUMPAGESIZE);
		safest_rx_cluster = MJUMPAGESIZE;
	}

	extfree_refs = counter_u64_alloc(M_WAITOK);
	extfree_rels = counter_u64_alloc(M_WAITOK);
	pullups = counter_u64_alloc(M_WAITOK);
	defrags = counter_u64_alloc(M_WAITOK);
	counter_u64_zero(extfree_refs);
	counter_u64_zero(extfree_rels);
	counter_u64_zero(pullups);
	counter_u64_zero(defrags);

	t4_init_shared_cpl_handlers();
	t4_register_cpl_handler(CPL_FW4_MSG, handle_fw_msg);
	t4_register_cpl_handler(CPL_FW6_MSG, handle_fw_msg);
	t4_register_cpl_handler(CPL_SGE_EGR_UPDATE, handle_sge_egr_update);
#ifdef RATELIMIT
	t4_register_shared_cpl_handler(CPL_FW4_ACK, ethofld_fw4_ack,
	    CPL_COOKIE_ETHOFLD);
#endif
	t4_register_fw_msg_handler(FW6_TYPE_CMD_RPL, t4_handle_fw_rpl);
	t4_register_fw_msg_handler(FW6_TYPE_WRERR_RPL, t4_handle_wrerr_rpl);
}

void
t4_sge_modunload(void)
{

	counter_u64_free(extfree_refs);
	counter_u64_free(extfree_rels);
	counter_u64_free(pullups);
	counter_u64_free(defrags);
}

uint64_t
t4_sge_extfree_refs(void)
{
	uint64_t refs, rels;

	rels = counter_u64_fetch(extfree_rels);
	refs = counter_u64_fetch(extfree_refs);

	return (refs - rels);
}

/* max 4096 */
#define MAX_PACK_BOUNDARY 512

static inline void
setup_pad_and_pack_boundaries(struct adapter *sc)
{
	uint32_t v, m;
	int pad, pack, pad_shift;

	pad_shift = chip_id(sc) > CHELSIO_T5 ? X_T6_INGPADBOUNDARY_SHIFT :
	    X_INGPADBOUNDARY_SHIFT;
	pad = fl_pad;
	if (fl_pad < (1 << pad_shift) ||
	    fl_pad > (1 << (pad_shift + M_INGPADBOUNDARY)) ||
	    !powerof2(fl_pad)) {
		/*
		 * If there is any chance that we might use buffer packing and
		 * the chip is a T4, then pick 64 as the pad/pack boundary.  Set
		 * it to the minimum allowed in all other cases.
		 */
		pad = is_t4(sc) && buffer_packing ? 64 : 1 << pad_shift;

		/*
		 * For fl_pad = 0 we'll still write a reasonable value to the
		 * register but all the freelists will opt out of padding.
		 * We'll complain here only if the user tried to set it to a
		 * value greater than 0 that was invalid.
		 */
		if (fl_pad > 0) {
			device_printf(sc->dev, "Invalid hw.cxgbe.fl_pad value"
			    " (%d), using %d instead.\n", fl_pad, pad);
		}
	}
	m = V_INGPADBOUNDARY(M_INGPADBOUNDARY);
	v = V_INGPADBOUNDARY(ilog2(pad) - pad_shift);
	t4_set_reg_field(sc, A_SGE_CONTROL, m, v);

	if (is_t4(sc)) {
		if (fl_pack != -1 && fl_pack != pad) {
			/* Complain but carry on. */
			device_printf(sc->dev, "hw.cxgbe.fl_pack (%d) ignored,"
			    " using %d instead.\n", fl_pack, pad);
		}
		return;
	}

	pack = fl_pack;
	if (fl_pack < 16 || fl_pack == 32 || fl_pack > 4096 ||
	    !powerof2(fl_pack)) {
		if (sc->params.pci.mps > MAX_PACK_BOUNDARY)
			pack = MAX_PACK_BOUNDARY;
		else
			pack = max(sc->params.pci.mps, CACHE_LINE_SIZE);
		MPASS(powerof2(pack));
		if (pack < 16)
			pack = 16;
		if (pack == 32)
			pack = 64;
		if (pack > 4096)
			pack = 4096;
		if (fl_pack != -1) {
			device_printf(sc->dev, "Invalid hw.cxgbe.fl_pack value"
			    " (%d), using %d instead.\n", fl_pack, pack);
		}
	}
	m = V_INGPACKBOUNDARY(M_INGPACKBOUNDARY);
	if (pack == 16)
		v = V_INGPACKBOUNDARY(0);
	else
		v = V_INGPACKBOUNDARY(ilog2(pack) - 5);

	MPASS(!is_t4(sc));	/* T4 doesn't have SGE_CONTROL2 */
	t4_set_reg_field(sc, A_SGE_CONTROL2, m, v);
}

/*
 * adap->params.vpd.cclk must be set up before this is called.
 */
void
t4_tweak_chip_settings(struct adapter *sc)
{
	int i, reg;
	uint32_t v, m;
	int intr_timer[SGE_NTIMERS] = {1, 5, 10, 50, 100, 200};
	int timer_max = M_TIMERVALUE0 * 1000 / sc->params.vpd.cclk;
	int intr_pktcount[SGE_NCOUNTERS] = {1, 8, 16, 32}; /* 63 max */
	uint16_t indsz = min(RX_COPY_THRESHOLD - 1, M_INDICATESIZE);
	static int sw_buf_sizes[] = {
		MCLBYTES,
#if MJUMPAGESIZE != MCLBYTES
		MJUMPAGESIZE,
#endif
		MJUM9BYTES,
		MJUM16BYTES
	};

	KASSERT(sc->flags & MASTER_PF,
	    ("%s: trying to change chip settings when not master.", __func__));

	m = V_PKTSHIFT(M_PKTSHIFT) | F_RXPKTCPLMODE | F_EGRSTATUSPAGESIZE;
	v = V_PKTSHIFT(fl_pktshift) | F_RXPKTCPLMODE |
	    V_EGRSTATUSPAGESIZE(spg_len == 128);
	t4_set_reg_field(sc, A_SGE_CONTROL, m, v);

	setup_pad_and_pack_boundaries(sc);

	v = V_HOSTPAGESIZEPF0(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF1(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF2(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF3(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF4(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF5(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF6(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF7(PAGE_SHIFT - 10);
	t4_write_reg(sc, A_SGE_HOST_PAGE_SIZE, v);

	t4_write_reg(sc, A_SGE_FL_BUFFER_SIZE0, 4096);
	t4_write_reg(sc, A_SGE_FL_BUFFER_SIZE1, 65536);
	reg = A_SGE_FL_BUFFER_SIZE2;
	for (i = 0; i < nitems(sw_buf_sizes); i++) {
		MPASS(reg <= A_SGE_FL_BUFFER_SIZE15);
		t4_write_reg(sc, reg, sw_buf_sizes[i]);
		reg += 4;
		MPASS(reg <= A_SGE_FL_BUFFER_SIZE15);
		t4_write_reg(sc, reg, sw_buf_sizes[i] - CL_METADATA_SIZE);
		reg += 4;
	}

	v = V_THRESHOLD_0(intr_pktcount[0]) | V_THRESHOLD_1(intr_pktcount[1]) |
	    V_THRESHOLD_2(intr_pktcount[2]) | V_THRESHOLD_3(intr_pktcount[3]);
	t4_write_reg(sc, A_SGE_INGRESS_RX_THRESHOLD, v);

	KASSERT(intr_timer[0] <= timer_max,
	    ("%s: not a single usable timer (%d, %d)", __func__, intr_timer[0],
	    timer_max));
	for (i = 1; i < nitems(intr_timer); i++) {
		KASSERT(intr_timer[i] >= intr_timer[i - 1],
		    ("%s: timers not listed in increasing order (%d)",
		    __func__, i));

		while (intr_timer[i] > timer_max) {
			if (i == nitems(intr_timer) - 1) {
				intr_timer[i] = timer_max;
				break;
			}
			intr_timer[i] += intr_timer[i - 1];
			intr_timer[i] /= 2;
		}
	}

	v = V_TIMERVALUE0(us_to_core_ticks(sc, intr_timer[0])) |
	    V_TIMERVALUE1(us_to_core_ticks(sc, intr_timer[1]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_0_AND_1, v);
	v = V_TIMERVALUE2(us_to_core_ticks(sc, intr_timer[2])) |
	    V_TIMERVALUE3(us_to_core_ticks(sc, intr_timer[3]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_2_AND_3, v);
	v = V_TIMERVALUE4(us_to_core_ticks(sc, intr_timer[4])) |
	    V_TIMERVALUE5(us_to_core_ticks(sc, intr_timer[5]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_4_AND_5, v);

	if (chip_id(sc) >= CHELSIO_T6) {
		m = V_TSCALE(M_TSCALE);
		if (tscale == 1)
			v = 0;
		else
			v = V_TSCALE(tscale - 2);
		t4_set_reg_field(sc, A_SGE_ITP_CONTROL, m, v);

		if (sc->debug_flags & DF_DISABLE_TCB_CACHE) {
			m = V_RDTHRESHOLD(M_RDTHRESHOLD) | F_WRTHRTHRESHEN |
			    V_WRTHRTHRESH(M_WRTHRTHRESH);
			t4_tp_pio_read(sc, &v, 1, A_TP_CMM_CONFIG, 1);
			v &= ~m;
			v |= V_RDTHRESHOLD(1) | F_WRTHRTHRESHEN |
			    V_WRTHRTHRESH(16);
			t4_tp_pio_write(sc, &v, 1, A_TP_CMM_CONFIG, 1);
		}
	}

	/* 4K, 16K, 64K, 256K DDP "page sizes" for TDDP */
	v = V_HPZ0(0) | V_HPZ1(2) | V_HPZ2(4) | V_HPZ3(6);
	t4_write_reg(sc, A_ULP_RX_TDDP_PSZ, v);

	/*
	 * 4K, 8K, 16K, 64K DDP "page sizes" for iSCSI DDP.  These have been
	 * chosen with MAXPHYS = 128K in mind.  The largest DDP buffer that we
	 * may have to deal with is MAXPHYS + 1 page.
	 */
	v = V_HPZ0(0) | V_HPZ1(1) | V_HPZ2(2) | V_HPZ3(4);
	t4_write_reg(sc, A_ULP_RX_ISCSI_PSZ, v);

	/* We use multiple DDP page sizes both in plain-TOE and ISCSI modes. */
	m = v = F_TDDPTAGTCB | F_ISCSITAGTCB;
	t4_set_reg_field(sc, A_ULP_RX_CTL, m, v);

	m = V_INDICATESIZE(M_INDICATESIZE) | F_REARMDDPOFFSET |
	    F_RESETDDPOFFSET;
	v = V_INDICATESIZE(indsz) | F_REARMDDPOFFSET | F_RESETDDPOFFSET;
	t4_set_reg_field(sc, A_TP_PARA_REG5, m, v);
}

/*
 * SGE wants the buffer to be at least 64B and then a multiple of 16.  Its
 * address mut be 16B aligned.  If padding is in use the buffer's start and end
 * need to be aligned to the pad boundary as well.  We'll just make sure that
 * the size is a multiple of the pad boundary here, it is up to the buffer
 * allocation code to make sure the start of the buffer is aligned.
 */
static inline int
hwsz_ok(struct adapter *sc, int hwsz)
{
	int mask = fl_pad ? sc->params.sge.pad_boundary - 1 : 16 - 1;

	return (hwsz >= 64 && (hwsz & mask) == 0);
}

/*
 * XXX: driver really should be able to deal with unexpected settings.
 */
int
t4_read_chip_settings(struct adapter *sc)
{
	struct sge *s = &sc->sge;
	struct sge_params *sp = &sc->params.sge;
	int i, j, n, rc = 0;
	uint32_t m, v, r;
	uint16_t indsz = min(RX_COPY_THRESHOLD - 1, M_INDICATESIZE);
	static int sw_buf_sizes[] = {	/* Sorted by size */
		MCLBYTES,
#if MJUMPAGESIZE != MCLBYTES
		MJUMPAGESIZE,
#endif
		MJUM9BYTES,
		MJUM16BYTES
	};
	struct rx_buf_info *rxb;

	m = F_RXPKTCPLMODE;
	v = F_RXPKTCPLMODE;
	r = sc->params.sge.sge_control;
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid SGE_CONTROL(0x%x)\n", r);
		rc = EINVAL;
	}

	/*
	 * If this changes then every single use of PAGE_SHIFT in the driver
	 * needs to be carefully reviewed for PAGE_SHIFT vs sp->page_shift.
	 */
	if (sp->page_shift != PAGE_SHIFT) {
		device_printf(sc->dev, "invalid SGE_HOST_PAGE_SIZE(0x%x)\n", r);
		rc = EINVAL;
	}

	s->safe_zidx = -1;
	rxb = &s->rx_buf_info[0];
	for (i = 0; i < SW_ZONE_SIZES; i++, rxb++) {
		rxb->size1 = sw_buf_sizes[i];
		rxb->zone = m_getzone(rxb->size1);
		rxb->type = m_gettype(rxb->size1);
		rxb->size2 = 0;
		rxb->hwidx1 = -1;
		rxb->hwidx2 = -1;
		for (j = 0; j < SGE_FLBUF_SIZES; j++) {
			int hwsize = sp->sge_fl_buffer_size[j];

			if (!hwsz_ok(sc, hwsize))
				continue;

			/* hwidx for size1 */
			if (rxb->hwidx1 == -1 && rxb->size1 == hwsize)
				rxb->hwidx1 = j;

			/* hwidx for size2 (buffer packing) */
			if (rxb->size1 - CL_METADATA_SIZE < hwsize)
				continue;
			n = rxb->size1 - hwsize - CL_METADATA_SIZE;
			if (n == 0) {
				rxb->hwidx2 = j;
				rxb->size2 = hwsize;
				break;	/* stop looking */
			}
			if (rxb->hwidx2 != -1) {
				if (n < sp->sge_fl_buffer_size[rxb->hwidx2] -
				    hwsize - CL_METADATA_SIZE) {
					rxb->hwidx2 = j;
					rxb->size2 = hwsize;
				}
			} else if (n <= 2 * CL_METADATA_SIZE) {
				rxb->hwidx2 = j;
				rxb->size2 = hwsize;
			}
		}
		if (rxb->hwidx2 != -1)
			sc->flags |= BUF_PACKING_OK;
		if (s->safe_zidx == -1 && rxb->size1 == safest_rx_cluster)
			s->safe_zidx = i;
	}

	if (sc->flags & IS_VF)
		return (0);

	v = V_HPZ0(0) | V_HPZ1(2) | V_HPZ2(4) | V_HPZ3(6);
	r = t4_read_reg(sc, A_ULP_RX_TDDP_PSZ);
	if (r != v) {
		device_printf(sc->dev, "invalid ULP_RX_TDDP_PSZ(0x%x)\n", r);
		rc = EINVAL;
	}

	m = v = F_TDDPTAGTCB;
	r = t4_read_reg(sc, A_ULP_RX_CTL);
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid ULP_RX_CTL(0x%x)\n", r);
		rc = EINVAL;
	}

	m = V_INDICATESIZE(M_INDICATESIZE) | F_REARMDDPOFFSET |
	    F_RESETDDPOFFSET;
	v = V_INDICATESIZE(indsz) | F_REARMDDPOFFSET | F_RESETDDPOFFSET;
	r = t4_read_reg(sc, A_TP_PARA_REG5);
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid TP_PARA_REG5(0x%x)\n", r);
		rc = EINVAL;
	}

	t4_init_tp_params(sc, 1);

	t4_read_mtu_tbl(sc, sc->params.mtus, NULL);
	t4_load_mtus(sc, sc->params.mtus, sc->params.a_wnd, sc->params.b_wnd);

	return (rc);
}

int
t4_create_dma_tag(struct adapter *sc)
{
	int rc;

	rc = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BUS_SPACE_MAXSIZE,
	    BUS_SPACE_UNRESTRICTED, BUS_SPACE_MAXSIZE, BUS_DMA_ALLOCNOW, NULL,
	    NULL, &sc->dmat);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create main DMA tag: %d\n", rc);
	}

	return (rc);
}

void
t4_sge_sysctls(struct adapter *sc, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *children)
{
	struct sge_params *sp = &sc->params.sge;

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "buffer_sizes",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    sysctl_bufsizes, "A", "freelist buffer sizes");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pktshift", CTLFLAG_RD,
	    NULL, sp->fl_pktshift, "payload DMA offset in rx buffer (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pad", CTLFLAG_RD,
	    NULL, sp->pad_boundary, "payload pad boundary (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "spg_len", CTLFLAG_RD,
	    NULL, sp->spg_len, "status page size (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "cong_drop", CTLFLAG_RD,
	    NULL, cong_drop, "congestion drop setting");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pack", CTLFLAG_RD,
	    NULL, sp->pack_boundary, "payload pack boundary (bytes)");
}

int
t4_destroy_dma_tag(struct adapter *sc)
{
	if (sc->dmat)
		bus_dma_tag_destroy(sc->dmat);

	return (0);
}

/*
 * Allocate and initialize the firmware event queue, control queues, and special
 * purpose rx queues owned by the adapter.
 *
 * Returns errno on failure.  Resources allocated up to that point may still be
 * allocated.  Caller is responsible for cleanup in case this function fails.
 */
int
t4_setup_adapter_queues(struct adapter *sc)
{
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;
	int rc, i;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	sysctl_ctx_init(&sc->ctx);
	sc->flags |= ADAP_SYSCTL_CTX;

	/*
	 * Firmware event queue
	 */
	rc = alloc_fwq(sc);
	if (rc != 0)
		return (rc);

	/*
	 * That's all for the VF driver.
	 */
	if (sc->flags & IS_VF)
		return (rc);

	oid = device_get_sysctl_tree(sc->dev);
	children = SYSCTL_CHILDREN(oid);

	/*
	 * XXX: General purpose rx queues, one per port.
	 */

	/*
	 * Control queues, one per port.
	 */
	oid = SYSCTL_ADD_NODE(&sc->ctx, children, OID_AUTO, "ctrlq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "control queues");
	for_each_port(sc, i) {
		struct sge_wrq *ctrlq = &sc->sge.ctrlq[i];

		rc = alloc_ctrlq(sc, ctrlq, i, oid);
		if (rc != 0)
			return (rc);
	}

	return (rc);
}

/*
 * Idempotent
 */
int
t4_teardown_adapter_queues(struct adapter *sc)
{
	int i;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	/* Do this before freeing the queue */
	if (sc->flags & ADAP_SYSCTL_CTX) {
		sysctl_ctx_free(&sc->ctx);
		sc->flags &= ~ADAP_SYSCTL_CTX;
	}

	if (!(sc->flags & IS_VF)) {
		for_each_port(sc, i)
			free_wrq(sc, &sc->sge.ctrlq[i]);
	}
	free_fwq(sc);

	return (0);
}

/* Maximum payload that could arrive with a single iq descriptor. */
static inline int
max_rx_payload(struct adapter *sc, struct ifnet *ifp, const bool ofld)
{
	int maxp;

	/* large enough even when hw VLAN extraction is disabled */
	maxp = sc->params.sge.fl_pktshift + ETHER_HDR_LEN +
	    ETHER_VLAN_ENCAP_LEN + ifp->if_mtu;
	if (ofld && sc->tt.tls && sc->cryptocaps & FW_CAPS_CONFIG_TLSKEYS &&
	    maxp < sc->params.tp.max_rx_pdu)
		maxp = sc->params.tp.max_rx_pdu;
	return (maxp);
}

int
t4_setup_vi_queues(struct vi_info *vi)
{
	int rc = 0, i, intr_idx, iqidx;
	struct sge_rxq *rxq;
	struct sge_txq *txq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
#endif
#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
	struct sge_wrq *ofld_txq;
#endif
#ifdef DEV_NETMAP
	int saved_idx;
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
#endif
	char name[16];
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = vi->ifp;
	struct sysctl_oid *oid = device_get_sysctl_tree(vi->dev);
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);
	int maxp;

	/* Interrupt vector to start from (when using multiple vectors) */
	intr_idx = vi->first_intr;

#ifdef DEV_NETMAP
	saved_idx = intr_idx;
	if (ifp->if_capabilities & IFCAP_NETMAP) {

		/* netmap is supported with direct interrupts only. */
		MPASS(!forwarding_intr_to_fwq(sc));

		/*
		 * We don't have buffers to back the netmap rx queues
		 * right now so we create the queues in a way that
		 * doesn't set off any congestion signal in the chip.
		 */
		oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "nm_rxq",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queues");
		for_each_nm_rxq(vi, i, nm_rxq) {
			rc = alloc_nm_rxq(vi, nm_rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			intr_idx++;
		}

		oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "nm_txq",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "tx queues");
		for_each_nm_txq(vi, i, nm_txq) {
			iqidx = vi->first_nm_rxq + (i % vi->nnmrxq);
			rc = alloc_nm_txq(vi, nm_txq, iqidx, i, oid);
			if (rc != 0)
				goto done;
		}
	}

	/* Normal rx queues and netmap rx queues share the same interrupts. */
	intr_idx = saved_idx;
#endif

	/*
	 * Allocate rx queues first because a default iqid is required when
	 * creating a tx queue.
	 */
	maxp = max_rx_payload(sc, ifp, false);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "rxq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queues");
	for_each_rxq(vi, i, rxq) {

		init_iq(&rxq->iq, sc, vi->tmr_idx, vi->pktc_idx, vi->qsize_rxq);

		snprintf(name, sizeof(name), "%s rxq%d-fl",
		    device_get_nameunit(vi->dev), i);
		init_fl(sc, &rxq->fl, vi->qsize_rxq / 8, maxp, name);

		rc = alloc_rxq(vi, rxq,
		    forwarding_intr_to_fwq(sc) ? -1 : intr_idx, i, oid);
		if (rc != 0)
			goto done;
		intr_idx++;
	}
#ifdef DEV_NETMAP
	if (ifp->if_capabilities & IFCAP_NETMAP)
		intr_idx = saved_idx + max(vi->nrxq, vi->nnmrxq);
#endif
#ifdef TCP_OFFLOAD
	maxp = max_rx_payload(sc, ifp, true);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "ofld_rxq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queues for offloaded TCP connections");
	for_each_ofld_rxq(vi, i, ofld_rxq) {

		init_iq(&ofld_rxq->iq, sc, vi->ofld_tmr_idx, vi->ofld_pktc_idx,
		    vi->qsize_rxq);

		snprintf(name, sizeof(name), "%s ofld_rxq%d-fl",
		    device_get_nameunit(vi->dev), i);
		init_fl(sc, &ofld_rxq->fl, vi->qsize_rxq / 8, maxp, name);

		rc = alloc_ofld_rxq(vi, ofld_rxq,
		    forwarding_intr_to_fwq(sc) ? -1 : intr_idx, i, oid);
		if (rc != 0)
			goto done;
		intr_idx++;
	}
#endif

	/*
	 * Now the tx queues.
	 */
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "txq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "tx queues");
	for_each_txq(vi, i, txq) {
		iqidx = vi->first_rxq + (i % vi->nrxq);
		snprintf(name, sizeof(name), "%s txq%d",
		    device_get_nameunit(vi->dev), i);
		init_eq(sc, &txq->eq, EQ_ETH, vi->qsize_txq, pi->tx_chan,
		    sc->sge.rxq[iqidx].iq.cntxt_id, name);

		rc = alloc_txq(vi, txq, i, oid);
		if (rc != 0)
			goto done;
	}
#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, "ofld_txq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "tx queues for TOE/ETHOFLD");
	for_each_ofld_txq(vi, i, ofld_txq) {
		struct sysctl_oid *oid2;

		snprintf(name, sizeof(name), "%s ofld_txq%d",
		    device_get_nameunit(vi->dev), i);
		if (vi->nofldrxq > 0) {
			iqidx = vi->first_ofld_rxq + (i % vi->nofldrxq);
			init_eq(sc, &ofld_txq->eq, EQ_OFLD, vi->qsize_txq,
			    pi->tx_chan, sc->sge.ofld_rxq[iqidx].iq.cntxt_id,
			    name);
		} else {
			iqidx = vi->first_rxq + (i % vi->nrxq);
			init_eq(sc, &ofld_txq->eq, EQ_OFLD, vi->qsize_txq,
			    pi->tx_chan, sc->sge.rxq[iqidx].iq.cntxt_id, name);
		}

		snprintf(name, sizeof(name), "%d", i);
		oid2 = SYSCTL_ADD_NODE(&vi->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		    name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "offload tx queue");

		rc = alloc_wrq(sc, vi, ofld_txq, oid2);
		if (rc != 0)
			goto done;
	}
#endif
done:
	if (rc)
		t4_teardown_vi_queues(vi);

	return (rc);
}

/*
 * Idempotent
 */
int
t4_teardown_vi_queues(struct vi_info *vi)
{
	int i;
	struct sge_rxq *rxq;
	struct sge_txq *txq;
#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct sge_wrq *ofld_txq;
#endif
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
#endif
#ifdef DEV_NETMAP
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
#endif

	/* Do this before freeing the queues */
	if (vi->flags & VI_SYSCTL_CTX) {
		sysctl_ctx_free(&vi->ctx);
		vi->flags &= ~VI_SYSCTL_CTX;
	}

#ifdef DEV_NETMAP
	if (vi->ifp->if_capabilities & IFCAP_NETMAP) {
		for_each_nm_txq(vi, i, nm_txq) {
			free_nm_txq(vi, nm_txq);
		}

		for_each_nm_rxq(vi, i, nm_rxq) {
			free_nm_rxq(vi, nm_rxq);
		}
	}
#endif

	/*
	 * Take down all the tx queues first, as they reference the rx queues
	 * (for egress updates, etc.).
	 */

	for_each_txq(vi, i, txq) {
		free_txq(vi, txq);
	}
#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
	for_each_ofld_txq(vi, i, ofld_txq) {
		free_wrq(sc, ofld_txq);
	}
#endif

	/*
	 * Then take down the rx queues.
	 */

	for_each_rxq(vi, i, rxq) {
		free_rxq(vi, rxq);
	}
#ifdef TCP_OFFLOAD
	for_each_ofld_rxq(vi, i, ofld_rxq) {
		free_ofld_rxq(vi, ofld_rxq);
	}
#endif

	return (0);
}

/*
 * Interrupt handler when the driver is using only 1 interrupt.  This is a very
 * unusual scenario.
 *
 * a) Deals with errors, if any.
 * b) Services firmware event queue, which is taking interrupts for all other
 *    queues.
 */
void
t4_intr_all(void *arg)
{
	struct adapter *sc = arg;
	struct sge_iq *fwq = &sc->sge.fwq;

	MPASS(sc->intr_count == 1);

	if (sc->intr_type == INTR_INTX)
		t4_write_reg(sc, MYPF_REG(A_PCIE_PF_CLI), 0);

	t4_intr_err(arg);
	t4_intr_evt(fwq);
}

/*
 * Interrupt handler for errors (installed directly when multiple interrupts are
 * being used, or called by t4_intr_all).
 */
void
t4_intr_err(void *arg)
{
	struct adapter *sc = arg;
	uint32_t v;
	const bool verbose = (sc->debug_flags & DF_VERBOSE_SLOWINTR) != 0;

	if (sc->flags & ADAP_ERR)
		return;

	v = t4_read_reg(sc, MYPF_REG(A_PL_PF_INT_CAUSE));
	if (v & F_PFSW) {
		sc->swintr++;
		t4_write_reg(sc, MYPF_REG(A_PL_PF_INT_CAUSE), v);
	}

	t4_slow_intr_handler(sc, verbose);
}

/*
 * Interrupt handler for iq-only queues.  The firmware event queue is the only
 * such queue right now.
 */
void
t4_intr_evt(void *arg)
{
	struct sge_iq *iq = arg;

	if (atomic_cmpset_int(&iq->state, IQS_IDLE, IQS_BUSY)) {
		service_iq(iq, 0);
		(void) atomic_cmpset_int(&iq->state, IQS_BUSY, IQS_IDLE);
	}
}

/*
 * Interrupt handler for iq+fl queues.
 */
void
t4_intr(void *arg)
{
	struct sge_iq *iq = arg;

	if (atomic_cmpset_int(&iq->state, IQS_IDLE, IQS_BUSY)) {
		service_iq_fl(iq, 0);
		(void) atomic_cmpset_int(&iq->state, IQS_BUSY, IQS_IDLE);
	}
}

#ifdef DEV_NETMAP
/*
 * Interrupt handler for netmap rx queues.
 */
void
t4_nm_intr(void *arg)
{
	struct sge_nm_rxq *nm_rxq = arg;

	if (atomic_cmpset_int(&nm_rxq->nm_state, NM_ON, NM_BUSY)) {
		service_nm_rxq(nm_rxq);
		(void) atomic_cmpset_int(&nm_rxq->nm_state, NM_BUSY, NM_ON);
	}
}

/*
 * Interrupt handler for vectors shared between NIC and netmap rx queues.
 */
void
t4_vi_intr(void *arg)
{
	struct irq *irq = arg;

	MPASS(irq->nm_rxq != NULL);
	t4_nm_intr(irq->nm_rxq);

	MPASS(irq->rxq != NULL);
	t4_intr(irq->rxq);
}
#endif

/*
 * Deals with interrupts on an iq-only (no freelist) queue.
 */
static int
service_iq(struct sge_iq *iq, int budget)
{
	struct sge_iq *q;
	struct adapter *sc = iq->adapter;
	struct iq_desc *d = &iq->desc[iq->cidx];
	int ndescs = 0, limit;
	int rsp_type;
	uint32_t lq;
	STAILQ_HEAD(, sge_iq) iql = STAILQ_HEAD_INITIALIZER(iql);

	KASSERT(iq->state == IQS_BUSY, ("%s: iq %p not BUSY", __func__, iq));
	KASSERT((iq->flags & IQ_HAS_FL) == 0,
	    ("%s: called for iq %p with fl (iq->flags 0x%x)", __func__, iq,
	    iq->flags));
	MPASS((iq->flags & IQ_ADJ_CREDIT) == 0);
	MPASS((iq->flags & IQ_LRO_ENABLED) == 0);

	limit = budget ? budget : iq->qsize / 16;

	/*
	 * We always come back and check the descriptor ring for new indirect
	 * interrupts and other responses after running a single handler.
	 */
	for (;;) {
		while ((d->rsp.u.type_gen & F_RSPD_GEN) == iq->gen) {

			rmb();

			rsp_type = G_RSPD_TYPE(d->rsp.u.type_gen);
			lq = be32toh(d->rsp.pldbuflen_qid);

			switch (rsp_type) {
			case X_RSPD_TYPE_FLBUF:
				panic("%s: data for an iq (%p) with no freelist",
				    __func__, iq);

				/* NOTREACHED */

			case X_RSPD_TYPE_CPL:
				KASSERT(d->rss.opcode < NUM_CPL_CMDS,
				    ("%s: bad opcode %02x.", __func__,
				    d->rss.opcode));
				t4_cpl_handler[d->rss.opcode](iq, &d->rss, NULL);
				break;

			case X_RSPD_TYPE_INTR:
				/*
				 * There are 1K interrupt-capable queues (qids 0
				 * through 1023).  A response type indicating a
				 * forwarded interrupt with a qid >= 1K is an
				 * iWARP async notification.
				 */
				if (__predict_true(lq >= 1024)) {
					t4_an_handler(iq, &d->rsp);
					break;
				}

				q = sc->sge.iqmap[lq - sc->sge.iq_start -
				    sc->sge.iq_base];
				if (atomic_cmpset_int(&q->state, IQS_IDLE,
				    IQS_BUSY)) {
					if (service_iq_fl(q, q->qsize / 16) == 0) {
						(void) atomic_cmpset_int(&q->state,
						    IQS_BUSY, IQS_IDLE);
					} else {
						STAILQ_INSERT_TAIL(&iql, q,
						    link);
					}
				}
				break;

			default:
				KASSERT(0,
				    ("%s: illegal response type %d on iq %p",
				    __func__, rsp_type, iq));
				log(LOG_ERR,
				    "%s: illegal response type %d on iq %p",
				    device_get_nameunit(sc->dev), rsp_type, iq);
				break;
			}

			d++;
			if (__predict_false(++iq->cidx == iq->sidx)) {
				iq->cidx = 0;
				iq->gen ^= F_RSPD_GEN;
				d = &iq->desc[0];
			}
			if (__predict_false(++ndescs == limit)) {
				t4_write_reg(sc, sc->sge_gts_reg,
				    V_CIDXINC(ndescs) |
				    V_INGRESSQID(iq->cntxt_id) |
				    V_SEINTARM(V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX)));
				ndescs = 0;

				if (budget) {
					return (EINPROGRESS);
				}
			}
		}

		if (STAILQ_EMPTY(&iql))
			break;

		/*
		 * Process the head only, and send it to the back of the list if
		 * it's still not done.
		 */
		q = STAILQ_FIRST(&iql);
		STAILQ_REMOVE_HEAD(&iql, link);
		if (service_iq_fl(q, q->qsize / 8) == 0)
			(void) atomic_cmpset_int(&q->state, IQS_BUSY, IQS_IDLE);
		else
			STAILQ_INSERT_TAIL(&iql, q, link);
	}

	t4_write_reg(sc, sc->sge_gts_reg, V_CIDXINC(ndescs) |
	    V_INGRESSQID((u32)iq->cntxt_id) | V_SEINTARM(iq->intr_params));

	return (0);
}

static inline int
sort_before_lro(struct lro_ctrl *lro)
{

	return (lro->lro_mbuf_max != 0);
}

static inline uint64_t
last_flit_to_ns(struct adapter *sc, uint64_t lf)
{
	uint64_t n = be64toh(lf) & 0xfffffffffffffff;	/* 60b, not 64b. */

	if (n > UINT64_MAX / 1000000)
		return (n / sc->params.vpd.cclk * 1000000);
	else
		return (n * 1000000 / sc->params.vpd.cclk);
}

static inline void
move_to_next_rxbuf(struct sge_fl *fl)
{

	fl->rx_offset = 0;
	if (__predict_false((++fl->cidx & 7) == 0)) {
		uint16_t cidx = fl->cidx >> 3;

		if (__predict_false(cidx == fl->sidx))
			fl->cidx = cidx = 0;
		fl->hw_cidx = cidx;
	}
}

/*
 * Deals with interrupts on an iq+fl queue.
 */
static int
service_iq_fl(struct sge_iq *iq, int budget)
{
	struct sge_rxq *rxq = iq_to_rxq(iq);
	struct sge_fl *fl;
	struct adapter *sc = iq->adapter;
	struct iq_desc *d = &iq->desc[iq->cidx];
	int ndescs, limit;
	int rsp_type, starved;
	uint32_t lq;
	uint16_t fl_hw_cidx;
	struct mbuf *m0;
#if defined(INET) || defined(INET6)
	const struct timeval lro_timeout = {0, sc->lro_timeout};
	struct lro_ctrl *lro = &rxq->lro;
#endif

	KASSERT(iq->state == IQS_BUSY, ("%s: iq %p not BUSY", __func__, iq));
	MPASS(iq->flags & IQ_HAS_FL);

	ndescs = 0;
#if defined(INET) || defined(INET6)
	if (iq->flags & IQ_ADJ_CREDIT) {
		MPASS(sort_before_lro(lro));
		iq->flags &= ~IQ_ADJ_CREDIT;
		if ((d->rsp.u.type_gen & F_RSPD_GEN) != iq->gen) {
			tcp_lro_flush_all(lro);
			t4_write_reg(sc, sc->sge_gts_reg, V_CIDXINC(1) |
			    V_INGRESSQID((u32)iq->cntxt_id) |
			    V_SEINTARM(iq->intr_params));
			return (0);
		}
		ndescs = 1;
	}
#else
	MPASS((iq->flags & IQ_ADJ_CREDIT) == 0);
#endif

	limit = budget ? budget : iq->qsize / 16;
	fl = &rxq->fl;
	fl_hw_cidx = fl->hw_cidx;	/* stable snapshot */
	while ((d->rsp.u.type_gen & F_RSPD_GEN) == iq->gen) {

		rmb();

		m0 = NULL;
		rsp_type = G_RSPD_TYPE(d->rsp.u.type_gen);
		lq = be32toh(d->rsp.pldbuflen_qid);

		switch (rsp_type) {
		case X_RSPD_TYPE_FLBUF:
			if (lq & F_RSPD_NEWBUF) {
				if (fl->rx_offset > 0)
					move_to_next_rxbuf(fl);
				lq = G_RSPD_LEN(lq);
			}
			if (IDXDIFF(fl->hw_cidx, fl_hw_cidx, fl->sidx) > 4) {
				FL_LOCK(fl);
				refill_fl(sc, fl, 64);
				FL_UNLOCK(fl);
				fl_hw_cidx = fl->hw_cidx;
			}

			if (d->rss.opcode == CPL_RX_PKT) {
				if (__predict_true(eth_rx(sc, rxq, d, lq) == 0))
					break;
				goto out;
			}
			m0 = get_fl_payload(sc, fl, lq);
			if (__predict_false(m0 == NULL))
				goto out;

			/* fall through */

		case X_RSPD_TYPE_CPL:
			KASSERT(d->rss.opcode < NUM_CPL_CMDS,
			    ("%s: bad opcode %02x.", __func__, d->rss.opcode));
			t4_cpl_handler[d->rss.opcode](iq, &d->rss, m0);
			break;

		case X_RSPD_TYPE_INTR:

			/*
			 * There are 1K interrupt-capable queues (qids 0
			 * through 1023).  A response type indicating a
			 * forwarded interrupt with a qid >= 1K is an
			 * iWARP async notification.  That is the only
			 * acceptable indirect interrupt on this queue.
			 */
			if (__predict_false(lq < 1024)) {
				panic("%s: indirect interrupt on iq_fl %p "
				    "with qid %u", __func__, iq, lq);
			}

			t4_an_handler(iq, &d->rsp);
			break;

		default:
			KASSERT(0, ("%s: illegal response type %d on iq %p",
			    __func__, rsp_type, iq));
			log(LOG_ERR, "%s: illegal response type %d on iq %p",
			    device_get_nameunit(sc->dev), rsp_type, iq);
			break;
		}

		d++;
		if (__predict_false(++iq->cidx == iq->sidx)) {
			iq->cidx = 0;
			iq->gen ^= F_RSPD_GEN;
			d = &iq->desc[0];
		}
		if (__predict_false(++ndescs == limit)) {
			t4_write_reg(sc, sc->sge_gts_reg, V_CIDXINC(ndescs) |
			    V_INGRESSQID(iq->cntxt_id) |
			    V_SEINTARM(V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX)));

#if defined(INET) || defined(INET6)
			if (iq->flags & IQ_LRO_ENABLED &&
			    !sort_before_lro(lro) &&
			    sc->lro_timeout != 0) {
				tcp_lro_flush_inactive(lro, &lro_timeout);
			}
#endif
			if (budget)
				return (EINPROGRESS);
			ndescs = 0;
		}
	}
out:
#if defined(INET) || defined(INET6)
	if (iq->flags & IQ_LRO_ENABLED) {
		if (ndescs > 0 && lro->lro_mbuf_count > 8) {
			MPASS(sort_before_lro(lro));
			/* hold back one credit and don't flush LRO state */
			iq->flags |= IQ_ADJ_CREDIT;
			ndescs--;
		} else {
			tcp_lro_flush_all(lro);
		}
	}
#endif

	t4_write_reg(sc, sc->sge_gts_reg, V_CIDXINC(ndescs) |
	    V_INGRESSQID((u32)iq->cntxt_id) | V_SEINTARM(iq->intr_params));

	FL_LOCK(fl);
	starved = refill_fl(sc, fl, 64);
	FL_UNLOCK(fl);
	if (__predict_false(starved != 0))
		add_fl_to_sfl(sc, fl);

	return (0);
}

static inline struct cluster_metadata *
cl_metadata(struct fl_sdesc *sd)
{

	return ((void *)(sd->cl + sd->moff));
}

static void
rxb_free(struct mbuf *m)
{
	struct cluster_metadata *clm = m->m_ext.ext_arg1;

	uma_zfree(clm->zone, clm->cl);
	counter_u64_add(extfree_rels, 1);
}

/*
 * The mbuf returned comes from zone_muf and carries the payload in one of these
 * ways
 * a) complete frame inside the mbuf
 * b) m_cljset (for clusters without metadata)
 * d) m_extaddref (cluster with metadata)
 */
static struct mbuf *
get_scatter_segment(struct adapter *sc, struct sge_fl *fl, int fr_offset,
    int remaining)
{
	struct mbuf *m;
	struct fl_sdesc *sd = &fl->sdesc[fl->cidx];
	struct rx_buf_info *rxb = &sc->sge.rx_buf_info[sd->zidx];
	struct cluster_metadata *clm;
	int len, blen;
	caddr_t payload;

	if (fl->flags & FL_BUF_PACKING) {
		u_int l, pad;

		blen = rxb->size2 - fl->rx_offset;	/* max possible in this buf */
		len = min(remaining, blen);
		payload = sd->cl + fl->rx_offset;

		l = fr_offset + len;
		pad = roundup2(l, fl->buf_boundary) - l;
		if (fl->rx_offset + len + pad < rxb->size2)
			blen = len + pad;
		MPASS(fl->rx_offset + blen <= rxb->size2);
	} else {
		MPASS(fl->rx_offset == 0);	/* not packing */
		blen = rxb->size1;
		len = min(remaining, blen);
		payload = sd->cl;
	}

	if (fr_offset == 0) {
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (__predict_false(m == NULL))
			return (NULL);
		m->m_pkthdr.len = remaining;
	} else {
		m = m_get(M_NOWAIT, MT_DATA);
		if (__predict_false(m == NULL))
			return (NULL);
	}
	m->m_len = len;

	if (sc->sc_do_rxcopy && len < RX_COPY_THRESHOLD) {
		/* copy data to mbuf */
		bcopy(payload, mtod(m, caddr_t), len);
		if (fl->flags & FL_BUF_PACKING) {
			fl->rx_offset += blen;
			MPASS(fl->rx_offset <= rxb->size2);
			if (fl->rx_offset < rxb->size2)
				return (m);	/* without advancing the cidx */
		}
	} else if (fl->flags & FL_BUF_PACKING) {
		clm = cl_metadata(sd);
		if (sd->nmbuf++ == 0) {
			clm->refcount = 1;
			clm->zone = rxb->zone;
			clm->cl = sd->cl;
			counter_u64_add(extfree_refs, 1);
		}
		m_extaddref(m, payload, blen, &clm->refcount, rxb_free, clm,
		    NULL);

		fl->rx_offset += blen;
		MPASS(fl->rx_offset <= rxb->size2);
		if (fl->rx_offset < rxb->size2)
			return (m);	/* without advancing the cidx */
	} else {
		m_cljset(m, sd->cl, rxb->type);
		sd->cl = NULL;	/* consumed, not a recycle candidate */
	}

	move_to_next_rxbuf(fl);

	return (m);
}

static struct mbuf *
get_fl_payload(struct adapter *sc, struct sge_fl *fl, const u_int plen)
{
	struct mbuf *m0, *m, **pnext;
	u_int remaining;

	if (__predict_false(fl->flags & FL_BUF_RESUME)) {
		M_ASSERTPKTHDR(fl->m0);
		MPASS(fl->m0->m_pkthdr.len == plen);
		MPASS(fl->remaining < plen);

		m0 = fl->m0;
		pnext = fl->pnext;
		remaining = fl->remaining;
		fl->flags &= ~FL_BUF_RESUME;
		goto get_segment;
	}

	/*
	 * Payload starts at rx_offset in the current hw buffer.  Its length is
	 * 'len' and it may span multiple hw buffers.
	 */

	m0 = get_scatter_segment(sc, fl, 0, plen);
	if (m0 == NULL)
		return (NULL);
	remaining = plen - m0->m_len;
	pnext = &m0->m_next;
	while (remaining > 0) {
get_segment:
		MPASS(fl->rx_offset == 0);
		m = get_scatter_segment(sc, fl, plen - remaining, remaining);
		if (__predict_false(m == NULL)) {
			fl->m0 = m0;
			fl->pnext = pnext;
			fl->remaining = remaining;
			fl->flags |= FL_BUF_RESUME;
			return (NULL);
		}
		*pnext = m;
		pnext = &m->m_next;
		remaining -= m->m_len;
	}
	*pnext = NULL;

	M_ASSERTPKTHDR(m0);
	return (m0);
}

static int
skip_scatter_segment(struct adapter *sc, struct sge_fl *fl, int fr_offset,
    int remaining)
{
	struct fl_sdesc *sd = &fl->sdesc[fl->cidx];
	struct rx_buf_info *rxb = &sc->sge.rx_buf_info[sd->zidx];
	int len, blen;

	if (fl->flags & FL_BUF_PACKING) {
		u_int l, pad;

		blen = rxb->size2 - fl->rx_offset;	/* max possible in this buf */
		len = min(remaining, blen);

		l = fr_offset + len;
		pad = roundup2(l, fl->buf_boundary) - l;
		if (fl->rx_offset + len + pad < rxb->size2)
			blen = len + pad;
		fl->rx_offset += blen;
		MPASS(fl->rx_offset <= rxb->size2);
		if (fl->rx_offset < rxb->size2)
			return (len);	/* without advancing the cidx */
	} else {
		MPASS(fl->rx_offset == 0);	/* not packing */
		blen = rxb->size1;
		len = min(remaining, blen);
	}
	move_to_next_rxbuf(fl);
	return (len);
}

static inline void
skip_fl_payload(struct adapter *sc, struct sge_fl *fl, int plen)
{
	int remaining, fr_offset, len;

	fr_offset = 0;
	remaining = plen;
	while (remaining > 0) {
		len = skip_scatter_segment(sc, fl, fr_offset, remaining);
		fr_offset += len;
		remaining -= len;
	}
}

static inline int
get_segment_len(struct adapter *sc, struct sge_fl *fl, int plen)
{
	int len;
	struct fl_sdesc *sd = &fl->sdesc[fl->cidx];
	struct rx_buf_info *rxb = &sc->sge.rx_buf_info[sd->zidx];

	if (fl->flags & FL_BUF_PACKING)
		len = rxb->size2 - fl->rx_offset;
	else
		len = rxb->size1;

	return (min(plen, len));
}

static int
eth_rx(struct adapter *sc, struct sge_rxq *rxq, const struct iq_desc *d,
    u_int plen)
{
	struct mbuf *m0;
	struct ifnet *ifp = rxq->ifp;
	struct sge_fl *fl = &rxq->fl;
	struct vi_info *vi = ifp->if_softc;
	const struct cpl_rx_pkt *cpl;
#if defined(INET) || defined(INET6)
	struct lro_ctrl *lro = &rxq->lro;
#endif
	uint16_t err_vec, tnl_type, tnlhdr_len;
	static const int sw_hashtype[4][2] = {
		{M_HASHTYPE_NONE, M_HASHTYPE_NONE},
		{M_HASHTYPE_RSS_IPV4, M_HASHTYPE_RSS_IPV6},
		{M_HASHTYPE_RSS_TCP_IPV4, M_HASHTYPE_RSS_TCP_IPV6},
		{M_HASHTYPE_RSS_UDP_IPV4, M_HASHTYPE_RSS_UDP_IPV6},
	};
	static const int sw_csum_flags[2][2] = {
		{
			/* IP, inner IP */
			CSUM_ENCAP_VXLAN |
			    CSUM_L3_CALC | CSUM_L3_VALID |
			    CSUM_L4_CALC | CSUM_L4_VALID |
			    CSUM_INNER_L3_CALC | CSUM_INNER_L3_VALID |
			    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID,

			/* IP, inner IP6 */
			CSUM_ENCAP_VXLAN |
			    CSUM_L3_CALC | CSUM_L3_VALID |
			    CSUM_L4_CALC | CSUM_L4_VALID |
			    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID,
		},
		{
			/* IP6, inner IP */
			CSUM_ENCAP_VXLAN |
			    CSUM_L4_CALC | CSUM_L4_VALID |
			    CSUM_INNER_L3_CALC | CSUM_INNER_L3_VALID |
			    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID,

			/* IP6, inner IP6 */
			CSUM_ENCAP_VXLAN |
			    CSUM_L4_CALC | CSUM_L4_VALID |
			    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID,
		},
	};

	MPASS(plen > sc->params.sge.fl_pktshift);
	if (vi->pfil != NULL && PFIL_HOOKED_IN(vi->pfil) &&
	    __predict_true((fl->flags & FL_BUF_RESUME) == 0)) {
		struct fl_sdesc *sd = &fl->sdesc[fl->cidx];
		caddr_t frame;
		int rc, slen;

		slen = get_segment_len(sc, fl, plen) -
		    sc->params.sge.fl_pktshift;
		frame = sd->cl + fl->rx_offset + sc->params.sge.fl_pktshift;
		CURVNET_SET_QUIET(ifp->if_vnet);
		rc = pfil_run_hooks(vi->pfil, frame, ifp,
		    slen | PFIL_MEMPTR | PFIL_IN, NULL);
		CURVNET_RESTORE();
		if (rc == PFIL_DROPPED || rc == PFIL_CONSUMED) {
			skip_fl_payload(sc, fl, plen);
			return (0);
		}
		if (rc == PFIL_REALLOCED) {
			skip_fl_payload(sc, fl, plen);
			m0 = pfil_mem2mbuf(frame);
			goto have_mbuf;
		}
	}

	m0 = get_fl_payload(sc, fl, plen);
	if (__predict_false(m0 == NULL))
		return (ENOMEM);

	m0->m_pkthdr.len -= sc->params.sge.fl_pktshift;
	m0->m_len -= sc->params.sge.fl_pktshift;
	m0->m_data += sc->params.sge.fl_pktshift;

have_mbuf:
	m0->m_pkthdr.rcvif = ifp;
	M_HASHTYPE_SET(m0, sw_hashtype[d->rss.hash_type][d->rss.ipv6]);
	m0->m_pkthdr.flowid = be32toh(d->rss.hash_val);

	cpl = (const void *)(&d->rss + 1);
	if (sc->params.tp.rx_pkt_encap) {
		const uint16_t ev = be16toh(cpl->err_vec);

		err_vec = G_T6_COMPR_RXERR_VEC(ev);
		tnl_type = G_T6_RX_TNL_TYPE(ev);
		tnlhdr_len = G_T6_RX_TNLHDR_LEN(ev);
	} else {
		err_vec = be16toh(cpl->err_vec);
		tnl_type = 0;
		tnlhdr_len = 0;
	}
	if (cpl->csum_calc && err_vec == 0) {
		int ipv6 = !!(cpl->l2info & htobe32(F_RXF_IP6));

		/* checksum(s) calculated and found to be correct. */

		MPASS((cpl->l2info & htobe32(F_RXF_IP)) ^
		    (cpl->l2info & htobe32(F_RXF_IP6)));
		m0->m_pkthdr.csum_data = be16toh(cpl->csum);
		if (tnl_type == 0) {
	    		if (!ipv6 && ifp->if_capenable & IFCAP_RXCSUM) {
				m0->m_pkthdr.csum_flags = CSUM_L3_CALC |
				    CSUM_L3_VALID | CSUM_L4_CALC |
				    CSUM_L4_VALID;
			} else if (ipv6 && ifp->if_capenable & IFCAP_RXCSUM_IPV6) {
				m0->m_pkthdr.csum_flags = CSUM_L4_CALC |
				    CSUM_L4_VALID;
			}
			rxq->rxcsum++;
		} else {
			MPASS(tnl_type == RX_PKT_TNL_TYPE_VXLAN);
			if (__predict_false(cpl->ip_frag)) {
				/*
				 * csum_data is for the inner frame (which is an
				 * IP fragment) and is not 0xffff.  There is no
				 * way to pass the inner csum_data to the stack.
				 * We don't want the stack to use the inner
				 * csum_data to validate the outer frame or it
				 * will get rejected.  So we fix csum_data here
				 * and let sw do the checksum of inner IP
				 * fragments.
				 *
				 * XXX: Need 32b for csum_data2 in an rx mbuf.
				 * Maybe stuff it into rcv_tstmp?
				 */
				m0->m_pkthdr.csum_data = 0xffff;
				if (ipv6) {
					m0->m_pkthdr.csum_flags = CSUM_L4_CALC |
					    CSUM_L4_VALID;
				} else {
					m0->m_pkthdr.csum_flags = CSUM_L3_CALC |
					    CSUM_L3_VALID | CSUM_L4_CALC |
					    CSUM_L4_VALID;
				}
			} else {
				int outer_ipv6;

				MPASS(m0->m_pkthdr.csum_data == 0xffff);

				outer_ipv6 = tnlhdr_len >=
				    sizeof(struct ether_header) +
				    sizeof(struct ip6_hdr);
				m0->m_pkthdr.csum_flags =
				    sw_csum_flags[outer_ipv6][ipv6];
			}
			rxq->vxlan_rxcsum++;
		}
	}

	if (cpl->vlan_ex) {
		m0->m_pkthdr.ether_vtag = be16toh(cpl->vlan);
		m0->m_flags |= M_VLANTAG;
		rxq->vlan_extraction++;
	}

	if (rxq->iq.flags & IQ_RX_TIMESTAMP) {
		/*
		 * Fill up rcv_tstmp but do not set M_TSTMP.
		 * rcv_tstmp is not in the format that the
		 * kernel expects and we don't want to mislead
		 * it.  For now this is only for custom code
		 * that knows how to interpret cxgbe's stamp.
		 */
		m0->m_pkthdr.rcv_tstmp =
		    last_flit_to_ns(sc, d->rsp.u.last_flit);
#ifdef notyet
		m0->m_flags |= M_TSTMP;
#endif
	}

#ifdef NUMA
	m0->m_pkthdr.numa_domain = ifp->if_numa_domain;
#endif
#if defined(INET) || defined(INET6)
	if (rxq->iq.flags & IQ_LRO_ENABLED && tnl_type == 0 &&
	    (M_HASHTYPE_GET(m0) == M_HASHTYPE_RSS_TCP_IPV4 ||
	    M_HASHTYPE_GET(m0) == M_HASHTYPE_RSS_TCP_IPV6)) {
		if (sort_before_lro(lro)) {
			tcp_lro_queue_mbuf(lro, m0);
			return (0); /* queued for sort, then LRO */
		}
		if (tcp_lro_rx(lro, m0, 0) == 0)
			return (0); /* queued for LRO */
	}
#endif
	ifp->if_input(ifp, m0);

	return (0);
}

/*
 * Must drain the wrq or make sure that someone else will.
 */
static void
wrq_tx_drain(void *arg, int n)
{
	struct sge_wrq *wrq = arg;
	struct sge_eq *eq = &wrq->eq;

	EQ_LOCK(eq);
	if (TAILQ_EMPTY(&wrq->incomplete_wrs) && !STAILQ_EMPTY(&wrq->wr_list))
		drain_wrq_wr_list(wrq->adapter, wrq);
	EQ_UNLOCK(eq);
}

static void
drain_wrq_wr_list(struct adapter *sc, struct sge_wrq *wrq)
{
	struct sge_eq *eq = &wrq->eq;
	u_int available, dbdiff;	/* # of hardware descriptors */
	u_int n;
	struct wrqe *wr;
	struct fw_eth_tx_pkt_wr *dst;	/* any fw WR struct will do */

	EQ_LOCK_ASSERT_OWNED(eq);
	MPASS(TAILQ_EMPTY(&wrq->incomplete_wrs));
	wr = STAILQ_FIRST(&wrq->wr_list);
	MPASS(wr != NULL);	/* Must be called with something useful to do */
	MPASS(eq->pidx == eq->dbidx);
	dbdiff = 0;

	do {
		eq->cidx = read_hw_cidx(eq);
		if (eq->pidx == eq->cidx)
			available = eq->sidx - 1;
		else
			available = IDXDIFF(eq->cidx, eq->pidx, eq->sidx) - 1;

		MPASS(wr->wrq == wrq);
		n = howmany(wr->wr_len, EQ_ESIZE);
		if (available < n)
			break;

		dst = (void *)&eq->desc[eq->pidx];
		if (__predict_true(eq->sidx - eq->pidx > n)) {
			/* Won't wrap, won't end exactly at the status page. */
			bcopy(&wr->wr[0], dst, wr->wr_len);
			eq->pidx += n;
		} else {
			int first_portion = (eq->sidx - eq->pidx) * EQ_ESIZE;

			bcopy(&wr->wr[0], dst, first_portion);
			if (wr->wr_len > first_portion) {
				bcopy(&wr->wr[first_portion], &eq->desc[0],
				    wr->wr_len - first_portion);
			}
			eq->pidx = n - (eq->sidx - eq->pidx);
		}
		wrq->tx_wrs_copied++;

		if (available < eq->sidx / 4 &&
		    atomic_cmpset_int(&eq->equiq, 0, 1)) {
				/*
				 * XXX: This is not 100% reliable with some
				 * types of WRs.  But this is a very unusual
				 * situation for an ofld/ctrl queue anyway.
				 */
			dst->equiq_to_len16 |= htobe32(F_FW_WR_EQUIQ |
			    F_FW_WR_EQUEQ);
		}

		dbdiff += n;
		if (dbdiff >= 16) {
			ring_eq_db(sc, eq, dbdiff);
			dbdiff = 0;
		}

		STAILQ_REMOVE_HEAD(&wrq->wr_list, link);
		free_wrqe(wr);
		MPASS(wrq->nwr_pending > 0);
		wrq->nwr_pending--;
		MPASS(wrq->ndesc_needed >= n);
		wrq->ndesc_needed -= n;
	} while ((wr = STAILQ_FIRST(&wrq->wr_list)) != NULL);

	if (dbdiff)
		ring_eq_db(sc, eq, dbdiff);
}

/*
 * Doesn't fail.  Holds on to work requests it can't send right away.
 */
void
t4_wrq_tx_locked(struct adapter *sc, struct sge_wrq *wrq, struct wrqe *wr)
{
#ifdef INVARIANTS
	struct sge_eq *eq = &wrq->eq;
#endif

	EQ_LOCK_ASSERT_OWNED(eq);
	MPASS(wr != NULL);
	MPASS(wr->wr_len > 0 && wr->wr_len <= SGE_MAX_WR_LEN);
	MPASS((wr->wr_len & 0x7) == 0);

	STAILQ_INSERT_TAIL(&wrq->wr_list, wr, link);
	wrq->nwr_pending++;
	wrq->ndesc_needed += howmany(wr->wr_len, EQ_ESIZE);

	if (!TAILQ_EMPTY(&wrq->incomplete_wrs))
		return;	/* commit_wrq_wr will drain wr_list as well. */

	drain_wrq_wr_list(sc, wrq);

	/* Doorbell must have caught up to the pidx. */
	MPASS(eq->pidx == eq->dbidx);
}

void
t4_update_fl_bufsize(struct ifnet *ifp)
{
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->adapter;
	struct sge_rxq *rxq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
#endif
	struct sge_fl *fl;
	int i, maxp;

	maxp = max_rx_payload(sc, ifp, false);
	for_each_rxq(vi, i, rxq) {
		fl = &rxq->fl;

		FL_LOCK(fl);
		fl->zidx = find_refill_source(sc, maxp,
		    fl->flags & FL_BUF_PACKING);
		FL_UNLOCK(fl);
	}
#ifdef TCP_OFFLOAD
	maxp = max_rx_payload(sc, ifp, true);
	for_each_ofld_rxq(vi, i, ofld_rxq) {
		fl = &ofld_rxq->fl;

		FL_LOCK(fl);
		fl->zidx = find_refill_source(sc, maxp,
		    fl->flags & FL_BUF_PACKING);
		FL_UNLOCK(fl);
	}
#endif
}

static inline int
mbuf_nsegs(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	KASSERT(m->m_pkthdr.inner_l5hlen > 0,
	    ("%s: mbuf %p missing information on # of segments.", __func__, m));

	return (m->m_pkthdr.inner_l5hlen);
}

static inline void
set_mbuf_nsegs(struct mbuf *m, uint8_t nsegs)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.inner_l5hlen = nsegs;
}

static inline int
mbuf_cflags(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_loc.eight[4]);
}

static inline void
set_mbuf_cflags(struct mbuf *m, uint8_t flags)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_loc.eight[4] = flags;
}

static inline int
mbuf_len16(struct mbuf *m)
{
	int n;

	M_ASSERTPKTHDR(m);
	n = m->m_pkthdr.PH_loc.eight[0];
	if (!(mbuf_cflags(m) & MC_TLS))
		MPASS(n > 0 && n <= SGE_MAX_WR_LEN / 16);

	return (n);
}

static inline void
set_mbuf_len16(struct mbuf *m, uint8_t len16)
{

	M_ASSERTPKTHDR(m);
	if (!(mbuf_cflags(m) & MC_TLS))
		MPASS(len16 > 0 && len16 <= SGE_MAX_WR_LEN / 16);
	m->m_pkthdr.PH_loc.eight[0] = len16;
}

#ifdef RATELIMIT
static inline int
mbuf_eo_nsegs(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_loc.eight[1]);
}

static inline void
set_mbuf_eo_nsegs(struct mbuf *m, uint8_t nsegs)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_loc.eight[1] = nsegs;
}

static inline int
mbuf_eo_len16(struct mbuf *m)
{
	int n;

	M_ASSERTPKTHDR(m);
	n = m->m_pkthdr.PH_loc.eight[2];
	MPASS(n > 0 && n <= SGE_MAX_WR_LEN / 16);

	return (n);
}

static inline void
set_mbuf_eo_len16(struct mbuf *m, uint8_t len16)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_loc.eight[2] = len16;
}

static inline int
mbuf_eo_tsclk_tsoff(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_loc.eight[3]);
}

static inline void
set_mbuf_eo_tsclk_tsoff(struct mbuf *m, uint8_t tsclk_tsoff)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_loc.eight[3] = tsclk_tsoff;
}

static inline int
needs_eo(struct m_snd_tag *mst)
{

	return (mst != NULL && mst->type == IF_SND_TAG_TYPE_RATE_LIMIT);
}
#endif

/*
 * Try to allocate an mbuf to contain a raw work request.  To make it
 * easy to construct the work request, don't allocate a chain but a
 * single mbuf.
 */
struct mbuf *
alloc_wr_mbuf(int len, int how)
{
	struct mbuf *m;

	if (len <= MHLEN)
		m = m_gethdr(how, MT_DATA);
	else if (len <= MCLBYTES)
		m = m_getcl(how, MT_DATA, M_PKTHDR);
	else
		m = NULL;
	if (m == NULL)
		return (NULL);
	m->m_pkthdr.len = len;
	m->m_len = len;
	set_mbuf_cflags(m, MC_RAW_WR);
	set_mbuf_len16(m, howmany(len, 16));
	return (m);
}

static inline bool
needs_hwcsum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP | CSUM_IP_UDP | CSUM_IP_TCP |
	    CSUM_IP_TSO | CSUM_INNER_IP | CSUM_INNER_IP_UDP |
	    CSUM_INNER_IP_TCP | CSUM_INNER_IP_TSO | CSUM_IP6_UDP |
	    CSUM_IP6_TCP | CSUM_IP6_TSO | CSUM_INNER_IP6_UDP |
	    CSUM_INNER_IP6_TCP | CSUM_INNER_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

static inline bool
needs_tso(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP_TSO | CSUM_IP6_TSO |
	    CSUM_INNER_IP_TSO | CSUM_INNER_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

static inline bool
needs_vxlan_csum(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & CSUM_ENCAP_VXLAN);
}

static inline bool
needs_vxlan_tso(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_ENCAP_VXLAN | CSUM_INNER_IP_TSO |
	    CSUM_INNER_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return ((m->m_pkthdr.csum_flags & csum_flags) != 0 &&
	    (m->m_pkthdr.csum_flags & csum_flags) != CSUM_ENCAP_VXLAN);
}

static inline bool
needs_inner_tcp_csum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_INNER_IP_TSO | CSUM_INNER_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

static inline bool
needs_l3_csum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP | CSUM_IP_TSO | CSUM_INNER_IP |
	    CSUM_INNER_IP_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

static inline bool
needs_outer_tcp_csum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP_TCP | CSUM_IP_TSO | CSUM_IP6_TCP |
	    CSUM_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

#ifdef RATELIMIT
static inline bool
needs_outer_l4_csum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP_UDP | CSUM_IP_TCP | CSUM_IP_TSO |
	    CSUM_IP6_UDP | CSUM_IP6_TCP | CSUM_IP6_TSO;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}

static inline bool
needs_outer_udp_csum(struct mbuf *m)
{
	const uint32_t csum_flags = CSUM_IP_UDP | CSUM_IP6_UDP;

	M_ASSERTPKTHDR(m);

	return (m->m_pkthdr.csum_flags & csum_flags);
}
#endif

static inline bool
needs_vlan_insertion(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);

	return (m->m_flags & M_VLANTAG);
}

static void *
m_advance(struct mbuf **pm, int *poffset, int len)
{
	struct mbuf *m = *pm;
	int offset = *poffset;
	uintptr_t p = 0;

	MPASS(len > 0);

	for (;;) {
		if (offset + len < m->m_len) {
			offset += len;
			p = mtod(m, uintptr_t) + offset;
			break;
		}
		len -= m->m_len - offset;
		m = m->m_next;
		offset = 0;
		MPASS(m != NULL);
	}
	*poffset = offset;
	*pm = m;
	return ((void *)p);
}

static inline int
count_mbuf_ext_pgs(struct mbuf *m, int skip, vm_paddr_t *nextaddr)
{
	vm_paddr_t paddr;
	int i, len, off, pglen, pgoff, seglen, segoff;
	int nsegs = 0;

	M_ASSERTEXTPG(m);
	off = mtod(m, vm_offset_t);
	len = m->m_len;
	off += skip;
	len -= skip;

	if (m->m_epg_hdrlen != 0) {
		if (off >= m->m_epg_hdrlen) {
			off -= m->m_epg_hdrlen;
		} else {
			seglen = m->m_epg_hdrlen - off;
			segoff = off;
			seglen = min(seglen, len);
			off = 0;
			len -= seglen;
			paddr = pmap_kextract(
			    (vm_offset_t)&m->m_epg_hdr[segoff]);
			if (*nextaddr != paddr)
				nsegs++;
			*nextaddr = paddr + seglen;
		}
	}
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs && len > 0; i++) {
		pglen = m_epg_pagelen(m, i, pgoff);
		if (off >= pglen) {
			off -= pglen;
			pgoff = 0;
			continue;
		}
		seglen = pglen - off;
		segoff = pgoff + off;
		off = 0;
		seglen = min(seglen, len);
		len -= seglen;
		paddr = m->m_epg_pa[i] + segoff;
		if (*nextaddr != paddr)
			nsegs++;
		*nextaddr = paddr + seglen;
		pgoff = 0;
	};
	if (len != 0) {
		seglen = min(len, m->m_epg_trllen - off);
		len -= seglen;
		paddr = pmap_kextract((vm_offset_t)&m->m_epg_trail[off]);
		if (*nextaddr != paddr)
			nsegs++;
		*nextaddr = paddr + seglen;
	}

	return (nsegs);
}


/*
 * Can deal with empty mbufs in the chain that have m_len = 0, but the chain
 * must have at least one mbuf that's not empty.  It is possible for this
 * routine to return 0 if skip accounts for all the contents of the mbuf chain.
 */
static inline int
count_mbuf_nsegs(struct mbuf *m, int skip, uint8_t *cflags)
{
	vm_paddr_t nextaddr, paddr;
	vm_offset_t va;
	int len, nsegs;

	M_ASSERTPKTHDR(m);
	MPASS(m->m_pkthdr.len > 0);
	MPASS(m->m_pkthdr.len >= skip);

	nsegs = 0;
	nextaddr = 0;
	for (; m; m = m->m_next) {
		len = m->m_len;
		if (__predict_false(len == 0))
			continue;
		if (skip >= len) {
			skip -= len;
			continue;
		}
		if ((m->m_flags & M_EXTPG) != 0) {
			*cflags |= MC_NOMAP;
			nsegs += count_mbuf_ext_pgs(m, skip, &nextaddr);
			skip = 0;
			continue;
		}
		va = mtod(m, vm_offset_t) + skip;
		len -= skip;
		skip = 0;
		paddr = pmap_kextract(va);
		nsegs += sglist_count((void *)(uintptr_t)va, len);
		if (paddr == nextaddr)
			nsegs--;
		nextaddr = pmap_kextract(va + len - 1) + 1;
	}

	return (nsegs);
}

/*
 * The maximum number of segments that can fit in a WR.
 */
static int
max_nsegs_allowed(struct mbuf *m, bool vm_wr)
{

	if (vm_wr) {
		if (needs_tso(m))
			return (TX_SGL_SEGS_VM_TSO);
		return (TX_SGL_SEGS_VM);
	}

	if (needs_tso(m)) {
		if (needs_vxlan_tso(m))
			return (TX_SGL_SEGS_VXLAN_TSO);
		else
			return (TX_SGL_SEGS_TSO);
	}

	return (TX_SGL_SEGS);
}

/*
 * Analyze the mbuf to determine its tx needs.  The mbuf passed in may change:
 * a) caller can assume it's been freed if this function returns with an error.
 * b) it may get defragged up if the gather list is too long for the hardware.
 */
int
parse_pkt(struct mbuf **mp, bool vm_wr)
{
	struct mbuf *m0 = *mp, *m;
	int rc, nsegs, defragged = 0, offset;
	struct ether_header *eh;
	void *l3hdr;
#if defined(INET) || defined(INET6)
	struct tcphdr *tcp;
#endif
#if defined(KERN_TLS) || defined(RATELIMIT)
	struct m_snd_tag *mst;
#endif
	uint16_t eh_type;
	uint8_t cflags;

	cflags = 0;
	M_ASSERTPKTHDR(m0);
	if (__predict_false(m0->m_pkthdr.len < ETHER_HDR_LEN)) {
		rc = EINVAL;
fail:
		m_freem(m0);
		*mp = NULL;
		return (rc);
	}
restart:
	/*
	 * First count the number of gather list segments in the payload.
	 * Defrag the mbuf if nsegs exceeds the hardware limit.
	 */
	M_ASSERTPKTHDR(m0);
	MPASS(m0->m_pkthdr.len > 0);
	nsegs = count_mbuf_nsegs(m0, 0, &cflags);
#if defined(KERN_TLS) || defined(RATELIMIT)
	if (m0->m_pkthdr.csum_flags & CSUM_SND_TAG)
		mst = m0->m_pkthdr.snd_tag;
	else
		mst = NULL;
#endif
#ifdef KERN_TLS
	if (mst != NULL && mst->type == IF_SND_TAG_TYPE_TLS) {
		int len16;

		cflags |= MC_TLS;
		set_mbuf_cflags(m0, cflags);
		rc = t6_ktls_parse_pkt(m0, &nsegs, &len16);
		if (rc != 0)
			goto fail;
		set_mbuf_nsegs(m0, nsegs);
		set_mbuf_len16(m0, len16);
		return (0);
	}
#endif
	if (nsegs > max_nsegs_allowed(m0, vm_wr)) {
		if (defragged++ > 0) {
			rc = EFBIG;
			goto fail;
		}
		counter_u64_add(defrags, 1);
		if ((m = m_defrag(m0, M_NOWAIT)) == NULL) {
			rc = ENOMEM;
			goto fail;
		}
		*mp = m0 = m;	/* update caller's copy after defrag */
		goto restart;
	}

	if (__predict_false(nsegs > 2 && m0->m_pkthdr.len <= MHLEN &&
	    !(cflags & MC_NOMAP))) {
		counter_u64_add(pullups, 1);
		m0 = m_pullup(m0, m0->m_pkthdr.len);
		if (m0 == NULL) {
			/* Should have left well enough alone. */
			rc = EFBIG;
			goto fail;
		}
		*mp = m0;	/* update caller's copy after pullup */
		goto restart;
	}
	set_mbuf_nsegs(m0, nsegs);
	set_mbuf_cflags(m0, cflags);
	calculate_mbuf_len16(m0, vm_wr);

#ifdef RATELIMIT
	/*
	 * Ethofld is limited to TCP and UDP for now, and only when L4 hw
	 * checksumming is enabled.  needs_outer_l4_csum happens to check for
	 * all the right things.
	 */
	if (__predict_false(needs_eo(mst) && !needs_outer_l4_csum(m0))) {
		m_snd_tag_rele(m0->m_pkthdr.snd_tag);
		m0->m_pkthdr.snd_tag = NULL;
		m0->m_pkthdr.csum_flags &= ~CSUM_SND_TAG;
		mst = NULL;
	}
#endif

	if (!needs_hwcsum(m0)
#ifdef RATELIMIT
   		 && !needs_eo(mst)
#endif
	)
		return (0);

	m = m0;
	eh = mtod(m, struct ether_header *);
	eh_type = ntohs(eh->ether_type);
	if (eh_type == ETHERTYPE_VLAN) {
		struct ether_vlan_header *evh = (void *)eh;

		eh_type = ntohs(evh->evl_proto);
		m0->m_pkthdr.l2hlen = sizeof(*evh);
	} else
		m0->m_pkthdr.l2hlen = sizeof(*eh);

	offset = 0;
	l3hdr = m_advance(&m, &offset, m0->m_pkthdr.l2hlen);

	switch (eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		m0->m_pkthdr.l3hlen = sizeof(struct ip6_hdr);
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
	{
		struct ip *ip = l3hdr;

		if (needs_vxlan_csum(m0)) {
			/* Driver will do the outer IP hdr checksum. */
			ip->ip_sum = 0;
			if (needs_vxlan_tso(m0)) {
				const uint16_t ipl = ip->ip_len;

				ip->ip_len = 0;
				ip->ip_sum = ~in_cksum_hdr(ip);
				ip->ip_len = ipl;
			} else
				ip->ip_sum = in_cksum_hdr(ip);
		}
		m0->m_pkthdr.l3hlen = ip->ip_hl << 2;
		break;
	}
#endif
	default:
		panic("%s: ethertype 0x%04x unknown.  if_cxgbe must be compiled"
		    " with the same INET/INET6 options as the kernel.",
		    __func__, eh_type);
	}

	if (needs_vxlan_csum(m0)) {
		m0->m_pkthdr.l4hlen = sizeof(struct udphdr);
		m0->m_pkthdr.l5hlen = sizeof(struct vxlan_header);

		/* Inner headers. */
		eh = m_advance(&m, &offset, m0->m_pkthdr.l3hlen +
		    sizeof(struct udphdr) + sizeof(struct vxlan_header));
		eh_type = ntohs(eh->ether_type);
		if (eh_type == ETHERTYPE_VLAN) {
			struct ether_vlan_header *evh = (void *)eh;

			eh_type = ntohs(evh->evl_proto);
			m0->m_pkthdr.inner_l2hlen = sizeof(*evh);
		} else
			m0->m_pkthdr.inner_l2hlen = sizeof(*eh);
		l3hdr = m_advance(&m, &offset, m0->m_pkthdr.inner_l2hlen);

		switch (eh_type) {
#ifdef INET6
		case ETHERTYPE_IPV6:
			m0->m_pkthdr.inner_l3hlen = sizeof(struct ip6_hdr);
			break;
#endif
#ifdef INET
		case ETHERTYPE_IP:
		{
			struct ip *ip = l3hdr;

			m0->m_pkthdr.inner_l3hlen = ip->ip_hl << 2;
			break;
		}
#endif
		default:
			panic("%s: VXLAN hw offload requested with unknown "
			    "ethertype 0x%04x.  if_cxgbe must be compiled"
			    " with the same INET/INET6 options as the kernel.",
			    __func__, eh_type);
		}
#if defined(INET) || defined(INET6)
		if (needs_inner_tcp_csum(m0)) {
			tcp = m_advance(&m, &offset, m0->m_pkthdr.inner_l3hlen);
			m0->m_pkthdr.inner_l4hlen = tcp->th_off * 4;
		}
#endif
		MPASS((m0->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0);
		m0->m_pkthdr.csum_flags &= CSUM_INNER_IP6_UDP |
		    CSUM_INNER_IP6_TCP | CSUM_INNER_IP6_TSO | CSUM_INNER_IP |
		    CSUM_INNER_IP_UDP | CSUM_INNER_IP_TCP | CSUM_INNER_IP_TSO |
		    CSUM_ENCAP_VXLAN;
	}

#if defined(INET) || defined(INET6)
	if (needs_outer_tcp_csum(m0)) {
		tcp = m_advance(&m, &offset, m0->m_pkthdr.l3hlen);
		m0->m_pkthdr.l4hlen = tcp->th_off * 4;
#ifdef RATELIMIT
		if (tsclk >= 0 && *(uint32_t *)(tcp + 1) == ntohl(0x0101080a)) {
			set_mbuf_eo_tsclk_tsoff(m0,
			    V_FW_ETH_TX_EO_WR_TSCLK(tsclk) |
			    V_FW_ETH_TX_EO_WR_TSOFF(sizeof(*tcp) / 2 + 1));
		} else
			set_mbuf_eo_tsclk_tsoff(m0, 0);
	} else if (needs_outer_udp_csum(m0)) {
		m0->m_pkthdr.l4hlen = sizeof(struct udphdr);
#endif
	}
#ifdef RATELIMIT
	if (needs_eo(mst)) {
		u_int immhdrs;

		/* EO WRs have the headers in the WR and not the GL. */
		immhdrs = m0->m_pkthdr.l2hlen + m0->m_pkthdr.l3hlen +
		    m0->m_pkthdr.l4hlen;
		cflags = 0;
		nsegs = count_mbuf_nsegs(m0, immhdrs, &cflags);
		MPASS(cflags == mbuf_cflags(m0));
		set_mbuf_eo_nsegs(m0, nsegs);
		set_mbuf_eo_len16(m0,
		    txpkt_eo_len16(nsegs, immhdrs, needs_tso(m0)));
	}
#endif
#endif
	MPASS(m0 == *mp);
	return (0);
}

void *
start_wrq_wr(struct sge_wrq *wrq, int len16, struct wrq_cookie *cookie)
{
	struct sge_eq *eq = &wrq->eq;
	struct adapter *sc = wrq->adapter;
	int ndesc, available;
	struct wrqe *wr;
	void *w;

	MPASS(len16 > 0);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc > 0 && ndesc <= SGE_MAX_WR_NDESC);

	EQ_LOCK(eq);

	if (TAILQ_EMPTY(&wrq->incomplete_wrs) && !STAILQ_EMPTY(&wrq->wr_list))
		drain_wrq_wr_list(sc, wrq);

	if (!STAILQ_EMPTY(&wrq->wr_list)) {
slowpath:
		EQ_UNLOCK(eq);
		wr = alloc_wrqe(len16 * 16, wrq);
		if (__predict_false(wr == NULL))
			return (NULL);
		cookie->pidx = -1;
		cookie->ndesc = ndesc;
		return (&wr->wr);
	}

	eq->cidx = read_hw_cidx(eq);
	if (eq->pidx == eq->cidx)
		available = eq->sidx - 1;
	else
		available = IDXDIFF(eq->cidx, eq->pidx, eq->sidx) - 1;
	if (available < ndesc)
		goto slowpath;

	cookie->pidx = eq->pidx;
	cookie->ndesc = ndesc;
	TAILQ_INSERT_TAIL(&wrq->incomplete_wrs, cookie, link);

	w = &eq->desc[eq->pidx];
	IDXINCR(eq->pidx, ndesc, eq->sidx);
	if (__predict_false(cookie->pidx + ndesc > eq->sidx)) {
		w = &wrq->ss[0];
		wrq->ss_pidx = cookie->pidx;
		wrq->ss_len = len16 * 16;
	}

	EQ_UNLOCK(eq);

	return (w);
}

void
commit_wrq_wr(struct sge_wrq *wrq, void *w, struct wrq_cookie *cookie)
{
	struct sge_eq *eq = &wrq->eq;
	struct adapter *sc = wrq->adapter;
	int ndesc, pidx;
	struct wrq_cookie *prev, *next;

	if (cookie->pidx == -1) {
		struct wrqe *wr = __containerof(w, struct wrqe, wr);

		t4_wrq_tx(sc, wr);
		return;
	}

	if (__predict_false(w == &wrq->ss[0])) {
		int n = (eq->sidx - wrq->ss_pidx) * EQ_ESIZE;

		MPASS(wrq->ss_len > n);	/* WR had better wrap around. */
		bcopy(&wrq->ss[0], &eq->desc[wrq->ss_pidx], n);
		bcopy(&wrq->ss[n], &eq->desc[0], wrq->ss_len - n);
		wrq->tx_wrs_ss++;
	} else
		wrq->tx_wrs_direct++;

	EQ_LOCK(eq);
	ndesc = cookie->ndesc;	/* Can be more than SGE_MAX_WR_NDESC here. */
	pidx = cookie->pidx;
	MPASS(pidx >= 0 && pidx < eq->sidx);
	prev = TAILQ_PREV(cookie, wrq_incomplete_wrs, link);
	next = TAILQ_NEXT(cookie, link);
	if (prev == NULL) {
		MPASS(pidx == eq->dbidx);
		if (next == NULL || ndesc >= 16) {
			int available;
			struct fw_eth_tx_pkt_wr *dst;	/* any fw WR struct will do */

			/*
			 * Note that the WR via which we'll request tx updates
			 * is at pidx and not eq->pidx, which has moved on
			 * already.
			 */
			dst = (void *)&eq->desc[pidx];
			available = IDXDIFF(eq->cidx, eq->pidx, eq->sidx) - 1;
			if (available < eq->sidx / 4 &&
			    atomic_cmpset_int(&eq->equiq, 0, 1)) {
				/*
				 * XXX: This is not 100% reliable with some
				 * types of WRs.  But this is a very unusual
				 * situation for an ofld/ctrl queue anyway.
				 */
				dst->equiq_to_len16 |= htobe32(F_FW_WR_EQUIQ |
				    F_FW_WR_EQUEQ);
			}

			ring_eq_db(wrq->adapter, eq, ndesc);
		} else {
			MPASS(IDXDIFF(next->pidx, pidx, eq->sidx) == ndesc);
			next->pidx = pidx;
			next->ndesc += ndesc;
		}
	} else {
		MPASS(IDXDIFF(pidx, prev->pidx, eq->sidx) == prev->ndesc);
		prev->ndesc += ndesc;
	}
	TAILQ_REMOVE(&wrq->incomplete_wrs, cookie, link);

	if (TAILQ_EMPTY(&wrq->incomplete_wrs) && !STAILQ_EMPTY(&wrq->wr_list))
		drain_wrq_wr_list(sc, wrq);

#ifdef INVARIANTS
	if (TAILQ_EMPTY(&wrq->incomplete_wrs)) {
		/* Doorbell must have caught up to the pidx. */
		MPASS(wrq->eq.pidx == wrq->eq.dbidx);
	}
#endif
	EQ_UNLOCK(eq);
}

static u_int
can_resume_eth_tx(struct mp_ring *r)
{
	struct sge_eq *eq = r->cookie;

	return (total_available_tx_desc(eq) > eq->sidx / 8);
}

static inline bool
cannot_use_txpkts(struct mbuf *m)
{
	/* maybe put a GL limit too, to avoid silliness? */

	return (needs_tso(m) || (mbuf_cflags(m) & (MC_RAW_WR | MC_TLS)) != 0);
}

static inline int
discard_tx(struct sge_eq *eq)
{

	return ((eq->flags & (EQ_ENABLED | EQ_QFLUSH)) != EQ_ENABLED);
}

static inline int
wr_can_update_eq(void *p)
{
	struct fw_eth_tx_pkts_wr *wr = p;

	switch (G_FW_WR_OP(be32toh(wr->op_pkd))) {
	case FW_ULPTX_WR:
	case FW_ETH_TX_PKT_WR:
	case FW_ETH_TX_PKTS_WR:
	case FW_ETH_TX_PKTS2_WR:
	case FW_ETH_TX_PKT_VM_WR:
	case FW_ETH_TX_PKTS_VM_WR:
		return (1);
	default:
		return (0);
	}
}

static inline void
set_txupdate_flags(struct sge_txq *txq, u_int avail,
    struct fw_eth_tx_pkt_wr *wr)
{
	struct sge_eq *eq = &txq->eq;
	struct txpkts *txp = &txq->txp;

	if ((txp->npkt > 0 || avail < eq->sidx / 2) &&
	    atomic_cmpset_int(&eq->equiq, 0, 1)) {
		wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ | F_FW_WR_EQUIQ);
		eq->equeqidx = eq->pidx;
	} else if (IDXDIFF(eq->pidx, eq->equeqidx, eq->sidx) >= 32) {
		wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ);
		eq->equeqidx = eq->pidx;
	}
}

/*
 * r->items[cidx] to r->items[pidx], with a wraparound at r->size, are ready to
 * be consumed.  Return the actual number consumed.  0 indicates a stall.
 */
static u_int
eth_tx(struct mp_ring *r, u_int cidx, u_int pidx, bool *coalescing)
{
	struct sge_txq *txq = r->cookie;
	struct ifnet *ifp = txq->ifp;
	struct sge_eq *eq = &txq->eq;
	struct txpkts *txp = &txq->txp;
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->adapter;
	u_int total, remaining;		/* # of packets */
	u_int n, avail, dbdiff;		/* # of hardware descriptors */
	int i, rc;
	struct mbuf *m0;
	bool snd;
	void *wr;	/* start of the last WR written to the ring */

	TXQ_LOCK_ASSERT_OWNED(txq);

	remaining = IDXDIFF(pidx, cidx, r->size);
	if (__predict_false(discard_tx(eq))) {
		for (i = 0; i < txp->npkt; i++)
			m_freem(txp->mb[i]);
		txp->npkt = 0;
		while (cidx != pidx) {
			m0 = r->items[cidx];
			m_freem(m0);
			if (++cidx == r->size)
				cidx = 0;
		}
		reclaim_tx_descs(txq, eq->sidx);
		*coalescing = false;
		return (remaining);	/* emptied */
	}

	/* How many hardware descriptors do we have readily available. */
	if (eq->pidx == eq->cidx) {
		avail = eq->sidx - 1;
		if (txp->score++ >= 5)
			txp->score = 5;	/* tx is completely idle, reset. */
	} else
		avail = IDXDIFF(eq->cidx, eq->pidx, eq->sidx) - 1;

	total = 0;
	if (remaining == 0) {
		if (txp->score-- == 1)	/* egr_update had to drain txpkts */
			txp->score = 1;
		goto send_txpkts;
	}

	dbdiff = 0;
	MPASS(remaining > 0);
	while (remaining > 0) {
		m0 = r->items[cidx];
		M_ASSERTPKTHDR(m0);
		MPASS(m0->m_nextpkt == NULL);

		if (avail < 2 * SGE_MAX_WR_NDESC)
			avail += reclaim_tx_descs(txq, 64);

		if (txp->npkt > 0 || remaining > 1 || txp->score > 3 ||
		    atomic_load_int(&txq->eq.equiq) != 0) {
			if (vi->flags & TX_USES_VM_WR)
				rc = add_to_txpkts_vf(sc, txq, m0, avail, &snd);
			else
				rc = add_to_txpkts_pf(sc, txq, m0, avail, &snd);
		} else {
			snd = false;
			rc = EINVAL;
		}
		if (snd) {
			MPASS(txp->npkt > 0);
			for (i = 0; i < txp->npkt; i++)
				ETHER_BPF_MTAP(ifp, txp->mb[i]);
			if (txp->npkt > 1) {
				if (txp->score++ >= 10)
					txp->score = 10;
				MPASS(avail >= tx_len16_to_desc(txp->len16));
				if (vi->flags & TX_USES_VM_WR)
					n = write_txpkts_vm_wr(sc, txq);
				else
					n = write_txpkts_wr(sc, txq);
			} else {
				MPASS(avail >=
				    tx_len16_to_desc(mbuf_len16(txp->mb[0])));
				if (vi->flags & TX_USES_VM_WR)
					n = write_txpkt_vm_wr(sc, txq,
					    txp->mb[0]);
				else
					n = write_txpkt_wr(sc, txq, txp->mb[0],
					    avail);
			}
			MPASS(n <= SGE_MAX_WR_NDESC);
			avail -= n;
			dbdiff += n;
			wr = &eq->desc[eq->pidx];
			IDXINCR(eq->pidx, n, eq->sidx);
			txp->npkt = 0;	/* emptied */
		}
		if (rc == 0) {
			/* m0 was coalesced into txq->txpkts. */
			goto next_mbuf;
		}
		if (rc == EAGAIN) {
			/*
			 * m0 is suitable for tx coalescing but could not be
			 * combined with the existing txq->txpkts, which has now
			 * been transmitted.  Start a new txpkts with m0.
			 */
			MPASS(snd);
			MPASS(txp->npkt == 0);
			continue;
		}

		MPASS(rc != 0 && rc != EAGAIN);
		MPASS(txp->npkt == 0);

		n = tx_len16_to_desc(mbuf_len16(m0));
		if (__predict_false(avail < n)) {
			avail += reclaim_tx_descs(txq, min(n, 32));
			if (avail < n)
				break;	/* out of descriptors */
		}

		wr = &eq->desc[eq->pidx];
		if (mbuf_cflags(m0) & MC_RAW_WR) {
			n = write_raw_wr(txq, wr, m0, avail);
#ifdef KERN_TLS
		} else if (mbuf_cflags(m0) & MC_TLS) {
			ETHER_BPF_MTAP(ifp, m0);
			n = t6_ktls_write_wr(txq, wr, m0, mbuf_nsegs(m0),
			    avail);
#endif
		} else {
			ETHER_BPF_MTAP(ifp, m0);
			if (vi->flags & TX_USES_VM_WR)
				n = write_txpkt_vm_wr(sc, txq, m0);
			else
				n = write_txpkt_wr(sc, txq, m0, avail);
		}
		MPASS(n >= 1 && n <= avail);
		if (!(mbuf_cflags(m0) & MC_TLS))
			MPASS(n <= SGE_MAX_WR_NDESC);

		avail -= n;
		dbdiff += n;
		IDXINCR(eq->pidx, n, eq->sidx);

		if (dbdiff >= 512 / EQ_ESIZE) {	/* X_FETCHBURSTMAX_512B */
			if (wr_can_update_eq(wr))
				set_txupdate_flags(txq, avail, wr);
			ring_eq_db(sc, eq, dbdiff);
			avail += reclaim_tx_descs(txq, 32);
			dbdiff = 0;
		}
next_mbuf:
		total++;
		remaining--;
		if (__predict_false(++cidx == r->size))
			cidx = 0;
	}
	if (dbdiff != 0) {
		if (wr_can_update_eq(wr))
			set_txupdate_flags(txq, avail, wr);
		ring_eq_db(sc, eq, dbdiff);
		reclaim_tx_descs(txq, 32);
	} else if (eq->pidx == eq->cidx && txp->npkt > 0 &&
	    atomic_load_int(&txq->eq.equiq) == 0) {
		/*
		 * If nothing was submitted to the chip for tx (it was coalesced
		 * into txpkts instead) and there is no tx update outstanding
		 * then we need to send txpkts now.
		 */
send_txpkts:
		MPASS(txp->npkt > 0);
		for (i = 0; i < txp->npkt; i++)
			ETHER_BPF_MTAP(ifp, txp->mb[i]);
		if (txp->npkt > 1) {
			MPASS(avail >= tx_len16_to_desc(txp->len16));
			if (vi->flags & TX_USES_VM_WR)
				n = write_txpkts_vm_wr(sc, txq);
			else
				n = write_txpkts_wr(sc, txq);
		} else {
			MPASS(avail >=
			    tx_len16_to_desc(mbuf_len16(txp->mb[0])));
			if (vi->flags & TX_USES_VM_WR)
				n = write_txpkt_vm_wr(sc, txq, txp->mb[0]);
			else
				n = write_txpkt_wr(sc, txq, txp->mb[0], avail);
		}
		MPASS(n <= SGE_MAX_WR_NDESC);
		wr = &eq->desc[eq->pidx];
		IDXINCR(eq->pidx, n, eq->sidx);
		txp->npkt = 0;	/* emptied */

		MPASS(wr_can_update_eq(wr));
		set_txupdate_flags(txq, avail - n, wr);
		ring_eq_db(sc, eq, n);
		reclaim_tx_descs(txq, 32);
	}
	*coalescing = txp->npkt > 0;

	return (total);
}

static inline void
init_iq(struct sge_iq *iq, struct adapter *sc, int tmr_idx, int pktc_idx,
    int qsize)
{

	KASSERT(tmr_idx >= 0 && tmr_idx < SGE_NTIMERS,
	    ("%s: bad tmr_idx %d", __func__, tmr_idx));
	KASSERT(pktc_idx < SGE_NCOUNTERS,	/* -ve is ok, means don't use */
	    ("%s: bad pktc_idx %d", __func__, pktc_idx));

	iq->flags = 0;
	iq->adapter = sc;
	iq->intr_params = V_QINTR_TIMER_IDX(tmr_idx);
	iq->intr_pktc_idx = SGE_NCOUNTERS - 1;
	if (pktc_idx >= 0) {
		iq->intr_params |= F_QINTR_CNT_EN;
		iq->intr_pktc_idx = pktc_idx;
	}
	iq->qsize = roundup2(qsize, 16);	/* See FW_IQ_CMD/iqsize */
	iq->sidx = iq->qsize - sc->params.sge.spg_len / IQ_ESIZE;
}

static inline void
init_fl(struct adapter *sc, struct sge_fl *fl, int qsize, int maxp, char *name)
{

	fl->qsize = qsize;
	fl->sidx = qsize - sc->params.sge.spg_len / EQ_ESIZE;
	strlcpy(fl->lockname, name, sizeof(fl->lockname));
	if (sc->flags & BUF_PACKING_OK &&
	    ((!is_t4(sc) && buffer_packing) ||	/* T5+: enabled unless 0 */
	    (is_t4(sc) && buffer_packing == 1)))/* T4: disabled unless 1 */
		fl->flags |= FL_BUF_PACKING;
	fl->zidx = find_refill_source(sc, maxp, fl->flags & FL_BUF_PACKING);
	fl->safe_zidx = sc->sge.safe_zidx;
}

static inline void
init_eq(struct adapter *sc, struct sge_eq *eq, int eqtype, int qsize,
    uint8_t tx_chan, uint16_t iqid, char *name)
{
	KASSERT(eqtype <= EQ_TYPEMASK, ("%s: bad qtype %d", __func__, eqtype));

	eq->flags = eqtype & EQ_TYPEMASK;
	eq->tx_chan = tx_chan;
	eq->iqid = iqid;
	eq->sidx = qsize - sc->params.sge.spg_len / EQ_ESIZE;
	strlcpy(eq->lockname, name, sizeof(eq->lockname));
}

static int
alloc_ring(struct adapter *sc, size_t len, bus_dma_tag_t *tag,
    bus_dmamap_t *map, bus_addr_t *pa, void **va)
{
	int rc;

	rc = bus_dma_tag_create(sc->dmat, 512, 0, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, len, 1, len, 0, NULL, NULL, tag);
	if (rc != 0) {
		device_printf(sc->dev, "cannot allocate DMA tag: %d\n", rc);
		goto done;
	}

	rc = bus_dmamem_alloc(*tag, va,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO, map);
	if (rc != 0) {
		device_printf(sc->dev, "cannot allocate DMA memory: %d\n", rc);
		goto done;
	}

	rc = bus_dmamap_load(*tag, *map, *va, len, oneseg_dma_callback, pa, 0);
	if (rc != 0) {
		device_printf(sc->dev, "cannot load DMA map: %d\n", rc);
		goto done;
	}
done:
	if (rc)
		free_ring(sc, *tag, *map, *pa, *va);

	return (rc);
}

static int
free_ring(struct adapter *sc, bus_dma_tag_t tag, bus_dmamap_t map,
    bus_addr_t pa, void *va)
{
	if (pa)
		bus_dmamap_unload(tag, map);
	if (va)
		bus_dmamem_free(tag, va, map);
	if (tag)
		bus_dma_tag_destroy(tag);

	return (0);
}

/*
 * Allocates the ring for an ingress queue and an optional freelist.  If the
 * freelist is specified it will be allocated and then associated with the
 * ingress queue.
 *
 * Returns errno on failure.  Resources allocated up to that point may still be
 * allocated.  Caller is responsible for cleanup in case this function fails.
 *
 * If the ingress queue will take interrupts directly then the intr_idx
 * specifies the vector, starting from 0.  -1 means the interrupts for this
 * queue should be forwarded to the fwq.
 */
static int
alloc_iq_fl(struct vi_info *vi, struct sge_iq *iq, struct sge_fl *fl,
    int intr_idx, int cong)
{
	int rc, i, cntxt_id;
	size_t len;
	struct fw_iq_cmd c;
	struct port_info *pi = vi->pi;
	struct adapter *sc = iq->adapter;
	struct sge_params *sp = &sc->params.sge;
	__be32 v = 0;

	len = iq->qsize * IQ_ESIZE;
	rc = alloc_ring(sc, len, &iq->desc_tag, &iq->desc_map, &iq->ba,
	    (void **)&iq->desc);
	if (rc != 0)
		return (rc);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(sc->pf) |
	    V_FW_IQ_CMD_VFN(0));

	c.alloc_to_len16 = htobe32(F_FW_IQ_CMD_ALLOC | F_FW_IQ_CMD_IQSTART |
	    FW_LEN16(c));

	/* Special handling for firmware event queue */
	if (iq == &sc->sge.fwq)
		v |= F_FW_IQ_CMD_IQASYNCH;

	if (intr_idx < 0) {
		/* Forwarded interrupts, all headed to fwq */
		v |= F_FW_IQ_CMD_IQANDST;
		v |= V_FW_IQ_CMD_IQANDSTINDEX(sc->sge.fwq.cntxt_id);
	} else {
		KASSERT(intr_idx < sc->intr_count,
		    ("%s: invalid direct intr_idx %d", __func__, intr_idx));
		v |= V_FW_IQ_CMD_IQANDSTINDEX(intr_idx);
	}

	c.type_to_iqandstindex = htobe32(v |
	    V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
	    V_FW_IQ_CMD_VIID(vi->viid) |
	    V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_INTERRUPT));
	c.iqdroprss_to_iqesize = htobe16(V_FW_IQ_CMD_IQPCIECH(pi->tx_chan) |
	    F_FW_IQ_CMD_IQGTSMODE |
	    V_FW_IQ_CMD_IQINTCNTTHRESH(iq->intr_pktc_idx) |
	    V_FW_IQ_CMD_IQESIZE(ilog2(IQ_ESIZE) - 4));
	c.iqsize = htobe16(iq->qsize);
	c.iqaddr = htobe64(iq->ba);
	if (cong >= 0)
		c.iqns_to_fl0congen = htobe32(F_FW_IQ_CMD_IQFLINTCONGEN);

	if (fl) {
		mtx_init(&fl->fl_lock, fl->lockname, NULL, MTX_DEF);

		len = fl->qsize * EQ_ESIZE;
		rc = alloc_ring(sc, len, &fl->desc_tag, &fl->desc_map,
		    &fl->ba, (void **)&fl->desc);
		if (rc)
			return (rc);

		/* Allocate space for one software descriptor per buffer. */
		rc = alloc_fl_sdesc(fl);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to setup fl software descriptors: %d\n",
			    rc);
			return (rc);
		}

		if (fl->flags & FL_BUF_PACKING) {
			fl->lowat = roundup2(sp->fl_starve_threshold2, 8);
			fl->buf_boundary = sp->pack_boundary;
		} else {
			fl->lowat = roundup2(sp->fl_starve_threshold, 8);
			fl->buf_boundary = 16;
		}
		if (fl_pad && fl->buf_boundary < sp->pad_boundary)
			fl->buf_boundary = sp->pad_boundary;

		c.iqns_to_fl0congen |=
		    htobe32(V_FW_IQ_CMD_FL0HOSTFCMODE(X_HOSTFCMODE_NONE) |
			F_FW_IQ_CMD_FL0FETCHRO | F_FW_IQ_CMD_FL0DATARO |
			(fl_pad ? F_FW_IQ_CMD_FL0PADEN : 0) |
			(fl->flags & FL_BUF_PACKING ? F_FW_IQ_CMD_FL0PACKEN :
			    0));
		if (cong >= 0) {
			c.iqns_to_fl0congen |=
				htobe32(V_FW_IQ_CMD_FL0CNGCHMAP(cong) |
				    F_FW_IQ_CMD_FL0CONGCIF |
				    F_FW_IQ_CMD_FL0CONGEN);
		}
		c.fl0dcaen_to_fl0cidxfthresh =
		    htobe16(V_FW_IQ_CMD_FL0FBMIN(chip_id(sc) <= CHELSIO_T5 ?
			X_FETCHBURSTMIN_128B : X_FETCHBURSTMIN_64B_T6) |
			V_FW_IQ_CMD_FL0FBMAX(chip_id(sc) <= CHELSIO_T5 ?
			X_FETCHBURSTMAX_512B : X_FETCHBURSTMAX_256B));
		c.fl0size = htobe16(fl->qsize);
		c.fl0addr = htobe64(fl->ba);
	}

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create ingress queue: %d\n", rc);
		return (rc);
	}

	iq->cidx = 0;
	iq->gen = F_RSPD_GEN;
	iq->intr_next = iq->intr_params;
	iq->cntxt_id = be16toh(c.iqid);
	iq->abs_id = be16toh(c.physiqid);
	iq->flags |= IQ_ALLOCATED;

	cntxt_id = iq->cntxt_id - sc->sge.iq_start;
	if (cntxt_id >= sc->sge.iqmap_sz) {
		panic ("%s: iq->cntxt_id (%d) more than the max (%d)", __func__,
		    cntxt_id, sc->sge.iqmap_sz - 1);
	}
	sc->sge.iqmap[cntxt_id] = iq;

	if (fl) {
		u_int qid;

		iq->flags |= IQ_HAS_FL;
		fl->cntxt_id = be16toh(c.fl0id);
		fl->pidx = fl->cidx = 0;

		cntxt_id = fl->cntxt_id - sc->sge.eq_start;
		if (cntxt_id >= sc->sge.eqmap_sz) {
			panic("%s: fl->cntxt_id (%d) more than the max (%d)",
			    __func__, cntxt_id, sc->sge.eqmap_sz - 1);
		}
		sc->sge.eqmap[cntxt_id] = (void *)fl;

		qid = fl->cntxt_id;
		if (isset(&sc->doorbells, DOORBELL_UDB)) {
			uint32_t s_qpp = sc->params.sge.eq_s_qpp;
			uint32_t mask = (1 << s_qpp) - 1;
			volatile uint8_t *udb;

			udb = sc->udbs_base + UDBS_DB_OFFSET;
			udb += (qid >> s_qpp) << PAGE_SHIFT;
			qid &= mask;
			if (qid < PAGE_SIZE / UDBS_SEG_SIZE) {
				udb += qid << UDBS_SEG_SHIFT;
				qid = 0;
			}
			fl->udb = (volatile void *)udb;
		}
		fl->dbval = V_QID(qid) | sc->chip_params->sge_fl_db;

		FL_LOCK(fl);
		/* Enough to make sure the SGE doesn't think it's starved */
		refill_fl(sc, fl, fl->lowat);
		FL_UNLOCK(fl);
	}

	if (chip_id(sc) >= CHELSIO_T5 && !(sc->flags & IS_VF) && cong >= 0) {
		uint32_t param, val;

		param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
		    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
		    V_FW_PARAMS_PARAM_YZ(iq->cntxt_id);
		if (cong == 0)
			val = 1 << 19;
		else {
			val = 2 << 19;
			for (i = 0; i < 4; i++) {
				if (cong & (1 << i))
					val |= 1 << (i << 2);
			}
		}

		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &param, &val);
		if (rc != 0) {
			/* report error but carry on */
			device_printf(sc->dev,
			    "failed to set congestion manager context for "
			    "ingress queue %d: %d\n", iq->cntxt_id, rc);
		}
	}

	/* Enable IQ interrupts */
	atomic_store_rel_int(&iq->state, IQS_IDLE);
	t4_write_reg(sc, sc->sge_gts_reg, V_SEINTARM(iq->intr_params) |
	    V_INGRESSQID(iq->cntxt_id));

	return (0);
}

static int
free_iq_fl(struct vi_info *vi, struct sge_iq *iq, struct sge_fl *fl)
{
	int rc;
	struct adapter *sc = iq->adapter;
	device_t dev;

	if (sc == NULL)
		return (0);	/* nothing to do */

	dev = vi ? vi->dev : sc->dev;

	if (iq->flags & IQ_ALLOCATED) {
		rc = -t4_iq_free(sc, sc->mbox, sc->pf, 0,
		    FW_IQ_TYPE_FL_INT_CAP, iq->cntxt_id,
		    fl ? fl->cntxt_id : 0xffff, 0xffff);
		if (rc != 0) {
			device_printf(dev,
			    "failed to free queue %p: %d\n", iq, rc);
			return (rc);
		}
		iq->flags &= ~IQ_ALLOCATED;
	}

	free_ring(sc, iq->desc_tag, iq->desc_map, iq->ba, iq->desc);

	bzero(iq, sizeof(*iq));

	if (fl) {
		free_ring(sc, fl->desc_tag, fl->desc_map, fl->ba,
		    fl->desc);

		if (fl->sdesc)
			free_fl_sdesc(sc, fl);

		if (mtx_initialized(&fl->fl_lock))
			mtx_destroy(&fl->fl_lock);

		bzero(fl, sizeof(*fl));
	}

	return (0);
}

static void
add_iq_sysctls(struct sysctl_ctx_list *ctx, struct sysctl_oid *oid,
    struct sge_iq *iq)
{
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UAUTO(ctx, children, OID_AUTO, "ba", CTLFLAG_RD, &iq->ba,
	    "bus address of descriptor ring");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "dmalen", CTLFLAG_RD, NULL,
	    iq->qsize * IQ_ESIZE, "descriptor ring size in bytes");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &iq->abs_id, 0,
	    sysctl_uint16, "I", "absolute id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &iq->cntxt_id, 0,
	    sysctl_uint16, "I", "SGE context id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &iq->cidx, 0,
	    sysctl_uint16, "I", "consumer index");
}

static void
add_fl_sysctls(struct adapter *sc, struct sysctl_ctx_list *ctx,
    struct sysctl_oid *oid, struct sge_fl *fl)
{
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "fl",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "freelist");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UAUTO(ctx, children, OID_AUTO, "ba", CTLFLAG_RD,
	    &fl->ba, "bus address of descriptor ring");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "dmalen", CTLFLAG_RD, NULL,
	    fl->sidx * EQ_ESIZE + sc->params.sge.spg_len,
	    "desc ring size in bytes");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &fl->cntxt_id, 0,
	    sysctl_uint16, "I", "SGE context id of the freelist");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "padding", CTLFLAG_RD, NULL,
	    fl_pad ? 1 : 0, "padding enabled");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "packing", CTLFLAG_RD, NULL,
	    fl->flags & FL_BUF_PACKING ? 1 : 0, "packing enabled");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD, &fl->cidx,
	    0, "consumer index");
	if (fl->flags & FL_BUF_PACKING) {
		SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "rx_offset",
		    CTLFLAG_RD, &fl->rx_offset, 0, "packing rx offset");
	}
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "pidx", CTLFLAG_RD, &fl->pidx,
	    0, "producer index");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_allocated",
	    CTLFLAG_RD, &fl->cl_allocated, "# of clusters allocated");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_recycled",
	    CTLFLAG_RD, &fl->cl_recycled, "# of clusters recycled");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_fast_recycled",
	    CTLFLAG_RD, &fl->cl_fast_recycled, "# of clusters recycled (fast)");
}

static int
alloc_fwq(struct adapter *sc)
{
	int rc, intr_idx;
	struct sge_iq *fwq = &sc->sge.fwq;
	struct sysctl_oid *oid = device_get_sysctl_tree(sc->dev);
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	init_iq(fwq, sc, 0, 0, FW_IQ_QSIZE);
	if (sc->flags & IS_VF)
		intr_idx = 0;
	else
		intr_idx = sc->intr_count > 1 ? 1 : 0;
	rc = alloc_iq_fl(&sc->port[0]->vi[0], fwq, NULL, intr_idx, -1);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create firmware event queue: %d\n", rc);
		return (rc);
	}

	oid = SYSCTL_ADD_NODE(&sc->ctx, children, OID_AUTO, "fwq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "firmware event queue");
	add_iq_sysctls(&sc->ctx, oid, fwq);

	return (0);
}

static int
free_fwq(struct adapter *sc)
{
	return free_iq_fl(NULL, &sc->sge.fwq, NULL);
}

static int
alloc_ctrlq(struct adapter *sc, struct sge_wrq *ctrlq, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	char name[16];
	struct sysctl_oid_list *children;

	snprintf(name, sizeof(name), "%s ctrlq%d", device_get_nameunit(sc->dev),
	    idx);
	init_eq(sc, &ctrlq->eq, EQ_CTRL, CTRL_EQ_QSIZE, sc->port[idx]->tx_chan,
	    sc->sge.fwq.cntxt_id, name);

	children = SYSCTL_CHILDREN(oid);
	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&sc->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "ctrl queue");
	rc = alloc_wrq(sc, NULL, ctrlq, oid);

	return (rc);
}

int
tnl_cong(struct port_info *pi, int drop)
{

	if (drop == -1)
		return (-1);
	else if (drop == 1)
		return (0);
	else
		return (pi->rx_e_chan_map);
}

static int
alloc_rxq(struct vi_info *vi, struct sge_rxq *rxq, int intr_idx, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	struct adapter *sc = vi->adapter;
	struct sysctl_oid_list *children;
	char name[16];

	rc = alloc_iq_fl(vi, &rxq->iq, &rxq->fl, intr_idx,
	    tnl_cong(vi->pi, cong_drop));
	if (rc != 0)
		return (rc);

	if (idx == 0)
		sc->sge.iq_base = rxq->iq.abs_id - rxq->iq.cntxt_id;
	else
		KASSERT(rxq->iq.cntxt_id + sc->sge.iq_base == rxq->iq.abs_id,
		    ("iq_base mismatch"));
	KASSERT(sc->sge.iq_base == 0 || sc->flags & IS_VF,
	    ("PF with non-zero iq_base"));

	/*
	 * The freelist is just barely above the starvation threshold right now,
	 * fill it up a bit more.
	 */
	FL_LOCK(&rxq->fl);
	refill_fl(sc, &rxq->fl, 128);
	FL_UNLOCK(&rxq->fl);

#if defined(INET) || defined(INET6)
	rc = tcp_lro_init_args(&rxq->lro, vi->ifp, lro_entries, lro_mbufs);
	if (rc != 0)
		return (rc);
	MPASS(rxq->lro.ifp == vi->ifp);	/* also indicates LRO init'ed */

	if (vi->ifp->if_capenable & IFCAP_LRO)
		rxq->iq.flags |= IQ_LRO_ENABLED;
#endif
	if (vi->ifp->if_capenable & IFCAP_HWRXTSTMP)
		rxq->iq.flags |= IQ_RX_TIMESTAMP;
	rxq->ifp = vi->ifp;

	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queue");
	children = SYSCTL_CHILDREN(oid);

	add_iq_sysctls(&vi->ctx, oid, &rxq->iq);
#if defined(INET) || defined(INET6)
	SYSCTL_ADD_U64(&vi->ctx, children, OID_AUTO, "lro_queued", CTLFLAG_RD,
	    &rxq->lro.lro_queued, 0, NULL);
	SYSCTL_ADD_U64(&vi->ctx, children, OID_AUTO, "lro_flushed", CTLFLAG_RD,
	    &rxq->lro.lro_flushed, 0, NULL);
#endif
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "rxcsum", CTLFLAG_RD,
	    &rxq->rxcsum, "# of times hardware assisted with checksum");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "vlan_extraction",
	    CTLFLAG_RD, &rxq->vlan_extraction,
	    "# of times hardware extracted 802.1Q tag");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "vxlan_rxcsum",
	    CTLFLAG_RD, &rxq->vxlan_rxcsum,
	    "# of times hardware assisted with inner checksum (VXLAN) ");

	add_fl_sysctls(sc, &vi->ctx, oid, &rxq->fl);

	return (rc);
}

static int
free_rxq(struct vi_info *vi, struct sge_rxq *rxq)
{
	int rc;

#if defined(INET) || defined(INET6)
	if (rxq->lro.ifp) {
		tcp_lro_free(&rxq->lro);
		rxq->lro.ifp = NULL;
	}
#endif

	rc = free_iq_fl(vi, &rxq->iq, &rxq->fl);
	if (rc == 0)
		bzero(rxq, sizeof(*rxq));

	return (rc);
}

#ifdef TCP_OFFLOAD
static int
alloc_ofld_rxq(struct vi_info *vi, struct sge_ofld_rxq *ofld_rxq,
    int intr_idx, int idx, struct sysctl_oid *oid)
{
	struct port_info *pi = vi->pi;
	int rc;
	struct sysctl_oid_list *children;
	char name[16];

	rc = alloc_iq_fl(vi, &ofld_rxq->iq, &ofld_rxq->fl, intr_idx, 0);
	if (rc != 0)
		return (rc);

	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queue");
	add_iq_sysctls(&vi->ctx, oid, &ofld_rxq->iq);
	add_fl_sysctls(pi->adapter, &vi->ctx, oid, &ofld_rxq->fl);

	return (rc);
}

static int
free_ofld_rxq(struct vi_info *vi, struct sge_ofld_rxq *ofld_rxq)
{
	int rc;

	rc = free_iq_fl(vi, &ofld_rxq->iq, &ofld_rxq->fl);
	if (rc == 0)
		bzero(ofld_rxq, sizeof(*ofld_rxq));

	return (rc);
}
#endif

#ifdef DEV_NETMAP
static int
alloc_nm_rxq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq, int intr_idx,
    int idx, struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_oid_list *children;
	struct sysctl_ctx_list *ctx;
	char name[16];
	size_t len;
	struct adapter *sc = vi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);

	MPASS(na != NULL);

	len = vi->qsize_rxq * IQ_ESIZE;
	rc = alloc_ring(sc, len, &nm_rxq->iq_desc_tag, &nm_rxq->iq_desc_map,
	    &nm_rxq->iq_ba, (void **)&nm_rxq->iq_desc);
	if (rc != 0)
		return (rc);

	len = na->num_rx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	rc = alloc_ring(sc, len, &nm_rxq->fl_desc_tag, &nm_rxq->fl_desc_map,
	    &nm_rxq->fl_ba, (void **)&nm_rxq->fl_desc);
	if (rc != 0)
		return (rc);

	nm_rxq->vi = vi;
	nm_rxq->nid = idx;
	nm_rxq->iq_cidx = 0;
	nm_rxq->iq_sidx = vi->qsize_rxq - sc->params.sge.spg_len / IQ_ESIZE;
	nm_rxq->iq_gen = F_RSPD_GEN;
	nm_rxq->fl_pidx = nm_rxq->fl_cidx = 0;
	nm_rxq->fl_sidx = na->num_rx_desc;
	nm_rxq->fl_sidx2 = nm_rxq->fl_sidx;	/* copy for rxsync cacheline */
	nm_rxq->intr_idx = intr_idx;
	nm_rxq->iq_cntxt_id = INVALID_NM_RXQ_CNTXT_ID;

	ctx = &vi->ctx;
	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_rxq->iq_abs_id,
	    0, sysctl_uint16, "I", "absolute id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_rxq->iq_cntxt_id,
	    0, sysctl_uint16, "I", "SGE context id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_rxq->iq_cidx, 0,
	    sysctl_uint16, "I", "consumer index");

	children = SYSCTL_CHILDREN(oid);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "fl",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "freelist");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_rxq->fl_cntxt_id,
	    0, sysctl_uint16, "I", "SGE context id of the freelist");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD,
	    &nm_rxq->fl_cidx, 0, "consumer index");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "pidx", CTLFLAG_RD,
	    &nm_rxq->fl_pidx, 0, "producer index");

	return (rc);
}


static int
free_nm_rxq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq)
{
	struct adapter *sc = vi->adapter;

	if (vi->flags & VI_INIT_DONE)
		MPASS(nm_rxq->iq_cntxt_id == INVALID_NM_RXQ_CNTXT_ID);
	else
		MPASS(nm_rxq->iq_cntxt_id == 0);

	free_ring(sc, nm_rxq->iq_desc_tag, nm_rxq->iq_desc_map, nm_rxq->iq_ba,
	    nm_rxq->iq_desc);
	free_ring(sc, nm_rxq->fl_desc_tag, nm_rxq->fl_desc_map, nm_rxq->fl_ba,
	    nm_rxq->fl_desc);

	return (0);
}

static int
alloc_nm_txq(struct vi_info *vi, struct sge_nm_txq *nm_txq, int iqidx, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	size_t len;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);
	char name[16];
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	len = na->num_tx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	rc = alloc_ring(sc, len, &nm_txq->desc_tag, &nm_txq->desc_map,
	    &nm_txq->ba, (void **)&nm_txq->desc);
	if (rc)
		return (rc);

	nm_txq->pidx = nm_txq->cidx = 0;
	nm_txq->sidx = na->num_tx_desc;
	nm_txq->nid = idx;
	nm_txq->iqidx = iqidx;
	nm_txq->cpl_ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(sc->pf) |
	    V_TXPKT_VF(vi->vin) | V_TXPKT_VF_VLD(vi->vfvld));
	if (sc->params.fw_vers >= FW_VERSION32(1, 24, 11, 0))
		nm_txq->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS2_WR));
	else
		nm_txq->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR));
	nm_txq->cntxt_id = INVALID_NM_TXQ_CNTXT_ID;

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "netmap tx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UINT(&vi->ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &nm_txq->cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(&vi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_txq->cidx, 0,
	    sysctl_uint16, "I", "consumer index");
	SYSCTL_ADD_PROC(&vi->ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &nm_txq->pidx, 0,
	    sysctl_uint16, "I", "producer index");

	return (rc);
}

static int
free_nm_txq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	struct adapter *sc = vi->adapter;

	if (vi->flags & VI_INIT_DONE)
		MPASS(nm_txq->cntxt_id == INVALID_NM_TXQ_CNTXT_ID);
	else
		MPASS(nm_txq->cntxt_id == 0);

	free_ring(sc, nm_txq->desc_tag, nm_txq->desc_map, nm_txq->ba,
	    nm_txq->desc);

	return (0);
}
#endif

/*
 * Returns a reasonable automatic cidx flush threshold for a given queue size.
 */
static u_int
qsize_to_fthresh(int qsize)
{
	u_int fthresh;

	while (!powerof2(qsize))
		qsize++;
	fthresh = ilog2(qsize);
	if (fthresh > X_CIDXFLUSHTHRESH_128)
		fthresh = X_CIDXFLUSHTHRESH_128;

	return (fthresh);
}

static int
ctrl_eq_alloc(struct adapter *sc, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_ctrl_cmd c;
	int qsize = eq->sidx + sc->params.sge.spg_len / EQ_ESIZE;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_CTRL_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_CTRL_CMD_PFN(sc->pf) |
	    V_FW_EQ_CTRL_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_CTRL_CMD_ALLOC |
	    F_FW_EQ_CTRL_CMD_EQSTART | FW_LEN16(c));
	c.cmpliqid_eqid = htonl(V_FW_EQ_CTRL_CMD_CMPLIQID(eq->iqid));
	c.physeqid_pkd = htobe32(0);
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_CTRL_CMD_HOSTFCMODE(X_HOSTFCMODE_STATUS_PAGE) |
		V_FW_EQ_CTRL_CMD_PCIECHN(eq->tx_chan) |
		F_FW_EQ_CTRL_CMD_FETCHRO | V_FW_EQ_CTRL_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_CTRL_CMD_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_EQ_CTRL_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_CTRL_CMD_CIDXFTHRESH(qsize_to_fthresh(qsize)) |
		V_FW_EQ_CTRL_CMD_EQSIZE(qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create control queue %d: %d\n", eq->tx_chan, rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_CTRL_CMD_EQID(be32toh(c.cmpliqid_eqid));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.eqmap_sz)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.eqmap_sz - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}

static int
eth_eq_alloc(struct adapter *sc, struct vi_info *vi, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_eth_cmd c;
	int qsize = eq->sidx + sc->params.sge.spg_len / EQ_ESIZE;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_ETH_CMD_PFN(sc->pf) |
	    V_FW_EQ_ETH_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_ETH_CMD_ALLOC |
	    F_FW_EQ_ETH_CMD_EQSTART | FW_LEN16(c));
	c.autoequiqe_to_viid = htobe32(F_FW_EQ_ETH_CMD_AUTOEQUIQE |
	    F_FW_EQ_ETH_CMD_AUTOEQUEQE | V_FW_EQ_ETH_CMD_VIID(vi->viid));
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_ETH_CMD_HOSTFCMODE(X_HOSTFCMODE_NONE) |
		V_FW_EQ_ETH_CMD_PCIECHN(eq->tx_chan) | F_FW_EQ_ETH_CMD_FETCHRO |
		V_FW_EQ_ETH_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_ETH_CMD_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_EQ_ETH_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_ETH_CMD_EQSIZE(qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(vi->dev,
		    "failed to create Ethernet egress queue: %d\n", rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_ETH_CMD_EQID(be32toh(c.eqid_pkd));
	eq->abs_id = G_FW_EQ_ETH_CMD_PHYSEQID(be32toh(c.physeqid_pkd));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.eqmap_sz)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.eqmap_sz - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}

#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
static int
ofld_eq_alloc(struct adapter *sc, struct vi_info *vi, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_ofld_cmd c;
	int qsize = eq->sidx + sc->params.sge.spg_len / EQ_ESIZE;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_OFLD_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_OFLD_CMD_PFN(sc->pf) |
	    V_FW_EQ_OFLD_CMD_VFN(0));
	c.alloc_to_len16 = htonl(F_FW_EQ_OFLD_CMD_ALLOC |
	    F_FW_EQ_OFLD_CMD_EQSTART | FW_LEN16(c));
	c.fetchszm_to_iqid =
		htonl(V_FW_EQ_OFLD_CMD_HOSTFCMODE(X_HOSTFCMODE_STATUS_PAGE) |
		    V_FW_EQ_OFLD_CMD_PCIECHN(eq->tx_chan) |
		    F_FW_EQ_OFLD_CMD_FETCHRO | V_FW_EQ_OFLD_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_OFLD_CMD_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_EQ_OFLD_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_OFLD_CMD_CIDXFTHRESH(qsize_to_fthresh(qsize)) |
		V_FW_EQ_OFLD_CMD_EQSIZE(qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(vi->dev,
		    "failed to create egress queue for TCP offload: %d\n", rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_OFLD_CMD_EQID(be32toh(c.eqid_pkd));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.eqmap_sz)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.eqmap_sz - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}
#endif

static int
alloc_eq(struct adapter *sc, struct vi_info *vi, struct sge_eq *eq)
{
	int rc, qsize;
	size_t len;

	mtx_init(&eq->eq_lock, eq->lockname, NULL, MTX_DEF);

	qsize = eq->sidx + sc->params.sge.spg_len / EQ_ESIZE;
	len = qsize * EQ_ESIZE;
	rc = alloc_ring(sc, len, &eq->desc_tag, &eq->desc_map,
	    &eq->ba, (void **)&eq->desc);
	if (rc)
		return (rc);

	eq->pidx = eq->cidx = eq->dbidx = 0;
	/* Note that equeqidx is not used with sge_wrq (OFLD/CTRL) queues. */
	eq->equeqidx = 0;
	eq->doorbells = sc->doorbells;

	switch (eq->flags & EQ_TYPEMASK) {
	case EQ_CTRL:
		rc = ctrl_eq_alloc(sc, eq);
		break;

	case EQ_ETH:
		rc = eth_eq_alloc(sc, vi, eq);
		break;

#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
	case EQ_OFLD:
		rc = ofld_eq_alloc(sc, vi, eq);
		break;
#endif

	default:
		panic("%s: invalid eq type %d.", __func__,
		    eq->flags & EQ_TYPEMASK);
	}
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to allocate egress queue(%d): %d\n",
		    eq->flags & EQ_TYPEMASK, rc);
	}

	if (isset(&eq->doorbells, DOORBELL_UDB) ||
	    isset(&eq->doorbells, DOORBELL_UDBWC) ||
	    isset(&eq->doorbells, DOORBELL_WCWR)) {
		uint32_t s_qpp = sc->params.sge.eq_s_qpp;
		uint32_t mask = (1 << s_qpp) - 1;
		volatile uint8_t *udb;

		udb = sc->udbs_base + UDBS_DB_OFFSET;
		udb += (eq->cntxt_id >> s_qpp) << PAGE_SHIFT;	/* pg offset */
		eq->udb_qid = eq->cntxt_id & mask;		/* id in page */
		if (eq->udb_qid >= PAGE_SIZE / UDBS_SEG_SIZE)
	    		clrbit(&eq->doorbells, DOORBELL_WCWR);
		else {
			udb += eq->udb_qid << UDBS_SEG_SHIFT;	/* seg offset */
			eq->udb_qid = 0;
		}
		eq->udb = (volatile void *)udb;
	}

	return (rc);
}

static int
free_eq(struct adapter *sc, struct sge_eq *eq)
{
	int rc;

	if (eq->flags & EQ_ALLOCATED) {
		switch (eq->flags & EQ_TYPEMASK) {
		case EQ_CTRL:
			rc = -t4_ctrl_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;

		case EQ_ETH:
			rc = -t4_eth_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;

#if defined(TCP_OFFLOAD) || defined(RATELIMIT)
		case EQ_OFLD:
			rc = -t4_ofld_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;
#endif

		default:
			panic("%s: invalid eq type %d.", __func__,
			    eq->flags & EQ_TYPEMASK);
		}
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to free egress queue (%d): %d\n",
			    eq->flags & EQ_TYPEMASK, rc);
			return (rc);
		}
		eq->flags &= ~EQ_ALLOCATED;
	}

	free_ring(sc, eq->desc_tag, eq->desc_map, eq->ba, eq->desc);

	if (mtx_initialized(&eq->eq_lock))
		mtx_destroy(&eq->eq_lock);

	bzero(eq, sizeof(*eq));
	return (0);
}

static int
alloc_wrq(struct adapter *sc, struct vi_info *vi, struct sge_wrq *wrq,
    struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_ctx_list *ctx = vi ? &vi->ctx : &sc->ctx;
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	rc = alloc_eq(sc, vi, &wrq->eq);
	if (rc)
		return (rc);

	wrq->adapter = sc;
	TASK_INIT(&wrq->wrq_tx_task, 0, wrq_tx_drain, wrq);
	TAILQ_INIT(&wrq->incomplete_wrs);
	STAILQ_INIT(&wrq->wr_list);
	wrq->nwr_pending = 0;
	wrq->ndesc_needed = 0;

	SYSCTL_ADD_UAUTO(ctx, children, OID_AUTO, "ba", CTLFLAG_RD,
	    &wrq->eq.ba, "bus address of descriptor ring");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "dmalen", CTLFLAG_RD, NULL,
	    wrq->eq.sidx * EQ_ESIZE + sc->params.sge.spg_len,
	    "desc ring size in bytes");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &wrq->eq.cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &wrq->eq.cidx, 0,
	    sysctl_uint16, "I", "consumer index");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &wrq->eq.pidx, 0,
	    sysctl_uint16, "I", "producer index");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "sidx", CTLFLAG_RD, NULL,
	    wrq->eq.sidx, "status page index");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "tx_wrs_direct", CTLFLAG_RD,
	    &wrq->tx_wrs_direct, "# of work requests (direct)");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "tx_wrs_copied", CTLFLAG_RD,
	    &wrq->tx_wrs_copied, "# of work requests (copied)");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "tx_wrs_sspace", CTLFLAG_RD,
	    &wrq->tx_wrs_ss, "# of work requests (copied from scratch space)");

	return (rc);
}

static int
free_wrq(struct adapter *sc, struct sge_wrq *wrq)
{
	int rc;

	rc = free_eq(sc, &wrq->eq);
	if (rc)
		return (rc);

	bzero(wrq, sizeof(*wrq));
	return (0);
}

static int
alloc_txq(struct vi_info *vi, struct sge_txq *txq, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct sge_eq *eq = &txq->eq;
	struct txpkts *txp;
	char name[16];
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	rc = mp_ring_alloc(&txq->r, eq->sidx, txq, eth_tx, can_resume_eth_tx,
	    M_CXGBE, &eq->eq_lock, M_WAITOK);
	if (rc != 0) {
		device_printf(sc->dev, "failed to allocate mp_ring: %d\n", rc);
		return (rc);
	}

	rc = alloc_eq(sc, vi, eq);
	if (rc != 0) {
		mp_ring_free(txq->r);
		txq->r = NULL;
		return (rc);
	}

	/* Can't fail after this point. */

	if (idx == 0)
		sc->sge.eq_base = eq->abs_id - eq->cntxt_id;
	else
		KASSERT(eq->cntxt_id + sc->sge.eq_base == eq->abs_id,
		    ("eq_base mismatch"));
	KASSERT(sc->sge.eq_base == 0 || sc->flags & IS_VF,
	    ("PF with non-zero eq_base"));

	TASK_INIT(&txq->tx_reclaim_task, 0, tx_reclaim, eq);
	txq->ifp = vi->ifp;
	txq->gl = sglist_alloc(TX_SGL_SEGS, M_WAITOK);
	if (vi->flags & TX_USES_VM_WR)
		txq->cpl_ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT_XT) |
		    V_TXPKT_INTF(pi->tx_chan));
	else
		txq->cpl_ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT_XT) |
		    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(sc->pf) |
		    V_TXPKT_VF(vi->vin) | V_TXPKT_VF_VLD(vi->vfvld));
	txq->tc_idx = -1;
	txq->sdesc = malloc(eq->sidx * sizeof(struct tx_sdesc), M_CXGBE,
	    M_ZERO | M_WAITOK);

	txp = &txq->txp;
	txp->score = 5;
	MPASS(nitems(txp->mb) >= sc->params.max_pkts_per_eth_tx_pkts_wr);
	txq->txp.max_npkt = min(nitems(txp->mb),
	    sc->params.max_pkts_per_eth_tx_pkts_wr);
	if (vi->flags & TX_USES_VM_WR && !(sc->flags & IS_VF))
		txq->txp.max_npkt--;

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "tx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UAUTO(&vi->ctx, children, OID_AUTO, "ba", CTLFLAG_RD,
	    &eq->ba, "bus address of descriptor ring");
	SYSCTL_ADD_INT(&vi->ctx, children, OID_AUTO, "dmalen", CTLFLAG_RD, NULL,
	    eq->sidx * EQ_ESIZE + sc->params.sge.spg_len,
	    "desc ring size in bytes");
	SYSCTL_ADD_UINT(&vi->ctx, children, OID_AUTO, "abs_id", CTLFLAG_RD,
	    &eq->abs_id, 0, "absolute id of the queue");
	SYSCTL_ADD_UINT(&vi->ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &eq->cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(&vi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &eq->cidx, 0,
	    sysctl_uint16, "I", "consumer index");
	SYSCTL_ADD_PROC(&vi->ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &eq->pidx, 0,
	    sysctl_uint16, "I", "producer index");
	SYSCTL_ADD_INT(&vi->ctx, children, OID_AUTO, "sidx", CTLFLAG_RD, NULL,
	    eq->sidx, "status page index");

	SYSCTL_ADD_PROC(&vi->ctx, children, OID_AUTO, "tc",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, vi, idx, sysctl_tc,
	    "I", "traffic class (-1 means none)");

	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txcsum", CTLFLAG_RD,
	    &txq->txcsum, "# of times hardware assisted with checksum");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "vlan_insertion",
	    CTLFLAG_RD, &txq->vlan_insertion,
	    "# of times hardware inserted 802.1Q tag");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "tso_wrs", CTLFLAG_RD,
	    &txq->tso_wrs, "# of TSO work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "imm_wrs", CTLFLAG_RD,
	    &txq->imm_wrs, "# of work requests with immediate data");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "sgl_wrs", CTLFLAG_RD,
	    &txq->sgl_wrs, "# of work requests with direct SGL");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txpkt_wrs", CTLFLAG_RD,
	    &txq->txpkt_wrs, "# of txpkt work requests (one pkt/WR)");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txpkts0_wrs",
	    CTLFLAG_RD, &txq->txpkts0_wrs,
	    "# of txpkts (type 0) work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txpkts1_wrs",
	    CTLFLAG_RD, &txq->txpkts1_wrs,
	    "# of txpkts (type 1) work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txpkts0_pkts",
	    CTLFLAG_RD, &txq->txpkts0_pkts,
	    "# of frames tx'd using type0 txpkts work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "txpkts1_pkts",
	    CTLFLAG_RD, &txq->txpkts1_pkts,
	    "# of frames tx'd using type1 txpkts work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "raw_wrs", CTLFLAG_RD,
	    &txq->raw_wrs, "# of raw work requests (non-packets)");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "vxlan_tso_wrs",
	    CTLFLAG_RD, &txq->vxlan_tso_wrs, "# of VXLAN TSO work requests");
	SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO, "vxlan_txcsum",
	    CTLFLAG_RD, &txq->vxlan_txcsum,
	    "# of times hardware assisted with inner checksums (VXLAN)");

#ifdef KERN_TLS
	if (sc->flags & KERN_TLS_OK) {
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_records", CTLFLAG_RD, &txq->kern_tls_records,
		    "# of NIC TLS records transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_short", CTLFLAG_RD, &txq->kern_tls_short,
		    "# of short NIC TLS records transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_partial", CTLFLAG_RD, &txq->kern_tls_partial,
		    "# of partial NIC TLS records transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_full", CTLFLAG_RD, &txq->kern_tls_full,
		    "# of full NIC TLS records transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_octets", CTLFLAG_RD, &txq->kern_tls_octets,
		    "# of payload octets in transmitted NIC TLS records");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_waste", CTLFLAG_RD, &txq->kern_tls_waste,
		    "# of octets DMAd but not transmitted in NIC TLS records");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_options", CTLFLAG_RD, &txq->kern_tls_options,
		    "# of NIC TLS options-only packets transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_header", CTLFLAG_RD, &txq->kern_tls_header,
		    "# of NIC TLS header-only packets transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_fin", CTLFLAG_RD, &txq->kern_tls_fin,
		    "# of NIC TLS FIN-only packets transmitted");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_fin_short", CTLFLAG_RD, &txq->kern_tls_fin_short,
		    "# of NIC TLS padded FIN packets on short TLS records");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_cbc", CTLFLAG_RD, &txq->kern_tls_cbc,
		    "# of NIC TLS sessions using AES-CBC");
		SYSCTL_ADD_UQUAD(&vi->ctx, children, OID_AUTO,
		    "kern_tls_gcm", CTLFLAG_RD, &txq->kern_tls_gcm,
		    "# of NIC TLS sessions using AES-GCM");
	}
#endif
	mp_ring_sysctls(txq->r, &vi->ctx, children);

	return (0);
}

static int
free_txq(struct vi_info *vi, struct sge_txq *txq)
{
	int rc;
	struct adapter *sc = vi->adapter;
	struct sge_eq *eq = &txq->eq;

	rc = free_eq(sc, eq);
	if (rc)
		return (rc);

	sglist_free(txq->gl);
	free(txq->sdesc, M_CXGBE);
	mp_ring_free(txq->r);

	bzero(txq, sizeof(*txq));
	return (0);
}

static void
oneseg_dma_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *ba = arg;

	KASSERT(nseg == 1,
	    ("%s meant for single segment mappings only.", __func__));

	*ba = error ? 0 : segs->ds_addr;
}

static inline void
ring_fl_db(struct adapter *sc, struct sge_fl *fl)
{
	uint32_t n, v;

	n = IDXDIFF(fl->pidx >> 3, fl->dbidx, fl->sidx);
	MPASS(n > 0);

	wmb();
	v = fl->dbval | V_PIDX(n);
	if (fl->udb)
		*fl->udb = htole32(v);
	else
		t4_write_reg(sc, sc->sge_kdoorbell_reg, v);
	IDXINCR(fl->dbidx, n, fl->sidx);
}

/*
 * Fills up the freelist by allocating up to 'n' buffers.  Buffers that are
 * recycled do not count towards this allocation budget.
 *
 * Returns non-zero to indicate that this freelist should be added to the list
 * of starving freelists.
 */
static int
refill_fl(struct adapter *sc, struct sge_fl *fl, int n)
{
	__be64 *d;
	struct fl_sdesc *sd;
	uintptr_t pa;
	caddr_t cl;
	struct rx_buf_info *rxb;
	struct cluster_metadata *clm;
	uint16_t max_pidx;
	uint16_t hw_cidx = fl->hw_cidx;		/* stable snapshot */

	FL_LOCK_ASSERT_OWNED(fl);

	/*
	 * We always stop at the beginning of the hardware descriptor that's just
	 * before the one with the hw cidx.  This is to avoid hw pidx = hw cidx,
	 * which would mean an empty freelist to the chip.
	 */
	max_pidx = __predict_false(hw_cidx == 0) ? fl->sidx - 1 : hw_cidx - 1;
	if (fl->pidx == max_pidx * 8)
		return (0);

	d = &fl->desc[fl->pidx];
	sd = &fl->sdesc[fl->pidx];

	while (n > 0) {

		if (sd->cl != NULL) {

			if (sd->nmbuf == 0) {
				/*
				 * Fast recycle without involving any atomics on
				 * the cluster's metadata (if the cluster has
				 * metadata).  This happens when all frames
				 * received in the cluster were small enough to
				 * fit within a single mbuf each.
				 */
				fl->cl_fast_recycled++;
				goto recycled;
			}

			/*
			 * Cluster is guaranteed to have metadata.  Clusters
			 * without metadata always take the fast recycle path
			 * when they're recycled.
			 */
			clm = cl_metadata(sd);
			MPASS(clm != NULL);

			if (atomic_fetchadd_int(&clm->refcount, -1) == 1) {
				fl->cl_recycled++;
				counter_u64_add(extfree_rels, 1);
				goto recycled;
			}
			sd->cl = NULL;	/* gave up my reference */
		}
		MPASS(sd->cl == NULL);
		rxb = &sc->sge.rx_buf_info[fl->zidx];
		cl = uma_zalloc(rxb->zone, M_NOWAIT);
		if (__predict_false(cl == NULL)) {
			if (fl->zidx != fl->safe_zidx) {
				rxb = &sc->sge.rx_buf_info[fl->safe_zidx];
				cl = uma_zalloc(rxb->zone, M_NOWAIT);
			}
			if (cl == NULL)
				break;
		}
		fl->cl_allocated++;
		n--;

		pa = pmap_kextract((vm_offset_t)cl);
		sd->cl = cl;
		sd->zidx = fl->zidx;

		if (fl->flags & FL_BUF_PACKING) {
			*d = htobe64(pa | rxb->hwidx2);
			sd->moff = rxb->size2;
		} else {
			*d = htobe64(pa | rxb->hwidx1);
			sd->moff = 0;
		}
recycled:
		sd->nmbuf = 0;
		d++;
		sd++;
		if (__predict_false((++fl->pidx & 7) == 0)) {
			uint16_t pidx = fl->pidx >> 3;

			if (__predict_false(pidx == fl->sidx)) {
				fl->pidx = 0;
				pidx = 0;
				sd = fl->sdesc;
				d = fl->desc;
			}
			if (n < 8 || pidx == max_pidx)
				break;

			if (IDXDIFF(pidx, fl->dbidx, fl->sidx) >= 4)
				ring_fl_db(sc, fl);
		}
	}

	if ((fl->pidx >> 3) != fl->dbidx)
		ring_fl_db(sc, fl);

	return (FL_RUNNING_LOW(fl) && !(fl->flags & FL_STARVING));
}

/*
 * Attempt to refill all starving freelists.
 */
static void
refill_sfl(void *arg)
{
	struct adapter *sc = arg;
	struct sge_fl *fl, *fl_temp;

	mtx_assert(&sc->sfl_lock, MA_OWNED);
	TAILQ_FOREACH_SAFE(fl, &sc->sfl, link, fl_temp) {
		FL_LOCK(fl);
		refill_fl(sc, fl, 64);
		if (FL_NOT_RUNNING_LOW(fl) || fl->flags & FL_DOOMED) {
			TAILQ_REMOVE(&sc->sfl, fl, link);
			fl->flags &= ~FL_STARVING;
		}
		FL_UNLOCK(fl);
	}

	if (!TAILQ_EMPTY(&sc->sfl))
		callout_schedule(&sc->sfl_callout, hz / 5);
}

static int
alloc_fl_sdesc(struct sge_fl *fl)
{

	fl->sdesc = malloc(fl->sidx * 8 * sizeof(struct fl_sdesc), M_CXGBE,
	    M_ZERO | M_WAITOK);

	return (0);
}

static void
free_fl_sdesc(struct adapter *sc, struct sge_fl *fl)
{
	struct fl_sdesc *sd;
	struct cluster_metadata *clm;
	int i;

	sd = fl->sdesc;
	for (i = 0; i < fl->sidx * 8; i++, sd++) {
		if (sd->cl == NULL)
			continue;

		if (sd->nmbuf == 0)
			uma_zfree(sc->sge.rx_buf_info[sd->zidx].zone, sd->cl);
		else if (fl->flags & FL_BUF_PACKING) {
			clm = cl_metadata(sd);
			if (atomic_fetchadd_int(&clm->refcount, -1) == 1) {
				uma_zfree(sc->sge.rx_buf_info[sd->zidx].zone,
				    sd->cl);
				counter_u64_add(extfree_rels, 1);
			}
		}
		sd->cl = NULL;
	}

	free(fl->sdesc, M_CXGBE);
	fl->sdesc = NULL;
}

static inline void
get_pkt_gl(struct mbuf *m, struct sglist *gl)
{
	int rc;

	M_ASSERTPKTHDR(m);

	sglist_reset(gl);
	rc = sglist_append_mbuf(gl, m);
	if (__predict_false(rc != 0)) {
		panic("%s: mbuf %p (%d segs) was vetted earlier but now fails "
		    "with %d.", __func__, m, mbuf_nsegs(m), rc);
	}

	KASSERT(gl->sg_nseg == mbuf_nsegs(m),
	    ("%s: nsegs changed for mbuf %p from %d to %d", __func__, m,
	    mbuf_nsegs(m), gl->sg_nseg));
#if 0	/* vm_wr not readily available here. */
	KASSERT(gl->sg_nseg > 0 && gl->sg_nseg <= max_nsegs_allowed(m, vm_wr),
	    ("%s: %d segments, should have been 1 <= nsegs <= %d", __func__,
		gl->sg_nseg, max_nsegs_allowed(m, vm_wr)));
#endif
}

/*
 * len16 for a txpkt WR with a GL.  Includes the firmware work request header.
 */
static inline u_int
txpkt_len16(u_int nsegs, const u_int extra)
{
	u_int n;

	MPASS(nsegs > 0);

	nsegs--; /* first segment is part of ulptx_sgl */
	n = extra + sizeof(struct fw_eth_tx_pkt_wr) +
	    sizeof(struct cpl_tx_pkt_core) +
	    sizeof(struct ulptx_sgl) + 8 * ((3 * nsegs) / 2 + (nsegs & 1));

	return (howmany(n, 16));
}

/*
 * len16 for a txpkt_vm WR with a GL.  Includes the firmware work
 * request header.
 */
static inline u_int
txpkt_vm_len16(u_int nsegs, const u_int extra)
{
	u_int n;

	MPASS(nsegs > 0);

	nsegs--; /* first segment is part of ulptx_sgl */
	n = extra + sizeof(struct fw_eth_tx_pkt_vm_wr) +
	    sizeof(struct cpl_tx_pkt_core) +
	    sizeof(struct ulptx_sgl) + 8 * ((3 * nsegs) / 2 + (nsegs & 1));

	return (howmany(n, 16));
}

static inline void
calculate_mbuf_len16(struct mbuf *m, bool vm_wr)
{
	const int lso = sizeof(struct cpl_tx_pkt_lso_core);
	const int tnl_lso = sizeof(struct cpl_tx_tnl_lso);

	if (vm_wr) {
		if (needs_tso(m))
			set_mbuf_len16(m, txpkt_vm_len16(mbuf_nsegs(m), lso));
		else
			set_mbuf_len16(m, txpkt_vm_len16(mbuf_nsegs(m), 0));
		return;
	}

	if (needs_tso(m)) {
		if (needs_vxlan_tso(m))
			set_mbuf_len16(m, txpkt_len16(mbuf_nsegs(m), tnl_lso));
		else
			set_mbuf_len16(m, txpkt_len16(mbuf_nsegs(m), lso));
	} else
		set_mbuf_len16(m, txpkt_len16(mbuf_nsegs(m), 0));
}

/*
 * len16 for a txpkts type 0 WR with a GL.  Does not include the firmware work
 * request header.
 */
static inline u_int
txpkts0_len16(u_int nsegs)
{
	u_int n;

	MPASS(nsegs > 0);

	nsegs--; /* first segment is part of ulptx_sgl */
	n = sizeof(struct ulp_txpkt) + sizeof(struct ulptx_idata) +
	    sizeof(struct cpl_tx_pkt_core) + sizeof(struct ulptx_sgl) +
	    8 * ((3 * nsegs) / 2 + (nsegs & 1));

	return (howmany(n, 16));
}

/*
 * len16 for a txpkts type 1 WR with a GL.  Does not include the firmware work
 * request header.
 */
static inline u_int
txpkts1_len16(void)
{
	u_int n;

	n = sizeof(struct cpl_tx_pkt_core) + sizeof(struct ulptx_sgl);

	return (howmany(n, 16));
}

static inline u_int
imm_payload(u_int ndesc)
{
	u_int n;

	n = ndesc * EQ_ESIZE - sizeof(struct fw_eth_tx_pkt_wr) -
	    sizeof(struct cpl_tx_pkt_core);

	return (n);
}

static inline uint64_t
csum_to_ctrl(struct adapter *sc, struct mbuf *m)
{
	uint64_t ctrl;
	int csum_type, l2hlen, l3hlen;
	int x, y;
	static const int csum_types[3][2] = {
		{TX_CSUM_TCPIP, TX_CSUM_TCPIP6},
		{TX_CSUM_UDPIP, TX_CSUM_UDPIP6},
		{TX_CSUM_IP, 0}
	};

	M_ASSERTPKTHDR(m);

	if (!needs_hwcsum(m))
		return (F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS);

	MPASS(m->m_pkthdr.l2hlen >= ETHER_HDR_LEN);
	MPASS(m->m_pkthdr.l3hlen >= sizeof(struct ip));

	if (needs_vxlan_csum(m)) {
		MPASS(m->m_pkthdr.l4hlen > 0);
		MPASS(m->m_pkthdr.l5hlen > 0);
		MPASS(m->m_pkthdr.inner_l2hlen >= ETHER_HDR_LEN);
		MPASS(m->m_pkthdr.inner_l3hlen >= sizeof(struct ip));

		l2hlen = m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen +
		    m->m_pkthdr.l4hlen + m->m_pkthdr.l5hlen +
		    m->m_pkthdr.inner_l2hlen - ETHER_HDR_LEN;
		l3hlen = m->m_pkthdr.inner_l3hlen;
	} else {
		l2hlen = m->m_pkthdr.l2hlen - ETHER_HDR_LEN;
		l3hlen = m->m_pkthdr.l3hlen;
	}

	ctrl = 0;
	if (!needs_l3_csum(m))
		ctrl |= F_TXPKT_IPCSUM_DIS;

	if (m->m_pkthdr.csum_flags & (CSUM_IP_TCP | CSUM_INNER_IP_TCP |
	    CSUM_IP6_TCP | CSUM_INNER_IP6_TCP))
		x = 0;	/* TCP */
	else if (m->m_pkthdr.csum_flags & (CSUM_IP_UDP | CSUM_INNER_IP_UDP |
	    CSUM_IP6_UDP | CSUM_INNER_IP6_UDP))
		x = 1;	/* UDP */
	else
		x = 2;

	if (m->m_pkthdr.csum_flags & (CSUM_IP | CSUM_IP_TCP | CSUM_IP_UDP |
	    CSUM_INNER_IP | CSUM_INNER_IP_TCP | CSUM_INNER_IP_UDP))
		y = 0;	/* IPv4 */
	else {
		MPASS(m->m_pkthdr.csum_flags & (CSUM_IP6_TCP | CSUM_IP6_UDP |
		    CSUM_INNER_IP6_TCP | CSUM_INNER_IP6_UDP));
		y = 1;	/* IPv6 */
	}
	/*
	 * needs_hwcsum returned true earlier so there must be some kind of
	 * checksum to calculate.
	 */
	csum_type = csum_types[x][y];
	MPASS(csum_type != 0);
	if (csum_type == TX_CSUM_IP)
		ctrl |= F_TXPKT_L4CSUM_DIS;
	ctrl |= V_TXPKT_CSUM_TYPE(csum_type) | V_TXPKT_IPHDR_LEN(l3hlen);
	if (chip_id(sc) <= CHELSIO_T5)
		ctrl |= V_TXPKT_ETHHDR_LEN(l2hlen);
	else
		ctrl |= V_T6_TXPKT_ETHHDR_LEN(l2hlen);

	return (ctrl);
}

static inline void *
write_lso_cpl(void *cpl, struct mbuf *m0)
{
	struct cpl_tx_pkt_lso_core *lso;
	uint32_t ctrl;

	KASSERT(m0->m_pkthdr.l2hlen > 0 && m0->m_pkthdr.l3hlen > 0 &&
	    m0->m_pkthdr.l4hlen > 0,
	    ("%s: mbuf %p needs TSO but missing header lengths",
		__func__, m0));

	ctrl = V_LSO_OPCODE(CPL_TX_PKT_LSO) |
	    F_LSO_FIRST_SLICE | F_LSO_LAST_SLICE |
	    V_LSO_ETHHDR_LEN((m0->m_pkthdr.l2hlen - ETHER_HDR_LEN) >> 2) |
	    V_LSO_IPHDR_LEN(m0->m_pkthdr.l3hlen >> 2) |
	    V_LSO_TCPHDR_LEN(m0->m_pkthdr.l4hlen >> 2);
	if (m0->m_pkthdr.l3hlen == sizeof(struct ip6_hdr))
		ctrl |= F_LSO_IPV6;

	lso = cpl;
	lso->lso_ctrl = htobe32(ctrl);
	lso->ipid_ofst = htobe16(0);
	lso->mss = htobe16(m0->m_pkthdr.tso_segsz);
	lso->seqno_offset = htobe32(0);
	lso->len = htobe32(m0->m_pkthdr.len);

	return (lso + 1);
}

static void *
write_tnl_lso_cpl(void *cpl, struct mbuf *m0)
{
	struct cpl_tx_tnl_lso *tnl_lso = cpl;
	uint32_t ctrl;

	KASSERT(m0->m_pkthdr.inner_l2hlen > 0 &&
	    m0->m_pkthdr.inner_l3hlen > 0 && m0->m_pkthdr.inner_l4hlen > 0 &&
	    m0->m_pkthdr.inner_l5hlen > 0,
	    ("%s: mbuf %p needs VXLAN_TSO but missing inner header lengths",
		__func__, m0));
	KASSERT(m0->m_pkthdr.l2hlen > 0 && m0->m_pkthdr.l3hlen > 0 &&
	    m0->m_pkthdr.l4hlen > 0 && m0->m_pkthdr.l5hlen > 0,
	    ("%s: mbuf %p needs VXLAN_TSO but missing outer header lengths",
		__func__, m0));

	/* Outer headers. */
	ctrl = V_CPL_TX_TNL_LSO_OPCODE(CPL_TX_TNL_LSO) |
	    F_CPL_TX_TNL_LSO_FIRST | F_CPL_TX_TNL_LSO_LAST |
	    V_CPL_TX_TNL_LSO_ETHHDRLENOUT(
		(m0->m_pkthdr.l2hlen - ETHER_HDR_LEN) >> 2) |
	    V_CPL_TX_TNL_LSO_IPHDRLENOUT(m0->m_pkthdr.l3hlen >> 2) |
	    F_CPL_TX_TNL_LSO_IPLENSETOUT;
	if (m0->m_pkthdr.l3hlen == sizeof(struct ip6_hdr))
		ctrl |= F_CPL_TX_TNL_LSO_IPV6OUT;
	else {
		ctrl |= F_CPL_TX_TNL_LSO_IPHDRCHKOUT |
		    F_CPL_TX_TNL_LSO_IPIDINCOUT;
	}
	tnl_lso->op_to_IpIdSplitOut = htobe32(ctrl);
	tnl_lso->IpIdOffsetOut = 0;
	tnl_lso->UdpLenSetOut_to_TnlHdrLen =
		htobe16(F_CPL_TX_TNL_LSO_UDPCHKCLROUT |
		    F_CPL_TX_TNL_LSO_UDPLENSETOUT |
		    V_CPL_TX_TNL_LSO_TNLHDRLEN(m0->m_pkthdr.l2hlen +
			m0->m_pkthdr.l3hlen + m0->m_pkthdr.l4hlen +
			m0->m_pkthdr.l5hlen) |
		    V_CPL_TX_TNL_LSO_TNLTYPE(TX_TNL_TYPE_VXLAN));
	tnl_lso->r1 = 0;

	/* Inner headers. */
	ctrl = V_CPL_TX_TNL_LSO_ETHHDRLEN(
	    (m0->m_pkthdr.inner_l2hlen - ETHER_HDR_LEN) >> 2) |
	    V_CPL_TX_TNL_LSO_IPHDRLEN(m0->m_pkthdr.inner_l3hlen >> 2) |
	    V_CPL_TX_TNL_LSO_TCPHDRLEN(m0->m_pkthdr.inner_l4hlen >> 2);
	if (m0->m_pkthdr.inner_l3hlen == sizeof(struct ip6_hdr))
		ctrl |= F_CPL_TX_TNL_LSO_IPV6;
	tnl_lso->Flow_to_TcpHdrLen = htobe32(ctrl);
	tnl_lso->IpIdOffset = 0;
	tnl_lso->IpIdSplit_to_Mss =
	    htobe16(V_CPL_TX_TNL_LSO_MSS(m0->m_pkthdr.tso_segsz));
	tnl_lso->TCPSeqOffset = 0;
	tnl_lso->EthLenOffset_Size =
	    htobe32(V_CPL_TX_TNL_LSO_SIZE(m0->m_pkthdr.len));

	return (tnl_lso + 1);
}

#define VM_TX_L2HDR_LEN	16	/* ethmacdst to vlantci */

/*
 * Write a VM txpkt WR for this packet to the hardware descriptors, update the
 * software descriptor, and advance the pidx.  It is guaranteed that enough
 * descriptors are available.
 *
 * The return value is the # of hardware descriptors used.
 */
static u_int
write_txpkt_vm_wr(struct adapter *sc, struct sge_txq *txq, struct mbuf *m0)
{
	struct sge_eq *eq;
	struct fw_eth_tx_pkt_vm_wr *wr;
	struct tx_sdesc *txsd;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;	/* used in many unrelated places */
	uint64_t ctrl1;
	int len16, ndesc, pktlen, nsegs;
	caddr_t dst;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m0);

	len16 = mbuf_len16(m0);
	nsegs = mbuf_nsegs(m0);
	pktlen = m0->m_pkthdr.len;
	ctrl = sizeof(struct cpl_tx_pkt_core);
	if (needs_tso(m0))
		ctrl += sizeof(struct cpl_tx_pkt_lso_core);
	ndesc = tx_len16_to_desc(len16);

	/* Firmware work request header */
	eq = &txq->eq;
	wr = (void *)&eq->desc[eq->pidx];
	wr->op_immdlen = htobe32(V_FW_WR_OP(FW_ETH_TX_PKT_VM_WR) |
	    V_FW_ETH_TX_PKT_WR_IMMDLEN(ctrl));

	ctrl = V_FW_WR_LEN16(len16);
	wr->equiq_to_len16 = htobe32(ctrl);
	wr->r3[0] = 0;
	wr->r3[1] = 0;

	/*
	 * Copy over ethmacdst, ethmacsrc, ethtype, and vlantci.
	 * vlantci is ignored unless the ethtype is 0x8100, so it's
	 * simpler to always copy it rather than making it
	 * conditional.  Also, it seems that we do not have to set
	 * vlantci or fake the ethtype when doing VLAN tag insertion.
	 */
	m_copydata(m0, 0, VM_TX_L2HDR_LEN, wr->ethmacdst);

	if (needs_tso(m0)) {
		cpl = write_lso_cpl(wr + 1, m0);
		txq->tso_wrs++;
	} else
		cpl = (void *)(wr + 1);

	/* Checksum offload */
	ctrl1 = csum_to_ctrl(sc, m0);
	if (ctrl1 != (F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS))
		txq->txcsum++;	/* some hardware assistance provided */

	/* VLAN tag insertion */
	if (needs_vlan_insertion(m0)) {
		ctrl1 |= F_TXPKT_VLAN_VLD |
		    V_TXPKT_VLAN(m0->m_pkthdr.ether_vtag);
		txq->vlan_insertion++;
	}

	/* CPL header */
	cpl->ctrl0 = txq->cpl_ctrl0;
	cpl->pack = 0;
	cpl->len = htobe16(pktlen);
	cpl->ctrl1 = htobe64(ctrl1);

	/* SGL */
	dst = (void *)(cpl + 1);

	/*
	 * A packet using TSO will use up an entire descriptor for the
	 * firmware work request header, LSO CPL, and TX_PKT_XT CPL.
	 * If this descriptor is the last descriptor in the ring, wrap
	 * around to the front of the ring explicitly for the start of
	 * the sgl.
	 */
	if (dst == (void *)&eq->desc[eq->sidx]) {
		dst = (void *)&eq->desc[0];
		write_gl_to_txd(txq, m0, &dst, 0);
	} else
		write_gl_to_txd(txq, m0, &dst, eq->sidx - ndesc < eq->pidx);
	txq->sgl_wrs++;
	txq->txpkt_wrs++;

	txsd = &txq->sdesc[eq->pidx];
	txsd->m = m0;
	txsd->desc_used = ndesc;

	return (ndesc);
}

/*
 * Write a raw WR to the hardware descriptors, update the software
 * descriptor, and advance the pidx.  It is guaranteed that enough
 * descriptors are available.
 *
 * The return value is the # of hardware descriptors used.
 */
static u_int
write_raw_wr(struct sge_txq *txq, void *wr, struct mbuf *m0, u_int available)
{
	struct sge_eq *eq = &txq->eq;
	struct tx_sdesc *txsd;
	struct mbuf *m;
	caddr_t dst;
	int len16, ndesc;

	len16 = mbuf_len16(m0);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	dst = wr;
	for (m = m0; m != NULL; m = m->m_next)
		copy_to_txd(eq, mtod(m, caddr_t), &dst, m->m_len);

	txq->raw_wrs++;

	txsd = &txq->sdesc[eq->pidx];
	txsd->m = m0;
	txsd->desc_used = ndesc;

	return (ndesc);
}

/*
 * Write a txpkt WR for this packet to the hardware descriptors, update the
 * software descriptor, and advance the pidx.  It is guaranteed that enough
 * descriptors are available.
 *
 * The return value is the # of hardware descriptors used.
 */
static u_int
write_txpkt_wr(struct adapter *sc, struct sge_txq *txq, struct mbuf *m0,
    u_int available)
{
	struct sge_eq *eq;
	struct fw_eth_tx_pkt_wr *wr;
	struct tx_sdesc *txsd;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;	/* used in many unrelated places */
	uint64_t ctrl1;
	int len16, ndesc, pktlen, nsegs;
	caddr_t dst;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m0);

	len16 = mbuf_len16(m0);
	nsegs = mbuf_nsegs(m0);
	pktlen = m0->m_pkthdr.len;
	ctrl = sizeof(struct cpl_tx_pkt_core);
	if (needs_tso(m0)) {
		if (needs_vxlan_tso(m0))
			ctrl += sizeof(struct cpl_tx_tnl_lso);
		else
			ctrl += sizeof(struct cpl_tx_pkt_lso_core);
	} else if (!(mbuf_cflags(m0) & MC_NOMAP) && pktlen <= imm_payload(2) &&
	    available >= 2) {
		/* Immediate data.  Recalculate len16 and set nsegs to 0. */
		ctrl += pktlen;
		len16 = howmany(sizeof(struct fw_eth_tx_pkt_wr) +
		    sizeof(struct cpl_tx_pkt_core) + pktlen, 16);
		nsegs = 0;
	}
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	/* Firmware work request header */
	eq = &txq->eq;
	wr = (void *)&eq->desc[eq->pidx];
	wr->op_immdlen = htobe32(V_FW_WR_OP(FW_ETH_TX_PKT_WR) |
	    V_FW_ETH_TX_PKT_WR_IMMDLEN(ctrl));

	ctrl = V_FW_WR_LEN16(len16);
	wr->equiq_to_len16 = htobe32(ctrl);
	wr->r3 = 0;

	if (needs_tso(m0)) {
		if (needs_vxlan_tso(m0)) {
			cpl = write_tnl_lso_cpl(wr + 1, m0);
			txq->vxlan_tso_wrs++;
		} else {
			cpl = write_lso_cpl(wr + 1, m0);
			txq->tso_wrs++;
		}
	} else
		cpl = (void *)(wr + 1);

	/* Checksum offload */
	ctrl1 = csum_to_ctrl(sc, m0);
	if (ctrl1 != (F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS)) {
		/* some hardware assistance provided */
		if (needs_vxlan_csum(m0))
			txq->vxlan_txcsum++;
		else
			txq->txcsum++;
	}

	/* VLAN tag insertion */
	if (needs_vlan_insertion(m0)) {
		ctrl1 |= F_TXPKT_VLAN_VLD |
		    V_TXPKT_VLAN(m0->m_pkthdr.ether_vtag);
		txq->vlan_insertion++;
	}

	/* CPL header */
	cpl->ctrl0 = txq->cpl_ctrl0;
	cpl->pack = 0;
	cpl->len = htobe16(pktlen);
	cpl->ctrl1 = htobe64(ctrl1);

	/* SGL */
	dst = (void *)(cpl + 1);
	if (__predict_false((uintptr_t)dst == (uintptr_t)&eq->desc[eq->sidx]))
		dst = (caddr_t)&eq->desc[0];
	if (nsegs > 0) {

		write_gl_to_txd(txq, m0, &dst, eq->sidx - ndesc < eq->pidx);
		txq->sgl_wrs++;
	} else {
		struct mbuf *m;

		for (m = m0; m != NULL; m = m->m_next) {
			copy_to_txd(eq, mtod(m, caddr_t), &dst, m->m_len);
#ifdef INVARIANTS
			pktlen -= m->m_len;
#endif
		}
#ifdef INVARIANTS
		KASSERT(pktlen == 0, ("%s: %d bytes left.", __func__, pktlen));
#endif
		txq->imm_wrs++;
	}

	txq->txpkt_wrs++;

	txsd = &txq->sdesc[eq->pidx];
	txsd->m = m0;
	txsd->desc_used = ndesc;

	return (ndesc);
}

static inline bool
cmp_l2hdr(struct txpkts *txp, struct mbuf *m)
{
	int len;

	MPASS(txp->npkt > 0);
	MPASS(m->m_len >= VM_TX_L2HDR_LEN);

	if (txp->ethtype == be16toh(ETHERTYPE_VLAN))
		len = VM_TX_L2HDR_LEN;
	else
		len = sizeof(struct ether_header);

	return (memcmp(m->m_data, &txp->ethmacdst[0], len) != 0);
}

static inline void
save_l2hdr(struct txpkts *txp, struct mbuf *m)
{
	MPASS(m->m_len >= VM_TX_L2HDR_LEN);

	memcpy(&txp->ethmacdst[0], mtod(m, const void *), VM_TX_L2HDR_LEN);
}

static int
add_to_txpkts_vf(struct adapter *sc, struct sge_txq *txq, struct mbuf *m,
    int avail, bool *send)
{
	struct txpkts *txp = &txq->txp;

	/* Cannot have TSO and coalesce at the same time. */
	if (cannot_use_txpkts(m)) {
cannot_coalesce:
		*send = txp->npkt > 0;
		return (EINVAL);
	}

	/* VF allows coalescing of type 1 (1 GL) only */
	if (mbuf_nsegs(m) > 1)
		goto cannot_coalesce;

	*send = false;
	if (txp->npkt > 0) {
		MPASS(tx_len16_to_desc(txp->len16) <= avail);
		MPASS(txp->npkt < txp->max_npkt);
		MPASS(txp->wr_type == 1);	/* VF supports type 1 only */

		if (tx_len16_to_desc(txp->len16 + txpkts1_len16()) > avail) {
retry_after_send:
			*send = true;
			return (EAGAIN);
		}
		if (m->m_pkthdr.len + txp->plen > 65535)
			goto retry_after_send;
		if (cmp_l2hdr(txp, m))
			goto retry_after_send;

		txp->len16 += txpkts1_len16();
		txp->plen += m->m_pkthdr.len;
		txp->mb[txp->npkt++] = m;
		if (txp->npkt == txp->max_npkt)
			*send = true;
	} else {
		txp->len16 = howmany(sizeof(struct fw_eth_tx_pkts_vm_wr), 16) +
		    txpkts1_len16();
		if (tx_len16_to_desc(txp->len16) > avail)
			goto cannot_coalesce;
		txp->npkt = 1;
		txp->wr_type = 1;
		txp->plen = m->m_pkthdr.len;
		txp->mb[0] = m;
		save_l2hdr(txp, m);
	}
	return (0);
}

static int
add_to_txpkts_pf(struct adapter *sc, struct sge_txq *txq, struct mbuf *m,
    int avail, bool *send)
{
	struct txpkts *txp = &txq->txp;
	int nsegs;

	MPASS(!(sc->flags & IS_VF));

	/* Cannot have TSO and coalesce at the same time. */
	if (cannot_use_txpkts(m)) {
cannot_coalesce:
		*send = txp->npkt > 0;
		return (EINVAL);
	}

	*send = false;
	nsegs = mbuf_nsegs(m);
	if (txp->npkt == 0) {
		if (m->m_pkthdr.len > 65535)
			goto cannot_coalesce;
		if (nsegs > 1) {
			txp->wr_type = 0;
			txp->len16 =
			    howmany(sizeof(struct fw_eth_tx_pkts_wr), 16) +
			    txpkts0_len16(nsegs);
		} else {
			txp->wr_type = 1;
			txp->len16 =
			    howmany(sizeof(struct fw_eth_tx_pkts_wr), 16) +
			    txpkts1_len16();
		}
		if (tx_len16_to_desc(txp->len16) > avail)
			goto cannot_coalesce;
		txp->npkt = 1;
		txp->plen = m->m_pkthdr.len;
		txp->mb[0] = m;
	} else {
		MPASS(tx_len16_to_desc(txp->len16) <= avail);
		MPASS(txp->npkt < txp->max_npkt);

		if (m->m_pkthdr.len + txp->plen > 65535) {
retry_after_send:
			*send = true;
			return (EAGAIN);
		}

		MPASS(txp->wr_type == 0 || txp->wr_type == 1);
		if (txp->wr_type == 0) {
			if (tx_len16_to_desc(txp->len16 +
			    txpkts0_len16(nsegs)) > min(avail, SGE_MAX_WR_NDESC))
				goto retry_after_send;
			txp->len16 += txpkts0_len16(nsegs);
		} else {
			if (nsegs != 1)
				goto retry_after_send;
			if (tx_len16_to_desc(txp->len16 + txpkts1_len16()) >
			    avail)
				goto retry_after_send;
			txp->len16 += txpkts1_len16();
		}

		txp->plen += m->m_pkthdr.len;
		txp->mb[txp->npkt++] = m;
		if (txp->npkt == txp->max_npkt)
			*send = true;
	}
	return (0);
}

/*
 * Write a txpkts WR for the packets in txp to the hardware descriptors, update
 * the software descriptor, and advance the pidx.  It is guaranteed that enough
 * descriptors are available.
 *
 * The return value is the # of hardware descriptors used.
 */
static u_int
write_txpkts_wr(struct adapter *sc, struct sge_txq *txq)
{
	const struct txpkts *txp = &txq->txp;
	struct sge_eq *eq = &txq->eq;
	struct fw_eth_tx_pkts_wr *wr;
	struct tx_sdesc *txsd;
	struct cpl_tx_pkt_core *cpl;
	uint64_t ctrl1;
	int ndesc, i, checkwrap;
	struct mbuf *m, *last;
	void *flitp;

	TXQ_LOCK_ASSERT_OWNED(txq);
	MPASS(txp->npkt > 0);
	MPASS(txp->len16 <= howmany(SGE_MAX_WR_LEN, 16));

	wr = (void *)&eq->desc[eq->pidx];
	wr->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR));
	wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(txp->len16));
	wr->plen = htobe16(txp->plen);
	wr->npkt = txp->npkt;
	wr->r3 = 0;
	wr->type = txp->wr_type;
	flitp = wr + 1;

	/*
	 * At this point we are 16B into a hardware descriptor.  If checkwrap is
	 * set then we know the WR is going to wrap around somewhere.  We'll
	 * check for that at appropriate points.
	 */
	ndesc = tx_len16_to_desc(txp->len16);
	last = NULL;
	checkwrap = eq->sidx - ndesc < eq->pidx;
	for (i = 0; i < txp->npkt; i++) {
		m = txp->mb[i];
		if (txp->wr_type == 0) {
			struct ulp_txpkt *ulpmc;
			struct ulptx_idata *ulpsc;

			/* ULP master command */
			ulpmc = flitp;
			ulpmc->cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
			    V_ULP_TXPKT_DEST(0) | V_ULP_TXPKT_FID(eq->iqid));
			ulpmc->len = htobe32(txpkts0_len16(mbuf_nsegs(m)));

			/* ULP subcommand */
			ulpsc = (void *)(ulpmc + 1);
			ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
			    F_ULP_TX_SC_MORE);
			ulpsc->len = htobe32(sizeof(struct cpl_tx_pkt_core));

			cpl = (void *)(ulpsc + 1);
			if (checkwrap &&
			    (uintptr_t)cpl == (uintptr_t)&eq->desc[eq->sidx])
				cpl = (void *)&eq->desc[0];
		} else {
			cpl = flitp;
		}

		/* Checksum offload */
		ctrl1 = csum_to_ctrl(sc, m);
		if (ctrl1 != (F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS)) {
			/* some hardware assistance provided */
			if (needs_vxlan_csum(m))
				txq->vxlan_txcsum++;
			else
				txq->txcsum++;
		}

		/* VLAN tag insertion */
		if (needs_vlan_insertion(m)) {
			ctrl1 |= F_TXPKT_VLAN_VLD |
			    V_TXPKT_VLAN(m->m_pkthdr.ether_vtag);
			txq->vlan_insertion++;
		}

		/* CPL header */
		cpl->ctrl0 = txq->cpl_ctrl0;
		cpl->pack = 0;
		cpl->len = htobe16(m->m_pkthdr.len);
		cpl->ctrl1 = htobe64(ctrl1);

		flitp = cpl + 1;
		if (checkwrap &&
		    (uintptr_t)flitp == (uintptr_t)&eq->desc[eq->sidx])
			flitp = (void *)&eq->desc[0];

		write_gl_to_txd(txq, m, (caddr_t *)(&flitp), checkwrap);

		if (last != NULL)
			last->m_nextpkt = m;
		last = m;
	}

	txq->sgl_wrs++;
	if (txp->wr_type == 0) {
		txq->txpkts0_pkts += txp->npkt;
		txq->txpkts0_wrs++;
	} else {
		txq->txpkts1_pkts += txp->npkt;
		txq->txpkts1_wrs++;
	}

	txsd = &txq->sdesc[eq->pidx];
	txsd->m = txp->mb[0];
	txsd->desc_used = ndesc;

	return (ndesc);
}

static u_int
write_txpkts_vm_wr(struct adapter *sc, struct sge_txq *txq)
{
	const struct txpkts *txp = &txq->txp;
	struct sge_eq *eq = &txq->eq;
	struct fw_eth_tx_pkts_vm_wr *wr;
	struct tx_sdesc *txsd;
	struct cpl_tx_pkt_core *cpl;
	uint64_t ctrl1;
	int ndesc, i;
	struct mbuf *m, *last;
	void *flitp;

	TXQ_LOCK_ASSERT_OWNED(txq);
	MPASS(txp->npkt > 0);
	MPASS(txp->wr_type == 1);	/* VF supports type 1 only */
	MPASS(txp->mb[0] != NULL);
	MPASS(txp->len16 <= howmany(SGE_MAX_WR_LEN, 16));

	wr = (void *)&eq->desc[eq->pidx];
	wr->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_VM_WR));
	wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(txp->len16));
	wr->r3 = 0;
	wr->plen = htobe16(txp->plen);
	wr->npkt = txp->npkt;
	wr->r4 = 0;
	memcpy(&wr->ethmacdst[0], &txp->ethmacdst[0], 16);
	flitp = wr + 1;

	/*
	 * At this point we are 32B into a hardware descriptor.  Each mbuf in
	 * the WR will take 32B so we check for the end of the descriptor ring
	 * before writing odd mbufs (mb[1], 3, 5, ..)
	 */
	ndesc = tx_len16_to_desc(txp->len16);
	last = NULL;
	for (i = 0; i < txp->npkt; i++) {
		m = txp->mb[i];
		if (i & 1 && (uintptr_t)flitp == (uintptr_t)&eq->desc[eq->sidx])
			flitp = &eq->desc[0];
		cpl = flitp;

		/* Checksum offload */
		ctrl1 = csum_to_ctrl(sc, m);
		if (ctrl1 != (F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS))
			txq->txcsum++;	/* some hardware assistance provided */

		/* VLAN tag insertion */
		if (needs_vlan_insertion(m)) {
			ctrl1 |= F_TXPKT_VLAN_VLD |
			    V_TXPKT_VLAN(m->m_pkthdr.ether_vtag);
			txq->vlan_insertion++;
		}

		/* CPL header */
		cpl->ctrl0 = txq->cpl_ctrl0;
		cpl->pack = 0;
		cpl->len = htobe16(m->m_pkthdr.len);
		cpl->ctrl1 = htobe64(ctrl1);

		flitp = cpl + 1;
		MPASS(mbuf_nsegs(m) == 1);
		write_gl_to_txd(txq, m, (caddr_t *)(&flitp), 0);

		if (last != NULL)
			last->m_nextpkt = m;
		last = m;
	}

	txq->sgl_wrs++;
	txq->txpkts1_pkts += txp->npkt;
	txq->txpkts1_wrs++;

	txsd = &txq->sdesc[eq->pidx];
	txsd->m = txp->mb[0];
	txsd->desc_used = ndesc;

	return (ndesc);
}

/*
 * If the SGL ends on an address that is not 16 byte aligned, this function will
 * add a 0 filled flit at the end.
 */
static void
write_gl_to_txd(struct sge_txq *txq, struct mbuf *m, caddr_t *to, int checkwrap)
{
	struct sge_eq *eq = &txq->eq;
	struct sglist *gl = txq->gl;
	struct sglist_seg *seg;
	__be64 *flitp, *wrap;
	struct ulptx_sgl *usgl;
	int i, nflits, nsegs;

	KASSERT(((uintptr_t)(*to) & 0xf) == 0,
	    ("%s: SGL must start at a 16 byte boundary: %p", __func__, *to));
	MPASS((uintptr_t)(*to) >= (uintptr_t)&eq->desc[0]);
	MPASS((uintptr_t)(*to) < (uintptr_t)&eq->desc[eq->sidx]);

	get_pkt_gl(m, gl);
	nsegs = gl->sg_nseg;
	MPASS(nsegs > 0);

	nflits = (3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1) + 2;
	flitp = (__be64 *)(*to);
	wrap = (__be64 *)(&eq->desc[eq->sidx]);
	seg = &gl->sg_segs[0];
	usgl = (void *)flitp;

	/*
	 * We start at a 16 byte boundary somewhere inside the tx descriptor
	 * ring, so we're at least 16 bytes away from the status page.  There is
	 * no chance of a wrap around in the middle of usgl (which is 16 bytes).
	 */

	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));
	usgl->len0 = htobe32(seg->ss_len);
	usgl->addr0 = htobe64(seg->ss_paddr);
	seg++;

	if (checkwrap == 0 || (uintptr_t)(flitp + nflits) <= (uintptr_t)wrap) {

		/* Won't wrap around at all */

		for (i = 0; i < nsegs - 1; i++, seg++) {
			usgl->sge[i / 2].len[i & 1] = htobe32(seg->ss_len);
			usgl->sge[i / 2].addr[i & 1] = htobe64(seg->ss_paddr);
		}
		if (i & 1)
			usgl->sge[i / 2].len[1] = htobe32(0);
		flitp += nflits;
	} else {

		/* Will wrap somewhere in the rest of the SGL */

		/* 2 flits already written, write the rest flit by flit */
		flitp = (void *)(usgl + 1);
		for (i = 0; i < nflits - 2; i++) {
			if (flitp == wrap)
				flitp = (void *)eq->desc;
			*flitp++ = get_flit(seg, nsegs - 1, i);
		}
	}

	if (nflits & 1) {
		MPASS(((uintptr_t)flitp) & 0xf);
		*flitp++ = 0;
	}

	MPASS((((uintptr_t)flitp) & 0xf) == 0);
	if (__predict_false(flitp == wrap))
		*to = (void *)eq->desc;
	else
		*to = (void *)flitp;
}

static inline void
copy_to_txd(struct sge_eq *eq, caddr_t from, caddr_t *to, int len)
{

	MPASS((uintptr_t)(*to) >= (uintptr_t)&eq->desc[0]);
	MPASS((uintptr_t)(*to) < (uintptr_t)&eq->desc[eq->sidx]);

	if (__predict_true((uintptr_t)(*to) + len <=
	    (uintptr_t)&eq->desc[eq->sidx])) {
		bcopy(from, *to, len);
		(*to) += len;
	} else {
		int portion = (uintptr_t)&eq->desc[eq->sidx] - (uintptr_t)(*to);

		bcopy(from, *to, portion);
		from += portion;
		portion = len - portion;	/* remaining */
		bcopy(from, (void *)eq->desc, portion);
		(*to) = (caddr_t)eq->desc + portion;
	}
}

static inline void
ring_eq_db(struct adapter *sc, struct sge_eq *eq, u_int n)
{
	u_int db;

	MPASS(n > 0);

	db = eq->doorbells;
	if (n > 1)
		clrbit(&db, DOORBELL_WCWR);
	wmb();

	switch (ffs(db) - 1) {
	case DOORBELL_UDB:
		*eq->udb = htole32(V_QID(eq->udb_qid) | V_PIDX(n));
		break;

	case DOORBELL_WCWR: {
		volatile uint64_t *dst, *src;
		int i;

		/*
		 * Queues whose 128B doorbell segment fits in the page do not
		 * use relative qid (udb_qid is always 0).  Only queues with
		 * doorbell segments can do WCWR.
		 */
		KASSERT(eq->udb_qid == 0 && n == 1,
		    ("%s: inappropriate doorbell (0x%x, %d, %d) for eq %p",
		    __func__, eq->doorbells, n, eq->dbidx, eq));

		dst = (volatile void *)((uintptr_t)eq->udb + UDBS_WR_OFFSET -
		    UDBS_DB_OFFSET);
		i = eq->dbidx;
		src = (void *)&eq->desc[i];
		while (src != (void *)&eq->desc[i + 1])
			*dst++ = *src++;
		wmb();
		break;
	}

	case DOORBELL_UDBWC:
		*eq->udb = htole32(V_QID(eq->udb_qid) | V_PIDX(n));
		wmb();
		break;

	case DOORBELL_KDB:
		t4_write_reg(sc, sc->sge_kdoorbell_reg,
		    V_QID(eq->cntxt_id) | V_PIDX(n));
		break;
	}

	IDXINCR(eq->dbidx, n, eq->sidx);
}

static inline u_int
reclaimable_tx_desc(struct sge_eq *eq)
{
	uint16_t hw_cidx;

	hw_cidx = read_hw_cidx(eq);
	return (IDXDIFF(hw_cidx, eq->cidx, eq->sidx));
}

static inline u_int
total_available_tx_desc(struct sge_eq *eq)
{
	uint16_t hw_cidx, pidx;

	hw_cidx = read_hw_cidx(eq);
	pidx = eq->pidx;

	if (pidx == hw_cidx)
		return (eq->sidx - 1);
	else
		return (IDXDIFF(hw_cidx, pidx, eq->sidx) - 1);
}

static inline uint16_t
read_hw_cidx(struct sge_eq *eq)
{
	struct sge_qstat *spg = (void *)&eq->desc[eq->sidx];
	uint16_t cidx = spg->cidx;	/* stable snapshot */

	return (be16toh(cidx));
}

/*
 * Reclaim 'n' descriptors approximately.
 */
static u_int
reclaim_tx_descs(struct sge_txq *txq, u_int n)
{
	struct tx_sdesc *txsd;
	struct sge_eq *eq = &txq->eq;
	u_int can_reclaim, reclaimed;

	TXQ_LOCK_ASSERT_OWNED(txq);
	MPASS(n > 0);

	reclaimed = 0;
	can_reclaim = reclaimable_tx_desc(eq);
	while (can_reclaim && reclaimed < n) {
		int ndesc;
		struct mbuf *m, *nextpkt;

		txsd = &txq->sdesc[eq->cidx];
		ndesc = txsd->desc_used;

		/* Firmware doesn't return "partial" credits. */
		KASSERT(can_reclaim >= ndesc,
		    ("%s: unexpected number of credits: %d, %d",
		    __func__, can_reclaim, ndesc));
		KASSERT(ndesc != 0,
		    ("%s: descriptor with no credits: cidx %d",
		    __func__, eq->cidx));

		for (m = txsd->m; m != NULL; m = nextpkt) {
			nextpkt = m->m_nextpkt;
			m->m_nextpkt = NULL;
			m_freem(m);
		}
		reclaimed += ndesc;
		can_reclaim -= ndesc;
		IDXINCR(eq->cidx, ndesc, eq->sidx);
	}

	return (reclaimed);
}

static void
tx_reclaim(void *arg, int n)
{
	struct sge_txq *txq = arg;
	struct sge_eq *eq = &txq->eq;

	do {
		if (TXQ_TRYLOCK(txq) == 0)
			break;
		n = reclaim_tx_descs(txq, 32);
		if (eq->cidx == eq->pidx)
			eq->equeqidx = eq->pidx;
		TXQ_UNLOCK(txq);
	} while (n > 0);
}

static __be64
get_flit(struct sglist_seg *segs, int nsegs, int idx)
{
	int i = (idx / 3) * 2;

	switch (idx % 3) {
	case 0: {
		uint64_t rc;

		rc = (uint64_t)segs[i].ss_len << 32;
		if (i + 1 < nsegs)
			rc |= (uint64_t)(segs[i + 1].ss_len);

		return (htobe64(rc));
	}
	case 1:
		return (htobe64(segs[i].ss_paddr));
	case 2:
		return (htobe64(segs[i + 1].ss_paddr));
	}

	return (0);
}

static int
find_refill_source(struct adapter *sc, int maxp, bool packing)
{
	int i, zidx = -1;
	struct rx_buf_info *rxb = &sc->sge.rx_buf_info[0];

	if (packing) {
		for (i = 0; i < SW_ZONE_SIZES; i++, rxb++) {
			if (rxb->hwidx2 == -1)
				continue;
			if (rxb->size1 < PAGE_SIZE &&
			    rxb->size1 < largest_rx_cluster)
				continue;
			if (rxb->size1 > largest_rx_cluster)
				break;
			MPASS(rxb->size1 - rxb->size2 >= CL_METADATA_SIZE);
			if (rxb->size2 >= maxp)
				return (i);
			zidx = i;
		}
	} else {
		for (i = 0; i < SW_ZONE_SIZES; i++, rxb++) {
			if (rxb->hwidx1 == -1)
				continue;
			if (rxb->size1 > largest_rx_cluster)
				break;
			if (rxb->size1 >= maxp)
				return (i);
			zidx = i;
		}
	}

	return (zidx);
}

static void
add_fl_to_sfl(struct adapter *sc, struct sge_fl *fl)
{
	mtx_lock(&sc->sfl_lock);
	FL_LOCK(fl);
	if ((fl->flags & FL_DOOMED) == 0) {
		fl->flags |= FL_STARVING;
		TAILQ_INSERT_TAIL(&sc->sfl, fl, link);
		callout_reset(&sc->sfl_callout, hz / 5, refill_sfl, sc);
	}
	FL_UNLOCK(fl);
	mtx_unlock(&sc->sfl_lock);
}

static void
handle_wrq_egr_update(struct adapter *sc, struct sge_eq *eq)
{
	struct sge_wrq *wrq = (void *)eq;

	atomic_readandclear_int(&eq->equiq);
	taskqueue_enqueue(sc->tq[eq->tx_chan], &wrq->wrq_tx_task);
}

static void
handle_eth_egr_update(struct adapter *sc, struct sge_eq *eq)
{
	struct sge_txq *txq = (void *)eq;

	MPASS((eq->flags & EQ_TYPEMASK) == EQ_ETH);

	atomic_readandclear_int(&eq->equiq);
	if (mp_ring_is_idle(txq->r))
		taskqueue_enqueue(sc->tq[eq->tx_chan], &txq->tx_reclaim_task);
	else
		mp_ring_check_drainage(txq->r, 64);
}

static int
handle_sge_egr_update(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_sge_egr_update *cpl = (const void *)(rss + 1);
	unsigned int qid = G_EGR_QID(ntohl(cpl->opcode_qid));
	struct adapter *sc = iq->adapter;
	struct sge *s = &sc->sge;
	struct sge_eq *eq;
	static void (*h[])(struct adapter *, struct sge_eq *) = {NULL,
		&handle_wrq_egr_update, &handle_eth_egr_update,
		&handle_wrq_egr_update};

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));

	eq = s->eqmap[qid - s->eq_start - s->eq_base];
	(*h[eq->flags & EQ_TYPEMASK])(sc, eq);

	return (0);
}

/* handle_fw_msg works for both fw4_msg and fw6_msg because this is valid */
CTASSERT(offsetof(struct cpl_fw4_msg, data) == \
    offsetof(struct cpl_fw6_msg, data));

static int
handle_fw_msg(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw6_msg *cpl = (const void *)(rss + 1);

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));

	if (cpl->type == FW_TYPE_RSSCPL || cpl->type == FW6_TYPE_RSSCPL) {
		const struct rss_header *rss2;

		rss2 = (const struct rss_header *)&cpl->data[0];
		return (t4_cpl_handler[rss2->opcode](iq, rss2, m));
	}

	return (t4_fw_msg_handler[cpl->type](sc, &cpl->data[0]));
}

/**
 *	t4_handle_wrerr_rpl - process a FW work request error message
 *	@adap: the adapter
 *	@rpl: start of the FW message
 */
static int
t4_handle_wrerr_rpl(struct adapter *adap, const __be64 *rpl)
{
	u8 opcode = *(const u8 *)rpl;
	const struct fw_error_cmd *e = (const void *)rpl;
	unsigned int i;

	if (opcode != FW_ERROR_CMD) {
		log(LOG_ERR,
		    "%s: Received WRERR_RPL message with opcode %#x\n",
		    device_get_nameunit(adap->dev), opcode);
		return (EINVAL);
	}
	log(LOG_ERR, "%s: FW_ERROR (%s) ", device_get_nameunit(adap->dev),
	    G_FW_ERROR_CMD_FATAL(be32toh(e->op_to_type)) ? "fatal" :
	    "non-fatal");
	switch (G_FW_ERROR_CMD_TYPE(be32toh(e->op_to_type))) {
	case FW_ERROR_TYPE_EXCEPTION:
		log(LOG_ERR, "exception info:\n");
		for (i = 0; i < nitems(e->u.exception.info); i++)
			log(LOG_ERR, "%s%08x", i == 0 ? "\t" : " ",
			    be32toh(e->u.exception.info[i]));
		log(LOG_ERR, "\n");
		break;
	case FW_ERROR_TYPE_HWMODULE:
		log(LOG_ERR, "HW module regaddr %08x regval %08x\n",
		    be32toh(e->u.hwmodule.regaddr),
		    be32toh(e->u.hwmodule.regval));
		break;
	case FW_ERROR_TYPE_WR:
		log(LOG_ERR, "WR cidx %d PF %d VF %d eqid %d hdr:\n",
		    be16toh(e->u.wr.cidx),
		    G_FW_ERROR_CMD_PFN(be16toh(e->u.wr.pfn_vfn)),
		    G_FW_ERROR_CMD_VFN(be16toh(e->u.wr.pfn_vfn)),
		    be32toh(e->u.wr.eqid));
		for (i = 0; i < nitems(e->u.wr.wrhdr); i++)
			log(LOG_ERR, "%s%02x", i == 0 ? "\t" : " ",
			    e->u.wr.wrhdr[i]);
		log(LOG_ERR, "\n");
		break;
	case FW_ERROR_TYPE_ACL:
		log(LOG_ERR, "ACL cidx %d PF %d VF %d eqid %d %s",
		    be16toh(e->u.acl.cidx),
		    G_FW_ERROR_CMD_PFN(be16toh(e->u.acl.pfn_vfn)),
		    G_FW_ERROR_CMD_VFN(be16toh(e->u.acl.pfn_vfn)),
		    be32toh(e->u.acl.eqid),
		    G_FW_ERROR_CMD_MV(be16toh(e->u.acl.mv_pkd)) ? "vlanid" :
		    "MAC");
		for (i = 0; i < nitems(e->u.acl.val); i++)
			log(LOG_ERR, " %02x", e->u.acl.val[i]);
		log(LOG_ERR, "\n");
		break;
	default:
		log(LOG_ERR, "type %#x\n",
		    G_FW_ERROR_CMD_TYPE(be32toh(e->op_to_type)));
		return (EINVAL);
	}
	return (0);
}

static int
sysctl_uint16(SYSCTL_HANDLER_ARGS)
{
	uint16_t *id = arg1;
	int i = *id;

	return sysctl_handle_int(oidp, &i, 0, req);
}

static inline bool
bufidx_used(struct adapter *sc, int idx)
{
	struct rx_buf_info *rxb = &sc->sge.rx_buf_info[0];
	int i;

	for (i = 0; i < SW_ZONE_SIZES; i++, rxb++) {
		if (rxb->size1 > largest_rx_cluster)
			continue;
		if (rxb->hwidx1 == idx || rxb->hwidx2 == idx)
			return (true);
	}

	return (false);
}

static int
sysctl_bufsizes(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sge_params *sp = &sc->params.sge;
	int i, rc;
	struct sbuf sb;
	char c;

	sbuf_new(&sb, NULL, 128, SBUF_AUTOEXTEND);
	for (i = 0; i < SGE_FLBUF_SIZES; i++) {
		if (bufidx_used(sc, i))
			c = '*';
		else
			c = '\0';

		sbuf_printf(&sb, "%u%c ", sp->sge_fl_buffer_size[i], c);
	}
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	rc = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (rc);
}

#ifdef RATELIMIT
/*
 * len16 for a txpkt WR with a GL.  Includes the firmware work request header.
 */
static inline u_int
txpkt_eo_len16(u_int nsegs, u_int immhdrs, u_int tso)
{
	u_int n;

	MPASS(immhdrs > 0);

	n = roundup2(sizeof(struct fw_eth_tx_eo_wr) +
	    sizeof(struct cpl_tx_pkt_core) + immhdrs, 16);
	if (__predict_false(nsegs == 0))
		goto done;

	nsegs--; /* first segment is part of ulptx_sgl */
	n += sizeof(struct ulptx_sgl) + 8 * ((3 * nsegs) / 2 + (nsegs & 1));
	if (tso)
		n += sizeof(struct cpl_tx_pkt_lso_core);

done:
	return (howmany(n, 16));
}

#define ETID_FLOWC_NPARAMS 6
#define ETID_FLOWC_LEN (roundup2((sizeof(struct fw_flowc_wr) + \
    ETID_FLOWC_NPARAMS * sizeof(struct fw_flowc_mnemval)), 16))
#define ETID_FLOWC_LEN16 (howmany(ETID_FLOWC_LEN, 16))

static int
send_etid_flowc_wr(struct cxgbe_rate_tag *cst, struct port_info *pi,
    struct vi_info *vi)
{
	struct wrq_cookie cookie;
	u_int pfvf = pi->adapter->pf << S_FW_VIID_PFN;
	struct fw_flowc_wr *flowc;

	mtx_assert(&cst->lock, MA_OWNED);
	MPASS((cst->flags & (EO_FLOWC_PENDING | EO_FLOWC_RPL_PENDING)) ==
	    EO_FLOWC_PENDING);

	flowc = start_wrq_wr(cst->eo_txq, ETID_FLOWC_LEN16, &cookie);
	if (__predict_false(flowc == NULL))
		return (ENOMEM);

	bzero(flowc, ETID_FLOWC_LEN);
	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(ETID_FLOWC_NPARAMS) | V_FW_WR_COMPL(0));
	flowc->flowid_len16 = htonl(V_FW_WR_LEN16(ETID_FLOWC_LEN16) |
	    V_FW_WR_FLOWID(cst->etid));
	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_PFNVFN;
	flowc->mnemval[0].val = htobe32(pfvf);
	flowc->mnemval[1].mnemonic = FW_FLOWC_MNEM_CH;
	flowc->mnemval[1].val = htobe32(pi->tx_chan);
	flowc->mnemval[2].mnemonic = FW_FLOWC_MNEM_PORT;
	flowc->mnemval[2].val = htobe32(pi->tx_chan);
	flowc->mnemval[3].mnemonic = FW_FLOWC_MNEM_IQID;
	flowc->mnemval[3].val = htobe32(cst->iqid);
	flowc->mnemval[4].mnemonic = FW_FLOWC_MNEM_EOSTATE;
	flowc->mnemval[4].val = htobe32(FW_FLOWC_MNEM_EOSTATE_ESTABLISHED);
	flowc->mnemval[5].mnemonic = FW_FLOWC_MNEM_SCHEDCLASS;
	flowc->mnemval[5].val = htobe32(cst->schedcl);

	commit_wrq_wr(cst->eo_txq, flowc, &cookie);

	cst->flags &= ~EO_FLOWC_PENDING;
	cst->flags |= EO_FLOWC_RPL_PENDING;
	MPASS(cst->tx_credits >= ETID_FLOWC_LEN16);	/* flowc is first WR. */
	cst->tx_credits -= ETID_FLOWC_LEN16;

	return (0);
}

#define ETID_FLUSH_LEN16 (howmany(sizeof (struct fw_flowc_wr), 16))

void
send_etid_flush_wr(struct cxgbe_rate_tag *cst)
{
	struct fw_flowc_wr *flowc;
	struct wrq_cookie cookie;

	mtx_assert(&cst->lock, MA_OWNED);

	flowc = start_wrq_wr(cst->eo_txq, ETID_FLUSH_LEN16, &cookie);
	if (__predict_false(flowc == NULL))
		CXGBE_UNIMPLEMENTED(__func__);

	bzero(flowc, ETID_FLUSH_LEN16 * 16);
	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(0) | F_FW_WR_COMPL);
	flowc->flowid_len16 = htobe32(V_FW_WR_LEN16(ETID_FLUSH_LEN16) |
	    V_FW_WR_FLOWID(cst->etid));

	commit_wrq_wr(cst->eo_txq, flowc, &cookie);

	cst->flags |= EO_FLUSH_RPL_PENDING;
	MPASS(cst->tx_credits >= ETID_FLUSH_LEN16);
	cst->tx_credits -= ETID_FLUSH_LEN16;
	cst->ncompl++;
}

static void
write_ethofld_wr(struct cxgbe_rate_tag *cst, struct fw_eth_tx_eo_wr *wr,
    struct mbuf *m0, int compl)
{
	struct cpl_tx_pkt_core *cpl;
	uint64_t ctrl1;
	uint32_t ctrl;	/* used in many unrelated places */
	int len16, pktlen, nsegs, immhdrs;
	caddr_t dst;
	uintptr_t p;
	struct ulptx_sgl *usgl;
	struct sglist sg;
	struct sglist_seg segs[38];	/* XXX: find real limit.  XXX: get off the stack */

	mtx_assert(&cst->lock, MA_OWNED);
	M_ASSERTPKTHDR(m0);
	KASSERT(m0->m_pkthdr.l2hlen > 0 && m0->m_pkthdr.l3hlen > 0 &&
	    m0->m_pkthdr.l4hlen > 0,
	    ("%s: ethofld mbuf %p is missing header lengths", __func__, m0));

	len16 = mbuf_eo_len16(m0);
	nsegs = mbuf_eo_nsegs(m0);
	pktlen = m0->m_pkthdr.len;
	ctrl = sizeof(struct cpl_tx_pkt_core);
	if (needs_tso(m0))
		ctrl += sizeof(struct cpl_tx_pkt_lso_core);
	immhdrs = m0->m_pkthdr.l2hlen + m0->m_pkthdr.l3hlen + m0->m_pkthdr.l4hlen;
	ctrl += immhdrs;

	wr->op_immdlen = htobe32(V_FW_WR_OP(FW_ETH_TX_EO_WR) |
	    V_FW_ETH_TX_EO_WR_IMMDLEN(ctrl) | V_FW_WR_COMPL(!!compl));
	wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(len16) |
	    V_FW_WR_FLOWID(cst->etid));
	wr->r3 = 0;
	if (needs_outer_udp_csum(m0)) {
		wr->u.udpseg.type = FW_ETH_TX_EO_TYPE_UDPSEG;
		wr->u.udpseg.ethlen = m0->m_pkthdr.l2hlen;
		wr->u.udpseg.iplen = htobe16(m0->m_pkthdr.l3hlen);
		wr->u.udpseg.udplen = m0->m_pkthdr.l4hlen;
		wr->u.udpseg.rtplen = 0;
		wr->u.udpseg.r4 = 0;
		wr->u.udpseg.mss = htobe16(pktlen - immhdrs);
		wr->u.udpseg.schedpktsize = wr->u.udpseg.mss;
		wr->u.udpseg.plen = htobe32(pktlen - immhdrs);
		cpl = (void *)(wr + 1);
	} else {
		MPASS(needs_outer_tcp_csum(m0));
		wr->u.tcpseg.type = FW_ETH_TX_EO_TYPE_TCPSEG;
		wr->u.tcpseg.ethlen = m0->m_pkthdr.l2hlen;
		wr->u.tcpseg.iplen = htobe16(m0->m_pkthdr.l3hlen);
		wr->u.tcpseg.tcplen = m0->m_pkthdr.l4hlen;
		wr->u.tcpseg.tsclk_tsoff = mbuf_eo_tsclk_tsoff(m0);
		wr->u.tcpseg.r4 = 0;
		wr->u.tcpseg.r5 = 0;
		wr->u.tcpseg.plen = htobe32(pktlen - immhdrs);

		if (needs_tso(m0)) {
			struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);

			wr->u.tcpseg.mss = htobe16(m0->m_pkthdr.tso_segsz);

			ctrl = V_LSO_OPCODE(CPL_TX_PKT_LSO) |
			    F_LSO_FIRST_SLICE | F_LSO_LAST_SLICE |
			    V_LSO_ETHHDR_LEN((m0->m_pkthdr.l2hlen -
				ETHER_HDR_LEN) >> 2) |
			    V_LSO_IPHDR_LEN(m0->m_pkthdr.l3hlen >> 2) |
			    V_LSO_TCPHDR_LEN(m0->m_pkthdr.l4hlen >> 2);
			if (m0->m_pkthdr.l3hlen == sizeof(struct ip6_hdr))
				ctrl |= F_LSO_IPV6;
			lso->lso_ctrl = htobe32(ctrl);
			lso->ipid_ofst = htobe16(0);
			lso->mss = htobe16(m0->m_pkthdr.tso_segsz);
			lso->seqno_offset = htobe32(0);
			lso->len = htobe32(pktlen);

			cpl = (void *)(lso + 1);
		} else {
			wr->u.tcpseg.mss = htobe16(0xffff);
			cpl = (void *)(wr + 1);
		}
	}

	/* Checksum offload must be requested for ethofld. */
	MPASS(needs_outer_l4_csum(m0));
	ctrl1 = csum_to_ctrl(cst->adapter, m0);

	/* VLAN tag insertion */
	if (needs_vlan_insertion(m0)) {
		ctrl1 |= F_TXPKT_VLAN_VLD |
		    V_TXPKT_VLAN(m0->m_pkthdr.ether_vtag);
	}

	/* CPL header */
	cpl->ctrl0 = cst->ctrl0;
	cpl->pack = 0;
	cpl->len = htobe16(pktlen);
	cpl->ctrl1 = htobe64(ctrl1);

	/* Copy Ethernet, IP & TCP/UDP hdrs as immediate data */
	p = (uintptr_t)(cpl + 1);
	m_copydata(m0, 0, immhdrs, (void *)p);

	/* SGL */
	dst = (void *)(cpl + 1);
	if (nsegs > 0) {
		int i, pad;

		/* zero-pad upto next 16Byte boundary, if not 16Byte aligned */
		p += immhdrs;
		pad = 16 - (immhdrs & 0xf);
		bzero((void *)p, pad);

		usgl = (void *)(p + pad);
		usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
		    V_ULPTX_NSGE(nsegs));

		sglist_init(&sg, nitems(segs), segs);
		for (; m0 != NULL; m0 = m0->m_next) {
			if (__predict_false(m0->m_len == 0))
				continue;
			if (immhdrs >= m0->m_len) {
				immhdrs -= m0->m_len;
				continue;
			}
			if (m0->m_flags & M_EXTPG)
				sglist_append_mbuf_epg(&sg, m0,
				    mtod(m0, vm_offset_t), m0->m_len);
                        else
				sglist_append(&sg, mtod(m0, char *) + immhdrs,
				    m0->m_len - immhdrs);
			immhdrs = 0;
		}
		MPASS(sg.sg_nseg == nsegs);

		/*
		 * Zero pad last 8B in case the WR doesn't end on a 16B
		 * boundary.
		 */
		*(uint64_t *)((char *)wr + len16 * 16 - 8) = 0;

		usgl->len0 = htobe32(segs[0].ss_len);
		usgl->addr0 = htobe64(segs[0].ss_paddr);
		for (i = 0; i < nsegs - 1; i++) {
			usgl->sge[i / 2].len[i & 1] = htobe32(segs[i + 1].ss_len);
			usgl->sge[i / 2].addr[i & 1] = htobe64(segs[i + 1].ss_paddr);
		}
		if (i & 1)
			usgl->sge[i / 2].len[1] = htobe32(0);
	}

}

static void
ethofld_tx(struct cxgbe_rate_tag *cst)
{
	struct mbuf *m;
	struct wrq_cookie cookie;
	int next_credits, compl;
	struct fw_eth_tx_eo_wr *wr;

	mtx_assert(&cst->lock, MA_OWNED);

	while ((m = mbufq_first(&cst->pending_tx)) != NULL) {
		M_ASSERTPKTHDR(m);

		/* How many len16 credits do we need to send this mbuf. */
		next_credits = mbuf_eo_len16(m);
		MPASS(next_credits > 0);
		if (next_credits > cst->tx_credits) {
			/*
			 * Tx will make progress eventually because there is at
			 * least one outstanding fw4_ack that will return
			 * credits and kick the tx.
			 */
			MPASS(cst->ncompl > 0);
			return;
		}
		wr = start_wrq_wr(cst->eo_txq, next_credits, &cookie);
		if (__predict_false(wr == NULL)) {
			/* XXX: wishful thinking, not a real assertion. */
			MPASS(cst->ncompl > 0);
			return;
		}
		cst->tx_credits -= next_credits;
		cst->tx_nocompl += next_credits;
		compl = cst->ncompl == 0 || cst->tx_nocompl >= cst->tx_total / 2;
		ETHER_BPF_MTAP(cst->com.ifp, m);
		write_ethofld_wr(cst, wr, m, compl);
		commit_wrq_wr(cst->eo_txq, wr, &cookie);
		if (compl) {
			cst->ncompl++;
			cst->tx_nocompl	= 0;
		}
		(void) mbufq_dequeue(&cst->pending_tx);

		/*
		 * Drop the mbuf's reference on the tag now rather
		 * than waiting until m_freem().  This ensures that
		 * cxgbe_rate_tag_free gets called when the inp drops
		 * its reference on the tag and there are no more
		 * mbufs in the pending_tx queue and can flush any
		 * pending requests.  Otherwise if the last mbuf
		 * doesn't request a completion the etid will never be
		 * released.
		 */
		m->m_pkthdr.snd_tag = NULL;
		m->m_pkthdr.csum_flags &= ~CSUM_SND_TAG;
		m_snd_tag_rele(&cst->com);

		mbufq_enqueue(&cst->pending_fwack, m);
	}
}

int
ethofld_transmit(struct ifnet *ifp, struct mbuf *m0)
{
	struct cxgbe_rate_tag *cst;
	int rc;

	MPASS(m0->m_nextpkt == NULL);
	MPASS(m0->m_pkthdr.csum_flags & CSUM_SND_TAG);
	MPASS(m0->m_pkthdr.snd_tag != NULL);
	cst = mst_to_crt(m0->m_pkthdr.snd_tag);

	mtx_lock(&cst->lock);
	MPASS(cst->flags & EO_SND_TAG_REF);

	if (__predict_false(cst->flags & EO_FLOWC_PENDING)) {
		struct vi_info *vi = ifp->if_softc;
		struct port_info *pi = vi->pi;
		struct adapter *sc = pi->adapter;
		const uint32_t rss_mask = vi->rss_size - 1;
		uint32_t rss_hash;

		cst->eo_txq = &sc->sge.ofld_txq[vi->first_ofld_txq];
		if (M_HASHTYPE_ISHASH(m0))
			rss_hash = m0->m_pkthdr.flowid;
		else
			rss_hash = arc4random();
		/* We assume RSS hashing */
		cst->iqid = vi->rss[rss_hash & rss_mask];
		cst->eo_txq += rss_hash % vi->nofldtxq;
		rc = send_etid_flowc_wr(cst, pi, vi);
		if (rc != 0)
			goto done;
	}

	if (__predict_false(cst->plen + m0->m_pkthdr.len > eo_max_backlog)) {
		rc = ENOBUFS;
		goto done;
	}

	mbufq_enqueue(&cst->pending_tx, m0);
	cst->plen += m0->m_pkthdr.len;

	/*
	 * Hold an extra reference on the tag while generating work
	 * requests to ensure that we don't try to free the tag during
	 * ethofld_tx() in case we are sending the final mbuf after
	 * the inp was freed.
	 */
	m_snd_tag_ref(&cst->com);
	ethofld_tx(cst);
	mtx_unlock(&cst->lock);
	m_snd_tag_rele(&cst->com);
	return (0);

done:
	mtx_unlock(&cst->lock);
	if (__predict_false(rc != 0))
		m_freem(m0);
	return (rc);
}

static int
ethofld_fw4_ack(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m0)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw4_ack *cpl = (const void *)(rss + 1);
	struct mbuf *m;
	u_int etid = G_CPL_FW4_ACK_FLOWID(be32toh(OPCODE_TID(cpl)));
	struct cxgbe_rate_tag *cst;
	uint8_t credits = cpl->credits;

	cst = lookup_etid(sc, etid);
	mtx_lock(&cst->lock);
	if (__predict_false(cst->flags & EO_FLOWC_RPL_PENDING)) {
		MPASS(credits >= ETID_FLOWC_LEN16);
		credits -= ETID_FLOWC_LEN16;
		cst->flags &= ~EO_FLOWC_RPL_PENDING;
	}

	KASSERT(cst->ncompl > 0,
	    ("%s: etid %u (%p) wasn't expecting completion.",
	    __func__, etid, cst));
	cst->ncompl--;

	while (credits > 0) {
		m = mbufq_dequeue(&cst->pending_fwack);
		if (__predict_false(m == NULL)) {
			/*
			 * The remaining credits are for the final flush that
			 * was issued when the tag was freed by the kernel.
			 */
			MPASS((cst->flags &
			    (EO_FLUSH_RPL_PENDING | EO_SND_TAG_REF)) ==
			    EO_FLUSH_RPL_PENDING);
			MPASS(credits == ETID_FLUSH_LEN16);
			MPASS(cst->tx_credits + cpl->credits == cst->tx_total);
			MPASS(cst->ncompl == 0);

			cst->flags &= ~EO_FLUSH_RPL_PENDING;
			cst->tx_credits += cpl->credits;
			cxgbe_rate_tag_free_locked(cst);
			return (0);	/* cst is gone. */
		}
		KASSERT(m != NULL,
		    ("%s: too many credits (%u, %u)", __func__, cpl->credits,
		    credits));
		KASSERT(credits >= mbuf_eo_len16(m),
		    ("%s: too few credits (%u, %u, %u)", __func__,
		    cpl->credits, credits, mbuf_eo_len16(m)));
		credits -= mbuf_eo_len16(m);
		cst->plen -= m->m_pkthdr.len;
		m_freem(m);
	}

	cst->tx_credits += cpl->credits;
	MPASS(cst->tx_credits <= cst->tx_total);

	if (cst->flags & EO_SND_TAG_REF) {
		/*
		 * As with ethofld_transmit(), hold an extra reference
		 * so that the tag is stable across ethold_tx().
		 */
		m_snd_tag_ref(&cst->com);
		m = mbufq_first(&cst->pending_tx);
		if (m != NULL && cst->tx_credits >= mbuf_eo_len16(m))
			ethofld_tx(cst);
		mtx_unlock(&cst->lock);
		m_snd_tag_rele(&cst->com);
	} else {
		/*
		 * There shouldn't be any pending packets if the tag
		 * was freed by the kernel since any pending packet
		 * should hold a reference to the tag.
		 */
		MPASS(mbufq_first(&cst->pending_tx) == NULL);
		mtx_unlock(&cst->lock);
	}

	return (0);
}
#endif
