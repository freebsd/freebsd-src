/** @file
  SetMem() and SetMemN() implementation.

  The following BaseMemoryLib instances contain the same copy of this file:

    BaseMemoryLib
    BaseMemoryLibMmx
    BaseMemoryLibSse2
    BaseMemoryLibRepStr
    BaseMemoryLibOptDxe
    BaseMemoryLibOptPei
    PeiMemoryLib
    UefiMemoryLib

  Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "MemLibInternals.h"

/**
  Fills a target buffer with a byte value, and returns the target buffer.

  This function fills Length bytes of Buffer with Value, and returns Buffer.
  
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer    The memory to set.
  @param  Length    The number of bytes to set.
  @param  Value     The value with which to fill Length bytes of Buffer.

  @return Buffer.

**/
VOID *
EFIAPI
SetMem (
  OUT VOID  *Buffer,
  IN UINTN  Length,
  IN UINT8  Value
  )
{
  if (Length == 0) {
    return Buffer;
  }

  ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINTN)Buffer));

  return InternalMemSetMem (Buffer, Length, Value);
}

/**
  Fills a target buffer with a value that is size UINTN, and returns the target buffer.

  This function fills Length bytes of Buffer with the UINTN sized value specified by
  Value, and returns Buffer. Value is repeated every sizeof(UINTN) bytes for Length
  bytes of Buffer.

  If Length > 0 and Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().
  If Buffer is not aligned on a UINTN boundary, then ASSERT().
  If Length is not aligned on a UINTN boundary, then ASSERT().

  @param  Buffer  The pointer to the target buffer to fill.
  @param  Length  The number of bytes in Buffer to fill.
  @param  Value   The value with which to fill Length bytes of Buffer.

  @return Buffer.

**/
VOID *
EFIAPI
SetMemN (
  OUT VOID  *Buffer,
  IN UINTN  Length,
  IN UINTN  Value
  )
{
  if (sizeof (UINTN) == sizeof (UINT64)) {
    return SetMem64 (Buffer, Length, (UINT64)Value);
  } else {
    return SetMem32 (Buffer, Length, (UINT32)Value);
  }
}
