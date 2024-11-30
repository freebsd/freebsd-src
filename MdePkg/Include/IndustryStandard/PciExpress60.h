/** @file
Support for the PCI Express 6.0 standard.

This header file may not define all structures.  Please extend as required.

Copyright (c) 2024, American Megatrends International LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCIEXPRESS60_H_
#define PCIEXPRESS60_H_

#include <IndustryStandard/PciExpress50.h>

/// The Physical Layer PCI Express Extended Capability definitions.
///
/// Based on section 7.7.7 of PCI Express Base Specification 6.0.
///@{
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_64_0_ID    0x0031
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_64_0_VER1  0x1

// Register offsets from Physical Layer PCI-E Ext Cap Header
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CAPABILITIES_OFFSET               0x04
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CONTROL_OFFSET                    0x08
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_STATUS_OFFSET                     0x0C
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_LANE_EQUALIZATION_CONTROL_OFFSET  0x10

#define PCI_EXPRESS_EXTENDED_CAPABILITY_DEVICE3_ID    0x002F
#define PCI_EXPRESS_EXTENDED_CAPABILITY_DEVICE3_VER1  0x1

#define EFI_PCIE_CAPABILITY_DEVICE_CAPABILITIES_3_OFFSET  0x04
#define EFI_PCIE_CAPABILITY_DEVICE_CONTROL_3_OFFSET       0x08
#define EFI_PCIE_CAPABILITY_DEVICE_STATUS_3_OFFSET        0x0C

#pragma pack(1)

typedef union {
  struct {
    UINT32    Reserved : 32;               // Reserved bit 0:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CAPABILITIES;

typedef union {
  struct {
    UINT32    Reserved : 32;               // Reserved bit 0:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CONTROL;

typedef union {
  struct {
    UINT32    EqualizationComplete      : 1;  // bit 0
    UINT32    EqualizationPhase1Success : 1;  // bit 1
    UINT32    EqualizationPhase2Success : 1;  // bit 2
    UINT32    EqualizationPhase3Success : 1;  // bit 3
    UINT32    LinkEqualizationRequest   : 1;  // bit 4
    UINT32    TransmitterPrecodingOn    : 1;  // bit 5
    UINT32    TransmitterPrecodeRequest : 1;  // bit 6
    UINT32    NoEqualizationNeededRcvd  : 1;  // bit 7
    UINT32    Reserved                  : 24; // Reserved bit 8:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_STATUS;

typedef union {
  struct {
    UINT8    DownstreamPortTransmitterPreset : 4; // bit 0..3
    UINT8    UpstreamPortTransmitterPreset   : 4; // bit 4..7
  } Bits;
  UINT8    Uint8;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_LANE_EQUALIZATION_CONTROL;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                         Header;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CAPABILITIES                 Capablities;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_CONTROL                      Control;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_STATUS                       Status;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_64_0_LANE_EQUALIZATION_CONTROL    LaneEqualizationControl[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_PHYSICAL_LAYER_64_0;
///@}

typedef union {
  struct {
    UINT32    DmwrRequestRouting        :   1;  // bit 0
    UINT32    FourteenBitTagCompleter   :   1;  // bit 1
    UINT32    FourteenBitTagRequester   :   1;  // bit 2
    UINT32    ReceiverL0p               :   1;  // bit 3
    UINT32    PortL0pExitLatencyLatency :   3;  // bit 4..6
    UINT32    RetimerL0pExit            :   3;  // bit 7..9
    UINT32    Reserved                  :   22; // bit 10..31
  } Bits;
  UINT32    Uint32;
} PCI_REG_PCIE_DEVICE_CAPABILITY3;

typedef union {
  struct {
    UINT32    DmwrRequesterEnable           :   1;  // bit 0
    UINT32    DmwrEgressBlocking            :   1;  // bit 1
    UINT32    FourteenBitTagRequesterEnable :   1;  // bit 2
    UINT32    L0pEnable                     :   1;  // bit 3
    UINT32    TargetLinkWidth               :   3;  // bit 4..6
    UINT32    Reserved                      :   25; // bit 7..31
  } Bits;
  UINT32    Uint32;
} PCI_REG_PCIE_DEVICE_CONTROL3;

typedef union {
  struct {
    UINT32    InitialLinkWidth   :   3;  // bit 0..2
    UINT32    SegmentCaptured    :   1;  // bit 3
    UINT32    RemoteL0pSupported :   1;  // bit 4
    UINT32    Reserved           :   27; // bit 5..31
  } Bits;
  UINT32    Uint32;
} PCI_REG_PCIE_DEVICE_STATUS3;

#pragma pack()

#endif
