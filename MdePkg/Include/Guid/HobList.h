/** @file
  GUIDs used for HOB List entries

  These GUIDs point the HOB List passed from PEI to DXE.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID introduced in PI Version 1.0.

**/

#ifndef __HOB_LIST_GUID_H__
#define __HOB_LIST_GUID_H__

#define HOB_LIST_GUID \
  { \
    0x7739f24c, 0x93d7, 0x11d4, {0x9a, 0x3a, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

extern EFI_GUID gEfiHobListGuid;

#endif
