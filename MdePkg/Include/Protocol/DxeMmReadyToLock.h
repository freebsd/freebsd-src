/** @file
  DXE MM Ready To Lock protocol introduced in the PI 1.5 specification.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _DXE_MM_READY_TO_LOCK_H_
#define _DXE_MM_READY_TO_LOCK_H_

#define EFI_DXE_MM_READY_TO_LOCK_PROTOCOL_GUID \
  { \
    0x60ff8964, 0xe906, 0x41d0, { 0xaf, 0xed, 0xf2, 0x41, 0xe9, 0x74, 0xe0, 0x8e } \
  }

extern EFI_GUID  gEfiDxeMmReadyToLockProtocolGuid;

#endif
