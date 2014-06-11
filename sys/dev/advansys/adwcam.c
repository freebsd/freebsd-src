/*-
 * CAM SCSI interface for the Advanced Systems Inc.
 * Second Generation SCSI controllers.
 *
 * Product specific probe and attach routines can be found in:
 * 
 * adw_pci.c	ABP[3]940UW, ABP950UW, ABP3940U2W
 *
 * Copyright (c) 1998, 1999, 2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
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
/*
 * Ported from:
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1998 Advanced System Products, Inc.
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_message.h>

#include <dev/advansys/adwvar.h>

/* Definitions for our use of the SIM private CCB area */
#define ccb_acb_ptr spriv_ptr0
#define ccb_adw_ptr spriv_ptr1

static __inline struct acb*	adwgetacb(struct adw_softc *adw);
static __inline void		adwfreeacb(struct adw_softc *adw,
					   struct acb *acb);

static void		adwmapmem(void *arg, bus_dma_segment_t *segs,
				  int nseg, int error);
static struct sg_map_node*
			adwallocsgmap(struct adw_softc *adw);
static int		adwallocacbs(struct adw_softc *adw);

static void		adwexecuteacb(void *arg, bus_dma_segment_t *dm_segs,
				      int nseg, int error);
static void		adw_action(struct cam_sim *sim, union ccb *ccb);
static void		adw_intr_locked(struct adw_softc *adw);
static void		adw_poll(struct cam_sim *sim);
static void		adw_async(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static void		adwprocesserror(struct adw_softc *adw, struct acb *acb);
static void		adwtimeout(void *arg);
static void		adw_handle_device_reset(struct adw_softc *adw,
						u_int target);
static void		adw_handle_bus_reset(struct adw_softc *adw,
					     int initiated);

static __inline struct acb*
adwgetacb(struct adw_softc *adw)
{
	struct	acb* acb;

	if (!dumping)
		mtx_assert(&adw->lock, MA_OWNED);
	if ((acb = SLIST_FIRST(&adw->free_acb_list)) != NULL) {
		SLIST_REMOVE_HEAD(&adw->free_acb_list, links);
	} else if (adw->num_acbs < adw->max_acbs) {
		adwallocacbs(adw);
		acb = SLIST_FIRST(&adw->free_acb_list);
		if (acb == NULL)
			device_printf(adw->device, "Can't malloc ACB\n");
		else {
			SLIST_REMOVE_HEAD(&adw->free_acb_list, links);
		}
	}

	return (acb);
}

static __inline void
adwfreeacb(struct adw_softc *adw, struct acb *acb)
{

	if (!dumping)
		mtx_assert(&adw->lock, MA_OWNED);
	if ((acb->state & ACB_ACTIVE) != 0)
		LIST_REMOVE(&acb->ccb->ccb_h, sim_links.le);
	if ((acb->state & ACB_RELEASE_SIMQ) != 0)
		acb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
	else if ((adw->state & ADW_RESOURCE_SHORTAGE) != 0
	      && (acb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		acb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		adw->state &= ~ADW_RESOURCE_SHORTAGE;
	}
	acb->state = ACB_FREE;
	SLIST_INSERT_HEAD(&adw->free_acb_list, acb, links);
}

static void
adwmapmem(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *busaddrp;

	busaddrp = (bus_addr_t *)arg;
	*busaddrp = segs->ds_addr;
}

static struct sg_map_node *
adwallocsgmap(struct adw_softc *adw)
{
	struct sg_map_node *sg_map;

	sg_map = malloc(sizeof(*sg_map), M_DEVBUF, M_NOWAIT);

	if (sg_map == NULL)
		return (NULL);

	/* Allocate S/G space for the next batch of ACBS */
	if (bus_dmamem_alloc(adw->sg_dmat, (void **)&sg_map->sg_vaddr,
			     BUS_DMA_NOWAIT, &sg_map->sg_dmamap) != 0) {
		free(sg_map, M_DEVBUF);
		return (NULL);
	}

	SLIST_INSERT_HEAD(&adw->sg_maps, sg_map, links);

	bus_dmamap_load(adw->sg_dmat, sg_map->sg_dmamap, sg_map->sg_vaddr,
			PAGE_SIZE, adwmapmem, &sg_map->sg_physaddr, /*flags*/0);

	bzero(sg_map->sg_vaddr, PAGE_SIZE);
	return (sg_map);
}

/*
 * Allocate another chunk of CCB's. Return count of entries added.
 */
static int
adwallocacbs(struct adw_softc *adw)
{
	struct acb *next_acb;
	struct sg_map_node *sg_map;
	bus_addr_t busaddr;
	struct adw_sg_block *blocks;
	int newcount;
	int i;

	next_acb = &adw->acbs[adw->num_acbs];
	sg_map = adwallocsgmap(adw);

	if (sg_map == NULL)
		return (0);

	blocks = sg_map->sg_vaddr;
	busaddr = sg_map->sg_physaddr;

	newcount = (PAGE_SIZE / (ADW_SG_BLOCKCNT * sizeof(*blocks)));
	for (i = 0; adw->num_acbs < adw->max_acbs && i < newcount; i++) {
		int error;

		error = bus_dmamap_create(adw->buffer_dmat, /*flags*/0,
					  &next_acb->dmamap);
		if (error != 0)
			break;
		next_acb->queue.scsi_req_baddr = acbvtob(adw, next_acb);
		next_acb->queue.scsi_req_bo = acbvtobo(adw, next_acb);
		next_acb->queue.sense_baddr =
		    acbvtob(adw, next_acb) + offsetof(struct acb, sense_data);
		next_acb->sg_blocks = blocks;
		next_acb->sg_busaddr = busaddr;
		next_acb->state = ACB_FREE;
		callout_init_mtx(&next_acb->timer, &adw->lock, 0);
		SLIST_INSERT_HEAD(&adw->free_acb_list, next_acb, links);
		blocks += ADW_SG_BLOCKCNT;
		busaddr += ADW_SG_BLOCKCNT * sizeof(*blocks);
		next_acb++;
		adw->num_acbs++;
	}
	return (i);
}

static void
adwexecuteacb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct	 acb *acb;
	union	 ccb *ccb;
	struct	 adw_softc *adw;

	acb = (struct acb *)arg;
	ccb = acb->ccb;
	adw = (struct adw_softc *)ccb->ccb_h.ccb_adw_ptr;

	if (!dumping)
		mtx_assert(&adw->lock, MA_OWNED);
	if (error != 0) {
		if (error != EFBIG)
			device_printf(adw->device, "Unexepected error 0x%x "
			    "returned from bus_dmamap_load\n", error);
		if (ccb->ccb_h.status == CAM_REQ_INPROG) {
			xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
			ccb->ccb_h.status = CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		adwfreeacb(adw, acb);
		xpt_done(ccb);
		return;
	}
		
	if (nseg != 0) {
		bus_dmasync_op_t op;

		acb->queue.data_addr = dm_segs[0].ds_addr;
		acb->queue.data_cnt = ccb->csio.dxfer_len;
		if (nseg > 1) {
			struct adw_sg_block *sg_block;
			struct adw_sg_elm *sg;
			bus_addr_t sg_busaddr;
			u_int sg_index;
			bus_dma_segment_t *end_seg;

			end_seg = dm_segs + nseg;

			sg_busaddr = acb->sg_busaddr;
			sg_index = 0;
			/* Copy the segments into our SG list */
			for (sg_block = acb->sg_blocks;; sg_block++) {
				u_int i;

				sg = sg_block->sg_list;
				for (i = 0; i < ADW_NO_OF_SG_PER_BLOCK; i++) {
					if (dm_segs >= end_seg)
						break;
				    
					sg->sg_addr = dm_segs->ds_addr;
					sg->sg_count = dm_segs->ds_len;
					sg++;
					dm_segs++;
				}
				sg_block->sg_cnt = i;
				sg_index += i;
				if (dm_segs == end_seg) {
					sg_block->sg_busaddr_next = 0;
					break;
				} else {
					sg_busaddr +=
					    sizeof(struct adw_sg_block);
					sg_block->sg_busaddr_next = sg_busaddr;
				}
			}
			acb->queue.sg_real_addr = acb->sg_busaddr;
		} else {
			acb->queue.sg_real_addr = 0;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(adw->buffer_dmat, acb->dmamap, op);

	} else {
		acb->queue.data_addr = 0;
		acb->queue.data_cnt = 0;
		acb->queue.sg_real_addr = 0;
	}

	/*
	 * Last time we need to check if this CCB needs to
	 * be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(adw->buffer_dmat, acb->dmamap);
		adwfreeacb(adw, acb);
		xpt_done(ccb);
		return;
	}

	acb->state |= ACB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	LIST_INSERT_HEAD(&adw->pending_ccbs, &ccb->ccb_h, sim_links.le);
	callout_reset(&acb->timer, (ccb->ccb_h.timeout * hz) / 1000,
	    adwtimeout, acb);

	adw_send_acb(adw, acb, acbvtob(adw, acb));
}

static void
adw_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	adw_softc *adw;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("adw_action\n"));
	
	adw = (struct adw_softc *)cam_sim_softc(sim);
	if (!dumping)
		mtx_assert(&adw->lock, MA_OWNED);

	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		struct	ccb_scsiio *csio;
		struct	acb *acb;
		int error;

		csio = &ccb->csio;

		/* Max supported CDB length is 12 bytes */
		if (csio->cdb_len > 12) { 
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}

		if ((acb = adwgetacb(adw)) == NULL) {
			adw->state |= ADW_RESOURCE_SHORTAGE;
			xpt_freeze_simq(sim, /*count*/1);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			return;
		}

		/* Link acb and ccb so we can find one from the other */
		acb->ccb = ccb;
		ccb->ccb_h.ccb_acb_ptr = acb;
		ccb->ccb_h.ccb_adw_ptr = adw;

		acb->queue.cntl = 0;
		acb->queue.target_cmd = 0;
		acb->queue.target_id = ccb->ccb_h.target_id;
		acb->queue.target_lun = ccb->ccb_h.target_lun;

		acb->queue.mflag = 0;
		acb->queue.sense_len =
			MIN(csio->sense_len, sizeof(acb->sense_data));
		acb->queue.cdb_len = csio->cdb_len;
		if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0) {
			switch (csio->tag_action) {
			case MSG_SIMPLE_Q_TAG:
				acb->queue.scsi_cntl = ADW_QSC_SIMPLE_Q_TAG;
				break;
			case MSG_HEAD_OF_Q_TAG:
				acb->queue.scsi_cntl = ADW_QSC_HEAD_OF_Q_TAG;
				break;
			case MSG_ORDERED_Q_TAG:
				acb->queue.scsi_cntl = ADW_QSC_ORDERED_Q_TAG;
				break;
			default:
				acb->queue.scsi_cntl = ADW_QSC_NO_TAGMSG;
				break;
			}
		} else
			acb->queue.scsi_cntl = ADW_QSC_NO_TAGMSG;

		if ((ccb->ccb_h.flags & CAM_DIS_DISCONNECT) != 0)
			acb->queue.scsi_cntl |= ADW_QSC_NO_DISC;

		acb->queue.done_status = 0;
		acb->queue.scsi_status = 0;
		acb->queue.host_status = 0;
		acb->queue.sg_wk_ix = 0;
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) == 0) {
				bcopy(csio->cdb_io.cdb_ptr,
				      acb->queue.cdb, csio->cdb_len);
			} else {
				/* I guess I could map it in... */
				ccb->ccb_h.status = CAM_REQ_INVALID;
				adwfreeacb(adw, acb);
				xpt_done(ccb);
				return;
			}
		} else {
			bcopy(csio->cdb_io.cdb_bytes,
			      acb->queue.cdb, csio->cdb_len);
		}

		error = bus_dmamap_load_ccb(adw->buffer_dmat,
					    acb->dmamap,
					    ccb,
					    adwexecuteacb,
					    acb, /*flags*/0);
		if (error == EINPROGRESS) {
			/*
			 * So as to maintain ordering, freeze the controller
			 * queue until our mapping is returned.
			 */
			xpt_freeze_simq(sim, 1);
			acb->state |= CAM_RELEASE_SIMQ;
		}
		break;
	}
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	{
		adw_idle_cmd_status_t status;

		status = adw_idle_cmd_send(adw, ADW_IDLE_CMD_DEVICE_RESET,
					   ccb->ccb_h.target_id);
		if (status == ADW_IDLE_CMD_SUCCESS) {
			ccb->ccb_h.status = CAM_REQ_CMP;
			if (bootverbose) {
				xpt_print_path(ccb->ccb_h.path);
				printf("BDR Delivered\n");
			}
		} else
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_spi *spi;
		struct	  ccb_trans_settings *cts;
		u_int	  target_mask;

		cts = &ccb->cts;
		target_mask = 0x01 << ccb->ccb_h.target_id;

		scsi = &cts->proto_specific.scsi;
		spi = &cts->xport_specific.spi;
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			u_int sdtrdone;

			sdtrdone = adw_lram_read_16(adw, ADW_MC_SDTR_DONE);
			if ((spi->valid & CTS_SPI_VALID_DISC) != 0) {
				u_int discenb;

				discenb =
				    adw_lram_read_16(adw, ADW_MC_DISC_ENABLE);

				if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) != 0)
					discenb |= target_mask;
				else
					discenb &= ~target_mask;

				adw_lram_write_16(adw, ADW_MC_DISC_ENABLE,
						  discenb);
			}
		
			if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {

				if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
					adw->tagenb |= target_mask;
				else
					adw->tagenb &= ~target_mask;
			}	

			if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0) {
				u_int wdtrenb_orig;
				u_int wdtrenb;
				u_int wdtrdone;

				wdtrenb_orig =
				    adw_lram_read_16(adw, ADW_MC_WDTR_ABLE);
				wdtrenb = wdtrenb_orig;
				wdtrdone = adw_lram_read_16(adw,
							    ADW_MC_WDTR_DONE);
				switch (spi->bus_width) {
				case MSG_EXT_WDTR_BUS_32_BIT:
				case MSG_EXT_WDTR_BUS_16_BIT:
					wdtrenb |= target_mask;
					break;
				case MSG_EXT_WDTR_BUS_8_BIT:
				default:
					wdtrenb &= ~target_mask;
					break;
				}
				if (wdtrenb != wdtrenb_orig) {
					adw_lram_write_16(adw,
							  ADW_MC_WDTR_ABLE,
							  wdtrenb);
					wdtrdone &= ~target_mask;
					adw_lram_write_16(adw,
							  ADW_MC_WDTR_DONE,
							  wdtrdone);
					/* Wide negotiation forces async */
					sdtrdone &= ~target_mask;
					adw_lram_write_16(adw,
							  ADW_MC_SDTR_DONE,
							  sdtrdone);
				}
			}

			if (((spi->valid & CTS_SPI_VALID_SYNC_RATE) != 0)
			 || ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0)) {
				u_int sdtr_orig;
				u_int sdtr;
				u_int sdtrable_orig;
				u_int sdtrable;

				sdtr = adw_get_chip_sdtr(adw,
							 ccb->ccb_h.target_id);
				sdtr_orig = sdtr;
				sdtrable = adw_lram_read_16(adw,
							    ADW_MC_SDTR_ABLE);
				sdtrable_orig = sdtrable;

				if ((spi->valid
				   & CTS_SPI_VALID_SYNC_RATE) != 0) {

					sdtr =
					    adw_find_sdtr(adw,
							  spi->sync_period);
				}
					
				if ((spi->valid
				   & CTS_SPI_VALID_SYNC_OFFSET) != 0) {
					if (spi->sync_offset == 0)
						sdtr = ADW_MC_SDTR_ASYNC;
				}

				if (sdtr == ADW_MC_SDTR_ASYNC)
					sdtrable &= ~target_mask;
				else
					sdtrable |= target_mask;
				if (sdtr != sdtr_orig
				 || sdtrable != sdtrable_orig) {
					adw_set_chip_sdtr(adw,
							  ccb->ccb_h.target_id,
							  sdtr);
					sdtrdone &= ~target_mask;
					adw_lram_write_16(adw, ADW_MC_SDTR_ABLE,
							  sdtrable);
					adw_lram_write_16(adw, ADW_MC_SDTR_DONE,
							  sdtrdone);
					
				}
			} 
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
		struct	ccb_trans_settings *cts;
		u_int	target_mask;
 
		cts = &ccb->cts;
		target_mask = 0x01 << ccb->ccb_h.target_id;
		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_2;
		cts->transport = XPORT_SPI;
		cts->transport_version = 2;

		scsi = &cts->proto_specific.scsi;
		spi = &cts->xport_specific.spi;
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			u_int mc_sdtr;

			spi->flags = 0;
			if ((adw->user_discenb & target_mask) != 0)
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;

			if ((adw->user_tagenb & target_mask) != 0)
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;

			if ((adw->user_wdtr & target_mask) != 0)
				spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			else
				spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;

			mc_sdtr = adw_get_user_sdtr(adw, ccb->ccb_h.target_id);
			spi->sync_period = adw_find_period(adw, mc_sdtr);
			if (spi->sync_period != 0)
				spi->sync_offset = 15; /* XXX ??? */
			else
				spi->sync_offset = 0;


		} else {
			u_int targ_tinfo;

			spi->flags = 0;
			if ((adw_lram_read_16(adw, ADW_MC_DISC_ENABLE)
			  & target_mask) != 0)
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;

			if ((adw->tagenb & target_mask) != 0)
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;

			targ_tinfo =
			    adw_lram_read_16(adw,
					     ADW_MC_DEVICE_HSHK_CFG_TABLE
					     + (2 * ccb->ccb_h.target_id));

			if ((targ_tinfo & ADW_HSHK_CFG_WIDE_XFR) != 0)
				spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			else
				spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;

			spi->sync_period =
			    adw_hshk_cfg_period_factor(targ_tinfo);

			spi->sync_offset = targ_tinfo & ADW_HSHK_CFG_OFFSET;
			if (spi->sync_period == 0)
				spi->sync_offset = 0;

			if (spi->sync_offset == 0)
				spi->sync_period = 0;
		}

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
		/*
		 * XXX Use Adaptec translation until I find out how to
		 *     get this information from the card.
		 */
		cam_calc_geometry(&ccb->ccg, /*extended*/1); 
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{
		int failure;

		failure = adw_reset_bus(adw);
		if (failure != 0) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			if (bootverbose) {
				xpt_print_path(adw->path);
				printf("Bus Reset Delivered\n");
			}
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
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
		
		cpi->version_num = 1;
		cpi->hba_inquiry = PI_WIDE_16|PI_SDTR_ABLE|PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = ADW_MAX_TID;
		cpi->max_lun = ADW_MAX_LUN;
		cpi->initiator_id = adw->initiator_id;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "AdvanSys", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
                cpi->transport = XPORT_SPI;
                cpi->transport_version = 2;
                cpi->protocol = PROTO_SCSI;
                cpi->protocol_version = SCSI_REV_2;
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

static void
adw_poll(struct cam_sim *sim)
{
	adw_intr_locked(cam_sim_softc(sim));
}

static void
adw_async(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
}

struct adw_softc *
adw_alloc(device_t dev, struct resource *regs, int regs_type, int regs_id)
{
	struct	 adw_softc *adw;

	adw = device_get_softc(dev);
	LIST_INIT(&adw->pending_ccbs);
	SLIST_INIT(&adw->sg_maps);
	mtx_init(&adw->lock, "adw", NULL, MTX_DEF);
	adw->device = dev;
	adw->regs_res_type = regs_type;
	adw->regs_res_id = regs_id;
	adw->regs = regs;
	return(adw);
}

void
adw_free(struct adw_softc *adw)
{
	switch (adw->init_level) {
	case 9:
	{
		struct sg_map_node *sg_map;

		while ((sg_map = SLIST_FIRST(&adw->sg_maps)) != NULL) {
			SLIST_REMOVE_HEAD(&adw->sg_maps, links);
			bus_dmamap_unload(adw->sg_dmat,
					  sg_map->sg_dmamap);
			bus_dmamem_free(adw->sg_dmat, sg_map->sg_vaddr,
					sg_map->sg_dmamap);
			free(sg_map, M_DEVBUF);
		}
		bus_dma_tag_destroy(adw->sg_dmat);
	}
	case 8:
		bus_dmamap_unload(adw->acb_dmat, adw->acb_dmamap);
	case 7:
		bus_dmamem_free(adw->acb_dmat, adw->acbs,
				adw->acb_dmamap);
	case 6:
		bus_dma_tag_destroy(adw->acb_dmat);
	case 5:
		bus_dmamap_unload(adw->carrier_dmat, adw->carrier_dmamap);
	case 4:
		bus_dmamem_free(adw->carrier_dmat, adw->carriers,
				adw->carrier_dmamap);
	case 3:
		bus_dma_tag_destroy(adw->carrier_dmat);
	case 2:
		bus_dma_tag_destroy(adw->buffer_dmat);
	case 1:
		bus_dma_tag_destroy(adw->parent_dmat);
	case 0:
		break;
	}
	
	if (adw->regs != NULL)
		bus_release_resource(adw->device,
				     adw->regs_res_type,
				     adw->regs_res_id,
				     adw->regs);

	if (adw->irq != NULL)
		bus_release_resource(adw->device,
				     adw->irq_res_type,
				     0, adw->irq);

	if (adw->sim != NULL) {
		if (adw->path != NULL) {
			xpt_async(AC_LOST_DEVICE, adw->path, NULL);
			xpt_free_path(adw->path);
		}
		xpt_bus_deregister(cam_sim_path(adw->sim));
		cam_sim_free(adw->sim, /*free_devq*/TRUE);
	}
	mtx_destroy(&adw->lock);
}

int
adw_init(struct adw_softc *adw)
{
	struct	  adw_eeprom eep_config;
	u_int	  tid;
	u_int	  i;
	u_int16_t checksum;
	u_int16_t scsicfg1;

	checksum = adw_eeprom_read(adw, &eep_config);
	bcopy(eep_config.serial_number, adw->serial_number,
	      sizeof(adw->serial_number));
	if (checksum != eep_config.checksum) {
		u_int16_t serial_number[3];

		adw->flags |= ADW_EEPROM_FAILED;
		device_printf(adw->device,
		    "EEPROM checksum failed.  Restoring Defaults\n");

	        /*
		 * Restore the default EEPROM settings.
		 * Assume the 6 byte board serial number that was read
		 * from EEPROM is correct even if the EEPROM checksum
		 * failed.
		 */
		bcopy(adw->default_eeprom, &eep_config, sizeof(eep_config));
		bcopy(adw->serial_number, eep_config.serial_number,
		      sizeof(serial_number));
		adw_eeprom_write(adw, &eep_config);
	}

	/* Pull eeprom information into our softc. */
	adw->bios_ctrl = eep_config.bios_ctrl;
	adw->user_wdtr = eep_config.wdtr_able;
	for (tid = 0; tid < ADW_MAX_TID; tid++) {
		u_int	  mc_sdtr;
		u_int16_t tid_mask;

		tid_mask = 0x1 << tid;
		if ((adw->features & ADW_ULTRA) != 0) {
			/*
			 * Ultra chips store sdtr and ultraenb
			 * bits in their seeprom, so we must
			 * construct valid mc_sdtr entries for
			 * indirectly.
			 */
			if (eep_config.sync1.sync_enable & tid_mask) {
				if (eep_config.sync2.ultra_enable & tid_mask)
					mc_sdtr = ADW_MC_SDTR_20;
				else
					mc_sdtr = ADW_MC_SDTR_10;
			} else
				mc_sdtr = ADW_MC_SDTR_ASYNC;
		} else {
			switch (ADW_TARGET_GROUP(tid)) {
			case 3:
				mc_sdtr = eep_config.sync4.sdtr4;
				break;
			case 2:
				mc_sdtr = eep_config.sync3.sdtr3;
				break;
			case 1:
				mc_sdtr = eep_config.sync2.sdtr2;
				break;
			default: /* Shut up compiler */
			case 0:
				mc_sdtr = eep_config.sync1.sdtr1;
				break;
			}
			mc_sdtr >>= ADW_TARGET_GROUP_SHIFT(tid);
			mc_sdtr &= 0xFF;
		}
		adw_set_user_sdtr(adw, tid, mc_sdtr);
	}
	adw->user_tagenb = eep_config.tagqng_able;
	adw->user_discenb = eep_config.disc_enable;
	adw->max_acbs = eep_config.max_host_qng;
	adw->initiator_id = (eep_config.adapter_scsi_id & ADW_MAX_TID);

	/*
	 * Sanity check the number of host openings.
	 */
	if (adw->max_acbs > ADW_DEF_MAX_HOST_QNG)
		adw->max_acbs = ADW_DEF_MAX_HOST_QNG;
	else if (adw->max_acbs < ADW_DEF_MIN_HOST_QNG) {
        	/* If the value is zero, assume it is uninitialized. */
		if (adw->max_acbs == 0)
			adw->max_acbs = ADW_DEF_MAX_HOST_QNG;
		else
			adw->max_acbs = ADW_DEF_MIN_HOST_QNG;
	}
	
	scsicfg1 = 0;
	if ((adw->features & ADW_ULTRA2) != 0) {
		switch (eep_config.termination_lvd) {
		default:
			device_printf(adw->device,
			    "Invalid EEPROM LVD Termination Settings.\n");
			device_printf(adw->device,
			    "Reverting to Automatic LVD Termination\n");
			/* FALLTHROUGH */
		case ADW_EEPROM_TERM_AUTO:
			break;
		case ADW_EEPROM_TERM_BOTH_ON:
			scsicfg1 |= ADW2_SCSI_CFG1_TERM_LVD_LO;
			/* FALLTHROUGH */
		case ADW_EEPROM_TERM_HIGH_ON:
			scsicfg1 |= ADW2_SCSI_CFG1_TERM_LVD_HI;
			/* FALLTHROUGH */
		case ADW_EEPROM_TERM_OFF:
			scsicfg1 |= ADW2_SCSI_CFG1_DIS_TERM_DRV;
			break;
		}
	}

	switch (eep_config.termination_se) {
	default:
		device_printf(adw->device,
		    "Invalid SE EEPROM Termination Settings.\n");
		device_printf(adw->device,
		    "Reverting to Automatic SE Termination\n");
		/* FALLTHROUGH */
	case ADW_EEPROM_TERM_AUTO:
		break;
	case ADW_EEPROM_TERM_BOTH_ON:
		scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_L;
		/* FALLTHROUGH */
	case ADW_EEPROM_TERM_HIGH_ON:
		scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_H;
		/* FALLTHROUGH */
	case ADW_EEPROM_TERM_OFF:
		scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_MANUAL;
		break;
	}
	device_printf(adw->device, "SCSI ID %d, ", adw->initiator_id);

	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(
			/* parent	*/ adw->parent_dmat,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR_32BIT,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ MAXBSIZE,
			/* nsegments	*/ ADW_SGSIZE,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ BUS_DMA_ALLOCNOW,
			/* lockfunc	*/ busdma_lock_mutex,
			/* lockarg	*/ &adw->lock,
			&adw->buffer_dmat) != 0) {
		return (ENOMEM);
	}

	adw->init_level++;

	/* DMA tag for our ccb carrier structures */
	if (bus_dma_tag_create(
			/* parent	*/ adw->parent_dmat,
			/* alignment	*/ 0x10,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR_32BIT,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ (adw->max_acbs +
					    ADW_NUM_CARRIER_QUEUES + 1) *
					    sizeof(struct adw_carrier),
			/* nsegments	*/ 1,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ 0,
			/* lockfunc	*/ NULL,
			/* lockarg	*/ NULL,
			&adw->carrier_dmat) != 0) {
		return (ENOMEM);
        }

	adw->init_level++;

	/* Allocation for our ccb carrier structures */
	if (bus_dmamem_alloc(adw->carrier_dmat, (void **)&adw->carriers,
			     BUS_DMA_NOWAIT, &adw->carrier_dmamap) != 0) {
		return (ENOMEM);
	}

	adw->init_level++;

	/* And permanently map them */
	bus_dmamap_load(adw->carrier_dmat, adw->carrier_dmamap,
			adw->carriers,
			(adw->max_acbs + ADW_NUM_CARRIER_QUEUES + 1)
			 * sizeof(struct adw_carrier),
			adwmapmem, &adw->carrier_busbase, /*flags*/0);

	/* Clear them out. */
	bzero(adw->carriers, (adw->max_acbs + ADW_NUM_CARRIER_QUEUES + 1)
			     * sizeof(struct adw_carrier));

	/* Setup our free carrier list */
	adw->free_carriers = adw->carriers;
	for (i = 0; i < adw->max_acbs + ADW_NUM_CARRIER_QUEUES; i++) {
		adw->carriers[i].carr_offset =
			carriervtobo(adw, &adw->carriers[i]);
		adw->carriers[i].carr_ba = 
			carriervtob(adw, &adw->carriers[i]);
		adw->carriers[i].areq_ba = 0;
		adw->carriers[i].next_ba = 
			carriervtobo(adw, &adw->carriers[i+1]);
	}
	/* Terminal carrier.  Never leaves the freelist */
	adw->carriers[i].carr_offset =
		carriervtobo(adw, &adw->carriers[i]);
	adw->carriers[i].carr_ba = 
		carriervtob(adw, &adw->carriers[i]);
	adw->carriers[i].areq_ba = 0;
	adw->carriers[i].next_ba = ~0;

	adw->init_level++;

	/* DMA tag for our acb structures */
	if (bus_dma_tag_create(
			/* parent	*/ adw->parent_dmat,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ adw->max_acbs * sizeof(struct acb),
			/* nsegments	*/ 1,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ 0,
			/* lockfunc	*/ NULL,
			/* lockarg	*/ NULL,
			&adw->acb_dmat) != 0) {
		return (ENOMEM);
        }

	adw->init_level++;

	/* Allocation for our ccbs */
	if (bus_dmamem_alloc(adw->acb_dmat, (void **)&adw->acbs,
			     BUS_DMA_NOWAIT, &adw->acb_dmamap) != 0)
		return (ENOMEM);

	adw->init_level++;

	/* And permanently map them */
	bus_dmamap_load(adw->acb_dmat, adw->acb_dmamap,
			adw->acbs,
			adw->max_acbs * sizeof(struct acb),
			adwmapmem, &adw->acb_busbase, /*flags*/0);

	/* Clear them out. */
	bzero(adw->acbs, adw->max_acbs * sizeof(struct acb)); 

	/* DMA tag for our S/G structures.  We allocate in page sized chunks */
	if (bus_dma_tag_create(
			/* parent	*/ adw->parent_dmat,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ BUS_SPACE_MAXADDR,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ PAGE_SIZE,
			/* nsegments	*/ 1,
			/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* flags	*/ 0,
			/* lockfunc	*/ NULL,
			/* lockarg	*/ NULL,
			&adw->sg_dmat) != 0) {
		return (ENOMEM);
        }

	adw->init_level++;

	/* Allocate our first batch of ccbs */
	mtx_lock(&adw->lock);
	if (adwallocacbs(adw) == 0) {
		mtx_unlock(&adw->lock);
		return (ENOMEM);
	}

	if (adw_init_chip(adw, scsicfg1) != 0) {
		mtx_unlock(&adw->lock);
		return (ENXIO);
	}

	printf("Queue Depth %d\n", adw->max_acbs);
	mtx_unlock(&adw->lock);

	return (0);
}

/*
 * Attach all the sub-devices we can find
 */
int
adw_attach(struct adw_softc *adw)
{
	struct ccb_setasync csa;
	struct cam_devq *devq;
	int error;

	/* Hook up our interrupt handler */
	error = bus_setup_intr(adw->device, adw->irq,
	    INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE, NULL, adw_intr, adw,
	    &adw->ih);
	if (error != 0) {				    
		device_printf(adw->device, "bus_setup_intr() failed: %d\n",
			      error);
		return (error);
	}

	/* Start the Risc processor now that we are fully configured. */
	adw_outw(adw, ADW_RISC_CSR, ADW_RISC_CSR_RUN);

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(adw->max_acbs);
	if (devq == NULL)
		return (ENOMEM);

	/*
	 * Construct our SIM entry.
	 */
	adw->sim = cam_sim_alloc(adw_action, adw_poll, "adw", adw,
	    device_get_unit(adw->device), &adw->lock, 1, adw->max_acbs, devq);
	if (adw->sim == NULL)
		return (ENOMEM);

	/*
	 * Register the bus.
	 */
	mtx_lock(&adw->lock);
	if (xpt_bus_register(adw->sim, adw->device, 0) != CAM_SUCCESS) {
		cam_sim_free(adw->sim, /*free devq*/TRUE);
		error = ENOMEM;
		goto fail;
	}

	if (xpt_create_path(&adw->path, /*periph*/NULL, cam_sim_path(adw->sim),
			    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD)
	   == CAM_REQ_CMP) {
		xpt_setup_ccb(&csa.ccb_h, adw->path, /*priority*/5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_LOST_DEVICE;
		csa.callback = adw_async;
		csa.callback_arg = adw;
		xpt_action((union ccb *)&csa);
	}

fail:
	mtx_unlock(&adw->lock);
	return (error);
}

void
adw_intr(void *arg)
{
	struct	adw_softc *adw;

	adw = arg;
	mtx_lock(&adw->lock);
	adw_intr_locked(adw);
	mtx_unlock(&adw->lock);
}

void
adw_intr_locked(struct adw_softc *adw)
{
	u_int	int_stat;
	
	if ((adw_inw(adw, ADW_CTRL_REG) & ADW_CTRL_REG_HOST_INTR) == 0)
		return;

	/* Reading the register clears the interrupt. */
	int_stat = adw_inb(adw, ADW_INTR_STATUS_REG);

	if ((int_stat & ADW_INTR_STATUS_INTRB) != 0) {
		u_int intrb_code;

		/* Async Microcode Event */
		intrb_code = adw_lram_read_8(adw, ADW_MC_INTRB_CODE);
		switch (intrb_code) {
		case ADW_ASYNC_CARRIER_READY_FAILURE:
			/*
			 * The RISC missed our update of
			 * the commandq.
			 */
			if (LIST_FIRST(&adw->pending_ccbs) != NULL)
				adw_tickle_risc(adw, ADW_TICKLE_A);
			break;
    		case ADW_ASYNC_SCSI_BUS_RESET_DET:
			/*
			 * The firmware detected a SCSI Bus reset.
			 */
			device_printf(adw->device, "Someone Reset the Bus\n");
			adw_handle_bus_reset(adw, /*initiated*/FALSE);
			break;
		case ADW_ASYNC_RDMA_FAILURE:
			/*
			 * Handle RDMA failure by resetting the
			 * SCSI Bus and chip.
			 */
#if 0 /* XXX */
			AdvResetChipAndSB(adv_dvc_varp);
#endif
			break;

		case ADW_ASYNC_HOST_SCSI_BUS_RESET:
			/*
			 * Host generated SCSI bus reset occurred.
			 */
			adw_handle_bus_reset(adw, /*initiated*/TRUE);
        		break;
    		default:
			printf("adw_intr: unknown async code 0x%x\n",
			       intrb_code);
			break;
		}
	}

	/*
	 * Run down the RequestQ.
	 */
	while ((adw->responseq->next_ba & ADW_RQ_DONE) != 0) {
		struct adw_carrier *free_carrier;
		struct acb *acb;
		union ccb *ccb;

#if 0
		printf("0x%x, 0x%x, 0x%x, 0x%x\n",
		       adw->responseq->carr_offset,
		       adw->responseq->carr_ba,
		       adw->responseq->areq_ba,
		       adw->responseq->next_ba);
#endif
		/*
		 * The firmware copies the adw_scsi_req_q.acb_baddr
		 * field into the areq_ba field of the carrier.
		 */
		acb = acbbotov(adw, adw->responseq->areq_ba);

		/*
		 * The least significant four bits of the next_ba
		 * field are used as flags.  Mask them out and then
		 * advance through the list.
		 */
		free_carrier = adw->responseq;
		adw->responseq =
		    carrierbotov(adw, free_carrier->next_ba & ADW_NEXT_BA_MASK);
		free_carrier->next_ba = adw->free_carriers->carr_offset;
		adw->free_carriers = free_carrier;

		/* Process CCB */
		ccb = acb->ccb;
		callout_stop(&acb->timer);
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			bus_dmasync_op_t op;

			if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
				op = BUS_DMASYNC_POSTREAD;
			else
				op = BUS_DMASYNC_POSTWRITE;
			bus_dmamap_sync(adw->buffer_dmat, acb->dmamap, op);
			bus_dmamap_unload(adw->buffer_dmat, acb->dmamap);
			ccb->csio.resid = acb->queue.data_cnt;
		} else 
			ccb->csio.resid = 0;

		/* Common Cases inline... */
		if (acb->queue.host_status == QHSTA_NO_ERROR
		 && (acb->queue.done_status == QD_NO_ERROR
		  || acb->queue.done_status == QD_WITH_ERROR)) {
			ccb->csio.scsi_status = acb->queue.scsi_status;
			ccb->ccb_h.status = 0;
			switch (ccb->csio.scsi_status) {
			case SCSI_STATUS_OK:
				ccb->ccb_h.status |= CAM_REQ_CMP;
				break;
			case SCSI_STATUS_CHECK_COND:
			case SCSI_STATUS_CMD_TERMINATED:
				bcopy(&acb->sense_data, &ccb->csio.sense_data,
				      ccb->csio.sense_len);
				ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
				ccb->csio.sense_resid = acb->queue.sense_len;
				/* FALLTHROUGH */
			default:
				ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR
						  |  CAM_DEV_QFRZN;
				xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
				break;
			}
			adwfreeacb(adw, acb);
			xpt_done(ccb);
		} else {
			adwprocesserror(adw, acb);
		}
	}
}

static void
adwprocesserror(struct adw_softc *adw, struct acb *acb)
{
	union ccb *ccb;

	ccb = acb->ccb;
	if (acb->queue.done_status == QD_ABORTED_BY_HOST) {
		ccb->ccb_h.status = CAM_REQ_ABORTED;
	} else {

		switch (acb->queue.host_status) {
		case QHSTA_M_SEL_TIMEOUT:
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			break;
		case QHSTA_M_SXFR_OFF_UFLW:
		case QHSTA_M_SXFR_OFF_OFLW:
		case QHSTA_M_DATA_OVER_RUN:
			ccb->ccb_h.status = CAM_DATA_RUN_ERR;
			break;
		case QHSTA_M_SXFR_DESELECTED:
		case QHSTA_M_UNEXPECTED_BUS_FREE:
			ccb->ccb_h.status = CAM_UNEXP_BUSFREE;
			break;
		case QHSTA_M_SCSI_BUS_RESET:
		case QHSTA_M_SCSI_BUS_RESET_UNSOL:
			ccb->ccb_h.status = CAM_SCSI_BUS_RESET;
			break;
		case QHSTA_M_BUS_DEVICE_RESET:
			ccb->ccb_h.status = CAM_BDR_SENT;
			break;
		case QHSTA_M_QUEUE_ABORTED:
			/* BDR or Bus Reset */
			xpt_print_path(adw->path);
			printf("Saw Queue Aborted\n");
			ccb->ccb_h.status = adw->last_reset;
			break;
		case QHSTA_M_SXFR_SDMA_ERR:
		case QHSTA_M_SXFR_SXFR_PERR:
		case QHSTA_M_RDMA_PERR:
			ccb->ccb_h.status = CAM_UNCOR_PARITY;
			break;
		case QHSTA_M_WTM_TIMEOUT:
		case QHSTA_M_SXFR_WD_TMO:
		{
			/* The SCSI bus hung in a phase */
			xpt_print_path(adw->path);
			printf("Watch Dog timer expired.  Resetting bus\n");
			adw_reset_bus(adw);
			break;
		}
		case QHSTA_M_SXFR_XFR_PH_ERR:
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			break;
		case QHSTA_M_SXFR_UNKNOWN_ERROR:
			break;
		case QHSTA_M_BAD_CMPL_STATUS_IN:
			/* No command complete after a status message */
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			break;
		case QHSTA_M_AUTO_REQ_SENSE_FAIL:
			ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
			break;
		case QHSTA_M_INVALID_DEVICE:
			ccb->ccb_h.status = CAM_PATH_INVALID;
			break;
		case QHSTA_M_NO_AUTO_REQ_SENSE:
			/*
			 * User didn't request sense, but we got a
			 * check condition.
			 */
			ccb->csio.scsi_status = acb->queue.scsi_status;
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			break;
		default:
			panic("%s: Unhandled Host status error %x",
			    device_get_nameunit(adw->device),
			    acb->queue.host_status);
			/* NOTREACHED */
		}
	}
	if ((acb->state & ACB_RECOVERY_ACB) != 0) {
		if (ccb->ccb_h.status == CAM_SCSI_BUS_RESET
		 || ccb->ccb_h.status == CAM_BDR_SENT)
		 	ccb->ccb_h.status = CAM_CMD_TIMEOUT;
	}
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
	}
	adwfreeacb(adw, acb);
	xpt_done(ccb);
}

static void
adwtimeout(void *arg)
{
	struct acb	     *acb;
	union  ccb	     *ccb;
	struct adw_softc     *adw;
	adw_idle_cmd_status_t status;
	int		      target_id;

	acb = (struct acb *)arg;
	ccb = acb->ccb;
	adw = (struct adw_softc *)ccb->ccb_h.ccb_adw_ptr;
	xpt_print_path(ccb->ccb_h.path);
	printf("ACB %p - timed out\n", (void *)acb);

	mtx_assert(&adw->lock, MA_OWNED);

	if ((acb->state & ACB_ACTIVE) == 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("ACB %p - timed out CCB already completed\n",
		       (void *)acb);
		return;
	}

	acb->state |= ACB_RECOVERY_ACB;
	target_id = ccb->ccb_h.target_id;

	/* Attempt a BDR first */
	status = adw_idle_cmd_send(adw, ADW_IDLE_CMD_DEVICE_RESET,
				   ccb->ccb_h.target_id);
	if (status == ADW_IDLE_CMD_SUCCESS) {
		device_printf(adw->device,
		    "BDR Delivered.  No longer in timeout\n");
		adw_handle_device_reset(adw, target_id);
	} else {
		adw_reset_bus(adw);
		xpt_print_path(adw->path);
		printf("Bus Reset Delivered.  No longer in timeout\n");
	}
}

static void
adw_handle_device_reset(struct adw_softc *adw, u_int target)
{
	struct cam_path *path;
	cam_status error;

	error = xpt_create_path(&path, /*periph*/NULL, cam_sim_path(adw->sim),
				target, CAM_LUN_WILDCARD);

	if (error == CAM_REQ_CMP) {
		xpt_async(AC_SENT_BDR, path, NULL);
		xpt_free_path(path);
	}
	adw->last_reset = CAM_BDR_SENT;
}

static void
adw_handle_bus_reset(struct adw_softc *adw, int initiated)
{
	if (initiated) {
		/*
		 * The microcode currently sets the SCSI Bus Reset signal
		 * while handling the AscSendIdleCmd() IDLE_CMD_SCSI_RESET
		 * command above.  But the SCSI Bus Reset Hold Time in the
		 * microcode is not deterministic (it may in fact be for less
		 * than the SCSI Spec. minimum of 25 us).  Therefore on return
		 * the Adv Library sets the SCSI Bus Reset signal for
		 * ADW_SCSI_RESET_HOLD_TIME_US, which is defined to be greater
		 * than 25 us.
		 */
		u_int scsi_ctrl;

	    	scsi_ctrl = adw_inw(adw, ADW_SCSI_CTRL) & ~ADW_SCSI_CTRL_RSTOUT;
		adw_outw(adw, ADW_SCSI_CTRL, scsi_ctrl | ADW_SCSI_CTRL_RSTOUT);
		DELAY(ADW_SCSI_RESET_HOLD_TIME_US);
		adw_outw(adw, ADW_SCSI_CTRL, scsi_ctrl);

		/*
		 * We will perform the async notification when the
		 * SCSI Reset interrupt occurs.
		 */
	} else
		xpt_async(AC_BUS_RESET, adw->path, NULL);
	adw->last_reset = CAM_SCSI_BUS_RESET;
}
MODULE_DEPEND(adw, cam, 1, 1, 1);

