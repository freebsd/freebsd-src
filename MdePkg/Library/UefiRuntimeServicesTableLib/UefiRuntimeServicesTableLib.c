/** @file
  UEFI Runtime Services Table Library.

  This library instance retrieve EFI_RUNTIME_SERVICES pointer from EFI system table
  in library's constructor.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>

EFI_RUNTIME_SERVICES  *gRT = NULL;

/**
  The constructor function caches the pointer of Runtime Services Table.

  The constructor function caches the pointer of Runtime Services Table.
  It will ASSERT() if the pointer of Runtime Services Table is NULL.
  It will always return EFI_SUCCESS.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiRuntimeServicesTableLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Cache pointer to the EFI Runtime Services Table
  //
  gRT = SystemTable->RuntimeServices;
  ASSERT (gRT != NULL);
  return EFI_SUCCESS;
}
