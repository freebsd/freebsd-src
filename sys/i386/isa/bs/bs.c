/*	$NecBSD: bs.c,v 1.1 1997/07/18 09:18:59 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/* $FreeBSD: src/sys/i386/isa/bs/bs.c,v 1.8 1999/12/03 11:58:11 nyan Exp $ */
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

#ifdef	__NetBSD__
#include <i386/Cbus/dev/bs/bsif.h>
#endif
#ifdef	__FreeBSD__
#include <i386/isa/bs/bsif.h>
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

/*****************************************************************
 * Inline phase funcs
 *****************************************************************/
/* static inline declare */
static BS_INLINE struct targ_info *bs_reselect __P((struct bs_softc *));
static BS_INLINE void bs_sat_continue __P((struct bs_softc *, struct targ_info *, struct bsccb *));
static BS_INLINE struct targ_info *bs_selected __P((struct bs_softc *, struct targ_info *, struct bsccb *));
static BS_INLINE u_int8_t bs_read_1byte __P((struct bs_softc *));
static BS_INLINE void bs_write_1byte __P((struct bs_softc *, u_int8_t));
static BS_INLINE void bs_commandout __P((struct bs_softc *, struct targ_info *, struct bsccb *));
static BS_INLINE void bs_status_check __P((struct bs_softc *, struct targ_info *));
static BS_INLINE void bs_msgin __P((struct bs_softc *, struct targ_info *));
static BS_INLINE void bs_msgout __P((struct bs_softc *, struct targ_info *, struct bsccb *));
static BS_INLINE void bs_disconnect_phase __P((struct bs_softc *, struct targ_info *, struct bsccb *));
static void bs_phase_error __P((struct targ_info *, struct bsccb *));
static int bs_scsi_cmd_poll_internal __P((struct targ_info *));
static int bs_xfer __P((struct bs_softc *, char *, int));
static void bs_io_xfer __P((struct targ_info *));
static void bs_quick_abort __P((struct targ_info *, u_int));
static void bs_msgin_error __P((struct targ_info *, u_int));
static void bs_msgin_ext __P((struct targ_info *));
static void bs_msg_reject __P((struct targ_info *));
static void bshoststart __P((struct bs_softc *, struct targ_info *));

/*****************************************************************
 * SIM interface
 *****************************************************************/
void
bs_scsi_cmd(struct cam_sim *sim, union ccb *ccb)
{
	struct bs_softc *bsc = (struct bs_softc *) cam_sim_softc(sim);
	int s, target = (u_int) (ccb->ccb_h.target_id);
	struct targ_info *ti;
	struct bsccb *cb;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		ti = bsc->sc_ti[target];
		if ((cb = bs_get_ccb()) == NULL) {
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(ccb);
			return;
		}

		/* make up ccb! */
		cb->ccb = ccb;
		cb->lun = ccb->ccb_h.target_lun;
		cb->cmd = ccb->csio.cdb_io.cdb_bytes;
		cb->cmdlen = (int) ccb->csio.cdb_len;
		cb->data = ccb->csio.data_ptr;
		cb->datalen = (int) ccb->csio.dxfer_len;
		cb->rcnt = 0;
		cb->msgoutlen = 0;
		cb->bsccb_flags = 0;
		bs_targ_flags(ti, cb);
		cb->tcmax = 0;/*(xs->timeout >> 10); default HN2*/
		if (cb->tcmax < BS_DEFAULT_TIMEOUT_SECOND)
			cb->tcmax = BS_DEFAULT_TIMEOUT_SECOND;

		s = splcam();

		TAILQ_INSERT_TAIL(&ti->ti_ctab, cb, ccb_chain);

		if (ti->ti_phase == FREE) {
			if (ti->ti_state == BS_TARG_START)
				bs_start_syncmsg(ti, NULL, BS_SYNCMSG_ASSERT);
			bscmdstart(ti, BSCMDSTART);
		}

		splx(s);
		break;
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
		/* XXX Implement */
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	case XPT_GET_TRAN_SETTINGS: {
		struct ccb_trans_settings *cts;
		struct targ_info *ti;
		/*int s;*/

		cts = &ccb->cts;
		ti = bsc->sc_ti[ccb->ccb_h.target_id];
		/*s = splcam();*/
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
			if (ti->ti_cfgflags & BS_SCSI_DISC)
				cts->flags = CCB_TRANS_DISC_ENB;
			else
				cts->flags = 0;
			if (ti->ti_cfgflags & BS_SCSI_QTAG)
				cts->flags |= CCB_TRANS_TAG_ENB;
			cts->sync_period = ti->ti_syncnow.period;
			cts->sync_offset = ti->ti_syncnow.offset;
			cts->bus_width = 0;/*HN2*/

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;

		/*splx(s);*/
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY: { /* not yet HN2 */
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		
		ccg->heads = 8;
		ccg->secs_per_track = 34;

		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
		bshw_chip_reset(bsc);	/* XXX need perfect RESET? */
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ: {		/* Path routing inquiry */
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = NTARGETS - 1;
		cpi->max_lun = 7;
		cpi->initiator_id = bsc->sc_hostid;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "NEC", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
/*printf("bs: non support func_code = %d ", ccb->ccb_h.func_code);*/
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

/**************************************************
 * ### NEXUS START and TERMINATE ###
 **************************************************/
/*
 * FLAGS     : BSCMDRESTART restart in case of error.
 */
int
bscmdstart(ti, flags)
	struct targ_info *ti;
	int flags;
{
	struct bsccb *cb;
	struct bs_softc *bsc = ti->ti_bsc;

	if ((cb = ti->ti_ctab.tqh_first) == NULL)
	{
		if (bsc->sc_nexus == NULL)
			bshoststart(bsc, NULL);
		return 0;
	}

	ti->ti_lun = cb->lun;
	ti->ti_error = 0;
	ti->ti_scsp.data = cb->data;
	ti->ti_scsp.datalen = cb->datalen;
	ti->ti_scsp.seglen = 0;
	if (cb->rcnt)
		cb->bsccb_flags &= ~(BSSAT | BSLINK);
	ti->ti_flags &= ~BSCFLAGSMASK;
	ti->ti_flags |= cb->bsccb_flags & BSCFLAGSMASK;
	cb->tc = cb->tcmax;

	/* GO GO */
	if (ti->ti_phase == FREE)
	{
		if (bsc->sc_nexus == NULL)
			bshoststart(bsc, ti);
		else
		{
			if (flags & BSCMDRESTART)
				bs_hostque_head(bsc, ti);
			else
				bs_hostque_tail(bsc, ti);
			BS_SETUP_PHASE(HOSTQUEUE)
		}
	}
	else if (bsc->sc_nexus == NULL)
		bshoststart(bsc, NULL);

	return 1;
}

struct bsccb *
bscmddone(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct bsccb *cb = ti->ti_ctab.tqh_first;
	union ccb *ccb;
	int error;

	if (ti->ti_state == BS_TARG_SYNCH)
	{
		if (bs_analyze_syncmsg(ti, cb))
			return cb;
	}

	if (bsc->sc_p.datalen != 0)
		ti->ti_error |= BSDMAABNORMAL;

	cb->error = ti->ti_error;

	do
	{
		ccb = cb->ccb;
		error = CAM_REQ_CMP;

		if (cb->bsccb_flags & (BSITSDONE | BSSENSECCB | BSCASTAT))
		{
			if (cb->bsccb_flags & BSSENSECCB)
			{
				cb->error &= ~BSDMAABNORMAL;
				if (cb->error == 0)
					ti->ti_flags |= BSCASTAT;

				ti->ti_flags |= BSERROROK;
			}
			else if (cb->bsccb_flags & BSCASTAT)
			{
				if (ti->ti_flags & BSCASTAT)
				{
					ti->ti_flags &= ~BSCASTAT;
					error = CAM_AUTOSNS_VALID|CAM_SCSI_STATUS_ERROR;
					if (ccb)
						ccb->csio.sense_data = ti->sense;/* XXX may not be csio.... */
				}
				else
					error = CAM_AUTOSENSE_FAIL;
				ti->ti_flags |= BSERROROK;
			} else
				bs_panic(bsc, "internal error");
		}

		while (cb->error)
		{
			if (ti->ti_flags & BSERROROK)
				break;

			if (cb->rcnt >= bsc->sc_retry || (cb->error & BSFATALIO))
			{
				if (cb->error & (BSSELTIMEOUT | BSTIMEOUT))
					error = CAM_CMD_TIMEOUT;
				else if (cb->error & BSTARGETBUSY)
					error = CAM_SCSI_STATUS_ERROR;
				else
					error = CAM_REQ_CMP_ERR;
				break;
			}

			if (cb->error & BSREQSENSE)
			{
				/* must clear the target's sense state */
				cb->rcnt++;
				cb->bsccb_flags |= (BSITSDONE | BSCASTAT);
				cb->error &= ~BSREQSENSE;
				return bs_request_sense(ti);
			}

			/* XXX: compat with upper driver */
			if ((cb->error & BSDMAABNORMAL) &&
			     BSHW_CMD_CHECK(cb, BSERROROK))
			{
				cb->error &= ~BSDMAABNORMAL;
				continue;
			}
			if (/*(xs && xs->bp) || can't know whether bufferd i/o or not */
			    (cb->error & BSSELTIMEOUT) == 0)
				bs_debug_print(bsc, ti);
			cb->rcnt++;
			return cb;
		}

#ifdef	BS_DIAG
		cb->bsccb_flags |= BSITSDONE;
#endif	/* BS_DIAG */
		if (bsc->sc_poll)
		{
			bsc->sc_flags |= BSJOBDONE;
			if (bsc->sc_outccb == cb)
			       bsc->sc_flags |= BSPOLLDONE;
		}

		TAILQ_REMOVE(&ti->ti_ctab, cb, ccb_chain);

		if (ccb)
		{
			ccb->ccb_h.status = error;
			ccb->csio.scsi_status = ti->ti_status;/*XXX*/
			xpt_done(ccb);
		}

		bs_free_ccb(cb);
		cb = ti->ti_ctab.tqh_first;

	}
	while (cb != NULL && (cb->bsccb_flags & BSITSDONE) != 0);

	/* complete */
	return NULL;
}

/**************************************************
 * ### PHASE FUNCTIONS ###
 **************************************************/
/**************************************************
 * <SELECTION PHASE>
 **************************************************/
static void
bshoststart(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{
	struct bsccb *cb;
	int s;

	if (bsc->sc_flags & BSINACTIVE)
		return;

again:
	if (ti == NULL)
	{
		if ((ti = bsc->sc_sttab.tqh_first) == NULL)
			return;
		bs_hostque_delete(bsc, ti);
	}

	if ((cb = ti->ti_ctab.tqh_first) == NULL)
	{
		bs_printf(ti, "bshoststart", "Warning: No ccb");
		BS_SETUP_PHASE(FREE);
		ti = NULL;
		goto again;
	}

#ifdef	BS_DIAG
	if (cb->bsccb_flags & BSITSDONE)
		bs_panic(bsc, "bshoststart: already done");

	if (bsc->sc_nexus || (ti->ti_flags & BSNEXUS))
	{
		char *s = ((ti->ti_flags & BSNEXUS) ?
				"nexus already established" : "scsi board busy");

		bs_debug_print(bsc, ti);
		bs_printf(ti, "bshoststart", s);
	}
#endif	/* BS_DIAG */

#ifdef	BS_STATICS
	bs_statics[ti->ti_id].select++;
#endif	/* BS_STATICS */

	if (ti->ti_cfgflags & BS_SCSI_WAIT)
	{
		struct targ_info *tmpti;

		for (tmpti = bsc->sc_titab.tqh_first; tmpti;
		     tmpti = tmpti->ti_tchain.tqe_next)
			if (tmpti->ti_phase >= DISCONNECTED)
				goto retry;
	}

	/* start selection */
	ti->ti_status = ST_UNK;
	if (bs_check_sat(ti))
	{
		if ((bshw_get_auxstat(bsc) & STR_BUSY) == 0)
		{
			BS_LOAD_SDP
			bshw_set_dst_id(bsc, ti->ti_id, ti->ti_lun);
			bshw_setup_ctrl_reg(bsc, ti->ti_cfgflags);
			bshw_cmd_pass(bsc, 0);
			bshw_set_sync_reg(bsc, ti->ti_sync);
			bshw_issue_satcmd(bsc, cb, bs_check_link(ti, cb));
			if (bs_check_smit(ti) || bsc->sc_p.datalen <= 0)
				bshw_set_count(bsc, 0);
			else
				bs_dma_xfer(ti, BSHW_CMD_CHECK(cb, BSREAD));

			s = splhigh();
			if ((bshw_get_auxstat(bsc) & STR_BUSY) == 0)
			{
				/* XXX:
				 * Reload a lun again here.
				 */
				bshw_set_lun(bsc, ti->ti_lun);
				bshw_start_sat(bsc, bs_check_disc(ti));
				if ((bshw_get_auxstat(bsc) & STR_LCI) == 0)
				{
					splx(s);
					BS_HOST_START
					BS_SELECTION_START
					BS_SETUP_PHASE(SATSEL);
					ti->ti_omsgoutlen = 0;
					ti->ti_msgout = bs_identify_msg(ti);
#ifdef	BS_DIAG
					ti->ti_flags |= BSNEXUS;
#endif	/* BS_DIAG */
#ifdef	BS_STATICS
					bs_statics[ti->ti_id].select_win++;
#endif	/* BS_STATICS */
					return;
				}
			}
			splx(s);

			if (bs_check_smit(ti) == 0)
				bshw_dmaabort(bsc, ti);
#ifdef	BS_STATICS
			bs_statics[ti->ti_id].select_miss_in_assert++;
#endif	/* BS_STATICS */
		}
	}
	else
	{
		s = splhigh();
		if ((bshw_get_auxstat(bsc) & STR_BUSY) == 0)
		{
			bshw_set_dst_id(bsc, ti->ti_id, ti->ti_lun);
			bshw_setup_ctrl_reg(bsc, ti->ti_cfgflags);
			bshw_set_sync_reg(bsc, ti->ti_sync);
			bshw_assert_select(bsc);

			if ((bshw_get_auxstat(bsc) & STR_LCI) == 0)
			{
				splx(s);
				BS_HOST_START
				BS_SELECTION_START
				BS_SETUP_PHASE(SELECTASSERT);
#ifdef	BS_STATICS
				bs_statics[ti->ti_id].select_win++;
#endif	/* BS_STATICS */
				return;
			}
#ifdef	BS_STATICS
			bs_statics[ti->ti_id].select_miss_in_assert++;
#endif	/* BS_STATICS */
		}
		splx(s);
	}

	/* RETRY LATER */
retry:
#ifdef	BS_STATICS
	bs_statics[ti->ti_id].select_miss++;
#endif	/* BS_STATICS */
	bs_hostque_head(bsc, ti);
	BS_SETUP_PHASE(HOSTQUEUE)
}

static BS_INLINE struct targ_info *
bs_selected(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{

	if (bsc->sc_busstat != BSR_SELECTED)
	{
		bs_phase_error(ti, cb);
		return NULL;
	}

#ifdef	BS_DIAG
	if (bsc->sc_selwait != ti)
		panic("%s selection internal error\n", bsc->sc_dvname);

	ti->ti_flags |= BSNEXUS;
#endif	/* BS_DIAG */

	/* clear select wait state */
	BS_SETUP_PHASE(SELECTED);
	BS_SELECTION_TERMINATE;
	BS_LOAD_SDP
	return ti;
}

/**************************************************
 *  <RESELECTION>
 **************************************************/
static BS_INLINE struct targ_info *
bs_reselect(bsc)
	struct bs_softc *bsc;
{
	u_int target;
	struct targ_info *ti;

	/* check collision */
	if ((ti = bsc->sc_selwait) != NULL)
	{
		if (ti->ti_phase == SATSEL)
		{
#ifdef	BS_DIAG
			ti->ti_flags &= ~BSNEXUS;
#endif	/* BS_DIAG */
			ti->ti_msgout = 0;
			if (bs_check_smit(ti) == 0)
				bshw_dmaabort(bsc, ti);
		}
		bs_hostque_head(bsc, ti);
		BS_SELECTION_TERMINATE
		BS_SETUP_PHASE(HOSTQUEUE)
#ifdef	BS_STATICS
		bs_statics[ti->ti_id].select_miss_by_reselect++;
		bs_statics[ti->ti_id].select_miss++;
#endif	/* BS_STATICS */
	}

	/* who are you ? */
	target = bshw_get_src_id(bsc);
	if ((ti = bsc->sc_ti[target]) == NULL)
	{
		bs_debug_print_all(bsc);
		printf("reselect: miss reselect. target(%d)\n", target);
		bs_reset_nexus(bsc);
		return NULL;
	}

	/* confirm nexus */
	BS_HOST_START
	bshw_setup_ctrl_reg(bsc, ti->ti_cfgflags);
	if (ti->ti_ctab.tqh_first == NULL || ti->ti_phase != DISCONNECTED)
	{
		bs_printf(ti, "reselect", "phase mismatch");
		BS_SETUP_PHASE(UNDEF)
		bs_force_abort(ti);
		bs_hostque_delete(bsc, ti);
	}
	else
		bsc->sc_dtgnum --;

	/* recover host */
	bshw_set_dst_id(bsc, ti->ti_id, ti->ti_lun);
	bshw_set_sync_reg(bsc, ti->ti_sync);
	BS_RESTORE_SDP
	BS_SETUP_PHASE(RESELECTED)
#ifdef	BS_STATICS
	bs_statics[ti->ti_id].reselect++;
#endif	/* BS_STATICS */
	return ti;
}

static BS_INLINE void
bs_sat_continue(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{

	BS_SETUP_PHASE(SATRESEL);
	bshw_set_dst_id(bsc, ti->ti_id, ti->ti_lun);
	bshw_cmd_pass(bsc, 0x44);
	bshw_set_sync_reg(bsc, ti->ti_sync);
	bshw_issue_satcmd(bsc, cb, 0);
	if (bs_check_smit(ti) || bsc->sc_p.datalen <= 0)
		bshw_set_count(bsc, 0);
	else
		bs_dma_xfer(ti, BSHW_CMD_CHECK(cb, BSREAD));
	bshw_set_lun(bsc, ti->ti_lun);		/* XXX */
	bshw_start_sat(bsc, 0);
}

/*************************************************
 * <DATA PHASE>
 *************************************************/
#define	DR	(STR_BSY | STR_DBR)

void
bs_poll_timeout(bsc, s)
	struct bs_softc *bsc;
	char *s;
{
	struct targ_info *ti;

	bs_printf(NULL, s, "timeout");
	bsc->sc_flags |= BSRESET;
	if ((ti = bsc->sc_nexus) && ti->ti_ctab.tqh_first)
		ti->ti_error |= BSTIMEOUT;
}

static BS_INLINE u_int8_t
bs_read_1byte(bsc)
	struct bs_softc *bsc;
{
	register u_int wc;

	bshw_start_sxfer(bsc);
	for (wc = bsc->sc_wc; (bshw_get_auxstat(bsc) & DR) != DR && --wc; );
	if (wc)
		return bshw_read_data(bsc);
	else
		bs_poll_timeout(bsc, "read_1byte");

	return 0;
}

static BS_INLINE void
bs_write_1byte(bsc, data)
	struct bs_softc *bsc;
	u_int8_t data;
{
	register u_int wc;

	bshw_start_sxfer(bsc);
	for (wc = bsc->sc_wc; (bshw_get_auxstat(bsc) & DR) != DR && --wc; );
	if (wc)
		bshw_write_data(bsc, data);
	else
		bs_poll_timeout(bsc, "write_1byte");
}

static int
bs_xfer(bsc, data, len)
	struct bs_softc *bsc;
	char *data;
	int len;
{
	u_int8_t aux;
	u_int count, wc;

	bshw_set_count(bsc, len);
	bshw_start_xfer(bsc);

	for (count = 0, wc = bsc->sc_wc; count < len && --wc; )
	{
		if (((aux = bshw_get_auxstat(bsc)) & DR) == DR)
		{
			if (bsc->sc_busstat & BSHW_READ)
				*(data++) = bshw_read_data(bsc);
			else
				bshw_write_data(bsc, *(data++));
			count++;
			wc = bsc->sc_wc;
		}

		if (aux & STR_INT)
			break;
	}

	if (wc == 0)
		bs_poll_timeout(bsc, "bs_xfer");

	return count;
}
#undef	DR

static void
bs_io_xfer(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct sc_p *sp = &bsc->sc_p;
	u_int count, dummy;

	/* switch dma trasnfr mode */
	bshw_set_poll_trans(bsc, ti->ti_cfgflags);
	sp->seglen = 0;
	sp->bufp = NULL;

	if (sp->datalen <= 0)
	{
		ti->ti_error |= BSDMAABNORMAL;
		dummy = 0;
		count = bs_xfer(bsc, (u_int8_t *) &dummy, 1);
	}
	else
		count = bs_xfer(bsc, sp->data, sp->datalen);

	sp->data += count;
	sp->datalen -= count;
}

/************************************************
 * <COMMAND PHASE>
 ************************************************/
static BS_INLINE void
bs_commandout(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{
	u_int8_t scsi_cmd[16];
	int len;

	BS_SETUP_PHASE(CMDPHASE);

	if (bs_check_link(ti, cb))
	{
		bcopy(cb->cmd, scsi_cmd, cb->cmdlen);
		scsi_cmd[cb->cmdlen - 1] |= 0x01;
		len = bs_xfer(bsc, scsi_cmd, cb->cmdlen);
	}
	else
		len = bs_xfer(bsc, cb->cmd, cb->cmdlen);

	if (len != cb->cmdlen)
		ti->ti_error |= BSCMDABNORMAL;
}

/************************************************
 * <STATUS IN>
 ************************************************/
static BS_INLINE void
bs_status_check(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_status == ST_GOOD || ti->ti_status == ST_INTERGOOD)
		return;

	switch (ti->ti_status)
	{
	case ST_MET:
	case ST_INTERMET:
	case ST_CHKCOND:
		ti->ti_error |= BSREQSENSE;
		break;

	case ST_BUSY:
		ti->ti_error |= BSTARGETBUSY;
		break;

	default:
		ti->ti_error |= BSSTATUSERROR;
		break;
	}
}

/************************************************
 * <MSG IN>
 ************************************************/
#define	MSGWAIT(cnt) { if (ti->ti_msginptr < (cnt)) return; }

static void
bs_quick_abort(ti, msg)
	struct targ_info *ti;
	u_int msg;
{
	struct bsccb *cb;

	if ((cb = ti->ti_ctab.tqh_first) == NULL)
		return;

	cb->msgoutlen = 1;
	cb->msgout[0] = msg;
	cb->rcnt++;

	ti->ti_error |= BSMSGERROR;
}

static void
bs_msgin_error(ti, count)
	struct targ_info *ti;
	u_int count;
{
	int n;

	MSGWAIT(count);

	bs_printf(ti, "msgin", "illegal msg");

	for (n = 0; n < ti->ti_msginptr; n ++)
		printf("[0x%x] ", (u_int) ti->ti_msgin[n]);
	printf("\n");

	bs_quick_abort(ti, MSG_REJECT);
	ti->ti_msginptr = 0;
}

static void
bs_msgin_ext(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct bsccb *cb = ti->ti_ctab.tqh_first;
	int count;
	u_int reqlen;
	u_int32_t *ptr;
	struct msgbase msg;

	MSGWAIT(2);

	reqlen = ti->ti_msgin[1];
	if (reqlen == 0)
		reqlen = 256;

	if (ti->ti_msginptr >= MAXMSGLEN)
		ti->ti_msginptr = 3;	/* XXX */

	MSGWAIT(reqlen + 2);

	switch (MKMSG_EXTEND(ti->ti_msgin[1], ti->ti_msgin[2]))
	{
	case MKMSG_EXTEND(MSG_EXTEND_MDPLEN, MSG_EXTEND_MDPCODE):
		ptr = (u_int32_t *)(&ti->ti_msgin[3]);
		count = (int) htonl((long) (*ptr));

		bsc->sc_p.seglen = ti->ti_scsp.seglen = 0;
		if (bsc->sc_p.datalen - count >= 0 &&
		    bsc->sc_p.datalen - count <= cb->datalen)
		{
			bsc->sc_p.datalen -= count;
			bsc->sc_p.data += count;
		}
		else
			bs_msgin_error(ti, 7);
		break;

	case MKMSG_EXTEND(MSG_EXTEND_SYNCHLEN, MSG_EXTEND_SYNCHCODE):
		ti->ti_syncnow.period = ti->ti_msgin[3];
		ti->ti_syncnow.offset = ti->ti_msgin[4];
		if (ti->ti_syncnow.offset == 0)
			ti->ti_syncnow.period = 0;

		if (ti->ti_syncnow.state != BS_SYNCMSG_ASSERT)
		{
			bs_start_syncmsg(ti, NULL, BS_SYNCMSG_REQUESTED);
			bscmdstart(ti, BSCMDSTART);
		}
		else
			BS_SETUP_SYNCSTATE(BS_SYNCMSG_ACCEPT)
		break;

	case MKMSG_EXTEND(MSG_EXTEND_WIDELEN, MSG_EXTEND_WIDECODE):
		msg.msglen = MSG_EXTEND_WIDELEN + 2;
		msg.msg[0] = MSG_EXTEND;
		msg.msg[1] = MSG_EXTEND_WIDELEN;
		msg.msg[2] = MSG_EXTEND_WIDECODE;
		msg.msg[3] = 0;
		msg.flag = 0;
		bs_make_msg_ccb(ti, cb->lun, cb, &msg, 0);
		break;

	default:
		bs_msgin_error(ti, 0);
		return;
	}

	ti->ti_msginptr = 0;
	return;
}

static void
bs_msg_reject(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct bsccb *cb = ti->ti_ctab.tqh_first;
	char *s = "unexpected msg reject";

	switch (ti->ti_ophase)
	{
	case CMDPHASE:
		s = "cmd rejected";
		cb->bsccb_flags &= ~BSLINK;
		BS_SETUP_MSGPHASE(IOCOMPLETED);
		break;

	case MSGOUT:
		if (ti->ti_msgout & 0x80)
		{
			s = "identify msg rejected";
			cb->bsccb_flags &= ~BSDISC;
			BS_SETUP_MSGPHASE(IOCOMPLETED);
		}
		else if (ti->ti_msgout == MSG_EXTEND)
		{
			switch (ti->ti_emsgout)
			{
			case MSG_EXTEND_SYNCHCODE:
				BS_SETUP_SYNCSTATE(BS_SYNCMSG_REJECT);
				return;

			default:
				break;
			}
		}
		break;

	default:
		break;
	}

	bs_debug_print(bsc, ti);
	bs_printf(ti, "msgin", s);
	ti->ti_error |= BSMSGERROR;
}

static BS_INLINE void
bs_msgin(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	BS_SETUP_PHASE(MSGIN);

	switch (ti->ti_msgin[0])
	{
	case MSG_SAVESP:
		BS_SAVE_SDP
		break;

	case MSG_RESTORESP:
		BS_RESTORE_SDP
		bs_printf(ti, "msgin", "restore scsi pointer");
		break;

	case MSG_REJECT:
		bs_msg_reject(ti);
		break;

	case 0xf:
		break;

	case MSG_I_ERROR:/* all I -> T : nothing to do*/
	case MSG_ABORT:
	case MSG_PARITY:
	case MSG_RESET:
	case 0xe:
		bs_msgin_error(ti, 1);
		goto resume;

	case MSG_NOOP:
		break;

	case MSG_EXTEND:
		bs_msgin_ext(ti);
		goto resume;

	case 0xd:
		bs_msgin_error(ti, 2);
		goto resume;

	case MSG_DISCON:
		BS_SETUP_MSGPHASE(DISCONNECTASSERT);
		break;

	case MSG_COMP:
		BS_SETUP_MSGPHASE(IOCOMPLETED);
		break;

	case MSG_LCOMP:
	case MSG_LCOMP_F:
		bs_status_check(bsc, ti);
		if (bscmddone(ti) == NULL)
		{
			if (bscmdstart(ti, BSCMDSTART) == 0)
			{
				bs_printf(ti, "msgin", "cmd line miss");
				bs_force_abort(ti);
			}
		}
		else
			bscmdstart(ti, BSCMDRESTART);
#ifdef	BS_STATICS
		bs_linkcmd_count[ti->ti_id]++;
#endif	/* BS_STATICS */
		BS_LOAD_SDP
		ti->ti_status = ST_UNK;
		break;

	default:
		if (ti->ti_msgin[0] & 0x80)
		{
			if ((ti->ti_msgin[0] & 0x07) != ti->ti_lun)
			{
				ti->ti_lun = (ti->ti_msgin[0] & 0x07);
				bshw_set_dst_id(bsc, ti->ti_id, ti->ti_lun);
				bshw_set_sync_reg(bsc, ti->ti_sync);

				bs_printf(ti, "msgin", "lun error");
				bs_quick_abort(ti, MSG_ABORT);
			}
			break;
		}
		else if (ti->ti_msgin[0] < 0x20)
			bs_msgin_error(ti, 1);
		else if (ti->ti_msgin[0] < 0x30)
			bs_msgin_error(ti, 2);
		else
			bs_msgin_error(ti, 1);
		goto resume;
	}

	ti->ti_msginptr = 0;

resume:
	return;
}

/************************************************
 * <MSG OUT>
 ************************************************/
static BS_INLINE void
bs_msgout(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{
	u_int8_t msg[MAXMSGLEN + 1];

	if (ti->ti_phase == MSGOUT)
	{
		if (cb->rcnt ++ < bsc->sc_retry)
			cb->msgoutlen = ti->ti_omsgoutlen;
	}
	else
		BS_SETUP_PHASE(MSGOUT);

	if (ti->ti_ophase == SELECTED)
	{
identify:
		if (cb->msgoutlen == 0)
		{
			ti->ti_msgout = bs_identify_msg(ti);
			ti->ti_omsgoutlen = 0;
			bs_write_1byte(bsc, ti->ti_msgout);
		}
		else
		{
			if (cb->msgout[0] != MSG_RESET &&
			    cb->msgout[0] != MSG_ABORT)
			{
				msg[0] = bs_identify_msg(ti);
				bcopy(cb->msgout, &msg[1], cb->msgoutlen);
				bs_xfer(bsc, msg, cb->msgoutlen + 1);
			}
			else
				bs_xfer(bsc, cb->msgout, cb->msgoutlen);

			ti->ti_msgout = cb->msgout[0];
			ti->ti_emsgout = cb->msgout[2];
			ti->ti_omsgoutlen = cb->msgoutlen;
			cb->msgoutlen = 0;
		}
		return;
	}

	if (ti->ti_ophase == SATSEL)
	{
		/* XXX:
		 * Maybe identify msg rejected due to
		 * a parity error in target side.
		 */

		bs_printf(ti, "msgout", "msg identify retry (SAT)");
		goto identify;
	}

	if (cb->msgoutlen == 0)
	{
		ti->ti_msgout = MSG_REJECT;
		ti->ti_omsgoutlen = 0;
		bs_write_1byte(bsc, ti->ti_msgout);
	}
	else
	{
		ti->ti_msgout = cb->msgout[0];
		ti->ti_emsgout = cb->msgout[2];
		ti->ti_omsgoutlen = cb->msgoutlen;
		bs_xfer(bsc, cb->msgout, cb->msgoutlen);
		cb->msgoutlen = 0;
	}
}

/************************************************
 * <DISCONNECT>
 ************************************************/
static BS_INLINE void
bs_disconnect_phase(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{

	switch (bsc->sc_msgphase)
	{
	default:
		panic("%s unknown msg phase\n", bsc->sc_dvname);
		break;

	case DISCONNECTASSERT:
	case FREE:
#ifdef	BS_STATICS
		bs_statics[ti->ti_id].disconnected++;
#endif	/* BS_STATICS */
		if (ti->ti_cfgflags & BS_SCSI_SAVESP)
			BS_SAVE_SDP;
		BS_HOST_TERMINATE;
		BS_SETUP_PHASE(DISCONNECTED);
		bsc->sc_dtgnum ++;
		bshoststart(bsc, NULL);
		break;

	case IOCOMPLETED:
		bs_status_check(bsc, ti);
		cb = bscmddone(ti);
#ifdef	BS_DIAG
		ti->ti_flags &= ~BSNEXUS;
#endif	/* BS_DIAG */
		BS_SETUP_PHASE(FREE);
		if (cb || bsc->sc_sttab.tqh_first == NULL)
		{
			BS_HOST_TERMINATE;
			bscmdstart(ti, BSCMDSTART);
		}
		else
		{
			/* give a chance to other target */
			bscmdstart(ti, BSCMDSTART);
			BS_HOST_TERMINATE;
			bshoststart(bsc, NULL);
		}
		break;
	}

	BS_SETUP_MSGPHASE(FREE);
}

/**************************************************
 * <PHASE ERROR>
 **************************************************/
#define	scsi_status	(bsc->sc_busstat)

struct bs_err {
	u_char	*pe_msg;
	u_int	pe_err;
	u_int	pe_ph;
};

struct bs_err bs_cmderr[] = {
/*0*/	{ "illegal cmd", BSABNORMAL, UNDEF },
/*1*/	{ "unexpected bus free", BSABNORMAL, FREE },
/*2*/	{ NULL, BSSELTIMEOUT, FREE},
/*3*/	{ "scsi bus parity error", BSPARITY, UNDEF },
/*4*/	{ "scsi bus parity error", BSPARITY, UNDEF },
/*5*/	{ "unknown" , BSFATALIO, UNDEF },
/*6*/	{ "miss reselection (target mode)", BSFATALIO, UNDEF },
/*7*/	{ "wrong status byte", BSPARITY, STATUSIN },
};

static void
bs_phase_error(ti, cb)
	struct targ_info *ti;
	struct bsccb *cb;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct bs_err *pep;

	if ((scsi_status & BSR_CM) == BSR_CMDERR &&
	    (scsi_status & BSR_PHVALID) == 0)
	{
		pep = &bs_cmderr[scsi_status & BSR_PM];
		ti->ti_error |= pep->pe_err;
		if (pep->pe_msg)
		{
			bs_debug_print(bsc, ti);
			bs_printf(ti, "bsintr", pep->pe_msg);
		}
		BS_SETUP_PHASE(pep->pe_ph);
	}
	else
	{
		ti->ti_error |= BSABNORMAL;
		bs_debug_print(bsc, ti);
		bs_printf(ti, "bsintr", "phase error");
		BS_SETUP_PHASE(UNDEF);
	}

	BS_SETUP_MSGPHASE(FREE);
	switch (ti->ti_phase)
	{
	case FREE:
		BS_SETUP_PHASE(UNDEF);
		cb = bscmddone(ti);
#ifdef	BS_DIAG
		ti->ti_flags &= ~BSNEXUS;
#endif	/* BS_DIAG */
		BS_HOST_TERMINATE;
		BS_SETUP_PHASE(FREE);
		bscmdstart(ti, ((cb == NULL) ? BSCMDSTART : BSCMDRESTART));
		break;

	case STATUSIN:
		ti->ti_error |= BSSTATUSERROR;
		ti->ti_status = bshw_get_status_insat(bsc);	/* XXX SAT */
		bs_debug_print(bsc, ti);
		break;

	case UNDEF:
	default:
		ti->ti_error |= BSABNORMAL;
		bs_reset_nexus(bsc);
		break;
	}
}

/**************************************************
 * ### SCSI PHASE SEQUENCER ###
 **************************************************/
static BS_INLINE void bs_ack_wait __P((struct bs_softc *, struct targ_info *, struct bsccb *));

static BS_INLINE void
bs_ack_wait(bsc, ti, cb)
	struct bs_softc *bsc;
	struct targ_info *ti;
	struct bsccb *cb;
{
	int wc = bsc->sc_wc;

	for (wc = bsc->sc_wc; bshw_get_busstat(bsc) != BSR_ACKREQ && wc > 0; )
		wc --;

	if (wc <= 0)
	{
		bs_printf(ti, "bs_ack_wait", "timeout I");
		return;
	}

	bshw_get_auxstat(bsc);
	scsi_status = bshw_get_busstat(bsc);

	if (cb->msgoutlen > 0)
	{
		bshw_assert_atn(bsc);
		delay(800);
		BS_SETUP_PHASE(ATTENTIONASSERT);
	}

	bshw_negate_ack(bsc);

#ifdef	WAITNEXTP
	for (wc = bsc->sc_wc; bshw_get_busstat(bsc) == BSR_ACKREQ && wc > 0; )
		wc --;

	if (wc <= 0)
		bs_printf(ti, "bs_ack_wait", "timeout II");
#endif	/* WAITNEXTP */
}

int
bs_sequencer(bsc)
	struct bs_softc *bsc;
{
	register struct targ_info *ti;
	struct bsccb *cb;

	/**************************************************
	 * Check reset
	 **************************************************/
	if (bsc->sc_flags & (BSRESET | BSINACTIVE))
	{
		if (bsc->sc_flags & BSRESET)
			bs_reset_nexus(bsc);
		return 1;
	}

	/**************************************************
	 * Get status & bus phase
	 **************************************************/
	if ((bshw_get_auxstat(bsc) & STR_INT) == 0)
		return 0;

	scsi_status = bshw_get_busstat(bsc);
	if (scsi_status == ((u_int8_t) -1))
	{
		bs_debug_print_all(bsc);
		return 1;
	}
	/**************************************************
	 * Check reselection, or nexus
	 **************************************************/
	if (scsi_status == BSR_RESEL)
	{
		bs_reselect(bsc);
		return 1;
	}

	ti = bsc->sc_nexus;
	if (ti == NULL || (cb = ti->ti_ctab.tqh_first) == NULL)
	{
		bs_debug_print_all(bsc);
		bs_printf(ti, "bsintr", "no nexus");
		bs_reset_nexus(bsc);
		return 1;
	}

	/**************************************************
	 * Debug section
	 **************************************************/
#ifdef	BS_DEBUG
	if (bs_debug_flag)
	{
		bs_debug_print(bsc, ti);
		if (bs_debug_flag > 1)
			Debugger();
	}
#endif	/* BS_DEBUG */

	/**************************************************
	 * internal scsi phase
	 **************************************************/
	switch (ti->ti_phase)
	{
	case SELECTASSERT:
		bs_selected(bsc, ti, cb);
		return 1;

	case SATSEL:
		BS_SELECTION_TERMINATE;

	case SATRESEL:
		if (bsc->sc_flags & (BSDMASTART | BSSMITSTART))
		{
			if (bsc->sc_flags & BSSMITSTART)
			{
				bs_debug_print_all(bsc);
				bs_reset_nexus(bsc);
				bs_printf(ti, "bsintr", "smit transfer");
				return 1;
			}

			BS_SETUP_PHASE(DATAPHASE);	/* XXX */
			bs_dma_xfer_end(ti);
			ti->ti_phase = ti->ti_ophase;	/* XXX */
		}
		break;

	default:
		/* XXX:
		 * check check check for safety !!
		 */
		if (bsc->sc_selwait)
		{
			/* Ghaaa! phase error! retry! */
			bs_phase_error(ti, cb);
			return 1;
		}

		if (bsc->sc_flags & (BSDMASTART | BSSMITSTART))
		{
			if (bsc->sc_flags & BSDMASTART)
				bs_dma_xfer_end(ti);
			else
				bs_smit_xfer_end(ti);
		}
		break;
	}

	/**************************************************
	 * hw scsi phase
	 **************************************************/
	if (scsi_status & BSR_PHVALID)
	{
		/**************************************************
		 * Normal SCSI phase.
		 **************************************************/
		if ((scsi_status & BSR_CM) == BSR_CMDABT)
		{
			bs_phase_error(ti, cb);
			return 1;
		}

		switch (scsi_status & BSR_PM)
		{
		case BSR_DATAOUT:
		case BSR_DATAIN:
			BS_SETUP_PHASE(DATAPHASE);

			if (bsc->sc_p.datalen <= 0 ||
			    (ti->ti_flags & BSFORCEIOPOLL))
			{
				bs_io_xfer(ti);
				return 1;
			}

			if (bs_check_smit(ti) &&
			    (bsc->sc_p.datalen % sizeof(u_int32_t)) == 0)
			{
				bs_lc_smit_xfer(ti, scsi_status & BSR_IOR);
				return 1;
			}

			bs_dma_xfer(ti, scsi_status & BSR_IOR);
			bshw_start_xfer(bsc);
			return 1;

		case BSR_CMDOUT:
			bs_commandout(bsc, ti, cb);
			return 1;

		case BSR_STATIN:
			if (bs_check_sat(ti))
			{
				BS_SETUP_PHASE(SATCOMPSEQ);
				bshw_set_count(bsc, 0);
				bshw_cmd_pass(bsc, 0x41);
				bshw_start_sat(bsc, 0);
			}
			else
			{
				BS_SETUP_PHASE(STATUSIN);
				ti->ti_status = bs_read_1byte(bsc);
			}
			return 1;

		case BSR_UNSPINFO0:
		case BSR_UNSPINFO1:
			bs_debug_print(bsc, ti);
			bs_printf(ti, "bsintr", "illegal bus phase");
			return 1;

		case BSR_MSGOUT:
			bs_msgout(bsc, ti, cb);
			return 1;

		case BSR_MSGIN:/* msg in */
			if (bs_check_sat(ti))
			{
				if (ti->ti_phase == RESELECTED)
				{
					bs_sat_continue(bsc, ti, cb);
					return 1;
				}
					/* XXX */
				if (ti->ti_status == ST_UNK)
					ti->ti_status = bshw_get_status_insat(bsc);
			}

			ti->ti_msgin[ti->ti_msginptr ++] = bs_read_1byte(bsc);
			bs_msgin(bsc, ti);
			if (bsc->sc_cfgflags & BSC_FASTACK)
				bs_ack_wait(bsc, ti, cb);

			return 1;
		}
	}
	else
	{
		/**************************************************
		 * Special SCSI phase
		 **************************************************/
		switch (scsi_status)
		{
		case BSR_SATSDP:/* SAT with save data pointer */
			BS_SAVE_SDP
			bshw_cmd_pass(bsc, 0x41);
			bshw_start_sat(bsc, 0);
			BS_SETUP_PHASE(SATSDP)
			return 1;

		case BSR_SATFIN:/* SAT COMPLETE */
			ti->ti_status = bshw_get_status_insat(bsc);
			BS_SETUP_MSGPHASE(IOCOMPLETED);
			bs_disconnect_phase(bsc, ti, cb);
			return 1;

		case BSR_ACKREQ:/* negate ACK */
			if (cb->msgoutlen > 0)
			{
				bshw_assert_atn(bsc);
				delay(800);
				BS_SETUP_PHASE(ATTENTIONASSERT);
			}
			bshw_negate_ack(bsc);
			return 1;

		case BSR_DISC:/* disconnect */
			bs_disconnect_phase(bsc, ti, cb);
			return 1;

		default:
			break;
		}
	}

	bs_phase_error(ti, cb);
	return 1;
}

/*****************************************************************
 * INTERNAL POLLING FUNCTIONS
 *****************************************************************/
static int
bs_scsi_cmd_poll_internal(cti)
	struct targ_info *cti;
{
	struct bs_softc *bsc = cti->ti_bsc;
	struct targ_info *ti;
	struct bsccb *cb;
	int i, waits, delay_count;

	bsc->sc_poll++;

	/* setup timeout count */
	if ((ti = bsc->sc_nexus) == NULL ||
	    (cb = ti->ti_ctab.tqh_first) == NULL)
		waits = BS_DEFAULT_TIMEOUT_SECOND * 1000000;
	else
		waits = cb->tcmax * 1000000;

	/* force all current jobs into the polling state. */
	for (i = 0; i < NTARGETS; i++)
	{
		if ((ti = bsc->sc_ti[i]) != NULL)
		{
			ti->ti_flags |= BSFORCEIOPOLL;
			if ((cb = ti->ti_ctab.tqh_first) != NULL)
				cb->bsccb_flags |= BSFORCEIOPOLL;
		}
	}

	/* do io */
	bsc->sc_flags &= ~BSJOBDONE;
	do
	{
		delay_count = ((bsc->sc_flags & BSDMASTART) ? 1000000 : 100);
		delay(delay_count);
		waits -= delay_count;
		bs_sequencer(bsc);
	}
	while (waits >= 0 && (bsc->sc_flags & (BSUNDERRESET | BSJOBDONE)) == 0);

	/* done */
	bsc->sc_poll--;
	if (waits < 0 || (bsc->sc_flags & BSUNDERRESET))
	{
		bs_printf(NULL, "cmd_poll", "timeout or fatal");
		return HASERROR;
	}

	return COMPLETE;
}

int
bs_scsi_cmd_poll(cti, targetcb)
	struct targ_info *cti;
	struct bsccb *targetcb;
{
	struct bs_softc *bsc = cti->ti_bsc;
	struct targ_info *ti;
	int s, error = COMPLETE;

	s = splcam();
	bs_terminate_timeout(bsc);

	if (bsc->sc_hstate == BSC_TARG_CHECK)
	{
		if ((error = bs_scsi_cmd_poll_internal(cti)) != COMPLETE)
			bs_reset_nexus(bsc);
	}
	else
	{
		if (bsc->sc_outccb)
			bs_panic(bsc, "bs_cmd_poll: internal error");

		bsc->sc_flags &= ~BSPOLLDONE;
		bsc->sc_outccb = targetcb;

		while ((bsc->sc_flags & BSPOLLDONE) == 0)
		{
			if (bs_scsi_cmd_poll_internal(cti) != COMPLETE)
			{
				if ((ti = bsc->sc_nexus) && ti->ti_ctab.tqh_first)
					ti->ti_error |= (BSTIMEOUT | BSABNORMAL);
				bs_reset_nexus(bsc);
			}
		}

		bsc->sc_outccb = NULL;
	}

	bs_start_timeout(bsc);
	softintr(bsc->sc_irq);
	splx(s);
	return error;
}
