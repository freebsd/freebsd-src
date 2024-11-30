/** @file
  This library provides helper functions to prevent integer overflow during
  type conversion, addition, subtraction, and multiplication.

  Copyright (c) 2017, Microsoft Corporation

  All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/SafeIntLib.h>
#include <Library/BaseLib.h>

//
// Magnitude of MIN_INT64 as expressed by a UINT64 number.
//
#define MIN_INT64_MAGNITUDE  (((UINT64)(- (MIN_INT64 + 1))) + 1)

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINTN)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINTN_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT8) && (Operand <= MAX_INT8)) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_INT8)) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  INT16  Operand,
  OUT UINT8  *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT8)) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINTN)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINTN_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  UINT16  Operand,
  OUT UINT8   *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT8) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT16) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT8) && (Operand <= MAX_INT8)) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_INT8)) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  INT32  Operand,
  OUT UINT8  *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT8)) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT16) && (Operand <= MAX_INT16)) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT16)) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  UINT32  Operand,
  OUT UINT8   *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT8) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT16) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT16) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT32) {
    *Result = (INT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT8) && (Operand <= MAX_INT8)) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_INT8)) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  INTN   Operand,
  OUT UINT8  *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT8)) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT16) && (Operand <= MAX_INT16)) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT16)) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINTN)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINTN_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  IN  UINTN  Operand,
  OUT UINT8  *Result
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT8) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT16) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT16) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT32) {
    *Result = (INT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INTN) {
    *Result = (INTN)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INTN_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT8) && (Operand <= MAX_INT8)) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_INT8)) {
    *Result = (CHAR8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT8)) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT16) && (Operand <= MAX_INT16)) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT16)) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= MIN_INT32) && (Operand <= MAX_INT32)) {
    *Result = (INT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Operand >= 0) && (Operand <= MAX_UINT32)) {
    *Result = (UINT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand >= 0) {
    *Result = (UINT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT8) {
    *Result = (INT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = CHAR8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT8) {
    *Result = (UINT8)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT16) {
    *Result = (INT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT16) {
    *Result = (UINT16)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT32) {
    *Result = (INT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_UINT32) {
    *Result = (UINT32)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INTN) {
    *Result = (INTN)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INTN_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Operand <= MAX_INT64) {
    *Result = (INT64)Operand;
    Status  = RETURN_SUCCESS;
  } else {
    *Result = INT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (((UINT8)(Augend + Addend)) >= Augend) {
    *Result = (UINT8)(Augend + Addend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (((UINT16)(Augend + Addend)) >= Augend) {
    *Result = (UINT16)(Augend + Addend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Augend + Addend) >= Augend) {
    *Result = (Augend + Addend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Augend + Addend) >= Augend) {
    *Result = (Augend + Addend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Minuend >= Subtrahend) {
    *Result = (UINT8)(Minuend - Subtrahend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT8_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Minuend >= Subtrahend) {
    *Result = (UINT16)(Minuend - Subtrahend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT16_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Minuend >= Subtrahend) {
    *Result = (Minuend - Subtrahend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT32_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Minuend >= Subtrahend) {
    *Result = (Minuend - Subtrahend);
    Status  = RETURN_SUCCESS;
  } else {
    *Result = UINT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  }

  return Status;
}

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
  )
{
  UINT32  IntermediateResult;

  IntermediateResult = ((UINT32)Multiplicand) *((UINT32)Multiplier);

  return SafeUint32ToUint8 (IntermediateResult, Result);
}

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
  )
{
  UINT32  IntermediateResult;

  IntermediateResult = ((UINT32)Multiplicand) *((UINT32)Multiplier);

  return SafeUint32ToUint16 (IntermediateResult, Result);
}

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
  )
{
  UINT64  IntermediateResult;

  IntermediateResult = ((UINT64)Multiplicand) *((UINT64)Multiplier);

  return SafeUint64ToUint32 (IntermediateResult, Result);
}

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
  )
{
  RETURN_STATUS  Status;
  UINT32         DwordA;
  UINT32         DwordB;
  UINT32         DwordC;
  UINT32         DwordD;
  UINT64         ProductAD;
  UINT64         ProductBC;
  UINT64         ProductBD;
  UINT64         UnsignedResult;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  ProductAD      = 0;
  ProductBC      = 0;
  ProductBD      = 0;
  UnsignedResult = 0;
  Status         = RETURN_BUFFER_TOO_SMALL;

  //
  // 64x64 into 128 is like 32.32 x 32.32.
  //
  // a.b * c.d = a*(c.d) + .b*(c.d) = a*c + a*.d + .b*c + .b*.d
  // back in non-decimal notation where A=a*2^32 and C=c*2^32:
  // A*C + A*d + b*C + b*d
  // So there are four components to add together.
  //   result = (a*c*2^64) + (a*d*2^32) + (b*c*2^32) + (b*d)
  //
  // a * c must be 0 or there would be bits in the high 64-bits
  // a * d must be less than 2^32 or there would be bits in the high 64-bits
  // b * c must be less than 2^32 or there would be bits in the high 64-bits
  // then there must be no overflow of the resulting values summed up.
  //
  DwordA = (UINT32)RShiftU64 (Multiplicand, 32);
  DwordC = (UINT32)RShiftU64 (Multiplier, 32);

  //
  // common case -- if high dwords are both zero, no chance for overflow
  //
  if ((DwordA == 0) && (DwordC == 0)) {
    DwordB = (UINT32)Multiplicand;
    DwordD = (UINT32)Multiplier;

    *Result = (((UINT64)DwordB) *(UINT64)DwordD);
    Status  = RETURN_SUCCESS;
  } else {
    //
    // a * c must be 0 or there would be bits set in the high 64-bits
    //
    if ((DwordA == 0) ||
        (DwordC == 0))
    {
      DwordD = (UINT32)Multiplier;

      //
      // a * d must be less than 2^32 or there would be bits set in the high 64-bits
      //
      ProductAD = MultU64x64 ((UINT64)DwordA, (UINT64)DwordD);
      if ((ProductAD & 0xffffffff00000000) == 0) {
        DwordB = (UINT32)Multiplicand;

        //
        // b * c must be less than 2^32 or there would be bits set in the high 64-bits
        //
        ProductBC = MultU64x64 ((UINT64)DwordB, (UINT64)DwordC);
        if ((ProductBC & 0xffffffff00000000) == 0) {
          //
          // now sum them all up checking for overflow.
          // shifting is safe because we already checked for overflow above
          //
          if (!RETURN_ERROR (SafeUint64Add (LShiftU64 (ProductBC, 32), LShiftU64 (ProductAD, 32), &UnsignedResult))) {
            //
            // b * d
            //
            ProductBD = MultU64x64 ((UINT64)DwordB, (UINT64)DwordD);

            if (!RETURN_ERROR (SafeUint64Add (UnsignedResult, ProductBD, &UnsignedResult))) {
              *Result = UnsignedResult;
              Status  = RETURN_SUCCESS;
            }
          }
        }
      }
    }
  }

  if (RETURN_ERROR (Status)) {
    *Result = UINT64_ERROR;
  }

  return Status;
}

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
  )
{
  return SafeInt32ToInt8 (((INT32)Augend) + ((INT32)Addend), Result);
}

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
  )
{
  INT32  Augend32;
  INT32  Addend32;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  Augend32 = (INT32)Augend;
  Addend32 = (INT32)Addend;
  if ((Augend32 < 0) || (Augend32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Addend32 < 0) || (Addend32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  return SafeInt32ToChar8 (Augend32 + Addend32, Result);
}

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
  )
{
  return SafeInt32ToInt16 (((INT32)Augend) + ((INT32)Addend), Result);
}

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
  )
{
  return SafeInt64ToInt32 (((INT64)Augend) + ((INT64)Addend), Result);
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // * An Addend of zero can never cause underflow or overflow.
  //
  // * A positive Addend can only cause overflow. The overflow condition is
  //
  //     (Augend + Addend) > MAX_INT64
  //
  //   Subtracting Addend from both sides yields
  //
  //     Augend > (MAX_INT64 - Addend)
  //
  //   This condition can be coded directly in C because the RHS will neither
  //   underflow nor overflow. That is due to the starting condition:
  //
  //     0 < Addend <= MAX_INT64
  //
  //   Multiplying all three sides by (-1) yields
  //
  //     0 > (-Addend) >= (-MAX_INT64)
  //
  //   Adding MAX_INT64 to all three sides yields
  //
  //     MAX_INT64 > (MAX_INT64 - Addend) >= 0
  //
  // * A negative Addend can only cause underflow. The underflow condition is
  //
  //     (Augend + Addend) < MIN_INT64
  //
  //   Subtracting Addend from both sides yields
  //
  //     Augend < (MIN_INT64 - Addend)
  //
  //   This condition can be coded directly in C because the RHS will neither
  //   underflow nor overflow. That is due to the starting condition:
  //
  //     MIN_INT64 <= Addend < 0
  //
  //   Multiplying all three sides by (-1) yields
  //
  //     (-MIN_INT64) >= (-Addend) > 0
  //
  //   Adding MIN_INT64 to all three sides yields
  //
  //     0 >= (MIN_INT64 - Addend) > MIN_INT64
  //
  if (((Addend > 0) && (Augend > (MAX_INT64 - Addend))) ||
      ((Addend < 0) && (Augend < (MIN_INT64 - Addend))))
  {
    *Result = INT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  } else {
    *Result = Augend + Addend;
    Status  = RETURN_SUCCESS;
  }

  return Status;
}

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
  )
{
  return SafeInt32ToInt8 (((INT32)Minuend) - ((INT32)Subtrahend), Result);
}

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
  )
{
  INT32  Minuend32;
  INT32  Subtrahend32;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  Minuend32    = (INT32)Minuend;
  Subtrahend32 = (INT32)Subtrahend;
  if ((Minuend32 < 0) || (Minuend32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Subtrahend32 < 0) || (Subtrahend32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  return SafeInt32ToChar8 (Minuend32 - Subtrahend32, Result);
}

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
  )
{
  return SafeInt32ToInt16 (((INT32)Minuend) - ((INT32)Subtrahend), Result);
}

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
  )
{
  return SafeInt64ToInt32 (((INT64)Minuend) - ((INT64)Subtrahend), Result);
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // * A Subtrahend of zero can never cause underflow or overflow.
  //
  // * A positive Subtrahend can only cause underflow. The underflow condition
  //   is:
  //
  //     (Minuend - Subtrahend) < MIN_INT64
  //
  //   Adding Subtrahend to both sides yields
  //
  //     Minuend < (MIN_INT64 + Subtrahend)
  //
  //   This condition can be coded directly in C because the RHS will neither
  //   underflow nor overflow. That is due to the starting condition:
  //
  //     0 < Subtrahend <= MAX_INT64
  //
  //   Adding MIN_INT64 to all three sides yields
  //
  //     MIN_INT64 < (MIN_INT64 + Subtrahend) <= (MIN_INT64 + MAX_INT64) = -1
  //
  // * A negative Subtrahend can only cause overflow. The overflow condition is
  //
  //     (Minuend - Subtrahend) > MAX_INT64
  //
  //   Adding Subtrahend to both sides yields
  //
  //     Minuend > (MAX_INT64 + Subtrahend)
  //
  //   This condition can be coded directly in C because the RHS will neither
  //   underflow nor overflow. That is due to the starting condition:
  //
  //     MIN_INT64 <= Subtrahend < 0
  //
  //   Adding MAX_INT64 to all three sides yields
  //
  //     -1 = (MAX_INT64 + MIN_INT64) <= (MAX_INT64 + Subtrahend) < MAX_INT64
  //
  if (((Subtrahend > 0) && (Minuend < (MIN_INT64 + Subtrahend))) ||
      ((Subtrahend < 0) && (Minuend > (MAX_INT64 + Subtrahend))))
  {
    *Result = INT64_ERROR;
    Status  = RETURN_BUFFER_TOO_SMALL;
  } else {
    *Result = Minuend - Subtrahend;
    Status  = RETURN_SUCCESS;
  }

  return Status;
}

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
  )
{
  return SafeInt32ToInt8 (((INT32)Multiplier) *((INT32)Multiplicand), Result);
}

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
  )
{
  INT32  Multiplicand32;
  INT32  Multiplier32;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  Multiplicand32 = (INT32)Multiplicand;
  Multiplier32   = (INT32)Multiplier;
  if ((Multiplicand32 < 0) || (Multiplicand32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  if ((Multiplier32 < 0) || (Multiplier32 > MAX_INT8)) {
    *Result = CHAR8_ERROR;
    return RETURN_BUFFER_TOO_SMALL;
  }

  return SafeInt32ToChar8 (Multiplicand32 * Multiplier32, Result);
}

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
  )
{
  return SafeInt32ToInt16 (((INT32)Multiplicand) *((INT32)Multiplier), Result);
}

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
  )
{
  return SafeInt64ToInt32 (MultS64x64 (Multiplicand, Multiplier), Result);
}

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
  )
{
  RETURN_STATUS  Status;
  UINT64         UnsignedMultiplicand;
  UINT64         UnsignedMultiplier;
  UINT64         UnsignedResult;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // Split into sign and magnitude, do unsigned operation, apply sign.
  //
  if (Multiplicand < 0) {
    //
    // Avoid negating the most negative number.
    //
    UnsignedMultiplicand = ((UINT64)(-(Multiplicand + 1))) + 1;
  } else {
    UnsignedMultiplicand = (UINT64)Multiplicand;
  }

  if (Multiplier < 0) {
    //
    // Avoid negating the most negative number.
    //
    UnsignedMultiplier = ((UINT64)(-(Multiplier + 1))) + 1;
  } else {
    UnsignedMultiplier = (UINT64)Multiplier;
  }

  Status = SafeUint64Mult (UnsignedMultiplicand, UnsignedMultiplier, &UnsignedResult);
  if (!RETURN_ERROR (Status)) {
    if ((Multiplicand < 0) != (Multiplier < 0)) {
      if (UnsignedResult > MIN_INT64_MAGNITUDE) {
        *Result = INT64_ERROR;
        Status  = RETURN_BUFFER_TOO_SMALL;
      } else if (UnsignedResult == MIN_INT64_MAGNITUDE) {
        *Result = MIN_INT64;
      } else {
        *Result = -((INT64)UnsignedResult);
      }
    } else {
      if (UnsignedResult > MAX_INT64) {
        *Result = INT64_ERROR;
        Status  = RETURN_BUFFER_TOO_SMALL;
      } else {
        *Result = (INT64)UnsignedResult;
      }
    }
  } else {
    *Result = INT64_ERROR;
  }

  return Status;
}
