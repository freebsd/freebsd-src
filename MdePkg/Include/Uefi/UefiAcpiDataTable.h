/** @file
  UEFI ACPI Data Table Definition.

Copyright (c) 2011 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UEFI_ACPI_DATA_TABLE_H__
#define __UEFI_ACPI_DATA_TABLE_H__

#include <IndustryStandard/Acpi.h>

#pragma pack(1)
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
  GUID                           Identifier;
  UINT16                         DataOffset;
} EFI_ACPI_DATA_TABLE;
#pragma pack()

#endif
