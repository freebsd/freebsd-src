/** @file
  AsmReadMm4 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of 64-bit MMX Register #4 (MM4).

  Reads and returns the current value of MM4. This function is only available
  on IA-32 and x64.

  @return The current value of MM4.

**/
UINT64
EFIAPI
AsmReadMm4 (
  VOID
  )
{
  _asm {
    push    eax
    push    eax
    movq    [esp], mm4
    pop     eax
    pop     edx
    emms
  }
}
