/** @file
  Entry point to a Standalone MM driver.

Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016 - 2018, ARM Ltd. All rights reserved.<BR>
Copyright (c) 2018, Linaro, Limited. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmDriverEntryPoint.h>

/**
  The entry point of PE/COFF Image for a Standalone MM Driver.

  This function is the entry point for a Standalone MM Driver.
  This function must call ProcessLibraryConstructorList() and
  ProcessModuleEntryPointList().
  If the return status from ProcessModuleEntryPointList()
  is an error status, then ProcessLibraryDestructorList() must be called.
  The return value from ProcessModuleEntryPointList() is returned.
  If _gMmRevision is not zero and SystemTable->Hdr.Revision is
  less than _gMmRevision, then return EFI_INCOMPATIBLE_VERSION.

  @param  ImageHandle    The image handle of the Standalone MM Driver.
  @param  MmSystemTable  A pointer to the MM System Table.

  @retval  EFI_SUCCESS               The Standalone MM Driver exited normally.
  @retval  EFI_INCOMPATIBLE_VERSION  _gMmRevision is greater than
                                     MmSystemTable->Hdr.Revision.
  @retval  Other                     Return value from
                                     ProcessModuleEntryPointList().

**/
EFI_STATUS
EFIAPI
_ModuleEntryPoint (
  IN EFI_HANDLE               ImageHandle,
  IN IN EFI_MM_SYSTEM_TABLE   *MmSystemTable
  )
{
  EFI_STATUS                 Status;

  if (_gMmRevision != 0) {
    //
    // Make sure that the MM spec revision of the platform
    // is >= MM spec revision of the driver
    //
    if (MmSystemTable->Hdr.Revision < _gMmRevision) {
      return EFI_INCOMPATIBLE_VERSION;
    }
  }

  //
  // Call constructor for all libraries
  //
  ProcessLibraryConstructorList (ImageHandle, MmSystemTable);

  //
  // Call the driver entry point
  //
  Status = ProcessModuleEntryPointList (ImageHandle, MmSystemTable);

  //
  // If all of the drivers returned errors, then invoke all of the library destructors
  //
  if (EFI_ERROR (Status)) {
    ProcessLibraryDestructorList (ImageHandle, MmSystemTable);
  }

  //
  // Return the cumulative return status code from all of the driver entry points
  //
  return Status;
}
