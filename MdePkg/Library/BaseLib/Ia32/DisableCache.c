/** @file
  AsmDisableCache function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Set CD bit and clear NW bit of CR0 followed by a WBINVD.

  Disables the caches by setting the CD bit of CR0 to 1, clearing the NW bit of CR0 to 0,
  and executing a WBINVD instruction.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmDisableCache (
  VOID
  )
{
  _asm {
    mov     eax, cr0
    bts     eax, 30
    btr     eax, 29
    mov     cr0, eax
    wbinvd
  }
}
