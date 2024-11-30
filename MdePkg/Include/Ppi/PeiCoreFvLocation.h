/** @file
  Header file for Pei Core FV Location PPI.

  This PPI contains a pointer to the firmware volume which contains the PEI Foundation.
  If the PEI Foundation does not reside in the BFV, then SEC must pass this PPI as a part
  of the PPI list provided to the PEI Foundation Entry Point, otherwise the PEI Foundation
  shall assume that it resides within the BFV.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.7 Volume 1:
  Standards

**/

#ifndef _EFI_PEI_CORE_FV_LOCATION_H_
#define _EFI_PEI_CORE_FV_LOCATION_H_

///
/// Global ID for EFI_PEI_CORE_FV_LOCATION_PPI
///
#define EFI_PEI_CORE_FV_LOCATION_GUID \
  { \
    0x52888eae, 0x5b10, 0x47d0, {0xa8, 0x7f, 0xb8, 0x22, 0xab, 0xa0, 0xca, 0xf4 } \
  }

///
/// This PPI provides location of EFI PeiCoreFv.
///
typedef struct {
  ///
  /// Pointer to the first byte of the firmware volume which contains the PEI Foundation.
  ///
  VOID    *PeiCoreFvLocation;
} EFI_PEI_CORE_FV_LOCATION_PPI;

extern EFI_GUID  gEfiPeiCoreFvLocationPpiGuid;

#endif // _EFI_PEI_CORE_FV_LOCATION_H_
