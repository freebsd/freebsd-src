/** @file
  CPU get interrupt state function for RISC-V

  Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BaseLibInternals.h"

extern UINT32 RiscVGetSupervisorModeInterrupts (VOID);

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
  unsigned long RetValue;

  RetValue = RiscVGetSupervisorModeInterrupts ();
  return RetValue? TRUE: FALSE;
}


