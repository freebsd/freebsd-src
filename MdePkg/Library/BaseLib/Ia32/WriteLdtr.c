/** @file
  AsmWriteLdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
  IN UINT16 Ldtr
  )
{
  _asm {
    xor     eax, eax
    mov     ax, Ldtr
    lldt    ax
  }
}

