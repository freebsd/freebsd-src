/** @file
  PE/Coff loader for RISC-V PE image

  Portions Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "BasePeCoffLibInternals.h"
#include <Library/BaseLib.h>

/**
  Performs an RISC-V specific relocation fixup and is a no-op on
  other instruction sets.
  RISC-V splits 32-bit fixup into 20bit and 12-bit with two relocation
  types. We have to know the lower 12-bit fixup first then we can deal
  carry over on high 20-bit fixup. So we log the high 20-bit in
  FixupData.

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
  UINT32  Value;
  UINT32  Value2;
  UINT32  *RiscVHi20Fixup;

  switch ((*Reloc) >> 12) {
    case EFI_IMAGE_REL_BASED_RISCV_HI20:
      *(UINT64 *)(*FixupData) = (UINT64)(UINTN)Fixup;
      break;

    case EFI_IMAGE_REL_BASED_RISCV_LOW12I:
      RiscVHi20Fixup =  (UINT32 *)(*(UINT64 *)(*FixupData));
      if (RiscVHi20Fixup != NULL) {
        Value  = (UINT32)(RV_X (*RiscVHi20Fixup, 12, 20) << 12);
        Value2 = (UINT32)(RV_X (*(UINT32 *)Fixup, 20, 12));
        if (Value2 & (RISCV_IMM_REACH/2)) {
          Value2 |= ~(RISCV_IMM_REACH-1);
        }

        Value                    += Value2;
        Value                    += (UINT32)Adjust;
        Value2                    = RISCV_CONST_HIGH_PART (Value);
        *(UINT32 *)RiscVHi20Fixup = (RV_X (Value2, 12, 20) << 12) | \
                                    (RV_X (*(UINT32 *)RiscVHi20Fixup, 0, 12));
        *(UINT32 *)Fixup = (RV_X (Value, 0, 12) << 20) | \
                           (RV_X (*(UINT32 *)Fixup, 0, 20));
      }

      break;

    case EFI_IMAGE_REL_BASED_RISCV_LOW12S:
      RiscVHi20Fixup =  (UINT32 *)(*(UINT64 *)(*FixupData));
      if (RiscVHi20Fixup != NULL) {
        Value  = (UINT32)(RV_X (*RiscVHi20Fixup, 12, 20) << 12);
        Value2 = (UINT32)(RV_X (*(UINT32 *)Fixup, 7, 5) | (RV_X (*(UINT32 *)Fixup, 25, 7) << 5));
        if (Value2 & (RISCV_IMM_REACH/2)) {
          Value2 |= ~(RISCV_IMM_REACH-1);
        }

        Value                    += Value2;
        Value                    += (UINT32)Adjust;
        Value2                    = RISCV_CONST_HIGH_PART (Value);
        *(UINT32 *)RiscVHi20Fixup = (RV_X (Value2, 12, 20) << 12) | \
                                    (RV_X (*(UINT32 *)RiscVHi20Fixup, 0, 12));
        Value2           = *(UINT32 *)Fixup & 0x01fff07f;
        Value           &= RISCV_IMM_REACH - 1;
        *(UINT32 *)Fixup = Value2 | (UINT32)(((RV_X (Value, 0, 5) << 7) | (RV_X (Value, 5, 7) << 25)));
      }

      break;

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
  /*
   * ARM64 and X64 may allow such foreign images to be used when
   * a driver implementing EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL is
   * present.
   */
  if ((Machine == IMAGE_FILE_MACHINE_RISCV64) ||
      (Machine == IMAGE_FILE_MACHINE_ARM64) ||
      (Machine == IMAGE_FILE_MACHINE_X64))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Performs an Itanium-based specific re-relocation fixup and is a no-op on other
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
  return RETURN_UNSUPPORTED;
}
