/* $FreeBSD$ */
/*
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

#include <dev/mpt/mpt_freebsd.h>

static void mpt_poll(struct cam_sim *);
static timeout_t mpttimeout;
static timeout_t mpttimeout2;
static void mpt_action(struct cam_sim *, union ccb *);
static int mpt_setwidth(mpt_softc_t *, int, int);
static int mpt_setsync(mpt_softc_t *, int, int, int);

void
mpt_cam_attach(mpt_softc_t *mpt)
{
	struct cam_devq *devq;
	struct cam_sim *sim;
	int maxq;

	mpt->bus = 0;
	maxq = (mpt->mpt_global_credits < MPT_MAX_REQUESTS(mpt))?
	    mpt->mpt_global_credits : MPT_MAX_REQUESTS(mpt);


	/*
	 * Create the device queue for our SIM(s).
	 */
	
	devq = cam_simq_alloc(maxq);
	if (devq == NULL) {
		return;
	}

	/*
	 * Construct our SIM entry.
	 */
	sim = cam_sim_alloc(mpt_action, mpt_poll, "mpt", mpt,
	    mpt->unit, 1, maxq, devq);
	if (sim == NULL) {
		cam_simq_free(devq);
		return;
	}

	/*
	 * Register exactly the bus.
	 */

	if (xpt_bus_register(sim, 0) != CAM_SUCCESS) {
		cam_sim_free(sim, TRUE);
		return;
	}

	if (xpt_create_path(&mpt->path, NULL, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, TRUE);
		return;
	}
	mpt->sim = sim;
}

void
mpt_cam_detach(mpt_softc_t *mpt)
{
	if (mpt->sim != NULL) {
		xpt_free_path(mpt->path);
		xpt_bus_deregister(cam_sim_path(mpt->sim));
		cam_sim_free(mpt->sim, TRUE);
		mpt->sim = NULL;
	}
}

/* This routine is used after a system crash to dump core onto the
 * swap device.
 */
static void
mpt_poll(struct cam_sim *sim)
{
	mpt_softc_t *mpt = (mpt_softc_t *) cam_sim_softc(sim);
	MPT_LOCK(mpt);
	mpt_intr(mpt);
	MPT_UNLOCK(mpt);
}

/*
 * This routine is called if the 9x9 does not return completion status
 * for a command after a CAM specified time.
 */
static void
mpttimeout(void *arg)
{
	request_t *req;
	union ccb *ccb = arg;
	u_int32_t oseq;
	mpt_softc_t *mpt;

	mpt = ccb->ccb_h.ccb_mpt_ptr;
	MPT_LOCK(mpt);
	req = ccb->ccb_h.ccb_req_ptr;
	oseq = req->sequence;
	mpt->timeouts++;
	if (mpt_intr(mpt)) {
		if (req->sequence != oseq) {
			mpt_prt(mpt, "bullet missed in timeout");
			MPT_UNLOCK(mpt);
			return;
		}
		mpt_prt(mpt, "bullet U-turned in timeout: got us");
	}
	mpt_prt(mpt,
	    "time out on request index = 0x%02x sequence = 0x%08x",
	    req->index, req->sequence);
        mpt_check_doorbell(mpt);
	mpt_prt(mpt, "Status %08x; Mask %08x; Doorbell %08x",
		mpt_read(mpt, MPT_OFFSET_INTR_STATUS),
		mpt_read(mpt, MPT_OFFSET_INTR_MASK),
		mpt_read(mpt, MPT_OFFSET_DOORBELL) );
	printf("request state %s\n", mpt_req_state(req->debug)); 
	if (ccb != req->ccb) {
		printf("time out: ccb %p != req->ccb %p\n",
			ccb,req->ccb);
	}
	mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);
	req->debug = REQ_TIMEOUT;
	req->ccb = NULL;
	req->link.sle_next = (void *) mpt;
	(void) timeout(mpttimeout2, (caddr_t)req, hz / 10);
	ccb->ccb_h.status = CAM_CMD_TIMEOUT;
	ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
	mpt->outofbeer = 0;
	MPTLOCK_2_CAMLOCK(mpt);
	xpt_done(ccb);
	CAMLOCK_2_MPTLOCK(mpt);
	MPT_UNLOCK(mpt);
}

static void
mpttimeout2(void *arg)
{
	request_t *req = arg;
	if (req->debug == REQ_TIMEOUT) {
		mpt_softc_t *mpt = (mpt_softc_t *) req->link.sle_next;
		MPT_LOCK(mpt);
		mpt_free_request(mpt, req);
		MPT_UNLOCK(mpt);
	}
}

/*
 * Callback routine from "bus_dmamap_load" or in simple case called directly.
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
	mpt_softc_t *mpt;
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
			mpt_prt(mpt, "bus_dmamap_load returned %d", error);
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
		u_int32_t flags;
		bus_dmasync_op_t op;
		SGE_CHAIN32 *ce;

		mpt_req->DataLength = ccb->csio.dxfer_len;
		flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

		se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		for (i = 0; i < MPT_NSGL_FIRST(mpt) - 1; i++, se++, dm_segs++) {
			u_int32_t tf;

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
			} else {
				ntodo = nleft;
				ce->NextChainOffset = 0;
			}
			ce->Length = ntodo * sizeof (SGE_SIMPLE32);
			ce->Address = req->req_pbuf +
			    ((char *)se - (char *)mpt_req);
			ce->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
			for (i = 0; i < ntodo; i++, se++, dm_segs++) {
				u_int32_t tf;

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
		u_int32_t flags;
		bus_dmasync_op_t op;

		mpt_req->DataLength = ccb->csio.dxfer_len;
		flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

		/* Copy the segments into our SG list */
		se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		for (i = 0; i < nseg; i++, se++, dm_segs++) {
			u_int32_t tf;

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
	MPTLOCK_2_CAMLOCK(mpt);
	if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
		ccb->ccb_h.timeout_ch =
			timeout(mpttimeout, (caddr_t)ccb,
				(ccb->ccb_h.timeout * hz) / 1000);
	} else {
		callout_handle_init(&ccb->ccb_h.timeout_ch);
	}
	if (mpt->verbose > 1)
		mpt_print_scsi_io_request(mpt_req);
	mpt_send_cmd(mpt, req);
	MPTLOCK_2_CAMLOCK(mpt);
}

static void
mpt_start(union ccb *ccb)
{
	request_t *req;
	struct mpt_softc *mpt;
	MSG_SCSI_IO_REQUEST *mpt_req;
	struct ccb_scsiio *csio = &ccb->csio;
	struct ccb_hdr *ccbh = &ccb->ccb_h;

	/* Get the pointer for the physical addapter */
	mpt = ccb->ccb_h.ccb_mpt_ptr;

	CAMLOCK_2_MPTLOCK(mpt);
	/* Get a request structure off the free list */
	if ((req = mpt_get_request(mpt)) == NULL) {
		if (mpt->outofbeer == 0) {
			mpt->outofbeer = 1;
			xpt_freeze_simq(mpt->sim, 1);
			if (mpt->verbose > 1) {
				mpt_prt(mpt, "FREEZEQ");
			}
		}
		MPTLOCK_2_CAMLOCK(mpt);
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}
	MPTLOCK_2_CAMLOCK(mpt);

	/* Link the ccb and the request structure so we can find */
	/* the other knowing either the request or the ccb		 */
	req->ccb = ccb;
	ccb->ccb_h.ccb_req_ptr = req;

	/* Now we build the command for the IOC */
	mpt_req = req->req_vbuf;
	bzero(mpt_req, sizeof *mpt_req);

	mpt_req->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	mpt_req->Bus = mpt->bus;

	mpt_req->SenseBufferLength =
		(csio->sense_len < MPT_SENSE_SIZE) ?
		 csio->sense_len : MPT_SENSE_SIZE;

	/* We use the message context to find the request structure when we */
	/* Get the command competion interrupt from the FC IOC.				*/
	mpt_req->MsgContext = req->index;

	/* Which physical device to do the I/O on */
	mpt_req->TargetID = ccb->ccb_h.target_id;
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
				 * one or more physical pages.
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
			 * These case could be easily done but they are not
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
mpt_bus_reset(union ccb *ccb)
{
	int error;
	request_t *req;
	mpt_softc_t *mpt;
	MSG_SCSI_TASK_MGMT *reset_req;

	/* Get the pointer for the physical adapter */
	mpt = ccb->ccb_h.ccb_mpt_ptr;

	/* Get a request structure off the free list */
	if ((req = mpt_get_request(mpt)) == NULL) {
		return (CAM_REQUEUE_REQ);
	}

	/* Link the ccb and the request structure so we can find */
	/* the other knowing either the request or the ccb		 */
	req->ccb = ccb;
	ccb->ccb_h.ccb_req_ptr = req;

	reset_req = req->req_vbuf;
	bzero(reset_req, sizeof *reset_req);

	reset_req->Function = MPI_FUNCTION_SCSI_TASK_MGMT;
	reset_req->MsgContext = req->index;
	reset_req->TaskType = MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS;
	if (mpt->is_fc) {
		/*
		 * Should really be TARGET_RESET_OPTION
		 */
		reset_req->MsgFlags =
		    MPI_SCSITASKMGMT_MSGFLAGS_LIP_RESET_OPTION;
	}
	/* Which physical device Reset */
	reset_req->TargetID = ccb->ccb_h.target_id;
	reset_req->LUN[1] = ccb->ccb_h.target_lun;

	ccb->ccb_h.status |= CAM_SIM_QUEUED;

	error = mpt_send_handshake_cmd(mpt,
	    sizeof (MSG_SCSI_TASK_MGMT), reset_req);
	if (error) {
		mpt_prt(mpt,
		    "mpt_bus_reset: mpt_send_handshake return %d", error);
		return (CAM_REQ_CMP_ERR);
	} else {
		return (CAM_REQ_CMP);
	}
}

/*
 * Process an asynchronous event from the IOC.
 */
static void mpt_ctlop(mpt_softc_t *, void *, u_int32_t);
static void mpt_event_notify_reply(mpt_softc_t *mpt, MSG_EVENT_NOTIFY_REPLY *);

static void
mpt_ctlop(mpt_softc_t *mpt, void *vmsg, u_int32_t reply)
{
	MSG_DEFAULT_REPLY *dmsg = vmsg;

	if (dmsg->Function == MPI_FUNCTION_EVENT_NOTIFICATION) {
		mpt_event_notify_reply(mpt, vmsg);
		mpt_free_reply(mpt, (reply << 1));
	} else if (dmsg->Function == MPI_FUNCTION_EVENT_ACK) {
		mpt_free_reply(mpt, (reply << 1));
	} else if (dmsg->Function == MPI_FUNCTION_PORT_ENABLE) {
		MSG_PORT_ENABLE_REPLY *msg = vmsg;
		int index = msg->MsgContext & ~0x80000000;
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "enable port reply idx %d", index);
		}
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
		}
		mpt_free_reply(mpt, (reply << 1));
	} else if (dmsg->Function == MPI_FUNCTION_CONFIG) {
		MSG_CONFIG_REPLY *msg = vmsg;
		int index = msg->MsgContext & ~0x80000000;
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
			req->sequence = reply;
		} else {
			mpt_free_reply(mpt, (reply << 1));
		}
	} else {
		mpt_prt(mpt, "unknown mpt_ctlop: %x", dmsg->Function);
	}
}

static void
mpt_event_notify_reply(mpt_softc_t *mpt, MSG_EVENT_NOTIFY_REPLY *msg)
{
	switch(msg->Event) {
	case MPI_EVENT_LOG_DATA:
		/* Some error occured that LSI wants logged */
		printf("\tEvtLogData: IOCLogInfo: 0x%08x\n", msg->IOCLogInfo);
		printf("\tEvtLogData: Event Data:");
		{
			int i;
			for (i = 0; i < msg->EventDataLength; i++) {
				printf("  %08x", msg->Data[i]);
			}
		}
		printf("\n");
		break;

	case MPI_EVENT_UNIT_ATTENTION:
		mpt_prt(mpt, "Bus: 0x%02x TargetID: 0x%02x",
		    (msg->Data[0] >> 8) & 0xff, msg->Data[0] & 0xff);
		break;

	case MPI_EVENT_IOC_BUS_RESET:
		/* We generated a bus reset */
		mpt_prt(mpt, "IOC Bus Reset Port: %d",
		    (msg->Data[0] >> 8) & 0xff);
		break;

	case MPI_EVENT_EXT_BUS_RESET:
		/* Someone else generated a bus reset */
		mpt_prt(mpt, "Ext Bus Reset");
		/*
		 * These replies don't return EventData like the MPI
		 * spec says they do
		 */	
/*		xpt_async(AC_BUS_RESET, path, NULL);  */
		break;

	case MPI_EVENT_RESCAN:
		/*
		 * In general this means a device has been added
		 * to the loop.
		 */
		mpt_prt(mpt, "Rescan Port: %d", (msg->Data[0] >> 8) & 0xff);
/*		xpt_async(AC_FOUND_DEVICE, path, NULL);  */
		break;

	case MPI_EVENT_LINK_STATUS_CHANGE:
		mpt_prt(mpt, "Port %d: LinkState: %s",
		    (msg->Data[1] >> 8) & 0xff,
		    ((msg->Data[0] & 0xff) == 0)?  "Failed" : "Active");
		break;

	case MPI_EVENT_LOOP_STATE_CHANGE:
		switch ((msg->Data[0] >> 16) & 0xff) {
		case 0x01:
			mpt_prt(mpt,
			    "Port 0x%x: FC LinkEvent: LIP(%02x,%02x) (Loop Initialization)\n",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			switch ((msg->Data[0] >> 8) & 0xff) {
			case 0xF7:
				if ((msg->Data[0] & 0xff) == 0xF7) {
					printf("Device needs AL_PA\n");
				} else {
					printf("Device %02x doesn't like FC performance\n", 
									msg->Data[0] & 0xFF);
				}
				break;
			case 0xF8:
				if ((msg->Data[0] & 0xff) == 0xF7) {
					printf("Device had loop failure at its receiver prior to acquiring AL_PA\n");
				} else {
					printf("Device %02x detected loop failure at its receiver\n", 
									msg->Data[0] & 0xFF);
				}
				break;
			default:
				printf("Device %02x requests that device %02x reset itself\n", 
					msg->Data[0] & 0xFF,
					(msg->Data[0] >> 8) & 0xFF);
				break;
			}
			break;
		case 0x02:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: LPE(%02x,%02x) (Loop Port Enable)",
				(msg->Data[1] >> 8) & 0xff, /* Port */
				(msg->Data[0] >>  8) & 0xff, /* Character 3 */
				(msg->Data[0]      ) & 0xff  /* Character 4 */
				);
			break;
		case 0x03:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: LPB(%02x,%02x) (Loop Port Bypass)",
				(msg->Data[1] >> 8) & 0xff, /* Port */
			(msg->Data[0] >> 8) & 0xff, /* Character 3 */
			(msg->Data[0]     ) & 0xff  /* Character 4 */
				);
			break;
		default:
			mpt_prt(mpt, "Port 0x%x: FC LinkEvent: Unknown FC event (%02x %02x %02x)",
				(msg->Data[1] >> 8) & 0xff, /* Port */
				(msg->Data[0] >> 16) & 0xff, /* Event */
				(msg->Data[0] >>  8) & 0xff, /* Character 3 */
				(msg->Data[0]      ) & 0xff  /* Character 4 */
				);
		}
		break;

	case MPI_EVENT_LOGOUT:
		mpt_prt(mpt, "FC Logout Port: %d N_PortID: %02x",
		    (msg->Data[1] >> 8) & 0xff, msg->Data[0]);
		break;
	case MPI_EVENT_EVENT_CHANGE:
		/* This is just an acknowledgement of our 
		   mpt_send_event_request */
		break;
	default:
		mpt_prt(mpt, "Unknown event 0x%x\n", msg->Event);
	}
	if (msg->AckRequired) {
		MSG_EVENT_ACK *ackp;
		request_t *req;
		if ((req = mpt_get_request(mpt)) == NULL) {
			panic("unable to get request to acknowledge notify");
		}
		ackp = (MSG_EVENT_ACK *) req->req_vbuf;
		bzero(ackp, sizeof *ackp);
		ackp->Function = MPI_FUNCTION_EVENT_ACK;
		ackp->Event = msg->Event;
		ackp->EventContext = msg->EventContext;
		ackp->MsgContext = req->index | 0x80000000;
		mpt_check_doorbell(mpt);
		mpt_send_cmd(mpt, req);
	}
}

void
mpt_done(mpt_softc_t *mpt, u_int32_t reply)
{
	int index;
	request_t *req;
	union ccb *ccb;
	MSG_REQUEST_HEADER *mpt_req;
	MSG_SCSI_IO_REPLY *mpt_reply;

	index = -1; /* Shutup the complier */

	if ((reply & MPT_CONTEXT_REPLY) == 0) {
		/* context reply */
		mpt_reply = NULL;
		index = reply & MPT_CONTEXT_MASK;
	} else {
		unsigned *pReply;

		bus_dmamap_sync(mpt->reply_dmat, mpt->reply_dmap,
		    BUS_DMASYNC_POSTREAD);
		/* address reply (Error) */
		mpt_reply = MPT_REPLY_PTOV(mpt, reply);
		if (mpt->verbose > 1) {
			pReply = (unsigned *) mpt_reply;
			mpt_prt(mpt, "Address Reply (index %u)",
			    mpt_reply->MsgContext & 0xffff);
			printf("%08x %08x %08x %08x\n",
			    pReply[0], pReply[1], pReply[2], pReply[3]);
			printf("%08x %08x %08x %08x\n",
			    pReply[4], pReply[5], pReply[6], pReply[7]);
			printf("%08x %08x %08x %08x\n\n",
			    pReply[8], pReply[9], pReply[10], pReply[11]);
		}
		index = mpt_reply->MsgContext;
	}

	/*
	 * Address reply with MessageContext high bit set
	 * This is most likely a notify message so we try
	 * to process it then free it
	 */
	if ((index & 0x80000000) != 0) {
		if (mpt_reply != NULL) {
			mpt_ctlop(mpt, mpt_reply, reply);
		} else {
			mpt_prt(mpt, "mpt_done: index 0x%x, NULL reply", index);
		}
		return;
	}

	/* Did we end up with a valid index into the table? */
	if (index < 0 || index >= MPT_MAX_REQUESTS(mpt)) {
		mpt_prt(mpt, "mpt_done: invalid index (%x) in reply", index);
		return;
	}

	req = &mpt->request_pool[index];

	/* Make sure memory hasn't been trashed */
	if (req->index != index) {
		printf("mpt_done: corrupted request struct");
		return;
	}

	/* Short cut for task management replys; nothing more for us to do */
	mpt_req = req->req_vbuf;
	if (mpt_req->Function == MPI_FUNCTION_SCSI_TASK_MGMT) {
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "mpt_done: TASK MGMT");
		}
		goto done;
	}

	if (mpt_req->Function == MPI_FUNCTION_PORT_ENABLE) {
		goto done;
	}

	/*
	 * At this point it better be a SCSI IO command, but don't
	 * crash if it isn't
	 */
	if (mpt_req->Function != MPI_FUNCTION_SCSI_IO_REQUEST)  {
		goto done;
	}

	/* Recover the CAM control block from the request structure */
	ccb = req->ccb;

	/* Can't have had a SCSI command with out a CAM control block */
	if (ccb == NULL || (ccb->ccb_h.status & CAM_SIM_QUEUED) == 0) {
		mpt_prt(mpt,
		    "mpt_done: corrupted ccb, index = 0x%02x seq = 0x%08x",
		    req->index, req->sequence);
		printf(" request state %s\nmpt_request:\n",
		    mpt_req_state(req->debug)); 
		mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);

		if (mpt_reply != NULL) {
			printf("\nmpt_done: reply:\n"); 
			mpt_print_reply(MPT_REPLY_PTOV(mpt, reply));
		} else {
			printf("\nmpt_done: context reply: 0x%08x\n", reply); 
		}
		goto done;
	}

	untimeout(mpttimeout, ccb, ccb->ccb_h.timeout_ch);

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			op = BUS_DMASYNC_POSTREAD;
		} else {
			op = BUS_DMASYNC_POSTWRITE;
		}
		bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
		bus_dmamap_unload(mpt->buffer_dmat, req->dmap);
	}
	ccb->csio.resid = 0;

	if (mpt_reply == NULL) {
		/* Context reply; report that the command was successfull */
		ccb->ccb_h.status = CAM_REQ_CMP;
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		if (mpt->outofbeer) {
			ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
			mpt->outofbeer = 0;
			if (mpt->verbose > 1) {
				mpt_prt(mpt, "THAWQ");
			}
		}
		MPTLOCK_2_CAMLOCK(mpt);
		xpt_done(ccb);
		CAMLOCK_2_MPTLOCK(mpt);
		goto done;
	}

	ccb->csio.scsi_status = mpt_reply->SCSIStatus;
	switch(mpt_reply->IOCStatus) {
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:
		ccb->ccb_h.status = CAM_DATA_RUN_ERR;
		break;

	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:
		/*
		 * Yikes, Tagged queue full comes through this path!
		 *
		 * So we'll change it to a status error and anything
		 * that returns status should probably be a status 
		 * error as well.
		 */
		ccb->csio.resid =
		    ccb->csio.dxfer_len - mpt_reply->TransferCount;
		if (mpt_reply->SCSIState & MPI_SCSI_STATE_NO_SCSI_STATUS) {
			ccb->ccb_h.status = CAM_DATA_RUN_ERR;
			break;
		}
		/* Fall through */
	case MPI_IOCSTATUS_SUCCESS:
	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (ccb->csio.scsi_status) {
		case SCSI_STATUS_OK:
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		default:
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			break;
		}
		break;
	case MPI_IOCSTATUS_BUSY:
	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:
		ccb->ccb_h.status = CAM_BUSY;
		break;

	case MPI_IOCSTATUS_SCSI_INVALID_BUS:
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		break;

	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		ccb->ccb_h.status = CAM_DATA_RUN_ERR;
		break;

	case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR:
	case MPI_IOCSTATUS_SCSI_IO_DATA_ERROR:
		ccb->ccb_h.status =  CAM_UNCOR_PARITY;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		ccb->ccb_h.status = CAM_UA_TERMIO;
		break;

	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:
		ccb->ccb_h.status = CAM_REQ_TERMIO;
		break;

	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:
		ccb->ccb_h.status = CAM_SCSI_BUS_RESET; 
		break;

	default:
		ccb->ccb_h.status = CAM_UNREC_HBA_ERROR;
		break;
	}

	if ((mpt_reply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_VALID) != 0) {
		if (ccb->ccb_h.flags & (CAM_SENSE_PHYS | CAM_SENSE_PTR)) {
			ccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
		} else {
			ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
			ccb->csio.sense_resid = mpt_reply->SenseCount;
			bcopy(req->sense_vbuf, &ccb->csio.sense_data,
			    ccb->csio.sense_len);
		}
	} else if (mpt_reply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_FAILED) {
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			ccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(ccb->ccb_h.path, 1);
		}
	}


	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	if (mpt->outofbeer) {
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		mpt->outofbeer = 0;
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "THAWQ");
		}
	}
	MPTLOCK_2_CAMLOCK(mpt);
	xpt_done(ccb);
	CAMLOCK_2_MPTLOCK(mpt);

done:
	/* If IOC done with this request free it up */
	if (mpt_reply == NULL || (mpt_reply->MsgFlags & 0x80) == 0)
		mpt_free_request(mpt, req);

	/* If address reply; give the buffer back to the IOC */
	if (mpt_reply != NULL)
		mpt_free_reply(mpt, (reply << 1));
}

static void
mpt_action(struct cam_sim *sim, union ccb *ccb)
{
	int  tgt, error;
	mpt_softc_t *mpt;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("mpt_action\n"));

	mpt = (mpt_softc_t *)cam_sim_softc(sim);

	ccb->ccb_h.ccb_mpt_ptr = mpt;

	switch (ccb->ccb_h.func_code) {
	case XPT_RESET_BUS:
		if (mpt->verbose > 1)
			mpt_prt(mpt, "XPT_RESET_BUS");
		CAMLOCK_2_MPTLOCK(mpt);
		error = mpt_bus_reset(ccb);
		switch (error) {
		case CAM_REQ_INPROG:
			MPTLOCK_2_CAMLOCK(mpt);
			break;
		case CAM_REQUEUE_REQ:
			if (mpt->outofbeer == 0) {
				mpt->outofbeer = 1;
				xpt_freeze_simq(sim, 1);
				if (mpt->verbose > 1) {
					mpt_prt(mpt, "FREEZEQ");
				}
			}
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			MPTLOCK_2_CAMLOCK(mpt);
			xpt_done(ccb);
			break;

		case CAM_REQ_CMP:
			ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
			ccb->ccb_h.status |= CAM_REQ_CMP;
			if (mpt->outofbeer) {
				ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
				mpt->outofbeer = 0;
				if (mpt->verbose > 1) {
					mpt_prt(mpt, "THAWQ");
				}
			}
			MPTLOCK_2_CAMLOCK(mpt);
			xpt_done(ccb);
			break;

		default:
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			MPTLOCK_2_CAMLOCK(mpt);
			xpt_done(ccb);
		}
		break;
		
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
		if (ccb->csio.cdb_len >
		    sizeof (((PTR_MSG_SCSI_IO_REQUEST)0)->CDB)) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		mpt_start(ccb);
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
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		tgt = cts->ccb_h.target_id;
		if (mpt->is_fc == 0) {
			u_int8_t dval = 0;
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
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					MPTLOCK_2_CAMLOCK(mpt);
					xpt_done(ccb);
					break;
				}
			}
			if (dval & DP_SYNC) {
				if (mpt_setsync(mpt, tgt, period, offset)) {
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					MPTLOCK_2_CAMLOCK(mpt);
					xpt_done(ccb);
					break;
				}
			}
			MPTLOCK_2_CAMLOCK(mpt);
			if (mpt->verbose > 1) {
				mpt_prt(mpt, 
				    "SET tgt %d flags %x period %x off %x",
				    tgt, dval, period, offset);
			}
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:
		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
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
			u_int8_t dval, pval, oval;

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
				if (mpt_read_cfg_page(mpt, tgt, &tmp.Header)) {
					mpt_prt(mpt,
					    "cannot get target %d DP0", tgt);
				} else  {
					if (mpt->verbose > 1) {
						mpt_prt(mpt,
                            "SPI Tgt %d Page 0: NParms %x Information %x",
						    tgt,
						    tmp.NegotiatedParameters,
						    tmp.Information);
					}
				}
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
			if (mpt->verbose > 1) {
				mpt_prt(mpt, 
				    "GET %s tgt %d flags %x period %x off %x",
				    IS_CURRENT_SETTINGS(cts)? "ACTIVE" :
				    "NVRAM", tgt, dval, pval, oval);
			}
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

		cam_calc_geometry(ccg, /*extended*/1);
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
		if (mpt->is_fc) {
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
mpt_setwidth(mpt_softc_t *mpt, int tgt, int onoff)
{
	CONFIG_PAGE_SCSI_DEVICE_1 tmp;
	tmp = mpt->mpt_dev_page1[tgt];
	if (onoff) {
		tmp.RequestedParameters |= MPI_SCSIDEVPAGE1_RP_WIDE;
	} else {
		tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_WIDE;
	}
	if (mpt_write_cfg_page(mpt, tgt, &tmp.Header)) {
		return (-1);
	}
	if (mpt_read_cfg_page(mpt, tgt, &tmp.Header)) {
		return (-1);
	}
	mpt->mpt_dev_page1[tgt] = tmp;
	if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Target %d Page 1: RequestedParameters %x Config %x",
		    tgt, mpt->mpt_dev_page1[tgt].RequestedParameters,
		    mpt->mpt_dev_page1[tgt].Configuration);
	}
	return (0);
}

static int
mpt_setsync(mpt_softc_t *mpt, int tgt, int period, int offset)
{
	CONFIG_PAGE_SCSI_DEVICE_1 tmp;
	tmp = mpt->mpt_dev_page1[tgt];
	tmp.RequestedParameters &=
	    ~MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK;
	tmp.RequestedParameters &=
	    ~MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK;
	tmp.RequestedParameters &=
	    ~MPI_SCSIDEVPAGE1_RP_DT;
	tmp.RequestedParameters &=
	    ~MPI_SCSIDEVPAGE1_RP_QAS;
	tmp.RequestedParameters &=
	    ~MPI_SCSIDEVPAGE1_RP_IU;
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
	if (mpt_write_cfg_page(mpt, tgt, &tmp.Header)) {
		return (-1);
	}
	if (mpt_read_cfg_page(mpt, tgt, &tmp.Header)) {
		return (-1);
	}
	mpt->mpt_dev_page1[tgt] = tmp;
	if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Target %d Page 1: RParams %x Config %x",
		    tgt, mpt->mpt_dev_page1[tgt].RequestedParameters,
		    mpt->mpt_dev_page1[tgt].Configuration);
	}
	return (0);
}
