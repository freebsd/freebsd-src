/************************************************************************/
/* File iSeries_vpdInfo.c created by Allan Trautman on Fri Feb  2 2001. */
/************************************************************************/
/* This code gets the card location of the hardware                     */
/* Copyright (C) 20yy  <Allan H Trautman> <IBM Corp>                    */
/*                                                                      */
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */ 
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */ 
/* along with this program; if not, write to the:                       */
/* Free Software Foundation, Inc.,                                      */ 
/* 59 Temple Place, Suite 330,                                          */ 
/* Boston, MA  02111-1307  USA                                          */
/************************************************************************/
/* Change Activity:                                                     */
/*   Created, Feb 2, 2001                                               */
/*   Ported to ppc64, August 20, 2001                                   */
/* End Change Activity                                                  */
/************************************************************************/
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/types.h>
#include <asm/resource.h>

#include <asm/iSeries/HvCallPci.h>
#include <asm/iSeries/HvTypes.h>
#include <asm/iSeries/mf.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCallPci.h>
//#include <asm/iSeries/iSeries_VpdInfo.h>
#include <asm/iSeries/iSeries_pci.h>
#include "pci.h"

/************************************************/
/* Size of Bus VPD data                         */
/************************************************/
#define BUS_VPDSIZE      1024
/************************************************/
/* Bus Vpd Tags                                 */
/************************************************/
#define  VpdEndOfDataTag   0x78
#define  VpdEndOfAreaTag   0x79
#define  VpdIdStringTag    0x82
#define  VpdVendorAreaTag  0x84
/************************************************/
/* Mfg Area Tags                                */
/************************************************/
#define  VpdFruFlag       0x4647     // "FG"
#define  VpdFruFrameId    0x4649     // "FI"
#define  VpdSlotMapFormat 0x4D46     // "MF"
#define  VpdAsmPartNumber 0x504E     // "PN"
#define  VpdFruSerial     0x534E     // "SN"
#define  VpdSlotMap       0x534D     // "SM"

/************************************************/
/* Structures of the areas                      */
/************************************************/
struct MfgVpdAreaStruct {
	u16 Tag;
	u8  TagLength;
	u8  AreaData1;
	u8  AreaData2;
};
typedef struct MfgVpdAreaStruct MfgArea;
#define MFG_ENTRY_SIZE   3

struct SlotMapStruct {
	u8   AgentId;
	u8   SecondaryAgentId;
	u8   PhbId;
	char CardLocation[3];
	char Parms[8];
	char Reserved[2];
}; 
typedef struct SlotMapStruct SlotMap;
#define SLOT_ENTRY_SIZE   16

/****************************************************************
 *                                                              *
 * Bus, Card, Board, FrameId, CardLocation.                     *
 ****************************************************************/
LocationData* iSeries_GetLocationData(struct pci_dev* PciDev)
{
	struct iSeries_Device_Node* DevNode = (struct iSeries_Device_Node*)PciDev->sysdata;
	LocationData* LocationPtr = (LocationData*)kmalloc(LOCATION_DATA_SIZE, GFP_KERNEL);
	if (LocationPtr == NULL) {
		printk("PCI: LocationData area allocation failed!\n");
		return NULL;
	}
	memset(LocationPtr,0,LOCATION_DATA_SIZE);
	LocationPtr->Bus              = ISERIES_BUS(DevNode);
	LocationPtr->Board            = DevNode->Board;
	LocationPtr->FrameId          = DevNode->FrameId;
	LocationPtr->Card             = PCI_SLOT(DevNode->DevFn);
	strcpy(&LocationPtr->CardLocation[0],&DevNode->CardLocation[0]);
	return LocationPtr;
}

/************************************************************************/
/* Formats the device information.                                      */
/* - Pass in pci_dev* pointer to the device.                            */
/* - Pass in buffer to place the data.  Danger here is the buffer must  */
/*   be as big as the client says it is.   Should be at least 128 bytes.*/
/* Return will the length of the string data put in the buffer.         */
/* Format:                                                              */
/* PCI: Bus  0, Device 26, Vendor 0x12AE  Frame  1, Card  C10  Ethernet */
/* controller                                                           */
/************************************************************************/
int  iSeries_Device_Information(struct pci_dev* PciDev,char* Buffer, int BufferSize)
{
	struct iSeries_Device_Node* DevNode = (struct iSeries_Device_Node*)PciDev->sysdata;
	char*          BufPtr  = Buffer;
	int            LineLen = 0;

	if (DevNode == NULL) {
		LineLen = sprintf(BufPtr+LineLen, "PCI: iSeries_Device_Information DevNode is NULL");
		return LineLen;
	}

	if (BufferSize >= 128) {
		LineLen =  sprintf(BufPtr+LineLen,"PCI: Bus%3d, Device%3d, Vendor %04X ",
				   ISERIES_BUS(DevNode), PCI_SLOT(PciDev->devfn),PciDev->vendor);

		LineLen += sprintf(BufPtr+LineLen,"Frame%3d, Card %4s  ", DevNode->FrameId,DevNode->CardLocation);

		if (pci_class_name(PciDev->class >> 8) == 0) {
			LineLen += sprintf(BufPtr+LineLen,"0x%04X  ",(int)(PciDev->class >> 8));
		}
		else {
			LineLen += sprintf(BufPtr+LineLen,"%s",pci_class_name(PciDev->class >> 8) );
		}
	}
	return LineLen;
}
/************************************************************************/
/* Build a character string of the device location, Frame  1, Card  C10 */
/************************************************************************/
int   device_Location(struct pci_dev* PciDev,char* BufPtr)
{
	struct iSeries_Device_Node* DevNode = (struct iSeries_Device_Node*)PciDev->sysdata;
	return sprintf(BufPtr,"PCI: Bus%3d, AgentId%3d, Vendor %04X, Location %s",
		       DevNode->DsaAddr.busNumber,
		       DevNode->AgentId,
		       DevNode->Vendor,
		       DevNode->Location);
}

/*****************************************************************/
/* Parse the Slot Area                                           */
/*****************************************************************/
void  iSeries_Parse_SlotArea(SlotMap* MapPtr,int MapLen, struct iSeries_Device_Node* DevNode)
{
	int      SlotMapLen = MapLen;
	SlotMap* SlotMapPtr = MapPtr;
	/*************************************************************/
	/* Parse Slot label until we find the one requrested         */
	/*************************************************************/
	while (SlotMapLen > 0) {
		if ((SlotMapPtr->AgentId == DevNode->AgentId) && 
		    (SlotMapPtr->SecondaryAgentId == 0x10   ) ) {
			/*******************************************************/
			/* If Phb wasn't found, grab the entry first one found.*/ 
			/*******************************************************/
			if (DevNode->PhbId == 0xff) {
				DevNode->PhbId = SlotMapPtr->PhbId; 
			}
			/**************************************************/
			/* Found it, extract the data.                    */
			/**************************************************/
			if (SlotMapPtr->PhbId == DevNode->PhbId ) {
	        		memcpy(&DevNode->CardLocation,&SlotMapPtr->CardLocation,3);
				DevNode->CardLocation[3]  = 0;
				break;
			}
		}
		/*********************************************************/
		/* Point to the next Slot                                */
		/*********************************************************/
		SlotMapPtr = (SlotMap*)((char*)SlotMapPtr+SLOT_ENTRY_SIZE);
		SlotMapLen -= SLOT_ENTRY_SIZE;
	}
}

/*****************************************************************/
/* Parse the Mfg Area                                            */
/*****************************************************************/
static void  iSeries_Parse_MfgArea(u8* AreaData,int AreaLen, struct iSeries_Device_Node* DevNode)
{
	MfgArea* MfgAreaPtr = (MfgArea*)AreaData;
	int      MfgAreaLen = AreaLen;
	u16      SlotMapFmt = 0;

	/*************************************************************/
	/* Parse Mfg Data                                            */
	/*************************************************************/
	while (MfgAreaLen > 0) {
		int MfgTagLen  = MfgAreaPtr->TagLength;
		/*******************************************************/
		/* Frame ID         (FI 4649020310 )                   */
		/*******************************************************/
		if (MfgAreaPtr->Tag == VpdFruFrameId) {     /* FI  */
			DevNode->FrameId = MfgAreaPtr->AreaData1;
		}
		/*******************************************************/
		/* Slot Map Format  (MF 4D46020004 )                   */
		/*******************************************************/
		else if (MfgAreaPtr->Tag == VpdSlotMapFormat){   /* MF  */
			SlotMapFmt = (MfgAreaPtr->AreaData1*256)+(MfgAreaPtr->AreaData2);
		}
		/*******************************************************/
		/* Slot Map         (SM 534D90                         */
		/*******************************************************/
		else if (MfgAreaPtr->Tag == VpdSlotMap){         /* SM  */
			SlotMap*  SlotMapPtr;
			if (SlotMapFmt == 0x1004) SlotMapPtr = (SlotMap*)((char*)MfgAreaPtr+MFG_ENTRY_SIZE+1);
 	    		else                     SlotMapPtr = (SlotMap*)((char*)MfgAreaPtr+MFG_ENTRY_SIZE);
	    		iSeries_Parse_SlotArea(SlotMapPtr,MfgTagLen, DevNode);
		}
		/*********************************************************/
		/* Point to the next Mfg Area                            */
		/* Use defined size, sizeof give wrong answer            */
		/*********************************************************/
		MfgAreaPtr = (MfgArea*)((char*)MfgAreaPtr + MfgTagLen + MFG_ENTRY_SIZE);
		MfgAreaLen -= (MfgTagLen + MFG_ENTRY_SIZE); 
	}	
}

/*****************************************************************/
/* Look for "BUS".. Data is not Null terminated.                 */
/* PHBID of 0xFF indicates PHB was not found in VPD Data.        */	
/*****************************************************************/
static int iSeries_Parse_PhbId(u8* AreaPtr,int AreaLength)
{
	u8*  PhbPtr  = AreaPtr;
	int  DataLen = AreaLength;
	char PhbId   = 0xFF;                   
	while (DataLen > 0) {
		if (*PhbPtr == 'B' && *(PhbPtr+1) == 'U' && *(PhbPtr+2) == 'S') {
			PhbPtr += 3;
			while(*PhbPtr == ' ') ++PhbPtr;
			PhbId = (*PhbPtr & 0x0F);
			break;
        	}
		++PhbPtr;
		--DataLen;
	}
	return PhbId;
}

/****************************************************************/
/* Parse out the VPD Areas                                      */
/****************************************************************/
static void  iSeries_Parse_Vpd(u8* VpdData, int VpdDataLen, struct iSeries_Device_Node* DevNode)
{
	u8*  TagPtr  = VpdData;
	int  DataLen = VpdDataLen-3;
	/*************************************************************/
	/* Parse the Areas                                           */
	/*************************************************************/
	while (*TagPtr != VpdEndOfAreaTag && DataLen > 0) {
		int  AreaLen   = *(TagPtr+1) + (*(TagPtr+2)*256);	
		u8*  AreaData  = TagPtr+3;

		if (*TagPtr == VpdIdStringTag) {
			DevNode->PhbId = iSeries_Parse_PhbId(AreaData,AreaLen);
		}
		else if (*TagPtr == VpdVendorAreaTag) {
	    		iSeries_Parse_MfgArea(AreaData,AreaLen,DevNode);
		}
		/*********************************************************
		 * Point to next Area.
		 *********************************************************/
		TagPtr  = AreaData + AreaLen;
		DataLen -= AreaLen;
	}
}    

/****************************************************************
 *  iSeries_Get_Location_Code(struct iSeries_Device_Node*)      *
 *
 ****************************************************************/
void  iSeries_Get_Location_Code(struct iSeries_Device_Node* DevNode)
{
	int  BusVpdLen = 0;
	u8*  BusVpdPtr = (u8*)kmalloc(BUS_VPDSIZE, GFP_KERNEL);
	if (BusVpdPtr == NULL) {
		printk("PCI: Bus VPD Buffer allocation failure.\n");
		return;
	}
	BusVpdLen  = HvCallPci_getBusVpd(ISERIES_BUS(DevNode),REALADDR(BusVpdPtr),BUS_VPDSIZE);
	if (BusVpdLen == 0) {
		kfree(BusVpdPtr);
		printk("PCI: Bus VPD Buffer zero length.\n");
		return;
	}
	//printk("PCI: DevNode:0x%p BusVpdPtr:0x%p Length:%d\n",DevNode, BusVpdPtr, BusVpdLen);
	/*************************************************************/
	/* Make sure this is what I think it is                      */
	/*************************************************************/
	if (*BusVpdPtr != VpdIdStringTag) {               /*0x82     */
		printk("PCI: Bus VPD Buffer missing starting tag.\n");
		kfree(BusVpdPtr);
		return;
	}
	/***************************************************************/
	/***************************************************************/
	iSeries_Parse_Vpd(BusVpdPtr,BusVpdLen, DevNode);
	sprintf(DevNode->Location,"Frame%3d, Card %-4s",DevNode->FrameId,DevNode->CardLocation);
	kfree(BusVpdPtr);
}
