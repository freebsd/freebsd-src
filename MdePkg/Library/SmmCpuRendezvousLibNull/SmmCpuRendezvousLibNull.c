/** @file
  SMM CPU Rendezvous sevice implement.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/SmmCpuRendezvousLib.h>

/**
  This routine wait for all AP processors to arrive in SMM.

  @param[in] BlockingMode  Blocking mode or non-blocking mode.

  @retval EFI_SUCCESS  All avaiable APs arrived.
  @retval EFI_TIMEOUT  Wait for all APs until timeout.
  @retval OTHER        Fail to register SMM CPU Rendezvous service Protocol.
**/
EFI_STATUS
EFIAPI
SmmWaitForAllProcessor (
  IN BOOLEAN  BlockingMode
  )
{
  return EFI_SUCCESS;
}
