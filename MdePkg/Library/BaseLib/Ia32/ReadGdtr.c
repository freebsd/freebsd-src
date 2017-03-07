/** @file
  AsmReadGdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include "BaseLibInternals.h"


/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and x64.

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
InternalX86ReadGdtr (
  OUT IA32_DESCRIPTOR  *Gdtr
  )
{
  _asm {
    mov     eax, Gdtr
    sgdt    fword ptr [eax]
  }
}

