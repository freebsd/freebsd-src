/*-
 * Copyright (c) 2001,2002 Thomas Quinot <thomas@cuivre.fr.eu.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devicestat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ata.h>
#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>

#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* hardware command descriptor block */
struct atapi_hcb {
    struct atapi_xpt_softc *softc;
    int			unit;
    int			bus;
    int			target;
    int			lun;
    union ccb		*ccb;
    u_int8_t		cmd[CAM_MAX_CDBLEN];
    int			flags;
#define DOING_AUTOSENSE 1

    char		*dxfer_alloc;
    TAILQ_ENTRY(atapi_hcb) chain;
};

/* private data associated with an ATA bus */
struct atapi_xpt_softc {
    struct ata_channel	*ata_ch;
    struct cam_path	*path;
    struct cam_sim	*sim;
    int			flags;
#define BUS_REGISTERED		0x01
#define RESOURCE_SHORTAGE	0x02

    TAILQ_HEAD(,atapi_hcb) pending_hcbs;
    LIST_ENTRY(atapi_xpt_softc) chain;
};

static LIST_HEAD(,atapi_xpt_softc) all_buses = LIST_HEAD_INITIALIZER(all_buses);

/* CAM XPT methods */
static void atapi_action(struct cam_sim *, union ccb *);
static void atapi_poll(struct cam_sim *);
static void atapi_async(void *, u_int32_t, struct cam_path *, void *);
static void atapi_async1(void *, u_int32_t, struct cam_path *, void *);
static int atapi_cb(struct atapi_request *);

/* internal functions */
static void setup_dev(struct atapi_xpt_softc *, struct ata_device *);
static void setup_async_cb(struct atapi_xpt_softc *, uint32_t);
static void cam_rescan_callback(struct cam_periph *, union ccb *);
static void cam_rescan(struct cam_sim *);
static void free_hcb_and_ccb_done(struct atapi_hcb *, u_int32_t);
static struct atapi_hcb *allocate_hcb(struct atapi_xpt_softc *, int, int, union ccb *);
static void free_hcb(struct atapi_hcb *hcb);
static void free_softc(struct atapi_xpt_softc *scp);
static struct ata_device *get_ata_device(struct atapi_xpt_softc *scp, int id);

static MALLOC_DEFINE(M_ATACAM, "ATA CAM transport", "ATA driver CAM-XPT layer");

void 
atapi_cam_attach_bus(struct ata_channel *ata_ch)
{
    struct atapi_xpt_softc *scp = NULL;
    struct cam_devq *devq = NULL;
    struct cam_sim *sim = NULL;
    struct cam_path *path = NULL;
    int unit;

    LIST_FOREACH(scp, &all_buses, chain) {
	if (scp->ata_ch == ata_ch)
	    return;
    }

    if ((scp = malloc(sizeof(struct atapi_xpt_softc),
		      M_ATACAM, M_NOWAIT | M_ZERO)) == NULL)
	goto error;

    scp->ata_ch = ata_ch;
    TAILQ_INIT(&scp->pending_hcbs);
    LIST_INSERT_HEAD(&all_buses, scp, chain);
    unit = device_get_unit(ata_ch->dev);

    if ((devq = cam_simq_alloc(16)) == NULL)
	goto error;

    if ((sim = cam_sim_alloc(atapi_action, atapi_poll, "ata",
		 (void *)scp, unit, 1, 1, devq)) == NULL) {
	cam_simq_free(devq);
	goto error;
    }
    scp->sim = sim;

    if (xpt_bus_register(sim, 0) != CAM_SUCCESS) {
	goto error;
    }
    scp->flags |= BUS_REGISTERED;

    if (xpt_create_path(&path, /*periph*/ NULL,
		cam_sim_path(sim), CAM_TARGET_WILDCARD,
		CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	goto error;
    }
    scp->path = path;

    CAM_DEBUG(path, CAM_DEBUG_TRACE, ("Registered SIM for ata%d\n", unit));

    setup_async_cb(scp, AC_LOST_DEVICE);

    if (ata_ch->devices & ATA_ATAPI_MASTER)
	setup_dev(scp, &ata_ch->device[MASTER]);
    if (ata_ch->devices & ATA_ATAPI_SLAVE)
	setup_dev(scp, &ata_ch->device[SLAVE]);

    cam_rescan(sim);
    return;

error:
    free_softc(scp);
}

void 
atapi_cam_detach_bus(struct ata_channel *ata_ch)
{
    struct atapi_xpt_softc *scp;

    LIST_FOREACH(scp, &all_buses, chain) {
	if (scp->ata_ch == ata_ch)
	    free_softc(scp);
    }
}

static void
setup_dev(struct atapi_xpt_softc *scp, struct ata_device *atp)
{
    if (atp->driver == NULL) {
	ata_set_name(atp, "atapicam",
		     2 * device_get_unit(atp->channel->dev) +
		     (atp->unit == ATA_MASTER) ? 0 : 1);
	atp->driver = (void *)scp;
    }
}

static void
setup_async_cb(struct atapi_xpt_softc *scp, uint32_t events)
{
    struct ccb_setasync csa;

    xpt_setup_ccb(&csa.ccb_h, scp->path, /*priority*/ 5);
    csa.ccb_h.func_code = XPT_SASYNC_CB;
    csa.event_enable = events;
    csa.callback = &atapi_async;
    csa.callback_arg = scp->sim;
    xpt_action((union ccb *) &csa);
}

static void 
atapi_action(struct cam_sim *sim, union ccb *ccb)
{
    struct atapi_xpt_softc *softc = (struct atapi_xpt_softc*)cam_sim_softc(sim);
    struct ccb_hdr *ccb_h = &ccb->ccb_h;
    struct atapi_hcb *hcb = NULL;
    int unit = cam_sim_unit(sim);
    int bus = cam_sim_bus(sim);
    int len, s;
    char *buf;

    switch (ccb_h->func_code) {
    case XPT_PATH_INQ: {
	struct ccb_pathinq *cpi = &ccb->cpi;

	cpi->version_num = 1;
	cpi->hba_inquiry = 0;
	cpi->hba_misc = 0;
	cpi->hba_eng_cnt = 0;
	bzero(cpi->vuhba_flags, sizeof(cpi->vuhba_flags));
	cpi->max_target = 1;
	cpi->max_lun = 0;
	cpi->async_flags = 0;
	cpi->hpath_id = 0;
	cpi->initiator_id = 7;
	strncpy(cpi->sim_vid, "FreeBSD", sizeof(cpi->sim_vid));
	strncpy(cpi->hba_vid, "ATAPI", sizeof(cpi->hba_vid));
	strncpy(cpi->dev_name, cam_sim_name(sim), sizeof cpi->dev_name);
	cpi->unit_number = cam_sim_unit(sim);
	cpi->bus_id = cam_sim_bus(sim);
	if (softc->ata_ch && ccb_h->target_id >= 0) {
	    switch (softc->ata_ch->device[ccb_h->target_id].mode) {
	    case ATA_PIO1:
		cpi->base_transfer_speed = 5200;
		break;
	    case ATA_PIO2:
		cpi->base_transfer_speed = 7000;
		break;
	    case ATA_PIO3:
		cpi->base_transfer_speed = 11000;
		break;
	    case ATA_PIO4:
	    case ATA_DMA:
	    case ATA_WDMA2:
		cpi->base_transfer_speed = 16000;
		break;
	    case ATA_UDMA2:
		cpi->base_transfer_speed = 33000;
		break;
	    case ATA_UDMA4:
		cpi->base_transfer_speed = 66000;
		break;
	    case ATA_UDMA5:
		cpi->base_transfer_speed = 100000;
		break;
	    case ATA_UDMA6:
		cpi->base_transfer_speed = 133000;
		break;
	    default: cpi->base_transfer_speed = 3300;
	    }
	}
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_RESET_DEV:
	/* should reset the device */
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("dev reset\n"));
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;

    case XPT_RESET_BUS:
	/* should reset the ATA bus */
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("bus reset\n"));
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;

    case XPT_SET_TRAN_SETTINGS:
	/* ignore these, we're not doing SCSI here */
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE,
		  ("SET_TRAN_SETTINGS not supported\n"));
	ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
	xpt_done(ccb);
	return;

    case XPT_GET_TRAN_SETTINGS: {
	struct ccb_trans_settings *cts = &ccb->cts;

	/*
	 * XXX The default CAM transport code is very scsi specific and
	 * doesn't understand IDE speeds very well.  Be silent about it
	 * here and let it default to what is set in XPT_PATH_INQ
	 */
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("GET_TRAN_SETTINGS\n"));
	cts->valid = (CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID);
	cts->flags &= ~(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_CALC_GEOMETRY: {
	struct ccb_calc_geometry *ccg;
	unsigned int size_mb;
	unsigned int secs_per_cylinder;
	int extended;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("CALC_GEOMETRY\n"));
	ccg = &ccb->ccg;
	size_mb = ccg->volume_size / ((1024L * 1024L) / ccg->block_size);
	extended = 1;

	if (size_mb > 1024 && extended) {
	    ccg->heads = 255;
	    ccg->secs_per_track = 63;
	} else {
	    ccg->heads = 64;
	    ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_SCSI_IO: {
	struct ccb_scsiio *csio = &ccb->csio;
	int tid = ccb_h->target_id, lid = ccb_h->target_lun;
	struct ata_device *dev = get_ata_device(softc, tid);

	CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE, ("XPT_SCSI_IO\n"));

	/* check that this request was not aborted already */
	if ((ccb_h->status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
	    printf("XPT_SCSI_IO received but already in progress?\n");
	    xpt_done(ccb);
	    return;
	}
	if (dev == NULL) {
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		      ("SCSI IO received for invalid device\n"));
	    ccb_h->status = CAM_TID_INVALID;
	    xpt_done(ccb);
	    return;
	}
	if (lid > 0) {
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		      ("SCSI IO received for invalid lun %d\n", lid));
	    ccb_h->status = CAM_LUN_INVALID;
	    xpt_done(ccb);
	    return;
	}
	if ((ccb_h->flags & CAM_SCATTER_VALID)) {
	    /* scatter-gather not supported */
	    xpt_print_path(ccb_h->path);
	    printf("ATAPI-CAM does not support scatter-gather yet!\n");
	    break;
	}
	if ((hcb = allocate_hcb(softc, unit, bus, ccb)) == NULL)
	    goto action_oom;

	ccb_h->status |= CAM_SIM_QUEUED;

	bcopy((ccb_h->flags & CAM_CDB_POINTER) ?
	      csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes,
	      hcb->cmd, csio->cdb_len);
#ifdef CAMDEBUG
	if (CAM_DEBUGGED(ccb_h->path, CAM_DEBUG_CDB)) {
		char cdb_str[(SCSI_MAX_CDBLEN * 3) + 1];

		printf("atapi_action: hcb@%p: %s\n", hcb,
		       scsi_cdb_string(hcb->cmd, cdb_str, sizeof(cdb_str)));
	}
#endif

	len = csio->dxfer_len;
	buf = csio->data_ptr;

	/* some SCSI commands require special processing */
	switch (hcb->cmd[0]) {
	case INQUIRY: {
	    /*
	     * many ATAPI devices seem to report more than
	     * SHORT_INQUIRY_LENGTH bytes of available INQUIRY
	     * information, but respond with some incorrect condition
	     * when actually asked for it, so we are going to pretend
	     * that only SHORT_INQUIRY_LENGTH are expected, anyway.
	     */
	    struct scsi_inquiry *inq = (struct scsi_inquiry *) &hcb->cmd[0];

	    if (inq->byte2 == 0 && inq->page_code == 0 &&
		inq->length > SHORT_INQUIRY_LENGTH) {
		bzero(buf, len);
		len = inq->length = SHORT_INQUIRY_LENGTH;
	    }
	    break;
	}
	case MODE_SELECT_6:
	    /* FALLTHROUGH */

	case MODE_SENSE_6:
	    /*
	     * not supported by ATAPI/MMC devices (per SCSI MMC spec)
	     * translate to _10 equivalent.
	     * (actually we should do this only if we have tried 
	     * MODE_foo_6 and received ILLEGAL_REQUEST or
	     * INVALID COMMAND OPERATION CODE)
	     * alternative fix: behave like a honest CAM transport, 
	     * do not muck with CDB contents, and change scsi_cd to 
	     * always use MODE_SENSE_10 in cdgetmode(), or let scsi_cd
	     * know that this specific unit is an ATAPI/MMC one, 
	     * and in /that case/ use MODE_SENSE_10
	     */

	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE, 
		      ("Translating %s into _10 equivalent\n",
		      (hcb->cmd[0] == MODE_SELECT_6) ?
		      "MODE_SELECT_6" : "MODE_SENSE_6"));
	    hcb->cmd[0] |= 0x40;
	    hcb->cmd[6] = 0;
	    hcb->cmd[7] = 0;
	    hcb->cmd[8] = hcb->cmd[4];
	    hcb->cmd[9] = hcb->cmd[5];
	    hcb->cmd[4] = 0;
	    hcb->cmd[5] = 0;
	    break;

	case READ_6:
	    /* FALLTHROUGH */

	case WRITE_6:
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE, 
		      ("Translating %s into _10 equivalent\n",
		      (hcb->cmd[0] == READ_6) ? "READ_6" : "WRITE_6"));
	    hcb->cmd[0] |= 0x20;
	    hcb->cmd[9] = hcb->cmd[5];
	    hcb->cmd[8] = hcb->cmd[4];
	    hcb->cmd[7] = 0;
	    hcb->cmd[6] = 0;
	    hcb->cmd[5] = hcb->cmd[3];
	    hcb->cmd[4] = hcb->cmd[2];
	    hcb->cmd[3] = hcb->cmd[1] & 0x1f;
	    hcb->cmd[2] = 0;
	    hcb->cmd[1] = 0;
	    break;
	}

	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN && (len & 1)) {
	    /* ATA always transfers an even number of bytes */
	    if (!(buf = hcb->dxfer_alloc = malloc(++len, M_ATACAM,
						  M_NOWAIT | M_ZERO)))
		goto action_oom;
	}
	s = splbio();
	TAILQ_INSERT_TAIL(&softc->pending_hcbs, hcb, chain);
	splx(s);
	if (atapi_queue_cmd(dev, hcb->cmd, buf, len,
			    (((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN) ?
			    ATPR_F_READ : 0) | ATPR_F_QUIET,
			    ccb_h->timeout, atapi_cb, (void *)hcb) == 0)
	    return;
	break;
    }

    default:
	CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		  ("unsupported function code 0x%02x\n", ccb_h->func_code));
	ccb_h->status = CAM_REQ_INVALID;
	xpt_done(ccb);
	return;
    }

action_oom:
    if (hcb != NULL)
	free_hcb(hcb);
    xpt_print_path(ccb_h->path);
    printf("out of memory, freezing queue.\n");
    softc->flags |= RESOURCE_SHORTAGE;
    xpt_freeze_simq(sim, /*count*/ 1);
    ccb_h->status = CAM_REQUEUE_REQ;
    xpt_done(ccb);
}

static void 
atapi_poll(struct cam_sim *sim)
{
    /* do nothing - we do not actually service any interrupts */
    printf("atapi_poll called!\n");
}

static int 
atapi_cb(struct atapi_request *req)
{
    struct atapi_hcb *hcb = (struct atapi_hcb *) req->driver;
    struct ccb_scsiio *csio = &hcb->ccb->csio;
    int hcb_status = req->result;
    int s = splbio();

#ifdef CAMDEBUG
	if (CAM_DEBUGGED(csio->ccb_h.path, CAM_DEBUG_CDB)) {
		printf("atapi_cb: hcb@%p status = %02x: (sk = %02x%s%s%s)\n",
		       hcb, hcb_status, hcb_status >> 4,
		       (hcb_status & 4) ? " ABRT" : "",
		       (hcb_status & 2) ? " EOM" : "",
		       (hcb_status & 1) ? " ILI" : "");
		printf("    %s: cmd %02x - sk=%02x asc=%02x ascq=%02x\n",
		       req->device->name, req->ccb[0], req->sense.sense_key,
		       req->sense.asc, req->sense.ascq);
	}
#endif
    if (hcb_status != 0) {
	csio->scsi_status = SCSI_STATUS_CHECK_COND;
	if ((csio->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0) {
	    csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	    bcopy((void *)&req->sense, (void *)&csio->sense_data,
		  sizeof(struct atapi_reqsense));
	}
	free_hcb_and_ccb_done(hcb, CAM_SCSI_STATUS_ERROR);
    } 
    else {
	if (((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) &&
	    hcb->dxfer_alloc != NULL)
	    bcopy(hcb->dxfer_alloc, csio->data_ptr, csio->dxfer_len);
	csio->scsi_status = SCSI_STATUS_OK;
	free_hcb_and_ccb_done(hcb, CAM_REQ_CMP);
    }
    splx(s);
    return 0;
}

static void
free_hcb_and_ccb_done(struct atapi_hcb *hcb, u_int32_t status)
{
    struct atapi_xpt_softc *softc = hcb->softc;
    union ccb *ccb = hcb->ccb;

    if (hcb != NULL) {
	/* we're about to free a hcb, so the shortage has ended */
	if (softc->flags & RESOURCE_SHORTAGE) {
	    softc->flags &= ~RESOURCE_SHORTAGE;
	    status |= CAM_RELEASE_SIMQ;
	}
	free_hcb(hcb);
    }
    ccb->ccb_h.status = 
	status | (ccb->ccb_h.status & ~(CAM_STATUS_MASK | CAM_SIM_QUEUED));
    xpt_done(ccb);
}

static void 
atapi_async(void *callback_arg, u_int32_t code,
	    struct cam_path *path, void *arg)
{
    int s = splbio();

    atapi_async1(callback_arg, code, path, arg);
    splx(s);
}

static void 
atapi_async1(void *callback_arg, u_int32_t code,
	     struct cam_path* path, void *arg)
{
    struct atapi_xpt_softc *softc;
    struct cam_sim *sim;
    int targ;

    sim = (struct cam_sim *) callback_arg;
    softc = (struct atapi_xpt_softc *) cam_sim_softc(sim);
    switch (code) {
    case AC_LOST_DEVICE:
	targ = xpt_path_target_id(path);
	xpt_print_path(path);
	if (targ == -1)
		printf("Lost host adapter\n");
	else
		printf("Lost target %d???\n", targ);
	break;

    default:
	break;
    }
}

static void
cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
	    CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    	      ("Rescan failed, 0x%04x\n", ccb->ccb_h.status));
	} else {
	    CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    	      ("Rescan succeeded\n"));
	}
	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_ATACAM);
}

static void
cam_rescan(struct cam_sim *sim)
{
    struct cam_path *path;
    union ccb *ccb = malloc(sizeof(union ccb), M_ATACAM, M_WAITOK | M_ZERO);
    
    if (xpt_create_path(&path, xpt_periph, cam_sim_path(sim),
			CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP)
	return;

    CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("Rescanning ATAPI bus.\n"));
    xpt_setup_ccb(&ccb->ccb_h, path, 5/*priority (low)*/);
    ccb->ccb_h.func_code = XPT_SCAN_BUS;
    ccb->ccb_h.cbfcnp = cam_rescan_callback;
    ccb->crcn.flags = CAM_FLAG_NONE;
    xpt_action(ccb);
    /* scan is in progress now */
}

static struct atapi_hcb *
allocate_hcb(struct atapi_xpt_softc *softc, int unit, int bus, union ccb *ccb)
{
    struct atapi_hcb *hcb = (struct atapi_hcb *)
    malloc(sizeof(struct atapi_hcb), M_ATACAM, M_NOWAIT | M_ZERO);

    if (hcb != NULL) {
	hcb->softc = softc;
	hcb->unit = unit;
	hcb->bus = bus;
	hcb->ccb = ccb;
    }
    return hcb;
}

static void 
free_hcb(struct atapi_hcb *hcb)
{
    TAILQ_REMOVE(&hcb->softc->pending_hcbs, hcb, chain);
    if (hcb->dxfer_alloc != NULL)
	free(hcb->dxfer_alloc, M_ATACAM);
    free(hcb, M_ATACAM);
}

static void
free_softc(struct atapi_xpt_softc *scp)
{
    struct atapi_hcb *hcb;

    if (scp != NULL) {
	TAILQ_FOREACH(hcb, &scp->pending_hcbs, chain) {
	    free_hcb_and_ccb_done(hcb, CAM_UNREC_HBA_ERROR);
	}
	if (scp->path != NULL) {
	    setup_async_cb(scp, 0);
	    xpt_free_path(scp->path);
	}
	if ((scp->flags & BUS_REGISTERED) != 0) {
	    if (xpt_bus_deregister(cam_sim_path(scp->sim)) == CAM_REQ_CMP)
		scp->flags &= ~BUS_REGISTERED;
	}
	if (scp->sim != NULL) {
	    if ((scp->flags & BUS_REGISTERED) == 0)
		cam_sim_free(scp->sim, /*free_devq*/TRUE);
	    else
		printf("Can't free %s SIM (still registered)\n",
		       cam_sim_name(scp->sim));
	}
	LIST_REMOVE(scp, chain);
	free(scp, M_ATACAM);
    }
}

static struct ata_device *
get_ata_device(struct atapi_xpt_softc *scp, int id)
{
    int role = ATA_ATAPI_MASTER;

    switch (id) {
    case 1:
	role = ATA_ATAPI_SLAVE;
	/* FALLTHROUGH */

    case 0:
	if (scp->ata_ch->devices & role)
	    return &scp->ata_ch->device[id];
	/* FALLTHROUGH */

    default:
	return NULL;
    }
}
