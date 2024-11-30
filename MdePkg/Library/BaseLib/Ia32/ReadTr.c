/** @file
  AsmReadTr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of Task Register (TR).

  Reads and returns the current value of TR. This function is only available on
  IA-32 and x64.

  @return The current value of TR.

**/
UINT16
EFIAPI
AsmReadTr (
  VOID
  )
{
  _asm {
    str     ax
  }
}
