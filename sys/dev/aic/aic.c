/*-
 * Copyright (c) 1999 Luoqi Chen.
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
__FBSDID("$FreeBSD: src/sys/dev/aic/aic.c,v 1.27.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_message.h>

#include <dev/aic/aic6360reg.h>
#include <dev/aic/aicvar.h>

static void aic_action(struct cam_sim *sim, union ccb *ccb);
static void aic_execute_scb(void *arg, bus_dma_segment_t *dm_segs,
				int nseg, int error);
static void aic_start(struct aic_softc *aic);
static void aic_select(struct aic_softc *aic);
static void aic_selected(struct aic_softc *aic);
static void aic_reselected(struct aic_softc *aic);
static void aic_reconnect(struct aic_softc *aic, int tag);
static void aic_cmd(struct aic_softc *aic);
static void aic_msgin(struct aic_softc *aic);
static void aic_handle_msgin(struct aic_softc *aic);
static void aic_msgout(struct aic_softc *aic);
static void aic_datain(struct aic_softc *aic);
static void aic_dataout(struct aic_softc *aic);
static void aic_done(struct aic_softc *aic, struct aic_scb *scb);
static void aic_poll(struct cam_sim *sim);
static void aic_timeout(void *arg);
static void aic_scsi_reset(struct aic_softc *aic);
static void aic_chip_reset(struct aic_softc *aic);
static void aic_reset(struct aic_softc *aic, int initiate_reset);

devclass_t aic_devclass;

static struct aic_scb *free_scbs;

static struct aic_scb *
aic_get_scb(struct aic_softc *aic)
{
	struct aic_scb *scb;
	int s = splcam();
	if ((scb = free_scbs) != NULL)
		free_scbs = (struct aic_scb *)free_scbs->ccb;
	splx(s);
	return (scb);
}

static void
aic_free_scb(struct aic_softc *aic, struct aic_scb *scb)
{
	int s = splcam();
	if ((aic->flags & AIC_RESOURCE_SHORTAGE) != 0 &&
	    (scb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		scb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		aic->flags &= ~AIC_RESOURCE_SHORTAGE;
	}
	scb->flags = 0;
	scb->ccb = (union ccb *)free_scbs;
	free_scbs = scb;
	splx(s);
}

static void
aic_action(struct cam_sim *sim, union ccb *ccb)
{
	struct aic_softc *aic;
	int s;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("aic_action\n"));

	aic = (struct aic_softc *)cam_sim_softc(sim);

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
        {               
		struct aic_scb *scb;

		if ((scb = aic_get_scb(aic)) == NULL) {
			s = splcam();
			aic->flags |= AIC_RESOURCE_SHORTAGE;
			splx(s);
			xpt_freeze_simq(aic->sim, /*count*/1);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			return;
		}

		scb->ccb = ccb;
		ccb->ccb_h.ccb_scb_ptr = scb;
		ccb->ccb_h.ccb_aic_ptr = aic;

		scb->target = ccb->ccb_h.target_id;
		scb->lun = ccb->ccb_h.target_lun;

		if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
			scb->cmd_len = ccb->csio.cdb_len;
			if (ccb->ccb_h.flags & CAM_CDB_POINTER) {
				if (ccb->ccb_h.flags & CAM_CDB_PHYS) {
					ccb->ccb_h.status = CAM_REQ_INVALID;
					aic_free_scb(aic, scb);
					xpt_done(ccb);
					return;
				}
				scb->cmd_ptr = ccb->csio.cdb_io.cdb_ptr;
			} else {
				scb->cmd_ptr = ccb->csio.cdb_io.cdb_bytes;
			}
			if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
				if ((ccb->ccb_h.flags & CAM_SCATTER_VALID) ||
				    (ccb->ccb_h.flags & CAM_DATA_PHYS)) {
					ccb->ccb_h.status = CAM_REQ_INVALID;
					aic_free_scb(aic, scb);
					xpt_done(ccb);
					return;
				}
				scb->data_ptr = ccb->csio.data_ptr;
				scb->data_len = ccb->csio.dxfer_len;
			} else {
				scb->data_ptr = NULL;
				scb->data_len = 0;
			}
			aic_execute_scb(scb, NULL, 0, 0);
		} else {
			scb->flags |= SCB_DEVICE_RESET;
			aic_execute_scb(scb, NULL, 0, 0);
		}
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = cts = &ccb->cts;
		struct aic_tinfo *ti = &aic->tinfo[ccb->ccb_h.target_id];
		struct ccb_trans_settings_scsi *scsi =
		    &cts->proto_specific.scsi;
		struct ccb_trans_settings_spi *spi =
		    &cts->xport_specific.spi;

		s = splcam();

		if ((spi->valid & CTS_SPI_VALID_DISC) != 0 &&
		    (aic->flags & AIC_DISC_ENABLE) != 0) {
			if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) != 0)
				ti->flags |= TINFO_DISC_ENB;
			else
				ti->flags &= ~TINFO_DISC_ENB;
		}

		if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
			if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
				ti->flags |= TINFO_TAG_ENB;
			else
				ti->flags &= ~TINFO_TAG_ENB;
		}

		if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) != 0) {
			ti->goal.period = spi->sync_period;

			if (ti->goal.period > aic->min_period) {
				ti->goal.period = 0;
				ti->goal.offset = 0;
			} else if (ti->goal.period < aic->max_period)
				ti->goal.period = aic->max_period;
		}

		if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0) {
			ti->goal.offset = spi->sync_offset;
			if (ti->goal.offset == 0)
				ti->goal.period = 0;
			else if (ti->goal.offset > AIC_SYNC_OFFSET)
				ti->goal.offset = AIC_SYNC_OFFSET;
		}

		if ((ti->goal.period != ti->current.period)
		 || (ti->goal.offset != ti->current.offset))
			ti->flags |= TINFO_SDTR_NEGO;

		splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = &ccb->cts;
		struct aic_tinfo *ti = &aic->tinfo[ccb->ccb_h.target_id];
		struct ccb_trans_settings_scsi *scsi =
		    &cts->proto_specific.scsi;
		struct ccb_trans_settings_spi *spi =
		    &cts->xport_specific.spi;

		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_2;
		cts->transport = XPORT_SPI;
		cts->transport_version = 2;
		scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
		spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;

		s = splcam();
		if ((ti->flags & TINFO_DISC_ENB) != 0)
			spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
		if ((ti->flags & TINFO_TAG_ENB) != 0)
			scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;

		if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			spi->sync_period = ti->current.period;
			spi->sync_offset = ti->current.offset;
		} else {
			spi->sync_period = ti->user.period;
			spi->sync_offset = ti->user.offset;
		}
		splx(s);

		spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		spi->valid = CTS_SPI_VALID_SYNC_RATE
			   | CTS_SPI_VALID_SYNC_OFFSET
			   | CTS_SPI_VALID_BUS_WIDTH
			   | CTS_SPI_VALID_DISC;
		scsi->valid = CTS_SCSI_VALID_TQ;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
		aic_reset(aic, /*initiate_reset*/TRUE);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
        case XPT_PATH_INQ:              /* Path routing inquiry */
        {       
                struct ccb_pathinq *cpi = &ccb->cpi;

                cpi->version_num = 1; /* XXX??? */
                cpi->hba_inquiry = PI_SDTR_ABLE | PI_TAG_ABLE;
                cpi->target_sprt = 0;
                cpi->hba_misc = 0;
                cpi->hba_eng_cnt = 0;
                cpi->max_target = 7;
                cpi->max_lun = 7;
                cpi->initiator_id = aic->initiator;
                cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
                strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
                strncpy(cpi->hba_vid, "Adaptec", HBA_IDLEN);
                strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
                cpi->unit_number = cam_sim_unit(sim);
                cpi->transport = XPORT_SPI;
                cpi->transport_version = 2;
                cpi->protocol = PROTO_SCSI;
                cpi->protocol_version = SCSI_REV_2;
                cpi->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
                break;
        }
	default:
                ccb->ccb_h.status = CAM_REQ_INVALID;
                xpt_done(ccb);
                break;
	}
}

static void
aic_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct aic_scb *scb = (struct aic_scb *)arg;
	union ccb *ccb = scb->ccb;
	struct aic_softc *aic = (struct aic_softc *)ccb->ccb_h.ccb_aic_ptr;
	int s;

	s = splcam();

	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		splx(s);
		aic_free_scb(aic, scb);
		xpt_done(ccb);
		return;
	}

	scb->flags |= SCB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	TAILQ_INSERT_TAIL(&aic->pending_ccbs, &ccb->ccb_h, sim_links.tqe);

	ccb->ccb_h.timeout_ch = timeout(aic_timeout, (caddr_t)scb,
		(ccb->ccb_h.timeout * hz) / 1000);

	aic_start(aic);
	splx(s);
}

/*
 * Start another command if the controller is not busy.
 */
static void
aic_start(struct aic_softc *aic)
{
	struct ccb_hdr *ccb_h;
	struct aic_tinfo *ti;

	if (aic->state != AIC_IDLE)
		return;

	TAILQ_FOREACH(ccb_h, &aic->pending_ccbs, sim_links.tqe) {
		ti = &aic->tinfo[ccb_h->target_id];
		if ((ti->lubusy & (1 << ccb_h->target_lun)) == 0) {
			TAILQ_REMOVE(&aic->pending_ccbs, ccb_h, sim_links.tqe);
			aic->nexus = (struct aic_scb *)ccb_h->ccb_scb_ptr;
			aic_select(aic);
			return;
		}
	}

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_start: idle\n"));

	aic_outb(aic, SIMODE0, ENSELDI);
	aic_outb(aic, SIMODE1, ENSCSIRST);
	aic_outb(aic, SCSISEQ, ENRESELI);
}

/*
 * Start a selection.
 */
static void
aic_select(struct aic_softc *aic)
{
	struct aic_scb *scb = aic->nexus;

	CAM_DEBUG(scb->ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("aic_select - ccb %p\n", scb->ccb));

	aic->state = AIC_SELECTING;

	aic_outb(aic, DMACNTRL1, 0);
	aic_outb(aic, SCSIID, aic->initiator << OID_S | scb->target);
	aic_outb(aic, SXFRCTL1, STIMO_256ms | ENSTIMER |
	    (aic->flags & AIC_PARITY_ENABLE ? ENSPCHK : 0));

	aic_outb(aic, SIMODE0, ENSELDI|ENSELDO);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENSELTIMO);
	aic_outb(aic, SCSISEQ, ENRESELI|ENSELO|ENAUTOATNO);
}

/*
 * We have successfully selected a target, prepare for the information
 * transfer phases.
 */
static void
aic_selected(struct aic_softc *aic)
{
	struct aic_scb *scb = aic->nexus;
	union ccb *ccb = scb->ccb;
	struct aic_tinfo *ti = &aic->tinfo[scb->target];

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("aic_selected - ccb %p\n", ccb));

	aic->state = AIC_HASNEXUS;

	if (scb->flags & SCB_DEVICE_RESET) {
		aic->msg_buf[0] = MSG_BUS_DEV_RESET;
		aic->msg_len = 1;
		aic->msg_outq = AIC_MSG_MSGBUF;
	} else {
		aic->msg_outq = AIC_MSG_IDENTIFY;
		if ((ti->flags & TINFO_TAG_ENB) != 0 &&
		    (ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0)
			aic->msg_outq |= AIC_MSG_TAG_Q;
		else
			ti->lubusy |= 1 << scb->lun;
		if ((ti->flags & TINFO_SDTR_NEGO) != 0)
			aic->msg_outq |= AIC_MSG_SDTR;
	}

	aic_outb(aic, CLRSINT0, CLRSELDO);
	aic_outb(aic, CLRSINT1, CLRBUSFREE);
	aic_outb(aic, SCSISEQ, ENAUTOATNP);
	aic_outb(aic, SIMODE0, 0);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
	aic_outb(aic, SCSIRATE, ti->scsirate);
}

/*
 * We are re-selected by a target, save the target id and wait for the
 * target to further identify itself.
 */
static void
aic_reselected(struct aic_softc *aic)
{
	u_int8_t selid;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_reselected\n"));

	/*
	 * If we have started a selection, it must have lost out in
	 * the arbitration, put the command back to the pending queue.
	 */
	if (aic->nexus) {
		TAILQ_INSERT_HEAD(&aic->pending_ccbs,
		    &aic->nexus->ccb->ccb_h, sim_links.tqe);
		aic->nexus = NULL;
	}

	selid = aic_inb(aic, SELID) & ~(1 << aic->initiator);
	if (selid & (selid - 1)) {
		/* this should never have happened */
		printf("aic_reselected: invalid selid %x\n", selid);
		aic_reset(aic, /*initiate_reset*/TRUE);
		return;
	}

	aic->state = AIC_RESELECTED;
	aic->target = ffs(selid) - 1;
	aic->lun = -1;

	aic_outb(aic, CLRSINT0, CLRSELDI);
	aic_outb(aic, CLRSINT1, CLRBUSFREE);
	aic_outb(aic, SIMODE0, 0);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
	aic_outb(aic, SCSISEQ, ENAUTOATNP);
	aic_outb(aic, SCSIRATE, aic->tinfo[aic->target].scsirate);
}

/*
 * Raise ATNO to signal the target that we have a message for it.
 */
static __inline void
aic_sched_msgout(struct aic_softc *aic, u_int8_t msg)
{
	if (msg) {
		aic->msg_buf[0] = msg;
		aic->msg_len = 1;
	}
	aic->msg_outq |= AIC_MSG_MSGBUF;
	aic_outb(aic, SCSISIGO, aic_inb(aic, SCSISIGI) | ATNO);
}

/*
 * Wait for SPIORDY (SCSI PIO ready) flag, or a phase change.
 */
static __inline int
aic_spiordy(struct aic_softc *aic)
{
	while (!(aic_inb(aic, DMASTAT) & INTSTAT) &&
	    !(aic_inb(aic, SSTAT0) & SPIORDY))
		;
	return !(aic_inb(aic, DMASTAT) & INTSTAT);
}

/*
 * Reestablish a disconnected nexus.
 */
static void
aic_reconnect(struct aic_softc *aic, int tag)
{
	struct aic_scb *scb;
	struct ccb_hdr *ccb_h;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_reconnect\n"));

	/* Find the nexus */
	scb = NULL;
	TAILQ_FOREACH(ccb_h, &aic->nexus_ccbs, sim_links.tqe) {
		scb = (struct aic_scb *)ccb_h->ccb_scb_ptr;
		if (scb->target == aic->target && scb->lun == aic->lun &&
		    (tag == -1 || scb->tag == tag))
			break;
	}

	/* ABORT if nothing is found */
	if (!ccb_h) {
		if (tag == -1)
			aic_sched_msgout(aic, MSG_ABORT);
		else
			aic_sched_msgout(aic, MSG_ABORT_TAG);
		xpt_async(AC_UNSOL_RESEL, aic->path, NULL);
		return;
	}

	/* Reestablish the nexus */
	TAILQ_REMOVE(&aic->nexus_ccbs, ccb_h, sim_links.tqe);
	aic->nexus = scb;
	scb->flags &= ~SCB_DISCONNECTED;
	aic->state = AIC_HASNEXUS;
}

/*
 * Read messages.
 */
static void
aic_msgin(struct aic_softc *aic)
{
	int msglen;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_msgin\n"));

	aic_outb(aic, SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE);
	aic_outb(aic, SXFRCTL0, CHEN|SPIOEN);

	aic->flags &= ~AIC_DROP_MSGIN;
	aic->msg_len = 0;
	do {
		/*
		 * If a parity error is detected, drop the remaining
		 * bytes and inform the target so it could resend
		 * the messages.
		 */
		if (aic_inb(aic, SSTAT1) & SCSIPERR) {
			aic_outb(aic, CLRSINT1, CLRSCSIPERR);
			aic->flags |= AIC_DROP_MSGIN;
			aic_sched_msgout(aic, MSG_PARITY_ERROR);
		}
		if ((aic->flags & AIC_DROP_MSGIN)) {
			aic_inb(aic, SCSIDAT);
			continue;
		}
		/* read the message byte without ACKing on it */
		aic->msg_buf[aic->msg_len++] = aic_inb(aic, SCSIBUS);
		if (aic->msg_buf[0] == MSG_EXTENDED) {
			if (aic->msg_len < 2) {
				(void) aic_inb(aic, SCSIDAT);
				continue;
			}
			switch (aic->msg_buf[2]) {
			case MSG_EXT_SDTR:
				msglen = MSG_EXT_SDTR_LEN;
				break;
			case MSG_EXT_WDTR:
				msglen = MSG_EXT_WDTR_LEN;
				break;
			default:
				msglen = 0;
				break;
			}
			if (aic->msg_buf[1] != msglen) {
				aic->flags |= AIC_DROP_MSGIN;
				aic_sched_msgout(aic, MSG_MESSAGE_REJECT);
			}
			msglen += 2;
		} else if (aic->msg_buf[0] >= 0x20 && aic->msg_buf[0] <= 0x2f)
			msglen = 2;
		else
			msglen = 1;
		/*
		 * If we have a complete message, handle it before the final
		 * ACK (in case we decide to reject the message).
		 */
		if (aic->msg_len == msglen) {
			aic_handle_msgin(aic);
			aic->msg_len = 0;
		}
		/* ACK on the message byte */
		(void) aic_inb(aic, SCSIDAT);
	} while (aic_spiordy(aic));

	aic_outb(aic, SXFRCTL0, CHEN);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
}

/*
 * Handle a message.
 */
static void
aic_handle_msgin(struct aic_softc *aic)
{
	struct aic_scb *scb;
	struct ccb_hdr *ccb_h;
	struct aic_tinfo *ti;
	struct ccb_trans_settings neg;
	struct ccb_trans_settings_spi *spi = &neg.xport_specific.spi;

	if (aic->state == AIC_RESELECTED) {
		if (!MSG_ISIDENTIFY(aic->msg_buf[0])) {
			aic_sched_msgout(aic, MSG_MESSAGE_REJECT);
			return;
		}
		aic->lun = aic->msg_buf[0] & MSG_IDENTIFY_LUNMASK;
		if (aic->tinfo[aic->target].lubusy & (1 << aic->lun))
			aic_reconnect(aic, -1);
		else
			aic->state = AIC_RECONNECTING;
		return;
	}

	if (aic->state == AIC_RECONNECTING) {
		if (aic->msg_buf[0] != MSG_SIMPLE_Q_TAG) {
			aic_sched_msgout(aic, MSG_MESSAGE_REJECT);
			return;
		}
		aic_reconnect(aic, aic->msg_buf[1]);
		return;
	}

	switch (aic->msg_buf[0]) {
	case MSG_CMDCOMPLETE: {
		struct ccb_scsiio *csio;
		scb = aic->nexus;
		ccb_h = &scb->ccb->ccb_h;
		csio = &scb->ccb->csio;
		if ((scb->flags & SCB_SENSE) != 0) {
			/* auto REQUEST SENSE command */
			scb->flags &= ~SCB_SENSE;
			csio->sense_resid = scb->data_len;
			if (scb->status == SCSI_STATUS_OK) {
				ccb_h->status |=
				    CAM_SCSI_STATUS_ERROR|CAM_AUTOSNS_VALID;
				/*scsi_sense_print(csio);*/
			} else {
				ccb_h->status |= CAM_AUTOSENSE_FAIL;
				printf("ccb %p sense failed %x\n",
				    ccb_h, scb->status);
			}
		} else {
			csio->scsi_status = scb->status;
			csio->resid = scb->data_len;
			if (scb->status == SCSI_STATUS_OK) {
				/* everything goes well */
				ccb_h->status |= CAM_REQ_CMP;
			} else if ((ccb_h->flags & CAM_DIS_AUTOSENSE) == 0 &&
			    (csio->scsi_status == SCSI_STATUS_CHECK_COND ||
			     csio->scsi_status == SCSI_STATUS_CMD_TERMINATED)) {
				/* try to retrieve sense information */
				scb->flags |= SCB_SENSE;
				aic->flags |= AIC_BUSFREE_OK;
				return;
			} else
				ccb_h->status |= CAM_SCSI_STATUS_ERROR;
		}
		aic_done(aic, scb);
		aic->flags |= AIC_BUSFREE_OK;
		break;
	}
	case MSG_EXTENDED:
		switch (aic->msg_buf[2]) {
		case MSG_EXT_SDTR:
			scb = aic->nexus;
			ti = &aic->tinfo[scb->target];
			if (ti->flags & TINFO_SDTR_SENT) {
				ti->current.period = aic->msg_buf[3];
				ti->current.offset = aic->msg_buf[4];
			} else {
				ti->current.period = aic->msg_buf[3] =
					max(ti->goal.period, aic->msg_buf[3]);
				ti->current.offset = aic->msg_buf[4] =
					min(ti->goal.offset, aic->msg_buf[4]);
				/*
				 * The target initiated the negotiation,
				 * send back a response.
				 */
				aic_sched_msgout(aic, 0);
			}
			ti->flags &= ~(TINFO_SDTR_SENT|TINFO_SDTR_NEGO);
			ti->scsirate = ti->current.offset ? ti->current.offset |
			    ((ti->current.period * 4 + 49) / 50 - 2) << 4 : 0;
			aic_outb(aic, SCSIRATE, ti->scsirate);
			memset(&neg, 0, sizeof (neg));
			neg.protocol = PROTO_SCSI;
			neg.protocol_version = SCSI_REV_2;
			neg.transport = XPORT_SPI;
			neg.transport_version = 2;
			spi->sync_period = ti->goal.period = ti->current.period;
			spi->sync_offset = ti->goal.offset = ti->current.offset;
			spi->valid = CTS_SPI_VALID_SYNC_RATE
				  | CTS_SPI_VALID_SYNC_OFFSET;
			ccb_h = &scb->ccb->ccb_h;
			xpt_setup_ccb(&neg.ccb_h, ccb_h->path, 1);
			xpt_async(AC_TRANSFER_NEG, ccb_h->path, &neg);
			break;
		case MSG_EXT_WDTR:
		default:
			aic_sched_msgout(aic, MSG_MESSAGE_REJECT);
			break;
		}
		break;
	case MSG_DISCONNECT:
		scb = aic->nexus;
		ccb_h = &scb->ccb->ccb_h;
		TAILQ_INSERT_TAIL(&aic->nexus_ccbs, ccb_h, sim_links.tqe);
		scb->flags |= SCB_DISCONNECTED;
		aic->flags |= AIC_BUSFREE_OK;
		aic->nexus = NULL;
		CAM_DEBUG(ccb_h->path, CAM_DEBUG_TRACE, ("disconnected\n"));
		break;
	case MSG_MESSAGE_REJECT:
		switch (aic->msg_outq & -aic->msg_outq) {
		case AIC_MSG_TAG_Q:
			scb = aic->nexus;
			ti = &aic->tinfo[scb->target];
			ti->flags &= ~TINFO_TAG_ENB;
			ti->lubusy |= 1 << scb->lun;
			break;
		case AIC_MSG_SDTR:
			scb = aic->nexus;
			ti = &aic->tinfo[scb->target];
			ti->current.period = ti->goal.period = 0;
			ti->current.offset = ti->goal.offset = 0;
			ti->flags &= ~(TINFO_SDTR_SENT|TINFO_SDTR_NEGO);
			ti->scsirate = 0;
			aic_outb(aic, SCSIRATE, ti->scsirate);
			memset(&neg, 0, sizeof (neg));
			neg.protocol = PROTO_SCSI;
			neg.protocol_version = SCSI_REV_2;
			neg.transport = XPORT_SPI;
			neg.transport_version = 2;
			spi->sync_period = ti->current.period;
			spi->sync_offset = ti->current.offset;
			spi->valid = CTS_SPI_VALID_SYNC_RATE
				  | CTS_SPI_VALID_SYNC_OFFSET;
			ccb_h = &scb->ccb->ccb_h;
			xpt_setup_ccb(&neg.ccb_h, ccb_h->path, 1);
			xpt_async(AC_TRANSFER_NEG, ccb_h->path, &neg);
			break;
		default:
			break;
		}
		break;
	case MSG_SAVEDATAPOINTER:
		break;  
	case MSG_RESTOREPOINTERS:
		break;
	case MSG_NOOP:
		break;
	default:
		aic_sched_msgout(aic, MSG_MESSAGE_REJECT);
		break;
	}
}

/*
 * Send messages.
 */
static void
aic_msgout(struct aic_softc *aic)
{
	struct aic_scb *scb;
	union ccb *ccb;
	struct aic_tinfo *ti;
	int msgidx = 0;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_msgout\n"));

	aic_outb(aic, SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE);
	aic_outb(aic, SXFRCTL0, CHEN|SPIOEN);

	/*
	 * If the previous phase is also the message out phase,
	 * we need to retransmit all the messages, probably
	 * because the target has detected a parity error during
	 * the past transmission.
	 */
	if (aic->prev_phase == PH_MSGOUT)
		aic->msg_outq = aic->msg_sent;

	do {
		int q = aic->msg_outq;
		if (msgidx > 0 && msgidx == aic->msg_len) {
			/* complete message sent, start the next one */
			q &= -q;
			aic->msg_sent |= q;
			aic->msg_outq ^= q;
			q = aic->msg_outq;
			msgidx = 0;
		}
		if (msgidx == 0) {
			/* setup the message */
			switch (q & -q) {
			case AIC_MSG_IDENTIFY:
				scb = aic->nexus;
				ccb = scb->ccb;
				ti = &aic->tinfo[scb->target];
				aic->msg_buf[0] = MSG_IDENTIFY(scb->lun,
				    (ti->flags & TINFO_DISC_ENB) &&
				    !(ccb->ccb_h.flags & CAM_DIS_DISCONNECT));
				aic->msg_len = 1;
				break;
			case AIC_MSG_TAG_Q:
				scb = aic->nexus;
				ccb = scb->ccb;
				aic->msg_buf[0] = ccb->csio.tag_action;
				aic->msg_buf[1] = scb->tag;
				aic->msg_len = 2;
				break;
			case AIC_MSG_SDTR:
				scb = aic->nexus;
				ti = &aic->tinfo[scb->target];
				aic->msg_buf[0] = MSG_EXTENDED;
				aic->msg_buf[1] = MSG_EXT_SDTR_LEN;
				aic->msg_buf[2] = MSG_EXT_SDTR;
				aic->msg_buf[3] = ti->goal.period;
				aic->msg_buf[4] = ti->goal.offset;
				aic->msg_len = MSG_EXT_SDTR_LEN + 2;
				ti->flags |= TINFO_SDTR_SENT;
				break;
			case AIC_MSG_MSGBUF:
				/* a single message already in the buffer */
				if (aic->msg_buf[0] == MSG_BUS_DEV_RESET ||
				    aic->msg_buf[0] == MSG_ABORT ||
				    aic->msg_buf[0] == MSG_ABORT_TAG)
					aic->flags |= AIC_BUSFREE_OK;
				break;
			}
		}
		/*
		 * If this is the last message byte of all messages,
		 * clear ATNO to signal transmission complete.
		 */
		if ((q & (q - 1)) == 0 && msgidx == aic->msg_len - 1)
			aic_outb(aic, CLRSINT1, CLRATNO);
		/* transmit the message byte */
		aic_outb(aic, SCSIDAT, aic->msg_buf[msgidx++]);
	} while (aic_spiordy(aic));

	aic_outb(aic, SXFRCTL0, CHEN);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
}

/*
 * Read data bytes.
 */
static void
aic_datain(struct aic_softc *aic)
{
	struct aic_scb *scb = aic->nexus;
	u_int8_t dmastat, dmacntrl0;
	int n;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_datain\n"));

	aic_outb(aic, SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE);
	aic_outb(aic, SXFRCTL0, SCSIEN|DMAEN|CHEN);

	dmacntrl0 = ENDMA;
	if (aic->flags & AIC_DWIO_ENABLE)
		dmacntrl0 |= DWORDPIO;
	aic_outb(aic, DMACNTRL0, dmacntrl0);

	while (scb->data_len > 0) {
		for (;;) {
			/* wait for the fifo to fill up or a phase change */
			dmastat = aic_inb(aic, DMASTAT);
			if (dmastat & (INTSTAT|DFIFOFULL))
				break;
		}
		if (dmastat & DFIFOFULL) {
			n = FIFOSIZE;
		} else {
			/*
			 * No more data, wait for the remaining bytes in
			 * the scsi fifo to be transfer to the host fifo.
			 */
			while (!(aic_inb(aic, SSTAT2) & SEMPTY))
				;
			n = aic_inb(aic, FIFOSTAT);
		}
		n = imin(scb->data_len, n);
		if (aic->flags & AIC_DWIO_ENABLE) {
			if (n >= 12) {
				aic_insl(aic, DMADATALONG, scb->data_ptr, n>>2);
				scb->data_ptr += n & ~3;
				scb->data_len -= n & ~3;
				n &= 3;
			}
		} else {
			if (n >= 8) {
				aic_insw(aic, DMADATA, scb->data_ptr, n >> 1);
				scb->data_ptr += n & ~1;
				scb->data_len -= n & ~1;
				n &= 1;
			}
		}
		if (n) {
			aic_outb(aic, DMACNTRL0, ENDMA|B8MODE);
			aic_insb(aic, DMADATA, scb->data_ptr, n);
			scb->data_ptr += n;
			scb->data_len -= n;
			aic_outb(aic, DMACNTRL0, dmacntrl0);
		}

		if (dmastat & INTSTAT)
			break;
	}

	aic_outb(aic, SXFRCTL0, CHEN);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
}

/*
 * Send data bytes.
 */
static void
aic_dataout(struct aic_softc *aic)
{
	struct aic_scb *scb = aic->nexus;
	u_int8_t dmastat, dmacntrl0, sstat2;
	int n;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_dataout\n"));

	aic_outb(aic, SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE);
	aic_outb(aic, SXFRCTL0, SCSIEN|DMAEN|CHEN);

	dmacntrl0 = ENDMA|WRITE;
	if (aic->flags & AIC_DWIO_ENABLE)
		dmacntrl0 |= DWORDPIO;
	aic_outb(aic, DMACNTRL0, dmacntrl0);

	while (scb->data_len > 0) {
		for (;;) {
			/* wait for the fifo to clear up or a phase change */
			dmastat = aic_inb(aic, DMASTAT);
			if (dmastat & (INTSTAT|DFIFOEMP))
				break;
		}
		if (dmastat & INTSTAT)
			break;
		n = imin(scb->data_len, FIFOSIZE);
		if (aic->flags & AIC_DWIO_ENABLE) {
			if (n >= 12) {
				aic_outsl(aic, DMADATALONG, scb->data_ptr,n>>2);
				scb->data_ptr += n & ~3;
				scb->data_len -= n & ~3;
				n &= 3;
			}
		} else {
			if (n >= 8) {
				aic_outsw(aic, DMADATA, scb->data_ptr, n >> 1);
				scb->data_ptr += n & ~1;
				scb->data_len -= n & ~1;
				n &= 1;
			}
		}
		if (n) {
			aic_outb(aic, DMACNTRL0, ENDMA|WRITE|B8MODE);
			aic_outsb(aic, DMADATA, scb->data_ptr, n);
			scb->data_ptr += n;
			scb->data_len -= n;
			aic_outb(aic, DMACNTRL0, dmacntrl0);
		}
	}

	for (;;) {
		/* wait until all bytes in the fifos are transmitted */
		dmastat = aic_inb(aic, DMASTAT);
		sstat2 = aic_inb(aic, SSTAT2);
		if ((dmastat & DFIFOEMP) && (sstat2 & SEMPTY))
			break;
		if (dmastat & INTSTAT) {
			/* adjust for untransmitted bytes */
			n = aic_inb(aic, FIFOSTAT) + (sstat2 & 0xf);
			scb->data_ptr -= n;
			scb->data_len += n;
			/* clear the fifo */
			aic_outb(aic, SXFRCTL0, CHEN|CLRCH);
			aic_outb(aic, DMACNTRL0, RSTFIFO);
			break;
		}
	}

	aic_outb(aic, SXFRCTL0, CHEN);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
}

/*
 * Send the scsi command.
 */
static void
aic_cmd(struct aic_softc *aic)
{
	struct aic_scb *scb = aic->nexus;
	struct scsi_request_sense sense_cmd;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_cmd\n"));

	if (scb->flags & SCB_SENSE) {
		/* autosense request */
		sense_cmd.opcode = REQUEST_SENSE;
		sense_cmd.byte2 = scb->lun << 5;
		sense_cmd.length = scb->ccb->csio.sense_len;
		sense_cmd.control = 0;
		sense_cmd.unused[0] = 0;
		sense_cmd.unused[1] = 0;
		scb->cmd_ptr = (u_int8_t *)&sense_cmd;
		scb->cmd_len = sizeof(sense_cmd);
		scb->data_ptr = (u_int8_t *)&scb->ccb->csio.sense_data;
		scb->data_len = scb->ccb->csio.sense_len;
	}

	aic_outb(aic, SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE);
	aic_outb(aic, DMACNTRL0, ENDMA|WRITE);
	aic_outb(aic, SXFRCTL0, SCSIEN|DMAEN|CHEN);
	aic_outsw(aic, DMADATA, (u_int16_t *)scb->cmd_ptr, scb->cmd_len >> 1);
	while ((aic_inb(aic, SSTAT2) & SEMPTY) == 0 &&
	    (aic_inb(aic, DMASTAT) & INTSTAT) == 0)
		;
	aic_outb(aic, SXFRCTL0, CHEN);
	aic_outb(aic, SIMODE1, ENSCSIRST|ENBUSFREE|ENREQINIT);
}

/*
 * Finish off a command. The caller is responsible to remove the ccb
 * from any queue.
 */
static void
aic_done(struct aic_softc *aic, struct aic_scb *scb)
{
	union ccb *ccb = scb->ccb;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("aic_done - ccb %p status %x resid %d\n",
		   ccb, ccb->ccb_h.status, ccb->csio.resid));

	untimeout(aic_timeout, (caddr_t)scb, ccb->ccb_h.timeout_ch);

	if ((scb->flags & SCB_DEVICE_RESET) != 0 &&
	    ccb->ccb_h.func_code != XPT_RESET_DEV) {
		struct cam_path *path;
		struct ccb_hdr *ccb_h;
		cam_status error;

		error = xpt_create_path(&path, /*periph*/NULL,
					cam_sim_path(aic->sim),
					scb->target,
					CAM_LUN_WILDCARD);

		if (error == CAM_REQ_CMP) {
			xpt_async(AC_SENT_BDR, path, NULL);
			xpt_free_path(path);
		}

		ccb_h = TAILQ_FIRST(&aic->pending_ccbs);
		while (ccb_h != NULL) {
			struct aic_scb *pending_scb;

			pending_scb = (struct aic_scb *)ccb_h->ccb_scb_ptr;
			if (ccb_h->target_id == scb->target) {
				ccb_h->status |= CAM_BDR_SENT;
				ccb_h = TAILQ_NEXT(ccb_h, sim_links.tqe);
				TAILQ_REMOVE(&aic->pending_ccbs,
				    &pending_scb->ccb->ccb_h, sim_links.tqe);
				aic_done(aic, pending_scb);
			} else {
				ccb_h->timeout_ch =
				    timeout(aic_timeout, (caddr_t)pending_scb,
					(ccb_h->timeout * hz) / 1000);
				ccb_h = TAILQ_NEXT(ccb_h, sim_links.tqe);
			}
		}

		ccb_h = TAILQ_FIRST(&aic->nexus_ccbs);
		while (ccb_h != NULL) {
			struct aic_scb *nexus_scb;

			nexus_scb = (struct aic_scb *)ccb_h->ccb_scb_ptr;
			if (ccb_h->target_id == scb->target) {
				ccb_h->status |= CAM_BDR_SENT;
				ccb_h = TAILQ_NEXT(ccb_h, sim_links.tqe);
				TAILQ_REMOVE(&aic->nexus_ccbs,
				    &nexus_scb->ccb->ccb_h, sim_links.tqe);
				aic_done(aic, nexus_scb);
			} else {
				ccb_h->timeout_ch =
				    timeout(aic_timeout, (caddr_t)nexus_scb,
					(ccb_h->timeout * hz) / 1000);
				ccb_h = TAILQ_NEXT(ccb_h, sim_links.tqe);
			}
		}
	}

	if (aic->nexus == scb || scb->flags & SCB_DISCONNECTED)
		aic->tinfo[scb->target].lubusy &= ~(1 << scb->lun);
	
	if (aic->nexus == scb) {
		aic->nexus = NULL;
	}
	aic_free_scb(aic, scb);
	xpt_done(ccb);
}

static void
aic_poll(struct cam_sim *sim)
{
	aic_intr(cam_sim_softc(sim));
}

static void
aic_timeout(void *arg)
{
	struct aic_scb *scb = (struct aic_scb *)arg;
	union ccb *ccb = scb->ccb;
	struct aic_softc *aic = (struct aic_softc *)ccb->ccb_h.ccb_aic_ptr;
	int s;

	xpt_print_path(ccb->ccb_h.path);
	printf("ccb %p - timed out", ccb);
	if (aic->nexus && aic->nexus != scb)
		printf(", nexus %p", aic->nexus->ccb);
	printf(", phase 0x%x, state %d\n", aic_inb(aic, SCSISIGI), aic->state);

	s = splcam();

	if ((scb->flags & SCB_ACTIVE) == 0) {
		splx(s);
		xpt_print_path(ccb->ccb_h.path);
		printf("ccb %p - timed out already completed\n", ccb);
		return;
	}

	if ((scb->flags & SCB_DEVICE_RESET) == 0 && aic->nexus == scb) {
		struct ccb_hdr *ccb_h = &scb->ccb->ccb_h;

		if ((ccb_h->status & CAM_RELEASE_SIMQ) == 0) {
			xpt_freeze_simq(aic->sim, /*count*/1);
			ccb_h->status |= CAM_RELEASE_SIMQ;
		}

		TAILQ_FOREACH(ccb_h, &aic->pending_ccbs, sim_links.tqe) {
			untimeout(aic_timeout, (caddr_t)ccb_h->ccb_scb_ptr,
			    ccb_h->timeout_ch);
		}

		TAILQ_FOREACH(ccb_h, &aic->nexus_ccbs, sim_links.tqe) {
			untimeout(aic_timeout, (caddr_t)ccb_h->ccb_scb_ptr,
			    ccb_h->timeout_ch);
		}

		scb->flags |= SCB_DEVICE_RESET;
		ccb->ccb_h.timeout_ch =
		    timeout(aic_timeout, (caddr_t)scb, 5 * hz);
		aic_sched_msgout(aic, MSG_BUS_DEV_RESET);
	} else {
		if (aic->nexus == scb) {
			ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
			aic_done(aic, scb);
		}
		aic_reset(aic, /*initiate_reset*/TRUE);
	}

	splx(s);
}

void
aic_intr(void *arg)
{
	struct aic_softc *aic = (struct aic_softc *)arg;
	u_int8_t sstat0, sstat1;
	union ccb *ccb;
	struct aic_scb *scb;

	if (!(aic_inb(aic, DMASTAT) & INTSTAT))
		return;

	aic_outb(aic, DMACNTRL0, 0);

	sstat0 = aic_inb(aic, SSTAT0);
	sstat1 = aic_inb(aic, SSTAT1);

	if ((sstat1 & SCSIRSTI) != 0) {
		/* a device-initiated bus reset */
		aic_outb(aic, CLRSINT1, CLRSCSIRSTI);
		aic_reset(aic, /*initiate_reset*/FALSE);
		return;
	}

	if ((sstat1 & SCSIPERR) != 0) {
		aic_outb(aic, CLRSINT1, CLRSCSIPERR);
		aic_sched_msgout(aic, MSG_PARITY_ERROR);
		aic_outb(aic, DMACNTRL0, INTEN);
		return;
	}

	if (aic_inb(aic, SSTAT4)) {
		aic_outb(aic, CLRSERR, CLRSYNCERR|CLRFWERR|CLRFRERR);
		aic_reset(aic, /*initiate_reset*/TRUE);
		return;
	}

	if (aic->state <= AIC_SELECTING) {
		if ((sstat0 & SELDI) != 0) {
			aic_reselected(aic);
			aic_outb(aic, DMACNTRL0, INTEN);
			return;
		}

		if ((sstat0 & SELDO) != 0) {
			aic_selected(aic);
			aic_outb(aic, DMACNTRL0, INTEN);
			return;
		}

		if ((sstat1 & SELTO) != 0) {
			scb = aic->nexus;
			ccb = scb->ccb;
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			aic_done(aic, scb);
			while ((sstat1 & BUSFREE) == 0)
				sstat1 = aic_inb(aic, SSTAT1);
			aic->flags |= AIC_BUSFREE_OK;
		}
	}

	if ((sstat1 & BUSFREE) != 0) {
		aic_outb(aic, SCSISEQ, 0);
		aic_outb(aic, CLRSINT0, sstat0);
		aic_outb(aic, CLRSINT1, sstat1);
		if ((scb = aic->nexus)) {
			if ((aic->flags & AIC_BUSFREE_OK) == 0) {
				ccb = scb->ccb;
				ccb->ccb_h.status = CAM_UNEXP_BUSFREE;
				aic_done(aic, scb);
			} else if (scb->flags & SCB_DEVICE_RESET) {
				ccb = scb->ccb;
				if (ccb->ccb_h.func_code == XPT_RESET_DEV) {
					xpt_async(AC_SENT_BDR,
					    ccb->ccb_h.path, NULL);
					ccb->ccb_h.status |= CAM_REQ_CMP;
				} else
					ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
				aic_done(aic, scb);
			} else if (scb->flags & SCB_SENSE) {
				/* autosense request */
				aic->flags &= ~AIC_BUSFREE_OK;
				aic->tinfo[scb->target].lubusy &=
				    ~(1 << scb->lun);
				aic_select(aic);
				aic_outb(aic, DMACNTRL0, INTEN);
				return;
			}
		}
		aic->flags &= ~AIC_BUSFREE_OK;
		aic->state = AIC_IDLE;
		aic_start(aic);
		aic_outb(aic, DMACNTRL0, INTEN);
		return;
	}

	if ((sstat1 & REQINIT) != 0) {
		u_int8_t phase = aic_inb(aic, SCSISIGI) & PH_MASK;
		aic_outb(aic, SCSISIGO, phase);
		aic_outb(aic, CLRSINT1, CLRPHASECHG);

		switch (phase) {
		case PH_MSGOUT:
			aic_msgout(aic);
			break;
		case PH_MSGIN:
			aic_msgin(aic);
			break;
		case PH_STAT:
			scb = aic->nexus;
			ccb = scb->ccb;
			aic_outb(aic, DMACNTRL0, 0);
			aic_outb(aic, SXFRCTL0, CHEN|SPIOEN);
			scb->status = aic_inb(aic, SCSIDAT);
			aic_outb(aic, SXFRCTL0, CHEN);
			break;
		case PH_CMD:
			aic_cmd(aic);
			break;
		case PH_DATAIN:
			aic_datain(aic);
			break;
		case PH_DATAOUT:
			aic_dataout(aic);
			break;
		}
		aic->prev_phase = phase;
		aic_outb(aic, DMACNTRL0, INTEN);
		return;
	}

	printf("aic_intr: unexpected intr sstat0 %x sstat1 %x\n",
		sstat0, sstat1);
	aic_outb(aic, DMACNTRL0, INTEN);
}

/*
 * Reset ourselves.
 */
static void
aic_chip_reset(struct aic_softc *aic)
{
	/*
	 * Doc. recommends to clear these two registers before
	 * operations commence
	 */
	aic_outb(aic, SCSITEST, 0);
	aic_outb(aic, TEST, 0);

	/* Reset SCSI-FIFO and abort any transfers */
	aic_outb(aic, SXFRCTL0, CHEN|CLRCH|CLRSTCNT);

	/* Reset HOST-FIFO */
	aic_outb(aic, DMACNTRL0, RSTFIFO);
	aic_outb(aic, DMACNTRL1, 0);

	/* Disable all selection features */
	aic_outb(aic, SCSISEQ, 0);
	aic_outb(aic, SXFRCTL1, 0);

	/* Disable interrupts */
	aic_outb(aic, SIMODE0, 0);
	aic_outb(aic, SIMODE1, 0);

	/* Clear interrupts */
	aic_outb(aic, CLRSINT0, 0x7f);
	aic_outb(aic, CLRSINT1, 0xef);

	/* Disable synchronous transfers */
	aic_outb(aic, SCSIRATE, 0);

	/* Haven't seen ant errors (yet) */
	aic_outb(aic, CLRSERR, 0x07);

	/* Set our SCSI-ID */
	aic_outb(aic, SCSIID, aic->initiator << OID_S);
	aic_outb(aic, BRSTCNTRL, EISA_BRST_TIM);
}

/*
 * Reset the SCSI bus
 */
static void
aic_scsi_reset(struct aic_softc *aic)
{
	aic_outb(aic, SCSISEQ, SCSIRSTO);
	DELAY(500);
	aic_outb(aic, SCSISEQ, 0);
	DELAY(50);
}

/*
 * Reset. Abort all pending commands.
 */
static void
aic_reset(struct aic_softc *aic, int initiate_reset)
{
	struct ccb_hdr *ccb_h;

	CAM_DEBUG_PRINT(CAM_DEBUG_TRACE, ("aic_reset\n"));

	if (initiate_reset)
		aic_scsi_reset(aic);
	aic_chip_reset(aic);

	xpt_async(AC_BUS_RESET, aic->path, NULL);

	while ((ccb_h = TAILQ_FIRST(&aic->pending_ccbs)) != NULL) {
		TAILQ_REMOVE(&aic->pending_ccbs, ccb_h, sim_links.tqe);
		ccb_h->status |= CAM_SCSI_BUS_RESET;
		aic_done(aic, (struct aic_scb *)ccb_h->ccb_scb_ptr);
	}

	while ((ccb_h = TAILQ_FIRST(&aic->nexus_ccbs)) != NULL) {
		TAILQ_REMOVE(&aic->nexus_ccbs, ccb_h, sim_links.tqe);
		ccb_h->status |= CAM_SCSI_BUS_RESET;
		aic_done(aic, (struct aic_scb *)ccb_h->ccb_scb_ptr);
	}

	if (aic->nexus) {
		ccb_h = &aic->nexus->ccb->ccb_h;
		ccb_h->status |= CAM_SCSI_BUS_RESET;
		aic_done(aic, aic->nexus);
	}

	aic->state = AIC_IDLE;
	aic_outb(aic, DMACNTRL0, INTEN);
}

static char *aic_chip_names[] = {
	"AIC6260", "AIC6360", "AIC6370", "GM82C700",
};

static struct {
    	int type;
	char *idstring;
} aic_chip_ids[] = {
    	{ AIC6360, IDSTRING_AIC6360 },
	{ AIC6370, IDSTRING_AIC6370 },
	{ GM82C700, IDSTRING_GM82C700 },
};

static void
aic_init(struct aic_softc *aic)
{
	struct aic_scb *scb;
	struct aic_tinfo *ti;
	u_int8_t porta, portb;
	char chip_id[33];
	int i;

	TAILQ_INIT(&aic->pending_ccbs);
	TAILQ_INIT(&aic->nexus_ccbs);
	aic->nexus = NULL;
	aic->state = AIC_IDLE;
	aic->prev_phase = -1;
	aic->flags = 0;

	aic_chip_reset(aic);
	aic_scsi_reset(aic);

	/* determine the chip type from its ID string */
	aic->chip_type = AIC6260;
	aic_insb(aic, ID, chip_id, sizeof(chip_id) - 1);
	chip_id[sizeof(chip_id) - 1] = '\0';
	for (i = 0; i < sizeof(aic_chip_ids) / sizeof(aic_chip_ids[0]); i++) {
		if (!strcmp(chip_id, aic_chip_ids[i].idstring)) {
			aic->chip_type = aic_chip_ids[i].type;
			break;
		}
	}

	porta = aic_inb(aic, PORTA);
	portb = aic_inb(aic, PORTB);

	aic->initiator = PORTA_ID(porta);
	if (PORTA_PARITY(porta))
		aic->flags |= AIC_PARITY_ENABLE;
	if (PORTB_DISC(portb))
		aic->flags |= AIC_DISC_ENABLE;
	if (PORTB_DMA(portb))
		aic->flags |= AIC_DMA_ENABLE;

	/*
	 * We can do fast SCSI (10MHz clock rate) if bit 4 of portb
	 * is set and we've got a 6360.  The 6260 can only do standard
	 * 5MHz SCSI.
	 */
	if (aic->chip_type > AIC6260 || aic_inb(aic, REV)) {
		if (PORTB_FSYNC(portb))
			aic->flags |= AIC_FAST_ENABLE;
		aic->flags |= AIC_DWIO_ENABLE;
	}

	if (aic->flags & AIC_FAST_ENABLE)
		aic->max_period = AIC_FAST_SYNC_PERIOD;
    	else
		aic->max_period = AIC_SYNC_PERIOD;
	aic->min_period = AIC_MIN_SYNC_PERIOD;
	
	free_scbs = NULL;
	for (i = 255; i >= 0; i--) {
		scb = &aic->scbs[i];
		scb->tag = i;
		aic_free_scb(aic, scb);
	}

	for (i = 0; i < 8; i++) {
		if (i == aic->initiator)
			continue;
		ti = &aic->tinfo[i];
		bzero(ti, sizeof(*ti));
		ti->flags = TINFO_TAG_ENB;
		if (aic->flags & AIC_DISC_ENABLE)
			ti->flags |= TINFO_DISC_ENB;
		ti->user.period = aic->max_period;
		ti->user.offset = AIC_SYNC_OFFSET;
		ti->scsirate = 0;
	}

	aic_outb(aic, DMACNTRL0, INTEN);
}

int
aic_probe(struct aic_softc *aic)
{
	int i;

	/* Remove aic6360 from possible powerdown mode */
	aic_outb(aic, DMACNTRL0, 0);

#define	STSIZE	16
	aic_outb(aic, DMACNTRL1, 0);	/* Reset stack pointer */
	for (i = 0; i < STSIZE; i++)
		aic_outb(aic, STACK, i);

	/* See if we can pull out the same sequence */
	aic_outb(aic, DMACNTRL1, 0);
	for (i = 0; i < STSIZE && aic_inb(aic, STACK) == i; i++)
		;
	if (i != STSIZE)
		return (ENXIO);
#undef	STSIZE
	return (0);
}

int
aic_attach(struct aic_softc *aic)
{
	struct cam_devq *devq;

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(256);
	if (devq == NULL)
		return (ENOMEM);

	/*
	 * Construct our SIM entry
	 */
	aic->sim = cam_sim_alloc(aic_action, aic_poll, "aic", aic,
				 aic->unit, &Giant, 2, 256, devq);
	if (aic->sim == NULL) {
		cam_simq_free(devq);
		return (ENOMEM);
	}

	if (xpt_bus_register(aic->sim, aic->dev, 0) != CAM_SUCCESS) {
		cam_sim_free(aic->sim, /*free_devq*/TRUE);
		return (ENXIO);
	}

	if (xpt_create_path(&aic->path, /*periph*/NULL,
			    cam_sim_path(aic->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(aic->sim));
		cam_sim_free(aic->sim, /*free_devq*/TRUE);
		return (ENXIO);
	}

	aic_init(aic);

	printf("aic%d: %s", aic->unit, aic_chip_names[aic->chip_type]);
	if (aic->flags & AIC_DMA_ENABLE)
		printf(", dma");
	if (aic->flags & AIC_DISC_ENABLE)
		printf(", disconnection");
	if (aic->flags & AIC_PARITY_ENABLE)
		printf(", parity check");
	if (aic->flags & AIC_FAST_ENABLE)
		printf(", fast SCSI");
	printf("\n");
	return (0);
}

int
aic_detach(struct aic_softc *aic)
{
	xpt_async(AC_LOST_DEVICE, aic->path, NULL);
	xpt_free_path(aic->path);
	xpt_bus_deregister(cam_sim_path(aic->sim));
	cam_sim_free(aic->sim, /*free_devq*/TRUE);
	return (0);
}
