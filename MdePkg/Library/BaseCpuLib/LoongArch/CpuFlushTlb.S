#------------------------------------------------------------------------------
#
# CpuFlushTlb() for LoongArch64
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(CpuFlushTlb)

ASM_PFX(CpuFlushTlb):
  tlbflush
  jirl $zero, $ra, 0
  .end
