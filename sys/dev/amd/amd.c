/*
 *********************************************************************
 *	FILE NAME  : amd.c
 *	     BY    : C.L. Huang 	(ching@tekram.com.tw)
 *		     Erich Chen     (erich@tekram.com.tw)
 *	Description: Device Driver for the amd53c974 PCI Bus Master
 *		     SCSI Host adapter found on cards such as
 *		     the Tekram DC-390(T).
 * (C)Copyright 1995-1999 Tekram Technology Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 *********************************************************************
 * $FreeBSD$
 */

/*
 *********************************************************************
 *	HISTORY:
 *
 *	REV#	DATE	NAME    	DESCRIPTION
 *	1.00  07/02/96	CLH	        First release for RELEASE-2.1.0
 *	1.01  08/20/96	CLH	        Update for RELEASE-2.1.5
 *	1.02  11/06/96	CLH	        Fixed more than 1 LUN scanning
 *	1.03  12/20/96	CLH	        Modify to support 2.2-ALPHA
 *	1.04  12/26/97	CLH	        Modify to support RELEASE-2.2.5
 *	1.05  01/01/99  ERICH CHEN	Modify to support RELEASE-3.0.x (CAM)
 *********************************************************************
 */

/* #define AMD_DEBUG0		*/
/* #define AMD_DEBUG_SCSI_PHASE */

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <dev/amd/amd.h>

#define PCI_DEVICE_ID_AMD53C974 	0x20201022ul
#define PCI_BASE_ADDR0	    		0x10

typedef u_int (phase_handler_t)(struct amd_softc *, struct amd_srb *, u_int);
typedef phase_handler_t *phase_handler_func_t;

static void amd_intr(void *vamd);
static int amdstart(struct amd_softc *amd, struct amd_srb * pSRB);
static phase_handler_t amd_NopPhase;

static phase_handler_t amd_DataOutPhase0;
static phase_handler_t amd_DataInPhase0;
#define amd_CommandPhase0 amd_NopPhase
static phase_handler_t amd_StatusPhase0;
static phase_handler_t amd_MsgOutPhase0;
static phase_handler_t amd_MsgInPhase0;
static phase_handler_t amd_DataOutPhase1;
static phase_handler_t amd_DataInPhase1;
static phase_handler_t amd_CommandPhase1;
static phase_handler_t amd_StatusPhase1;
static phase_handler_t amd_MsgOutPhase1;
static phase_handler_t amd_MsgInPhase1;

static void	amdsetupcommand(struct amd_softc *amd, struct amd_srb *srb);
static int	amdparsemsg(struct amd_softc *amd);
static int	amdhandlemsgreject(struct amd_softc *amd);
static void	amdconstructsdtr(struct amd_softc *amd,
				 u_int period, u_int offset);
static u_int	amdfindclockrate(struct amd_softc *amd, u_int *period);
static int	amdsentmsg(struct amd_softc *amd, u_int msgtype, int full);

static void DataIO_Comm(struct amd_softc *amd, struct amd_srb *pSRB, u_int dir);
static void amd_Disconnect(struct amd_softc *amd);
static void amd_Reselect(struct amd_softc *amd);
static void SRBdone(struct amd_softc *amd, struct amd_srb *pSRB);
static void amd_ScsiRstDetect(struct amd_softc *amd);
static void amd_ResetSCSIBus(struct amd_softc *amd);
static void RequestSense(struct amd_softc *amd, struct amd_srb *pSRB);
static void amd_InvalidCmd(struct amd_softc *amd);

static void amd_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nsegs,
			  int error);

#if 0
static void amd_timeout(void *arg1);
static void amd_reset(struct amd_softc *amd);
#endif
static u_int8_t * phystovirt(struct amd_srb *pSRB, u_int32_t xferCnt);

void    amd_linkSRB(struct amd_softc *amd);
static int amd_init(device_t);
static void amd_load_defaults(struct amd_softc *amd);
static void amd_load_eeprom_or_defaults(struct amd_softc *amd);
static int amd_EEpromInDO(struct amd_softc *amd);
static u_int16_t EEpromGetData1(struct amd_softc *amd);
static void amd_EnDisableCE(struct amd_softc *amd, int mode, int *regval);
static void amd_EEpromOutDI(struct amd_softc *amd, int *regval, int Carry);
static void amd_Prepare(struct amd_softc *amd, int *regval, u_int8_t EEpromCmd);
static void amd_ReadEEprom(struct amd_softc *amd);

static int amd_probe(device_t);
static int amd_attach(device_t);
static void amdcompletematch(struct amd_softc *amd, target_id_t target,
			     lun_id_t lun, u_int tag, struct srb_queue *queue,
			     cam_status status);
static void amdsetsync(struct amd_softc *amd, u_int target, u_int clockrate,
		       u_int period, u_int offset, u_int type);
static void amdsettags(struct amd_softc *amd, u_int target, int tagenb);

static __inline void amd_clear_msg_state(struct amd_softc *amd);

static __inline void
amd_clear_msg_state(struct amd_softc *amd)
{
	amd->msgout_len = 0;
	amd->msgout_index = 0;
	amd->msgin_index = 0;
}

static __inline uint32_t
amd_get_sense_bufaddr(struct amd_softc *amd, struct amd_srb *pSRB)
{
	int offset;

	offset = pSRB->TagNumber;
	return (amd->sense_busaddr + (offset * sizeof(struct scsi_sense_data)));
}

static __inline struct scsi_sense_data *
amd_get_sense_buf(struct amd_softc *amd, struct amd_srb *pSRB)
{
	int offset;

	offset = pSRB->TagNumber;
	return (&amd->sense_buffers[offset]);
}

static __inline uint32_t
amd_get_sense_bufsize(struct amd_softc *amd, struct amd_srb *pSRB)
{
	return (sizeof(struct scsi_sense_data));
}

/* CAM SIM entry points */
#define ccb_srb_ptr spriv_ptr0
#define ccb_amd_ptr spriv_ptr1
static void	amd_action(struct cam_sim *sim, union ccb *ccb);
static void	amd_poll(struct cam_sim *sim);

/*
 * State engine function tables indexed by SCSI phase number
 */
phase_handler_func_t amd_SCSI_phase0[] = {
	amd_DataOutPhase0,
	amd_DataInPhase0,
	amd_CommandPhase0,
	amd_StatusPhase0,
	amd_NopPhase,
	amd_NopPhase,
	amd_MsgOutPhase0,
	amd_MsgInPhase0
};

phase_handler_func_t amd_SCSI_phase1[] = {
	amd_DataOutPhase1,
	amd_DataInPhase1,
	amd_CommandPhase1,
	amd_StatusPhase1,
	amd_NopPhase,
	amd_NopPhase,
	amd_MsgOutPhase1,
	amd_MsgInPhase1
};

/*
 * EEProm/BIOS negotiation periods
 */
u_int8_t   eeprom_period[] = {
	 25,	/* 10.0MHz */
	 32,	/*  8.0MHz */
	 38,	/*  6.6MHz */
	 44,	/*  5.7MHz */
	 50,	/*  5.0MHz */
	 63,	/*  4.0MHz */
	 83,	/*  3.0MHz */
	125	/*  2.0MHz */
};

/*
 * chip clock setting to SCSI specified sync parameter table.
 */
u_int8_t tinfo_sync_period[] = {
	25,	/* 10.0 */
	32,	/* 8.0 */
	38,	/* 6.6 */
	44,	/* 5.7 */
	50,	/* 5.0 */
	57,	/* 4.4 */
	63,	/* 4.0 */
	70,	/* 3.6 */
	76,	/* 3.3 */
	83	/* 3.0 */
};

static __inline struct amd_srb *
amdgetsrb(struct amd_softc * amd)
{
	int     intflag;
	struct amd_srb *    pSRB;

	intflag = splcam();
	pSRB = TAILQ_FIRST(&amd->free_srbs);
	if (pSRB)
		TAILQ_REMOVE(&amd->free_srbs, pSRB, links);
	splx(intflag);
	return (pSRB);
}

static void
amdsetupcommand(struct amd_softc *amd, struct amd_srb *srb)
{
	struct scsi_request_sense sense_cmd;
	u_int8_t *cdb;
	u_int cdb_len;

	if (srb->SRBFlag & AUTO_REQSENSE) {
		sense_cmd.opcode = REQUEST_SENSE;
		sense_cmd.byte2 = srb->pccb->ccb_h.target_lun << 5;
		sense_cmd.unused[0] = 0;
		sense_cmd.unused[1] = 0;
		sense_cmd.length = sizeof(struct scsi_sense_data);
		sense_cmd.control = 0;
		cdb = &sense_cmd.opcode;
		cdb_len = sizeof(sense_cmd);
	} else {
		cdb = &srb->CmdBlock[0];
		cdb_len = srb->ScsiCmdLen;
	}
	amd_write8_multi(amd, SCSIFIFOREG, cdb, cdb_len);
}

/*
 * Attempt to start a waiting transaction.  Interrupts must be disabled
 * upon entry to this function.
 */
static void
amdrunwaiting(struct amd_softc *amd) {
	struct amd_srb *srb;

	if (amd->last_phase != SCSI_BUS_FREE)
		return;

	srb = TAILQ_FIRST(&amd->waiting_srbs);
	if (srb == NULL)
		return;
	
	if (amdstart(amd, srb) == 0) {
		TAILQ_REMOVE(&amd->waiting_srbs, srb, links);
		TAILQ_INSERT_HEAD(&amd->running_srbs, srb, links);
	}
}

static void
amdexecutesrb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct	 amd_srb *srb;
	union	 ccb *ccb;
	struct	 amd_softc *amd;
	int	 s;

	srb = (struct amd_srb *)arg;
	ccb = srb->pccb;
	amd = (struct amd_softc *)ccb->ccb_h.ccb_amd_ptr;

	if (error != 0) {
		if (error != EFBIG)
			printf("amd%d: Unexepected error 0x%x returned from "
			       "bus_dmamap_load\n", amd->unit, error);
		if (ccb->ccb_h.status == CAM_REQ_INPROG) {
			xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
			ccb->ccb_h.status = CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		TAILQ_INSERT_HEAD(&amd->free_srbs, srb, links);
		xpt_done(ccb);
		return;
	}

	if (nseg != 0) {
		struct amd_sg *sg;
		bus_dma_segment_t *end_seg;
		bus_dmasync_op_t op;

		end_seg = dm_segs + nseg;

		/* Copy the segments into our SG list */
		srb->pSGlist = &srb->SGsegment[0];
		sg = srb->pSGlist;
		while (dm_segs < end_seg) {
			sg->SGXLen = dm_segs->ds_len;
			sg->SGXPtr = dm_segs->ds_addr;
			sg++;
			dm_segs++;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(amd->buffer_dmat, srb->dmamap, op);

	}
	srb->SGcount = nseg;
	srb->SGIndex = 0;
	srb->AdaptStatus = 0;
	srb->TargetStatus = 0;
	srb->MsgCnt = 0;
	srb->SRBStatus = 0;
	srb->SRBFlag = 0;
	srb->SRBState = 0;
	srb->TotalXferredLen = 0;
	srb->SGPhysAddr = 0;
	srb->SGToBeXferLen = 0;
	srb->EndMessage = 0;

	s = splcam();

	/*
	 * Last time we need to check if this CCB needs to
	 * be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(amd->buffer_dmat, srb->dmamap);
		TAILQ_INSERT_HEAD(&amd->free_srbs, srb, links);
		xpt_done(ccb);
		splx(s);
		return;
	}
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
#if 0
	/* XXX Need a timeout handler */
	ccb->ccb_h.timeout_ch =
	    timeout(amdtimeout, (caddr_t)srb,
		    (ccb->ccb_h.timeout * hz) / 1000);
#endif
	TAILQ_INSERT_TAIL(&amd->waiting_srbs, srb, links);
	amdrunwaiting(amd);
	splx(s);
}

static void
amd_action(struct cam_sim * psim, union ccb * pccb)
{
	struct amd_softc *    amd;
	u_int   target_id;

	CAM_DEBUG(pccb->ccb_h.path, CAM_DEBUG_TRACE, ("amd_action\n"));

	amd = (struct amd_softc *) cam_sim_softc(psim);
	target_id = pccb->ccb_h.target_id;

	switch (pccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct amd_srb *    pSRB;
		struct ccb_scsiio *pcsio;

		pcsio = &pccb->csio;

		/*
		 * Assign an SRB and connect it with this ccb.
		 */
		pSRB = amdgetsrb(amd);

		if (!pSRB) {
			/* Freeze SIMQ */
			pccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(pccb);
			return;
		}
		pSRB->pccb = pccb;
		pccb->ccb_h.ccb_srb_ptr = pSRB;
		pccb->ccb_h.ccb_amd_ptr = amd;
		pSRB->ScsiCmdLen = pcsio->cdb_len;
		bcopy(pcsio->cdb_io.cdb_bytes, pSRB->CmdBlock, pcsio->cdb_len);
		if ((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			if ((pccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) {
				/*
				 * We've been given a pointer
				 * to a single buffer.
				 */
				if ((pccb->ccb_h.flags & CAM_DATA_PHYS) == 0) {
					int s;
					int error;

					s = splsoftvm();
					error =
					    bus_dmamap_load(amd->buffer_dmat,
							    pSRB->dmamap,
							    pcsio->data_ptr,
							    pcsio->dxfer_len,
							    amdexecutesrb,
							    pSRB, /*flags*/0);
					if (error == EINPROGRESS) {
						/*
						 * So as to maintain
						 * ordering, freeze the
						 * controller queue
						 * until our mapping is
						 * returned.
						 */
						xpt_freeze_simq(amd->psim, 1);
						pccb->ccb_h.status |=
						    CAM_RELEASE_SIMQ;
					}
					splx(s);
				} else {
					struct bus_dma_segment seg;

					/* Pointer to physical buffer */
					seg.ds_addr =
					    (bus_addr_t)pcsio->data_ptr;
					seg.ds_len = pcsio->dxfer_len;
					amdexecutesrb(pSRB, &seg, 1, 0);
				}
			} else {
				struct bus_dma_segment *segs;

				if ((pccb->ccb_h.flags & CAM_SG_LIST_PHYS) == 0
				 || (pccb->ccb_h.flags & CAM_DATA_PHYS) != 0) {
					TAILQ_INSERT_HEAD(&amd->free_srbs,
							  pSRB, links);
					pccb->ccb_h.status = CAM_PROVIDE_FAIL;
					xpt_done(pccb);
					return;
				}

				/* Just use the segments provided */
				segs =
				    (struct bus_dma_segment *)pcsio->data_ptr;
				amdexecutesrb(pSRB, segs, pcsio->sglist_cnt, 0);
			}
		} else
			amdexecutesrb(pSRB, NULL, 0, 0);
		break;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &pccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE | PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7;
		cpi->max_lun = amd->max_lun;	/* 7 or 0 */
		cpi->initiator_id = amd->AdaptSCSIID;
		cpi->bus_id = cam_sim_bus(psim);
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "TRM-AMD", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(psim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(psim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(pccb);
		break;
	}
	case XPT_ABORT:
		pccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	case XPT_RESET_BUS:
	{

		int     i;

		amd_ResetSCSIBus(amd);
		amd->ACBFlag = 0;

		for (i = 0; i < 500; i++) {
			DELAY(1000);	/* Wait until our interrupt
					 * handler sees it */
		}

		pccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(pccb);
		break;
	}
	case XPT_RESET_DEV:
		pccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	case XPT_TERM_IO:
		pccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(pccb);
		/* XXX: intentional fall-through ?? */
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts;
		struct amd_target_info *targ_info;
		struct amd_transinfo *tinfo;
		int     intflag;

		cts = &pccb->cts;
		intflag = splcam();
		targ_info = &amd->tinfo[target_id];
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
			/* current transfer settings */
			if (targ_info->disc_tag & AMD_CUR_DISCENB) {
				cts->flags = CCB_TRANS_DISC_ENB;
			} else {
				cts->flags = 0;	/* no tag & disconnect */
			}
			if (targ_info->disc_tag & AMD_CUR_TAGENB) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			tinfo = &targ_info->current;
		} else {
			/* default(user) transfer settings */
			if (targ_info->disc_tag & AMD_USR_DISCENB) {
				cts->flags = CCB_TRANS_DISC_ENB;
			} else {
				cts->flags = 0;
			}
			if (targ_info->disc_tag & AMD_USR_TAGENB) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			tinfo = &targ_info->user;
		}

		cts->sync_period = tinfo->period;
		cts->sync_offset = tinfo->offset;
		cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		splx(intflag);
		cts->valid = CCB_TRANS_SYNC_RATE_VALID
			   | CCB_TRANS_SYNC_OFFSET_VALID
			   | CCB_TRANS_BUS_WIDTH_VALID
			   | CCB_TRANS_DISC_VALID
			   | CCB_TRANS_TQ_VALID;
		pccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(pccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts;
		struct amd_target_info *targ_info;
		u_int  update_type;
		int    intflag;
		int    last_entry;

		cts = &pccb->cts;
		update_type = 0;
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
			update_type |= AMD_TRANS_GOAL;
		} else if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
			update_type |= AMD_TRANS_USER;
		}
		if (update_type == 0
		 || update_type == (AMD_TRANS_USER|AMD_TRANS_GOAL)) {
			cts->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
		}

		intflag = splcam();
		targ_info = &amd->tinfo[target_id];

		if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
			if (update_type & AMD_TRANS_GOAL) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0) {
					targ_info->disc_tag |= AMD_CUR_DISCENB;
				} else {
					targ_info->disc_tag &= ~AMD_CUR_DISCENB;
				}
			}
			if (update_type & AMD_TRANS_USER) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0) {
					targ_info->disc_tag |= AMD_USR_DISCENB;
				} else {
					targ_info->disc_tag &= ~AMD_USR_DISCENB;
				}
			}
		}
		if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {
			if (update_type & AMD_TRANS_GOAL) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0) {
					targ_info->disc_tag |= AMD_CUR_TAGENB;
				} else {
					targ_info->disc_tag &= ~AMD_CUR_TAGENB;
				}
			}
			if (update_type & AMD_TRANS_USER) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0) {
					targ_info->disc_tag |= AMD_USR_TAGENB;
				} else {
					targ_info->disc_tag &= ~AMD_USR_TAGENB;
				}
			}
		}

		if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) == 0) {
			if (update_type & AMD_TRANS_GOAL)
				cts->sync_offset = targ_info->goal.offset;
			else
				cts->sync_offset = targ_info->user.offset;
		}

		if (cts->sync_offset > AMD_MAX_SYNC_OFFSET)
			cts->sync_offset = AMD_MAX_SYNC_OFFSET;

		if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) == 0) {
			if (update_type & AMD_TRANS_GOAL)
				cts->sync_period = targ_info->goal.period;
			else
				cts->sync_period = targ_info->user.period;
		}

		last_entry = sizeof(tinfo_sync_period) - 1;
		if ((cts->sync_period != 0)
		 && (cts->sync_period < tinfo_sync_period[0]))
			cts->sync_period = tinfo_sync_period[0];
		if (cts->sync_period > tinfo_sync_period[last_entry])
		 	cts->sync_period = 0;
		if (cts->sync_offset == 0)
			cts->sync_period = 0;

		if ((update_type & AMD_TRANS_USER) != 0) {
			targ_info->user.period = cts->sync_period;
			targ_info->user.offset = cts->sync_offset;
		}
		if ((update_type & AMD_TRANS_GOAL) != 0) {
			targ_info->goal.period = cts->sync_period;
			targ_info->goal.offset = cts->sync_offset;
		}
		splx(intflag);
		pccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(pccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		int     extended;

		extended = (amd->eepromBuf[EE_MODE2] & GREATER_1G) != 0;
		cam_calc_geometry(&pccb->ccg, extended);
		xpt_done(pccb);
		break;
	}
	default:
		pccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	}
}

static void
amd_poll(struct cam_sim * psim)
{
	amd_intr(cam_sim_softc(psim));
}

static u_int8_t * 
phystovirt(struct amd_srb * pSRB, u_int32_t xferCnt)
{
	intptr_t   dataPtr;
	struct ccb_scsiio *pcsio;
	u_int8_t   i;
	struct amd_sg *    pseg;

	dataPtr = 0;
	pcsio = &pSRB->pccb->csio;

	dataPtr = (intptr_t) pcsio->data_ptr;
	pseg = pSRB->SGsegment;
	for (i = 0; i < pSRB->SGIndex; i++) {
		dataPtr += (int) pseg->SGXLen;
		pseg++;
	}
	dataPtr += (int) xferCnt;
	return ((u_int8_t *) dataPtr);
}

static void
amd_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *baddr;

	baddr = (bus_addr_t *)arg;
	*baddr = segs->ds_addr;
}

static void
ResetDevParam(struct amd_softc * amd)
{
	u_int target;

	for (target = 0; target <= amd->max_id; target++) {
		if (amd->AdaptSCSIID != target) {
			amdsetsync(amd, target, /*clockrate*/0,
				   /*period*/0, /*offset*/0, AMD_TRANS_CUR);
		}
	}
}

static void
amdcompletematch(struct amd_softc *amd, target_id_t target, lun_id_t lun,
		 u_int tag, struct srb_queue *queue, cam_status status)
{
	struct amd_srb *srb;
	struct amd_srb *next_srb;

	for (srb = TAILQ_FIRST(queue); srb != NULL; srb = next_srb) {
		union ccb *ccb;

		next_srb = TAILQ_NEXT(srb, links);
		if (srb->pccb->ccb_h.target_id != target
		 && target != CAM_TARGET_WILDCARD)
			continue;

		if (srb->pccb->ccb_h.target_lun != lun
		 && lun != CAM_LUN_WILDCARD)
			continue;

		if (srb->TagNumber != tag
		 && tag != AMD_TAG_WILDCARD)
			continue;
		
		ccb = srb->pccb;
		TAILQ_REMOVE(queue, srb, links);
		TAILQ_INSERT_HEAD(&amd->free_srbs, srb, links);
		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) == 0
		 && (status & CAM_DEV_QFRZN) != 0)
			xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status = status;
		xpt_done(ccb);
	}

}

static void
amdsetsync(struct amd_softc *amd, u_int target, u_int clockrate,
	   u_int period, u_int offset, u_int type)
{
	struct amd_target_info *tinfo;
	u_int old_period;
	u_int old_offset;

	tinfo = &amd->tinfo[target];
	old_period = tinfo->current.period;
	old_offset = tinfo->current.offset;
	if ((type & AMD_TRANS_CUR) != 0
	 && (old_period != period || old_offset != offset)) {
		struct cam_path *path;

		tinfo->current.period = period;
		tinfo->current.offset = offset;
		tinfo->sync_period_reg = clockrate;
		tinfo->sync_offset_reg = offset;
		tinfo->CtrlR3 &= ~FAST_SCSI;
		tinfo->CtrlR4 &= ~EATER_25NS;
		if (clockrate > 7)
			tinfo->CtrlR4 |= EATER_25NS;
		else
			tinfo->CtrlR3 |= FAST_SCSI;

		if ((type & AMD_TRANS_ACTIVE) == AMD_TRANS_ACTIVE) {
			amd_write8(amd, SYNCPERIOREG, tinfo->sync_period_reg);
			amd_write8(amd, SYNCOFFREG, tinfo->sync_offset_reg);
			amd_write8(amd, CNTLREG3, tinfo->CtrlR3);
			amd_write8(amd, CNTLREG4, tinfo->CtrlR4);
		}
		/* If possible, update the XPT's notion of our transfer rate */
		if (xpt_create_path(&path, /*periph*/NULL,
				    cam_sim_path(amd->psim), target,
				    CAM_LUN_WILDCARD) == CAM_REQ_CMP) {
			struct ccb_trans_settings neg;

			xpt_setup_ccb(&neg.ccb_h, path, /*priority*/1);
			neg.sync_period = period;
			neg.sync_offset = offset;
			neg.valid = CCB_TRANS_SYNC_RATE_VALID
				  | CCB_TRANS_SYNC_OFFSET_VALID;
			xpt_async(AC_TRANSFER_NEG, path, &neg);
			xpt_free_path(path);	
		}
	}
	if ((type & AMD_TRANS_GOAL) != 0) {
		tinfo->goal.period = period;
		tinfo->goal.offset = offset;
	}

	if ((type & AMD_TRANS_USER) != 0) {
		tinfo->user.period = period;
		tinfo->user.offset = offset;
	}
}

static void
amdsettags(struct amd_softc *amd, u_int target, int tagenb)
{
	panic("Implement me!\n");
}


#if 0
/*
 **********************************************************************
 * Function : amd_reset (struct amd_softc * amd)
 * Purpose  : perform a hard reset on the SCSI bus( and AMD chip).
 * Inputs   : cmd - command which caused the SCSI RESET
 **********************************************************************
 */
static void
amd_reset(struct amd_softc * amd)
{
	int	   intflag;
	u_int8_t   bval;
	u_int16_t  i;


#ifdef AMD_DEBUG0
	printf("DC390: RESET");
#endif

	intflag = splcam();
	bval = amd_read8(amd, CNTLREG1);
	bval |= DIS_INT_ON_SCSI_RST;
	amd_write8(amd, CNTLREG1, bval);	/* disable interrupt */
	amd_ResetSCSIBus(amd);

	for (i = 0; i < 500; i++) {
		DELAY(1000);
	}

	bval = amd_read8(amd, CNTLREG1);
	bval &= ~DIS_INT_ON_SCSI_RST;
	amd_write8(amd, CNTLREG1, bval);	/* re-enable interrupt */

	amd_write8(amd, DMA_Cmd, DMA_IDLE_CMD);
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);

	ResetDevParam(amd);
	amdcompletematch(amd, CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD,
			 AMD_TAG_WILDCARD, &amd->running_srbs,
			 CAM_DEV_QFRZN|CAM_SCSI_BUS_RESET);
	amdcompletematch(amd, CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD,
			 AMD_TAG_WILDCARD, &amd->waiting_srbs,
			 CAM_DEV_QFRZN|CAM_SCSI_BUS_RESET);
	amd->active_srb = NULL;
	amd->ACBFlag = 0;
	splx(intflag);
	return;
}

void
amd_timeout(void *arg1)
{
	struct amd_srb *    pSRB;

	pSRB = (struct amd_srb *) arg1;
}
#endif

static int
amdstart(struct amd_softc *amd, struct amd_srb *pSRB)
{
	union ccb *pccb;
	struct ccb_scsiio *pcsio;
	struct amd_target_info *targ_info;
	u_int identify_msg;
	u_int command;
	u_int target;
	u_int lun;

	pccb = pSRB->pccb;
	pcsio = &pccb->csio;
	target = pccb->ccb_h.target_id;
	lun = pccb->ccb_h.target_lun;
	targ_info = &amd->tinfo[target];

	amd_clear_msg_state(amd);
	amd_write8(amd, SCSIDESTIDREG, target);
	amd_write8(amd, SYNCPERIOREG, targ_info->sync_period_reg);
	amd_write8(amd, SYNCOFFREG, targ_info->sync_offset_reg);
	amd_write8(amd, CNTLREG1, targ_info->CtrlR1);
	amd_write8(amd, CNTLREG3, targ_info->CtrlR3);
	amd_write8(amd, CNTLREG4, targ_info->CtrlR4);
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);

	identify_msg = MSG_IDENTIFYFLAG | lun;
	if ((targ_info->disc_tag & AMD_CUR_DISCENB) != 0
	  && (pccb->ccb_h.flags & CAM_DIS_DISCONNECT) == 0
	  && (pSRB->CmdBlock[0] != REQUEST_SENSE)
	  && (pSRB->SRBFlag & AUTO_REQSENSE) == 0)
		identify_msg |= MSG_IDENTIFY_DISCFLAG;

	amd_write8(amd, SCSIFIFOREG, identify_msg);
	if ((targ_info->disc_tag & AMD_CUR_TAGENB) == 0
	  || (identify_msg & MSG_IDENTIFY_DISCFLAG) == 0)
		pccb->ccb_h.flags &= ~CAM_TAG_ACTION_VALID;
	if (targ_info->current.period != targ_info->goal.period
	 || targ_info->current.offset != targ_info->goal.offset) {
		command = SEL_W_ATN_STOP;
		amdconstructsdtr(amd, targ_info->goal.period,
				 targ_info->goal.offset);
	} else if ((pccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0) {
		command = SEL_W_ATN2;
		pSRB->SRBState = SRB_START;
		amd_write8(amd, SCSIFIFOREG, pcsio->tag_action);
		amd_write8(amd, SCSIFIFOREG, pSRB->TagNumber);
	} else {
		command = SEL_W_ATN;
		pSRB->SRBState = SRB_START;
	}
	if (command != SEL_W_ATN_STOP)
		amdsetupcommand(amd, pSRB);

	if (amd_read8(amd, SCSISTATREG) & INTERRUPT) {
		pSRB->SRBState = SRB_READY;
		return (1);
	} else {
		amd->last_phase = SCSI_ARBITRATING;
		amd_write8(amd, SCSICMDREG, command);
		amd->active_srb = pSRB;
		amd->cur_target = target;
		amd->cur_lun = lun;
		return (0);
	}
}

/*
 *  Catch an interrupt from the adapter.
 *  Process pending device interrupts.
 */
static void 
amd_intr(void   *arg)
{
	struct amd_softc *amd;
	struct amd_srb *pSRB;
	u_int  internstat = 0;
	u_int  scsistat;
	u_int  intstat;

	amd = (struct amd_softc *)arg;

	if (amd == NULL) {
#ifdef AMD_DEBUG0
		printf("amd_intr: amd NULL return......");
#endif
		return;
	}

	scsistat = amd_read8(amd, SCSISTATREG);
	if (!(scsistat & INTERRUPT)) {
#ifdef AMD_DEBUG0
		printf("amd_intr: scsistat = NULL ,return......");
#endif
		return;
	}
#ifdef AMD_DEBUG_SCSI_PHASE
	printf("scsistat=%2x,", scsistat);
#endif

	internstat = amd_read8(amd, INTERNSTATREG);
	intstat = amd_read8(amd, INTSTATREG);

#ifdef AMD_DEBUG_SCSI_PHASE
	printf("intstat=%2x,", intstat);
#endif

	if (intstat & DISCONNECTED) {
		amd_Disconnect(amd);
		return;
	}
	if (intstat & RESELECTED) {
		amd_Reselect(amd);
		return;
	}
	if (intstat & INVALID_CMD) {
		amd_InvalidCmd(amd);
		return;
	}
	if (intstat & SCSI_RESET_) {
		amd_ScsiRstDetect(amd);
		return;
	}
	if (intstat & (SUCCESSFUL_OP + SERVICE_REQUEST)) {
		pSRB = amd->active_srb;
		/*
		 * Run our state engine.  First perform
		 * post processing for the last phase we
		 * were in, followed by any processing
		 * required to handle the current phase.
		 */
		scsistat =
		    amd_SCSI_phase0[amd->last_phase](amd, pSRB, scsistat);
		amd->last_phase = scsistat & SCSI_PHASE_MASK;
		(void)amd_SCSI_phase1[amd->last_phase](amd, pSRB, scsistat);
	}
}

static u_int
amd_DataOutPhase0(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	struct amd_sg *psgl;
	u_int32_t   ResidCnt, xferCnt;

	if (!(pSRB->SRBState & SRB_XFERPAD)) {
		if (scsistat & PARITY_ERR) {
			pSRB->SRBStatus |= PARITY_ERROR;
		}
		if (scsistat & COUNT_2_ZERO) {
			while ((amd_read8(amd, DMA_Status)&DMA_XFER_DONE) == 0)
				;
			pSRB->TotalXferredLen += pSRB->SGToBeXferLen;
			pSRB->SGIndex++;
			if (pSRB->SGIndex < pSRB->SGcount) {
				pSRB->pSGlist++;
				psgl = pSRB->pSGlist;
				pSRB->SGPhysAddr = psgl->SGXPtr;
				pSRB->SGToBeXferLen = psgl->SGXLen;
			} else {
				pSRB->SGToBeXferLen = 0;
			}
		} else {
			ResidCnt = amd_read8(amd, CURRENTFIFOREG) & 0x1f;
			ResidCnt += amd_read8(amd, CTCREG_LOW)
				  | (amd_read8(amd, CTCREG_MID) << 8)
				  | (amd_read8(amd, CURTXTCNTREG) << 16);

			xferCnt = pSRB->SGToBeXferLen - ResidCnt;
			pSRB->SGPhysAddr += xferCnt;
			pSRB->TotalXferredLen += xferCnt;
			pSRB->SGToBeXferLen = ResidCnt;
		}
	}
	amd_write8(amd, DMA_Cmd, WRITE_DIRECTION | DMA_IDLE_CMD);
	return (scsistat);
}

static u_int
amd_DataInPhase0(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	u_int8_t bval;
	u_int16_t  i, residual;
	struct amd_sg *psgl;
	u_int32_t   ResidCnt, xferCnt;
	u_int8_t *  ptr;

	if (!(pSRB->SRBState & SRB_XFERPAD)) {
		if (scsistat & PARITY_ERR) {
			pSRB->SRBStatus |= PARITY_ERROR;
		}
		if (scsistat & COUNT_2_ZERO) {
			while (1) {
				bval = amd_read8(amd, DMA_Status);
				if ((bval & DMA_XFER_DONE) != 0)
					break;
			}
			amd_write8(amd, DMA_Cmd, READ_DIRECTION|DMA_IDLE_CMD);

			pSRB->TotalXferredLen += pSRB->SGToBeXferLen;
			pSRB->SGIndex++;
			if (pSRB->SGIndex < pSRB->SGcount) {
				pSRB->pSGlist++;
				psgl = pSRB->pSGlist;
				pSRB->SGPhysAddr = psgl->SGXPtr;
				pSRB->SGToBeXferLen = psgl->SGXLen;
			} else {
				pSRB->SGToBeXferLen = 0;
			}
		} else {	/* phase changed */
			residual = 0;
			bval = amd_read8(amd, CURRENTFIFOREG);
			while (bval & 0x1f) {
				if ((bval & 0x1f) == 1) {
					for (i = 0; i < 0x100; i++) {
						bval = amd_read8(amd, CURRENTFIFOREG);
						if (!(bval & 0x1f)) {
							goto din_1;
						} else if (i == 0x0ff) {
							residual = 1;
							goto din_1;
						}
					}
				} else {
					bval = amd_read8(amd, CURRENTFIFOREG);
				}
			}
	din_1:
			amd_write8(amd, DMA_Cmd, READ_DIRECTION|DMA_BLAST_CMD);
			for (i = 0; i < 0x8000; i++) {
				if ((amd_read8(amd, DMA_Status)&BLAST_COMPLETE))
					break;
			}
			amd_write8(amd, DMA_Cmd, READ_DIRECTION|DMA_IDLE_CMD);

			ResidCnt = amd_read8(amd, CTCREG_LOW)
				 | (amd_read8(amd, CTCREG_MID) << 8)
				 | (amd_read8(amd, CURTXTCNTREG) << 16);
			xferCnt = pSRB->SGToBeXferLen - ResidCnt;
			pSRB->SGPhysAddr += xferCnt;
			pSRB->TotalXferredLen += xferCnt;
			pSRB->SGToBeXferLen = ResidCnt;
			if (residual) {
				/* get residual byte */	
				bval = amd_read8(amd, SCSIFIFOREG);
				ptr = phystovirt(pSRB, xferCnt);
				*ptr = bval;
				pSRB->SGPhysAddr++;
				pSRB->TotalXferredLen++;
				pSRB->SGToBeXferLen--;
			}
		}
	}
	return (scsistat);
}

static u_int
amd_StatusPhase0(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	pSRB->TargetStatus = amd_read8(amd, SCSIFIFOREG);
	/* get message */
	pSRB->EndMessage = amd_read8(amd, SCSIFIFOREG);
	pSRB->SRBState = SRB_COMPLETED;
	amd_write8(amd, SCSICMDREG, MSG_ACCEPTED_CMD);
	return (SCSI_NOP0);
}

static u_int
amd_MsgOutPhase0(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	if (pSRB->SRBState & (SRB_UNEXPECT_RESEL + SRB_ABORT_SENT)) {
		scsistat = SCSI_NOP0;
	}
	return (scsistat);
}

static u_int
amd_MsgInPhase0(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	int done;
	
	amd->msgin_buf[amd->msgin_index]  = amd_read8(amd, SCSIFIFOREG);

	done = amdparsemsg(amd);
	if (done)
		amd->msgin_index = 0;
	else 
		amd->msgin_index++;
	return (SCSI_NOP0);
}

static int
amdparsemsg(struct amd_softc *amd)
{
	int	reject;
	int	done;
	int	response;

	done = FALSE;
	response = FALSE;
	reject = FALSE;

	/*
	 * Parse as much of the message as is availible,
	 * rejecting it if we don't support it.  When
	 * the entire message is availible and has been
	 * handled, return TRUE indicating that we have
	 * parsed an entire message.
	 */
	switch (amd->msgin_buf[0]) {
	case MSG_DISCONNECT:
		amd->active_srb->SRBState = SRB_DISCONNECT;
		amd->disc_count[amd->cur_target][amd->cur_lun]++;
		done = TRUE;
		break;
	case MSG_SIMPLE_Q_TAG:
	{
		struct amd_srb *disc_srb;

		if (amd->msgin_index < 1)
			break;		
		disc_srb = &amd->SRB_array[amd->msgin_buf[1]];
		if (amd->active_srb != NULL
		 || disc_srb->SRBState != SRB_DISCONNECT
		 || disc_srb->pccb->ccb_h.target_id != amd->cur_target
		 || disc_srb->pccb->ccb_h.target_lun != amd->cur_lun) {
			printf("amd%d: Unexpected tagged reselection "
			       "for target %d, Issuing Abort\n", amd->unit,
			       amd->cur_target);
			amd->msgout_buf[0] = MSG_ABORT;
			amd->msgout_len = 1;
			response = TRUE;
			break;
		}
		amd->active_srb = disc_srb;
		amd->disc_count[amd->cur_target][amd->cur_lun]--;
		done = TRUE;
		break;
	}
	case MSG_MESSAGE_REJECT:
		response = amdhandlemsgreject(amd);
		if (response == FALSE)
			amd_write8(amd, SCSICMDREG, RESET_ATN_CMD);
		/* FALLTHROUGH */
	case MSG_NOOP:
		done = TRUE;
		break;
	case MSG_EXTENDED:
	{
		u_int clockrate;
		u_int period;
		u_int offset;
		u_int saved_offset;

		/* Wait for enough of the message to begin validation */
		if (amd->msgin_index < 1)
			break;
		if (amd->msgin_buf[1] != MSG_EXT_SDTR_LEN) {
			reject = TRUE;
			break;
		}

		/* Wait for opcode */
		if (amd->msgin_index < 2)
			break;

		if (amd->msgin_buf[2] != MSG_EXT_SDTR) {
			reject = TRUE;
			break;
		}

		/*
		 * Wait until we have both args before validating
		 * and acting on this message.
		 *
		 * Add one to MSG_EXT_SDTR_LEN to account for
		 * the extended message preamble.
		 */
		if (amd->msgin_index < (MSG_EXT_SDTR_LEN + 1))
			break;

		period = amd->msgin_buf[3];
		saved_offset = offset = amd->msgin_buf[4];
		clockrate = amdfindclockrate(amd, &period);
		if (offset > AMD_MAX_SYNC_OFFSET)
			offset = AMD_MAX_SYNC_OFFSET;
		if (period == 0 || offset == 0) {
			offset = 0;
			period = 0;
			clockrate = 0;
		}
		amdsetsync(amd, amd->cur_target, clockrate, period, offset,
			   AMD_TRANS_ACTIVE|AMD_TRANS_GOAL);

		/*
		 * See if we initiated Sync Negotiation
		 * and didn't have to fall down to async
		 * transfers.
		 */
		if (amdsentmsg(amd, MSG_EXT_SDTR, /*full*/TRUE)) {
			/* We started it */
			if (saved_offset != offset) {
				/* Went too low - force async */
				reject = TRUE;
			}
		} else {
			/*
			 * Send our own SDTR in reply
			 */
			if (bootverbose)
				printf("Sending SDTR!\n");
			amd->msgout_index = 0;
			amd->msgout_len = 0;
			amdconstructsdtr(amd, period, offset);
			amd->msgout_index = 0;
			response = TRUE;
		}
		done = TRUE;
		break;
	}
	case MSG_SAVEDATAPOINTER:
	case MSG_RESTOREPOINTERS:
		/* XXX Implement!!! */
		done = TRUE;
		break;
	default:
		reject = TRUE;
		break;
	}

	if (reject) {
		amd->msgout_index = 0;
		amd->msgout_len = 1;
		amd->msgout_buf[0] = MSG_MESSAGE_REJECT;
		done = TRUE;
		response = TRUE;
	}

	if (response)
		amd_write8(amd, SCSICMDREG, SET_ATN_CMD);

	if (done && !response)
		/* Clear the outgoing message buffer */
		amd->msgout_len = 0;

	/* Drop Ack */
	amd_write8(amd, SCSICMDREG, MSG_ACCEPTED_CMD);

	return (done);
}

static u_int
amdfindclockrate(struct amd_softc *amd, u_int *period)
{
	u_int i;
	u_int clockrate;

	for (i = 0; i < sizeof(tinfo_sync_period); i++) {
		u_int8_t *table_entry;

		table_entry = &tinfo_sync_period[i];
		if (*period <= *table_entry) {
			/*
			 * When responding to a target that requests
			 * sync, the requested rate may fall between
			 * two rates that we can output, but still be
			 * a rate that we can receive.  Because of this,
			 * we want to respond to the target with
			 * the same rate that it sent to us even
			 * if the period we use to send data to it
			 * is lower.  Only lower the response period
			 * if we must.
			 */ 
			if (i == 0) {
				*period = *table_entry;
			}
			break;
		}
	}

	if (i == sizeof(tinfo_sync_period)) {
		/* Too slow for us.  Use asnyc transfers. */
		*period = 0;
		clockrate = 0;
	} else
		clockrate = i + 4;

	return (clockrate);
}

/*
 * See if we sent a particular extended message to the target.
 * If "full" is true, the target saw the full message.
 * If "full" is false, the target saw at least the first
 * byte of the message.
 */
static int
amdsentmsg(struct amd_softc *amd, u_int msgtype, int full)
{
	int found;
	int index;

	found = FALSE;
	index = 0;

	while (index < amd->msgout_len) {
		if ((amd->msgout_buf[index] & MSG_IDENTIFYFLAG) != 0
		 || amd->msgout_buf[index] == MSG_MESSAGE_REJECT)
			index++;
		else if (amd->msgout_buf[index] >= MSG_SIMPLE_Q_TAG
		      && amd->msgout_buf[index] < MSG_IGN_WIDE_RESIDUE) {
			/* Skip tag type and tag id */
			index += 2;
		} else if (amd->msgout_buf[index] == MSG_EXTENDED) {
			/* Found a candidate */
			if (amd->msgout_buf[index+2] == msgtype) {
				u_int end_index;

				end_index = index + 1
					  + amd->msgout_buf[index + 1];
				if (full) {
					if (amd->msgout_index > end_index)
						found = TRUE;
				} else if (amd->msgout_index > index)
					found = TRUE;
			}
			break;
		} else {
			panic("amdsentmsg: Inconsistent msg buffer");
		}
	}
	return (found);
}

static void
amdconstructsdtr(struct amd_softc *amd, u_int period, u_int offset)
{
	amd->msgout_buf[amd->msgout_index++] = MSG_EXTENDED;
	amd->msgout_buf[amd->msgout_index++] = MSG_EXT_SDTR_LEN;
	amd->msgout_buf[amd->msgout_index++] = MSG_EXT_SDTR;
	amd->msgout_buf[amd->msgout_index++] = period;
	amd->msgout_buf[amd->msgout_index++] = offset;
	amd->msgout_len += 5;
}

static int
amdhandlemsgreject(struct amd_softc *amd)
{
	/*
	 * If we had an outstanding SDTR for this
	 * target, this is a signal that the target
	 * is refusing negotiation.  Also watch out
	 * for rejected tag messages.
	 */
	struct	amd_srb *srb;
	struct	amd_target_info *targ_info;
	int	response = FALSE;

	srb = amd->active_srb;
	targ_info = &amd->tinfo[amd->cur_target];
	if (amdsentmsg(amd, MSG_EXT_SDTR, /*full*/FALSE)) {
		/* note asynch xfers and clear flag */
		amdsetsync(amd, amd->cur_target, /*clockrate*/0,
			   /*period*/0, /*offset*/0,
			   AMD_TRANS_ACTIVE|AMD_TRANS_GOAL);
		printf("amd%d:%d: refuses synchronous negotiation. "
		       "Using asynchronous transfers\n",
		       amd->unit, amd->cur_target);
	} else if ((srb != NULL)
		&& (srb->pccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0) {
		struct  ccb_trans_settings neg;

		printf("amd%d:%d: refuses tagged commands.  Performing "
		       "non-tagged I/O\n", amd->unit, amd->cur_target);

		amdsettags(amd, amd->cur_target, FALSE);
		neg.flags = 0;
		neg.valid = CCB_TRANS_TQ_VALID;
		xpt_setup_ccb(&neg.ccb_h, srb->pccb->ccb_h.path, /*priority*/1);
		xpt_async(AC_TRANSFER_NEG, srb->pccb->ccb_h.path, &neg);

		/*
		 * Resend the identify for this CCB as the target
		 * may believe that the selection is invalid otherwise.
		 */
		if (amd->msgout_len != 0)
			bcopy(&amd->msgout_buf[0], &amd->msgout_buf[1],
			      amd->msgout_len);
		amd->msgout_buf[0] = MSG_IDENTIFYFLAG
				    | srb->pccb->ccb_h.target_lun;
		amd->msgout_len++;
		if ((targ_info->disc_tag & AMD_CUR_DISCENB) != 0
		  && (srb->pccb->ccb_h.flags & CAM_DIS_DISCONNECT) == 0)
			amd->msgout_buf[0] |= MSG_IDENTIFY_DISCFLAG;

		srb->pccb->ccb_h.flags &= ~CAM_TAG_ACTION_VALID;

		/*
		 * Requeue all tagged commands for this target
		 * currently in our posession so they can be
		 * converted to untagged commands.
		 */
		amdcompletematch(amd, amd->cur_target, amd->cur_lun,
				 AMD_TAG_WILDCARD, &amd->waiting_srbs,
				 CAM_DEV_QFRZN|CAM_REQUEUE_REQ);
	} else {
		/*
		 * Otherwise, we ignore it.
		 */
		printf("amd%d:%d: Message reject received -- ignored\n",
		       amd->unit, amd->cur_target);
	}
	return (response);
}

#if 0
	if (!(pSRB->SRBState & SRB_MSGIN_MULTI)) {
		if (bval == MSG_DISCONNECT) {
			pSRB->SRBState = SRB_DISCONNECT;
		} else if (bval == MSG_SAVEDATAPOINTER) {
			goto min6;
		} else if ((bval == MSG_EXTENDED)
			|| ((bval >= MSG_SIMPLE_Q_TAG)
			 && (bval <= MSG_ORDERED_Q_TAG))) {
			pSRB->SRBState |= SRB_MSGIN_MULTI;
			pSRB->MsgInBuf[0] = bval;
			pSRB->MsgCnt = 1;
			pSRB->pMsgPtr = &pSRB->MsgInBuf[1];
		} else if (bval == MSG_MESSAGE_REJECT) {
			amd_write8(amd, SCSICMDREG, RESET_ATN_CMD);

			if (pSRB->SRBState & DO_SYNC_NEGO) {
				goto set_async;
			}
		} else if (bval == MSG_RESTOREPOINTERS) {
			goto min6;
		} else {
			goto min6;
		}
	} else {		/* minx: */
		*pSRB->pMsgPtr = bval;
		pSRB->MsgCnt++;
		pSRB->pMsgPtr++;
		if ((pSRB->MsgInBuf[0] >= MSG_SIMPLE_Q_TAG)
		 && (pSRB->MsgInBuf[0] <= MSG_ORDERED_Q_TAG)) {
			if (pSRB->MsgCnt == 2) {
				pSRB->SRBState = 0;
				pSRB = &amd->SRB_array[pSRB->MsgInBuf[1]];
				if (pSRB->SRBState & SRB_DISCONNECT) == 0) {
					pSRB = amd->pTmpSRB;
					pSRB->SRBState = SRB_UNEXPECT_RESEL;
					pDCB->pActiveSRB = pSRB;
					pSRB->MsgOutBuf[0] = MSG_ABORT_TAG;
					EnableMsgOut2(amd, pSRB);
				} else {
					if (pDCB->DCBFlag & ABORT_DEV_) {
						pSRB->SRBState = SRB_ABORT_SENT;
						EnableMsgOut1(amd, pSRB);
					}
					pDCB->pActiveSRB = pSRB;
					pSRB->SRBState = SRB_DATA_XFER;
				}
			}
		} else if ((pSRB->MsgInBuf[0] == MSG_EXTENDED)
			&& (pSRB->MsgCnt == 5)) {
			pSRB->SRBState &= ~(SRB_MSGIN_MULTI + DO_SYNC_NEGO);
			if ((pSRB->MsgInBuf[1] != 3)
			 || (pSRB->MsgInBuf[2] != 1)) {	/* reject_msg: */
				pSRB->MsgCnt = 1;
				pSRB->MsgInBuf[0] = MSG_MESSAGE_REJECT;
				amd_write8(amd, SCSICMDREG, SET_ATN_CMD);
			} else if (!(pSRB->MsgInBuf[3])
				|| !(pSRB->MsgInBuf[4])) {
		set_async:	/* set async */

				pDCB = pSRB->pSRBDCB;
				/* disable sync & sync nego */
				pDCB->SyncMode &= ~(SYNC_ENABLE|SYNC_NEGO_DONE);
				pDCB->SyncPeriod = 0;
				pDCB->SyncOffset = 0;

				pDCB->tinfo.goal.period = 0;
				pDCB->tinfo.goal.offset = 0;

				pDCB->tinfo.current.period = 0;
				pDCB->tinfo.current.offset = 0;
				pDCB->tinfo.current.width =
				    MSG_EXT_WDTR_BUS_8_BIT;

				pDCB->CtrlR3 = FAST_CLK; /* non_fast */
				pDCB->CtrlR4 &= 0x3f;
				pDCB->CtrlR4 |= EATER_25NS; 
				goto re_prog;
			} else {/* set sync */

				pDCB = pSRB->pSRBDCB;
				/* enable sync & sync nego */
				pDCB->SyncMode |= SYNC_ENABLE|SYNC_NEGO_DONE;

				/* set sync offset */
				pDCB->SyncOffset &= 0x0f0;
				pDCB->SyncOffset |= pSRB->MsgInBuf[4];

				/* set sync period */
				pDCB->MaxNegoPeriod = pSRB->MsgInBuf[3];

				wval = (u_int16_t) pSRB->MsgInBuf[3];
				wval = wval << 2;
				wval--;
				wval1 = wval / 25;
				if ((wval1 * 25) != wval) {
					wval1++;
				}
				bval = FAST_CLK|FAST_SCSI;
				pDCB->CtrlR4 &= 0x3f;
				if (wval1 >= 8) {
					/* Fast SCSI */
					wval1--;
					bval = FAST_CLK;
					pDCB->CtrlR4 |= EATER_25NS;
				}
				pDCB->CtrlR3 = bval;
				pDCB->SyncPeriod = (u_int8_t) wval1;

				pDCB->tinfo.goal.period =
				    tinfo_sync_period[pDCB->SyncPeriod - 4];
				pDCB->tinfo.goal.offset = pDCB->SyncOffset;
				pDCB->tinfo.current.period =
				    tinfo_sync_period[pDCB->SyncPeriod - 4];;
				pDCB->tinfo.current.offset = pDCB->SyncOffset;

				/*
				 * program SCSI control register
				 */
		re_prog:
				amd_write8(amd, SYNCPERIOREG, pDCB->SyncPeriod);
				amd_write8(amd, SYNCOFFREG, pDCB->SyncOffset);
				amd_write8(amd, CNTLREG3, pDCB->CtrlR3);
				amd_write8(amd, CNTLREG4, pDCB->CtrlR4);
			}
		}
	}
min6:
	amd_write8(amd, SCSICMDREG, MSG_ACCEPTED_CMD);
	return (SCSI_NOP0);
}
#endif

static u_int
amd_DataOutPhase1(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	DataIO_Comm(amd, pSRB, WRITE_DIRECTION);
	return (scsistat);
}

static u_int 
amd_DataInPhase1(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	DataIO_Comm(amd, pSRB, READ_DIRECTION);
	return (scsistat);
}

static void
DataIO_Comm(struct amd_softc *amd, struct amd_srb *pSRB, u_int ioDir)
{
	struct amd_sg *    psgl;
	u_int32_t   lval;

	if (pSRB->SGIndex < pSRB->SGcount) {
		amd_write8(amd, DMA_Cmd, DMA_IDLE_CMD|ioDir);/* |EN_DMA_INT */

		if (!pSRB->SGToBeXferLen) {
			psgl = pSRB->pSGlist;
			pSRB->SGPhysAddr = psgl->SGXPtr;
			pSRB->SGToBeXferLen = psgl->SGXLen;
		}
		lval = pSRB->SGToBeXferLen;
		amd_write8(amd, CTCREG_LOW, lval);
		amd_write8(amd, CTCREG_MID, lval >> 8);
		amd_write8(amd, CURTXTCNTREG, lval >> 16);

		amd_write32(amd, DMA_XferCnt, pSRB->SGToBeXferLen);

		amd_write32(amd, DMA_XferAddr, pSRB->SGPhysAddr);

		pSRB->SRBState = SRB_DATA_XFER;

		amd_write8(amd, SCSICMDREG, DMA_COMMAND|INFO_XFER_CMD);

		amd_write8(amd, DMA_Cmd, DMA_IDLE_CMD|ioDir); /* |EN_DMA_INT */

		amd_write8(amd, DMA_Cmd, DMA_START_CMD|ioDir);/* |EN_DMA_INT */
	} else {		/* xfer pad */
		if (pSRB->SGcount) {
			pSRB->AdaptStatus = H_OVER_UNDER_RUN;
			pSRB->SRBStatus |= OVER_RUN;
		}
		amd_write8(amd, CTCREG_LOW, 0);
		amd_write8(amd, CTCREG_MID, 0);
		amd_write8(amd, CURTXTCNTREG, 0);

		pSRB->SRBState |= SRB_XFERPAD;
		amd_write8(amd, SCSICMDREG, DMA_COMMAND|XFER_PAD_BYTE);
	}
}

static u_int
amd_CommandPhase1(struct amd_softc *amd, struct amd_srb *srb, u_int scsistat)
{
	amd_write8(amd, SCSICMDREG, RESET_ATN_CMD);
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);

	amdsetupcommand(amd, srb);

	srb->SRBState = SRB_COMMAND;
	amd_write8(amd, SCSICMDREG, INFO_XFER_CMD);
	return (scsistat);
}

static u_int
amd_StatusPhase1(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);
	pSRB->SRBState = SRB_STATUS;
	amd_write8(amd, SCSICMDREG, INITIATOR_CMD_CMPLTE);
	return (scsistat);
}

static u_int
amd_MsgOutPhase1(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);

	if (amd->msgout_len == 0) {
		amd->msgout_buf[0] = MSG_NOOP;
		amd->msgout_len = 1;
	}
	amd_write8_multi(amd, SCSIFIFOREG, amd->msgout_buf, amd->msgout_len);
	amd_write8(amd, SCSICMDREG, INFO_XFER_CMD);
	return (scsistat);
}

static u_int
amd_MsgInPhase1(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);
	amd_write8(amd, SCSICMDREG, INFO_XFER_CMD);
	return (scsistat);
}

static u_int
amd_NopPhase(struct amd_softc *amd, struct amd_srb *pSRB, u_int scsistat)
{
	return (scsistat);
}

static void
amd_Disconnect(struct amd_softc * amd)
{
	struct	amd_srb *srb;
	int	target;
	int	lun;

	srb = amd->active_srb;
	amd->active_srb = NULL;
	amd->last_phase = SCSI_BUS_FREE;
	amd_write8(amd, SCSICMDREG, EN_SEL_RESEL);
	target = amd->cur_target;
	lun = amd->cur_lun;

	if (srb == NULL) {
		/* Invalid reselection */
		amdrunwaiting(amd);
	} else if (srb->SRBState & SRB_ABORT_SENT) {
		/* Clean up and done this srb */
#if 0
		while (( = TAILQ_FIRST(&amd->running_srbs)) != NULL) {
			/* XXX What about "done'ing" these srbs??? */
			if (pSRB->pSRBDCB == pDCB) {
				TAILQ_REMOVE(&amd->running_srbs, pSRB, links);
				TAILQ_INSERT_HEAD(&amd->free_srbs, pSRB, links);
			}
		}
		amdrunwaiting(amd);
#endif
	} else {
		if ((srb->SRBState & (SRB_START | SRB_MSGOUT))
		 || !(srb->SRBState & (SRB_DISCONNECT | SRB_COMPLETED))) {
			srb->TargetStatus = AMD_SCSI_STAT_SEL_TIMEOUT;
			goto disc1;
		} else if (srb->SRBState & SRB_DISCONNECT) {
			if (!(srb->pccb->ccb_h.flags & CAM_TAG_ACTION_VALID))
				amd->untagged_srbs[target][lun] = srb;
			amdrunwaiting(amd);
		} else if (srb->SRBState & SRB_COMPLETED) {
	disc1:
			srb->SRBState = SRB_FREE;
			SRBdone(amd, srb);
		}
	}
	return;
}

static void
amd_Reselect(struct amd_softc *amd)
{
	struct amd_target_info *tinfo;
	u_int16_t disc_count;

	amd_clear_msg_state(amd);
	if (amd->active_srb != NULL) {
		/* Requeue the SRB for our attempted Selection */
		TAILQ_REMOVE(&amd->running_srbs, amd->active_srb, links);
		TAILQ_INSERT_HEAD(&amd->waiting_srbs, amd->active_srb, links);
		amd->active_srb = NULL;
	}
	/* get ID */
	amd->cur_target = amd_read8(amd, SCSIFIFOREG);
	amd->cur_target ^= amd->HostID_Bit;
	amd->cur_target = ffs(amd->cur_target) - 1;
	amd->cur_lun = amd_read8(amd, SCSIFIFOREG) & 7;
	tinfo = &amd->tinfo[amd->cur_target];
	amd->active_srb = amd->untagged_srbs[amd->cur_target][amd->cur_lun];
	disc_count = amd->disc_count[amd->cur_target][amd->cur_lun];
	if (disc_count == 0) {
		printf("amd%d: Unexpected reselection for target %d, "
		       "Issuing Abort\n", amd->unit, amd->cur_target);
		amd->msgout_buf[0] = MSG_ABORT;
		amd->msgout_len = 1;
		amd_write8(amd, SCSICMDREG, SET_ATN_CMD);
	}
	if (amd->active_srb != NULL) {
		amd->disc_count[amd->cur_target][amd->cur_lun]--;
		amd->untagged_srbs[amd->cur_target][amd->cur_lun] = NULL;
	}
	
	amd_write8(amd, SCSIDESTIDREG, amd->cur_target);
	amd_write8(amd, SYNCPERIOREG, tinfo->sync_period_reg);
	amd_write8(amd, SYNCOFFREG, tinfo->sync_offset_reg);
	amd_write8(amd, CNTLREG1, tinfo->CtrlR1);
	amd_write8(amd, CNTLREG3, tinfo->CtrlR3);
	amd_write8(amd, CNTLREG4, tinfo->CtrlR4);
	amd_write8(amd, SCSICMDREG, MSG_ACCEPTED_CMD);/* drop /ACK */
	amd->last_phase = SCSI_NOP0;
}

static void
SRBdone(struct amd_softc *amd, struct amd_srb *pSRB)
{
	u_int8_t   bval, i, status;
	union ccb *pccb;
	struct ccb_scsiio *pcsio;
	int	   intflag;
	struct amd_sg *ptr2;
	u_int32_t   swlval;

	pccb = pSRB->pccb;
	pcsio = &pccb->csio;

	CAM_DEBUG(pccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("SRBdone - TagNumber %d\n", pSRB->TagNumber));

	if ((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(amd->buffer_dmat, pSRB->dmamap, op);
		bus_dmamap_unload(amd->buffer_dmat, pSRB->dmamap);
	}

	status = pSRB->TargetStatus;
	pccb->ccb_h.status = CAM_REQ_CMP;
	pccb->ccb_h.status = CAM_REQ_CMP;
	if (pSRB->SRBFlag & AUTO_REQSENSE) {
		pSRB->SRBFlag &= ~AUTO_REQSENSE;
		pSRB->AdaptStatus = 0;
		pSRB->TargetStatus = SCSI_STATUS_CHECK_COND;

		if (status == SCSI_STATUS_CHECK_COND) {
			pccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
			goto ckc_e;
		}
		*((u_int32_t *)&(pSRB->CmdBlock[0])) = pSRB->Segment0[0];

		pcsio->sense_resid = pcsio->sense_len
				   - pSRB->TotalXferredLen;
		pSRB->TotalXferredLen = pSRB->Segment1[1];
		if (pSRB->TotalXferredLen) {
			/* ???? */
			pcsio->resid = pcsio->dxfer_len
				     - pSRB->TotalXferredLen;
			/* The resid field contains valid data	 */
			/* Flush resid bytes on complete        */
		} else {
			pcsio->scsi_status = SCSI_STATUS_CHECK_COND;
		}
		bzero(&pcsio->sense_data, pcsio->sense_len);
		bcopy(amd_get_sense_buf(amd, pSRB), &pcsio->sense_data,
		      pcsio->sense_len);
		pccb->ccb_h.status = CAM_AUTOSNS_VALID;
		goto ckc_e;
	}
	if (status) {
		if (status == SCSI_STATUS_CHECK_COND) {

			if ((pSRB->SGIndex < pSRB->SGcount)
			 && (pSRB->SGcount) && (pSRB->SGToBeXferLen)) {
				bval = pSRB->SGcount;
				swlval = pSRB->SGToBeXferLen;
				ptr2 = pSRB->pSGlist;
				ptr2++;
				for (i = pSRB->SGIndex + 1; i < bval; i++) {
					swlval += ptr2->SGXLen;
					ptr2++;
				}
				/* ??????? */
				pcsio->resid = (u_int32_t) swlval;

#ifdef	AMD_DEBUG0
				printf("XferredLen=%8x,NotYetXferLen=%8x,",
					pSRB->TotalXferredLen, swlval);
#endif
			}
			if ((pcsio->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0) {
#ifdef	AMD_DEBUG0
				printf("RequestSense..................\n");
#endif
				RequestSense(amd, pSRB);
				return;
			}
			pcsio->scsi_status = SCSI_STATUS_CHECK_COND;
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			goto ckc_e;
		} else if (status == SCSI_STATUS_QUEUE_FULL) {
			pSRB->AdaptStatus = 0;
			pSRB->TargetStatus = 0;
			pcsio->scsi_status = SCSI_STATUS_QUEUE_FULL;
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			goto ckc_e;
		} else if (status == AMD_SCSI_STAT_SEL_TIMEOUT) {
			pSRB->AdaptStatus = H_SEL_TIMEOUT;
			pSRB->TargetStatus = 0;

			pcsio->scsi_status = AMD_SCSI_STAT_SEL_TIMEOUT;
			pccb->ccb_h.status = CAM_SEL_TIMEOUT;
		} else if (status == SCSI_STATUS_BUSY) {
#ifdef AMD_DEBUG0
			printf("DC390: target busy at %s %d\n",
			       __FILE__, __LINE__);
#endif
			pcsio->scsi_status = SCSI_STATUS_BUSY;
			pccb->ccb_h.status = CAM_SCSI_BUSY;
		} else if (status == SCSI_STATUS_RESERV_CONFLICT) {
#ifdef AMD_DEBUG0
			printf("DC390: target reserved at %s %d\n",
			       __FILE__, __LINE__);
#endif
			pcsio->scsi_status = SCSI_STATUS_RESERV_CONFLICT;
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR; /* XXX */
		} else {
			pSRB->AdaptStatus = 0;
#ifdef AMD_DEBUG0
			printf("DC390: driver stuffup at %s %d\n",
			       __FILE__, __LINE__);
#endif
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
		}
	} else {
		status = pSRB->AdaptStatus;
		if (status & H_OVER_UNDER_RUN) {
			pSRB->TargetStatus = 0;

			pccb->ccb_h.status = CAM_DATA_RUN_ERR;	
		} else if (pSRB->SRBStatus & PARITY_ERROR) {
#ifdef AMD_DEBUG0
			printf("DC390: driver stuffup %s %d\n",
			       __FILE__, __LINE__);
#endif
			/* Driver failed to perform operation	  */
			pccb->ccb_h.status = CAM_UNCOR_PARITY;
		} else {	/* No error */
			pSRB->AdaptStatus = 0;
			pSRB->TargetStatus = 0;
			pcsio->resid = 0;
			/* there is no error, (sense is invalid)  */
		}
	}
ckc_e:
	intflag = splcam();
	if ((pccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		/* CAM request not yet complete =>device_Q frozen */
		xpt_freeze_devq(pccb->ccb_h.path, 1);
		pccb->ccb_h.status |= CAM_DEV_QFRZN;
	}
	TAILQ_REMOVE(&amd->running_srbs, pSRB, links);
	TAILQ_INSERT_HEAD(&amd->free_srbs, pSRB, links);
	amdrunwaiting(amd);
	splx(intflag);
	xpt_done(pccb);

}

static void
amd_ResetSCSIBus(struct amd_softc * amd)
{
	int     intflag;

	intflag = splcam();
	amd->ACBFlag |= RESET_DEV;
	amd_write8(amd, DMA_Cmd, DMA_IDLE_CMD);
	amd_write8(amd, SCSICMDREG, RST_SCSI_BUS_CMD);
	splx(intflag);
	return;
}

static void
amd_ScsiRstDetect(struct amd_softc * amd)
{
	int     intflag;
	u_int32_t   wlval;

#ifdef AMD_DEBUG0
	printf("amd_ScsiRstDetect \n");
#endif

	wlval = 1000;
	while (--wlval) {	/* delay 1 sec */
		DELAY(1000);
	}
	intflag = splcam();

	amd_write8(amd, DMA_Cmd, DMA_IDLE_CMD);
	amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);

	if (amd->ACBFlag & RESET_DEV) {
		amd->ACBFlag |= RESET_DONE;
	} else {
		amd->ACBFlag |= RESET_DETECT;
		ResetDevParam(amd);
		amdcompletematch(amd, CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD,
				 AMD_TAG_WILDCARD, &amd->running_srbs,
				 CAM_DEV_QFRZN|CAM_SCSI_BUS_RESET);
		amdcompletematch(amd, CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD,
				 AMD_TAG_WILDCARD, &amd->waiting_srbs,
				 CAM_DEV_QFRZN|CAM_SCSI_BUS_RESET);
		amd->active_srb = NULL;
		amd->ACBFlag = 0;
		amdrunwaiting(amd);
	}
	splx(intflag);
	return;
}

static void
RequestSense(struct amd_softc *amd, struct amd_srb *pSRB)
{
	union ccb *pccb;
	struct ccb_scsiio *pcsio;

	pccb = pSRB->pccb;
	pcsio = &pccb->csio;

	pSRB->SRBFlag |= AUTO_REQSENSE;
	pSRB->Segment0[0] = *((u_int32_t *) & (pSRB->CmdBlock[0]));
	pSRB->Segment0[1] = *((u_int32_t *) & (pSRB->CmdBlock[4]));
	pSRB->Segment1[0] = (pSRB->ScsiCmdLen << 8) + pSRB->SGcount;
	pSRB->Segment1[1] = pSRB->TotalXferredLen;

	pSRB->AdaptStatus = 0;
	pSRB->TargetStatus = 0;

	pSRB->Segmentx.SGXPtr = amd_get_sense_bufaddr(amd, pSRB);
	pSRB->Segmentx.SGXLen = amd_get_sense_bufsize(amd, pSRB);

	pSRB->pSGlist = &pSRB->Segmentx;
	pSRB->SGcount = 1;
	pSRB->SGIndex = 0;

	pSRB->CmdBlock[0] = REQUEST_SENSE;
	pSRB->CmdBlock[1] = pSRB->pccb->ccb_h.target_lun << 5;
	pSRB->CmdBlock[2] = 0;
	pSRB->CmdBlock[3] = 0;
	pSRB->CmdBlock[4] = pcsio->sense_len;
	pSRB->CmdBlock[5] = 0;
	pSRB->ScsiCmdLen = 6;

	pSRB->TotalXferredLen = 0;
	pSRB->SGToBeXferLen = 0;
	if (amdstart(amd, pSRB) != 0) {
		TAILQ_REMOVE(&amd->running_srbs, pSRB, links);
		TAILQ_INSERT_HEAD(&amd->waiting_srbs, pSRB, links);
	}
}

static void
amd_InvalidCmd(struct amd_softc * amd)
{
	struct amd_srb *srb;

	srb = amd->active_srb;
	if (srb->SRBState & (SRB_START|SRB_MSGOUT))
		amd_write8(amd, SCSICMDREG, CLEAR_FIFO_CMD);
}

void 
amd_linkSRB(struct amd_softc *amd)
{
	u_int16_t  count, i;
	struct amd_srb *psrb;

	count = amd->SRBCount;

	for (i = 0; i < count; i++) {
		psrb = (struct amd_srb *)&amd->SRB_array[i];
		psrb->TagNumber = i;
		TAILQ_INSERT_TAIL(&amd->free_srbs, psrb, links);
	}
}

static void
amd_EnDisableCE(struct amd_softc *amd, int mode, int *regval)
{
	if (mode == ENABLE_CE) {
		*regval = 0xc0;
	} else {
		*regval = 0x80;
	}
	pci_write_config(amd->dev, *regval, 0, /*bytes*/1);
	if (mode == DISABLE_CE) {
		pci_write_config(amd->dev, *regval, 0, /*bytes*/1);
	}
	DELAY(160);
}

static void
amd_EEpromOutDI(struct amd_softc *amd, int *regval, int Carry)
{
	u_int bval;

	bval = 0;
	if (Carry) {
		bval = 0x40;
		*regval = 0x80;
		pci_write_config(amd->dev, *regval, bval, /*bytes*/1);
	}
	DELAY(160);
	bval |= 0x80;
	pci_write_config(amd->dev, *regval, bval, /*bytes*/1);
	DELAY(160);
	pci_write_config(amd->dev, *regval, 0, /*bytes*/1);
	DELAY(160);
}

static int
amd_EEpromInDO(struct amd_softc *amd)
{
	pci_write_config(amd->dev, 0x80, 0x80, /*bytes*/1);
	DELAY(160);
	pci_write_config(amd->dev, 0x80, 0x40, /*bytes*/1);
	DELAY(160);
	if (pci_read_config(amd->dev, 0, /*bytes*/1) == 0x22)
		return (1);
	return (0);
}

static u_int16_t
EEpromGetData1(struct amd_softc *amd)
{
	u_int	  i;
	u_int	  carryFlag;
	u_int16_t wval;

	wval = 0;
	for (i = 0; i < 16; i++) {
		wval <<= 1;
		carryFlag = amd_EEpromInDO(amd);
		wval |= carryFlag;
	}
	return (wval);
}

static void
amd_Prepare(struct amd_softc *amd, int *regval, u_int8_t EEpromCmd)
{
	u_int i, j;
	int carryFlag;

	carryFlag = 1;
	j = 0x80;
	for (i = 0; i < 9; i++) {
		amd_EEpromOutDI(amd, regval, carryFlag);
		carryFlag = (EEpromCmd & j) ? 1 : 0;
		j >>= 1;
	}
}

static void
amd_ReadEEprom(struct amd_softc *amd)
{
	int	   regval;
	u_int	   i;
	u_int16_t *ptr;
	u_int8_t   cmd;

	ptr = (u_int16_t *)&amd->eepromBuf[0];
	cmd = EEPROM_READ;
	for (i = 0; i < 0x40; i++) {
		amd_EnDisableCE(amd, ENABLE_CE, &regval);
		amd_Prepare(amd, &regval, cmd);
		*ptr = EEpromGetData1(amd);
		ptr++;
		cmd++;
		amd_EnDisableCE(amd, DISABLE_CE, &regval);
	}
}

static void
amd_load_defaults(struct amd_softc *amd)
{
	int target;

	bzero(&amd->eepromBuf, sizeof amd->eepromBuf);
	for (target = 0; target < MAX_SCSI_ID; target++)
		amd->eepromBuf[target << 2] =
		    (TAG_QUEUING|EN_DISCONNECT|SYNC_NEGO|PARITY_CHK);
	amd->eepromBuf[EE_ADAPT_SCSI_ID] = 7;
	amd->eepromBuf[EE_MODE2] = ACTIVE_NEGATION|LUN_CHECK|GREATER_1G;
	amd->eepromBuf[EE_TAG_CMD_NUM] = 4;
}

static void
amd_load_eeprom_or_defaults(struct amd_softc *amd)
{
	u_int16_t  wval, *ptr;
	u_int8_t   i;

	amd_ReadEEprom(amd);
	wval = 0;
	ptr = (u_int16_t *) & amd->eepromBuf[0];
	for (i = 0; i < EE_DATA_SIZE; i += 2, ptr++)
		wval += *ptr;

	if (wval != EE_CHECKSUM) {
		if (bootverbose)
			printf("amd%d: SEEPROM data unavailable.  "
			       "Using default device parameters.\n",
			       amd->unit);
		amd_load_defaults(amd);
	}
}

/*
 **********************************************************************
 * Function      : static int amd_init (struct Scsi_Host *host)
 * Purpose       : initialize the internal structures for a given SCSI host
 * Inputs        : host - pointer to this host adapter's structure/
 **********************************************************************
 */
static int
amd_init(device_t dev)
{
	struct amd_softc *amd = device_get_softc(dev);
	struct resource	*iores;
	int	i, rid;
	u_int	bval;

	rid = PCI_BASE_ADDR0;
	iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
				   RF_ACTIVE);
	if (iores == NULL) {
		if (bootverbose)
			printf("amd_init: bus_alloc_resource failure!\n");
		return ENXIO;
	}
	amd->tag = rman_get_bustag(iores);
	amd->bsh = rman_get_bushandle(iores);

	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(/*parent_dmat*/NULL, /*alignment*/1,
			       /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/MAXBSIZE, /*nsegments*/AMD_NSEG,
			       /*maxsegsz*/AMD_MAXTRANSFER_SIZE,
			       /*flags*/BUS_DMA_ALLOCNOW,
			       &amd->buffer_dmat) != 0) {
		if (bootverbose)
			printf("amd_init: bus_dma_tag_create failure!\n");
		return ENXIO;
        }

	/* Create, allocate, and map DMA buffers for autosense data */
	if (bus_dma_tag_create(/*parent_dmat*/NULL, /*alignment*/1,
			       /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       sizeof(struct scsi_sense_data) * MAX_SRB_CNT,
			       /*nsegments*/1,
			       /*maxsegsz*/AMD_MAXTRANSFER_SIZE,
			       /*flags*/0, &amd->sense_dmat) != 0) {
		if (bootverbose)
			device_printf(dev, "cannot create sense buffer dmat\n");
		return (ENXIO);
	}

	if (bus_dmamem_alloc(amd->sense_dmat, (void **)&amd->sense_buffers,
			     BUS_DMA_NOWAIT, &amd->sense_dmamap) != 0)
		return (ENOMEM);

	bus_dmamap_load(amd->sense_dmat, amd->sense_dmamap,
		       amd->sense_buffers,
		       sizeof(struct scsi_sense_data) * MAX_SRB_CNT,
		       amd_dmamap_cb, &amd->sense_busaddr, /*flags*/0);

	TAILQ_INIT(&amd->free_srbs);
	TAILQ_INIT(&amd->running_srbs);
	TAILQ_INIT(&amd->waiting_srbs);
	amd->last_phase = SCSI_BUS_FREE;
	amd->dev = dev;
	amd->unit = device_get_unit(dev);
	amd->SRBCount = MAX_SRB_CNT;
	amd->status = 0;
	amd_load_eeprom_or_defaults(amd);
	amd->max_id = 7;
	if (amd->eepromBuf[EE_MODE2] & LUN_CHECK) {
		amd->max_lun = 7;
	} else {
		amd->max_lun = 0;
	}
	amd->AdaptSCSIID = amd->eepromBuf[EE_ADAPT_SCSI_ID];
	amd->HostID_Bit = (1 << amd->AdaptSCSIID);
	amd->AdaptSCSILUN = 0;
	/* (eepromBuf[EE_TAG_CMD_NUM]) << 2; */
	amd->ACBFlag = 0;
	amd->Gmode2 = amd->eepromBuf[EE_MODE2];
	amd_linkSRB(amd);
	for (i = 0; i <= amd->max_id; i++) {

		if (amd->AdaptSCSIID != i) {
			struct amd_target_info *tinfo;
			PEEprom prom;

			tinfo = &amd->tinfo[i];
			prom = (PEEprom)&amd->eepromBuf[i << 2];
			if ((prom->EE_MODE1 & EN_DISCONNECT) != 0) {
				tinfo->disc_tag |= AMD_USR_DISCENB;
				if ((prom->EE_MODE1 & TAG_QUEUING) != 0)
					tinfo->disc_tag |= AMD_USR_TAGENB;
			}
			if ((prom->EE_MODE1 & SYNC_NEGO) != 0) {
				tinfo->user.period =
				    eeprom_period[prom->EE_SPEED];
				tinfo->user.offset = AMD_MAX_SYNC_OFFSET;
			}
			tinfo->CtrlR1 = amd->AdaptSCSIID;
			if ((prom->EE_MODE1 & PARITY_CHK) != 0)
				tinfo->CtrlR1 |= PARITY_ERR_REPO;
			tinfo->CtrlR3 = FAST_CLK;
			tinfo->CtrlR4 = EATER_25NS;
			if ((amd->eepromBuf[EE_MODE2] & ACTIVE_NEGATION) != 0)
				tinfo->CtrlR4 |= NEGATE_REQACKDATA;
		}
	}
	amd_write8(amd, SCSITIMEOUTREG, 153); /* 250ms selection timeout */
	/* Conversion factor = 0 , 40MHz clock */
	amd_write8(amd, CLKFACTREG, CLK_FREQ_40MHZ);
	/* NOP cmd - clear command register */
	amd_write8(amd, SCSICMDREG, NOP_CMD);	
	amd_write8(amd, CNTLREG2, EN_FEATURE|EN_SCSI2_CMD);
	amd_write8(amd, CNTLREG3, FAST_CLK);
	bval = EATER_25NS;
	if (amd->eepromBuf[EE_MODE2] & ACTIVE_NEGATION) {
		bval |= NEGATE_REQACKDATA;
	}
	amd_write8(amd, CNTLREG4, bval);

	/* Disable SCSI bus reset interrupt */
	amd_write8(amd, CNTLREG1, DIS_INT_ON_SCSI_RST);

	return 0;
}

/*
 * attach and init a host adapter
 */
static int
amd_attach(device_t dev)
{
	struct cam_devq	*devq;	/* Device Queue to use for this SIM */
	u_int8_t	intstat;
	struct amd_softc *amd = device_get_softc(dev);
	int		unit = device_get_unit(dev);
	int		rid;
	void		*ih;
	struct resource	*irqres;

	if (amd_init(dev)) {
		if (bootverbose)
			printf("amd_attach: amd_init failure!\n");
		return ENXIO;
	}

	/* Reset Pending INT */
	intstat = amd_read8(amd, INTSTATREG);

	/* After setting up the adapter, map our interrupt */
	rid = 0;
	irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				    RF_SHAREABLE | RF_ACTIVE);
	if (irqres == NULL ||
	    bus_setup_intr(dev, irqres, INTR_TYPE_CAM | INTR_ENTROPY,
	    amd_intr, amd, &ih)) {
		if (bootverbose)
			printf("amd%d: unable to register interrupt handler!\n",
			       unit);
		return ENXIO;
	}

	/*
	 * Now let the CAM generic SCSI layer find the SCSI devices on
	 * the bus *  start queue to reset to the idle loop. *
	 * Create device queue of SIM(s) *  (MAX_START_JOB - 1) :
	 * max_sim_transactions
	 */
	devq = cam_simq_alloc(MAX_START_JOB);
	if (devq == NULL) {
		if (bootverbose)
			printf("amd_attach: cam_simq_alloc failure!\n");
		return ENXIO;
	}

	amd->psim = cam_sim_alloc(amd_action, amd_poll, "amd",
				  amd, amd->unit, 1, MAX_TAGS_CMD_QUEUE,
				  devq);
	if (amd->psim == NULL) {
		cam_simq_free(devq);
		if (bootverbose)
			printf("amd_attach: cam_sim_alloc failure!\n");
		return ENXIO;
	}

	if (xpt_bus_register(amd->psim, 0) != CAM_SUCCESS) {
		cam_sim_free(amd->psim, /*free_devq*/TRUE);
		if (bootverbose)
			printf("amd_attach: xpt_bus_register failure!\n");
		return ENXIO;
	}

	if (xpt_create_path(&amd->ppath, /* periph */ NULL,
			    cam_sim_path(amd->psim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(amd->psim));
		cam_sim_free(amd->psim, /* free_simq */ TRUE);
		if (bootverbose)
			printf("amd_attach: xpt_create_path failure!\n");
		return ENXIO;
	}

	return 0;
}

static int
amd_probe(device_t dev)
{
	if (pci_get_devid(dev) == PCI_DEVICE_ID_AMD53C974) {
		device_set_desc(dev,
			"Tekram DC390(T)/AMD53c974 SCSI Host Adapter");
		return 0;
	}
	return ENXIO;
}

static device_method_t amd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		amd_probe),
	DEVMETHOD(device_attach,	amd_attach),
	{ 0, 0 }
};

static driver_t amd_driver = {
	"amd", amd_methods, sizeof(struct amd_softc)
};

static devclass_t amd_devclass;
DRIVER_MODULE(amd, pci, amd_driver, amd_devclass, 0, 0);
