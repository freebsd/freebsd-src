/** @file
  GUIDs used to locate the SMBIOS tables in the UEFI 2.5 system table.

  These GUIDs in the system table are the only legal ways to search for and
  locate the SMBIOS tables. Do not search the 0xF0000 segment to find SMBIOS
  tables.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in UEFI 2.5 spec.

**/

#ifndef __SMBIOS_GUID_H__
#define __SMBIOS_GUID_H__

#define SMBIOS_TABLE_GUID \
  { \
    0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define SMBIOS3_TABLE_GUID \
  { \
    0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94 } \
  }

extern EFI_GUID       gEfiSmbiosTableGuid;
extern EFI_GUID       gEfiSmbios3TableGuid;

#endif
