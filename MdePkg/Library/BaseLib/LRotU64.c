/** @file
  Math worker functions.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Rotates a 64-bit integer left between 0 and 63 bits, filling the low bits
  with the high bits that were rotated.

  This function rotates the 64-bit value Operand to the left by Count bits. The
  low Count bits are fill with the high Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand << Count

**/
UINT64
EFIAPI
LRotU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  ASSERT (Count < 64);
  return InternalMathLRotU64 (Operand, Count);
}
