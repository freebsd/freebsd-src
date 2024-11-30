/** @file
  Google Test mocks for UefiRuntimeServicesTableLib

  Copyright (c) 2022, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>

MOCK_INTERFACE_DEFINITION (MockUefiRuntimeServicesTableLib);

MOCK_FUNCTION_DEFINITION (MockUefiRuntimeServicesTableLib, gRT_GetVariable, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiRuntimeServicesTableLib, gRT_SetVariable, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiRuntimeServicesTableLib, gRT_GetTime, 2, EFIAPI);

static EFI_RUNTIME_SERVICES  localRt = {
  { 0 },            // EFI_TABLE_HEADER

  gRT_GetTime,      // EFI_GET_TIME
  NULL,             // EFI_SET_TIME
  NULL,             // EFI_GET_WAKEUP_TIME
  NULL,             // EFI_SET_WAKEUP_TIME

  NULL,             // EFI_SET_VIRTUAL_ADDRESS_MAP
  NULL,             // EFI_CONVERT_POINTER

  gRT_GetVariable,  // EFI_GET_VARIABLE
  NULL,             // EFI_GET_NEXT_VARIABLE_NAME
  gRT_SetVariable,  // EFI_SET_VARIABLE

  NULL,             // EFI_GET_NEXT_HIGH_MONO_COUNT
  NULL,             // EFI_RESET_SYSTEM

  NULL,             // EFI_UPDATE_CAPSULE
  NULL,             // EFI_QUERY_CAPSULE_CAPABILITIES

  NULL,             // EFI_QUERY_VARIABLE_INFO
};

extern "C" {
  EFI_RUNTIME_SERVICES  *gRT = &localRt;
}
