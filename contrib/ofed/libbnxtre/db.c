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
 * Description: Doorbell handling functions.
 */

#include <malloc.h>
#include <unistd.h>

#include "abi.h"
#include "main.h"

#define BNXT_RE_DB_FIFO_ROOM_MASK_P5	0x1FFF8000
#define BNXT_RE_MAX_FIFO_DEPTH_P5	0x2c00

#define BNXT_RE_DB_FIFO_ROOM_MASK_P7	0x3FFF8000
#define BNXT_RE_MAX_FIFO_DEPTH_P7	0x8000

#define BNXT_RE_DB_FIFO_ROOM_SHIFT      15
#define BNXT_RE_DB_THRESHOLD		20

#define BNXT_RE_DB_FIFO_ROOM_MASK(ctx)	\
	(_is_chip_thor2((ctx)) ? \
	 BNXT_RE_DB_FIFO_ROOM_MASK_P7 :\
	 BNXT_RE_DB_FIFO_ROOM_MASK_P5)
#define BNXT_RE_MAX_FIFO_DEPTH(ctx)	\
	(_is_chip_thor2((ctx)) ? \
	 BNXT_RE_MAX_FIFO_DEPTH_P7 :\
	 BNXT_RE_MAX_FIFO_DEPTH_P5)

static uint32_t xorshift32(struct xorshift32_state *state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = state->seed;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return state->seed = x;
}

static uint16_t rnd(struct xorshift32_state *state, uint16_t range)
{
	/* range must be a power of 2 - 1 */
	return (xorshift32(state) & range);
}

static int calculate_fifo_occupancy(struct bnxt_re_context *cntx)
{
	uint32_t *dbr_map = cntx->bar_map + 0x1a8;
	uint32_t read_val, fifo_occup;

	read_val = *dbr_map;
	fifo_occup = BNXT_RE_MAX_FIFO_DEPTH(cntx->cctx) -
		((read_val & BNXT_RE_DB_FIFO_ROOM_MASK(cntx->cctx)) >>
		 BNXT_RE_DB_FIFO_ROOM_SHIFT);

	return fifo_occup;
}

static inline uint32_t find_min(uint32_t x, uint32_t y)
{
	return (y > x ? x : y);
}

int bnxt_re_do_pacing(struct bnxt_re_context *cntx, struct xorshift32_state *state)
{
	/* First 4 bytes  of shared page (pacing_info) contains the DBR
	 * pacing information. Second 4 bytes (pacing_th)  contains
	 * the pacing threshold value to determine whether to
	 * add delay or not
	 */
	struct bnxt_re_pacing_data *pacing_data =
		(struct bnxt_re_pacing_data *)cntx->dbr_page;
	uint32_t wait_time = 1;
	uint32_t fifo_occup;

	if (!pacing_data)
		return 0;
	/* If the device in error recovery state, return error to
	 * not to ring new doorbells in this state.
	 */
	if (pacing_data->dev_err_state)
		return -EFAULT;

	if (rnd(state, BNXT_RE_MAX_DO_PACING) < pacing_data->do_pacing) {
		while ((fifo_occup = calculate_fifo_occupancy(cntx))
			>  pacing_data->pacing_th) {
			struct bnxt_re_cq *cq;
			uint32_t usec_wait;

			if (pacing_data->alarm_th && fifo_occup > pacing_data->alarm_th) {
				cq = container_of(cntx->dbr_cq, struct bnxt_re_cq, ibvcq);
				bnxt_re_poll_kernel_cq(cq);
			}
			usec_wait = rnd(state, wait_time - 1);
			if (usec_wait)
				bnxt_re_sub_sec_busy_wait(usec_wait * 1000);
			/* wait time capped at 128 us */
			wait_time = find_min(wait_time * 2, 128);
		}
	}
	return 0;
}

static inline void bnxt_re_ring_db(struct bnxt_re_dpi *dpi, __u64 key,
				   uint64_t *db_key, uint8_t *lock)
{
	while (1) {
		if (__sync_bool_compare_and_swap(lock, 0, 1)) {
			*db_key = key;
			bnxt_re_wm_barrier();
			iowrite64(dpi->dbpage, key);
			bnxt_re_wm_barrier();
			*lock = 0;
			break;
		}
	}
}

static inline void bnxt_re_init_push_hdr(struct bnxt_re_db_hdr *hdr,
					 uint32_t indx, uint32_t qid,
					 uint32_t typ, uint32_t pidx)
{
	__u64 key_lo, key_hi;

	key_lo = (((pidx & BNXT_RE_DB_PILO_MASK) << BNXT_RE_DB_PILO_SHIFT) |
		  (indx & BNXT_RE_DB_INDX_MASK));
	key_hi = ((((pidx & BNXT_RE_DB_PIHI_MASK) << BNXT_RE_DB_PIHI_SHIFT) |
		   (qid & BNXT_RE_DB_QID_MASK)) |
		  ((typ & BNXT_RE_DB_TYP_MASK) << BNXT_RE_DB_TYP_SHIFT) |
		  (0x1UL << BNXT_RE_DB_VALID_SHIFT));
	hdr->typ_qid_indx = htole64((key_lo | (key_hi << 32)));
}

static inline void bnxt_re_init_db_hdr(struct bnxt_re_db_hdr *hdr,
				       uint32_t indx, uint32_t toggle,
				       uint32_t qid, uint32_t typ)
{
	__u64 key_lo, key_hi;

	key_lo = htole32(indx | toggle);
	key_hi = ((qid & BNXT_RE_DB_QID_MASK) |
		  ((typ & BNXT_RE_DB_TYP_MASK) << BNXT_RE_DB_TYP_SHIFT) |
		  (0x1UL << BNXT_RE_DB_VALID_SHIFT));
	hdr->typ_qid_indx = htole64((key_lo | (key_hi << 32)));
}

static inline void __bnxt_re_ring_pend_db(__u64 *ucdb, __u64 key,
					  struct  bnxt_re_qp *qp)
{
	struct bnxt_re_db_hdr hdr;

	bnxt_re_init_db_hdr(&hdr,
			    (*qp->jsqq->hwque->dbtail |
			     ((qp->jsqq->hwque->flags &
			       BNXT_RE_FLAG_EPOCH_TAIL_MASK) <<
			    BNXT_RE_DB_EPOCH_TAIL_SHIFT)), 0,
			    qp->qpid,
			    BNXT_RE_QUE_TYPE_SQ);

	while (1) {
		if (__sync_bool_compare_and_swap(&qp->sq_dbr_lock, 0, 1)) {
			qp->sq_shadow_db_key = hdr.typ_qid_indx;
			bnxt_re_wm_barrier();
			iowrite64(ucdb, key);
			bnxt_re_wm_barrier();
			qp->sq_dbr_lock = 0;
			break;
		}
	}
}

void bnxt_re_ring_rq_db(struct bnxt_re_qp *qp)
{
	struct bnxt_re_db_hdr hdr;

	if (bnxt_re_do_pacing(qp->cntx, &qp->rand))
		return;
	bnxt_re_init_db_hdr(&hdr,
			    (*qp->jrqq->hwque->dbtail |
			     ((qp->jrqq->hwque->flags &
			       BNXT_RE_FLAG_EPOCH_TAIL_MASK) <<
			      BNXT_RE_DB_EPOCH_TAIL_SHIFT)), 0,
			    qp->qpid,
			    BNXT_RE_QUE_TYPE_RQ);
	bnxt_re_ring_db(qp->udpi, hdr.typ_qid_indx, &qp->rq_shadow_db_key,
			&qp->rq_dbr_lock);
}

void bnxt_re_ring_sq_db(struct bnxt_re_qp *qp)
{
	struct bnxt_re_db_hdr hdr;

	if (bnxt_re_do_pacing(qp->cntx, &qp->rand))
		return;
	bnxt_re_init_db_hdr(&hdr,
			    (*qp->jsqq->hwque->dbtail |
			     ((qp->jsqq->hwque->flags &
			       BNXT_RE_FLAG_EPOCH_TAIL_MASK) <<
			     BNXT_RE_DB_EPOCH_TAIL_SHIFT)), 0,
			    qp->qpid,
			    BNXT_RE_QUE_TYPE_SQ);
	bnxt_re_ring_db(qp->udpi, hdr.typ_qid_indx, &qp->sq_shadow_db_key,
			&qp->sq_dbr_lock);
}

void bnxt_re_ring_srq_db(struct bnxt_re_srq *srq)
{
	struct bnxt_re_db_hdr hdr;

	if (bnxt_re_do_pacing(srq->uctx, &srq->rand))
		return;
	bnxt_re_init_db_hdr(&hdr,
			    (srq->srqq->tail |
			     ((srq->srqq->flags &
			       BNXT_RE_FLAG_EPOCH_TAIL_MASK) <<
			     BNXT_RE_DB_EPOCH_TAIL_SHIFT)), 0,
			    srq->srqid, BNXT_RE_QUE_TYPE_SRQ);
	bnxt_re_ring_db(srq->udpi, hdr.typ_qid_indx, &srq->shadow_db_key,
			&srq->dbr_lock);
}

void bnxt_re_ring_srq_arm(struct bnxt_re_srq *srq)
{
	struct bnxt_re_db_hdr hdr;

	if (bnxt_re_do_pacing(srq->uctx, &srq->rand))
		return;
	bnxt_re_init_db_hdr(&hdr, srq->cap.srq_limit, 0, srq->srqid,
			    BNXT_RE_QUE_TYPE_SRQ_ARM);
	bnxt_re_ring_db(srq->udpi, hdr.typ_qid_indx, &srq->shadow_db_key,
			&srq->dbr_lock);
}

void bnxt_re_ring_cq_db(struct bnxt_re_cq *cq)
{
	struct bnxt_re_db_hdr hdr;

	if (bnxt_re_do_pacing(cq->cntx, &cq->rand))
		return;
	bnxt_re_init_db_hdr(&hdr,
			    (cq->cqq->head |
			     ((cq->cqq->flags &
			       BNXT_RE_FLAG_EPOCH_HEAD_MASK) <<
			     BNXT_RE_DB_EPOCH_HEAD_SHIFT)), 0,
			    cq->cqid,
			    BNXT_RE_QUE_TYPE_CQ);
	bnxt_re_ring_db(cq->udpi, hdr.typ_qid_indx, &cq->shadow_db_key,
			&cq->dbr_lock);
}

void bnxt_re_ring_cq_arm_db(struct bnxt_re_cq *cq, uint8_t aflag)
{
	uint32_t *cq_page = cq->cq_page;
	struct bnxt_re_db_hdr hdr;
	uint32_t toggle = 0;

	if (cq_page)
		toggle = *cq_page;

	if (bnxt_re_do_pacing(cq->cntx, &cq->rand))
		return;
	bnxt_re_init_db_hdr(&hdr,
			    (cq->cqq->head |
			     ((cq->cqq->flags &
			       BNXT_RE_FLAG_EPOCH_HEAD_MASK) <<
			     BNXT_RE_DB_EPOCH_HEAD_SHIFT)),
			     toggle << BNXT_RE_DB_TOGGLE_SHIFT,
			    cq->cqid, aflag);
	bnxt_re_ring_db(cq->udpi, hdr.typ_qid_indx, &cq->shadow_db_key,
			&cq->dbr_lock);
}

void bnxt_re_ring_pstart_db(struct bnxt_re_qp *qp,
			    struct bnxt_re_push_buffer *pbuf)
{
	__u64 key;

	if (bnxt_re_do_pacing(qp->cntx, &qp->rand))
		return;
	key = ((((pbuf->wcdpi & BNXT_RE_DB_PIHI_MASK) <<
		  BNXT_RE_DB_PIHI_SHIFT) | (pbuf->qpid & BNXT_RE_DB_QID_MASK)) |
	       ((BNXT_RE_PUSH_TYPE_START & BNXT_RE_DB_TYP_MASK) <<
		 BNXT_RE_DB_TYP_SHIFT) | (0x1UL << BNXT_RE_DB_VALID_SHIFT));
	key <<= 32;
	key |= ((((__u32)pbuf->wcdpi & BNXT_RE_DB_PILO_MASK) <<
		  BNXT_RE_DB_PILO_SHIFT) | (pbuf->st_idx &
					    BNXT_RE_DB_INDX_MASK));
	bnxt_re_wm_barrier();
	iowrite64(pbuf->ucdb, key);
}

void bnxt_re_ring_pend_db(struct bnxt_re_qp *qp,
			  struct bnxt_re_push_buffer *pbuf)
{
	__u64 key;

	if (bnxt_re_do_pacing(qp->cntx, &qp->rand))
		return;
	key = ((((pbuf->wcdpi & BNXT_RE_DB_PIHI_MASK) <<
		  BNXT_RE_DB_PIHI_SHIFT) | (pbuf->qpid & BNXT_RE_DB_QID_MASK)) |
	       ((BNXT_RE_PUSH_TYPE_END & BNXT_RE_DB_TYP_MASK) <<
		 BNXT_RE_DB_TYP_SHIFT) | (0x1UL << BNXT_RE_DB_VALID_SHIFT));
	key <<= 32;
	key |= ((((__u32)pbuf->wcdpi & BNXT_RE_DB_PILO_MASK) <<
		  BNXT_RE_DB_PILO_SHIFT) | (pbuf->tail &
					    BNXT_RE_DB_INDX_MASK));
	__bnxt_re_ring_pend_db(pbuf->ucdb, key, qp);
}

void bnxt_re_fill_ppp(struct bnxt_re_push_buffer *pbuf,
		      struct bnxt_re_qp *qp, uint8_t len, uint32_t idx)
{
	struct bnxt_re_db_ppp_hdr phdr = {};
	__u64 *dst, *src;
	__u8 plen;
	int indx;

	src = (__u64 *)&phdr;
	plen = len + sizeof(phdr) + bnxt_re_get_sqe_hdr_sz();

	bnxt_re_init_db_hdr(&phdr.db_hdr,
			    (*qp->jsqq->hwque->dbtail |
			     ((qp->jsqq->hwque->flags &
			       BNXT_RE_FLAG_EPOCH_TAIL_MASK) <<
			     BNXT_RE_DB_EPOCH_TAIL_SHIFT)), 0,
			    qp->qpid,
			    BNXT_RE_QUE_TYPE_SQ);

	phdr.rsv_psz_pidx = ((pbuf->st_idx & BNXT_RE_DB_INDX_MASK) |
			     (((plen % 8 ? (plen / 8) + 1 :
				plen / 8) & BNXT_RE_PUSH_SIZE_MASK) <<
			       BNXT_RE_PUSH_SIZE_SHIFT));

	bnxt_re_wm_barrier();
	for (indx = 0; indx < 2; indx++) {
		dst = (__u64 *)(pbuf->pbuf + indx);
		iowrite64(dst, *src);
		src++;
	}
	bnxt_re_copy_data_to_pb(pbuf, 1, idx);
	mmio_flush_writes();
}

void bnxt_re_fill_push_wcb(struct bnxt_re_qp *qp,
			   struct bnxt_re_push_buffer *pbuf, uint32_t idx)
{
	bnxt_re_ring_pstart_db(qp, pbuf);
	mmio_wc_start();
	bnxt_re_copy_data_to_pb(pbuf, 0, idx);
	/* Flush WQE write before push end db. */
	mmio_flush_writes();
	bnxt_re_ring_pend_db(qp, pbuf);
}

int bnxt_re_init_pbuf_list(struct bnxt_re_context *ucntx)
{
	struct bnxt_re_push_buffer *pbuf;
	int indx, wqesz;
	int size, offt;
	__u64 wcpage;
	__u64 dbpage;
	void *base;

	size = (sizeof(*ucntx->pbrec) +
		16 * (sizeof(*ucntx->pbrec->pbuf) +
		      sizeof(struct bnxt_re_push_wqe)));
	ucntx->pbrec = calloc(1, size);
	if (!ucntx->pbrec)
		goto out;

	offt = sizeof(*ucntx->pbrec);
	base = ucntx->pbrec;
	ucntx->pbrec->pbuf = (base + offt);
	ucntx->pbrec->pbmap = ~0x00;
	ucntx->pbrec->pbmap &= ~0x7fff; /* 15 bits */
	ucntx->pbrec->udpi = &ucntx->udpi;

	wqesz = sizeof(struct bnxt_re_push_wqe);
	wcpage = (__u64)ucntx->udpi.wcdbpg;
	dbpage = (__u64)ucntx->udpi.dbpage;
	offt = sizeof(*ucntx->pbrec->pbuf) * 16;
	base = (char *)ucntx->pbrec->pbuf + offt;
	for (indx = 0; indx < 16; indx++) {
		pbuf = &ucntx->pbrec->pbuf[indx];
		pbuf->wqe = base + indx * wqesz;
		pbuf->pbuf = (__u64 *)(wcpage + indx * wqesz);
		pbuf->ucdb = (__u64 *)(dbpage + (indx + 1) * sizeof(__u64));
		pbuf->wcdpi = ucntx->udpi.wcdpi;
	}

	return 0;
out:
	return -ENOMEM;
}

struct bnxt_re_push_buffer *bnxt_re_get_pbuf(uint8_t *push_st_en,
					     uint8_t ppp_idx,
					     struct bnxt_re_context *cntx)
{
	struct bnxt_re_push_buffer *pbuf = NULL;
	uint8_t buf_state = 0;
	__u32 old;
	int bit;

	if (_is_chip_thor2(cntx->cctx)) {
		buf_state = !!(*push_st_en & BNXT_RE_PPP_STATE_MASK);
		pbuf = &cntx->pbrec->pbuf[(ppp_idx * 2) + buf_state];
		/* Flip */
		*push_st_en ^= 1UL << BNXT_RE_PPP_ST_SHIFT;
	} else {
		old = cntx->pbrec->pbmap;
		while ((bit = __builtin_ffs(~cntx->pbrec->pbmap)) != 0) {
			if (__sync_bool_compare_and_swap
						(&cntx->pbrec->pbmap,
						 old,
						 (old | 0x01 << (bit - 1))))
				break;
			old = cntx->pbrec->pbmap;
		}

		if (bit) {
			pbuf = &cntx->pbrec->pbuf[bit];
			pbuf->nbit = bit;
		}
	}

	return pbuf;
}

void bnxt_re_put_pbuf(struct bnxt_re_context *cntx,
		      struct bnxt_re_push_buffer *pbuf)
{
	struct bnxt_re_push_rec *pbrec;
	__u32 old;
	int bit;

	if (_is_chip_thor2(cntx->cctx))
		return;

	pbrec = cntx->pbrec;

	if (pbuf->nbit) {
		bit = pbuf->nbit;
		pbuf->nbit = 0;
		old = pbrec->pbmap;
		while (!__sync_bool_compare_and_swap(&pbrec->pbmap, old,
						     (old & (~(0x01 <<
							       (bit - 1))))))
			old = pbrec->pbmap;
	}
}

void bnxt_re_destroy_pbuf_list(struct bnxt_re_context *cntx)
{
	free(cntx->pbrec);
}

void bnxt_re_replay_db(struct bnxt_re_context *cntx,
		       struct xorshift32_state *state, struct bnxt_re_dpi *dpi,
		       uint64_t *shadow_key, uint8_t *dbr_lock)
{
	if (bnxt_re_do_pacing(cntx, state))
		return;
	cntx->replay_cnt++;
	if (cntx->replay_cnt % BNXT_RE_DB_REPLAY_YIELD_CNT == 0)
		pthread_yield();
	if (__sync_bool_compare_and_swap(dbr_lock, 0, 1)) {
		bnxt_re_wm_barrier();
		if (*shadow_key == BNXT_RE_DB_KEY_INVALID) {
			*dbr_lock = 0;
			return;
		}
		iowrite64(dpi->dbpage, *shadow_key);
		bnxt_re_wm_barrier();
		*dbr_lock = 0;
	}
}

void bnxt_re_db_recovery(struct bnxt_re_context *cntx)
{
	struct bnxt_re_list_node *cur, *tmp;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;
	struct bnxt_re_srq *srq;

	pthread_spin_lock(&cntx->qp_dbr_res.lock);
	list_for_each_node_safe(cur, tmp, &cntx->qp_dbr_res.head) {
		qp = list_node(cur, struct bnxt_re_qp, dbnode);
		bnxt_re_replay_db(cntx, &qp->rand, qp->udpi,
				  &qp->sq_shadow_db_key, &qp->sq_dbr_lock);
		bnxt_re_replay_db(cntx, &qp->rand, qp->udpi,
				  &qp->rq_shadow_db_key, &qp->rq_dbr_lock);
	}
	pthread_spin_unlock(&cntx->qp_dbr_res.lock);
	pthread_spin_lock(&cntx->cq_dbr_res.lock);
	list_for_each_node_safe(cur, tmp, &cntx->cq_dbr_res.head) {
		cq = list_node(cur, struct bnxt_re_cq, dbnode);
		bnxt_re_replay_db(cntx, &cq->rand, cq->udpi,
				  &cq->shadow_db_key, &cq->dbr_lock);
	}
	pthread_spin_unlock(&cntx->cq_dbr_res.lock);
	pthread_spin_lock(&cntx->srq_dbr_res.lock);
	list_for_each_node_safe(cur, tmp, &cntx->srq_dbr_res.head) {
		srq = list_node(cur, struct bnxt_re_srq, dbnode);
		bnxt_re_replay_db(cntx, &srq->rand, srq->udpi,
				  &srq->shadow_db_key, &srq->dbr_lock);
	}
	pthread_spin_unlock(&cntx->srq_dbr_res.lock);
}

void *bnxt_re_dbr_thread(void *arg)
{
	uint32_t *epoch, *epoch_ack, usr_epoch;
	struct bnxt_re_context *cntx = arg;
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret;

	while (1) {
		ret = ibv_get_cq_event(cntx->dbr_ev_chan, &ev_cq, &ev_ctx);
		if (ret) {
			fprintf(stderr, "Failed to get cq_event\n");
			pthread_exit(NULL);
		}
		epoch = cntx->db_recovery_page;
		epoch_ack = epoch + 1;
		if (!epoch || !epoch_ack) {
			fprintf(stderr, "DB reovery page is NULL\n");
			pthread_exit(NULL);
		}
		if (*epoch == *epoch_ack) {
			ibv_ack_cq_events(ev_cq, 1);
			continue;
		}
		usr_epoch = *epoch;
		bnxt_re_db_recovery(cntx);
		*epoch_ack = usr_epoch;
		ibv_ack_cq_events(ev_cq, 1);
	}
}
