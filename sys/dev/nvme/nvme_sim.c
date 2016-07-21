/*-
 * Copyright (c) 2016 Netflix, Inc
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
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_internal.h>	// Yes, this is wrong.
#include <cam/cam_debug.h>

#include "nvme_private.h"

#define ccb_accb_ptr spriv_ptr0
#define ccb_ctrlr_ptr spriv_ptr1
static void	nvme_sim_action(struct cam_sim *sim, union ccb *ccb);
static void	nvme_sim_poll(struct cam_sim *sim);

#define sim2softc(sim)	((struct nvme_sim_softc *)cam_sim_softc(sim))
#define sim2ns(sim)	(sim2softc(sim)->s_ns)
#define sim2ctrlr(sim)	(sim2softc(sim)->s_ctrlr)

struct nvme_sim_softc
{
	struct nvme_controller	*s_ctrlr;
	struct nvme_namespace	*s_ns;
	struct cam_sim		*s_sim;
	struct cam_path		*s_path;
};

static void
nvme_sim_nvmeio_done(void *ccb_arg, const struct nvme_completion *cpl)
{
	union ccb *ccb = (union ccb *)ccb_arg;

	/*
	 * Let the periph know the completion, and let it sort out what
	 * it means. Make our best guess, though for the status code.
	 */
	memcpy(&ccb->nvmeio.cpl, cpl, sizeof(*cpl));
	if (nvme_completion_is_error(cpl))
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
	else
		ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
}

static void
nvme_sim_nvmeio(struct cam_sim *sim, union ccb *ccb)
{
	struct ccb_nvmeio	*nvmeio = &ccb->nvmeio;
	struct nvme_request	*req;
	void			*payload;
	uint32_t		size;
	struct nvme_controller *ctrlr;

	ctrlr = sim2ctrlr(sim);
	payload = nvmeio->data_ptr;
	size = nvmeio->dxfer_len;
	/* SG LIST ??? */
	if ((nvmeio->ccb_h.flags & CAM_DATA_MASK) == CAM_DATA_BIO)
		req = nvme_allocate_request_bio((struct bio *)payload,
		    nvme_sim_nvmeio_done, ccb);
	else if (payload == NULL)
		req = nvme_allocate_request_null(nvme_sim_nvmeio_done, ccb);
	else
		req = nvme_allocate_request_vaddr(payload, size,
		    nvme_sim_nvmeio_done, ccb);

	if (req == NULL) {
		nvmeio->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	memcpy(&req->cmd, &ccb->nvmeio.cmd, sizeof(ccb->nvmeio.cmd));

	nvme_ctrlr_submit_io_request(ctrlr, req);

	ccb->ccb_h.status |= CAM_SIM_QUEUED;
}

static void
nvme_sim_action(struct cam_sim *sim, union ccb *ccb)
{
	struct nvme_controller *ctrlr;
	struct nvme_namespace *ns;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("nvme_sim_action: func= %#x\n",
		ccb->ccb_h.func_code));

	/*
	 * XXX when we support multiple namespaces in the base driver we'll need
	 * to revisit how all this gets stored and saved in the periph driver's
	 * reserved areas. Right now we store all three in the softc of the sim.
	 */
	ns = sim2ns(sim);
	ctrlr = sim2ctrlr(sim);

	printf("Sim action: ctrlr %p ns %p\n", ctrlr, ns);

	mtx_assert(&ctrlr->lock, MA_OWNED);

	switch (ccb->ccb_h.func_code) {
	case XPT_CALC_GEOMETRY:		/* Calculate Geometry Totally nuts ? XXX */
		/* 
		 * Only meaningful for old-school SCSI disks since only the SCSI
		 * da driver generates them. Reject all these that slip through.
		 */
		/*FALLTHROUGH*/
	case XPT_ABORT:			/* Abort the specified CCB */
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
		/*
		 * Only target mode generates these, and only for SCSI. They are
		 * all invalid/unsupported for NVMe.
		 */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	case XPT_SET_TRAN_SETTINGS:
		/*
		 * NVMe doesn't really have different transfer settings, but
		 * other parts of CAM think failure here is a big deal.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		/*
		 * NVMe may have multiple LUNs on the same path. Current generation
		 * of NVMe devives support only a single name space. Multiple name
		 * space drives are coming, but it's unclear how we should report
		 * them up the stack.
		 */
		cpi->version_num = 1;
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc =  PIM_UNMAPPED /* | PIM_NOSCAN */;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 0;
		cpi->max_lun = ctrlr->cdata.nn;
		cpi->maxio = nvme_ns_get_max_io_xfer_size(ns);
		cpi->initiator_id = 0;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 4000000;	/* 4 GB/s 4 lanes pcie 3 */
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "NVMe", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
                cpi->transport = XPORT_NVME;		/* XXX XPORT_PCIE ? */
                cpi->transport_version = 1;		/* XXX Get PCIe spec ? */
                cpi->protocol = PROTO_NVME;
                cpi->protocol_version = NVME_REV_1;	/* Groks all 1.x NVMe cards */
		cpi->xport_specific.nvme.nsid = ns->id;
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_GET_TRAN_SETTINGS:	/* Get transport settings */
	{
		struct ccb_trans_settings	*cts;
		struct ccb_trans_settings_nvme	*nvmep;
		struct ccb_trans_settings_nvme	*nvmex;

		cts = &ccb->cts;
		nvmex = &cts->xport_specific.nvme;
		nvmep = &cts->proto_specific.nvme;

		nvmex->valid = CTS_NVME_VALID_SPEC;
		nvmex->spec_major = 1;			/* XXX read from card */
		nvmex->spec_minor = 2;
		nvmex->spec_tiny = 0;

		nvmep->valid = CTS_NVME_VALID_SPEC;
		nvmep->spec_major = 1;			/* XXX read from card */
		nvmep->spec_minor = 2;
		nvmep->spec_tiny = 0;
		cts->transport = XPORT_NVME;
		cts->protocol = PROTO_NVME;
		cts->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/*
		 * every driver handles this, but nothing generates it. Assume
		 * it's OK to just say 'that worked'.
		 */
		/*FALLTHROUGH*/
	case XPT_RESET_DEV:		/* Bus Device Reset the specified device */
	case XPT_RESET_BUS:		/* Reset the specified bus */
		/*
		 * NVMe doesn't really support physically resetting the bus. It's part
		 * of the bus scanning dance, so return sucess to tell the process to
		 * proceed.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_NVME_IO:		/* Execute the requested I/O operation */
		nvme_sim_nvmeio(sim, ccb);
		return;			/* no done */
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	xpt_done(ccb);
}

static void
nvme_sim_poll(struct cam_sim *sim)
{

	nvme_ctrlr_intx_handler(sim2ctrlr(sim));
}

static void *
nvme_sim_new_controller(struct nvme_controller *ctrlr)
{
	struct cam_devq *devq;
	int max_trans;
	int unit;
	struct nvme_sim_softc *sc = NULL;

	max_trans = 256;/* XXX not so simple -- must match queues */
	unit = device_get_unit(ctrlr->dev);
	devq = cam_simq_alloc(max_trans);
	if (devq == NULL)
		return NULL;

	sc = malloc(sizeof(*sc), M_NVME, M_ZERO | M_WAITOK);

	sc->s_ctrlr = ctrlr;

	sc->s_sim = cam_sim_alloc(nvme_sim_action, nvme_sim_poll,
	    "nvme", sc, unit, &ctrlr->lock, max_trans, max_trans, devq);
	if (sc->s_sim == NULL) {
		printf("Failed to allocate a sim\n");
		cam_simq_free(devq);
		free(sc, M_NVME);
		return NULL;
	}

	return sc;
}

static void
nvme_sim_rescan_target(struct nvme_controller *ctrlr, struct cam_path *path)
{
	union ccb *ccb;

	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		printf("unable to alloc CCB for rescan\n");
		return;
	}

	if (xpt_clone_path(&ccb->ccb_h.path, path) != CAM_REQ_CMP) {
		printf("unable to copy path for rescan\n");
		xpt_free_ccb(ccb);
		return;
	}

	xpt_rescan(ccb);
}
	
static void *
nvme_sim_new_ns(struct nvme_namespace *ns, void *sc_arg)
{
	struct nvme_sim_softc *sc = sc_arg;
	struct nvme_controller *ctrlr = sc->s_ctrlr;
	int i;

	sc->s_ns = ns;

	printf("Our SIM's softc %p ctrlr %p ns %p\n", sc, ctrlr, ns);

	/*
	 * XXX this is creating one bus per ns, but it should be one
	 * XXX target per controller, and one LUN per namespace.
	 * XXX Current drives only support one NS, so there's time
	 * XXX to fix it later when new drives arrive.
	 *
	 * XXX I'm pretty sure the xpt_bus_register() call below is
	 * XXX like super lame and it really belongs in the sim_new_ctrlr
	 * XXX callback. Then the create_path below would be pretty close
	 * XXX to being right. Except we should be per-ns not per-ctrlr
	 * XXX data.
	 */

	mtx_lock(&ctrlr->lock);
/* Create bus */

	/*
	 * XXX do I need to lock ctrlr->lock ? 
	 * XXX do I need to lock the path?
	 * ata and scsi seem to in their code, but their discovery is
	 * somewhat more asynchronous. We're only every called one at a
	 * time, and nothing is in parallel.
	 */

	i = 0;
	if (xpt_bus_register(sc->s_sim, ctrlr->dev, 0) != CAM_SUCCESS)
		goto error;
	i++;
	if (xpt_create_path(&sc->s_path, /*periph*/NULL, cam_sim_path(sc->s_sim),
	    1, ns->id) != CAM_REQ_CMP)
		goto error;
	i++;

	sc->s_path->device->nvme_data = nvme_ns_get_data(ns);
	sc->s_path->device->nvme_cdata = nvme_ctrlr_get_data(ns->ctrlr);

/* Scan bus */
	printf("Initiate rescan of the bus\n");
	nvme_sim_rescan_target(ctrlr, sc->s_path);

	mtx_unlock(&ctrlr->lock);

	return ns;

error:
	switch (i) {
	case 2:
		xpt_free_path(sc->s_path);
	case 1:
		xpt_bus_deregister(cam_sim_path(sc->s_sim));
	case 0:
		cam_sim_free(sc->s_sim, /*free_devq*/TRUE);
	}
	mtx_unlock(&ctrlr->lock);
	return NULL;
}

static void
nvme_sim_controller_fail(void *ctrlr_arg)
{
	/* XXX cleanup XXX */
}

struct nvme_consumer *consumer_cookie;

static void
nvme_sim_init(void)
{

	consumer_cookie = nvme_register_consumer(nvme_sim_new_ns,
	    nvme_sim_new_controller, NULL, nvme_sim_controller_fail);
}

SYSINIT(nvme_sim_register, SI_SUB_DRIVERS, SI_ORDER_ANY,
    nvme_sim_init, NULL);

static void
nvme_sim_uninit(void)
{
	/* XXX Cleanup */

	nvme_unregister_consumer(consumer_cookie);
}

SYSUNINIT(nvme_sim_unregister, SI_SUB_DRIVERS, SI_ORDER_ANY,
    nvme_sim_uninit, NULL);
