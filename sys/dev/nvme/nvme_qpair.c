/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/proc.h>

#include <dev/pci/pcivar.h>

#include "nvme_private.h"

typedef enum error_print { ERROR_PRINT_NONE, ERROR_PRINT_NO_RETRY, ERROR_PRINT_ALL } error_print_t;
#define DO_NOT_RETRY	1

static void	_nvme_qpair_submit_request(struct nvme_qpair *qpair,
					   struct nvme_request *req);
static void	nvme_qpair_destroy(struct nvme_qpair *qpair);

struct nvme_opcode_string {
	uint16_t	opc;
	const char *	str;
};

static struct nvme_opcode_string admin_opcode[] = {
	{ NVME_OPC_DELETE_IO_SQ, "DELETE IO SQ" },
	{ NVME_OPC_CREATE_IO_SQ, "CREATE IO SQ" },
	{ NVME_OPC_GET_LOG_PAGE, "GET LOG PAGE" },
	{ NVME_OPC_DELETE_IO_CQ, "DELETE IO CQ" },
	{ NVME_OPC_CREATE_IO_CQ, "CREATE IO CQ" },
	{ NVME_OPC_IDENTIFY, "IDENTIFY" },
	{ NVME_OPC_ABORT, "ABORT" },
	{ NVME_OPC_SET_FEATURES, "SET FEATURES" },
	{ NVME_OPC_GET_FEATURES, "GET FEATURES" },
	{ NVME_OPC_ASYNC_EVENT_REQUEST, "ASYNC EVENT REQUEST" },
	{ NVME_OPC_FIRMWARE_ACTIVATE, "FIRMWARE ACTIVATE" },
	{ NVME_OPC_FIRMWARE_IMAGE_DOWNLOAD, "FIRMWARE IMAGE DOWNLOAD" },
	{ NVME_OPC_DEVICE_SELF_TEST, "DEVICE SELF-TEST" },
	{ NVME_OPC_NAMESPACE_ATTACHMENT, "NAMESPACE ATTACHMENT" },
	{ NVME_OPC_KEEP_ALIVE, "KEEP ALIVE" },
	{ NVME_OPC_DIRECTIVE_SEND, "DIRECTIVE SEND" },
	{ NVME_OPC_DIRECTIVE_RECEIVE, "DIRECTIVE RECEIVE" },
	{ NVME_OPC_VIRTUALIZATION_MANAGEMENT, "VIRTUALIZATION MANAGEMENT" },
	{ NVME_OPC_NVME_MI_SEND, "NVME-MI SEND" },
	{ NVME_OPC_NVME_MI_RECEIVE, "NVME-MI RECEIVE" },
	{ NVME_OPC_DOORBELL_BUFFER_CONFIG, "DOORBELL BUFFER CONFIG" },
	{ NVME_OPC_FORMAT_NVM, "FORMAT NVM" },
	{ NVME_OPC_SECURITY_SEND, "SECURITY SEND" },
	{ NVME_OPC_SECURITY_RECEIVE, "SECURITY RECEIVE" },
	{ NVME_OPC_SANITIZE, "SANITIZE" },
	{ NVME_OPC_GET_LBA_STATUS, "GET LBA STATUS" },
	{ 0xFFFF, "ADMIN COMMAND" }
};

static struct nvme_opcode_string io_opcode[] = {
	{ NVME_OPC_FLUSH, "FLUSH" },
	{ NVME_OPC_WRITE, "WRITE" },
	{ NVME_OPC_READ, "READ" },
	{ NVME_OPC_WRITE_UNCORRECTABLE, "WRITE UNCORRECTABLE" },
	{ NVME_OPC_COMPARE, "COMPARE" },
	{ NVME_OPC_WRITE_ZEROES, "WRITE ZEROES" },
	{ NVME_OPC_DATASET_MANAGEMENT, "DATASET MANAGEMENT" },
	{ NVME_OPC_VERIFY, "VERIFY" },
	{ NVME_OPC_RESERVATION_REGISTER, "RESERVATION REGISTER" },
	{ NVME_OPC_RESERVATION_REPORT, "RESERVATION REPORT" },
	{ NVME_OPC_RESERVATION_ACQUIRE, "RESERVATION ACQUIRE" },
	{ NVME_OPC_RESERVATION_RELEASE, "RESERVATION RELEASE" },
	{ 0xFFFF, "IO COMMAND" }
};

static const char *
get_admin_opcode_string(uint16_t opc)
{
	struct nvme_opcode_string *entry;

	entry = admin_opcode;

	while (entry->opc != 0xFFFF) {
		if (entry->opc == opc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}

static const char *
get_io_opcode_string(uint16_t opc)
{
	struct nvme_opcode_string *entry;

	entry = io_opcode;

	while (entry->opc != 0xFFFF) {
		if (entry->opc == opc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}

static void
nvme_admin_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{

	nvme_printf(qpair->ctrlr, "%s (%02x) sqid:%d cid:%d nsid:%x "
	    "cdw10:%08x cdw11:%08x\n",
	    get_admin_opcode_string(cmd->opc), cmd->opc, qpair->id, cmd->cid,
	    le32toh(cmd->nsid), le32toh(cmd->cdw10), le32toh(cmd->cdw11));
}

static void
nvme_io_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{

	switch (cmd->opc) {
	case NVME_OPC_WRITE:
	case NVME_OPC_READ:
	case NVME_OPC_WRITE_UNCORRECTABLE:
	case NVME_OPC_COMPARE:
	case NVME_OPC_WRITE_ZEROES:
	case NVME_OPC_VERIFY:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d "
		    "lba:%llu len:%d\n",
		    get_io_opcode_string(cmd->opc), qpair->id, cmd->cid, le32toh(cmd->nsid),
		    ((unsigned long long)le32toh(cmd->cdw11) << 32) + le32toh(cmd->cdw10),
		    (le32toh(cmd->cdw12) & 0xFFFF) + 1);
		break;
	case NVME_OPC_FLUSH:
	case NVME_OPC_DATASET_MANAGEMENT:
	case NVME_OPC_RESERVATION_REGISTER:
	case NVME_OPC_RESERVATION_REPORT:
	case NVME_OPC_RESERVATION_ACQUIRE:
	case NVME_OPC_RESERVATION_RELEASE:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d\n",
		    get_io_opcode_string(cmd->opc), qpair->id, cmd->cid, le32toh(cmd->nsid));
		break;
	default:
		nvme_printf(qpair->ctrlr, "%s (%02x) sqid:%d cid:%d nsid:%d\n",
		    get_io_opcode_string(cmd->opc), cmd->opc, qpair->id,
		    cmd->cid, le32toh(cmd->nsid));
		break;
	}
}

static void
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

struct nvme_status_string {
	uint16_t	sc;
	const char *	str;
};

static struct nvme_status_string generic_status[] = {
	{ NVME_SC_SUCCESS, "SUCCESS" },
	{ NVME_SC_INVALID_OPCODE, "INVALID OPCODE" },
	{ NVME_SC_INVALID_FIELD, "INVALID_FIELD" },
	{ NVME_SC_COMMAND_ID_CONFLICT, "COMMAND ID CONFLICT" },
	{ NVME_SC_DATA_TRANSFER_ERROR, "DATA TRANSFER ERROR" },
	{ NVME_SC_ABORTED_POWER_LOSS, "ABORTED - POWER LOSS" },
	{ NVME_SC_INTERNAL_DEVICE_ERROR, "INTERNAL DEVICE ERROR" },
	{ NVME_SC_ABORTED_BY_REQUEST, "ABORTED - BY REQUEST" },
	{ NVME_SC_ABORTED_SQ_DELETION, "ABORTED - SQ DELETION" },
	{ NVME_SC_ABORTED_FAILED_FUSED, "ABORTED - FAILED FUSED" },
	{ NVME_SC_ABORTED_MISSING_FUSED, "ABORTED - MISSING FUSED" },
	{ NVME_SC_INVALID_NAMESPACE_OR_FORMAT, "INVALID NAMESPACE OR FORMAT" },
	{ NVME_SC_COMMAND_SEQUENCE_ERROR, "COMMAND SEQUENCE ERROR" },
	{ NVME_SC_INVALID_SGL_SEGMENT_DESCR, "INVALID SGL SEGMENT DESCRIPTOR" },
	{ NVME_SC_INVALID_NUMBER_OF_SGL_DESCR, "INVALID NUMBER OF SGL DESCRIPTORS" },
	{ NVME_SC_DATA_SGL_LENGTH_INVALID, "DATA SGL LENGTH INVALID" },
	{ NVME_SC_METADATA_SGL_LENGTH_INVALID, "METADATA SGL LENGTH INVALID" },
	{ NVME_SC_SGL_DESCRIPTOR_TYPE_INVALID, "SGL DESCRIPTOR TYPE INVALID" },
	{ NVME_SC_INVALID_USE_OF_CMB, "INVALID USE OF CONTROLLER MEMORY BUFFER" },
	{ NVME_SC_PRP_OFFET_INVALID, "PRP OFFET INVALID" },
	{ NVME_SC_ATOMIC_WRITE_UNIT_EXCEEDED, "ATOMIC WRITE UNIT EXCEEDED" },
	{ NVME_SC_OPERATION_DENIED, "OPERATION DENIED" },
	{ NVME_SC_SGL_OFFSET_INVALID, "SGL OFFSET INVALID" },
	{ NVME_SC_HOST_ID_INCONSISTENT_FORMAT, "HOST IDENTIFIER INCONSISTENT FORMAT" },
	{ NVME_SC_KEEP_ALIVE_TIMEOUT_EXPIRED, "KEEP ALIVE TIMEOUT EXPIRED" },
	{ NVME_SC_KEEP_ALIVE_TIMEOUT_INVALID, "KEEP ALIVE TIMEOUT INVALID" },
	{ NVME_SC_ABORTED_DUE_TO_PREEMPT, "COMMAND ABORTED DUE TO PREEMPT AND ABORT" },
	{ NVME_SC_SANITIZE_FAILED, "SANITIZE FAILED" },
	{ NVME_SC_SANITIZE_IN_PROGRESS, "SANITIZE IN PROGRESS" },
	{ NVME_SC_SGL_DATA_BLOCK_GRAN_INVALID, "SGL_DATA_BLOCK_GRANULARITY_INVALID" },
	{ NVME_SC_NOT_SUPPORTED_IN_CMB, "COMMAND NOT SUPPORTED FOR QUEUE IN CMB" },
	{ NVME_SC_NAMESPACE_IS_WRITE_PROTECTED, "NAMESPACE IS WRITE PROTECTED" },
	{ NVME_SC_COMMAND_INTERRUPTED, "COMMAND INTERRUPTED" },
	{ NVME_SC_TRANSIENT_TRANSPORT_ERROR, "TRANSIENT TRANSPORT ERROR" },

	{ NVME_SC_LBA_OUT_OF_RANGE, "LBA OUT OF RANGE" },
	{ NVME_SC_CAPACITY_EXCEEDED, "CAPACITY EXCEEDED" },
	{ NVME_SC_NAMESPACE_NOT_READY, "NAMESPACE NOT READY" },
	{ NVME_SC_RESERVATION_CONFLICT, "RESERVATION CONFLICT" },
	{ NVME_SC_FORMAT_IN_PROGRESS, "FORMAT IN PROGRESS" },
	{ 0xFFFF, "GENERIC" }
};

static struct nvme_status_string command_specific_status[] = {
	{ NVME_SC_COMPLETION_QUEUE_INVALID, "INVALID COMPLETION QUEUE" },
	{ NVME_SC_INVALID_QUEUE_IDENTIFIER, "INVALID QUEUE IDENTIFIER" },
	{ NVME_SC_MAXIMUM_QUEUE_SIZE_EXCEEDED, "MAX QUEUE SIZE EXCEEDED" },
	{ NVME_SC_ABORT_COMMAND_LIMIT_EXCEEDED, "ABORT CMD LIMIT EXCEEDED" },
	{ NVME_SC_ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED, "ASYNC LIMIT EXCEEDED" },
	{ NVME_SC_INVALID_FIRMWARE_SLOT, "INVALID FIRMWARE SLOT" },
	{ NVME_SC_INVALID_FIRMWARE_IMAGE, "INVALID FIRMWARE IMAGE" },
	{ NVME_SC_INVALID_INTERRUPT_VECTOR, "INVALID INTERRUPT VECTOR" },
	{ NVME_SC_INVALID_LOG_PAGE, "INVALID LOG PAGE" },
	{ NVME_SC_INVALID_FORMAT, "INVALID FORMAT" },
	{ NVME_SC_FIRMWARE_REQUIRES_RESET, "FIRMWARE REQUIRES RESET" },
	{ NVME_SC_INVALID_QUEUE_DELETION, "INVALID QUEUE DELETION" },
	{ NVME_SC_FEATURE_NOT_SAVEABLE, "FEATURE IDENTIFIER NOT SAVEABLE" },
	{ NVME_SC_FEATURE_NOT_CHANGEABLE, "FEATURE NOT CHANGEABLE" },
	{ NVME_SC_FEATURE_NOT_NS_SPECIFIC, "FEATURE NOT NAMESPACE SPECIFIC" },
	{ NVME_SC_FW_ACT_REQUIRES_NVMS_RESET, "FIRMWARE ACTIVATION REQUIRES NVM SUBSYSTEM RESET" },
	{ NVME_SC_FW_ACT_REQUIRES_RESET, "FIRMWARE ACTIVATION REQUIRES RESET" },
	{ NVME_SC_FW_ACT_REQUIRES_TIME, "FIRMWARE ACTIVATION REQUIRES MAXIMUM TIME VIOLATION" },
	{ NVME_SC_FW_ACT_PROHIBITED, "FIRMWARE ACTIVATION PROHIBITED" },
	{ NVME_SC_OVERLAPPING_RANGE, "OVERLAPPING RANGE" },
	{ NVME_SC_NS_INSUFFICIENT_CAPACITY, "NAMESPACE INSUFFICIENT CAPACITY" },
	{ NVME_SC_NS_ID_UNAVAILABLE, "NAMESPACE IDENTIFIER UNAVAILABLE" },
	{ NVME_SC_NS_ALREADY_ATTACHED, "NAMESPACE ALREADY ATTACHED" },
	{ NVME_SC_NS_IS_PRIVATE, "NAMESPACE IS PRIVATE" },
	{ NVME_SC_NS_NOT_ATTACHED, "NS NOT ATTACHED" },
	{ NVME_SC_THIN_PROV_NOT_SUPPORTED, "THIN PROVISIONING NOT SUPPORTED" },
	{ NVME_SC_CTRLR_LIST_INVALID, "CONTROLLER LIST INVALID" },
	{ NVME_SC_SELF_TEST_IN_PROGRESS, "DEVICE SELF-TEST IN PROGRESS" },
	{ NVME_SC_BOOT_PART_WRITE_PROHIB, "BOOT PARTITION WRITE PROHIBITED" },
	{ NVME_SC_INVALID_CTRLR_ID, "INVALID CONTROLLER IDENTIFIER" },
	{ NVME_SC_INVALID_SEC_CTRLR_STATE, "INVALID SECONDARY CONTROLLER STATE" },
	{ NVME_SC_INVALID_NUM_OF_CTRLR_RESRC, "INVALID NUMBER OF CONTROLLER RESOURCES" },
	{ NVME_SC_INVALID_RESOURCE_ID, "INVALID RESOURCE IDENTIFIER" },
	{ NVME_SC_SANITIZE_PROHIBITED_WPMRE, "SANITIZE PROHIBITED WRITE PERSISTENT MEMORY REGION ENABLED" },
	{ NVME_SC_ANA_GROUP_ID_INVALID, "ANA GROUP IDENTIFIED INVALID" },
	{ NVME_SC_ANA_ATTACH_FAILED, "ANA ATTACH FAILED" },

	{ NVME_SC_CONFLICTING_ATTRIBUTES, "CONFLICTING ATTRIBUTES" },
	{ NVME_SC_INVALID_PROTECTION_INFO, "INVALID PROTECTION INFO" },
	{ NVME_SC_ATTEMPTED_WRITE_TO_RO_PAGE, "WRITE TO RO PAGE" },
	{ 0xFFFF, "COMMAND SPECIFIC" }
};

static struct nvme_status_string media_error_status[] = {
	{ NVME_SC_WRITE_FAULTS, "WRITE FAULTS" },
	{ NVME_SC_UNRECOVERED_READ_ERROR, "UNRECOVERED READ ERROR" },
	{ NVME_SC_GUARD_CHECK_ERROR, "GUARD CHECK ERROR" },
	{ NVME_SC_APPLICATION_TAG_CHECK_ERROR, "APPLICATION TAG CHECK ERROR" },
	{ NVME_SC_REFERENCE_TAG_CHECK_ERROR, "REFERENCE TAG CHECK ERROR" },
	{ NVME_SC_COMPARE_FAILURE, "COMPARE FAILURE" },
	{ NVME_SC_ACCESS_DENIED, "ACCESS DENIED" },
	{ NVME_SC_DEALLOCATED_OR_UNWRITTEN, "DEALLOCATED OR UNWRITTEN LOGICAL BLOCK" },
	{ 0xFFFF, "MEDIA ERROR" }
};

static struct nvme_status_string path_related_status[] = {
	{ NVME_SC_INTERNAL_PATH_ERROR, "INTERNAL PATH ERROR" },
	{ NVME_SC_ASYMMETRIC_ACCESS_PERSISTENT_LOSS, "ASYMMETRIC ACCESS PERSISTENT LOSS" },
	{ NVME_SC_ASYMMETRIC_ACCESS_INACCESSIBLE, "ASYMMETRIC ACCESS INACCESSIBLE" },
	{ NVME_SC_ASYMMETRIC_ACCESS_TRANSITION, "ASYMMETRIC ACCESS TRANSITION" },
	{ NVME_SC_CONTROLLER_PATHING_ERROR, "CONTROLLER PATHING ERROR" },
	{ NVME_SC_HOST_PATHING_ERROR, "HOST PATHING ERROR" },
	{ NVME_SC_COMMAND_ABOTHED_BY_HOST, "COMMAND ABOTHED BY HOST" },
	{ 0xFFFF, "PATH RELATED" },
};

static const char *
get_status_string(uint16_t sct, uint16_t sc)
{
	struct nvme_status_string *entry;

	switch (sct) {
	case NVME_SCT_GENERIC:
		entry = generic_status;
		break;
	case NVME_SCT_COMMAND_SPECIFIC:
		entry = command_specific_status;
		break;
	case NVME_SCT_MEDIA_ERROR:
		entry = media_error_status;
		break;
	case NVME_SCT_PATH_RELATED:
		entry = path_related_status;
		break;
	case NVME_SCT_VENDOR_SPECIFIC:
		return ("VENDOR SPECIFIC");
	default:
		return ("RESERVED");
	}

	while (entry->sc != 0xFFFF) {
		if (entry->sc == sc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}

static void
nvme_qpair_print_completion(struct nvme_qpair *qpair,
    struct nvme_completion *cpl)
{
	uint8_t sct, sc, crd, m, dnr;

	sct = NVME_STATUS_GET_SCT(cpl->status);
	sc = NVME_STATUS_GET_SC(cpl->status);
	crd = NVME_STATUS_GET_CRD(cpl->status);
	m = NVME_STATUS_GET_M(cpl->status);
	dnr = NVME_STATUS_GET_DNR(cpl->status);

	nvme_printf(qpair->ctrlr, "%s (%02x/%02x) crd:%x m:%x dnr:%x "
	    "sqid:%d cid:%d cdw0:%x\n",
	    get_status_string(sct, sc), sct, sc, crd, m, dnr,
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
	struct nvme_qpair * qpair = tr->qpair;
	struct nvme_request	*req;
	bool			retry, error, retriable;

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
		if (req->type != NVME_REQUEST_NULL) {
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
		if (req->type != NVME_REQUEST_NULL) {
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

static void
nvme_qpair_manual_complete_tracker(
    struct nvme_tracker *tr, uint32_t sct, uint32_t sc, uint32_t dnr,
    error_print_t print_on_error)
{
	struct nvme_completion	cpl;

	memset(&cpl, 0, sizeof(cpl));

	struct nvme_qpair * qpair = tr->qpair;

	cpl.sqid = qpair->id;
	cpl.cid = tr->cid;
	cpl.status |= (sct & NVME_STATUS_SCT_MASK) << NVME_STATUS_SCT_SHIFT;
	cpl.status |= (sc & NVME_STATUS_SC_MASK) << NVME_STATUS_SC_SHIFT;
	cpl.status |= (dnr & NVME_STATUS_DNR_MASK) << NVME_STATUS_DNR_SHIFT;
	nvme_qpair_complete_tracker(tr, &cpl, print_on_error);
}

void
nvme_qpair_manual_complete_request(struct nvme_qpair *qpair,
    struct nvme_request *req, uint32_t sct, uint32_t sc)
{
	struct nvme_completion	cpl;
	bool			error;

	memset(&cpl, 0, sizeof(cpl));
	cpl.sqid = qpair->id;
	cpl.status |= (sct & NVME_STATUS_SCT_MASK) << NVME_STATUS_SCT_SHIFT;
	cpl.status |= (sc & NVME_STATUS_SC_MASK) << NVME_STATUS_SC_SHIFT;

	error = nvme_completion_is_error(&cpl);

	if (error) {
		nvme_qpair_print_command(qpair, &req->cmd);
		nvme_qpair_print_completion(qpair, &cpl);
	}

	if (req->cb_fn)
		req->cb_fn(req->cb_arg, &cpl);

	nvme_free_request(req);
}

bool
nvme_qpair_process_completions(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;
	struct nvme_completion	cpl;
	int done = 0;
	bool in_panic = dumping || SCHEDULER_STOPPED();

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

		done++;
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
			/* nvme_dump_completion expects device endianess */
			nvme_dump_completion(&qpair->cpl[qpair->cq_head]);
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

	if (done != 0) {
		bus_space_write_4(qpair->ctrlr->bus_tag, qpair->ctrlr->bus_handle,
		    qpair->cq_hdbl_off, qpair->cq_head);
	}

	return (done != 0);
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

	callout_init(&qpair->timer, 1);
	qpair->timer_armed = false;
	qpair->recovery_state = RECOVERY_WAITING;

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

	tr = TAILQ_FIRST(&qpair->outstanding_tr);
	while (tr != NULL) {
		if (tr->req->cmd.opc == NVME_OPC_ASYNC_EVENT_REQUEST) {
			nvme_qpair_manual_complete_tracker(tr,
			    NVME_SCT_GENERIC, NVME_SC_ABORTED_SQ_DELETION, 0,
			    ERROR_PRINT_NONE);
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
	nvme_qpair_destroy(qpair);
}

void
nvme_io_qpair_destroy(struct nvme_qpair *qpair)
{

	nvme_qpair_destroy(qpair);
}

static void
nvme_qpair_timeout(void *arg)
{
	struct nvme_qpair	*qpair = arg;
	struct nvme_controller	*ctrlr = qpair->ctrlr;
	struct nvme_tracker	*tr;
	sbintime_t		now;
	bool			idle;
	uint32_t		csts;
	uint8_t			cfs;

	mtx_lock(&qpair->lock);
	idle = TAILQ_EMPTY(&qpair->outstanding_tr);
again:
	switch (qpair->recovery_state) {
	case RECOVERY_NONE:
		if (idle)
			break;
		now = getsbinuptime();
		idle = true;
		TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq) {
			if (tr->deadline == SBT_MAX)
				continue;
			idle = false;
			if (now > tr->deadline) {
				/*
				 * We're now passed our earliest deadline. We
				 * need to do expensive things to cope, but next
				 * time. Flag that and close the door to any
				 * further processing.
				 */
				qpair->recovery_state = RECOVERY_START;
				nvme_printf(ctrlr, "RECOVERY_START %jd vs %jd\n",
				    (uintmax_t)now, (uintmax_t)tr->deadline);
				break;
			}
		}
		break;
	case RECOVERY_START:
		/*
		 * Read csts to get value of cfs - controller fatal status.
		 * If no fatal status, try to call the completion routine, and
		 * if completes transactions, report a missed interrupt and
		 * return (this may need to be rate limited). Otherwise, if
		 * aborts are enabled and the controller is not reporting
		 * fatal status, abort the command. Otherwise, just reset the
		 * controller and hope for the best.
		 */
		csts = nvme_mmio_read_4(ctrlr, csts);
		cfs = (csts >> NVME_CSTS_REG_CFS_SHIFT) & NVME_CSTS_REG_CFS_MASK;
		if (cfs) {
			nvme_printf(ctrlr, "Controller in fatal status, resetting\n");
			qpair->recovery_state = RECOVERY_RESET;
			goto again;
		}
		mtx_unlock(&qpair->lock);
		if (nvme_qpair_process_completions(qpair)) {
			nvme_printf(ctrlr, "Completions present in output without an interrupt\n");
			qpair->recovery_state = RECOVERY_NONE;
		} else {
			nvme_printf(ctrlr, "timeout with nothing complete, resetting\n");
			qpair->recovery_state = RECOVERY_RESET;
			mtx_lock(&qpair->lock);
			goto again;
		}
		mtx_lock(&qpair->lock);
		break;
	case RECOVERY_RESET:
		/*
		 * If we get here due to a possible surprise hot-unplug event,
		 * then we let nvme_ctrlr_reset confirm and fail the
		 * controller.
		 */
		nvme_printf(ctrlr, "Resetting controller due to a timeout%s.\n",
		    (csts == 0xffffffff) ? " and possible hot unplug" :
		    (cfs ? " and fatal error status" : ""));
		nvme_printf(ctrlr, "RECOVERY_WAITING\n");
		qpair->recovery_state = RECOVERY_WAITING;
		nvme_ctrlr_reset(ctrlr);
		break;
	case RECOVERY_WAITING:
		nvme_printf(ctrlr, "waiting\n");
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
	mtx_unlock(&qpair->lock);
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
	bus_space_write_4(qpair->ctrlr->bus_tag, qpair->ctrlr->bus_handle,
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

	mtx_assert(&qpair->lock, MA_OWNED);

	tr = TAILQ_FIRST(&qpair->free_tr);
	req->qpair = qpair;

	if (tr == NULL || qpair->recovery_state != RECOVERY_NONE) {
		/*
		 * No tracker is available, or the qpair is disabled due to
		 *  an in-progress controller-level reset or controller
		 *  failure.
		 */

		if (qpair->ctrlr->is_failed) {
			/*
			 * The controller has failed, so fail the request.
			 */
			nvme_qpair_manual_complete_request(qpair, req,
			    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST);
		} else {
			/*
			 * Put the request on the qpair's request queue to be
			 *  processed when a tracker frees up via a command
			 *  completion or when the controller reset is
			 *  completed.
			 */
			STAILQ_INSERT_TAIL(&qpair->queued_req, req, stailq);
		}
		return;
	}

	TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
	TAILQ_INSERT_TAIL(&qpair->outstanding_tr, tr, tailq);
	if (!qpair->timer_armed)
		tr->deadline = SBT_MAX;
	tr->req = req;

	switch (req->type) {
	case NVME_REQUEST_VADDR:
		KASSERT(req->payload_size <= qpair->ctrlr->max_xfer_size,
		    ("payload_size (%d) exceeds max_xfer_size (%d)\n",
		    req->payload_size, qpair->ctrlr->max_xfer_size));
		err = bus_dmamap_load(tr->qpair->dma_tag_payload,
		    tr->payload_dma_map, req->u.payload, req->payload_size,
		    nvme_payload_map, tr, 0);
		if (err != 0)
			nvme_printf(qpair->ctrlr,
			    "bus_dmamap_load returned 0x%x!\n", err);
		break;
	case NVME_REQUEST_NULL:
		nvme_qpair_submit_tracker(tr->qpair, tr);
		break;
	case NVME_REQUEST_BIO:
		KASSERT(req->u.bio->bio_bcount <= qpair->ctrlr->max_xfer_size,
		    ("bio->bio_bcount (%jd) exceeds max_xfer_size (%d)\n",
		    (intmax_t)req->u.bio->bio_bcount,
		    qpair->ctrlr->max_xfer_size));
		err = bus_dmamap_load_bio(tr->qpair->dma_tag_payload,
		    tr->payload_dma_map, req->u.bio, nvme_payload_map, tr, 0);
		if (err != 0)
			nvme_printf(qpair->ctrlr,
			    "bus_dmamap_load_bio returned 0x%x!\n", err);
		break;
	case NVME_REQUEST_CCB:
		err = bus_dmamap_load_ccb(tr->qpair->dma_tag_payload,
		    tr->payload_dma_map, req->u.payload,
		    nvme_payload_map, tr, 0);
		if (err != 0)
			nvme_printf(qpair->ctrlr,
			    "bus_dmamap_load_ccb returned 0x%x!\n", err);
		break;
	default:
		panic("unknown nvme request type 0x%x\n", req->type);
		break;
	}

	if (err != 0) {
		/*
		 * The dmamap operation failed, so we manually fail the
		 *  tracker here with DATA_TRANSFER_ERROR status.
		 *
		 * nvme_qpair_manual_complete_tracker must not be called
		 *  with the qpair lock held.
		 */
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
	mtx_assert(&qpair->lock, MA_OWNED);

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

	/*
	 * Manually abort each outstanding admin command.  Do not retry
	 *  admin commands found here, since they will be left over from
	 *  a controller reset and its likely the context in which the
	 *  command was issued no longer applies.
	 */
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_printf(qpair->ctrlr,
		    "aborting outstanding admin command\n");
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, DO_NOT_RETRY, ERROR_PRINT_ALL);
	}

	mtx_lock(&qpair->lock);
	nvme_qpair_enable(qpair);
	mtx_unlock(&qpair->lock);
}

void
nvme_io_qpair_enable(struct nvme_qpair *qpair)
{
	STAILQ_HEAD(, nvme_request)	temp;
	struct nvme_tracker		*tr;
	struct nvme_tracker		*tr_temp;
	struct nvme_request		*req;

	/*
	 * Manually abort each outstanding I/O.  This normally results in a
	 *  retry, unless the retry count on the associated request has
	 *  reached its limit.
	 */
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_printf(qpair->ctrlr, "aborting outstanding i/o\n");
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 0, ERROR_PRINT_NO_RETRY);
	}

	mtx_lock(&qpair->lock);

	nvme_qpair_enable(qpair);

	STAILQ_INIT(&temp);
	STAILQ_SWAP(&qpair->queued_req, &temp, nvme_request);

	while (!STAILQ_EMPTY(&temp)) {
		req = STAILQ_FIRST(&temp);
		STAILQ_REMOVE_HEAD(&temp, stailq);
		nvme_printf(qpair->ctrlr, "resubmitting queued i/o\n");
		nvme_qpair_print_command(qpair, &req->cmd);
		_nvme_qpair_submit_request(qpair, req);
	}

	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_disable(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr, *tr_temp;

	mtx_lock(&qpair->lock);
	qpair->recovery_state = RECOVERY_WAITING;
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		tr->deadline = SBT_MAX;
	}
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

void
nvme_qpair_fail(struct nvme_qpair *qpair)
{
	struct nvme_tracker		*tr;
	struct nvme_request		*req;

	if (!mtx_initialized(&qpair->lock))
		return;

	mtx_lock(&qpair->lock);

	while (!STAILQ_EMPTY(&qpair->queued_req)) {
		req = STAILQ_FIRST(&qpair->queued_req);
		STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
		nvme_printf(qpair->ctrlr, "failing queued i/o\n");
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_request(qpair, req, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST);
		mtx_lock(&qpair->lock);
	}

	/* Manually abort each outstanding I/O. */
	while (!TAILQ_EMPTY(&qpair->outstanding_tr)) {
		tr = TAILQ_FIRST(&qpair->outstanding_tr);
		/*
		 * Do not remove the tracker.  The abort_tracker path will
		 *  do that for us.
		 */
		nvme_printf(qpair->ctrlr, "failing outstanding i/o\n");
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, DO_NOT_RETRY, ERROR_PRINT_ALL);
		mtx_lock(&qpair->lock);
	}

	mtx_unlock(&qpair->lock);
}
