/** @file
  Platform Configuration Database (PCD) Info Ppi defined in PI 1.2.1 Vol3.

  The PPI that provides additional information about items that reside in the PCD database.

  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  PI Version 1.2.1 Vol 3.
**/

#ifndef __PI_PCD_INFO_PPI_H__
#define __PI_PCD_INFO_PPI_H__

extern EFI_GUID gEfiGetPcdInfoPpiGuid;

#define EFI_GET_PCD_INFO_PPI_GUID \
  { 0xa60c6b59, 0xe459, 0x425d, { 0x9c, 0x69,  0xb, 0xcc, 0x9c, 0xb2, 0x7d, 0x81 } }

///
/// The forward declaration for EFI_GET_PCD_INFO_PPI.
///
typedef struct _EFI_GET_PCD_INFO_PPI  EFI_GET_PCD_INFO_PPI;

/**
  Retrieve additional information associated with a PCD token.

  This includes information such as the type of value the TokenNumber is associated with as well as possible
  human readable name that is associated with the token.

  @param[in]    Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]    TokenNumber The PCD token number.
  @param[out]   PcdInfo     The returned information associated with the requested TokenNumber.

  @retval  EFI_SUCCESS      The PCD information was returned successfully
  @retval  EFI_NOT_FOUND    The PCD service could not find the requested token number.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_PCD_INFO_PPI_GET_INFO) (
  IN CONST  EFI_GUID        *Guid,
  IN        UINTN           TokenNumber,
  OUT       EFI_PCD_INFO    *PcdInfo
);

/**
  Retrieve the currently set SKU Id.

  @return   The currently set SKU Id. If the platform has not set at a SKU Id, then the
            default SKU Id value of 0 is returned. If the platform has set a SKU Id, then the currently set SKU
            Id is returned.
**/
typedef
UINTN
(EFIAPI *EFI_GET_PCD_INFO_PPI_GET_SKU) (
  VOID
);

///
/// This is the PCD service to use when querying for some additional data that can be contained in the
/// PCD database.
///
struct _EFI_GET_PCD_INFO_PPI {
  ///
  /// Retrieve additional information associated with a PCD.
  ///
  EFI_GET_PCD_INFO_PPI_GET_INFO         GetInfo;
  ///
  /// Retrieve the currently set SKU Id.
  ///
  EFI_GET_PCD_INFO_PPI_GET_SKU          GetSku;
};

#endif

