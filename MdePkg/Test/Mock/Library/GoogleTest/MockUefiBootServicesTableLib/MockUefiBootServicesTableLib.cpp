/** @file
  Google Test mocks for UefiBootServicesTableLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>

MOCK_INTERFACE_DEFINITION (MockUefiBootServicesTableLib);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_GetMemoryMap, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_CreateEvent, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_CloseEvent, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_HandleProtocol, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_LocateProtocol, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiBootServicesTableLib, gBS_CreateEventEx, 6, EFIAPI);

static EFI_BOOT_SERVICES  LocalBs = {
  { 0, 0, 0, 0, 0 },    // EFI_TABLE_HEADER
  NULL,                 // EFI_RAISE_TPL
  NULL,                 // EFI_RESTORE_TPL
  NULL,                 // EFI_ALLOCATE_PAGES
  NULL,                 // EFI_FREE_PAGES
  gBS_GetMemoryMap,     // EFI_GET_MEMORY_MAP
  NULL,                 // EFI_ALLOCATE_POOL
  NULL,                 // EFI_FREE_POOL
  gBS_CreateEvent,      // EFI_CREATE_EVENT
  NULL,                 // EFI_SET_TIMER
  NULL,                 // EFI_WAIT_FOR_EVENT
  NULL,                 // EFI_SIGNAL_EVENT
  gBS_CloseEvent,       // EFI_CLOSE_EVENT
  NULL,                 // EFI_CHECK_EVENT
  NULL,                 // EFI_INSTALL_PROTOCOL_INTERFACE
  NULL,                 // EFI_REINSTALL_PROTOCOL_INTERFACE
  NULL,                 // EFI_UNINSTALL_PROTOCOL_INTERFACE
  gBS_HandleProtocol,   // EFI_HANDLE_PROTOCOL
  NULL,                 // VOID
  NULL,                 // EFI_REGISTER_PROTOCOL_NOTIFY
  NULL,                 // EFI_LOCATE_HANDLE
  NULL,                 // EFI_LOCATE_DEVICE_PATH
  NULL,                 // EFI_INSTALL_CONFIGURATION_TABLE
  NULL,                 // EFI_IMAGE_LOAD
  NULL,                 // EFI_IMAGE_START
  NULL,                 // EFI_EXIT
  NULL,                 // EFI_IMAGE_UNLOAD
  NULL,                 // EFI_EXIT_BOOT_SERVICES
  NULL,                 // EFI_GET_NEXT_MONOTONIC_COUNT
  NULL,                 // EFI_STALL
  NULL,                 // EFI_SET_WATCHDOG_TIMER
  NULL,                 // EFI_CONNECT_CONTROLLER
  NULL,                 // EFI_DISCONNECT_CONTROLLER
  NULL,                 // EFI_OPEN_PROTOCOL
  NULL,                 // EFI_CLOSE_PROTOCOL
  NULL,                 // EFI_OPEN_PROTOCOL_INFORMATION
  NULL,                 // EFI_PROTOCOLS_PER_HANDLE
  NULL,                 // EFI_LOCATE_HANDLE_BUFFER
  gBS_LocateProtocol,   // EFI_LOCATE_PROTOCOL
  NULL,                 // EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES
  NULL,                 // EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
  NULL,                 // EFI_CALCULATE_CRC32
  NULL,                 // EFI_COPY_MEM
  NULL,                 // EFI_SET_MEM
  gBS_CreateEventEx     // EFI_CREATE_EVENT_EX
};

extern "C" {
  EFI_BOOT_SERVICES  *gBS         = &LocalBs;
  EFI_HANDLE         gImageHandle = NULL;
  EFI_SYSTEM_TABLE   *gST         = NULL;
}
