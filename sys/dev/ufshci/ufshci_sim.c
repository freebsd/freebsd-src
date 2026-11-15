/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include "ufshci_private.h"

#define sim2ctrlr(sim) ((struct ufshci_controller *)cam_sim_softc(sim))

/*
 * The transport says whether the command reached the device, and the
 * response says what the device made of it. The SCSI result is read out
 * of the response here, so the error flag is not used.
 */
static void
ufshci_sim_scsiio_done(void *ccb_arg, const struct ufshci_completion *cpl,
    bool error)
{
	union ccb *ccb = (union ccb *)ccb_arg;
	struct ccb_scsiio *csio = &ccb->csio;
	const struct ufshci_cmd_response_upiu *resp =
	    &cpl->response_upiu.cmd_response_upiu;
	uint16_t sense_len;

	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;

	if (cpl->ocs == UFSHCI_DESC_ABORTED) {
		/*
		 * A reset took the request back, so requeue it without a
		 * retry. A failed controller rejects the resubmit, so
		 * this cannot loop.
		 */
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}

	if (cpl->ocs != UFSHCI_DESC_SUCCESS) {
		/* The command never reached the device intact. */
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
		return;
	}

	csio->scsi_status = resp->header.ext_iid_or_status;

	/* A failed response with a GOOD status carries no result. */
	if (csio->scsi_status == SCSI_STATUS_OK &&
	    resp->header.response != UFSHCI_RESPONSE_CODE_TARGET_SUCCESS) {
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
		return;
	}

	if (csio->scsi_status == SCSI_STATUS_OK) {
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done_direct(ccb);
		return;
	}

	ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;

	if (csio->scsi_status == SCSI_STATUS_CHECK_COND) {
		sense_len = be16toh(resp->sense_data_len);
		sense_len = min(sense_len, sizeof(resp->sense_data));
		sense_len = min(sense_len, csio->sense_len);
		memcpy(&csio->sense_data, resp->sense_data, sense_len);
		csio->sense_resid = csio->sense_len - sense_len;
		if (sense_len > 0)
			ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}

	/*
	 * Hold the device queue until CAM has read this. Without the
	 * freeze it reissues at once and spends its retries on a device
	 * that is telling it to wait.
	 */
	ccb->ccb_h.status |= CAM_DEV_QFRZN;
	xpt_freeze_devq(ccb->ccb_h.path, 1);
	xpt_done(ccb);
}

/*
 * Complete the command as an illegal command with invalid field
 */
static void
ufshci_sim_illegal_request(union ccb *ccb)
{
	scsi_set_sense_data(&ccb->csio.sense_data,
	    /*sense_format*/ SSD_TYPE_NONE,
	    /*current_error*/ 1,
	    /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
	    /*asc*/ 0x24, /* 24h/00h INVALID FIELD IN CDB */
	    /*ascq*/ 0x00,
	    /*extra args*/ SSD_ELEM_NONE);
	ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
	ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR | CAM_AUTOSNS_VALID |
	    CAM_DEV_QFRZN;
	xpt_freeze_devq(ccb->ccb_h.path, 1);
	xpt_done(ccb);
}

/*
 * The SCSI LUN format and the UFS UPIU LUN format are different.
 * This function converts the SCSI LUN format to the UFS UPIU LUN format.
 */
uint8_t
ufshci_sim_translate_scsi_to_ufs_lun(lun_id_t scsi_lun)
{
	const int address_format_offset = 8;
	uint8_t address_format = scsi_lun >> address_format_offset;

	/* Well known logical unit */
	if (((address_format & RPL_LUNDATA_ATYP_MASK) ==
		RPL_LUNDATA_ATYP_EXTLUN) &&
	    ((address_format & RPL_LUNDATA_EXT_EAM_MASK) ==
		RPL_LUNDATA_EXT_EAM_WK))
		return ((scsi_lun & UFSHCI_UPIU_UNIT_NUMBER_ID_MASK) |
		    UFSHCI_UPIU_WLUN_ID_MASK);

	/* Logical unit */
	return (scsi_lun & UFSHCI_UPIU_UNIT_NUMBER_ID_MASK);
}

uint64_t
ufshci_sim_translate_ufs_to_scsi_lun(uint8_t ufs_lun)
{
	/* Logical unit */
	if (!(ufs_lun & UFSHCI_UPIU_WLUN_ID_MASK)) {
		return ufs_lun;
	}

	/* Well known logical unit */
	return (((uint64_t)ufs_lun & ~UFSHCI_UPIU_WLUN_ID_MASK) |
	    (RPL_LUNDATA_ATYP_EXTLUN | RPL_LUNDATA_EXT_EAM_WK) << 8);
}

static void
ufshchi_sim_scsiio(struct cam_sim *sim, union ccb *ccb)
{
	struct ccb_scsiio *csio = &ccb->csio;
	struct ufshci_request *req;
	void *payload;
	struct ufshci_cmd_command_upiu *upiu;
	uint8_t *cdb;
	uint32_t payload_len;
	bool is_write;
	struct ufshci_controller *ctrlr;
	uint8_t data_direction;
	int error;

	/* UFS device cannot process these commands */
	if (csio->cdb_io.cdb_bytes[0] == MODE_SENSE_6 ||
	    csio->cdb_io.cdb_bytes[0] == MODE_SELECT_6 ||
	    csio->cdb_io.cdb_bytes[0] == READ_12 ||
	    csio->cdb_io.cdb_bytes[0] == WRITE_12) {
		ufshci_sim_illegal_request(ccb);
		return;
	}

	ctrlr = sim2ctrlr(sim);
	payload = csio->data_ptr;

	payload_len = csio->dxfer_len;
	is_write = csio->ccb_h.flags & CAM_DIR_OUT;

	if (csio->ccb_h.flags & CAM_CDB_POINTER)
		cdb = csio->cdb_io.cdb_ptr;
	else
		cdb = csio->cdb_io.cdb_bytes;

	if (cdb == NULL || csio->cdb_len > sizeof(upiu->cdb)) {
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		return;
	}

	/* TODO: Check other data type */
	if ((csio->ccb_h.flags & CAM_DATA_MASK) == CAM_DATA_BIO)
		req = ufshci_allocate_request_bio((struct bio *)payload,
		    M_NOWAIT, ufshci_sim_scsiio_done, ccb);
	else
		req = ufshci_allocate_request_vaddr(payload, payload_len,
		    M_NOWAIT, ufshci_sim_scsiio_done, ccb);
	if (req == NULL) {
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	req->request_size = sizeof(struct ufshci_cmd_command_upiu);
	req->response_size = sizeof(struct ufshci_cmd_response_upiu);

	switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		data_direction = UFSHCI_DATA_DIRECTION_FROM_TGT_TO_SYS;
		break;
	case CAM_DIR_OUT:
		data_direction = UFSHCI_DATA_DIRECTION_FROM_SYS_TO_TGT;
		break;
	default:
		data_direction = UFSHCI_DATA_DIRECTION_NO_DATA_TRANSFER;
	}
	req->data_direction = data_direction;
	req->is_admin = false;

	upiu = (struct ufshci_cmd_command_upiu *)&req->request_upiu;
	memset(upiu, 0, req->request_size);
	upiu->header.trans_type = UFSHCI_UPIU_TRANSACTION_CODE_COMMAND;
	upiu->header.operational_flags = is_write ? UFSHCI_OPERATIONAL_FLAG_W :
						    UFSHCI_OPERATIONAL_FLAG_R;
	upiu->header.lun = ufshci_sim_translate_scsi_to_ufs_lun(
	    csio->ccb_h.target_lun);
	upiu->header.cmd_set_type = UFSHCI_COMMAND_SET_TYPE_SCSI;

	upiu->expected_data_transfer_length = htobe32(payload_len);

	memcpy(upiu->cdb, cdb, csio->cdb_len);

	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	error = ufshci_ctrlr_submit_transfer_request(ctrlr, req);
	if (error == EBUSY) {
		/*
		 * No slot is free. Freeze and requeue so CAM neither
		 * spins nor burns its retries.
		 */
		xpt_freeze_devq(ccb->ccb_h.path, 1);
		ccb->ccb_h.status = CAM_REQUEUE_REQ | CAM_DEV_QFRZN;
		ufshci_free_request(req);
		xpt_done(ccb);
		return;
	} else if (error) {
		ccb->ccb_h.status = CAM_REQ_INVALID;
		ufshci_free_request(req);
		xpt_done(ccb);
		return;
	}
}

static uint32_t
ufshci_link_kBps(struct ufshci_controller *ctrlr)
{
	uint32_t gear = ctrlr->hs_gear;
	uint32_t lanes = ctrlr->rx_lanes;

	/*
	 * per-lane effective bandwidth (KB/s, SI 1 KB = 1000 B)
	 * All HS-Gears use 8b/10b line coding, i.e. 80 % efficiency.
	 * - KB/s per lane = raw-rate(Gbps) × 0.8(8b/10b) / 8(bit)
	 */
	static const uint32_t kbps_per_lane[] = {
		0,	 /* unused */
		145920,	 /* HS-Gear1 : 1459.2 Mbps */
		291840,	 /* HS-Gear2 : 2918.4 Mbps */
		583680,	 /* HS-Gear3 : 5836.8 Mbps */
		1167360, /* HS-Gear4 : 11673.6 Mbps */
		2334720	 /* HS-Gear5 : 23347.2 Mbps */
	};

	/* Sanity checks */
	if (gear >= nitems(kbps_per_lane))
		gear = 0; /* out-of-range -> treat as invalid */

	if (lanes == 0 || lanes > 2)
		lanes = 1; /* UFS spec allows 1–2 data lanes */

	return kbps_per_lane[gear] * lanes;
}

static void
ufshci_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct ufshci_controller *ctrlr = sim2ctrlr(sim);

	if (ctrlr == NULL) {
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;
		xpt_done(ccb);
		return;
	}

	/* Perform the requested action */
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
		ufshchi_sim_scsiio(sim, ccb);
		return;
	case XPT_PATH_INQ: {
		struct ccb_pathinq *cpi = &ccb->cpi;
		uint32_t need_scan_wluns = 0;

		if (!(ctrlr->quirks & UFSHCI_QUIRK_SKIP_WELL_KNOWN_LUNS))
			need_scan_wluns = PIM_WLUNS;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE | PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = need_scan_wluns | PIM_UNMAPPED | PIM_NO_6_BYTE;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 0;
		cpi->max_lun = ctrlr->max_lun_count - 1;
		cpi->async_flags = 0;
		cpi->maxio = ctrlr->max_xfer_size;
		cpi->initiator_id = 1;
		strlcpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strlcpy(cpi->hba_vid, "UFSHCI", HBA_IDLEN);
		strlcpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->base_transfer_speed = ufshci_link_kBps(ctrlr);
		cpi->transport = XPORT_UFSHCI;
		cpi->transport_version = 1;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_SPC5;
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_RESET_BUS:
	case XPT_RESET_DEV:
		/*
		 * This callback cannot sleep: CAM calls it with the SIM
		 * lock and the CAM device lock held. It cannot reset the
		 * device here. Report success so CAM keeps going, like
		 * nvme_sim(4) does.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_ABORT:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		break;
	case XPT_SET_TRAN_SETTINGS:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		break;
	case XPT_GET_TRAN_SETTINGS: {
		struct ccb_trans_settings *cts;
		struct ccb_trans_settings_ufshci *ufshcix;

		cts = &ccb->cts;
		ufshcix = &cts->xport_specific.ufshci;

		ufshcix->hs_gear = ctrlr->hs_gear;
		ufshcix->tx_lanes = ctrlr->tx_lanes;
		ufshcix->rx_lanes = ctrlr->rx_lanes;
		ufshcix->max_hs_gear = ctrlr->max_rx_hs_gear;
		ufshcix->max_tx_lanes = ctrlr->max_tx_lanes;
		ufshcix->max_rx_lanes = ctrlr->max_rx_lanes;
		ufshcix->valid = CTS_UFSHCI_VALID_LINK;

		cts->transport = XPORT_UFSHCI;
		cts->transport_version = 1;
		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_SPC5;
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_CALC_GEOMETRY:
		cam_calc_geometry(&ccb->ccg, 1);
		break;
	case XPT_NOOP:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	default:
		printf("invalid ccb=%p func=%#x\n", ccb, ccb->ccb_h.func_code);
		break;
	}
	xpt_done(ccb);

	return;
}

static void
ufshci_cam_poll(struct cam_sim *sim)
{
	struct ufshci_controller *ctrlr = sim2ctrlr(sim);

	ufshci_ctrlr_poll(ctrlr);
}

int
ufshci_sim_attach(struct ufshci_controller *ctrlr)
{
	device_t dev;
	struct cam_devq *devq;
	int max_trans;

	dev = ctrlr->dev;
	max_trans = ctrlr->max_hw_pend_io;
	if ((devq = cam_simq_alloc(max_trans)) == NULL) {
		printf("Failed to allocate a simq\n");
		return (ENOMEM);
	}

	ctrlr->ufshci_sim = cam_sim_alloc(ufshci_cam_action, ufshci_cam_poll,
	    "ufshci", ctrlr, device_get_unit(dev), &ctrlr->sc_mtx, max_trans,
	    max_trans, devq);
	if (ctrlr->ufshci_sim == NULL) {
		printf("Failed to allocate a sim\n");
		cam_simq_free(devq);
		return (ENOMEM);
	}

	mtx_lock(&ctrlr->sc_mtx);
	if (xpt_bus_register(ctrlr->ufshci_sim, ctrlr->dev, 0) != CAM_SUCCESS) {
		/* cam_sim_free() with free_devq also frees the devq. */
		cam_sim_free(ctrlr->ufshci_sim, /*free_devq*/ TRUE);
		ctrlr->ufshci_sim = NULL;
		mtx_unlock(&ctrlr->sc_mtx);
		printf("Failed to create a bus\n");
		return (ENOMEM);
	}

	if (xpt_create_path(&ctrlr->ufshci_path, /*periph*/ NULL,
		cam_sim_path(ctrlr->ufshci_sim), CAM_TARGET_WILDCARD,
		CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(ctrlr->ufshci_sim));
		cam_sim_free(ctrlr->ufshci_sim, /*free_devq*/ TRUE);
		ctrlr->ufshci_sim = NULL;
		mtx_unlock(&ctrlr->sc_mtx);
		printf("Failed to create a path\n");
		return (ENOMEM);
	}
	mtx_unlock(&ctrlr->sc_mtx);

	return (0);
}

/*
 * Drop the cached WLUN periph reference. cam_periph_release() takes the
 * CAM device lock itself, so call this without sc_mtx held: CAM takes
 * the device lock before the SIM lock, not the other way around.
 */
void
ufshci_sim_release_wlun_periph(struct ufshci_controller *ctrlr)
{
	if (ctrlr->ufs_device_wlun_periph != NULL) {
		cam_periph_release(ctrlr->ufs_device_wlun_periph);
		ctrlr->ufs_device_wlun_periph = NULL;
	}
}

void
ufshci_sim_controller_failed(struct ufshci_controller *ctrlr)
{
	/* Attach may fail before the path is set up. */
	if (ctrlr->ufshci_path == NULL)
		return;

	/*
	 * Hand the loss to CAM. The event goes to every periph on the
	 * devices under the path, where cam_periph_async() invalidates
	 * each one, so da(4) detaches and the disk node goes away rather
	 * than pointing at a controller that answers nothing. The path
	 * stored at attach covers the whole bus, so one post reaches
	 * every logical unit. xpt_async clones the path, so the stored
	 * one survives the call.
	 */
	xpt_async(AC_LOST_DEVICE, ctrlr->ufshci_path, NULL);
}

void
ufshci_sim_detach(struct ufshci_controller *ctrlr)
{
	int error;

	if (ctrlr->ufshci_path != NULL) {
		xpt_free_path(ctrlr->ufshci_path);
		ctrlr->ufshci_path = NULL;
	}

	if (ctrlr->ufshci_sim != NULL) {
		error = xpt_bus_deregister(cam_sim_path(ctrlr->ufshci_sim));
		if (error == 0) {
			/* accessing the softc is not possible after this */
			ctrlr->ufshci_sim->softc = NULL;
			ufshci_printf(ctrlr,
			    "%s: %s:%d:%d caling "
			    "cam_sim_free sim %p refc %u mtx %p\n",
			    __func__, ctrlr->sc_name,
			    cam_sim_path(ctrlr->ufshci_sim), ctrlr->sc_unit,
			    ctrlr->ufshci_sim, ctrlr->ufshci_sim->refcount,
			    ctrlr->ufshci_sim->mtx);
		} else {
			panic("%s: %s: CAM layer is busy: errno %d\n", __func__,
			    ctrlr->sc_name, error);
		}

		cam_sim_free(ctrlr->ufshci_sim, /* free_devq */ TRUE);
		ctrlr->ufshci_sim = NULL;
	}
}

/*
 * On success this returns a referenced periph; the caller is responsible
 * for dropping the reference with cam_periph_release().
 */
struct cam_periph *
ufshci_sim_find_periph(struct ufshci_controller *ctrlr, uint8_t wlun)
{
	struct cam_path *path;
	struct cam_periph *periph = NULL;
	uint64_t scsi_lun;
	sbintime_t timeout;

	/* The reset path can get here before the SIM is attached. */
	if (ctrlr->ufshci_sim == NULL)
		return (NULL);

	scsi_lun = ufshci_sim_translate_ufs_to_scsi_lun(wlun);

	if (xpt_create_path(&path, /*periph*/ NULL,
		cam_sim_path(ctrlr->ufshci_sim), 0, scsi_lun) != CAM_REQ_CMP) {
		return NULL;
	}

	/* Wait for the perip device to be found */
	timeout = sbinuptime() + mstosbt(ctrlr->device_init_timeout_in_ms);

	while (1) {
		xpt_path_lock(path);
		periph = cam_periph_find(path, "pass");
		if (periph != NULL && cam_periph_acquire(periph) != 0)
			periph = NULL;
		xpt_path_unlock(path);

		if (periph)
			break;

		if (sbinuptime() >= timeout) {
			ufshci_printf(ctrlr,
			    "Failed to find the Well known LUN(0x%x)\n", wlun);
			break;
		}

		pause_sbt("ufshci_find_periph", ustosbt(100), 0, C_PREL(1));
	}

	xpt_free_path(path);

	return periph;
}

/* This function is called during suspend/resume. */
int
ufshci_sim_send_ssu(struct ufshci_controller *ctrlr, bool start,
    uint8_t power_condition, bool immed)
{
	struct cam_periph *periph = ctrlr->ufs_device_wlun_periph;
	union ccb *ccb;
	int err;

	/* Acquire a periph reference for the duration of this call. */
	if (periph != NULL && cam_periph_acquire(periph) != 0) {
		/* The cached periph is going away; drop its reference. */
		cam_periph_release(periph);
		ctrlr->ufs_device_wlun_periph = NULL;
		periph = NULL;
	}

	if (periph == NULL) {
		/*
		 * If the periph device does not exist, try to find it again.
		 * The reference returned by ufshci_sim_find_periph() is used
		 * for this call; take an extra one for the cached pointer.
		 */
		periph = ufshci_sim_find_periph(ctrlr,
		    (uint8_t)UFSHCI_WLUN_UFS_DEVICE);
		if (periph != NULL && cam_periph_acquire(periph) == 0)
			ctrlr->ufs_device_wlun_periph = periph;
	}

	if (periph == NULL) {
		ufshci_printf(ctrlr,
		    "Well-known LUN `UFS Device (0x50)` not found\n");
		return ENODEV;
	}
	cam_periph_lock(periph);
	ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
	if (!ccb) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return ENOMEM;
	}

	scsi_start_stop_pc(&ccb->csio,
	    /*retries*/ 4,
	    /*cbfcnp*/ NULL,
	    /*tag_action*/ MSG_SIMPLE_Q_TAG,
	    /*start*/ start ? 1 : 0,
	    /*load_eject*/ 0,
	    /*immediate*/ immed ? 1 : 0,
	    /*power_condition*/ power_condition, SSD_MIN_SIZE,
	    ctrlr->device_init_timeout_in_ms);

	ccb->ccb_h.flags |= CAM_DIR_NONE | CAM_DEV_QFRZDIS;

	err = cam_periph_runccb(ccb, NULL, 0, SF_RETRY_UA, NULL);

	xpt_release_ccb(ccb);

	cam_periph_unlock(periph);
	/* Release periph reference */
	cam_periph_release(periph);

	return (err == 0) ? 0 : EIO;
}
