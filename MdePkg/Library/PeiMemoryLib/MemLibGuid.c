/** @file
  Implementation of GUID functions.

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
  Copies a source GUID to a destination GUID.

  This function copies the contents of the 128-bit GUID specified by SourceGuid to
  DestinationGuid, and returns DestinationGuid.

  If DestinationGuid is NULL, then ASSERT().
  If SourceGuid is NULL, then ASSERT().

  @param  DestinationGuid   The pointer to the destination GUID.
  @param  SourceGuid        The pointer to the source GUID.

  @return DestinationGuid.

**/
GUID *
EFIAPI
CopyGuid (
  OUT GUID       *DestinationGuid,
  IN CONST GUID  *SourceGuid
  )
{
  WriteUnaligned64 (
    (UINT64 *)DestinationGuid,
    ReadUnaligned64 ((CONST UINT64 *)SourceGuid)
    );
  WriteUnaligned64 (
    (UINT64 *)DestinationGuid + 1,
    ReadUnaligned64 ((CONST UINT64 *)SourceGuid + 1)
    );
  return DestinationGuid;
}

/**
  Compares two GUIDs.

  This function compares Guid1 to Guid2.  If the GUIDs are identical then TRUE is returned.
  If there are any bit differences in the two GUIDs, then FALSE is returned.

  If Guid1 is NULL, then ASSERT().
  If Guid2 is NULL, then ASSERT().

  @param  Guid1       A pointer to a 128 bit GUID.
  @param  Guid2       A pointer to a 128 bit GUID.

  @retval TRUE        Guid1 and Guid2 are identical.
  @retval FALSE       Guid1 and Guid2 are not identical.

**/
BOOLEAN
EFIAPI
CompareGuid (
  IN CONST GUID  *Guid1,
  IN CONST GUID  *Guid2
  )
{
  UINT64  LowPartOfGuid1;
  UINT64  LowPartOfGuid2;
  UINT64  HighPartOfGuid1;
  UINT64  HighPartOfGuid2;

  LowPartOfGuid1  = ReadUnaligned64 ((CONST UINT64 *)Guid1);
  LowPartOfGuid2  = ReadUnaligned64 ((CONST UINT64 *)Guid2);
  HighPartOfGuid1 = ReadUnaligned64 ((CONST UINT64 *)Guid1 + 1);
  HighPartOfGuid2 = ReadUnaligned64 ((CONST UINT64 *)Guid2 + 1);

  return (BOOLEAN)(LowPartOfGuid1 == LowPartOfGuid2 && HighPartOfGuid1 == HighPartOfGuid2);
}

/**
  Scans a target buffer for a GUID, and returns a pointer to the matching GUID
  in the target buffer.

  This function searches the target buffer specified by Buffer and Length from
  the lowest address to the highest address at 128-bit increments for the 128-bit
  GUID value that matches Guid.  If a match is found, then a pointer to the matching
  GUID in the target buffer is returned.  If no match is found, then NULL is returned.
  If Length is 0, then NULL is returned.

  If Length > 0 and Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 128-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer  The pointer to the target buffer to scan.
  @param  Length  The number of bytes in Buffer to scan.
  @param  Guid    The value to search for in the target buffer.

  @return A pointer to the matching Guid in the target buffer or NULL otherwise.

**/
VOID *
EFIAPI
ScanGuid (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN CONST GUID  *Guid
  )
{
  CONST GUID  *GuidPtr;

  ASSERT (((UINTN)Buffer & (sizeof (Guid->Data1) - 1)) == 0);
  ASSERT (Length <= (MAX_ADDRESS - (UINTN)Buffer + 1));
  ASSERT ((Length & (sizeof (*GuidPtr) - 1)) == 0);

  GuidPtr = (GUID *)Buffer;
  Buffer  = GuidPtr + Length / sizeof (*GuidPtr);
  while (GuidPtr < (CONST GUID *)Buffer) {
    if (CompareGuid (GuidPtr, Guid)) {
      return (VOID *)GuidPtr;
    }

    GuidPtr++;
  }

  return NULL;
}

/**
  Checks if the given GUID is a zero GUID.

  This function checks whether the given GUID is a zero GUID. If the GUID is
  identical to a zero GUID then TRUE is returned. Otherwise, FALSE is returned.

  If Guid is NULL, then ASSERT().

  @param  Guid        The pointer to a 128 bit GUID.

  @retval TRUE        Guid is a zero GUID.
  @retval FALSE       Guid is not a zero GUID.

**/
BOOLEAN
EFIAPI
IsZeroGuid (
  IN CONST GUID  *Guid
  )
{
  UINT64  LowPartOfGuid;
  UINT64  HighPartOfGuid;

  LowPartOfGuid  = ReadUnaligned64 ((CONST UINT64 *)Guid);
  HighPartOfGuid = ReadUnaligned64 ((CONST UINT64 *)Guid + 1);

  return (BOOLEAN)(LowPartOfGuid == 0 && HighPartOfGuid == 0);
}
