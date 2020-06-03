/** @file
  This file declares Memory Discovered PPI.

  This PPI is published by the PEI Foundation when the main memory is installed.
  It is essentially a PPI with no associated interface. Its purpose is to be used
  as a signal for other PEIMs who can register for a notification on its installation.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __PEI_MEMORY_DISCOVERED_PPI_H__
#define __PEI_MEMORY_DISCOVERED_PPI_H__

#define EFI_PEI_PERMANENT_MEMORY_INSTALLED_PPI_GUID \
  { \
    0xf894643d, 0xc449, 0x42d1, {0x8e, 0xa8, 0x85, 0xbd, 0xd8, 0xc6, 0x5b, 0xde } \
  }

extern EFI_GUID gEfiPeiMemoryDiscoveredPpiGuid;

#endif
