/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>

#include "nvme_private.h"

static void	_nvme_qpair_submit_request(struct nvme_qpair *qpair,
					   struct nvme_request *req);

static boolean_t
nvme_completion_is_error(struct nvme_completion *cpl)
{

	return (cpl->sf_sc != 0 || cpl->sf_sct != 0);
}

static boolean_t
nvme_completion_is_retry(const struct nvme_completion *cpl)
{
	/*
	 * TODO: spec is not clear how commands that are aborted due
	 *  to TLER will be marked.  So for now, it seems
	 *  NAMESPACE_NOT_READY is the only case where we should
	 *  look at the DNR bit.
	 */
	switch (cpl->sf_sct) {
	case NVME_SCT_GENERIC:
		switch (cpl->sf_sc) {
		case NVME_SC_ABORTED_BY_REQUEST:
			return (1);
		case NVME_SC_NAMESPACE_NOT_READY:
			if (cpl->sf_dnr)
				return (0);
			else
				return (1);
		case NVME_SC_INVALID_OPCODE:
		case NVME_SC_INVALID_FIELD:
		case NVME_SC_COMMAND_ID_CONFLICT:
		case NVME_SC_DATA_TRANSFER_ERROR:
		case NVME_SC_ABORTED_POWER_LOSS:
		case NVME_SC_INTERNAL_DEVICE_ERROR:
		case NVME_SC_ABORTED_SQ_DELETION:
		case NVME_SC_ABORTED_FAILED_FUSED:
		case NVME_SC_ABORTED_MISSING_FUSED:
		case NVME_SC_INVALID_NAMESPACE_OR_FORMAT:
		case NVME_SC_COMMAND_SEQUENCE_ERROR:
		case NVME_SC_LBA_OUT_OF_RANGE:
		case NVME_SC_CAPACITY_EXCEEDED:
		default:
			return (0);
		}
	case NVME_SCT_COMMAND_SPECIFIC:
	case NVME_SCT_MEDIA_ERROR:
	case NVME_SCT_VENDOR_SPECIFIC:
	default:
		return (0);
	}
}

static void
nvme_qpair_construct_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr,
    uint16_t cid)
{

	bus_dmamap_create(qpair->dma_tag, 0, &tr->payload_dma_map);
	bus_dmamap_create(qpair->dma_tag, 0, &tr->prp_dma_map);

	bus_dmamap_load(qpair->dma_tag, tr->prp_dma_map, tr->prp,
	    sizeof(tr->prp), nvme_single_map, &tr->prp_bus_addr, 0);

	callout_init(&tr->timer, 1);
	tr->cid = cid;
	tr->qpair = qpair;
}

static void
nvme_qpair_complete_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr,
    struct nvme_completion *cpl, boolean_t print_on_error)
{
	struct nvme_request	*req;
	boolean_t		retry, error;

	req = tr->req;
	error = nvme_completion_is_error(cpl);
	retry = error && nvme_completion_is_retry(cpl);

	if (error && print_on_error) {
		nvme_dump_completion(cpl);
		nvme_dump_command(&req->cmd);
	}

	qpair->act_tr[cpl->cid] = NULL;

	KASSERT(cpl->cid == req->cmd.cid, ("cpl cid does not match cmd cid\n"));

	if (req->cb_fn && !retry)
		req->cb_fn(req->cb_arg, cpl);

	mtx_lock(&qpair->lock);
	callout_stop(&tr->timer);

	if (retry)
		nvme_qpair_submit_tracker(qpair, tr);
	else {
		if (req->payload_size > 0 || req->uio != NULL)
			bus_dmamap_unload(qpair->dma_tag,
			    tr->payload_dma_map);

		nvme_free_request(req);
		tr->req = NULL;

		TAILQ_REMOVE(&qpair->outstanding_tr, tr, tailq);
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);

		if (!STAILQ_EMPTY(&qpair->queued_req)) {
			req = STAILQ_FIRST(&qpair->queued_req);
			STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
			_nvme_qpair_submit_request(qpair, req);
		}
	}

	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_manual_complete_tracker(struct nvme_qpair *qpair,
    struct nvme_tracker *tr, uint32_t sct, uint32_t sc,
    boolean_t print_on_error)
{
	struct nvme_completion	cpl;

	memset(&cpl, 0, sizeof(cpl));
	cpl.sqid = qpair->id;
	cpl.cid = tr->cid;
	cpl.sf_sct = sct;
	cpl.sf_sc = sc;
	nvme_qpair_complete_tracker(qpair, tr, &cpl, print_on_error);
}

void
nvme_qpair_process_completions(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;
	struct nvme_completion	*cpl;

	qpair->num_intr_handler_calls++;

	if (!qpair->is_enabled)
		/*
		 * qpair is not enabled, likely because a controller reset is
		 *  is in progress.  Ignore the interrupt - any I/O that was
		 *  associated with this interrupt will get retried when the
		 *  reset is complete.
		 */
		return;

	while (1) {
		cpl = &qpair->cpl[qpair->cq_head];

		if (cpl->p != qpair->phase)
			break;

		tr = qpair->act_tr[cpl->cid];

		if (tr != NULL) {
			nvme_qpair_complete_tracker(qpair, tr, cpl, TRUE);
			qpair->sq_head = cpl->sqhd;
		} else {
			printf("cpl does not map to outstanding cmd\n");
			nvme_dump_completion(cpl);
			KASSERT(0, ("received completion for unknown cmd\n"));
		}

		if (++qpair->cq_head == qpair->num_entries) {
			qpair->cq_head = 0;
			qpair->phase = !qpair->phase;
		}

		nvme_mmio_write_4(qpair->ctrlr, doorbell[qpair->id].cq_hdbl,
		    qpair->cq_head);
	}
}

static void
nvme_qpair_msix_handler(void *arg)
{
	struct nvme_qpair *qpair = arg;

	nvme_qpair_process_completions(qpair);
}

void
nvme_qpair_construct(struct nvme_qpair *qpair, uint32_t id,
    uint16_t vector, uint32_t num_entries, uint32_t num_trackers,
    uint32_t max_xfer_size, struct nvme_controller *ctrlr)
{
	struct nvme_tracker	*tr;
	uint32_t		i;

	qpair->id = id;
	qpair->vector = vector;
	qpair->num_entries = num_entries;
#ifdef CHATHAM2
	/*
	 * Chatham prototype board starts having issues at higher queue
	 *  depths.  So use a conservative estimate here of no more than 64
	 *  outstanding I/O per queue at any one point.
	 */
	if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID)
		num_trackers = min(num_trackers, 64);
#endif
	qpair->num_trackers = num_trackers;
	qpair->max_xfer_size = max_xfer_size;
	qpair->ctrlr = ctrlr;

	if (ctrlr->msix_enabled) {

		/*
		 * MSI-X vector resource IDs start at 1, so we add one to
		 *  the queue's vector to get the corresponding rid to use.
		 */
		qpair->rid = vector + 1;

		qpair->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
		    &qpair->rid, RF_ACTIVE);

		bus_setup_intr(ctrlr->dev, qpair->res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL,
		    nvme_qpair_msix_handler, qpair, &qpair->tag);
	}

	mtx_init(&qpair->lock, "nvme qpair lock", NULL, MTX_DEF);

	bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    sizeof(uint64_t), PAGE_SIZE, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, qpair->max_xfer_size,
	    (qpair->max_xfer_size/PAGE_SIZE)+1, PAGE_SIZE, 0,
	    NULL, NULL, &qpair->dma_tag);

	qpair->num_cmds = 0;
	qpair->num_intr_handler_calls = 0;

	/* TODO: error checking on contigmalloc, bus_dmamap_load calls */
	qpair->cmd = contigmalloc(qpair->num_entries *
	    sizeof(struct nvme_command), M_NVME, M_ZERO | M_NOWAIT,
	    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);
	qpair->cpl = contigmalloc(qpair->num_entries *
	    sizeof(struct nvme_completion), M_NVME, M_ZERO | M_NOWAIT,
	    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);

	bus_dmamap_create(qpair->dma_tag, 0, &qpair->cmd_dma_map);
	bus_dmamap_create(qpair->dma_tag, 0, &qpair->cpl_dma_map);

	bus_dmamap_load(qpair->dma_tag, qpair->cmd_dma_map,
	    qpair->cmd, qpair->num_entries * sizeof(struct nvme_command),
	    nvme_single_map, &qpair->cmd_bus_addr, 0);
	bus_dmamap_load(qpair->dma_tag, qpair->cpl_dma_map,
	    qpair->cpl, qpair->num_entries * sizeof(struct nvme_completion),
	    nvme_single_map, &qpair->cpl_bus_addr, 0);

	qpair->sq_tdbl_off = nvme_mmio_offsetof(doorbell[id].sq_tdbl);
	qpair->cq_hdbl_off = nvme_mmio_offsetof(doorbell[id].cq_hdbl);

	TAILQ_INIT(&qpair->free_tr);
	TAILQ_INIT(&qpair->outstanding_tr);
	STAILQ_INIT(&qpair->queued_req);

	for (i = 0; i < qpair->num_trackers; i++) {
		tr = malloc(sizeof(*tr), M_NVME, M_ZERO | M_NOWAIT);

		if (tr == NULL) {
			printf("warning: nvme tracker malloc failed\n");
			break;
		}

		nvme_qpair_construct_tracker(qpair, tr, i);
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);
	}

	qpair->act_tr = malloc(sizeof(struct nvme_tracker *) * qpair->num_entries,
	    M_NVME, M_ZERO | M_NOWAIT);
}

static void
nvme_qpair_destroy(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	if (qpair->tag)
		bus_teardown_intr(qpair->ctrlr->dev, qpair->res, qpair->tag);

	if (qpair->res)
		bus_release_resource(qpair->ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(qpair->res), qpair->res);

	if (qpair->dma_tag)
		bus_dma_tag_destroy(qpair->dma_tag);

	if (qpair->act_tr)
		free(qpair->act_tr, M_NVME);

	while (!TAILQ_EMPTY(&qpair->free_tr)) {
		tr = TAILQ_FIRST(&qpair->free_tr);
		TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
		bus_dmamap_destroy(qpair->dma_tag, tr->payload_dma_map);
		bus_dmamap_destroy(qpair->dma_tag, tr->prp_dma_map);
		free(tr, M_NVME);
	}
}

static void
nvme_admin_qpair_abort_aers(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	tr = TAILQ_FIRST(&qpair->outstanding_tr);
	while (tr != NULL) {
		if (tr->req->cmd.opc == NVME_OPC_ASYNC_EVENT_REQUEST) {
			nvme_qpair_manual_complete_tracker(qpair, tr,
			    NVME_SCT_GENERIC, NVME_SC_ABORTED_SQ_DELETION,
			    FALSE);
			tr = TAILQ_FIRST(&qpair->outstanding_tr);
		} else {
			tr = TAILQ_NEXT(tr, tailq);
		}
	}
}

void
nvme_admin_qpair_destroy(struct nvme_qpair *qpair)
{

	nvme_admin_qpair_abort_aers(qpair);

	/*
	 * For NVMe, you don't send delete queue commands for the admin
	 *  queue, so we just need to unload and free the cmd and cpl memory.
	 */
	bus_dmamap_unload(qpair->dma_tag, qpair->cmd_dma_map);
	bus_dmamap_destroy(qpair->dma_tag, qpair->cmd_dma_map);

	contigfree(qpair->cmd,
	    qpair->num_entries * sizeof(struct nvme_command), M_NVME);

	bus_dmamap_unload(qpair->dma_tag, qpair->cpl_dma_map);
	bus_dmamap_destroy(qpair->dma_tag, qpair->cpl_dma_map);
	contigfree(qpair->cpl,
	    qpair->num_entries * sizeof(struct nvme_completion), M_NVME);

	nvme_qpair_destroy(qpair);
}

static void
nvme_free_cmd_ring(void *arg, const struct nvme_completion *status)
{
	struct nvme_qpair *qpair;

	qpair = (struct nvme_qpair *)arg;
	bus_dmamap_unload(qpair->dma_tag, qpair->cmd_dma_map);
	bus_dmamap_destroy(qpair->dma_tag, qpair->cmd_dma_map);
	contigfree(qpair->cmd,
	    qpair->num_entries * sizeof(struct nvme_command), M_NVME);
	qpair->cmd = NULL;
}

static void
nvme_free_cpl_ring(void *arg, const struct nvme_completion *status)
{
	struct nvme_qpair *qpair;

	qpair = (struct nvme_qpair *)arg;
	bus_dmamap_unload(qpair->dma_tag, qpair->cpl_dma_map);
	bus_dmamap_destroy(qpair->dma_tag, qpair->cpl_dma_map);
	contigfree(qpair->cpl,
	    qpair->num_entries * sizeof(struct nvme_completion), M_NVME);
	qpair->cpl = NULL;
}

void
nvme_io_qpair_destroy(struct nvme_qpair *qpair)
{
	struct nvme_controller *ctrlr = qpair->ctrlr;

	if (qpair->num_entries > 0) {

		nvme_ctrlr_cmd_delete_io_sq(ctrlr, qpair, nvme_free_cmd_ring,
		    qpair);
		/* Spin until free_cmd_ring sets qpair->cmd to NULL. */
		while (qpair->cmd)
			DELAY(5);

		nvme_ctrlr_cmd_delete_io_cq(ctrlr, qpair, nvme_free_cpl_ring,
		    qpair);
		/* Spin until free_cpl_ring sets qpair->cmd to NULL. */
		while (qpair->cpl)
			DELAY(5);

		nvme_qpair_destroy(qpair);
	}
}

static void
nvme_abort_complete(void *arg, const struct nvme_completion *status)
{
	struct nvme_tracker	*tr = arg;

	/*
	 * If cdw0 == 1, the controller was not able to abort the command
	 *  we requested.  We still need to check the active tracker array,
	 *  to cover race where I/O timed out at same time controller was
	 *  completing the I/O.
	 */
	if (status->cdw0 == 1 && tr->qpair->act_tr[tr->cid] != NULL) {
		/*
		 * An I/O has timed out, and the controller was unable to
		 *  abort it for some reason.  Construct a fake completion
		 *  status, and then complete the I/O's tracker manually.
		 */
		printf("abort command failed, aborting command manually\n");
		nvme_qpair_manual_complete_tracker(tr->qpair, tr,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST, TRUE);
	}
}

static void
nvme_timeout(void *arg)
{
	struct nvme_tracker	*tr = arg;
	struct nvme_qpair	*qpair = tr->qpair;
	struct nvme_controller	*ctrlr = qpair->ctrlr;
	union csts_register	csts;

	csts.raw = nvme_mmio_read_4(ctrlr, csts);
	if (csts.bits.cfs == 1) {
		/*
		 * The controller is reporting fatal status.  Don't bother
		 *  trying to abort the timed out command - proceed
		 *  immediately to a controller-level reset.
		 */
		device_printf(ctrlr->dev,
		    "controller reports fatal status, resetting...\n");
		nvme_ctrlr_reset(ctrlr);
		return;
	}

	nvme_ctrlr_cmd_abort(ctrlr, tr->cid, qpair->id,
	    nvme_abort_complete, tr);
}

void
nvme_qpair_submit_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr)
{
	struct nvme_request *req;

	mtx_assert(&qpair->lock, MA_OWNED);

	req = tr->req;
	req->cmd.cid = tr->cid;
	qpair->act_tr[tr->cid] = tr;

	if (req->timeout > 0)
#if __FreeBSD_version >= 800030
		callout_reset_curcpu(&tr->timer, req->timeout * hz,
		    nvme_timeout, tr);
#else
		callout_reset(&tr->timer, req->timeout * hz, nvme_timeout, tr);
#endif

	/* Copy the command from the tracker to the submission queue. */
	memcpy(&qpair->cmd[qpair->sq_tail], &req->cmd, sizeof(req->cmd));

	if (++qpair->sq_tail == qpair->num_entries)
		qpair->sq_tail = 0;

	wmb();
	nvme_mmio_write_4(qpair->ctrlr, doorbell[qpair->id].sq_tdbl,
	    qpair->sq_tail);

	qpair->num_cmds++;
}

static void
_nvme_qpair_submit_request(struct nvme_qpair *qpair, struct nvme_request *req)
{
	struct nvme_tracker	*tr;
	int			err;

	mtx_assert(&qpair->lock, MA_OWNED);

	tr = TAILQ_FIRST(&qpair->free_tr);

	if (tr == NULL || !qpair->is_enabled) {
		/*
		 * No tracker is available, or the qpair is disabled due to
		 *  an in-progress controller-level reset.
		 *
		 * Put the request on the qpair's request queue to be processed
		 *  when a tracker frees up via a command completion or when
		 *  the controller reset is completed.
		 */
		STAILQ_INSERT_TAIL(&qpair->queued_req, req, stailq);
		return;
	}

	TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
	TAILQ_INSERT_TAIL(&qpair->outstanding_tr, tr, tailq);
	tr->req = req;

	if (req->uio == NULL) {
		if (req->payload_size > 0) {
			err = bus_dmamap_load(tr->qpair->dma_tag,
					      tr->payload_dma_map, req->payload,
					      req->payload_size,
					      nvme_payload_map, tr, 0);
			if (err != 0)
				panic("bus_dmamap_load returned non-zero!\n");
		} else
			nvme_qpair_submit_tracker(tr->qpair, tr);
	} else {
		err = bus_dmamap_load_uio(tr->qpair->dma_tag,
					  tr->payload_dma_map, req->uio,
					  nvme_payload_map_uio, tr, 0);
		if (err != 0)
			panic("bus_dmamap_load returned non-zero!\n");
	}
}

void
nvme_qpair_submit_request(struct nvme_qpair *qpair, struct nvme_request *req)
{

	mtx_lock(&qpair->lock);
	_nvme_qpair_submit_request(qpair, req);
	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_enable(struct nvme_qpair *qpair)
{

	qpair->is_enabled = TRUE;
	qpair->sq_head = qpair->sq_tail = qpair->cq_head = 0;

	/*
	 * First time through the completion queue, HW will set phase
	 *  bit on completions to 1.  So set this to 1 here, indicating
	 *  we're looking for a 1 to know which entries have completed.
	 *  we'll toggle the bit each time when the completion queue
	 *  rolls over.
	 */
	qpair->phase = 1;

	memset(qpair->cmd, 0,
	    qpair->num_entries * sizeof(struct nvme_command));
	memset(qpair->cpl, 0,
	    qpair->num_entries * sizeof(struct nvme_completion));
}

void
nvme_admin_qpair_enable(struct nvme_qpair *qpair)
{

	nvme_qpair_enable(qpair);
}

void
nvme_io_qpair_enable(struct nvme_qpair *qpair)
{
	STAILQ_HEAD(, nvme_request)	temp;
	struct nvme_tracker		*tr;
	struct nvme_request		*req;

	mtx_lock(&qpair->lock);

	nvme_qpair_enable(qpair);

	TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq)
		nvme_qpair_submit_tracker(qpair, tr);

	STAILQ_INIT(&temp);
	STAILQ_SWAP(&qpair->queued_req, &temp, nvme_request);

	while (!STAILQ_EMPTY(&temp)) {
		req = STAILQ_FIRST(&temp);
		STAILQ_REMOVE_HEAD(&temp, stailq);
		_nvme_qpair_submit_request(qpair, req);
	}

	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_disable(struct nvme_qpair *qpair)
{
	struct nvme_tracker *tr;

	qpair->is_enabled = FALSE;
	mtx_lock(&qpair->lock);
	TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq)
		callout_stop(&tr->timer);
	mtx_unlock(&qpair->lock);
}

void
nvme_admin_qpair_disable(struct nvme_qpair *qpair)
{

	nvme_qpair_disable(qpair);
	nvme_admin_qpair_abort_aers(qpair);
}

void
nvme_io_qpair_disable(struct nvme_qpair *qpair)
{

	nvme_qpair_disable(qpair);
}
