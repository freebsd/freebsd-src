/** @file
  Boot Device Selection Architectural Protocol as defined in PI spec Volume 2 DXE

  When the DXE core is done it calls the BDS via this protocol.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_BDS_H__
#define __ARCH_PROTOCOL_BDS_H__

///
/// Global ID for the BDS Architectural Protocol
///
#define EFI_BDS_ARCH_PROTOCOL_GUID \
  { 0x665E3FF6, 0x46CC, 0x11d4, {0x9A, 0x38, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

///
/// Declare forward reference for the BDS Architectural Protocol
///
typedef struct _EFI_BDS_ARCH_PROTOCOL   EFI_BDS_ARCH_PROTOCOL;

/**
  This function uses policy data from the platform to determine what operating
  system or system utility should be loaded and invoked.  This function call
  also optionally make the use of user input to determine the operating system
  or system utility to be loaded and invoked.  When the DXE Core has dispatched
  all the drivers on the dispatch queue, this function is called.  This
  function will attempt to connect the boot devices required to load and invoke
  the selected operating system or system utility.  During this process,
  additional firmware volumes may be discovered that may contain addition DXE
  drivers that can be dispatched by the DXE Core.   If a boot device cannot be
  fully connected, this function calls the DXE Service Dispatch() to allow the
  DXE drivers from any newly discovered firmware volumes to be dispatched.
  Then the boot device connection can be attempted again.  If the same boot
  device connection operation fails twice in a row, then that boot device has
  failed, and should be skipped.  This function should never return.

  @param  This             The EFI_BDS_ARCH_PROTOCOL instance.

  @return None.

**/
typedef
VOID
(EFIAPI *EFI_BDS_ENTRY)(
  IN EFI_BDS_ARCH_PROTOCOL  *This
  );

///
/// The EFI_BDS_ARCH_PROTOCOL transfers control from DXE to an operating
/// system or a system utility.  If there are not enough drivers initialized
/// when this protocol is used to access the required boot device(s), then
/// this protocol should add drivers to the dispatch queue and return control
/// back to the dispatcher.  Once the required boot devices are available, then
/// the boot device can be used to load and invoke an OS or a system utility.
///
struct _EFI_BDS_ARCH_PROTOCOL {
  EFI_BDS_ENTRY Entry;
};

extern EFI_GUID gEfiBdsArchProtocolGuid;

#endif
