/** @file
  This library implement library class DxeServiceTableLib.
  It produce EFI_DXE_SERVICE pointer in global variable gDS in library's constructure.
  
  A DXE driver can use gDS pointer to access services in EFI_DXE_SERVICE, if this
  DXE driver declare that use DxeServicesTableLib library class and link to this 
  library instance.

  Please attention this library instance can not be used util EFI_SYSTEM_TABLE was 
  initialized.
  
  This library contains contruct function to retrieve EFI_DXE_SERIVCE, this construct
  function will be invoked in DXE driver's autogen file.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <Guid/DxeServices.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>

//
// Cache copy of the DXE Services Table
//
EFI_DXE_SERVICES  *gDS      = NULL;

/**
  The constructor function caches the pointer of DXE Services Table.

  The constructor function caches the pointer of DXE Services Table.
  It will ASSERT() if that operation fails.
  It will ASSERT() if the pointer of DXE Services Table is NULL.
  It will always return EFI_SUCCESS.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
DxeServicesTableLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Cache copy of the DXE Services Table
  //
  Status = EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **) &gDS);
  ASSERT_EFI_ERROR (Status);
  ASSERT (gDS != NULL);

  return Status;
}
