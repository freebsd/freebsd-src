/** @file
  Support for the PCI Express 3.0 standard.

  This header file may not define all structures.  Please extend as required.

  Copyright (c) 2014 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _PCIEXPRESS30_H_
#define _PCIEXPRESS30_H_

#include <IndustryStandard/PciExpress21.h>

#pragma pack(1)

#define PCI_EXPRESS_EXTENDED_CAPABILITY_SECONDARY_PCIE_ID    0x0019
#define PCI_EXPRESS_EXTENDED_CAPABILITY_SECONDARY_PCIE_VER1  0x1

typedef union {
  struct {
    UINT32 PerformEqualization : 1;
    UINT32 LinkEqualizationRequestInterruptEnable : 1;
    UINT32 Reserved : 30;
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_LINK_CONTROL3;

typedef union {
  struct {
    UINT16 DownstreamPortTransmitterPreset : 4;
    UINT16 DownstreamPortReceiverPresetHint : 3;
    UINT16 Reserved : 1;
    UINT16 UpstreamPortTransmitterPreset : 4;
    UINT16 UpstreamPortReceiverPresetHint : 3;
    UINT16 Reserved2 : 1;
  } Bits;
  UINT16   Uint16;
} PCI_EXPRESS_REG_LANE_EQUALIZATION_CONTROL;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                Header;
  PCI_EXPRESS_REG_LINK_CONTROL3                           LinkControl3;
  UINT32                                                  LaneErrorStatus;
  PCI_EXPRESS_REG_LANE_EQUALIZATION_CONTROL               EqualizationControl[2];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_SECONDARY_PCIE;

#pragma pack()

#endif
