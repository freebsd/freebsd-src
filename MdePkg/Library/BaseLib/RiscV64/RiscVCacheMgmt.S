//------------------------------------------------------------------------------
//
// RISC-V cache operation.
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
// Copyright (c) 2023, Rivos Inc. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------
.include "RiscVasm.inc"

.align 3
ASM_GLOBAL ASM_PFX(RiscVInvalidateInstCacheFenceAsm)
ASM_GLOBAL ASM_PFX(RiscVInvalidateDataCacheFenceAsm)

ASM_PFX(RiscVInvalidateInstCacheFenceAsm):
    fence.i
    ret

ASM_PFX(RiscVInvalidateDataCacheFenceAsm):
    fence
    ret

ASM_GLOBAL ASM_PFX (RiscVCpuCacheFlushCmoAsm)
ASM_PFX (RiscVCpuCacheFlushCmoAsm):
    RISCVCMOFLUSH
    ret

ASM_GLOBAL ASM_PFX (RiscVCpuCacheCleanCmoAsm)
ASM_PFX (RiscVCpuCacheCleanCmoAsm):
    RISCVCMOCLEAN
    ret

ASM_GLOBAL ASM_PFX (RiscVCpuCacheInvalCmoAsm)
ASM_PFX (RiscVCpuCacheInvalCmoAsm):
    RISCVCMOINVALIDATE
    ret
