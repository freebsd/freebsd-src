/** @file
  DxeServicesLib memory allocation routines

  Copyright (c) 2018, Linaro, Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesLib.h>

/**
  Allocates one or more 4KB pages of a given type from a memory region that is
  accessible to PEI.

  Allocates the number of 4KB pages of type 'MemoryType' and returns a
  pointer to the allocated buffer.  The buffer returned is aligned on a 4KB
  boundary.  If Pages is 0, then NULL is returned.  If there is not enough
  memory remaining to satisfy the request, then NULL is returned.

  @param[in]  MemoryType            The memory type to allocate
  @param[in]  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocatePeiAccessiblePages (
  IN EFI_MEMORY_TYPE  MemoryType,
  IN UINTN            Pages
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Memory;

  if (Pages == 0) {
    return NULL;
  }

  Status = gBS->AllocatePages (AllocateAnyPages, MemoryType, Pages, &Memory);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return (VOID *)(UINTN)Memory;
}
