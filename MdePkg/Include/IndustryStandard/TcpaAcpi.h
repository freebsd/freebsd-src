/** @file
  TCPA ACPI table definition.

Copyright (c) 2013, Intel Corporation. All rights reserved. <BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _TCPA_ACPI_H_
#define _TCPA_ACPI_H_

#include <IndustryStandard/Acpi.h>

#pragma pack (1)

typedef struct _EFI_TCG_CLIENT_ACPI_TABLE {
  EFI_ACPI_DESCRIPTION_HEADER       Header;
  UINT16                            PlatformClass;
  UINT32                            Laml;
  UINT64                            Lasa;
} EFI_TCG_CLIENT_ACPI_TABLE;

typedef struct _EFI_TCG_SERVER_ACPI_TABLE {
  EFI_ACPI_DESCRIPTION_HEADER             Header;
  UINT16                                  PlatformClass;
  UINT16                                  Reserved0;
  UINT64                                  Laml;
  UINT64                                  Lasa;
  UINT16                                  SpecRev;
  UINT8                                   DeviceFlags;
  UINT8                                   InterruptFlags;
  UINT8                                   Gpe;
  UINT8                                   Reserved1[3];
  UINT32                                  GlobalSysInt;
  EFI_ACPI_3_0_GENERIC_ADDRESS_STRUCTURE  BaseAddress;
  UINT32                                  Reserved2;
  EFI_ACPI_3_0_GENERIC_ADDRESS_STRUCTURE  ConfigAddress;
  UINT8                                   PciSegNum;
  UINT8                                   PciBusNum;
  UINT8                                   PciDevNum;
  UINT8                                   PciFuncNum;
} EFI_TCG_SERVER_ACPI_TABLE;

//
// TCG Platform Type based on TCG ACPI Specification Version 1.00
//
#define TCG_PLATFORM_TYPE_CLIENT   0
#define TCG_PLATFORM_TYPE_SERVER   1

#pragma pack ()

#endif
