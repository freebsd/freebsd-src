/** @file
  Implementation of the InternalMemCopyMem routine. This function is broken
  out into its own source file so that it can be excluded from a build for a
  particular platform easily if an optimized version is desired.

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2012 - 2013, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MemLibInternals.h"

/**
  Copy Length bytes from Source to Destination.

  @param  DestinationBuffer The target of the copy request.
  @param  SourceBuffer      The place to copy from.
  @param  Length            The number of bytes to copy.

  @return Destination

**/
VOID *
EFIAPI
InternalMemCopyMem (
  OUT     VOID        *DestinationBuffer,
  IN      CONST VOID  *SourceBuffer,
  IN      UINTN       Length
  )
{
  //
  // Declare the local variables that actually move the data elements as
  // volatile to prevent the optimizer from replacing this function with
  // the intrinsic memcpy()
  //
  volatile UINT8   *Destination8;
  CONST UINT8      *Source8;
  volatile UINT32  *Destination32;
  CONST UINT32     *Source32;
  volatile UINT64  *Destination64;
  CONST UINT64     *Source64;
  UINTN            Alignment;

  if ((((UINTN)DestinationBuffer & 0x7) == 0) && (((UINTN)SourceBuffer & 0x7) == 0) && (Length >= 8)) {
    if (SourceBuffer > DestinationBuffer) {
      Destination64 = (UINT64 *)DestinationBuffer;
      Source64      = (CONST UINT64 *)SourceBuffer;
      while (Length >= 8) {
        *(Destination64++) = *(Source64++);
        Length            -= 8;
      }

      // Finish if there are still some bytes to copy
      Destination8 = (UINT8 *)Destination64;
      Source8      = (CONST UINT8 *)Source64;
      while (Length-- != 0) {
        *(Destination8++) = *(Source8++);
      }
    } else if (SourceBuffer < DestinationBuffer) {
      Destination64 = (UINT64 *)((UINTN)DestinationBuffer + Length);
      Source64      = (CONST UINT64 *)((UINTN)SourceBuffer + Length);

      // Destination64 and Source64 were aligned on a 64-bit boundary
      // but if length is not a multiple of 8 bytes then they won't be
      // anymore.

      Alignment = Length & 0x7;
      if (Alignment != 0) {
        Destination8 = (UINT8 *)Destination64;
        Source8      = (CONST UINT8 *)Source64;

        while (Alignment-- != 0) {
          *(--Destination8) = *(--Source8);
          --Length;
        }

        Destination64 = (UINT64 *)Destination8;
        Source64      = (CONST UINT64 *)Source8;
      }

      while (Length > 0) {
        *(--Destination64) = *(--Source64);
        Length            -= 8;
      }
    }
  } else if ((((UINTN)DestinationBuffer & 0x3) == 0) && (((UINTN)SourceBuffer & 0x3) == 0) && (Length >= 4)) {
    if (SourceBuffer > DestinationBuffer) {
      Destination32 = (UINT32 *)DestinationBuffer;
      Source32      = (CONST UINT32 *)SourceBuffer;
      while (Length >= 4) {
        *(Destination32++) = *(Source32++);
        Length            -= 4;
      }

      // Finish if there are still some bytes to copy
      Destination8 = (UINT8 *)Destination32;
      Source8      = (CONST UINT8 *)Source32;
      while (Length-- != 0) {
        *(Destination8++) = *(Source8++);
      }
    } else if (SourceBuffer < DestinationBuffer) {
      Destination32 = (UINT32 *)((UINTN)DestinationBuffer + Length);
      Source32      = (CONST UINT32 *)((UINTN)SourceBuffer + Length);

      // Destination32 and Source32 were aligned on a 32-bit boundary
      // but if length is not a multiple of 4 bytes then they won't be
      // anymore.

      Alignment = Length & 0x3;
      if (Alignment != 0) {
        Destination8 = (UINT8 *)Destination32;
        Source8      = (CONST UINT8 *)Source32;

        while (Alignment-- != 0) {
          *(--Destination8) = *(--Source8);
          --Length;
        }

        Destination32 = (UINT32 *)Destination8;
        Source32      = (CONST UINT32 *)Source8;
      }

      while (Length > 0) {
        *(--Destination32) = *(--Source32);
        Length            -= 4;
      }
    }
  } else {
    if (SourceBuffer > DestinationBuffer) {
      Destination8 = (UINT8 *)DestinationBuffer;
      Source8      = (CONST UINT8 *)SourceBuffer;
      while (Length-- != 0) {
        *(Destination8++) = *(Source8++);
      }
    } else if (SourceBuffer < DestinationBuffer) {
      Destination8 = (UINT8 *)DestinationBuffer + (Length - 1);
      Source8      = (CONST UINT8 *)SourceBuffer + (Length - 1);
      while (Length-- != 0) {
        *(Destination8--) = *(Source8--);
      }
    }
  }

  return DestinationBuffer;
}
