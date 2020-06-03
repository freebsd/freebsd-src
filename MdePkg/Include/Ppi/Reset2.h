/** @file
  This file declares Reset2 PPI used to reset the platform.

  This PPI is installed by some platform- or chipset-specific PEIM that
  abstracts the Reset Service to other agents.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.4.

**/

#ifndef __RESET2_PPI_H__
#define __RESET2_PPI_H__

#define EFI_PEI_RESET2_PPI_GUID \
  { \
    0x6cc45765, 0xcce4, 0x42fd, {0xbc, 0x56, 0x1, 0x1a, 0xaa, 0xc6, 0xc9, 0xa8 } \
  }

///
/// This PPI provides provide a simple reset service.
///
typedef struct _EFI_PEI_RESET2_PPI {
  EFI_PEI_RESET2_SYSTEM ResetSystem;
} EFI_PEI_RESET2_PPI;

extern EFI_GUID gEfiPeiReset2PpiGuid;

#endif
