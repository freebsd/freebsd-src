/** @file MockPostCodeLib.h
  Google Test mocks for PostCodeLib

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_POST_CODE_LIB_H_
#define MOCK_POST_CODE_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/PostCodeLib.h>
}

struct MockPostCodeLib {
  MOCK_INTERFACE_DECLARATION (MockPostCodeLib);

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PostCode,
    (
     IN UINT32  Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PostCodeWithDescription,
    (
     IN UINT32       Value,
     IN CONST CHAR8  *Description  OPTIONAL
    )
    );

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    PostCodeEnabled,
    (

    )
    );

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    PostCodeDescriptionEnabled,
    (

    )
    );
};

#endif //MOCK_PCI_EXPRESS_LIB_H_
