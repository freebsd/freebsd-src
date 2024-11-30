/** @file
  Instance of SMM memory check library.

  SMM memory check library library implementation. This library consumes SMM_ACCESS2_PROTOCOL
  to get SMRAM information. In order to use this library instance, the platform should produce
  all SMRAM range via SMM_ACCESS2_PROTOCOL, including the range for firmware (like SMM Core
  and SMM driver) and/or specific dedicated hardware.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiSmm.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/HobLib.h>
#include <Protocol/SmmAccess2.h>
#include <Protocol/SmmReadyToLock.h>
#include <Protocol/SmmEndOfDxe.h>
#include <Guid/MemoryAttributesTable.h>

//
// attributes for reserved memory before it is promoted to system memory
//
#define EFI_MEMORY_PRESENT      0x0100000000000000ULL
#define EFI_MEMORY_INITIALIZED  0x0200000000000000ULL
#define EFI_MEMORY_TESTED       0x0400000000000000ULL

EFI_SMRAM_DESCRIPTOR  *mSmmMemLibInternalSmramRanges;
UINTN                 mSmmMemLibInternalSmramCount;

//
// Maximum support address used to check input buffer
//
EFI_PHYSICAL_ADDRESS  mSmmMemLibInternalMaximumSupportAddress = 0;

UINTN                  mMemoryMapEntryCount;
EFI_MEMORY_DESCRIPTOR  *mMemoryMap;
UINTN                  mDescriptorSize;

EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *mSmmMemLibGcdMemSpace       = NULL;
UINTN                            mSmmMemLibGcdMemNumberOfDesc = 0;

EFI_MEMORY_ATTRIBUTES_TABLE  *mSmmMemLibMemoryAttributesTable = NULL;

VOID  *mRegistrationEndOfDxe;
VOID  *mRegistrationReadyToLock;

BOOLEAN  mSmmMemLibSmmReadyToLock = FALSE;

/**
  Calculate and save the maximum support address.

**/
VOID
SmmMemLibInternalCalculateMaximumSupportAddress (
  VOID
  )
{
  VOID    *Hob;
  UINT32  RegEax;
  UINT8   PhysicalAddressBits;

  //
  // Get physical address bits supported.
  //
  Hob = GetFirstHob (EFI_HOB_TYPE_CPU);
  if (Hob != NULL) {
    PhysicalAddressBits = ((EFI_HOB_CPU *)Hob)->SizeOfMemorySpace;
  } else {
    AsmCpuid (0x80000000, &RegEax, NULL, NULL, NULL);
    if (RegEax >= 0x80000008) {
      AsmCpuid (0x80000008, &RegEax, NULL, NULL, NULL);
      PhysicalAddressBits = (UINT8)RegEax;
    } else {
      PhysicalAddressBits = 36;
    }
  }

  //
  // IA-32e paging translates 48-bit linear addresses to 52-bit physical addresses.
  //
  ASSERT (PhysicalAddressBits <= 52);
  if (PhysicalAddressBits > 48) {
    PhysicalAddressBits = 48;
  }

  //
  // Save the maximum support address in one global variable
  //
  mSmmMemLibInternalMaximumSupportAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)(LShiftU64 (1, PhysicalAddressBits) - 1);
  DEBUG ((DEBUG_INFO, "mSmmMemLibInternalMaximumSupportAddress = 0x%lx\n", mSmmMemLibInternalMaximumSupportAddress));
}

/**
  This function check if the buffer is valid per processor architecture and not overlap with SMRAM.

  @param Buffer  The buffer start address to be checked.
  @param Length  The buffer length to be checked.

  @retval TRUE  This buffer is valid per processor architecture and not overlap with SMRAM.
  @retval FALSE This buffer is not valid per processor architecture or overlap with SMRAM.
**/
BOOLEAN
EFIAPI
SmmIsBufferOutsideSmmValid (
  IN EFI_PHYSICAL_ADDRESS  Buffer,
  IN UINT64                Length
  )
{
  UINTN  Index;

  //
  // Check override.
  // NOTE: (B:0->L:4G) is invalid for IA32, but (B:1->L:4G-1)/(B:4G-1->L:1) is valid.
  //
  if ((Length > mSmmMemLibInternalMaximumSupportAddress) ||
      (Buffer > mSmmMemLibInternalMaximumSupportAddress) ||
      ((Length != 0) && (Buffer > (mSmmMemLibInternalMaximumSupportAddress - (Length - 1)))))
  {
    //
    // Overflow happen
    //
    DEBUG ((
      DEBUG_ERROR,
      "SmmIsBufferOutsideSmmValid: Overflow: Buffer (0x%lx) - Length (0x%lx), MaximumSupportAddress (0x%lx)\n",
      Buffer,
      Length,
      mSmmMemLibInternalMaximumSupportAddress
      ));
    return FALSE;
  }

  for (Index = 0; Index < mSmmMemLibInternalSmramCount; Index++) {
    if (((Buffer >= mSmmMemLibInternalSmramRanges[Index].CpuStart) && (Buffer < mSmmMemLibInternalSmramRanges[Index].CpuStart + mSmmMemLibInternalSmramRanges[Index].PhysicalSize)) ||
        ((mSmmMemLibInternalSmramRanges[Index].CpuStart >= Buffer) && (mSmmMemLibInternalSmramRanges[Index].CpuStart < Buffer + Length)))
    {
      DEBUG ((
        DEBUG_ERROR,
        "SmmIsBufferOutsideSmmValid: Overlap: Buffer (0x%lx) - Length (0x%lx), ",
        Buffer,
        Length
        ));
      DEBUG ((
        DEBUG_ERROR,
        "CpuStart (0x%lx) - PhysicalSize (0x%lx)\n",
        mSmmMemLibInternalSmramRanges[Index].CpuStart,
        mSmmMemLibInternalSmramRanges[Index].PhysicalSize
        ));
      return FALSE;
    }
  }

  //
  // Check override for Valid Communication Region
  //
  if (mSmmMemLibSmmReadyToLock) {
    EFI_MEMORY_DESCRIPTOR  *MemoryMap;
    BOOLEAN                InValidCommunicationRegion;

    InValidCommunicationRegion = FALSE;
    MemoryMap                  = mMemoryMap;
    for (Index = 0; Index < mMemoryMapEntryCount; Index++) {
      if ((Buffer >= MemoryMap->PhysicalStart) &&
          (Buffer + Length <= MemoryMap->PhysicalStart + LShiftU64 (MemoryMap->NumberOfPages, EFI_PAGE_SHIFT)))
      {
        InValidCommunicationRegion = TRUE;
      }

      MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, mDescriptorSize);
    }

    if (!InValidCommunicationRegion) {
      DEBUG ((
        DEBUG_ERROR,
        "SmmIsBufferOutsideSmmValid: Not in ValidCommunicationRegion: Buffer (0x%lx) - Length (0x%lx)\n",
        Buffer,
        Length
        ));
      return FALSE;
    }

    //
    // Check untested memory as invalid communication buffer.
    //
    for (Index = 0; Index < mSmmMemLibGcdMemNumberOfDesc; Index++) {
      if (((Buffer >= mSmmMemLibGcdMemSpace[Index].BaseAddress) && (Buffer < mSmmMemLibGcdMemSpace[Index].BaseAddress + mSmmMemLibGcdMemSpace[Index].Length)) ||
          ((mSmmMemLibGcdMemSpace[Index].BaseAddress >= Buffer) && (mSmmMemLibGcdMemSpace[Index].BaseAddress < Buffer + Length)))
      {
        DEBUG ((
          DEBUG_ERROR,
          "SmmIsBufferOutsideSmmValid: In Untested Memory Region: Buffer (0x%lx) - Length (0x%lx)\n",
          Buffer,
          Length
          ));
        return FALSE;
      }
    }

    //
    // Check UEFI runtime memory with EFI_MEMORY_RO as invalid communication buffer.
    //
    if (mSmmMemLibMemoryAttributesTable != NULL) {
      EFI_MEMORY_DESCRIPTOR  *Entry;

      Entry = (EFI_MEMORY_DESCRIPTOR *)(mSmmMemLibMemoryAttributesTable + 1);
      for (Index = 0; Index < mSmmMemLibMemoryAttributesTable->NumberOfEntries; Index++) {
        if ((Entry->Type == EfiRuntimeServicesCode) || (Entry->Type == EfiRuntimeServicesData)) {
          if ((Entry->Attribute & EFI_MEMORY_RO) != 0) {
            if (((Buffer >= Entry->PhysicalStart) && (Buffer < Entry->PhysicalStart + LShiftU64 (Entry->NumberOfPages, EFI_PAGE_SHIFT))) ||
                ((Entry->PhysicalStart >= Buffer) && (Entry->PhysicalStart < Buffer + Length)))
            {
              DEBUG ((
                DEBUG_ERROR,
                "SmmIsBufferOutsideSmmValid: In RuntimeCode Region: Buffer (0x%lx) - Length (0x%lx)\n",
                Buffer,
                Length
                ));
              return FALSE;
            }
          }
        }

        Entry = NEXT_MEMORY_DESCRIPTOR (Entry, mSmmMemLibMemoryAttributesTable->DescriptorSize);
      }
    }
  }

  return TRUE;
}

/**
  Copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if source buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it return EFI_SECURITY_VIOLATION.
  The implementation must be reentrant.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The SourceBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMemToSmram (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  )
{
  if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)SourceBuffer, Length)) {
    DEBUG ((DEBUG_ERROR, "SmmCopyMemToSmram: Security Violation: Source (0x%x), Length (0x%x)\n", SourceBuffer, Length));
    return EFI_SECURITY_VIOLATION;
  }

  CopyMem (DestinationBuffer, SourceBuffer, Length);
  return EFI_SUCCESS;
}

/**
  Copies a source buffer (SMRAM) to a destination buffer (NON-SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if destination buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.
  The implementation must be reentrant.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The DestinationBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMemFromSmram (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  )
{
  if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)DestinationBuffer, Length)) {
    DEBUG ((DEBUG_ERROR, "SmmCopyMemFromSmram: Security Violation: Destination (0x%x), Length (0x%x)\n", DestinationBuffer, Length));
    return EFI_SECURITY_VIOLATION;
  }

  CopyMem (DestinationBuffer, SourceBuffer, Length);
  return EFI_SUCCESS;
}

/**
  Copies a source buffer (NON-SMRAM) to a destination buffer (NON-SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if source buffer and destination buffer are valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.
  The implementation must be reentrant, and it must handle the case where source buffer overlaps destination buffer.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The DestinationBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SECURITY_VIOLATION The SourceBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMem (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  )
{
  if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)DestinationBuffer, Length)) {
    DEBUG ((DEBUG_ERROR, "SmmCopyMem: Security Violation: Destination (0x%x), Length (0x%x)\n", DestinationBuffer, Length));
    return EFI_SECURITY_VIOLATION;
  }

  if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)SourceBuffer, Length)) {
    DEBUG ((DEBUG_ERROR, "SmmCopyMem: Security Violation: Source (0x%x), Length (0x%x)\n", SourceBuffer, Length));
    return EFI_SECURITY_VIOLATION;
  }

  CopyMem (DestinationBuffer, SourceBuffer, Length);
  return EFI_SUCCESS;
}

/**
  Fills a target buffer (NON-SMRAM) with a byte value.

  This function fills a target buffer (non-SMRAM) with a byte value.
  It checks if target buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it fills memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.

  @param  Buffer    The memory to set.
  @param  Length    The number of bytes to set.
  @param  Value     The value with which to fill Length bytes of Buffer.

  @retval EFI_SECURITY_VIOLATION The Buffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is set.

**/
EFI_STATUS
EFIAPI
SmmSetMem (
  OUT VOID  *Buffer,
  IN UINTN  Length,
  IN UINT8  Value
  )
{
  if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)Buffer, Length)) {
    DEBUG ((DEBUG_ERROR, "SmmSetMem: Security Violation: Source (0x%x), Length (0x%x)\n", Buffer, Length));
    return EFI_SECURITY_VIOLATION;
  }

  SetMem (Buffer, Length, Value);
  return EFI_SUCCESS;
}

/**
  Get GCD memory map.
  Only record untested memory as invalid communication buffer.
**/
VOID
SmmMemLibInternalGetGcdMemoryMap (
  VOID
  )
{
  UINTN                            NumberOfDescriptors;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemSpaceMap;
  EFI_STATUS                       Status;
  UINTN                            Index;

  Status = gDS->GetMemorySpaceMap (&NumberOfDescriptors, &MemSpaceMap);
  if (EFI_ERROR (Status)) {
    return;
  }

  mSmmMemLibGcdMemNumberOfDesc = 0;
  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemSpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((MemSpaceMap[Index].Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      mSmmMemLibGcdMemNumberOfDesc++;
    }
  }

  mSmmMemLibGcdMemSpace = AllocateZeroPool (mSmmMemLibGcdMemNumberOfDesc * sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR));
  ASSERT (mSmmMemLibGcdMemSpace != NULL);
  if (mSmmMemLibGcdMemSpace == NULL) {
    mSmmMemLibGcdMemNumberOfDesc = 0;
    gBS->FreePool (MemSpaceMap);
    return;
  }

  mSmmMemLibGcdMemNumberOfDesc = 0;
  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemSpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((MemSpaceMap[Index].Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      CopyMem (
        &mSmmMemLibGcdMemSpace[mSmmMemLibGcdMemNumberOfDesc],
        &MemSpaceMap[Index],
        sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR)
        );
      mSmmMemLibGcdMemNumberOfDesc++;
    }
  }

  gBS->FreePool (MemSpaceMap);
}

/**
  Get UEFI MemoryAttributesTable.
**/
VOID
SmmMemLibInternalGetUefiMemoryAttributesTable (
  VOID
  )
{
  EFI_STATUS                   Status;
  EFI_MEMORY_ATTRIBUTES_TABLE  *MemoryAttributesTable;
  UINTN                        MemoryAttributesTableSize;

  Status = EfiGetSystemConfigurationTable (&gEfiMemoryAttributesTableGuid, (VOID **)&MemoryAttributesTable);
  if (!EFI_ERROR (Status) && (MemoryAttributesTable != NULL)) {
    MemoryAttributesTableSize       = sizeof (EFI_MEMORY_ATTRIBUTES_TABLE) + MemoryAttributesTable->DescriptorSize * MemoryAttributesTable->NumberOfEntries;
    mSmmMemLibMemoryAttributesTable = AllocateCopyPool (MemoryAttributesTableSize, MemoryAttributesTable);
    ASSERT (mSmmMemLibMemoryAttributesTable != NULL);
  }
}

/**
  Notification for SMM EndOfDxe protocol.

  @param[in] Protocol   Points to the protocol's unique identifier.
  @param[in] Interface  Points to the interface instance.
  @param[in] Handle     The handle on which the interface was installed.

  @retval EFI_SUCCESS   Notification runs successfully.
**/
EFI_STATUS
EFIAPI
SmmLibInternalEndOfDxeNotify (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  EFI_STATUS             Status;
  UINTN                  MapKey;
  UINTN                  MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  EFI_MEMORY_DESCRIPTOR  *MemoryMapStart;
  EFI_MEMORY_DESCRIPTOR  *SmmMemoryMapStart;
  UINTN                  MemoryMapEntryCount;
  UINTN                  DescriptorSize;
  UINT32                 DescriptorVersion;
  UINTN                  Index;

  MemoryMapSize = 0;
  MemoryMap     = NULL;
  Status        = gBS->GetMemoryMap (
                         &MemoryMapSize,
                         MemoryMap,
                         &MapKey,
                         &DescriptorSize,
                         &DescriptorVersion
                         );
  ASSERT (Status == EFI_BUFFER_TOO_SMALL);

  do {
    Status = gBS->AllocatePool (EfiBootServicesData, MemoryMapSize, (VOID **)&MemoryMap);
    ASSERT (MemoryMap != NULL);

    Status = gBS->GetMemoryMap (
                    &MemoryMapSize,
                    MemoryMap,
                    &MapKey,
                    &DescriptorSize,
                    &DescriptorVersion
                    );
    if (EFI_ERROR (Status)) {
      gBS->FreePool (MemoryMap);
    }
  } while (Status == EFI_BUFFER_TOO_SMALL);

  //
  // Get Count
  //
  mDescriptorSize      = DescriptorSize;
  MemoryMapEntryCount  = MemoryMapSize/DescriptorSize;
  MemoryMapStart       = MemoryMap;
  mMemoryMapEntryCount = 0;
  for (Index = 0; Index < MemoryMapEntryCount; Index++) {
    switch (MemoryMap->Type) {
      case EfiReservedMemoryType:
      case EfiRuntimeServicesCode:
      case EfiRuntimeServicesData:
      case EfiACPIMemoryNVS:
        mMemoryMapEntryCount++;
        break;
    }

    MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, DescriptorSize);
  }

  MemoryMap = MemoryMapStart;

  //
  // Get Data
  //
  mMemoryMap = AllocatePool (mMemoryMapEntryCount*DescriptorSize);
  ASSERT (mMemoryMap != NULL);
  SmmMemoryMapStart = mMemoryMap;
  for (Index = 0; Index < MemoryMapEntryCount; Index++) {
    switch (MemoryMap->Type) {
      case EfiReservedMemoryType:
      case EfiRuntimeServicesCode:
      case EfiRuntimeServicesData:
      case EfiACPIMemoryNVS:
        CopyMem (mMemoryMap, MemoryMap, DescriptorSize);
        mMemoryMap = NEXT_MEMORY_DESCRIPTOR (mMemoryMap, DescriptorSize);
        break;
    }

    MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, DescriptorSize);
  }

  mMemoryMap = SmmMemoryMapStart;
  MemoryMap  = MemoryMapStart;

  gBS->FreePool (MemoryMap);

  //
  // Get additional information from GCD memory map.
  //
  SmmMemLibInternalGetGcdMemoryMap ();

  //
  // Get UEFI memory attributes table.
  //
  SmmMemLibInternalGetUefiMemoryAttributesTable ();

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
SmmLibInternalReadyToLockNotify (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  mSmmMemLibSmmReadyToLock = TRUE;
  return EFI_SUCCESS;
}

/**
  The constructor function initializes the Smm Mem library

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmMemLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_SMM_ACCESS2_PROTOCOL  *SmmAccess;
  UINTN                     Size;

  //
  // Get SMRAM information
  //
  Status = gBS->LocateProtocol (&gEfiSmmAccess2ProtocolGuid, NULL, (VOID **)&SmmAccess);
  ASSERT_EFI_ERROR (Status);

  Size   = 0;
  Status = SmmAccess->GetCapabilities (SmmAccess, &Size, NULL);
  ASSERT (Status == EFI_BUFFER_TOO_SMALL);

  mSmmMemLibInternalSmramRanges = AllocatePool (Size);
  ASSERT (mSmmMemLibInternalSmramRanges != NULL);

  Status = SmmAccess->GetCapabilities (SmmAccess, &Size, mSmmMemLibInternalSmramRanges);
  ASSERT_EFI_ERROR (Status);

  mSmmMemLibInternalSmramCount = Size / sizeof (EFI_SMRAM_DESCRIPTOR);

  //
  // Calculate and save maximum support address
  //
  SmmMemLibInternalCalculateMaximumSupportAddress ();

  //
  // Register EndOfDxe to get UEFI memory map
  //
  Status = gSmst->SmmRegisterProtocolNotify (&gEfiSmmEndOfDxeProtocolGuid, SmmLibInternalEndOfDxeNotify, &mRegistrationEndOfDxe);
  ASSERT_EFI_ERROR (Status);

  //
  // Register ready to lock so that we can know when to check valid SMRAM region
  //
  Status = gSmst->SmmRegisterProtocolNotify (&gEfiSmmReadyToLockProtocolGuid, SmmLibInternalReadyToLockNotify, &mRegistrationReadyToLock);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  The destructor function frees resource used in the Smm Mem library

  @param[in]  ImageHandle   The firmware allocated handle for the EFI image.
  @param[in]  SystemTable   A pointer to the EFI System Table.

  @retval     EFI_SUCCESS   The deconstructor always returns EFI_SUCCESS.
**/
EFI_STATUS
EFIAPI
SmmMemLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  FreePool (mSmmMemLibInternalSmramRanges);

  gSmst->SmmRegisterProtocolNotify (&gEfiSmmEndOfDxeProtocolGuid, NULL, &mRegistrationEndOfDxe);
  gSmst->SmmRegisterProtocolNotify (&gEfiSmmReadyToLockProtocolGuid, NULL, &mRegistrationReadyToLock);
  return EFI_SUCCESS;
}
