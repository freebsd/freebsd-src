/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023, Broadcom Inc. All rights reserved.
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

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
#include <sys/queue.h>
#include <sys/kthread.h>
#include "mpi3mr.h"
#include "mpi3mr_cam.h"
#include "mpi3mr_app.h"

static void mpi3mr_repost_reply_buf(struct mpi3mr_softc *sc,
	U64 reply_dma);
static int mpi3mr_complete_admin_cmd(struct mpi3mr_softc *sc);
static void mpi3mr_port_enable_complete(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drvrcmd);
static void mpi3mr_flush_io(struct mpi3mr_softc *sc);
static int mpi3mr_issue_reset(struct mpi3mr_softc *sc, U16 reset_type,
	U32 reset_reason);
static void mpi3mr_dev_rmhs_send_tm(struct mpi3mr_softc *sc, U16 handle,
	struct mpi3mr_drvr_cmd *cmdparam, U8 iou_rc);
static void mpi3mr_dev_rmhs_complete_iou(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drv_cmd);
static void mpi3mr_dev_rmhs_complete_tm(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drv_cmd);
static void mpi3mr_send_evt_ack(struct mpi3mr_softc *sc, U8 event,
	struct mpi3mr_drvr_cmd *cmdparam, U32 event_ctx);
static void mpi3mr_print_fault_info(struct mpi3mr_softc *sc);
static inline void mpi3mr_set_diagsave(struct mpi3mr_softc *sc);
static const char *mpi3mr_reset_rc_name(enum mpi3mr_reset_reason reason_code);

void
mpi3mr_hexdump(void *buf, int sz, int format)
{
        int i;
        U32 *buf_loc = (U32 *)buf;

        for (i = 0; i < (sz / sizeof(U32)); i++) {
                if ((i % format) == 0) {
                        if (i != 0)
                                printf("\n");
                        printf("%08x: ", (i * 4));
                }
                printf("%08x ", buf_loc[i]);
        }
        printf("\n");
}

void
init_completion(struct completion *completion)
{
	completion->done = 0;
}

void
complete(struct completion *completion)
{
	completion->done = 1;
	wakeup(complete);
}

void wait_for_completion_timeout(struct completion *completion,
	    U32 timeout)
{
	U32 count = timeout * 1000;

	while ((completion->done == 0) && count) {
                DELAY(1000);
		count--;
	}

	if (completion->done == 0) {
		printf("%s: Command is timedout\n", __func__);
		completion->done = 1;
	}
}
void wait_for_completion_timeout_tm(struct completion *completion,
	    U32 timeout, struct mpi3mr_softc *sc)
{
	U32 count = timeout * 1000;

	while ((completion->done == 0) && count) {
		msleep(&sc->tm_chan, &sc->mpi3mr_mtx, PRIBIO,
		       "TM command", 1 * hz);
		count--;
	}

	if (completion->done == 0) {
		printf("%s: Command is timedout\n", __func__);
		completion->done = 1;
	}
}


void
poll_for_command_completion(struct mpi3mr_softc *sc,
       struct mpi3mr_drvr_cmd *cmd, U16 wait)
{
	int wait_time = wait * 1000;
       while (wait_time) {
               mpi3mr_complete_admin_cmd(sc);
               if (cmd->state & MPI3MR_CMD_COMPLETE)
                       break;
	       DELAY(1000);
               wait_time--;
       }
}

/**
 * mpi3mr_trigger_snapdump - triggers firmware snapdump
 * @sc: Adapter instance reference
 * @reason_code: reason code for the fault.
 *
 * This routine will trigger the snapdump and wait for it to
 * complete or timeout before it returns.
 * This will be called during initilaization time faults/resets/timeouts
 * before soft reset invocation.
 *
 * Return:  None.
 */
static void
mpi3mr_trigger_snapdump(struct mpi3mr_softc *sc, U32 reason_code)
{
	U32 host_diagnostic, timeout = MPI3_SYSIF_DIAG_SAVE_TIMEOUT * 10;
	
	mpi3mr_dprint(sc, MPI3MR_INFO, "snapdump triggered: reason code: %s\n",
	    mpi3mr_reset_rc_name(reason_code));

	mpi3mr_set_diagsave(sc);
	mpi3mr_issue_reset(sc, MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT,
			   reason_code);
	
	do {
		host_diagnostic = mpi3mr_regread(sc, MPI3_SYSIF_HOST_DIAG_OFFSET);
		if (!(host_diagnostic & MPI3_SYSIF_HOST_DIAG_SAVE_IN_PROGRESS))
			break;
                DELAY(100 * 1000);
	} while (--timeout);

	return;
}

/**
 * mpi3mr_check_rh_fault_ioc - check reset history and fault
 * controller
 * @sc: Adapter instance reference
 * @reason_code, reason code for the fault.
 *
 * This routine will fault the controller with
 * the given reason code if it is not already in the fault or
 * not asynchronosuly reset. This will be used to handle
 * initilaization time faults/resets/timeout as in those cases
 * immediate soft reset invocation is not required.
 *
 * Return:  None.
 */
static void mpi3mr_check_rh_fault_ioc(struct mpi3mr_softc *sc, U32 reason_code)
{
	U32 ioc_status;

	if (sc->unrecoverable) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "controller is unrecoverable\n");
		return;
	}
	
	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	if ((ioc_status & MPI3_SYSIF_IOC_STATUS_RESET_HISTORY) ||
	    (ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT)) {
		mpi3mr_print_fault_info(sc);
		return;
	}
	
	mpi3mr_trigger_snapdump(sc, reason_code);
	
	return;
}

static void * mpi3mr_get_reply_virt_addr(struct mpi3mr_softc *sc,
    bus_addr_t phys_addr)
{
	if (!phys_addr)
		return NULL;
	if ((phys_addr < sc->reply_buf_dma_min_address) ||
	    (phys_addr > sc->reply_buf_dma_max_address))
		return NULL;

	return sc->reply_buf + (phys_addr - sc->reply_buf_phys);
}

static void * mpi3mr_get_sensebuf_virt_addr(struct mpi3mr_softc *sc,
    bus_addr_t phys_addr)
{
	if (!phys_addr)
		return NULL;
	return sc->sense_buf + (phys_addr - sc->sense_buf_phys);
}

static void mpi3mr_repost_reply_buf(struct mpi3mr_softc *sc,
    U64 reply_dma)
{
	U32 old_idx = 0;

	mtx_lock_spin(&sc->reply_free_q_lock);
	old_idx  =  sc->reply_free_q_host_index;
	sc->reply_free_q_host_index = ((sc->reply_free_q_host_index ==
	    (sc->reply_free_q_sz - 1)) ? 0 :
	    (sc->reply_free_q_host_index + 1));
	sc->reply_free_q[old_idx] = reply_dma;
	mpi3mr_regwrite(sc, MPI3_SYSIF_REPLY_FREE_HOST_INDEX_OFFSET,
		sc->reply_free_q_host_index);
	mtx_unlock_spin(&sc->reply_free_q_lock);
}

static void mpi3mr_repost_sense_buf(struct mpi3mr_softc *sc,
    U64 sense_buf_phys)
{
	U32 old_idx = 0;

	mtx_lock_spin(&sc->sense_buf_q_lock);
	old_idx  =  sc->sense_buf_q_host_index;
	sc->sense_buf_q_host_index = ((sc->sense_buf_q_host_index ==
	    (sc->sense_buf_q_sz - 1)) ? 0 :
	    (sc->sense_buf_q_host_index + 1));
	sc->sense_buf_q[old_idx] = sense_buf_phys;
	mpi3mr_regwrite(sc, MPI3_SYSIF_SENSE_BUF_FREE_HOST_INDEX_OFFSET,
		sc->sense_buf_q_host_index);
	mtx_unlock_spin(&sc->sense_buf_q_lock);

}

void mpi3mr_set_io_divert_for_all_vd_in_tg(struct mpi3mr_softc *sc,
	struct mpi3mr_throttle_group_info *tg, U8 divert_value)
{
	struct mpi3mr_target *target;

	mtx_lock_spin(&sc->target_lock);
	TAILQ_FOREACH(target, &sc->cam_sc->tgt_list, tgt_next) {
		if (target->throttle_group == tg)
			target->io_divert = divert_value;
	}
	mtx_unlock_spin(&sc->target_lock);
}

/**
 * mpi3mr_submit_admin_cmd - Submit request to admin queue
 * @mrioc: Adapter reference
 * @admin_req: MPI3 request
 * @admin_req_sz: Request size
 *
 * Post the MPI3 request into admin request queue and
 * inform the controller, if the queue is full return
 * appropriate error.
 *
 * Return: 0 on success, non-zero on failure.
 */
int mpi3mr_submit_admin_cmd(struct mpi3mr_softc *sc, void *admin_req,
    U16 admin_req_sz)
{
	U16 areq_pi = 0, areq_ci = 0, max_entries = 0;
	int retval = 0;
	U8 *areq_entry;

	mtx_lock_spin(&sc->admin_req_lock);
	areq_pi = sc->admin_req_pi;
	areq_ci = sc->admin_req_ci;
	max_entries = sc->num_admin_reqs;
	
	if (sc->unrecoverable)
		return -EFAULT;
	
	if ((areq_ci == (areq_pi + 1)) || ((!areq_ci) &&
					   (areq_pi == (max_entries - 1)))) {
		printf(IOCNAME "AdminReqQ full condition detected\n",
		    sc->name);
		retval = -EAGAIN;
		goto out;
	}
	areq_entry = (U8 *)sc->admin_req + (areq_pi *
						     MPI3MR_AREQ_FRAME_SZ);
	memset(areq_entry, 0, MPI3MR_AREQ_FRAME_SZ);
	memcpy(areq_entry, (U8 *)admin_req, admin_req_sz);

	if (++areq_pi == max_entries)
		areq_pi = 0;
	sc->admin_req_pi = areq_pi;

	mpi3mr_regwrite(sc, MPI3_SYSIF_ADMIN_REQ_Q_PI_OFFSET, sc->admin_req_pi);

out:
	mtx_unlock_spin(&sc->admin_req_lock);
	return retval;
}

/**
 * mpi3mr_check_req_qfull - Check request queue is full or not
 * @op_req_q: Operational reply queue info
 *
 * Return: true when queue full, false otherwise.
 */
static inline bool
mpi3mr_check_req_qfull(struct mpi3mr_op_req_queue *op_req_q)
{
	U16 pi, ci, max_entries;
	bool is_qfull = false;

	pi = op_req_q->pi;
	ci = op_req_q->ci;
	max_entries = op_req_q->num_reqs;

	if ((ci == (pi + 1)) || ((!ci) && (pi == (max_entries - 1))))
		is_qfull = true;

	return is_qfull;
}

/**
 * mpi3mr_submit_io - Post IO command to firmware
 * @sc:		      Adapter instance reference
 * @op_req_q:	      Operational Request queue reference
 * @req:	      MPT request data
 *
 * This function submits IO command to firmware.
 *
 * Return: Nothing
 */
int mpi3mr_submit_io(struct mpi3mr_softc *sc,
    struct mpi3mr_op_req_queue *op_req_q, U8 *req)
{
	U16 pi, max_entries;
	int retval = 0;
	U8 *req_entry;
	U16 req_sz = sc->facts.op_req_sz;
	struct mpi3mr_irq_context *irq_ctx;
	
	mtx_lock_spin(&op_req_q->q_lock);

	pi = op_req_q->pi;
	max_entries = op_req_q->num_reqs;
	if (mpi3mr_check_req_qfull(op_req_q)) {
		irq_ctx = &sc->irq_ctx[op_req_q->reply_qid - 1];
		mpi3mr_complete_io_cmd(sc, irq_ctx);

		if (mpi3mr_check_req_qfull(op_req_q)) {
			printf(IOCNAME "OpReqQ full condition detected\n",
				sc->name);
			retval = -EBUSY;
			goto out;
		}
	}

	req_entry = (U8 *)op_req_q->q_base + (pi * req_sz);
	memset(req_entry, 0, req_sz);
	memcpy(req_entry, req, MPI3MR_AREQ_FRAME_SZ);
	if (++pi == max_entries)
		pi = 0;
	op_req_q->pi = pi;

	mpi3mr_atomic_inc(&sc->op_reply_q[op_req_q->reply_qid - 1].pend_ios);

	mpi3mr_regwrite(sc, MPI3_SYSIF_OPER_REQ_Q_N_PI_OFFSET(op_req_q->qid), op_req_q->pi);
	if (sc->mpi3mr_debug & MPI3MR_TRACE) {
		device_printf(sc->mpi3mr_dev, "IO submission: QID:%d PI:0x%x\n", op_req_q->qid, op_req_q->pi);
		mpi3mr_hexdump(req_entry, MPI3MR_AREQ_FRAME_SZ, 8);
	}

out:
	mtx_unlock_spin(&op_req_q->q_lock);
	return retval;
}

inline void 
mpi3mr_add_sg_single(void *paddr, U8 flags, U32 length,
		     bus_addr_t dma_addr)
{
	Mpi3SGESimple_t *sgel = paddr;

	sgel->Flags = flags;
	sgel->Length = (length);
	sgel->Address = (U64)dma_addr;
}

void mpi3mr_build_zero_len_sge(void *paddr)
{
	U8 sgl_flags = (MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE |
		MPI3_SGE_FLAGS_DLAS_SYSTEM | MPI3_SGE_FLAGS_END_OF_LIST);

	mpi3mr_add_sg_single(paddr, sgl_flags, 0, -1);

}

void mpi3mr_enable_interrupts(struct mpi3mr_softc *sc)
{
	sc->intr_enabled = 1;
}

void mpi3mr_disable_interrupts(struct mpi3mr_softc *sc)
{
	sc->intr_enabled = 0;
}

void
mpi3mr_memaddr_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *addr;

	addr = arg;
	*addr = segs[0].ds_addr;
}

static int mpi3mr_delete_op_reply_queue(struct mpi3mr_softc *sc, U16 qid)
{
	Mpi3DeleteReplyQueueRequest_t delq_req;
	struct mpi3mr_op_reply_queue *op_reply_q;
	int retval = 0;


	op_reply_q = &sc->op_reply_q[qid - 1];

	if (!op_reply_q->qid)
	{
		retval = -1;
		printf(IOCNAME "Issue DelRepQ: called with invalid Reply QID\n",
		    sc->name);
		goto out;
	}

	memset(&delq_req, 0, sizeof(delq_req));
	
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue DelRepQ: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue DelRepQ: Init command is in use\n",
		    sc->name);
		goto out;
	}
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	delq_req.HostTag = MPI3MR_HOSTTAG_INITCMDS;
	delq_req.Function = MPI3_FUNCTION_DELETE_REPLY_QUEUE;
	delq_req.QueueID = qid;

	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &delq_req, sizeof(delq_req));
	if (retval) {
		printf(IOCNAME "Issue DelRepQ: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}
	wait_for_completion_timeout(&sc->init_cmds.completion,
	    (MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "Issue DelRepQ: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
		    MPI3MR_RESET_FROM_DELREPQ_TIMEOUT);
		sc->unrecoverable = 1;

		retval = -1;
		goto out_unlock;
	}
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "Issue DelRepQ: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}
	sc->irq_ctx[qid - 1].op_reply_q = NULL;
	
	if (sc->op_reply_q[qid - 1].q_base_phys != 0)
		bus_dmamap_unload(sc->op_reply_q[qid - 1].q_base_tag, sc->op_reply_q[qid - 1].q_base_dmamap);
	if (sc->op_reply_q[qid - 1].q_base != NULL)
		bus_dmamem_free(sc->op_reply_q[qid - 1].q_base_tag, sc->op_reply_q[qid - 1].q_base, sc->op_reply_q[qid - 1].q_base_dmamap);
	if (sc->op_reply_q[qid - 1].q_base_tag != NULL)
		bus_dma_tag_destroy(sc->op_reply_q[qid - 1].q_base_tag);

	sc->op_reply_q[qid - 1].q_base = NULL;
	sc->op_reply_q[qid - 1].qid = 0;
out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);
out:
	return retval;
}

/**
 * mpi3mr_create_op_reply_queue - create operational reply queue
 * @sc: Adapter instance reference
 * @qid: operational reply queue id
 *
 * Create operatinal reply queue by issuing MPI request
 * through admin queue.
 *
 * Return:  0 on success, non-zero on failure.
 */
static int mpi3mr_create_op_reply_queue(struct mpi3mr_softc *sc, U16 qid)
{
	Mpi3CreateReplyQueueRequest_t create_req;
	struct mpi3mr_op_reply_queue *op_reply_q;
	int retval = 0;
	char q_lock_name[32];

	op_reply_q = &sc->op_reply_q[qid - 1];

	if (op_reply_q->qid)
	{
		retval = -1;
		printf(IOCNAME "CreateRepQ: called for duplicate qid %d\n",
		    sc->name, op_reply_q->qid);
		return retval;
	}

	op_reply_q->ci = 0;
	if (pci_get_revid(sc->mpi3mr_dev) == SAS4116_CHIP_REV_A0)
		op_reply_q->num_replies = MPI3MR_OP_REP_Q_QD_A0;
	else
		op_reply_q->num_replies = MPI3MR_OP_REP_Q_QD;

	op_reply_q->qsz = op_reply_q->num_replies * sc->op_reply_sz;
	op_reply_q->ephase = 1;
       
        if (!op_reply_q->q_base) {
		snprintf(q_lock_name, 32, "Reply Queue Lock[%d]", qid);
		mtx_init(&op_reply_q->q_lock, q_lock_name, NULL, MTX_SPIN);

		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
					4, 0,			/* algnmnt, boundary */
					sc->dma_loaddr,		/* lowaddr */
					sc->dma_hiaddr,		/* highaddr */
					NULL, NULL,		/* filter, filterarg */
					op_reply_q->qsz,		/* maxsize */
					1,			/* nsegments */
					op_reply_q->qsz,		/* maxsegsize */
					0,			/* flags */
					NULL, NULL,		/* lockfunc, lockarg */
					&op_reply_q->q_base_tag)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate Operational reply DMA tag\n");
			return (ENOMEM);
		}

		if (bus_dmamem_alloc(op_reply_q->q_base_tag, (void **)&op_reply_q->q_base,
		    BUS_DMA_NOWAIT, &op_reply_q->q_base_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
			return (ENOMEM);
		}
		bzero(op_reply_q->q_base, op_reply_q->qsz);
		bus_dmamap_load(op_reply_q->q_base_tag, op_reply_q->q_base_dmamap, op_reply_q->q_base, op_reply_q->qsz,
		    mpi3mr_memaddr_cb, &op_reply_q->q_base_phys, BUS_DMA_NOWAIT);
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Operational Reply queue ID: %d phys addr= %#016jx virt_addr: %pa size= %d\n",
		    qid, (uintmax_t)op_reply_q->q_base_phys, op_reply_q->q_base, op_reply_q->qsz);
		
		if (!op_reply_q->q_base)
		{
			retval = -1;
			printf(IOCNAME "CreateRepQ: memory alloc failed for qid %d\n",
			    sc->name, qid);
			goto out;
		}
	}

	memset(&create_req, 0, sizeof(create_req));
	
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "CreateRepQ: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}

	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	create_req.HostTag = MPI3MR_HOSTTAG_INITCMDS;
	create_req.Function = MPI3_FUNCTION_CREATE_REPLY_QUEUE;
	create_req.QueueID = qid;
	create_req.Flags = MPI3_CREATE_REPLY_QUEUE_FLAGS_INT_ENABLE_ENABLE;
	create_req.MSIxIndex = sc->irq_ctx[qid - 1].msix_index;
	create_req.BaseAddress = (U64)op_reply_q->q_base_phys;
	create_req.Size = op_reply_q->num_replies;

	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &create_req,
	    sizeof(create_req));
	if (retval) {
		printf(IOCNAME "CreateRepQ: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}
	
	wait_for_completion_timeout(&sc->init_cmds.completion,
	  	MPI3MR_INTADMCMD_TIMEOUT);
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "CreateRepQ: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
		    MPI3MR_RESET_FROM_CREATEREPQ_TIMEOUT);
		sc->unrecoverable = 1;
		retval = -1;
		goto out_unlock;
	}
	
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "CreateRepQ: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}
	op_reply_q->qid = qid;
	sc->irq_ctx[qid - 1].op_reply_q = op_reply_q;

out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);
out:
	if (retval) {
		if (op_reply_q->q_base_phys != 0)
			bus_dmamap_unload(op_reply_q->q_base_tag, op_reply_q->q_base_dmamap);
		if (op_reply_q->q_base != NULL)
			bus_dmamem_free(op_reply_q->q_base_tag, op_reply_q->q_base, op_reply_q->q_base_dmamap);
		if (op_reply_q->q_base_tag != NULL)
			bus_dma_tag_destroy(op_reply_q->q_base_tag);
		op_reply_q->q_base = NULL;
		op_reply_q->qid = 0;
	}
	
	return retval;
}

/**
 * mpi3mr_create_op_req_queue - create operational request queue
 * @sc: Adapter instance reference
 * @req_qid: operational request queue id
 * @reply_qid: Reply queue ID
 *
 * Create operatinal request queue by issuing MPI request
 * through admin queue.
 *
 * Return:  0 on success, non-zero on failure.
 */
static int mpi3mr_create_op_req_queue(struct mpi3mr_softc *sc, U16 req_qid, U8 reply_qid)
{
	Mpi3CreateRequestQueueRequest_t create_req;
	struct mpi3mr_op_req_queue *op_req_q;
	int retval = 0;
	char q_lock_name[32];

	op_req_q = &sc->op_req_q[req_qid - 1];

	if (op_req_q->qid)
	{
		retval = -1;
		printf(IOCNAME "CreateReqQ: called for duplicate qid %d\n",
		    sc->name, op_req_q->qid);
		return retval;
	}
	
	op_req_q->ci = 0;
	op_req_q->pi = 0;
	op_req_q->num_reqs = MPI3MR_OP_REQ_Q_QD;
	op_req_q->qsz = op_req_q->num_reqs * sc->facts.op_req_sz;
	op_req_q->reply_qid = reply_qid;

	if (!op_req_q->q_base) {
		snprintf(q_lock_name, 32, "Request Queue Lock[%d]", req_qid);
		mtx_init(&op_req_q->q_lock, q_lock_name, NULL, MTX_SPIN);

		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
					4, 0,			/* algnmnt, boundary */
					sc->dma_loaddr,		/* lowaddr */
					sc->dma_hiaddr,		/* highaddr */
					NULL, NULL,		/* filter, filterarg */
					op_req_q->qsz,		/* maxsize */
					1,			/* nsegments */
					op_req_q->qsz,		/* maxsegsize */
					0,			/* flags */
					NULL, NULL,		/* lockfunc, lockarg */
					&op_req_q->q_base_tag)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
			return (ENOMEM);
		}

		if (bus_dmamem_alloc(op_req_q->q_base_tag, (void **)&op_req_q->q_base,
		    BUS_DMA_NOWAIT, &op_req_q->q_base_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
			return (ENOMEM);
		}

		bzero(op_req_q->q_base, op_req_q->qsz);
		
		bus_dmamap_load(op_req_q->q_base_tag, op_req_q->q_base_dmamap, op_req_q->q_base, op_req_q->qsz,
		    mpi3mr_memaddr_cb, &op_req_q->q_base_phys, BUS_DMA_NOWAIT);
		
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Operational Request QID: %d phys addr= %#016jx virt addr= %pa size= %d associated Reply QID: %d\n",
		    req_qid, (uintmax_t)op_req_q->q_base_phys, op_req_q->q_base, op_req_q->qsz, reply_qid);

		if (!op_req_q->q_base) {
			retval = -1;
			printf(IOCNAME "CreateReqQ: memory alloc failed for qid %d\n",
			    sc->name, req_qid);
			goto out;
		}
	}

	memset(&create_req, 0, sizeof(create_req));
	
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "CreateReqQ: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	create_req.HostTag = MPI3MR_HOSTTAG_INITCMDS;
	create_req.Function = MPI3_FUNCTION_CREATE_REQUEST_QUEUE;
	create_req.QueueID = req_qid;
	create_req.Flags = 0;
	create_req.ReplyQueueID = reply_qid;
	create_req.BaseAddress = (U64)op_req_q->q_base_phys;
	create_req.Size = op_req_q->num_reqs;

	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &create_req,
	    sizeof(create_req));
	if (retval) {
		printf(IOCNAME "CreateReqQ: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}
	
	wait_for_completion_timeout(&sc->init_cmds.completion,
	    (MPI3MR_INTADMCMD_TIMEOUT));
	
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "CreateReqQ: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
			MPI3MR_RESET_FROM_CREATEREQQ_TIMEOUT);
		sc->unrecoverable = 1;
		retval = -1;
		goto out_unlock;
	}
	
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "CreateReqQ: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}
	op_req_q->qid = req_qid;

out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);
out:
	if (retval) {
		if (op_req_q->q_base_phys != 0)
			bus_dmamap_unload(op_req_q->q_base_tag, op_req_q->q_base_dmamap);
		if (op_req_q->q_base != NULL)
			bus_dmamem_free(op_req_q->q_base_tag, op_req_q->q_base, op_req_q->q_base_dmamap);
		if (op_req_q->q_base_tag != NULL)
			bus_dma_tag_destroy(op_req_q->q_base_tag);
		op_req_q->q_base = NULL;
		op_req_q->qid = 0;
	}
	return retval;
}

/**
 * mpi3mr_create_op_queues - create operational queues
 * @sc: Adapter instance reference
 *
 * Create operatinal queues(request queues and reply queues).
 * Return:  0 on success, non-zero on failure.
 */
static int mpi3mr_create_op_queues(struct mpi3mr_softc *sc)
{
	int retval = 0;
	U16 num_queues = 0, i = 0, qid;

	num_queues = min(sc->facts.max_op_reply_q,
	    sc->facts.max_op_req_q);
	num_queues = min(num_queues, sc->msix_count);

	/*
	 * During reset set the num_queues to the number of queues
	 * that was set before the reset.
	 */
	if (sc->num_queues)
		num_queues = sc->num_queues;
	
	mpi3mr_dprint(sc, MPI3MR_XINFO, "Trying to create %d Operational Q pairs\n",
	    num_queues);

	if (!sc->op_req_q) {
		sc->op_req_q = malloc(sizeof(struct mpi3mr_op_req_queue) *
		    num_queues, M_MPI3MR, M_NOWAIT | M_ZERO);

		if (!sc->op_req_q) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to alloc memory for Request queue info\n");
			retval = -1;
			goto out_failed;
		}
	}

	if (!sc->op_reply_q) {
		sc->op_reply_q = malloc(sizeof(struct mpi3mr_op_reply_queue) * num_queues,
			M_MPI3MR, M_NOWAIT | M_ZERO);

		if (!sc->op_reply_q) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to alloc memory for Reply queue info\n");
			retval = -1;
			goto out_failed;
		}
	}

	sc->num_hosttag_op_req_q = (sc->max_host_ios + 1) / num_queues;

	/*Operational Request and reply queue ID starts with 1*/
	for (i = 0; i < num_queues; i++) {
		qid = i + 1;
		if (mpi3mr_create_op_reply_queue(sc, qid)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to create Reply queue %d\n",
			    qid);
			break;
		}
		if (mpi3mr_create_op_req_queue(sc, qid,
		    sc->op_reply_q[qid - 1].qid)) {
			mpi3mr_delete_op_reply_queue(sc, qid);
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to create Request queue %d\n",
			    qid);
			break;
		}

	}
	
	/* Not even one queue is created successfully*/
        if (i == 0) {
                retval = -1;
                goto out_failed;
        }

	if (!sc->num_queues) {
		sc->num_queues = i;
	} else {
		if (num_queues != i) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Number of queues (%d) post reset are not same as" 
					"queues allocated (%d) during driver init\n", i, num_queues);
			goto out_failed;
		}
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "Successfully created %d Operational Queue pairs\n",
	    sc->num_queues);
	mpi3mr_dprint(sc, MPI3MR_INFO, "Request Queue QD: %d Reply queue QD: %d\n",
	    sc->op_req_q[0].num_reqs, sc->op_reply_q[0].num_replies);

	return retval;
out_failed:
	if (sc->op_req_q) {
		free(sc->op_req_q, M_MPI3MR);
		sc->op_req_q = NULL;
	}
	if (sc->op_reply_q) {
		free(sc->op_reply_q, M_MPI3MR);
		sc->op_reply_q = NULL;
	}
	return retval;
}

/**
 * mpi3mr_setup_admin_qpair - Setup admin queue pairs
 * @sc: Adapter instance reference
 *
 * Allocation and setup admin queues(request queues and reply queues).
 * Return:  0 on success, non-zero on failure.
 */
static int mpi3mr_setup_admin_qpair(struct mpi3mr_softc *sc)
{
	int retval = 0;
	U32 num_adm_entries = 0;
	
	sc->admin_req_q_sz = MPI3MR_AREQQ_SIZE;
	sc->num_admin_reqs = sc->admin_req_q_sz / MPI3MR_AREQ_FRAME_SZ;
	sc->admin_req_ci = sc->admin_req_pi = 0;

	sc->admin_reply_q_sz = MPI3MR_AREPQ_SIZE;
	sc->num_admin_replies = sc->admin_reply_q_sz/ MPI3MR_AREP_FRAME_SZ;
	sc->admin_reply_ci = 0;
	sc->admin_reply_ephase = 1;

	if (!sc->admin_req) {
		/*
		 * We need to create the tag for the admin queue to get the
		 * iofacts to see how many bits the controller decodes.  Solve
		 * this chicken and egg problem by only doing lower 4GB DMA.
		 */
		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
					4, 0,			/* algnmnt, boundary */
					BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
					BUS_SPACE_MAXADDR,	/* highaddr */
					NULL, NULL,		/* filter, filterarg */
					sc->admin_req_q_sz,	/* maxsize */
					1,			/* nsegments */
					sc->admin_req_q_sz,	/* maxsegsize */
					0,			/* flags */
					NULL, NULL,		/* lockfunc, lockarg */
					&sc->admin_req_tag)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
			return (ENOMEM);
		}

		if (bus_dmamem_alloc(sc->admin_req_tag, (void **)&sc->admin_req,
		    BUS_DMA_NOWAIT, &sc->admin_req_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
			return (ENOMEM);
		}
		bzero(sc->admin_req, sc->admin_req_q_sz);
		bus_dmamap_load(sc->admin_req_tag, sc->admin_req_dmamap, sc->admin_req, sc->admin_req_q_sz,
		    mpi3mr_memaddr_cb, &sc->admin_req_phys, BUS_DMA_NOWAIT);
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Admin Req queue phys addr= %#016jx size= %d\n",
		    (uintmax_t)sc->admin_req_phys, sc->admin_req_q_sz);
		
		if (!sc->admin_req)
		{
			retval = -1;
			printf(IOCNAME "Memory alloc for AdminReqQ: failed\n",
			    sc->name);
			goto out_failed;
		}
	}
	
	if (!sc->admin_reply) {
		mtx_init(&sc->admin_reply_lock, "Admin Reply Queue Lock", NULL, MTX_SPIN);
	
		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
					4, 0,			/* algnmnt, boundary */
					BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
					BUS_SPACE_MAXADDR,	/* highaddr */
					NULL, NULL,		/* filter, filterarg */
					sc->admin_reply_q_sz,	/* maxsize */
					1,			/* nsegments */
					sc->admin_reply_q_sz,	/* maxsegsize */
					0,			/* flags */
					NULL, NULL,		/* lockfunc, lockarg */
					&sc->admin_reply_tag)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate reply DMA tag\n");
			return (ENOMEM);
		}

		if (bus_dmamem_alloc(sc->admin_reply_tag, (void **)&sc->admin_reply,
		    BUS_DMA_NOWAIT, &sc->admin_reply_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
			return (ENOMEM);
		}
		bzero(sc->admin_reply, sc->admin_reply_q_sz);
		bus_dmamap_load(sc->admin_reply_tag, sc->admin_reply_dmamap, sc->admin_reply, sc->admin_reply_q_sz,
		    mpi3mr_memaddr_cb, &sc->admin_reply_phys, BUS_DMA_NOWAIT);
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Admin Reply queue phys addr= %#016jx size= %d\n",
		    (uintmax_t)sc->admin_reply_phys, sc->admin_req_q_sz);
		

		if (!sc->admin_reply)
		{
			retval = -1;
			printf(IOCNAME "Memory alloc for AdminRepQ: failed\n",
			    sc->name);
			goto out_failed;
		}
	}

	num_adm_entries = (sc->num_admin_replies << 16) |
				(sc->num_admin_reqs);
	mpi3mr_regwrite(sc, MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_OFFSET, num_adm_entries);
	mpi3mr_regwrite64(sc, MPI3_SYSIF_ADMIN_REQ_Q_ADDR_LOW_OFFSET, sc->admin_req_phys);
	mpi3mr_regwrite64(sc, MPI3_SYSIF_ADMIN_REPLY_Q_ADDR_LOW_OFFSET, sc->admin_reply_phys);
	mpi3mr_regwrite(sc, MPI3_SYSIF_ADMIN_REQ_Q_PI_OFFSET, sc->admin_req_pi);
	mpi3mr_regwrite(sc, MPI3_SYSIF_ADMIN_REPLY_Q_CI_OFFSET, sc->admin_reply_ci);
	
	return retval;

out_failed:
	/* Free Admin reply*/
	if (sc->admin_reply_phys)
		bus_dmamap_unload(sc->admin_reply_tag, sc->admin_reply_dmamap);
	
	if (sc->admin_reply != NULL)
		bus_dmamem_free(sc->admin_reply_tag, sc->admin_reply,
		    sc->admin_reply_dmamap);

	if (sc->admin_reply_tag != NULL)
		bus_dma_tag_destroy(sc->admin_reply_tag);
	
	/* Free Admin request*/
	if (sc->admin_req_phys)
		bus_dmamap_unload(sc->admin_req_tag, sc->admin_req_dmamap);

	if (sc->admin_req != NULL)
		bus_dmamem_free(sc->admin_req_tag, sc->admin_req,
		    sc->admin_req_dmamap);

	if (sc->admin_req_tag != NULL)
		bus_dma_tag_destroy(sc->admin_req_tag);

	return retval;
}

/**
 * mpi3mr_print_fault_info - Display fault information
 * @sc: Adapter instance reference
 *
 * Display the controller fault information if there is a
 * controller fault.
 *
 * Return: Nothing.
 */
static void mpi3mr_print_fault_info(struct mpi3mr_softc *sc)
{
	U32 ioc_status, code, code1, code2, code3;

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);

	if (ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT) {
		code = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_OFFSET) &
			MPI3_SYSIF_FAULT_CODE_MASK;
		code1 = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_INFO0_OFFSET);
		code2 = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_INFO1_OFFSET);
		code3 = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_INFO2_OFFSET);
		printf(IOCNAME "fault codes 0x%04x:0x%04x:0x%04x:0x%04x\n",
		    sc->name, code, code1, code2, code3);
	}
}

enum mpi3mr_iocstate mpi3mr_get_iocstate(struct mpi3mr_softc *sc)
{
	U32 ioc_status, ioc_control;
	U8 ready, enabled;
	
	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	ioc_control = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);

	if(sc->unrecoverable)
		return MRIOC_STATE_UNRECOVERABLE; 
	if (ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT)
		return MRIOC_STATE_FAULT;

	ready = (ioc_status & MPI3_SYSIF_IOC_STATUS_READY);
	enabled = (ioc_control & MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC);

	if (ready && enabled)
		return MRIOC_STATE_READY;
	if ((!ready) && (!enabled))
		return MRIOC_STATE_RESET;
	if ((!ready) && (enabled))
		return MRIOC_STATE_BECOMING_READY;

	return MRIOC_STATE_RESET_REQUESTED;
}

static inline void mpi3mr_clear_resethistory(struct mpi3mr_softc *sc)
{
        U32 ioc_status;

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
        if (ioc_status & MPI3_SYSIF_IOC_STATUS_RESET_HISTORY)
		mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_STATUS_OFFSET, ioc_status);

}

/**
 * mpi3mr_mur_ioc - Message unit Reset handler
 * @sc: Adapter instance reference
 * @reset_reason: Reset reason code
 *
 * Issue Message unit Reset to the controller and wait for it to
 * be complete.
 *
 * Return: 0 on success, -1 on failure.
 */
static int mpi3mr_mur_ioc(struct mpi3mr_softc *sc, U32 reset_reason)
{
        U32 ioc_config, timeout, ioc_status;
        int retval = -1;

        mpi3mr_dprint(sc, MPI3MR_INFO, "Issuing Message Unit Reset(MUR)\n");
        if (sc->unrecoverable) {
                mpi3mr_dprint(sc, MPI3MR_ERROR, "IOC is unrecoverable MUR not issued\n");
                return retval;
        }
        mpi3mr_clear_resethistory(sc);
	mpi3mr_regwrite(sc, MPI3_SYSIF_SCRATCHPAD0_OFFSET, reset_reason);
	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
        ioc_config &= ~MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC;
	mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET, ioc_config);

        timeout = MPI3MR_MUR_TIMEOUT * 10;
        do {
		ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
                if ((ioc_status & MPI3_SYSIF_IOC_STATUS_RESET_HISTORY)) {
                        mpi3mr_clear_resethistory(sc);
			ioc_config =
				mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
                        if (!((ioc_status & MPI3_SYSIF_IOC_STATUS_READY) ||
                            (ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT) ||
                            (ioc_config & MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC))) {
                                retval = 0;
                                break;
                        }
                }
                DELAY(100 * 1000);
        } while (--timeout);

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);

        mpi3mr_dprint(sc, MPI3MR_INFO, "IOC Status/Config after %s MUR is (0x%x)/(0x%x)\n",
                !retval ? "successful":"failed", ioc_status, ioc_config);
        return retval;
}

/**
 * mpi3mr_bring_ioc_ready - Bring controller to ready state
 * @sc: Adapter instance reference
 *
 * Set Enable IOC bit in IOC configuration register and wait for
 * the controller to become ready.
 *
 * Return: 0 on success, appropriate error on failure.
 */
static int mpi3mr_bring_ioc_ready(struct mpi3mr_softc *sc)
{
        U32 ioc_config, timeout;
        enum mpi3mr_iocstate current_state;

	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
        ioc_config |= MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC;
	mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET, ioc_config);

        timeout = sc->ready_timeout * 10;
        do {
                current_state = mpi3mr_get_iocstate(sc);
                if (current_state == MRIOC_STATE_READY)
                        return 0;
                DELAY(100 * 1000);
        } while (--timeout);

        return -1;
}

static const struct {
	enum mpi3mr_iocstate value;
	char *name;
} mrioc_states[] = {
	{ MRIOC_STATE_READY, "ready" },
	{ MRIOC_STATE_FAULT, "fault" },
	{ MRIOC_STATE_RESET, "reset" },
	{ MRIOC_STATE_BECOMING_READY, "becoming ready" },
	{ MRIOC_STATE_RESET_REQUESTED, "reset requested" },
	{ MRIOC_STATE_COUNT, "Count" },
};

static const char *mpi3mr_iocstate_name(enum mpi3mr_iocstate mrioc_state)
{
	int i;
	char *name = NULL;

	for (i = 0; i < MRIOC_STATE_COUNT; i++) {
		if (mrioc_states[i].value == mrioc_state){
			name = mrioc_states[i].name;
			break;
		}
	}
	return name;
}

/* Reset reason to name mapper structure*/
static const struct {
	enum mpi3mr_reset_reason value;
	char *name;
} mpi3mr_reset_reason_codes[] = {
	{ MPI3MR_RESET_FROM_BRINGUP, "timeout in bringup" },
	{ MPI3MR_RESET_FROM_FAULT_WATCH, "fault" },
	{ MPI3MR_RESET_FROM_IOCTL, "application" },
	{ MPI3MR_RESET_FROM_EH_HOS, "error handling" },
	{ MPI3MR_RESET_FROM_TM_TIMEOUT, "TM timeout" },
	{ MPI3MR_RESET_FROM_IOCTL_TIMEOUT, "IOCTL timeout" },
	{ MPI3MR_RESET_FROM_SCSIIO_TIMEOUT, "SCSIIO timeout" },
	{ MPI3MR_RESET_FROM_MUR_FAILURE, "MUR failure" },
	{ MPI3MR_RESET_FROM_CTLR_CLEANUP, "timeout in controller cleanup" },
	{ MPI3MR_RESET_FROM_CIACTIV_FAULT, "component image activation fault" },
	{ MPI3MR_RESET_FROM_PE_TIMEOUT, "port enable timeout" },
	{ MPI3MR_RESET_FROM_TSU_TIMEOUT, "time stamp update timeout" },
	{ MPI3MR_RESET_FROM_DELREQQ_TIMEOUT, "delete request queue timeout" },
	{ MPI3MR_RESET_FROM_DELREPQ_TIMEOUT, "delete reply queue timeout" },
	{
		MPI3MR_RESET_FROM_CREATEREPQ_TIMEOUT,
		"create request queue timeout"
	},
	{
		MPI3MR_RESET_FROM_CREATEREQQ_TIMEOUT,
		"create reply queue timeout"
	},
	{ MPI3MR_RESET_FROM_IOCFACTS_TIMEOUT, "IOC facts timeout" },
	{ MPI3MR_RESET_FROM_IOCINIT_TIMEOUT, "IOC init timeout" },
	{ MPI3MR_RESET_FROM_EVTNOTIFY_TIMEOUT, "event notify timeout" },
	{ MPI3MR_RESET_FROM_EVTACK_TIMEOUT, "event acknowledgment timeout" },
	{
		MPI3MR_RESET_FROM_CIACTVRST_TIMER,
		"component image activation timeout"
	},
	{
		MPI3MR_RESET_FROM_GETPKGVER_TIMEOUT,
		"get package version timeout"
	},
	{
		MPI3MR_RESET_FROM_PELABORT_TIMEOUT,
		"persistent event log abort timeout"
	},
	{ MPI3MR_RESET_FROM_SYSFS, "sysfs invocation" },
	{ MPI3MR_RESET_FROM_SYSFS_TIMEOUT, "sysfs TM timeout" },
	{
		MPI3MR_RESET_FROM_DIAG_BUFFER_POST_TIMEOUT,
		"diagnostic buffer post timeout"
	},
	{ MPI3MR_RESET_FROM_FIRMWARE, "firmware asynchronus reset" },
	{ MPI3MR_RESET_REASON_COUNT, "Reset reason count" },
};

/**
 * mpi3mr_reset_rc_name - get reset reason code name
 * @reason_code: reset reason code value
 *
 * Map reset reason to an NULL terminated ASCII string
 *
 * Return: Name corresponding to reset reason value or NULL.
 */
static const char *mpi3mr_reset_rc_name(enum mpi3mr_reset_reason reason_code)
{
	int i;
	char *name = NULL;

	for (i = 0; i < MPI3MR_RESET_REASON_COUNT; i++) {
		if (mpi3mr_reset_reason_codes[i].value == reason_code) {
			name = mpi3mr_reset_reason_codes[i].name;
			break;
		}
	}
	return name;
}

#define MAX_RESET_TYPE 3
/* Reset type to name mapper structure*/
static const struct {
	U16 reset_type;
	char *name;
} mpi3mr_reset_types[] = {
	{ MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET, "soft" },
	{ MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT, "diag fault" },
	{ MAX_RESET_TYPE, "count"}
};

/**
 * mpi3mr_reset_type_name - get reset type name
 * @reset_type: reset type value
 *
 * Map reset type to an NULL terminated ASCII string
 *
 * Return: Name corresponding to reset type value or NULL.
 */
static const char *mpi3mr_reset_type_name(U16 reset_type)
{
	int i;
	char *name = NULL;

	for (i = 0; i < MAX_RESET_TYPE; i++) {
		if (mpi3mr_reset_types[i].reset_type == reset_type) {
			name = mpi3mr_reset_types[i].name;
			break;
		}
	}
	return name;
}

/**
 * mpi3mr_soft_reset_success - Check softreset is success or not
 * @ioc_status: IOC status register value
 * @ioc_config: IOC config register value
 *
 * Check whether the soft reset is successful or not based on
 * IOC status and IOC config register values.
 *
 * Return: True when the soft reset is success, false otherwise.
 */
static inline bool
mpi3mr_soft_reset_success(U32 ioc_status, U32 ioc_config)
{
	if (!((ioc_status & MPI3_SYSIF_IOC_STATUS_READY) ||
	    (ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT) ||
	    (ioc_config & MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC)))
		return true;
	return false;
}

/**
 * mpi3mr_diagfault_success - Check diag fault is success or not
 * @sc: Adapter reference
 * @ioc_status: IOC status register value
 *
 * Check whether the controller hit diag reset fault code.
 *
 * Return: True when there is diag fault, false otherwise.
 */
static inline bool mpi3mr_diagfault_success(struct mpi3mr_softc *sc,
	U32 ioc_status)
{
	U32 fault;

	if (!(ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT))
		return false;
	fault = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_OFFSET) & MPI3_SYSIF_FAULT_CODE_MASK;
	if (fault == MPI3_SYSIF_FAULT_CODE_DIAG_FAULT_RESET)
		return true;
	return false;
}

/**
 * mpi3mr_issue_iocfacts - Send IOC Facts
 * @sc: Adapter instance reference
 * @facts_data: Cached IOC facts data
 *
 * Issue IOC Facts MPI request through admin queue and wait for
 * the completion of it or time out.
 *
 * Return: 0 on success, non-zero on failures.
 */
static int mpi3mr_issue_iocfacts(struct mpi3mr_softc *sc,
    Mpi3IOCFactsData_t *facts_data)
{
	Mpi3IOCFactsRequest_t iocfacts_req;
	bus_dma_tag_t data_tag = NULL;
	bus_dmamap_t data_map = NULL;
	bus_addr_t data_phys = 0;
	void *data = NULL;
	U32 data_len = sizeof(*facts_data);
	int retval = 0;
	
	U8 sgl_flags = (MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE |
                	MPI3_SGE_FLAGS_DLAS_SYSTEM |
			MPI3_SGE_FLAGS_END_OF_LIST);


	/*
	 * We can't use sc->dma_loaddr / hiaddr here.  We set those only after
	 * we get the iocfacts.  So allocate in the lower 4GB.  The amount of
	 * data is tiny and we don't do this that often, so any bouncing we
	 * might have to do isn't a cause for concern.
	 */
        if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				4, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                data_len,		/* maxsize */
                                1,			/* nsegments */
                                data_len,		/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &data_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
		return (ENOMEM);
        }

        if (bus_dmamem_alloc(data_tag, (void **)&data,
	    BUS_DMA_NOWAIT, &data_map)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d Data  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }

        bzero(data, data_len);
        bus_dmamap_load(data_tag, data_map, data, data_len,
	    mpi3mr_memaddr_cb, &data_phys, BUS_DMA_NOWAIT);
	mpi3mr_dprint(sc, MPI3MR_XINFO, "Func: %s line: %d IOCfacts data phys addr= %#016jx size= %d\n",
	    __func__, __LINE__, (uintmax_t)data_phys, data_len);
	
	if (!data)
	{
		retval = -1;
		printf(IOCNAME "Memory alloc for IOCFactsData: failed\n",
		    sc->name);
		goto out;
	}

	mtx_lock(&sc->init_cmds.completion.lock);
	memset(&iocfacts_req, 0, sizeof(iocfacts_req));

	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue IOCFacts: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}

	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	iocfacts_req.HostTag = (MPI3MR_HOSTTAG_INITCMDS);
	iocfacts_req.Function = MPI3_FUNCTION_IOC_FACTS;
	
	mpi3mr_add_sg_single(&iocfacts_req.SGL, sgl_flags, data_len,
	    data_phys);

	init_completion(&sc->init_cmds.completion);

	retval = mpi3mr_submit_admin_cmd(sc, &iocfacts_req,
	    sizeof(iocfacts_req));

	if (retval) {
		printf(IOCNAME "Issue IOCFacts: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}

	wait_for_completion_timeout(&sc->init_cmds.completion,
	    (MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "Issue IOCFacts: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
		    MPI3MR_RESET_FROM_IOCFACTS_TIMEOUT);
		sc->unrecoverable = 1;
		retval = -1;
		goto out_unlock;
	}
	
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "Issue IOCFacts: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}
	
	memcpy(facts_data, (U8 *)data, data_len);
out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	if (data_phys != 0)
		bus_dmamap_unload(data_tag, data_map);
	if (data != NULL)
		bus_dmamem_free(data_tag, data, data_map);
	if (data_tag != NULL)
		bus_dma_tag_destroy(data_tag);
	return retval;
}

/**
 * mpi3mr_process_factsdata - Process IOC facts data
 * @sc: Adapter instance reference
 * @facts_data: Cached IOC facts data
 *
 * Convert IOC facts data into cpu endianness and cache it in
 * the driver .
 *
 * Return: Nothing.
 */
static int mpi3mr_process_factsdata(struct mpi3mr_softc *sc,
    Mpi3IOCFactsData_t *facts_data)
{
	int retval = 0;
	U32 ioc_config, req_sz, facts_flags;
        struct mpi3mr_compimg_ver *fwver;

	if (le16toh(facts_data->IOCFactsDataLength) !=
	    (sizeof(*facts_data) / 4)) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "IOCFacts data length mismatch "
		    " driver_sz(%ld) firmware_sz(%d) \n",
		    sizeof(*facts_data),
		    facts_data->IOCFactsDataLength);
	}

	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
        req_sz = 1 << ((ioc_config & MPI3_SYSIF_IOC_CONFIG_OPER_REQ_ENT_SZ) >>
                  MPI3_SYSIF_IOC_CONFIG_OPER_REQ_ENT_SZ_SHIFT);

	if (facts_data->IOCRequestFrameSize != (req_sz/4)) {
		 mpi3mr_dprint(sc, MPI3MR_INFO, "IOCFacts data reqFrameSize mismatch "
		    " hw_size(%d) firmware_sz(%d) \n" , req_sz/4,
		    facts_data->IOCRequestFrameSize);
	}

	memset(&sc->facts, 0, sizeof(sc->facts));

	facts_flags = le32toh(facts_data->Flags);
	sc->facts.op_req_sz = req_sz;
	sc->op_reply_sz = 1 << ((ioc_config &
                                  MPI3_SYSIF_IOC_CONFIG_OPER_RPY_ENT_SZ) >>
                                  MPI3_SYSIF_IOC_CONFIG_OPER_RPY_ENT_SZ_SHIFT);

	sc->facts.ioc_num = facts_data->IOCNumber;
        sc->facts.who_init = facts_data->WhoInit;
        sc->facts.max_msix_vectors = facts_data->MaxMSIxVectors;
	sc->facts.personality = (facts_flags &
	    MPI3_IOCFACTS_FLAGS_PERSONALITY_MASK);
	sc->facts.dma_mask = (facts_flags &
	    MPI3_IOCFACTS_FLAGS_DMA_ADDRESS_WIDTH_MASK) >>
	    MPI3_IOCFACTS_FLAGS_DMA_ADDRESS_WIDTH_SHIFT;
        sc->facts.protocol_flags = facts_data->ProtocolFlags;
        sc->facts.mpi_version = (facts_data->MPIVersion.Word);
        sc->facts.max_reqs = (facts_data->MaxOutstandingRequests);
        sc->facts.product_id = (facts_data->ProductID);
	sc->facts.reply_sz = (facts_data->ReplyFrameSize) * 4;
        sc->facts.exceptions = (facts_data->IOCExceptions);
        sc->facts.max_perids = (facts_data->MaxPersistentID);
        sc->facts.max_vds = (facts_data->MaxVDs);
        sc->facts.max_hpds = (facts_data->MaxHostPDs);
        sc->facts.max_advhpds = (facts_data->MaxAdvHostPDs);
        sc->facts.max_raidpds = (facts_data->MaxRAIDPDs);
        sc->facts.max_nvme = (facts_data->MaxNVMe);
        sc->facts.max_pcieswitches =
                (facts_data->MaxPCIeSwitches);
        sc->facts.max_sasexpanders =
                (facts_data->MaxSASExpanders);
        sc->facts.max_sasinitiators =
                (facts_data->MaxSASInitiators);
        sc->facts.max_enclosures = (facts_data->MaxEnclosures);
        sc->facts.min_devhandle = (facts_data->MinDevHandle);
        sc->facts.max_devhandle = (facts_data->MaxDevHandle);
	sc->facts.max_op_req_q =
                (facts_data->MaxOperationalRequestQueues);
	sc->facts.max_op_reply_q =
                (facts_data->MaxOperationalReplyQueues);
        sc->facts.ioc_capabilities =
                (facts_data->IOCCapabilities);
        sc->facts.fw_ver.build_num =
                (facts_data->FWVersion.BuildNum);
        sc->facts.fw_ver.cust_id =
                (facts_data->FWVersion.CustomerID);
        sc->facts.fw_ver.ph_minor = facts_data->FWVersion.PhaseMinor;
        sc->facts.fw_ver.ph_major = facts_data->FWVersion.PhaseMajor;
        sc->facts.fw_ver.gen_minor = facts_data->FWVersion.GenMinor;
        sc->facts.fw_ver.gen_major = facts_data->FWVersion.GenMajor;
        sc->max_msix_vectors = min(sc->max_msix_vectors,
            sc->facts.max_msix_vectors);
        sc->facts.sge_mod_mask = facts_data->SGEModifierMask;
        sc->facts.sge_mod_value = facts_data->SGEModifierValue;
        sc->facts.sge_mod_shift = facts_data->SGEModifierShift;
        sc->facts.shutdown_timeout =
                (facts_data->ShutdownTimeout);
	sc->facts.max_dev_per_tg = facts_data->MaxDevicesPerThrottleGroup;
	sc->facts.io_throttle_data_length =
	    facts_data->IOThrottleDataLength;
	sc->facts.max_io_throttle_group =
	    facts_data->MaxIOThrottleGroup;
	sc->facts.io_throttle_low = facts_data->IOThrottleLow;
	sc->facts.io_throttle_high = facts_data->IOThrottleHigh;

	/*Store in 512b block count*/
	if (sc->facts.io_throttle_data_length)
		sc->io_throttle_data_length =
		    (sc->facts.io_throttle_data_length * 2 * 4);
	else
		/* set the length to 1MB + 1K to disable throttle*/
		sc->io_throttle_data_length = MPI3MR_MAX_SECTORS + 2;

	sc->io_throttle_high = (sc->facts.io_throttle_high * 2 * 1024);
	sc->io_throttle_low = (sc->facts.io_throttle_low * 2 * 1024);
        
	fwver = &sc->facts.fw_ver;
	snprintf(sc->fw_version, sizeof(sc->fw_version),
	    "%d.%d.%d.%d.%05d-%05d",
	    fwver->gen_major, fwver->gen_minor, fwver->ph_major,
	    fwver->ph_minor, fwver->cust_id, fwver->build_num);

	mpi3mr_dprint(sc, MPI3MR_INFO, "ioc_num(%d), maxopQ(%d), maxopRepQ(%d), maxdh(%d),"
            "maxreqs(%d), mindh(%d) maxPDs(%d) maxvectors(%d) maxperids(%d)\n",
	    sc->facts.ioc_num, sc->facts.max_op_req_q,
	    sc->facts.max_op_reply_q, sc->facts.max_devhandle,
            sc->facts.max_reqs, sc->facts.min_devhandle,
            sc->facts.max_pds, sc->facts.max_msix_vectors,
            sc->facts.max_perids);
        mpi3mr_dprint(sc, MPI3MR_INFO, "SGEModMask 0x%x SGEModVal 0x%x SGEModShift 0x%x\n",
            sc->facts.sge_mod_mask, sc->facts.sge_mod_value,
            sc->facts.sge_mod_shift);
	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "max_dev_per_throttle_group(%d), max_throttle_groups(%d), io_throttle_data_len(%dKiB), io_throttle_high(%dMiB), io_throttle_low(%dMiB)\n",
	    sc->facts.max_dev_per_tg, sc->facts.max_io_throttle_group,
	    sc->facts.io_throttle_data_length * 4,
	    sc->facts.io_throttle_high, sc->facts.io_throttle_low);

	sc->max_host_ios = sc->facts.max_reqs -
	    (MPI3MR_INTERNALCMDS_RESVD + 1);

	/*
	 * Set the DMA mask for the card.  dma_mask is the number of bits that
	 * can have bits set in them.  Translate this into bus_dma loaddr/hiaddr
	 * args.  Add sanity for more bits than address space or other overflow
	 * situations.
	 */
	if (sc->facts.dma_mask == 0 ||
	    (sc->facts.dma_mask >= sizeof(bus_addr_t) * 8))
		sc->dma_loaddr = BUS_SPACE_MAXADDR;
	else
		sc->dma_loaddr = ~((1ull << sc->facts.dma_mask) - 1);
	sc->dma_hiaddr = BUS_SPACE_MAXADDR;
	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "dma_mask bits: %d loaddr 0x%jx hiaddr 0x%jx\n",
	    sc->facts.dma_mask, sc->dma_loaddr, sc->dma_hiaddr);

	return retval;
}

static inline void mpi3mr_setup_reply_free_queues(struct mpi3mr_softc *sc)
{
	int i;
	bus_addr_t phys_addr;

	/* initialize Reply buffer Queue */
	for (i = 0, phys_addr = sc->reply_buf_phys;
	    i < sc->num_reply_bufs; i++, phys_addr += sc->reply_sz)
		sc->reply_free_q[i] = phys_addr;
	sc->reply_free_q[i] = (0);

	/* initialize Sense Buffer Queue */
	for (i = 0, phys_addr = sc->sense_buf_phys;
	    i < sc->num_sense_bufs; i++, phys_addr += MPI3MR_SENSEBUF_SZ)
		sc->sense_buf_q[i] = phys_addr;
	sc->sense_buf_q[i] = (0);

}

static int mpi3mr_reply_dma_alloc(struct mpi3mr_softc *sc)
{
	U32 sz;
	
	sc->num_reply_bufs = sc->facts.max_reqs + MPI3MR_NUM_EVTREPLIES;
	sc->reply_free_q_sz = sc->num_reply_bufs + 1;
	sc->num_sense_bufs = sc->facts.max_reqs / MPI3MR_SENSEBUF_FACTOR;
	sc->sense_buf_q_sz = sc->num_sense_bufs + 1;
        
	sz = sc->num_reply_bufs * sc->reply_sz;
	
	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,  /* parent */
				16, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                sz,			/* maxsize */
                                1,			/* nsegments */
                                sz,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->reply_buf_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
		return (ENOMEM);
        }
        
	if (bus_dmamem_alloc(sc->reply_buf_tag, (void **)&sc->reply_buf,
	    BUS_DMA_NOWAIT, &sc->reply_buf_dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }
        
	bzero(sc->reply_buf, sz);
        bus_dmamap_load(sc->reply_buf_tag, sc->reply_buf_dmamap, sc->reply_buf, sz,
	    mpi3mr_memaddr_cb, &sc->reply_buf_phys, BUS_DMA_NOWAIT);
	
	sc->reply_buf_dma_min_address = sc->reply_buf_phys;
	sc->reply_buf_dma_max_address = sc->reply_buf_phys + sz;
	mpi3mr_dprint(sc, MPI3MR_XINFO, "reply buf (0x%p): depth(%d), frame_size(%d), "
	    "pool_size(%d kB), reply_buf_dma(0x%llx)\n",
	    sc->reply_buf, sc->num_reply_bufs, sc->reply_sz,
	    (sz / 1024), (unsigned long long)sc->reply_buf_phys);

	/* reply free queue, 8 byte align */
	sz = sc->reply_free_q_sz * 8;

        if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				8, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                sz,			/* maxsize */
                                1,			/* nsegments */
                                sz,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->reply_free_q_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate reply free queue DMA tag\n");
		return (ENOMEM);
        }

        if (bus_dmamem_alloc(sc->reply_free_q_tag, (void **)&sc->reply_free_q,
	    BUS_DMA_NOWAIT, &sc->reply_free_q_dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }
        
	bzero(sc->reply_free_q, sz);
        bus_dmamap_load(sc->reply_free_q_tag, sc->reply_free_q_dmamap, sc->reply_free_q, sz,
	    mpi3mr_memaddr_cb, &sc->reply_free_q_phys, BUS_DMA_NOWAIT);
	
	mpi3mr_dprint(sc, MPI3MR_XINFO, "reply_free_q (0x%p): depth(%d), frame_size(%d), "
	    "pool_size(%d kB), reply_free_q_dma(0x%llx)\n",
	    sc->reply_free_q, sc->reply_free_q_sz, 8, (sz / 1024),
	    (unsigned long long)sc->reply_free_q_phys);

	/* sense buffer pool,  4 byte align */
	sz = sc->num_sense_bufs * MPI3MR_SENSEBUF_SZ;

        if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				4, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                sz,			/* maxsize */
                                1,			/* nsegments */
                                sz,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->sense_buf_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate Sense buffer DMA tag\n");
		return (ENOMEM);
        }
        
	if (bus_dmamem_alloc(sc->sense_buf_tag, (void **)&sc->sense_buf,
	    BUS_DMA_NOWAIT, &sc->sense_buf_dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }
        
	bzero(sc->sense_buf, sz);
        bus_dmamap_load(sc->sense_buf_tag, sc->sense_buf_dmamap, sc->sense_buf, sz,
	    mpi3mr_memaddr_cb, &sc->sense_buf_phys, BUS_DMA_NOWAIT);

	mpi3mr_dprint(sc, MPI3MR_XINFO, "sense_buf (0x%p): depth(%d), frame_size(%d), "
	    "pool_size(%d kB), sense_dma(0x%llx)\n",
	    sc->sense_buf, sc->num_sense_bufs, MPI3MR_SENSEBUF_SZ,
	    (sz / 1024), (unsigned long long)sc->sense_buf_phys);

	/* sense buffer queue, 8 byte align */
	sz = sc->sense_buf_q_sz * 8;

        if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				8, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                sz,			/* maxsize */
                                1,			/* nsegments */
                                sz,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->sense_buf_q_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate Sense buffer Queue DMA tag\n");
		return (ENOMEM);
        }
        
	if (bus_dmamem_alloc(sc->sense_buf_q_tag, (void **)&sc->sense_buf_q,
	    BUS_DMA_NOWAIT, &sc->sense_buf_q_dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }
        
	bzero(sc->sense_buf_q, sz);
        bus_dmamap_load(sc->sense_buf_q_tag, sc->sense_buf_q_dmamap, sc->sense_buf_q, sz,
	    mpi3mr_memaddr_cb, &sc->sense_buf_q_phys, BUS_DMA_NOWAIT);

	mpi3mr_dprint(sc, MPI3MR_XINFO, "sense_buf_q (0x%p): depth(%d), frame_size(%d), "
	    "pool_size(%d kB), sense_dma(0x%llx)\n",
	    sc->sense_buf_q, sc->sense_buf_q_sz, 8, (sz / 1024),
	    (unsigned long long)sc->sense_buf_q_phys);

	return 0;
}

static int mpi3mr_reply_alloc(struct mpi3mr_softc *sc)
{
	int retval = 0;
	U32 i;

	if (sc->init_cmds.reply)
		goto post_reply_sbuf;

	sc->init_cmds.reply = malloc(sc->reply_sz,
		M_MPI3MR, M_NOWAIT | M_ZERO);
	
	if (!sc->init_cmds.reply) {
		printf(IOCNAME "Cannot allocate memory for init_cmds.reply\n",
		    sc->name);
		goto out_failed;
	}

	sc->ioctl_cmds.reply = malloc(sc->reply_sz, M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->ioctl_cmds.reply) {
		printf(IOCNAME "Cannot allocate memory for ioctl_cmds.reply\n",
		    sc->name);
		goto out_failed;
	}

	sc->host_tm_cmds.reply = malloc(sc->reply_sz, M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->host_tm_cmds.reply) {
		printf(IOCNAME "Cannot allocate memory for host_tm.reply\n",
		    sc->name);
		goto out_failed;
	}
	for (i=0; i<MPI3MR_NUM_DEVRMCMD; i++) {
		sc->dev_rmhs_cmds[i].reply = malloc(sc->reply_sz,
		    M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->dev_rmhs_cmds[i].reply) {
			printf(IOCNAME "Cannot allocate memory for"
			    " dev_rmhs_cmd[%d].reply\n",
			    sc->name, i);
			goto out_failed;
		}
	}

	for (i = 0; i < MPI3MR_NUM_EVTACKCMD; i++) {
		sc->evtack_cmds[i].reply = malloc(sc->reply_sz,
			M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->evtack_cmds[i].reply)
			goto out_failed;
	}

	sc->dev_handle_bitmap_sz = MPI3MR_DIV_ROUND_UP(sc->facts.max_devhandle, 8);
	
	sc->removepend_bitmap = malloc(sc->dev_handle_bitmap_sz,
	    M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->removepend_bitmap) {
		printf(IOCNAME "Cannot alloc memory for remove pend bitmap\n",
		    sc->name);
		goto out_failed;
	}

	sc->devrem_bitmap_sz = MPI3MR_DIV_ROUND_UP(MPI3MR_NUM_DEVRMCMD, 8);
	sc->devrem_bitmap = malloc(sc->devrem_bitmap_sz,
	    M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->devrem_bitmap) {
		printf(IOCNAME "Cannot alloc memory for dev remove bitmap\n",
		    sc->name);
		goto out_failed;
	}
	
	sc->evtack_cmds_bitmap_sz = MPI3MR_DIV_ROUND_UP(MPI3MR_NUM_EVTACKCMD, 8);

	sc->evtack_cmds_bitmap = malloc(sc->evtack_cmds_bitmap_sz,
		M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->evtack_cmds_bitmap)
		goto out_failed;

	if (mpi3mr_reply_dma_alloc(sc)) {
		printf(IOCNAME "func:%s line:%d DMA memory allocation failed\n",
		    sc->name, __func__, __LINE__);
		goto out_failed;
	}

post_reply_sbuf:
	mpi3mr_setup_reply_free_queues(sc);
	return retval;
out_failed:
	mpi3mr_cleanup_interrupts(sc);
	mpi3mr_free_mem(sc);
	retval = -1;
	return retval;
}

static void
mpi3mr_print_fw_pkg_ver(struct mpi3mr_softc *sc)
{
	int retval = 0;
	void *fw_pkg_ver = NULL;
	bus_dma_tag_t fw_pkg_ver_tag;
	bus_dmamap_t fw_pkg_ver_map;
	bus_addr_t fw_pkg_ver_dma;
	Mpi3CIUploadRequest_t ci_upload;
	Mpi3ComponentImageHeader_t *ci_header;
	U32 fw_pkg_ver_len = sizeof(*ci_header);
	U8 sgl_flags = MPI3MR_SGEFLAGS_SYSTEM_SIMPLE_END_OF_LIST;

	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,  /* parent */
				4, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				fw_pkg_ver_len,		/* maxsize */
				1,			/* nsegments */
				fw_pkg_ver_len,		/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&fw_pkg_ver_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate fw package version request DMA tag\n");
		return;
	}

	if (bus_dmamem_alloc(fw_pkg_ver_tag, (void **)&fw_pkg_ver, BUS_DMA_NOWAIT, &fw_pkg_ver_map)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d fw package version DMA mem alloc failed\n",
			      __func__, __LINE__);
		return;
	}

	bzero(fw_pkg_ver, fw_pkg_ver_len);

	bus_dmamap_load(fw_pkg_ver_tag, fw_pkg_ver_map, fw_pkg_ver, fw_pkg_ver_len,
	    mpi3mr_memaddr_cb, &fw_pkg_ver_dma, BUS_DMA_NOWAIT);

	mpi3mr_dprint(sc, MPI3MR_XINFO, "Func: %s line: %d fw package version phys addr= %#016jx size= %d\n",
		      __func__, __LINE__, (uintmax_t)fw_pkg_ver_dma, fw_pkg_ver_len);

	if (!fw_pkg_ver) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Memory alloc for fw package version failed\n");
		goto out;
	}

	memset(&ci_upload, 0, sizeof(ci_upload));
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		mpi3mr_dprint(sc, MPI3MR_INFO,"Issue CI Header Upload: command is in use\n");
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	ci_upload.HostTag = htole16(MPI3MR_HOSTTAG_INITCMDS);
	ci_upload.Function = MPI3_FUNCTION_CI_UPLOAD;
	ci_upload.MsgFlags = MPI3_CI_UPLOAD_MSGFLAGS_LOCATION_PRIMARY;
	ci_upload.ImageOffset = MPI3_IMAGE_HEADER_SIGNATURE0_OFFSET;
	ci_upload.SegmentSize = MPI3_IMAGE_HEADER_SIZE;

	mpi3mr_add_sg_single(&ci_upload.SGL, sgl_flags, fw_pkg_ver_len,
	    fw_pkg_ver_dma);

	init_completion(&sc->init_cmds.completion);
	if ((retval = mpi3mr_submit_admin_cmd(sc, &ci_upload, sizeof(ci_upload)))) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Issue CI Header Upload: Admin Post failed\n");
		goto out_unlock;
	}
	wait_for_completion_timeout(&sc->init_cmds.completion,
		(MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Issue CI Header Upload: command timed out\n");
		sc->init_cmds.is_waiting = 0;
		if (!(sc->init_cmds.state & MPI3MR_CMD_RESET))
			mpi3mr_check_rh_fault_ioc(sc,
				MPI3MR_RESET_FROM_GETPKGVER_TIMEOUT);
		goto out_unlock;
	}
	if ((GET_IOC_STATUS(sc->init_cmds.ioc_status)) != MPI3_IOCSTATUS_SUCCESS) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
			      "Issue CI Header Upload: Failed IOCStatus(0x%04x) Loginfo(0x%08x)\n",
			      GET_IOC_STATUS(sc->init_cmds.ioc_status), sc->init_cmds.ioc_loginfo);
		goto out_unlock;
	}

	ci_header = (Mpi3ComponentImageHeader_t *) fw_pkg_ver;
	mpi3mr_dprint(sc, MPI3MR_XINFO,
		      "Issue CI Header Upload:EnvVariableOffset(0x%x) \
		      HeaderSize(0x%x) Signature1(0x%x)\n",
		      ci_header->EnvironmentVariableOffset,
		      ci_header->HeaderSize,
		      ci_header->Signature1);
	mpi3mr_dprint(sc, MPI3MR_INFO, "FW Package Version: %02d.%02d.%02d.%02d\n",
		      ci_header->ComponentImageVersion.GenMajor,
		      ci_header->ComponentImageVersion.GenMinor,
		      ci_header->ComponentImageVersion.PhaseMajor,
		      ci_header->ComponentImageVersion.PhaseMinor);
out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	if (fw_pkg_ver_dma != 0)
		bus_dmamap_unload(fw_pkg_ver_tag, fw_pkg_ver_map);
	if (fw_pkg_ver)
		bus_dmamem_free(fw_pkg_ver_tag, fw_pkg_ver, fw_pkg_ver_map);
	if (fw_pkg_ver_tag)
		bus_dma_tag_destroy(fw_pkg_ver_tag);

}

/**
 * mpi3mr_issue_iocinit - Send IOC Init
 * @sc: Adapter instance reference
 *
 * Issue IOC Init MPI request through admin queue and wait for
 * the completion of it or time out.
 *
 * Return: 0 on success, non-zero on failures.
 */
static int mpi3mr_issue_iocinit(struct mpi3mr_softc *sc)
{
	Mpi3IOCInitRequest_t iocinit_req;
	Mpi3DriverInfoLayout_t *drvr_info = NULL;
	bus_dma_tag_t drvr_info_tag;
	bus_dmamap_t drvr_info_map;
	bus_addr_t drvr_info_phys;
	U32 drvr_info_len = sizeof(*drvr_info);
	int retval = 0;
	struct timeval now;
	uint64_t time_in_msec;

	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,  /* parent */
				4, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                drvr_info_len,		/* maxsize */
                                1,			/* nsegments */
                                drvr_info_len,		/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &drvr_info_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
		return (ENOMEM);
        }
        
	if (bus_dmamem_alloc(drvr_info_tag, (void **)&drvr_info,
	    BUS_DMA_NOWAIT, &drvr_info_map)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d Data  DMA mem alloc failed\n",
			__func__, __LINE__);
		return (ENOMEM);
        }
        
	bzero(drvr_info, drvr_info_len);
        bus_dmamap_load(drvr_info_tag, drvr_info_map, drvr_info, drvr_info_len,
	    mpi3mr_memaddr_cb, &drvr_info_phys, BUS_DMA_NOWAIT);
	mpi3mr_dprint(sc, MPI3MR_XINFO, "Func: %s line: %d IOCfacts drvr_info phys addr= %#016jx size= %d\n",
	    __func__, __LINE__, (uintmax_t)drvr_info_phys, drvr_info_len);
	
	if (!drvr_info)
	{
		retval = -1;
		printf(IOCNAME "Memory alloc for Driver Info failed\n",
		    sc->name);
		goto out;
	}
	drvr_info->InformationLength = (drvr_info_len);
	strcpy(drvr_info->DriverSignature, "Broadcom");
	strcpy(drvr_info->OsName, "FreeBSD");
	strcpy(drvr_info->OsVersion, fmt_os_ver);
	strcpy(drvr_info->DriverName, MPI3MR_DRIVER_NAME);
	strcpy(drvr_info->DriverVersion, MPI3MR_DRIVER_VERSION);
	strcpy(drvr_info->DriverReleaseDate, MPI3MR_DRIVER_RELDATE);
	drvr_info->DriverCapabilities = 0;
	memcpy((U8 *)&sc->driver_info, (U8 *)drvr_info, sizeof(sc->driver_info));

	memset(&iocinit_req, 0, sizeof(iocinit_req));
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue IOCInit: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
        iocinit_req.HostTag = MPI3MR_HOSTTAG_INITCMDS;
        iocinit_req.Function = MPI3_FUNCTION_IOC_INIT;
        iocinit_req.MPIVersion.Struct.Dev = MPI3_VERSION_DEV;
        iocinit_req.MPIVersion.Struct.Unit = MPI3_VERSION_UNIT;
        iocinit_req.MPIVersion.Struct.Major = MPI3_VERSION_MAJOR;
        iocinit_req.MPIVersion.Struct.Minor = MPI3_VERSION_MINOR;
        iocinit_req.WhoInit = MPI3_WHOINIT_HOST_DRIVER;
        iocinit_req.ReplyFreeQueueDepth = sc->reply_free_q_sz;
        iocinit_req.ReplyFreeQueueAddress =
                sc->reply_free_q_phys;
        iocinit_req.SenseBufferLength = MPI3MR_SENSEBUF_SZ;
        iocinit_req.SenseBufferFreeQueueDepth =
                sc->sense_buf_q_sz;
        iocinit_req.SenseBufferFreeQueueAddress =
                sc->sense_buf_q_phys;
        iocinit_req.DriverInformationAddress = drvr_info_phys;

	getmicrotime(&now);
	time_in_msec = (now.tv_sec * 1000 + now.tv_usec/1000);
	iocinit_req.TimeStamp = htole64(time_in_msec);

	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &iocinit_req,
	    sizeof(iocinit_req));
	
	if (retval) {
		printf(IOCNAME "Issue IOCInit: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}
	
	wait_for_completion_timeout(&sc->init_cmds.completion,
	    (MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "Issue IOCInit: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
		    MPI3MR_RESET_FROM_IOCINIT_TIMEOUT);
		sc->unrecoverable = 1;
		retval = -1;
		goto out_unlock;
	}
	
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "Issue IOCInit: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}

out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	if (drvr_info_phys != 0)
		bus_dmamap_unload(drvr_info_tag, drvr_info_map);
	if (drvr_info != NULL)
		bus_dmamem_free(drvr_info_tag, drvr_info, drvr_info_map);
	if (drvr_info_tag != NULL)
		bus_dma_tag_destroy(drvr_info_tag);
	return retval;
}

static void
mpi3mr_display_ioc_info(struct mpi3mr_softc *sc)
{
        int i = 0;
        char personality[16];

        switch (sc->facts.personality) {
        case MPI3_IOCFACTS_FLAGS_PERSONALITY_EHBA:
                strcpy(personality, "Enhanced HBA");
                break;
        case MPI3_IOCFACTS_FLAGS_PERSONALITY_RAID_DDR:
                strcpy(personality, "RAID");
                break;
        default:
                strcpy(personality, "Unknown");
                break;
        }

	mpi3mr_dprint(sc, MPI3MR_INFO, "Current Personality: %s\n", personality);

	mpi3mr_dprint(sc, MPI3MR_INFO, "%s\n", sc->fw_version);

        mpi3mr_dprint(sc, MPI3MR_INFO, "Protocol=(");

        if (sc->facts.protocol_flags &
            MPI3_IOCFACTS_PROTOCOL_SCSI_INITIATOR) {
                printf("Initiator");
                i++;
        }

        if (sc->facts.protocol_flags &
            MPI3_IOCFACTS_PROTOCOL_SCSI_TARGET) {
                printf("%sTarget", i ? "," : "");
                i++;
        }

        if (sc->facts.protocol_flags &
            MPI3_IOCFACTS_PROTOCOL_NVME) {
                printf("%sNVMe attachment", i ? "," : "");
                i++;
        }
        i = 0;
        printf("), ");
        printf("Capabilities=(");

        if (sc->facts.ioc_capabilities &
            MPI3_IOCFACTS_CAPABILITY_RAID_CAPABLE) {
                printf("RAID");
                i++;
        }

        printf(")\n");
}

/**
 * mpi3mr_unmask_events - Unmask events in event mask bitmap
 * @sc: Adapter instance reference
 * @event: MPI event ID
 *
 * Un mask the specific event by resetting the event_mask
 * bitmap.
 *
 * Return: None. 
 */
static void mpi3mr_unmask_events(struct mpi3mr_softc *sc, U16 event)
{
	U32 desired_event;

	if (event >= 128)
		return;

	desired_event = (1 << (event % 32));

	if (event < 32)
		sc->event_masks[0] &= ~desired_event;
	else if (event < 64)
		sc->event_masks[1] &= ~desired_event;
	else if (event < 96)
		sc->event_masks[2] &= ~desired_event;
	else if (event < 128)
		sc->event_masks[3] &= ~desired_event;
}

static void mpi3mr_set_events_mask(struct mpi3mr_softc *sc)
{
	int i;
	for (i = 0; i < MPI3_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
		sc->event_masks[i] = -1;

        mpi3mr_unmask_events(sc, MPI3_EVENT_DEVICE_ADDED);
        mpi3mr_unmask_events(sc, MPI3_EVENT_DEVICE_INFO_CHANGED);
        mpi3mr_unmask_events(sc, MPI3_EVENT_DEVICE_STATUS_CHANGE);

        mpi3mr_unmask_events(sc, MPI3_EVENT_ENCL_DEVICE_STATUS_CHANGE);

        mpi3mr_unmask_events(sc, MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST);
        mpi3mr_unmask_events(sc, MPI3_EVENT_SAS_DISCOVERY);
        mpi3mr_unmask_events(sc, MPI3_EVENT_SAS_DEVICE_DISCOVERY_ERROR);
        mpi3mr_unmask_events(sc, MPI3_EVENT_SAS_BROADCAST_PRIMITIVE);

        mpi3mr_unmask_events(sc, MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST);
        mpi3mr_unmask_events(sc, MPI3_EVENT_PCIE_ENUMERATION);

        mpi3mr_unmask_events(sc, MPI3_EVENT_PREPARE_FOR_RESET);
        mpi3mr_unmask_events(sc, MPI3_EVENT_CABLE_MGMT);
        mpi3mr_unmask_events(sc, MPI3_EVENT_ENERGY_PACK_CHANGE);
}

/**
 * mpi3mr_issue_event_notification - Send event notification
 * @sc: Adapter instance reference
 *
 * Issue event notification MPI request through admin queue and
 * wait for the completion of it or time out.
 *
 * Return: 0 on success, non-zero on failures.
 */
int mpi3mr_issue_event_notification(struct mpi3mr_softc *sc)
{
	Mpi3EventNotificationRequest_t evtnotify_req;
	int retval = 0;
	U8 i;

	memset(&evtnotify_req, 0, sizeof(evtnotify_req));
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue EvtNotify: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	evtnotify_req.HostTag = (MPI3MR_HOSTTAG_INITCMDS);
	evtnotify_req.Function = MPI3_FUNCTION_EVENT_NOTIFICATION;
	for (i = 0; i < MPI3_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
		evtnotify_req.EventMasks[i] =
		    (sc->event_masks[i]);
	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &evtnotify_req,
	    sizeof(evtnotify_req));
	if (retval) {
		printf(IOCNAME "Issue EvtNotify: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}
	
	poll_for_command_completion(sc,
				    &sc->init_cmds,
				    (MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "Issue EvtNotify: command timed out\n",
		    sc->name);
		mpi3mr_check_rh_fault_ioc(sc,
		    MPI3MR_RESET_FROM_EVTNOTIFY_TIMEOUT);
		retval = -1;
		goto out_unlock;
	}
	
	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "Issue EvtNotify: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}

out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	return retval;
}

int
mpi3mr_register_events(struct mpi3mr_softc *sc)
{
	int error;

	mpi3mr_set_events_mask(sc);

	error = mpi3mr_issue_event_notification(sc);

	if (error) {
		printf(IOCNAME "Failed to issue event notification %d\n",
		    sc->name, error);
	}

	return error;
}

/**
 * mpi3mr_process_event_ack - Process event acknowledgment
 * @sc: Adapter instance reference
 * @event: MPI3 event ID
 * @event_ctx: Event context
 *
 * Send event acknowledgement through admin queue and wait for
 * it to complete.
 *
 * Return: 0 on success, non-zero on failures.
 */
int mpi3mr_process_event_ack(struct mpi3mr_softc *sc, U8 event,
	U32 event_ctx)
{
	Mpi3EventAckRequest_t evtack_req;
	int retval = 0;

	memset(&evtack_req, 0, sizeof(evtack_req));
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue EvtAck: Init command is in use\n",
		    sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}
	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	sc->init_cmds.is_waiting = 1;
	sc->init_cmds.callback = NULL;
	evtack_req.HostTag = htole16(MPI3MR_HOSTTAG_INITCMDS);
	evtack_req.Function = MPI3_FUNCTION_EVENT_ACK;
	evtack_req.Event = event;
	evtack_req.EventContext = htole32(event_ctx);

	init_completion(&sc->init_cmds.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &evtack_req,
	    sizeof(evtack_req));
	if (retval) {
		printf(IOCNAME "Issue EvtAck: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}

	wait_for_completion_timeout(&sc->init_cmds.completion,
	    (MPI3MR_INTADMCMD_TIMEOUT));
	if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "Issue EvtAck: command timed out\n",
		    sc->name);
		retval = -1;
		goto out_unlock;
	}

	if ((sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	     != MPI3_IOCSTATUS_SUCCESS ) {
		printf(IOCNAME "Issue EvtAck: Failed IOCStatus(0x%04x) "
		    " Loginfo(0x%08x) \n" , sc->name,
		    (sc->init_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    sc->init_cmds.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}

out_unlock:
	sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	return retval;
}


static int mpi3mr_alloc_chain_bufs(struct mpi3mr_softc *sc)
{
	int retval = 0;
	U32 sz, i;
	U16 num_chains;

	num_chains = sc->max_host_ios;

	sc->chain_buf_count = num_chains;
	sz = sizeof(struct mpi3mr_chain) * num_chains;
	
	sc->chain_sgl_list = malloc(sz, M_MPI3MR, M_NOWAIT | M_ZERO);
	
	if (!sc->chain_sgl_list) {
		printf(IOCNAME "Cannot allocate memory for chain SGL list\n",
		    sc->name);
		retval = -1;
		goto out_failed;
	}

	sz = MPI3MR_CHAINSGE_SIZE;

        if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,  /* parent */
				4096, 0,		/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                sz,			/* maxsize */
                                1,			/* nsegments */
                                sz,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->chain_sgl_list_tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate Chain buffer DMA tag\n");
		return (ENOMEM);
        }
        
	for (i = 0; i < num_chains; i++) {
		if (bus_dmamem_alloc(sc->chain_sgl_list_tag, (void **)&sc->chain_sgl_list[i].buf,
		    BUS_DMA_NOWAIT, &sc->chain_sgl_list[i].buf_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Func: %s line: %d  DMA mem alloc failed\n",
				__func__, __LINE__);
			return (ENOMEM);
		}
		
		bzero(sc->chain_sgl_list[i].buf, sz);
		bus_dmamap_load(sc->chain_sgl_list_tag, sc->chain_sgl_list[i].buf_dmamap, sc->chain_sgl_list[i].buf, sz,
		    mpi3mr_memaddr_cb, &sc->chain_sgl_list[i].buf_phys, BUS_DMA_NOWAIT);
		mpi3mr_dprint(sc, MPI3MR_XINFO, "Func: %s line: %d phys addr= %#016jx size= %d\n",
		    __func__, __LINE__, (uintmax_t)sc->chain_sgl_list[i].buf_phys, sz);
	}
	
	sc->chain_bitmap_sz = MPI3MR_DIV_ROUND_UP(num_chains, 8);

	sc->chain_bitmap = malloc(sc->chain_bitmap_sz, M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->chain_bitmap) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "Cannot alloc memory for chain bitmap\n");
		retval = -1;
		goto out_failed;
	}
	return retval;

out_failed:
	for (i = 0; i < num_chains; i++) {
		if (sc->chain_sgl_list[i].buf_phys != 0)
			bus_dmamap_unload(sc->chain_sgl_list_tag, sc->chain_sgl_list[i].buf_dmamap);
		if (sc->chain_sgl_list[i].buf != NULL)
			bus_dmamem_free(sc->chain_sgl_list_tag, sc->chain_sgl_list[i].buf, sc->chain_sgl_list[i].buf_dmamap);
	}
	if (sc->chain_sgl_list_tag != NULL)
		bus_dma_tag_destroy(sc->chain_sgl_list_tag);
	return retval;
}

static int mpi3mr_pel_alloc(struct mpi3mr_softc *sc)
{
	int retval = 0;
	
	if (!sc->pel_cmds.reply) {
		sc->pel_cmds.reply = malloc(sc->reply_sz, M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->pel_cmds.reply) {
			printf(IOCNAME "Cannot allocate memory for pel_cmds.reply\n",
			    sc->name);
			goto out_failed;
		}
	}
	
	if (!sc->pel_abort_cmd.reply) {
		sc->pel_abort_cmd.reply = malloc(sc->reply_sz, M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->pel_abort_cmd.reply) {
			printf(IOCNAME "Cannot allocate memory for pel_abort_cmd.reply\n",
			    sc->name);
			goto out_failed;
		}
	}
	
	if (!sc->pel_seq_number) {
		sc->pel_seq_number_sz = sizeof(Mpi3PELSeq_t);
		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,   /* parent */
				 4, 0,                           /* alignment, boundary */
				 sc->dma_loaddr,	        /* lowaddr */
				 sc->dma_hiaddr,	              /* highaddr */
				 NULL, NULL,                     /* filter, filterarg */
				 sc->pel_seq_number_sz,		 /* maxsize */
				 1,                              /* nsegments */
				 sc->pel_seq_number_sz,          /* maxsegsize */
				 0,                              /* flags */
				 NULL, NULL,                     /* lockfunc, lockarg */
				 &sc->pel_seq_num_dmatag)) {
			 mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot create PEL seq number dma memory tag\n");
			 retval = -ENOMEM;
			 goto out_failed;
		}

		if (bus_dmamem_alloc(sc->pel_seq_num_dmatag, (void **)&sc->pel_seq_number,
		    BUS_DMA_NOWAIT, &sc->pel_seq_num_dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate PEL seq number kernel buffer dma memory\n");
			retval = -ENOMEM;
			goto out_failed;
		}

		bzero(sc->pel_seq_number, sc->pel_seq_number_sz);
		
		bus_dmamap_load(sc->pel_seq_num_dmatag, sc->pel_seq_num_dmamap, sc->pel_seq_number,
		    sc->pel_seq_number_sz, mpi3mr_memaddr_cb, &sc->pel_seq_number_dma, BUS_DMA_NOWAIT);
		
		if (!sc->pel_seq_number) {
			printf(IOCNAME "%s:%d Cannot load PEL seq number dma memory for size: %d\n", sc->name,
				__func__, __LINE__, sc->pel_seq_number_sz);
			retval = -ENOMEM;
			goto out_failed;
		}
	}

out_failed:
	return retval;
}

/**
 * mpi3mr_validate_fw_update - validate IOCFacts post adapter reset
 * @sc: Adapter instance reference
 *
 * Return zero if the new IOCFacts is compatible with previous values
 * else return appropriate error
 */
static int
mpi3mr_validate_fw_update(struct mpi3mr_softc *sc)
{
	U16 dev_handle_bitmap_sz;
	U8 *removepend_bitmap;

	if (sc->facts.reply_sz > sc->reply_sz) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Cannot increase reply size from %d to %d\n",
		    sc->reply_sz, sc->reply_sz);
		return -EPERM;
	}

	if (sc->num_io_throttle_group != sc->facts.max_io_throttle_group) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "max io throttle group doesn't match old(%d), new(%d)\n",
		    sc->num_io_throttle_group,
		    sc->facts.max_io_throttle_group);
		return -EPERM;
	}

	if (sc->facts.max_op_reply_q < sc->num_queues) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Cannot reduce number of operational reply queues from %d to %d\n",
		    sc->num_queues,
		    sc->facts.max_op_reply_q);
		return -EPERM;
	}

	if (sc->facts.max_op_req_q < sc->num_queues) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Cannot reduce number of operational request queues from %d to %d\n",
		    sc->num_queues, sc->facts.max_op_req_q);
		return -EPERM;
	}

	dev_handle_bitmap_sz = MPI3MR_DIV_ROUND_UP(sc->facts.max_devhandle, 8);

	if (dev_handle_bitmap_sz > sc->dev_handle_bitmap_sz) {
		removepend_bitmap = realloc(sc->removepend_bitmap,
		    dev_handle_bitmap_sz, M_MPI3MR, M_NOWAIT);

		if (!removepend_bitmap) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "failed to increase removepend_bitmap sz from: %d to %d\n",
			    sc->dev_handle_bitmap_sz, dev_handle_bitmap_sz);
			return -ENOMEM;
		}

		memset(removepend_bitmap + sc->dev_handle_bitmap_sz, 0,
		    dev_handle_bitmap_sz - sc->dev_handle_bitmap_sz);
		sc->removepend_bitmap = removepend_bitmap;
		mpi3mr_dprint(sc, MPI3MR_INFO,
		    "increased dev_handle_bitmap_sz from %d to %d\n",
		    sc->dev_handle_bitmap_sz, dev_handle_bitmap_sz);
		sc->dev_handle_bitmap_sz = dev_handle_bitmap_sz;
	}

	return 0;
}

/*
 * mpi3mr_initialize_ioc - Controller initialization
 * @dev: pointer to device struct
 *
 * This function allocates the controller wide resources and brings
 * the controller to operational state
 *
 * Return: 0 on success and proper error codes on failure
 */
int mpi3mr_initialize_ioc(struct mpi3mr_softc *sc, U8 init_type)
{
	int retval = 0;
	enum mpi3mr_iocstate ioc_state;
	U64 ioc_info;
	U32 ioc_status, ioc_control, i, timeout;
	Mpi3IOCFactsData_t facts_data;
	char str[32];
	U32 size;

	sc->cpu_count = mp_ncpus;

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	ioc_control = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
	ioc_info = mpi3mr_regread64(sc, MPI3_SYSIF_IOC_INFO_LOW_OFFSET);

	mpi3mr_dprint(sc, MPI3MR_INFO, "SOD ioc_status: 0x%x ioc_control: 0x%x "
	    "ioc_info: 0x%lx\n", ioc_status, ioc_control, ioc_info);

        /*The timeout value is in 2sec unit, changing it to seconds*/
	sc->ready_timeout =
                ((ioc_info & MPI3_SYSIF_IOC_INFO_LOW_TIMEOUT_MASK) >>
                    MPI3_SYSIF_IOC_INFO_LOW_TIMEOUT_SHIFT) * 2;

	ioc_state = mpi3mr_get_iocstate(sc);
	
	mpi3mr_dprint(sc, MPI3MR_INFO, "IOC state: %s   IOC ready timeout: %d\n",
	    mpi3mr_iocstate_name(ioc_state), sc->ready_timeout);

	if (ioc_state == MRIOC_STATE_BECOMING_READY ||
	    ioc_state == MRIOC_STATE_RESET_REQUESTED) {
		timeout = sc->ready_timeout * 10;
		do {
			DELAY(1000 * 100);
		} while (--timeout);

		ioc_state = mpi3mr_get_iocstate(sc);
		mpi3mr_dprint(sc, MPI3MR_INFO,
			"IOC in %s state after waiting for reset time\n",
			mpi3mr_iocstate_name(ioc_state));
	}

	if (ioc_state == MRIOC_STATE_READY) {
                retval = mpi3mr_mur_ioc(sc, MPI3MR_RESET_FROM_BRINGUP);
                if (retval) {
                        mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to MU reset IOC, error 0x%x\n",
                                retval);
                }
                ioc_state = mpi3mr_get_iocstate(sc);
        }

        if (ioc_state != MRIOC_STATE_RESET) {
                mpi3mr_print_fault_info(sc);
		 mpi3mr_dprint(sc, MPI3MR_ERROR, "issuing soft reset to bring to reset state\n");
                 retval = mpi3mr_issue_reset(sc,
                     MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET,
                     MPI3MR_RESET_FROM_BRINGUP);
                if (retval) {
                        mpi3mr_dprint(sc, MPI3MR_ERROR,
                            "%s :Failed to soft reset IOC, error 0x%d\n",
                            __func__, retval);
                        goto out_failed;
                }
        }
        
	ioc_state = mpi3mr_get_iocstate(sc);

        if (ioc_state != MRIOC_STATE_RESET) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot bring IOC to reset state\n");
		goto out_failed;
        }

	retval = mpi3mr_setup_admin_qpair(sc);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to setup Admin queues, error 0x%x\n",
		    retval);
		goto out_failed;
	}
	
	retval = mpi3mr_bring_ioc_ready(sc);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to bring IOC ready, error 0x%x\n",
		    retval);
		goto out_failed;
	}

	if (init_type == MPI3MR_INIT_TYPE_INIT) {
		retval = mpi3mr_alloc_interrupts(sc, 1);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocate interrupts, error 0x%x\n",
			    retval);
			goto out_failed;
		}
	
		retval = mpi3mr_setup_irqs(sc);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to setup ISR, error 0x%x\n",
			    retval);
			goto out_failed;
		}
	}

	mpi3mr_enable_interrupts(sc);

	if (init_type == MPI3MR_INIT_TYPE_INIT) {
		mtx_init(&sc->mpi3mr_mtx, "SIM lock", NULL, MTX_DEF);
		mtx_init(&sc->io_lock, "IO lock", NULL, MTX_DEF);
		mtx_init(&sc->admin_req_lock, "Admin Request Queue lock", NULL, MTX_SPIN);
		mtx_init(&sc->reply_free_q_lock, "Reply free Queue lock", NULL, MTX_SPIN);
		mtx_init(&sc->sense_buf_q_lock, "Sense buffer Queue lock", NULL, MTX_SPIN);
		mtx_init(&sc->chain_buf_lock, "Chain buffer lock", NULL, MTX_SPIN);
		mtx_init(&sc->cmd_pool_lock, "Command pool lock", NULL, MTX_DEF);
		mtx_init(&sc->fwevt_lock, "Firmware Event lock", NULL, MTX_DEF);
		mtx_init(&sc->target_lock, "Target lock", NULL, MTX_SPIN);
		mtx_init(&sc->reset_mutex, "Reset lock", NULL, MTX_DEF);

		mtx_init(&sc->init_cmds.completion.lock, "Init commands lock", NULL, MTX_DEF);
		sc->init_cmds.reply = NULL;
		sc->init_cmds.state = MPI3MR_CMD_NOTUSED;
		sc->init_cmds.dev_handle = MPI3MR_INVALID_DEV_HANDLE;
		sc->init_cmds.host_tag = MPI3MR_HOSTTAG_INITCMDS;

		mtx_init(&sc->ioctl_cmds.completion.lock, "IOCTL commands lock", NULL, MTX_DEF);
		sc->ioctl_cmds.reply = NULL;
		sc->ioctl_cmds.state = MPI3MR_CMD_NOTUSED;
		sc->ioctl_cmds.dev_handle = MPI3MR_INVALID_DEV_HANDLE;
		sc->ioctl_cmds.host_tag = MPI3MR_HOSTTAG_IOCTLCMDS;
		
		mtx_init(&sc->pel_abort_cmd.completion.lock, "PEL Abort command lock", NULL, MTX_DEF);
		sc->pel_abort_cmd.reply = NULL;
		sc->pel_abort_cmd.state = MPI3MR_CMD_NOTUSED;
		sc->pel_abort_cmd.dev_handle = MPI3MR_INVALID_DEV_HANDLE;
		sc->pel_abort_cmd.host_tag = MPI3MR_HOSTTAG_PELABORT;

		mtx_init(&sc->host_tm_cmds.completion.lock, "TM commands lock", NULL, MTX_DEF);
		sc->host_tm_cmds.reply = NULL;
		sc->host_tm_cmds.state = MPI3MR_CMD_NOTUSED;
		sc->host_tm_cmds.dev_handle = MPI3MR_INVALID_DEV_HANDLE;
		sc->host_tm_cmds.host_tag = MPI3MR_HOSTTAG_TMS;

		TAILQ_INIT(&sc->cmd_list_head);
		TAILQ_INIT(&sc->event_list);
		TAILQ_INIT(&sc->delayed_rmhs_list);
		TAILQ_INIT(&sc->delayed_evtack_cmds_list);
		
		for (i = 0; i < MPI3MR_NUM_DEVRMCMD; i++) {
			snprintf(str, 32, "Dev REMHS commands lock[%d]", i);
			mtx_init(&sc->dev_rmhs_cmds[i].completion.lock, str, NULL, MTX_DEF);
			sc->dev_rmhs_cmds[i].reply = NULL;
			sc->dev_rmhs_cmds[i].state = MPI3MR_CMD_NOTUSED;
			sc->dev_rmhs_cmds[i].dev_handle = MPI3MR_INVALID_DEV_HANDLE;
			sc->dev_rmhs_cmds[i].host_tag = MPI3MR_HOSTTAG_DEVRMCMD_MIN
							    + i;
		}
	}

	retval = mpi3mr_issue_iocfacts(sc, &facts_data);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to Issue IOC Facts, retval: 0x%x\n",
		    retval);
		goto out_failed;
	}

	retval = mpi3mr_process_factsdata(sc, &facts_data);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "IOC Facts data processing failedi, retval: 0x%x\n",
		    retval);
		goto out_failed;
	}

	sc->num_io_throttle_group = sc->facts.max_io_throttle_group;
	mpi3mr_atomic_set(&sc->pend_large_data_sz, 0);
	
	if (init_type == MPI3MR_INIT_TYPE_RESET) {
		retval = mpi3mr_validate_fw_update(sc);
		if (retval)
			goto out_failed;
	} else {
		sc->reply_sz = sc->facts.reply_sz;
	}

	mpi3mr_display_ioc_info(sc);

	retval = mpi3mr_reply_alloc(sc);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocated reply and sense buffers, retval: 0x%x\n",
		    retval);
		goto out_failed;
	}
	
	if (init_type == MPI3MR_INIT_TYPE_INIT) {
		retval = mpi3mr_alloc_chain_bufs(sc);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocated chain buffers, retval: 0x%x\n",
			    retval);
			goto out_failed;
		}
	}
	
	retval = mpi3mr_issue_iocinit(sc);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to Issue IOC Init, retval: 0x%x\n",
		    retval);
		goto out_failed;
	}

	mpi3mr_print_fw_pkg_ver(sc);

	sc->reply_free_q_host_index = sc->num_reply_bufs;
	mpi3mr_regwrite(sc, MPI3_SYSIF_REPLY_FREE_HOST_INDEX_OFFSET,
		sc->reply_free_q_host_index);
	
	sc->sense_buf_q_host_index = sc->num_sense_bufs;
	
	mpi3mr_regwrite(sc, MPI3_SYSIF_SENSE_BUF_FREE_HOST_INDEX_OFFSET,
		sc->sense_buf_q_host_index);

	if (init_type == MPI3MR_INIT_TYPE_INIT) {
		retval = mpi3mr_alloc_interrupts(sc, 0);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocate interrupts, retval: 0x%x\n",
			    retval);
			goto out_failed;
		}

		retval = mpi3mr_setup_irqs(sc);
		if (retval) {
			printf(IOCNAME "Failed to setup ISR, error: 0x%x\n",
			    sc->name, retval);
			goto out_failed;
		}

		mpi3mr_enable_interrupts(sc);

	} else
		mpi3mr_enable_interrupts(sc);
	
	retval = mpi3mr_create_op_queues(sc);

	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to create operational queues, error: %d\n",
		    retval);
		goto out_failed;
	}

	if (!sc->throttle_groups && sc->num_io_throttle_group) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "allocating memory for throttle groups\n");
		size = sizeof(struct mpi3mr_throttle_group_info);
		sc->throttle_groups = (struct mpi3mr_throttle_group_info *)
					  malloc(sc->num_io_throttle_group *
					      size, M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->throttle_groups)
			goto out_failed;
	}

	if (init_type == MPI3MR_INIT_TYPE_RESET) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "Re-register events\n");
		retval = mpi3mr_register_events(sc);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_INFO, "Failed to re-register events, retval: 0x%x\n",
			    retval);
			goto out_failed;
		}

		mpi3mr_dprint(sc, MPI3MR_INFO, "Issuing Port Enable\n");
		retval = mpi3mr_issue_port_enable(sc, 0);
		if (retval) {
			mpi3mr_dprint(sc, MPI3MR_INFO, "Failed to issue port enable, retval: 0x%x\n",
			    retval);
			goto out_failed;
		}
	}
	retval = mpi3mr_pel_alloc(sc);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to allocate memory for PEL, retval: 0x%x\n",
		    retval);
		goto out_failed;
	}
	
	return retval;

out_failed:
	retval = -1;
	return retval;
}

static void mpi3mr_port_enable_complete(struct mpi3mr_softc *sc,
    struct mpi3mr_drvr_cmd *drvrcmd)
{
	drvrcmd->state = MPI3MR_CMD_NOTUSED;
	drvrcmd->callback = NULL;
	printf(IOCNAME "Completing Port Enable Request\n", sc->name);
	sc->mpi3mr_flags |= MPI3MR_FLAGS_PORT_ENABLE_DONE;
	mpi3mr_startup_decrement(sc->cam_sc);
}

int mpi3mr_issue_port_enable(struct mpi3mr_softc *sc, U8 async)
{
	Mpi3PortEnableRequest_t pe_req;
	int retval = 0;
	
	memset(&pe_req, 0, sizeof(pe_req));
	mtx_lock(&sc->init_cmds.completion.lock);
	if (sc->init_cmds.state & MPI3MR_CMD_PENDING) {
		retval = -1;
		printf(IOCNAME "Issue PortEnable: Init command is in use\n", sc->name);
		mtx_unlock(&sc->init_cmds.completion.lock);
		goto out;
	}

	sc->init_cmds.state = MPI3MR_CMD_PENDING;
	
	if (async) {
		sc->init_cmds.is_waiting = 0;
		sc->init_cmds.callback = mpi3mr_port_enable_complete;
	} else {
		sc->init_cmds.is_waiting = 1;
		sc->init_cmds.callback = NULL;
		init_completion(&sc->init_cmds.completion);
	}
	pe_req.HostTag = MPI3MR_HOSTTAG_INITCMDS;
	pe_req.Function = MPI3_FUNCTION_PORT_ENABLE;

	printf(IOCNAME "Sending Port Enable Request\n", sc->name);
	retval = mpi3mr_submit_admin_cmd(sc, &pe_req, sizeof(pe_req));
	if (retval) {
		printf(IOCNAME "Issue PortEnable: Admin Post failed\n",
		    sc->name);
		goto out_unlock;
	}

	if (!async) {
		wait_for_completion_timeout(&sc->init_cmds.completion,
		    MPI3MR_PORTENABLE_TIMEOUT);
		if (!(sc->init_cmds.state & MPI3MR_CMD_COMPLETE)) {
			printf(IOCNAME "Issue PortEnable: command timed out\n",
			    sc->name);
			retval = -1;
			mpi3mr_check_rh_fault_ioc(sc, MPI3MR_RESET_FROM_PE_TIMEOUT);
			goto out_unlock;
		}
		mpi3mr_port_enable_complete(sc, &sc->init_cmds);
	}
out_unlock:
	mtx_unlock(&sc->init_cmds.completion.lock);

out:
	return retval;
}

void
mpi3mr_watchdog_thread(void *arg)
{
	struct mpi3mr_softc *sc;
	enum mpi3mr_iocstate ioc_state;
	U32 fault, host_diagnostic, ioc_status;

	sc = (struct mpi3mr_softc *)arg;

	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s\n", __func__);

	sc->watchdog_thread_active = 1;
	mtx_lock(&sc->reset_mutex);
	for (;;) {
		if (sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN || 
		    (sc->unrecoverable == 1)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "Exit due to %s from %s\n",
			   sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN ? "Shutdown" :
			    "Hardware critical error", __func__);
			break;
		}
		mtx_unlock(&sc->reset_mutex);

		if ((sc->prepare_for_reset) &&
		    ((sc->prepare_for_reset_timeout_counter++) >=
		     MPI3MR_PREPARE_FOR_RESET_TIMEOUT)) {
			mpi3mr_soft_reset_handler(sc,
			    MPI3MR_RESET_FROM_CIACTVRST_TIMER, 1);
			goto sleep;
		}
	
		ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
		
		if (ioc_status & MPI3_SYSIF_IOC_STATUS_RESET_HISTORY) {
			mpi3mr_soft_reset_handler(sc, MPI3MR_RESET_FROM_FIRMWARE, 0);
			goto sleep;
		}

		ioc_state = mpi3mr_get_iocstate(sc);
		if (ioc_state == MRIOC_STATE_FAULT) {
			fault = mpi3mr_regread(sc, MPI3_SYSIF_FAULT_OFFSET) &
			    MPI3_SYSIF_FAULT_CODE_MASK;
			
			host_diagnostic = mpi3mr_regread(sc, MPI3_SYSIF_HOST_DIAG_OFFSET);
			if (host_diagnostic & MPI3_SYSIF_HOST_DIAG_SAVE_IN_PROGRESS) {
				if (!sc->diagsave_timeout) {
					mpi3mr_print_fault_info(sc);
					mpi3mr_dprint(sc, MPI3MR_INFO,
						"diag save in progress\n");
				}
				if ((sc->diagsave_timeout++) <= MPI3_SYSIF_DIAG_SAVE_TIMEOUT)
					goto sleep;
			}
			mpi3mr_print_fault_info(sc);
			sc->diagsave_timeout = 0;

			if ((fault == MPI3_SYSIF_FAULT_CODE_POWER_CYCLE_REQUIRED) || 
			    (fault == MPI3_SYSIF_FAULT_CODE_COMPLETE_RESET_NEEDED)) {
				mpi3mr_dprint(sc, MPI3MR_INFO,
				    "Controller requires system power cycle or complete reset is needed,"
				    "fault code: 0x%x. marking controller as unrecoverable\n", fault);
				sc->unrecoverable = 1;
				break;
			}
			if ((fault == MPI3_SYSIF_FAULT_CODE_DIAG_FAULT_RESET)
			    || (fault == MPI3_SYSIF_FAULT_CODE_SOFT_RESET_IN_PROGRESS)
			    || (sc->reset_in_progress))
				break;
			if (fault == MPI3_SYSIF_FAULT_CODE_CI_ACTIVATION_RESET)
				mpi3mr_soft_reset_handler(sc,
				    MPI3MR_RESET_FROM_CIACTIV_FAULT, 0);
			else
				mpi3mr_soft_reset_handler(sc,
				    MPI3MR_RESET_FROM_FAULT_WATCH, 0);

		}
		
		if (sc->reset.type == MPI3MR_TRIGGER_SOFT_RESET) {
			mpi3mr_print_fault_info(sc);
			mpi3mr_soft_reset_handler(sc, sc->reset.reason, 1);
		}
sleep:
		mtx_lock(&sc->reset_mutex);
		/*
		 * Sleep for 1 second if we're not exiting, then loop to top
		 * to poll exit status and hardware health.
		 */
		if ((sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN) == 0 &&
		    !sc->unrecoverable) {
			msleep(&sc->watchdog_chan, &sc->reset_mutex, PRIBIO,
			    "mpi3mr_watchdog", 1 * hz);
		}
	}
	mtx_unlock(&sc->reset_mutex);
	sc->watchdog_thread_active = 0;
	mpi3mr_kproc_exit(0);
}

static void mpi3mr_display_event_data(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_rep)
{
	char *desc = NULL;
	U16 event;

	event = event_rep->Event;

	switch (event) {
	case MPI3_EVENT_LOG_DATA:
		desc = "Log Data";
		break;
	case MPI3_EVENT_CHANGE:
		desc = "Event Change";
		break;
	case MPI3_EVENT_GPIO_INTERRUPT:
		desc = "GPIO Interrupt";
		break;
	case MPI3_EVENT_CABLE_MGMT:
		desc = "Cable Management";
		break;
	case MPI3_EVENT_ENERGY_PACK_CHANGE:
		desc = "Energy Pack Change";
		break;
	case MPI3_EVENT_DEVICE_ADDED:
	{
		Mpi3DevicePage0_t *event_data =
		    (Mpi3DevicePage0_t *)event_rep->EventData;
		mpi3mr_dprint(sc, MPI3MR_EVENT, "Device Added: Dev=0x%04x Form=0x%x Perst id: 0x%x\n",
			event_data->DevHandle, event_data->DeviceForm, event_data->PersistentID);
		return;
	}
	case MPI3_EVENT_DEVICE_INFO_CHANGED:
	{
		Mpi3DevicePage0_t *event_data =
		    (Mpi3DevicePage0_t *)event_rep->EventData;
		mpi3mr_dprint(sc, MPI3MR_EVENT, "Device Info Changed: Dev=0x%04x Form=0x%x\n",
			event_data->DevHandle, event_data->DeviceForm);
		return;
	}
	case MPI3_EVENT_DEVICE_STATUS_CHANGE:
	{
		Mpi3EventDataDeviceStatusChange_t *event_data =
		    (Mpi3EventDataDeviceStatusChange_t *)event_rep->EventData;
		mpi3mr_dprint(sc, MPI3MR_EVENT, "Device Status Change: Dev=0x%04x RC=0x%x\n",
			event_data->DevHandle, event_data->ReasonCode);
		return;
	}
	case MPI3_EVENT_SAS_DISCOVERY:
	{
		Mpi3EventDataSasDiscovery_t *event_data =
		    (Mpi3EventDataSasDiscovery_t *)event_rep->EventData;
		mpi3mr_dprint(sc, MPI3MR_EVENT, "SAS Discovery: (%s)",
			(event_data->ReasonCode == MPI3_EVENT_SAS_DISC_RC_STARTED) ?
		    "start" : "stop");
		if (event_data->DiscoveryStatus &&
		    (sc->mpi3mr_debug & MPI3MR_EVENT)) {
			printf("discovery_status(0x%08x)",
			    event_data->DiscoveryStatus);

		}

		if (sc->mpi3mr_debug & MPI3MR_EVENT)
			printf("\n");
		return;
	}
	case MPI3_EVENT_SAS_BROADCAST_PRIMITIVE:
		desc = "SAS Broadcast Primitive";
		break;
	case MPI3_EVENT_SAS_NOTIFY_PRIMITIVE:
		desc = "SAS Notify Primitive";
		break;
	case MPI3_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE:
		desc = "SAS Init Device Status Change";
		break;
	case MPI3_EVENT_SAS_INIT_TABLE_OVERFLOW:
		desc = "SAS Init Table Overflow";
		break;
	case MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		desc = "SAS Topology Change List";
		break;
	case MPI3_EVENT_ENCL_DEVICE_STATUS_CHANGE:
		desc = "Enclosure Device Status Change";
		break;
	case MPI3_EVENT_HARD_RESET_RECEIVED:
		desc = "Hard Reset Received";
		break;
	case MPI3_EVENT_SAS_PHY_COUNTER:
		desc = "SAS PHY Counter";
		break;
	case MPI3_EVENT_SAS_DEVICE_DISCOVERY_ERROR:
		desc = "SAS Device Discovery Error";
		break;
	case MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST:
		desc = "PCIE Topology Change List";
		break;
	case MPI3_EVENT_PCIE_ENUMERATION:
	{
		Mpi3EventDataPcieEnumeration_t *event_data =
			(Mpi3EventDataPcieEnumeration_t *)event_rep->EventData;
		mpi3mr_dprint(sc, MPI3MR_EVENT, "PCIE Enumeration: (%s)",
			(event_data->ReasonCode ==
			    MPI3_EVENT_PCIE_ENUM_RC_STARTED) ? "start" :
			    "stop");
		if (event_data->EnumerationStatus)
			mpi3mr_dprint(sc, MPI3MR_EVENT, "enumeration_status(0x%08x)",
			   event_data->EnumerationStatus);
		if (sc->mpi3mr_debug & MPI3MR_EVENT)
			printf("\n");
		return;
	}
	case MPI3_EVENT_PREPARE_FOR_RESET:
		desc = "Prepare For Reset";
		break;
	}

	if (!desc)
		return;

	mpi3mr_dprint(sc, MPI3MR_EVENT, "%s\n", desc);
}

struct mpi3mr_target *
mpi3mr_find_target_by_per_id(struct mpi3mr_cam_softc *cam_sc,
    uint16_t per_id)
{
	struct mpi3mr_target *target = NULL;

	mtx_lock_spin(&cam_sc->sc->target_lock);
	TAILQ_FOREACH(target, &cam_sc->tgt_list, tgt_next) {
		if (target->per_id == per_id)
			break;
	}

	mtx_unlock_spin(&cam_sc->sc->target_lock);
	return target;
}

struct mpi3mr_target *
mpi3mr_find_target_by_dev_handle(struct mpi3mr_cam_softc *cam_sc,
    uint16_t handle)
{
	struct mpi3mr_target *target = NULL;

	mtx_lock_spin(&cam_sc->sc->target_lock);
	TAILQ_FOREACH(target, &cam_sc->tgt_list, tgt_next) {
		if (target->dev_handle == handle)
			break;

	}
	mtx_unlock_spin(&cam_sc->sc->target_lock);
	return target;
}

void mpi3mr_update_device(struct mpi3mr_softc *sc,
    struct mpi3mr_target *tgtdev, Mpi3DevicePage0_t *dev_pg0,
    bool is_added)
{
	U16 flags = 0;

	tgtdev->per_id = (dev_pg0->PersistentID);
	tgtdev->dev_handle = (dev_pg0->DevHandle);
	tgtdev->dev_type = dev_pg0->DeviceForm;
	tgtdev->encl_handle = (dev_pg0->EnclosureHandle);
	tgtdev->parent_handle = (dev_pg0->ParentDevHandle);
	tgtdev->slot = (dev_pg0->Slot);
	tgtdev->qdepth = (dev_pg0->QueueDepth);
	tgtdev->wwid = (dev_pg0->WWID);
	
	flags = (dev_pg0->Flags);
	tgtdev->is_hidden = (flags & MPI3_DEVICE0_FLAGS_HIDDEN);
	if (is_added == true)
		tgtdev->io_throttle_enabled =
		    (flags & MPI3_DEVICE0_FLAGS_IO_THROTTLING_REQUIRED) ? 1 : 0;
 
	switch (dev_pg0->AccessStatus) {
	case MPI3_DEVICE0_ASTATUS_NO_ERRORS:
	case MPI3_DEVICE0_ASTATUS_PREPARE:
	case MPI3_DEVICE0_ASTATUS_NEEDS_INITIALIZATION:
	case MPI3_DEVICE0_ASTATUS_DEVICE_MISSING_DELAY:
		break;
	default:
		tgtdev->is_hidden = 1;
		break;
	}

	switch (tgtdev->dev_type) {
	case MPI3_DEVICE_DEVFORM_SAS_SATA:
	{
		Mpi3Device0SasSataFormat_t *sasinf =
		    &dev_pg0->DeviceSpecific.SasSataFormat;
		U16 dev_info = (sasinf->DeviceInfo);
		tgtdev->dev_spec.sassata_inf.dev_info = dev_info;
		tgtdev->dev_spec.sassata_inf.sas_address =
		    (sasinf->SASAddress);
		if ((dev_info & MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_MASK) !=
		    MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_END_DEVICE)
			tgtdev->is_hidden = 1;
		else if (!(dev_info & (MPI3_SAS_DEVICE_INFO_STP_SATA_TARGET |
			    MPI3_SAS_DEVICE_INFO_SSP_TARGET)))
			tgtdev->is_hidden = 1;
		break;
	}
	case MPI3_DEVICE_DEVFORM_PCIE:
	{
		Mpi3Device0PcieFormat_t *pcieinf =
		    &dev_pg0->DeviceSpecific.PcieFormat;
		U16 dev_info = (pcieinf->DeviceInfo);

		tgtdev->q_depth = dev_pg0->QueueDepth;
		tgtdev->dev_spec.pcie_inf.dev_info = dev_info;
		tgtdev->dev_spec.pcie_inf.capb =
		    (pcieinf->Capabilities);
		tgtdev->dev_spec.pcie_inf.mdts = MPI3MR_DEFAULT_MDTS;
		if (dev_pg0->AccessStatus == MPI3_DEVICE0_ASTATUS_NO_ERRORS) {
			tgtdev->dev_spec.pcie_inf.mdts =
			    (pcieinf->MaximumDataTransferSize);
			tgtdev->dev_spec.pcie_inf.pgsz = pcieinf->PageSize;
			tgtdev->dev_spec.pcie_inf.reset_to =
				pcieinf->ControllerResetTO;
			tgtdev->dev_spec.pcie_inf.abort_to =
				pcieinf->NVMeAbortTO;
		}
		if (tgtdev->dev_spec.pcie_inf.mdts > (1024 * 1024))
			tgtdev->dev_spec.pcie_inf.mdts = (1024 * 1024);

		if (((dev_info & MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_MASK) !=
		    MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_NVME_DEVICE) &&
		    ((dev_info & MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_MASK) !=
		    MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_SCSI_DEVICE))
			tgtdev->is_hidden = 1;

		break;
	}
	case MPI3_DEVICE_DEVFORM_VD:
	{
		Mpi3Device0VdFormat_t *vdinf =
		    &dev_pg0->DeviceSpecific.VdFormat;
		struct mpi3mr_throttle_group_info *tg = NULL;
	
		tgtdev->dev_spec.vol_inf.state = vdinf->VdState;
		if (vdinf->VdState == MPI3_DEVICE0_VD_STATE_OFFLINE)
			tgtdev->is_hidden = 1;
		tgtdev->dev_spec.vol_inf.tg_id = vdinf->IOThrottleGroup;
		tgtdev->dev_spec.vol_inf.tg_high =
			vdinf->IOThrottleGroupHigh * 2048;
		tgtdev->dev_spec.vol_inf.tg_low =
			vdinf->IOThrottleGroupLow * 2048;
		if (vdinf->IOThrottleGroup < sc->num_io_throttle_group) {
			tg = sc->throttle_groups + vdinf->IOThrottleGroup;
			tg->id = vdinf->IOThrottleGroup;
			tg->high = tgtdev->dev_spec.vol_inf.tg_high;
			tg->low = tgtdev->dev_spec.vol_inf.tg_low;
			if (is_added == true)
				tg->fw_qd = tgtdev->q_depth;
			tg->modified_qd = tgtdev->q_depth;
		}
		tgtdev->dev_spec.vol_inf.tg = tg;
		tgtdev->throttle_group = tg;
		break;
	}
	default:
		goto out;
	}

out:
	return;
}

int mpi3mr_create_device(struct mpi3mr_softc *sc,
    Mpi3DevicePage0_t *dev_pg0)
{
	int retval = 0;
	struct mpi3mr_target *target = NULL;
	U16 per_id = 0;

	per_id = dev_pg0->PersistentID;

	mtx_lock_spin(&sc->target_lock);
	TAILQ_FOREACH(target, &sc->cam_sc->tgt_list, tgt_next) {
		if (target->per_id == per_id) {
			target->state = MPI3MR_DEV_CREATED;
			break;
		}
	}
	mtx_unlock_spin(&sc->target_lock);

	if (target) {
			mpi3mr_update_device(sc, target, dev_pg0, true);
	} else {
			target = malloc(sizeof(*target), M_MPI3MR,
				 M_NOWAIT | M_ZERO);

			if (target == NULL) {
				retval = -1;
				goto out;
			}
			
			target->exposed_to_os = 0;
			mpi3mr_update_device(sc, target, dev_pg0, true);
			mtx_lock_spin(&sc->target_lock);
			TAILQ_INSERT_TAIL(&sc->cam_sc->tgt_list, target, tgt_next);
			target->state = MPI3MR_DEV_CREATED;
			mtx_unlock_spin(&sc->target_lock);
	}
out:
	return retval;
}

/**
 * mpi3mr_dev_rmhs_complete_iou - Device removal IOUC completion
 * @sc: Adapter instance reference
 * @drv_cmd: Internal command tracker
 *
 * Issues a target reset TM to the firmware from the device
 * removal TM pend list or retry the removal handshake sequence
 * based on the IOU control request IOC status.
 *
 * Return: Nothing
 */
static void mpi3mr_dev_rmhs_complete_iou(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drv_cmd)
{
	U16 cmd_idx = drv_cmd->host_tag - MPI3MR_HOSTTAG_DEVRMCMD_MIN;
	struct delayed_dev_rmhs_node *delayed_dev_rmhs = NULL;

	mpi3mr_dprint(sc, MPI3MR_EVENT,
	    "%s :dev_rmhs_iouctrl_complete:handle(0x%04x), ioc_status(0x%04x), loginfo(0x%08x)\n",
	    __func__, drv_cmd->dev_handle, drv_cmd->ioc_status,
	    drv_cmd->ioc_loginfo);
	if (drv_cmd->ioc_status != MPI3_IOCSTATUS_SUCCESS) {
		if (drv_cmd->retry_count < MPI3MR_DEVRMHS_RETRYCOUNT) {
			drv_cmd->retry_count++;
			mpi3mr_dprint(sc, MPI3MR_EVENT,
			    "%s :dev_rmhs_iouctrl_complete: handle(0x%04x)retrying handshake retry=%d\n",
			    __func__, drv_cmd->dev_handle,
			    drv_cmd->retry_count);
			mpi3mr_dev_rmhs_send_tm(sc, drv_cmd->dev_handle,
			    drv_cmd, drv_cmd->iou_rc);
			return;
		}
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "%s :dev removal handshake failed after all retries: handle(0x%04x)\n",
		    __func__, drv_cmd->dev_handle);
	} else {
		mpi3mr_dprint(sc, MPI3MR_INFO,
		    "%s :dev removal handshake completed successfully: handle(0x%04x)\n",
		    __func__, drv_cmd->dev_handle);
		mpi3mr_clear_bit(drv_cmd->dev_handle, sc->removepend_bitmap);
	}

	if (!TAILQ_EMPTY(&sc->delayed_rmhs_list)) {
		delayed_dev_rmhs = TAILQ_FIRST(&sc->delayed_rmhs_list);
		drv_cmd->dev_handle = delayed_dev_rmhs->handle;
		drv_cmd->retry_count = 0;
		drv_cmd->iou_rc = delayed_dev_rmhs->iou_rc;
		mpi3mr_dprint(sc, MPI3MR_EVENT,
		    "%s :dev_rmhs_iouctrl_complete: processing delayed TM: handle(0x%04x)\n",
		    __func__, drv_cmd->dev_handle);
		mpi3mr_dev_rmhs_send_tm(sc, drv_cmd->dev_handle, drv_cmd,
		    drv_cmd->iou_rc);
		TAILQ_REMOVE(&sc->delayed_rmhs_list, delayed_dev_rmhs, list);
		free(delayed_dev_rmhs, M_MPI3MR);
		return;
	}
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	drv_cmd->callback = NULL;
	drv_cmd->retry_count = 0;
	drv_cmd->dev_handle = MPI3MR_INVALID_DEV_HANDLE;
	mpi3mr_clear_bit(cmd_idx, sc->devrem_bitmap);
}

/**
 * mpi3mr_dev_rmhs_complete_tm - Device removal TM completion
 * @sc: Adapter instance reference
 * @drv_cmd: Internal command tracker
 *
 * Issues a target reset TM to the firmware from the device
 * removal TM pend list or issue IO Unit control request as
 * part of device removal or hidden acknowledgment handshake.
 *
 * Return: Nothing
 */
static void mpi3mr_dev_rmhs_complete_tm(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drv_cmd)
{
	Mpi3IoUnitControlRequest_t iou_ctrl;
	U16 cmd_idx = drv_cmd->host_tag - MPI3MR_HOSTTAG_DEVRMCMD_MIN;
	Mpi3SCSITaskMgmtReply_t *tm_reply = NULL;
	int retval;

	if (drv_cmd->state & MPI3MR_CMD_REPLYVALID)
		tm_reply = (Mpi3SCSITaskMgmtReply_t *)drv_cmd->reply;

	if (tm_reply)
		printf(IOCNAME
		    "dev_rmhs_tr_complete:handle(0x%04x), ioc_status(0x%04x), loginfo(0x%08x), term_count(%d)\n",
		    sc->name, drv_cmd->dev_handle, drv_cmd->ioc_status,
		    drv_cmd->ioc_loginfo,
		    le32toh(tm_reply->TerminationCount));

	printf(IOCNAME "Issuing IOU CTL: handle(0x%04x) dev_rmhs idx(%d)\n",
	    sc->name, drv_cmd->dev_handle, cmd_idx);

	memset(&iou_ctrl, 0, sizeof(iou_ctrl));

	drv_cmd->state = MPI3MR_CMD_PENDING;
	drv_cmd->is_waiting = 0;
	drv_cmd->callback = mpi3mr_dev_rmhs_complete_iou;
	iou_ctrl.Operation = drv_cmd->iou_rc;
	iou_ctrl.Param16[0] = htole16(drv_cmd->dev_handle);
	iou_ctrl.HostTag = htole16(drv_cmd->host_tag);
	iou_ctrl.Function = MPI3_FUNCTION_IO_UNIT_CONTROL;

	retval = mpi3mr_submit_admin_cmd(sc, &iou_ctrl, sizeof(iou_ctrl));
	if (retval) {
		printf(IOCNAME "Issue DevRmHsTMIOUCTL: Admin post failed\n",
		    sc->name);
		goto out_failed;
	}

	return;
out_failed:
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	drv_cmd->callback = NULL;
	drv_cmd->dev_handle = MPI3MR_INVALID_DEV_HANDLE;
	drv_cmd->retry_count = 0;
	mpi3mr_clear_bit(cmd_idx, sc->devrem_bitmap);
}

/**
 * mpi3mr_dev_rmhs_send_tm - Issue TM for device removal
 * @sc: Adapter instance reference
 * @handle: Device handle
 * @cmdparam: Internal command tracker
 * @iou_rc: IO Unit reason code
 *
 * Issues a target reset TM to the firmware or add it to a pend
 * list as part of device removal or hidden acknowledgment
 * handshake.
 *
 * Return: Nothing
 */
static void mpi3mr_dev_rmhs_send_tm(struct mpi3mr_softc *sc, U16 handle,
	struct mpi3mr_drvr_cmd *cmdparam, U8 iou_rc)
{
	Mpi3SCSITaskMgmtRequest_t tm_req;
	int retval = 0;
	U16 cmd_idx = MPI3MR_NUM_DEVRMCMD;
	U8 retrycount = 5;
	struct mpi3mr_drvr_cmd *drv_cmd = cmdparam;
	struct delayed_dev_rmhs_node *delayed_dev_rmhs = NULL;
	struct mpi3mr_target *tgtdev = NULL;
	
	mtx_lock_spin(&sc->target_lock);
	TAILQ_FOREACH(tgtdev, &sc->cam_sc->tgt_list, tgt_next) {
		if ((tgtdev->dev_handle == handle) &&
		    (iou_rc == MPI3_CTRL_OP_REMOVE_DEVICE)) {
			tgtdev->state = MPI3MR_DEV_REMOVE_HS_STARTED;
			break;
		}
	}
	mtx_unlock_spin(&sc->target_lock);

	if (drv_cmd)
		goto issue_cmd;
	do {
		cmd_idx = mpi3mr_find_first_zero_bit(sc->devrem_bitmap,
		    MPI3MR_NUM_DEVRMCMD);
		if (cmd_idx < MPI3MR_NUM_DEVRMCMD) {
			if (!mpi3mr_test_and_set_bit(cmd_idx, sc->devrem_bitmap))
				break;
			cmd_idx = MPI3MR_NUM_DEVRMCMD;
		}
	} while (retrycount--);
	
	if (cmd_idx >= MPI3MR_NUM_DEVRMCMD) {
		delayed_dev_rmhs = malloc(sizeof(*delayed_dev_rmhs),M_MPI3MR,
		     M_ZERO|M_NOWAIT);

		if (!delayed_dev_rmhs)
			return;
		delayed_dev_rmhs->handle = handle;
		delayed_dev_rmhs->iou_rc = iou_rc;
		TAILQ_INSERT_TAIL(&(sc->delayed_rmhs_list), delayed_dev_rmhs, list);
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s :DevRmHs: tr:handle(0x%04x) is postponed\n",
		    __func__, handle);
		

		return;
	}
	drv_cmd = &sc->dev_rmhs_cmds[cmd_idx];

issue_cmd:
	cmd_idx = drv_cmd->host_tag - MPI3MR_HOSTTAG_DEVRMCMD_MIN;
	mpi3mr_dprint(sc, MPI3MR_EVENT,
	    "%s :Issuing TR TM: for devhandle 0x%04x with dev_rmhs %d\n",
	    __func__, handle, cmd_idx);

	memset(&tm_req, 0, sizeof(tm_req));
	if (drv_cmd->state & MPI3MR_CMD_PENDING) {
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s :Issue TM: Command is in use\n", __func__);
		goto out;
	}
	drv_cmd->state = MPI3MR_CMD_PENDING;
	drv_cmd->is_waiting = 0;
	drv_cmd->callback = mpi3mr_dev_rmhs_complete_tm;
	drv_cmd->dev_handle = handle;
	drv_cmd->iou_rc = iou_rc;
	tm_req.DevHandle = htole16(handle);
	tm_req.TaskType = MPI3_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	tm_req.HostTag = htole16(drv_cmd->host_tag);
	tm_req.TaskHostTag = htole16(MPI3MR_HOSTTAG_INVALID);
	tm_req.Function = MPI3_FUNCTION_SCSI_TASK_MGMT;

	mpi3mr_set_bit(handle, sc->removepend_bitmap);
	retval = mpi3mr_submit_admin_cmd(sc, &tm_req, sizeof(tm_req));
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "%s :Issue DevRmHsTM: Admin Post failed\n",
		    __func__);
		goto out_failed;
	}
out:
	return;
out_failed:
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	drv_cmd->callback = NULL;
	drv_cmd->dev_handle = MPI3MR_INVALID_DEV_HANDLE;
	drv_cmd->retry_count = 0;
	mpi3mr_clear_bit(cmd_idx, sc->devrem_bitmap);
}

/**
 * mpi3mr_complete_evt_ack - Event ack request completion
 * @sc: Adapter instance reference
 * @drv_cmd: Internal command tracker
 *
 * This is the completion handler for non blocking event
 * acknowledgment sent to the firmware and this will issue any
 * pending event acknowledgment request.
 *
 * Return: Nothing
 */
static void mpi3mr_complete_evt_ack(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *drv_cmd)
{
	U16 cmd_idx = drv_cmd->host_tag - MPI3MR_HOSTTAG_EVTACKCMD_MIN;
	struct delayed_evtack_node *delayed_evtack = NULL;

	if (drv_cmd->ioc_status != MPI3_IOCSTATUS_SUCCESS) {
		mpi3mr_dprint(sc, MPI3MR_EVENT,
		    "%s: Failed IOCStatus(0x%04x) Loginfo(0x%08x)\n", __func__,
		    (drv_cmd->ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		    drv_cmd->ioc_loginfo);
	}

	if (!TAILQ_EMPTY(&sc->delayed_evtack_cmds_list)) {
		delayed_evtack = TAILQ_FIRST(&sc->delayed_evtack_cmds_list);
		mpi3mr_dprint(sc, MPI3MR_EVENT,
		    "%s: processing delayed event ack for event %d\n",
		    __func__, delayed_evtack->event);
		mpi3mr_send_evt_ack(sc, delayed_evtack->event, drv_cmd,
		    delayed_evtack->event_ctx);
		TAILQ_REMOVE(&sc->delayed_evtack_cmds_list, delayed_evtack, list);
		free(delayed_evtack, M_MPI3MR);
		return;
	}
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	drv_cmd->callback = NULL;
	mpi3mr_clear_bit(cmd_idx, sc->evtack_cmds_bitmap);
}

/**
 * mpi3mr_send_evt_ack - Issue event acknwoledgment request
 * @sc: Adapter instance reference
 * @event: MPI3 event id
 * @cmdparam: Internal command tracker
 * @event_ctx: Event context
 *
 * Issues event acknowledgment request to the firmware if there
 * is a free command to send the event ack else it to a pend
 * list so that it will be processed on a completion of a prior
 * event acknowledgment .
 *
 * Return: Nothing
 */
static void mpi3mr_send_evt_ack(struct mpi3mr_softc *sc, U8 event,
	struct mpi3mr_drvr_cmd *cmdparam, U32 event_ctx)
{
	Mpi3EventAckRequest_t evtack_req;
	int retval = 0;
	U8 retrycount = 5;
	U16 cmd_idx = MPI3MR_NUM_EVTACKCMD;
	struct mpi3mr_drvr_cmd *drv_cmd = cmdparam;
	struct delayed_evtack_node *delayed_evtack = NULL;

	if (drv_cmd)
		goto issue_cmd;
	do {
		cmd_idx = mpi3mr_find_first_zero_bit(sc->evtack_cmds_bitmap,
		    MPI3MR_NUM_EVTACKCMD);
		if (cmd_idx < MPI3MR_NUM_EVTACKCMD) {
			if (!mpi3mr_test_and_set_bit(cmd_idx,
			    sc->evtack_cmds_bitmap))
				break;
			cmd_idx = MPI3MR_NUM_EVTACKCMD;
		}
	} while (retrycount--);

	if (cmd_idx >= MPI3MR_NUM_EVTACKCMD) {
		delayed_evtack = malloc(sizeof(*delayed_evtack),M_MPI3MR,
		     M_ZERO | M_NOWAIT);
		if (!delayed_evtack)
			return;
		delayed_evtack->event = event;
		delayed_evtack->event_ctx = event_ctx;
		TAILQ_INSERT_TAIL(&(sc->delayed_evtack_cmds_list), delayed_evtack, list);
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s : Event ack for event:%d is postponed\n",
		    __func__, event);
		return;
	}
	drv_cmd = &sc->evtack_cmds[cmd_idx];

issue_cmd:
	cmd_idx = drv_cmd->host_tag - MPI3MR_HOSTTAG_EVTACKCMD_MIN;

	memset(&evtack_req, 0, sizeof(evtack_req));
	if (drv_cmd->state & MPI3MR_CMD_PENDING) {
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s: Command is in use\n", __func__);
		goto out;
	}
	drv_cmd->state = MPI3MR_CMD_PENDING;
	drv_cmd->is_waiting = 0;
	drv_cmd->callback = mpi3mr_complete_evt_ack;
	evtack_req.HostTag = htole16(drv_cmd->host_tag);
	evtack_req.Function = MPI3_FUNCTION_EVENT_ACK;
	evtack_req.Event = event;
	evtack_req.EventContext = htole32(event_ctx);
	retval = mpi3mr_submit_admin_cmd(sc, &evtack_req,
	    sizeof(evtack_req));

	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Admin Post failed\n", __func__);
		goto out_failed;
	}
out:
	return;
out_failed:
	drv_cmd->state = MPI3MR_CMD_NOTUSED;
	drv_cmd->callback = NULL;
	mpi3mr_clear_bit(cmd_idx, sc->evtack_cmds_bitmap);
}

/*
 * mpi3mr_pcietopochg_evt_th - PCIETopologyChange evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Checks for the reason code and based on that either block I/O
 * to device, or unblock I/O to the device, or start the device
 * removal handshake with reason as remove with the firmware for
 * PCIe devices.
 *
 * Return: Nothing
 */
static void mpi3mr_pcietopochg_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	Mpi3EventDataPcieTopologyChangeList_t *topo_evt =
	    (Mpi3EventDataPcieTopologyChangeList_t *) event_reply->EventData;
	int i;
	U16 handle;
	U8 reason_code;
	struct mpi3mr_target *tgtdev = NULL;

	for (i = 0; i < topo_evt->NumEntries; i++) {
		handle = le16toh(topo_evt->PortEntry[i].AttachedDevHandle);
		if (!handle)
			continue;
		reason_code = topo_evt->PortEntry[i].PortStatus;
		tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, handle);
		switch (reason_code) {
		case MPI3_EVENT_PCIE_TOPO_PS_NOT_RESPONDING:
			if (tgtdev) {
				tgtdev->dev_removed = 1;
				tgtdev->dev_removedelay = 0;
				mpi3mr_atomic_set(&tgtdev->block_io, 0);
			}
			mpi3mr_dev_rmhs_send_tm(sc, handle, NULL,
			    MPI3_CTRL_OP_REMOVE_DEVICE);
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_DELAY_NOT_RESPONDING:
			if (tgtdev) {
				tgtdev->dev_removedelay = 1;
				mpi3mr_atomic_inc(&tgtdev->block_io);
			}
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_RESPONDING:
			if (tgtdev &&
			    tgtdev->dev_removedelay) {
				tgtdev->dev_removedelay = 0;
				if (mpi3mr_atomic_read(&tgtdev->block_io) > 0)
					mpi3mr_atomic_dec(&tgtdev->block_io);
			}
			break;
		case MPI3_EVENT_PCIE_TOPO_PS_PORT_CHANGED:
		default:
			break;
		}
	}
}

/**
 * mpi3mr_sastopochg_evt_th - SASTopologyChange evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Checks for the reason code and based on that either block I/O
 * to device, or unblock I/O to the device, or start the device
 * removal handshake with reason as remove with the firmware for
 * SAS/SATA devices.
 *
 * Return: Nothing
 */
static void mpi3mr_sastopochg_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	Mpi3EventDataSasTopologyChangeList_t *topo_evt =
	    (Mpi3EventDataSasTopologyChangeList_t *)event_reply->EventData;
	int i;
	U16 handle;
	U8 reason_code;
	struct mpi3mr_target *tgtdev = NULL;

	for (i = 0; i < topo_evt->NumEntries; i++) {
		handle = le16toh(topo_evt->PhyEntry[i].AttachedDevHandle);
		if (!handle)
			continue;
		reason_code = topo_evt->PhyEntry[i].Status &
		    MPI3_EVENT_SAS_TOPO_PHY_RC_MASK;
		tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, handle);
		switch (reason_code) {
		case MPI3_EVENT_SAS_TOPO_PHY_RC_TARG_NOT_RESPONDING:
			if (tgtdev) {
				tgtdev->dev_removed = 1;
				tgtdev->dev_removedelay = 0;
				mpi3mr_atomic_set(&tgtdev->block_io, 0);
			}
			mpi3mr_dev_rmhs_send_tm(sc, handle, NULL,
			    MPI3_CTRL_OP_REMOVE_DEVICE);
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_DELAY_NOT_RESPONDING:
			if (tgtdev) {
				tgtdev->dev_removedelay = 1;
				mpi3mr_atomic_inc(&tgtdev->block_io);
			}
			break;
		case MPI3_EVENT_SAS_TOPO_PHY_RC_RESPONDING:
			if (tgtdev &&
			    tgtdev->dev_removedelay) {
				tgtdev->dev_removedelay = 0;
				if (mpi3mr_atomic_read(&tgtdev->block_io) > 0)
					mpi3mr_atomic_dec(&tgtdev->block_io);
			}
		case MPI3_EVENT_SAS_TOPO_PHY_RC_PHY_CHANGED:
		default:
			break;
		}
	}

}
/**
 * mpi3mr_devstatuschg_evt_th - DeviceStatusChange evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Checks for the reason code and based on that either block I/O
 * to device, or unblock I/O to the device, or start the device
 * removal handshake with reason as remove/hide acknowledgment
 * with the firmware.
 *
 * Return: Nothing
 */
static void mpi3mr_devstatuschg_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	U16 dev_handle = 0;
	U8 ublock = 0, block = 0, hide = 0, uhide = 0, delete = 0, remove = 0;
	struct mpi3mr_target *tgtdev = NULL;
	Mpi3EventDataDeviceStatusChange_t *evtdata =
	    (Mpi3EventDataDeviceStatusChange_t *) event_reply->EventData;

	dev_handle = le16toh(evtdata->DevHandle);

	switch (evtdata->ReasonCode) {
	case MPI3_EVENT_DEV_STAT_RC_INT_DEVICE_RESET_STRT:
	case MPI3_EVENT_DEV_STAT_RC_INT_IT_NEXUS_RESET_STRT:
		block = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_HIDDEN:
		delete = 1;
		hide = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_NOT_HIDDEN:
		uhide = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_VD_NOT_RESPONDING:
		delete = 1;
		remove = 1;
		break;
	case MPI3_EVENT_DEV_STAT_RC_INT_DEVICE_RESET_CMP:
	case MPI3_EVENT_DEV_STAT_RC_INT_IT_NEXUS_RESET_CMP:
		ublock = 1;
		break;
	default:
		break;
	}

	tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, dev_handle);

	if (!tgtdev) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "%s :target with dev_handle:0x%x not found\n",
		    __func__, dev_handle);
		return;
	}

	if (block)
		mpi3mr_atomic_inc(&tgtdev->block_io);

	if (hide)
		tgtdev->is_hidden = hide;
	
	if (uhide) {
		tgtdev->is_hidden = 0;
		tgtdev->dev_removed = 0;
	}

	if (delete)
		tgtdev->dev_removed = 1;

	if (ublock) {
		if (mpi3mr_atomic_read(&tgtdev->block_io) > 0)
			mpi3mr_atomic_dec(&tgtdev->block_io);
	}
	
	if (remove) {
		mpi3mr_dev_rmhs_send_tm(sc, dev_handle, NULL,
					MPI3_CTRL_OP_REMOVE_DEVICE);
	}
	if (hide)
		mpi3mr_dev_rmhs_send_tm(sc, dev_handle, NULL,
					MPI3_CTRL_OP_HIDDEN_ACK);
}

/**
 * mpi3mr_preparereset_evt_th - Prepareforreset evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Blocks and unblocks host level I/O based on the reason code
 *
 * Return: Nothing
 */
static void mpi3mr_preparereset_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	Mpi3EventDataPrepareForReset_t *evtdata =
	    (Mpi3EventDataPrepareForReset_t *)event_reply->EventData;

	if (evtdata->ReasonCode == MPI3_EVENT_PREPARE_RESET_RC_START) {
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s :Recieved PrepForReset Event with RC=START\n",
		    __func__);
		if (sc->prepare_for_reset)
			return;
		sc->prepare_for_reset = 1;
		sc->prepare_for_reset_timeout_counter = 0;
	} else if (evtdata->ReasonCode == MPI3_EVENT_PREPARE_RESET_RC_ABORT) {
		mpi3mr_dprint(sc, MPI3MR_EVENT, "%s :Recieved PrepForReset Event with RC=ABORT\n",
		    __func__);
		sc->prepare_for_reset = 0;
		sc->prepare_for_reset_timeout_counter = 0;
	}
	if ((event_reply->MsgFlags & MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_MASK)
	    == MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_REQUIRED)
		mpi3mr_send_evt_ack(sc, event_reply->Event, NULL,
		    le32toh(event_reply->EventContext));
}

/**
 * mpi3mr_energypackchg_evt_th - Energypackchange evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Identifies the new shutdown timeout value and update.
 *
 * Return: Nothing
 */
static void mpi3mr_energypackchg_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	Mpi3EventDataEnergyPackChange_t *evtdata =
	    (Mpi3EventDataEnergyPackChange_t *)event_reply->EventData;
	U16 shutdown_timeout = le16toh(evtdata->ShutdownTimeout);

	if (shutdown_timeout <= 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "%s :Invalid Shutdown Timeout received = %d\n",
		    __func__, shutdown_timeout);
		return;
	}

	mpi3mr_dprint(sc, MPI3MR_EVENT,
	    "%s :Previous Shutdown Timeout Value = %d New Shutdown Timeout Value = %d\n",
	    __func__, sc->facts.shutdown_timeout, shutdown_timeout);
	sc->facts.shutdown_timeout = shutdown_timeout;
}

/**
 * mpi3mr_cablemgmt_evt_th - Cable mgmt evt tophalf
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Displays Cable manegemt event details.
 *
 * Return: Nothing
 */
static void mpi3mr_cablemgmt_evt_th(struct mpi3mr_softc *sc,
	Mpi3EventNotificationReply_t *event_reply)
{
	Mpi3EventDataCableManagement_t *evtdata =
	    (Mpi3EventDataCableManagement_t *)event_reply->EventData;

	switch (evtdata->Status) {
	case MPI3_EVENT_CABLE_MGMT_STATUS_INSUFFICIENT_POWER:
	{
		mpi3mr_dprint(sc, MPI3MR_INFO, "An active cable with ReceptacleID %d cannot be powered.\n"
		    "Devices connected to this cable are not detected.\n"
		    "This cable requires %d mW of power.\n",
		    evtdata->ReceptacleID,
		    le32toh(evtdata->ActiveCablePowerRequirement));
		break;
	}
	case MPI3_EVENT_CABLE_MGMT_STATUS_DEGRADED:
	{
		mpi3mr_dprint(sc, MPI3MR_INFO, "A cable with ReceptacleID %d is not running at optimal speed\n",
		    evtdata->ReceptacleID);
		break;
	}
	default:
		break;
	}
}

/**
 * mpi3mr_process_events - Event's toph-half handler
 * @sc: Adapter instance reference
 * @event_reply: Event data
 *
 * Top half of event processing.
 *
 * Return: Nothing
 */
static void mpi3mr_process_events(struct mpi3mr_softc *sc,
    uintptr_t data, Mpi3EventNotificationReply_t *event_reply)
{
	U16 evt_type;
	bool ack_req = 0, process_evt_bh = 0;
	struct mpi3mr_fw_event_work *fw_event;
	U16 sz;

	if (sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN)
		goto out;

	if ((event_reply->MsgFlags & MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_MASK)
	    == MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_REQUIRED)
		ack_req = 1;

	evt_type = event_reply->Event;

	switch (evt_type) {
	case MPI3_EVENT_DEVICE_ADDED:
	{
		Mpi3DevicePage0_t *dev_pg0 =
			(Mpi3DevicePage0_t *) event_reply->EventData;
		if (mpi3mr_create_device(sc, dev_pg0))
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			"%s :Failed to add device in the device add event\n",
			__func__);
		else
			process_evt_bh = 1;
		break;
	}
	
	case MPI3_EVENT_DEVICE_STATUS_CHANGE:
	{
		process_evt_bh = 1;
		mpi3mr_devstatuschg_evt_th(sc, event_reply);
		break;
	}
	case MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
	{
		process_evt_bh = 1;
		mpi3mr_sastopochg_evt_th(sc, event_reply);
		break;
	}
	case MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST:
	{
		process_evt_bh = 1;
		mpi3mr_pcietopochg_evt_th(sc, event_reply);
		break;
	}
	case MPI3_EVENT_PREPARE_FOR_RESET:
	{
		mpi3mr_preparereset_evt_th(sc, event_reply);
		ack_req = 0;
		break;
	}
	case MPI3_EVENT_DEVICE_INFO_CHANGED:
	case MPI3_EVENT_LOG_DATA:
	{
		process_evt_bh = 1;
		break;
	}
	case MPI3_EVENT_ENERGY_PACK_CHANGE:
	{
		mpi3mr_energypackchg_evt_th(sc, event_reply);
		break;
	}
	case MPI3_EVENT_CABLE_MGMT:
	{
		mpi3mr_cablemgmt_evt_th(sc, event_reply);
		break;
	}
	
	case MPI3_EVENT_ENCL_DEVICE_STATUS_CHANGE:
	case MPI3_EVENT_SAS_DISCOVERY:
	case MPI3_EVENT_SAS_DEVICE_DISCOVERY_ERROR:
	case MPI3_EVENT_SAS_BROADCAST_PRIMITIVE:
	case MPI3_EVENT_PCIE_ENUMERATION:
		break;
	default:
		mpi3mr_dprint(sc, MPI3MR_INFO, "%s :Event 0x%02x is not handled by driver\n",
		    __func__, evt_type);
		break;
	}
	
	if (process_evt_bh || ack_req) {
		fw_event = malloc(sizeof(struct mpi3mr_fw_event_work), M_MPI3MR,
		     M_ZERO|M_NOWAIT);

		if (!fw_event) {
			printf("%s: allocate failed for fw_event\n", __func__);
			return;
		}

		sz = le16toh(event_reply->EventDataLength) * 4;
		fw_event->event_data = malloc(sz, M_MPI3MR, M_ZERO|M_NOWAIT);

		if (!fw_event->event_data) {
			printf("%s: allocate failed for event_data\n", __func__);
			free(fw_event, M_MPI3MR);
			return;
		}

		bcopy(event_reply->EventData, fw_event->event_data, sz);
		fw_event->event = event_reply->Event;
		if ((event_reply->Event == MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
		    event_reply->Event == MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST ||
		    event_reply->Event == MPI3_EVENT_ENCL_DEVICE_STATUS_CHANGE ) &&
		    sc->track_mapping_events)
			sc->pending_map_events++;

		/*
		 * Events should be processed after Port enable is completed.
		 */
		if ((event_reply->Event == MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
		    event_reply->Event == MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST ) &&
		    !(sc->mpi3mr_flags & MPI3MR_FLAGS_PORT_ENABLE_DONE))
			mpi3mr_startup_increment(sc->cam_sc);

		fw_event->send_ack = ack_req;
		fw_event->event_context = le32toh(event_reply->EventContext);
		fw_event->event_data_size = sz;
		fw_event->process_event = process_evt_bh;

		mtx_lock(&sc->fwevt_lock);
		TAILQ_INSERT_TAIL(&sc->cam_sc->ev_queue, fw_event, ev_link);
		taskqueue_enqueue(sc->cam_sc->ev_tq, &sc->cam_sc->ev_task);
		mtx_unlock(&sc->fwevt_lock);

	}
out:
	return;
}

static void mpi3mr_handle_events(struct mpi3mr_softc *sc, uintptr_t data,
    Mpi3DefaultReply_t *def_reply)
{
	Mpi3EventNotificationReply_t *event_reply =
		(Mpi3EventNotificationReply_t *)def_reply;

	sc->change_count = event_reply->IOCChangeCount;
	mpi3mr_display_event_data(sc, event_reply);

	mpi3mr_process_events(sc, data, event_reply);
}

static void mpi3mr_process_admin_reply_desc(struct mpi3mr_softc *sc,
    Mpi3DefaultReplyDescriptor_t *reply_desc, U64 *reply_dma)
{
	U16 reply_desc_type, host_tag = 0, idx;
	U16 ioc_status = MPI3_IOCSTATUS_SUCCESS;
	U32 ioc_loginfo = 0;
	Mpi3StatusReplyDescriptor_t *status_desc;
	Mpi3AddressReplyDescriptor_t *addr_desc;
	Mpi3SuccessReplyDescriptor_t *success_desc;
	Mpi3DefaultReply_t *def_reply = NULL;
	struct mpi3mr_drvr_cmd *cmdptr = NULL;
	Mpi3SCSIIOReply_t *scsi_reply;
	U8 *sense_buf = NULL;

	*reply_dma = 0;
	reply_desc_type = reply_desc->ReplyFlags &
			    MPI3_REPLY_DESCRIPT_FLAGS_TYPE_MASK;
	switch (reply_desc_type) {
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_STATUS:
		status_desc = (Mpi3StatusReplyDescriptor_t *)reply_desc;
		host_tag = status_desc->HostTag;
		ioc_status = status_desc->IOCStatus;
		if (ioc_status &
		    MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_LOGINFOAVAIL)
			ioc_loginfo = status_desc->IOCLogInfo;
		ioc_status &= MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_STATUS_MASK;
		break;
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_ADDRESS_REPLY:
		addr_desc = (Mpi3AddressReplyDescriptor_t *)reply_desc;
		*reply_dma = addr_desc->ReplyFrameAddress;
		def_reply = mpi3mr_get_reply_virt_addr(sc, *reply_dma);
		if (def_reply == NULL)
			goto out;
		host_tag = def_reply->HostTag;
		ioc_status = def_reply->IOCStatus;
		if (ioc_status &
		    MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_LOGINFOAVAIL)
			ioc_loginfo = def_reply->IOCLogInfo;
		ioc_status &= MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_STATUS_MASK;
		if (def_reply->Function == MPI3_FUNCTION_SCSI_IO) {
			scsi_reply = (Mpi3SCSIIOReply_t *)def_reply;
			sense_buf = mpi3mr_get_sensebuf_virt_addr(sc,
			    scsi_reply->SenseDataBufferAddress);
		}
		break;
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_SUCCESS:
		success_desc = (Mpi3SuccessReplyDescriptor_t *)reply_desc;
		host_tag = success_desc->HostTag;
		break;
	default:
		break;
	}
	switch (host_tag) {
	case MPI3MR_HOSTTAG_INITCMDS:
		cmdptr = &sc->init_cmds;
		break;
	case MPI3MR_HOSTTAG_IOCTLCMDS:
		cmdptr = &sc->ioctl_cmds;
		break;
	case MPI3MR_HOSTTAG_TMS:
		cmdptr = &sc->host_tm_cmds;
		wakeup((void *)&sc->tm_chan);
		break;
	case MPI3MR_HOSTTAG_PELABORT:
		cmdptr = &sc->pel_abort_cmd;
		break;
	case MPI3MR_HOSTTAG_PELWAIT:
		cmdptr = &sc->pel_cmds;
		break;
	case MPI3MR_HOSTTAG_INVALID:
		if (def_reply && def_reply->Function ==
		    MPI3_FUNCTION_EVENT_NOTIFICATION)
			mpi3mr_handle_events(sc, *reply_dma ,def_reply);
	default:
		break;
	}

	if (host_tag >= MPI3MR_HOSTTAG_DEVRMCMD_MIN &&
	    host_tag <= MPI3MR_HOSTTAG_DEVRMCMD_MAX ) {
		idx = host_tag - MPI3MR_HOSTTAG_DEVRMCMD_MIN;
		cmdptr = &sc->dev_rmhs_cmds[idx];
	}

	if (host_tag >= MPI3MR_HOSTTAG_EVTACKCMD_MIN &&
	    host_tag <= MPI3MR_HOSTTAG_EVTACKCMD_MAX) {
		idx = host_tag - MPI3MR_HOSTTAG_EVTACKCMD_MIN;
		cmdptr = &sc->evtack_cmds[idx];
	}

	if (cmdptr) {
		if (cmdptr->state & MPI3MR_CMD_PENDING) {
			cmdptr->state |= MPI3MR_CMD_COMPLETE;
			cmdptr->ioc_loginfo = ioc_loginfo;
			cmdptr->ioc_status = ioc_status;
			cmdptr->state &= ~MPI3MR_CMD_PENDING;
			if (def_reply) {
				cmdptr->state |= MPI3MR_CMD_REPLYVALID;
				memcpy((U8 *)cmdptr->reply, (U8 *)def_reply,
				    sc->reply_sz);
			}
			if (sense_buf && cmdptr->sensebuf) {
				cmdptr->is_senseprst = 1;
				memcpy(cmdptr->sensebuf, sense_buf,
				    MPI3MR_SENSEBUF_SZ);
			}
			if (cmdptr->is_waiting) {
				complete(&cmdptr->completion);
				cmdptr->is_waiting = 0;
			} else if (cmdptr->callback)
				cmdptr->callback(sc, cmdptr);
		}
	}
out:
	if (sense_buf != NULL)
		mpi3mr_repost_sense_buf(sc,
		    scsi_reply->SenseDataBufferAddress);
	return;
}

/*
 * mpi3mr_complete_admin_cmd:	ISR routine for admin commands
 * @sc:				Adapter's soft instance
 *
 * This function processes admin command completions.
 */
static int mpi3mr_complete_admin_cmd(struct mpi3mr_softc *sc)
{
	U32 exp_phase = sc->admin_reply_ephase;
	U32 adm_reply_ci = sc->admin_reply_ci;
	U32 num_adm_reply = 0;
	U64 reply_dma = 0;
	Mpi3DefaultReplyDescriptor_t *reply_desc;
	
	mtx_lock_spin(&sc->admin_reply_lock);
	if (sc->admin_in_use == false) {
		sc->admin_in_use = true;
		mtx_unlock_spin(&sc->admin_reply_lock);
	} else {
		mtx_unlock_spin(&sc->admin_reply_lock);
		return 0;
	}

	reply_desc = (Mpi3DefaultReplyDescriptor_t *)sc->admin_reply +
		adm_reply_ci;
	
	if ((reply_desc->ReplyFlags &
	     MPI3_REPLY_DESCRIPT_FLAGS_PHASE_MASK) != exp_phase) {
		mtx_lock_spin(&sc->admin_reply_lock);
		sc->admin_in_use = false;
		mtx_unlock_spin(&sc->admin_reply_lock);
		return 0;
	}

	do {
		sc->admin_req_ci = reply_desc->RequestQueueCI;
		mpi3mr_process_admin_reply_desc(sc, reply_desc, &reply_dma);
		if (reply_dma)
			mpi3mr_repost_reply_buf(sc, reply_dma);
		num_adm_reply++;
		if (++adm_reply_ci == sc->num_admin_replies) {
			adm_reply_ci = 0;
			exp_phase ^= 1;
		}
		reply_desc =
			(Mpi3DefaultReplyDescriptor_t *)sc->admin_reply +
			    adm_reply_ci;
		if ((reply_desc->ReplyFlags &
		     MPI3_REPLY_DESCRIPT_FLAGS_PHASE_MASK) != exp_phase)
			break;
	} while (1);

	mpi3mr_regwrite(sc, MPI3_SYSIF_ADMIN_REPLY_Q_CI_OFFSET, adm_reply_ci);
	sc->admin_reply_ci = adm_reply_ci;
	sc->admin_reply_ephase = exp_phase;
	mtx_lock_spin(&sc->admin_reply_lock);
	sc->admin_in_use = false;
	mtx_unlock_spin(&sc->admin_reply_lock);
	return num_adm_reply;
}

static void
mpi3mr_cmd_done(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cmd)
{
	mpi3mr_unmap_request(sc, cmd);

	mtx_lock(&sc->mpi3mr_mtx);
	if (cmd->callout_owner) {
		callout_stop(&cmd->callout);
		cmd->callout_owner = false;
	}

	if (sc->unrecoverable)
		mpi3mr_set_ccbstatus(cmd->ccb, CAM_DEV_NOT_THERE);

	xpt_done(cmd->ccb);
	cmd->ccb = NULL;
	mtx_unlock(&sc->mpi3mr_mtx);
	mpi3mr_release_command(cmd);
}

void mpi3mr_process_op_reply_desc(struct mpi3mr_softc *sc,
    Mpi3DefaultReplyDescriptor_t *reply_desc, U64 *reply_dma)
{
	U16 reply_desc_type, host_tag = 0;
	U16 ioc_status = MPI3_IOCSTATUS_SUCCESS;
	U32 ioc_loginfo = 0;
	Mpi3StatusReplyDescriptor_t *status_desc = NULL;
	Mpi3AddressReplyDescriptor_t *addr_desc = NULL;
	Mpi3SuccessReplyDescriptor_t *success_desc = NULL;
	Mpi3SCSIIOReply_t *scsi_reply = NULL;
	U8 *sense_buf = NULL;
	U8 scsi_state = 0, scsi_status = 0, sense_state = 0;
	U32 xfer_count = 0, sense_count =0, resp_data = 0;
	struct mpi3mr_cmd *cm = NULL;
	union ccb *ccb;
	struct ccb_scsiio *csio;
	struct mpi3mr_cam_softc *cam_sc;
	U32 target_id;
	U8 *scsi_cdb;
	struct mpi3mr_target *target = NULL;
	U32 ioc_pend_data_len = 0, tg_pend_data_len = 0, data_len_blks = 0;
	struct mpi3mr_throttle_group_info *tg = NULL;
	U8 throttle_enabled_dev = 0;
	static int ratelimit;

	*reply_dma = 0;
	reply_desc_type = reply_desc->ReplyFlags &
			    MPI3_REPLY_DESCRIPT_FLAGS_TYPE_MASK;
	switch (reply_desc_type) {
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_STATUS:
		status_desc = (Mpi3StatusReplyDescriptor_t *)reply_desc;
		host_tag = status_desc->HostTag;
		ioc_status = status_desc->IOCStatus;
		if (ioc_status &
		    MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_LOGINFOAVAIL)
			ioc_loginfo = status_desc->IOCLogInfo;
		ioc_status &= MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_STATUS_MASK;
		break;
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_ADDRESS_REPLY:
		addr_desc = (Mpi3AddressReplyDescriptor_t *)reply_desc;
		*reply_dma = addr_desc->ReplyFrameAddress;
		scsi_reply = mpi3mr_get_reply_virt_addr(sc,
		    *reply_dma);
		if (scsi_reply == NULL) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "scsi_reply is NULL, "
			    "this shouldn't happen, reply_desc: %p\n",
			    reply_desc);
			goto out;
		}

		host_tag = scsi_reply->HostTag;
		ioc_status = scsi_reply->IOCStatus;
		scsi_status = scsi_reply->SCSIStatus;
		scsi_state = scsi_reply->SCSIState;
		sense_state = (scsi_state & MPI3_SCSI_STATE_SENSE_MASK);
		xfer_count = scsi_reply->TransferCount;
		sense_count = scsi_reply->SenseCount;
		resp_data = scsi_reply->ResponseData;
		sense_buf = mpi3mr_get_sensebuf_virt_addr(sc,
		    scsi_reply->SenseDataBufferAddress);
		if (ioc_status &
		    MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_LOGINFOAVAIL)
			ioc_loginfo = scsi_reply->IOCLogInfo;
		ioc_status &= MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_STATUS_MASK;
		if (sense_state == MPI3_SCSI_STATE_SENSE_BUFF_Q_EMPTY)
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Ran out of sense buffers\n");

		break;
	case MPI3_REPLY_DESCRIPT_FLAGS_TYPE_SUCCESS:
		success_desc = (Mpi3SuccessReplyDescriptor_t *)reply_desc;
		host_tag = success_desc->HostTag;

	default:
		break;
	}
	
	cm = sc->cmd_list[host_tag];

	if (cm->state == MPI3MR_CMD_STATE_FREE)
		goto out;

	cam_sc = sc->cam_sc;
	ccb = cm->ccb;
	csio = &ccb->csio;
	target_id = csio->ccb_h.target_id;

	scsi_cdb = scsiio_cdb_ptr(csio);

	target = mpi3mr_find_target_by_per_id(cam_sc, target_id);
	if (sc->iot_enable) {
		data_len_blks = csio->dxfer_len >> 9;
		
		if (target) {
			tg = target->throttle_group;
			throttle_enabled_dev =
				target->io_throttle_enabled;
		}

		if ((data_len_blks >= sc->io_throttle_data_length) &&
		     throttle_enabled_dev) {
			mpi3mr_atomic_sub(&sc->pend_large_data_sz, data_len_blks);
			ioc_pend_data_len = mpi3mr_atomic_read(
			    &sc->pend_large_data_sz);
			if (tg) {
				mpi3mr_atomic_sub(&tg->pend_large_data_sz,
					data_len_blks);
				tg_pend_data_len = mpi3mr_atomic_read(&tg->pend_large_data_sz);
				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"large vd_io completion persist_id(%d), handle(0x%04x), data_len(%d),"
						"ioc_pending(%d), tg_pending(%d), ioc_low(%d), tg_low(%d)\n",
						    target->per_id,
						    target->dev_handle,
						    data_len_blks, ioc_pend_data_len,
						    tg_pend_data_len,
						    sc->io_throttle_low,
						    tg->low);
					ratelimit++;
				}
				if (tg->io_divert  && ((ioc_pend_data_len <=
				    sc->io_throttle_low) &&
				    (tg_pend_data_len <= tg->low))) {
					tg->io_divert = 0;
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"VD: Coming out of divert perst_id(%d) tg_id(%d)\n",
						target->per_id, tg->id);
					mpi3mr_set_io_divert_for_all_vd_in_tg(
					    sc, tg, 0);
				}
			} else {
				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
					    "large pd_io completion persist_id(%d), handle(0x%04x), data_len(%d), ioc_pending(%d), ioc_low(%d)\n",
					    target->per_id,
					    target->dev_handle,
					    data_len_blks, ioc_pend_data_len,
					    sc->io_throttle_low);
					ratelimit++;
				}

				if (ioc_pend_data_len <= sc->io_throttle_low) {
					target->io_divert = 0;
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"PD: Coming out of divert perst_id(%d)\n",
						target->per_id);
				}
			}
			
			} else if (target->io_divert) {
			ioc_pend_data_len = mpi3mr_atomic_read(&sc->pend_large_data_sz);
			if (!tg) {
				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
					    "pd_io completion persist_id(%d), handle(0x%04x), data_len(%d), ioc_pending(%d), ioc_low(%d)\n",
					    target->per_id,
					    target->dev_handle,
					    data_len_blks, ioc_pend_data_len,
					    sc->io_throttle_low);
					ratelimit++;
				}

				if ( ioc_pend_data_len <= sc->io_throttle_low) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"PD: Coming out of divert perst_id(%d)\n",
						target->per_id);
					target->io_divert = 0;
				}

			} else if (ioc_pend_data_len <= sc->io_throttle_low) {
				tg_pend_data_len = mpi3mr_atomic_read(&tg->pend_large_data_sz);
				if (ratelimit % 1000) {
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"vd_io completion persist_id(%d), handle(0x%04x), data_len(%d),"
						"ioc_pending(%d), tg_pending(%d), ioc_low(%d), tg_low(%d)\n",
						    target->per_id,
						    target->dev_handle,
						    data_len_blks, ioc_pend_data_len,
						    tg_pend_data_len,
						    sc->io_throttle_low,
						    tg->low);
					ratelimit++;
				}
				if (tg->io_divert  && (tg_pend_data_len <= tg->low)) {
					tg->io_divert = 0;
					mpi3mr_dprint(sc, MPI3MR_IOT,
						"VD: Coming out of divert perst_id(%d) tg_id(%d)\n",
						target->per_id, tg->id);
					mpi3mr_set_io_divert_for_all_vd_in_tg(
					    sc, tg, 0);
				}

			}
		}
	}

	if (success_desc) {
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		goto out_success;
	}

	if (ioc_status == MPI3_IOCSTATUS_SCSI_DATA_UNDERRUN
	    && xfer_count == 0 && (scsi_status == MPI3_SCSI_STATUS_BUSY ||
	    scsi_status == MPI3_SCSI_STATUS_RESERVATION_CONFLICT ||
	    scsi_status == MPI3_SCSI_STATUS_TASK_SET_FULL))
		ioc_status = MPI3_IOCSTATUS_SUCCESS;

	if ((sense_state == MPI3_SCSI_STATE_SENSE_VALID) && sense_count
	    && sense_buf) {
		int sense_len, returned_sense_len;

		returned_sense_len = min(le32toh(sense_count),
		    sizeof(struct scsi_sense_data));
		if (returned_sense_len < csio->sense_len)
			csio->sense_resid = csio->sense_len -
			    returned_sense_len;
		else
			csio->sense_resid = 0;

		sense_len = min(returned_sense_len,
		    csio->sense_len - csio->sense_resid);
		bzero(&csio->sense_data, sizeof(csio->sense_data));
		bcopy(sense_buf, &csio->sense_data, sense_len);
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}

	switch (ioc_status) {
	case MPI3_IOCSTATUS_BUSY:
	case MPI3_IOCSTATUS_INSUFFICIENT_RESOURCES:
		mpi3mr_set_ccbstatus(ccb, CAM_REQUEUE_REQ);
		break;
	case MPI3_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		/*
		 * If devinfo is 0 this will be a volume.  In that case don't
		 * tell CAM that the volume is not there.  We want volumes to
		 * be enumerated until they are deleted/removed, not just
		 * failed.
		 */
		if (cm->targ->devinfo == 0)
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		else
			mpi3mr_set_ccbstatus(ccb, CAM_DEV_NOT_THERE);
		break;
	case MPI3_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI3_IOCSTATUS_SCSI_IOC_TERMINATED:
	case MPI3_IOCSTATUS_SCSI_EXT_TERMINATED:
		mpi3mr_set_ccbstatus(ccb, CAM_SCSI_BUSY);
		mpi3mr_dprint(sc, MPI3MR_TRACE,
		    "func: %s line:%d tgt %u Hosttag %u loginfo %x\n",
		    __func__, __LINE__,
		    target_id, cm->hosttag,
		    le32toh(scsi_reply->IOCLogInfo));
		mpi3mr_dprint(sc, MPI3MR_TRACE,
		    "SCSIStatus %x SCSIState %x xfercount %u\n",
		    scsi_reply->SCSIStatus, scsi_reply->SCSIState,
		    le32toh(xfer_count));
		break;
	case MPI3_IOCSTATUS_SCSI_DATA_OVERRUN:
		/* resid is ignored for this condition */
		csio->resid = 0;
		mpi3mr_set_ccbstatus(ccb, CAM_DATA_RUN_ERR);
		break;
	case MPI3_IOCSTATUS_SCSI_DATA_UNDERRUN:
		csio->resid = cm->length - le32toh(xfer_count);
	case MPI3_IOCSTATUS_SCSI_RECOVERED_ERROR:
	case MPI3_IOCSTATUS_SUCCESS:
		if ((scsi_reply->IOCStatus & MPI3_REPLY_DESCRIPT_STATUS_IOCSTATUS_STATUS_MASK) ==
		    MPI3_IOCSTATUS_SCSI_RECOVERED_ERROR)
			mpi3mr_dprint(sc, MPI3MR_XINFO, "func: %s line: %d recovered error\n",  __func__, __LINE__);

		/* Completion failed at the transport level. */
		if (scsi_reply->SCSIState & (MPI3_SCSI_STATE_NO_SCSI_STATUS |
		    MPI3_SCSI_STATE_TERMINATED)) {
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP_ERR);
			break;
		}

		/* In a modern packetized environment, an autosense failure
		 * implies that there's not much else that can be done to
		 * recover the command.
		 */
		if (scsi_reply->SCSIState & MPI3_SCSI_STATE_SENSE_VALID) {
			mpi3mr_set_ccbstatus(ccb, CAM_AUTOSENSE_FAIL);
			break;
		}

		/*
		 * Intentionally override the normal SCSI status reporting
		 * for these two cases.  These are likely to happen in a
		 * multi-initiator environment, and we want to make sure that
		 * CAM retries these commands rather than fail them.
		 */
		if ((scsi_reply->SCSIStatus == MPI3_SCSI_STATUS_COMMAND_TERMINATED) ||
		    (scsi_reply->SCSIStatus == MPI3_SCSI_STATUS_TASK_ABORTED)) {
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_ABORTED);
			break;
		}

		/* Handle normal status and sense */
		csio->scsi_status = scsi_reply->SCSIStatus;
		if (scsi_reply->SCSIStatus == MPI3_SCSI_STATUS_GOOD)
			mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP);
		else
			mpi3mr_set_ccbstatus(ccb, CAM_SCSI_STATUS_ERROR);

		if (scsi_reply->SCSIState & MPI3_SCSI_STATE_SENSE_VALID) {
			int sense_len, returned_sense_len;

			returned_sense_len = min(le32toh(scsi_reply->SenseCount),
			    sizeof(struct scsi_sense_data));
			if (returned_sense_len < csio->sense_len)
				csio->sense_resid = csio->sense_len -
				    returned_sense_len;
			else
				csio->sense_resid = 0;

			sense_len = min(returned_sense_len,
			    csio->sense_len - csio->sense_resid);
			bzero(&csio->sense_data, sizeof(csio->sense_data));
			bcopy(cm->sense, &csio->sense_data, sense_len);
			ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
		}

		break;
	case MPI3_IOCSTATUS_INVALID_SGL:
		mpi3mr_set_ccbstatus(ccb, CAM_UNREC_HBA_ERROR);
		break;
	case MPI3_IOCSTATUS_EEDP_GUARD_ERROR:
	case MPI3_IOCSTATUS_EEDP_REF_TAG_ERROR:
	case MPI3_IOCSTATUS_EEDP_APP_TAG_ERROR:
	case MPI3_IOCSTATUS_SCSI_PROTOCOL_ERROR:
	case MPI3_IOCSTATUS_INVALID_FUNCTION:
	case MPI3_IOCSTATUS_INTERNAL_ERROR:
	case MPI3_IOCSTATUS_INVALID_FIELD:
	case MPI3_IOCSTATUS_INVALID_STATE:
	case MPI3_IOCSTATUS_SCSI_IO_DATA_ERROR:
	case MPI3_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
	case MPI3_IOCSTATUS_INSUFFICIENT_POWER:
	case MPI3_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
	default:
		csio->resid = cm->length;
		mpi3mr_set_ccbstatus(ccb, CAM_REQ_CMP_ERR);
		break;
	}

out_success:
	if (mpi3mr_get_ccbstatus(ccb) != CAM_REQ_CMP) {
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/ 1);
	}

	mpi3mr_atomic_dec(&cm->targ->outstanding);
	mpi3mr_cmd_done(sc, cm);
	mpi3mr_dprint(sc, MPI3MR_TRACE, "Completion IO path :"
		" cdb[0]: %x targetid: 0x%x SMID: %x ioc_status: 0x%x ioc_loginfo: 0x%x scsi_status: 0x%x "
		"scsi_state: 0x%x response_data: 0x%x\n", scsi_cdb[0], target_id, host_tag,
		ioc_status, ioc_loginfo, scsi_status, scsi_state, resp_data);
	mpi3mr_atomic_dec(&sc->fw_outstanding);
out:

	if (sense_buf)
		mpi3mr_repost_sense_buf(sc,
		    scsi_reply->SenseDataBufferAddress);
	return;
}

/*
 * mpi3mr_complete_io_cmd:	ISR routine for IO commands
 * @sc:				Adapter's soft instance
 * @irq_ctx:			Driver's internal per IRQ structure
 *
 * This function processes IO command completions.
 */
int mpi3mr_complete_io_cmd(struct mpi3mr_softc *sc,
    struct mpi3mr_irq_context *irq_ctx)
{
	struct mpi3mr_op_reply_queue *op_reply_q = irq_ctx->op_reply_q;
	U32 exp_phase = op_reply_q->ephase;
	U32 reply_ci = op_reply_q->ci;
	U32 num_op_replies = 0;
	U64 reply_dma = 0;
	Mpi3DefaultReplyDescriptor_t *reply_desc;
	U16 req_qid = 0;

	mtx_lock_spin(&op_reply_q->q_lock);
	if (op_reply_q->in_use == false) {
		op_reply_q->in_use = true;
		mtx_unlock_spin(&op_reply_q->q_lock);
	} else {
		mtx_unlock_spin(&op_reply_q->q_lock);
		return 0;
	}
	
	reply_desc = (Mpi3DefaultReplyDescriptor_t *)op_reply_q->q_base + reply_ci;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "[QID:%d]:reply_desc: (%pa) reply_ci: %x"
		" reply_desc->ReplyFlags: 0x%x\n"
		"reply_q_base_phys: %#016jx reply_q_base: (%pa) exp_phase: %x\n",
		op_reply_q->qid, reply_desc, reply_ci, reply_desc->ReplyFlags, op_reply_q->q_base_phys,
		op_reply_q->q_base, exp_phase);

	if (((reply_desc->ReplyFlags &
	     MPI3_REPLY_DESCRIPT_FLAGS_PHASE_MASK) != exp_phase) || !op_reply_q->qid) {
		mtx_lock_spin(&op_reply_q->q_lock);
		op_reply_q->in_use = false;
		mtx_unlock_spin(&op_reply_q->q_lock);
		return 0;
	}

	do {
		req_qid = reply_desc->RequestQueueID;
		sc->op_req_q[req_qid - 1].ci =
		    reply_desc->RequestQueueCI;

		mpi3mr_process_op_reply_desc(sc, reply_desc, &reply_dma);
		mpi3mr_atomic_dec(&op_reply_q->pend_ios);
		if (reply_dma)
			mpi3mr_repost_reply_buf(sc, reply_dma);
		num_op_replies++;
		if (++reply_ci == op_reply_q->num_replies) {
			reply_ci = 0;
			exp_phase ^= 1;
		}
		reply_desc =
		    (Mpi3DefaultReplyDescriptor_t *)op_reply_q->q_base + reply_ci;
		if ((reply_desc->ReplyFlags &
		     MPI3_REPLY_DESCRIPT_FLAGS_PHASE_MASK) != exp_phase)
			break;
	} while (1);


	mpi3mr_regwrite(sc, MPI3_SYSIF_OPER_REPLY_Q_N_CI_OFFSET(op_reply_q->qid), reply_ci);
	op_reply_q->ci = reply_ci;
	op_reply_q->ephase = exp_phase;
	mtx_lock_spin(&op_reply_q->q_lock);
	op_reply_q->in_use = false;
	mtx_unlock_spin(&op_reply_q->q_lock);
	return num_op_replies;
}

/*
 * mpi3mr_isr:			Primary ISR function
 * privdata:			Driver's internal per IRQ structure
 *
 * This is driver's primary ISR function which is being called whenever any admin/IO
 * command completion.
 */
void mpi3mr_isr(void *privdata)
{
	struct mpi3mr_irq_context *irq_ctx = (struct mpi3mr_irq_context *)privdata;
	struct mpi3mr_softc *sc = irq_ctx->sc;
	U16 msi_idx;

	if (!irq_ctx)
		return;

	msi_idx = irq_ctx->msix_index;

	if (!sc->intr_enabled)
		return;

	if (!msi_idx)
		mpi3mr_complete_admin_cmd(sc);

	if (irq_ctx->op_reply_q && irq_ctx->op_reply_q->qid) {
		mpi3mr_complete_io_cmd(sc, irq_ctx);
	}
}

/*
 * mpi3mr_alloc_requests - Allocates host commands
 * @sc: Adapter reference
 *
 * This function allocates controller supported host commands
 *
 * Return: 0 on success and proper error codes on failure
 */
int
mpi3mr_alloc_requests(struct mpi3mr_softc *sc)
{
	struct mpi3mr_cmd *cmd;
	int i, j, nsegs, ret;
	
	nsegs = MPI3MR_SG_DEPTH;
	ret = bus_dma_tag_create( sc->mpi3mr_parent_dmat,    /* parent */
				1, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE,	/* maxsize */
                                nsegs,			/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
                                BUS_DMA_ALLOCNOW,	/* flags */
                                busdma_lock_mutex,	/* lockfunc */
				&sc->io_lock,	/* lockarg */
				&sc->buffer_dmat);
	if (ret) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate buffer DMA tag ret: %d\n", ret);
		return (ENOMEM);
        }

	/*
	 * sc->cmd_list is an array of struct mpi3mr_cmd pointers.
	 * Allocate the dynamic array first and then allocate individual
	 * commands.
	 */
	sc->cmd_list = malloc(sizeof(struct mpi3mr_cmd *) * sc->max_host_ios,
	    M_MPI3MR, M_NOWAIT | M_ZERO);
	
	if (!sc->cmd_list) {
		device_printf(sc->mpi3mr_dev, "Cannot alloc memory for mpt_cmd_list.\n");
		return (ENOMEM);
	}
	
	for (i = 0; i < sc->max_host_ios; i++) {
		sc->cmd_list[i] = malloc(sizeof(struct mpi3mr_cmd),
		    M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!sc->cmd_list[i]) {
			for (j = 0; j < i; j++)
				free(sc->cmd_list[j], M_MPI3MR);
			free(sc->cmd_list, M_MPI3MR);
			sc->cmd_list = NULL;
			return (ENOMEM);
		}
	}

	for (i = 1; i < sc->max_host_ios; i++) {
		cmd = sc->cmd_list[i];
		cmd->hosttag = i;
		cmd->sc = sc;
		cmd->state = MPI3MR_CMD_STATE_BUSY;
		callout_init_mtx(&cmd->callout, &sc->mpi3mr_mtx, 0);
		cmd->ccb = NULL;
		TAILQ_INSERT_TAIL(&(sc->cmd_list_head), cmd, next);
		if (bus_dmamap_create(sc->buffer_dmat, 0, &cmd->dmamap))
			return ENOMEM;
	}
	return (0);
}

/*
 * mpi3mr_get_command:		Get a coomand structure from free command pool
 * @sc:				Adapter soft instance
 * Return:			MPT command reference	
 *
 * This function returns an MPT command to the caller.
 */
struct mpi3mr_cmd *
mpi3mr_get_command(struct mpi3mr_softc *sc)
{
	struct mpi3mr_cmd *cmd = NULL;

	mtx_lock(&sc->cmd_pool_lock);
	if (!TAILQ_EMPTY(&sc->cmd_list_head)) {
		cmd = TAILQ_FIRST(&sc->cmd_list_head);
		TAILQ_REMOVE(&sc->cmd_list_head, cmd, next);
	} else {
		goto out;
	}

	mpi3mr_dprint(sc, MPI3MR_TRACE, "Get command SMID: 0x%x\n", cmd->hosttag);

	memset((uint8_t *)&cmd->io_request, 0, MPI3MR_AREQ_FRAME_SZ);
	cmd->data_dir = 0;
	cmd->ccb = NULL;
	cmd->targ = NULL;
	cmd->state = MPI3MR_CMD_STATE_BUSY;
	cmd->data = NULL;
	cmd->length = 0;
out:
	mtx_unlock(&sc->cmd_pool_lock);
	return cmd;
}

/*
 * mpi3mr_release_command:	Return a cmd to free command pool
 * input:			Command packet for return to free command pool
 *
 * This function returns an MPT command to the free command list.
 */
void
mpi3mr_release_command(struct mpi3mr_cmd *cmd)
{
	struct mpi3mr_softc *sc = cmd->sc;

	mtx_lock(&sc->cmd_pool_lock);
	TAILQ_INSERT_HEAD(&(sc->cmd_list_head), cmd, next);
	cmd->state = MPI3MR_CMD_STATE_FREE;
	cmd->req_qidx = 0;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "Release command SMID: 0x%x\n", cmd->hosttag);
	mtx_unlock(&sc->cmd_pool_lock);

	return;
}

 /**
 * mpi3mr_free_ioctl_dma_memory - free memory for ioctl dma
 * @sc: Adapter instance reference
 *
 * Free the DMA memory allocated for IOCTL handling purpose.
 *
 * Return: None
 */
static void mpi3mr_free_ioctl_dma_memory(struct mpi3mr_softc *sc)
{
	U16 i;
	struct dma_memory_desc *mem_desc;
	
	for (i=0; i<MPI3MR_NUM_IOCTL_SGE; i++) {
		mem_desc = &sc->ioctl_sge[i];
		if (mem_desc->addr && mem_desc->dma_addr) {
			bus_dmamap_unload(mem_desc->tag, mem_desc->dmamap);
			bus_dmamem_free(mem_desc->tag, mem_desc->addr, mem_desc->dmamap);
			mem_desc->addr = NULL;
			if (mem_desc->tag != NULL)
				bus_dma_tag_destroy(mem_desc->tag);
		}
	}

	mem_desc = &sc->ioctl_chain_sge;
	if (mem_desc->addr && mem_desc->dma_addr) {
		bus_dmamap_unload(mem_desc->tag, mem_desc->dmamap);
		bus_dmamem_free(mem_desc->tag, mem_desc->addr, mem_desc->dmamap);
		mem_desc->addr = NULL;
		if (mem_desc->tag != NULL)
			bus_dma_tag_destroy(mem_desc->tag);
	}
	
	mem_desc = &sc->ioctl_resp_sge;
	if (mem_desc->addr && mem_desc->dma_addr) {
		bus_dmamap_unload(mem_desc->tag, mem_desc->dmamap);
		bus_dmamem_free(mem_desc->tag, mem_desc->addr, mem_desc->dmamap);
		mem_desc->addr = NULL;
		if (mem_desc->tag != NULL)
			bus_dma_tag_destroy(mem_desc->tag);
	}
	
	sc->ioctl_sges_allocated = false;
}

/**
 * mpi3mr_alloc_ioctl_dma_memory - Alloc memory for ioctl dma
 * @sc: Adapter instance reference
 *
 * This function allocates dmaable memory required to handle the
 * application issued MPI3 IOCTL requests.
 *
 * Return: None
 */
void mpi3mr_alloc_ioctl_dma_memory(struct mpi3mr_softc *sc)
{
	struct dma_memory_desc *mem_desc;
	U16 i;

	for (i=0; i<MPI3MR_NUM_IOCTL_SGE; i++) {
		mem_desc = &sc->ioctl_sge[i];
		mem_desc->size = MPI3MR_IOCTL_SGE_SIZE;
		
		if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
					4, 0,			/* algnmnt, boundary */
					sc->dma_loaddr,		/* lowaddr */
					sc->dma_hiaddr,		/* highaddr */
					NULL, NULL,		/* filter, filterarg */
					mem_desc->size,		/* maxsize */
					1,			/* nsegments */
					mem_desc->size,		/* maxsegsize */
					0,			/* flags */
					NULL, NULL,		/* lockfunc, lockarg */
					&mem_desc->tag)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
			goto out_failed;
		}

		if (bus_dmamem_alloc(mem_desc->tag, (void **)&mem_desc->addr,
		    BUS_DMA_NOWAIT, &mem_desc->dmamap)) {
			mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
			goto out_failed;
		}
		bzero(mem_desc->addr, mem_desc->size);
		bus_dmamap_load(mem_desc->tag, mem_desc->dmamap, mem_desc->addr, mem_desc->size,
		    mpi3mr_memaddr_cb, &mem_desc->dma_addr, BUS_DMA_NOWAIT);

		if (!mem_desc->addr)
			goto out_failed;
	}

	mem_desc = &sc->ioctl_chain_sge;
	mem_desc->size = MPI3MR_4K_PGSZ;
	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				4, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				mem_desc->size,		/* maxsize */
				1,			/* nsegments */
				mem_desc->size,		/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&mem_desc->tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
		goto out_failed;
	}

	if (bus_dmamem_alloc(mem_desc->tag, (void **)&mem_desc->addr,
	    BUS_DMA_NOWAIT, &mem_desc->dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "%s: Cannot allocate replies memory\n", __func__);
		goto out_failed;
	}
	bzero(mem_desc->addr, mem_desc->size);
	bus_dmamap_load(mem_desc->tag, mem_desc->dmamap, mem_desc->addr, mem_desc->size,
	    mpi3mr_memaddr_cb, &mem_desc->dma_addr, BUS_DMA_NOWAIT);

	if (!mem_desc->addr)
		goto out_failed;

	mem_desc = &sc->ioctl_resp_sge;
	mem_desc->size = MPI3MR_4K_PGSZ;
	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,    /* parent */
				4, 0,			/* algnmnt, boundary */
				sc->dma_loaddr,		/* lowaddr */
				sc->dma_hiaddr,		/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				mem_desc->size,		/* maxsize */
				1,			/* nsegments */
				mem_desc->size,		/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&mem_desc->tag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate request DMA tag\n");
		goto out_failed;
	}

	if (bus_dmamem_alloc(mem_desc->tag, (void **)&mem_desc->addr,
	    BUS_DMA_NOWAIT, &mem_desc->dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate replies memory\n");
		goto out_failed;
	}
	bzero(mem_desc->addr, mem_desc->size);
	bus_dmamap_load(mem_desc->tag, mem_desc->dmamap, mem_desc->addr, mem_desc->size,
	    mpi3mr_memaddr_cb, &mem_desc->dma_addr, BUS_DMA_NOWAIT);

	if (!mem_desc->addr)
		goto out_failed;
	
	sc->ioctl_sges_allocated = true;

	return;
out_failed:
	printf("cannot allocate DMA memory for the mpt commands"
	    "  from the applications, application interface for MPT command is disabled\n");
	mpi3mr_free_ioctl_dma_memory(sc);
}

void
mpi3mr_destory_mtx(struct mpi3mr_softc *sc)
{
	int i;
	struct mpi3mr_op_req_queue *op_req_q;
	struct mpi3mr_op_reply_queue *op_reply_q;
	
	if (sc->admin_reply) {
		if (mtx_initialized(&sc->admin_reply_lock))
			mtx_destroy(&sc->admin_reply_lock);
	}

	if (sc->op_reply_q) {
		for(i = 0; i < sc->num_queues; i++) {
			op_reply_q = sc->op_reply_q + i;
			if (mtx_initialized(&op_reply_q->q_lock))
				mtx_destroy(&op_reply_q->q_lock);
		}
	}

	if (sc->op_req_q) {
		for(i = 0; i < sc->num_queues; i++) {
			op_req_q = sc->op_req_q + i;
			if (mtx_initialized(&op_req_q->q_lock))
				mtx_destroy(&op_req_q->q_lock);
		}
	}
	
	if (mtx_initialized(&sc->init_cmds.completion.lock))
		mtx_destroy(&sc->init_cmds.completion.lock);
	
	if (mtx_initialized(&sc->ioctl_cmds.completion.lock))
		mtx_destroy(&sc->ioctl_cmds.completion.lock);
	
	if (mtx_initialized(&sc->host_tm_cmds.completion.lock))
		mtx_destroy(&sc->host_tm_cmds.completion.lock);
	
	for (i = 0; i < MPI3MR_NUM_DEVRMCMD; i++) {
		if (mtx_initialized(&sc->dev_rmhs_cmds[i].completion.lock))
			mtx_destroy(&sc->dev_rmhs_cmds[i].completion.lock);
	}
	
	if (mtx_initialized(&sc->reset_mutex))
		mtx_destroy(&sc->reset_mutex);
	
	if (mtx_initialized(&sc->target_lock))
		mtx_destroy(&sc->target_lock);

	if (mtx_initialized(&sc->fwevt_lock))
		mtx_destroy(&sc->fwevt_lock);
	
	if (mtx_initialized(&sc->cmd_pool_lock))
		mtx_destroy(&sc->cmd_pool_lock);
	
	if (mtx_initialized(&sc->reply_free_q_lock))
		mtx_destroy(&sc->reply_free_q_lock);
	
	if (mtx_initialized(&sc->sense_buf_q_lock))
		mtx_destroy(&sc->sense_buf_q_lock);

	if (mtx_initialized(&sc->chain_buf_lock))
		mtx_destroy(&sc->chain_buf_lock);

	if (mtx_initialized(&sc->admin_req_lock))
		mtx_destroy(&sc->admin_req_lock);
	
	if (mtx_initialized(&sc->mpi3mr_mtx))
		mtx_destroy(&sc->mpi3mr_mtx);
}

/**
 * mpi3mr_free_mem - Freeup adapter level data structures
 * @sc: Adapter reference
 *
 * Return: Nothing.
 */
void
mpi3mr_free_mem(struct mpi3mr_softc *sc)
{
	int i;
	struct mpi3mr_op_req_queue *op_req_q;
	struct mpi3mr_op_reply_queue *op_reply_q;
	struct mpi3mr_irq_context *irq_ctx;
	
	if (sc->cmd_list) {
		for (i = 0; i < sc->max_host_ios; i++) {
			free(sc->cmd_list[i], M_MPI3MR);
		}
		free(sc->cmd_list, M_MPI3MR);
		sc->cmd_list = NULL;
	}

	if (sc->pel_seq_number && sc->pel_seq_number_dma) {
		bus_dmamap_unload(sc->pel_seq_num_dmatag, sc->pel_seq_num_dmamap);
		bus_dmamem_free(sc->pel_seq_num_dmatag, sc->pel_seq_number, sc->pel_seq_num_dmamap);
		sc->pel_seq_number = NULL;
		if (sc->pel_seq_num_dmatag != NULL)
			bus_dma_tag_destroy(sc->pel_seq_num_dmatag);
	}
	
	if (sc->throttle_groups) {
		free(sc->throttle_groups, M_MPI3MR);
		sc->throttle_groups = NULL;
	}

	/* Free up operational queues*/
	if (sc->op_req_q) {
		for (i = 0; i < sc->num_queues; i++) {
			op_req_q = sc->op_req_q + i;
			if (op_req_q->q_base && op_req_q->q_base_phys) {
				bus_dmamap_unload(op_req_q->q_base_tag, op_req_q->q_base_dmamap);
				bus_dmamem_free(op_req_q->q_base_tag, op_req_q->q_base, op_req_q->q_base_dmamap);
				op_req_q->q_base = NULL;
				if (op_req_q->q_base_tag != NULL)
					bus_dma_tag_destroy(op_req_q->q_base_tag);
			}
		}
		free(sc->op_req_q, M_MPI3MR);
		sc->op_req_q = NULL;
	}
	
	if (sc->op_reply_q) {
		for (i = 0; i < sc->num_queues; i++) {
			op_reply_q = sc->op_reply_q + i;
			if (op_reply_q->q_base && op_reply_q->q_base_phys) {
				bus_dmamap_unload(op_reply_q->q_base_tag, op_reply_q->q_base_dmamap);
				bus_dmamem_free(op_reply_q->q_base_tag, op_reply_q->q_base, op_reply_q->q_base_dmamap);
				op_reply_q->q_base = NULL;
				if (op_reply_q->q_base_tag != NULL)
					bus_dma_tag_destroy(op_reply_q->q_base_tag);
			}
		}
		free(sc->op_reply_q, M_MPI3MR);
		sc->op_reply_q = NULL;
	}
	
	/* Free up chain buffers*/
	if (sc->chain_sgl_list) {
		for (i = 0; i < sc->chain_buf_count; i++) {
			if (sc->chain_sgl_list[i].buf && sc->chain_sgl_list[i].buf_phys) {
				bus_dmamap_unload(sc->chain_sgl_list_tag, sc->chain_sgl_list[i].buf_dmamap);
				bus_dmamem_free(sc->chain_sgl_list_tag, sc->chain_sgl_list[i].buf,
						sc->chain_sgl_list[i].buf_dmamap);
				sc->chain_sgl_list[i].buf = NULL;
			}
		}
		if (sc->chain_sgl_list_tag != NULL)
			bus_dma_tag_destroy(sc->chain_sgl_list_tag);
		free(sc->chain_sgl_list, M_MPI3MR);
		sc->chain_sgl_list = NULL;
	}
	
	if (sc->chain_bitmap) {
		free(sc->chain_bitmap, M_MPI3MR);
		sc->chain_bitmap = NULL;
	}

	for (i = 0; i < sc->msix_count; i++) {
		irq_ctx = sc->irq_ctx + i;
		if (irq_ctx)
			irq_ctx->op_reply_q = NULL;
	}
	
	/* Free reply_buf_tag */
	if (sc->reply_buf && sc->reply_buf_phys) { 
		bus_dmamap_unload(sc->reply_buf_tag, sc->reply_buf_dmamap);
		bus_dmamem_free(sc->reply_buf_tag, sc->reply_buf,
				sc->reply_buf_dmamap);
		sc->reply_buf = NULL;
		if (sc->reply_buf_tag != NULL)
			bus_dma_tag_destroy(sc->reply_buf_tag);
	}
	
	/* Free reply_free_q_tag */
	if (sc->reply_free_q && sc->reply_free_q_phys) {
		bus_dmamap_unload(sc->reply_free_q_tag, sc->reply_free_q_dmamap);
		bus_dmamem_free(sc->reply_free_q_tag, sc->reply_free_q,
				sc->reply_free_q_dmamap);
		sc->reply_free_q = NULL;
		if (sc->reply_free_q_tag != NULL)
			bus_dma_tag_destroy(sc->reply_free_q_tag);
	}
	
	/* Free sense_buf_tag */
	if (sc->sense_buf && sc->sense_buf_phys) {
		bus_dmamap_unload(sc->sense_buf_tag, sc->sense_buf_dmamap);
		bus_dmamem_free(sc->sense_buf_tag, sc->sense_buf,
				sc->sense_buf_dmamap);
		sc->sense_buf = NULL;
		if (sc->sense_buf_tag != NULL)
			bus_dma_tag_destroy(sc->sense_buf_tag);
	}

	/* Free sense_buf_q_tag */
	if (sc->sense_buf_q && sc->sense_buf_q_phys) {
		bus_dmamap_unload(sc->sense_buf_q_tag, sc->sense_buf_q_dmamap);
		bus_dmamem_free(sc->sense_buf_q_tag, sc->sense_buf_q,
				sc->sense_buf_q_dmamap);
		sc->sense_buf_q = NULL;
		if (sc->sense_buf_q_tag != NULL)
			bus_dma_tag_destroy(sc->sense_buf_q_tag);
	}

	/* Free up internal(non-IO) commands*/
	if (sc->init_cmds.reply) {
		free(sc->init_cmds.reply, M_MPI3MR);
		sc->init_cmds.reply = NULL;
	}
	
	if (sc->ioctl_cmds.reply) {
		free(sc->ioctl_cmds.reply, M_MPI3MR);
		sc->ioctl_cmds.reply = NULL;
	}
	
	if (sc->pel_cmds.reply) {
		free(sc->pel_cmds.reply, M_MPI3MR);
		sc->pel_cmds.reply = NULL;
	}
	
	if (sc->pel_abort_cmd.reply) {
		free(sc->pel_abort_cmd.reply, M_MPI3MR);
		sc->pel_abort_cmd.reply = NULL;
	}
	
	if (sc->host_tm_cmds.reply) {
		free(sc->host_tm_cmds.reply, M_MPI3MR);
		sc->host_tm_cmds.reply = NULL;
	}
	
	if (sc->log_data_buffer) {
		free(sc->log_data_buffer, M_MPI3MR);
		sc->log_data_buffer = NULL;
	}

	for (i = 0; i < MPI3MR_NUM_DEVRMCMD; i++) {
		if (sc->dev_rmhs_cmds[i].reply) {
			free(sc->dev_rmhs_cmds[i].reply, M_MPI3MR);
			sc->dev_rmhs_cmds[i].reply = NULL;
		}
	}

	for (i = 0; i < MPI3MR_NUM_EVTACKCMD; i++) {
		if (sc->evtack_cmds[i].reply) {
			free(sc->evtack_cmds[i].reply, M_MPI3MR);
			sc->evtack_cmds[i].reply = NULL;
		}
	}

	if (sc->removepend_bitmap) {
		free(sc->removepend_bitmap, M_MPI3MR);
		sc->removepend_bitmap = NULL;
	}

	if (sc->devrem_bitmap) {
		free(sc->devrem_bitmap, M_MPI3MR);
		sc->devrem_bitmap = NULL;
	}

	if (sc->evtack_cmds_bitmap) {
		free(sc->evtack_cmds_bitmap, M_MPI3MR);
		sc->evtack_cmds_bitmap = NULL;
	}

	/* Free Admin reply*/
	if (sc->admin_reply && sc->admin_reply_phys) {
		bus_dmamap_unload(sc->admin_reply_tag, sc->admin_reply_dmamap);
		bus_dmamem_free(sc->admin_reply_tag, sc->admin_reply,
				sc->admin_reply_dmamap);
		sc->admin_reply = NULL;
		if (sc->admin_reply_tag != NULL)
			bus_dma_tag_destroy(sc->admin_reply_tag);
	}
	
	/* Free Admin request*/
	if (sc->admin_req && sc->admin_req_phys) {
		bus_dmamap_unload(sc->admin_req_tag, sc->admin_req_dmamap);
		bus_dmamem_free(sc->admin_req_tag, sc->admin_req,
				sc->admin_req_dmamap);
		sc->admin_req = NULL;
		if (sc->admin_req_tag != NULL)
			bus_dma_tag_destroy(sc->admin_req_tag);
	}
	mpi3mr_free_ioctl_dma_memory(sc);

}

/**
 * mpi3mr_drv_cmd_comp_reset - Flush a internal driver command
 * @sc: Adapter instance reference
 * @cmdptr: Internal command tracker
 *
 * Complete an internal driver commands with state indicating it
 * is completed due to reset.
 *
 * Return: Nothing.
 */
static inline void mpi3mr_drv_cmd_comp_reset(struct mpi3mr_softc *sc,
	struct mpi3mr_drvr_cmd *cmdptr)
{
	if (cmdptr->state & MPI3MR_CMD_PENDING) {
		cmdptr->state |= MPI3MR_CMD_RESET;
		cmdptr->state &= ~MPI3MR_CMD_PENDING;
		if (cmdptr->is_waiting) {
			complete(&cmdptr->completion);
			cmdptr->is_waiting = 0;
		} else if (cmdptr->callback)
			cmdptr->callback(sc, cmdptr);
	}
}

/**
 * mpi3mr_flush_drv_cmds - Flush internal driver commands
 * @sc: Adapter instance reference
 *
 * Flush all internal driver commands post reset
 *
 * Return: Nothing.
 */
static void mpi3mr_flush_drv_cmds(struct mpi3mr_softc *sc)
{
	int i = 0;
	struct mpi3mr_drvr_cmd *cmdptr;

	cmdptr = &sc->init_cmds;
	mpi3mr_drv_cmd_comp_reset(sc, cmdptr);

	cmdptr = &sc->ioctl_cmds;
	mpi3mr_drv_cmd_comp_reset(sc, cmdptr);

	cmdptr = &sc->host_tm_cmds;
	mpi3mr_drv_cmd_comp_reset(sc, cmdptr);

	for (i = 0; i < MPI3MR_NUM_DEVRMCMD; i++) {
		cmdptr = &sc->dev_rmhs_cmds[i];
		mpi3mr_drv_cmd_comp_reset(sc, cmdptr);
	}

	for (i = 0; i < MPI3MR_NUM_EVTACKCMD; i++) {
		cmdptr = &sc->evtack_cmds[i];
		mpi3mr_drv_cmd_comp_reset(sc, cmdptr);
	}

	cmdptr = &sc->pel_cmds;
	mpi3mr_drv_cmd_comp_reset(sc, cmdptr);

	cmdptr = &sc->pel_abort_cmd;
	mpi3mr_drv_cmd_comp_reset(sc, cmdptr);
}


/**
 * mpi3mr_memset_buffers - memset memory for a controller
 * @sc: Adapter instance reference
 *
 * clear all the memory allocated for a controller, typically
 * called post reset to reuse the memory allocated during the
 * controller init.
 *
 * Return: Nothing.
 */
static void mpi3mr_memset_buffers(struct mpi3mr_softc *sc)
{
	U16 i;
	struct mpi3mr_throttle_group_info *tg;

	memset(sc->admin_req, 0, sc->admin_req_q_sz);
	memset(sc->admin_reply, 0, sc->admin_reply_q_sz);

	memset(sc->init_cmds.reply, 0, sc->reply_sz);
	memset(sc->ioctl_cmds.reply, 0, sc->reply_sz);
	memset(sc->host_tm_cmds.reply, 0, sc->reply_sz);
	memset(sc->pel_cmds.reply, 0, sc->reply_sz);
	memset(sc->pel_abort_cmd.reply, 0, sc->reply_sz);
	for (i = 0; i < MPI3MR_NUM_DEVRMCMD; i++)
		memset(sc->dev_rmhs_cmds[i].reply, 0, sc->reply_sz);
	for (i = 0; i < MPI3MR_NUM_EVTACKCMD; i++)
		memset(sc->evtack_cmds[i].reply, 0, sc->reply_sz);
	memset(sc->removepend_bitmap, 0, sc->dev_handle_bitmap_sz);
	memset(sc->devrem_bitmap, 0, sc->devrem_bitmap_sz);
	memset(sc->evtack_cmds_bitmap, 0, sc->evtack_cmds_bitmap_sz);

	for (i = 0; i < sc->num_queues; i++) {
		sc->op_reply_q[i].qid = 0;
		sc->op_reply_q[i].ci = 0;
		sc->op_reply_q[i].num_replies = 0;
		sc->op_reply_q[i].ephase = 0;
		mpi3mr_atomic_set(&sc->op_reply_q[i].pend_ios, 0);
		memset(sc->op_reply_q[i].q_base, 0, sc->op_reply_q[i].qsz);

		sc->op_req_q[i].ci = 0;
		sc->op_req_q[i].pi = 0;
		sc->op_req_q[i].num_reqs = 0;
		sc->op_req_q[i].qid = 0;
		sc->op_req_q[i].reply_qid = 0;
		memset(sc->op_req_q[i].q_base, 0, sc->op_req_q[i].qsz);
	}

	mpi3mr_atomic_set(&sc->pend_large_data_sz, 0);
	if (sc->throttle_groups) {
		tg = sc->throttle_groups;
		for (i = 0; i < sc->num_io_throttle_group; i++, tg++) {
			tg->id = 0;
			tg->fw_qd = 0;
			tg->modified_qd = 0;
			tg->io_divert= 0;
			tg->high = 0;
			tg->low = 0;
			mpi3mr_atomic_set(&tg->pend_large_data_sz, 0);
		}
 	}
}

/**
 * mpi3mr_invalidate_devhandles -Invalidate device handles
 * @sc: Adapter instance reference
 *
 * Invalidate the device handles in the target device structures
 * . Called post reset prior to reinitializing the controller.
 *
 * Return: Nothing.
 */
static void mpi3mr_invalidate_devhandles(struct mpi3mr_softc *sc)
{
	struct mpi3mr_target *target = NULL;

	mtx_lock_spin(&sc->target_lock);
	TAILQ_FOREACH(target, &sc->cam_sc->tgt_list, tgt_next) {
		if (target) {
			target->dev_handle = MPI3MR_INVALID_DEV_HANDLE;
			target->io_throttle_enabled = 0;
			target->io_divert = 0;
			target->throttle_group = NULL;
		}
	}
	mtx_unlock_spin(&sc->target_lock);
}

/**
 * mpi3mr_rfresh_tgtdevs - Refresh target device exposure
 * @sc: Adapter instance reference
 *
 * This is executed post controller reset to identify any
 * missing devices during reset and remove from the upper layers
 * or expose any newly detected device to the upper layers.
 *
 * Return: Nothing.
 */

static void mpi3mr_rfresh_tgtdevs(struct mpi3mr_softc *sc)
{
	struct mpi3mr_target *target = NULL;
	struct mpi3mr_target *target_temp = NULL;

	TAILQ_FOREACH_SAFE(target, &sc->cam_sc->tgt_list, tgt_next, target_temp) {
		if (target->dev_handle == MPI3MR_INVALID_DEV_HANDLE) {
			if (target->exposed_to_os)
				mpi3mr_remove_device_from_os(sc, target->dev_handle);
			mpi3mr_remove_device_from_list(sc, target, true);
		}
	}

	TAILQ_FOREACH(target, &sc->cam_sc->tgt_list, tgt_next) {
		if ((target->dev_handle != MPI3MR_INVALID_DEV_HANDLE) &&
		    !target->is_hidden && !target->exposed_to_os) {
			mpi3mr_add_device(sc, target->per_id);
		}
	}

}

static void mpi3mr_flush_io(struct mpi3mr_softc *sc)
{
	int i;
	struct mpi3mr_cmd *cmd = NULL;
	union ccb *ccb = NULL;

	for (i = 0; i < sc->max_host_ios; i++) {
		cmd = sc->cmd_list[i];

		if (cmd && cmd->ccb) {
			if (cmd->callout_owner) {
				ccb = (union ccb *)(cmd->ccb);
				ccb->ccb_h.status = CAM_SCSI_BUS_RESET;
				mpi3mr_cmd_done(sc, cmd);
			} else {
				cmd->ccb = NULL;
				mpi3mr_release_command(cmd);
			}
		}
	}
}
/**
 * mpi3mr_clear_reset_history - Clear reset history
 * @sc: Adapter instance reference
 *
 * Write the reset history bit in IOC Status to clear the bit,
 * if it is already set.
 *
 * Return: Nothing.
 */
static inline void mpi3mr_clear_reset_history(struct mpi3mr_softc *sc)
{
	U32 ioc_status;

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	if (ioc_status & MPI3_SYSIF_IOC_STATUS_RESET_HISTORY)
		mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_STATUS_OFFSET, ioc_status);
}

/**
 * mpi3mr_set_diagsave - Set diag save bit for snapdump
 * @sc: Adapter reference
 *
 * Set diag save bit in IOC configuration register to enable
 * snapdump.
 *
 * Return: Nothing.
 */
static inline void mpi3mr_set_diagsave(struct mpi3mr_softc *sc)
{
	U32 ioc_config;

	ioc_config =
	    mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
	ioc_config |= MPI3_SYSIF_IOC_CONFIG_DIAG_SAVE;
	mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET, ioc_config);
}

/**
 * mpi3mr_issue_reset - Issue reset to the controller
 * @sc: Adapter reference
 * @reset_type: Reset type
 * @reset_reason: Reset reason code
 *
 * Unlock the host diagnostic registers and write the specific
 * reset type to that, wait for reset acknowledgement from the
 * controller, if the reset is not successful retry for the
 * predefined number of times.
 *
 * Return: 0 on success, non-zero on failure.
 */
static int mpi3mr_issue_reset(struct mpi3mr_softc *sc, U16 reset_type,
	U32 reset_reason)
{
	int retval = -1;
	U8 unlock_retry_count = 0;
	U32 host_diagnostic, ioc_status, ioc_config;
	U32 timeout = MPI3MR_RESET_ACK_TIMEOUT * 10;

	if ((reset_type != MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET) &&
	    (reset_type != MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT))
		return retval;
	if (sc->unrecoverable)
		return retval;
	
	if (reset_reason == MPI3MR_RESET_FROM_FIRMWARE) {
		retval = 0;
		return retval;
	}
	
	mpi3mr_dprint(sc, MPI3MR_INFO, "%s reset due to %s(0x%x)\n",
	    mpi3mr_reset_type_name(reset_type),
	    mpi3mr_reset_rc_name(reset_reason), reset_reason);
	
	mpi3mr_clear_reset_history(sc);
	do {
		mpi3mr_dprint(sc, MPI3MR_INFO,
		    "Write magic sequence to unlock host diag register (retry=%d)\n",
		    ++unlock_retry_count);
		if (unlock_retry_count >= MPI3MR_HOSTDIAG_UNLOCK_RETRY_COUNT) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "%s reset failed! due to host diag register unlock failure"
			    "host_diagnostic(0x%08x)\n", mpi3mr_reset_type_name(reset_type),
			    host_diagnostic);
			sc->unrecoverable = 1;
			return retval;
		}

		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_FLUSH);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_1ST);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_2ND);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_3RD);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_4TH);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_5TH);
		mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET,
			MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_6TH);

		DELAY(1000); /* delay in usec */
		host_diagnostic = mpi3mr_regread(sc, MPI3_SYSIF_HOST_DIAG_OFFSET);
		mpi3mr_dprint(sc, MPI3MR_INFO,
		    "wrote magic sequence: retry_count(%d), host_diagnostic(0x%08x)\n",
		    unlock_retry_count, host_diagnostic);
	} while (!(host_diagnostic & MPI3_SYSIF_HOST_DIAG_DIAG_WRITE_ENABLE));

	mpi3mr_regwrite(sc, MPI3_SYSIF_SCRATCHPAD0_OFFSET, reset_reason);
	mpi3mr_regwrite(sc, MPI3_SYSIF_HOST_DIAG_OFFSET, host_diagnostic | reset_type);
	
	if (reset_type == MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET) {
		do {
			ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
			if (ioc_status &
			    MPI3_SYSIF_IOC_STATUS_RESET_HISTORY) {
				ioc_config =
				    mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
				if (mpi3mr_soft_reset_success(ioc_status,
				    ioc_config)) {
					mpi3mr_clear_reset_history(sc);
					retval = 0;
					break;
				}
			}
			DELAY(100 * 1000);
		} while (--timeout);
	} else if (reset_type == MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT) {
		do {
			ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
			if (mpi3mr_diagfault_success(sc, ioc_status)) {
				retval = 0;
				break;
			}
			DELAY(100 * 1000);
		} while (--timeout);
	}
	
	mpi3mr_regwrite(sc, MPI3_SYSIF_WRITE_SEQUENCE_OFFSET, 
		MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_2ND);

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);

	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "IOC Status/Config after %s reset is (0x%x)/(0x%x)\n",
	    !retval ? "successful":"failed", ioc_status,
	    ioc_config);
	
	if (retval)
		sc->unrecoverable = 1;
	
	return retval;
}

inline void mpi3mr_cleanup_event_taskq(struct mpi3mr_softc *sc)
{
	/*
	 * Block the taskqueue before draining. This means any new tasks won't
	 * be queued to a worker thread. But it doesn't stop the current workers
	 * that are running. taskqueue_drain waits for those correctly in the
	 * case of thread backed taskqueues.
	 */
	taskqueue_block(sc->cam_sc->ev_tq);
	taskqueue_drain(sc->cam_sc->ev_tq, &sc->cam_sc->ev_task);
}

/**
 * mpi3mr_soft_reset_handler - Reset the controller
 * @sc: Adapter instance reference
 * @reset_reason: Reset reason code
 * @snapdump: snapdump enable/disbale bit
 *
 * This is an handler for recovering controller by issuing soft
 * reset or diag fault reset. This is a blocking function and
 * when one reset is executed if any other resets they will be
 * blocked. All IOCTLs/IO will be blocked during the reset. If
 * controller reset is successful then the controller will be
 * reinitalized, otherwise the controller will be marked as not
 * recoverable
 *
 * Return: 0 on success, non-zero on failure.
 */
int mpi3mr_soft_reset_handler(struct mpi3mr_softc *sc,
	U32 reset_reason, bool snapdump)
{
	int retval = 0, i = 0;
	enum mpi3mr_iocstate ioc_state;
	
	mpi3mr_dprint(sc, MPI3MR_INFO, "soft reset invoked: reason code: %s\n",
	    mpi3mr_reset_rc_name(reset_reason));

	if ((reset_reason == MPI3MR_RESET_FROM_IOCTL) &&
	     (sc->reset.ioctl_reset_snapdump != true))
		snapdump = false;
	
	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "soft_reset_handler: wait if diag save is in progress\n");
	while (sc->diagsave_timeout)
		DELAY(1000 * 1000);
	
	ioc_state = mpi3mr_get_iocstate(sc);
	if (ioc_state == MRIOC_STATE_UNRECOVERABLE) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "controller is in unrecoverable state, exit\n");
		sc->reset.type = MPI3MR_NO_RESET;
		sc->reset.reason = MPI3MR_DEFAULT_RESET_REASON;
		sc->reset.status = -1; 
		sc->reset.ioctl_reset_snapdump = false;	
		return -1;
	}
	
	if (sc->reset_in_progress) {
		mpi3mr_dprint(sc, MPI3MR_INFO, "reset is already in progress, exit\n");
		return -1;
	}

	/* Pause IOs, drain and block the event taskqueue */
	xpt_freeze_simq(sc->cam_sc->sim, 1);

	mpi3mr_cleanup_event_taskq(sc);

	sc->reset_in_progress = 1;
	sc->block_ioctls = 1;

	while (mpi3mr_atomic_read(&sc->pend_ioctls) && (i < PEND_IOCTLS_COMP_WAIT_TIME)) {
		ioc_state = mpi3mr_get_iocstate(sc);
		if (ioc_state == MRIOC_STATE_FAULT)
			break;
		i++;
		if (!(i % 5)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2ds]waiting for IOCTL to be finished from %s\n", i, __func__);
		}
		DELAY(1000 * 1000);
	}

	if ((!snapdump) && (reset_reason != MPI3MR_RESET_FROM_FAULT_WATCH) &&
	    (reset_reason != MPI3MR_RESET_FROM_FIRMWARE) &&
	    (reset_reason != MPI3MR_RESET_FROM_CIACTIV_FAULT)) {
		
		mpi3mr_dprint(sc, MPI3MR_INFO, "Turn off events prior to reset\n");

		for (i = 0; i < MPI3_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
			sc->event_masks[i] = -1;
		mpi3mr_issue_event_notification(sc);
	}

	mpi3mr_disable_interrupts(sc);

	if (snapdump)
		mpi3mr_trigger_snapdump(sc, reset_reason);

	retval = mpi3mr_issue_reset(sc,
	    MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET, reset_reason);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Failed to issue soft reset to the ioc\n");
		goto out;
	}

	mpi3mr_flush_drv_cmds(sc);
	mpi3mr_flush_io(sc);
	mpi3mr_invalidate_devhandles(sc);
	mpi3mr_memset_buffers(sc);

	if (sc->prepare_for_reset) {
		sc->prepare_for_reset = 0;
		sc->prepare_for_reset_timeout_counter = 0;
	}
	
	retval = mpi3mr_initialize_ioc(sc, MPI3MR_INIT_TYPE_RESET);
	if (retval) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "reinit after soft reset failed: reason %d\n",
		    reset_reason);
		goto out;
	}

	DELAY((1000 * 1000) * 10);
out:
	if (!retval) {
		sc->diagsave_timeout = 0;
		sc->reset_in_progress = 0;
		mpi3mr_rfresh_tgtdevs(sc);
		sc->ts_update_counter = 0;
		sc->block_ioctls = 0;
		sc->pel_abort_requested = 0;
		if (sc->pel_wait_pend) {
			sc->pel_cmds.retry_count = 0;
			mpi3mr_issue_pel_wait(sc, &sc->pel_cmds);
			mpi3mr_app_send_aen(sc);
		}
	} else {
		mpi3mr_issue_reset(sc,
		    MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT, reset_reason);
		sc->unrecoverable = 1;
		sc->reset_in_progress = 0;
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "Soft Reset: %s\n", ((retval == 0) ? "SUCCESS" : "FAILED"));

	taskqueue_unblock(sc->cam_sc->ev_tq);
	xpt_release_simq(sc->cam_sc->sim, 1);
	
	sc->reset.type = MPI3MR_NO_RESET;
	sc->reset.reason = MPI3MR_DEFAULT_RESET_REASON;
	sc->reset.status = retval; 
	sc->reset.ioctl_reset_snapdump = false;	

	return retval;
}

/**
 * mpi3mr_issue_ioc_shutdown - shutdown controller
 * @sc: Adapter instance reference
 *
 * Send shutodwn notification to the controller and wait for the
 * shutdown_timeout for it to be completed.
 *
 * Return: Nothing.
 */
static void mpi3mr_issue_ioc_shutdown(struct mpi3mr_softc *sc)
{
	U32 ioc_config, ioc_status;
	U8 retval = 1, retry = 0;
	U32 timeout = MPI3MR_DEFAULT_SHUTDOWN_TIME * 10;

	mpi3mr_dprint(sc, MPI3MR_INFO, "sending shutdown notification\n");
	if (sc->unrecoverable) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, 
		    "controller is unrecoverable, shutdown not issued\n");
		return;
	}
	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	if ((ioc_status & MPI3_SYSIF_IOC_STATUS_SHUTDOWN_MASK)
	    == MPI3_SYSIF_IOC_STATUS_SHUTDOWN_IN_PROGRESS) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "shutdown already in progress\n");
		return;
	}

	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
	ioc_config |= MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_NORMAL;
	ioc_config |= MPI3_SYSIF_IOC_CONFIG_DEVICE_SHUTDOWN_SEND_REQ;

	mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET, ioc_config);

	if (sc->facts.shutdown_timeout)
		timeout = sc->facts.shutdown_timeout * 10;

	do {
		ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
		if ((ioc_status & MPI3_SYSIF_IOC_STATUS_SHUTDOWN_MASK)
		    == MPI3_SYSIF_IOC_STATUS_SHUTDOWN_COMPLETE) {
			retval = 0;
			break;
		}
		
		if (sc->unrecoverable)
			break;

		if ((ioc_status & MPI3_SYSIF_IOC_STATUS_FAULT)) {
			mpi3mr_print_fault_info(sc);
			
			if (retry >= MPI3MR_MAX_SHUTDOWN_RETRY_COUNT)
				break;
			
			if (mpi3mr_issue_reset(sc,
			    MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET,
			    MPI3MR_RESET_FROM_CTLR_CLEANUP))
				break;
			
			ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);
			ioc_config |= MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_NORMAL;
			ioc_config |= MPI3_SYSIF_IOC_CONFIG_DEVICE_SHUTDOWN_SEND_REQ;
			
			mpi3mr_regwrite(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET, ioc_config);
			
			if (sc->facts.shutdown_timeout)
				timeout = sc->facts.shutdown_timeout * 10;
			
			retry++;
		}

                DELAY(100 * 1000);

	} while (--timeout);

	ioc_status = mpi3mr_regread(sc, MPI3_SYSIF_IOC_STATUS_OFFSET);
	ioc_config = mpi3mr_regread(sc, MPI3_SYSIF_IOC_CONFIG_OFFSET);

	if (retval) {
		if ((ioc_status & MPI3_SYSIF_IOC_STATUS_SHUTDOWN_MASK)
		    == MPI3_SYSIF_IOC_STATUS_SHUTDOWN_IN_PROGRESS)
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "shutdown still in progress after timeout\n");
	}

	mpi3mr_dprint(sc, MPI3MR_INFO,
	    "ioc_status/ioc_config after %s shutdown is (0x%x)/(0x%x)\n",
	    (!retval)?"successful":"failed", ioc_status,
	    ioc_config);
}

/**
 * mpi3mr_cleanup_ioc - Cleanup controller
 * @sc: Adapter instance reference

 * controller cleanup handler, Message unit reset or soft reset
 * and shutdown notification is issued to the controller.
 *
 * Return: Nothing.
 */
void mpi3mr_cleanup_ioc(struct mpi3mr_softc *sc)
{
	enum mpi3mr_iocstate ioc_state;

	mpi3mr_dprint(sc, MPI3MR_INFO, "cleaning up the controller\n");
	mpi3mr_disable_interrupts(sc);

	ioc_state = mpi3mr_get_iocstate(sc);

	if ((!sc->unrecoverable) && (!sc->reset_in_progress) &&
	    (ioc_state == MRIOC_STATE_READY)) {
		if (mpi3mr_mur_ioc(sc,
		    MPI3MR_RESET_FROM_CTLR_CLEANUP))
			mpi3mr_issue_reset(sc,
			    MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET,
			    MPI3MR_RESET_FROM_MUR_FAILURE);
		mpi3mr_issue_ioc_shutdown(sc);
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "controller cleanup completed\n");
}
