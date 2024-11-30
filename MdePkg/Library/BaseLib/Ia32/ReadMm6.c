/** @file
  AsmReadMm6 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of 64-bit MMX Register #6 (MM6).

  Reads and returns the current value of MM6. This function is only available
  on IA-32 and x64.

  @return The current value of MM6.

**/
UINT64
EFIAPI
AsmReadMm6 (
  VOID
  )
{
  _asm {
    push    eax
    push    eax
    movq    [esp], mm6
    pop     eax
    pop     edx
    emms
  }
}
