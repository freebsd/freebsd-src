/** @file
  Resource Publication Library that uses PEI Core Services to publish system memory.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/



#include <PiPei.h>


#include <Library/ResourcePublicationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/DebugLib.h>


/**
  Declares the presence of permanent system memory in the platform.

  Declares that the system memory buffer specified by MemoryBegin and MemoryLength
  as permanent memory that may be used for general purpose use by software.
  The amount of memory available to software may be less than MemoryLength
  if published memory has alignment restrictions.
  If MemoryLength is 0, then ASSERT().
  If MemoryLength is greater than (MAX_ADDRESS - MemoryBegin + 1), then ASSERT().

  @param  MemoryBegin               The start address of the memory being declared.
  @param  MemoryLength              The number of bytes of memory being declared.

  @retval  RETURN_SUCCESS           The memory buffer was published.
  @retval  RETURN_OUT_OF_RESOURCES  There are not enough resources to publish the memory buffer

**/
RETURN_STATUS
EFIAPI
PublishSystemMemory (
  IN PHYSICAL_ADDRESS       MemoryBegin,
  IN UINT64                 MemoryLength
  )
{
  EFI_STATUS        Status;

  ASSERT (MemoryLength > 0);
  ASSERT (MemoryLength <= (MAX_ADDRESS - MemoryBegin + 1));

  Status      = PeiServicesInstallPeiMemory (MemoryBegin, MemoryLength);

  return (RETURN_STATUS) Status;
}

