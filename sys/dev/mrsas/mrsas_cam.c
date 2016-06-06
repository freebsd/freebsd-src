/*
 * Copyright (c) 2015, AVAGO Tech. All rights reserved. Author: Marian Choy
 * Copyright (c) 2014, LSI Corp. All rights reserved. Author: Marian Choy
 * Support: freebsdraid@avagotech.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer. 2. Redistributions
 * in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution. 3. Neither the name of the
 * <ORGANIZATION> nor the names of its contributors may be used to endorse or
 * promote products derived from this software without specific prior written
 * permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/mrsas/mrsas.h"

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <sys/taskqueue.h>
#include <sys/kernel.h>


#include <sys/time.h>			/* XXX for pcpu.h */
#include <sys/pcpu.h>			/* XXX for PCPU_GET */

#define	smp_processor_id()  PCPU_GET(cpuid)

/*
 * Function prototypes
 */
int	mrsas_cam_attach(struct mrsas_softc *sc);
int	mrsas_find_io_type(struct cam_sim *sim, union ccb *ccb);
int	mrsas_bus_scan(struct mrsas_softc *sc);
int	mrsas_bus_scan_sim(struct mrsas_softc *sc, struct cam_sim *sim);
int 
mrsas_map_request(struct mrsas_softc *sc,
    struct mrsas_mpt_cmd *cmd, union ccb *ccb);
int
mrsas_build_ldio_rw(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb);
int
mrsas_build_ldio_nonrw(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb);
int
mrsas_build_syspdio(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb, struct cam_sim *sim, u_int8_t fp_possible);
int
mrsas_setup_io(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb, u_int32_t device_id,
    MRSAS_RAID_SCSI_IO_REQUEST * io_request);
void	mrsas_xpt_freeze(struct mrsas_softc *sc);
void	mrsas_xpt_release(struct mrsas_softc *sc);
void	mrsas_cam_detach(struct mrsas_softc *sc);
void	mrsas_release_mpt_cmd(struct mrsas_mpt_cmd *cmd);
void	mrsas_unmap_request(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd);
void	mrsas_cmd_done(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd);
void
mrsas_fire_cmd(struct mrsas_softc *sc, u_int32_t req_desc_lo,
    u_int32_t req_desc_hi);
void
mrsas_set_pd_lba(MRSAS_RAID_SCSI_IO_REQUEST * io_request,
    u_int8_t cdb_len, struct IO_REQUEST_INFO *io_info, union ccb *ccb,
    MR_DRV_RAID_MAP_ALL * local_map_ptr, u_int32_t ref_tag,
    u_int32_t ld_block_size);
static void mrsas_freeze_simq(struct mrsas_mpt_cmd *cmd, struct cam_sim *sim);
static void mrsas_cam_poll(struct cam_sim *sim);
static void mrsas_action(struct cam_sim *sim, union ccb *ccb);
static void mrsas_scsiio_timeout(void *data);
static void
mrsas_data_load_cb(void *arg, bus_dma_segment_t *segs,
    int nseg, int error);
static int32_t
mrsas_startio(struct mrsas_softc *sc, struct cam_sim *sim,
    union ccb *ccb);
struct mrsas_mpt_cmd *mrsas_get_mpt_cmd(struct mrsas_softc *sc);
MRSAS_REQUEST_DESCRIPTOR_UNION *
	mrsas_get_request_desc(struct mrsas_softc *sc, u_int16_t index);

extern u_int16_t MR_TargetIdToLdGet(u_int32_t ldTgtId, MR_DRV_RAID_MAP_ALL * map);
extern u_int32_t
MR_LdBlockSizeGet(u_int32_t ldTgtId, MR_DRV_RAID_MAP_ALL * map,
    struct mrsas_softc *sc);
extern void mrsas_isr(void *arg);
extern void mrsas_aen_handler(struct mrsas_softc *sc);
extern u_int8_t
MR_BuildRaidContext(struct mrsas_softc *sc,
    struct IO_REQUEST_INFO *io_info, RAID_CONTEXT * pRAID_Context,
    MR_DRV_RAID_MAP_ALL * map);
extern u_int16_t
MR_LdSpanArrayGet(u_int32_t ld, u_int32_t span,
    MR_DRV_RAID_MAP_ALL * map);
extern u_int16_t 
mrsas_get_updated_dev_handle(struct mrsas_softc *sc,
    PLD_LOAD_BALANCE_INFO lbInfo, struct IO_REQUEST_INFO *io_info);
extern u_int8_t
megasas_get_best_arm(PLD_LOAD_BALANCE_INFO lbInfo, u_int8_t arm,
    u_int64_t block, u_int32_t count);
extern int mrsas_complete_cmd(struct mrsas_softc *sc, u_int32_t MSIxIndex);


/*
 * mrsas_cam_attach:	Main entry to CAM subsystem
 * input:				Adapter instance soft state
 *
 * This function is called from mrsas_attach() during initialization to perform
 * SIM allocations and XPT bus registration.  If the kernel version is 7.4 or
 * earlier, it would also initiate a bus scan.
 */
int
mrsas_cam_attach(struct mrsas_softc *sc)
{
	struct cam_devq *devq;
	int mrsas_cam_depth;

	mrsas_cam_depth = sc->max_fw_cmds - MRSAS_INTERNAL_CMDS;

	if ((devq = cam_simq_alloc(mrsas_cam_depth)) == NULL) {
		device_printf(sc->mrsas_dev, "Cannot allocate SIM queue\n");
		return (ENOMEM);
	}
	/*
	 * Create SIM for bus 0 and register, also create path
	 */
	sc->sim_0 = cam_sim_alloc(mrsas_action, mrsas_cam_poll, "mrsas", sc,
	    device_get_unit(sc->mrsas_dev), &sc->sim_lock, mrsas_cam_depth,
	    mrsas_cam_depth, devq);
	if (sc->sim_0 == NULL) {
		cam_simq_free(devq);
		device_printf(sc->mrsas_dev, "Cannot register SIM\n");
		return (ENXIO);
	}
	/* Initialize taskqueue for Event Handling */
	TASK_INIT(&sc->ev_task, 0, (void *)mrsas_aen_handler, sc);
	sc->ev_tq = taskqueue_create("mrsas_taskq", M_NOWAIT | M_ZERO,
	    taskqueue_thread_enqueue, &sc->ev_tq);

	/* Run the task queue with lowest priority */
	taskqueue_start_threads(&sc->ev_tq, 1, 255, "%s taskq",
	    device_get_nameunit(sc->mrsas_dev));
	mtx_lock(&sc->sim_lock);
	if (xpt_bus_register(sc->sim_0, sc->mrsas_dev, 0) != CAM_SUCCESS) {
		cam_sim_free(sc->sim_0, TRUE);	/* passing true frees the devq */
		mtx_unlock(&sc->sim_lock);
		return (ENXIO);
	}
	if (xpt_create_path(&sc->path_0, NULL, cam_sim_path(sc->sim_0),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sim_0));
		cam_sim_free(sc->sim_0, TRUE);	/* passing true will free the
						 * devq */
		mtx_unlock(&sc->sim_lock);
		return (ENXIO);
	}
	mtx_unlock(&sc->sim_lock);

	/*
	 * Create SIM for bus 1 and register, also create path
	 */
	sc->sim_1 = cam_sim_alloc(mrsas_action, mrsas_cam_poll, "mrsas", sc,
	    device_get_unit(sc->mrsas_dev), &sc->sim_lock, mrsas_cam_depth,
	    mrsas_cam_depth, devq);
	if (sc->sim_1 == NULL) {
		cam_simq_free(devq);
		device_printf(sc->mrsas_dev, "Cannot register SIM\n");
		return (ENXIO);
	}
	mtx_lock(&sc->sim_lock);
	if (xpt_bus_register(sc->sim_1, sc->mrsas_dev, 1) != CAM_SUCCESS) {
		cam_sim_free(sc->sim_1, TRUE);	/* passing true frees the devq */
		mtx_unlock(&sc->sim_lock);
		return (ENXIO);
	}
	if (xpt_create_path(&sc->path_1, NULL, cam_sim_path(sc->sim_1),
	    CAM_TARGET_WILDCARD,
	    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sim_1));
		cam_sim_free(sc->sim_1, TRUE);
		mtx_unlock(&sc->sim_lock);
		return (ENXIO);
	}
	mtx_unlock(&sc->sim_lock);

#if (__FreeBSD_version <= 704000)
	if (mrsas_bus_scan(sc)) {
		device_printf(sc->mrsas_dev, "Error in bus scan.\n");
		return (1);
	}
#endif
	return (0);
}

/*
 * mrsas_cam_detach:	De-allocates and teardown CAM
 * input:				Adapter instance soft state
 *
 * De-registers and frees the paths and SIMs.
 */
void
mrsas_cam_detach(struct mrsas_softc *sc)
{
	if (sc->ev_tq != NULL)
		taskqueue_free(sc->ev_tq);
	mtx_lock(&sc->sim_lock);
	if (sc->path_0)
		xpt_free_path(sc->path_0);
	if (sc->sim_0) {
		xpt_bus_deregister(cam_sim_path(sc->sim_0));
		cam_sim_free(sc->sim_0, FALSE);
	}
	if (sc->path_1)
		xpt_free_path(sc->path_1);
	if (sc->sim_1) {
		xpt_bus_deregister(cam_sim_path(sc->sim_1));
		cam_sim_free(sc->sim_1, TRUE);
	}
	mtx_unlock(&sc->sim_lock);
}

/*
 * mrsas_action:	SIM callback entry point
 * input:			pointer to SIM pointer to CAM Control Block
 *
 * This function processes CAM subsystem requests. The type of request is stored
 * in ccb->ccb_h.func_code.  The preprocessor #ifdef is necessary because
 * ccb->cpi.maxio is not supported for FreeBSD version 7.4 or earlier.
 */
static void
mrsas_action(struct cam_sim *sim, union ccb *ccb)
{
	struct mrsas_softc *sc = (struct mrsas_softc *)cam_sim_softc(sim);
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	u_int32_t device_id;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
		{
			device_id = ccb_h->target_id;

			/*
			 * bus 0 is LD, bus 1 is for system-PD
			 */
			if (cam_sim_bus(sim) == 1 &&
			    sc->pd_list[device_id].driveState != MR_PD_STATE_SYSTEM) {
				ccb->ccb_h.status |= CAM_DEV_NOT_THERE;
				xpt_done(ccb);
			} else {
				if (mrsas_startio(sc, sim, ccb)) {
					ccb->ccb_h.status |= CAM_REQ_INVALID;
					xpt_done(ccb);
				}
			}
			break;
		}
	case XPT_ABORT:
		{
			ccb->ccb_h.status = CAM_UA_ABORT;
			xpt_done(ccb);
			break;
		}
	case XPT_RESET_BUS:
		{
			xpt_done(ccb);
			break;
		}
	case XPT_GET_TRAN_SETTINGS:
		{
			ccb->cts.protocol = PROTO_SCSI;
			ccb->cts.protocol_version = SCSI_REV_2;
			ccb->cts.transport = XPORT_SPI;
			ccb->cts.transport_version = 2;
			ccb->cts.xport_specific.spi.valid = CTS_SPI_VALID_DISC;
			ccb->cts.xport_specific.spi.flags = CTS_SPI_FLAGS_DISC_ENB;
			ccb->cts.proto_specific.scsi.valid = CTS_SCSI_VALID_TQ;
			ccb->cts.proto_specific.scsi.flags = CTS_SCSI_FLAGS_TAG_ENB;
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
			break;
		}
	case XPT_SET_TRAN_SETTINGS:
		{
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			xpt_done(ccb);
			break;
		}
	case XPT_CALC_GEOMETRY:
		{
			cam_calc_geometry(&ccb->ccg, 1);
			xpt_done(ccb);
			break;
		}
	case XPT_PATH_INQ:
		{
			ccb->cpi.version_num = 1;
			ccb->cpi.hba_inquiry = 0;
			ccb->cpi.target_sprt = 0;
#if (__FreeBSD_version >= 902001)
			ccb->cpi.hba_misc = PIM_UNMAPPED;
#else
			ccb->cpi.hba_misc = 0;
#endif
			ccb->cpi.hba_eng_cnt = 0;
			ccb->cpi.max_lun = MRSAS_SCSI_MAX_LUNS;
			ccb->cpi.unit_number = cam_sim_unit(sim);
			ccb->cpi.bus_id = cam_sim_bus(sim);
			ccb->cpi.initiator_id = MRSAS_SCSI_INITIATOR_ID;
			ccb->cpi.base_transfer_speed = 150000;
			strncpy(ccb->cpi.sim_vid, "FreeBSD", SIM_IDLEN);
			strncpy(ccb->cpi.hba_vid, "AVAGO", HBA_IDLEN);
			strncpy(ccb->cpi.dev_name, cam_sim_name(sim), DEV_IDLEN);
			ccb->cpi.transport = XPORT_SPI;
			ccb->cpi.transport_version = 2;
			ccb->cpi.protocol = PROTO_SCSI;
			ccb->cpi.protocol_version = SCSI_REV_2;
			if (ccb->cpi.bus_id == 0)
				ccb->cpi.max_target = MRSAS_MAX_PD - 1;
			else
				ccb->cpi.max_target = MRSAS_MAX_LD_IDS - 1;
#if (__FreeBSD_version > 704000)
			ccb->cpi.maxio = sc->max_num_sge * MRSAS_PAGE_SIZE;
#endif
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
			break;
		}
	default:
		{
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
	}
}

/*
 * mrsas_scsiio_timeout:	Callback function for IO timed out
 * input:					mpt command context
 *
 * This function will execute after timeout value provided by ccb header from
 * CAM layer, if timer expires. Driver will run timer for all DCDM and LDIO
 * comming from CAM layer. This function is callback function for IO timeout
 * and it runs in no-sleep context. Set do_timedout_reset in Adapter context
 * so that it will execute OCR/Kill adpter from ocr_thread context.
 */
static void
mrsas_scsiio_timeout(void *data)
{
	struct mrsas_mpt_cmd *cmd;
	struct mrsas_softc *sc;

	cmd = (struct mrsas_mpt_cmd *)data;
	sc = cmd->sc;

	if (cmd->ccb_ptr == NULL) {
		printf("command timeout with NULL ccb\n");
		return;
	}
	/*
	 * Below callout is dummy entry so that it will be cancelled from
	 * mrsas_cmd_done(). Now Controller will go to OCR/Kill Adapter based
	 * on OCR enable/disable property of Controller from ocr_thread
	 * context.
	 */
#if (__FreeBSD_version >= 1000510)
	callout_reset_sbt(&cmd->cm_callout, SBT_1S * 600, 0,
	    mrsas_scsiio_timeout, cmd, 0);
#else
	callout_reset(&cmd->cm_callout, (600000 * hz) / 1000,
	    mrsas_scsiio_timeout, cmd);
#endif
	sc->do_timedout_reset = SCSIIO_TIMEOUT_OCR;
	if (sc->ocr_thread_active)
		wakeup(&sc->ocr_chan);
}

/*
 * mrsas_startio:	SCSI IO entry point
 * input:			Adapter instance soft state
 * 					pointer to CAM Control Block
 *
 * This function is the SCSI IO entry point and it initiates IO processing. It
 * copies the IO and depending if the IO is read/write or inquiry, it would
 * call mrsas_build_ldio() or mrsas_build_dcdb(), respectively.  It returns 0
 * if the command is sent to firmware successfully, otherwise it returns 1.
 */
static int32_t
mrsas_startio(struct mrsas_softc *sc, struct cam_sim *sim,
    union ccb *ccb)
{
	struct mrsas_mpt_cmd *cmd;
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	struct ccb_scsiio *csio = &(ccb->csio);
	MRSAS_REQUEST_DESCRIPTOR_UNION *req_desc;
	u_int8_t cmd_type;

	if ((csio->cdb_io.cdb_bytes[0]) == SYNCHRONIZE_CACHE) {
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return (0);
	}
	ccb_h->status |= CAM_SIM_QUEUED;
	cmd = mrsas_get_mpt_cmd(sc);

	if (!cmd) {
		ccb_h->status |= CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return (0);
	}
	if ((ccb_h->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if (ccb_h->flags & CAM_DIR_IN)
			cmd->flags |= MRSAS_DIR_IN;
		if (ccb_h->flags & CAM_DIR_OUT)
			cmd->flags |= MRSAS_DIR_OUT;
	} else
		cmd->flags = MRSAS_DIR_NONE;	/* no data */

/* For FreeBSD 9.2 and higher */
#if (__FreeBSD_version >= 902001)
	/*
	 * XXX We don't yet support physical addresses here.
	 */
	switch ((ccb->ccb_h.flags & CAM_DATA_MASK)) {
	case CAM_DATA_PADDR:
	case CAM_DATA_SG_PADDR:
		device_printf(sc->mrsas_dev, "%s: physical addresses not supported\n",
		    __func__);
		mrsas_release_mpt_cmd(cmd);
		ccb_h->status = CAM_REQ_INVALID;
		ccb_h->status &= ~CAM_SIM_QUEUED;
		goto done;
	case CAM_DATA_SG:
		device_printf(sc->mrsas_dev, "%s: scatter gather is not supported\n",
		    __func__);
		mrsas_release_mpt_cmd(cmd);
		ccb_h->status = CAM_REQ_INVALID;
		goto done;
	case CAM_DATA_VADDR:
		if (csio->dxfer_len > (sc->max_num_sge * MRSAS_PAGE_SIZE)) {
			mrsas_release_mpt_cmd(cmd);
			ccb_h->status = CAM_REQ_TOO_BIG;
			goto done;
		}
		cmd->length = csio->dxfer_len;
		if (cmd->length)
			cmd->data = csio->data_ptr;
		break;
	case CAM_DATA_BIO:
		if (csio->dxfer_len > (sc->max_num_sge * MRSAS_PAGE_SIZE)) {
			mrsas_release_mpt_cmd(cmd);
			ccb_h->status = CAM_REQ_TOO_BIG;
			goto done;
		}
		cmd->length = csio->dxfer_len;
		if (cmd->length)
			cmd->data = csio->data_ptr;
		break;
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		goto done;
	}
#else
	if (!(ccb_h->flags & CAM_DATA_PHYS)) {	/* Virtual data address */
		if (!(ccb_h->flags & CAM_SCATTER_VALID)) {
			if (csio->dxfer_len > (sc->max_num_sge * MRSAS_PAGE_SIZE)) {
				mrsas_release_mpt_cmd(cmd);
				ccb_h->status = CAM_REQ_TOO_BIG;
				goto done;
			}
			cmd->length = csio->dxfer_len;
			if (cmd->length)
				cmd->data = csio->data_ptr;
		} else {
			mrsas_release_mpt_cmd(cmd);
			ccb_h->status = CAM_REQ_INVALID;
			goto done;
		}
	} else {			/* Data addresses are physical. */
		mrsas_release_mpt_cmd(cmd);
		ccb_h->status = CAM_REQ_INVALID;
		ccb_h->status &= ~CAM_SIM_QUEUED;
		goto done;
	}
#endif
	/* save ccb ptr */
	cmd->ccb_ptr = ccb;

	req_desc = mrsas_get_request_desc(sc, (cmd->index) - 1);
	if (!req_desc) {
		device_printf(sc->mrsas_dev, "Cannot get request_descriptor.\n");
		return (FAIL);
	}
	memset(req_desc, 0, sizeof(MRSAS_REQUEST_DESCRIPTOR_UNION));
	cmd->request_desc = req_desc;

	if (ccb_h->flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, cmd->io_request->CDB.CDB32, csio->cdb_len);
	else
		bcopy(csio->cdb_io.cdb_bytes, cmd->io_request->CDB.CDB32, csio->cdb_len);
	mtx_lock(&sc->raidmap_lock);

	/* Check for IO type READ-WRITE targeted for Logical Volume */
	cmd_type = mrsas_find_io_type(sim, ccb);
	switch (cmd_type) {
	case READ_WRITE_LDIO:
		/* Build READ-WRITE IO for Logical Volume  */
		if (mrsas_build_ldio_rw(sc, cmd, ccb)) {
			device_printf(sc->mrsas_dev, "Build RW LDIO failed.\n");
			mtx_unlock(&sc->raidmap_lock);
			return (1);
		}
		break;
	case NON_READ_WRITE_LDIO:
		/* Build NON READ-WRITE IO for Logical Volume  */
		if (mrsas_build_ldio_nonrw(sc, cmd, ccb)) {
			device_printf(sc->mrsas_dev, "Build NON-RW LDIO failed.\n");
			mtx_unlock(&sc->raidmap_lock);
			return (1);
		}
		break;
	case READ_WRITE_SYSPDIO:
	case NON_READ_WRITE_SYSPDIO:
		if (sc->secure_jbod_support &&
		    (cmd_type == NON_READ_WRITE_SYSPDIO)) {
			/* Build NON-RW IO for JBOD */
			if (mrsas_build_syspdio(sc, cmd, ccb, sim, 0)) {
				device_printf(sc->mrsas_dev,
				    "Build SYSPDIO failed.\n");
				mtx_unlock(&sc->raidmap_lock);
				return (1);
			}
		} else {
			/* Build RW IO for JBOD */
			if (mrsas_build_syspdio(sc, cmd, ccb, sim, 1)) {
				device_printf(sc->mrsas_dev,
				    "Build SYSPDIO failed.\n");
				mtx_unlock(&sc->raidmap_lock);
				return (1);
			}
		}
	}
	mtx_unlock(&sc->raidmap_lock);

	if (cmd->flags == MRSAS_DIR_IN)	/* from device */
		cmd->io_request->Control |= MPI2_SCSIIO_CONTROL_READ;
	else if (cmd->flags == MRSAS_DIR_OUT)	/* to device */
		cmd->io_request->Control |= MPI2_SCSIIO_CONTROL_WRITE;

	cmd->io_request->SGLFlags = MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
	cmd->io_request->SGLOffset0 = offsetof(MRSAS_RAID_SCSI_IO_REQUEST, SGL) / 4;
	cmd->io_request->SenseBufferLowAddress = cmd->sense_phys_addr;
	cmd->io_request->SenseBufferLength = MRSAS_SCSI_SENSE_BUFFERSIZE;

	req_desc = cmd->request_desc;
	req_desc->SCSIIO.SMID = cmd->index;

	/*
	 * Start timer for IO timeout. Default timeout value is 90 second.
	 */
#if (__FreeBSD_version >= 1000510)
	callout_reset_sbt(&cmd->cm_callout, SBT_1S * 600, 0,
	    mrsas_scsiio_timeout, cmd, 0);
#else
	callout_reset(&cmd->cm_callout, (600000 * hz) / 1000,
	    mrsas_scsiio_timeout, cmd);
#endif
	mrsas_atomic_inc(&sc->fw_outstanding);

	if (mrsas_atomic_read(&sc->fw_outstanding) > sc->io_cmds_highwater)
		sc->io_cmds_highwater++;

	mrsas_fire_cmd(sc, req_desc->addr.u.low, req_desc->addr.u.high);
	return (0);

done:
	xpt_done(ccb);
	return (0);
}

/*
 * mrsas_find_io_type:	Determines if IO is read/write or inquiry
 * input:			pointer to CAM Control Block
 *
 * This function determines if the IO is read/write or inquiry.  It returns a 1
 * if the IO is read/write and 0 if it is inquiry.
 */
int 
mrsas_find_io_type(struct cam_sim *sim, union ccb *ccb)
{
	struct ccb_scsiio *csio = &(ccb->csio);

	switch (csio->cdb_io.cdb_bytes[0]) {
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
	case READ_6:
	case WRITE_6:
	case READ_16:
	case WRITE_16:
		return (cam_sim_bus(sim) ?
		    READ_WRITE_SYSPDIO : READ_WRITE_LDIO);
	default:
		return (cam_sim_bus(sim) ?
		    NON_READ_WRITE_SYSPDIO : NON_READ_WRITE_LDIO);
	}
}

/*
 * mrsas_get_mpt_cmd:	Get a cmd from free command pool
 * input:				Adapter instance soft state
 *
 * This function removes an MPT command from the command free list and
 * initializes it.
 */
struct mrsas_mpt_cmd *
mrsas_get_mpt_cmd(struct mrsas_softc *sc)
{
	struct mrsas_mpt_cmd *cmd = NULL;

	mtx_lock(&sc->mpt_cmd_pool_lock);
	if (!TAILQ_EMPTY(&sc->mrsas_mpt_cmd_list_head)) {
		cmd = TAILQ_FIRST(&sc->mrsas_mpt_cmd_list_head);
		TAILQ_REMOVE(&sc->mrsas_mpt_cmd_list_head, cmd, next);
	} else {
		goto out;
	}

	memset((uint8_t *)cmd->io_request, 0, MRSAS_MPI2_RAID_DEFAULT_IO_FRAME_SIZE);
	cmd->data = NULL;
	cmd->length = 0;
	cmd->flags = 0;
	cmd->error_code = 0;
	cmd->load_balance = 0;
	cmd->ccb_ptr = NULL;

out:
	mtx_unlock(&sc->mpt_cmd_pool_lock);
	return cmd;
}

/*
 * mrsas_release_mpt_cmd:	Return a cmd to free command pool
 * input:					Command packet for return to free command pool
 *
 * This function returns an MPT command to the free command list.
 */
void
mrsas_release_mpt_cmd(struct mrsas_mpt_cmd *cmd)
{
	struct mrsas_softc *sc = cmd->sc;

	mtx_lock(&sc->mpt_cmd_pool_lock);
	cmd->sync_cmd_idx = (u_int32_t)MRSAS_ULONG_MAX;
	TAILQ_INSERT_TAIL(&(sc->mrsas_mpt_cmd_list_head), cmd, next);
	mtx_unlock(&sc->mpt_cmd_pool_lock);

	return;
}

/*
 * mrsas_get_request_desc:	Get request descriptor from array
 * input:					Adapter instance soft state
 * 							SMID index
 *
 * This function returns a pointer to the request descriptor.
 */
MRSAS_REQUEST_DESCRIPTOR_UNION *
mrsas_get_request_desc(struct mrsas_softc *sc, u_int16_t index)
{
	u_int8_t *p;

	if (index >= sc->max_fw_cmds) {
		device_printf(sc->mrsas_dev, "Invalid SMID (0x%x)request for desc\n", index);
		return NULL;
	}
	p = sc->req_desc + sizeof(MRSAS_REQUEST_DESCRIPTOR_UNION) * index;

	return (MRSAS_REQUEST_DESCRIPTOR_UNION *) p;
}

/*
 * mrsas_build_ldio_rw:	Builds an LDIO command
 * input:				Adapter instance soft state
 * 						Pointer to command packet
 * 						Pointer to CCB
 *
 * This function builds the LDIO command packet.  It returns 0 if the command is
 * built successfully, otherwise it returns a 1.
 */
int
mrsas_build_ldio_rw(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb)
{
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	struct ccb_scsiio *csio = &(ccb->csio);
	u_int32_t device_id;
	MRSAS_RAID_SCSI_IO_REQUEST *io_request;

	device_id = ccb_h->target_id;

	io_request = cmd->io_request;
	io_request->RaidContext.VirtualDiskTgtId = device_id;
	io_request->RaidContext.status = 0;
	io_request->RaidContext.exStatus = 0;

	/* just the cdb len, other flags zero, and ORed-in later for FP */
	io_request->IoFlags = csio->cdb_len;

	if (mrsas_setup_io(sc, cmd, ccb, device_id, io_request) != SUCCESS)
		device_printf(sc->mrsas_dev, "Build ldio or fpio error\n");

	io_request->DataLength = cmd->length;

	if (mrsas_map_request(sc, cmd, ccb) == SUCCESS) {
		if (cmd->sge_count > sc->max_num_sge) {
			device_printf(sc->mrsas_dev, "Error: sge_count (0x%x) exceeds"
			    "max (0x%x) allowed\n", cmd->sge_count, sc->max_num_sge);
			return (FAIL);
		}
		/*
		 * numSGE store lower 8 bit of sge_count. numSGEExt store
		 * higher 8 bit of sge_count
		 */
		io_request->RaidContext.numSGE = cmd->sge_count;
		io_request->RaidContext.numSGEExt = (uint8_t)(cmd->sge_count >> 8);

	} else {
		device_printf(sc->mrsas_dev, "Data map/load failed.\n");
		return (FAIL);
	}
	return (0);
}

/*
 * mrsas_setup_io:	Set up data including Fast Path I/O
 * input:			Adapter instance soft state
 * 					Pointer to command packet
 * 					Pointer to CCB
 *
 * This function builds the DCDB inquiry command.  It returns 0 if the command
 * is built successfully, otherwise it returns a 1.
 */
int
mrsas_setup_io(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb, u_int32_t device_id,
    MRSAS_RAID_SCSI_IO_REQUEST * io_request)
{
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	struct ccb_scsiio *csio = &(ccb->csio);
	struct IO_REQUEST_INFO io_info;
	MR_DRV_RAID_MAP_ALL *map_ptr;
	u_int8_t fp_possible;
	u_int32_t start_lba_hi, start_lba_lo, ld_block_size;
	u_int32_t datalength = 0;

	start_lba_lo = 0;
	start_lba_hi = 0;
	fp_possible = 0;

	/*
	 * READ_6 (0x08) or WRITE_6 (0x0A) cdb
	 */
	if (csio->cdb_len == 6) {
		datalength = (u_int32_t)csio->cdb_io.cdb_bytes[4];
		start_lba_lo = ((u_int32_t)csio->cdb_io.cdb_bytes[1] << 16) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[2] << 8) |
		    (u_int32_t)csio->cdb_io.cdb_bytes[3];
		start_lba_lo &= 0x1FFFFF;
	}
	/*
	 * READ_10 (0x28) or WRITE_6 (0x2A) cdb
	 */
	else if (csio->cdb_len == 10) {
		datalength = (u_int32_t)csio->cdb_io.cdb_bytes[8] |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[7] << 8);
		start_lba_lo = ((u_int32_t)csio->cdb_io.cdb_bytes[2] << 24) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[3] << 16) |
		    (u_int32_t)csio->cdb_io.cdb_bytes[4] << 8 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[5]);
	}
	/*
	 * READ_12 (0xA8) or WRITE_12 (0xAA) cdb
	 */
	else if (csio->cdb_len == 12) {
		datalength = (u_int32_t)csio->cdb_io.cdb_bytes[6] << 24 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[7] << 16) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[8] << 8) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[9]);
		start_lba_lo = ((u_int32_t)csio->cdb_io.cdb_bytes[2] << 24) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[3] << 16) |
		    (u_int32_t)csio->cdb_io.cdb_bytes[4] << 8 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[5]);
	}
	/*
	 * READ_16 (0x88) or WRITE_16 (0xx8A) cdb
	 */
	else if (csio->cdb_len == 16) {
		datalength = (u_int32_t)csio->cdb_io.cdb_bytes[10] << 24 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[11] << 16) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[12] << 8) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[13]);
		start_lba_lo = ((u_int32_t)csio->cdb_io.cdb_bytes[6] << 24) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[7] << 16) |
		    (u_int32_t)csio->cdb_io.cdb_bytes[8] << 8 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[9]);
		start_lba_hi = ((u_int32_t)csio->cdb_io.cdb_bytes[2] << 24) |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[3] << 16) |
		    (u_int32_t)csio->cdb_io.cdb_bytes[4] << 8 |
		    ((u_int32_t)csio->cdb_io.cdb_bytes[5]);
	}
	memset(&io_info, 0, sizeof(struct IO_REQUEST_INFO));
	io_info.ldStartBlock = ((u_int64_t)start_lba_hi << 32) | start_lba_lo;
	io_info.numBlocks = datalength;
	io_info.ldTgtId = device_id;

	switch (ccb_h->flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		io_info.isRead = 1;
		break;
	case CAM_DIR_OUT:
		io_info.isRead = 0;
		break;
	case CAM_DIR_NONE:
	default:
		mrsas_dprint(sc, MRSAS_TRACE, "From %s : DMA Flag is %d \n", __func__, ccb_h->flags & CAM_DIR_MASK);
		break;
	}

	map_ptr = sc->ld_drv_map[(sc->map_id & 1)];
	ld_block_size = MR_LdBlockSizeGet(device_id, map_ptr, sc);

	if ((MR_TargetIdToLdGet(device_id, map_ptr) >= MAX_LOGICAL_DRIVES_EXT) ||
	    (!sc->fast_path_io)) {
		io_request->RaidContext.regLockFlags = 0;
		fp_possible = 0;
	} else {
		if (MR_BuildRaidContext(sc, &io_info, &io_request->RaidContext, map_ptr))
			fp_possible = io_info.fpOkForIo;
	}

	cmd->request_desc->SCSIIO.MSIxIndex =
	    sc->msix_vectors ? smp_processor_id() % sc->msix_vectors : 0;


	if (fp_possible) {
		mrsas_set_pd_lba(io_request, csio->cdb_len, &io_info, ccb, map_ptr,
		    start_lba_lo, ld_block_size);
		io_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
		cmd->request_desc->SCSIIO.RequestFlags =
		    (MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY <<
		    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
		if ((sc->device_id == MRSAS_INVADER) ||
		    (sc->device_id == MRSAS_FURY) ||
		    (sc->device_id == MRSAS_INTRUDER) ||
		    (sc->device_id == MRSAS_INTRUDER_24) ||
		    (sc->device_id == MRSAS_CUTLASS_52) ||
		    (sc->device_id == MRSAS_CUTLASS_53)) {
			if (io_request->RaidContext.regLockFlags == REGION_TYPE_UNUSED)
				cmd->request_desc->SCSIIO.RequestFlags =
				    (MRSAS_REQ_DESCRIPT_FLAGS_NO_LOCK <<
				    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
			io_request->RaidContext.Type = MPI2_TYPE_CUDA;
			io_request->RaidContext.nseg = 0x1;
			io_request->IoFlags |= MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH;
			io_request->RaidContext.regLockFlags |=
			    (MR_RL_FLAGS_GRANT_DESTINATION_CUDA |
			    MR_RL_FLAGS_SEQ_NUM_ENABLE);
		}
		if ((sc->load_balance_info[device_id].loadBalanceFlag) &&
		    (io_info.isRead)) {
			io_info.devHandle =
			    mrsas_get_updated_dev_handle(sc,
			    &sc->load_balance_info[device_id], &io_info);
			cmd->load_balance = MRSAS_LOAD_BALANCE_FLAG;
			cmd->pd_r1_lb = io_info.pd_after_lb;
		} else
			cmd->load_balance = 0;
		cmd->request_desc->SCSIIO.DevHandle = io_info.devHandle;
		io_request->DevHandle = io_info.devHandle;
	} else {
		/* Not FP IO */
		io_request->RaidContext.timeoutValue = map_ptr->raidMap.fpPdIoTimeoutSec;
		cmd->request_desc->SCSIIO.RequestFlags =
		    (MRSAS_REQ_DESCRIPT_FLAGS_LD_IO <<
		    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
		if ((sc->device_id == MRSAS_INVADER) ||
		    (sc->device_id == MRSAS_FURY) ||
		    (sc->device_id == MRSAS_INTRUDER) ||
		    (sc->device_id == MRSAS_INTRUDER_24) ||
		    (sc->device_id == MRSAS_CUTLASS_52) ||
		    (sc->device_id == MRSAS_CUTLASS_53)) {
			if (io_request->RaidContext.regLockFlags == REGION_TYPE_UNUSED)
				cmd->request_desc->SCSIIO.RequestFlags =
				    (MRSAS_REQ_DESCRIPT_FLAGS_NO_LOCK <<
				    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
			io_request->RaidContext.Type = MPI2_TYPE_CUDA;
			io_request->RaidContext.regLockFlags |=
			    (MR_RL_FLAGS_GRANT_DESTINATION_CPU0 |
			    MR_RL_FLAGS_SEQ_NUM_ENABLE);
			io_request->RaidContext.nseg = 0x1;
		}
		io_request->Function = MRSAS_MPI2_FUNCTION_LD_IO_REQUEST;
		io_request->DevHandle = device_id;
	}
	return (0);
}

/*
 * mrsas_build_ldio_nonrw:	Builds an LDIO command
 * input:				Adapter instance soft state
 * 						Pointer to command packet
 * 						Pointer to CCB
 *
 * This function builds the LDIO command packet.  It returns 0 if the command is
 * built successfully, otherwise it returns a 1.
 */
int
mrsas_build_ldio_nonrw(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb)
{
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	u_int32_t device_id;
	MRSAS_RAID_SCSI_IO_REQUEST *io_request;

	io_request = cmd->io_request;
	device_id = ccb_h->target_id;

	/* FW path for LD Non-RW (SCSI management commands) */
	io_request->Function = MRSAS_MPI2_FUNCTION_LD_IO_REQUEST;
	io_request->DevHandle = device_id;
	cmd->request_desc->SCSIIO.RequestFlags =
	    (MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO <<
	    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);

	io_request->RaidContext.VirtualDiskTgtId = device_id;
	io_request->LUN[1] = ccb_h->target_lun & 0xF;
	io_request->DataLength = cmd->length;

	if (mrsas_map_request(sc, cmd, ccb) == SUCCESS) {
		if (cmd->sge_count > sc->max_num_sge) {
			device_printf(sc->mrsas_dev, "Error: sge_count (0x%x) exceeds"
			    "max (0x%x) allowed\n", cmd->sge_count, sc->max_num_sge);
			return (1);
		}
		/*
		 * numSGE store lower 8 bit of sge_count. numSGEExt store
		 * higher 8 bit of sge_count
		 */
		io_request->RaidContext.numSGE = cmd->sge_count;
		io_request->RaidContext.numSGEExt = (uint8_t)(cmd->sge_count >> 8);
	} else {
		device_printf(sc->mrsas_dev, "Data map/load failed.\n");
		return (1);
	}
	return (0);
}

/*
 * mrsas_build_syspdio:	Builds an DCDB command
 * input:				Adapter instance soft state
 * 						Pointer to command packet
 * 						Pointer to CCB
 *
 * This function builds the DCDB inquiry command.  It returns 0 if the command
 * is built successfully, otherwise it returns a 1.
 */
int
mrsas_build_syspdio(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd,
    union ccb *ccb, struct cam_sim *sim, u_int8_t fp_possible)
{
	struct ccb_hdr *ccb_h = &(ccb->ccb_h);
	u_int32_t device_id;
	MR_DRV_RAID_MAP_ALL *local_map_ptr;
	MRSAS_RAID_SCSI_IO_REQUEST *io_request;
	struct MR_PD_CFG_SEQ_NUM_SYNC *pd_sync;

	pd_sync = (void *)sc->jbodmap_mem[(sc->pd_seq_map_id - 1) & 1];

	io_request = cmd->io_request;
	device_id = ccb_h->target_id;
	local_map_ptr = sc->ld_drv_map[(sc->map_id & 1)];
	io_request->RaidContext.RAIDFlags = MR_RAID_FLAGS_IO_SUB_TYPE_SYSTEM_PD
	    << MR_RAID_CTX_RAID_FLAGS_IO_SUB_TYPE_SHIFT;
	io_request->RaidContext.regLockFlags = 0;
	io_request->RaidContext.regLockRowLBA = 0;
	io_request->RaidContext.regLockLength = 0;

	/* If FW supports PD sequence number */
	if (sc->use_seqnum_jbod_fp &&
	    sc->pd_list[device_id].driveType == 0x00) {
		//printf("Using Drv seq num\n");
		io_request->RaidContext.VirtualDiskTgtId = device_id + 255;
		io_request->RaidContext.configSeqNum = pd_sync->seq[device_id].seqNum;
		io_request->DevHandle = pd_sync->seq[device_id].devHandle;
		io_request->RaidContext.regLockFlags |=
		    (MR_RL_FLAGS_SEQ_NUM_ENABLE | MR_RL_FLAGS_GRANT_DESTINATION_CUDA);
		io_request->RaidContext.Type = MPI2_TYPE_CUDA;
		io_request->RaidContext.nseg = 0x1;
	} else if (sc->fast_path_io) {
		//printf("Using LD RAID map\n");
		io_request->RaidContext.VirtualDiskTgtId = device_id;
		io_request->RaidContext.configSeqNum = 0;
		local_map_ptr = sc->ld_drv_map[(sc->map_id & 1)];
		io_request->DevHandle =
		    local_map_ptr->raidMap.devHndlInfo[device_id].curDevHdl;
	} else {
		//printf("Using FW PATH\n");
		/* Want to send all IO via FW path */
		io_request->RaidContext.VirtualDiskTgtId = device_id;
		io_request->RaidContext.configSeqNum = 0;
		io_request->DevHandle = 0xFFFF;
	}

	cmd->request_desc->SCSIIO.DevHandle = io_request->DevHandle;
	cmd->request_desc->SCSIIO.MSIxIndex =
	    sc->msix_vectors ? smp_processor_id() % sc->msix_vectors : 0;

	if (!fp_possible) {
		/* system pd firmware path */
		io_request->Function = MRSAS_MPI2_FUNCTION_LD_IO_REQUEST;
		cmd->request_desc->SCSIIO.RequestFlags =
		    (MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO <<
		    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
		io_request->RaidContext.timeoutValue =
		    local_map_ptr->raidMap.fpPdIoTimeoutSec;
		io_request->RaidContext.VirtualDiskTgtId = device_id;
	} else {
		/* system pd fast path */
		io_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
		io_request->RaidContext.timeoutValue = local_map_ptr->raidMap.fpPdIoTimeoutSec;

		/*
		 * NOTE - For system pd RW cmds only IoFlags will be FAST_PATH
		 * Because the NON RW cmds will now go via FW Queue
		 * and not the Exception queue
		 */
		io_request->IoFlags |= MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH;

		cmd->request_desc->SCSIIO.RequestFlags =
		    (MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY <<
		    MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	}

	io_request->LUN[1] = ccb_h->target_lun & 0xF;
	io_request->DataLength = cmd->length;

	if (mrsas_map_request(sc, cmd, ccb) == SUCCESS) {
		if (cmd->sge_count > sc->max_num_sge) {
			device_printf(sc->mrsas_dev, "Error: sge_count (0x%x) exceeds"
			    "max (0x%x) allowed\n", cmd->sge_count, sc->max_num_sge);
			return (1);
		}
		/*
		 * numSGE store lower 8 bit of sge_count. numSGEExt store
		 * higher 8 bit of sge_count
		 */
		io_request->RaidContext.numSGE = cmd->sge_count;
		io_request->RaidContext.numSGEExt = (uint8_t)(cmd->sge_count >> 8);
	} else {
		device_printf(sc->mrsas_dev, "Data map/load failed.\n");
		return (1);
	}
	return (0);
}

/*
 * mrsas_map_request:	Map and load data
 * input:				Adapter instance soft state
 * 						Pointer to command packet
 *
 * For data from OS, map and load the data buffer into bus space.  The SG list
 * is built in the callback.  If the  bus dmamap load is not successful,
 * cmd->error_code will contain the  error code and a 1 is returned.
 */
int 
mrsas_map_request(struct mrsas_softc *sc,
    struct mrsas_mpt_cmd *cmd, union ccb *ccb)
{
	u_int32_t retcode = 0;
	struct cam_sim *sim;

	sim = xpt_path_sim(cmd->ccb_ptr->ccb_h.path);

	if (cmd->data != NULL) {
		/* Map data buffer into bus space */
		mtx_lock(&sc->io_lock);
#if (__FreeBSD_version >= 902001)
		retcode = bus_dmamap_load_ccb(sc->data_tag, cmd->data_dmamap, ccb,
		    mrsas_data_load_cb, cmd, 0);
#else
		retcode = bus_dmamap_load(sc->data_tag, cmd->data_dmamap, cmd->data,
		    cmd->length, mrsas_data_load_cb, cmd, BUS_DMA_NOWAIT);
#endif
		mtx_unlock(&sc->io_lock);
		if (retcode)
			device_printf(sc->mrsas_dev, "bus_dmamap_load(): retcode = %d\n", retcode);
		if (retcode == EINPROGRESS) {
			device_printf(sc->mrsas_dev, "request load in progress\n");
			mrsas_freeze_simq(cmd, sim);
		}
	}
	if (cmd->error_code)
		return (1);
	return (retcode);
}

/*
 * mrsas_unmap_request:	Unmap and unload data
 * input:				Adapter instance soft state
 * 						Pointer to command packet
 *
 * This function unmaps and unloads data from OS.
 */
void
mrsas_unmap_request(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd)
{
	if (cmd->data != NULL) {
		if (cmd->flags & MRSAS_DIR_IN)
			bus_dmamap_sync(sc->data_tag, cmd->data_dmamap, BUS_DMASYNC_POSTREAD);
		if (cmd->flags & MRSAS_DIR_OUT)
			bus_dmamap_sync(sc->data_tag, cmd->data_dmamap, BUS_DMASYNC_POSTWRITE);
		mtx_lock(&sc->io_lock);
		bus_dmamap_unload(sc->data_tag, cmd->data_dmamap);
		mtx_unlock(&sc->io_lock);
	}
}

/*
 * mrsas_data_load_cb:	Callback entry point
 * input:				Pointer to command packet as argument
 * 						Pointer to segment
 * 						Number of segments Error
 *
 * This is the callback function of the bus dma map load.  It builds the SG
 * list.
 */
static void
mrsas_data_load_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct mrsas_mpt_cmd *cmd = (struct mrsas_mpt_cmd *)arg;
	struct mrsas_softc *sc = cmd->sc;
	MRSAS_RAID_SCSI_IO_REQUEST *io_request;
	pMpi25IeeeSgeChain64_t sgl_ptr;
	int i = 0, sg_processed = 0;

	if (error) {
		cmd->error_code = error;
		device_printf(sc->mrsas_dev, "mrsas_data_load_cb: error=%d\n", error);
		if (error == EFBIG) {
			cmd->ccb_ptr->ccb_h.status = CAM_REQ_TOO_BIG;
			return;
		}
	}
	if (cmd->flags & MRSAS_DIR_IN)
		bus_dmamap_sync(cmd->sc->data_tag, cmd->data_dmamap,
		    BUS_DMASYNC_PREREAD);
	if (cmd->flags & MRSAS_DIR_OUT)
		bus_dmamap_sync(cmd->sc->data_tag, cmd->data_dmamap,
		    BUS_DMASYNC_PREWRITE);
	if (nseg > sc->max_num_sge) {
		device_printf(sc->mrsas_dev, "SGE count is too large or 0.\n");
		return;
	}
	io_request = cmd->io_request;
	sgl_ptr = (pMpi25IeeeSgeChain64_t)&io_request->SGL;

	if ((sc->device_id == MRSAS_INVADER) ||
	    (sc->device_id == MRSAS_FURY) ||
	    (sc->device_id == MRSAS_INTRUDER) ||
	    (sc->device_id == MRSAS_INTRUDER_24) ||
	    (sc->device_id == MRSAS_CUTLASS_52) ||
	    (sc->device_id == MRSAS_CUTLASS_53)) {
		pMpi25IeeeSgeChain64_t sgl_ptr_end = sgl_ptr;

		sgl_ptr_end += sc->max_sge_in_main_msg - 1;
		sgl_ptr_end->Flags = 0;
	}
	if (nseg != 0) {
		for (i = 0; i < nseg; i++) {
			sgl_ptr->Address = segs[i].ds_addr;
			sgl_ptr->Length = segs[i].ds_len;
			sgl_ptr->Flags = 0;
			if ((sc->device_id == MRSAS_INVADER) ||
			    (sc->device_id == MRSAS_FURY) ||
			    (sc->device_id == MRSAS_INTRUDER) ||
			    (sc->device_id == MRSAS_INTRUDER_24) ||
			    (sc->device_id == MRSAS_CUTLASS_52) ||
			    (sc->device_id == MRSAS_CUTLASS_53)) {
				if (i == nseg - 1)
					sgl_ptr->Flags = IEEE_SGE_FLAGS_END_OF_LIST;
			}
			sgl_ptr++;
			sg_processed = i + 1;
			if ((sg_processed == (sc->max_sge_in_main_msg - 1)) &&
			    (nseg > sc->max_sge_in_main_msg)) {
				pMpi25IeeeSgeChain64_t sg_chain;

				if ((sc->device_id == MRSAS_INVADER) ||
				    (sc->device_id == MRSAS_FURY) ||
				    (sc->device_id == MRSAS_INTRUDER) ||
				    (sc->device_id == MRSAS_INTRUDER_24) ||
				    (sc->device_id == MRSAS_CUTLASS_52) ||
				    (sc->device_id == MRSAS_CUTLASS_53)) {
					if ((cmd->io_request->IoFlags & MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH)
					    != MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH)
						cmd->io_request->ChainOffset = sc->chain_offset_io_request;
					else
						cmd->io_request->ChainOffset = 0;
				} else
					cmd->io_request->ChainOffset = sc->chain_offset_io_request;
				sg_chain = sgl_ptr;
				if ((sc->device_id == MRSAS_INVADER) ||
				    (sc->device_id == MRSAS_FURY) ||
				    (sc->device_id == MRSAS_INTRUDER) ||
				    (sc->device_id == MRSAS_INTRUDER_24) ||
				    (sc->device_id == MRSAS_CUTLASS_52) ||
				    (sc->device_id == MRSAS_CUTLASS_53))
					sg_chain->Flags = IEEE_SGE_FLAGS_CHAIN_ELEMENT;
				else
					sg_chain->Flags = (IEEE_SGE_FLAGS_CHAIN_ELEMENT | MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR);
				sg_chain->Length = (sizeof(MPI2_SGE_IO_UNION) * (nseg - sg_processed));
				sg_chain->Address = cmd->chain_frame_phys_addr;
				sgl_ptr = (pMpi25IeeeSgeChain64_t)cmd->chain_frame;
			}
		}
	}
	cmd->sge_count = nseg;
}

/*
 * mrsas_freeze_simq:	Freeze SIM queue
 * input:				Pointer to command packet
 * 						Pointer to SIM
 *
 * This function freezes the sim queue.
 */
static void
mrsas_freeze_simq(struct mrsas_mpt_cmd *cmd, struct cam_sim *sim)
{
	union ccb *ccb = (union ccb *)(cmd->ccb_ptr);

	xpt_freeze_simq(sim, 1);
	ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
	ccb->ccb_h.status |= CAM_REQUEUE_REQ;
}

void
mrsas_xpt_freeze(struct mrsas_softc *sc)
{
	xpt_freeze_simq(sc->sim_0, 1);
	xpt_freeze_simq(sc->sim_1, 1);
}

void
mrsas_xpt_release(struct mrsas_softc *sc)
{
	xpt_release_simq(sc->sim_0, 1);
	xpt_release_simq(sc->sim_1, 1);
}

/*
 * mrsas_cmd_done:	Perform remaining command completion
 * input:			Adapter instance soft state  Pointer to command packet
 *
 * This function calls ummap request and releases the MPT command.
 */
void
mrsas_cmd_done(struct mrsas_softc *sc, struct mrsas_mpt_cmd *cmd)
{
	callout_stop(&cmd->cm_callout);
	mrsas_unmap_request(sc, cmd);
	mtx_lock(&sc->sim_lock);
	xpt_done(cmd->ccb_ptr);
	cmd->ccb_ptr = NULL;
	mtx_unlock(&sc->sim_lock);
	mrsas_release_mpt_cmd(cmd);
}

/*
 * mrsas_cam_poll:	Polling entry point
 * input:			Pointer to SIM
 *
 * This is currently a stub function.
 */
static void
mrsas_cam_poll(struct cam_sim *sim)
{
	int i;
	struct mrsas_softc *sc = (struct mrsas_softc *)cam_sim_softc(sim);

	if (sc->msix_vectors != 0){
		for (i=0; i<sc->msix_vectors; i++){
			mrsas_complete_cmd(sc, i);
		}
	} else {
		mrsas_complete_cmd(sc, 0);
	}
}

/*
 * mrsas_bus_scan:	Perform bus scan
 * input:			Adapter instance soft state
 *
 * This mrsas_bus_scan function is needed for FreeBSD 7.x.  Also, it should not
 * be called in FreeBSD 8.x and later versions, where the bus scan is
 * automatic.
 */
int
mrsas_bus_scan(struct mrsas_softc *sc)
{
	union ccb *ccb_0;
	union ccb *ccb_1;

	if ((ccb_0 = xpt_alloc_ccb()) == NULL) {
		return (ENOMEM);
	}
	if ((ccb_1 = xpt_alloc_ccb()) == NULL) {
		xpt_free_ccb(ccb_0);
		return (ENOMEM);
	}
	mtx_lock(&sc->sim_lock);
	if (xpt_create_path(&ccb_0->ccb_h.path, xpt_periph, cam_sim_path(sc->sim_0),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb_0);
		xpt_free_ccb(ccb_1);
		mtx_unlock(&sc->sim_lock);
		return (EIO);
	}
	if (xpt_create_path(&ccb_1->ccb_h.path, xpt_periph, cam_sim_path(sc->sim_1),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb_0);
		xpt_free_ccb(ccb_1);
		mtx_unlock(&sc->sim_lock);
		return (EIO);
	}
	mtx_unlock(&sc->sim_lock);
	xpt_rescan(ccb_0);
	xpt_rescan(ccb_1);

	return (0);
}

/*
 * mrsas_bus_scan_sim:	Perform bus scan per SIM
 * input:				adapter instance soft state
 *
 * This function will be called from Event handler on LD creation/deletion,
 * JBOD on/off.
 */
int
mrsas_bus_scan_sim(struct mrsas_softc *sc, struct cam_sim *sim)
{
	union ccb *ccb;

	if ((ccb = xpt_alloc_ccb()) == NULL) {
		return (ENOMEM);
	}
	mtx_lock(&sc->sim_lock);
	if (xpt_create_path(&ccb->ccb_h.path, xpt_periph, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		mtx_unlock(&sc->sim_lock);
		return (EIO);
	}
	mtx_unlock(&sc->sim_lock);
	xpt_rescan(ccb);

	return (0);
}
