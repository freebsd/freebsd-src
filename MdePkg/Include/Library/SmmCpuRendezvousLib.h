/** @file
  SMM CPU Rendezvous library header file.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SMM_CPU_RENDEZVOUS_H_
#define SMM_CPU_RENDEZVOUS_H_

/**
  This routine wait for all AP processors to arrive in SMM.

  @param[in]  BlockingMode  Blocking mode or non-blocking mode.

  @retval EFI_SUCCESS       All processors checked in to SMM.
  @retval EFI_TIMEOUT       Wait for all APs until timeout.

**/
EFI_STATUS
EFIAPI
SmmWaitForAllProcessor (
  IN  BOOLEAN  BlockingMode
  );

#endif
