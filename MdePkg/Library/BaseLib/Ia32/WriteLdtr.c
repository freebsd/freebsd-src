/** @file
  AsmWriteLdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Writes the current Local Descriptor Table Register (GDTR) selector.

  Writes and the current LDTR descriptor specified by Ldtr. This function is
  only available on IA-32 and x64.

  @param  Ldtr  16-bit LDTR selector value.

**/
VOID
EFIAPI
AsmWriteLdtr (
  IN UINT16  Ldtr
  )
{
  _asm {
    xor     eax, eax
    mov     ax, Ldtr
    lldt    ax
  }
}
