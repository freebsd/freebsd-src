#------------------------------------------------------------------------------
#
# InitializeFloatingPointUnits() for LoongArch64
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InitializeFloatingPointUnits)

ASM_PFX(InitializeFloatingPointUnits):
  li.d        $t0, 0x0      // RNE mode
  movgr2fcsr  $r0, $t0
  li.d        $t1, -1       // SNaN

  movgr2fr.d  $f0, $t1
  movgr2fr.d  $f1, $t1
  movgr2fr.d  $f2, $t1
  movgr2fr.d  $f3, $t1
  movgr2fr.d  $f4, $t1
  movgr2fr.d  $f5, $t1
  movgr2fr.d  $f6, $t1
  movgr2fr.d  $f7, $t1
  movgr2fr.d  $f8, $t1
  movgr2fr.d  $f9, $t1
  movgr2fr.d  $f10, $t1
  movgr2fr.d  $f11, $t1
  movgr2fr.d  $f12, $t1
  movgr2fr.d  $f13, $t1
  movgr2fr.d  $f14, $t1
  movgr2fr.d  $f15, $t1
  movgr2fr.d  $f16, $t1
  movgr2fr.d  $f17, $t1
  movgr2fr.d  $f18, $t1
  movgr2fr.d  $f19, $t1
  movgr2fr.d  $f20, $t1
  movgr2fr.d  $f21, $t1
  movgr2fr.d  $f22, $t1
  movgr2fr.d  $f23, $t1
  movgr2fr.d  $f24, $t1
  movgr2fr.d  $f25, $t1
  movgr2fr.d  $f26, $t1
  movgr2fr.d  $f27, $t1
  movgr2fr.d  $f28, $t1
  movgr2fr.d  $f29, $t1
  movgr2fr.d  $f30, $t1
  movgr2fr.d  $f31, $t1

  jirl $zero, $ra, 0
  .end
