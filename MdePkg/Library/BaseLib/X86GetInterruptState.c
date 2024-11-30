/** @file
  IA-32/x64 GetInterruptState()

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Retrieves the current CPU interrupt state.

  Returns TRUE is interrupts are currently enabled. Otherwise
  returns FALSE.

  @retval TRUE  CPU interrupts are enabled.
  @retval FALSE CPU interrupts are disabled.

**/
BOOLEAN
EFIAPI
GetInterruptState (
  VOID
  )
{
  IA32_EFLAGS32  EFlags;

  EFlags.UintN = AsmReadEflags ();
  return (BOOLEAN)(1 == EFlags.Bits.IF);
}
