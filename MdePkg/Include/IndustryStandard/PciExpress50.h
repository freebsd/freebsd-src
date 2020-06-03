/** @file
Support for the PCI Express 5.0 standard.

This header file may not define all structures.  Please extend as required.

Copyright (c) 2020, American Megatrends International LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PCIEXPRESS50_H_
#define _PCIEXPRESS50_H_

#include <IndustryStandard/PciExpress40.h>

#pragma pack(1)

/// The Physical Layer PCI Express Extended Capability definitions.
///
/// Based on section 7.7.6 of PCI Express Base Specification 5.0.
///@{
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_32_0_ID    0x002A
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_32_0_VER1  0x1

// Register offsets from Physical Layer PCI-E Ext Cap Header
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CAPABILITIES_OFFSET                         0x04
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CONTROL_OFFSET                              0x08
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_STATUS_OFFSET                               0x0C
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA1_OFFSET               0x10
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA2_OFFSET               0x14
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA1_OFFSET              0x18
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA2_OFFSET              0x1C
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_LANE_EQUALIZATION_CONTROL_OFFSET            0x20

typedef union {
  struct {
    UINT32 EqualizationByPassToHighestRateSupport                  : 1; // bit 0
    UINT32 NoEqualizationNeededSupport                             : 1; // bit 1
    UINT32 Reserved1                                               : 6; // Reserved bit 2:7
    UINT32 ModifiedTSUsageMode0Support                             : 1; // bit 8
    UINT32 ModifiedTSUsageMode1Support                             : 1; // bit 9
    UINT32 ModifiedTSUsageMode2Support                             : 1; // bit 10
    UINT32 ModifiedTSReservedUsageModes                            : 5; // bit 11:15
    UINT32 Reserved2                                               : 16; // Reserved bit 16:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CAPABILITIES;

typedef union {
  struct {
    UINT32 EqualizationByPassToHighestRateDisable                  : 1; // bit 0
    UINT32 NoEqualizationNeededDisable                             : 1; // bit 1
    UINT32 Reserved1                                               : 6; // Reserved bit 2:7
    UINT32 ModifiedTSUsageModeSelected                             : 3; // bit 8:10
    UINT32 Reserved2                                               : 21; // Reserved bit 11:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CONTROL;

typedef union {
  struct {
    UINT32 EqualizationComplete      : 1; // bit 0
    UINT32 EqualizationPhase1Success : 1; // bit 1
    UINT32 EqualizationPhase2Success : 1; // bit 2
    UINT32 EqualizationPhase3Success : 1; // bit 3
    UINT32 LinkEqualizationRequest   : 1; // bit 4
    UINT32 ModifiedTSRcvd            : 1; // bit 5
    UINT32 RcvdEnhancedLinkControl   : 2; // bit 6:7
    UINT32 TransmitterPrecodingOn    : 1; // bit 8
    UINT32 TransmitterPrecodeRequest : 1; // bit 9
    UINT32 NoEqualizationNeededRcvd  : 1; // bit 10
    UINT32 Reserved                  : 21; // Reserved bit 11:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_STATUS;

typedef union {
  struct {
    UINT32 RcvdModifiedTSUsageMode   : 3; // bit 0:2
    UINT32 RcvdModifiedTSUsageInfo1  : 13; // bit 3:15
    UINT32 RcvdModifiedTSVendorId    : 16; // bit 16:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA1;

typedef union {
  struct {
    UINT32 RcvdModifiedTSUsageInfo2     : 24; // bit 0:23
    UINT32 AltProtocolNegotiationStatus : 2; // bit 24:25
    UINT32 Reserved                     : 6; // Reserved bit 26:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA2;

typedef union {
  struct {
    UINT32 TransModifiedTSUsageMode   : 3; // bit 0:2
    UINT32 TransModifiedTSUsageInfo1  : 13; // bit 3:15
    UINT32 TransModifiedTSVendorId    : 16; // bit 16:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA1;

typedef union {
  struct {
    UINT32 TransModifiedTSUsageInfo2    : 24; // bit 0:23
    UINT32 AltProtocolNegotiationStatus : 2; // bit 24:25
    UINT32 Reserved                     : 6; // Reserved bit 26:31
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA2;

typedef union {
  struct {
    UINT8 DownstreamPortTransmitterPreset : 4; //bit 0..3
    UINT8 UpstreamPortTransmitterPreset   : 4; //bit 4..7
  } Bits;
  UINT8   Uint8;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_LANE_EQUALIZATION_CONTROL;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                      Header;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CAPABILITIES              Capablities;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_CONTROL                   Control;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_STATUS                    Status;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA1    RcvdModifiedTs1Data;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_RCVD_MODIFIED_TS_DATA2    RcvdModifiedTs2Data;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA1   TransModifiedTs1Data;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_TRANS_MODIFIED_TS_DATA2   TransModifiedTs2Data;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_32_0_LANE_EQUALIZATION_CONTROL LaneEqualizationControl[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_PHYSICAL_LAYER_32_0;
///@}

#pragma pack()

#endif
