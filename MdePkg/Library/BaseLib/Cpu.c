/** @file
  Base Library CPU Functions for all architectures.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BaseLibInternals.h"


/**
  Disables CPU interrupts and returns the interrupt state prior to the disable
  operation.

  @retval TRUE  CPU interrupts were enabled on entry to this call.
  @retval FALSE CPU interrupts were disabled on entry to this call.

**/
BOOLEAN
EFIAPI
SaveAndDisableInterrupts (
  VOID
  )
{
  BOOLEAN                           InterruptState;

  InterruptState = GetInterruptState ();
  DisableInterrupts ();
  return InterruptState;
}

/**
  Set the current CPU interrupt state.

  Sets the current CPU interrupt state to the state specified by
  InterruptState. If InterruptState is TRUE, then interrupts are enabled. If
  InterruptState is FALSE, then interrupts are disabled. InterruptState is
  returned.

  @param  InterruptState  TRUE if interrupts should be enabled. FALSE if
                          interrupts should be disabled.

  @return InterruptState

**/
BOOLEAN
EFIAPI
SetInterruptState (
  IN      BOOLEAN                   InterruptState
  )
{
  if (InterruptState) {
    EnableInterrupts ();
  } else {
    DisableInterrupts ();
  }
  return InterruptState;
}
