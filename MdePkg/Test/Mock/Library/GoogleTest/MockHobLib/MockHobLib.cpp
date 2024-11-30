/** @file
  Google Test mocks for HobLib

  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockHobLib.h>

MOCK_INTERFACE_DEFINITION (MockHobLib);

MOCK_FUNCTION_DEFINITION (MockHobLib, GetHobList, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, GetNextHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, GetFirstHob, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, GetNextGuidHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, GetFirstGuidHob, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, GetBootModeHob, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildModuleHob, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildResourceDescriptorWithOwnerHob, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildResourceDescriptorHob, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildGuidHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildGuidDataHob, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildFvHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildFv2Hob, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildFv3Hob, 6, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildCvHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildCpuHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildStackHob, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildBspStoreHob, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHobLib, BuildMemoryAllocationHob, 3, EFIAPI);
