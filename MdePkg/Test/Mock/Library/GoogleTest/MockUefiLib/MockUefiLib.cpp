/** @file
  Google Test mocks for UefiLib

  Copyright (c) 2022, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockUefiLib.h>

MOCK_INTERFACE_DEFINITION (MockUefiLib);

MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiGetSystemConfigurationTable, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiCreateProtocolNotifyEvent, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiNamedEventListen, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiNamedEventSignal, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiEventGroupSignal, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiEventEmptyFunction, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiGetCurrentTpl, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiInitializeLock, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiAcquireLock, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiAcquireLockOrFail, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiReleaseLock, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiTestManagedDevice, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiTestChildHandle, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, IsLanguageSupported, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, LookupUnicodeString, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, LookupUnicodeString2, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, AddUnicodeString, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, AddUnicodeString2, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, FreeUnicodeStringTable, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, GetVariable2, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, GetEfiGlobalVariable2, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, GetVariable3, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, GetGlyphWidth, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, UnicodeStringDisplayLength, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiSignalEventReadyToBoot, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiSignalEventLegacyBoot, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiCreateEventLegacyBoot, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiCreateEventLegacyBootEx, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiCreateEventReadyToBoot, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiCreateEventReadyToBootEx, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiInitializeFwVolDevicepathNode, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiGetNameGuidFromFwVolDevicePathNode, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibInstallDriverBinding, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibUninstallDriverBinding, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibInstallAllDriverProtocols, 7, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibUninstallAllDriverProtocols, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibInstallDriverBindingComponentName2, 6, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibUninstallDriverBindingComponentName2, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibInstallAllDriverProtocols2, 10, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLibUninstallAllDriverProtocols2, 7, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLocateProtocolBuffer, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiOpenFileByDevicePath, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLocateNextAcpiTable, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockUefiLib, EfiLocateFirstAcpiTable, 1, EFIAPI);
