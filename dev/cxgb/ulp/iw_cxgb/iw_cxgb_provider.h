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

$FreeBSD$

***************************************************************************/
#ifndef __IWCH_PROVIDER_H__
#define __IWCH_PROVIDER_H__

#include <rdma/ib_verbs.h>

struct iwch_pd {
	struct ib_pd ibpd;
	u32 pdid;
	struct iwch_dev *rhp;
};

#ifndef container_of
#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))
#endif
static __inline struct iwch_pd *
to_iwch_pd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct iwch_pd, ibpd);
}

struct tpt_attributes {
	u32 stag;
	u32 state:1;
	u32 type:2;
	u32 rsvd:1;
	enum tpt_mem_perm perms;
	u32 remote_invaliate_disable:1;
	u32 zbva:1;
	u32 mw_bind_enable:1;
	u32 page_size:5;

	u32 pdid;
	u32 qpid;
	u32 pbl_addr;
	u32 len;
	u64 va_fbo;
	u32 pbl_size;
};

struct iwch_mr {
	struct ib_mr ibmr;
	struct ib_umem *umem;
	struct iwch_dev *rhp;
	u64 kva;
	struct tpt_attributes attr;
};

typedef struct iwch_mw iwch_mw_handle;

static __inline struct iwch_mr *
to_iwch_mr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct iwch_mr, ibmr);
}

struct iwch_mw {
	struct ib_mw ibmw;
	struct iwch_dev *rhp;
	u64 kva;
	struct tpt_attributes attr;
};

static __inline struct iwch_mw *
to_iwch_mw(struct ib_mw *ibmw)
{
	return container_of(ibmw, struct iwch_mw, ibmw);
}

struct iwch_cq {
	struct ib_cq ibcq;
	struct iwch_dev *rhp;
	struct t3_cq cq;
	struct mtx lock;
	int refcnt;
	u32 /* __user */ *user_rptr_addr;
};

static __inline struct iwch_cq *
to_iwch_cq(struct ib_cq *ibcq)
{
	return container_of(ibcq, struct iwch_cq, ibcq);
}

enum IWCH_QP_FLAGS {
	QP_QUIESCED = 0x01
};

struct iwch_mpa_attributes {
	u8 initiator;
	u8 recv_marker_enabled;
	u8 xmit_marker_enabled;	/* iWARP: enable inbound Read Resp. */
	u8 crc_enabled;
	u8 version;	/* 0 or 1 */
};

struct iwch_qp_attributes {
	u32 scq;
	u32 rcq;
	u32 sq_num_entries;
	u32 rq_num_entries;
	u32 sq_max_sges;
	u32 sq_max_sges_rdma_write;
	u32 rq_max_sges;
	u32 state;
	u8 enable_rdma_read;
	u8 enable_rdma_write;	/* enable inbound Read Resp. */
	u8 enable_bind;
	u8 enable_mmid0_fastreg;	/* Enable STAG0 + Fast-register */
	/*
	 * Next QP state. If specify the current state, only the
	 * QP attributes will be modified.
	 */
	u32 max_ord;
	u32 max_ird;
	u32 pd;	/* IN */
	u32 next_state;
	char terminate_buffer[52];
	u32 terminate_msg_len;
	u8 is_terminate_local;
	struct iwch_mpa_attributes mpa_attr;	/* IN-OUT */
	struct iwch_ep *llp_stream_handle;
	char *stream_msg_buf;	/* Last stream msg. before Idle -> RTS */
	u32 stream_msg_buf_len;	/* Only on Idle -> RTS */
};

struct iwch_qp {
	struct ib_qp ibqp;
	struct iwch_dev *rhp;
	struct iwch_ep *ep;
	struct iwch_qp_attributes attr;
	struct t3_wq wq;
	struct mtx lock;
	int refcnt;
	enum IWCH_QP_FLAGS flags;
	struct callout timer;
};

static __inline int
qp_quiesced(struct iwch_qp *qhp)
{
	return qhp->flags & QP_QUIESCED;
}

static __inline struct iwch_qp *
to_iwch_qp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct iwch_qp, ibqp);
}

void iwch_qp_add_ref(struct ib_qp *qp);
void iwch_qp_rem_ref(struct ib_qp *qp);

struct iwch_ucontext {
	struct ib_ucontext ibucontext;
	struct cxio_ucontext uctx;
	u32 key;
	struct mtx mmap_lock;
	TAILQ_HEAD( ,iwch_mm_entry) mmaps;
};

static __inline struct iwch_ucontext *
to_iwch_ucontext(struct ib_ucontext *c)
{
	return container_of(c, struct iwch_ucontext, ibucontext);
}

struct iwch_mm_entry {
	TAILQ_ENTRY(iwch_mm_entry) entry;
	u64 addr;
	u32 key;
	unsigned len;
};

static __inline struct iwch_mm_entry *
remove_mmap(struct iwch_ucontext *ucontext,
						u32 key, unsigned len)
{
	struct iwch_mm_entry *tmp, *mm;

	mtx_lock(&ucontext->mmap_lock);
	TAILQ_FOREACH_SAFE(mm, &ucontext->mmaps, entry, tmp) {
		if (mm->key == key && mm->len == len) {
			TAILQ_REMOVE(&ucontext->mmaps, mm, entry);
			mtx_unlock(&ucontext->mmap_lock);
			CTR4(KTR_IW_CXGB, "%s key 0x%x addr 0x%llx len %d\n", __FUNCTION__,
			     key, (unsigned long long) mm->addr, mm->len);
			return mm;
		}
	}
	mtx_unlock(&ucontext->mmap_lock);

	return NULL;
}

static __inline void
insert_mmap(struct iwch_ucontext *ucontext,
			       struct iwch_mm_entry *mm)
{
	mtx_lock(&ucontext->mmap_lock);
	CTR4(KTR_IW_CXGB, "%s key 0x%x addr 0x%llx len %d\n", __FUNCTION__,
	     mm->key, (unsigned long long) mm->addr, mm->len);
	TAILQ_INSERT_TAIL(&ucontext->mmaps, mm, entry);
	mtx_unlock(&ucontext->mmap_lock);
}

enum iwch_qp_attr_mask {
	IWCH_QP_ATTR_NEXT_STATE = 1 << 0,
	IWCH_QP_ATTR_ENABLE_RDMA_READ = 1 << 7,
	IWCH_QP_ATTR_ENABLE_RDMA_WRITE = 1 << 8,
	IWCH_QP_ATTR_ENABLE_RDMA_BIND = 1 << 9,
	IWCH_QP_ATTR_MAX_ORD = 1 << 11,
	IWCH_QP_ATTR_MAX_IRD = 1 << 12,
	IWCH_QP_ATTR_LLP_STREAM_HANDLE = 1 << 22,
	IWCH_QP_ATTR_STREAM_MSG_BUFFER = 1 << 23,
	IWCH_QP_ATTR_MPA_ATTR = 1 << 24,
	IWCH_QP_ATTR_QP_CONTEXT_ACTIVATE = 1 << 25,
	IWCH_QP_ATTR_VALID_MODIFY = (IWCH_QP_ATTR_ENABLE_RDMA_READ |
				     IWCH_QP_ATTR_ENABLE_RDMA_WRITE |
				     IWCH_QP_ATTR_MAX_ORD |
				     IWCH_QP_ATTR_MAX_IRD |
				     IWCH_QP_ATTR_LLP_STREAM_HANDLE |
				     IWCH_QP_ATTR_STREAM_MSG_BUFFER |
				     IWCH_QP_ATTR_MPA_ATTR |
				     IWCH_QP_ATTR_QP_CONTEXT_ACTIVATE)
};

int iwch_modify_qp(struct iwch_dev *rhp,
				struct iwch_qp *qhp,
				enum iwch_qp_attr_mask mask,
				struct iwch_qp_attributes *attrs,
				int internal);

enum iwch_qp_state {
	IWCH_QP_STATE_IDLE,
	IWCH_QP_STATE_RTS,
	IWCH_QP_STATE_ERROR,
	IWCH_QP_STATE_TERMINATE,
	IWCH_QP_STATE_CLOSING,
	IWCH_QP_STATE_TOT
};

static __inline int
iwch_convert_state(enum ib_qp_state ib_state)
{
	switch (ib_state) {
	case IB_QPS_RESET:
	case IB_QPS_INIT:
		return IWCH_QP_STATE_IDLE;
	case IB_QPS_RTS:
		return IWCH_QP_STATE_RTS;
	case IB_QPS_SQD:
		return IWCH_QP_STATE_CLOSING;
	case IB_QPS_SQE:
		return IWCH_QP_STATE_TERMINATE;
	case IB_QPS_ERR:
		return IWCH_QP_STATE_ERROR;
	default:
		return -1;
	}
}

static __inline u32
iwch_ib_to_tpt_access(int acc)
{
	return (acc & IB_ACCESS_REMOTE_WRITE ? TPT_REMOTE_WRITE : 0) |
	       (acc & IB_ACCESS_REMOTE_READ ? TPT_REMOTE_READ : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE ? TPT_LOCAL_WRITE : 0) |
	       TPT_LOCAL_READ;
}

static __inline u32
iwch_ib_to_mwbind_access(int acc)
{
	return (acc & IB_ACCESS_REMOTE_WRITE ? T3_MEM_ACCESS_REM_WRITE : 0) |
	       (acc & IB_ACCESS_REMOTE_READ ? T3_MEM_ACCESS_REM_READ : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE ? T3_MEM_ACCESS_LOCAL_WRITE : 0) |
	       T3_MEM_ACCESS_LOCAL_READ;
}

enum iwch_mmid_state {
	IWCH_STAG_STATE_VALID,
	IWCH_STAG_STATE_INVALID
};

enum iwch_qp_query_flags {
	IWCH_QP_QUERY_CONTEXT_NONE = 0x0,	/* No ctx; Only attrs */
	IWCH_QP_QUERY_CONTEXT_GET = 0x1,	/* Get ctx + attrs */
	IWCH_QP_QUERY_CONTEXT_SUSPEND = 0x2,	/* Not Supported */

	/*
	 * Quiesce QP context; Consumer
	 * will NOT replay outstanding WR
	 */
	IWCH_QP_QUERY_CONTEXT_QUIESCE = 0x4,
	IWCH_QP_QUERY_CONTEXT_REMOVE = 0x8,
	IWCH_QP_QUERY_TEST_USERWRITE = 0x32	/* Test special */
};

int iwch_post_send(struct ib_qp *ibqp, struct ib_send_wr *wr,
		      struct ib_send_wr **bad_wr);
int iwch_post_receive(struct ib_qp *ibqp, struct ib_recv_wr *wr,
		      struct ib_recv_wr **bad_wr);
int iwch_bind_mw(struct ib_qp *qp,
			     struct ib_mw *mw,
			     struct ib_mw_bind *mw_bind);
int iwch_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc);
int iwch_post_terminate(struct iwch_qp *qhp, struct respQ_msg_t *rsp_msg);
int iwch_register_device(struct iwch_dev *dev);
void iwch_unregister_device(struct iwch_dev *dev);
void stop_read_rep_timer(struct iwch_qp *qhp);
int iwch_register_mem(struct iwch_dev *rhp, struct iwch_pd *php,
					struct iwch_mr *mhp,
					int shift);
int iwch_reregister_mem(struct iwch_dev *rhp, struct iwch_pd *php,
					struct iwch_mr *mhp,
					int shift,
					int npages);
int iwch_alloc_pbl(struct iwch_mr *mhp, int npages);
void iwch_free_pbl(struct iwch_mr *mhp);
int iwch_write_pbl(struct iwch_mr *mhp, __be64 *pages, int npages, int offset);
int build_phys_page_list(struct ib_phys_buf *buffer_list,
					int num_phys_buf,
					u64 *iova_start,
					u64 *total_size,
					int *npages,
					int *shift,
					__be64 **page_list);


#define IWCH_NODE_DESC "cxgb3 Chelsio Communications"

#endif
