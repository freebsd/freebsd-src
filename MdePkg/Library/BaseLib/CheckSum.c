/** @file
  Utility functions to generate checksum based on 2's complement
  algorithm.

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BaseLibInternals.h"

/**
  Returns the sum of all elements in a buffer in unit of UINT8.
  During calculation, the carry bits are dropped.

  This function calculates the sum of all elements in a buffer
  in unit of UINT8. The carry bits in result of addition are dropped.
  The result is returned as UINT8. If Length is Zero, then Zero is
  returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT8
EFIAPI
CalculateSum8 (
  IN      CONST UINT8              *Buffer,
  IN      UINTN                     Length
  )
{
  UINT8     Sum;
  UINTN     Count;

  ASSERT (Buffer != NULL);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN) Buffer) + 1));

  for (Sum = 0, Count = 0; Count < Length; Count++) {
    Sum = (UINT8) (Sum + *(Buffer + Count));
  }
  
  return Sum;
}


/**
  Returns the two's complement checksum of all elements in a buffer
  of 8-bit values.

  This function first calculates the sum of the 8-bit values in the
  buffer specified by Buffer and Length.  The carry bits in the result
  of addition are dropped. Then, the two's complement of the sum is
  returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT8
EFIAPI
CalculateCheckSum8 (
  IN      CONST UINT8              *Buffer,
  IN      UINTN                     Length
  )
{
  UINT8     CheckSum;

  CheckSum = CalculateSum8 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT8) (0x100 - CheckSum);
}

/**
  Returns the sum of all elements in a buffer of 16-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 16-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT16
EFIAPI
CalculateSum16 (
  IN      CONST UINT16             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT16    Sum;
  UINTN     Count;
  UINTN     Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN) Buffer & 0x1) == 0);
  ASSERT ((Length & 0x1) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN) Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = (UINT16) (Sum + *(Buffer + Count));
  }
  
  return Sum;
}


/**
  Returns the two's complement checksum of all elements in a buffer of
  16-bit values.

  This function first calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT16
EFIAPI
CalculateCheckSum16 (
  IN      CONST UINT16             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT16     CheckSum;

  CheckSum = CalculateSum16 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT16) (0x10000 - CheckSum);
}


/**
  Returns the sum of all elements in a buffer of 32-bit values. During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 32-bit result is returned. If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT32
EFIAPI
CalculateSum32 (
  IN      CONST UINT32             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT32    Sum;
  UINTN     Count;
  UINTN     Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN) Buffer & 0x3) == 0);
  ASSERT ((Length & 0x3) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN) Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = Sum + *(Buffer + Count);
  }
  
  return Sum;
}


/**
  Returns the two's complement checksum of all elements in a buffer of
  32-bit values.

  This function first calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT32
EFIAPI
CalculateCheckSum32 (
  IN      CONST UINT32             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT32     CheckSum;

  CheckSum = CalculateSum32 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT32) ((UINT32)(-1) - CheckSum + 1);
}


/**
  Returns the sum of all elements in a buffer of 64-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 64-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT64
EFIAPI
CalculateSum64 (
  IN      CONST UINT64             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT64    Sum;
  UINTN     Count;
  UINTN     Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN) Buffer & 0x7) == 0);
  ASSERT ((Length & 0x7) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN) Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = Sum + *(Buffer + Count);
  }
  
  return Sum;
}


/**
  Returns the two's complement checksum of all elements in a buffer of
  64-bit values.

  This function first calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT64
EFIAPI
CalculateCheckSum64 (
  IN      CONST UINT64             *Buffer,
  IN      UINTN                     Length
  )
{
  UINT64     CheckSum;

  CheckSum = CalculateSum64 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT64) ((UINT64)(-1) - CheckSum + 1);
}


