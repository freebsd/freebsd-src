/** @file
  AsmReadEs function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of ES Data Segment Register (ES).

  Reads and returns the current value of ES. This function is only available on
  IA-32 and x64.

  @return The current value of ES.

**/
UINT16
EFIAPI
AsmReadEs (
  VOID
  )
{
  __asm {
    xor     eax, eax
    mov     ax, es
  }
}
