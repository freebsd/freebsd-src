/** @file
  Implementation of 64-bit swap bytes

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Switches the endianess of a 64-bit integer.

  This function swaps the bytes in a 64-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Operand A 64-bit unsigned value.

  @return The byte swaped Operand.

**/
UINT64
EFIAPI
InternalMathSwapBytes64 (
  IN      UINT64                    Operand
  )
{
  _asm {
    mov     eax, dword ptr [Operand + 4]
    mov     edx, dword ptr [Operand + 0]
    bswap   eax
    bswap   edx
  }
}

