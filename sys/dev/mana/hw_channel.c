/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>

#include "mana.h"
#include "hw_channel.h"

static int
mana_hwc_get_msg_index(struct hw_channel_context *hwc, uint16_t *msg_id)
{
	struct gdma_resource *r = &hwc->inflight_msg_res;
	uint32_t index;

	sema_wait(&hwc->sema);

	mtx_lock_spin(&r->lock_spin);

	index = find_first_zero_bit(hwc->inflight_msg_res.map,
	    hwc->inflight_msg_res.size);

	bitmap_set(hwc->inflight_msg_res.map, index, 1);

	mtx_unlock_spin(&r->lock_spin);

	*msg_id = index;

	return 0;
}

static void
mana_hwc_put_msg_index(struct hw_channel_context *hwc, uint16_t msg_id)
{
	struct gdma_resource *r = &hwc->inflight_msg_res;

	mtx_lock_spin(&r->lock_spin);
	bitmap_clear(hwc->inflight_msg_res.map, msg_id, 1);
	mtx_unlock_spin(&r->lock_spin);

	sema_post(&hwc->sema);
}

static int
mana_hwc_verify_resp_msg(const struct hwc_caller_ctx *caller_ctx,
    const struct gdma_resp_hdr *resp_msg,
    uint32_t resp_len)
{
	if (resp_len < sizeof(*resp_msg))
		return EPROTO;

	if (resp_len > caller_ctx->output_buflen)
		return EPROTO;

	return 0;
}

static void
mana_hwc_handle_resp(struct hw_channel_context *hwc, uint32_t resp_len,
    const struct gdma_resp_hdr *resp_msg)
{
	struct hwc_caller_ctx *ctx;
	int err;

	if (!test_bit(resp_msg->response.hwc_msg_id,
	    hwc->inflight_msg_res.map)) {
		device_printf(hwc->dev, "hwc_rx: invalid msg_id = %u\n",
		    resp_msg->response.hwc_msg_id);
		return;
	}

	ctx = hwc->caller_ctx + resp_msg->response.hwc_msg_id;
	err = mana_hwc_verify_resp_msg(ctx, resp_msg, resp_len);
	if (err)
		goto out;

	ctx->status_code = resp_msg->status;

	memcpy(ctx->output_buf, resp_msg, resp_len);
out:
	ctx->error = err;
	complete(&ctx->comp_event);
}

static int
mana_hwc_post_rx_wqe(const struct hwc_wq *hwc_rxq,
    struct hwc_work_request *req)
{
	device_t dev = hwc_rxq->hwc->dev;
	struct gdma_sge *sge;
	int err;

	sge = &req->sge;
	sge->address = (uintptr_t)req->buf_sge_addr;
	sge->mem_key = hwc_rxq->msg_buf->gpa_mkey;
	sge->size = req->buf_len;

	memset(&req->wqe_req, 0, sizeof(struct gdma_wqe_request));
	req->wqe_req.sgl = sge;
	req->wqe_req.num_sge = 1;
	req->wqe_req.client_data_unit = 0;

	err = mana_gd_post_and_ring(hwc_rxq->gdma_wq, &req->wqe_req, NULL);
	if (err)
		device_printf(dev,
		    "Failed to post WQE on HWC RQ: %d\n", err);
	return err;
}

static void
mana_hwc_init_event_handler(void *ctx, struct gdma_queue *q_self,
    struct gdma_event *event)
{
	struct hw_channel_context *hwc = ctx;
	struct gdma_dev *gd = hwc->gdma_dev;
	union hwc_init_type_data type_data;
	union hwc_init_eq_id_db eq_db;
	uint32_t type, val;

	switch (event->type) {
	case GDMA_EQE_HWC_INIT_EQ_ID_DB:
		eq_db.as_uint32 = event->details[0];
		hwc->cq->gdma_eq->id = eq_db.eq_id;
		gd->doorbell = eq_db.doorbell;
		break;

	case GDMA_EQE_HWC_INIT_DATA:
		type_data.as_uint32 = event->details[0];
		type = type_data.type;
		val = type_data.value;

		switch (type) {
		case HWC_INIT_DATA_CQID:
			hwc->cq->gdma_cq->id = val;
			break;

		case HWC_INIT_DATA_RQID:
			hwc->rxq->gdma_wq->id = val;
			break;

		case HWC_INIT_DATA_SQID:
			hwc->txq->gdma_wq->id = val;
			break;

		case HWC_INIT_DATA_QUEUE_DEPTH:
			hwc->hwc_init_q_depth_max = (uint16_t)val;
			break;

		case HWC_INIT_DATA_MAX_REQUEST:
			hwc->hwc_init_max_req_msg_size = val;
			break;

		case HWC_INIT_DATA_MAX_RESPONSE:
			hwc->hwc_init_max_resp_msg_size = val;
			break;

		case HWC_INIT_DATA_MAX_NUM_CQS:
			gd->gdma_context->max_num_cqs = val;
			break;

		case HWC_INIT_DATA_PDID:
			hwc->gdma_dev->pdid = val;
			break;

		case HWC_INIT_DATA_GPA_MKEY:
			hwc->rxq->msg_buf->gpa_mkey = val;
			hwc->txq->msg_buf->gpa_mkey = val;
			break;
		}

		break;

	case GDMA_EQE_HWC_INIT_DONE:
		complete(&hwc->hwc_init_eqe_comp);
		break;

	default:
		/* Ignore unknown events, which should never happen. */
		break;
	}
}

static void
mana_hwc_rx_event_handler(void *ctx, uint32_t gdma_rxq_id,
    const struct hwc_rx_oob *rx_oob)
{
	struct hw_channel_context *hwc = ctx;
	struct hwc_wq *hwc_rxq = hwc->rxq;
	struct hwc_work_request *rx_req;
	struct gdma_resp_hdr *resp;
	struct gdma_wqe *dma_oob;
	struct gdma_queue *rq;
	struct gdma_sge *sge;
	uint64_t rq_base_addr;
	uint64_t rx_req_idx;
	uint8_t *wqe;

	if (hwc_rxq->gdma_wq->id != gdma_rxq_id) {
		mana_warn(NULL, "unmatched rx queue %u != %u\n",
		    hwc_rxq->gdma_wq->id, gdma_rxq_id);
		return;
	}


	rq = hwc_rxq->gdma_wq;
	wqe = mana_gd_get_wqe_ptr(rq, rx_oob->wqe_offset / GDMA_WQE_BU_SIZE);
	dma_oob = (struct gdma_wqe *)wqe;

	bus_dmamap_sync(rq->mem_info.dma_tag, rq->mem_info.dma_map,
	    BUS_DMASYNC_POSTREAD);

	sge = (struct gdma_sge *)(wqe + 8 + dma_oob->inline_oob_size_div4 * 4);

	/* Select the RX work request for virtual address and for reposting. */
	rq_base_addr = hwc_rxq->msg_buf->mem_info.dma_handle;
	rx_req_idx = (sge->address - rq_base_addr) / hwc->max_req_msg_size;

	bus_dmamap_sync(hwc_rxq->msg_buf->mem_info.dma_tag,
	    hwc_rxq->msg_buf->mem_info.dma_map,
	    BUS_DMASYNC_POSTREAD);

	rx_req = &hwc_rxq->msg_buf->reqs[rx_req_idx];
	resp = (struct gdma_resp_hdr *)rx_req->buf_va;

	if (resp->response.hwc_msg_id >= hwc->num_inflight_msg) {
		device_printf(hwc->dev, "HWC RX: wrong msg_id=%u\n",
		    resp->response.hwc_msg_id);
		return;
	}

	mana_hwc_handle_resp(hwc, rx_oob->tx_oob_data_size, resp);

	/* Do no longer use 'resp', because the buffer is posted to the HW
	 * in the below mana_hwc_post_rx_wqe().
	 */
	resp = NULL;

	bus_dmamap_sync(hwc_rxq->msg_buf->mem_info.dma_tag,
	    hwc_rxq->msg_buf->mem_info.dma_map,
	    BUS_DMASYNC_PREREAD);

	mana_hwc_post_rx_wqe(hwc_rxq, rx_req);
}

static void
mana_hwc_tx_event_handler(void *ctx, uint32_t gdma_txq_id,
    const struct hwc_rx_oob *rx_oob)
{
	struct hw_channel_context *hwc = ctx;
	struct hwc_wq *hwc_txq = hwc->txq;

	if (!hwc_txq || hwc_txq->gdma_wq->id != gdma_txq_id) {
		mana_warn(NULL, "unmatched tx queue %u != %u\n",
		    hwc_txq->gdma_wq->id, gdma_txq_id);
	}

	bus_dmamap_sync(hwc_txq->gdma_wq->mem_info.dma_tag,
	    hwc_txq->gdma_wq->mem_info.dma_map,
	    BUS_DMASYNC_POSTWRITE);
}

static int
mana_hwc_create_gdma_wq(struct hw_channel_context *hwc,
    enum gdma_queue_type type, uint64_t queue_size,
    struct gdma_queue **queue)
{
	struct gdma_queue_spec spec = {};

	if (type != GDMA_SQ && type != GDMA_RQ)
		return EINVAL;

	spec.type = type;
	spec.monitor_avl_buf = false;
	spec.queue_size = queue_size;

	return mana_gd_create_hwc_queue(hwc->gdma_dev, &spec, queue);
}

static int
mana_hwc_create_gdma_cq(struct hw_channel_context *hwc,
    uint64_t queue_size,
    void *ctx, gdma_cq_callback *cb,
    struct gdma_queue *parent_eq,
    struct gdma_queue **queue)
{
	struct gdma_queue_spec spec = {};

	spec.type = GDMA_CQ;
	spec.monitor_avl_buf = false;
	spec.queue_size = queue_size;
	spec.cq.context = ctx;
	spec.cq.callback = cb;
	spec.cq.parent_eq = parent_eq;

	return mana_gd_create_hwc_queue(hwc->gdma_dev, &spec, queue);
}

static int
mana_hwc_create_gdma_eq(struct hw_channel_context *hwc,
    uint64_t queue_size,
    void *ctx, gdma_eq_callback *cb,
    struct gdma_queue **queue)
{
	struct gdma_queue_spec spec = {};

	spec.type = GDMA_EQ;
	spec.monitor_avl_buf = false;
	spec.queue_size = queue_size;
	spec.eq.context = ctx;
	spec.eq.callback = cb;
	spec.eq.log2_throttle_limit = DEFAULT_LOG2_THROTTLING_FOR_ERROR_EQ;

	return mana_gd_create_hwc_queue(hwc->gdma_dev, &spec, queue);
}

static void
mana_hwc_comp_event(void *ctx, struct gdma_queue *q_self)
{
	struct hwc_rx_oob comp_data = {};
	struct gdma_comp *completions;
	struct hwc_cq *hwc_cq = ctx;
	int comp_read, i;

	completions = hwc_cq->comp_buf;
	comp_read = mana_gd_poll_cq(q_self, completions, hwc_cq->queue_depth);

	for (i = 0; i < comp_read; ++i) {
		comp_data = *(struct hwc_rx_oob *)completions[i].cqe_data;

		if (completions[i].is_sq)
			hwc_cq->tx_event_handler(hwc_cq->tx_event_ctx,
			    completions[i].wq_num,
			    &comp_data);
		else
			hwc_cq->rx_event_handler(hwc_cq->rx_event_ctx,
			    completions[i].wq_num,
			    &comp_data);
	}

	bus_dmamap_sync(q_self->mem_info.dma_tag, q_self->mem_info.dma_map,
	    BUS_DMASYNC_POSTREAD);

	mana_gd_ring_cq(q_self, SET_ARM_BIT);
}

static void
mana_hwc_destroy_cq(struct gdma_context *gc, struct hwc_cq *hwc_cq)
{
	if (hwc_cq->comp_buf)
		free(hwc_cq->comp_buf, M_DEVBUF);

	if (hwc_cq->gdma_cq)
		mana_gd_destroy_queue(gc, hwc_cq->gdma_cq);

	if (hwc_cq->gdma_eq)
		mana_gd_destroy_queue(gc, hwc_cq->gdma_eq);

	free(hwc_cq, M_DEVBUF);
}

static int
mana_hwc_create_cq(struct hw_channel_context *hwc,
    uint16_t q_depth,
    gdma_eq_callback *callback, void *ctx,
    hwc_rx_event_handler_t *rx_ev_hdlr, void *rx_ev_ctx,
    hwc_tx_event_handler_t *tx_ev_hdlr, void *tx_ev_ctx,
    struct hwc_cq **hwc_cq_ptr)
{
	struct gdma_queue *eq, *cq;
	struct gdma_comp *comp_buf;
	struct hwc_cq *hwc_cq;
	uint32_t eq_size, cq_size;
	int err;

	eq_size = roundup_pow_of_two(GDMA_EQE_SIZE * q_depth);
	if (eq_size < MINIMUM_SUPPORTED_PAGE_SIZE)
		eq_size = MINIMUM_SUPPORTED_PAGE_SIZE;

	cq_size = roundup_pow_of_two(GDMA_CQE_SIZE * q_depth);
	if (cq_size < MINIMUM_SUPPORTED_PAGE_SIZE)
		cq_size = MINIMUM_SUPPORTED_PAGE_SIZE;

	hwc_cq = malloc(sizeof(*hwc_cq), M_DEVBUF, M_WAITOK | M_ZERO);

	err = mana_hwc_create_gdma_eq(hwc, eq_size, ctx, callback, &eq);
	if (err) {
		device_printf(hwc->dev,
		    "Failed to create HWC EQ for RQ: %d\n", err);
		goto out;
	}
	hwc_cq->gdma_eq = eq;

	err = mana_hwc_create_gdma_cq(hwc, cq_size, hwc_cq,
	    mana_hwc_comp_event, eq, &cq);
	if (err) {
		device_printf(hwc->dev,
		    "Failed to create HWC CQ for RQ: %d\n", err);
		goto out;
	}
	hwc_cq->gdma_cq = cq;

	comp_buf = mallocarray(q_depth, sizeof(struct gdma_comp),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	hwc_cq->hwc = hwc;
	hwc_cq->comp_buf = comp_buf;
	hwc_cq->queue_depth = q_depth;
	hwc_cq->rx_event_handler = rx_ev_hdlr;
	hwc_cq->rx_event_ctx = rx_ev_ctx;
	hwc_cq->tx_event_handler = tx_ev_hdlr;
	hwc_cq->tx_event_ctx = tx_ev_ctx;

	*hwc_cq_ptr = hwc_cq;
	return 0;
out:
	mana_hwc_destroy_cq(hwc->gdma_dev->gdma_context, hwc_cq);
	return err;
}

static int
mana_hwc_alloc_dma_buf(struct hw_channel_context *hwc, uint16_t q_depth,
    uint32_t max_msg_size,
    struct hwc_dma_buf **dma_buf_ptr)
{
	struct gdma_context *gc = hwc->gdma_dev->gdma_context;
	struct hwc_work_request *hwc_wr;
	struct hwc_dma_buf *dma_buf;
	struct gdma_mem_info *gmi;
	uint32_t buf_size;
	uint8_t *base_pa;
	void *virt_addr;
	uint16_t i;
	int err;

	dma_buf = malloc(sizeof(*dma_buf) +
	    q_depth * sizeof(struct hwc_work_request),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	dma_buf->num_reqs = q_depth;

	buf_size = ALIGN(q_depth * max_msg_size, PAGE_SIZE);

	gmi = &dma_buf->mem_info;
	err = mana_gd_alloc_memory(gc, buf_size, gmi);
	if (err) {
		device_printf(hwc->dev,
		    "Failed to allocate DMA buffer: %d\n", err);
		goto out;
	}

	virt_addr = dma_buf->mem_info.virt_addr;
	base_pa = (uint8_t *)dma_buf->mem_info.dma_handle;

	for (i = 0; i < q_depth; i++) {
		hwc_wr = &dma_buf->reqs[i];

		hwc_wr->buf_va = (char *)virt_addr + i * max_msg_size;
		hwc_wr->buf_sge_addr = base_pa + i * max_msg_size;

		hwc_wr->buf_len = max_msg_size;
	}

	*dma_buf_ptr = dma_buf;
	return 0;
out:
	free(dma_buf, M_DEVBUF);
	return err;
}

static void
mana_hwc_dealloc_dma_buf(struct hw_channel_context *hwc,
    struct hwc_dma_buf *dma_buf)
{
	if (!dma_buf)
		return;

	mana_gd_free_memory(&dma_buf->mem_info);

	free(dma_buf, M_DEVBUF);
}

static void
mana_hwc_destroy_wq(struct hw_channel_context *hwc,
    struct hwc_wq *hwc_wq)
{
	mana_hwc_dealloc_dma_buf(hwc, hwc_wq->msg_buf);

	if (hwc_wq->gdma_wq)
		mana_gd_destroy_queue(hwc->gdma_dev->gdma_context,
		    hwc_wq->gdma_wq);

	free(hwc_wq, M_DEVBUF);
}

static int
mana_hwc_create_wq(struct hw_channel_context *hwc,
    enum gdma_queue_type q_type, uint16_t q_depth,
    uint32_t max_msg_size, struct hwc_cq *hwc_cq,
    struct hwc_wq **hwc_wq_ptr)
{
	struct gdma_queue *queue;
	struct hwc_wq *hwc_wq;
	uint32_t queue_size;
	int err;

	if (q_type != GDMA_SQ && q_type != GDMA_RQ) {
		/* XXX should fail and return error? */
		mana_warn(NULL, "Invalid q_type %u\n", q_type);
	}

	if (q_type == GDMA_RQ)
		queue_size = roundup_pow_of_two(GDMA_MAX_RQE_SIZE * q_depth);
	else
		queue_size = roundup_pow_of_two(GDMA_MAX_SQE_SIZE * q_depth);

	if (queue_size < MINIMUM_SUPPORTED_PAGE_SIZE)
		queue_size = MINIMUM_SUPPORTED_PAGE_SIZE;

	hwc_wq = malloc(sizeof(*hwc_wq), M_DEVBUF, M_WAITOK | M_ZERO);

	err = mana_hwc_create_gdma_wq(hwc, q_type, queue_size, &queue);
	if (err)
		goto out;

	hwc_wq->hwc = hwc;
	hwc_wq->gdma_wq = queue;
	hwc_wq->queue_depth = q_depth;
	hwc_wq->hwc_cq = hwc_cq;

	err = mana_hwc_alloc_dma_buf(hwc, q_depth, max_msg_size,
	    &hwc_wq->msg_buf);
	if (err)
		goto out;

	*hwc_wq_ptr = hwc_wq;
	return 0;
out:
	if (err)
		mana_hwc_destroy_wq(hwc, hwc_wq);
	return err;
}

static int
mana_hwc_post_tx_wqe(const struct hwc_wq *hwc_txq,
    struct hwc_work_request *req,
    uint32_t dest_virt_rq_id, uint32_t dest_virt_rcq_id,
    bool dest_pf)
{
	device_t dev = hwc_txq->hwc->dev;
	struct hwc_tx_oob *tx_oob;
	struct gdma_sge *sge;
	int err;

	if (req->msg_size == 0 || req->msg_size > req->buf_len) {
		device_printf(dev, "wrong msg_size: %u, buf_len: %u\n",
		    req->msg_size, req->buf_len);
		return EINVAL;
	}

	tx_oob = &req->tx_oob;

	tx_oob->vrq_id = dest_virt_rq_id;
	tx_oob->dest_vfid = 0;
	tx_oob->vrcq_id = dest_virt_rcq_id;
	tx_oob->vscq_id = hwc_txq->hwc_cq->gdma_cq->id;
	tx_oob->loopback = false;
	tx_oob->lso_override = false;
	tx_oob->dest_pf = dest_pf;
	tx_oob->vsq_id = hwc_txq->gdma_wq->id;

	sge = &req->sge;
	sge->address = (uintptr_t)req->buf_sge_addr;
	sge->mem_key = hwc_txq->msg_buf->gpa_mkey;
	sge->size = req->msg_size;

	memset(&req->wqe_req, 0, sizeof(struct gdma_wqe_request));
	req->wqe_req.sgl = sge;
	req->wqe_req.num_sge = 1;
	req->wqe_req.inline_oob_size = sizeof(struct hwc_tx_oob);
	req->wqe_req.inline_oob_data = tx_oob;
	req->wqe_req.client_data_unit = 0;

	err = mana_gd_post_and_ring(hwc_txq->gdma_wq, &req->wqe_req, NULL);
	if (err)
		device_printf(dev,
		    "Failed to post WQE on HWC SQ: %d\n", err);
	return err;
}

static int
mana_hwc_init_inflight_msg(struct hw_channel_context *hwc, uint16_t num_msg)
{
	int err;

	sema_init(&hwc->sema, num_msg, "gdma hwc sema");

	err = mana_gd_alloc_res_map(num_msg, &hwc->inflight_msg_res,
	    "gdma hwc res lock");
	if (err)
		device_printf(hwc->dev,
		    "Failed to init inflight_msg_res: %d\n", err);

	return (err);
}

static int
mana_hwc_test_channel(struct hw_channel_context *hwc, uint16_t q_depth,
    uint32_t max_req_msg_size, uint32_t max_resp_msg_size)
{
	struct gdma_context *gc = hwc->gdma_dev->gdma_context;
	struct hwc_wq *hwc_rxq = hwc->rxq;
	struct hwc_work_request *req;
	struct hwc_caller_ctx *ctx;
	int err;
	int i;

	/* Post all WQEs on the RQ */
	for (i = 0; i < q_depth; i++) {
		req = &hwc_rxq->msg_buf->reqs[i];
		err = mana_hwc_post_rx_wqe(hwc_rxq, req);
		if (err)
			return err;
	}

	ctx = malloc(q_depth * sizeof(struct hwc_caller_ctx),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	for (i = 0; i < q_depth; ++i)
		init_completion(&ctx[i].comp_event);

	hwc->caller_ctx = ctx;

	return mana_gd_test_eq(gc, hwc->cq->gdma_eq);
}

static int
mana_hwc_establish_channel(struct gdma_context *gc, uint16_t *q_depth,
    uint32_t *max_req_msg_size,
    uint32_t *max_resp_msg_size)
{
	struct hw_channel_context *hwc = gc->hwc.driver_data;
	struct gdma_queue *rq = hwc->rxq->gdma_wq;
	struct gdma_queue *sq = hwc->txq->gdma_wq;
	struct gdma_queue *eq = hwc->cq->gdma_eq;
	struct gdma_queue *cq = hwc->cq->gdma_cq;
	int err;

	init_completion(&hwc->hwc_init_eqe_comp);

	err = mana_smc_setup_hwc(&gc->shm_channel, false,
	    eq->mem_info.dma_handle,
	    cq->mem_info.dma_handle,
	    rq->mem_info.dma_handle,
	    sq->mem_info.dma_handle,
	    eq->eq.msix_index);
	if (err)
		return err;

	if (wait_for_completion_timeout(&hwc->hwc_init_eqe_comp, 60 * hz))
		return ETIMEDOUT;

	*q_depth = hwc->hwc_init_q_depth_max;
	*max_req_msg_size = hwc->hwc_init_max_req_msg_size;
	*max_resp_msg_size = hwc->hwc_init_max_resp_msg_size;

	/* Both were set in mana_hwc_init_event_handler(). */
	if (cq->id >= gc->max_num_cqs) {
		mana_warn(NULL, "invalid cq id %u > %u\n",
		    cq->id, gc->max_num_cqs);
		return EPROTO;
	}

	gc->cq_table = malloc(gc->max_num_cqs * sizeof(struct gdma_queue *),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	gc->cq_table[cq->id] = cq;

	return 0;
}

static int
mana_hwc_init_queues(struct hw_channel_context *hwc, uint16_t q_depth,
    uint32_t max_req_msg_size, uint32_t max_resp_msg_size)
{
	int err;

	err = mana_hwc_init_inflight_msg(hwc, q_depth);
	if (err)
		return err;

	/* CQ is shared by SQ and RQ, so CQ's queue depth is the sum of SQ
	 * queue depth and RQ queue depth.
	 */
	err = mana_hwc_create_cq(hwc, q_depth * 2,
	    mana_hwc_init_event_handler, hwc,
	    mana_hwc_rx_event_handler, hwc,
	    mana_hwc_tx_event_handler, hwc, &hwc->cq);
	if (err) {
		device_printf(hwc->dev, "Failed to create HWC CQ: %d\n", err);
		goto out;
	}

	err = mana_hwc_create_wq(hwc, GDMA_RQ, q_depth, max_req_msg_size,
	    hwc->cq, &hwc->rxq);
	if (err) {
		device_printf(hwc->dev, "Failed to create HWC RQ: %d\n", err);
		goto out;
	}

	err = mana_hwc_create_wq(hwc, GDMA_SQ, q_depth, max_resp_msg_size,
	    hwc->cq, &hwc->txq);
	if (err) {
		device_printf(hwc->dev, "Failed to create HWC SQ: %d\n", err);
		goto out;
	}

	hwc->num_inflight_msg = q_depth;
	hwc->max_req_msg_size = max_req_msg_size;

	return 0;
out:
	/* mana_hwc_create_channel() will do the cleanup.*/
	return err;
}

int
mana_hwc_create_channel(struct gdma_context *gc)
{
	uint32_t max_req_msg_size, max_resp_msg_size;
	struct gdma_dev *gd = &gc->hwc;
	struct hw_channel_context *hwc;
	uint16_t q_depth_max;
	int err;

	hwc = malloc(sizeof(*hwc), M_DEVBUF, M_WAITOK | M_ZERO);

	gd->gdma_context = gc;
	gd->driver_data = hwc;
	hwc->gdma_dev = gd;
	hwc->dev = gc->dev;

	/* HWC's instance number is always 0. */
	gd->dev_id.as_uint32 = 0;
	gd->dev_id.type = GDMA_DEVICE_HWC;

	gd->pdid = INVALID_PDID;
	gd->doorbell = INVALID_DOORBELL;

	/*
	 * mana_hwc_init_queues() only creates the required data structures,
	 * and doesn't touch the HWC device.
	 */
	err = mana_hwc_init_queues(hwc, HW_CHANNEL_VF_BOOTSTRAP_QUEUE_DEPTH,
	    HW_CHANNEL_MAX_REQUEST_SIZE,
	    HW_CHANNEL_MAX_RESPONSE_SIZE);
	if (err) {
		device_printf(hwc->dev, "Failed to initialize HWC: %d\n",
		    err);
		goto out;
	}

	err = mana_hwc_establish_channel(gc, &q_depth_max, &max_req_msg_size,
	    &max_resp_msg_size);
	if (err) {
		device_printf(hwc->dev, "Failed to establish HWC: %d\n", err);
		goto out;
	}

	err = mana_hwc_test_channel(gc->hwc.driver_data,
	    HW_CHANNEL_VF_BOOTSTRAP_QUEUE_DEPTH,
	    max_req_msg_size, max_resp_msg_size);
	if (err) {
		/* Test failed, but the channel has been established */
		device_printf(hwc->dev, "Failed to test HWC: %d\n", err);
		return EIO;
	}

	return 0;
out:
	mana_hwc_destroy_channel(gc);
	return (err);
}

void
mana_hwc_destroy_channel(struct gdma_context *gc)
{
	struct hw_channel_context *hwc = gc->hwc.driver_data;

	if (!hwc)
		return;

	/*
	 * gc->max_num_cqs is set in mana_hwc_init_event_handler(). If it's
	 * non-zero, the HWC worked and we should tear down the HWC here.
	 */
	if (gc->max_num_cqs > 0) {
		mana_smc_teardown_hwc(&gc->shm_channel, false);
		gc->max_num_cqs = 0;
	}

	free(hwc->caller_ctx, M_DEVBUF);
	hwc->caller_ctx = NULL;

	if (hwc->txq)
		mana_hwc_destroy_wq(hwc, hwc->txq);

	if (hwc->rxq)
		mana_hwc_destroy_wq(hwc, hwc->rxq);

	if (hwc->cq)
		mana_hwc_destroy_cq(hwc->gdma_dev->gdma_context, hwc->cq);

	mana_gd_free_res_map(&hwc->inflight_msg_res);

	hwc->num_inflight_msg = 0;

	hwc->gdma_dev->doorbell = INVALID_DOORBELL;
	hwc->gdma_dev->pdid = INVALID_PDID;

	free(hwc, M_DEVBUF);
	gc->hwc.driver_data = NULL;
	gc->hwc.gdma_context = NULL;

	free(gc->cq_table, M_DEVBUF);
	gc->cq_table = NULL;
}

int
mana_hwc_send_request(struct hw_channel_context *hwc, uint32_t req_len,
    const void *req, uint32_t resp_len, void *resp)
{
	struct hwc_work_request *tx_wr;
	struct hwc_wq *txq = hwc->txq;
	struct gdma_req_hdr *req_msg;
	struct hwc_caller_ctx *ctx;
	uint16_t msg_id;
	int err;

	mana_hwc_get_msg_index(hwc, &msg_id);

	tx_wr = &txq->msg_buf->reqs[msg_id];

	if (req_len > tx_wr->buf_len) {
		device_printf(hwc->dev,
		    "HWC: req msg size: %d > %d\n", req_len,
		    tx_wr->buf_len);
		err = EINVAL;
		goto out;
	}

	ctx = hwc->caller_ctx + msg_id;
	ctx->output_buf = resp;
	ctx->output_buflen = resp_len;

	req_msg = (struct gdma_req_hdr *)tx_wr->buf_va;
	if (req)
		memcpy(req_msg, req, req_len);

	req_msg->req.hwc_msg_id = msg_id;

	tx_wr->msg_size = req_len;

	err = mana_hwc_post_tx_wqe(txq, tx_wr, 0, 0, false);
	if (err) {
		device_printf(hwc->dev,
		    "HWC: Failed to post send WQE: %d\n", err);
		goto out;
	}

	if (wait_for_completion_timeout(&ctx->comp_event, 30 * hz)) {
		device_printf(hwc->dev, "HWC: Request timed out!\n");
		err = ETIMEDOUT;
		goto out;
	}

	if (ctx->error) {
		err = ctx->error;
		goto out;
	}

	if (ctx->status_code && ctx->status_code != GDMA_STATUS_MORE_ENTRIES) {
		device_printf(hwc->dev,
		    "HWC: Failed hw_channel req: 0x%x\n", ctx->status_code);
		err = EPROTO;
		goto out;
	}
out:
	mana_hwc_put_msg_index(hwc, msg_id);
	return err;
}
