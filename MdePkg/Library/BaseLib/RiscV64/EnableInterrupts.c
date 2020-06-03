/** @file
  CPU enable interrupt function for RISC-V

  Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BaseLibInternals.h"

extern VOID RiscVEnableSupervisorModeInterrupt (VOID);

/**
  Enables CPU interrupts.

**/
VOID
EFIAPI
EnableInterrupts (
  VOID
  )
{
  RiscVEnableSupervisorModeInterrupt ();
}

