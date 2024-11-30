/** @file
  AsmReadDr3 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of Debug Register 3 (DR3).

  Reads and returns the current value of DR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmReadDr3 (
  VOID
  )
{
  __asm {
    mov     eax, dr3
  }
}
