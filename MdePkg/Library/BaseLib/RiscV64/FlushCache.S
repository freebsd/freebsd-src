//------------------------------------------------------------------------------
//
// RISC-V cache operation.
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

.align 3
ASM_GLOBAL ASM_PFX(RiscVInvalidateInstCacheAsm)
ASM_GLOBAL ASM_PFX(RiscVInvalidateDataCacheAsm)

ASM_PFX(RiscVInvalidateInstCacheAsm):
    fence.i
    ret

ASM_PFX(RiscVInvalidateDataCacheAsm):
    fence
    ret
