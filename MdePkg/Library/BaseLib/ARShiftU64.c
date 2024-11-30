/** @file
  Math worker functions.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits are filled
  with the original integer's bit 63. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to bit 63 of Operand.  The shifted value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand >> Count

**/
UINT64
EFIAPI
ARShiftU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  ASSERT (Count < 64);
  return InternalMathARShiftU64 (Operand, Count);
}
