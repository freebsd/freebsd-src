/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/module.h>

#include <cam/scsi/scsi_all.h>

#include "sys/kassert.h"
#include "ufshci_private.h"

static void ufshci_req_queue_submit_tracker(struct ufshci_req_queue *req_queue,
    struct ufshci_tracker *tr, enum ufshci_data_direction data_direction);

static const struct ufshci_qops sdb_utmr_qops = {
	.construct = ufshci_req_sdb_construct,
	.destroy = ufshci_req_sdb_destroy,
	.get_hw_queue = ufshci_req_sdb_get_hw_queue,
	.enable = ufshci_req_sdb_enable,
	.reserve_slot = ufshci_req_sdb_reserve_slot,
	.reserve_admin_slot = ufshci_req_sdb_reserve_slot,
	.ring_doorbell = ufshci_req_sdb_utmr_ring_doorbell,
	.is_doorbell_cleared = ufshci_req_sdb_utmr_is_doorbell_cleared,
	.clear_cpl_ntf = ufshci_req_sdb_utmr_clear_cpl_ntf,
	.process_cpl = ufshci_req_sdb_process_cpl,
	.get_inflight_io = ufshci_req_sdb_get_inflight_io,
};

static const struct ufshci_qops sdb_utr_qops = {
	.construct = ufshci_req_sdb_construct,
	.destroy = ufshci_req_sdb_destroy,
	.get_hw_queue = ufshci_req_sdb_get_hw_queue,
	.enable = ufshci_req_sdb_enable,
	.reserve_slot = ufshci_req_sdb_reserve_slot,
	.reserve_admin_slot = ufshci_req_sdb_reserve_slot,
	.ring_doorbell = ufshci_req_sdb_utr_ring_doorbell,
	.is_doorbell_cleared = ufshci_req_sdb_utr_is_doorbell_cleared,
	.clear_cpl_ntf = ufshci_req_sdb_utr_clear_cpl_ntf,
	.process_cpl = ufshci_req_sdb_process_cpl,
	.get_inflight_io = ufshci_req_sdb_get_inflight_io,
};

int
ufshci_utmr_req_queue_construct(struct ufshci_controller *ctrlr)
{
	struct ufshci_req_queue *req_queue;
	int error;

	/*
	 * UTP Task Management Request only supports Legacy Single Doorbell
	 * Queue.
	 */
	req_queue = &ctrlr->task_mgmt_req_queue;
	req_queue->queue_mode = UFSHCI_Q_MODE_SDB;
	req_queue->qops = sdb_utmr_qops;

	error = req_queue->qops.construct(ctrlr, req_queue, UFSHCI_UTRM_ENTRIES,
	    /*is_task_mgmt*/ true);

	return (error);
}

void
ufshci_utmr_req_queue_destroy(struct ufshci_controller *ctrlr)
{
	ctrlr->task_mgmt_req_queue.qops.destroy(ctrlr,
	    &ctrlr->task_mgmt_req_queue);
}

int
ufshci_utmr_req_queue_enable(struct ufshci_controller *ctrlr)
{
	return (ctrlr->task_mgmt_req_queue.qops.enable(ctrlr,
	    &ctrlr->task_mgmt_req_queue));
}

int
ufshci_utr_req_queue_construct(struct ufshci_controller *ctrlr)
{
	struct ufshci_req_queue *req_queue;
	int error;

	/*
	 * Currently, it does not support MCQ mode, so it should be set to SDB
	 * mode by default.
	 * TODO: Determine queue mode by checking Capability Registers
	 */
	req_queue = &ctrlr->transfer_req_queue;
	req_queue->queue_mode = UFSHCI_Q_MODE_SDB;
	req_queue->qops = sdb_utr_qops;

	error = req_queue->qops.construct(ctrlr, req_queue, UFSHCI_UTR_ENTRIES,
	    /*is_task_mgmt*/ false);

	return (error);
}

void
ufshci_utr_req_queue_destroy(struct ufshci_controller *ctrlr)
{
	ctrlr->transfer_req_queue.qops.destroy(ctrlr,
	    &ctrlr->transfer_req_queue);
}

int
ufshci_utr_req_queue_enable(struct ufshci_controller *ctrlr)
{
	return (ctrlr->transfer_req_queue.qops.enable(ctrlr,
	    &ctrlr->transfer_req_queue));
}

static bool
ufshci_req_queue_response_is_error(struct ufshci_req_queue *req_queue,
    uint8_t ocs, union ufshci_reponse_upiu *response)
{
	bool is_error = false;

	/* Check request descriptor */
	if (ocs != UFSHCI_DESC_SUCCESS) {
		ufshci_printf(req_queue->ctrlr, "Invalid OCS = 0x%x\n", ocs);
		is_error = true;
	}

	/* Check response UPIU header */
	if (response->header.response != UFSHCI_RESPONSE_CODE_TARGET_SUCCESS) {
		ufshci_printf(req_queue->ctrlr,
		    "Invalid response code = 0x%x\n",
		    response->header.response);
		is_error = true;
	}

	return (is_error);
}

static void
ufshci_req_queue_manual_complete_tracker(struct ufshci_tracker *tr, uint8_t ocs,
    uint8_t rc)
{
	struct ufshci_utp_xfer_req_desc *desc;
	struct ufshci_upiu_header *resp_header;

	mtx_assert(&tr->hwq->qlock, MA_NOTOWNED);

	resp_header = (struct ufshci_upiu_header *)tr->ucd->response_upiu;
	resp_header->response = rc;

	desc = &tr->hwq->utrd[tr->slot_num];
	desc->overall_command_status = ocs;

	ufshci_req_queue_complete_tracker(tr);
}

static void
ufshci_req_queue_manual_complete_request(struct ufshci_req_queue *req_queue,
    struct ufshci_request *req, uint8_t ocs, uint8_t rc)
{
	struct ufshci_completion cpl;
	bool error;

	memset(&cpl, 0, sizeof(cpl));
	cpl.response_upiu.header.response = rc;
	error = ufshci_req_queue_response_is_error(req_queue, ocs,
	    &cpl.response_upiu);

	if (error) {
		ufshci_printf(req_queue->ctrlr,
		    "Manual complete request error:0x%x", error);
	}

	if (req->cb_fn)
		req->cb_fn(req->cb_arg, &cpl, error);

	ufshci_free_request(req);
}

void
ufshci_req_queue_fail(struct ufshci_controller *ctrlr,
    struct ufshci_hw_queue *hwq)
{
	struct ufshci_req_queue *req_queue;
	struct ufshci_tracker *tr;
	struct ufshci_request *req;
	int i;

	if (!mtx_initialized(&hwq->qlock))
		return;

	mtx_lock(&hwq->qlock);

	req_queue = &ctrlr->transfer_req_queue;

	for (i = 0; i < req_queue->num_entries; i++) {
		tr = hwq->act_tr[i];
		req = tr->req;

		if (tr->slot_state == UFSHCI_SLOT_STATE_RESERVED) {
			mtx_unlock(&hwq->qlock);
			ufshci_req_queue_manual_complete_request(req_queue, req,
			    UFSHCI_DESC_ABORTED,
			    UFSHCI_RESPONSE_CODE_GENERAL_FAILURE);
			mtx_lock(&hwq->qlock);
		} else if (tr->slot_state == UFSHCI_SLOT_STATE_SCHEDULED) {
			/*
			 * Do not remove the tracker. The abort_tracker path
			 * will do that for us.
			 */
			mtx_unlock(&hwq->qlock);
			ufshci_req_queue_manual_complete_tracker(tr,
			    UFSHCI_DESC_ABORTED,
			    UFSHCI_RESPONSE_CODE_GENERAL_FAILURE);
			mtx_lock(&hwq->qlock);
		}
	}

	mtx_unlock(&hwq->qlock);
}

void
ufshci_req_queue_complete_tracker(struct ufshci_tracker *tr)
{
	struct ufshci_req_queue *req_queue = tr->req_queue;
	struct ufshci_request *req = tr->req;
	struct ufshci_completion cpl;
	uint8_t ocs;
	bool retry, error, retriable;

	mtx_assert(&tr->hwq->qlock, MA_NOTOWNED);

	/* Copy the response from the Request Descriptor or UTP Command
	 * Descriptor. */
	if (req_queue->is_task_mgmt) {
		cpl.size = tr->response_size;
		memcpy(&cpl.response_upiu,
		    (void *)tr->hwq->utmrd[tr->slot_num].response_upiu,
		    cpl.size);

		ocs = tr->hwq->utmrd[tr->slot_num].overall_command_status;
	} else {
		bus_dmamap_sync(req_queue->dma_tag_ucd, req_queue->ucdmem_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cpl.size = tr->response_size;
		memcpy(&cpl.response_upiu, (void *)tr->ucd->response_upiu,
		    cpl.size);

		ocs = tr->hwq->utrd[tr->slot_num].overall_command_status;
	}

	error = ufshci_req_queue_response_is_error(req_queue, ocs,
	    &cpl.response_upiu);

	/* TODO: Implement retry */
	// retriable = ufshci_completion_is_retry(cpl);
	retriable = false;
	retry = error && retriable &&
	    req->retries < req_queue->ctrlr->retry_count;
	if (retry)
		tr->hwq->num_retries++;
	if (error && req->retries >= req_queue->ctrlr->retry_count && retriable)
		tr->hwq->num_failures++;

	KASSERT(tr->req, ("there is no request assigned to the tracker\n"));
	KASSERT(cpl.response_upiu.header.task_tag ==
		req->request_upiu.header.task_tag,
	    ("response task_tag does not match request task_tag\n"));

	if (!retry) {
		if (req->payload_valid) {
			bus_dmamap_sync(req_queue->dma_tag_payload,
			    tr->payload_dma_map,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		}
		/* Copy response from the command descriptor */
		if (req->cb_fn)
			req->cb_fn(req->cb_arg, &cpl, error);
	}

	mtx_lock(&tr->hwq->qlock);

	/* Clear the UTRL Completion Notification register */
	req_queue->qops.clear_cpl_ntf(req_queue->ctrlr, tr);

	if (retry) {
		req->retries++;
		ufshci_req_queue_submit_tracker(req_queue, tr,
		    req->data_direction);
	} else {
		if (req->payload_valid) {
			bus_dmamap_unload(req_queue->dma_tag_payload,
			    tr->payload_dma_map);
		}

		/* Clear tracker */
		ufshci_free_request(req);
		tr->req = NULL;
		tr->slot_state = UFSHCI_SLOT_STATE_FREE;
	}

	mtx_unlock(&tr->hwq->qlock);
}

bool
ufshci_req_queue_process_completions(struct ufshci_req_queue *req_queue)
{
	return (req_queue->qops.process_cpl(req_queue));
}

static void
ufshci_payload_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct ufshci_tracker *tr = arg;
	struct ufshci_prdt_entry *prdt_entry;
	int i;

	/*
	 * If the mapping operation failed, return immediately. The caller
	 * is responsible for detecting the error status and failing the
	 * tracker manually.
	 */
	if (error != 0) {
		ufshci_printf(tr->req_queue->ctrlr,
		    "Failed to map payload %d\n", error);
		return;
	}

	prdt_entry = (struct ufshci_prdt_entry *)tr->ucd->prd_table;

	tr->prdt_entry_cnt = nseg;

	for (i = 0; i < nseg; i++) {
		prdt_entry->data_base_address = htole64(seg[i].ds_addr) &
		    0xffffffff;
		prdt_entry->data_base_address_upper = htole64(seg[i].ds_addr) >>
		    32;
		prdt_entry->data_byte_count = htole32(seg[i].ds_len - 1);

		++prdt_entry;
	}

	bus_dmamap_sync(tr->req_queue->dma_tag_payload, tr->payload_dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

static void
ufshci_req_queue_prepare_prdt(struct ufshci_tracker *tr)
{
	struct ufshci_request *req = tr->req;
	struct ufshci_utp_cmd_desc *cmd_desc = tr->ucd;
	int error;

	tr->prdt_off = UFSHCI_UTP_XFER_REQ_SIZE + UFSHCI_UTP_XFER_RESP_SIZE;

	memset(cmd_desc->prd_table, 0, sizeof(cmd_desc->prd_table));

	/* Filling PRDT enrties with payload */
	error = bus_dmamap_load_mem(tr->req_queue->dma_tag_payload,
	    tr->payload_dma_map, &req->payload, ufshci_payload_map, tr,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		/*
		 * The dmamap operation failed, so we manually fail the
		 *  tracker here with UFSHCI_DESC_INVALID_PRDT_ATTRIBUTES.
		 *
		 * ufshci_req_queue_manual_complete_tracker must not be called
		 *  with the req_queue lock held.
		 */
		ufshci_printf(tr->req_queue->ctrlr,
		    "bus_dmamap_load_mem returned with error:0x%x!\n", error);

		mtx_unlock(&tr->hwq->qlock);
		ufshci_req_queue_manual_complete_tracker(tr,
		    UFSHCI_DESC_INVALID_PRDT_ATTRIBUTES,
		    UFSHCI_RESPONSE_CODE_GENERAL_FAILURE);
		mtx_lock(&tr->hwq->qlock);
	}
}

static void
ufshci_req_queue_fill_utmr_descriptor(
    struct ufshci_utp_task_mgmt_req_desc *desc, struct ufshci_request *req)
{
	memset(desc, 0, sizeof(struct ufshci_utp_task_mgmt_req_desc));
	desc->interrupt = true;
	/* Set the initial value to Invalid. */
	desc->overall_command_status = UFSHCI_UTMR_OCS_INVALID;

	memcpy(desc->request_upiu, &req->request_upiu, req->request_size);
}

static void
ufshci_req_queue_fill_utr_descriptor(struct ufshci_utp_xfer_req_desc *desc,
    uint8_t data_direction, const uint64_t paddr, const uint16_t response_off,
    const uint16_t response_len, const uint16_t prdt_off,
    const uint16_t prdt_entry_cnt)
{
	uint8_t command_type;
	/* Value to convert bytes to dwords */
	const uint16_t dword_size = 4;

	/*
	 * Set command type to UFS storage.
	 * The UFS 4.1 spec only defines 'UFS Storage' as a command type.
	 */
	command_type = UFSHCI_COMMAND_TYPE_UFS_STORAGE;

	memset(desc, 0, sizeof(struct ufshci_utp_xfer_req_desc));
	desc->command_type = command_type;
	desc->data_direction = data_direction;
	desc->interrupt = true;
	/* Set the initial value to Invalid. */
	desc->overall_command_status = UFSHCI_UTR_OCS_INVALID;
	desc->utp_command_descriptor_base_address = (uint32_t)(paddr &
	    0xffffffff);
	desc->utp_command_descriptor_base_address_upper = (uint32_t)(paddr >>
	    32);

	desc->response_upiu_offset = response_off / dword_size;
	desc->response_upiu_length = response_len / dword_size;
	desc->prdt_offset = prdt_off / dword_size;
	desc->prdt_length = prdt_entry_cnt;
}

/*
 * Submit the tracker to the hardware.
 */
static void
ufshci_req_queue_submit_tracker(struct ufshci_req_queue *req_queue,
    struct ufshci_tracker *tr, enum ufshci_data_direction data_direction)
{
	struct ufshci_controller *ctrlr = req_queue->ctrlr;
	struct ufshci_request *req = tr->req;
	uint64_t ucd_paddr;
	uint16_t request_len, response_off, response_len;
	uint8_t slot_num = tr->slot_num;

	mtx_assert(&req_queue->qops.get_hw_queue(req_queue)->qlock, MA_OWNED);

	/* TODO: Check timeout */

	if (req_queue->is_task_mgmt) {
		/* Prepare UTP Task Management Request Descriptor. */
		ufshci_req_queue_fill_utmr_descriptor(&tr->hwq->utmrd[slot_num],
		    req);
	} else {
		request_len = req->request_size;
		response_off = UFSHCI_UTP_XFER_REQ_SIZE;
		response_len = req->response_size;

		/* Prepare UTP Command Descriptor */
		memcpy(tr->ucd, &req->request_upiu, request_len);
		memset((uint8_t *)tr->ucd + response_off, 0, response_len);

		/* Prepare PRDT */
		if (req->payload_valid)
			ufshci_req_queue_prepare_prdt(tr);

		/* Prepare UTP Transfer Request Descriptor. */
		ucd_paddr = tr->ucd_bus_addr;
		ufshci_req_queue_fill_utr_descriptor(&tr->hwq->utrd[slot_num],
		    data_direction, ucd_paddr, response_off, response_len,
		    tr->prdt_off, tr->prdt_entry_cnt);

		bus_dmamap_sync(req_queue->dma_tag_ucd, req_queue->ucdmem_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	bus_dmamap_sync(tr->hwq->dma_tag_queue, tr->hwq->queuemem_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	tr->slot_state = UFSHCI_SLOT_STATE_SCHEDULED;

	/* Ring the doorbell */
	req_queue->qops.ring_doorbell(ctrlr, tr);
}

static int
_ufshci_req_queue_submit_request(struct ufshci_req_queue *req_queue,
    struct ufshci_request *req)
{
	struct ufshci_tracker *tr = NULL;
	int error;

	mtx_assert(&req_queue->qops.get_hw_queue(req_queue)->qlock, MA_OWNED);

	error = req_queue->qops.reserve_slot(req_queue, &tr);
	if (error != 0) {
		ufshci_printf(req_queue->ctrlr, "Failed to get tracker");
		return (error);
	}
	KASSERT(tr, ("There is no tracker allocated."));

	if (tr->slot_state == UFSHCI_SLOT_STATE_RESERVED ||
	    tr->slot_state == UFSHCI_SLOT_STATE_SCHEDULED)
		return (EBUSY);

	/* Set the task_tag value to slot_num for traceability. */
	req->request_upiu.header.task_tag = tr->slot_num;

	tr->slot_state = UFSHCI_SLOT_STATE_RESERVED;
	tr->response_size = req->response_size;
	tr->deadline = SBT_MAX;
	tr->req = req;

	ufshci_req_queue_submit_tracker(req_queue, tr, req->data_direction);

	return (0);
}

int
ufshci_req_queue_submit_request(struct ufshci_req_queue *req_queue,
    struct ufshci_request *req, bool is_admin)
{
	struct ufshci_hw_queue *hwq;
	uint32_t error;

	/* TODO: MCQs should use a separate Admin queue. */

	hwq = req_queue->qops.get_hw_queue(req_queue);
	KASSERT(hwq, ("There is no HW queue allocated."));

	mtx_lock(&hwq->qlock);
	error = _ufshci_req_queue_submit_request(req_queue, req);
	mtx_unlock(&hwq->qlock);

	return (error);
}
