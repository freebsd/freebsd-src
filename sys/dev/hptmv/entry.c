/*-
 * Copyright (c) 2003-2004 HighPoint Technologies, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/callout.h>
#include <sys/signalvar.h>
#include <sys/eventhandler.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/hptmv/global.h>
#include <dev/hptmv/hptintf.h>
#include <dev/hptmv/osbsd.h>
#include <contrib/dev/hptmv/access601.h>

#ifdef DEBUG
#ifdef DEBUG_LEVEL
int hpt_dbg_level = DEBUG_LEVEL;
#else
int hpt_dbg_level = 0;
#endif
#endif

#define MV_ERROR printf
/*
 * CAM SIM entry points
 */
static int	hpt_probe(device_t dev);
static int	hpt_attach(device_t dev);
static int	hpt_detach(device_t dev);
static int	hpt_shutdown(device_t dev);
static void	hpt_poll(struct cam_sim *sim);
static void	hpt_intr(void *arg);
static void	hpt_action(struct cam_sim *sim, union ccb *ccb);
static void	SetInquiryData(PINQUIRYDATA inquiryData, PVDevice pVDev);
static void	HPTLIBAPI OsSendCommand (_VBUS_ARG union ccb * ccb);
static void	HPTLIBAPI fOsCommandDone(_VBUS_ARG PCommand pCmd);
static void	ccb_done(union ccb *ccb);
static void	hpt_queue_ccb(union ccb **ccb_Q, union ccb *ccb);
static void	hpt_free_ccb(union ccb **ccb_Q, union ccb *ccb);
static void	launch_worker_thread(void);
static MV_SATA_CHANNEL gMvSataChannels[MAX_VBUS][MV_SATA_CHANNELS_NUM];
static void	hptmv_free_edma_queues(IAL_ADAPTER_T *pAdapter);
static void	hptmv_free_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum);
static void	handleEdmaError(_VBUS_ARG PCommand pCmd);
static int	hptmv_init_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum);
static int	fResetActiveCommands(PVBus _vbus_p);
static void	fRegisterVdevice(IAL_ADAPTER_T *pAdapter);
static int	hptmv_allocate_edma_queues(IAL_ADAPTER_T *pAdapter);
static void	hptmv_handle_event_disconnect(void *data);
static void	hptmv_handle_event_connect(void *data);
static int	start_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum);
static void	init_vdev_params(IAL_ADAPTER_T *pAdapter, MV_U8 channel);
static int	hptmv_parse_identify_results(MV_SATA_CHANNEL *pMvSataChannel);
static void	hpt_async(void *callback_arg, u_int32_t code,
    struct cam_path *path, void *arg);
static int HPTLIBAPI fOsBuildSgl(_VBUS_ARG PCommand pCmd, FPSCAT_GATH pSg,
    int logical);
static MV_BOOLEAN CommandCompletionCB(MV_SATA_ADAPTER *pMvSataAdapter,
    MV_U8 channelNum, MV_COMPLETION_TYPE comp_type, MV_VOID_PTR commandId,
    MV_U16 responseFlags, MV_U32 timeStamp,
    MV_STORAGE_DEVICE_REGISTERS *registerStruct);
static MV_BOOLEAN hptmv_event_notify(MV_SATA_ADAPTER *pMvSataAdapter,
    MV_EVENT_TYPE eventType, MV_U32 param1, MV_U32 param2);

#define ccb_ccb_ptr spriv_ptr0
#define ccb_adapter ccb_h.spriv_ptr1

IAL_ADAPTER_T *gIal_Adapter = 0;
IAL_ADAPTER_T *pCurAdapter = 0;

typedef struct st_HPT_DPC {
	IAL_ADAPTER_T *pAdapter;
	void (*dpc)(IAL_ADAPTER_T *, void *, UCHAR);
	void *arg;
	UCHAR flags;
} ST_HPT_DPC;

#define MAX_DPC 16
UCHAR DPC_Request_Nums = 0; 
static ST_HPT_DPC DpcQueue[MAX_DPC];
static int DpcQueue_First=0;
static int DpcQueue_Last = 0;

static device_method_t driver_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hpt_probe),
	DEVMETHOD(device_attach,	hpt_attach),
	DEVMETHOD(device_detach,	hpt_detach),
	{ 0, 0 }
};

static driver_t hpt_pci_driver = {
	__str(PROC_DIR_NAME),
	driver_methods,
	sizeof(IAL_ADAPTER_T)
};

static devclass_t	hpt_devclass;

DRIVER_MODULE(PROC_DIR_NAME, pci, hpt_pci_driver, hpt_devclass, 0, 0);
MODULE_DEPEND(PROC_DIR_NAME, cam, 1, 1, 1);

intrmask_t
lock_driver()
{
	intrmask_t spl = splcam();
	return spl;
}

void
unlock_driver(intrmask_t spl)
{
	splx(spl);
}

/*******************************************************************************
 *	Name:	hptmv_free_channel
 *
 *	Description:	free allocated queues for the given channel
 *
 *	Parameters:    	pMvSataAdapter - pointer to the RR182x controler this 
 * 					channel connected to. 
 *			channelNum - channel number. 
 *     
 ******************************************************************************/
static void
hptmv_free_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum)
{
	PVDevice pVDev = &(pAdapter->VDevices[channelNum]);
	_VBUS_INST(&pAdapter->VBus);

	HPT_ASSERT(channelNum < MV_SATA_CHANNELS_NUM);

	pAdapter->mvSataAdapter.sataChannel[channelNum] = NULL;

	if(pVDev->vf_online)
	{
		pVDev->u.disk.df_on_line = 0;
		pVDev->vf_online = 0;
		if (pVDev->pfnDeviceFailed) {
			CallWhenIdle(_VBUS_P (DPC_PROC)pVDev->pfnDeviceFailed,
				     pVDev);
		}
	}
}

int MvSataResetChannel(MV_SATA_ADAPTER *pMvSataAdapter, MV_U8 channel);

static void
handleEdmaError(_VBUS_ARG PCommand pCmd)
{
	PDevice pDevice = &pCmd->pVDevice->u.disk;
	MV_SATA_ADAPTER * pSataAdapter = pDevice->mv->mvSataAdapter;

	MV_ERROR("Reset channel\n");

	MvSataResetChannel(pSataAdapter, pDevice->mv->channelNumber);
	/*now no other cmds on this channel*/
	if (!pDevice->df_on_line) {
		KdPrint(("Device is offline"));
		pCmd->Result = RETURN_BAD_DEVICE;
		CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);	
		return;
	}

	if (pCmd->RetryCount++>5) {
		pDevice->df_on_line = 0;
		pCmd->pVDevice->vf_online = 0;
		if (pCmd->pVDevice->pfnDeviceFailed) 
			CallWhenIdle(_VBUS_P
			    (DPC_PROC)pCmd->pVDevice->pfnDeviceFailed,
			    pCmd->pVDevice);
		fNotifyGUI(ET_DEVICE_REMOVED, Map2pVDevice(pDevice));
		pCmd->Result = RETURN_IDE_ERROR;
		CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);	
		return;
	}
	/* retry the command */
	fDeviceSendCommand(_VBUS_P pCmd);
}

/****************************************************************
 *	Name:	hptmv_init_channel
 *
 *	Description:	allocate request and response queues for the EDMA of
 *                      the given channel and sets other fields.
 *	Parameters:    	
 *		pAdapter - pointer to the emulated adapter data structure
 *		channelNum - channel number. 
 *	Return:	0 on success, otherwise on failure
 ****************************************************************/
static int
hptmv_init_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum)
{
	MV_SATA_CHANNEL *pMvSataChannel;
	dma_addr_t    req_dma_addr;
	dma_addr_t    rsp_dma_addr;

	if (channelNum >= MV_SATA_CHANNELS_NUM)
	{
		MV_ERROR("RR182x[%d]: Bad channelNum=%d",
				 pAdapter->mvSataAdapter.adapterId, channelNum);
		return -1;
	}

	pMvSataChannel =
	    &gMvSataChannels[pAdapter->mvSataAdapter.adapterId][channelNum];
	pAdapter->mvSataAdapter.sataChannel[channelNum] = pMvSataChannel;
	pMvSataChannel->channelNumber = channelNum;
	pMvSataChannel->lba48Address = MV_FALSE;
	pMvSataChannel->maxReadTransfer = MV_FALSE;

	pMvSataChannel->requestQueue =
	    (struct mvDmaRequestQueueEntry *)
	    (pAdapter->requestsArrayBaseAlignedAddr +
	    (channelNum * MV_EDMA_REQUEST_QUEUE_SIZE));
	req_dma_addr = pAdapter->requestsArrayBaseDmaAlignedAddr +
	    (channelNum * MV_EDMA_REQUEST_QUEUE_SIZE);


	KdPrint(("requestQueue addr is 0x%lX", (u_long)req_dma_addr));

	/* check the 1K alignment of the request queue*/
	if (req_dma_addr & 0x3ff)
	{
		MV_ERROR("RR182x[%d]: request queue allocated not 1 K aligned,"
			 " dma_addr=%lx channel=%d\n",
			 pAdapter->mvSataAdapter.adapterId,(u_long)req_dma_addr,
			 channelNum);
		return -1;
	}
	pMvSataChannel->requestQueuePciLowAddress = req_dma_addr;
	pMvSataChannel->requestQueuePciHiAddress = 0;
	KdPrint(("RR182x[%d,%d]: request queue allocated: 0x%p",
		pAdapter->mvSataAdapter.adapterId, channelNum,
		pMvSataChannel->requestQueue));
	pMvSataChannel->responseQueue =
	    (struct mvDmaResponseQueueEntry *)
	    (pAdapter->responsesArrayBaseAlignedAddr +
	    (channelNum * MV_EDMA_RESPONSE_QUEUE_SIZE));
	rsp_dma_addr = pAdapter->responsesArrayBaseDmaAlignedAddr +
	    (channelNum * MV_EDMA_RESPONSE_QUEUE_SIZE);

	/* check the 256 alignment of the response queue*/
	if (rsp_dma_addr & 0xff)
	{
		MV_ERROR("RR182x[%d,%d]: response queue allocated not 256 byte"
			 " aligned, dma_addr=%lx\n",
			 pAdapter->mvSataAdapter.adapterId, channelNum,
			 (u_long)rsp_dma_addr);
		return -1;
	}
	pMvSataChannel->responseQueuePciLowAddress = rsp_dma_addr;
	pMvSataChannel->responseQueuePciHiAddress = 0;
	KdPrint(("RR182x[%d,%d]: response queue allocated: 0x%p",
		pAdapter->mvSataAdapter.adapterId, channelNum,
		pMvSataChannel->responseQueue));

	pAdapter->mvChannel[channelNum].online = MV_TRUE;
	return 0;
}

/******************************************************************************
 *	Name: hptmv_parse_identify_results
 *
 *	Description:	this functions parses the identify command results,
 *			checks that the connected deives can be accesed by
 *			RR182x EDMA, and updates the channel stucture
 *			accordingly.
 *	Parameters:     pMvSataChannel, pointer to the channel data structure.
 *
 *	Returns:       	=0 ->success, < 0 ->failure.
 *
 ******************************************************************************/
static int
hptmv_parse_identify_results(MV_SATA_CHANNEL *pMvSataChannel)
{
	MV_U16  *iden = pMvSataChannel->identifyDevice;

	/*LBA addressing*/
	if (! (iden[IDEN_CAPACITY_1_OFFSET] & 0x200)) {
		KdPrint(("IAL Error in IDENTIFY info: LBA not supported\n"));
		return -1;
	} else {
		KdPrint(("%25s - %s\n", "Capabilities", "LBA supported"));
	}
	/*DMA support*/
	if (! (iden[IDEN_CAPACITY_1_OFFSET] & 0x100)) {
		KdPrint(("IAL Error in IDENTIFY info: DMA not supported\n"));
		return -1;
	} else {
		KdPrint(("%25s - %s\n", "Capabilities", "DMA supported"));
	}
	/* PIO */
	if ((iden[IDEN_VALID] & 2) == 0) {
		KdPrint(("IAL Error in IDENTIFY info: not able to find PIO "
			"mode\n"));
		return -1;
	}
	KdPrint(("%25s - 0x%02x\n", "PIO modes supported",
			  iden[IDEN_PIO_MODE_SPPORTED] & 0xff));

	/*UDMA*/
	if ((iden[IDEN_VALID] & 4) == 0) {
		KdPrint(("IAL Error in IDENTIFY info: not able to find UDMA "
			"mode\n"));
		return -1;
	}

	/* 48 bit address */
	if ((iden[IDEN_SUPPORTED_COMMANDS2] & 0x400)) {
		KdPrint(("%25s - %s\n", "LBA48 addressing", "supported"));
		pMvSataChannel->lba48Address = MV_TRUE;
	} else {
		KdPrint(("%25s - %s\n", "LBA48 addressing", "Not supported"));
		pMvSataChannel->lba48Address = MV_FALSE;
	}
	return 0;
}

static void
init_vdev_params(IAL_ADAPTER_T *pAdapter, MV_U8 channel)
{
	PVDevice pVDev;
	MV_SATA_CHANNEL *pMvSataChannel;
	MV_U16_PTR IdentifyData;

	pVDev = &pAdapter->VDevices[channel];
	pMvSataChannel = pAdapter->mvSataAdapter.sataChannel[channel];
	pMvSataChannel->outstandingCommands = 0;
	IdentifyData = pMvSataChannel->identifyDevice;

	pVDev->u.disk.mv         = pMvSataChannel;
	pVDev->u.disk.df_on_line = 1;
	pVDev->u.disk.pVBus      = &pAdapter->VBus;
	pVDev->pVBus             = &pAdapter->VBus;

#ifdef SUPPORT_48BIT_LBA
	if (pMvSataChannel->lba48Address == MV_TRUE)
		pVDev->u.disk.dDeRealCapacity =
		    ((IdentifyData[101]<<16) | IdentifyData[100]) - 1;
	else
#endif
	if(IdentifyData[53] & 1) {
		pVDev->u.disk.dDeRealCapacity = 
		    (((IdentifyData[58]<<16 | IdentifyData[57]) <
		    (IdentifyData[61]<<16 | IdentifyData[60])) ? 
		    (IdentifyData[61]<<16 | IdentifyData[60]) :
		    (IdentifyData[58]<<16 | IdentifyData[57])) - 1;
	} else
		pVDev->u.disk.dDeRealCapacity = 
		    (IdentifyData[61]<<16 | IdentifyData[60]) - 1;

	pVDev->u.disk.bDeUsable_Mode = pVDev->u.disk.bDeModeSetting = 
	    pAdapter->mvChannel[channel].maxPioModeSupported -
	    MV_ATA_TRANSFER_PIO_0;

	if (pAdapter->mvChannel[channel].maxUltraDmaModeSupported!=0xFF) {
		pVDev->u.disk.bDeUsable_Mode = pVDev->u.disk.bDeModeSetting = 
		    pAdapter->mvChannel[channel].maxUltraDmaModeSupported -
		    MV_ATA_TRANSFER_UDMA_0 + 8;
	}
}

static void
device_change(IAL_ADAPTER_T *pAdapter , MV_U8 channelIndex, int plugged)
{
	PVDevice pVDev;
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_SATA_CHANNEL *pMvSataChannel;
	PVBus _vbus_p;

	pMvSataAdapter = &pAdapter->mvSataAdapter;
	pMvSataChannel = pMvSataAdapter->sataChannel[channelIndex];
	_vbus_p  = &pAdapter->VBus;

	if (!pMvSataChannel)
		return;

	if (plugged) {
		pVDev = &(pAdapter->VDevices[channelIndex]);
		init_vdev_params(pAdapter, channelIndex);

		pVDev->VDeviceType = pVDev->u.disk.df_atapi ? VD_ATAPI : 
		    pVDev->u.disk.df_removable_drive ? VD_REMOVABLE :
		    VD_SINGLE_DISK;

		pVDev->VDeviceCapacity = pVDev->u.disk.dDeRealCapacity;
		pVDev->pfnSendCommand = pfnSendCommand[pVDev->VDeviceType];
		pVDev->pfnDeviceFailed = pfnDeviceFailed[pVDev->VDeviceType];
		pVDev->vf_online = 1;

#ifdef SUPPORT_ARRAY
		if(pVDev->pParent) {
			int iMember;

			for (iMember = 0;
			     iMember < pVDev->pParent->u.array.bArnMember;
			     iMember++)
				if ((PVDevice)pVDev->pParent->u.array.pMember[iMember] == pVDev)
					pVDev->pParent->u.array.pMember[iMember] = NULL;
			pVDev->pParent = NULL;
		}
#endif
		fNotifyGUI(ET_DEVICE_PLUGGED,pVDev);
		fCheckBootable(pVDev);
		RegisterVDevice(pVDev);

#ifndef FOR_DEMO
		if (pAdapter->beeping) {
			pAdapter->beeping = 0;
			BeepOff(pAdapter->mvSataAdapter.adapterIoBaseAddress);
		}
#endif

	} else {
		pVDev  = &(pAdapter->VDevices[channelIndex]);
		pVDev->u.disk.df_on_line = 0;
		pVDev->vf_online = 0;
		if (pVDev->pfnDeviceFailed) {
			_VBUS_INST(&pAdapter->VBus)
			CallWhenIdle(_VBUS_P (DPC_PROC)pVDev->pfnDeviceFailed,
				     pVDev);
		}
		fNotifyGUI(ET_DEVICE_REMOVED,pVDev);

#ifndef FOR_DEMO
		if (pAdapter->ver_601==2 && !pAdapter->beeping) {
			pAdapter->beeping = 1;
			BeepOn(pAdapter->mvSataAdapter.adapterIoBaseAddress);
			set_fail_led(&pAdapter->mvSataAdapter, channelIndex, 1);
		}
#endif

	}
}

static int
start_channel(IAL_ADAPTER_T *pAdapter, MV_U8 channelNum)
{
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_SATA_CHANNEL *pMvSataChannel;
	MV_CHANNEL *pChannelInfo;
	MV_U32 udmaMode,pioMode;

	pMvSataAdapter = &pAdapter->mvSataAdapter;
	pMvSataChannel = pMvSataAdapter->sataChannel[channelNum];
	pChannelInfo = &(pAdapter->mvChannel[channelNum]);

	KdPrint(("RR182x [%d]: start channel (%d)", pMvSataAdapter->adapterId, 
		channelNum));


	/* Software reset channel */
	if (mvStorageDevATASoftResetDevice(pMvSataAdapter, channelNum) ==
	    MV_FALSE) {
		MV_ERROR("RR182x [%d,%d]: failed to perform Software reset\n",
			 pMvSataAdapter->adapterId, channelNum);
		return -1;
	}

	/* Hardware reset channel */
	if (mvSataChannelHardReset(pMvSataAdapter, channelNum) == MV_FALSE) {
		/*
		 * If failed, try again - this is when trying to hardreset a
		 * channel when drive is just spinning up
		 */
		StallExec(5000000); /* wait 5 sec before trying again */
		if (mvSataChannelHardReset(pMvSataAdapter, channelNum) ==
		    MV_FALSE) {
			MV_ERROR("RR182x [%d,%d]: failed to perform Hard "
				 "reset\n", pMvSataAdapter->adapterId,
				 channelNum);
			return -1;
		}
	}

	/* identify device*/
	if (mvStorageDevATAIdentifyDevice(pMvSataAdapter, channelNum) ==
	    MV_FALSE) {
		MV_ERROR("RR182x [%d,%d]: failed to perform ATA Identify "
			 "command\n", pMvSataAdapter->adapterId, channelNum);
		return -1;
	}
	if (hptmv_parse_identify_results(pMvSataChannel)) {
		MV_ERROR("RR182x [%d,%d]: Error in parsing ATA Identify "
			 "message\n", pMvSataAdapter->adapterId, channelNum);
		return -1;
	}

	/* mvStorageDevATASetFeatures */
	/* Disable 8 bit PIO in case CFA enabled */
	if (pMvSataChannel->identifyDevice[86] & 4) {
		KdPrint(("RR182x [%d]: Disable 8 bit PIO (CFA enabled) \n",
			pMvSataAdapter->adapterId));
		if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
		    MV_ATA_SET_FEATURES_DISABLE_8_BIT_PIO, 0, 0, 0, 0) ==
		    MV_FALSE) {
			MV_ERROR("RR182x [%d]: channel %d: "
				 "mvStorageDevATASetFeatures failed\n",
				 pMvSataAdapter->adapterId, channelNum); 
			return -1;
		}
	}

#ifdef ENABLE_WRITE_CACHE
	/* Write cache */
	if (pMvSataChannel->identifyDevice[82] & 0x20) {
		if (!(pMvSataChannel->identifyDevice[85] & 0x20)) {
			/* if not enabled by default */
			if (mvStorageDevATASetFeatures(pMvSataAdapter,
			    channelNum, MV_ATA_SET_FEATURES_ENABLE_WCACHE, 0,
			    0, 0, 0) == MV_FALSE) {
				MV_ERROR("RR182x [%d]: channel %d: "
					 "mvStorageDevATASetFeatures failed\n",
					 pMvSataAdapter->adapterId, channelNum);
				return -1;
			}
		}
		KdPrint(("RR182x [%d]: channel %d, write cache enabled\n",
			pMvSataAdapter->adapterId, channelNum));
	} else {
		KdPrint(("RR182x [%d]: channel %d, write cache not supported\n",
			pMvSataAdapter->adapterId, channelNum));
	}
#else
	/* disable write cache */
	if (pMvSataChannel->identifyDevice[85] & 0x20) {
		KdPrint(("RR182x [%d]: channel =%d, disable write cache\n",
			pMvSataAdapter->adapterId, channelNum));
		if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
		    MV_ATA_SET_FEATURES_DISABLE_WCACHE, 0, 0, 0, 0) ==
		    MV_FALSE) {
			MV_ERROR("RR182x [%d]: channel %d: "
				 "mvStorageDevATASetFeatures failed\n",
				 pMvSataAdapter->adapterId, channelNum); 
			return -1;
		}
	}
	KdPrint(("RR182x [%d]: channel=%d, write cache disabled\n",
		pMvSataAdapter->adapterId, channelNum));
#endif

	/* Set transfer mode */
	KdPrint(("RR182x [%d] Set transfer mode XFER_PIO_SLOW\n",
		pMvSataAdapter->adapterId));
	if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
	    MV_ATA_SET_FEATURES_TRANSFER, MV_ATA_TRANSFER_PIO_SLOW, 0, 0, 0) == 
	    MV_FALSE) {
		MV_ERROR("RR182x [%d] channel %d: Set Features failed\n",
			 pMvSataAdapter->adapterId, channelNum); 
		return -1;
	}

	if (pMvSataChannel->identifyDevice[IDEN_PIO_MODE_SPPORTED] & 1) {
		pioMode = MV_ATA_TRANSFER_PIO_4;
	} else if (pMvSataChannel->identifyDevice[IDEN_PIO_MODE_SPPORTED] & 2) {
		pioMode = MV_ATA_TRANSFER_PIO_3;
	} else {
		MV_ERROR("IAL Error in IDENTIFY info: PIO modes 3 and 4 not "
			 "supported\n");
		pioMode = MV_ATA_TRANSFER_PIO_SLOW;
	}

	KdPrint(("RR182x [%d] Set transfer mode XFER_PIO_4\n",
		pMvSataAdapter->adapterId));
	pAdapter->mvChannel[channelNum].maxPioModeSupported = pioMode;
	if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
	    MV_ATA_SET_FEATURES_TRANSFER, pioMode, 0, 0, 0) == MV_FALSE) {
		MV_ERROR("RR182x [%d] channel %d: Set Features failed\n",
			 pMvSataAdapter->adapterId, channelNum); 
		return -1;
	}

	udmaMode = MV_ATA_TRANSFER_UDMA_0;
	if (pMvSataChannel->identifyDevice[IDEN_UDMA_MODE] & 0x40) {
		udmaMode =  MV_ATA_TRANSFER_UDMA_6;
	} else if (pMvSataChannel->identifyDevice[IDEN_UDMA_MODE] & 0x20) {
		udmaMode =  MV_ATA_TRANSFER_UDMA_5;
	} else if (pMvSataChannel->identifyDevice[IDEN_UDMA_MODE] & 0x10) {
		udmaMode =  MV_ATA_TRANSFER_UDMA_4;
	} else if (pMvSataChannel->identifyDevice[IDEN_UDMA_MODE] & 8) {
		udmaMode =  MV_ATA_TRANSFER_UDMA_3;
	} else if (pMvSataChannel->identifyDevice[IDEN_UDMA_MODE] & 4) {
		udmaMode =  MV_ATA_TRANSFER_UDMA_2;
	}

	KdPrint(("RR182x [%d] Set transfer mode XFER_UDMA_%d\n",
		pMvSataAdapter->adapterId, udmaMode & 0xf));
	pChannelInfo->maxUltraDmaModeSupported = udmaMode;

#if 0
	if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
	    MV_ATA_SET_FEATURES_TRANSFER, udmaMode, 0, 0, 0) == MV_FALSE) {
		MV_ERROR("RR182x [%d] channel %d: Set Features failed\n",
			 pMvSataAdapter->adapterId, channelNum); 
		return -1;
	}
#endif
	if (pChannelInfo->maxUltraDmaModeSupported == 0xFF) 
		return TRUE;

	do {
		if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
		    MV_ATA_SET_FEATURES_TRANSFER, 
		    pChannelInfo->maxUltraDmaModeSupported, 0, 0, 0) !=
		    MV_FALSE) {
			break;
		}

		if (pChannelInfo->maxUltraDmaModeSupported <=
		    MV_ATA_TRANSFER_UDMA_0) {
			return FALSE;
		}
		if (mvStorageDevATASoftResetDevice(pMvSataAdapter,
		    channelNum) == MV_FALSE) {
			mv_reg_write_byte(pMvSataAdapter->adapterIoBaseAddress,
			    pMvSataChannel->eDmaRegsOffset + 0x11c,
			    /* command reg */ MV_ATA_COMMAND_IDLE_IMMEDIATE); 
			mvMicroSecondsDelay(10000);
			mvSataChannelHardReset(pMvSataAdapter, channelNum);
			if (mvStorageDevATASoftResetDevice(pMvSataAdapter,
			    channelNum) == MV_FALSE)
				return FALSE;
		}
		if (mvSataChannelHardReset(pMvSataAdapter, channelNum) ==
		    MV_FALSE)
			return FALSE;
		pChannelInfo->maxUltraDmaModeSupported--;
	} while (1);

#ifdef ENABLE_READ_AHEAD
	/* Read look ahead */
	if (pMvSataChannel->identifyDevice[82] & 0x40) {
		if (!(pMvSataChannel->identifyDevice[85] & 0x40)) {
			/* if not enabled by default */
			if (mvStorageDevATASetFeatures(pMvSataAdapter,
			    channelNum, MV_ATA_SET_FEATURES_ENABLE_RLA, 0, 0,
			    0, 0) == MV_FALSE) {
				MV_ERROR("RR182x [%d] channel %d: Set Features "
					 "failed\n", pMvSataAdapter->adapterId,
					 channelNum); 
				return -1;
			}
		}
		KdPrint(("RR182x [%d]: channel=%d, read look ahead enabled\n", 
			pMvSataAdapter->adapterId, channelNum));
	} else {
		KdPrint(("RR182x [%d]: channel %d, Read Look Ahead not "
			"supported\n", pMvSataAdapter->adapterId, channelNum));
	}
#else
	if (pMvSataChannel->identifyDevice[86] & 0x20) {
		KdPrint(("RR182x [%d]:channel %d, disable read look ahead\n",
			pMvSataAdapter->adapterId, channelNum));
		if (mvStorageDevATASetFeatures(pMvSataAdapter, channelNum,
		    MV_ATA_SET_FEATURES_DISABLE_RLA, 0, 0, 0, 0) == MV_FALSE) {
			MV_ERROR("RR182x [%d]:channel %d:  ATA Set Features "
				 "failed\n", pMvSataAdapter->adapterId,
				 channelNum); 
			return -1;
		}
	}
	KdPrint(("RR182x [%d]:channel %d, read look ahead disabled\n",
		  pMvSataAdapter->adapterId, channelNum));
#endif

#if 0
	KdPrint(("RR182x [%d]:channel %d, Set standby timer to 200 seconds\n",
			  pMvSataAdapter->adapterId, channelNum));
	if (mvStorageDevATAExecuteNonUDMACommand(pMvSataAdapter, channelNum,
						 MV_NON_UDMA_PROTOCOL_NON_DATA,
						 MV_FALSE,	/* isEXT*/
						 NULL, 0, 0,	/* features*/
						 40,		/*sectorCount*/
						 0,		/*lbaLow*/
						 0,		/*lbaMid*/
						 0,		/*lbaHigh*/
	 					 0,		/*device*/
						 MV_ATA_COMMAND_IDLE) ==
						 MV_FALSE) {
		MV_ERROR("RR182x [%d]:channel %d:  ATA Idle command failed\n",
			 pMvSataAdapter->adapterId, channelNum); 
		return -1;
	}
#endif

#if 0
	/* 2003-9-16 disable TCQ until we have better solution */
	if ((pMvSataChannel->identifyDevice[IDEN_SUPPORTED_COMMANDS2] & 2)) {
		MV_U8       depth;
		MV_BOOLEAN  result;

		depth = (pMvSataChannel->identifyDevice[IDEN_QUEUE_DEPTH] &
		    0x1f) + 1;
		KdPrint(("RR182x [%d]: channel %d config EDMA, Queued Mode, "
			"queue depth %d\n", pMvSataAdapter->adapterId,
			channelNum, depth));
		result = mvSataConfigEdmaMode(pMvSataAdapter, channelNum,
		    MV_EDMA_MODE_QUEUED, depth);
		if (result == MV_FALSE) {
			MV_ERROR("RR182x [%d] Error: mvSataConfigEdmaMode "
				 "failed\n", pMvSataAdapter->adapterId);
			return -1;
		}
	} else {
#endif
	KdPrint(("RR182x [%d]: channel %d config EDMA, Non Queued Mode\n",
		pMvSataAdapter->adapterId, channelNum));
	if (mvSataConfigEdmaMode(pMvSataAdapter, channelNum,
	    MV_EDMA_MODE_NOT_QUEUED, 0) == MV_FALSE) {
		MV_ERROR("RR182x [%d] channel %d Error: mvSataConfigEdmaMode "
			 "failed\n", pMvSataAdapter->adapterId, channelNum);
		return -1;
	}

	/* Enable EDMA */
	if (mvSataEnableChannelDma(pMvSataAdapter, channelNum) == MV_FALSE) {
		MV_ERROR("RR182x [%d] Failed to enable DMA, channel=%d\n",
			 pMvSataAdapter->adapterId, channelNum);
		return -1;
	}
	MV_ERROR("RR182x [%d,%d]: channel started successfully\n",
		 pMvSataAdapter->adapterId, channelNum);

#ifndef FOR_DEMO
	set_fail_led(pMvSataAdapter, channelNum, 0);
#endif
	return 0;
}

static void
hptmv_handle_event(void * data, int flag)
{
	IAL_ADAPTER_T *pAdapter;
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_U8 channelIndex;

	pAdapter = (IAL_ADAPTER_T *)data;
	pMvSataAdapter = &pAdapter->mvSataAdapter;

	mvOsSemTake(&pMvSataAdapter->semaphore);
	for (channelIndex = 0; channelIndex < MV_SATA_CHANNELS_NUM;
	     channelIndex++) {
		switch(pAdapter->sataEvents[channelIndex]) {
		case SATA_EVENT_CHANNEL_CONNECTED:
			/* Handle only connects */
			if (flag == 1)
				break;
			KdPrint(("RR182x [%d,%d]: new device connected\n",
				pMvSataAdapter->adapterId, channelIndex));
			hptmv_init_channel(pAdapter, channelIndex);
			if (mvSataConfigureChannel( pMvSataAdapter,
			    channelIndex) == MV_FALSE) {
				MV_ERROR("RR182x [%d,%d] Failed to configure\n",
					 pMvSataAdapter->adapterId,
					 channelIndex);
				hptmv_free_channel(pAdapter, channelIndex);
			} else {
#if 0
				mvSataChannelHardReset(pMvSataAdapter, channel);
#endif
				if (start_channel( pAdapter, channelIndex)) {
					MV_ERROR("RR182x [%d,%d]Failed to start"
						 " channel\n",
						 pMvSataAdapter->adapterId,
						 channelIndex);
					hptmv_free_channel(pAdapter,
					    channelIndex);
				} else {
					device_change(pAdapter, channelIndex,
					    TRUE);
				}
			}
			pAdapter->sataEvents[channelIndex] =
			    SATA_EVENT_NO_CHANGE;
			break;

		case SATA_EVENT_CHANNEL_DISCONNECTED:
			/* Handle only disconnects */
			if (flag == 0)
				break;
			KdPrint(("RR182x [%d,%d]: device disconnected\n",
				 pMvSataAdapter->adapterId, channelIndex));
				/* Flush pending commands */
			if(pMvSataAdapter->sataChannel[channelIndex]) {
				_VBUS_INST(&pAdapter->VBus)
				mvSataFlushDmaQueue (pMvSataAdapter,
				    channelIndex, MV_FLUSH_TYPE_CALLBACK);
				CheckPendingCall(_VBUS_P0);
				mvSataRemoveChannel(pMvSataAdapter,
				    channelIndex);
				hptmv_free_channel(pAdapter, channelIndex);
				pMvSataAdapter->sataChannel[channelIndex] =
				    NULL;
				KdPrint(("RR182x [%d,%d]: channel removed\n",
					pMvSataAdapter->adapterId,
					channelIndex));
				if (pAdapter->outstandingCommands==0 &&
				    DPC_Request_Nums==0)
					Check_Idle_Call(pAdapter);
				} else {
				KdPrint(("RR182x [%d,%d]: channel already "
					"removed!!\n",
					pMvSataAdapter->adapterId,
					channelIndex));
			}
			pAdapter->sataEvents[channelIndex] =
			    SATA_EVENT_NO_CHANGE;
			break;
				
		case SATA_EVENT_NO_CHANGE:
			break;

		default:
			break;
		}
	}
	mvOsSemRelease(&pMvSataAdapter->semaphore);
}

#define EVENT_CONNECT					1
#define EVENT_DISCONNECT				0

static void
hptmv_handle_event_connect(void *data)
{
	hptmv_handle_event (data, 0);
}

static void
hptmv_handle_event_disconnect(void *data)
{
	hptmv_handle_event (data, 1);
}

static MV_BOOLEAN
hptmv_event_notify(MV_SATA_ADAPTER *pMvSataAdapter, MV_EVENT_TYPE eventType,
		   MV_U32 param1, MV_U32 param2)
{
	IAL_ADAPTER_T *pAdapter = pMvSataAdapter->IALData;

	switch (eventType) {
	case MV_EVENT_TYPE_SATA_CABLE:
	{
		MV_U8   channel = param2;

		if (param1 == EVENT_CONNECT) {
			pAdapter->sataEvents[channel] =
			    SATA_EVENT_CHANNEL_CONNECTED;
			KdPrint(("RR182x [%d,%d]: device connected event "
				"received\n", pMvSataAdapter->adapterId,
				channel));
			/*
			 * Delete previous timers (if multiple drives connected
			 * in the same time
			 */
			pAdapter->event_timer_connect =
			    timeout(hptmv_handle_event_connect, pAdapter,10*hz);
		} else if (param1 == EVENT_DISCONNECT) {
			pAdapter->sataEvents[channel] =
			    SATA_EVENT_CHANNEL_DISCONNECTED;
			KdPrint(("RR182x [%d,%d]: device disconnected event "
				"received \n", pMvSataAdapter->adapterId,
				channel));
			device_change(pAdapter, channel, FALSE);
			/*
			 * Delete previous timers (if multiple drives
			 * disconnected in the same time
			 */
			pAdapter->event_timer_disconnect =
			    timeout(hptmv_handle_event_disconnect, pAdapter,
			    10*hz);
		} else {
			MV_ERROR("RR182x: illigal value for param1(%d) at "
				 "connect/disconect event, host=%d\n", param1,
				 pMvSataAdapter->adapterId );

		}
		break;
	}
	case MV_EVENT_TYPE_ADAPTER_ERROR:
		KdPrint(("RR182x: DEVICE error event received, pci cause "
			  "reg=%x,  don't how to handle this\n", param1));
		return MV_TRUE;
	default:
		MV_ERROR("RR182x[%d]: unknown event type (%d)\n",
			 pMvSataAdapter->adapterId, eventType);
		return MV_FALSE;
	}
	return MV_TRUE;
}

static void
hptmv_map_req(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	dma_addr_t *addr;

	addr = (dma_addr_t *)arg;

	if (error || nsegs != 1)
		return;

	*addr = segs[0].ds_addr;

	return;
}

static int
hptmv_allocate_edma_queues(IAL_ADAPTER_T *pAdapter)
{
	if (bus_dmamem_alloc(pAdapter->req_dmat,
	    (void **)&pAdapter->requestsArrayBaseAddr, BUS_DMA_WAITOK,
	    &pAdapter->req_map) != 0) {
		MV_ERROR("RR182x[%d]: Failed to allocate memory for EDMA "
		    "request queues\n", pAdapter->mvSataAdapter.adapterId);
		return -1;
	}
	
	(void)bus_dmamap_load(pAdapter->req_dmat, pAdapter->req_map,
	    pAdapter->requestsArrayBaseAddr, REQUESTS_ARRAY_SIZE, hptmv_map_req,
	    &pAdapter->requestsArrayBaseDmaAddr, 0);

	pAdapter->requestsArrayBaseAlignedAddr =
	    pAdapter->requestsArrayBaseAddr;
	pAdapter->requestsArrayBaseAlignedAddr += MV_EDMA_REQUEST_QUEUE_SIZE;
	pAdapter->requestsArrayBaseAlignedAddr =
	    (MV_U8 *)(((ULONG_PTR)pAdapter->requestsArrayBaseAlignedAddr) &
	    ~(ULONG_PTR)(MV_EDMA_REQUEST_QUEUE_SIZE - 1));
	pAdapter->requestsArrayBaseDmaAlignedAddr =
	    pAdapter->requestsArrayBaseDmaAddr; 
	pAdapter->requestsArrayBaseDmaAlignedAddr += MV_EDMA_REQUEST_QUEUE_SIZE;
	pAdapter->requestsArrayBaseDmaAlignedAddr &=
	    ~(ULONG_PTR)(MV_EDMA_REQUEST_QUEUE_SIZE - 1);

	if ((pAdapter->requestsArrayBaseDmaAlignedAddr -
	    pAdapter->requestsArrayBaseDmaAddr) != 
	    (pAdapter->requestsArrayBaseAlignedAddr -
	    pAdapter->requestsArrayBaseAddr)) {
		MV_ERROR("RR182x[%d]: Error in Request Quueues Alignment\n",
			 pAdapter->mvSataAdapter.adapterId);
		bus_dmamap_unload(pAdapter->req_dmat, pAdapter->req_map);
		bus_dmamem_free(pAdapter->req_dmat,
		    pAdapter->requestsArrayBaseAddr, pAdapter->req_map);
		return -1;
	}
	/* response queues */
	if (bus_dmamem_alloc(pAdapter->resp_dmat,
	    (void **)&pAdapter->responsesArrayBaseAddr, BUS_DMA_WAITOK,
	    &pAdapter->resp_map) != 0) {
		MV_ERROR("RR182x[%d]: Failed to allocate memory for EDMA "
		    "response queues\n", pAdapter->mvSataAdapter.adapterId);
		bus_dmamap_unload(pAdapter->req_dmat, pAdapter->req_map);
		bus_dmamem_free(pAdapter->req_dmat,
		    pAdapter->requestsArrayBaseAddr, pAdapter->req_map);
		return -1;
	}
	
	(void)bus_dmamap_load(pAdapter->resp_dmat, pAdapter->resp_map,
	    pAdapter->responsesArrayBaseAddr, RESPONSES_ARRAY_SIZE,
	    hptmv_map_req, &pAdapter->responsesArrayBaseDmaAddr, 0);

	pAdapter->responsesArrayBaseAlignedAddr =
	    pAdapter->responsesArrayBaseAddr;
	pAdapter->responsesArrayBaseAlignedAddr += MV_EDMA_RESPONSE_QUEUE_SIZE;
	pAdapter->responsesArrayBaseAlignedAddr =
	    (MV_U8 *)(((ULONG_PTR)pAdapter->responsesArrayBaseAlignedAddr) &
	    ~(ULONG_PTR)(MV_EDMA_RESPONSE_QUEUE_SIZE - 1));
	pAdapter->responsesArrayBaseDmaAlignedAddr =
	    pAdapter->responsesArrayBaseDmaAddr; 
	pAdapter->responsesArrayBaseDmaAlignedAddr +=
	    MV_EDMA_RESPONSE_QUEUE_SIZE;
	pAdapter->responsesArrayBaseDmaAlignedAddr &=
	    ~(ULONG_PTR)(MV_EDMA_RESPONSE_QUEUE_SIZE - 1);

	if ((pAdapter->responsesArrayBaseDmaAlignedAddr -
	    pAdapter->responsesArrayBaseDmaAddr) != 
	    (pAdapter->responsesArrayBaseAlignedAddr -
	    pAdapter->responsesArrayBaseAddr)) {
		MV_ERROR("RR182x[%d]: Error in Response Quueues Alignment\n",
			 pAdapter->mvSataAdapter.adapterId);
		hptmv_free_edma_queues(pAdapter);
		return -1;
	}
	return 0;
}

static void
hptmv_free_edma_queues(IAL_ADAPTER_T *pAdapter)
{
	bus_dmamap_unload(pAdapter->req_dmat, pAdapter->req_map);
	bus_dmamem_free(pAdapter->req_dmat, pAdapter->requestsArrayBaseAddr,
	    pAdapter->req_map);
	bus_dmamap_unload(pAdapter->resp_dmat, pAdapter->resp_map);
	bus_dmamem_free(pAdapter->resp_dmat, pAdapter->responsesArrayBaseAddr,
	    pAdapter->resp_map);
}

static PVOID
AllocatePRDTable(IAL_ADAPTER_T *pAdapter)
{
	PVOID ret;
	if (pAdapter->pFreePRDLink) {
		KdPrint(("pAdapter->pFreePRDLink:%p\n",
		    pAdapter->pFreePRDLink));
		ret = pAdapter->pFreePRDLink;
		pAdapter->pFreePRDLink = *(void**)ret;
		return ret;
	}
	return NULL;
}

static void
FreePRDTable(IAL_ADAPTER_T *pAdapter, PVOID PRDTable)
{
	*(void**)PRDTable = pAdapter->pFreePRDLink;
	pAdapter->pFreePRDLink = PRDTable;
}

extern PVDevice fGetFirstChild(PVDevice pLogical);
extern void fResetBootMark(PVDevice pLogical);
static void
fRegisterVdevice(IAL_ADAPTER_T *pAdapter)
{
	PVDevice pPhysical, pLogical;
	PVBus  pVBus;
	int i,j;

	for(i = 0; i < MV_SATA_CHANNELS_NUM; i++) {
		pPhysical = &(pAdapter->VDevices[i]);
		pLogical = pPhysical;
		while (pLogical->pParent) pLogical = pLogical->pParent;
		if (pLogical->vf_online==0) {
			pPhysical->vf_bootmark = pLogical->vf_bootmark = 0;
			continue;
		}
		if (pLogical->VDeviceType == VD_SPARE ||
		    pPhysical != fGetFirstChild(pLogical)) 
			continue;

		pVBus = &pAdapter->VBus;
		if(pVBus) {
			j=0;
			while(j < MAX_VDEVICE_PER_VBUS && pVBus->pVDevice[j])
				j++;
			if (j < MAX_VDEVICE_PER_VBUS) {
				pVBus->pVDevice[j] = pLogical; 
				pLogical->pVBus = pVBus;

				if (j>0 && pLogical->vf_bootmark) {
					if (pVBus->pVDevice[0]->vf_bootmark) {
						fResetBootMark(pLogical);
					} else {
						do {
							pVBus->pVDevice[j] =
							   pVBus->pVDevice[j-1];
						} while (--j);
						pVBus->pVDevice[0] = pLogical;
					}
				}
			}
		}
	}
}

PVDevice
GetSpareDisk(_VBUS_ARG PVDevice pArray)
{
	IAL_ADAPTER_T *pAdapter;
	ULONG capacity;
	ULONG thiscap, maxcap = MAX_LBA_T;
	PVDevice pVDevice, pFind = NULL;
	int i;

	pAdapter = (IAL_ADAPTER_T *)pArray->pVBus->OsExt;
	capacity =
	    LongDiv(pArray->VDeviceCapacity, pArray->u.array.bArnMember-1);
	for (i = 0;i < MV_SATA_CHANNELS_NUM; i++) {
		pVDevice = &pAdapter->VDevices[i];
		if(!pVDevice) 
			continue;
		thiscap = pArray->vf_format_v2 ?
		   pVDevice->u.disk.dDeRealCapacity : pVDevice->VDeviceCapacity;
		/* find the smallest usable spare disk */
		if (pVDevice->VDeviceType==VD_SPARE &&
		    pVDevice->u.disk.df_on_line && thiscap < maxcap &&
		    thiscap >= capacity) {
			maxcap = pVDevice->VDeviceCapacity;
			pFind = pVDevice;			
		}
	}
	return pFind;
}

/******************************************************************
 * IO ATA Command
 *******************************************************************/
int HPTLIBAPI
fDeReadWrite(PDevice pDev, ULONG Lba, UCHAR Cmd, void *tmpBuffer)
{
	return mvReadWrite(pDev->mv, Lba, Cmd, tmpBuffer);
}

void HPTLIBAPI fDeSelectMode(PDevice pDev, UCHAR NewMode)
{
#ifndef SIMULATE	
	MV_SATA_CHANNEL *pSataChannel;
	MV_SATA_ADAPTER *pSataAdapter;
	MV_U8 channelIndex;
	UCHAR mvMode;

	pSataChannel = pDev->mv;
	pSataAdapter = pSataChannel->mvSataAdapter;
	channelIndex = pSataChannel->channelNumber;

	/* 508x don't use MW-DMA? */
	if (NewMode>4 && NewMode<8) NewMode = 4;
	pDev->bDeModeSetting = NewMode;
	if (NewMode<=4)
		mvMode = MV_ATA_TRANSFER_PIO_0 + NewMode;
	else
		mvMode = MV_ATA_TRANSFER_UDMA_0 + (NewMode-8);

	/*To fix 88i8030 bug*/
	if (mvMode > MV_ATA_TRANSFER_UDMA_0 && mvMode < MV_ATA_TRANSFER_UDMA_4)
		mvMode = MV_ATA_TRANSFER_UDMA_0;

	mvSataDisableChannelDma(pSataAdapter, channelIndex);
	/* Flush pending commands */
	mvSataFlushDmaQueue (pSataAdapter, channelIndex, MV_FLUSH_TYPE_NONE);

	if (mvStorageDevATASetFeatures(pSataAdapter, channelIndex,
	    MV_ATA_SET_FEATURES_TRANSFER, mvMode, 0, 0, 0) == MV_FALSE) {
		KdPrint(("channel %d: Set Features failed\n", channelIndex)); 
	}
	/* Enable EDMA */
	if (mvSataEnableChannelDma(pSataAdapter, channelIndex) == MV_FALSE)
		KdPrint(("Failed to enable DMA, channel=%d", channelIndex));
#endif
}

#ifdef SUPPORT_ARRAY
#define IdeRegisterVDevice  fCheckArray
#else
void
IdeRegisterVDevice(PDevice pDev)
{
	PVDevice pVDev = Map2pVDevice(pDev);

	pVDev->VDeviceType = pDev->df_atapi? VD_ATAPI : 
	    pDev->df_removable_drive ? VD_REMOVABLE : VD_SINGLE_DISK;
	pVDev->vf_online = 1;
	pVDev->VDeviceCapacity = pDev->dDeRealCapacity;
	pVDev->pfnSendCommand = pfnSendCommand[pVDev->VDeviceType];
	pVDev->pfnDeviceFailed = pfnDeviceFailed[pVDev->VDeviceType];
}
#endif

static int num_adapters = 0;
static int
init_adapter(IAL_ADAPTER_T *pAdapter)
{
	PCommand pCmd;
	pPrivCommand prvCmd;
	PVBus _vbus_p = &pAdapter->VBus;
	MV_SATA_ADAPTER *pMvSataAdapter;
	PUCHAR PRDTable;
	int i, channel, rid, error;

	PVDevice pVDev;

	intrmask_t oldspl = lock_driver();

	pAdapter->next = 0;

	if(gIal_Adapter == 0) {
		gIal_Adapter = pAdapter;
		pCurAdapter = gIal_Adapter;
	} else {
		pCurAdapter->next = pAdapter;
		pCurAdapter = pAdapter;
	}

	pAdapter->outstandingCommands = 0;

	pMvSataAdapter = &(pAdapter->mvSataAdapter);
	_vbus_p->OsExt = (void *)pAdapter; 
	pMvSataAdapter->IALData = pAdapter;

	if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, BUS_SPACE_MAXSIZE_32BIT,
	    MV_MAX_SEGMENTS, BUS_SPACE_MAXSIZE_32BIT, 0, NULL, NULL,
	    &pAdapter->parent_dmat) != 0) {
		MV_ERROR("RR182x: Failed to create busdma resources\n");
		unlock_driver(oldspl);
		return (ENOMEM);
	}

	if (bus_dma_tag_create(pAdapter->parent_dmat, PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    REQUESTS_ARRAY_SIZE, 1, REQUESTS_ARRAY_SIZE, 0, NULL, NULL,
	    &pAdapter->req_dmat) != 0) {
		MV_ERROR("RR182x: Failed to create busdma resources\n");
		error = ENOMEM;
		goto unregister;
	}

	if (bus_dma_tag_create(pAdapter->parent_dmat, PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    RESPONSES_ARRAY_SIZE, 1, RESPONSES_ARRAY_SIZE, 0, NULL, NULL,
	    &pAdapter->resp_dmat) != 0) {
		MV_ERROR("RR182x: Failed to create busdma resources\n");
		error = ENOMEM;
		goto unregister;
	}

	if (bus_dma_tag_create(pAdapter->parent_dmat, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MAXBSIZE, MV_MAX_SEGMENTS, MAXBSIZE, 0, busdma_lock_mutex, &Giant,
	    &pAdapter->buf_dmat) != 0) {
		MV_ERROR("RR182x: Failed to create busdma resources\n");
		error = ENOMEM;
		goto unregister;
	}

	if (hptmv_allocate_edma_queues(pAdapter)) {
		MV_ERROR("RR182x: Failed to allocate memory for EDMA queues\n");
		error = ENOMEM;
		goto unregister;
	}

	/* also map EPROM address */
	rid = 0x10;
	if ((pAdapter->mem_res = bus_alloc_resource(pAdapter->hpt_dev,
	    SYS_RES_MEMORY, &rid, 0, ~0, MV_SATA_PCI_BAR0_SPACE_SIZE+0x40000,
	    RF_ACTIVE)) == 0) {
		MV_ERROR("RR182x: Failed to remap memory space\n");
		error = ENXIO;
		goto unregister;
	}

	/*
	 * This field is opaque.  Abuse it so that the bus_space functions
	 * can get the info that they need when called.
	 */
	pMvSataAdapter->adapterIoBaseAddress = pAdapter;
	pAdapter->mem_bsh = rman_get_bushandle(pAdapter->mem_res);
	pAdapter->mem_btag = rman_get_bustag(pAdapter->mem_res);
	
	pMvSataAdapter->adapterId = num_adapters++;
	/* get the revision ID */
	pMvSataAdapter->pciConfigRevisionId =
	    pci_read_config(pAdapter->hpt_dev, PCIR_REVID, 1);
	pMvSataAdapter->pciConfigDeviceId = pci_get_device(pAdapter->hpt_dev);

	/* init RR182x */
	pMvSataAdapter->intCoalThre[0]= 1;
	pMvSataAdapter->intCoalThre[1]= 1;
	pMvSataAdapter->intTimeThre[0] = 1;
	pMvSataAdapter->intTimeThre[1] = 1;
	pMvSataAdapter->pciCommand = 0x0107E371;
	pMvSataAdapter->pciSerrMask = 0xd77fe6ul;
	pMvSataAdapter->pciInterruptMask = 0xd77fe6ul;
	pMvSataAdapter->mvSataEventNotify = hptmv_event_notify;

	if (mvSataInitAdapter(pMvSataAdapter) == MV_FALSE) {
		MV_ERROR("RR182x[%d]: core failed to initialize the adapter\n",
			 pMvSataAdapter->adapterId);
		error = ENXIO;
		goto unregister;
	}
	pAdapter->ver_601 = pMvSataAdapter->pcbVersion;

#ifndef FOR_DEMO
	set_fail_leds(pMvSataAdapter, 0);
#endif
	
	/* setup command blocks */
	KdPrint(("Allocate command blocks\n"));
	_vbus_(pFreeCommands) = 0;
	pAdapter->pCommandBlocks = malloc(sizeof(struct _Command) *
	    MAX_COMMAND_BLOCKS_FOR_EACH_VBUS, M_DEVBUF, M_ZERO | M_WAITOK);
	KdPrint(("pCommandBlocks:%p\n", pAdapter->pCommandBlocks));

	/*
	 * Gotta cheat here.  The _Command struct only gives us a single
	 * pointer for private data, but we need to store more than that.
	 * Of course the pCommand retains no type stability, and FreeCommand
	 * is hidden in the binary object, so gotta track these on our own
	 * list.
	 */
	pAdapter->pPrivateBlocks = malloc(sizeof(struct _privCommand) *
	    MAX_COMMAND_BLOCKS_FOR_EACH_VBUS, M_DEVBUF, M_ZERO | M_WAITOK);
	TAILQ_INIT(&pAdapter->PrivCmdTQH);

	for (i = 0; i < MAX_COMMAND_BLOCKS_FOR_EACH_VBUS; i++) {
		pCmd = &pAdapter->pCommandBlocks[i];
		prvCmd = &pAdapter->pPrivateBlocks[i];
		prvCmd->pAdapter = pAdapter;
		if ((error = bus_dmamap_create(pAdapter->buf_dmat, 0,
		    &prvCmd->buf_map)) == 0) {
			FreeCommand(_VBUS_P (pCmd));
			FreePrivCommand(pAdapter, prvCmd);
		} else
			break;
	}

	/* setup PRD Tables */
	KdPrint(("Allocate PRD Tables\n"));
	pAdapter->pFreePRDLink = 0;

	if (bus_dma_tag_create(pAdapter->parent_dmat, PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    PRD_ENTRIES_SIZE * PRD_TABLES_FOR_VBUS, 1,
	    PRD_ENTRIES_SIZE * PRD_TABLES_FOR_VBUS, 0, NULL, NULL,
	    &pAdapter->prd_dmat) != 0) {
		MV_ERROR("RR182x: Failed to create busdma resources\n");
		error = ENOMEM;
		goto unregister;
	}

	if (bus_dmamem_alloc(pAdapter->prd_dmat,
	    (void **)&pAdapter->prdTableAddr, BUS_DMA_WAITOK,
	    &pAdapter->prd_map) != 0)
		goto unregister;

	(void)bus_dmamap_load(pAdapter->prd_dmat, pAdapter->prd_map,
	    pAdapter->prdTableAddr, PRD_ENTRIES_SIZE * PRD_TABLES_FOR_VBUS,
	    hptmv_map_req, &pAdapter->prdTableDmaAddr, 0);

	KdPrint(("prdTableAddr:%p\n",pAdapter->prdTableAddr));
	if (!pAdapter->prdTableAddr) {
		MV_ERROR("insufficient PRD Tables\n");
		error = ENOMEM;
		goto unregister;
	}

	PRDTable = pAdapter->prdTableAddr;
	for (i = 0; i < PRD_TABLES_FOR_VBUS; i++) {
		KdPrint(("i= %d, pAdapter->pFreePRDLink= %p\n", i,
		    pAdapter->pFreePRDLink));
		FreePRDTable(pAdapter, PRDTable);
		PRDTable += PRD_ENTRIES_SIZE;
	}

	/* enable the adapter interrupts */

	/* configure and start the connected channels*/
	for (channel = 0; channel < MV_SATA_CHANNELS_NUM; channel++) {
		pAdapter->mvChannel[channel].online = MV_FALSE;
		if (mvSataIsStorageDeviceConnected(pMvSataAdapter, channel)
		    != MV_TRUE)
			continue;

		KdPrint(("RR182x[%d]: channel %d is connected\n",
			pMvSataAdapter->adapterId, channel));

		if (hptmv_init_channel(pAdapter, channel) == 0) {
			if (mvSataConfigureChannel(pMvSataAdapter, channel)
			    == MV_FALSE) {
				MV_ERROR("RR182x[%d]: Failed to configure "
				    "channel %d\n", pMvSataAdapter->adapterId,
				    channel);
				hptmv_free_channel(pAdapter, channel);
				continue;
			}
			if (start_channel(pAdapter, channel)) {
				MV_ERROR("RR182x[%d]: Failed to start channel, "
				   "channel=%d\n", pMvSataAdapter->adapterId,
				    channel);
				hptmv_free_channel(pAdapter, channel);
			}
			pAdapter->mvChannel[channel].online = MV_TRUE; 
#if 0
			mvSataChannelSetEdmaLoopBackMode(
			    pMvSataAdapter, channel, MV_TRUE);
#endif
		}
		KdPrint(("pAdapter->mvChannel[channel].online:%x, channel:%d\n",
			pAdapter->mvChannel[channel].online, channel));
	}

#ifdef SUPPORT_ARRAY
	for(i = MAX_ARRAY_DEVICE - 1; i >= 0; i--) {
		pVDev = ArrayTables(i);
		mArFreeArrayTable(pVDev);
	}
#endif

	KdPrint(("Initialize Devices\n"));
	for (channel = 0; channel < MV_SATA_CHANNELS_NUM; channel++) {
		MV_SATA_CHANNEL *pMvSataChannel;

		pMvSataChannel = pMvSataAdapter->sataChannel[channel];
		if (pMvSataChannel) {
			init_vdev_params(pAdapter, channel);
			IdeRegisterVDevice(&pAdapter->VDevices[channel].u.disk);
		}
	}
#ifdef SUPPORT_ARRAY
	CheckArrayCritical(_VBUS_P0);
#endif
	_vbus_p->nInstances = 1;
	fRegisterVdevice(pAdapter);

	for (channel=0;channel<MV_SATA_CHANNELS_NUM;channel++) {
		pVDev = _vbus_p->pVDevice[channel];
		if (pVDev && pVDev->vf_online)
			fCheckBootable(pVDev);
	}

#if defined(SUPPORT_ARRAY) && defined(_RAID5N_)
	init_raid5_memory(_VBUS_P0);
	_vbus_(r5).enable_write_back = 1;
	printf("RR182x: RAID5 write-back %s\n",
	    _vbus_(r5).enable_write_back? "enabled" : "disabled");
#endif

	mvSataUnmaskAdapterInterrupt(pMvSataAdapter);
	unlock_driver(oldspl);
	return 0;

unregister:
	if (pAdapter->mem_res != 0)
		bus_release_resource(pAdapter->hpt_dev, SYS_RES_MEMORY, rid,
		    pAdapter->mem_res);

	hptmv_free_edma_queues(pAdapter);

	if (pAdapter->resp_dmat != NULL)
		bus_dma_tag_destroy(pAdapter->resp_dmat);
	if (pAdapter->req_dmat != NULL)
		bus_dma_tag_destroy(pAdapter->req_dmat);
	if (pAdapter->buf_dmat != NULL)
		bus_dma_tag_destroy(pAdapter->buf_dmat);
	if (pAdapter->parent_dmat != NULL)
		bus_dma_tag_destroy(pAdapter->parent_dmat);

	unlock_driver(oldspl);
	return error;
}

int
MvSataResetChannel(MV_SATA_ADAPTER *pMvSataAdapter, MV_U8 channel)
{
	IAL_ADAPTER_T *pAdapter = (IAL_ADAPTER_T *)pMvSataAdapter->IALData;

	mvSataDisableChannelDma(pMvSataAdapter, channel);

	/* Flush pending commands */
	mvSataFlushDmaQueue (pMvSataAdapter, channel, MV_FLUSH_TYPE_CALLBACK);

	/* Software reset channel */
	if (mvStorageDevATASoftResetDevice(pMvSataAdapter, channel) ==
	    MV_FALSE) {
		MV_ERROR("RR182x [%d,%d]: failed to perform Software reset\n",
			 pMvSataAdapter->adapterId, channel);
		return -1;
	}
	
	/* Hardware reset channel */
	if (mvSataChannelHardReset(pMvSataAdapter, channel)== MV_FALSE) {
		MV_ERROR("RR182x [%d,%d] Failed to Hard reser the SATA "
			 "channel\n", pMvSataAdapter->adapterId, channel);
		hptmv_free_channel(pAdapter, channel);
		return -1;
	}

	if (mvSataIsStorageDeviceConnected(pMvSataAdapter, channel) ==
	    MV_FALSE) {
		MV_ERROR("RR182x [%d,%d] Failed to Connect Device\n",
			 pMvSataAdapter->adapterId, channel);
		hptmv_free_channel(pAdapter, channel);
		return -1;
	} else {
		/* Set transfer mode */
		if((mvStorageDevATASetFeatures(pMvSataAdapter, channel,
		    MV_ATA_SET_FEATURES_TRANSFER, MV_ATA_TRANSFER_PIO_SLOW, 0,
		    0, 0) == MV_FALSE) ||
		    (mvStorageDevATASetFeatures(pMvSataAdapter, channel,
		    MV_ATA_SET_FEATURES_TRANSFER,
		    pAdapter->mvChannel[channel].maxPioModeSupported, 0, 0, 0)
		    == MV_FALSE) || (mvStorageDevATASetFeatures(pMvSataAdapter,
		    channel, MV_ATA_SET_FEATURES_TRANSFER,
		    pAdapter->mvChannel[channel].maxUltraDmaModeSupported, 0,
		    0, 0) == MV_FALSE)) {
			MV_ERROR("channel %d: Set Features failed", channel);
			hptmv_free_channel(pAdapter, channel);
			return -1;
		}
		/* Enable EDMA */
		if (mvSataEnableChannelDma(pMvSataAdapter, channel)==MV_FALSE) {
			MV_ERROR("Failed to enable DMA, channel=%d", channel);
			hptmv_free_channel(pAdapter, channel);
			return -1;
		}
	}
	return 0;
}

static int
fResetActiveCommands(PVBus _vbus_p)
{
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_U8 channel;
	int   rtn = 0;

	pMvSataAdapter = &((IAL_ADAPTER_T *)_vbus_p->OsExt)->mvSataAdapter;
	for (channel=0;channel< MV_SATA_CHANNELS_NUM;channel++) {
		if (pMvSataAdapter->sataChannel[channel] &&
		    pMvSataAdapter->sataChannel[channel]->outstandingCommands) 
			if (MvSataResetChannel(pMvSataAdapter,channel) == -1)
				 rtn = -1;
	}
	HPT_ASSERT(rtn==0);
	return 0;
}

void
fCompleteAllCommandsSynchronously(PVBus _vbus_p)
{
	UINT cont;
	ULONG ticks = 0;
	MV_U8 channel;
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_SATA_CHANNEL *pMvSataChannel;

	pMvSataAdapter = &((IAL_ADAPTER_T *)_vbus_p->OsExt)->mvSataAdapter;
	do {
check_cmds:
		cont = 0;
		CheckPendingCall(_VBUS_P0);
#ifdef _RAID5N_
		dataxfer_poll();
		xor_poll();
#endif
		for (channel = 0; channel < MV_SATA_CHANNELS_NUM; channel++) {
			pMvSataChannel = pMvSataAdapter->sataChannel[channel];
			if (pMvSataChannel &&
			    pMvSataChannel->outstandingCommands) {
				while (pMvSataChannel->outstandingCommands) {
					if (!mvSataInterruptServiceRoutine(
					    pMvSataAdapter)) {
						StallExec(1000);
						if (ticks++ > 3000) {
							MvSataResetChannel(
							    pMvSataAdapter,
							    channel);
							goto check_cmds;
						}
					} else 
						ticks = 0;
				}
				cont = 1;
			}
		}
	} while (cont);
}

void
fResetVBus(_VBUS_ARG0)
{
	KdPrint(("fMvResetBus(%p)", _vbus_p));

	/* some commands may already finished. */
	CheckPendingCall(_VBUS_P0);

	fResetActiveCommands(_vbus_p);
	/* 
	 * the other pending commands may still be finished successfully.
	 */
	fCompleteAllCommandsSynchronously(_vbus_p);

	/* Now there should be no pending commands. No more action needed. */
	CheckIdleCall(_VBUS_P0);

	KdPrint(("fMvResetBus() done"));
}

void
fRescanAllDevice(_VBUS_ARG0)
{
}

static MV_BOOLEAN 
CommandCompletionCB(MV_SATA_ADAPTER *pMvSataAdapter, MV_U8 channelNum,
    MV_COMPLETION_TYPE comp_type, MV_VOID_PTR commandId, MV_U16 responseFlags,
    MV_U32 timeStamp, MV_STORAGE_DEVICE_REGISTERS *registerStruct)
{
	PCommand pCmd = (PCommand) commandId;
	_VBUS_INST(pCmd->pVDevice->pVBus)

	if (pCmd->uScratch.sata_param.prdAddr) 
		FreePRDTable(pMvSataAdapter->IALData, 
		    pCmd->uScratch.sata_param.prdAddr);

	switch (comp_type) {
	case MV_COMPLETION_TYPE_NORMAL:
		pCmd->Result = RETURN_SUCCESS;
		break;
	case MV_COMPLETION_TYPE_ABORT:
		pCmd->Result = RETURN_BUS_RESET;
		break;
	case MV_COMPLETION_TYPE_ERROR:
		 MV_ERROR("IAL: COMPLETION ERROR, adapter %d, channel %d, "
			  "flags=%x\n", pMvSataAdapter->adapterId, channelNum,
			  responseFlags);

		if (responseFlags & 4) {
			MV_ERROR("ATA regs: error %x, sector count %x, LBA low "
				 "%x, LBA mid %x, LBA high %x, device %x, "
				 "status %x\n", registerStruct->errorRegister,
				 registerStruct->sectorCountRegister,
				 registerStruct->lbaLowRegister,
				 registerStruct->lbaMidRegister,
				 registerStruct->lbaHighRegister,
				 registerStruct->deviceRegister,
				 registerStruct->statusRegister);
		}
		/*
		 * We can't do handleEdmaError directly here, because
		 * CommandCompletionCB is called by mv's ISR, if we retry the
		 * command, than the internel data structure may be destroyed
		 */
		pCmd->uScratch.sata_param.responseFlags = responseFlags;
		pCmd->uScratch.sata_param.bIdeStatus =
		    registerStruct->statusRegister;
		pCmd->uScratch.sata_param.errorRegister =
		    registerStruct->errorRegister;
		pCmd->pVDevice->u.disk.QueueLength--;
		CallAfterReturn(_VBUS_P (DPC_PROC)handleEdmaError,pCmd);
		return TRUE;
		
	default:
		MV_ERROR(" Unknown completion type (%d)\n", comp_type);
		return MV_FALSE;
	}
	
	if (pCmd->uCmd.Ide.Command == IDE_COMMAND_VERIFY &&
	    pCmd->uScratch.sata_param.cmd_priv > 1) {
		pCmd->uScratch.sata_param.cmd_priv --;
		return TRUE;
	}
	pCmd->pVDevice->u.disk.QueueLength--;
	CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);
	return TRUE;
}

void
fDeviceSendCommand(_VBUS_ARG PCommand pCmd)
{
	MV_SATA_EDMA_PRD_ENTRY *pPRDTable = 0;
	MV_SATA_ADAPTER *pMvSataAdapter;
	MV_SATA_CHANNEL *pMvSataChannel;
	IAL_ADAPTER_T *pAdapter;
	MV_QUEUE_COMMAND_RESULT result;
	MV_QUEUE_COMMAND_INFO commandInfo;	
	MV_UDMA_COMMAND_PARAMS *pUdmaParams;
	MV_NONE_UDMA_COMMAND_PARAMS *pNoUdmaParams;
	MV_BOOLEAN is48bit = MV_FALSE;
	PVDevice pVDevice;
	PDevice pDevice;
	ULONG Lba;
	USHORT nSector;
	MV_U8 channel;
	int  i=0;

	pVDevice = pCmd->pVDevice;
	pDevice = &pVDevice->u.disk;
	Lba = pCmd->uCmd.Ide.Lba;
	nSector = pCmd->uCmd.Ide.nSectors;
	pUdmaParams = &commandInfo.commandParams.udmaCommand;
	pNoUdmaParams = &commandInfo.commandParams.NoneUdmaCommand;

	DECLARE_BUFFER(FPSCAT_GATH, tmpSg);

	if (!pDevice->df_on_line) {
		MV_ERROR("Device is offline");
		pCmd->Result = RETURN_BAD_DEVICE;
		CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);
		return;
	}

	pDevice->HeadPosition = pCmd->uCmd.Ide.Lba + pCmd->uCmd.Ide.nSectors;
	pMvSataChannel = pDevice->mv;
	pMvSataAdapter = pMvSataChannel->mvSataAdapter;
	channel = pMvSataChannel->channelNumber;
	pAdapter = pMvSataAdapter->IALData;
	
	/*
	 * Old RAID0 has hidden lba. Remember to clear dDeHiddenLba when
	 * deleting array!
	 */
	Lba += pDevice->dDeHiddenLba;
	/* check LBA */
	if (Lba+nSector-1 > pDevice->dDeRealCapacity) {
		pCmd->Result = RETURN_INVALID_REQUEST;
		CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);
		return;
	}
	
	if(Lba & 0xF0000000){
		is48bit = MV_TRUE;
	}

	switch (pCmd->uCmd.Ide.Command)	{
	case IDE_COMMAND_READ:
	case IDE_COMMAND_WRITE:
		if (pDevice->bDeModeSetting<8) goto pio;
		
		commandInfo.type = MV_QUEUED_COMMAND_TYPE_UDMA;
		pUdmaParams->isEXT = is48bit;
		pUdmaParams->numOfSectors = nSector;
		pUdmaParams->lowLBAAddress = Lba;
		pUdmaParams->highLBAAddress = 0;
		pUdmaParams->prdHighAddr = 0;
		pUdmaParams->callBack = CommandCompletionCB;
		pUdmaParams->commandId = (MV_VOID_PTR )pCmd;
		if(pCmd->uCmd.Ide.Command == IDE_COMMAND_READ)
			pUdmaParams->readWrite = MV_UDMA_TYPE_READ;
		else 
			pUdmaParams->readWrite = MV_UDMA_TYPE_WRITE;
		
		if (pCmd->pSgTable && pCmd->cf_physical_sg) {
			FPSCAT_GATH sg1=tmpSg, sg2=pCmd->pSgTable;
			do {
				*sg1++=*sg2;
			} while ((sg2++->wSgFlag & SG_FLAG_EOT)==0);
		} else if (!pCmd->pfnBuildSgl ||
			    !pCmd->pfnBuildSgl(_VBUS_P pCmd, tmpSg, 0)) {
pio:				
			mvSataDisableChannelDma(pMvSataAdapter, channel);
			mvSataFlushDmaQueue(pMvSataAdapter, channel,
			    MV_FLUSH_TYPE_CALLBACK);

			if (pCmd->pSgTable && pCmd->cf_physical_sg==0) {
				FPSCAT_GATH sg1=tmpSg, sg2=pCmd->pSgTable;
				do {
					*sg1++=*sg2;
				} while ((sg2++->wSgFlag & SG_FLAG_EOT)==0);
			} else if (!pCmd->pfnBuildSgl ||
				    !pCmd->pfnBuildSgl(_VBUS_P pCmd, tmpSg, 1)){
				pCmd->Result = RETURN_NEED_LOGICAL_SG;
				goto finish_cmd;
			}
									
			do {
				ULONG size;
				ULONG_PTR addr = tmpSg->dSgAddress;
				
				size = tmpSg->wSgSize? tmpSg->wSgSize : 0x10000;
				if (size & 0x1ff) {
					pCmd->Result = RETURN_INVALID_REQUEST;
					goto finish_cmd;
				}
				if (mvStorageDevATAExecuteNonUDMACommand(
				    pMvSataAdapter, channel,
				    (pCmd->cf_data_out) ?
				    MV_NON_UDMA_PROTOCOL_PIO_DATA_OUT :
				    MV_NON_UDMA_PROTOCOL_PIO_DATA_IN,
				    is48bit, (MV_U16_PTR)addr, 
				    size >> 1,	/* count */
				    0,		/* features N/A */
				    (MV_U16)(size>>9),	/*sector count*/
				    (MV_U16)((is48bit ?
				    (MV_U16)((Lba >> 16) & 0xFF00) : 0 ) |
				    (UCHAR)(Lba & 0xFF) ), /*lbalow*/
				    (MV_U16)((Lba >> 8) & 0xFF), /* lbaMid */
				    (MV_U16)((Lba >> 16) & 0xFF),/* lbaHig */
				    (MV_U8)(0x40 | (is48bit ? 0 :
				    (UCHAR)(Lba >> 24) & 0xFF )),/* device */
				    (MV_U8)(is48bit ? (pCmd->cf_data_in ?
				    IDE_COMMAND_READ_EXT :
				    IDE_COMMAND_WRITE_EXT) :
				    pCmd->uCmd.Ide.Command))==MV_FALSE) {
					pCmd->Result = RETURN_IDE_ERROR;
					goto finish_cmd;
				}
				Lba += size>>9;
				if(Lba & 0xF0000000) is48bit = MV_TRUE;
			}
			while ((tmpSg++->wSgFlag & SG_FLAG_EOT)==0);
			pCmd->Result = RETURN_SUCCESS;
finish_cmd:
			mvSataEnableChannelDma(pMvSataAdapter,channel);
			CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion,
			    pCmd);
			return;
		}
		
		pPRDTable = AllocatePRDTable(pAdapter);
		KdPrint(("pPRDTable:%p\n",pPRDTable));
		if (!pPRDTable) {
			pCmd->Result = RETURN_DEVICE_BUSY;
			CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion,
			    pCmd);
			HPT_ASSERT(0);
			return;
		}

		do {
			pPRDTable[i].highBaseAddr = 0;
			pPRDTable[i].flags = (MV_U16)tmpSg->wSgFlag;
			pPRDTable[i].byteCount = (MV_U16)tmpSg->wSgSize;
			pPRDTable[i].lowBaseAddr = (MV_U32)tmpSg->dSgAddress;
			pPRDTable[i].reserved = 0;
			i++;
		} while((tmpSg++->wSgFlag & SG_FLAG_EOT)==0);
		
		pUdmaParams->prdLowAddr = pAdapter->prdTableDmaAddr +
		    ((ULONG)pPRDTable - (ULONG)pAdapter->prdTableAddr);

		if ((pUdmaParams->numOfSectors == 256) &&
		    (pMvSataChannel->lba48Address == MV_FALSE)) {
			pUdmaParams->numOfSectors = 0;
		}
		
		pCmd->uScratch.sata_param.prdAddr = (PVOID)pPRDTable;

		result = mvSataQueueCommand(pMvSataAdapter, channel,
		    &commandInfo);

		if (result != MV_QUEUE_COMMAND_RESULT_OK) {
queue_failed:
			switch (result) {
			case MV_QUEUE_COMMAND_RESULT_BAD_LBA_ADDRESS:
				MV_ERROR("IAL Error: Edma Queue command "
					 "failed. Bad LBA LBA[31:0](0x%08x)\n",
					 pUdmaParams->lowLBAAddress);
				pCmd->Result = RETURN_IDE_ERROR;
				break;
			case MV_QUEUE_COMMAND_RESULT_QUEUED_MODE_DISABLED:
				MV_ERROR("IAL Error: Edma Queue command "
					 "failed. EDMA disabled adapter %d "
					 "channel %d\n",
					 pMvSataAdapter->adapterId, channel);
				mvSataEnableChannelDma(pMvSataAdapter,channel);
				pCmd->Result = RETURN_IDE_ERROR;
				break;
			case MV_QUEUE_COMMAND_RESULT_FULL:
				MV_ERROR("IAL Error: Edma Queue command "
					 "failed. Queue is Full adapter %d "
					 "channel %d\n",
					 pMvSataAdapter->adapterId, channel);
				pCmd->Result = RETURN_DEVICE_BUSY;
				break;
			case MV_QUEUE_COMMAND_RESULT_BAD_PARAMS:
				MV_ERROR("IAL Error: Edma Queue command "
					 "failed. (Bad Params), pMvSataAdapter:"
					 " %p,  pSataChannel: %p.\n",
					 pMvSataAdapter,
					 pMvSataAdapter->sataChannel[channel]);
				pCmd->Result = RETURN_IDE_ERROR;
				break;
			default:
				MV_ERROR("IAL Error: Bad result value (%d) "
					 "from queue command\n", result);
				pCmd->Result = RETURN_IDE_ERROR;
			}
			if(pPRDTable) 
				FreePRDTable(pAdapter, pPRDTable);
			CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion,
			    pCmd);
		}
		pDevice->QueueLength++;
		return;
		
	case IDE_COMMAND_VERIFY:
		commandInfo.type = MV_QUEUED_COMMAND_TYPE_NONE_UDMA;
		pNoUdmaParams->bufPtr = NULL;
		pNoUdmaParams->callBack = CommandCompletionCB;
		pNoUdmaParams->commandId = (MV_VOID_PTR)pCmd;
		pNoUdmaParams->count = 0;
		pNoUdmaParams->features = 0;
		pNoUdmaParams->protocolType = MV_NON_UDMA_PROTOCOL_NON_DATA;
		
		pCmd->uScratch.sata_param.cmd_priv = 1;
		if (pMvSataChannel->lba48Address == MV_TRUE){
			pNoUdmaParams->command =
			    MV_ATA_COMMAND_READ_VERIFY_SECTORS_EXT;
			pNoUdmaParams->isEXT = MV_TRUE;
			pNoUdmaParams->lbaHigh =
			    (MV_U16)((Lba & 0xff0000) >> 16);
			pNoUdmaParams->lbaMid = (MV_U16)((Lba & 0xff00) >> 8);   
			pNoUdmaParams->lbaLow = 
			    (MV_U16)(((Lba & 0xff000000) >> 16)| (Lba & 0xff));
			pNoUdmaParams->sectorCount = nSector;
			pNoUdmaParams->device = 0x40;
			result = mvSataQueueCommand(pMvSataAdapter, channel,
			     &commandInfo);
			if (result != MV_QUEUE_COMMAND_RESULT_OK) {
				goto queue_failed;
			}
			return;
		}
		pNoUdmaParams->command = MV_ATA_COMMAND_READ_VERIFY_SECTORS;
		pNoUdmaParams->isEXT = MV_FALSE;
		pNoUdmaParams->lbaHigh = (MV_U16)((Lba & 0xff0000) >> 16);
		pNoUdmaParams->lbaMid = (MV_U16)((Lba & 0xff00) >> 8);   
		pNoUdmaParams->lbaLow = (MV_U16)(Lba & 0xff);
		pNoUdmaParams->sectorCount = 0xff & nSector;
		pNoUdmaParams->device = (MV_U8)(0x40 |
			((Lba & 0xf000000) >> 24));
		pNoUdmaParams->callBack = CommandCompletionCB;
		result = mvSataQueueCommand(pMvSataAdapter, channel,
		     &commandInfo);
		/*
		 * FIXME: how about the commands already queued? but marvel
		 * also forgets to consider this
		 */
		if (result != MV_QUEUE_COMMAND_RESULT_OK){
			goto queue_failed;
		}
		break;
	default:
		pCmd->Result = RETURN_INVALID_REQUEST;
		CallAfterReturn(_VBUS_P (DPC_PROC)pCmd->pfnCompletion, pCmd);
		break;
	}
}

/**********************************************************
 *
 *	Probe the hostadapter.
 *
 **********************************************************/
static int
hpt_probe(device_t dev)
{
	if ((pci_get_vendor(dev) == MV_SATA_VENDOR_ID) &&
		(pci_get_device(dev) == MV_SATA_DEVICE_ID_5081
#ifdef FOR_DEMO
		|| pci_get_device(dev) == MV_SATA_DEVICE_ID_5080
#endif
		)) {
		KdPrintI((CONTROLLER_NAME " found\n"));
		device_set_desc(dev, CONTROLLER_NAME);
		return 0;
	}
	else
		return(ENXIO);
}

/***********************************************************
 *
 *      Auto configuration:  attach and init a host adapter.
 *
 ***********************************************************/
static int
hpt_attach(device_t dev)
{
	IAL_ADAPTER_T * pAdapter;
	int rid;
	union ccb *ccb;
	struct cam_devq *devq;
	struct cam_sim *hpt_vsim;

	printf("%s Version %s\n", DRIVER_NAME, DRIVER_VERSION);

	pAdapter = device_get_softc(dev);
	pAdapter->hpt_dev = dev;
	
	rid = init_adapter(pAdapter);
	if (rid)
		return rid;

	rid = 0;
	if ((pAdapter->hpt_irq = bus_alloc_resource(pAdapter->hpt_dev,
	    SYS_RES_IRQ, &rid, 0, ~0ul, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL){
		hpt_printk(("can't allocate interrupt\n"));
		return(ENXIO);
	}

	if(bus_setup_intr(pAdapter->hpt_dev, pAdapter->hpt_irq, INTR_TYPE_CAM,
	    hpt_intr, pAdapter, &pAdapter->hpt_intr)) {
		hpt_printk(("can't set up interrupt\n"));
		free(pAdapter, M_DEVBUF);
		return(ENXIO);
	}

#if 1
	if ((ccb = malloc(sizeof(*ccb), M_DEVBUF, M_WAITOK | M_ZERO)) != NULL) {
		ccb->ccb_h.pinfo.priority = 1;
		ccb->ccb_h.pinfo.index = CAM_UNQUEUED_INDEX;
	} else {
		return ENOMEM;
	}
#endif
	/*
	 * Create the device queue for our SIM(s).
	 */
	if((devq = cam_simq_alloc(8/*MAX_QUEUE_COMM*/)) == NULL) {
		KdPrint(("ENXIO\n"));
		return ENOMEM;
	}

	/*
	 * Construct our SIM entry
	 */
	if ((hpt_vsim = cam_sim_alloc(hpt_action, hpt_poll,__str(PROC_DIR_NAME),
	    pAdapter, device_get_unit(pAdapter->hpt_dev), /*untagged*/1,
	    /*tagged*/8,  devq)) == NULL) {
		cam_simq_free(devq);
		return ENOMEM;
	}

	if(xpt_bus_register(hpt_vsim, 0) != CAM_SUCCESS) {
		cam_sim_free(hpt_vsim, /*free devq*/ TRUE);
		hpt_vsim = NULL;
		return ENXIO;
	}

	if(xpt_create_path(&pAdapter->path, /*periph */ NULL,
	    cam_sim_path(hpt_vsim), CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD)
	    != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(hpt_vsim));
		cam_sim_free(hpt_vsim, /*free_devq*/TRUE);
		hpt_vsim = NULL;
		return ENXIO;
	}

	xpt_setup_ccb(&(ccb->ccb_h), pAdapter->path, /*priority*/5);
	ccb->ccb_h.func_code = XPT_SASYNC_CB;
	ccb->csa.event_enable = AC_LOST_DEVICE;
	ccb->csa.callback = hpt_async;
	ccb->csa.callback_arg = hpt_vsim;
	xpt_action((union ccb *)ccb);
	free(ccb, M_DEVBUF);

	/* Only do this setup for the first device. */
	if (device_get_unit(dev) == 0) {
		pAdapter->eh = EVENTHANDLER_REGISTER(shutdown_final,
		    hpt_shutdown, dev, SHUTDOWN_PRI_DEFAULT);
		if (pAdapter->eh != NULL)
			launch_worker_thread();
		else
			printf("hptmv: shutdown event registration failed\n");
	}

	return 0;
}

static int
hpt_detach(device_t dev)
{
	return (EBUSY);
}

/***************************************************************
 * The poll function is used to simulate the interrupt when
 * the interrupt subsystem is not functioning.
 *
 ***************************************************************/
static void
hpt_poll(struct cam_sim *sim)
{
	hpt_intr((void *)cam_sim_softc(sim));
}

/****************************************************************
 *	Name:	hpt_intr
 *	Description:	Interrupt handler.
 ****************************************************************/
static void
hpt_intr(void *arg)
{
	IAL_ADAPTER_T *pAdapter = (IAL_ADAPTER_T *)arg;
	intrmask_t oldspl;
	
	 oldspl = lock_driver();
	/* KdPrintI(("----- Entering Isr() -----\n")); */
	if (mvSataInterruptServiceRoutine(&pAdapter->mvSataAdapter) == MV_TRUE){
		_VBUS_INST(&pAdapter->VBus)
		CheckPendingCall(_VBUS_P0);
	}

	/* KdPrintI(("----- Leaving Isr() -----\n")); */
	unlock_driver(oldspl);
}

/**********************************************************
 * 			Asynchronous Events
 *********************************************************/
#if (!defined(UNREFERENCED_PARAMETER))
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif

static void
hpt_async(void * callback_arg, u_int32_t code, struct cam_path * path,
    void * arg)
{
	/* debug XXXX */
	panic("Here");
	UNREFERENCED_PARAMETER(callback_arg);
	UNREFERENCED_PARAMETER(code);
	UNREFERENCED_PARAMETER(path);
	UNREFERENCED_PARAMETER(arg);

}

static void
FlushAdapter(IAL_ADAPTER_T *pAdapter)
{
	int i;

	hpt_printk(("flush all devices\n"));
	
	/* flush all devices */
	for (i=0; i<MAX_VDEVICE_PER_VBUS; i++) {
		PVDevice pVDev = pAdapter->VBus.pVDevice[i];
		if (pVDev)
			fFlushVDev(pVDev);
	}
}

static int
hpt_shutdown(device_t dev)
{
	IAL_ADAPTER_T *pAdapter;

	pAdapter = device_get_softc(dev);
	if (pAdapter == NULL)
		return (EINVAL);

	EVENTHANDLER_DEREGISTER(shutdown_final, pAdapter->eh);
	FlushAdapter(pAdapter);

	return 0;
}

void
Check_Idle_Call(IAL_ADAPTER_T *pAdapter)
{
	int i = 0;

	_VBUS_INST(&pAdapter->VBus)

	if (mWaitingForIdle(_VBUS_P0)) {
		CheckIdleCall(_VBUS_P0);
#ifdef SUPPORT_ARRAY
		for(i = 0; i < MAX_ARRAY_PER_VBUS; i++){
			PVDevice pArray;

			if ((pArray = ArrayTables(i))->u.array.dArStamp == 0) 
				continue; 
			else if (pArray->u.array.rf_auto_rebuild) {
				KdPrint(("auto rebuild.\n"));
				pArray->u.array.rf_auto_rebuild = 0;
				hpt_queue_dpc((HPT_DPC)hpt_rebuild_data_block,
				    pAdapter, pArray, DUPLICATE);
			}
		}
#endif
	}
	/* launch the awaiting commands blocked by mWaitingForIdle */
	while(pAdapter->pending_Q!= NULL) {
		_VBUS_INST(&pAdapter->VBus)
		union ccb *ccb =
		    (union ccb *)pAdapter->pending_Q->ccb_h.ccb_ccb_ptr;

		hpt_free_ccb(&pAdapter->pending_Q, ccb);
		CallAfterReturn(_VBUS_P (DPC_PROC)OsSendCommand, ccb);
	}
}

static void
ccb_done(union ccb *ccb)
{
	IAL_ADAPTER_T * pAdapter = (IAL_ADAPTER_T *)ccb->ccb_adapter;
	KdPrintI(("ccb_done: ccb %p status %x", ccb, ccb->ccb_h.status));

	xpt_done(ccb);

	pAdapter->outstandingCommands--;

	if (pAdapter->outstandingCommands == 0) {
		if(DPC_Request_Nums == 0)
			Check_Idle_Call(pAdapter);
	}
}

/****************************************************************
 *	Name:	hpt_action
 *	Description:	Process a queued command from the CAM layer.
 *	Parameters:		sim - Pointer to SIM object
 *					ccb - Pointer to SCSI command structure.
 ****************************************************************/

void
hpt_action(struct cam_sim *sim, union ccb *ccb)
{
	intrmask_t oldspl;
	IAL_ADAPTER_T * pAdapter = (IAL_ADAPTER_T *) cam_sim_softc(sim);
	_VBUS_INST(&pAdapter->VBus)
	
	ccb->ccb_adapter = pAdapter;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("hpt_action\n"));
	KdPrint(("hpt_action(%lx,%lx{%x})\n", (u_long)sim, (u_long)ccb,
	    ccb->ccb_h.func_code));

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		/* ccb->ccb_h.path_id is not our bus id - don't check it */

		if (ccb->ccb_h.target_lun)	{
			ccb->ccb_h.status = CAM_LUN_INVALID;
			xpt_done(ccb);
			return;
		}
		if (ccb->ccb_h.target_id >= MAX_VDEVICE_PER_VBUS ||
			pAdapter->VBus.pVDevice[ccb->ccb_h.target_id]==0) {
			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			return;
		}

		oldspl = lock_driver();
		if (pAdapter->outstandingCommands==0 && DPC_Request_Nums==0)
			Check_Idle_Call(pAdapter);

		if (mWaitingForIdle(_VBUS_P0))
			hpt_queue_ccb(&pAdapter->pending_Q, ccb);
		else
			OsSendCommand(_VBUS_P ccb);
		unlock_driver(oldspl);

		/* KdPrint(("leave scsiio\n")); */
		break;

	case XPT_RESET_BUS:
		KdPrint(("reset bus\n"));
		oldspl = lock_driver();
		fResetVBus(_VBUS_P0);
		unlock_driver(oldspl);
		xpt_done(ccb);
		break;

	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_GET_TRAN_SETTINGS:
	case XPT_SET_TRAN_SETTINGS:
		/* XXX Implement */
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size / ((1024L*1024L) / ccg->block_size);

		if (size_mb > 1024 ) {
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

	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		/* Not necessary to reset bus */
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;

		cpi->max_target = MAX_VDEVICE_PER_VBUS;
		cpi->max_lun = 0;
		cpi->initiator_id = MAX_VDEVICE_PER_VBUS;

		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "HPT   ", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	default:
		KdPrint(("invalid cmd\n"));
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	/* KdPrint(("leave hpt_action..............\n")); */
}

/* shall be called at lock_driver() */
static void
hpt_queue_ccb(union ccb **ccb_Q, union ccb *ccb)
{
	if(*ccb_Q == NULL)
		ccb->ccb_h.ccb_ccb_ptr = ccb;
	else {
		ccb->ccb_h.ccb_ccb_ptr = (*ccb_Q)->ccb_h.ccb_ccb_ptr;
		(*ccb_Q)->ccb_h.ccb_ccb_ptr = (char *)ccb;
	}

	*ccb_Q = ccb;
}

/* shall be called at lock_driver() */
static void
hpt_free_ccb(union ccb **ccb_Q, union ccb *ccb)
{
	union ccb *TempCCB;

	TempCCB = *ccb_Q;

	if(ccb->ccb_h.ccb_ccb_ptr == ccb)
		/*it means SCpnt is the last one in CURRCMDs*/
		*ccb_Q = NULL;
	else {
		while(TempCCB->ccb_h.ccb_ccb_ptr != (char *)ccb)
			TempCCB = (union ccb *)TempCCB->ccb_h.ccb_ccb_ptr;

		TempCCB->ccb_h.ccb_ccb_ptr = ccb->ccb_h.ccb_ccb_ptr;

		if(*ccb_Q == ccb)
			*ccb_Q = TempCCB;
	}
}

#ifdef SUPPORT_ARRAY
/***************************************************************************
 * Function:     hpt_worker_thread
 * Description:  Do background rebuilding. Execute in kernel thread context.
 * Returns:      None
 ***************************************************************************/
static void hpt_worker_thread(void)
{
	intrmask_t oldspl;

	for(;;)	{
		while (DpcQueue_First!=DpcQueue_Last) {
			ST_HPT_DPC p;
			IAL_ADAPTER_T *pAdapter;
			PVDevice      pArray;
			PVBus         _vbus_p;
			int i;

			oldspl = lock_driver();
			p = DpcQueue[DpcQueue_First];
			DpcQueue_First++;
			DpcQueue_First %= MAX_DPC;
			DPC_Request_Nums++;
			unlock_driver(oldspl);
			p.dpc(p.pAdapter, p.arg, p.flags);

			oldspl = lock_driver();
			DPC_Request_Nums--;
			/*
			 * since we may have prevented Check_Idle_Call, do it
			 * here
			 */
			if (DPC_Request_Nums==0) {
				if (p.pAdapter->outstandingCommands == 0) {
					_VBUS_INST(&p.pAdapter->VBus);
					Check_Idle_Call(p.pAdapter);
					CheckPendingCall(_VBUS_P0);
				}
			}
			unlock_driver(oldspl);
			if (SIGISMEMBER(curproc->p_siglist, SIGSTOP) == 0)
				continue;

			/* abort rebuilding process. */
			pAdapter = gIal_Adapter;
			while (pAdapter != 0) {
				_vbus_p = &pAdapter->VBus;
				for (i = 0; i < MAX_ARRAY_PER_VBUS;i++){
					if ((pArray=ArrayTables(i))->u.array.dArStamp==0) 
						continue; 
					if (pArray->u.array.rf_rebuilding ||
					    pArray->u.array.rf_verifying ||
					    pArray->u.array.rf_initializing) {
						pArray->u.array.rf_abort_rebuild = 1;
					}
				}
				pAdapter = pAdapter->next;
			}
		}

#ifdef DEBUG
		if (SIGISMEMBER(curproc->p_siglist, SIGSTOP))
			tsleep(hpt_worker_thread, PPAUSE, "hptrdy", 2 * hz);
#endif
#if (__FreeBSD_version >= 500043)
		kthread_suspend_check(curproc);
#else
		kproc_suspend_loop(curproc);
#endif
		/* wait for something to do */
		tsleep(hpt_worker_thread, PPAUSE, "hptrdy", 2 * hz);
	}
}

static struct proc *hptdaemonproc;
static struct kproc_desc hpt_kp = {
	"hpt_wt",
	hpt_worker_thread,
	&hptdaemonproc
};

static void
launch_worker_thread(void)
{
	IAL_ADAPTER_T *pAdapTemp;
	
	kproc_start(&hpt_kp);

	for (pAdapTemp = gIal_Adapter; pAdapTemp; pAdapTemp = pAdapTemp->next) {

		_VBUS_INST(&pAdapTemp->VBus)
		int i;
		PVDevice pVDev;

		for(i = 0; i < MAX_ARRAY_PER_VBUS; i++) 
			if ((pVDev=ArrayTables(i))->u.array.dArStamp==0) 
				continue; 
			if (pVDev->u.array.rf_need_rebuild &&
			    !pVDev->u.array.rf_rebuilding) {
				hpt_queue_dpc((HPT_DPC)hpt_rebuild_data_block,
				    pAdapTemp, pVDev,
				    (UCHAR)((pVDev->u.array.CriticalMembers ||
				    pVDev->VDeviceType == VD_RAID_1) ?
				    DUPLICATE : REBUILD_PARITY));
			    }
	}

	/*
	 * hpt_worker_thread needs to be suspended after shutdown sync, when fs
	 * sync finished.
	 */
#if (__FreeBSD_version < 500043)
	EVENTHANDLER_REGISTER(shutdown_post_sync, shutdown_kproc,
	    hptdaemonproc, SHUTDOWN_PRI_FIRST);
#else
	EVENTHANDLER_REGISTER(shutdown_post_sync, kproc_shutdown,
	    hptdaemonproc, SHUTDOWN_PRI_FIRST);
#endif
}

#endif /* SUPPORT_ARRAY */

/* build sgl with merge function */
#define ON64KBOUNDARY(x) (((ULONG_PTR)(x) & 0xFFFF) == 0)

/* XXX */
/* #define NOTNEIGHBORPAGE(x, y) (max(x, y) - min(x, y) > PAGE_SIZE) */
#define NOTNEIGHBORPAGE(highvaddr, lowvaddr)	\
    ((ULONG_PTR)(highvaddr) - (ULONG_PTR)(lowvaddr) != PAGE_SIZE)


/********************************************************************************/

static void
hptmv_buffer_callback(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	FPSCAT_GATH pSg;
	int idx;

	if (error || nsegs == 0) {
		panic("busdma bewm");
		return;
	}

	pSg = *(FPSCAT_GATH *)arg;

	for (idx = 0; idx < nsegs; idx++) {
		pSg[idx].dSgAddress = (ULONG_PTR)segs[idx].ds_addr;
		pSg[idx].wSgSize = segs[idx].ds_len;
		pSg[idx].wSgFlag = 0;
	}
	pSg[idx - 1].wSgFlag = SG_FLAG_EOT;

	return;
}
	
static int HPTLIBAPI
fOsBuildSgl(_VBUS_ARG PCommand pCmd, FPSCAT_GATH pSg, int logical)
{
	IAL_ADAPTER_T *pAdapter;
	pPrivCommand prvCmd;
	union ccb *ccb;
	struct ccb_hdr *ccb_h;
	struct ccb_scsiio *csio;
	bus_dma_segment_t *sgList;
	int error;

	prvCmd = pCmd->pOrgCommand;
	pAdapter = prvCmd->pAdapter;
	ccb = prvCmd->ccb;
	ccb_h = &ccb->ccb_h;
	csio = &ccb->csio;
	sgList = (bus_dma_segment_t *)(csio->data_ptr);

	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_NONE)
		return TRUE;

	if ((ccb_h->flags & CAM_SCATTER_VALID) != 0) {
		if((ccb_h->flags & CAM_DATA_PHYS) != 0)
			panic(KMSG_LEADING "physical address unsupported!");

		hptmv_buffer_callback(&pSg, sgList, csio->sglist_cnt, 0);
		return TRUE;
	}

	if (logical) {
		if ((ccb_h->flags & CAM_DATA_PHYS) != 0)
			panic(KMSG_LEADING "physical address unsupported\n");

		pSg->dSgAddress = (ULONG_PTR)csio->data_ptr;
		pSg->wSgSize = (USHORT)csio->dxfer_len;
		pSg->wSgFlag = SG_FLAG_EOT;
		return TRUE;
	}

	KdPrint(("use sgl (physical) ...........\n"));

	/*
	 * XXX Hack to make this work with PAE.  It will fail under
	 * heavy load.
	 */
	error = bus_dmamap_load(pAdapter->buf_dmat, prvCmd->buf_map,
	    csio->data_ptr, csio->dxfer_len, hptmv_buffer_callback, &pSg,
	    BUS_DMA_NOWAIT);

	if (error) {
		printf("bus_dmamap_load failed error= %d\n", error);
		return FALSE;
	}

/*#ifdef DEBUG
	do {
		int size, i = 0;
		KdPrintI(("sg[%d]:0x%lx %d\n", i++, pSg[i].dSgAddress,
		    pSg[i].wSgSize));
		size = pSg->wSgSize;
		if (pSg[i].wSgFlag & SG_FLAG_EOT)
			break;
	} while (i<17);
#endif*/

	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(pAdapter->buf_dmat, prvCmd->buf_map,
		    BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(pAdapter->buf_dmat, prvCmd->buf_map,
		    BUS_DMASYNC_PREWRITE);
	}

	return TRUE;
}

/*******************************************************************************/
ULONG HPTLIBAPI
GetStamp(void)
{
	ULONG stamp;

	/* 
	 * the system variable, ticks, can't be used since it hasn't yet been active 
	 * when our driver starts (ticks==0, it's a invalid stamp value)
	 */
	do {
		stamp = random();
	} while (stamp==0);
	
	return stamp;
}


static void
SetInquiryData(PINQUIRYDATA inquiryData, PVDevice pVDev)
{
	int i;
	IDENTIFY_DATA2 *pIdentify;
	
	pIdentify = (IDENTIFY_DATA2*)pVDev->u.disk.mv->identifyDevice;
	inquiryData->DeviceType = T_DIRECT; /*DIRECT_ACCESS_DEVICE*/
	inquiryData->AdditionalLength = (UCHAR)(sizeof(INQUIRYDATA) - 5);
#ifndef SERIAL_CMDS
	inquiryData->CommandQueue = 1;
#endif

	switch(pVDev->VDeviceType) {
	case VD_SINGLE_DISK:
	case VD_ATAPI:
	case VD_REMOVABLE:
		/* Set the removable bit, if applicable. */
		if ((pVDev->u.disk.df_removable_drive) ||
		    (pIdentify->GeneralConfiguration & 0x80))
			inquiryData->RemovableMedia = 1;

		/* Fill in vendor identification fields. */
		for (i = 0; i < 16; i += 2) {				
			inquiryData->VendorId[i] = 
			    ((PUCHAR)pIdentify->ModelNumber)[i + 1];
			inquiryData->VendorId[i+1] =
			    ((PUCHAR)pIdentify->ModelNumber)[i];

		}

		/* Initialize unused portion of product id. */
		for (i = 0; i < 4; i++) inquiryData->ProductId[12+i] = ' ';

		/* firmware revision */
		for (i = 0; i < 4; i += 2) {				
			inquiryData->ProductRevisionLevel[i] =
			    ((PUCHAR)pIdentify->FirmwareRevision)[i+1];
			inquiryData->ProductRevisionLevel[i+1] =
			    ((PUCHAR)pIdentify->FirmwareRevision)[i];
		}
		break;
	default:
		memcpy(&inquiryData->VendorId, "RR182x  ", 8);
#ifdef SUPPORT_ARRAY
		switch(pVDev->VDeviceType) {
		case VD_RAID_0:
			if ((pVDev->u.array.pMember[0] &&
			    mIsArray(pVDev->u.array.pMember[0])) ||
			    (pVDev->u.array.pMember[1] &&
			    mIsArray(pVDev->u.array.pMember[1])))
				memcpy(&inquiryData->ProductId,
				    "RAID 1/0 Array  ", 16);
			else
				memcpy(&inquiryData->ProductId,
				    "RAID 0 Array    ", 16);
			break;
		case VD_RAID_1:
			if ((pVDev->u.array.pMember[0] &&
			    mIsArray(pVDev->u.array.pMember[0])) ||
			    (pVDev->u.array.pMember[1] &&
			    mIsArray(pVDev->u.array.pMember[1])))
				memcpy(&inquiryData->ProductId,
				    "RAID 0/1 Array  ", 16);
			else
				memcpy(&inquiryData->ProductId,
				    "RAID 1 Array    ", 16);
			break;
		case VD_RAID_5:
			memcpy(&inquiryData->ProductId, "RAID 5 Array    ", 16);
			break;
		case VD_JBOD:
			memcpy(&inquiryData->ProductId, "JBOD Array      ", 16);
			break;
		}
#endif
		memcpy(&inquiryData->ProductRevisionLevel, "3.00", 4);
		break;
	}
}

static void
hpt_timeout(void *arg)
{
	_VBUS_INST(&((IAL_ADAPTER_T*)((union ccb *)arg)->ccb_adapter)->VBus)
	intrmask_t oldspl;
	
	oldspl = lock_driver();
	fResetVBus(_VBUS_P0);
	unlock_driver(oldspl);
}

static void HPTLIBAPI
OsSendCommand(_VBUS_ARG union ccb *ccb)
{
	IAL_ADAPTER_T *pAdapter;
	struct ccb_hdr *ccb_h;
	struct ccb_scsiio *csio;
	PVDevice pVDev;
	
	pAdapter = (IAL_ADAPTER_T *)ccb->ccb_adapter;
	ccb_h = &ccb->ccb_h;
	csio = &ccb->csio;
	pVDev = pAdapter->VBus.pVDevice[ccb_h->target_id];

	KdPrintI(("OsSendCommand: ccb %p cdb %x-%x-%x\n",
		ccb,
		*(ULONG *)&ccb->csio.cdb_io.cdb_bytes[0],
		*(ULONG *)&ccb->csio.cdb_io.cdb_bytes[4],
		*(ULONG *)&ccb->csio.cdb_io.cdb_bytes[8]
	));

	pAdapter->outstandingCommands++;

	if (pVDev == NULL || pVDev->vf_online == 0) {
		ccb->ccb_h.status = CAM_REQ_INVALID;
		ccb_done(ccb);
		goto Command_Complished;
	}

	switch(ccb->csio.cdb_io.cdb_bytes[0])
	{
	case TEST_UNIT_READY:
	case START_STOP_UNIT:
	case SYNCHRONIZE_CACHE:
		/* FALLTHROUGH */
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;

	case INQUIRY:
		ZeroMemory(ccb->csio.data_ptr, ccb->csio.dxfer_len);
		SetInquiryData((PINQUIRYDATA)ccb->csio.data_ptr, pVDev);
		ccb_h->status = CAM_REQ_CMP;
		break;

	case READ_CAPACITY:
	{
		UCHAR swip[4];
		/* Claim 512 byte blocks (big-endian). */
		((PREAD_CAPACITY_DATA)csio->data_ptr)->BytesPerBlock = 0x20000;
		*(ULONG*)swip = pVDev->VDeviceCapacity - 1;
		((PREAD_CAPACITY_DATA)csio->data_ptr)->LogicalBlockAddress =
			(swip[0] << 24) |  (swip[1] << 16) | (swip[2] << 8) | swip[3];
		ccb_h->status = CAM_REQ_CMP;
		break;
	}

	case READ_6:
	case WRITE_6:
	case READ_10:
	case WRITE_10:
	case 0x13:
	case 0x2f:
	{
		UCHAR Cdb[16];
		UCHAR CdbLength;
		_VBUS_INST(pVDev->pVBus)
		PCommand pCmd;

		pCmd = AllocateCommand(_VBUS_P0);
		HPT_ASSERT(pCmd);

		CdbLength = csio->cdb_len;
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) == 0) {
				bcopy(csio->cdb_io.cdb_ptr, Cdb, CdbLength);
			} else {
				KdPrintE(("ERROR!!!\n"));
				ccb->ccb_h.status = CAM_REQ_INVALID;
				break;
			}
		} else {
			bcopy(csio->cdb_io.cdb_bytes, Cdb, CdbLength);
		}

		pCmd->pOrgCommand = AllocPrivCommand(pAdapter);
		if (pCmd->pOrgCommand == NULL)
			panic("command leak!");
		((pPrivCommand)(pCmd->pOrgCommand))->ccb = ccb;
		pCmd->pVDevice = pVDev;
		pCmd->pfnCompletion = fOsCommandDone;
		pCmd->pfnBuildSgl = fOsBuildSgl;

		switch (Cdb[0]) {
		case READ_6:
		case WRITE_6:
		case 0x13:
			pCmd->uCmd.Ide.Lba =  ((ULONG)Cdb[1] << 16) |
			    ((ULONG)Cdb[2] << 8) | (ULONG)Cdb[3];
			pCmd->uCmd.Ide.nSectors = (USHORT) Cdb[4];
			break;
		default:
			pCmd->uCmd.Ide.Lba = (ULONG)Cdb[5] |
			    ((ULONG)Cdb[4] << 8) | ((ULONG)Cdb[3] << 16) |
			    ((ULONG)Cdb[2] << 24);
			pCmd->uCmd.Ide.nSectors = (USHORT) Cdb[8] |
			    ((USHORT)Cdb[7]<<8);
			break;
		}

		switch (Cdb[0]) {
		case READ_6:
		case READ_10:
			pCmd->uCmd.Ide.Command = IDE_COMMAND_READ;
			pCmd->cf_data_in = 1;
			break;

		case WRITE_6:
		case WRITE_10:
			pCmd->uCmd.Ide.Command = IDE_COMMAND_WRITE;
			pCmd->cf_data_out = 1;
			break;
		case 0x13:
		case 0x2f:
			pCmd->uCmd.Ide.Command = IDE_COMMAND_VERIFY;
			break;
		}

		ccb->ccb_h.timeout_ch = timeout(hpt_timeout, (caddr_t)ccb, 20*hz);

		pVDev->pfnSendCommand(_VBUS_P pCmd);
		goto Command_Complished;
	}

	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	ccb_done(ccb);
Command_Complished:
	CheckPendingCall(_VBUS_P0);
	return;
}

static void HPTLIBAPI
fOsCommandDone(_VBUS_ARG PCommand pCmd)
{
	IAL_ADAPTER_T *pAdapter;
	pPrivCommand prvCmd;
	union ccb *ccb;

	prvCmd = pCmd->pOrgCommand;
	pAdapter = prvCmd->pAdapter;
	ccb = prvCmd->ccb;

	KdPrint(("fOsCommandDone(%p, %d)", pCmd, pCmd->Result));
	
	untimeout(hpt_timeout, (caddr_t)ccb, ccb->ccb_h.timeout_ch);
	
	switch(pCmd->Result) {
	case RETURN_SUCCESS:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case RETURN_BAD_DEVICE:
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		break;
	case RETURN_DEVICE_BUSY:
		ccb->ccb_h.status = CAM_BUSY;
		break;
	case RETURN_INVALID_REQUEST:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	case RETURN_SELECTION_TIMEOUT:
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;
		break;
	case RETURN_RETRY:
		ccb->ccb_h.status = CAM_BUSY;
		break;
	default:
		ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
		break;
	}

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(pAdapter->buf_dmat, prvCmd->buf_map,
		    BUS_DMASYNC_POSTREAD);
	} else {
		bus_dmamap_sync(pAdapter->buf_dmat, prvCmd->buf_map,
		    BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_unload(pAdapter->buf_dmat, prvCmd->buf_map);

	FreePrivCommand(pAdapter, prvCmd);
	FreeCommand(_VBUS_P pCmd);
	ccb_done(ccb);
}

int
hpt_queue_dpc(HPT_DPC dpc, IAL_ADAPTER_T * pAdapter, void *arg, UCHAR flags)
{
	int p;

	p = (DpcQueue_Last + 1) % MAX_DPC;
	if (p==DpcQueue_First) {
		KdPrint(("DPC Queue full!\n"));
		return -1;
	}

	DpcQueue[DpcQueue_Last].dpc = dpc;
	DpcQueue[DpcQueue_Last].pAdapter = pAdapter;
	DpcQueue[DpcQueue_Last].arg = arg;
	DpcQueue[DpcQueue_Last].flags = flags;
	DpcQueue_Last = p;

	return 0;
}

#ifdef _RAID5N_
/* 
 * Allocate memory above 16M, otherwise we may eat all low memory for ISA
 * devices.
 *
 * Busdma should be used here, not contigmalloc/free.  However, this API
 * will need to be changed to use it effective.
 */
void
*os_alloc_page(_VBUS_ARG0)
{ 
	return (void *)contigmalloc(0x1000, M_DEVBUF, M_NOWAIT, 0x1000000,
	    0xffffffff, PAGE_SIZE, 0);
}
void
*os_alloc_dma_page(_VBUS_ARG0)
{
	return (void *)contigmalloc(0x1000, M_DEVBUF, M_NOWAIT, 0x1000000,
	    0xffffffff, PAGE_SIZE, 0);
}

/*
 * The next two are not used right now.
 */
void
os_free_page(_VBUS_ARG void *p)
{
	contigfree(p, 0x1000, M_DEVBUF);
}

void
os_free_dma_page(_VBUS_ARG void *p)
{
	contigfree(p, 0x1000, M_DEVBUF);
}

void
DoXor1(ULONG *p0, ULONG *p1, ULONG *p2, UINT nBytes)
{
	UINT i;
	for (i = 0; i < nBytes / 4; i++)
		*p0++ = *p1++ ^ *p2++;
}

void
DoXor2(ULONG *p0, ULONG *p2, UINT nBytes)
{
	UINT i;
	for (i = 0; i < nBytes / 4; i++)
		*p0++ ^= *p2++;
}
#endif
