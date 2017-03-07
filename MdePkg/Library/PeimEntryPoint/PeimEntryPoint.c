/** @file
  Entry point to a PEIM.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include <PiPei.h>


#include <Library/PeimEntryPoint.h>
#include <Library/DebugLib.h>

/**
  The entry point of PE/COFF Image for a PEIM.

  This function is the entry point for a PEIM.  This function must call ProcessLibraryConstructorList() 
  and ProcessModuleEntryPointList().  The return value from ProcessModuleEntryPointList() is returned.
  If _gPeimRevision is not zero and PeiServices->Hdr.Revision is less than _gPeimRevison, then ASSERT().

  @param  FileHandle  Handle of the file being invoked. 
  @param  PeiServices Describes the list of possible PEI Services.

  @retval  EFI_SUCCESS   The PEIM executed normally.
  @retval  !EFI_SUCCESS  The PEIM failed to execute normally.
**/
EFI_STATUS
EFIAPI
_ModuleEntryPoint (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  if (_gPeimRevision != 0) {
    //
    // Make sure that the PEI spec revision of the platform is >= PEI spec revision of the driver
    //
    ASSERT ((*PeiServices)->Hdr.Revision >= _gPeimRevision);
  }

  //
  // Call constructor for all libraries
  //
  ProcessLibraryConstructorList (FileHandle, PeiServices);

  //
  // Call the driver entry point
  //
  return ProcessModuleEntryPointList (FileHandle, PeiServices);
}


/**
  Required by the EBC compiler and identical in functionality to _ModuleEntryPoint().
  
  This function is required to call _ModuleEntryPoint() passing in FileHandle and PeiServices.

  @param  FileHandle  Handle of the file being invoked. 
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS  The PEIM executed normally.
  @retval !EFI_SUCCESS The PEIM failed to execute normally.

**/
EFI_STATUS
EFIAPI
EfiMain (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  return _ModuleEntryPoint (FileHandle, PeiServices);
}
