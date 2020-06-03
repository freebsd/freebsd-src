/** @file
  UEFI Dxe DebugLib constructor that prevent some debug service after ExitBootServices event,
  because some pointer is nulled at that phase.

  Copyright (c) 2018, Microsoft Corporation
  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

//
// BOOLEAN value to indicate if it is at the post ExitBootServices pahse
//
BOOLEAN     mPostEBS = FALSE;

static EFI_EVENT   mExitBootServicesEvent;

//
// Pointer to SystemTable
// This library instance may have a cycle consume with UefiBootServicesTableLib
// because of the constructors.
//
EFI_SYSTEM_TABLE      *mDebugST;

/**
  This routine sets the mPostEBS for exit boot servies true
  to prevent DebugPort protocol dereferences when the pointer is nulled.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
ExitBootServicesCallback (
  EFI_EVENT   Event,
  VOID*       Context
  )
{
  mPostEBS = TRUE;
  return;
}

/**
  The constructor gets the pointers to the system table.
  And create a event to indicate it is after ExitBootServices.

  @param  ImageHandle     The firmware allocated handle for the EFI image.
  @param  SystemTable     A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
DxeDebugLibConstructor(
  IN EFI_HANDLE                 ImageHandle,
  IN EFI_SYSTEM_TABLE           *SystemTable
  )
{
  mDebugST = SystemTable;

  SystemTable->BootServices->CreateEvent (
                                EVT_SIGNAL_EXIT_BOOT_SERVICES,
                                TPL_NOTIFY,
                                ExitBootServicesCallback,
                                NULL,
                                &mExitBootServicesEvent
                                );

  return EFI_SUCCESS;
}

/**
  The destructor closes Exit Boot Services Event.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The destructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
DxeDebugLibDestructor(
  IN EFI_HANDLE                 ImageHandle,
  IN EFI_SYSTEM_TABLE           *SystemTable
  )
{
  if (mExitBootServicesEvent != NULL) {
    SystemTable->BootServices->CloseEvent (mExitBootServicesEvent);
  }

  return EFI_SUCCESS;
}
