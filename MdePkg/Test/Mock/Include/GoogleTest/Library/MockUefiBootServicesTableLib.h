/** @file
  Google Test mocks for UefiBootServicesTableLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_UEFI_BOOT_SERVICES_TABLE_LIB_H_
#define MOCK_UEFI_BOOT_SERVICES_TABLE_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/UefiBootServicesTableLib.h>
}

//
// Declarations to handle usage of the UefiBootServiceTableLib by creating mock
//
struct MockUefiBootServicesTableLib {
  MOCK_INTERFACE_DECLARATION (MockUefiBootServicesTableLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_GetMemoryMap,
    (IN OUT UINTN                 *MemoryMapSize,
     OUT    EFI_MEMORY_DESCRIPTOR *MemoryMap,
     OUT    UINTN                 *MapKey,
     OUT    UINTN                 *DescriptorSize,
     OUT    UINT32                *DescriptorVersion)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_CreateEvent,
    (IN  UINT32           Type,
     IN  EFI_TPL          NotifyTpl,
     IN  EFI_EVENT_NOTIFY NotifyFunction,
     IN  VOID             *NotifyContext,
     OUT EFI_EVENT        *Event)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_CloseEvent,
    (IN EFI_EVENT Event)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_HandleProtocol,
    (IN  EFI_HANDLE Handle,
     IN  EFI_GUID   *Protocol,
     OUT VOID       **Interface)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_LocateProtocol,
    (IN  EFI_GUID *Protocol,
     IN  VOID      *Registration  OPTIONAL,
     OUT VOID      **Interface)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gBS_CreateEventEx,
    (IN UINT32            Type,
     IN EFI_TPL           NotifyTpl,
     IN EFI_EVENT_NOTIFY  NotifyFunction OPTIONAL,
     IN CONST VOID        *NotifyContext OPTIONAL,
     IN CONST EFI_GUID    *EventGroup OPTIONAL,
     OUT EFI_EVENT        *Event)
    );
};

#endif // MOCK_UEFI_BOOT_SERVICES_TABLE_LIB_H_
