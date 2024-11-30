/** @file
  Math worker functions.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Multiplies a 64-bit signed integer by a 64-bit signed integer and generates a
  64-bit signed result.

  This function multiplies the 64-bit signed value Multiplicand by the 64-bit
  signed value Multiplier and generates a 64-bit signed result. This 64-bit
  signed result is returned.

  @param  Multiplicand  A 64-bit signed value.
  @param  Multiplier    A 64-bit signed value.

  @return Multiplicand * Multiplier.

**/
INT64
EFIAPI
MultS64x64 (
  IN      INT64  Multiplicand,
  IN      INT64  Multiplier
  )
{
  return (INT64)MultU64x64 ((UINT64)Multiplicand, (UINT64)Multiplier);
}
