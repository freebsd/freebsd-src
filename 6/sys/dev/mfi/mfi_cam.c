/*-
 * Copyright 2007 Scott Long
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
__FBSDID("$FreeBSD$");

#include "opt_mfi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/selinfo.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/rman.h>
#include <sys/bus_dma.h>
#include <sys/bio.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

struct mfip_softc {
	device_t	dev;
	struct mfi_softc *mfi_sc;
	struct cam_devq *devq;
	struct cam_sim	*sim;
	struct cam_path	*path;
};

static int	mfip_probe(device_t);
static int	mfip_attach(device_t);
static int	mfip_detach(device_t);
static void	mfip_cam_action(struct cam_sim *, union ccb *);
static void	mfip_cam_poll(struct cam_sim *);
static struct mfi_command * mfip_start(void *);
static void	mfip_done(struct mfi_command *cm);

static devclass_t	mfip_devclass;
static device_method_t	mfip_methods[] = {
	DEVMETHOD(device_probe,		mfip_probe),
	DEVMETHOD(device_attach,	mfip_attach),
	DEVMETHOD(device_detach,	mfip_detach),
	{0, 0}
};
static driver_t mfip_driver = {
	"mfip",
	mfip_methods,
	sizeof(struct mfip_softc)
};
DRIVER_MODULE(mfip, mfi, mfip_driver, mfip_devclass, 0, 0);
MODULE_DEPEND(mfip, cam, 1, 1, 1);

#define ccb_mfip_ptr sim_priv.entries[0].ptr

static int
mfip_probe(device_t dev)
{

	device_set_desc(dev, "SCSI Passthrough Bus");
	return (0);
}

static int
mfip_attach(device_t dev)
{
	struct mfip_softc *sc;
	struct mfi_softc *mfisc;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);

	mfisc = device_get_softc(device_get_parent(dev));
	sc->dev = dev;
	sc->mfi_sc = mfisc;
	mfisc->mfi_cam_start = mfip_start;

	if ((sc->devq = cam_simq_alloc(MFI_SCSI_MAX_CMDS)) == NULL)
		return (ENOMEM);

	sc->sim = cam_sim_alloc(mfip_cam_action, mfip_cam_poll, "mfi", sc,
				device_get_unit(dev), 1,
				MFI_SCSI_MAX_CMDS, sc->devq);
	if (sc->sim == NULL) {
		cam_simq_free(sc->devq);
		device_printf(dev, "CAM SIM attach failed\n");
		return (EINVAL);
	}

	if (xpt_bus_register(sc->sim, 0) != 0) {
		device_printf(dev, "XPT bus registration failed\n");
		cam_sim_free(sc->sim, FALSE);
		cam_simq_free(sc->devq);
		return (EINVAL);
	}

	return (0);
}

static int
mfip_detach(device_t dev)
{
	struct mfip_softc *sc;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);

	if (sc->sim != NULL) {
		xpt_bus_deregister(cam_sim_path(sc->sim));
		cam_sim_free(sc->sim, FALSE);
	}

	if (sc->devq != NULL)
		cam_simq_free(sc->devq);

	return (0);
}

static void
mfip_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct mfip_softc *sc = cam_sim_softc(sim);
	struct mfi_softc *mfisc = sc->mfi_sc;

	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = MFI_SCSI_MAX_TARGETS;
		cpi->max_lun = MFI_SCSI_MAX_LUNS;
		cpi->initiator_id = MFI_SCSI_INITIATOR_ID;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "LSI", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 150000;
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_RESET_BUS:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_RESET_DEV:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_GET_TRAN_SETTINGS:
	{
		ccb->cts.flags &= ~(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
		ccb->cts.valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		break;
	case XPT_SCSI_IO:
	{
		struct ccb_hdr		*ccbh = &ccb->ccb_h;
		struct ccb_scsiio	*csio = &ccb->csio;

		ccbh->status = CAM_REQ_INPROG;
		if (csio->cdb_len > MFI_SCSI_MAX_CDB_LEN) {
			ccbh->status = CAM_REQ_INVALID;
			break;
		}
		if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			if (ccbh->flags & CAM_DATA_PHYS) {
				ccbh->status = CAM_REQ_INVALID;
				break;
			}
			if (ccbh->flags & CAM_SCATTER_VALID) {
				ccbh->status = CAM_REQ_INVALID;
				break;
			}
		}

		ccbh->ccb_mfip_ptr = sc;
		TAILQ_INSERT_TAIL(&mfisc->mfi_cam_ccbq, ccbh, sim_links.tqe);
		mfi_startio(mfisc);
		return;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}

	xpt_done(ccb);
	return;
}

static struct mfi_command *
mfip_start(void *data)
{
	union ccb *ccb = data;
	struct ccb_hdr *ccbh = &ccb->ccb_h;
	struct ccb_scsiio *csio = &ccb->csio;
	struct mfip_softc *sc;
	struct mfi_pass_frame *pt;
	struct mfi_command *cm;

	sc = ccbh->ccb_mfip_ptr;

	if ((cm = mfi_dequeue_free(sc->mfi_sc)) == NULL)
		return (NULL);

	pt = &cm->cm_frame->pass;
	pt->header.cmd = MFI_CMD_PD_SCSI_IO;
	pt->header.cmd_status = 0;
	pt->header.scsi_status = 0;
	pt->header.target_id = ccbh->target_id;
	pt->header.lun_id = ccbh->target_lun;
	pt->header.flags = 0;
	pt->header.timeout = 0;
	pt->header.data_len = csio->dxfer_len;
	pt->header.sense_len = MFI_SENSE_LEN;
	pt->header.cdb_len = csio->cdb_len;
	pt->sense_addr_lo = cm->cm_sense_busaddr;
	pt->sense_addr_hi = 0;
	if (ccbh->flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, &pt->cdb[0], csio->cdb_len);
	else
		bcopy(csio->cdb_io.cdb_bytes, &pt->cdb[0], csio->cdb_len);
	cm->cm_complete = mfip_done;
	cm->cm_private = ccb;
	cm->cm_sg = &pt->sgl;
	cm->cm_total_frame_size = MFI_PASS_FRAME_SIZE;
	cm->cm_data = csio->data_ptr;
	cm->cm_len = csio->dxfer_len;
	switch (ccbh->flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		cm->cm_flags = MFI_CMD_DATAIN;
		break;
	case CAM_DIR_OUT:
		cm->cm_flags = MFI_CMD_DATAOUT;
		break;
	case CAM_DIR_NONE:
	default:
		cm->cm_data = NULL;
		cm->cm_len = 0;
		cm->cm_flags = 0;
		break;
	}

	TAILQ_REMOVE(&sc->mfi_sc->mfi_cam_ccbq, ccbh, sim_links.tqe);
	return (cm);
}

static void
mfip_done(struct mfi_command *cm)
{
	union ccb *ccb = cm->cm_private;
	struct ccb_hdr *ccbh = &ccb->ccb_h;
	struct ccb_scsiio *csio = &ccb->csio;
	struct mfip_softc *sc;
	struct mfi_pass_frame *pt;

	sc = ccbh->ccb_mfip_ptr;
	pt = &cm->cm_frame->pass;

	switch (pt->header.cmd_status) {
	case MFI_STAT_OK:
	{
		uint8_t command, device;

		ccbh->status = CAM_REQ_CMP;
		csio->scsi_status = pt->header.scsi_status;
		if (ccbh->flags & CAM_CDB_POINTER)
			command = ccb->csio.cdb_io.cdb_ptr[0];
		else
			command = ccb->csio.cdb_io.cdb_bytes[0];
		if (command == INQUIRY) {
			device = ccb->csio.data_ptr[0] & 0x1f;
			if ((device == T_DIRECT) || (device == T_PROCESSOR))
				csio->data_ptr[0] =
				     (device & 0xe0) | T_NODEVICE;
		}
		break;
	}
	case MFI_STAT_SCSI_DONE_WITH_ERROR:
	{
		int sense_len;

		ccbh->status = CAM_REQ_CMP_ERR | CAM_AUTOSNS_VALID;
		csio->scsi_status = pt->header.scsi_status;
		sense_len = min(pt->header.sense_len, sizeof(struct scsi_sense_data));
		bzero(&csio->sense_data, sizeof(struct scsi_sense_data));
		bcopy(&cm->cm_sense->data[0], &csio->sense_data, sense_len);
		break;
	}
	case MFI_STAT_DEVICE_NOT_FOUND:
		ccbh->status = CAM_DEV_NOT_THERE;
		break;
	case MFI_STAT_SCSI_IO_FAILED:
		ccbh->status = CAM_REQ_CMP_ERR;
		csio->scsi_status = pt->header.scsi_status;
		break;
	default:
		ccbh->status = CAM_REQ_CMP_ERR;
		csio->scsi_status = pt->header.scsi_status;
		break;
	}

	mfi_release_command(cm);
	xpt_done(ccb);
}

static void
mfip_cam_poll(struct cam_sim *sim)
{
	return;
}

