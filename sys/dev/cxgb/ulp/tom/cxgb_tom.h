/**************************************************************************

Copyright (c) 2007, 2009 Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


$FreeBSD$

***************************************************************************/
#ifndef CXGB_TOM_H_
#define CXGB_TOM_H_
#include <sys/protosw.h>
#include <netinet/toecore.h>

MALLOC_DECLARE(M_CXGB);

#define	KTR_CXGB	KTR_SPARE3

#define LISTEN_HASH_SIZE 32 

/*
 * Holds the size, base address, free list start, etc of the TID, server TID,
 * and active-open TID tables for a offload device.
 * The tables themselves are allocated dynamically.
 */
struct tid_info {
	void **tid_tab;
	unsigned int ntids;
	volatile unsigned int tids_in_use;

	union listen_entry *stid_tab;
	unsigned int nstids;
	unsigned int stid_base;

	union active_open_entry *atid_tab;
	unsigned int natids;
	unsigned int atid_base;

	/*
	 * The following members are accessed R/W so we put them in their own
	 * cache lines.  TOM_XXX: actually do what is said here.
	 *
	 * XXX We could combine the atid fields above with the lock here since
	 * atids are use once (unlike other tids).  OTOH the above fields are
	 * usually in cache due to tid_tab.
	 */
	struct mtx atid_lock;
	union active_open_entry *afree;
	unsigned int atids_in_use;

	struct mtx stid_lock;
	union listen_entry *sfree;
	unsigned int stids_in_use;
};

struct tom_data {
        struct toedev tod;

	/*
	 * toepcb's associated with this TOE device are either on the
	 * toep list or in the synq of a listening socket in lctx hash.
	 */
	struct mtx toep_list_lock;
	TAILQ_HEAD(, toepcb) toep_list;

	struct l2t_data *l2t;
	struct tid_info tid_maps;

        /*
	 * The next two locks listen_lock, and tid_release_lock are used rarely
	 * so we let them potentially share a cacheline.
         */

	LIST_HEAD(, listen_ctx) *listen_hash;
	u_long listen_mask;
	int lctx_count;		/* # of lctx in the hash table */
        struct mtx lctx_hash_lock;

        void **tid_release_list;
        struct mtx tid_release_lock;
        struct task tid_release_task;
};

struct synq_entry {
	TAILQ_ENTRY(synq_entry) link;	/* listen_ctx's synq link */
	int flags;			/* same as toepcb's tp_flags */
	int tid;
	struct mbuf *m;			/* backpointer to containing mbuf */
	struct listen_ctx *lctx;	/* backpointer to listen ctx */
	struct cpl_pass_establish *cpl;
	struct toepcb *toep;
	struct l2t_entry *e;
	uint32_t iss;
	uint32_t ts;
	uint32_t opt0h;
	uint32_t qset;
	int rx_credits;
	volatile u_int refcnt;

#define RPL_OK		0	/* ok to reply */
#define RPL_DONE	1	/* replied already */
#define RPL_DONT	2	/* don't reply */
	volatile u_int reply;	/* see above. */
};

#define LCTX_RPL_PENDING	1	/* waiting for CPL_PASS_OPEN_RPL */

struct listen_ctx {
	LIST_ENTRY(listen_ctx) link;	/* listen hash linkage */
	volatile int refcnt;
	int stid;
	int flags;
	struct inpcb *inp;		/* listening socket's inp */
	int qset;
	TAILQ_HEAD(, synq_entry) synq;
};

void t3_process_tid_release_list(void *data, int pending);

static inline struct tom_data *
t3_tomdata(struct toedev *tod)
{

	return (__containerof(tod, struct tom_data, tod));
}

union listen_entry {
	void *ctx;
	union listen_entry *next;
};

union active_open_entry {
	void *ctx;
	union active_open_entry *next;
};

/*
 * Map an ATID or STID to their entries in the corresponding TID tables.
 */
static inline union active_open_entry *atid2entry(const struct tid_info *t,
                                                  unsigned int atid)
{
        return &t->atid_tab[atid - t->atid_base];
}


static inline union listen_entry *stid2entry(const struct tid_info *t,
                                             unsigned int stid)
{
        return &t->stid_tab[stid - t->stid_base];
}

/*
 * Find the connection corresponding to a TID.
 */
static inline void *lookup_tid(const struct tid_info *t, unsigned int tid)
{
	void *p;

	if (tid >= t->ntids)
		return (NULL);

	p = t->tid_tab[tid];
	if (p < (void *)t->tid_tab || p >= (void *)&t->atid_tab[t->natids])
		return (p);

	return (NULL);
}

/*
 * Find the connection corresponding to a server TID.
 */
static inline void *lookup_stid(const struct tid_info *t, unsigned int tid)
{
	void *p;

        if (tid < t->stid_base || tid >= t->stid_base + t->nstids)
                return (NULL);

	p = stid2entry(t, tid)->ctx;
	if (p < (void *)t->tid_tab || p >= (void *)&t->atid_tab[t->natids])
		return (p);

	return (NULL);
}

/*
 * Find the connection corresponding to an active-open TID.
 */
static inline void *lookup_atid(const struct tid_info *t, unsigned int tid)
{
	void *p;

        if (tid < t->atid_base || tid >= t->atid_base + t->natids)
                return (NULL);

	p = atid2entry(t, tid)->ctx;
	if (p < (void *)t->tid_tab || p >= (void *)&t->atid_tab[t->natids])
		return (p);

	return (NULL);
}

static inline uint32_t
calc_opt2(int cpu_idx)
{
	uint32_t opt2 = F_CPU_INDEX_VALID | V_CPU_INDEX(cpu_idx);

	/* 3 = highspeed CC algorithm */
	opt2 |= V_FLAVORS_VALID(1) | V_CONG_CONTROL_FLAVOR(3) |
	    V_PACING_FLAVOR(1);

	/* coalesce and push bit semantics */
	opt2 |= F_RX_COALESCE_VALID | V_RX_COALESCE(3);

	return (htobe32(opt2));
}

/* cxgb_tom.c */
struct toepcb *toepcb_alloc(struct toedev *);
void toepcb_free(struct toepcb *);

/* cxgb_cpl_io.c */
void t3_init_cpl_io(struct adapter *);
int t3_push_frames(struct socket *, int);
int t3_connect(struct toedev *, struct socket *, struct rtentry *,
    struct sockaddr *);
int t3_tod_output(struct toedev *, struct tcpcb *);
int t3_send_rst(struct toedev *, struct tcpcb *);
int t3_send_fin(struct toedev *, struct tcpcb *);
void insert_tid(struct tom_data *, void *, unsigned int);
void update_tid(struct tom_data *, void *, unsigned int);
void remove_tid(struct tom_data *, unsigned int);
uint32_t calc_opt0h(struct socket *, int, int, struct l2t_entry *);
uint32_t calc_opt0l(struct socket *, int);
void queue_tid_release(struct toedev *, unsigned int);
void offload_socket(struct socket *, struct toepcb *);
void undo_offload_socket(struct socket *);
int select_rcv_wscale(void);
unsigned long select_rcv_wnd(struct socket *);
int find_best_mtu_idx(struct adapter *, struct in_conninfo *, int);
void make_established(struct socket *, uint32_t, uint32_t, uint16_t);
void t3_rcvd(struct toedev *, struct tcpcb *);
void t3_pcb_detach(struct toedev *, struct tcpcb *);
void send_abort_rpl(struct toedev *, int, int);
void release_tid(struct toedev *, unsigned int, int);

/* cxgb_listen.c */
void t3_init_listen_cpl_handlers(struct adapter *);
int t3_listen_start(struct toedev *, struct tcpcb *);
int t3_listen_stop(struct toedev *, struct tcpcb *);
void t3_syncache_added(struct toedev *, void *);
void t3_syncache_removed(struct toedev *, void *);
int t3_syncache_respond(struct toedev *, void *, struct mbuf *);
int do_abort_req_synqe(struct sge_qset *, struct rsp_desc *, struct mbuf *);
int do_abort_rpl_synqe(struct sge_qset *, struct rsp_desc *, struct mbuf *);
void t3_offload_socket(struct toedev *, void *, struct socket *);
#endif
