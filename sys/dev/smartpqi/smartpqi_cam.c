/*-
 * Copyright 2016-2021 Microchip Technology, Inc. and/or its subsidiaries.
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

/* $FreeBSD$ */
/*
 * CAM interface for smartpqi driver
 */

#include "smartpqi_includes.h"

/*
 * Set cam sim properties of the smartpqi adapter.
 */
static void
update_sim_properties(struct cam_sim *sim, struct ccb_pathinq *cpi)
{

	pqisrc_softstate_t *softs = (struct pqisrc_softstate *)
					cam_sim_softc(sim);

	device_t dev = softs->os_specific.pqi_dev;

	DBG_FUNC("IN\n");

	cpi->version_num = 1;
	cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
	cpi->target_sprt = 0;
	cpi->hba_misc = PIM_NOBUSRESET | PIM_UNMAPPED;
	cpi->hba_eng_cnt = 0;
	cpi->max_lun = PQI_MAX_MULTILUN;
	cpi->max_target = 1088;
	cpi->maxio = (softs->pqi_cap.max_sg_elem - 1) * PAGE_SIZE;
	cpi->initiator_id = 255;
	strlcpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
	strlcpy(cpi->hba_vid, "Microsemi", HBA_IDLEN);
	strlcpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
	cpi->unit_number = cam_sim_unit(sim);
	cpi->bus_id = cam_sim_bus(sim);
	cpi->base_transfer_speed = 1200000; /* Base bus speed in KB/sec */
	cpi->protocol = PROTO_SCSI;
	cpi->protocol_version = SCSI_REV_SPC4;
	cpi->transport = XPORT_SPI;
	cpi->transport_version = 2;
	cpi->ccb_h.status = CAM_REQ_CMP;
	cpi->hba_vendor = pci_get_vendor(dev);
	cpi->hba_device = pci_get_device(dev);
	cpi->hba_subvendor = pci_get_subvendor(dev);
	cpi->hba_subdevice = pci_get_subdevice(dev);


	DBG_FUNC("OUT\n");
}

/*
 * Get transport settings of the smartpqi adapter 
 */
static void
get_transport_settings(struct pqisrc_softstate *softs,
		struct ccb_trans_settings *cts)
{
	struct ccb_trans_settings_scsi	*scsi = &cts->proto_specific.scsi;
	struct ccb_trans_settings_sas	*sas = &cts->xport_specific.sas;
	struct ccb_trans_settings_spi	*spi = &cts->xport_specific.spi;

	DBG_FUNC("IN\n");
	
	cts->protocol = PROTO_SCSI;
	cts->protocol_version = SCSI_REV_SPC4;
	cts->transport = XPORT_SPI;
	cts->transport_version = 2;
	spi->valid = CTS_SPI_VALID_DISC;
	spi->flags = CTS_SPI_FLAGS_DISC_ENB;
	scsi->valid = CTS_SCSI_VALID_TQ;
	scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
	sas->valid = CTS_SAS_VALID_SPEED;
	cts->ccb_h.status = CAM_REQ_CMP;

	DBG_FUNC("OUT\n");
}

/*
 *  Add the target to CAM layer and rescan, when a new device is found
 */
void
os_add_device(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	union ccb *ccb;

	DBG_FUNC("IN\n");

	if(softs->os_specific.sim_registered) {	
		if ((ccb = xpt_alloc_ccb_nowait()) == NULL) {
			DBG_ERR("rescan failed (can't allocate CCB)\n");
			return;
		}

		if (xpt_create_path(&ccb->ccb_h.path, NULL,
			cam_sim_path(softs->os_specific.sim),
			device->target, device->lun) != CAM_REQ_CMP) {
			DBG_ERR("rescan failed (can't create path)\n");
			xpt_free_ccb(ccb);
			return;
		}
		xpt_rescan(ccb);
	}

	DBG_FUNC("OUT\n");
}

/*
 * Remove the device from CAM layer when deleted or hot removed
 */
void
os_remove_device(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	struct cam_path *tmppath;

	DBG_FUNC("IN\n");
	
	if(softs->os_specific.sim_registered) {
		if (xpt_create_path(&tmppath, NULL, 
			cam_sim_path(softs->os_specific.sim),
			device->target, device->lun) != CAM_REQ_CMP) {
			DBG_ERR("unable to create path for async event");
			return;
		}
		xpt_async(AC_LOST_DEVICE, tmppath, NULL);
		xpt_free_path(tmppath);
		softs->device_list[device->target][device->lun] = NULL;
		pqisrc_free_device(softs, device);
	}

	DBG_FUNC("OUT\n");

}

/*
 * Function to release the frozen simq
 */
static void
pqi_release_camq(rcb_t *rcb)
{
	pqisrc_softstate_t *softs;
	struct ccb_scsiio *csio;

	csio = (struct ccb_scsiio *)&rcb->cm_ccb->csio;
	softs = rcb->softs;

	DBG_FUNC("IN\n");

	if (softs->os_specific.pqi_flags & PQI_FLAG_BUSY) {
		softs->os_specific.pqi_flags &= ~PQI_FLAG_BUSY;
		if (csio->ccb_h.status & CAM_RELEASE_SIMQ)
			xpt_release_simq(xpt_path_sim(csio->ccb_h.path), 0);
		else
			csio->ccb_h.status |= CAM_RELEASE_SIMQ;
	}

	DBG_FUNC("OUT\n");
}

static void
pqi_synch_request(rcb_t *rcb)
{
	pqisrc_softstate_t *softs = rcb->softs;

	DBG_IO("IN rcb = %p\n", rcb);

	if (!(rcb->cm_flags & PQI_CMD_MAPPED))
		return;

	if (rcb->bcount != 0 ) {
		if (rcb->data_dir == SOP_DATA_DIR_FROM_DEVICE)
			bus_dmamap_sync(softs->os_specific.pqi_buffer_dmat,
					rcb->cm_datamap,
					BUS_DMASYNC_POSTREAD);
		if (rcb->data_dir == SOP_DATA_DIR_TO_DEVICE)
			bus_dmamap_sync(softs->os_specific.pqi_buffer_dmat,
					rcb->cm_datamap,
					BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(softs->os_specific.pqi_buffer_dmat,
					rcb->cm_datamap);
	}
	rcb->cm_flags &= ~PQI_CMD_MAPPED;

	if(rcb->sgt && rcb->nseg)
		os_mem_free(rcb->softs, (void*)rcb->sgt,
				rcb->nseg*sizeof(sgt_t));

	DBG_IO("OUT\n");
}

/*
 * Function to dma-unmap the completed request
 */
static inline void
pqi_unmap_request(rcb_t *rcb)
{
	DBG_IO("IN rcb = %p\n", rcb);

	pqi_synch_request(rcb);
	pqisrc_put_tag(&rcb->softs->taglist, rcb->tag);

	DBG_IO("OUT\n");
}

/*
 * Construct meaningful LD name for volume here.
 */
static void
smartpqi_fix_ld_inquiry(pqisrc_softstate_t *softs, struct ccb_scsiio *csio)
{
	struct scsi_inquiry_data *inq = NULL;
	uint8_t *cdb = NULL;
	pqi_scsi_dev_t *device = NULL;

	DBG_FUNC("IN\n");

	if (pqisrc_ctrl_offline(softs))
		return;

 	cdb = (csio->ccb_h.flags & CAM_CDB_POINTER) ?
		(uint8_t *)csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes;
	if(cdb[0] == INQUIRY &&
		(cdb[1] & SI_EVPD) == 0 &&
		(csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN &&
		csio->dxfer_len >= SHORT_INQUIRY_LENGTH) {

		inq = (struct scsi_inquiry_data *)csio->data_ptr;

		device = softs->device_list[csio->ccb_h.target_id][csio->ccb_h.target_lun];

		/* Let the disks be probed and dealt with via CAM. Only for LD
		  let it fall through and inquiry be tweaked */
		if (!device || !pqisrc_is_logical_device(device) ||
				(device->devtype != DISK_DEVICE) ||
				pqisrc_is_external_raid_device(device)) {
 	 		return;
		}

		strncpy(inq->vendor, device->vendor,
				SID_VENDOR_SIZE);
		strncpy(inq->product,
				pqisrc_raidlevel_to_string(device->raid_level),
				SID_PRODUCT_SIZE);
		strncpy(inq->revision, device->volume_offline?"OFF":"OK",
				SID_REVISION_SIZE);
    	}

	DBG_FUNC("OUT\n");
}

static void
pqi_complete_scsi_io(struct ccb_scsiio *csio, rcb_t *rcb)
{
	uint32_t release_tag;
	pqisrc_softstate_t *softs = rcb->softs;

	DBG_IO("IN scsi io = %p\n", csio);

	pqi_synch_request(rcb);
	smartpqi_fix_ld_inquiry(rcb->softs, csio);
	pqi_release_camq(rcb);
	release_tag = rcb->tag;
	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, release_tag);
	xpt_done((union ccb *)csio);

	DBG_FUNC("OUT\n");
}

/*
 * Handle completion of a command - pass results back through the CCB
 */
void
os_io_response_success(rcb_t *rcb)
{
	struct ccb_scsiio *csio;

	DBG_IO("IN rcb = %p\n", rcb);

	if (rcb == NULL)
		panic("rcb is null");

	csio = (struct ccb_scsiio *)&rcb->cm_ccb->csio;

	if (csio == NULL)
		panic("csio is null");

	rcb->status = REQUEST_SUCCESS;
	csio->ccb_h.status = CAM_REQ_CMP;

	pqi_complete_scsi_io(csio, rcb);

	DBG_IO("OUT\n");
}

static void
copy_sense_data_to_csio(struct ccb_scsiio *csio,
		uint8_t *sense_data, uint16_t sense_data_len)
{
	DBG_IO("IN csio = %p\n", csio);

	memset(&csio->sense_data, 0, csio->sense_len);

	sense_data_len = (sense_data_len > csio->sense_len) ?
		csio->sense_len : sense_data_len;

	if (sense_data)
		memcpy(&csio->sense_data, sense_data, sense_data_len);

	if (csio->sense_len > sense_data_len)
		csio->sense_resid = csio->sense_len - sense_data_len;
	else
		csio->sense_resid = 0;

	DBG_IO("OUT\n");
}

/*
 * Error response handling for raid IO
 */
void
os_raid_response_error(rcb_t *rcb, raid_path_error_info_elem_t *err_info)
{
	struct ccb_scsiio *csio;
	pqisrc_softstate_t *softs;

	DBG_IO("IN\n");

	csio = (struct ccb_scsiio *)&rcb->cm_ccb->csio;

	if (csio == NULL)
		panic("csio is null");

	softs = rcb->softs;

	csio->ccb_h.status = CAM_REQ_CMP_ERR;

	if (!err_info || !rcb->dvp) {
		DBG_ERR("couldn't be accessed! error info = %p, rcb->dvp = %p\n",
				err_info, rcb->dvp);
		goto error_out;
	}

	csio->scsi_status = err_info->status;

	if (csio->ccb_h.func_code == XPT_SCSI_IO) {
		/*
		 * Handle specific SCSI status values.
		 */
		switch(csio->scsi_status) {
			case PQI_RAID_STATUS_QUEUE_FULL:
				csio->ccb_h.status = CAM_REQ_CMP;
				DBG_ERR("Queue Full error\n");
				break;
				/* check condition, sense data included */
			case PQI_RAID_STATUS_CHECK_CONDITION:
				{
					uint16_t sense_data_len =
						LE_16(err_info->sense_data_len);
					uint8_t *sense_data = NULL;
					if (sense_data_len)
						sense_data = err_info->data;
					copy_sense_data_to_csio(csio, sense_data, sense_data_len);
					csio->ccb_h.status = CAM_SCSI_STATUS_ERROR
							| CAM_AUTOSNS_VALID
							| CAM_REQ_CMP_ERR;

				}
				break;

			case PQI_RAID_DATA_IN_OUT_UNDERFLOW:
				{
					uint32_t resid = 0;
					resid = rcb->bcount-err_info->data_out_transferred;
					csio->resid  = resid;
					csio->ccb_h.status = CAM_REQ_CMP;
				}
				break;
			default:
				csio->ccb_h.status = CAM_REQ_CMP;
				break;
		}
	}

error_out:
	pqi_complete_scsi_io(csio, rcb);

	DBG_IO("OUT\n");
}

/*
 * Error response handling for aio.
 */
void
os_aio_response_error(rcb_t *rcb, aio_path_error_info_elem_t *err_info)
{
	struct ccb_scsiio *csio;
	pqisrc_softstate_t *softs;

	DBG_IO("IN\n");

	if (rcb == NULL)
		panic("rcb is null");

	rcb->status = REQUEST_SUCCESS;
	csio = (struct ccb_scsiio *)&rcb->cm_ccb->csio;
	if (csio == NULL)
                panic("csio is null");

	softs = rcb->softs;

	if (!err_info || !rcb->dvp) {
		csio->ccb_h.status = CAM_REQ_CMP_ERR;
		DBG_ERR("couldn't be accessed! error info = %p, rcb->dvp = %p\n",
				err_info, rcb->dvp);
		goto error_out;
	}

	switch (err_info->service_resp) {
		case PQI_AIO_SERV_RESPONSE_COMPLETE:
			csio->ccb_h.status = err_info->status;
			break;
		case PQI_AIO_SERV_RESPONSE_FAILURE:
			switch(err_info->status) {
				case PQI_AIO_STATUS_IO_ABORTED:
					csio->ccb_h.status = CAM_REQ_ABORTED;
					DBG_WARN_BTL(rcb->dvp, "IO aborted\n");
					break;
				case PQI_AIO_STATUS_UNDERRUN:
					csio->ccb_h.status = CAM_REQ_CMP;
					csio->resid =
						LE_32(err_info->resd_count);
					break;
				case PQI_AIO_STATUS_OVERRUN:
					csio->ccb_h.status = CAM_REQ_CMP;
					break;
				case PQI_AIO_STATUS_AIO_PATH_DISABLED:
					DBG_WARN_BTL(rcb->dvp,"AIO Path Disabled\n");
					/* Timed out TMF response comes here */
					if (rcb->tm_req) {
						rcb->req_pending = false;
						rcb->status = REQUEST_SUCCESS;
						DBG_ERR("AIO Disabled for TMF\n");
						return;
					}
					rcb->dvp->aio_enabled = false;
					rcb->dvp->offload_enabled = false;
					csio->ccb_h.status |= CAM_REQUEUE_REQ;
					break;
				case PQI_AIO_STATUS_IO_ERROR:
				case PQI_AIO_STATUS_IO_NO_DEVICE:
				case PQI_AIO_STATUS_INVALID_DEVICE:
				default:
					DBG_WARN_BTL(rcb->dvp,"IO Error/Invalid/No device\n");
					csio->ccb_h.status |=
						CAM_SCSI_STATUS_ERROR;
					break;
			}
			break;
		case PQI_AIO_SERV_RESPONSE_TMF_COMPLETE:
		case PQI_AIO_SERV_RESPONSE_TMF_SUCCEEDED:
			DBG_ERR("PQI_AIO_SERV_RESPONSE_TMF %s\n",
				(err_info->service_resp == PQI_AIO_SERV_RESPONSE_TMF_COMPLETE) ? "COMPLETE" : "SUCCEEDED");
			rcb->status = REQUEST_SUCCESS;
			rcb->req_pending = false;
			return;
		case PQI_AIO_SERV_RESPONSE_TMF_REJECTED:
		case PQI_AIO_SERV_RESPONSE_TMF_INCORRECT_LUN:
			DBG_ERR("PQI_AIO_SERV_RESPONSE_TMF %s\n",
				(err_info->service_resp == PQI_AIO_SERV_RESPONSE_TMF_REJECTED) ? "REJECTED" : "INCORRECT LUN");
			rcb->status = REQUEST_FAILED;
			rcb->req_pending = false;
			return;
		default:
			DBG_WARN_BTL(rcb->dvp,"Scsi Status Error\n");
			csio->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			break;
	}

	if(err_info->data_pres == DATA_PRESENT_SENSE_DATA ) {
		csio->scsi_status = PQI_AIO_STATUS_CHECK_CONDITION;
		uint8_t *sense_data = NULL;
		unsigned sense_data_len = LE_16(err_info->data_len);
		if (sense_data_len)
			sense_data = err_info->data;
		DBG_INFO("SCSI_STATUS_CHECK_COND  sense size %u\n",
			sense_data_len);
		copy_sense_data_to_csio(csio, sense_data, sense_data_len);
		csio->ccb_h.status = CAM_SCSI_STATUS_ERROR | CAM_AUTOSNS_VALID;
	}

error_out:
	pqi_complete_scsi_io(csio, rcb);
	DBG_IO("OUT\n");
}

static void
pqi_freeze_ccb(union ccb *ccb)
{
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
		xpt_freeze_devq(ccb->ccb_h.path, 1);
	}
}

/*
 * Command-mapping helper function - populate this command's s/g table.
 */
static void
pqi_request_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	rcb_t *rcb = (rcb_t *)arg;
	pqisrc_softstate_t *softs = rcb->softs;
	union ccb *ccb;

	if (error || nseg > softs->pqi_cap.max_sg_elem) {
		DBG_ERR_BTL(rcb->dvp, "map failed err = %d or nseg(%d) > sgelem(%d)\n",
			error, nseg, softs->pqi_cap.max_sg_elem);
		goto error_io;
	}

	rcb->sgt = os_mem_alloc(softs, nseg * sizeof(sgt_t));

	if (!rcb->sgt) {
		DBG_ERR_BTL(rcb->dvp, "os_mem_alloc() failed; nseg = %d\n", nseg);
		goto error_io;
	}

	rcb->nseg = nseg;
	for (int i = 0; i < nseg; i++) {
		rcb->sgt[i].addr = segs[i].ds_addr;
		rcb->sgt[i].len = segs[i].ds_len;
		rcb->sgt[i].flags = 0;
	}

	if (rcb->data_dir == SOP_DATA_DIR_FROM_DEVICE)
		bus_dmamap_sync(softs->os_specific.pqi_buffer_dmat,
			rcb->cm_datamap, BUS_DMASYNC_PREREAD);
	if (rcb->data_dir == SOP_DATA_DIR_TO_DEVICE)
		bus_dmamap_sync(softs->os_specific.pqi_buffer_dmat,
			rcb->cm_datamap, BUS_DMASYNC_PREWRITE);

	/* Call IO functions depending on pd or ld */
	rcb->status = REQUEST_PENDING;

	error = pqisrc_build_send_io(softs, rcb);

	if (error) {
		rcb->req_pending = false;
		DBG_ERR_BTL(rcb->dvp, "Build IO failed, error = %d\n", error);
	} else {
		/* Successfully IO was submitted to the device. */
		return;
	}

error_io:
	ccb = rcb->cm_ccb;
	ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
	pqi_freeze_ccb(ccb);
	pqi_unmap_request(rcb);
	xpt_done(ccb);
	return;
}

/*
 * Function to dma-map the request buffer
 */
static int
pqi_map_request(rcb_t *rcb)
{
	pqisrc_softstate_t *softs = rcb->softs;
	int bsd_status = BSD_SUCCESS;
	union ccb *ccb = rcb->cm_ccb;

	DBG_FUNC("IN\n");

	/* check that mapping is necessary */
	if (rcb->cm_flags & PQI_CMD_MAPPED)
		return BSD_SUCCESS;

	rcb->cm_flags |= PQI_CMD_MAPPED;

	if (rcb->bcount) {
		bsd_status = bus_dmamap_load_ccb(softs->os_specific.pqi_buffer_dmat,
			rcb->cm_datamap, ccb, pqi_request_map_helper, rcb, 0);
		if (bsd_status != BSD_SUCCESS && bsd_status != EINPROGRESS) {
			DBG_ERR_BTL(rcb->dvp, "bus_dmamap_load_ccb failed, return status = %d transfer length = %d\n",
					bsd_status, rcb->bcount);
			return bsd_status;
		}
	} else {
		/*
		 * Set up the command to go to the controller.  If there are no
		 * data buffers associated with the command then it can bypass
		 * busdma.
		 */
		/* Call IO functions depending on pd or ld */
		rcb->status = REQUEST_PENDING;

		if (pqisrc_build_send_io(softs, rcb) != PQI_STATUS_SUCCESS) {
			bsd_status = EIO;
		}
	}

	DBG_FUNC("OUT error = %d\n", bsd_status);

	return bsd_status;
}

/*
 * Function to clear the request control block
 */
void
os_reset_rcb(rcb_t *rcb)
{
	rcb->error_info = NULL;
	rcb->req = NULL;
	rcb->status = -1;
	rcb->tag = INVALID_ELEM;
	rcb->dvp = NULL;
	rcb->cdbp = NULL;
	rcb->softs = NULL;
	rcb->cm_flags = 0;
	rcb->cm_data = NULL;
	rcb->bcount = 0;
	rcb->nseg = 0;
	rcb->sgt = NULL;
	rcb->cm_ccb = NULL;
	rcb->encrypt_enable = false;
	rcb->ioaccel_handle = 0;
	rcb->resp_qid = 0;
	rcb->req_pending = false;
	rcb->tm_req = false;
}

/*
 * Callback function for the lun rescan
 */
static void
smartpqi_lunrescan_cb(struct cam_periph *periph, union ccb *ccb)
{
        xpt_free_path(ccb->ccb_h.path);
        xpt_free_ccb(ccb);
}


/*
 * Function to rescan the lun
 */
static void
smartpqi_lun_rescan(struct pqisrc_softstate *softs, int target,
			int lun)
{
	union ccb *ccb = NULL;
	cam_status status = 0;
	struct cam_path *path = NULL;

	DBG_FUNC("IN\n");

	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		DBG_ERR("Unable to alloc ccb for lun rescan\n");
		return;
	}

	status = xpt_create_path(&path, NULL,
				cam_sim_path(softs->os_specific.sim), target, lun);
	if (status != CAM_REQ_CMP) {
		DBG_ERR("xpt_create_path status(%d) != CAM_REQ_CMP \n",
				 status);
		xpt_free_ccb(ccb);
		return;
	}

	bzero(ccb, sizeof(union ccb));
	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_LUN;
	ccb->ccb_h.cbfcnp = smartpqi_lunrescan_cb;
	ccb->crcn.flags = CAM_FLAG_NONE;

	xpt_action(ccb);

	DBG_FUNC("OUT\n");
}

/*
 * Function to rescan the lun under each target
 */
void
smartpqi_target_rescan(struct pqisrc_softstate *softs)
{
	int target = 0, lun = 0;

	DBG_FUNC("IN\n");

	for(target = 0; target < PQI_MAX_DEVICES; target++){
		for(lun = 0; lun < PQI_MAX_MULTILUN; lun++){
			if(softs->device_list[target][lun]){
				smartpqi_lun_rescan(softs, target, lun);
			}
		}
	}

	DBG_FUNC("OUT\n");
}

/*
 * Set the mode of tagged command queueing for the current task.
 */
uint8_t
os_get_task_attr(rcb_t *rcb)
{
	union ccb *ccb = rcb->cm_ccb;
	uint8_t tag_action = SOP_TASK_ATTRIBUTE_SIMPLE;

	switch(ccb->csio.tag_action) {
	case MSG_HEAD_OF_Q_TAG:
		tag_action = SOP_TASK_ATTRIBUTE_HEAD_OF_QUEUE;
		break;
	case MSG_ORDERED_Q_TAG:
		tag_action = SOP_TASK_ATTRIBUTE_ORDERED;
		break;
	case MSG_SIMPLE_Q_TAG:
	default:
		tag_action = SOP_TASK_ATTRIBUTE_SIMPLE;
		break;
	}
	return tag_action;
}

/*
 * Complete all outstanding commands
 */
void
os_complete_outstanding_cmds_nodevice(pqisrc_softstate_t *softs)
{
	int tag = 0;
	pqi_scsi_dev_t  *dvp = NULL;

	DBG_FUNC("IN\n");

	for (tag = 1; tag <= softs->max_outstanding_io; tag++) {
		rcb_t *prcb = &softs->rcb[tag];
		dvp = prcb->dvp;
		if(prcb->req_pending && prcb->cm_ccb ) {
			prcb->req_pending = false;
			prcb->cm_ccb->ccb_h.status = CAM_REQ_ABORTED | CAM_REQ_CMP;
			pqi_complete_scsi_io(&prcb->cm_ccb->csio, prcb);
			if (dvp)
				pqisrc_decrement_device_active_io(softs, dvp);

		}
	}

	DBG_FUNC("OUT\n");
}

/*
 * IO handling functionality entry point
 */
static int
pqisrc_io_start(struct cam_sim *sim, union ccb *ccb)
{
	rcb_t *rcb;
	uint32_t tag, no_transfer = 0;
	pqisrc_softstate_t *softs = (struct pqisrc_softstate *)
					cam_sim_softc(sim);
	int32_t error;
	pqi_scsi_dev_t *dvp;

	DBG_FUNC("IN\n");

	if (softs->device_list[ccb->ccb_h.target_id][ccb->ccb_h.target_lun] == NULL) {
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		DBG_INFO("Device  = %d not there\n", ccb->ccb_h.target_id);
		return ENXIO;
	}

	dvp = softs->device_list[ccb->ccb_h.target_id][ccb->ccb_h.target_lun];
	/* Check  controller state */
	if (IN_PQI_RESET(softs)) {
		ccb->ccb_h.status = CAM_SCSI_BUS_RESET
					| CAM_BUSY | CAM_REQ_INPROG;
		DBG_WARN("Device  = %d BUSY/IN_RESET\n", ccb->ccb_h.target_id);
		return ENXIO;
	}
	/* Check device state */
	if (pqisrc_ctrl_offline(softs) || DEV_GONE(dvp)) {
		ccb->ccb_h.status = CAM_DEV_NOT_THERE | CAM_REQ_CMP;
		DBG_WARN("Device  = %d GONE/OFFLINE\n", ccb->ccb_h.target_id);
		return ENXIO;
	}
	/* Check device reset */
	if (DEVICE_RESET(dvp)) {
		ccb->ccb_h.status = CAM_SCSI_BUSY | CAM_REQ_INPROG | CAM_BUSY;
		DBG_WARN("Device %d reset returned busy\n", ccb->ccb_h.target_id);
		return EBUSY;
	}

	if (dvp->expose_device == false) {
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		DBG_INFO("Device  = %d not exposed\n", ccb->ccb_h.target_id);
		return ENXIO;
	}

	tag = pqisrc_get_tag(&softs->taglist);
	if (tag == INVALID_ELEM) {
		DBG_ERR("Get Tag failed\n");
		xpt_freeze_simq(softs->os_specific.sim, 1);
		softs->os_specific.pqi_flags |= PQI_FLAG_BUSY;
		ccb->ccb_h.status |= (CAM_REQUEUE_REQ | CAM_RELEASE_SIMQ);
		return EIO;
	}

	DBG_IO("tag = %d &softs->taglist : %p\n", tag, &softs->taglist);

	rcb = &softs->rcb[tag];
	os_reset_rcb(rcb);
	rcb->tag = tag;
	rcb->softs = softs;
	rcb->cmdlen = ccb->csio.cdb_len;
	ccb->ccb_h.sim_priv.entries[0].ptr = rcb;

	switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
		case CAM_DIR_IN:
			rcb->data_dir = SOP_DATA_DIR_FROM_DEVICE;
			break;
		case CAM_DIR_OUT:
			rcb->data_dir = SOP_DATA_DIR_TO_DEVICE;
			break;
		case CAM_DIR_NONE:
			no_transfer = 1;
			break;
		default:
			DBG_ERR("Unknown Dir\n");
			break;
	}
	rcb->cm_ccb = ccb;
	rcb->dvp = softs->device_list[ccb->ccb_h.target_id][ccb->ccb_h.target_lun];

	if (!no_transfer) {
		rcb->cm_data = (void *)ccb->csio.data_ptr;
		rcb->bcount = ccb->csio.dxfer_len;
	} else {
		rcb->cm_data = NULL;
		rcb->bcount = 0;
	}
	/*
	 * Submit the request to the adapter.
	 *
	 * Note that this may fail if we're unable to map the request (and
	 * if we ever learn a transport layer other than simple, may fail
	 * if the adapter rejects the command).
	 */
	if ((error = pqi_map_request(rcb)) != BSD_SUCCESS) {
		xpt_freeze_simq(softs->os_specific.sim, 1);
		if (error == EINPROGRESS) {
			/* Release simq in the completion */
			softs->os_specific.pqi_flags |= PQI_FLAG_BUSY;
			error = BSD_SUCCESS;
		} else {
			rcb->req_pending = false;
			ccb->ccb_h.status |= CAM_REQUEUE_REQ | CAM_RELEASE_SIMQ;
			DBG_WARN("Requeue req error = %d target = %d\n", error,
				ccb->ccb_h.target_id);
			pqi_unmap_request(rcb);
			error = EIO;
		}
	}

	DBG_FUNC("OUT error = %d\n", error);

	return error;
}

static inline int
pqi_tmf_status_to_bsd_tmf_status(int pqi_status, rcb_t *rcb)
{
	if (PQI_STATUS_SUCCESS == pqi_status &&
			REQUEST_SUCCESS == rcb->status)
		return BSD_SUCCESS;
	else
		return EIO;
}

/*
 * Abort a task, task management functionality
 */
static int
pqisrc_scsi_abort_task(pqisrc_softstate_t *softs,  union ccb *ccb)
{
	struct ccb_hdr *ccb_h = &ccb->ccb_h;
	rcb_t *rcb = NULL;
	rcb_t *prcb = ccb->ccb_h.sim_priv.entries[0].ptr;
	uint32_t tag;
	int rval;

	DBG_FUNC("IN\n");

	tag = pqisrc_get_tag(&softs->taglist);
	rcb = &softs->rcb[tag];
	rcb->tag = tag;

	if (!rcb->dvp) {
		DBG_ERR("dvp is null, tmf type : 0x%x\n", ccb_h->func_code);
		rval = ENXIO;
		goto error_tmf;
	}

	rcb->tm_req = true;

	rval = pqisrc_send_tmf(softs, rcb->dvp, rcb, prcb,
		SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK);

	if ((rval = pqi_tmf_status_to_bsd_tmf_status(rval, rcb)) == BSD_SUCCESS)
		ccb->ccb_h.status = CAM_REQ_ABORTED;

error_tmf:
	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, tag);

	DBG_FUNC("OUT rval = %d\n", rval);

	return rval;
}

/*
 * Abort a taskset, task management functionality
 */
static int
pqisrc_scsi_abort_task_set(pqisrc_softstate_t *softs, union ccb *ccb)
{
	struct ccb_hdr *ccb_h = &ccb->ccb_h;
	rcb_t *rcb = NULL;
	uint32_t tag;
	int rval;

	DBG_FUNC("IN\n");

	tag = pqisrc_get_tag(&softs->taglist);
	rcb = &softs->rcb[tag];
	rcb->tag = tag;

	if (!rcb->dvp) {
		DBG_ERR("dvp is null, tmf type : 0x%x\n", ccb_h->func_code);
		rval = ENXIO;
		goto error_tmf;
	}

	rcb->tm_req = true;

	rval = pqisrc_send_tmf(softs, rcb->dvp, rcb, NULL,
			SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK_SET);

	rval = pqi_tmf_status_to_bsd_tmf_status(rval, rcb);

error_tmf:
	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, tag);

	DBG_FUNC("OUT rval = %d\n", rval);

	return rval;
}

/*
 * Target reset task management functionality
 */
static int
pqisrc_target_reset( pqisrc_softstate_t *softs,  union ccb *ccb)
{
	struct ccb_hdr  *ccb_h = &ccb->ccb_h;
	pqi_scsi_dev_t *devp = softs->device_list[ccb->ccb_h.target_id][ccb->ccb_h.target_lun];
	rcb_t *rcb = NULL;
	uint32_t tag;
	int rval;

	DBG_FUNC("IN\n");

	if (devp == NULL) {
		DBG_ERR("bad target %d, tmf type : 0x%x\n", ccb_h->target_id, ccb_h->func_code);
		return ENXIO;
	}

	tag = pqisrc_get_tag(&softs->taglist);
	rcb = &softs->rcb[tag];
	rcb->tag = tag;

	devp->reset_in_progress = true;

	rcb->tm_req = true;

	rval = pqisrc_send_tmf(softs, devp, rcb, NULL,
		SOP_TASK_MANAGEMENT_LUN_RESET);

	rval = pqi_tmf_status_to_bsd_tmf_status(rval, rcb);
	devp->reset_in_progress = false;

	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, tag);

	DBG_FUNC("OUT rval = %d\n", rval);

	return rval;

}

/*
 * cam entry point of the smartpqi module.
 */
static void
smartpqi_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct pqisrc_softstate *softs = cam_sim_softc(sim);
	struct ccb_hdr  *ccb_h = &ccb->ccb_h;

	DBG_FUNC("IN\n");

	switch (ccb_h->func_code) {
		case XPT_SCSI_IO:
		{
			if(!pqisrc_io_start(sim, ccb)) {
				return;
			}
			break;
		}
		case XPT_CALC_GEOMETRY:
		{
			struct ccb_calc_geometry *ccg;
			ccg = &ccb->ccg;
			if (ccg->block_size == 0) {
				ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
				ccb->ccb_h.status |= CAM_REQ_INVALID;
				break;
			}
			cam_calc_geometry(ccg, /* extended */ 1);
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
		case XPT_PATH_INQ:
		{
			update_sim_properties(sim, &ccb->cpi);
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
		case XPT_GET_TRAN_SETTINGS:
			get_transport_settings(softs, &ccb->cts);
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		case XPT_ABORT:
			if(pqisrc_scsi_abort_task(softs,  ccb)) {
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				xpt_done(ccb);
				DBG_ERR("Abort task failed on %d\n",
					ccb->ccb_h.target_id);
				return;
			}
			break;
		case XPT_TERM_IO:
			if (pqisrc_scsi_abort_task_set(softs,  ccb)) {
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				DBG_ERR("Abort task set failed on %d\n",
					ccb->ccb_h.target_id);
				xpt_done(ccb);
				return;
			}
			break;
		case XPT_RESET_DEV:
			if(pqisrc_target_reset(softs,  ccb)) {
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				DBG_ERR("Target reset failed on %d\n",
					ccb->ccb_h.target_id);
				xpt_done(ccb);
				return;
			} else {
				ccb->ccb_h.status = CAM_REQ_CMP;
			}
			break;
		case XPT_RESET_BUS:
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		case XPT_SET_TRAN_SETTINGS:
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			return;
		default:
			DBG_WARN("UNSUPPORTED FUNC CODE\n");
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			break;
	}
	xpt_done(ccb);

	DBG_FUNC("OUT\n");
}

/*
 * Function to poll the response, when interrupts are unavailable
 * This also serves supporting crash dump.
 */
static void
smartpqi_poll(struct cam_sim *sim)
{
	struct pqisrc_softstate *softs = cam_sim_softc(sim);
	int i;

	for (i = 1; i < softs->intr_count; i++ )
		pqisrc_process_response_queue(softs, i);
}

/*
 * Function to adjust the queue depth of a device
 */
void
smartpqi_adjust_queue_depth(struct cam_path *path, uint32_t queue_depth)
{
	struct ccb_relsim crs;

	DBG_INFO("IN\n");

	memset(&crs, 0, sizeof(crs));
	xpt_setup_ccb(&crs.ccb_h, path, 5);
	crs.ccb_h.func_code = XPT_REL_SIMQ;
	crs.ccb_h.flags = CAM_DEV_QFREEZE;
	crs.release_flags = RELSIM_ADJUST_OPENINGS;
	crs.openings = queue_depth;
	xpt_action((union ccb *)&crs);
	if(crs.ccb_h.status != CAM_REQ_CMP) {
		printf("XPT_REL_SIMQ failed stat=%d\n", crs.ccb_h.status);
	}

	DBG_INFO("OUT\n");
}

/*
 * Function to register async callback for setting queue depth
 */
static void
smartpqi_async(void *callback_arg, u_int32_t code,
		struct cam_path *path, void *arg)
{
	struct pqisrc_softstate *softs;
	softs = (struct pqisrc_softstate*)callback_arg;

	DBG_FUNC("IN\n");

	switch (code) {
		case AC_FOUND_DEVICE:
		{
			struct ccb_getdev *cgd;
			cgd = (struct ccb_getdev *)arg;
			if (cgd == NULL) {
				break;
			}
			uint32_t t_id = cgd->ccb_h.target_id;

			if (t_id <= (PQI_CTLR_INDEX - 1)) {
				if (softs != NULL) {
					pqi_scsi_dev_t *dvp = softs->device_list[t_id][cgd->ccb_h.target_lun];
					if (dvp == NULL) {
						DBG_ERR("Target is null, target id=%d\n", t_id);
						break;
					}
					smartpqi_adjust_queue_depth(path,
							dvp->queue_depth);
				}
			}
			break;
		}
		default:
			break;
	}

	DBG_FUNC("OUT\n");
}

/*
 * Function to register sim with CAM layer for smartpqi driver
 */
int
register_sim(struct pqisrc_softstate *softs, int card_index)
{
	int max_transactions;
	union ccb   *ccb = NULL;
	int error;
	struct ccb_setasync csa;
	struct cam_sim *sim;

	DBG_FUNC("IN\n");

	max_transactions = softs->max_io_for_scsi_ml;
	softs->os_specific.devq = cam_simq_alloc(max_transactions);
	if (softs->os_specific.devq == NULL) {
		DBG_ERR("cam_simq_alloc failed txns = %d\n",
			max_transactions);
		return ENOMEM;
	}

	sim = cam_sim_alloc(smartpqi_cam_action, \
				smartpqi_poll, "smartpqi", softs, \
				card_index, &softs->os_specific.cam_lock, \
				1, max_transactions, softs->os_specific.devq);
	if (sim == NULL) {
		DBG_ERR("cam_sim_alloc failed txns = %d\n",
			max_transactions);
		cam_simq_free(softs->os_specific.devq);
		return ENOMEM;
	}

	softs->os_specific.sim = sim;
	mtx_lock(&softs->os_specific.cam_lock);
	error = xpt_bus_register(sim, softs->os_specific.pqi_dev, 0);
	if (error != CAM_SUCCESS) {
		DBG_ERR("xpt_bus_register failed errno %d\n", error);
		cam_sim_free(softs->os_specific.sim, FALSE);
		cam_simq_free(softs->os_specific.devq);
		mtx_unlock(&softs->os_specific.cam_lock);
		return ENXIO;
	}

	softs->os_specific.sim_registered = TRUE;
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		DBG_ERR("xpt_create_path failed\n");
		return ENXIO;
	}

	if (xpt_create_path(&ccb->ccb_h.path, NULL,
			cam_sim_path(softs->os_specific.sim),
			CAM_TARGET_WILDCARD,
			CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		DBG_ERR("xpt_create_path failed\n");
		xpt_free_ccb(ccb);
		xpt_bus_deregister(cam_sim_path(softs->os_specific.sim));
		cam_sim_free(softs->os_specific.sim, TRUE);
		mtx_unlock(&softs->os_specific.cam_lock);
		return ENXIO;
	}
	/*
 	 * Callback to set the queue depth per target which is 
	 * derived from the FW.
 	 */
	softs->os_specific.path = ccb->ccb_h.path;
	memset(&csa, 0, sizeof(csa));
	xpt_setup_ccb(&csa.ccb_h, softs->os_specific.path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_FOUND_DEVICE;
	csa.callback = smartpqi_async;
	csa.callback_arg = softs;
	xpt_action((union ccb *)&csa);
	if (csa.ccb_h.status != CAM_REQ_CMP) {
		DBG_ERR("Unable to register smartpqi_aysnc handler: %d!\n", 
			csa.ccb_h.status);
	}

	mtx_unlock(&softs->os_specific.cam_lock);
	DBG_INFO("OUT\n");

	return BSD_SUCCESS;
}

/*
 * Function to deregister smartpqi sim from cam layer
 */
void
deregister_sim(struct pqisrc_softstate *softs)
{
	struct ccb_setasync csa;
	
	DBG_FUNC("IN\n");

	if (softs->os_specific.mtx_init) {
		mtx_lock(&softs->os_specific.cam_lock);
	}


	memset(&csa, 0, sizeof(csa));
	xpt_setup_ccb(&csa.ccb_h, softs->os_specific.path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = smartpqi_async;
	csa.callback_arg = softs;
	xpt_action((union ccb *)&csa);
	xpt_free_path(softs->os_specific.path);

	if (softs->os_specific.sim) {
		xpt_release_simq(softs->os_specific.sim, 0);
		xpt_bus_deregister(cam_sim_path(softs->os_specific.sim));
		softs->os_specific.sim_registered = FALSE;
		cam_sim_free(softs->os_specific.sim, FALSE);
		softs->os_specific.sim = NULL;
	}

	if (softs->os_specific.mtx_init) {
		mtx_unlock(&softs->os_specific.cam_lock);
	}
	if (softs->os_specific.devq != NULL) {
		cam_simq_free(softs->os_specific.devq);
	}
	if (softs->os_specific.mtx_init) {
		mtx_destroy(&softs->os_specific.cam_lock);
		softs->os_specific.mtx_init = FALSE;
	}

	mtx_destroy(&softs->os_specific.map_lock);

	DBG_FUNC("OUT\n");
}

void
os_rescan_target(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
       struct cam_path *tmppath;

       DBG_FUNC("IN\n");

       if(softs->os_specific.sim_registered) {
               if (xpt_create_path(&tmppath, NULL,
                       cam_sim_path(softs->os_specific.sim),
                       device->target, device->lun) != CAM_REQ_CMP) {
                       DBG_ERR("unable to create path for async event!!! Bus: %d Target: %d Lun: %d\n",
                               device->bus, device->target, device->lun);
                       return;
               }
               xpt_async(AC_INQ_CHANGED, tmppath, NULL);
               xpt_free_path(tmppath);
       }

       device->scsi_rescan = false;

       DBG_FUNC("OUT\n");
}
