/** @file MockCpuLib.cpp
  Google Test mocks for BaseLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockCpuLib.h>

MOCK_INTERFACE_DEFINITION (MockCpuLib);

MOCK_FUNCTION_DEFINITION (MockCpuLib, CpuSleep, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockCpuLib, CpuFlushTlb, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockCpuLib, InitializeFloatingPointUnits, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockCpuLib, StandardSignatureIsAuthenticAMD, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockCpuLib, GetCpuFamilyModel, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockCpuLib, GetCpuSteppingId, 0, EFIAPI);
