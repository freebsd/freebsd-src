/*-
 * Copyright (c) 2003-04 3ware, Inc.
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
 *
 *	$FreeBSD$
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#include <dev/twa/twa_includes.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

static int	twa_execute_scsi(struct twa_request *tr, union ccb *ccb);
static void	twa_action(struct cam_sim *sim, union ccb *ccb);
static void	twa_poll(struct cam_sim *sim);
static void	twa_async(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg);
static void	twa_bus_scan_cb(struct cam_periph *periph, union ccb *ccb);



/*
 * Function name:	twa_cam_setup
 * Description:		Attaches the driver to CAM.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_cam_setup(struct twa_softc *sc)
{
	struct cam_devq		*devq;
	struct ccb_setasync	csa;

	twa_dbg_dprint(3, sc, "sc = %p", sc);
	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(TWA_Q_LENGTH);
	if (devq == NULL)
		return(ENOMEM);

	/*
	 * Create a SIM entry.  Though we can support TWA_Q_LENGTH simultaneous
	 * requests, we claim to be able to handle only (TWA_Q_LENGTH - 1), so
	 * that we always have a request packet available to service attention
	 * interrupts.
	 */
	twa_dbg_dprint(3, sc, "Calling cam_sim_alloc");
	sc->twa_sim = cam_sim_alloc(twa_action, twa_poll, "twa", sc,
					device_get_unit(sc->twa_bus_dev),
					TWA_Q_LENGTH - 1, 1, devq);
	if (sc->twa_sim == NULL) {
		cam_simq_free(devq);
		return(ENOMEM);
	}

	/*
	 * Register the bus.
	 */
	twa_dbg_dprint(3, sc, "Calling xpt_bus_register");
	if (xpt_bus_register(sc->twa_sim, 0) != CAM_SUCCESS) {
		cam_sim_free(sc->twa_sim, TRUE);
		sc->twa_sim = NULL; /* so twa_cam_detach will not try to free it */
		return(ENXIO);
	}

	twa_dbg_dprint(3, sc, "Calling xpt_create_path");
	if (xpt_create_path(&sc->twa_path, NULL,
				cam_sim_path(sc->twa_sim),
				CAM_TARGET_WILDCARD,
				CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path (sc->twa_sim));
		cam_sim_free(sc->twa_sim, TRUE); /* passing TRUE will free the devq as well */
		return(ENXIO);
	}

	twa_dbg_dprint(3, sc, "Calling xpt_setup_ccb");
	xpt_setup_ccb(&csa.ccb_h, sc->twa_path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_FOUND_DEVICE | AC_LOST_DEVICE;
	csa.callback = twa_async;
	csa.callback_arg = sc;
	xpt_action((union ccb *)&csa);

	twa_dbg_dprint(3, sc, "Calling twa_request_bus_scan");
	/*
	 * Request a bus scan, so that CAM gets to know of
	 * the logical units that we control.
	 */
	twa_request_bus_scan(sc);
	twa_dbg_dprint(3, sc, "Exiting");
	return(0);
}



/*
 * Function name:	twa_cam_detach
 * Description:		Detaches the driver from CAM.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_cam_detach(struct twa_softc *sc)
{
	if (sc->twa_path)
		xpt_free_path(sc->twa_path);
	if (sc->twa_sim) {
		xpt_bus_deregister(cam_sim_path(sc->twa_sim));
		cam_sim_free(sc->twa_sim, TRUE); /* passing TRUE will free the devq as well */
	}
}



/*
 * Function name:	twa_send_scsi_cmd
 * Description:		Sends down a scsi cmd to fw.
 *
 * Input:		tr	-- ptr to request pkt
 *			cmd	-- opcode of scsi cmd to send
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_send_scsi_cmd(struct twa_request *tr, int cmd)
{
	union ccb	ccb;

	bzero(&ccb, sizeof(union ccb));
	ccb.csio.cdb_io.cdb_bytes[0] = (u_int8_t)cmd;
	ccb.csio.cdb_io.cdb_bytes[4] = 128;
	ccb.csio.cdb_len = 16;
	if ((ccb.csio.data_ptr = malloc(TWA_SECTOR_SIZE, M_DEVBUF, M_NOWAIT))
					== NULL)
		return(ENOMEM);
	bzero(ccb.csio.data_ptr, TWA_SECTOR_SIZE);
	ccb.csio.dxfer_len = TWA_SECTOR_SIZE;

	ccb.ccb_h.target_id = 0;
	ccb.ccb_h.flags |= CAM_DIR_IN;

	if (twa_execute_scsi(tr, &ccb))
		return(EIO);
	return(0);
}



/*
 * Function name:	twa_execute_scsi
 * Description:		Build a fw cmd, based on a CAM style ccb, and
 *			send it down.
 *
 * Input:		tr	-- ptr to request pkt
 *			ccb	-- ptr to CAM style ccb
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_execute_scsi(struct twa_request *tr, union ccb *ccb)
{
	struct twa_softc		*sc = tr->tr_sc;
	struct twa_command_packet	*cmdpkt;
	struct twa_command_9k		*cmd9k;
	struct ccb_hdr			*ccb_h = &(ccb->ccb_h);
	struct ccb_scsiio		*csio = &(ccb->csio);
	int				error;

	twa_dbg_dprint(3, sc, "SCSI I/O request 0x%x", 
				csio->cdb_io.cdb_bytes[0]);

	if (ccb_h->target_id >= TWA_MAX_UNITS) {
		twa_dbg_dprint(3, sc, "Invalid target. PTL = %x %x %x",
			ccb_h->path_id, ccb_h->target_id, ccb_h->target_lun);
		ccb_h->status |= CAM_TID_INVALID;
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL)
			xpt_done(ccb);
		return(1);
	}
	if (ccb_h->target_lun != 0) {
		twa_dbg_dprint(3, sc, "Invalid lun. PTL = %x %x %x",
			ccb_h->path_id, ccb_h->target_id, ccb_h->target_lun);
		ccb_h->status |= CAM_LUN_INVALID;
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL)
			xpt_done(ccb);
		return(1);
	}

	if(ccb_h->flags & CAM_CDB_PHYS) {
		twa_printf(sc, "Physical CDB address!\n");
		ccb_h->status = CAM_REQ_CMP_ERR;
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL)
			xpt_done(ccb);
		return(1);
	}

	/*
	 * We are going to work on this request.  Mark it as enqueued (though
	 * we don't actually queue it...)
	 */
	ccb_h->status |= CAM_SIM_QUEUED;

	if((ccb_h->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if(ccb_h->flags & CAM_DIR_IN)
			tr->tr_flags |= TWA_CMD_DATA_IN;
		else
			tr->tr_flags |= TWA_CMD_DATA_OUT;
	}

	cmdpkt = tr->tr_command;

	cmdpkt->cmd_hdr.header_desc.size_header = 128;
		
	cmd9k = &(cmdpkt->command.cmd_pkt_9k);
	cmd9k->command.opcode = TWA_OP_EXECUTE_SCSI_COMMAND;
	cmd9k->unit = ccb_h->target_id;
	cmd9k->request_id = tr->tr_request_id;
	cmd9k->status = 0;
	cmd9k->sgl_offset = 16; /* offset from end of hdr = max cdb len */

	if(ccb_h->flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, cmd9k->cdb, csio->cdb_len);
	else
		bcopy(csio->cdb_io.cdb_bytes, cmd9k->cdb, csio->cdb_len);

	if (!(ccb_h->flags & CAM_DATA_PHYS)) {
		/* Virtual data addresses.  Need to convert them... */
		twa_dbg_dprint(3, sc, "XPT_SCSI_IO: Single virtual address!");
		if (!(ccb_h->flags & CAM_SCATTER_VALID)) {
			if (csio->dxfer_len > TWA_MAX_IO_SIZE) {
				twa_printf(sc, "I/O size %d too big.\n",
							csio->dxfer_len);
				ccb_h->status = CAM_REQ_TOO_BIG;
				if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL)
					xpt_done(ccb);
				return(1);
			}

			if ((tr->tr_length = csio->dxfer_len)) {
				tr->tr_data = csio->data_ptr;
				cmd9k->sgl_entries = 1;
			}
		} else {
			twa_printf(sc, "twa_execute_scsi: XPT_SCSI_IO: Got SGList!\n");
			ccb_h->status = CAM_REQ_CMP_ERR;
			if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL) {
				xpt_done(ccb);
			}
			return(1);
		}
	} else {
		/* Data addresses are physical. */
		twa_printf(sc, "twa_execute_scsi: XPT_SCSI_IO: Physical data addresses!\n");
		ccb_h->status = CAM_REQ_CMP_ERR;
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL) {
			ccb_h->status |= CAM_RELEASE_SIMQ;
			ccb_h->status &= ~CAM_SIM_QUEUED;
			xpt_done(ccb);
		}
		return(1);
	}

	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_9K;
	/* twa_setup_data_dmamap will fill in the SGL, and submit the I/O. */
	error = twa_map_request(tr);
	return(error);
}



/*
 * Function name:	twa_action
 * Description:		Driver entry point for CAM's use.
 *
 * Input:		sim	-- sim corresponding to the ctlr
 *			ccb	-- ptr to CAM request
 * Output:		None
 * Return value:	None
 */
void
twa_action(struct cam_sim *sim, union ccb *ccb)
{
	struct twa_softc	*sc = (struct twa_softc *)cam_sim_softc(sim);
	struct ccb_hdr		*ccb_h = &(ccb->ccb_h);

	switch (ccb_h->func_code) {
	case XPT_SCSI_IO:	/* SCSI I/O */
	{
		struct twa_request	*tr;

		if ((sc->twa_state & TWA_STATE_SIMQ_FROZEN) ||
				((tr = twa_get_request(sc)) == NULL)) {
			twa_dbg_dprint(2, sc, "simq frozen/Cannot get request pkt.");
			/*
			 * Freeze the simq to maintain ccb ordering.  The next
			 * ccb that gets completed will unfreeze the simq.
			 */
			twa_disallow_new_requests(sc);
			ccb_h->status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		}
		tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_EXTERNAL;
		tr->tr_private = ccb;
		tr->tr_callback = twa_complete_io;
		if (twa_execute_scsi(tr, ccb))
			twa_release_request(tr);
		break;
	}

	case XPT_ABORT:
		twa_dbg_dprint(2, sc, "Abort request");
		ccb_h->status = CAM_UA_ABORT;
		xpt_done(ccb);
		break;

	case XPT_RESET_BUS:
		twa_printf(sc, "Reset Bus request from CAM...\n");
		if (twa_reset(sc)) {
			twa_printf(sc, "Reset Bus failed!\n");
			ccb_h->status = CAM_REQ_CMP_ERR;
		}
		else
			ccb_h->status = CAM_REQ_CMP;

		xpt_done(ccb);
		break;

	case XPT_SET_TRAN_SETTINGS:
		twa_dbg_dprint(3, sc, "XPT_SET_TRAN_SETTINGS");

		/*
		 * This command is not supported, since it's very specific
		 * to SCSI, and we are doing ATA.
		 */
  		ccb_h->status = CAM_FUNC_NOTAVAIL;
  		xpt_done(ccb);
  		break;

	case XPT_GET_TRAN_SETTINGS: 
	{
		struct ccb_trans_settings	*cts = &ccb->cts;

		twa_dbg_dprint(3, sc, "XPT_GET_TRAN_SETTINGS");
		cts->valid = (CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID);
		cts->flags &= ~(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry	*geom;

		twa_dbg_dprint(3, sc, "XPT_CALC_GEOMETRY request");
		geom = &ccb->ccg;

		if (geom->volume_size > 0x200000) /* 1 GB */ {
			geom->heads = 255;
			geom->secs_per_track = 63;
		} else {
			geom->heads = 64;
			geom->secs_per_track = 32;
		}
		geom->cylinders = geom->volume_size /
					(geom->heads * geom->secs_per_track);
		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_PATH_INQ:    /* Path inquiry -- get twa properties */
	{
		struct ccb_pathinq	*path_inq = &ccb->cpi;

		twa_dbg_dprint(3, sc, "XPT_PATH_INQ request");

		path_inq->version_num = 1;
		path_inq->hba_inquiry = 0;
		path_inq->target_sprt = 0;
		path_inq->hba_misc = 0;
		path_inq->hba_eng_cnt = 0;
		path_inq->max_target = TWA_MAX_UNITS;
		path_inq->max_lun = 0;
		path_inq->unit_number = cam_sim_unit(sim);
		path_inq->bus_id = cam_sim_bus(sim);
		path_inq->initiator_id = 12;
		path_inq->base_transfer_speed = 100000;
		strncpy(path_inq->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(path_inq->hba_vid, "3ware", HBA_IDLEN);
		strncpy(path_inq->dev_name, cam_sim_name(sim), DEV_IDLEN);
		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	default:
		twa_dbg_dprint(3, sc, "func_code = %x", ccb_h->func_code);
		ccb_h->status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}



/*
 * Function name:	twa_poll
 * Description:		Driver entry point called when interrupts are not available.
 *
 * Input:		sim	-- sim corresponding to the controller
 * Output:		None
 * Return value:	None
 */
void
twa_poll(struct cam_sim *sim)
{
#ifdef TWA_DEBUG
	struct twa_softc *sc = (struct twa_softc *)cam_sim_softc(sim);
#endif /* TWA_DEBUG */

	twa_dbg_dprint(3, sc, "Entering sc = %p", sc);
	twa_interrupt(cam_sim_softc(sim));
	twa_dbg_dprint(3, sc, "Exiting sc = %p", sc);
}



/*
 * Function name:	twa_async
 * Description:		Driver entry point for CAM to notify driver of special
 *			events.  We don't use this for now.
 *
 * Input:		callback_arg	-- ptr to per ctlr structure
 *			code		-- code associated with the event
 *			path		-- cam path
 *			arg		-- 
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
void
twa_async(void *callback_arg, u_int32_t code, 
			struct cam_path *path, void *arg)
{
#ifdef TWA_DEBUG
	struct twa_softc *sc = (struct twa_softc *)callback_arg;
#endif /* TWA_DEBUG */

	twa_dbg_dprint(3, sc, "sc = %p, code = %x, path = %p, arg = %p",
				sc, code, path, arg);
}



/*
 * Function name:	twa_request_bus_scan
 * Description:		Requests CAM for a scan of the bus.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_request_bus_scan(struct twa_softc *sc)
{
	struct cam_path	*path;
	union ccb	*ccb;

	if ((ccb = malloc(sizeof(union ccb), M_TEMP, M_WAITOK)) == NULL)
		return;
	bzero(ccb, sizeof(union ccb));
	if (xpt_create_path(&path, xpt_periph, cam_sim_path(sc->twa_sim),
			CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP)
		return;

	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = twa_bus_scan_cb;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);
}



/*
 * Function name:	twa_bus_scan_cb
 * Description:		Callback from CAM on a bus scan request.
 *
 * Input:		periph	-- we don't use this
 *			ccb	-- bus scan request ccb that we sent to CAM
 * Output:		None
 * Return value:	None
 */
static void
twa_bus_scan_cb(struct cam_periph *periph, union ccb *ccb)
{
	twa_dbg_print(3, "ccb = %p\n", ccb);
	if (ccb->ccb_h.status != CAM_REQ_CMP)
		printf("cam_scan_callback: failure status = %x\n",
					ccb->ccb_h.status);
	else
		twa_dbg_print(3, "success");

	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_TEMP);
}



/*
 * Function name:	twa_scsi_complete
 * Description:		Called to complete CAM scsi requests.
 *
 * Input:		tr	-- ptr to request pkt to be completed
 * Output:		None
 * Return value:	None
 */
void
twa_scsi_complete(struct twa_request *tr)
{
	struct twa_softc		*sc = tr->tr_sc;
	struct twa_command_header	*cmd_hdr = &(tr->tr_command->cmd_hdr);
	struct twa_command_9k		*cmd = &(tr->tr_command->command.cmd_pkt_9k);
	union ccb			*ccb = (union ccb *)(tr->tr_private);
	u_int16_t			error;
	u_int8_t			*cdb;

	if (tr->tr_error) {
		if (tr->tr_error == EBUSY)
			ccb->ccb_h.status |= CAM_REQUEUE_REQ;
		else if (tr->tr_error == EFBIG)
			ccb->ccb_h.status = CAM_REQ_TOO_BIG;
		else
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
	} else {
		if (cmd->status) {
			twa_dbg_dprint(1, sc, "req_id = 0x%x, status = 0x%x",
						cmd->request_id,
						cmd->status);

			error = cmd_hdr->status_block.error;
			if ((error == TWA_ERROR_LOGICAL_UNIT_NOT_SUPPORTED) ||
					(error == TWA_ERROR_UNIT_OFFLINE)) {
				twa_dbg_dprint(3, sc, "Unsupported unit. PTL = %x %x %x",
							ccb->ccb_h.path_id,
							ccb->ccb_h.target_id,
							ccb->ccb_h.target_lun);
				ccb->ccb_h.status |= CAM_TID_INVALID;
			} else {
				twa_dbg_dprint(2, sc, "cmd = %x %x %x %x %x %x %x",
						cmd->command.opcode,
						cmd->command.reserved,
						cmd->unit,
						cmd->request_id,
						cmd->status,
						cmd->sgl_offset,
						cmd->sgl_entries);

				cdb = (u_int8_t *)(cmd->cdb);
				twa_dbg_dprint(2, sc, "cdb = %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
					cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
					cdb[8], cdb[9], cdb[10], cdb[11], cdb[12], cdb[13], cdb[14], cdb[15]);

				cmd_hdr->err_specific_desc[sizeof(cmd_hdr->err_specific_desc) - 1] = '\0';
				/* 
				 * Print the error. Firmware doesn't yet support
				 * the 'Mode Sense' cmd.  Don't print if the cmd
				 * is 'Mode Sense', and the error is 'Invalid field
				 * in CDB'.
				 */
				if (! ((cdb[0] == 0x1A) && (error == 0x10D)))
					twa_printf(sc, "SCSI cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
						cdb[0],
						TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
						error,
						twa_find_msg_string(twa_error_table, error),
						cmd_hdr->err_specific_desc);
			}

			bcopy(cmd_hdr->sense_data, &(ccb->csio.sense_data),
						TWA_SENSE_DATA_LENGTH);
			ccb->csio.sense_len = TWA_SENSE_DATA_LENGTH;
			ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR | CAM_AUTOSNS_VALID;
		} else
			ccb->ccb_h.status = CAM_REQ_CMP;

		ccb->csio.scsi_status = cmd->status;
		/* If simq is frozen, unfreeze it. */
		if (sc->twa_state & TWA_STATE_SIMQ_FROZEN)
			twa_allow_new_requests(sc, (void *)ccb);
	}

	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	xpt_done(ccb);
}



/*
 * Function name:	twa_drain_busy_queue
 * Description:		This function gets called after a controller reset.
 *			It errors back to CAM, all those requests that were
 *			pending with the firmware, at the time of the reset.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_drain_busy_queue(struct twa_softc *sc)
{
	struct twa_request	*tr;
	union ccb		*ccb;

	/* Walk the busy queue. */
	while ((tr = twa_dequeue_busy(sc))) {
		twa_unmap_request(tr);
		if ((tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_INTERNAL) ||
			(tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_IOCTL)) {
			/* It's an internal/ioctl request.  Simply free it. */
			if (tr->tr_data)
				free(tr->tr_data, M_DEVBUF);
		} else {
			if ((ccb = tr->tr_private)) {
				/* It's a SCSI request.  Complete it. */
				ccb->ccb_h.status = CAM_SCSI_BUS_RESET |
							CAM_RELEASE_SIMQ;
				ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
				xpt_done(ccb);
			}
		}
		twa_release_request(tr);
	}
}



/*
 * Function name:	twa_allow_new_requests
 * Description:		Sets the appropriate status bits in a ccb such that,
 *			when the ccb is completed by a call to xpt_done,
 *			CAM knows that it's ok to unfreeze the flow of new
 *			requests to this controller, if the flow is frozen.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			ccb	-- ptr to CAM request
 * Output:		None
 * Return value:	None
 */
void
twa_allow_new_requests(struct twa_softc *sc, void *ccb)
{
	((union ccb *)(ccb))->ccb_h.status |= CAM_RELEASE_SIMQ;
	sc->twa_state &= ~TWA_STATE_SIMQ_FROZEN;
}



/*
 * Function name:	twa_disallow_new_requests
 * Description:		Calls the appropriate CAM function, so as to freeze
 *			the flow of new requests from CAM to this controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_disallow_new_requests(struct twa_softc *sc)
{
	xpt_freeze_simq(sc->twa_sim, 1);
	sc->twa_state |= TWA_STATE_SIMQ_FROZEN;
}
