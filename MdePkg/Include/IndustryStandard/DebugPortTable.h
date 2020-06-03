/** @file
  ACPI debug port table definition, defined at
  Microsoft DebugPortSpecification.

  Copyright (c) 2012 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _DEBUG_PORT_TABLE_H_
#define _DEBUG_PORT_TABLE_H_

#include <IndustryStandard/Acpi.h>

//
// Ensure proper structure formats
//
#pragma pack(1)

//
// Debug Port Table definition.
//
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER               Header;
  UINT8                                     InterfaceType;
  UINT8                                     Reserved_37[3];
  EFI_ACPI_2_0_GENERIC_ADDRESS_STRUCTURE    BaseAddress;
} EFI_ACPI_DEBUG_PORT_DESCRIPTION_TABLE;

#pragma pack()

//
// DBGP Revision (defined in spec)
//
#define EFI_ACPI_DEBUG_PORT_TABLE_REVISION      0x01

//
// Interface Type
//
#define EFI_ACPI_DBGP_INTERFACE_TYPE_FULL_16550                                 0
#define EFI_ACPI_DBGP_INTERFACE_TYPE_16550_SUBSET_COMPATIBLE_WITH_MS_DBGP_SPEC  1

#endif
