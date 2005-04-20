/*
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
 */
/*
 * gui_lib.c
 * Copyright (c) 2002-2004 HighPoint Technologies, Inc. All rights reserved.
 *
 *  Platform independent ioctl interface implementation.
 *  The platform dependent part may reuse this function and/or use it own 
 *  implementation for each ioctl function.
 *
 *  This implementation doesn't use any synchronization; the caller must
 *  assure the proper context when calling these functions.
 */
 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/hptmv/global.h>
#include <dev/hptmv/hptintf.h>
#include <dev/hptmv/osbsd.h>
#include <contrib/dev/hptmv/access601.h>

static int hpt_get_driver_capabilities(PDRIVER_CAPABILITIES cap);
static int hpt_get_controller_count(void);
static int hpt_get_controller_info(int id, PCONTROLLER_INFO pInfo);
static int hpt_get_channel_info(int id, int bus, PCHANNEL_INFO pInfo);
static int hpt_get_logical_devices(DEVICEID * pIds, int nMaxCount);
static int hpt_get_device_info(DEVICEID id, PLOGICAL_DEVICE_INFO pInfo);
static DEVICEID hpt_create_array(_VBUS_ARG PCREATE_ARRAY_PARAMS pParam);
static int hpt_add_spare_disk(_VBUS_ARG DEVICEID idDisk);
static int hpt_remove_spare_disk(_VBUS_ARG DEVICEID idDisk);
static int hpt_set_array_info(_VBUS_ARG DEVICEID idArray, PALTERABLE_ARRAY_INFO pInfo);
static int hpt_set_device_info(_VBUS_ARG DEVICEID idDisk, PALTERABLE_DEVICE_INFO pInfo);

int check_VDevice_valid(PVDevice p)
{
	int i;
	PVDevice pVDevice;
	PVBus    _vbus_p;
	IAL_ADAPTER_T *pAdapter = gIal_Adapter;
	
	while(pAdapter != 0)
	{
		for (i = 0; i < MV_SATA_CHANNELS_NUM; i++)
			if(&(pAdapter->VDevices[i]) == p)  return 0;
		pAdapter = pAdapter->next;
	}

#ifdef SUPPORT_ARRAY
	pAdapter = gIal_Adapter;
	while(pAdapter != 0)
	{
		_vbus_p = &pAdapter->VBus;
		for (i=0;i<MAX_ARRAY_PER_VBUS;i++) 
		{
			pVDevice=ArrayTables(i);
			if ((pVDevice->u.array.dArStamp != 0) && (pVDevice == p))
				return 0;
		}
		pAdapter = pAdapter->next;
	}
#endif

	return -1;
}

#ifdef SUPPORT_ARRAY
static void get_array_info(PVDevice pVDevice, PLOGICAL_DEVICE_INFO pInfo)
{
	int	i;

	pInfo->Type = LDT_ARRAY;
	pInfo->Capacity = pVDevice->VDeviceCapacity;
	pInfo->ParentArray = VDEV_TO_ID(pVDevice->pParent);

	memcpy(pInfo->u.array.Name, pVDevice->u.array.ArrayName, MAX_ARRAY_NAME);

	switch( pVDevice->VDeviceType )
	{
		case VD_RAID_0:
			pInfo->u.array.ArrayType = AT_RAID0;
			break;
		case VD_RAID_1:
			pInfo->u.array.ArrayType = AT_RAID1;
			break;
		case VD_JBOD:
			pInfo->u.array.ArrayType = AT_JBOD;
			break;
		case VD_RAID_5:
			pInfo->u.array.ArrayType = AT_RAID5;
			break;
		default:
			pInfo->u.array.ArrayType = AT_UNKNOWN;
	}

	pInfo->u.array.BlockSizeShift = pVDevice->u.array.bArBlockSizeShift;

	pInfo->u.array.RebuiltSectors = pVDevice->u.array.RebuildSectors;	

	/* The array is disabled */
	if(!pVDevice->vf_online)	{
		pInfo->u.array.Flags |= ARRAY_FLAG_DISABLED;
		goto ignore_info;
	}

	/* array need synchronizing */
	if(pVDevice->u.array.rf_need_rebuild && !pVDevice->u.array.rf_duplicate_and_create)
		pInfo->u.array.Flags |= ARRAY_FLAG_NEEDBUILDING;

	pInfo->u.array.RebuildingProgress = ((pVDevice->u.array.RebuildSectors>>11)*1000 / 
		(pVDevice->VDeviceCapacity>>11) * (pVDevice->u.array.bArnMember-1)) * 10;

	/* array is in rebuilding process */
	if(pVDevice->u.array.rf_rebuilding)
		pInfo->u.array.Flags |= ARRAY_FLAG_REBUILDING;

	/* array is being verified */
	if(pVDevice->u.array.rf_verifying)
		pInfo->u.array.Flags |= ARRAY_FLAG_VERIFYING;

	/* array is being initialized */
	if(pVDevice->u.array.rf_initializing)
		pInfo->u.array.Flags |= ARRAY_FLAG_INITIALIZING;

	/* broken but may still working */
	if(pVDevice->u.array.rf_broken)
		pInfo->u.array.Flags |= ARRAY_FLAG_BROKEN;

	/* array has a active partition */
	if(pVDevice->vf_bootable)
		pInfo->u.array.Flags |= ARRAY_FLAG_BOOTDISK;

	/* a newly created array */
	if(pVDevice->u.array.rf_newly_created)
		pInfo->u.array.Flags |= ARRAY_FLAG_NEWLY_CREATED;

	/* array has boot mark set */
	if(pVDevice->vf_bootmark)
		pInfo->u.array.Flags |= ARRAY_FLAG_BOOTMARK;

	/* auto-rebuild should start */
	if(pVDevice->u.array.rf_auto_rebuild)
		pInfo->u.array.Flags |= ARRAY_FLAG_NEED_AUTOREBUILD;

	for(i = 0; i < pVDevice->u.array.bArnMember; i++)
	{
		PVDevice pMember = pVDevice->u.array.pMember[i];
		if (!pMember || !pMember->vf_online || (pMember->VDeviceType==VD_SINGLE_DISK))
			continue;

		/* array need synchronizing */
		if(pMember->u.array.rf_need_rebuild && 
		   !pMember->u.array.rf_duplicate_and_create)
			pInfo->u.array.Flags |= ARRAY_FLAG_NEEDBUILDING;

		/* array is in rebuilding process */
		if(pMember->u.array.rf_rebuilding)
			pInfo->u.array.Flags |= ARRAY_FLAG_REBUILDING;
		
		/* array is being verified */	
		if(pMember->u.array.rf_verifying)
			pInfo->u.array.Flags |= ARRAY_FLAG_VERIFYING;
			
		/* array is being initialized */
		if(pMember->u.array.rf_initializing)
			pInfo->u.array.Flags |= ARRAY_FLAG_INITIALIZING;

		/* broken but may still working */
		if(pMember->u.array.rf_broken)
			pInfo->u.array.Flags |= ARRAY_FLAG_BROKEN;

		/* a newly created array */
		if(pMember->u.array.rf_newly_created)
			pInfo->u.array.Flags |= ARRAY_FLAG_NEWLY_CREATED;

		/* auto-rebuild should start */
		if(pMember->u.array.rf_auto_rebuild)
			pInfo->u.array.Flags |= ARRAY_FLAG_NEED_AUTOREBUILD;

		/* for RAID1/0 case */
		if (pMember->u.array.rf_rebuilding || 
			pMember->u.array.rf_verifying ||
			pMember->u.array.rf_initializing)
		{
			DWORD percent = ((pMember->u.array.RebuildSectors>>11)*1000 / 
				(pMember->VDeviceCapacity>>11) * (pMember->u.array.bArnMember-1)) * 10;
			if (pInfo->u.array.RebuildingProgress==0 || 
				pInfo->u.array.RebuildingProgress>percent)
				pInfo->u.array.RebuildingProgress = percent;
		}
	}
	
	if (pInfo->u.array.RebuildingProgress>10000)
		pInfo->u.array.RebuildingProgress = 10000;

ignore_info:

	pInfo->u.array.nDisk = 0;
	for(i=0; i<MAX_ARRAY_MEMBERS; i++)
		pInfo->u.array.Members[i] = INVALID_DEVICEID;

	for(i = 0; i < pVDevice->u.array.bArnMember; i++)
	{
		if(pVDevice->u.array.pMember[i] != NULL)
		{
			pInfo->u.array.Members[pInfo->u.array.nDisk] = VDEV_TO_ID(pVDevice->u.array.pMember[i]);
			pInfo->u.array.nDisk++;
		}
	}
}
#endif

static int get_disk_info(PVDevice pVDevice, PLOGICAL_DEVICE_INFO pInfo)
{
	MV_SATA_ADAPTER *pSataAdapter;
	MV_SATA_CHANNEL *pSataChannel;
	IAL_ADAPTER_T   *pAdapter;
	char *p;
	int i;

	pInfo->Type = LDT_DEVICE;

	if (pVDevice->pParent)
		pInfo->ParentArray = VDEV_TO_ID(pVDevice->pParent);
	else
		pInfo->ParentArray = INVALID_DEVICEID;

	/* report real capacity to be compatible with old arrays */
	pInfo->Capacity = pVDevice->u.disk.dDeRealCapacity;

	/* device location */
	pSataChannel = pVDevice->u.disk.mv;
	if(pSataChannel == NULL)	return -1;	
	pInfo->u.device.TargetId = 0;
	pSataAdapter = pSataChannel->mvSataAdapter;
	if(pSataAdapter == NULL)	return -1;

	pAdapter = pSataAdapter->IALData;

	pInfo->u.device.PathId = pSataChannel->channelNumber;
	pInfo->u.device.ControllerId = (UCHAR)pSataAdapter->adapterId;

/*GUI uses DeviceModeSetting to display to users
(1) if users select a mode, GUI/BIOS should display that mode.
(2) if SATA/150, GUI/BIOS should display 150 if case (1) isn't satisfied.
(3) display real mode if case (1)&&(2) not satisfied.
*/
	if (pVDevice->u.disk.df_user_mode_set)
		pInfo->u.device.DeviceModeSetting = pVDevice->u.disk.bDeUserSelectMode;
	else if ((((PIDENTIFY_DATA)pVDevice->u.disk.mv->identifyDevice)->SataCapability & 3)==2) 
		pInfo->u.device.DeviceModeSetting = 15;
	else {
		p = (char *)&((PIDENTIFY_DATA)pVDevice->u.disk.mv->identifyDevice)->ModelNumber;
		if (*(WORD*)p==0x5354 /*'ST'*/ &&
			(*(WORD*)(p+8)==0x4153/*'AS'*/ || (p[8]=='A' && p[11]=='S')))
			pInfo->u.device.DeviceModeSetting = 15;
		else
			pInfo->u.device.DeviceModeSetting = pVDevice->u.disk.bDeModeSetting;
	}
		
	pInfo->u.device.UsableMode = pVDevice->u.disk.bDeUsable_Mode;

	pInfo->u.device.DeviceType = PDT_HARDDISK;

	pInfo->u.device.Flags = 0x0;

	/* device is disabled */
	if(!pVDevice->u.disk.df_on_line)
		pInfo->u.device.Flags |= DEVICE_FLAG_DISABLED;

	/* disk has a active partition */
	if(pVDevice->vf_bootable)
		pInfo->u.device.Flags |= DEVICE_FLAG_BOOTDISK;

	/* disk has boot mark set */
	if(pVDevice->vf_bootmark)
		pInfo->u.device.Flags |= DEVICE_FLAG_BOOTMARK;

	pInfo->u.device.Flags |= DEVICE_FLAG_SATA;

	/* is a spare disk */
	if(pVDevice->VDeviceType == VD_SPARE)
		pInfo->u.device.Flags |= DEVICE_FLAG_IS_SPARE;

	memcpy(&(pInfo->u.device.IdentifyData), (pSataChannel->identifyDevice), sizeof(IDENTIFY_DATA2));
	p = (char *)&pInfo->u.device.IdentifyData.ModelNumber;
	for (i = 0; i < 20; i++)
		((WORD*)p)[i] = shortswap(pSataChannel->identifyDevice[IDEN_MODEL_OFFSET+i]);
	p[39] = '\0';

	return 0;
}

static int hpt_get_driver_capabilities(PDRIVER_CAPABILITIES cap)
{
	ZeroMemory(cap, sizeof(DRIVER_CAPABILITIES));
	cap->dwSize = sizeof(DRIVER_CAPABILITIES);
	cap->MaximumControllers = MAX_VBUS;

	/* cap->SupportCrossControllerRAID = 0; */
	/* take care for various OSes! */
	cap->SupportCrossControllerRAID = 0;


	cap->MinimumBlockSizeShift = MinBlockSizeShift;
	cap->MaximumBlockSizeShift = MaxBlockSizeShift;
	cap->SupportDiskModeSetting = 0;
	cap->SupportSparePool = 1;		
	cap->MaximumArrayNameLength = MAX_ARRAY_NAME - 1;
	cap->SupportDedicatedSpare = 0;
	
#ifdef SUPPORT_HOTSWAP
	cap->SupportHotSwap = 1;
#endif

#ifdef SUPPORT_ARRAY
	/* Stripe */
	cap->SupportedRAIDTypes[0] = AT_RAID0;
	cap->MaximumArrayMembers[0] = MAX_MEMBERS;
	/* Mirror */
	cap->SupportedRAIDTypes[1] = AT_RAID1;
	cap->MaximumArrayMembers[1] = 2;
	/* Mirror + Stripe */
#ifdef ARRAY_V2_ONLY
	cap->SupportedRAIDTypes[2] = (AT_RAID1<<4)|AT_RAID0; /* RAID0/1 */
#else
	cap->SupportedRAIDTypes[2] = (AT_RAID0<<4)|AT_RAID1; /* RAID1/0 */
#endif
	cap->MaximumArrayMembers[2] = MAX_MEMBERS;
	/* Jbod */
	cap->SupportedRAIDTypes[3] = AT_JBOD;
	cap->MaximumArrayMembers[3] = MAX_MEMBERS;
	/* RAID5 */
#if SUPPORT_RAID5
	cap->SupportedRAIDTypes[4] = AT_RAID5;
	cap->MaximumArrayMembers[4] = MAX_MEMBERS;
#endif
#if 0 /* don't let GUI create RAID 0/1. */
	/* Stripe + Mirror */
	cap->SupportedRAIDTypes[5] = (AT_RAID1<<4)|AT_RAID0;
	cap->MaximumArrayMembers[5] = 4;
#endif
#endif /* SUPPORT_ARRAY */
	return 0;
}

static int hpt_get_controller_count(void)
{
	IAL_ADAPTER_T    *pAdapTemp = gIal_Adapter;
	int iControllerCount = 0;
	
	while(pAdapTemp != 0)
	{		 
		iControllerCount++;
		pAdapTemp = pAdapTemp->next;
	}
	
	return iControllerCount;
}

static int hpt_get_controller_info(int id, PCONTROLLER_INFO pInfo)
{
	IAL_ADAPTER_T    *pAdapTemp;
	int iControllerCount = 0;

	for (pAdapTemp = gIal_Adapter; pAdapTemp; pAdapTemp = pAdapTemp->next) {
		if (iControllerCount++==id) {
			pInfo->InterruptLevel = 0;
			pInfo->ChipType = 0;
			pInfo->ChipFlags = CHIP_SUPPORT_ULTRA_100;
			strcpy( pInfo->szVendorID, "HighPoint Technologies, Inc.");
#ifdef GUI_CONTROLLER_NAME
#ifdef FORCE_ATA150_DISPLAY
			/* show "Bus Type: ATA/150" in GUI for SATA controllers */
			pInfo->ChipFlags = CHIP_SUPPORT_ULTRA_150;
#endif
			strcpy(pInfo->szProductID, GUI_CONTROLLER_NAME);
#define _set_product_id(x)
#else
#define _set_product_id(x) strcpy(pInfo->szProductID, x)
#endif
			_set_product_id("RocketRAID 182x SATA Controller");			
			pInfo->NumBuses = 8;
			pInfo->ChipFlags |= CHIP_SUPPORT_ULTRA_133|CHIP_SUPPORT_ULTRA_150;
			return 0;
		}
	}
	return -1;
}


static int hpt_get_channel_info(int id, int bus, PCHANNEL_INFO pInfo)
{
	IAL_ADAPTER_T    *pAdapTemp = gIal_Adapter;
	int i,iControllerCount = 0;

	while(pAdapTemp != 0)
	{
		if (iControllerCount++==id) 
			goto found;
		pAdapTemp = pAdapTemp->next;
	}
	return -1;

found:
	
	pInfo->IoPort = 0;
	pInfo->ControlPort = 0;
	
	for (i=0; i<2 ;i++)
	{
		pInfo->Devices[i] = (DEVICEID)INVALID_DEVICEID;
	}

	if (pAdapTemp->mvChannel[bus].online == MV_TRUE)
		pInfo->Devices[0] = VDEV_TO_ID(&pAdapTemp->VDevices[bus]);
	else
		pInfo->Devices[0] = (DEVICEID)INVALID_DEVICEID;

	return 0;
	

}

static int hpt_get_logical_devices(DEVICEID * pIds, int nMaxCount)
{
	int count = 0;
	int	i,j;
	PVDevice pPhysical, pLogical;
	IAL_ADAPTER_T    *pAdapTemp;

	for(i = 0; i < nMaxCount; i++)
		pIds[i] = INVALID_DEVICEID;

	/* append the arrays not registered on VBus */
	for (pAdapTemp = gIal_Adapter; pAdapTemp; pAdapTemp = pAdapTemp->next) {
		for(i = 0; i < MV_SATA_CHANNELS_NUM; i++)
		{
			pPhysical = &pAdapTemp->VDevices[i];
			pLogical = pPhysical;
			
			while (pLogical->pParent) pLogical = pLogical->pParent;
			if (pLogical->VDeviceType==VD_SPARE)
				continue;
			
			for (j=0; j<count; j++)
				if (pIds[j]==VDEV_TO_ID(pLogical)) goto next;
			pIds[count++] = VDEV_TO_ID(pLogical);
			if (count>=nMaxCount) goto done;
			next:;
		}
	}

done:
	return count;
}

static int hpt_get_device_info(DEVICEID id, PLOGICAL_DEVICE_INFO pInfo)
{
	PVDevice pVDevice = ID_TO_VDEV(id);

	if((id == HPT_NULL_ID) || check_VDevice_valid(pVDevice))
		return -1;

#ifdef SUPPORT_ARRAY
	if (mIsArray(pVDevice))
		get_array_info(pVDevice, pInfo);
	else
#endif
	return	get_disk_info(pVDevice, pInfo);

	return 0;
}

#ifdef SUPPORT_ARRAY
static DEVICEID hpt_create_array(_VBUS_ARG PCREATE_ARRAY_PARAMS pParam)
{
	ULONG Stamp = GetStamp();
	int	i,j;
	ULONG  capacity = MAX_LBA_T;
	PVDevice pArray,pChild;
#if MAX_VBUS==1
	PVBus	_vbus_p = NULL;
#endif
	int		Loca = -1;

	for(i = 0; i < pParam->nDisk; i++)
	{
		PVDevice pVDev = ID_TO_VDEV(pParam->Members[i]);
		if (check_VDevice_valid(pVDev)) return INVALID_DEVICEID;
		if (mIsArray(pVDev)) return INVALID_DEVICEID;
		if (!pVDev->vf_online) return INVALID_DEVICEID;
		if (!_vbus_p)
			_vbus_p = pVDev->u.disk.pVBus;
		else if (_vbus_p != pVDev->u.disk.pVBus)
			return INVALID_DEVICEID;
	}
	if (!_vbus_p) return INVALID_DEVICEID;

	mArGetArrayTable(pArray);
	if(!pArray)	return INVALID_DEVICEID;

	switch (pParam->ArrayType)
	{
		case AT_JBOD:
			pArray->VDeviceType = VD_JBOD;
			goto simple;

		case AT_RAID0:
			if((pParam->BlockSizeShift < MinBlockSizeShift) || (pParam->BlockSizeShift > MaxBlockSizeShift))
				goto error;
			pArray->VDeviceType = VD_RAID_0;
			goto simple;

		case AT_RAID5:
			if((pParam->BlockSizeShift < MinBlockSizeShift) || (pParam->BlockSizeShift > MaxBlockSizeShift))
				goto error;
			pArray->VDeviceType = VD_RAID_5;
			/* only "no build" R5 is not critical after creation. */
			if ((pParam->CreateFlags & CAF_CREATE_R5_NO_BUILD)==0)
				pArray->u.array.rf_need_rebuild = 1;
			goto simple;

		case AT_RAID1:
			if(pParam->nDisk <= 2)
			{
				pArray->VDeviceType = VD_RAID_1;
simple:
				pArray->u.array.bArnMember = pParam->nDisk;
				pArray->u.array.bArRealnMember = pParam->nDisk;
				pArray->u.array.bArBlockSizeShift = pParam->BlockSizeShift;
				pArray->u.array.bStripeWitch = (1 << pParam->BlockSizeShift);
				pArray->u.array.dArStamp = Stamp;

				pArray->u.array.rf_need_sync = 1;
				pArray->u.array.rf_newly_created = 1;

				if ((pParam->CreateFlags & CAF_CREATE_AND_DUPLICATE) && 
					(pArray->VDeviceType == VD_RAID_1))
				{
					pArray->u.array.rf_newly_created = 0; /* R1 shall still be accessible */
					pArray->u.array.rf_need_rebuild = 1;
					pArray->u.array.rf_auto_rebuild = 1;
					pArray->u.array.rf_duplicate_and_create = 1;

					for(i = 0; i < MAX_VDEVICE_PER_VBUS; i++)
						if (_vbus_p->pVDevice[i] == ID_TO_VDEV(pParam->Members[0]))
							Loca = i;
				}
				
				pArray->u.array.RebuildSectors = pArray->u.array.rf_need_rebuild? 0 : MAX_LBA_T;

				memcpy(pArray->u.array.ArrayName, pParam->ArrayName, MAX_ARRAY_NAME);

				for(i = 0; i < pParam->nDisk; i++)
				{
					pArray->u.array.pMember[i] = ID_TO_VDEV(pParam->Members[i]);
					pArray->u.array.pMember[i]->bSerialNumber = i;
					pArray->u.array.pMember[i]->pParent = pArray;

					/* don't unregister source disk for duplicate RAID1 */
					if (i ||
						pArray->VDeviceType!=VD_RAID_1 ||
						(pParam->CreateFlags & CAF_CREATE_AND_DUPLICATE)==0)
						UnregisterVDevice(pArray->u.array.pMember[i]);

					if(pArray->VDeviceType == VD_RAID_5)
						pArray->u.array.pMember[i]->vf_cache_disk = 1;
				}
			}
			else
			{
				for(i = 0; i < (pParam->nDisk / 2); i++)
				{
					mArGetArrayTable(pChild);
					pChild->VDeviceType = VD_RAID_1;

					pChild->u.array.bArnMember = 2;
					pChild->u.array.bArRealnMember = 2;
					pChild->u.array.bArBlockSizeShift = pParam->BlockSizeShift;
					pChild->u.array.bStripeWitch = (1 << pParam->BlockSizeShift);
					pChild->u.array.dArStamp = Stamp;

					pChild->u.array.rf_need_sync = 1;
					pChild->u.array.rf_newly_created = 1;

					pChild->u.array.RebuildSectors = MAX_LBA_T;	
					
					memcpy(pChild->u.array.ArrayName, pParam->ArrayName, MAX_ARRAY_NAME);

					for(j = 0; j < 2; j++)
					{
						pChild->u.array.pMember[j] = ID_TO_VDEV(pParam->Members[i*2 + j]);
						pChild->u.array.pMember[j]->bSerialNumber = j;
						pChild->u.array.pMember[j]->pParent = pChild;
						pChild->u.array.pMember[j]->pfnDeviceFailed = pfnDeviceFailed[pChild->VDeviceType];
						UnregisterVDevice(pChild->u.array.pMember[j]);
					}

					pArray->u.array.pMember[i] = pChild;

					pChild->vf_online = 1;
					pChild->bSerialNumber = i;
					pChild->pParent = pArray;
					pChild->VDeviceCapacity = MIN(pChild->u.array.pMember[0]->VDeviceCapacity,
						pChild->u.array.pMember[1]->VDeviceCapacity);

					pChild->pfnSendCommand = pfnSendCommand[pChild->VDeviceType];
					pChild->pfnDeviceFailed = pfnDeviceFailed[VD_RAID_0];
				}

				pArray->VDeviceType = VD_RAID_0;

				pArray->u.array.bArnMember = pParam->nDisk / 2;
				pArray->u.array.bArRealnMember = pParam->nDisk / 2;
				pArray->u.array.bArBlockSizeShift = pParam->BlockSizeShift;
				pArray->u.array.bStripeWitch = (1 << pParam->BlockSizeShift);
				pArray->u.array.dArStamp = Stamp;

				pArray->u.array.rf_need_sync = 1;
				pArray->u.array.rf_newly_created = 1;

				memcpy(pArray->u.array.ArrayName, pParam->ArrayName, MAX_ARRAY_NAME);
			}
			break;

		default:
			goto error;
	}

	for(i = 0; i < pArray->u.array.bArnMember; i++)
		pArray->u.array.pMember[i]->pfnDeviceFailed = pfnDeviceFailed[pArray->VDeviceType];

	if ((pParam->CreateFlags & CAF_CREATE_AND_DUPLICATE) && 
		(pArray->VDeviceType == VD_RAID_1))
	{
		pArray->vf_bootmark = pArray->u.array.pMember[0]->vf_bootmark;
		pArray->vf_bootable = pArray->u.array.pMember[0]->vf_bootable;
		pArray->u.array.pMember[0]->vf_bootable = 0;
		pArray->u.array.pMember[0]->vf_bootmark = 0;
		if (Loca>=0) {
			_vbus_p->pVDevice[Loca] = pArray;
			/* to comfort OS */
			pArray->u.array.rf_duplicate_and_created = 1;
			pArray->pVBus = _vbus_p;
		}
	}
	else {
		UCHAR TempBuffer[512];
		ZeroMemory(TempBuffer, 512);
		for(i = 0; i < pParam->nDisk; i++)
		{
			PVDevice	pDisk = ID_TO_VDEV(pParam->Members[i]);
			pDisk->vf_bootmark = pDisk->vf_bootable = 0;
			fDeReadWrite(&pDisk->u.disk, 0, IDE_COMMAND_WRITE, TempBuffer);
		}
	}

	pArray->vf_online = 1;
	pArray->pParent = NULL;

	switch(pArray->VDeviceType)
	{
		case VD_RAID_0:
			for(i = 0; i < pArray->u.array.bArnMember; i++)
				if(pArray->u.array.pMember[i]->VDeviceCapacity < capacity)
					capacity = pArray->u.array.pMember[i]->VDeviceCapacity;
#ifdef ARRAY_V2_ONLY
			capacity -= 10;
#endif
			capacity &= ~(pArray->u.array.bStripeWitch - 1);
			/* shrink member capacity for RAID 1/0 */
			for(i = 0; i < pArray->u.array.bArnMember; i++)
				if (mIsArray(pArray->u.array.pMember[i]))
					pArray->u.array.pMember[i]->VDeviceCapacity = capacity;
			pArray->VDeviceCapacity = capacity * pArray->u.array.bArnMember;
			break;

		case VD_RAID_1:
			pArray->VDeviceCapacity = MIN(pArray->u.array.pMember[0]->VDeviceCapacity,
						pArray->u.array.pMember[1]->VDeviceCapacity);
			break;

		case VD_JBOD:
			for(i = 0; i < pArray->u.array.bArnMember; i++)
				pArray->VDeviceCapacity += pArray->u.array.pMember[i]->VDeviceCapacity
#ifdef ARRAY_V2_ONLY
				-10
#endif
				;
			break;

		case VD_RAID_5:
			for(i = 0; i < pArray->u.array.bArnMember; i++)
				if(pArray->u.array.pMember[i]->VDeviceCapacity < capacity)
					capacity = pArray->u.array.pMember[i]->VDeviceCapacity;
			pArray->VDeviceCapacity = (capacity & ~(pArray->u.array.bStripeWitch - 1))
				* (pArray->u.array.bArnMember - 1);
			break;

		default:
			goto error;
	}

	pArray->pfnSendCommand = pfnSendCommand[pArray->VDeviceType];
	pArray->pfnDeviceFailed = fOsDiskFailed;
	SyncArrayInfo(pArray);

	if (!pArray->u.array.rf_duplicate_and_created)
		RegisterVDevice(pArray);
	return VDEV_TO_ID(pArray);

error:
	for(i = 0; i < pArray->u.array.bArnMember; i++)
	{
		pChild = pArray->u.array.pMember[i];
		if((pChild != NULL) && (pChild->VDeviceType != VD_SINGLE_DISK))
			mArFreeArrayTable(pChild);
	}
	mArFreeArrayTable(pArray);
	return INVALID_DEVICEID;
}

#ifdef SUPPORT_OLD_ARRAY
/* this is only for old RAID 0/1 */
int old_add_disk_to_raid01(_VBUS_ARG DEVICEID idArray, DEVICEID idDisk)
{
	PVDevice pArray1 = ID_TO_VDEV(idArray);
	PVDevice pArray2 = 0;
	PVDevice pDisk	= ID_TO_VDEV(idDisk);
	int	i;
	IAL_ADAPTER_T *pAdapter = gIal_Adapter;

#if MAX_VBUS>1
	if (pArray1->pVBus!=_vbus_p) { HPT_ASSERT(0); return -1;}
#endif
	
	if(pDisk->u.disk.dDeRealCapacity < (pArray1->VDeviceCapacity / 2))
		return -1;
		
	pArray2 = pArray1->u.array.pMember[1];
	if(pArray2 == NULL)	{
		/* create a Stripe */		
		mArGetArrayTable(pArray2);
		pArray2->VDeviceType = VD_RAID_0;
		pArray2->u.array.dArStamp = GetStamp();
		pArray2->vf_format_v2 = 1;	
		pArray2->u.array.rf_broken = 1;	
		pArray2->u.array.bArBlockSizeShift = pArray1->u.array.bArBlockSizeShift;
		pArray2->u.array.bStripeWitch = (1 << pArray2->u.array.bArBlockSizeShift);
		pArray2->u.array.bArnMember = 2;
		pArray2->VDeviceCapacity = pArray1->VDeviceCapacity;
		pArray2->pfnSendCommand = pfnSendCommand[pArray2->VDeviceType];
		pArray2->pfnDeviceFailed = pfnDeviceFailed[pArray1->VDeviceType];
		memcpy(pArray2->u.array.ArrayName, pArray1->u.array.ArrayName, MAX_ARRAY_NAME);			
		pArray2->pParent = pArray1;
		pArray2->bSerialNumber = 1;
		pArray1->u.array.pMember[1] = pArray2;			
		pArray1->u.array.bArRealnMember++;						
	}
	
	for(i = 0; i < pArray2->u.array.bArnMember; i++)
		if((pArray2->u.array.pMember[i] == NULL) || !pArray2->u.array.pMember[i]->vf_online)
		{
			if(pArray2->u.array.pMember[i] != NULL)
				pArray2->u.array.pMember[i]->pParent = NULL;
			pArray2->u.array.pMember[i] = pDisk;
			goto find;
		}
	return -1;
	
find:
	UnregisterVDevice(pDisk);
	pDisk->VDeviceType = VD_SINGLE_DISK;
	pDisk->bSerialNumber = i;
	pDisk->pParent = pArray2;
	pDisk->vf_format_v2 = 1;	
	pDisk->u.disk.dDeHiddenLba = i? 10 : 0;
	pDisk->VDeviceCapacity = pDisk->u.disk.dDeRealCapacity;
	pDisk->pfnDeviceFailed = pfnDeviceFailed[pArray2->VDeviceType];

	pArray2->u.array.bArRealnMember++;
	if(pArray2->u.array.bArnMember == pArray2->u.array.bArRealnMember){				
		pArray2->vf_online = 1;
		pArray2->u.array.rf_broken = 0;
	}	
	
	if(pArray1->u.array.pMember[0]->vf_online && pArray1->u.array.pMember[1]->vf_online){
		pArray1->u.array.bArRealnMember = pArray1->u.array.bArnMember;
		pArray1->u.array.rf_broken = 0;
		pArray1->u.array.rf_need_rebuild = 1;		
		pArray1->u.array.rf_auto_rebuild = 1;
		
	}
	pArray1->u.array.RebuildSectors = 0;
	pArray1->u.array.dArStamp = GetStamp();
	SyncArrayInfo(pArray1);
	return 1;	
}
#endif

int hpt_add_disk_to_array(_VBUS_ARG DEVICEID idArray, DEVICEID idDisk)
{
	int	i;

	ULONG	Capacity;
	PVDevice pArray = ID_TO_VDEV(idArray);
	PVDevice pDisk	= ID_TO_VDEV(idDisk);

	if((idArray == HPT_NULL_ID) || (idDisk == HPT_NULL_ID))	return -1;
	if(check_VDevice_valid(pArray) || check_VDevice_valid(pDisk))	return -1;
	if(!pArray->u.array.rf_broken)	return -1;

	if(pArray->VDeviceType != VD_RAID_1 && pArray->VDeviceType != VD_RAID_5)
		return -1;
	if((pDisk->VDeviceType != VD_SINGLE_DISK) && (pDisk->VDeviceType != VD_SPARE))
		return -1;

#ifdef SUPPORT_OLD_ARRAY
	/* RAID 0 + 1 */
	if (pArray->vf_format_v2 && pArray->VDeviceType==VD_RAID_1 && 
		pArray->u.array.pMember[0] &&
		mIsArray(pArray->u.array.pMember[0]))
	{
		if(old_add_disk_to_raid01(_VBUS_P idArray, idDisk))
			return 0;
		else 
			return -1;
	}
#endif

	Capacity = pArray->VDeviceCapacity / (pArray->u.array.bArnMember - 1);

	if (pArray->vf_format_v2) {
		if(pDisk->u.disk.dDeRealCapacity < Capacity) return -1;
	}
	else
		if(pDisk->VDeviceCapacity < Capacity) return -1;
	
#if MAX_VBUS>1
	if (pArray->pVBus!=_vbus_p) { HPT_ASSERT(0); return -1;}
#endif

	for(i = 0; i < pArray->u.array.bArnMember; i++)
		if((pArray->u.array.pMember[i] == NULL) || !pArray->u.array.pMember[i]->vf_online)
		{
			if(pArray->u.array.pMember[i] != NULL)
				pArray->u.array.pMember[i]->pParent = NULL;
			pArray->u.array.pMember[i] = pDisk;
			goto find;
		}
	return -1;

find:
	UnregisterVDevice(pDisk);
	pDisk->VDeviceType = VD_SINGLE_DISK;
	pDisk->bSerialNumber = i;
	pDisk->pParent = pArray;
	if (pArray->VDeviceType==VD_RAID_5) pDisk->vf_cache_disk = 1;
	pDisk->pfnDeviceFailed = pfnDeviceFailed[pArray->VDeviceType];
	if (pArray->vf_format_v2) {
		pDisk->vf_format_v2 = 1;
		pDisk->VDeviceCapacity = pDisk->u.disk.dDeRealCapacity;
	}

	pArray->u.array.bArRealnMember++;
	if(pArray->u.array.bArnMember == pArray->u.array.bArRealnMember)
	{
		pArray->u.array.rf_need_rebuild = 1;
		pArray->u.array.RebuildSectors = 0;
		pArray->u.array.rf_auto_rebuild = 1;
		pArray->u.array.rf_broken = 0;
	}
	pArray->u.array.RebuildSectors = 0;

	/* sync the whole array */
	while (pArray->pParent) pArray = pArray->pParent;
	pArray->u.array.dArStamp = GetStamp();
	SyncArrayInfo(pArray);
	return 0;
}

static int hpt_add_spare_disk(_VBUS_ARG DEVICEID idDisk)
{
	PVDevice pVDevice = ID_TO_VDEV(idDisk);
	DECLARE_BUFFER(PUCHAR, pbuffer);

	if(idDisk == HPT_NULL_ID || check_VDevice_valid(pVDevice))
		return -1;
	if (pVDevice->VDeviceType != VD_SINGLE_DISK || pVDevice->pParent)
		return -1;

#if MAX_VBUS>1
	if (pVDevice->u.disk.pVBus!=_vbus_p) return -1;
#endif

	UnregisterVDevice(pVDevice);
	pVDevice->VDeviceType = VD_SPARE;
	pVDevice->vf_bootmark = 0;

	ZeroMemory((char *)pbuffer, 512);
	fDeReadWrite(&pVDevice->u.disk, 0, IDE_COMMAND_WRITE, pbuffer);
	SyncArrayInfo(pVDevice);
	return 0;
}

static int hpt_remove_spare_disk(_VBUS_ARG DEVICEID idDisk)
{
	PVDevice pVDevice = ID_TO_VDEV(idDisk);

	if(idDisk == 0 || check_VDevice_valid(pVDevice))	return -1;

#if MAX_VBUS>1
	if (pVDevice->u.disk.pVBus!=_vbus_p) return -1;
#endif

	pVDevice->VDeviceType = VD_SINGLE_DISK;

	SyncArrayInfo(pVDevice);
	RegisterVDevice(pVDevice);
	return 0;
}

static int hpt_set_array_info(_VBUS_ARG DEVICEID idArray, PALTERABLE_ARRAY_INFO pInfo)
{
	PVDevice pVDevice = ID_TO_VDEV(idArray);

	if(idArray == HPT_NULL_ID || check_VDevice_valid(pVDevice)) return -1;
	if (!mIsArray(pVDevice)) return -1;

	/* if the pVDevice isn't a top level, return -1; */
	if(pVDevice->pParent != NULL) return -1;

#if MAX_VBUS>1
	if (pVDevice->pVBus!=_vbus_p) { HPT_ASSERT(0); return -1;}
#endif

	if (pInfo->ValidFields & AAIF_NAME) {
		memset(pVDevice->u.array.ArrayName, 0, MAX_ARRAY_NAME);
		memcpy(pVDevice->u.array.ArrayName, pInfo->Name, sizeof(pInfo->Name));
		pVDevice->u.array.rf_need_sync = 1;
	}

	if (pInfo->ValidFields & AAIF_DESCRIPTION) {
		memcpy(pVDevice->u.array.Description, pInfo->Description, sizeof(pInfo->Description));
		pVDevice->u.array.rf_need_sync = 1;
	}

	if (pVDevice->u.array.rf_need_sync)
		SyncArrayInfo(pVDevice);
	return 0;
}

static int hpt_set_device_info(_VBUS_ARG DEVICEID idDisk, PALTERABLE_DEVICE_INFO pInfo)
{
	PVDevice pVDevice = ID_TO_VDEV(idDisk);

	/* stop buzzer. */
	if(idDisk == HPT_NULL_ID) {
#ifndef FOR_DEMO
		IAL_ADAPTER_T *pAdapter;
		for (pAdapter=gIal_Adapter; pAdapter; pAdapter=pAdapter->next) {
			if (pAdapter->beeping) {
				pAdapter->beeping = 0;
				BeepOff(pAdapter->mvSataAdapter.adapterIoBaseAddress);
			}
		}
#endif
		return 0;
	}
	
	if (check_VDevice_valid(pVDevice)) return -1;
	if (mIsArray(pVDevice))
		return -1;

#if MAX_VBUS>1
	if (pVDevice->u.disk.pVBus!=_vbus_p) return -1;
#endif

/*	if (pInfo->ValidFields & ADIF_MODE) {
		pVDevice->u.disk.bDeModeSetting = pInfo->DeviceModeSetting;
		pVDevice->u.disk.bDeUserSelectMode = pInfo->DeviceModeSetting;
		pVDevice->u.disk.df_user_mode_set = 1;
		fDeSelectMode((PDevice)&(pVDevice->u.disk), (UCHAR)pInfo->DeviceModeSetting);
		SyncArrayInfo(pVDevice);
	}*/
	return 0;
}
#endif /* SUPPORT_ARRAY */

#ifdef SUPPORT_HPT601
int hpt_get_601_info(DEVICEID idDisk, PHPT601_INFO pInfo)
{
	PVDevice pVDevice = ID_TO_VDEV(idDisk);
	PChannel          pChan = pVDevice->u.disk.pChannel;
	PIDE_REGISTERS_1  IoPort = pChan->BaseIoAddress1;

	if(!pVDevice->u.disk.df_with_601) return -1;

	mSelectUnit(IoPort, pVDevice->u.disk.bDeUnitId);
	pChan->pChipInstance->ftbl.pfnWaitOnBusy(pChan, pVDevice->u.disk.bDeUnitId, 1);
	
	BeginAccess601(IoPort);

	mSetBlockCount(IoPort, 0);
	pInfo->DeviceId = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 0x14);
	pInfo->Temperature = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 0xA);
	pInfo->FanStatus = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 7);
	pInfo->BeeperControl = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 3);
	pInfo->LED1Control = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 5);
	pInfo->LED2Control = InWord(&IoPort->Data);

	mSetBlockCount(IoPort, 0x18);
	pInfo->PowerStatus = InWord(&IoPort->Data);
		
	EndAccess601(IoPort);
	pInfo->ValidFields = 0x7F;
	/*DEVICEID|TEMPERATURE|FANSTATUS|BEEPERCONTROL|LED1CONTROL|LED2CONTROL|POWERSTATUS*/
	return 0;
}

int hpt_set_601_info(DEVICEID idDisk, PHPT601_INFO pInfo)
{
	PVDevice pVDevice = ID_TO_VDEV(idDisk);
	PChannel          pChan = pVDevice->u.disk.pChannel;
	PIDE_REGISTERS_1  IoPort = pChan->BaseIoAddress1;

	if(!pVDevice->u.disk.df_with_601) return -1;

	mSelectUnit(IoPort, pVDevice->u.disk.bDeUnitId);
	pChan->pChipInstance->ftbl.pfnWaitOnBusy(pChan, pVDevice->u.disk.bDeUnitId, 1);
	
	BeginAccess601(IoPort);

	if (pInfo->ValidFields & HPT601_INFO_TEMPERATURE) {
		mSetBlockCount(IoPort, 1);
		OutWord(&IoPort->Data, pInfo->Temperature);
	}

	if (pInfo->ValidFields & HPT601_INFO_FANSTATUS) {
		mSetBlockCount(IoPort, 0xA);
		OutWord(&IoPort->Data, pInfo->FanStatus);
	}

	if (pInfo->ValidFields & HPT601_INFO_BEEPERCONTROL) {
		mSetBlockCount(IoPort, 7);
		OutWord(&IoPort->Data, pInfo->BeeperControl);
	}

	if (pInfo->ValidFields & HPT601_INFO_LED1CONTROL) {
		mSetBlockCount(IoPort, 3);
		OutWord(&IoPort->Data, pInfo->LED1Control);
	}

	if (pInfo->ValidFields & HPT601_INFO_LED2CONTROL) {
		mSetBlockCount(IoPort, 5);
		OutWord(&IoPort->Data, pInfo->LED2Control);
	}
		
	EndAccess601(IoPort);

	return 0;
}
#endif

/* hpt_default_ioctl()
 *  This is a default implementation. The platform dependent part
 *  may reuse this function and/or use it own implementation for
 *  each ioctl function.
 */
int hpt_default_ioctl(_VBUS_ARG
							DWORD dwIoControlCode,       	/* operation control code */
							PVOID lpInBuffer,            	/* input data buffer */
							DWORD nInBufferSize,         	/* size of input data buffer */
							PVOID lpOutBuffer,           	/* output data buffer */
							DWORD nOutBufferSize,        	/* size of output data buffer */
							PDWORD lpBytesReturned      	/* byte count */
					)
{
	switch(dwIoControlCode)	{

	case HPT_IOCTL_GET_VERSION:

		if (nInBufferSize != 0) return -1;
		if (nOutBufferSize != sizeof(DWORD)) return -1;
		*((DWORD*)lpOutBuffer) = HPT_INTERFACE_VERSION;
		break;

	case HPT_IOCTL_GET_CONTROLLER_COUNT:

		if (nOutBufferSize!=sizeof(DWORD)) return -1;
		*(PDWORD)lpOutBuffer = hpt_get_controller_count();
		break;

	case HPT_IOCTL_GET_CONTROLLER_INFO:
		{
			int id;
			PCONTROLLER_INFO pInfo;
	
			if (nInBufferSize!=sizeof(DWORD)) return -1;
			if (nOutBufferSize!=sizeof(CONTROLLER_INFO)) return -1;
	
			id = *(DWORD *)lpInBuffer;
			pInfo = (PCONTROLLER_INFO)lpOutBuffer;
			if (hpt_get_controller_info(id, pInfo)!=0)
				return -1;
		}
		break;

	case HPT_IOCTL_GET_CHANNEL_INFO:
		{
			int id, bus;
			PCHANNEL_INFO pInfo;

			if (nInBufferSize!=8) return -1;
			if (nOutBufferSize!=sizeof(CHANNEL_INFO)) return -1;

			id = *(DWORD *)lpInBuffer;
			bus = ((DWORD *)lpInBuffer)[1];
			pInfo = (PCHANNEL_INFO)lpOutBuffer;

			if (hpt_get_channel_info(id, bus, pInfo)!=0)
				return -1;
		}
		break;

	case HPT_IOCTL_GET_LOGICAL_DEVICES:
		{
			DWORD nMax;
			DEVICEID *pIds;

			if (nInBufferSize!=sizeof(DWORD)) return -1;
			nMax = *(DWORD *)lpInBuffer;
			if (nOutBufferSize < sizeof(DWORD)+sizeof(DWORD)*nMax) return -1;

			pIds = ((DEVICEID *)lpOutBuffer)+1;
			*(DWORD*)lpOutBuffer = hpt_get_logical_devices(pIds, nMax);
		}
		break;

	case HPT_IOCTL_GET_DEVICE_INFO:
		{
			DEVICEID id;
			PLOGICAL_DEVICE_INFO pInfo;

			if (nInBufferSize!=sizeof(DEVICEID)) return -1;
			if (nOutBufferSize!=sizeof(LOGICAL_DEVICE_INFO)) return -1;

			id = *(DWORD *)lpInBuffer;
			if (id == INVALID_DEVICEID)	return -1;

			pInfo = (PLOGICAL_DEVICE_INFO)lpOutBuffer;
			memset(pInfo, 0, sizeof(LOGICAL_DEVICE_INFO));

			if (hpt_get_device_info(id, pInfo)!=0)
				return -1;
		}
		break;

#ifdef SUPPORT_ARRAY
	case HPT_IOCTL_CREATE_ARRAY:
		{
			CREATE_ARRAY_PARAMS *pParam;
			DEVICEID id;

			if (nInBufferSize!=sizeof(CREATE_ARRAY_PARAMS)) return -1;
			if (nOutBufferSize!=sizeof(DEVICEID)) return -1;

			pParam = (PCREATE_ARRAY_PARAMS)lpInBuffer;

			id = hpt_create_array(_VBUS_P pParam);
			*(DEVICEID *)lpOutBuffer = id;

			if(id == (DEVICEID)INVALID_DEVICEID)
				return -1;
		}
		break;

	case HPT_IOCTL_SET_ARRAY_INFO:
		{
			DEVICEID idArray;
			PALTERABLE_ARRAY_INFO pInfo;

			if (nInBufferSize!=sizeof(HPT_SET_ARRAY_INFO)) return -1;
			if (nOutBufferSize!=0) return -1;

			idArray = ((PHPT_SET_ARRAY_INFO)lpInBuffer)->idArray;
			pInfo = &((PHPT_SET_ARRAY_INFO)lpInBuffer)->Info;

			if(hpt_set_array_info(_VBUS_P idArray,  pInfo))
				return -1;
		}
		break;

	case HPT_IOCTL_SET_DEVICE_INFO:
		{
			DEVICEID idDisk;
			PALTERABLE_DEVICE_INFO pInfo;

			if (nInBufferSize!=sizeof(HPT_SET_DEVICE_INFO)) return -1;
			if (nOutBufferSize!=0) return -1;

			idDisk = ((PHPT_SET_DEVICE_INFO)lpInBuffer)->idDisk;
			pInfo = &((PHPT_SET_DEVICE_INFO)lpInBuffer)->Info;
			if(hpt_set_device_info(_VBUS_P idDisk, pInfo) != 0)
				return -1;
		}
		break;

	case HPT_IOCTL_SET_BOOT_MARK:
		{
			DEVICEID id;
			PVDevice pTop;
			int i;
			IAL_ADAPTER_T *pAdapter = gIal_Adapter;
			PVBus pVBus;

			if (nInBufferSize!=sizeof(DEVICEID)) return -1;
			id = *(DEVICEID *)lpInBuffer;
			while(pAdapter != 0)
			{
				pVBus = &pAdapter->VBus;
				for(i = 0; i < MAX_VDEVICE_PER_VBUS; i++)
				{
					if(!(pTop = pVBus->pVDevice[i])) continue;
#if MAX_VBUS>1
					if (pTop->pVBus!=_vbus_p) return -1;
#endif
					while (pTop->pParent) pTop = pTop->pParent;
					if (id==0 && pTop->vf_bootmark)
						pTop->vf_bootmark = 0;
					else if (pTop==ID_TO_VDEV(id) && !pTop->vf_bootmark)
						pTop->vf_bootmark = 1;
					else
						continue;
					SyncArrayInfo(pTop);
					break;
				}
				pAdapter = pAdapter->next;
			}
		}
		break;
#endif /* SUPPORT_ARRAY */
	case HPT_IOCTL_RESCAN_DEVICES:
		{
			fRescanAllDevice(_VBUS_P0);
			if (nInBufferSize!=0) return -1;
			if (nOutBufferSize!=0) return -1;
			fRescanAllDevice(_VBUS_P0);
		}
		break;

#ifdef SUPPORT_ARRAY
	case HPT_IOCTL_ADD_SPARE_DISK:
		{
			DEVICEID id;

			if (nInBufferSize!=sizeof(DEVICEID)) return -1;
			if (nOutBufferSize!=0) return -1;

			id = *(DEVICEID *)lpInBuffer;

			if(hpt_add_spare_disk(_VBUS_P id))
				return -1;
		}
		break;

	case HPT_IOCTL_REMOVE_SPARE_DISK:
		{
			DEVICEID id;

			if (nInBufferSize!=sizeof(DEVICEID)) return -1;
			if (nOutBufferSize!=0) return -1;

			id = *(DEVICEID *)lpInBuffer;

			if(hpt_remove_spare_disk(_VBUS_P id))
				return -1;
		}
		break;

	case HPT_IOCTL_ADD_DISK_TO_ARRAY:
		{
			DEVICEID id1,id2;
			id1 = ((PHPT_ADD_DISK_TO_ARRAY)lpInBuffer)->idArray;
			id2 = ((PHPT_ADD_DISK_TO_ARRAY)lpInBuffer)->idDisk;

			if (nInBufferSize != sizeof(HPT_ADD_DISK_TO_ARRAY)) return -1;
			if (nOutBufferSize != 0) return -1;

			if(hpt_add_disk_to_array(_VBUS_P id1, id2))
				return -1;
		}
		break;
#endif
	case HPT_IOCTL_GET_DRIVER_CAPABILITIES:
		{
			PDRIVER_CAPABILITIES cap;
			if (nOutBufferSize<sizeof(DRIVER_CAPABILITIES)) return -1;
			cap = (PDRIVER_CAPABILITIES)lpOutBuffer;

			if(hpt_get_driver_capabilities(cap))
				return -1;
		}
		break;

#ifdef SUPPORT_HPT601	
	case HPT_IOCTL_GET_601_INFO:
		{
			DEVICEID id;
			PHPT601_INFO pInfo;

			if (nInBufferSize!=sizeof(DEVICEID)) return -1;
			if (nOutBufferSize!=sizeof(HPT601_INFO)) return -1;

			id = *(DWORD *)lpInBuffer;
			if (id == INVALID_DEVICEID)	return -1;

			pInfo = (PHPT601_INFO)lpOutBuffer;
			memset(pInfo, 0, sizeof(HPT601_INFO));

			if (hpt_get_601_info(id, pInfo)!=0)
				return -1;
		}
		break;

	case HPT_IOCTL_SET_601_INFO:
		{
			DEVICEID id;
			PHPT601_INFO pInfo;

			if (nInBufferSize!=sizeof(HPT_SET_601_INFO)) return -1;
			if (nOutBufferSize!=0) return -1;

			id = ((PHPT_SET_601_INFO)lpInBuffer)->idDisk;
			pInfo = &((PHPT_SET_601_INFO)lpInBuffer)->Info;
			if(hpt_set_601_info(id, pInfo) != 0)
				return -1;
		}
		break;
#endif
	default:
		return -1;
	}

	if (lpBytesReturned)
		*lpBytesReturned = nOutBufferSize;
	return 0;
}
