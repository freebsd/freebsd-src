/** @file

  Unaccepted memory is a special type of private memory. In Td guest
  TDCALL [TDG.MEM.PAGE.ACCEPT] is invoked to accept the unaccepted
  memory before use it.

  Copyright (c) 2020 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/Tdx.h>
#include <Uefi/UefiBaseType.h>
#include <Library/TdxLib.h>
#include <Library/BaseMemoryLib.h>

UINT64  mNumberOfDuplicatedAcceptedPages;

#define TDX_ACCEPTPAGE_MAX_RETRIED  3

// PageSize is mapped to PageLevel like below:
// 4KB - 0, 2MB - 1
UINT32  mTdxAcceptPageLevelMap[2] = {
  SIZE_4KB,
  SIZE_2MB
};

#define INVALID_ACCEPT_PAGELEVEL  ARRAY_SIZE(mTdxAcceptPageLevelMap)

/**
  This function gets the PageLevel according to the input page size.

  @param[in]  PageSize    Page size

  @return UINT32          The mapped page level
**/
UINT32
GetGpaPageLevel (
  UINT32  PageSize
  )
{
  UINT32  Index;

  for (Index = 0; Index < ARRAY_SIZE (mTdxAcceptPageLevelMap); Index++) {
    if (mTdxAcceptPageLevelMap[Index] == PageSize) {
      break;
    }
  }

  return Index;
}

/**
  This function accept a pending private page, and initialize the page to
  all-0 using the TD ephemeral private key.

  Sometimes TDCALL [TDG.MEM.PAGE.ACCEPT] may return
  TDX_EXIT_REASON_PAGE_SIZE_MISMATCH. It indicates the input PageLevel is
  not workable. In this case we need to try to fallback to a smaller
  PageLevel if possible.

  @param[in]  StartAddress      Guest physical address of the private
                                page to accept. [63:52] and [11:0] must be 0.
  @param[in]  NumberOfPages     Number of the pages to be accepted.
  @param[in]  PageSize          GPA page size. Only accept 2M/4K size.

  @return EFI_SUCCESS           Accept successfully
  @return others                Indicate other errors
**/
EFI_STATUS
EFIAPI
TdAcceptPages (
  IN UINT64  StartAddress,
  IN UINT64  NumberOfPages,
  IN UINT32  PageSize
  )
{
  EFI_STATUS  Status;
  UINT64      Address;
  UINT64      TdxStatus;
  UINT64      Index;
  UINT32      GpaPageLevel;
  UINT32      PageSize2;
  UINTN       Retried;

  Retried = 0;

  if ((StartAddress & ~0xFFFFFFFFFF000ULL) != 0) {
    ASSERT (FALSE);
    DEBUG ((DEBUG_ERROR, "Accept page address(0x%llx) is not valid. [63:52] and [11:0] must be 0\n", StartAddress));
    return EFI_INVALID_PARAMETER;
  }

  Address = StartAddress;

  GpaPageLevel = GetGpaPageLevel (PageSize);
  if (GpaPageLevel == INVALID_ACCEPT_PAGELEVEL) {
    ASSERT (FALSE);
    DEBUG ((DEBUG_ERROR, "Accept page size must be 4K/2M. Invalid page size - 0x%llx\n", PageSize));
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  for (Index = 0; Index < NumberOfPages; Index++) {
    Retried = 0;

DoAcceptPage:
    TdxStatus = TdCall (TDCALL_TDACCEPTPAGE, Address | GpaPageLevel, 0, 0, 0);
    if (TdxStatus != TDX_EXIT_REASON_SUCCESS) {
      if ((TdxStatus & ~0xFFFFULL) == TDX_EXIT_REASON_PAGE_ALREADY_ACCEPTED) {
        //
        // Already accepted
        //
        mNumberOfDuplicatedAcceptedPages++;
        DEBUG ((DEBUG_WARN, "Page at Address (0x%llx) has already been accepted. - %d\n", Address, mNumberOfDuplicatedAcceptedPages));
      } else if ((TdxStatus & ~0xFFFFULL) == TDX_EXIT_REASON_PAGE_SIZE_MISMATCH) {
        //
        // GpaPageLevel is mismatch, fall back to a smaller GpaPageLevel if possible
        //
        DEBUG ((DEBUG_VERBOSE, "Address %llx cannot be accepted in PageLevel of %d\n", Address, GpaPageLevel));

        if (GpaPageLevel == 0) {
          //
          // Cannot fall back to smaller page level
          //
          DEBUG ((DEBUG_ERROR, "AcceptPage cannot fallback from PageLevel %d\n", GpaPageLevel));
          Status = EFI_INVALID_PARAMETER;
          break;
        } else {
          //
          // Fall back to a smaller page size
          //
          PageSize2 = mTdxAcceptPageLevelMap[GpaPageLevel - 1];
          Status    = TdAcceptPages (Address, 512, PageSize2);
          if (EFI_ERROR (Status)) {
            break;
          }
        }
      } else if ((TdxStatus & ~0xFFFFULL) == TDX_EXIT_REASON_OPERAND_BUSY) {
        //
        // Concurrent TDG.MEM.PAGE.ACCEPT is using the same Secure EPT entry
        // So try it again. There is a max retried count. If Retried exceeds the max count,
        // report the error and quit.
        //
        Retried += 1;
        if (Retried > TDX_ACCEPTPAGE_MAX_RETRIED) {
          DEBUG ((
            DEBUG_ERROR,
            "Address %llx (%d) failed to be accepted because of OPERAND_BUSY. Retried %d time.\n",
            Address,
            Index,
            Retried
            ));
          Status = EFI_INVALID_PARAMETER;
          break;
        } else {
          goto DoAcceptPage;
        }
      } else {
        //
        // Other errors
        //
        DEBUG ((
          DEBUG_ERROR,
          "Address %llx (%d) failed to be accepted. Error = 0x%llx\n",
          Address,
          Index,
          TdxStatus
          ));
        Status = EFI_INVALID_PARAMETER;
        break;
      }
    }

    Address += PageSize;
  }

  return Status;
}
