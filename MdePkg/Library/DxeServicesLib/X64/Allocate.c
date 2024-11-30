/** @file
  DxeServicesLib memory allocation routines

  Copyright (c) 2018, Linaro, Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/HobLib.h>
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
  EFI_STATUS                  Status;
  EFI_ALLOCATE_TYPE           AllocType;
  EFI_PHYSICAL_ADDRESS        Memory;
  EFI_HOB_HANDOFF_INFO_TABLE  *PhitHob;

  if (Pages == 0) {
    return NULL;
  }

  AllocType = AllocateAnyPages;
  //
  // A X64 build of DXE may be combined with a 32-bit build of PEI, and so we
  // need to check the memory limit set by PEI, and allocate below 4 GB if the
  // limit is set to 4 GB or lower.
  //
  PhitHob = (EFI_HOB_HANDOFF_INFO_TABLE *)GetHobList ();
  if (PhitHob->EfiFreeMemoryTop <= MAX_UINT32) {
    AllocType = AllocateMaxAddress;
    Memory    = MAX_UINT32;
  }

  Status = gBS->AllocatePages (AllocType, MemoryType, Pages, &Memory);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return (VOID *)(UINTN)Memory;
}
