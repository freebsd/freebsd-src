/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/amr/amr_compat.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <machine/resource.h>
#include <machine/bus.h>

#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>

static void		amr_cam_action(struct cam_sim *sim, union ccb *ccb);
static void		amr_cam_poll(struct cam_sim *sim);
static void		amr_cam_complete(struct amr_command *ac);


/********************************************************************************
 * Enqueue/dequeue functions
 */
static __inline void
amr_enqueue_ccb(struct amr_softc *sc, union ccb *ccb)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_TAIL(&sc->amr_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
}

static __inline void
amr_requeue_ccb(struct amr_softc *sc, union ccb *ccb)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_HEAD(&sc->amr_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
}

static __inline union ccb *
amr_dequeue_ccb(struct amr_softc *sc)
{
    union ccb	*ccb;
    int		s;

    s = splbio();
    if ((ccb = (union ccb *)TAILQ_FIRST(&sc->amr_cam_ccbq)) != NULL)
	TAILQ_REMOVE(&sc->amr_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
    return(ccb);
}

/********************************************************************************
 * Attach our 'real' SCSI channels to CAM
 */
int
amr_cam_attach(struct amr_softc *sc)
{
    struct cam_devq	*devq;
    int			chn;

    /* initialise the ccb queue */
    TAILQ_INIT(&sc->amr_cam_ccbq);

    /*
     * Allocate a devq for all our channels combined.  This should
     * allow for the maximum number of SCSI commands we will accept
     * at one time.
     */
    if ((devq = cam_simq_alloc(AMR_MAX_SCSI_CMDS)) == NULL)
	return(ENOMEM);

    /*
     * Iterate over our channels, registering them with CAM
     */
    for (chn = 0; chn < sc->amr_maxchan; chn++) {
	
	/* allocate a sim */
	if ((sc->amr_cam_sim[chn] = cam_sim_alloc(amr_cam_action,
						  amr_cam_poll,
						  "amr",
						  sc, 
						  device_get_unit(sc->amr_dev),
						  1, 
						  AMR_MAX_SCSI_CMDS,
						  devq)) == NULL) {
	    cam_simq_free(devq);
	    device_printf(sc->amr_dev, "CAM SIM attach failed\n");
	    return(ENOMEM);
	}
	
	/* register the bus ID so we can get it later */
	if (xpt_bus_register(sc->amr_cam_sim[chn], chn)) {
	    device_printf(sc->amr_dev, "CAM XPT bus registration failed\n");
	    return(ENXIO);
	}
    }
    /*
     * XXX we should scan the config and work out which devices are actually
     * protected.
     */
    return(0);
}

/********************************************************************************
 * Disconnect ourselves from CAM
 */
void
amr_cam_detach(struct amr_softc *sc)
{
    int		chn, first;

    for (chn = 0, first = 1; chn < sc->amr_maxchan; chn++) {
	
	/*
	 * If a sim was allocated for this channel, free it
	 */
	if (sc->amr_cam_sim[chn] != NULL) {
	    xpt_bus_deregister(cam_sim_path(sc->amr_cam_sim[chn]));
	    cam_sim_free(sc->amr_cam_sim[chn], first ? TRUE : FALSE);
	    first = 0;
	}
    }
}

/********************************************************************************
 ********************************************************************************
                                                        CAM passthrough interface
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Handle a request for action from CAM
 */
static void
amr_cam_action(struct cam_sim *sim, union ccb *ccb)
{
    struct amr_softc	*sc = cam_sim_softc(sim);

    switch(ccb->ccb_h.func_code) {

    /*
     * Perform SCSI I/O to a physical device.
     */
    case XPT_SCSI_IO:
    {
	struct ccb_hdr		*ccbh = &ccb->ccb_h;
	struct ccb_scsiio	*csio = &ccb->csio;

	/* Validate the CCB */
	ccbh->status = CAM_REQ_INPROG;

	/* check the CDB length */
	if (csio->cdb_len > AMR_MAX_CDB_LEN)
	    ccbh->status = CAM_REQ_CMP_ERR;

	/* check that the CDB pointer is not to a physical address */
	if ((ccbh->flags & CAM_CDB_POINTER) && (ccbh->flags & CAM_CDB_PHYS))
	    ccbh->status = CAM_REQ_CMP_ERR;

	/* if there is data transfer, it must be to/from a virtual address */
	if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
	    if (ccbh->flags & CAM_DATA_PHYS)		/* we can't map it */
		ccbh->status = CAM_REQ_CMP_ERR;		
	    if (ccbh->flags & CAM_SCATTER_VALID)	/* we want to do the s/g setup */
		ccbh->status = CAM_REQ_CMP_ERR;
	}

	/*
	 * If the command is to a LUN other than 0, fail it.
	 * This is probably incorrect, but during testing the firmware did not
	 * seem to respect the LUN field, and thus devices appear echoed.
	 */
	if (csio->ccb_h.target_lun != 0)
	    ccbh->status = CAM_REQ_CMP_ERR;

	/* if we're happy with the request, queue it for attention */
	if (ccbh->status == CAM_REQ_INPROG) {

	    /* save the channel number in the ccb */
	    csio->ccb_h.sim_priv.entries[0].field = cam_sim_bus(sim);

	    amr_enqueue_ccb(sc, ccb);
	    amr_startio(sc);
	    return;
	}
	break;
    }

    case XPT_CALC_GEOMETRY:
    {
	struct    ccb_calc_geometry *ccg = &ccb->ccg;
	u_int32_t size_in_mb;
	u_int32_t secs_per_cylinder;

	size_in_mb = ccg->volume_size / ((1024L * 1024L) / ccg->block_size);

	if (size_in_mb > 1024) {
	    ccg->heads = 255;
	    ccg->secs_per_track = 63;
	} else {
	    ccg->heads = 64;
	    ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	ccb->ccb_h.status = CAM_REQ_CMP;
	break;
    }

    /*
     * Return path stats.  Some of these should probably be
     * amended.
     */
    case XPT_PATH_INQ:
    {
	struct ccb_pathinq	*cpi = & ccb->cpi;

	cpi->version_num = 1;			/* XXX??? */
	cpi->hba_inquiry = PI_SDTR_ABLE;
	cpi->target_sprt = 0;
	cpi->hba_misc = 0;
	cpi->hba_eng_cnt = 0;
	cpi->max_target = AMR_MAX_TARGETS;
	cpi->max_lun = AMR_MAX_LUNS;
	cpi->initiator_id = 7;			/* XXX variable? */
	strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
	strncpy(cpi->hba_vid, "BSDi", HBA_IDLEN);
	strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
	cpi->unit_number = cam_sim_unit(sim);
	cpi->bus_id = cam_sim_bus(sim);
	cpi->base_transfer_speed = 132 * 1024;	/* XXX get from controller? */
	cpi->ccb_h.status = CAM_REQ_CMP;

	break;
    }

    /*
     * Reject anything else as unsupported.
     */
    default:
	/* we can't do this */
	ccb->ccb_h.status = CAM_REQ_INVALID;
	break;
    }
    xpt_done(ccb);
}

/********************************************************************************
 * Convert a CAM CCB off the top of the CCB queue to a passthrough SCSI command.
 */
int
amr_cam_command(struct amr_softc *sc, struct amr_command **acp)
{
    struct amr_command		*ac;
    struct amr_passthrough	*ap;
    struct ccb_scsiio		*csio;
    int				bus, target, error;

    error = 0;
    ac = NULL;
    ap = NULL;

    /* check to see if there is a ccb for us to work with */
    if ((csio = (struct ccb_scsiio *)amr_dequeue_ccb(sc)) == NULL)
	goto out;

    /* get bus/target, XXX validate against protected devices? */
    bus = csio->ccb_h.sim_priv.entries[0].field;
    target = csio->ccb_h.target_id;

    /* 
     * Build a passthrough command.
     */

    /* construct passthrough */
    if ((ap = malloc(sizeof(*ap), M_DEVBUF, M_NOWAIT)) == NULL) {
	error = ENOMEM;
	goto out;
    }
    bzero(ap, sizeof(*ap));
    ap->ap_timeout = 0;
    ap->ap_ars = 1;
    ap->ap_request_sense_length = 14;
    ap->ap_islogical = 0;
    ap->ap_channel = bus;
    ap->ap_scsi_id = target;
    ap->ap_logical_drive_no = csio->ccb_h.target_lun;
    ap->ap_cdb_length = csio->cdb_len;
    if (csio->ccb_h.flags & CAM_CDB_POINTER) {
	bcopy(csio->cdb_io.cdb_ptr, ap->ap_cdb, csio->cdb_len);
    } else {
	bcopy(csio->cdb_io.cdb_bytes, ap->ap_cdb, csio->cdb_len);
    }
    /* we leave the data s/g list and s/g count to the map routine later */

    debug(2, " COMMAND %x/%d+%d to %d:%d:%d", ap->ap_cdb[0], ap->ap_cdb_length, csio->dxfer_len,
	  ap->ap_channel, ap->ap_scsi_id, ap->ap_logical_drive_no);

    /* construct command */
    if ((ac = amr_alloccmd(sc)) == NULL) {
	error = ENOMEM;
	goto out;
    }

    ac->ac_data = ap;
    ac->ac_length = sizeof(*ap);
    ac->ac_flags |= AMR_CMD_DATAOUT;

    ac->ac_ccb_data = csio->data_ptr;
    ac->ac_ccb_length = csio->dxfer_len;
    if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
	ac->ac_flags |= AMR_CMD_CCB_DATAIN;
    if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
	ac->ac_flags |= AMR_CMD_CCB_DATAOUT;

    ac->ac_complete = amr_cam_complete;
    ac->ac_private = csio;
    ac->ac_mailbox.mb_command = AMR_CMD_PASS;

out:
    if (error != 0) {
	if (ac != NULL)
	    amr_releasecmd(ac);
	if (ap != NULL)
	    free(ap, M_DEVBUF);
	if (csio != NULL)			/* put it back and try again later */
	    amr_requeue_ccb(sc, (union ccb *)csio);
    }
    *acp = ac;
    return(error);
}

/********************************************************************************
 * Check for interrupt status
 */
static void
amr_cam_poll(struct cam_sim *sim)
{
    amr_done(cam_sim_softc(sim));
}

/********************************************************************************
 * Handle completion of a command submitted via CAM.
 */
static void
amr_cam_complete(struct amr_command *ac)
{
    struct amr_passthrough	*ap = (struct amr_passthrough *)ac->ac_data;
    struct ccb_scsiio		*csio = (struct ccb_scsiio *)ac->ac_private;
    struct scsi_inquiry_data	*inq = (struct scsi_inquiry_data *)csio->data_ptr;

    /* XXX note that we're ignoring ac->ac_status - good idea? */

    debug(1, "status 0x%x  scsi_status 0x%x", ac->ac_status, ap->ap_scsi_status);

    /* 
     * Hide disks from CAM so that they're not picked up and treated as 'normal' disks.
     *
     * If the configuration provides a mechanism to mark a disk a "not managed", we
     * could add handling for that to allow disks to be selectively visible.
     */
#if 0
    if ((ap->ap_cdb[0] == INQUIRY) && (SID_TYPE(inq) == T_DIRECT)) {
	bzero(csio->data_ptr, csio->dxfer_len);
	if (ap->ap_scsi_status == 0xf0) {
	    csio->ccb_h.status = CAM_SCSI_STATUS_ERROR;
	} else {
	    csio->ccb_h.status = CAM_DEV_NOT_THERE;
	}
    } else {
#else 
    {
#endif

	/* handle passthrough SCSI status */
	switch(ap->ap_scsi_status) {
	case 0:				/* completed OK */
	    csio->ccb_h.status = CAM_REQ_CMP;
	    break;

	case 0x02:
	    csio->ccb_h.status = CAM_SCSI_STATUS_ERROR;
	    csio->scsi_status = SCSI_STATUS_CHECK_COND;
	    bcopy(ap->ap_request_sense_area, &csio->sense_data, AMR_MAX_REQ_SENSE_LEN);
	    csio->sense_len = AMR_MAX_REQ_SENSE_LEN;
	    csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	    break;

	case 0x08:
	    csio->ccb_h.status = CAM_SCSI_BUSY;
	    break;
	    
	case 0xf0:
	case 0xf4:
	default:
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	    break;
	}	    
    }
    free(ap, M_DEVBUF);
    if ((csio->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE)
	debug(2, "%*D\n", imin(csio->dxfer_len, 16), csio->data_ptr, " ");
    xpt_done((union ccb *)csio);
    amr_releasecmd(ac);
}

