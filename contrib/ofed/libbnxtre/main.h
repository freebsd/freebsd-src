/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __BNXT_RE_MAIN_H__
#define __BNXT_RE_MAIN_H__

#include <sys/param.h>
#include <sys/queue.h>

#include <infiniband/driver.h>
#include <infiniband/endian.h>
#include <infiniband/udma_barrier.h>

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "abi.h"
#include "list.h"
#include "memory.h"

#define DEV	"bnxt_re : "
#define BNXT_RE_UD_QP_STALL	0x400000

#define CHIP_NUM_57508		0x1750
#define CHIP_NUM_57504		0x1751
#define CHIP_NUM_57502		0x1752
#define CHIP_NUM_58818          0xd818
#define CHIP_NUM_57608		0x1760

#define BNXT_NSEC_PER_SEC	1000000000UL

struct bnxt_re_chip_ctx {
	__u16	chip_num;
	__u8	chip_rev;
	__u8	chip_metal;
	bool	chip_is_gen_p5_thor2;
};

#define BNXT_RE_MAP_WC	0x1000
#define BNXT_RE_DBR_PAGE 0x2000
#define BNXT_RE_DB_RECOVERY_PAGE 0x3000

#define BNXT_RE_DB_REPLAY_YIELD_CNT 256
#define BNXT_RE_DB_KEY_INVALID -1
#define BNXT_RE_MAX_DO_PACING 0xFFFF
#define bnxt_re_wm_barrier()		udma_to_device_barrier()
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define CNA(v, d)					\
	{	.vendor = PCI_VENDOR_ID_##v,		\
		.device = d }
#define BNXT_RE_DEFINE_CNA_TABLE(_name)			\
	static const struct {				\
		unsigned vendor;			\
		unsigned device;			\
	} _name[]

struct bnxt_re_dpi {
	__u32 dpindx;
	__u32 wcdpi;
	__u64 *dbpage;
	__u64 *wcdbpg;
};

struct bnxt_re_pd {
	struct ibv_pd ibvpd;
	uint32_t pdid;
};

struct xorshift32_state {
	uint32_t seed;
};

struct bnxt_re_cq {
	struct ibv_cq ibvcq;
	struct bnxt_re_list_head sfhead;
	struct bnxt_re_list_head rfhead;
	struct bnxt_re_list_head prev_cq_head;
	struct bnxt_re_context *cntx;
	struct bnxt_re_queue *cqq;
	struct bnxt_re_dpi *udpi;
	struct bnxt_re_mem *resize_mem;
	struct bnxt_re_mem *mem;
	struct bnxt_re_list_node dbnode;
	uint64_t shadow_db_key;
	uint32_t cqe_sz;
	uint32_t cqid;
	struct xorshift32_state rand;
	int deferred_arm_flags;
	bool first_arm;
	bool deferred_arm;
	bool phase;
	uint8_t dbr_lock;
	void *cq_page;
};

struct bnxt_re_push_buffer {
	__u64 *pbuf; /*push wc buffer */
	__u64 *wqe; /* hwqe addresses */
	__u64 *ucdb;
	uint32_t st_idx;
	uint32_t qpid;
	uint16_t wcdpi;
	uint16_t nbit;
	uint32_t tail;
};

enum bnxt_re_push_info_mask {
	BNXT_RE_PUSH_SIZE_MASK  = 0x1FUL,
	BNXT_RE_PUSH_SIZE_SHIFT = 0x18UL
};

struct bnxt_re_db_ppp_hdr {
	struct bnxt_re_db_hdr db_hdr;
	__u64 rsv_psz_pidx;
};

struct bnxt_re_push_rec {
	struct bnxt_re_dpi *udpi;
	struct bnxt_re_push_buffer *pbuf;
	__u32 pbmap; /* only 16 bits in use */
};

struct bnxt_re_wrid {
	uint64_t wrid;
	int next_idx;
	uint32_t bytes;
	uint8_t sig;
	uint8_t slots;
	uint8_t wc_opcd;
};

struct bnxt_re_qpcap {
	uint32_t max_swr;
	uint32_t max_rwr;
	uint32_t max_ssge;
	uint32_t max_rsge;
	uint32_t max_inline;
	uint8_t	sqsig;
	uint8_t is_atomic_cap;
};

struct bnxt_re_srq {
	struct ibv_srq ibvsrq;
	struct ibv_srq_attr cap;
	uint32_t srqid;
	struct bnxt_re_context *uctx;
	struct bnxt_re_queue *srqq;
	struct bnxt_re_wrid *srwrid;
	struct bnxt_re_dpi *udpi;
	struct bnxt_re_mem *mem;
	int start_idx;
	int last_idx;
	struct bnxt_re_list_node dbnode;
	uint64_t shadow_db_key;
	struct xorshift32_state rand;
	uint8_t dbr_lock;
	bool arm_req;
};

struct bnxt_re_joint_queue {
	struct bnxt_re_context *cntx;
	struct bnxt_re_queue *hwque;
	struct bnxt_re_wrid *swque;
	uint32_t start_idx;
	uint32_t last_idx;
};

struct bnxt_re_qp {
	struct ibv_qp ibvqp;
	struct bnxt_re_qpcap cap;
	struct bnxt_re_context *cntx;
	struct bnxt_re_chip_ctx *cctx;
	struct bnxt_re_joint_queue *jsqq;
	struct bnxt_re_joint_queue *jrqq;
	struct bnxt_re_dpi *udpi;
	uint64_t wqe_cnt;
	uint16_t mtu;
	uint16_t qpst;
	uint8_t qptyp;
	uint8_t qpmode;
	uint8_t push_st_en;
	uint8_t ppp_idx;
	uint32_t sq_psn;
	uint32_t sq_msn;
	uint32_t qpid;
	uint16_t max_push_sz;
	uint8_t sq_dbr_lock;
	uint8_t rq_dbr_lock;
	struct xorshift32_state rand;
	struct bnxt_re_list_node snode;
	struct bnxt_re_list_node rnode;
	struct bnxt_re_srq *srq;
	struct bnxt_re_cq *rcq;
	struct bnxt_re_cq *scq;
	struct bnxt_re_mem *mem;/* at cl 6 */
	struct bnxt_re_list_node dbnode;
	uint64_t sq_shadow_db_key;
	uint64_t rq_shadow_db_key;
};

struct bnxt_re_mr {
	struct ibv_mr vmr;
};

struct bnxt_re_ah {
	struct ibv_ah ibvah;
	struct bnxt_re_pd *pd;
	uint32_t avid;
};

struct bnxt_re_dev {
	struct verbs_device vdev;
	struct ibv_device_attr devattr;
	uint32_t pg_size;
	uint32_t cqe_size;
	uint32_t max_cq_depth;
	uint8_t abi_version;
};

struct bnxt_re_res_list {
	struct bnxt_re_list_head head;
	pthread_spinlock_t lock;
};

struct bnxt_re_context {
	struct ibv_context ibvctx;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_chip_ctx *cctx;
	uint64_t comp_mask;
	struct bnxt_re_dpi udpi;
	uint32_t dev_id;
	uint32_t max_qp;
	uint32_t max_srq;
	uint32_t modes;
	void *shpg;
	pthread_mutex_t shlock;
	struct bnxt_re_push_rec *pbrec;
	void *dbr_page;
	void *bar_map;
	struct bnxt_re_res_list qp_dbr_res;
	struct bnxt_re_res_list cq_dbr_res;
	struct bnxt_re_res_list srq_dbr_res;
	void *db_recovery_page;
	struct ibv_comp_channel *dbr_ev_chan;
	struct ibv_cq *dbr_cq;
	pthread_t dbr_thread;
	uint64_t replay_cnt;
};

struct bnxt_re_pacing_data {
	uint32_t do_pacing;
	uint32_t pacing_th;
	uint32_t dev_err_state;
	uint32_t alarm_th;
};

/* Chip context related functions */
bool _is_chip_gen_p5(struct bnxt_re_chip_ctx *cctx);
bool _is_chip_a0(struct bnxt_re_chip_ctx *cctx);
bool _is_chip_thor2(struct bnxt_re_chip_ctx *cctx);
bool _is_chip_gen_p5_thor2(struct bnxt_re_chip_ctx *cctx);

/* DB ring functions used internally*/
void bnxt_re_ring_rq_db(struct bnxt_re_qp *qp);
void bnxt_re_ring_sq_db(struct bnxt_re_qp *qp);
void bnxt_re_ring_srq_arm(struct bnxt_re_srq *srq);
void bnxt_re_ring_srq_db(struct bnxt_re_srq *srq);
void bnxt_re_ring_cq_db(struct bnxt_re_cq *cq);
void bnxt_re_ring_cq_arm_db(struct bnxt_re_cq *cq, uint8_t aflag);

void bnxt_re_ring_pstart_db(struct bnxt_re_qp *qp,
			    struct bnxt_re_push_buffer *pbuf);
void bnxt_re_ring_pend_db(struct bnxt_re_qp *qp,
			  struct bnxt_re_push_buffer *pbuf);
void bnxt_re_fill_push_wcb(struct bnxt_re_qp *qp,
			   struct bnxt_re_push_buffer *pbuf,
			   uint32_t idx);

void bnxt_re_fill_ppp(struct bnxt_re_push_buffer *pbuf,
		      struct bnxt_re_qp *qp, uint8_t len, uint32_t idx);
int bnxt_re_init_pbuf_list(struct bnxt_re_context *cntx);
void bnxt_re_destroy_pbuf_list(struct bnxt_re_context *cntx);
struct bnxt_re_push_buffer *bnxt_re_get_pbuf(uint8_t *push_st_en,
					     uint8_t ppp_idx,
					     struct bnxt_re_context *cntx);
void bnxt_re_put_pbuf(struct bnxt_re_context *cntx,
		      struct bnxt_re_push_buffer *pbuf);

void bnxt_re_db_recovery(struct bnxt_re_context *cntx);
void *bnxt_re_dbr_thread(void *arg);
bool _is_db_drop_recovery_enable(struct bnxt_re_context *cntx);
int bnxt_re_poll_kernel_cq(struct bnxt_re_cq *cq);
extern int bnxt_single_threaded;
extern int bnxt_dyn_debug;

#define bnxt_re_trace(fmt, ...)					\
{								\
	if (bnxt_dyn_debug)					\
		fprintf(stderr, fmt, ##__VA_ARGS__);		\
}

/* pointer conversion functions*/
static inline struct bnxt_re_dev *to_bnxt_re_dev(struct ibv_device *ibvdev)
{
	return container_of(ibvdev, struct bnxt_re_dev, vdev);
}

static inline struct bnxt_re_context *to_bnxt_re_context(
		struct ibv_context *ibvctx)
{
	return container_of(ibvctx, struct bnxt_re_context, ibvctx);
}

static inline struct bnxt_re_pd *to_bnxt_re_pd(struct ibv_pd *ibvpd)
{
	return container_of(ibvpd, struct bnxt_re_pd, ibvpd);
}

static inline struct bnxt_re_cq *to_bnxt_re_cq(struct ibv_cq *ibvcq)
{
	return container_of(ibvcq, struct bnxt_re_cq, ibvcq);
}

static inline struct bnxt_re_qp *to_bnxt_re_qp(struct ibv_qp *ibvqp)
{
	return container_of(ibvqp, struct bnxt_re_qp, ibvqp);
}

static inline struct bnxt_re_srq *to_bnxt_re_srq(struct ibv_srq *ibvsrq)
{
	return container_of(ibvsrq, struct bnxt_re_srq, ibvsrq);
}

static inline struct bnxt_re_ah *to_bnxt_re_ah(struct ibv_ah *ibvah)
{
	return container_of(ibvah, struct bnxt_re_ah, ibvah);
}

/* CQE manipulations */
#define bnxt_re_get_cqe_sz()	(sizeof(struct bnxt_re_req_cqe) +	\
				 sizeof(struct bnxt_re_bcqe))
#define bnxt_re_get_sqe_hdr_sz()	(sizeof(struct bnxt_re_bsqe) +	\
					 sizeof(struct bnxt_re_send))
#define bnxt_re_get_srqe_hdr_sz()	(sizeof(struct bnxt_re_brqe) +	\
					 sizeof(struct bnxt_re_srqe))
#define bnxt_re_get_srqe_sz()		(sizeof(struct bnxt_re_brqe) +	\
					 sizeof(struct bnxt_re_srqe) +	\
					 BNXT_RE_MAX_INLINE_SIZE)
#define bnxt_re_is_cqe_valid(valid, phase)				\
				(((valid) & BNXT_RE_BCQE_PH_MASK) == (phase))

static inline void bnxt_re_change_cq_phase(struct bnxt_re_cq *cq)
{
	if (!cq->cqq->head)
		cq->phase = !(cq->phase & BNXT_RE_BCQE_PH_MASK);
}

static inline void *bnxt_re_get_swqe(struct bnxt_re_joint_queue *jqq,
				     uint32_t *wqe_idx)
{
	if (wqe_idx)
		*wqe_idx = jqq->start_idx;
	return &jqq->swque[jqq->start_idx];
}

static inline void bnxt_re_jqq_mod_start(struct bnxt_re_joint_queue *jqq,
					 uint32_t idx)
{
	jqq->start_idx = jqq->swque[idx].next_idx;
}

static inline void bnxt_re_jqq_mod_last(struct bnxt_re_joint_queue *jqq,
					uint32_t idx)
{
	jqq->last_idx = jqq->swque[idx].next_idx;
}

static inline uint32_t bnxt_re_init_depth(uint32_t ent, uint64_t cmask)
{
	return cmask & BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED ?
		ent : roundup_pow_of_two(ent);
}

static inline uint32_t bnxt_re_get_diff(uint64_t cmask)
{
	return cmask & BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED ?
		0 : BNXT_RE_FULL_FLAG_DELTA;
}

static inline int bnxt_re_calc_wqe_sz(int nsge)
{
	/* This is used for both sq and rq. In case hdr size differs
	 * in future move to individual functions.
	 */
	return sizeof(struct bnxt_re_sge) * nsge + bnxt_re_get_sqe_hdr_sz();
}

/* Helper function to copy to push buffers */
static inline void bnxt_re_copy_data_to_pb(struct bnxt_re_push_buffer *pbuf,
					   uint8_t offset, uint32_t idx)
{
	__u64 *src;
	__u64 *dst;
	int indx;

	for (indx = 0; indx < idx; indx++) {
		dst = (__u64 *)(pbuf->pbuf + 2 * (indx + offset));
		src = (__u64 *)pbuf->wqe[indx];
		iowrite64(dst, *src);

		dst++;
		src++;
		iowrite64(dst, *src);
	}
}

static inline int bnxt_re_dp_spin_init(struct bnxt_spinlock *lock, int pshared, int need_lock)
{
	lock->in_use = 0;
	lock->need_lock = need_lock;
	return pthread_spin_init(&lock->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline int bnxt_re_dp_spin_destroy(struct bnxt_spinlock *lock)
{
	return pthread_spin_destroy(&lock->lock);
}

static inline int bnxt_spin_lock(struct bnxt_spinlock *lock)
{
	if (lock->need_lock)
		return pthread_spin_lock(&lock->lock);

	if (unlikely(lock->in_use)) {
		fprintf(stderr, "*** ERROR: multithreading violation ***\n"
			"You are running a multithreaded application but\n"
			"you set BNXT_SINGLE_THREADED=1. Please unset it.\n");
		abort();
	} else {
		lock->in_use = 1;
		 /* This fence is not at all correct, but it increases the */
		 /* chance that in_use is detected by another thread without */
		 /* much runtime cost. */
		atomic_thread_fence(memory_order_acq_rel);
	}

	return 0;
}

static inline int bnxt_spin_unlock(struct bnxt_spinlock *lock)
{
	if (lock->need_lock)
		return pthread_spin_unlock(&lock->lock);

	lock->in_use = 0;
	return 0;
}

static void timespec_sub(const struct timespec *a, const struct timespec *b,
			 struct timespec *res)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (res->tv_nsec < 0) {
		res->tv_sec--;
		res->tv_nsec += BNXT_NSEC_PER_SEC;
	}
}

/*
 * Function waits in a busy loop for a given nano seconds
 * The maximum wait period allowed is less than one second
 */
static inline void bnxt_re_sub_sec_busy_wait(uint32_t nsec)
{
	struct timespec start, cur, res;

	if (nsec >= BNXT_NSEC_PER_SEC)
		return;

	if (clock_gettime(CLOCK_REALTIME, &start)) {
		fprintf(stderr, "%s: failed to get time : %d",
			__func__, errno);
		return;
	}

	while (1) {
		if (clock_gettime(CLOCK_REALTIME, &cur)) {
			fprintf(stderr, "%s: failed to get time : %d",
				__func__, errno);
			return;
		}

		timespec_sub(&cur, &start, &res);
		if (res.tv_nsec >= nsec)
			break;
	}
}

#define BNXT_RE_HW_RETX(a) ((a)->comp_mask & BNXT_RE_COMP_MASK_UCNTX_HW_RETX_ENABLED)
#define bnxt_re_dp_spin_lock(lock)     bnxt_spin_lock(lock)
#define bnxt_re_dp_spin_unlock(lock)   bnxt_spin_unlock(lock)

#endif
