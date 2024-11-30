/** @file
  ScanMem16() implementation.

  The following BaseMemoryLib instances contain the same copy of this file:

    BaseMemoryLib
    BaseMemoryLibMmx
    BaseMemoryLibSse2
    BaseMemoryLibRepStr
    BaseMemoryLibOptDxe
    BaseMemoryLibOptPei
    PeiMemoryLib
    UefiMemoryLib

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MemLibInternals.h"

/**
  Scans a target buffer for a 16-bit value, and returns a pointer to the matching 16-bit value
  in the target buffer.

  This function searches the target buffer specified by Buffer and Length from the lowest
  address to the highest address for a 16-bit value that matches Value.  If a match is found,
  then a pointer to the matching byte in the target buffer is returned.  If no match is found,
  then NULL is returned.  If Length is 0, then NULL is returned.

  If Length > 0 and Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the target buffer to scan.
  @param  Length      The number of bytes in Buffer to scan.
  @param  Value       The value to search for in the target buffer.

  @return A pointer to the matching byte in the target buffer or NULL otherwise.

**/
VOID *
EFIAPI
ScanMem16 (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN UINT16      Value
  )
{
  if (Length == 0) {
    return NULL;
  }

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN)Buffer & (sizeof (Value) - 1)) == 0);
  ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINTN)Buffer));
  ASSERT ((Length & (sizeof (Value) - 1)) == 0);

  return (VOID *)InternalMemScanMem16 (Buffer, Length / sizeof (Value), Value);
}
