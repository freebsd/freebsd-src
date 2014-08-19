/*-
 * Copyright (c) 2014 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <machine/atomic.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <cam/ctl/ctl_tpc.h>
#include <cam/ctl/ctl_error.h>

#define	TPC_MAX_CSCDS	64
#define	TPC_MAX_SEGS	64
#define	TPC_MAX_SEG	0
#define	TPC_MAX_LIST	8192
#define	TPC_MAX_INLINE	0
#define	TPC_MAX_LISTS	255
#define	TPC_MAX_IO_SIZE	(1024 * 1024)

MALLOC_DEFINE(M_CTL_TPC, "ctltpc", "CTL TPC");

typedef enum {
	TPC_ERR_RETRY		= 0x000,
	TPC_ERR_FAIL		= 0x001,
	TPC_ERR_MASK		= 0x0ff,
	TPC_ERR_NO_DECREMENT	= 0x100
} tpc_error_action;

struct tpc_list;
TAILQ_HEAD(runl, tpc_io);
struct tpc_io {
	union ctl_io		*io;
	uint64_t		 lun;
	struct tpc_list		*list;
	struct runl		 run;
	TAILQ_ENTRY(tpc_io)	 rlinks;
	TAILQ_ENTRY(tpc_io)	 links;
};

struct tpc_list {
	uint8_t			 service_action;
	int			 init_port;
	uint32_t		 init_idx;
	uint32_t		 list_id;
	uint8_t			 flags;
	uint8_t			*params;
	struct scsi_ec_cscd	*cscd;
	struct scsi_ec_segment	*seg[TPC_MAX_SEGS];
	uint8_t			*inl;
	int			 ncscd;
	int			 nseg;
	int			 leninl;
	int			 curseg;
	off_t			 curbytes;
	int			 curops;
	int			 stage;
	uint8_t			*buf;
	int			 segbytes;
	int			 tbdio;
	int			 error;
	int			 abort;
	int			 completed;
	TAILQ_HEAD(, tpc_io)	 allio;
	struct scsi_sense_data	 sense_data;
	uint8_t			 sense_len;
	uint8_t			 scsi_status;
	struct ctl_scsiio	*ctsio;
	struct ctl_lun		*lun;
	TAILQ_ENTRY(tpc_list)	 links;
};

void
ctl_tpc_init(struct ctl_lun *lun)
{

	TAILQ_INIT(&lun->tpc_lists);
}

void
ctl_tpc_shutdown(struct ctl_lun *lun)
{
	struct tpc_list *list;

	while ((list = TAILQ_FIRST(&lun->tpc_lists)) != NULL) {
		TAILQ_REMOVE(&lun->tpc_lists, list, links);
		KASSERT(list->completed,
		    ("Not completed TPC (%p) on shutdown", list));
		free(list, M_CTL);
	}
}

int
ctl_inquiry_evpd_tpc(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_tpc *tpc_ptr;
	struct scsi_vpd_tpc_descriptor *d_ptr;
	struct scsi_vpd_tpc_descriptor_sc *sc_ptr;
	struct scsi_vpd_tpc_descriptor_sc_descr *scd_ptr;
	struct scsi_vpd_tpc_descriptor_pd *pd_ptr;
	struct scsi_vpd_tpc_descriptor_sd *sd_ptr;
	struct scsi_vpd_tpc_descriptor_sdid *sdid_ptr;
	struct scsi_vpd_tpc_descriptor_gco *gco_ptr;
	struct ctl_lun *lun;
	int data_len;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	data_len = sizeof(struct scsi_vpd_tpc) +
	    roundup2(sizeof(struct scsi_vpd_tpc_descriptor_sc) +
	     2 * sizeof(struct scsi_vpd_tpc_descriptor_sc_descr) + 7, 4) +
	    sizeof(struct scsi_vpd_tpc_descriptor_pd) +
	    roundup2(sizeof(struct scsi_vpd_tpc_descriptor_sd) + 4, 4) +
	    roundup2(sizeof(struct scsi_vpd_tpc_descriptor_sdid) + 2, 4) +
	    sizeof(struct scsi_vpd_tpc_descriptor_gco);

	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	tpc_ptr = (struct scsi_vpd_tpc *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.
	 */
	if (lun != NULL)
		tpc_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				     lun->be_lun->lun_type;
	else
		tpc_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	tpc_ptr->page_code = SVPD_SCSI_TPC;
	scsi_ulto2b(data_len - 4, tpc_ptr->page_length);

	/* Supported commands */
	d_ptr = (struct scsi_vpd_tpc_descriptor *)&tpc_ptr->descr[0];
	sc_ptr = (struct scsi_vpd_tpc_descriptor_sc *)d_ptr;
	scsi_ulto2b(SVPD_TPC_SC, sc_ptr->desc_type);
	sc_ptr->list_length = 2 * sizeof(*scd_ptr) + 7;
	scsi_ulto2b(roundup2(1 + sc_ptr->list_length, 4), sc_ptr->desc_length);
	scd_ptr = &sc_ptr->descr[0];
	scd_ptr->opcode = EXTENDED_COPY;
	scd_ptr->sa_length = 3;
	scd_ptr->supported_service_actions[0] = EC_EC_LID1;
	scd_ptr->supported_service_actions[1] = EC_EC_LID4;
	scd_ptr->supported_service_actions[2] = EC_COA;
	scd_ptr = (struct scsi_vpd_tpc_descriptor_sc_descr *)
	    &scd_ptr->supported_service_actions[scd_ptr->sa_length];
	scd_ptr->opcode = RECEIVE_COPY_STATUS;
	scd_ptr->sa_length = 4;
	scd_ptr->supported_service_actions[0] = RCS_RCS_LID1;
	scd_ptr->supported_service_actions[1] = RCS_RCFD;
	scd_ptr->supported_service_actions[2] = RCS_RCS_LID4;
	scd_ptr->supported_service_actions[3] = RCS_RCOP;

	/* Parameter data. */
	d_ptr = (struct scsi_vpd_tpc_descriptor *)
	    (&d_ptr->parameters[0] + scsi_2btoul(d_ptr->desc_length));
	pd_ptr = (struct scsi_vpd_tpc_descriptor_pd *)d_ptr;
	scsi_ulto2b(SVPD_TPC_PD, pd_ptr->desc_type);
	scsi_ulto2b(sizeof(*pd_ptr) - 4, pd_ptr->desc_length);
	scsi_ulto2b(TPC_MAX_CSCDS, pd_ptr->maximum_cscd_descriptor_count);
	scsi_ulto2b(TPC_MAX_SEGS, pd_ptr->maximum_segment_descriptor_count);
	scsi_ulto4b(TPC_MAX_LIST, pd_ptr->maximum_descriptor_list_length);
	scsi_ulto4b(TPC_MAX_INLINE, pd_ptr->maximum_inline_data_length);

	/* Supported Descriptors */
	d_ptr = (struct scsi_vpd_tpc_descriptor *)
	    (&d_ptr->parameters[0] + scsi_2btoul(d_ptr->desc_length));
	sd_ptr = (struct scsi_vpd_tpc_descriptor_sd *)d_ptr;
	scsi_ulto2b(SVPD_TPC_SD, sd_ptr->desc_type);
	scsi_ulto2b(roundup2(sizeof(*sd_ptr) - 4 + 4, 4), sd_ptr->desc_length);
	sd_ptr->list_length = 4;
	sd_ptr->supported_descriptor_codes[0] = EC_SEG_B2B;
	sd_ptr->supported_descriptor_codes[1] = EC_SEG_VERIFY;
	sd_ptr->supported_descriptor_codes[2] = EC_SEG_REGISTER_KEY;
	sd_ptr->supported_descriptor_codes[3] = EC_CSCD_ID;

	/* Supported CSCD Descriptor IDs */
	d_ptr = (struct scsi_vpd_tpc_descriptor *)
	    (&d_ptr->parameters[0] + scsi_2btoul(d_ptr->desc_length));
	sdid_ptr = (struct scsi_vpd_tpc_descriptor_sdid *)d_ptr;
	scsi_ulto2b(SVPD_TPC_SDID, sdid_ptr->desc_type);
	scsi_ulto2b(roundup2(sizeof(*sdid_ptr) - 4 + 2, 4), sdid_ptr->desc_length);
	scsi_ulto2b(2, sdid_ptr->list_length);
	scsi_ulto2b(0xffff, &sdid_ptr->supported_descriptor_ids[0]);

	/* General Copy Operations */
	d_ptr = (struct scsi_vpd_tpc_descriptor *)
	    (&d_ptr->parameters[0] + scsi_2btoul(d_ptr->desc_length));
	gco_ptr = (struct scsi_vpd_tpc_descriptor_gco *)d_ptr;
	scsi_ulto2b(SVPD_TPC_GCO, gco_ptr->desc_type);
	scsi_ulto2b(sizeof(*gco_ptr) - 4, gco_ptr->desc_length);
	scsi_ulto4b(TPC_MAX_LISTS, gco_ptr->total_concurrent_copies);
	scsi_ulto4b(TPC_MAX_LISTS, gco_ptr->maximum_identified_concurrent_copies);
	scsi_ulto4b(TPC_MAX_SEG, gco_ptr->maximum_segment_length);
	gco_ptr->data_segment_granularity = 0;
	gco_ptr->inline_data_granularity = 0;

	ctsio->scsi_status = SCSI_STATUS_OK;
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);

	return (CTL_RETVAL_COMPLETE);
}

int
ctl_receive_copy_operating_parameters(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_receive_copy_operating_parameters *cdb;
	struct scsi_receive_copy_operating_parameters_data *data;
	int retval;
	int alloc_len, total_len;

	CTL_DEBUG_PRINT(("ctl_report_supported_tmf\n"));

	cdb = (struct scsi_receive_copy_operating_parameters *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	total_len = sizeof(*data) + 4;
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_receive_copy_operating_parameters_data *)ctsio->kern_data_ptr;
	scsi_ulto4b(sizeof(*data) - 4 + 4, data->length);
	data->snlid = RCOP_SNLID;
	scsi_ulto2b(TPC_MAX_CSCDS, data->maximum_cscd_descriptor_count);
	scsi_ulto2b(TPC_MAX_SEGS, data->maximum_segment_descriptor_count);
	scsi_ulto4b(TPC_MAX_LIST, data->maximum_descriptor_list_length);
	scsi_ulto4b(TPC_MAX_SEG, data->maximum_segment_length);
	scsi_ulto4b(TPC_MAX_INLINE, data->maximum_inline_data_length);
	scsi_ulto4b(0, data->held_data_limit);
	scsi_ulto4b(0, data->maximum_stream_device_transfer_size);
	scsi_ulto2b(TPC_MAX_LISTS, data->total_concurrent_copies);
	data->maximum_concurrent_copies = TPC_MAX_LISTS;
	data->data_segment_granularity = 0;
	data->inline_data_granularity = 0;
	data->held_data_granularity = 0;
	data->implemented_descriptor_list_length = 4;
	data->list_of_implemented_descriptor_type_codes[0] = EC_SEG_B2B;
	data->list_of_implemented_descriptor_type_codes[1] = EC_SEG_VERIFY;
	data->list_of_implemented_descriptor_type_codes[2] = EC_SEG_REGISTER_KEY;
	data->list_of_implemented_descriptor_type_codes[3] = EC_CSCD_ID;

	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;

	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_receive_copy_status_lid1(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_receive_copy_status_lid1 *cdb;
	struct scsi_receive_copy_status_lid1_data *data;
	struct tpc_list *list;
	struct tpc_list list_copy;
	int retval;
	int alloc_len, total_len;
	uint32_t list_id;

	CTL_DEBUG_PRINT(("ctl_receive_copy_status_lid1\n"));

	cdb = (struct scsi_receive_copy_status_lid1 *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	list_id = cdb->list_identifier;
	mtx_lock(&lun->lun_lock);
	TAILQ_FOREACH(list, &lun->tpc_lists, links) {
		if ((list->flags & EC_LIST_ID_USAGE_MASK) !=
		     EC_LIST_ID_USAGE_NONE && list->list_id == list_id)
			break;
	}
	if (list == NULL) {
		mtx_unlock(&lun->lun_lock);
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
		    /*command*/ 1, /*field*/ 2, /*bit_valid*/ 0,
		    /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}
	list_copy = *list;
	if (list->completed) {
		TAILQ_REMOVE(&lun->tpc_lists, list, links);
		free(list, M_CTL);
	}
	mtx_unlock(&lun->lun_lock);

	total_len = sizeof(*data);
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_receive_copy_status_lid1_data *)ctsio->kern_data_ptr;
	scsi_ulto4b(sizeof(*data) - 4, data->available_data);
	if (list_copy.completed) {
		if (list_copy.error || list_copy.abort)
			data->copy_command_status = RCS_CCS_ERROR;
		else
			data->copy_command_status = RCS_CCS_COMPLETED;
	} else
		data->copy_command_status = RCS_CCS_INPROG;
	scsi_ulto2b(list_copy.curseg, data->segments_processed);
	if (list_copy.curbytes <= UINT32_MAX) {
		data->transfer_count_units = RCS_TC_BYTES;
		scsi_ulto4b(list_copy.curbytes, data->transfer_count);
	} else {
		data->transfer_count_units = RCS_TC_MBYTES;
		scsi_ulto4b(list_copy.curbytes >> 20, data->transfer_count);
	}

	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;

	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_receive_copy_failure_details(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_receive_copy_failure_details *cdb;
	struct scsi_receive_copy_failure_details_data *data;
	struct tpc_list *list;
	struct tpc_list list_copy;
	int retval;
	int alloc_len, total_len;
	uint32_t list_id;

	CTL_DEBUG_PRINT(("ctl_receive_copy_failure_details\n"));

	cdb = (struct scsi_receive_copy_failure_details *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	list_id = cdb->list_identifier;
	mtx_lock(&lun->lun_lock);
	TAILQ_FOREACH(list, &lun->tpc_lists, links) {
		if (list->completed && (list->flags & EC_LIST_ID_USAGE_MASK) !=
		     EC_LIST_ID_USAGE_NONE && list->list_id == list_id)
			break;
	}
	if (list == NULL) {
		mtx_unlock(&lun->lun_lock);
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
		    /*command*/ 1, /*field*/ 2, /*bit_valid*/ 0,
		    /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}
	list_copy = *list;
	TAILQ_REMOVE(&lun->tpc_lists, list, links);
	free(list, M_CTL);
	mtx_unlock(&lun->lun_lock);

	total_len = sizeof(*data) + list_copy.sense_len;
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_receive_copy_failure_details_data *)ctsio->kern_data_ptr;
	if (list_copy.completed && (list_copy.error || list_copy.abort)) {
		scsi_ulto4b(sizeof(*data) - 4, data->available_data);
		data->copy_command_status = RCS_CCS_ERROR;
	} else
		scsi_ulto4b(0, data->available_data);
	scsi_ulto2b(list_copy.sense_len, data->sense_data_length);
	memcpy(data->sense_data, &list_copy.sense_data, list_copy.sense_len);

	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;

	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_receive_copy_status_lid4(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_receive_copy_status_lid4 *cdb;
	struct scsi_receive_copy_status_lid4_data *data;
	struct tpc_list *list;
	struct tpc_list list_copy;
	int retval;
	int alloc_len, total_len;
	uint32_t list_id;

	CTL_DEBUG_PRINT(("ctl_receive_copy_status_lid4\n"));

	cdb = (struct scsi_receive_copy_status_lid4 *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	list_id = scsi_4btoul(cdb->list_identifier);
	mtx_lock(&lun->lun_lock);
	TAILQ_FOREACH(list, &lun->tpc_lists, links) {
		if ((list->flags & EC_LIST_ID_USAGE_MASK) !=
		     EC_LIST_ID_USAGE_NONE && list->list_id == list_id)
			break;
	}
	if (list == NULL) {
		mtx_unlock(&lun->lun_lock);
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
		    /*command*/ 1, /*field*/ 2, /*bit_valid*/ 0,
		    /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}
	list_copy = *list;
	if (list->completed) {
		TAILQ_REMOVE(&lun->tpc_lists, list, links);
		free(list, M_CTL);
	}
	mtx_unlock(&lun->lun_lock);

	total_len = sizeof(*data) + list_copy.sense_len;
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_receive_copy_status_lid4_data *)ctsio->kern_data_ptr;
	scsi_ulto4b(sizeof(*data) - 4, data->available_data);
	data->response_to_service_action = list_copy.service_action;
	if (list_copy.completed) {
		if (list_copy.error)
			data->copy_command_status = RCS_CCS_ERROR;
		else if (list_copy.abort)
			data->copy_command_status = RCS_CCS_ABORTED;
		else
			data->copy_command_status = RCS_CCS_COMPLETED;
	} else
		data->copy_command_status = RCS_CCS_INPROG_FG;
	scsi_ulto2b(list_copy.curops, data->operation_counter);
	scsi_ulto4b(UINT32_MAX, data->estimated_status_update_delay);
	if (list_copy.curbytes <= UINT32_MAX) {
		data->transfer_count_units = RCS_TC_BYTES;
		scsi_ulto4b(list_copy.curbytes, data->transfer_count);
	} else {
		data->transfer_count_units = RCS_TC_MBYTES;
		scsi_ulto4b(list_copy.curbytes >> 20, data->transfer_count);
	}
	scsi_ulto2b(list_copy.curseg, data->segments_processed);
	data->sense_data_length = list_copy.sense_len;
	memcpy(data->sense_data, &list_copy.sense_data, list_copy.sense_len);

	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;

	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_copy_operation_abort(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_copy_operation_abort *cdb;
	struct tpc_list *list;
	int retval;
	uint32_t list_id;

	CTL_DEBUG_PRINT(("ctl_copy_operation_abort\n"));

	cdb = (struct scsi_copy_operation_abort *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	list_id = scsi_4btoul(cdb->list_identifier);
	mtx_lock(&lun->lun_lock);
	TAILQ_FOREACH(list, &lun->tpc_lists, links) {
		if ((list->flags & EC_LIST_ID_USAGE_MASK) !=
		     EC_LIST_ID_USAGE_NONE && list->list_id == list_id)
			break;
	}
	if (list == NULL) {
		mtx_unlock(&lun->lun_lock);
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
		    /*command*/ 1, /*field*/ 2, /*bit_valid*/ 0,
		    /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}
	list->abort = 1;
	mtx_unlock(&lun->lun_lock);

	ctl_set_success(ctsio);
	ctl_done((union ctl_io *)ctsio);
	return (retval);
}

static uint64_t
tpc_resolve(struct tpc_list *list, uint16_t idx, uint32_t *ss)
{

	if (idx == 0xffff) {
		if (ss && list->lun->be_lun)
			*ss = list->lun->be_lun->blocksize;
		return (list->lun->lun);
	}
	if (idx >= list->ncscd)
		return (UINT64_MAX);
	return (tpcl_resolve(list->init_port, &list->cscd[idx], ss));
}

static int
tpc_process_b2b(struct tpc_list *list)
{
	struct scsi_ec_segment_b2b *seg;
	struct scsi_ec_cscd_dtsp *sdstp, *ddstp;
	struct tpc_io *tior, *tiow;
	struct runl run, *prun;
	uint64_t sl, dl;
	off_t srclba, dstlba, numbytes, donebytes, roundbytes;
	int numlba;
	uint32_t srcblock, dstblock;

	if (list->stage == 1) {
complete:
		while ((tior = TAILQ_FIRST(&list->allio)) != NULL) {
			TAILQ_REMOVE(&list->allio, tior, links);
			ctl_free_io(tior->io);
			free(tior, M_CTL);
		}
		free(list->buf, M_CTL);
		if (list->abort) {
			ctl_set_task_aborted(list->ctsio);
			return (CTL_RETVAL_ERROR);
		} else if (list->error) {
			ctl_set_sense(list->ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_COPY_ABORTED,
			    /*asc*/ 0x0d, /*ascq*/ 0x01, SSD_ELEM_NONE);
			return (CTL_RETVAL_ERROR);
		} else {
			list->curbytes += list->segbytes;
			return (CTL_RETVAL_COMPLETE);
		}
	}

	TAILQ_INIT(&list->allio);
	seg = (struct scsi_ec_segment_b2b *)list->seg[list->curseg];
	sl = tpc_resolve(list, scsi_2btoul(seg->src_cscd), &srcblock);
	dl = tpc_resolve(list, scsi_2btoul(seg->dst_cscd), &dstblock);
	if (sl >= CTL_MAX_LUNS || dl >= CTL_MAX_LUNS) {
		ctl_set_sense(list->ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_COPY_ABORTED,
		    /*asc*/ 0x08, /*ascq*/ 0x04, SSD_ELEM_NONE);
		return (CTL_RETVAL_ERROR);
	}
	sdstp = &list->cscd[scsi_2btoul(seg->src_cscd)].dtsp;
	if (scsi_3btoul(sdstp->block_length) != 0)
		srcblock = scsi_3btoul(sdstp->block_length);
	ddstp = &list->cscd[scsi_2btoul(seg->dst_cscd)].dtsp;
	if (scsi_3btoul(ddstp->block_length) != 0)
		dstblock = scsi_3btoul(ddstp->block_length);
	numlba = scsi_2btoul(seg->number_of_blocks);
	if (seg->flags & EC_SEG_DC)
		numbytes = (off_t)numlba * dstblock;
	else
		numbytes = (off_t)numlba * srcblock;
	srclba = scsi_8btou64(seg->src_lba);
	dstlba = scsi_8btou64(seg->dst_lba);

//	printf("Copy %ju bytes from %ju @ %ju to %ju @ %ju\n",
//	    (uintmax_t)numbytes, sl, scsi_8btou64(seg->src_lba),
//	    dl, scsi_8btou64(seg->dst_lba));

	if (numbytes == 0)
		return (CTL_RETVAL_COMPLETE);

	if (numbytes % srcblock != 0 || numbytes % dstblock != 0) {
		ctl_set_sense(list->ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_COPY_ABORTED,
		    /*asc*/ 0x26, /*ascq*/ 0x0A, SSD_ELEM_NONE);
		return (CTL_RETVAL_ERROR);
	}

	list->buf = malloc(numbytes, M_CTL, M_WAITOK);
	list->segbytes = numbytes;
	donebytes = 0;
	TAILQ_INIT(&run);
	prun = &run;
	list->tbdio = 1;
	while (donebytes < numbytes) {
		roundbytes = MIN(numbytes - donebytes, TPC_MAX_IO_SIZE);

		tior = malloc(sizeof(*tior), M_CTL, M_WAITOK | M_ZERO);
		TAILQ_INIT(&tior->run);
		tior->list = list;
		TAILQ_INSERT_TAIL(&list->allio, tior, links);
		tior->io = tpcl_alloc_io();
		if (tior->io == NULL) {
			list->error = 1;
			goto complete;
		}
		ctl_scsi_read_write(tior->io,
				    /*data_ptr*/ &list->buf[donebytes],
				    /*data_len*/ roundbytes,
				    /*read_op*/ 1,
				    /*byte2*/ 0,
				    /*minimum_cdb_size*/ 0,
				    /*lba*/ srclba + donebytes / srcblock,
				    /*num_blocks*/ roundbytes / srcblock,
				    /*tag_type*/ CTL_TAG_SIMPLE,
				    /*control*/ 0);
		tior->io->io_hdr.retries = 3;
		tior->lun = sl;
		tior->io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = tior;

		tiow = malloc(sizeof(*tior), M_CTL, M_WAITOK | M_ZERO);
		TAILQ_INIT(&tiow->run);
		tiow->list = list;
		TAILQ_INSERT_TAIL(&list->allio, tiow, links);
		tiow->io = tpcl_alloc_io();
		if (tiow->io == NULL) {
			list->error = 1;
			goto complete;
		}
		ctl_scsi_read_write(tiow->io,
				    /*data_ptr*/ &list->buf[donebytes],
				    /*data_len*/ roundbytes,
				    /*read_op*/ 0,
				    /*byte2*/ 0,
				    /*minimum_cdb_size*/ 0,
				    /*lba*/ dstlba + donebytes / dstblock,
				    /*num_blocks*/ roundbytes / dstblock,
				    /*tag_type*/ CTL_TAG_SIMPLE,
				    /*control*/ 0);
		tiow->io->io_hdr.retries = 3;
		tiow->lun = dl;
		tiow->io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = tior;

		TAILQ_INSERT_TAIL(&tior->run, tiow, rlinks);
		TAILQ_INSERT_TAIL(prun, tior, rlinks);
		prun = &tior->run;
		donebytes += roundbytes;
	}

	while ((tior = TAILQ_FIRST(&run)) != NULL) {
		TAILQ_REMOVE(&run, tior, rlinks);
		if (tpcl_queue(tior->io, tior->lun) != CTL_RETVAL_COMPLETE)
			panic("tpcl_queue() error");
	}

	list->stage++;
	return (CTL_RETVAL_QUEUED);
}

static int
tpc_process_verify(struct tpc_list *list)
{
	struct scsi_ec_segment_verify *seg;
	struct tpc_io *tio;
	uint64_t sl;

	if (list->stage == 1) {
complete:
		while ((tio = TAILQ_FIRST(&list->allio)) != NULL) {
			TAILQ_REMOVE(&list->allio, tio, links);
			ctl_free_io(tio->io);
			free(tio, M_CTL);
		}
		if (list->abort) {
			ctl_set_task_aborted(list->ctsio);
			return (CTL_RETVAL_ERROR);
		} else if (list->error) {
			ctl_set_sense(list->ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_COPY_ABORTED,
			    /*asc*/ 0x0d, /*ascq*/ 0x01, SSD_ELEM_NONE);
			return (CTL_RETVAL_ERROR);
		} else
			return (CTL_RETVAL_COMPLETE);
	}

	TAILQ_INIT(&list->allio);
	seg = (struct scsi_ec_segment_verify *)list->seg[list->curseg];
	sl = tpc_resolve(list, scsi_2btoul(seg->src_cscd), NULL);
	if (sl >= CTL_MAX_LUNS) {
		ctl_set_sense(list->ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_COPY_ABORTED,
		    /*asc*/ 0x08, /*ascq*/ 0x04, SSD_ELEM_NONE);
		return (CTL_RETVAL_ERROR);
	}

//	printf("Verify %ju\n", sl);

	if ((seg->tur & 0x01) == 0)
		return (CTL_RETVAL_COMPLETE);

	list->tbdio = 1;
	tio = malloc(sizeof(*tio), M_CTL, M_WAITOK | M_ZERO);
	TAILQ_INIT(&tio->run);
	tio->list = list;
	TAILQ_INSERT_TAIL(&list->allio, tio, links);
	tio->io = tpcl_alloc_io();
	if (tio->io == NULL) {
		list->error = 1;
		goto complete;
	}
	ctl_scsi_tur(tio->io, /*tag_type*/ CTL_TAG_SIMPLE, /*control*/ 0);
	tio->io->io_hdr.retries = 3;
	tio->lun = sl;
	tio->io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = tio;
	list->stage++;
	if (tpcl_queue(tio->io, tio->lun) != CTL_RETVAL_COMPLETE)
		panic("tpcl_queue() error");
	return (CTL_RETVAL_QUEUED);
}

static int
tpc_process_register_key(struct tpc_list *list)
{
	struct scsi_ec_segment_register_key *seg;
	struct tpc_io *tio;
	uint64_t dl;
	int datalen;

	if (list->stage == 1) {
complete:
		while ((tio = TAILQ_FIRST(&list->allio)) != NULL) {
			TAILQ_REMOVE(&list->allio, tio, links);
			ctl_free_io(tio->io);
			free(tio, M_CTL);
		}
		free(list->buf, M_CTL);
		if (list->abort) {
			ctl_set_task_aborted(list->ctsio);
			return (CTL_RETVAL_ERROR);
		} else if (list->error) {
			ctl_set_sense(list->ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_COPY_ABORTED,
			    /*asc*/ 0x0d, /*ascq*/ 0x01, SSD_ELEM_NONE);
			return (CTL_RETVAL_ERROR);
		} else
			return (CTL_RETVAL_COMPLETE);
	}

	TAILQ_INIT(&list->allio);
	seg = (struct scsi_ec_segment_register_key *)list->seg[list->curseg];
	dl = tpc_resolve(list, scsi_2btoul(seg->dst_cscd), NULL);
	if (dl >= CTL_MAX_LUNS) {
		ctl_set_sense(list->ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_COPY_ABORTED,
		    /*asc*/ 0x08, /*ascq*/ 0x04, SSD_ELEM_NONE);
		return (CTL_RETVAL_ERROR);
	}

//	printf("Register Key %ju\n", dl);

	list->tbdio = 1;
	tio = malloc(sizeof(*tio), M_CTL, M_WAITOK | M_ZERO);
	TAILQ_INIT(&tio->run);
	tio->list = list;
	TAILQ_INSERT_TAIL(&list->allio, tio, links);
	tio->io = tpcl_alloc_io();
	if (tio->io == NULL) {
		list->error = 1;
		goto complete;
	}
	datalen = sizeof(struct scsi_per_res_out_parms);
	list->buf = malloc(datalen, M_CTL, M_WAITOK);
	ctl_scsi_persistent_res_out(tio->io,
	    list->buf, datalen, SPRO_REGISTER, -1,
	    scsi_8btou64(seg->res_key), scsi_8btou64(seg->sa_res_key),
	    /*tag_type*/ CTL_TAG_SIMPLE, /*control*/ 0);
	tio->io->io_hdr.retries = 3;
	tio->lun = dl;
	tio->io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = tio;
	list->stage++;
	if (tpcl_queue(tio->io, tio->lun) != CTL_RETVAL_COMPLETE)
		panic("tpcl_queue() error");
	return (CTL_RETVAL_QUEUED);
}

static void
tpc_process(struct tpc_list *list)
{
	struct ctl_lun *lun = list->lun;
	struct scsi_ec_segment *seg;
	struct ctl_scsiio *ctsio = list->ctsio;
	int retval = CTL_RETVAL_COMPLETE;

//printf("ZZZ %d cscd, %d segs\n", list->ncscd, list->nseg);
	while (list->curseg < list->nseg) {
		seg = list->seg[list->curseg];
		switch (seg->type_code) {
		case EC_SEG_B2B:
			retval = tpc_process_b2b(list);
			break;
		case EC_SEG_VERIFY:
			retval = tpc_process_verify(list);
			break;
		case EC_SEG_REGISTER_KEY:
			retval = tpc_process_register_key(list);
			break;
		default:
			ctl_set_sense(ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_COPY_ABORTED,
			    /*asc*/ 0x26, /*ascq*/ 0x09, SSD_ELEM_NONE);
			goto done;
		}
		if (retval == CTL_RETVAL_QUEUED)
			return;
		if (retval == CTL_RETVAL_ERROR) {
			list->error = 1;
			goto done;
		}
		list->curseg++;
		list->stage = 0;
	}

	ctl_set_success(ctsio);

done:
//printf("ZZZ done\n");
	mtx_lock(&lun->lun_lock);
	if ((list->flags & EC_LIST_ID_USAGE_MASK) == EC_LIST_ID_USAGE_NONE) {
		TAILQ_REMOVE(&lun->tpc_lists, list, links);
		free(list, M_CTL);
	} else {
		list->completed = 1;
		list->sense_data = ctsio->sense_data;
		list->sense_len = ctsio->sense_len;
		list->scsi_status = ctsio->scsi_status;
	}
	mtx_unlock(&lun->lun_lock);

	ctl_done((union ctl_io *)ctsio);
}

/*
 * For any sort of check condition, busy, etc., we just retry.  We do not
 * decrement the retry count for unit attention type errors.  These are
 * normal, and we want to save the retry count for "real" errors.  Otherwise,
 * we could end up with situations where a command will succeed in some
 * situations and fail in others, depending on whether a unit attention is
 * pending.  Also, some of our error recovery actions, most notably the
 * LUN reset action, will cause a unit attention.
 *
 * We can add more detail here later if necessary.
 */
static tpc_error_action
tpc_checkcond_parse(union ctl_io *io)
{
	tpc_error_action error_action;
	int error_code, sense_key, asc, ascq;

	/*
	 * Default to retrying the command.
	 */
	error_action = TPC_ERR_RETRY;

	scsi_extract_sense_len(&io->scsiio.sense_data,
			       io->scsiio.sense_len,
			       &error_code,
			       &sense_key,
			       &asc,
			       &ascq,
			       /*show_errors*/ 1);

	switch (error_code) {
	case SSD_DEFERRED_ERROR:
	case SSD_DESC_DEFERRED_ERROR:
		error_action |= TPC_ERR_NO_DECREMENT;
		break;
	case SSD_CURRENT_ERROR:
	case SSD_DESC_CURRENT_ERROR:
	default:
		switch (sense_key) {
		case SSD_KEY_UNIT_ATTENTION:
			error_action |= TPC_ERR_NO_DECREMENT;
			break;
		case SSD_KEY_HARDWARE_ERROR:
			/*
			 * This is our generic "something bad happened"
			 * error code.  It often isn't recoverable.
			 */
			if ((asc == 0x44) && (ascq == 0x00))
				error_action = TPC_ERR_FAIL;
			break;
		case SSD_KEY_NOT_READY:
			/*
			 * If the LUN is powered down, there likely isn't
			 * much point in retrying right now.
			 */
			if ((asc == 0x04) && (ascq == 0x02))
				error_action = TPC_ERR_FAIL;
			/*
			 * If the LUN is offline, there probably isn't much
			 * point in retrying, either.
			 */
			if ((asc == 0x04) && (ascq == 0x03))
				error_action = TPC_ERR_FAIL;
			break;
		}
	}
	return (error_action);
}

static tpc_error_action
tpc_error_parse(union ctl_io *io)
{
	tpc_error_action error_action = TPC_ERR_RETRY;

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		switch (io->io_hdr.status & CTL_STATUS_MASK) {
		case CTL_SCSI_ERROR:
			switch (io->scsiio.scsi_status) {
			case SCSI_STATUS_CHECK_COND:
				error_action = tpc_checkcond_parse(io);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		break;
	case CTL_IO_TASK:
		break;
	default:
		panic("%s: invalid ctl_io type %d\n", __func__,
		      io->io_hdr.io_type);
		break;
	}
	return (error_action);
}

void
tpc_done(union ctl_io *io)
{
	struct tpc_io *tio, *tior;

	/*
	 * Very minimal retry logic.  We basically retry if we got an error
	 * back, and the retry count is greater than 0.  If we ever want
	 * more sophisticated initiator type behavior, the CAM error
	 * recovery code in ../common might be helpful.
	 */
//	if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
//		ctl_io_error_print(io, NULL);
	tio = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	if (((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
	 && (io->io_hdr.retries > 0)) {
		ctl_io_status old_status;
		tpc_error_action error_action;

		error_action = tpc_error_parse(io);
		switch (error_action & TPC_ERR_MASK) {
		case TPC_ERR_FAIL:
			break;
		case TPC_ERR_RETRY:
		default:
			if ((error_action & TPC_ERR_NO_DECREMENT) == 0)
				io->io_hdr.retries--;
			old_status = io->io_hdr.status;
			io->io_hdr.status = CTL_STATUS_NONE;
			io->io_hdr.flags &= ~CTL_FLAG_ABORT;
			io->io_hdr.flags &= ~CTL_FLAG_SENT_2OTHER_SC;
			if (tpcl_queue(io, tio->lun) != CTL_RETVAL_COMPLETE) {
				printf("%s: error returned from ctl_queue()!\n",
				       __func__);
				io->io_hdr.status = old_status;
			} else
				return;
		}
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
		tio->list->error = 1;
	else
		atomic_add_int(&tio->list->curops, 1);
	if (!tio->list->error && !tio->list->abort) {
		while ((tior = TAILQ_FIRST(&tio->run)) != NULL) {
			TAILQ_REMOVE(&tio->run, tior, rlinks);
			atomic_add_int(&tio->list->tbdio, 1);
			if (tpcl_queue(tior->io, tior->lun) != CTL_RETVAL_COMPLETE)
				panic("tpcl_queue() error");
		}
	}
	if (atomic_fetchadd_int(&tio->list->tbdio, -1) == 1)
		tpc_process(tio->list);
}

int
ctl_extended_copy_lid1(struct ctl_scsiio *ctsio)
{
	struct scsi_extended_copy *cdb;
	struct scsi_extended_copy_lid1_data *data;
	struct ctl_lun *lun;
	struct tpc_list *list, *tlist;
	uint8_t *ptr;
	char *value;
	int len, off, lencscd, lenseg, leninl, nseg;

	CTL_DEBUG_PRINT(("ctl_extended_copy_lid1\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_extended_copy *)ctsio->cdb;
	len = scsi_4btoul(cdb->length);

	if (len < sizeof(struct scsi_extended_copy_lid1_data) ||
	    len > sizeof(struct scsi_extended_copy_lid1_data) +
	    TPC_MAX_LIST + TPC_MAX_INLINE) {
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1, /*command*/ 1,
		    /*field*/ 9, /*bit_valid*/ 0, /*bit*/ 0);
		goto done;
	}

	/*
	 * If we've got a kernel request that hasn't been malloced yet,
	 * malloc it and tell the caller the data buffer is here.
	 */
	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(len, M_CTL, M_WAITOK);
		ctsio->kern_data_len = len;
		ctsio->kern_total_len = len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	data = (struct scsi_extended_copy_lid1_data *)ctsio->kern_data_ptr;
	lencscd = scsi_2btoul(data->cscd_list_length);
	lenseg = scsi_4btoul(data->segment_list_length);
	leninl = scsi_4btoul(data->inline_data_length);
	if (len < sizeof(struct scsi_extended_copy_lid1_data) +
	    lencscd + lenseg + leninl ||
	    leninl > TPC_MAX_INLINE) {
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1, /*command*/ 0,
		    /*field*/ 2, /*bit_valid*/ 0, /*bit*/ 0);
		goto done;
	}
	if (lencscd > TPC_MAX_CSCDS * sizeof(struct scsi_ec_cscd)) {
		ctl_set_sense(ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
		    /*asc*/ 0x26, /*ascq*/ 0x06, SSD_ELEM_NONE);
		goto done;
	}
	if (lencscd + lenseg > TPC_MAX_LIST) {
		ctl_set_param_len_error(ctsio);
		goto done;
	}

	list = malloc(sizeof(struct tpc_list), M_CTL, M_WAITOK | M_ZERO);
	list->service_action = cdb->service_action;
	value = ctl_get_opt(&lun->be_lun->options, "insecure_tpc");
	if (value != NULL && strcmp(value, "on") == 0)
		list->init_port = -1;
	else
		list->init_port = ctsio->io_hdr.nexus.targ_port;
	list->init_idx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	list->list_id = data->list_identifier;
	list->flags = data->flags;
	list->params = ctsio->kern_data_ptr;
	list->cscd = (struct scsi_ec_cscd *)&data->data[0];
	ptr = &data->data[lencscd];
	for (nseg = 0, off = 0; off < lenseg; nseg++) {
		if (nseg >= TPC_MAX_SEGS) {
			free(list, M_CTL);
			ctl_set_sense(ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
			    /*asc*/ 0x26, /*ascq*/ 0x08, SSD_ELEM_NONE);
			goto done;
		}
		list->seg[nseg] = (struct scsi_ec_segment *)(ptr + off);
		off += sizeof(struct scsi_ec_segment) +
		    scsi_2btoul(list->seg[nseg]->descr_length);
	}
	list->inl = &data->data[lencscd + lenseg];
	list->ncscd = lencscd / sizeof(struct scsi_ec_cscd);
	list->nseg = nseg;
	list->leninl = leninl;
	list->ctsio = ctsio;
	list->lun = lun;
	mtx_lock(&lun->lun_lock);
	if ((list->flags & EC_LIST_ID_USAGE_MASK) != EC_LIST_ID_USAGE_NONE) {
		TAILQ_FOREACH(tlist, &lun->tpc_lists, links) {
			if ((tlist->flags & EC_LIST_ID_USAGE_MASK) !=
			     EC_LIST_ID_USAGE_NONE &&
			    tlist->list_id == list->list_id)
				break;
		}
		if (tlist != NULL && !tlist->completed) {
			mtx_unlock(&lun->lun_lock);
			free(list, M_CTL);
			ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
			    /*command*/ 0, /*field*/ 0, /*bit_valid*/ 0,
			    /*bit*/ 0);
			goto done;
		}
		if (tlist != NULL) {
			TAILQ_REMOVE(&lun->tpc_lists, tlist, links);
			free(tlist, M_CTL);
		}
	}
	TAILQ_INSERT_TAIL(&lun->tpc_lists, list, links);
	mtx_unlock(&lun->lun_lock);

	tpc_process(list);
	return (CTL_RETVAL_COMPLETE);

done:
	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_extended_copy_lid4(struct ctl_scsiio *ctsio)
{
	struct scsi_extended_copy *cdb;
	struct scsi_extended_copy_lid4_data *data;
	struct ctl_lun *lun;
	struct tpc_list *list, *tlist;
	uint8_t *ptr;
	char *value;
	int len, off, lencscd, lenseg, leninl, nseg;

	CTL_DEBUG_PRINT(("ctl_extended_copy_lid4\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_extended_copy *)ctsio->cdb;
	len = scsi_4btoul(cdb->length);

	if (len < sizeof(struct scsi_extended_copy_lid4_data) ||
	    len > sizeof(struct scsi_extended_copy_lid4_data) +
	    TPC_MAX_LIST + TPC_MAX_INLINE) {
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1, /*command*/ 1,
		    /*field*/ 9, /*bit_valid*/ 0, /*bit*/ 0);
		goto done;
	}

	/*
	 * If we've got a kernel request that hasn't been malloced yet,
	 * malloc it and tell the caller the data buffer is here.
	 */
	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(len, M_CTL, M_WAITOK);
		ctsio->kern_data_len = len;
		ctsio->kern_total_len = len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	data = (struct scsi_extended_copy_lid4_data *)ctsio->kern_data_ptr;
	lencscd = scsi_2btoul(data->cscd_list_length);
	lenseg = scsi_2btoul(data->segment_list_length);
	leninl = scsi_2btoul(data->inline_data_length);
	if (len < sizeof(struct scsi_extended_copy_lid4_data) +
	    lencscd + lenseg + leninl ||
	    leninl > TPC_MAX_INLINE) {
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1, /*command*/ 0,
		    /*field*/ 2, /*bit_valid*/ 0, /*bit*/ 0);
		goto done;
	}
	if (lencscd > TPC_MAX_CSCDS * sizeof(struct scsi_ec_cscd)) {
		ctl_set_sense(ctsio, /*current_error*/ 1,
		    /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
		    /*asc*/ 0x26, /*ascq*/ 0x06, SSD_ELEM_NONE);
		goto done;
	}
	if (lencscd + lenseg > TPC_MAX_LIST) {
		ctl_set_param_len_error(ctsio);
		goto done;
	}

	list = malloc(sizeof(struct tpc_list), M_CTL, M_WAITOK | M_ZERO);
	list->service_action = cdb->service_action;
	value = ctl_get_opt(&lun->be_lun->options, "insecure_tpc");
	if (value != NULL && strcmp(value, "on") == 0)
		list->init_port = -1;
	else
		list->init_port = ctsio->io_hdr.nexus.targ_port;
	list->init_idx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	list->list_id = scsi_4btoul(data->list_identifier);
	list->flags = data->flags;
	list->params = ctsio->kern_data_ptr;
	list->cscd = (struct scsi_ec_cscd *)&data->data[0];
	ptr = &data->data[lencscd];
	for (nseg = 0, off = 0; off < lenseg; nseg++) {
		if (nseg >= TPC_MAX_SEGS) {
			free(list, M_CTL);
			ctl_set_sense(ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
			    /*asc*/ 0x26, /*ascq*/ 0x08, SSD_ELEM_NONE);
			goto done;
		}
		list->seg[nseg] = (struct scsi_ec_segment *)(ptr + off);
		off += sizeof(struct scsi_ec_segment) +
		    scsi_2btoul(list->seg[nseg]->descr_length);
	}
	list->inl = &data->data[lencscd + lenseg];
	list->ncscd = lencscd / sizeof(struct scsi_ec_cscd);
	list->nseg = nseg;
	list->leninl = leninl;
	list->ctsio = ctsio;
	list->lun = lun;
	mtx_lock(&lun->lun_lock);
	if ((list->flags & EC_LIST_ID_USAGE_MASK) != EC_LIST_ID_USAGE_NONE) {
		TAILQ_FOREACH(tlist, &lun->tpc_lists, links) {
			if ((tlist->flags & EC_LIST_ID_USAGE_MASK) !=
			     EC_LIST_ID_USAGE_NONE &&
			    tlist->list_id == list->list_id)
				break;
		}
		if (tlist != NULL && !tlist->completed) {
			mtx_unlock(&lun->lun_lock);
			free(list, M_CTL);
			ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
			    /*command*/ 0, /*field*/ 0, /*bit_valid*/ 0,
			    /*bit*/ 0);
			goto done;
		}
		if (tlist != NULL) {
			TAILQ_REMOVE(&lun->tpc_lists, tlist, links);
			free(tlist, M_CTL);
		}
	}
	TAILQ_INSERT_TAIL(&lun->tpc_lists, list, links);
	mtx_unlock(&lun->lun_lock);

	tpc_process(list);
	return (CTL_RETVAL_COMPLETE);

done:
	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

