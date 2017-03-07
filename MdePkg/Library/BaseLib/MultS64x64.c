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
  IN      INT64                     Multiplicand,
  IN      INT64                     Multiplier
  )
{
  return (INT64)MultU64x64 ((UINT64) Multiplicand, (UINT64) Multiplier);
}
