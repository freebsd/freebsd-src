/** @file
  AsmWriteDr5 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Writes a value to Debug Register 5 (DR5).

  Writes and returns a new value to DR5. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Value The value to write to Dr5.

  @return The value written to Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmWriteDr5 (
  IN UINTN  Value
  )
{
  _asm {
    mov     eax, Value
    _emit   0x0f         // mov dr5, eax
    _emit   0x23
    _emit   0xe8
  }
}
