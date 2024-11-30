/** @file
  Instance of SMM IO check library.

  SMM IO check library library implementation. This library consumes GCD to collect all valid
  IO space defined by a platform.
  A platform may have its own SmmIoLib instance to exclude more IO space.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiSmm.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Protocol/SmmReadyToLock.h>
#include <Protocol/SmmEndOfDxe.h>

EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *mSmmIoLibGcdMemSpace       = NULL;
UINTN                            mSmmIoLibGcdMemNumberOfDesc = 0;

EFI_PHYSICAL_ADDRESS  mSmmIoLibInternalMaximumSupportMemAddress = 0;

VOID  *mSmmIoLibRegistrationEndOfDxe;
VOID  *mSmmIoLibRegistrationReadyToLock;

BOOLEAN  mSmmIoLibReadyToLock = FALSE;

/**
  Calculate and save the maximum support address.

**/
VOID
SmmIoLibInternalCalculateMaximumSupportAddress (
  VOID
  )
{
  VOID    *Hob;
  UINT32  RegEax;
  UINT8   MemPhysicalAddressBits;

  //
  // Get physical address bits supported.
  //
  Hob = GetFirstHob (EFI_HOB_TYPE_CPU);
  if (Hob != NULL) {
    MemPhysicalAddressBits = ((EFI_HOB_CPU *)Hob)->SizeOfMemorySpace;
  } else {
    AsmCpuid (0x80000000, &RegEax, NULL, NULL, NULL);
    if (RegEax >= 0x80000008) {
      AsmCpuid (0x80000008, &RegEax, NULL, NULL, NULL);
      MemPhysicalAddressBits = (UINT8)RegEax;
    } else {
      MemPhysicalAddressBits = 36;
    }
  }

  //
  // IA-32e paging translates 48-bit linear addresses to 52-bit physical addresses.
  //
  ASSERT (MemPhysicalAddressBits <= 52);
  if (MemPhysicalAddressBits > 48) {
    MemPhysicalAddressBits = 48;
  }

  //
  // Save the maximum support address in one global variable
  //
  mSmmIoLibInternalMaximumSupportMemAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)(LShiftU64 (1, MemPhysicalAddressBits) - 1);
  DEBUG ((DEBUG_INFO, "mSmmIoLibInternalMaximumSupportMemAddress = 0x%lx\n", mSmmIoLibInternalMaximumSupportMemAddress));
}

/**
  This function check if the MMIO resource is valid per processor architecture and
  valid per platform design.

  @param BaseAddress  The MMIO start address to be checked.
  @param Length       The MMIO length to be checked.
  @param Owner        A GUID representing the owner of the resource.
                      This GUID may be used by producer to correlate the device ownership of the resource.
                      NULL means no specific owner.

  @retval TRUE  This MMIO resource is valid per processor architecture and valid per platform design.
  @retval FALSE This MMIO resource is not valid per processor architecture or valid per platform design.
**/
BOOLEAN
EFIAPI
SmmIsMmioValid (
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length,
  IN EFI_GUID              *Owner  OPTIONAL
  )
{
  UINTN                            Index;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Desc;
  BOOLEAN                          InValidRegion;

  //
  // Check override.
  // NOTE: (B:0->L:4G) is invalid for IA32, but (B:1->L:4G-1)/(B:4G-1->L:1) is valid.
  //
  if ((Length > mSmmIoLibInternalMaximumSupportMemAddress) ||
      (BaseAddress > mSmmIoLibInternalMaximumSupportMemAddress) ||
      ((Length != 0) && (BaseAddress > (mSmmIoLibInternalMaximumSupportMemAddress - (Length - 1)))))
  {
    //
    // Overflow happen
    //
    DEBUG ((
      DEBUG_ERROR,
      "SmmIsMmioValid: Overflow: BaseAddress (0x%lx) - Length (0x%lx), MaximumSupportMemAddress (0x%lx)\n",
      BaseAddress,
      Length,
      mSmmIoLibInternalMaximumSupportMemAddress
      ));
    return FALSE;
  }

  //
  // Check override for valid MMIO region
  //
  if (mSmmIoLibReadyToLock) {
    InValidRegion = FALSE;
    for (Index = 0; Index < mSmmIoLibGcdMemNumberOfDesc; Index++) {
      Desc = &mSmmIoLibGcdMemSpace[Index];
      if ((Desc->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) &&
          (BaseAddress >= Desc->BaseAddress) &&
          ((BaseAddress + Length) <= (Desc->BaseAddress + Desc->Length)))
      {
        InValidRegion = TRUE;
      }
    }

    if (!InValidRegion) {
      DEBUG ((
        DEBUG_ERROR,
        "SmmIsMmioValid: Not in valid MMIO region: BaseAddress (0x%lx) - Length (0x%lx)\n",
        BaseAddress,
        Length
        ));
      return FALSE;
    }
  }

  return TRUE;
}

/**
  Merge continuous entries whose type is EfiGcdMemoryTypeMemoryMappedIo.

  @param[in, out]  GcdMemoryMap           A pointer to the buffer in which firmware places
                                          the current GCD memory map.
  @param[in, out]  NumberOfDescriptors    A pointer to the number of the
                                          GcdMemoryMap buffer. On input, this is the number of
                                          the current GCD memory map.  On output,
                                          it is the number of new GCD memory map after merge.
**/
STATIC
VOID
MergeGcdMmioEntry (
  IN OUT EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *GcdMemoryMap,
  IN OUT UINTN                            *NumberOfDescriptors
  )
{
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *GcdMemoryMapEntry;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *GcdMemoryMapEnd;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *NewGcdMemoryMapEntry;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *NextGcdMemoryMapEntry;

  GcdMemoryMapEntry    = GcdMemoryMap;
  NewGcdMemoryMapEntry = GcdMemoryMap;
  GcdMemoryMapEnd      = (EFI_GCD_MEMORY_SPACE_DESCRIPTOR *)((UINT8 *)GcdMemoryMap + (*NumberOfDescriptors) * sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR));
  while ((UINTN)GcdMemoryMapEntry < (UINTN)GcdMemoryMapEnd) {
    CopyMem (NewGcdMemoryMapEntry, GcdMemoryMapEntry, sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR));
    NextGcdMemoryMapEntry = GcdMemoryMapEntry + 1;

    do {
      if (((UINTN)NextGcdMemoryMapEntry < (UINTN)GcdMemoryMapEnd) &&
          (GcdMemoryMapEntry->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) && (NextGcdMemoryMapEntry->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) &&
          ((GcdMemoryMapEntry->BaseAddress + GcdMemoryMapEntry->Length) == NextGcdMemoryMapEntry->BaseAddress))
      {
        GcdMemoryMapEntry->Length += NextGcdMemoryMapEntry->Length;
        if (NewGcdMemoryMapEntry != GcdMemoryMapEntry) {
          NewGcdMemoryMapEntry->Length += NextGcdMemoryMapEntry->Length;
        }

        NextGcdMemoryMapEntry = NextGcdMemoryMapEntry + 1;
        continue;
      } else {
        GcdMemoryMapEntry = NextGcdMemoryMapEntry - 1;
        break;
      }
    } while (TRUE);

    GcdMemoryMapEntry    = GcdMemoryMapEntry + 1;
    NewGcdMemoryMapEntry = NewGcdMemoryMapEntry + 1;
  }

  *NumberOfDescriptors = ((UINTN)NewGcdMemoryMapEntry - (UINTN)GcdMemoryMap) / sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR);

  return;
}

/**
  Notification for SMM EndOfDxe protocol.

  @param[in] Protocol   Points to the protocol's unique identifier.
  @param[in] Interface  Points to the interface instance.
  @param[in] Handle     The handle on which the interface was installed.

  @retval EFI_SUCCESS           Notification runs successfully.
  @retval EFI_OUT_OF_RESOURCES  No enough resources to save GCD MMIO map.
**/
EFI_STATUS
EFIAPI
SmmIoLibInternalEndOfDxeNotify (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  UINTN                            NumberOfDescriptors;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemSpaceMap;
  EFI_STATUS                       Status;

  Status = gDS->GetMemorySpaceMap (&NumberOfDescriptors, &MemSpaceMap);
  if (!EFI_ERROR (Status)) {
    MergeGcdMmioEntry (MemSpaceMap, &NumberOfDescriptors);

    mSmmIoLibGcdMemSpace = AllocateCopyPool (NumberOfDescriptors * sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR), MemSpaceMap);
    ASSERT (mSmmIoLibGcdMemSpace != NULL);
    if (mSmmIoLibGcdMemSpace == NULL) {
      gBS->FreePool (MemSpaceMap);
      return EFI_OUT_OF_RESOURCES;
    }

    mSmmIoLibGcdMemNumberOfDesc = NumberOfDescriptors;
    gBS->FreePool (MemSpaceMap);
  }

  return EFI_SUCCESS;
}

/**
  Notification for SMM ReadyToLock protocol.

  @param[in] Protocol   Points to the protocol's unique identifier.
  @param[in] Interface  Points to the interface instance.
  @param[in] Handle     The handle on which the interface was installed.

  @retval EFI_SUCCESS   Notification runs successfully.
**/
EFI_STATUS
EFIAPI
SmmIoLibInternalReadyToLockNotify (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  mSmmIoLibReadyToLock = TRUE;
  return EFI_SUCCESS;
}

/**
  The constructor function initializes the Smm IO library

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmIoLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Calculate and save maximum support address
  //
  SmmIoLibInternalCalculateMaximumSupportAddress ();

  //
  // Register EndOfDxe to get GCD resource map
  //
  Status = gSmst->SmmRegisterProtocolNotify (&gEfiSmmEndOfDxeProtocolGuid, SmmIoLibInternalEndOfDxeNotify, &mSmmIoLibRegistrationEndOfDxe);
  ASSERT_EFI_ERROR (Status);

  //
  // Register ready to lock so that we can know when to check valid resource region
  //
  Status = gSmst->SmmRegisterProtocolNotify (&gEfiSmmReadyToLockProtocolGuid, SmmIoLibInternalReadyToLockNotify, &mSmmIoLibRegistrationReadyToLock);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  The destructor function frees resource used in the Smm IO library

  @param[in]  ImageHandle   The firmware allocated handle for the EFI image.
  @param[in]  SystemTable   A pointer to the EFI System Table.

  @retval     EFI_SUCCESS   The deconstructor always returns EFI_SUCCESS.
**/
EFI_STATUS
EFIAPI
SmmIoLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gSmst->SmmRegisterProtocolNotify (&gEfiSmmEndOfDxeProtocolGuid, NULL, &mSmmIoLibRegistrationEndOfDxe);
  gSmst->SmmRegisterProtocolNotify (&gEfiSmmReadyToLockProtocolGuid, NULL, &mSmmIoLibRegistrationReadyToLock);
  return EFI_SUCCESS;
}
