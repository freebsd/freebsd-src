/** @file
  TPM2 ACPI table definition.

Copyright (c) 2013 - 2019, Intel Corporation. All rights reserved. <BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _TPM2_ACPI_H_
#define _TPM2_ACPI_H_

#include <IndustryStandard/Acpi.h>

#pragma pack (1)

#define EFI_TPM2_ACPI_TABLE_REVISION_3  3
#define EFI_TPM2_ACPI_TABLE_REVISION_4  4
#define EFI_TPM2_ACPI_TABLE_REVISION    EFI_TPM2_ACPI_TABLE_REVISION_4

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;
  // Flags field is replaced in version 4 and above
  //    BIT0~15:  PlatformClass      This field is only valid for version 4 and above
  //    BIT16~31: Reserved
  UINT32                      Flags;
  UINT64                      AddressOfControlArea;
  UINT32                      StartMethod;
//UINT8                       PlatformSpecificParameters[];  // size up to 12
//UINT32                      Laml;                          // Optional
//UINT64                      Lasa;                          // Optional
} EFI_TPM2_ACPI_TABLE;

#define EFI_TPM2_ACPI_TABLE_START_METHOD_ACPI                                          2
#define EFI_TPM2_ACPI_TABLE_START_METHOD_TIS                                           6
#define EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE             7
#define EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_ACPI   8
#define EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_SMC    11

typedef struct {
  UINT32   Reserved;
  UINT32   Error;
  UINT32   Cancel;
  UINT32   Start;
  UINT64   InterruptControl;
  UINT32   CommandSize;
  UINT64   Command;
  UINT32   ResponseSize;
  UINT64   Response;
} EFI_TPM2_ACPI_CONTROL_AREA;

#pragma pack ()

#endif
