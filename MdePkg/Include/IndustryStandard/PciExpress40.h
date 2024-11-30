/** @file
Support for the PCI Express 4.0 standard.

This header file may not define all structures.  Please extend as required.

Copyright (c) 2018, American Megatrends, Inc. All rights reserved.<BR>
Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PCIEXPRESS40_H_
#define _PCIEXPRESS40_H_

#include <IndustryStandard/PciExpress31.h>

#pragma pack(1)

/// The Physical Layer PCI Express Extended Capability definitions.
///
/// Based on section 7.7.5 of PCI Express Base Specification 4.0.
///@{
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_16_0_ID    0x0026
#define PCI_EXPRESS_EXTENDED_CAPABILITY_PHYSICAL_LAYER_16_0_VER1  0x1

// Register offsets from Physical Layer PCI-E Ext Cap Header
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CAPABILITIES_OFFSET                       0x04
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CONTROL_OFFSET                            0x08
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_STATUS_OFFSET                             0x0C
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_LOCAL_DATA_PARITY_STATUS_OFFSET           0x10
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_FIRST_RETIMER_DATA_PARITY_STATUS_OFFSET   0x14
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_SECOND_RETIMER_DATA_PARITY_STATUS_OFFSET  0x18
#define PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_LANE_EQUALIZATION_CONTROL_OFFSET          0x20

typedef union {
  struct {
    UINT32    Reserved : 32;               // Reserved bit 0:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CAPABILITIES;

typedef union {
  struct {
    UINT32    Reserved : 32;               // Reserved bit 0:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CONTROL;

typedef union {
  struct {
    UINT32    EqualizationComplete      : 1;  // bit 0
    UINT32    EqualizationPhase1Success : 1;  // bit 1
    UINT32    EqualizationPhase2Success : 1;  // bit 2
    UINT32    EqualizationPhase3Success : 1;  // bit 3
    UINT32    LinkEqualizationRequest   : 1;  // bit 4
    UINT32    Reserved                  : 27; // Reserved bit 5:31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_STATUS;

typedef union {
  struct {
    UINT8    DownstreamPortTransmitterPreset : 4; // bit 0..3
    UINT8    UpstreamPortTransmitterPreset   : 4; // bit 4..7
  } Bits;
  UINT8    Uint8;
} PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_LANE_EQUALIZATION_CONTROL;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                         Header;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CAPABILITIES                 Capablities;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_CONTROL                      Control;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_STATUS                       Status;
  UINT32                                                           LocalDataParityMismatchStatus;
  UINT32                                                           FirstRetimerDataParityMismatchStatus;
  UINT32                                                           SecondRetimerDataParityMismatchStatus;
  UINT32                                                           Reserved;
  PCI_EXPRESS_REG_PHYSICAL_LAYER_16_0_LANE_EQUALIZATION_CONTROL    LaneEqualizationControl[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_PHYSICAL_LAYER_16_0;
///@}

/// The Designated Vendor Specific Capability definitions
/// Based on section 7.9.6 of PCI Express Base Specification 4.0.
///@{
#define PCI_EXPRESS_EXTENDED_CAPABILITY_DESIGNATED_VENDOR_SPECIFIC_ID  0x0023

typedef union {
  struct {
    UINT32    DvsecVendorId : 16;                                     // bit 0..15
    UINT32    DvsecRevision : 4;                                      // bit 16..19
    UINT32    DvsecLength   : 12;                                     // bit 20..31
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_1;

typedef union {
  struct {
    UINT16    DvsecId : 16;                                           // bit 0..15
  } Bits;
  UINT16    Uint16;
} PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_2;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER           Header;
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_1    DesignatedVendorSpecificHeader1;
  PCI_EXPRESS_DESIGNATED_VENDOR_SPECIFIC_HEADER_2    DesignatedVendorSpecificHeader2;
  UINT8                                              DesignatedVendorSpecific[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_DESIGNATED_VENDOR_SPECIFIC;
///@}

#pragma pack()

#endif
