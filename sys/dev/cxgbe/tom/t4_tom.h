/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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
 * $FreeBSD$
 *
 */

#ifndef __T4_TOM_H__
#define __T4_TOM_H__

#define KTR_CXGBE	KTR_SPARE3
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

/* TOE PCB flags */
enum {
	TPF_ATTACHED,		/* a tcpcb refers to this toepcb */
	TPF_FLOWC_WR_SENT,	/* firmware flow context WR sent */
	TPF_TX_DATA_SENT,	/* some data sent */
	TPF_TX_SUSPENDED,	/* tx suspended for lack of resources */
	TPF_SEND_FIN,		/* send FIN after sending all pending data */
	TPF_FIN_SENT,		/* FIN has been sent */
	TPF_ABORT_SHUTDOWN,	/* connection abort is in progress */
	TPF_CPL_PENDING,	/* haven't received the last CPL */
	TPF_SYNQE,		/* synq_entry, not really a toepcb */
	TPF_SYNQE_NEEDFREE,	/* synq_entry was allocated externally */
};

struct ofld_tx_sdesc {
	uint32_t plen;		/* payload length */
	uint8_t tx_credits;	/* firmware tx credits (unit is 16B) */
};

struct toepcb {
	TAILQ_ENTRY(toepcb) link; /* toep_list */
	unsigned int flags;	/* miscellaneous flags */
	struct tom_data *td;
	struct inpcb *inp;	/* backpointer to host stack's PCB */
	struct port_info *port;	/* physical port */
	struct sge_wrq *ofld_txq;
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ctrlq;
	struct l2t_entry *l2te;	/* L2 table entry used by this connection */
	int tid;		/* Connection identifier */
	unsigned int tx_credits;/* tx WR credits (in 16 byte units) remaining */
	unsigned int enqueued;	/* # of bytes added to so_rcv (not yet read) */
	int rx_credits;		/* rx credits (in bytes) to be returned to hw */

	unsigned int ulp_mode;	/* ULP mode */

	/* Tx software descriptor */
	uint8_t txsd_total;
	uint8_t txsd_pidx;
	uint8_t txsd_cidx;
	uint8_t txsd_avail;
	struct ofld_tx_sdesc txsd[];
};

struct flowc_tx_params {
	uint32_t snd_nxt;
	uint32_t rcv_nxt;
	unsigned int snd_space;
	unsigned int mss;
};

static inline int
toepcb_flag(struct toepcb *toep, int flag)
{

	return isset(&toep->flags, flag);
}

static inline void
toepcb_set_flag(struct toepcb *toep, int flag)
{

	setbit(&toep->flags, flag);
}

static inline void
toepcb_clr_flag(struct toepcb *toep, int flag)
{

	clrbit(&toep->flags, flag);
}

/*
 * Compressed state for embryonic connections for a listener.  Barely fits in
 * 64B, try not to grow it further.
 */
struct synq_entry {
	TAILQ_ENTRY(synq_entry) link;	/* listen_ctx's synq link */
	int flags;			/* same as toepcb's tp_flags */
	int tid;
	struct listen_ctx *lctx;	/* backpointer to listen ctx */
	struct mbuf *syn;
	uint32_t iss;
	uint32_t ts;
	volatile uintptr_t wr;
	volatile u_int refcnt;
	uint16_t l2e_idx;
	uint16_t rcv_bufsize;
};

static inline int
synqe_flag(struct synq_entry *synqe, int flag)
{

	return isset(&synqe->flags, flag);
}

static inline void
synqe_set_flag(struct synq_entry *synqe, int flag)
{

	setbit(&synqe->flags, flag);
}

static inline void
synqe_clr_flag(struct synq_entry *synqe, int flag)
{

	clrbit(&synqe->flags, flag);
}

/* listen_ctx flags */
#define LCTX_RPL_PENDING 1	/* waiting for a CPL_PASS_OPEN_RPL */

struct listen_ctx {
	LIST_ENTRY(listen_ctx) link;	/* listen hash linkage */
	volatile int refcount;
	int stid;
	int flags;
	struct inpcb *inp;		/* listening socket's inp */
	struct sge_wrq *ctrlq;
	struct sge_ofld_rxq *ofld_rxq;
	TAILQ_HEAD(, synq_entry) synq;
};

struct tom_data {
	struct toedev tod;

	/* toepcb's associated with this TOE device */
	struct mtx toep_list_lock;
	TAILQ_HEAD(, toepcb) toep_list;

	LIST_HEAD(, listen_ctx) *listen_hash;
	u_long listen_mask;
	int lctx_count;		/* # of lctx in the hash table */
	struct mtx lctx_hash_lock;
};

static inline struct tom_data *
tod_td(struct toedev *tod)
{

	return (member2struct(tom_data, tod, tod));
}

static inline struct adapter *
td_adapter(struct tom_data *td)
{

	return (td->tod.tod_softc);
}

/* t4_tom.c */
struct toepcb *alloc_toepcb(struct port_info *, int, int, int);
void free_toepcb(struct toepcb *);
void offload_socket(struct socket *, struct toepcb *);
void undo_offload_socket(struct socket *);
void final_cpl_received(struct toepcb *);
void insert_tid(struct adapter *, int, void *);
void *lookup_tid(struct adapter *, int);
void update_tid(struct adapter *, int, void *);
void remove_tid(struct adapter *, int);
void release_tid(struct adapter *, int, struct sge_wrq *);
int find_best_mtu_idx(struct adapter *, struct in_conninfo *, int);
u_long select_rcv_wnd(struct socket *);
int select_rcv_wscale(void);
uint64_t calc_opt0(struct socket *, struct port_info *, struct l2t_entry *,
    int, int, int, int);
uint32_t select_ntuple(struct port_info *, struct l2t_entry *, uint32_t);

/* t4_connect.c */
void t4_init_connect_cpl_handlers(struct adapter *);
int t4_connect(struct toedev *, struct socket *, struct rtentry *,
    struct sockaddr *);

/* t4_listen.c */
void t4_init_listen_cpl_handlers(struct adapter *);
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

/* t4_cpl_io.c */
void t4_init_cpl_io_handlers(struct adapter *);
void send_abort_rpl(struct adapter *, struct sge_wrq *, int , int);
void send_flowc_wr(struct toepcb *, struct flowc_tx_params *);
void send_reset(struct adapter *, struct toepcb *, uint32_t);
void make_established(struct toepcb *, uint32_t, uint32_t, uint16_t);
void t4_rcvd(struct toedev *, struct tcpcb *);
int t4_tod_output(struct toedev *, struct tcpcb *);
int t4_send_fin(struct toedev *, struct tcpcb *);
int t4_send_rst(struct toedev *, struct tcpcb *);

#endif
