/** @file
  Common Unit Test Host functions.

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UnitTestHost.h"

///
/// Module global variable for simple system emulation of interrupt state
///
STATIC BOOLEAN  mUnitTestHostBaseLibInterruptState;

/**
  Enables CPU interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibEnableInterrupts (
  VOID
  )
{
  mUnitTestHostBaseLibInterruptState = TRUE;
}

/**
  Disables CPU interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibDisableInterrupts (
  VOID
  )
{
  mUnitTestHostBaseLibInterruptState = FALSE;
}

/**
  Enables CPU interrupts for the smallest window required to capture any
  pending interrupts.

**/
VOID
EFIAPI
UnitTestHostBaseLibEnableDisableInterrupts (
  VOID
  )
{
  mUnitTestHostBaseLibInterruptState = FALSE;
}

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
  )
{
  return mUnitTestHostBaseLibInterruptState;
}

/**
  Enables CPU interrupts.

**/
VOID
EFIAPI
EnableInterrupts (
  VOID
  )
{
  gUnitTestHostBaseLib.Common->EnableInterrupts ();
}

/**
  Disables CPU interrupts.

**/
VOID
EFIAPI
DisableInterrupts (
  VOID
  )
{
  gUnitTestHostBaseLib.Common->DisableInterrupts ();
}

/**
  Enables CPU interrupts for the smallest window required to capture any
  pending interrupts.

**/
VOID
EFIAPI
EnableDisableInterrupts (
  VOID
  )
{
  gUnitTestHostBaseLib.Common->EnableDisableInterrupts ();
}

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
GetInterruptState (
  VOID
  )
{
  return gUnitTestHostBaseLib.Common->GetInterruptState ();
}
