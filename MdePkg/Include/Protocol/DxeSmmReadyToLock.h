/** @file
  DXE SMM Ready To Lock protocol introduced in the PI 1.2 specification.

  According to PI 1.4a specification, this UEFI protocol indicates that
  resources and services that should not be used by the third party code
  are about to be locked.
  This protocol is a mandatory protocol published by PI platform code.
  This protocol in tandem with the End of DXE Event facilitates transition
  of the platform from the environment where all of the components are
  under the authority of the platform manufacturer to the environment where
  third party extensible modules such as UEFI drivers and UEFI applications
  are executed. The protocol is published immediately after signaling of the
  End of DXE Event. PI modules that need to lock or protect their resources
  in anticipation of the invocation of 3rd party extensible modules should
  register for notification on installation of this protocol and effect the
  appropriate protections in their notification handlers. For example, PI
  platform code may choose to use notification handler to lock SMM by invoking
  EFI_SMM_ACCESS2_PROTOCOL.Lock() function.

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _DXE_SMM_READY_TO_LOCK_H_
#define _DXE_SMM_READY_TO_LOCK_H_

#include <Protocol/DxeMmReadyToLock.h>

#define EFI_DXE_SMM_READY_TO_LOCK_PROTOCOL_GUID EFI_DXE_MM_READY_TO_LOCK_PROTOCOL_GUID

extern EFI_GUID gEfiDxeSmmReadyToLockProtocolGuid;

#endif
