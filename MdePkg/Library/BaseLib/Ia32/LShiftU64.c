/** @file
  64-bit left shift function for IA-32.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Shifts a 64-bit integer left between 0 and 63 bits. The low bits
  are filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the left by Count bits. The
  low Count bits are set to zero. The shifted value is returned.

  @param  Operand The 64-bit operand to shift left.
  @param  Count   The number of bits to shift left.

  @return Operand << Count

**/
UINT64
EFIAPI
InternalMathLShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  )
{
  _asm {
    mov     cl, byte ptr [Count]
    xor     eax, eax
    mov     edx, dword ptr [Operand + 0]
    test    cl, 32                      // Count >= 32?
    jnz     L0
    mov     eax, edx
    mov     edx, dword ptr [Operand + 4]
L0:
    shld    edx, eax, cl
    shl     eax, cl
  }
}

