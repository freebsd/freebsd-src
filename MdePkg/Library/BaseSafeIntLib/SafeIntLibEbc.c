/** @file
  This library provides helper functions to prevent integer overflow during
  type conversion, addition, subtraction, and multiplication.

  Copyright (c) 2017, Microsoft Corporation

  All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/SafeIntLib.h>

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
  )
{
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt32ToUint32 (Operand, (UINT32 *)Result);
  }
  return SafeInt32ToUint64 (Operand, (UINT64 *) Result);
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeUint32ToInt32 (Operand, (INT32 *)Result);
  }
  *Result = Operand;
  return RETURN_SUCCESS;
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    *Result = (INT32)Operand;
    return RETURN_SUCCESS;
  }
  return SafeInt64ToInt32 ((INT64) Operand, Result);
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    if (Operand >= 0) {
      *Result = (UINT32)Operand;
      Status = RETURN_SUCCESS;
    } else {
      *Result = UINT32_ERROR;
      Status = RETURN_BUFFER_TOO_SMALL;
    }

    return Status;
  }
  return SafeInt64ToUint32 ((INT64)Operand, Result);
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    *Result = (UINT32)Operand;
    return RETURN_SUCCESS;
  }
  return SafeUint64ToUint32 ((UINT64)Operand, Result);
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    *Result = (INT64)Operand;
    return RETURN_SUCCESS;
  }
  return SafeUint64ToInt64 ((UINT64)Operand, Result);
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt64ToInt32 (Operand, (INT32 *)Result);
  }
  *Result = (INTN)Operand;
  return RETURN_SUCCESS;
}

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
  )
{
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt64ToUint32 (Operand, (UINT32 *)Result);
  }
  return SafeInt64ToUint64 (Operand, (UINT64 *)Result);
}

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
  )
{
  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeUint64ToUint32 ((UINT64) Operand, (UINT32 *)Result);
  }
  *Result = Operand;
  return RETURN_SUCCESS;
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    if ((UINT32)(Augend + Addend) >= Augend) {
      *Result = (Augend + Addend);
      Status = RETURN_SUCCESS;
    } else {
      *Result = UINTN_ERROR;
      Status = RETURN_BUFFER_TOO_SMALL;
    }

    return Status;
  }
  return SafeUint64Add ((UINT64)Augend, (UINT64)Addend, (UINT64 *)Result);
}

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
  )
{
  RETURN_STATUS  Status;

  if (Result == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    if (Minuend >= Subtrahend) {
      *Result = (Minuend - Subtrahend);
      Status = RETURN_SUCCESS;
    } else {
      *Result = UINTN_ERROR;
      Status = RETURN_BUFFER_TOO_SMALL;
    }

    return Status;
  }
  return SafeUint64Sub ((UINT64)Minuend, (UINT64)Subtrahend, (UINT64 *)Result);
}

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
  )
{
  UINT64  IntermediateResult;

  if (sizeof (UINTN) == sizeof (UINT32)) {
    IntermediateResult = ((UINT64) Multiplicand) *((UINT64) Multiplier);

    return SafeUint64ToUintn (IntermediateResult, Result);
  }
  return SafeUint64Mult ((UINT64)Multiplicand, (UINT64)Multiplier, (UINT64 *)Result);
}

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
  )
{
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt64ToIntn (((INT64)Augend) + ((INT64)Addend), Result);
  }
  return SafeInt64Add ((INT64)Augend, (INT64)Addend, (INT64 *)Result);
}

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
  )
{
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt64ToIntn (((INT64)Minuend) - ((INT64)Subtrahend), Result);
  }
  return SafeInt64Sub ((INT64)Minuend, (INT64)Subtrahend, (INT64 *)Result);
}

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
  )
{
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return SafeInt64ToIntn (((INT64)Multiplicand) *((INT64)Multiplier), Result);
  }
  return SafeInt64Mult ((INT64)Multiplicand, (INT64)Multiplier, (INT64 *)Result);
}

