/*-
 * Copyright (c) 2001-2003 Thomas Quinot <thomas@cuivre.fr.eu.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ata.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <vm/uma.h>
#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>

#include <dev/ata/ata-all.h>

/* hardware command descriptor block */
struct atapi_hcb {
    struct atapi_xpt_softc *softc;
    int			unit;
    int			bus;
    int			target;
    int			lun;
    union ccb		*ccb;
    int			flags;
#define QUEUED		0x0001
#define AUTOSENSE       0x0002
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

enum reinit_reason { BOOT_ATTACH, ATTACH, RESET };

static struct mtx atapicam_softc_mtx;
static LIST_HEAD(,atapi_xpt_softc) all_buses = LIST_HEAD_INITIALIZER(all_buses);

/* CAM XPT methods */
static void atapi_action(struct cam_sim *, union ccb *);
static void atapi_poll(struct cam_sim *);
static void atapi_async(void *, u_int32_t, struct cam_path *, void *);
static void atapi_async1(void *, u_int32_t, struct cam_path *, void *);
static void atapi_cb(struct ata_request *);

/* internal functions */
static void reinit_bus(struct atapi_xpt_softc *scp, enum reinit_reason reason);
static void setup_dev(struct atapi_xpt_softc *, struct ata_device *);
static void setup_async_cb(struct atapi_xpt_softc *, uint32_t);
static void cam_rescan_callback(struct cam_periph *, union ccb *);
static void cam_rescan(struct cam_sim *);
static void free_hcb_and_ccb_done(struct atapi_hcb *, u_int32_t);
static struct atapi_hcb *allocate_hcb(struct atapi_xpt_softc *, int, int, union ccb *);
static void free_hcb(struct atapi_hcb *hcb);
static void free_softc(struct atapi_xpt_softc *scp);
static struct atapi_xpt_softc *get_softc(struct ata_channel *ata_ch);
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

    GIANT_REQUIRED;

    if (mtx_initialized(&atapicam_softc_mtx) == 0)
	mtx_init(&atapicam_softc_mtx, "ATAPI/CAM softc mutex", NULL, MTX_DEF);

    mtx_lock(&atapicam_softc_mtx);

    LIST_FOREACH(scp, &all_buses, chain) {
	if (scp->ata_ch == ata_ch)
	    break;
    }
    mtx_unlock(&atapicam_softc_mtx);

    if (scp != NULL)
	return;

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
    reinit_bus(scp, cold ? BOOT_ATTACH : ATTACH);
    return;

error:
    free_softc(scp);
}

void
atapi_cam_detach_bus(struct ata_channel *ata_ch)
{
    struct atapi_xpt_softc *scp = get_softc(ata_ch);

    mtx_lock(&Giant);
    free_softc(scp);
    mtx_unlock(&Giant);
}

void
atapi_cam_reinit_bus(struct ata_channel *ata_ch) {
    struct atapi_xpt_softc *scp;


    /*
     * We might not be properly set up yet if the bus is being
     * reinitialised during the boot-up sequence, before the ATAPI
     * bus is registered.
     */

    if ((mtx_initialized(&atapicam_softc_mtx) == 0)
        || ((scp = get_softc(ata_ch)) == NULL))
	return;

    mtx_lock(&Giant);
    reinit_bus(scp, RESET);
    mtx_unlock(&Giant);
}

static void
reinit_bus(struct atapi_xpt_softc *scp, enum reinit_reason reason) {

    GIANT_REQUIRED;

    if (scp->ata_ch->devices & ATA_ATAPI_MASTER)
	setup_dev(scp, &scp->ata_ch->device[MASTER]);
    if (scp->ata_ch->devices & ATA_ATAPI_SLAVE)
	setup_dev(scp, &scp->ata_ch->device[SLAVE]);

    switch (reason) {
	case BOOT_ATTACH:
	    break;
	case RESET:
	    xpt_async(AC_BUS_RESET, scp->path, NULL);
	    /*FALLTHROUGH*/
	case ATTACH:
	    cam_rescan(scp->sim);
	    break;
    }
}

static void
setup_dev(struct atapi_xpt_softc *scp, struct ata_device *atp)
{
    if (atp->softc == NULL) {
	ata_set_name(atp, "atapicam",
		     2 * device_get_unit(atp->channel->dev) +
		     (atp->unit == ATA_MASTER) ? 0 : 1);
	atp->softc = (void *)scp;
    }
}

static void
setup_async_cb(struct atapi_xpt_softc *scp, uint32_t events)
{
    struct ccb_setasync csa;

    GIANT_REQUIRED;

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
    struct ata_request *request = NULL;
    int unit = cam_sim_unit(sim);
    int bus = cam_sim_bus(sim);
    int len;
    char *buf;

    switch (ccb_h->func_code) {
    case XPT_PATH_INQ: {
	struct ccb_pathinq *cpi = &ccb->cpi;

	cpi->version_num = 1;
	cpi->hba_inquiry = 0;
	cpi->target_sprt = 0;
	cpi->hba_misc = PIM_NO_6_BYTE;
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
	cpi->base_transfer_speed = 3300;

	if (softc->ata_ch && ccb_h->target_id != CAM_TARGET_WILDCARD) {
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
	    default:
		break;
	    }
	}
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_RESET_DEV: {
	int tid = ccb_h->target_id;
	struct ata_device *dev = get_ata_device(softc, tid);

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("dev reset\n"));
	ata_controlcmd(dev, ATA_ATAPI_RESET, 0, 0, 0);
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_RESET_BUS:
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("bus reset\n"));
	ata_reinit(softc->ata_ch);
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
	 * XXX The default CAM transport code is very SCSI-specific and
	 * doesn't understand IDE speeds very well. Be silent about it
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
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("CALC_GEOMETRY\n"));
	cam_calc_geometry(&ccb->ccg, /*extended*/1);
	xpt_done(ccb);
	return;
    }

    case XPT_SCSI_IO: {
	struct ccb_scsiio *csio = &ccb->csio;
	int tid = ccb_h->target_id, lid = ccb_h->target_lun;
	struct ata_device *dev = get_ata_device(softc, tid);
	int request_flags = ATA_R_QUIET | ATA_R_ATAPI;

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
	    goto action_invalid;
	}
	if (lid > 0) {
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		      ("SCSI IO received for invalid lun %d\n", lid));
	    goto action_invalid;
	}
	if (csio->cdb_len > sizeof request->u.atapi.ccb) {
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		("CAM CCB too long for ATAPI"));
	    goto action_invalid;
	}
	if ((ccb_h->flags & CAM_SCATTER_VALID)) {
	    /* scatter-gather not supported */
	    xpt_print_path(ccb_h->path);
	    printf("ATAPI/CAM does not support scatter-gather yet!\n");
	    goto action_invalid;
	}

	switch (ccb_h->flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
	     request_flags |= ATA_R_READ|ATA_R_DMA;
	     break;
	case CAM_DIR_OUT:
	     request_flags |= ATA_R_WRITE|ATA_R_DMA;
	     break;
	case CAM_DIR_NONE:
	     /* No flags need to be set */
	     break;
	default:
	     ata_prtdev(dev, "unknown IO operation\n");
	     goto action_invalid;
	}
	if (dev->mode < ATA_DMA)
	    request_flags &= ~ATA_R_DMA;

	if ((hcb = allocate_hcb(softc, unit, bus, ccb)) == NULL) {
	    printf("cannot allocate ATAPI/CAM hcb\n");
	    goto action_oom;
	}
	if ((request = ata_alloc_request()) == NULL) {
	    printf("cannot allocate ATAPI/CAM request\n");
	    goto action_oom;
	}

	bcopy((ccb_h->flags & CAM_CDB_POINTER) ?
	      csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes,
	      request->u.atapi.ccb, csio->cdb_len);
#ifdef CAMDEBUG
	if (CAM_DEBUGGED(ccb_h->path, CAM_DEBUG_CDB)) {
		char cdb_str[(SCSI_MAX_CDBLEN * 3) + 1];

		printf("atapi_action: hcb@%p: %s\n", hcb,
		       scsi_cdb_string(request->u.atapi.ccb, cdb_str, sizeof(cdb_str)));
	}
#endif

	len = csio->dxfer_len;
	buf = csio->data_ptr;

	/* some SCSI commands require special processing */
	switch (request->u.atapi.ccb[0]) {
	case INQUIRY: {
	    /*
	     * many ATAPI devices seem to report more than
	     * SHORT_INQUIRY_LENGTH bytes of available INQUIRY
	     * information, but respond with some incorrect condition
	     * when actually asked for it, so we are going to pretend
	     * that only SHORT_INQUIRY_LENGTH are expected, anyway.
	     */
	    struct scsi_inquiry *inq = (struct scsi_inquiry *) &request->u.atapi.ccb[0];

	    if (inq->byte2 == 0 && inq->page_code == 0 &&
		inq->length > SHORT_INQUIRY_LENGTH) {
		bzero(buf, len);
		len = inq->length = SHORT_INQUIRY_LENGTH;
	    }
	    break;
	}
	case READ_6:
	    /* FALLTHROUGH */

	case WRITE_6:
	    CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		      ("Translating %s into _10 equivalent\n",
		      (request->u.atapi.ccb[0] == READ_6) ? "READ_6" : "WRITE_6"));
	    request->u.atapi.ccb[0] |= 0x20;
	    request->u.atapi.ccb[9] = request->u.atapi.ccb[5];
	    request->u.atapi.ccb[8] = request->u.atapi.ccb[4];
	    request->u.atapi.ccb[7] = 0;
	    request->u.atapi.ccb[6] = 0;
	    request->u.atapi.ccb[5] = request->u.atapi.ccb[3];
	    request->u.atapi.ccb[4] = request->u.atapi.ccb[2];
	    request->u.atapi.ccb[3] = request->u.atapi.ccb[1] & 0x1f;
	    request->u.atapi.ccb[2] = 0;
	    request->u.atapi.ccb[1] = 0;
	    break;
	}

	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN && (len & 1)) {
	    /* ATA always transfers an even number of bytes */
	    if ((buf = hcb->dxfer_alloc
                 = malloc(++len, M_ATACAM, M_NOWAIT | M_ZERO)) == NULL) {
		printf("cannot allocate ATAPI/CAM buffer\n");
		goto action_oom;
	    }
	}
	request->device = dev;
	request->driver = hcb;
	request->data = buf;
	request->bytecount = len;
	request->transfersize = min(request->bytecount, 65534);
	request->timeout = ccb_h->timeout / 1000; /* XXX lost granularity */
	request->retries = 2;
	request->callback = &atapi_cb;
	request->flags = request_flags;

	TAILQ_INSERT_TAIL(&softc->pending_hcbs, hcb, chain);
	hcb->flags |= QUEUED;
	ccb_h->status |= CAM_SIM_QUEUED;

	ata_queue_request(request);
	return;
    }

    default:
	CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE,
		  ("unsupported function code 0x%02x\n", ccb_h->func_code));
	goto action_invalid;
    }

    /* NOTREACHED */

action_oom:
    if (request != NULL)
	ata_free_request(request);
    if (hcb != NULL)
	free_hcb(hcb);
    xpt_print_path(ccb_h->path);
    printf("out of memory, freezing queue.\n");
    softc->flags |= RESOURCE_SHORTAGE;
    xpt_freeze_simq(sim, /*count*/ 1);
    ccb_h->status = CAM_REQUEUE_REQ;
    xpt_done(ccb);
    return;

action_invalid:
    ccb_h->status = CAM_REQ_INVALID;
    xpt_done(ccb);
    return;
}

static void
atapi_poll(struct cam_sim *sim)
{
    /* do nothing - we do not actually service any interrupts */
    printf("atapi_poll called!\n");
}

static void
atapi_cb(struct ata_request *request)
{
    struct atapi_hcb *hcb;
    struct ccb_scsiio *csio;
    u_int32_t rc;

    mtx_lock(&Giant);

    hcb = (struct atapi_hcb *)request->driver;
    csio = &hcb->ccb->csio;

#ifdef CAMDEBUG
# define err (request->u.atapi.sense_key)
    if (CAM_DEBUGGED(csio->ccb_h.path, CAM_DEBUG_CDB)) {
	printf("atapi_cb: hcb@%p error = %02x: (sk = %02x%s%s%s)\n",
	       hcb, err, err >> 4,
	       (err & 4) ? " ABRT" : "",
	       (err & 2) ? " EOM" : "",
	       (err & 1) ? " ILI" : "");
	printf("dev %s: cmd %02x status %02x result %02x\n",
	    request->device->name, request->u.atapi.ccb[0],
	    request->status, request->result);
    }
#endif

    if ((hcb->flags & AUTOSENSE) != 0) {
	rc = CAM_SCSI_STATUS_ERROR;
	if (request->result == 0) {
	    csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	}
    } else if (request->result != 0) {
	rc = CAM_SCSI_STATUS_ERROR;
	csio->scsi_status = SCSI_STATUS_CHECK_COND;

	if ((csio->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0) {
#if 0
	    static const int8_t ccb[16] = { ATAPI_REQUEST_SENSE, 0, 0, 0,
		sizeof(struct atapi_sense), 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0 };

	    bcopy (ccb, request->u.atapi.ccb, sizeof ccb);
	    request->data = (caddr_t)&csio->sense_data;
	    request->bytecount = sizeof(struct atapi_sense);
	    request->transfersize = min(request->bytecount, 65534);
	    request->timeout = csio->ccb_h.timeout / 1000;
	    request->retries = 2;
	    request->flags = ATA_R_QUIET|ATA_R_ATAPI|ATA_R_IMMEDIATE;
	    hcb->flags |= AUTOSENSE;

	    mtx_unlock (&Giant);
	    ata_queue_request(request);
	    return;
#else
	    /* The ATA driver has already requested sense for us. */
	    if (request->error == 0) {
		/* The ATA autosense suceeded. */
		bcopy (&request->u.atapi.sense_data, &csio->sense_data, sizeof(struct atapi_sense));
		csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	    }
#endif
	}
    } else {
	rc = CAM_REQ_CMP;
	csio->scsi_status = SCSI_STATUS_OK;
	if (((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) &&
	    hcb->dxfer_alloc != NULL)
	{
	    bcopy(hcb->dxfer_alloc, csio->data_ptr, csio->dxfer_len);
	}
    }

    free_hcb_and_ccb_done(hcb, rc);
    mtx_unlock(&Giant);

    ata_free_request(request);
}

static void
free_hcb_and_ccb_done(struct atapi_hcb *hcb, u_int32_t status)
{
    struct atapi_xpt_softc *softc = hcb->softc;
    union ccb *ccb = hcb->ccb;

    GIANT_REQUIRED;

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
    mtx_lock(&Giant);
    atapi_async1(callback_arg, code, path, arg);
    mtx_unlock(&Giant);
}

static void
atapi_async1(void *callback_arg, u_int32_t code,
	     struct cam_path* path, void *arg)
{
    struct atapi_xpt_softc *softc;
    struct cam_sim *sim;
    int targ;

    GIANT_REQUIRED;

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
			CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	free(ccb, M_ATACAM);
	return;
    }

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
    if ((hcb->flags & QUEUED) != 0)
	TAILQ_REMOVE(&hcb->softc->pending_hcbs, hcb, chain);
    if (hcb->dxfer_alloc != NULL)
	free(hcb->dxfer_alloc, M_ATACAM);
    free(hcb, M_ATACAM);
}

static void
free_softc(struct atapi_xpt_softc *scp)
{
    struct atapi_hcb *hcb;

    GIANT_REQUIRED;

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

static struct atapi_xpt_softc *
get_softc(struct ata_channel *ata_ch) {
    struct atapi_xpt_softc *scp = NULL;

    mtx_lock(&atapicam_softc_mtx);
    LIST_FOREACH(scp, &all_buses, chain) {
	if (scp->ata_ch == ata_ch)
	    break;
    }
    mtx_unlock(&atapicam_softc_mtx);
    return scp;
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
