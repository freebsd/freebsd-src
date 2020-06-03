/** @file
  GUID used to identify the DXE Services Table

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID introduced in PI Version 1.0.

**/

#ifndef __DXE_SERVICES_GUID_H__
#define __DXE_SERVICES_GUID_H__

#define DXE_SERVICES_TABLE_GUID \
  { \
    0x5ad34ba, 0x6f02, 0x4214, {0x95, 0x2e, 0x4d, 0xa0, 0x39, 0x8e, 0x2b, 0xb9 } \
  }

extern EFI_GUID gEfiDxeServicesTableGuid;

#endif
