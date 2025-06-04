/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Netflix, Inc.
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

#ifdef _KERNEL
#include "opt_scsi.h"

#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#else
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt.h>
#include <cam/nvme/nvme_all.h>
#include <sys/sbuf.h>
#include <sys/endian.h>

#ifdef _KERNEL
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#endif

/* XXX: This duplicates lists in nvme_qpair.c. */

#define OPC_ENTRY(x)		[NVME_OPC_ ## x] = #x

static const char *admin_opcode[256] = {
	OPC_ENTRY(DELETE_IO_SQ),
	OPC_ENTRY(CREATE_IO_SQ),
	OPC_ENTRY(GET_LOG_PAGE),
	OPC_ENTRY(DELETE_IO_CQ),
	OPC_ENTRY(CREATE_IO_CQ),
	OPC_ENTRY(IDENTIFY),
	OPC_ENTRY(ABORT),
	OPC_ENTRY(SET_FEATURES),
	OPC_ENTRY(GET_FEATURES),
	OPC_ENTRY(ASYNC_EVENT_REQUEST),
	OPC_ENTRY(NAMESPACE_MANAGEMENT),
	OPC_ENTRY(FIRMWARE_ACTIVATE),
	OPC_ENTRY(FIRMWARE_IMAGE_DOWNLOAD),
	OPC_ENTRY(DEVICE_SELF_TEST),
	OPC_ENTRY(NAMESPACE_ATTACHMENT),
	OPC_ENTRY(KEEP_ALIVE),
	OPC_ENTRY(DIRECTIVE_SEND),
	OPC_ENTRY(DIRECTIVE_RECEIVE),
	OPC_ENTRY(VIRTUALIZATION_MANAGEMENT),
	OPC_ENTRY(NVME_MI_SEND),
	OPC_ENTRY(NVME_MI_RECEIVE),
	OPC_ENTRY(CAPACITY_MANAGEMENT),
	OPC_ENTRY(LOCKDOWN),
	OPC_ENTRY(DOORBELL_BUFFER_CONFIG),
	OPC_ENTRY(FABRICS_COMMANDS),
	OPC_ENTRY(FORMAT_NVM),
	OPC_ENTRY(SECURITY_SEND),
	OPC_ENTRY(SECURITY_RECEIVE),
	OPC_ENTRY(SANITIZE),
	OPC_ENTRY(GET_LBA_STATUS),
};

static const char *nvm_opcode[256] = {
	OPC_ENTRY(FLUSH),
	OPC_ENTRY(WRITE),
	OPC_ENTRY(READ),
	OPC_ENTRY(WRITE_UNCORRECTABLE),
	OPC_ENTRY(COMPARE),
	OPC_ENTRY(WRITE_ZEROES),
	OPC_ENTRY(DATASET_MANAGEMENT),
	OPC_ENTRY(VERIFY),
	OPC_ENTRY(RESERVATION_REGISTER),
	OPC_ENTRY(RESERVATION_REPORT),
	OPC_ENTRY(RESERVATION_ACQUIRE),
	OPC_ENTRY(RESERVATION_RELEASE),
	OPC_ENTRY(COPY),
};

#define SC_ENTRY(x)		[NVME_SC_ ## x] = #x

static const char *generic_status[256] = {
	SC_ENTRY(SUCCESS),
	SC_ENTRY(INVALID_OPCODE),
	SC_ENTRY(INVALID_FIELD),
	SC_ENTRY(COMMAND_ID_CONFLICT),
	SC_ENTRY(DATA_TRANSFER_ERROR),
	SC_ENTRY(ABORTED_POWER_LOSS),
	SC_ENTRY(INTERNAL_DEVICE_ERROR),
	SC_ENTRY(ABORTED_BY_REQUEST),
	SC_ENTRY(ABORTED_SQ_DELETION),
	SC_ENTRY(ABORTED_FAILED_FUSED),
	SC_ENTRY(ABORTED_MISSING_FUSED),
	SC_ENTRY(INVALID_NAMESPACE_OR_FORMAT),
	SC_ENTRY(COMMAND_SEQUENCE_ERROR),
	SC_ENTRY(INVALID_SGL_SEGMENT_DESCR),
	SC_ENTRY(INVALID_NUMBER_OF_SGL_DESCR),
	SC_ENTRY(DATA_SGL_LENGTH_INVALID),
	SC_ENTRY(METADATA_SGL_LENGTH_INVALID),
	SC_ENTRY(SGL_DESCRIPTOR_TYPE_INVALID),
	SC_ENTRY(INVALID_USE_OF_CMB),
	SC_ENTRY(PRP_OFFET_INVALID),
	SC_ENTRY(ATOMIC_WRITE_UNIT_EXCEEDED),
	SC_ENTRY(OPERATION_DENIED),
	SC_ENTRY(SGL_OFFSET_INVALID),
	SC_ENTRY(HOST_ID_INCONSISTENT_FORMAT),
	SC_ENTRY(KEEP_ALIVE_TIMEOUT_EXPIRED),
	SC_ENTRY(KEEP_ALIVE_TIMEOUT_INVALID),
	SC_ENTRY(ABORTED_DUE_TO_PREEMPT),
	SC_ENTRY(SANITIZE_FAILED),
	SC_ENTRY(SANITIZE_IN_PROGRESS),
	SC_ENTRY(SGL_DATA_BLOCK_GRAN_INVALID),
	SC_ENTRY(NOT_SUPPORTED_IN_CMB),
	SC_ENTRY(NAMESPACE_IS_WRITE_PROTECTED),
	SC_ENTRY(COMMAND_INTERRUPTED),
	SC_ENTRY(TRANSIENT_TRANSPORT_ERROR),

	SC_ENTRY(LBA_OUT_OF_RANGE),
	SC_ENTRY(CAPACITY_EXCEEDED),
	SC_ENTRY(NAMESPACE_NOT_READY),
	SC_ENTRY(RESERVATION_CONFLICT),
	SC_ENTRY(FORMAT_IN_PROGRESS),
};

static const char *command_specific_status[256] = {
	SC_ENTRY(COMPLETION_QUEUE_INVALID),
	SC_ENTRY(INVALID_QUEUE_IDENTIFIER),
	SC_ENTRY(MAXIMUM_QUEUE_SIZE_EXCEEDED),
	SC_ENTRY(ABORT_COMMAND_LIMIT_EXCEEDED),
	SC_ENTRY(ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED),
	SC_ENTRY(INVALID_FIRMWARE_SLOT),
	SC_ENTRY(INVALID_FIRMWARE_IMAGE),
	SC_ENTRY(INVALID_INTERRUPT_VECTOR),
	SC_ENTRY(INVALID_LOG_PAGE),
	SC_ENTRY(INVALID_FORMAT),
	SC_ENTRY(FIRMWARE_REQUIRES_RESET),
	SC_ENTRY(INVALID_QUEUE_DELETION),
	SC_ENTRY(FEATURE_NOT_SAVEABLE),
	SC_ENTRY(FEATURE_NOT_CHANGEABLE),
	SC_ENTRY(FEATURE_NOT_NS_SPECIFIC),
	SC_ENTRY(FW_ACT_REQUIRES_NVMS_RESET),
	SC_ENTRY(FW_ACT_REQUIRES_RESET),
	SC_ENTRY(FW_ACT_REQUIRES_TIME),
	SC_ENTRY(FW_ACT_PROHIBITED),
	SC_ENTRY(OVERLAPPING_RANGE),
	SC_ENTRY(NS_INSUFFICIENT_CAPACITY),
	SC_ENTRY(NS_ID_UNAVAILABLE),
	SC_ENTRY(NS_ALREADY_ATTACHED),
	SC_ENTRY(NS_IS_PRIVATE),
	SC_ENTRY(NS_NOT_ATTACHED),
	SC_ENTRY(THIN_PROV_NOT_SUPPORTED),
	SC_ENTRY(CTRLR_LIST_INVALID),
	SC_ENTRY(SELF_TEST_IN_PROGRESS),
	SC_ENTRY(BOOT_PART_WRITE_PROHIB),
	SC_ENTRY(INVALID_CTRLR_ID),
	SC_ENTRY(INVALID_SEC_CTRLR_STATE),
	SC_ENTRY(INVALID_NUM_OF_CTRLR_RESRC),
	SC_ENTRY(INVALID_RESOURCE_ID),
	SC_ENTRY(SANITIZE_PROHIBITED_WPMRE),
	SC_ENTRY(ANA_GROUP_ID_INVALID),
	SC_ENTRY(ANA_ATTACH_FAILED),

	SC_ENTRY(CONFLICTING_ATTRIBUTES),
	SC_ENTRY(INVALID_PROTECTION_INFO),
	SC_ENTRY(ATTEMPTED_WRITE_TO_RO_PAGE),
};

static const char *media_error_status[256] = {
	SC_ENTRY(WRITE_FAULTS),
	SC_ENTRY(UNRECOVERED_READ_ERROR),
	SC_ENTRY(GUARD_CHECK_ERROR),
	SC_ENTRY(APPLICATION_TAG_CHECK_ERROR),
	SC_ENTRY(REFERENCE_TAG_CHECK_ERROR),
	SC_ENTRY(COMPARE_FAILURE),
	SC_ENTRY(ACCESS_DENIED),
	SC_ENTRY(DEALLOCATED_OR_UNWRITTEN),
};

static const char *path_related_status[256] = {
	SC_ENTRY(INTERNAL_PATH_ERROR),
	SC_ENTRY(ASYMMETRIC_ACCESS_PERSISTENT_LOSS),
	SC_ENTRY(ASYMMETRIC_ACCESS_INACCESSIBLE),
	SC_ENTRY(ASYMMETRIC_ACCESS_TRANSITION),
	SC_ENTRY(CONTROLLER_PATHING_ERROR),
	SC_ENTRY(HOST_PATHING_ERROR),
	SC_ENTRY(COMMAND_ABORTED_BY_HOST),
};

void
nvme_ns_cmd(struct ccb_nvmeio *nvmeio, uint8_t cmd, uint32_t nsid,
    uint32_t cdw10, uint32_t cdw11, uint32_t cdw12, uint32_t cdw13,
    uint32_t cdw14, uint32_t cdw15)
{
	bzero(&nvmeio->cmd, sizeof(struct nvme_command));
	nvmeio->cmd.opc = cmd;
	nvmeio->cmd.nsid = htole32(nsid);
	nvmeio->cmd.cdw10 = htole32(cdw10);
	nvmeio->cmd.cdw11 = htole32(cdw11);
	nvmeio->cmd.cdw12 = htole32(cdw12);
	nvmeio->cmd.cdw13 = htole32(cdw13);
	nvmeio->cmd.cdw14 = htole32(cdw14);
	nvmeio->cmd.cdw15 = htole32(cdw15);
}

int
nvme_identify_match(caddr_t identbuffer, caddr_t table_entry)
{
	return 0;
}

void
nvme_print_ident(const struct nvme_controller_data *cdata,
    const struct nvme_namespace_data *data, struct sbuf *sb)
{
	nvme_print_ident_short(cdata, data, sb);
	sbuf_putc(sb, '\n');
}

void
nvme_print_ident_short(const struct nvme_controller_data *cdata,
    const struct nvme_namespace_data *data, struct sbuf *sb)
{
	sbuf_putc(sb, '<');
	cam_strvis_sbuf(sb, cdata->mn, sizeof(cdata->mn),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, ' ');
	cam_strvis_sbuf(sb, cdata->fr, sizeof(cdata->fr),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, ' ');
	cam_strvis_sbuf(sb, cdata->sn, sizeof(cdata->sn),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, '>');
}

const char *
nvme_command_string(struct ccb_nvmeio *nvmeio, char *cmd_string, size_t len)
{
	struct sbuf sb;
	int error;

	if (len == 0)
		return ("");

	sbuf_new(&sb, cmd_string, len, SBUF_FIXEDLEN);
	nvme_command_sbuf(nvmeio, &sb);

	error = sbuf_finish(&sb);
	if (error != 0 &&
#ifdef _KERNEL
	    error != ENOMEM)
#else
	    errno != ENOMEM)
#endif
		return ("");

	return(sbuf_data(&sb));
}

void
nvme_opcode_sbuf(bool admin, uint8_t opc, struct sbuf *sb)
{
	const char *s, *type;

	if (admin) {
		s = admin_opcode[opc];
		type = "ADMIN";
	} else {
		s = nvm_opcode[opc];
		type = "NVM";
	}
	if (s == NULL)
		sbuf_printf(sb, "%s:0x%02x", type, opc);
	else
		sbuf_printf(sb, "%s", s);
}

void
nvme_cmd_sbuf(const struct nvme_command *cmd, struct sbuf *sb)
{

	/*
	 * cid, rsvd areas and mptr not printed, since they are used
	 * only internally by the SIM.
	 */
	sbuf_printf(sb,
	    "opc=%x fuse=%x nsid=%x prp1=%llx prp2=%llx cdw=%x %x %x %x %x %x",
	    cmd->opc, cmd->fuse, cmd->nsid,
	    (unsigned long long)cmd->prp1, (unsigned long long)cmd->prp2,
	    cmd->cdw10, cmd->cdw11, cmd->cdw12,
	    cmd->cdw13, cmd->cdw14, cmd->cdw15);
}

/*
 * nvme_command_sbuf() returns 0 for success and -1 for failure.
 */
int
nvme_command_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb)
{

	nvme_opcode_sbuf(nvmeio->ccb_h.func_code == XPT_NVME_ADMIN,
	    nvmeio->cmd.opc, sb);
	sbuf_cat(sb, ". NCB: ");
	nvme_cmd_sbuf(&nvmeio->cmd, sb);
	return(0);
}

void
nvme_cpl_sbuf(const struct nvme_completion *cpl, struct sbuf *sb)
{
	const char *s, *type;
	uint16_t status;

	status = le16toh(cpl->status);
	switch (NVME_STATUS_GET_SCT(status)) {
	case NVME_SCT_GENERIC:
		s = generic_status[NVME_STATUS_GET_SC(status)];
		type = "GENERIC";
		break;
	case NVME_SCT_COMMAND_SPECIFIC:
		s = command_specific_status[NVME_STATUS_GET_SC(status)];
		type = "COMMAND SPECIFIC";
		break;
	case NVME_SCT_MEDIA_ERROR:
		s = media_error_status[NVME_STATUS_GET_SC(status)];
		type = "MEDIA ERROR";
		break;
	case NVME_SCT_PATH_RELATED:
		s = path_related_status[NVME_STATUS_GET_SC(status)];
		type = "PATH RELATED";
		break;
	case NVME_SCT_VENDOR_SPECIFIC:
		s = NULL;
		type = "VENDOR SPECIFIC";
		break;
	default:
		s = "RESERVED";
		type = NULL;
		break;
	}

	if (s == NULL)
		sbuf_printf(sb, "%s:0x%02x", type, NVME_STATUS_GET_SC(status));
	else
		sbuf_printf(sb, "%s", s);
	if (NVME_STATUS_GET_M(status) != 0)
		sbuf_printf(sb, " M");
	if (NVME_STATUS_GET_DNR(status) != 0)
		sbuf_printf(sb, " DNR");
}

/*
 * nvme_status_sbuf() returns 0 for success and -1 for failure.
 */
int
nvme_status_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb)
{
	nvme_cpl_sbuf(&nvmeio->cpl, sb);
	return (0);
}

#ifdef _KERNEL
const void *
nvme_get_identify_cntrl(struct cam_periph *periph)
{
	struct cam_ed *device;

	device = periph->path->device;

	return device->nvme_cdata;
}

const void *
nvme_get_identify_ns(struct cam_periph *periph)
{
	struct cam_ed *device;

	device = periph->path->device;

	return device->nvme_data;
}
#endif
