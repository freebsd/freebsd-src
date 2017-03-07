/** @file
  Set error flag for all division functions

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result and an optional 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 32-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.
  @param  Remainder A pointer to a 32-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivRemU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor,
  OUT     UINT32                    *Remainder
  )
{
  _asm {
    mov     ecx, Divisor
    mov     eax, dword ptr [Dividend + 4]
    xor     edx, edx
    div     ecx
    push    eax
    mov     eax, dword ptr [Dividend + 0]
    div     ecx
    mov     ecx, Remainder
    jecxz   RemainderNull                      // abandon remainder if Remainder == NULL
    mov     [ecx], edx
RemainderNull:
    pop     edx
  }
}

