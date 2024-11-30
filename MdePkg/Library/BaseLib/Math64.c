/** @file
  Leaf math worker functions that require 64-bit arithmetic support from the
  compiler.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Shifts a 64-bit integer left between 0 and 63 bits. The low bits
  are filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the left by Count bits. The
  low Count bits are set to zero. The shifted value is returned.

  @param  Operand The 64-bit operand to shift left.
  @param  Count   The number of bits to shift left.

  @return Operand << Count.

**/
UINT64
EFIAPI
InternalMathLShiftU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  return Operand << Count;
}

/**
  Shifts a 64-bit integer right between 0 and 63 bits. This high bits
  are filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to zero. The shifted value is returned.

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand >> Count.

**/
UINT64
EFIAPI
InternalMathRShiftU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  return Operand >> Count;
}

/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits
  are filled with original integer's bit 63. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to bit 63 of Operand.  The shifted value is returned.

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand arithmetically shifted right by Count.

**/
UINT64
EFIAPI
InternalMathARShiftU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  INTN  TestValue;

  //
  // Test if this compiler supports arithmetic shift
  //
  TestValue = (INTN)((INT64)(1ULL << 63) >> 63);
  if (TestValue == -1) {
    //
    // Arithmetic shift is supported
    //
    return (UINT64)((INT64)Operand >> Count);
  }

  //
  // Arithmetic is not supported
  //
  return (Operand >> Count) |
         ((INTN)Operand < 0 ? ~((UINTN)-1 >> Count) : 0);
}

/**
  Rotates a 64-bit integer left between 0 and 63 bits, filling
  the low bits with the high bits that were rotated.

  This function rotates the 64-bit value Operand to the left by Count bits. The
  low Count bits are fill with the high Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand <<< Count.

**/
UINT64
EFIAPI
InternalMathLRotU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  return (Operand << Count) | (Operand >> (64 - Count));
}

/**
  Rotates a 64-bit integer right between 0 and 63 bits, filling
  the high bits with the high low bits that were rotated.

  This function rotates the 64-bit value Operand to the right by Count bits.
  The high Count bits are fill with the low Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >>> Count.

**/
UINT64
EFIAPI
InternalMathRRotU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  return (Operand >> Count) | (Operand << (64 - Count));
}

/**
  Switches the endianess of a 64-bit integer.

  This function swaps the bytes in a 64-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Operand A 64-bit unsigned value.

  @return The byte swapped Operand.

**/
UINT64
EFIAPI
InternalMathSwapBytes64 (
  IN      UINT64  Operand
  )
{
  UINT64  LowerBytes;
  UINT64  HigherBytes;

  LowerBytes  = (UINT64)SwapBytes32 ((UINT32)Operand);
  HigherBytes = (UINT64)SwapBytes32 ((UINT32)(Operand >> 32));

  return (LowerBytes << 32 | HigherBytes);
}

/**
  Multiplies a 64-bit unsigned integer by a 32-bit unsigned integer
  and generates a 64-bit unsigned result.

  This function multiplies the 64-bit unsigned value Multiplicand by the 32-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 32-bit unsigned value.

  @return Multiplicand * Multiplier

**/
UINT64
EFIAPI
InternalMathMultU64x32 (
  IN      UINT64  Multiplicand,
  IN      UINT32  Multiplier
  )
{
  return Multiplicand * Multiplier;
}

/**
  Multiplies a 64-bit unsigned integer by a 64-bit unsigned integer
  and generates a 64-bit unsigned result.

  This function multiplies the 64-bit unsigned value Multiplicand by the 64-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 64-bit unsigned value.

  @return Multiplicand * Multiplier.

**/
UINT64
EFIAPI
InternalMathMultU64x64 (
  IN      UINT64  Multiplicand,
  IN      UINT64  Multiplier
  )
{
  return Multiplicand * Multiplier;
}

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. This
  function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend / Divisor.

**/
UINT64
EFIAPI
InternalMathDivU64x32 (
  IN      UINT64  Dividend,
  IN      UINT32  Divisor
  )
{
  return Dividend / Divisor;
}

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 32-bit remainder. This function
  returns the 32-bit unsigned remainder.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend % Divisor.

**/
UINT32
EFIAPI
InternalMathModU64x32 (
  IN      UINT64  Dividend,
  IN      UINT32  Divisor
  )
{
  return (UINT32)(Dividend % Divisor);
}

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result and an optional 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 32-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.
  @param  Remainder A pointer to a 32-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor.

**/
UINT64
EFIAPI
InternalMathDivRemU64x32 (
  IN      UINT64  Dividend,
  IN      UINT32  Divisor,
  OUT     UINT32  *Remainder OPTIONAL
  )
{
  if (Remainder != NULL) {
    *Remainder = (UINT32)(Dividend % Divisor);
  }

  return Dividend / Divisor;
}

/**
  Divides a 64-bit unsigned integer by a 64-bit unsigned integer and
  generates a 64-bit unsigned result and an optional 64-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 64-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 64-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 64-bit unsigned value.
  @param  Remainder A pointer to a 64-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivRemU64x64 (
  IN      UINT64  Dividend,
  IN      UINT64  Divisor,
  OUT     UINT64  *Remainder OPTIONAL
  )
{
  if (Remainder != NULL) {
    *Remainder = Dividend % Divisor;
  }

  return Dividend / Divisor;
}

/**
  Divides a 64-bit signed integer by a 64-bit signed integer and
  generates a 64-bit signed result and an optional 64-bit signed remainder.

  This function divides the 64-bit signed value Dividend by the 64-bit
  signed value Divisor and generates a 64-bit signed quotient. If Remainder
  is not NULL, then the 64-bit signed remainder is returned in Remainder.
  This function returns the 64-bit signed quotient.

  @param  Dividend  A 64-bit signed value.
  @param  Divisor   A 64-bit signed value.
  @param  Remainder A pointer to a 64-bit signed value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor.

**/
INT64
EFIAPI
InternalMathDivRemS64x64 (
  IN      INT64  Dividend,
  IN      INT64  Divisor,
  OUT     INT64  *Remainder  OPTIONAL
  )
{
  if (Remainder != NULL) {
    *Remainder = Dividend % Divisor;
  }

  return Dividend / Divisor;
}
