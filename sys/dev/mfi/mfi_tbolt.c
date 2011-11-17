
 /*-
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *            Copyright 1994-2009 The FreeBSD Project.
 *            All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FREEBSD PROJECT OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY,OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mfi/mfi_tbolt.c,v 1.00 2010/06/30 16:00:00 Bharat Gusain Exp $");

#include "opt_mfi.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/selinfo.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/ioccom.h>
#include <sys/eventhandler.h>
#include <sys/callout.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

struct mfi_cmd_tbolt *mfi_tbolt_get_cmd(struct mfi_softc *sc);
union mfi_mpi2_request_descriptor *
mfi_tbolt_get_request_descriptor(struct mfi_softc *sc, uint16_t index);
void mfi_tbolt_complete_cmd(struct mfi_softc *sc);
int mfi_tbolt_build_io(struct mfi_softc *sc, struct mfi_command *mfi_cmd,
    struct mfi_cmd_tbolt *cmd);
static inline void mfi_tbolt_return_cmd(struct mfi_softc *sc,
    struct mfi_cmd_tbolt *cmd);
union mfi_mpi2_request_descriptor *mfi_tbolt_build_mpt_cmd(struct mfi_softc
    *sc, struct mfi_command *cmd);
uint8_t
mfi_build_mpt_pass_thru(struct mfi_softc *sc, struct mfi_command *mfi_cmd);
union mfi_mpi2_request_descriptor *mfi_build_and_issue_cmd(struct mfi_softc
    *sc, struct mfi_command *mfi_cmd);
int mfi_tbolt_is_ldio(struct mfi_command *mfi_cmd);
void mfi_tbolt_build_ldio(struct mfi_softc *sc, struct mfi_command *mfi_cmd,
    struct mfi_cmd_tbolt *cmd);
static int mfi_tbolt_make_sgl(struct mfi_softc *sc, struct mfi_command
    *mfi_cmd, pMpi25IeeeSgeChain64_t sgl_ptr, struct mfi_cmd_tbolt *cmd);
static int mfi_tbolt_build_cdb(struct mfi_softc *sc, struct mfi_command
    *mfi_cmd, uint8_t *cdb);
void
map_tbolt_cmd_status(struct mfi_command *mfi_cmd, uint8_t status,
     uint8_t ext_status);
static void mfi_issue_pending_cmds_again (struct mfi_softc *sc);
static void mfi_kill_hba (struct mfi_softc *sc);
static void mfi_process_fw_state_chg_isr(void *arg);
uint8_t mfi_tbolt_get_map_info(struct mfi_softc *sc);

#define MFI_FUSION_ENABLE_INTERRUPT_MASK	(0x00000008)

void
mfi_tbolt_enable_intr_ppc(struct mfi_softc *sc)
{
	//MFI_WRITE4(sc, MFI_ODCR0, 0xFFFFFFFF);
	MFI_WRITE4(sc, MFI_OMSK, ~MFI_FUSION_ENABLE_INTERRUPT_MASK);
	MFI_READ4(sc, MFI_OMSK);
}

void
mfi_tbolt_disable_intr_ppc(struct mfi_softc *sc)
{
	MFI_WRITE4(sc, MFI_OMSK, 0xFFFFFFFF);
	MFI_READ4(sc, MFI_OMSK);
}

int32_t
mfi_tbolt_read_fw_status_ppc(struct mfi_softc *sc)
{
	return MFI_READ4(sc, MFI_OSP0);
}

int32_t
mfi_tbolt_check_clear_intr_ppc(struct mfi_softc *sc)
{
	int32_t status, mfi_status = 0;

	status = MFI_READ4(sc, MFI_OSTS);

	if (status & 1)
	{
		MFI_WRITE4(sc, MFI_OSTS, status);
		MFI_READ4(sc, MFI_OSTS);
		if (status & MFI_STATE_CHANGE_INTERRUPT){
			mfi_status |= MFI_FIRMWARE_STATE_CHANGE;
		}

		return mfi_status;
	}
	if(!(status & MFI_FUSION_ENABLE_INTERRUPT_MASK))
		return 1;

	MFI_READ4(sc, MFI_OSTS);
	return 0;
}


void
mfi_tbolt_issue_cmd_ppc(struct mfi_softc *sc, bus_addr_t bus_add,
   uint32_t frame_cnt)
{
	bus_add |= (MFI_REQ_DESCRIPT_FLAGS_MFA
	    << MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	MFI_WRITE4(sc, MFI_IQPL, (uint32_t)bus_add);
	MFI_WRITE4(sc, MFI_IQPH, (uint32_t)((uint64_t)bus_add >> 32));
}

/**
 * mfi_tbolt_adp_reset - For controller reset
 * @regs: MFI register set
 */
int mfi_tbolt_adp_reset(struct mfi_softc *sc)
{
    int retry = 0, i = 0;
    int HostDiag;

    MFI_WRITE4(sc, MFI_WSR, 0xF);
    MFI_WRITE4(sc, MFI_WSR, 4);
    MFI_WRITE4(sc, MFI_WSR, 0xB);
    MFI_WRITE4(sc, MFI_WSR, 2);
    MFI_WRITE4(sc, MFI_WSR, 7);
    MFI_WRITE4(sc, MFI_WSR, 0xD);

    for (i=0;i<10000;i++) ;

    HostDiag = (uint32_t)MFI_READ4(sc, MFI_HDR);

    while ( !( HostDiag & DIAG_WRITE_ENABLE) )
    {
        for (i=0;i<1000;i++) ;
        HostDiag = (uint32_t)MFI_READ4(sc, MFI_HDR);
        device_printf(sc->mfi_dev,"ADP_RESET_TBOLT: retry time=%x, "
	    "hostdiag=%x\n", retry, HostDiag);

        if (retry++ >= 100)
            return 1;

    }

    device_printf(sc->mfi_dev,"ADP_RESET_TBOLT: HostDiag=%x\n", HostDiag);

    MFI_WRITE4(sc, MFI_HDR, (HostDiag | DIAG_RESET_ADAPTER));

    for (i=0; i < 10; i++) {
        for (i = 0; i < 10000; i++);
    }

    HostDiag = (uint32_t)MFI_READ4(sc, MFI_RSR);
    while (HostDiag & DIAG_RESET_ADAPTER)
    {
        for (i=0;i<1000;i++) ;
        HostDiag = (uint32_t)MFI_READ4(sc, MFI_RSR);
        device_printf(sc->mfi_dev,"ADP_RESET_TBOLT: retry time=%x, "
	    "hostdiag=%x\n", retry, HostDiag);

        if (retry++ >= 1000)
            return 1;
    }
    return 0;
}

/*
 *******************************************************************************************
 * Description:
 *      This routine initialize Thunderbolt specific device information
 *******************************************************************************************
 */
void mfi_tbolt_init_globals(struct mfi_softc *sc)
{
	/* Initialize single reply size and Message size */
	sc->reply_size = MEGASAS_THUNDERBOLT_REPLY_SIZE;
	sc->raid_io_msg_size = MEGASAS_THUNDERBOLT_NEW_MSG_SIZE;

	/*
	 * Calculating how many SGEs allowed in a allocated main message
	 * (size of the Message - Raid SCSI IO message size(except SGE))
	 * / size of SGE
	 * (0x100 - (0x90 - 0x10)) / 0x10 = 8
	 */
	sc->max_SGEs_in_main_message =
	    (uint8_t)((sc->raid_io_msg_size
	    - (sizeof(struct mfi_mpi2_request_raid_scsi_io)
	    - sizeof(MPI2_SGE_IO_UNION))) / sizeof(MPI2_SGE_IO_UNION));
	/*
	 * (Command frame size allocaed in SRB ext - Raid SCSI IO message size)
	 * / size of SGL ;
	 * (1280 - 256) / 16 = 64
	 */
	sc->max_SGEs_in_chain_message = (MR_COMMAND_SIZE
	    - sc->raid_io_msg_size) / sizeof(MPI2_SGE_IO_UNION);
	/*
	 * (0x08-1) + 0x40 = 0x47 - 0x01 = 0x46  one is left for command
	 * colscing
	*/
	sc->mfi_max_sge = (sc->max_SGEs_in_main_message - 1)
	    + sc->max_SGEs_in_chain_message - 1;
	/*
	* This is the offset in number of 4 * 32bit words to the next chain
	* (0x100 - 0x10)/0x10 = 0xF(15)
	*/
	sc->chain_offset_value_for_main_message = (sc->raid_io_msg_size
	    - sizeof(MPI2_SGE_IO_UNION))/16;
	sc->chain_offset_value_for_mpt_ptmsg
	    = offsetof(struct mfi_mpi2_request_raid_scsi_io, SGL)/16;
	sc->mfi_cmd_pool_tbolt = NULL;
	sc->request_desc_pool = NULL;


}

/*
 ****************************************************************************
 * Description:
 *      This function calculates the memory requirement for Thunderbolt
 *      controller
 * Return Value:
 *      Total required memory in bytes
 ****************************************************************************
 */

uint32_t mfi_tbolt_get_memory_requirement(struct mfi_softc *sc)
{
	uint32_t size;
	size = MEGASAS_THUNDERBOLT_MSG_ALLIGNMENT;	// for Alignment
	size += sc->raid_io_msg_size * (sc->mfi_max_fw_cmds + 1);
	size += sc->reply_size * sc->mfi_max_fw_cmds;
	// this is for SGL's
	size += MEGASAS_MAX_SZ_CHAIN_FRAME * sc->mfi_max_fw_cmds;
	return size;
}

/*
 ****************************************************************************
 * Description:
 *      This function will prepare message pools for the Thunderbolt controller
 * Arguments:
 *      DevExt - HBA miniport driver's adapter data storage structure
 *      pMemLocation - start of the memory allocated for Thunderbolt.
 * Return Value:
 *      TRUE if successful
 *      FALSE if failed
 ****************************************************************************
 */
int mfi_tbolt_init_desc_pool(struct mfi_softc *sc, uint8_t* mem_location,
    uint32_t tbolt_contg_length)
{
	uint32_t     offset = 0;
	uint8_t      *addr = mem_location;

	/* Request Descriptor Base physical Address */

	/* For Request Decriptors Virtual Memory */
	/* Initialise the aligned IO Frames Virtual Memory Pointer */
	if (((uintptr_t)addr) & (0xFF))	{
		addr = &addr[sc->raid_io_msg_size];
		addr = (uint8_t *)((uintptr_t)addr & (~0xFF));
		sc->request_message_pool_align = addr;
	} else
		sc->request_message_pool_align = addr;

	offset = sc->request_message_pool_align - sc->request_message_pool;
	sc->request_msg_busaddr = sc->mfi_tb_busaddr + offset;

	/* DJA XXX should this be bus dma ??? */
	/* Skip request message pool */
	addr = &addr[sc->raid_io_msg_size * (sc->mfi_max_fw_cmds + 1)];
	/* Reply Frame Pool is initialized */
	sc->reply_frame_pool = (struct mfi_mpi2_reply_header *) addr;
	if (((uintptr_t)addr) & (0xFF))	{
		addr = &addr[sc->reply_size];
		addr = (uint8_t *)((uintptr_t)addr & (~0xFF));
	}
	sc->reply_frame_pool_align
		    = (struct mfi_mpi2_reply_header *)addr;

	offset = (uintptr_t)sc->reply_frame_pool_align
	    - (uintptr_t)sc->request_message_pool;
	sc->reply_frame_busaddr = sc->mfi_tb_busaddr + offset;

	/* Skip Reply Frame Pool */
	addr += sc->reply_size * sc->mfi_max_fw_cmds;
	sc->reply_pool_limit = addr;

	/* initializing reply address to 0xFFFFFFFF */
	memset((uint8_t *)sc->reply_frame_pool, 0xFF,
	       (sc->reply_size * sc->mfi_max_fw_cmds));

	offset = sc->reply_size * sc->mfi_max_fw_cmds;
	sc->sg_frame_busaddr = sc->reply_frame_busaddr + offset;
	/* initialize the last_reply_idx to 0 */
	sc->last_reply_idx = 0;
	offset = (sc->sg_frame_busaddr + (MEGASAS_MAX_SZ_CHAIN_FRAME *
	    sc->mfi_max_fw_cmds)) - sc->mfi_tb_busaddr;
	if(offset > tbolt_contg_length)
		device_printf(sc->mfi_dev,"Error:Initialized more than "
		    "allocated\n");
	return 0;
}

/*
 ****************************************************************************
 * Description:
 *   This routine prepare and issue INIT2 frame to the Firmware
 ****************************************************************************
 */

int
mfi_tbolt_init_MFI_queue(struct mfi_softc *sc)
{
	struct MPI2_IOC_INIT_REQUEST   *mpi2IocInit;
	struct mfi_init_frame	*mfi_init;
	uintptr_t			offset = 0;
	bus_addr_t			phyAddress;
	MFI_ADDRESS			*mfiAddressTemp;
	struct mfi_command *cm;
	int error;

	mpi2IocInit = (struct MPI2_IOC_INIT_REQUEST *)sc->mfi_tb_ioc_init_desc;
	/* Check if initialization is already completed */
	if(sc->MFA_enabled) {
		return 1;
	}

	mtx_lock(&sc->mfi_io_lock);
	if ((cm = mfi_dequeue_free(sc)) == NULL) {
		mtx_unlock(&sc->mfi_io_lock);
		return (EBUSY);
	}
	cm->cm_frame = (union mfi_frame *)((uintptr_t)sc->mfi_tb_init);
	cm->cm_frame_busaddr = sc->mfi_tb_init_busaddr;
	cm->cm_dmamap = sc->mfi_tb_init_dmamap;
	cm->cm_frame->header.context = 0;
	cm->cm_sc = sc;
	cm->cm_index = 0;

	/*
	 * Abuse the SG list area of the frame to hold the init_qinfo
	 * object;
	 */
	mfi_init = &cm->cm_frame->init;

	bzero(mpi2IocInit, sizeof(struct MPI2_IOC_INIT_REQUEST));
	mpi2IocInit->Function  = MPI2_FUNCTION_IOC_INIT;
	mpi2IocInit->WhoInit   = MPI2_WHOINIT_HOST_DRIVER;

	/* set MsgVersion and HeaderVersion host driver was built with */
	mpi2IocInit->MsgVersion = MPI2_VERSION;
	mpi2IocInit->HeaderVersion = MPI2_HEADER_VERSION;
	mpi2IocInit->SystemRequestFrameSize = sc->raid_io_msg_size/4;
	mpi2IocInit->ReplyDescriptorPostQueueDepth
	    = (uint16_t)sc->mfi_max_fw_cmds;
	mpi2IocInit->ReplyFreeQueueDepth = 0; /* Not supported by MR. */

	/* Get physical address of reply frame pool */
	offset = (uintptr_t) sc->reply_frame_pool_align
	    - (uintptr_t)sc->request_message_pool;
	phyAddress = sc->mfi_tb_busaddr + offset;
	mfiAddressTemp =
	    (MFI_ADDRESS *)&mpi2IocInit->ReplyDescriptorPostQueueAddress;
	mfiAddressTemp->u.addressLow = (uint32_t)phyAddress;
	mfiAddressTemp->u.addressHigh = (uint32_t)((uint64_t)phyAddress >> 32);

	/* Get physical address of request message pool */
	offset = sc->request_message_pool_align - sc->request_message_pool;
	phyAddress =  sc->mfi_tb_busaddr + offset;
	mfiAddressTemp = (MFI_ADDRESS *)&mpi2IocInit->SystemRequestFrameBaseAddress;
	mfiAddressTemp->u.addressLow = (uint32_t)phyAddress;
	mfiAddressTemp->u.addressHigh = (uint32_t)((uint64_t)phyAddress >> 32);
	mpi2IocInit->ReplyFreeQueueAddress =  0; // Not supported by MR.
	mpi2IocInit->TimeStamp = time_uptime;

	if (sc->verbuf) {
		snprintf((char *)sc->verbuf, strlen(MEGASAS_VERSION) + 2, "%s\n",
                MEGASAS_VERSION);
		mfi_init->driver_ver_lo = (uint32_t)sc->verbuf_h_busaddr;
		mfi_init->driver_ver_hi =
		    (uint32_t)((uint64_t)sc->verbuf_h_busaddr >> 32);
	}
	/* Get the physical address of the mpi2 ioc init command */
	phyAddress =  sc->mfi_tb_ioc_init_busaddr;
	mfi_init->qinfo_new_addr_lo = (uint32_t)phyAddress;
	mfi_init->qinfo_new_addr_hi = (uint32_t)((uint64_t)phyAddress >> 32);
	mfi_init->header.flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	mfi_init->header.cmd = MFI_CMD_INIT;
	mfi_init->header.data_len = sizeof(struct MPI2_IOC_INIT_REQUEST);
	mfi_init->header.cmd_status = MFI_STAT_INVALID_STATUS;

	cm->cm_data = NULL;
	cm->cm_flags |= MFI_CMD_POLLED;
	cm->cm_timestamp = time_uptime;
	if ((error = mfi_mapcmd(sc, cm)) != 0) {
		device_printf(sc->mfi_dev, "failed to send IOC init2 "
		    "command %d at %lx\n", error, (long)cm->cm_frame_busaddr);
		mfi_release_command(cm);
		mtx_unlock(&sc->mfi_io_lock);
		return (error);
	}
	mfi_release_command(cm);
	mtx_unlock(&sc->mfi_io_lock);

	if(mfi_init->header.cmd_status == 0) {
		sc->MFA_enabled = 1;
	}
	else {
		device_printf(sc->mfi_dev, "Init command Failed %x\n",
		    mfi_init->header.cmd_status);
		return 1;
	}

	return 0;

}

int mfi_tbolt_alloc_cmd(struct mfi_softc *sc)
{
	struct mfi_cmd_tbolt *cmd;
	bus_addr_t io_req_base_phys;
	uint8_t *io_req_base;
	int i = 0, j = 0, offset = 0;

	/*
	 * sc->mfi_cmd_pool_tbolt is an array of struct mfi_cmd_tbolt pointers.
	 * Allocate the dynamic array first and then allocate individual
	 * commands.
	 */
	sc->request_desc_pool = malloc(sizeof(
	    union mfi_mpi2_request_descriptor) * sc->mfi_max_fw_cmds,
	    M_MFIBUF, M_NOWAIT|M_ZERO);
	sc->mfi_cmd_pool_tbolt = malloc(sizeof(struct mfi_cmd_tbolt*)
	    * sc->mfi_max_fw_cmds, M_MFIBUF, M_NOWAIT|M_ZERO);

	if (!sc->mfi_cmd_pool_tbolt) {
		device_printf(sc->mfi_dev, "out of memory. Could not alloc "
		    "memory for cmd_list_fusion\n");
		return 1;
	}

	for (i = 0; i < sc->mfi_max_fw_cmds; i++) {
		sc->mfi_cmd_pool_tbolt[i] = malloc(sizeof(
		    struct mfi_cmd_tbolt),M_MFIBUF, M_NOWAIT|M_ZERO);

		if (!sc->mfi_cmd_pool_tbolt[i]) {
			device_printf(sc->mfi_dev, "Could not alloc cmd list "
			    "fusion\n");

			for (j = 0; j < i; j++)
				free(sc->mfi_cmd_pool_tbolt[j], M_MFIBUF);

			free(sc->mfi_cmd_pool_tbolt, M_MFIBUF);
			sc->mfi_cmd_pool_tbolt = NULL;
		}
	}

	/*
	 * The first 256 bytes (SMID 0) is not used. Don't add to the cmd
	 *list
	 */
	io_req_base = sc->request_message_pool_align
		+ MEGASAS_THUNDERBOLT_NEW_MSG_SIZE;
	io_req_base_phys = sc->request_msg_busaddr
		+ MEGASAS_THUNDERBOLT_NEW_MSG_SIZE;

	/*
	 * Add all the commands to command pool (instance->cmd_pool)
	 */
	/* SMID 0 is reserved. Set SMID/index from 1 */

	for (i = 0; i < sc->mfi_max_fw_cmds; i++) {
		cmd = sc->mfi_cmd_pool_tbolt[i];
		offset = MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * i;
		cmd->index = i + 1;
		cmd->request_desc = (union mfi_mpi2_request_descriptor *)
		    (sc->request_desc_pool + i);
		cmd->io_request = (struct mfi_mpi2_request_raid_scsi_io *)
		    (io_req_base + offset);
		cmd->io_request_phys_addr = io_req_base_phys + offset;
		cmd->sg_frame = (MPI2_SGE_IO_UNION *)(sc->reply_pool_limit
		    + i * MEGASAS_MAX_SZ_CHAIN_FRAME);
		cmd->sg_frame_phys_addr = sc->sg_frame_busaddr + i
		    * MEGASAS_MAX_SZ_CHAIN_FRAME;

		TAILQ_INSERT_TAIL(&(sc->mfi_cmd_tbolt_tqh), cmd, next);
	}
	return 0;
}

int mfi_tbolt_reset(struct mfi_softc *sc)
{
	uint32_t fw_state;

	mtx_lock(&sc->mfi_io_lock);
	if (atomic_read(&sc->fw_reset_no_pci_access)){
		device_printf(sc->mfi_dev,"NO PCI ACCESS\n");
		mtx_unlock(&sc->mfi_io_lock);
		return 1;
	}

	if (sc->hw_crit_error){
		device_printf(sc->mfi_dev,"HW CRITICAL ERROR\n");
		mtx_unlock(&sc->mfi_io_lock);
		return 1;
	}

	if (sc->mfi_flags & MFI_FLAGS_TBOLT){
		fw_state = sc->mfi_read_fw_status(sc);
		if((fw_state & MFI_FWSTATE_FAULT) == MFI_FWSTATE_FAULT)
		{
			if((sc->disableOnlineCtrlReset == 0)
			    && (sc->adpreset == 0)){
				device_printf(sc->mfi_dev,"Adapter RESET "
				    "condition is detected\n");
				sc->adpreset = 1;
				sc->issuepend_done = 0;
				sc->MFA_enabled = 0;
				sc->last_reply_idx = 0;
				mfi_process_fw_state_chg_isr((void *) sc);
			}
			mtx_unlock(&sc->mfi_io_lock);
			return 0;
		}
	}
	mtx_unlock(&sc->mfi_io_lock);
	return 1;
}

/*
 * mfi_intr_tbolt - isr entry point
 */
void mfi_intr_tbolt(void *arg)
{
	struct mfi_softc *sc = (struct mfi_softc *)arg;

	if(sc->mfi_check_clear_intr(sc) == 1)
	{
		return;
	}
	if(sc->shutdown_issued)
		return;
	mtx_lock(&sc->mfi_io_lock);
	mfi_tbolt_complete_cmd(sc);
	if(sc->mfi_flags & MFI_FLAGS_QFRZN)
		sc->mfi_flags &= ~MFI_FLAGS_QFRZN;
	mfi_startio(sc);
	mtx_unlock(&sc->mfi_io_lock);
	return;
}

/**
 * map_cmd_status -	Maps FW cmd status to OS cmd status
 * @cmd :		Pointer to cmd
 * @status :		status of cmd returned by FW
 * @ext_status :	ext status of cmd returned by FW
 */

void
map_tbolt_cmd_status(struct mfi_command *mfi_cmd, uint8_t status,
    uint8_t ext_status)
{

	switch (status) {

		case MFI_STAT_OK:
			mfi_cmd->cm_frame->header.cmd_status = 0;
			mfi_cmd->cm_frame->dcmd.header.cmd_status = 0;
			break;

		case MFI_STAT_SCSI_IO_FAILED:
		case MFI_STAT_LD_INIT_IN_PROGRESS:
			mfi_cmd->cm_frame->header.cmd_status = status;
			mfi_cmd->cm_frame->header.scsi_status = ext_status;
			mfi_cmd->cm_frame->dcmd.header.cmd_status = status;
			mfi_cmd->cm_frame->dcmd.header.scsi_status
			    = ext_status;
			break;

		case MFI_STAT_SCSI_DONE_WITH_ERROR:
			mfi_cmd->cm_frame->header.cmd_status = ext_status;
			mfi_cmd->cm_frame->dcmd.header.cmd_status = ext_status;
			break;

		case MFI_STAT_LD_OFFLINE:
		case MFI_STAT_DEVICE_NOT_FOUND:
			mfi_cmd->cm_frame->header.cmd_status = status;
			mfi_cmd->cm_frame->dcmd.header.cmd_status = status;
			break;

		default:
			mfi_cmd->cm_frame->header.cmd_status = status;
			mfi_cmd->cm_frame->dcmd.header.cmd_status = status;
			break;
		}
}



void mfi_tbolt_complete_cmd(struct mfi_softc *sc)
{
	struct mfi_mpi2_reply_header *desc, *reply_desc;
	struct mfi_command *cmd_mfi;	/* For MFA Cmds */
	struct mfi_cmd_tbolt *cmd_tbolt;
	uint16_t smid;
	uint8_t reply_descript_type;
	struct mfi_mpi2_request_raid_scsi_io  *scsi_io_req;
	uint32_t status, extStatus;
	uint16_t num_completed;
	union desc_value val;

	desc = (struct mfi_mpi2_reply_header *)
		((uintptr_t)sc->reply_frame_pool_align
		+ sc->last_reply_idx * sc->reply_size);
	reply_desc = desc;

	if (!reply_desc)
		device_printf(sc->mfi_dev, "reply desc is NULL!!\n");

	reply_descript_type = reply_desc->ReplyFlags
	     & MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK;
	if (reply_descript_type == MPI2_RPY_DESCRIPT_FLAGS_UNUSED)
		return;

	num_completed = 0;
	val.word = ((union mfi_mpi2_reply_descriptor *)desc)->words;

	/* Read Reply descriptor */
	while ((val.u.low != 0xFFFFFFFF) && (val.u.high != 0xFFFFFFFF)) {

		smid = reply_desc->SMID;
		if (!smid || smid > sc->mfi_max_fw_cmds + 1) {
			device_printf(sc->mfi_dev, "smid is %x. Cannot "
			    "proceed. Returning \n",smid);
			return;
		}

		cmd_tbolt = sc->mfi_cmd_pool_tbolt[smid - 1];
		cmd_mfi = &sc->mfi_commands[cmd_tbolt->sync_cmd_idx];
		scsi_io_req = cmd_tbolt->io_request;

		/* Check if internal commands */
		status = cmd_mfi->cm_frame->dcmd.header.cmd_status;
		extStatus = cmd_mfi->cm_frame->dcmd.header.scsi_status;

		/*
		switch (scsi_io_req->Function)
		{
		case MPI2_FUNCTION_SCSI_IO_REQUEST :
			printf("HELLO MPI2_FUNCTION_SCSI_IO_REQUEST\n");
			break;
		case MPI2_FUNCTION_LD_IO_REQUEST :
			printf("HELLO MPI2_FUNCTION_LD_IO_REQUEST\n");
			break;
		case MPI2_FUNCTION_PASSTHRU_IO_REQUEST:
			printf("HELLO MPI2_FUNCTION_PASSTHRU_IO_REQUEST\n");
			break;
		default:
			printf("HELLO default\n");
			break;
		}
		*/

		switch (scsi_io_req->Function)
		{
		case MPI2_FUNCTION_LD_IO_REQUEST:
			/* Regular Path IO. */
			/* Map the Fw Error Status. */
			map_tbolt_cmd_status(cmd_mfi, status,
			    extStatus);
			if ((cmd_mfi->cm_frame->dcmd.opcode
			    == MFI_DCMD_LD_MAP_GET_INFO)
			    && (cmd_mfi->cm_frame->dcmd.mbox[1] == 1))
				{
					if (cmd_mfi->cm_frame->header.cmd_status
					    != 0)
						device_printf(sc->mfi_dev,"map sync failed\n");
					else {
						sc->map_id++;
						device_printf(sc->mfi_dev,"map sync completed\n");
						mfi_release_command(cmd_mfi);
					}
				}
			if ((cmd_mfi->cm_flags & MFI_ON_MFIQ_BUSY)
			    == MFI_ON_MFIQ_BUSY
			    && (cmd_mfi->cm_flags & MFI_CMD_POLLED) == 0) {
				/* BHARAT poll workaround */
				mfi_remove_busy(cmd_mfi);
				cmd_mfi->cm_error = 0;
				mfi_complete(sc, cmd_mfi);
			}
			mfi_tbolt_return_cmd(sc, cmd_tbolt);
			break;
		case MPI2_FUNCTION_PASSTHRU_IO_REQUEST:
			map_tbolt_cmd_status(cmd_mfi, status, extStatus);
			if ((cmd_mfi->cm_frame->dcmd.opcode
			    == MFI_DCMD_LD_MAP_GET_INFO)
			    && (cmd_mfi->cm_frame->dcmd.mbox[1] == 1)) {
				if(cmd_mfi->cm_frame->header.cmd_status != 0)
					device_printf(sc->mfi_dev,"map sync failed\n");
				else {
					sc->map_id++;
					device_printf(sc->mfi_dev,"map sync completed\n");
					mfi_release_command(cmd_mfi);
				}
			}
			if ((cmd_mfi->cm_flags & MFI_ON_MFIQ_BUSY)
			    == MFI_ON_MFIQ_BUSY
			    && (cmd_mfi->cm_flags & MFI_CMD_POLLED) == 0) {
				/* BHARAT poll workaround */
				mfi_remove_busy(cmd_mfi);
				cmd_mfi->cm_error = 0;
				mfi_complete(sc, cmd_mfi);
			}
			mfi_tbolt_return_cmd(sc, cmd_tbolt);
			break;
		}

		sc->last_reply_idx++;
		if (sc->last_reply_idx >= sc->mfi_max_fw_cmds) {
			MFI_WRITE4(sc, MFI_RPI, sc->last_reply_idx);
			sc->last_reply_idx = 0;
		}
		/*set it back to all 0xfff.*/
		((union mfi_mpi2_reply_descriptor*)desc)->words =
			~((uint64_t)0x00);

		num_completed++;

		/* Get the next reply descriptor */
		desc = (struct mfi_mpi2_reply_header *)
		    ((uintptr_t)sc->reply_frame_pool_align
		    + sc->last_reply_idx * sc->reply_size);
		reply_desc = desc;
		val.word = ((union mfi_mpi2_reply_descriptor*)desc)->words;
		reply_descript_type = reply_desc->ReplyFlags
		    & MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK;
		if(reply_descript_type == MPI2_RPY_DESCRIPT_FLAGS_UNUSED)
			break;
	}

	if (!num_completed)
		return;

	/* update replyIndex to FW */
	if(sc->last_reply_idx)
		MFI_WRITE4(sc, MFI_RPI, sc->last_reply_idx);

	return;
}

/**
 * mfi_get_cmd -	Get a command from the free pool
 * @instance:		Adapter soft state
 *
 * Returns a free command from the pool
 */

struct mfi_cmd_tbolt *mfi_tbolt_get_cmd(struct mfi_softc
						  *sc)
{
	struct mfi_cmd_tbolt *cmd = NULL;

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

	cmd = TAILQ_FIRST(&sc->mfi_cmd_tbolt_tqh);
	TAILQ_REMOVE(&sc->mfi_cmd_tbolt_tqh, cmd, next);
	memset((uint8_t *)cmd->sg_frame, 0, MEGASAS_MAX_SZ_CHAIN_FRAME);
	memset((uint8_t *)cmd->io_request, 0,
	    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE);
	return cmd;
}

/**
 * mfi_tbolt_return_cmd -	Return a cmd to free command pool
 * @instance:		Adapter soft state
 * @cmd:		Command packet to be returned to free command pool
 */
static inline void
mfi_tbolt_return_cmd(struct mfi_softc *sc, struct mfi_cmd_tbolt *cmd)
{
	mtx_assert(&sc->mfi_io_lock, MA_OWNED);

	TAILQ_INSERT_TAIL(&sc->mfi_cmd_tbolt_tqh, cmd, next);
}


union mfi_mpi2_request_descriptor *
mfi_tbolt_get_request_descriptor(struct mfi_softc *sc, uint16_t index)
{
	uint8_t *p;

	if (index >= sc->mfi_max_fw_cmds) {
		device_printf(sc->mfi_dev, "Invalid SMID (0x%x)request "
		    "for descriptor\n", index);
		return NULL;
	}
	p = sc->request_desc_pool + sizeof(union mfi_mpi2_request_descriptor)
	    * index;
	memset(p, 0, sizeof(union mfi_mpi2_request_descriptor));
	return (union mfi_mpi2_request_descriptor *)p;
}


// Used to build IOCTL cmd
uint8_t
mfi_build_mpt_pass_thru(struct mfi_softc *sc, struct mfi_command *mfi_cmd)
{
	MPI25_IEEE_SGE_CHAIN64 *mpi25_ieee_chain;
	struct mfi_mpi2_request_raid_scsi_io *io_req;
	struct mfi_cmd_tbolt *cmd;

	cmd = mfi_tbolt_get_cmd(sc);
	if (!cmd)
		return EBUSY;
	mfi_cmd->cm_extra_frames = cmd->index; // Frame count used as SMID
	cmd->sync_cmd_idx = mfi_cmd->cm_index;
	io_req = cmd->io_request;
	mpi25_ieee_chain = (MPI25_IEEE_SGE_CHAIN64 *)&io_req->SGL.IeeeChain;

	io_req->Function = MPI2_FUNCTION_PASSTHRU_IO_REQUEST;
	io_req->SGLOffset0 = offsetof(struct mfi_mpi2_request_raid_scsi_io,
	    SGL) / 4;
	io_req->ChainOffset = sc->chain_offset_value_for_mpt_ptmsg;

	mpi25_ieee_chain->Address = mfi_cmd->cm_frame_busaddr;

	/*
	  In MFI pass thru, nextChainOffset will always be zero to
	  indicate the end of the chain.
	*/
	mpi25_ieee_chain->Flags= MPI2_IEEE_SGE_FLAGS_CHAIN_ELEMENT
		| MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR;

	/* setting the length to the maximum length */
	mpi25_ieee_chain->Length = 1024;

	return 0;
}

void
mfi_tbolt_build_ldio(struct mfi_softc *sc, struct mfi_command *mfi_cmd,
    struct mfi_cmd_tbolt *cmd)
{
	uint32_t start_lba_lo = 0, start_lba_hi = 0, device_id;
	struct mfi_mpi2_request_raid_scsi_io	*io_request;
	struct IO_REQUEST_INFO io_info;

	device_id = mfi_cmd->cm_frame->io.header.target_id;
	io_request = cmd->io_request;
	io_request->RaidContext.TargetID = device_id;
	io_request->RaidContext.Status = 0;
	io_request->RaidContext.exStatus =0;

	start_lba_lo = mfi_cmd->cm_frame->io.lba_lo;
	start_lba_hi = mfi_cmd->cm_frame->io.lba_hi;

	memset(&io_info, 0, sizeof(struct IO_REQUEST_INFO));
	io_info.ldStartBlock = ((uint64_t)start_lba_hi << 32) | start_lba_lo;
	io_info.numBlocks = mfi_cmd->cm_frame->io.header.data_len;
	io_info.ldTgtId = device_id;
	if ((mfi_cmd->cm_frame->header.flags & MFI_FRAME_DIR_READ) ==
	    MFI_FRAME_DIR_READ)
		io_info.isRead = 1;

		io_request->RaidContext.timeoutValue
		     = MFI_FUSION_FP_DEFAULT_TIMEOUT;
		io_request->Function = MPI2_FUNCTION_LD_IO_REQUEST;
		io_request->DevHandle = device_id;
		cmd->request_desc->header.RequestFlags
		    = (MFI_REQ_DESCRIPT_FLAGS_LD_IO
		    << MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	if((io_request->IoFlags == 6) && (io_info.numBlocks == 0))
		io_request->RaidContext.RegLockLength = 0x100;
	io_request->DataLength = mfi_cmd->cm_frame->io.header.data_len
	    * MFI_SECTOR_LEN;
}

int mfi_tbolt_is_ldio(struct mfi_command *mfi_cmd)
{
	if(mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_READ
	    || mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
		return 1;
	else
		return 0;
}


int
mfi_tbolt_build_io(struct mfi_softc *sc, struct mfi_command *mfi_cmd, struct mfi_cmd_tbolt *cmd)
{
	uint32_t device_id;
	uint32_t sge_count;
	uint8_t cdb[32], cdb_len;

	memset(cdb, 0, 32);
	struct mfi_mpi2_request_raid_scsi_io *io_request = cmd->io_request;

	device_id = mfi_cmd->cm_frame->header.target_id;

	/* Have to build CDB here for TB as BSD don't have a scsi layer */
	if((cdb_len = mfi_tbolt_build_cdb(sc, mfi_cmd, cdb)) == 1)
		return 1;

	/* Just the CDB length,rest of the Flags are zero */
	io_request->IoFlags = cdb_len;
	memcpy(io_request->CDB.CDB32, cdb, 32);

	if (mfi_tbolt_is_ldio(mfi_cmd))
		mfi_tbolt_build_ldio(sc, mfi_cmd , cmd);
	else
		return 1;

	/*
	 * Construct SGL
	 */
	sge_count = mfi_tbolt_make_sgl(sc, mfi_cmd,
	    (pMpi25IeeeSgeChain64_t) &io_request->SGL, cmd);
	if (sge_count > sc->mfi_max_sge) {
		device_printf(sc->mfi_dev, "Error. sge_count (0x%x) exceeds "
		    "max (0x%x) allowed\n", sge_count, sc->mfi_max_sge);
		return 1;
	}
	io_request->RaidContext.numSGE = sge_count;
	io_request->SGLFlags = MPI2_SGE_FLAGS_64_BIT_ADDRESSING;

	if (mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
		io_request->Control = MPI2_SCSIIO_CONTROL_WRITE;
	else
		io_request->Control = MPI2_SCSIIO_CONTROL_READ;

	io_request->SGLOffset0 = offsetof(
	    struct mfi_mpi2_request_raid_scsi_io, SGL)/4;

	io_request->SenseBufferLowAddress = mfi_cmd->cm_sense_busaddr;
	io_request->SenseBufferLength = MFI_SENSE_LEN;
	return 0;
}

static int
mfi_tbolt_build_cdb(struct mfi_softc *sc, struct mfi_command *mfi_cmd,
    uint8_t *cdb)
{
	uint32_t lba_lo, lba_hi, num_lba;
	uint8_t cdb_len;

	if(mfi_cmd == NULL || cdb == NULL)
		return 1;
	num_lba = mfi_cmd->cm_frame->io.header.data_len;
	lba_lo = mfi_cmd->cm_frame->io.lba_lo;
	lba_hi = mfi_cmd->cm_frame->io.lba_hi;

	if((num_lba <= 0xFF) && (lba_lo <= 0x1FFFFF))
	{
		if (mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
			/* Read 6 or Write 6 */
			cdb[0] = (uint8_t) (0x0A);
		else
			cdb[0] = (uint8_t) (0x08);

		cdb[4] = (uint8_t) num_lba;
		cdb[3] = (uint8_t) (lba_lo & 0xFF);
		cdb[2] = (uint8_t) (lba_lo >> 8);
		cdb[1] = (uint8_t) ((lba_lo >> 16) & 0x1F);
		cdb_len = 6;
	}
	else if((num_lba <= 0xFFFF) && (lba_lo <= 0xFFFFFFFF))
	{
		if (mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
			/* Read 10 or Write 10 */
			cdb[0] = (uint8_t) (0x2A);
		else
			cdb[0] = (uint8_t) (0x28);
		cdb[8] = (uint8_t) (num_lba & 0xFF);
		cdb[7] = (uint8_t) (num_lba >> 8);
		cdb[5] = (uint8_t) (lba_lo & 0xFF);
		cdb[4] = (uint8_t) (lba_lo >> 8);
		cdb[3] = (uint8_t) (lba_lo >> 16);
		cdb[2] = (uint8_t) (lba_lo >> 24);
		cdb_len = 10;
	}
	else if((num_lba > 0xFFFF) && (lba_hi == 0))
	{
		if (mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
			/* Read 12 or Write 12 */
			cdb[0] = (uint8_t) (0xAA);
		else
			cdb[0] = (uint8_t) (0xA8);
		cdb[9] = (uint8_t) (num_lba & 0xFF);
		cdb[8] = (uint8_t) (num_lba >> 8);
		cdb[7] = (uint8_t) (num_lba >> 16);
		cdb[6] = (uint8_t) (num_lba >> 24);
		cdb[5] = (uint8_t) (lba_lo & 0xFF);
		cdb[4] = (uint8_t) (lba_lo >> 8);
		cdb[3] = (uint8_t) (lba_lo >> 16);
		cdb[2] = (uint8_t) (lba_lo >> 24);
		cdb_len = 12;
	}
	else
	{
		if (mfi_cmd->cm_frame->header.cmd == MFI_CMD_LD_WRITE)
			cdb[0] = (uint8_t) (0x8A);
		else
			cdb[0] = (uint8_t) (0x88);
		cdb[13] = (uint8_t) (num_lba & 0xFF);
                cdb[12] = (uint8_t) (num_lba >> 8);
                cdb[11] = (uint8_t) (num_lba >> 16);
                cdb[10] = (uint8_t) (num_lba >> 24);
		cdb[9] = (uint8_t) (lba_lo & 0xFF);
		cdb[8] = (uint8_t) (lba_lo >> 8);
                cdb[7] = (uint8_t) (lba_lo >> 16);
                cdb[6] = (uint8_t) (lba_lo >> 24);
		cdb[5] = (uint8_t) (lba_hi & 0xFF);
                cdb[4] = (uint8_t) (lba_hi >> 8);
                cdb[3] = (uint8_t) (lba_hi >> 16);
                cdb[2] = (uint8_t) (lba_hi >> 24);
		cdb_len = 16;
	}
	return cdb_len;
}

static int
mfi_tbolt_make_sgl(struct mfi_softc *sc, struct mfi_command *mfi_cmd,
		   pMpi25IeeeSgeChain64_t sgl_ptr,struct mfi_cmd_tbolt *cmd)
{
	uint8_t i, sg_processed,sg_to_process;
	uint8_t sge_count, sge_idx;
	union mfi_sgl *os_sgl;

	/*
	 * Return 0 if there is no data transfer
	 */
	if (!mfi_cmd->cm_sg || !mfi_cmd->cm_len) {
	 	device_printf(sc->mfi_dev, "Buffer empty \n");
		return 0;
	}
	os_sgl = mfi_cmd->cm_sg;
	sge_count = mfi_cmd->cm_frame->header.sg_count;

	if (sge_count > sc->mfi_max_sge) {
		device_printf(sc->mfi_dev, "sgl ptr %p sg_cnt %d \n",
		    os_sgl,sge_count);
		return sge_count;
	}

	if (sge_count > sc->max_SGEs_in_main_message)
		/* One element to store the chain info */
		sge_idx = sc->max_SGEs_in_main_message - 1;
	else
		sge_idx = sge_count;

	for (i = 0; i < sge_idx; i++) {
		/*
		  For 32bit BSD we are getting 32 bit SGL's from OS
		  but FW only take 64 bit SGL's so copying from 32 bit
		  SGL's to 64.
		*/
		/*if((sc->mfi_flags & MFI_FLAGS_SG64) == 0)
		{
			sgl_ptr->Length = (uint32_t) os_sgl->sg64[0].len;
			sgl_ptr->Address = (os_sgl->sg64[0].addr);
		}
		else*/
		if (sc->mfi_flags & MFI_FLAGS_SKINNY) {
			sgl_ptr->Length = os_sgl->sg_skinny[i].len;
			sgl_ptr->Address = os_sgl->sg_skinny[i].addr;
		} else {
			sgl_ptr->Length = os_sgl->sg32[i].len;
			sgl_ptr->Address = os_sgl->sg32[i].addr;
		}
		sgl_ptr->Flags = 0;
		sgl_ptr++;
		cmd->io_request->ChainOffset = 0;
	}

	sg_processed = i;

	if (sg_processed < sge_count) {
		pMpi25IeeeSgeChain64_t sg_chain;
		sg_to_process = sge_count - sg_processed;
		cmd->io_request->ChainOffset =
		    sc->chain_offset_value_for_main_message;
		sg_chain = sgl_ptr;
		/* Prepare chain element */
		sg_chain->NextChainOffset = 0;
		sg_chain->Flags = (MPI2_IEEE_SGE_FLAGS_CHAIN_ELEMENT |
		    MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR);
		sg_chain->Length =  (sizeof(MPI2_SGE_IO_UNION) *
		    (sge_count - sg_processed));
		sg_chain->Address = cmd->sg_frame_phys_addr;
		sgl_ptr = (pMpi25IeeeSgeChain64_t)cmd->sg_frame;
		for (; i < sge_count; i++) {
			if (sc->mfi_flags & MFI_FLAGS_SKINNY) {
				sgl_ptr->Length = os_sgl->sg_skinny[i].len;
				sgl_ptr->Address = os_sgl->sg_skinny[i].addr;
			} else {
				sgl_ptr->Length = os_sgl->sg32[i].len;
				sgl_ptr->Address = os_sgl->sg32[i].addr;
			}
			sgl_ptr->Flags = 0;
			sgl_ptr++;
		}
	}
	return sge_count;
}

union mfi_mpi2_request_descriptor *
mfi_build_and_issue_cmd(struct mfi_softc *sc, struct mfi_command *mfi_cmd)
{
	struct mfi_cmd_tbolt *cmd;
	union mfi_mpi2_request_descriptor *req_desc = NULL;
	uint16_t index;
	cmd = mfi_tbolt_get_cmd(sc);
	if (!cmd)
		return NULL;
	mfi_cmd->cm_extra_frames = cmd->index;
	cmd->sync_cmd_idx = mfi_cmd->cm_index;

	index = cmd->index;
	req_desc = mfi_tbolt_get_request_descriptor(sc, index-1);
	//req_desc->Words = 0;
	if(mfi_tbolt_build_io(sc, mfi_cmd, cmd))
		return NULL;
	req_desc->header.SMID = index;
	return req_desc;
}

union mfi_mpi2_request_descriptor *
mfi_tbolt_build_mpt_cmd(struct mfi_softc *sc, struct mfi_command *cmd)
{
	union mfi_mpi2_request_descriptor *req_desc = NULL;
	uint16_t index;
	if (mfi_build_mpt_pass_thru(sc, cmd)) {
		device_printf(sc->mfi_dev, "Couldn't build MFI pass thru "
		    "cmd\n");
		return NULL;
	}
	/* For fusion the frame_count variable is used for SMID */
	index = cmd->cm_extra_frames;

	req_desc = mfi_tbolt_get_request_descriptor(sc, index - 1);
	if(!req_desc)
		return NULL;

	bzero(req_desc, sizeof(req_desc));
	req_desc->header.RequestFlags = (MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO <<
	    MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	req_desc->header.SMID = index;
	return req_desc;
}

int
mfi_tbolt_send_frame(struct mfi_softc *sc, struct mfi_command *cm)
{
	struct mfi_frame_header *hdr;
	uint8_t *cdb;
	union mfi_mpi2_request_descriptor *req_desc = NULL;
	int tm = MFI_POLL_TIMEOUT_SECS * 1000;

	hdr = &cm->cm_frame->header;
	cdb = cm->cm_frame->pass.cdb;
	if(sc->adpreset)
		return 1;
	if ((cm->cm_flags & MFI_CMD_POLLED) == 0) {
		cm->cm_timestamp = time_uptime;
		mfi_enqueue_busy(cm);
	}
	else {
		hdr->cmd_status = 0xff;
		hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;
	}

	if (hdr->cmd == MFI_CMD_PD_SCSI_IO) {
		/* check for inquiry commands coming from CLI */
		if (cdb[0] != 0x28 || cdb[0] != 0x2A) {
			if ((req_desc = mfi_tbolt_build_mpt_cmd(sc, cm)) ==
			    NULL) {
				device_printf(sc->mfi_dev, "Mapping from MFI "
				    "to MPT Failed \n");
				return 1;
			}
		}
		else
			device_printf(sc->mfi_dev, "DJA NA XXX SYSPDIO\n");
	}
	else if (hdr->cmd == MFI_CMD_LD_SCSI_IO ||
	    hdr->cmd == MFI_CMD_LD_READ || hdr->cmd == MFI_CMD_LD_WRITE) {
		if ((req_desc = mfi_build_and_issue_cmd(sc, cm)) == NULL) {
			device_printf(sc->mfi_dev, "LDIO Failed \n");
			return 1;
		}
	} else
		if ((req_desc = mfi_tbolt_build_mpt_cmd(sc, cm)) == NULL) {
			device_printf(sc->mfi_dev, "Mapping from MFI to MPT "
			    "Failed\n");
			return 1;
		}
	MFI_WRITE4(sc, MFI_ILQP, (req_desc->words & 0xFFFFFFFF));
	MFI_WRITE4(sc, MFI_IHQP, (req_desc->words >>0x20));

	if ((cm->cm_flags & MFI_CMD_POLLED) == 0)
		return 0;

	/* This is a polled command, so busy-wait for it to complete. */
	while (hdr->cmd_status == 0xff) {
		DELAY(1000);
		tm -= 1;
		if (tm <= 0)
			break;
	}

	if (hdr->cmd_status == 0xff) {
		device_printf(sc->mfi_dev, "Frame %p timed out "
		      "command 0x%X\n", hdr, cm->cm_frame->dcmd.opcode);
		return (ETIMEDOUT);
	}
	return 0;
}

static void mfi_issue_pending_cmds_again (struct mfi_softc *sc)
{
	struct mfi_command *cm,*tmp;

	mtx_assert(&sc->mfi_io_lock, MA_OWNED);
	TAILQ_FOREACH_REVERSE_SAFE(cm, &sc->mfi_busy, BUSYQ, cm_link, tmp){

		cm->retry_for_fw_reset++;

		/*
		 * If a command has continuously been tried multiple times
		 * and causing a FW reset condition, no further recoveries
		 * should be performed on the controller
		 */
		if (cm->retry_for_fw_reset == 3) {
			device_printf(sc->mfi_dev,"megaraid_sas: command %d "
			    "was tried multiple times during adapter reset"
			    "Shutting down the HBA\n", cm->cm_index);
			mfi_kill_hba(sc);
			sc->hw_crit_error = 1;
			return;
		}

		if ((cm->cm_flags & MFI_ON_MFIQ_BUSY) != 0) {
			struct mfi_cmd_tbolt *cmd;
			mfi_remove_busy(cm);
			cmd = sc->mfi_cmd_pool_tbolt[cm->cm_extra_frames -
			    1 ];
			mfi_tbolt_return_cmd(sc, cmd);
			if ((cm->cm_flags & MFI_ON_MFIQ_MASK) == 0) {
				if (cm->cm_frame->dcmd.opcode !=
				    MFI_DCMD_CTRL_EVENT_WAIT) {
					device_printf(sc->mfi_dev,
					    "APJ ****requeue command %d \n",
					    cm->cm_index);
					mfi_requeue_ready(cm);
				}
			}
			else
				mfi_release_command(cm);
		}
	}
	mfi_startio(sc);
}

static void mfi_kill_hba (struct mfi_softc *sc)
{
	if (sc->mfi_flags & MFI_FLAGS_TBOLT)
		MFI_WRITE4 (sc, 0x00,MFI_STOP_ADP);
	else
		MFI_WRITE4 (sc, MFI_IDB,MFI_STOP_ADP);
}

static void mfi_process_fw_state_chg_isr(void *arg)
{
	struct mfi_softc *sc= (struct mfi_softc *)arg;
	struct mfi_cmd_tbolt *cmd;
	int error, status;

	if (sc->adpreset == 1) {
		device_printf(sc->mfi_dev,"First stage of FW reset "
		     "initiated...\n");

		sc->mfi_adp_reset(sc);
		sc->mfi_enable_intr(sc);

		device_printf(sc->mfi_dev,"First stage of reset complete, "
		    "second stage initiated...\n");

		sc->adpreset = 2;

		/* waiting for about 20 second before start the second init */
		for(int wait = 0; wait < 20000; wait++)
			DELAY(1000);
		device_printf(sc->mfi_dev,"Second stage of FW reset "
		     "initiated...\n");
		while((status = MFI_READ4(sc, MFI_RSR)) & 0x04);

		sc->mfi_disable_intr(sc);

		/* We expect the FW state to be READY */
		if (mfi_transition_firmware(sc)) {
			device_printf(sc->mfi_dev,"controller is not in ready "
			    "state\n");
			mfi_kill_hba(sc);
			sc->hw_crit_error= 1;
			return ;
		}
		if ((error = mfi_tbolt_init_MFI_queue(sc)) != 0)
				return;

		mtx_lock(&sc->mfi_io_lock);

		sc->mfi_enable_intr(sc);
		sc->adpreset = 0;
		free(sc->mfi_aen_cm->cm_data, M_MFIBUF);
		mfi_remove_busy(sc->mfi_aen_cm);
		cmd = sc->mfi_cmd_pool_tbolt[sc->mfi_aen_cm->cm_extra_frames
		    - 1];
		mfi_tbolt_return_cmd(sc, cmd);
		if (sc->mfi_aen_cm) {
			mfi_release_command(sc->mfi_aen_cm);
			sc->mfi_aen_cm = NULL;
		}
		if (sc->map_update_cmd) {
			mfi_release_command(sc->map_update_cmd);
			sc->map_update_cmd = NULL;
		}
		mfi_issue_pending_cmds_again(sc);

		/*
		 * Issue pending command can result in adapter being marked
		 * dead because of too many re-tries. Check for that
		 * condition before clearing the reset condition on the FW
		 */
		if (!sc->hw_crit_error) {
			/*
			 * Initiate AEN (Asynchronous Event Notification)
			 */
			mfi_aen_setup(sc, sc->last_seq_num);
			sc->issuepend_done = 1;
			device_printf(sc->mfi_dev,"second stage of reset "
			    "complete, FW is ready now.\n");
		} else {
			device_printf(sc->mfi_dev,"second stage of reset "
			     "never completed, hba was marked offline.\n");
		}
	} else {
		device_printf(sc->mfi_dev, "mfi_process_fw_state_chg_isr "
		    "called with unhandled value:%d\n", sc->adpreset);
	}
	mtx_unlock(&sc->mfi_io_lock);
}

