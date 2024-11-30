/** @file
  Unaligned access functions of BaseLib.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Reads a 16-bit value from memory that may be unaligned.

  This function returns the 16-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 16-bit value that may be unaligned.

  @return The 16-bit value read from Buffer.

**/
UINT16
EFIAPI
ReadUnaligned16 (
  IN CONST UINT16  *Buffer
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer;
}

/**
  Writes a 16-bit value to memory that may be unaligned.

  This function writes the 16-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 16-bit value that may be unaligned.
  @param  Value   16-bit value to write to Buffer.

  @return The 16-bit value to write to Buffer.

**/
UINT16
EFIAPI
WriteUnaligned16 (
  OUT UINT16  *Buffer,
  IN  UINT16  Value
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer = Value;
}

/**
  Reads a 24-bit value from memory that may be unaligned.

  This function returns the 24-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 24-bit value that may be unaligned.

  @return The 24-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned24 (
  IN CONST UINT32  *Buffer
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer & 0xffffff;
}

/**
  Writes a 24-bit value to memory that may be unaligned.

  This function writes the 24-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 24-bit value that may be unaligned.
  @param  Value   24-bit value to write to Buffer.

  @return The 24-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned24 (
  OUT UINT32  *Buffer,
  IN  UINT32  Value
  )
{
  ASSERT (Buffer != NULL);

  *Buffer = BitFieldWrite32 (*Buffer, 0, 23, Value);
  return Value;
}

/**
  Reads a 32-bit value from memory that may be unaligned.

  This function returns the 32-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 32-bit value that may be unaligned.

  @return The 32-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned32 (
  IN CONST UINT32  *Buffer
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer;
}

/**
  Writes a 32-bit value to memory that may be unaligned.

  This function writes the 32-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 32-bit value that may be unaligned.
  @param  Value   The 32-bit value to write to Buffer.

  @return The 32-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned32 (
  OUT UINT32  *Buffer,
  IN  UINT32  Value
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer = Value;
}

/**
  Reads a 64-bit value from memory that may be unaligned.

  This function returns the 64-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 64-bit value that may be unaligned.

  @return The 64-bit value read from Buffer.

**/
UINT64
EFIAPI
ReadUnaligned64 (
  IN CONST UINT64  *Buffer
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer;
}

/**
  Writes a 64-bit value to memory that may be unaligned.

  This function writes the 64-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 64-bit value that may be unaligned.
  @param  Value   The 64-bit value to write to Buffer.

  @return The 64-bit value to write to Buffer.

**/
UINT64
EFIAPI
WriteUnaligned64 (
  OUT UINT64  *Buffer,
  IN  UINT64  Value
  )
{
  ASSERT (Buffer != NULL);

  return *Buffer = Value;
}
