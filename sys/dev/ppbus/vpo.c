/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu
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
 *	$Id: vpo.c,v 1.11 1999/01/10 12:04:55 nsouch Exp $
 *
 */

#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#include <machine/clock.h>

#endif	/* KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>

#ifdef	KERNEL
#include <sys/kernel.h>
#endif /*KERNEL */

#include "opt_vpo.h"

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/vpoio.h>

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
	struct cam_path *path;

	struct vpo_sense vpo_sense;

	struct vpoio_data vpo_io;	/* interface to low level functions */
};

/* cam related functions */
static void	vpo_action(struct cam_sim *sim, union ccb *ccb);
static void	vpo_poll(struct cam_sim *sim);

static int	nvpo = 0;
#define MAXVP0	8			/* XXX not much better! */
static struct vpo_data *vpodata[MAXVP0];

#ifdef KERNEL

/*
 * Make ourselves visible as a ppbus driver
 */
static struct ppb_device	*vpoprobe(struct ppb_data *ppb);
static int			vpoattach(struct ppb_device *dev);

static struct ppb_driver vpodriver = {
    vpoprobe, vpoattach, "vpo"
};
DATA_SET(ppbdriver_set, vpodriver);

#endif /* KERNEL */

/*
 * vpoprobe()
 *
 * Called by ppb_attachdevs().
 */
static struct ppb_device *
vpoprobe(struct ppb_data *ppb)
{
	struct vpo_data *vpo;
	struct ppb_device *dev;

	if (nvpo >= MAXVP0) {
		printf("vpo: Too many devices (max %d)\n", MAXVP0);
		return(NULL);
	}

	vpo = (struct vpo_data *)malloc(sizeof(struct vpo_data),
							M_DEVBUF, M_NOWAIT);
	if (!vpo) {
		printf("vpo: cannot malloc!\n");
		return(NULL);
	}
	bzero(vpo, sizeof(struct vpo_data));

	vpodata[nvpo] = vpo;

	/* vpo dependent initialisation */
	vpo->vpo_unit = nvpo;

	/* ok, go to next device on next probe */
	nvpo ++;

	/* low level probe */
	vpoio_set_unit(&vpo->vpo_io, vpo->vpo_unit);

	/* check ZIP before ZIP+ or imm_probe() will send controls to
	 * the printer or whatelse connected to the port */
	if ((dev = vpoio_probe(ppb, &vpo->vpo_io))) {
		vpo->vpo_isplus = 0;
	} else if ((dev = imm_probe(ppb, &vpo->vpo_io))) {
		vpo->vpo_isplus = 1;
	} else {
		free(vpo, M_DEVBUF);
		return (NULL);
	}

	return (dev);
}

/*
 * vpoattach()
 *
 * Called by ppb_attachdevs().
 */
static int
vpoattach(struct ppb_device *dev)
{
	struct vpo_data *vpo = vpodata[dev->id_unit];
	struct cam_devq *devq;

	/* low level attachment */
	if (vpo->vpo_isplus) {
		if (!imm_attach(&vpo->vpo_io))
			return (0);
	} else {
		if (!vpoio_attach(&vpo->vpo_io))
			return (0);
	}

	/*
	**	Now tell the generic SCSI layer
	**	about our bus.
	*/
	devq = cam_simq_alloc(/*maxopenings*/1);
	/* XXX What about low-level detach on error? */
	if (devq == NULL)
		return (0);

	vpo->sim = cam_sim_alloc(vpo_action, vpo_poll, "vpo", vpo, dev->id_unit,
				 /*untagged*/1, /*tagged*/0, devq);
	if (vpo->sim == NULL) {
		cam_simq_free(devq);
		return (0);
	}

	if (xpt_bus_register(vpo->sim, /*bus*/0) != CAM_SUCCESS) {
		cam_sim_free(vpo->sim, /*free_devq*/TRUE);
		return (0);
	}

	if (xpt_create_path(&vpo->path, /*periph*/NULL,
			    cam_sim_path(vpo->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(vpo->sim));
		cam_sim_free(vpo->sim, /*free_devq*/TRUE);
		return (0);
	}

	/* all went ok */

	return (1);
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
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

#ifdef VP0_DEBUG
		printf("vpo%d: XPT_CALC_GEOMETRY (%d, %d) request\n",
			vpo->vpo_unit, ccg->volume_size, ccg->block_size);
#endif
		
		ccg->heads = 64;
		ccg->secs_per_track = 32;

		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;

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
