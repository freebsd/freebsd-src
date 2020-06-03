/** @file
  This file declares Sec Hob Data PPI.

  This PPI provides a way for the SEC code to pass zero or more HOBs in a HOB list.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.5.

**/

#ifndef __SEC_HOB_DATA_PPI_H__
#define __SEC_HOB_DATA_PPI_H__

#include <Pi/PiHob.h>

#define EFI_SEC_HOB_DATA_PPI_GUID \
  { \
    0x3ebdaf20, 0x6667, 0x40d8, {0xb4, 0xee, 0xf5, 0x99, 0x9a, 0xc1, 0xb7, 0x1f } \
  }

typedef struct _EFI_SEC_HOB_DATA_PPI EFI_SEC_HOB_DATA_PPI;

/**
  Return a pointer to a buffer containing zero or more HOBs that
  will be installed into the PEI HOB List.

  This function returns a pointer to a pointer to zero or more HOBs,
  terminated with a HOB of type EFI_HOB_TYPE_END_OF_HOB_LIST.
  Note: The HobList must not contain a EFI_HOB_HANDOFF_INFO_TABLE HOB (PHIT) HOB.

  @param[in]  This          Pointer to this PPI structure.
  @param[out] HobList       A pointer to a returned pointer to zero or more HOBs.
                            If no HOBs are to be returned, then the returned pointer
                            is a pointer to a HOB of type EFI_HOB_TYPE_END_OF_HOB_LIST.

  @retval EFI_SUCCESS       This function completed successfully.
  @retval EFI_NOT_FOUND     No HOBS are available.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SEC_HOB_DATA_GET) (
  IN CONST EFI_SEC_HOB_DATA_PPI *This,
  OUT EFI_HOB_GENERIC_HEADER    **HobList
);

///
/// This PPI provides a way for the SEC code to pass zero or more HOBs in a HOB list.
///
struct _EFI_SEC_HOB_DATA_PPI {
  EFI_SEC_HOB_DATA_GET          GetHobs;
};

extern EFI_GUID gEfiSecHobDataPpiGuid;

#endif
