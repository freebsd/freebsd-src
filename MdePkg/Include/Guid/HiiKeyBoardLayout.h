/** @file

  HII keyboard layout GUID as defined in UEFI2.1 specification

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in UEFI 2.1 spec.

**/

#ifndef __HII_KEYBOARD_LAYOUT_GUID_H__
#define __HII_KEYBOARD_LAYOUT_GUID_H__

#define EFI_HII_SET_KEYBOARD_LAYOUT_EVENT_GUID \
  { 0x14982a4f, 0xb0ed, 0x45b8, { 0xa8, 0x11, 0x5a, 0x7a, 0x9b, 0xc2, 0x32, 0xdf }}

extern EFI_GUID gEfiHiiKeyBoardLayoutGuid;

#endif
