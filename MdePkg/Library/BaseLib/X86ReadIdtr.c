/** @file
  IA-32/x64 AsmReadIdtr()

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
  Reads the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmReadIdtr (
  OUT     IA32_DESCRIPTOR           *Idtr
  )
{
  ASSERT (Idtr != NULL);
  InternalX86ReadIdtr (Idtr);
}
