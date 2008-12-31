/*-
 * Copyright (c) 2001-2007 Thomas Quinot <thomas@cuivre.fr.eu.org>
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
__FBSDID("$FreeBSD: src/sys/dev/ata/atapi-cam.c,v 1.55.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <machine/resource.h>
#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>

#include <dev/ata/ata-all.h>
#include <ata_if.h>

/* private data associated with an ATA bus */
struct atapi_xpt_softc {
    struct ata_device   atapi_cam_dev;  /* must be first */
    device_t            dev;
    device_t            parent;
    struct ata_channel  *ata_ch;
    struct cam_path     *path;
    struct cam_sim      *sim;
    int                 flags;
#define BUS_REGISTERED          0x01
#define RESOURCE_SHORTAGE       0x02
#define DETACHING               0x04

    TAILQ_HEAD(,atapi_hcb) pending_hcbs;
    struct ata_device   *atadev[2];
    struct mtx          state_lock;
};

/* hardware command descriptor block */
struct atapi_hcb {
    struct atapi_xpt_softc *softc;
    int                 unit;
    int                 bus;
    int                 target;
    int                 lun;
    union ccb           *ccb;
    int                 flags;
#define QUEUED          0x0001
#define AUTOSENSE       0x0002
    char                *dxfer_alloc;
    TAILQ_ENTRY(atapi_hcb) chain;
};

enum reinit_reason { BOOT_ATTACH, ATTACH, RESET };

/* Device methods */
static void atapi_cam_identify(device_t *dev, device_t parent);
static int atapi_cam_probe(device_t dev);
static int atapi_cam_attach(device_t dev);
static int atapi_cam_detach(device_t dev);
static int atapi_cam_reinit(device_t dev);

/* CAM XPT methods */
static void atapi_action(struct cam_sim *, union ccb *);
static void atapi_poll(struct cam_sim *);
static void atapi_async(void *, u_int32_t, struct cam_path *, void *);
static void atapi_cb(struct ata_request *);

/* Module methods */
static int atapi_cam_event_handler(module_t mod, int what, void *arg);

/* internal functions */
static void reinit_bus(struct atapi_xpt_softc *scp, enum reinit_reason reason);
static void setup_async_cb(struct atapi_xpt_softc *, uint32_t);
static void cam_rescan_callback(struct cam_periph *, union ccb *);
static void cam_rescan(struct cam_sim *);
static void free_hcb_and_ccb_done(struct atapi_hcb *, u_int32_t);
static struct atapi_hcb *allocate_hcb(struct atapi_xpt_softc *, int, int, union ccb *);
static void free_hcb(struct atapi_hcb *hcb);
static void free_softc(struct atapi_xpt_softc *scp);

static MALLOC_DEFINE(M_ATACAM, "ata_cam", "ATA driver CAM-XPT layer");

static device_method_t atapi_cam_methods[] = {
	DEVMETHOD(device_identify,      atapi_cam_identify),
	DEVMETHOD(device_probe,         atapi_cam_probe),
	DEVMETHOD(device_attach,        atapi_cam_attach),
	DEVMETHOD(device_detach,        atapi_cam_detach),
	DEVMETHOD(ata_reinit,           atapi_cam_reinit),
	{0, 0}
};

static driver_t atapi_cam_driver = {
	"atapicam",
	atapi_cam_methods,
	sizeof(struct atapi_xpt_softc)
};

static devclass_t       atapi_cam_devclass;
DRIVER_MODULE(atapicam, ata,
	atapi_cam_driver,
	atapi_cam_devclass,
	atapi_cam_event_handler,
	/*arg*/NULL);
MODULE_VERSION(atapicam, 1);
MODULE_DEPEND(atapicam, ata, 1, 1, 1);
MODULE_DEPEND(atapicam, cam, 1, 1, 1);

static void
atapi_cam_identify(device_t *dev, device_t parent)
{
	struct atapi_xpt_softc *scp =
	    malloc (sizeof (struct atapi_xpt_softc), M_ATACAM, M_NOWAIT|M_ZERO);
	device_t child;

	if (scp == NULL) {
		printf ("atapi_cam_identify: out of memory");
		return;
	}

	/* Assume one atapicam instance per parent channel instance. */
	child = device_add_child(parent, "atapicam", -1);
	if (child == NULL) {
		printf ("atapi_cam_identify: out of memory, can't add child");
		free (scp, M_ATACAM);
		return;
	}
	scp->atapi_cam_dev.unit = -1;
	scp->atapi_cam_dev.dev  = child;
	device_quiet(child);
	device_set_softc(child, scp);
}

static int
atapi_cam_probe(device_t dev)
{
	struct ata_device *atadev = device_get_softc (dev);

	KASSERT(atadev != NULL, ("expect valid struct ata_device"));
	if (atadev->unit < 0) {
		device_set_desc(dev, "ATAPI CAM Attachment");
		return (0);
	} else {
		return ENXIO;
	}
}

static int
atapi_cam_attach(device_t dev)
{
    struct atapi_xpt_softc *scp = NULL;
    struct cam_devq *devq = NULL;
    struct cam_sim *sim = NULL;
    struct cam_path *path = NULL;
    int unit, error;

    scp = (struct atapi_xpt_softc *)device_get_softc(dev);
    if (scp == NULL) {
	device_printf(dev, "Cannot get softc\n");
	return (ENOMEM);
    }

    mtx_init(&scp->state_lock, "ATAPICAM lock", NULL, MTX_DEF);

    scp->dev = dev;
    scp->parent = device_get_parent(dev);
    scp->ata_ch = device_get_softc(scp->parent);
    TAILQ_INIT(&scp->pending_hcbs);
    unit = device_get_unit(dev);

    if ((devq = cam_simq_alloc(16)) == NULL) {
	error = ENOMEM;
	goto out;
    }

    if ((sim = cam_sim_alloc(atapi_action, atapi_poll, "ata",
		 (void *)scp, unit, &scp->state_lock, 1, 1, devq)) == NULL) {
	error = ENOMEM;
	goto out;
    }
    scp->sim = sim;

    mtx_lock(&scp->state_lock);
    if (xpt_bus_register(sim, dev, 0) != CAM_SUCCESS) {
	error = EINVAL;
	mtx_unlock(&scp->state_lock);
	goto out;
    }
    scp->flags |= BUS_REGISTERED;

    if (xpt_create_path(&path, /*periph*/ NULL,
		cam_sim_path(sim), CAM_TARGET_WILDCARD,
		CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	error = ENOMEM;
	mtx_unlock(&scp->state_lock);
	goto out;
    }
    scp->path = path;

    CAM_DEBUG(path, CAM_DEBUG_TRACE, ("Registered SIM for ata%d\n", unit));

    setup_async_cb(scp, AC_LOST_DEVICE);
    reinit_bus(scp, cold ? BOOT_ATTACH : ATTACH);
    error = 0;
    mtx_unlock(&scp->state_lock);

out:
    if (error != 0)
	free_softc(scp);

    return (error);
}

static int
atapi_cam_detach(device_t dev)
{
    struct atapi_xpt_softc *scp = device_get_softc(dev);

    mtx_lock(&scp->state_lock);
    xpt_freeze_simq(scp->sim, 1 /*count*/);
    scp->flags |= DETACHING;
    mtx_unlock(&scp->state_lock);
    free_softc(scp);
    return (0);
}

static int
atapi_cam_reinit(device_t dev) {
    struct atapi_xpt_softc *scp = device_get_softc(dev);

    /*
     * scp might be null if the bus is being reinitialised during
     * the boot-up sequence, before the ATAPI bus is registered.
     */

    if (scp != NULL) {
	mtx_lock(&scp->state_lock);
	reinit_bus(scp, RESET);
	mtx_unlock(&scp->state_lock);
    }
    return (0);
}

static void
reinit_bus(struct atapi_xpt_softc *scp, enum reinit_reason reason) {
    struct ata_device *old_atadev[2], *atadev;
    device_t *children;
    int nchildren, i, dev_changed;

    if (device_get_children(scp->parent, &children, &nchildren) != 0) {
	return;
    }

    old_atadev[0] = scp->atadev[0];
    old_atadev[1] = scp->atadev[1];
    scp->atadev[0] = NULL;
    scp->atadev[1] = NULL;

    for (i = 0; i < nchildren; i++) {
	/* XXX Does the child need to actually be attached yet? */
	if (children[i] != NULL) {
	    atadev = device_get_softc(children[i]);
	    if ((atadev->unit == ATA_MASTER) &&
		(scp->ata_ch->devices & ATA_ATAPI_MASTER) != 0)
		scp->atadev[0] = atadev;
	    if ((atadev->unit == ATA_SLAVE) &&
		(scp->ata_ch->devices & ATA_ATAPI_SLAVE) != 0)
		scp->atadev[1] = atadev;
	}
    }
    dev_changed = (old_atadev[0] != scp->atadev[0])
	       || (old_atadev[1] != scp->atadev[1]);
    free(children, M_TEMP);

    switch (reason) {
	case BOOT_ATTACH:
	    break;
	case RESET:
	    xpt_async(AC_BUS_RESET, scp->path, NULL);

	    if (!dev_changed)
		break;

	    /*FALLTHROUGH*/
	case ATTACH:
	    cam_rescan(scp->sim);
	    break;
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
    struct ata_request *request = NULL;
    int unit = cam_sim_unit(sim);
    int bus = cam_sim_bus(sim);
    int len;
    char *buf;

    switch (ccb_h->func_code) {
    case XPT_PATH_INQ: {
	struct ccb_pathinq *cpi = &ccb->cpi;
	int tid = ccb_h->target_id;

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
	cpi->transport = XPORT_ATA;
	cpi->transport_version = 2;
	cpi->protocol = PROTO_SCSI;
	cpi->protocol_version = SCSI_REV_2;

	if (softc->ata_ch && tid != CAM_TARGET_WILDCARD) {
	    if (softc->atadev[tid] == NULL) {
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		xpt_done(ccb);
		return;
	    }
	    switch (softc->atadev[ccb_h->target_id]->mode) {
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

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("dev reset\n"));
	mtx_unlock(&softc->state_lock);
	ata_controlcmd(softc->atadev[tid]->dev, ATA_DEVICE_RESET, 0, 0, 0);
	mtx_lock(&softc->state_lock);
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
	return;
    }

    case XPT_RESET_BUS:
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("bus reset\n"));
	mtx_unlock(&softc->state_lock);
	ata_reinit(softc->parent);
	mtx_lock(&softc->state_lock);
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
	cts->protocol = PROTO_SCSI;
	cts->protocol_version = SCSI_REV_2;
	cts->transport = XPORT_ATA;
	cts->transport_version = XPORT_VERSION_UNSPECIFIED;
    	cts->proto_specific.valid = 0;
    	cts->xport_specific.valid = 0;
	/* nothing more to do */
	ccb->ccb_h.status = CAM_REQ_CMP;
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE, ("GET_TRAN_SETTINGS\n"));
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
	int request_flags = ATA_R_ATAPI;

	CAM_DEBUG(ccb_h->path, CAM_DEBUG_SUBTRACE, ("XPT_SCSI_IO\n"));

	if (softc->flags & DETACHING) {
	    ccb->ccb_h.status = CAM_REQ_ABORTED;
	    xpt_done(ccb);
	    return;
	}

	if (softc->atadev[tid] == NULL) {
	    ccb->ccb_h.status = CAM_DEV_NOT_THERE;
	    xpt_done(ccb);
	    return;
	}

	/* check that this request was not aborted already */
	if ((ccb_h->status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
	    printf("XPT_SCSI_IO received but already in progress?\n");
	    xpt_done(ccb);
	    return;
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
	     request_flags |= ATA_R_READ;
	     break;
	case CAM_DIR_OUT:
	     request_flags |= ATA_R_WRITE;
	     break;
	case CAM_DIR_NONE:
	     /* No flags need to be set */
	     break;
	default:
	     device_printf(softc->dev, "unknown IO operation\n");
	     goto action_invalid;
	}

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
	if (CAM_DEBUGGED(ccb_h->path, CAM_DEBUG_SUBTRACE)) {
		request_flags |= ATA_R_DEBUG;
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
	    /* FALLTHROUGH */

	case READ_10:
	    /* FALLTHROUGH */
	case WRITE_10:
	    /* FALLTHROUGH */
	case READ_12:
	    /* FALLTHROUGH */
	case WRITE_12:
	    /*
	     * Enable DMA (if target supports it) for READ and WRITE commands
	     * only, as some combinations of drive, controller and chipset do
	     * not behave correctly when DMA is enabled for other commands.
	     */
	    if (softc->atadev[tid]->mode >= ATA_DMA)
		request_flags |= ATA_R_DMA;
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
	request->dev = softc->atadev[tid]->dev;
	request->driver = hcb;
	request->data = buf;
	request->bytecount = len;
	request->transfersize = min(request->bytecount, 65534);
	request->timeout = ccb_h->timeout / 1000; /* XXX lost granularity */
	request->callback = &atapi_cb;
	request->flags = request_flags;

	/*
	 * no retries are to be performed at the ATA level; any retries
	 * will be done by CAM.
	 */
	request->retries = 0;

	TAILQ_INSERT_TAIL(&softc->pending_hcbs, hcb, chain);
	hcb->flags |= QUEUED;
	ccb_h->status |= CAM_SIM_QUEUED;
	mtx_unlock(&softc->state_lock);

	ata_queue_request(request);
	mtx_lock(&softc->state_lock);
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
    mtx_unlock(&softc->state_lock);
    return;

action_invalid:
    ccb_h->status = CAM_REQ_INVALID;
    xpt_done(ccb);
    mtx_unlock(&softc->state_lock);
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
    struct atapi_xpt_softc *scp;
    struct atapi_hcb *hcb;
    struct ccb_scsiio *csio;
    u_int32_t rc;

    hcb = (struct atapi_hcb *)request->driver;
    scp = hcb->softc;
    csio = &hcb->ccb->csio;

#ifdef CAMDEBUG
# define err (request->u.atapi.sense.key)
    if (CAM_DEBUGGED(csio->ccb_h.path, CAM_DEBUG_CDB)) {
	printf("atapi_cb: hcb@%p sense = %02x: sk = %01x%s%s%s\n",
	       hcb, err, err & 0x0f,
	       (err & 0x80) ? ", Filemark" : "",
	       (err & 0x40) ? ", EOM" : "",
	       (err & 0x20) ? ", ILI" : "");
	device_printf(request->dev,
            "cmd %s status %02x result %02x error %02x\n",
	    ata_cmd2str(request),
	    request->status, request->result, request->error);
    }
#endif

    if ((hcb->flags & AUTOSENSE) != 0) {
	rc = CAM_SCSI_STATUS_ERROR;
	if (request->result == 0) {
	    csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	}
    } else if (request->result != 0) {
	if ((request->flags & ATA_R_TIMEOUT) != 0) {
	    rc = CAM_CMD_TIMEOUT;
	} else {
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

		ata_queue_request(request);
		return;
#else
		/*
		 * Use auto-sense data from the ATA layer, if it has
		 * issued a REQUEST SENSE automatically and that operation
		 * returned without error.
		 */
		if (request->u.atapi.sense.key != 0 && request->error == 0) {
		    bcopy (&request->u.atapi.sense, &csio->sense_data, sizeof(struct atapi_sense));
		    csio->ccb_h.status |= CAM_AUTOSNS_VALID;
		}
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

    mtx_lock(&scp->state_lock);
    free_hcb_and_ccb_done(hcb, rc);
    mtx_unlock(&scp->state_lock);

    ata_free_request(request);
}

static void
free_hcb_and_ccb_done(struct atapi_hcb *hcb, u_int32_t status)
{
    struct atapi_xpt_softc *softc;
    union ccb *ccb;

    if (hcb == NULL)
	return;

    softc = hcb->softc;
    ccb = hcb->ccb;

    /* we're about to free a hcb, so the shortage has ended */
    if (softc->flags & RESOURCE_SHORTAGE) {
	softc->flags &= ~RESOURCE_SHORTAGE;
	status |= CAM_RELEASE_SIMQ;
    }
    free_hcb(hcb);
    ccb->ccb_h.status =
	status | (ccb->ccb_h.status & ~(CAM_STATUS_MASK | CAM_SIM_QUEUED));
    xpt_done(ccb);
}

static void
atapi_async(void *callback_arg, u_int32_t code,
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
	xpt_free_ccb(ccb);
}

static void
cam_rescan(struct cam_sim *sim)
{
    struct cam_path *path;
    union ccb *ccb;

    ccb = xpt_alloc_ccb_nowait();
    if (ccb == NULL)
	return;

    if (xpt_create_path(&path, xpt_periph, cam_sim_path(sim),
			CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
	xpt_free_ccb(ccb);
	return;
    }

    CAM_DEBUG(path, CAM_DEBUG_TRACE, ("Rescanning ATAPI bus.\n"));
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

    if (scp != NULL) {
	mtx_lock(&scp->state_lock);
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
	mtx_destroy(&scp->state_lock);
    }
}

static int
atapi_cam_event_handler(module_t mod, int what, void *arg) {
    device_t *devlist;
    int devcount;

    switch (what) {
	case MOD_UNLOAD:
	    if (devclass_get_devices(atapi_cam_devclass, &devlist, &devcount)
		  != 0)
		return ENXIO;
	    if (devlist != NULL) {
		while (devlist != NULL && devcount > 0) {
		    device_t child = devlist[--devcount];
		    struct atapi_xpt_softc *scp = device_get_softc(child);

		    device_delete_child(device_get_parent(child),child);
		    if (scp != NULL)
			free(scp, M_ATACAM);
		}
		free(devlist, M_TEMP);
	    }
	    break;

	default:
	    break;
    }
    return 0;
}
