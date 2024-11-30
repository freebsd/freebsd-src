/** @file MockReportStatusCodeLib.h
  Google Test mocks for ReportStatusCodeLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_REPORT_STATUS_CODE_LIB_H_
#define MOCK_REPORT_STATUS_CODE_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/ReportStatusCodeLib.h>
}

struct MockReportStatusCodeLib {
  MOCK_INTERFACE_DECLARATION (MockReportStatusCodeLib);

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    ReportProgressCodeEnabled,
    ()
    );
};

#endif //MOCK_REPORT_STATUS_CODE_LIB_H_
