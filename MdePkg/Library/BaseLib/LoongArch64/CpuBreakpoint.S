#------------------------------------------------------------------------------
#
# CpuBreakpoint for LoongArch
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(CpuBreakpoint)

#/**
#  Generates a breakpoint on the CPU.
#
#  Generates a breakpoint on the CPU. The breakpoint must be implemented such
#  that code can resume normal execution after the breakpoint.
#
#**/

ASM_PFX(CpuBreakpoint):
  break 3
  jirl  $zero, $ra, 0
  .end
