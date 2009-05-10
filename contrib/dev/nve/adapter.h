/***************************************************************************\
|*                                                                           *|
|*       Copyright 2001-2004 NVIDIA Corporation.  All Rights Reserved.       *|
|*                                                                           *|
|*     THE INFORMATION CONTAINED HEREIN  IS PROPRIETARY AND CONFIDENTIAL     *|
|*     TO NVIDIA, CORPORATION.   USE,  REPRODUCTION OR DISCLOSURE TO ANY     *|
|*     THIRD PARTY IS SUBJECT TO WRITTEN PRE-APPROVAL BY NVIDIA, CORP.       *|
|*                                                                           *|
|*     THE INFORMATION CONTAINED HEREIN IS PROVIDED  "AS IS" WITHOUT         *|
|*     EXPRESS OR IMPLIED WARRANTY OF ANY KIND, INCLUDING ALL IMPLIED        *|
|*     WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A     *|
|*     PARTICULAR PURPOSE.                                                   *|
|*                                                                           *|
\***************************************************************************/ 

/*
    FILE:   adapter.h
    DATE:   2/7/00

    This file contains the hardware interface to the ethernet adapter.
*/

#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define HDA_VERSION_STRING "HDR A: $Revision: #46 $"

#ifdef MODS_NETWORK_BUILD
#ifndef _DRVAPP_H_
#include "drvapp.h"
#endif
#endif

//////////////////////////////////////////////////////////////////
// For the set and get configuration calls.
typedef struct  _ADAPTER_CONFIG
{
    NV_UINT32   ulFlags;
}   ADAPTER_CONFIG, *PADAPTER_CONFIG;
//////////////////////////////////////////////////////////////////

typedef struct _ADAPTER_WRITE_OFFLOAD
{
    NV_UINT32   usBitmask;
    NV_UINT32   ulMss;

} ADAPTER_WRITE_OFFLOAD;

//////////////////////////////////////////////////////////////////
// For the ADAPTER_Write1 call.
/* This scatter gather list should be same as defined in ndis.h by MS.
   For ULONG_PTR MS header file says that it will be of same size as
   pointer. It has been defined to take care of casting between differenet
   sizes.
*/
typedef struct _NVSCATTER_GATHER_ELEMENT {
    NV_UINT32 PhysLow;
    NV_UINT32 PhysHigh;
    NV_UINT32 Length;
    NV_VOID *Reserved;
} NVSCATTER_GATHER_ELEMENT, *PNVSCATTER_GATHER_ELEMENT;

#ifndef linux
#pragma warning(disable:4200)
#endif
typedef struct _NVSCATTER_GATHER_LIST {
    NV_UINT32       NumberOfElements;
    NV_VOID         *Reserved;
    NVSCATTER_GATHER_ELEMENT Elements[0];   // Made 0 sized element to remove MODS compilation error
                                            // Elements[0] and Elements[] have the same effect. 
                                            // sizeof(NVSCATTER_GATHER_LIST) is the same (value of 8) in both cases
                                            // And both lead to Warning 4200 in MSVC
} NVSCATTER_GATHER_LIST, *PNVSCATTER_GATHER_LIST;
#ifndef linux
#pragma warning(default:4200)
#endif

typedef struct  _ADAPTER_WRITE_DATA1
{
    NV_UINT32                   ulTotalLength;
    PNV_VOID                    pvID;
    NV_UINT8                    uc8021pPriority;
    ADAPTER_WRITE_OFFLOAD       *psOffload;
    PNVSCATTER_GATHER_LIST      pNVSGL;
}   ADAPTER_WRITE_DATA1, *PADAPTER_WRITE_DATA1;


//////////////////////////////////////////////////////////////////
// For the ADAPTER_Write call.
typedef struct  _ADAPTER_WRITE_ELEMENT
{
    PNV_VOID   pPhysical;
    NV_UINT32   ulLength;
}   ADAPTER_WRITE_ELEMENT, *PADAPTER_WRITE_ELEMENT;


#define ADAPTER_WRITE_OFFLOAD_BP_SEGOFFLOAD      0
#define ADAPTER_WRITE_OFFLOAD_BP_IPV4CHECKSUM    1
#define ADAPTER_WRITE_OFFLOAD_BP_IPV6CHECKSUM    2
#define ADAPTER_WRITE_OFFLOAD_BP_TCPCHECKSUM     3
#define ADAPTER_WRITE_OFFLOAD_BP_UDPCHECKSUM     4
#define ADAPTER_WRITE_OFFLOAD_BP_IPCHECKSUM      5


// pvID is a value that will be passed back into OSAPI.pfnPacketWasSent
// when the transmission completes. if pvID is NULL, the ADAPTER code
// assumes the caller does not want the pfnPacketWasSent callback.
typedef struct  _ADAPTER_WRITE_DATA
{
    NV_UINT32                   ulNumberOfElements;
    NV_UINT32                   ulTotalLength;
    PNV_VOID                    pvID;
    NV_UINT8                    uc8021pPriority;
    ADAPTER_WRITE_OFFLOAD       *psOffload;
#ifdef linux
    ADAPTER_WRITE_ELEMENT       sElement[32];
#else
    ADAPTER_WRITE_ELEMENT       sElement[100];
#endif
}   ADAPTER_WRITE_DATA, *PADAPTER_WRITE_DATA;
//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// For the ADAPTER_Read call.
typedef struct  _ADAPTER_READ_ELEMENT
{
    PNV_VOID   pPhysical;
    NV_UINT32   ulLength;
}   ADAPTER_READ_ELEMENT, *PADAPTER_READ_ELEMENT;

typedef struct _ADAPTER_READ_OFFLOAD
{
    NV_UINT8  ucChecksumStatus;

} ADAPTER_READ_OFFLOAD;

typedef struct _ADAPTER_READ_DATA
{
    NV_UINT32                   ulNumberOfElements;
    NV_UINT32                   ulTotalLength;
    PNV_VOID                    pvID;
    NV_UINT32                   ulFilterMatch;
    ADAPTER_READ_OFFLOAD        sOffload;
    ADAPTER_READ_ELEMENT        sElement[10];
}   ADAPTER_READ_DATA, *PADAPTER_READ_DATA;


#define RDFLAG_CHK_NOCHECKSUM      0
#define RDFLAG_CHK_IPPASSTCPFAIL   1
#define RDFLAG_CHK_IPPASSUDPFAIL   2
#define RDFLAG_CHK_IPFAIL          3
#define RDFLAG_CHK_IPPASSNOTCPUDP  4
#define RDFLAG_CHK_IPPASSTCPPASS   5
#define RDFLAG_CHK_IPPASSUDPPASS   6
#define RDFLAG_CHK_RESERVED        7


// The ulFilterMatch flag can be a logical OR of the following
#define ADREADFL_UNICAST_MATCH          0x00000001
#define ADREADFL_MULTICAST_MATCH        0x00000002
#define ADREADFL_BROADCAST_MATCH        0x00000004
//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// For the ADAPTER_GetPowerCapabilities call.
typedef struct  _ADAPTER_POWERCAPS
{
    NV_UINT32   ulPowerFlags;
    NV_UINT32   ulMagicPacketWakeUpFlags;
    NV_UINT32   ulPatternWakeUpFlags;
    NV_UINT32   ulLinkChangeWakeUpFlags;
    NV_SINT32     iMaxWakeUpPatterns;
}   ADAPTER_POWERCAPS, *PADAPTER_POWERCAPS;

// For the ADAPTER_GetPowerState and ADAPTER_SetPowerState call.
typedef struct  _ADAPTER_POWERSTATE
{
    NV_UINT32   ulPowerFlags;
    NV_UINT32   ulMagicPacketWakeUpFlags;
    NV_UINT32   ulPatternWakeUpFlags;
    NV_UINT32   ulLinkChangeWakeUpFlags;
}   ADAPTER_POWERSTATE, *PADAPTER_POWERSTATE;

// Each of the flag fields in the POWERCAPS structure above can have
// any of the following bitflags set giving the capabilites of the
// adapter. In the case of the wake up fields, these flags mean that
// wake up can happen from the specified power state.

// For the POWERSTATE structure, the ulPowerFlags field should just
// have one of these bits set to go to that particular power state.
// The WakeUp fields can have one or more of these bits set to indicate
// what states should be woken up from.
#define POWER_STATE_D0          0x00000001
#define POWER_STATE_D1          0x00000002
#define POWER_STATE_D2          0x00000004
#define POWER_STATE_D3          0x00000008

#define POWER_STATE_ALL         (POWER_STATE_D0 | \
                                POWER_STATE_D1  | \
                                POWER_STATE_D2  | \
                                POWER_STATE_D3)
//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// The ADAPTER_GetPacketFilterCaps call returns a NV_UINT32 that can
// have the following capability bits set.
#define ACCEPT_UNICAST_PACKETS      0x00000001
#define ACCEPT_MULTICAST_PACKETS    0x00000002
#define ACCEPT_BROADCAST_PACKETS    0x00000004
#define ACCEPT_ALL_PACKETS          0x00000008

#define ETH_LENGTH_OF_ADDRESS        6

// The ADAPTER_SetPacketFilter call uses this structure to know what
// packet filter to set. The ulPacketFilter field can contain some
// union of the bit flags above. The acMulticastMask array holds a
// 48 bit MAC address mask with a 0 in every bit position that should
// be ignored on compare and a 1 in every bit position that should
// be taken into account when comparing to see if the destination
// address of a packet should be accepted for multicast.
typedef struct  _PACKET_FILTER
{
    NV_UINT32   ulFilterFlags;
    NV_UINT8   acMulticastAddress[ETH_LENGTH_OF_ADDRESS];
    NV_UINT8   acMulticastMask[ETH_LENGTH_OF_ADDRESS];
}   PACKET_FILTER, *PPACKET_FILTER;
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
// A WAKE_UP_PATTERN is a 128-byte pattern that the adapter can
// look for in incoming packets to decide when to wake up.  Higher-
// level protocols can use this to, for example, wake up the
// adapter whenever it sees an IP packet that is addressed to it.
// A pattern consists of 128 bits of byte masks that indicate
// which bytes in the packet are relevant to the pattern, plus
// values for each byte.
#define WAKE_UP_PATTERN_SIZE 128

typedef struct _WAKE_UP_PATTERN
{
    NV_UINT32   aulByteMask[WAKE_UP_PATTERN_SIZE/32];
    NV_UINT8   acData[WAKE_UP_PATTERN_SIZE];
}   WAKE_UP_PATTERN, *PWAKE_UP_PATTERN;



//
//
// Adapter offload
//
typedef struct _ADAPTER_OFFLOAD {

    NV_UINT32 Type;
    NV_UINT32 Value0;

} ADAPTER_OFFLOAD, *PADAPTER_OFFLOAD;

#define ADAPTER_OFFLOAD_VLAN        0x00000001
#define ADAPTER_OFFLOAD_IEEE802_1P    0x00000002
#define ADAPTER_OFFLOAD_IEEE802_1PQ_PAD    0x00000004

//////////////////////////////////////////////////////////////////

//  CMNDATA_OS_ADAPTER
//  Structure common to OS and Adapter layers
//  Used for moving data from the OS layer to the adapter layer through SetCommonData 
//  function call from OS layer to Adapter layer
// 

typedef struct  _CMNDATA_OS_ADAPTER
{
#ifndef linux
    ASF_SEC0_BASE   sRegSec0Base;
#endif
    NV_UINT32           bFPGA; 
    NV_UINT32           ulFPGAEepromSize;
    NV_UINT32           bChecksumOffloadEnable;
    NV_UINT32           ulChecksumOffloadBM;
    NV_UINT32           ulChecksumOffloadOS;
    NV_UINT32           ulMediaIF;
    NV_UINT32           bOemCustomEventRead;

    // Debug only right now
    //!!! Beware mods is relying on the fields blow.
    NV_UINT32           ulWatermarkTFBW;
    NV_UINT32           ulBackoffRseed;
    NV_UINT32           ulBackoffSlotTime;
    NV_UINT32           ulModeRegTxReadCompleteEnable;
    NV_UINT32           ulFatalErrorRegister;

} CMNDATA_OS_ADAPTER;


//////////////////////////////////////////////////////////////////
// The functional typedefs for the ADAPTER Api
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_CLOSE)  (PNV_VOID pvContext, NV_UINT8 ucIsPowerDown);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_INIT)  (PNV_VOID pvContext, NV_UINT16 usForcedSpeed, NV_UINT8 ucForceDpx, NV_UINT8 ucForceMode, NV_UINT8 ucAsyncMode, NV_UINT32 *puiLinkState);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_DEINIT)  (PNV_VOID pvContext, NV_UINT8 ucIsPowerDown);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_START)  (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_STOP)   (PNV_VOID pvContext, NV_UINT8 ucIsPowerDown);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_QUERY_WRITE_SLOTS) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_WRITE) (PNV_VOID pvContext, ADAPTER_WRITE_DATA *pADWriteData);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_WRITE1) (PNV_VOID pvContext, ADAPTER_WRITE_DATA1 *pADWriteData1);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_QUERY_INTERRUPT) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_HANDLE_INTERRUPT) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_DISABLE_INTERRUPTS) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_ENABLE_INTERRUPTS) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_CLEAR_INTERRUPTS) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_CLEAR_TX_DESC) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_LINK_SPEED) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_LINK_MODE) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_LINK_STATE) (PNV_VOID pvContext, NV_UINT32 *pulLinkState);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_IS_LINK_INITIALIZING) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_RESET_PHY_INIT_STATE) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_TRANSMIT_QUEUE_SIZE) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_RECEIVE_QUEUE_SIZE) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_STATISTICS) (PNV_VOID pvContext, PADAPTER_STATS pADStats);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_POWER_CAPS) (PNV_VOID pvContext, PADAPTER_POWERCAPS pADPowerCaps);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_POWER_STATE) (PNV_VOID pvContext, PADAPTER_POWERSTATE pADPowerState);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_SET_POWER_STATE) (PNV_VOID pvContext, PADAPTER_POWERSTATE pADPowerState);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_SET_LOW_SPEED_FOR_PM) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_GET_PACKET_FILTER_CAPS) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_SET_PACKET_FILTER) (PNV_VOID pvContext, PPACKET_FILTER pPacketFilter);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_SET_WAKE_UP_PATTERN) (PNV_VOID pvContext, NV_SINT32 iPattern, PWAKE_UP_PATTERN pPattern);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_ENABLE_WAKE_UP_PATTERN) (PNV_VOID pvContext, NV_SINT32 iPattern, NV_SINT32 iEnable);
typedef NV_API_CALL NV_SINT32 (* PFN_SET_NODE_ADDRESS) (PNV_VOID pvContext, NV_UINT8 *pNodeAddress);
typedef NV_API_CALL NV_SINT32 (* PFN_GET_NODE_ADDRESS) (PNV_VOID pvContext, NV_UINT8 *pNodeAddress);
typedef NV_API_CALL NV_SINT32 (* PFN_GET_ADAPTER_INFO) (PNV_VOID pvContext, PNV_VOID pVoidPtr, NV_SINT32 iType, NV_SINT32 *piLength);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_READ_PHY)  (PNV_VOID pvContext, NV_UINT32 ulPhyAddr, NV_UINT32 ulPhyReg, NV_UINT32 *pulValue);
typedef NV_API_CALL NV_SINT32 (* PFN_ADAPTER_WRITE_PHY) (PNV_VOID pvContext, NV_UINT32 ulPhyAddr, NV_UINT32 ulPhyReg, NV_UINT32 ulValue);
typedef NV_API_CALL NV_VOID(* PFN_ADAPTER_SET_SPPED_DUPLEX) (PNV_VOID pvContext);
typedef NV_API_CALL NV_SINT32 (*PFN_REGISTER_OFFLOAD) (PNV_VOID pvContext,  PADAPTER_OFFLOAD pOffload);
typedef NV_API_CALL NV_SINT32 (*PFN_DEREGISTER_OFFLOAD) (PNV_VOID pvContext, PADAPTER_OFFLOAD pOffload);
typedef NV_API_CALL NV_SINT32 (*PFN_RX_BUFF_READY) (PNV_VOID pvContext, PMEMORY_BLOCK pMemBlock, PNV_VOID pvID);

#ifndef linux
typedef NV_SINT32 (*PFN_ADAPTER_ASF_SETUPREGISTERS) (PNV_VOID pvContext, NV_SINT32 bInitTime);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_GETSEC0BASEADDRESS) (PNV_VOID pvContext, ASF_SEC0_BASE **ppsSec0Base);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_SETSOURCEIPADDRESS) (PNV_VOID pvContext, NV_UINT8 *pucSrcIPAddress);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_GETDESTIPADDRESS) (PNV_VOID pvContext, NV_UINT8 *pucDestIPAddress);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_SETDESTIPADDRESS) (PNV_VOID pvContext, NV_UINT8 *pucDestIPAddress);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_WRITEEEPROMANDSETUPREGISTERS) (PNV_VOID pvContext, NV_BOOLEAN bCompare, PNV_VOID pucValue, PNV_VOID pszSec0BaseMember, 
                                      NV_UINT16 usCount, NV_UINT32 ulAddressOffset);

typedef NV_SINT32 (*PFN_ADAPTER_ASF_ISASFREADY) (PNV_VOID pvContext, ASF_ASFREADY *psASFReady);

typedef NV_SINT32 (*PFN_ADAPTER_ASF_SETDESTMACADDRESS) (PNV_VOID pvContext, NV_UINT8 *pucDestMACAddress);
typedef NV_SINT32 (*PFN_ADAPTER_ASF_GETSOURCEMACADDRESS) (PNV_VOID pvContext, NV_UINT8 *pucSrcMACAddress);

typedef NV_SINT32 (*PFN_ADAPTER_ASF_CHECK_FOR_EEPROM_PRESENCE)  (PNV_VOID pvContext);
#endif

typedef NV_API_CALL NV_VOID (*PFN_ADAPTER_SET_COMMONDATA) (PNV_VOID pvContext, CMNDATA_OS_ADAPTER *psOSAdpater);
typedef NV_API_CALL NV_VOID (*PFN_ADAPTER_SET_CHECKSUMOFFLOAD) (PNV_VOID pvContext, NV_UINT32 bSet);


 
typedef struct  _ADAPTER_API
{
    // The adapter context
    PNV_VOID                                   pADCX;

    // The adapter interface
    PFN_ADAPTER_CLOSE                       pfnClose;
    PFN_ADAPTER_INIT                        pfnInit;
    PFN_ADAPTER_DEINIT                      pfnDeinit;
    PFN_ADAPTER_START                       pfnStart;
    PFN_ADAPTER_STOP                        pfnStop;
    PFN_ADAPTER_QUERY_WRITE_SLOTS           pfnQueryWriteSlots;
    PFN_ADAPTER_WRITE                       pfnWrite;
    PFN_ADAPTER_WRITE1                      pfnWrite1;
    PFN_ADAPTER_QUERY_INTERRUPT             pfnQueryInterrupt;
    PFN_ADAPTER_HANDLE_INTERRUPT            pfnHandleInterrupt;
    PFN_ADAPTER_DISABLE_INTERRUPTS          pfnDisableInterrupts;
    PFN_ADAPTER_ENABLE_INTERRUPTS           pfnEnableInterrupts;
    PFN_ADAPTER_CLEAR_INTERRUPTS            pfnClearInterrupts;
    PFN_ADAPTER_CLEAR_TX_DESC                pfnClearTxDesc;
    PFN_ADAPTER_GET_LINK_SPEED              pfnGetLinkSpeed;
    PFN_ADAPTER_GET_LINK_MODE               pfnGetLinkMode;
    PFN_ADAPTER_GET_LINK_STATE              pfnGetLinkState;
    PFN_ADAPTER_IS_LINK_INITIALIZING        pfnIsLinkInitializing;
    PFN_ADAPTER_RESET_PHY_INIT_STATE        pfnResetPhyInitState;
    PFN_ADAPTER_GET_TRANSMIT_QUEUE_SIZE     pfnGetTransmitQueueSize;
    PFN_ADAPTER_GET_RECEIVE_QUEUE_SIZE      pfnGetReceiveQueueSize;
    PFN_ADAPTER_GET_STATISTICS              pfnGetStatistics;
    PFN_ADAPTER_GET_POWER_CAPS              pfnGetPowerCaps;
    PFN_ADAPTER_GET_POWER_STATE             pfnGetPowerState;
    PFN_ADAPTER_SET_POWER_STATE             pfnSetPowerState;
    PFN_ADAPTER_SET_LOW_SPEED_FOR_PM        pfnSetLowSpeedForPM;
    PFN_ADAPTER_GET_PACKET_FILTER_CAPS      pfnGetPacketFilterCaps;
    PFN_ADAPTER_SET_PACKET_FILTER           pfnSetPacketFilter;
    PFN_ADAPTER_SET_WAKE_UP_PATTERN         pfnSetWakeUpPattern;
    PFN_ADAPTER_ENABLE_WAKE_UP_PATTERN      pfnEnableWakeUpPattern;
    PFN_SET_NODE_ADDRESS                    pfnSetNodeAddress;
    PFN_GET_NODE_ADDRESS                    pfnGetNodeAddress;
    PFN_GET_ADAPTER_INFO                    pfnGetAdapterInfo;
    PFN_ADAPTER_SET_SPPED_DUPLEX            pfnSetSpeedDuplex;
    PFN_ADAPTER_READ_PHY                    pfnReadPhy;
    PFN_ADAPTER_WRITE_PHY                    pfnWritePhy;
    PFN_REGISTER_OFFLOAD                    pfnRegisterOffload;
    PFN_DEREGISTER_OFFLOAD                    pfnDeRegisterOffload;
    PFN_RX_BUFF_READY                        pfnRxBuffReady;
#ifndef linux
    PFN_ADAPTER_ASF_SETUPREGISTERS          pfnASFSetupRegisters;
    PFN_ADAPTER_ASF_GETSEC0BASEADDRESS      pfnASFGetSec0BaseAddress;
    PFN_ADAPTER_ASF_SETSOURCEIPADDRESS      pfnASFSetSourceIPAddress;
    PFN_ADAPTER_ASF_GETDESTIPADDRESS        pfnASFGetDestIPAddress;
    PFN_ADAPTER_ASF_SETDESTIPADDRESS        pfnASFSetDestIPAddress;
    PFN_ADAPTER_ASF_WRITEEEPROMANDSETUPREGISTERS pfnASFWriteEEPROMAndSetupRegisters;
    PFN_ADAPTER_ASF_SETDESTMACADDRESS       pfnASFSetDestMACAddress;
    PFN_ADAPTER_ASF_GETSOURCEMACADDRESS     pfnASFGetSourceMACAddress;
    PFN_ADAPTER_ASF_ISASFREADY              pfnASFIsASFReady;
    PFN_ADAPTER_ASF_CHECK_FOR_EEPROM_PRESENCE pfnASFCheckForEepromPresence;
#endif
    PFN_ADAPTER_SET_COMMONDATA              pfnSetCommonData;

    PFN_ADAPTER_SET_CHECKSUMOFFLOAD         pfnSetChecksumOffload;

}   ADAPTER_API, *PADAPTER_API;
//////////////////////////////////////////////////////////////////

#define MAX_PACKET_TO_ACCUMULATE    16

typedef struct _ADAPTER_OPEN_PARAMS
{
    PNV_VOID pOSApi; //pointer to OSAPI structure passed from higher layer
    PNV_VOID pvHardwareBaseAddress; //memory mapped address passed from higher layer
    NV_UINT32 ulPollInterval; //poll interval in micro seconds. Used in polling mode
    NV_UINT32 MaxDpcLoop; //Maximum number of times we loop to in function ADAPTER_HandleInterrupt
    NV_UINT32 MaxRxPkt; //Maximum number of packet we process each time in function UpdateReceiveDescRingData
    NV_UINT32 MaxTxPkt; //Maximum number of packet we process each time in function UpdateTransmitDescRingData
    NV_UINT32 MaxRxPktToAccumulate; //maximum number of rx packet we accumulate in UpdateReceiveDescRingData before
                                //indicating packets to OS.
    NV_UINT32 SentPacketStatusSuccess; //Status returned from adapter layer to higher layer when packet was sent successfully
    NV_UINT32 SentPacketStatusFailure; ////Status returned from adapter layer to higher layer when packet send was unsuccessful
    NV_UINT32 SetForcedModeEveryNthRxPacket; //NOT USED: For experiment with descriptor based interrupt
    NV_UINT32 SetForcedModeEveryNthTxPacket; //NOT USED: For experiment with descriptor based interrupt
    NV_UINT32 RxForcedInterrupt; //NOT USED: For experiment with descriptor based interrupt
    NV_UINT32 TxForcedInterrupt; //NOT USED: For experiment with descriptor based interrupt
    NV_UINT32 DeviceId; //Of MAC
    NV_UINT32 DeviceType;
    NV_UINT32 PollIntervalInusForThroughputMode; //Of MAC
    NV_UINT32 bASFEnabled;
    NV_UINT32 ulDescriptorVersion;
    NV_UINT32 ulMaxPacketSize;


#define MEDIA_IF_AUTO       0
#define MEDIA_IF_RGMII      1
#define MEDIA_IF_MII        2
    NV_UINT32 ulMediaIF;

	NV_UINT32	PhyPowerIsolationTimeoutInms;
	NV_UINT32	PhyResetTimeoutInms;
	NV_UINT32	PhyAutonegotiateTimeoutInms;
	NV_UINT32	PhyLinkupTimeoutInms;
	NV_UINT32	PhyRdWrTimeoutInus;
	NV_UINT32	PhyPowerdownOnClose;

    // Added for Bug 100715
    NV_UINT32   bDisableMIIInterruptAndReadPhyStatus;

}ADAPTER_OPEN_PARAMS, *PADAPTER_OPEN_PARAMS;

//////////////////////////////////////////////////////////////////
// This is the one function in the adapter interface that is publicly
// available. The rest of the interface is returned in the pAdapterApi.
// The first argument needs to be cast to a OSAPI structure pointer.
// The second argument should be cast to a ADPATER_API structure pointer.
NV_API_CALL NV_SINT32 ADAPTER_Open (PADAPTER_OPEN_PARAMS pAdapterOpenParams, PNV_VOID *pvpAdapterApi, NV_UINT32 *pulPhyAddr);

//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// Here are the error codes the adapter function calls return.
#define ADAPTERERR_NONE                             0x0000
#define ADAPTERERR_COULD_NOT_ALLOC_CONTEXT          0x0001
#define ADAPTERERR_COULD_NOT_CREATE_CONTEXT         0x0002
#define ADAPTERERR_COULD_NOT_OPEN_PHY               0x0003
#define ADAPTERERR_TRANSMIT_QUEUE_FULL              0x0004
#define ADAPTERERR_COULD_NOT_INIT_PHY               0x0005
#define ADAPTERERR_PHYS_SIZE_SMALL                    0x0006
#define ADAPTERERR_ERROR                            0x0007  // Generic error
//////////////////////////////////////////////////////////////////

// This block moved from myadap.h
// nFlag for Stop/Start ReceiverAndOrTransmitter can be an OR of
// the following two flags
#define AFFECT_RECEIVER     0x01
#define AFFECT_TRANSMITTER  0x02

#define REDUCE_LENGTH_BY 48

#define EXTRA_WRITE_SLOT_TO_REDUCE_PER_SEND    4
#define MAX_TX_DESCS                    256 
#define MAX_TX_DESCS_VER2               (256 * 4)

typedef struct _TX_INFO_ADAP
{
    NV_UINT32   NoOfDesc; 
    PNV_VOID    pvVar2; 
}TX_INFO_ADAP, *PTX_INFO_ADAP;

#define WORKAROUND_FOR_MCP3_TX_STALL

#ifdef WORKAROUND_FOR_MCP3_TX_STALL
NV_SINT32 ADAPTER_WorkaroundTXHang(PNV_VOID pvContext);
#endif

//#define TRACK_INIT_TIME

#ifdef TRACK_INIT_TIME
//This routine is defined in entry.c adapter doesn't link int64.lib
//We defined here so that its easy to use it in phy as well as mswin

#define MAX_PRINT_INDEX        32
extern NV_VOID PrintTime(NV_UINT32 ulIndex);
#define PRINT_INIT_TIME(_a) PrintTime((_a))
#else
#define PRINT_INIT_TIME(_a)
#endif

// Segmentation offload info
#define DEVCAPS_SEGOL_BP_ENABLE       0   
#define DEVCAPS_SEGOL_BP_IPOPTIONS    1
#define DEVCAPS_SEGOL_BP_TCPOPTIONS   2
#define DEVCAPS_SEGOL_BP_SEGSIZE_LO   8
#define DEVCAPS_SEGOL_BP_SEGSIZE_HI   31


// Checksum offload info
// Byte 0 : V4 TX
#define DEVCAPS_V4_TX_BP_IPOPTIONS      0
#define DEVCAPS_V4_TX_BP_TCPOPTIONS     1
#define DEVCAPS_V4_TX_BP_TCPCHECKSUM    2
#define DEVCAPS_V4_TX_BP_UDPCHECKSUM    3
#define DEVCAPS_V4_TX_BP_IPCHECKSUM     4

// Byte 0 : V4 RX
#define DEVCAPS_V4_RX_BP_IPOPTIONS      8
#define DEVCAPS_V4_RX_BP_TCPOPTIONS     9
#define DEVCAPS_V4_RX_BP_TCPCHECKSUM    10
#define DEVCAPS_V4_RX_BP_UDPCHECKSUM    11
#define DEVCAPS_V4_RX_BP_IPCHECKSUM     12

// Byte 1 : V6 TX
#define DEVCAPS_V6_TX_BP_IPOPTIONS      16
#define DEVCAPS_V6_TX_BP_TCPOPTIONS     17
#define DEVCAPS_V6_TX_BP_TCPCHECKSUM    18
#define DEVCAPS_V6_TX_BP_UDPCHECKSUM    19

// Byte 2 : V6 RX
#define DEVCAPS_V6_RX_BP_IPOPTIONS      24
#define DEVCAPS_V6_RX_BP_TCPOPTIONS     25
#define DEVCAPS_V6_RX_BP_TCPCHECKSUM    26
#define DEVCAPS_V6_RX_BP_UDPCHECKSUM    27


#define DESCR_VER_1         1       // MCP1, MCP2 and CK8 descriptor version
#define DESCR_VER_2         2       // The decsriptor structure for CK8G

// Get device and vendor IDs from 32 bit DeviceVendorID 
#define GET_DEVICEID(x)   (((x) >> 16) & 0xFFFF)
#define GET_VENDORID(x)   ((x) & 0xFFFF)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _ADAPTER_H_
