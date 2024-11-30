/** @file
  Architecture Independent Base Memory Library Implementation.

  The following BaseMemoryLib instances contain the same copy of this file:
    BaseMemoryLib
    PeiMemoryLib
    UefiMemoryLib

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "../MemLibInternals.h"

/**
  Scans a target buffer for a 16-bit value, and returns a pointer to the
  matching 16-bit value in the target buffer.

  @param  Buffer  The pointer to the target buffer to scan.
  @param  Length  The count of 16-bit value to scan. Must be non-zero.
  @param  Value   The value to search for in the target buffer.

  @return The pointer to the first occurrence, or NULL if not found.

**/
CONST VOID *
EFIAPI
InternalMemScanMem16 (
  IN      CONST VOID  *Buffer,
  IN      UINTN       Length,
  IN      UINT16      Value
  )
{
  CONST UINT16  *Pointer;

  Pointer = (CONST UINT16 *)Buffer;
  do {
    if (*Pointer == Value) {
      return Pointer;
    }

    ++Pointer;
  } while (--Length != 0);

  return NULL;
}

/**
  Scans a target buffer for a 32-bit value, and returns a pointer to the
  matching 32-bit value in the target buffer.

  @param  Buffer  The pointer to the target buffer to scan.
  @param  Length  The count of 32-bit value to scan. Must be non-zero.
  @param  Value   The value to search for in the target buffer.

  @return The pointer to the first occurrence, or NULL if not found.

**/
CONST VOID *
EFIAPI
InternalMemScanMem32 (
  IN      CONST VOID  *Buffer,
  IN      UINTN       Length,
  IN      UINT32      Value
  )
{
  CONST UINT32  *Pointer;

  Pointer = (CONST UINT32 *)Buffer;
  do {
    if (*Pointer == Value) {
      return Pointer;
    }

    ++Pointer;
  } while (--Length != 0);

  return NULL;
}

/**
  Scans a target buffer for a 64-bit value, and returns a pointer to the
  matching 64-bit value in the target buffer.

  @param  Buffer  The pointer to the target buffer to scan.
  @param  Length  The count of 64-bit value to scan. Must be non-zero.
  @param  Value   The value to search for in the target buffer.

  @return The pointer to the first occurrence, or NULL if not found.

**/
CONST VOID *
EFIAPI
InternalMemScanMem64 (
  IN      CONST VOID  *Buffer,
  IN      UINTN       Length,
  IN      UINT64      Value
  )
{
  CONST UINT64  *Pointer;

  Pointer = (CONST UINT64 *)Buffer;
  do {
    if (*Pointer == Value) {
      return Pointer;
    }

    ++Pointer;
  } while (--Length != 0);

  return NULL;
}

/**
  Checks whether the contents of a buffer are all zeros.

  @param  Buffer  The pointer to the buffer to be checked.
  @param  Length  The size of the buffer (in bytes) to be checked.

  @retval TRUE    Contents of the buffer are all zeros.
  @retval FALSE   Contents of the buffer are not all zeros.

**/
BOOLEAN
EFIAPI
InternalMemIsZeroBuffer (
  IN CONST VOID  *Buffer,
  IN UINTN       Length
  )
{
  CONST UINT8  *BufferData;
  UINTN        Index;

  BufferData = Buffer;
  for (Index = 0; Index < Length; Index++) {
    if (BufferData[Index] != 0) {
      return FALSE;
    }
  }

  return TRUE;
}
