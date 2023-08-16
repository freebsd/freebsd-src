/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012, 2015 Chelsio Communications, Inc.
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
 *
 */

#ifndef __T4_TOM_H__
#define __T4_TOM_H__
#include <sys/vmem.h>
#include "common/t4_hw.h"
#include "common/t4_msg.h"
#include "tom/t4_tls.h"

#define LISTEN_HASH_SIZE 32

/*
 * Min receive window.  We want it to be large enough to accommodate receive
 * coalescing, handle jumbo frames, and not trigger sender SWS avoidance.
 */
#define MIN_RCV_WND (24 * 1024U)

/*
 * Max receive window supported by HW in bytes.  Only a small part of it can
 * be set through option0, the rest needs to be set through RX_DATA_ACK.
 */
#define MAX_RCV_WND ((1U << 27) - 1)

#define	DDP_RSVD_WIN (16 * 1024U)
#define	SB_DDP_INDICATE	SB_IN_TOE	/* soreceive must respond to indicate */

#define USE_DDP_RX_FLOW_CONTROL

#define PPOD_SZ(n)	((n) * sizeof(struct pagepod))
#define PPOD_SIZE	(PPOD_SZ(1))

/* TOE PCB flags */
enum {
	TPF_ATTACHED	   = (1 << 0),	/* a tcpcb refers to this toepcb */
	TPF_FLOWC_WR_SENT  = (1 << 1),	/* firmware flow context WR sent */
	TPF_TX_DATA_SENT   = (1 << 2),	/* some data sent */
	TPF_TX_SUSPENDED   = (1 << 3),	/* tx suspended for lack of resources */
	TPF_SEND_FIN	   = (1 << 4),	/* send FIN after all pending data */
	TPF_FIN_SENT	   = (1 << 5),	/* FIN has been sent */
	TPF_ABORT_SHUTDOWN = (1 << 6),	/* connection abort is in progress */
	TPF_CPL_PENDING    = (1 << 7),	/* haven't received the last CPL */
	TPF_SYNQE	   = (1 << 8),	/* synq_entry, not really a toepcb */
	TPF_SYNQE_EXPANDED = (1 << 9),	/* toepcb ready, tid context updated */
	TPF_TLS_STARTING   = (1 << 10), /* starting TLS receive */
	TPF_KTLS           = (1 << 11), /* send TLS records from KTLS */
	TPF_INITIALIZED    = (1 << 12), /* init_toepcb has been called */
	TPF_TLS_RECEIVE	   = (1 << 13), /* should receive TLS records */
	TPF_TLS_RX_QUIESCED = (1 << 14), /* RX quiesced for TLS RX startup */
	TPF_WAITING_FOR_FINAL = (1<< 15), /* waiting for wakeup on final CPL */
};

enum {
	DDP_OK		= (1 << 0),	/* OK to turn on DDP */
	DDP_SC_REQ	= (1 << 1),	/* state change (on/off) requested */
	DDP_ON		= (1 << 2),	/* DDP is turned on */
	DDP_BUF0_ACTIVE	= (1 << 3),	/* buffer 0 in use (not invalidated) */
	DDP_BUF1_ACTIVE	= (1 << 4),	/* buffer 1 in use (not invalidated) */
	DDP_TASK_ACTIVE = (1 << 5),	/* requeue task is queued / running */
	DDP_DEAD	= (1 << 6),	/* toepcb is shutting down */
};

struct bio;
struct ctl_sg_entry;
struct sockopt;
struct offload_settings;

/*
 * Connection parameters for an offloaded connection.  These are mostly (but not
 * all) hardware TOE parameters.
 */
struct conn_params {
	int8_t rx_coalesce;
	int8_t cong_algo;
	int8_t tc_idx;
	int8_t tstamp;
	int8_t sack;
	int8_t nagle;
	int8_t keepalive;
	int8_t wscale;
	int8_t ecn;
	int8_t mtu_idx;
	int8_t ulp_mode;
	int8_t tx_align;
	int16_t txq_idx;	/* ofld_txq = &sc->sge.ofld_txq[txq_idx] */
	int16_t rxq_idx;	/* ofld_rxq = &sc->sge.ofld_rxq[rxq_idx] */
	int16_t l2t_idx;
	uint16_t emss;
	uint16_t opt0_bufsize;
	u_int sndbuf;		/* controls TP tx pages */
};

struct ofld_tx_sdesc {
	uint32_t plen;		/* payload length */
	uint8_t tx_credits;	/* firmware tx credits (unit is 16B) */
};

struct ppod_region {
	u_int pr_start;
	u_int pr_len;
	u_int pr_page_shift[4];
	uint32_t pr_tag_mask;		/* hardware tagmask for this region. */
	uint32_t pr_invalid_bit;	/* OR with this to invalidate tag. */
	uint32_t pr_alias_mask;		/* AND with tag to get alias bits. */
	u_int pr_alias_shift;		/* shift this much for first alias bit. */
	vmem_t *pr_arena;
};

struct ppod_reservation {
	struct ppod_region *prsv_pr;
	uint32_t prsv_tag;		/* Full tag: pgsz, alias, tag, color */
	u_int prsv_nppods;
};

struct pageset {
	TAILQ_ENTRY(pageset) link;
	vm_page_t *pages;
	int npages;
	int flags;
	int offset;		/* offset in first page */
	int len;
	struct ppod_reservation prsv;
	struct vmspace *vm;
	vm_offset_t start;
	u_int vm_timestamp;
};

TAILQ_HEAD(pagesetq, pageset);

#define	PS_PPODS_WRITTEN	0x0001	/* Page pods written to the card. */

struct ddp_buffer {
	struct pageset *ps;

	struct kaiocb *job;
	int cancel_pending;
};

struct ddp_pcb {
	u_int flags;
	struct ddp_buffer db[2];
	TAILQ_HEAD(, pageset) cached_pagesets;
	TAILQ_HEAD(, kaiocb) aiojobq;
	u_int waiting_count;
	u_int active_count;
	u_int cached_count;
	int active_id;	/* the currently active DDP buffer */
	struct task requeue_task;
	struct kaiocb *queueing;
	struct mtx lock;
};

struct toepcb {
	struct tom_data *td;
	struct inpcb *inp;	/* backpointer to host stack's PCB */
	u_int flags;		/* miscellaneous flags */
	TAILQ_ENTRY(toepcb) link; /* toep_list */
	int refcount;
	struct vnet *vnet;
	struct vi_info *vi;	/* virtual interface */
	struct sge_ofld_txq *ofld_txq;
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ctrlq;
	struct l2t_entry *l2te;	/* L2 table entry used by this connection */
	struct clip_entry *ce;	/* CLIP table entry used by this tid */
	int tid;		/* Connection identifier */

	/* tx credit handling */
	u_int tx_total;		/* total tx WR credits (in 16B units) */
	u_int tx_credits;	/* tx WR credits (in 16B units) available */
	u_int tx_nocompl;	/* tx WR credits since last compl request */
	u_int plen_nocompl;	/* payload since last compl request */

	struct conn_params params;

	void *ulpcb;
	void *ulpcb2;
	struct mbufq ulp_pduq;	/* PDUs waiting to be sent out. */
	struct mbufq ulp_pdu_reclaimq;

	struct ddp_pcb ddp;
	struct tls_ofld_info tls;

	TAILQ_HEAD(, kaiocb) aiotx_jobq;
	struct task aiotx_task;
	struct socket *aiotx_so;

	/* Tx software descriptor */
	uint8_t txsd_total;
	uint8_t txsd_pidx;
	uint8_t txsd_cidx;
	uint8_t txsd_avail;
	struct ofld_tx_sdesc txsd[];
};

static inline int
ulp_mode(struct toepcb *toep)
{

	return (toep->params.ulp_mode);
}

#define	DDP_LOCK(toep)		mtx_lock(&(toep)->ddp.lock)
#define	DDP_UNLOCK(toep)	mtx_unlock(&(toep)->ddp.lock)
#define	DDP_ASSERT_LOCKED(toep)	mtx_assert(&(toep)->ddp.lock, MA_OWNED)

/*
 * Compressed state for embryonic connections for a listener.
 */
struct synq_entry {
	struct listen_ctx *lctx;	/* backpointer to listen ctx */
	struct mbuf *syn;
	int flags;			/* same as toepcb's tp_flags */
	volatile int ok_to_respond;
	volatile u_int refcnt;
	int tid;
	uint32_t iss;
	uint32_t irs;
	uint32_t ts;
	uint32_t rss_hash;
	__be16 tcp_opt; /* from cpl_pass_establish */
	struct toepcb *toep;

	struct conn_params params;
};

/* listen_ctx flags */
#define LCTX_RPL_PENDING 1	/* waiting for a CPL_PASS_OPEN_RPL */

struct listen_ctx {
	LIST_ENTRY(listen_ctx) link;	/* listen hash linkage */
	volatile int refcount;
	int stid;
	struct stid_region stid_region;
	int flags;
	struct inpcb *inp;		/* listening socket's inp */
	struct vnet *vnet;
	struct sge_wrq *ctrlq;
	struct sge_ofld_rxq *ofld_rxq;
	struct clip_entry *ce;
};

/* tcb_histent flags */
#define TE_RPL_PENDING	1
#define TE_ACTIVE	2

/* bits in one 8b tcb_histent sample. */
#define TS_RTO			(1 << 0)
#define TS_DUPACKS		(1 << 1)
#define TS_FASTREXMT		(1 << 2)
#define TS_SND_BACKLOGGED	(1 << 3)
#define TS_CWND_LIMITED		(1 << 4)
#define TS_ECN_ECE		(1 << 5)
#define TS_ECN_CWR		(1 << 6)
#define TS_RESERVED		(1 << 7)	/* Unused. */

struct tcb_histent {
	struct mtx te_lock;
	struct callout te_callout;
	uint64_t te_tcb[TCB_SIZE / sizeof(uint64_t)];
	struct adapter *te_adapter;
	u_int te_flags;
	u_int te_tid;
	uint8_t te_pidx;
	uint8_t te_sample[100];
};

struct tom_data {
	struct toedev tod;

	/* toepcb's associated with this TOE device */
	struct mtx toep_list_lock;
	TAILQ_HEAD(, toepcb) toep_list;

	struct mtx lctx_hash_lock;
	LIST_HEAD(, listen_ctx) *listen_hash;
	u_long listen_mask;
	int lctx_count;		/* # of lctx in the hash table */

	struct ppod_region pr;

	struct rwlock tcb_history_lock __aligned(CACHE_LINE_SIZE);
	struct tcb_histent **tcb_history;
	int dupack_threshold;

	/* WRs that will not be sent to the chip because L2 resolution failed */
	struct mtx unsent_wr_lock;
	STAILQ_HEAD(, wrqe) unsent_wr_list;
	struct task reclaim_wr_resources;
};

static inline struct tom_data *
tod_td(struct toedev *tod)
{

	return (__containerof(tod, struct tom_data, tod));
}

static inline struct adapter *
td_adapter(struct tom_data *td)
{

	return (td->tod.tod_softc);
}

static inline void
set_mbuf_raw_wr(struct mbuf *m, bool raw)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_per.eight[6] = raw;
}

static inline bool
mbuf_raw_wr(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_per.eight[6]);
}

static inline void
set_mbuf_ulp_submode(struct mbuf *m, uint8_t ulp_submode)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_per.eight[0] = ulp_submode;
}

static inline uint8_t
mbuf_ulp_submode(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_per.eight[0]);
}

static inline void
set_mbuf_iscsi_iso(struct mbuf *m, bool iso)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_per.eight[1] = iso;
}

static inline bool
mbuf_iscsi_iso(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_per.eight[1]);
}

/* Flags for iSCSI segmentation offload. */
#define	CXGBE_ISO_TYPE(flags)	((flags) & 0x3)
#define	CXGBE_ISO_F		0x4

static inline void
set_mbuf_iscsi_iso_flags(struct mbuf *m, uint8_t flags)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_per.eight[2] = flags;
}

static inline uint8_t
mbuf_iscsi_iso_flags(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_per.eight[2]);
}

static inline void
set_mbuf_iscsi_iso_mss(struct mbuf *m, uint16_t mss)
{

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_per.sixteen[2] = mss;
}

static inline uint16_t
mbuf_iscsi_iso_mss(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_per.sixteen[2]);
}

/* t4_tom.c */
struct toepcb *alloc_toepcb(struct vi_info *, int);
int init_toepcb(struct vi_info *, struct toepcb *);
struct toepcb *hold_toepcb(struct toepcb *);
void free_toepcb(struct toepcb *);
void offload_socket(struct socket *, struct toepcb *);
void restore_so_proto(struct socket *, bool);
void undo_offload_socket(struct socket *);
void final_cpl_received(struct toepcb *);
void insert_tid(struct adapter *, int, void *, int);
void *lookup_tid(struct adapter *, int);
void update_tid(struct adapter *, int, void *);
void remove_tid(struct adapter *, int, int);
u_long select_rcv_wnd(struct socket *);
int select_rcv_wscale(void);
void init_conn_params(struct vi_info *, struct offload_settings *,
    struct in_conninfo *, struct socket *, const struct tcp_options *, int16_t,
    struct conn_params *cp);
__be64 calc_options0(struct vi_info *, struct conn_params *);
__be32 calc_options2(struct vi_info *, struct conn_params *);
uint64_t select_ntuple(struct vi_info *, struct l2t_entry *);
int negative_advice(int);
int add_tid_to_history(struct adapter *, u_int);

/* t4_connect.c */
void t4_init_connect_cpl_handlers(void);
void t4_uninit_connect_cpl_handlers(void);
int t4_connect(struct toedev *, struct socket *, struct nhop_object *,
    struct sockaddr *);
void act_open_failure_cleanup(struct adapter *, u_int, u_int);

/* t4_listen.c */
void t4_init_listen_cpl_handlers(void);
void t4_uninit_listen_cpl_handlers(void);
int t4_listen_start(struct toedev *, struct tcpcb *);
int t4_listen_stop(struct toedev *, struct tcpcb *);
void t4_syncache_added(struct toedev *, void *);
void t4_syncache_removed(struct toedev *, void *);
int t4_syncache_respond(struct toedev *, void *, struct mbuf *);
int do_abort_req_synqe(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
int do_abort_rpl_synqe(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
void t4_offload_socket(struct toedev *, void *, struct socket *);
void synack_failure_cleanup(struct adapter *, int);

/* t4_cpl_io.c */
void aiotx_init_toep(struct toepcb *);
int t4_aio_queue_aiotx(struct socket *, struct kaiocb *);
void t4_init_cpl_io_handlers(void);
void t4_uninit_cpl_io_handlers(void);
void send_abort_rpl(struct adapter *, struct sge_ofld_txq *, int , int);
void send_flowc_wr(struct toepcb *, struct tcpcb *);
void send_reset(struct adapter *, struct toepcb *, uint32_t);
int send_rx_credits(struct adapter *, struct toepcb *, int);
void make_established(struct toepcb *, uint32_t, uint32_t, uint16_t);
int t4_close_conn(struct adapter *, struct toepcb *);
void t4_rcvd(struct toedev *, struct tcpcb *);
void t4_rcvd_locked(struct toedev *, struct tcpcb *);
int t4_tod_output(struct toedev *, struct tcpcb *);
int t4_send_fin(struct toedev *, struct tcpcb *);
int t4_send_rst(struct toedev *, struct tcpcb *);
void t4_set_tcb_field(struct adapter *, struct sge_wrq *, struct toepcb *,
    uint16_t, uint64_t, uint64_t, int, int);
void t4_push_frames(struct adapter *, struct toepcb *, int);
void t4_push_pdus(struct adapter *, struct toepcb *, int);

/* t4_ddp.c */
int t4_init_ppod_region(struct ppod_region *, struct t4_range *, u_int,
    const char *);
void t4_free_ppod_region(struct ppod_region *);
int t4_alloc_page_pods_for_ps(struct ppod_region *, struct pageset *);
int t4_alloc_page_pods_for_bio(struct ppod_region *, struct bio *,
    struct ppod_reservation *);
int t4_alloc_page_pods_for_buf(struct ppod_region *, vm_offset_t, int,
    struct ppod_reservation *);
int t4_alloc_page_pods_for_sgl(struct ppod_region *, struct ctl_sg_entry *, int,
    struct ppod_reservation *);
int t4_write_page_pods_for_ps(struct adapter *, struct sge_wrq *, int,
    struct pageset *);
int t4_write_page_pods_for_bio(struct adapter *, struct toepcb *,
    struct ppod_reservation *, struct bio *, struct mbufq *);
int t4_write_page_pods_for_buf(struct adapter *, struct toepcb *,
    struct ppod_reservation *, vm_offset_t, int, struct mbufq *);
int t4_write_page_pods_for_sgl(struct adapter *, struct toepcb *,
    struct ppod_reservation *, struct ctl_sg_entry *, int, int, struct mbufq *);
void t4_free_page_pods(struct ppod_reservation *);
int t4_soreceive_ddp(struct socket *, struct sockaddr **, struct uio *,
    struct mbuf **, struct mbuf **, int *);
int t4_aio_queue_ddp(struct socket *, struct kaiocb *);
void t4_ddp_mod_load(void);
void t4_ddp_mod_unload(void);
void ddp_assert_empty(struct toepcb *);
void ddp_init_toep(struct toepcb *);
void ddp_uninit_toep(struct toepcb *);
void ddp_queue_toep(struct toepcb *);
void release_ddp_resources(struct toepcb *toep);
void handle_ddp_close(struct toepcb *, struct tcpcb *, uint32_t);
void handle_ddp_indicate(struct toepcb *);
void insert_ddp_data(struct toepcb *, uint32_t);
const struct offload_settings *lookup_offload_policy(struct adapter *, int,
    struct mbuf *, uint16_t, struct inpcb *);

/* t4_tls.c */
bool can_tls_offload(struct adapter *);
void do_rx_data_tls(const struct cpl_rx_data *, struct toepcb *, struct mbuf *);
void t4_push_ktls(struct adapter *, struct toepcb *, int);
void tls_received_starting_data(struct adapter *, struct toepcb *,
    struct sockbuf *, int);
void t4_tls_mod_load(void);
void t4_tls_mod_unload(void);
void tls_init_toep(struct toepcb *);
int tls_tx_key(struct toepcb *);
void tls_uninit_toep(struct toepcb *);
int tls_alloc_ktls(struct toepcb *, struct ktls_session *, int);

#endif
