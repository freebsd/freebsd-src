/** @file
  Google Test mocks for FdtLib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockFdtLib.h>

MOCK_INTERFACE_DEFINITION (MockFdtLib);

MOCK_FUNCTION_DEFINITION (MockFdtLib, Fdt16ToCpu, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, CpuToFdt16, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, Fdt32ToCpu, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, CpuToFdt32, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, Fdt64ToCpu, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, CpuToFdt64, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtCheckHeader, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtCreateEmptyTree, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtNextNode, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtFirstSubnode, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtNextSubnode, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtSubnodeOffsetNameLen, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtNodeOffsetByPropertyValue, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtGetProperty, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtFirstPropertyOffset, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtNextPropertyOffset, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtGetPropertyByOffset, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtGetString, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtAddSubnode, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtSetProperty, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtGetName, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockFdtLib, FdtNodeDepth, 2, EFIAPI);
