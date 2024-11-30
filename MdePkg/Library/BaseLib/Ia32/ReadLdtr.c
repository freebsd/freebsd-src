/** @file
  AsmReadLdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and x64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
AsmReadLdtr (
  VOID
  )
{
  _asm {
    sldt    ax
  }
}
