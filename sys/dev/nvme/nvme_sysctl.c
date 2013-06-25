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
#include <sys/sysctl.h>

#include "nvme_private.h"

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
		nvme_dump_completion(cpl);
	}

	printf("Submission queue:\n");
	for (i = 0; i < qpair->num_entries; i++) {
		cmd = &qpair->cmd[i];
		printf("%05d: ", i);
		nvme_dump_command(cmd);
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

static void
nvme_sysctl_initialize_queue(struct nvme_qpair *qpair,
    struct sysctl_ctx_list *ctrlr_ctx, struct sysctl_oid *que_tree)
{
	struct sysctl_oid_list	*que_list = SYSCTL_CHILDREN(que_tree);

	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_entries",
	    CTLFLAG_RD, &qpair->num_entries, 0,
	    "Number of entries in hardware queue");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_tr",
	    CTLFLAG_RD, &qpair->num_tr, 0,
	    "Number of trackers allocated");
	SYSCTL_ADD_UINT(ctrlr_ctx, que_list, OID_AUTO, "num_prp_list",
	    CTLFLAG_RD, &qpair->num_prp_list, 0,
	    "Number of PRP lists allocated");
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

	SYSCTL_ADD_PROC(ctrlr_ctx, que_list, OID_AUTO,
	    "dump_debug", CTLTYPE_UINT | CTLFLAG_RW, qpair, 0,
	    nvme_sysctl_dump_debug, "IU", "Dump debug data");
}

void
nvme_sysctl_initialize_ctrlr(struct nvme_controller *ctrlr)
{
	struct sysctl_ctx_list	*ctrlr_ctx;
	struct sysctl_oid	*ctrlr_tree, *que_tree;
	struct sysctl_oid_list	*ctrlr_list;
#define QUEUE_NAME_LENGTH	16
	char			queue_name[QUEUE_NAME_LENGTH];
	int			i;

	ctrlr_ctx = device_get_sysctl_ctx(ctrlr->dev);
	ctrlr_tree = device_get_sysctl_tree(ctrlr->dev);
	ctrlr_list = SYSCTL_CHILDREN(ctrlr_tree);

	if (ctrlr->is_started) {
		SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
		    "int_coal_time", CTLTYPE_UINT | CTLFLAG_RW, ctrlr, 0,
		    nvme_sysctl_int_coal_time, "IU",
		    "Interrupt coalescing timeout (in microseconds)");

		SYSCTL_ADD_PROC(ctrlr_ctx, ctrlr_list, OID_AUTO,
		    "int_coal_threshold", CTLTYPE_UINT | CTLFLAG_RW, ctrlr, 0,
		    nvme_sysctl_int_coal_threshold, "IU",
		    "Interrupt coalescing threshold");
	}

	que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO, "adminq",
	    CTLFLAG_RD, NULL, "Admin Queue");

	nvme_sysctl_initialize_queue(&ctrlr->adminq, ctrlr_ctx, que_tree);

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		snprintf(queue_name, QUEUE_NAME_LENGTH, "ioq%d", i);
		que_tree = SYSCTL_ADD_NODE(ctrlr_ctx, ctrlr_list, OID_AUTO,
		    queue_name, CTLFLAG_RD, NULL, "IO Queue");
		nvme_sysctl_initialize_queue(&ctrlr->ioq[i], ctrlr_ctx,
		    que_tree);
	}
}
