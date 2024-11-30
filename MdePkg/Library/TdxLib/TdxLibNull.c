/** @file

  Null stub of TdxLib

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/TdxLib.h>

/**
  This function accepts a pending private page, and initialize the page to
  all-0 using the TD ephemeral private key.

  @param[in]  StartAddress     Guest physical address of the private page
                               to accept.
  @param[in]  NumberOfPages    Number of the pages to be accepted.
  @param[in]  PageSize         GPA page size. Accept 1G/2M/4K page size.

  @return EFI_SUCCESS
**/
EFI_STATUS
EFIAPI
TdAcceptPages (
  IN UINT64  StartAddress,
  IN UINT64  NumberOfPages,
  IN UINT32  PageSize
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function extends one of the RTMR measurement register
  in TDCS with the provided extension data in memory.
  RTMR extending supports SHA384 which length is 48 bytes.

  @param[in]  Data      Point to the data to be extended
  @param[in]  DataLen   Length of the data. Must be 48
  @param[in]  Index     RTMR index

  @return EFI_SUCCESS
  @return EFI_INVALID_PARAMETER
  @return EFI_DEVICE_ERROR

**/
EFI_STATUS
EFIAPI
TdExtendRtmr (
  IN  UINT32  *Data,
  IN  UINT32  DataLen,
  IN  UINT8   Index
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function gets the Td guest shared page mask.

  The guest indicates if a page is shared using the Guest Physical Address
  (GPA) Shared (S) bit. If the GPA Width(GPAW) is 48, the S-bit is bit-47.
  If the GPAW is 52, the S-bit is bit-51.

  @return Shared page bit mask
**/
UINT64
EFIAPI
TdSharedPageMask (
  VOID
  )
{
  return 0;
}

/**
  This function gets the maximum number of Virtual CPUs that are usable for
  Td Guest.

  @return maximum Virtual CPUs number
**/
UINT32
EFIAPI
TdMaxVCpuNum (
  VOID
  )
{
  return 0;
}

/**
  This function gets the number of Virtual CPUs that are usable for Td
  Guest.

  @return Virtual CPUs number
**/
UINT32
EFIAPI
TdVCpuNum (
  VOID
  )
{
  return 0;
}
