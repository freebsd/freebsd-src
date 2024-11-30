#------------------------------------------------------------------------------
#
# EnableFloatingPointUnits() for LoongArch64
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(EnableFloatingPointUnits)

ASM_PFX(EnableFloatingPointUnits):
  li.w        $t0, 0x1
  csrxchg     $t0, $t0, 0x2

  jirl $zero, $ra, 0
  .end
