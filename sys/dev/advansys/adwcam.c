/*
 * CAM SCSI interface for the the Advanced Systems Inc.
 * Second Generation SCSI controllers.
 *
 * Product specific probe and attach routines can be found in:
 * 
 * pci/adw_pci.c	ABP940UW
 *
 * Copyright (c) 1998 Justin Gibbs.
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
 *
 *      $Id: adwcam.c,v 1.2 1998/10/15 23:47:14 gibbs Exp $
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
#include <stddef.h>	/* For offsetof */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/clock.h>

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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

u_long adw_unit;

static __inline u_int32_t	acbvtop(struct adw_softc *adw,
					   struct acb *acb);
static __inline struct acb *	acbptov(struct adw_softc *adw,
					u_int32_t busaddr);
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
static void		adw_poll(struct cam_sim *sim);
static void		adw_async(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static void		adwprocesserror(struct adw_softc *adw, struct acb *acb);
static void		adwtimeout(void *arg);
static void		adw_handle_device_reset(struct adw_softc *adw,
						u_int target);
static void		adw_handle_bus_reset(struct adw_softc *adw,
					     int initiated);

static __inline u_int32_t
acbvtop(struct adw_softc *adw, struct acb *acb)
{
	return (adw->acb_busbase
	      + (u_int32_t)((caddr_t)acb - (caddr_t)adw->acbs));
}

static __inline struct acb *
acbptov(struct adw_softc *adw, u_int32_t busaddr)
{
	return (adw->acbs
	      + ((struct acb *)busaddr - (struct acb *)adw->acb_busbase));
}

static __inline struct acb*
adwgetacb(struct adw_softc *adw)
{
	struct	acb* acb;
	int	s;

	s = splcam();
	if ((acb = SLIST_FIRST(&adw->free_acb_list)) != NULL) {
		SLIST_REMOVE_HEAD(&adw->free_acb_list, links);
	} else if (adw->num_acbs < adw->max_acbs) {
		adwallocacbs(adw);
		acb = SLIST_FIRST(&adw->free_acb_list);
		if (acb == NULL)
			printf("%s: Can't malloc ACB\n", adw_name(adw));
		else {
			SLIST_REMOVE_HEAD(&adw->free_acb_list, links);
		}
	}
	splx(s);

	return (acb);
}

static __inline void
adwfreeacb(struct adw_softc *adw, struct acb *acb)
{
	int s;

	s = splcam();
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
	splx(s);
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
 * Assumed to be called at splcam().
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
		int j;

		error = bus_dmamap_create(adw->buffer_dmat, /*flags*/0,
					  &next_acb->dmamap);
		if (error != 0)
			break;
		next_acb->queue.scsi_req_baddr = acbvtop(adw, next_acb);
		next_acb->queue.sense_addr =
		    acbvtop(adw, next_acb) + offsetof(struct acb, sense_data);
		next_acb->sg_blocks = blocks;
		next_acb->sg_busaddr = busaddr;
		/* Setup static data in the sg blocks */
		for (j = 0; j < ADW_SG_BLOCKCNT; j++) {
			next_acb->sg_blocks[j].first_entry_no =
			    j * ADW_NO_OF_SG_PER_BLOCK;
		}
		next_acb->state = ACB_FREE;
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
	int	 s;

	acb = (struct acb *)arg;
	ccb = acb->ccb;
	adw = (struct adw_softc *)ccb->ccb_h.ccb_adw_ptr;

	if (error != 0) {
		if (error != EFBIG)
			printf("%s: Unexepected error 0x%x returned from "
			       "bus_dmamap_load\n", adw_name(adw), error);
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
				u_int sg_left;

				sg_left = ADW_NO_OF_SG_PER_BLOCK;
				sg = sg_block->sg_list;
				while (dm_segs < end_seg && sg_left != 0) {
					sg->sg_addr = dm_segs->ds_addr;
					sg->sg_count = dm_segs->ds_len;
					sg++;
					dm_segs++;
					sg_left--;
				}
				sg_index += ADW_NO_OF_SG_PER_BLOCK - sg_left;
				sg_block->last_entry_no = sg_index - 1;
				if (dm_segs == end_seg) {
					sg_block->sg_busaddr_next = 0;
					break;
				} else {
					sg_busaddr +=
					    sizeof(struct adw_sg_block);
					sg_block->sg_busaddr_next = sg_busaddr;
				}
			}

			acb->queue.sg_entry_cnt = nseg;
			acb->queue.sg_real_addr = acb->sg_busaddr;
		} else {
			acb->queue.sg_entry_cnt = 0;
			acb->queue.sg_real_addr = 0;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(adw->buffer_dmat, acb->dmamap, op);

	} else {
		acb->queue.sg_entry_cnt = 0;
		acb->queue.data_addr = 0;
		acb->queue.data_cnt = 0;
		acb->queue.sg_real_addr = 0;
	}
	acb->queue.free_scsiq_link = 0;
	acb->queue.ux_wk_data_cnt = 0;

	s = splcam();

	/*
	 * Last time we need to check if this CCB needs to
	 * be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(adw->buffer_dmat, acb->dmamap);
		adwfreeacb(adw, acb);
		xpt_done(ccb);
		splx(s);
		return;
	}
		
	acb->state |= ACB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	LIST_INSERT_HEAD(&adw->pending_ccbs, &ccb->ccb_h, sim_links.le);
	ccb->ccb_h.timeout_ch =
	    timeout(adwtimeout, (caddr_t)acb,
		    (ccb->ccb_h.timeout * hz) / 1000);

	adw_send_acb(adw, acb, acbvtop(adw, acb));

	splx(s);
}

static void
adw_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	adw_softc *adw;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("adw_action\n"));
	
	adw = (struct adw_softc *)cam_sim_softc(sim);

	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		struct	ccb_scsiio *csio;
		struct	ccb_hdr *ccbh;
		struct	acb *acb;

		csio = &ccb->csio;
		ccbh = &ccb->ccb_h;
		/* Max supported CDB length is 12 bytes */
		if (csio->cdb_len > 12) { 
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}

		if ((acb = adwgetacb(adw)) == NULL) {
			int s;
	
			s = splcam();
			adw->state |= ADW_RESOURCE_SHORTAGE;
			splx(s);
			xpt_freeze_simq(sim, /*count*/1);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			return;
		}

		/* Link dccb and ccb so we can find one from the other */
		acb->ccb = ccb;
		ccb->ccb_h.ccb_acb_ptr = acb;
		ccb->ccb_h.ccb_adw_ptr = adw;

		acb->queue.cntl = 0;
		acb->queue.target_id = ccb->ccb_h.target_id;
		acb->queue.target_lun = ccb->ccb_h.target_lun;

		acb->queue.srb_ptr = 0;
		acb->queue.a_flag = 0;
		acb->queue.sense_len =
			MIN(csio->sense_len, sizeof(acb->sense_data));
		acb->queue.cdb_len = csio->cdb_len;

		if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0)
			acb->queue.tag_code = csio->tag_action;
		else
			acb->queue.tag_code = 0;

		acb->queue.done_status = 0;
		acb->queue.scsi_status = 0;
		acb->queue.host_status = 0;
		acb->queue.ux_sg_ix = 0;

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

		/*
		 * If we have any data to send with this command,
		 * map it into bus space.
		 */
		if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			if ((ccbh->flags & CAM_SCATTER_VALID) == 0) {
				/*
				 * We've been given a pointer
				 * to a single buffer.
				 */
				if ((ccbh->flags & CAM_DATA_PHYS) == 0) {
					int s;
					int error;

					s = splsoftvm();
					error =
					    bus_dmamap_load(adw->buffer_dmat,
							    acb->dmamap,
							    csio->data_ptr,
							    csio->dxfer_len,
							    adwexecuteacb,
							    acb, /*flags*/0);
					if (error == EINPROGRESS) {
						/*
						 * So as to maintain ordering,
						 * freeze the controller queue
						 * until our mapping is
						 * returned.
						 */
						xpt_freeze_simq(sim, 1);
						acb->state |= CAM_RELEASE_SIMQ;
					}
					splx(s);
				} else {
					struct bus_dma_segment seg; 

					/* Pointer to physical buffer */
					seg.ds_addr =
					    (bus_addr_t)csio->data_ptr;
					seg.ds_len = csio->dxfer_len;
					adwexecuteacb(acb, &seg, 1, 0);
				}
			} else {
				struct bus_dma_segment *segs;

				if ((ccbh->flags & CAM_DATA_PHYS) != 0)
					panic("adw_action - Physical "
					      "segment pointers "
					      "unsupported");

				if ((ccbh->flags&CAM_SG_LIST_PHYS)==0)
					panic("adw_action - Virtual "
					      "segment addresses "
					      "unsupported");

				/* Just use the segments provided */
				segs = (struct bus_dma_segment *)csio->data_ptr;
				adwexecuteacb(acb, segs, csio->sglist_cnt,
					      (csio->sglist_cnt < ADW_SGSIZE)
					      ? 0 : EFBIG);
			}
		} else {
			adwexecuteacb(acb, NULL, 0, 0);
		}
		break;
	}
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	{
		adw_idle_cmd_status_t status;

		adw_idle_cmd_send(adw, ADW_IDLE_CMD_DEVICE_RESET,
				  ccb->ccb_h.target_id);
		status = adw_idle_cmd_wait(adw);
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
		struct	  ccb_trans_settings *cts;
		u_int	  target_mask;
		int	  s;

		cts = &ccb->cts;
		target_mask = 0x01 << ccb->ccb_h.target_id;

		s = splcam();
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
			if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
				u_int discenb;

				discenb =
				    adw_lram_read_16(adw, ADW_MC_DISC_ENABLE);

				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0)
					discenb |= target_mask;
				else
					discenb &= ~target_mask;

				adw_lram_write_16(adw, ADW_MC_DISC_ENABLE,
						  discenb);
			}
		
			if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {

				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0)
					adw->tagenb |= target_mask;
				else
					adw->tagenb &= ~target_mask;
			}	

			if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0) {
				u_int wdtrenb_orig;
				u_int wdtrenb;
				u_int wdtrdone;

				wdtrenb_orig =
				    adw_lram_read_16(adw, ADW_MC_WDTR_ABLE);
				wdtrenb = wdtrenb_orig;
				wdtrdone = adw_lram_read_16(adw,
							    ADW_MC_WDTR_DONE);
				switch (cts->bus_width) {
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
				}
			}

			if (((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0)
			 || ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0)) {
				u_int sdtrenb_orig;
				u_int sdtrenb;
				u_int ultraenb_orig;
				u_int ultraenb;
				u_int sdtrdone;

				sdtrenb_orig =
				    adw_lram_read_16(adw, ADW_MC_SDTR_ABLE);
				sdtrenb = sdtrenb_orig;

				ultraenb_orig =
				    adw_lram_read_16(adw, ADW_MC_ULTRA_ABLE);
				ultraenb = ultraenb_orig;

				sdtrdone = adw_lram_read_16(adw,
							    ADW_MC_SDTR_DONE);

				if ((cts->valid
				   & CCB_TRANS_SYNC_RATE_VALID) != 0) {

					if (cts->sync_period == 0) {
						sdtrenb &= ~target_mask;
					} else if (cts->sync_period > 12) {
						ultraenb &= ~target_mask;
						sdtrenb |= target_mask;
					} else {
						ultraenb |= target_mask;
						sdtrenb |= target_mask;
					}
				}
					
				if ((cts->valid
				   & CCB_TRANS_SYNC_OFFSET_VALID) != 0) {
					if (cts->sync_offset == 0)
						sdtrenb &= ~target_mask;
				}

				if (sdtrenb != sdtrenb_orig
				 || ultraenb != ultraenb_orig) {
					adw_lram_write_16(adw, ADW_MC_SDTR_ABLE,
							  sdtrenb);
					adw_lram_write_16(adw,
							  ADW_MC_ULTRA_ABLE,
							  ultraenb);
					sdtrdone &= ~target_mask;
					adw_lram_write_16(adw, ADW_MC_SDTR_DONE,
							  sdtrdone);
				}
			} 
		}
		splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	ccb_trans_settings *cts;
		u_int	target_mask;
 
		cts = &ccb->cts;
		target_mask = 0x01 << ccb->ccb_h.target_id;
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) { 
			cts->flags = 0;
			if ((adw->user_discenb & target_mask) != 0)
				cts->flags |= CCB_TRANS_DISC_ENB;

			if ((adw->user_tagenb & target_mask) != 0)
				cts->flags |= CCB_TRANS_TAG_ENB;

			if ((adw->user_wdtr & target_mask) != 0)
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			else
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;

			if ((adw->user_sdtr & target_mask) != 0) {
				if ((adw->user_ultra & target_mask) != 0)
					cts->sync_period = 12; /* 20MHz */
				else
					cts->sync_period = 25; /* 10MHz */
				cts->sync_offset = 15; /* XXX ??? */
			}

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else {
			u_int targ_tinfo;

			cts->flags = 0;
			if ((adw_lram_read_16(adw, ADW_MC_DISC_ENABLE)
			  & target_mask) != 0)
				cts->flags |= CCB_TRANS_DISC_ENB;

			if ((adw->tagenb & target_mask) != 0)
				cts->flags |= CCB_TRANS_TAG_ENB;

			targ_tinfo =
			    adw_lram_read_16(adw,
					     ADW_MC_DEVICE_HSHK_CFG_TABLE
					     + (2 * ccb->ccb_h.target_id));

			if ((targ_tinfo & ADW_HSHK_CFG_WIDE_XFR) != 0)
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			else
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;

			cts->sync_period =
			    ADW_HSHK_CFG_PERIOD_FACTOR(targ_tinfo);

			cts->sync_offset = targ_tinfo & ADW_HSHK_CFG_OFFSET;
			if (cts->sync_period == 0)
				cts->sync_offset = 0;

			if (cts->sync_offset == 0)
				cts->sync_period = 0;
		}
		cts->valid = CCB_TRANS_SYNC_RATE_VALID
			   | CCB_TRANS_SYNC_OFFSET_VALID
			   | CCB_TRANS_BUS_WIDTH_VALID
			   | CCB_TRANS_DISC_VALID
			   | CCB_TRANS_TQ_VALID;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int	  extended;

		/*
		 * XXX Use Adaptec translation until I find out how to
		 *     get this information from the card.
		 */
		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		extended = 1;
		
		if (size_mb > 1024 && extended) {
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
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{
		adw_idle_cmd_status_t status;

		adw_idle_cmd_send(adw, ADW_IDLE_CMD_SCSI_RESET, /*param*/0);
		status = adw_idle_cmd_wait(adw);
		if (status == ADW_IDLE_CMD_SUCCESS) {
			ccb->ccb_h.status = CAM_REQ_CMP;
			if (bootverbose) {
				xpt_print_path(adw->path);
				printf("Bus Reset Delivered\n");
			}
		} else
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
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
	adw_intr(cam_sim_softc(sim));
}

static void
adw_async(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
}

struct adw_softc *
adw_alloc(int unit, bus_space_tag_t tag, bus_space_handle_t bsh)
{
	struct	 adw_softc *adw;
	int	 i;
   
	/*
	 * Allocate a storage area for us
	 */
	adw = malloc(sizeof(struct adw_softc), M_DEVBUF, M_NOWAIT);
	if (adw == NULL) {
		printf("adw%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(adw, sizeof(struct adw_softc));
	LIST_INIT(&adw->pending_ccbs);
	SLIST_INIT(&adw->sg_maps);
	adw->unit = unit;
	adw->tag = tag;
	adw->bsh = bsh;
	i = adw->unit / 10;
	adw->name = malloc(sizeof("adw") + i + 1, M_DEVBUF, M_NOWAIT);
	if (adw->name == NULL) {
		printf("adw%d: cannot malloc name!\n", unit);
		free(adw, M_DEVBUF);
		return NULL;
	}
	sprintf(adw->name, "adw%d", adw->unit);
	return(adw);
}

void
adw_free(struct adw_softc *adw)
{
	switch (adw->init_level) {
	case 6:
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
	case 5:
		bus_dmamap_unload(adw->acb_dmat, adw->acb_dmamap);
	case 4:
		bus_dmamem_free(adw->acb_dmat, adw->acbs,
				adw->acb_dmamap);
		bus_dmamap_destroy(adw->acb_dmat, adw->acb_dmamap);
	case 3:
		bus_dma_tag_destroy(adw->acb_dmat);
	case 2:
		bus_dma_tag_destroy(adw->buffer_dmat);
	case 1:
		bus_dma_tag_destroy(adw->parent_dmat);
	case 0:
		break;
	}
	free(adw->name, M_DEVBUF);
	free(adw, M_DEVBUF);
}

int
adw_init(struct adw_softc *adw)
{
	struct	  adw_eeprom eep_config;
	u_int16_t checksum;
	u_int16_t scsicfg1;

	adw_reset_chip(adw);
	checksum = adw_eeprom_read(adw, &eep_config);
	bcopy(eep_config.serial_number, adw->serial_number,
	      sizeof(adw->serial_number));
	if (checksum != eep_config.checksum) {
		u_int16_t serial_number[3];

		printf("%s: EEPROM checksum failed.  Restoring Defaults\n",
		       adw_name(adw));

	        /*
		 * Restore the default EEPROM settings.
		 * Assume the 6 byte board serial number that was read
		 * from EEPROM is correct even if the EEPROM checksum
		 * failed.
		 */
		bcopy(&adw_default_eeprom, &eep_config, sizeof(eep_config));
		bcopy(adw->serial_number, eep_config.serial_number,
		      sizeof(serial_number));
		adw_eeprom_write(adw, &eep_config);
	}

	/* Pull eeprom information into our softc. */
	adw->bios_ctrl = eep_config.bios_ctrl;
	adw->user_wdtr = eep_config.wdtr_able;
	adw->user_sdtr = eep_config.sdtr_able;
	adw->user_ultra = eep_config.ultra_able;
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
	switch (eep_config.termination) {
	default:
		printf("%s: Invalid EEPROM Termination Settings.\n",
		       adw_name(adw));
		printf("%s: Reverting to Automatic Termination\n",
		       adw_name(adw));
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

	printf("%s: SCSI ID %d, ", adw_name(adw), adw->initiator_id);

	if (adw_init_chip(adw, scsicfg1) != 0)
		return (-1);

	printf("Queue Depth %d\n", adw->max_acbs);

	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(adw->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/MAXBSIZE, /*nsegments*/ADW_SGSIZE,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/BUS_DMA_ALLOCNOW,
			       &adw->buffer_dmat) != 0) {
		return (-1);
	}

	adw->init_level++;

	/* DMA tag for our ccb structures */
	if (bus_dma_tag_create(adw->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       adw->max_acbs * sizeof(struct acb),
			       /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &adw->acb_dmat) != 0) {
		return (-1);
        }

	adw->init_level++;

	/* Allocation for our ccbs */
	if (bus_dmamem_alloc(adw->acb_dmat, (void **)&adw->acbs,
			     BUS_DMA_NOWAIT, &adw->acb_dmamap) != 0) {
		return (-1);
	}

	adw->init_level++;

	/* And permanently map them */
	bus_dmamap_load(adw->acb_dmat, adw->acb_dmamap,
			adw->acbs,
			adw->max_acbs * sizeof(struct acb),
			adwmapmem, &adw->acb_busbase, /*flags*/0);

	/* Clear them out. */
	bzero(adw->acbs, adw->max_acbs * sizeof(struct acb)); 

	/* DMA tag for our S/G structures.  We allocate in page sized chunks */
	if (bus_dma_tag_create(adw->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       PAGE_SIZE, /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &adw->sg_dmat) != 0) {
		return (-1);
        }

	adw->init_level++;

	/* Allocate our first batch of ccbs */
	if (adwallocacbs(adw) == 0)
		return (-1);

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

	/* Start the Risc processor now that we are fully configured. */
	adw_outw(adw, ADW_RISC_CSR, ADW_RISC_CSR_RUN);

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(adw->max_acbs);
	if (devq == NULL)
		return (0);

	/*
	 * Construct our SIM entry.
	 */
	adw->sim = cam_sim_alloc(adw_action, adw_poll, "adw", adw, adw->unit,
				 1, adw->max_acbs, devq);
	if (adw->sim == NULL)
		return (0);

	/*
	 * Register the bus.
	 */
	if (xpt_bus_register(adw->sim, 0) != CAM_SUCCESS) {
		cam_sim_free(adw->sim, /*free devq*/TRUE);
		return (0);
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

	return (0);
}

void
adw_intr(void *arg)
{
	struct	adw_softc *adw;
	u_int	int_stat;
	u_int	next_doneq;
	u_int	next_completeq;
	u_int	doneq_start;
	
	adw = (struct adw_softc *)arg;
	if ((adw_inw(adw, ADW_CTRL_REG) & ADW_CTRL_REG_HOST_INTR) == 0)
		return;

	/* Reading the register clears the interrupt. */
	int_stat = adw_inb(adw, ADW_INTR_STATUS_REG);

	if ((int_stat & ADW_INTR_STATUS_INTRB) != 0) {
		/* Idle Command Complete */
		adw->idle_command_cmp = 1;
		switch (adw->idle_cmd) {
		case ADW_IDLE_CMD_DEVICE_RESET:
			adw_handle_device_reset(adw,
						/*target*/adw->idle_cmd_param);
			break;
		case ADW_IDLE_CMD_SCSI_RESET:
			adw_handle_bus_reset(adw, /*initiated*/TRUE);
			break;
		default:
			break;
		}
		adw->idle_cmd = ADW_IDLE_CMD_COMPLETED;
	}

	if ((int_stat & ADW_INTR_STATUS_INTRC) != 0) {
		/* SCSI Bus Reset */
		adw_handle_bus_reset(adw, /*initiated*/FALSE);
        }

	/*
	 * ADW_MC_HOST_NEXT_DONE is actually the last completed RISC
	 * Queue List request. Its forward pointer (RQL_FWD) points to the
	 * current completed RISC Queue List request.
	 */
	next_doneq = adw_lram_read_8(adw, ADW_MC_HOST_NEXT_DONE);
	next_doneq = ADW_MC_RISC_Q_LIST_BASE + RQL_FWD
		   + (next_doneq * ADW_MC_RISC_Q_LIST_SIZE);

	next_completeq = adw_lram_read_8(adw, next_doneq);
	doneq_start = ADW_MC_NULL_Q;
	/* Loop until all completed Q's are processed. */
	while (next_completeq != ADW_MC_NULL_Q) {
		u_int32_t acb_busaddr;
		struct	  acb *acb;
		union	  ccb *ccb;

		doneq_start = next_completeq;

		next_doneq = ADW_MC_RISC_Q_LIST_BASE +
			     (next_completeq * ADW_MC_RISC_Q_LIST_SIZE);

		/*
		 * Read the ADW_SCSI_REQ_Q physical address pointer from
		 * the RISC list entry.
		 */
		acb_busaddr = adw_lram_read_32(adw, next_doneq + RQL_PHYADDR);
		acb = acbptov(adw, acb_busaddr);
		
		/* Change the RISC Queue List state to free. */
		adw_lram_write_8(adw, next_doneq + RQL_STATE, ADW_MC_QS_FREE);

		/* Get the RISC Queue List forward pointer. */
		next_completeq = adw_lram_read_8(adw, next_doneq + RQL_FWD);

		/* Process CCB */
		ccb = acb->ccb;
		untimeout(adwtimeout, acb, ccb->ccb_h.timeout_ch);
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

	if (doneq_start != ADW_MC_NULL_Q)
		adw_lram_write_8(adw, ADW_MC_HOST_NEXT_DONE, doneq_start);
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
		case QHSTA_M_QUEUE_ABORTED:
			/* BDR or Bus Reset */
			ccb->ccb_h.status = adw->last_reset;
			break;
		case QHSTA_M_SXFR_SDMA_ERR:
		case QHSTA_M_SXFR_SXFR_PERR:
		case QHSTA_M_RDMA_PERR:
			ccb->ccb_h.status = CAM_UNCOR_PARITY;
			break;
		case QHSTA_M_WTM_TIMEOUT:
		case QHSTA_M_SXFR_WD_TMO:
			/* The SCSI bus hung in a phase */
			ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
			adw_idle_cmd_send(adw, ADW_IDLE_CMD_SCSI_RESET,
					  /*param*/0);
			break;
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
			      adw_name(adw), acb->queue.host_status);
			/* NOTREACHED */
		}
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
	int		      s;

	acb = (struct acb *)arg;
	ccb = acb->ccb;
	adw = (struct adw_softc *)ccb->ccb_h.ccb_adw_ptr;
	xpt_print_path(ccb->ccb_h.path);
	printf("ACB %p - timed out\n", (void *)acb);

	s = splcam();

	if ((acb->state & ACB_ACTIVE) == 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("ACB %p - timed out CCB already completed\n",
		       (void *)acb);
		splx(s);
		return;
	}

	/* Attempt a BDR first */
	adw_idle_cmd_send(adw, ADW_IDLE_CMD_DEVICE_RESET,
			  ccb->ccb_h.target_id);
	splx(s);
	status = adw_idle_cmd_wait(adw);
	if (status == ADW_IDLE_CMD_SUCCESS) {
		printf("%s: BDR Delivered.  No longer in timeout\n",
		       adw_name(adw));
	} else {
		adw_idle_cmd_send(adw, ADW_IDLE_CMD_SCSI_RESET, /*param*/0);
		status = adw_idle_cmd_wait(adw);
		if (status != ADW_IDLE_CMD_SUCCESS)
			panic("%s: Bus Reset during timeout failed",
			      adw_name(adw));
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
