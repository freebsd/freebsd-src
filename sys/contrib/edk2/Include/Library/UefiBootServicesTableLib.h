/** @file
  Provides a service to retrieve a pointer to the EFI Boot Services Table.
  Only available to DXE and UEFI module types.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UEFI_BOOT_SERVICES_TABLE_LIB_H__
#define __UEFI_BOOT_SERVICES_TABLE_LIB_H__

///
/// Cache the Image Handle
///
extern EFI_HANDLE         gImageHandle;

///
/// Cache pointer to the EFI System Table
///
extern EFI_SYSTEM_TABLE   *gST;

///
/// Cache pointer to the EFI Boot Services Table
///
extern EFI_BOOT_SERVICES  *gBS;

#endif
