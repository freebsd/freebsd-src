/** @file
Support for the PCI Express 3.1 standard.

This header file may not define all structures.  Please extend as required.

Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PCIEXPRESS31_H_
#define _PCIEXPRESS31_H_

#include <IndustryStandard/PciExpress30.h>

#pragma pack(1)

#define PCI_EXPRESS_EXTENDED_CAPABILITY_L1_PM_SUBSTATES_ID    0x001E
#define PCI_EXPRESS_EXTENDED_CAPABILITY_L1_PM_SUBSTATES_VER1  0x1

typedef union {
  struct {
    UINT32    PciPmL12              : 1;
    UINT32    PciPmL11              : 1;
    UINT32    AspmL12               : 1;
    UINT32    AspmL11               : 1;
    UINT32    L1PmSubstates         : 1;
    UINT32    Reserved              : 3;
    UINT32    CommonModeRestoreTime : 8;
    UINT32    TPowerOnScale         : 2;
    UINT32    Reserved2             : 1;
    UINT32    TPowerOnValue         : 5;
    UINT32    Reserved3             : 8;
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_L1_PM_SUBSTATES_CAPABILITY;

typedef union {
  struct {
    UINT32    PciPmL12              : 1;
    UINT32    PciPmL11              : 1;
    UINT32    AspmL12               : 1;
    UINT32    AspmL11               : 1;
    UINT32    Reserved              : 4;
    UINT32    CommonModeRestoreTime : 8;
    UINT32    LtrL12ThresholdValue  : 10;
    UINT32    Reserved2             : 3;
    UINT32    LtrL12ThresholdScale  : 3;
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_L1_PM_SUBSTATES_CONTROL1;

typedef union {
  struct {
    UINT32    TPowerOnScale : 2;
    UINT32    Reserved      : 1;
    UINT32    TPowerOnValue : 5;
    UINT32    Reserved2     : 24;
  } Bits;
  UINT32    Uint32;
} PCI_EXPRESS_REG_L1_PM_SUBSTATES_CONTROL2;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER      Header;
  PCI_EXPRESS_REG_L1_PM_SUBSTATES_CAPABILITY    Capability;
  PCI_EXPRESS_REG_L1_PM_SUBSTATES_CONTROL1      Control1;
  PCI_EXPRESS_REG_L1_PM_SUBSTATES_CONTROL2      Control2;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_L1_PM_SUBSTATES;

#pragma pack()

#endif
