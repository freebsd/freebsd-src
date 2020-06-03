/** @file
  Unaligned access functions of BaseLib for ARM.

  volatile was added to work around optimization issues.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Reads a 16-bit value from memory that may be unaligned.

  This function returns the 16-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 16-bit value that may be unaligned.

  @return The 16-bit value read from Buffer.

**/
UINT16
EFIAPI
ReadUnaligned16 (
  IN CONST UINT16              *Buffer
  )
{
  volatile UINT8 LowerByte;
  volatile UINT8 HigherByte;

  ASSERT (Buffer != NULL);

  LowerByte = ((UINT8*)Buffer)[0];
  HigherByte = ((UINT8*)Buffer)[1];

  return (UINT16)(LowerByte | (HigherByte << 8));
}

/**
  Writes a 16-bit value to memory that may be unaligned.

  This function writes the 16-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 16-bit value that may be unaligned.
  @param  Value   16-bit value to write to Buffer.

  @return The 16-bit value to write to Buffer.

**/
UINT16
EFIAPI
WriteUnaligned16 (
  OUT UINT16                    *Buffer,
  IN  UINT16                    Value
  )
{
  ASSERT (Buffer != NULL);

  ((volatile UINT8*)Buffer)[0] = (UINT8)Value;
  ((volatile UINT8*)Buffer)[1] = (UINT8)(Value >> 8);

  return Value;
}

/**
  Reads a 24-bit value from memory that may be unaligned.

  This function returns the 24-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 24-bit value that may be unaligned.

  @return The 24-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned24 (
  IN CONST UINT32              *Buffer
  )
{
  ASSERT (Buffer != NULL);

  return (UINT32)(
            ReadUnaligned16 ((UINT16*)Buffer) |
            (((UINT8*)Buffer)[2] << 16)
            );
}

/**
  Writes a 24-bit value to memory that may be unaligned.

  This function writes the 24-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 24-bit value that may be unaligned.
  @param  Value   24-bit value to write to Buffer.

  @return The 24-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned24 (
  OUT UINT32                    *Buffer,
  IN  UINT32                    Value
  )
{
  ASSERT (Buffer != NULL);

  WriteUnaligned16 ((UINT16*)Buffer, (UINT16)Value);
  *(UINT8*)((UINT16*)Buffer + 1) = (UINT8)(Value >> 16);
  return Value;
}

/**
  Reads a 32-bit value from memory that may be unaligned.

  This function returns the 32-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 32-bit value that may be unaligned.

  @return The 32-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned32 (
  IN CONST UINT32              *Buffer
  )
{
  UINT16  LowerBytes;
  UINT16  HigherBytes;

  ASSERT (Buffer != NULL);

  LowerBytes  = ReadUnaligned16 ((UINT16*) Buffer);
  HigherBytes = ReadUnaligned16 ((UINT16*) Buffer + 1);

  return (UINT32) (LowerBytes | (HigherBytes << 16));
}

/**
  Writes a 32-bit value to memory that may be unaligned.

  This function writes the 32-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 32-bit value that may be unaligned.
  @param  Value   32-bit value to write to Buffer.

  @return The 32-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned32 (
  OUT UINT32                    *Buffer,
  IN  UINT32                    Value
  )
{
  ASSERT (Buffer != NULL);

  WriteUnaligned16 ((UINT16*)Buffer, (UINT16)Value);
  WriteUnaligned16 ((UINT16*)Buffer + 1, (UINT16)(Value >> 16));
  return Value;
}

/**
  Reads a 64-bit value from memory that may be unaligned.

  This function returns the 64-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 64-bit value that may be unaligned.

  @return The 64-bit value read from Buffer.

**/
UINT64
EFIAPI
ReadUnaligned64 (
  IN CONST UINT64              *Buffer
  )
{
  UINT32  LowerBytes;
  UINT32  HigherBytes;

  ASSERT (Buffer != NULL);

  LowerBytes  = ReadUnaligned32 ((UINT32*) Buffer);
  HigherBytes = ReadUnaligned32 ((UINT32*) Buffer + 1);

  return (UINT64) (LowerBytes | LShiftU64 (HigherBytes, 32));
}

/**
  Writes a 64-bit value to memory that may be unaligned.

  This function writes the 64-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 64-bit value that may be unaligned.
  @param  Value   64-bit value to write to Buffer.

  @return The 64-bit value to write to Buffer.

**/
UINT64
EFIAPI
WriteUnaligned64 (
  OUT UINT64                    *Buffer,
  IN  UINT64                    Value
  )
{
  ASSERT (Buffer != NULL);

  WriteUnaligned32 ((UINT32*)Buffer, (UINT32)Value);
  WriteUnaligned32 ((UINT32*)Buffer + 1, (UINT32)RShiftU64 (Value, 32));
  return Value;
}
