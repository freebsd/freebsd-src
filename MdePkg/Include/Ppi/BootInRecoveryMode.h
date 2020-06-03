/** @file
  This PPI is installed by the platform PEIM to designate that a recovery boot
  is in progress.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __BOOT_IN_RECOVERY_MODE_PPI_H__
#define __BOOT_IN_RECOVERY_MODE_PPI_H__

#define EFI_PEI_BOOT_IN_RECOVERY_MODE_PPI \
  { \
    0x17ee496a, 0xd8e4, 0x4b9a, {0x94, 0xd1, 0xce, 0x82, 0x72, 0x30, 0x8, 0x50 } \
  }


extern EFI_GUID gEfiPeiBootInRecoveryModePpiGuid;

#endif
