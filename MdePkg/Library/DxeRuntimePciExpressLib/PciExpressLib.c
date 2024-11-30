/** @file
  Functions in this library instance make use of MMIO functions in IoLib to
  access memory mapped PCI configuration space.

  All assertions for I/O operations are handled in MMIO functions in the IoLib
  Library.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Guid/EventGroup.h>

#include <Library/BaseLib.h>
#include <Library/PciExpressLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>

/**
 Assert the validity of a PCI address. A valid PCI address should contain 1's
 only in the low 28 bits.

 @param A The address to validate.

**/
#define ASSERT_INVALID_PCI_ADDRESS(A) \
 ASSERT (((A) & ~0xfffffff) == 0)

///
/// Define table for mapping PCI Express MMIO physical addresses to virtual addresses at OS runtime
///
typedef struct {
  UINTN    PhysicalAddress;
  UINTN    VirtualAddress;
} PCI_EXPRESS_RUNTIME_REGISTRATION_TABLE;

///
/// Set Virtual Address Map Event
///
EFI_EVENT  mDxeRuntimePciExpressLibVirtualNotifyEvent = NULL;

///
/// Module global that contains the base physical address and size of the PCI Express MMIO range.
///
UINTN  mDxeRuntimePciExpressLibPciExpressBaseAddress = 0;
UINTN  mDxeRuntimePciExpressLibPciExpressBaseSize    = 0;

///
/// The number of PCI devices that have been registered for runtime access.
///
UINTN  mDxeRuntimePciExpressLibNumberOfRuntimeRanges = 0;

///
/// The table of PCI devices that have been registered for runtime access.
///
PCI_EXPRESS_RUNTIME_REGISTRATION_TABLE  *mDxeRuntimePciExpressLibRegistrationTable = NULL;

///
/// The table index of the most recent virtual address lookup.
///
UINTN  mDxeRuntimePciExpressLibLastRuntimeRange = 0;

/**
  Convert the physical PCI Express MMIO addresses for all registered PCI devices
  to virtual addresses.

  @param[in]    Event   The event that is being processed.
  @param[in]    Context The Event Context.
**/
VOID
EFIAPI
DxeRuntimePciExpressLibVirtualNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN  Index;

  //
  // If there have been no runtime registrations, then just return
  //
  if (mDxeRuntimePciExpressLibRegistrationTable == NULL) {
    return;
  }

  //
  // Convert physical addresses associated with the set of registered PCI devices to
  // virtual addresses.
  //
  for (Index = 0; Index < mDxeRuntimePciExpressLibNumberOfRuntimeRanges; Index++) {
    EfiConvertPointer (0, (VOID **)&(mDxeRuntimePciExpressLibRegistrationTable[Index].VirtualAddress));
  }

  //
  // Convert table pointer that is allocated from EfiRuntimeServicesData to a virtual address.
  //
  EfiConvertPointer (0, (VOID **)&mDxeRuntimePciExpressLibRegistrationTable);
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
DxeRuntimePciExpressLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Cache the physical address of the PCI Express MMIO range into a module global variable
  //
  mDxeRuntimePciExpressLibPciExpressBaseAddress = (UINTN)PcdGet64 (PcdPciExpressBaseAddress);
  mDxeRuntimePciExpressLibPciExpressBaseSize    = (UINTN)PcdGet64 (PcdPciExpressBaseSize);

  //
  // Register SetVirtualAddressMap () notify function
  //
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  DxeRuntimePciExpressLibVirtualNotify,
                  NULL,
                  &mDxeRuntimePciExpressLibVirtualNotifyEvent
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
DxeRuntimePciExpressLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // If one or more PCI devices have been registered for runtime access, then
  // free the registration table.
  //
  if (mDxeRuntimePciExpressLibRegistrationTable != NULL) {
    FreePool (mDxeRuntimePciExpressLibRegistrationTable);
  }

  //
  // Close the Set Virtual Address Map event
  //
  Status = gBS->CloseEvent (mDxeRuntimePciExpressLibVirtualNotifyEvent);
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Gets the base address of PCI Express.

  This internal functions retrieves PCI Express Base Address via a PCD entry
  PcdPciExpressBaseAddress.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address   The address that encodes the PCI Bus, Device, Function and Register.

  @retval (UINTN)-1 Invalid PCI address.
  @retval other     The base address of PCI Express.

**/
UINTN
GetPciExpressAddress (
  IN UINTN  Address
  )
{
  UINTN  Index;

  //
  // Make sure Address is valid
  //
  ASSERT_INVALID_PCI_ADDRESS (Address);

  //
  // Make sure the Address is in MMCONF address space
  //
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINTN)-1;
  }

  //
  // Convert Address to a physical address in the MMIO PCI Express range
  //
  Address += mDxeRuntimePciExpressLibPciExpressBaseAddress;

  //
  // If SetVirtualAddressMap() has not been called, then just return the physical address
  //
  if (!EfiGoneVirtual ()) {
    return Address;
  }

  //
  // See if there is a physical address match at the exact same index as the last address match
  //
  if (mDxeRuntimePciExpressLibRegistrationTable[mDxeRuntimePciExpressLibLastRuntimeRange].PhysicalAddress == (Address & (~0x00000fff))) {
    //
    // Convert the physical address to a virtual address and return the virtual address
    //
    return (Address & 0x00000fff) + mDxeRuntimePciExpressLibRegistrationTable[mDxeRuntimePciExpressLibLastRuntimeRange].VirtualAddress;
  }

  //
  // Search the entire table for a physical address match
  //
  for (Index = 0; Index < mDxeRuntimePciExpressLibNumberOfRuntimeRanges; Index++) {
    if (mDxeRuntimePciExpressLibRegistrationTable[Index].PhysicalAddress == (Address & (~0x00000fff))) {
      //
      // Cache the matching index value
      //
      mDxeRuntimePciExpressLibLastRuntimeRange = Index;
      //
      // Convert the physical address to a virtual address and return the virtual address
      //
      return (Address & 0x00000fff) + mDxeRuntimePciExpressLibRegistrationTable[Index].VirtualAddress;
    }
  }

  //
  // No match was found.  This is a critical error at OS runtime, so ASSERT() and force a breakpoint.
  //
  CpuBreakpoint ();

  //
  // Return the physical address
  //
  return Address;
}

/**
  Registers a PCI device so PCI configuration registers may be accessed after
  SetVirtualAddressMap().

  Registers the PCI device specified by Address so all the PCI configuration
  registers associated with that PCI device may be accessed after SetVirtualAddressMap()
  is called.

  If Address > 0x0FFFFFFF, then ASSERT().

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
PciExpressRegisterForRuntimeAccess (
  IN UINTN  Address
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  UINTN                            Index;
  VOID                             *NewTable;

  //
  // Return an error if this function is called after ExitBootServices().
  //
  if (EfiAtRuntime ()) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Make sure Address is valid
  //
  ASSERT_INVALID_PCI_ADDRESS (Address);

  //
  // Make sure the Address is in MMCONF address space
  //
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert Address to a physical address in the MMIO PCI Express range
  // at the beginning of the PCI Configuration header for the specified
  // PCI Bus/Dev/Func
  //
  Address = GetPciExpressAddress (Address & 0x0ffff000);

  //
  // See if Address has already been registered for runtime access
  //
  for (Index = 0; Index < mDxeRuntimePciExpressLibNumberOfRuntimeRanges; Index++) {
    if (mDxeRuntimePciExpressLibRegistrationTable[Index].PhysicalAddress == Address) {
      return RETURN_SUCCESS;
    }
  }

  //
  // Get the GCD Memory Descriptor for the PCI Express Bus/Dev/Func specified by Address
  //
  Status = gDS->GetMemorySpaceDescriptor (Address, &Descriptor);
  if (EFI_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Mark the 4KB region for the PCI Express Bus/Dev/Func as EFI_RUNTIME_MEMORY so the OS
  // will allocate a virtual address range for the 4KB PCI Configuration Header.
  //
  Status = gDS->SetMemorySpaceAttributes (Address, 0x1000, Descriptor.Attributes | EFI_MEMORY_RUNTIME);
  if (EFI_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Grow the size of the registration table
  //
  NewTable = ReallocateRuntimePool (
               (mDxeRuntimePciExpressLibNumberOfRuntimeRanges + 0) * sizeof (PCI_EXPRESS_RUNTIME_REGISTRATION_TABLE),
               (mDxeRuntimePciExpressLibNumberOfRuntimeRanges + 1) * sizeof (PCI_EXPRESS_RUNTIME_REGISTRATION_TABLE),
               mDxeRuntimePciExpressLibRegistrationTable
               );
  if (NewTable == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }

  mDxeRuntimePciExpressLibRegistrationTable                                                                = NewTable;
  mDxeRuntimePciExpressLibRegistrationTable[mDxeRuntimePciExpressLibNumberOfRuntimeRanges].PhysicalAddress = Address;
  mDxeRuntimePciExpressLibRegistrationTable[mDxeRuntimePciExpressLibNumberOfRuntimeRanges].VirtualAddress  = Address;
  mDxeRuntimePciExpressLibNumberOfRuntimeRanges++;

  return RETURN_SUCCESS;
}

/**
  Reads an 8-bit PCI configuration register.

  Reads and returns the 8-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @retval 0xFF  Invalid PCI address.
  @retval other The read value from the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressRead8 (
  IN      UINTN  Address
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address);
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioRead8 (GetPciExpressAddress (Address));
}

/**
  Writes an 8-bit PCI configuration register.

  Writes the 8-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressWrite8 (
  IN      UINTN  Address,
  IN      UINT8  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioWrite8 (GetPciExpressAddress (Address), Value);
}

/**
  Performs a bitwise OR of an 8-bit PCI configuration register with
  an 8-bit value.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 8-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressOr8 (
  IN      UINTN  Address,
  IN      UINT8  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioOr8 (GetPciExpressAddress (Address), OrData);
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 8-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @retval 0xFF  Invalid PCI address.
  @retval other  The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressAnd8 (
  IN      UINTN  Address,
  IN      UINT8  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioAnd8 (GetPciExpressAddress (Address), AndData);
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value, followed a  bitwise OR with another 8-bit value.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 8-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressAndThenOr8 (
  IN      UINTN  Address,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioAndThenOr8 (
           GetPciExpressAddress (Address),
           AndData,
           OrData
           );
}

/**
  Reads a bit field of a PCI configuration register.

  Reads the bit field in an 8-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @retval 0xFF  Invalid PCI address.
  @retval other The value of the bit field read from the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressBitFieldRead8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioBitFieldRead8 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit
           );
}

/**
  Writes a bit field to a PCI configuration register.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  8-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     The new value of the bit field.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressBitFieldWrite8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioBitFieldWrite8 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           Value
           );
}

/**
  Reads a bit field in an 8-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 8-bit port.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 8-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with the PCI configuration register.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressBitFieldOr8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioBitFieldOr8 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Reads a bit field in an 8-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 8-bit register.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 8-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the PCI configuration register.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressBitFieldAnd8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioBitFieldAnd8 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData
           );
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  8-bit port.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 8-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @retval 0xFF  Invalid PCI address.
  @retval other The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
PciExpressBitFieldAndThenOr8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT8)-1;
  }

  return MmioBitFieldAndThenOr8 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData,
           OrData
           );
}

/**
  Reads a 16-bit PCI configuration register.

  Reads and returns the 16-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The read value from the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressRead16 (
  IN      UINTN  Address
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioRead16 (GetPciExpressAddress (Address));
}

/**
  Writes a 16-bit PCI configuration register.

  Writes the 16-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressWrite16 (
  IN      UINTN   Address,
  IN      UINT16  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioWrite16 (GetPciExpressAddress (Address), Value);
}

/**
  Performs a bitwise OR of a 16-bit PCI configuration register with
  a 16-bit value.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 16-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressOr16 (
  IN      UINTN   Address,
  IN      UINT16  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioOr16 (GetPciExpressAddress (Address), OrData);
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 16-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressAnd16 (
  IN      UINTN   Address,
  IN      UINT16  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioAnd16 (GetPciExpressAddress (Address), AndData);
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value, followed a  bitwise OR with another 16-bit value.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 16-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressAndThenOr16 (
  IN      UINTN   Address,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioAndThenOr16 (
           GetPciExpressAddress (Address),
           AndData,
           OrData
           );
}

/**
  Reads a bit field of a PCI configuration register.

  Reads the bit field in a 16-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value of the bit field read from the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressBitFieldRead16 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioBitFieldRead16 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit
           );
}

/**
  Writes a bit field to a PCI configuration register.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  16-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     The new value of the bit field.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressBitFieldWrite16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioBitFieldWrite16 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           Value
           );
}

/**
  Reads a bit field in a 16-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 16-bit port.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 16-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with the PCI configuration register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressBitFieldOr16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioBitFieldOr16 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Reads a bit field in a 16-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 16-bit register.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 16-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the PCI configuration register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressBitFieldAnd16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioBitFieldAnd16 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData
           );
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  16-bit port.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 16-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
PciExpressBitFieldAndThenOr16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT16)-1;
  }

  return MmioBitFieldAndThenOr16 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData,
           OrData
           );
}

/**
  Reads a 32-bit PCI configuration register.

  Reads and returns the 32-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.

  @retval 0xFFFF  Invalid PCI address.
  @retval other   The read value from the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressRead32 (
  IN      UINTN  Address
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioRead32 (GetPciExpressAddress (Address));
}

/**
  Writes a 32-bit PCI configuration register.

  Writes the 32-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressWrite32 (
  IN      UINTN   Address,
  IN      UINT32  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioWrite32 (GetPciExpressAddress (Address), Value);
}

/**
  Performs a bitwise OR of a 32-bit PCI configuration register with
  a 32-bit value.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 32-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressOr32 (
  IN      UINTN   Address,
  IN      UINT32  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioOr32 (GetPciExpressAddress (Address), OrData);
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 32-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressAnd32 (
  IN      UINTN   Address,
  IN      UINT32  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioAnd32 (GetPciExpressAddress (Address), AndData);
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value, followed a  bitwise OR with another 32-bit value.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 32-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressAndThenOr32 (
  IN      UINTN   Address,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioAndThenOr32 (
           GetPciExpressAddress (Address),
           AndData,
           OrData
           );
}

/**
  Reads a bit field of a PCI configuration register.

  Reads the bit field in a 32-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value of the bit field read from the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressBitFieldRead32 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioBitFieldRead32 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit
           );
}

/**
  Writes a bit field to a PCI configuration register.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  32-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     The new value of the bit field.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressBitFieldWrite32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  Value
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioBitFieldWrite32 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           Value
           );
}

/**
  Reads a bit field in a 32-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 32-bit port.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 32-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the PCI configuration register.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressBitFieldOr32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioBitFieldOr32 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           OrData
           );
}

/**
  Reads a bit field in a 32-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 32-bit register.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 32-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the PCI configuration register.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressBitFieldAnd32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioBitFieldAnd32 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData
           );
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  32-bit port.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 32-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
PciExpressBitFieldAndThenOr32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  if (Address >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINT32)-1;
  }

  return MmioBitFieldAndThenOr32 (
           GetPciExpressAddress (Address),
           StartBit,
           EndBit,
           AndData,
           OrData
           );
}

/**
  Reads a range of PCI configuration registers into a caller supplied buffer.

  Reads the range of PCI configuration registers specified by StartAddress and
  Size into the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be read. Size is
  returned. When possible 32-bit PCI configuration read cycles are used to read
  from StartAddress to StartAddress + Size. Due to alignment restrictions, 8-bit
  and 16-bit PCI configuration read cycles may be used at the beginning and the
  end of the range.

  If StartAddress > 0x0FFFFFFF, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  The starting address that encodes the PCI Bus, Device,
                        Function and Register.
  @param  Size          The size in bytes of the transfer.
  @param  Buffer        The pointer to a buffer receiving the data read.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      Size read data from StartAddress.

**/
UINTN
EFIAPI
PciExpressReadBuffer (
  IN      UINTN  StartAddress,
  IN      UINTN  Size,
  OUT     VOID   *Buffer
  )
{
  UINTN  ReturnValue;

  //
  // Make sure Address is valid
  //
  ASSERT_INVALID_PCI_ADDRESS (StartAddress);
  ASSERT (((StartAddress & 0xFFF) + Size) <= 0x1000);

  //
  // Make sure the Address is in MMCONF address space
  //
  if (StartAddress >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINTN)-1;
  }

  if (Size == 0) {
    return Size;
  }

  ASSERT (Buffer != NULL);

  //
  // Save Size for return
  //
  ReturnValue = Size;

  if ((StartAddress & 1) != 0) {
    //
    // Read a byte if StartAddress is byte aligned
    //
    *(volatile UINT8 *)Buffer = PciExpressRead8 (StartAddress);
    StartAddress             += sizeof (UINT8);
    Size                     -= sizeof (UINT8);
    Buffer                    = (UINT8 *)Buffer + 1;
  }

  if ((Size >= sizeof (UINT16)) && ((StartAddress & 2) != 0)) {
    //
    // Read a word if StartAddress is word aligned
    //
    WriteUnaligned16 ((UINT16 *)Buffer, (UINT16)PciExpressRead16 (StartAddress));

    StartAddress += sizeof (UINT16);
    Size         -= sizeof (UINT16);
    Buffer        = (UINT16 *)Buffer + 1;
  }

  while (Size >= sizeof (UINT32)) {
    //
    // Read as many double words as possible
    //
    WriteUnaligned32 ((UINT32 *)Buffer, (UINT32)PciExpressRead32 (StartAddress));

    StartAddress += sizeof (UINT32);
    Size         -= sizeof (UINT32);
    Buffer        = (UINT32 *)Buffer + 1;
  }

  if (Size >= sizeof (UINT16)) {
    //
    // Read the last remaining word if exist
    //
    WriteUnaligned16 ((UINT16 *)Buffer, (UINT16)PciExpressRead16 (StartAddress));
    StartAddress += sizeof (UINT16);
    Size         -= sizeof (UINT16);
    Buffer        = (UINT16 *)Buffer + 1;
  }

  if (Size >= sizeof (UINT8)) {
    //
    // Read the last remaining byte if exist
    //
    *(volatile UINT8 *)Buffer = PciExpressRead8 (StartAddress);
  }

  return ReturnValue;
}

/**
  Copies the data in a caller supplied buffer to a specified range of PCI
  configuration space.

  Writes the range of PCI configuration registers specified by StartAddress and
  Size from the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be written. Size is
  returned. When possible 32-bit PCI configuration write cycles are used to
  write from StartAddress to StartAddress + Size. Due to alignment restrictions,
  8-bit and 16-bit PCI configuration write cycles may be used at the beginning
  and the end of the range.

  If StartAddress > 0x0FFFFFFF, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  The starting address that encodes the PCI Bus, Device,
                        Function and Register.
  @param  Size          The size in bytes of the transfer.
  @param  Buffer        The pointer to a buffer containing the data to write.

  @retval 0xFFFFFFFF Invalid PCI address.
  @retval other      Size written to StartAddress.

**/
UINTN
EFIAPI
PciExpressWriteBuffer (
  IN      UINTN  StartAddress,
  IN      UINTN  Size,
  IN      VOID   *Buffer
  )
{
  UINTN  ReturnValue;

  //
  // Make sure Address is valid
  //
  ASSERT_INVALID_PCI_ADDRESS (StartAddress);
  ASSERT (((StartAddress & 0xFFF) + Size) <= 0x1000);

  //
  // Make sure the Address is in MMCONF address space
  //
  if (StartAddress >= mDxeRuntimePciExpressLibPciExpressBaseSize) {
    return (UINTN)-1;
  }

  if (Size == 0) {
    return 0;
  }

  ASSERT (Buffer != NULL);

  //
  // Save Size for return
  //
  ReturnValue = Size;

  if ((StartAddress & 1) != 0) {
    //
    // Write a byte if StartAddress is byte aligned
    //
    PciExpressWrite8 (StartAddress, *(UINT8 *)Buffer);
    StartAddress += sizeof (UINT8);
    Size         -= sizeof (UINT8);
    Buffer        = (UINT8 *)Buffer + 1;
  }

  if ((Size >= sizeof (UINT16)) && ((StartAddress & 2) != 0)) {
    //
    // Write a word if StartAddress is word aligned
    //
    PciExpressWrite16 (StartAddress, ReadUnaligned16 ((UINT16 *)Buffer));
    StartAddress += sizeof (UINT16);
    Size         -= sizeof (UINT16);
    Buffer        = (UINT16 *)Buffer + 1;
  }

  while (Size >= sizeof (UINT32)) {
    //
    // Write as many double words as possible
    //
    PciExpressWrite32 (StartAddress, ReadUnaligned32 ((UINT32 *)Buffer));
    StartAddress += sizeof (UINT32);
    Size         -= sizeof (UINT32);
    Buffer        = (UINT32 *)Buffer + 1;
  }

  if (Size >= sizeof (UINT16)) {
    //
    // Write the last remaining word if exist
    //
    PciExpressWrite16 (StartAddress, ReadUnaligned16 ((UINT16 *)Buffer));
    StartAddress += sizeof (UINT16);
    Size         -= sizeof (UINT16);
    Buffer        = (UINT16 *)Buffer + 1;
  }

  if (Size >= sizeof (UINT8)) {
    //
    // Write the last remaining byte if exist
    //
    PciExpressWrite8 (StartAddress, *(UINT8 *)Buffer);
  }

  return ReturnValue;
}
