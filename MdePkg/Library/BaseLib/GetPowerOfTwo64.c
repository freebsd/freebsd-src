/** @file
  Math worker functions.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Returns the value of the highest bit set in a 64-bit value. Equivalent to
  1 << log2(x).

  This function computes the value of the highest bit set in the 64-bit value
  specified by Operand. If Operand is zero, then zero is returned.

  @param  Operand The 64-bit operand to evaluate.

  @return 1 << HighBitSet64(Operand)
  @retval 0 Operand is zero.

**/
UINT64
EFIAPI
GetPowerOfTwo64 (
  IN      UINT64  Operand
  )
{
  if (Operand == 0) {
    return 0;
  }

  return LShiftU64 (1, (UINTN)HighBitSet64 (Operand));
}
