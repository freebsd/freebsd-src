/** @file
  Google Test mocks for FdtLib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_FDT_LIB_H_
#define MOCK_FDT_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Base.h>
  #include <Library/FdtLib.h>
}

struct MockFdtLib {
  MOCK_INTERFACE_DECLARATION (MockFdtLib);

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    Fdt16ToCpu,
    (IN UINT16 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    UINT16,
    CpuToFdt16,
    (IN UINT16 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    UINT32,
    Fdt32ToCpu,
    (IN UINT32 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    UINT32,
    CpuToFdt32,
    (IN UINT32 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    UINT64,
    Fdt64ToCpu,
    (IN UINT64 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    UINT64,
    CpuToFdt64,
    (IN UINT64 Value)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtCheckHeader,
    (IN CONST VOID  *Fdt)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtCreateEmptyTree,
    (IN VOID    *Buffer,
     IN UINT32  BufferSize)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtNextNode,
    (IN CONST VOID  *Fdt,
     IN INT32       Offset,
     IN INT32       *Depth)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtFirstSubnode,
    (IN CONST VOID  *Fdt,
     IN INT32       Offset)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtNextSubnode,
    (IN CONST VOID  *Fdt,
     IN INT32       Offset)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtSubnodeOffsetNameLen,
    (IN CONST VOID   *Fdt,
     IN INT32        ParentOffset,
     IN CONST CHAR8  *Name,
     IN INT32        NameLength)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtNodeOffsetByPropertyValue,
    (IN CONST VOID   *Fdt,
     IN INT32        StartOffset,
     IN CONST CHAR8  *PropertyName,
     IN CONST VOID   *PropertyValue,
     IN INT32        PropertyLength)
    );
  MOCK_FUNCTION_DECLARATION (
    CONST FDT_PROPERTY *,
    FdtGetProperty,
    (IN CONST VOID   *Fdt,
     IN INT32        NodeOffset,
     IN CONST CHAR8  *Name,
     IN INT32        *Length)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtFirstPropertyOffset,
    (IN CONST VOID  *Fdt,
     IN INT32       NodeOffset)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtNextPropertyOffset,
    (IN CONST VOID  *Fdt,
     IN INT32       NodeOffset)
    );
  MOCK_FUNCTION_DECLARATION (
    CONST FDT_PROPERTY *,
    FdtGetPropertyByOffset,
    (IN CONST VOID  *Fdt,
     IN INT32       Offset,
     IN INT32       *Length)
    );
  MOCK_FUNCTION_DECLARATION (
    CONST CHAR8 *,
    FdtGetString,
    (IN CONST VOID  *Fdt,
     IN INT32       StrOffset,
     IN INT32       *Length        OPTIONAL)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtAddSubnode,
    (IN VOID         *Fdt,
     IN INT32        ParentOffset,
     IN CONST CHAR8  *Name)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtSetProperty,
    (IN VOID         *Fdt,
     IN INT32        NodeOffset,
     IN CONST CHAR8  *Name,
     IN CONST VOID   *Value,
     IN UINT32       Length)
    );
  MOCK_FUNCTION_DECLARATION (
    CONST CHAR8 *,
    FdtGetName,
    (IN VOID    *Fdt,
     IN INT32   NodeOffset,
     IN INT32   *Length)
    );
  MOCK_FUNCTION_DECLARATION (
    INT32,
    FdtNodeDepth,
    (IN CONST VOID  *Fdt,
     IN INT32       NodeOffset)
    );
};

#endif
