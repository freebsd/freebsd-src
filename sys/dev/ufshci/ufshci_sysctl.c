/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include "ufshci_private.h"

static int
ufshci_sysctl_timeout_period(SYSCTL_HANDLER_ARGS)
{
	uint32_t *ptr = arg1;
	uint32_t newval = *ptr;
	int error = sysctl_handle_int(oidp, &newval, 0, req);

	if (error || (req->newptr == NULL))
		return (error);

	if (newval > UFSHCI_MAX_TIMEOUT_PERIOD ||
	    newval < UFSHCI_MIN_TIMEOUT_PERIOD) {
		return (EINVAL);
	} else {
		*ptr = newval;
	}

	return (0);
}

static int
ufshci_sysctl_num_cmds(SYSCTL_HANDLER_ARGS)
{
	struct ufshci_controller *ctrlr = arg1;
	int64_t num_cmds = 0;
	int i;

	num_cmds = ctrlr->task_mgmt_req_queue.hwq[UFSHCI_SDB_Q].num_cmds;

	if (ctrlr->transfer_req_queue.hwq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_cmds += ctrlr->transfer_req_queue.hwq[i].num_cmds;
	}

	return (sysctl_handle_64(oidp, &num_cmds, 0, req));
}

static int
ufshci_sysctl_num_intr_handler_calls(SYSCTL_HANDLER_ARGS)
{
	struct ufshci_controller *ctrlr = arg1;
	int64_t num_intr_handler_calls = 0;
	int i;

	num_intr_handler_calls =
	    ctrlr->task_mgmt_req_queue.hwq[UFSHCI_SDB_Q].num_intr_handler_calls;

	if (ctrlr->transfer_req_queue.hwq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_intr_handler_calls += ctrlr->transfer_req_queue
						      .hwq[i]
						      .num_intr_handler_calls;
	}

	return (sysctl_handle_64(oidp, &num_intr_handler_calls, 0, req));
}

static int
ufshci_sysctl_num_retries(SYSCTL_HANDLER_ARGS)
{
	struct ufshci_controller *ctrlr = arg1;
	int64_t num_retries = 0;
	int i;

	num_retries = ctrlr->task_mgmt_req_queue.hwq[UFSHCI_SDB_Q].num_retries;

	if (ctrlr->transfer_req_queue.hwq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_retries +=
			    ctrlr->transfer_req_queue.hwq[i].num_retries;
	}

	return (sysctl_handle_64(oidp, &num_retries, 0, req));
}

static int
ufshci_sysctl_num_failures(SYSCTL_HANDLER_ARGS)
{
	struct ufshci_controller *ctrlr = arg1;
	int64_t num_failures = 0;
	int i;

	num_failures =
	    ctrlr->task_mgmt_req_queue.hwq[UFSHCI_SDB_Q].num_failures;

	if (ctrlr->transfer_req_queue.hwq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_failures +=
			    ctrlr->transfer_req_queue.hwq[i].num_failures;
	}

	return (sysctl_handle_64(oidp, &num_failures, 0, req));
}

static void
ufshci_sysctl_initialize_queue(struct ufshci_hw_queue *hwq,
    struct sysctl_ctx_list *ctrlr_ctx, struct sysctl_oid *que_tree)
{
	struct sysctl_oid_list *que_list = SYSCTL_CHILDREN(que_tree);

	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_entries",
	    CTLFLAG_RD, &hwq->num_entries, 0,
	    "Number of entries in hardware queue");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_trackers",
	    CTLFLAG_RD, &hwq->num_trackers, 0,
	    "Number of trackers pre-allocated for this queue pair");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "sq_head", CTLFLAG_RD,
	    &hwq->sq_head, 0,
	    "Current head of submission queue (as observed by driver)");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "sq_tail", CTLFLAG_RD,
	    &hwq->sq_tail, 0,
	    "Current tail of submission queue (as observed by driver)");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "cq_head", CTLFLAG_RD,
	    &hwq->cq_head, 0,
	    "Current head of completion queue (as observed by driver)");

	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_cmds", CTLFLAG_RD,
	    &hwq->num_cmds, "Number of commands submitted");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_intr_handler_calls",
	    CTLFLAG_RD, &hwq->num_intr_handler_calls,
	    "Number of times interrupt handler was invoked (will typically be "
	    "less than number of actual interrupts generated due to "
	    "interrupt aggregation)");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_retries",
	    CTLFLAG_RD, &hwq->num_retries, "Number of commands retried");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_failures",
	    CTLFLAG_RD, &hwq->num_failures,
	    "Number of commands ending in failure after all retries");

	/* TODO: Implement num_ignored */
	/* TODO: Implement recovery state */
	/* TODO: Implement dump debug */
}

void
ufshci_sysctl_initialize_ctrlr(struct ufshci_controller *ctrlr)
{
	struct sysctl_ctx_list *ctrlr_ctx;
	struct sysctl_oid *ctrlr_tree, *que_tree, *ioq_tree;
	struct sysctl_oid_list *ctrlr_list, *ioq_list;
	struct ufshci_device *dev = &ctrlr->ufs_dev;
#define QUEUE_NAME_LENGTH 16
	char queue_name[QUEUE_NAME_LENGTH];
	int i;

	ctrlr_ctx = device_get_sysctl_ctx(ctrlr->dev);
	ctrlr_tree = device_get_sysctl_tree(ctrlr->dev);
	ctrlr_list = SYSCTL_CHILDREN(ctrlr_tree);

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "major_version",
	    CTLFLAG_RD, &ctrlr->major_version, 0, "UFS spec major version");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "minor_version",
	    CTLFLAG_RD, &ctrlr->minor_version, 0, "UFS spec minor version");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "io_queue_mode",
	    CTLFLAG_RD, &ctrlr->transfer_req_queue.queue_mode, 0,
	    "Active host-side queuing scheme "
	    "(Single-Doorbell or Multi-Circular-Queue)");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "num_io_queues",
	    CTLFLAG_RD, &ctrlr->num_io_queues, 0, "Number of I/O queue pairs");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "cap", CTLFLAG_RD,
	    &ctrlr->cap, 0, "Number of I/O queue pairs");

	SYSCTL_ADD_BOOL(ctrlr_ctx, ctrlr_list, OID_AUTO, "wb_enabled",
	    CTLFLAG_RD, &dev->is_wb_enabled, 0, "WriteBooster enable/disable");

	SYSCTL_ADD_BOOL(ctrlr_ctx, ctrlr_list, OID_AUTO, "wb_flush_enabled",
	    CTLFLAG_RD, &dev->is_wb_flush_enabled, 0,
	    "WriteBooster flush enable/disable");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "wb_buffer_type",
	    CTLFLAG_RD, &dev->wb_buffer_type, 0, "WriteBooster type");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "wb_buffer_size_mb",
	    CTLFLAG_RD, &dev->wb_buffer_size_mb, 0,
	    "WriteBooster buffer size in MB");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "wb_user_space_config_option", CTLFLAG_RD,
	    &dev->wb_user_space_config_option, 0,
	    "WriteBooster preserve user space mode");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO, "timeout_period",
	    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, &ctrlr->timeout_period,
	    0, ufshci_sysctl_timeout_period, "IU",
	    "Timeout period for I/O queues (in seconds)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO, "num_cmds",
	    CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE, ctrlr, 0,
	    ufshci_sysctl_num_cmds, "IU", "Number of commands submitted");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_intr_handler_calls", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, ufshci_sysctl_num_intr_handler_calls, "IU",
	    "Number of times interrupt handler was invoked (will "
	    "typically be less than number of actual interrupts "
	    "generated due to coalescing)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO, "num_retries",
	    CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE, ctrlr, 0,
	    ufshci_sysctl_num_retries, "IU", "Number of commands retried");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO, "num_failures",
	    CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE, ctrlr, 0,
	    ufshci_sysctl_num_failures, "IU",
	    "Number of commands ending in failure after all retries");

	que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO, "utmrq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "UTP Task Management Request Queue");

	ufshci_sysctl_initialize_queue(
	    &ctrlr->task_mgmt_req_queue.hwq[UFSHCI_SDB_Q], ctrlr_ctx, que_tree);

	/*
	 * Make sure that we've constructed the I/O queues before setting up the
	 * sysctls. Failed controllers won't allocate it, but we want the rest
	 * of the sysctls to diagnose things.
	 */
	if (ctrlr->transfer_req_queue.hwq != NULL) {
		ioq_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO,
		    "ioq", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
		    "UTP Transfer Request Queue (I/O Queue)");
		ioq_list = SYSCTL_CHILDREN(ioq_tree);

		for (i = 0; i < ctrlr->num_io_queues; i++) {
			snprintf(queue_name, QUEUE_NAME_LENGTH, "%d", i);
			que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ioq_list,
			    OID_AUTO, queue_name, CTLFLAG_RD | CTLFLAG_MPSAFE,
			    NULL, "IO Queue");
			ufshci_sysctl_initialize_queue(
			    &ctrlr->transfer_req_queue.hwq[i], ctrlr_ctx,
			    que_tree);
		}
	}
}
