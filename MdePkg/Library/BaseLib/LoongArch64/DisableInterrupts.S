#------------------------------------------------------------------------------
#
# LoongArch interrupt disable operations
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <Base.h>
#include <Register/LoongArch64/Csr.h>

ASM_GLOBAL ASM_PFX(DisableLocalInterrupts)
ASM_GLOBAL ASM_PFX(DisableInterrupts)

#/**
#  Disables local CPU interrupts.
#
#  @param  Needs to disable local interrupt bit.
#**/
ASM_PFX(DisableLocalInterrupts):
  csrxchg $zero, $a0, LOONGARCH_CSR_ECFG
  jirl    $zero, $ra, 0

#/**
#  Disables global CPU interrupts.
#**/
ASM_PFX(DisableInterrupts):
  li.w    $t0, BIT2
  csrxchg $zero, $t0, LOONGARCH_CSR_CRMD
  jirl    $zero, $ra, 0
  .end
