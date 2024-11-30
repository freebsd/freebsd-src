/** @file
  Calculate the quotient of a 64-bit integer by a 32-bit integer

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. This
  function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivU64x32 (
  IN      UINT64  Dividend,
  IN      UINT32  Divisor
  )
{
  _asm {
    mov     eax, dword ptr [Dividend + 4]
    mov     ecx, Divisor
    xor     edx, edx
    div     ecx
    push    eax                     ; save quotient on stack
    mov     eax, dword ptr [Dividend]
    div     ecx
    pop     edx                     ; restore high-order dword of the quotient
  }
}
