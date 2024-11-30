/** @file
  AsmWriteMm1 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Writes the current value of 64-bit MMX Register #1 (MM1).

  Writes the current value of MM1. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM1.

**/
VOID
EFIAPI
AsmWriteMm1 (
  IN UINT64  Value
  )
{
  _asm {
    movq    mm1, qword ptr [Value]
    emms
  }
}
