/** @file
  Implementation of IsZeroBuffer function.

  The following BaseMemoryLib instances contain the same copy of this file:

    BaseMemoryLib
    BaseMemoryLibMmx
    BaseMemoryLibSse2
    BaseMemoryLibRepStr
    BaseMemoryLibOptDxe
    BaseMemoryLibOptPei
    PeiMemoryLib
    UefiMemoryLib

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MemLibInternals.h"

/**
  Checks if the contents of a buffer are all zeros.

  This function checks whether the contents of a buffer are all zeros. If the
  contents are all zeros, return TRUE. Otherwise, return FALSE.

  If Length > 0 and Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to be checked.
  @param  Length      The size of the buffer (in bytes) to be checked.

  @retval TRUE        Contents of the buffer are all zeros.
  @retval FALSE       Contents of the buffer are not all zeros.

**/
BOOLEAN
EFIAPI
IsZeroBuffer (
  IN CONST VOID  *Buffer,
  IN UINTN       Length
  )
{
  ASSERT (!(Buffer == NULL && Length > 0));
  ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINTN)Buffer));
  return InternalMemIsZeroBuffer (Buffer, Length);
}
