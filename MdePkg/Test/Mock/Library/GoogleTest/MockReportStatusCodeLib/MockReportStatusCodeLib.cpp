/** @file MockReportStatusCodeLib.cpp
  Google Test mocks for ReportStatusCodeLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockReportStatusCodeLib.h>

MOCK_INTERFACE_DEFINITION (MockReportStatusCodeLib);
MOCK_FUNCTION_DEFINITION (MockReportStatusCodeLib, ReportProgressCodeEnabled, 0, EFIAPI);
