/** @file
  Google Test mocks for UefiRuntimeServicesTableLib

  Copyright (c) 2022, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_UEFI_RUNTIME_SERVICES_TABLE_LIB_H_
#define MOCK_UEFI_RUNTIME_SERVICES_TABLE_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/UefiRuntimeServicesTableLib.h>
}

struct MockUefiRuntimeServicesTableLib {
  MOCK_INTERFACE_DECLARATION (MockUefiRuntimeServicesTableLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gRT_GetVariable,
    (IN      CHAR16    *VariableName,
     IN      EFI_GUID  *VendorGuid,
     OUT     UINT32    *Attributes OPTIONAL,
     IN OUT  UINTN     *DataSize,
     OUT     VOID      *Data)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gRT_SetVariable,
    (IN CHAR16    *VariableName,
     IN EFI_GUID  *VendorGuid,
     IN UINT32    Attributes,
     IN UINTN     DataSize,
     IN VOID      *Data)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gRT_GetTime,
    (OUT  EFI_TIME                    *Time,
     OUT  EFI_TIME_CAPABILITIES       *Capabilities OPTIONAL)
    );
};

#endif
