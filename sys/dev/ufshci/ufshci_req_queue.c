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
	.disable = ufshci_req_sdb_disable,
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
	.disable = ufshci_req_sdb_disable,
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

void
ufshci_utmr_req_queue_disable(struct ufshci_controller *ctrlr)
{
	ctrlr->task_mgmt_req_queue.qops.disable(ctrlr,
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

void
ufshci_utr_req_queue_disable(struct ufshci_controller *ctrlr)
{
	ctrlr->transfer_req_queue.qops.disable(ctrlr,
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
		    "Function(0x%x) Invalid response code = 0x%x\n",
		    response->header.ext_iid_or_function,
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
	struct ufshci_hw_queue *hwq = tr->hwq;
	struct ufshci_request *req = tr->req;
	struct ufshci_completion cpl;
	uint8_t ocs;
	bool retry, error, retriable;

	mtx_assert(&hwq->qlock, MA_NOTOWNED);

	/* Copy the response from the Request Descriptor or UTP Command
	 * Descriptor. */
	cpl.size = tr->response_size;
	if (req_queue->is_task_mgmt) {
		memcpy(&cpl.response_upiu,
		    (void *)hwq->utmrd[tr->slot_num].response_upiu, cpl.size);

		ocs = hwq->utmrd[tr->slot_num].overall_command_status;
	} else {
		bus_dmamap_sync(req_queue->dma_tag_ucd, req_queue->ucdmem_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		memcpy(&cpl.response_upiu, (void *)tr->ucd->response_upiu,
		    cpl.size);

		ocs = hwq->utrd[tr->slot_num].overall_command_status;
	}

	error = ufshci_req_queue_response_is_error(req_queue, ocs,
	    &cpl.response_upiu);

	/* TODO: Implement retry */
	// retriable = ufshci_completion_is_retry(cpl);
	retriable = false;
	retry = error && retriable &&
	    req->retries < req_queue->ctrlr->retry_count;
	if (retry)
		hwq->num_retries++;
	if (error && req->retries >= req_queue->ctrlr->retry_count && retriable)
		hwq->num_failures++;

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

	mtx_lock(&hwq->qlock);

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

		TAILQ_REMOVE(&hwq->outstanding_tr, tr, tailq);
		TAILQ_INSERT_HEAD(&hwq->free_tr, tr, tailq);
	}

	mtx_unlock(&tr->hwq->qlock);
}

bool
ufshci_req_queue_process_completions(struct ufshci_req_queue *req_queue)
{
	struct ufshci_hw_queue *hwq;
	bool done;

	hwq = req_queue->qops.get_hw_queue(req_queue);

	mtx_lock(&hwq->recovery_lock);
	done = req_queue->qops.process_cpl(req_queue);
	mtx_unlock(&hwq->recovery_lock);

	return (done);
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

static void
ufshci_req_queue_timeout_recovery(struct ufshci_controller *ctrlr,
    struct ufshci_hw_queue *hwq)
{
	/* TODO: Step 2. Logical unit reset */
	/* TODO: Step 3. Target device reset */
	/* TODO: Step 4. Bus reset */

	/*
	 * Step 5. All previous commands were timeout.
	 * Recovery failed, reset the host controller.
	 */
	ufshci_printf(ctrlr,
	    "Recovery step 5: Resetting controller due to a timeout.\n");
	hwq->recovery_state = RECOVERY_WAITING;

	ufshci_ctrlr_reset(ctrlr);
}

static void
ufshci_abort_complete(void *arg, const struct ufshci_completion *status,
    bool error)
{
	struct ufshci_tracker *tr = arg;

	/*
	 * We still need to check the active tracker array, to cover race where
	 * I/O timed out at same time controller was completing the I/O. An
	 * abort request always is on the Task Management Request queue, but
	 * affects either an Task Management Request or an I/O (UTRL) queue, so
	 * take the appropriate queue lock for the original command's queue,
	 * since we'll need it to avoid races with the completion code and to
	 * complete the command manually.
	 */
	mtx_lock(&tr->hwq->qlock);
	if (tr->slot_state != UFSHCI_SLOT_STATE_FREE) {
		mtx_unlock(&tr->hwq->qlock);
		/*
		 * An I/O has timed out, and the controller was unable to abort
		 * it for some reason.  And we've not processed a completion for
		 * it yet. Construct a fake completion status, and then complete
		 * the I/O's tracker manually.
		 */
		ufshci_printf(tr->hwq->ctrlr,
		    "abort task request failed, aborting task manually\n");
		ufshci_req_queue_manual_complete_tracker(tr,
		    UFSHCI_DESC_ABORTED, UFSHCI_RESPONSE_CODE_GENERAL_FAILURE);

		if ((status->response_upiu.task_mgmt_response_upiu
			    .output_param1 ==
			UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_COMPLETE) ||
		    (status->response_upiu.task_mgmt_response_upiu
			    .output_param1 ==
			UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_SUCCEEDED)) {
			ufshci_printf(tr->hwq->ctrlr,
			    "Warning: the abort task request completed \
			    successfully, but the original task is still incomplete.");
			return;
		}

		/* Abort Task failed. Perform recovery steps 2-5 */
		ufshci_req_queue_timeout_recovery(tr->hwq->ctrlr, tr->hwq);
	} else {
		mtx_unlock(&tr->hwq->qlock);
	}
}

static void
ufshci_req_queue_timeout(void *arg)
{
	struct ufshci_hw_queue *hwq = arg;
	struct ufshci_controller *ctrlr = hwq->ctrlr;
	struct ufshci_tracker *tr;
	sbintime_t now;
	bool idle = true;
	bool fast;

	mtx_assert(&hwq->recovery_lock, MA_OWNED);

	/*
	 * If the controller is failed, then stop polling. This ensures that any
	 * failure processing that races with the hwq timeout will fail safely.
	 */
	if (ctrlr->is_failed) {
		ufshci_printf(ctrlr,
		    "Failed controller, stopping watchdog timeout.\n");
		hwq->timer_armed = false;
		return;
	}

	/*
	 * Shutdown condition: We set hwq->timer_armed to false in
	 * ufshci_req_sdb_destroy before calling callout_drain. When we call
	 * that, this routine might get called one last time. Exit w/o setting a
	 * timeout. None of the watchdog stuff needs to be done since we're
	 * destroying the hwq.
	 */
	if (!hwq->timer_armed) {
		ufshci_printf(ctrlr,
		    "Timeout fired during ufshci_utr_req_queue_destroy\n");
		return;
	}

	switch (hwq->recovery_state) {
	case RECOVERY_NONE:
		/*
		 * See if there's any recovery needed. First, do a fast check to
		 * see if anything could have timed out. If not, then skip
		 * everything else.
		 */
		fast = false;
		mtx_lock(&hwq->qlock);
		now = getsbinuptime();
		TAILQ_FOREACH(tr, &hwq->outstanding_tr, tailq) {
			/*
			 * If the first real transaction is not in timeout, then
			 * we're done. Otherwise, we try recovery.
			 */
			idle = false;
			if (now <= tr->deadline)
				fast = true;
			break;
		}
		mtx_unlock(&hwq->qlock);
		if (idle || fast)
			break;

		/*
		 * There's a stale transaction at the start of the queue whose
		 * deadline has passed. Poll the competions as a last-ditch
		 * effort in case an interrupt has been missed.
		 */
		hwq->req_queue->qops.process_cpl(hwq->req_queue);

		/*
		 * Now that we've run the ISR, re-rheck to see if there's any
		 * timed out commands and abort them or reset the card if so.
		 */
		mtx_lock(&hwq->qlock);
		idle = true;
		TAILQ_FOREACH(tr, &hwq->outstanding_tr, tailq) {
			/*
			 * If we know this tracker hasn't timed out, we also
			 * know all subsequent ones haven't timed out. The tr
			 * queue is in submission order and all normal commands
			 * in a queue have the same timeout (or the timeout was
			 * changed by the user, but we eventually timeout then).
			 */
			idle = false;
			if (now <= tr->deadline)
				break;

			/*
			 * Timeout recovery is performed in five steps. If
			 * recovery fails at any step, the process continues to
			 * the next one:
			 * next steps:
			 * Step 1. Abort task
			 * Step 2. Logical unit reset 	(TODO)
			 * Step 3. Target device reset 	(TODO)
			 * Step 4. Bus reset 		(TODO)
			 * Step 5. Host controller reset
			 *
			 * If the timeout occurred in the Task Management
			 * Request queue, ignore Step 1.
			 */
			if (ctrlr->enable_aborts &&
			    !hwq->req_queue->is_task_mgmt &&
			    tr->req->cb_fn != ufshci_abort_complete) {
				/*
				 * Step 1. Timeout expired, abort the task.
				 *
				 * This isn't an abort command, ask for a
				 * hardware abort. This goes to the Task
				 * Management Request queue which will reset the
				 * task if it times out.
				 */
				ufshci_printf(ctrlr,
				    "Recovery step 1: Timeout occurred. aborting the task(%d).\n",
				    tr->req->request_upiu.header.task_tag);
				ufshci_ctrlr_cmd_send_task_mgmt_request(ctrlr,
				    ufshci_abort_complete, tr,
				    UFSHCI_TASK_MGMT_FUNCTION_ABORT_TASK,
				    tr->req->request_upiu.header.lun,
				    tr->req->request_upiu.header.task_tag, 0);
			} else {
				/* Recovery Step 2-5 */
				ufshci_req_queue_timeout_recovery(ctrlr, hwq);
				idle = false;
				break;
			}
		}
		mtx_unlock(&hwq->qlock);
		break;

	case RECOVERY_WAITING:
		/*
		 * These messages aren't interesting while we're suspended. We
		 * put the queues into waiting state while suspending.
		 * Suspending takes a while, so we'll see these during that time
		 * and they aren't diagnostic. At other times, they indicate a
		 * problem that's worth complaining about.
		 */
		if (!device_is_suspended(ctrlr->dev))
			ufshci_printf(ctrlr, "Waiting for reset to complete\n");
		idle = false; /* We want to keep polling */
		break;
	}

	/*
	 * Rearm the timeout.
	 */
	if (!idle) {
		callout_schedule_sbt(&hwq->timer, SBT_1S / 2, SBT_1S / 2, 0);
	} else {
		hwq->timer_armed = false;
	}
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
	struct ufshci_hw_queue *hwq;
	uint64_t ucd_paddr;
	uint16_t request_len, response_off, response_len;
	uint8_t slot_num = tr->slot_num;
	int timeout;

	hwq = req_queue->qops.get_hw_queue(req_queue);

	mtx_assert(&hwq->qlock, MA_OWNED);

	if (req->cb_fn == ufshci_completion_poll_cb)
		timeout = 1;
	else
		timeout = ctrlr->timeout_period;
	tr->deadline = getsbinuptime() + timeout * SBT_1S;
	if (!hwq->timer_armed) {
		hwq->timer_armed = true;
		/*
		 * It wakes up once every 0.5 seconds to check if the deadline
		 * has passed.
		 */
		callout_reset_sbt_on(&hwq->timer, SBT_1S / 2, SBT_1S / 2,
		    ufshci_req_queue_timeout, hwq, hwq->cpu, 0);
	}

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

	TAILQ_REMOVE(&tr->hwq->free_tr, tr, tailq);
	TAILQ_INSERT_TAIL(&tr->hwq->outstanding_tr, tr, tailq);

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
