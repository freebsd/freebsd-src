/** @file
  ACPI Watchdog Resource Table (WDRT) as defined at
  Microsoft Windows Hardware Developer Central.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _WATCHDOG_RESOURCE_TABLE_H_
#define _WATCHDOG_RESOURCE_TABLE_H_

#include <IndustryStandard/Acpi.h>

//
// Ensure proper structure formats
//
#pragma pack(1)

///
/// Watchdog Resource Table definition.
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER               Header;
  EFI_ACPI_2_0_GENERIC_ADDRESS_STRUCTURE    ControlRegisterAddress;
  EFI_ACPI_2_0_GENERIC_ADDRESS_STRUCTURE    CountRegisterAddress;
  UINT16                                    PCIDeviceID;
  UINT16                                    PCIVendorID;
  UINT8                                     PCIBusNumber;
  UINT8                                     PCIDeviceNumber;
  UINT8                                     PCIFunctionNumber;
  UINT8                                     PCISegment;
  UINT16                                    MaxCount;
  UINT8                                     Units;
} EFI_ACPI_WATCHDOG_RESOURCE_1_0_TABLE;

#pragma pack()

//
// WDRT Revision (defined in spec)
//
#define EFI_ACPI_WATCHDOG_RESOURCE_1_0_TABLE_REVISION  0x01

//
// WDRT 1.0 Count Unit
//
#define EFI_ACPI_WDRT_1_0_COUNT_UNIT_1_SEC_PER_COUNT         1
#define EFI_ACPI_WDRT_1_0_COUNT_UNIT_100_MILLISEC_PER_COUNT  2
#define EFI_ACPI_WDRT_1_0_COUNT_UNIT_10_MILLISEC_PER_COUNT   3

#endif
