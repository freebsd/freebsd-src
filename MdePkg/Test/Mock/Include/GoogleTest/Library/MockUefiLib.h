/** @file
  Google Test mocks for UefiLib

  Copyright (c) 2022, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_UEFI_LIB_H_
#define MOCK_UEFI_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/UefiLib.h>
}

struct MockUefiLib {
  MOCK_INTERFACE_DECLARATION (MockUefiLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiGetSystemConfigurationTable,
    (IN  EFI_GUID  *TableGuid,
     OUT VOID      **Table)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_EVENT,
    EfiCreateProtocolNotifyEvent,
    (IN  EFI_GUID          *ProtocolGuid,
     IN  EFI_TPL           NotifyTpl,
     IN  EFI_EVENT_NOTIFY  NotifyFunction,
     IN  VOID              *NotifyContext OPTIONAL,
     OUT VOID              **Registration)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiNamedEventListen,
    (IN CONST EFI_GUID          *Name,
     IN       EFI_TPL           NotifyTpl,
     IN       EFI_EVENT_NOTIFY  NotifyFunction,
     IN CONST VOID              *NotifyContext OPTIONAL,
     OUT      VOID              *Registration OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiNamedEventSignal,
    (IN CONST EFI_GUID  *Name)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiEventGroupSignal,
    (IN CONST EFI_GUID  *EventGroup)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiEventEmptyFunction,
    (IN EFI_EVENT  Event,
     IN VOID       *Context)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_TPL,
    EfiGetCurrentTpl,
    ()
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_LOCK *,
    EfiInitializeLock,
    (IN OUT EFI_LOCK  *Lock,
     IN     EFI_TPL   Priority)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiAcquireLock,
    (IN EFI_LOCK  *Lock)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiAcquireLockOrFail,
    (IN EFI_LOCK  *Lock)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiReleaseLock,
    (IN EFI_LOCK  *Lock)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiTestManagedDevice,
    (IN CONST EFI_HANDLE  ControllerHandle,
     IN CONST EFI_HANDLE  DriverBindingHandle,
     IN CONST EFI_GUID    *ProtocolGuid)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiTestChildHandle,
    (IN CONST EFI_HANDLE  ControllerHandle,
     IN CONST EFI_HANDLE  ChildHandle,
     IN CONST EFI_GUID    *ProtocolGuid)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    IsLanguageSupported,
    (IN CONST CHAR8  *SupportedLanguages,
     IN CONST CHAR8  *TargetLanguage)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    LookupUnicodeString,
    (IN CONST CHAR8                     *Language,
     IN CONST CHAR8                     *SupportedLanguages,
     IN CONST EFI_UNICODE_STRING_TABLE  *UnicodeStringTable,
     OUT      CHAR16                    **UnicodeString)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    LookupUnicodeString2,
    (IN CONST CHAR8                     *Language,
     IN CONST CHAR8                     *SupportedLanguages,
     IN CONST EFI_UNICODE_STRING_TABLE  *UnicodeStringTable,
     OUT      CHAR16                    **UnicodeString,
     IN       BOOLEAN                   Iso639Language)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    AddUnicodeString,
    (IN CONST CHAR8                     *Language,
     IN CONST CHAR8                     *SupportedLanguages,
     IN OUT   EFI_UNICODE_STRING_TABLE  **UnicodeStringTable,
     IN CONST CHAR16                    *UnicodeString)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    AddUnicodeString2,
    (IN CONST CHAR8                     *Language,
     IN CONST CHAR8                     *SupportedLanguages,
     IN OUT   EFI_UNICODE_STRING_TABLE  **UnicodeStringTable,
     IN CONST CHAR16                    *UnicodeString,
     IN       BOOLEAN                   Iso639Language)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    FreeUnicodeStringTable,
    (IN EFI_UNICODE_STRING_TABLE  *UnicodeStringTable)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    GetVariable2,
    (IN CONST CHAR16    *Name,
     IN CONST EFI_GUID  *Guid,
     OUT      VOID      **Value,
     OUT      UINTN     *Size OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    GetEfiGlobalVariable2,
    (IN CONST CHAR16  *Name,
     OUT      VOID    **Value,
     OUT      UINTN   *Size OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    GetVariable3,
    (IN CONST CHAR16    *Name,
     IN CONST EFI_GUID  *Guid,
     OUT      VOID      **Value,
     OUT      UINTN     *Size OPTIONAL,
     OUT      UINT32    *Attr OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    UINTN,
    GetGlyphWidth,
    (IN CHAR16  UnicodeChar)
    );

  MOCK_FUNCTION_DECLARATION (
    UINTN,
    UnicodeStringDisplayLength,
    (IN CONST CHAR16  *String)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiSignalEventReadyToBoot,
    ()
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiSignalEventLegacyBoot,
    ()
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiCreateEventLegacyBoot,
    (OUT EFI_EVENT  *LegacyBootEvent)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiCreateEventLegacyBootEx,
    (IN  EFI_TPL           NotifyTpl,
     IN  EFI_EVENT_NOTIFY  NotifyFunction OPTIONAL,
     IN  VOID              *NotifyContext OPTIONAL,
     OUT EFI_EVENT         *LegacyBootEvent)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiCreateEventReadyToBoot,
    (OUT EFI_EVENT  *ReadyToBootEvent)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiCreateEventReadyToBootEx,
    (IN  EFI_TPL           NotifyTpl,
     IN  EFI_EVENT_NOTIFY  NotifyFunction OPTIONAL,
     IN  VOID              *NotifyContext OPTIONAL,
     OUT EFI_EVENT         *ReadyToBootEvent)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    EfiInitializeFwVolDevicepathNode,
    (IN OUT   MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvDevicePathNode,
     IN CONST EFI_GUID                           *NameGuid)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_GUID *,
    EfiGetNameGuidFromFwVolDevicePathNode,
    (IN CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvDevicePathNode)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibInstallDriverBinding,
    (IN CONST EFI_HANDLE                   ImageHandle,
     IN CONST EFI_SYSTEM_TABLE             *SystemTable,
     IN       EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
     IN       EFI_HANDLE                   DriverBindingHandle)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibUninstallDriverBinding,
    (IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibInstallAllDriverProtocols,
    (IN CONST EFI_HANDLE                         ImageHandle,
     IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
     IN       EFI_DRIVER_BINDING_PROTOCOL        *DriverBinding,
     IN       EFI_HANDLE                         DriverBindingHandle,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibUninstallAllDriverProtocols,
    (IN       EFI_DRIVER_BINDING_PROTOCOL        *DriverBinding,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibInstallDriverBindingComponentName2,
    (IN CONST EFI_HANDLE                    ImageHandle,
     IN CONST EFI_SYSTEM_TABLE              *SystemTable,
     IN       EFI_DRIVER_BINDING_PROTOCOL   *DriverBinding,
     IN       EFI_HANDLE                    DriverBindingHandle,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL   *ComponentName OPTIONAL,
     IN CONST EFI_COMPONENT_NAME2_PROTOCOL  *ComponentName2 OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibUninstallDriverBindingComponentName2,
    (IN       EFI_DRIVER_BINDING_PROTOCOL   *DriverBinding,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL   *ComponentName OPTIONAL,
     IN CONST EFI_COMPONENT_NAME2_PROTOCOL  *ComponentName2 OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibInstallAllDriverProtocols2,
    (IN CONST EFI_HANDLE                          ImageHandle,
     IN CONST EFI_SYSTEM_TABLE                    *SystemTable,
     IN       EFI_DRIVER_BINDING_PROTOCOL         *DriverBinding,
     IN       EFI_HANDLE                          DriverBindingHandle,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL         *ComponentName OPTIONAL,
     IN CONST EFI_COMPONENT_NAME2_PROTOCOL        *ComponentName2 OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL   *DriverConfiguration OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION2_PROTOCOL  *DriverConfiguration2 OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL     *DriverDiagnostics OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS2_PROTOCOL    *DriverDiagnostics2 OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLibUninstallAllDriverProtocols2,
    (IN       EFI_DRIVER_BINDING_PROTOCOL         *DriverBinding,
     IN CONST EFI_COMPONENT_NAME_PROTOCOL         *ComponentName OPTIONAL,
     IN CONST EFI_COMPONENT_NAME2_PROTOCOL        *ComponentName2 OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL   *DriverConfiguration OPTIONAL,
     IN CONST EFI_DRIVER_CONFIGURATION2_PROTOCOL  *DriverConfiguration2 OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL     *DriverDiagnostics OPTIONAL,
     IN CONST EFI_DRIVER_DIAGNOSTICS2_PROTOCOL    *DriverDiagnostics2 OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiLocateProtocolBuffer,
    (IN  EFI_GUID  *Protocol,
     OUT UINTN     *NoProtocols,
     OUT VOID      ***Buffer)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    EfiOpenFileByDevicePath,
    (IN OUT EFI_DEVICE_PATH_PROTOCOL  **FilePath,
     OUT    EFI_FILE_PROTOCOL         **File,
     IN     UINT64                    OpenMode,
     IN     UINT64                    Attributes)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_ACPI_COMMON_HEADER *,
    EfiLocateNextAcpiTable,
    (IN UINT32                  Signature,
     IN EFI_ACPI_COMMON_HEADER  *PreviousTable OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_ACPI_COMMON_HEADER *,
    EfiLocateFirstAcpiTable,
    (IN UINT32  Signature)
    );
};

#endif
