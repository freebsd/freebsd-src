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
// Set Supervisor Address Translation and
// Protection Register.
//
ASM_FUNC (RiscVSetSupervisorAddressTranslationRegister)
    csrw  CSR_SATP, a0
    ret

//
// Get the value of Supervisor Address Translation and
// Protection Register.
//
ASM_FUNC (RiscVGetSupervisorAddressTranslationRegister)
    csrr  a0, CSR_SATP
    ret
