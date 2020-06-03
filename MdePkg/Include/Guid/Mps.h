/** @file
  GUIDs used for MPS entries in the UEFI 2.0 system table
  ACPI is the primary means of exporting MPS information to the OS. MPS only was
  included to support Itanium-based platform power on. So don't use it if you don't have too.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUIDs defined in UEFI 2.0 spec.

**/

#ifndef __MPS_GUID_H__
#define __MPS_GUID_H__

#define EFI_MPS_TABLE_GUID \
  { \
    0xeb9d2d2f, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

//
// GUID name defined in spec.
//
#define MPS_TABLE_GUID EFI_MPS_TABLE_GUID

extern EFI_GUID gEfiMpsTableGuid;

#endif
