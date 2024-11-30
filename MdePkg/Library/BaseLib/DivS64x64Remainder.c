/** @file
  Math worker functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Divides a 64-bit signed integer by a 64-bit signed integer and generates a
  64-bit signed result and a optional 64-bit signed remainder.

  This function divides the 64-bit signed value Dividend by the 64-bit signed
  value Divisor and generates a 64-bit signed quotient. If Remainder is not
  NULL, then the 64-bit signed remainder is returned in Remainder. This
  function returns the 64-bit signed quotient.

  It is the caller's responsibility to not call this function with a Divisor of 0.
  If Divisor is 0, then the quotient and remainder should be assumed to be
  the largest negative integer.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit signed value.
  @param  Divisor   A 64-bit signed value.
  @param  Remainder A pointer to a 64-bit signed value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
INT64
EFIAPI
DivS64x64Remainder (
  IN      INT64  Dividend,
  IN      INT64  Divisor,
  OUT     INT64  *Remainder  OPTIONAL
  )
{
  ASSERT (Divisor != 0);
  return InternalMathDivRemS64x64 (Dividend, Divisor, Remainder);
}
