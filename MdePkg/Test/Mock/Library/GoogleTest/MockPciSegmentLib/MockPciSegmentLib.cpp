/** @file MockPciSegmentLib.cpp
  Google Test mocks for PciSegmentLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockPciSegmentLib.h>

MOCK_INTERFACE_DEFINITION (MockPciSegmentLib);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentRegisterForRuntimeAccess, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentRead8, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentWrite8, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentOr8, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAnd8, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAndThenOr8, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldRead8, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldWrite8, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldOr8, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAnd8, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAndThenOr8, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentRead16, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentWrite16, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentOr16, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAnd16, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAndThenOr16, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldRead16, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldWrite16, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldOr16, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAnd16, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAndThenOr16, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentRead32, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentWrite32, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentOr32, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAnd32, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentAndThenOr32, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldRead32, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldWrite32, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldOr32, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAnd32, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentBitFieldAndThenOr32, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentReadBuffer, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPciSegmentLib, PciSegmentWriteBuffer, 3, EFIAPI);
