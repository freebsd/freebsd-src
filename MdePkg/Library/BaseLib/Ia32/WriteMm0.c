/** @file
  AsmWriteMm0 function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Writes the current value of 64-bit MMX Register #0 (MM0).

  Writes the current value of MM0. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM0.

**/
VOID
EFIAPI
AsmWriteMm0 (
  IN UINT64   Value
  )
{
  _asm {
    movq    mm0, qword ptr [Value]
    emms
  }
}

