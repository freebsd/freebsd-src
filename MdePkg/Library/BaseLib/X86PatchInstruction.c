/** @file
  IA-32/x64 PatchInstructionX86()

  Copyright (C) 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (C) 2018, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BaseLibInternals.h"

/**
  Patch the immediate operand of an IA32 or X64 instruction such that the byte,
  word, dword or qword operand is encoded at the end of the instruction's
  binary representation.

  This function should be used to update object code that was compiled with
  NASM from assembly source code. Example:

  NASM source code:

        mov     eax, strict dword 0 ; the imm32 zero operand will be patched
    ASM_PFX(gPatchCr3):
        mov     cr3, eax

  C source code:

    X86_ASSEMBLY_PATCH_LABEL gPatchCr3;
    PatchInstructionX86 (gPatchCr3, AsmReadCr3 (), 4);

  @param[out] InstructionEnd  Pointer right past the instruction to patch. The
                              immediate operand to patch is expected to
                              comprise the trailing bytes of the instruction.
                              If InstructionEnd is closer to address 0 than
                              ValueSize permits, then ASSERT().

  @param[in] PatchValue       The constant to write to the immediate operand.
                              The caller is responsible for ensuring that
                              PatchValue can be represented in the byte, word,
                              dword or qword operand (as indicated through
                              ValueSize); otherwise ASSERT().

  @param[in] ValueSize        The size of the operand in bytes; must be 1, 2,
                              4, or 8. ASSERT() otherwise.
**/
VOID
EFIAPI
PatchInstructionX86 (
  OUT X86_ASSEMBLY_PATCH_LABEL *InstructionEnd,
  IN  UINT64                   PatchValue,
  IN  UINTN                    ValueSize
  )
{
  //
  // The equality ((UINTN)InstructionEnd == ValueSize) would assume a zero-size
  // instruction at address 0; forbid it.
  //
  ASSERT ((UINTN)InstructionEnd > ValueSize);

  switch (ValueSize) {
  case 1:
    ASSERT (PatchValue <= MAX_UINT8);
    *((UINT8 *)(UINTN)InstructionEnd - 1) = (UINT8)PatchValue;
    break;

  case 2:
    ASSERT (PatchValue <= MAX_UINT16);
    WriteUnaligned16 ((UINT16 *)(UINTN)InstructionEnd - 1, (UINT16)PatchValue);
    break;

  case 4:
    ASSERT (PatchValue <= MAX_UINT32);
    WriteUnaligned32 ((UINT32 *)(UINTN)InstructionEnd - 1, (UINT32)PatchValue);
    break;

  case 8:
    WriteUnaligned64 ((UINT64 *)(UINTN)InstructionEnd - 1, PatchValue);
    break;

  default:
    ASSERT (FALSE);
  }
}
