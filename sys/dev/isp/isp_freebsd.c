/* $Id: isp_freebsd.c,v 1.10.2.3 1999/05/12 23:26:41 mjacob Exp $ */
/* release_6_2_99 */
/*
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <dev/isp/isp_freebsd.h>

static void isp_cam_async __P((void *, u_int32_t, struct cam_path *, void *));
static void isp_poll __P((struct cam_sim *));
static void isp_action __P((struct cam_sim *, union ccb *));

void
isp_attach(struct ispsoftc *isp)
{
	int primary, secondary;
	struct ccb_setasync csa;
	struct cam_devq *devq;
	struct cam_sim *sim;
	struct cam_path *path;

	/*
	 * Establish (in case of 12X0) which bus is the primary.
	 */

	primary = 0;
	secondary = 1;

	/*
	 * Create the device queue for our SIM(s).
	 */
	devq = cam_simq_alloc(MAXISPREQUEST);
	if (devq == NULL) {
		return;
	}

	/*
	 * Construct our SIM entry.
	 */
	sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp,
	    isp->isp_unit, 1, MAXISPREQUEST, devq);
	if (sim == NULL) {
		cam_simq_free(devq);
		return;
	}
	if (xpt_bus_register(sim, primary) != CAM_SUCCESS) {
		cam_sim_free(sim, TRUE);
		return;
	}

	if (xpt_create_path(&path, NULL, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, TRUE);
		return;
	}

	xpt_setup_ccb(&csa.ccb_h, path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = isp_cam_async;
	csa.callback_arg = sim;
	xpt_action((union ccb *)&csa);
	isp->isp_sim = sim;
	isp->isp_path = path;

	/*
	 * If we have a second channel, construct SIM entry for that.
	 */
	if (IS_12X0(isp)) {
		sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp,
		    isp->isp_unit, 1, MAXISPREQUEST, devq);
		if (sim == NULL) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_simq_free(devq);
			return;
		}
		if (xpt_bus_register(sim, secondary) != CAM_SUCCESS) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_sim_free(sim, TRUE);
			return;
		}

		if (xpt_create_path(&path, NULL, cam_sim_path(sim),
		    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			xpt_bus_deregister(cam_sim_path(sim));
			cam_sim_free(sim, TRUE);
			return;
		}

		xpt_setup_ccb(&csa.ccb_h, path, 5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_LOST_DEVICE;
		csa.callback = isp_cam_async;
		csa.callback_arg = sim;
		xpt_action((union ccb *)&csa);
		isp->isp_sim2 = sim;
		isp->isp_path2 = path;
	}
	if (isp->isp_state == ISP_INITSTATE)
		isp->isp_state = ISP_RUNSTATE;
}

static void
isp_cam_async(void *cbarg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct cam_sim *sim;
	struct ispsoftc *isp;

	sim = (struct cam_sim *)cbarg;
	isp = (struct ispsoftc *) cam_sim_softc(sim);
	switch (code) {
	case AC_LOST_DEVICE:
		if (isp->isp_type & ISP_HA_SCSI) {
			u_int16_t oflags, nflags;
			sdparam *sdp = isp->isp_param;
			int s, tgt = xpt_path_target_id(path);

			s = splcam();
			sdp += cam_sim_bus(sim);
			isp->isp_update |= (1 << cam_sim_bus(sim));

			nflags = DPARM_SAFE_DFLT;
			if (ISP_FW_REVX(isp->isp_fwrev) >=
			    ISP_FW_REV(7, 55, 0)) {
				nflags |= DPARM_NARROW | DPARM_ASYNC;
			}
			oflags = sdp->isp_devparam[tgt].dev_flags;
			sdp->isp_devparam[tgt].dev_flags = nflags;
			sdp->isp_devparam[tgt].dev_update = 1;
			(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, NULL);
			sdp->isp_devparam[tgt].dev_flags = oflags;
			(void) splx(s);
		}
		break;
	default:
		break;
	}
}

static void
isp_poll(struct cam_sim *sim)
{
	isp_intr((struct ispsoftc *) cam_sim_softc(sim));
}


static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	int s, tgt, error;
	struct ispsoftc *isp;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));
	
	isp = (struct ispsoftc *)cam_sim_softc(sim);
	ccb->ccb_h.sim_priv.entries[0].field = 0;
	ccb->ccb_h.sim_priv.entries[1].ptr = isp;
	/*
	 * This should only happen for Fibre Channel adapters.
	 * We want to pass through all but XPT_SCSI_IO (e.g.,
	 * path inquiry) but fail if we can't get good Fibre
	 * Channel link status.
	 */
	if (ccb->ccb_h.func_code == XPT_SCSI_IO &&
	    isp->isp_state != ISP_RUNSTATE) {
		s = splcam();
		DISABLE_INTS(isp);
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			(void) splx(s);
			/*
			 * Lie. Say it was a selection timeout.
			 */
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			xpt_done(ccb);
			return;
		}
		isp->isp_state = ISP_RUNSTATE;
		ENABLE_INTS(isp);
		(void) splx(s);
	}
	
	IDPRINTF(4, ("%s: isp_action code %x\n", isp->isp_name,
	    ccb->ccb_h.func_code));

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		/*
		 * Do a couple of preliminary checks...
		 */
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) != 0) {
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}
		}

		
		if (isp->isp_type & ISP_HA_SCSI) {
			if (ccb->ccb_h.target_id > (MAX_TARGETS-1)) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
			} else if (ISP_FW_REVX(isp->isp_fwrev) >=
			    ISP_FW_REV(7, 55, 0)) {
				/*
				 * Too much breakage.
				 */
#if	0
				if (ccb->ccb_h.target_lun > 31) {
					ccb->ccb_h.status = CAM_PATH_INVALID;
				}
#else
				if (ccb->ccb_h.target_lun > 7) {
					ccb->ccb_h.status = CAM_PATH_INVALID;
				}
#endif
			} else if (ccb->ccb_h.target_lun > 7) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
			}
		} else {
			if (ccb->ccb_h.target_id > (MAX_FC_TARG-1)) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
#ifdef	ISP2100_SCCLUN
			} else if (ccb->ccb_h.target_lun > 65535) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
#else
			} else if (ccb->ccb_h.target_lun > 15) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
#endif
			}
		}
		if (ccb->ccb_h.status == CAM_PATH_INVALID) {
			printf("%s: invalid tgt/lun (%d.%d) in XPT_SCSI_IO\n",
			    isp->isp_name, ccb->ccb_h.target_id,
			    ccb->ccb_h.target_lun);
			xpt_done(ccb);
			break;
		}
		s = splcam();
		DISABLE_INTS(isp);
		switch (ispscsicmd((ISP_SCSI_XFER_T *) ccb)) {
		case CMD_QUEUED:
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
			break;
		case CMD_EAGAIN:
			if (!(isp->isp_osinfo.simqfrozen & SIMQFRZ_RESOURCE)) {
				xpt_freeze_simq(sim, 1);
				isp->isp_osinfo.simqfrozen |= SIMQFRZ_RESOURCE;
			}
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		case CMD_COMPLETE:
			/*
			 * Just make sure that we didn't get it returned
			 * as completed, but with the request still in
			 * progress. In theory, 'cannot happen'.
			 */
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_REQ_INPROG) {
				ccb->ccb_h.status &= ~CAM_STATUS_MASK;
				ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
			}
			xpt_done(ccb);
			break;
		}
		ENABLE_INTS(isp);
		splx(s);
		break;

	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_RESET_DEV:		/* BDR the specified SCSI device */
		tgt = ccb->ccb_h.target_id; /* XXX: Which Bus? */
		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_DEV, &tgt);
		(void) splx(s);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;
	case XPT_ABORT:			/* Abort the specified CCB */
		s = splcam();
		error = isp_control(isp, ISPCTL_ABORT_CMD, ccb);
		(void) splx(s);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;

	case XPT_SET_TRAN_SETTINGS:	/* Nexus Settings */

		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		s = splcam();
		if (isp->isp_type & ISP_HA_FC) {
			;	/* nothing to change */
		} else {
			sdparam *sdp = isp->isp_param;
			u_int16_t *dptr;
			int bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));

			sdp += bus;
#if	0
			if (cts->flags & CCB_TRANS_CURRENT_SETTINGS)
				dptr = &sdp->isp_devparam[tgt].cur_dflags;
			else
				dptr = &sdp->isp_devparam[tgt].dev_flags;
#else
			/*
			 * We always update (internally) from dev_flags
			 * so any request to change settings just gets
			 * vectored to that location.
			 */
			dptr = &sdp->isp_devparam[tgt].dev_flags;
#endif

			/*
			 * Note that these operations affect the
			 * the goal flags (dev_flags)- not
			 * the current state flags. Then we mark
			 * things so that the next operation to
			 * this HBA will cause the update to occur.
			 */
			if (cts->valid & CCB_TRANS_DISC_VALID) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0) {
					*dptr |= DPARM_DISC;
				} else {
					*dptr &= ~DPARM_DISC;
				}
			}
			if (cts->valid & CCB_TRANS_TQ_VALID) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0) {
					*dptr |= DPARM_TQING;
				} else {
					*dptr &= ~DPARM_TQING;
				}
			}
			if (cts->valid & CCB_TRANS_BUS_WIDTH_VALID) {
				switch (cts->bus_width) {
				case MSG_EXT_WDTR_BUS_16_BIT:
					*dptr |= DPARM_WIDE;
					break;
				default:
					*dptr &= ~DPARM_WIDE;
				}
			}
			/*
			 * Any SYNC RATE of nonzero and SYNC_OFFSET
			 * of nonzero will cause us to go to the
			 * selected (from NVRAM) maximum value for
			 * this device. At a later point, we'll
			 * allow finer control.
			 */
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) &&
			    (cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) &&
			    (cts->sync_offset > 0)) {
				*dptr |= DPARM_SYNC;
			} else {
				*dptr &= ~DPARM_SYNC;
			}
			if (bootverbose || isp->isp_dblev >= 3)
				printf("%s: %d.%d set %s period 0x%x offset "
				    "0x%x flags 0x%x\n", isp->isp_name, bus,
				    tgt,
				    (cts->flags & CCB_TRANS_CURRENT_SETTINGS)?
				    "current" : "user", 
				    sdp->isp_devparam[tgt].sync_period,
				    sdp->isp_devparam[tgt].sync_offset,
				    sdp->isp_devparam[tgt].dev_flags);
			s = splcam();
			sdp->isp_devparam[tgt].dev_update = 1;
			isp->isp_update |= (1 << bus);
			(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, NULL);
			(void) splx(s);
		}
		(void) splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:

		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		if (isp->isp_type & ISP_HA_FC) {
			/*
			 * a lot of normal SCSI things don't make sense.
			 */
			cts->flags = CCB_TRANS_TAG_ENB | CCB_TRANS_DISC_ENB;
			cts->valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			/*
			 * How do you measure the width of a high
			 * speed serial bus? Well, in bytes.
			 *
			 * Offset and period make no sense, though, so we set
			 * (above) a 'base' transfer speed to be gigabit.
			 */
			cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		} else {
			sdparam *sdp = isp->isp_param;
			u_int16_t dval, pval, oval;
			int bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));

			sdp += bus;
			if (cts->flags & CCB_TRANS_CURRENT_SETTINGS) {
				s = splcam();
				/*
				 * First do a refresh to see if things
				 * have changed recently!
				 */
				sdp->isp_devparam[tgt].dev_refresh = 1;
				isp->isp_update |= (1 << bus);
				(void) isp_control(isp, ISPCTL_UPDATE_PARAMS,
				    NULL);
				(void) splx(s);
				dval = sdp->isp_devparam[tgt].cur_dflags;
				oval = sdp->isp_devparam[tgt].cur_offset;
				pval = sdp->isp_devparam[tgt].cur_period;
			} else {
				dval = sdp->isp_devparam[tgt].dev_flags;
				oval = sdp->isp_devparam[tgt].sync_offset;
				pval = sdp->isp_devparam[tgt].sync_period;
			}

			s = splcam();
			cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);

			if (dval & DPARM_DISC) {
				cts->flags |= CCB_TRANS_DISC_ENB;
			}
			if (dval & DPARM_TQING) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			if (dval & DPARM_WIDE) {
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			cts->valid = CCB_TRANS_BUS_WIDTH_VALID |
			    CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;

			if ((dval & DPARM_SYNC) && oval != 0) {
				cts->sync_period = pval;
				cts->sync_offset = oval;
				cts->valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
			splx(s);
			if (bootverbose || isp->isp_dblev >= 3)
				printf("%s: %d.%d get %s period 0x%x offset "
				    "0x%x flags 0x%x\n", isp->isp_name, bus,
				    tgt,
			    	    (cts->flags & CCB_TRANS_CURRENT_SETTINGS)?
				    "current" : "user", pval, oval, dval);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
		u_int32_t secs_per_cylinder;
		u_int32_t size_mb;

		ccg = &ccb->ccg;
		if (ccg->block_size == 0) {
			printf("%s: %d.%d XPT_CALC_GEOMETRY block size 0?\n",
				isp->isp_name, ccg->ccb_h.target_id,
				ccg->ccb_h.target_lun);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		size_mb = ccg->volume_size /((1024L * 1024L) / ccg->block_size);
		if (size_mb > 1024) {
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
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified bus */
		tgt = cam_sim_bus(sim);
		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_BUS, &tgt);
		(void) splx(s);
		if (error)
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else {
			if (cam_sim_bus(sim) && isp->isp_path2 != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
			else if (isp->isp_path != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* Does this need to be implemented? */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_eng_cnt = 0;
		if (IS_FC(isp)) {
			cpi->hba_misc = PIM_NOBUSRESET;
			cpi->max_target = MAX_FC_TARG-1;
			cpi->initiator_id =
			    ((fcparam *)isp->isp_param)->isp_loopid;
#ifdef	ISP2100_SCCLUN
#if	0
			/* Too much breakage as yet... */
			cpi->max_lun = (1 << 16) - 1;
#else
			cpi->max_lun = (1 << 4) - 1;
#endif
#else
			cpi->max_lun = (1 << 4) - 1;
#endif
			/*
			 * Set base transfer capabilities for Fibre Channel.
			 * Technically not correct because we don't know
			 * what media we're running on top of- but we'll
			 * look good if we always say 100MB/s.
			 */
			cpi->base_transfer_speed = 100000;
		} else {
			sdparam *sdp = isp->isp_param;
			sdp += cam_sim_bus(xpt_path_sim(cpi->ccb_h.path));
			cpi->hba_misc = 0;
			cpi->initiator_id = sdp->isp_initiator_id;
			cpi->max_target =  MAX_TARGETS-1;
			if (ISP_FW_REVX(isp->isp_fwrev) >=
			    ISP_FW_REV(7, 55, 0)) {
#if	0
				/*
				 * Too much breakage.
				 */
				cpi->max_lun = (1 << 5) - 1;
#else
				cpi->max_lun = (1 << 3) - 1;
#endif
			} else {
				cpi->max_lun = (1 << 3) - 1;
			}
			cpi->base_transfer_speed = 3300;
		}

		cpi->bus_id = cam_sim_bus(sim);
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Qlogic", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

#define	ISPDDB	(CAM_DEBUG_INFO|CAM_DEBUG_TRACE|CAM_DEBUG_CDB)
void
isp_done(struct ccb_scsiio *sccb)
{
	struct ispsoftc *isp = XS_ISP(sccb);

	if (XS_NOERR(sccb))
		XS_SETERR(sccb, CAM_REQ_CMP);
	sccb->ccb_h.status &= ~CAM_STATUS_MASK;
	sccb->ccb_h.status |= sccb->ccb_h.spriv_field0;
	if ((sccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP &&
	    (sccb->scsi_status != SCSI_STATUS_OK)) {
		sccb->ccb_h.status &= ~CAM_STATUS_MASK;
		sccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
	}
	if ((sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			IDPRINTF(3, ("%s: freeze devq %d.%d ccbstat 0x%x\n",
			    isp->isp_name, sccb->ccb_h.target_id,
			    sccb->ccb_h.target_lun, sccb->ccb_h.status));
			xpt_freeze_devq(sccb->ccb_h.path, 1);
			sccb->ccb_h.status |= CAM_DEV_QFRZN;
		}
	}
	if (isp->isp_osinfo.simqfrozen & SIMQFRZ_RESOURCE) {
		isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_RESOURCE;
		sccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		xpt_release_simq(isp->isp_sim, 1);
	}
	sccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	if (CAM_DEBUGGED(sccb->ccb_h.path, ISPDDB) &&
	    (sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print_path(sccb->ccb_h.path);
		printf("cam completion status 0x%x\n", sccb->ccb_h.status);
	}
	xpt_done((union ccb *) sccb);
}

int
isp_async(isp, cmd, arg)
	struct ispsoftc *isp;
	ispasync_t cmd;
	void *arg;
{
	int bus, rv = 0;
	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
		if (isp->isp_type & ISP_HA_SCSI) {
			int flags, tgt;
			sdparam *sdp = isp->isp_param;
			struct ccb_trans_settings neg;
			struct cam_path *tmppath;

			tgt = *((int *)arg);
			bus = (tgt >> 16) & 0xffff;
			tgt &= 0xffff;
			sdp += bus;
			if (xpt_create_path(&tmppath, NULL,
			    cam_sim_path(bus? isp->isp_sim2 : isp->isp_sim),
			    tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
				xpt_print_path(isp->isp_path);
				printf("isp_async cannot make temp path for "
				    "target %d bus %d\n", tgt, bus);
				rv = -1;
				break;
			}
			flags = sdp->isp_devparam[tgt].cur_dflags;
			neg.valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			if (flags & DPARM_DISC) {
				neg.flags |= CCB_TRANS_DISC_ENB;
			}
			if (flags & DPARM_TQING) {
				neg.flags |= CCB_TRANS_TAG_ENB;
			}
			neg.valid |= CCB_TRANS_BUS_WIDTH_VALID;
			neg.bus_width = (flags & DPARM_WIDE)?
			    MSG_EXT_WDTR_BUS_8_BIT : MSG_EXT_WDTR_BUS_16_BIT;
			neg.sync_period = sdp->isp_devparam[tgt].cur_period;
			neg.sync_offset = sdp->isp_devparam[tgt].cur_offset;
			if (flags & DPARM_SYNC) {
				neg.valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
			IDPRINTF(3, ("%s: NEW_TGT_PARAMS bus %d tgt %d period "
			    "0x%x offset 0x%x flags 0x%x\n", isp->isp_name,
			    bus, tgt, neg.sync_period, neg.sync_offset, flags));
			xpt_setup_ccb(&neg.ccb_h, tmppath, 1);
			xpt_async(AC_TRANSFER_NEG, tmppath, &neg);
			xpt_free_path(tmppath);
		}
		break;
	case ISPASYNC_BUS_RESET:
		bus = *((int *)arg);
		printf("%s: SCSI bus reset on bus %d detected\n",
		    isp->isp_name, bus);
		if (bus > 0 && isp->isp_path2) {
			xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
		} else if (isp->isp_path) {
			xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
		}
		break;
	case ISPASYNC_LOOP_DOWN:
		if (isp->isp_path) {
			/*
			 * We can get multiple LOOP downs, so only count one.
			 */
			if (!(isp->isp_osinfo.simqfrozen & SIMQFRZ_LOOPDOWN)) {
				xpt_freeze_simq(isp->isp_sim, 1);
				isp->isp_osinfo.simqfrozen |= SIMQFRZ_LOOPDOWN;
				printf("%s: Loop DOWN- freezing SIMQ until Loop"
				    " comes up\n", isp->isp_name);
			}
		} else {
			printf("%s: Loop DOWN\n", isp->isp_name);
		}
		break;
	case ISPASYNC_LOOP_UP:
		if (isp->isp_path) {
			if (isp->isp_osinfo.simqfrozen & SIMQFRZ_LOOPDOWN) {
				xpt_release_simq(isp->isp_sim, 1);
				isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_LOOPDOWN;
				if (isp->isp_osinfo.simqfrozen) {
					printf("%s: Loop UP- SIMQ still "
					    "frozen\n", isp->isp_name);
				} else {
					printf("%s: Loop UP-releasing frozen "
					    "SIMQ\n", isp->isp_name);
				}
			}
		} else {
			printf("%s: Loop UP\n", isp->isp_name);
		}
		break;
	case ISPASYNC_PDB_CHANGED:
	if (IS_FC(isp)) {
		const char *fmt = "%s: Target %d (Loop 0x%x) Port ID 0x%x "
		    "role %s %s\n Port WWN 0x%08x%08x\n Node WWN 0x%08x%08x\n";
		const static char *roles[4] = {
		    "No", "Target", "Initiator", "Target/Initiator"
		};
		char *ptr;
		fcparam *fcp = isp->isp_param;
		int tgt = *((int *) arg);
		struct lportdb *lp = &fcp->portdb[tgt]; 

		if (lp->valid) {
			ptr = "arrived";
		} else {
			ptr = "disappeared";
		}
		printf(fmt, isp->isp_name, tgt, lp->loopid, lp->portid,
		    roles[lp->roles & 0x3], ptr,
		    (u_int32_t) (lp->port_wwn >> 32),
		    (u_int32_t) (lp->port_wwn & 0xffffffffLL),
		    (u_int32_t) (lp->node_wwn >> 32),
		    (u_int32_t) (lp->node_wwn & 0xffffffffLL));
		break;
	}
#ifdef	ISP2100_FABRIC
	case ISPASYNC_CHANGE_NOTIFY:
		printf("%s: Name Server Database Changed\n", isp->isp_name);
		break;
	case ISPASYNC_FABRIC_DEV:
	{
		int target;
		struct lportdb *lp;
		sns_scrsp_t *resp = (sns_scrsp_t *) arg;
		u_int32_t portid;
		u_int64_t wwn;
		fcparam *fcp = isp->isp_param;

		rv = -1;

		portid =
		    (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));
		wwn =
		    (((u_int64_t)resp->snscb_portname[0]) << 56) |
		    (((u_int64_t)resp->snscb_portname[1]) << 48) |
		    (((u_int64_t)resp->snscb_portname[2]) << 40) |
		    (((u_int64_t)resp->snscb_portname[3]) << 32) |
		    (((u_int64_t)resp->snscb_portname[4]) << 24) |
		    (((u_int64_t)resp->snscb_portname[5]) << 16) |
		    (((u_int64_t)resp->snscb_portname[6]) <<  8) |
		    (((u_int64_t)resp->snscb_portname[7]));
		printf("%s: type 0x%x@portid 0x%x 0x%08x%08x\n",
		    isp->isp_name, resp->snscb_port_type, portid,
		    ((u_int32_t)(wwn >> 32)), ((u_int32_t)(wwn & 0xffffffff)));

		if (resp->snscb_port_type != 2) {
			rv = 0;
			break;
		}
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == wwn)
				break;
		}
		if (target < MAX_FC_TARG) {
			rv = 0;
			break;
		}
		for (target = FC_SNS_ID+1; target < MAX_FC_TARG; target++) {
			lp = &fcp->portdb[target];
			if (lp->port_wwn == 0)
				break;
		}
		if (target == MAX_FC_TARG) {
			printf("%s: no more space for fabric devices\n",
			    isp->isp_name);
			break;
		}
		lp->port_wwn = lp->node_wwn = wwn;
		lp->portid = portid;
		rv = 0;
		break;
	}
#endif
	default:
		rv = -1;
		break;
	}
	return (rv);
}


/*
 * Locks are held before coming here.
 */
void
isp_uninit(struct ispsoftc *isp)
{
	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	DISABLE_INTS(isp);
}
