/** @file
  EFI_HII_PACKAGE_LIST_PROTOCOL as defined in UEFI 2.1.
  Boot service LoadImage() installs EFI_HII_PACKAGE_LIST_PROTOCOL on the handle
  if the image contains a custom PE/COFF resource with the type 'HII'.
  The protocol's interface pointer points to the HII package list, which is
  contained in the resource's data.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HII_PACKAGE_LIST_H__
#define __HII_PACKAGE_LIST_H__

#define EFI_HII_PACKAGE_LIST_PROTOCOL_GUID \
  { 0x6a1ee763, 0xd47a, 0x43b4, {0xaa, 0xbe, 0xef, 0x1d, 0xe2, 0xab, 0x56, 0xfc}}

typedef EFI_HII_PACKAGE_LIST_HEADER *EFI_HII_PACKAGE_LIST_PROTOCOL;

extern EFI_GUID  gEfiHiiPackageListProtocolGuid;

#endif
