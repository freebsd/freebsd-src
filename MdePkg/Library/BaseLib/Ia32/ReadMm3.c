/** @file
  AsmReadMm3 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of 64-bit MMX Register #3 (MM3).

  Reads and returns the current value of MM3. This function is only available
  on IA-32 and x64.

  @return The current value of MM3.

**/
UINT64
EFIAPI
AsmReadMm3 (
  VOID
  )
{
  _asm {
    push    eax
    push    eax
    movq    [esp], mm3
    pop     eax
    pop     edx
    emms
  }
}
