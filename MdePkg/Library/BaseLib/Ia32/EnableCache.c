/** @file
  AsmEnableCache function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Perform a WBINVD and clear both the CD and NW bits of CR0.

  Enables the caches by executing a WBINVD instruction and then clear both the CD and NW
  bits of CR0 to 0.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmEnableCache (
  VOID
  )
{
  _asm {
    wbinvd
    mov     eax, cr0
    btr     eax, 30
    btr     eax, 29
    mov     cr0, eax
  }
}
