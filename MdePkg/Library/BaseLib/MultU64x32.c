/** @file
  Math worker functions.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




#include "BaseLibInternals.h"

/**
  Multiplies a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function multiplies the 64-bit unsigned value Multiplicand by the 32-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 32-bit unsigned value.

  @return Multiplicand * Multiplier.

**/
UINT64
EFIAPI
MultU64x32 (
  IN      UINT64                    Multiplicand,
  IN      UINT32                    Multiplier
  )
{
  UINT64                            Result;

  Result = InternalMathMultU64x32 (Multiplicand, Multiplier);

  return Result;
}
