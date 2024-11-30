/** @file
  Library provides a hook called when a stack cookie check fails.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef STACK_COOKIE_FAILURE_HOOK_LIB_H_
#define STACK_COOKIE_FAILURE_HOOK_LIB_H_

#include <Uefi.h>

/**
  This function gets called when a compiler generated stack cookie fails. This allows a platform to hook this
  call and perform any required actions/telemetry at that time.

  @param  FailureAddress  The address of the function that failed the stack cookie check.

**/
VOID
EFIAPI
StackCheckFailureHook (
  VOID  *FailureAddress
  );

#endif
