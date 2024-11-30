#------------------------------------------------------------------------------
#
# LoongArch interrupt enable operations
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <Base.h>
#include <Register/LoongArch64/Csr.h>

ASM_GLOBAL ASM_PFX(EnableLocalInterrupts)
ASM_GLOBAL ASM_PFX(EnableInterrupts)

#/**
#  Enables local CPU interrupts.
#
#  @param  Needs to enable local interrupt bit.
#**/
ASM_PFX(EnableLocalInterrupts):
  csrxchg $a0, $a0, LOONGARCH_CSR_ECFG
  jirl    $zero, $ra, 0

#/**
#  Enables global CPU interrupts.
#**/
ASM_PFX(EnableInterrupts):
  li.w    $t0, BIT2
  csrxchg $t0, $t0, LOONGARCH_CSR_CRMD
  jirl    $zero, $ra, 0
  .end
