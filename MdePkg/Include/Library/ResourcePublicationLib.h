/** @file
  Provides a service to publish discovered system resources.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __RESOURCE_PUBLICATION_LIB__
#define __RESOURCE_PUBLICATION_LIB__

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
  );

#endif
