/** @file
  GUID is used to define the signed section.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID introduced in PI Version 1.2.1.

**/

#ifndef __FIRMWARE_CONTENTS_SIGNED_GUID_H__
#define __FIRMWARE_CONTENTS_SIGNED_GUID_H__

#define EFI_FIRMWARE_CONTENTS_SIGNED_GUID \
   { 0xf9d89e8, 0x9259, 0x4f76, {0xa5, 0xaf, 0xc, 0x89, 0xe3, 0x40, 0x23, 0xdf } }

extern EFI_GUID  gEfiFirmwareContentsSignedGuid;

#endif
