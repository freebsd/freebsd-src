/*-
 * Generic driver for the Advanced Systems Inc. SCSI controllers
 * Product specific probe and attach routines can be found in:
 * 
 * i386/isa/adv_isa.c	ABP5140, ABP542, ABP5150, ABP842, ABP852
 * i386/eisa/adv_eisa.c	ABP742, ABP752
 * pci/adv_pci.c	ABP920, ABP930, ABP930U, ABP930UA, ABP940, ABP940U,
 *			ABP940UA, ABP950, ABP960, ABP960U, ABP960UA,
 *			ABP970, ABP970U
 *
 * Copyright (c) 1996-2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
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
/*-
 * Ported from:
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1997 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
 
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

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

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/advansys/advansys.h>

static void	adv_action(struct cam_sim *sim, union ccb *ccb);
static void	adv_execute_ccb(void *arg, bus_dma_segment_t *dm_segs,
				int nsegments, int error);
static void	adv_intr_locked(struct adv_softc *adv);
static void	adv_poll(struct cam_sim *sim);
static void	adv_run_doneq(struct adv_softc *adv);
static struct adv_ccb_info *
		adv_alloc_ccb_info(struct adv_softc *adv);
static void	adv_destroy_ccb_info(struct adv_softc *adv,
				     struct adv_ccb_info *cinfo); 
static __inline struct adv_ccb_info *
		adv_get_ccb_info(struct adv_softc *adv);
static __inline void adv_free_ccb_info(struct adv_softc *adv,
				       struct adv_ccb_info *cinfo);
static __inline void adv_set_state(struct adv_softc *adv, adv_state state);
static __inline void adv_clear_state(struct adv_softc *adv, union ccb* ccb);
static void adv_clear_state_really(struct adv_softc *adv, union ccb* ccb);

static __inline struct adv_ccb_info *
adv_get_ccb_info(struct adv_softc *adv)
{
	struct adv_ccb_info *cinfo;

	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);
	if ((cinfo = SLIST_FIRST(&adv->free_ccb_infos)) != NULL) {
		SLIST_REMOVE_HEAD(&adv->free_ccb_infos, links);
	} else {
		cinfo = adv_alloc_ccb_info(adv);
	}

	return (cinfo);
}

static __inline void
adv_free_ccb_info(struct adv_softc *adv, struct adv_ccb_info *cinfo)
{       

	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);
	cinfo->state = ACCB_FREE;
	SLIST_INSERT_HEAD(&adv->free_ccb_infos, cinfo, links);
}

static __inline void
adv_set_state(struct adv_softc *adv, adv_state state)
{
	if (adv->state == 0)
		xpt_freeze_simq(adv->sim, /*count*/1);
	adv->state |= state;
}

static __inline void
adv_clear_state(struct adv_softc *adv, union ccb* ccb)
{
	if (adv->state != 0)
		adv_clear_state_really(adv, ccb);
}

static void
adv_clear_state_really(struct adv_softc *adv, union ccb* ccb)
{

	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);
	if ((adv->state & ADV_BUSDMA_BLOCK_CLEARED) != 0)
		adv->state &= ~(ADV_BUSDMA_BLOCK_CLEARED|ADV_BUSDMA_BLOCK);
	if ((adv->state & ADV_RESOURCE_SHORTAGE) != 0) {
		int openings;

		openings = adv->max_openings - adv->cur_active - ADV_MIN_FREE_Q;
		if (openings >= adv->openings_needed) {
			adv->state &= ~ADV_RESOURCE_SHORTAGE;
			adv->openings_needed = 0;
		}
	}
		
	if ((adv->state & ADV_IN_TIMEOUT) != 0) {
		struct adv_ccb_info *cinfo;

		cinfo = (struct adv_ccb_info *)ccb->ccb_h.ccb_cinfo_ptr;
		if ((cinfo->state & ACCB_RECOVERY_CCB) != 0) {
			struct ccb_hdr *ccb_h;

			/*
			 * We now traverse our list of pending CCBs
			 * and reinstate their timeouts.
			 */
			ccb_h = LIST_FIRST(&adv->pending_ccbs);
			while (ccb_h != NULL) {
				cinfo = ccb_h->ccb_cinfo_ptr;
				callout_reset_sbt(&cinfo->timer,
				    SBT_1MS * ccb_h->timeout, 0,
				    adv_timeout, ccb_h, 0);
				ccb_h = LIST_NEXT(ccb_h, sim_links.le);
			}
			adv->state &= ~ADV_IN_TIMEOUT;
			device_printf(adv->dev, "No longer in timeout\n");
		}
	}
	if (adv->state == 0)
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
}

void     
adv_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t* physaddr;
 
	physaddr = (bus_addr_t*)arg;
	*physaddr = segs->ds_addr;
}

static void
adv_action(struct cam_sim *sim, union ccb *ccb)
{
	struct adv_softc *adv;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("adv_action\n"));

	adv = (struct adv_softc *)cam_sim_softc(sim);
	mtx_assert(&adv->lock, MA_OWNED);

	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		struct	ccb_hdr *ccb_h;
		struct	ccb_scsiio *csio;
		struct	adv_ccb_info *cinfo;
		int error;

		ccb_h = &ccb->ccb_h;
		csio = &ccb->csio;
		cinfo = adv_get_ccb_info(adv);
		if (cinfo == NULL)
			panic("XXX Handle CCB info error!!!");

		ccb_h->ccb_cinfo_ptr = cinfo;
		cinfo->ccb = ccb;

		error = bus_dmamap_load_ccb(adv->buffer_dmat,
					    cinfo->dmamap,
					    ccb,
					    adv_execute_ccb,
					    csio, /*flags*/0);
		if (error == EINPROGRESS) {
			/*
			 * So as to maintain ordering, freeze the controller
			 * queue until our mapping is returned.
			 */
			adv_set_state(adv, ADV_BUSDMA_BLOCK);
		}
		break;
	}
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_TARGET_IO:	/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
#define	IS_CURRENT_SETTINGS(c)	(c->type == CTS_TYPE_CURRENT_SETTINGS)
#define	IS_USER_SETTINGS(c)	(c->type == CTS_TYPE_USER_SETTINGS)
	case XPT_SET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_spi *spi;
		struct	 ccb_trans_settings *cts;
		target_bit_vector targ_mask;
		struct adv_transinfo *tconf;
		u_int	 update_type;

		cts = &ccb->cts;
		targ_mask = ADV_TID_TO_TARGET_MASK(cts->ccb_h.target_id);
		update_type = 0;

		/*
		 * The user must specify which type of settings he wishes
		 * to change.
		 */
		if (IS_CURRENT_SETTINGS(cts) && !IS_USER_SETTINGS(cts)) {
			tconf = &adv->tinfo[cts->ccb_h.target_id].current;
			update_type |= ADV_TRANS_GOAL;
		} else if (IS_USER_SETTINGS(cts) && !IS_CURRENT_SETTINGS(cts)) {
			tconf = &adv->tinfo[cts->ccb_h.target_id].user;
			update_type |= ADV_TRANS_USER;
		} else {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		
		scsi = &cts->proto_specific.scsi;
		spi = &cts->xport_specific.spi;
		if ((update_type & ADV_TRANS_GOAL) != 0) {
			if ((spi->valid & CTS_SPI_VALID_DISC) != 0) {
				if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) != 0)
					adv->disc_enable |= targ_mask;
				else
					adv->disc_enable &= ~targ_mask;
				adv_write_lram_8(adv, ADVV_DISC_ENABLE_B,
						 adv->disc_enable); 
			}

			if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
				if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
					adv->cmd_qng_enabled |= targ_mask;
				else
					adv->cmd_qng_enabled &= ~targ_mask;
			}
		}

		if ((update_type & ADV_TRANS_USER) != 0) {
			if ((spi->valid & CTS_SPI_VALID_DISC) != 0) {
				if ((spi->flags & CTS_SPI_VALID_DISC) != 0)
					adv->user_disc_enable |= targ_mask;
				else
					adv->user_disc_enable &= ~targ_mask;
			}

			if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
				if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
					adv->user_cmd_qng_enabled |= targ_mask;
				else
					adv->user_cmd_qng_enabled &= ~targ_mask;
			}
		}
		
		/*
		 * If the user specifies either the sync rate, or offset,
		 * but not both, the unspecified parameter defaults to its
		 * current value in transfer negotiations.
		 */
		if (((spi->valid & CTS_SPI_VALID_SYNC_RATE) != 0)
		 || ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0)) {
			/*
			 * If the user provided a sync rate but no offset,
			 * use the current offset.
			 */
			if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) == 0)
				spi->sync_offset = tconf->offset;

			/*
			 * If the user provided an offset but no sync rate,
			 * use the current sync rate.
			 */
			if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) == 0)
				spi->sync_period = tconf->period;

			adv_period_offset_to_sdtr(adv, &spi->sync_period,
						  &spi->sync_offset,
						  cts->ccb_h.target_id);
			
			adv_set_syncrate(adv, /*struct cam_path */NULL,
					 cts->ccb_h.target_id, spi->sync_period,
					 spi->sync_offset, update_type);
		}

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_spi *spi;
		struct ccb_trans_settings *cts;
		struct adv_transinfo *tconf;
		target_bit_vector target_mask;

		cts = &ccb->cts;
		target_mask = ADV_TID_TO_TARGET_MASK(cts->ccb_h.target_id);

		scsi = &cts->proto_specific.scsi;
		spi = &cts->xport_specific.spi;

		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_2;
		cts->transport = XPORT_SPI;
		cts->transport_version = 2;

		scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
		spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;

		if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			tconf = &adv->tinfo[cts->ccb_h.target_id].current;
			if ((adv->disc_enable & target_mask) != 0)
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
			if ((adv->cmd_qng_enabled & target_mask) != 0)
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
		} else {
			tconf = &adv->tinfo[cts->ccb_h.target_id].user;
			if ((adv->user_disc_enable & target_mask) != 0)
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
			if ((adv->user_cmd_qng_enabled & target_mask) != 0)
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
		}
		spi->sync_period = tconf->period;
		spi->sync_offset = tconf->offset;
		spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		spi->valid = CTS_SPI_VALID_SYNC_RATE
			   | CTS_SPI_VALID_SYNC_OFFSET
			   | CTS_SPI_VALID_BUS_WIDTH
			   | CTS_SPI_VALID_DISC;
		scsi->valid = CTS_SCSI_VALID_TQ;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		int	  extended;

		extended = (adv->control & ADV_CNTL_BIOS_GT_1GB) != 0;
		cam_calc_geometry(&ccb->ccg, extended); 
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{

		adv_stop_execution(adv);
		adv_reset_bus(adv, /*initiate_reset*/TRUE);
		adv_start_execution(adv);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7;
		cpi->max_lun = 7;
		cpi->initiator_id = adv->scsi_id;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Advansys", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
                cpi->transport = XPORT_SPI;
                cpi->transport_version = 2;
                cpi->protocol = PROTO_SCSI;
                cpi->protocol_version = SCSI_REV_2;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

/*
 * Currently, the output of bus_dmammap_load suits our needs just
 * fine, but should it change, we'd need to do something here.
 */
#define adv_fixup_dmasegs(adv, dm_segs) (struct adv_sg_entry *)(dm_segs)

static void
adv_execute_ccb(void *arg, bus_dma_segment_t *dm_segs,
		int nsegments, int error)
{
	struct	ccb_scsiio *csio;
	struct	ccb_hdr *ccb_h;
	struct	cam_sim *sim;
        struct	adv_softc *adv;
	struct	adv_ccb_info *cinfo;
	struct	adv_scsi_q scsiq;
	struct	adv_sg_head sghead;

	csio = (struct ccb_scsiio *)arg;
	ccb_h = &csio->ccb_h;
	sim = xpt_path_sim(ccb_h->path);
	adv = (struct adv_softc *)cam_sim_softc(sim);
	cinfo = (struct adv_ccb_info *)csio->ccb_h.ccb_cinfo_ptr;
	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);

	/*
	 * Setup our done routine to release the simq on
	 * the next ccb that completes.
	 */
	if ((adv->state & ADV_BUSDMA_BLOCK) != 0)
		adv->state |= ADV_BUSDMA_BLOCK_CLEARED;

	if ((ccb_h->flags & CAM_CDB_POINTER) != 0) {
		if ((ccb_h->flags & CAM_CDB_PHYS) == 0) {
			/* XXX Need phystovirt!!!! */
			/* How about pmap_kenter??? */
			scsiq.cdbptr = csio->cdb_io.cdb_ptr;
		} else {
			scsiq.cdbptr = csio->cdb_io.cdb_ptr;
		}
	} else {
		scsiq.cdbptr = csio->cdb_io.cdb_bytes;
	}
	/*
	 * Build up the request
	 */
	scsiq.q1.status = 0;
	scsiq.q1.q_no = 0;
	scsiq.q1.cntl = 0;
	scsiq.q1.sg_queue_cnt = 0;
	scsiq.q1.target_id = ADV_TID_TO_TARGET_MASK(ccb_h->target_id);
	scsiq.q1.target_lun = ccb_h->target_lun;
	scsiq.q1.sense_len = csio->sense_len;
	scsiq.q1.extra_bytes = 0;
	scsiq.q2.ccb_index = cinfo - adv->ccb_infos;
	scsiq.q2.target_ix = ADV_TIDLUN_TO_IX(ccb_h->target_id,
					      ccb_h->target_lun);
	scsiq.q2.flag = 0;
	scsiq.q2.cdb_len = csio->cdb_len;
	if ((ccb_h->flags & CAM_TAG_ACTION_VALID) != 0)
		scsiq.q2.tag_code = csio->tag_action;
	else
		scsiq.q2.tag_code = 0;
	scsiq.q2.vm_id = 0;

	if (nsegments != 0) {
		bus_dmasync_op_t op;

		scsiq.q1.data_addr = dm_segs->ds_addr;
                scsiq.q1.data_cnt = dm_segs->ds_len;
		if (nsegments > 1) {
			scsiq.q1.cntl |= QC_SG_HEAD;
			sghead.entry_cnt
			    = sghead.entry_to_copy
			    = nsegments;
			sghead.res = 0;
			sghead.sg_list = adv_fixup_dmasegs(adv, dm_segs);
			scsiq.sg_head = &sghead;
		} else {
			scsiq.sg_head = NULL;
		}
		if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;
		bus_dmamap_sync(adv->buffer_dmat, cinfo->dmamap, op);
	} else {
		scsiq.q1.data_addr = 0;	
		scsiq.q1.data_cnt = 0;
		scsiq.sg_head = NULL;
	}

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */             
	if (ccb_h->status != CAM_REQ_INPROG) {
		if (nsegments != 0)
			bus_dmamap_unload(adv->buffer_dmat, cinfo->dmamap);
		adv_clear_state(adv, (union ccb *)csio);
		adv_free_ccb_info(adv, cinfo);
		xpt_done((union ccb *)csio);
		return;
	}

	if (adv_execute_scsi_queue(adv, &scsiq, csio->dxfer_len) != 0) {
		/* Temporary resource shortage */
		adv_set_state(adv, ADV_RESOURCE_SHORTAGE);
		if (nsegments != 0)
			bus_dmamap_unload(adv->buffer_dmat, cinfo->dmamap);
		csio->ccb_h.status = CAM_REQUEUE_REQ;
		adv_clear_state(adv, (union ccb *)csio);
		adv_free_ccb_info(adv, cinfo);
		xpt_done((union ccb *)csio);
		return;
	}
	cinfo->state |= ACCB_ACTIVE;
	ccb_h->status |= CAM_SIM_QUEUED;
	LIST_INSERT_HEAD(&adv->pending_ccbs, ccb_h, sim_links.le);
	/* Schedule our timeout */
	callout_reset_sbt(&cinfo->timer, SBT_1MS * ccb_h->timeout, 0,
	    adv_timeout, csio, 0);
}

static struct adv_ccb_info *
adv_alloc_ccb_info(struct adv_softc *adv)
{
	int error;
	struct adv_ccb_info *cinfo;

	cinfo = &adv->ccb_infos[adv->ccb_infos_allocated];
	cinfo->state = ACCB_FREE;
	callout_init_mtx(&cinfo->timer, &adv->lock, 0);
	error = bus_dmamap_create(adv->buffer_dmat, /*flags*/0,
				  &cinfo->dmamap);
	if (error != 0) {
		device_printf(adv->dev, "Unable to allocate CCB info "
		    "dmamap - error %d\n", error);
		return (NULL);
	}
	adv->ccb_infos_allocated++;
	return (cinfo);
}

static void
adv_destroy_ccb_info(struct adv_softc *adv, struct adv_ccb_info *cinfo)
{

	callout_drain(&cinfo->timer);
	bus_dmamap_destroy(adv->buffer_dmat, cinfo->dmamap);
}

void
adv_timeout(void *arg)
{
	union ccb *ccb;
	struct adv_softc *adv;
	struct adv_ccb_info *cinfo, *cinfo2;

	ccb = (union ccb *)arg;
	adv = (struct adv_softc *)xpt_path_sim(ccb->ccb_h.path)->softc;
	cinfo = (struct adv_ccb_info *)ccb->ccb_h.ccb_cinfo_ptr;
	mtx_assert(&adv->lock, MA_OWNED);

	xpt_print_path(ccb->ccb_h.path);
	printf("Timed out\n");

	/* Have we been taken care of already?? */
	if (cinfo == NULL || cinfo->state == ACCB_FREE) {
		return;
	}

	adv_stop_execution(adv);

	if ((cinfo->state & ACCB_ABORT_QUEUED) == 0) {
		struct ccb_hdr *ccb_h;

		/*
		 * In order to simplify the recovery process, we ask the XPT
		 * layer to halt the queue of new transactions and we traverse
		 * the list of pending CCBs and remove their timeouts. This
		 * means that the driver attempts to clear only one error
		 * condition at a time.  In general, timeouts that occur
		 * close together are related anyway, so there is no benefit
		 * in attempting to handle errors in parallel.  Timeouts will
		 * be reinstated when the recovery process ends.
		 */
		adv_set_state(adv, ADV_IN_TIMEOUT);

		/* This CCB is the CCB representing our recovery actions */
		cinfo->state |= ACCB_RECOVERY_CCB|ACCB_ABORT_QUEUED;

		ccb_h = LIST_FIRST(&adv->pending_ccbs);
		while (ccb_h != NULL) {
			cinfo2 = ccb_h->ccb_cinfo_ptr;
			callout_stop(&cinfo2->timer);
			ccb_h = LIST_NEXT(ccb_h, sim_links.le);
		}

		/* XXX Should send a BDR */
		/* Attempt an abort as our first tact */
		xpt_print_path(ccb->ccb_h.path);
		printf("Attempting abort\n");
		adv_abort_ccb(adv, ccb->ccb_h.target_id,
			      ccb->ccb_h.target_lun, ccb,
			      CAM_CMD_TIMEOUT, /*queued_only*/FALSE);
		callout_reset(&cinfo->timer, 2 * hz, adv_timeout, ccb);
	} else {
		/* Our attempt to perform an abort failed, go for a reset */
		xpt_print_path(ccb->ccb_h.path);
		printf("Resetting bus\n");		
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
		adv_reset_bus(adv, /*initiate_reset*/TRUE);
	}
	adv_start_execution(adv);
}

struct adv_softc *
adv_alloc(device_t dev, struct resource *res, long offset)
{
	struct adv_softc *adv = device_get_softc(dev);

	/*
	 * Allocate a storage area for us
	 */
	LIST_INIT(&adv->pending_ccbs);
	SLIST_INIT(&adv->free_ccb_infos);
	adv->dev = dev;
	adv->res = res;
	adv->reg_off = offset;
	mtx_init(&adv->lock, "adv", NULL, MTX_DEF);

	return(adv);
}

void
adv_free(struct adv_softc *adv)
{
	switch (adv->init_level) {
	case 6:
	{
		struct adv_ccb_info *cinfo;

		while ((cinfo = SLIST_FIRST(&adv->free_ccb_infos)) != NULL) {
			SLIST_REMOVE_HEAD(&adv->free_ccb_infos, links);
			adv_destroy_ccb_info(adv, cinfo);	
		}
		
		bus_dmamap_unload(adv->sense_dmat, adv->sense_dmamap);
	}
	case 5:
		bus_dmamem_free(adv->sense_dmat, adv->sense_buffers,
                                adv->sense_dmamap);
	case 4:
		bus_dma_tag_destroy(adv->sense_dmat);
	case 3:
		bus_dma_tag_destroy(adv->buffer_dmat);
	case 2:
		bus_dma_tag_destroy(adv->parent_dmat);
	case 1:
		if (adv->ccb_infos != NULL)
			free(adv->ccb_infos, M_DEVBUF);
	case 0:
		mtx_destroy(&adv->lock);
		break;
	}
}

int
adv_init(struct adv_softc *adv)
{
	struct	  adv_eeprom_config eeprom_config;
	int	  checksum, i;
	int	  max_sync;
	u_int16_t config_lsw;
	u_int16_t config_msw;

	mtx_lock(&adv->lock);
	adv_lib_init(adv);

  	/*
	 * Stop script execution.
	 */  
	adv_write_lram_16(adv, ADV_HALTCODE_W, 0x00FE);
	adv_stop_execution(adv);
	if (adv_stop_chip(adv) == 0 || adv_is_chip_halted(adv) == 0) {
		mtx_unlock(&adv->lock);
		device_printf(adv->dev,
		    "Unable to halt adapter. Initialization failed\n");
		return (1);
	}
	ADV_OUTW(adv, ADV_REG_PROG_COUNTER, ADV_MCODE_START_ADDR);
	if (ADV_INW(adv, ADV_REG_PROG_COUNTER) != ADV_MCODE_START_ADDR) {
		mtx_unlock(&adv->lock);
		device_printf(adv->dev,
		    "Unable to set program counter. Initialization failed\n");
		return (1);
	}

	config_msw = ADV_INW(adv, ADV_CONFIG_MSW);
	config_lsw = ADV_INW(adv, ADV_CONFIG_LSW);

	if ((config_msw & ADV_CFG_MSW_CLR_MASK) != 0) {
		config_msw &= ~ADV_CFG_MSW_CLR_MASK;
		/*
		 * XXX The Linux code flags this as an error,
		 * but what should we report to the user???
		 * It seems that clearing the config register
		 * makes this error recoverable.
		 */
		ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw);
	}

	/* Suck in the configuration from the EEProm */
	checksum = adv_get_eeprom_config(adv, &eeprom_config);

	if (ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_AUTO_CONFIG) {
		/*
		 * XXX The Linux code sets a warning level for this
		 * condition, yet nothing of meaning is printed to
		 * the user.  What does this mean???
		 */
		if (adv->chip_version == 3) {
			if (eeprom_config.cfg_lsw != config_lsw)
				eeprom_config.cfg_lsw = config_lsw;
			if (eeprom_config.cfg_msw != config_msw) {
				eeprom_config.cfg_msw = config_msw;
			}
		}
	}
	if (checksum == eeprom_config.chksum) {

		/* Range/Sanity checking */
		if (eeprom_config.max_total_qng < ADV_MIN_TOTAL_QNG) {
			eeprom_config.max_total_qng = ADV_MIN_TOTAL_QNG;
		}
		if (eeprom_config.max_total_qng > ADV_MAX_TOTAL_QNG) {
			eeprom_config.max_total_qng = ADV_MAX_TOTAL_QNG;
		}
		if (eeprom_config.max_tag_qng > eeprom_config.max_total_qng) {
			eeprom_config.max_tag_qng = eeprom_config.max_total_qng;
		}
		if (eeprom_config.max_tag_qng < ADV_MIN_TAG_Q_PER_DVC) {
			eeprom_config.max_tag_qng = ADV_MIN_TAG_Q_PER_DVC;
		}
		adv->max_openings = eeprom_config.max_total_qng;
		adv->user_disc_enable = eeprom_config.disc_enable;
		adv->user_cmd_qng_enabled = eeprom_config.use_cmd_qng;
		adv->isa_dma_speed = EEPROM_DMA_SPEED(eeprom_config);
		adv->scsi_id = EEPROM_SCSIID(eeprom_config) & ADV_MAX_TID;
		EEPROM_SET_SCSIID(eeprom_config, adv->scsi_id);
		adv->control = eeprom_config.cntl;
		for (i = 0; i <= ADV_MAX_TID; i++) {
			u_int8_t sync_data;

			if ((eeprom_config.init_sdtr & (0x1 << i)) == 0)
				sync_data = 0;
			else
				sync_data = eeprom_config.sdtr_data[i];
			adv_sdtr_to_period_offset(adv,
						  sync_data,
						  &adv->tinfo[i].user.period,
						  &adv->tinfo[i].user.offset,
						  i);
		}
		config_lsw = eeprom_config.cfg_lsw;
		eeprom_config.cfg_msw = config_msw;
	} else {
		u_int8_t sync_data;

		device_printf(adv->dev, "Warning EEPROM Checksum mismatch. "
		       "Using default device parameters\n");

		/* Set reasonable defaults since we can't read the EEPROM */
		adv->isa_dma_speed = /*ADV_DEF_ISA_DMA_SPEED*/1;
		adv->max_openings = ADV_DEF_MAX_TOTAL_QNG;
		adv->disc_enable = TARGET_BIT_VECTOR_SET;
		adv->user_disc_enable = TARGET_BIT_VECTOR_SET;
		adv->cmd_qng_enabled = TARGET_BIT_VECTOR_SET;
		adv->user_cmd_qng_enabled = TARGET_BIT_VECTOR_SET;
		adv->scsi_id = 7;
		adv->control = 0xFFFF;

		if (adv->chip_version == ADV_CHIP_VER_PCI_ULTRA_3050)
			/* Default to no Ultra to support the 3030 */
			adv->control &= ~ADV_CNTL_SDTR_ENABLE_ULTRA;
		sync_data = ADV_DEF_SDTR_OFFSET | (ADV_DEF_SDTR_INDEX << 4);
		for (i = 0; i <= ADV_MAX_TID; i++) {
			adv_sdtr_to_period_offset(adv, sync_data,
						  &adv->tinfo[i].user.period,
						  &adv->tinfo[i].user.offset,
						  i);
		}
		config_lsw |= ADV_CFG_LSW_SCSI_PARITY_ON;
	}
	config_msw &= ~ADV_CFG_MSW_CLR_MASK;
	config_lsw |= ADV_CFG_LSW_HOST_INT_ON;
	if ((adv->type & (ADV_PCI|ADV_ULTRA)) == (ADV_PCI|ADV_ULTRA)
	 && (adv->control & ADV_CNTL_SDTR_ENABLE_ULTRA) == 0)
		/* 25ns or 10MHz */
		max_sync = 25;
	else
		/* Unlimited */
		max_sync = 0;
	for (i = 0; i <= ADV_MAX_TID; i++) {
		if (adv->tinfo[i].user.period < max_sync)
			adv->tinfo[i].user.period = max_sync;
	}

	if (adv_test_external_lram(adv) == 0) {
		if ((adv->type & (ADV_PCI|ADV_ULTRA)) == (ADV_PCI|ADV_ULTRA)) {
			eeprom_config.max_total_qng =
			    ADV_MAX_PCI_ULTRA_INRAM_TOTAL_QNG;
			eeprom_config.max_tag_qng =
			    ADV_MAX_PCI_ULTRA_INRAM_TAG_QNG;
		} else {
			eeprom_config.cfg_msw |= 0x0800;
			config_msw |= 0x0800;
			eeprom_config.max_total_qng =
			     ADV_MAX_PCI_INRAM_TOTAL_QNG;
			eeprom_config.max_tag_qng = ADV_MAX_INRAM_TAG_QNG;
		}
		adv->max_openings = eeprom_config.max_total_qng;
	}
	ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw);
	ADV_OUTW(adv, ADV_CONFIG_LSW, config_lsw);
#if 0
	/*
	 * Don't write the eeprom data back for now.
	 * I'd rather not mess up the user's card.  We also don't
	 * fully sanitize the eeprom settings above for the write-back
	 * to be 100% correct.
	 */
	if (adv_set_eeprom_config(adv, &eeprom_config) != 0)
		device_printf(adv->dev,
		    "WARNING! Failure writing to EEPROM.\n");
#endif

	adv_set_chip_scsiid(adv, adv->scsi_id);
	if (adv_init_lram_and_mcode(adv)) {
		mtx_unlock(&adv->lock);
		return (1);
	}

	adv->disc_enable = adv->user_disc_enable;

	adv_write_lram_8(adv, ADVV_DISC_ENABLE_B, adv->disc_enable); 
	for (i = 0; i <= ADV_MAX_TID; i++) {
		/*
		 * Start off in async mode.
		 */
		adv_set_syncrate(adv, /*struct cam_path */NULL,
				 i, /*period*/0, /*offset*/0,
				 ADV_TRANS_CUR);
		/*
		 * Enable the use of tagged commands on all targets.
		 * This allows the kernel driver to make up it's own mind
		 * as it sees fit to tag queue instead of having the
		 * firmware try and second guess the tag_code settins.
		 */
		adv_write_lram_8(adv, ADVV_MAX_DVC_QNG_BEG + i,
				 adv->max_openings);
	}
	adv_write_lram_8(adv, ADVV_USE_TAGGED_QNG_B, TARGET_BIT_VECTOR_SET);
	adv_write_lram_8(adv, ADVV_CAN_TAGGED_QNG_B, TARGET_BIT_VECTOR_SET);
	device_printf(adv->dev,
	    "AdvanSys %s Host Adapter, SCSI ID %d, queue depth %d\n",
	    (adv->type & ADV_ULTRA) && (max_sync == 0)
	    ? "Ultra SCSI" : "SCSI",
	    adv->scsi_id, adv->max_openings);
	mtx_unlock(&adv->lock);
	return (0);
}

void
adv_intr(void *arg)
{
	struct	  adv_softc *adv;

	adv = arg;
	mtx_lock(&adv->lock);
	adv_intr_locked(adv);
	mtx_unlock(&adv->lock);
}

void
adv_intr_locked(struct adv_softc *adv)
{
	u_int16_t chipstat;
	u_int16_t saved_ram_addr;
	u_int8_t  ctrl_reg;
	u_int8_t  saved_ctrl_reg;
	u_int8_t  host_flag;

	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);
	chipstat = ADV_INW(adv, ADV_CHIP_STATUS);

	/* Is it for us? */
	if ((chipstat & (ADV_CSW_INT_PENDING|ADV_CSW_SCSI_RESET_LATCH)) == 0)
		return;

	ctrl_reg = ADV_INB(adv, ADV_CHIP_CTRL);
	saved_ctrl_reg = ctrl_reg & (~(ADV_CC_SCSI_RESET | ADV_CC_CHIP_RESET |
				       ADV_CC_SINGLE_STEP | ADV_CC_DIAG |
				       ADV_CC_TEST));

	if ((chipstat & (ADV_CSW_SCSI_RESET_LATCH|ADV_CSW_SCSI_RESET_ACTIVE))) {
		device_printf(adv->dev, "Detected Bus Reset\n");
		adv_reset_bus(adv, /*initiate_reset*/FALSE);
		return;
	}

	if ((chipstat & ADV_CSW_INT_PENDING) != 0) {
		
		saved_ram_addr = ADV_INW(adv, ADV_LRAM_ADDR);
		host_flag = adv_read_lram_8(adv, ADVV_HOST_FLAG_B);
		adv_write_lram_8(adv, ADVV_HOST_FLAG_B,
				 host_flag | ADV_HOST_FLAG_IN_ISR);

		adv_ack_interrupt(adv);
		
		if ((chipstat & ADV_CSW_HALTED) != 0
		 && (ctrl_reg & ADV_CC_SINGLE_STEP) != 0) {
			adv_isr_chip_halted(adv);
			saved_ctrl_reg &= ~ADV_CC_HALT;
		} else {
			adv_run_doneq(adv);
		}
		ADV_OUTW(adv, ADV_LRAM_ADDR, saved_ram_addr);
#ifdef DIAGNOSTIC	
		if (ADV_INW(adv, ADV_LRAM_ADDR) != saved_ram_addr)
			panic("adv_intr: Unable to set LRAM addr");
#endif	
		adv_write_lram_8(adv, ADVV_HOST_FLAG_B, host_flag);
	}
	
	ADV_OUTB(adv, ADV_CHIP_CTRL, saved_ctrl_reg);
}

static void
adv_run_doneq(struct adv_softc *adv)
{
	struct adv_q_done_info scsiq;
	u_int		  doneq_head;
	u_int		  done_qno;

	doneq_head = adv_read_lram_16(adv, ADVV_DONE_Q_TAIL_W) & 0xFF;
	done_qno = adv_read_lram_8(adv, ADV_QNO_TO_QADDR(doneq_head)
				   + ADV_SCSIQ_B_FWD);
	while (done_qno != ADV_QLINK_END) {
		union ccb* ccb;
		struct adv_ccb_info *cinfo;
		u_int done_qaddr;
		u_int sg_queue_cnt;

		done_qaddr = ADV_QNO_TO_QADDR(done_qno);

		/* Pull status from this request */
		sg_queue_cnt = adv_copy_lram_doneq(adv, done_qaddr, &scsiq,
						   adv->max_dma_count);

		/* Mark it as free */
		adv_write_lram_8(adv, done_qaddr + ADV_SCSIQ_B_STATUS,
				 scsiq.q_status & ~(QS_READY|QS_ABORTED));

		/* Process request based on retrieved info */
		if ((scsiq.cntl & QC_SG_HEAD) != 0) {
			u_int i;

			/*
			 * S/G based request.  Free all of the queue
			 * structures that contained S/G information.
			 */
			for (i = 0; i < sg_queue_cnt; i++) {
				done_qno = adv_read_lram_8(adv, done_qaddr
							   + ADV_SCSIQ_B_FWD);

#ifdef DIAGNOSTIC				
				if (done_qno == ADV_QLINK_END) {
					panic("adv_qdone: Corrupted SG "
					      "list encountered");
				}
#endif				
				done_qaddr = ADV_QNO_TO_QADDR(done_qno);

				/* Mark SG queue as free */
				adv_write_lram_8(adv, done_qaddr
						 + ADV_SCSIQ_B_STATUS, QS_FREE);
			}
		} else 
			sg_queue_cnt = 0;
#ifdef DIAGNOSTIC
		if (adv->cur_active < (sg_queue_cnt + 1))
			panic("adv_qdone: Attempting to free more "
			      "queues than are active");
#endif		
		adv->cur_active -= sg_queue_cnt + 1;

		if ((scsiq.q_status != QS_DONE)
		 && (scsiq.q_status & QS_ABORTED) == 0)
			panic("adv_qdone: completed scsiq with unknown status");

		scsiq.remain_bytes += scsiq.extra_bytes;
			
		if ((scsiq.d3.done_stat == QD_WITH_ERROR) &&
		    (scsiq.d3.host_stat == QHSTA_M_DATA_OVER_RUN)) {
			if ((scsiq.cntl & (QC_DATA_IN|QC_DATA_OUT)) == 0) {
				scsiq.d3.done_stat = QD_NO_ERROR;
				scsiq.d3.host_stat = QHSTA_NO_ERROR;
			}
		}

		cinfo = &adv->ccb_infos[scsiq.d2.ccb_index];
		ccb = cinfo->ccb;
		ccb->csio.resid = scsiq.remain_bytes;
		adv_done(adv, ccb,
			 scsiq.d3.done_stat, scsiq.d3.host_stat,
			 scsiq.d3.scsi_stat, scsiq.q_no);

		doneq_head = done_qno;
		done_qno = adv_read_lram_8(adv, done_qaddr + ADV_SCSIQ_B_FWD);
	}
	adv_write_lram_16(adv, ADVV_DONE_Q_TAIL_W, doneq_head);
}


void
adv_done(struct adv_softc *adv, union ccb *ccb, u_int done_stat,
	 u_int host_stat, u_int scsi_status, u_int q_no)
{
	struct	   adv_ccb_info *cinfo;

	if (!dumping)
		mtx_assert(&adv->lock, MA_OWNED);
	cinfo = (struct adv_ccb_info *)ccb->ccb_h.ccb_cinfo_ptr;
	LIST_REMOVE(&ccb->ccb_h, sim_links.le);
	callout_stop(&cinfo->timer);
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(adv->buffer_dmat, cinfo->dmamap, op);
		bus_dmamap_unload(adv->buffer_dmat, cinfo->dmamap);
	}

	switch (done_stat) {
	case QD_NO_ERROR:
		if (host_stat == QHSTA_NO_ERROR) {
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
		xpt_print_path(ccb->ccb_h.path);
		printf("adv_done - queue done without error, "
		       "but host status non-zero(%x)\n", host_stat);
		/*FALLTHROUGH*/
	case QD_WITH_ERROR:
		switch (host_stat) {
		case QHSTA_M_TARGET_STATUS_BUSY:
		case QHSTA_M_BAD_QUEUE_FULL_OR_BUSY:
			/*
			 * Assume that if we were a tagged transaction
			 * the target reported queue full.  Otherwise,
			 * report busy.  The firmware really should just
			 * pass the original status back up to us even
			 * if it thinks the target was in error for
			 * returning this status as no other transactions
			 * from this initiator are in effect, but this
			 * ignores multi-initiator setups and there is
			 * evidence that the firmware gets its per-device
			 * transaction counts screwed up occasionally.
			 */
			ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0
			 && host_stat != QHSTA_M_TARGET_STATUS_BUSY)
				scsi_status = SCSI_STATUS_QUEUE_FULL;
			else
				scsi_status = SCSI_STATUS_BUSY;
			adv_abort_ccb(adv, ccb->ccb_h.target_id,
				      ccb->ccb_h.target_lun,
				      /*ccb*/NULL, CAM_REQUEUE_REQ,
				      /*queued_only*/TRUE);
			/*FALLTHROUGH*/
		case QHSTA_M_NO_AUTO_REQ_SENSE:
		case QHSTA_NO_ERROR:
			ccb->csio.scsi_status = scsi_status;
			switch (scsi_status) {
			case SCSI_STATUS_CHECK_COND:
			case SCSI_STATUS_CMD_TERMINATED:
				ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
				/* Structure copy */
				ccb->csio.sense_data =
				    adv->sense_buffers[q_no - 1];
				/* FALLTHROUGH */
			case SCSI_STATUS_BUSY:
			case SCSI_STATUS_RESERV_CONFLICT:
			case SCSI_STATUS_QUEUE_FULL:
			case SCSI_STATUS_COND_MET:
			case SCSI_STATUS_INTERMED:
			case SCSI_STATUS_INTERMED_COND_MET:
				ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
				break;
			case SCSI_STATUS_OK:
				ccb->ccb_h.status |= CAM_REQ_CMP;
				break;
			}
			break;
		case QHSTA_M_SEL_TIMEOUT:
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			break;
		case QHSTA_M_DATA_OVER_RUN:
			ccb->ccb_h.status = CAM_DATA_RUN_ERR;
			break;
		case QHSTA_M_UNEXPECTED_BUS_FREE:
			ccb->ccb_h.status = CAM_UNEXP_BUSFREE;
			break;
		case QHSTA_M_BAD_BUS_PHASE_SEQ:
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			break;
		case QHSTA_M_BAD_CMPL_STATUS_IN:
			/* No command complete after a status message */
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			break;
		case QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT:
		case QHSTA_M_WTM_TIMEOUT:
		case QHSTA_M_HUNG_REQ_SCSI_BUS_RESET:
			/* The SCSI bus hung in a phase */
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			adv_reset_bus(adv, /*initiate_reset*/TRUE);
			break;
		case QHSTA_M_AUTO_REQ_SENSE_FAIL:
			ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
			break;
		case QHSTA_D_QDONE_SG_LIST_CORRUPTED:
		case QHSTA_D_ASC_DVC_ERROR_CODE_SET:
		case QHSTA_D_HOST_ABORT_FAILED:
		case QHSTA_D_EXE_SCSI_Q_FAILED:
		case QHSTA_D_ASPI_NO_BUF_POOL:
		case QHSTA_M_BAD_TAG_CODE:
		case QHSTA_D_LRAM_CMP_ERROR:
		case QHSTA_M_MICRO_CODE_ERROR_HALT:
		default:
			panic("%s: Unhandled Host status error %x",
			    device_get_nameunit(adv->dev), host_stat);
			/* NOTREACHED */
		}
		break;

	case QD_ABORTED_BY_HOST:
		/* Don't clobber any, more explicit, error codes we've set */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG)
			ccb->ccb_h.status = CAM_REQ_ABORTED;
		break;

	default:
		xpt_print_path(ccb->ccb_h.path);
		printf("adv_done - queue done with unknown status %x:%x\n",
		       done_stat, host_stat);
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		break;
	}
	adv_clear_state(adv, ccb);
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP
	 && (ccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
	}
	adv_free_ccb_info(adv, cinfo);
	/*
	 * Null this out so that we catch driver bugs that cause a
	 * ccb to be completed twice.
	 */
	ccb->ccb_h.ccb_cinfo_ptr = NULL;
	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	xpt_done(ccb);
}

/*
 * Function to poll for command completion when
 * interrupts are disabled (crash dumps)
 */
static void
adv_poll(struct cam_sim *sim)
{

	adv_intr_locked(cam_sim_softc(sim));
}

/*
 * Attach all the sub-devices we can find
 */
int
adv_attach(adv)
	struct adv_softc *adv;
{
	struct ccb_setasync csa;
	struct cam_devq *devq;
	int max_sg;

	/*
	 * Allocate an array of ccb mapping structures.  We put the
	 * index of the ccb_info structure into the queue representing
	 * a transaction and use it for mapping the queue to the
	 * upper level SCSI transaction it represents.
	 */
	adv->ccb_infos = malloc(sizeof(*adv->ccb_infos) * adv->max_openings,
				M_DEVBUF, M_NOWAIT);

	if (adv->ccb_infos == NULL)
		return (ENOMEM);

	adv->init_level++;
		
	/*
	 * Create our DMA tags.  These tags define the kinds of device
	 * accessible memory allocations and memory mappings we will 
	 * need to perform during normal operation.
	 *
	 * Unless we need to further restrict the allocation, we rely
	 * on the restrictions of the parent dmat, hence the common
	 * use of MAXADDR and MAXSIZE.
	 *
	 * The ASC boards use chains of "queues" (the transactional
	 * resources on the board) to represent long S/G lists.
	 * The first queue represents the command and holds a
	 * single address and data pair.  The queues that follow
	 * can each hold ADV_SG_LIST_PER_Q entries.  Given the
	 * total number of queues, we can express the largest
	 * transaction we can map.  We reserve a few queues for
	 * error recovery.  Take those into account as well.
	 *
	 * There is a way to take an interrupt to download the
	 * next batch of S/G entries if there are more than 255
	 * of them (the counter in the queue structure is a u_int8_t).
	 * We don't use this feature, so limit the S/G list size
	 * accordingly.
	 */
	max_sg = (adv->max_openings - ADV_MIN_FREE_Q - 1) * ADV_SG_LIST_PER_Q;
	if (max_sg > 255)
		max_sg = 255;

	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(
			/* parent	*/ adv->parent_dmat,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ ADV_MAXPHYS,
			/* nsegments	*/ max_sg,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ BUS_DMA_ALLOCNOW,
			/* lockfunc	*/ busdma_lock_mutex,
			/* lockarg	*/ &adv->lock,
			&adv->buffer_dmat) != 0) {
		return (ENXIO);
	}
	adv->init_level++;

	/* DMA tag for our sense buffers */
	if (bus_dma_tag_create(
			/* parent	*/ adv->parent_dmat,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ sizeof(struct scsi_sense_data) *
					   adv->max_openings,
			/* nsegments	*/ 1,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ 0,
			/* lockfunc	*/ busdma_lock_mutex,
			/* lockarg	*/ &adv->lock,
			&adv->sense_dmat) != 0) {
		return (ENXIO);
        }

	adv->init_level++;

	/* Allocation for our sense buffers */
	if (bus_dmamem_alloc(adv->sense_dmat, (void **)&adv->sense_buffers,
			     BUS_DMA_NOWAIT, &adv->sense_dmamap) != 0) {
		return (ENOMEM);
	}

	adv->init_level++;

	/* And permanently map them */
	bus_dmamap_load(adv->sense_dmat, adv->sense_dmamap,
       			adv->sense_buffers,
			sizeof(struct scsi_sense_data)*adv->max_openings,
			adv_map, &adv->sense_physbase, /*flags*/0);

	adv->init_level++;

	/*
	 * Fire up the chip
	 */
	if (adv_start_chip(adv) != 1) {
		device_printf(adv->dev,
		    "Unable to start on board processor. Aborting.\n");
		return (ENXIO);
	}

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(adv->max_openings);
	if (devq == NULL)
		return (ENOMEM);

	/*
	 * Construct our SIM entry.
	 */
	adv->sim = cam_sim_alloc(adv_action, adv_poll, "adv", adv,
	    device_get_unit(adv->dev), &adv->lock, 1, adv->max_openings, devq);
	if (adv->sim == NULL)
		return (ENOMEM);

	/*
	 * Register the bus.
	 *
	 * XXX Twin Channel EISA Cards???
	 */
	mtx_lock(&adv->lock);
	if (xpt_bus_register(adv->sim, adv->dev, 0) != CAM_SUCCESS) {
		cam_sim_free(adv->sim, /*free devq*/TRUE);
		mtx_unlock(&adv->lock);
		return (ENXIO);
	}

	if (xpt_create_path(&adv->path, /*periph*/NULL, cam_sim_path(adv->sim),
			    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD)
	    != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(adv->sim));
		cam_sim_free(adv->sim, /*free devq*/TRUE);
		mtx_unlock(&adv->lock);
		return (ENXIO);
	}

	xpt_setup_ccb(&csa.ccb_h, adv->path, /*priority*/5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_FOUND_DEVICE|AC_LOST_DEVICE;
	csa.callback = advasync;
	csa.callback_arg = adv;
	xpt_action((union ccb *)&csa);
	mtx_unlock(&adv->lock);
	return (0);
}
MODULE_DEPEND(adv, cam, 1, 1, 1);
