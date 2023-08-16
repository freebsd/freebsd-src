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
 *
 */

#ifndef _HW_CHANNEL_H
#define _HW_CHANNEL_H

#include <sys/sema.h>

#define DEFAULT_LOG2_THROTTLING_FOR_ERROR_EQ	4

#define HW_CHANNEL_MAX_REQUEST_SIZE		0x1000
#define HW_CHANNEL_MAX_RESPONSE_SIZE		0x1000

#define HW_CHANNEL_VF_BOOTSTRAP_QUEUE_DEPTH	1

#define HWC_INIT_DATA_CQID		1
#define HWC_INIT_DATA_RQID		2
#define HWC_INIT_DATA_SQID		3
#define HWC_INIT_DATA_QUEUE_DEPTH	4
#define HWC_INIT_DATA_MAX_REQUEST	5
#define HWC_INIT_DATA_MAX_RESPONSE	6
#define HWC_INIT_DATA_MAX_NUM_CQS	7
#define HWC_INIT_DATA_PDID		8
#define HWC_INIT_DATA_GPA_MKEY		9

/* Structures labeled with "HW DATA" are exchanged with the hardware. All of
 * them are naturally aligned and hence don't need __packed.
 */

union hwc_init_eq_id_db {
	uint32_t			as_uint32;

	struct {
		uint32_t		eq_id	: 16;
		uint32_t		doorbell: 16;
	};
}; /* HW DATA */

union hwc_init_type_data {
	uint32_t			as_uint32;

	struct {
		uint32_t value	: 24;
		uint32_t type	:  8;
	};
}; /* HW DATA */

struct hwc_rx_oob {
	uint32_t type	: 6;
	uint32_t eom		: 1;
	uint32_t som		: 1;
	uint32_t vendor_err	: 8;
	uint32_t reserved1	: 16;

	uint32_t src_virt_wq	: 24;
	uint32_t src_vfid	: 8;

	uint32_t reserved2;

	union {
		uint32_t wqe_addr_low;
		uint32_t wqe_offset;
	};

	uint32_t wqe_addr_high;

	uint32_t client_data_unit	: 14;
	uint32_t reserved3		: 18;

	uint32_t tx_oob_data_size;

	uint32_t chunk_offset	: 21;
	uint32_t reserved4		: 11;
}; /* HW DATA */

struct hwc_tx_oob {
	uint32_t reserved1;

	uint32_t reserved2;

	uint32_t vrq_id	: 24;
	uint32_t dest_vfid	: 8;

	uint32_t vrcq_id	: 24;
	uint32_t reserved3	: 8;

	uint32_t vscq_id	: 24;
	uint32_t loopback	: 1;
	uint32_t lso_override: 1;
	uint32_t dest_pf	: 1;
	uint32_t reserved4	: 5;

	uint32_t vsq_id	: 24;
	uint32_t reserved5	: 8;
}; /* HW DATA */

struct hwc_work_request {
	void *buf_va;
	void *buf_sge_addr;
	uint32_t buf_len;
	uint32_t msg_size;

	struct gdma_wqe_request wqe_req;
	struct hwc_tx_oob tx_oob;

	struct gdma_sge sge;
};

/* hwc_dma_buf represents the array of in-flight WQEs.
 * mem_info as know as the GDMA mapped memory is partitioned and used by
 * in-flight WQEs.
 * The number of WQEs is determined by the number of in-flight messages.
 */
struct hwc_dma_buf {
	struct gdma_mem_info mem_info;

	uint32_t gpa_mkey;

	uint32_t num_reqs;
	struct hwc_work_request reqs[];
};

typedef void hwc_rx_event_handler_t(void *ctx, uint32_t gdma_rxq_id,
				    const struct hwc_rx_oob *rx_oob);

typedef void hwc_tx_event_handler_t(void *ctx, uint32_t gdma_txq_id,
				    const struct hwc_rx_oob *rx_oob);

struct hwc_cq {
	struct hw_channel_context *hwc;

	struct gdma_queue *gdma_cq;
	struct gdma_queue *gdma_eq;
	struct gdma_comp *comp_buf;
	uint16_t queue_depth;

	hwc_rx_event_handler_t *rx_event_handler;
	void *rx_event_ctx;

	hwc_tx_event_handler_t *tx_event_handler;
	void *tx_event_ctx;
};

struct hwc_wq {
	struct hw_channel_context *hwc;

	struct gdma_queue *gdma_wq;
	struct hwc_dma_buf *msg_buf;
	uint16_t queue_depth;

	struct hwc_cq *hwc_cq;
};

struct hwc_caller_ctx {
	struct completion comp_event;
	void *output_buf;
	uint32_t output_buflen;

	uint32_t error; /* Error code */
	uint32_t status_code;
};

struct hw_channel_context {
	struct gdma_dev *gdma_dev;
	device_t dev;

	uint16_t num_inflight_msg;
	uint32_t max_req_msg_size;

	uint16_t hwc_init_q_depth_max;
	uint32_t hwc_init_max_req_msg_size;
	uint32_t hwc_init_max_resp_msg_size;

	struct completion hwc_init_eqe_comp;

	struct hwc_wq *rxq;
	struct hwc_wq *txq;
	struct hwc_cq *cq;

	struct sema sema;
	struct gdma_resource inflight_msg_res;

	struct hwc_caller_ctx *caller_ctx;
};

int mana_hwc_create_channel(struct gdma_context *gc);
void mana_hwc_destroy_channel(struct gdma_context *gc);

int mana_hwc_send_request(struct hw_channel_context *hwc, uint32_t req_len,
			  const void *req, uint32_t resp_len, void *resp);

#endif /* _HW_CHANNEL_H */
