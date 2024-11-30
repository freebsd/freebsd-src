/** @file
  Specific relocation fixups for ARM architecture.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BasePeCoffLibInternals.h"
#include <Library/BaseLib.h>

/**
  Pass in a pointer to an ARM MOVT or MOVW immediate instruciton and
  return the immediate data encoded in the instruction.

  @param  Instruction   Pointer to ARM MOVT or MOVW immediate instruction

  @return Immediate address encoded in the instruction

**/
UINT16
ThumbMovtImmediateAddress (
  IN UINT16  *Instruction
  )
{
  UINT32  Movt;
  UINT16  Address;

  // Thumb2 is two 16-bit instructions working together. Not a single 32-bit instruction
  // Example MOVT R0, #0 is 0x0000f2c0 or 0xf2c0 0x0000
  Movt = (*Instruction << 16) | (*(Instruction + 1));

  // imm16 = imm4:i:imm3:imm8
  //         imm4 -> Bit19:Bit16
  //         i    -> Bit26
  //         imm3 -> Bit14:Bit12
  //         imm8 -> Bit7:Bit0
  Address  = (UINT16)(Movt & 0x000000ff);         // imm8
  Address |= (UINT16)((Movt >> 4) &  0x0000f700); // imm4 imm3
  Address |= (((Movt & BIT26) != 0) ? BIT11 : 0); // i
  return Address;
}

/**
  Update an ARM MOVT or MOVW immediate instruction immediate data.

  @param  Instruction   Pointer to ARM MOVT or MOVW immediate instruction
  @param  Address       New addres to patch into the instruction
**/
VOID
ThumbMovtImmediatePatch (
  IN OUT UINT16  *Instruction,
  IN     UINT16  Address
  )
{
  UINT16  Patch;

  // First 16-bit chunk of instruciton
  Patch  = ((Address >> 12) & 0x000f);             // imm4
  Patch |= (((Address & BIT11) != 0) ? BIT10 : 0); // i
  // Mask out instruction bits and or in address
  *(Instruction) = (*Instruction & ~0x040f) | Patch;

  // Second 16-bit chunk of instruction
  Patch  =  Address & 0x000000ff;          // imm8
  Patch |=  ((Address << 4) & 0x00007000); // imm3
  // Mask out instruction bits and or in address
  Instruction++;
  *Instruction = (*Instruction & ~0x70ff) | Patch;
}

/**
  Pass in a pointer to an ARM MOVW/MOVT instruciton pair and
  return the immediate data encoded in the two` instruction.

  @param  Instructions  Pointer to ARM MOVW/MOVT insturction pair

  @return Immediate address encoded in the instructions

**/
UINT32
ThumbMovwMovtImmediateAddress (
  IN UINT16  *Instructions
  )
{
  UINT16  *Word;
  UINT16  *Top;

  Word = Instructions;  // MOVW
  Top  = Word + 2;      // MOVT

  return (ThumbMovtImmediateAddress (Top) << 16) + ThumbMovtImmediateAddress (Word);
}

/**
  Update an ARM MOVW/MOVT immediate instruction instruction pair.

  @param  Instructions  Pointer to ARM MOVW/MOVT instruction pair
  @param  Address       New addres to patch into the instructions
**/
VOID
ThumbMovwMovtImmediatePatch (
  IN OUT UINT16  *Instructions,
  IN     UINT32  Address
  )
{
  UINT16  *Word;
  UINT16  *Top;

  Word = Instructions;  // MOVW
  Top  = Word + 2;      // MOVT

  ThumbMovtImmediatePatch (Word, (UINT16)(Address & 0xffff));
  ThumbMovtImmediatePatch (Top, (UINT16)(Address >> 16));
}

/**
  Performs an ARM-based specific relocation fixup and is a no-op on other
  instruction sets.

  @param  Reloc       The pointer to the relocation record.
  @param  Fixup       The pointer to the address to fix up.
  @param  FixupData   The pointer to a buffer to log the fixups.
  @param  Adjust      The offset to adjust the fixup.

  @return Status code.

**/
RETURN_STATUS
PeCoffLoaderRelocateImageEx (
  IN UINT16     *Reloc,
  IN OUT CHAR8  *Fixup,
  IN OUT CHAR8  **FixupData,
  IN UINT64     Adjust
  )
{
  UINT16  *Fixup16;
  UINT32  FixupVal;

  Fixup16 = (UINT16 *)Fixup;

  switch ((*Reloc) >> 12) {
    case EFI_IMAGE_REL_BASED_ARM_MOV32T:
      FixupVal = ThumbMovwMovtImmediateAddress (Fixup16) + (UINT32)Adjust;
      ThumbMovwMovtImmediatePatch (Fixup16, FixupVal);

      if (*FixupData != NULL) {
        *FixupData = ALIGN_POINTER (*FixupData, sizeof (UINT64));
        // Fixup16 is not aligned so we must copy it. Thumb instructions are streams of 16 bytes.
        CopyMem (*FixupData, Fixup16, sizeof (UINT64));
        *FixupData = *FixupData + sizeof (UINT64);
      }

      break;

    case EFI_IMAGE_REL_BASED_ARM_MOV32A:
      ASSERT (FALSE);
    // break omitted - ARM instruction encoding not implemented
    default:
      return RETURN_UNSUPPORTED;
  }

  return RETURN_SUCCESS;
}

/**
  Returns TRUE if the machine type of PE/COFF image is supported. Supported
  does not mean the image can be executed it means the PE/COFF loader supports
  loading and relocating of the image type. It's up to the caller to support
  the entry point.

  @param  Machine   Machine type from the PE Header.

  @return TRUE if this PE/COFF loader can load the image

**/
BOOLEAN
PeCoffLoaderImageFormatSupported (
  IN  UINT16  Machine
  )
{
  if ((Machine == IMAGE_FILE_MACHINE_ARMTHUMB_MIXED) || (Machine ==  IMAGE_FILE_MACHINE_EBC)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Performs an ARM-based specific re-relocation fixup and is a no-op on other
  instruction sets. This is used to re-relocated the image into the EFI virtual
  space for runtime calls.

  @param  Reloc       The pointer to the relocation record.
  @param  Fixup       The pointer to the address to fix up.
  @param  FixupData   The pointer to a buffer to log the fixups.
  @param  Adjust      The offset to adjust the fixup.

  @return Status code.

**/
RETURN_STATUS
PeHotRelocateImageEx (
  IN UINT16     *Reloc,
  IN OUT CHAR8  *Fixup,
  IN OUT CHAR8  **FixupData,
  IN UINT64     Adjust
  )
{
  UINT16  *Fixup16;
  UINT32  FixupVal;

  Fixup16 = (UINT16 *)Fixup;

  switch ((*Reloc) >> 12) {
    case EFI_IMAGE_REL_BASED_ARM_MOV32T:
      *FixupData = ALIGN_POINTER (*FixupData, sizeof (UINT64));
      if (*(UINT64 *)(*FixupData) == ReadUnaligned64 ((UINT64 *)Fixup16)) {
        FixupVal = ThumbMovwMovtImmediateAddress (Fixup16) + (UINT32)Adjust;
        ThumbMovwMovtImmediatePatch (Fixup16, FixupVal);
      }

      *FixupData = *FixupData + sizeof (UINT64);
      break;

    case EFI_IMAGE_REL_BASED_ARM_MOV32A:
      ASSERT (FALSE);
    // break omitted - ARM instruction encoding not implemented
    default:
      DEBUG ((DEBUG_ERROR, "PeHotRelocateEx:unknown fixed type\n"));
      return RETURN_UNSUPPORTED;
  }

  return RETURN_SUCCESS;
}
