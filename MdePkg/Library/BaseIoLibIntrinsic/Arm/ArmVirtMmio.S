#
#  Copyright (c) 2014-2018, Linaro Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#

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
  ldrb    r0, [r0]
  dmb
  bx      lr

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
  dmb     st
  strb    r1, [r0]
  bx      lr

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
  ldrh    r0, [r0]
  dmb
  bx      lr

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
  dmb     st
  strh    r1, [r0]
  bx      lr

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
  ldr     r0, [r0]
  dmb
  bx      lr

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
  dmb     st
  str     r1, [r0]
  bx      lr

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
  ldr     r1, [r0, #4]
  ldr     r0, [r0]
  dmb
  bx      lr

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
  dmb     st
  str     r2, [r0]
  str     r3, [r0, #4]
  bx      lr
