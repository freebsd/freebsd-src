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
#include "opt_nvme.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include "nvme_private.h"

#ifndef NVME_USE_NVD
#define NVME_USE_NVD 0
#endif

int nvme_use_nvd = NVME_USE_NVD;
bool nvme_verbose_cmd_dump = false;

SYSCTL_NODE(_hw, OID_AUTO, nvme, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "NVMe sysctl tunables");
SYSCTL_INT(_hw_nvme, OID_AUTO, use_nvd, CTLFLAG_RDTUN,
    &nvme_use_nvd, 1, "1 = Create NVD devices, 0 = Create NDA devices");
SYSCTL_BOOL(_hw_nvme, OID_AUTO, verbose_cmd_dump, CTLFLAG_RWTUN,
    &nvme_verbose_cmd_dump, 0,
    "enable verbose command printing when a command fails");

static void
nvme_dump_queue(struct nvme_qpair *qpair)
{
	struct nvme_completion *cpl;
	struct nvme_command *cmd;
	int i;

	printf("id:%04Xh phase:%d\n", qpair->id, qpair->phase);

	printf("Completion queue:\n");
	for (i = 0; i < qpair->num_entries; i++) {
		cpl = &qpair->cpl[i];
		printf("%05d: ", i);
		nvme_qpair_print_completion(qpair, cpl);
	}

	printf("Submission queue:\n");
	for (i = 0; i < qpair->num_entries; i++) {
		cmd = &qpair->cmd[i];
		printf("%05d: ", i);
		nvme_qpair_print_command(qpair, cmd);
	}
}

static int
nvme_sysctl_dump_debug(SYSCTL_HANDLER_ARGS)
{
	struct nvme_qpair 	*qpair = arg1;
	uint32_t		val = 0;

	int error = sysctl_handle_int(oidp, &val, 0, req);

	if (error)
		return (error);

	if (val != 0)
		nvme_dump_queue(qpair);

	return (0);
}

static int
nvme_sysctl_int_coal_time(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller *ctrlr = arg1;
	uint32_t oldval = ctrlr->int_coal_time;
	int error = sysctl_handle_int(oidp, &ctrlr->int_coal_time, 0,
	    req);

	if (error)
		return (error);

	if (oldval != ctrlr->int_coal_time)
		nvme_ctrlr_cmd_set_interrupt_coalescing(ctrlr,
		    ctrlr->int_coal_time, ctrlr->int_coal_threshold, NULL,
		    NULL);

	return (0);
}

static int
nvme_sysctl_int_coal_threshold(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller *ctrlr = arg1;
	uint32_t oldval = ctrlr->int_coal_threshold;
	int error = sysctl_handle_int(oidp, &ctrlr->int_coal_threshold, 0,
	    req);

	if (error)
		return (error);

	if (oldval != ctrlr->int_coal_threshold)
		nvme_ctrlr_cmd_set_interrupt_coalescing(ctrlr,
		    ctrlr->int_coal_time, ctrlr->int_coal_threshold, NULL,
		    NULL);

	return (0);
}

static int
nvme_sysctl_timeout_period(SYSCTL_HANDLER_ARGS)
{
	uint32_t *ptr = arg1;
	uint32_t newval = *ptr;
	int error = sysctl_handle_int(oidp, &newval, 0, req);

	if (error || (req->newptr == NULL))
		return (error);

	if (newval > NVME_MAX_TIMEOUT_PERIOD ||
	    newval < NVME_MIN_TIMEOUT_PERIOD) {
		return (EINVAL);
	} else {
		*ptr = newval;
	}

	return (0);
}

static void
nvme_qpair_reset_stats(struct nvme_qpair *qpair)
{
	/*
	 * Reset the values. Due to sanity checks in
	 * nvme_qpair_process_completions, we reset the number of interrupt
	 * calls to 1.
	 */
	qpair->num_cmds = 0;
	qpair->num_intr_handler_calls = 1;
	qpair->num_retries = 0;
	qpair->num_failures = 0;
	qpair->num_ignored = 0;
	qpair->num_recovery_nolock = 0;
}

static int
nvme_sysctl_num_cmds(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num_cmds = 0;
	int			i;

	num_cmds = ctrlr->adminq.num_cmds;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_cmds += ctrlr->ioq[i].num_cmds;
	}

	return (sysctl_handle_64(oidp, &num_cmds, 0, req));
}

static int
nvme_sysctl_num_intr_handler_calls(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num_intr_handler_calls = 0;
	int			i;

	num_intr_handler_calls = ctrlr->adminq.num_intr_handler_calls;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_intr_handler_calls += ctrlr->ioq[i].num_intr_handler_calls;
	}

	return (sysctl_handle_64(oidp, &num_intr_handler_calls, 0, req));
}

static int
nvme_sysctl_num_retries(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num_retries = 0;
	int			i;

	num_retries = ctrlr->adminq.num_retries;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_retries += ctrlr->ioq[i].num_retries;
	}

	return (sysctl_handle_64(oidp, &num_retries, 0, req));
}

static int
nvme_sysctl_num_failures(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num_failures = 0;
	int			i;

	num_failures = ctrlr->adminq.num_failures;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_failures += ctrlr->ioq[i].num_failures;
	}

	return (sysctl_handle_64(oidp, &num_failures, 0, req));
}

static int
nvme_sysctl_num_ignored(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num_ignored = 0;
	int			i;

	num_ignored = ctrlr->adminq.num_ignored;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num_ignored += ctrlr->ioq[i].num_ignored;
	}

	return (sysctl_handle_64(oidp, &num_ignored, 0, req));
}

static int
nvme_sysctl_num_recovery_nolock(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	int64_t			num;
	int			i;

	num = ctrlr->adminq.num_recovery_nolock;

	if (ctrlr->ioq != NULL) {
		for (i = 0; i < ctrlr->num_io_queues; i++)
			num += ctrlr->ioq[i].num_recovery_nolock;
	}

	return (sysctl_handle_64(oidp, &num, 0, req));
}

static int
nvme_sysctl_reset_stats(SYSCTL_HANDLER_ARGS)
{
	struct nvme_controller 	*ctrlr = arg1;
	uint32_t		i, val = 0;

	int error = sysctl_handle_int(oidp, &val, 0, req);

	if (error)
		return (error);

	if (val != 0) {
		nvme_qpair_reset_stats(&ctrlr->adminq);

		if (ctrlr->ioq != NULL) {
			for (i = 0; i < ctrlr->num_io_queues; i++)
				nvme_qpair_reset_stats(&ctrlr->ioq[i]);
		}
	}

	return (0);
}

static void
nvme_sysctl_initialize_queue(struct nvme_qpair *qpair,
    struct sysctl_ctx_list *ctrlr_ctx, struct sysctl_oid *que_tree)
{
	struct sysctl_oid_list	*que_list = SYSCTL_CHILDREN(que_tree);

	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_entries",
	    CTLFLAG_RD, &qpair->num_entries, 0,
	    "Number of entries in hardware queue");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_trackers",
	    CTLFLAG_RD, &qpair->num_trackers, 0,
	    "Number of trackers pre-allocated for this queue pair");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "sq_head",
	    CTLFLAG_RD, &qpair->sq_head, 0,
	    "Current head of submission queue (as observed by driver)");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "sq_tail",
	    CTLFLAG_RD, &qpair->sq_tail, 0,
	    "Current tail of submission queue (as observed by driver)");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "cq_head",
	    CTLFLAG_RD, &qpair->cq_head, 0,
	    "Current head of completion queue (as observed by driver)");

	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_cmds",
	    CTLFLAG_RD, &qpair->num_cmds, "Number of commands submitted");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_intr_handler_calls",
	    CTLFLAG_RD, &qpair->num_intr_handler_calls,
	    "Number of times interrupt handler was invoked (will typically be "
	    "less than number of actual interrupts generated due to "
	    "coalescing)");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_retries",
	    CTLFLAG_RD, &qpair->num_retries, "Number of commands retried");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_failures",
	    CTLFLAG_RD, &qpair->num_failures,
	    "Number of commands ending in failure after all retries");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_ignored",
	    CTLFLAG_RD, &qpair->num_ignored,
	    "Number of interrupts posted, but were administratively ignored");
	SYSCTL_ADD_QUAD(ctrlr_ctx, que_list, OID_AUTO, "num_recovery_nolock",
	    CTLFLAG_RD, &qpair->num_recovery_nolock,
	    "Number of times that we failed to lock recovery in the ISR");

	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "recovery",
	    CTLFLAG_RW, &qpair->recovery_state, 0,
	    "Current recovery state of the queue");

	SYSCTL_ADD_PROC(ctrlr_ctx, que_list, OID_AUTO,
	    "dump_debug", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    qpair, 0, nvme_sysctl_dump_debug, "IU", "Dump debug data");
}

void
nvme_sysctl_initialize_ctrlr(struct nvme_controller *ctrlr)
{
	struct sysctl_ctx_list	*ctrlr_ctx;
	struct sysctl_oid	*ctrlr_tree, *que_tree, *ioq_tree;
	struct sysctl_oid_list	*ctrlr_list, *ioq_list;
#define QUEUE_NAME_LENGTH	16
	char			queue_name[QUEUE_NAME_LENGTH];
	int			i;

	ctrlr_ctx = device_get_sysctl_ctx(ctrlr->dev);
	ctrlr_tree = device_get_sysctl_tree(ctrlr->dev);
	ctrlr_list = SYSCTL_CHILDREN(ctrlr_tree);

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "num_io_queues",
	    CTLFLAG_RD, &ctrlr->num_io_queues, 0,
	    "Number of I/O queue pairs");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "int_coal_time", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_int_coal_time, "IU",
	    "Interrupt coalescing timeout (in microseconds)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "int_coal_threshold",
	    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, ctrlr, 0,
	    nvme_sysctl_int_coal_threshold, "IU",
	    "Interrupt coalescing threshold");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "admin_timeout_period", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &ctrlr->admin_timeout_period, 0, nvme_sysctl_timeout_period, "IU",
	    "Timeout period for Admin queue (in seconds)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "timeout_period", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &ctrlr->timeout_period, 0, nvme_sysctl_timeout_period, "IU",
	    "Timeout period for I/O queues (in seconds)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_cmds", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_num_cmds, "IU",
	    "Number of commands submitted");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_intr_handler_calls",
	    CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE, ctrlr, 0,
	    nvme_sysctl_num_intr_handler_calls, "IU",
	    "Number of times interrupt handler was invoked (will "
	    "typically be less than number of actual interrupts "
	    "generated due to coalescing)");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_retries", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_num_retries, "IU",
	    "Number of commands retried");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_failures", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_num_failures, "IU",
	    "Number of commands ending in failure after all retries");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_ignored", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_num_ignored, "IU",
	    "Number of interrupts ignored administratively");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "num_recovery_nolock", CTLTYPE_S64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    ctrlr, 0, nvme_sysctl_num_recovery_nolock, "IU",
	    "Number of times that we failed to lock recovery in the ISR");

	SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
	    "reset_stats", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, ctrlr,
	    0, nvme_sysctl_reset_stats, "IU", "Reset statistics to zero");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "cap_lo",
	    CTLFLAG_RD, &ctrlr->cap_lo, 0,
	    "Low 32-bits of capacities for the drive");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "cap_hi",
	    CTLFLAG_RD, &ctrlr->cap_hi, 0,
	    "Hi 32-bits of capacities for the drive");

	SYSCTL_ADD_UINT(ctrlr_ctx, ctrlr_list, OID_AUTO, "fail_on_reset",
	    CTLFLAG_RD, &ctrlr->fail_on_reset, 0,
	    "Pretend the next reset fails and fail the controller");

	que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO, "adminq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Admin Queue");

	nvme_sysctl_initialize_queue(&ctrlr->adminq, ctrlr_ctx, que_tree);

	/*
	 * Make sure that we've constructed the I/O queues before setting up the
	 * sysctls. Failed controllers won't allocate it, but we want the rest
	 * of the sysctls to diagnose things.
	 */
	if (ctrlr->ioq != NULL) {
		ioq_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO,
		    "ioq", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "I/O Queues");
		ioq_list = SYSCTL_CHILDREN(ioq_tree);

		for (i = 0; i < ctrlr->num_io_queues; i++) {
			snprintf(queue_name, QUEUE_NAME_LENGTH, "%d", i);
			que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ioq_list, OID_AUTO,
			    queue_name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "IO Queue");
			nvme_sysctl_initialize_queue(&ctrlr->ioq[i], ctrlr_ctx,
			    que_tree);
		}
	}

	SYSCTL_ADD_COUNTER_U64(ctrlr_ctx, ctrlr_list, OID_AUTO, "alignment_splits",
	    CTLFLAG_RD, &ctrlr->alignment_splits,
	    "Number of times we split the I/O alignment for drives with preferred alignment");
}
