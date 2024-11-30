/** @file
  Bit field functions of BaseLib.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Worker function that returns a bit field from Operand.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.

  @return The bit field read.

**/
UINTN
EFIAPI
InternalBaseLibBitFieldReadUint (
  IN      UINTN  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  //
  // ~((UINTN)-2 << EndBit) is a mask in which bit[0] thru bit[EndBit]
  // are 1's while bit[EndBit + 1] thru the most significant bit are 0's.
  //
  return (Operand & ~((UINTN)-2 << EndBit)) >> StartBit;
}

/**
  Worker function that reads a bit field from Operand, performs a bitwise OR,
  and returns the result.

  Performs a bitwise OR between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new value is returned.

  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
  @param  OrData    The value to OR with the read value from the value.

  @return The new value.

**/
UINTN
EFIAPI
InternalBaseLibBitFieldOrUint (
  IN      UINTN  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINTN  OrData
  )
{
  //
  // Higher bits in OrData those are not used must be zero.
  //
  // EndBit - StartBit + 1 might be 32 while the result right shifting 32 on a 32bit integer is undefined,
  // So the logic is updated to right shift (EndBit - StartBit) bits and compare the last bit directly.
  //
  ASSERT ((OrData >> (EndBit - StartBit)) == ((OrData >> (EndBit - StartBit)) & 1));

  //
  // ~((UINTN)-2 << EndBit) is a mask in which bit[0] thru bit[EndBit]
  // are 1's while bit[EndBit + 1] thru the most significant bit are 0's.
  //
  return Operand | ((OrData << StartBit) & ~((UINTN)-2 << EndBit));
}

/**
  Worker function that reads a bit field from Operand, performs a bitwise AND,
  and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new value is returned.

  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
  @param  AndData    The value to And with the read value from the value.

  @return The new value.

**/
UINTN
EFIAPI
InternalBaseLibBitFieldAndUint (
  IN      UINTN  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINTN  AndData
  )
{
  //
  // Higher bits in AndData those are not used must be zero.
  //
  // EndBit - StartBit + 1 might be 32 while the result right shifting 32 on a 32bit integer is undefined,
  // So the logic is updated to right shift (EndBit - StartBit) bits and compare the last bit directly.
  //
  ASSERT ((AndData >> (EndBit - StartBit)) == ((AndData >> (EndBit - StartBit)) & 1));

  //
  // ~((UINTN)-2 << EndBit) is a mask in which bit[0] thru bit[EndBit]
  // are 1's while bit[EndBit + 1] thru the most significant bit are 0's.
  //
  return Operand & ~((~AndData << StartBit) & ~((UINTN)-2 << EndBit));
}

/**
  Returns a bit field from an 8-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @return The bit field read.

**/
UINT8
EFIAPI
BitFieldRead8 (
  IN      UINT8  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  ASSERT (EndBit < 8);
  ASSERT (StartBit <= EndBit);
  return (UINT8)InternalBaseLibBitFieldReadUint (Operand, StartBit, EndBit);
}

/**
  Writes a bit field to an 8-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 8-bit value is
  returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     The new value of the bit field.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldWrite8 (
  IN      UINT8  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  Value
  )
{
  ASSERT (EndBit < 8);
  ASSERT (StartBit <= EndBit);
  return BitFieldAndThenOr8 (Operand, StartBit, EndBit, 0, Value);
}

/**
  Reads a bit field from an 8-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with the read value from the value.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldOr8 (
  IN      UINT8  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  OrData
  )
{
  ASSERT (EndBit < 8);
  ASSERT (StartBit <= EndBit);
  return (UINT8)InternalBaseLibBitFieldOrUint (Operand, StartBit, EndBit, OrData);
}

/**
  Reads a bit field from an 8-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the value.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldAnd8 (
  IN      UINT8  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData
  )
{
  ASSERT (EndBit < 8);
  ASSERT (StartBit <= EndBit);
  return (UINT8)InternalBaseLibBitFieldAndUint (Operand, StartBit, EndBit, AndData);
}

/**
  Reads a bit field from an 8-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldAndThenOr8 (
  IN      UINT8  Operand,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  ASSERT (EndBit < 8);
  ASSERT (StartBit <= EndBit);
  return BitFieldOr8 (
           BitFieldAnd8 (Operand, StartBit, EndBit, AndData),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Returns a bit field from a 16-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @return The bit field read.

**/
UINT16
EFIAPI
BitFieldRead16 (
  IN      UINT16  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit
  )
{
  ASSERT (EndBit < 16);
  ASSERT (StartBit <= EndBit);
  return (UINT16)InternalBaseLibBitFieldReadUint (Operand, StartBit, EndBit);
}

/**
  Writes a bit field to a 16-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 16-bit value is
  returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     The new value of the bit field.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldWrite16 (
  IN      UINT16  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  Value
  )
{
  ASSERT (EndBit < 16);
  ASSERT (StartBit <= EndBit);
  return BitFieldAndThenOr16 (Operand, StartBit, EndBit, 0, Value);
}

/**
  Reads a bit field from a 16-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with the read value from the value.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldOr16 (
  IN      UINT16  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  OrData
  )
{
  ASSERT (EndBit < 16);
  ASSERT (StartBit <= EndBit);
  return (UINT16)InternalBaseLibBitFieldOrUint (Operand, StartBit, EndBit, OrData);
}

/**
  Reads a bit field from a 16-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the value.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldAnd16 (
  IN      UINT16  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData
  )
{
  ASSERT (EndBit < 16);
  ASSERT (StartBit <= EndBit);
  return (UINT16)InternalBaseLibBitFieldAndUint (Operand, StartBit, EndBit, AndData);
}

/**
  Reads a bit field from a 16-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldAndThenOr16 (
  IN      UINT16  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  ASSERT (EndBit < 16);
  ASSERT (StartBit <= EndBit);
  return BitFieldOr16 (
           BitFieldAnd16 (Operand, StartBit, EndBit, AndData),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Returns a bit field from a 32-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The bit field read.

**/
UINT32
EFIAPI
BitFieldRead32 (
  IN      UINT32  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit
  )
{
  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);
  return (UINT32)InternalBaseLibBitFieldReadUint (Operand, StartBit, EndBit);
}

/**
  Writes a bit field to a 32-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 32-bit value is
  returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     The new value of the bit field.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldWrite32 (
  IN      UINT32  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  Value
  )
{
  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);
  return BitFieldAndThenOr32 (Operand, StartBit, EndBit, 0, Value);
}

/**
  Reads a bit field from a 32-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the read value from the value.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldOr32 (
  IN      UINT32  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  OrData
  )
{
  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);
  return (UINT32)InternalBaseLibBitFieldOrUint (Operand, StartBit, EndBit, OrData);
}

/**
  Reads a bit field from a 32-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the value.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldAnd32 (
  IN      UINT32  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData
  )
{
  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);
  return (UINT32)InternalBaseLibBitFieldAndUint (Operand, StartBit, EndBit, AndData);
}

/**
  Reads a bit field from a 32-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldAndThenOr32 (
  IN      UINT32  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);
  return BitFieldOr32 (
           BitFieldAnd32 (Operand, StartBit, EndBit, AndData),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Returns a bit field from a 64-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.

  @return The bit field read.

**/
UINT64
EFIAPI
BitFieldRead64 (
  IN      UINT64  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit
  )
{
  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);
  return RShiftU64 (Operand & ~LShiftU64 ((UINT64)-2, EndBit), StartBit);
}

/**
  Writes a bit field to a 64-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 64-bit value is
  returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  Value     The new value of the bit field.

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldWrite64 (
  IN      UINT64  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  Value
  )
{
  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);
  return BitFieldAndThenOr64 (Operand, StartBit, EndBit, 0, Value);
}

/**
  Reads a bit field from a 64-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  OrData    The value to OR with the read value from the value

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldOr64 (
  IN      UINT64  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  OrData
  )
{
  UINT64  Value1;
  UINT64  Value2;

  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);
  //
  // Higher bits in OrData those are not used must be zero.
  //
  // EndBit - StartBit + 1 might be 64 while the result right shifting 64 on RShiftU64() API is invalid,
  // So the logic is updated to right shift (EndBit - StartBit) bits and compare the last bit directly.
  //
  ASSERT (RShiftU64 (OrData, EndBit - StartBit) == (RShiftU64 (OrData, EndBit - StartBit) & 1));

  Value1 = LShiftU64 (OrData, StartBit);
  Value2 = LShiftU64 ((UINT64)-2, EndBit);

  return Operand | (Value1 & ~Value2);
}

/**
  Reads a bit field from a 64-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the value.

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldAnd64 (
  IN      UINT64  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData
  )
{
  UINT64  Value1;
  UINT64  Value2;

  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);
  //
  // Higher bits in AndData those are not used must be zero.
  //
  // EndBit - StartBit + 1 might be 64 while the right shifting 64 on RShiftU64() API is invalid,
  // So the logic is updated to right shift (EndBit - StartBit) bits and compare the last bit directly.
  //
  ASSERT (RShiftU64 (AndData, EndBit - StartBit) == (RShiftU64 (AndData, EndBit - StartBit) & 1));

  Value1 = LShiftU64 (~AndData, StartBit);
  Value2 = LShiftU64 ((UINT64)-2, EndBit);

  return Operand & ~(Value1 & ~Value2);
}

/**
  Reads a bit field from a 64-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldAndThenOr64 (
  IN      UINT64  Operand,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);
  return BitFieldOr64 (
           BitFieldAnd64 (Operand, StartBit, EndBit, AndData),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Reads a bit field from a 32-bit value, counts and returns
  the number of set bits.

  Counts the number of set bits in the  bit field specified by
  StartBit and EndBit in Operand. The count is returned.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The number of bits set between StartBit and EndBit.

**/
UINT8
EFIAPI
BitFieldCountOnes32 (
  IN       UINT32  Operand,
  IN       UINTN   StartBit,
  IN       UINTN   EndBit
  )
{
  UINT32  Count;

  ASSERT (EndBit < 32);
  ASSERT (StartBit <= EndBit);

  Count  = BitFieldRead32 (Operand, StartBit, EndBit);
  Count -= ((Count >> 1) & 0x55555555);
  Count  = (Count & 0x33333333) + ((Count >> 2) & 0x33333333);
  Count += Count >> 4;
  Count &= 0x0F0F0F0F;
  Count += Count >> 8;
  Count += Count >> 16;

  return (UINT8)Count & 0x3F;
}

/**
   Reads a bit field from a 64-bit value, counts and returns
   the number of set bits.

   Counts the number of set bits in the  bit field specified by
   StartBit and EndBit in Operand. The count is returned.

   If StartBit is greater than 63, then ASSERT().
   If EndBit is greater than 63, then ASSERT().
   If EndBit is less than StartBit, then ASSERT().

   @param  Operand   Operand on which to perform the bitfield operation.
   @param  StartBit  The ordinal of the least significant bit in the bit field.
   Range 0..63.
   @param  EndBit    The ordinal of the most significant bit in the bit field.
   Range 0..63.

   @return The number of bits set between StartBit and EndBit.

**/
UINT8
EFIAPI
BitFieldCountOnes64 (
  IN       UINT64  Operand,
  IN       UINTN   StartBit,
  IN       UINTN   EndBit
  )
{
  UINT64  BitField;
  UINT8   Count;

  ASSERT (EndBit < 64);
  ASSERT (StartBit <= EndBit);

  BitField = BitFieldRead64 (Operand, StartBit, EndBit);
  Count    = BitFieldCountOnes32 ((UINT32)BitField, 0, 31);
  Count   += BitFieldCountOnes32 ((UINT32)RShiftU64 (BitField, 32), 0, 31);

  return Count;
}
