/** @file
  Google Test mocks for the SafeInt Library

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_SAFE_INT_LIB_H_
#define MOCK_SAFE_INT_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/SafeIntLib.h>
}

struct MockSafeIntLib {
  MOCK_INTERFACE_DECLARATION (MockSafeIntLib);

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToUint8,
    (
     IN  INT8   Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToChar8,
    (
     IN  INT8   Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToUint16,
    (
     IN  INT8    Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToUint32,
    (
     IN  INT8    Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToUintn,
    (
     IN  INT8   Operand,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8ToUint64,
    (
     IN  INT8    Operand,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint8ToInt8,
    (
     IN  UINT8  Operand,
     OUT INT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint8ToChar8,
    (
     IN  UINT8  Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToInt8,
    (
     IN  INT16  Operand,
     OUT INT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToChar8,
    (
     IN  INT16  Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToUint8,
    (
     IN INT16   Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToUint16,
    (
     IN  INT16   Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToUint32,
    (
     IN  INT16   Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToUintn,
    (
     IN  INT16  Operand,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16ToUint64,
    (
     IN  INT16   Operand,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16ToInt8,
    (
     IN  UINT16  Operand,
     OUT INT8    *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16ToChar8,
    (
     IN  UINT16  Operand,
     OUT CHAR8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16ToUint8,
    (
     IN UINT16  Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16ToInt16,
    (
     IN  UINT16  Operand,
     OUT INT16   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToInt8,
    (
     IN  INT32  Operand,
     OUT INT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToChar8,
    (
     IN  INT32  Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToUint8,
    (
     IN INT32   Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToInt16,
    (
     IN  INT32  Operand,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToUint16,
    (
     IN  INT32   Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToUint32,
    (
     IN  INT32   Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToUintn,
    (
     IN  INT32  Operand,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32ToUint64,
    (
     IN  INT32   Operand,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToInt8,
    (
     IN  UINT32  Operand,
     OUT INT8    *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToChar8,
    (
     IN  UINT32  Operand,
     OUT CHAR8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToUint8,
    (
     IN UINT32  Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToInt16,
    (
     IN  UINT32  Operand,
     OUT INT16   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToUint16,
    (
     IN  UINT32  Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToInt32,
    (
     IN  UINT32  Operand,
     OUT INT32   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32ToIntn,
    (
     IN  UINT32  Operand,
     OUT INTN    *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToInt8,
    (
     IN  INTN  Operand,
     OUT INT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToChar8,
    (
     IN  INTN   Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToUint8,
    (
     IN INTN    Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToInt16,
    (
     IN  INTN   Operand,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToUint16,
    (
     IN  INTN    Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToInt32,
    (
     IN  INTN   Operand,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToUint32,
    (
     IN  INTN    Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToUintn,
    (
     IN  INTN   Operand,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnToUint64,
    (
     IN  INTN    Operand,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToInt8,
    (
     IN  UINTN  Operand,
     OUT INT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToChar8,
    (
     IN  UINTN  Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToUint8,
    (
     IN UINTN   Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToInt16,
    (
     IN  UINTN  Operand,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToUint16,
    (
     IN  UINTN   Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToInt32,
    (
     IN  UINTN  Operand,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToUint32,
    (
     IN  UINTN   Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToIntn,
    (
     IN  UINTN  Operand,
     OUT INTN   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnToInt64,
    (
     IN  UINTN  Operand,
     OUT INT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToInt8,
    (
     IN  INT64  Operand,
     OUT INT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToChar8,
    (
     IN  INT64  Operand,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToUint8,
    (
     IN  INT64  Operand,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToInt16,
    (
     IN  INT64  Operand,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToUint16,
    (
     IN  INT64   Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToInt32,
    (
     IN  INT64  Operand,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToUint32,
    (
     IN  INT64   Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToIntn,
    (
     IN  INT64  Operand,
     OUT INTN   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToUintn,
    (
     IN  INT64  Operand,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64ToUint64,
    (
     IN  INT64   Operand,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToInt8,
    (
     IN  UINT64  Operand,
     OUT INT8    *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToChar8,
    (
     IN  UINT64  Operand,
     OUT CHAR8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToUint8,
    (
     IN  UINT64  Operand,
     OUT UINT8   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToInt16,
    (
     IN  UINT64  Operand,
     OUT INT16   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToUint16,
    (
     IN  UINT64  Operand,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToInt32,
    (
     IN  UINT64  Operand,
     OUT INT32   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToUint32,
    (
     IN  UINT64  Operand,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToIntn,
    (
     IN  UINT64  Operand,
     OUT INTN    *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToUintn,
    (
     IN  UINT64  Operand,
     OUT UINTN   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64ToInt64,
    (
     IN  UINT64  Operand,
     OUT INT64   *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint8Add,
    (
     IN  UINT8  Augend,
     IN  UINT8  Addend,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16Add,
    (
     IN  UINT16  Augend,
     IN  UINT16  Addend,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32Add,
    (
     IN  UINT32  Augend,
     IN  UINT32  Addend,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnAdd,
    (
     IN  UINTN  Augend,
     IN  UINTN  Addend,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64Add,
    (
     IN  UINT64  Augend,
     IN  UINT64  Addend,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint8Sub,
    (
     IN  UINT8  Minuend,
     IN  UINT8  Subtrahend,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16Sub,
    (
     IN  UINT16  Minuend,
     IN  UINT16  Subtrahend,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32Sub,
    (
     IN  UINT32  Minuend,
     IN  UINT32  Subtrahend,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnSub,
    (
     IN  UINTN  Minuend,
     IN  UINTN  Subtrahend,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64Sub,
    (
     IN  UINT64  Minuend,
     IN  UINT64  Subtrahend,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint8Mult,
    (
     IN  UINT8  Multiplicand,
     IN  UINT8  Multiplier,
     OUT UINT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint16Mult,
    (
     IN  UINT16  Multiplicand,
     IN  UINT16  Multiplier,
     OUT UINT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint32Mult,
    (
     IN  UINT32  Multiplicand,
     IN  UINT32  Multiplier,
     OUT UINT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUintnMult,
    (
     IN  UINTN  Multiplicand,
     IN  UINTN  Multiplier,
     OUT UINTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeUint64Mult,
    (
     IN  UINT64  Multiplicand,
     IN  UINT64  Multiplier,
     OUT UINT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8Add,
    (
     IN  INT8  Augend,
     IN  INT8  Addend,
     OUT INT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeChar8Add,
    (
     IN  CHAR8  Augend,
     IN  CHAR8  Addend,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16Add,
    (
     IN  INT16  Augend,
     IN  INT16  Addend,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32Add,
    (
     IN  INT32  Augend,
     IN  INT32  Addend,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnAdd,
    (
     IN  INTN  Augend,
     IN  INTN  Addend,
     OUT INTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64Add,
    (
     IN  INT64  Augend,
     IN  INT64  Addend,
     OUT INT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8Sub,
    (
     IN  INT8  Minuend,
     IN  INT8  Subtrahend,
     OUT INT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeChar8Sub,
    (
     IN  CHAR8  Minuend,
     IN  CHAR8  Subtrahend,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16Sub,
    (
     IN  INT16  Minuend,
     IN  INT16  Subtrahend,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32Sub,
    (
     IN  INT32  Minuend,
     IN  INT32  Subtrahend,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnSub,
    (
     IN  INTN  Minuend,
     IN  INTN  Subtrahend,
     OUT INTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64Sub,
    (
     IN  INT64  Minuend,
     IN  INT64  Subtrahend,
     OUT INT64  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt8Mult,
    (
     IN  INT8  Multiplicand,
     IN  INT8  Multiplier,
     OUT INT8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeChar8Mult,
    (
     IN  CHAR8  Multiplicand,
     IN  CHAR8  Multiplier,
     OUT CHAR8  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt16Mult,
    (
     IN  INT16  Multiplicand,
     IN  INT16  Multiplier,
     OUT INT16  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt32Mult,
    (
     IN  INT32  Multiplicand,
     IN  INT32  Multiplier,
     OUT INT32  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeIntnMult,
    (
     IN  INTN  Multiplicand,
     IN  INTN  Multiplier,
     OUT INTN  *Result
    )
    );

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    SafeInt64Mult,
    (
     IN  INT64  Multiplicand,
     IN  INT64  Multiplier,
     OUT INT64  *Result
    )
    );
};

#endif
