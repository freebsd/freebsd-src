/**************************************************************************

Copyright (c) 2007, 2008 Chelsio Inc.
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

$FreeBSD: src/sys/dev/cxgb/ulp/iw_cxgb/iw_cxgb_cm.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

***************************************************************************/

#ifndef _IWCH_CM_H_
#define _IWCH_CM_H_
#include <contrib/rdma/ib_verbs.h>
#include <contrib/rdma/iw_cm.h>
#include <sys/refcount.h>
#include <sys/condvar.h>
#include <sys/proc.h>


#define MPA_KEY_REQ "MPA ID Req Frame"
#define MPA_KEY_REP "MPA ID Rep Frame"

#define MPA_MAX_PRIVATE_DATA	256
#define MPA_REV		o0	/* XXX - amso1100 uses rev 0 ! */
#define MPA_REJECT		0x20
#define MPA_CRC			0x40
#define MPA_MARKERS		0x80
#define MPA_FLAGS_MASK		0xE0

#define put_ep(ep) { \
	CTR4(KTR_IW_CXGB, "put_ep (via %s:%u) ep %p refcnt %d\n", __FUNCTION__, __LINE__,  \
	     ep, atomic_load_acq_int(&((ep)->refcount))); \
	if (refcount_release(&((ep)->refcount)))  \
		__free_ep(ep); \
}

#define get_ep(ep) { \
	CTR4(KTR_IW_CXGB, "get_ep (via %s:%u) ep %p, refcnt %d\n", __FUNCTION__, __LINE__, \
	     ep, atomic_load_acq_int(&((ep)->refcount))); \
	refcount_acquire(&((ep)->refcount));	  \
}

struct mpa_message {
	u8 key[16];
	u8 flags;
	u8 revision;
	__be16 private_data_size;
	u8 private_data[0];
};

struct terminate_message {
	u8 layer_etype;
	u8 ecode;
	__be16 hdrct_rsvd;
	u8 len_hdrs[0];
};

#define TERM_MAX_LENGTH (sizeof(struct terminate_message) + 2 + 18 + 28)

enum iwch_layers_types {
	LAYER_RDMAP		= 0x00,
	LAYER_DDP		= 0x10,
	LAYER_MPA		= 0x20,
	RDMAP_LOCAL_CATA	= 0x00,
	RDMAP_REMOTE_PROT	= 0x01,
	RDMAP_REMOTE_OP		= 0x02,
	DDP_LOCAL_CATA		= 0x00,
	DDP_TAGGED_ERR		= 0x01,
	DDP_UNTAGGED_ERR	= 0x02,
	DDP_LLP			= 0x03
};

enum iwch_rdma_ecodes {
	RDMAP_INV_STAG		= 0x00,
	RDMAP_BASE_BOUNDS	= 0x01,
	RDMAP_ACC_VIOL		= 0x02,
	RDMAP_STAG_NOT_ASSOC	= 0x03,
	RDMAP_TO_WRAP		= 0x04,
	RDMAP_INV_VERS		= 0x05,
	RDMAP_INV_OPCODE	= 0x06,
	RDMAP_STREAM_CATA	= 0x07,
	RDMAP_GLOBAL_CATA	= 0x08,
	RDMAP_CANT_INV_STAG	= 0x09,
	RDMAP_UNSPECIFIED	= 0xff
};

enum iwch_ddp_ecodes {
	DDPT_INV_STAG		= 0x00,
	DDPT_BASE_BOUNDS	= 0x01,
	DDPT_STAG_NOT_ASSOC	= 0x02,
	DDPT_TO_WRAP		= 0x03,
	DDPT_INV_VERS		= 0x04,
	DDPU_INV_QN		= 0x01,
	DDPU_INV_MSN_NOBUF	= 0x02,
	DDPU_INV_MSN_RANGE	= 0x03,
	DDPU_INV_MO		= 0x04,
	DDPU_MSG_TOOBIG		= 0x05,
	DDPU_INV_VERS		= 0x06
};

enum iwch_mpa_ecodes {
	MPA_CRC_ERR		= 0x02,
	MPA_MARKER_ERR		= 0x03
};

enum iwch_ep_state {
	IDLE = 0,
	LISTEN,
	CONNECTING,
	MPA_REQ_WAIT,
	MPA_REQ_SENT,
	MPA_REQ_RCVD,
	MPA_REP_SENT,
	FPDU_MODE,
	ABORTING,
	CLOSING,
	MORIBUND,
	DEAD,
};

enum iwch_ep_flags {
	PEER_ABORT_IN_PROGRESS	= (1 << 0),
	ABORT_REQ_IN_PROGRESS	= (1 << 1),
};

struct iwch_ep_common {
	TAILQ_ENTRY(iwch_ep_common) entry;
	struct iw_cm_id *cm_id;
	struct iwch_qp *qp;
	struct t3cdev *tdev;
	enum iwch_ep_state state;
	u_int refcount;
	struct cv waitq;
	struct mtx lock;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	int rpl_err;
	int rpl_done;
	struct thread *thread;
	struct socket *so;
};

struct iwch_listen_ep {
	struct iwch_ep_common com;
	unsigned int stid;
	int backlog;
};

struct iwch_ep {
	struct iwch_ep_common com;
	struct iwch_ep *parent_ep;
	struct callout timer;
	unsigned int atid;
	u32 hwtid;
	u32 snd_seq;
	u32 rcv_seq;
	struct l2t_entry *l2t;
	struct rtentry *dst;
	struct mbuf *mpa_mbuf;
	struct iwch_mpa_attributes mpa_attr;
	unsigned int mpa_pkt_len;
	u8 mpa_pkt[sizeof(struct mpa_message) + MPA_MAX_PRIVATE_DATA];
	u8 tos;
	u16 emss;
	u16 plen;
	u32 ird;
	u32 ord;
	u32 flags;
};

static inline struct iwch_ep *to_ep(struct iw_cm_id *cm_id)
{
	return cm_id->provider_data;
}

static inline struct iwch_listen_ep *to_listen_ep(struct iw_cm_id *cm_id)
{
	return cm_id->provider_data;
}

static inline int compute_wscale(int win)
{
	int wscale = 0;

	while (wscale < 14 && (65535<<wscale) < win)
		wscale++;
	return wscale;
}

static __inline void
iwch_wait(struct cv *cv, struct mtx *lock, int *rpl_done)
{
	mtx_lock(lock);
	if (!*rpl_done) {
		CTR0(KTR_IW_CXGB, "sleeping for rpl_done\n");
		cv_wait_unlock(cv, lock);
	}
	CTR1(KTR_IW_CXGB, "*rpl_done=%d\n", *rpl_done);
}

static __inline void
iwch_wakeup(struct cv *cv, struct mtx *lock, int *rpl_done)
{
	mtx_lock(lock);
	*rpl_done=1;	
	CTR0(KTR_IW_CXGB, "wakeup for rpl_done\n");
	cv_broadcast(cv);
	mtx_unlock(lock);	
}

/* CM prototypes */

int iwch_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param);
int iwch_create_listen(struct iw_cm_id *cm_id, int backlog);
int iwch_destroy_listen(struct iw_cm_id *cm_id);
int iwch_reject_cr(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len);
int iwch_accept_cr(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param);
int iwch_ep_disconnect(struct iwch_ep *ep, int abrupt, int flags);
int iwch_quiesce_tid(struct iwch_ep *ep);
int iwch_resume_tid(struct iwch_ep *ep);
void __free_ep(struct iwch_ep_common *ep);
void iwch_rearp(struct iwch_ep *ep);
int iwch_ep_redirect(void *ctx, struct rtentry *old, struct rtentry *new, struct l2t_entry *l2t);

int iwch_cm_init(void);
void iwch_cm_term(void);

#endif				/* _IWCH_CM_H_ */
