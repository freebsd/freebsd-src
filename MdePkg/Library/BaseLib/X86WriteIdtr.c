/** @file
  IA-32/x64 AsmWriteIdtr()

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




#include "BaseLibInternals.h"

/**
  Writes the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmWriteIdtr (
  IN      CONST IA32_DESCRIPTOR     *Idtr
  )
{
  ASSERT (Idtr != NULL);
  InternalX86WriteIdtr (Idtr);
}
