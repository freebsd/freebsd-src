#------------------------------------------------------------------------------
#
# Get LoongArch interrupt status
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(GetInterruptState)

#/**
#  Retrieves the current CPU interrupt state.
#
#  Returns TRUE means interrupts are currently enabled. Otherwise,
#  returns FALSE.
#
#  @retval TRUE  CPU interrupts are enabled.
#  @retval FALSE CPU interrupts are disabled.
#
#**/

ASM_PFX(GetInterruptState):
  li.w  $t1, 0x4
  csrrd $t0, 0x0
  and   $t0, $t0, $t1
  beqz  $t0, 1f
  li.w  $a0, 0x1
  b     2f
1:
  li.w  $a0, 0x0
2:
  jirl  $zero, $ra, 0
  .end
