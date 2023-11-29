/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2023, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Authors: Sumit Saxena <sumit.saxena@broadcom.com>
 *	    Chandrakanth Patil <chandrakanth.patil@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/selinfo.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/sbuf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/smp_all.h>

#include <dev/nvme/nvme.h>
#include "mpi/mpi30_api.h"
#include "mpi3mr_cam.h"
#include "mpi3mr.h"
#include <sys/time.h>			/* XXX for pcpu.h */
#include <sys/pcpu.h>			/* XXX for PCPU_GET */

#define	smp_processor_id()  PCPU_GET(cpuid)

static void
mpi3mr_enqueue_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cm);
static void
mpi3mr_map_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cm);
void
mpi3mr_release_simq_reinit(struct mpi3mr_cam_softc *cam_sc);
static void
mpi3mr_freeup_events(struct mpi3mr_softc *sc);

extern int
mpi3mr_register_events(struct mpi3mr_softc *sc);
extern void mpi3mr_add_sg_single(void *paddr, U8 flags, U32 length,
    bus_addr_t dma_addr);
extern void mpi3mr_build_zero_len_sge(void *paddr);

static U32 event_count;

static void mpi3mr_prepare_sgls(void *arg,
	bus_dma_segment_t *segs, int nsegs, int error)
{
	struct mpi3mr_softc *sc;
	struct mpi3mr_cmd *cm;
	u_int i;
	bus_addr_t chain_dma;
	void *chain;
	U8 *sg_local;
	U32 chain_length;
	int sges_left;
	U32 sges_in_segment;
	U8 simple_sgl_flags;
	U8 simple_sgl_flags_last;
	U8 last_chain_sgl_flags;
	struct mpi3mr_chain *chain_req;
	Mpi3SCSIIORequest_t *scsiio_req;
	union ccb *ccb;
	
	cm = (struct mpi3mr_cmd *)arg;
	sc = cm->sc;
	scsiio_req = (Mpi3SCSIIORequest_t *) &cm->io_request;
	ccb = cm->ccb;

	if (error) {
		device_printf(sc->mpi3mr_dev, "%s: error=%d\n",__func__, error);
		if (error == EFBIG) {
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_TOO_BIG);
		} else {
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP_ERR);
		}
		mpi3mr_release_command(cm);
		xpt_done(ccb);
		return;
	}
	
	if (cm->data_dir == MPI3MR_READ)
		bus_dmamap_sync(sc->buffer_dmat, cm->dmamap,
		    BUS_DMASYNC_PREREAD);
	if (cm->data_dir == MPI3MR_WRITE)
		bus_dmamap_sync(sc->buffer_dmat, cm->dmamap,
		    BUS_DMASYNC_PREWRITE);

	KASSERT(nsegs <= MPI3MR_SG_DEPTH && nsegs > 0,
	    ("%s: bad SGE count: %d\n", device_get_nameunit(sc->mpi3mr_dev), nsegs));

	simple_sgl_flags = MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE |
	    MPI3_SGE_FLAGS_DLAS_SYSTEM;
	simple_sgl_flags_last = simple_sgl_flags |
	    MPI3_SGE_FLAGS_END_OF_LIST;
	last_chain_sgl_flags = MPI3_SGE_FLAGS_ELEMENT_TYPE_LAST_CHAIN |
	    MPI3_SGE_FLAGS_DLAS_SYSTEM;

	sg_local = (U8 *)&scsiio_req->SGL;

	if (scsiio_req->DataLength == 0) {
		/* XXX we don't ever get here when DataLength == 0, right? cm->data is NULL */
		/* This whole if can likely be removed -- we handle it in mpi3mr_request_map */
		mpi3mr_build_zero_len_sge(sg_local);
		goto enqueue;
	}
	
	sges_left = nsegs;

	sges_in_segment = (sc->facts.op_req_sz -
	    offsetof(Mpi3SCSIIORequest_t, SGL))/sizeof(Mpi3SGESimple_t);

	i = 0;

	mpi3mr_dprint(sc, MPI3MR_TRACE, "SGE count: %d IO size: %d\n",
		nsegs, scsiio_req->DataLength);

	if (sges_left <= sges_in_segment)
		goto fill_in_last_segment;

	/* fill in main message segment when there is a chain following */
	while (sges_in_segment > 1) {
		mpi3mr_add_sg_single(sg_local, simple_sgl_flags,
		    segs[i].ds_len, segs[i].ds_addr);
		sg_local += sizeof(Mpi3SGESimple_t);
		sges_left--;
		sges_in_segment--;
		i++;
	}

	chain_req = &sc->chain_sgl_list[cm->hosttag];
	
	chain = chain_req->buf;
	chain_dma = chain_req->buf_phys;
	memset(chain_req->buf, 0, PAGE_SIZE);
	sges_in_segment = sges_left;
	chain_length = sges_in_segment * sizeof(Mpi3SGESimple_t);

	mpi3mr_add_sg_single(sg_local, last_chain_sgl_flags,
	    chain_length, chain_dma);

	sg_local = chain;

fill_in_last_segment:
	while (sges_left > 0) {
		if (sges_left == 1)
			mpi3mr_add_sg_single(sg_local,
			    simple_sgl_flags_last, segs[i].ds_len,
			    segs[i].ds_addr);
		else
			mpi3mr_add_sg_single(sg_local, simple_sgl_flags,
			    segs[i].ds_len, segs[i].ds_addr);
		sg_local += sizeof(Mpi3SGESimple_t);
		sges_left--;
		i++;
	}

enqueue:
	/*
	 * Now that we've created the sgls, we send the request to the device.
	 * Unlike in Linux, dmaload isn't guaranteed to load every time, but
	 * this function is always called when the resources are available, so
	 * we can send the request to hardware here always. mpi3mr_map_request
	 * knows about this quirk and will only take evasive action when an
	 * error other than EINPROGRESS is returned from dmaload.
	 */
	mpi3mr_enqueue_request(sc, cm);

	return;
}

static void
mpi3mr_map_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cm)
{
	u_int32_t retcode = 0;
	union ccb *ccb;

	ccb = cm->ccb;
	if (cm->data != NULL) {
		mtx_lock(&sc->io_lock);
		/* Map data buffer into bus space */
		retcode = bus_dmamap_load_ccb(sc->buffer_dmat, cm->dmamap,
		    ccb, mpi3mr_prepare_sgls, cm, 0);
		mtx_unlock(&sc->io_lock);
		if (retcode != 0 && retcode != EINPROGRESS) {
			device_printf(sc->mpi3mr_dev,
			    "bus_dmamap_load(): retcode = %d\n", retcode);
			/*
			 * Any other error means prepare_sgls wasn't called, and
			 * will never be called, so we have to mop up. This error
			 * should never happen, though.
			 */
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP_ERR);
			mpi3mr_release_command(cm);
			xpt_done(ccb);
		}
	} else {
		/*
		 * No data, we enqueue it directly here.
		 */
		mpi3mr_enqueue_request(sc, cm);
	}
}

void
mpi3mr_unmap_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cmd)
{
	if (cmd->data != NULL) {
		if (cmd->data_dir == MPI3MR_READ)
			bus_dmamap_sync(sc->buffer_dmat, cmd->dmamap, BUS_DMASYNC_POSTREAD);
		if (cmd->data_dir == MPI3MR_WRITE)
			bus_dmamap_sync(sc->buffer_dmat, cmd->dmamap, BUS_DMASYNC_POSTWRITE);
		mtx_lock(&sc->io_lock);
		bus_dmamap_unload(sc->buffer_dmat, cmd->dmamap);
		mtx_unlock(&sc->io_lock);
	}
}

/**
 * mpi3mr_allow_unmap_to_fw - Whether an unmap is allowed to fw
 * @sc: Adapter instance reference
 * @ccb: SCSI Command reference
 *
 * The controller hardware cannot handle certain unmap commands
 * for NVMe drives, this routine checks those and return true
 * and completes the SCSI command with proper status and sense
 * data.
 *
 * Return: TRUE for allowed unmap, FALSE otherwise.
 */
static bool mpi3mr_allow_unmap_to_fw(struct mpi3mr_softc *sc,
	union ccb *ccb)
{
	struct ccb_scsiio *csio;
	uint16_t param_list_len, block_desc_len, trunc_param_len = 0;

	csio = &ccb->csio;
	param_list_len = (uint16_t) ((scsiio_cdb_ptr(csio)[7] << 8) | scsiio_cdb_ptr(csio)[8]);

	switch(pci_get_revid(sc->mpi3mr_dev)) {
	case SAS4116_CHIP_REV_A0:
		if (!param_list_len) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "%s: CDB received with zero parameter length\n",
			    __func__);
			mpi3mr_print_cdb(ccb);
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
			xpt_done(ccb);
			return false;
		}

		if (param_list_len < 24) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "%s: CDB received with invalid param_list_len: %d\n",
			    __func__, param_list_len);
			mpi3mr_print_cdb(ccb);
			scsi_set_sense_data(&ccb->csio.sense_data,
				/*sense_format*/ SSD_TYPE_FIXED,
				/*current_error*/ 1,
				/*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				/*asc*/ 0x1A,
				/*ascq*/ 0x00,
				/*extra args*/ SSD_ELEM_NONE);
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
			ccb->ccb_h.status =
			    CAM_SCSI_STATUS_ERROR |
			    CAM_AUTOSNS_VALID;
			return false;
		}

		if (param_list_len != csio->dxfer_len) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "%s: CDB received with param_list_len: %d bufflen: %d\n",
			    __func__, param_list_len, csio->dxfer_len);
			mpi3mr_print_cdb(ccb);
			scsi_set_sense_data(&ccb->csio.sense_data,
				/*sense_format*/ SSD_TYPE_FIXED,
				/*current_error*/ 1,
				/*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				/*asc*/ 0x1A,
				/*ascq*/ 0x00,
				/*extra args*/ SSD_ELEM_NONE);
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
			ccb->ccb_h.status =
			    CAM_SCSI_STATUS_ERROR |
			    CAM_AUTOSNS_VALID;
			xpt_done(ccb);
			return false;
		}
		
		block_desc_len = (uint16_t) (csio->data_ptr[2] << 8 | csio->data_ptr[3]);

		if (block_desc_len < 16) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "%s: Invalid descriptor length in param list: %d\n",
			    __func__, block_desc_len);
			mpi3mr_print_cdb(ccb);
			scsi_set_sense_data(&ccb->csio.sense_data,
				/*sense_format*/ SSD_TYPE_FIXED,
				/*current_error*/ 1,
				/*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				/*asc*/ 0x26,
				/*ascq*/ 0x00,
				/*extra args*/ SSD_ELEM_NONE);
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
			ccb->ccb_h.status =
			    CAM_SCSI_STATUS_ERROR |
			    CAM_AUTOSNS_VALID;
			xpt_done(ccb);
			return false;
		}

		if (param_list_len > (block_desc_len + 8)) {
			mpi3mr_print_cdb(ccb);
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "%s: Truncating param_list_len(%d) to block_desc_len+8(%d)\n",
			    __func__, param_list_len, (block_desc_len + 8));
			param_list_len = block_desc_len + 8;
			scsiio_cdb_ptr(csio)[7] = (param_list_len >> 8) | 0xff;
			scsiio_cdb_ptr(csio)[8] = param_list_len | 0xff;
			mpi3mr_print_cdb(ccb);
		}
		break;

	case SAS4116_CHIP_REV_B0:
		if ((param_list_len > 24) && ((param_list_len - 8) & 0xF)) {
			trunc_param_len -= (param_list_len - 8) & 0xF;
			mpi3mr_print_cdb(ccb);
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "%s: Truncating param_list_len from (%d) to (%d)\n",
			    __func__, param_list_len, trunc_param_len);
			scsiio_cdb_ptr(csio)[7] = (param_list_len >> 8) | 0xff; 
			scsiio_cdb_ptr(csio)[8] = param_list_len | 0xff;
			mpi3mr_print_cdb(ccb);
		}
		break;
	}

	return true;
}

/**
 * mpi3mr_tm_response_name -  get TM response as a string
 * @resp_code: TM response code
 *
 * Convert known task management response code as a readable
 * string.
 *
 * Return: response code string.
 */
static const char* mpi3mr_tm_response_name(U8 resp_code)
{
	char *desc;

	switch (resp_code) {
	case MPI3_SCSITASKMGMT_RSPCODE_TM_COMPLETE:
		desc = "task management request completed";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_INVALID_FRAME:
		desc = "invalid frame";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_FUNCTION_NOT_SUPPORTED:
		desc = "task management request not supported";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_FAILED:
		desc = "task management request failed";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_SUCCEEDED:
		desc = "task management request succeeded";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_INVALID_LUN:
		desc = "invalid LUN";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_OVERLAPPED_TAG:
		desc = "overlapped tag attempted";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_IO_QUEUED_ON_IOC:
		desc = "task queued, however not sent to target";
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_TM_NVME_DENIED:
		desc = "task management request denied by NVMe device";
		break;
	default:
		desc = "unknown";
		break;
	}

	return desc;
}

void mpi3mr_poll_pend_io_completions(struct mpi3mr_softc *sc)
{
	int i;
	int num_of_reply_queues = sc->num_queues;
	struct mpi3mr_irq_context *irq_ctx;

	for (i = 0; i < num_of_reply_queues; i++) {
		irq_ctx = &sc->irq_ctx[i];
		mpi3mr_complete_io_cmd(sc, irq_ctx);
	}
}

void
trigger_reset_from_watchdog(struct mpi3mr_softc *sc, U8 reset_type, U32 reset_reason)
{
	if (sc->reset_in_progress) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "Another reset is in progress, no need to trigger the reset\n");
		return;
	}
	sc->reset.type = reset_type;
	sc->reset.reason = reset_reason;

	return;
}

/**
 * mpi3mr_issue_tm - Issue Task Management request
 * @sc: Adapter instance reference
 * @tm_type: Task Management type
 * @handle: Device handle
 * @lun: lun ID
 * @htag: Host tag of the TM request
 * @timeout: TM timeout value
 * @drv_cmd: Internal command tracker
 * @resp_code: Response code place holder
 * @cmd: Timed out command reference
 *
 * Issues a Task Management Request to the controller for a
 * specified target, lun and command and wait for its completion
 * and check TM response. Recover the TM if it timed out by
 * issuing controller reset.
 *
 * Return: 0 on success, non-zero on errors
 */
static int
mpi3mr_issue_tm(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cmd,
		U8 tm_type, unsigned long timeout)
{
	int retval = 0;
	MPI3_SCSI_TASK_MGMT_REQUEST tm_req;
	MPI3_SCSI_TASK_MGMT_REPLY *tm_reply = NULL;
	struct mpi3mr_drvr_cmd *drv_cmd = NULL;
	struct mpi3mr_target *tgtdev = NULL;
	struct mpi3mr_op_req_queue *op_req_q = NULL;
	union ccb *ccb;
	U8 resp_code;

	
	if (sc->unrecoverable) {
		mpi3mr_dprint(sc, MPI3MR_INFO, 
			"Controller is in unrecoverable state!! TM not required\n");
		return retval;
	}
	if (sc->reset_in_progress) {
		mpi3mr_dprint(sc, MPI3MR_INFO, 
			"controller reset in progress!! TM not required\n");
		return retval;
	}
	
	if (!cmd->ccb) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "SCSIIO command timed-out with NULL ccb\n");
		return retval;
	}
	ccb = cmd->ccb;

	tgtdev = cmd->targ;
	if (tgtdev == NULL)  {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Device does not exist target ID:0x%x,"
			      "TM is not required\n", ccb->ccb_h.target_id);
		return retval;
	}
	if (tgtdev->dev_removed == 1)  {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Device(0x%x) is removed, TM is not required\n",
			      ccb->ccb_h.target_id);
		return retval;
	}
	
	drv_cmd = &sc->host_tm_cmds;
	mtx_lock(&drv_cmd->lock);

	memset(&tm_req, 0, sizeof(tm_req));
	tm_req.DevHandle = htole16(tgtdev->dev_handle);
	tm_req.TaskType = tm_type;
	tm_req.HostTag = htole16(MPI3MR_HOSTTAG_TMS);
	int_to_lun(ccb->ccb_h.target_lun, tm_req.LUN);
	tm_req.Function = MPI3_FUNCTION_SCSI_TASK_MGMT;
	drv_cmd->state = MPI3MR_CMD_PENDING;
	drv_cmd->is_waiting = 1;
	drv_cmd->callback = NULL;

	if (ccb) {
		if (tm_type == MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK) {
			op_req_q = &sc->op_req_q[cmd->req_qidx];
			tm_req.TaskHostTag = htole16(cmd->hosttag);
			tm_req.TaskRequestQueueID = htole16(op_req_q->qid);
		}
	} 
	
	if (tgtdev)
		mpi3mr_atomic_inc(&tgtdev->block_io);

	if (tgtdev && (tgtdev->dev_type == MPI3_DEVICE_DEVFORM_PCIE)) {
		if ((tm_type == MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK)
		     && tgtdev->dev_spec.pcie_inf.abort_to)
 			timeout = tgtdev->dev_spec.pcie_inf.abort_to;
		else if ((tm_type == MPI3_SCSITASKMGMT_TASKTYPE_TARGET_RESET)
			 && tgtdev->dev_spec.pcie_inf.reset_to)
			 timeout = tgtdev->dev_spec.pcie_inf.reset_to;
	}
	
	sc->tm_chan = (void *)&drv_cmd;
	
	mpi3mr_dprint(sc, MPI3MR_DEBUG_TM, 
		      "posting task management request: type(%d), handle(0x%04x)\n",
		       tm_type, tgtdev->dev_handle);

	init_completion(&drv_cmd->completion);
	retval = mpi3mr_submit_admin_cmd(sc, &tm_req, sizeof(tm_req));
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, 
			      "posting task management request is failed\n");
		retval = -1;
		goto out_unlock;
	}
	wait_for_completion_timeout_tm(&drv_cmd->completion, timeout, sc);

	if (!(drv_cmd->state & MPI3MR_CMD_COMPLETE)) {
		drv_cmd->is_waiting = 0;
		retval = -1;
		if (!(drv_cmd->state & MPI3MR_CMD_RESET)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, 
				      "task management request timed out after %ld seconds\n", timeout);
			if (sc->mpi3mr_debug & MPI3MR_DEBUG_TM) {
				mpi3mr_dprint(sc, MPI3MR_INFO, "tm_request dump\n");
				mpi3mr_hexdump(&tm_req, sizeof(tm_req), 8);
			}
			trigger_reset_from_watchdog(sc, MPI3MR_TRIGGER_SOFT_RESET, MPI3MR_RESET_FROM_TM_TIMEOUT); 
			retval = ETIMEDOUT;
		}
		goto out_unlock;
	}

	if (!(drv_cmd->state & MPI3MR_CMD_REPLYVALID)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, 
			      "invalid task management reply message\n");
		retval = -1;
		goto out_unlock;
	}
	tm_reply = (MPI3_SCSI_TASK_MGMT_REPLY *)drv_cmd->reply;

	switch (drv_cmd->ioc_status) {
	case MPI3_IOCSTATUS_SUCCESS:
		resp_code = tm_reply->ResponseData & MPI3MR_RI_MASK_RESPCODE;
		break;
	case MPI3_IOCSTATUS_SCSI_IOC_TERMINATED:
		resp_code = MPI3_SCSITASKMGMT_RSPCODE_TM_COMPLETE;
		break;
	default:
		mpi3mr_dprint(sc, MPI3MR_ERROR, 
			      "task management request to handle(0x%04x) is failed with ioc_status(0x%04x) log_info(0x%08x)\n",
			       tgtdev->dev_handle, drv_cmd->ioc_status, drv_cmd->ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}

	switch (resp_code) {
	case MPI3_SCSITASKMGMT_RSPCODE_TM_SUCCEEDED:
	case MPI3_SCSITASKMGMT_RSPCODE_TM_COMPLETE:
		break;
	case MPI3_SCSITASKMGMT_RSPCODE_IO_QUEUED_ON_IOC:
		if (tm_type != MPI3_SCSITASKMGMT_TASKTYPE_QUERY_TASK)
			retval = -1;
		break;
	default:
		retval = -1;
		break;
	}
	
	mpi3mr_dprint(sc, MPI3MR_DEBUG_TM, 
		      "task management request type(%d) completed for handle(0x%04x) with ioc_status(0x%04x), log_info(0x%08x)"
		      "termination_count(%u), response:%s(0x%x)\n", tm_type, tgtdev->dev_handle, drv_cmd->ioc_status, drv_cmd->ioc_loginfo,
		      tm_reply->TerminationCount, mpi3mr_tm_response_name(resp_code), resp_code);
	
	if (retval)
		goto out_unlock;

	mpi3mr_disable_interrupts(sc);
	mpi3mr_poll_pend_io_completions(sc);
	mpi3mr_enable_interrupts(sc);
	mpi3mr_poll_pend_io_completions(sc);

	switch (tm_type) {
	case MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK:
		if (cmd->state == MPI3MR_CMD_STATE_IN_TM) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, 
				      "%s: task abort returned success from firmware but corresponding CCB (%p) was not terminated"
				      "marking task abort failed!\n", sc->name, cmd->ccb);
			retval = -1;
		}
		break;
	case MPI3_SCSITASKMGMT_TASKTYPE_TARGET_RESET:
		if (mpi3mr_atomic_read(&tgtdev->outstanding)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, 
				      "%s: target reset returned success from firmware but IOs are still pending on the target (%p)"
				      "marking target reset failed!\n",
				      sc->name, tgtdev);
			retval = -1;
		}
		break;
	default:
		break;
	}
	
out_unlock:
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&drv_cmd->lock);
	if (tgtdev && mpi3mr_atomic_read(&tgtdev->block_io) > 0)
		mpi3mr_atomic_dec(&tgtdev->block_io);
	
	return retval;
}

/**
 * mpi3mr_task_abort- Abort error handling callback
 * @cmd: Timed out command reference
 *
 * Issue Abort Task Management if the command is in LLD scope
 * and verify if it is aborted successfully and return status
 * accordingly.
 *
 * Return: SUCCESS of successful abort the SCSI command else FAILED
 */
static int mpi3mr_task_abort(struct mpi3mr_cmd *cmd)
{
	int retval = 0;
	struct mpi3mr_softc *sc;
	union ccb *ccb;

	sc = cmd->sc;

	if (!cmd->ccb) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "SCSIIO command timed-out with NULL ccb\n");
		return retval;
	}
	ccb = cmd->ccb;
	
	mpi3mr_dprint(sc, MPI3MR_INFO, 
		      "attempting abort task for ccb(%p)\n", ccb);
	
	mpi3mr_print_cdb(ccb);

	if (cmd->state != MPI3MR_CMD_STATE_BUSY) {
		mpi3mr_dprint(sc, MPI3MR_INFO, 
			      "%s: ccb is not in driver scope, abort task is not required\n",
			      sc->name);
		return retval;
	}
	cmd->state = MPI3MR_CMD_STATE_IN_TM;

	retval = mpi3mr_issue_tm(sc, cmd, MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK, MPI3MR_ABORTTM_TIMEOUT);
	
	mpi3mr_dprint(sc, MPI3MR_INFO, 
		      "abort task is %s for ccb(%p)\n", ((retval == 0) ? "SUCCESS" : "FAILED"), ccb);
	
	return retval;
}

/**
 * mpi3mr_target_reset - Target reset error handling callback
 * @cmd: Timed out command reference
 *
 * Issue Target reset Task Management and verify the SCSI commands are
 * terminated successfully and return status accordingly.
 *
 * Return: SUCCESS of successful termination of the SCSI commands else
 *         FAILED
 */
static int mpi3mr_target_reset(struct mpi3mr_cmd *cmd)
{
	int retval = 0;
	struct mpi3mr_softc *sc;
	struct mpi3mr_target *target;

	sc = cmd->sc;
	
	target = cmd->targ;
	if (target == NULL)  {
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Device does not exist for target:0x%p,"
			      "target reset is not required\n", target);
		return retval;
	}
	
	mpi3mr_dprint(sc, MPI3MR_INFO, 
		      "attempting target reset on target(%d)\n", target->per_id);

	
	if (mpi3mr_atomic_read(&target->outstanding)) {
		mpi3mr_dprint(sc, MPI3MR_INFO, 
			      "no outstanding IOs on the target(%d),"
			      " target reset not required.\n", target->per_id);
		return retval;
	}
	
	retval = mpi3mr_issue_tm(sc, cmd, MPI3_SCSITASKMGMT_TASKTYPE_TARGET_RESET, MPI3MR_RESETTM_TIMEOUT);

	mpi3mr_dprint(sc, MPI3MR_INFO, 
		      "target reset is %s for target(%d)\n", ((retval == 0) ? "SUCCESS" : "FAILED"),
		      target->per_id);

	return retval;
}

/**
 * mpi3mr_get_fw_pending_ios - Calculate pending I/O count
 * @sc: Adapter instance reference
 *
 * Calculate the pending I/Os for the controller and return.
 *
 * Return: Number of pending I/Os
 */
static inline int mpi3mr_get_fw_pending_ios(struct mpi3mr_softc *sc)
{
	U16 i, pend_ios = 0;

	for (i = 0; i < sc->num_queues; i++)
		pend_ios += mpi3mr_atomic_read(&sc->op_reply_q[i].pend_ios);
	return pend_ios;
}

/**
 * mpi3mr_wait_for_host_io - block for I/Os to complete
 * @sc: Adapter instance reference
 * @timeout: time out in seconds
 * 
 * Waits for pending I/Os for the given adapter to complete or
 * to hit the timeout.
 *
 * Return: Nothing
 */
static int mpi3mr_wait_for_host_io(struct mpi3mr_softc *sc, U32 timeout)
{
	enum mpi3mr_iocstate iocstate;

	iocstate = mpi3mr_get_iocstate(sc);
	if (iocstate != MRIOC_STATE_READY) {
		mpi3mr_dprint(sc, MPI3MR_XINFO, "%s :Controller is in NON-READY state! Proceed with Reset\n", __func__);
		return -1;
	}

	if (!mpi3mr_get_fw_pending_ios(sc))
		return 0;
	
	mpi3mr_dprint(sc, MPI3MR_INFO,
		      "%s :Waiting for %d seconds prior to reset for %d pending I/Os to complete\n",
		      __func__, timeout, mpi3mr_get_fw_pending_ios(sc));

	int i;
	for (i = 0; i < timeout; i++) {
		if (!mpi3mr_get_fw_pending_ios(sc)) {
			mpi3mr_dprint(sc, MPI3MR_INFO, "%s :All pending I/Os got completed while waiting! Reset not required\n", __func__);
			return 0;
			
		}
		iocstate = mpi3mr_get_iocstate(sc);
		if (iocstate != MRIOC_STATE_READY) {
			mpi3mr_dprint(sc, MPI3MR_XINFO, "%s :Controller state becomes NON-READY while waiting! dont wait further"
				      "Proceed with Reset\n", __func__);
			return -1;
		}
		DELAY(1000 * 1000);
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "%s :Pending I/Os after wait exaust is %d! Proceed with Reset\n", __func__,
		      mpi3mr_get_fw_pending_ios(sc));

	return -1;
}

static void
mpi3mr_scsiio_timeout(void *data)
{
	int retval = 0;
	struct mpi3mr_softc *sc;
	struct mpi3mr_cmd *cmd;
	struct mpi3mr_target *targ_dev = NULL;

	if (!data)
		return;

	cmd = (struct mpi3mr_cmd *)data;
	sc = cmd->sc;

	if (cmd->ccb == NULL) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "SCSIIO command timed-out with NULL ccb\n");
		return;
	}

	/*
	 * TMs are not supported for IO timeouts on VD/LD, so directly issue controller reset
	 * with max timeout for outstanding IOs to complete is 180sec.
	 */
	targ_dev = cmd->targ;
	if (targ_dev && (targ_dev->dev_type == MPI3_DEVICE_DEVFORM_VD)) { 
		if (mpi3mr_wait_for_host_io(sc, MPI3MR_RAID_ERRREC_RESET_TIMEOUT))
			trigger_reset_from_watchdog(sc, MPI3MR_TRIGGER_SOFT_RESET, MPI3MR_RESET_FROM_SCSIIO_TIMEOUT);
		return;
 	}
	
	/* Issue task abort to recover the timed out IO */
	retval = mpi3mr_task_abort(cmd);
	if (!retval || (retval == ETIMEDOUT))
		return;
	
	/* 
	 * task abort has failed to recover the timed out IO,
	 * try with the target reset
	 */
	retval = mpi3mr_target_reset(cmd);
	if (!retval || (retval == ETIMEDOUT))
		return;

	/* 
	 * task abort and target reset has failed. So issue Controller reset(soft reset) 
	 * through OCR thread context
	 */
	trigger_reset_from_watchdog(sc, MPI3MR_TRIGGER_SOFT_RESET, MPI3MR_RESET_FROM_SCSIIO_TIMEOUT); 

	return;
}

void int_to_lun(unsigned int lun, U8 *req_lun)
{
	int i;
	
	memset(req_lun, 0, sizeof(*req_lun));

	for (i = 0; i < sizeof(lun); i += 2) {
		req_lun[i] = (lun >> 8) & 0xFF;
		req_lun[i+1] = lun & 0xFF;
		lun = lun >> 16;
	}

}

static U16 get_req_queue_index(struct mpi3mr_softc *sc)
{
	U16 i = 0, reply_q_index = 0, reply_q_pend_ios = 0;
	
	reply_q_pend_ios = mpi3mr_atomic_read(&sc->op_reply_q[0].pend_ios);
	for (i = 0; i < sc->num_queues; i++) {
		if (reply_q_pend_ios > mpi3mr_atomic_read(&sc->op_reply_q[i].pend_ios)) {
			reply_q_pend_ios = mpi3mr_atomic_read(&sc->op_reply_q[i].pend_ios);
			reply_q_index = i;
		}
	}
	
	return reply_q_index;
}

static void
mpi3mr_action_scsiio(struct mpi3mr_cam_softc *cam_sc, union ccb *ccb)
{
	Mpi3SCSIIORequest_t *req = NULL;
	struct ccb_scsiio *csio;
	struct mpi3mr_softc *sc;
	struct mpi3mr_target *targ;
	struct mpi3mr_cmd *cm;
	uint8_t scsi_opcode, queue_idx;
	uint32_t mpi_control;

	sc = cam_sc->sc;
	mtx_assert(&sc->mpi3mr_mtx, MA_OWNED);
	
	if (sc->unrecoverable) {
		mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		xpt_done(ccb);
		return;
	}

	csio = &ccb->csio;
	KASSERT(csio->ccb_h.target_id < cam_sc->maxtargets,
	    ("Target %d out of bounds in XPT_SCSI_IO\n",
	     csio->ccb_h.target_id));
	
	scsi_opcode = scsiio_cdb_ptr(csio)[0];
	
	if ((sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN) &&
	    !((scsi_opcode == SYNCHRONIZE_CACHE) ||
	      (scsi_opcode == START_STOP_UNIT))) {
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		xpt_done(ccb);
		return;
	}

	targ = mpi3mr_find_target_by_per_id(cam_sc, csio->ccb_h.target_id);
	if (targ == NULL)  {
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Device with target ID: 0x%x does not exist\n",
			      csio->ccb_h.target_id);
		mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		xpt_done(ccb);
		return;
	}
	
	if (targ && targ->is_hidden)  {
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Device with target ID: 0x%x is hidden\n",
			      csio->ccb_h.target_id);
		mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		xpt_done(ccb);
		return;
	}

	if (targ->dev_removed == 1)  {
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Device with target ID: 0x%x is removed\n", csio->ccb_h.target_id);
		mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		xpt_done(ccb);
		return;
	}

	if (targ->dev_handle == 0x0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "%s NULL handle for target 0x%x\n", 
		    __func__, csio->ccb_h.target_id);
		mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		xpt_done(ccb);
		return;
	}

	if (mpi3mr_atomic_read(&targ->block_io) ||
		(sc->reset_in_progress == 1) || (sc->prepare_for_reset == 1)) {
		mpi3mr_dprint(sc, MPI3MR_TRACE, "%s target is busy target_id: 0x%x\n",
		    __func__, csio->ccb_h.target_id);
		mpi3mr_set_ccbstatus(ccb, CAM_REQUEUE_REQ);
		xpt_done(ccb);
		return;
	}

	/*
	 * Sometimes, it is possible to get a command that is not "In
	 * Progress" and was actually aborted by the upper layer.  Check for
	 * this here and complete the command without error.
	 */
	if (mpi3mr_get_ccbstatus(ccb) != CAM_REQ_INPROG) {
		mpi3mr_dprint(sc, MPI3MR_TRACE, "%s Command is not in progress for "
		    "target %u\n", __func__, csio->ccb_h.target_id);
		xpt_done(ccb);
		return;
	}
	/*
	 * If devinfo is 0 this will be a volume.  In that case don't tell CAM
	 * that the volume has timed out.  We want volumes to be enumerated
	 * until they are deleted/removed, not just failed.
	 */
	if (targ->flags & MPI3MRSAS_TARGET_INREMOVAL) {
		if (targ->devinfo == 0)
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		else
			mpi3mr_set_ccbstatus(ccb, CAM_SEL_TIMEOUT);
		xpt_done(ccb);
		return;
	}

	if ((scsi_opcode == UNMAP) &&
		(pci_get_device(sc->mpi3mr_dev) == MPI3_MFGPAGE_DEVID_SAS4116) &&
		(targ->dev_type == MPI3_DEVICE_DEVFORM_PCIE) &&
		(mpi3mr_allow_unmap_to_fw(sc, ccb) == false))
		return;

	cm = mpi3mr_get_command(sc);
	if (cm == NULL || (sc->mpi3mr_flags & MPI3MR_FLAGS_DIAGRESET)) {
		if (cm != NULL) {
			mpi3mr_release_command(cm);
		}
		if ((cam_sc->flags & MPI3MRSAS_QUEUE_FROZEN) == 0) {
			xpt_freeze_simq(cam_sc->sim, 1);
			cam_sc->flags |= MPI3MRSAS_QUEUE_FROZEN;
		}
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		mpi3mr_set_ccbstatus(ccb, CAM_REQUEUE_REQ);
		xpt_done(ccb);
		return;
	}

	switch (csio->ccb_h.flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		mpi_control = MPI3_SCSIIO_FLAGS_DATADIRECTION_READ;
		cm->data_dir = MPI3MR_READ;
		break;
	case CAM_DIR_OUT:
		mpi_control = MPI3_SCSIIO_FLAGS_DATADIRECTION_WRITE;
		cm->data_dir = MPI3MR_WRITE;
		break;
	case CAM_DIR_NONE:
	default:
		mpi_control = MPI3_SCSIIO_FLAGS_DATADIRECTION_NO_DATA_TRANSFER;
		break;
	}

	if (csio->cdb_len > 16)
		mpi_control |= MPI3_SCSIIO_FLAGS_CDB_GREATER_THAN_16;

	req = (Mpi3SCSIIORequest_t *)&cm->io_request;
	bzero(req, sizeof(*req));
	req->Function = MPI3_FUNCTION_SCSI_IO;
	req->HostTag = cm->hosttag;
	req->DataLength = htole32(csio->dxfer_len);
	req->DevHandle = htole16(targ->dev_handle);

	/*
	 * It looks like the hardware doesn't require an explicit tag
	 * number for each transaction.  SAM Task Management not supported
	 * at the moment.
	 */
	switch (csio->tag_action) {
	case MSG_HEAD_OF_Q_TAG:
		mpi_control |= MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_HEADOFQ;
		break;
	case MSG_ORDERED_Q_TAG:
		mpi_control |= MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ORDEREDQ;
		break;
	case MSG_ACA_TASK:
		mpi_control |= MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ACAQ;
		break;
	case CAM_TAG_ACTION_NONE:
	case MSG_SIMPLE_Q_TAG:
	default:
		mpi_control |= MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_SIMPLEQ;
		break;
	}

	req->Flags = htole32(mpi_control);

	if (csio->ccb_h.flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, &req->CDB.CDB32[0], csio->cdb_len);
	else {
		KASSERT(csio->cdb_len <= IOCDBLEN,
		    ("cdb_len %d is greater than IOCDBLEN but CAM_CDB_POINTER "
		    "is not set", csio->cdb_len));
		bcopy(csio->cdb_io.cdb_bytes, &req->CDB.CDB32[0],csio->cdb_len);
	}

	cm->length = csio->dxfer_len;
	cm->targ = targ;
	int_to_lun(csio->ccb_h.target_lun, req->LUN);
	cm->ccb = ccb;
	csio->ccb_h.qos.sim_data = sbinuptime();
	queue_idx = get_req_queue_index(sc);
	cm->req_qidx = queue_idx;
	
	mpi3mr_dprint(sc, MPI3MR_TRACE, "[QID:%d]: func: %s line:%d CDB: 0x%x targetid: %x SMID: 0x%x\n",
		(queue_idx + 1), __func__, __LINE__, scsi_opcode, csio->ccb_h.target_id, cm->hosttag);

	switch ((ccb->ccb_h.flags & CAM_DATA_MASK)) {
	case CAM_DATA_PADDR:
	case CAM_DATA_SG_PADDR:
		device_printf(sc->mpi3mr_dev, "%s: physical addresses not supported\n",
		    __func__);
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_INVALID);
		mpi3mr_release_command(cm);
		xpt_done(ccb);
		return;
	case CAM_DATA_SG:
		device_printf(sc->mpi3mr_dev, "%s: scatter gather is not supported\n",
		    __func__);
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_INVALID);
		mpi3mr_release_command(cm);
		xpt_done(ccb);
		return;
	case CAM_DATA_VADDR:
	case CAM_DATA_BIO:
		if (csio->dxfer_len > (MPI3MR_SG_DEPTH * MPI3MR_4K_PGSZ)) {
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_TOO_BIG);
			mpi3mr_release_command(cm);
			xpt_done(ccb);
			return;
		}
		ccb->ccb_h.status |= CAM_SIM_QUEUED;
		cm->length = csio->dxfer_len;
		if (cm->length)
			cm->data = csio->data_ptr;
		break;
	default:
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_INVALID);
		mpi3mr_release_command(cm);
		xpt_done(ccb);
		return;
	}

	/* Prepare SGEs and queue to hardware */
	mpi3mr_map_request(sc, cm);
}

static void
mpi3mr_enqueue_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cm)
{
	static int ratelimit;
	struct mpi3mr_op_req_queue *opreqq = &sc->op_req_q[cm->req_qidx];
	struct mpi3mr_throttle_group_info *tg = NULL;
	uint32_t data_len_blks = 0;
	uint32_t tracked_io_sz = 0;
	uint32_t ioc_pend_data_len = 0, tg_pend_data_len = 0;
	struct mpi3mr_target *targ = cm->targ;
	union ccb *ccb = cm->ccb;
	Mpi3SCSIIORequest_t *req = (Mpi3SCSIIORequest_t *)&cm->io_request;

	if (sc->iot_enable) {
		data_len_blks = ccb->csio.dxfer_len >> 9;

		if ((data_len_blks >= sc->io_throttle_data_length) &&
		    targ->io_throttle_enabled) {

			tracked_io_sz = data_len_blks;
			tg = targ->throttle_group;
			if (tg) {
				mpi3mr_atomic_add(&sc->pend_large_data_sz, data_len_blks);
				mpi3mr_atomic_add(&tg->pend_large_data_sz, data_len_blks);
				
				ioc_pend_data_len = mpi3mr_atomic_read(&sc->pend_large_data_sz);
				tg_pend_data_len = mpi3mr_atomic_read(&tg->pend_large_data_sz);

				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"large vd_io persist_id(%d), handle(0x%04x), data_len(%d),"
						"ioc_pending(%d), tg_pending(%d), ioc_high(%d), tg_high(%d)\n",
						targ->per_id, targ->dev_handle,
						data_len_blks, ioc_pend_data_len,
						tg_pend_data_len, sc->io_throttle_high,
						tg->high);
					ratelimit++;
				}

				if (!tg->io_divert  && ((ioc_pend_data_len >=
				    sc->io_throttle_high) ||
				    (tg_pend_data_len >= tg->high))) {
					tg->io_divert = 1;
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"VD: Setting divert flag for tg_id(%d), persist_id(%d)\n",
						tg->id, targ->per_id);
					if (sc->mpi3mr_debug | MPI3MR_IOT)
						mpi3mr_print_cdb(ccb);
					mpi3mr_set_io_divert_for_all_vd_in_tg(sc,
					    tg, 1);
				}
			} else {
				mpi3mr_atomic_add(&sc->pend_large_data_sz, data_len_blks);
				ioc_pend_data_len = mpi3mr_atomic_read(&sc->pend_large_data_sz);
				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
					    "large pd_io persist_id(%d), handle(0x%04x), data_len(%d), ioc_pending(%d), ioc_high(%d)\n",
					    targ->per_id, targ->dev_handle,
					    data_len_blks, ioc_pend_data_len,
					    sc->io_throttle_high);
					ratelimit++;
				}

				if (ioc_pend_data_len >= sc->io_throttle_high) {
					targ->io_divert = 1;
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"PD: Setting divert flag for persist_id(%d)\n",
						targ->per_id);
					if (sc->mpi3mr_debug | MPI3MR_IOT)
						mpi3mr_print_cdb(ccb);
				}
			}
		}

		if (targ->io_divert) {
			req->MsgFlags |= MPI3_SCSIIO_MSGFLAGS_DIVERT_TO_FIRMWARE;
			req->Flags = htole32(le32toh(req->Flags) | MPI3_SCSIIO_FLAGS_DIVERT_REASON_IO_THROTTLING);
		}
	}

	if (mpi3mr_submit_io(sc, opreqq, (U8 *)&cm->io_request)) {
		if (tracked_io_sz) {
			mpi3mr_atomic_sub(&sc->pend_large_data_sz, tracked_io_sz);
			if (tg)
				mpi3mr_atomic_sub(&tg->pend_large_data_sz, tracked_io_sz);
		}
		mpi3mr_set_ccbstatus(ccb, CAM_RESRC_UNAVAIL);
		mpi3mr_release_command(cm);
		xpt_done(ccb);
	} else {
		callout_reset_sbt(&cm->callout, mstosbt(ccb->ccb_h.timeout), 0,
		    mpi3mr_scsiio_timeout, cm, 0);
		cm->callout_owner = true;
		mpi3mr_atomic_inc(&sc->fw_outstanding);
		mpi3mr_atomic_inc(&targ->outstanding);
		if (mpi3mr_atomic_read(&sc->fw_outstanding) > sc->io_cmds_highwater)
			sc->io_cmds_highwater++;
	}

	return;
}

static void
mpi3mr_cam_poll(struct cam_sim *sim)
{
	struct mpi3mr_cam_softc *cam_sc;
	struct mpi3mr_irq_context *irq_ctx;
	struct mpi3mr_softc *sc;
	int i;

	cam_sc = cam_sim_softc(sim);
	sc = cam_sc->sc;

	mpi3mr_dprint(cam_sc->sc, MPI3MR_TRACE, "func: %s line: %d is called\n",
		__func__, __LINE__);

	for (i = 0; i < sc->num_queues; i++) {
		irq_ctx = sc->irq_ctx + i;
		if (irq_ctx->op_reply_q->qid) {
			mpi3mr_complete_io_cmd(sc, irq_ctx);
		}
	}
}

static void
mpi3mr_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct mpi3mr_cam_softc *cam_sc;
	struct mpi3mr_target *targ;

	cam_sc = cam_sim_softc(sim);

	mpi3mr_dprint(cam_sc->sc, MPI3MR_TRACE, "ccb func_code 0x%x target id: 0x%x\n",
	    ccb->ccb_h.func_code, ccb->ccb_h.target_id);

	mtx_assert(&cam_sc->sc->mpi3mr_mtx, MA_OWNED);

	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET | PIM_UNMAPPED | PIM_NOSCAN;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = cam_sc->maxtargets - 1;
		cpi->max_lun = 0;

		/*
		 * initiator_id is set here to an ID outside the set of valid
		 * target IDs (including volumes).
		 */
		cpi->initiator_id = cam_sc->maxtargets;
		strlcpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strlcpy(cpi->hba_vid, "Broadcom", HBA_IDLEN);
		strlcpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = cam_sim_bus(sim);
		/*
		 * XXXSLM-I think this needs to change based on config page or
		 * something instead of hardcoded to 150000.
		 */
		cpi->base_transfer_speed = 150000;
		cpi->transport = XPORT_SAS;
		cpi->transport_version = 0;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_SPC;

		targ = mpi3mr_find_target_by_per_id(cam_sc, ccb->ccb_h.target_id);

		if (targ && (targ->dev_type == MPI3_DEVICE_DEVFORM_PCIE) &&
		    ((targ->dev_spec.pcie_inf.dev_info &
		    MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_MASK) ==
		    MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_NVME_DEVICE)) {
			cpi->maxio = targ->dev_spec.pcie_inf.mdts;
			mpi3mr_dprint(cam_sc->sc, MPI3MR_XINFO,
				"PCI device target_id: %u max io size: %u\n",
				ccb->ccb_h.target_id, cpi->maxio);
		} else {
			cpi->maxio = PAGE_SIZE * (MPI3MR_SG_DEPTH - 1);
		}
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings	*cts;
		struct ccb_trans_settings_sas	*sas;
		struct ccb_trans_settings_scsi	*scsi;

		cts = &ccb->cts;
		sas = &cts->xport_specific.sas;
		scsi = &cts->proto_specific.scsi;

		KASSERT(cts->ccb_h.target_id < cam_sc->maxtargets,
		    ("Target %d out of bounds in XPT_GET_TRAN_SETTINGS\n",
		    cts->ccb_h.target_id));
		targ = mpi3mr_find_target_by_per_id(cam_sc, cts->ccb_h.target_id);
		
		if (targ == NULL) {
			mpi3mr_dprint(cam_sc->sc, MPI3MR_TRACE, "Device with target ID: 0x%x does not exist\n",
			cts->ccb_h.target_id);
			mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
			break;
		}

		if ((targ->dev_handle == 0x0) || (targ->dev_removed == 1))  {
			mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
			break;
		}

		cts->protocol_version = SCSI_REV_SPC2;
		cts->transport = XPORT_SAS;
		cts->transport_version = 0;

		sas->valid = CTS_SAS_VALID_SPEED;
		
		switch (targ->link_rate) {
		case 0x08:
			sas->bitrate = 150000;
			break;
		case 0x09:
			sas->bitrate = 300000;
			break;
		case 0x0a:
			sas->bitrate = 600000;
			break;
		case 0x0b:
			sas->bitrate = 1200000;
			break;
		default:
			sas->valid = 0;
		}

		cts->protocol = PROTO_SCSI;
		scsi->valid = CTS_SCSI_VALID_TQ;
		scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;

		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_CALC_GEOMETRY:
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		break;
	case XPT_RESET_DEV:
		mpi3mr_dprint(cam_sc->sc, MPI3MR_INFO, "mpi3mr_action "
		    "XPT_RESET_DEV\n");
		return;
	case XPT_RESET_BUS:
	case XPT_ABORT:
	case XPT_TERM_IO:
		mpi3mr_dprint(cam_sc->sc, MPI3MR_INFO, "mpi3mr_action faking success "
		    "for abort or reset\n");
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		break;
	case XPT_SCSI_IO:
		mpi3mr_action_scsiio(cam_sc, ccb);
		return;
	default:
		mpi3mr_set_ccbstatus(ccb, CAM_FUNC_NOTAVAIL);
		break;
	}
	xpt_done(ccb);
}

void
mpi3mr_startup_increment(struct mpi3mr_cam_softc *cam_sc)
{
	if ((cam_sc->flags & MPI3MRSAS_IN_STARTUP) != 0) {
		if (cam_sc->startup_refcount++ == 0) {
			/* just starting, freeze the simq */
			mpi3mr_dprint(cam_sc->sc, MPI3MR_XINFO,
			    "%s freezing simq\n", __func__);
			xpt_hold_boot();
		}
		mpi3mr_dprint(cam_sc->sc, MPI3MR_XINFO, "%s refcount %u\n", __func__,
		    cam_sc->startup_refcount);
	}
}

void
mpi3mr_release_simq_reinit(struct mpi3mr_cam_softc *cam_sc)
{
	if (cam_sc->flags & MPI3MRSAS_QUEUE_FROZEN) {
		cam_sc->flags &= ~MPI3MRSAS_QUEUE_FROZEN;
		xpt_release_simq(cam_sc->sim, 1);
		mpi3mr_dprint(cam_sc->sc, MPI3MR_INFO, "Unfreezing SIM queue\n");
	}
}

void
mpi3mr_rescan_target(struct mpi3mr_softc *sc, struct mpi3mr_target *targ)
{
	struct mpi3mr_cam_softc *cam_sc = sc->cam_sc;
	path_id_t pathid;
	target_id_t targetid;
	union ccb *ccb;

	pathid = cam_sim_path(cam_sc->sim);
	if (targ == NULL)
		targetid = CAM_TARGET_WILDCARD;
	else
		targetid = targ->per_id;

	/*
	 * Allocate a CCB and schedule a rescan.
	 */
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "unable to alloc CCB for rescan\n");
		return;
	}

	if (xpt_create_path(&ccb->ccb_h.path, NULL, pathid, targetid,
	    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "unable to create path for rescan\n");
		xpt_free_ccb(ccb);
		return;
	}

	if (targetid == CAM_TARGET_WILDCARD)
		ccb->ccb_h.func_code = XPT_SCAN_BUS;
	else
		ccb->ccb_h.func_code = XPT_SCAN_TGT;

	mpi3mr_dprint(sc, MPI3MR_EVENT, "%s target id 0x%x\n", __func__, targetid);
	xpt_rescan(ccb);
}

void
mpi3mr_startup_decrement(struct mpi3mr_cam_softc *cam_sc)
{
	if ((cam_sc->flags & MPI3MRSAS_IN_STARTUP) != 0) {
		if (--cam_sc->startup_refcount == 0) {
			/* finished all discovery-related actions, release
			 * the simq and rescan for the latest topology.
			 */
			mpi3mr_dprint(cam_sc->sc, MPI3MR_XINFO,
			    "%s releasing simq\n", __func__);
			cam_sc->flags &= ~MPI3MRSAS_IN_STARTUP;
			xpt_release_simq(cam_sc->sim, 1);
			xpt_release_boot();
		}
		mpi3mr_dprint(cam_sc->sc, MPI3MR_XINFO, "%s refcount %u\n", __func__,
		    cam_sc->startup_refcount);
	}
}

static void
mpi3mr_fw_event_free(struct mpi3mr_softc *sc, struct mpi3mr_fw_event_work *fw_event)
{
	if (!fw_event)
		return;

	if (fw_event->event_data != NULL) {
		free(fw_event->event_data, M_MPI3MR);
		fw_event->event_data = NULL;
	}

	free(fw_event, M_MPI3MR);
	fw_event = NULL;
}

static void
mpi3mr_freeup_events(struct mpi3mr_softc *sc)
{
	struct mpi3mr_fw_event_work *fw_event = NULL;
	mtx_lock(&sc->mpi3mr_mtx);
	while ((fw_event = TAILQ_FIRST(&sc->cam_sc->ev_queue)) != NULL) {
		TAILQ_REMOVE(&sc->cam_sc->ev_queue, fw_event, ev_link);
		mpi3mr_fw_event_free(sc, fw_event);
	}
	mtx_unlock(&sc->mpi3mr_mtx);
}

static void
mpi3mr_sastopochg_evt_debug(struct mpi3mr_softc *sc,
	Mpi3EventDataSasTopologyChangeList_t *event_data)
{
	int i;
	U16 handle;
	U8 reason_code, phy_number;
	char *status_str = NULL;
	U8 link_rate, prev_link_rate;

	switch (event_data->ExpStatus) {
	case MPI3_EVENT_SAS_TOPO_ES_NOT_RESPONDING:
		status_str = "remove";
		break;
	case MPI3_EVENT_SAS_TOPO_ES_RESPONDING:
		status_str =  "responding";
		break;
	case MPI3_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING:
		status_str = "remove delay";
		break;
	case MPI3_EVENT_SAS_TOPO_ES_NO_EXPANDER:
		status_str = "direct attached";
		break;
	default:
		status_str = "unknown status";
		break;
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "%s :sas topology change: (%s)\n",
	    __func__, status_str);
	mpi3mr_dprint(sc, MPI3MR_INFO,
		"%s :\texpander_handle(0x%04x), enclosure_handle(0x%04x) "
	    "start_phy(%02d), num_entries(%d)\n", __func__,
	    (event_data->ExpanderDevHandle),
	    (event_data->EnclosureHandle),
	    event_data->StartPhyNum, event_data->NumEntries);
	for (i = 0; i < event_data->NumEntries; i++) {
		handle = (event_data->PhyEntry[i].AttachedDevHandle);
		if (!handle)
			continue;
		phy_number = event_data->StartPhyNum + i;
		reason_code = event_data->PhyEntry[i].Status &
		    MPI3_EVENT_SAS_TOPO_PHY_RC_MASK;
		switch (reason_code) {
		case MPI3_EVENT_SAS_TOPO_PHY_RC_TARG_NOT_RESPONDING:
			status_str = "target remove";
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_DELAY_NOT_RESPONDING:
			status_str = "delay target remove";
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_PHY_CHANGED:
			status_str = "link rate change";
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_NO_CHANGE:
			status_str = "target responding";
			break;
		default:
			status_str = "unknown";
			break;
		}
		link_rate = event_data->PhyEntry[i].LinkRate >> 4;
		prev_link_rate = event_data->PhyEntry[i].LinkRate & 0xF;
		mpi3mr_dprint(sc, MPI3MR_INFO, "%s :\tphy(%02d), attached_handle(0x%04x): %s:"
		    " link rate: new(0x%02x), old(0x%02x)\n", __func__,
		    phy_number, handle, status_str, link_rate, prev_link_rate);
	}
}

static void
mpi3mr_process_sastopochg_evt(struct mpi3mr_softc *sc, struct mpi3mr_fw_event_work *fwevt)
{

	Mpi3EventDataSasTopologyChangeList_t *event_data =
		    (Mpi3EventDataSasTopologyChangeList_t *)fwevt->event_data;
	int i;
	U16 handle;
	U8 reason_code, link_rate;
	struct mpi3mr_target *target = NULL;
	
	
	mpi3mr_sastopochg_evt_debug(sc, event_data);

	for (i = 0; i < event_data->NumEntries; i++) {
		handle = le16toh(event_data->PhyEntry[i].AttachedDevHandle);
		link_rate = event_data->PhyEntry[i].LinkRate >> 4;
		
		if (!handle)
			continue;
		target = mpi3mr_find_target_by_dev_handle(sc->cam_sc, handle);

		if (!target)
			continue;
		
		target->link_rate = link_rate;
		reason_code = event_data->PhyEntry[i].Status &
			MPI3_EVENT_SAS_TOPO_PHY_RC_MASK;

		switch (reason_code) {
		case MPI3_EVENT_SAS_TOPO_PHY_RC_TARG_NOT_RESPONDING:
			if (target->exposed_to_os)
				mpi3mr_remove_device_from_os(sc, target->dev_handle);
			mpi3mr_remove_device_from_list(sc, target, false);
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_PHY_CHANGED:
			break;
		default:
			break;
		}
	}

	/*
	 * refcount was incremented for this event in
	 * mpi3mr_evt_handler. Decrement it here because the event has
	 * been processed.
	 */
	mpi3mr_startup_decrement(sc->cam_sc);
	return;
}

static inline void
mpi3mr_logdata_evt_bh(struct mpi3mr_softc *sc,
		      struct mpi3mr_fw_event_work *fwevt)
{
	mpi3mr_app_save_logdata(sc, fwevt->event_data,
				fwevt->event_data_size);
}

static void
mpi3mr_pcietopochg_evt_debug(struct mpi3mr_softc *sc,
	Mpi3EventDataPcieTopologyChangeList_t *event_data)
{
	int i;
	U16 handle;
	U16 reason_code;
	U8 port_number;
	char *status_str = NULL;
	U8 link_rate, prev_link_rate;

	switch (event_data->SwitchStatus) {
	case MPI3_EVENT_PCIE_TOPO_SS_NOT_RESPONDING:
		status_str = "remove";
		break;
	case MPI3_EVENT_PCIE_TOPO_SS_RESPONDING:
		status_str =  "responding";
		break;
	case MPI3_EVENT_PCIE_TOPO_SS_DELAY_NOT_RESPONDING:
		status_str = "remove delay";
		break;
	case MPI3_EVENT_PCIE_TOPO_SS_NO_PCIE_SWITCH:
		status_str = "direct attached";
		break;
	default:
		status_str = "unknown status";
		break;
	}
	mpi3mr_dprint(sc, MPI3MR_INFO, "%s :pcie topology change: (%s)\n",
		__func__, status_str);
	mpi3mr_dprint(sc, MPI3MR_INFO,
		"%s :\tswitch_handle(0x%04x), enclosure_handle(0x%04x)"
		"start_port(%02d), num_entries(%d)\n", __func__,
		le16toh(event_data->SwitchDevHandle),
		le16toh(event_data->EnclosureHandle),
		event_data->StartPortNum, event_data->NumEntries);
	for (i = 0; i < event_data->NumEntries; i++) {
		handle =
			le16toh(event_data->PortEntry[i].AttachedDevHandle);
		if (!handle)
			continue;
		port_number = event_data->StartPortNum + i;
		reason_code = event_data->PortEntry[i].PortStatus;
		switch (reason_code) {
		case MPI3_EVENT_PCIE_TOPO_PS_NOT_RESPONDING:
			status_str = "target remove";
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_DELAY_NOT_RESPONDING:
			status_str = "delay target remove";
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_PORT_CHANGED:
			status_str = "link rate change";
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_NO_CHANGE:
			status_str = "target responding";
			break;
		default:
			status_str = "unknown";
			break;
		}
		link_rate = event_data->PortEntry[i].CurrentPortInfo &
			MPI3_EVENT_PCIE_TOPO_PI_RATE_MASK;
		prev_link_rate = event_data->PortEntry[i].PreviousPortInfo &
			MPI3_EVENT_PCIE_TOPO_PI_RATE_MASK;
		mpi3mr_dprint(sc, MPI3MR_INFO, "%s :\tport(%02d), attached_handle(0x%04x): %s:"
		    " link rate: new(0x%02x), old(0x%02x)\n", __func__,
		    port_number, handle, status_str, link_rate, prev_link_rate);
	}
}

static void mpi3mr_process_pcietopochg_evt(struct mpi3mr_softc *sc,
    struct mpi3mr_fw_event_work *fwevt)
{
	Mpi3EventDataPcieTopologyChangeList_t *event_data =
		    (Mpi3EventDataPcieTopologyChangeList_t *)fwevt->event_data;
	int i;
	U16 handle;
	U8 reason_code, link_rate;
	struct mpi3mr_target *target = NULL;
	

	mpi3mr_pcietopochg_evt_debug(sc, event_data);

	for (i = 0; i < event_data->NumEntries; i++) {
		handle =
			le16toh(event_data->PortEntry[i].AttachedDevHandle);
		if (!handle)
			continue;
		target = mpi3mr_find_target_by_dev_handle(sc->cam_sc, handle);
		if (!target)
			continue;
		
		link_rate = event_data->PortEntry[i].CurrentPortInfo &
			MPI3_EVENT_PCIE_TOPO_PI_RATE_MASK;
		target->link_rate = link_rate;

		reason_code = event_data->PortEntry[i].PortStatus;

		switch (reason_code) {
		case MPI3_EVENT_PCIE_TOPO_PS_NOT_RESPONDING:
			if (target->exposed_to_os)
				mpi3mr_remove_device_from_os(sc, target->dev_handle);
			mpi3mr_remove_device_from_list(sc, target, false);
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_PORT_CHANGED:
			break;
		default:
			break;
		}
	}

	/*
	 * refcount was incremented for this event in
	 * mpi3mr_evt_handler. Decrement it here because the event has
	 * been processed.
	 */
	mpi3mr_startup_decrement(sc->cam_sc);
	return;
}

void mpi3mr_add_device(struct mpi3mr_softc *sc, U16 per_id)
{
	struct mpi3mr_target *target;

	mpi3mr_dprint(sc, MPI3MR_EVENT,
		"Adding device(persistent id: 0x%x)\n", per_id);

	mpi3mr_startup_increment(sc->cam_sc);
	target = mpi3mr_find_target_by_per_id(sc->cam_sc, per_id);
	
	if (!target) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "Not available in driver's"
		    "internal target list, persistent_id: %d\n",
		    per_id);
		goto out;
	}

	if (target->is_hidden) {
		mpi3mr_dprint(sc, MPI3MR_EVENT, "Target is hidden, persistent_id: %d\n",
			per_id);
		goto out;
	}

	if (!target->exposed_to_os && !sc->reset_in_progress) {
		mpi3mr_rescan_target(sc, target);
		mpi3mr_dprint(sc, MPI3MR_INFO,
			"Added device persistent_id: %d dev_handle: %d\n", per_id, target->dev_handle);
		target->exposed_to_os = 1;
	}

out:
	mpi3mr_startup_decrement(sc->cam_sc);
}

int mpi3mr_remove_device_from_os(struct mpi3mr_softc *sc, U16 handle)
{
	U32 i = 0;
	int retval = 0;
	struct mpi3mr_target *target;

	mpi3mr_dprint(sc, MPI3MR_EVENT,
		"Removing Device (dev_handle: %d)\n", handle);
			
	target = mpi3mr_find_target_by_dev_handle(sc->cam_sc, handle);
	
	if (!target) {
		mpi3mr_dprint(sc, MPI3MR_INFO,
			"Device (persistent_id: %d dev_handle: %d) is already removed from driver's list\n",
			target->per_id, handle);
		mpi3mr_rescan_target(sc, NULL);
		retval = -1;
		goto out;
	}

	target->flags |= MPI3MRSAS_TARGET_INREMOVAL;

	while (mpi3mr_atomic_read(&target->outstanding) && (i < 30)) {
		i++;
		if (!(i % 2)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2d]waiting for "
			    "waiting for outstanding commands to complete on target: %d\n",
			    i, target->per_id);
		}
		DELAY(1000 * 1000);
	}
	
	if (target->exposed_to_os && !sc->reset_in_progress) {
		mpi3mr_rescan_target(sc, target);
		mpi3mr_dprint(sc, MPI3MR_INFO,
			"Removed device(persistent_id: %d dev_handle: %d)\n", target->per_id, handle);
		target->exposed_to_os = 0;
	}

	target->flags &= ~MPI3MRSAS_TARGET_INREMOVAL;
out:
	return retval;
}

void mpi3mr_remove_device_from_list(struct mpi3mr_softc *sc,
	struct mpi3mr_target *target, bool must_delete)
{
	mtx_lock_spin(&sc->target_lock);
	if ((target->state == MPI3MR_DEV_REMOVE_HS_STARTED) ||
	    (must_delete == true)) {
		TAILQ_REMOVE(&sc->cam_sc->tgt_list, target, tgt_next);
		target->state = MPI3MR_DEV_DELETED;
	}
	mtx_unlock_spin(&sc->target_lock);

	if (target->state == MPI3MR_DEV_DELETED) {
 		free(target, M_MPI3MR);
 		target = NULL;
 	}
	
	return;
}

/**
 * mpi3mr_devstatuschg_evt_bh - DevStatusChange evt bottomhalf
 * @sc: Adapter instance reference
 * @fwevt: Firmware event
 *
 * Process Device Status Change event and based on device's new
 * information, either expose the device to the upper layers, or
 * remove the device from upper layers.
 *
 * Return: Nothing.
 */
static void mpi3mr_devstatuschg_evt_bh(struct mpi3mr_softc *sc,
	struct mpi3mr_fw_event_work *fwevt)
{
	U16 dev_handle = 0;
	U8 uhide = 0, delete = 0, cleanup = 0;
	struct mpi3mr_target *tgtdev = NULL;
	Mpi3EventDataDeviceStatusChange_t *evtdata =
	    (Mpi3EventDataDeviceStatusChange_t *)fwevt->event_data;
	


	dev_handle = le16toh(evtdata->DevHandle);
	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "%s :device status change: handle(0x%04x): reason code(0x%x)\n",
	    __func__, dev_handle, evtdata->ReasonCode);
	switch (evtdata->ReasonCode) {
	case MPI3_EVENT_DEV_STAT_RC_HIDDEN:
		delete = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_NOT_HIDDEN:
		uhide = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_VD_NOT_RESPONDING:
		delete = 1;
		cleanup = 1;
		break;
	default:
		mpi3mr_dprint(sc, MPI3MR_INFO, "%s :Unhandled reason code(0x%x)\n", __func__,
		    evtdata->ReasonCode);
		break;
	}

	tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, dev_handle);
	if (!tgtdev)
		return;

	if (uhide) {
		if (!tgtdev->exposed_to_os)
			mpi3mr_add_device(sc, tgtdev->per_id);
	}

	if (delete)
		mpi3mr_remove_device_from_os(sc, dev_handle);

	if (cleanup)
		mpi3mr_remove_device_from_list(sc, tgtdev, false);
}

/**
 * mpi3mr_devinfochg_evt_bh - DeviceInfoChange evt bottomhalf
 * @sc: Adapter instance reference
 * @dev_pg0: New device page0
 *
 * Process Device Info Change event and based on device's new
 * information, either expose the device to the upper layers, or
 * remove the device from upper layers or update the details of
 * the device.
 *
 * Return: Nothing.
 */
static void mpi3mr_devinfochg_evt_bh(struct mpi3mr_softc *sc,
	Mpi3DevicePage0_t *dev_pg0)
{
	struct mpi3mr_target *tgtdev = NULL;
	U16 dev_handle = 0, perst_id = 0;

	perst_id = le16toh(dev_pg0->PersistentID);
	dev_handle = le16toh(dev_pg0->DevHandle);
	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "%s :Device info change: handle(0x%04x): persist_id(0x%x)\n",
	    __func__, dev_handle, perst_id);
	tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, dev_handle);
	if (!tgtdev)
		return;
	
	mpi3mr_update_device(sc, tgtdev, dev_pg0, false);
	if (!tgtdev->is_hidden && !tgtdev->exposed_to_os)
		mpi3mr_add_device(sc, perst_id);

	if (tgtdev->is_hidden && tgtdev->exposed_to_os)
		mpi3mr_remove_device_from_os(sc, tgtdev->dev_handle);
}

static void
mpi3mr_fw_work(struct mpi3mr_softc *sc, struct mpi3mr_fw_event_work *fw_event)
{
	if (sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN)
		goto out;

	if (!fw_event->process_event)
		goto evt_ack;

	mpi3mr_dprint(sc, MPI3MR_EVENT, "(%d)->(%s) Working on  Event: [%x]\n",
	    event_count++, __func__, fw_event->event);

	switch (fw_event->event) {
	case MPI3_EVENT_DEVICE_ADDED:
	{
		Mpi3DevicePage0_t *dev_pg0 =
			(Mpi3DevicePage0_t *) fw_event->event_data;
		mpi3mr_add_device(sc, dev_pg0->PersistentID);
		break;
	}
	case MPI3_EVENT_DEVICE_INFO_CHANGED:
	{
		mpi3mr_devinfochg_evt_bh(sc,
		    (Mpi3DevicePage0_t *) fw_event->event_data);
		break;
	}
	case MPI3_EVENT_DEVICE_STATUS_CHANGE:
	{
		mpi3mr_devstatuschg_evt_bh(sc, fw_event);
		break;
	}
	case MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
	{
		mpi3mr_process_sastopochg_evt(sc, fw_event);
		break;
	}
	case MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST:
	{
		mpi3mr_process_pcietopochg_evt(sc, fw_event);
		break;
	}
	case MPI3_EVENT_LOG_DATA:
	{
		mpi3mr_logdata_evt_bh(sc, fw_event);
		break;
	}
	default:
		mpi3mr_dprint(sc, MPI3MR_TRACE,"Unhandled event 0x%0X\n",
		    fw_event->event);
		break;

	}

evt_ack:
	if (fw_event->send_ack) {
		mpi3mr_dprint(sc, MPI3MR_EVENT,"Process event ACK for event 0x%0X\n",
		    fw_event->event);
		mpi3mr_process_event_ack(sc, fw_event->event,
		    fw_event->event_context);
	}

out:
	mpi3mr_dprint(sc, MPI3MR_EVENT, "(%d)->(%s) Event Free: [%x]\n", event_count,
	    __func__, fw_event->event);
	
	mpi3mr_fw_event_free(sc, fw_event);
}

void
mpi3mr_firmware_event_work(void *arg, int pending)
{
	struct mpi3mr_fw_event_work *fw_event;
	struct mpi3mr_softc *sc;

	sc = (struct mpi3mr_softc *)arg;

	mtx_lock(&sc->fwevt_lock);
	while ((fw_event = TAILQ_FIRST(&sc->cam_sc->ev_queue)) != NULL) {
		TAILQ_REMOVE(&sc->cam_sc->ev_queue, fw_event, ev_link);
		mtx_unlock(&sc->fwevt_lock);
		mpi3mr_fw_work(sc, fw_event);
		mtx_lock(&sc->fwevt_lock);
	}
	mtx_unlock(&sc->fwevt_lock);
}


/*
 * mpi3mr_cam_attach - CAM layer registration
 * @sc: Adapter reference
 *
 * This function does simq allocation, cam registration, xpt_bus registration,
 * event taskqueue initialization and async event handler registration.
 *
 * Return: 0 on success and proper error codes on failure
 */
int
mpi3mr_cam_attach(struct mpi3mr_softc *sc)
{
	struct mpi3mr_cam_softc *cam_sc;
	cam_status status;
	int unit, error = 0, reqs;

	mpi3mr_dprint(sc, MPI3MR_XINFO, "Starting CAM Attach\n");

	cam_sc = malloc(sizeof(struct mpi3mr_cam_softc), M_MPI3MR, M_WAITOK|M_ZERO);
	if (!cam_sc) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Failed to allocate memory for controller CAM instance\n");
		return (ENOMEM);
	}

	cam_sc->maxtargets = sc->facts.max_perids + 1;

	TAILQ_INIT(&cam_sc->tgt_list);
	
	sc->cam_sc = cam_sc;
	cam_sc->sc = sc;

	reqs = sc->max_host_ios;

	if ((cam_sc->devq = cam_simq_alloc(reqs)) == NULL) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocate SIMQ\n");
		error = ENOMEM;
		goto out;
	}

	unit = device_get_unit(sc->mpi3mr_dev);
	cam_sc->sim = cam_sim_alloc(mpi3mr_cam_action, mpi3mr_cam_poll, "mpi3mr", cam_sc,
	    unit, &sc->mpi3mr_mtx, reqs, reqs, cam_sc->devq);
	if (cam_sc->sim == NULL) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocate SIM\n");
		error = EINVAL;
		goto out;
	}

	TAILQ_INIT(&cam_sc->ev_queue);

	/* Initialize taskqueue for Event Handling */
	TASK_INIT(&cam_sc->ev_task, 0, mpi3mr_firmware_event_work, sc);
	cam_sc->ev_tq = taskqueue_create("mpi3mr_taskq", M_NOWAIT | M_ZERO,
	    taskqueue_thread_enqueue, &cam_sc->ev_tq);
	taskqueue_start_threads(&cam_sc->ev_tq, 1, PRIBIO, "%s taskq", 
	    device_get_nameunit(sc->mpi3mr_dev));

	mtx_lock(&sc->mpi3mr_mtx);

	/*
	 * XXX There should be a bus for every port on the adapter, but since
	 * we're just going to fake the topology for now, we'll pretend that
	 * everything is just a target on a single bus.
	 */
	if ((error = xpt_bus_register(cam_sc->sim, sc->mpi3mr_dev, 0)) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Error 0x%x registering SCSI bus\n", error);
		mtx_unlock(&sc->mpi3mr_mtx);
		goto out;
	}

	/*
	 * Assume that discovery events will start right away.
	 *
	 * Hold off boot until discovery is complete.
	 */
	cam_sc->flags |= MPI3MRSAS_IN_STARTUP | MPI3MRSAS_IN_DISCOVERY;
	sc->cam_sc->startup_refcount = 0;
	mpi3mr_startup_increment(cam_sc);

	callout_init(&cam_sc->discovery_callout, 1 /*mpsafe*/);

	/*
	 * Register for async events so we can determine the EEDP
	 * capabilities of devices.
	 */
	status = xpt_create_path(&cam_sc->path, /*periph*/NULL,
	    cam_sim_path(sc->cam_sc->sim), CAM_TARGET_WILDCARD,
	    CAM_LUN_WILDCARD);
	if (status != CAM_REQ_CMP) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Error 0x%x creating sim path\n", status);
		cam_sc->path = NULL;
	}

	if (status != CAM_REQ_CMP) {
		/*
		 * EEDP use is the exception, not the rule.
		 * Warn the user, but do not fail to attach.
		 */
		mpi3mr_dprint(sc, MPI3MR_INFO, "EEDP capabilities disabled.\n");
	}

	mtx_unlock(&sc->mpi3mr_mtx);

	error = mpi3mr_register_events(sc);

out:
	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s Exiting CAM attach, error: 0x%x n", __func__, error);
	return (error);
}

int
mpi3mr_cam_detach(struct mpi3mr_softc *sc)
{
	struct mpi3mr_cam_softc *cam_sc;
	struct mpi3mr_target *target;

	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s, Starting CAM detach\n", __func__);
	if (sc->cam_sc == NULL)
		return (0);

	cam_sc = sc->cam_sc;
	
	mpi3mr_freeup_events(sc);
	
	/*
	 * Drain and free the event handling taskqueue with the lock
	 * unheld so that any parallel processing tasks drain properly
	 * without deadlocking.
	 */
	if (cam_sc->ev_tq != NULL)
		taskqueue_free(cam_sc->ev_tq);

	mtx_lock(&sc->mpi3mr_mtx);

	while (cam_sc->startup_refcount != 0)
		mpi3mr_startup_decrement(cam_sc);

	/* Deregister our async handler */
	if (cam_sc->path != NULL) {
		xpt_free_path(cam_sc->path);
		cam_sc->path = NULL;
	}

	if (cam_sc->flags & MPI3MRSAS_IN_STARTUP)
		xpt_release_simq(cam_sc->sim, 1);

	if (cam_sc->sim != NULL) {
		xpt_bus_deregister(cam_sim_path(cam_sc->sim));
		cam_sim_free(cam_sc->sim, FALSE);
	}

	mtx_unlock(&sc->mpi3mr_mtx);

	if (cam_sc->devq != NULL)
		cam_simq_free(cam_sc->devq);

get_target:
	mtx_lock_spin(&sc->target_lock);
 	TAILQ_FOREACH(target, &cam_sc->tgt_list, tgt_next) {
 		TAILQ_REMOVE(&sc->cam_sc->tgt_list, target, tgt_next);
		mtx_unlock_spin(&sc->target_lock);
		goto out_tgt_free;
	}
	mtx_unlock_spin(&sc->target_lock);
out_tgt_free: 
	if (target) {
		free(target, M_MPI3MR);
		target = NULL;
		goto get_target;
 	}

	free(cam_sc, M_MPI3MR);
	sc->cam_sc = NULL;

	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s, Exiting CAM detach\n", __func__);
	return (0);
}
