/** @file
  AsmWriteCr2 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Writes a value to Control Register 2 (CR2).

  Writes and returns a new value to CR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Value The value to write to CR2.

  @return The value written to CR2.

**/
UINTN
EFIAPI
AsmWriteCr2 (
  UINTN  Value
  )
{
  _asm {
    mov     eax, Value
    mov     cr2, eax
  }
}
