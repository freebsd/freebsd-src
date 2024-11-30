/** @file
  AsmReadEflags function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Reads the current value of the EFLAGS register.

  Reads and returns the current value of the EFLAGS register. This function is
  only available on IA-32 and x64. This returns a 32-bit value on IA-32 and a
  64-bit value on x64.

  @return EFLAGS on IA-32 or RFLAGS on x64.

**/
UINTN
EFIAPI
AsmReadEflags (
  VOID
  )
{
  __asm {
    pushfd
    pop     eax
  }
}
