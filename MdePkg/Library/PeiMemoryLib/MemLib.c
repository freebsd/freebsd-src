/** @file
  Base Memory Library functions implementation bases on PeiServcie.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "MemLibInternals.h"

/**
  Copies a source buffer to a destination buffer, and returns the destination buffer.

  This function wraps the gPS->CopyMem ().
  
  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @return DestinationBuffer.

**/
VOID *
EFIAPI
InternalMemCopyMem (
  OUT     VOID                      *Destination,
  IN      CONST VOID                *Source,
  IN      UINTN                     Length
  )
{
  (*GetPeiServicesTablePointer ())->CopyMem (
                                      Destination,
                                      (VOID*)Source,
                                      Length
                                      );
  return Destination;
}

/**
  Fills a target buffer with a byte value, and returns the target buffer.

  This function wraps the gPS->SetMem ().
  
  @param  Buffer    Memory to set.
  @param  Size      The number of bytes to set.
  @param  Value     Value of the set operation.

  @return Buffer.

**/
VOID *
EFIAPI
InternalMemSetMem (
  OUT     VOID                      *Buffer,
  IN      UINTN                     Size,
  IN      UINT8                     Value
  )
{
  (*GetPeiServicesTablePointer ())->SetMem (
                                      Buffer,
                                      Size,
                                      Value
                                      );
  return Buffer;
}
