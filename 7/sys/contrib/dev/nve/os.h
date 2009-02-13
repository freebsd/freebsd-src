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
    FILE:   os.h
    DATE:   2/7/00

    This file contains the os interface. Note that the os interface is
    itself an OS-independent API. The OS specific module is implemented
    by ndis.c for Win9X/NT and linuxnet.c for linux.
*/
#ifndef _OS_H_
#define _OS_H_

#include "phy.h"

#define HDO_VERSION_STRING "HDR O: $Revision: #21 $";

// This is the maximum packet size that we will be sending
// #define MAX_PACKET_SIZE     2048
//#define RX_BUFFER_SIZE      2048

#define MIN_PACKET_MTU_SIZE     576
#define MAX_PACKET_MTU_SIZE     9202
#define MAX_PACKET_SIZE_2048      2048
#define MAX_PACKET_SIZE_1514    1514
#define MAX_PACKET_SIZE_1518    1518
#define MAX_PACKET_SIZE_JUMBO   (9 * 1024)

typedef struct  _MEMORY_BLOCK
{
    PNV_VOID   pLogical;
    PNV_VOID   pPhysical;
    NV_UINT32    uiLength;
}   MEMORY_BLOCK, *PMEMORY_BLOCK;

#define        ALLOC_MEMORY_NONCACHED    0x0001
#define        ALLOC_MEMORY_ALIGNED    0x0002

typedef struct  _MEMORY_BLOCKEX
{
    PNV_VOID   pLogical;
    PNV_VOID   pPhysical;
    NV_UINT32    uiLength;
    /* Parameter to OS layer to indicate what type of memory is needed */
    NV_UINT16    AllocFlags;
    NV_UINT16    AlignmentSize; //always power of 2
    /* Following three fields used for aligned memory allocation */
    PNV_VOID   pLogicalOrig;
    NV_UINT32    pPhysicalOrigLow;
    NV_UINT32    pPhysicalOrigHigh;
    NV_UINT32    uiLengthOrig;
}   MEMORY_BLOCKEX, *PMEMORY_BLOCKEX;


// The typedefs for the OS functions
typedef NV_API_CALL NV_SINT32 (* PFN_MEMORY_ALLOC) (PNV_VOID pOSCX, PMEMORY_BLOCK pMem);
typedef NV_API_CALL NV_SINT32 (* PFN_MEMORY_FREE)  (PNV_VOID pOSCX, PMEMORY_BLOCK pMem);
typedef NV_API_CALL NV_SINT32 (* PFN_MEMORY_ALLOCEX) (PNV_VOID pOSCX, PMEMORY_BLOCKEX pMem);
typedef NV_API_CALL NV_SINT32 (* PFN_MEMORY_FREEEX)  (PNV_VOID pOSCX, PMEMORY_BLOCKEX pMem);
typedef NV_API_CALL NV_SINT32 (* PFN_CLEAR_MEMORY)  (PNV_VOID pOSCX, PNV_VOID pMem, NV_SINT32 iLength);
typedef NV_API_CALL NV_SINT32 (* PFN_STALL_EXECUTION) (PNV_VOID pOSCX, NV_UINT32 ulTimeInMicroseconds);
typedef NV_API_CALL NV_SINT32 (* PFN_ALLOC_RECEIVE_BUFFER) (PNV_VOID pOSCX, PMEMORY_BLOCK pMem, PNV_VOID *ppvID);
typedef NV_API_CALL NV_SINT32 (* PFN_FREE_RECEIVE_BUFFER) (PNV_VOID pOSCX, PMEMORY_BLOCK pMem, PNV_VOID pvID);
typedef NV_API_CALL NV_SINT32 (* PFN_PACKET_WAS_SENT) (PNV_VOID pOSCX, PNV_VOID pvID, NV_UINT32 ulSuccess);
typedef NV_API_CALL NV_SINT32 (* PFN_PACKET_WAS_RECEIVED) (PNV_VOID pOSCX, PNV_VOID pvADReadData, NV_UINT32 ulSuccess, NV_UINT8 *pNewBuffer, NV_UINT8 uc8021pPriority);
typedef NV_API_CALL NV_SINT32 (* PFN_LINK_STATE_HAS_CHANGED) (PNV_VOID pOSCX, NV_SINT32 nEnabled);
typedef NV_API_CALL NV_SINT32 (* PFN_ALLOC_TIMER) (PNV_VOID pvContext, PNV_VOID *ppvTimer);
typedef NV_API_CALL NV_SINT32 (* PFN_FREE_TIMER) (PNV_VOID pvContext, PNV_VOID pvTimer);
typedef NV_API_CALL NV_SINT32 (* PFN_INITIALIZE_TIMER) (PNV_VOID pvContext, PNV_VOID pvTimer, PTIMER_FUNC pvFunc, PNV_VOID pvFuncParameter);
typedef NV_API_CALL NV_SINT32 (* PFN_SET_TIMER) (PNV_VOID pvContext, PNV_VOID pvTimer, NV_UINT32 dwMillisecondsDelay);
typedef NV_API_CALL NV_SINT32 (* PFN_CANCEL_TIMER) (PNV_VOID pvContext, PNV_VOID pvTimer);

typedef NV_API_CALL NV_SINT32 (* PFN_PREPROCESS_PACKET) (PNV_VOID pvContext, PNV_VOID pvADReadData, PNV_VOID *ppvID,
                NV_UINT8 *pNewBuffer, NV_UINT8 uc8021pPriority);
typedef NV_API_CALL PNV_VOID (* PFN_PREPROCESS_PACKET_NOPQ) (PNV_VOID pvContext, PNV_VOID pvADReadData);
typedef NV_API_CALL NV_SINT32 (* PFN_INDICATE_PACKETS) (PNV_VOID pvContext, PNV_VOID *ppvID, NV_UINT32 ulNumPacket);
typedef NV_API_CALL NV_SINT32 (* PFN_LOCK_ALLOC) (PNV_VOID pOSCX, NV_SINT32 iLockType, PNV_VOID *ppvLock);
typedef NV_API_CALL NV_SINT32 (* PFN_LOCK_ACQUIRE) (PNV_VOID pOSCX, NV_SINT32 iLockType, PNV_VOID pvLock);
typedef NV_API_CALL NV_SINT32 (* PFN_LOCK_RELEASE) (PNV_VOID pOSCX, NV_SINT32 iLockType, PNV_VOID pvLock);
typedef NV_API_CALL PNV_VOID (* PFN_RETURN_BUFFER_VIRTUAL) (PNV_VOID pvContext, PNV_VOID pvADReadData);

// Here are the OS functions that those objects below the OS interface
// can call up to.
typedef struct  _OS_API
{
    // OS Context -- this is a parameter to every OS API call
    PNV_VOID                       pOSCX;

    // Basic OS functions
    PFN_MEMORY_ALLOC            pfnAllocMemory;
    PFN_MEMORY_FREE             pfnFreeMemory;
    PFN_MEMORY_ALLOCEX          pfnAllocMemoryEx;
    PFN_MEMORY_FREEEX           pfnFreeMemoryEx;
    PFN_CLEAR_MEMORY            pfnClearMemory;
    PFN_STALL_EXECUTION         pfnStallExecution;
    PFN_ALLOC_RECEIVE_BUFFER    pfnAllocReceiveBuffer;
    PFN_FREE_RECEIVE_BUFFER     pfnFreeReceiveBuffer;
    PFN_PACKET_WAS_SENT         pfnPacketWasSent;
    PFN_PACKET_WAS_RECEIVED     pfnPacketWasReceived;
    PFN_LINK_STATE_HAS_CHANGED  pfnLinkStateHasChanged;
    PFN_ALLOC_TIMER                pfnAllocTimer;
    PFN_FREE_TIMER                pfnFreeTimer;
    PFN_INITIALIZE_TIMER        pfnInitializeTimer;
    PFN_SET_TIMER                pfnSetTimer;
    PFN_CANCEL_TIMER            pfnCancelTimer;
    PFN_PREPROCESS_PACKET       pfnPreprocessPacket;
    PFN_PREPROCESS_PACKET_NOPQ  pfnPreprocessPacketNopq;
    PFN_INDICATE_PACKETS        pfnIndicatePackets;
    PFN_LOCK_ALLOC                pfnLockAlloc;
    PFN_LOCK_ACQUIRE            pfnLockAcquire;
    PFN_LOCK_RELEASE            pfnLockRelease;
    PFN_RETURN_BUFFER_VIRTUAL    pfnReturnBufferVirtual;
}   OS_API, *POS_API;

#endif // _OS_H_
