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
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/smp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "nvme_private.h"

static void nvme_ctrlr_construct_and_submit_aer(struct nvme_controller *ctrlr,
						struct nvme_async_event_request *aer);

static void
nvme_ctrlr_cb(void *arg, const struct nvme_completion *status)
{
	struct nvme_completion	*cpl = arg;
	struct mtx		*mtx;

	/*
	 * Copy status into the argument passed by the caller, so that
	 *  the caller can check the status to determine if the
	 *  the request passed or failed.
	 */
	memcpy(cpl, status, sizeof(*cpl));
	mtx = mtx_pool_find(mtxpool_sleep, cpl);
	mtx_lock(mtx);
	wakeup(cpl);
	mtx_unlock(mtx);
}

static int
nvme_ctrlr_allocate_bar(struct nvme_controller *ctrlr)
{

	/* Chatham puts the NVMe MMRs behind BAR 2/3, not BAR 0/1. */
	if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID)
		ctrlr->resource_id = PCIR_BAR(2);
	else
		ctrlr->resource_id = PCIR_BAR(0);

	ctrlr->resource = bus_alloc_resource(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->resource_id, 0, ~0, 1, RF_ACTIVE);

	if(ctrlr->resource == NULL) {
		device_printf(ctrlr->dev, "unable to allocate pci resource\n");
		return (ENOMEM);
	}

	ctrlr->bus_tag = rman_get_bustag(ctrlr->resource);
	ctrlr->bus_handle = rman_get_bushandle(ctrlr->resource);
	ctrlr->regs = (struct nvme_registers *)ctrlr->bus_handle;

	/*
	 * The NVMe spec allows for the MSI-X table to be placed behind
	 *  BAR 4/5, separate from the control/doorbell registers.  Always
	 *  try to map this bar, because it must be mapped prior to calling
	 *  pci_alloc_msix().  If the table isn't behind BAR 4/5,
	 *  bus_alloc_resource() will just return NULL which is OK.
	 */
	ctrlr->bar4_resource_id = PCIR_BAR(4);
	ctrlr->bar4_resource = bus_alloc_resource(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->bar4_resource_id, 0, ~0, 1, RF_ACTIVE);

	return (0);
}

#ifdef CHATHAM2
static int
nvme_ctrlr_allocate_chatham_bar(struct nvme_controller *ctrlr)
{

	ctrlr->chatham_resource_id = PCIR_BAR(CHATHAM_CONTROL_BAR);
	ctrlr->chatham_resource = bus_alloc_resource(ctrlr->dev,
	    SYS_RES_MEMORY, &ctrlr->chatham_resource_id, 0, ~0, 1,
	    RF_ACTIVE);

	if(ctrlr->chatham_resource == NULL) {
		device_printf(ctrlr->dev, "unable to alloc pci resource\n");
		return (ENOMEM);
	}

	ctrlr->chatham_bus_tag = rman_get_bustag(ctrlr->chatham_resource);
	ctrlr->chatham_bus_handle =
	    rman_get_bushandle(ctrlr->chatham_resource);

	return (0);
}

static void
nvme_ctrlr_setup_chatham(struct nvme_controller *ctrlr)
{
	uint64_t reg1, reg2, reg3;
	uint64_t temp1, temp2;
	uint32_t temp3;
	uint32_t use_flash_timings = 0;

	DELAY(10000);

	temp3 = chatham_read_4(ctrlr, 0x8080);

	device_printf(ctrlr->dev, "Chatham version: 0x%x\n", temp3);

	ctrlr->chatham_lbas = chatham_read_4(ctrlr, 0x8068) - 0x110;
	ctrlr->chatham_size = ctrlr->chatham_lbas * 512;

	device_printf(ctrlr->dev, "Chatham size: %jd\n",
	    (intmax_t)ctrlr->chatham_size);

	reg1 = reg2 = reg3 = ctrlr->chatham_size - 1;

	TUNABLE_INT_FETCH("hw.nvme.use_flash_timings", &use_flash_timings);
	if (use_flash_timings) {
		device_printf(ctrlr->dev, "Chatham: using flash timings\n");
		temp1 = 0x00001b58000007d0LL;
		temp2 = 0x000000cb00000131LL;
	} else {
		device_printf(ctrlr->dev, "Chatham: using DDR timings\n");
		temp1 = temp2 = 0x0LL;
	}

	chatham_write_8(ctrlr, 0x8000, reg1);
	chatham_write_8(ctrlr, 0x8008, reg2);
	chatham_write_8(ctrlr, 0x8010, reg3);

	chatham_write_8(ctrlr, 0x8020, temp1);
	temp3 = chatham_read_4(ctrlr, 0x8020);

	chatham_write_8(ctrlr, 0x8028, temp2);
	temp3 = chatham_read_4(ctrlr, 0x8028);

	chatham_write_8(ctrlr, 0x8030, temp1);
	chatham_write_8(ctrlr, 0x8038, temp2);
	chatham_write_8(ctrlr, 0x8040, temp1);
	chatham_write_8(ctrlr, 0x8048, temp2);
	chatham_write_8(ctrlr, 0x8050, temp1);
	chatham_write_8(ctrlr, 0x8058, temp2);

	DELAY(10000);
}

static void
nvme_chatham_populate_cdata(struct nvme_controller *ctrlr)
{
	struct nvme_controller_data *cdata;

	cdata = &ctrlr->cdata;

	cdata->vid = 0x8086;
	cdata->ssvid = 0x2011;

	/*
	 * Chatham2 puts garbage data in these fields when we
	 *  invoke IDENTIFY_CONTROLLER, so we need to re-zero
	 *  the fields before calling bcopy().
	 */
	memset(cdata->sn, 0, sizeof(cdata->sn));
	memcpy(cdata->sn, "2012", strlen("2012"));
	memset(cdata->mn, 0, sizeof(cdata->mn));
	memcpy(cdata->mn, "CHATHAM2", strlen("CHATHAM2"));
	memset(cdata->fr, 0, sizeof(cdata->fr));
	memcpy(cdata->fr, "0", strlen("0"));
	cdata->rab = 8;
	cdata->aerl = 3;
	cdata->lpa.ns_smart = 1;
	cdata->sqes.min = 6;
	cdata->sqes.max = 6;
	cdata->sqes.min = 4;
	cdata->sqes.max = 4;
	cdata->nn = 1;

	/* Chatham2 doesn't support DSM command */
	cdata->oncs.dsm = 0;

	cdata->vwc.present = 1;
}
#endif /* CHATHAM2 */

static void
nvme_ctrlr_construct_admin_qpair(struct nvme_controller *ctrlr)
{
	struct nvme_qpair	*qpair;
	uint32_t		num_entries;

	qpair = &ctrlr->adminq;

	num_entries = NVME_ADMIN_ENTRIES;
	TUNABLE_INT_FETCH("hw.nvme.admin_entries", &num_entries);
	/*
	 * If admin_entries was overridden to an invalid value, revert it
	 *  back to our default value.
	 */
	if (num_entries < NVME_MIN_ADMIN_ENTRIES ||
	    num_entries > NVME_MAX_ADMIN_ENTRIES) {
		printf("nvme: invalid hw.nvme.admin_entries=%d specified\n",
		    num_entries);
		num_entries = NVME_ADMIN_ENTRIES;
	}

	/*
	 * The admin queue's max xfer size is treated differently than the
	 *  max I/O xfer size.  16KB is sufficient here - maybe even less?
	 */
	nvme_qpair_construct(qpair, 
			     0, /* qpair ID */
			     0, /* vector */
			     num_entries,
			     NVME_ADMIN_TRACKERS,
			     16*1024, /* max xfer size */
			     ctrlr);
}

static int
nvme_ctrlr_construct_io_qpairs(struct nvme_controller *ctrlr)
{
	struct nvme_qpair	*qpair;
	union cap_lo_register	cap_lo;
	int			i, num_entries, num_trackers;

	num_entries = NVME_IO_ENTRIES;
	TUNABLE_INT_FETCH("hw.nvme.io_entries", &num_entries);

	/*
	 * NVMe spec sets a hard limit of 64K max entries, but
	 *  devices may specify a smaller limit, so we need to check
	 *  the MQES field in the capabilities register.
	 */
	cap_lo.raw = nvme_mmio_read_4(ctrlr, cap_lo);
	num_entries = min(num_entries, cap_lo.bits.mqes+1);

	num_trackers = NVME_IO_TRACKERS;
	TUNABLE_INT_FETCH("hw.nvme.io_trackers", &num_trackers);

	num_trackers = max(num_trackers, NVME_MIN_IO_TRACKERS);
	num_trackers = min(num_trackers, NVME_MAX_IO_TRACKERS);
	/*
	 * No need to have more trackers than entries in the submit queue.
	 *  Note also that for a queue size of N, we can only have (N-1)
	 *  commands outstanding, hence the "-1" here.
	 */
	num_trackers = min(num_trackers, (num_entries-1));

	ctrlr->max_xfer_size = NVME_MAX_XFER_SIZE;
	TUNABLE_INT_FETCH("hw.nvme.max_xfer_size", &ctrlr->max_xfer_size);
	/*
	 * Check that tunable doesn't specify a size greater than what our
	 *  driver supports, and is an even PAGE_SIZE multiple.
	 */
	if (ctrlr->max_xfer_size > NVME_MAX_XFER_SIZE ||
	    ctrlr->max_xfer_size % PAGE_SIZE)
		ctrlr->max_xfer_size = NVME_MAX_XFER_SIZE;

	ctrlr->ioq = malloc(ctrlr->num_io_queues * sizeof(struct nvme_qpair),
	    M_NVME, M_ZERO | M_NOWAIT);

	if (ctrlr->ioq == NULL)
		return (ENOMEM);

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		qpair = &ctrlr->ioq[i];

		/*
		 * Admin queue has ID=0. IO queues start at ID=1 -
		 *  hence the 'i+1' here.
		 *
		 * For I/O queues, use the controller-wide max_xfer_size
		 *  calculated in nvme_attach().
		 */
		nvme_qpair_construct(qpair,
				     i+1, /* qpair ID */
				     ctrlr->msix_enabled ? i+1 : 0, /* vector */
				     num_entries,
				     num_trackers,
				     ctrlr->max_xfer_size,
				     ctrlr);

		if (ctrlr->per_cpu_io_queues)
			bus_bind_intr(ctrlr->dev, qpair->res, i);
	}

	return (0);
}

static int
nvme_ctrlr_wait_for_ready(struct nvme_controller *ctrlr)
{
	int ms_waited;
	union cc_register cc;
	union csts_register csts;

	cc.raw = nvme_mmio_read_4(ctrlr, cc);
	csts.raw = nvme_mmio_read_4(ctrlr, csts);

	if (!cc.bits.en) {
		device_printf(ctrlr->dev, "%s called with cc.en = 0\n",
		    __func__);
		return (ENXIO);
	}

	ms_waited = 0;

	while (!csts.bits.rdy) {
		DELAY(1000);
		if (ms_waited++ > ctrlr->ready_timeout_in_ms) {
			device_printf(ctrlr->dev, "controller did not become "
			    "ready within %d ms\n", ctrlr->ready_timeout_in_ms);
			return (ENXIO);
		}
		csts.raw = nvme_mmio_read_4(ctrlr, csts);
	}

	return (0);
}

static void
nvme_ctrlr_disable(struct nvme_controller *ctrlr)
{
	union cc_register cc;
	union csts_register csts;

	cc.raw = nvme_mmio_read_4(ctrlr, cc);
	csts.raw = nvme_mmio_read_4(ctrlr, csts);

	if (cc.bits.en == 1 && csts.bits.rdy == 0)
		nvme_ctrlr_wait_for_ready(ctrlr);

	cc.bits.en = 0;
	nvme_mmio_write_4(ctrlr, cc, cc.raw);
	DELAY(5000);
}

static int
nvme_ctrlr_enable(struct nvme_controller *ctrlr)
{
	union cc_register	cc;
	union csts_register	csts;
	union aqa_register	aqa;

	cc.raw = nvme_mmio_read_4(ctrlr, cc);
	csts.raw = nvme_mmio_read_4(ctrlr, csts);

	if (cc.bits.en == 1) {
		if (csts.bits.rdy == 1)
			return (0);
		else
			return (nvme_ctrlr_wait_for_ready(ctrlr));
	}

	nvme_mmio_write_8(ctrlr, asq, ctrlr->adminq.cmd_bus_addr);
	DELAY(5000);
	nvme_mmio_write_8(ctrlr, acq, ctrlr->adminq.cpl_bus_addr);
	DELAY(5000);

	aqa.raw = 0;
	/* acqs and asqs are 0-based. */
	aqa.bits.acqs = ctrlr->adminq.num_entries-1;
	aqa.bits.asqs = ctrlr->adminq.num_entries-1;
	nvme_mmio_write_4(ctrlr, aqa, aqa.raw);
	DELAY(5000);

	cc.bits.en = 1;
	cc.bits.css = 0;
	cc.bits.ams = 0;
	cc.bits.shn = 0;
	cc.bits.iosqes = 6; /* SQ entry size == 64 == 2^6 */
	cc.bits.iocqes = 4; /* CQ entry size == 16 == 2^4 */

	/* This evaluates to 0, which is according to spec. */
	cc.bits.mps = (PAGE_SIZE >> 13);

	nvme_mmio_write_4(ctrlr, cc, cc.raw);
	DELAY(5000);

	return (nvme_ctrlr_wait_for_ready(ctrlr));
}

int
nvme_ctrlr_hw_reset(struct nvme_controller *ctrlr)
{
	int i;

	nvme_admin_qpair_disable(&ctrlr->adminq);
	for (i = 0; i < ctrlr->num_io_queues; i++)
		nvme_io_qpair_disable(&ctrlr->ioq[i]);

	DELAY(100*1000);

	nvme_ctrlr_disable(ctrlr);
	return (nvme_ctrlr_enable(ctrlr));
}

void
nvme_ctrlr_reset(struct nvme_controller *ctrlr)
{
	int cmpset;

	cmpset = atomic_cmpset_32(&ctrlr->is_resetting, 0, 1);

	if (cmpset == 0)
		/* Controller is already resetting. */
		return;

	taskqueue_enqueue(ctrlr->taskqueue, &ctrlr->reset_task);
}

static int
nvme_ctrlr_identify(struct nvme_controller *ctrlr)
{
	struct mtx		*mtx;
	struct nvme_completion	cpl;
	int			status;

	mtx = mtx_pool_find(mtxpool_sleep, &cpl);

	mtx_lock(mtx);
	nvme_ctrlr_cmd_identify_controller(ctrlr, &ctrlr->cdata,
	    nvme_ctrlr_cb, &cpl);
	status = msleep(&cpl, mtx, PRIBIO, "nvme_start", hz*5);
	mtx_unlock(mtx);
	if ((status != 0) || nvme_completion_is_error(&cpl)) {
		printf("nvme_identify_controller failed!\n");
		return (ENXIO);
	}

#ifdef CHATHAM2
	if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID)
		nvme_chatham_populate_cdata(ctrlr);
#endif

	return (0);
}

static int
nvme_ctrlr_set_num_qpairs(struct nvme_controller *ctrlr)
{
	struct mtx		*mtx;
	struct nvme_completion	cpl;
	int			cq_allocated, sq_allocated, status;

	mtx = mtx_pool_find(mtxpool_sleep, &cpl);

	mtx_lock(mtx);
	nvme_ctrlr_cmd_set_num_queues(ctrlr, ctrlr->num_io_queues,
	    nvme_ctrlr_cb, &cpl);
	status = msleep(&cpl, mtx, PRIBIO, "nvme_start", hz*5);
	mtx_unlock(mtx);
	if ((status != 0) || nvme_completion_is_error(&cpl)) {
		printf("nvme_set_num_queues failed!\n");
		return (ENXIO);
	}

	/*
	 * Data in cdw0 is 0-based.
	 * Lower 16-bits indicate number of submission queues allocated.
	 * Upper 16-bits indicate number of completion queues allocated.
	 */
	sq_allocated = (cpl.cdw0 & 0xFFFF) + 1;
	cq_allocated = (cpl.cdw0 >> 16) + 1;

	/*
	 * Check that the controller was able to allocate the number of
	 *  queues we requested.  If not, revert to one IO queue.
	 */
	if (sq_allocated < ctrlr->num_io_queues ||
	    cq_allocated < ctrlr->num_io_queues) {
		ctrlr->num_io_queues = 1;
		ctrlr->per_cpu_io_queues = 0;

		/* TODO: destroy extra queues that were created
		 *  previously but now found to be not needed.
		 */
	}

	return (0);
}

static int
nvme_ctrlr_create_qpairs(struct nvme_controller *ctrlr)
{
	struct mtx		*mtx;
	struct nvme_qpair	*qpair;
	struct nvme_completion	cpl;
	int			i, status;

	mtx = mtx_pool_find(mtxpool_sleep, &cpl);

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		qpair = &ctrlr->ioq[i];

		mtx_lock(mtx);
		nvme_ctrlr_cmd_create_io_cq(ctrlr, qpair, qpair->vector,
		    nvme_ctrlr_cb, &cpl);
		status = msleep(&cpl, mtx, PRIBIO, "nvme_start", hz*5);
		mtx_unlock(mtx);
		if ((status != 0) || nvme_completion_is_error(&cpl)) {
			printf("nvme_create_io_cq failed!\n");
			return (ENXIO);
		}

		mtx_lock(mtx);
		nvme_ctrlr_cmd_create_io_sq(qpair->ctrlr, qpair,
		    nvme_ctrlr_cb, &cpl);
		status = msleep(&cpl, mtx, PRIBIO, "nvme_start", hz*5);
		mtx_unlock(mtx);
		if ((status != 0) || nvme_completion_is_error(&cpl)) {
			printf("nvme_create_io_sq failed!\n");
			return (ENXIO);
		}
	}

	return (0);
}

static int
nvme_ctrlr_construct_namespaces(struct nvme_controller *ctrlr)
{
	struct nvme_namespace	*ns;
	int			i, status;

	for (i = 0; i < ctrlr->cdata.nn; i++) {
		ns = &ctrlr->ns[i];
		status = nvme_ns_construct(ns, i+1, ctrlr);
		if (status != 0)
			return (status);
	}

	return (0);
}

static boolean_t
is_log_page_id_valid(uint8_t page_id)
{

	switch (page_id) {
	case NVME_LOG_ERROR:
	case NVME_LOG_HEALTH_INFORMATION:
	case NVME_LOG_FIRMWARE_SLOT:
		return (TRUE);
	}

	return (FALSE);
}

static uint32_t
nvme_ctrlr_get_log_page_size(struct nvme_controller *ctrlr, uint8_t page_id)
{
	uint32_t	log_page_size;

	switch (page_id) {
	case NVME_LOG_ERROR:
		log_page_size = min(
		    sizeof(struct nvme_error_information_entry) *
		    ctrlr->cdata.elpe,
		    NVME_MAX_AER_LOG_SIZE);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
		log_page_size = sizeof(struct nvme_health_information_page);
		break;
	case NVME_LOG_FIRMWARE_SLOT:
		log_page_size = sizeof(struct nvme_firmware_page);
		break;
	default:
		log_page_size = 0;
		break;
	}

	return (log_page_size);
}

static void
nvme_ctrlr_async_event_log_page_cb(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_async_event_request	*aer = arg;

	nvme_notify_async_consumers(aer->ctrlr, &aer->cpl);

	/*
	 * Repost another asynchronous event request to replace the one
	 *  that just completed.
	 */
	nvme_ctrlr_construct_and_submit_aer(aer->ctrlr, aer);
}

static void
nvme_ctrlr_async_event_cb(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_async_event_request	*aer = arg;
	uint8_t				log_page_id;

	if (cpl->status.sc == NVME_SC_ABORTED_SQ_DELETION) {
		/*
		 *  This is simulated when controller is being shut down, to
		 *  effectively abort outstanding asynchronous event requests
		 *  and make sure all memory is freed.  Do not repost the
		 *  request in this case.
		 */
		return;
	}

	printf("Asynchronous event occurred.\n");

	/* Associated log page is in bits 23:16 of completion entry dw0. */
	log_page_id = (cpl->cdw0 & 0xFF0000) >> 16;

	if (is_log_page_id_valid(log_page_id)) {
		aer->log_page_size = nvme_ctrlr_get_log_page_size(aer->ctrlr,
		    log_page_id);
		memcpy(&aer->cpl, cpl, sizeof(*cpl));
		nvme_ctrlr_cmd_get_log_page(aer->ctrlr, log_page_id,
		    NVME_GLOBAL_NAMESPACE_TAG, aer->log_page_buffer,
		    aer->log_page_size, nvme_ctrlr_async_event_log_page_cb,
		    aer);
		/* Wait to notify consumers until after log page is fetched. */
	} else {
		nvme_notify_async_consumers(aer->ctrlr, cpl);

		/*
		 * Repost another asynchronous event request to replace the one
		 *  that just completed.
		 */
		nvme_ctrlr_construct_and_submit_aer(aer->ctrlr, aer);
	}
}

static void
nvme_ctrlr_construct_and_submit_aer(struct nvme_controller *ctrlr,
    struct nvme_async_event_request *aer)
{
	struct nvme_request *req;

	aer->ctrlr = ctrlr;
	req = nvme_allocate_request(NULL, 0, nvme_ctrlr_async_event_cb, aer);
	aer->req = req;

	/*
	 * Disable timeout here, since asynchronous event requests should by
	 *  nature never be timed out.
	 */
	req->timeout = FALSE;
	req->cmd.opc = NVME_OPC_ASYNC_EVENT_REQUEST;
	nvme_ctrlr_submit_admin_request(ctrlr, req);
}

static void
nvme_ctrlr_configure_aer(struct nvme_controller *ctrlr)
{
	union nvme_critical_warning_state	state;
	struct nvme_async_event_request		*aer;
	uint32_t				i;

	state.raw = 0xFF;
	state.bits.reserved = 0;
	nvme_ctrlr_cmd_set_async_event_config(ctrlr, state, NULL, NULL);

	/* aerl is a zero-based value, so we need to add 1 here. */
	ctrlr->num_aers = min(NVME_MAX_ASYNC_EVENTS, (ctrlr->cdata.aerl+1));

	/* Chatham doesn't support AERs. */
	if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID)
		ctrlr->num_aers = 0;

	for (i = 0; i < ctrlr->num_aers; i++) {
		aer = &ctrlr->aer[i];
		nvme_ctrlr_construct_and_submit_aer(ctrlr, aer);
	}
}

static void
nvme_ctrlr_configure_int_coalescing(struct nvme_controller *ctrlr)
{

	ctrlr->int_coal_time = 0;
	TUNABLE_INT_FETCH("hw.nvme.int_coal_time",
	    &ctrlr->int_coal_time);

	ctrlr->int_coal_threshold = 0;
	TUNABLE_INT_FETCH("hw.nvme.int_coal_threshold",
	    &ctrlr->int_coal_threshold);

	nvme_ctrlr_cmd_set_interrupt_coalescing(ctrlr, ctrlr->int_coal_time,
	    ctrlr->int_coal_threshold, NULL, NULL);
}

void
nvme_ctrlr_start(void *ctrlr_arg)
{
	struct nvme_controller *ctrlr = ctrlr_arg;
	int i;

	nvme_admin_qpair_enable(&ctrlr->adminq);

	if (nvme_ctrlr_identify(ctrlr) != 0)
		goto err;

	if (nvme_ctrlr_set_num_qpairs(ctrlr) != 0)
		goto err;

	if (nvme_ctrlr_create_qpairs(ctrlr) != 0)
		goto err;

	if (nvme_ctrlr_construct_namespaces(ctrlr) != 0)
		goto err;

	nvme_ctrlr_configure_aer(ctrlr);
	nvme_ctrlr_configure_int_coalescing(ctrlr);

	for (i = 0; i < ctrlr->num_io_queues; i++)
		nvme_io_qpair_enable(&ctrlr->ioq[i]);

	ctrlr->is_started = TRUE;

err:

	if (ctrlr->num_start_attempts == 0) {
		/*
		 * Initialize sysctls, even if controller failed to start, to
		 *  assist with debugging admin queue pair.  Only run this
		 *  code on the initial start attempt though, and not
		 *  subsequent start attempts due to controller-level resets.
		 *
		 */
		nvme_sysctl_initialize_ctrlr(ctrlr);
		config_intrhook_disestablish(&ctrlr->config_hook);
	}

	ctrlr->num_start_attempts++;
}

static void
nvme_ctrlr_reset_task(void *arg, int pending)
{
	struct nvme_controller	*ctrlr = arg;
	int			status;

	device_printf(ctrlr->dev, "resetting controller");
	status = nvme_ctrlr_hw_reset(ctrlr);
	/*
	 * Use pause instead of DELAY, so that we yield to any nvme interrupt
	 *  handlers on this CPU that were blocked on a qpair lock. We want
	 *  all nvme interrupts completed before proceeding with restarting the
	 *  controller.
	 *
	 * XXX - any way to guarantee the interrupt handlers have quiesced?
	 */
	pause("nvmereset", hz / 10);
	if (status == 0)
		nvme_ctrlr_start(ctrlr);

	atomic_cmpset_32(&ctrlr->is_resetting, 1, 0);
}

static void
nvme_ctrlr_intx_handler(void *arg)
{
	struct nvme_controller *ctrlr = arg;

	nvme_mmio_write_4(ctrlr, intms, 1);

	nvme_qpair_process_completions(&ctrlr->adminq);

	if (ctrlr->ioq[0].cpl)
		nvme_qpair_process_completions(&ctrlr->ioq[0]);

	nvme_mmio_write_4(ctrlr, intmc, 1);
}

static int
nvme_ctrlr_configure_intx(struct nvme_controller *ctrlr)
{

	ctrlr->num_io_queues = 1;
	ctrlr->per_cpu_io_queues = 0;
	ctrlr->rid = 0;
	ctrlr->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
	    &ctrlr->rid, RF_SHAREABLE | RF_ACTIVE);

	if (ctrlr->res == NULL) {
		device_printf(ctrlr->dev, "unable to allocate shared IRQ\n");
		return (ENOMEM);
	}

	bus_setup_intr(ctrlr->dev, ctrlr->res,
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, nvme_ctrlr_intx_handler,
	    ctrlr, &ctrlr->tag);

	if (ctrlr->tag == NULL) {
		device_printf(ctrlr->dev,
		    "unable to setup legacy interrupt handler\n");
		return (ENOMEM);
	}

	return (0);
}

static int
nvme_ctrlr_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct nvme_controller	*ctrlr;
	struct nvme_completion	cpl;
	struct mtx		*mtx;

	ctrlr = cdev->si_drv1;

	switch (cmd) {
	case NVME_IDENTIFY_CONTROLLER:
#ifdef CHATHAM2
		/*
		 * Don't refresh data on Chatham, since Chatham returns
		 *  garbage on IDENTIFY anyways.
		 */
		if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID) {
			memcpy(arg, &ctrlr->cdata, sizeof(ctrlr->cdata));
			break;
		}
#endif
		/* Refresh data before returning to user. */
		mtx = mtx_pool_find(mtxpool_sleep, &cpl);
		mtx_lock(mtx);
		nvme_ctrlr_cmd_identify_controller(ctrlr, &ctrlr->cdata,
		    nvme_ctrlr_cb, &cpl);
		msleep(&cpl, mtx, PRIBIO, "nvme_ioctl", 0);
		mtx_unlock(mtx);
		if (nvme_completion_is_error(&cpl))
			return (ENXIO);
		memcpy(arg, &ctrlr->cdata, sizeof(ctrlr->cdata));
		break;
	case NVME_RESET_CONTROLLER:
		nvme_ctrlr_reset(ctrlr);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

static struct cdevsw nvme_ctrlr_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_ioctl =	nvme_ctrlr_ioctl
};

int
nvme_ctrlr_construct(struct nvme_controller *ctrlr, device_t dev)
{
	union cap_lo_register	cap_lo;
	union cap_hi_register	cap_hi;
	int			num_vectors, per_cpu_io_queues, status = 0;
	int			timeout_period;

	ctrlr->dev = dev;
	ctrlr->is_started = FALSE;
	ctrlr->num_start_attempts = 0;

	status = nvme_ctrlr_allocate_bar(ctrlr);

	if (status != 0)
		return (status);

#ifdef CHATHAM2
	if (pci_get_devid(dev) == CHATHAM_PCI_ID) {
		status = nvme_ctrlr_allocate_chatham_bar(ctrlr);
		if (status != 0)
			return (status);
		nvme_ctrlr_setup_chatham(ctrlr);
	}
#endif

	/*
	 * Software emulators may set the doorbell stride to something
	 *  other than zero, but this driver is not set up to handle that.
	 */
	cap_hi.raw = nvme_mmio_read_4(ctrlr, cap_hi);
	if (cap_hi.bits.dstrd != 0)
		return (ENXIO);

	/* Get ready timeout value from controller, in units of 500ms. */
	cap_lo.raw = nvme_mmio_read_4(ctrlr, cap_lo);
	ctrlr->ready_timeout_in_ms = cap_lo.bits.to * 500;

	timeout_period = NVME_DEFAULT_TIMEOUT_PERIOD;
	TUNABLE_INT_FETCH("hw.nvme.timeout_period", &timeout_period);
	timeout_period = min(timeout_period, NVME_MAX_TIMEOUT_PERIOD);
	timeout_period = max(timeout_period, NVME_MIN_TIMEOUT_PERIOD);
	ctrlr->timeout_period = timeout_period;

	per_cpu_io_queues = 1;
	TUNABLE_INT_FETCH("hw.nvme.per_cpu_io_queues", &per_cpu_io_queues);
	ctrlr->per_cpu_io_queues = per_cpu_io_queues ? TRUE : FALSE;

	if (ctrlr->per_cpu_io_queues)
		ctrlr->num_io_queues = mp_ncpus;
	else
		ctrlr->num_io_queues = 1;

	ctrlr->force_intx = 0;
	TUNABLE_INT_FETCH("hw.nvme.force_intx", &ctrlr->force_intx);

	ctrlr->enable_aborts = 0;
	TUNABLE_INT_FETCH("hw.nvme.enable_aborts", &ctrlr->enable_aborts);

	ctrlr->msix_enabled = 1;

	if (ctrlr->force_intx) {
		ctrlr->msix_enabled = 0;
		goto intx;
	}

	/* One vector per IO queue, plus one vector for admin queue. */
	num_vectors = ctrlr->num_io_queues + 1;

	if (pci_msix_count(dev) < num_vectors) {
		ctrlr->msix_enabled = 0;
		goto intx;
	}

	if (pci_alloc_msix(dev, &num_vectors) != 0)
		ctrlr->msix_enabled = 0;

intx:

	if (!ctrlr->msix_enabled)
		nvme_ctrlr_configure_intx(ctrlr);

	nvme_ctrlr_construct_admin_qpair(ctrlr);

	status = nvme_ctrlr_construct_io_qpairs(ctrlr);

	if (status != 0)
		return (status);

	ctrlr->cdev = make_dev(&nvme_ctrlr_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "nvme%d", device_get_unit(dev));

	if (ctrlr->cdev == NULL)
		return (ENXIO);

	ctrlr->cdev->si_drv1 = (void *)ctrlr;

	TASK_INIT(&ctrlr->reset_task, 0, nvme_ctrlr_reset_task, ctrlr);
	ctrlr->taskqueue = taskqueue_create("nvme_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &ctrlr->taskqueue);
	taskqueue_start_threads(&ctrlr->taskqueue, 1, PI_DISK, "nvme taskq");

	ctrlr->is_resetting = 0;

	return (0);
}

void
nvme_ctrlr_destruct(struct nvme_controller *ctrlr, device_t dev)
{
	int				i;

	taskqueue_free(ctrlr->taskqueue);

	for (i = 0; i < NVME_MAX_NAMESPACES; i++)
		nvme_ns_destruct(&ctrlr->ns[i]);

	if (ctrlr->cdev)
		destroy_dev(ctrlr->cdev);

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		nvme_io_qpair_destroy(&ctrlr->ioq[i]);
	}

	free(ctrlr->ioq, M_NVME);

	nvme_admin_qpair_destroy(&ctrlr->adminq);

	if (ctrlr->resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->resource_id, ctrlr->resource);
	}

	if (ctrlr->bar4_resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->bar4_resource_id, ctrlr->bar4_resource);
	}

#ifdef CHATHAM2
	if (ctrlr->chatham_resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->chatham_resource_id, ctrlr->chatham_resource);
	}
#endif

	if (ctrlr->tag)
		bus_teardown_intr(ctrlr->dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);

	if (ctrlr->msix_enabled)
		pci_release_msi(dev);
}

void
nvme_ctrlr_submit_admin_request(struct nvme_controller *ctrlr,
    struct nvme_request *req)
{

	nvme_qpair_submit_request(&ctrlr->adminq, req);
}

void
nvme_ctrlr_submit_io_request(struct nvme_controller *ctrlr,
    struct nvme_request *req)
{
	struct nvme_qpair       *qpair;

	if (ctrlr->per_cpu_io_queues)
		qpair = &ctrlr->ioq[curcpu];
	else
		qpair = &ctrlr->ioq[0];

	nvme_qpair_submit_request(qpair, req);
}

device_t
nvme_ctrlr_get_device(struct nvme_controller *ctrlr)
{

	return (ctrlr->dev);
}

const struct nvme_controller_data *
nvme_ctrlr_get_data(struct nvme_controller *ctrlr)
{

	return (&ctrlr->cdata);
}
