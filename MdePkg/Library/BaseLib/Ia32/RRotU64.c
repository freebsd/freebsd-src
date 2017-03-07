/** @file
  64-bit right rotation for Ia32

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Rotates a 64-bit integer right between 0 and 63 bits, filling
  the high bits with the high low bits that were rotated.

  This function rotates the 64-bit value Operand to the right by Count bits.
  The high Count bits are fill with the low Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >>> Count

**/
UINT64
EFIAPI
InternalMathRRotU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  )
{
  _asm {
    mov     cl, byte ptr [Count]
    mov     eax, dword ptr [Operand + 0]
    mov     edx, dword ptr [Operand + 4]
    shrd    ebx, eax, cl
    shrd    eax, edx, cl
    rol     ebx, cl
    shrd    edx, ebx, cl
    test    cl, 32                      // Count >= 32?
    jz      L0
    mov     ecx, eax
    mov     eax, edx
    mov     edx, ecx
L0:
  }
}

