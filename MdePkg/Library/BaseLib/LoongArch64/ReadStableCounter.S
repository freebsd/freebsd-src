#------------------------------------------------------------------------------
#
# LoongArch Read Stable Counter
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(AsmReadStableCounter)

#/**
#  Gets the timer count value.
#
#  @param[] VOID
#  @retval  timer count value.
#
#**/

ASM_PFX(AsmReadStableCounter):
  rdtime.d   $a0, $zero
  jirl       $zero, $ra, 0
  .end
