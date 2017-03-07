/** @file
  Entry point to the DXE Core.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include <PiDxe.h>


#include <Library/DxeCoreEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>

//
// Cache copy of HobList pointer. 
// 
VOID *gHobList = NULL;

/**
  The entry point of PE/COFF Image for the DXE Core. 

  This function is the entry point for the DXE Core. This function is required to call
  ProcessModuleEntryPointList() and ProcessModuleEntryPointList() is never expected to return.
  The DXE Core is responsible for calling ProcessLibraryConstructorList() as soon as the EFI
  System Table and the image handle for the DXE Core itself have been established.
  If ProcessModuleEntryPointList() returns, then ASSERT() and halt the system.

  @param  HobStart  The pointer to the beginning of the HOB List passed in from the PEI Phase. 

**/
VOID
EFIAPI
_ModuleEntryPoint (
  IN VOID  *HobStart
  )
{
  //
  // Cache a pointer to the HobList
  //
  gHobList = HobStart;

  //
  // Call the DXE Core entry point
  //
  ProcessModuleEntryPointList (HobStart);

  //
  // Should never return
  //
  ASSERT(FALSE);
  CpuDeadLoop ();
}


/**
  Required by the EBC compiler and identical in functionality to _ModuleEntryPoint().

  This function is required to call _ModuleEntryPoint() passing in HobStart.

  @param  HobStart  The pointer to the beginning of the HOB List passed in from the PEI Phase. 

**/
VOID
EFIAPI
EfiMain (
  IN VOID  *HobStart
  )
{
  _ModuleEntryPoint (HobStart);
}
