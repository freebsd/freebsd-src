/** @file
  Instance of Runtime PCI Segment Library that support multi-segment PCI configuration access.

  PCI Segment Library that consumes segment information provided by PciSegmentInfoLib to
   support multi-segment PCI configuration access through enhanced configuration access mechanism.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PciSegmentLibCommon.h"
#include <PiDxe.h>
#include <Guid/EventGroup.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciSegmentInfoLib.h>

///
/// Define table for mapping PCI Segment MMIO physical addresses to virtual addresses at OS runtime
///
typedef struct {
  UINTN    PhysicalAddress;
  UINTN    VirtualAddress;
} PCI_SEGMENT_RUNTIME_REGISTRATION_TABLE;

///
/// Set Virtual Address Map Event
///
EFI_EVENT  mDxeRuntimePciSegmentLibVirtualNotifyEvent = NULL;

///
/// The number of PCI devices that have been registered for runtime access.
///
UINTN  mDxeRuntimePciSegmentLibNumberOfRuntimeRanges = 0;

///
/// The table of PCI devices that have been registered for runtime access.
///
PCI_SEGMENT_RUNTIME_REGISTRATION_TABLE  *mDxeRuntimePciSegmentLibRegistrationTable = NULL;

///
/// The table index of the most recent virtual address lookup.
///
UINTN  mDxeRuntimePciSegmentLibLastRuntimeRange = 0;

/**
  Convert the physical PCI Express MMIO addresses for all registered PCI devices
  to virtual addresses.

  @param[in]    Event   The event that is being processed.
  @param[in]    Context The Event Context.
**/
VOID
EFIAPI
DxeRuntimePciSegmentLibVirtualNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN       Index;
  EFI_STATUS  Status;

  //
  // If there have been no runtime registrations, then just return
  //
  if (mDxeRuntimePciSegmentLibRegistrationTable == NULL) {
    return;
  }

  //
  // Convert physical addresses associated with the set of registered PCI devices to
  // virtual addresses.
  //
  for (Index = 0; Index < mDxeRuntimePciSegmentLibNumberOfRuntimeRanges; Index++) {
    Status = EfiConvertPointer (0, (VOID **)&(mDxeRuntimePciSegmentLibRegistrationTable[Index].VirtualAddress));
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Convert table pointer that is allocated from EfiRuntimeServicesData to a virtual address.
  //
  Status = EfiConvertPointer (0, (VOID **)&mDxeRuntimePciSegmentLibRegistrationTable);
  ASSERT_EFI_ERROR (Status);
}

/**
  The constructor function caches the PCI Express Base Address and creates a
  Set Virtual Address Map event to convert physical address to virtual addresses.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor completed successfully.
  @retval Other value   The constructor did not complete successfully.

**/
EFI_STATUS
EFIAPI
DxeRuntimePciSegmentLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Register SetVirtualAddressMap () notify function
  //
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  DxeRuntimePciSegmentLibVirtualNotify,
                  NULL,
                  &mDxeRuntimePciSegmentLibVirtualNotifyEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  The destructor function frees any allocated buffers and closes the Set Virtual
  Address Map event.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The destructor completed successfully.
  @retval Other value   The destructor did not complete successfully.

**/
EFI_STATUS
EFIAPI
DxeRuntimePciSegmentLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // If one or more PCI devices have been registered for runtime access, then
  // free the registration table.
  //
  if (mDxeRuntimePciSegmentLibRegistrationTable != NULL) {
    FreePool (mDxeRuntimePciSegmentLibRegistrationTable);
  }

  //
  // Close the Set Virtual Address Map event
  //
  Status = gBS->CloseEvent (mDxeRuntimePciSegmentLibVirtualNotifyEvent);
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Register a PCI device so PCI configuration registers may be accessed after
  SetVirtualAddressMap().

  If any reserved bits in Address are set, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.

  @retval RETURN_SUCCESS           The PCI device was registered for runtime access.
  @retval RETURN_UNSUPPORTED       An attempt was made to call this function
                                   after ExitBootServices().
  @retval RETURN_UNSUPPORTED       The resources required to access the PCI device
                                   at runtime could not be mapped.
  @retval RETURN_OUT_OF_RESOURCES  There are not enough resources available to
                                   complete the registration.

**/
RETURN_STATUS
EFIAPI
PciSegmentRegisterForRuntimeAccess (
  IN UINTN  Address
  )
{
  RETURN_STATUS                    Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  UINTN                            Index;
  VOID                             *NewTable;
  UINTN                            Count;
  PCI_SEGMENT_INFO                 *SegmentInfo;
  UINT64                           EcamAddress;

  //
  // Convert Address to a ECAM address at the beginning of the PCI Configuration
  // header for the specified PCI Bus/Dev/Func
  //
  Address    &= ~(UINTN)EFI_PAGE_MASK;
  SegmentInfo = GetPciSegmentInfo (&Count);
  EcamAddress = PciSegmentLibGetEcamAddress (Address, SegmentInfo, Count);

  //
  // Return an error if this function is called after ExitBootServices().
  //
  if (EfiAtRuntime ()) {
    return RETURN_UNSUPPORTED;
  }

  if (sizeof (UINTN) == sizeof (UINT32)) {
    ASSERT (EcamAddress < BASE_4GB);
  }

  Address = (UINTN)EcamAddress;

  //
  // See if Address has already been registered for runtime access
  //
  for (Index = 0; Index < mDxeRuntimePciSegmentLibNumberOfRuntimeRanges; Index++) {
    if (mDxeRuntimePciSegmentLibRegistrationTable[Index].PhysicalAddress == Address) {
      return RETURN_SUCCESS;
    }
  }

  //
  // Get the GCD Memory Descriptor for the ECAM Address
  //
  Status = gDS->GetMemorySpaceDescriptor (Address, &Descriptor);
  if (EFI_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Mark the 4KB region for the PCI Express Bus/Dev/Func as EFI_RUNTIME_MEMORY so the OS
  // will allocate a virtual address range for the 4KB PCI Configuration Header.
  //
  Status = gDS->SetMemorySpaceAttributes (Address, EFI_PAGE_SIZE, Descriptor.Attributes | EFI_MEMORY_RUNTIME);
  if (EFI_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Grow the size of the registration table
  //
  NewTable = ReallocateRuntimePool (
               (mDxeRuntimePciSegmentLibNumberOfRuntimeRanges + 0) * sizeof (PCI_SEGMENT_RUNTIME_REGISTRATION_TABLE),
               (mDxeRuntimePciSegmentLibNumberOfRuntimeRanges + 1) * sizeof (PCI_SEGMENT_RUNTIME_REGISTRATION_TABLE),
               mDxeRuntimePciSegmentLibRegistrationTable
               );
  if (NewTable == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }

  mDxeRuntimePciSegmentLibRegistrationTable                                                                = NewTable;
  mDxeRuntimePciSegmentLibRegistrationTable[mDxeRuntimePciSegmentLibNumberOfRuntimeRanges].PhysicalAddress = Address;
  mDxeRuntimePciSegmentLibRegistrationTable[mDxeRuntimePciSegmentLibNumberOfRuntimeRanges].VirtualAddress  = Address;
  mDxeRuntimePciSegmentLibNumberOfRuntimeRanges++;

  return RETURN_SUCCESS;
}

/**
  Return the linear address for the physical address.

  @param  Address  The physical address.

  @retval The linear address.
**/
UINTN
PciSegmentLibVirtualAddress (
  IN UINTN  Address
  )
{
  UINTN  Index;

  //
  // If SetVirtualAddressMap() has not been called, then just return the physical address
  //
  if (!EfiGoneVirtual ()) {
    return Address;
  }

  //
  // See if there is a physical address match at the exact same index as the last address match
  //
  if (mDxeRuntimePciSegmentLibRegistrationTable[mDxeRuntimePciSegmentLibLastRuntimeRange].PhysicalAddress == (Address & (~(UINTN)EFI_PAGE_MASK))) {
    //
    // Convert the physical address to a virtual address and return the virtual address
    //
    return (Address & EFI_PAGE_MASK) + mDxeRuntimePciSegmentLibRegistrationTable[mDxeRuntimePciSegmentLibLastRuntimeRange].VirtualAddress;
  }

  //
  // Search the entire table for a physical address match
  //
  for (Index = 0; Index < mDxeRuntimePciSegmentLibNumberOfRuntimeRanges; Index++) {
    if (mDxeRuntimePciSegmentLibRegistrationTable[Index].PhysicalAddress == (Address & (~(UINTN)EFI_PAGE_MASK))) {
      //
      // Cache the matching index value
      //
      mDxeRuntimePciSegmentLibLastRuntimeRange = Index;
      //
      // Convert the physical address to a virtual address and return the virtual address
      //
      return (Address & EFI_PAGE_MASK) + mDxeRuntimePciSegmentLibRegistrationTable[Index].VirtualAddress;
    }
  }

  //
  // No match was found.  This is a critical error at OS runtime, so ASSERT() and force a breakpoint.
  //
  ASSERT (FALSE);
  CpuBreakpoint ();

  //
  // Return the physical address
  //
  return Address;
}
