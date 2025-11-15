/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2014 Intel Corporation
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/proc.h>
#include <sys/sbuf.h>

#include <dev/pci/pcivar.h>

#include "nvme_private.h"

typedef enum error_print { ERROR_PRINT_NONE, ERROR_PRINT_NO_RETRY, ERROR_PRINT_ALL } error_print_t;
#define DO_NOT_RETRY	1

static void	_nvme_qpair_submit_request(struct nvme_qpair *qpair,
					   struct nvme_request *req);
static void	nvme_qpair_destroy(struct nvme_qpair *qpair);

static const char *
get_opcode_string(bool admin, uint8_t opc, char *buf, size_t len)
{
	struct sbuf sb;

	sbuf_new(&sb, buf, len, SBUF_FIXEDLEN);
	nvme_opcode_sbuf(admin, opc, &sb);
	if (sbuf_finish(&sb) != 0)
		return ("");
	return (buf);
}

static void
nvme_admin_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{
	char buf[64];

	nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%x "
	    "cdw10:%08x cdw11:%08x\n",
	    get_opcode_string(true, cmd->opc, buf, sizeof(buf)), qpair->id,
	    cmd->cid, le32toh(cmd->nsid), le32toh(cmd->cdw10),
	    le32toh(cmd->cdw11));
}

static void
nvme_io_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{
	char buf[64];

	switch (cmd->opc) {
	case NVME_OPC_WRITE:
	case NVME_OPC_READ:
	case NVME_OPC_WRITE_UNCORRECTABLE:
	case NVME_OPC_COMPARE:
	case NVME_OPC_WRITE_ZEROES:
	case NVME_OPC_VERIFY:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d "
		    "lba:%llu len:%d\n",
		    get_opcode_string(false, cmd->opc, buf, sizeof(buf)),
		    qpair->id, cmd->cid, le32toh(cmd->nsid),
		    ((unsigned long long)le32toh(cmd->cdw11) << 32) + le32toh(cmd->cdw10),
		    (le32toh(cmd->cdw12) & 0xFFFF) + 1);
		break;
	default:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d\n",
		    get_opcode_string(false, cmd->opc, buf, sizeof(buf)),
		    qpair->id, cmd->cid, le32toh(cmd->nsid));
		break;
	}
}

void
nvme_qpair_print_command(struct nvme_qpair *qpair, struct nvme_command *cmd)
{
	if (qpair->id == 0)
		nvme_admin_qpair_print_command(qpair, cmd);
	else
		nvme_io_qpair_print_command(qpair, cmd);
	if (nvme_verbose_cmd_dump) {
		nvme_printf(qpair->ctrlr,
		    "nsid:%#x rsvd2:%#x rsvd3:%#x mptr:%#jx prp1:%#jx prp2:%#jx\n",
		    cmd->nsid, cmd->rsvd2, cmd->rsvd3, (uintmax_t)cmd->mptr,
		    (uintmax_t)cmd->prp1, (uintmax_t)cmd->prp2);
		nvme_printf(qpair->ctrlr,
		    "cdw10: %#x cdw11:%#x cdw12:%#x cdw13:%#x cdw14:%#x cdw15:%#x\n",
		    cmd->cdw10, cmd->cdw11, cmd->cdw12, cmd->cdw13, cmd->cdw14,
		    cmd->cdw15);
	}
}

static const char *
get_status_string(const struct nvme_completion *cpl, char *buf, size_t len)
{
	struct sbuf sb;

	sbuf_new(&sb, buf, len, SBUF_FIXEDLEN);
	nvme_sc_sbuf(cpl, &sb);
	if (sbuf_finish(&sb) != 0)
		return ("");
	return (buf);
}

void
nvme_qpair_print_completion(struct nvme_qpair *qpair,
    struct nvme_completion *cpl)
{
	char buf[64];
	uint8_t crd, m, dnr, p;

	crd = NVME_STATUS_GET_CRD(cpl->status);
	m = NVME_STATUS_GET_M(cpl->status);
	dnr = NVME_STATUS_GET_DNR(cpl->status);
	p = NVME_STATUS_GET_P(cpl->status);

	nvme_printf(qpair->ctrlr, "%s crd:%x m:%x dnr:%x p:%d "
	    "sqid:%d cid:%d cdw0:%x\n",
	    get_status_string(cpl, buf, sizeof(buf)), crd, m, dnr, p,
	    cpl->sqid, cpl->cid, cpl->cdw0);
}

static bool
nvme_completion_is_retry(const struct nvme_completion *cpl)
{
	uint8_t sct, sc, dnr;

	sct = NVME_STATUS_GET_SCT(cpl->status);
	sc = NVME_STATUS_GET_SC(cpl->status);
	dnr = NVME_STATUS_GET_DNR(cpl->status);	/* Do Not Retry Bit */

	/*
	 * TODO: spec is not clear how commands that are aborted due
	 *  to TLER will be marked.  So for now, it seems
	 *  NAMESPACE_NOT_READY is the only case where we should
	 *  look at the DNR bit. Requests failed with ABORTED_BY_REQUEST
	 *  set the DNR bit correctly since the driver controls that.
	 */
	switch (sct) {
	case NVME_SCT_GENERIC:
		switch (sc) {
		case NVME_SC_ABORTED_BY_REQUEST:
		case NVME_SC_NAMESPACE_NOT_READY:
			if (dnr)
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
		return (0);
	case NVME_SCT_PATH_RELATED:
		switch (sc) {
		case NVME_SC_INTERNAL_PATH_ERROR:
			if (dnr)
				return (0);
			else
				return (1);
		default:
			return (0);
		}
	case NVME_SCT_VENDOR_SPECIFIC:
	default:
		return (0);
	}
}

static void
nvme_qpair_complete_tracker(struct nvme_tracker *tr,
    struct nvme_completion *cpl, error_print_t print_on_error)
{
	struct nvme_qpair	*qpair = tr->qpair;
	struct nvme_request	*req;
	bool			retry, error, retriable;

	mtx_assert(&qpair->lock, MA_NOTOWNED);

	req = tr->req;
	error = nvme_completion_is_error(cpl);
	retriable = nvme_completion_is_retry(cpl);
	retry = error && retriable && req->retries < nvme_retry_count;
	if (retry)
		qpair->num_retries++;
	if (error && req->retries >= nvme_retry_count && retriable)
		qpair->num_failures++;

	if (error && (print_on_error == ERROR_PRINT_ALL ||
		(!retry && print_on_error == ERROR_PRINT_NO_RETRY))) {
		nvme_qpair_print_command(qpair, &req->cmd);
		nvme_qpair_print_completion(qpair, cpl);
	}

	qpair->act_tr[cpl->cid] = NULL;

	KASSERT(cpl->cid == req->cmd.cid, ("cpl cid does not match cmd cid\n"));

	if (!retry) {
		if (req->payload_valid) {
			bus_dmamap_sync(qpair->dma_tag_payload,
			    tr->payload_dma_map,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		}
		if (req->cb_fn)
			req->cb_fn(req->cb_arg, cpl);
	}

	mtx_lock(&qpair->lock);

	if (retry) {
		req->retries++;
		nvme_qpair_submit_tracker(qpair, tr);
	} else {
		if (req->payload_valid) {
			bus_dmamap_unload(qpair->dma_tag_payload,
			    tr->payload_dma_map);
		}

		nvme_free_request(req);
		tr->req = NULL;

		TAILQ_REMOVE(&qpair->outstanding_tr, tr, tailq);
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);

		/*
		 * If the controller is in the middle of resetting, don't
		 *  try to submit queued requests here - let the reset logic
		 *  handle that instead.
		 */
		if (!STAILQ_EMPTY(&qpair->queued_req) &&
		    !qpair->ctrlr->is_resetting) {
			req = STAILQ_FIRST(&qpair->queued_req);
			STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
			_nvme_qpair_submit_request(qpair, req);
		}
	}

	mtx_unlock(&qpair->lock);
}

static uint32_t
nvme_qpair_make_status(uint32_t sct, uint32_t sc, uint32_t dnr)
{
	uint32_t status = 0;

	status |= NVMEF(NVME_STATUS_SCT, sct);
	status |= NVMEF(NVME_STATUS_SC, sc);
	status |= NVMEF(NVME_STATUS_DNR, dnr);
	/* M=0 : this is artificial so no data in error log page */
	/* CRD=0 : this is artificial and no delayed retry support anyway */
	/* P=0 : phase not checked */
	return (status);
}

static void
nvme_qpair_manual_complete_tracker(
    struct nvme_tracker *tr, uint32_t sct, uint32_t sc, uint32_t dnr,
    error_print_t print_on_error)
{
	struct nvme_completion	cpl;
	struct nvme_qpair * qpair = tr->qpair;

	mtx_assert(&qpair->lock, MA_NOTOWNED);

	memset(&cpl, 0, sizeof(cpl));

	cpl.sqid = qpair->id;
	cpl.cid = tr->cid;
	cpl.status = nvme_qpair_make_status(sct, sc, dnr);
	nvme_qpair_complete_tracker(tr, &cpl, print_on_error);
}

static void
nvme_qpair_manual_complete_request(struct nvme_qpair *qpair,
    struct nvme_request *req, uint32_t sct, uint32_t sc, uint32_t dnr,
    error_print_t print_on_error)
{
	struct nvme_completion	cpl;
	bool			error;

	memset(&cpl, 0, sizeof(cpl));
	cpl.sqid = qpair->id;
	cpl.status = nvme_qpair_make_status(sct, sc, dnr);
	error = nvme_completion_is_error(&cpl);

	if (error && print_on_error == ERROR_PRINT_ALL) {
		nvme_qpair_print_command(qpair, &req->cmd);
		nvme_qpair_print_completion(qpair, &cpl);
	}

	if (req->cb_fn)
		req->cb_fn(req->cb_arg, &cpl);

	nvme_free_request(req);
}

/* Locked version of completion processor */
static bool
_nvme_qpair_process_completions(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;
	struct nvme_completion	cpl;
	bool done = false;
	bool in_panic = dumping || SCHEDULER_STOPPED();

	mtx_assert(&qpair->recovery, MA_OWNED);

	/*
	 * qpair is not enabled, likely because a controller reset is in
	 * progress.  Ignore the interrupt - any I/O that was associated with
	 * this interrupt will get retried when the reset is complete. Any
	 * pending completions for when we're in startup will be completed
	 * as soon as initialization is complete and we start sending commands
	 * to the device.
	 */
	if (qpair->recovery_state != RECOVERY_NONE) {
		qpair->num_ignored++;
		return (false);
	}

	/*
	 * Sanity check initialization. After we reset the hardware, the phase
	 * is defined to be 1. So if we get here with zero prior calls and the
	 * phase is 0, it means that we've lost a race between the
	 * initialization and the ISR running. With the phase wrong, we'll
	 * process a bunch of completions that aren't really completions leading
	 * to a KASSERT below.
	 */
	KASSERT(!(qpair->num_intr_handler_calls == 0 && qpair->phase == 0),
	    ("%s: Phase wrong for first interrupt call.",
		device_get_nameunit(qpair->ctrlr->dev)));

	qpair->num_intr_handler_calls++;

	bus_dmamap_sync(qpair->dma_tag, qpair->queuemem_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	/*
	 * A panic can stop the CPU this routine is running on at any point.  If
	 * we're called during a panic, complete the sq_head wrap protocol for
	 * the case where we are interrupted just after the increment at 1
	 * below, but before we can reset cq_head to zero at 2. Also cope with
	 * the case where we do the zero at 2, but may or may not have done the
	 * phase adjustment at step 3. The panic machinery flushes all pending
	 * memory writes, so we can make these strong ordering assumptions
	 * that would otherwise be unwise if we were racing in real time.
	 */
	if (__predict_false(in_panic)) {
		if (qpair->cq_head == qpair->num_entries) {
			/*
			 * Here we know that we need to zero cq_head and then negate
			 * the phase, which hasn't been assigned if cq_head isn't
			 * zero due to the atomic_store_rel.
			 */
			qpair->cq_head = 0;
			qpair->phase = !qpair->phase;
		} else if (qpair->cq_head == 0) {
			/*
			 * In this case, we know that the assignment at 2
			 * happened below, but we don't know if it 3 happened or
			 * not. To do this, we look at the last completion
			 * entry and set the phase to the opposite phase
			 * that it has. This gets us back in sync
			 */
			cpl = qpair->cpl[qpair->num_entries - 1];
			nvme_completion_swapbytes(&cpl);
			qpair->phase = !NVME_STATUS_GET_P(cpl.status);
		}
	}

	while (1) {
		uint16_t status;

		/*
		 * We need to do this dance to avoid a race between the host and
		 * the device where the device overtakes the host while the host
		 * is reading this record, leaving the status field 'new' and
		 * the sqhd and cid fields potentially stale. If the phase
		 * doesn't match, that means status hasn't yet been updated and
		 * we'll get any pending changes next time. It also means that
		 * the phase must be the same the second time. We have to sync
		 * before reading to ensure any bouncing completes.
		 */
		status = le16toh(qpair->cpl[qpair->cq_head].status);
		if (NVME_STATUS_GET_P(status) != qpair->phase)
			break;

		bus_dmamap_sync(qpair->dma_tag, qpair->queuemem_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		cpl = qpair->cpl[qpair->cq_head];
		nvme_completion_swapbytes(&cpl);

		KASSERT(
		    NVME_STATUS_GET_P(status) == NVME_STATUS_GET_P(cpl.status),
		    ("Phase unexpectedly inconsistent"));

		if (cpl.cid < qpair->num_trackers)
			tr = qpair->act_tr[cpl.cid];
		else
			tr = NULL;

		done = true;
		if (tr != NULL) {
			nvme_qpair_complete_tracker(tr, &cpl, ERROR_PRINT_ALL);
			qpair->sq_head = cpl.sqhd;
		} else if (!in_panic) {
			/*
			 * A missing tracker is normally an error.  However, a
			 * panic can stop the CPU this routine is running on
			 * after completing an I/O but before updating
			 * qpair->cq_head at 1 below.  Later, we re-enter this
			 * routine to poll I/O associated with the kernel
			 * dump. We find that the tr has been set to null before
			 * calling the completion routine.  If it hasn't
			 * completed (or it triggers a panic), then '1' below
			 * won't have updated cq_head. Rather than panic again,
			 * ignore this condition because it's not unexpected.
			 */
			nvme_printf(qpair->ctrlr,
			    "cpl (cid = %u) does not map to outstanding cmd\n",
				cpl.cid);
			nvme_qpair_print_completion(qpair,
			    &qpair->cpl[qpair->cq_head]);
			KASSERT(0, ("received completion for unknown cmd"));
		}

		/*
		 * There's a number of races with the following (see above) when
		 * the system panics. We compensate for each one of them by
		 * using the atomic store to force strong ordering (at least when
		 * viewed in the aftermath of a panic).
		 */
		if (++qpair->cq_head == qpair->num_entries) {		/* 1 */
			atomic_store_rel_int(&qpair->cq_head, 0);	/* 2 */
			qpair->phase = !qpair->phase;			/* 3 */
		}
	}

	if (done) {
		bus_space_write_4(qpair->ctrlr->bus_tag, qpair->ctrlr->bus_handle,
		    qpair->cq_hdbl_off, qpair->cq_head);
	}

	return (done);
}

bool
nvme_qpair_process_completions(struct nvme_qpair *qpair)
{
	bool done = false;

	/*
	 * Interlock with reset / recovery code. This is an usually uncontended
	 * to make sure that we drain out of the ISRs before we reset the card
	 * and to prevent races with the recovery process called from a timeout
	 * context.
	 */
	mtx_lock(&qpair->recovery);

	if (__predict_true(qpair->recovery_state == RECOVERY_NONE))
		done = _nvme_qpair_process_completions(qpair);
	else
		qpair->num_recovery_nolock++;	// XXX likely need to rename

	mtx_unlock(&qpair->recovery);

	return (done);
}

static void
nvme_qpair_msi_handler(void *arg)
{
	struct nvme_qpair *qpair = arg;

	nvme_qpair_process_completions(qpair);
}

int
nvme_qpair_construct(struct nvme_qpair *qpair,
    uint32_t num_entries, uint32_t num_trackers,
    struct nvme_controller *ctrlr)
{
	struct nvme_tracker	*tr;
	size_t			cmdsz, cplsz, prpsz, allocsz, prpmemsz;
	uint64_t		queuemem_phys, prpmem_phys, list_phys;
	uint8_t			*queuemem, *prpmem, *prp_list;
	int			i, err;

	qpair->vector = ctrlr->msi_count > 1 ? qpair->id : 0;
	qpair->num_entries = num_entries;
	qpair->num_trackers = num_trackers;
	qpair->ctrlr = ctrlr;

	mtx_init(&qpair->lock, "nvme qpair lock", NULL, MTX_DEF);
	mtx_init(&qpair->recovery, "nvme qpair recovery", NULL, MTX_DEF);

	callout_init_mtx(&qpair->timer, &qpair->recovery, 0);
	qpair->timer_armed = false;
	qpair->recovery_state = RECOVERY_WAITING;

	/* Note: NVMe PRP format is restricted to 4-byte alignment. */
	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    4, ctrlr->page_size, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, ctrlr->max_xfer_size,
	    howmany(ctrlr->max_xfer_size, ctrlr->page_size) + 1,
	    ctrlr->page_size, 0,
	    NULL, NULL, &qpair->dma_tag_payload);
	if (err != 0) {
		nvme_printf(ctrlr, "payload tag create failed %d\n", err);
		goto out;
	}

	/*
	 * Each component must be page aligned, and individual PRP lists
	 * cannot cross a page boundary.
	 */
	cmdsz = qpair->num_entries * sizeof(struct nvme_command);
	cmdsz = roundup2(cmdsz, ctrlr->page_size);
	cplsz = qpair->num_entries * sizeof(struct nvme_completion);
	cplsz = roundup2(cplsz, ctrlr->page_size);
	/*
	 * For commands requiring more than 2 PRP entries, one PRP will be
	 * embedded in the command (prp1), and the rest of the PRP entries
	 * will be in a list pointed to by the command (prp2).
	 */
	prpsz = sizeof(uint64_t) *
	    howmany(ctrlr->max_xfer_size, ctrlr->page_size);
	prpmemsz = qpair->num_trackers * prpsz;
	allocsz = cmdsz + cplsz + prpmemsz;

	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    ctrlr->page_size, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    allocsz, 1, allocsz, 0, NULL, NULL, &qpair->dma_tag);
	if (err != 0) {
		nvme_printf(ctrlr, "tag create failed %d\n", err);
		goto out;
	}
	bus_dma_tag_set_domain(qpair->dma_tag, qpair->domain);

	if (bus_dmamem_alloc(qpair->dma_tag, (void **)&queuemem,
	     BUS_DMA_COHERENT | BUS_DMA_NOWAIT, &qpair->queuemem_map)) {
		nvme_printf(ctrlr, "failed to alloc qpair memory\n");
		goto out;
	}

	if (bus_dmamap_load(qpair->dma_tag, qpair->queuemem_map,
	    queuemem, allocsz, nvme_single_map, &queuemem_phys, 0) != 0) {
		nvme_printf(ctrlr, "failed to load qpair memory\n");
		bus_dmamem_free(qpair->dma_tag, qpair->cmd,
		    qpair->queuemem_map);
		goto out;
	}

	qpair->num_cmds = 0;
	qpair->num_intr_handler_calls = 0;
	qpair->num_retries = 0;
	qpair->num_failures = 0;
	qpair->num_ignored = 0;
	qpair->cmd = (struct nvme_command *)queuemem;
	qpair->cpl = (struct nvme_completion *)(queuemem + cmdsz);
	prpmem = (uint8_t *)(queuemem + cmdsz + cplsz);
	qpair->cmd_bus_addr = queuemem_phys;
	qpair->cpl_bus_addr = queuemem_phys + cmdsz;
	prpmem_phys = queuemem_phys + cmdsz + cplsz;

	/*
	 * Calcuate the stride of the doorbell register. Many emulators set this
	 * value to correspond to a cache line. However, some hardware has set
	 * it to various small values.
	 */
	qpair->sq_tdbl_off = nvme_mmio_offsetof(doorbell[0]) +
	    (qpair->id << (ctrlr->dstrd + 1));
	qpair->cq_hdbl_off = nvme_mmio_offsetof(doorbell[0]) +
	    (qpair->id << (ctrlr->dstrd + 1)) + (1 << ctrlr->dstrd);

	TAILQ_INIT(&qpair->free_tr);
	TAILQ_INIT(&qpair->outstanding_tr);
	STAILQ_INIT(&qpair->queued_req);

	list_phys = prpmem_phys;
	prp_list = prpmem;
	for (i = 0; i < qpair->num_trackers; i++) {
		if (list_phys + prpsz > prpmem_phys + prpmemsz) {
			qpair->num_trackers = i;
			break;
		}

		/*
		 * Make sure that the PRP list for this tracker doesn't
		 * overflow to another nvme page.
		 */
		if (trunc_page(list_phys) !=
		    trunc_page(list_phys + prpsz - 1)) {
			list_phys = roundup2(list_phys, ctrlr->page_size);
			prp_list =
			    (uint8_t *)roundup2((uintptr_t)prp_list, ctrlr->page_size);
		}

		tr = malloc_domainset(sizeof(*tr), M_NVME,
		    DOMAINSET_PREF(qpair->domain), M_ZERO | M_WAITOK);
		bus_dmamap_create(qpair->dma_tag_payload, 0,
		    &tr->payload_dma_map);
		tr->cid = i;
		tr->qpair = qpair;
		tr->prp = (uint64_t *)prp_list;
		tr->prp_bus_addr = list_phys;
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);
		list_phys += prpsz;
		prp_list += prpsz;
	}

	if (qpair->num_trackers == 0) {
		nvme_printf(ctrlr, "failed to allocate enough trackers\n");
		goto out;
	}

	qpair->act_tr = malloc_domainset(sizeof(struct nvme_tracker *) *
	    qpair->num_entries, M_NVME, DOMAINSET_PREF(qpair->domain),
	    M_ZERO | M_WAITOK);

	if (ctrlr->msi_count > 1) {
		/*
		 * MSI-X vector resource IDs start at 1, so we add one to
		 *  the queue's vector to get the corresponding rid to use.
		 */
		qpair->rid = qpair->vector + 1;

		qpair->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
		    &qpair->rid, RF_ACTIVE);
		if (qpair->res == NULL) {
			nvme_printf(ctrlr, "unable to allocate MSI\n");
			goto out;
		}
		if (bus_setup_intr(ctrlr->dev, qpair->res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL,
		    nvme_qpair_msi_handler, qpair, &qpair->tag) != 0) {
			nvme_printf(ctrlr, "unable to setup MSI\n");
			goto out;
		}
		if (qpair->id == 0) {
			bus_describe_intr(ctrlr->dev, qpair->res, qpair->tag,
			    "admin");
		} else {
			bus_describe_intr(ctrlr->dev, qpair->res, qpair->tag,
			    "io%d", qpair->id - 1);
		}
	}

	return (0);

out:
	nvme_qpair_destroy(qpair);
	return (ENOMEM);
}

static void
nvme_qpair_destroy(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	mtx_lock(&qpair->recovery);
	qpair->timer_armed = false;
	mtx_unlock(&qpair->recovery);
	callout_drain(&qpair->timer);

	if (qpair->tag) {
		bus_teardown_intr(qpair->ctrlr->dev, qpair->res, qpair->tag);
		qpair->tag = NULL;
	}

	if (qpair->act_tr) {
		free(qpair->act_tr, M_NVME);
		qpair->act_tr = NULL;
	}

	while (!TAILQ_EMPTY(&qpair->free_tr)) {
		tr = TAILQ_FIRST(&qpair->free_tr);
		TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
		bus_dmamap_destroy(qpair->dma_tag_payload,
		    tr->payload_dma_map);
		free(tr, M_NVME);
	}

	if (qpair->cmd != NULL) {
		bus_dmamap_unload(qpair->dma_tag, qpair->queuemem_map);
		bus_dmamem_free(qpair->dma_tag, qpair->cmd,
		    qpair->queuemem_map);
		qpair->cmd = NULL;
	}

	if (qpair->dma_tag) {
		bus_dma_tag_destroy(qpair->dma_tag);
		qpair->dma_tag = NULL;
	}

	if (qpair->dma_tag_payload) {
		bus_dma_tag_destroy(qpair->dma_tag_payload);
		qpair->dma_tag_payload = NULL;
	}

	if (mtx_initialized(&qpair->lock))
		mtx_destroy(&qpair->lock);
	if (mtx_initialized(&qpair->recovery))
		mtx_destroy(&qpair->recovery);

	if (qpair->res) {
		bus_release_resource(qpair->ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(qpair->res), qpair->res);
		qpair->res = NULL;
	}
}

static void
nvme_admin_qpair_abort_aers(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	/*
	 * nvme_complete_tracker must be called without the qpair lock held. It
	 * takes the lock to adjust outstanding_tr list, so make sure we don't
	 * have it yet. We need the lock to make the list traverse safe, but
	 * have to drop the lock to complete any AER. We restart the list scan
	 * when we do this to make this safe. There's interlock with the ISR so
	 * we know this tracker won't be completed twice.
	 */
	mtx_assert(&qpair->lock, MA_NOTOWNED);

	mtx_lock(&qpair->lock);
	tr = TAILQ_FIRST(&qpair->outstanding_tr);
	while (tr != NULL) {
		if (tr->req->cmd.opc != NVME_OPC_ASYNC_EVENT_REQUEST) {
			tr = TAILQ_NEXT(tr, tailq);
			continue;
		}
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(tr,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_SQ_DELETION, 0,
		    ERROR_PRINT_NONE);
		mtx_lock(&qpair->lock);
		tr = TAILQ_FIRST(&qpair->outstanding_tr);
	}
	mtx_unlock(&qpair->lock);
}

void
nvme_admin_qpair_destroy(struct nvme_qpair *qpair)
{
	mtx_assert(&qpair->lock, MA_NOTOWNED);

	nvme_admin_qpair_abort_aers(qpair);
	nvme_qpair_destroy(qpair);
}

void
nvme_io_qpair_destroy(struct nvme_qpair *qpair)
{
	nvme_qpair_destroy(qpair);
}

static void
nvme_abort_complete(void *arg, const struct nvme_completion *status)
{
	struct nvme_tracker     *tr = arg;

	/*
	 * If cdw0 bit 0 == 1, the controller was not able to abort the command
	 * we requested.  We still need to check the active tracker array, to
	 * cover race where I/O timed out at same time controller was completing
	 * the I/O. An abort command always is on the admin queue, but affects
	 * either an admin or an I/O queue, so take the appropriate qpair lock
	 * for the original command's queue, since we'll need it to avoid races
	 * with the completion code and to complete the command manually.
	 */
	mtx_lock(&tr->qpair->lock);
	if ((status->cdw0 & 1) == 1 && tr->qpair->act_tr[tr->cid] != NULL) {
		/*
		 * An I/O has timed out, and the controller was unable to abort
		 * it for some reason.  And we've not processed a completion for
		 * it yet. Construct a fake completion status, and then complete
		 * the I/O's tracker manually.
		 */
		nvme_printf(tr->qpair->ctrlr,
		    "abort command failed, aborting command manually\n");
		nvme_qpair_manual_complete_tracker(tr,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST, 0, ERROR_PRINT_ALL);
	}
	/*
	 * XXX We don't check status for the possible 'Could not abort because
	 * excess aborts were submitted to the controller'. We don't prevent
	 * that, either. Document for the future here, since the standard is
	 * squishy and only says 'may generate' but implies anything is possible
	 * including hangs if you exceed the ACL.
	 */
	mtx_unlock(&tr->qpair->lock);
}

static void
nvme_qpair_timeout(void *arg)
{
	struct nvme_qpair	*qpair = arg;
	struct nvme_controller	*ctrlr = qpair->ctrlr;
	struct nvme_tracker	*tr;
	sbintime_t		now;
	bool			idle = true;
	bool			is_admin = qpair == &ctrlr->adminq;
	bool			fast;
	uint32_t		csts;
	uint8_t			cfs;

	mtx_assert(&qpair->recovery, MA_OWNED);

	/*
	 * If the controller is failed, then stop polling. This ensures that any
	 * failure processing that races with the qpair timeout will fail
	 * safely.
	 */
	if (is_admin ? qpair->ctrlr->is_failed_admin : qpair->ctrlr->is_failed) {
		nvme_printf(qpair->ctrlr,
		    "%sFailed controller, stopping watchdog timeout.\n",
		    is_admin ? "Complete " : "");
		qpair->timer_armed = false;
		return;
	}

	/*
	 * Shutdown condition: We set qpair->timer_armed to false in
	 * nvme_qpair_destroy before calling callout_drain. When we call that,
	 * this routine might get called one last time. Exit w/o setting a
	 * timeout. None of the watchdog stuff needs to be done since we're
	 * destroying the qpair.
	 */
	if (!qpair->timer_armed) {
		nvme_printf(qpair->ctrlr,
		    "Timeout fired during nvme_qpair_destroy\n");
		return;
	}

	switch (qpair->recovery_state) {
	case RECOVERY_NONE:
		/*
		 * Read csts to get value of cfs - controller fatal status.  If
		 * we are in the hot-plug or controller failed status proceed
		 * directly to reset. We also bail early if the status reads all
		 * 1's or the control fatal status bit is now 1. The latter is
		 * always true when the former is true, but not vice versa.  The
		 * intent of the code is that if the card is gone (all 1's) or
		 * we've failed, then try to do a reset (which someitmes
		 * unwedges a card reading all 1's that's not gone away, but
		 * usually doesn't).
		 */
		csts = nvme_mmio_read_4(ctrlr, csts);
		cfs = NVMEV(NVME_CSTS_REG_CFS, csts);
		if (csts == NVME_GONE || cfs == 1) {
			/*
			 * We've had a command timeout that we weren't able to
			 * abort or we have aborts disabled and any command
			 * timed out.
			 *
			 * If we get here due to a possible surprise hot-unplug
			 * event, then we let nvme_ctrlr_reset confirm and fail
			 * the controller.
			 */
do_reset:
			nvme_printf(ctrlr, "Resetting controller due to a timeout%s.\n",
			    (csts == 0xffffffff) ? " and possible hot unplug" :
			    (cfs ? " and fatal error status" : ""));
			qpair->recovery_state = RECOVERY_WAITING;
			nvme_ctrlr_reset(ctrlr);
			idle = false;
			break;
		}


		/*
		 * See if there's any recovery needed. First, do a fast check to
		 * see if anything could have timed out. If not, then skip
		 * everything else.
		 */
		fast = false;
		mtx_lock(&qpair->lock);
		now = getsbinuptime();
		TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq) {
			/*
			 * Skip async commands, they are posted to the card for
			 * an indefinite amount of time and have no deadline.
			 */
			if (tr->deadline == SBT_MAX)
				continue;

			/*
			 * If the first real transaction is not in timeout, then
			 * we're done. Otherwise, we try recovery.
			 */
			idle = false;
			if (now <= tr->deadline)
				fast = true;
			break;
		}
		mtx_unlock(&qpair->lock);
		if (idle || fast)
			break;

		/*
		 * There's a stale transaction at the start of the queue whose
		 * deadline has passed. Poll the competions as a last-ditch
		 * effort in case an interrupt has been missed. Warn the user if
		 * transactions were found of possible interrupt issues, but
		 * just once per controller.
		 */
		if (_nvme_qpair_process_completions(qpair) && !ctrlr->isr_warned) {
			nvme_printf(ctrlr, "System interrupt issues?\n");
			ctrlr->isr_warned = true;
		}

		/*
		 * Now that we've run the ISR, re-rheck to see if there's any
		 * timed out commands and abort them or reset the card if so.
		 */
		mtx_lock(&qpair->lock);
		idle = true;
		TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq) {
			/*
			 * Skip async commands, they are posted to the card for
			 * an indefinite amount of time and have no deadline.
			 */
			if (tr->deadline == SBT_MAX)
				continue;

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
			 * Timeout expired, abort it or reset controller.
			 */
			if (ctrlr->enable_aborts &&
			    tr->req->cb_fn != nvme_abort_complete) {
				/*
				 * This isn't an abort command, ask for a
				 * hardware abort. This goes to the admin
				 * queue which will reset the card if it
				 * times out.
				 */
				nvme_ctrlr_cmd_abort(ctrlr, tr->cid, qpair->id,
				    nvme_abort_complete, tr);
			} else {
				/*
				 * We have a live command in the card (either
				 * one we couldn't abort, or aborts weren't
				 * enabled).  We can only reset.
				 */
				mtx_unlock(&qpair->lock);
				goto do_reset;
			}
		}
		mtx_unlock(&qpair->lock);
		break;

	case RECOVERY_WAITING:
		/*
		 * These messages aren't interesting while we're suspended. We
		 * put the queues into waiting state while
		 * suspending. Suspending takes a while, so we'll see these
		 * during that time and they aren't diagnostic. At other times,
		 * they indicate a problem that's worth complaining about.
		 */
		if (!device_is_suspended(ctrlr->dev))
			nvme_printf(ctrlr, "Waiting for reset to complete\n");
		idle = false;		/* We want to keep polling */
		break;
	}

	/*
	 * Rearm the timeout.
	 */
	if (!idle) {
		callout_schedule_sbt(&qpair->timer, SBT_1S / 2, SBT_1S / 2, 0);
	} else {
		qpair->timer_armed = false;
	}
}

/*
 * Submit the tracker to the hardware. Must already be in the
 * outstanding queue when called.
 */
void
nvme_qpair_submit_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr)
{
	struct nvme_request	*req;
	struct nvme_controller	*ctrlr;
	int timeout;

	mtx_assert(&qpair->lock, MA_OWNED);

	req = tr->req;
	req->cmd.cid = tr->cid;
	qpair->act_tr[tr->cid] = tr;
	ctrlr = qpair->ctrlr;

	if (req->timeout) {
		if (req->cb_fn == nvme_completion_poll_cb)
			timeout = 1;
		else if (qpair->id == 0)
			timeout = ctrlr->admin_timeout_period;
		else
			timeout = ctrlr->timeout_period;
		tr->deadline = getsbinuptime() + timeout * SBT_1S;
		if (!qpair->timer_armed) {
			qpair->timer_armed = true;
			callout_reset_sbt_on(&qpair->timer, SBT_1S / 2, SBT_1S / 2,
			    nvme_qpair_timeout, qpair, qpair->cpu, 0);
		}
	} else
		tr->deadline = SBT_MAX;

	/* Copy the command from the tracker to the submission queue. */
	memcpy(&qpair->cmd[qpair->sq_tail], &req->cmd, sizeof(req->cmd));

	if (++qpair->sq_tail == qpair->num_entries)
		qpair->sq_tail = 0;

	bus_dmamap_sync(qpair->dma_tag, qpair->queuemem_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_space_write_4(ctrlr->bus_tag, ctrlr->bus_handle,
	    qpair->sq_tdbl_off, qpair->sq_tail);
	qpair->num_cmds++;
}

static void
nvme_payload_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct nvme_tracker 	*tr = arg;
	uint32_t		cur_nseg;

	/*
	 * If the mapping operation failed, return immediately.  The caller
	 *  is responsible for detecting the error status and failing the
	 *  tracker manually.
	 */
	if (error != 0) {
		nvme_printf(tr->qpair->ctrlr,
		    "nvme_payload_map err %d\n", error);
		return;
	}

	/*
	 * Note that we specified ctrlr->page_size for alignment and max
	 * segment size when creating the bus dma tags.  So here we can safely
	 * just transfer each segment to its associated PRP entry.
	 */
	tr->req->cmd.prp1 = htole64(seg[0].ds_addr);

	if (nseg == 2) {
		tr->req->cmd.prp2 = htole64(seg[1].ds_addr);
	} else if (nseg > 2) {
		cur_nseg = 1;
		tr->req->cmd.prp2 = htole64((uint64_t)tr->prp_bus_addr);
		while (cur_nseg < nseg) {
			tr->prp[cur_nseg-1] =
			    htole64((uint64_t)seg[cur_nseg].ds_addr);
			cur_nseg++;
		}
	} else {
		/*
		 * prp2 should not be used by the controller
		 *  since there is only one segment, but set
		 *  to 0 just to be safe.
		 */
		tr->req->cmd.prp2 = 0;
	}

	bus_dmamap_sync(tr->qpair->dma_tag_payload, tr->payload_dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	nvme_qpair_submit_tracker(tr->qpair, tr);
}

static void
_nvme_qpair_submit_request(struct nvme_qpair *qpair, struct nvme_request *req)
{
	struct nvme_tracker	*tr;
	int			err = 0;
	bool			is_admin = qpair == &qpair->ctrlr->adminq;

	mtx_assert(&qpair->lock, MA_OWNED);

	tr = TAILQ_FIRST(&qpair->free_tr);
	req->qpair = qpair;

	/*
	 * The controller has failed, so fail the request. Note, that this races
	 * the recovery / timeout code. Since we hold the qpair lock, we know
	 * it's safe to fail directly. is_failed is set when we fail the
	 * controller.  It is only ever reset in the ioctl reset controller
	 * path, which is safe to race (for failed controllers, we make no
	 * guarantees about bringing it out of failed state relative to other
	 * commands). We try hard to allow admin commands when the entire
	 * controller hasn't failed, only something related to I/O queues.
	 */
	if (is_admin ? qpair->ctrlr->is_failed_admin : qpair->ctrlr->is_failed) {
		nvme_qpair_manual_complete_request(qpair, req,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST, 1,
		    ERROR_PRINT_NONE);
		return;
	}

	/*
	 * No tracker is available, or the qpair is disabled due to an
	 * in-progress controller-level reset. If we lose the race with
	 * recovery_state, then we may add an extra request to the queue which
	 * will be resubmitted later.  We only set recovery_state to NONE with
	 * qpair->lock also held, so if we observe that the state is not NONE,
	 * we know it won't transition back to NONE without retrying queued
	 * request.
	 */
	if (tr == NULL || qpair->recovery_state != RECOVERY_NONE) {
		STAILQ_INSERT_TAIL(&qpair->queued_req, req, stailq);
		return;
	}

	TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
	TAILQ_INSERT_TAIL(&qpair->outstanding_tr, tr, tailq);
	tr->deadline = SBT_MAX;
	tr->req = req;

	if (!req->payload_valid) {
		nvme_qpair_submit_tracker(tr->qpair, tr);
		return;
	}

	/*
	 * tr->deadline updating when nvme_payload_map calls
	 * nvme_qpair_submit_tracker (we call it above directly
	 * when there's no map to load).
	 */
	err = bus_dmamap_load_mem(tr->qpair->dma_tag_payload,
	    tr->payload_dma_map, &req->payload, nvme_payload_map, tr, 0);
	if (err != 0) {
		/*
		 * The dmamap operation failed, so we manually fail the
		 *  tracker here with DATA_TRANSFER_ERROR status.
		 *
		 * nvme_qpair_manual_complete_tracker must not be called
		 *  with the qpair lock held.
		 */
		nvme_printf(qpair->ctrlr,
		    "bus_dmamap_load_mem returned 0x%x!\n", err);
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_DATA_TRANSFER_ERROR, DO_NOT_RETRY, ERROR_PRINT_ALL);
		mtx_lock(&qpair->lock);
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
	bool is_admin __unused = qpair == &qpair->ctrlr->adminq;

	if (mtx_initialized(&qpair->recovery))
		mtx_assert(&qpair->recovery, MA_OWNED);
	if (mtx_initialized(&qpair->lock))
		mtx_assert(&qpair->lock, MA_OWNED);
	KASSERT(!(is_admin ? qpair->ctrlr->is_failed_admin : qpair->ctrlr->is_failed),
	    ("Enabling a failed qpair\n"));

	qpair->recovery_state = RECOVERY_NONE;
}

void
nvme_qpair_reset(struct nvme_qpair *qpair)
{
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
	struct nvme_tracker		*tr;
	struct nvme_tracker		*tr_temp;
	bool				rpt;

	/*
	 * Manually abort each outstanding admin command.  Do not retry
	 * admin commands found here, since they will be left over from
	 * a controller reset and its likely the context in which the
	 * command was issued no longer applies.
	 */
	rpt = !TAILQ_EMPTY(&qpair->outstanding_tr);
	if (rpt)
		nvme_printf(qpair->ctrlr,
		    "aborting outstanding admin command\n");
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, DO_NOT_RETRY, ERROR_PRINT_ALL);
	}
	if (rpt)
		nvme_printf(qpair->ctrlr,
		    "done aborting outstanding admin\n");

	mtx_lock(&qpair->recovery);
	mtx_lock(&qpair->lock);
	nvme_qpair_enable(qpair);
	mtx_unlock(&qpair->lock);
	mtx_unlock(&qpair->recovery);
}

void
nvme_io_qpair_enable(struct nvme_qpair *qpair)
{
	STAILQ_HEAD(, nvme_request)	temp;
	struct nvme_tracker		*tr;
	struct nvme_tracker		*tr_temp;
	struct nvme_request		*req;
	bool				report;

	/*
	 * Manually abort each outstanding I/O.  This normally results in a
	 * retry, unless the retry count on the associated request has
	 * reached its limit.
	 */
	report = !TAILQ_EMPTY(&qpair->outstanding_tr);
	if (report)
		nvme_printf(qpair->ctrlr, "aborting outstanding i/o\n");
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 0, ERROR_PRINT_NO_RETRY);
	}
	if (report)
		nvme_printf(qpair->ctrlr, "done aborting outstanding i/o\n");

	mtx_lock(&qpair->recovery);
	mtx_lock(&qpair->lock);
	nvme_qpair_enable(qpair);

	STAILQ_INIT(&temp);
	STAILQ_SWAP(&qpair->queued_req, &temp, nvme_request);

	report = !STAILQ_EMPTY(&temp);
	if (report)
		nvme_printf(qpair->ctrlr, "resubmitting queued i/o\n");
	while (!STAILQ_EMPTY(&temp)) {
		req = STAILQ_FIRST(&temp);
		STAILQ_REMOVE_HEAD(&temp, stailq);
		nvme_qpair_print_command(qpair, &req->cmd);
		_nvme_qpair_submit_request(qpair, req);
	}
	if (report)
		nvme_printf(qpair->ctrlr, "done resubmitting i/o\n");

	mtx_unlock(&qpair->lock);
	mtx_unlock(&qpair->recovery);
}

static void
nvme_qpair_disable(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr, *tr_temp;

	if (mtx_initialized(&qpair->recovery))
		mtx_assert(&qpair->recovery, MA_OWNED);
	if (mtx_initialized(&qpair->lock))
		mtx_assert(&qpair->lock, MA_OWNED);

	qpair->recovery_state = RECOVERY_WAITING;
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		tr->deadline = SBT_MAX;
	}
}

void
nvme_admin_qpair_disable(struct nvme_qpair *qpair)
{
	mtx_lock(&qpair->recovery);

	mtx_lock(&qpair->lock);
	nvme_qpair_disable(qpair);
	mtx_unlock(&qpair->lock);

	nvme_admin_qpair_abort_aers(qpair);

	mtx_unlock(&qpair->recovery);
}

void
nvme_io_qpair_disable(struct nvme_qpair *qpair)
{
	mtx_lock(&qpair->recovery);
	mtx_lock(&qpair->lock);

	nvme_qpair_disable(qpair);

	mtx_unlock(&qpair->lock);
	mtx_unlock(&qpair->recovery);
}

void
nvme_qpair_fail(struct nvme_qpair *qpair)
{
	struct nvme_tracker		*tr;
	struct nvme_request		*req;

	if (!mtx_initialized(&qpair->lock))
		return;

	mtx_lock(&qpair->lock);

	if (!STAILQ_EMPTY(&qpair->queued_req)) {
		nvme_printf(qpair->ctrlr, "failing queued i/o\n");
	}
	while (!STAILQ_EMPTY(&qpair->queued_req)) {
		req = STAILQ_FIRST(&qpair->queued_req);
		STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_request(qpair, req, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 1, ERROR_PRINT_ALL);
		mtx_lock(&qpair->lock);
	}

	if (!TAILQ_EMPTY(&qpair->outstanding_tr)) {
		nvme_printf(qpair->ctrlr, "failing outstanding i/o\n");
	}
	/* Manually abort each outstanding I/O. */
	while (!TAILQ_EMPTY(&qpair->outstanding_tr)) {
		tr = TAILQ_FIRST(&qpair->outstanding_tr);
		/*
		 * Do not remove the tracker.  The abort_tracker path will
		 *  do that for us.
		 */
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, DO_NOT_RETRY, ERROR_PRINT_ALL);
		mtx_lock(&qpair->lock);
	}

	mtx_unlock(&qpair->lock);
}
