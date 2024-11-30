/*
 *
 * RISC-V cache operation encoding.
 * Copyright (c) 2023, Rivos Inc. All rights reserved.<BR>
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

.macro RISCVCMOFLUSH
    .word 0x25200f
.endm

.macro RISCVCMOINVALIDATE
    .word 0x05200f
.endm

.macro RISCVCMOCLEAN
    .word 0x15200f
.endm
