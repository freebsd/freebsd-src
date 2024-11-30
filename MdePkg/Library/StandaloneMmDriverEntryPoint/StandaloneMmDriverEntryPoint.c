/** @file
  Entry point to a Standalone MM driver.

Copyright (c) 2015 - 2021, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016 - 2018, ARM Ltd. All rights reserved.<BR>
Copyright (c) 2018, Linaro, Limited. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Protocol/LoadedImage.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmDriverEntryPoint.h>

/**
  Unloads an image from memory.

  This function is a callback that a driver registers to do cleanup
  when the UnloadImage boot service function is called.

  @param  ImageHandle The handle to the image to unload.

  @return Status returned by all unload().

**/
EFI_STATUS
EFIAPI
_DriverUnloadHandler (
  EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  //
  // If an UnloadImage() handler is specified, then call it
  //
  Status = ProcessModuleUnloadList (ImageHandle);

  //
  // If the driver specific unload handler does not return an error, then call all of the
  // library destructors.  If the unload handler returned an error, then the driver can not be
  // unloaded, and the library destructors should not be called
  //
  if (!EFI_ERROR (Status)) {
    ProcessLibraryDestructorList (ImageHandle, gMmst);
  }

  //
  // Return the status from the driver specific unload handler
  //
  return Status;
}

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
  IN EFI_HANDLE              ImageHandle,
  IN IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;

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
  //  Install unload handler...
  //
  if (_gDriverUnloadImageCount != 0) {
    Status = gMmst->MmHandleProtocol (
                      ImageHandle,
                      &gEfiLoadedImageProtocolGuid,
                      (VOID **)&LoadedImage
                      );
    ASSERT_EFI_ERROR (Status);
    LoadedImage->Unload = _DriverUnloadHandler;
  }

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
