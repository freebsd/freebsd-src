/** @file
  This library provides helper functions to prevent integer overflow during
  type conversion, addition, subtraction, and multiplication.

  Copyright (c) 2017, Microsoft Corporation

  All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __INT_SAFE_LIB_H__
#define __INT_SAFE_LIB_H__

//
// It is common for -1 to be used as an error value
//
#define INT8_ERROR    ((INT8) -1)
#define UINT8_ERROR   MAX_UINT8
#define CHAR8_ERROR   ((CHAR8)(MAX_INT8))
#define INT16_ERROR   ((INT16) -1)
#define UINT16_ERROR  MAX_UINT16
#define CHAR16_ERROR  MAX_UINT16
#define INT32_ERROR   ((INT32) -1)
#define UINT32_ERROR  MAX_UINT32
#define INT64_ERROR   ((INT64) -1)
#define UINT64_ERROR  MAX_UINT64
#define INTN_ERROR    ((INTN) -1)
#define UINTN_ERROR   MAX_UINTN

//
// CHAR16 is defined to be the same as UINT16, so for CHAR16
// operations redirect to the UINT16 ones:
//
#define SafeInt8ToChar16    SafeInt8ToUint16
#define SafeInt16ToChar16   SafeInt16ToUint16
#define SafeInt32ToChar16   SafeInt32ToUint16
#define SafeUint32ToChar16  SafeUint32ToUint16
#define SafeInt64ToChar16   SafeInt64ToUint16
#define SafeUint64ToChar16  SafeUint64ToUint16
#define SafeIntnToChar16    SafeIntnToUint16
#define SafeUintnToChar16   SafeUintnToUint16

#define SafeChar16ToInt8   SafeUint16ToInt8
#define SafeChar16ToUint8  SafeUint16ToUint8
#define SafeChar16ToChar8  SafeUint16ToChar8
#define SafeChar16ToInt16  SafeUint16ToInt16

#define SafeChar16Mult  SafeUint16Mult
#define SafeChar16Sub   SafeUint16Sub
#define SafeChar16Add   SafeUint16Add

//
// Conversion functions
//
// There are three reasons for having conversion functions:
//
// 1. We are converting from a signed type to an unsigned type of the same
//    size, or vice-versa.
//
// 2. We are converting to a smaller type, and we could therefore possibly
//    overflow.
//
// 3. We are converting to a bigger type, and we are signed and the type we are
//    converting to is unsigned.
//

/**
  INT8 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToUint8 (
  IN  INT8   Operand,
  OUT UINT8  *Result
  );

/**
  INT8 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToChar8 (
  IN  INT8   Operand,
  OUT CHAR8  *Result
  );

/**
  INT8 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToUint16 (
  IN  INT8    Operand,
  OUT UINT16  *Result
  );

/**
  INT8 -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToUint32 (
  IN  INT8    Operand,
  OUT UINT32  *Result
  );

/**
  INT8 -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToUintn (
  IN  INT8   Operand,
  OUT UINTN  *Result
  );

/**
  INT8 -> UINT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8ToUint64 (
  IN  INT8    Operand,
  OUT UINT64  *Result
  );

/**
  UINT8 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint8ToInt8 (
  IN  UINT8  Operand,
  OUT INT8   *Result
  );

/**
  UINT8 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint8ToChar8 (
  IN  UINT8  Operand,
  OUT CHAR8  *Result
  );

/**
  INT16 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToInt8 (
  IN  INT16  Operand,
  OUT INT8   *Result
  );

/**
  INT16 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToChar8 (
  IN  INT16  Operand,
  OUT CHAR8  *Result
  );

/**
  INT16 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToUint8 (
  IN INT16   Operand,
  OUT UINT8  *Result
  );

/**
  INT16 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToUint16 (
  IN  INT16   Operand,
  OUT UINT16  *Result
  );

/**
  INT16 -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToUint32 (
  IN  INT16   Operand,
  OUT UINT32  *Result
  );

/**
  INT16 -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToUintn (
  IN  INT16  Operand,
  OUT UINTN  *Result
  );

/**
  INT16 -> UINT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16ToUint64 (
  IN  INT16   Operand,
  OUT UINT64  *Result
  );

/**
  UINT16 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16ToInt8 (
  IN  UINT16  Operand,
  OUT INT8    *Result
  );

/**
  UINT16 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16ToChar8 (
  IN  UINT16  Operand,
  OUT CHAR8   *Result
  );

/**
  UINT16 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16ToUint8 (
  IN UINT16  Operand,
  OUT UINT8  *Result
  );

/**
  UINT16 -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16ToInt16 (
  IN  UINT16  Operand,
  OUT INT16   *Result
  );

/**
  INT32 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToInt8 (
  IN  INT32  Operand,
  OUT INT8   *Result
  );

/**
  INT32 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToChar8 (
  IN  INT32  Operand,
  OUT CHAR8  *Result
  );

/**
  INT32 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToUint8 (
  IN INT32   Operand,
  OUT UINT8  *Result
  );

/**
  INT32 -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToInt16 (
  IN  INT32  Operand,
  OUT INT16  *Result
  );

/**
  INT32 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToUint16 (
  IN  INT32   Operand,
  OUT UINT16  *Result
  );

/**
  INT32 -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToUint32 (
  IN  INT32   Operand,
  OUT UINT32  *Result
  );

/**
  INT32 -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToUintn (
  IN  INT32  Operand,
  OUT UINTN  *Result
  );

/**
  INT32 -> UINT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32ToUint64 (
  IN  INT32   Operand,
  OUT UINT64  *Result
  );

/**
  UINT32 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToInt8 (
  IN  UINT32  Operand,
  OUT INT8    *Result
  );

/**
  UINT32 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToChar8 (
  IN  UINT32  Operand,
  OUT CHAR8   *Result
  );

/**
  UINT32 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToUint8 (
  IN UINT32  Operand,
  OUT UINT8  *Result
  );

/**
  UINT32 -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToInt16 (
  IN  UINT32  Operand,
  OUT INT16   *Result
  );

/**
  UINT32 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToUint16 (
  IN  UINT32  Operand,
  OUT UINT16  *Result
  );

/**
  UINT32 -> INT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToInt32 (
  IN  UINT32  Operand,
  OUT INT32   *Result
  );

/**
  UINT32 -> INTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32ToIntn (
  IN  UINT32  Operand,
  OUT INTN    *Result
  );

/**
  INTN -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToInt8 (
  IN  INTN  Operand,
  OUT INT8  *Result
  );

/**
  INTN -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToChar8 (
  IN  INTN   Operand,
  OUT CHAR8  *Result
  );

/**
  INTN -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToUint8 (
  IN INTN    Operand,
  OUT UINT8  *Result
  );

/**
  INTN -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToInt16 (
  IN  INTN   Operand,
  OUT INT16  *Result
  );

/**
  INTN -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToUint16 (
  IN  INTN    Operand,
  OUT UINT16  *Result
  );

/**
  INTN -> INT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToInt32 (
  IN  INTN   Operand,
  OUT INT32  *Result
  );

/**
  INTN -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToUint32 (
  IN  INTN    Operand,
  OUT UINT32  *Result
  );

/**
  INTN -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToUintn (
  IN  INTN   Operand,
  OUT UINTN  *Result
  );

/**
  INTN -> UINT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnToUint64 (
  IN  INTN    Operand,
  OUT UINT64  *Result
  );

/**
  UINTN -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToInt8 (
  IN  UINTN  Operand,
  OUT INT8   *Result
  );

/**
  UINTN -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToChar8 (
  IN  UINTN  Operand,
  OUT CHAR8  *Result
  );

/**
  UINTN -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToUint8 (
  IN UINTN   Operand,
  OUT UINT8  *Result
  );

/**
  UINTN -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToInt16 (
  IN  UINTN  Operand,
  OUT INT16  *Result
  );

/**
  UINTN -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToUint16 (
  IN  UINTN   Operand,
  OUT UINT16  *Result
  );

/**
  UINTN -> INT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToInt32 (
  IN  UINTN  Operand,
  OUT INT32  *Result
  );

/**
  UINTN -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToUint32 (
  IN  UINTN   Operand,
  OUT UINT32  *Result
  );

/**
  UINTN -> INTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToIntn (
  IN  UINTN  Operand,
  OUT INTN   *Result
  );

/**
  UINTN -> INT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnToInt64 (
  IN  UINTN  Operand,
  OUT INT64  *Result
  );

/**
  INT64 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToInt8 (
  IN  INT64  Operand,
  OUT INT8   *Result
  );

/**
  INT64 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToChar8 (
  IN  INT64  Operand,
  OUT CHAR8  *Result
  );

/**
  INT64 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToUint8 (
  IN  INT64  Operand,
  OUT UINT8  *Result
  );

/**
  INT64 -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToInt16 (
  IN  INT64  Operand,
  OUT INT16  *Result
  );

/**
  INT64 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToUint16 (
  IN  INT64   Operand,
  OUT UINT16  *Result
  );

/**
  INT64 -> INT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToInt32 (
  IN  INT64  Operand,
  OUT INT32  *Result
  );

/**
  INT64 -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToUint32 (
  IN  INT64   Operand,
  OUT UINT32  *Result
  );

/**
  INT64 -> INTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToIntn (
  IN  INT64  Operand,
  OUT INTN   *Result
  );

/**
  INT64 -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToUintn (
  IN  INT64  Operand,
  OUT UINTN  *Result
  );

/**
  INT64 -> UINT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64ToUint64 (
  IN  INT64   Operand,
  OUT UINT64  *Result
  );

/**
  UINT64 -> INT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToInt8 (
  IN  UINT64  Operand,
  OUT INT8    *Result
  );

/**
  UINT64 -> CHAR8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToChar8 (
  IN  UINT64  Operand,
  OUT CHAR8   *Result
  );

/**
  UINT64 -> UINT8 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToUint8 (
  IN  UINT64  Operand,
  OUT UINT8   *Result
  );

/**
  UINT64 -> INT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToInt16 (
  IN  UINT64  Operand,
  OUT INT16   *Result
  );

/**
  UINT64 -> UINT16 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToUint16 (
  IN  UINT64  Operand,
  OUT UINT16  *Result
  );

/**
  UINT64 -> INT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToInt32 (
  IN  UINT64  Operand,
  OUT INT32   *Result
  );

/**
  UINT64 -> UINT32 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToUint32 (
  IN  UINT64  Operand,
  OUT UINT32  *Result
  );

/**
  UINT64 -> INTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToIntn (
  IN  UINT64  Operand,
  OUT INTN    *Result
  );

/**
  UINT64 -> UINTN conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToUintn (
  IN  UINT64  Operand,
  OUT UINTN   *Result
  );

/**
  UINT64 -> INT64 conversion

  Converts the value specified by Operand to a value specified by Result type
  and stores the converted value into the caller allocated output buffer
  specified by Result.  The caller must pass in a Result buffer that is at
  least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the conversion results in an overflow or an underflow condition, then
  Result is set to INT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Operand  Operand to be converted to new type
  @param[out]  Result   Pointer to the result of conversion

  @retval  RETURN_SUCCESS            Successful conversion
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64ToInt64 (
  IN  UINT64  Operand,
  OUT INT64   *Result
  );

//
// Addition functions
//

/**
  UINT8 addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint8Add (
  IN  UINT8  Augend,
  IN  UINT8  Addend,
  OUT UINT8  *Result
  );

/**
  UINT16 addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16Add (
  IN  UINT16  Augend,
  IN  UINT16  Addend,
  OUT UINT16  *Result
  );

/**
  UINT32 addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32Add (
  IN  UINT32  Augend,
  IN  UINT32  Addend,
  OUT UINT32  *Result
  );

/**
  UINTN addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnAdd (
  IN  UINTN  Augend,
  IN  UINTN  Addend,
  OUT UINTN  *Result
  );

/**
  UINT64 addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64Add (
  IN  UINT64  Augend,
  IN  UINT64  Addend,
  OUT UINT64  *Result
  );

//
// Subtraction functions
//

/**
  UINT8 subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint8Sub (
  IN  UINT8  Minuend,
  IN  UINT8  Subtrahend,
  OUT UINT8  *Result
  );

/**
  UINT16 subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16Sub (
  IN  UINT16  Minuend,
  IN  UINT16  Subtrahend,
  OUT UINT16  *Result
  );

/**
  UINT32 subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32Sub (
  IN  UINT32  Minuend,
  IN  UINT32  Subtrahend,
  OUT UINT32  *Result
  );

/**
  UINTN subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnSub (
  IN  UINTN  Minuend,
  IN  UINTN  Subtrahend,
  OUT UINTN  *Result
  );

/**
  UINT64 subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64Sub (
  IN  UINT64  Minuend,
  IN  UINT64  Subtrahend,
  OUT UINT64  *Result
  );

//
// Multiplication functions
//

/**
  UINT8 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint8Mult (
  IN  UINT8  Multiplicand,
  IN  UINT8  Multiplier,
  OUT UINT8  *Result
  );

/**
  UINT16 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint16Mult (
  IN  UINT16  Multiplicand,
  IN  UINT16  Multiplier,
  OUT UINT16  *Result
  );

/**
  UINT32 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint32Mult (
  IN  UINT32  Multiplicand,
  IN  UINT32  Multiplier,
  OUT UINT32  *Result
  );

/**
  UINTN multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUintnMult (
  IN  UINTN  Multiplicand,
  IN  UINTN  Multiplier,
  OUT UINTN  *Result
  );

/**
  UINT64 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to UINT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeUint64Mult (
  IN  UINT64  Multiplicand,
  IN  UINT64  Multiplier,
  OUT UINT64  *Result
  );

//
// Signed operations
//
// Strongly consider using unsigned numbers.
//
// Signed numbers are often used where unsigned numbers should be used.
// For example file sizes and array indices should always be unsigned.
// Subtracting a larger positive signed number from a smaller positive
// signed number with SafeInt32Sub will succeed, producing a negative number,
// that then must not be used as an array index (but can occasionally be
// used as a pointer index.) Similarly for adding a larger magnitude
// negative number to a smaller magnitude positive number.
//
// This library does not protect you from such errors. It tells you if your
// integer operations overflowed, not if you are doing the right thing
// with your non-overflowed integers.
//
// Likewise you can overflow a buffer with a non-overflowed unsigned index.
//

//
// Signed addition functions
//

/**
  INT8 Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8Add (
  IN  INT8  Augend,
  IN  INT8  Addend,
  OUT INT8  *Result
  );

/**
  CHAR8 Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeChar8Add (
  IN  CHAR8  Augend,
  IN  CHAR8  Addend,
  OUT CHAR8  *Result
  );

/**
  INT16 Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16Add (
  IN  INT16  Augend,
  IN  INT16  Addend,
  OUT INT16  *Result
  );

/**
  INT32 Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32Add (
  IN  INT32  Augend,
  IN  INT32  Addend,
  OUT INT32  *Result
  );

/**
  INTN Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnAdd (
  IN  INTN  Augend,
  IN  INTN  Addend,
  OUT INTN  *Result
  );

/**
  INT64 Addition

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Augend  A number to which addend will be added
  @param[in]   Addend  A number to be added to another
  @param[out]  Result  Pointer to the result of addition

  @retval  RETURN_SUCCESS            Successful addition
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64Add (
  IN  INT64  Augend,
  IN  INT64  Addend,
  OUT INT64  *Result
  );

//
// Signed subtraction functions
//

/**
  INT8 Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8Sub (
  IN  INT8  Minuend,
  IN  INT8  Subtrahend,
  OUT INT8  *Result
  );

/**
  CHAR8 Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeChar8Sub (
  IN  CHAR8  Minuend,
  IN  CHAR8  Subtrahend,
  OUT CHAR8  *Result
  );

/**
  INT16 Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16Sub (
  IN  INT16  Minuend,
  IN  INT16  Subtrahend,
  OUT INT16  *Result
  );

/**
  INT32 Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32Sub (
  IN  INT32  Minuend,
  IN  INT32  Subtrahend,
  OUT INT32  *Result
  );

/**
  INTN Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnSub (
  IN  INTN  Minuend,
  IN  INTN  Subtrahend,
  OUT INTN  *Result
  );

/**
  INT64 Subtraction

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Minuend     A number from which another is to be subtracted.
  @param[in]   Subtrahend  A number to be subtracted from another
  @param[out]  Result      Pointer to the result of subtraction

  @retval  RETURN_SUCCESS            Successful subtraction
  @retval  RETURN_BUFFER_TOO_SMALL   Underflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64Sub (
  IN  INT64  Minuend,
  IN  INT64  Subtrahend,
  OUT INT64  *Result
  );

//
// Signed multiplication functions
//

/**
  INT8 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt8Mult (
  IN  INT8  Multiplicand,
  IN  INT8  Multiplier,
  OUT INT8  *Result
  );

/**
  CHAR8 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to CHAR8_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeChar8Mult (
  IN  CHAR8  Multiplicand,
  IN  CHAR8  Multiplier,
  OUT CHAR8  *Result
  );

/**
  INT16 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT16_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt16Mult (
  IN  INT16  Multiplicand,
  IN  INT16  Multiplier,
  OUT INT16  *Result
  );

/**
  INT32 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT32_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt32Mult (
  IN  INT32  Multiplicand,
  IN  INT32  Multiplier,
  OUT INT32  *Result
  );

/**
  INTN multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INTN_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeIntnMult (
  IN  INTN  Multiplicand,
  IN  INTN  Multiplier,
  OUT INTN  *Result
  );

/**
  INT64 multiplication

  Performs the requested operation using the input parameters into a value
  specified by Result type and stores the converted value into the caller
  allocated output buffer specified by Result.  The caller must pass in a
  Result buffer that is at least as large as the Result type.

  If Result is NULL, RETURN_INVALID_PARAMETER is returned.

  If the requested operation results in an overflow or an underflow condition,
  then Result is set to INT64_ERROR and RETURN_BUFFER_TOO_SMALL is returned.

  @param[in]   Multiplicand  A number that is to be multiplied by another
  @param[in]   Multiplier    A number by which the multiplicand is to be multiplied
  @param[out]  Result        Pointer to the result of multiplication

  @retval  RETURN_SUCCESS            Successful multiplication
  @retval  RETURN_BUFFER_TOO_SMALL   Overflow
  @retval  RETURN_INVALID_PARAMETER  Result is NULL
**/
RETURN_STATUS
EFIAPI
SafeInt64Mult (
  IN  INT64  Multiplicand,
  IN  INT64  Multiplier,
  OUT INT64  *Result
  );

#endif // __INT_SAFE_LIB_H__
