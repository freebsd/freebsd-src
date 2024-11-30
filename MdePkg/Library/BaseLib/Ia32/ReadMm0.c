/** @file
  AsmReadMm0 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of 64-bit MMX Register #0 (MM0).

  Reads and returns the current value of MM0. This function is only available
  on IA-32 and x64.

  @return The current value of MM0.

**/
UINT64
EFIAPI
AsmReadMm0 (
  VOID
  )
{
  _asm {
    push    eax
    push    eax
    movq    [esp], mm0
    pop     eax
    pop     edx
    emms
  }
}
