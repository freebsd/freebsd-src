/** @file
  AsmReadCr2 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Reads the current value of the Control Register 2 (CR2).

  Reads and returns the current value of CR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 2 (CR2).

**/
UINTN
EFIAPI
AsmReadCr2 (
  VOID
  )
{
  __asm {
    mov     eax, cr2
  }
}

