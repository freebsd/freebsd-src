/** @file
  This file declares DXE Initial Program Load PPI.
  When the PEI core is done it calls the DXE IPL PPI to load the DXE Foundation.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __DXE_IPL_H__
#define __DXE_IPL_H__

#define EFI_DXE_IPL_PPI_GUID \
  { \
    0xae8ce5d, 0xe448, 0x4437, {0xa8, 0xd7, 0xeb, 0xf5, 0xf1, 0x94, 0xf7, 0x31 } \
  }

typedef struct _EFI_DXE_IPL_PPI EFI_DXE_IPL_PPI;

/**
  The architectural PPI that the PEI Foundation invokes when
  there are no additional PEIMs to invoke.

  This function is invoked by the PEI Foundation.
  The PEI Foundation will invoke this service when there are
  no additional PEIMs to invoke in the system.
  If this PPI does not exist, it is an error condition and
  an ill-formed firmware set. The DXE IPL PPI should never
  return after having been invoked by the PEI Foundation.
  The DXE IPL PPI can do many things internally, including the following:
    - Invoke the DXE entry point from a firmware volume
    - Invoke the recovery processing modules
    - Invoke the S3 resume modules

  @param  This           Pointer to the DXE IPL PPI instance
  @param  PeiServices    Pointer to the PEI Services Table.
  @param  HobList        Pointer to the list of Hand-Off Block (HOB) entries.

  @retval EFI_SUCCESS    Upon this return code, the PEI Foundation should enter
                         some exception handling.Under normal circumstances,
                         the DXE IPL PPI should not return.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DXE_IPL_ENTRY)(
  IN CONST EFI_DXE_IPL_PPI        *This,
  IN EFI_PEI_SERVICES             **PeiServices,
  IN EFI_PEI_HOB_POINTERS         HobList
  );

///
/// Final service to be invoked by the PEI Foundation.
/// The DXE IPL PPI is responsible for locating and loading the DXE Foundation.
/// The DXE IPL PPI may use PEI services to locate and load the DXE Foundation.
///
struct _EFI_DXE_IPL_PPI {
  EFI_DXE_IPL_ENTRY    Entry;
};

extern EFI_GUID  gEfiDxeIplPpiGuid;

#endif
