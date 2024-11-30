//------------------------------------------------------------------------------
//
// MemoryFence() for RiscV64
//
// Copyright (c) 2021, Hewlett Packard Enterprise Development. All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

.text
.p2align 2

ASM_GLOBAL ASM_PFX(MemoryFence)

//
// Memory fence for RiscV64
//
//
ASM_PFX(MemoryFence):
    fence  // Fence on all memory and I/O
    ret
