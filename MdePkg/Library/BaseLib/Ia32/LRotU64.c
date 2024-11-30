/** @file
  64-bit left rotation for Ia32

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Rotates a 64-bit integer left between 0 and 63 bits, filling
  the low bits with the high bits that were rotated.

  This function rotates the 64-bit value Operand to the left by Count bits. The
  low Count bits are fill with the high Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand <<< Count

**/
UINT64
EFIAPI
InternalMathLRotU64 (
  IN      UINT64  Operand,
  IN      UINTN   Count
  )
{
  _asm {
    mov     cl, byte ptr [Count]
    mov     edx, dword ptr [Operand + 4]
    mov     eax, dword ptr [Operand + 0]
    shld    ebx, edx, cl
    shld    edx, eax, cl
    ror     ebx, cl
    shld    eax, ebx, cl
    test    cl, 32                      ; Count >= 32?
    jz      L0
    mov     ecx, eax
    mov     eax, edx
    mov     edx, ecx
    L0 :
  }
}
