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
    FILE:   phy.h
    DATE:   2/7/00

    This file contains the functional interface to the PHY.
*/
#ifndef _PHY_H_
#define _PHY_H_

//#include "basetype.h"
//#include "nvevent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_PHY_ADDRESS   1


#define HDP_VERSION_STRING "HDR P: $Revision: #23 $"

//
// Defaults for PHY timeout values.
//
#define PHY_POWER_ISOLATION_MS_TIMEOUT_DEFAULT      50
#define PHY_RESET_MS_TIMEOUT_DEFAULT                50
#define PHY_AUTONEG_MS_TIMEOUT_DEFAULT              3000
#define PHY_LINK_UP_MS_TIMEOUT_DEFAULT              2400
#define PHY_RDWR_US_TIMEOUT_DEFAULT                 2048
#define PHY_POWER_DOWN_US_TIMEOUT_DEFAULT           500


/////////////////////////////////////////////////////////////////////////
// The phy module knows the values that need to go into the phy registers
// but typically the method of writing those registers is controlled by
// another module (usually the adapter because it is really the hardware
// interface.) Hence, the phy needs routines to call to read and write the
// phy registers. This structure with appropriate routines will be provided
// in the PHY_Open call.

typedef NV_API_CALL NV_SINT32 (* PFN_READ_PHY)  (PNV_VOID pvData, NV_UINT32 ulPhyAddr, NV_UINT32 ulPhyReg, NV_UINT32 *pulValue);
typedef NV_API_CALL NV_SINT32 (* PFN_WRITE_PHY) (PNV_VOID pvData, NV_UINT32 ulPhyAddr, NV_UINT32 ulPhyReg, NV_UINT32 ulValue);

typedef struct  PHY_SUPPORT_API
{
    PNV_VOID            pADCX;
    PFN_READ_PHY        pfnRead;
    PFN_WRITE_PHY       pfnWrite;
	// PFN_EVENT_OCCURED   pfnEventOccurred;

    //
    // These fields are passed down via the FD.  FD get's them
    // from the registry.  They allow one to fine tune the timeout
    // values in the PHY.
    //
    NV_UINT32	PhyPowerIsolationTimeoutInms;
	NV_UINT32	PhyResetTimeoutInms;
	NV_UINT32	PhyAutonegotiateTimeoutInms;
	NV_UINT32	PhyLinkupTimeoutInms;
	NV_UINT32	PhyPowerdownOnCloseInus;

}   PHY_SUPPORT_API, *PPHY_SUPPORT_API;
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// The functional typedefs for the PHY Api
typedef NV_SINT32 (* PFN_PHY_INIT) (PNV_VOID pvContext, NV_UINT32 *pulLinkState, NV_UINT32 PhyMode);
typedef NV_SINT32 (* PFN_PHY_DEINIT) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_PHY_CLOSE) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_GET_LINK_SPEED) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_GET_LINK_MODE) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_GET_LINK_STATE) (PNV_VOID pvContext, NV_UINT32 *pulLinkState);
typedef NV_SINT32 (* PFN_IS_LINK_INITIALIZING) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_RESET_PHY_INIT_STATE) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_FORCE_SPEED_DUPLEX) (PNV_VOID pvContext, NV_UINT16 usSpeed, NV_UINT8 ucForceDpx, NV_UINT8 ucForceMode);
typedef NV_SINT32 (* PFN_PHY_POWERDOWN) (PNV_VOID pvContext);
typedef NV_SINT32 (* PFN_SET_LOW_SPEED_FOR_PM) (PNV_VOID pvContext);


typedef struct  _PHY_API
{
    // This is the context to pass back in as the first arg on all
    // the calls in the API below.
    PNV_VOID               pPHYCX;

    PFN_PHY_INIT                pfnInit;
    PFN_PHY_INIT                pfnInitFast;
    PFN_PHY_DEINIT                pfnDeinit;
    PFN_PHY_CLOSE                pfnClose;
    PFN_GET_LINK_SPEED            pfnGetLinkSpeed;
    PFN_GET_LINK_MODE            pfnGetLinkMode;
    PFN_GET_LINK_STATE            pfnGetLinkState;
    PFN_IS_LINK_INITIALIZING    pfnIsLinkInitializing;
    PFN_RESET_PHY_INIT_STATE    pfnResetPhyInitState;
    PFN_FORCE_SPEED_DUPLEX        pfnForceSpeedDuplex;
    PFN_PHY_POWERDOWN            pfnPowerdown;
    PFN_SET_LOW_SPEED_FOR_PM    pfnSetLowSpeedForPM;
}   PHY_API, *PPHY_API;
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// This is the one function in the PHY interface that is publicly
// available. The rest of the interface is returned in the pPhyApi;
// The first argument needs to be cast to a POS_API structure ptr.
// On input the second argument is a ptr to a PPHY_SUPPORT_API.
// On output, the second argument should be treated as a ptr to a
// PPHY_API and set appropriately.
extern NV_SINT32 PHY_Open (PNV_VOID pvOSApi, PNV_VOID pPhyApi, NV_UINT32 *pulPhyAddr, NV_UINT32 *pulPhyConnected);
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// Here are the error codes the phy functions can return.
#define PHYERR_NONE                                 0x0000
#define PHYERR_COULD_NOT_ALLOC_CONTEXT              0x0001
#define PHYERR_RESET_NEVER_FINISHED                 0x0002
#define PHYERR_NO_AVAILABLE_LINK_SPEED              0x0004
#define PHYERR_INVALID_SETTINGS                     0x0005
#define PHYERR_READ_FAILED                          0x0006
#define PHYERR_WRITE_FAILED                         0x0007
#define PHYERR_NO_PHY                               0x0008
#define PHYERR_NO_RESOURCE                          0x0009
#define PHYERR_POWER_ISOLATION_TIMEOUT              0x000A
#define PHYERR_POWER_DOWN_TIMEOUT                   0x000B
#define PHYERR_AUTONEG_TIMEOUT                      0x000C
#define PHYERR_PHY_LINK_SPEED_UNCHANGED             0x000D

#define PHY_INVALID_PHY_ADDR                    0xFFFF;

/////////////////////////////////////////////////////////////////////////

// This value can be used in the ulPhyLinkSpeed field.
#define PHY_LINK_SPEED_UNKNOWN          0x0FFFFFFFF

//
// Values used to configure PHY mode.
//
#define PHY_MODE_MII    1
#define PHY_MODE_RGMII  2

typedef NV_VOID (* PTIMER_FUNC) (PNV_VOID pvContext);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //_PHY_H_
