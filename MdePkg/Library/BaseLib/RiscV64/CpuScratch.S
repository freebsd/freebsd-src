//------------------------------------------------------------------------------
//
// CPU scratch register related functions for RISC-V
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

#include <Register/RiscV64/RiscVImpl.h>

.data
.align 3
.section .text

//
// Set Supervisor mode scratch.
// @param a0 : Value set to Supervisor mode scratch
//
ASM_FUNC (RiscVSetSupervisorScratch)
    csrw CSR_SSCRATCH, a0
    ret

//
// Get Supervisor mode scratch.
// @retval a0 : Value in Supervisor mode scratch
//
ASM_FUNC (RiscVGetSupervisorScratch)
    csrr a0, CSR_SSCRATCH
    ret
