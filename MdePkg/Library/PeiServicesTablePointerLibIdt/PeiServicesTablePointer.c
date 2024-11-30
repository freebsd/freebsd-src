/** @file
  PEI Services Table Pointer Library for IA-32 and x64.

  According to PI specification, the peiservice pointer is stored prior at IDT
  table in IA32 and x64 architecture.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/BaseLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Retrieves the cached value of the PEI Services Table pointer.

  Returns the cached value of the PEI Services Table pointer in a CPU specific manner
  as specified in the CPU binding section of the Platform Initialization Pre-EFI
  Initialization Core Interface Specification.

  If the cached PEI Services Table pointer is NULL, then ASSERT().

  @return  The pointer to PeiServices.

**/
CONST EFI_PEI_SERVICES **
EFIAPI
GetPeiServicesTablePointer (
  VOID
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  IA32_DESCRIPTOR         Idtr;

  AsmReadIdtr (&Idtr);
  PeiServices = (CONST EFI_PEI_SERVICES **)(*(UINTN *)(Idtr.Base - sizeof (UINTN)));
  ASSERT (PeiServices != NULL);
  return PeiServices;
}

/**
  Caches a pointer PEI Services Table.

  Caches the pointer to the PEI Services Table specified by PeiServicesTablePointer
  in a CPU specific manner as specified in the CPU binding section of the Platform Initialization
  Pre-EFI Initialization Core Interface Specification.
  The function set the pointer of PEI services immediately preceding the IDT table
  according to PI specification.

  If PeiServicesTablePointer is NULL, then ASSERT().

  @param    PeiServicesTablePointer   The address of PeiServices pointer.
**/
VOID
EFIAPI
SetPeiServicesTablePointer (
  IN CONST EFI_PEI_SERVICES  **PeiServicesTablePointer
  )
{
  IA32_DESCRIPTOR  Idtr;

  ASSERT (PeiServicesTablePointer != NULL);
  AsmReadIdtr (&Idtr);
  (*(UINTN *)(Idtr.Base - sizeof (UINTN))) = (UINTN)PeiServicesTablePointer;
}

/**
  Perform CPU specific actions required to migrate the PEI Services Table
  pointer from temporary RAM to permanent RAM.

  For IA32 CPUs, the PEI Services Table pointer is stored in the 4 bytes
  immediately preceding the Interrupt Descriptor Table (IDT) in memory.
  For X64 CPUs, the PEI Services Table pointer is stored in the 8 bytes
  immediately preceding the Interrupt Descriptor Table (IDT) in memory.
  For Itanium and ARM CPUs, a the PEI Services Table Pointer is stored in
  a dedicated CPU register.  This means that there is no memory storage
  associated with storing the PEI Services Table pointer, so no additional
  migration actions are required for Itanium or ARM CPUs.

  If The cached PEI Services Table pointer is NULL, then ASSERT().
  If the permanent memory is allocated failed, then ASSERT().
**/
VOID
EFIAPI
MigratePeiServicesTablePointer (
  VOID
  )
{
  EFI_STATUS              Status;
  IA32_DESCRIPTOR         Idtr;
  EFI_PHYSICAL_ADDRESS    IdtBase;
  CONST EFI_PEI_SERVICES  **PeiServices;

  //
  // Get PEI Services Table pointer
  //
  AsmReadIdtr (&Idtr);
  PeiServices = (CONST EFI_PEI_SERVICES **)(*(UINTN *)(Idtr.Base - sizeof (UINTN)));
  ASSERT (PeiServices != NULL);
  //
  // Allocate the permanent memory.
  //
  Status = (*PeiServices)->AllocatePages (
                             PeiServices,
                             EfiBootServicesCode,
                             EFI_SIZE_TO_PAGES (Idtr.Limit + 1 + sizeof (UINTN)),
                             &IdtBase
                             );
  ASSERT_EFI_ERROR (Status);
  //
  // Idt table needs to be migrated into memory.
  //
  CopyMem ((VOID *)(UINTN)IdtBase, (VOID *)(Idtr.Base - sizeof (UINTN)), Idtr.Limit + 1 + sizeof (UINTN));
  Idtr.Base = (UINTN)IdtBase + sizeof (UINTN);
  AsmWriteIdtr (&Idtr);

  return;
}
