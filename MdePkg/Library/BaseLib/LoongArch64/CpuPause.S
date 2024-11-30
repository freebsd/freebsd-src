#------------------------------------------------------------------------------
#
# CpuPause for LoongArch
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(CpuPause)

#/**
#  Requests CPU to pause for a short period of time.
#
#  Requests CPU to pause for a short period of time. Typically used in MP
#  systems to prevent memory starvation while waiting for a spin lock.
#
#**/

ASM_PFX(CpuPause):
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  jirl $zero, $ra, 0
  .end
