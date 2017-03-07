/** @file
  Calculate the product of a 64-bit integer and a 32-bit integer

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Multiples a 64-bit unsigned integer by a 32-bit unsigned integer
  and generates a 64-bit unsigned result.

  This function multiples the 64-bit unsigned value Multiplicand by the 32-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 32-bit unsigned value.

  @return Multiplicand * Multiplier

**/
UINT64
EFIAPI
InternalMathMultU64x32 (
  IN      UINT64                    Multiplicand,
  IN      UINT32                    Multiplier
  )
{
  _asm {
    mov     ecx, Multiplier
    mov     eax, ecx
    imul    ecx, dword ptr [Multiplicand + 4]  // overflow not detectable
    mul     dword ptr [Multiplicand + 0]
    add     edx, ecx
  }
}

