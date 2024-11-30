//------------------------------------------------------------------------------
//
// RISC-V Supervisor Mode interrupt enable/disable
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

#include <Register/RiscV64/RiscVImpl.h>

ASM_GLOBAL ASM_PFX(RiscVDisableSupervisorModeInterrupts)
ASM_GLOBAL ASM_PFX(RiscVEnableSupervisorModeInterrupt)
ASM_GLOBAL ASM_PFX(RiscVGetSupervisorModeInterrupts)

#define  SSTATUS_SPP_BIT_POSITION  8

//
// This routine disables supervisor mode interrupt
//
ASM_PFX(RiscVDisableSupervisorModeInterrupts):
  add   sp, sp, -(__SIZEOF_POINTER__)
  sd    a1, (sp)
  li    a1, SSTATUS_SIE
  csrc  CSR_SSTATUS, a1
  ld    a1, (sp)
  add   sp, sp, (__SIZEOF_POINTER__)
  ret

//
// This routine enables supervisor mode interrupt
//
ASM_PFX(RiscVEnableSupervisorModeInterrupt):
  add   sp, sp, -2*(__SIZEOF_POINTER__)
  sd    a0, (0*__SIZEOF_POINTER__)(sp)
  sd    a1, (1*__SIZEOF_POINTER__)(sp)

  csrr  a0, CSR_SSTATUS
  and   a0, a0, (1 << SSTATUS_SPP_BIT_POSITION)
  bnez  a0, InTrap      // We are in supervisor mode (SMode)
                        // trap handler.
                        // Skip enabling SIE becasue SIE
                        // is set to disabled by RISC-V hart
                        // when the trap takes hart to SMode.

  li    a1, SSTATUS_SIE
  csrs  CSR_SSTATUS, a1
InTrap:
  ld    a0, (0*__SIZEOF_POINTER__)(sp)
  ld    a1, (1*__SIZEOF_POINTER__)(sp)
  add   sp, sp, 2*(__SIZEOF_POINTER__)
  ret

//
// Set Supervisor mode trap vector.
// @param a0 : Value set to Supervisor mode trap vector
//
ASM_FUNC (RiscVSetSupervisorStvec)
    csrrw a1, CSR_STVEC, a0
    ret

//
// Get Supervisor mode trap vector.
// @retval a0 : Value in Supervisor mode trap vector
//
ASM_FUNC (RiscVGetSupervisorStvec)
    csrr a0, CSR_STVEC
    ret

//
// Get Supervisor trap cause CSR.
//
ASM_FUNC (RiscVGetSupervisorTrapCause)
    csrrs a0, CSR_SCAUSE, 0
    ret
//
// This routine returns supervisor mode interrupt
// status.
//
ASM_FUNC (RiscVGetSupervisorModeInterrupts)
  csrr a0, CSR_SSTATUS
  andi a0, a0, SSTATUS_SIE
  ret

//
// This routine disables supervisor mode timer interrupt
//
ASM_FUNC (RiscVDisableTimerInterrupt)
    li   a0, SIP_STIP
    csrc CSR_SIE, a0
    ret

//
// This routine enables supervisor mode timer interrupt
//
ASM_FUNC (RiscVEnableTimerInterrupt)
    li    a0, SIP_STIP
    csrs CSR_SIE, a0
    ret

//
// This routine clears pending supervisor mode timer interrupt
//
ASM_FUNC (RiscVClearPendingTimerInterrupt)
    li   a0, SIP_STIP
    csrc CSR_SIP, a0
    ret
