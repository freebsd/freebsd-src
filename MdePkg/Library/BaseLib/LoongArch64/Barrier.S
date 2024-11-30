#------------------------------------------------------------------------------
#
# LoongArch Barrier Operations
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(AsmDataBarrierLoongArch)
ASM_GLOBAL ASM_PFX(AsmInstructionBarrierLoongArch)

#
# Data barrier operation for LoongArch.
#
ASM_PFX(AsmDataBarrierLoongArch):
  dbar 0
  jirl $zero, $ra, 0

#
# Instruction barrier operation for LoongArch.
#
ASM_PFX(AsmInstructionBarrierLoongArch):
  ibar 0
  jirl $zero, $ra, 0

  .end
