/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/refcount.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <dev/nvmf/host/nvmf_var.h>

/*
 * The I/O completion may trigger after the received CQE if the I/O
 * used a zero-copy mbuf that isn't harvested until after the NIC
 * driver processes TX completions.  Use spriv_field0 to as a refcount.
 *
 * Store any I/O error returned in spriv_field1.
 */
static __inline u_int *
ccb_refs(union ccb *ccb)
{
	return ((u_int *)&ccb->ccb_h.spriv_field0);
}

#define	spriv_ioerror	spriv_field1

static void
nvmf_ccb_done(union ccb *ccb)
{
	if (!refcount_release(ccb_refs(ccb)))
		return;

	if (nvmf_cqe_aborted(&ccb->nvmeio.cpl)) {
		struct cam_sim *sim = xpt_path_sim(ccb->ccb_h.path);
		struct nvmf_softc *sc = cam_sim_softc(sim);

		if (nvmf_fail_disconnect || sc->sim_shutdown)
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		else
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
	} else if (ccb->nvmeio.cpl.status != 0) {
		ccb->ccb_h.status = CAM_NVME_STATUS_ERROR;
		xpt_done(ccb);
	} else if (ccb->ccb_h.spriv_ioerror != 0) {
		KASSERT(ccb->ccb_h.spriv_ioerror != EJUSTRETURN,
		    ("%s: zero sized transfer without CQE error", __func__));
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
	} else {
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
	}
}

static void
nvmf_ccb_io_complete(void *arg, size_t xfered, int error)
{
	union ccb *ccb = arg;

	/*
	 * TODO: Reporting partial completions requires extending
	 * nvmeio to support resid and updating nda to handle partial
	 * reads, either by returning partial success (or an error) to
	 * the caller, or retrying all or part of the request.
	 */
	ccb->ccb_h.spriv_ioerror = error;
	if (error == 0) {
		if (xfered == 0) {
#ifdef INVARIANTS
			/*
			 * If the request fails with an error in the CQE
			 * there will be no data transferred but also no
			 * I/O error.
			 */
			ccb->ccb_h.spriv_ioerror = EJUSTRETURN;
#endif
		} else
			KASSERT(xfered == ccb->nvmeio.dxfer_len,
			    ("%s: partial CCB completion", __func__));
	}

	nvmf_ccb_done(ccb);
}

static void
nvmf_ccb_complete(void *arg, const struct nvme_completion *cqe)
{
	union ccb *ccb = arg;

	ccb->nvmeio.cpl = *cqe;
	nvmf_ccb_done(ccb);
}

static void
nvmf_sim_io(struct nvmf_softc *sc, union ccb *ccb)
{
	struct ccb_nvmeio *nvmeio = &ccb->nvmeio;
	struct memdesc mem;
	struct nvmf_request *req;
	struct nvmf_host_qpair *qp;

	mtx_lock(&sc->sim_mtx);
	if (sc->sim_disconnected) {
		mtx_unlock(&sc->sim_mtx);
		if (nvmf_fail_disconnect || sc->sim_shutdown)
			nvmeio->ccb_h.status = CAM_DEV_NOT_THERE;
		else
			nvmeio->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}
	if (nvmeio->ccb_h.func_code == XPT_NVME_IO)
		qp = nvmf_select_io_queue(sc);
	else
		qp = sc->admin;
	req = nvmf_allocate_request(qp, &nvmeio->cmd, nvmf_ccb_complete,
	    ccb, M_NOWAIT);
	mtx_unlock(&sc->sim_mtx);
	if (req == NULL) {
		nvmeio->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	if (nvmeio->dxfer_len != 0) {
		refcount_init(ccb_refs(ccb), 2);
		mem = memdesc_ccb(ccb);
		nvmf_capsule_append_data(req->nc, &mem, nvmeio->dxfer_len,
		    (ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT,
		    nvmf_ccb_io_complete, ccb);
	} else
		refcount_init(ccb_refs(ccb), 1);

	/*
	 * Clear spriv_ioerror as it can hold an earlier error if this
	 * CCB was aborted and has been retried.
	 */
	ccb->ccb_h.spriv_ioerror = 0;
	KASSERT(ccb->ccb_h.status == CAM_REQ_INPROG,
	    ("%s: incoming CCB is not in-progress", __func__));
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	nvmf_submit_request(req);
}

static void
nvmf_sim_action(struct cam_sim *sim, union ccb *ccb)
{
	struct nvmf_softc *sc = cam_sim_softc(sim);

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("nvmf_sim_action: func= %#x\n",
		ccb->ccb_h.func_code));

	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ:	/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc =  PIM_UNMAPPED | PIM_NOSCAN;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 0;
		cpi->max_lun = sc->cdata->nn;
		cpi->async_flags = 0;
		cpi->hpath_id = 0;
		cpi->initiator_id = 0;
		strlcpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strlcpy(cpi->hba_vid, "NVMeoF", HBA_IDLEN);
		strlcpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = 0;

		/* XXX: Same as iSCSI. */
		cpi->base_transfer_speed = 150000;
		cpi->protocol = PROTO_NVME;
		cpi->protocol_version = sc->vs;
		cpi->transport = XPORT_NVMF;
		cpi->transport_version = sc->vs;
		cpi->xport_specific.nvmf.nsid =
		    xpt_path_lun_id(ccb->ccb_h.path);
		cpi->xport_specific.nvmf.trtype = sc->trtype;
		strlcpy(cpi->xport_specific.nvmf.dev_name,
		    device_get_nameunit(sc->dev),
		    sizeof(cpi->xport_specific.nvmf.dev_name));
		cpi->maxio = sc->max_xfer_size;
		cpi->hba_vendor = 0;
		cpi->hba_device = 0;
		cpi->hba_subvendor = 0;
		cpi->hba_subdevice = 0;
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_GET_TRAN_SETTINGS:	/* Get transport settings */
	{
		struct ccb_trans_settings *cts = &ccb->cts;
		struct ccb_trans_settings_nvme *nvme;
		struct ccb_trans_settings_nvmf *nvmf;

		cts->protocol = PROTO_NVME;
		cts->protocol_version = sc->vs;
		cts->transport = XPORT_NVMF;
		cts->transport_version = sc->vs;

		nvme = &cts->proto_specific.nvme;
		nvme->valid = CTS_NVME_VALID_SPEC;
		nvme->spec = sc->vs;

		nvmf = &cts->xport_specific.nvmf;
		nvmf->valid = CTS_NVMF_VALID_TRTYPE;
		nvmf->trtype = sc->trtype;
		cts->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_SET_TRAN_SETTINGS:	/* Set transport settings */
		/*
		 * No transfer settings can be set, but nvme_xpt sends
		 * this anyway.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_NVME_IO:		/* Execute the requested I/O */
	case XPT_NVME_ADMIN:		/* or Admin operation */
		nvmf_sim_io(sc, ccb);
		return;
	default:
		/* XXX */
		device_printf(sc->dev, "unhandled sim function %#x\n",
		    ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	xpt_done(ccb);
}

int
nvmf_init_sim(struct nvmf_softc *sc)
{
	struct cam_devq *devq;
	int max_trans;

	max_trans = sc->max_pending_io * 3 / 4;
	devq = cam_simq_alloc(max_trans);
	if (devq == NULL) {
		device_printf(sc->dev, "Failed to allocate CAM simq\n");
		return (ENOMEM);
	}

	mtx_init(&sc->sim_mtx, "nvmf sim", NULL, MTX_DEF);
	sc->sim = cam_sim_alloc(nvmf_sim_action, NULL, "nvme", sc,
	    device_get_unit(sc->dev), NULL, max_trans, max_trans, devq);
	if (sc->sim == NULL) {
		device_printf(sc->dev, "Failed to allocate CAM sim\n");
		cam_simq_free(devq);
		mtx_destroy(&sc->sim_mtx);
		return (ENXIO);
	}
	if (xpt_bus_register(sc->sim, sc->dev, 0) != CAM_SUCCESS) {
		device_printf(sc->dev, "Failed to create CAM bus\n");
		cam_sim_free(sc->sim, TRUE);
		mtx_destroy(&sc->sim_mtx);
		return (ENXIO);
	}
	if (xpt_create_path(&sc->path, NULL, cam_sim_path(sc->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		device_printf(sc->dev, "Failed to create CAM path\n");
		xpt_bus_deregister(cam_sim_path(sc->sim));
		cam_sim_free(sc->sim, TRUE);
		mtx_destroy(&sc->sim_mtx);
		return (ENXIO);
	}
	return (0);
}

void
nvmf_sim_rescan_ns(struct nvmf_softc *sc, uint32_t id)
{
	union ccb *ccb;

	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		device_printf(sc->dev,
		    "unable to alloc CCB for rescan of namespace %u\n", id);
		return;
	}

	/*
	 * As with nvme_sim, map NVMe namespace IDs onto CAM unit
	 * LUNs.
	 */
	if (xpt_create_path(&ccb->ccb_h.path, NULL, cam_sim_path(sc->sim), 0,
	    id) != CAM_REQ_CMP) {
		device_printf(sc->dev,
		    "Unable to create path for rescan of namespace %u\n", id);
		xpt_free_ccb(ccb);
		return;
	}
	xpt_rescan(ccb);
}

void
nvmf_disconnect_sim(struct nvmf_softc *sc)
{
	mtx_lock(&sc->sim_mtx);
	sc->sim_disconnected = true;
	xpt_freeze_simq(sc->sim, 1);
	mtx_unlock(&sc->sim_mtx);
}

void
nvmf_reconnect_sim(struct nvmf_softc *sc)
{
	mtx_lock(&sc->sim_mtx);
	sc->sim_disconnected = false;
	mtx_unlock(&sc->sim_mtx);
	xpt_release_simq(sc->sim, 1);
}

void
nvmf_shutdown_sim(struct nvmf_softc *sc)
{
	mtx_lock(&sc->sim_mtx);
	sc->sim_shutdown = true;
	mtx_unlock(&sc->sim_mtx);
	xpt_release_simq(sc->sim, 1);
}

void
nvmf_destroy_sim(struct nvmf_softc *sc)
{
	xpt_async(AC_LOST_DEVICE, sc->path, NULL);
	if (sc->sim_disconnected)
		xpt_release_simq(sc->sim, 1);
	xpt_free_path(sc->path);
	xpt_bus_deregister(cam_sim_path(sc->sim));
	cam_sim_free(sc->sim, TRUE);
	mtx_destroy(&sc->sim_mtx);
}
