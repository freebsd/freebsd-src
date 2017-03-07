/** @file
  AsmDisableCache function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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

