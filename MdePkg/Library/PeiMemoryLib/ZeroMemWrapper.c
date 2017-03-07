/** @file
  ZeroMem() implementation.

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
  Fills a target buffer with zeros, and returns the target buffer.

  This function fills Length bytes of Buffer with zeros, and returns Buffer.
  
  If Length > 0 and Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the target buffer to fill with zeros.
  @param  Length      The number of bytes in Buffer to fill with zeros.

  @return Buffer.

**/
VOID *
EFIAPI
ZeroMem (
  OUT VOID  *Buffer,
  IN UINTN  Length
  )
{
  if (Length == 0) {
    return Buffer;
  }

  ASSERT (Buffer != NULL);
  ASSERT (Length <= (MAX_ADDRESS - (UINTN)Buffer + 1));
  return InternalMemZeroMem (Buffer, Length);
}
