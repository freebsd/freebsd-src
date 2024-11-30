/** @file MockPostCodeLib.cpp
  Google Test mocks for PostCodeLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockPostCodeLib.h>

MOCK_INTERFACE_DEFINITION (MockPostCodeLib);
MOCK_FUNCTION_DEFINITION (MockPostCodeLib, PostCode, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPostCodeLib, PostCodeWithDescription, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPostCodeLib, PostCodeEnabled, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockPostCodeLib, PostCodeDescriptionEnabled, 0, EFIAPI);
