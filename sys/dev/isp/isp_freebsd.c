/* $FreeBSD$ */
/* $Id: isp_freebsd.c,v 1.3 1998/09/15 08:42:55 gibbs Exp $ */
/*
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
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

#ifdef	SCSI_CAM
static void isp_async __P((void *, u_int32_t, struct cam_path *, void *));
static void isp_poll __P((struct cam_sim *));
static void isp_action __P((struct cam_sim *, union ccb *));

void
isp_attach(isp)
	struct ispsoftc *isp;
{
	struct ccb_setasync csa;
	struct cam_devq *devq;

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(MAXISPREQUEST);
	if (devq == NULL) {
		return;
	}

	/*
	 * Construct our SIM entry
	 */
	isp->isp_sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp,
	    isp->isp_unit, 1, MAXISPREQUEST, devq);
	if (isp->isp_sim == NULL) {
		cam_simq_free(devq);
		return;
	}
	if (xpt_bus_register(isp->isp_sim, 0) != CAM_SUCCESS) {
		cam_sim_free(isp->isp_sim, TRUE);
		return;
	}

	if (xpt_create_path(&isp->isp_path, NULL, cam_sim_path(isp->isp_sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(isp->isp_sim));
		cam_sim_free(isp->isp_sim, TRUE);
		return;
	}

	xpt_setup_ccb(&csa.ccb_h, isp->isp_path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = isp_async;
	csa.callback_arg = isp->isp_sim;
	xpt_action((union ccb *)&csa);

	/*
	 * Set base transfer capabilities for Fibre Channel.
	 * Technically not correct because we don't know
	 * what media we're running on top of- but we'll
	 * look good if we always say 100MB/s.
	 */
	if (isp->isp_type & ISP_HA_FC) {
		isp->isp_sim->base_transfer_speed = 100000;
	}
	isp->isp_state = ISP_RUNSTATE;
}

static void
isp_async(cbarg, code, path, arg)
	void *cbarg;
	u_int32_t code;
	struct cam_path *path;
	void *arg;
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

			nflags = DPARM_SAFE_DFLT;
			if (isp->isp_fwrev >= ISP_FW_REV(7, 55)) {
				nflags |= DPARM_NARROW | DPARM_ASYNC;
			}
			oflags = sdp->isp_devparam[tgt].dev_flags;
			sdp->isp_devparam[tgt].dev_flags = nflags;
			sdp->isp_devparam[tgt].dev_update = 1;

			s = splcam();
			(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, NULL);
			(void) splx(s);
			sdp->isp_devparam[tgt].dev_flags = oflags;
		}
		break;
	default:
		break;
	}
}

static void
isp_poll(sim)
	struct cam_sim *sim;
{
	isp_intr((struct ispsoftc *) cam_sim_softc(sim));
}


static void
isp_action(sim, ccb)
	struct cam_sim *sim;
	union ccb *ccb;
{
	int s, tgt, error;
	struct ispsoftc *isp;
	struct ccb_trans_settings *cts, set;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));
	
	isp = (struct ispsoftc *)cam_sim_softc(sim);
	ccb->ccb_h.sim_priv.entries[0].field = 0;
	ccb->ccb_h.sim_priv.entries[1].ptr = isp;
	
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
			} else if (isp->isp_fwrev >= ISP_FW_REV(7, 55)) {
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
			} else if (ccb->ccb_h.target_lun > 15) {
				ccb->ccb_h.status = CAM_PATH_INVALID;
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
		switch (ispscsicmd((ISP_SCSI_XFER_T *) ccb)) {
		case CMD_QUEUED:
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
			break;
		case CMD_EAGAIN:
			printf("%s: EAGAINed %d.%d\n", isp->isp_name,
			    ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
			printf("%s: %d EAGAIN\n", __FILE__, __LINE__);
			xpt_freeze_simq(sim, 1);
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		case CMD_COMPLETE:
			printf("%s: COMPLETEd for %d.%d with cam status 0%x\n",
			    isp->isp_name, ccb->ccb_h.target_id,
			    ccb->ccb_h.target_lun, ccb->ccb_h.status);
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) !=
			    CAM_REQ_INPROG) {
				/* XXX: Cannot Happen */
				ccb->ccb_h.status &= ~CAM_STATUS_MASK;
				ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
			}
			xpt_done(ccb);
			break;
		}
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
		tgt = ccb->ccb_h.target_id;
		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_DEV, (void *)(intptr_t) tgt);
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
			 * the permanent flags (dev_flags)- not
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
			IDPRINTF(3, ("%s: target %d new dev_flags 0x%x\n",
			    isp->isp_name, tgt,
			    sdp->isp_devparam[tgt].dev_flags));
			s = splcam();
			sdp->isp_devparam[tgt].dev_update = 1;
			isp->isp_update = 1;
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
			u_int16_t dval;

			if (cts->flags & CCB_TRANS_CURRENT_SETTINGS)
				dval = sdp->isp_devparam[tgt].cur_dflags;
			else
				dval = sdp->isp_devparam[tgt].dev_flags;

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

			if ((dval & DPARM_SYNC) &&
			    (sdp->isp_devparam[tgt].sync_offset)) {
				cts->sync_period =
				    sdp->isp_devparam[tgt].sync_period;
				cts->sync_offset =
				    sdp->isp_devparam[tgt].sync_offset;
				cts->valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
			splx(s);
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
		if (isp->isp_type & ISP_HA_FC) {
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
                	break;
		}
		s = splcam();
		error = isp_control(isp, ISPCTL_RESET_BUS, NULL);
		(void) splx(s);
		if (error)
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else
			ccb->ccb_h.status = CAM_REQ_CMP;
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
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		if (isp->isp_type & ISP_HA_FC) {
			cpi->max_target = MAX_FC_TARG-1;
			cpi->initiator_id =
			    ((fcparam *)isp->isp_param)->isp_loopid;
			/*
			 * XXX: actually, this is not right if we have
			 * XXX: 1.14 F/W and the second level lun addressing
			 * XXX: in place. It's also probably not right
			 * XXX: even for 1.13 f/w.
			 */
			cpi->max_lun = 15;
		} else {
			cpi->initiator_id =
			    ((sdparam *)isp->isp_param)->isp_initiator_id;
			cpi->max_target =  MAX_TARGETS-1;
			if (isp->isp_fwrev >= ISP_FW_REV(7, 55)) {
				/*
				 * Too much breakage.
				 */
#if	0
				cpi->max_lun = 31;
#else
				cpi->max_lun = 7;
#endif
			} else {
				cpi->max_lun = 7;
			}
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
#else

static void ispminphys __P((struct buf *));
static u_int32_t isp_adapter_info __P((int));
static int ispcmd __P((ISP_SCSI_XFER_T *));

static struct scsi_adapter isp_switch = {
	ispcmd, ispminphys, 0, 0, isp_adapter_info, "isp", { 0, 0 }
};
static struct scsi_device isp_dev = {
	NULL, NULL, NULL, NULL, "isp", 0, { 0, 0 }
};
static int isp_poll __P((struct ispsoftc *, ISP_SCSI_XFER_T *, int));


/*
 * Complete attachment of hardware, include subdevices.
 */
void
isp_attach(isp)
	struct ispsoftc *isp;
{
	struct scsibus_data *scbus;

	scbus = scsi_alloc_bus(); 
	if(!scbus) {
		return;
	}
	isp->isp_state = ISP_RUNSTATE;

	isp->isp_osinfo._link.adapter_unit = isp->isp_osinfo.unit;
	isp->isp_osinfo._link.adapter_softc = isp;
	isp->isp_osinfo._link.adapter = &isp_switch;
	isp->isp_osinfo._link.device = &isp_dev;
	isp->isp_osinfo._link.flags = 0;
	if (isp->isp_type & ISP_HA_FC) {
		isp->isp_osinfo._link.adapter_targ =
			((fcparam *)isp->isp_param)->isp_loopid;
		scbus->maxtarg = MAX_FC_TARG-1;
	} else {
		isp->isp_osinfo._link.adapter_targ =
			((sdparam *)isp->isp_param)->isp_initiator_id;
		scbus->maxtarg = MAX_TARGETS-1;
	}
	/*
	 * Prepare the scsibus_data area for the upperlevel scsi code.
	 */ 
	scbus->adapter_link = &isp->isp_osinfo._link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);
}


/*
 * minphys our xfers
 *
 * Unfortunately, the buffer pointer describes the target device- not the
 * adapter device, so we can't use the pointer to find out what kind of
 * adapter we are and adjust accordingly.
 */

static void
ispminphys(bp)
	struct buf *bp;
{
	/*
	 * Only the 10X0 has a 24 bit limit.
	 */
	if (bp->b_bcount >= (1 << 24)) {
		bp->b_bcount = (1 << 24);
	}
}

static u_int32_t
isp_adapter_info(unit)
	int unit;
{
	/*
 	 * XXX: FIND ISP BASED UPON UNIT AND GET REAL QUEUE LIMIT FROM THAT
	 */
	return (2);
}

static int
ispcmd(xs)
	ISP_SCSI_XFER_T *xs;
{
	struct ispsoftc *isp;
	int r;
	ISP_LOCKVAL_DECL;

	isp = XS_ISP(xs);
	ISP_LOCK;
	r = ispscsicmd(xs);
	if (r != CMD_QUEUED || (xs->flags & SCSI_NOMASK) == 0) {
		ISP_UNLOCK;
		return (r);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (isp_poll(isp, xs, XS_TIME(xs))) {
		/*
		 * If no other error occurred but we didn't finish,
		 * something bad happened.
		 */
		if (XS_IS_CMD_DONE(xs) == 0) {
			isp->isp_nactive--;
			if (isp->isp_nactive < 0)
				isp->isp_nactive = 0;
			if (XS_NOERR(xs)) {
				isp_lostcmd(isp, xs);
				XS_SETERR(xs, HBA_BOTCH);
			}
		}
	}
	ISP_UNLOCK;
	return (CMD_COMPLETE);
}

static int
isp_poll(isp, xs, mswait)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
	int mswait;
{

	while (mswait) {
		/* Try the interrupt handling routine */
		(void)isp_intr((void *)isp);

		/* See if the xs is now done */
		if (XS_IS_CMD_DONE(xs))
			return (0);
		SYS_DELAY(1000);	/* wait one millisecond */
		mswait--;
	}
	return (1);
}
#endif
