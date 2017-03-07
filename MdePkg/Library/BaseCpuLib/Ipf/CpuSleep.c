/** @file
  Base Library CPU functions for Itanium

  Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/PalLib.h>
#include <Library/BaseLib.h>

/**
  Places the CPU in a sleep state until an interrupt is received.

  Places the CPU in a sleep state until an interrupt is received. If interrupts
  are disabled prior to calling this function, then the CPU will be placed in a
  sleep state indefinitely.

**/
VOID
EFIAPI
CpuSleep (
  VOID
  )
{
  UINT64  Tpr;

  //
  // It is the TPR register that controls if external interrupt would bring processor in LIGHT HALT low-power state
  // back to normal state. PAL_HALT_LIGHT does not depend on PSR setting.
  // So here if interrupts are disabled (via PSR.i), TRP.mmi needs to be set to prevent processor being interrupted by external interrupts.
  // If interrupts are enabled, then just use current TRP setting.
  //
  if (GetInterruptState ()) {
    //
    // If interrupts are enabled, then call PAL_HALT_LIGHT with the current TPR setting.
    //
    PalCall (PAL_HALT_LIGHT, 0, 0, 0);
  } else {
    //
    // If interrupts are disabled on entry, then mask all interrupts in TPR before calling PAL_HALT_LIGHT.
    //

    //
    // Save TPR
    //
    Tpr = AsmReadTpr();
    //
    // Set TPR.mmi to mask all external interrupts
    //
    AsmWriteTpr (BIT16 | Tpr);

    PalCall (PAL_HALT_LIGHT, 0, 0, 0);

    //
    // Restore TPR
    //
    AsmWriteTpr (Tpr);
  }
}
