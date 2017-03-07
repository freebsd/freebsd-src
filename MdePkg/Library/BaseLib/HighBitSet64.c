/** @file
  Math worker functions.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




#include "BaseLibInternals.h"

/**
  Returns the bit position of the highest bit set in a 64-bit value. Equivalent
  to log2(x).

  This function computes the bit position of the highest bit set in the 64-bit
  value specified by Operand. If Operand is zero, then -1 is returned.
  Otherwise, a value between 0 and 63 is returned.

  @param  Operand The 64-bit operand to evaluate.

  @retval 0..63   Position of the highest bit set in Operand if found.
  @retval -1      Operand is zero.

**/
INTN
EFIAPI
HighBitSet64 (
  IN      UINT64                    Operand
  )
{
  if (Operand == (UINT32)Operand) {
    //
    // Operand is just a 32-bit integer
    //
    return HighBitSet32 ((UINT32)Operand);
  }

  //
  // Operand is really a 64-bit integer
  //
  if (sizeof (UINTN) == sizeof (UINT32)) {
    return HighBitSet32 (((UINT32*)&Operand)[1]) + 32;
  } else {
    return HighBitSet32 ((UINT32)RShiftU64 (Operand, 32)) + 32;
  }
}
