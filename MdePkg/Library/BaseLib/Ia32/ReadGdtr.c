/** @file
  AsmReadGdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

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

