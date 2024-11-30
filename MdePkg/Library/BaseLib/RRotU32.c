/** @file
  Math worker functions.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Rotates a 32-bit integer right between 0 and 31 bits, filling the high bits
  with the low bits that were rotated.

  This function rotates the 32-bit value Operand to the right by Count bits.
  The high Count bits are fill with the low Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 31, then ASSERT().

  @param  Operand The 32-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >> Count.

**/
UINT32
EFIAPI
RRotU32 (
  IN      UINT32  Operand,
  IN      UINTN   Count
  )
{
  ASSERT (Count < 32);
  return (Operand >> Count) | (Operand << (32 - Count));
}
