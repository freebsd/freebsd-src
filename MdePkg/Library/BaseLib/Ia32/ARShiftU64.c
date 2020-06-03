/** @file
  64-bit arithmetic right shift function for IA-32.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits
  are filled with original integer's bit 63. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to bit 63 of Operand.  The shifted value is returned.

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand arithmetically shifted right by Count.

**/
UINT64
EFIAPI
InternalMathARShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  )
{
  _asm {
    mov     cl, byte ptr [Count]
    mov     eax, dword ptr [Operand + 4]
    cdq
    test    cl, 32
    jnz     L0
    mov     edx, eax
    mov     eax, dword ptr [Operand + 0]
L0:
    shrd    eax, edx, cl
    sar     edx, cl
  }
}

