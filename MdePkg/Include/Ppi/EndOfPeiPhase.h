/** @file
  This PPI will be installed at the end of PEI for all boot paths, including
  normal, recovery, and S3. It allows for PEIMs to possibly quiesce hardware,
  build handoff information for the next phase of execution,
  or provide some terminal processing behavior.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __END_OF_PEI_PHASE_PPI_H__
#define __END_OF_PEI_PHASE_PPI_H__

#define EFI_PEI_END_OF_PEI_PHASE_PPI_GUID \
  { \
    0x605EA650, 0xC65C, 0x42e1, {0xBA, 0x80, 0x91, 0xA5, 0x2A, 0xB6, 0x18, 0xC6 } \
  }

extern EFI_GUID gEfiEndOfPeiSignalPpiGuid;

#endif
