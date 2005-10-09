/***************************************************************************\
|*                                                                           *|
|*         Copyright 2001-2003 NVIDIA, Corporation.  All rights reserved.    *|
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
 *   This file contains the header info common to the network drivers and applications.
 *   Currently, these applications include ASF, co-installers, and qstats.
 *
 *
 */

#ifndef _DRVINFO_H_
#define _DRVINFO_H_

// Switch to byte packing, regardless of global packing specified by the compiler switch
#pragma pack(1)  

//////////////////////////////////////////////////////////////////
// For the ADAPTER_GetStatistics call used by qstats.  This 
// is the template used by the legacy driver.
#define MAX_TRANSMIT_COLISION_STATS     16

#define ADAPTER_STATS_LEGACY_VERSION    1
#define ADAPTER_STATS_RM_VERSION        2

typedef struct  _ADAPTER_STATS_V1
{
    NV_UINT32   ulVersion;

    NV_UINT32   ulSuccessfulTransmissions;
    NV_UINT32   ulFailedTransmissions;
    NV_UINT32   ulRetryErrors;
    NV_UINT32   ulUnderflowErrors;
    NV_UINT32   ulLossOfCarrierErrors;
    NV_UINT32   ulLateCollisionErrors;
    NV_UINT32   ulDeferredTransmissions;
    NV_UINT32    ulExcessDeferredTransmissions;
    NV_UINT32   aulSuccessfulTransmitsAfterCollisions[MAX_TRANSMIT_COLISION_STATS];

    NV_UINT32   ulMissedFrames;
    NV_UINT32   ulSuccessfulReceptions;
    NV_UINT32   ulFailedReceptions;
    NV_UINT32   ulCRCErrors;
    NV_UINT32   ulFramingErrors;
    NV_UINT32   ulOverFlowErrors;
    NV_UINT32    ulFrameErrorsPrivate; //Not for public.
    NV_UINT32    ulNullBufferReceivePrivate; //Not for public, These are the packets which we didn't indicate to OS

    //interrupt related statistics
    NV_UINT32   ulRxInterrupt;
    NV_UINT32   ulRxInterruptUnsuccessful;
    NV_UINT32   ulTxInterrupt;
    NV_UINT32   ulTxInterruptUnsuccessful;
    NV_UINT32   ulPhyInterrupt;

}   ADAPTER_STATS_V1, *PADAPTER_STATS_V1;
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
// For the ADAPTER_GetStatistics call used by qstats.  This 
// is the template used by the FD.
typedef struct  _ADAPTER_STATS
{
    NV_UINT32   ulVersion;
    NV_UINT8    ulMacAddress[6];

    //
    // Tx counters.
    //
    NV_UINT64   ulSuccessfulTransmissions;
    NV_UINT64   ulFailedTransmissions;
    NV_UINT64   ulRetryErrors;
    NV_UINT64   ulUnderflowErrors;
    NV_UINT64   ulLossOfCarrierErrors;
    NV_UINT64   ulLateCollisionErrors;
    NV_UINT64   ulDeferredTransmissions;
    NV_UINT64    ulExcessDeferredTransmissions;
    NV_UINT64   aulSuccessfulTransmitsAfterCollisions[MAX_TRANSMIT_COLISION_STATS];

    //
    // New Tx counters for GigE.
    //
    NV_UINT64   ulTxByteCount;

    //
    // Rx counters.
    //
    NV_UINT64   ulMissedFrames;
    NV_UINT64   ulSuccessfulReceptions;
    NV_UINT64   ulFailedReceptions;
    NV_UINT64   ulCRCErrors;
    NV_UINT64   ulLengthErrors;
    NV_UINT64   ulFramingErrors;
    NV_UINT64   ulOverFlowErrors;
    NV_UINT64   ulRxNoBuffer;
    NV_UINT64   ulFrameErrorsPrivate; //Not for public.
    NV_UINT64   ulNullBufferReceivePrivate; //Not for public, These are the packets which we didn't indicate to OS

    //
    // New Rx counters for GigE.
    //
    NV_UINT64   ulRxExtraByteCount;
    NV_UINT64   ulRxFrameTooLongCount;
    NV_UINT64   ulRxFrameAlignmentErrorCount;
    NV_UINT64   ulRxLateCollisionErrors;
    NV_UINT64   ulRxRuntPacketErrors;

    NV_UINT64   ulRxUnicastFrameCount;
    NV_UINT64   ulRxMulticastFrameCount;
    NV_UINT64   ulRxBroadcastFrameCount;
    NV_UINT64   ulRxPromiscuousModeFrameCount;

    //Interrupt related statistics
    NV_UINT64   ulRxInterrupt;
    NV_UINT64   ulRxInterruptUnsuccessful;
    NV_UINT64   ulTxInterrupt;
    NV_UINT64   ulTxInterruptUnsuccessful;
    NV_UINT64   ulPhyInterrupt;


    //
    // Handy things to know
    //
    NV_UINT64   ulDescriptorVersion;
    NV_UINT64   ulPollingCfg;       // configured for cpu or throughput
    NV_UINT64   ulPollingState;     // current optimizefor state.

    NV_UINT64   ulNumTxDesc;
    NV_UINT64   ulNumRxDesc;

    // 
    // Useful to determine if TX is stuck.
    //
    NV_UINT64   ulNumTxPktsQueued;
    NV_UINT64   ulNumTxPktsInProgress;

    //
    // Rx Xsum Cntrs
    //
    NV_UINT64   ulNoRxPktsNoXsum;
    NV_UINT64   ulNoRxPktsXsumIpPassTcpFail;
    NV_UINT64   ulNoRxPktsXsumIpPassUdpFail;
    NV_UINT64   ulNoRxPktsXsumIpFail;
    NV_UINT64   ulNoRxPktsXsumIpPassNoTcpUdp;
    NV_UINT64   ulNoRxPktsXsumIpPassTcpPass;
    NV_UINT64   ulNoRxPktsXsumIpPassUdpPass;
    NV_UINT64   ulNoRxPktsXsumReserved;

#ifdef _PERF_LOOP_CNTRS
    NV_UINT64  ulNumTxCmplsToProcess;
    NV_UINT64  ulNumRxCmplsToProcess;
    NV_UINT64  ulNumIntsToProcess;

    NV_UINT64  IntLoop0Cnt;
    NV_UINT64  IntLoop1Cnt;
    NV_UINT64  IntLoop2Cnt;
    NV_UINT64  IntLoop3Cnt;
    NV_UINT64  IntLoop4Cnt;
    NV_UINT64  IntLoop5Cnt;
    NV_UINT64  IntLoop6To10Cnt;
    NV_UINT64  IntLoop11Cnt;
    NV_UINT64  IntMaxLoopCnt;

    NV_UINT64   IntRxCnt0;
    NV_UINT64   IntTxCnt0;

    NV_UINT64   MaxRxLoopCnt;
    NV_UINT64   MaxTxLoopCnt;

#endif
}   ADAPTER_STATS, *PADAPTER_STATS;
//////////////////////////////////////////////////////////////////

#pragma pack()  


#endif   // #define _DRVINFO_H_


