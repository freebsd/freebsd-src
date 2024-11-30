/** @file
  This file declares Boot Mode PPI.

  The Master Boot Mode PPI is installed by a PEIM to signal that a final
  boot has been determined and set. This signal is useful in that PEIMs
  with boot-mode-specific behavior can put this PPI in their dependency expression.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __MASTER_BOOT_MODE_PPI_H__
#define __MASTER_BOOT_MODE_PPI_H__

#define EFI_PEI_MASTER_BOOT_MODE_PEIM_PPI \
  { \
    0x7408d748, 0xfc8c, 0x4ee6, {0x92, 0x88, 0xc4, 0xbe, 0xc0, 0x92, 0xa4, 0x10 } \
  }

extern EFI_GUID  gEfiPeiMasterBootModePpiGuid;

#endif
