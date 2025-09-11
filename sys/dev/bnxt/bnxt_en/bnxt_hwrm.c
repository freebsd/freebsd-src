/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/endian.h>
#include <linux/pci.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "hsi_struct_def.h"

static int bnxt_hwrm_err_map(uint16_t err);
static inline int _is_valid_ether_addr(uint8_t *);
static inline void get_random_ether_addr(uint8_t *);
static void	bnxt_hwrm_set_link_common(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);
static void	bnxt_hwrm_set_pause_common(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);
static void	bnxt_hwrm_set_eee(struct bnxt_softc *softc,
		    struct hwrm_port_phy_cfg_input *req);

/* NVRam stuff has a five minute timeout */
#define BNXT_NVM_TIMEO	(5 * 60 * 1000)

#define BNXT_RX_STATS_PRI_ENTRY(counter, n)		\
	BNXT_RX_STATS_EXT_OFFSET(counter##_cos0)

#define BNXT_TX_STATS_PRI_ENTRY(counter, n)		\
	 BNXT_TX_STATS_EXT_OFFSET(counter##_cos0)

#define BNXT_RX_STATS_PRI_ENTRIES(counter)		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 0),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 1),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 2),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 3),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 4),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 5),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 6),		\
	BNXT_RX_STATS_PRI_ENTRY(counter, 7)

#define BNXT_TX_STATS_PRI_ENTRIES(counter)		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 0),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 1),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 2),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 3),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 4),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 5),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 6),		\
	BNXT_TX_STATS_PRI_ENTRY(counter, 7)


long bnxt_rx_bytes_pri_arr_base_off[] = {BNXT_RX_STATS_PRI_ENTRIES(rx_bytes)};
long bnxt_rx_pkts_pri_arr_base_off[] = {BNXT_RX_STATS_PRI_ENTRIES(rx_packets)};
long bnxt_tx_bytes_pri_arr_base_off[] = {BNXT_TX_STATS_PRI_ENTRIES(tx_bytes)};
long bnxt_tx_pkts_pri_arr_base_off[] = {BNXT_TX_STATS_PRI_ENTRIES(tx_packets)};

static int
bnxt_hwrm_err_map(uint16_t err)
{
	int rc;

	switch (err) {
	case HWRM_ERR_CODE_SUCCESS:
		return 0;
	case HWRM_ERR_CODE_INVALID_PARAMS:
	case HWRM_ERR_CODE_INVALID_FLAGS:
	case HWRM_ERR_CODE_INVALID_ENABLES:
		return EINVAL;
	case HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED:
		return EACCES;
	case HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR:
		return ENOMEM;
	case HWRM_ERR_CODE_CMD_NOT_SUPPORTED:
		return ENOSYS;
	case HWRM_ERR_CODE_FAIL:
		return EIO;
	case HWRM_ERR_CODE_HWRM_ERROR:
	case HWRM_ERR_CODE_UNKNOWN_ERR:
	default:
		return EDOOFUS;
	}

	return rc;
}

int
bnxt_alloc_hwrm_dma_mem(struct bnxt_softc *softc)
{
	int rc;

	rc = iflib_dma_alloc(softc->ctx, PAGE_SIZE, &softc->hwrm_cmd_resp,
	    BUS_DMA_NOWAIT);
	return rc;
}

void
bnxt_free_hwrm_dma_mem(struct bnxt_softc *softc)
{
	if (softc->hwrm_cmd_resp.idi_vaddr)
		iflib_dma_free(&softc->hwrm_cmd_resp);
	softc->hwrm_cmd_resp.idi_vaddr = NULL;
	return;
}

void
bnxt_hwrm_cmd_hdr_init(struct bnxt_softc *softc, void *request,
    uint16_t req_type)
{
	struct input *req = request;

	req->req_type = htole16(req_type);
	req->cmpl_ring = 0xffff;
	req->target_id = 0xffff;
	req->resp_addr = htole64(softc->hwrm_cmd_resp.idi_paddr);
}

int
_hwrm_send_message(struct bnxt_softc *softc, void *msg, uint32_t msg_len)
{
	struct input *req = msg;
	struct hwrm_err_output *resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	uint32_t *data = msg;
	int i;
	uint8_t *valid;
	uint16_t err;
	uint16_t max_req_len = BNXT_HWRM_MAX_REQ_LEN;
	struct hwrm_short_input short_input = {0};

	/* TODO: DMASYNC in here. */
	req->seq_id = htole16(softc->hwrm_cmd_seq++);
	memset(resp, 0, PAGE_SIZE);

	if (BNXT_NO_FW_ACCESS(softc) &&
	    (req->req_type != HWRM_FUNC_RESET && req->req_type != HWRM_VER_GET))
		return -EINVAL;

	if ((softc->flags & BNXT_FLAG_SHORT_CMD) ||
	    msg_len > BNXT_HWRM_MAX_REQ_LEN) {
		void *short_cmd_req = softc->hwrm_short_cmd_req_addr.idi_vaddr;
                uint16_t max_msg_len;

                /* Set boundary for maximum extended request length for short
                 * cmd format. If passed up from device use the max supported
                 * internal req length.
		 */

		max_msg_len = softc->hwrm_max_ext_req_len;


		memcpy(short_cmd_req, req, msg_len);
                if (msg_len < max_msg_len)
			memset((uint8_t *) short_cmd_req + msg_len, 0,
				max_msg_len - msg_len);

		short_input.req_type = req->req_type;
		short_input.signature =
		    htole16(HWRM_SHORT_INPUT_SIGNATURE_SHORT_CMD);
		short_input.size = htole16(msg_len);
		short_input.req_addr =
		    htole64(softc->hwrm_short_cmd_req_addr.idi_paddr);

		data = (uint32_t *)&short_input;
		msg_len = sizeof(short_input);

		/* Sync memory write before updating doorbell */
		wmb();

		max_req_len = BNXT_HWRM_SHORT_REQ_LEN;
	}

	/* Write request msg to hwrm channel */
	for (i = 0; i < msg_len; i += 4) {
		bus_space_write_4(softc->hwrm_bar.tag,
				  softc->hwrm_bar.handle,
				  i, *data);
		data++;
	}

	/* Clear to the end of the request buffer */
	for (i = msg_len; i < max_req_len; i += 4)
		bus_space_write_4(softc->hwrm_bar.tag, softc->hwrm_bar.handle,
		    i, 0);

	/* Ring channel doorbell */
	bus_space_write_4(softc->hwrm_bar.tag,
			  softc->hwrm_bar.handle,
			  0x100, htole32(1));

	/* Check if response len is updated */
	for (i = 0; i < softc->hwrm_cmd_timeo; i++) {
		if (resp->resp_len && resp->resp_len <= 4096)
			break;
		DELAY(1000);
	}
	if (i >= softc->hwrm_cmd_timeo) {
		device_printf(softc->dev,
		    "Timeout sending %s: (timeout: %u) seq: %d\n",
		    GET_HWRM_REQ_TYPE(req->req_type), softc->hwrm_cmd_timeo,
		    le16toh(req->seq_id));
		return ETIMEDOUT;
	}
	/* Last byte of resp contains the valid key */
	valid = (uint8_t *)resp + resp->resp_len - 1;
	for (i = 0; i < softc->hwrm_cmd_timeo; i++) {
		if (*valid == HWRM_RESP_VALID_KEY)
			break;
		DELAY(1000);
	}
	if (i >= softc->hwrm_cmd_timeo) {
		device_printf(softc->dev, "Timeout sending %s: "
		    "(timeout: %u) msg {0x%x 0x%x} len:%d v: %d\n",
		    GET_HWRM_REQ_TYPE(req->req_type),
		    softc->hwrm_cmd_timeo, le16toh(req->req_type),
		    le16toh(req->seq_id), msg_len,
		    *valid);
		return ETIMEDOUT;
	}

	err = le16toh(resp->error_code);
	if (err) {
		/* HWRM_ERR_CODE_FAIL is a "normal" error, don't log */
		if (err != HWRM_ERR_CODE_FAIL) {
			device_printf(softc->dev,
			    "%s command returned %s error.\n",
			    GET_HWRM_REQ_TYPE(req->req_type),
			    GET_HWRM_ERROR_CODE(err));
		}
		return bnxt_hwrm_err_map(err);
	}

	return 0;
}

int
hwrm_send_message(struct bnxt_softc *softc, void *msg, uint32_t msg_len)
{
	int rc;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, msg, msg_len);
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_queue_qportcfg(struct bnxt_softc *softc, uint32_t path_dir)
{
	int rc = 0;
	struct hwrm_queue_qportcfg_input req = {0};
	struct hwrm_queue_qportcfg_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	uint8_t max_tc, max_lltc, *max_q;
	uint8_t queue_profile, queue_id;
	struct bnxt_queue_info *q_info;
	uint8_t i, j, *qptr, *q_ids;
	bool no_rdma;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_QPORTCFG);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto qportcfg_exit;

	if (!resp->max_configurable_queues) {
		rc = -EINVAL;
		goto qportcfg_exit;
	}

	if (resp->queue_cfg_info & HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_CFG_INFO_ASYM_CFG) {
		softc->is_asym_q = true;
		/* bnxt_init_cosq_names(softc, path_dir); */
	} else {
		softc->is_asym_q = false;
		/* bnxt_free_stats_cosqnames_mem(softc); */
	}

	max_tc = min_t(uint8_t, resp->max_configurable_queues, BNXT_MAX_QUEUE);
	max_lltc = resp->max_configurable_lossless_queues;

	/*
	 * No RDMA support yet.
	 * no_rdma = !(softc->flags & BNXT_FLAG_ROCE_CAP);
	 */
	no_rdma = true;
	qptr = &resp->queue_id0;

	if (path_dir == HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX) {
		q_info = softc->tx_q_info;
		q_ids = softc->tx_q_ids;
		max_q = &softc->tx_max_q;
	} else {
		q_info = softc->rx_q_info;
		q_ids = softc->rx_q_ids;
		max_q = &softc->rx_max_q;
	}

	for (i = 0, j = 0; i < max_tc; i++) {
		queue_id = *qptr;
		qptr++;

		queue_profile = *qptr;
		qptr++;

		q_info[j].queue_id = queue_id;
		q_info[j].queue_profile = queue_profile;
		q_ids[i] = queue_id;

		softc->tc_to_qidx[j] = j;

		if (!BNXT_CNPQ(q_info[j].queue_profile) ||
		    (no_rdma && BNXT_PF(softc)))
			j++;
	}
	*max_q = max_tc;
	max_tc = max_t(uint8_t, j, 1);
	softc->max_tc = softc->max_tc ? min(softc->max_tc, max_tc) : max_tc;
	softc->max_lltc = softc->max_lltc ? min(softc->max_lltc, max_lltc) : max_lltc;

	if (softc->max_lltc > softc->max_tc)
		softc->max_lltc = softc->max_tc;

qportcfg_exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

static int bnxt_alloc_all_ctx_pg_info(struct bnxt_softc *softc, int ctx_max)
{
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	u16 type;

	for (type = 0; type < ctx_max; type++) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		int n = 1;

		if (!ctxm->max_entries || ctxm->pg_info)
			continue;

		if (ctxm->instance_bmap)
			n = hweight32(ctxm->instance_bmap);
		ctxm->pg_info = kcalloc(n, sizeof(*ctxm->pg_info), GFP_ATOMIC);
		if (!ctxm->pg_info)
			return -ENOMEM;
	}
	return 0;
}

static void bnxt_init_ctx_initializer(struct bnxt_ctx_mem_type *ctxm,
				      u8 init_val, u8 init_offset,
				      bool init_mask_set)
{
	ctxm->init_value = init_val;
	ctxm->init_offset = BNXT_CTX_INIT_INVALID_OFFSET;
	if (init_mask_set)
		ctxm->init_offset = init_offset * 4;
	else
		ctxm->init_value = 0;
}

#define BNXT_CTX_INIT_VALID(flags)      \
        (!!((flags) &                   \
            HWRM_FUNC_BACKING_STORE_QCAPS_V2_OUTPUT_FLAGS_ENABLE_CTX_KIND_INIT))

static int
bnxt_hwrm_func_backing_store_qcaps_v2(struct bnxt_softc *softc)
{
	struct hwrm_func_backing_store_qcaps_v2_input req = {0};
	struct hwrm_func_backing_store_qcaps_v2_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct bnxt_ctx_mem_info *ctx = NULL;
	u16 type;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_BACKING_STORE_QCAPS_V2);

	ctx = malloc(sizeof(*ctx), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!ctx)
		return -ENOMEM;

	softc->ctx_mem = ctx;

	BNXT_HWRM_LOCK(softc);
	for (type = 0; type < BNXT_CTX_V2_MAX; ) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		u8 init_val, init_off, i;
		__le32 *p;
		u32 flags;

		req.type = cpu_to_le16(type);
		rc = _hwrm_send_message(softc, &req, sizeof(req));
		if (rc)
			goto ctx_done;
		flags = le32_to_cpu(resp->flags);
		type = le16_to_cpu(resp->next_valid_type);
		if (!(flags & HWRM_FUNC_BACKING_STORE_QCAPS_V2_OUTPUT_FLAGS_TYPE_VALID))
			continue;

		ctxm->type = le16_to_cpu(resp->type);
		ctxm->flags = flags;

		ctxm->entry_size = le16_to_cpu(resp->entry_size);
		ctxm->instance_bmap = le32_to_cpu(resp->instance_bit_map);
		ctxm->entry_multiple = resp->entry_multiple;
		ctxm->max_entries = le32_to_cpu(resp->max_num_entries);
		ctxm->min_entries = le32_to_cpu(resp->min_num_entries);
		init_val = resp->ctx_init_value;
		init_off = resp->ctx_init_offset;
		bnxt_init_ctx_initializer(ctxm, init_val, init_off,
					  BNXT_CTX_INIT_VALID(flags));
		ctxm->split_entry_cnt = min_t(u8, resp->subtype_valid_cnt,
					      BNXT_MAX_SPLIT_ENTRY);
		for (i = 0, p = &resp->split_entry_0; i < ctxm->split_entry_cnt;
		     i++, p++)
			ctxm->split[i] = le32_to_cpu(*p);
	}
	rc = bnxt_alloc_all_ctx_pg_info(softc, BNXT_CTX_V2_MAX);

ctx_done:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int bnxt_hwrm_func_backing_store_qcaps(struct bnxt_softc *softc)
{
	struct hwrm_func_backing_store_qcaps_input req = {0};
	struct hwrm_func_backing_store_qcaps_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	if (softc->hwrm_spec_code < 0x10902 || softc->ctx_mem)
		return 0;

	if (BNXT_CHIP_P7(softc)) {
		if (softc->fw_cap & BNXT_FW_CAP_BACKING_STORE_V2)
			return bnxt_hwrm_func_backing_store_qcaps_v2(softc);
	}

	if (BNXT_VF(softc))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_BACKING_STORE_QCAPS);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		struct bnxt_ctx_mem_type *ctxm;
		struct bnxt_ctx_mem_info *ctx;
		u8 init_val, init_idx = 0;
		u16 init_mask;

		ctx = softc->ctx_mem;
		if (!ctx) {
			ctx = malloc(sizeof(*ctx), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!ctx) {
				rc = -ENOMEM;
				goto ctx_err;
			}
			softc->ctx_mem = ctx;
		}
		init_val = resp->ctx_kind_initializer;
		init_mask = le16_to_cpu(resp->ctx_init_mask);

		ctxm = &ctx->ctx_arr[BNXT_CTX_QP];
		ctxm->max_entries = le32_to_cpu(resp->qp_max_entries);
		ctxm->qp_qp1_entries = le16_to_cpu(resp->qp_min_qp1_entries);
		ctxm->qp_l2_entries = le16_to_cpu(resp->qp_max_l2_entries);
		ctxm->entry_size = le16_to_cpu(resp->qp_entry_size);
		bnxt_init_ctx_initializer(ctxm, init_val, resp->qp_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_SRQ];
		ctxm->srq_l2_entries = le16_to_cpu(resp->srq_max_l2_entries);
		ctxm->max_entries = le32_to_cpu(resp->srq_max_entries);
		ctxm->entry_size = le16_to_cpu(resp->srq_entry_size);
		bnxt_init_ctx_initializer(ctxm, init_val, resp->srq_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_CQ];
		ctxm->cq_l2_entries = le16_to_cpu(resp->cq_max_l2_entries);
		ctxm->max_entries = le32_to_cpu(resp->cq_max_entries);
		ctxm->entry_size = le16_to_cpu(resp->cq_entry_size);
		bnxt_init_ctx_initializer(ctxm, init_val, resp->cq_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_VNIC];
		ctxm->vnic_entries = le32_to_cpu(resp->vnic_max_vnic_entries);
		ctxm->max_entries = ctxm->vnic_entries +
			le16_to_cpu(resp->vnic_max_ring_table_entries);
		ctxm->entry_size = le16_to_cpu(resp->vnic_entry_size);
		bnxt_init_ctx_initializer(ctxm, init_val,
					  resp->vnic_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_STAT];
		ctxm->max_entries = le32_to_cpu(resp->stat_max_entries);
		ctxm->entry_size = le16_to_cpu(resp->stat_entry_size);
		bnxt_init_ctx_initializer(ctxm, init_val,
					  resp->stat_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_STQM];
		ctxm->entry_size = le16_to_cpu(resp->tqm_entry_size);
		ctxm->min_entries = le32_to_cpu(resp->tqm_min_entries_per_ring);
		ctxm->max_entries = le32_to_cpu(resp->tqm_max_entries_per_ring);
		ctxm->entry_multiple = resp->tqm_entries_multiple;
		if (!ctxm->entry_multiple)
			ctxm->entry_multiple = 1;

		memcpy(&ctx->ctx_arr[BNXT_CTX_FTQM], ctxm, sizeof(*ctxm));

		ctxm = &ctx->ctx_arr[BNXT_CTX_MRAV];
		ctxm->max_entries = le32_to_cpu(resp->mrav_max_entries);
		ctxm->entry_size = le16_to_cpu(resp->mrav_entry_size);
		ctxm->mrav_num_entries_units =
			le16_to_cpu(resp->mrav_num_entries_units);
		bnxt_init_ctx_initializer(ctxm, init_val,
					  resp->mrav_init_offset,
					  (init_mask & (1 << init_idx++)) != 0);

		ctxm = &ctx->ctx_arr[BNXT_CTX_TIM];
		ctxm->entry_size = le16_to_cpu(resp->tim_entry_size);
		ctxm->max_entries = le32_to_cpu(resp->tim_max_entries);

		ctx->tqm_fp_rings_count = resp->tqm_fp_rings_count;
		if (!ctx->tqm_fp_rings_count)
			ctx->tqm_fp_rings_count = softc->tx_max_q;
		else if (ctx->tqm_fp_rings_count > BNXT_MAX_TQM_FP_LEGACY_RINGS)
			ctx->tqm_fp_rings_count = BNXT_MAX_TQM_FP_LEGACY_RINGS;
		if (ctx->tqm_fp_rings_count == BNXT_MAX_TQM_FP_LEGACY_RINGS &&
		    softc->hwrm_max_ext_req_len >= BNXT_BACKING_STORE_CFG_LEN) {
			ctx->tqm_fp_rings_count += resp->tqm_fp_rings_count_ext;
			if (ctx->tqm_fp_rings_count > BNXT_MAX_TQM_FP_RINGS)
				ctx->tqm_fp_rings_count = BNXT_MAX_TQM_FP_RINGS;
		}
		ctxm = &ctx->ctx_arr[BNXT_CTX_FTQM];
		memcpy(ctxm, &ctx->ctx_arr[BNXT_CTX_STQM], sizeof(*ctxm));
		ctxm->instance_bmap = (1 << ctx->tqm_fp_rings_count) - 1;

		rc = bnxt_alloc_all_ctx_pg_info(softc, BNXT_CTX_MAX);
	} else {
		rc = 0;
	}
ctx_err:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

#define HWRM_FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES                 \
        (HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP |                \
         HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ |               \
         HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ |                \
         HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC |              \
         HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_STAT)

static void bnxt_hwrm_set_pg_attr(struct bnxt_ring_mem_info *rmem, uint8_t *pg_attr,
				  uint64_t *pg_dir)
{
	if (!rmem->nr_pages)
		return;

	BNXT_SET_CTX_PAGE_ATTR(*pg_attr);

	if (rmem->depth >= 1) {
		if (rmem->depth == 2)
			*pg_attr |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_QPC_LVL_LVL_2;
		else
			*pg_attr |= HWRM_FUNC_BACKING_STORE_CFG_INPUT_QPC_LVL_LVL_1;
		*pg_dir = htole64(rmem->pg_tbl.idi_paddr);
	} else {
		*pg_dir = htole64(rmem->pg_arr[0].idi_paddr);
	}
}

int bnxt_hwrm_func_backing_store_cfg(struct bnxt_softc *softc, uint32_t enables)
{
	struct hwrm_func_backing_store_cfg_input req = {0};
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	struct bnxt_ctx_pg_info *ctx_pg;
	struct bnxt_ctx_mem_type *ctxm;
	u32 req_len = sizeof(req);
	__le32 *num_entries;
	u32 ena, flags = 0;
	__le64 *pg_dir;
	u8 *pg_attr;
	int i;

	if (!ctx)
		return 0;

	if (req_len > softc->hwrm_max_ext_req_len)
		req_len = BNXT_BACKING_STORE_CFG_LEGACY_LEN;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_BACKING_STORE_CFG);
	req.enables = htole32(enables);

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_QP];
		ctx_pg = ctxm->pg_info;
		req.qp_num_entries = cpu_to_le32(ctx_pg->entries);
		req.qp_num_qp1_entries = cpu_to_le16(ctxm->qp_qp1_entries);
		req.qp_num_l2_entries = cpu_to_le16(ctxm->qp_l2_entries);
		req.qp_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.qpc_pg_size_qpc_lvl,
				&req.qpc_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_SRQ];
		ctx_pg = ctxm->pg_info;
		req.srq_num_entries = cpu_to_le32(ctx_pg->entries);
		req.srq_num_l2_entries = cpu_to_le16(ctxm->srq_l2_entries);
		req.srq_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.srq_pg_size_srq_lvl,
				&req.srq_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_CQ];
		ctx_pg = ctxm->pg_info;
		req.cq_num_entries = cpu_to_le32(ctx_pg->entries);
		req.cq_num_l2_entries = cpu_to_le16(ctxm->cq_l2_entries);
		req.cq_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.cq_pg_size_cq_lvl,
				&req.cq_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_MRAV) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_MRAV];
		ctx_pg = ctxm->pg_info;
		req.mrav_num_entries = cpu_to_le32(ctx_pg->entries);
		if (ctxm->mrav_num_entries_units)
			flags |=
			HWRM_FUNC_BACKING_STORE_CFG_INPUT_FLAGS_MRAV_RESERVATION_SPLIT;
		req.mrav_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.mrav_pg_size_mrav_lvl,
				&req.mrav_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TIM) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_TIM];
		ctx_pg = ctxm->pg_info;
		req.tim_num_entries = cpu_to_le32(ctx_pg->entries);
		req.tim_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.tim_pg_size_tim_lvl,
				&req.tim_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_VNIC];
		ctx_pg = ctxm->pg_info;
		req.vnic_num_vnic_entries = cpu_to_le16(ctxm->vnic_entries);
		req.vnic_num_ring_table_entries =
			cpu_to_le16(ctxm->max_entries - ctxm->vnic_entries);
		req.vnic_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.vnic_pg_size_vnic_lvl,
				&req.vnic_page_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_STAT) {
		ctxm = &ctx->ctx_arr[BNXT_CTX_STAT];
		ctx_pg = ctxm->pg_info;
		req.stat_num_entries = cpu_to_le32(ctxm->max_entries);
		req.stat_entry_size = cpu_to_le16(ctxm->entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				&req.stat_pg_size_stat_lvl,
				&req.stat_page_dir);
	}
	ctxm = &ctx->ctx_arr[BNXT_CTX_STQM];
	for (i = 0, num_entries = &req.tqm_sp_num_entries,
			pg_attr = &req.tqm_sp_pg_size_tqm_sp_lvl,
			pg_dir = &req.tqm_sp_page_dir,
	     ena = HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_SP,
	     ctx_pg = ctxm->pg_info;
	     i < BNXT_MAX_TQM_LEGACY_RINGS;
	     ctx_pg = &ctx->ctx_arr[BNXT_CTX_FTQM].pg_info[i],
	     i++, num_entries++, pg_attr++, pg_dir++, ena <<= 1) {
		if (!(enables & ena))
			continue;

		req.tqm_entry_size = cpu_to_le16(ctxm->entry_size);
		*num_entries = cpu_to_le32(ctx_pg->entries);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem, pg_attr, pg_dir);
	}
	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_RING8) {
		pg_attr = &req.tqm_ring8_pg_size_tqm_ring_lvl;
		pg_dir = &req.tqm_ring8_page_dir;
		ctx_pg = &ctx->ctx_arr[BNXT_CTX_FTQM].pg_info[8];
		req.tqm_ring8_num_entries = cpu_to_le32(ctx_pg->entries);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem, pg_attr, pg_dir);
	}
	req.flags = cpu_to_le32(flags);
	return hwrm_send_message(softc, &req, req_len);
}

int bnxt_hwrm_func_resc_qcaps(struct bnxt_softc *softc, bool all)
{
	struct hwrm_func_resource_qcaps_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_func_resource_qcaps_input req = {0};
	struct bnxt_hw_resc *hw_resc = &softc->hw_resc;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_RESOURCE_QCAPS);
	req.fid = htole16(0xffff);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc) {
		rc = -EIO;
		goto hwrm_func_resc_qcaps_exit;
	}

	hw_resc->max_tx_sch_inputs = le16toh(resp->max_tx_scheduler_inputs);
	if (!all)
		goto hwrm_func_resc_qcaps_exit;

	hw_resc->min_rsscos_ctxs = le16toh(resp->min_rsscos_ctx);
	hw_resc->max_rsscos_ctxs = le16toh(resp->max_rsscos_ctx);
	hw_resc->min_cp_rings = le16toh(resp->min_cmpl_rings);
	hw_resc->max_cp_rings = le16toh(resp->max_cmpl_rings);
	hw_resc->min_tx_rings = le16toh(resp->min_tx_rings);
	hw_resc->max_tx_rings = le16toh(resp->max_tx_rings);
	hw_resc->min_rx_rings = le16toh(resp->min_rx_rings);
	hw_resc->max_rx_rings = le16toh(resp->max_rx_rings);
	hw_resc->min_hw_ring_grps = le16toh(resp->min_hw_ring_grps);
	hw_resc->max_hw_ring_grps = le16toh(resp->max_hw_ring_grps);
	hw_resc->min_l2_ctxs = le16toh(resp->min_l2_ctxs);
	hw_resc->max_l2_ctxs = le16toh(resp->max_l2_ctxs);
	hw_resc->min_vnics = le16toh(resp->min_vnics);
	hw_resc->max_vnics = le16toh(resp->max_vnics);
	hw_resc->min_stat_ctxs = le16toh(resp->min_stat_ctx);
	hw_resc->max_stat_ctxs = le16toh(resp->max_stat_ctx);

	if (BNXT_CHIP_P5_PLUS(softc)) {
		hw_resc->max_nqs = le16toh(resp->max_msix);
		hw_resc->max_hw_ring_grps = hw_resc->max_rx_rings;
	}

hwrm_func_resc_qcaps_exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int bnxt_hwrm_func_backing_store_cfg_v2(struct bnxt_softc *softc,
					struct bnxt_ctx_mem_type *ctxm,
					bool last)
{
	struct hwrm_func_backing_store_cfg_v2_input req = {0};
	u32 instance_bmap = ctxm->instance_bmap;
	int i, j, rc = 0, n = 1;
	__le32 *p;

	if (!(ctxm->flags & BNXT_CTX_MEM_TYPE_VALID) || !ctxm->pg_info)
		return 0;

	if (instance_bmap)
		n = hweight32(ctxm->instance_bmap);
	else
		instance_bmap = 1;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_BACKING_STORE_CFG_V2);

	BNXT_HWRM_LOCK(softc);
	req.type = cpu_to_le16(ctxm->type);
	req.entry_size = cpu_to_le16(ctxm->entry_size);
	req.subtype_valid_cnt = ctxm->split_entry_cnt;
	for (i = 0, p = &req.split_entry_0; i < ctxm->split_entry_cnt; i++)
		p[i] = cpu_to_le32(ctxm->split[i]);
	for (i = 0, j = 0; j < n && !rc; i++) {
		struct bnxt_ctx_pg_info *ctx_pg;

		if (!(instance_bmap & (1 << i)))
			continue;
		req.instance = cpu_to_le16(i);
		ctx_pg = &ctxm->pg_info[j++];
		if (!ctx_pg->entries)
			continue;
		req.num_entries = cpu_to_le32(ctx_pg->entries);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.page_size_pbl_level,
				      &req.page_dir);
		if (last && j == n)
			req.flags =
				cpu_to_le32(HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_FLAGS_BS_CFG_ALL_DONE);
		rc = _hwrm_send_message(softc, &req, sizeof(req));
	}
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_passthrough(struct bnxt_softc *softc, void *req, uint32_t req_len,
		void *resp, uint32_t resp_len, uint32_t app_timeout)
{
	int rc = 0;
	void *output = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct input *input = req;
	uint32_t old_timeo;

	input->resp_addr = htole64(softc->hwrm_cmd_resp.idi_paddr);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	if (input->req_type == HWRM_NVM_INSTALL_UPDATE)
		softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	else
		softc->hwrm_cmd_timeo = max(app_timeout, softc->hwrm_cmd_timeo);
	rc = _hwrm_send_message(softc, req, req_len);
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc) {
		device_printf(softc->dev, "%s: %s command failed with rc: 0x%x\n",
			      __FUNCTION__, GET_HWRM_REQ_TYPE(input->req_type), rc);
		goto fail;
	}

	memcpy(resp, output, resp_len);
fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}


int
bnxt_hwrm_ver_get(struct bnxt_softc *softc)
{
	struct hwrm_ver_get_input	req = {0};
	struct hwrm_ver_get_output	*resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int				rc;
	const char nastr[] = "<not installed>";
	const char naver[] = "<N/A>";
	uint32_t dev_caps_cfg;
	uint16_t fw_maj, fw_min, fw_bld, fw_rsv, len;

	softc->hwrm_max_req_len = HWRM_MAX_REQ_LEN;
	softc->hwrm_cmd_timeo = 1000;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VER_GET);

	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	snprintf(softc->ver_info->hwrm_if_ver, BNXT_VERSTR_SIZE, "%d.%d.%d",
	    resp->hwrm_intf_maj_8b, resp->hwrm_intf_min_8b, resp->hwrm_intf_upd_8b);
	softc->ver_info->hwrm_if_major = resp->hwrm_intf_maj_8b;
	softc->ver_info->hwrm_if_minor = resp->hwrm_intf_min_8b;
	softc->ver_info->hwrm_if_update = resp->hwrm_intf_upd_8b;
	strlcpy(softc->ver_info->driver_hwrm_if_ver, HWRM_VERSION_STR,
	    BNXT_VERSTR_SIZE);
	strlcpy(softc->ver_info->hwrm_fw_name, resp->hwrm_fw_name,
	    BNXT_NAME_SIZE);

	 softc->hwrm_spec_code = resp->hwrm_intf_maj_8b << 16 |
                             resp->hwrm_intf_min_8b << 8 |
                             resp->hwrm_intf_upd_8b;
	if (resp->hwrm_intf_maj_8b < 1) {
		 device_printf(softc->dev, "HWRM interface %d.%d.%d is older "
			       "than 1.0.0.\n", resp->hwrm_intf_maj_8b,
			       resp->hwrm_intf_min_8b, resp->hwrm_intf_upd_8b);
		 device_printf(softc->dev, "Please update firmware with HWRM "
				"interface 1.0.0 or newer.\n");
	 }
	if (resp->mgmt_fw_major == 0 && resp->mgmt_fw_minor == 0 &&
	    resp->mgmt_fw_build == 0) {
		strlcpy(softc->ver_info->mgmt_fw_ver, naver, BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->mgmt_fw_name, nastr, BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->mgmt_fw_ver, FW_VER_STR_LEN,
		    "%d.%d.%d.%d", resp->mgmt_fw_major, resp->mgmt_fw_minor,
		    resp->mgmt_fw_build, resp->mgmt_fw_patch);
		strlcpy(softc->ver_info->mgmt_fw_name, resp->mgmt_fw_name,
		    BNXT_NAME_SIZE);
	}
	if (resp->netctrl_fw_major == 0 && resp->netctrl_fw_minor == 0 &&
	    resp->netctrl_fw_build == 0) {
		strlcpy(softc->ver_info->netctrl_fw_ver, naver,
		    BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->netctrl_fw_name, nastr,
		    BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->netctrl_fw_ver, FW_VER_STR_LEN,
		    "%d.%d.%d.%d", resp->netctrl_fw_major, resp->netctrl_fw_minor,
		    resp->netctrl_fw_build, resp->netctrl_fw_patch);
		strlcpy(softc->ver_info->netctrl_fw_name, resp->netctrl_fw_name,
		    BNXT_NAME_SIZE);
	}
	if (resp->roce_fw_major == 0 && resp->roce_fw_minor == 0 &&
	    resp->roce_fw_build == 0) {
		strlcpy(softc->ver_info->roce_fw_ver, naver, BNXT_VERSTR_SIZE);
		strlcpy(softc->ver_info->roce_fw_name, nastr, BNXT_NAME_SIZE);
	}
	else {
		snprintf(softc->ver_info->roce_fw_ver, BNXT_VERSTR_SIZE,
		    "%d.%d.%d.%d", resp->roce_fw_major, resp->roce_fw_minor,
		    resp->roce_fw_build, resp->roce_fw_patch);
		strlcpy(softc->ver_info->roce_fw_name, resp->roce_fw_name,
		    BNXT_NAME_SIZE);
	}

	fw_maj = le32toh(resp->hwrm_fw_major);
	if (softc->hwrm_spec_code > 0x10803 && fw_maj) {
		fw_min = le16toh(resp->hwrm_fw_minor);
		fw_bld = le16toh(resp->hwrm_fw_build);
		fw_rsv = le16toh(resp->hwrm_fw_patch);
		len = FW_VER_STR_LEN;
	} else {
		fw_maj = resp->hwrm_fw_maj_8b;
		fw_min = resp->hwrm_fw_min_8b;
		fw_bld = resp->hwrm_fw_bld_8b;
		fw_rsv = resp->hwrm_fw_rsvd_8b;
		len = BC_HWRM_STR_LEN;
	}

	softc->ver_info->fw_ver_code = BNXT_FW_VER_CODE(fw_maj, fw_min, fw_bld, fw_rsv);
	snprintf (softc->ver_info->fw_ver_str, len, "%d.%d.%d.%d",
			fw_maj, fw_min, fw_bld, fw_rsv);

	if (strlen(resp->active_pkg_name)) {
		int fw_ver_len = strlen (softc->ver_info->fw_ver_str);

		snprintf(softc->ver_info->fw_ver_str + fw_ver_len,
				FW_VER_STR_LEN - fw_ver_len - 1, "/pkg %s",
				resp->active_pkg_name);
		softc->fw_cap |= BNXT_FW_CAP_PKG_VER;
	}

	softc->ver_info->chip_num = le16toh(resp->chip_num);
	softc->ver_info->chip_rev = resp->chip_rev;
	softc->ver_info->chip_metal = resp->chip_metal;
	softc->ver_info->chip_bond_id = resp->chip_bond_id;
	softc->ver_info->chip_type = resp->chip_platform_type;

	if (resp->hwrm_intf_maj_8b >= 1) {
		softc->hwrm_max_req_len = le16toh(resp->max_req_win_len);
		softc->hwrm_max_ext_req_len = le16toh(resp->max_ext_req_len);
	}
	softc->hwrm_cmd_timeo = le16toh(resp->def_req_timeout);
	if (!softc->hwrm_cmd_timeo)
		softc->hwrm_cmd_timeo = DFLT_HWRM_CMD_TIMEOUT;

	dev_caps_cfg = le32toh(resp->dev_caps_cfg);
	if ((dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	    (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_REQUIRED))
		softc->flags |= BNXT_FLAG_SHORT_CMD;

	if ((dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	    (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_REQUIRED))
		softc->fw_cap |= BNXT_FW_CAP_SHORT_CMD;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_KONG_MB_CHNL_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_KONG_MB_CHNL;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_FLOW_HANDLE_64BIT_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_OVS_64BIT_HANDLE;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_TRUSTED_VF_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_TRUSTED_VF;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_CFA_ADV_FLOW_MGNT_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_CFA_ADV_FLOW;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_CFA_EEM_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_CFA_EEM;

	if (dev_caps_cfg & HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_CFA_TRUFLOW_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_TRUFLOW_EN;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

static const u16 bnxt_async_events_arr[] = {
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_PHY_CFG_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_RECOVERY,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RING_MONITOR_MSG,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEFAULT_VNIC_CHANGE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEBUG_NOTIFICATION,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEFERRED_RESPONSE,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ECHO_REQUEST,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PPS_TIMESTAMP,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT,
	HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PHC_UPDATE,
};

int bnxt_hwrm_func_drv_rgtr(struct bnxt_softc *bp, unsigned long *bmap, int bmap_size,
			    bool async_only)
{
	DECLARE_BITMAP(async_events_bmap, 256);
	u32 *events = (u32 *)async_events_bmap;
	struct hwrm_func_drv_rgtr_output *resp =
		(void *)bp->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_func_drv_rgtr_input req = {0};
	u32 flags = 0;
	int rc;
	int i;

	bnxt_hwrm_cmd_hdr_init(bp, &req, HWRM_FUNC_DRV_RGTR);
	req.ver_maj = HWRM_VERSION_MAJOR;
	req.ver_min = HWRM_VERSION_MINOR;
	req.ver_upd = HWRM_VERSION_UPDATE;

	req.enables = htole32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_OS_TYPE |
				   HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER |
				   HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD);

	if (bp->fw_cap & BNXT_FW_CAP_HOT_RESET)
		flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_HOT_RESET_SUPPORT;
	if (bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY)
		flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_ERROR_RECOVERY_SUPPORT |
			 HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_MASTER_SUPPORT;
	if (bp->fw_cap & BNXT_FW_CAP_NPAR_1_2)
		flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_NPAR_1_2_SUPPORT;
	flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_ASYM_QUEUE_CFG_SUPPORT;
	req.flags = htole32(flags);
	req.os_type = htole16(HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_FREEBSD);

	if (BNXT_PF(bp)) {
		req.enables |=
			htole32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_REQ_FWD);
	}

	if (bp->fw_cap & BNXT_FW_CAP_OVS_64BIT_HANDLE)
		req.flags |= cpu_to_le32(HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FLOW_HANDLE_64BIT_MODE);

	memset(async_events_bmap, 0, sizeof(async_events_bmap));
	for (i = 0; i < ARRAY_SIZE(bnxt_async_events_arr); i++) {
		u16 event_id = bnxt_async_events_arr[i];

		if (event_id == HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_RECOVERY &&
		    !(bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY)) {
			continue;
		}
		__set_bit(bnxt_async_events_arr[i], async_events_bmap);
	}
	if (bmap && bmap_size) {
		for (i = 0; i < bmap_size; i++) {
			if (test_bit(i, bmap))
				__set_bit(i, async_events_bmap);
		}
	}
	for (i = 0; i < 8; i++)
		req.async_event_fwd[i] |= htole32(events[i]);

	if (async_only)
		req.enables =
			htole32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD);

	rc = hwrm_send_message(bp, &req, sizeof(req));

	if (!rc) {
		if (resp->flags &
		    le32toh(HWRM_FUNC_DRV_RGTR_OUTPUT_FLAGS_IF_CHANGE_SUPPORTED))
			bp->fw_cap |= BNXT_FW_CAP_IF_CHANGE;
	}


	return rc;
}

int
bnxt_hwrm_func_drv_unrgtr(struct bnxt_softc *softc, bool shutdown)
{
	struct hwrm_func_drv_unrgtr_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_DRV_UNRGTR);
	if (shutdown == true)
		req.flags |=
		    HWRM_FUNC_DRV_UNRGTR_INPUT_FLAGS_PREPARE_FOR_SHUTDOWN;
	return hwrm_send_message(softc, &req, sizeof(req));
}

static inline int
_is_valid_ether_addr(uint8_t *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN)))
		return (FALSE);

	return (TRUE);
}

static inline void
get_random_ether_addr(uint8_t *addr)
{
	uint8_t temp[ETHER_ADDR_LEN];

	arc4rand(&temp, sizeof(temp), 0);
	temp[0] &= 0xFE;
	temp[0] |= 0x02;
	bcopy(temp, addr, sizeof(temp));
}

int
bnxt_hwrm_func_qcaps(struct bnxt_softc *softc)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {0};
	struct hwrm_func_qcaps_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct bnxt_func_info *func = &softc->func;
	uint32_t flags, flags_ext, flags_ext2;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_QCAPS);
	req.fid = htole16(0xffff);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	flags = htole32(resp->flags);
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_WOL_MAGICPKT_SUPPORTED)
		softc->flags |= BNXT_FLAG_WOL_CAP;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_STATS_SUPPORTED)
		softc->flags |= BNXT_FLAG_FW_CAP_EXT_STATS;

	/* Enable RoCE only on Thor devices */
	if (BNXT_CHIP_P5_PLUS(softc)) {
		if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ROCE_V1_SUPPORTED)
			softc->flags |= BNXT_FLAG_ROCEV1_CAP;
		if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ROCE_V2_SUPPORTED)
			softc->flags |= BNXT_FLAG_ROCEV2_CAP;
	}

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_LINK_ADMIN_STATUS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_LINK_ADMIN;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ADMIN_PF_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_ADMIN_PF;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_HOT_RESET_CAPABLE)
		softc->fw_cap |= BNXT_FW_CAP_HOT_RESET;
	if (flags &  HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ERROR_RECOVERY_CAPABLE)
		softc->fw_cap |= BNXT_FW_CAP_ERROR_RECOVERY;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PCIE_STATS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_PCIE_STATS_SUPPORTED;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_STATS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_EXT_STATS_SUPPORTED;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ERR_RECOVER_RELOAD)
		softc->fw_cap |= BNXT_FW_CAP_ERR_RECOVER_RELOAD;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_NOTIFY_VF_DEF_VNIC_CHNG_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_VF_VNIC_NOTIFY;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_CRASHDUMP_CMD_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_CRASHDUMP;
	if (!(flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_VLAN_ACCELERATION_TX_DISABLED))
		softc->fw_cap |= BNXT_FW_CAP_VLAN_TX_INSERT;
	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_DBG_QCAPS_CMD_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_DBG_QCAPS;

	flags_ext = htole32(resp->flags_ext);
	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_EXT_HW_STATS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_EXT_HW_STATS_SUPPORTED;
	if (BNXT_PF(softc) && (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_ECN_STATS_SUPPORTED))
		softc->fw_cap |= BNXT_FW_CAP_ECN_STATS;

	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_PTP_PPS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_PTP_PPS;
	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_PTP_PTM_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_PTP_PTM;
	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_PTP_64BIT_RTC_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_PTP_RTC;
	if (BNXT_PF(softc) && (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_HOT_RESET_IF_SUPPORT))
		softc->fw_cap |= BNXT_FW_CAP_HOT_RESET_IF;
	if (BNXT_PF(softc) && (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_FW_LIVEPATCH_SUPPORTED))
		softc->fw_cap |= BNXT_FW_CAP_LIVEPATCH;
	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_NPAR_1_2_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_NPAR_1_2;
	if (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_BS_V2_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_BACKING_STORE_V2;
	if (BNXT_PF(softc) &&
	    (flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_VF_CFG_ASYNC_FOR_PF_SUPPORTED))
		softc->fw_cap |= BNXT_FW_CAP_VF_CFG_FOR_PF;

	flags_ext2 = htole32(resp->flags_ext2);
	if (flags_ext2 & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_RX_ALL_PKTS_TIMESTAMPS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_RX_ALL_PKT_TS;
	if (flags_ext2 & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_SW_DBR_DROP_RECOVERY_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_DBR_SUPPORTED;
	if (flags_ext2 & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_DBR_PACING_EXT_SUPPORTED ||
	    flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_DBR_PACING_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_DBR_PACING_SUPPORTED;

	if (flags_ext2 & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_GENERIC_STATS_SUPPORTED)
		softc->fw_cap |= BNXT_FW_CAP_GENERIC_STATS;
	func->fw_fid = le16toh(resp->fid);
	memcpy(func->mac_addr, resp->mac_address, ETHER_ADDR_LEN);
	func->max_rsscos_ctxs = le16toh(resp->max_rsscos_ctx);
	func->max_cp_rings = le16toh(resp->max_cmpl_rings);
	func->max_tx_rings = le16toh(resp->max_tx_rings);
	func->max_rx_rings = le16toh(resp->max_rx_rings);
	func->max_hw_ring_grps = le32toh(resp->max_hw_ring_grps);
	if (!func->max_hw_ring_grps)
		func->max_hw_ring_grps = func->max_tx_rings;
	func->max_l2_ctxs = le16toh(resp->max_l2_ctxs);
	func->max_vnics = le16toh(resp->max_vnics);
	func->max_stat_ctxs = le16toh(resp->max_stat_ctx);
	if (BNXT_PF(softc)) {
		struct bnxt_pf_info *pf = &softc->pf;

		pf->port_id = le16toh(resp->port_id);
		pf->first_vf_id = le16toh(resp->first_vf_id);
		pf->max_vfs = le16toh(resp->max_vfs);
		pf->max_encap_records = le32toh(resp->max_encap_records);
		pf->max_decap_records = le32toh(resp->max_decap_records);
		pf->max_tx_em_flows = le32toh(resp->max_tx_em_flows);
		pf->max_tx_wm_flows = le32toh(resp->max_tx_wm_flows);
		pf->max_rx_em_flows = le32toh(resp->max_rx_em_flows);
		pf->max_rx_wm_flows = le32toh(resp->max_rx_wm_flows);
	}
	if (!_is_valid_ether_addr(func->mac_addr)) {
		device_printf(softc->dev, "Invalid ethernet address, generating random locally administered address\n");
		get_random_ether_addr(func->mac_addr);
	}

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_func_qcfg(struct bnxt_softc *softc)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct bnxt_func_qcfg *fn_qcfg = &softc->fn_qcfg;
	uint32_t min_db_offset = 0;
	uint16_t flags;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_QCFG);
	req.fid = htole16(0xffff);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto end;

	fn_qcfg->alloc_completion_rings = le16toh(resp->alloc_cmpl_rings);
	fn_qcfg->alloc_tx_rings = le16toh(resp->alloc_tx_rings);
	fn_qcfg->alloc_rx_rings = le16toh(resp->alloc_rx_rings);
	fn_qcfg->alloc_vnics = le16toh(resp->alloc_vnics);

	switch (resp->port_partition_type) {
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_0:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_2:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_5:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR2_0:
		softc->port_partition_type = resp->port_partition_type;
		break;
	}

	flags = le16toh(resp->flags);
	if (flags & (HWRM_FUNC_QCFG_OUTPUT_FLAGS_FW_DCBX_AGENT_ENABLED |
		     HWRM_FUNC_QCFG_OUTPUT_FLAGS_FW_LLDP_AGENT_ENABLED)) {
		softc->fw_cap |= BNXT_FW_CAP_LLDP_AGENT;
		if (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_FW_DCBX_AGENT_ENABLED)
			softc->fw_cap |= BNXT_FW_CAP_DCBX_AGENT;
	}
	if (BNXT_PF(softc) && (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_MULTI_HOST))
		softc->flags |= BNXT_FLAG_MULTI_HOST;
	if (BNXT_PF(softc) && (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_MULTI_ROOT))
		softc->flags |= BNXT_FLAG_MULTI_ROOT;
	if (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_SECURE_MODE_ENABLED)
		softc->fw_cap |= BNXT_FW_CAP_SECURE_MODE;
	if (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_RING_MONITOR_ENABLED)
		softc->fw_cap |= BNXT_FW_CAP_RING_MONITOR;
	if (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_ENABLE_RDMA_SRIOV)
		softc->fw_cap |= BNXT_FW_CAP_ENABLE_RDMA_SRIOV;

	if (softc->db_size)
		goto end;

	softc->legacy_db_size = le16_to_cpu(resp->legacy_l2_db_size_kb) * 1024;

	if (BNXT_CHIP_P5(softc)) {
		if (BNXT_PF(softc))
			min_db_offset = DB_PF_OFFSET_P5;
		else
			min_db_offset = DB_VF_OFFSET_P5;
		softc->legacy_db_size = min_db_offset;
	}

	softc->db_size = roundup2(le16_to_cpu(resp->l2_doorbell_bar_size_kb) *
			1024, PAGE_SIZE);
	if (!softc->db_size || softc->db_size > pci_resource_len(softc->pdev, 2) ||
			softc->db_size <= min_db_offset)
		softc->db_size = pci_resource_len(softc->pdev, 2);

	end:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_func_reset(struct bnxt_softc *softc)
{
	struct hwrm_func_reset_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_RESET);
	req.enables = 0;

	return hwrm_send_message(softc, &req, sizeof(req));
}

static void
bnxt_hwrm_set_link_common(struct bnxt_softc *softc,
    struct hwrm_port_phy_cfg_input *req)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	uint8_t autoneg = softc->link_info.autoneg;
	uint16_t fw_link_speed = softc->link_info.req_link_speed;

	if (autoneg & BNXT_AUTONEG_SPEED) {
		uint8_t phy_type = get_phy_type(softc);

		if (phy_type == HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASET ||
		    phy_type == HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET ||
		    phy_type == HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE) {

			req->auto_mode |= htole32(HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK);
			if (link_info->advertising) {
				req->enables |= htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK);
				req->auto_link_speed_mask = htole16(link_info->advertising);
			}
		} else {
			req->auto_mode |= HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS;
		}

		req->enables |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE);
		req->flags |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG);
	} else {

		if (link_info->force_speed2_nrz ||
		    link_info->force_pam4_56_speed2 ||
		    link_info->force_pam4_112_speed2) {
			req->force_link_speeds2 = htole16(fw_link_speed);
			req->enables |= htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_LINK_SPEEDS2);
			link_info->force_speed2_nrz = false;
			link_info->force_pam4_56_speed2 = false;
			link_info->force_pam4_112_speed2 = false;
		} else if (link_info->force_pam4_speed) {
			req->force_pam4_link_speed = htole16(fw_link_speed);
			req->enables |= htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAM4_LINK_SPEED);
			link_info->force_pam4_speed = false;
		} else {
			req->force_link_speed = htole16(fw_link_speed);
		}

		req->flags |= htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE);
	}

	/* tell chimp that the setting takes effect immediately */
	req->flags |= htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY);
}

static void
bnxt_hwrm_set_pause_common(struct bnxt_softc *softc,
    struct hwrm_port_phy_cfg_input *req)
{
	struct bnxt_link_info *link_info = &softc->link_info;

	if (link_info->flow_ctrl.autoneg) {
		req->auto_pause =
		    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_AUTONEG_PAUSE;
		if (link_info->flow_ctrl.rx)
			req->auto_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX;
		if (link_info->flow_ctrl.tx)
			req->auto_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX;
		req->enables |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE);
	} else {
		if (link_info->flow_ctrl.rx)
			req->force_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX;
		if (link_info->flow_ctrl.tx)
			req->force_pause |=
			    HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX;
		req->enables |=
			htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE);
		req->auto_pause = req->force_pause;
		req->enables |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE);
	}
}

/* JFV this needs interface connection */
static void
bnxt_hwrm_set_eee(struct bnxt_softc *softc, struct hwrm_port_phy_cfg_input *req)
{
	/* struct ethtool_eee *eee = &softc->eee; */
	bool	eee_enabled = false;

	if (eee_enabled) {
#if 0
		uint16_t eee_speeds;
		uint32_t flags = HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_ENABLE;

		if (eee->tx_lpi_enabled)
			flags |= HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI;

		req->flags |= htole32(flags);
		eee_speeds = bnxt_get_fw_auto_link_speeds(eee->advertised);
		req->eee_link_speed_mask = htole16(eee_speeds);
		req->tx_lpi_timer = htole32(eee->tx_lpi_timer);
#endif
	} else {
		req->flags |=
		    htole32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_DISABLE);
	}
}

int
bnxt_hwrm_set_link_setting(struct bnxt_softc *softc, bool set_pause,
    bool set_eee, bool set_link)
{
	struct hwrm_port_phy_cfg_input req = {0};
	int rc;

	if (softc->flags & BNXT_FLAG_NPAR)
		return ENOTSUP;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_CFG);

	if (set_pause) {
		bnxt_hwrm_set_pause_common(softc, &req);

		if (softc->link_info.flow_ctrl.autoneg)
			set_link = true;
	}

	if (set_link)
		bnxt_hwrm_set_link_common(softc, &req);

	if (set_eee)
		bnxt_hwrm_set_eee(softc, &req);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));

	if (!rc) {
		if (set_pause) {
			/* since changing of 'force pause' setting doesn't
			 * trigger any link change event, the driver needs to
			 * update the current pause result upon successfully i
			 * return of the phy_cfg command */
			if (!softc->link_info.flow_ctrl.autoneg)
				bnxt_report_link(softc);
		}
	}
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_vnic_set_hds(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_plcmodes_cfg_input req = {0};

	if (!BNXT_CHIP_P5_PLUS(softc))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_PLCMODES_CFG);

	req.flags = htole32(HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_JUMBO_PLACEMENT);
	req.vnic_id = htole16(vnic->id);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_vnic_cfg(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_cfg_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_CFG);

	if (vnic->flags & BNXT_VNIC_FLAG_DEFAULT)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT);
	if (vnic->flags & BNXT_VNIC_FLAG_BD_STALL)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE);
	if (vnic->flags & BNXT_VNIC_FLAG_VLAN_STRIP)
		req.flags |= htole32(HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE);
	if (BNXT_CHIP_P5_PLUS (softc)) {
		req.default_rx_ring_id =
			htole16(softc->rx_rings[0].phys_id);
		req.default_cmpl_ring_id =
			htole16(softc->rx_cp_rings[0].ring.phys_id);
		req.enables |=
			htole32(HWRM_VNIC_CFG_INPUT_ENABLES_DEFAULT_RX_RING_ID |
			    HWRM_VNIC_CFG_INPUT_ENABLES_DEFAULT_CMPL_RING_ID);
		req.vnic_id = htole16(vnic->id);
	} else {
		req.enables = htole32(HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP |
				HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE);
		req.vnic_id = htole16(vnic->id);
		req.dflt_ring_grp = htole16(vnic->def_ring_grp);
	}
	req.rss_rule = htole16(vnic->rss_id);
	req.cos_rule = htole16(vnic->cos_rule);
	req.lb_rule = htole16(vnic->lb_rule);
	req.enables |= htole32(HWRM_VNIC_CFG_INPUT_ENABLES_MRU);
	req.mru = htole16(vnic->mru);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_vnic_free(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_free_input req = {0};
	int rc = 0;

	if (vnic->id == (uint16_t)HWRM_NA_SIGNATURE)
		return rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_FREE);

	req.vnic_id = htole32(vnic->id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_vnic_alloc(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_alloc_input req = {0};
	struct hwrm_vnic_alloc_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	if (vnic->id != (uint16_t)HWRM_NA_SIGNATURE) {
		device_printf(softc->dev,
		    "Attempt to re-allocate vnic %04x\n", vnic->id);
		return EDOOFUS;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_ALLOC);

	if (vnic->flags & BNXT_VNIC_FLAG_DEFAULT)
		req.flags = htole32(HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	vnic->id = le32toh(resp->vnic_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_vnic_ctx_free(struct bnxt_softc *softc, uint16_t ctx_id)
{
	struct hwrm_vnic_rss_cos_lb_ctx_free_input req = {0};
	int rc = 0;

	if (ctx_id == (uint16_t)HWRM_NA_SIGNATURE)
		return rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_COS_LB_CTX_FREE);
	req.rss_cos_lb_ctx_id = htole16(ctx_id);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_vnic_ctx_alloc(struct bnxt_softc *softc, uint16_t *ctx_id)
{
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_input req = {0};
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	if (*ctx_id != (uint16_t)HWRM_NA_SIGNATURE) {
		device_printf(softc->dev,
		    "Attempt to re-allocate vnic ctx %04x\n", *ctx_id);
		return EDOOFUS;
	}

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_COS_LB_CTX_ALLOC);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	*ctx_id = le32toh(resp->rss_cos_lb_ctx_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_ring_grp_alloc(struct bnxt_softc *softc, struct bnxt_grp_info *grp)
{
	struct hwrm_ring_grp_alloc_input req = {0};
	struct hwrm_ring_grp_alloc_output *resp;
	int rc = 0;

	if (grp->grp_id != (uint16_t)HWRM_NA_SIGNATURE) {
		device_printf(softc->dev,
		    "Attempt to re-allocate ring group %04x\n", grp->grp_id);
		return EDOOFUS;
	}

	if (BNXT_CHIP_P5_PLUS (softc))
		return 0;

	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_GRP_ALLOC);
	req.cr = htole16(grp->cp_ring_id);
	req.rr = htole16(grp->rx_ring_id);
	req.ar = htole16(grp->ag_ring_id);
	req.sc = htole16(grp->stats_ctx);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	grp->grp_id = le32toh(resp->ring_group_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_ring_grp_free(struct bnxt_softc *softc, struct bnxt_grp_info *grp)
{
	struct hwrm_ring_grp_free_input req = {0};
	int rc = 0;

	if (grp->grp_id == (uint16_t)HWRM_NA_SIGNATURE)
		return 0;

	if (BNXT_CHIP_P5_PLUS (softc))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_GRP_FREE);

	req.ring_group_id = htole32(grp->grp_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int bnxt_hwrm_ring_free(struct bnxt_softc *softc, uint32_t ring_type,
		struct bnxt_ring *ring, int cmpl_ring_id)
{
        struct hwrm_ring_free_input req = {0};
	struct hwrm_ring_free_output *resp;
	int rc = 0;
        uint16_t error_code;

	if (ring->phys_id == (uint16_t)HWRM_NA_SIGNATURE)
		return 0;

	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_FREE);
	req.cmpl_ring = htole16(cmpl_ring_id);
        req.ring_type = ring_type;
        req.ring_id = htole16(ring->phys_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
        error_code = le16toh(resp->error_code);

	if (rc || error_code) {
		device_printf(softc->dev, "hwrm_ring_free type %d failed. "
				"rc:%x err:%x\n", ring_type, rc, error_code);
		if (!rc)
			rc = -EIO;
	}

	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

/*
 * Ring allocation message to the firmware
 */
int
bnxt_hwrm_ring_alloc(struct bnxt_softc *softc, uint8_t type,
                     struct bnxt_ring *ring)
{
	struct hwrm_ring_alloc_input req = {0};
	struct hwrm_ring_alloc_output *resp;
	uint16_t idx = ring->idx;
	struct bnxt_cp_ring *cp_ring;
	int rc;

	if (ring->phys_id != (uint16_t)HWRM_NA_SIGNATURE) {
		device_printf(softc->dev,
		    "Attempt to re-allocate ring %04x\n", ring->phys_id);
		return EDOOFUS;
	}

	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_RING_ALLOC);
	req.enables = htole32(0);
	req.fbo = htole32(0);
	req.ring_type = type;
	req.page_tbl_addr = htole64(ring->paddr);
	req.logical_id = htole16(ring->id);
	req.length = htole32(ring->ring_size);

	switch (type) {
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
		cp_ring = &softc->tx_cp_rings[idx];

		req.cmpl_ring_id = htole16(cp_ring->ring.phys_id);
		/* queue_id - what CoS queue the TX ring is associated with */
		req.queue_id = htole16(softc->tx_q_info[0].queue_id);

		req.stat_ctx_id = htole32(cp_ring->stats_ctx_id);
		req.enables |= htole32(
		    HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID);
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		if (!BNXT_CHIP_P5_PLUS(softc))
			break;

		cp_ring = &softc->rx_cp_rings[idx];

		req.stat_ctx_id = htole32(cp_ring->stats_ctx_id);
		req.rx_buf_size = htole16(softc->rx_buf_size);
		req.enables |= htole32(
			HWRM_RING_ALLOC_INPUT_ENABLES_RX_BUF_SIZE_VALID |
			HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID);
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG:
		if (!BNXT_CHIP_P5_PLUS(softc)) {
			req.ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX;
			break;
		}

		cp_ring = &softc->rx_cp_rings[idx];

		req.rx_ring_id = htole16(softc->rx_rings[idx].phys_id);
		req.stat_ctx_id = htole32(cp_ring->stats_ctx_id);
		req.rx_buf_size = htole16(softc->rx_buf_size);
		req.enables |= htole32(
			HWRM_RING_ALLOC_INPUT_ENABLES_RX_RING_ID_VALID |
			HWRM_RING_ALLOC_INPUT_ENABLES_RX_BUF_SIZE_VALID |
			HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID);
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
		if (!BNXT_CHIP_P5_PLUS(softc)) {
			req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
			break;
		}

                req.cq_handle = htole64(ring->id);
		req.nq_ring_id = htole16(softc->nq_rings[idx].ring.phys_id);
		req.enables |= htole32(
			HWRM_RING_ALLOC_INPUT_ENABLES_NQ_RING_ID_VALID);
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ:
		req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		break;
	default:
		device_printf(softc->dev,
			      "hwrm alloc invalid ring type %d\n", type);
		return -1;
	}

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	ring->phys_id = le16toh(resp->ring_id);

fail:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_stat_ctx_free(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr)
{
	struct hwrm_stat_ctx_free_input req = {0};
	int rc = 0;

	if (cpr->stats_ctx_id == HWRM_NA_SIGNATURE)
		return rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_STAT_CTX_FREE);

	req.stat_ctx_id = htole16(cpr->stats_ctx_id);
	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

fail:
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

int
bnxt_hwrm_stat_ctx_alloc(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr,
    uint64_t paddr)
{
	struct hwrm_stat_ctx_alloc_input req = {0};
	struct hwrm_stat_ctx_alloc_output *resp;
	int rc = 0;

	if (cpr->stats_ctx_id != HWRM_NA_SIGNATURE) {
		device_printf(softc->dev,
		    "Attempt to re-allocate stats ctx %08x\n",
		    cpr->stats_ctx_id);
		return EDOOFUS;
	}

	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_STAT_CTX_ALLOC);

	req.update_period_ms = htole32(1000);
	req.stats_dma_addr = htole64(paddr);

	if (BNXT_CHIP_P7(softc))
		req.stats_dma_length = htole16(sizeof(struct ctx_hw_stats_ext));
	else if (BNXT_CHIP_P5(softc))
		req.stats_dma_length = htole16(sizeof(struct ctx_hw_stats_ext) - 8);
	else
		req.stats_dma_length = htole16(sizeof(struct ctx_hw_stats));

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	cpr->stats_ctx_id = le32toh(resp->stat_ctx_id);

fail:
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

int
bnxt_hwrm_port_qstats(struct bnxt_softc *softc)
{
	struct hwrm_port_qstats_input req = {0};
	int rc = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_QSTATS);

	req.port_id = htole16(softc->pf.port_id);
	req.rx_stat_host_addr = htole64(softc->hw_rx_port_stats.idi_paddr);
	req.tx_stat_host_addr = htole64(softc->hw_tx_port_stats.idi_paddr);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}
static int bnxt_hwrm_pri2cos_idx(struct bnxt_softc *softc, uint32_t path_dir)
{
	struct hwrm_queue_pri2cos_qcfg_input req = {0};
	struct hwrm_queue_pri2cos_qcfg_output *resp;
	uint8_t *pri2cos_idx, *q_ids, max_q;
	int rc, i, j;
	uint8_t *pri2cos;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_QUEUE_PRI2COS_QCFG);
	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;

	req.flags = htole32(HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_IVLAN |
			    path_dir);
	rc = hwrm_send_message(softc, &req, sizeof(req));

	if (rc)
		return rc;

	if (path_dir == HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_PATH_TX) {
		pri2cos_idx = softc->tx_pri2cos_idx;
		q_ids = softc->tx_q_ids;
		max_q = softc->tx_max_q;
	} else {
		pri2cos_idx = softc->rx_pri2cos_idx;
		q_ids = softc->rx_q_ids;
		max_q = softc->rx_max_q;
	}

	pri2cos = &resp->pri0_cos_queue_id;

	for (i = 0; i < BNXT_MAX_QUEUE; i++) {
		uint8_t queue_id = pri2cos[i];
		uint8_t queue_idx;

		/* Per port queue IDs start from 0, 10, 20, etc */
		queue_idx = queue_id % 10;
		if (queue_idx > BNXT_MAX_QUEUE) {
			softc->pri2cos_valid = false;
			rc = -EINVAL;
			return rc;
		}

		for (j = 0; j < max_q; j++) {
			if (q_ids[j] == queue_id)
				pri2cos_idx[i] = queue_idx;
		}
	}

	softc->pri2cos_valid = true;

	return rc;
}

int
bnxt_hwrm_port_qstats_ext(struct bnxt_softc *softc)
{
	struct hwrm_port_qstats_ext_input req = {0};
	struct hwrm_port_qstats_ext_output *resp;
	int rc = 0, i;
	uint32_t tx_stat_size;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_QSTATS_EXT);
	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;

	tx_stat_size = sizeof(struct tx_port_stats_ext);
	req.port_id = htole16(softc->pf.port_id);
	req.tx_stat_size = htole16(tx_stat_size);
	req.rx_stat_size = htole16(sizeof(struct rx_port_stats_ext));
	req.rx_stat_host_addr = htole64(softc->hw_rx_port_stats_ext.idi_paddr);
	req.tx_stat_host_addr = htole64(softc->hw_tx_port_stats_ext.idi_paddr);

	rc = hwrm_send_message(softc, &req, sizeof(req));

	if (!rc) {
		softc->fw_rx_stats_ext_size =
			le16toh(resp->rx_stat_size) / 8;
		if (BNXT_FW_MAJ(softc) < 220 && !BNXT_CHIP_P7(softc) &&
		    softc->fw_rx_stats_ext_size > BNXT_RX_STATS_EXT_NUM_LEGACY)
			softc->fw_rx_stats_ext_size = BNXT_RX_STATS_EXT_NUM_LEGACY;

		softc->fw_tx_stats_ext_size = tx_stat_size ?
			le16toh(resp->tx_stat_size) / 8 : 0;
	} else {
		softc->fw_rx_stats_ext_size = 0;
		softc->fw_tx_stats_ext_size = 0;
	}

	if (softc->fw_tx_stats_ext_size <=
	    offsetof(struct tx_port_stats_ext, pfc_pri0_tx_duration_us) / 8) {
		softc->pri2cos_valid = false;
		return rc;
	}

	rc = bnxt_hwrm_pri2cos_idx(softc, HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_PATH_TX);
	if (rc)
		return rc;

	if (softc->is_asym_q) {
		rc = bnxt_hwrm_pri2cos_idx(softc, HWRM_QUEUE_PRI2COS_QCFG_INPUT_FLAGS_PATH_RX);
		if (rc)
			return rc;
	} else {
		memcpy(softc->rx_pri2cos_idx, softc->tx_pri2cos_idx, sizeof(softc->rx_pri2cos_idx));
	}

	u64 *rx_port_stats_ext = (u64 *)softc->hw_rx_port_stats_ext.idi_vaddr;
	u64 *tx_port_stats_ext = (u64 *)softc->hw_tx_port_stats_ext.idi_vaddr;

	if (softc->pri2cos_valid) {
		for (i = 0; i < 8; i++) {
			long n = bnxt_rx_bytes_pri_arr_base_off[i] +
				 softc->rx_pri2cos_idx[i];

			softc->rx_bytes_pri[i] = *(rx_port_stats_ext + n);
		}
		for (i = 0; i < 8; i++) {
			long n = bnxt_rx_pkts_pri_arr_base_off[i] +
				 softc->rx_pri2cos_idx[i];

			softc->rx_packets_pri[i] = *(rx_port_stats_ext + n);
		}
		for (i = 0; i < 8; i++) {
			long n = bnxt_tx_bytes_pri_arr_base_off[i] +
				 softc->tx_pri2cos_idx[i];

			softc->tx_bytes_pri[i] = *(tx_port_stats_ext + n);
		}
		for (i = 0; i < 8; i++) {
			long n = bnxt_tx_pkts_pri_arr_base_off[i] +
				 softc->tx_pri2cos_idx[i];

			softc->tx_packets_pri[i] = *(tx_port_stats_ext + n);
		}
	}

	return rc;
}

int
bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt_softc *softc,
    struct bnxt_vnic_info *vnic)
{
	struct hwrm_cfa_l2_set_rx_mask_input req = {0};
	uint32_t mask = vnic->rx_mask;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_SET_RX_MASK);

	req.vnic_id = htole32(vnic->id);
	req.mask = htole32(mask);
	req.mc_tbl_addr = htole64(vnic->mc_list.idi_paddr);
	req.num_mc_entries = htole32(vnic->mc_list_count);
	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_l2_filter_free(struct bnxt_softc *softc, uint64_t filter_id)
{
	struct hwrm_cfa_l2_filter_free_input	req = {0};
	int rc = 0;

	if (filter_id == -1)
		return rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_FILTER_FREE);

	req.l2_filter_id = htole64(filter_id);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_free_filter(struct bnxt_softc *softc)
{
	struct bnxt_vnic_info *vnic = &softc->vnic_info;
	struct bnxt_vlan_tag *tag;
	int rc = 0;

	rc = bnxt_hwrm_l2_filter_free(softc, softc->vnic_info.filter_id);
	if (rc)
		goto end;

	SLIST_FOREACH(tag, &vnic->vlan_tags, next) {
		rc = bnxt_hwrm_l2_filter_free(softc, tag->filter_id);
		if (rc)
			goto end;
		tag->filter_id = -1;
	}

end:
	return rc;
}

int
bnxt_hwrm_l2_filter_alloc(struct bnxt_softc *softc, uint16_t vlan_tag,
		uint64_t *filter_id)
{
	struct hwrm_cfa_l2_filter_alloc_input	req = {0};
	struct hwrm_cfa_l2_filter_alloc_output	*resp;
	struct bnxt_vnic_info *vnic = &softc->vnic_info;
	uint32_t enables = 0;
	int rc = 0;

	if (*filter_id != -1) {
		device_printf(softc->dev, "Attempt to re-allocate l2 ctx "
		    "filter (fid: 0x%jx)\n", (uintmax_t)*filter_id);
		return EDOOFUS;
	}

	resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_CFA_L2_FILTER_ALLOC);

	req.flags = htole32(HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX);
	enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR
	    | HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK
	    | HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID;

	if (vlan_tag != 0xffff) {
		enables |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_NUM_VLANS;
		req.l2_ivlan_mask = 0xffff;
		req.l2_ivlan = vlan_tag;
		req.num_vlans = 1;
	}

	req.enables = htole32(enables);
	req.dst_id = htole16(vnic->id);
	memcpy(req.l2_addr, if_getlladdr(iflib_get_ifp(softc->ctx)),
	    ETHER_ADDR_LEN);
	memset(&req.l2_addr_mask, 0xff, sizeof(req.l2_addr_mask));

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto fail;

	*filter_id = le64toh(resp->l2_filter_id);
fail:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_set_filter(struct bnxt_softc *softc)
{
	struct bnxt_vnic_info *vnic = &softc->vnic_info;
	struct bnxt_vlan_tag *tag;
	int rc = 0;

	rc = bnxt_hwrm_l2_filter_alloc(softc, 0xffff, &vnic->filter_id);
	if (rc)
		goto end;

	SLIST_FOREACH(tag, &vnic->vlan_tags, next) {
		rc = bnxt_hwrm_l2_filter_alloc(softc, tag->tag,
				&tag->filter_id);
		if (rc)
			goto end;
	}

end:
	return rc;
}

int
bnxt_hwrm_rss_cfg(struct bnxt_softc *softc, struct bnxt_vnic_info *vnic,
    uint32_t hash_type)
{
	struct hwrm_vnic_rss_cfg_input	req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_RSS_CFG);

	if (BNXT_CHIP_P7(softc))
		req.flags |= HWRM_VNIC_RSS_CFG_INPUT_FLAGS_IPSEC_HASH_TYPE_CFG_SUPPORT;

	req.hash_type = htole32(hash_type);
	req.ring_grp_tbl_addr = htole64(vnic->rss_grp_tbl.idi_paddr);
	req.hash_key_tbl_addr = htole64(vnic->rss_hash_key_tbl.idi_paddr);
	req.rss_ctx_idx = htole16(vnic->rss_id);
	req.hash_mode_flags = HWRM_FUNC_SPD_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
	if (BNXT_CHIP_P5_PLUS(softc)) {
		req.vnic_id = htole16(vnic->id);
		req.ring_table_pair_index = 0x0;
	}

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_reserve_pf_rings(struct bnxt_softc *softc)
{
	struct hwrm_func_cfg_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);

	req.fid = htole16(0xffff);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_MSIX);
	req.enables |= htole32(HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS);
	req.num_msix = htole16(BNXT_MAX_NUM_QUEUES);
	req.num_rsscos_ctxs = htole16(0x8);
	req.num_cmpl_rings = htole16(BNXT_MAX_NUM_QUEUES * 2);
	req.num_tx_rings = htole16(BNXT_MAX_NUM_QUEUES);
	req.num_rx_rings = htole16(BNXT_MAX_NUM_QUEUES);
	req.num_vnics = htole16(BNXT_MAX_NUM_QUEUES);
	req.num_stat_ctxs = htole16(BNXT_MAX_NUM_QUEUES * 2);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_cfg_async_cr(struct bnxt_softc *softc)
{
	int rc = 0;
	struct hwrm_func_cfg_input req = {0};

	if (!BNXT_PF(softc))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);

	req.fid = htole16(0xffff);
	req.enables = htole32(HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
	if (BNXT_CHIP_P5_PLUS(softc))
		req.async_event_cr = htole16(softc->nq_rings[0].ring.phys_id);
	else
		req.async_event_cr = htole16(softc->def_cp_ring.ring.phys_id);

	rc = hwrm_send_message(softc, &req, sizeof(req));

	return rc;
}

void
bnxt_validate_hw_lro_settings(struct bnxt_softc *softc)
{
	softc->hw_lro.enable = min(softc->hw_lro.enable, 1);

        softc->hw_lro.is_mode_gro = min(softc->hw_lro.is_mode_gro, 1);

	softc->hw_lro.max_agg_segs = min(softc->hw_lro.max_agg_segs,
		HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_MAX);

	softc->hw_lro.max_aggs = min(softc->hw_lro.max_aggs,
		HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_MAX);

	softc->hw_lro.min_agg_len = min(softc->hw_lro.min_agg_len, BNXT_MAX_MTU);
}

int
bnxt_hwrm_vnic_tpa_cfg(struct bnxt_softc *softc)
{
	struct hwrm_vnic_tpa_cfg_input req = {0};
	uint32_t flags;

	if (softc->vnic_info.id == (uint16_t) HWRM_NA_SIGNATURE) {
		return 0;
	}

	if (!(softc->flags & BNXT_FLAG_TPA))
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_VNIC_TPA_CFG);

	if (softc->hw_lro.enable) {
		flags = HWRM_VNIC_TPA_CFG_INPUT_FLAGS_TPA |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_ENCAP_TPA |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_ECN |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_SAME_GRE_SEQ;

        	if (softc->hw_lro.is_mode_gro)
			flags |= HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO;
		else
			flags |= HWRM_VNIC_TPA_CFG_INPUT_FLAGS_RSC_WND_UPDATE;

		req.flags = htole32(flags);

		req.enables = htole32(HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGG_SEGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MIN_AGG_LEN);

		req.max_agg_segs = htole16(softc->hw_lro.max_agg_segs);
		req.max_aggs = htole16(softc->hw_lro.max_aggs);
		req.min_agg_len = htole32(softc->hw_lro.min_agg_len);
	}

	req.vnic_id = htole16(softc->vnic_info.id);

	return hwrm_send_message(softc, &req, sizeof(req));
}

int
bnxt_hwrm_nvm_find_dir_entry(struct bnxt_softc *softc, uint16_t type,
    uint16_t *ordinal, uint16_t ext, uint16_t *index, bool use_index,
    uint8_t search_opt, uint32_t *data_length, uint32_t *item_length,
    uint32_t *fw_ver)
{
	struct hwrm_nvm_find_dir_entry_input req = {0};
	struct hwrm_nvm_find_dir_entry_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int	rc = 0;
	uint32_t old_timeo;

	MPASS(ordinal);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_FIND_DIR_ENTRY);
	if (use_index) {
		req.enables = htole32(
		    HWRM_NVM_FIND_DIR_ENTRY_INPUT_ENABLES_DIR_IDX_VALID);
		req.dir_idx = htole16(*index);
	}
	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(*ordinal);
	req.dir_ext = htole16(ext);
	req.opt_ordinal = search_opt;

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (item_length)
		*item_length = le32toh(resp->dir_item_length);
	if (data_length)
		*data_length = le32toh(resp->dir_data_length);
	if (fw_ver)
		*fw_ver = le32toh(resp->fw_ver);
	*ordinal = le16toh(resp->dir_ordinal);
	if (index)
		*index = le16toh(resp->dir_idx);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return (rc);
}

int
bnxt_hwrm_nvm_read(struct bnxt_softc *softc, uint16_t index, uint32_t offset,
    uint32_t length, struct iflib_dma_info *data)
{
	struct hwrm_nvm_read_input req = {0};
	int rc;
	uint32_t old_timeo;

	if (length > data->idi_size) {
		rc = EINVAL;
		goto exit;
	}
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_READ);
	req.host_dest_addr = htole64(data->idi_paddr);
	req.dir_idx = htole16(index);
	req.offset = htole32(offset);
	req.len = htole32(length);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	if (rc)
		goto exit;
	bus_dmamap_sync(data->idi_tag, data->idi_map, BUS_DMASYNC_POSTREAD);

	goto exit;

exit:
	return rc;
}

int
bnxt_hwrm_nvm_modify(struct bnxt_softc *softc, uint16_t index, uint32_t offset,
    void *data, bool cpyin, uint32_t length)
{
	struct hwrm_nvm_modify_input req = {0};
	struct iflib_dma_info dma_data;
	int rc;
	uint32_t old_timeo;

	if (length == 0 || !data)
		return EINVAL;
	rc = iflib_dma_alloc(softc->ctx, length, &dma_data,
	    BUS_DMA_NOWAIT);
	if (rc)
		return ENOMEM;
	if (cpyin) {
		rc = copyin(data, dma_data.idi_vaddr, length);
		if (rc)
			goto exit;
	}
	else
		memcpy(dma_data.idi_vaddr, data, length);
	bus_dmamap_sync(dma_data.idi_tag, dma_data.idi_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_MODIFY);
	req.host_src_addr = htole64(dma_data.idi_paddr);
	req.dir_idx = htole16(index);
	req.offset = htole32(offset);
	req.len = htole32(length);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);

exit:
	iflib_dma_free(&dma_data);
	return rc;
}

int
bnxt_hwrm_fw_reset(struct bnxt_softc *softc, uint8_t processor,
    uint8_t *selfreset)
{
	struct hwrm_fw_reset_input req = {0};
	struct hwrm_fw_reset_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	MPASS(selfreset);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_RESET);
	req.embedded_proc_type = processor;
	req.selfrst_status = *selfreset;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;
	*selfreset = resp->selfrst_status;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_qstatus(struct bnxt_softc *softc, uint8_t type, uint8_t *selfreset)
{
	struct hwrm_fw_qstatus_input req = {0};
	struct hwrm_fw_qstatus_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	MPASS(selfreset);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_QSTATUS);
	req.embedded_proc_type = type;

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;
	*selfreset = resp->selfrst_status;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_write(struct bnxt_softc *softc, void *data, bool cpyin,
    uint16_t type, uint16_t ordinal, uint16_t ext, uint16_t attr,
    uint16_t option, uint32_t data_length, bool keep, uint32_t *item_length,
    uint16_t *index)
{
	struct hwrm_nvm_write_input req = {0};
	struct hwrm_nvm_write_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct iflib_dma_info dma_data;
	int rc;
	uint32_t old_timeo;

	if (data_length) {
		rc = iflib_dma_alloc(softc->ctx, data_length, &dma_data,
		    BUS_DMA_NOWAIT);
		if (rc)
			return ENOMEM;
		if (cpyin) {
			rc = copyin(data, dma_data.idi_vaddr, data_length);
			if (rc)
				goto early_exit;
		}
		else
			memcpy(dma_data.idi_vaddr, data, data_length);
		bus_dmamap_sync(dma_data.idi_tag, dma_data.idi_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	else
		dma_data.idi_paddr = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_WRITE);

	req.host_src_addr = htole64(dma_data.idi_paddr);
	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(ordinal);
	req.dir_ext = htole16(ext);
	req.dir_attr = htole16(attr);
	req.dir_data_length = htole32(data_length);
	req.option = htole16(option);
	if (keep) {
		req.flags =
		    htole16(HWRM_NVM_WRITE_INPUT_FLAGS_KEEP_ORIG_ACTIVE_IMG);
	}
	if (item_length)
		req.dir_item_length = htole32(*item_length);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;
	if (item_length)
		*item_length = le32toh(resp->dir_item_length);
	if (index)
		*index = le16toh(resp->dir_idx);

exit:
	BNXT_HWRM_UNLOCK(softc);
early_exit:
	if (data_length)
		iflib_dma_free(&dma_data);
	return rc;
}

int
bnxt_hwrm_nvm_erase_dir_entry(struct bnxt_softc *softc, uint16_t index)
{
	struct hwrm_nvm_erase_dir_entry_input req = {0};
	uint32_t old_timeo;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_ERASE_DIR_ENTRY);
	req.dir_idx = htole16(index);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_get_dir_info(struct bnxt_softc *softc, uint32_t *entries,
    uint32_t *entry_length)
{
	struct hwrm_nvm_get_dir_info_input req = {0};
	struct hwrm_nvm_get_dir_info_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DIR_INFO);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (entries)
		*entries = le32toh(resp->entries);
	if (entry_length)
		*entry_length = le32toh(resp->entry_length);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_get_dir_entries(struct bnxt_softc *softc, uint32_t *entries,
    uint32_t *entry_length, struct iflib_dma_info *dma_data)
{
	struct hwrm_nvm_get_dir_entries_input req = {0};
	uint32_t ent;
	uint32_t ent_len;
	int rc;
	uint32_t old_timeo;

	if (!entries)
		entries = &ent;
	if (!entry_length)
		entry_length = &ent_len;

	rc = bnxt_hwrm_nvm_get_dir_info(softc, entries, entry_length);
	if (rc)
		goto exit;
	if (*entries * *entry_length > dma_data->idi_size) {
		rc = EINVAL;
		goto exit;
	}

	/*
	 * TODO: There's a race condition here that could blow up DMA memory...
	 *	 we need to allocate the max size, not the currently in use
	 *	 size.  The command should totally have a max size here.
	 */
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DIR_ENTRIES);
	req.host_dest_addr = htole64(dma_data->idi_paddr);
	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	if (rc)
		goto exit;
	bus_dmamap_sync(dma_data->idi_tag, dma_data->idi_map,
	    BUS_DMASYNC_POSTWRITE);

exit:
	return rc;
}

int
bnxt_hwrm_nvm_get_dev_info(struct bnxt_softc *softc, uint16_t *mfg_id,
    uint16_t *device_id, uint32_t *sector_size, uint32_t *nvram_size,
    uint32_t *reserved_size, uint32_t *available_size)
{
	struct hwrm_nvm_get_dev_info_input req = {0};
	struct hwrm_nvm_get_dev_info_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_GET_DEV_INFO);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (mfg_id)
		*mfg_id = le16toh(resp->manufacturer_id);
	if (device_id)
		*device_id = le16toh(resp->device_id);
	if (sector_size)
		*sector_size = le32toh(resp->sector_size);
	if (nvram_size)
		*nvram_size = le32toh(resp->nvram_size);
	if (reserved_size)
		*reserved_size = le32toh(resp->reserved_size);
	if (available_size)
		*available_size = le32toh(resp->available_size);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_install_update(struct bnxt_softc *softc,
    uint32_t install_type, uint64_t *installed_items, uint8_t *result,
    uint8_t *problem_item, uint8_t *reset_required)
{
	struct hwrm_nvm_install_update_input req = {0};
	struct hwrm_nvm_install_update_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;
	uint32_t old_timeo;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_INSTALL_UPDATE);
	req.install_type = htole32(install_type);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	if (rc)
		goto exit;

	if (installed_items)
		*installed_items = le32toh(resp->installed_items);
	if (result)
		*result = resp->result;
	if (problem_item)
		*problem_item = resp->problem_item;
	if (reset_required)
		*reset_required = resp->reset_required;

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_nvm_verify_update(struct bnxt_softc *softc, uint16_t type,
    uint16_t ordinal, uint16_t ext)
{
	struct hwrm_nvm_verify_update_input req = {0};
	uint32_t old_timeo;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_NVM_VERIFY_UPDATE);

	req.dir_type = htole16(type);
	req.dir_ordinal = htole16(ordinal);
	req.dir_ext = htole16(ext);

	BNXT_HWRM_LOCK(softc);
	old_timeo = softc->hwrm_cmd_timeo;
	softc->hwrm_cmd_timeo = BNXT_NVM_TIMEO;
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	softc->hwrm_cmd_timeo = old_timeo;
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_get_time(struct bnxt_softc *softc, uint16_t *year, uint8_t *month,
    uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second,
    uint16_t *millisecond, uint16_t *zone)
{
	struct hwrm_fw_get_time_input req = {0};
	struct hwrm_fw_get_time_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_GET_TIME);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;

	if (year)
		*year = le16toh(resp->year);
	if (month)
		*month = resp->month;
	if (day)
		*day = resp->day;
	if (hour)
		*hour = resp->hour;
	if (minute)
		*minute = resp->minute;
	if (second)
		*second = resp->second;
	if (millisecond)
		*millisecond = le16toh(resp->millisecond);
	if (zone)
		*zone = le16toh(resp->zone);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

int
bnxt_hwrm_fw_set_time(struct bnxt_softc *softc, uint16_t year, uint8_t month,
    uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
    uint16_t millisecond, uint16_t zone)
{
	struct hwrm_fw_set_time_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FW_SET_TIME);

	req.year = htole16(year);
	req.month = month;
	req.day = day;
	req.hour = hour;
	req.minute = minute;
	req.second = second;
	req.millisecond = htole16(millisecond);
	req.zone = htole16(zone);
	return hwrm_send_message(softc, &req, sizeof(req));
}

int bnxt_read_sfp_module_eeprom_info(struct bnxt_softc *softc, uint16_t i2c_addr,
    uint16_t page_number, uint8_t bank,bool bank_sel_en, uint16_t start_addr,
    uint16_t data_length, uint8_t *buf)
{
	struct hwrm_port_phy_i2c_read_output *output =
			(void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_port_phy_i2c_read_input req = {0};
	int rc = 0, byte_offset = 0;

	BNXT_HWRM_LOCK(softc);
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_I2C_READ);

	req.i2c_slave_addr = i2c_addr;
	req.page_number = htole16(page_number);
	req.port_id = htole16(softc->pf.port_id);
	do {
		uint16_t xfer_size;

		xfer_size = min_t(uint16_t, data_length, BNXT_MAX_PHY_I2C_RESP_SIZE);
		data_length -= xfer_size;
		req.page_offset = htole16(start_addr + byte_offset);
		req.data_length = xfer_size;
		req.bank_number = bank;
		req.enables = htole32((start_addr + byte_offset ?
				HWRM_PORT_PHY_I2C_READ_INPUT_ENABLES_PAGE_OFFSET : 0) |
				(bank_sel_en ?
				HWRM_PORT_PHY_I2C_READ_INPUT_ENABLES_BANK_NUMBER : 0));
		rc = _hwrm_send_message(softc, &req, sizeof(req));
		if (!rc)
			memcpy(buf + byte_offset, output->data, xfer_size);
		byte_offset += xfer_size;
	} while (!rc && data_length > 0);

	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

int
bnxt_hwrm_port_phy_qcfg(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	struct hwrm_port_phy_qcfg_input req = {0};
	struct hwrm_port_phy_qcfg_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_QCFG);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;

	memcpy(&link_info->phy_qcfg_resp, resp, sizeof(*resp));
	link_info->phy_link_status = resp->link;
	link_info->duplex =  resp->duplex_cfg;
	link_info->auto_mode = resp->auto_mode;

        /*
         * When AUTO_PAUSE_AUTONEG_PAUSE bit is set to 1,
         * the advertisement of pause is enabled.
         * 1. When the auto_mode is not set to none and this flag is set to 1,
         *    then the auto_pause bits on this port are being advertised and
         *    autoneg pause results are being interpreted.
         * 2. When the auto_mode is not set to none and this flag is set to 0,
         *    the pause is forced as indicated in force_pause, and also
	 *    advertised as auto_pause bits, but the autoneg results are not
	 *    interpreted since the pause configuration is being forced.
         * 3. When the auto_mode is set to none and this flag is set to 1,
         *    auto_pause bits should be ignored and should be set to 0.
         */

	link_info->flow_ctrl.autoneg = false;
	link_info->flow_ctrl.tx = false;
	link_info->flow_ctrl.rx = false;

	if ((resp->auto_mode) &&
            (resp->auto_pause & BNXT_AUTO_PAUSE_AUTONEG_PAUSE)) {
			link_info->flow_ctrl.autoneg = true;
	}

	if (link_info->flow_ctrl.autoneg) {
		if (resp->auto_pause & BNXT_PAUSE_TX)
			link_info->flow_ctrl.tx = true;
		if (resp->auto_pause & BNXT_PAUSE_RX)
			link_info->flow_ctrl.rx = true;
	} else {
		if (resp->force_pause & BNXT_PAUSE_TX)
			link_info->flow_ctrl.tx = true;
		if (resp->force_pause & BNXT_PAUSE_RX)
			link_info->flow_ctrl.rx = true;
	}

	link_info->duplex_setting = resp->duplex_cfg;
	if (link_info->phy_link_status == HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK)
		link_info->link_speed = le16toh(resp->link_speed);
	else
		link_info->link_speed = 0;
	link_info->force_link_speed = le16toh(resp->force_link_speed);
	link_info->auto_link_speeds = le16toh(resp->auto_link_speed);
	link_info->support_speeds = le16toh(resp->support_speeds);
	link_info->auto_link_speeds = le16toh(resp->auto_link_speed_mask);
	link_info->preemphasis = le32toh(resp->preemphasis);
	link_info->phy_ver[0] = resp->phy_maj;
	link_info->phy_ver[1] = resp->phy_min;
	link_info->phy_ver[2] = resp->phy_bld;
	snprintf(softc->ver_info->phy_ver, sizeof(softc->ver_info->phy_ver),
	    "%d.%d.%d", link_info->phy_ver[0], link_info->phy_ver[1],
	    link_info->phy_ver[2]);
	strlcpy(softc->ver_info->phy_vendor, resp->phy_vendor_name,
	    BNXT_NAME_SIZE);
	strlcpy(softc->ver_info->phy_partnumber, resp->phy_vendor_partnumber,
	    BNXT_NAME_SIZE);
	link_info->media_type = resp->media_type;
	link_info->phy_type = resp->phy_type;
	link_info->transceiver = resp->xcvr_pkg_type;
	link_info->phy_addr = resp->eee_config_phy_addr &
	    HWRM_PORT_PHY_QCFG_OUTPUT_PHY_ADDR_MASK;
	link_info->module_status = resp->module_status;
	link_info->support_pam4_speeds = le16toh(resp->support_pam4_speeds);
	link_info->auto_pam4_link_speeds = le16toh(resp->auto_pam4_link_speed_mask);
	link_info->force_pam4_link_speed = le16toh(resp->force_pam4_link_speed);

	if (softc->hwrm_spec_code >= 0x10504)
		link_info->active_fec_sig_mode = resp->active_fec_signal_mode;

	link_info->support_speeds2 = le16toh(resp->support_speeds2);
	link_info->auto_link_speeds2 = le16toh(resp->auto_link_speeds2);
	link_info->force_link_speeds2 = le16toh(resp->force_link_speeds2);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

static bool
bnxt_phy_qcaps_no_speed(struct hwrm_port_phy_qcaps_output *resp)
{
	if (!resp->supported_speeds_auto_mode &&
	    !resp->supported_speeds_force_mode &&
	    !resp->supported_pam4_speeds_auto_mode &&
	    !resp->supported_pam4_speeds_force_mode &&
	    !resp->supported_speeds2_auto_mode &&
	    !resp->supported_speeds2_force_mode)
		return true;

	return false;
}

int bnxt_hwrm_phy_qcaps(struct bnxt_softc *softc)
{
	struct bnxt_link_info *link_info = &softc->link_info;
	struct hwrm_port_phy_qcaps_output *resp =
	    (void *)softc->hwrm_cmd_resp.idi_vaddr;
	struct hwrm_port_phy_qcaps_input req = {};
	int rc;

	if (softc->hwrm_spec_code < 0x10201)
		return 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_PORT_PHY_QCAPS);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	if (rc)
		goto exit;

	softc->phy_flags = resp->flags | (resp->flags2 << 8);
	if (resp->flags & HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_EEE_SUPPORTED) {

		softc->lpi_tmr_lo = le32toh(resp->tx_lpi_timer_low) &
					    HWRM_PORT_PHY_QCAPS_OUTPUT_TX_LPI_TIMER_LOW_MASK;
		softc->lpi_tmr_hi = le32toh(resp->valid_tx_lpi_timer_high) &
					    HWRM_PORT_PHY_QCAPS_OUTPUT_TX_LPI_TIMER_HIGH_MASK;
	}

	if (softc->hwrm_spec_code >= 0x10a01) {
		if (bnxt_phy_qcaps_no_speed(resp)) {
			link_info->phy_state = BNXT_PHY_STATE_DISABLED;
			device_printf(softc->dev, "Ethernet link disabled\n");
		} else if (link_info->phy_state == BNXT_PHY_STATE_DISABLED) {
			link_info->phy_state = BNXT_PHY_STATE_ENABLED;
			device_printf(softc->dev, "Ethernet link enabled\n");
			/* Phy re-enabled, reprobe the speeds */
			link_info->support_auto_speeds = 0;
			link_info->support_pam4_auto_speeds = 0;
			link_info->support_auto_speeds2 = 0;
		}
	}
	if (resp->supported_speeds_auto_mode)
		link_info->support_auto_speeds =
			le16toh(resp->supported_speeds_auto_mode);
	if (resp->supported_speeds_force_mode)
		link_info->support_force_speeds =
			le16toh(resp->supported_speeds_force_mode);
	if (resp->supported_pam4_speeds_auto_mode)
		link_info->support_pam4_auto_speeds =
			le16toh(resp->supported_pam4_speeds_auto_mode);
	if (resp->supported_pam4_speeds_force_mode)
		link_info->support_pam4_force_speeds =
			le16toh(resp->supported_pam4_speeds_force_mode);

	if (resp->supported_speeds2_auto_mode)
		link_info->support_auto_speeds2 =
			le16toh(resp->supported_speeds2_auto_mode);

	if (resp->supported_speeds2_force_mode)
		link_info->support_force_speeds2 =
			le16toh(resp->supported_speeds2_force_mode);

exit:
	BNXT_HWRM_UNLOCK(softc);
	return rc;
}

uint16_t
bnxt_hwrm_get_wol_fltrs(struct bnxt_softc *softc, uint16_t handle)
{
	struct hwrm_wol_filter_qcfg_input req = {0};
	struct hwrm_wol_filter_qcfg_output *resp =
			(void *)softc->hwrm_cmd_resp.idi_vaddr;
	uint16_t next_handle = 0;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_QCFG);
	req.port_id = htole16(softc->pf.port_id);
	req.handle = htole16(handle);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		next_handle = le16toh(resp->next_handle);
		if (next_handle != 0) {
			if (resp->wol_type ==
				HWRM_WOL_FILTER_ALLOC_INPUT_WOL_TYPE_MAGICPKT) {
				softc->wol = 1;
				softc->wol_filter_id = resp->wol_filter_id;
			}
		}
	}
	return next_handle;
}

int
bnxt_hwrm_alloc_wol_fltr(struct bnxt_softc *softc)
{
	struct hwrm_wol_filter_alloc_input req = {0};
	struct hwrm_wol_filter_alloc_output *resp =
		(void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_ALLOC);
	req.port_id = htole16(softc->pf.port_id);
	req.wol_type = HWRM_WOL_FILTER_ALLOC_INPUT_WOL_TYPE_MAGICPKT;
	req.enables =
		htole32(HWRM_WOL_FILTER_ALLOC_INPUT_ENABLES_MAC_ADDRESS);
	memcpy(req.mac_address, softc->func.mac_addr, ETHER_ADDR_LEN);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (!rc)
		softc->wol_filter_id = resp->wol_filter_id;

	return rc;
}

int
bnxt_hwrm_free_wol_fltr(struct bnxt_softc *softc)
{
	struct hwrm_wol_filter_free_input req = {0};

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_WOL_FILTER_FREE);
	req.port_id = htole16(softc->pf.port_id);
	req.enables =
		htole32(HWRM_WOL_FILTER_FREE_INPUT_ENABLES_WOL_FILTER_ID);
	req.wol_filter_id = softc->wol_filter_id;
	return hwrm_send_message(softc, &req, sizeof(req));
}

static void bnxt_hwrm_set_coal_params(struct bnxt_softc *softc, uint32_t max_frames,
        uint32_t buf_tmrs, uint16_t flags,
        struct hwrm_ring_cmpl_ring_cfg_aggint_params_input *req)
{
        req->flags = htole16(flags);
        req->num_cmpl_dma_aggr = htole16((uint16_t)max_frames);
        req->num_cmpl_dma_aggr_during_int = htole16(max_frames >> 16);
        req->cmpl_aggr_dma_tmr = htole16((uint16_t)buf_tmrs);
        req->cmpl_aggr_dma_tmr_during_int = htole16(buf_tmrs >> 16);
        /* Minimum time between 2 interrupts set to buf_tmr x 2 */
        req->int_lat_tmr_min = htole16((uint16_t)buf_tmrs * 2);
        req->int_lat_tmr_max = htole16((uint16_t)buf_tmrs * 4);
        req->num_cmpl_aggr_int = htole16((uint16_t)max_frames * 4);
}

int bnxt_hwrm_set_coal(struct bnxt_softc *softc)
{
	int i, rc = 0;
	struct hwrm_ring_cmpl_ring_cfg_aggint_params_input req_rx = {0},
							   req_tx = {0}, *req;
	uint16_t max_buf, max_buf_irq;
	uint16_t buf_tmr, buf_tmr_irq;
	uint32_t flags;

	bnxt_hwrm_cmd_hdr_init(softc, &req_rx,
			       HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS);
	bnxt_hwrm_cmd_hdr_init(softc, &req_tx,
			       HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS);

	/* Each rx completion (2 records) should be DMAed immediately.
	 * DMA 1/4 of the completion buffers at a time.
	 */
	max_buf = min_t(uint16_t, softc->rx_coal_frames / 4, 2);
	/* max_buf must not be zero */
	max_buf = clamp_t(uint16_t, max_buf, 1, 63);
	max_buf_irq = clamp_t(uint16_t, softc->rx_coal_frames_irq, 1, 63);
	buf_tmr = BNXT_USEC_TO_COAL_TIMER(softc->rx_coal_usecs);
	/* buf timer set to 1/4 of interrupt timer */
	buf_tmr = max_t(uint16_t, buf_tmr / 4, 1);
	buf_tmr_irq = BNXT_USEC_TO_COAL_TIMER(softc->rx_coal_usecs_irq);
	buf_tmr_irq = max_t(uint16_t, buf_tmr_irq, 1);

	flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET;

	/* RING_IDLE generates more IRQs for lower latency.  Enable it only
	 * if coal_usecs is less than 25 us.
	 */
	if (softc->rx_coal_usecs < 25)
		flags |= HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_RING_IDLE;

	bnxt_hwrm_set_coal_params(softc, max_buf_irq << 16 | max_buf,
				  buf_tmr_irq << 16 | buf_tmr, flags, &req_rx);

	/* max_buf must not be zero */
	max_buf = clamp_t(uint16_t, softc->tx_coal_frames, 1, 63);
	max_buf_irq = clamp_t(uint16_t, softc->tx_coal_frames_irq, 1, 63);
	buf_tmr = BNXT_USEC_TO_COAL_TIMER(softc->tx_coal_usecs);
	/* buf timer set to 1/4 of interrupt timer */
	buf_tmr = max_t(uint16_t, buf_tmr / 4, 1);
	buf_tmr_irq = BNXT_USEC_TO_COAL_TIMER(softc->tx_coal_usecs_irq);
	buf_tmr_irq = max_t(uint16_t, buf_tmr_irq, 1);
	flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET;
	bnxt_hwrm_set_coal_params(softc, max_buf_irq << 16 | max_buf,
				  buf_tmr_irq << 16 | buf_tmr, flags, &req_tx);

	for (i = 0; i < softc->nrxqsets; i++) {

		req = &req_rx;
		req->ring_id = htole16(softc->grp_info[i].cp_ring_id);

		rc = hwrm_send_message(softc, req, sizeof(*req));
		if (rc)
			break;
	}
	return rc;
}

void bnxt_hwrm_ring_info_get(struct bnxt_softc *softc, uint8_t ring_type,
			     uint32_t ring_id, uint32_t *prod, uint32_t *cons)
{
	hwrm_dbg_ring_info_get_input_t req = {0};
	hwrm_dbg_ring_info_get_output_t *resp = (void *)softc->hwrm_cmd_resp.idi_vaddr;
	int rc = 0;

	*prod = *cons = 0xffffffff;
	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_DBG_RING_INFO_GET);
        req.ring_type = le32toh(ring_type);
        req.fw_ring_id = le32toh(ring_id);
	rc = hwrm_send_message(softc, &req, sizeof(req));
	if (!rc) {
		*prod = resp->producer_index;
		*cons = resp->consumer_index;
	}

	return;
}
