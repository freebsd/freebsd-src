/** @file
  Implementation of the EfiSetMem routine. This function is broken
  out into its own source file so that it can be excluded from a
  build for a particular platform easily if an optimized version
  is desired.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2012 - 2013, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MemLibInternals.h"

/**
  Set Buffer to Value for Size bytes.

  @param  Buffer   The memory to set.
  @param  Length   The number of bytes to set.
  @param  Value    The value of the set operation.

  @return Buffer

**/
VOID *
EFIAPI
InternalMemSetMem (
  OUT     VOID   *Buffer,
  IN      UINTN  Length,
  IN      UINT8  Value
  )
{
  //
  // Declare the local variables that actually move the data elements as
  // volatile to prevent the optimizer from replacing this function with
  // the intrinsic memset()
  //
  volatile UINT8   *Pointer8;
  volatile UINT32  *Pointer32;
  volatile UINT64  *Pointer64;
  UINT32           Value32;
  UINT64           Value64;

  if ((((UINTN)Buffer & 0x7) == 0) && (Length >= 8)) {
    // Generate the 64bit value
    Value32 = (Value << 24) | (Value << 16) | (Value << 8) | Value;
    Value64 = LShiftU64 (Value32, 32) | Value32;

    Pointer64 = (UINT64 *)Buffer;
    while (Length >= 8) {
      *(Pointer64++) = Value64;
      Length        -= 8;
    }

    // Finish with bytes if needed
    Pointer8 = (UINT8 *)Pointer64;
  } else if ((((UINTN)Buffer & 0x3) == 0) && (Length >= 4)) {
    // Generate the 32bit value
    Value32 = (Value << 24) | (Value << 16) | (Value << 8) | Value;

    Pointer32 = (UINT32 *)Buffer;
    while (Length >= 4) {
      *(Pointer32++) = Value32;
      Length        -= 4;
    }

    // Finish with bytes if needed
    Pointer8 = (UINT8 *)Pointer32;
  } else {
    Pointer8 = (UINT8 *)Buffer;
  }

  while (Length-- > 0) {
    *(Pointer8++) = Value;
  }

  return Buffer;
}
