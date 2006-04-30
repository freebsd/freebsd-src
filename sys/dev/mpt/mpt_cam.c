/*-
 * FreeBSD/CAM specific routines for LSI '909 FC  adapters.
 * FreeBSD Version.
 *
 * Copyright (c)  2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */
/*-
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/mpt/mpt.h>
#include <dev/mpt/mpt_cam.h>
#include <dev/mpt/mpt_raid.h>

#include "dev/mpt/mpilib/mpi_ioc.h" /* XXX Fix Event Handling!!! */
#include "dev/mpt/mpilib/mpi_init.h"
#include "dev/mpt/mpilib/mpi_targ.h"

#include <sys/callout.h>
#include <sys/kthread.h>

static void mpt_poll(struct cam_sim *);
static timeout_t mpt_timeout;
static void mpt_action(struct cam_sim *, union ccb *);
static int mpt_setwidth(struct mpt_softc *, int, int);
static int mpt_setsync(struct mpt_softc *, int, int, int);
static void mpt_calc_geometry(struct ccb_calc_geometry *ccg, int extended);
static mpt_reply_handler_t mpt_scsi_reply_handler;
static mpt_reply_handler_t mpt_scsi_tmf_reply_handler;
static int mpt_scsi_reply_frame_handler(struct mpt_softc *mpt, request_t *req,
					MSG_DEFAULT_REPLY *reply_frame);
static int mpt_bus_reset(struct mpt_softc *, int /*sleep_ok*/);

static int mpt_spawn_recovery_thread(struct mpt_softc *mpt);
static void mpt_terminate_recovery_thread(struct mpt_softc *mpt);
static void mpt_recovery_thread(void *arg);
static int mpt_scsi_send_tmf(struct mpt_softc *, u_int /*type*/,
			     u_int /*flags*/, u_int /*channel*/,
			     u_int /*target*/, u_int /*lun*/,
			     u_int /*abort_ctx*/, int /*sleep_ok*/);
static void mpt_recover_commands(struct mpt_softc *mpt);

static uint32_t scsi_io_handler_id = MPT_HANDLER_ID_NONE;
static uint32_t scsi_tmf_handler_id = MPT_HANDLER_ID_NONE;

static mpt_probe_handler_t	mpt_cam_probe;
static mpt_attach_handler_t	mpt_cam_attach;
static mpt_event_handler_t	mpt_cam_event;
static mpt_reset_handler_t	mpt_cam_ioc_reset;
static mpt_detach_handler_t	mpt_cam_detach;

static struct mpt_personality mpt_cam_personality =
{
	.name		= "mpt_cam",
	.probe		= mpt_cam_probe,
	.attach		= mpt_cam_attach,
	.event		= mpt_cam_event,
	.reset		= mpt_cam_ioc_reset,
	.detach		= mpt_cam_detach,
};

DECLARE_MPT_PERSONALITY(mpt_cam, SI_ORDER_SECOND);

int
mpt_cam_probe(struct mpt_softc *mpt)
{
	/*
	 * Only attach to nodes that support the initiator
	 * role or have RAID physical devices that need
	 * CAM pass-thru support.
	 */
	if ((mpt->mpt_proto_flags & MPI_PORTFACTS_PROTOCOL_INITIATOR) != 0
	 || (mpt->ioc_page2 != NULL && mpt->ioc_page2->MaxPhysDisks != 0))
		return (0);
	return (ENODEV);
}

int
mpt_cam_attach(struct mpt_softc *mpt)
{
	struct cam_devq *devq;
	mpt_handler_t	 handler;
	int		 maxq;
	int		 error;

	MPTLOCK_2_CAMLOCK(mpt);
	TAILQ_INIT(&mpt->request_timeout_list);
	mpt->bus = 0;
	maxq = (mpt->mpt_global_credits < MPT_MAX_REQUESTS(mpt))?
	    mpt->mpt_global_credits : MPT_MAX_REQUESTS(mpt);

	handler.reply_handler = mpt_scsi_reply_handler;
	error = mpt_register_handler(mpt, MPT_HANDLER_REPLY, handler,
				     &scsi_io_handler_id);
	if (error != 0)
		goto cleanup;
	handler.reply_handler = mpt_scsi_tmf_reply_handler;
	error = mpt_register_handler(mpt, MPT_HANDLER_REPLY, handler,
				     &scsi_tmf_handler_id);
	if (error != 0)
		goto cleanup;

	/*
	 * We keep one request reserved for timeout TMF requests.
	 */
	mpt->tmf_req = mpt_get_request(mpt, /*sleep_ok*/FALSE);
	if (mpt->tmf_req == NULL) {
		mpt_prt(mpt, "Unable to allocate dedicated TMF request!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Mark the request as free even though not on the free list.
	 * There is only one TMF request allowed to be outstanding at
	 * a time and the TMF routines perform their own allocation
	 * tracking using the standard state flags.
	 */
	mpt->tmf_req->state = REQ_STATE_FREE;
	maxq--;

	if (mpt_spawn_recovery_thread(mpt) != 0) {
		mpt_prt(mpt, "Unable to spawn recovery thread!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Create the device queue for our SIM(s).
	 */
	devq = cam_simq_alloc(maxq);
	if (devq == NULL) {
		mpt_prt(mpt, "Unable to allocate CAM SIMQ!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Construct our SIM entry.
	 */
	mpt->sim = cam_sim_alloc(mpt_action, mpt_poll, "mpt", mpt,
	    mpt->unit, 1, maxq, devq);
	if (mpt->sim == NULL) {
		mpt_prt(mpt, "Unable to allocate CAM SIM!\n");
		cam_simq_free(devq);
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Register exactly the bus.
	 */
	if (xpt_bus_register(mpt->sim, 0) != CAM_SUCCESS) {
		mpt_prt(mpt, "Bus registration Failed!\n");
		error = ENOMEM;
		goto cleanup;
	}

	if (xpt_create_path(&mpt->path, NULL, cam_sim_path(mpt->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		mpt_prt(mpt, "Unable to allocate Path!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Only register a second bus for RAID physical
	 * devices if the controller supports RAID.
	 */
	if (mpt->ioc_page2 == NULL
	 || mpt->ioc_page2->MaxPhysDisks == 0)
		return (0);

	/*
	 * Create a "bus" to export all hidden disks to CAM.
	 */
	mpt->phydisk_sim = cam_sim_alloc(mpt_action, mpt_poll, "mpt", mpt,
	    mpt->unit, 1, maxq, devq);
	if (mpt->phydisk_sim == NULL) {
		mpt_prt(mpt, "Unable to allocate Physical Disk CAM SIM!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Register exactly the bus.
	 */
	if (xpt_bus_register(mpt->phydisk_sim, 1) != CAM_SUCCESS) {
		mpt_prt(mpt, "Physical Disk Bus registration Failed!\n");
		error = ENOMEM;
		goto cleanup;
	}

	if (xpt_create_path(&mpt->phydisk_path, NULL,
	    cam_sim_path(mpt->phydisk_sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		mpt_prt(mpt, "Unable to allocate Physical Disk Path!\n");
		error = ENOMEM;
		goto cleanup;
	}

	CAMLOCK_2_MPTLOCK(mpt);
	return (0);
cleanup:
	CAMLOCK_2_MPTLOCK(mpt);
	mpt_cam_detach(mpt);
	return (error);
}

void
mpt_cam_detach(struct mpt_softc *mpt)
{
	mpt_handler_t handler;

	mpt_terminate_recovery_thread(mpt); 

	handler.reply_handler = mpt_scsi_reply_handler;
	mpt_deregister_handler(mpt, MPT_HANDLER_REPLY, handler,
			       scsi_io_handler_id);
	handler.reply_handler = mpt_scsi_tmf_reply_handler;
	mpt_deregister_handler(mpt, MPT_HANDLER_REPLY, handler,
			       scsi_tmf_handler_id);

	if (mpt->tmf_req != NULL) {
		mpt_free_request(mpt, mpt->tmf_req);
		mpt->tmf_req = NULL;
	}

	if (mpt->sim != NULL) {
		xpt_free_path(mpt->path);
		xpt_bus_deregister(cam_sim_path(mpt->sim));
		cam_sim_free(mpt->sim, TRUE);
		mpt->sim = NULL;
	}

	if (mpt->phydisk_sim != NULL) {
		xpt_free_path(mpt->phydisk_path);
		xpt_bus_deregister(cam_sim_path(mpt->phydisk_sim));
		cam_sim_free(mpt->phydisk_sim, TRUE);
		mpt->phydisk_sim = NULL;
	}
}

/* This routine is used after a system crash to dump core onto the
 * swap device.
 */
static void
mpt_poll(struct cam_sim *sim)
{
	struct mpt_softc *mpt;

	mpt = (struct mpt_softc *)cam_sim_softc(sim);
	MPT_LOCK(mpt);
	mpt_intr(mpt);
	MPT_UNLOCK(mpt);
}

/*
 * Watchdog timeout routine for SCSI requests.
 */
static void
mpt_timeout(void *arg)
{
	union ccb	 *ccb;
	struct mpt_softc *mpt;
	request_t	 *req;

	ccb = (union ccb *)arg;
#if NOTYET
	mpt = mpt_find_softc(mpt);
	if (mpt == NULL)
		return;
#else
	mpt = ccb->ccb_h.ccb_mpt_ptr;
#endif

	MPT_LOCK(mpt);
	req = ccb->ccb_h.ccb_req_ptr;
	mpt_prt(mpt, "Request %p Timed out.\n", req);
	if ((req->state & REQ_STATE_QUEUED) == REQ_STATE_QUEUED) {
		TAILQ_REMOVE(&mpt->request_pending_list, req, links);
		TAILQ_INSERT_TAIL(&mpt->request_timeout_list, req, links);
		req->state |= REQ_STATE_TIMEDOUT;
		mpt_wakeup_recovery_thread(mpt);
	}
	MPT_UNLOCK(mpt);
}

/*
 * Callback routine from "bus_dmamap_load" or, in simple cases, called directly.
 *
 * Takes a list of physical segments and builds the SGL for SCSI IO command
 * and forwards the commard to the IOC after one last check that CAM has not
 * aborted the transaction.
 */
static void
mpt_execute_req(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	request_t *req;
	union ccb *ccb;
	struct mpt_softc *mpt;
	MSG_SCSI_IO_REQUEST *mpt_req;
	SGE_SIMPLE32 *se;

	req = (request_t *)arg;
	ccb = req->ccb;

	mpt = ccb->ccb_h.ccb_mpt_ptr;
	req = ccb->ccb_h.ccb_req_ptr;
	mpt_req = req->req_vbuf;

	if (error == 0 && nseg > MPT_SGL_MAX) {
		error = EFBIG;
	}

	if (error != 0) {
		if (error != EFBIG)
			mpt_prt(mpt, "bus_dmamap_load returned %d\n", error);
		if (ccb->ccb_h.status == CAM_REQ_INPROG) {
			xpt_freeze_devq(ccb->ccb_h.path, 1);
			ccb->ccb_h.status = CAM_DEV_QFRZN;
			if (error == EFBIG)
				ccb->ccb_h.status |= CAM_REQ_TOO_BIG;
			else
				ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
		}
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		xpt_done(ccb);
		CAMLOCK_2_MPTLOCK(mpt);
		mpt_free_request(mpt, req);
		MPTLOCK_2_CAMLOCK(mpt);
		return;
	}
	
	if (nseg > MPT_NSGL_FIRST(mpt)) {
		int i, nleft = nseg;
		uint32_t flags;
		bus_dmasync_op_t op;
		SGE_CHAIN32 *ce;

		mpt_req->DataLength = ccb->csio.dxfer_len;
		flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

		se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		for (i = 0; i < MPT_NSGL_FIRST(mpt) - 1; i++, se++, dm_segs++) {
			uint32_t tf;

			bzero(se, sizeof (*se));
			se->Address = dm_segs->ds_addr;
			MPI_pSGE_SET_LENGTH(se, dm_segs->ds_len);
			tf = flags;
			if (i == MPT_NSGL_FIRST(mpt) - 2) {
				tf |= MPI_SGE_FLAGS_LAST_ELEMENT;
			}
			MPI_pSGE_SET_FLAGS(se, tf);
			nleft -= 1;
		}

		/*
		 * Tell the IOC where to find the first chain element
		 */
		mpt_req->ChainOffset = ((char *)se - (char *)mpt_req) >> 2;

		/*
		 * Until we're finished with all segments...
		 */
		while (nleft) {
			int ntodo;
			/*
			 * Construct the chain element that point to the
			 * next segment.
			 */
			ce = (SGE_CHAIN32 *) se++;
			if (nleft > MPT_NSGL(mpt)) {
				ntodo = MPT_NSGL(mpt) - 1;
				ce->NextChainOffset = (MPT_RQSL(mpt) -
				    sizeof (SGE_SIMPLE32)) >> 2;
				ce->Length = MPT_NSGL(mpt) *
				    sizeof (SGE_SIMPLE32);
			} else {
				ntodo = nleft;
				ce->NextChainOffset = 0;
				ce->Length = ntodo * sizeof (SGE_SIMPLE32);
			}
			ce->Address = req->req_pbuf +
			    ((char *)se - (char *)mpt_req);
			ce->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
			for (i = 0; i < ntodo; i++, se++, dm_segs++) {
				uint32_t tf;

				bzero(se, sizeof (*se));
				se->Address = dm_segs->ds_addr;
				MPI_pSGE_SET_LENGTH(se, dm_segs->ds_len);
				tf = flags;
				if (i == ntodo - 1) {
					tf |= MPI_SGE_FLAGS_LAST_ELEMENT;
					if (ce->NextChainOffset == 0) {
						tf |=
						    MPI_SGE_FLAGS_END_OF_LIST |
						    MPI_SGE_FLAGS_END_OF_BUFFER;
					}
				}
				MPI_pSGE_SET_FLAGS(se, tf);
				nleft -= 1;
			}

		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;
		if (!(ccb->ccb_h.flags & (CAM_SG_LIST_PHYS|CAM_DATA_PHYS))) {
			bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
		}
	} else if (nseg > 0) {
		int i;
		uint32_t flags;
		bus_dmasync_op_t op;

		mpt_req->DataLength = ccb->csio.dxfer_len;
		flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

		/* Copy the segments into our SG list */
		se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		for (i = 0; i < nseg; i++, se++, dm_segs++) {
			uint32_t tf;

			bzero(se, sizeof (*se));
			se->Address = dm_segs->ds_addr;
			MPI_pSGE_SET_LENGTH(se, dm_segs->ds_len);
			tf = flags;
			if (i == nseg - 1) {
				tf |=
				    MPI_SGE_FLAGS_LAST_ELEMENT |
				    MPI_SGE_FLAGS_END_OF_BUFFER |
				    MPI_SGE_FLAGS_END_OF_LIST;
			}
			MPI_pSGE_SET_FLAGS(se, tf);
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;
		if (!(ccb->ccb_h.flags & (CAM_SG_LIST_PHYS|CAM_DATA_PHYS))) {
			bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
		}
	} else {
		se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		/*
		 * No data to transfer so we just make a single simple SGL
		 * with zero length.
		 */
		MPI_pSGE_SET_FLAGS(se,
		    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
		    MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
	}

	/*
	 * Last time we need to check if this CCB needs to be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg && (ccb->ccb_h.flags & CAM_SG_LIST_PHYS) == 0)
			bus_dmamap_unload(mpt->buffer_dmat, req->dmap);
		CAMLOCK_2_MPTLOCK(mpt);
		mpt_free_request(mpt, req);
		MPTLOCK_2_CAMLOCK(mpt);
		xpt_done(ccb);
		return;
	}

	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	CAMLOCK_2_MPTLOCK(mpt);
	if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
		ccb->ccb_h.timeout_ch =
			timeout(mpt_timeout, (caddr_t)ccb,
				(ccb->ccb_h.timeout * hz) / 1000);
	} else {
		callout_handle_init(&ccb->ccb_h.timeout_ch);
	}
	if (mpt->verbose >= MPT_PRT_DEBUG)
		mpt_print_scsi_io_request(mpt_req);
	mpt_send_cmd(mpt, req);
	MPTLOCK_2_CAMLOCK(mpt);
}

static void
mpt_start(struct cam_sim *sim, union ccb *ccb)
{
	request_t *req;
	struct mpt_softc *mpt;
	MSG_SCSI_IO_REQUEST *mpt_req;
	struct ccb_scsiio *csio = &ccb->csio;
	struct ccb_hdr *ccbh = &ccb->ccb_h;
	int raid_passthru;

	/* Get the pointer for the physical addapter */
	mpt = ccb->ccb_h.ccb_mpt_ptr;
	raid_passthru = (sim == mpt->phydisk_sim);

	CAMLOCK_2_MPTLOCK(mpt);
	/* Get a request structure off the free list */
	if ((req = mpt_get_request(mpt, /*sleep_ok*/FALSE)) == NULL) {
		if (mpt->outofbeer == 0) {
			mpt->outofbeer = 1;
			xpt_freeze_simq(mpt->sim, 1);
			mpt_lprt(mpt, MPT_PRT_DEBUG, "FREEZEQ\n");
		}
		MPTLOCK_2_CAMLOCK(mpt);
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}

	MPTLOCK_2_CAMLOCK(mpt);

#if 0
	COWWWWW
	if (raid_passthru) {
		status = mpt_raid_quiesce_disk(mpt, mpt->raid_disks + ccb->ccb_h.target_id,
		     request_t *req)
#endif 

	/*
	 * Link the ccb and the request structure so we can find
	 * the other knowing either the request or the ccb
	 */
	req->ccb = ccb;
	ccb->ccb_h.ccb_req_ptr = req;

	/* Now we build the command for the IOC */
	mpt_req = req->req_vbuf;
	bzero(mpt_req, sizeof *mpt_req);

	mpt_req->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	if (raid_passthru)
		mpt_req->Function = MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;

	mpt_req->Bus = mpt->bus;

	mpt_req->SenseBufferLength =
		(csio->sense_len < MPT_SENSE_SIZE) ?
		 csio->sense_len : MPT_SENSE_SIZE;

	/*
	 * We use the message context to find the request structure when we
	 * Get the command completion interrupt from the IOC.
	 */
	mpt_req->MsgContext = htole32(req->index | scsi_io_handler_id);

	/* Which physical device to do the I/O on */
	mpt_req->TargetID = ccb->ccb_h.target_id;
	/*
	 * XXX Assumes Single level, Single byte, CAM LUN type.
	 */
	mpt_req->LUN[1] = ccb->ccb_h.target_lun;

	/* Set the direction of the transfer */
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
		mpt_req->Control = MPI_SCSIIO_CONTROL_READ;
	else if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
		mpt_req->Control = MPI_SCSIIO_CONTROL_WRITE;
	else
		mpt_req->Control = MPI_SCSIIO_CONTROL_NODATATRANSFER;

	if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0) {
		switch(ccb->csio.tag_action) {
		case MSG_HEAD_OF_Q_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_HEADOFQ;
			break;
		case MSG_ACA_TASK:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_ACAQ;
			break;
		case MSG_ORDERED_Q_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_ORDEREDQ;
			break;
		case MSG_SIMPLE_Q_TAG:
		default:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_SIMPLEQ;
			break;
		}
	} else {
		if (mpt->is_fc)
			mpt_req->Control |= MPI_SCSIIO_CONTROL_SIMPLEQ;
		else
			/* XXX No such thing for a target doing packetized. */
			mpt_req->Control |= MPI_SCSIIO_CONTROL_UNTAGGED;
	}

	if (mpt->is_fc == 0) {
		if (ccb->ccb_h.flags & CAM_DIS_DISCONNECT) {
			mpt_req->Control |= MPI_SCSIIO_CONTROL_NO_DISCONNECT;
		}
	}

	/* Copy the scsi command block into place */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0)
		bcopy(csio->cdb_io.cdb_ptr, mpt_req->CDB, csio->cdb_len);
	else
		bcopy(csio->cdb_io.cdb_bytes, mpt_req->CDB, csio->cdb_len);

	mpt_req->CDBLength = csio->cdb_len;
	mpt_req->DataLength = csio->dxfer_len;
	mpt_req->SenseBufferLowAddr = req->sense_pbuf;

	/*
	 * If we have any data to send with this command,
	 * map it into bus space.
	 */

	if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if ((ccbh->flags & CAM_SCATTER_VALID) == 0) {
			/*
			 * We've been given a pointer to a single buffer.
			 */
			if ((ccbh->flags & CAM_DATA_PHYS) == 0) {
				/*
				 * Virtual address that needs to translated into
				 * one or more physical address ranges.
				 */
				int error;

				error = bus_dmamap_load(mpt->buffer_dmat,
				    req->dmap, csio->data_ptr, csio->dxfer_len,
				    mpt_execute_req, req, 0);
				if (error == EINPROGRESS) {
					/*
					 * So as to maintain ordering,
					 * freeze the controller queue
					 * until our mapping is
					 * returned.
					 */
					xpt_freeze_simq(mpt->sim, 1);
					ccbh->status |= CAM_RELEASE_SIMQ;
				}
			} else {
				/*
				 * We have been given a pointer to single
				 * physical buffer.
				 */
				struct bus_dma_segment seg;
				seg.ds_addr = 
				    (bus_addr_t)(vm_offset_t)csio->data_ptr;
				seg.ds_len = csio->dxfer_len;
				mpt_execute_req(req, &seg, 1, 0);
			}
		} else {
			/*
			 * We have been given a list of addresses.
			 * This case could be easily supported but they are not
			 * currently generated by the CAM subsystem so there
			 * is no point in wasting the time right now.
			 */
			struct bus_dma_segment *segs;
			if ((ccbh->flags & CAM_SG_LIST_PHYS) == 0) {
				mpt_execute_req(req, NULL, 0, EFAULT);
			} else {
				/* Just use the segments provided */
				segs = (struct bus_dma_segment *)csio->data_ptr;
				mpt_execute_req(req, segs, csio->sglist_cnt,
				    (csio->sglist_cnt < MPT_SGL_MAX)?
				    0 : EFBIG);
			}
		}
	} else {
		mpt_execute_req(req, NULL, 0, 0);
	}
}

static int
mpt_bus_reset(struct mpt_softc *mpt, int sleep_ok)
{
	int   error;
	u_int status;

	error = mpt_scsi_send_tmf(mpt, MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
	    mpt->is_fc ? MPI_SCSITASKMGMT_MSGFLAGS_LIP_RESET_OPTION : 0,
	    /*bus*/0, /*target_id*/0, /*target_lun*/0, /*abort_ctx*/0,
	    sleep_ok);

	if (error != 0) {
		/*
		 * mpt_scsi_send_tmf hard resets on failure, so no
		 * need to do so here.
		 */
		mpt_prt(mpt,
		    "mpt_bus_reset: mpt_scsi_send_tmf returned %d\n", error);
		return (EIO);
	}

	/* Wait for bus reset to be processed by the IOC. */
	error = mpt_wait_req(mpt, mpt->tmf_req, REQ_STATE_DONE,
	    REQ_STATE_DONE, sleep_ok, /*time_ms*/5000);

	status = mpt->tmf_req->IOCStatus;
	mpt->tmf_req->state = REQ_STATE_FREE;
	if (error) {
		mpt_prt(mpt, "mpt_bus_reset: Reset timed-out."
			"Resetting controller.\n");
		mpt_reset(mpt, /*reinit*/TRUE);
		return (ETIMEDOUT);
	} else if ((status & MPI_IOCSTATUS_MASK) != MPI_SCSI_STATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_bus_reset: TMF Status %d."
			"Resetting controller.\n", status);
		mpt_reset(mpt, /*reinit*/TRUE);
		return (EIO);
	}
	return (0);
}

static int
mpt_cam_event(struct mpt_softc *mpt, request_t *req,
	      MSG_EVENT_NOTIFY_REPLY *msg)
{
	switch(msg->Event & 0xFF) {
	case MPI_EVENT_UNIT_ATTENTION:
		mpt_prt(mpt, "Bus: 0x%02x TargetID: 0x%02x\n",
		    (msg->Data[0] >> 8) & 0xff, msg->Data[0] & 0xff);
		break;

	case MPI_EVENT_IOC_BUS_RESET:
		/* We generated a bus reset */
		mpt_prt(mpt, "IOC Bus Reset Port: %d\n",
		    (msg->Data[0] >> 8) & 0xff);
		xpt_async(AC_BUS_RESET, mpt->path, NULL);
		break;

	case MPI_EVENT_EXT_BUS_RESET:
		/* Someone else generated a bus reset */
		mpt_prt(mpt, "Ext Bus Reset\n");
		/*
		 * These replies don't return EventData like the MPI
		 * spec says they do
		 */	
		xpt_async(AC_BUS_RESET, mpt->path, NULL);
		break;

	case MPI_EVENT_RESCAN:
		/*
		 * In general this means a device has been added
		 * to the loop.
		 */
		mpt_prt(mpt, "Rescan Port: %d\n", (msg->Data[0] >> 8) & 0xff);
/*		xpt_async(AC_FOUND_DEVICE, path, NULL);  */
		break;

	case MPI_EVENT_LINK_STATUS_CHANGE:
		mpt_prt(mpt, "Port %d: LinkState: %s\n",
		    (msg->Data[1] >> 8) & 0xff,
		    ((msg->Data[0] & 0xff) == 0)?  "Failed" : "Active");
		break;

	case MPI_EVENT_LOOP_STATE_CHANGE:
		switch ((msg->Data[0] >> 16) & 0xff) {
		case 0x01:
			mpt_prt(mpt,
			    "Port 0x%x: FC LinkEvent: LIP(%02x,%02x) "
			    "(Loop Initialization)\n",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			switch ((msg->Data[0] >> 8) & 0xff) {
			case 0xF7:
				if ((msg->Data[0] & 0xff) == 0xF7) {
					printf("Device needs AL_PA\n");
				} else {
					printf("Device %02x doesn't like "
					    "FC performance\n",
					    msg->Data[0] & 0xFF);
				}
				break;
			case 0xF8:
				if ((msg->Data[0] & 0xff) == 0xF7) {
					printf("Device had loop failure at its "
					    "receiver prior to acquiring "
					    "AL_PA\n");
				} else {
					printf("Device %02x detected loop "
					    "failure at its receiver\n", 
					    msg->Data[0] & 0xFF);
				}
				break;
			default:
				printf("Device %02x requests that device "
				    "%02x reset itself\n", 
				    msg->Data[0] & 0xFF,
				    (msg->Data[0] >> 8) & 0xFF);
				break;
			}
			break;
		case 0x02:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: "
			    "LPE(%02x,%02x) (Loop Port Enable)\n",
			    (msg->Data[1] >> 8) & 0xff, /* Port */
			    (msg->Data[0] >>  8) & 0xff, /* Character 3 */
			    (msg->Data[0]      ) & 0xff  /* Character 4 */);
			break;
		case 0x03:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: "
			    "LPB(%02x,%02x) (Loop Port Bypass)\n",
			    (msg->Data[1] >> 8) & 0xff, /* Port */
			    (msg->Data[0] >> 8) & 0xff, /* Character 3 */
			    (msg->Data[0]     ) & 0xff  /* Character 4 */);
			break;
		default:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: Unknown "
			    "FC event (%02x %02x %02x)\n",
			    (msg->Data[1] >> 8) & 0xff, /* Port */
			    (msg->Data[0] >> 16) & 0xff, /* Event */
			    (msg->Data[0] >>  8) & 0xff, /* Character 3 */
			    (msg->Data[0]      ) & 0xff  /* Character 4 */);
		}
		break;

	case MPI_EVENT_LOGOUT:
		mpt_prt(mpt, "FC Logout Port: %d N_PortID: %02x\n",
		    (msg->Data[1] >> 8) & 0xff, msg->Data[0]);
		break;
	default:
		return (/*handled*/0);
	}
	return (/*handled*/1);
}

/*
 * Reply path for all SCSI I/O requests, called from our
 * interrupt handler by extracting our handler index from
 * the MsgContext field of the reply from the IOC.
 *
 * This routine is optimized for the common case of a
 * completion without error.  All exception handling is
 * offloaded to non-inlined helper routines to minimize
 * cache footprint.
 */
static int
mpt_scsi_reply_handler(struct mpt_softc *mpt, request_t *req,
		       MSG_DEFAULT_REPLY *reply_frame)
{
	MSG_SCSI_IO_REQUEST *scsi_req;
	union ccb *ccb;
	
	scsi_req = (MSG_SCSI_IO_REQUEST *)req->req_vbuf;
	ccb = req->ccb;
	if (ccb == NULL) {
		mpt_prt(mpt, "Completion without CCB. Flags %#x, Func %#x\n",
			req->state, scsi_req->Function);
		mpt_print_scsi_io_request(scsi_req);
		return (/*free_reply*/TRUE);
	}

	untimeout(mpt_timeout, ccb, ccb->ccb_h.timeout_ch);

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
		bus_dmamap_unload(mpt->buffer_dmat, req->dmap);
	}

	if (reply_frame == NULL) {
		/*
		 * Context only reply, completion
		 * without error status.
		 */
		ccb->csio.resid = 0;
		mpt_set_ccb_status(ccb, CAM_REQ_CMP);
		ccb->csio.scsi_status = SCSI_STATUS_OK;
	} else {
		mpt_scsi_reply_frame_handler(mpt, req, reply_frame);
	}

	if (mpt->outofbeer) {
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		mpt->outofbeer = 0;
		mpt_lprt(mpt, MPT_PRT_DEBUG, "THAWQ\n");
	}
	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	MPTLOCK_2_CAMLOCK(mpt);
	if (scsi_req->Function == MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH
	 && scsi_req->CDB[0] == INQUIRY
	 && (scsi_req->CDB[1] & SI_EVPD) == 0) {
		struct scsi_inquiry_data *inq;

		/*
		 * Fake out the device type so that only the
		 * pass-thru device will attach.
		 */
		inq = (struct scsi_inquiry_data *)ccb->csio.data_ptr;
		inq->device &= ~0x1F;
		inq->device |= T_NODEVICE;
	}
	xpt_done(ccb);
	CAMLOCK_2_MPTLOCK(mpt);
	if ((req->state & REQ_STATE_TIMEDOUT) == 0)
		TAILQ_REMOVE(&mpt->request_pending_list, req, links);
	else
		TAILQ_REMOVE(&mpt->request_timeout_list, req, links);

	if ((req->state & REQ_STATE_NEED_WAKEUP) == 0) {
		mpt_free_request(mpt, req);
		return (/*free_reply*/TRUE);
	}
	req->state &= ~REQ_STATE_QUEUED;
	req->state |= REQ_STATE_DONE;
	wakeup(req);
	return (/*free_reply*/TRUE);
}

static int
mpt_scsi_tmf_reply_handler(struct mpt_softc *mpt, request_t *req,
			   MSG_DEFAULT_REPLY *reply_frame)
{
	MSG_SCSI_TASK_MGMT_REPLY *tmf_reply;
	u_int			  status;

	mpt_lprt(mpt, MPT_PRT_DEBUG, "TMF Complete: req %p, reply %p\n",
		 req, reply_frame);
	KASSERT(req == mpt->tmf_req, ("TMF Reply not using mpt->tmf_req"));

	tmf_reply = (MSG_SCSI_TASK_MGMT_REPLY *)reply_frame;

	/* Record status of TMF for any waiters. */
	req->IOCStatus = tmf_reply->IOCStatus;
	status = le16toh(tmf_reply->IOCStatus);
	mpt_lprt(mpt, MPT_PRT_DEBUG, "TMF Complete: status 0x%x\n", status);
	TAILQ_REMOVE(&mpt->request_pending_list, req, links);
	if ((req->state & REQ_STATE_NEED_WAKEUP) != 0) {
		req->state |= REQ_STATE_DONE;
		wakeup(req);
	} else
		mpt->tmf_req->state = REQ_STATE_FREE;

	return (/*free_reply*/TRUE);
}

/*
 * Clean up all SCSI Initiator personality state in response
 * to a controller reset.
 */
static void
mpt_cam_ioc_reset(struct mpt_softc *mpt, int type)
{
	/*
	 * The pending list is already run down by
	 * the generic handler.  Perform the same
	 * operation on the timed out request list.
	 */
	mpt_complete_request_chain(mpt, &mpt->request_timeout_list,
				   MPI_IOCSTATUS_INVALID_STATE);

	/*
	 * Inform the XPT that a bus reset has occurred.
	 */
	xpt_async(AC_BUS_RESET, mpt->path, NULL);
}

/*
 * Parse additional completion information in the reply
 * frame for SCSI I/O requests.
 */
static int
mpt_scsi_reply_frame_handler(struct mpt_softc *mpt, request_t *req,
			     MSG_DEFAULT_REPLY *reply_frame)
{
	union ccb *ccb;
	MSG_SCSI_IO_REPLY *scsi_io_reply;
	u_int ioc_status;
	u_int sstate;
	u_int loginfo;

	MPT_DUMP_REPLY_FRAME(mpt, reply_frame);
	KASSERT(reply_frame->Function == MPI_FUNCTION_SCSI_IO_REQUEST
	     || reply_frame->Function == MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH,
		("MPT SCSI I/O Handler called with incorrect reply type"));
	KASSERT((reply_frame->MsgFlags & MPI_MSGFLAGS_CONTINUATION_REPLY) == 0,
		("MPT SCSI I/O Handler called with continuation reply"));

	scsi_io_reply = (MSG_SCSI_IO_REPLY *)reply_frame;
	ioc_status = le16toh(scsi_io_reply->IOCStatus);
	loginfo = ioc_status & MPI_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE;
	ioc_status &= MPI_IOCSTATUS_MASK;
	sstate = scsi_io_reply->SCSIState;

	ccb = req->ccb;
	ccb->csio.resid =
	    ccb->csio.dxfer_len - le32toh(scsi_io_reply->TransferCount);

	if ((sstate & MPI_SCSI_STATE_AUTOSENSE_VALID) != 0
	 && (ccb->ccb_h.flags & (CAM_SENSE_PHYS | CAM_SENSE_PTR)) == 0) {
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
		ccb->csio.sense_resid =
		    ccb->csio.sense_len - scsi_io_reply->SenseCount;
		bcopy(req->sense_vbuf, &ccb->csio.sense_data,
		      min(ccb->csio.sense_len, scsi_io_reply->SenseCount));
	}

	if ((sstate & MPI_SCSI_STATE_QUEUE_TAG_REJECTED) != 0) {
		/*
		 * Tag messages rejected, but non-tagged retry
		 * was successful.
XXXX
		mpt_set_tags(mpt, devinfo, MPT_QUEUE_NONE);
		 */
	}

	switch(ioc_status) {
	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		/*
		 * XXX
		 * Linux driver indicates that a zero
		 * transfer length with this error code
		 * indicates a CRC error.
		 *
		 * No need to swap the bytes for checking
		 * against zero.
		 */
		if (scsi_io_reply->TransferCount == 0) {
			mpt_set_ccb_status(ccb, CAM_UNCOR_PARITY);
			break;
		}
		/* FALLTHROUGH */
	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:
	case MPI_IOCSTATUS_SUCCESS:
	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:
		if ((sstate & MPI_SCSI_STATE_NO_SCSI_STATUS) != 0) {
			/*
			 * Status was never returned for this transaction.
			 */
			mpt_set_ccb_status(ccb, CAM_UNEXP_BUSFREE);
		} else if (scsi_io_reply->SCSIStatus != SCSI_STATUS_OK) {
			ccb->csio.scsi_status = scsi_io_reply->SCSIStatus;
			mpt_set_ccb_status(ccb, CAM_SCSI_STATUS_ERROR);
			if ((sstate & MPI_SCSI_STATE_AUTOSENSE_FAILED) != 0)
				mpt_set_ccb_status(ccb, CAM_AUTOSENSE_FAIL);
		} else if ((sstate & MPI_SCSI_STATE_RESPONSE_INFO_VALID) != 0) {

			/* XXX Handle SPI-Packet and FCP-2 reponse info. */
			mpt_set_ccb_status(ccb, CAM_REQ_CMP_ERR);
		} else
			mpt_set_ccb_status(ccb, CAM_REQ_CMP);
		break;
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:
		mpt_set_ccb_status(ccb, CAM_DATA_RUN_ERR);
		break;
	case MPI_IOCSTATUS_SCSI_IO_DATA_ERROR:
		mpt_set_ccb_status(ccb, CAM_UNCOR_PARITY);
		break;
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		/*
		 * Since selection timeouts and "device really not
		 * there" are grouped into this error code, report
		 * selection timeout.  Selection timeouts are
		 * typically retried before giving up on the device
		 * whereas "device not there" errors are considered
		 * unretryable.
		 */
		mpt_set_ccb_status(ccb, CAM_SEL_TIMEOUT);
		break;
	case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		mpt_set_ccb_status(ccb, CAM_SEQUENCE_FAIL);
		break;
	case MPI_IOCSTATUS_SCSI_INVALID_BUS:
		mpt_set_ccb_status(ccb, CAM_PATH_INVALID);
		break;
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:
		mpt_set_ccb_status(ccb, CAM_TID_INVALID);
		break;
	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		ccb->ccb_h.status = CAM_UA_TERMIO;
		break;
	case MPI_IOCSTATUS_INVALID_STATE:
		/*
		 * The IOC has been reset.  Emulate a bus reset.
		 */
		/* FALLTHROUGH */
	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:
		ccb->ccb_h.status = CAM_SCSI_BUS_RESET; 
		break;
	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:
		/*
		 * Don't clobber any timeout status that has
		 * already been set for this transaction.  We
		 * want the SCSI layer to be able to differentiate
		 * between the command we aborted due to timeout
		 * and any innocent bystanders.
		 */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG)
			break;
		mpt_set_ccb_status(ccb, CAM_REQ_TERMIO);
		break;

	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:
		mpt_set_ccb_status(ccb, CAM_RESRC_UNAVAIL);
		break;
	case MPI_IOCSTATUS_BUSY:
		mpt_set_ccb_status(ccb, CAM_BUSY);
		break;
	case MPI_IOCSTATUS_INVALID_FUNCTION:
	case MPI_IOCSTATUS_INVALID_SGL:
	case MPI_IOCSTATUS_INTERNAL_ERROR:
	case MPI_IOCSTATUS_INVALID_FIELD:
	default:
		/* XXX
		 * Some of the above may need to kick
		 * of a recovery action!!!!
		 */
		ccb->ccb_h.status = CAM_UNREC_HBA_ERROR;
		break;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
		mpt_freeze_ccb(ccb);

	return (/*free_reply*/TRUE);
}

static void
mpt_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	mpt_softc *mpt;
	struct	ccb_trans_settings *cts;
	u_int	tgt;
	int	raid_passthru;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("mpt_action\n"));

	mpt = (struct mpt_softc *)cam_sim_softc(sim);
	raid_passthru = (sim == mpt->phydisk_sim);

	tgt = ccb->ccb_h.target_id;
	if (raid_passthru
	 && ccb->ccb_h.func_code != XPT_PATH_INQ
	 && ccb->ccb_h.func_code != XPT_RESET_BUS) {
		CAMLOCK_2_MPTLOCK(mpt);
		if (mpt_map_physdisk(mpt, ccb, &tgt) != 0) {
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			MPTLOCK_2_CAMLOCK(mpt);
			xpt_done(ccb);
			return;
		}
		MPTLOCK_2_CAMLOCK(mpt);
	}

	ccb->ccb_h.ccb_mpt_ptr = mpt;

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
		/* Max supported CDB length is 16 bytes */
		/* XXX Unless we implement the new 32byte message type */
		if (ccb->csio.cdb_len >
		    sizeof (((PTR_MSG_SCSI_IO_REQUEST)0)->CDB)) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		mpt_start(sim, ccb);
		break;

	case XPT_RESET_BUS:
		mpt_lprt(mpt, MPT_PRT_DEBUG, "XPT_RESET_BUS\n");
		if (!raid_passthru) {
			CAMLOCK_2_MPTLOCK(mpt);
			(void)mpt_bus_reset(mpt, /*sleep_ok*/FALSE);
			MPTLOCK_2_CAMLOCK(mpt);
		}
		/*
		 * mpt_bus_reset is always successful in that it
		 * will fall back to a hard reset should a bus
		 * reset attempt fail.
		 */
		mpt_set_ccb_status(ccb, CAM_REQ_CMP);
		xpt_done(ccb);
		break;
		
	case XPT_ABORT:
		/*
		 * XXX: Need to implement
		 */
		ccb->ccb_h.status = CAM_UA_ABORT;
		xpt_done(ccb);
		break;

#ifdef	CAM_NEW_TRAN_CODE
#define	IS_CURRENT_SETTINGS(c)	(c->type == CTS_TYPE_CURRENT_SETTINGS)
#else
#define	IS_CURRENT_SETTINGS(c)	(c->flags & CCB_TRANS_CURRENT_SETTINGS)
#endif
#define	DP_DISC_ENABLE	0x1
#define	DP_DISC_DISABL	0x2
#define	DP_DISC		(DP_DISC_ENABLE|DP_DISC_DISABL)

#define	DP_TQING_ENABLE	0x4
#define	DP_TQING_DISABL	0x8
#define	DP_TQING	(DP_TQING_ENABLE|DP_TQING_DISABL)

#define	DP_WIDE		0x10
#define	DP_NARROW	0x20
#define	DP_WIDTH	(DP_WIDE|DP_NARROW)

#define	DP_SYNC		0x40

	case XPT_SET_TRAN_SETTINGS:	/* Nexus Settings */
		cts = &ccb->cts;
		if (!IS_CURRENT_SETTINGS(cts)) {
			mpt_prt(mpt, "Attempt to set User settings\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		if (mpt->is_fc == 0) {
			uint8_t dval = 0;
			u_int period = 0, offset = 0;
#ifndef	CAM_NEW_TRAN_CODE
			if (cts->valid & CCB_TRANS_DISC_VALID) {
				dval |= DP_DISC_ENABLE;
			}
			if (cts->valid & CCB_TRANS_TQ_VALID) {
				dval |= DP_TQING_ENABLE;
			}
			if (cts->valid & CCB_TRANS_BUS_WIDTH_VALID) {
				if (cts->bus_width)
					dval |= DP_WIDE;
				else
					dval |= DP_NARROW;
			}
			/*
			 * Any SYNC RATE of nonzero and SYNC_OFFSET
			 * of nonzero will cause us to go to the
			 * selected (from NVRAM) maximum value for
			 * this device. At a later point, we'll
			 * allow finer control.
			 */
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) &&
			    (cts->valid & CCB_TRANS_SYNC_OFFSET_VALID)) {
				dval |= DP_SYNC;
				period = cts->sync_period;
				offset = cts->sync_offset;
			}
#else
			struct ccb_trans_settings_scsi *scsi =
			    &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi =
			    &cts->xport_specific.spi;

			if ((spi->valid & CTS_SPI_VALID_DISC) != 0) {
				if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) != 0)
					dval |= DP_DISC_ENABLE;
				else
					dval |= DP_DISC_DISABL;
			}

			if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
				if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
					dval |= DP_TQING_ENABLE;
				else
					dval |= DP_TQING_DISABL;
			}

			if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0) {
				if (spi->bus_width == MSG_EXT_WDTR_BUS_16_BIT)
					dval |= DP_WIDE;
				else
					dval |= DP_NARROW;
			}

			if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) &&
			    (spi->valid & CTS_SPI_VALID_SYNC_RATE) &&
			    (spi->sync_period && spi->sync_offset)) {
				dval |= DP_SYNC;
				period = spi->sync_period;
				offset = spi->sync_offset;
			}
#endif
			CAMLOCK_2_MPTLOCK(mpt);
			if (dval & DP_DISC_ENABLE) {
				mpt->mpt_disc_enable |= (1 << tgt);
			} else if (dval & DP_DISC_DISABL) {
				mpt->mpt_disc_enable &= ~(1 << tgt);
			}
			if (dval & DP_TQING_ENABLE) {
				mpt->mpt_tag_enable |= (1 << tgt);
			} else if (dval & DP_TQING_DISABL) {
				mpt->mpt_tag_enable &= ~(1 << tgt);
			}
			if (dval & DP_WIDTH) {
				if (mpt_setwidth(mpt, tgt, dval & DP_WIDE)) {
mpt_prt(mpt, "Set width Failed!\n");
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					MPTLOCK_2_CAMLOCK(mpt);
					xpt_done(ccb);
					break;
				}
			}
			if (dval & DP_SYNC) {
				if (mpt_setsync(mpt, tgt, period, offset)) {
mpt_prt(mpt, "Set sync Failed!\n");
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					MPTLOCK_2_CAMLOCK(mpt);
					xpt_done(ccb);
					break;
				}
			}
			MPTLOCK_2_CAMLOCK(mpt);
			mpt_lprt(mpt, MPT_PRT_DEBUG,
				 "SET tgt %d flags %x period %x off %x\n",
				 tgt, dval, period, offset);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:
		cts = &ccb->cts;
		if (mpt->is_fc) {
#ifndef	CAM_NEW_TRAN_CODE
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
#else
			struct ccb_trans_settings_fc *fc =
			    &cts->xport_specific.fc;

			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_FC;
			cts->transport_version = 0;

			fc->valid = CTS_FC_VALID_SPEED;
			fc->bitrate = 100000;	/* XXX: Need for 2Gb/s */
			/* XXX: need a port database for each target */
#endif
		} else {
#ifdef	CAM_NEW_TRAN_CODE
			struct ccb_trans_settings_scsi *scsi =
			    &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi =
			    &cts->xport_specific.spi;
#endif
			uint8_t dval, pval, oval;
			int rv;

			/*
			 * We aren't going off of Port PAGE2 params for
			 * tagged queuing or disconnect capabilities
			 * for current settings. For goal settings,
			 * we assert all capabilities- we've had some
			 * problems with reading NVRAM data.
			 */
			if (IS_CURRENT_SETTINGS(cts)) {
				CONFIG_PAGE_SCSI_DEVICE_0 tmp;
				dval = 0;

				tmp = mpt->mpt_dev_page0[tgt];
				CAMLOCK_2_MPTLOCK(mpt);
				rv = mpt_read_cur_cfg_page(mpt, tgt,
							   &tmp.Header,
							   sizeof(tmp),
							   /*sleep_ok*/FALSE,
							   /*timeout_ms*/5000);
				if (rv) {
					mpt_prt(mpt,
					    "cannot get target %d DP0\n", tgt);
				}
				mpt_lprt(mpt, MPT_PRT_DEBUG,
					 "SPI Tgt %d Page 0: NParms %x "
					 "Information %x\n", tgt,
					 tmp.NegotiatedParameters,
					 tmp.Information);
				MPTLOCK_2_CAMLOCK(mpt);

				if (tmp.NegotiatedParameters & 
				    MPI_SCSIDEVPAGE0_NP_WIDE)
					dval |= DP_WIDE;

				if (mpt->mpt_disc_enable & (1 << tgt)) {
					dval |= DP_DISC_ENABLE;
				}
				if (mpt->mpt_tag_enable & (1 << tgt)) {
					dval |= DP_TQING_ENABLE;
				}
				oval = (tmp.NegotiatedParameters >> 16) & 0xff;
				pval = (tmp.NegotiatedParameters >>  8) & 0xff;
			} else {
				/*
				 * XXX: Fix wrt NVRAM someday. Attempts
				 * XXX: to read port page2 device data
				 * XXX: just returns zero in these areas.
				 */
				dval = DP_WIDE|DP_DISC|DP_TQING;
				oval = (mpt->mpt_port_page0.Capabilities >> 16);
				pval = (mpt->mpt_port_page0.Capabilities >>  8);
			}
#ifndef	CAM_NEW_TRAN_CODE
			cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);
			if (dval & DP_DISC_ENABLE) {
				cts->flags |= CCB_TRANS_DISC_ENB;
			}
			if (dval & DP_TQING_ENABLE) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			if (dval & DP_WIDE) {
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			cts->valid = CCB_TRANS_BUS_WIDTH_VALID |
			    CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			if (oval) {
				cts->sync_period = pval;
				cts->sync_offset = oval;
				cts->valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
#else
			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_SPI;
			cts->transport_version = 2;

			scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
			spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;
			if (dval & DP_DISC_ENABLE) {
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
			}
			if (dval & DP_TQING_ENABLE) {
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
			}
			if (oval && pval) {
				spi->sync_offset = oval;
				spi->sync_period = pval;
				spi->valid |= CTS_SPI_VALID_SYNC_OFFSET;
				spi->valid |= CTS_SPI_VALID_SYNC_RATE;
			}
			spi->valid |= CTS_SPI_VALID_BUS_WIDTH;
			if (dval & DP_WIDE) {
				spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			if (cts->ccb_h.target_lun != CAM_LUN_WILDCARD) {
				scsi->valid = CTS_SCSI_VALID_TQ;
				spi->valid |= CTS_SPI_VALID_DISC;
			} else {
				scsi->valid = 0;
			}
#endif
			mpt_lprt(mpt, MPT_PRT_DEBUG,
				 "GET %s tgt %d flags %x period %x offset %x\n",
				 IS_CURRENT_SETTINGS(cts)
			       ? "ACTIVE" : "NVRAM",
				 tgt, dval, pval, oval);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;

		ccg = &ccb->ccg;
		if (ccg->block_size == 0) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}

		mpt_calc_geometry(ccg, /*extended*/1);
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->target_sprt = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_lun = 7;
		cpi->bus_id = cam_sim_bus(sim);
		/* XXX Report base speed more accurately for FC/SAS, etc.*/
		if (raid_passthru) {
			cpi->max_target = mpt->ioc_page2->MaxPhysDisks;
			cpi->hba_misc = PIM_NOBUSRESET;
			cpi->initiator_id = cpi->max_target + 1;
			cpi->hba_inquiry = PI_TAG_ABLE;
			if (mpt->is_fc) {
				cpi->base_transfer_speed = 100000;
			} else {
				cpi->base_transfer_speed = 3300;
				cpi->hba_inquiry |=
				    PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			}
		} else if (mpt->is_fc) {
			cpi->max_target = 255;
			cpi->hba_misc = PIM_NOBUSRESET;
			cpi->initiator_id = cpi->max_target + 1;
			cpi->base_transfer_speed = 100000;
			cpi->hba_inquiry = PI_TAG_ABLE;
		} else {
			cpi->initiator_id = mpt->mpt_ini_id;
			cpi->base_transfer_speed = 3300;
			cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			cpi->hba_misc = 0;
			cpi->max_target = 15;
		}

		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "LSI", HBA_IDLEN);
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

static int
mpt_setwidth(struct mpt_softc *mpt, int tgt, int onoff)
{
	CONFIG_PAGE_SCSI_DEVICE_1 tmp;
	int rv;

	tmp = mpt->mpt_dev_page1[tgt];
	if (onoff) {
		tmp.RequestedParameters |= MPI_SCSIDEVPAGE1_RP_WIDE;
	} else {
		tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_WIDE;
	}
	rv = mpt_write_cur_cfg_page(mpt, tgt, &tmp.Header, sizeof(tmp),
				    /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "mpt_setwidth: write cur page failed\n");
		return (-1);
	}
	rv = mpt_read_cur_cfg_page(mpt, tgt, &tmp.Header, sizeof(tmp),
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "mpt_setwidth: read cur page failed\n");
		return (-1);
	}
	mpt->mpt_dev_page1[tgt] = tmp;
	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "SPI Target %d Page 1: RequestedParameters %x Config %x\n",
		 tgt, mpt->mpt_dev_page1[tgt].RequestedParameters,
		 mpt->mpt_dev_page1[tgt].Configuration);
	return (0);
}

static int
mpt_setsync(struct mpt_softc *mpt, int tgt, int period, int offset)
{
	CONFIG_PAGE_SCSI_DEVICE_1 tmp;
	int rv;

	tmp = mpt->mpt_dev_page1[tgt];
	tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK;
	tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK;
	tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_DT;
	tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_QAS;
	tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_IU;
	/*
	 * XXX: For now, we're ignoring specific settings
	 */
	if (period && offset) {
		int factor, offset, np;
		factor = (mpt->mpt_port_page0.Capabilities >> 8) & 0xff;
		offset = (mpt->mpt_port_page0.Capabilities >> 16) & 0xff;
		np = 0;
		if (factor < 0x9) {
			np |= MPI_SCSIDEVPAGE1_RP_QAS;
			np |= MPI_SCSIDEVPAGE1_RP_IU;
		}
		if (factor < 0xa) {
			np |= MPI_SCSIDEVPAGE1_RP_DT;
		}
		np |= (factor << 8) | (offset << 16);
		tmp.RequestedParameters |= np;
	}
	rv = mpt_write_cur_cfg_page(mpt, tgt, &tmp.Header, sizeof(tmp),
				    /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "mpt_setsync: write cur page failed\n");
		return (-1);
	}
	rv = mpt_read_cur_cfg_page(mpt, tgt, &tmp.Header, sizeof(tmp),
				   /*sleep_ok*/FALSE, /*timeout_ms*/500);
	if (rv) {
		mpt_prt(mpt, "mpt_setsync: read cur page failed\n");
		return (-1);
	}
	mpt->mpt_dev_page1[tgt] = tmp;
	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "SPI Target %d Page 1: RParams %x Config %x\n",
		 tgt, mpt->mpt_dev_page1[tgt].RequestedParameters,
		 mpt->mpt_dev_page1[tgt].Configuration);
	return (0);
}

static void
mpt_calc_geometry(struct ccb_calc_geometry *ccg, int extended)
{
#if __FreeBSD_version >= 500000
	cam_calc_geometry(ccg, extended);
#else
	uint32_t size_mb;
	uint32_t secs_per_cylinder;

	size_mb = ccg->volume_size / ((1024L * 1024L) / ccg->block_size);
	if (size_mb > 1024 && extended) {
		ccg->heads = 255;
		ccg->secs_per_track = 63;
	} else {
		ccg->heads = 64;
		ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	ccg->ccb_h.status = CAM_REQ_CMP;
#endif
}

/****************************** Timeout Recovery ******************************/
static int
mpt_spawn_recovery_thread(struct mpt_softc *mpt)
{
	int error;

	error = mpt_kthread_create(mpt_recovery_thread, mpt,
	    &mpt->recovery_thread, /*flags*/0,
	    /*altstack*/0, "mpt_recovery%d", mpt->unit);
	return (error);
}

/*
 * Lock is not held on entry.
 */
static void
mpt_terminate_recovery_thread(struct mpt_softc *mpt)
{

	MPT_LOCK(mpt);
	if (mpt->recovery_thread == NULL) {
		MPT_UNLOCK(mpt);
		return;
	}
	mpt->shutdwn_recovery = 1;
	wakeup(mpt);
	/*
	 * Sleep on a slightly different location
	 * for this interlock just for added safety.
	 */
	mpt_sleep(mpt, &mpt->recovery_thread, PUSER, "thtrm", 0);
	MPT_UNLOCK(mpt);
}

static void
mpt_recovery_thread(void *arg)
{
	struct mpt_softc *mpt;

#if __FreeBSD_version >= 500000
	mtx_lock(&Giant);
#endif
	mpt = (struct mpt_softc *)arg;
	MPT_LOCK(mpt);
	for (;;) {

		if (TAILQ_EMPTY(&mpt->request_timeout_list) != 0
		 && mpt->shutdwn_recovery == 0)
			mpt_sleep(mpt, mpt, PUSER, "idle", 0);

		if (mpt->shutdwn_recovery != 0)
			break;

		MPT_UNLOCK(mpt);
		mpt_recover_commands(mpt);
		MPT_LOCK(mpt);
	}
	mpt->recovery_thread = NULL;
	wakeup(&mpt->recovery_thread);
	MPT_UNLOCK(mpt);
#if __FreeBSD_version >= 500000
	mtx_unlock(&Giant);
#endif
	kthread_exit(0);
}

static int
mpt_scsi_send_tmf(struct mpt_softc *mpt, u_int type,
		  u_int flags, u_int channel, u_int target, u_int lun,
		  u_int abort_ctx, int sleep_ok)
{
	MSG_SCSI_TASK_MGMT *tmf_req;
	int		    error;

	/*
	 * Wait for any current TMF request to complete.
	 * We're only allowed to issue one TMF at a time.
	 */
	error = mpt_wait_req(mpt, mpt->tmf_req, REQ_STATE_FREE, REQ_STATE_MASK,
	    sleep_ok, MPT_TMF_MAX_TIMEOUT);
	if (error != 0) {
		mpt_reset(mpt, /*reinit*/TRUE);
		return (ETIMEDOUT);
	}

	mpt->tmf_req->state = REQ_STATE_ALLOCATED|REQ_STATE_QUEUED;
	TAILQ_INSERT_HEAD(&mpt->request_pending_list, mpt->tmf_req, links);

	tmf_req = (MSG_SCSI_TASK_MGMT *)mpt->tmf_req->req_vbuf;
	bzero(tmf_req, sizeof(*tmf_req));
	tmf_req->TargetID = target;
	tmf_req->Bus = channel;
	tmf_req->ChainOffset = 0;
	tmf_req->Function = MPI_FUNCTION_SCSI_TASK_MGMT;
	tmf_req->Reserved = 0;
	tmf_req->TaskType = type;
	tmf_req->Reserved1 = 0;
	tmf_req->MsgFlags = flags;
	tmf_req->MsgContext =
	    htole32(mpt->tmf_req->index | scsi_tmf_handler_id);
	bzero(&tmf_req->LUN, sizeof(tmf_req->LUN) + sizeof(tmf_req->Reserved2));
	tmf_req->LUN[1] = lun;
	tmf_req->TaskMsgContext = abort_ctx;

	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "Issuing TMF %p with MsgContext of 0x%x\n", tmf_req,
		 tmf_req->MsgContext);
	if (mpt->verbose > MPT_PRT_DEBUG)
		mpt_print_request(tmf_req);

	error = mpt_send_handshake_cmd(mpt, sizeof(*tmf_req), tmf_req);
	if (error != 0)
		mpt_reset(mpt, /*reinit*/TRUE);
	return (error);
}

/*
 * When a command times out, it is placed on the requeust_timeout_list
 * and we wake our recovery thread.  The MPT-Fusion architecture supports
 * only a single TMF operation at a time, so we serially abort/bdr, etc,
 * the timedout transactions.  The next TMF is issued either by the
 * completion handler of the current TMF waking our recovery thread,
 * or the TMF timeout handler causing a hard reset sequence.
 */
static void
mpt_recover_commands(struct mpt_softc *mpt)
{
	request_t	   *req;
	union ccb	   *ccb;
	int		    error;

	MPT_LOCK(mpt);

	/*
	 * Flush any commands whose completion coincides
	 * with their timeout.
	 */
	mpt_intr(mpt);

	if (TAILQ_EMPTY(&mpt->request_timeout_list) != 0) {
		/*
		 * The timedout commands have already
		 * completed.  This typically means
		 * that either the timeout value was on
		 * the hairy edge of what the device
		 * requires or - more likely - interrupts
		 * are not happening.
		 */
		mpt_prt(mpt, "Timedout requests already complete. "
                       "Interrupts may not be functioning.\n");
                MPT_UNLOCK(mpt);
                return;
	}

	/*
	 * We have no visibility into the current state of the
	 * controller, so attempt to abort the commands in the
	 * order they timed-out.
	 */
	while ((req = TAILQ_FIRST(&mpt->request_timeout_list)) != NULL) {
		u_int status;

		mpt_prt(mpt, "Attempting to Abort Req %p\n", req);

		ccb = req->ccb;
		mpt_set_ccb_status(ccb, CAM_CMD_TIMEOUT);
		error = mpt_scsi_send_tmf(mpt,
		    MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK,
		    /*MsgFlags*/0, mpt->bus, ccb->ccb_h.target_id,
		    ccb->ccb_h.target_lun,
		    htole32(req->index | scsi_io_handler_id), /*sleep_ok*/TRUE);

		if (error != 0) {
			/*
			 * mpt_scsi_send_tmf hard resets on failure, so no
			 * need to do so here.  Our queue should be emptied
			 * by the hard reset.
			 */
			continue;
		}

		error = mpt_wait_req(mpt, mpt->tmf_req, REQ_STATE_DONE,
		    REQ_STATE_DONE, /*sleep_ok*/TRUE, /*time_ms*/5000);

		status = mpt->tmf_req->IOCStatus;
		if (error != 0) {

			/*
			 * If we've errored out and the transaction is still
			 * pending, reset the controller.
			 */
			mpt_prt(mpt, "mpt_recover_commands: Abort timed-out."
				"Resetting controller\n");
			mpt_reset(mpt, /*reinit*/TRUE);
			continue;
		}

		/*
		 * TMF is complete.
		 */
		mpt->tmf_req->state = REQ_STATE_FREE;
		if ((status & MPI_IOCSTATUS_MASK) == MPI_SCSI_STATUS_SUCCESS)
			continue;

		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "mpt_recover_commands: Abort Failed "
			 "with status 0x%x\n.  Resetting bus", status);

		/*
		 * If the abort attempt fails for any reason, reset the bus.
		 * We should find all of the timed-out commands on our
		 * list are in the done state after this completes.
		 */
		mpt_bus_reset(mpt, /*sleep_ok*/TRUE);
	}
	
	MPT_UNLOCK(mpt);
}
