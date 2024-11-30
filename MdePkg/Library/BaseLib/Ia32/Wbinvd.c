/** @file
  AsmWbinvd function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Executes a WBINVD instruction.

  Executes a WBINVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
AsmWbinvd (
  VOID
  )
{
  _asm {
    wbinvd
  }
}
