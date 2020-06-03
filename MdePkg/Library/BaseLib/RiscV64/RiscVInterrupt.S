//------------------------------------------------------------------------------
//
// RISC-V Supervisor Mode interrupt enable/disable
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(RiscVDisableSupervisorModeInterrupts)
ASM_GLOBAL ASM_PFX(RiscVEnableSupervisorModeInterrupt)
ASM_GLOBAL ASM_PFX(RiscVGetSupervisorModeInterrupts)

# define  MSTATUS_SIE    0x00000002
# define  CSR_SSTATUS    0x100

ASM_PFX(RiscVDisableSupervisorModeInterrupts):
  li   a1, MSTATUS_SIE
  csrc CSR_SSTATUS, a1
  ret

ASM_PFX(RiscVEnableSupervisorModeInterrupt):
  li   a1, MSTATUS_SIE
  csrs CSR_SSTATUS, a1
  ret

ASM_PFX(RiscVGetSupervisorModeInterrupts):
  csrr a0, CSR_SSTATUS
  andi a0, a0, MSTATUS_SIE
  ret

