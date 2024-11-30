#
#  Copyright (c) 2014-2018, Linaro Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#

.text
.align 3

GCC_ASM_EXPORT(MmioRead8Internal)
GCC_ASM_EXPORT(MmioWrite8Internal)
GCC_ASM_EXPORT(MmioRead16Internal)
GCC_ASM_EXPORT(MmioWrite16Internal)
GCC_ASM_EXPORT(MmioRead32Internal)
GCC_ASM_EXPORT(MmioWrite32Internal)
GCC_ASM_EXPORT(MmioRead64Internal)
GCC_ASM_EXPORT(MmioWrite64Internal)

//
//  Reads an 8-bit MMIO register.
//
//  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
//  returned. This function must guarantee that all MMIO read and write
//  operations are serialized.
//
//  @param  Address The MMIO register to read.
//
//  @return The value read.
//
ASM_PFX(MmioRead8Internal):
  AARCH64_BTI(c)
  ldrb    w0, [x0]
  dmb     ld
  ret

//
//  Writes an 8-bit MMIO register.
//
//  Writes the 8-bit MMIO register specified by Address with the value specified
//  by Value and returns Value. This function must guarantee that all MMIO read
//  and write operations are serialized.
//
//  @param  Address The MMIO register to write.
//  @param  Value   The value to write to the MMIO register.
//
ASM_PFX(MmioWrite8Internal):
  AARCH64_BTI(c)
  dmb     st
  strb    w1, [x0]
  ret

//
//  Reads a 16-bit MMIO register.
//
//  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
//  returned. This function must guarantee that all MMIO read and write
//  operations are serialized.
//
//  @param  Address The MMIO register to read.
//
//  @return The value read.
//
ASM_PFX(MmioRead16Internal):
  AARCH64_BTI(c)
  ldrh    w0, [x0]
  dmb     ld
  ret

//
//  Writes a 16-bit MMIO register.
//
//  Writes the 16-bit MMIO register specified by Address with the value specified
//  by Value and returns Value. This function must guarantee that all MMIO read
//  and write operations are serialized.
//
//  @param  Address The MMIO register to write.
//  @param  Value   The value to write to the MMIO register.
//
ASM_PFX(MmioWrite16Internal):
  AARCH64_BTI(c)
  dmb     st
  strh    w1, [x0]
  ret

//
//  Reads a 32-bit MMIO register.
//
//  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
//  returned. This function must guarantee that all MMIO read and write
//  operations are serialized.
//
//  @param  Address The MMIO register to read.
//
//  @return The value read.
//
ASM_PFX(MmioRead32Internal):
  AARCH64_BTI(c)
  ldr     w0, [x0]
  dmb     ld
  ret

//
//  Writes a 32-bit MMIO register.
//
//  Writes the 32-bit MMIO register specified by Address with the value specified
//  by Value and returns Value. This function must guarantee that all MMIO read
//  and write operations are serialized.
//
//  @param  Address The MMIO register to write.
//  @param  Value   The value to write to the MMIO register.
//
ASM_PFX(MmioWrite32Internal):
  AARCH64_BTI(c)
  dmb     st
  str     w1, [x0]
  ret

//
//  Reads a 64-bit MMIO register.
//
//  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
//  returned. This function must guarantee that all MMIO read and write
//  operations are serialized.
//
//  @param  Address The MMIO register to read.
//
//  @return The value read.
//
ASM_PFX(MmioRead64Internal):
  AARCH64_BTI(c)
  ldr     x0, [x0]
  dmb     ld
  ret

//
//  Writes a 64-bit MMIO register.
//
//  Writes the 64-bit MMIO register specified by Address with the value specified
//  by Value and returns Value. This function must guarantee that all MMIO read
//  and write operations are serialized.
//
//  @param  Address The MMIO register to write.
//  @param  Value   The value to write to the MMIO register.
//
ASM_PFX(MmioWrite64Internal):
  AARCH64_BTI(c)
  dmb     st
  str     x1, [x0]
  ret
