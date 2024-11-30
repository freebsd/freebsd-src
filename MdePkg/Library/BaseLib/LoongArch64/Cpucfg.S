#------------------------------------------------------------------------------
#
# AsmCpucfg for LoongArch
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(AsmCpucfg)

#/**
#  Read CPUCFG register.
#
#  @param   a0 Specifies the register number of the CPUCFG to read the data.
#  @param   a1 Pointer to the variable used to store the CPUCFG register value.
#
#**/

ASM_PFX(AsmCpucfg):
  cpucfg  $t0, $a0
  stptr.w $t0, $a1, 0

  jirl    $zero, $ra, 0
  .end
