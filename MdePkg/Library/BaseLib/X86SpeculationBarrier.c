/** @file
  SpeculationBarrier() function for IA32 and x64.

  Copyright (C) 2018 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>

/**
  Uses as a barrier to stop speculative execution.

  Ensures that no later instruction will execute speculatively, until all prior
  instructions have completed.

**/
VOID
EFIAPI
SpeculationBarrier (
  VOID
  )
{
  if (PcdGet8 (PcdSpeculationBarrierType) == 0x01) {
    AsmLfence ();
  } else if (PcdGet8 (PcdSpeculationBarrierType) == 0x02) {
    AsmCpuid (0x01, NULL, NULL, NULL, NULL);
  }
}
