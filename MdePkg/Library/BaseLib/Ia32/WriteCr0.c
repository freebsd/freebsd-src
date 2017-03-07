/** @file
  AsmWriteCr0 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**
  Writes a value to Control Register 0 (CR0).

  Writes and returns a new value to CR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Value The value to write to CR0.

  @return The value written to CR0.

**/
UINTN
EFIAPI
AsmWriteCr0 (
  UINTN  Value
  )
{
  _asm {
    mov     eax, Value
    mov     cr0, eax
  }
}

