/** @file
  Google Test mocks for PeiServicesLib

  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockPeiServicesLib.h>

MOCK_INTERFACE_DEFINITION (MockPeiServicesLib);

MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesInstallPpi, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesReInstallPpi, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesLocatePpi, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesNotifyPpi, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesGetBootMode, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesSetBootMode, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesGetHobList, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesCreateHob, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsFindNextVolume, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsFindNextFile, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsFindSectionData, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsFindSectionData3, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesInstallPeiMemory, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesAllocatePages, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFreePages, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesAllocatePool, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesResetSystem, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsFindFileByName, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsGetFileInfo, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsGetFileInfo2, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesFfsGetVolumeInfo, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesRegisterForShadow, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesInstallFvInfoPpi, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesInstallFvInfo2Ppi, 6, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPeiServicesLib, PeiServicesResetSystem2, 4, EFIAPI);
