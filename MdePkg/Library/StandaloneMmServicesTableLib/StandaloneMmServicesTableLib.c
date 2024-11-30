/** @file
  MM Services Table Library.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2018, Linaro, Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>
#include <Library/MmServicesTableLib.h>
#include <Library/DebugLib.h>

EFI_MM_SYSTEM_TABLE  *gMmst = NULL;

/**
  The constructor function caches the pointer of the MM Services Table.

  @param  ImageHandle     The firmware allocated handle for the EFI image.
  @param  MmSystemTable   A pointer to the MM System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
StandaloneMmServicesTableLibConstructor (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  gMmst = MmSystemTable;
  ASSERT (gMmst != NULL);
  return EFI_SUCCESS;
}
