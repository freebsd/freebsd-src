/** @file
  Unit Test Host functions.

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UNIT_TEST_HOST_H__
#define __UNIT_TEST_HOST_H__

#include "BaseLibInternals.h"
#include <Library/UnitTestHostBaseLib.h>

/**
  Enables CPU interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibEnableInterrupts (
  VOID
  );

/**
  Disables CPU interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibDisableInterrupts (
  VOID
  );

/**
  Enables CPU interrupts for the smallest window required to capture any
  pending interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibEnableDisableInterrupts (
  VOID
  );

/**
  Set the current CPU interrupt state.

  Sets the current CPU interrupt state to the state specified by
  InterruptState. If InterruptState is TRUE, then interrupts are enabled. If
  InterruptState is FALSE, then interrupts are disabled. InterruptState is
  returned.

  @param  InterruptState  TRUE if interrupts should enabled. FALSE if
                          interrupts should be disabled.

  @return InterruptState

**/
BOOLEAN
EFIAPI
UnitTestHostBaseLibGetInterruptState (
  VOID
  );

#endif
