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
/*
 * CAM interface for FreeBSD
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devicestat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>

#include <machine/resource.h>
#include <machine/bus.h>

#include <dev/mly/mlyreg.h>
#include <dev/mly/mlyvar.h>
#include <dev/mly/mly_tables.h>

static void			mly_cam_poll(struct cam_sim *sim);
static void			mly_cam_action(struct cam_sim *sim, union ccb *ccb);
static void			mly_cam_complete(struct mly_command *mc);
static struct cam_periph	*mly_find_periph(struct mly_softc *sc, int bus, int target);

/********************************************************************************
 * CAM-specific queue primitives
 */
static __inline void
mly_enqueue_ccb(struct mly_softc *sc, union ccb *ccb)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_TAIL(&sc->mly_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
}

static __inline void
mly_requeue_ccb(struct mly_softc *sc, union ccb *ccb)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_HEAD(&sc->mly_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
}

static __inline union ccb *
mly_dequeue_ccb(struct mly_softc *sc)
{
    union ccb	*ccb;
    int		s;

    s = splcam();
    if ((ccb = (union ccb *)TAILQ_FIRST(&sc->mly_cam_ccbq)) != NULL)
	TAILQ_REMOVE(&sc->mly_cam_ccbq, &ccb->ccb_h, sim_links.tqe);
    splx(s);
    return(ccb);
}

/********************************************************************************
 * space-fill a character string
 */
static __inline void
padstr(char *targ, char *src, int len)
{
    while (len-- > 0) {
	if (*src != 0) {
	    *targ++ = *src++;
	} else {
	    *targ++ = ' ';
	}
    }
}

/********************************************************************************
 * Attach the real and virtual SCSI busses to CAM
 */
int
mly_cam_attach(struct mly_softc *sc)
{
    struct cam_devq	*devq;
    int			chn, nchn;

    debug_called(1);

    /* initialise the CCB queue */
    TAILQ_INIT(&sc->mly_cam_ccbq);

    /*
     * Allocate a devq for all our channels combined.
     */
    if ((devq = cam_simq_alloc(sc->mly_controllerinfo->maximum_parallel_commands)) == NULL) {
	mly_printf(sc, "can't allocate CAM SIM\n");
	return(ENOMEM);
    }

    /*
     * Iterate over channels, registering them with CAM.
     */
    nchn = sc->mly_controllerinfo->physical_channels_present +
	sc->mly_controllerinfo->virtual_channels_present;
    for (chn = 0; chn < nchn; chn++) {

	/* allocate a sim */
	if ((sc->mly_cam_sim[chn] = cam_sim_alloc(mly_cam_action, 
						  mly_cam_poll, 
						  "mly", 
						  sc,
						  device_get_unit(sc->mly_dev), 
						  1,
						  sc->mly_controllerinfo->maximum_parallel_commands,
						  devq)) ==  NULL) {
	    cam_simq_free(devq);
	    mly_printf(sc, "CAM SIM attach failed\n");
	    return(ENOMEM);
	}

	/* register the bus ID so we can get it later */
	if (xpt_bus_register(sc->mly_cam_sim[chn], chn)) {
	    mly_printf(sc, "CAM XPT bus registration failed\n");
	    return(ENXIO);
	}
	debug(1, "registered sim %p bus %d", sc->mly_cam_sim[chn], chn);

    }

    return(0);
}

/********************************************************************************
 * Detach from CAM
 */
void
mly_cam_detach(struct mly_softc *sc)
{
    int		chn, nchn, first;

    debug_called(1);

    nchn = sc->mly_controllerinfo->physical_channels_present +
	sc->mly_controllerinfo->virtual_channels_present;

    /*
     * Iterate over channels, deregistering as we go.
     */
    nchn = sc->mly_controllerinfo->physical_channels_present +
	sc->mly_controllerinfo->virtual_channels_present;
    for (chn = 0, first = 1; chn < nchn; chn++) {

	/*
	 * If a sim was registered for this channel, free it.
	 */
	if (sc->mly_cam_sim[chn] != NULL) {
	    debug(1, "deregister bus %d", chn);
	    xpt_bus_deregister(cam_sim_path(sc->mly_cam_sim[chn]));
	    debug(1, "free sim for channel %d (%sfree queue)", chn, first ? "" : "don't ");
	    cam_sim_free(sc->mly_cam_sim[chn], first ? TRUE : FALSE);
	    first = 0;
	}
    }
}

/********************************************************************************
 * Handle an action requested by CAM
 */
static void
mly_cam_action(struct cam_sim *sim, union ccb *ccb)
{
    struct mly_softc	*sc = cam_sim_softc(sim);

    debug_called(2);

    switch (ccb->ccb_h.func_code) {

	/* perform SCSI I/O */
    case XPT_SCSI_IO:
    {
	struct ccb_scsiio	*csio = &ccb->csio;
	int			bus, target;

	bus = cam_sim_bus(sim);
	target = csio->ccb_h.target_id;

	debug(2, "XPT_SCSI_IO %d:%d:%d", bus, target, ccb->ccb_h.target_lun);

	/*  check for I/O attempt to a protected device */
	if (sc->mly_btl[bus][target].mb_flags & MLY_BTL_PROTECTED) {
	    debug(2, "  device protected");
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}

	/* check for I/O attempt to nonexistent device */
	if (!(sc->mly_btl[bus][target].mb_flags & (MLY_BTL_LOGICAL | MLY_BTL_PHYSICAL))) {
	    debug(2, "  device does not exist");
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}

	/* XXX increase if/when we support large SCSI commands */
	if (csio->cdb_len > MLY_CMD_SCSI_SMALL_CDB) {
	    debug(2, "  command too large (%d > %d)", csio->cdb_len, MLY_CMD_SCSI_SMALL_CDB);
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}

        /* check that the CDB pointer is not to a physical address */
        if ((csio->ccb_h.flags & CAM_CDB_POINTER) && (csio->ccb_h.flags & CAM_CDB_PHYS)) {
	    debug(2, "  CDB pointer is to physical address");
            csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}

        /* if there is data transfer, it must be to/from a virtual address */
        if ((csio->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
            if (csio->ccb_h.flags & CAM_DATA_PHYS) {		/* we can't map it */
		debug(2, "  data pointer is to physical address");
                csio->ccb_h.status = CAM_REQ_CMP_ERR;
	    }
            if (csio->ccb_h.flags & CAM_SCATTER_VALID) {	/* we want to do the s/g setup */
		debug(2, "  data has premature s/g setup");
                csio->ccb_h.status = CAM_REQ_CMP_ERR;
	    }
        }

	/* abandon aborted ccbs or those that have failed validation */
	if ((csio->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
	    debug(2, "abandoning CCB due to abort/validation failure");
	    break;
	}

	/* save the channel number in the ccb */
	csio->ccb_h.sim_priv.entries[0].field = bus;

	/* enqueue the ccb and start I/O */
	mly_enqueue_ccb(sc, ccb);
	mly_startio(sc);
	return;
    }

	/* perform geometry calculations */
    case XPT_CALC_GEOMETRY:
    {
	struct ccb_calc_geometry	*ccg = &ccb->ccg;
        u_int32_t			secs_per_cylinder;

	debug(2, "XPT_CALC_GEOMETRY %d:%d:%d", cam_sim_bus(sim), ccb->ccb_h.target_id, ccb->ccb_h.target_lun);

	if (sc->mly_controllerparam->bios_geometry == MLY_BIOSGEOM_8G) {
	    ccg->heads = 255;
            ccg->secs_per_track = 63;
	} else {				/* MLY_BIOSGEOM_2G */
	    ccg->heads = 128;
            ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
        ccg->cylinders = ccg->volume_size / secs_per_cylinder;
        ccb->ccb_h.status = CAM_REQ_CMP;
        break;
    }

	/* handle path attribute inquiry */
    case XPT_PATH_INQ:
    {
	struct ccb_pathinq	*cpi = &ccb->cpi;

	debug(2, "XPT_PATH_INQ %d:%d:%d", cam_sim_bus(sim), ccb->ccb_h.target_id, ccb->ccb_h.target_lun);

	cpi->version_num = 1;
	cpi->hba_inquiry = PI_TAG_ABLE;		/* XXX extra flags for physical channels? */
	cpi->target_sprt = 0;
	cpi->hba_misc = 0;
	cpi->max_target = MLY_MAX_TARGETS - 1;
	cpi->max_lun = MLY_MAX_LUNS - 1;
	cpi->initiator_id = sc->mly_controllerparam->initiator_id;
	strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
        strncpy(cpi->hba_vid, "BSDi", HBA_IDLEN);
        strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
        cpi->unit_number = cam_sim_unit(sim);
        cpi->bus_id = cam_sim_bus(sim);
	cpi->base_transfer_speed = 132 * 1024;	/* XXX what to set this to? */
	ccb->ccb_h.status = CAM_REQ_CMP;
	break;
    }

    default:		/* we can't do this */
	debug(2, "unspported func_code = 0x%x", ccb->ccb_h.func_code);
	ccb->ccb_h.status = CAM_REQ_INVALID;
	break;
    }

    xpt_done(ccb);
}

/********************************************************************************
 * Check for possibly-completed commands.
 */
static void
mly_cam_poll(struct cam_sim *sim)
{
    struct mly_softc	*sc = cam_sim_softc(sim);

    debug_called(2);

    mly_done(sc);
}

/********************************************************************************
 * Pull a CCB off the work queue and turn it into a command.
 */
int
mly_cam_command(struct mly_softc *sc, struct mly_command **mcp)
{
    struct mly_command			*mc;
    struct mly_command_scsi_small	*ss;
    struct ccb_scsiio			*csio;
    int					error;

    debug_called(2);

    error = 0;
    mc = NULL;
    csio = NULL;

    /* check for a CCB */
    if (!(csio = (struct ccb_scsiio *)mly_dequeue_ccb(sc)))
	goto out;

    /* get a command to back it */
    if (mly_alloc_command(sc, &mc)) {
	error = ENOMEM;
	goto out;
    }

    /* build the command */
    MLY_CMD_SETSTATE(mc, MLY_CMD_SETUP);
    mc->mc_data = csio->data_ptr;
    mc->mc_length = csio->dxfer_len;
    mc->mc_complete = mly_cam_complete;
    mc->mc_private = csio;

    /* build the packet for the controller */
    ss = &mc->mc_packet->scsi_small;
    ss->opcode = MDACMD_SCSI;
    if (csio->ccb_h.flags * CAM_DIS_DISCONNECT)
	ss->command_control.disable_disconnect = 1;
    if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
	ss->command_control.data_direction = MLY_CCB_WRITE;
    ss->data_size = csio->dxfer_len;
    ss->addr.phys.lun = csio->ccb_h.target_lun;
    ss->addr.phys.target = csio->ccb_h.target_id;
    ss->addr.phys.channel = csio->ccb_h.sim_priv.entries[0].field;
    if (csio->ccb_h.timeout < (60 * 1000)) {
	ss->timeout.value = csio->ccb_h.timeout / 1000;
	ss->timeout.scale = MLY_TIMEOUT_SECONDS;
    } else if (csio->ccb_h.timeout < (60 * 60 * 1000)) {
	ss->timeout.value = csio->ccb_h.timeout / (60 * 1000);
	ss->timeout.scale = MLY_TIMEOUT_MINUTES;
    } else {
	ss->timeout.value = csio->ccb_h.timeout / (60 * 60 * 1000);	/* overflow? */
	ss->timeout.scale = MLY_TIMEOUT_HOURS;
    }
    ss->maximum_sense_size = csio->sense_len;
    ss->cdb_length = csio->cdb_len;
    if (csio->ccb_h.flags & CAM_CDB_POINTER) {
	bcopy(csio->cdb_io.cdb_ptr, ss->cdb, csio->cdb_len);
    } else {
	bcopy(csio->cdb_io.cdb_bytes, ss->cdb, csio->cdb_len);
    }

out:
    if (error != 0) {
	if (mc != NULL) {
	    mly_release_command(mc);
	    mc = NULL;
	}
	if (csio != NULL)
	    mly_requeue_ccb(sc, (union ccb *)csio);
    }
    *mcp = mc;
    return(error);
}

/********************************************************************************
 * Handle completion of a command - pass results back through the CCB
 */
static void
mly_cam_complete(struct mly_command *mc)
{
    struct mly_softc		*sc = mc->mc_sc;
    struct ccb_scsiio		*csio = (struct ccb_scsiio *)mc->mc_private;
    struct scsi_inquiry_data	*inq = (struct scsi_inquiry_data *)csio->data_ptr;
    struct mly_btl		*btl;
    u_int8_t			cmd;
    int				bus, target;

    debug_called(2);

    csio->scsi_status = mc->mc_status;
    switch(mc->mc_status) {
    case SCSI_STATUS_OK:
	/*
	 * In order to report logical device type and status, we overwrite
	 * the result of the INQUIRY command to logical devices.
	 */
	bus = csio->ccb_h.sim_priv.entries[0].field;
	if (bus >= sc->mly_controllerinfo->physical_channels_present) {
	    if (csio->ccb_h.flags & CAM_CDB_POINTER) {
		cmd = *csio->cdb_io.cdb_ptr;
	    } else {
		cmd = csio->cdb_io.cdb_bytes[0];
	    }
	    if (cmd == INQUIRY) {
		target = csio->ccb_h.target_id;
		btl = &sc->mly_btl[bus][target];
		padstr(inq->vendor, mly_describe_code(mly_table_device_type, btl->mb_type), 8);
		padstr(inq->product, mly_describe_code(mly_table_device_state, btl->mb_state), 16);
		padstr(inq->revision, "", 4);
	    }
	}

	debug(2, "SCSI_STATUS_OK");
	csio->ccb_h.status = CAM_REQ_CMP;
	break;

    case SCSI_STATUS_CHECK_COND:
	debug(2, "SCSI_STATUS_CHECK_COND  sense %d  resid %d", mc->mc_sense, mc->mc_resid);
	csio->ccb_h.status = CAM_SCSI_STATUS_ERROR;
	bzero(&csio->sense_data, SSD_FULL_SIZE);
	bcopy(mc->mc_packet, &csio->sense_data, mc->mc_sense);
	csio->sense_len = mc->mc_sense;
	csio->ccb_h.status |= CAM_AUTOSNS_VALID;
	csio->resid = mc->mc_resid;	/* XXX this is a signed value... */
	break;

    case SCSI_STATUS_BUSY:
	debug(2, "SCSI_STATUS_BUSY");
	csio->ccb_h.status = CAM_SCSI_BUSY;
	break;

    default:
	debug(2, "unknown status 0x%x", csio->scsi_status);
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
	break;
    }
    xpt_done((union ccb *)csio);
    mly_release_command(mc);
}

/********************************************************************************
 * Find a peripheral attahed at (bus),(target)
 */
static struct cam_periph *
mly_find_periph(struct mly_softc *sc, int bus, int target)
{
    struct cam_periph	*periph;
    struct cam_path	*path;
    int			status;

    status = xpt_create_path(&path, NULL, cam_sim_path(sc->mly_cam_sim[bus]), target, 0);
    if (status == CAM_REQ_CMP) {
	periph = cam_periph_find(path, NULL);
	xpt_free_path(path);
    } else {
	periph = NULL;
    }
    return(periph);
}

/********************************************************************************
 * Name the device at (bus)(target)
 */
int
mly_name_device(struct mly_softc *sc, int bus, int target)
{
    struct cam_periph	*periph;

    if ((periph = mly_find_periph(sc, bus, target)) != NULL) {
	sprintf(sc->mly_btl[bus][target].mb_name, "%s%d", periph->periph_name, periph->unit_number);
	return(0);
    }
    sc->mly_btl[bus][target].mb_name[0] = 0;
    return(ENOENT);
}
