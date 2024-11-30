/** @file

  Fetch the Tdx info.

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/Tdx.h>
#include <Uefi/UefiBaseType.h>
#include <Library/TdxLib.h>
#include <Library/BaseMemoryLib.h>

UINT64   mTdSharedPageMask = 0;
UINT32   mTdMaxVCpuNum     = 0;
UINT32   mTdVCpuNum        = 0;
BOOLEAN  mTdDataReturned   = FALSE;

/**
  This function call TDCALL_TDINFO to get the TD_RETURN_DATA.
  If the TDCALL is successful, populate below variables:
   - mTdSharedPageMask
   - mTdMaxVCpunum
   - mTdVCpuNum
   - mTdDataReturned

  @return TRUE  The TDCALL is successful and above variables are populated.
  @return FALSE The TDCALL is failed. Above variables are not set.
**/
BOOLEAN
GetTdInfo (
  VOID
  )
{
  UINT64          Status;
  TD_RETURN_DATA  TdReturnData;
  UINT8           Gpaw;

  Status = TdCall (TDCALL_TDINFO, 0, 0, 0, &TdReturnData);
  if (Status == TDX_EXIT_REASON_SUCCESS) {
    Gpaw              = (UINT8)(TdReturnData.TdInfo.Gpaw & 0x3f);
    mTdSharedPageMask = 1ULL << (Gpaw - 1);
    mTdMaxVCpuNum     = TdReturnData.TdInfo.MaxVcpus;
    mTdVCpuNum        = TdReturnData.TdInfo.NumVcpus;
    mTdDataReturned   = TRUE;
  } else {
    DEBUG ((DEBUG_ERROR, "Failed call TDCALL_TDINFO. %llx\n", Status));
    mTdDataReturned = FALSE;
  }

  return mTdDataReturned;
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
  if (mTdDataReturned) {
    return mTdSharedPageMask;
  }

  return GetTdInfo () ? mTdSharedPageMask : 0;
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
  if (mTdDataReturned) {
    return mTdMaxVCpuNum;
  }

  return GetTdInfo () ? mTdMaxVCpuNum : 0;
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
  if (mTdDataReturned) {
    return mTdVCpuNum;
  }

  return GetTdInfo () ? mTdVCpuNum : 0;
}
