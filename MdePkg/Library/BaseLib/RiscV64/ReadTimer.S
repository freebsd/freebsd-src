//------------------------------------------------------------------------------
//
// Read CPU timer
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
// Read TIME CSR.
// @retval a0 : 64-bit timer.
//
ASM_FUNC (RiscVReadTimer)
    csrr a0, CSR_TIME
    ret

//
// Set Supervisor Time Compare Register
//
ASM_FUNC (RiscVSetSupervisorTimeCompareRegister)
    csrw  CSR_STIMECMP, a0
    ret
