/** @file
  Implementation of GUID functions for ARM and AARCH64

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MemLibInternals.h"

/**
  Internal function to compare two GUIDs.

  This function compares Guid1 to Guid2.  If the GUIDs are identical then TRUE is returned.
  If there are any bit differences in the two GUIDs, then FALSE is returned.

  @param  Guid1       A pointer to a 128 bit GUID.
  @param  Guid2       A pointer to a 128 bit GUID.

  @retval TRUE        Guid1 and Guid2 are identical.
  @retval FALSE       Guid1 and Guid2 are not identical.

**/
BOOLEAN
EFIAPI
InternalMemCompareGuid (
  IN CONST GUID  *Guid1,
  IN CONST GUID  *Guid2
  );

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
  ASSERT (DestinationGuid != NULL);
  ASSERT (SourceGuid != NULL);

  return InternalMemCopyMem (DestinationGuid, SourceGuid, sizeof (GUID));
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
  ASSERT (Guid1 != NULL);
  ASSERT (Guid2 != NULL);

  return InternalMemCompareGuid (Guid1, Guid2);
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
    if (InternalMemCompareGuid (GuidPtr, Guid)) {
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
  ASSERT (Guid != NULL);

  return InternalMemCompareGuid (Guid, NULL);
}
