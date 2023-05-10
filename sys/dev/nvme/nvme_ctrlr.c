/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2016 Intel Corporation
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

#include "opt_cam.h"
#include "opt_nvme.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/uio.h>
#include <sys/sbuf.h>
#include <sys/endian.h>
#include <machine/stdarg.h>
#include <vm/vm.h>

#include "nvme_private.h"

#define B4_CHK_RDY_DELAY_MS	2300		/* work around controller bug */

static void nvme_ctrlr_construct_and_submit_aer(struct nvme_controller *ctrlr,
						struct nvme_async_event_request *aer);

static void
nvme_ctrlr_barrier(struct nvme_controller *ctrlr, int flags)
{
	bus_barrier(ctrlr->resource, 0, rman_get_size(ctrlr->resource), flags);
}

static void
nvme_ctrlr_devctl_log(struct nvme_controller *ctrlr, const char *type, const char *msg, ...)
{
	struct sbuf sb;
	va_list ap;
	int error;

	if (sbuf_new(&sb, NULL, 0, SBUF_AUTOEXTEND | SBUF_NOWAIT) == NULL)
		return;
	sbuf_printf(&sb, "%s: ", device_get_nameunit(ctrlr->dev));
	va_start(ap, msg);
	sbuf_vprintf(&sb, msg, ap);
	va_end(ap);
	error = sbuf_finish(&sb);
	if (error == 0)
		printf("%s\n", sbuf_data(&sb));

	sbuf_clear(&sb);
	sbuf_printf(&sb, "name=\"%s\" reason=\"", device_get_nameunit(ctrlr->dev));
	va_start(ap, msg);
	sbuf_vprintf(&sb, msg, ap);
	va_end(ap);
	sbuf_printf(&sb, "\"");
	error = sbuf_finish(&sb);
	if (error == 0)
		devctl_notify("nvme", "controller", type, sbuf_data(&sb));
	sbuf_delete(&sb);
}

static int
nvme_ctrlr_construct_admin_qpair(struct nvme_controller *ctrlr)
{
	struct nvme_qpair	*qpair;
	uint32_t		num_entries;
	int			error;

	qpair = &ctrlr->adminq;
	qpair->id = 0;
	qpair->cpu = CPU_FFS(&cpuset_domain[ctrlr->domain]) - 1;
	qpair->domain = ctrlr->domain;

	num_entries = NVME_ADMIN_ENTRIES;
	TUNABLE_INT_FETCH("hw.nvme.admin_entries", &num_entries);
	/*
	 * If admin_entries was overridden to an invalid value, revert it
	 *  back to our default value.
	 */
	if (num_entries < NVME_MIN_ADMIN_ENTRIES ||
	    num_entries > NVME_MAX_ADMIN_ENTRIES) {
		nvme_printf(ctrlr, "invalid hw.nvme.admin_entries=%d "
		    "specified\n", num_entries);
		num_entries = NVME_ADMIN_ENTRIES;
	}

	/*
	 * The admin queue's max xfer size is treated differently than the
	 *  max I/O xfer size.  16KB is sufficient here - maybe even less?
	 */
	error = nvme_qpair_construct(qpair, num_entries, NVME_ADMIN_TRACKERS,
	     ctrlr);
	return (error);
}

#define QP(ctrlr, c)	((c) * (ctrlr)->num_io_queues / mp_ncpus)

static int
nvme_ctrlr_construct_io_qpairs(struct nvme_controller *ctrlr)
{
	struct nvme_qpair	*qpair;
	uint32_t		cap_lo;
	uint16_t		mqes;
	int			c, error, i, n;
	int			num_entries, num_trackers, max_entries;

	/*
	 * NVMe spec sets a hard limit of 64K max entries, but devices may
	 * specify a smaller limit, so we need to check the MQES field in the
	 * capabilities register. We have to cap the number of entries to the
	 * current stride allows for in BAR 0/1, otherwise the remainder entries
	 * are inaccessible. MQES should reflect this, and this is just a
	 * fail-safe.
	 */
	max_entries =
	    (rman_get_size(ctrlr->resource) - nvme_mmio_offsetof(doorbell[0])) /
	    (1 << (ctrlr->dstrd + 1));
	num_entries = NVME_IO_ENTRIES;
	TUNABLE_INT_FETCH("hw.nvme.io_entries", &num_entries);
	cap_lo = nvme_mmio_read_4(ctrlr, cap_lo);
	mqes = NVME_CAP_LO_MQES(cap_lo);
	num_entries = min(num_entries, mqes + 1);
	num_entries = min(num_entries, max_entries);

	num_trackers = NVME_IO_TRACKERS;
	TUNABLE_INT_FETCH("hw.nvme.io_trackers", &num_trackers);

	num_trackers = max(num_trackers, NVME_MIN_IO_TRACKERS);
	num_trackers = min(num_trackers, NVME_MAX_IO_TRACKERS);
	/*
	 * No need to have more trackers than entries in the submit queue.  Note
	 * also that for a queue size of N, we can only have (N-1) commands
	 * outstanding, hence the "-1" here.
	 */
	num_trackers = min(num_trackers, (num_entries-1));

	/*
	 * Our best estimate for the maximum number of I/Os that we should
	 * normally have in flight at one time. This should be viewed as a hint,
	 * not a hard limit and will need to be revisited when the upper layers
	 * of the storage system grows multi-queue support.
	 */
	ctrlr->max_hw_pend_io = num_trackers * ctrlr->num_io_queues * 3 / 4;

	ctrlr->ioq = malloc(ctrlr->num_io_queues * sizeof(struct nvme_qpair),
	    M_NVME, M_ZERO | M_WAITOK);

	for (i = c = n = 0; i < ctrlr->num_io_queues; i++, c += n) {
		qpair = &ctrlr->ioq[i];

		/*
		 * Admin queue has ID=0. IO queues start at ID=1 -
		 *  hence the 'i+1' here.
		 */
		qpair->id = i + 1;
		if (ctrlr->num_io_queues > 1) {
			/* Find number of CPUs served by this queue. */
			for (n = 1; QP(ctrlr, c + n) == i; n++)
				;
			/* Shuffle multiple NVMe devices between CPUs. */
			qpair->cpu = c + (device_get_unit(ctrlr->dev)+n/2) % n;
			qpair->domain = pcpu_find(qpair->cpu)->pc_domain;
		} else {
			qpair->cpu = CPU_FFS(&cpuset_domain[ctrlr->domain]) - 1;
			qpair->domain = ctrlr->domain;
		}

		/*
		 * For I/O queues, use the controller-wide max_xfer_size
		 *  calculated in nvme_attach().
		 */
		error = nvme_qpair_construct(qpair, num_entries, num_trackers,
		    ctrlr);
		if (error)
			return (error);

		/*
		 * Do not bother binding interrupts if we only have one I/O
		 *  interrupt thread for this controller.
		 */
		if (ctrlr->num_io_queues > 1)
			bus_bind_intr(ctrlr->dev, qpair->res, qpair->cpu);
	}

	return (0);
}

static void
nvme_ctrlr_fail(struct nvme_controller *ctrlr)
{
	int i;

	ctrlr->is_failed = true;
	nvme_admin_qpair_disable(&ctrlr->adminq);
	nvme_qpair_fail(&ctrlr->adminq);
	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++) {
			nvme_io_qpair_disable(&ctrlr->ioq[i]);
			nvme_qpair_fail(&ctrlr->ioq[i]);
		}
	}
	nvme_notify_fail_consumers(ctrlr);
}

void
nvme_ctrlr_post_failed_request(struct nvme_controller *ctrlr,
    struct nvme_request *req)
{

	mtx_lock(&ctrlr->lock);
	STAILQ_INSERT_TAIL(&ctrlr->fail_req, req, stailq);
	mtx_unlock(&ctrlr->lock);
	if (!ctrlr->is_dying)
		taskqueue_enqueue(ctrlr->taskqueue, &ctrlr->fail_req_task);
}

static void
nvme_ctrlr_fail_req_task(void *arg, int pending)
{
	struct nvme_controller	*ctrlr = arg;
	struct nvme_request	*req;

	mtx_lock(&ctrlr->lock);
	while ((req = STAILQ_FIRST(&ctrlr->fail_req)) != NULL) {
		STAILQ_REMOVE_HEAD(&ctrlr->fail_req, stailq);
		mtx_unlock(&ctrlr->lock);
		nvme_qpair_manual_complete_request(req->qpair, req,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST);
		mtx_lock(&ctrlr->lock);
	}
	mtx_unlock(&ctrlr->lock);
}

/*
 * Wait for RDY to change.
 *
 * Starts sleeping for 1us and geometrically increases it the longer we wait,
 * capped at 1ms.
 */
static int
nvme_ctrlr_wait_for_ready(struct nvme_controller *ctrlr, int desired_val)
{
	int timeout = ticks + MSEC_2_TICKS(ctrlr->ready_timeout_in_ms);
	sbintime_t delta_t = SBT_1US;
	uint32_t csts;

	while (1) {
		csts = nvme_mmio_read_4(ctrlr, csts);
		if (csts == NVME_GONE)		/* Hot unplug. */
			return (ENXIO);
		if (((csts >> NVME_CSTS_REG_RDY_SHIFT) & NVME_CSTS_REG_RDY_MASK)
		    == desired_val)
			break;
		if (timeout - ticks < 0) {
			nvme_printf(ctrlr, "controller ready did not become %d "
			    "within %d ms\n", desired_val, ctrlr->ready_timeout_in_ms);
			return (ENXIO);
		}

		pause_sbt("nvmerdy", delta_t, 0, C_PREL(1));
		delta_t = min(SBT_1MS, delta_t * 3 / 2);
	}

	return (0);
}

static int
nvme_ctrlr_disable(struct nvme_controller *ctrlr)
{
	uint32_t cc;
	uint32_t csts;
	uint8_t  en, rdy;
	int err;

	cc = nvme_mmio_read_4(ctrlr, cc);
	csts = nvme_mmio_read_4(ctrlr, csts);

	en = (cc >> NVME_CC_REG_EN_SHIFT) & NVME_CC_REG_EN_MASK;
	rdy = (csts >> NVME_CSTS_REG_RDY_SHIFT) & NVME_CSTS_REG_RDY_MASK;

	/*
	 * Per 3.1.5 in NVME 1.3 spec, transitioning CC.EN from 0 to 1
	 * when CSTS.RDY is 1 or transitioning CC.EN from 1 to 0 when
	 * CSTS.RDY is 0 "has undefined results" So make sure that CSTS.RDY
	 * isn't the desired value. Short circuit if we're already disabled.
	 */
	if (en == 0) {
		/* Wait for RDY == 0 or timeout & fail */
		if (rdy == 0)
			return (0);
		return (nvme_ctrlr_wait_for_ready(ctrlr, 0));
	}
	if (rdy == 0) {
		/* EN == 1, wait for  RDY == 1 or timeout & fail */
		err = nvme_ctrlr_wait_for_ready(ctrlr, 1);
		if (err != 0)
			return (err);
	}

	cc &= ~NVME_CC_REG_EN_MASK;
	nvme_mmio_write_4(ctrlr, cc, cc);

	/*
	 * A few drives have firmware bugs that freeze the drive if we access
	 * the mmio too soon after we disable.
	 */
	if (ctrlr->quirks & QUIRK_DELAY_B4_CHK_RDY)
		pause("nvmeR", MSEC_2_TICKS(B4_CHK_RDY_DELAY_MS));
	return (nvme_ctrlr_wait_for_ready(ctrlr, 0));
}

static int
nvme_ctrlr_enable(struct nvme_controller *ctrlr)
{
	uint32_t	cc;
	uint32_t	csts;
	uint32_t	aqa;
	uint32_t	qsize;
	uint8_t		en, rdy;
	int		err;

	cc = nvme_mmio_read_4(ctrlr, cc);
	csts = nvme_mmio_read_4(ctrlr, csts);

	en = (cc >> NVME_CC_REG_EN_SHIFT) & NVME_CC_REG_EN_MASK;
	rdy = (csts >> NVME_CSTS_REG_RDY_SHIFT) & NVME_CSTS_REG_RDY_MASK;

	/*
	 * See note in nvme_ctrlr_disable. Short circuit if we're already enabled.
	 */
	if (en == 1) {
		if (rdy == 1)
			return (0);
		return (nvme_ctrlr_wait_for_ready(ctrlr, 1));
	}

	/* EN == 0 already wait for RDY == 0 or timeout & fail */
	err = nvme_ctrlr_wait_for_ready(ctrlr, 0);
	if (err != 0)
		return (err);

	nvme_mmio_write_8(ctrlr, asq, ctrlr->adminq.cmd_bus_addr);
	nvme_mmio_write_8(ctrlr, acq, ctrlr->adminq.cpl_bus_addr);

	/* acqs and asqs are 0-based. */
	qsize = ctrlr->adminq.num_entries - 1;

	aqa = 0;
	aqa = (qsize & NVME_AQA_REG_ACQS_MASK) << NVME_AQA_REG_ACQS_SHIFT;
	aqa |= (qsize & NVME_AQA_REG_ASQS_MASK) << NVME_AQA_REG_ASQS_SHIFT;
	nvme_mmio_write_4(ctrlr, aqa, aqa);

	/* Initialization values for CC */
	cc = 0;
	cc |= 1 << NVME_CC_REG_EN_SHIFT;
	cc |= 0 << NVME_CC_REG_CSS_SHIFT;
	cc |= 0 << NVME_CC_REG_AMS_SHIFT;
	cc |= 0 << NVME_CC_REG_SHN_SHIFT;
	cc |= 6 << NVME_CC_REG_IOSQES_SHIFT; /* SQ entry size == 64 == 2^6 */
	cc |= 4 << NVME_CC_REG_IOCQES_SHIFT; /* CQ entry size == 16 == 2^4 */

	/*
	 * Use the Memory Page Size selected during device initialization.  Note
	 * that value stored in mps is suitable to use here without adjusting by
	 * NVME_MPS_SHIFT.
	 */
	cc |= ctrlr->mps << NVME_CC_REG_MPS_SHIFT;

	nvme_ctrlr_barrier(ctrlr, BUS_SPACE_BARRIER_WRITE);
	nvme_mmio_write_4(ctrlr, cc, cc);

	return (nvme_ctrlr_wait_for_ready(ctrlr, 1));
}

static void
nvme_ctrlr_disable_qpairs(struct nvme_controller *ctrlr)
{
	int i;

	nvme_admin_qpair_disable(&ctrlr->adminq);
	/*
	 * I/O queues are not allocated before the initial HW
	 *  reset, so do not try to disable them.  Use is_initialized
	 *  to determine if this is the initial HW reset.
	 */
	if (ctrlr->is_initialized) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			nvme_io_qpair_disable(&ctrlr->ioq[i]);
	}
}

static int
nvme_ctrlr_hw_reset(struct nvme_controller *ctrlr)
{
	int err;

	TSENTER();

	nvme_ctrlr_disable_qpairs(ctrlr);

	err = nvme_ctrlr_disable(ctrlr);
	if (err != 0)
		return err;

	err = nvme_ctrlr_enable(ctrlr);
	TSEXIT();
	return (err);
}

void
nvme_ctrlr_reset(struct nvme_controller *ctrlr)
{
	int cmpset;

	cmpset = atomic_cmpset_32(&ctrlr->is_resetting, 0, 1);

	if (cmpset == 0 || ctrlr->is_failed)
		/*
		 * Controller is already resetting or has failed.  Return
		 *  immediately since there is no need to kick off another
		 *  reset in these cases.
		 */
		return;

	if (!ctrlr->is_dying)
		taskqueue_enqueue(ctrlr->taskqueue, &ctrlr->reset_task);
}

static int
nvme_ctrlr_identify(struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;

	status.done = 0;
	nvme_ctrlr_cmd_identify_controller(ctrlr, &ctrlr->cdata,
	    nvme_completion_poll_cb, &status);
	nvme_completion_poll(&status);
	if (nvme_completion_is_error(&status.cpl)) {
		nvme_printf(ctrlr, "nvme_identify_controller failed!\n");
		return (ENXIO);
	}

	/* Convert data to host endian */
	nvme_controller_data_swapbytes(&ctrlr->cdata);

	/*
	 * Use MDTS to ensure our default max_xfer_size doesn't exceed what the
	 *  controller supports.
	 */
	if (ctrlr->cdata.mdts > 0)
		ctrlr->max_xfer_size = min(ctrlr->max_xfer_size,
		    1 << (ctrlr->cdata.mdts + NVME_MPS_SHIFT +
			NVME_CAP_HI_MPSMIN(ctrlr->cap_hi)));

	return (0);
}

static int
nvme_ctrlr_set_num_qpairs(struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;
	int					cq_allocated, sq_allocated;

	status.done = 0;
	nvme_ctrlr_cmd_set_num_queues(ctrlr, ctrlr->num_io_queues,
	    nvme_completion_poll_cb, &status);
	nvme_completion_poll(&status);
	if (nvme_completion_is_error(&status.cpl)) {
		nvme_printf(ctrlr, "nvme_ctrlr_set_num_qpairs failed!\n");
		return (ENXIO);
	}

	/*
	 * Data in cdw0 is 0-based.
	 * Lower 16-bits indicate number of submission queues allocated.
	 * Upper 16-bits indicate number of completion queues allocated.
	 */
	sq_allocated = (status.cpl.cdw0 & 0xFFFF) + 1;
	cq_allocated = (status.cpl.cdw0 >> 16) + 1;

	/*
	 * Controller may allocate more queues than we requested,
	 *  so use the minimum of the number requested and what was
	 *  actually allocated.
	 */
	ctrlr->num_io_queues = min(ctrlr->num_io_queues, sq_allocated);
	ctrlr->num_io_queues = min(ctrlr->num_io_queues, cq_allocated);
	if (ctrlr->num_io_queues > vm_ndomains)
		ctrlr->num_io_queues -= ctrlr->num_io_queues % vm_ndomains;

	return (0);
}

static int
nvme_ctrlr_create_qpairs(struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;
	struct nvme_qpair			*qpair;
	int					i;

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		qpair = &ctrlr->ioq[i];

		status.done = 0;
		nvme_ctrlr_cmd_create_io_cq(ctrlr, qpair,
		    nvme_completion_poll_cb, &status);
		nvme_completion_poll(&status);
		if (nvme_completion_is_error(&status.cpl)) {
			nvme_printf(ctrlr, "nvme_create_io_cq failed!\n");
			return (ENXIO);
		}

		status.done = 0;
		nvme_ctrlr_cmd_create_io_sq(ctrlr, qpair,
		    nvme_completion_poll_cb, &status);
		nvme_completion_poll(&status);
		if (nvme_completion_is_error(&status.cpl)) {
			nvme_printf(ctrlr, "nvme_create_io_sq failed!\n");
			return (ENXIO);
		}
	}

	return (0);
}

static int
nvme_ctrlr_delete_qpairs(struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;
	struct nvme_qpair			*qpair;

	for (int i = 0; i < ctrlr->num_io_queues; i++) {
		qpair = &ctrlr->ioq[i];

		status.done = 0;
		nvme_ctrlr_cmd_delete_io_sq(ctrlr, qpair,
		    nvme_completion_poll_cb, &status);
		nvme_completion_poll(&status);
		if (nvme_completion_is_error(&status.cpl)) {
			nvme_printf(ctrlr, "nvme_destroy_io_sq failed!\n");
			return (ENXIO);
		}

		status.done = 0;
		nvme_ctrlr_cmd_delete_io_cq(ctrlr, qpair,
		    nvme_completion_poll_cb, &status);
		nvme_completion_poll(&status);
		if (nvme_completion_is_error(&status.cpl)) {
			nvme_printf(ctrlr, "nvme_destroy_io_cq failed!\n");
			return (ENXIO);
		}
	}

	return (0);
}

static int
nvme_ctrlr_construct_namespaces(struct nvme_controller *ctrlr)
{
	struct nvme_namespace	*ns;
	uint32_t 		i;

	for (i = 0; i < min(ctrlr->cdata.nn, NVME_MAX_NAMESPACES); i++) {
		ns = &ctrlr->ns[i];
		nvme_ns_construct(ns, i+1, ctrlr);
	}

	return (0);
}

static bool
is_log_page_id_valid(uint8_t page_id)
{

	switch (page_id) {
	case NVME_LOG_ERROR:
	case NVME_LOG_HEALTH_INFORMATION:
	case NVME_LOG_FIRMWARE_SLOT:
	case NVME_LOG_CHANGED_NAMESPACE:
	case NVME_LOG_COMMAND_EFFECT:
	case NVME_LOG_RES_NOTIFICATION:
	case NVME_LOG_SANITIZE_STATUS:
		return (true);
	}

	return (false);
}

static uint32_t
nvme_ctrlr_get_log_page_size(struct nvme_controller *ctrlr, uint8_t page_id)
{
	uint32_t	log_page_size;

	switch (page_id) {
	case NVME_LOG_ERROR:
		log_page_size = min(
		    sizeof(struct nvme_error_information_entry) *
		    (ctrlr->cdata.elpe + 1), NVME_MAX_AER_LOG_SIZE);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
		log_page_size = sizeof(struct nvme_health_information_page);
		break;
	case NVME_LOG_FIRMWARE_SLOT:
		log_page_size = sizeof(struct nvme_firmware_page);
		break;
	case NVME_LOG_CHANGED_NAMESPACE:
		log_page_size = sizeof(struct nvme_ns_list);
		break;
	case NVME_LOG_COMMAND_EFFECT:
		log_page_size = sizeof(struct nvme_command_effects_page);
		break;
	case NVME_LOG_RES_NOTIFICATION:
		log_page_size = sizeof(struct nvme_res_notification_page);
		break;
	case NVME_LOG_SANITIZE_STATUS:
		log_page_size = sizeof(struct nvme_sanitize_status_page);
		break;
	default:
		log_page_size = 0;
		break;
	}

	return (log_page_size);
}

static void
nvme_ctrlr_log_critical_warnings(struct nvme_controller *ctrlr,
    uint8_t state)
{

	if (state & NVME_CRIT_WARN_ST_AVAILABLE_SPARE)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "available spare space below threshold");

	if (state & NVME_CRIT_WARN_ST_TEMPERATURE)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "temperature above threshold");

	if (state & NVME_CRIT_WARN_ST_DEVICE_RELIABILITY)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "device reliability degraded");

	if (state & NVME_CRIT_WARN_ST_READ_ONLY)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "media placed in read only mode");

	if (state & NVME_CRIT_WARN_ST_VOLATILE_MEMORY_BACKUP)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "volatile memory backup device failed");

	if (state & NVME_CRIT_WARN_ST_RESERVED_MASK)
		nvme_ctrlr_devctl_log(ctrlr, "critical",
		    "unknown critical warning(s): state = 0x%02x", state);
}

static void
nvme_ctrlr_async_event_log_page_cb(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_async_event_request		*aer = arg;
	struct nvme_health_information_page	*health_info;
	struct nvme_ns_list			*nsl;
	struct nvme_error_information_entry	*err;
	int i;

	/*
	 * If the log page fetch for some reason completed with an error,
	 *  don't pass log page data to the consumers.  In practice, this case
	 *  should never happen.
	 */
	if (nvme_completion_is_error(cpl))
		nvme_notify_async_consumers(aer->ctrlr, &aer->cpl,
		    aer->log_page_id, NULL, 0);
	else {
		/* Convert data to host endian */
		switch (aer->log_page_id) {
		case NVME_LOG_ERROR:
			err = (struct nvme_error_information_entry *)aer->log_page_buffer;
			for (i = 0; i < (aer->ctrlr->cdata.elpe + 1); i++)
				nvme_error_information_entry_swapbytes(err++);
			break;
		case NVME_LOG_HEALTH_INFORMATION:
			nvme_health_information_page_swapbytes(
			    (struct nvme_health_information_page *)aer->log_page_buffer);
			break;
		case NVME_LOG_FIRMWARE_SLOT:
			nvme_firmware_page_swapbytes(
			    (struct nvme_firmware_page *)aer->log_page_buffer);
			break;
		case NVME_LOG_CHANGED_NAMESPACE:
			nvme_ns_list_swapbytes(
			    (struct nvme_ns_list *)aer->log_page_buffer);
			break;
		case NVME_LOG_COMMAND_EFFECT:
			nvme_command_effects_page_swapbytes(
			    (struct nvme_command_effects_page *)aer->log_page_buffer);
			break;
		case NVME_LOG_RES_NOTIFICATION:
			nvme_res_notification_page_swapbytes(
			    (struct nvme_res_notification_page *)aer->log_page_buffer);
			break;
		case NVME_LOG_SANITIZE_STATUS:
			nvme_sanitize_status_page_swapbytes(
			    (struct nvme_sanitize_status_page *)aer->log_page_buffer);
			break;
		case INTEL_LOG_TEMP_STATS:
			intel_log_temp_stats_swapbytes(
			    (struct intel_log_temp_stats *)aer->log_page_buffer);
			break;
		default:
			break;
		}

		if (aer->log_page_id == NVME_LOG_HEALTH_INFORMATION) {
			health_info = (struct nvme_health_information_page *)
			    aer->log_page_buffer;
			nvme_ctrlr_log_critical_warnings(aer->ctrlr,
			    health_info->critical_warning);
			/*
			 * Critical warnings reported through the
			 *  SMART/health log page are persistent, so
			 *  clear the associated bits in the async event
			 *  config so that we do not receive repeated
			 *  notifications for the same event.
			 */
			aer->ctrlr->async_event_config &=
			    ~health_info->critical_warning;
			nvme_ctrlr_cmd_set_async_event_config(aer->ctrlr,
			    aer->ctrlr->async_event_config, NULL, NULL);
		} else if (aer->log_page_id == NVME_LOG_CHANGED_NAMESPACE &&
		    !nvme_use_nvd) {
			nsl = (struct nvme_ns_list *)aer->log_page_buffer;
			for (i = 0; i < nitems(nsl->ns) && nsl->ns[i] != 0; i++) {
				if (nsl->ns[i] > NVME_MAX_NAMESPACES)
					break;
				nvme_notify_ns(aer->ctrlr, nsl->ns[i]);
			}
		}

		/*
		 * Pass the cpl data from the original async event completion,
		 *  not the log page fetch.
		 */
		nvme_notify_async_consumers(aer->ctrlr, &aer->cpl,
		    aer->log_page_id, aer->log_page_buffer, aer->log_page_size);
	}

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

	if (nvme_completion_is_error(cpl)) {
		/*
		 *  Do not retry failed async event requests.  This avoids
		 *  infinite loops where a new async event request is submitted
		 *  to replace the one just failed, only to fail again and
		 *  perpetuate the loop.
		 */
		return;
	}

	/* Associated log page is in bits 23:16 of completion entry dw0. */
	aer->log_page_id = (cpl->cdw0 & 0xFF0000) >> 16;

	nvme_printf(aer->ctrlr, "async event occurred (type 0x%x, info 0x%02x,"
	    " page 0x%02x)\n", (cpl->cdw0 & 0x07), (cpl->cdw0 & 0xFF00) >> 8,
	    aer->log_page_id);

	if (is_log_page_id_valid(aer->log_page_id)) {
		aer->log_page_size = nvme_ctrlr_get_log_page_size(aer->ctrlr,
		    aer->log_page_id);
		memcpy(&aer->cpl, cpl, sizeof(*cpl));
		nvme_ctrlr_cmd_get_log_page(aer->ctrlr, aer->log_page_id,
		    NVME_GLOBAL_NAMESPACE_TAG, aer->log_page_buffer,
		    aer->log_page_size, nvme_ctrlr_async_event_log_page_cb,
		    aer);
		/* Wait to notify consumers until after log page is fetched. */
	} else {
		nvme_notify_async_consumers(aer->ctrlr, cpl, aer->log_page_id,
		    NULL, 0);

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
	req = nvme_allocate_request_null(nvme_ctrlr_async_event_cb, aer);
	aer->req = req;

	/*
	 * Disable timeout here, since asynchronous event requests should by
	 *  nature never be timed out.
	 */
	req->timeout = false;
	req->cmd.opc = NVME_OPC_ASYNC_EVENT_REQUEST;
	nvme_ctrlr_submit_admin_request(ctrlr, req);
}

static void
nvme_ctrlr_configure_aer(struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;
	struct nvme_async_event_request		*aer;
	uint32_t				i;

	ctrlr->async_event_config = NVME_CRIT_WARN_ST_AVAILABLE_SPARE |
	    NVME_CRIT_WARN_ST_DEVICE_RELIABILITY |
	    NVME_CRIT_WARN_ST_READ_ONLY |
	    NVME_CRIT_WARN_ST_VOLATILE_MEMORY_BACKUP;
	if (ctrlr->cdata.ver >= NVME_REV(1, 2))
		ctrlr->async_event_config |= NVME_ASYNC_EVENT_NS_ATTRIBUTE |
		    NVME_ASYNC_EVENT_FW_ACTIVATE;

	status.done = 0;
	nvme_ctrlr_cmd_get_feature(ctrlr, NVME_FEAT_TEMPERATURE_THRESHOLD,
	    0, NULL, 0, nvme_completion_poll_cb, &status);
	nvme_completion_poll(&status);
	if (nvme_completion_is_error(&status.cpl) ||
	    (status.cpl.cdw0 & 0xFFFF) == 0xFFFF ||
	    (status.cpl.cdw0 & 0xFFFF) == 0x0000) {
		nvme_printf(ctrlr, "temperature threshold not supported\n");
	} else
		ctrlr->async_event_config |= NVME_CRIT_WARN_ST_TEMPERATURE;

	nvme_ctrlr_cmd_set_async_event_config(ctrlr,
	    ctrlr->async_event_config, NULL, NULL);

	/* aerl is a zero-based value, so we need to add 1 here. */
	ctrlr->num_aers = min(NVME_MAX_ASYNC_EVENTS, (ctrlr->cdata.aerl+1));

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

static void
nvme_ctrlr_hmb_free(struct nvme_controller *ctrlr)
{
	struct nvme_hmb_chunk *hmbc;
	int i;

	if (ctrlr->hmb_desc_paddr) {
		bus_dmamap_unload(ctrlr->hmb_desc_tag, ctrlr->hmb_desc_map);
		bus_dmamem_free(ctrlr->hmb_desc_tag, ctrlr->hmb_desc_vaddr,
		    ctrlr->hmb_desc_map);
		ctrlr->hmb_desc_paddr = 0;
	}
	if (ctrlr->hmb_desc_tag) {
		bus_dma_tag_destroy(ctrlr->hmb_desc_tag);
		ctrlr->hmb_desc_tag = NULL;
	}
	for (i = 0; i < ctrlr->hmb_nchunks; i++) {
		hmbc = &ctrlr->hmb_chunks[i];
		bus_dmamap_unload(ctrlr->hmb_tag, hmbc->hmbc_map);
		bus_dmamem_free(ctrlr->hmb_tag, hmbc->hmbc_vaddr,
		    hmbc->hmbc_map);
	}
	ctrlr->hmb_nchunks = 0;
	if (ctrlr->hmb_tag) {
		bus_dma_tag_destroy(ctrlr->hmb_tag);
		ctrlr->hmb_tag = NULL;
	}
	if (ctrlr->hmb_chunks) {
		free(ctrlr->hmb_chunks, M_NVME);
		ctrlr->hmb_chunks = NULL;
	}
}

static void
nvme_ctrlr_hmb_alloc(struct nvme_controller *ctrlr)
{
	struct nvme_hmb_chunk *hmbc;
	size_t pref, min, minc, size;
	int err, i;
	uint64_t max;

	/* Limit HMB to 5% of RAM size per device by default. */
	max = (uint64_t)physmem * PAGE_SIZE / 20;
	TUNABLE_UINT64_FETCH("hw.nvme.hmb_max", &max);

	/*
	 * Units of Host Memory Buffer in the Identify info are always in terms
	 * of 4k units.
	 */
	min = (long long unsigned)ctrlr->cdata.hmmin * NVME_HMB_UNITS;
	if (max == 0 || max < min)
		return;
	pref = MIN((long long unsigned)ctrlr->cdata.hmpre * NVME_HMB_UNITS, max);
	minc = MAX(ctrlr->cdata.hmminds * NVME_HMB_UNITS, ctrlr->page_size);
	if (min > 0 && ctrlr->cdata.hmmaxd > 0)
		minc = MAX(minc, min / ctrlr->cdata.hmmaxd);
	ctrlr->hmb_chunk = pref;

again:
	/*
	 * However, the chunk sizes, number of chunks, and alignment of chunks
	 * are all based on the current MPS (ctrlr->page_size).
	 */
	ctrlr->hmb_chunk = roundup2(ctrlr->hmb_chunk, ctrlr->page_size);
	ctrlr->hmb_nchunks = howmany(pref, ctrlr->hmb_chunk);
	if (ctrlr->cdata.hmmaxd > 0 && ctrlr->hmb_nchunks > ctrlr->cdata.hmmaxd)
		ctrlr->hmb_nchunks = ctrlr->cdata.hmmaxd;
	ctrlr->hmb_chunks = malloc(sizeof(struct nvme_hmb_chunk) *
	    ctrlr->hmb_nchunks, M_NVME, M_WAITOK);
	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    ctrlr->page_size, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    ctrlr->hmb_chunk, 1, ctrlr->hmb_chunk, 0, NULL, NULL, &ctrlr->hmb_tag);
	if (err != 0) {
		nvme_printf(ctrlr, "HMB tag create failed %d\n", err);
		nvme_ctrlr_hmb_free(ctrlr);
		return;
	}

	for (i = 0; i < ctrlr->hmb_nchunks; i++) {
		hmbc = &ctrlr->hmb_chunks[i];
		if (bus_dmamem_alloc(ctrlr->hmb_tag,
		    (void **)&hmbc->hmbc_vaddr, BUS_DMA_NOWAIT,
		    &hmbc->hmbc_map)) {
			nvme_printf(ctrlr, "failed to alloc HMB\n");
			break;
		}
		if (bus_dmamap_load(ctrlr->hmb_tag, hmbc->hmbc_map,
		    hmbc->hmbc_vaddr, ctrlr->hmb_chunk, nvme_single_map,
		    &hmbc->hmbc_paddr, BUS_DMA_NOWAIT) != 0) {
			bus_dmamem_free(ctrlr->hmb_tag, hmbc->hmbc_vaddr,
			    hmbc->hmbc_map);
			nvme_printf(ctrlr, "failed to load HMB\n");
			break;
		}
		bus_dmamap_sync(ctrlr->hmb_tag, hmbc->hmbc_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	if (i < ctrlr->hmb_nchunks && i * ctrlr->hmb_chunk < min &&
	    ctrlr->hmb_chunk / 2 >= minc) {
		ctrlr->hmb_nchunks = i;
		nvme_ctrlr_hmb_free(ctrlr);
		ctrlr->hmb_chunk /= 2;
		goto again;
	}
	ctrlr->hmb_nchunks = i;
	if (ctrlr->hmb_nchunks * ctrlr->hmb_chunk < min) {
		nvme_ctrlr_hmb_free(ctrlr);
		return;
	}

	size = sizeof(struct nvme_hmb_desc) * ctrlr->hmb_nchunks;
	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    16, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, 0, NULL, NULL, &ctrlr->hmb_desc_tag);
	if (err != 0) {
		nvme_printf(ctrlr, "HMB desc tag create failed %d\n", err);
		nvme_ctrlr_hmb_free(ctrlr);
		return;
	}
	if (bus_dmamem_alloc(ctrlr->hmb_desc_tag,
	    (void **)&ctrlr->hmb_desc_vaddr, BUS_DMA_WAITOK,
	    &ctrlr->hmb_desc_map)) {
		nvme_printf(ctrlr, "failed to alloc HMB desc\n");
		nvme_ctrlr_hmb_free(ctrlr);
		return;
	}
	if (bus_dmamap_load(ctrlr->hmb_desc_tag, ctrlr->hmb_desc_map,
	    ctrlr->hmb_desc_vaddr, size, nvme_single_map,
	    &ctrlr->hmb_desc_paddr, BUS_DMA_NOWAIT) != 0) {
		bus_dmamem_free(ctrlr->hmb_desc_tag, ctrlr->hmb_desc_vaddr,
		    ctrlr->hmb_desc_map);
		nvme_printf(ctrlr, "failed to load HMB desc\n");
		nvme_ctrlr_hmb_free(ctrlr);
		return;
	}

	for (i = 0; i < ctrlr->hmb_nchunks; i++) {
		ctrlr->hmb_desc_vaddr[i].addr =
		    htole64(ctrlr->hmb_chunks[i].hmbc_paddr);
		ctrlr->hmb_desc_vaddr[i].size = htole32(ctrlr->hmb_chunk / ctrlr->page_size);
	}
	bus_dmamap_sync(ctrlr->hmb_desc_tag, ctrlr->hmb_desc_map,
	    BUS_DMASYNC_PREWRITE);

	nvme_printf(ctrlr, "Allocated %lluMB host memory buffer\n",
	    (long long unsigned)ctrlr->hmb_nchunks * ctrlr->hmb_chunk
	    / 1024 / 1024);
}

static void
nvme_ctrlr_hmb_enable(struct nvme_controller *ctrlr, bool enable, bool memret)
{
	struct nvme_completion_poll_status	status;
	uint32_t cdw11;

	cdw11 = 0;
	if (enable)
		cdw11 |= 1;
	if (memret)
		cdw11 |= 2;
	status.done = 0;
	nvme_ctrlr_cmd_set_feature(ctrlr, NVME_FEAT_HOST_MEMORY_BUFFER, cdw11,
	    ctrlr->hmb_nchunks * ctrlr->hmb_chunk / ctrlr->page_size,
	    ctrlr->hmb_desc_paddr, ctrlr->hmb_desc_paddr >> 32,
	    ctrlr->hmb_nchunks, NULL, 0,
	    nvme_completion_poll_cb, &status);
	nvme_completion_poll(&status);
	if (nvme_completion_is_error(&status.cpl))
		nvme_printf(ctrlr, "nvme_ctrlr_hmb_enable failed!\n");
}

static void
nvme_ctrlr_start(void *ctrlr_arg, bool resetting)
{
	struct nvme_controller *ctrlr = ctrlr_arg;
	uint32_t old_num_io_queues;
	int i;

	TSENTER();

	/*
	 * Only reset adminq here when we are restarting the
	 *  controller after a reset.  During initialization,
	 *  we have already submitted admin commands to get
	 *  the number of I/O queues supported, so cannot reset
	 *  the adminq again here.
	 */
	if (resetting) {
		nvme_qpair_reset(&ctrlr->adminq);
		nvme_admin_qpair_enable(&ctrlr->adminq);
	}

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			nvme_qpair_reset(&ctrlr->ioq[i]);
	}

	/*
	 * If it was a reset on initialization command timeout, just
	 * return here, letting initialization code fail gracefully.
	 */
	if (resetting && !ctrlr->is_initialized)
		return;

	if (resetting && nvme_ctrlr_identify(ctrlr) != 0) {
		nvme_ctrlr_fail(ctrlr);
		return;
	}

	/*
	 * The number of qpairs are determined during controller initialization,
	 *  including using NVMe SET_FEATURES/NUMBER_OF_QUEUES to determine the
	 *  HW limit.  We call SET_FEATURES again here so that it gets called
	 *  after any reset for controllers that depend on the driver to
	 *  explicit specify how many queues it will use.  This value should
	 *  never change between resets, so panic if somehow that does happen.
	 */
	if (resetting) {
		old_num_io_queues = ctrlr->num_io_queues;
		if (nvme_ctrlr_set_num_qpairs(ctrlr) != 0) {
			nvme_ctrlr_fail(ctrlr);
			return;
		}

		if (old_num_io_queues != ctrlr->num_io_queues) {
			panic("num_io_queues changed from %u to %u",
			      old_num_io_queues, ctrlr->num_io_queues);
		}
	}

	if (ctrlr->cdata.hmpre > 0 && ctrlr->hmb_nchunks == 0) {
		nvme_ctrlr_hmb_alloc(ctrlr);
		if (ctrlr->hmb_nchunks > 0)
			nvme_ctrlr_hmb_enable(ctrlr, true, false);
	} else if (ctrlr->hmb_nchunks > 0)
		nvme_ctrlr_hmb_enable(ctrlr, true, true);

	if (nvme_ctrlr_create_qpairs(ctrlr) != 0) {
		nvme_ctrlr_fail(ctrlr);
		return;
	}

	if (nvme_ctrlr_construct_namespaces(ctrlr) != 0) {
		nvme_ctrlr_fail(ctrlr);
		return;
	}

	nvme_ctrlr_configure_aer(ctrlr);
	nvme_ctrlr_configure_int_coalescing(ctrlr);

	for (i = 0; i < ctrlr->num_io_queues; i++)
		nvme_io_qpair_enable(&ctrlr->ioq[i]);
	TSEXIT();
}

void
nvme_ctrlr_start_config_hook(void *arg)
{
	struct nvme_controller *ctrlr = arg;

	TSENTER();

	if (nvme_ctrlr_hw_reset(ctrlr) != 0) {
fail:
		nvme_ctrlr_fail(ctrlr);
		config_intrhook_disestablish(&ctrlr->config_hook);
		return;
	}

#ifdef NVME_2X_RESET
	/*
	 * Reset controller twice to ensure we do a transition from cc.en==1 to
	 * cc.en==0.  This is because we don't really know what status the
	 * controller was left in when boot handed off to OS.  Linux doesn't do
	 * this, however, and when the controller is in state cc.en == 0, no
	 * I/O can happen.
	 */
	if (nvme_ctrlr_hw_reset(ctrlr) != 0)
		goto fail;
#endif

	nvme_qpair_reset(&ctrlr->adminq);
	nvme_admin_qpair_enable(&ctrlr->adminq);

	if (nvme_ctrlr_identify(ctrlr) == 0 &&
	    nvme_ctrlr_set_num_qpairs(ctrlr) == 0 &&
	    nvme_ctrlr_construct_io_qpairs(ctrlr) == 0)
		nvme_ctrlr_start(ctrlr, false);
	else
		goto fail;

	nvme_sysctl_initialize_ctrlr(ctrlr);
	config_intrhook_disestablish(&ctrlr->config_hook);

	ctrlr->is_initialized = 1;
	nvme_notify_new_controller(ctrlr);
	TSEXIT();
}

static void
nvme_ctrlr_reset_task(void *arg, int pending)
{
	struct nvme_controller	*ctrlr = arg;
	int			status;

	nvme_ctrlr_devctl_log(ctrlr, "RESET", "resetting controller");
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
		nvme_ctrlr_start(ctrlr, true);
	else
		nvme_ctrlr_fail(ctrlr);

	atomic_cmpset_32(&ctrlr->is_resetting, 1, 0);
}

/*
 * Poll all the queues enabled on the device for completion.
 */
void
nvme_ctrlr_poll(struct nvme_controller *ctrlr)
{
	int i;

	nvme_qpair_process_completions(&ctrlr->adminq);

	for (i = 0; i < ctrlr->num_io_queues; i++)
		if (ctrlr->ioq && ctrlr->ioq[i].cpl)
			nvme_qpair_process_completions(&ctrlr->ioq[i]);
}

/*
 * Poll the single-vector interrupt case: num_io_queues will be 1 and
 * there's only a single vector. While we're polling, we mask further
 * interrupts in the controller.
 */
void
nvme_ctrlr_shared_handler(void *arg)
{
	struct nvme_controller *ctrlr = arg;

	nvme_mmio_write_4(ctrlr, intms, 1);
	nvme_ctrlr_poll(ctrlr);
	nvme_mmio_write_4(ctrlr, intmc, 1);
}

static void
nvme_pt_done(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_pt_command *pt = arg;
	struct mtx *mtx = pt->driver_lock;
	uint16_t status;

	bzero(&pt->cpl, sizeof(pt->cpl));
	pt->cpl.cdw0 = cpl->cdw0;

	status = cpl->status;
	status &= ~NVME_STATUS_P_MASK;
	pt->cpl.status = status;

	mtx_lock(mtx);
	pt->driver_lock = NULL;
	wakeup(pt);
	mtx_unlock(mtx);
}

int
nvme_ctrlr_passthrough_cmd(struct nvme_controller *ctrlr,
    struct nvme_pt_command *pt, uint32_t nsid, int is_user_buffer,
    int is_admin_cmd)
{
	struct nvme_request	*req;
	struct mtx		*mtx;
	struct buf		*buf = NULL;
	int			ret = 0;

	if (pt->len > 0) {
		if (pt->len > ctrlr->max_xfer_size) {
			nvme_printf(ctrlr, "pt->len (%d) "
			    "exceeds max_xfer_size (%d)\n", pt->len,
			    ctrlr->max_xfer_size);
			return EIO;
		}
		if (is_user_buffer) {
			/*
			 * Ensure the user buffer is wired for the duration of
			 *  this pass-through command.
			 */
			PHOLD(curproc);
			buf = uma_zalloc(pbuf_zone, M_WAITOK);
			buf->b_iocmd = pt->is_read ? BIO_READ : BIO_WRITE;
			if (vmapbuf(buf, pt->buf, pt->len, 1) < 0) {
				ret = EFAULT;
				goto err;
			}
			req = nvme_allocate_request_vaddr(buf->b_data, pt->len, 
			    nvme_pt_done, pt);
		} else
			req = nvme_allocate_request_vaddr(pt->buf, pt->len,
			    nvme_pt_done, pt);
	} else
		req = nvme_allocate_request_null(nvme_pt_done, pt);

	/* Assume user space already converted to little-endian */
	req->cmd.opc = pt->cmd.opc;
	req->cmd.fuse = pt->cmd.fuse;
	req->cmd.rsvd2 = pt->cmd.rsvd2;
	req->cmd.rsvd3 = pt->cmd.rsvd3;
	req->cmd.cdw10 = pt->cmd.cdw10;
	req->cmd.cdw11 = pt->cmd.cdw11;
	req->cmd.cdw12 = pt->cmd.cdw12;
	req->cmd.cdw13 = pt->cmd.cdw13;
	req->cmd.cdw14 = pt->cmd.cdw14;
	req->cmd.cdw15 = pt->cmd.cdw15;

	req->cmd.nsid = htole32(nsid);

	mtx = mtx_pool_find(mtxpool_sleep, pt);
	pt->driver_lock = mtx;

	if (is_admin_cmd)
		nvme_ctrlr_submit_admin_request(ctrlr, req);
	else
		nvme_ctrlr_submit_io_request(ctrlr, req);

	mtx_lock(mtx);
	while (pt->driver_lock != NULL)
		mtx_sleep(pt, mtx, PRIBIO, "nvme_pt", 0);
	mtx_unlock(mtx);

err:
	if (buf != NULL) {
		uma_zfree(pbuf_zone, buf);
		PRELE(curproc);
	}

	return (ret);
}

static int
nvme_ctrlr_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct nvme_controller			*ctrlr;
	struct nvme_pt_command			*pt;

	ctrlr = cdev->si_drv1;

	switch (cmd) {
	case NVME_RESET_CONTROLLER:
		nvme_ctrlr_reset(ctrlr);
		break;
	case NVME_PASSTHROUGH_CMD:
		pt = (struct nvme_pt_command *)arg;
		return (nvme_ctrlr_passthrough_cmd(ctrlr, pt, le32toh(pt->cmd.nsid),
		    1 /* is_user_buffer */, 1 /* is_admin_cmd */));
	case NVME_GET_NSID:
	{
		struct nvme_get_nsid *gnsid = (struct nvme_get_nsid *)arg;
		strncpy(gnsid->cdev, device_get_nameunit(ctrlr->dev),
		    sizeof(gnsid->cdev));
		gnsid->cdev[sizeof(gnsid->cdev) - 1] = '\0';
		gnsid->nsid = 0;
		break;
	}
	case NVME_GET_MAX_XFER_SIZE:
		*(uint64_t *)arg = ctrlr->max_xfer_size;
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
	struct make_dev_args	md_args;
	uint32_t	cap_lo;
	uint32_t	cap_hi;
	uint32_t	to, vs, pmrcap;
	int		status, timeout_period;

	ctrlr->dev = dev;

	mtx_init(&ctrlr->lock, "nvme ctrlr lock", NULL, MTX_DEF);
	if (bus_get_domain(dev, &ctrlr->domain) != 0)
		ctrlr->domain = 0;

	ctrlr->cap_lo = cap_lo = nvme_mmio_read_4(ctrlr, cap_lo);
	if (bootverbose) {
		device_printf(dev, "CapLo: 0x%08x: MQES %u%s%s%s%s, TO %u\n",
		    cap_lo, NVME_CAP_LO_MQES(cap_lo),
		    NVME_CAP_LO_CQR(cap_lo) ? ", CQR" : "",
		    NVME_CAP_LO_AMS(cap_lo) ? ", AMS" : "",
		    (NVME_CAP_LO_AMS(cap_lo) & 0x1) ? " WRRwUPC" : "",
		    (NVME_CAP_LO_AMS(cap_lo) & 0x2) ? " VS" : "",
		    NVME_CAP_LO_TO(cap_lo));
	}
	ctrlr->cap_hi = cap_hi = nvme_mmio_read_4(ctrlr, cap_hi);
	if (bootverbose) {
		device_printf(dev, "CapHi: 0x%08x: DSTRD %u%s, CSS %x%s, "
		    "MPSMIN %u, MPSMAX %u%s%s\n", cap_hi,
		    NVME_CAP_HI_DSTRD(cap_hi),
		    NVME_CAP_HI_NSSRS(cap_hi) ? ", NSSRS" : "",
		    NVME_CAP_HI_CSS(cap_hi),
		    NVME_CAP_HI_BPS(cap_hi) ? ", BPS" : "",
		    NVME_CAP_HI_MPSMIN(cap_hi),
		    NVME_CAP_HI_MPSMAX(cap_hi),
		    NVME_CAP_HI_PMRS(cap_hi) ? ", PMRS" : "",
		    NVME_CAP_HI_CMBS(cap_hi) ? ", CMBS" : "");
	}
	if (bootverbose) {
		vs = nvme_mmio_read_4(ctrlr, vs);
		device_printf(dev, "Version: 0x%08x: %d.%d\n", vs,
		    NVME_MAJOR(vs), NVME_MINOR(vs));
	}
	if (bootverbose && NVME_CAP_HI_PMRS(cap_hi)) {
		pmrcap = nvme_mmio_read_4(ctrlr, pmrcap);
		device_printf(dev, "PMRCap: 0x%08x: BIR %u%s%s, PMRTU %u, "
		    "PMRWBM %x, PMRTO %u%s\n", pmrcap,
		    NVME_PMRCAP_BIR(pmrcap),
		    NVME_PMRCAP_RDS(pmrcap) ? ", RDS" : "",
		    NVME_PMRCAP_WDS(pmrcap) ? ", WDS" : "",
		    NVME_PMRCAP_PMRTU(pmrcap),
		    NVME_PMRCAP_PMRWBM(pmrcap),
		    NVME_PMRCAP_PMRTO(pmrcap),
		    NVME_PMRCAP_CMSS(pmrcap) ? ", CMSS" : "");
	}

	ctrlr->dstrd = NVME_CAP_HI_DSTRD(cap_hi) + 2;

	ctrlr->mps = NVME_CAP_HI_MPSMIN(cap_hi);
	ctrlr->page_size = 1 << (NVME_MPS_SHIFT + ctrlr->mps);

	/* Get ready timeout value from controller, in units of 500ms. */
	to = NVME_CAP_LO_TO(cap_lo) + 1;
	ctrlr->ready_timeout_in_ms = to * 500;

	timeout_period = NVME_DEFAULT_TIMEOUT_PERIOD;
	TUNABLE_INT_FETCH("hw.nvme.timeout_period", &timeout_period);
	timeout_period = min(timeout_period, NVME_MAX_TIMEOUT_PERIOD);
	timeout_period = max(timeout_period, NVME_MIN_TIMEOUT_PERIOD);
	ctrlr->timeout_period = timeout_period;

	nvme_retry_count = NVME_DEFAULT_RETRY_COUNT;
	TUNABLE_INT_FETCH("hw.nvme.retry_count", &nvme_retry_count);

	ctrlr->enable_aborts = 0;
	TUNABLE_INT_FETCH("hw.nvme.enable_aborts", &ctrlr->enable_aborts);

	/* Cap transfers by the maximum addressable by page-sized PRP (4KB pages -> 2MB). */
	ctrlr->max_xfer_size = MIN(maxphys, (ctrlr->page_size / 8 * ctrlr->page_size));
	if (nvme_ctrlr_construct_admin_qpair(ctrlr) != 0)
		return (ENXIO);

	/*
	 * Create 2 threads for the taskqueue. The reset thread will block when
	 * it detects that the controller has failed until all I/O has been
	 * failed up the stack. The fail_req task needs to be able to run in
	 * this case to finish the request failure for some cases.
	 *
	 * We could partially solve this race by draining the failed requeust
	 * queue before proceding to free the sim, though nothing would stop
	 * new I/O from coming in after we do that drain, but before we reach
	 * cam_sim_free, so this big hammer is used instead.
	 */
	ctrlr->taskqueue = taskqueue_create("nvme_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &ctrlr->taskqueue);
	taskqueue_start_threads(&ctrlr->taskqueue, 2, PI_DISK, "nvme taskq");

	ctrlr->is_resetting = 0;
	ctrlr->is_initialized = 0;
	ctrlr->notification_sent = 0;
	TASK_INIT(&ctrlr->reset_task, 0, nvme_ctrlr_reset_task, ctrlr);
	TASK_INIT(&ctrlr->fail_req_task, 0, nvme_ctrlr_fail_req_task, ctrlr);
	STAILQ_INIT(&ctrlr->fail_req);
	ctrlr->is_failed = false;

	make_dev_args_init(&md_args);
	md_args.mda_devsw = &nvme_ctrlr_cdevsw;
	md_args.mda_uid = UID_ROOT;
	md_args.mda_gid = GID_WHEEL;
	md_args.mda_mode = 0600;
	md_args.mda_unit = device_get_unit(dev);
	md_args.mda_si_drv1 = (void *)ctrlr;
	status = make_dev_s(&md_args, &ctrlr->cdev, "nvme%d",
	    device_get_unit(dev));
	if (status != 0)
		return (ENXIO);

	return (0);
}

void
nvme_ctrlr_destruct(struct nvme_controller *ctrlr, device_t dev)
{
	int	gone, i;

	ctrlr->is_dying = true;

	if (ctrlr->resource == NULL)
		goto nores;
	if (!mtx_initialized(&ctrlr->adminq.lock))
		goto noadminq;

	/*
	 * Check whether it is a hot unplug or a clean driver detach.
	 * If device is not there any more, skip any shutdown commands.
	 */
	gone = (nvme_mmio_read_4(ctrlr, csts) == NVME_GONE);
	if (gone)
		nvme_ctrlr_fail(ctrlr);
	else
		nvme_notify_fail_consumers(ctrlr);

	for (i = 0; i < NVME_MAX_NAMESPACES; i++)
		nvme_ns_destruct(&ctrlr->ns[i]);

	if (ctrlr->cdev)
		destroy_dev(ctrlr->cdev);

	if (ctrlr->is_initialized) {
		if (!gone) {
			if (ctrlr->hmb_nchunks > 0)
				nvme_ctrlr_hmb_enable(ctrlr, false, false);
			nvme_ctrlr_delete_qpairs(ctrlr);
		}
		nvme_ctrlr_hmb_free(ctrlr);
	}
	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			nvme_io_qpair_destroy(&ctrlr->ioq[i]);
		free(ctrlr->ioq, M_NVME);
	}
	nvme_admin_qpair_destroy(&ctrlr->adminq);

	/*
	 *  Notify the controller of a shutdown, even though this is due to
	 *   a driver unload, not a system shutdown (this path is not invoked
	 *   during shutdown).  This ensures the controller receives a
	 *   shutdown notification in case the system is shutdown before
	 *   reloading the driver.
	 */
	if (!gone)
		nvme_ctrlr_shutdown(ctrlr);

	if (!gone)
		nvme_ctrlr_disable(ctrlr);

noadminq:
	if (ctrlr->taskqueue)
		taskqueue_free(ctrlr->taskqueue);

	if (ctrlr->tag)
		bus_teardown_intr(ctrlr->dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);

	if (ctrlr->bar4_resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->bar4_resource_id, ctrlr->bar4_resource);
	}

	bus_release_resource(dev, SYS_RES_MEMORY,
	    ctrlr->resource_id, ctrlr->resource);

nores:
	mtx_destroy(&ctrlr->lock);
}

void
nvme_ctrlr_shutdown(struct nvme_controller *ctrlr)
{
	uint32_t	cc;
	uint32_t	csts;
	int		timeout;

	cc = nvme_mmio_read_4(ctrlr, cc);
	cc &= ~(NVME_CC_REG_SHN_MASK << NVME_CC_REG_SHN_SHIFT);
	cc |= NVME_SHN_NORMAL << NVME_CC_REG_SHN_SHIFT;
	nvme_mmio_write_4(ctrlr, cc, cc);

	timeout = ticks + (ctrlr->cdata.rtd3e == 0 ? 5 * hz :
	    ((uint64_t)ctrlr->cdata.rtd3e * hz + 999999) / 1000000);
	while (1) {
		csts = nvme_mmio_read_4(ctrlr, csts);
		if (csts == NVME_GONE)		/* Hot unplug. */
			break;
		if (NVME_CSTS_GET_SHST(csts) == NVME_SHST_COMPLETE)
			break;
		if (timeout - ticks < 0) {
			nvme_printf(ctrlr, "shutdown timeout\n");
			break;
		}
		pause("nvmeshut", 1);
	}
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

	qpair = &ctrlr->ioq[QP(ctrlr, curcpu)];
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

int
nvme_ctrlr_suspend(struct nvme_controller *ctrlr)
{
	int to = hz;

	/*
	 * Can't touch failed controllers, so it's already suspended.
	 */
	if (ctrlr->is_failed)
		return (0);

	/*
	 * We don't want the reset taskqueue running, since it does similar
	 * things, so prevent it from running after we start. Wait for any reset
	 * that may have been started to complete. The reset process we follow
	 * will ensure that any new I/O will queue and be given to the hardware
	 * after we resume (though there should be none).
	 */
	while (atomic_cmpset_32(&ctrlr->is_resetting, 0, 1) == 0 && to-- > 0)
		pause("nvmesusp", 1);
	if (to <= 0) {
		nvme_printf(ctrlr,
		    "Competing reset task didn't finish. Try again later.\n");
		return (EWOULDBLOCK);
	}

	if (ctrlr->hmb_nchunks > 0)
		nvme_ctrlr_hmb_enable(ctrlr, false, false);

	/*
	 * Per Section 7.6.2 of NVMe spec 1.4, to properly suspend, we need to
	 * delete the hardware I/O queues, and then shutdown. This properly
	 * flushes any metadata the drive may have stored so it can survive
	 * having its power removed and prevents the unsafe shutdown count from
	 * incriminating. Once we delete the qpairs, we have to disable them
	 * before shutting down.
	 */
	nvme_ctrlr_delete_qpairs(ctrlr);
	nvme_ctrlr_disable_qpairs(ctrlr);
	nvme_ctrlr_shutdown(ctrlr);

	return (0);
}

int
nvme_ctrlr_resume(struct nvme_controller *ctrlr)
{

	/*
	 * Can't touch failed controllers, so nothing to do to resume.
	 */
	if (ctrlr->is_failed)
		return (0);

	if (nvme_ctrlr_hw_reset(ctrlr) != 0)
		goto fail;
#ifdef NVME_2X_RESET
	/*
	 * Prior to FreeBSD 13.1, FreeBSD's nvme driver reset the hardware twice
	 * to get it into a known good state. However, the hardware's state is
	 * good and we don't need to do this for proper functioning.
	 */
	if (nvme_ctrlr_hw_reset(ctrlr) != 0)
		goto fail;
#endif

	/*
	 * Now that we've reset the hardware, we can restart the controller. Any
	 * I/O that was pending is requeued. Any admin commands are aborted with
	 * an error. Once we've restarted, take the controller out of reset.
	 */
	nvme_ctrlr_start(ctrlr, true);
	(void)atomic_cmpset_32(&ctrlr->is_resetting, 1, 0);

	return (0);
fail:
	/*
	 * Since we can't bring the controller out of reset, announce and fail
	 * the controller. However, we have to return success for the resume
	 * itself, due to questionable APIs.
	 */
	nvme_printf(ctrlr, "Failed to reset on resume, failing.\n");
	nvme_ctrlr_fail(ctrlr);
	(void)atomic_cmpset_32(&ctrlr->is_resetting, 1, 0);
	return (0);
}
