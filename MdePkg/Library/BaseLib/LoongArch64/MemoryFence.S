#------------------------------------------------------------------------------
#
# MemoryFence() for LoongArch
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(MemoryFence)

#
# Memory fence for LoongArch
#
ASM_PFX(MemoryFence):
  b  AsmDataBarrierLoongArch
  .end
