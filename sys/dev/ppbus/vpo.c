/*-
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>	/* for struct devstat */


#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>

#include <sys/kernel.h>

#include "opt_vpo.h"

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/vpoio.h>

#include "ppbus_if.h"

struct vpo_sense {
	struct scsi_sense cmd;
	unsigned int stat;
	unsigned int count;
};

struct vpo_data {
	unsigned short vpo_unit;

	int vpo_stat;
	int vpo_count;
	int vpo_error;

	int vpo_isplus;

	struct cam_sim  *sim;

	struct vpo_sense vpo_sense;

	struct vpoio_data vpo_io;	/* interface to low level functions */
};

#define DEVTOSOFTC(dev) \
	((struct vpo_data *)device_get_softc(dev))

/* cam related functions */
static void	vpo_action(struct cam_sim *sim, union ccb *ccb);
static void	vpo_poll(struct cam_sim *sim);
static void	vpo_cam_rescan_callback(struct cam_periph *periph,
					union ccb *ccb);
static void	vpo_cam_rescan(struct vpo_data *vpo);

static void
vpo_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "vpo", -1);
}

/*
 * vpo_probe()
 */
static int
vpo_probe(device_t dev)
{
	struct vpo_data *vpo;
	int error;

	vpo = DEVTOSOFTC(dev);
	bzero(vpo, sizeof(struct vpo_data));

	/* vpo dependent initialisation */
	vpo->vpo_unit = device_get_unit(dev);

	/* low level probe */
	vpoio_set_unit(&vpo->vpo_io, vpo->vpo_unit);

	/* check ZIP before ZIP+ or imm_probe() will send controls to
	 * the printer or whatelse connected to the port */
	if ((error = vpoio_probe(dev, &vpo->vpo_io)) == 0) {
		vpo->vpo_isplus = 0;
		device_set_desc(dev,
				"Iomega VPI0 Parallel to SCSI interface");
	} else if ((error = imm_probe(dev, &vpo->vpo_io)) == 0) {
		vpo->vpo_isplus = 1;
		device_set_desc(dev,
				"Iomega Matchmaker Parallel to SCSI interface");
	} else {
		return (error);
	}

	return (0);
}

/*
 * vpo_attach()
 */
static int
vpo_attach(device_t dev)
{
	struct vpo_data *vpo = DEVTOSOFTC(dev);
	struct cam_devq *devq;
	int error;

	/* low level attachment */
	if (vpo->vpo_isplus) {
		if ((error = imm_attach(&vpo->vpo_io)))
			return (error);
	} else {
		if ((error = vpoio_attach(&vpo->vpo_io)))
			return (error);
	}

	/*
	**	Now tell the generic SCSI layer
	**	about our bus.
	*/
	devq = cam_simq_alloc(/*maxopenings*/1);
	/* XXX What about low-level detach on error? */
	if (devq == NULL)
		return (ENXIO);

	vpo->sim = cam_sim_alloc(vpo_action, vpo_poll, "vpo", vpo,
				 device_get_unit(dev),
				 /*untagged*/1, /*tagged*/0, devq);
	if (vpo->sim == NULL) {
		cam_simq_free(devq);
		return (ENXIO);
	}

	if (xpt_bus_register(vpo->sim, /*bus*/0) != CAM_SUCCESS) {
		cam_sim_free(vpo->sim, /*free_devq*/TRUE);
		return (ENXIO);
	}

	/* all went ok */

	vpo_cam_rescan(vpo);	/* have CAM rescan the bus */

	return (0);
}

static void
vpo_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
        free(ccb, M_TEMP);
}

static void
vpo_cam_rescan(struct vpo_data *vpo)
{
        struct cam_path *path;
        union ccb *ccb = malloc(sizeof(union ccb), M_TEMP, M_ZERO);

        if (xpt_create_path(&path, xpt_periph, cam_sim_path(vpo->sim), 0, 0)
            != CAM_REQ_CMP) {
		/* A failure is benign as the user can do a manual rescan */
                return;
	}

        xpt_setup_ccb(&ccb->ccb_h, path, 5/*priority (low)*/);
        ccb->ccb_h.func_code = XPT_SCAN_BUS;
        ccb->ccb_h.cbfcnp = vpo_cam_rescan_callback;
        ccb->crcn.flags = CAM_FLAG_NONE;
        xpt_action(ccb);

        /* The scan is in progress now. */
}

/*
 * vpo_intr()
 */
static void
vpo_intr(struct vpo_data *vpo, struct ccb_scsiio *csio)
{
	int errno;	/* error in errno.h */
	int s;
#ifdef VP0_DEBUG
	int i;
#endif

	s = splcam();

	if (vpo->vpo_isplus) {
		errno = imm_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
			csio->ccb_h.target_id,
			(char *)&csio->cdb_io.cdb_bytes, csio->cdb_len,
			(char *)csio->data_ptr, csio->dxfer_len,
			&vpo->vpo_stat, &vpo->vpo_count, &vpo->vpo_error);
	} else {
		errno = vpoio_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
			csio->ccb_h.target_id,
			(char *)&csio->cdb_io.cdb_bytes, csio->cdb_len,
			(char *)csio->data_ptr, csio->dxfer_len,
			&vpo->vpo_stat, &vpo->vpo_count, &vpo->vpo_error);
	}

#ifdef VP0_DEBUG
	printf("vpo_do_scsi = %d, status = 0x%x, count = %d, vpo_error = %d\n", 
		 errno, vpo->vpo_stat, vpo->vpo_count, vpo->vpo_error);

	/* dump of command */
	for (i=0; i<csio->cdb_len; i++)
		printf("%x ", ((char *)&csio->cdb_io.cdb_bytes)[i]);

	printf("\n");
#endif

	if (errno) {
		/* connection to ppbus interrupted */
		csio->ccb_h.status = CAM_CMD_TIMEOUT;
		goto error;
	}

	/* if a timeout occured, no sense */
	if (vpo->vpo_error) {
		if (vpo->vpo_error != VP0_ESELECT_TIMEOUT)
			printf("vpo%d: VP0 error/timeout (%d)\n",
				vpo->vpo_unit, vpo->vpo_error);

		csio->ccb_h.status = CAM_CMD_TIMEOUT;
		goto error;
	}

	/* check scsi status */
	if (vpo->vpo_stat != SCSI_STATUS_OK) {
	   csio->scsi_status = vpo->vpo_stat;

	   /* check if we have to sense the drive */
	   if ((vpo->vpo_stat & SCSI_STATUS_CHECK_COND) != 0) {

		vpo->vpo_sense.cmd.opcode = REQUEST_SENSE;
		vpo->vpo_sense.cmd.length = csio->sense_len;
		vpo->vpo_sense.cmd.control = 0;

		if (vpo->vpo_isplus) {
			errno = imm_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
				csio->ccb_h.target_id,
				(char *)&vpo->vpo_sense.cmd,
				sizeof(vpo->vpo_sense.cmd),
				(char *)&csio->sense_data, csio->sense_len,
				&vpo->vpo_sense.stat, &vpo->vpo_sense.count,
				&vpo->vpo_error);
		} else {
			errno = vpoio_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
				csio->ccb_h.target_id,
				(char *)&vpo->vpo_sense.cmd,
				sizeof(vpo->vpo_sense.cmd),
				(char *)&csio->sense_data, csio->sense_len,
				&vpo->vpo_sense.stat, &vpo->vpo_sense.count,
				&vpo->vpo_error);
		}
			

#ifdef VP0_DEBUG
		printf("(sense) vpo_do_scsi = %d, status = 0x%x, count = %d, vpo_error = %d\n", 
			errno, vpo->vpo_sense.stat, vpo->vpo_sense.count, vpo->vpo_error);
#endif

		/* check sense return status */
		if (errno == 0 && vpo->vpo_sense.stat == SCSI_STATUS_OK) {
		   /* sense ok */
		   csio->ccb_h.status = CAM_AUTOSNS_VALID | CAM_SCSI_STATUS_ERROR;
		   csio->sense_resid = csio->sense_len - vpo->vpo_sense.count;

#ifdef VP0_DEBUG
		   /* dump of sense info */
		   printf("(sense) ");
		   for (i=0; i<vpo->vpo_sense.count; i++)
			printf("%x ", ((char *)&csio->sense_data)[i]);
		   printf("\n");
#endif

		} else {
		   /* sense failed */
		   csio->ccb_h.status = CAM_AUTOSENSE_FAIL;
		}
	   } else {
		/* no sense */
		csio->ccb_h.status = CAM_SCSI_STATUS_ERROR;			
	   }

	   goto error;
	}

	csio->resid = csio->dxfer_len - vpo->vpo_count;
	csio->ccb_h.status = CAM_REQ_CMP;

error:
	splx(s);

	return;
}

static void
vpo_action(struct cam_sim *sim, union ccb *ccb)
{

	struct vpo_data *vpo = (struct vpo_data *)sim->softc;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio;

		csio = &ccb->csio;

#ifdef VP0_DEBUG
		printf("vpo%d: XPT_SCSI_IO (0x%x) request\n",
			vpo->vpo_unit, csio->cdb_io.cdb_bytes[0]);
#endif
		
		vpo_intr(vpo, csio);

		xpt_done(ccb);

		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;

		ccg = &ccb->ccg;

#ifdef VP0_DEBUG
		printf("vpo%d: XPT_CALC_GEOMETRY (bs=%d,vs=%d,c=%d,h=%d,spt=%d) request\n",
			vpo->vpo_unit,
			ccg->block_size,
			ccg->volume_size,
			ccg->cylinders,
			ccg->heads,
			ccg->secs_per_track);
#endif

		ccg->heads = 64;
		ccg->secs_per_track = 32;
		ccg->cylinders = ccg->volume_size /
				 (ccg->heads * ccg->secs_per_track);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{

#ifdef VP0_DEBUG
		printf("vpo%d: XPT_RESET_BUS request\n", vpo->vpo_unit);
#endif

		if (vpo->vpo_isplus) {
			if (imm_reset_bus(&vpo->vpo_io)) {
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				xpt_done(ccb);
				return;
			}
		} else {
			if (vpoio_reset_bus(&vpo->vpo_io)) {
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				xpt_done(ccb);
				return;
			}
		}

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
#ifdef VP0_DEBUG
		printf("vpo%d: XPT_PATH_INQ request\n", vpo->vpo_unit);
#endif
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7;
		cpi->max_lun = 0;
		cpi->initiator_id = VP0_INITIATOR;
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 93;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Iomega", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}

	return;
}

static void
vpo_poll(struct cam_sim *sim)
{       
	/* The ZIP is actually always polled throw vpo_action() */
	return;
}

static devclass_t vpo_devclass;

static device_method_t vpo_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	vpo_identify),
	DEVMETHOD(device_probe,		vpo_probe),
	DEVMETHOD(device_attach,	vpo_attach),

	{ 0, 0 }
};

static driver_t vpo_driver = {
	"vpo",
	vpo_methods,
	sizeof(struct vpo_data),
};
DRIVER_MODULE(vpo, ppbus, vpo_driver, vpo_devclass, 0, 0);
