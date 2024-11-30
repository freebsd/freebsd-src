/** @file MockSmmServicesTableLib.cpp
  Google Test mocks for SmmServicesTableLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockSmmServicesTableLib.h>

MOCK_INTERFACE_DEFINITION (MockSmmServicesTableLib);

MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmAllocatePool, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmFreePool, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmAllocatePages, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmFreePages, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmStartupThisAp, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmInstallProtocolInterface, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmUninstallProtocolInterface, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmHandleProtocol, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmRegisterProtocolNotify, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmLocateHandle, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmLocateProtocol, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmiManage, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmInterruptRegister, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockSmmServicesTableLib, gSmst_SmmInterruptUnRegister, 1, EFIAPI);

static EFI_SMM_SYSTEM_TABLE2  LocalSmst = {
  { 0, 0, 0, 0, 0 },                   // EFI_TABLE_HEADER
  NULL,                                // SmmFirmwareVendor
  0,                                   // SmmFirmwareRevision
  NULL,                                // EFI_SMM_INSTALL_CONFIGURATION_TABLE2
  { NULL },                            // EFI_SMM_CPU_IO2_PROTOCOL
  gSmst_SmmAllocatePool,               // EFI_ALLOCATE_POOL
  gSmst_SmmFreePool,                   // EFI_FREE_POOL
  gSmst_SmmAllocatePages,              // EFI_ALLOCATE_PAGES
  gSmst_SmmFreePages,                  // EFI_FREE_PAGES
  gSmst_SmmStartupThisAp,              // EFI_SMM_STARTUP_THIS_AP
  0,                                   // CurrentlyExecutingCpu
  0,                                   // NumberOfCpus
  NULL,                                // CpuSaveStateSize
  NULL,                                // CpuSaveState
  0,                                   // NumberOfTableEntries
  NULL,                                // EFI_CONFIGURATION_TABLE
  gSmst_SmmInstallProtocolInterface,   // EFI_INSTALL_PROTOCOL_INTERFACE
  gSmst_SmmUninstallProtocolInterface, // EFI_UNINSTALL_PROTOCOL_INTERFACE
  gSmst_SmmHandleProtocol,             // EFI_HANDLE_PROTOCOL
  gSmst_SmmRegisterProtocolNotify,     // EFI_SMM_REGISTER_PROTOCOL_NOTIFY
  gSmst_SmmLocateHandle,               // EFI_LOCATE_HANDLE
  gSmst_SmmLocateProtocol,             // EFI_LOCATE_PROTOCOL
  gSmst_SmiManage,                     // EFI_SMM_INTERRUPT_MANAGE
  gSmst_SmmInterruptRegister,          // EFI_SMM_INTERRUPT_REGISTER
  gSmst_SmmInterruptUnRegister         // EFI_SMM_INTERRUPT_UNREGISTER
};

extern "C" {
  EFI_SMM_SYSTEM_TABLE2  *gSmst = &LocalSmst;
}
