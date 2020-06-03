;
;  Copyright (c) 2014-2018, Linaro Limited. All rights reserved.
;
;  SPDX-License-Identifier: BSD-2-Clause-Patent
;


AREA IoLibMmio, CODE, READONLY

EXPORT MmioRead8Internal
EXPORT MmioWrite8Internal
EXPORT MmioRead16Internal
EXPORT MmioWrite16Internal
EXPORT MmioRead32Internal
EXPORT MmioWrite32Internal
EXPORT MmioRead64Internal
EXPORT MmioWrite64Internal

;
;  Reads an 8-bit MMIO register.
;
;  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
;  returned. This function must guarantee that all MMIO read and write
;  operations are serialized.
;
;  @param  Address The MMIO register to read.
;
;  @return The value read.
;
MmioRead8Internal
  ldrb    w0, [x0]
  dmb     ld
  ret

;
;  Writes an 8-bit MMIO register.
;
;  Writes the 8-bit MMIO register specified by Address with the value specified
;  by Value and returns Value. This function must guarantee that all MMIO read
;  and write operations are serialized.
;
;  @param  Address The MMIO register to write.
;  @param  Value   The value to write to the MMIO register.
;
MmioWrite8Internal
  dmb     st
  strb    w1, [x0]
  ret

;
;  Reads a 16-bit MMIO register.
;
;  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
;  returned. This function must guarantee that all MMIO read and write
;  operations are serialized.
;
;  @param  Address The MMIO register to read.
;
;  @return The value read.
;
MmioRead16Internal
  ldrh    w0, [x0]
  dmb     ld
  ret

;
;  Writes a 16-bit MMIO register.
;
;  Writes the 16-bit MMIO register specified by Address with the value specified
;  by Value and returns Value. This function must guarantee that all MMIO read
;  and write operations are serialized.
;
;  @param  Address The MMIO register to write.
;  @param  Value   The value to write to the MMIO register.
;
MmioWrite16Internal
  dmb     st
  strh    w1, [x0]
  ret

;
;  Reads a 32-bit MMIO register.
;
;  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
;  returned. This function must guarantee that all MMIO read and write
;  operations are serialized.
;
;  @param  Address The MMIO register to read.
;
;  @return The value read.
;
MmioRead32Internal
  ldr     w0, [x0]
  dmb     ld
  ret

;
;  Writes a 32-bit MMIO register.
;
;  Writes the 32-bit MMIO register specified by Address with the value specified
;  by Value and returns Value. This function must guarantee that all MMIO read
;  and write operations are serialized.
;
;  @param  Address The MMIO register to write.
;  @param  Value   The value to write to the MMIO register.
;
MmioWrite32Internal
  dmb     st
  str     w1, [x0]
  ret

;
;  Reads a 64-bit MMIO register.
;
;  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
;  returned. This function must guarantee that all MMIO read and write
;  operations are serialized.
;
;  @param  Address The MMIO register to read.
;
;  @return The value read.
;
MmioRead64Internal
  ldr    x0, [x0]
  dmb    ld
  ret

;
;  Writes a 64-bit MMIO register.
;
;  Writes the 64-bit MMIO register specified by Address with the value specified
;  by Value and returns Value. This function must guarantee that all MMIO read
;  and write operations are serialized.
;
;  @param  Address The MMIO register to write.
;  @param  Value   The value to write to the MMIO register.
;
MmioWrite64Internal
  dmb     st
  str     x1, [x0]
  ret

  END
